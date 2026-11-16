#include "dp.h"

#include <algorithm>

std::vector<int> findVerticalSeamDP(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    CV_Assert(m >= 1 && n >= 1);

    // Step 1: fill the cumulative-cost table M (same size as the energy).
    cv::Mat M(m, n, CV_64F);
    energy.row(0).copyTo(M.row(0)); // base case: M[0][j] = e[0][j]

    for (int i = 1; i < m; ++i)
    {
        const double* e = energy.ptr<double>(i);
        const double* up = M.ptr<double>(i - 1);
        double* cur = M.ptr<double>(i);
        for (int j = 0; j < n; ++j)
        {
            double best = up[j];
            if (j > 0)     best = std::min(best, up[j - 1]);
            if (j < n - 1) best = std::min(best, up[j + 1]);
            cur[j] = e[j] + best;
        }
    }

    // Step 2: the optimal seam ends at the minimum of the last row.
    std::vector<int> seam(m);
    const double* last = M.ptr<double>(m - 1);
    seam[m - 1] = static_cast<int>(std::min_element(last, last + n) - last);

    // Step 3: backtrack. From (i, j), step to whichever parent in row i-1
    // produced the minimum (ties go straight up, as in the greedy finder).
    for (int i = m - 1; i > 0; --i)
    {
        const double* up = M.ptr<double>(i - 1);
        const int j = seam[i];
        int best = j;
        if (j > 0     && up[j - 1] < up[best]) best = j - 1;
        if (j < n - 1 && up[j + 1] < up[best]) best = j + 1;
        seam[i - 1] = best;
    }
    return seam;
}

std::vector<int> findHorizontalSeamDP(const cv::Mat& energy)
{
    // A horizontal seam in e is a vertical seam in e transposed.
    cv::Mat t;
    cv::transpose(energy, t);
    return findVerticalSeamDP(t);
}
