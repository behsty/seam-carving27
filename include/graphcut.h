#pragma once

// Graph-cut (min-cut / max-flow) seam finding.
//
// The finders match SeamFinder in seam.h, so they plug into the shared
// carving loop (seamCarve / carveToSize) like the DP and greedy finders.

#include <opencv2/core.hpp>
#include <vector>

// |I[i,j+1] - I[i,j-1]| on the grayscale image (borders clamped). CV_64F.
cv::Mat computeForwardCost(const cv::Mat& image);

// Finds the optimal vertical seam for a per-pixel removal cost matrix (CV_64F,
// m x n, n >= 2) by building the s-t graph and solving min-cut.
// Returns seam[i] = column of the pixel to remove in row i.
// If maxflowMs is given, the time spent inside max-flow is added to it.
std::vector<int> findVerticalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs = nullptr);

// Same for a horizontal seam (done on the transposed cost matrix).
// Returns seam[j] = row of the pixel to remove in column j.
std::vector<int> findHorizontalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs = nullptr);
