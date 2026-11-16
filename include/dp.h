#pragma once

// Dynamic-programming seam finding (optimal).
//
// Vertical seam on energy e (m x n), rows i = 0..m-1, columns j = 0..n-1:
//
//   M[0][j] = e[0][j]
//   M[i][j] = e[i][j] + min( M[i-1][j-1], M[i-1][j], M[i-1][j+1] )
//             (neighbours outside the image are ignored)
//
// M[i][j] is the cost of the cheapest seam that runs from the top row down
// to pixel (i, j). Optimal substructure: the cheapest seam ending at (i, j)
// must reach (i, j) from one of its 3 parents in row i-1, and the part of
// it above row i must itself be the cheapest seam ending at that parent.
//
// The optimal seam ends at the minimum of the last row; it is recovered by
// walking back up, each time moving to the parent that gave the minimum.
//
// Time O(m * n) per seam, space O(m * n) for the table M.

#include <opencv2/core.hpp>
#include <vector>

// energy: CV_64F, m x n. Returns seam[i] = column to remove in row i.
std::vector<int> findVerticalSeamDP(const cv::Mat& energy);

// Same on the transposed energy. Returns seam[j] = row to remove in column j.
std::vector<int> findHorizontalSeamDP(const cv::Mat& energy);
