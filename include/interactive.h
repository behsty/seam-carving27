#pragma once

// Interactive (manual) mode: a setup window with HighGUI sliders for the
// target width, height and method. Nothing is carved while dragging; Enter
// carves with the step-by-step visualization, then returns to the sliders.

#include <vector>
#include <opencv2/core.hpp>

#include "seam.h"

// initialWidth / initialHeight: starting slider values (-1 = original size).
// Returns 0 when the user quits (Esc or closing the window).
int runInteractive(const cv::Mat& image, const EnergyFn& energyFn,
    const std::vector<SeamMethod>& methods, int delayMs,
    int initialWidth = -1, int initialHeight = -1);
