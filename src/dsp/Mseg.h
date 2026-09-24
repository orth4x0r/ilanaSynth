#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

class Mseg
{
public:
    static constexpr int numPoints = 4;

    void prepare (double newSampleRate) { sampleRate = newSampleRate; }

    void setParams (const float* newLevels, const float* newTimes, double rateHz, bool shouldLoop)
    {
        if (shouldLoop && ! loop)
            finished = false;

        auto total = 0.0;

        for (int i = 0; i < numPoints; ++i)
            total += juce::jmax (0.01f, newTimes[i]);

        for (int i = 0; i < numPoints; ++i)
        {
            levels[i] = juce::jlimit (-1.0f, 1.0f, newLevels[i]);
            times[i] = (double) juce::jmax (0.01f, newTimes[i]) / total;
        }

        rate = juce::jlimit (0.01, 40.0, rateHz);
        loop = shouldLoop;
    }

    void reset()
    {
        phase = 0.0;
        finished = false;
    }

    float getNextValue()
    {
        const auto value = valueAt (phase);

        if (! finished)
        {
            phase += rate / sampleRate;

            if (phase >= 1.0)
            {
                if (loop)
                    phase -= std::floor (phase);
                else
                {
                    phase = 1.0;
                    finished = true;
                }
            }
        }

        return value;
    }

private:
    float valueAt (double p) const
    {
        auto cumulative = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const auto duration = times[i];

            if (p < cumulative + duration || i == numPoints - 1)
            {
                const auto local = duration > 0.0001 ? (p - cumulative) / duration : 0.0;
                const auto from = levels[i];
                const auto to = (i + 1 < numPoints) ? levels[i + 1]
                                                    : (loop ? levels[0] : levels[numPoints - 1]);

                return (float) (from + ((double) to - (double) from) * juce::jlimit (0.0, 1.0, local));
            }

            cumulative += duration;
        }

        return levels[numPoints - 1];
    }

    float levels[numPoints] { 0.0f, 1.0f, 0.0f, -1.0f };
    double times[numPoints] { 0.25, 0.25, 0.25, 0.25 };
    double sampleRate = 44100.0;
    double rate = 1.0;
    double phase = 0.0;
    bool loop = true;
    bool finished = false;
};
