//necessary libraries to include
#include "opencv2/opencv.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "cordef.h"
#include "GenApi/GenApi.h"		//!< GenApi lib definitions.
#include "gevapi.h"				//!< GEV lib definitions.
#include "SapX11Util.h"
#include "X_Display_utils.h"
#include "FileUtil.h"
#include <sched.h>
#include <vector>
#include <string>
using namespace cv;
using namespace std;

#define MAX_NETIF					8
#define MAX_CAMERAS_PER_NETIF	32
#define MAX_CAMERAS		(MAX_NETIF * MAX_CAMERAS_PER_NETIF)

// Enable/disable Bayer to RGB conversion
// (If disabled - Bayer format will be treated as Monochrome).
#define ENABLE_BAYER_CONVERSION 1

// Enable/disable buffer FULL/EMPTY handling (cycling)
#define USE_SYNCHRONOUS_BUFFER_CYCLING	0

#define WINDOW_NAME "Stream from Camera"

#define NUM_BUF	8
void *m_latestBuffer = NULL;

typedef struct tagMY_CONTEXT
{
	const char * 		View;
   	//X_VIEW_HANDLE     View;
	GEV_CAMERA_HANDLE camHandle;
	int					depth;
	int 					format;
	void 					*convertBuffer;
	BOOL					convertFormat;
	BOOL              exit;
}MY_CONTEXT, *PMY_CONTEXT;

char GetKey()
{
   char key = getchar();
   while ((key == '\r') || (key == '\n'))
   {
      key = getchar();
   }
   return key;
}

void PrintMenu()
{
   printf("GRAB CTL : [S]=stop,  [G]=continuous, [Q]or[ESC]=end\n");
}

void * ImageDisplayThread( void *context)
{
	MY_CONTEXT *displayContext = (MY_CONTEXT *)context;
	Mat src = imread("/home/jpcasas/Desktop/download.jpeg");

	if (displayContext != NULL)
	{
   		// While we are still running.
		while(!displayContext->exit)
		{
			GEV_BUFFER_OBJECT *img = NULL;
			GEV_STATUS status = 0;
	
			// Wait for images to be received
			status = GevWaitForNextImage(displayContext->camHandle, &img, 1000);

			if ((img != NULL) && (status == GEVLIB_OK))
			{
				if (img->status == 0)
				{
					m_latestBuffer = img->address;
					//Display_Image( displayContext->View, img->d,  img->w, img->h, img->address );
					Mat imgCv=Mat(img->h, img->w, CV_8UC1, m_latestBuffer);
					imshow (displayContext->View,imgCv);
					//imshow("Elephant",src);
					waitKey(10);
				}
				else
				{
					// Image had an error (incomplete (timeout/overflow/lost)).
					// Do any handling of this condition necessary.
				}
			}
#if USE_SYNCHRONOUS_BUFFER_CYCLING
			if (img != NULL)
			{
				// Release the buffer back to the image transfer process.
				GevReleaseImage( displayContext->camHandle, img);
			}
#endif
		}
	}
	pthread_exit(0);	
}


