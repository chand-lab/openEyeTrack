#include <opencv2/opencv.hpp>
using namespace cv;

Mat3b rgb;
Mat3b hsv;

void on_trackbar(int hue, void*)
{
    hsv.setTo(Scalar(hue, 255, 255));
    cvtColor(hsv, rgb, COLOR_HSV2BGR);
    imshow("HSV", rgb);
}

int main(int argc, char** argv)
{
    // Init images
    rgb = Mat3b(100, 300, Vec3b(0,0,0));
    cvtColor(rgb, hsv, COLOR_BGR2HSV);

    /// Initialize values
    int sliderValue = 0;

    /// Create Windows
    namedWindow("HSV", 1);

    /// Create Trackbars
    createTrackbar("Hue", "HSV", &sliderValue, 180, on_trackbar);

    /// Show some stuff
    on_trackbar(sliderValue, NULL);

    /// Wait until user press some key
    waitKey(0);
    return 0;
}