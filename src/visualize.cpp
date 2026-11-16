#include "visualize.h"

#include <algorithm>
#include <iostream>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
    const cv::Scalar RED(0, 0, 255); // BGR
}

double displayScale(cv::Size size)
{
    const int w = std::max(1, size.width);
    const int h = std::max(1, size.height);
    double s = std::clamp(600 / w, 1, 4);            // enlarge small images
    return std::min({ s, 1280.0 / w, 720.0 / h });  // shrink large ones
}

cv::Mat makeDisplayFrame(const cv::Mat& image, cv::Size canvasSize, double scale,
    const std::string& label)
{
    cv::Mat bgr;
    if (image.channels() == 1)
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    else
        bgr = image;

    // Black canvas the size of the original image, plus a label bar on top.
    cv::Mat frame(cvRound(canvasSize.height * scale) + LABEL_BAR_HEIGHT,
        cvRound(canvasSize.width * scale), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat scaled;
    cv::resize(bgr, scaled,
        cv::Size(std::min(cvRound(bgr.cols * scale), frame.cols),
                 std::min(cvRound(bgr.rows * scale), frame.rows - LABEL_BAR_HEIGHT)),
        0, 0, scale >= 1.0 ? cv::INTER_NEAREST : cv::INTER_AREA);
    scaled.copyTo(frame(cv::Rect(0, LABEL_BAR_HEIGHT, scaled.cols, scaled.rows)));

    // Shrink the label until it fits the frame width.
    double fontScale = 0.55;
    int baseline = 0;
    while (fontScale > 0.3 &&
        cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseline).width
            > frame.cols - 16)
        fontScale -= 0.05;
    cv::putText(frame, label, cv::Point(8, LABEL_BAR_HEIGHT - 9),
        cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    return frame;
}

SeamVisualizer::SeamVisualizer(const std::string& title, cv::Size canvasSize,
    int totalSeams, bool showWindow, int delayMs, const std::string& videoPath,
    double fps, const std::string& windowName)
    : title_(title), window_(windowName.empty() ? title : windowName),
      ownsWindow_(windowName.empty()), canvas_(canvasSize), totalSeams_(totalSeams),
      showWindow_(showWindow), delayMs_(std::max(1, delayMs)),
      scale_(displayScale(canvasSize)), fps_(fps), videoPath_(videoPath)
{
    if (showWindow_ && ownsWindow_)
        cv::namedWindow(window_, cv::WINDOW_AUTOSIZE);
}

SeamVisualizer::~SeamVisualizer()
{
    finish();
}

void SeamVisualizer::emit(const cv::Mat& frame, int waitMs)
{
    if (!videoPath_.empty())
    {
        if (!triedOpen_)
        {
            triedOpen_ = true;
            writer_.open(videoPath_, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                fps_, frame.size());
            if (!writer_.isOpened())
            {
                // MP4 codec not available in this OpenCV build: use OpenCV's
                // built-in Motion-JPEG AVI writer, which always works.
                std::string avi = videoPath_.substr(0, videoPath_.find_last_of('.')) + ".avi";
                writer_.open(avi, cv::CAP_OPENCV_MJPEG,
                    cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps_, frame.size());
                videoPath_ = writer_.isOpened() ? avi : "";
                if (videoPath_.empty())
                    std::cerr << "\nWarning: could not open a video writer\n";
            }
        }
        if (writer_.isOpened())
            writer_.write(frame);
    }

    if (showWindow_ && !skip_)
    {
        cv::imshow(window_, frame);
        if (cv::waitKey(waitMs) == 27) // Esc: skip the rest of the animation
            skip_ = true;
    }
}

void SeamVisualizer::operator()(const cv::Mat& image, const std::vector<int>& seam,
    bool vertical)
{
    ++step_;
    const std::string label = title_ + "  |  seam " + std::to_string(step_) + "/"
        + std::to_string(totalSeams_) + (vertical ? " (vertical)" : " (horizontal)")
        + "  |  " + std::to_string(image.cols) + "x" + std::to_string(image.rows);
    cv::Mat frame = makeDisplayFrame(image, canvas_, scale_, label);

    // Draw the seam on the display frame (the carving loop's image is never
    // modified). Drawn after scaling, so it stays visible when shrunk.
    std::vector<cv::Point> pts;
    pts.reserve(seam.size());
    for (int k = 0; k < static_cast<int>(seam.size()); ++k)
    {
        const int row = vertical ? k : seam[k];   // vertical: seam[i] = column
        const int col = vertical ? seam[k] : k;   // horizontal: seam[j] = row
        pts.emplace_back(cvFloor((col + 0.5) * scale_),
            LABEL_BAR_HEIGHT + cvFloor((row + 0.5) * scale_));
    }
    cv::polylines(frame, pts, false, RED, std::max(1, cvRound(scale_)), cv::LINE_8);

    emit(frame, delayMs_);
}

void SeamVisualizer::showFinal(const cv::Mat& image)
{
    const std::string label = title_ + "  |  done: " + std::to_string(image.cols)
        + "x" + std::to_string(image.rows)
        + (showWindow_ ? "  |  press any key" : "");
    cv::Mat frame = makeDisplayFrame(image, canvas_, scale_, label);

    // Hold the final frame for ~1 s in the video.
    for (int k = 0; k + 1 < static_cast<int>(fps_); ++k)
        if (writer_.isOpened())
            writer_.write(frame);

    skip_ = false; // always show the result, even if the animation was skipped
    emit(frame, showWindow_ ? 0 : 1);
}

std::string SeamVisualizer::finish()
{
    if (writer_.isOpened())
        writer_.release();
    if (showWindow_ && ownsWindow_)
    {
        cv::destroyWindow(window_);
        cv::waitKey(1); // let the window actually close
    }
    showWindow_ = false;
    return videoPath_;
}
