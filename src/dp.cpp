#include "dp.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
    constexpr double kInf = std::numeric_limits<double>::infinity();
}

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

std::vector<int> findVerticalSeamDPCompact(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    CV_Assert(m >= 1 && n >= 1);

    // Two rolling rows of cumulative cost: prev = M[i-1][*], cur = M[i][*].
    const double* e0 = energy.ptr<double>(0);
    std::vector<double> prev(e0, e0 + n);
    std::vector<double> cur(n);

    // parent[i*n + j] = offset (-1, 0, +1) to the best parent of (i, j) in row
    // i-1. Row 0 has no parent and is never read.
    std::vector<std::int8_t> parent(static_cast<size_t>(m) * n, 0);

    for (int i = 1; i < m; ++i)
    {
        const double* e = energy.ptr<double>(i);
        std::int8_t* par = parent.data() + static_cast<size_t>(i) * n;
        // Same tie-breaking as the full-table backtrack: straight up first,
        // then left, then right, each only if strictly cheaper. Written with
        // conditional selects instead of if-branches: the winner is
        // data-dependent and unpredictable, so branches would often be
        // mispredicted; selects compile to branch-free code.
        auto relax = [&](int j, double left, double right)
            {
                double best = prev[j];
                std::int8_t off = 0;
                off  = (left < best) ? std::int8_t(-1) : off;
                best = (left < best) ? left : best;
                off  = (right < best) ? std::int8_t(1) : off;
                best = (right < best) ? right : best;
                cur[j] = e[j] + best;
                par[j] = off;
            };

        // Border columns have only 2 parents; treat the missing one as +inf
        // so the interior loop needs no bounds checks.
        relax(0, kInf, n > 1 ? prev[1] : kInf);
        for (int j = 1; j < n - 1; ++j)
            relax(j, prev[j - 1], prev[j + 1]);
        if (n > 1)
            relax(n - 1, prev[n - 2], kInf);
        prev.swap(cur); // prev now holds row i
    }

    // prev holds the last row: the seam ends at its minimum.
    std::vector<int> seam(m);
    seam[m - 1] = static_cast<int>(std::min_element(prev.begin(), prev.end()) - prev.begin());

    // Backtrack using the stored parent offsets (no costs needed).
    for (int i = m - 1; i > 0; --i)
        seam[i - 1] = seam[i] + parent[static_cast<size_t>(i) * n + seam[i]];
    return seam;
}

std::vector<int> findHorizontalSeamDPCompact(const cv::Mat& energy)
{
    cv::Mat t;
    cv::transpose(energy, t);
    return findVerticalSeamDPCompact(t);
}
