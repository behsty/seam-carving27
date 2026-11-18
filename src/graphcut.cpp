#include "graphcut.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
            level_[s] = 0;
            int qHead = 0, qTail = 0;
            q_[qTail++] = s;

            while (qHead < qTail)
            {
                int u = q_[qHead++];
                if (level_[t] >= 0 && level_[u] >= level_[t])
                    break;
                for (int e = head_[u]; e != -1; e = nxt_[e])
                {
                    int v = to_[e];
                    if (cap_[e] > 0 && level_[v] < 0)
                    {
                        level_[v] = level_[u] + 1;
                        q_[qTail++] = v;
                    }
                }
            }
            return level_[t] >= 0;
        }

        ll dfs(int u, int t, ll pushed)
        {
            if (pushed == 0 || u == t)
                return pushed;
            for (int& e = it_[u]; e != -1; e = nxt_[e])
            {
                int v = to_[e];
                if (level_[u] + 1 != level_[v] || cap_[e] == 0)
                    continue;
                ll tr = dfs(v, t, std::min(pushed, cap_[e]));
                if (tr == 0)
                    continue;
                cap_[e] -= tr;
                cap_[e ^ 1] += tr;
                return tr;
            }
            return 0;
        }

        ll dinic(int s, int t)
        {
            q_.resize(head_.size());
            ll flow = 0;
            while (buildLevels(s, t))
            {
                it_ = head_;
                while (ll pushed = dfs(s, t, INF_CAP * 4))
                    flow += pushed;
            }
            return flow;
        }

        std::vector<int> head_, to_, nxt_;
        std::vector<ll> cap_;
        std::vector<int> level_, it_;
        std::vector<int> q_;
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
