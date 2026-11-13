#include "energy.h"
#include <opencv2/imgproc.hpp>

cv::Mat calculateEnergyMap(const cv::Mat& image)
{
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    cv::Mat gradientX;
    cv::Mat gradientY;

    cv::Sobel(gray, gradientX, CV_64F, 1, 0, 3);
    cv::Sobel(gray, gradientY, CV_64F, 0, 1, 3);

    cv::Mat energy;
    cv::magnitude(gradientX, gradientY, energy);

    return energy;
}