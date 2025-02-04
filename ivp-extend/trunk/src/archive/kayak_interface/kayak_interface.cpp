#include <iterator>
#include "kayak_interface.h"
#include "MBUtils.h"

using namespace std;
using namespace boost::asio;
using namespace boost::posix_time;

KAYAK_INTERFACE::KAYAK_INTERFACE() : port(io), timeout(io) {
	// initialize buffers with some size larger than should ever be needed
	writeBuffer = vector<unsigned char> (1000, 0);
	readBuffer = vector<unsigned char> (1000, 0);

	bytesToWrite = 0;
	asyncBytesRead = 0;
	data_available = false;
	stop_requested = false;

	my_baud_rate = 115200;
	my_port_name = "/dev/ttyO0";

	string_buffer = "";

	m_desired_rudder = 0;
	m_desired_thrust = 0;
	newCommand = false;
	m_last_command_time = 0;

	STARTUP_THRUST = 20;
	THRUST_CUTOFF = 0;
	MIN_UPDATE_PERIOD = .25;
	INVERT_RUDDER = false;
	RUDDER_OFFSET = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool KAYAK_INTERFACE::OnNewMail(MOOSMSG_LIST &NewMail)
{
	MOOSMSG_LIST::iterator p;
	for(p=NewMail.begin(); p!=NewMail.end(); p++) {
		CMOOSMsg &msg = *p;
		string key  = msg.GetKey();

		if (key == "DESIRED_RUDDER") {
			m_desired_rudder = mapRudder( (int) msg.GetDouble() );
			newCommand = true;
		} else if (key == "DESIRED_THRUST" ) {
			m_desired_thrust = mapThrust( (int) msg.GetDouble() );
			newCommand = true;
		} else if ( key == "CALIBRATE_COMPASS" ) {
			toggleCompassCalibration();
		}
	}

	return(true);
}

int KAYAK_INTERFACE::mapRudder( int rudder_command ) {
	rudder_command += RUDDER_OFFSET;
	if ( INVERT_RUDDER )
		rudder_command*=-1;

	if ( rudder_command > 90 )
		rudder_command = 90;
	else if ( rudder_command < -90 )
		rudder_command = -90;

	return rudder_command;
}

int KAYAK_INTERFACE::mapThrust( int thrust_command ) {
	if ( thrust_command==0 || thrust_command < THRUST_CUTOFF )
		return 0;
	else if ( thrust_command >= 100 )
		return 100;
	else
		return (int) (thrust_command*(100.0-STARTUP_THRUST)/100.0 + STARTUP_THRUST);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool KAYAK_INTERFACE::OnConnectToServer()
{
	// I prefer to read my config file here, so I can be sure I finish reading it before doing anything else
	STRING_LIST sParams;
	m_MissionReader.EnableVerbatimQuoting(false);
	m_MissionReader.GetConfiguration(GetAppName(), sParams);

	bool ok1, ok2;
	double * ptr = &m_lat_origin;
	ok1 = m_MissionReader.GetValue("LatOrigin", *ptr);
	ptr = &m_lon_origin;
	ok2 = m_MissionReader.GetValue("LongOrigin", *ptr);
	if ( !ok1 || !ok2 ) {
		cout << "Error reading Lat/Lon origin from MOOS file." << endl;
		return false;
	}

	STRING_LIST::iterator p;
	for (p=sParams.begin(); p!=sParams.end(); p++) {
		string sLine = *p;
		string sVarName = MOOSChomp(sLine, "=");
		sLine = stripBlankEnds(sLine);

		// this is the variable name we found
		cout << sVarName << endl;

		// match names to what you expect, and parse the values
		if (MOOSStrCmp(sVarName, "BAUD_RATE")) {
			if(!strContains(sLine, " "))
				my_baud_rate = boost::lexical_cast<int>(stripBlankEnds(sLine));
		} else if (MOOSStrCmp(sVarName, "PORT_NAME")) {
			if(!strContains(sLine, " "))
				my_port_name = stripBlankEnds(sLine);
		} else if ( sVarName == "STARTUP_THRUST" ) {
			if(!strContains(sLine, " "))
				STARTUP_THRUST = atoi(stripBlankEnds(sLine).c_str());
		} else if ( sVarName == "THRUST_CUTOFF" ) {
			if(!strContains(sLine, " "))
				THRUST_CUTOFF = atoi(stripBlankEnds(sLine).c_str());
		} else if ( sVarName == "MIN_UPDATE_PERIOD" ) {
			if(!strContains(sLine, " "))
				MIN_UPDATE_PERIOD = atof(stripBlankEnds(sLine).c_str());
		} else if ( sVarName == "INVERT_RUDDER" ) {
			if(!strContains(sLine, " ")) {
				int tmp = atoi(stripBlankEnds(sLine).c_str());
				if ( tmp==0 )
					INVERT_RUDDER = false;
				else if ( tmp==1 )
					INVERT_RUDDER = true;
			}
		} else if ( sVarName == "RUDDER_OFFSET" ) {
			if(!strContains(sLine, " "))
				RUDDER_OFFSET = atoi(stripBlankEnds(sLine).c_str());
		}
	}

	if ( STARTUP_THRUST < 0 || STARTUP_THRUST > 99 ) {
		cout << "Invalid startup thrust set: " << STARTUP_THRUST << endl;
		return false;
	}

	RegisterVariables();

	open_port( my_port_name, my_baud_rate );
	//serial_thread = boost::thread(boost::bind(&KAYAK_INTERFACE::serialLoop, this));

	return(true);
}

void KAYAK_INTERFACE::toggleCompassCalibration() {
	char msg [] = "cal\n";
	writeData( &msg[0], 4 );
}

//------------------------------------------------------------
// Procedure: RegisterVariables

void KAYAK_INTERFACE::RegisterVariables()
{
	m_Comms.Register("DESIRED_RUDDER", 0);
	m_Comms.Register("DESIRED_THRUST", 0);
	m_Comms.Register("CALIBRATE_COMPASS", 0);
}


void KAYAK_INTERFACE::sendMotorCommands() {
	char tmp [100];
	int size = sprintf(&tmp[0], "mtr:thrust=%d,angle=%d\n",
			m_desired_thrust, m_desired_rudder);
	writeData( &tmp[0], size );

//	cout << "sending command string: " << string(tmp) << endl;
}

//---------------------------------------------------------
// Procedure: Iterate()

bool KAYAK_INTERFACE::Iterate()
{
	if ( MOOSTime() > m_last_command_time + MIN_UPDATE_PERIOD ) {
		sendMotorCommands();
		m_last_command_time = MOOSTime();
		newCommand = false;
	}
//	m_Comms.Notify("VARIABLE_NAME", "I did something.");
	return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//      Note: happens before connection is open

bool KAYAK_INTERFACE::OnStartUp()
{
	// I prefer to do nothing here
	return(true);
}

void KAYAK_INTERFACE::open_port( string port_name, int baudRate ) {
	// open the serial port
	port.open(port_name);

	// serial port must be configured after being opened
	port.set_option(serial_port_base::baud_rate(baudRate));
	port.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none));
	port.set_option(serial_port_base::parity(serial_port_base::parity::none));
	port.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));
	port.set_option(serial_port_base::character_size(8));

	// start the background thread
	serial_thread = boost::thread(boost::bind(&KAYAK_INTERFACE::serialLoop, this));
}

