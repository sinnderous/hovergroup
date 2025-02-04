/*
 * uPokeRandom
 *        File: PokeDB.cpp
 *  Created on: May 12, 2013
 *      Author: Josh Leighton
 */

/*
 * This file is modified from the original MOOS-IvP version to
 * support poking randomized strings.
 */

/*****************************************************************/
/*    NAME: Michael Benjamin, Henrik Schmidt, and John Leonard   */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: PokeDB.cpp                                           */
/*    DATE: May 9th 2008                                         */
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

// File modified July 19 2012 by Josh Leighton
#include "PokeDB.h"
#include "MBUtils.h"

using namespace std;

extern bool MOOSAPP_OnConnect(void*);
extern bool MOOSAPP_OnDisconnect(void*);

//------------------------------------------------------------
// Constructor

PokeDB::PokeDB() {
    m_db_start_time = 0;
    m_iteration = 0;
    m_sServerHost = "localhost";
    m_lServerPort = 9000;

    m_configure_comms_locally = false;
}

//------------------------------------------------------------
// Constructor

PokeDB::PokeDB(string g_server_host, long int g_server_port) {
    m_db_start_time = 0;
    m_iteration = 0;
    m_sServerHost = g_server_host;
    m_lServerPort = g_server_port;

    m_configure_comms_locally = false;
}

//------------------------------------------------------------
// Procedure: ConfigureComms
//      Note: Overload the MOOSApp::ConfigureComms implementation
//            which would have grabbed the port/host info from the
//            .moos file instead.

bool PokeDB::ConfigureComms() {
    //cout << "PokeDB::ConfigureComms:" << endl;
    //cout << "  m_sServerHost: " << M_Sserverhost << endl;
    //cout << "  m_lServErport: " << m_lServerPort << endl;

    if (!m_configure_comms_locally)
        return (CMOOSApp::ConfigureComms());

    //cout << "**Doing things locally. " << endl;

    //register a callback for On Connect
    m_Comms.SetOnConnectCallBack(MOOSAPP_OnConnect, this);

    //and one for the disconnect callback
    m_Comms.SetOnDisconnectCallBack(MOOSAPP_OnDisconnect, this);

    //start the comms client....
    if (m_sMOOSName.empty())
        m_sMOOSName = m_sAppName;

    m_nCommsFreq = 10;

    m_Comms.Run(m_sServerHost.c_str(), m_lServerPort, m_sMOOSName.c_str(),
            m_nCommsFreq);

    return (true);
}

vector<unsigned char> PokeDB::hexStringToBinary(string hex_data) {
//	cout << hex_data << endl;
    vector<string> sub_strings;
    string this_sub;
    for (int i = 0; i < hex_data.size(); i++) {
        if (hex_data[i] == ':') {
            sub_strings.push_back(this_sub);
            this_sub.clear();
        } else {
            this_sub.push_back(hex_data[i]);
        }
    }
    if (!this_sub.empty())
        sub_strings.push_back(this_sub);

    vector<unsigned char> output;
    for (int i = 0; i < sub_strings.size(); i++) {
        this_sub = sub_strings[i];
        if (this_sub.size() == 0 || this_sub.size() > 2) {
            cout << "Substring size invalid: " << this_sub.size() << endl;
            output.clear();
            return output;
        }
        unsigned char val = 0;
        for (int j = 0; j < this_sub.size(); j++) {
            switch (this_sub[j]) {
            case '0':
                val += 0;
                break;
            case '1':
                val += 1 * (16 - 15 * j);
                break;
            case '2':
                val += 2 * (16 - 15 * j);
                break;
            case '3':
                val += 3 * (16 - 15 * j);
                break;
            case '4':
                val += 4 * (16 - 15 * j);
                break;
            case '5':
                val += 5 * (16 - 15 * j);
                break;
            case '6':
                val += 6 * (16 - 15 * j);
                break;
            case '7':
                val += 7 * (16 - 15 * j);
                break;
            case '8':
                val += 8 * (16 - 15 * j);
                break;
            case '9':
                val += 9 * (16 - 15 * j);
                break;
            case 'a':
                val += 10 * (16 - 15 * j);
                break;
            case 'b':
                val += 11 * (16 - 15 * j);
                break;
            case 'c':
                val += 12 * (16 - 15 * j);
                break;
            case 'd':
                val += 13 * (16 - 15 * j);
                break;
            case 'e':
                val += 14 * (16 - 15 * j);
                break;
            case 'f':
                val += 15 * (16 - 15 * j);
                break;
            case 'A':
                val += 10 * (16 - 15 * j);
                break;
            case 'B':
                val += 11 * (16 - 15 * j);
                break;
            case 'C':
                val += 12 * (16 - 15 * j);
                break;
            case 'D':
                val += 13 * (16 - 15 * j);
                break;
            case 'E':
                val += 14 * (16 - 15 * j);
                break;
            case 'F':
                val += 15 * (16 - 15 * j);
                break;
            default:
                cout << "Invalid hex value: " << this_sub[j] << endl;
                output.clear();
                return output;
            }
        }
        output.push_back(val);
        cout << "parsed " << this_sub << " into " << hex << (int) val << endl;
    }
}

