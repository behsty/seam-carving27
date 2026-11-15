
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "energy.h"
#include "graphcut.h"

int main()
{
    // Hide informational probes for optional plugins; retain warnings and errors.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::string path = "assets/sky2.png";
    int numV = 50; // vertical seams to remove(image gets narrower)
    int numH = 0;   // horizontal seams to remove (image gets shorter)

    cv::Mat image = cv::imread(path);

    if (image.empty())
    {
        std::cout << "Failed to load image!" << std::endl;
        return 1;
    }
    cv::Mat energy = calculateEnergyMap(image);

    // Make a separate 8-bit preview for viewing.
    cv::Mat energyDisplay;
    cv::normalize(
        energy, energyDisplay,
        0, 255, cv::NORM_MINMAX, CV_8U
    );

    if (!cv::imwrite("energy.png", energyDisplay))
    {
        std::cerr << "Failed to save energy.png\n";
        return 1;
    }

    std::cout << "Saved energy.png\n";

    std::cout << "OpenCV setup successful!" << std::endl;
    std::cout << "Image width: " << image.cols << std::endl;
    std::cout << "Image height: " << image.rows << std::endl;

    // ---- Graph cut (min-cut / max-flow) seam carving ----
    std::cout << "\nGraph cut (Dinic): ";
    std::cout << "removing " << numV << " vertical + " << numH << " horizontal seams\n";

    // One-line progress bar with elapsed time and estimated time remaining.
    // (The ETA is a little pessimistic: seams get faster as the image shrinks.)
    auto showProgress = [](int done, int total, double elapsedMs)
        {
            auto fmt = [](double ms)
                {
                    int s = static_cast<int>(ms / 1000.0 + 0.5);
                    return std::to_string(s / 60) + "m" + (s % 60 < 10 ? "0" : "") +
                        std::to_string(s % 60) + "s";
                };
            const int width = 30;
            int filled = done * width / total;
            double eta = elapsedMs / done * (total - done);
            std::cout << "\r[" << std::string(filled, '#')
                << std::string(width - filled, '-') << "] "
                << done << "/" << total << " (" << done * 100 / total << "%)"
                << "  elapsed " << fmt(elapsedMs)
                << "  ETA " << fmt(eta) << "    " << std::flush;
            if (done == total)
                std::cout << "\n";
        };

    GraphCutResult res;
    try
    {
        res = seamCarveGraphCut(image, numV, numH, calculateEnergyMap, showProgress);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    bool ok1 = cv::imwrite("graphcut_result.png", res.image);
    bool ok2 = cv::imwrite("graphcut_seams.png", res.seamOverlay);
    if (!ok1) std::cerr << "Failed to save graphcut_result.png\n";
    if (!ok2) std::cerr << "Failed to save graphcut_seams.png\n";
    if (!ok1 || !ok2)
        return 1;

    std::cout << "Removed:       " << res.verticalRemoved << " vertical, "
        << res.horizontalRemoved << " horizontal seams\n";
    std::cout << "Output size:   " << res.image.cols << "x" << res.image.rows << "\n"
        << "Total time:    " << res.totalMs << " ms\n"
        << "Max-flow time: " << res.maxflowMs << " ms\n"
        << "Saved graphcut_result.png and graphcut_seams.png\n";

    return 0;
}
