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

// ---------------------------------------------------------------------------
// Memory-reduced DP ("do we need to maintain the entire DP table?")
//
// Row i of M only depends on row i-1, so the costs need just two rows of
// n doubles that are swapped each step. To backtrack we still need to know,
// for every pixel, which parent gave the minimum - but that is one of only
// 3 choices (-1, 0, +1), so it fits in a 1-byte int8 instead of an 8-byte
// double.
//
//   full table:  m*n doubles               = 8*m*n bytes
//   compact:     2*n doubles + m*n int8    = m*n + 16*n bytes  (~8x less)
//
// Time is still O(m * n). Ties are broken exactly as in findVerticalSeamDP,
// so both functions return the identical seam.
// ---------------------------------------------------------------------------
std::vector<int> findVerticalSeamDPCompact(const cv::Mat& energy);
std::vector<int> findHorizontalSeamDPCompact(const cv::Mat& energy);

// ---------------------------------------------------------------------------
// Row-parallel DP ("does multithreading help?")
//
// Rows must be computed in order (row i needs row i-1), but the n cells
// within one row are independent of each other, so each row is split
// between threads (OpenMP), with a barrier before the next row.
//
//   work:  O(m * n)      (unchanged)
//   span:  O(m * n / p + m * sync)   with p threads
//
// The barrier after every row is the cost: with narrow images the per-row
// work is too small to pay for it, so rows narrower than a threshold are
// done by one thread. Seams themselves cannot be found in parallel: each
// seam depends on the image left after removing the previous one.
//
// Memory layout and tie-breaking are the same as the compact finder, so the
// seam is identical to findVerticalSeamDP. Built without OpenMP, it simply
// runs serially.
// ---------------------------------------------------------------------------
std::vector<int> findVerticalSeamDPParallel(const cv::Mat& energy);
std::vector<int> findHorizontalSeamDPParallel(const cv::Mat& energy);

// Number of threads the parallel finder will use (1 without OpenMP).
int dpParallelThreads();
