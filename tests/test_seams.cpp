// Correctness tests for the seam finders.
//
//   1. DP on a small hand-worked example (known seam and cost).
//   2. DP vs brute force (every possible seam enumerated) on random small
//      energy maps, vertical and horizontal.
//   3. Graph cut finds a seam of the same (optimal) cost as DP.
//   4. Greedy is never cheaper than DP.
//   5. Edge cases: 1 column, 1 row, uniform energy.
//   6. End to end: seamCarve() with DP removes the zero-energy column/row.
//
// Every returned seam is also checked to be valid: right length, inside the
// image, and connected (consecutive positions differ by at most 1).
//
// Plain C++ with no test framework; exits with 0 only if every check passes.

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "dp.h"
#include "graphcut.h"
#include "greedy.h"

namespace
{
    using Grid = std::vector<std::vector<double>>;

    int g_checks = 0;
    int g_failures = 0;

    void check(bool ok, const std::string& what)
    {
        ++g_checks;
        if (!ok)
        {
            ++g_failures;
            std::printf("  FAIL: %s\n", what.c_str());
        }
    }

    cv::Mat toMat(const Grid& g)
    {
        cv::Mat m(static_cast<int>(g.size()), static_cast<int>(g[0].size()), CV_64F);
        for (int i = 0; i < m.rows; ++i)
            for (int j = 0; j < m.cols; ++j)
                m.at<double>(i, j) = g[i][j];
        return m;
    }

    Grid transposeGrid(const Grid& g)
    {
        Grid t(g[0].size(), std::vector<double>(g.size()));
        for (size_t i = 0; i < g.size(); ++i)
            for (size_t j = 0; j < g[0].size(); ++j)
                t[j][i] = g[i][j];
        return t;
    }

    Grid randomGrid(std::mt19937& rng, int m, int n, int maxValue)
    {
        // Small integers make ties common, which is the tricky case, and keep
        // costs exact for graph cut (it scales costs to integers internally).
        std::uniform_int_distribution<int> val(0, maxValue);
        Grid g(m, std::vector<double>(n));
        for (auto& row : g)
            for (double& v : row)
                v = val(rng);
        return g;
    }

    // Valid vertical seam: one column per row, in range, 8-connected.
    bool isValidVerticalSeam(const std::vector<int>& seam, int m, int n)
    {
        if (static_cast<int>(seam.size()) != m)
            return false;
        for (int i = 0; i < m; ++i)
        {
            if (seam[i] < 0 || seam[i] >= n)
                return false;
            if (i > 0 && std::abs(seam[i] - seam[i - 1]) > 1)
                return false;
        }
        return true;
    }

    double verticalCost(const Grid& g, const std::vector<int>& seam)
    {
        double sum = 0.0;
        for (size_t i = 0; i < g.size(); ++i)
            sum += g[i][seam[i]];
        return sum;
    }

    // Brute force: try every one of the (up to 3^(m-1) * n) vertical seams.
    void bruteRec(const Grid& g, size_t i, int j, double acc, double& best)
    {
        acc += g[i][j];
        if (i + 1 == g.size())
        {
            best = std::min(best, acc);
            return;
        }
        const int n = static_cast<int>(g[0].size());
        for (int d = -1; d <= 1; ++d)
            if (j + d >= 0 && j + d < n)
                bruteRec(g, i + 1, j + d, acc, best);
    }

    double bruteForceMinVertical(const Grid& g)
    {
        double best = std::numeric_limits<double>::infinity();
        for (int j = 0; j < static_cast<int>(g[0].size()); ++j)
            bruteRec(g, 0, j, 0.0, best);
        return best;
    }

    // -----------------------------------------------------------------------

    void testHandExample()
    {
        std::printf("DP hand-worked example\n");
        // Cumulative table M (worked by hand):
        //   1 4 3 5        1 4 3 5
        //   3 2 5 2   ->   4 3 8 5
        //   5 2 4 2        8 5 7 7
        //   2 3 3 1        7 8 8 8
        // Minimum of the last row is 7 at column 0; backtracking gives
        // columns 0 <- 1 <- 1 <- 0, i.e. energies 1 + 2 + 2 + 2 = 7.
        const Grid g = { { 1, 4, 3, 5 },
                         { 3, 2, 5, 2 },
                         { 5, 2, 4, 2 },
                         { 2, 3, 3, 1 } };
        std::vector<int> seam = findVerticalSeamDP(toMat(g));
        check(seam == std::vector<int>({ 0, 1, 1, 0 }), "seam is {0,1,1,0}");
        check(verticalCost(g, seam) == 7.0, "seam cost is 7");
    }

    void testDPvsBruteForce(std::mt19937& rng)
    {
        std::printf("DP vs brute force (random maps up to 8x7)\n");
        std::uniform_int_distribution<int> dimM(1, 8), dimN(1, 7);
        int bad = 0;
        for (int t = 0; t < 2000; ++t)
        {
            const int m = dimM(rng), n = dimN(rng);
            const Grid g = randomGrid(rng, m, n, 9);

            // Vertical seam.
            std::vector<int> v = findVerticalSeamDP(toMat(g));
            if (!isValidVerticalSeam(v, m, n) ||
                verticalCost(g, v) != bruteForceMinVertical(g))
                ++bad;

            // Horizontal seam = vertical seam of the transposed map.
            const Grid gt = transposeGrid(g);
            std::vector<int> h = findHorizontalSeamDP(toMat(g));
            if (!isValidVerticalSeam(h, n, m) ||
                verticalCost(gt, h) != bruteForceMinVertical(gt))
                ++bad;
        }
        check(bad == 0, std::to_string(bad) + " of 4000 seams not optimal/valid");
    }

