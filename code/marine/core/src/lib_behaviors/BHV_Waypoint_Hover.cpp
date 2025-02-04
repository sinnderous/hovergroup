/*
 * BHV_Waypoint_Hover
 *        File: BHV_Waypoint.cpp
 *  Created on: Aug 17, 2014
 *      Author: Josh Leighton
 */

/*
 * This file is modified from the original MOOS-IvP version to
 * reduce overshoot when starting on a new leg.
 */

/*****************************************************************/
/*    NAME: Michael Benjamin, Henrik Schmidt, and John Leonard   */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Waypoint.cpp                                     */
/*    DATE: Nov 2004                                             */
/*                                                               */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License   */
/* as published by the Free Software Foundation; either version  */
/* 2 of the License, or (at your option) any later version.      */
/*                                                               */
/* This program is distributed in the hope that it will be       */
/* useful, but WITHOUT ANY WARRANTY; without even the implied    */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       */
/* PURPOSE. See the GNU General Public License for more details. */
/*                                                               */
/* You should have received a copy of the GNU General Public     */
/* License along with this program; if not, write to the Free    */
/* Software Foundation, Inc., 59 Temple Place - Suite 330,       */
/* Boston, MA 02111-1307, USA.                                   */
/*****************************************************************/

#ifdef _WIN32
#pragma warning(disable : 4786)
#pragma warning(disable : 4503)
#endif

#include <cmath> 
#include <cstdlib>
#include <iostream>
#include "BHV_Waypoint_Hover.h"
#include "OF_Reflector.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "AOF_Waypoint.h"
#include "GeomUtils.h"
#include "BuildUtils.h"
#include "PathUtils.h"
#include "FunctionEncoder.h"
#include "ZAIC_PEAK.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"
#include "XYFormatUtilsPoint.h"
#include "XYFormatUtilsSegl.h"
#include "ColorParse.h"

using namespace std;

//-----------------------------------------------------------
// Procedure: Constructor

BHV_Waypoint_Hover::BHV_Waypoint_Hover(IvPDomain gdomain) :
        IvPBehavior(gdomain) {
    // First set variables at the superclass level
    m_descriptor = "bhv_waypoint_hover";
    m_domain = subDomain(m_domain, "course,speed");

    // Set behavior configuration variables
    m_cruise_speed = 0;  // meters/second
    m_lead_distance = -1; // meters - default of -1 means unused
    m_lead_damper = -1; // meters - default of -1 means unused
    m_ipf_type = "zaic";

    m_var_report = "WPT_STAT";
    m_var_index = "WPT_INDEX";
    m_var_cyindex = "CYCLE_INDEX";
    m_var_suffix = "";

    m_hint_vertex_color = "";
    m_hint_edge_color = "";
    m_hint_vertex_size = -1;
    m_hint_edge_size = -1;
    m_lead_to_start = false;

    // The completed and perpetual vars are initialized in superclass
    // but we initialize here just to be safe and clear.
    m_completed = false;
    m_perpetual = false;

    m_osx = 0;
    m_osy = 0;
    m_osv = 0;

    m_greedy_tour_pending = false;

    m_heading_cutoff = 180;
    m_nav_heading = 0;
    m_mod_is_ending = false;
    m_mod_is_active = false;

    addInfoVars("NAV_X, NAV_Y, NAV_SPEED, NAV_HEADING");
    m_markpt.set_active(false);
    m_markpt.set_vertex_size(4);

    m_prevpt.set_label("prev_pt");

    m_skip_first_waypoint = false;
}

//-----------------------------------------------------------
// Procedure: onSetParamComplete()

void BHV_Waypoint_Hover::onSetParamComplete() {
    m_trackpt.set_label(m_us_name + "'s track-point");
    m_trackpt.set_vertex_size(4);

    m_nextpt.set_label(m_us_name + "'s next waypoint");
    m_nextpt.set_vertex_size(4);
}

//-----------------------------------------------------------
// Procedure: setParam
//     Notes: We expect the "waypoint" entries will be of the form
//            "xposition,yposition".
//            The "radius" parameter indicates what it means to have
//            arrived at the waypoint.

