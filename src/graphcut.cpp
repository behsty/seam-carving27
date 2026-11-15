#include "graphcut.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <queue>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

namespace
{
    using Clock = std::chrono::steady_clock;
    using ll = long long;

    double msSince(Clock::time_point t0)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    }

    // Costs are converted to integers so max-flow is exact (no floating point
    // residual errors).
    constexpr double COST_SCALE = 1000.0;
    constexpr ll INF_CAP = 1000000000000000LL; // 1e15, far above any finite cut

    // ---------------------------------------------------------------------------
    // Flow network with residual edges. Edge e and e^1 are a forward/backward pair.
    // ---------------------------------------------------------------------------
    class FlowGraph
    {
    public:
        FlowGraph(int nodes, int edgeHint) : head_(nodes, -1), level_(nodes), it_(nodes)
        {
            to_.reserve(edgeHint * 2);
            nxt_.reserve(edgeHint * 2);
            cap_.reserve(edgeHint * 2);
        }

        void addEdge(int u, int v, ll c)
        {
            push(u, v, c);
            push(v, u, 0);
        }

        ll maxflow(int s, int t)
        {
            return dinic(s, t);
        }

        // Nodes reachable from s in the residual graph = source side S of a min cut.
        std::vector<char> reachableFrom(int s) const
        {
            std::vector<char> seen(head_.size(), 0);
            std::queue<int> q;
            seen[s] = 1;
            q.push(s);
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                for (int e = head_[u]; e != -1; e = nxt_[e])
                {
                    if (cap_[e] > 0 && !seen[to_[e]])
                    {
                        seen[to_[e]] = 1;
                        q.push(to_[e]);
                    }
                }
            }
            return seen;
        }

    private:
        void push(int u, int v, ll c)
        {
            to_.push_back(v);
            cap_.push_back(c);
            nxt_.push_back(head_[u]);
            head_[u] = static_cast<int>(to_.size()) - 1;
        }

        // ---- Dinic: BFS level graph + blocking flow (iterative DFS) ----
        bool buildLevels(int s, int t)
        {
            std::fill(level_.begin(), level_.end(), -1);
            std::queue<int> q;
            level_[s] = 0;
            q.push(s);
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                // Nodes at or beyond the sink's level can't be on a shortest path.
                if (level_[t] >= 0 && level_[u] >= level_[t])
                    break;
                for (int e = head_[u]; e != -1; e = nxt_[e])
                {
                    if (cap_[e] > 0 && level_[to_[e]] < 0)
                    {
                        level_[to_[e]] = level_[u] + 1;
                        q.push(to_[e]);
                    }
                }
            }
            return level_[t] >= 0;
        }

        ll blockingFlow(int s, int t)
        {
            ll total = 0;
            std::vector<int> path; // edge ids from s to the current node
            int u = s;
            while (true)
            {
                if (u == t)
                {
                    ll bottleneck = INF_CAP * 4;
                    for (int e : path)
                        bottleneck = std::min(bottleneck, cap_[e]);
                    size_t firstSaturated = path.size();
                    for (size_t k = 0; k < path.size(); ++k)
                    {
                        int e = path[k];
                        cap_[e] -= bottleneck;
                        cap_[e ^ 1] += bottleneck;
                        if (cap_[e] == 0 && firstSaturated == path.size())
                            firstSaturated = k;
                    }
                    total += bottleneck;
                    // Retreat only to the tail of the first saturated edge instead
                    // of restarting from the source.
                    u = to_[path[firstSaturated] ^ 1];
                    path.resize(firstSaturated);
                    continue;
                }

                bool advanced = false;
                while (it_[u] != -1)
                {
                    int e = it_[u];
                    int v = to_[e];
                    if (cap_[e] > 0 && level_[v] == level_[u] + 1)
                    {
                        path.push_back(e);
                        u = v;
                        advanced = true;
                        break;
                    }
                    it_[u] = nxt_[e];
                }
                if (advanced)
                    continue;

                // dead end: retreat
                if (u == s)
                    break;
                level_[u] = -1;
                int e = path.back();
                path.pop_back();
                u = to_[e ^ 1];
            }
            return total;
        }

        ll dinic(int s, int t)
        {
            ll flow = 0;
            while (buildLevels(s, t))
            {
                it_ = head_;
                flow += blockingFlow(s, t);
            }
            return flow;
        }

        std::vector<int> head_, to_, nxt_;
        std::vector<ll> cap_;
        std::vector<int> level_, it_;
    };
} // namespace

