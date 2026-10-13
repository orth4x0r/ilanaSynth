#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Fractional-position sample reads for the sample player, with selectable quality.
// Header-only, allocation-free and free of JUCE types, so it is safe to call from the audio thread.
// Out-of-range taps read the nearest edge sample (clamp), like the original linear read.
namespace SampleInterpolation
{
enum class Quality { Linear = 0, Cubic = 1, Sinc = 2, SincAntiAlias = 3 };

namespace detail
{
constexpr int kHalfTaps = 8;                         // Taps on each side of the read position.
constexpr int kTaps = 2 * kHalfTaps;                 // 16 taps in total.
constexpr int kPhases = 256;                         // Sub-sample positions in the polyphase table.
constexpr int kKernelSize = kHalfTaps * kPhases;     // Kernel samples from 0 to 8 source samples.
constexpr double kKaiserBeta = 8.6;
constexpr double kMaxRatio = 4.0;                    // Caps the stretched kernel at 64 taps.

struct Tables
{
    // Polyphase taps for a read at index + phase / kPhases. Tap j sits at source index (index - 7 + j).
    // One extra phase lets the linear blend between phases read phase + 1 without a branch.
    std::array<std::array<float, kTaps>, kPhases + 1> phases {};

    // The same kernel sampled at 1 / kPhases spacing for |x| in [0, 8], used when it is stretched.
    // Two guard entries sit past the end so interpolation never reads out of bounds.
    std::array<float, kKernelSize + 2> kernel {};
};

inline double besselI0 (double x) noexcept
{
    // Power series, which converges quickly for the small arguments a Kaiser window needs.
    auto sum = 1.0;
    auto term = 1.0;
    const auto q = x * x * 0.25;

    for (int k = 1; k < 60; ++k)
    {
        term *= q / (double) (k * k);
        sum += term;

        if (term < 1.0e-18 * sum)
            break;
    }

    return sum;
}

inline double kaiserSinc (double x) noexcept
{
    // Kaiser-windowed sinc with its cutoff at Nyquist, zero outside the 8 sample half width.
    const auto r = x / (double) kHalfTaps;

    if (std::abs (r) >= 1.0)
        return 0.0;

    const auto pix = 3.14159265358979323846 * x;
    const auto s = std::abs (x) < 1.0e-12 ? 1.0 : std::sin (pix) / pix;
    const auto window = besselI0 (kKaiserBeta * std::sqrt (1.0 - r * r)) / besselI0 (kKaiserBeta);
    return s * window;
}

inline const Tables& getTables() noexcept
{
    // Built once on first use. Thread-safe static initialisation means later calls never allocate or lock.
    static const Tables tables = []
    {
        Tables t;

        for (int k = 0; k < kKernelSize + 2; ++k)
            t.kernel[(size_t) k] = (float) kaiserSinc ((double) k / (double) kPhases);

        for (int p = 0; p <= kPhases; ++p)
        {
            const auto frac = (double) p / (double) kPhases;
            auto sum = 0.0;

            for (int j = 0; j < kTaps; ++j)
            {
                const auto x = (double) (j - (kHalfTaps - 1)) - frac;
                sum += kaiserSinc (x);
            }

            // Each phase sums to 1 so DC passes with exactly unity gain. Blending two phases keeps that.
            for (int j = 0; j < kTaps; ++j)
            {
                const auto x = (double) (j - (kHalfTaps - 1)) - frac;
                t.phases[(size_t) p][(size_t) j] = (float) (kaiserSinc (x) / sum);
            }
        }

        return t;
    }();

    return tables;
}

inline int clampIndex (int i, int n) noexcept
{
    return i < 0 ? 0 : (i > n - 1 ? n - 1 : i);
}

inline float readLinear (const float* data, int n, double position) noexcept
{
    // Clamp in double first so huge positions cannot overflow the int conversion.
    const auto index = (int) std::min (std::max (std::floor (position), 0.0), (double) (n - 1));
    const auto next = std::min (index + 1, n - 1);
    const auto frac = (float) (position - index);
    return data[index] + (data[next] - data[index]) * frac;
}

inline float readCubic (const float* data, int n, double position) noexcept
{
    // Far outside the data every tap is an edge sample anyway, so limit the range before converting.
    const auto p = std::min (std::max (position, -8.0), (double) n + 8.0);
    const auto i = (int) std::floor (p);
    const auto f = (float) (p - i);

    float y0, y1, y2, y3;

    if (i >= 1 && i + 2 < n)
    {
        y0 = data[i - 1];
        y1 = data[i];
        y2 = data[i + 1];
        y3 = data[i + 2];
    }
    else
    {
        y0 = data[clampIndex (i - 1, n)];
        y1 = data[clampIndex (i, n)];
        y2 = data[clampIndex (i + 1, n)];
        y3 = data[clampIndex (i + 2, n)];
    }

    // Catmull-Rom spline: a 3rd-order Hermite form whose tangents come from the neighbouring points.
    const auto a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const auto b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto c = -0.5f * y0 + 0.5f * y2;
    return ((a * f + b) * f + c) * f + y1;
}

inline float readSinc (const float* data, int n, double position) noexcept
{
    const auto& tables = getTables();

    const auto p = std::min (std::max (position, -32.0), (double) n + 32.0);
    const auto i = (int) std::floor (p);
    const auto fine = (p - i) * (double) kPhases;
    const auto phase = std::min ((int) fine, kPhases - 1);
    const auto w = (float) (fine - phase);

    const auto& t0 = tables.phases[(size_t) phase];
    const auto& t1 = tables.phases[(size_t) phase + 1];

    const auto first = i - (kHalfTaps - 1);
    float acc = 0.0f;

    if (first >= 0 && first + kTaps <= n)
    {
        // Fast path: every tap is inside the data, so no clamping is needed.
        const auto* src = data + first;

        for (int j = 0; j < kTaps; ++j)
            acc += (t0[(size_t) j] + (t1[(size_t) j] - t0[(size_t) j]) * w) * src[j];
    }
    else
    {
        for (int j = 0; j < kTaps; ++j)
            acc += (t0[(size_t) j] + (t1[(size_t) j] - t0[(size_t) j]) * w) * data[clampIndex (first + j, n)];
    }

    return acc;
}

inline float readSincStretched (const float* data, int n, double position, double ratio) noexcept
{
    const auto& tables = getTables();

    // Stretching the kernel by the ratio lowers its cutoff to 1 / ratio of Nyquist, which filters the
    // frequencies that would alias when the read steps over several source samples at a time.
    const auto p = std::min (std::max (position, -64.0), (double) n + 64.0);
    const auto reach = (double) kHalfTaps * ratio;
    const auto scale = (double) kPhases / ratio;

    const auto lo = (int) std::ceil (p - reach);
    const auto hi = (int) std::floor (p + reach);
    const auto inside = lo >= 0 && hi < n;

    float acc = 0.0f;
    float weightSum = 0.0f;

    for (int s = lo; s <= hi; ++s)
    {
        // Look the kernel up at a fractional table position and blend the two neighbouring entries.
        const auto u = std::abs ((double) s - p) * scale;

        if (u >= (double) kKernelSize)
            continue;

        const auto k = (int) u;
        const auto w = (float) (u - k);
        const auto weight = tables.kernel[(size_t) k] + (tables.kernel[(size_t) k + 1] - tables.kernel[(size_t) k]) * w;
        const auto sample = inside ? data[s] : data[clampIndex (s, n)];

        acc += weight * sample;
        weightSum += weight;
    }

    // Dividing by the weights actually used gives unity DC gain whatever the ratio or sub-sample offset.
    return weightSum != 0.0f ? acc / weightSum : 0.0f;
}
} // namespace detail

// Reads data at position (index + frac). Out-of-range taps read the nearest edge sample (clamp).
// ratio = source samples advanced per output sample (absolute value), used only by SincAntiAlias.
inline float read (const float* data, int numSamples, double position, Quality quality, double ratio) noexcept
{
    if (data == nullptr || numSamples <= 0)
        return 0.0f;

    switch (quality)
    {
        case Quality::Cubic:
            return detail::readCubic (data, numSamples, position);

        case Quality::Sinc:
            return detail::readSinc (data, numSamples, position);

        case Quality::SincAntiAlias:
        {
            // A NaN ratio fails the comparison and falls back to the plain sinc read.
            const auto r = std::abs (ratio);

            if (! (r > 1.0))
                return detail::readSinc (data, numSamples, position);

            return detail::readSincStretched (data, numSamples, position, std::min (r, detail::kMaxRatio));
        }

        case Quality::Linear:
        default:
            return detail::readLinear (data, numSamples, position);
    }
}
} // namespace SampleInterpolation