bool BHV_Waypoint_Hover::setParam(string param, string param_val) {
    double dval = atof(param_val.c_str());

    cout << "param" << param << endl;
    cout << "param_val" << param_val << endl;

    if ((param == "polygon") || (param == "points")) {
        XYSegList new_seglist = string2SegList(param_val);
        if (new_seglist.size() == 0) {
            XYPolygon new_poly = string2Poly(param_val);
            new_seglist = new_poly.exportSegList(m_osx, m_osy);
        }
        if (new_seglist.size() == 0)
            return (false);
        m_waypoint_engine.setSegList(new_seglist);
        m_markpt.set_active(false);
        return (true);
    } else if (param == "point") {
        XYPoint point = string2Point(param_val);
        XYSegList new_seglist;
        new_seglist.add_vertex(point);
        if (new_seglist.size() == 0)
            return (false);
        m_waypoint_engine.setSegList(new_seglist);
        m_markpt.set_active(false);
        return (true);
    } else if ((param == "speed") && (dval > 0)) {
        m_cruise_speed = dval;
        return (true);
    } else if ((param == "greedy_tour") || (param == "shortest_tour")) {
        setBooleanOnString(m_greedy_tour_pending, param_val);
        return (true);
    } else if ((param == "wpt_status") || (param == "wpt_status_var")) {
        if (strContainsWhite(param_val) || (param_val == ""))
            return (false);
        m_var_report = param_val;
        if (tolower(m_var_report) == "silent")
            m_var_report = "silent";
        return (true);
    } else if ((param == "wpt_index") || (param == "wpt_index_var")) {
        if (strContainsWhite(param_val) || (param_val == ""))
            return (false);
        m_var_index = param_val;
        if (tolower(m_var_index) == "silent")
            m_var_index = "silent";
        return (true);
    } else if (param == "cycle_index_var") {   // Depricated
        if (strContainsWhite(param_val) || (param_val == ""))
            return (false);
        m_var_cyindex = param_val;
        if (tolower(m_var_cyindex) == "silent")
            m_var_index = "silent";
        return (true);
    } else if (param == "post_suffix") {
        if (strContainsWhite(param_val))
            return (false);
        if ((param_val.length() > 0) && (param_val.at(0) != '_'))
            param_val = '_' + param_val;
        m_var_suffix = param_val;
        return (true);
    } else if (param == "cycleflag") {
        string variable = biteStringX(param_val, '=');
        string value = param_val;
        if ((variable == "") || (value == ""))
            return (false);
        VarDataPair pair(variable, value, "auto");
        m_cycle_flags.push_back(pair);
        return (true);
    } else if (param == "wptflag") {
        string variable = biteStringX(param_val, '=');
        string value = param_val;
        if ((variable == "") || (value == ""))
            return (false);
        VarDataPair pair(variable, value, "auto");
        m_wpt_flags.push_back(pair);
        return (true);
    } else if (param == "ipf-type") {
        param_val = tolower(param_val);
        if ((param_val == "zaic") || (param_val == "roc")
                || (param_val == "rate_of_closure"))
            m_ipf_type = param_val;
        return (true);
    } else if ((param == "lead") && (dval > 0)) {
        m_lead_distance = dval;
        return (true);
    } else if (param == "lead_to_start")
        return (setBooleanOnString(m_lead_to_start, param_val));
    else if ((param == "lead_damper") && (dval > 0)) {
        m_lead_damper = dval;
        return (true);
    } else if (param == "order") {
        if ((param_val != "reverse") && (param_val != "reversed")
                && (param_val != "normal"))
            return (false);
        bool reverse = ((param_val == "reverse") || (param_val == "reversed"));
        m_waypoint_engine.setReverse(reverse);
        return (true);
    } else if ((param == "repeat") && (tolower(param_val) == "forever")) {
        IvPBehavior::setParam("perpetual", "true");
        m_waypoint_engine.setRepeatsEndless(true);
        return (true);
    } else if (param == "repeat") {
        int ival = atoi(param_val.c_str());
        if ((ival < 0) || (!isNumber(param_val)))
            return (false);
        if (ival > 0)
            IvPBehavior::setParam("perpetual", "true");
        //    else
        //      IvPBehavior::setParam("perpetual", "false");
        m_waypoint_engine.setRepeat(ival);
        m_waypoint_engine.setRepeatsEndless(false);
        return (true);
    } else if ((param == "radius") || (param == "capture_radius")) {
        if (dval <= 0)
            return (false);
        m_waypoint_engine.setCaptureRadius(dval);
        return (true);
    } else if ((param == "nm_radius") || (param == "slip_radius")) {
        if (dval < 0)
            return (false);
        m_waypoint_engine.setNonmonotonicRadius(dval);
        return (true);
    } else if (param == "capture_line") {
        param_val = tolower(param_val);
        if (!isBoolean(param_val) && (param_val != "absolute"))
            return (false);
        if (param_val == "absolute") {
            param_val = "true";
            m_waypoint_engine.setSlipRadius(0);
            m_waypoint_engine.setCaptureRadius(0);
        }
        if (param_val == "true")
            m_waypoint_engine.setCaptureLine(true);
        else
            m_waypoint_engine.setCaptureLine(false);
        return (true);
    } else if (param == "visual_hints") {
        vector<string> svector = parseStringQ(param_val, ',');
        unsigned int i, vsize = svector.size();
        for (i = 0; i < vsize; i++)
            handleVisualHint(svector[i]);
        return (true);
    } else if (param == "trackpoint_heading_cutoff") {
        if (dval < 0)
            return false;
        m_heading_cutoff = dval;
        return true;
    } else if (param == "skip_first_waypoint") {
        return (setBooleanOnString(m_skip_first_waypoint, param_val));
    }
    return (false);
}

