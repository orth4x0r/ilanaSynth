// OfflineStretch::renderPghi - phase vocoder time stretch whose synthesis
// phases are rebuilt from the magnitudes alone by Phase Gradient Heap
// Integration (Prusa & Holighaus, "Phase Vocoder Done Right", 2022; Prusa,
// Balazs, Soendergaard, IEEE/ACM TASLP 2017).
//
// How it works
//  * Truncated Gaussian window, FFT size M ~ 85 ms (4096 at 48 kHz), synthesis
//    hop Hs = M/8 (redundancy 8). Every synthesis frame m sits at output
//    sample m*Hs and takes its magnitudes from the input at m*Hs/rho
//    (rho = stretch factor), so only the magnitudes are stretched.
//  * For a Gaussian window g(t) = exp(-pi t^2 / lambda), sigma^2 = lambda /
//    (2 pi), the phase of the STFT (taken relative to the window centre) obeys
//        d(phase)/dt     = w + s_w / sigma^2     (instantaneous frequency)
//        d(phase)/dw     =    - sigma^2 * s_t    (group delay)
//    with s = log magnitude. Both derivatives come from finite differences of
//    the synthesis spectrogram.
//  * Frames are done in order (real-time PGHI, one frame of look-ahead): the
//    previous frame's bins and the current frame's bins sit in one max-heap
//    ordered by magnitude. The strongest bin spreads its phase to its time
//    neighbour (trapezoid on the instantaneous frequency) and its frequency
//    neighbours (trapezoid on the group delay). Bins under the tolerance get
//    random phase; an empty heap is re-seeded with the largest bin left over.
//  * Stereo: the PGHI phase is built from the power sum of all channels. Each
//    channel keeps its own magnitude and its phase relative to the channel
//    sum (the inter-channel phase), so the image does not wobble. Mono stays mono.
//  * Pitch: pitchRatio > 1 resamples the input first (shorter), pitchRatio < 1
//    resamples after stretching; either way the big signal is never longer
//    than the final one.
// Memory is O(output length + FFT size): no whole-spectrogram arrays.

#include "OfflineStretch.h"

#include "pffft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace OfflineStretch
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

constexpr double kWindowEdge = 1.0e-4;     // Gaussian value at the window edge (truncation)
constexpr double kTolerance = 1.0e-5;      // -100 dB below a full-scale sine bin
constexpr double kMaxFreqOffsetBins = 10.0; // clamp of s_w / sigma^2, in bins
constexpr int    kRedundancy = 8;

// Aligned float buffer with RAII
struct AlignedBuf
{
    float* p = nullptr;
    explicit AlignedBuf (int n)
    {
        p = (float*) pffft_aligned_malloc ((size_t) n * sizeof (float));
        if (p != nullptr)
            std::memset (p, 0, (size_t) n * sizeof (float));
    }
    ~AlignedBuf() { if (p != nullptr) pffft_aligned_free (p); }
    AlignedBuf (const AlignedBuf&) = delete;
    AlignedBuf& operator= (const AlignedBuf&) = delete;
};

// Small deterministic noise source (random phases, repeatable renders)
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    double phase() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return ((double) (s >> 8) / 16777216.0) * kTwoPi - kPi;
    }
};

inline double wrapPhase (double p) noexcept { return std::remainder (p, kTwoPi); }

int chooseFftSize (double sampleRate) noexcept
{
    double target = 4096.0 * sampleRate / 48000.0;
    if (! (target > 0.0))
        target = 4096.0;
    int m = 1024;
    while (m < 16384 && (double) m * 1.41421356 < target)
        m *= 2;
    return m;
}

// One analysed frame: centred-phase spectrum per channel + magnitudes.
struct Frame
{
    std::vector<float> re, im; // [channel * K + k]
    std::vector<float> mag;    // sqrt of the channel power sum
    std::vector<float> logMag;
};

struct HeapItem
{
    float mag;
    int   bin;
    bool  fromPrev;
};
struct HeapLess
{
    bool operator() (const HeapItem& a, const HeapItem& b) const noexcept { return a.mag < b.mag; }
};

