#pragma once

// Seam removal and resizing, shared by every seam-finding method.
//
// The carving loop is identical for every method:
//     energy map -> find seam -> remove seam -> repeat
// Only the seam finder changes (DP, greedy, graph cut), which keeps the
// comparison fair. The algorithm modules do not include this header; they
// just provide functions that match SeamFinder, and main wires them in.

#include <functional>
#include <string>
#include <opencv2/core.hpp>
#include <vector>

// Computes an energy map (CV_64F, same size as the image) for a BGR image.
using EnergyFn = std::function<cv::Mat(const cv::Mat&)>;

// Called after every removed seam: seams done so far, total seams planned
// and elapsed milliseconds.
using ProgressFn = std::function<void(int done, int total, double elapsedMs)>;

// Called once per seam, just BEFORE it is removed, with the current image and
// the seam (vertical = true for a vertical seam). Used for step-by-step
// visualization; it only looks at the image and cannot change the result.
// Time spent inside it is not counted in CarveResult::totalMs.
using StepFn = std::function<void(const cv::Mat& image,
    const std::vector<int>& seam, bool vertical)>;

// Finds one seam in an energy map (CV_64F, m x n).
// Vertical finder:   returns seam[i] = column to remove in row i    (size m).
// Horizontal finder: returns seam[j] = row to remove in column j    (size n).
using SeamFinder = std::function<std::vector<int>(const cv::Mat& energy)>;

// A named seam-finding method (e.g. DP, greedy, graph cut): a vertical and a
// horizontal finder. Used by both the auto run and interactive mode.
struct SeamMethod
{
    std::string name;     // printed label
    std::string prefix;   // output file prefix
    SeamFinder findV;
    SeamFinder findH;
};

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
    const ProgressFn& progress = nullptr,
    const StepFn& onStep = nullptr);

// Resizes the image to targetWidth x targetHeight by seam removal
// (vertical seams first, then horizontal). The target must be at least 1x1
// and no larger than the original. Used by the CLI (--width/--height) and
// the manual-mode sliders.
CarveResult carveToSize(
    const cv::Mat& image, int targetWidth, int targetHeight,
    const EnergyFn& energyFn,
    const SeamFinder& findVertical,
    const SeamFinder& findHorizontal,
    const ProgressFn& progress = nullptr,
    const StepFn& onStep = nullptr);
