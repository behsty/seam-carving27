#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "dp.h"
#include "energy.h"
#include "graphcut.h"
#include "greedy.h"
#include "interactive.h"
#include "seam.h"
#include "visualize.h"

int main(int argc, char** argv)
{
    // Hide informational probes for optional plugins; retain warnings and errors.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    // ---- Options ----
    //   --image <file>             image to carve (default assets/sky2.png)
    //   --width <W> --height <H>   target size (default: 50 columns narrower)
    //   --interactive              setup window with width/height/method
    //                              sliders; Enter carves (bonus: user interaction)
    //   --visualize   show each method's seams being removed, step by step
    //   --video       save each method's steps as <method>_steps.mp4 (or .avi)
    //   --delay <ms>  pause per seam in the window (default 1)
    // Without --visualize/--video nothing is drawn, so timings stay representative.
    std::string path = "assets/sky2.png";
    int targetW = -1; // -1 = keep original width
    int targetH = -1; // -1 = keep original height
    bool interactive = false;
    bool visualize = false;
    bool saveVideo = false;
    int delayMs = 1;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--interactive")
            interactive = true;
        else if (arg == "--visualize")
            visualize = true;
        else if (arg == "--video")
            saveVideo = true;
        else if (arg == "--delay" && i + 1 < argc)
            delayMs = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--image" && i + 1 < argc)
            path = argv[++i];
        else if (arg == "--width" && i + 1 < argc)
            targetW = std::atoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc)
            targetH = std::atoi(argv[++i]);
        else
        {
            std::cerr << "Unknown option: " << arg << "\n"
                << "Usage: seam_carving [--image file] [--width W] [--height H] [--interactive]\n"
                << "                    [--visualize] [--video] [--delay ms]\n";
            return 1;
        }
    }

    int numV = 50; // vertical seams to remove (image gets narrower)
    int numH = 0;  // horizontal seams to remove (image gets shorter)

    cv::Mat image = cv::imread(path);

    if (image.empty())
    {
        std::cout << "Failed to load image: " << path << std::endl;
        return 1;
    }

    // Target size -> number of seams. Without --width/--height, keep the
    // default of 50 vertical seams.
    if (!interactive && (targetW != -1 || targetH != -1))
    {
        const int w = targetW == -1 ? image.cols : targetW;
        const int h = targetH == -1 ? image.rows : targetH;
        if (w < 1 || h < 1 || w > image.cols || h > image.rows)
        {
            std::cerr << "Target size must be between 1x1 and the original size ("
                << image.cols << "x" << image.rows << ")\n";
            return 1;
        }
        if (w == image.cols && h == image.rows)
        {
            std::cerr << "Target size equals the original size: nothing to carve\n";
            return 1;
        }
        numV = image.cols - w;
        numH = image.rows - h;
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

    double maxflowMs = 0.0;
    // Dynamic programming is first: it is the reference (optimal) method the
    // others are compared against below.
    std::vector<SeamMethod> methods = {
        { "Dynamic programming", "dp",
          findVerticalSeamDP, findHorizontalSeamDP },
        { "DP (compact memory)", "dp_compact",
          findVerticalSeamDPCompact, findHorizontalSeamDPCompact },
        { "DP (parallel rows)", "dp_parallel",
          findVerticalSeamDPParallel, findHorizontalSeamDPParallel },
        { "Greedy", "greedy",
          findVerticalSeamGreedy, findHorizontalSeamGreedy },
        { "Graph cut", "graphcut",
          [&](const cv::Mat& e) { return findVerticalSeamGraphCut(e, &maxflowMs); },
          [&](const cv::Mat& e) { return findHorizontalSeamGraphCut(e, &maxflowMs); } },
    };

    // Interactive mode: sliders first, carve on Enter (no comparison table).
    if (interactive)
        return runInteractive(image, calculateEnergyMap, methods, delayMs,
            targetW, targetH);

    std::vector<CarveResult> results;
    for (const SeamMethod& m : methods)
    {
        std::cout << "\n" << m.name << ": removing " << numV << " vertical + "
            << numH << " horizontal seams\n";

        // Optional step-by-step visualization (off by default).
        std::unique_ptr<SeamVisualizer> vis;
        StepFn onStep;
        if (visualize || saveVideo)
        {
            vis = std::make_unique<SeamVisualizer>(m.name, image.size(), numV + numH,
                visualize, delayMs, saveVideo ? m.prefix + "_steps.mp4" : "");
            onStep = [&vis](const cv::Mat& img, const std::vector<int>& seam, bool vertical)
                { (*vis)(img, seam, vertical); };
        }

        CarveResult res;
        try
        {
            res = seamCarve(image, numV, numH, calculateEnergyMap,
                m.findV, m.findH, showProgress, onStep);
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
        if (vis)
        {
            vis->showFinal(res.image);
            const std::string video = vis->finish();
            if (!video.empty())
                std::cout << "Saved " << video << "\n";
        }
        results.push_back(res);
    }

    // ---- Comparison table ----
    const int seams = numV + numH;
#ifdef NDEBUG
    std::cout << "\nBuild: Release (timings are representative)\n";
#else
    std::cout << "\nBuild: Debug (timings are NOT representative; "
        "use the windows-release preset for reported numbers)\n";
#endif
    std::cout << "DP (parallel rows) uses " << dpParallelThreads() << " thread(s)\n";
    std::cout << "Output size: " << results[0].image.cols << "x"
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
