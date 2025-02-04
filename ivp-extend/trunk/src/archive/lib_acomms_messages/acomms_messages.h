/*
 * lib_receive_info.h
 *
 *  Created on: May 24, 2012
 *      Author: josh
 */

#ifndef LIB_RECEIVE_INFO_H_
#define LIB_RECEIVE_INFO_H_

#include <sstream>
#include <vector>
#include <stdlib.h>

namespace lib_acomms_messages {

std::string string_chomp( std::string s, char c );
std::vector<std::string> tokenizeString( std::string message, std::string tokens );

class SIMPLIFIED_RECEIVE_INFO {
public:
	SIMPLIFIED_RECEIVE_INFO() {};
	SIMPLIFIED_RECEIVE_INFO( std::string msg );

	std::string vehicle_name;
	int rate, source;
	int num_frames, num_good_frames, num_bad_frames;

	std::string serializeToString();

};

class SIMPLIFIED_TRANSMIT_INFO {
public:
	SIMPLIFIED_TRANSMIT_INFO() {};
	SIMPLIFIED_TRANSMIT_INFO( std::string msg );

	std::string vehicle_name;
	int rate;
	int dest;
	int num_frames;

	std::string serializeToString();
};

class LOSS_RATE_INFO {
public:
	LOSS_RATE_INFO(){};
	LOSS_RATE_INFO(std::string msg);
	LOSS_RATE_INFO(std::string, std::string, double, double, double, double);

	std::string receiver_name;
	std::string transmitter_name;
	double sync_loss_rate;
	double bad_crc_loss_rate;
	double success_rate;
	double total_messages;

	std::string serializeToString();
};

}

#endif /* LIB_RECEIVE_INFO_H_ */