void KAYAK_INTERFACE::close_port() {
	stop_requested = true;
	serial_thread.join();
	port.close();
}

void KAYAK_INTERFACE::writeData( unsigned char *ptr, int length ) {
	writeBufferMutex.lock();
	memcpy(&writeBuffer[bytesToWrite], ptr, length);
	bytesToWrite += length;
	writeBufferMutex.unlock();
}

void KAYAK_INTERFACE::writeData( char * ptr, int length ) {
	writeBufferMutex.lock();
	memcpy(&writeBuffer[bytesToWrite], ptr, length);
	bytesToWrite += length;
	writeBufferMutex.unlock();
}

void KAYAK_INTERFACE::read_handler(bool& data_available, deadline_timer& timeout,
	const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (error || !bytes_transferred) {
		// no data read
		data_available = false;
		return;
	}
	data_available = true;
	asyncBytesRead = bytes_transferred;
	timeout.cancel();
}

void KAYAK_INTERFACE::wait_callback(serial_port& ser_port, const boost::system::error_code& error)
{
	if (error) {
		// data read, timeout cancelled
		return;
	}
	port.cancel(); // read_callback fires with error
}

void KAYAK_INTERFACE::processWriteBuffer() {
	// take out lock
	writeBufferMutex.lock();
	if ( bytesToWrite > 0 ) {
		// if there is data waiting, copy it to a local buffer
		vector<unsigned char> localWriteBuffer( bytesToWrite, 0 );
		memcpy(&localWriteBuffer[0], &writeBuffer[0], bytesToWrite);
		bytesToWrite = 0;
		// release lock to prevent outside write requests from blocking on serial write
		writeBufferMutex.unlock();

//		cout << endl << dec << "writing " << localWriteBuffer.size() << " bytes" << endl;

		// simple synchronous serial write
		port.write_some( buffer(localWriteBuffer, localWriteBuffer.size()) );
	} else {
		// no data to write, release lock
		writeBufferMutex.unlock();
	}
}

