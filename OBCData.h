//	OBCData.h
//	Interface for OBCData class.  This class encapsulates OBC data and parsing of the data
//	stream read from a USB port.

#ifndef _OBCData_H_
#define _OBCData_H_

using namespace std;

const int MAX_FIELDS=19;


// OBCData class definition
class OBCData
{
public:
	bool obc_mode;	// Flag indicating whether or not OBC is available.
	string input;	// Input string as read
	long ms;	// OBC internal time in milliseconds
	int yy;		// Two digit year from GPS
	int mm;		// Two digit month from GPS
	int dd;		// Two digit date from GPS
	int hh;		// GPS hours
	int min;	// GPS minutes
	int ss;		// GPS seconds
	double lat;	// GPS latitude
	double lon;	// GPS longitude
	double alt;	// GPS altitut
	double ax;	// IMU acceleration along the x-axis
	double ay;	// IMU acceleration along the y-axis
	double az;	// IMU acceleration along the z-axis
	double gx;	// IMU gyroscope in the x-axis
	double gy;	// IMU gyroscope in the y-axis
	double gz;	// IMU gyroscope in the z-axis
	double mx;	// IMU magnetometer in the x-axis
	double my;	// IMU magnetometer in the y-axis
	double mz;	// IMU magnetometer in the z-axis

	void parseField(string field, int pos);
	string display();
	string getTimeString();
	string getGPSPos();
	string getIMU();
};


// Shared data buffer
struct SharedData
{
	mutex m;
	OBCData obc_data;
	bool available;		// Flag indicating data is available in shared data buffer
};

extern SharedData shared_data;

extern string get_time_string();
extern FILE* init_usb(string dev_path, string timestr);
extern void read_usb(FILE* fp);

#endif
