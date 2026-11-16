

#include "seam.h"

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
    using Clock = std::chrono::steady_clock;

    double msSince(Clock::time_point t0)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    }
}

// ---------------------------------------------------------------------------
// Seam removal
// ---------------------------------------------------------------------------
cv::Mat removeVerticalSeam(const cv::Mat& src, const std::vector<int>& seam)
{
    CV_Assert(static_cast<int>(seam.size()) == src.rows);
    cv::Mat dst(src.rows, src.cols - 1, src.type());
    const size_t es = src.elemSize();
    for (int i = 0; i < src.rows; ++i)
    {
        const uchar* s = src.ptr<uchar>(i);
        uchar* d = dst.ptr<uchar>(i);
        const int k = seam[i];
        std::memcpy(d, s, static_cast<size_t>(k) * es);
        std::memcpy(d + static_cast<size_t>(k) * es,
            s + static_cast<size_t>(k + 1) * es,
            static_cast<size_t>(src.cols - k - 1) * es);
    }
    return dst;
}

cv::Mat removeHorizontalSeam(const cv::Mat& src, const std::vector<int>& seam)
{
    cv::Mat t, r, out;
    cv::transpose(src, t);
    r = removeVerticalSeam(t, seam);
    cv::transpose(r, out);
    return out;
}

// ---------------------------------------------------------------------------
// Seam cost
// ---------------------------------------------------------------------------
double verticalSeamCost(const cv::Mat& energy, const std::vector<int>& seam)
{
    double sum = 0.0;
    for (int i = 0; i < energy.rows; ++i)
        sum += energy.at<double>(i, seam[i]);
    return sum;
}

double horizontalSeamCost(const cv::Mat& energy, const std::vector<int>& seam)
{
    double sum = 0.0;
    for (int j = 0; j < energy.cols; ++j)
        sum += energy.at<double>(seam[j], j);
    return sum;
}

// ---------------------------------------------------------------------------
// Generic carving loop (any seam finder)
// ---------------------------------------------------------------------------
CarveResult seamCarve(
    const cv::Mat& image, int numVertical, int numHorizontal,
    const EnergyFn& energyFn,
    const SeamFinder& findVertical,
    const SeamFinder& findHorizontal,
    const ProgressFn& progress,
    const StepFn& onStep)
{
    if (numVertical < 0 || numHorizontal < 0 ||
        numVertical >= image.cols || numHorizontal >= image.rows)
        throw std::runtime_error("Invalid number of seams to remove");

    CarveResult res;
    res.seamOverlay = image.clone();
    const cv::Vec3b red(0, 0, 255);

    // origPos(i,j) = (x,y) in the ORIGINAL image of the pixel now at (i,j).
    // Lets us draw every seam on the original picture.
    cv::Mat origPos(image.rows, image.cols, CV_32SC2);
    for (int y = 0; y < image.rows; ++y)
        for (int x = 0; x < image.cols; ++x)
            origPos.at<cv::Vec2i>(y, x) = cv::Vec2i(x, y);

    cv::Mat cur = image.clone();
    const int total = numVertical + numHorizontal;
    auto t0 = Clock::now();
    double stepMs = 0.0; // time inside onStep, excluded from the timings

    auto step = [&](const std::vector<int>& seam, bool vertical)
        {
            if (!onStep)
                return;
            auto t = Clock::now();
            onStep(cur, seam, vertical);
            stepMs += msSince(t);
        };

    auto timedEnergy = [&](const cv::Mat& img)
        {
            auto t = Clock::now();
            cv::Mat e = energyFn(img);
            res.energyMs += msSince(t);
            return e;
        };

    auto timedFind = [&](const SeamFinder& finder, const cv::Mat& energy)
        {
            auto t = Clock::now();
            std::vector<int> seam = finder(energy);
            res.seamMs += msSince(t);
            return seam;
        };

    auto mark = [&](int row, int col)
        {
            cv::Vec2i p = origPos.at<cv::Vec2i>(row, col);
            res.seamOverlay.at<cv::Vec3b>(p[1], p[0]) = red;
        };

    auto report = [&]()
        {
            if (progress)
                progress(res.verticalRemoved + res.horizontalRemoved, total, msSince(t0) - stepMs);
        };

    for (int k = 0; k < numVertical; ++k)
    {
        cv::Mat energy = timedEnergy(cur);
        std::vector<int> seam = timedFind(findVertical, energy);
        step(seam, true);
        res.totalSeamCost += verticalSeamCost(energy, seam);
        for (int i = 0; i < cur.rows; ++i)
            mark(i, seam[i]);
        cur = removeVerticalSeam(cur, seam);
        origPos = removeVerticalSeam(origPos, seam);
        ++res.verticalRemoved;
        report();
    }

    for (int k = 0; k < numHorizontal; ++k)
    {
        cv::Mat energy = timedEnergy(cur);
        std::vector<int> seam = timedFind(findHorizontal, energy);
        step(seam, false);
        res.totalSeamCost += horizontalSeamCost(energy, seam);
        for (int j = 0; j < cur.cols; ++j)
            mark(seam[j], j);
        cur = removeHorizontalSeam(cur, seam);
        origPos = removeHorizontalSeam(origPos, seam);
        ++res.horizontalRemoved;
        report();
    }

    res.image = cur;
    res.totalMs = msSince(t0) - stepMs;
    return res;
}


// ---------------------------------------------------------------------------
// Resize to a target size (any seam finder)
// ---------------------------------------------------------------------------
CarveResult carveToSize(
    const cv::Mat& image, int targetWidth, int targetHeight,
    const EnergyFn& energyFn,
    const SeamFinder& findVertical,
    const SeamFinder& findHorizontal,
    const ProgressFn& progress,
    const StepFn& onStep)
{
    if (targetWidth < 1 || targetHeight < 1 ||
        targetWidth > image.cols || targetHeight > image.rows)
        throw std::runtime_error(
            "Target size must be between 1x1 and the original size ("
            + std::to_string(image.cols) + "x" + std::to_string(image.rows) + ")");

    return seamCarve(image,
        image.cols - targetWidth,   // vertical seams make it narrower
        image.rows - targetHeight,  // horizontal seams make it shorter
        energyFn, findVertical, findHorizontal, progress, onStep);
}
