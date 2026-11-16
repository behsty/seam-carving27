
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "dp.h"
#include "energy.h"
#include "graphcut.h"
#include "greedy.h"

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

    // ---- Run every method with the same energy function and carving loop ----
    struct Method
    {
        std::string name;     // printed label
        std::string prefix;   // output file prefix
        SeamFinder findV;
        SeamFinder findH;
    };

    double maxflowMs = 0.0;
    // Dynamic programming is first: it is the reference (optimal) method the
    // others are compared against below.
    std::vector<Method> methods = {
        { "Dynamic programming", "dp",
          findVerticalSeamDP, findHorizontalSeamDP },
        { "DP (compact memory)", "dp_compact",
          findVerticalSeamDPCompact, findHorizontalSeamDPCompact },
        { "Greedy", "greedy",
          findVerticalSeamGreedy, findHorizontalSeamGreedy },
        { "Graph cut (Dinic)", "graphcut",
          [&](const cv::Mat& e) { return findVerticalSeamGraphCut(e, &maxflowMs); },
          [&](const cv::Mat& e) { return findHorizontalSeamGraphCut(e, &maxflowMs); } },
    };

    std::vector<CarveResult> results;
    for (const Method& m : methods)
    {
        std::cout << "\n" << m.name << ": removing " << numV << " vertical + "
            << numH << " horizontal seams\n";

        CarveResult res;
        try
        {
            res = seamCarve(image, numV, numH, calculateEnergyMap,
                m.findV, m.findH, showProgress);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error: " << ex.what() << std::endl;
            return 1;
        }

        const std::string resultFile = m.prefix + "_result.png";
        const std::string seamsFile = m.prefix + "_seams.png";
        if (!cv::imwrite(resultFile, res.image) || !cv::imwrite(seamsFile, res.seamOverlay))
        {
            std::cerr << "Failed to save " << resultFile << " / " << seamsFile << "\n";
            return 1;
        }
        std::cout << "Saved " << resultFile << " and " << seamsFile << "\n";
        results.push_back(res);
    }

    // ---- Comparison table ----
    const int seams = numV + numH;
    std::cout << "\nOutput size: " << results[0].image.cols << "x"
        << results[0].image.rows << "\n\n"
        << std::left << std::setw(22) << "Method"
        << std::right << std::setw(12) << "total ms"
        << std::setw(12) << "energy ms"
        << std::setw(12) << "seam ms"
        << std::setw(14) << "ms/seam find"
        << std::setw(16) << "avg seam cost" << "\n"
        << std::string(88, '-') << "\n"
        << std::fixed << std::setprecision(2);
    for (size_t k = 0; k < methods.size(); ++k)
    {
        const CarveResult& r = results[k];
        std::cout << std::left << std::setw(22) << methods[k].name
            << std::right << std::setw(12) << r.totalMs
            << std::setw(12) << r.energyMs
            << std::setw(12) << r.seamMs
            << std::setw(14) << r.seamMs / seams
            << std::setw(16) << r.totalSeamCost / seams << "\n";
    }
    std::cout << "(graph cut: " << maxflowMs << " ms of its seam time was inside max-flow)\n";

    // DP and graph cut both find the optimal seam, so on the same energy map
    // graph cut should match DP and greedy can never be cheaper. (Totals can
    // still drift slightly over many seams: when two seams tie, the methods
    // may pick different ones, and the images then diverge.)
    const double optCost = results[0].totalSeamCost;
    if (optCost > 0)
    {
        for (size_t k = 1; k < methods.size(); ++k)
        {
            std::cout << methods[k].name << " seam cost vs DP (optimal): "
                << std::showpos << (results[k].totalSeamCost / optCost - 1.0) * 100.0
                << std::noshowpos << "%\n";
        }
    }

    return 0;
}
