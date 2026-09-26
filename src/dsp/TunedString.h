#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

// A lossy string loop for sympathetic resonance: an integer delay, a Thiran
// allpass for the fractional part and a one-pole loss filter. The whole loop
// is tuned to exactly one period, to a fraction of a cent: a string that rings
// for seconds only answers within a fraction of a hertz of its note, so plain
// linear interpolation (a few cents off, and lossy) leaves it nearly silent.
// Its fundamental falls 60 dB in t60 seconds whatever its pitch. Optionally
// its second partial runs sharp (a first-order dispersion allpass), as a
// stiff piano string's does: that is what lets a stretch-tuned octave above
// wake it.
class TunedString
{
public:
    void allocate (int capacity)
    {
        delay.assign ((size_t) juce::jmax (4, capacity), 0.0f);
        tunedFrequency = -1.0;
        reset();
    }

    void reset()
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        write = 0;
        thiranIn = thiranOut = lowState = dispersionIn = dispersionOut = 0.0f;
    }

    // lowpass: the loss filter's coefficient (1 = no loss, smaller = the
    // overtones die sooner).
    // secondPartial: the second partial's ratio to the fundamental (2 =
    // harmonic).
    void tune (double sampleRate, double frequency, double t60, float lowpass, double secondPartial = 2.0)
    {
        if (frequency == tunedFrequency && t60 == tunedT60 && lowpass == tunedLowpass && sampleRate == tunedRate
            && secondPartial == tunedPartial)
            return;

        tunedFrequency = frequency;
        tunedT60 = t60;
        tunedRate = sampleRate;
        tunedPartial = secondPartial;
        tunedLowpass = lowpass;

        // The loop can't gain energy at DC, so the loss filter may only take
        // half of the fundamental's loss per pass: high strings get a
        // brighter filter, or they could never ring for t60.
        const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto passGain = std::pow (10.0, -3.0 / (t60 * frequency));
        const auto magnitude = [w] (double x) { return x / std::abs (1.0 - (1.0 - x) * std::polar (1.0, -w)); };
        auto a = juce::jlimit (0.01, 1.0, (double) lowpass);
        if (magnitude (a) < std::sqrt (passGain))
        {
            auto low = a, high = 1.0;
            for (int iteration = 0; iteration < 40; ++iteration)
                (magnitude (0.5 * (low + high)) < std::sqrt (passGain) ? low : high) = 0.5 * (low + high);
            a = high;
        }
        loss = (float) a;
        const auto lowpassAt = [a] (double x) { return a / (1.0 - (1.0 - a) * std::polar (1.0, -x)); };
        const auto allpassDelay = [] (double c, double x)
        {
            return -std::arg ((c + std::polar (1.0, -x)) / (1.0 + c * std::polar (1.0, -x))) / x;
        };
        const auto response = lowpassAt (w);
        const auto lowpassDelay = -std::arg (response) / w;
        const auto period = sampleRate / frequency;

        // Dispersion: the allpass whose falling delay puts the second
        // resonance at secondPartial (the loop's phase there is 4 pi). The
        // Thiran's small dispersion is ignored.
        dispersion = 0.0f;
        const auto w2 = w * secondPartial;
        if (secondPartial > 2.0 && w2 < 3.0)
        {
            const auto error = [&] (double c)
            {
                // Everything in the loop at the second partial: the delay line
                // and Thiran (a fixed length, set so the fundamental is exact),
                // the loss filter and the dispersion.
                const auto fixed = period - lowpassDelay - allpassDelay (c, w);
                const auto total = fixed + (-std::arg (lowpassAt (w2)) / w2) + allpassDelay (c, w2);
                return w2 * total - 4.0 * juce::MathConstants<double>::pi;
            };
            auto low = -0.995, high = 0.0;
            // The dispersion must leave room for the delay line.
            for (int step = 0; step < 20 && period - lowpassDelay - allpassDelay (low, w) < 2.0; ++step)
                low = 0.5 * (low - 0.5);
            if (error (low) < 0.0)
            {
                for (int iteration = 0; iteration < 40; ++iteration)
                {
                    const auto c = 0.5 * (low + high);
                    (error (c) < 0.0 ? low : high) = c;
                }
                dispersion = (float) (0.5 * (low + high));
            }
        }

        const auto dispersionDelay = dispersion != 0.0f ? allpassDelay ((double) dispersion, w) : 0.0;
        feedback = (float) juce::jmin (0.99995, passGain / std::abs (response));

        // The loop's length, then a correction: the loss filter's falling gain
        // pulls the resonance peak a little below where the phase says
        // (noticeable in the treble), so find the actual peak and move it.
        auto target = period;
        for (int pass = 0; pass < 3; ++pass)
        {
            setLength (juce::jlimit (1.5, (double) delay.size() - 2.0, target - lowpassDelay - dispersionDelay), w);
            const auto peak = resonancePeak (frequency, sampleRate);
            target *= peak / frequency;
        }
    }

    float process (float input)
    {
        const auto size = (int) delay.size();
        auto readIndex = write - whole;
        if (readIndex < 0)
            readIndex += size;

        const auto x = delay[(size_t) readIndex];
        const auto y = thiranCoefficient * x + thiranIn - thiranCoefficient * thiranOut;
        thiranIn = x;
        thiranOut = y;
        auto looped = y;
        if (dispersion != 0.0f)
        {
            looped = dispersion * y + dispersionIn - dispersion * dispersionOut;
            dispersionIn = y;
            dispersionOut = looped;
        }
        lowState += (looped - lowState) * loss;
        delay[(size_t) write] = juce::jlimit (-4.0f, 4.0f, lowState * feedback + input);
        write = (write + 1) % size;
        return y;
    }

