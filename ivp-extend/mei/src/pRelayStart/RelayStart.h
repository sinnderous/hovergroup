/************************************************************/
/*    NAME:                                               */
/*    ORGN: MIT                                             */
/*    FILE: RelayStart.h                                          */
/*    DATE:                                                 */
/************************************************************/

#ifndef RelayStart_HEADER
#define RelayStart_HEADER

#include "MOOS/libMOOS/MOOSLib.h"

using namespace std;

class RelayStart : public CMOOSApp
{
public:
	RelayStart();
	virtual ~RelayStart();

	bool OnNewMail(MOOSMSG_LIST &NewMail);
	bool Iterate();
	bool OnConnectToServer();
	bool OnStartUp();
	string getRandomString(int);

protected:
		int rate,mail_counter,length;
		string driver_status;
};

#endif 
