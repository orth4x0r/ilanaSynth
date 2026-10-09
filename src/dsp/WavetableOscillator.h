#pragma once

#include "Wavetable.h"

#include <cmath>

// Oscillator warp modes (Serum-style): phase remaps applied before the table
// lookup. FM and Ring use another oscillator, so the voice applies those.
// floor() without the libm call: baseline x86-64 builds can't inline it,
// and this runs for every unison voice on every sample.
inline double fastFloor (double value) noexcept
{
    const auto truncated = (double) (long long) value;
    return value < truncated ? truncated - 1.0 : truncated;
}

namespace Warp
{
enum
{
    Off = 0,
    Sync,
    BendPlus,
    BendMinus,
    Pwm,
    Mirror,
    Asym,
    Quantize,
    Fm,
    Ring,
    // M6: Casio CZ-style phase distortion. Appended, so saved indices keep
    // their meaning.
    PdSaw,
    PdSquare,
    PdPulse,
    PdRes1,
    PdRes2,
    PdRes3,
    // Appended after the PD family: a windowed formant and a centre squeeze.
    Formant,
    Squeeze,
    Count
};

inline juce::StringArray getNames()
{
    return { "Off", "Sync", "Bend +", "Bend -", "PWM", "Mirror", "Asym", "Quantize", "FM", "Ring",
             "PD Saw", "PD Square", "PD Pulse", "PD Res I", "PD Res II", "PD Res III", "Formant", "Squeeze" };
}

inline bool isPhaseWarp (int mode) { return mode >= Sync && mode <= Quantize; }
inline bool isPhaseDistortion (int mode) { return mode >= PdSaw && mode <= PdRes3; }
inline bool isResonance (int mode) { return mode >= PdRes1 && mode <= PdRes3; }

// The oscillator applies these itself (FM and Ring need another oscillator).
inline bool isShaper (int mode) { return mode == Formant || mode == Squeeze; }
inline bool isOscillatorWarp (int mode) { return isPhaseWarp (mode) || isPhaseDistortion (mode) || isShaper (mode); }

// The second stage of the PD chain offers only the warps an oscillator can
// apply on its own: every mode except FM and Ring.
inline juce::StringArray getStageTwoNames()
{
    auto names = getNames();
    names.removeString ("FM");
    names.removeString ("Ring");
    return names;
}

inline int modeForStageTwoChoice (int choice) { return choice < Fm ? choice : choice + 2; }

// PD knee and resonance settings, shared by the oscillator and the displays.
inline double pdKnee (int mode, double a)
{
    switch (mode)
    {
        case PdSaw:
        case PdSquare: return 0.5 - a * 0.49;
        case PdPulse:  return 1.0 - a * 0.95;
        default:       return 1.0;
    }
}

inline double pdResonance (double a) { return 1.0 + a * 15.0; }

// How much a warp raises the highest harmonic, so the oscillator can pick a
// band-limited table level that won't alias as badly.
inline double harmonicStretch (int mode, float amount)
{
    switch (mode)
    {
        case Sync:      return 1.0 + (double) amount * 7.0;
        case BendPlus:
        case BendMinus: return 1.0 + (double) amount * 2.0;
        case Pwm:       return 1.0 / (1.0 - (double) amount * 0.95);
        case Mirror:    return 1.0 + (double) amount;
        case Asym:      return 0.5 / (0.5 - (double) amount * 0.47);
        case PdSaw:
        case PdSquare:  return 0.5 / pdKnee (mode, (double) amount);
        case PdPulse:   return 1.0 / pdKnee (mode, (double) amount);
        case PdRes1:
        case PdRes2:
        case PdRes3:    return pdResonance ((double) amount);
        case Formant:   return 1.0 + (double) amount * 15.0;
        case Squeeze:   return 1.0 + (double) amount * 7.0;
        default:        return 1.0;
    }
}

// Returns the warped phase; sets silent for the dead part of a PWM cycle.
inline double apply (int mode, float amount, double phase, bool& silent)
{
    silent = false;
    const auto a = (double) juce::jlimit (0.0f, 1.0f, amount);

    switch (mode)
    {
        case Sync:
        {
            const auto p = phase * (1.0 + a * 7.0);
            return p - fastFloor (p);
        }

        case BendPlus:  return std::pow (phase, std::exp2 (-a * 2.5));
        case BendMinus: return std::pow (phase, std::exp2 (a * 2.5));

        case Pwm:
        {
            const auto p = phase / (1.0 - a * 0.95);

            if (p >= 1.0)
            {
                silent = true;
                return 0.0;
            }

            return p;
        }

        case Mirror:
        {
            // Play forwards then backwards; amount moves the turn-around
            // point from the end of the cycle to the middle.
            const auto turn = 1.0 - a * 0.5;
            return phase < turn ? phase / turn : (1.0 - phase) / (1.0 - turn);
        }

        case Asym:
        {
            const auto knee = 0.5 - a * 0.47;
            return phase < knee ? 0.5 * phase / knee : 0.5 + 0.5 * (phase - knee) / (1.0 - knee);
        }

        case Quantize:
        {
            const auto steps = std::exp2 (1.0 + (1.0 - a) * 8.0);
            return fastFloor (phase * steps) / steps;
        }

        case Formant:
        {
            // The cycle plays 1..16 times faster; applyStage fades it with a window.
            const auto p = phase * (1.0 + a * 15.0);
            return p - fastFloor (p);
        }

        case Squeeze:
            // The middle of the cycle runs faster and the ends hold still.
            return juce::jlimit (0.0, 1.0, 0.5 + (phase - 0.5) * (1.0 + a * 7.0));

        default:
            return phase;
    }
}

// Phase distortion, as on the Casio CZ: the CZ reads a cosine through a bent
// phase. Tables here are sine-phase, so the bend happens a quarter cycle on
// (where a sine table reads as a cosine) and is shifted back. With the Sine
// table the result is the CZ wave (inverted and a quarter cycle late, which
// is inaudible); any other table gets the same bend.
//
// The resonance modes are a sine at 1..16x the pitch that restarts every
// cycle, faded by a window (a falling saw, a triangle or a trapezoid) so the
// restart doesn't click. gain carries the window.
inline double applyPhaseDistortion (int mode, float amount, double phase, float& gain)
{
    const auto a = (double) juce::jlimit (0.0f, 1.0f, amount);
    gain = 1.0f;

    if (isResonance (mode))
    {
        // Fades in from the plain wave over the first quarter of the knob.
        const auto depth = juce::jmin (1.0, a * 4.0);
        double window;

        if (mode == PdRes1)      window = 1.0 - phase;
        else if (mode == PdRes2) window = 1.0 - std::abs (2.0 * phase - 1.0);
        else                     window = juce::jmin (1.0, 2.0 * (1.0 - phase));

        gain = (float) (1.0 - depth * (1.0 - window));
        const auto p = phase * pdResonance (a);
        return p - fastFloor (p);
    }

    auto q = phase + 0.25;
    q -= fastFloor (q);
    const auto d = pdKnee (mode, a);
    double bent = q;

    switch (mode)
    {
        case PdSaw:
            bent = q < d ? 0.5 * q / d : 0.5 + 0.5 * (q - d) / (1.0 - d);
            break;

        case PdSquare:
        {
            const auto half = q < 0.5 ? q : q - 0.5;
            const auto rise = half < d ? 0.5 * half / d : 0.5;
            bent = q < 0.5 ? rise : 0.5 + rise;
            break;
        }

        case PdPulse:
            bent = q < d ? q / d : 1.0;
            break;

        default:
            break;
    }

    bent -= 0.25;
    return bent - fastFloor (bent);
}
} // namespace Warp

