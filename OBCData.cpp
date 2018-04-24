//	OBCData.cpp
//	Implementation of OBCData class.  This class encapsulates OBC data and parsing of the data
//	stream read from a USB port.  This is intended to be used in a separate thread to manage
//	communications with the OBC.  Data is read from the USB port as fast as possible and stored
//	in a shared buffer.  This makes the most recent data from the OBC available for the
//	main thread.

// System includes
#include <string>
#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <unistd.h>

// Local includes
#include "OBCData.h"

// System namespace
using namespace std;

SharedData shared_data;


// Initialize USB device.
// Attempt to open the device file and loop until successfully opened.
FILE* init_usb(string dev_path, string timestr)
{
	// Attempt to open USB device repeatedly until successful
	FILE* fp;
	while ( (fp = fopen(dev_path.c_str(), "r")) == NULL )
	{
		if ( errno != EBUSY && errno != ENOENT )
		{
			system_error e {errno, system_category(), dev_path};
			cerr << timestr << " " << e.what() << endl;
		}
		sleep(2);
	}

	// stty settings:
	// speed 115200 baud; rows 0; columns 0; line = 0;
	// intr = ^C; quit = ^\; erase = ^?; kill = ^U; eof = ^D; eol = <undef>; eol2 = <undef>; swtch = <undef>;
	//  start = ^Q; stop = ^S;
	// susp = ^Z; rprnt = ^R; werase = ^W; lnext = ^V; discard = ^O; min = 1; time = 0;
	// -parenb -parodd -cmspar cs8 -hupcl -cstopb cread clocal -crtscts
	// ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8
	// -opost -olcuc -ocrnl -onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0
	// -isig -icanon -iexten -echo -echoe -echok -echonl noflsh -xcase -tostop -echoprt -echoctl -echoke -flusho -extproc

	string magic{" 1:0:18b2:80:3:1c:7f:15:4:0:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0"};
	string syscmd{"/bin/stty -F " + dev_path + magic};
	system(syscmd.c_str());

	cerr << timestr << " Configured USB device: " << dev_path << endl;

	return fp;
}


// Read OBC data from the USB device continuously.
// Shared variable is updated with received data
void read_usb(FILE* fp)
{
	int c;
	int pos;
	string field;
	OBCData input_data;

	do {
		c = fgetc(fp);
		input_data.input += (char)c;

		if ( c == '$' )
		{
			// Reset input buffer
			pos = 0;
			input_data = OBCData();
		}
		else if ( c == ',' )
		{
			// Populate field at current position and clear field
			input_data.parseField(field, pos);
			pos++;
			field = "";
		}
		else if ( c == ';' )
		{
			// Populate field at current position
			input_data.parseField(field, pos);
			field = "";

			// Transfer input buffer to shared data
			shared_data.m.lock();
			shared_data.obc_data = input_data;
			shared_data.m.unlock();
		}
		else if ( isdigit(c) || c == '-' || c == '.' )
		{
			field += (char) c;
		}

	} while ( c != EOF );

	fclose(fp);
}


// Parse data read from the USB device
void OBCData::parseField(string field, int pos)
{
	switch (pos)
	{
	case 0: ms = stol(field); break;
	case 1: yy = stoi(field); break;
	case 2: mm = stoi(field); break;
	case 3: dd = stoi(field); break;
	case 4: hh = stoi(field); break;
	case 5: min = stoi(field); break;
	case 6: ss = stoi(field); break;
	case 7: lat = stod(field); break;
	case 8: lon = stod(field); break;
	case 9: alt = stod(field); break;
	case 10: ax = stod(field); break;
	case 11: ay = stod(field); break;
	case 12: az = stod(field); break;
	case 13: gx = stod(field); break;
	case 14: gy = stod(field); break;
	case 15: gz = stod(field); break;
	case 16: mx = stod(field); break;
	case 17: my = stod(field); break;
	case 18: mz = stod(field); break;
	}
}


string OBCData::display()
{
	ostringstream output_line;
	output_line << ms << " ";
	output_line << yy << " ";
	output_line << mm << " ";
	output_line << dd << " ";
	output_line << hh << " ";
	output_line << min << " ";
	output_line << ss << " ";
	output_line << lat << " ";
	output_line << lon << " ";
	output_line << alt << " ";
	output_line << ax << " ";
	output_line << ay << " ";
	output_line << az << " ";
	output_line << gx << " ";
	output_line << gy << " ";
	output_line << gz << " ";
	output_line << mx << " ";
	output_line << my << " ";
	output_line << mz;
	return output_line.str();
}

