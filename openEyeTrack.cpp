
//necessary libraries to include
#include "opencv2/opencv.hpp"
#include <stdio.h>
#include <stdlib.h>

#include "GenApi/GenApi.h"		//!< GenApi lib definitions.
#include "gevapi.h"				//!< GEV lib definitions.
#include <vector>
#include <string>
#include <pthread.h>

#include <ctime>
#include <queue>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include <signal.h>


#define CVUI_IMPLEMENTATION
#include "cvui.h"

#define PI 3.1415927

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define BLU   "\x1B[34m"
#define YELLOW "\x1B[33m"
#define RESET "\x1B[0m"


//====================================================================================

using namespace cv;
using namespace std;
#define CAMERA_WINDOW "openEyeTrack"
// define the number of image processing threads to use
#define NUM_PROC_THREADS 6
// define the amount of time in micro-seconds to sleep - forces an artifical frame rate on the camera
#define SLEEP_TIME 500
// Enable/disable buffer FULL/EMPTY handling (cycling) - stick with synchronous
#define USE_SYNCHRONOUS_BUFFER_CYCLING	1
// define UDP port number
#define PORT     8080 
//enable/disable print statements for debugging purposes
#define DEBUG_STATEMENTS 1
//define the number of buffers to use when capturing images
#define NUM_BUFF 8

//structure containing info for images captured by the camera
typedef struct
{
	int h;
	int w;
	char image[1];
} CAPTURED_IMG;

//structure containing info for images processed through opencv's blob detection
typedef struct 
{
	Mat img;
	vector<KeyPoint> keypoints;
	int frameID;
} PROCESSED_IMG;

//structure containing info for a "packet" of data to be sent out
typedef struct 
{
	int x;
	int y;
	int area;
	long long timestamp;
	int frameID;
} NETWORK_DATA;

//structure containing all the parameters for pupil detection
typedef struct _DETECTION_PARAMS
{
	//Simple Blob Detector Parameters
	int minThreshold;
	int maxThreshold;
	int filterByColor;
	int BlobColor;
	int filterByArea;
	int area;
	int filterByCircularity;
	int minCircularity;
	int filterByConvexity;
	int minConvexity;
	int filterByInertia;
	int minInertiaRatio;

	
	//region of interest parameters
	int gateLeft;
	int gateRight;
	int gateBottom ;
	int gateTop ;

	Rect roi;

} DETECTION_PARAMS;

//main structure that is passed into the threads
//contains information related to camera and queues holding unprocessed/processed frames
typedef struct 
{
	GEV_STATUS status;
	GEV_CAMERA_HANDLE handle = NULL;
	int numCamera =0;
	int image_size;
	int done=0;
	int tracking=0;
	int changeParams=0;
	int globalFrameCount = 0;
	int height;
	int width;

	PUINT8 bufAddress[NUM_BUFF];

	// lock/queue for captured images
	pthread_mutex_t capture_lock;
	queue<CAPTURED_IMG *> capture_queue;

	// lock/queue for processed images
	pthread_mutex_t display_lock;
	queue<Mat> display_queue;

	// lock for next consumer of the capture_queue
	pthread_mutex_t process_lock;

	// lock/queue for data to be sent out over network
	pthread_mutex_t network_lock;
	queue<NETWORK_DATA> network_queue;
} MY_CAMERA;


//====================================================================================

//global variables
int capturedCount=0;
MY_CAMERA myCam;
DETECTION_PARAMS dectParams;

//====================================================================================

//GUI related button callback functions
void quit (int state, void* userdata)
{
	*(int *)userdata=1;
	destroyAllWindows();
}

void enableFilterByInertia(int state, void* userdata)
{
	if(state==0)
		*(int *) userdata=0;
	else
		*(int *) userdata=1;
}

void enableFilterByCircularity(int state, void* userdata)
{
	if(state==0)
		*(int *) userdata=0;
	else
		*(int *) userdata=1;
}

void enableFilterByConvexity(int state, void* userdata)
{
	if(state==0)
		*(int *) userdata=0;
	else
		*(int *) userdata=1;
}

void enableFilterByArea(int state, void* userdata)
{
	if(state==0)
		*(int *) userdata=0;
	else
		*(int *) userdata=1;
}
void trackBlobsFunc(int state, void* userdata)
{
	if(state==0)
		*(int *) userdata=0;
	else
		*(int *) userdata=1;
}

