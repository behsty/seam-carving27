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

    // Rows narrower than this are filled by one thread: splitting a short
    // row costs more in thread synchronisation than it saves.
    constexpr int kParallelMinCols = 512;

    // One cell of the DP recurrence, used by the compact and parallel
    // finders. prev = M[i-1][*], e = energy row i, writes cur[j] = M[i][j]
    // and par[j] = offset of the winning parent.
    //
    // Same tie-breaking as the full-table backtrack: straight up first, then
    // left, then right, each only if strictly cheaper. Written with
    // conditional selects instead of if-branches: the winner is
    // data-dependent and unpredictable, so branches would often be
    // mispredicted; selects compile to branch-free code.
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
        const double* p = prev.data();
        double* c = cur.data();

        // Border columns have only 2 parents; treat the missing one as +inf
        // so the interior loop needs no bounds checks.
        relaxCell(0, p, e, c, par, kInf, n > 1 ? p[1] : kInf);
        for (int j = 1; j < n - 1; ++j)
            relaxCell(j, p, e, c, par, p[j - 1], p[j + 1]);
        if (n > 1)
            relaxCell(n - 1, p, e, c, par, p[n - 2], kInf);
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

std::vector<int> findVerticalSeamDPParallel(const cv::Mat& energy)
{
    CV_Assert(energy.type() == CV_64F);
    const int m = energy.rows;
    const int n = energy.cols;
    CV_Assert(m >= 1 && n >= 1);

    // Two row buffers used alternately: row i reads rows[(i-1)%2] and writes
    // rows[i%2]. Indexing by parity (instead of swapping) means no thread
    // has to do the swap while the others wait.
    const double* e0 = energy.ptr<double>(0);
    std::vector<double> rows[2] = { std::vector<double>(e0, e0 + n),
                                    std::vector<double>(n) };
    std::vector<std::int8_t> parent(static_cast<size_t>(m) * n, 0);

    // One parallel region for the whole table, so threads are started once
    // per seam rather than once per row. Every thread walks the row loop;
    // "omp for" splits the columns of each row between them, and its
    // implicit barrier guarantees row i is complete before row i+1 starts
    // (row i+1 reads row i). Without OpenMP the pragmas are ignored and this
    // runs serially with the same result.
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

    // Backtracking is inherently sequential (each step needs the previous
    // one) but it is only O(m), negligible next to the O(m*n) table.
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
