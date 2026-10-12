// OfflineStretch::resampleTo - band-limited resampling by windowed sinc.
//
// Output sample n sits at source position (n + 0.5) * r - 0.5, with
// r = oldLength / newLength, so both ends of the signal line up and the
// output length is exactly newLength. The kernel is a Kaiser-windowed sinc
// with 16 zero crossings each side (32 taps when not shrinking). When
// shrinking, the cutoff drops to newLength/oldLength of Nyquist and the
// kernel widens by the inverse, so nothing aliases. Samples outside the input
// count as zero; each output is divided by the sum of its (full) kernel so DC
// gain is exactly one.

#include "OfflineStretch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace OfflineStretch
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int    kZeroCrossings = 16;   // each side of the centre
constexpr int    kTableRes = 1024;      // table entries per zero crossing
constexpr double kKaiserBeta = 8.0;     // about -80 dB stop band

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = 0.25 * x * x;
    for (int k = 1; k < 60; ++k)
    {
        term *= q / ((double) k * (double) k);
        sum += term;
        if (term < 1e-17 * sum)
            break;
    }
    return sum;
}

// Kernel at unit cutoff, u in [-kZeroCrossings, kZeroCrossings]: sinc(u) * kaiser(u / kZeroCrossings).
// Stored for u >= 0 only (it is even), one guard entry at the end for interpolation.
struct KernelTable
{
    std::vector<double> t;
    KernelTable()
    {
        const int n = kZeroCrossings * kTableRes;
        t.resize ((size_t) n + 2, 0.0);
        const double i0b = besselI0 (kKaiserBeta);
        for (int i = 0; i <= n; ++i)
        {
            const double u = (double) i / (double) kTableRes;
            const double s = (i == 0) ? 1.0 : std::sin (kPi * u) / (kPi * u);
            const double r = u / (double) kZeroCrossings;
            const double w = besselI0 (kKaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            t[(size_t) i] = s * w;
        }
        t[(size_t) n + 1] = 0.0;
    }

    double at (double u) const noexcept // u >= 0
    {
        const double p = u * kTableRes;
        const size_t i = (size_t) p;
        if (i >= t.size() - 1)
            return 0.0;
        const double f = p - (double) i;
        return t[i] + f * (t[i + 1] - t[i]);
    }
};

const KernelTable& kernel()
{
    static const KernelTable k; // thread-safe init
    return k;
}
} // namespace

Audio resampleTo (const Audio& input, int newLength)
{
    Audio out;
    out.sampleRate = input.sampleRate;
    const int oldLength = input.numFrames();
    out.channels.assign (input.channels.size(), std::vector<float>());
    if (input.channels.empty() || newLength <= 0)
        return out;

    if (oldLength == 0)
    {
        for (auto& c : out.channels)
            c.assign ((size_t) newLength, 0.0f);
        return out;
    }

    if (newLength == oldLength)
    {
        out.channels = input.channels;
        return out;
    }

    const KernelTable& kt = kernel();
    const double r = (double) oldLength / (double) newLength; // source samples per output sample
    const double fc = std::min (1.0, 1.0 / r);                // cutoff relative to source Nyquist
    const double halfWidth = (double) kZeroCrossings / fc;    // kernel half width in source samples
    const int    reach = (int) std::ceil (halfWidth);

    for (size_t ch = 0; ch < input.channels.size(); ++ch)
    {
        const std::vector<float>& src = input.channels[ch];
        const int srcLen = (int) std::min<size_t> (src.size(), (size_t) oldLength);
        std::vector<float>& dst = out.channels[ch];
        dst.assign ((size_t) newLength, 0.0f);

        for (int n = 0; n < newLength; ++n)
        {
            const double pos = ((double) n + 0.5) * r - 0.5;
            const int centre = (int) std::floor (pos);
            const int i0 = centre - reach + 1;
            const int i1 = centre + reach;
            double acc = 0.0, wsum = 0.0;
            for (int i = i0; i <= i1; ++i)
            {
                const double u = std::fabs (((double) i - pos) * fc);
                if (u >= (double) kZeroCrossings)
                    continue;
                const double h = kt.at (u);
                wsum += h;
                if (i >= 0 && i < srcLen)
                    acc += h * (double) src[(size_t) i];
            }
            double v = (wsum > 1e-9) ? acc / wsum : 0.0;
            if (! std::isfinite (v))
                v = 0.0;
            dst[(size_t) n] = (float) v;
        }
    }
    return out;
}
} // namespace OfflineStretch
