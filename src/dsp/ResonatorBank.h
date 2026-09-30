#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

class ResonatorBank
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const auto size = juce::nextPowerOfTwo ((int) (sampleRate / 30.0) + 1);

        for (auto& buffer : buffers)
        {
            buffer.assign ((size_t) size, 0.0f);
        }

        reset();
    }

    void reset()
    {
        for (auto& buffer : buffers)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (auto& state : lowpassState)
            state = 0.0f;

        writePosition = 0;
    }

    void setTuning (double baseFrequency, float offsetSemitones, float keytrack)
    {
        if (buffers[0].empty())
            return;

        const auto trackedRoot = baseFrequency * std::exp2 ((double) offsetSemitones / 12.0);
        const auto fixedRoot = 220.0 * std::exp2 ((double) offsetSemitones / 12.0);
        const auto blendedRoot = trackedRoot * (double) juce::jlimit (0.0f, 1.0f, keytrack)
                                 + fixedRoot * (double) (1.0f - juce::jlimit (0.0f, 1.0f, keytrack));

        for (int i = 0; i < 3; ++i)
        {
            const auto frequency = juce::jlimit (25.0, sampleRate * 0.45, blendedRoot * ratios[i]);
            delays[i] = sampleRate / frequency;
        }
    }

    void setParams (float newAmount, float newDecay, float newDamping)
    {
        const auto clampedAmount = juce::jlimit (0.0f, 1.0f, newAmount);

        if (clampedAmount < 0.001f && amount >= 0.001f)
            reset();

        amount = clampedAmount;
        feedback = 0.5f + juce::jlimit (0.0f, 1.0f, newDecay) * 0.48f;
        lowpassCoefficient = 1.0f - juce::jlimit (0.0f, 1.0f, newDamping) * 0.9f;
    }

    // excite rings the delay lines without joining the dry mix (the live
    // input in ilanaSynth FX); at 0 the output is exactly as before.
    float process (float input, float excite = 0.0f)
    {
        if (buffers[0].empty() || amount < 0.001f)
            return input;

        const auto size = (int) buffers[0].size();
        auto sum = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            auto readPosition = (double) writePosition - delays[i];

            while (readPosition < 0.0)
                readPosition += (double) size;

            const auto index = (int) readPosition;
            const auto next = (index + 1) % size;
            const auto fraction = (float) (readPosition - (double) index);
            const auto delayed = buffers[(size_t) i][(size_t) index]
                                 + (buffers[(size_t) i][(size_t) next] - buffers[(size_t) i][(size_t) index]) * fraction;

            lowpassState[i] += (delayed - lowpassState[i]) * lowpassCoefficient;
            buffers[(size_t) i][(size_t) writePosition] = input + excite + lowpassState[i] * feedback;
            sum += delayed;
        }

        writePosition = (writePosition + 1) % size;

        return input * (1.0f - amount) + (sum / 3.0f) * amount;
    }

private:
    static constexpr double ratios[3] = { 1.0, 2.0, 3.0 };

    std::vector<float> buffers[3];
    float lowpassState[3] {};
    double delays[3] {};
    double sampleRate = 44100.0;
    float amount = 0.0f;
    float feedback = 0.9f;
    float lowpassCoefficient = 0.5f;
    int writePosition = 0;
};