//------------------------------------------------------------
// Procedure: Iterate()

bool PokeDB::Iterate() {
    m_iteration++;

    // Make sure we have a chance to receive and read incoming mail
    // on the poke variables, prior to having their new values poked.
    if (m_iteration == 1)
        return (true);

    // After the first iteration, poke the all the scheduled values.
    if (m_iteration == 2) {
        unsigned int i, vsize = m_varname.size();
        for (i = 0; i < vsize; i++) {
            string varval = m_varvalue[i];
            if (strContains(varval, "@MOOSTIME")) {
                double curr_time = MOOSTime();
                string stime = doubleToStringX(curr_time, 2);
                varval = findReplace(varval, "@MOOSTIME", stime);
            }
            if (m_valtype[i] == "double") {
//				double dval = atof(varval.c_str());
//				m_Comms.Notify(m_varname[i], dval, MOOSTime());
            } else {
//				vector<unsigned char> binary = hexStringToBinary( varval );
//				if ( binary.empty() ) return false;
                int length = atoi(varval.c_str());
                m_Comms.Notify(m_varname[i], getRandomString(length));
            }
        }
    }

    if (m_iteration > 3)
        exit(0);

    if (m_iteration == 2)
        printf("\n\nPRIOR to Poking the MOOSDB \n");
    else if (m_iteration == 3)
        printf("\n\nAFTER Poking the MOOSDB \n");

    printReport();
    return (true);
}

//------------------------------------------------------------
// Procedure: OnNewMail()

bool PokeDB::OnNewMail(MOOSMSG_LIST &NewMail) {
    MOOSMSG_LIST::iterator p;

    // First scan the mail for the DB_UPTIME message to get an
    // up-to-date value of DB uptime *before* handling other vars
    if (m_db_start_time == 0) {
        for (p = NewMail.begin(); p != NewMail.end(); p++) {
            CMOOSMsg &msg = *p;
            if (msg.GetKey() == "DB_UPTIME")
                m_db_start_time = MOOSTime() - msg.GetDouble();
        }
    }

    // Update the values of all variables we have registered for.
    // All variables "values" are stored as strings. We let MOOS
    // tell us the type of the variable, and we keep track of the
    // type locally, just so we can put quotes around string values.
    for (p = NewMail.begin(); p != NewMail.end(); p++) {
        CMOOSMsg &msg = *p;
        updateVariable(msg);
    }
    return (true);
}

//------------------------------------------------------------
// Procedure: OnStartUp()

bool PokeDB::OnStartUp() {
    CMOOSApp::OnStartUp();

    registerVariables();
    return (true);
}

