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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

// Pylon API header files
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <pylon/usb/BaslerUsbInstantCameraArray.h>

// Local include
#include "OBCData.h"

// System namespace
using namespace std;

// Pylon and Basler namespaces
using namespace Pylon;
using namespace GenApi;
using namespace Basler_UsbCameraParams;


// GLOBAL VARIABLES
string dev_path = "/dev/ttyACM0";
//string image_dir = "/media/odroid/NITELITE2/FlightImages/";
string image_dir = "/home/odroid/Pictures";
string camera_dir[3];


// Create a formatted string from the current system time
//string get_time_string()
//{
//	time_t rawtime;
//	struct tm* timeinfo;
//
//	time(&rawtime);
//	timeinfo = localtime(&rawtime);
//
//	char buffer[80];
//	strftime(buffer, sizeof(buffer), "%b %d %X", timeinfo);
//	return string(buffer);
//}


// Enumerate the connected cameras and initialize each one.
// Return value: number of cameras initialized
int initialize_cameras(CBaslerUsbInstantCameraArray &cameras)
{
	CTlFactory& tlFactory = CTlFactory::GetInstance();
	DeviceInfoList_t lstDevices;
	int i = 0;

	// Find and configure camera resources
	if ( tlFactory.EnumerateDevices(lstDevices) > 0 )
	{
		cerr << get_time_string() << " Found " << lstDevices.size() << " camera" << ((lstDevices.size() > 1)? "s" : "") << endl;
		cameras.Initialize(lstDevices.size());

		DeviceInfoList_t::const_iterator it;
		it = lstDevices.begin();
		for ( it = lstDevices.begin(); it != lstDevices.end(); ++it, ++i )
		{
			try
			{
				cameras[i].Attach(tlFactory.CreateDevice(*it));
				cerr << get_time_string() << " Attached camera: " << i << endl;
				cameras[i].Open();
				cerr << get_time_string() << " Opened camera: " << i << endl;

				// Set camera parameters
				cameras[i].PixelFormat.SetValue(PixelFormat_BayerRG12);
				cameras[i].DeviceLinkThroughputLimitMode.SetValue(DeviceLinkThroughputLimitMode_On);
				int64_t DeviceLink_MinimumValue = cameras[i].DeviceLinkThroughputLimit.GetMin();
				cameras[i].DeviceLinkThroughputLimit.SetValue(DeviceLink_MinimumValue);
				int64_t MaxTransferSize_MaxValue = cameras[i].GetStreamGrabberParams().MaxTransferSize.GetMax();
				cameras[i].GetStreamGrabberParams().MaxTransferSize.SetValue(MaxTransferSize_MaxValue);

				cerr << get_time_string() << " Camera " << cameras[i].GetDeviceInfo().GetFullName();
				cerr << " sn: " << cameras[i].GetDeviceInfo().GetSerialNumber();
				cerr << " configured" << endl;
			}
			catch (const GenericException &e)
			{
				cerr << get_time_string() << " Exception in initialize_cameras(), camera " << i << ": " << e.what() << endl;
			}
		}
	}
	else
		cerr << get_time_string() << " No cameras detected" << endl;

	return i;
}


// Check for existence of top level image_dir and create if not there.
int check_image_dir()
{
	// Make sure image_dir contains a trailing '/'
	if ( image_dir.at(image_dir.length() - 1) != '/' )
		image_dir += '/';

	// Test for existence of image_dir
	FILE* fp = fopen(image_dir.c_str(), "r");
	if ( fp == NULL )
	{
		throw system_error{errno, system_category(), get_time_string() + " Failed to open image directory " + image_dir};
	}
	else
	{
		fclose(fp);
	}
}