// ---------------------------------------------------------------------------
// Seam finding
// ---------------------------------------------------------------------------

std::vector<int> findVerticalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs)
{
    CV_Assert(cost.type() == CV_64F);
    const int m = cost.rows;
    const int n = cost.cols;
    if (n < 2)
        throw std::runtime_error("Cannot remove a seam from an image with fewer than 2 columns");

    // Node id of pixel (i,j); plus source and sink.
    auto id = [n](int i, int j) { return i * n + j; };
    const int S = m * n;
    const int T = m * n + 1;

    FlowGraph g(m * n + 2, m * n * 5);

    for (int i = 0; i < m; ++i)
    {
        const double* c = cost.ptr<double>(i);

        // source -> first column, infinite
        g.addEdge(S, id(i, 0), INF_CAP);

        for (int j = 0; j < n; ++j)
        {
            ll cap = static_cast<ll>(std::llround(c[j] * COST_SCALE));

            // Edge u -> right neighbour carries the cost of removing u.
            // The last column's "right neighbour" is the sink, so that pixel
            // can be removed too (every row has exactly one finite cut edge).
            if (j + 1 < n)
            {
                g.addEdge(id(i, j), id(i, j + 1), cap);
                // backward infinite edge: S side of each row is a prefix
                g.addEdge(id(i, j + 1), id(i, j), INF_CAP);
            }
            else
            {
                g.addEdge(id(i, j), T, cap);
            }

            // diagonal infinite edges to the left neighbours in adjacent rows:
            // keeps the seam 8-connected (|c_i - c_{i+-1}| <= 1)
            if (j >= 1)
            {
                if (i > 0)
                    g.addEdge(id(i, j), id(i - 1, j - 1), INF_CAP);
                if (i + 1 < m)
                    g.addEdge(id(i, j), id(i + 1, j - 1), INF_CAP);
            }
        }
    }

    auto t0 = Clock::now();
    g.maxflow(S, T);
    if (maxflowMs)
        *maxflowMs += msSince(t0);

    // Source side S of the min cut = reachable set in the residual graph.
    std::vector<char> inS = g.reachableFrom(S);

    std::vector<int> seam(m, 0);
    for (int i = 0; i < m; ++i)
    {
        int rightmost = 0;
        for (int j = 0; j < n; ++j)
            if (inS[id(i, j)])
                rightmost = j;
        seam[i] = rightmost; // rightmost pixel in S is the seam pixel
    }
    return seam;
}

std::vector<int> findHorizontalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs)
{
    cv::Mat costT;
    cv::transpose(cost, costT);
    return findVerticalSeamGraphCut(costT, maxflowMs);
}

// ---------------------------------------------------------------------------
// Seam removal
// ---------------------------------------------------------------------------
cv::Mat removeVerticalSeam(const cv::Mat& src, const std::vector<int>& seam)
{
    CV_Assert(static_cast<int>(seam.size()) == src.rows);
    cv::Mat dst(src.rows, src.cols - 1, src.type());
    const size_t es = src.elemSize();
    for (int i = 0; i < src.rows; ++i)
    {
        const uchar* s = src.ptr<uchar>(i);
        uchar* d = dst.ptr<uchar>(i);
        const int k = seam[i];
        std::memcpy(d, s, static_cast<size_t>(k) * es);
        std::memcpy(d + static_cast<size_t>(k) * es,
            s + static_cast<size_t>(k + 1) * es,
            static_cast<size_t>(src.cols - k - 1) * es);
    }
    return dst;
}

cv::Mat removeHorizontalSeam(const cv::Mat& src, const std::vector<int>& seam)
{
    cv::Mat t, r, out;
    cv::transpose(src, t);
    r = removeVerticalSeam(t, seam);
    cv::transpose(r, out);
    return out;
}

// ---------------------------------------------------------------------------
// Seam cost
// ---------------------------------------------------------------------------
double verticalSeamCost(const cv::Mat& energy, const std::vector<int>& seam)
{
    double sum = 0.0;
    for (int i = 0; i < energy.rows; ++i)
        sum += energy.at<double>(i, seam[i]);
    return sum;
}

