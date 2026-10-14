#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Pitch detection for a plain sample (REVIEW-PLAN step 29): YIN on a few
// windows of the sample, the median of the confident ones. Offline only (the
// DETECT button), never on the audio thread.
namespace PitchDetect
{
struct Result
{
    double frequency = 0.0; // Hz; 0 when nothing steady was found
    double midiNote = 0.0;  // fractional MIDI note (A4 = 69), 0 when none
    bool found() const noexcept { return frequency > 0.0; }
};

// One window: the pitch in Hz, or 0 when the window is not clearly pitched.
inline double yin (const float* x, int n, double sampleRate, double minHz, double maxHz)
{
    const auto tauMin = juce::jmax (2, (int) std::floor (sampleRate / maxHz));
    const auto tauMax = juce::jmin (n / 2 - 1, (int) std::ceil (sampleRate / minHz));
    if (tauMax <= tauMin + 2)
        return 0.0;

    std::vector<double> d ((size_t) tauMax + 1, 0.0), cmnd ((size_t) tauMax + 1, 1.0);
    const auto span = n - tauMax;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i < span; ++i)
        {
            const double diff = (double) x[i] - (double) x[i + tau];
            sum += diff * diff;
        }
        d[(size_t) tau] = sum;
    }

    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += d[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
    }

    // First dip under the threshold, walked down to its bottom (an octave
    // error is the usual YIN failure, so the first dip wins, not the deepest).
    constexpr double threshold = 0.15;
    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t)
        if (cmnd[(size_t) t] < threshold)
        {
            while (t + 1 <= tauMax && cmnd[(size_t) t + 1] < cmnd[(size_t) t])
                ++t;
            tau = t;
            break;
        }
    if (tau < 0)
        return 0.0;

    // Parabolic refinement of the dip.
    double refined = (double) tau;
    if (tau > 1 && tau < tauMax)
    {
        const auto a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], c = cmnd[(size_t) tau + 1];
        const auto denominator = a - 2.0 * b + c;
        if (std::abs (denominator) > 1.0e-12)
            refined += 0.5 * (a - c) / denominator;
    }
    return refined > 0.0 ? sampleRate / refined : 0.0;
}

// The sample's pitch: windows of about 4 periods of the lowest note (at least
// 2048 samples), spread over the part with sound in it, the median of those
// that agree to within a semitone.
inline Result detect (const juce::AudioBuffer<float>& buffer, double sampleRate,
                      double minHz = 30.0, double maxHz = 2000.0)
{
    Result result;
    const auto length = buffer.getNumSamples();
    if (length < 256 || sampleRate <= 0.0 || buffer.getNumChannels() < 1)
        return result;

    // Mono, capped at the first 8 seconds: the pitch of a long recording
    // is that of its start, and the detector is quadratic in the window.
    const auto count = (int) juce::jmin ((double) length, 8.0 * sampleRate);
    std::vector<float> mono ((size_t) count, 0.0f);
    const auto channels = buffer.getNumChannels();
    for (int c = 0; c < channels; ++c)
        juce::FloatVectorOperations::add (mono.data(), buffer.getReadPointer (c), count);
    juce::FloatVectorOperations::multiply (mono.data(), 1.0f / (float) channels, count);

    // Where the sound is: from the first block over -40 dB of the peak.
    const auto peak = juce::FloatVectorOperations::findMaximum (mono.data(), count);
    const auto minimum = juce::FloatVectorOperations::findMinimum (mono.data(), count);
    const auto top = juce::jmax (peak, -minimum);
    if (top < 1.0e-4f)
        return result;
    int first = 0;
    for (; first < count; ++first)
        if (std::abs (mono[(size_t) first]) > top * 0.01f)
            break;

    const auto window = juce::jlimit (2048, 16384, (int) std::ceil (4.0 * sampleRate / minHz) / 2 * 2);
    if (count - first < window)
        return result;

    std::vector<double> found;
    constexpr int windows = 7;
    const auto room = count - first - window;
    for (int w = 0; w < windows; ++w)
    {
        const auto start = first + (room * w) / (windows - 1);
        // Quiet windows (a tail) do not vote.
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
            energy += (double) mono[(size_t) (start + i)] * mono[(size_t) (start + i)];
        if (std::sqrt (energy / window) < top * 0.02)
            continue;
        if (const auto hz = yin (mono.data() + start, window, sampleRate, minHz, maxHz); hz > 0.0)
            found.push_back (12.0 * std::log2 (hz / 440.0) + 69.0);
    }

    if (found.empty())
        return result;
    std::sort (found.begin(), found.end());
    const auto median = found[found.size() / 2];
    std::vector<double> agreeing;
    for (const auto note : found)
        if (std::abs (note - median) < 1.0)
            agreeing.push_back (note);
    // Too few windows agreeing means noise, not a note.
    if (agreeing.size() * 2 < found.size() || agreeing.size() < (found.size() < 3 ? 1u : 2u))
        return result;

    const auto note = agreeing[agreeing.size() / 2];
    result.midiNote = note;
    result.frequency = 440.0 * std::exp2 ((note - 69.0) / 12.0);
    return result;
}
}
