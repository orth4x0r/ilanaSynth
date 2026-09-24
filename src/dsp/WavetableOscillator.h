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
    Count
};

inline juce::StringArray getNames()
{
    return { "Off", "Sync", "Bend +", "Bend -", "PWM", "Mirror", "Asym", "Quantize", "FM", "Ring" };
}

inline bool isPhaseWarp (int mode) { return mode >= Sync && mode <= Quantize; }

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

        default:
            return phase;
    }
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
        const auto mode = Warp::isPhaseWarp (newMode) ? newMode : Warp::Off;
        const auto amount = juce::jlimit (0.0f, 1.0f, newAmount);

        if (mode == warpMode && std::abs (amount - warpAmount) < 1.0e-4f)
            return;

        warpMode = mode;
        warpAmount = amount;
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
    };

    static FrameRead frameReadFor (const Wavetable* table, float position) noexcept
    {
        FrameRead read;

        if (table == nullptr || table->getNumFrames() == 0)
            return read;

        const auto numFrames = table->getNumFrames();
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

    float getNextSample (double phaseModulation, const FrameRead& frames)
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

        if (warpMode != Warp::Off && warpAmount > 0.0f)
            modulatedPhase = Warp::apply (warpMode, warpAmount, modulatedPhase, silent);

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
        }

        phase += frequency / sampleRate;

        while (phase >= 1.0)
        {
            phase -= 1.0;
            wrapped = true;
        }

        return value;
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
        const auto stretch = warpMode != Warp::Off ? Warp::harmonicStretch (warpMode, warpAmount) : 1.0;

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
    bool wrapped = false;
};