private:
    // Integer delay plus the Thiran fraction. The allpass's phase delay is
    // only its nominal fraction at DC: nudge the fraction until its delay at
    // the note is what's needed.
    void setLength (double length, double w)
    {
        whole = (int) std::floor (length - 0.5);
        const auto wanted = length - (double) whole;
        auto part = wanted;
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            const auto c = (1.0 - part) / (1.0 + part);
            const auto allpass = (c + std::polar (1.0, -w)) / (1.0 + c * std::polar (1.0, -w));
            part = juce::jlimit (0.5, 1.6, part + wanted - (-std::arg (allpass) / w));
        }
        thiranCoefficient = (float) ((1.0 - part) / (1.0 + part));
    }

    // The loop's resonance nearest `frequency` (golden-section search on the
    // magnitude of 1 / (1 - loop gain), within +-15 cents).
    double resonancePeak (double frequency, double sampleRate) const
    {
        const auto response = [this, sampleRate] (double hz)
        {
            const auto w = juce::MathConstants<double>::twoPi * hz / sampleRate;
            const auto z1 = std::polar (1.0, -w);
            const auto c = (double) thiranCoefficient, d = (double) dispersion, a = (double) loss;
            auto loop = std::polar (1.0, -w * whole) * (c + z1) / (1.0 + c * z1) * (a / (1.0 - (1.0 - a) * z1))
                        * (double) feedback;
            if (d != 0.0)
                loop *= (d + z1) / (1.0 + d * z1);
            return std::abs (1.0 / (1.0 - loop));
        };
        auto low = frequency * std::exp2 (-15.0 / 1200.0), high = frequency * std::exp2 (15.0 / 1200.0);
        constexpr double golden = 0.6180339887;
        for (int iteration = 0; iteration < 50; ++iteration)
        {
            const auto left = high - golden * (high - low), right = low + golden * (high - low);
            (response (left) < response (right) ? low : high) = response (left) < response (right) ? left : right;
        }
        return 0.5 * (low + high);
    }

    std::vector<float> delay;
    int write = 0, whole = 1;
    float thiranCoefficient = 0.0f, thiranIn = 0.0f, thiranOut = 0.0f;
    float lowState = 0.0f, loss = 0.5f, feedback = 0.0f;
    float dispersion = 0.0f, dispersionIn = 0.0f, dispersionOut = 0.0f;
    double tunedFrequency = -1.0, tunedT60 = 0.0, tunedRate = 0.0, tunedPartial = 2.0;
    float tunedLowpass = -1.0f;
};
