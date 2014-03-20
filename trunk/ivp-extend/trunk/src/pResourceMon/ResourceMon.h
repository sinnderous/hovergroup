/************************************************************/
/*    NAME:                                               */
/*    ORGN: MIT                                             */
/*    FILE: ResourceMon.h                                          */
/*    DATE:                                                 */
/************************************************************/

#ifndef ResourceMon_HEADER
#define ResourceMon_HEADER

#include "MOOS/libMOOS/MOOSLib.h"
#include "JoshUtils.h"

class ResourceMon: public CMOOSApp {
public:
    ResourceMon();
    ~ResourceMon();

protected:
    bool OnNewMail(MOOSMSG_LIST &NewMail) { return true; }
    bool Iterate();
    bool OnConnectToServer() { return true; }
    bool OnStartUp() { return true; }

private:
    unsigned long m_lastActive, m_lastIdle;

};

#endif 