// split string into substrings using provided tokens
vector<string> KAYAK_INTERFACE::tokenizeString( string message, string tokens ) {
	char * cstr = new char [message.size() + 1];
	strcpy(cstr, message.c_str());
	char * ctokens = new char [tokens.size()+1];
	strcpy(ctokens, tokens.c_str());

	char * pch;
	vector<string> subs;

	pch = strtok(cstr,ctokens);
	while (pch != NULL) {
		subs.push_back(string(pch));
		pch  = strtok(NULL,ctokens);
	}

	delete cstr;
	delete ctokens;

	return subs;
}

void KAYAK_INTERFACE::parseGPRMC( string msg ) {
	// for example:
	//	$GPRMC,174929,A,4221.5066,N,07105.2446,W,000.1,000.0,050412,015.6,W*70
	// lat and lon are in degrees and minutes
	// 4221.5066 = 42 degrees, 21.5066 minutes
	vector<string> subs = tokenizeString( msg, ",");

	// only proceed if we have lock
	if ( subs[2] != "A" ) {
		m_Comms.Notify("GPS_LOCK", "false");
	} else {
		m_Comms.Notify("GPS_LOCK", "true");

		string date_string = subs[9];
		string time_string = subs[1];
		string modified_date = "20" + date_string.substr(4,2) +
				date_string.substr(2,2) +
				date_string.substr(0,2);
		string composite = modified_date+"T"+time_string;
		ptime t(from_iso_string(composite));

		m_Comms.Notify("GPS_PTIME", to_simple_string(t));

		double seconds = t.time_of_day().total_milliseconds()/1000.0;
		m_Comms.Notify("GPS_TIME_SECONDS", seconds);

		string lat_string = subs[3];
		string lon_string = subs[5];
//		double temp_lat = atof(subs[3].c_str());
//		double temp_lon = atof(subs[5].c_str());
//		double lat_degrees = floor(temp_lat/100.0);
//		double lon_degrees = floor(temp_lon/100.0);
//		double lat_minutes = temp_lat - lat_degrees*100.0;
//		double lon_minutes = temp_lon - lon_degrees*100.0;
//		m_lat = lat_degrees + lat_minutes/60.0;
//		m_lon = lon_degrees + lon_minutes/60.0;

		m_lat = 0;
		m_lon = 0;
		m_lat += atof(lat_string.substr(0,2).c_str());
		m_lat += atof(lat_string.substr(2,lat_string.size()-2).c_str())/60;
		m_lon += atof(lon_string.substr(0,3).c_str());
		m_lon += atof(lon_string.substr(3,lon_string.size()-3).c_str())/60;
		if ( subs[4] == "S" )
			m_lat*=-1;
		if ( subs[6] == "W" )
			m_lon*=-1;

		m_Comms.Notify("GPS_LATITUDE", m_lat);
		m_Comms.Notify("GPS_LONGITUDE", m_lon);

		m_speed =  atof(subs[7].c_str());
		m_course = atof(subs[8].c_str());

		m_Comms.Notify("GPS_SPEED", m_speed);
		m_Comms.Notify("GPS_COURSE", m_course);

		double latError = m_lat - m_lat_origin;
		double lonError = m_lon - m_lon_origin;
		double rlat = m_lat * M_PI/180;
		double latErrorRad = latError * M_PI/180;
		double lonErrorRad = lonError * M_PI/180 * cos(rlat);

		double a = 6378137; //equatorial radius in m
		double b = 6356752.3; //polar radius in m
		double localEarthRadius = sqrt( ( pow(a,4) * pow( cos( rlat ), 2 ) +
			pow( b, 4 ) * pow( sin( rlat ), 2 ) ) / ( pow( a * cos( rlat ), 2 ) +
			pow( b * sin( rlat ), 2 ) ) );

		double latErrorMeters = latErrorRad * localEarthRadius;
		double lonErrorMeters = lonErrorRad * localEarthRadius;

		m_Comms.Notify("GPS_X", lonErrorMeters);
		m_Comms.Notify("GPS_Y", latErrorMeters);

		m_Comms.Notify("GPS_MAGNETIC_VARIATION", atof(subs[10].c_str()));
	}
}