//====================================================================================

// for printing out frame rate speed and debugging
// timer in milliseconds
static unsigned long msec( void )
{
	struct timeval tm;
	gettimeofday (&tm, NULL);
	unsigned long msec = (tm.tv_sec * 1000) + (tm.tv_usec / 1000);
	return msec;
}

//====================================================================================

//function to reset/stop the transfer of frames from the camera
void resetTransfer(PUINT8 *bufAddress, bool stop, bool quit)
{
	if (stop || quit) {
		GevStopTransfer(myCam.handle);
		GevAbortTransfer(myCam.handle);
		GevFreeTransfer(myCam.handle);
	} 
	if (quit) {
		return;
	}

	//initialize image buffers
	for (int i = 0; i < NUM_BUFF; i++) {
		memset(bufAddress[i], 0, myCam.image_size);
	}


	#if USE_SYNCHRONOUS_BUFFER_CYCLING
		// Initialize a transfer with synchronous buffer handling.
		myCam.status = GevInitializeTransfer( myCam.handle, SynchronousNextEmpty, myCam.image_size, NUM_BUFF, bufAddress);
	#else
		// Initialize a transfer with asynchronous buffer handling.
		myCam.status = GevInitializeTransfer( myCam.handle, Asynchronous, myCam.image_size, NUM_BUFF, bufAddress);
	#endif
	
	GevStartTransfer(myCam.handle, -1);
}

//initialize timers
unsigned long tstart = msec();
auto start = std::chrono::high_resolution_clock::now();

//thread to store captured frames from camera into memory
//much faster rate than processing
void* imageCaptureThread(void *ptr)
{
	// *ptr is a required parameter for thread creation but will not actually be used within the thread

	printf ("Buffer size = %d\n", myCam.image_size);
	
	// Allocate image buffers
	for (int i = 0; i < NUM_BUFF; i++) {
		myCam.bufAddress[i] = (PUINT8)malloc(myCam.image_size);
	}
	resetTransfer (myCam.bufAddress, false,false);

	int count = 0;
	int total = 0;
	
	int reset_count = 0;
	int err_image_count = 0;
	double duration;

	//main loop to continously capture images
	while(! myCam.done)
	{
		total++;
		 duration = (msec() - tstart)/(double)1000;
		int err = 1;

		// limit how fast frames are captured (forces an artificial frame rate) as long as it is acceptable.
		usleep(SLEEP_TIME);	

		//wait for images to be received
		GEV_BUFFER_OBJECT *img = NULL;
		GEV_STATUS status = GevWaitForNextImage(myCam.handle, &img, 100);

		if (img != NULL) // Process this image
		{
			if (status == GEVLIB_OK && img->status == 0) {
				CAPTURED_IMG *cimg = (CAPTURED_IMG *) malloc(sizeof (CAPTURED_IMG)+myCam.image_size);
				cimg->h = img->h;
				cimg->w = img->w;
				memcpy (cimg->image , img->address, myCam.image_size);
				pthread_mutex_lock(&myCam.capture_lock);
				myCam.capture_queue.push(cimg);
				pthread_mutex_unlock(&myCam.capture_lock);

				count++;
				capturedCount++;
				err = 0;
			}
			err_image_count=0;

		#if USE_SYNCHRONOUS_BUFFER_CYCLING
					GevReleaseImage(myCam.handle, img);
		#endif
		} 
		else // notify user there is an error and try to capture a new image
		{

			err_image_count++;
			//if there are more than 3 errors in a row reset the transfer
			if (err_image_count > 3) { 
				printf ("restarting capture transfers\n");
				resetTransfer (myCam.bufAddress, true,false);
				reset_count++;
			}
		}

		//if there was an error, display error information
		if (err) {
			if (status != GEVLIB_OK) {
				printf ("capture error: %s status=%d\n",
					(status != GEVLIB_OK) ? "no image": "not displayable", status);
			}

		#if DEBUG_STATEMENTS
				//display information related to the image capture process
				} else if (count%500 == 0) {
					printf(YELLOW "capture %d, Q = %lu, %3.2f f/s, %3.2f s, resets = %d \n" RESET, count, myCam.capture_queue.size(), count/duration, duration, reset_count);
				
		#endif
		}
	}

	//end the transfer
	resetTransfer(myCam.bufAddress,true,true);
	for (int i = 0; i < NUM_BUFF; i++) {		
		free(myCam.bufAddress[i]);
	}
	printf (GRN "capture thread done\n" RESET);
	pthread_exit(NULL);
}

