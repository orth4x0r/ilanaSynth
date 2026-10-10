// Prototype measurements for docs/ROUTING-CPU.md. Standalone (no JUCE):
//   g++ -O2 -std=c++20 -pthread routing_proto.cpp -o routing_proto && ./routing_proto
// A: cost of handing parallel branches to worker threads (futex wait vs spin).
// B: what denormals cost in a decaying feedback tail, FTZ/DAZ off vs on.
// C: buffer pooling (liveness colouring) vs one buffer per cable, and what
//    block copies cost.
// D: what a silence detector costs next to the work it can skip.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
#include <vector>
#include <xmmintrin.h>
#include <pmmintrin.h>

using Clock = std::chrono::steady_clock;
static double now_us() { return std::chrono::duration<double, std::micro> (Clock::now().time_since_epoch()).count(); }

// ---- A ------------------------------------------------------------------
static volatile double sink;
static void burn (double us) { const double end = now_us() + us; double x = 1; while (now_us() < end) x = x * 1.0000001 + 1e-9; sink = x; }

struct Pool
{
    std::atomic<std::uint32_t> gen { 0 };
    std::atomic<std::uint64_t> next { 0 };
    std::atomic<int> done { 0 };
    std::atomic<bool> quit { false };
    std::atomic<double> burnUs { 0 };
    bool spin = false;
    std::vector<std::thread> workers;

    void run()
    {
        while (true)
        {
            const auto t = next.fetch_add (1);
            if ((int) (t & 0xffffffffu) >= (int) (t >> 32)) return;
            burn (burnUs.load());
            done.fetch_add (1);
        }
    }
    void start (int n, bool spinning)
    {
        spin = spinning;
        for (int i = 0; i < n; ++i)
            workers.emplace_back ([this] {
                auto seen = gen.load();
                while (! quit.load())
                {
                    if (spin) { while (gen.load() == seen && ! quit.load()) _mm_pause(); }
                    else gen.wait (seen);
                    seen = gen.load();
                    if (quit.load()) return;
                    run();
                }
            });
    }
    void stop() { quit = true; gen.fetch_add (1); gen.notify_all(); for (auto& w : workers) w.join(); }
    double block (int tasks, double us)
    {
        burnUs = us; done = 0; next.store ((std::uint64_t) tasks << 32);
        const auto t0 = now_us();
        gen.fetch_add (1); if (! spin) gen.notify_all();
        run();
        while (done.load() < tasks) std::this_thread::yield();
        return now_us() - t0;
    }
};

static void benchA()
{
    std::printf ("\n== A. parallel branches: wall time per block (us), serial vs 3 workers + caller (4 cores)\n");
    std::printf ("%-8s %-6s %9s %12s %12s   (wake overhead = parallel - burn)\n", "tasks", "us/ea", "serial", "futex-wait", "spin");
    for (int tasks : { 2, 4 })
        for (double us : { 5.0, 20.0, 50.0, 100.0, 300.0, 1000.0 })
        {
            double res[2];
            for (int mode = 0; mode < 2; ++mode)
            {
                Pool pool; pool.start (3, mode == 1);
                std::this_thread::sleep_for (std::chrono::milliseconds (20));
                std::vector<double> t;
                for (int i = 0; i < 400; ++i)
                {
                    t.push_back (pool.block (tasks, us));
                    // a real host leaves the cores idle between blocks (~ 10 ms at 512 @ 48k): use 2 ms here
                    std::this_thread::sleep_for (std::chrono::microseconds (2000));
                }
                std::sort (t.begin(), t.end());
                res[mode] = t[t.size() / 2];
                pool.stop();
            }
            std::printf ("%-8d %-6.0f %9.1f %12.1f %12.1f\n", tasks, us, tasks * us, res[0], res[1]);
        }
}

// ---- B ------------------------------------------------------------------
static double tailBench (bool ftz)
{
    if (ftz) { _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON); }
    else     { _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_OFF); _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_OFF); }
    // 8 feedback combs (reverb-like) ringing out from an impulse, 20 s at 48 kHz, 0.999 loop gain per ~30 ms
    constexpr int N = 8, L = 1500;
    static float line[N][L]; std::memset (line, 0, sizeof line);
    int pos = 0; float lp[N] = {};
    double out = 0;
    const auto t0 = now_us();
    for (int n = 0; n < 48000 * 20; ++n)
    {
        const float in = n == 0 ? 1.0f : 0.0f;
        float sum = 0;
        for (int c = 0; c < N; ++c)
        {
            float y = line[c][pos];
            lp[c] += 0.4f * (y - lp[c]);
            line[c][pos] = in + 0.93f * lp[c];
            sum += y;
        }
        pos = (pos + 1) % L;
        out += sum;
    }
    sink = out;
    return now_us() - t0;
}