int main(int argc, char* argv[])
{
	GEV_DEVICE_INTERFACE  pCamera[MAX_CAMERAS] = {0};
	GEV_STATUS status;
	int numCamera = 0;
	int camIndex = 0;
   X_VIEW_HANDLE  View = NULL;
	MY_CONTEXT context = {0};
   pthread_t  tid;
	char c;
	int done = FALSE;

	
	//============================================================================
	// Greetings
	printf ("\nGigE Vision Library GenICam C++ Example Program (%s)\n", __DATE__);
	printf ("Copyright (c) 2015, DALSA.\nAll rights reserved.\n\n");

	//===================================================================================
	// Set default options for the library.
	{
		GEVLIB_CONFIG_OPTIONS options = {0};

		GevGetLibraryConfigOptions( &options);
		//options.logLevel = GEV_LOG_LEVEL_OFF;
		//options.logLevel = GEV_LOG_LEVEL_TRACE;
		options.logLevel = GEV_LOG_LEVEL_NORMAL;
		GevSetLibraryConfigOptions( &options);
	}

	//====================================================================================
	// DISCOVER Cameras
	//
	// Get all the IP addresses of attached network cards.

	status = GevGetCameraList( pCamera, MAX_CAMERAS, &numCamera);

	printf ("%d camera(s) on the network\n", numCamera);

	// Select the first camera found (unless the command line has a parameter = the camera index)
	if (numCamera != 0)
	{
		if (argc > 1)
		{
			sscanf(argv[1], "%d", &camIndex);
			if (camIndex >= (int)numCamera)
			{
				printf("Camera index out of range - only %d camera(s) are present\n", numCamera);
				camIndex = -1;
			}
		}

		if (camIndex != -1)
		{
			//====================================================================
			// Connect to Camera
			//
			//
			int i;
			int type;
			UINT32 height = 0;
			UINT32 width = 0;
			UINT32 format = 0;
			UINT32 maxHeight = 1600;
			UINT32 maxWidth = 2048;
			UINT32 maxDepth = 2;
			UINT64 size;
			UINT64 payload_size;
			int numBuffers = NUM_BUF;
			PUINT8 bufAddress[NUM_BUF];
			GEV_CAMERA_HANDLE handle = NULL;
			UINT32 pixFormat = 0;
			UINT32 pixDepth = 0;
			UINT32 convertedGevFormat = 0;
			
			//====================================================================
			// Open the camera.
			status = GevOpenCamera( &pCamera[camIndex], GevExclusiveMode, &handle);			
			
			// Go on to adjust some API related settings (for tuning / diagnostics / etc....).
			if ( status == 0 )
			{
				GEV_CAMERA_OPTIONS camOptions = {0};

				// Adjust the camera interface options if desired (see the manual)
				GevGetCameraInterfaceOptions( handle, &camOptions);
				//camOptions.heartbeat_timeout_ms = 60000;		// For debugging (delay camera timeout while in debugger)
				camOptions.heartbeat_timeout_ms = 5000;		// Disconnect detection (5 seconds)

				// Write the adjusted interface options back.
				GevSetCameraInterfaceOptions( handle, &camOptions);

				//=====================================================================
				// Get the GenICam FeatureNodeMap object and access the camera features.
				GenApi::CNodeMapRef *Camera = static_cast<GenApi::CNodeMapRef*>(GevGetFeatureNodeMap(handle));
				
				if (Camera)
				{
					// Access some features using the bare GenApi interface methods
					try 
					{
						//Mandatory features....
						GenApi::CIntegerPtr ptrIntNode = Camera->_GetNode("Width");
						width = (UINT32) ptrIntNode->GetValue();
						ptrIntNode = Camera->_GetNode("Height");
						height = (UINT32) ptrIntNode->GetValue();
						ptrIntNode = Camera->_GetNode("PayloadSize");
						payload_size = (UINT64) ptrIntNode->GetValue();
						GenApi::CEnumerationPtr ptrEnumNode = Camera->_GetNode("PixelFormat") ;
						format = (UINT32)ptrEnumNode->GetIntValue();
					}
					// Catch all possible exceptions from a node access.
					CATCH_GENAPI_ERROR(status);
				}

				if (status == 0)
				{
					//=================================================================
					// Set up a grab/transfer from this camera
					//
					printf("Camera ROI set for \n\tHeight = %d\n\tWidth = %d\n\tPixelFormat (val) = 0x%08x\n", height,width,format);

					maxHeight = height;
					maxWidth = width;
					maxDepth = GetPixelSizeInBytes(format);

					// Allocate image buffers
					// (Either the image size or the payload_size, whichever is larger - allows for packed pixel formats).
					size = maxDepth * maxWidth * maxHeight;
					size = (payload_size > size) ? payload_size : size;
					for (i = 0; i < numBuffers; i++)
					{
						bufAddress[i] = (PUINT8)malloc(size);
						memset(bufAddress[i], 0, size);

					}

#if USE_SYNCHRONOUS_BUFFER_CYCLING
					// Initialize a transfer with synchronous buffer handling.
					status = GevInitializeTransfer( handle, SynchronousNextEmpty, size, numBuffers, bufAddress);
#else
					// Initialize a transfer with asynchronous buffer handling.
					status = GevInitializeTransfer( handle, Asynchronous, size, numBuffers, bufAddress);
#endif

					// Create an image display window.
					// This works best for monochrome and RGB. The packed color formats (with Y, U, V, etc..) require 
					// conversion as do, if desired, Bayer formats.
					// (Packed pixels are unpacked internally unless passthru mode is enabled).

					// Translate the raw pixel format to one suitable for the (limited) Linux display routines.			

					status = GetX11DisplayablePixelFormat( ENABLE_BAYER_CONVERSION, format, &convertedGevFormat, &pixFormat);

					if (format != convertedGevFormat) 
					{
						// We MAY need to convert the data on the fly to display it.
						if (GevIsPixelTypeRGB(convertedGevFormat))
						{
							// Conversion to RGB888 required.
							pixDepth = 32;	// Assume 4 8bit components for color display (RGBA)
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;
							context.convertBuffer = malloc((maxWidth * maxHeight * ((pixDepth + 7)/8)));
							context.convertFormat = TRUE;
						}
						else
						{
							// Converted format is MONO - generally this is handled
							// internally (unpacking etc...) unless in passthru mode.
							// (						
							pixDepth = GevGetPixelDepthInBits(convertedGevFormat);
							context.format = Convert_SaperaFormat_To_X11( pixFormat);
							context.depth = pixDepth;							
							context.convertBuffer = NULL;
							context.convertFormat = FALSE;
						}
					}
					else
					{
						pixDepth = GevGetPixelDepthInBits(convertedGevFormat);
						context.format = Convert_SaperaFormat_To_X11( pixFormat);
						context.depth = pixDepth;
						context.convertBuffer = NULL;
						context.convertFormat = FALSE;
					}
					//namedWindow(WINDOW_NAME,WINDOW_AUTOSIZE);
					//View = CreateDisplayWindow("GigE-V GenApi Console Demo", TRUE, height, width, pixDepth, pixFormat, FALSE ); 

					// Create a thread to receive images from the API and display them.
					//context.View = View;
					context.View = WINDOW_NAME;
					context.camHandle = handle;
					context.exit = FALSE;
		   		pthread_create(&tid, NULL, ImageDisplayThread, &context); 

		         // Call the main command loop or the example.
		         PrintMenu();
		         while(!done)
		         {
		            c = GetKey();

		            // Stop
		            if ((c == 'S') || (c=='s') || (c == '0'))
		            {
							GevStopTransfer(handle);
		            }

		            // Continuous grab.
		            if ((c == 'G') || (c=='g'))
		            {
							for (i = 0; i < numBuffers; i++)
							{
								memset(bufAddress[i], 0, size);
							}
	 						status = GevStartTransfer( handle, -1);
							if (status != 0) printf("Error starting grab - 0x%x  or %d\n", status, status); 
		            }
					
		            

		            if ((c == 0x1b) || (c == 'q') || (c == 'Q'))
		            {
							GevStopTransfer(handle);
		               done = TRUE;
							context.exit = TRUE;
		   				pthread_join( tid, NULL);      
		            }
		         }

					GevAbortTransfer(handle);
					status = GevFreeTransfer(handle);
					DestroyDisplayWindow(View);


					for (i = 0; i < numBuffers; i++)
					{	
						free(bufAddress[i]);
					}
					if (context.convertBuffer != NULL)
					{
						free(context.convertBuffer);
						context.convertBuffer = NULL;
					}
				}
				GevCloseCamera(&handle);
			}
			else
			{
				printf("Error : 0x%0x : opening camera\n", status);
			}
		}
	}

	// Close down the API.
	GevApiUninitialize();

	// Close socket API
	_CloseSocketAPI ();	// must close API even on error


	//printf("Hit any key to exit\n");
	//kbhit();

	return 0;
}