double horizontalSeamCost(const cv::Mat& energy, const std::vector<int>& seam)
{
    double sum = 0.0;
    for (int j = 0; j < energy.cols; ++j)
        sum += energy.at<double>(seam[j], j);
    return sum;
}

// ---------------------------------------------------------------------------
// Generic carving loop (any seam finder)
// ---------------------------------------------------------------------------
CarveResult seamCarve(
    const cv::Mat& image, int numVertical, int numHorizontal,
    const EnergyFn& energyFn,
    const SeamFinder& findVertical,
    const SeamFinder& findHorizontal,
    const ProgressFn& progress)
{
    if (numVertical < 0 || numHorizontal < 0 ||
        numVertical >= image.cols || numHorizontal >= image.rows)
        throw std::runtime_error("Invalid number of seams to remove");

    CarveResult res;
    res.seamOverlay = image.clone();
    const cv::Vec3b red(0, 0, 255);

    // origPos(i,j) = (x,y) in the ORIGINAL image of the pixel now at (i,j).
    // Lets us draw every seam on the original picture.
    cv::Mat origPos(image.rows, image.cols, CV_32SC2);
    for (int y = 0; y < image.rows; ++y)
        for (int x = 0; x < image.cols; ++x)
            origPos.at<cv::Vec2i>(y, x) = cv::Vec2i(x, y);

    cv::Mat cur = image.clone();
    const int total = numVertical + numHorizontal;
    auto t0 = Clock::now();

    auto timedEnergy = [&](const cv::Mat& img)
        {
            auto t = Clock::now();
            cv::Mat e = energyFn(img);
            res.energyMs += msSince(t);
            return e;
        };

    auto timedFind = [&](const SeamFinder& finder, const cv::Mat& energy)
        {
            auto t = Clock::now();
            std::vector<int> seam = finder(energy);
            res.seamMs += msSince(t);
            return seam;
        };

    auto mark = [&](int row, int col)
        {
            cv::Vec2i p = origPos.at<cv::Vec2i>(row, col);
            res.seamOverlay.at<cv::Vec3b>(p[1], p[0]) = red;
        };

    auto report = [&]()
        {
            if (progress)
                progress(res.verticalRemoved + res.horizontalRemoved, total, msSince(t0));
        };

    for (int k = 0; k < numVertical; ++k)
    {
        cv::Mat energy = timedEnergy(cur);
        std::vector<int> seam = timedFind(findVertical, energy);
        res.totalSeamCost += verticalSeamCost(energy, seam);
        for (int i = 0; i < cur.rows; ++i)
            mark(i, seam[i]);
        cur = removeVerticalSeam(cur, seam);
        origPos = removeVerticalSeam(origPos, seam);
        ++res.verticalRemoved;
        report();
    }

    for (int k = 0; k < numHorizontal; ++k)
    {
        cv::Mat energy = timedEnergy(cur);
        std::vector<int> seam = timedFind(findHorizontal, energy);
        res.totalSeamCost += horizontalSeamCost(energy, seam);
        for (int j = 0; j < cur.cols; ++j)
            mark(seam[j], j);
        cur = removeHorizontalSeam(cur, seam);
        origPos = removeHorizontalSeam(origPos, seam);
        ++res.horizontalRemoved;
        report();
    }

    res.image = cur;
    res.totalMs = msSince(t0);
    return res;
}

// ---------------------------------------------------------------------------
// Graph-cut carving = generic loop + graph-cut seam finders
// ---------------------------------------------------------------------------
GraphCutResult seamCarveGraphCut(
    const cv::Mat& image, int numVertical, int numHorizontal,
    const EnergyFn& energyFn, const ProgressFn& progress)
{
    double maxflowMs = 0.0;
    CarveResult base = seamCarve(
        image, numVertical, numHorizontal, energyFn,
        [&](const cv::Mat& e) { return findVerticalSeamGraphCut(e, &maxflowMs); },
        [&](const cv::Mat& e) { return findHorizontalSeamGraphCut(e, &maxflowMs); },
        progress);

    GraphCutResult res;
    static_cast<CarveResult&>(res) = base;
    res.maxflowMs = maxflowMs;
    return res;
}