//------------------------------------------------------------
// Procedure: OnConnectToServer()

bool PokeDB::OnConnectToServer() {
    registerVariables();
    return (true);
}

//------------------------------------------------------------
// Procedure: setPokeDouble

void PokeDB::setPokeDouble(const string& varname, const string& value) {
    m_varname.push_back(varname);
    m_valtype.push_back("double");
    m_varvalue.push_back(value);

    m_dvalue_read.push_back("");
    m_svalue_read.push_back("");
    m_source_read.push_back("");
    m_valtype_read.push_back("");
    m_wrtime_read.push_back("");
    m_varname_recd.push_back(false);
}

//------------------------------------------------------------
// Procedure: setPokeString

void PokeDB::setPokeString(const string& varname, const string& value) {
    m_varname.push_back(varname);
    m_valtype.push_back("string");
    m_varvalue.push_back(value);

    m_dvalue_read.push_back("");
    m_svalue_read.push_back("");
    m_source_read.push_back("");
    m_valtype_read.push_back("");
    m_wrtime_read.push_back("");
    m_varname_recd.push_back(false);
}

//------------------------------------------------------------
// Procedure: registerVariables

void PokeDB::registerVariables() {
    unsigned int i, vsize = m_varname.size();
    for (i = 0; i < vsize; i++)
        m_Comms.Register(m_varname[i], 0);

    m_Comms.Register("DB_UPTIME", 0);
}

//------------------------------------------------------------
// Procedure: updateVariable
//      Note: Will read a MOOS Mail message and grab the fields
//            and update the variable only if its in the vector 
//            of variables vector<string> vars.

void PokeDB::updateVariable(CMOOSMsg &msg) {
    string varname = msg.GetKey();

    int ix = -1;
    unsigned int index, vsize = m_varname.size();
    for (index = 0; index < vsize; index++)
        if (m_varname[index] == varname)
            ix = index;
    if (ix == -1)
        return;

    double vtime = msg.GetTime() - m_db_start_time;
    string vtime_str = doubleToString(vtime, 2);
    vtime_str = dstringCompact(vtime_str);

    m_source_read[ix] = msg.GetSource();
    m_wrtime_read[ix] = vtime_str;

    if (msg.IsString()) {
        m_svalue_read[ix] = msg.GetString();
        m_valtype_read[ix] = "string";
    } else if (msg.IsDouble()) {
        m_dvalue_read[ix] = doubleToString(msg.GetDouble());
        m_valtype_read[ix] = "double";
    }
}

//------------------------------------------------------------
// Procedure: printReport()

void PokeDB::printReport() {
    printf("  %-22s", "VarName");

    printf("%-12s", "(S)ource");
    printf("%-12s", "(T)ime");
    printf("VarValue\n");

    printf("  %-22s", "----------------");
    printf("%-12s", "----------");
    printf("%-12s", "----------");
    printf(" -------------\n");

    unsigned int i, vsize = m_varname.size();
    for (i = 0; i < vsize; i++) {

        printf("  %-22s ", m_varname[i].c_str());
        printf("%-12s", m_source_read[i].c_str());
        printf("%-12s", m_wrtime_read[i].c_str());
        if (m_valtype_read[i] == "string") {
            if (m_svalue_read[i] != "") {
                printf("\"%s\"", m_svalue_read[i].c_str());
            } else
                printf("n/a");
        } else if (m_valtype_read[i] == "double")
            printf("%s", m_dvalue_read[i].c_str());
        printf("\n");
    }
}

std::string PokeDB::getRandomString(int length) {
    srand((unsigned) time(NULL));

    std::stringstream ss;
    const int passLen = length;
    for (int i = 0; i < passLen; i++) {
        char num = (char) (rand() % 62);
        if (num < 10)
            num += '0';
        else if (num < 36)
            num += 'A' - 10;
        else
            num += 'a' - 36;
        ss << num;
    }

    return ss.str();
}

