/*
 * 
 *        File: SmartCommsVehicle.cpp
 *  Created on: 
 *      Author: 
 */

#include <iterator>
#include "MBUtils.h"
#include "SmartCommsVehicle.h"

using namespace std;

//---------------------------------------------------------
// Constructor

SmartCommsVehicle::SmartCommsVehicle()
{
    m_iterations = 0;
    m_timewarp   = 1;
}

//---------------------------------------------------------
// Destructor

SmartCommsVehicle::~SmartCommsVehicle()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool SmartCommsVehicle::OnNewMail(MOOSMSG_LIST &NewMail)
{
    MOOSMSG_LIST::iterator p;

    for(p=NewMail.begin(); p!=NewMail.end(); p++) {
        CMOOSMsg &msg = *p;

#if 0 // Keep these around just for template
    string key   = msg.GetKey();
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif
    }

    return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool SmartCommsVehicle::OnConnectToServer()
{
    // register for variables here
    // possibly look at the mission file?
    // m_MissionReader.GetConfigurationParam("Name", <string>);
    // m_Comms.Register("VARNAME", 0);

    RegisterVariables();
    return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool SmartCommsVehicle::Iterate()
{
    m_iterations++;
    return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool SmartCommsVehicle::OnStartUp()
{
    list<string> sParams;
    m_MissionReader.EnableVerbatimQuoting(false);
    if(m_MissionReader.GetConfiguration(GetAppName(), sParams)) {
        list<string>::iterator p;
        for(p=sParams.begin(); p!=sParams.end(); p++) {
            string original_line = *p;
            string param = stripBlankEnds(toupper(biteString(*p, '=')));
            string value = stripBlankEnds(*p);
            
            if(param == "FOO") {
                //handled
            } else if(param == "BAR") {
                //handled
            }
        }
    }

    m_timewarp = GetMOOSTimeWarp();

    RegisterVariables();	
    return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables

void SmartCommsVehicle::RegisterVariables()
{
    // m_Comms.Register("FOOBAR", 0);
}

