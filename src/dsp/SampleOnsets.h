#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Offline onset (transient) detection for loaded samples. No JUCE dependency.
// It is meant to run once on the message thread when a sample is loaded.
namespace SampleOnsets
{
namespace detail
{
// Mixes one frame of up to two channels down to mono.
inline float mixFrame (const float* const* channels, int numChannels, int i) noexcept
{
    return numChannels >= 2 ? 0.5f * (channels[0][i] + channels[1][i]) : channels[0][i];
}

// Last frame at or before position (within radius) where the mix changes sign, or -1.
inline int zeroCrossingBefore (const float* const* channels, int numChannels, int numSamples,
                               int position, int radius) noexcept
{
    const auto top = std::min (position, numSamples - 1);
    const auto bottom = std::max (1, position - radius);

    for (auto i = top; i >= bottom; --i)
    {
        const auto a = mixFrame (channels, numChannels, i - 1);
        const auto b = mixFrame (channels, numChannels, i);

        if ((a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f))
            return i;
    }

    return -1;
}
} // namespace detail

// Frame positions where a new sound starts, ascending. The first may be 0 if the sample starts loud.
// sensitivity 0..1 (0.5 default): higher finds more and quieter onsets.
inline std::vector<int> find (const float* const* channels, int numChannels, int numSamples,
                              double sampleRate, float sensitivity = 0.5f)
{
    std::vector<int> result;

    if (channels == nullptr || channels[0] == nullptr || numChannels < 1 || numSamples <= 0 || sampleRate <= 0.0)
        return result;

    const auto nc = std::min (numChannels, 2);
    const auto sens = std::clamp (sensitivity, 0.0f, 1.0f);

    // Hops of about 5 ms: fine enough to time an attack, coarse enough to be cheap.
    const auto hop = std::max (8, (int) std::lround (0.005 * sampleRate));
    const auto numHops = (numSamples + hop - 1) / hop;

    constexpr double floorDb = -60.0;     // Plain level below which nothing counts as a sound.
    constexpr double minDiffDb = -80.0;   // Lower clamp of the high-passed level, keeps logs finite.
    constexpr int baseHops = 4;           // Hops averaged as the "before" level.
    constexpr int localHops = 20;         // Half-width of the adaptive threshold window (about 100 ms).

    // Per hop: plain level (for the floor) and first-difference level (high-pass, so attacks stand out
    // from slow swells and low rumble).
    std::vector<float> plainDb ((size_t) numHops), diffDb ((size_t) numHops);
    auto prev = detail::mixFrame (channels, nc, 0);

    for (auto h = 0; h < numHops; ++h)
    {
        const auto start = h * hop;
        const auto end = std::min (numSamples, start + hop);
        double plain = 0.0, diff = 0.0;

        for (auto i = start; i < end; ++i)
        {
            const auto m = detail::mixFrame (channels, nc, i);
            const auto d = m - prev;
            plain += (double) m * m;
            diff += (double) d * d;
            prev = m;
        }

        const auto count = (double) (end - start);
        plainDb[(size_t) h] = (float) (10.0 * std::log10 (plain / count + 1.0e-12));
        diffDb[(size_t) h] = (float) std::max (minDiffDb, 10.0 * std::log10 (diff / count + 1.0e-12));
    }

    // Novelty: how far this hop's level rose above the mean of the few hops before it.
    // Hops before the start count as silence so a loud first hop is an onset.
    std::vector<float> novelty ((size_t) numHops, 0.0f);

    for (auto h = 0; h < numHops; ++h)
    {
        const auto nextPlain = h + 1 < numHops ? plainDb[(size_t) h + 1] : plainDb[(size_t) h];

        if (std::max (plainDb[(size_t) h], nextPlain) < floorDb)
            continue;

        double base = 0.0;

        for (auto k = 1; k <= baseHops; ++k)
            base += h - k >= 0 ? diffDb[(size_t) (h - k)] : minDiffDb;

        base /= baseHops;
        novelty[(size_t) h] = (float) std::max (0.0, (double) diffDb[(size_t) h] - base);
    }

    // Adaptive threshold: local mean novelty plus a margin that shrinks as sensitivity rises.
    std::vector<double> prefix ((size_t) numHops + 1, 0.0);

    for (auto h = 0; h < numHops; ++h)
        prefix[(size_t) h + 1] = prefix[(size_t) h] + novelty[(size_t) h];

    const auto margin = 11.0 - 8.0 * (double) sens;
    const auto minGapHops = std::max (1, (int) std::lround (0.05 / (hop / sampleRate)));
    std::vector<int> peaks;

    for (auto h = 0; h < numHops; ++h)
    {
        const auto n = novelty[(size_t) h];

        if (n <= 0.0f)
            continue;

        const auto lo = std::max (0, h - localHops);
        const auto hi = std::min (numHops, h + localHops + 1);
        const auto localMean = (prefix[(size_t) hi] - prefix[(size_t) lo]) / (double) (hi - lo);

        if ((double) n < localMean + margin)
            continue;

        const auto before = h > 0 ? novelty[(size_t) h - 1] : 0.0f;
        const auto after = h + 1 < numHops ? novelty[(size_t) h + 1] : 0.0f;

        if (n < before || n <= after)
            continue; // Not a local peak.

        // Keep only the stronger of two peaks closer than the minimum gap.
        if (! peaks.empty() && h - peaks.back() < minGapHops)
        {
            if (n > novelty[(size_t) peaks.back()])
                peaks.back() = h;

            continue;
        }

        peaks.push_back (h);
    }

    // Refine each peak back to where the rise actually began.
    const auto zeroRadius = std::max (1, (int) std::lround (0.002 * sampleRate));
    result.reserve (peaks.size());

    for (const auto h : peaks)
    {
        // Search from one hop before the peak hop, so the start of a rise that straddles hops is not missed.
        const auto from = std::max (0, (h - 1) * hop);
        const auto to = std::min (numSamples, (h + 2) * hop);
        float peak = 0.0f;

        for (auto i = from; i < to; ++i)
            peak = std::max (peak, std::abs (detail::mixFrame (channels, nc, i)));

        // First frame that is clearly part of the attack (a fraction of the local peak).
        const auto threshold = 0.1f * peak;
        auto pos = from;

        while (pos < to - 1 && std::abs (detail::mixFrame (channels, nc, pos)) < threshold)
            ++pos;

        // Snap to a zero crossing just before it so a slice starts without a click.
        const auto zc = detail::zeroCrossingBefore (channels, nc, numSamples, pos, zeroRadius);

        if (zc >= 0)
            pos = zc;

        if (result.empty() || pos > result.back())
            result.push_back (pos);
    }

    return result;
}

// The nearest onset at or before position, or -1 when there is none.
inline int atOrBefore (const std::vector<int>& onsets, int position) noexcept
{
    const auto it = std::upper_bound (onsets.begin(), onsets.end(), position);
    return it == onsets.begin() ? -1 : *(it - 1);
}

// The nearest zero crossing of the channel mix within radius frames of position
// (position itself when none).
inline int nearestZeroCrossing (const float* const* channels, int numChannels, int numSamples,
                                int position, int radius) noexcept
{
    if (channels == nullptr || channels[0] == nullptr || numSamples < 2)
        return position;

    const auto nc = std::min (numChannels, 2);

    const auto crosses = [&] (int i)
    {
        if (i < 1 || i >= numSamples)
            return false;

        const auto a = detail::mixFrame (channels, nc, i - 1);
        const auto b = detail::mixFrame (channels, nc, i);
        return (a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f);
    };

    for (auto d = 0; d <= radius; ++d)
    {
        if (crosses (position - d))
            return position - d;

        if (crosses (position + d))
            return position + d;
    }

    return position;
}
} // namespace SampleOnsets
