// Paulstretch (Paul Nasca): long windows, magnitudes kept, phases randomised.
// Plain C++ (no JUCE). FFT: pffft.

#include "OfflineStretch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include "pffft.h"

namespace OfflineStretch
{
namespace
{
const double kPi = 3.14159265358979323846;

int pickWindowSize (double sampleRate, double stretch)
{
    // 0.25 s near 1x, growing with sqrt(stretch) to 1 s at 16x.
    const double seconds = std::min (1.0, 0.25 * std::sqrt (std::max (1.0, stretch)));
    const double target = std::max (256.0, seconds * sampleRate);
    int n = 512;
    while (n * 1.41421356 < target && n < (1 << 22))
        n *= 2;                                   // nearest power of two (log domain)
    return n;
}
} // namespace

Audio renderPaulstretch (const Audio& input, double lengthFactor, double pitchRatio)
{
    Audio out;
    out.sampleRate = input.sampleRate;
    const int inFrames = input.numFrames();
    const int numCh = (int) input.channels.size();
    if (numCh == 0 || inFrames <= 0)
        return out;

    lengthFactor = std::min (16.0, std::max (1.0 / 16.0, lengthFactor));
    pitchRatio   = std::min (16.0, std::max (1.0 / 16.0, pitchRatio));
    if (! (lengthFactor == lengthFactor) || ! (pitchRatio == pitchRatio))
        return out;

    const int targetLen = std::max (1, (int) std::llround ((double) inFrames * lengthFactor));
    const double stretch = lengthFactor * pitchRatio;
    const int stretchedLen = std::max (1, (int) std::llround ((double) inFrames * stretch));

    const int N = pickWindowSize (input.sampleRate, stretch);
    const int half = N / 2;
    const int hop = half;                         // synthesis hop (50% overlap)
    const double inHop = (double) hop / stretch;  // analysis hop

    PFFFT_Setup* setup = pffft_new_setup (N, PFFFT_REAL);
    if (setup == nullptr)
        return out;
    float* bufIn  = (float*) pffft_aligned_malloc ((size_t) N * sizeof (float));
    float* spec   = (float*) pffft_aligned_malloc ((size_t) N * sizeof (float));
    float* work   = (float*) pffft_aligned_malloc ((size_t) N * sizeof (float));
    float* timeB  = (float*) pffft_aligned_malloc ((size_t) N * sizeof (float));

    std::vector<float> win ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double x = -1.0 + 2.0 * (double) i / (double) N;
        win[(size_t) i] = (float) std::pow (std::max (0.0, 1.0 - x * x), 1.25);
    }

    std::vector<std::vector<float>> acc ((size_t) numCh, std::vector<float> ((size_t) stretchedLen, 0.0f));
    std::vector<float> wsq ((size_t) stretchedLen, 0.0f);
    std::vector<float> phC ((size_t) half + 1), phS ((size_t) half + 1);

    std::mt19937 rng (0x9E3779B9u);
    std::uniform_real_distribution<double> uni (0.0, 2.0 * kPi);

    const double invN = 1.0 / (double) N;
    for (int k = 0;; ++k)
    {
        if (cancelled())
            return Audio{};
        const double outCentre = (double) k * hop;
        if (outCentre >= (double) stretchedLen + 1.0)
            break;
        const long long inStart = (long long) std::llround ((double) k * inHop) - half;
        const long long outStart = (long long) k * hop - half;

        for (int b = 1; b < half; ++b)
        {
            const double p = uni (rng);
            phC[(size_t) b] = (float) std::cos (p);
            phS[(size_t) b] = (float) std::sin (p);
        }

        for (int c = 0; c < numCh; ++c)
        {
            const std::vector<float>& src = input.channels[(size_t) c];
            for (int i = 0; i < N; ++i)
            {
                const long long idx = inStart + i;
                const float s = (idx >= 0 && idx < inFrames) ? src[(size_t) idx] : 0.0f;
                bufIn[i] = s * win[(size_t) i];
            }
            pffft_transform_ordered (setup, bufIn, spec, work, PFFFT_FORWARD);

            spec[0] = 0.0f;                                  // DC removed
            spec[1] = std::fabs (spec[1]);                   // Nyquist magnitude
            for (int b = 1; b < half; ++b)
            {
                const float re = spec[2 * b], im = spec[2 * b + 1];
                const float mag = std::sqrt (re * re + im * im);
                spec[2 * b]     = mag * phC[(size_t) b];
                spec[2 * b + 1] = mag * phS[(size_t) b];
            }
            pffft_transform_ordered (setup, spec, timeB, work, PFFFT_BACKWARD);

            std::vector<float>& dst = acc[(size_t) c];
            for (int i = 0; i < N; ++i)
            {
                const long long o = outStart + i;
                if (o < 0 || o >= stretchedLen)
                    continue;
                dst[(size_t) o] += timeB[i] * (float) invN * win[(size_t) i];
            }
        }
        for (int i = 0; i < N; ++i)
        {
            const long long o = outStart + i;
            if (o >= 0 && o < stretchedLen)
                wsq[(size_t) o] += win[(size_t) i] * win[(size_t) i];
        }
    }

    pffft_aligned_free (bufIn); pffft_aligned_free (spec);
    pffft_aligned_free (work);  pffft_aligned_free (timeB);
    pffft_destroy_setup (setup);

    // Random-phase frames add in power: divide by sqrt of the window-square sum.
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < stretchedLen; ++i)
            acc[(size_t) c][(size_t) i] /= std::sqrt (std::max (wsq[(size_t) i], 0.05f));

    Audio stretched;
    stretched.sampleRate = input.sampleRate;
    stretched.channels = std::move (acc);

    Audio result;
    if (stretchedLen == targetLen)
        result = std::move (stretched);
    else
    {
        result = resampleTo (stretched, targetLen);
        if (result.numFrames() != targetLen || (int) result.channels.size() != numCh)
            return Audio {};
        result.sampleRate = input.sampleRate;
    }

    // Measured RMS match of the whole output to the input (all channels).
    double eIn = 0.0, eOut = 0.0;
    for (int c = 0; c < numCh; ++c)
    {
        for (float v : input.channels[(size_t) c]) eIn += (double) v * v;
        for (float v : result.channels[(size_t) c]) eOut += (double) v * v;
    }
    const double rmsIn = std::sqrt (eIn / ((double) inFrames * numCh));
    const double rmsOut = std::sqrt (eOut / ((double) targetLen * numCh));
    double gain = 1.0;
    if (rmsOut > 1.0e-12 && rmsIn > 1.0e-12)
        gain = std::min (1000.0, std::max (0.001, rmsIn / rmsOut));

    const int fade = std::min (targetLen, std::max (1, (int) (0.010 * input.sampleRate)));
    for (int c = 0; c < numCh; ++c)
    {
        std::vector<float>& d = result.channels[(size_t) c];
        for (int i = 0; i < targetLen; ++i)
        {
            float v = d[(size_t) i] * (float) gain;
            if (i >= targetLen - fade)
                v *= (float) (targetLen - 1 - i) / (float) fade;
            if (! std::isfinite (v)) v = 0.0f;
            d[(size_t) i] = v;
        }
    }
    return result;
}
} // namespace OfflineStretch