class WavetableOscillator
{
public:
    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateLevel();
    }

    void setWavetable (const Wavetable* newTable)
    {
        table = newTable;
        updateLevel();
    }

    void setFrequency (double newFrequency)
    {
        frequency = newFrequency;
        updateLevel();
    }

    void setWarp (int newMode, float newAmount)
    {
        const auto mode = Warp::isOscillatorWarp (newMode) ? newMode : Warp::Off;
        const auto amount = juce::jlimit (0.0f, 1.0f, newAmount);

        if (mode == warpMode && std::abs (amount - warpAmount) < 1.0e-4f)
            return;

        warpMode = mode;
        warpAmount = amount;
        updateLevel();
    }

    // The PD chain's second stage, applied after the first.
    void setWarp2 (int newMode, float newAmount)
    {
        const auto mode = Warp::isOscillatorWarp (newMode) ? newMode : Warp::Off;
        const auto amount = juce::jlimit (0.0f, 1.0f, newAmount);

        if (mode == warpMode2 && std::abs (amount - warpAmount2) < 1.0e-4f)
            return;

        warpMode2 = mode;
        warpAmount2 = amount;
        updateLevel();
    }

    void setFramePosition (float position) { framePosition = juce::jlimit (0.0f, 1.0f, position); }

    void resetPhase (double newPhase = 0.0) { phase = newPhase; }
    float getPhase() const { return (float) phase; }
    bool wrappedThisSample() const { return wrapped; }

    // Where in the table to read: shared by every unison voice of an
    // oscillator, so the voice works it out once per sample.
    struct FrameRead
    {
        int frame0 = 0;
        int frame1 = 0;
        float frac = 0.0f;
        float position = 0.0f; // 0..1, kept so a spread can offset it per unison voice
    };

    static FrameRead frameReadFor (const Wavetable* table, float position) noexcept
    {
        FrameRead read;

        if (table == nullptr || table->getNumFrames() == 0)
            return read;

        const auto numFrames = table->getNumFrames();
        read.position = position;
        const auto scaled = juce::jlimit (0.0f, 1.0f, position) * (float) (numFrames - 1);
        read.frame0 = (int) scaled;
        read.frame1 = juce::jmin (read.frame0 + 1, numFrames - 1);
        read.frac = scaled - (float) read.frame0;
        return read;
    }

    float getNextSample (double phaseModulation = 0.0)
    {
        return getNextSample (phaseModulation, frameReadFor (table, framePosition));
    }

    // incrementScale bends the frequency for this sample (through-zero and
    // exponential FM); 1 is the plain pitch, negative runs backwards.
    float getNextSample (double phaseModulation, const FrameRead& frames, double incrementScale = 1.0)
    {
        wrapped = false;

        if (table == nullptr || table->getNumFrames() == 0 || sampleRate <= 0.0)
            return 0.0f;

        const auto frame0 = frames.frame0;
        const auto frame1 = frames.frame1;
        const auto frameFrac = frames.frac;

        auto modulatedPhase = phase + phaseModulation;
        modulatedPhase -= fastFloor (modulatedPhase);

        auto value = 0.0f;
        auto silent = false;
        auto warpGain = 1.0f;

        if (warpMode != Warp::Off && warpAmount > 0.0f)
            modulatedPhase = applyStage (warpMode, warpAmount, modulatedPhase, silent, warpGain);

        if (warpMode2 != Warp::Off && warpAmount2 > 0.0f && ! silent)
        {
            auto gain2 = 1.0f;
            modulatedPhase = applyStage (warpMode2, warpAmount2, modulatedPhase, silent, gain2);
            warpGain *= gain2;
        }

        if (! silent)
        {
            const auto samplePosition = modulatedPhase * (double) Wavetable::frameSize;
            const auto index = juce::jlimit (0, Wavetable::frameSize - 1, (int) samplePosition);
            const auto frac = (float) (samplePosition - (double) index);

            const auto value0 = cubicInterpolate (table->getFrameData (currentLevel, frame0), index, frac);

            if (frameFrac > 0.0f && frame1 != frame0)
            {
                const auto value1 = cubicInterpolate (table->getFrameData (currentLevel, frame1), index, frac);
                value = value0 + (value1 - value0) * frameFrac;
            }
            else
            {
                value = value0;
            }

            if (warpGain != 1.0f)
                value *= warpGain;
        }

        phase += frequency / sampleRate * incrementScale;

        while (phase >= 1.0)
        {
            phase -= 1.0;
            wrapped = true;
        }

        while (phase < 0.0)
            phase += 1.0;

        return value;
    }

    // One warp stage (phase warp or phase distortion). Public so the
    // unison bank runs the same chain.
    static double applyStage (int mode, float amount, double phase, bool& silent, float& gain)
    {
        if (Warp::isPhaseDistortion (mode))
        {
            silent = false;
            return Warp::applyPhaseDistortion (mode, amount, phase, gain);
        }

        const auto warped = Warp::apply (mode, amount, phase, silent);

        // Formant fades each cycle in and out (a Hann window, fully in
        // from a quarter of the knob up) so the shifted waves join smoothly.
        if (mode == Warp::Formant)
        {
            const auto depth = juce::jmin (1.0, (double) juce::jlimit (0.0f, 1.0f, amount) * 4.0);
            gain = (float) (1.0 - depth * (0.5 + 0.5 * std::cos (juce::MathConstants<double>::twoPi * phase)));
        }
        else
            gain = 1.0f;

        return warped;
    }

private:
    static float cubicInterpolate (const float* data, int index, float frac) noexcept
    {
        const auto y0 = data[index];
        const auto y1 = data[index + 1];
        const auto y2 = data[index + 2];
        const auto y3 = data[index + 3];

        return 0.5f * (2.0f * y1
                       + (-y0 + y2) * frac
                       + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * frac * frac
                       + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * frac * frac * frac);
    }

    void updateLevel()
    {
        auto stretch = warpMode != Warp::Off ? Warp::harmonicStretch (warpMode, warpAmount) : 1.0;

        if (warpMode2 != Warp::Off)
            stretch *= Warp::harmonicStretch (warpMode2, warpAmount2);

        currentLevel = table != nullptr && sampleRate > 0.0
                           ? table->getLevelForFrequency (frequency * stretch, sampleRate)
                           : 0;
    }

    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    double frequency = 440.0;
    double phase = 0.0;
    float framePosition = 0.0f;
    int currentLevel = 0;
    int warpMode = Warp::Off;
    float warpAmount = 0.0f;
    int warpMode2 = Warp::Off;
    float warpAmount2 = 0.0f;
    bool wrapped = false;
};