// Check for the existence of the camera image directories and create if not there.
// This uses global variable "image_dir".
void initialize_image_dirs(CBaslerUsbInstantCameraArray &cameras)
{
	// Check for camera directories
	for ( int i = 0; i < cameras.GetSize(); i++ )
	{
		String_t sn = cameras[i].GetDeviceInfo().GetSerialNumber();
		if ( String_t("N/A") == sn )
		{
			cerr << get_time_string() << " initialize_image_dirs(): Camera " << i << " not accessible" << endl;
			continue;
		}

		ostringstream dirpath;
		//dirpath << image_dir << cameras[i].GetDeviceInfo().GetSerialNumber() << "/";
		dirpath << image_dir << sn << "/";
		camera_dir[i] = dirpath.str();

		FILE* fp = fopen(camera_dir[i].c_str(), "r");
		if ( fp == NULL )
		{
			// Camera directory doesn't exist, so create it
			if ( mkdir(camera_dir[i].c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) < 0 )
			{
				throw system_error{errno, system_category(),
					get_time_string() + " Failed to create camera directory " + camera_dir[i]};
			}
			else
			{
				cerr << get_time_string() << " Created camera directory: " << camera_dir[i] << endl;
			}
		}
		else
		{
			cerr << get_time_string() << " Camera directory exists: " << camera_dir[i] << endl;
			fclose(fp);
		}
	}
}


// Release camera resources
void terminate_cameras(CBaslerUsbInstantCameraArray &cameras)
{
	for ( int i = 0; i < cameras.GetSize(); i++ )
		cameras[i].Close();

	cerr << get_time_string() << " Cameras terminated" << endl;
}


// Create a filename from the image capture parameters.
gcstring create_filename(string timestr, int cameraID, int exposure, string sn, int seq, EImageFileFormat format)
{
	ostringstream filename;
	filename << camera_dir[cameraID] << timestr << "_" << cameraID << "_" << exposure << "_" << seq;
	filename << (( format == ImageFileFormat_Tiff )? ".tiff" : ".raw");
	return gcstring(filename.str().c_str());
}


// Capture an image from each camera in the camera array
void take_exposures(CBaslerUsbInstantCamera &camera, int exposure_time, int stacks, int cameraNum, EImageFileFormat format)
{
	CGrabResultPtr ptrGrabResult;

	for(int idx = 0; idx < stacks; idx++)
	{
		string serial_number("");
		double internal_temp = 0;

		// Get most recent OBC data from the shared buffer
		OBCData data;
		shared_data.m.lock();
		data = shared_data.obc_data;
		shared_data.m.unlock();
		
		// Strings for OBC time and Odroid time
		string obc_time = data.getTimeString();
		string odroid_time = get_time_string();

		try
		{
			serial_number = camera.GetDeviceInfo().GetSerialNumber().c_str();
			internal_temp = camera.DeviceTemperature.GetValue();
			camera.ExposureTime.SetValue(exposure_time * 1000); // in microseconds

			if ( camera.GrabOne(1000, ptrGrabResult) && ptrGrabResult->GrabSucceeded() )
			{
				gcstring gc_filename = create_filename(obc_time, cameraNum, exposure_time, serial_number, idx, format);
				CImagePersistence::Save(format, gc_filename, ptrGrabResult);
				cout << odroid_time << ", " << obc_time << ", " << cameraNum << ", " << serial_number << ", " << exposure_time << ", " << idx;
				cout << ", " << internal_temp << ", " << gc_filename;
				cout << ", " << data.getGPSPos() << ", " << data.getIMU() << endl;
			}
			else
			{
				// Handle grab failed error
				cout << odroid_time << ", " << obc_time << ", " << cameraNum << ", " << serial_number << ", " << exposure_time << ", " << idx;
				cout << ", " << internal_temp << ", grab failed: " << ptrGrabResult->GetErrorDescription() << endl;
				cerr << odroid_time << " grab failed: " << ptrGrabResult->GetErrorDescription() << endl;
			}
		}
		catch (const TimeoutException &te)
		{
			// Catch timeout exception thrown from GrabOne()
			cout << odroid_time << ", " << obc_time << ", " << cameraNum << ", " << serial_number << ", " << exposure_time << ", " << idx;
			cout << ", " << internal_temp << ", grab failed: TimeoutException" << te.what() << endl;
			cout.flush();
			cerr << odroid_time << " TimeoutException occurred in GrabOne(): " << te.what() << endl;
			cerr.flush();
		}
		catch (const GenericException &e)
		{
			// Pylon error handling.
			cout << odroid_time << ", " << obc_time << ", " << cameraNum << ", " << serial_number << ", " << exposure_time << ", " << idx;
			cout << ", " << internal_temp << ", grab failed: " << e.what() << endl;
			cout.flush();
			cerr << odroid_time << " An exception occurred in GrabOne() or CImagePersistence::Save(): " << e.what() << endl;
			cerr.flush();
		}

	}
}


