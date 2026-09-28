#pragma once

// M8.3: the west-coast voice. A wavefolder (FOLD, SYMMETRY, 1-4 stages)
// into a low-pass gate: a filter and an amplifier in one, both steered by a
// vactrol, so a strike opens it fast and it closes slowly, getting darker as
// it gets quieter (the "bongo").
//
// Folder: cascaded sine folds, y = sin(pi/2 g x), each stage feeding the
// next, with first-order antiderivative antialiasing (Parker, Zavalishin and
// Le Bivic, DAFx 2016) so heavy folding doesn't alias at the base rate.
//
// Vactrol: an LED lighting a photocell. Its response is asymmetric and
// level dependent (Parker and D'Angelo, "A digital model of the Buchla
// lowpass-gate", DAFx 2013): it lights up in about 12 ms and goes dark over
// about 250 ms, and the darker the cell, the slower it gets, which gives the
// long tail. The gate's cutoff follows the cell's conductance (a 2-pole
// low-pass), its gain a power of it.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

#include "WestCoastTuning.h"

class Wavefolder
{
public:
    void reset()
    {
        previous.fill (0.0);
        upHistory.fill (0.0f);
        downHistory.fill (0.0f);
        historyPosition = 0;
        dcIn = dcOut = 0.0f;
    }

    // fold 0..1 (0 is almost clean, 1 folds about a dozen times), symmetry
    // -1..1 (a bias: even harmonics), 1-4 stages.
    void setParams (float fold, float symmetry, int newStages)
    {
        gain = 0.35 + 11.65 * (double) (fold * fold);
        makeUp = 1.0 / std::sin (juce::MathConstants<double>::halfPi * juce::jmin (1.0, gain));
        bias = 0.5 * (double) symmetry;
        stages = juce::jlimit (1, maxStages, newStages);
    }

    // Four times oversampled (a 32-tap windowed-sinc polyphase FIR each
    // way), with the antialiased fold at the high rate.
    float process (float input)
    {
        const auto& taps = firTaps();
        upHistory[(size_t) historyPosition] = input;
        auto out = 0.0;
        for (int phase = 0; phase < factor; ++phase)
        {
            // Interpolate: phase `phase` of the 4x signal.
            auto up = 0.0;
            for (int t = 0; t < tapsPerPhase; ++t)
                up += (double) taps[(size_t) (t * factor + phase)] * (double) upHistory[(size_t) ((historyPosition - t + historyLength) % historyLength)];
            up *= (double) factor;
            const auto folded = foldAll (up);
            // Decimate: the FIR again, over the 4x samples.
            downHistory[(size_t) ((historyPosition * factor + phase) % downLength)] = (float) folded;
        }
        const auto newest = historyPosition * factor + factor - 1;
        for (int t = 0; t < numTaps; ++t)
            out += (double) taps[(size_t) t] * (double) downHistory[(size_t) ((newest - t + downLength * 4) % downLength)];
        historyPosition = (historyPosition + 1) % historyLength;

        // The bias leaves an offset; block it.
        const auto y = (float) out;
        const auto blocked = y - dcIn + 0.9995f * dcOut;
        dcIn = y;
        dcOut = std::isfinite (blocked) ? blocked : 0.0f;
        return dcOut;
    }

    static double fold (double x) { return std::sin (juce::MathConstants<double>::halfPi * x); }
    static double foldIntegral (double x) { return -std::cos (juce::MathConstants<double>::halfPi * x) / juce::MathConstants<double>::halfPi; }

private:
    static constexpr int maxStages = 4;
    static constexpr int factor = 4, tapsPerPhase = 8, numTaps = factor * tapsPerPhase;
    static constexpr int historyLength = tapsPerPhase, downLength = numTaps;

    static const std::array<float, numTaps>& firTaps()
    {
        // Low-pass at a quarter of the 4x rate's Nyquist (the base rate's
        // Nyquist), Blackman windowed, unity DC gain.
        static const auto taps = []
        {
            std::array<float, numTaps> t {};
            auto sum = 0.0;
            for (int i = 0; i < numTaps; ++i)
            {
                const auto m = (double) i - (numTaps - 1) * 0.5;
                const auto x = juce::MathConstants<double>::pi * m / (double) factor * 0.9;
                const auto sinc = std::abs (m) < 1.0e-9 ? 1.0 : std::sin (x) / x;
                const auto w = 0.42 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (numTaps - 1))
                               + 0.08 * std::cos (2.0 * juce::MathConstants<double>::twoPi * i / (numTaps - 1));
                t[(size_t) i] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (auto& v : t)
                v = (float) ((double) v / sum);
            return t;
        }();
        return taps;
    }

