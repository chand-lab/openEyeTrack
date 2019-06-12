//necessary libraries to include
#include "opencv2/opencv.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "cordef.h"
#include "GenApi/GenApi.h"		//!< GenApi lib definitions.
#include "gevapi.h"				//!< GEV lib definitions.
//#include "SapX11Util.h"
//#include "X_Display_utils.h"
//#include "FileUtil.h"
//#include <sched.h>
#include <vector>
#include <string>
#include <thread>

using namespace cv;
using namespace std;
#define CAMERA_WINDOW "Camera Stream"
#define TRACK_WINDOW "Eye Tracking"
#define CONTROL_WINDOW "User Controls"


void *m_latestBuffer = NULL;

// Enable or disable tracking
int tracking=0;

int done=0;

int changeParams=0;

void startTracking(int state, void* userdata)
{
	if(state==0)
		tracking=0;
	else
		tracking=1;
}

void quit (int state, void* userdata)
{
	done=1;
	destroyAllWindows();
}

void changeDetectorParams(int state, void* userdata)
{
	changeParams=1;
} 

int main(int argc, char* argvp[])
{

	GEV_DEVICE_INTERFACE  pCamera[8*32] = {0};
	GEV_CAMERA_HANDLE handle = NULL;
	int numCamera =0;
	GEV_STATUS status;
	char key;
	printf("\nEye Tracking Program using Teledyne Dalsa Genie Nano Camera and OpenCV (%s) \n", __DATE__);
	printf("Based off of Oculomatic code by Jan, put together by Paolo\n \n");

	//verify if there are available cameras
	status = GevGetCameraList (pCamera, 8*32, &numCamera);
	if (!status){
		printf("%d camera(s) on the network \n", numCamera);
	} 

	//attempt to open camera
	status = GevOpenCamera(&pCamera[0], GevExclusiveMode, &handle);
	if(status){
		printf("Failed to open camera\n");
	} else {
		printf("\nSuccessfully opened camera\n");
	}

	//connect to camera
	int i;
	UINT32 height = 0;
	UINT32 width = 0;
	UINT32 format = 0;
	UINT32 maxHeight = 1600;
	UINT32 maxWidth = 2048;
	UINT32 maxDepth = 2;
	UINT64 size;
	UINT64 payload_size;
	int numBuffers = 8;
	PUINT8 bufAddress[8];
	

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

	status = GevInitializeTransfer( handle, Asynchronous, size, numBuffers, bufAddress);
	
	GEV_BUFFER_OBJECT *img = NULL;
	
	status = GevStartTransfer( handle, -1);
	
	Mat blackImg = Mat(512,672, CV_8UC3, Scalar(0,0,0));
	namedWindow(CAMERA_WINDOW);
	
	//namedWindow(TRACK_WINDOW);
	imshow(CONTROL_WINDOW,blackImg);
	namedWindow(CONTROL_WINDOW,CV_WINDOW_NORMAL);
	//resizeWindow(CONTROL_WINDOW,500,500);
	
	// Setup SimpleBlobDetector parameters.
	SimpleBlobDetector::Params params;

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
	params.minInertiaRatio = minInertiaRatio/100;

	createButton("Tracking On/Off",startTracking,NULL,CV_CHECKBOX,0);
	createButton("Change Detector Parameters",changeDetectorParams,NULL,CV_PUSH_BUTTON,0);
	createButton("Quit",quit,NULL,CV_PUSH_BUTTON,0);
	
	createTrackbar("Min Threshold",CONTROL_WINDOW,&minThreshold,255,NULL);
	createTrackbar("Max Threshold",CONTROL_WINDOW,&maxThreshold,255,NULL);
	createTrackbar("Min Area",CONTROL_WINDOW,&area,10000,NULL);
	createTrackbar("Min Circularity",CONTROL_WINDOW,&minCircularity,100,NULL);
	createTrackbar("Min Convexity",CONTROL_WINDOW,&minConvexity,100,NULL);
	createTrackbar("Min Inertia Ratio",CONTROL_WINDOW,&minInertiaRatio,100,NULL);
	
	// Storage for blobs
	vector<KeyPoint> keypoints;

	// Set up detector with params
	Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);
	Mat thresh;
	Mat imgCv_with_keypoints;
	
	Mat final=Mat::zeros(blackImg.rows, blackImg.cols*2+10, CV_8UC3);
	Mat imgCv;

	putText(blackImg,"Current Detection Parameters Set for:",Point(10,50), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Min Threshold = " + to_string(minThreshold),Point(10,80), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Max Threshold = " + to_string(maxThreshold),Point(10,110), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Min Area = " + to_string(area),Point(10,140), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Min Circularity = " + to_string(minCircularity),Point(10,170), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Min Convexity = " + to_string(minConvexity),Point(10,200), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
	putText(blackImg,"Min Inertia Ratio = " + to_string(minInertiaRatio),Point(10,230), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));

	while(!done){
		
	// Wait for images to be received
		status = GevWaitForNextImage(handle, &img, 1000);

		if ((img != NULL) && (status == GEVLIB_OK))
		{
			if (img->status == 0)
			{
				m_latestBuffer = img->address;
				Mat grey=Mat(img->h, img->w, CV_8UC1, m_latestBuffer);
				cvtColor(grey,imgCv,COLOR_GRAY2BGR);
				imgCv.copyTo(final(Range::all(), Range(0, blackImg.cols)));
				/*
				imgCv.copyTo(final(Range::all(), Range(blackImg.cols+10, blackImg.cols*2+10)));
				imshow(CAMERA_WINDOW,final);
				waitKey(25);
				*/
				
				//printf("imgCv h = %d w = %d \n",imgCv.rows,imgCv.cols);
				
				//threshold(imgCv,thresh,getTrackbarPos("Min Threshold",CONTROL_WINDOW),getTrackbarPos("Max Threshold",CONTROL_WINDOW),THRESH_BINARY_INV);
				if(tracking){
					/*
					printf("imgCv h = %d w = %d \n",imgCv.rows,imgCv.cols);
					printf("blackimgCv h = %d w = %d \n",blackImg.rows,blackImg.cols);
					printf("final h = %d w = %d \n",final.rows,final.cols);
					*/
					
					//printf("here\n");
					detector->detect(imgCv,keypoints);
					drawKeypoints( imgCv, keypoints, imgCv_with_keypoints, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
					
					if(!keypoints.empty()){
						Point2f xy=keypoints[0].pt;
						string coordinates="x = " + to_string((int)xy.x) + ", y = " + to_string((int)xy.y);
						putText(imgCv_with_keypoints,coordinates.c_str(),Point(10,50), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
					}
					
					//hconcat(imgCv,imgCv_with_keypoints,final);
					imgCv_with_keypoints.copyTo(final(Range::all(), Range(blackImg.cols+10, blackImg.cols*2+10)));
					imshow(CAMERA_WINDOW, final);	
					imshow(CONTROL_WINDOW, blackImg);
					key = waitKey(15);
					//printf("key h =%d w = %d \n", imgCv_with_keypoints.rows,imgCv_with_keypoints.cols);				
					

					if(changeParams){
						minThreshold=getTrackbarPos("Min Threshold",CONTROL_WINDOW);
						params.minThreshold=minThreshold;
						maxThreshold=getTrackbarPos("Max Threshold",CONTROL_WINDOW);
						params.maxThreshold=maxThreshold;
						area=getTrackbarPos("Min Area", CONTROL_WINDOW);
						params.minArea=area;
						minCircularity=getTrackbarPos("Min Circularity",CONTROL_WINDOW);
						params.minCircularity=minCircularity/100;
						minConvexity=getTrackbarPos("Min Convexity",CONTROL_WINDOW);
						params.minConvexity=minConvexity/100;
						minInertiaRatio=getTrackbarPos("Min Inertia Ratio",CONTROL_WINDOW);
						params.minInertiaRatio=minInertiaRatio/100;
						detector=SimpleBlobDetector::create(params);
						changeParams=0;
						blackImg.setTo(Scalar(0,0,0));

						putText(blackImg,"Current Detection Parameters Set for:",Point(10,50), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Min Threshold = " + to_string(minThreshold),Point(10,80), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Max Threshold = " + to_string(maxThreshold),Point(10,110), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Min Area = " + to_string(area),Point(10,140), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Min Circularity = " + to_string(minCircularity),Point(10,170), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Min Convexity = " + to_string(minConvexity),Point(10,200), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
						putText(blackImg,"Min Inertia Ratio = " + to_string(minInertiaRatio),Point(10,230), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));
					}
					
				} else {
				
				imshow(CAMERA_WINDOW,final);
				key = waitKey(15);
				}
				
				if (key==0x1B)
				{
					done=1;
					destroyAllWindows();
				}	
			}
			
		}
	}
	GevStopTransfer(handle);
	GevAbortTransfer(handle);
	status = GevFreeTransfer(handle);
	for (i = 0; i < numBuffers; i++)
	{	
		free(bufAddress[i]);
	}
	GevCloseCamera(&handle);
	GevApiUninitialize();
	_CloseSocketAPI();


	return 0;
}