// Take exposures for one imaging cycle consisting of 5 raw images at 50ms and one TIFF image at 100ms.
void imaging_cycle(CBaslerUsbInstantCameraArray &cameras)
{
	string timestring;

	for(int idx = 0; idx < cameras.GetSize(); idx++)
	{
		try
		{
			take_exposures(cameras[idx], 50, 5, idx, ImageFileFormat_Raw);
		}
		catch (const GenericException &e)
		{
			timestring = get_time_string();
			cerr << timestring << " An exception occurred in take_exposures(): " << e.what() << endl;
			if ( cameras[idx].IsCameraDeviceRemoved() )
			{
				cerr << timestring << " Camera " << idx << " removed" << endl;
			}
			cerr.flush();
		}

	}
	for(int idx = 0; idx < cameras.GetSize(); idx++)
	{
		try
		{
			take_exposures(cameras[idx], 100, 1, idx, ImageFileFormat_Tiff);
		}
		catch (const GenericException &e)
		{
			timestring = get_time_string();
			cerr << timestring << " An exception occurred in take_exposures(): " << e.what() << endl;
			if ( cameras[idx].IsCameraDeviceRemoved() )
			{
				cerr << timestring << " Camera " << idx << " removed" << endl;
			}
			cerr.flush();
		}
	}
}


void usage(char* argv[])
{
	cout << "Usage: " << argv[0] << " [OPTIONS] [directory path] [device path]" << endl;
	cout << "Options:" << endl;
	cout << "  -h    Display command line usage (this message)" << endl;
	cout << "  -d    Daemon mode (use for flight operations)" << endl;
	cout << "  -n    No OBC mode (use for ground testing without OBC)" << endl;
	cout << "  -w s  Wait for s seconds between imaging cycles (default is 5 seconds)" << endl;
	cout << "Defaults:" << endl;
	cout << "  [directory path] = " << image_dir << endl;
	cout << "  [device path] = " << dev_path << endl;
	exit(-1);
}


// Convert process to a daemon.
void daemonize()
{
	// Fork a child process
	pid_t pid = fork();
	if ( pid == -1 )
		throw system_error{errno, system_category(), " fork() failed"};
	else if ( pid != 0 )
		// Parent process exits
		exit(EXIT_SUCCESS);

	// Create new session and process group
	if ( setsid() == -1 )
		throw system_error{errno, system_category(), " setsid() failed"};

	// Change working directory to root directory.
	if ( chdir("/") == -1 )
		throw system_error{errno, system_category(), " chdir() failed"};

	// Close open file descriptors.
	for ( int i = 0; i < 3; i++ )
		close(i);

	// Any errors in opening the log files (e.g. image_dir not specified properly)
	// will not be logged because we just closed the stderr fd.

	// Redirect stdin to /dev/null
	open("/dev/null", O_RDWR);

	// Redirect stdout to image_dir/image.log
	if ( open((image_dir + "image.log").c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO) < 0 )
		throw system_error{errno, system_category(), image_dir + "image.log"};

	// Redirect stderr to image_dir/error.log
	if ( open((image_dir + "error.log").c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO) < 0 )
		throw system_error{errno, system_category(), image_dir + "error.log"};

	cerr << get_time_string() << " Entering daemon mode." << endl;
}