static void benchB()
{
    std::printf ("\n== B. denormals: 8-comb feedback tail, 20 s of audio\n");
    const auto off = tailBench (false), on = tailBench (true);
    std::printf ("FTZ/DAZ off: %.1f ms   on: %.1f ms   slowdown without: %.2fx\n", off / 1000, on / 1000, off / on);
    // And the same tail with FTZ off but a 1e-18 offset added in the loop
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_OFF); _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_OFF);
    constexpr int N = 8, L = 1500; static float line[N][L]; std::memset (line, 0, sizeof line);
    int pos = 0; float lp[N] = {}; double out = 0; const auto t0 = now_us();
    for (int n = 0; n < 48000 * 20; ++n)
    {
        const float in = n == 0 ? 1.0f : 0.0f; float sum = 0;
        for (int c = 0; c < N; ++c)
        {
            float y = line[c][pos]; lp[c] += 0.4f * (y - lp[c]);
            line[c][pos] = in + 0.93f * lp[c] + ((n & 1) ? 1e-18f : -1e-18f);
            sum += y;
        }
        pos = (pos + 1) % L; out += sum;
    }
    sink = out;
    std::printf ("FTZ off + 1e-18 alternating offset: %.1f ms (%.2fx of FTZ on)\n", (now_us() - t0) / 1000, (now_us() - t0) / on);
}

// ---- C ------------------------------------------------------------------
static void benchC()
{
    std::printf ("\n== C. buffer pooling and copies\n");
    std::mt19937 rng (3);
    for (int nodes : { 6, 10, 16, 24 })
    {
        double naive = 0, pooled = 0; const int trials = 200;
        for (int t = 0; t < trials; ++t)
        {
            // random DAG in topological order: each node reads 1-2 earlier outputs (node 0 = rack input)
            std::vector<std::vector<int>> in (nodes);
            std::vector<int> lastUse (nodes, 0);
            for (int i = 1; i < nodes; ++i)
            {
                in[i].push_back (std::uniform_int_distribution<int> (std::max (0, i - 3), i - 1) (rng));
                if (rng() % 3 == 0) in[i].push_back (std::uniform_int_distribution<int> (0, i - 1) (rng));
                for (int s : in[i]) lastUse[s] = i;
            }
            // naive: one buffer per node output. pooled: free a buffer after its last reader.
            int live = 0, peak = 0;
            for (int i = 0; i < nodes; ++i)
            {
                ++live; peak = std::max (peak, live);
                for (int s = 0; s < i; ++s) if (lastUse[s] == i + 0 && s != i) {}
                for (int s = 0; s < nodes; ++s) if (lastUse[s] == i && s < i) --live;
            }
            naive += nodes; pooled += peak;
        }
        std::printf ("%2d nodes: one buffer per node %.1f, liveness-pooled peak %.1f (stereo 512 floats = 4 KB each)\n", nodes, naive / trials, pooled / trials);
    }
    alignas(32) static float a[2][1024], b[2][1024]; for (auto& c : a) for (auto& v : c) v = 0.5f;
    const int reps = 200000; auto t0 = now_us();
    for (int r = 0; r < reps; ++r) { std::memcpy (b, a, sizeof a); __asm__ volatile ("" ::: "memory"); }
    std::printf ("copy of one stereo block (2 x 1024 floats = 8 KB): %.0f ns\n", (now_us() - t0) * 1000 / reps);
    t0 = now_us();
    for (int r = 0; r < reps; ++r) { for (int c = 0; c < 2; ++c) for (int i = 0; i < 1024; ++i) b[c][i] += a[c][i] * 0.25f; __asm__ volatile ("" ::: "memory"); }
    std::printf ("sum-with-gain of one stereo block (the parallel mixdown, per branch): %.0f ns\n", (now_us() - t0) * 1000 / reps);
}

// ---- D ------------------------------------------------------------------
static void benchD()
{
    std::printf ("\n== D. silence detector cost\n");
    alignas(32) static float a[2][1024]; for (auto& c : a) for (auto& v : c) v = 0.0f;
    const int reps = 200000; volatile float pk;
    auto t0 = now_us();
    for (int r = 0; r < reps; ++r)
    {
        __m128 m = _mm_setzero_ps(); const __m128 mask = _mm_castsi128_ps (_mm_set1_epi32 (0x7fffffff));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 1024; i += 4) m = _mm_max_ps (m, _mm_and_ps (_mm_loadu_ps (&a[c][i]), mask));
        float t[4]; _mm_storeu_ps (t, m); pk = std::max (std::max (t[0], t[1]), std::max (t[2], t[3]));
    }
    std::printf ("peak scan of a stereo block (2 x 1024), SSE: %.0f ns\n", (now_us() - t0) * 1000 / reps);
}

int main()
{
    benchA(); benchB(); benchC(); benchD();
}
