//	baslerctrl.cpp
//	This program performs one image capture cycle using the Basler USB camera.
//	An image capture cycle consists of a sequence of images taken with varying
//	exposures. Both raw and TIFF versions of the images are saved to a specified
//	directory and named based on the time the image was taken, the exposure, the
//	image number in the sequence, and the camera ID. Metadata for each image is
//	also logged to the standard output.

// System includes
#include <iostream>
#include <sstream>
#include <system_error>
#include <thread>
#include <mutex>
#include <cstdio>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

// Pylon API header files
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>

// System namespace
using namespace std;

// Pylon and Basler namespaces
using namespace Pylon;
using namespace GenApi;
using namespace Basler_UsbCameraParams;


string dev_path = "/dev/ttyACM0";
string image_dir = "/home/odroid/Pictures/";
string camera_dir[3];
int CameraID = 0;


// Shared buffer for passing data between read_usb thread and main thread
struct SharedData
{
	mutex m;
	string obc_data;
} shared_buffer;


// Create a formatted string from the current system time
string get_time_string()
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	char buffer[80];
	strftime(buffer, sizeof(buffer), "%y%m%d_%H%M%S", timeinfo);
	return string(buffer);
}


// Initialize USB device and continuously read data
// Shared variable is updated with received data
void read_usb()
{
	// Attempt to open USB device repeatedly until successful
	FILE* fp;
	while ( (fp = fopen(dev_path.c_str(), "r")) == NULL )
	{
		if ( errno != EBUSY && errno != ENOENT )
		{
			system_error e {errno, system_category(), dev_path};
			cerr << e.what() << endl;
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
	

	if ( fp == NULL )
	{
		system_error e {errno, system_category(), dev_path};
		cerr << e.what() << endl;
	}
	else
	{
		int c;
		string inputLine;

		do {
			c = fgetc(fp);
			inputLine += (char) c;
			if ( c == '\n' )
			{
				shared_buffer.m.lock();
				shared_buffer.obc_data = inputLine;
				shared_buffer.m.unlock();
				inputLine = "";
			}
		} while ( c != EOF );
		shared_buffer.m.lock();
		shared_buffer.obc_data = inputLine;
		shared_buffer.m.unlock();
		fclose(fp);
	}
}


// Enumerate the connected cameras and initialize each one.
// Return value: number of cameras initialized
int initialize_cameras(CBaslerUsbInstantCameraArray &cameras, string timestr)
{
	CTlFactory& tlFactory = CTlFactory::GetInstance();
	DeviceInfoList_t lstDevices;
	int i = 0;

	// Find and configure camera resources
	if ( tlFactory.EnumerateDevices(lstDevices) > 0 )
	{
		cerr << timestr << " Found " << lstDevices.size() << " camera" << ((lstDevices.size() > 1)? "s" : "") << endl;
		cameras.Initialize(lstDevices.size());

		DeviceInfoList_t::const_iterator it;
		it = lstDevices.begin();
		for ( it = lstDevices.begin(); it != lstDevices.end(); ++it, ++i )
		{
			cameras[i].Attach(tlFactory.CreateDevice(*it));
			cameras[i].Open();
			cameras[i].PixelFormat.SetValue(PixelFormat_BayerRG12);

			cerr << timestr << " Camera " << cameras[i].GetDeviceInfo().GetFullName();
			cerr << " sn: " << cameras[i].GetDeviceInfo().GetSerialNumber();
			cerr << " configured" << endl;
		}
	}
	return i;
}


// Check for existence of image directories and create if not there.
// This uses global variable "image_dir".
// Return value: 0 - directory initialization succeeded
//		-1 - directory initialization failed
int initialize_image_dirs(CBaslerUsbInstantCameraArray &cameras, string timestr)
{
	if ( image_dir.at(image_dir.length() - 1) != '/' )
		image_dir += '/';
	
	// Check for top level image dir
	FILE* fp = fopen(image_dir.c_str(), "r");
	if ( fp == NULL )
	{
		throw system_error{errno, system_category(), timestr + " Failed to open image directory " + image_dir};
	}
	else
	{
		cerr << timestr << " Image directory exists: " << image_dir << endl;
		fclose(fp);
	}

	// Check for camera directories
	for ( int i = 0; i < cameras.GetSize(); i++ )
	{
		ostringstream dirpath;
		dirpath << image_dir << cameras[i].GetDeviceInfo().GetSerialNumber() << "/";
		camera_dir[i] = dirpath.str();

		fp = fopen(camera_dir[i].c_str(), "r");
		if ( fp == NULL )
		{
			// Camera directory doesn't exist, so create it
			if ( mkdir(camera_dir[i].c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) < 0 )
			{
				throw system_error{errno, system_category(),
					timestr + " Failed to create camera directory " + camera_dir[i]};
			}
			else
			{
				cerr << timestr << " Created camera directory: " << camera_dir[i] << endl;
			}
		}
		else
		{
			cerr << timestr << " Camera directory exists: " << camera_dir[i] << endl;
			fclose(fp);
		}
	}
}


// Release camera resources
void terminate_cameras(CBaslerUsbInstantCameraArray &cameras, string timestr)
{
	for ( int i = 0; i < cameras.GetSize(); i++ )
		cameras[i].Close();
	cerr << timestr << " Terminated cameras" << endl;
}


// Create a filename from the image capture parameters.
gcstring create_filename(string timestr, int cameraID, int exposure, string sn, int seq, EImageFileFormat format)
{
	ostringstream filename;
	filename << camera_dir[cameraID] << timestr << "_" << exposure << "_" << sn << "_" << seq;
	filename << (( format == ImageFileFormat_Tiff )? ".tiff" : ".raw");
	return gcstring(filename.str().c_str());
}


// Capture an image from each camera in the camera array
void take_exposures(CBaslerUsbInstantCameraArray &cameras, int exposure_time, int seq, EImageFileFormat format)
{
	CGrabResultPtr ptrGrabResult;
	string timestr = get_time_string();

	for(int idx = 0; idx < cameras.GetSize(); idx++)
	{
		string serial_number = cameras[idx].GetDeviceInfo().GetSerialNumber().c_str();
		double internal_temp = cameras[idx].DeviceTemperature.GetValue();
		cameras[idx].ExposureTime.SetValue(exposure_time * 1000); // in microseconds

		if ( cameras[idx].GrabOne(1000, ptrGrabResult) )
		{
			// The pylon grab result smart pointer classes provide a cast operator to the IImage
			// interface. This makes it possible to pass a grab result directly to the
			// function that saves an image to disk.
			gcstring gc_filename = create_filename(timestr, idx, exposure_time, serial_number, seq, format);
			CImagePersistence::Save(format, gc_filename, ptrGrabResult);
			cout << timestr << ", " << idx << ", " << exposure_time << ", " << seq;
			cout << ", t=" << internal_temp << ", " << gc_filename << endl;
		}
		else
			cout << timestr << ", " << idx << ", " << exposure_time << ", " << seq;
			cout << ", t=" << internal_temp << ", grab failed" << endl;
	}
}


// Take exposures for one imaging cycle consisting of 5 raw images at 50ms and one TIFF image at 100ms.
void imaging_cycle(CBaslerUsbInstantCameraArray &cameras)
{
	take_exposures(cameras, 50, 0, ImageFileFormat_Raw);
	take_exposures(cameras, 50, 1, ImageFileFormat_Raw);
	take_exposures(cameras, 50, 2, ImageFileFormat_Raw);
	take_exposures(cameras, 50, 3, ImageFileFormat_Raw);
	take_exposures(cameras, 50, 4, ImageFileFormat_Raw);
	take_exposures(cameras, 100, 0, ImageFileFormat_Tiff);
}


void usage(char* argv[])
{
	cout << "usage: " << argv[0] << " [directory path] [device path]" << endl;
	exit(-1);
}


int main(int argc, char* argv[])
{
	// Check command line parameters
	if ( argc > 2 )
	{
		// Get directory path and USB device path parameters
		if ( string("-h") == argv[1] )
			usage(argv);
		else
		{
			image_dir = argv[1];
			dev_path = argv[2];
		}
	}
	else if ( argc > 1 )
	{
		if ( string("-h") == argv[1] )
			usage(argv);
		else
			image_dir = argv[1];
	}


	string timestr = get_time_string();
	cerr << timestr << " Image directory path: " << image_dir << ", USB device path: " << dev_path << endl;

	thread t1 {read_usb};

	// Initialize Pylon runtime before using any Pylon methods 
	PylonInitialize();

	try
	{
		CBaslerUsbInstantCameraArray cameras;
		int n = initialize_cameras(cameras, timestr);
		initialize_image_dirs(cameras, timestr);

		//for ( int count = 0; count < 1; count++ )
		while ( true )
		{
			//imaging_cycle(cameras);
			shared_buffer.m.lock();
			string data = shared_buffer.obc_data;
			shared_buffer.m.unlock();
			if ( data != "" )
				cout << data;
			else
				cout << "no data" << endl;
			sleep(2);
		}

		terminate_cameras(cameras, get_time_string());
	}
	catch (const GenericException &e)
	{
		// Pylon error handling.
		cerr << timestr << " An exception occurred: " << e.what() << endl;
		exit(-1);
	}
	catch (const system_error &e)
	{
		// System error handling.
		cerr << e.what() << endl;
		exit(-1);
	}


	// Releases all Pylon resources.
	PylonTerminate();

	exit(0);
}
