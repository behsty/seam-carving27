#include "greedy.h"

#include <algorithm>

std::vector<int> findVerticalSeamGreedy(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    std::vector<int> seam(m);

    // Step 1: lowest-energy pixel in the top row.
    const double* r0 = energy.ptr<double>(0);
    seam[0] = static_cast<int>(std::min_element(r0, r0 + n) - r0);

    // Step 2: row by row, take the cheapest of the 3 pixels below.
    for (int i = 1; i < m; ++i)
    {
        const double* r = energy.ptr<double>(i);
        const int prev = seam[i - 1];
        int best = prev; // tie -> go straight down
        if (prev > 0     && r[prev - 1] < r[best]) best = prev - 1;
        if (prev < n - 1 && r[prev + 1] < r[best]) best = prev + 1;
        seam[i] = best;
    }
    return seam;
}

std::vector<int> findHorizontalSeamGreedy(const cv::Mat& energy)
{
    cv::Mat t;
    cv::transpose(energy, t);
    return findVerticalSeamGreedy(t);
}
