/************************************************************/
/*    NAME:                                               */
/*    ORGN: MIT                                             */
/*    FILE: TDOAComms.cpp                                        */
/*    DATE:                                                 */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "TDOAComms.h"
#include "HoverAcomms.h"

using namespace std;

//---------------------------------------------------------
// Constructor

TDOAComms::TDOAComms() {
	resetOutput();
	m_offsets = std::vector<double>(4, 0);

	m_paused = true;
	m_state = PAUSED;
	m_lastOutputState = -1;

	m_codec = goby::acomms::DCCLCodec::get();
    try {
        m_codec->validate<TDOAPSK1>();
    } catch (goby::acomms::DCCLException& e) {
        std::cout << "failed to validate" << std::endl;
        exit(1);
    }
}

//---------------------------------------------------------
// Destructor

TDOAComms::~TDOAComms() {
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool TDOAComms::OnNewMail(MOOSMSG_LIST &NewMail) {
	MOOSMSG_LIST::iterator p;

	for (p = NewMail.begin(); p != NewMail.end(); p++) {
		CMOOSMsg &msg = *p;
		string key = msg.GetKey();

		// handle incoming acomms events
		if (key=="ACOMMS_RECEIVED") {
			acommsReceive(msg.GetString());
		}

		// set offset of target according to acomms scheduler
		else if (key == "ACOMMS_SCHEDULER_OFFSET") {
			m_targetOffset = msg.GetDouble();
			m_slotFunctions.base_offset = m_targetOffset;
		}

		// adjust offset or period after launch
		else if (key == "TDOA_OFFSET") {
			m_localOffset = msg.GetDouble();
		} else if (key == "TDOA_PERIOD") {
			m_slotFunctions.period = msg.GetDouble();
		}

		// pause/unpause
		else if (key == "TDOA_COMMAND") {
			string sval = MOOSToUpper(msg.GetString());
			if (sval == "RUN") {
				m_paused = false;
				resetOutput();
			} else if (sval == "PAUSE") {
				m_paused = true;
				m_state = PAUSED;
			}
		}

		// update nav info
		else if (key == "NAV_X") {
			m_navX = msg.GetDouble();
		} else if (key == "NAV_Y") {
			m_navY = msg.GetDouble();
		}
	}

	return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool TDOAComms::OnConnectToServer() {
	m_MissionReader.GetConfigurationParam("ID", m_id);
	m_MissionReader.GetConfigurationParam("period", m_slotFunctions.period);
	m_MissionReader.GetConfigurationParam("f1_offset", m_offsets[0]);
	m_MissionReader.GetConfigurationParam("f2_offset", m_offsets[1]);
	m_MissionReader.GetConfigurationParam("f3_offset", m_offsets[2]);
	m_MissionReader.GetConfigurationParam("target_id", m_targetID);

	m_outgoingUpdate.set_local_id(m_id);
	m_outgoingUpdate.set_cycle_state(TDOAUpdate_StateEnum_PAUSED);

	RegisterVariables();
	return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool TDOAComms::Iterate() {
	// do nothing if paused
	if (m_paused || m_state==PAUSED)
		return true;

	cout << m_slotFunctions.getSlotFractionSeconds() << endl;

	// check if next slot by time
	if (testAdvanceSlot(m_offsets[m_state])) {
	    cout << "advancing state past offset " << m_offsets[m_state] << endl;

	    // post output if we didn't already through an acomms reception
	    if (m_lastOutputState != m_state)
	        postOutput();

        // advance slot
		advanceSlot();

        // transmit if it's our turn
        if (m_state == m_id) {
            acommsTransmit();
        }

		if (m_state == LEADER_SLOT)
			resetOutput();

        cout << "next offset: " << m_offsets[m_state] << endl;
	}

	return (true);
}

bool TDOAComms::testAdvanceSlot(double offset) {
    if (offset == 0 && m_slotFunctions.getSlotFractionSeconds() > m_slotFunctions.period/2)
        return false;
	// advance if past specified offset and more than 1 sec since last state change
	return (m_slotFunctions.getSlotFractionSeconds() > offset && MOOSTime()-m_lastStateAdvanceTime > 1);
}

void TDOAComms::advanceSlot() {
	m_lastStateAdvanceTime = MOOSTime();
	m_state++;
	if (m_state == PAUSED)
		m_state = LEADER_SLOT;

	m_Comms.Notify("TDOA_STATE", m_state);
}

void TDOAComms::acommsTransmit() {
	TDOAPSK1 packet;
	for (int i=0; i<m_outgoingUpdate.data_size(); i++) {
		packet.add_data()->CopyFrom(m_outgoingUpdate.data(i));
	}

	std::string bytes;
	m_codec->encode(&bytes, packet);

	m_Comms.Notify("ACOMMS_TRANSMIT_DATA_BINARY", (void*) bytes.data(), bytes.size());

	cout << "sent " << bytes.size() << " bytes" << endl;
	cout << packet.DebugString() << endl;
}

void TDOAComms::acommsReceive(string msg) {
	HoverAcomms::AcommsReception reception;
	if (reception.parseFromString(msg)) {
		// if from target, store arrival time
		if (reception.getSource()==m_targetID) {
			// check our state matches if we're not paused
			if (!m_paused && m_state!=LEADER_SLOT) {
				cout << "Source " << reception.getSource() << " different from state " << m_state << endl;
				m_state = LEADER_SLOT;
				resetOutput();
			}

			// save data
			TDOAData* dat = m_outgoingUpdate.add_data();
			dat->set_x(m_navX);
			dat->set_y(m_navY);
			dat->set_id(m_id);
			dat->set_toa(reception.getRangingTime());

			// post data and advance if not paused
			if (!m_paused) {
				postOutput();
//				advanceSlot();
			}
		}

		// if not from target, try to parse data if data is good
		else if (reception.getStatus() == HoverAcomms::GOOD && !m_paused) {
			// check our state matches if we're not paused
			if (!m_paused && reception.getSource()!=m_state) {
				cout << "Source " << reception.getSource() << " different from state " << m_state << endl;
				m_state = reception.getSource();
			}

			// try to parse packet
			TDOAPSK1 packet;
			try {
			    m_codec->decode(reception.getData(), &packet);
			} catch(goby::acomms::DCCLException&) {
			    cout << "failed to decode acomms message" << endl;
			    return;
			}
            cout << "received packet: " << endl << packet.DebugString() << endl;

            // add data to output if we don't already have it
            for (int i=0; i<packet.data_size(); i++) {
                // check if this is fill data
                if (packet.data(i).id()!=0) {
                    bool already_have = false;
                    for (int j=0; j<m_outgoingUpdate.data_size(); j++) {
                        if (packet.data(i).id() == m_outgoingUpdate.data(j).id()) {
                            already_have = true;
                            break;
                        }
                    }

                    if (!already_have) {
                        m_outgoingUpdate.add_data()->CopyFrom(packet.data(i));
                    }
                }
            }

            // post data and advance if not paused
            postOutput();
//            advanceSlot();
		}
	} else {
	    std::cout << "failed to parse acomms reception" << std::endl;
	}
}

void TDOAComms::postOutput() {
	m_outgoingUpdate.set_cycle_state((TDOAUpdate_StateEnum) m_state);
	m_lastOutputState = m_state;
	string out = m_outgoingUpdate.SerializeAsString();
	m_Comms.Notify("TDOA_PROTOBUF", (void*) out.data(), out.size());
	out = m_outgoingUpdate.DebugString();
	while (out.find("\n") != std::string::npos) {
		out.replace(out.find("\n"), 1, "<|>");
	}
	m_Comms.Notify("TDOA_PROTOBUF_DEBUG", out);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool TDOAComms::OnStartUp() {
	return (true);
}

void TDOAComms::resetOutput() {
	m_outgoingUpdate.mutable_data()->Clear();
}

//---------------------------------------------------------
// Procedure: RegisterVariables

void TDOAComms::RegisterVariables() {
	m_Comms.Register("ACOMMS_RECEIVED", 0);
	m_Comms.Register("NAV_X", 0);
	m_Comms.Register("NAV_Y", 0);
	m_Comms.Register("ACOMMS_SCHEDULER_OFFSET", 0);
	m_Comms.Register("TDOA_OFFSET", 0);
	m_Comms.Register("TDOA_PERIOD", 0);
	m_Comms.Register("TDOA_COMMAND", 0);
}

