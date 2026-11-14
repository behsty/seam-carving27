
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "energy.h"
#include "graphcut.h"

int main()
{
    // Hide informational probes for optional plugins; retain warnings and errors.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::string path = "assets/sky2.png";

    cv::Mat image = cv::imread(path);

    if (image.empty())
    {
        std::cout << "Failed to load image!" << std::endl;
        return 1;
    }
    cv::Mat energy = calculateEnergyMap(image);

    // Make a separate 8-bit preview for viewing.
    cv::Mat energyDisplay;
    cv::normalize(
        energy, energyDisplay,
        0, 255, cv::NORM_MINMAX, CV_8U
    );

    if (!cv::imwrite("energy.png", energyDisplay))
    {
        std::cerr << "Failed to save energy.png\n";
        return 1;
    }

    std::cout << "Saved energy.png\n";

    std::cout << "OpenCV setup successful!" << std::endl;
    std::cout << "Image width: " << image.cols << std::endl;
    std::cout << "Image height: " << image.rows << std::endl;

    return 0;
}
