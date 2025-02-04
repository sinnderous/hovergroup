/*
 * pPursuitVehicle
 *        File: PursuitVehicle.cpp
 *  Created on: Sept 5, 2014
 *      Author: Josh Leighton
 */

#include <iterator>
#include "MBUtils.h"
#include "PursuitVehicle.h"
#include "GeomUtils.h"
#include "AngleUtils.h"

using namespace std;
using namespace JoshUtil;

//---------------------------------------------------------
// Constructor

PursuitVehicle::PursuitVehicle() {
    m_running = false;
    receive_count=0;
    m_projection = false;
    m_project_time = 7;
    m_min_speed = 0;

    codec = goby::acomms::DCCLCodec::get();
    try {
        codec->validate<PursuitReportDCCL>();
        codec->validate<PursuitCommandDCCL>();
    } catch (goby::acomms::DCCLException& e) {
        std::cout << "failed to validate encoder" << std::endl;
        exit (1);
    }

    dccl_report.set_ack(false);
    multicast = true;
}

//---------------------------------------------------------
// Destructor

PursuitVehicle::~PursuitVehicle() {
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool PursuitVehicle::OnNewMail(MOOSMSG_LIST &NewMail) {
    MOOSMSG_LIST::iterator p;

    for (p = NewMail.begin(); p != NewMail.end(); p++) {
        CMOOSMsg &msg = *p;
        string key = msg.GetKey();
        if (key == "PURSUIT_COMMAND") {
            string cmd = MOOSToUpper(msg.GetString());
            if (cmd == "RUN") {
                tdma_engine.run();
                command_trajectory.clear();
                current_x = initial_x;
                current_y = initial_y;
                m_Comms.Notify("PURSUIT_ACTION", "STATION");
                stringstream ss;
                ss << "station_pt = " << current_x << "," << current_y;
                m_Comms.Notify("PURSUIT_STATION_UPDATES", ss.str());
            } else if (cmd == "STOP") {
                tdma_engine.stop();
            } else if (cmd == "RESET") {
                tdma_engine.reset();
                receive_count = 0;
            }
        } else if (key == "NAV_X") {
            m_navx = msg.GetDouble();
        } else if (key == "NAV_Y") {
            m_navy = msg.GetDouble();
        } else if (key == "ACOMMS_RECEIVED") {
            HoverAcomms::AcommsReception reception;
            // try to parse acomms message
            if (reception.parseFromString(msg.GetString())) {
                bool proceed = true;
//                if (!multicast) {
//                    if (tdma_engine.getCurrentSlot() != m_id*2 - 2) {
//                        proceed = false;
//                    }
//                }
                // check source and status
                if (reception.getSource() == 4 &&
                        reception.getStatus() == HoverAcomms::GOOD
                        && proceed) {

                    // perform decode
                    PursuitCommandDCCL cmd;
                    bool decode_okay = true;
                    try {
                        codec->decode(reception.getData(), &cmd);
                    } catch (goby::acomms::DCCLException &) {
                        stringstream ss;
                        ss << "failed decoding acomms message" << endl;
                        cout << ss.str() << endl;
                        m_Comms.Notify("PURSUIT_ERROR", ss.str());
                        decode_okay = false;
                    }

                    if (decode_okay) {
                        switch (m_id) {
                        case 1:
                            if (cmd.has_1()) {
                                receive_count++;
                                m_Comms.Notify("PURSUIT_RECEIVE_COUNT", (double) receive_count);
                                command_trajectory.clear();
                                for (int i=0; i<cmd.vehicle1_cmd_size(); i++) {
                                    command_trajectory.push_back(cmd.vehicle1_cmd(i));
                                }
                                cout << "Implementing new command trajectory" << endl;
                                dccl_report.set_ack(true);
                            }
                            break;
                        case 2:
                            if (cmd.has_2()) {
                                receive_count++;
                                m_Comms.Notify("PURSUIT_RECEIVE_COUNT", (double) receive_count);
                                command_trajectory.clear();
                                for (int i=0; i<cmd.vehicle2_cmd_size(); i++) {
                                    command_trajectory.push_back(cmd.vehicle2_cmd(i));
                                }
                                cout << "Implementing new command trajectory" << endl;
                                dccl_report.set_ack(true);
                            }
                            break;
                        case 3:
                            if (cmd.has_3()) {
                                receive_count++;
                                m_Comms.Notify("PURSUIT_RECEIVE_COUNT", (double) receive_count);
                                command_trajectory.clear();
                                for (int i=0; i<cmd.vehicle3_cmd_size(); i++) {
                                    command_trajectory.push_back(cmd.vehicle3_cmd(i));
                                }
                                cout << "Implementing new command trajectory" << endl;
                                dccl_report.set_ack(true);
                            }
                            break;
                        }
                        m_Comms.Notify("PURSUIT_TRAJECTORY_LENGTH", command_trajectory.size());
                    }
                }
            }
        }
    }

    return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool PursuitVehicle::OnConnectToServer() {
    string mode;
    m_MissionReader.GetConfigurationParam("command_mode", mode);
    MOOSToUpper(mode);
    if (mode == "MULTICAST") {
        multicast = true;
    } else if (mode == "INTERLEAVED") {
        multicast = false;
    }

    string test_param;
    m_MissionReader.GetConfigurationParam("id", m_id);
    m_MissionReader.GetConfigurationParam("positive_x", positive_x);
    m_MissionReader.GetConfigurationParam("positive_y", positive_y);
    m_MissionReader.GetConfigurationParam("negative_x", negative_x);
    m_MissionReader.GetConfigurationParam("negative_y", negative_y);
    m_MissionReader.GetConfigurationParam("initial_x", initial_x);
    m_MissionReader.GetConfigurationParam("initial_y", initial_y);
    m_MissionReader.GetConfigurationParam("use_projection", m_projection);
    m_MissionReader.GetConfigurationParam("project_time", m_project_time);
    m_MissionReader.GetConfigurationParam("min_speed", m_min_speed);

    string my_name;
    m_MissionReader.GetValue("Community", my_name);

    XYSegList trackline;
    trackline.add_vertex(positive_x, positive_y, 0, "");
    trackline.add_vertex(negative_x, negative_y, 0, "");
    trackline.set_label(my_name+"_pursuit_trackline");
    m_Comms.Notify("VIEW_SEGLIST", trackline.get_spec());

    list<string> sParams;
    m_MissionReader.EnableVerbatimQuoting(false);
    if(m_MissionReader.GetConfiguration(GetAppName(), sParams)) {
        list<string>::iterator p;
        for (p = sParams.begin(); p != sParams.end(); p++) {
            string original_line = *p;
            string param = stripBlankEnds(toupper(biteString(*p, '=')));
            string value = stripBlankEnds(*p);

            if (param == "CODE") {
                string left = MOOSChomp(value,",");
                int a = atoi(left.c_str());
                double b = atof(value.c_str());
                command_map[a] = b;
                cout << "Added " << a << " --> " << b << " to command map." << endl;
            }
        }
    }

    tdma_engine.parseConfig(m_MissionReader, GetAppName());
    cout << tdma_engine.getSummary() << endl;
    m_Comms.Notify("TDMA_SUMMARY", tdma_engine.getSingleLineSummary());

    m_Comms.Register("PURSUIT_COMMAND", 0);
    m_Comms.Register("NAV_X", 0);
    m_Comms.Register("NAV_Y", 0);
    m_Comms.Register("ACOMMS_RECEIVED", 0);
    return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool PursuitVehicle::Iterate() {
    if (tdma_engine.testAdvance()) {
        m_Comms.Notify("TDMA_SLOT", tdma_engine.getCurrentSlot());
        m_Comms.Notify("TDMA_CYCLE_COUNT", tdma_engine.getCycleCount());
        m_Comms.Notify("TDMA_CYCLE_NUMBER", tdma_engine.getCycleNumber());

        m_Comms.Notify("PURSUIT_X", m_navx);
        m_Comms.Notify("PURSUIT_Y", m_navy);

        int slot = tdma_engine.getCurrentSlot();

        // send ack via wifi
        if (multicast) {
            if (tdma_engine.getCurrentSlot() == 1) {
                if (dccl_report.ack()) {
                    m_Comms.Notify("PURSUIT_COMMAND_RECEIVED", 1.0);
                } else {
                    m_Comms.Notify("PURSUIT_COMMAND_RECEIVED", 0.0);
                }
            }
        } else {
            int index;
            switch (m_id) {
            case 1:
                index = 3;
                break;
            case 2:
                index = 5;
                break;
            case 3:
                index = 1;
                break;
            }
            if (slot == index) {
                if (dccl_report.ack()) {
                    m_Comms.Notify("PURSUIT_COMMAND_RECEIVED", 1.0);
                } else {
                    m_Comms.Notify("PURSUIT_COMMAND_RECEIVED", 0.0);
                }
            }
        }

        // transmit during our slot
        bool transmit = false;
        if (multicast && slot == m_id)
            transmit = true;
        if (!multicast && slot == m_id*2-1)
            transmit = true;

        if (transmit) {
            dccl_report.set_id(m_id);
            dccl_report.set_x(m_navx);
            dccl_report.set_y(m_navy);
            dccl_report.set_slot(slot);

            std::string bytes;
            codec->encode(&bytes, dccl_report);
            cout << "Transmitting: " << endl;
            cout << dccl_report.DebugString() << endl;
            m_Comms.Notify("ACOMMS_TRANSMIT_DATA_BINARY", (void*) bytes.data(),
                    bytes.size());
            cout << "Total size " << bytes.size() << endl;

            // reset ack to false
            dccl_report.set_ack(false);
        }

        // implement commands
        if (command_trajectory.size() > 0) {
            if (command_map.find(command_trajectory[0]) != command_map.end()) {
                double desired_speed = command_map[command_trajectory[0]];
                m_Comms.Notify("PURSUIT_DESIRED_SPEED", desired_speed);
                m_Comms.Notify("PURSUIT_QUANTIZED_COMMAND",
                        command_trajectory[0]);
                command_trajectory.erase(command_trajectory.begin());

                cout << tdma_engine.getCurrentSlot() << " Speed: "
                        << desired_speed << endl;

                if (m_projection) {
                    if (desired_speed == 0) {
//                        m_Comms.Notify("PURSUIT_ACTION", "STATION");
//                        stringstream ss;
//                        ss << "station_pt = " << current_x << "," << current_y;
//                        m_Comms.Notify("PURSUIT_STATION_UPDATES", ss.str());
                    } else {
                        // project current target onto the trackline
                        double nx, ny;
                        perpSegIntPt(positive_x, positive_y, negative_x,
                                negative_y, current_x, current_y, nx, ny);
                        XYPoint perp_pt(nx, ny);

                        double angle, dist;
                        double distance = fabs(desired_speed * m_project_time);
                        if (desired_speed > 0) {
                            angle = relAng(negative_x, negative_y, positive_x,
                                    positive_y);
                            dist = distPointToPoint(nx, ny, positive_x, positive_y);
                        } else {
                            angle = relAng(positive_x, positive_y, negative_x,
                                    negative_y);
                            dist = distPointToPoint(nx, ny, negative_x, negative_y);
                        }
                        double targx, targy;
                        if (current_x == positive_x && current_y == positive_y && desired_speed > 0) {
                            targx = positive_x;
                            targy = positive_y;
                            cout << "capped at " << targx << "," << targy
                                    << endl;
                        } else if (current_x == negative_x && current_y == negative_y && desired_speed < 0) {
                            targx = negative_x;
                            targy = negative_y;
                            cout << "capped at " << targx << "," << targy
                                    << endl;
                        } else if (distance > dist) {
                            if (desired_speed > 0) {
                                targx = positive_x;
                                targy = positive_y;
                            } else {
                                targx = negative_x;
                                targy = negative_y;
                            }
                            cout << "capped at " << targx << "," << targy
                                    << endl;
                        } else {
                            projectPoint(angle, distance, nx, ny, targx, targy);
                            cout << "projected to " << targx << "," << targy
                                    << endl;
                        }
                        current_x = targx;
                        current_y = targy;


                        stringstream ss;
                        ss << targx << "," << targy;
                        m_Comms.Notify("PURSUIT_WAYPOINT", ss.str());

                        ss.str("");
                        ss << "points = ";
                        if (desired_speed > 0) {
                            ss << negative_x << "," << negative_y << ":"
                                    << targx << "," << targy;
                        } else {
                            ss << positive_x << "," << positive_y << ":"
                                    << targx << "," << targy;
                        }
                        m_Comms.Notify("PURSUIT_WAYPOINT_UPDATES", ss.str());
                        m_Comms.Notify("PURSUIT_ACTION", "WAYPOINT");

                        ss.str("");
                        if (fabs(desired_speed) < m_min_speed) {
                            desired_speed = m_min_speed;
                        }
                        ss << "speed = " << fabs(desired_speed);
                        m_Comms.Notify("PURSUIT_WAYPOINT_UPDATES", ss.str());

                        ss.str("");
                        ss << "station_pt = " << targx << "," << targy;
                        m_Comms.Notify("PURSUIT_STATION_UPDATES", ss.str());
                    }
                } else {
                    stringstream ss;
                    if (desired_speed == 0) {
                        m_Comms.Notify("PURSUIT_ACTION", "STATION");
                    } else {
                        ss << "speed = " << fabs(desired_speed);
                        m_Comms.Notify("PURSUIT_WAYPOINT_UPDATES", ss.str());
                        ss.str("");
                        ss << "points = ";
                        if (desired_speed > 0) {
                            ss << negative_x << "," << negative_y << ":"
                                    << positive_x << "," << positive_y;
                        } else {
                            ss << positive_x << "," << positive_y << ":"
                                    << negative_x << "," << negative_y;
                        }
                        m_Comms.Notify("PURSUIT_WAYPOINT_UPDATES", ss.str());
                        m_Comms.Notify("PURSUIT_ACTION", "WAYPOINT");
                    }
                }
            } else {
                cout << tdma_engine.getCurrentSlot() << " Unknown command "
                        << command_trajectory[0] << endl;
                stringstream ss;
                ss << "Value " << command_trajectory[0]
                        << " does not exist in map";
                cout << ss.str() << endl;
                m_Comms.Notify("PURSUIT_ERROR", ss.str());
            }
        } else {
            cout << tdma_engine.getCurrentSlot() << " Command trajectory empty"
                    << endl;
        }

        m_Comms.Notify("PURSUIT_TRAJECTORY_LENGTH", command_trajectory.size());
    }
    return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool PursuitVehicle::OnStartUp() {
    return (true);
}
