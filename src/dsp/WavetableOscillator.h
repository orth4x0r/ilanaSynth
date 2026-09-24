#pragma once

#include "Wavetable.h"

#include <cmath>

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

    void setFramePosition (float position) { framePosition = juce::jlimit (0.0f, 1.0f, position); }

    void resetPhase (double newPhase = 0.0) { phase = newPhase; }
    float getPhase() const { return (float) phase; }
    bool wrappedThisSample() const { return wrapped; }

    float getNextSample (double phaseModulation = 0.0)
    {
        wrapped = false;

        if (table == nullptr || table->getNumFrames() == 0 || sampleRate <= 0.0)
            return 0.0f;

        const auto numFrames = table->getNumFrames();
        const auto framePositionScaled = framePosition * (float) (numFrames - 1);
        const auto frame0 = (int) framePositionScaled;
        const auto frame1 = juce::jmin (frame0 + 1, numFrames - 1);
        const auto frameFrac = framePositionScaled - (float) frame0;

        auto modulatedPhase = phase + phaseModulation;
        modulatedPhase -= std::floor (modulatedPhase);

        const auto samplePosition = modulatedPhase * (double) Wavetable::frameSize;
        const auto index = juce::jlimit (0, Wavetable::frameSize - 1, (int) samplePosition);
        const auto frac = (float) (samplePosition - (double) index);

        const auto* data0 = table->getFrameData (currentLevel, frame0);
        const auto* data1 = table->getFrameData (currentLevel, frame1);

        const auto value0 = cubicInterpolate (data0, index, frac);
        const auto value1 = cubicInterpolate (data1, index, frac);
        const auto value = value0 + (value1 - value0) * frameFrac;

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
        currentLevel = table != nullptr && sampleRate > 0.0
                           ? table->getLevelForFrequency (frequency, sampleRate)
                           : 0;
    }

    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    double frequency = 440.0;
    double phase = 0.0;
    float framePosition = 0.0f;
    int currentLevel = 0;
    bool wrapped = false;
};