// Time-stretches by targetLen / input length with PGHI. input must be non-empty.
Audio stretchCore (const Audio& input, int targetLen)
{
    Audio out;
    out.sampleRate = input.sampleRate;
    const int nch = (int) input.channels.size();
    const int inLen = input.numFrames();
    out.channels.assign ((size_t) nch, std::vector<float>());

    const int M = chooseFftSize (input.sampleRate);
    const int K = M / 2 + 1;
    const double rho = (double) targetLen / (double) inLen;
    const int Hs = (rho >= 1.0) ? M / kRedundancy
                                : std::max (2, (int) std::lround (rho * (double) M / kRedundancy));
    const double anaStep = (double) Hs / rho; // input samples per synthesis frame
    const int P = (M / 2) / Hs;
    const int mFirst = -P;
    const int mLast = (targetLen + Hs - 1) / Hs + P;

    PFFFT_Setup* setup = pffft_new_setup (M, PFFFT_REAL);
    AlignedBuf bufIn (M), bufOut (M), bufWork (M);
    if (setup == nullptr || bufIn.p == nullptr || bufOut.p == nullptr || bufWork.p == nullptr)
    {
        if (setup != nullptr)
            pffft_destroy_setup (setup);
        return Audio{};
    }

    // Truncated Gaussian, centred on M/2: g = exp(-pi t^2 / lambda)
    const double lambda = kPi * (double) M * (double) M / (4.0 * std::log (1.0 / kWindowEdge));
    const double sigma2 = lambda / kTwoPi;
    std::vector<float> win ((size_t) M);
    double sumW = 0.0;
    for (int n = 0; n < M; ++n)
    {
        const double t = (double) (n - M / 2);
        win[(size_t) n] = (float) std::exp (-kPi * t * t / lambda);
        sumW += win[(size_t) n];
    }

    float peak = 0.0f;
    for (const auto& c : input.channels)
        for (int i = 0; i < inLen && i < (int) c.size(); ++i)
            peak = std::max (peak, std::fabs (c[(size_t) i]));
    if (! (peak > 0.0f) || ! std::isfinite (peak))
    {
        pffft_destroy_setup (setup);
        for (auto& c : out.channels)
            c.assign ((size_t) targetLen, 0.0f);
        return out;
    }
    const double tolAbs = kTolerance * 0.5 * (double) peak * sumW;
    const double logFloor = 1e-3 * tolAbs;
    const double dw = kTwoPi / (double) M;

    // Output accumulators; sample p of the output lives at index p + M.
    const size_t accLen = (size_t) targetLen + 2 * (size_t) M + (size_t) Hs;
    std::vector<std::vector<float>> acc ((size_t) nch, std::vector<float> (accLen, 0.0f));
    std::vector<float> wsum (accLen, 0.0f);

    // --- analysis of one frame (window centred on input sample m*anaStep) ---
    auto analyse = [&] (int m, Frame& f)
    {
        f.re.assign ((size_t) nch * K, 0.0f);
        f.im.assign ((size_t) nch * K, 0.0f);
        f.mag.assign ((size_t) K, 0.0f);
        f.logMag.assign ((size_t) K, 0.0f);
        const long long centre = std::llround ((double) m * anaStep);
        const long long start = centre - M / 2;
        for (int c = 0; c < nch; ++c)
        {
            const std::vector<float>& x = input.channels[(size_t) c];
            const long long xl = (long long) x.size();
            for (int n = 0; n < M; ++n)
            {
                const long long i = start + n;
                bufIn.p[n] = (i >= 0 && i < xl && i < inLen) ? x[(size_t) i] * win[(size_t) n] : 0.0f;
            }
            pffft_transform_ordered (setup, bufIn.p, bufOut.p, bufWork.p, PFFFT_FORWARD);
            float* re = f.re.data() + (size_t) c * K;
            float* im = f.im.data() + (size_t) c * K;
            re[0] = bufOut.p[0];
            re[K - 1] = bufOut.p[1];
            for (int k = 1; k < K - 1; ++k)
            {
                // (-1)^k moves the phase reference from the frame start to the window centre
                const float sgn = (k & 1) ? -1.0f : 1.0f;
                re[k] = sgn * bufOut.p[2 * k];
                im[k] = sgn * bufOut.p[2 * k + 1];
            }
            for (int k = 0; k < K; ++k)
                f.mag[(size_t) k] += re[k] * re[k] + im[k] * im[k];
        }
        for (int k = 0; k < K; ++k)
        {
            f.mag[(size_t) k] = std::sqrt (f.mag[(size_t) k]);
            f.logMag[(size_t) k] = (float) std::log ((double) f.mag[(size_t) k] + logFloor);
        }
    };

    Frame fPrev, fCur, fNext;
    bool havePrev = false;
    std::vector<double> phase ((size_t) K), phasePrev ((size_t) K, 0.0);
    std::vector<double> wi ((size_t) K), wiPrev ((size_t) K, 0.0), gd ((size_t) K);
    std::vector<unsigned char> done ((size_t) K);
    std::vector<HeapItem> heap;
    heap.reserve ((size_t) 2 * K);
    std::vector<int> order;
    Rng rng;
    const double maxOffset = kMaxFreqOffsetBins * dw;
    const double maxDelay = 0.5 * (double) M;

    analyse (mFirst, fCur);
    for (int m = mFirst; m <= mLast; ++m)
    {
        const bool haveNext = (m < mLast);
        if (haveNext)
            analyse (m + 1, fNext);

        const std::vector<float>& mag = fCur.mag;
        const std::vector<float>& lg = fCur.logMag;

        // Phase gradients of the current frame
        for (int k = 0; k < K; ++k)
        {
            const int kp = std::min (k + 1, K - 1), km = std::max (k - 1, 0);
            double off = 0.0;
            if (kp != km)
                off = ((double) lg[(size_t) kp] - (double) lg[(size_t) km]) / ((double) (kp - km) * dw) / sigma2;
            off = std::max (-maxOffset, std::min (maxOffset, off));
            wi[(size_t) k] = (double) k * dw + off;

            double st = 0.0;
            const float* lp = havePrev ? fPrev.logMag.data() : nullptr;
            const float* ln = haveNext ? fNext.logMag.data() : nullptr;
            if (lp != nullptr && ln != nullptr)
                st = ((double) ln[k] - (double) lp[k]) / (2.0 * Hs);
            else if (ln != nullptr)
                st = ((double) ln[k] - (double) lg[(size_t) k]) / (double) Hs;
            else if (lp != nullptr)
                st = ((double) lg[(size_t) k] - (double) lp[k]) / (double) Hs;
            gd[(size_t) k] = std::max (-maxDelay, std::min (maxDelay, -sigma2 * st));
        }

        // Heap integration
        std::fill (done.begin(), done.end(), (unsigned char) 0);
        int pending = 0;
        for (int k = 0; k < K; ++k)
            if ((double) mag[(size_t) k] > tolAbs)
                ++pending;
        heap.clear();
        if (havePrev)
            for (int k = 0; k < K; ++k)
                if ((double) fPrev.mag[(size_t) k] > tolAbs)
                    heap.push_back ({ fPrev.mag[(size_t) k], k, true });
        std::make_heap (heap.begin(), heap.end(), HeapLess());
        bool orderReady = false;
        size_t orderPos = 0;

        while (pending > 0)
        {
            if (heap.empty())
            {
                // Re-seed with the strongest bin that has no phase yet
                if (! orderReady)
                {
                    order.clear();
                    for (int k = 0; k < K; ++k)
                        if ((double) mag[(size_t) k] > tolAbs)
                            order.push_back (k);
                    std::sort (order.begin(), order.end(),
                               [&] (int a, int b) { return mag[(size_t) a] > mag[(size_t) b]; });
                    orderReady = true;
                    orderPos = 0;
                }
                while (orderPos < order.size() && done[(size_t) order[orderPos]])
                    ++orderPos;
                if (orderPos >= order.size())
                    break;
                const int k = order[orderPos];
                phase[(size_t) k] = rng.phase();
                done[(size_t) k] = 1;
                --pending;
                heap.push_back ({ mag[(size_t) k], k, false });
                std::push_heap (heap.begin(), heap.end(), HeapLess());
                continue;
            }

            std::pop_heap (heap.begin(), heap.end(), HeapLess());
            const HeapItem it = heap.back();
            heap.pop_back();
            const int k = it.bin;

            if (it.fromPrev)
            {
                // Same bin, one frame later: trapezoid on the instantaneous frequency
                if (! done[(size_t) k] && (double) mag[(size_t) k] > tolAbs)
                {
                    phase[(size_t) k] = wrapPhase (phasePrev[(size_t) k]
                                                   + 0.5 * (double) Hs * (wiPrev[(size_t) k] + wi[(size_t) k]));
                    done[(size_t) k] = 1;
                    --pending;
                    heap.push_back ({ mag[(size_t) k], k, false });
                    std::push_heap (heap.begin(), heap.end(), HeapLess());
                }
            }
            else
            {
                // Neighbouring bins of this frame: trapezoid on the group delay
                const int nb[2] = { k + 1, k - 1 };
                const double sgn[2] = { 1.0, -1.0 };
                for (int j = 0; j < 2; ++j)
                {
                    const int n = nb[j];
                    if (n < 0 || n >= K || done[(size_t) n] || ! ((double) mag[(size_t) n] > tolAbs))
                        continue;
                    phase[(size_t) n] = wrapPhase (phase[(size_t) k]
                                                   + sgn[j] * 0.5 * dw * (gd[(size_t) k] + gd[(size_t) n]));
                    done[(size_t) n] = 1;
                    --pending;
                    heap.push_back ({ mag[(size_t) n], n, false });
                    std::push_heap (heap.begin(), heap.end(), HeapLess());
                }
            }
        }
        for (int k = 0; k < K; ++k)
            if (! done[(size_t) k])
                phase[(size_t) k] = rng.phase();

        // Synthesis: new phase on the channel-sum reference, channels keep their relative phase
        const long long outStart = (long long) m * Hs - M / 2 + M; // index into acc
        for (int c = 0; c < nch; ++c)
        {
            const float* re = fCur.re.data() + (size_t) c * K;
            const float* im = fCur.im.data() + (size_t) c * K;
            auto emit = [&] (int k, double yr, double yi)
            {
                if (k == 0)
                    bufIn.p[0] = (float) yr;
                else if (k == K - 1)
                    bufIn.p[1] = (float) yr;
                else
                {
                    const float sgn = (k & 1) ? -1.0f : 1.0f;
                    bufIn.p[2 * k] = sgn * (float) yr;
                    bufIn.p[2 * k + 1] = sgn * (float) yi;
                }
            };
            for (int k = 0; k < K; ++k)
            {
                const double cr = std::cos (phase[(size_t) k]), ci = std::sin (phase[(size_t) k]);
                double yr = 0.0, yi = 0.0;
                if (nch == 1)
                {
                    const double a = std::sqrt ((double) re[k] * re[k] + (double) im[k] * im[k]);
                    yr = a * cr;
                    yi = a * ci;
                }
                else
                {
                    // reference = channel sum, or the strongest channel if the sum cancels
                    double sr = 0.0, si = 0.0, pw = 0.0, best = -1.0;
                    int bestC = 0;
                    for (int cc = 0; cc < nch; ++cc)
                    {
                        const double a = fCur.re[(size_t) cc * K + k], b = fCur.im[(size_t) cc * K + k];
                        sr += a; si += b;
                        const double p = a * a + b * b;
                        pw += p;
                        if (p > best) { best = p; bestC = cc; }
                    }
                    if (sr * sr + si * si < 0.05 * pw)
                    {
                        sr = fCur.re[(size_t) bestC * K + k];
                        si = fCur.im[(size_t) bestC * K + k];
                    }
                    const double rm = std::sqrt (sr * sr + si * si);
                    if (rm > 0.0)
                    {
                        // y = e^{i phase} * x * conj(ref) / |ref|
                        const double xr = re[k], xi = im[k];
                        const double dr = (xr * sr + xi * si) / rm;
                        const double di = (xi * sr - xr * si) / rm;
                        yr = cr * dr - ci * di;
                        yi = cr * di + ci * dr;
                    }
                }
                emit (k, yr, yi);
            }
            pffft_transform_ordered (setup, bufIn.p, bufOut.p, bufWork.p, PFFFT_BACKWARD);
            float* a = acc[(size_t) c].data();
            const float invM = 1.0f / (float) M;
            for (int n = 0; n < M; ++n)
            {
                const long long idx = outStart + n;
                if (idx >= 0 && (size_t) idx < accLen)
                    a[idx] += bufOut.p[n] * win[(size_t) n] * invM;
            }
        }
        for (int n = 0; n < M; ++n)
        {
            const long long idx = outStart + n;
            if (idx >= 0 && (size_t) idx < accLen)
                wsum[(size_t) idx] += win[(size_t) n] * win[(size_t) n];
        }

        // roll the frames
        phasePrev.swap (phase);
        wiPrev.swap (wi);
        std::swap (fPrev, fCur);
        std::swap (fCur, fNext);
        havePrev = true;
    }

    // Normalise by the window power sum and cut out [0, targetLen)
    float wMax = 0.0f;
    for (float v : wsum)
        wMax = std::max (wMax, v);
    const float wFloor = std::max (1e-6f, 1e-3f * wMax);
    for (int c = 0; c < nch; ++c)
    {
        std::vector<float>& dst = out.channels[(size_t) c];
        dst.resize ((size_t) targetLen);
        const float* a = acc[(size_t) c].data();
        for (int i = 0; i < targetLen; ++i)
        {
            const size_t idx = (size_t) i + (size_t) M;
            float v = a[idx] / std::max (wsum[idx], wFloor);
            dst[(size_t) i] = std::isfinite (v) ? v : 0.0f;
        }
    }
    pffft_destroy_setup (setup);
    return out;
}
} // namespace

