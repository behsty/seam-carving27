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

    // Returns elapsed milliseconds since timestamp t0
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
        // Initializes adjacency list arrays for `nodes` vertices and pre-reserves space for directed half-edges
        FlowGraph(int nodes, int edgeHint) : head_(nodes, -1), level_(nodes), it_(nodes)
        {
            to_.reserve(edgeHint * 2);      // Each logical edge has 2 half-edges (forward + reverse residual)
            nxt_.reserve(edgeHint * 2);     // Next half-edge index in u's adjacency list
            cap_.reserve(edgeHint * 2);     // Remaining capacity of each half-edge
        }

        // Adds a directed edge u -> v with capacity c, plus its 0-capacity reverse residual edge v -> u
        void addEdge(int u, int v, ll c)
        {
            push(u, v, c);      // Forward half-edge (gets an even index 2k)
            push(v, u, 0);      // Reverse residual half-edge (gets an odd index 2k+1)
        }

        // Computes the maximum flow from source s to sink t using Dinic's algorithm
        ll maxflow(int s, int t)
        {
            return dinic(s, t);
        }

        // Nodes reachable from s in the residual graph = source side S of a min cut.
        std::vector<char> reachableFrom(int s) const
        {
            std::vector<char> seen(head_.size(), 0);
            std::queue<int> q;      // BFS queue
            seen[s] = 1;            // Mark source s as visited (in set S)
            q.push(s);
            while (!q.empty())
            {
                int u = q.front();
                q.pop();
                for (int e = head_[u]; e != -1; e = nxt_[e])        // Iterate over outgoing half-edges of u
                {
                    if (cap_[e] > 0 && !seen[to_[e]])               // Can push residual flow to unvisited neighbor?
                    {
                        seen[to_[e]] = 1;                           // Mark neighbor as reachable from s
                        q.push(to_[e]);
                    }
                }
            }
            return seen;
        }

    private:
        // Appends a single directed half-edge u -> v with capacity c to the flat adjacency structure
        void push(int u, int v, ll c)
        {   
            to_.push_back(v);                               // Store destination vertex v
            cap_.push_back(c);                              // Store initial capacity c
            nxt_.push_back(head_[u]);                       // Link previous head of u's list after this edge
            head_[u] = static_cast<int>(to_.size()) - 1;    // Set this new half-edge as head of u's list
        }

        // ---- Dinic: BFS level graph + blocking flow (iterative DFS) ----
        // Assigns BFS distances (`level_`) from s; returns true if sink t is reachable
        bool buildLevels(int s, int t)
        {
            std::fill(level_.begin(), level_.end(), -1);        // Reset all node levels to unvisited (-1)
            level_[s] = 0;                                      // Source is at level 0
            int qHead = 0, qTail = 0;
            q_[qTail++] = s;

            while (qHead < qTail)                               // Standard BFS loop
            {
                int u = q_[qHead++];                            // Dequeue current node u
                if (level_[t] >= 0 && level_[u] >= level_[t])   // No need to expand beyond sink t's shortest level
                    break;
                for (int e = head_[u]; e != -1; e = nxt_[e])    // Scan outgoing edges from u
                {
                    int v = to_[e];                             // Neighbor vertex v
                    if (cap_[e] > 0 && level_[v] < 0)           // Positive residual capacity and unvisited v?
                    {
                        level_[v] = level_[u] + 1;              // Set shortest-path level of v
                        q_[qTail++] = v;
                    }
                }
            }
            return level_[t] >= 0;                              // True if a layered path from s to t exists
        }

        // Augments flow along level-graph edges up to `pushed`; returns actual flow sent
        ll dfs(int u, int t, ll pushed)
        {
            if (pushed == 0 || u == t)                          // Base case: zero flow left or reached sink t
                return pushed;
            for (int& e = it_[u]; e != -1; e = nxt_[e])
            {
                int v = to_[e];
                if (level_[u] + 1 != level_[v] || cap_[e] == 0)     // Must advance 1 level with available capacity
                    continue;
                ll tr = dfs(v, t, std::min(pushed, cap_[e]));       // Recursively push bottleneck capacity to t
                if (tr == 0)        // Path from v to t is blocked
                    continue;
                cap_[e] -= tr;      // Decrease forward residual capacity
                cap_[e ^ 1] += tr;  // Increase reverse residual capacity so flow can be undone later
                return tr;          // Return flow pushed along this augmenting path
            }
            return 0;
        }

        // Repeatedly builds level graphs and finds blocking flows until t cannot be reached from s
        ll dinic(int s, int t)
        {
            q_.resize(head_.size());
            ll flow = 0;
            while (buildLevels(s, t))                           // While augmenting paths exist in residual graph
            {
                it_ = head_;                                    // Reset current-arc pointers to start of each node's adjacency list
                while (ll pushed = dfs(s, t, INF_CAP * 4))      // Push flow until blocking flow is reached
                    flow += pushed;
            }
            return flow;
        }

        std::vector<int> head_, to_, nxt_; // CSR/linked-list adjacency arrays for half-edges
        std::vector<ll> cap_;              // Residual capacities of half-edges
        std::vector<int> level_, it_;      // BFS level per node and current-edge iterator per node
        std::vector<int> q_;               // Reusable BFS queue buffer
    };
} // namespace