//thread to send processed image data out on network via a UDP socket
void * networkDataThread(void *ptr)
{	
	int count=0;
	int outputSocket;
	int first;
	NETWORK_DATA data;
	struct sockaddr_in broadcastAddress;
	int broadcastPermission;
	char buffer[120]={0};

	// Creating socket file descriptor 
    if ( (outputSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }

    memset(&broadcastAddress, 0, sizeof(broadcastAddress)); 
      
    // Filling server information 
    broadcastAddress.sin_family = AF_INET; 
    broadcastAddress.sin_port = htons(PORT); 

    //change this address if need be
    //may experience significant packet loss if recieving end is not within same network
    broadcastAddress.sin_addr.s_addr = inet_addr("255.255.255.255");
    
    //edit socket settings
    broadcastPermission = 1;
    if (setsockopt(outputSocket, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, 
          sizeof(broadcastPermission)) < 0)
        printf("setsockopt() failed");
    
    //main loop to continously send out available data
	while(!myCam.done)
	{
		//wait for available data
		if (!myCam.network_queue.empty()){
			usleep(2000);
		}
		//if there is data, send it out on specified socket
		while(!myCam.network_queue.empty()){
			data=myCam.network_queue.front();
			myCam.network_queue.pop();

			int network_data=sprintf(buffer,"frameID=%d, x= %d, y= %d, area=%d, time=%.4lld us",data.frameID,data.x,data.y,
				data.area,data.timestamp);
		#if DEBUG_STATEMENTS
					if (count%500==0){
						printf( GRN "%s \n" RESET,buffer);
					}
					if (count==0)
					{
						first=data.frameID;
						
					}
		#endif
			sendto(outputSocket, (const char *) buffer, strlen(buffer),0, (const struct sockaddr *) &broadcastAddress,  sizeof(broadcastAddress)); 
			count++;
		}
	}

		#if DEBUG_STATEMENTS
			printf("first frameID sent = %d\n",first);
			printf("last frameID sent = %d \n",data.frameID);
			printf("total data sent: %d\n",count);
		#endif
		
		printf ("network thread done\n");
		pthread_exit(NULL);
}

//function for thread(s) to take frames from the captured images queue, run blob detection, and store in memory
void* imageProcessingThread(void * ptr)
{
	int my_idx = (long int) ptr;
	int image_gap=10;
	Mat final=Mat::zeros(myCam.height, myCam.width*2+image_gap, CV_8UC3);
	Mat color_image,grey_image,original_image;
	NETWORK_DATA data;
	

	//======================================================================================
	// Setup SimpleBlobDetector parameters.
	// Create blob detector at every instance.
	SimpleBlobDetector::Params params;

	//main loop to process captured frames
	while(true)
	{
		// prevents multiple processing threads from trying to access queue, saves resources
		pthread_mutex_lock(&myCam.process_lock);
		// check/grab image from capture_queue
		while (!myCam.done && myCam.capture_queue.empty()) {
			usleep(1000);
		}
		//prevents threads from getting stuck waiting for the process lock upon termination
		if (myCam.done) {
			pthread_mutex_unlock(&myCam.process_lock);
			break;
		}
		//ensures only one thread can grab and pop images from queue
		pthread_mutex_lock(&myCam.capture_lock);

		CAPTURED_IMG *cimg=myCam.capture_queue.front();
	
		myCam.capture_queue.pop();
		int frameID = myCam.globalFrameCount++;
		pthread_mutex_unlock(&myCam.capture_lock);

		// all frames go through the capture_queue ... 
		int size=myCam.capture_queue.size();

		// allow other threads to get the next item in the capture_queue
		pthread_mutex_unlock(&myCam.process_lock);

		PROCESSED_IMG pImg;

		// NOTE: clone so there is no reference to cimg->image and we can free cimg immediately
		pImg.img = Mat(cimg->h, cimg->w, CV_8UC1, cimg->image).clone();
		pImg.frameID = frameID;
		
		grey_image=pImg.img.clone();
		cvtColor(grey_image,original_image,COLOR_GRAY2BGR);
		cvtColor(grey_image,color_image,COLOR_GRAY2BGR);
		
		free(cimg);
		if(myCam.tracking)
		{	

			// Force update of detection parameters for all threads on every iteration, allows for "real-time" modifications
			params.minThreshold = dectParams.minThreshold;
			params.maxThreshold = dectParams.maxThreshold;
			params.filterByArea = dectParams.filterByArea;
			params.minArea = dectParams.area;
			params.filterByCircularity = dectParams.filterByCircularity;
			params.minCircularity = dectParams.minCircularity/100;
			params.filterByConvexity = dectParams.filterByConvexity;
			params.minConvexity = dectParams.minConvexity/100;
			params.filterByInertia = dectParams.filterByInertia;
			params.minInertiaRatio = dectParams.minInertiaRatio/100;
			dectParams.roi=Rect(dectParams.gateLeft, dectParams.gateBottom, dectParams.gateRight - dectParams.gateLeft, dectParams.gateTop - dectParams.gateBottom);
			
			// one detector object per thread
			Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);
			//threshold original image for easier and faster blob detection
			threshold(pImg.img,pImg.img,dectParams.minThreshold,dectParams.maxThreshold,THRESH_BINARY);
			//detect blobs in region of interest
			detector->detect(pImg.img(dectParams.roi), pImg.keypoints);
			cvtColor(pImg.img,color_image,COLOR_GRAY2BGR);
			//draw region of interst for user reference
			rectangle(color_image,Point(dectParams.gateLeft,dectParams.gateTop),Point(dectParams.gateRight,dectParams.gateBottom),Scalar(255,0,0));
				
				//print data to window and send to network queue
				if(!pImg.keypoints.empty())
				{
				
					Point2f center=pImg.keypoints[0].pt;
					Point2f offset(center.x+dectParams.gateLeft,center.y+dectParams.gateBottom);
					float r = pImg.keypoints[0].size/2.0;

					//draw circles around pupil in original and thresholded image
					circle(color_image,offset,r,Scalar(0,0,255),2,4);
					circle(original_image,offset,r,Scalar(0,0,255),2,4);
					int area = PI*(pow(r,2));
					string coordinates="x = " + to_string((int)center.x+dectParams.gateLeft) + ", y = " + to_string((int)center.y+dectParams.gateBottom) + ", Area=" + to_string(area);
					putText(color_image,coordinates.c_str(),Point(10,50), FONT_HERSHEY_DUPLEX, .8, Scalar(0,0,255));

					data.x=center.x+dectParams.gateLeft;
					data.y=center.y+dectParams.gateBottom;
					data.area=area;
					auto elapsed = std::chrono::high_resolution_clock::now() - start;
					long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
					data.timestamp=microseconds;
					data.frameID=frameID;

					pthread_mutex_lock(&myCam.network_lock);
					myCam.network_queue.push(data);
					pthread_mutex_unlock(&myCam.network_lock);
				}

		}
		//put original and processed images side-by-side in the "final" image to be displayed
		original_image.copyTo(final(Range(0,myCam.height), Range(0,myCam.width)));
		color_image.copyTo(final(Range(0,myCam.height), Range(myCam.width+image_gap,myCam.width*2+image_gap)));
		pImg.img.release();
		pImg.keypoints.clear();

		pthread_mutex_lock(&myCam.display_lock);
		myCam.display_queue.push(final);
		pthread_mutex_unlock(&myCam.display_lock);
	}
	printf (GRN "process thread %d done\n" RESET, my_idx);
	pthread_exit(NULL);
}

