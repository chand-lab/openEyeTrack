# openEyeTrack
## Hardware Specs
- Computer: Intel(R) Core(TM) i7-5820K CPU @ 3.30GHz, 64GB
 - Basic IR light source
 - Camera: Genie Nano M640 NIR
 - Netgear Power Over Ethernet

## Software Specs
- Ubuntu 16.04
- OpenCV 4.1.0
- QT5
- GigE-V Framework for Linux

## Installation

### Installing GigE-V Framework
- Go to Teledyne dalsa website and download the GigE-V Framework for Linux under support → downloads → SDKs
- Install all prerequisites listed in User Manual (GNU make can be installed through build-essential and libX11-dev should be libx11-dev)
- Unzip the .tar file in downloads and then go inside the newly created directory and copy the .tar file that looks like GigE-V-Framework_x86_2.02.0.0132.tar.gz to $HOME
- Cd into home and unpack tar file (use tar -zxf)
- Cd into DALSA and type $ ./corinstall
- Go to connection settings and edit the ethernet connection that is associated with the camera
- Go to the IPv4 settings and change the method to “Link-Local Only” and save changes
- Make sure the connection is enabled and verify you can connect to camera by typing     $ GigeDeviceStatus from terminal
- Try running the genicam_cpp_demo found under DALSA/GigeV/examples
- ***run the gev_nettweak tool (found under GigeV/bin) to enhance performance and prevent resetting the image transfer***
This needs to be re-run if the system restarts

### Installing QT5
- sudo apt-get install build-essential 
- sudo apt-get install qtcreator 
- sudo apt-get install qt5-default

### Installing opencv (version 4.1.0)
https://www.learnopencv.com/install-opencv-3-4-4-on-ubuntu-16-04/

- Navigate to the directory where you want to clone the repository
- Type:
  - $sudo apt -y update
  - $sudo apt -y upgrade
  - $sudo apt -y remove x264 libx264-dev

- Install dependencies
  - $sudo apt -y install build-essential checkinstall cmake pkg-config yasm
  - $sudo apt -y install git gfortran
  - $sudo apt -y install libjpeg8-dev libjasper-dev libpng12-dev
  - $sudo apt -y install libtiff5-dev
  - $sudo apt -y install libtiff-dev
  - $sudo apt -y install libavcodec-dev libavformat-dev libswscale-dev libdc1394-22-dev
  - $sudo apt -y install libxine2-dev libv4l-dev
- cd /usr/include/linux
- sudo ln -s -f ../libv4l1-videodev.h videodev.h
- cd $cwd
- $ sudo apt -y install libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev
- $ sudo apt -y install libgtk2.0-dev libtbb-dev qt5-default
- $ sudo apt -y install libatlas-base-dev
- $ sudo apt -y install libfaac-dev libmp3lame-dev libtheora-dev
- $ sudo apt -y install libvorbis-dev libxvidcore-dev
- $ sudo apt -y install libopencore-amrnb-dev libopencore-amrwb-dev
- $ sudo apt -y install libavresample-dev
- $ sudo apt -y install x264 v4l-utils
- Optional dependencies
  - $ sudo apt -y install libprotobuf-dev protobuf-compiler
  - $ sudo apt -y install libgoogle-glog-dev libgflags-dev
  - $ sudo apt -y install libgphoto2-dev libeigen3-dev libhdf5-dev doxygen

- Clone opencv and opencv_contrib
  - $ git clone https://github.com/opencv/opencv.git
  - $ cd opencv
  - $ git checkout 4.1.0
  - $ cd ..
  - $ git clone https://github.com/opencv/opencv_contrib.git
  - $ cd opencv_contrib
  - $ git checkout 4.1.0
  - $ cd ..

 (Need to clone the same version of opencv_contrib)
 
  - Make a build directory within opencv and start compilation process
  - $ cd opencv
  - $ mkdir build
  - $ cd build
  - Use: cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local-D WITH_TBB=ON -D WITH_V4L=ON -D WITH_QT=ON -D WITH_OPENGL=ON -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules -D BUILD_EXAMPLES=OFF -D OPENCV_GENERATE_PKGCONFIG=ON ..

(For later versions of OpenCV, need to include OPENCV_GENERATE_PKGCONFIG=ON)

 - Use  $ sudo make install -j4
(having the ‘-j4’ says to use use multiple threads which greatly speeds up this step)

- Then type:
  - $ sudo sh -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/opencv.conf'
  - $ sudo ldconfig

### Usage

- Open up terminal and navigate to the directory where you want the repository to be cloned to
- Clone the repository from git using: $git clone https://github.com/mailchand/openEyeTrack.git
- Open up the makefile and check that the paths found on lines 5 and 31 (start with “IROOT” and “include” are correct)
- Type in: $ make 
  - If there are errors it is likely due to a linking libraries issue
- Type in: $ ./openEyeTrack
- If there are no errors so far, your screen should look something like this:
# add image here

- To bring up the control panel and enable tracking: you can either press Ctrl+P while the display window is selected or navigate to the very right of the toolbar and click on the paintbrush icon

# add 2 images here



- From here, you can enable/disable the tracking and adjust the various parameters
  - The main filtering parameters will be the thresholding values, the minimum area of the blob, and the gates which specify the region of interest

# add last image here

 - To quit the application, either click the “quit” button on the control panel or press the “q” key while the display window is selected

### Performance Tuning

- If you experience the camera resetting its transfer while capturing images then running the gev_nettweak tool as mentioned above will likely solve this problem
  - Additionally, you can adjust the SLEEP_TIME parameter (is in microseconds) to force an artificial frame rate upon the capture thread
- When tracking is enabled, if there is a lag between movement in real life and the time when it is shown on the display window then it is likely due to the expansion of the capture_queue (this can be seen if DEBUG_STATEMENTS is set to 1). There are two main solutions to fix this problem:
  - One approach is to increase the number of processing threads (defined as NUM_PROC_THREADS near the top of the program). This allows multiple frames to be processed simultaneously which ensures the length of the capture_queue does not increase.
  - The better approach is to adjust the lighting in the environment. Something as simple as adjusting the aperture of the camera may do the trick but in the case of pupil detection, an infrared light source is often required.
You can also play around with the number of buffers (defined as NUM_BUFF) that the camera initially stores the frames in but 4 seems to be a good number and no noticeable advantages have been found from increasing this amount

### Notes
- You can set DEBUG_STATEMENTS to 1 (found near the top of the code) in order to display features such as frame rates and queue lengths
- There is an option to use sychronous vs. asychronous buffer cycling denoted by USE_SYNCHRONOUS_BUFFER_CYCLING, program seems to work beter with sychronous mode so keep value set at 1
