//necessary libraries to include
#include "opencv2/opencv.hpp"
#include "opencv2/highgui.hpp"
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
#include <thread>

using namespace cv;
using namespace std;

#define MAX_NETIF					8
#define MAX_CAMERAS_PER_NETIF	32
#define MAX_CAMERAS		(MAX_NETIF * MAX_CAMERAS_PER_NETIF)

// Enable/disable buffer FULL/EMPTY handling (cycling)
#define USE_SYNCHRONOUS_BUFFER_CYCLING	0

#define NUM_BUF	8
#define WINDOW_NAME "Stream from Camera"
#define MONITOR_SCALE 0.5

void *m_latestBuffer = NULL;

typedef struct tagMY_CONTEXT
{
   	const char *        View;
	GEV_CAMERA_HANDLE 	camHandle;
	int					depth;
	int 				format;
	void 				*convertBuffer;
	BOOL				convertFormat;
	BOOL              	exit;
	BOOL 				done;

}	MY_CONTEXT, *PMY_CONTEXT;

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



void *printSomething()
{

   printf("\nHello");

}

void * ImageDisplayThread( void *context)
{
	MY_CONTEXT *displayContext = (MY_CONTEXT *)context;


	int tracking=0;
	//createButton("Tracking Disabled/Enabled",NULL,&tracking,CV_CHECKBOX,0);
	createTrackbar("Tracking Disabled/Enabled",WINDOW_NAME,&tracking,1);
	int changeParams=0;

	// Setup SimpleBlobDetector parameters.
	SimpleBlobDetector::Params params;

	int imageThreshold=40;

	// Change thresholds
	int minThreshold=10;
	params.minThreshold = minThreshold;
	int maxThreshold=255;
	params.maxThreshold = maxThreshold;

	// Filter by Area.
	params.filterByArea = true;
	int area=100;
	params.minArea = area;

	// Filter by Circularity
	params.filterByCircularity = false;
	int minCircularity=60;
	params.minCircularity = minCircularity/100;

	// Filter by Convexity
	params.filterByConvexity = true;
	int minConvexity=87;
	params.minConvexity = minConvexity/100;

	// Filter by Inertia
	params.filterByInertia = false;
	int minInertiaRatio=50;
	params.minInertiaRatio = minInertiaRatio/100;;

	createTrackbar("Min Threshold",WINDOW_NAME,&minThreshold,255);
	createTrackbar("Max Threshold",WINDOW_NAME,&maxThreshold,255);
	createTrackbar("Min Area",WINDOW_NAME,&area,10000);
	createTrackbar("Min Circularity",WINDOW_NAME,&minCircularity,100);
	createTrackbar("Min Convexity",WINDOW_NAME,&minConvexity,100);
	createTrackbar("Min Intertia Ratio",WINDOW_NAME,&minInertiaRatio,100);
	createTrackbar("Change Detection Parameters",WINDOW_NAME,&changeParams,1);
	
	

	// Storage for blobs
	vector<KeyPoint> keypoints;

	// Set up detector with params
	Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);

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
					Mat imgCv=Mat(img->h, img->w, CV_8UC1, m_latestBuffer);
					Mat thresh;
					threshold(imgCv,thresh,getTrackbarPos("Min Threshold",WINDOW_NAME),getTrackbarPos("Max Threshold",WINDOW_NAME),THRESH_BINARY_INV);
					if(tracking){
						Mat imgCv_with_keypoints;
						detector->detect(imgCv,keypoints);
						drawKeypoints( thresh, keypoints, imgCv_with_keypoints, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
						if(!keypoints.empty()){
							Point2f xy=keypoints[0].pt;
							string coordinates="x = " + to_string((int)xy.x) + ", y = " + to_string((int)xy.y);
							putText(imgCv_with_keypoints,coordinates.c_str(),Point(10,50), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						} 
										
					imshow (displayContext->View,imgCv_with_keypoints);					
					waitKey(25);

					if(getTrackbarPos("Change Detection Parameters",WINDOW_NAME)==1){
						params.minThreshold=getTrackbarPos("Min Threshold",WINDOW_NAME);
						params.maxThreshold=getTrackbarPos("Max Threshold",WINDOW_NAME);
						params.minArea=getTrackbarPos("Min Area", WINDOW_NAME);
						params.minCircularity=getTrackbarPos("Min Circularity",WINDOW_NAME)/100;
						params.minConvexity=getTrackbarPos("Min Convexity",WINDOW_NAME)/100;
						params.minInertiaRatio=getTrackbarPos("Min Intertia Ratio",WINDOW_NAME)/100;
						detector=SimpleBlobDetector::create(params);
						setTrackbarPos("Change Detection Parameters",WINDOW_NAME,0);
					}

					} else {
						imshow(displayContext->View,imgCv);
						waitKey(25);
					}

					if(getWindowProperty(WINDOW_NAME,1)<0){
						GevStopTransfer(displayContext->camHandle);
						displayContext->exit=TRUE;
						

					}
				}
				else 
				{
					printf("Error Displaying Image");
					break;
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
	MY_CONTEXT context = {0};
   	pthread_t  tid;
   	thread anotherThread(printSomething);
	char c;
	int done = FALSE;

	//===================================================================================

	printf("\nEye Tracking Program using Teledyne Dalsa Genie Nano Camera and OpenCV (%s) \n", __DATE__);
	printf("Based off of Oculomatic code by Jan, put together by Paolo\n ");

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
	
	status = GevGetCameraList (pCamera, MAX_CAMERAS, &numCamera);
	if (status){
		printf("%d camera(s) on the network \n", numCamera);
	} 

	//====================================================================
	// Connect to Camera

	int i;
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
			
	//====================================================================
	// Open the camera.
	//printf("here2\n");
	status = GevOpenCamera(&pCamera[0], GevExclusiveMode, &handle);

	if(status){
		printf("Failed to open camera\n");
	} else {
		printf("\nSuccessfully opened camera\n");
	}

	GEV_CAMERA_OPTIONS camOptions = {0};
	GevGetCameraInterfaceOptions(handle, &camOptions);

	camOptions.heartbeat_timeout_ms = 90000;		// For debugging (delay camera timeout while in debugger)
	camOptions.streamFrame_timeout_ms = 1001;				// Internal timeout for frame reception.
	camOptions.streamNumFramesBuffered = 4;				// Buffer frames internally.
	camOptions.streamMemoryLimitMax = 64*1024*1024;		// Adjust packet memory buffering limit.	
	camOptions.streamPktSize = 9180;							// Adjust the GVSP packet size.
	camOptions.streamPktDelay = 10;							// Add usecs between packets to pace arrival at NIC.
	
	GevSetCameraInterfaceOptions(handle, &camOptions);

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

	//=================================================================
		// Set up a grab/transfer from this camera
		//
		printf("Camera parameters set for \n\tHeight = %d\n\tWidth = %d\n\tPixelFormat (val) = 0x%08x\n", height,width,format);
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

		namedWindow(WINDOW_NAME,WINDOW_AUTOSIZE);
		
		// Create a thread to receive images from the API and display them.
		context.View = WINDOW_NAME;
		context.camHandle = handle;
		context.exit = FALSE;
		context.done = FALSE;
		pthread_create(&tid, NULL, ImageDisplayThread, &context); 
		
	    // Call the main command loop or the example.
	    PrintMenu();
	    while(!done && !context.exit)
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
   				anotherThread.join();
            }
	    }

	    GevAbortTransfer(handle);
	    status = GevFreeTransfer(handle);
	    

		for (i = 0; i < numBuffers; i++)
		{	
			free(bufAddress[i]);
		}
		if (context.convertBuffer != NULL)
		{
			free(context.convertBuffer);
			context.convertBuffer = NULL;
		}

		GevCloseCamera(&handle);
		GevApiUninitialize();
		if (getWindowProperty(WINDOW_NAME, WND_PROP_AUTOSIZE) != -1)
        	destroyWindow(WINDOW_NAME);
		_CloseSocketAPI();
}