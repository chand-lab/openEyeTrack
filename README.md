# openEyeTrack
## Tested Hardware Specs
- Computer: Intel(R) Core(TM) i7-5820K CPU @ 3.30GHz, 64GB
 - Basic IR light source
 - Camera: Genie Nano M640 NIR (Teledyne DALSA)
 - Netgear Power-Over-Ethernet Switch

## Software Specs
- Ubuntu 16.04
- OpenCV 4.1.0
- Qt 5.5.1
- GigE-V Framework for Linux
- Git

## Installation

 - Install git by typing: $ sudo apt-get install git-core
 - Clone repository by typing: $ git clone https://github.com/mailchand/openEyeTrack.git
 - After installing the GigE-V Framework, you can either manually set up Qt and OpenCV or run the installation script denoted as *software_install* within the repository
   - If you choose to run the installation script you may need to first convert it to an executable by typing: $ chmod +x software_install.sh
   - Occassionally, you may be prompted to press "y" to continue the installation process
   - Please explicitly run the last two commands at the bottom of this file concerning linking

### Installing GigE-V Framework
- Go to Teledyne dalsa website and download the GigE-V Framework for Linux under support → downloads → SDKs
- Install all prerequisites listed in User Manual (GNU make can be installed through build-essential and libX11-dev should be libx11-dev)
- Unpack the .tar file in downloads and then go inside the newly created directory and copy the .tar file that looks like GigE-V-Framework_**x86**_2.02.0.0132.tar.gz to $HOME
- Cd into home and unpack tar file (use tar -zxf)
- Cd into DALSA and type $ ./corinstall
- Go to connection settings and edit the ethernet connection that is associated with the camera
- Go to the IPv4 settings and change the method to “Link-Local Only” and save changes
- Make sure the connection is enabled and verify you can connect to camera by going to terminal and typing:$ GigeDeviceStatus 
- Try running the genicam_cpp_demo found under DALSA/GigeV/examples
- ***run the gev_nettweak tool (found under GigeV/bin) to enhance performance and prevent resetting the image transfer.***
This tool needs to be re-run if the system restarts

### Installing Qt 5.5.1
    sudo apt-get install build-essential
    sudo apt-get install qt5-default

### Installing opencv (version 4.1.0)
Navigate to the directory where you want to clone the repository
Type:

    sudo apt -y update
    sudo apt -y upgrade
    sudo apt -y remove x264 libx264-dev

Install dependencies
```
    sudo apt -y install build-essential checkinstall cmake pkg-config yasm
    sudo apt -y install git gfortran
    sudo apt -y install libjpeg8-dev libjasper-dev libpng12-dev
    sudo apt -y install libtiff5-dev
    sudo apt -y install libtiff-dev
    sudo apt -y install libavcodec-dev libavformat-dev libswscale-dev libdc1394-22-dev
    sudo apt -y install libxine2-dev libv4l-dev
    sudo apt -y install libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev
    sudo apt -y install libgtk2.0-dev libtbb-dev qt5-default
    sudo apt -y install libatlas-base-dev
    sudo apt -y install libfaac-dev libmp3lame-dev libtheora-dev
    sudo apt -y install libvorbis-dev libxvidcore-dev
    sudo apt -y install libopencore-amrnb-dev libopencore-amrwb-dev
    sudo apt -y install libavresample-dev
    sudo apt -y install x264 v4l-utils
``` 
Optional dependencies

    sudo apt -y install libprotobuf-dev protobuf-compiler
    sudo apt -y install libgoogle-glog-dev libgflags-dev
    sudo apt -y install libgphoto2-dev libeigen3-dev libhdf5-dev doxygen

Clone opencv and opencv_contrib

    git clone https://github.com/opencv/opencv.git
    cd opencv
    git checkout 4.1.0
    cd ..
    git clone https://github.com/opencv/opencv_contrib.git
    cd opencv_contrib
    git checkout 4.1.0
    cd ..

 (Need to clone the same version of opencv_contrib)
 
 Make a build directory within opencv and start compilation process
 
    cd opencv
    mkdir build
    cd build
    
 Use:
 ```
 cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local 
 -D WITH_TBB=ON 
 -D WITH_V4L=ON 
 -D WITH_QT=ON 
 -D WITH_OPENGL=ON 
 -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules 
 -D BUILD_EXAMPLES=OFF 
 -D OPENCV_GENERATE_PKGCONFIG=ON ..
```
(For later versions of OpenCV, need to include OPENCV_GENERATE_PKGCONFIG=ON)

Use:  

    sudo make install -j4
(having the ‘-j4’ says to use use multiple threads which greatly speeds up this step)

**Then type:**

    sudo sh -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/opencv.conf'
    sudo ldconfig

For detailed usage examples, see [USAGE.md](docs/USAGE.md) found under the docs folder




