/************************************************************/
/*    NAME:                                                 */
/*    ORGN: MIT                                             */
/*    FILE: NSFModem.h                                      */
/*    DATE: 19-08-2013                                      */
/************************************************************/

#ifndef NSFModem_HEADER
#define NSFModem_HEADER

#include "MOOS/libMOOS/MOOSLib.h"

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

class NSFModem: public CMOOSApp {
public:
	NSFModem();
	~NSFModem();

	enum NSFModemState {
		Starting, Stopping, Running
	};

	enum NSFVoltage {
		OKAY,
		LOW
	};

protected:
	bool OnNewMail(MOOSMSG_LIST &NewMail);
	bool Iterate();
	bool OnConnectToServer();
	bool OnStartUp();
	void RegisterVariables();

private:
	NSFModemState m_state;
	int m_current_power_level;    // the current power level
	int m_requested_power_level;  // the latest power level request

	static const int debounce_time = 30; // ms
	static const int gap_time = 5; // ms

	std::ofstream m_power_increase_pin_value,
		m_power_increase_pin_direction,
		m_power_decrease_pin_value,
		m_power_decrease_pin_direction,
		m_voltage_sense_pin_direction;

    std::ifstream m_voltage_sense_pin_value;

	boost::thread m_power_write_thread; // a thread that takes care of writing to
										// the pins to prevent OnNewMail from
										// blocking.

private:
	void power_write_loop();

	void print_power_status();
};

#endif 
