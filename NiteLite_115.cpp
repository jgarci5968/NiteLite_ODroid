// Utility_ImageLoadAndSave.cpp
/*
    Note: Before getting started, Basler recommends reading the Programmer's Guide topic
    in the pylon C++ API documentation that gets installed with pylon.
//    If you are upgrading to a higher major version of pylon, Basler also
//    strongly recommends reading the Migration topic in the pylon C++ API documentation.
//
//    This sample illustrates how to load and save images.
//
//    The CImagePersistence class provides static functions for
//    loading and saving images. It uses the image
//    class related interfaces IImage and IReusableImage of pylon.
//
//    IImage can be used to access image properties and image buffer.
//    Therefore, it is used when saving images. In addition to that images can also
//    be saved by passing an image buffer and the corresponding properties.
//
//    The IReusableImage interface extends the IImage interface to be able to reuse
//    the resources of the image to represent a different image. The IReusableImage
//    interface is used when loading images.
//
//    The CPylonImage and CPylonBitmapImage image classes implement the
//    IReusableImage interface. These classes can therefore be used as targets
//    for loading images.
//
//    The gab result smart pointer classes provide a cast operator to the IImage
//    interface. This makes it possible to pass a grab result directly to the
//    function that saves images to disk.
//*/
//
//// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <sstream> // needed for filename 

#include <iostream> // added for time stuff`
#include <ctime>    // ditto!

// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;
using namespace Basler_UsbCameraParams;

// Namespace for using cout.
using namespace std;

const string filepath = string("/home/odroid/Pictures/");

 
void take_exposures(CBaslerUsbInstantCamera &Camera, int t, int n, int exposure_time) {
	// This smart pointer will receive the grab result data.
	CGrabResultPtr ptrGrabResult;

	double internal_temp = Camera.DeviceTemperature.GetValue();

	cout << "n = " << n << ", exposure = " << exposure_time << ", internal_temp = " << internal_temp << "\n";
	for(int idx = 1; idx <= n; idx++)
	{ 
		// stolen from github.com/ellenschallig/internship/GrabImage.cpp
		ostringstream base_filename;
		base_filename << "image_" << t << "_" << exposure_time << "_" << idx;
		cout << base_filename.str() << " ";

		ostringstream filename1;
		filename1 << filepath << base_filename.str() << ".raw";
		gcstring filename_raw = filename1.str().c_str();
		
		ostringstream filename2;
		filename2 << filepath << base_filename.str() <<  ".tiff";
		gcstring filename_tiff = filename2.str().c_str();
		// End of theft

		Camera.ExposureTime.SetValue(exposure_time); // in microseconds 
	       
		if ( Camera.GrabOne( 1000, ptrGrabResult))
		{
			// The pylon grab result smart pointer classes provide a cast operator to the IImage
			// interface. This makes it possible to pass a grab result directly to the
			// function that saves an image to disk.
			CImagePersistence::Save(ImageFileFormat_Tiff, filename_tiff, ptrGrabResult);
			CImagePersistence::Save(ImageFileFormat_Raw, filename_raw, ptrGrabResult);

			//cout << ".\n";
		}

	} // End of idx for loop
	cout << endl;
}


//int main(int argc, char* argv[]) // This was included in the original file from Basler
int main()
{


   // Swipped from  https://stackoverflow.com/questions/16357999/current-date-and-time-as-string
   // This all looks like a good candiate for a function. 
   time_t rawtime;
   struct tm * timeinfo;
   char buffer[80];

   time (&rawtime);
   timeinfo = localtime(&rawtime);

   strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
   std::string image_time(buffer);

   std::cout << image_time << " Waiting for an image to be grabbed." << endl;
   // end of theft 

   clock_t t; 
   t = clock();   		

   // The exit code of the sample application.
   int exitCode = 0;

   // Before using any pylon methods, the pylon runtime must be initialized. 
   PylonInitialize();

   int exposure_time=0;
   try
   {
           // Try to get a grab result.
           try
           {
		CBaslerUsbInstantCamera Camera( CTlFactory::GetInstance().CreateFirstDevice());

		Camera.Open();
		Camera.PixelFormat.SetValue(PixelFormat_BayerRG12);

		take_exposures(Camera, t, 5, 50000);
		take_exposures(Camera, t, 1, 100000);
		take_exposures(Camera, t, 1, 250000);
		take_exposures(Camera, t, 1, 500000);

		Camera.Close();
            }
            catch (const GenericException &e)
            {

                cerr << "Could not grab an image: " << endl
                    << e.GetDescription() << endl;
            }
    }
    catch (const GenericException &e)
    {
        // Error handling.
        cerr << "An exception occurred." << endl
        << e.GetDescription() << endl;
        exitCode = 1;
    }

    // Comment the following two lines to disable waiting on exit.
//    cerr << endl << "Press Enter to exit." << endl;
//    while( cin.get() != '\n');

    // Releases all pylon resources. 
    PylonTerminate(); 

    return exitCode;
}
