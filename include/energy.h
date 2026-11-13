#pragma once

#include <opencv2/core.hpp>

// Returns a single-channel CV_64F energy map.
cv::Mat calculateEnergyMap(const cv::Mat& image);