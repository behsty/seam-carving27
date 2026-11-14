#pragma once

// Graph-cut (min-cut / max-flow) seam carving.
//
// Self-contained: it only needs OpenCV core. The energy map is supplied by the
// caller (e.g. calculateEnergyMap from energy.h), so this module does not
// depend on any other project file.

#include <functional>
#include <opencv2/core.hpp>
#include <vector>

// Computes an energy map (CV_64F, same size as the image) for a BGR image.
using EnergyFn = std::function<cv::Mat(const cv::Mat&)>;

// Called after every removed seam: seams done so far, total seams planned
// and elapsed milliseconds.
using ProgressFn = std::function<void(int done, int total, double elapsedMs)>;

// |I[i,j+1] - I[i,j-1]| on the grayscale image (borders clamped). CV_64F.
cv::Mat computeForwardCost(const cv::Mat& image);

// Finds the optimal vertical seam for a per-pixel removal cost matrix (CV_64F,
// m x n, n >= 2) by building the s-t graph and solving min-cut.
// Returns seam[i] = column of the pixel to remove in row i.
std::vector<int> findVerticalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs = nullptr);

// Same for a horizontal seam (done on the transposed cost matrix).
// Returns seam[j] = row of the pixel to remove in column j.
std::vector<int> findHorizontalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs = nullptr);

