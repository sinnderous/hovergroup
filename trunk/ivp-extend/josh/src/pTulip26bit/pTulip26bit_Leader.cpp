/*
 * pTulip26bit_Leader.cpp
 *
 *  Created on: Oct 16, 2012
 *      Author: josh
 */

#include "Tulip26bit.h"

void Tulip26bit::onTransmit_leader() {
	unsigned char transmit_x = LinearEncode( m_set_x, m_osx_minimum, m_osx_maximum, 5);
	unsigned char transmit_y = LinearEncode( m_set_y, m_osy_minimum, m_osy_maximum, 5);
	unsigned char range = 1;

//	std::cout << std::dec << "x-bin: " << (int) transmit_x << "  y-bin: " << (int) transmit_y << std::endl;

	std::vector<unsigned char> data (2, 0);
	data[1] = (transmit_y<<3) + range;
	data[0] = transmit_x;

//	std::cout << "Transmitting: " << std::hex << (int) data[0] << " " << (int) data[1] << std::endl;

	m_Comms.Notify("ACOMMS_TRANSMIT_DATA_BINARY", &data[0], 2);
}

void Tulip26bit::onGoodReceive_leader( const std::string data ) {
	if ( data.size() != 2 ) {
		std::stringstream ss;
		ss << "Wrong data length: " << data.size();
		handleDebug( ss.str() );
	}

	double received_x = LinearDecode( data[0], m_osx_minimum, m_osx_maximum, 5);
	double received_y = LinearDecode( data[1]>>3, m_osy_minimum, m_osy_maximum, 5);
	unsigned char received_range = data[1] & 0x07;

	m_Comms.Notify("FOLLOWER_RANGE_BIN", (int)received_range);
	m_Comms.Notify("FOLLOWER_X", received_x);
	m_Comms.Notify("FOLLOWER_Y", received_y);
	publishLeaderPos();
    m_Comms.Notify("FOLLOWER_PACKET",1);
}

void Tulip26bit::onBadReceive_leader() {
    publishLeaderPos();
    m_Comms.Notify("FOLLOWER_PACKET",0);
}

void Tulip26bit::publishLeaderPos() {
    m_Comms.Notify("LEADER_X", m_osx);
    m_Comms.Notify("LEADER_Y", m_osy);
    m_Comms.Notify("LEADER_RANGE", m_target_range);
}