    double foldAll (double input)
    {
        auto x = input * gain + bias;
        for (int stage = 0; stage < stages; ++stage)
        {
            x = foldAntialiased (x, previous[(size_t) stage]);
            if (stage + 1 < stages)
                x *= 1.0 + 0.35 * gain / (double) stages; // each stage folds the last a little further
        }
        return x * makeUp;
    }

    // First-order ADAA: the mean of the fold over the step from the last
    // input to this one.
    static double foldAntialiased (double x, double& last)
    {
        const auto difference = x - last;
        const auto y = std::abs (difference) < 1.0e-5 ? fold (0.5 * (x + last))
                                                       : (foldIntegral (x) - foldIntegral (last)) / difference;
        last = x;
        return y;
    }

    std::array<double, maxStages> previous {};
    std::array<float, historyLength> upHistory {};
    std::array<float, downLength> downHistory {};
    int historyPosition = 0;
    double gain = 1.0, bias = 0.0, makeUp = 1.0;
    int stages = 1;
    float dcIn = 0.0f, dcOut = 0.0f;
};

// The vactrol: control (0..1, the LED's drive) to conductance (0..1).
class Vactrol
{
public:
    void prepare (double rate) { sampleRate = rate; }
    void reset() { conductance = 0.0; }

    // decayScale: DECAY (1 = the vactrol's own 250 ms).
    void setDecay (float scale) { decayScale = juce::jlimit (0.05, 8.0, (double) scale); }

    float process (float control)
    {
        const auto& t = WestCoastTuning::get();
        // A strike overdrives the LED (up to twice a steady full drive), so
        // the cell lights fully in well under its time constant; the
        // conductance itself tops out at 1.
        const auto target = (double) juce::jlimit (0.0f, 2.0f, control);
        // Lighting up is fast; going dark is slow, and slower the darker it
        // is (the photocell's memory).
        const auto seconds = target > conductance
                                 ? (double) t.riseSeconds
                                 : (double) t.fallSeconds * decayScale * std::pow (1.0 + (double) t.fallSlowing * (1.0 - conductance), 2.0)
                                       / std::pow (1.0 + (double) t.fallSlowing * 0.5, 2.0);
        conductance += (target - conductance) * (1.0 - std::exp (-1.0 / (juce::jmax (1.0e-4, seconds) * sampleRate)));
        conductance = juce::jlimit (0.0, 1.0, conductance);
        return (float) conductance;
    }

    float getConductance() const { return (float) conductance; }

private:
    double sampleRate = 48000.0, conductance = 0.0, decayScale = 1.0;
};

class LowPassGate
{
public:
    enum class Mode { Combo = 0, LowPass, Vca };

    void prepare (double rate)
    {
        sampleRate = rate;
        vactrol.prepare (rate);
        reset();
    }

    void reset()
    {
        vactrol.reset();
        s1 = s2 = 0.0;
    }

    void setParams (Mode newMode, float decay, float resonance)
    {
        mode = newMode;
        vactrol.setDecay (decay);
        damping = 2.0 * (1.0 - 0.9 * (double) juce::jlimit (0.0f, 1.0f, resonance)) * 0.7071;
    }

    // One sample: `control` drives the LED.
    float process (float input, float control)
    {
        const auto& t = WestCoastTuning::get();
        const auto c = (double) vactrol.process (control);
        auto y = (double) input;

        if (mode != Mode::Vca)
        {
            // Cutoff: exponential in the conductance, from a closed gate's
            // near-silence to the open top.
            const auto cutoff = (double) t.closedHz * std::pow ((double) t.openHz / (double) t.closedHz, std::pow (c, (double) t.cutoffCurve));
            const auto g = std::tan (juce::MathConstants<double>::pi * juce::jmin (cutoff, sampleRate * 0.45) / sampleRate);
            // TPT state-variable low-pass.
            const auto hp = (y - (damping + g) * s1 - s2) / (1.0 + damping * g + g * g);
            const auto bp = g * hp + s1;
            s1 = g * hp + bp;
            const auto lp = g * bp + s2;
            s2 = g * bp + lp;
            y = lp;
        }
        if (mode != Mode::LowPass)
            y *= std::pow (c, (double) t.gainCurve);

        if (! std::isfinite (y))
        {
            reset();
            return 0.0f;
        }
        return (float) y;
    }

    float getConductance() const { return vactrol.getConductance(); }

private:
    Vactrol vactrol;
    Mode mode = Mode::Combo;
    double sampleRate = 48000.0, s1 = 0.0, s2 = 0.0, damping = 1.414;
};
