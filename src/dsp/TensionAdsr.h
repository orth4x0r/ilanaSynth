#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

class TensionAdsr
{
public:
    struct Parameters
    {
        float attack = 0.005f;
        float decay = 0.3f;
        float sustain = 0.8f;
        float release = 0.25f;
        float curve = 0.0f;
    };

    void setSampleRate (double newSampleRate) { sampleRate = juce::jmax (1.0, newSampleRate); }

    void setParameters (const Parameters& newParameters)
    {
        params = newParameters;
        params.attack = juce::jmax (0.0005f, params.attack);
        params.decay = juce::jmax (0.0005f, params.decay);
        params.release = juce::jmax (0.0005f, params.release);
        params.sustain = juce::jlimit (0.0f, 1.0f, params.sustain);
        params.curve = juce::jlimit (-1.0f, 1.0f, params.curve);

        exponent = std::exp2 (-(double) params.curve * 2.0);
    }

    void reset()
    {
        stage = Stage::Idle;
        position = 0.0;
        currentValue = 0.0f;
    }

    void noteOn()
    {
        stage = Stage::Attack;
        position = 0.0;
        currentValue = 0.0f;
        attackStart = 0.0f;
    }

    // Mono retrigger: restart the attack from wherever the envelope is now,
    // so a new note doesn't click by snapping to zero first.
    void retrigger()
    {
        attackStart = stage == Stage::Idle ? 0.0f : currentValue;
        stage = Stage::Attack;
        position = 0.0;
    }

    void noteOff()
    {
        if (stage == Stage::Idle)
            return;

        stage = Stage::Release;
        position = 0.0;
        releaseStart = currentValue;
    }

    bool isActive() const { return stage != Stage::Idle; }
    float getCurrentValue() const { return currentValue; }

    float getNextSample()
    {
        switch (stage)
        {
            case Stage::Attack:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.attack * sampleRate);

                if (position >= length)
                {
                    currentValue = 1.0f;
                    stage = Stage::Decay;
                    position = 0.0;
                }
                else
                {
                    const auto progress = position / length;
                    currentValue = attackStart + (1.0f - attackStart) * (float) std::pow (progress, exponent);
                }

                break;
            }

            case Stage::Decay:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.decay * sampleRate);

                if (position >= length)
                {
                    currentValue = params.sustain;
                    stage = Stage::Sustain;
                }
                else
                {
                    const auto progress = position / length;
                    currentValue = params.sustain
                                   + (1.0f - params.sustain) * (float) std::pow (1.0 - progress, exponent);
                }

                break;
            }

            case Stage::Sustain:
                currentValue = params.sustain;
                break;

            case Stage::Release:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.release * sampleRate);

                if (position >= length)
                {
                    currentValue = 0.0f;
                    stage = Stage::Idle;
                }
                else
                {
                    const auto progress = position / length;
                    currentValue = releaseStart * (float) std::pow (1.0 - progress, exponent);
                }

                break;
            }

            case Stage::Idle:
            default:
                currentValue = 0.0f;
                break;
        }

        return currentValue;
    }

private:
    enum class Stage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    Parameters params;
    Stage stage = Stage::Idle;
    double sampleRate = 44100.0;
    double position = 0.0;
    double exponent = 1.0;
    float currentValue = 0.0f;
    float releaseStart = 0.0f;
    float attackStart = 0.0f;
};