// ---------------------------------------------------------------------------
// Seam finding
// ---------------------------------------------------------------------------

// Finds a minimum-cost 8-connected vertical seam (1 pixel per row i in [0..m-1]) via min-cut
std::vector<int> findVerticalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs)
{
    CV_Assert(cost.type() == CV_64F); // Ensure cost matrix stores 64-bit floating-point values
    const int m = cost.rows;          // Number of rows (image height)
    const int n = cost.cols;          // Number of columns (image width)
    if (n < 2)                        // Removing a seam requires at least 2 columns so >= 1 remains
        throw std::runtime_error("Cannot remove a seam from an image with fewer than 2 columns");

    // Node id of pixel (i,j); plus source and sink.
    auto id = [n](int i, int j) { return i * n + j; }; // Maps 2D pixel (row i, col j) to 1D node index
    const int S = m * n;                               // Virtual source node index
    const int T = m * n + 1;                           // Virtual sink node index

    FlowGraph g(m * n + 2, m * n * 5);                  // Graph with m*n pixel nodes + S + T, hinting ~5 edges/pixel

    for (int i = 0; i < m; ++i)
    {
        const double* c = cost.ptr<double>(i);

        // source -> first column, infinite
        g.addEdge(S, id(i, 0), INF_CAP); // Pixel (i, 0) starts on the source side unless cut across a diagonal

        for (int j = 0; j < n; ++j)
        {
            ll cap = static_cast<ll>(std::llround(c[j] * COST_SCALE));

            // Edge u -> right neighbour carries the cost of removing u.
            // The last column's "right neighbour" is the sink, so that pixel
            // can be removed too (every row has exactly one finite cut edge).
            if (j + 1 < n)
            {
                g.addEdge(id(i, j), id(i, j + 1), cap); // Finite cut edge pays cost of selecting (i, j)
                // backward infinite edge: S side of each row is a prefix
                g.addEdge(id(i, j + 1), id(i, j), INF_CAP); // Prevents "islands" in S; forces row prefix structure
            }
            else
            {
                g.addEdge(id(i, j), T, cap);            // Last column connects to T with cost of removing (i, n-1)
            }

            // diagonal infinite edges to the left neighbours in adjacent rows:
            // keeps the seam 8-connected (|c_i - c_{i+-1}| <= 1)
            if (j >= 1)
            {
                // If (i, j) is in S, (i-1, j-1) must also be in S
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
    for (int i = 0; i < m; ++i) // Extract seam column per row
    {
        int rightmost = 0; // Track highest column index in row i that remains in S
        for (int j = 0; j < n; ++j)
            if (inS[id(i, j)]) // Pixel (i, j) is on the source side S
                rightmost = j; // Update rightmost S-side pixel in row i
        seam[i] = rightmost; // rightmost pixel in S is the seam pixel
    }
    return seam; // Return vertical seam of length m
}

std::vector<int> findHorizontalSeamGraphCut(
    const cv::Mat& cost, double* maxflowMs)
{
    cv::Mat costT;
    cv::transpose(cost, costT);                         // Swap rows and columns so a horizontal path becomes vertical
    return findVerticalSeamGraphCut(costT, maxflowMs); // Solve vertical seam on transposed matrix
}