void KAYAK_INTERFACE::parseSensors( string msg ) {
//	cout << msg << endl;
	vector<string> subs = tokenizeString( msg, ",=" );
	double heading = atoi(subs[1].c_str())/10.0;
	double voltage = atoi(subs[5].c_str())/100.0;

	m_Comms.Notify("ARDUINO_HEADING", heading);
	m_Comms.Notify("VOLTAGE", voltage);
//	cout << heading << " " << voltage << endl;
}

void KAYAK_INTERFACE::parseLine( string msg ) {
	if ( msg.find("$GPRMC") != -1 ) {
		int index = msg.find("$GPRMC");
		parseGPRMC( msg.substr(index, msg.length()-index) );
//		cout << "found gps: " << index << endl;
	} else if ( msg.find("heading") != -1 ) {
		int index = msg.find("heading");
		parseSensors( msg.substr(index, msg.length()-index) );
//		cout << "found sensors: " << index << endl;
	} else {
		m_Comms.Notify("ARDUINO_MESSAGES", msg);
		cout << "WARNING: Unhandled Arduino message: " << msg << endl;
	}
}

void KAYAK_INTERFACE::serialLoop() {
	while (!stop_requested) {

		processWriteBuffer();

		// set up an asynchronous read that will read up to 100 bytes, but will return as soon as any bytes area read
		// bytes read will be placed into readBuffer starting at index 0
		port.async_read_some( buffer( &readBuffer[0], 1000 ),
				boost::bind( &KAYAK_INTERFACE::read_handler, this, boost::ref(data_available), boost::ref(timeout),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred ) );
		// setup a timer that will prevent the asynchronous operation for more than 100 ms
		timeout.expires_from_now( boost::posix_time::milliseconds(1000) );
		timeout.async_wait( boost::bind( &KAYAK_INTERFACE::wait_callback, this, boost::ref(port),
				boost::asio::placeholders::error ) );

		// reset then run the io service to start the asynchronous operation
		io.reset();
		io.run();

		if (data_available) {
			string_buffer += string(readBuffer.begin(), readBuffer.begin()+=asyncBytesRead);
//			cout << string_buffer << endl;
			if ( string_buffer.find('\n',1)!=string::npos ) {
//				cout << string_buffer << endl;
				int index = string_buffer.find('\n',1);
				parseLine( string_buffer.substr(0, index) );
				string_buffer = string_buffer.substr( index, string_buffer.size()-index );
			}
		}
	}
}