    void testGraphCutMatchesDP(std::mt19937& rng)
    {
        std::printf("Graph cut cost == DP cost (random maps, n >= 2)\n");
        std::uniform_int_distribution<int> dimM(1, 12), dimN(2, 12);
        int bad = 0;
        for (int t = 0; t < 500; ++t)
        {
            const int m = dimM(rng), n = dimN(rng);
            const Grid g = randomGrid(rng, m, n, 20);
            const cv::Mat e = toMat(g);

            std::vector<int> dp = findVerticalSeamDP(e);
            std::vector<int> gc = findVerticalSeamGraphCut(e);
            // Seams may differ when there are ties; the cost may not.
            if (!isValidVerticalSeam(gc, m, n) ||
                verticalCost(g, gc) != verticalCost(g, dp))
                ++bad;
        }
        check(bad == 0, std::to_string(bad) + " of 500 graph-cut seams differ in cost");
    }

    void testGreedyNeverBetter(std::mt19937& rng)
    {
        std::printf("Greedy cost >= DP cost (random maps)\n");
        std::uniform_int_distribution<int> dimM(1, 30), dimN(1, 30);
        int bad = 0, strictlyWorse = 0;
        const int trials = 1000;
        for (int t = 0; t < trials; ++t)
        {
            const int m = dimM(rng), n = dimN(rng);
            const Grid g = randomGrid(rng, m, n, 99);
            const cv::Mat e = toMat(g);

            const std::vector<int> gr = findVerticalSeamGreedy(e);
            const double dpCost = verticalCost(g, findVerticalSeamDP(e));
            const double grCost = verticalCost(g, gr);
            if (!isValidVerticalSeam(gr, m, n) || grCost < dpCost)
                ++bad;
            if (grCost > dpCost)
                ++strictlyWorse;
        }
        check(bad == 0, std::to_string(bad) + " greedy seams cheaper than DP or invalid");
        std::printf("  (greedy was strictly worse than DP in %d of %d maps)\n",
            strictlyWorse, trials);
    }

    void testEdgeCases()
    {
        std::printf("Edge cases\n");

        // One column: the only seam is column 0 in every row.
        std::vector<int> s = findVerticalSeamDP(toMat({ { 5 }, { 1 }, { 9 } }));
        check(s == std::vector<int>({ 0, 0, 0 }), "single column -> all zeros");

        // One row: the seam is just the cheapest pixel.
        s = findVerticalSeamDP(toMat({ { 4, 2, 7, 1, 3 } }));
        check(s == std::vector<int>({ 3 }), "single row -> argmin");

        // Uniform energy: any seam is optimal; must still be valid.
        const Grid flat(6, std::vector<double>(5, 1.0));
        s = findVerticalSeamDP(toMat(flat));
        check(isValidVerticalSeam(s, 6, 5) && verticalCost(flat, s) == 6.0,
            "uniform energy -> valid seam of cost m");

        // Horizontal on a single row: one seam pixel per column, all row 0.
        s = findHorizontalSeamDP(toMat({ { 4, 2, 7 } }));
        check(s == std::vector<int>({ 0, 0, 0 }), "horizontal on single row -> all zeros");
    }

    void testSeamCarveEndToEnd()
    {
        std::printf("seamCarve() with DP removes the zero-energy column/row\n");

        // Toy energy: the blue channel itself, so a dark stripe has energy 0.
        auto blueEnergy = [](const cv::Mat& img)
            {
                cv::Mat e(img.rows, img.cols, CV_64F);
                for (int i = 0; i < img.rows; ++i)
                    for (int j = 0; j < img.cols; ++j)
                        e.at<double>(i, j) = img.at<cv::Vec3b>(i, j)[0];
                return e;
            };
        auto countDark = [](const cv::Mat& img)
            {
                int dark = 0;
                for (int i = 0; i < img.rows; ++i)
                    for (int j = 0; j < img.cols; ++j)
                        if (img.at<cv::Vec3b>(i, j)[0] == 0)
                            ++dark;
                return dark;
            };

        // 5x6 image, bright everywhere except a dark column 3 and dark row 2
        // (the stripes cross at (2,3)).
        cv::Mat img(5, 6, CV_8UC3);
        for (int i = 0; i < img.rows; ++i)
            for (int j = 0; j < img.cols; ++j)
            {
                const unsigned char v = (j == 3 || i == 2) ? 0 : 200;
                img.at<cv::Vec3b>(i, j) = cv::Vec3b(v, v, v);
            }

        CarveResult r = seamCarve(img, 1, 0, blueEnergy,
            findVerticalSeamDP, findHorizontalSeamDP);
        check(r.image.rows == 5 && r.image.cols == 5, "1 vertical seam -> 5x5");
        check(r.totalSeamCost == 0.0, "vertical seam followed the dark column");
        check(countDark(r.image) == 5, "only the dark row is left (5 pixels)");

        r = seamCarve(img, 1, 1, blueEnergy,
            findVerticalSeamDP, findHorizontalSeamDP);
        check(r.image.rows == 4 && r.image.cols == 5, "1 + 1 seams -> 4x5");
        check(r.totalSeamCost == 0.0, "both seams followed the dark stripes");
        check(countDark(r.image) == 0, "no dark pixels remain");
    }
} // namespace

int main()
{
    std::mt19937 rng(12345); // fixed seed: runs are reproducible

    testHandExample();
    testDPvsBruteForce(rng);
    testGraphCutMatchesDP(rng);
    testGreedyNeverBetter(rng);
    testEdgeCases();
    testSeamCarveEndToEnd();

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