//-----------------------------------------------------------
// Procedure: onRunToIdleState
//      Note: Invoked automatically by the helm when the behavior
//            first transitions from the Running to Idle state.

void BHV_Waypoint_Hover::onRunToIdleState() {
    postErasables();
    m_waypoint_engine.resetCPA();
}

//-----------------------------------------------------------
// Procedure: onRunState

BehaviorReport BHV_Waypoint_Hover::onRunState(string input) {
    IvPFunction *ipf = onRunState();
    BehaviorReport bhv_report(m_descriptor);

    bhv_report.addIPF(ipf);
    bhv_report.setPriority(m_priority_wt);
    return (bhv_report);
}

//-----------------------------------------------------------
// Procedure: onRunState

IvPFunction *BHV_Waypoint_Hover::onRunState() {
    m_waypoint_engine.setPerpetual(m_perpetual);

    // Set m_osx, m_osy, m_osv
    if (!updateInfoIn()) {
        postMessage("VIEW_POINT", m_nextpt.get_spec("active=false"), "wpt");
        return (0);
    }

    // Note the waypoint prior to possibly incrementing the waypoint
    double this_x = m_waypoint_engine.getPointX();
    double this_y = m_waypoint_engine.getPointY();
    if (!m_prevpt.valid())
        m_prevpt.set_vertex(m_osx, m_osy);

    // Possibly increment the waypoint
    // Set m_nextpt, m_trackpt
    bool next_point = setNextWaypoint();

    // Update things if the waypoint was indeed incremented
    double next_x = m_waypoint_engine.getPointX();
    double next_y = m_waypoint_engine.getPointY();
    if ((next_x != this_x) || (next_y != this_y)) {
        m_prevpt.set_vertex(this_x, this_y);
        postWptFlags(this_x, this_y);
    }

    // We want to report the updated cycle info regardless of the
    // above result. Even if the next_point is false and there are
    // no more points, this means the cyindex is probably incremented.
    if (m_var_cyindex != "silent") {
        int waypt_cycles = m_waypoint_engine.getCycleCount();
        postMessage((m_var_cyindex + m_var_suffix), waypt_cycles);
    }

    // Only publish these reports if we have another point to go.
    if (next_point) {
        postStatusReport();
        postViewableSegList();
        //postMessage("VIEW_POINT", m_prevpt.get_spec("active=true"), "prevpt");
        postMessage("VIEW_POINT", m_nextpt.get_spec("active=true"), "wpt");
        double dist = hypot((m_nextpt.x() - m_trackpt.x()),
                (m_nextpt.y() - m_trackpt.y()));
        // If the trackpoint and next waypoint differ by more than five
        // meters then post a visual cue for the track point.
        if (dist > 5)
            postMessage("VIEW_POINT", m_trackpt.get_spec("active=true"), "trk");
        else
            postMessage("VIEW_POINT", m_trackpt.get_spec("active=false"),
                    "trk");
    }
    // Otherwise "erase" the next waypoint marker
    else {
        postMessage("VIEW_POINT", m_nextpt.get_spec("active=false"), "wpt");
        return (0);
    }

    IvPFunction *ipf = buildOF(m_ipf_type);
    if (ipf)
        ipf->setPWT(m_priority_wt);

    return (ipf);
}

