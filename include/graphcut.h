#pragma once

// Graph-cut (min-cut / max-flow) seam carving, plus the generic carving loop
// shared with the other seam-finding methods (e.g. greedy).
//
// The carving loop is identical for every method:
//     energy map -> find seam -> remove seam -> repeat
// Only the seam finder changes, which keeps the comparison fair.
//
// The energy map is supplied by the caller (e.g. calculateEnergyMap from
// energy.h), so this module only needs OpenCV core.

#include <functional>
#include <opencv2/core.hpp>
#include <vector>

// Computes an energy map (CV_64F, same size as the image) for a BGR image.
using EnergyFn = std::function<cv::Mat(const cv::Mat&)>;

// Called after every removed seam: seams done so far, total seams planned
// and elapsed milliseconds.
using ProgressFn = std::function<void(int done, int total, double elapsedMs)>;

// Finds one seam in an energy map (CV_64F, m x n).
// Vertical finder:   returns seam[i] = column to remove in row i    (size m).
// Horizontal finder: returns seam[j] = row to remove in column j    (size n).
using SeamFinder = std::function<std::vector<int>(const cv::Mat& energy)>;

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

// Generic seam removal; works for any Mat type (BGR image, index maps, ...).
cv::Mat removeVerticalSeam(const cv::Mat& src, const std::vector<int>& seam);
cv::Mat removeHorizontalSeam(const cv::Mat& src, const std::vector<int>& seam);

// Sum of energy along a seam (the "disruption measure" of the seam).
double verticalSeamCost(const cv::Mat& energy, const std::vector<int>& seam);
double horizontalSeamCost(const cv::Mat& energy, const std::vector<int>& seam);

struct CarveResult
{
    cv::Mat image;          // resized image
    cv::Mat seamOverlay;    // original image with every removed seam drawn in red
    double totalMs = 0.0;   // whole carving loop
    double energyMs = 0.0;  // time spent computing energy maps
    double seamMs = 0.0;    // time spent inside the seam finder
    double totalSeamCost = 0.0; // sum of the energy of every removed seam
    int verticalRemoved = 0;
    int horizontalRemoved = 0;
};

// Removes numVertical vertical seams, then numHorizontal horizontal seams,
// recomputing the energy after every removal. Works with any seam finder.
CarveResult seamCarve(
    const cv::Mat& image, int numVertical, int numHorizontal,
    const EnergyFn& energyFn,
    const SeamFinder& findVertical,
    const SeamFinder& findHorizontal,
    const ProgressFn& progress = nullptr);

struct GraphCutResult : CarveResult
{
    double maxflowMs = 0.0; // time spent only inside max-flow
};

// seamCarve with the graph-cut seam finders.
GraphCutResult seamCarveGraphCut(
    const cv::Mat& image, int numVertical, int numHorizontal,
    const EnergyFn& energyFn,
    const ProgressFn& progress = nullptr);
