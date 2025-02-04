/*
 * acomms_alog_parser_main.cpp
 *
 *  Created on: Jun 1, 2012
 *      Author: josh
 */

// copy to your own tree before making modifications
// this version is subject to being changed or overwritten
using namespace std;

#include "tdoa.pb.h"
#include "goby/acomms/protobuf/mm_driver.pb.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <google/protobuf/text_format.h>
#include "LogUtils.h"
#include "MBUtils.h"
#include "MOOS/libMOOS/Utils/ProcessConfigReader.h"
#include <stdio.h>
#include <vector>
#include <time.h>
#include <map>
#include <math.h>
#include <algorithm>
#include <stdlib.h>

using namespace std;
using namespace boost::posix_time;

#define MAX_LINE_LENGTH 10000

ALogEntry getNextRawALogEntry_josh(FILE *fileptr, bool allstrings = false);
void printHelp();

ofstream output;

double last_print_time = -100;

int mfd_pow, mfd_ratio, status, spl, stdev_noise;
//int snr_in, snr_out, spl, stddev_noise, mse_equalizer;

int main(int argc, char *argv[]) {
	// Look for a request for version information
	if (scanArgs(argc, argv, "-h", "--help")) {
		printHelp();
		return (0);
	}

	if (argc != 2) {
		cout << "Insufficient arguments - must provide input log file." << endl;
		return 0;
	}
	FILE *logfile = fopen(argv[1], "r");
	for (int i = 0; i < 5; i++) {
		getNextRawLine(logfile);
	}

	// open output file and print header
	std::string logname(argv[1]);
	logname.replace(logname.end()-=5, logname.end(), "_tdoa.csv");

	output.open(logname.c_str());
	output << "moos_time,id1,x1,y1,toa1,id2,x2,y2,toa2,id3,x3,y3,toa3" << endl;

	TDOAUpdate old_tdoa;
	old_tdoa.set_local_id(-1);
	old_tdoa.set_cycle_state(TDOAUpdate_StateEnum_PAUSED);

	// read first alog entry
	ALogEntry entry = getNextRawALogEntry_josh(logfile);
	while (entry.getStatus() != "eof") {
		// variable name and message time
		string key = entry.getVarName();
		double msg_time = entry.getTimeStamp();

		if (key == "TDOA_PROTOBUF_DEBUG") {
			string msg_val = entry.getStringVal();

			TDOAUpdate tdoa;
			string temp;
			while (msg_val.find("<|>") != string::npos) {
				msg_val.replace(msg_val.find("<|>"), 3, "\n");
			}
			string const* ptr = &msg_val;
			if (google::protobuf::TextFormat::ParseFromString(*ptr, &tdoa)) {
			    if (old_tdoa.data_size() != tdoa.data_size() && tdoa.data_size()>0) {
                    std::stringstream ss;
                    ss << msg_time << ",";
                    for (int i=0; i<3; i++) {
                        bool found = false;
                        for (int j=0; j<tdoa.data_size(); j++) {
                            if (tdoa.data(j).id() == i+1) {
                                ss << tdoa.data(j).id() << ",";
                                ss << tdoa.data(j).x() << ",";
                                ss << tdoa.data(j).y() << ",";
                                ss << tdoa.data(j).toa();
                                found = true;
                            }
                        }

                        if (!found) {
                            ss << ",,,";
                        }

                        if (i != 2) {
                            ss << ",";
                        }
                    }

                    output << ss.str() << endl;
			    }
			    old_tdoa.Clear();
			    old_tdoa.CopyFrom(tdoa);
			}
		}

		entry = getNextRawALogEntry_josh(logfile);
	}

	return 0;
}

// nothing past here to worry about

void printHelp() {

}

ALogEntry getNextRawALogEntry_josh(FILE *fileptr, bool allstrings) {
	ALogEntry entry;
	if (!fileptr) {
		cout << "failed getNextRawALogEntry() - null file pointer" << endl;
		entry.setStatus("invalid");
		return (entry);
	}

	bool EOLine = false;
	bool EOFile = false;
	int buffix = 0;
	int lineix = 0;
	int myint = '\0';
	char buff[MAX_LINE_LENGTH];

	string time, var, rawsrc, val;

	// Simple state machine:
	//   0: time
	//   1: between time and variable
	//   2: variable
	//   3: between variable and source
	//   4: source
	//   5: between source and value
	//   6: value
	int state = 0;

	while ((!EOLine) && (!EOFile) && (lineix < MAX_LINE_LENGTH)) {
		//		cout << "state: " << state << endl;
		myint = fgetc(fileptr);
		unsigned char mychar = myint;
		switch (myint) {
		case EOF:
			EOFile = true;
			break;
		case ' ':
			if (state == 6) {
				buff[buffix] = mychar;
				buffix++;
			}
			//			break;
		case '\t':
			if (state == 0) {
				buff[buffix] = '\0';
				time = buff;
				buffix = 0;
				state = 1;
			} else if (state == 2) {
				buff[buffix] = '\0';
				var = buff;
				buffix = 0;
				state = 3;
			} else if (state == 4) {
				buff[buffix] = '\0';
				rawsrc = buff;
				buffix = 0;
				state = 5;
			}
			break;
		case '\n':
			buff[buffix] = '\0'; // attach terminating NULL
			val = buff;
			EOLine = true;
			break;
		default:
			if (state == 1)
				state = 2;
			else if (state == 3)
				state = 4;
			else if (state == 5)
				state = 6;
			buff[buffix] = mychar;
			buffix++;
		}
		lineix++;
	}

	string src = biteString(rawsrc, ':');
	string srcaux = rawsrc;

	val = stripBlankEnds(val);

	//	cout << "t:" << time << " v:" << var << " s:" << src << " v:" << val << endl;

	if ((time != "") && (var != "") && (src != "") && (val != "")
			&& isNumber(time)) {
		if (allstrings || !isNumber(val))
			entry.set(atof(time.c_str()), var, src, srcaux, val);
		else
			entry.set(atof(time.c_str()), var, src, srcaux, atof(val.c_str()));
	} else {
		if (EOFile)
			entry.setStatus("eof");
		else
			entry.setStatus("invalid");
	}

	return (entry);
}