Audio renderPghi (const Audio& input, double lengthFactor, double pitchRatio)
{
    try
    {
        if (input.channels.empty() || input.numFrames() <= 0)
            return Audio{};
        if (! std::isfinite (lengthFactor)) lengthFactor = 1.0;
        if (! std::isfinite (pitchRatio)) pitchRatio = 1.0;
        lengthFactor = std::max (1.0 / 16.0, std::min (16.0, lengthFactor));
        pitchRatio = std::max (1.0 / 16.0, std::min (16.0, pitchRatio));

        // Make every channel the same length
        Audio in;
        in.sampleRate = input.sampleRate;
        in.channels = input.channels;
        const size_t len = in.channels[0].size();
        for (auto& c : in.channels)
        {
            c.resize (len, 0.0f);
            for (float& v : c)
                if (! std::isfinite (v))
                    v = 0.0f;
        }

        const int inLen = (int) len;
        const int outLen = (int) std::llround ((double) inLen * lengthFactor);
        if (outLen <= 0)
        {
            Audio e;
            e.sampleRate = input.sampleRate;
            e.channels.assign (in.channels.size(), std::vector<float>());
            return e;
        }

        if (std::fabs (pitchRatio - 1.0) < 1e-9)
            return stretchCore (in, outLen);

        if (pitchRatio > 1.0)
        {
            // shrink first (pitch up), then stretch straight to the final length
            const int shortLen = std::max (1, (int) std::llround ((double) inLen / pitchRatio));
            return stretchCore (resampleTo (in, shortLen), outLen);
        }

        // stretch to length * pitch (shorter than final), then resample up
        const int midLen = std::max (1, (int) std::llround ((double) inLen * lengthFactor * pitchRatio));
        Audio mid = stretchCore (in, midLen);
        if (mid.numFrames() != midLen)
            return Audio{};
        return resampleTo (mid, outLen);
    }
    catch (...)
    {
        return Audio{};
    }
}
} // namespace OfflineStretch
