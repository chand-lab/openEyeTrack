---
title: 'openEyeTrack - A high speed multi-threaded eye tracker for head-fixed applications'
tags:
 - C++
 - Eye-Tracking
 - Threading
 - OpenCV
 - Teledyne DALSA
authors:
  - name: Jorge Paolo Casas
    orcid: 0000-0002-9575-1717
    affiliation: 1
  - name: Chandramouli Chandrasekaran
    orcid: 0000-0002-1711-590X
    affiliation: "2, 3, 4"
affiliations:
 - name: Department of Biomedical Engineering, Boston University, Boston, MA 02215, USA
   index: 1
 - name: Department of Anatomy and Neurobiology, Boston University, Boston, MA 02118, USA
   index: 2
 - name: Department of Psychological and Brain Sciences, Boston University, Boston, MA 02215, USA
   index: 3
 - name: Center for Systems Neuroscience, Boston University, Boston, MA 02215, USA
   index: 4
date: 26 July 2019
bibliography: paper.bib
---

### Statement of Need

When faced with a decision, an organism uses information gathered by its senses in order to determine the best course of action. Vision is one of the primary senses and tracking eye gaze can offer insight into the cues that affect decision making behavior. Thus, to study decision-making and other cognitive processes it is fundamentally necessary to accurately track eye position. However, commercial eye trackers are 1) often very expensive, and 2) incorporate their own proprietary software to detect the movement of the eye. Closed source solutions also limit the researcher’s ability to be fully informed regarding the algorithms used to track the eye within their experiment and to incorporate modifications tailored to their needs. Here, we present our software solution, _openEyeTrack_, a low cost, high speed, low latency, open-source video-based eye tracker. Video based eye trackers can perform nearly as well as classical scleral search coil based methods and can be used for most applications [@10.3389/fnbeh.2012.00049]. 

**Planned Use Cases**: We expect to incorporate _openEyeTrack_ in research concerning the neural dynamics of cognition, decision-making, and motor-control conducted at the Chandrasekaran Lab at Boston University, and in other labs performing human psychophysics experiments at Boston University. We also anticipate using it in the Zimmerman Lab at the University of Minnesota. These experiments will further validate _openEyeTrack_ and allow other users to benefit from it.


### Software and Hardware components 

_openEyeTrack_ is a video based eye tracker that takes advantage of OpenCV [@openCVcomputerlibrary,@opencvmanual,@opencvlibrary], a low cost high speed infrared camera and Gige-V APIs for Linux provided by Teledyne DALSA [@GigeV], the graphical user interface toolkit QT5 [@qt5] and the OpenCV based gui elements [@cvui]. All of the software components can be downloaded for free. The only costs are from the hardware components such as the camera (Genie Nano M640 NIR, Teledyne DALSA, ~$450, ~730 frames per second) and infrared light source, an articulated arm to position the camera (Manfrotto: $130), a computer with one or more gigabit network interface cards, and a power over ethernet switch to power and receive data from the camera. 

By using the Gige-V Framework to capture the frames from the DALSA camera and the OpenCV simple blob detector, _openEyeTrack_ is able to accurately calculate the position and area of the pupil. We include pupil size calculations because of its putative link to arousal levels and emotions of the observer [@doi:10.1111/j.1469-8986.2007.00606.x]. 


### Multithreading provides improvements over existing open source solutions

_openEyeTrack_ is based on other open-source eye trackers currently available such as “Oculomatic” [@ZIMMERMANN2016138]. However, most of these programs are single threaded: the frames are captured, processed, and displayed sequentially, only executing the next stage once the previous stage has been completed. Although single threaded  methods have become more effective over the years, these stages are time consuming and can limit the overall performance. In order to increase performance _openEyeTrack_ was developed as a multithreaded application. The capture, display, data transmission, and most importantly, processing components all happen within their own separate threads. By incorporating multiple threads, the processing speed of the frames is able to match the frame capture rate of the camera, allowing for lossless processing of data.


### Algorithm

As depicted in **Figure 1** below, as frames transition between the capture, detection, and display stages, they are stored in queues which enable the separate stages to run independently and allow for asynchronous capture, detection, and display. Once the camera grabs a new frame, it is very briefly stored in the Genicam memory buffers before being extracted and packaged by the "capture thread" into a struct and stored in a queue. This approach allows for the sequence of acquisition frames to be preserved and the frame acquisition process to take place without being slowed down by the blob detection or displaying. The frames in the "capture queue" are then popped off by the “n” (user specified) detection thread(s). Each "detection thread" takes the data from the "capture queue," converts it into an OpenCV Mat object, applies the OpenCV blob detection algorithm, notes the key features. Each detection thread also outputs the position of the blob as text on the frame and  draws a circle around the blob. This process is very time consuming, which is why initializing multiple threads are recommended for higher performance. Once the detection stage has completed, the frames are stored in a "display queue" that the "display thread" will grab from to show the images. The detection threads also packages the frame and keypoints information into a struct object which is then stored in a "network queue". The "network thread" reads from this queue and sends out packets over a UDP socket for downstream applications.
 
### Performance 

Under the conditions at the time of development, frame acquisition frame rates of up to 715 fps and display rates of up to 145 fps were achieved. Although more  threads in theory should increase speed, four processing threads were sufficient to keep up with the camera. Performance improved when we used the gev_nettweak tool provided by Teledyne Dalsa, which adjusts various features for the network buffers allowing higher throughput transmission from the camera to the computer. Additionally, the environmental lighting significantly affects the speed at which the blob detection occurs. The OpenCV blob detector by default looks for black blobs and thus more light allows for easier detection by increasing the contrast between darker and lighter areas of the image. To facilitate the detection process, the images undergo a binary thresholding and the user can specify a region of interest for the blob detector to focus on. For eye tracking, it is necessary to have an infrared IR light source to increase the contrast between the pupil and the surrounding regions.

### Limitations

Our eye tracking solution is not meant to solve all gaze tracking issues which may be more readily available in commercial solutions. 

1. Our eye tracker cannot be used if the head is freely moving. In our approach, which only detects the pupil, head motion is confounded with pupil motion. One future solution is to use both the corneal reflection and the pupil to allow for head-free eye tracking. This will be implemented in future versions of _openEyeTrack_.

2. _openEyeTrack_ does not output signals to analog channels which is a typical feature of commercial eye trackers. These analog signals were proxies for the analog signals from scleral search coils used for eye tracking. 

3. Using _openEyeTrack_ requires knowledge of Linux and some degree of comfort with the command line to compile and install various components and thus it is not as seamless and polished as commercial solutions. On the other hand it provides open source code for eye-tracking.

_openEyeTrack_ is available on GitHub at [https://github.com/chand-lab/openEyeTrack](https://github.com/chand-lab/openEyeTrack) and a more detailed description of usage can be found under the README.md file located in the repository. 


-![Fidgit deposited in figshare.](openEyeTrack_Overview(1).png)
*Figure 1: A visual depiction of the overall software and hardware architecture in openEyeTrack.* 

### Acknowledgements

We would like to acknowledge Jeremy Casas for his invaluable feedback and support throughout this project. This work was supported by an NIH/NINDS Grant to CC (4R00NS092972-03) and startup funds provided by Boston University to CC.

### References