//-----------------------------------------------------------
// Procedure: updateInfoIn()
//   Purpose: Update info need by the behavior from the info_buffer.
//            Error or warning messages can be posted.
//   Returns: true if no vital info is missing from the info_buffer.
//            false otherwise.
//      Note: By posting an EMessage, this sets the state_ok member
//            variable to false which will communicate the gravity
//            of the situation to the helm.

bool BHV_Waypoint_Hover::updateInfoIn() {
    bool ok1, ok2, ok3, ok4;
    m_osx = getBufferDoubleVal("NAV_X", ok1);
    m_osy = getBufferDoubleVal("NAV_Y", ok2);
    m_osv = getBufferDoubleVal("NAV_SPEED", ok3);
    m_nav_heading = getBufferDoubleVal("NAV_HEADING", ok4);

    // Must get ownship position from InfoBuffer
    if (!ok1 || !ok2 || !ok4) {
        postEMessage("No ownship X/Y/heading info in info_buffer.");
        return (false);
    }

    if (m_greedy_tour_pending) {
        m_greedy_tour_pending = false;
        XYSegList original_segl = m_waypoint_engine.getSegList();
        XYSegList shtour_segl = greedyPath(original_segl, m_osx, m_osy);
        m_waypoint_engine.setSegList(shtour_segl);
    }

    // If NAV_SPEED info is not found in the info_buffer, its
    // not a show-stopper. A warning will be posted.
    if (!ok3)
        postWMessage("No ownship speed info in info_buffer");

    return (true);
}

//-----------------------------------------------------------
// Procedure: setNextWaypoint

