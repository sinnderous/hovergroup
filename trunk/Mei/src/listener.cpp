
// Listener Modem Implementation
// 
// Listens for messages to report statistics
//
// Args: usb#
//

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "serial.h"
#include "parser.h"

using namespace std;

int main(int argc, char* argv[]){
	
int baudRate = 19200;
bool cycle = true;	
int modem = 2;		//designate Listener as 2

string usb;

	if(argc < 2){
		cout << "listener usb# \n";
		return 1;
	}
	
	usb = argv[1];
	usb = "/dev/ttyUSB" + usb;
	//modem = atoi(argv[2]);
	cout << "Usb Number: "<< usb << "\nModem Number: " << modem << "\n";
	
	SERIAL_VANILLA lport;
	lport.open_port(usb, baudRate);	

	while(!lport.active()&&cycle){
		
		char ch;
		cin.get(ch);
		if(ch == 3){
		cycle = false;
		}
				
		//Config the Modem
		//string s = "Hello world.";
		//vector<unsigned char> v( s.begin(), s.end() );
		//lport.writeData( &v[0], v.size());	
	}
	
	lport.close_port();
	
	return 0;
}
