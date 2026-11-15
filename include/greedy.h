#pragma once

// Greedy seam finding (not guaranteed optimal).
//
// Vertical seam on energy e (m x n):
//   1. Start at the lowest-energy pixel of the top row.
//   2. In each following row, move to the lowest-energy pixel among the
//      3 below the current one (j-1, j, j+1), ties going straight down.
// Only local information is used, so the seam can walk into a high-energy
// region that dynamic programming would have routed around.
// Time O(n + m) per seam, O(1) extra space besides the seam itself.

#include <opencv2/core.hpp>
#include <vector>

// energy: CV_64F, m x n. Returns seam[i] = column to remove in row i.
std::vector<int> findVerticalSeamGreedy(const cv::Mat& energy);

// Same on the transposed energy. Returns seam[j] = row to remove in column j.
std::vector<int> findHorizontalSeamGreedy(const cv::Mat& energy);
