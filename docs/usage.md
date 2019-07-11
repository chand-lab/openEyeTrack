### Usage

- Open up terminal and navigate to the directory where you want the repository to be cloned to
- Clone the repository from git using: $git clone https://github.com/mailchand/openEyeTrack.git
- Open up the makefile and check that the paths found on lines 5 and 31 (start with “IROOT” and “include” are correct)
- Type in: $ make 
  - If there are errors it is likely due to a linking libraries issue
- Type in: $ ./openEyeTrack
- If there are no errors so far, your screen should look something like this:
 ![ScreenShot](/docs/README1.png)


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
You can also play around with the number of buffers (defined as NUM_BUFF) that the camera initially stores the frames in but 4 seems to be a good number and no noticeable advantages have been found from increasing this value

### Notes
- You can set DEBUG_STATEMENTS to 1 (found near the top of the code) in order to display features such as frame rates and queue lengths
- There is an option to use sychronous vs. asychronous buffer cycling denoted by USE_SYNCHRONOUS_BUFFER_CYCLING, program seems to work beter with sychronous mode so keep value set at 1
- Occasionally you may run into a “bus error” or “segmentation fault” upon start up but this is easily fixed by spamming Crtl+C and waiting a few seconds until the light on the camera turns blue
(usually caused by closing and trying to reopen the camera too quickly)
 - Only the most recent frames are actually shown in the display window as displaying images as a costly process, however all the frames are still being sent out through the UDP socket
 - When adjusting the gate values for the roi in the control panel, keep in mind that the top left corner of the image is considered (0,0)
