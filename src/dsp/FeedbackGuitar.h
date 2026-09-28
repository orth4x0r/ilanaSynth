#pragma once

// M8.5: an amp and a speaker inside the string's loop (the Feedback
// exciter). The string's pickup drives an amplifier (GAIN, a valve-like
// asymmetric clip), a speaker (a 4x12-ish band: low cut, cone resonance,
// presence, top roll-off), and the sound travels back through the air (the
// amp's DISTANCE: a delay of d / 343 m/s, and a 1/d loss) to push the
// string again (FEEDBACK). Whichever partial comes back in phase and with
// the most loop gain grows: a plucked note sustains, then blooms into one
// of its harmonics, as guitar feedback does. The amp's clipping sets the
// level it settles at.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <vector>

#include "FeedbackGuitarTuning.h"

class FeedbackLoop
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;
        delayLine.assign ((size_t) juce::nextPowerOfTwo ((int) (rate * 0.03) + 4), 0.0f);
        mask = (int) delayLine.size() - 1;
        design();
        reset();
    }

    void reset()
    {
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        write = 0;
        for (auto& b : speaker)
            b.reset();
        dcIn = dcOut = 0.0f;
    }

    // feedback 0..1, gain 0..1, distance 0..1 (0.3 to 4 m).
    void setParams (float newFeedback, float newGain, float newDistance)
    {
        feedback = juce::jlimit (0.0f, 1.0f, newFeedback);
        gainKnob = juce::jlimit (0.0f, 1.0f, newGain);
        const auto metres = 0.3 * std::pow (4.0 / 0.3, (double) juce::jlimit (0.0f, 1.0f, newDistance));
        if (std::abs (metres - distance) > 1.0e-6)
        {
            distance = metres;
            design();
        }
    }

    bool isActive() const { return feedback > 0.0f; }

    // One sample: the string's output in, the force back onto the string out.
    float process (float pickup)
    {
        if (feedback <= 0.0f || delayLine.empty())
            return 0.0f;
        const auto& t = FeedbackGuitarTuning::get();

        // Amp: preamp gain (0 to 46 dB), an asymmetric valve-ish clip.
        const auto drive = (double) pickup * std::pow (10.0, (double) gainKnob * (double) t.gainRangeDb / 20.0) * (double) t.pickupLevel;
        auto amp = drive >= 0.0 ? std::tanh (drive) : std::tanh (drive * 0.8) / 0.8;
        // Blocks the clip's offset.
        const auto y = (float) amp;
        const auto blocked = y - dcIn + 0.999f * dcOut;
        dcIn = y;
        dcOut = blocked;
        amp = blocked;

        // Speaker.
        for (auto& band : speaker)
            amp = band.process (amp);

        // Air: the delay back to the guitar.
        delayLine[(size_t) write] = (float) amp;
        auto read = write - delaySamples;
        write = (write + 1) & mask;
        const auto back = delayLine[(size_t) (read & mask)];
        const auto force = back * feedback * (float) (t.loopGain / distance);
        return std::isfinite (force) ? force : 0.0f;
    }

    int getDelaySamples() const { return delaySamples; }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0.0; }
        double process (double x)
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void highPass (double rate, double hz, double q)
        {
            const auto w = juce::MathConstants<double>::twoPi * hz / rate, alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
            b0 = (1.0 + c) / 2.0 / a0; b1 = -(1.0 + c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
        }
        void lowPass (double rate, double hz, double q)
        {
            const auto w = juce::MathConstants<double>::twoPi * juce::jmin (hz, rate * 0.45) / rate, alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
            b0 = (1.0 - c) / 2.0 / a0; b1 = (1.0 - c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
        }
        void peak (double rate, double hz, double q, double db)
        {
            const auto a = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * juce::jmin (hz, rate * 0.45) / rate;
            const auto alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha / a;
            b0 = (1.0 + alpha * a) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - alpha * a) / a0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha / a) / a0;
        }
    };

    void design()
    {
        const auto& t = FeedbackGuitarTuning::get();
        speaker[0].highPass (sampleRate, (double) t.speakerLow, 0.7);
        speaker[1].peak (sampleRate, (double) t.coneHz, 1.2, (double) t.coneDb);
        speaker[2].peak (sampleRate, (double) t.presenceHz, 0.9, (double) t.presenceDb);
        speaker[3].lowPass (sampleRate, (double) t.speakerHigh, 0.7);
        delaySamples = juce::jlimit (1, mask > 0 ? mask - 1 : 1, (int) std::round (distance / 343.0 * sampleRate + (double) t.latencySamples));
    }

    std::vector<float> delayLine;
    int mask = 0, write = 0, delaySamples = 48;
    std::array<Biquad, 4> speaker;
    double sampleRate = 48000.0, distance = 1.0;
    float feedback = 0.0f, gainKnob = 0.5f, dcIn = 0.0f, dcOut = 0.0f;
};
