#pragma once

// Dynamic-programming seam finding (optimal).
//
// M[i][j] = cheapest seam cost from the top row to pixel (i, j):
//   M[0][j] = e[0][j]
//   M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])
//
// The seam ends at the minimum of the last row; backtrack through the
// parent that gave each minimum. Time and space O(m * n) per seam.

#include <opencv2/core.hpp>
#include <vector>

// energy: CV_64F, m x n. Returns seam[i] = column to remove in row i.
std::vector<int> findVerticalSeamDP(const cv::Mat& energy);

// Runs on the transposed energy. Returns seam[j] = row to remove in column j.
std::vector<int> findHorizontalSeamDP(const cv::Mat& energy);

// Compact DP: keeps only 2 cost rows plus a 1-byte parent offset (-1/0/+1)
// per pixel. Memory ~m*n bytes instead of 8*m*n; same time, same seam.
std::vector<int> findVerticalSeamDPCompact(const cv::Mat& energy);
std::vector<int> findHorizontalSeamDPCompact(const cv::Mat& energy);

// Parallel DP: rows are computed in order, but each row's cells are split
// across threads (OpenMP). Rows under 512 columns stay single-threaded,
// since the per-row sync costs more than it saves. Same seam as above;
// runs serially without OpenMP.
std::vector<int> findVerticalSeamDPParallel(const cv::Mat& energy);
std::vector<int> findHorizontalSeamDPParallel(const cv::Mat& energy);

// Threads used by the parallel finder (1 without OpenMP).
int dpParallelThreads();
