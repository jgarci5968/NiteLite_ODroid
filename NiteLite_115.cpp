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

// Pylon API header files
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>

// System namespace
using namespace std;

// Pylon and Basler namespaces
using namespace Pylon;
using namespace GenApi;
using namespace Basler_UsbCameraParams;


string filepath = "/home/odroid/Pictures/";
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


// Create a filename from the image capture parameters.
gcstring create_filename(string timestr, int exposure, int seq, string ext)
{
	ostringstream filename;
	filename << filepath << timestr << "_" << exposure << "_" << seq << "_" << CameraID << ext;
	return gcstring(filename.str().c_str());
}


// Capture a sequence of images with a given exposure
void take_exposures(CBaslerUsbInstantCamera &Camera, int exposure_time, int n)
{
	// This smart pointer will receive the grab result data.
	CGrabResultPtr ptrGrabResult;

	double internal_temp = Camera.DeviceTemperature.GetValue();

	for(int idx = 1; idx <= n; idx++)
	{
		string timestr = get_time_string();
		gcstring gc_filename_raw = create_filename(timestr, exposure_time, idx, ".raw");
		gcstring gc_filename_tiff = create_filename(timestr, exposure_time, idx, ".tiff");

		Camera.ExposureTime.SetValue(exposure_time * 1000); // in microseconds

		if ( Camera.GrabOne( 1000, ptrGrabResult))
		{
			// The pylon grab result smart pointer classes provide a cast operator to the IImage
			// interface. This makes it possible to pass a grab result directly to the
			// function that saves an image to disk.
			CImagePersistence::Save(ImageFileFormat_Tiff, gc_filename_tiff, ptrGrabResult);
			CImagePersistence::Save(ImageFileFormat_Raw, gc_filename_raw, ptrGrabResult);
			cout << timestr << ", " << exposure_time << ", " << idx << ", " << CameraID << ", t=" << internal_temp << endl;
		}
	}
}


int main(int argc, char* argv[])
{
	if ( argc == 1 )
	{
		// No command line arguments, display usage
		cerr << "usage: " << argv[0] << " camera-id [path]" << endl;
		exit(-1);
	}
	if ( argc > 1 )
	{
		// Get CameraID from first command line argument (required)
		CameraID = atoi(argv[1]);
	}
	if ( argc > 2 )
	{
		// Get image directory path from second command line argument (optional)
		FILE* fp = fopen(argv[2], "r");
		if ( fp == NULL )
		{
			perror(argv[2]);
			exit(-1);
		}
		else
			fclose(fp);
		filepath = argv[2];
	}


	string timestr = get_time_string();
	cerr << timestr << " filepath: " << filepath << ", CameraID=" << CameraID << endl;

	// Initialize Pylon runtime before using any Pylon methods 
	PylonInitialize();

	try
	{
		// Configure camera resources
		CBaslerUsbInstantCamera Camera(CTlFactory::GetInstance().CreateFirstDevice());
		Camera.Open();
		Camera.PixelFormat.SetValue(PixelFormat_BayerRG12);

		cerr << timestr << " grabbing images" << endl;

		// Capture images at different exposures
		take_exposures(Camera, 50, 5);
		take_exposures(Camera, 100, 1);
		take_exposures(Camera, 250, 1);
		take_exposures(Camera, 500, 1);

		Camera.Close();
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