bool BHV_Waypoint_Hover::setNextWaypoint() {
    if (m_waypoint_engine.size() == 0)
        return (false);

    if (m_waypoint_engine.getCurrIndex()==0 &&
            m_waypoint_engine.size() > 1 &&
            m_skip_first_waypoint) {
        m_prevpt.set_vx(m_waypoint_engine.getPointX());
        m_prevpt.set_vy(m_waypoint_engine.getPointY());
        m_waypoint_engine.setCurrIndex(1);
    }

    m_waypoint_engine.setPrevPoint(m_prevpt);
    string feedback_msg = m_waypoint_engine.setNextWaypoint(m_osx, m_osy);

    if ((feedback_msg == "completed") || (feedback_msg == "cycled")) {
        if (tolower(m_var_report) != "silent") {
            string feedback_msg_aug = "behavior-name=" + m_descriptor + ",";
            feedback_msg_aug += feedback_msg;
            postMessage((m_var_report + m_var_suffix), feedback_msg_aug);
        }

        postCycleFlags();

        if (feedback_msg == "completed") {
            double this_x = m_waypoint_engine.getPointX();
            double this_y = m_waypoint_engine.getPointY();
            postWptFlags(this_x, this_y);

            setComplete();
            m_markpt.set_active(false);
            if (m_perpetual)
                m_waypoint_engine.resetForNewTraversal();
            return (false);
        }
    }

    double next_ptx = m_waypoint_engine.getPointX();
    double next_pty = m_waypoint_engine.getPointY();
    m_nextpt.set_vertex(next_ptx, next_pty);

    // By default, the steering point is the next waypoint.
    m_trackpt.set_vertex(next_ptx, next_pty);

    // If m_lead_distance is non-neg, and we've already hit the
    // first waypoint, then steer to an intermediate point that
    // is m_lead_distance away from the perpendicular intersection
    // point between the current position and the trackline.

    if (m_lead_distance >= 0) {
        int current_waypt = m_waypoint_engine.getCurrIndex();
        bool track_anchor = false;
        double tx, ty;
        if (current_waypt > 0) {
            tx = m_waypoint_engine.getPointX(current_waypt - 1);
            ty = m_waypoint_engine.getPointY(current_waypt - 1);
            track_anchor = true;
        } else if (m_waypoint_engine.getCycleCount() > 0) {
            unsigned int pt_count = m_waypoint_engine.size();
            tx = m_waypoint_engine.getPointX(pt_count - 1);
            ty = m_waypoint_engine.getPointY(pt_count - 1);
            track_anchor = true;
        } else if (m_lead_to_start) {
            if (!m_markpt.active()) {
                m_markpt.set_vertex(m_osx, m_osy);
                m_markpt.set_active(true);
            }
            tx = m_markpt.x();
            ty = m_markpt.y();
            track_anchor = true;
        }

        // tx,ty is the previous waypoint, or the start of your current trackline
        // m_nextpt is the next waypoint, or the end of your current trackline

        if (track_anchor) {
            // project vehicle position onto the trackline
            double nx, ny;
            perpSegIntPt(tx, ty, m_nextpt.x(), m_nextpt.y(), m_osx, m_osy, nx,
                    ny);
            XYPoint perp_pt(nx, ny);

            // nx, ny is vehicle's projected position on trackline

            // increase the lead distance if we are close to trackline
            double damper_factor = 1.0;
            if (m_lead_damper > 0) {
                double dist_to_trackline = hypot((nx - m_osx), (ny - m_osy));
                if (dist_to_trackline < m_lead_damper) {
                    double augment = 1 - (dist_to_trackline / m_lead_damper);
                    damper_factor += 2 * augment;
                }
            }

            // check if the lead distance is greater than the distance to the
            // next waypoint
            double angle = relAng(tx, ty, m_nextpt.x(), m_nextpt.y());
            double dist = distPointToPoint(nx, ny, m_nextpt.x(), m_nextpt.y());
            if (dist > (m_lead_distance * damper_factor))
                dist = m_lead_distance * damper_factor;

            // place the trackpoint by projecting forward from nx, ny
            m_trackpt.projectPt(perp_pt, angle, dist);

            // only start modified behavior when starting a new leg
            if (m_nextpt.x() != m_latestX || m_nextpt.y() != m_latestY) {
                // get bearing to trackpoint
                double bearing = relAng(m_osx, m_osy, m_nextpt.x(), m_nextpt.y());

                // check heading error
                if (abs(angleDiff(bearing, m_nav_heading)) > m_heading_cutoff) {
                    m_mod_is_active = true;
                    m_mod_is_ending = false;
                }
            }

            // mod is active and using waypoint as next trackpoint
            if (m_mod_is_active && !m_mod_is_ending) {
                // get bearing to trackpoint
                double bearing = relAng(m_osx, m_osy, m_nextpt.x(), m_nextpt.y());

                // check heading error
                if (abs(angleDiff(bearing, m_nav_heading)) > m_heading_cutoff) {
                    // set to next waypoint
                    m_trackpt.set_vertex(next_ptx, next_pty);
                } else {
                    // begin next phase
                    m_mod_is_ending = true;
                    m_start_time = getBufferCurrTime();
                }
            }

            // mod is active and bringing trackpoitn back to normal location
            if (m_mod_is_active && m_mod_is_ending) {
                double dist = distPointToPoint(nx, ny, m_nextpt.x(), m_nextpt.y());
                if (dist < (m_lead_distance * damper_factor)) {
                    // track point is already at next waypoint, disable mod
                    m_mod_is_active = false;
                    m_mod_is_ending = false;
                } else {
                    // move the trackpoint
                    dist -= m_lead_distance * damper_factor;
                    double elapsed = getBufferCurrTime()-m_start_time;
                    double new_dist = dist / exp(0.75*elapsed);
                    new_dist += m_lead_distance * damper_factor;
                    m_trackpt.projectPt(perp_pt, angle, new_dist);

                    if (elapsed >= 5.0) {
                        m_mod_is_active = false;
                        m_mod_is_ending = false;
                    }
                }
            }
        }
    }

    m_latestX = m_nextpt.x();
    m_latestY = m_nextpt.y();

    return (true);
}