//thread to grab processed images and display them in separate window
void imageDisplayThread ()
{
	
	Mat final;
	int displayed = 0;
	double duration = 0;

	while (true) 
	{
		while (! myCam.done && myCam.display_queue.empty()) 
		{
			usleep(1000);
		}

		if (myCam.done) 
		{
			break;
		}
		pthread_mutex_lock(&myCam.display_lock);

		final=myCam.display_queue.front();
		myCam.display_queue.pop();
		// force drain display queue
		while(!myCam.display_queue.empty()) 
		{
			Mat tmp = myCam.display_queue.back();
			tmp.release();
			myCam.display_queue.pop();
		}

		pthread_mutex_unlock(&myCam.display_lock);

		
		duration = (msec() - tstart)/(double)1000.0;

		#if DEBUG_STATEMENTS
			if (displayed % 500 == 0) 
			{
				printf(BLU "display %d Q=%lu %3.2f f/s \n" RESET, displayed, myCam.display_queue.size(), (double)displayed/duration);		
			}	
		#endif

		//print frame rate
		cvui::printf(final,1000,485, .6, 0x0000FF, "Capture Frame Rate: %3.2f", (double)capturedCount/duration);

		//show the original image and processed image on the window
		imshow(CAMERA_WINDOW,final);
		int key = waitKey(1);

		final.release();

		//quit by pressing q whem image window is selected
		if (key == 'q' || key == 'Q') {
			myCam.done = true;
			destroyAllWindows();
		}

		displayed++;

	}	

	printf ("display thread done\n");
	//not really a thread (called within main) so no need to pthread_exit
}
void displayLine()
{
	printf(YELLOW "----------------------------------------------------------------------------------------\n" RESET);
}
void dispRed(const char *s)
{
	printf (RED "%s" RESET, s);
}
void dispGRN(const char *s)
{
	printf (GRN "%s" RESET, s);
}


