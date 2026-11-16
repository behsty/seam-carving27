#pragma once

// Step-by-step seam visualization.
//
// Plugs into the carving loop as its StepFn (see seam.h): for every seam it
// shows the current image with that seam drawn in red, and can also save the
// frames as a video. It only reads the image, so results are unaffected.

#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

// Height of the label strip drawn above the image.
constexpr int LABEL_BAR_HEIGHT = 28;

// Display scale for an image of this size: small images are enlarged (up to
// 4x) so single-pixel seams are visible; large ones are shrunk to fit on
// screen (max 1280x720).
double displayScale(cv::Size size);

// Frame for display/video: a black canvas of the ORIGINAL size (times scale)
// with a label bar on top and the (possibly smaller) image in the top-left.
cv::Mat makeDisplayFrame(const cv::Mat& image, cv::Size canvasSize, double scale,
    const std::string& label);

class SeamVisualizer
{
public:
    // title:       label shown in the frame, e.g. "Dynamic programming"
    // canvasSize:  size of the ORIGINAL image; every frame is padded to it,
    //              so the window does not jump and video frames match
    // totalSeams:  seams that will be removed (for the "seam k/total" label)
    // showWindow:  show frames in a window (false = only write the video)
    // delayMs:     pause per frame in the window (>= 1)
    // videoPath:   "" for no video; .mp4 if supported, else falls back to .avi
    // windowName:  draw into an existing window (e.g. interactive mode);
    //              "" = open and close a window named after the title
    SeamVisualizer(const std::string& title, cv::Size canvasSize, int totalSeams,
        bool showWindow, int delayMs = 1, const std::string& videoPath = "",
        double fps = 30.0, const std::string& windowName = "");
    ~SeamVisualizer();

    SeamVisualizer(const SeamVisualizer&) = delete;
    SeamVisualizer& operator=(const SeamVisualizer&) = delete;

    // One step: matches StepFn in seam.h.
    void operator()(const cv::Mat& image, const std::vector<int>& seam, bool vertical);

    // Shows the final image (and adds it to the video). If the window is
    // open, waits for a key press; Esc during the animation skips ahead.
    void showFinal(const cv::Mat& image);

    // Closes the video file and (if this object opened it) the window.
    // Returns the video file written ("" if none).
    std::string finish();

private:
    void emit(const cv::Mat& frame, int waitMs);

    std::string title_;
    std::string window_;
    bool ownsWindow_;
    cv::Size canvas_;
    int totalSeams_;
    bool showWindow_;
    int delayMs_;
    double scale_ = 1.0;
    int step_ = 0;
    bool skip_ = false;    // Esc pressed: stop showing (video still written)
    double fps_;
    std::string videoPath_;
    cv::VideoWriter writer_;
    bool triedOpen_ = false;
};