double BHV_Waypoint_Hover::angleDiff(double a, double b) {
    double dif = a - b;
    while (dif < -360)
        dif += 360;
    while (dif > 360)
        dif -= 360;
    return dif;
}

//-----------------------------------------------------------
// Procedure: buildOF

IvPFunction *BHV_Waypoint_Hover::buildOF(string method) {
    IvPFunction *ipf = 0;

    if ((method == "roc") || (method == "rate_of_closure")) {
        bool ok = true;
        AOF_Waypoint aof_wpt(m_domain);
        ok = ok && aof_wpt.setParam("desired_speed", m_cruise_speed);
        ok = ok && aof_wpt.setParam("osx", m_osx);
        ok = ok && aof_wpt.setParam("osy", m_osy);
        ok = ok && aof_wpt.setParam("ptx", m_trackpt.x());
        ok = ok && aof_wpt.setParam("pty", m_trackpt.y());
        ok = ok && aof_wpt.initialize();

        if (ok) {
            OF_Reflector reflector(&aof_wpt);
            reflector.create(600, 500);
            //reflector.setParam("uniform_piece", "discrete @ course:3,speed:3");
            //reflector.create();
            string info = reflector.getUniformPieceStr();
            //postMessage("WAYPT_UPIECE", info);
            ipf = reflector.extractIvPFunction();
        }
    } else { // if (method == "zaic")
        ZAIC_PEAK spd_zaic(m_domain, "speed");
        spd_zaic.setParams(m_cruise_speed, 0, 2.6, 0, 0, 100);
        IvPFunction *spd_of = spd_zaic.extractIvPFunction();
        if (!spd_of)
            postWMessage("Failure on the SPD ZAIC");

        double rel_ang_to_wpt = relAng(m_osx, m_osy, m_trackpt.x(),
                m_trackpt.y());
        ZAIC_PEAK crs_zaic(m_domain, "course");
        crs_zaic.setValueWrap(true);
        crs_zaic.setParams(rel_ang_to_wpt, 0, 180, 50, 0, 100);
        IvPFunction *crs_of = crs_zaic.extractIvPFunction();
        if (!crs_of)
            postWMessage("Failure on the CRS ZAIC");

        OF_Coupler coupler;
        ipf = coupler.couple(crs_of, spd_of);
        if (!ipf)
            postWMessage("Failure on the COUPLER");
    }
    return (ipf);
}

//-----------------------------------------------------------
// Procedure: postStatusReport()

void BHV_Waypoint_Hover::postStatusReport() {
    int current_waypt = m_waypoint_engine.getCurrIndex();
    unsigned int waypt_cycles = m_waypoint_engine.getCycleCount();
    unsigned int total_hits = m_waypoint_engine.getTotalHits();
    unsigned int capture_hits = m_waypoint_engine.getCaptureHits();

    double dist_meters = hypot((m_osx - m_nextpt.x()), (m_osy - m_nextpt.y()));
    double eta_seconds = dist_meters / m_osv;

    string hits_str = uintToString(capture_hits);
    hits_str += "/" + uintToString(total_hits);

    string stat = "vname=" + m_us_name + ",";
    stat += "behavior-name=" + m_descriptor + ",";
    stat += "index=" + intToString(current_waypt) + ",";
    stat += "hits=" + hits_str + ",";
    stat += "cycles=" + uintToString(waypt_cycles) + ",";
    stat += "dist=" + doubleToString(dist_meters, 0) + ",";
    stat += "eta=" + doubleToString(eta_seconds, 0);

    if (m_var_report != "silent")
        postMessage((m_var_report + m_var_suffix), stat);
    if (m_var_index != "silent")
        postMessage((m_var_index + m_var_suffix), current_waypt);
}

