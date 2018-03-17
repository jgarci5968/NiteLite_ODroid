//	NiteLite_115.cpp
//	This program performs one image capture cycle using the Basler USB camera.
//	An image capture cycle consists of a sequence of images taken with varying
//	exposures. Both raw and TIFF versions of the images are saved to a specified
//	directory and named based on the time the image was taken, the exposure, the
//	image number in the sequence, and the camera ID. Metadata for each image is
//	also logged to the standard output.

// System includes
#include <sstream>
#include <iostream>
#include <ctime>
#include <cstdio>
#include <unistd.h>

// Pylon API header files
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>

// System namespace
using namespace std;

// Pylon and Basler namespaces
using namespace Pylon;
using namespace GenApi;
using namespace Basler_UsbCameraParams;


string image_dir = "/home/odroid/Pictures/";
int CameraID = 0;


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


// Enumerate the connected cameras and initialize each one.
int InitializeCameras(CBaslerUsbInstantCamera* cameras[], string timestr)
{
	CTlFactory& TlFactory = CTlFactory::GetInstance();
	DeviceInfoList_t lstDevices;
	int i = 0;

	// Find and configure camera resources
	TlFactory.EnumerateDevices(lstDevices);
	if ( !lstDevices.empty() )
	{
		DeviceInfoList_t::const_iterator it;
		it = lstDevices.begin();
		for ( it = lstDevices.begin(); it != lstDevices.end(); ++it, ++i )
		{
			cameras[i] = new CBaslerUsbInstantCamera(TlFactory.CreateDevice(*it));
			cameras[i]->Open();
			cameras[i]->PixelFormat.SetValue(PixelFormat_BayerRG12);

			cerr << timestr << " Camera " << cameras[i]->GetDeviceInfo().GetFullName();
			cerr << " sn: " << cameras[i]->GetDeviceInfo().GetSerialNumber();
			cerr << " configured" << endl;
		}
	}
	return i;
}


// Create a filename from the image capture parameters.
gcstring create_filename(string timestr, int exposure, string sn, EImageFileFormat format)
{
	ostringstream filename;
	filename << image_dir << timestr << "_" << exposure << "_" << sn << (( format == ImageFileFormat_Tiff )? ".tiff" : ".raw");
	return gcstring(filename.str().c_str());
}


// Capture an image from each camera in the camera array
void take_exposures(CBaslerUsbInstantCamera* cameras[], int n, int exposure_time, EImageFileFormat format)
{
	CGrabResultPtr ptrGrabResult;
	string timestr = get_time_string();

	for(int idx = 0; idx < n; idx++)
	{
		string serial_number = cameras[idx]->GetDeviceInfo().GetSerialNumber().c_str();
		double internal_temp = cameras[idx]->DeviceTemperature.GetValue();
		cameras[idx]->ExposureTime.SetValue(exposure_time * 1000); // in microseconds

		if ( cameras[idx]->GrabOne(1000, ptrGrabResult) )
		{
			// The pylon grab result smart pointer classes provide a cast operator to the IImage
			// interface. This makes it possible to pass a grab result directly to the
			// function that saves an image to disk.
			gcstring gc_filename = create_filename(timestr, exposure_time, serial_number, format);
			CImagePersistence::Save(format, gc_filename, ptrGrabResult);
			cout << timestr << ", " << idx << ", " << exposure_time << ", t=" << internal_temp << ", " << gc_filename << endl;
		}
		else
			cout << timestr << ", " << idx << ", " << exposure_time << ", t=" << internal_temp << ", grab failed" << endl;
	}
}


int main(int argc, char* argv[])
{
	if ( argc > 1 )
	{
		if ( string("-h") == argv[1] )
		{
			// Display usage
			cerr << "usage: " << argv[0] << " [directory path]" << endl;
			exit(-1);
		}
	
		// Get image directory path from second command line argument (optional)
		FILE* fp = fopen(argv[2], "r");
		if ( fp == NULL )
		{
			perror(argv[1]);
			exit(-1);
		}
		else
			fclose(fp);
		image_dir = argv[2];
	}


	string timestr = get_time_string();
	cerr << timestr << " Image directory path: " << image_dir << endl;

	// Initialize Pylon runtime before using any Pylon methods 
	PylonInitialize();

	try
	{
		CBaslerUsbInstantCamera* cameras[3];
		int n = InitializeCameras(cameras, timestr);
		cerr << timestr << " Found " << n << " cameras" << endl;

		for ( int count = 0; count < 10; count++ )
		{
			take_exposures(cameras, n, 50, ImageFileFormat_Raw);
			take_exposures(cameras, n, 50, ImageFileFormat_Raw);
			take_exposures(cameras, n, 50, ImageFileFormat_Raw);
			take_exposures(cameras, n, 50, ImageFileFormat_Raw);
			take_exposures(cameras, n, 50, ImageFileFormat_Raw);
			take_exposures(cameras, n, 100, ImageFileFormat_Tiff);
			sleep(1);
		}

		for ( int i = 0; i < n; i++ )
			cameras[i]->Close();
	}
	catch (const GenericException &e)
	{
		// Error handling.
		cerr << timestr << " An exception occurred: " << e.what() << endl;
		exit(-1);
	}

	// Releases all Pylon resources.
	PylonTerminate();

	exit(0);
}
