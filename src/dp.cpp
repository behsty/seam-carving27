#include "dp.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace
{
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // Narrower rows run single-threaded (sync would cost more than it saves).
    constexpr int kParallelMinCols = 512;

    // One DP cell: cur[j] = e[j] + min parent, par[j] = its offset (-1/0/+1).
    // Ties prefer up, then left, then right (matches full-table DP).
    // Selects instead of ifs keep it branch-free and vectorisable.
    inline void relaxCell(int j, const double* prev, const double* e,
        double* cur, std::int8_t* par, double left, double right)
    {
        double best = prev[j];
        std::int8_t off = 0;
        off  = (left < best) ? std::int8_t(-1) : off;
        best = (left < best) ? left : best;
        off  = (right < best) ? std::int8_t(1) : off;
        best = (right < best) ? right : best;
        cur[j] = e[j] + best;
        par[j] = off;
    }
}

std::vector<int> findVerticalSeamDP(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    CV_Assert(m >= 1 && n >= 1);

    // 1. Fill the cost table. Base case: M[0][j] = e[0][j].
    cv::Mat M(m, n, CV_64F);
    energy.row(0).copyTo(M.row(0));

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

    // 2. The seam ends at the minimum of the last row.
    std::vector<int> seam(m);
    const double* last = M.ptr<double>(m - 1);
    seam[m - 1] = static_cast<int>(std::min_element(last, last + n) - last);

    // 3. Backtrack to the cheapest parent (ties go straight up).
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
    // Horizontal seam = vertical seam of the transpose.
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

    // Rolling cost rows: prev = M[i-1], cur = M[i].
    const double* e0 = energy.ptr<double>(0);
    std::vector<double> prev(e0, e0 + n);
    std::vector<double> cur(n);

    // parent[i*n + j] = offset to the best parent of (i, j). Row 0 unused.
    std::vector<std::int8_t> parent(static_cast<size_t>(m) * n, 0);

    for (int i = 1; i < m; ++i)
    {
        const double* e = energy.ptr<double>(i);
        std::int8_t* par = parent.data() + static_cast<size_t>(i) * n;
        const double* p = prev.data();
        double* c = cur.data();

        // Borders handled separately (missing parent = +inf), so the
        // interior loop has no bounds checks.
        relaxCell(0, p, e, c, par, kInf, n > 1 ? p[1] : kInf);
        for (int j = 1; j < n - 1; ++j)
            relaxCell(j, p, e, c, par, p[j - 1], p[j + 1]);
        if (n > 1)
            relaxCell(n - 1, p, e, c, par, p[n - 2], kInf);
        prev.swap(cur);
    }

    // Seam ends at the minimum of the last row (now in prev).
    std::vector<int> seam(m);
    seam[m - 1] = static_cast<int>(std::min_element(prev.begin(), prev.end()) - prev.begin());

    // Backtrack via stored offsets.
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

std::vector<int> findVerticalSeamDPParallel(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    CV_Assert(m >= 1 && n >= 1);

    // Row i reads rows[(i-1)%2], writes rows[i%2]; no swap needed between threads.
    const double* e0 = energy.ptr<double>(0);
    std::vector<double> rows[2] = { std::vector<double>(e0, e0 + n),
                                    std::vector<double>(n) };
    std::vector<std::int8_t> parent(static_cast<size_t>(m) * n, 0);

    // Threads start once per seam. "omp for" splits each row's columns; its
    // implicit barrier finishes row i before row i+1 starts.
#pragma omp parallel if (n >= kParallelMinCols)
    {
        for (int i = 1; i < m; ++i)
        {
            const double* e = energy.ptr<double>(i);
            const double* p = rows[(i - 1) & 1].data();
            double* c = rows[i & 1].data();
            std::int8_t* par = parent.data() + static_cast<size_t>(i) * n;

#pragma omp for schedule(static)
            for (int j = 0; j < n; ++j)
            {
                const double left  = (j > 0)     ? p[j - 1] : kInf;
                const double right = (j < n - 1) ? p[j + 1] : kInf;
                relaxCell(j, p, e, c, par, left, right);
            }
        }
    }

    // Backtrack serially: only O(m).
    const std::vector<double>& last = rows[(m - 1) & 1];
    std::vector<int> seam(m);
    seam[m - 1] = static_cast<int>(std::min_element(last.begin(), last.end()) - last.begin());
    for (int i = m - 1; i > 0; --i)
        seam[i - 1] = seam[i] + parent[static_cast<size_t>(i) * n + seam[i]];
    return seam;
}

std::vector<int> findHorizontalSeamDPParallel(const cv::Mat& energy)
{
    cv::Mat t;
    cv::transpose(energy, t);
    return findVerticalSeamDPParallel(t);
}

int dpParallelThreads()
{
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}