// This function forks a child process to operate the Basler cameras and then
// calls wait() to monitor the child. If the child process should exit, the
// termination status is written to the error log and a new child is forked.
// The parent process should never return from this function.
void monitor_child()
{
	while (true)
	{
		// Fork a child process
		pid_t pid = fork();
		if ( pid == -1 )
			throw system_error{errno, system_category(), " monitor_child(): fork() failed"};
		else if ( pid == 0 )
		{
			// Child process continues and operates Basler
			cerr << get_time_string() << " monitor_child(): child process forked" << endl;
			break;
		}

		// Wait for status of child
		int wstatus;
		pid = wait(&wstatus);
		if ( pid == -1 )
			throw system_error{errno, system_category(), " monitor_child(): wait() failed"};

		cerr << get_time_string() << " monitor_child(): child ";
		if ( WIFEXITED(wstatus) )
			cerr << "exited, status=" << WEXITSTATUS(wstatus) << endl;
		else if ( WIFSIGNALED(wstatus) )
			cerr << "killed by signal " << WTERMSIG(wstatus) << endl;
		else if ( WIFSTOPPED(wstatus) )
			cerr << "stopped by signal " << WSTOPSIG(wstatus) << endl;
		else if ( WIFCONTINUED(wstatus) )
			cerr << "continued" << endl;
	}

}


int main(int argc, char* argv[])
{
	// Check command line parameters
	bool id_set = false;
	bool daemon = false;
	int cycle_delay = 5;
	shared_data.obc_data.obc_mode = true;

	for ( int i = 1; i < argc; i++ )
	{
		if ( i == 1 && string("-h") == argv[i] )
			usage(argv);
		if ( string("-d") == argv[i] )
			daemon = true;
		else if ( string("-n") == argv[i] )
			shared_data.obc_data.obc_mode = false;
		else if ( string("-w") == argv[i] )
			cycle_delay = atoi(argv[++i]);
		else
		{
			if ( !id_set )
			{
				image_dir = argv[i];
				id_set = true;
			}
			else
			{
				dev_path = argv[i];
			}
		}
	}
	cerr << get_time_string() << " Daemon mode " << (daemon? "enabled" : "disabled");
	cerr << ", OBC mode ";
	if ( shared_data.obc_data.obc_mode )
		cerr << "enabled, timecodes are from OBC";
	else
		cerr << "disabled, timecodes are from Odroid";
	cerr << ", imaging cycle delay = " << cycle_delay << endl;

	try
	{
		check_image_dir();

		if ( daemon )
		{
			daemonize();
			monitor_child();
		}

		cerr << get_time_string() << " Image directory path: " << image_dir << ", USB device path: " << dev_path << endl;

		if ( shared_data.obc_data.obc_mode )
		{
			// Initialize USB device and start reading data
			cerr << get_time_string() << " Connecting to OBC" << endl;
			FILE* fp = init_usb(dev_path, get_time_string());
			thread t1 {read_usb, fp};
			t1.detach();
		}

		// Initialize Pylon runtime before using any Pylon methods 
		cerr << get_time_string() << " Initializing Pylon" << endl;
		PylonInitialize();

		// Initialize the cameras and create camera image directories
		CBaslerUsbInstantCameraArray cameras;
		int n = initialize_cameras(cameras);
		if ( n > 0 )
		{
			initialize_image_dirs(cameras);

			// Start the imaging cycle
			while ( true )
			{
				imaging_cycle(cameras);
				sleep(cycle_delay);
			}

			// Clean up
			terminate_cameras(cameras);
		}
	}
	catch (const GenericException &e)
	{
		// Pylon error handling.
		cerr << get_time_string() << " An exception occurred: " << e.what() << endl;
		exit(-1);
	}
	catch (const system_error &e)
	{
		// System error handling.
		cerr << e.what() << endl;
		exit(-1);
	}

	// Release all Pylon resources
	PylonTerminate();

	cerr << get_time_string() << " Program terminated normally" << endl;
	exit(0);
}