//-----------------------------------------------------------
// Procedure: postViewableSegList()
//      Note: Recall that for a seglist to be drawn and erased, 
//            it must match in the label. For a seglist to be 
//            "ignored" it must set active=false.

void BHV_Waypoint_Hover::postViewableSegList() {
    XYSegList seglist = m_waypoint_engine.getSegList();
    seglist.set_label(m_us_name + "_" + m_descriptor);
    if (m_hint_vertex_color != "")
        seglist.set_color("vertex", m_hint_vertex_color);
    if (m_hint_edge_color != "")
        seglist.set_color("edge", m_hint_edge_color);
    if (m_hint_edge_size >= 0)
        seglist.set_edge_size(m_hint_edge_size);
    if (m_hint_vertex_size >= 0)
        seglist.set_vertex_size(m_hint_vertex_size);
    string segmsg = seglist.get_spec();
    postMessage("VIEW_SEGLIST", segmsg);
}

//-----------------------------------------------------------
// Procedure: postErasables
//      Note: Recall that for a seglist to be drawn and erased, 
//            it must match in the label. For a seglist to be 
//            "ignored" it must set active=false.

void BHV_Waypoint_Hover::postErasables() {
    postMessage("VIEW_POINT", m_trackpt.get_spec("active=false"), "trk");
    postMessage("VIEW_POINT", m_nextpt.get_spec("active=false"), "wpt");

    XYSegList seglist = m_waypoint_engine.getSegList();
    seglist.set_label(m_us_name + "_" + m_descriptor);
    seglist.set_active(false);
    string segmsg = seglist.get_spec();
    postMessage("VIEW_SEGLIST", segmsg);
}

//-----------------------------------------------------------
// Procedure: postCycleFlags()

void BHV_Waypoint_Hover::postCycleFlags() {
    int vsize = m_cycle_flags.size();
    for (int i = 0; i < vsize; i++) {
        string var = m_cycle_flags[i].get_var();
        string sdata = m_cycle_flags[i].get_sdata();
        double ddata = m_cycle_flags[i].get_ddata();
        if (m_cycle_flags[i].is_string())
            postRepeatableMessage(var, sdata);
        else
            postRepeatableMessage(var, ddata);
    }
}

//-----------------------------------------------------------
// Procedure: postWptFlags()

void BHV_Waypoint_Hover::postWptFlags(double x, double y) {
    string xpos = doubleToStringX(x, 2);
    string ypos = doubleToStringX(y, 2);

    int vsize = m_wpt_flags.size();
    for (int i = 0; i < vsize; i++) {
        string var = m_wpt_flags[i].get_var();
        if (m_wpt_flags[i].is_string()) {
            string sdata = m_wpt_flags[i].get_sdata();
            sdata = findReplace(sdata, "$(X)", xpos);
            sdata = findReplace(sdata, "$(Y)", ypos);
            sdata = findReplace(sdata, "$[X]", xpos);
            sdata = findReplace(sdata, "$[Y]", ypos);
            postRepeatableMessage(var, sdata);
        } else {
            double ddata = m_wpt_flags[i].get_ddata();
            postRepeatableMessage(var, ddata);
        }
    }
}

//-----------------------------------------------------------
// Procedure: handleVisualHint()

void BHV_Waypoint_Hover::handleVisualHint(string hint) {
    string param = tolower(stripBlankEnds(biteString(hint, '=')));
    string value = stripBlankEnds(hint);
    double dval = atof(value.c_str());

    if ((param == "nextpt_color") && isColor(value)) {
        m_trackpt.set_color("vertex", hint);
        m_nextpt.set_color("vertex", hint);
    } else if ((param == "nextpt_lcolor") && isColor(value)) {
        m_trackpt.set_color("label", hint);
        m_nextpt.set_color("label", hint);
    } else if ((param == "vertex_color") && isColor(value))
        m_hint_vertex_color = value;
    else if ((param == "edge_color") && isColor(value))
        m_hint_edge_color = value;
    else if ((param == "edge_size") && isNumber(value) && (dval >= 0))
        m_hint_edge_size = dval;
    else if ((param == "vertex_size") && isNumber(value) && (dval >= 0))
        m_hint_vertex_size = dval;
}

