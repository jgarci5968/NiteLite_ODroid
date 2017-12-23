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
//#include "../include/SampleImageCreator.h"
//#include <GenApi/IEnumerationT.h> // Jeff Junk
//#include <pylon/PixelType.h>      //More Jeff Junk
#include <pylon/usb/BaslerUsbInstantCamera.h>

// Namespace for using pylon objects.
using namespace Pylon;
using namespace GenApi;

// Namespace for using cout.
using namespace std;

int main(int argc, char* argv[])
{
    // The exit code of the sample application.
    int exitCode = 0;

    // Before using any pylon methods, the pylon runtime must be initialized. 
    PylonInitialize();

    try
    {
        // Saving grabbed images.
        {
            // Try to get a grab result.
            cout << endl << "Waiting for an image to be grabbed." << endl;
            try
            {
                // This smart pointer will receive the grab result data.
                CGrabResultPtr ptrGrabResult;
                CBaslerUsbInstantCamera Camera( CTlFactory::GetInstance().CreateFirstDevice());
                //Camera.Open();
                //INodeMap &control = Camera.GetNodeMap();
                //CEnumerationPtr(control.GetNode("PixelFormat"))->FromString("BayerRG12");
                Camera.PixelFormat.SetValue(PixelFormat_BayerRG12);
                if ( Camera.GrabOne( 1000, ptrGrabResult))
                {
                    // The pylon grab result smart pointer classes provide a cast operator to the IImage
                    // interface. This makes it possible to pass a grab result directly to the
                    // function that saves an image to disk.
                    CImagePersistence::Save( ImageFileFormat_Tiff, "GrabbedImage.tiff", ptrGrabResult);
                }
                Camera.Close();
            }
            catch (const GenericException &e)
            {

                cerr << "Could not grab an image: " << endl
                    << e.GetDescription() << endl;
            }
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
    cerr << endl << "Press Enter to exit." << endl;
    while( cin.get() != '\n');

    // Releases all pylon resources. 
    PylonTerminate(); 

    return exitCode;
}
