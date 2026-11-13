
#include <iostream>
#include <opencv2/opencv.hpp>

int main()
{
    cv::Mat image = cv::imread("assets/test.png");

    if (image.empty())
    {
        std::cout << "Failed to load image!" << std::endl;
        return 1;
    }

    std::cout << "OpenCV setup successful!" << std::endl;
    std::cout << "Image width: " << image.cols << std::endl;
    std::cout << "Image height: " << image.rows << std::endl;

    return 0;
}
