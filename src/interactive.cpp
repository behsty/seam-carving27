#include "interactive.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "visualize.h"

namespace
{
    const std::string WINDOW = "Seam carving";

    bool windowClosed()
    {
        return cv::getWindowProperty(WINDOW, cv::WND_PROP_VISIBLE) < 1.0;
    }

    // Setup screen: original image, everything outside the target size
    // dimmed, and a yellow outline of the target size.
    cv::Mat setupFrame(const cv::Mat& image, double scale, int w, int h,
        const SeamMethod& method)
    {
        const std::string label = "Target " + std::to_string(w) + "x"
            + std::to_string(h) + "  |  " + method.name
            + "  |  Enter: carve   Esc: quit";
        cv::Mat frame = makeDisplayFrame(image, image.size(), scale, label);

        cv::Mat area = frame(cv::Rect(0, LABEL_BAR_HEIGHT, frame.cols,
            frame.rows - LABEL_BAR_HEIGHT));
        const cv::Rect target(0, 0, std::max(1, cvRound(w * scale)),
            std::max(1, cvRound(h * scale)));
        cv::Mat bright = area(target).clone();
        area *= 0.35;                 // dim everything...
        bright.copyTo(area(target));  // ...except the target region
        cv::rectangle(area, target, cv::Scalar(0, 255, 255), 2);
        return frame;
    }
}

int runInteractive(const cv::Mat& image, const EnergyFn& energyFn,
    const std::vector<SeamMethod>& methods, int delayMs,
    int initialWidth, int initialHeight)
{
    const double scale = displayScale(image.size());

    cv::namedWindow(WINDOW, cv::WINDOW_AUTOSIZE);
    cv::createTrackbar("Width", WINDOW, nullptr, image.cols);
    cv::createTrackbar("Height", WINDOW, nullptr, image.rows);
    cv::createTrackbar("Method", WINDOW, nullptr, static_cast<int>(methods.size()) - 1);
    cv::setTrackbarPos("Width", WINDOW,
        initialWidth > 0 ? std::min(initialWidth, image.cols) : image.cols);
    cv::setTrackbarPos("Height", WINDOW,
        initialHeight > 0 ? std::min(initialHeight, image.rows) : image.rows);
    cv::setTrackbarPos("Method", WINDOW, 0);

    std::cout << "\nInteractive mode: drag the sliders, press Enter to carve, Esc to quit.\n"
        << "Methods: ";
    for (size_t k = 0; k < methods.size(); ++k)
        std::cout << k << " = " << methods[k].name << (k + 1 < methods.size() ? ", " : "\n");

    while (true)
    {
        // Sliders start at 0, but a 0-pixel target is not allowed.
        const int w = std::max(1, cv::getTrackbarPos("Width", WINDOW));
        const int h = std::max(1, cv::getTrackbarPos("Height", WINDOW));
        const int m = std::clamp(cv::getTrackbarPos("Method", WINDOW), 0,
            static_cast<int>(methods.size()) - 1);
        const SeamMethod& method = methods[m];

        cv::imshow(WINDOW, setupFrame(image, scale, w, h, method));
        const int key = cv::waitKey(30);
        if (key == 27 || windowClosed())
            break;
        if (key != 13 && key != 10) // Enter
            continue;

        if (w == image.cols && h == image.rows)
        {
            std::cout << "Target equals the original size: move a slider first.\n";
            continue;
        }

        const int seams = (image.cols - w) + (image.rows - h);
        std::cout << "\n" << method.name << ": " << image.cols << "x" << image.rows
            << " -> " << w << "x" << h << " (" << seams << " seams)\n";

        SeamVisualizer vis(method.name, image.size(), seams, true, delayMs, "",
            30.0, WINDOW);
        CarveResult res = carveToSize(image, w, h, energyFn, method.findV, method.findH,
            nullptr,
            [&vis](const cv::Mat& img, const std::vector<int>& seam, bool vertical)
            { vis(img, seam, vertical); });

        std::cout << std::fixed << std::setprecision(2)
            << "  time " << res.totalMs << " ms, avg seam cost "
            << res.totalSeamCost / seams << "\n";
        if (cv::imwrite("interactive_result.png", res.image) &&
            cv::imwrite("interactive_seams.png", res.seamOverlay))
            std::cout << "  saved interactive_result.png and interactive_seams.png\n";

        vis.showFinal(res.image); // waits for a key, then back to the sliders
        vis.finish();
        if (windowClosed())
            break;
    }

    cv::destroyAllWindows();
    return 0;
}
