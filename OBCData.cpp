//	OBCData.cpp
//	Implementation of OBCData class.  This class encapsulates OBC data and parsing of the data
//	stream read from a USB port.  This is intended to be used in a separate thread to manage
//	communications with the OBC.  Data is read from the USB port as fast as possible and stored
//	in a shared buffer.  This makes the most recent data from the OBC available for the
//	main thread.

// System includes
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <ctime>
#include <unistd.h>

// Local includes
#include "OBCData.h"

// System namespace
using namespace std;

// Import from baslerctrl.cpp
extern string get_time_string();

SharedData shared_data;


// Create a formatted string from the current system time
string get_time_string()
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char buffer[80];
	strftime(buffer, sizeof(buffer), "%b %d %X", timeinfo);
	return string(buffer);
}


// Initialize USB device.
// Attempt to open the device file and loop until successfully opened.
FILE* init_usb(string dev_path, string timestr)
{
	time_t tm1;
	time(&tm1);
	
	// Attempt to open USB device repeatedly until successful
	FILE* fp;
	while ( (fp = fopen(dev_path.c_str(), "r")) == NULL )
	{
		if ( errno != EBUSY && errno != ENOENT )
		{
			system_error e {errno, system_category(), dev_path};
			cerr << timestr << " " << e.what() << endl;
		}
		time_t tm2;
		time(&tm2);
		if ( 20 < difftime(tm2, tm1) )
		{
			throw system_error {errno, system_category(), timestr + " OBC not found on " + dev_path};
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
	bool discard = true;

	do {
		try
		{
			c = fgetc(fp);
			input_data.input += (char)c;
			if ( discard && c != '$' )
				// Discard input data until next record delimiter
				continue;

			if ( c == '$' )
			{
				// Found start of record so reset input buffer
				discard = false;
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
				input_data.obc_mode = shared_data.obc_data.obc_mode;
				shared_data.obc_data = input_data;
				shared_data.available = true;
				shared_data.m.unlock();
			}
			else if ( isdigit(c) || c == '-' || c == '.' )
			{
				field += (char) c;
			}
		}
		catch (const exception &e)
		{
			cerr << get_time_string() << " Exception in read_usb(), pos = " << pos;
			cerr << " field = [" << field << "] " << e.what() << endl;

			// Ignore input until next record delimiter
			discard = true;
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


// Display the entire OBC data record
string OBCData::display()
{
	ostringstream output_line;
	output_line << ms << " ";
	output_line << yy << " ";
	output_line << setw(2) << setfill('0') << mm << " ";
	output_line << setw(2) << setfill('0') << dd << " ";
	output_line << setw(2) << setfill('0') << hh << " ";
	output_line << setw(2) << setfill('0') << min << " ";
	output_line << setw(2) << setfill('0') << ss << " ";
	output_line.flags(ios_base::fixed);
	output_line.precision(5);
	output_line << lat << " ";
	output_line << lon << " ";
	output_line.precision(2);
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


// Create a string representation of the time that has the following format:
//	YYYYMMDD_HHMMSS_NNNNNNN
// The NNNNNNN value increases monotonically to provide a unique value during
// the lifetime of program execution. 
string OBCData::getTimeString()
{
	ostringstream output_line;
	if ( obc_mode )
	{
		//output_line << "OBC";
		output_line << yy;
		output_line << setw(2) << setfill('0') << mm;
		output_line << setw(2) << setfill('0') << dd << "_";
		output_line << setw(2) << setfill('0') << hh;
		output_line << setw(2) << setfill('0') << min;
		output_line << setw(2) << setfill('0') << ss << "_";
	}
	else
	{
		// OBC not available, so generate time string from local time
		time_t rawtime;
		struct tm* timeinfo;

		time(&rawtime);
		timeinfo = localtime(&rawtime);
		//output_line << "ODR";
		output_line << timeinfo->tm_year + 1900;
		output_line << setw(2) << setfill('0') << timeinfo->tm_mon;
		output_line << setw(2) << setfill('0') << timeinfo->tm_mday << "_";
		output_line << setw(2) << setfill('0') << timeinfo->tm_hour;
		output_line << setw(2) << setfill('0') << timeinfo->tm_min;
		output_line << setw(2) << setfill('0') << timeinfo->tm_sec << "_";
	}
	// Use Odroid process time to provide a unique value
	output_line << clock();
	return output_line.str();
}


string OBCData::getGPSPos()
{
	ostringstream output_line;
	output_line.flags(ios_base::fixed);
	output_line << setprecision(5) << lat << ", " << lon << ", ";
	output_line << setprecision(2) << alt;
	return output_line.str();
}


string OBCData::getIMU()
{
	ostringstream output_line;
	output_line.flags(ios_base::fixed);
	output_line << setprecision(2);
	output_line << ax << ", " << ay << ", " << az << ", ";
	output_line << gx << ", " << gy << ", " << gz << ", ";
	output_line << mx << " ," << my << ", " << mz;
	return output_line.str();
}