void cleanUpandFinishMain(int sig)
{
    myCam.done=true;
    displayLine();
}

void displayWelcome()
{
	displayLine();
	dispRed("\n openEyeTrack Copyright (C) 2019 Jorge (Paolo) Casas and Chandramouli Chandrasekaran, Boston University.");
    dispRed("\n This program comes with ABSOLUTELY NO WARRANTY; for details see the License file");
    dispRed("\n This is free software, and you are welcome to redistribute it under certain conditions (see License). \n" );
    dispRed(" Based off of Oculomatic code by Jan, rebuilt by Paolo and Chand (Chand Lab)\n");
    printf(RED "\n Eye Tracking Program using Teledyne Dalsa Genie Nano Camera and OpenCV (%s) \n", __DATE__ RESET);
    displayLine();
}

int main(int argc, char* argvp[])
{
	//does this need to be 8*32? TODO
	GEV_DEVICE_INTERFACE  pCamera[1] = {0};

	//greetings
	displayWelcome();

	//============================================================================================
	
	//verify if there are available cameras
	myCam.status = GevGetCameraList (pCamera, 8*32, &myCam.numCamera);
	if (!myCam.status){
		printf(GRN "%d camera(s) on the network \n" RESET, myCam.numCamera );
	} else {
		exit(0);
	}

	//============================================================================================ 
	
	//attempt to open a camera
	myCam.status = GevOpenCamera(&pCamera[0], GevExclusiveMode, &myCam.handle);
	if(myCam.status){
		printf("Failed to open camera\n");
		exit(0);
	} else {
		dispGRN("\nSuccessfully opened camera\n");
	}

	//============================================================================================
	
	//connect to camera
	UINT32 height = 0;
	UINT32 width = 0;
	UINT32 format = 0;
	UINT32 maxHeight = 1600;
	UINT32 maxWidth = 2048;
	UINT32 maxDepth = 2;
	UINT64 size;
	UINT64 payload_size;
	float frameRate = 0;

	char key;
	Scalar textColor=Scalar(0,0,255);

	GenApi::CNodeMapRef *Camera = static_cast<GenApi::CNodeMapRef*>(GevGetFeatureNodeMap(myCam.handle));
	
	//============================================================================================
	//get and print camera features
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

			GenApi::CFloatPtr ptrFloatNode = Camera->_GetNode("AcquisitionFrameRate");
			frameRate = (float)ptrFloatNode->GetValue();
		}
		// Catch all possible exceptions from a node access.
		CATCH_GENAPI_ERROR(myCam.status);
	}

	
	printf(GRN "Camera parameters set for \n\tHeight = %d\n\tWidth = %d\n\tPixelFormat (val) = 0x%08x,\n\tFrame Rate: %3.2f fps \n" RESET, 
		height,width,format, frameRate);
	displayLine();
	maxHeight = height;
	maxWidth = width;
	maxDepth = GetPixelSizeInBytes(format);
	myCam.height=height;
	myCam.width=width;
	
	// initialize detection parameters
	dectParams.minThreshold = 25;
	dectParams.maxThreshold = 100;
	dectParams.filterByArea=1;
	dectParams.area=100;
	dectParams.filterByCircularity=0;
	dectParams.minCircularity=0;
	dectParams.filterByConvexity=0;
	dectParams.minConvexity=0;
	dectParams.filterByInertia=0;
	dectParams.minInertiaRatio=0;
	dectParams.gateLeft=100;
	dectParams.gateRight=470;
	dectParams.gateBottom=150;
	dectParams.gateTop=450;
	myCam.globalFrameCount = 0;


	//============================================================================================
	// determine size for image buffer allocation
	size = maxDepth * maxWidth * maxHeight;
	size = (payload_size > size) ? payload_size : size;
	myCam.image_size = size;


	//============================================================================================

	//set up GUI and user control
	namedWindow(CAMERA_WINDOW);

	createButton("Enable Tracking via Blob Detection",trackBlobsFunc,&myCam.tracking,QT_CHECKBOX,0);
	createTrackbar("Min Threshold","",&dectParams.minThreshold,255,NULL);
	createTrackbar("Max Threshold","",&dectParams.maxThreshold,255,NULL);

	createButton("Filter By Area",enableFilterByArea,&dectParams.filterByArea,QT_CHECKBOX,1);
	createTrackbar("Min Blob Area","",&dectParams.area,10000,NULL);

	createButton("Filter By Circularity",enableFilterByCircularity,&dectParams.filterByCircularity,QT_CHECKBOX,0);
	createTrackbar("Min Circularity","",&dectParams.minCircularity,100,NULL);

	createButton("Filter By Convexity",enableFilterByConvexity,&dectParams.filterByConvexity,QT_CHECKBOX,0);
	createTrackbar("Min Convexity","",&dectParams.minConvexity,100,NULL);

	createButton("Filter By Inertia",enableFilterByInertia,&dectParams.filterByInertia,QT_CHECKBOX,0);
	createTrackbar("Min Inertia Ratio","",&dectParams.minInertiaRatio,100,NULL);
	
	createTrackbar("Left Gate","",&dectParams.gateLeft,672,NULL);
	createTrackbar("Right Gate","",&dectParams.gateRight,672,NULL);
	createTrackbar("Top Gate","",&dectParams.gateBottom,512,NULL);
	createTrackbar("Bottom Gate","",&dectParams.gateTop,512,NULL);

	createButton("Quit",quit,&myCam.done,QT_PUSH_BUTTON | QT_NEW_BUTTONBAR,0);

	//============================================================================================

	// If the user presses Ctrl-C to exit the program
	
	signal(SIGINT, cleanUpandFinishMain);

	//initialize locks and threads ...
	pthread_mutex_init(&myCam.capture_lock, NULL);
	pthread_mutex_init(&myCam.display_lock, NULL);
	pthread_mutex_init(&myCam.process_lock, NULL);
	pthread_mutex_init(&myCam.network_lock, NULL);

	printf ("NUM_PROC_THREADS = %d\n", NUM_PROC_THREADS);
	pthread_t thread_proc[NUM_PROC_THREADS], thread_capture, thread_display, thread_network;
	
	//create threads for image capture, image processing, image display, and data transmission
	for (int i = 0; i < NUM_PROC_THREADS; i++) {
		pthread_create(&thread_proc[i],NULL,imageProcessingThread, (void *) (intptr_t) i);
	}
	pthread_create(&thread_capture,NULL,imageCaptureThread,NULL);
	pthread_create(&thread_network,NULL,networkDataThread,NULL);
	imageDisplayThread();


	//wait for all threads to finish before closing the program
	pthread_join(thread_capture,NULL);
	for (int i = 0; i < NUM_PROC_THREADS; i++) {
		pthread_join(thread_proc[i],NULL);
	}

	printf(RED "Finishing Main\n" RESET);
    printf(GRN "Gracefully exiting the program -- Terminating threads and closing camera \n" RESET);

	destroyAllWindows();
	GevCloseCamera(&myCam.handle);

	GevApiUninitialize();
	_CloseSocketAPI();

	pthread_mutex_destroy(&myCam.capture_lock);
	pthread_mutex_destroy(&myCam.display_lock);
	pthread_mutex_destroy(&myCam.process_lock);
	pthread_mutex_destroy(&myCam.network_lock);
	
	return 0;
}
