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
        // M5 DAHDSR: a wait before the attack and a hold at the peak. Both
        // default to zero, which skips their stages entirely.
        float delay = 0.0f;
        float hold = 0.0f;
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
        params.delay = juce::jmax (0.0f, params.delay);
        params.hold = juce::jmax (0.0f, params.hold);

        const auto newExponent = std::exp2 (-(double) params.curve * 2.0);
        if (newExponent != exponent)
            lineValid = false;
        exponent = newExponent;
    }

    void reset()
    {
        stage = Stage::Idle;
        position = 0.0;
        currentValue = 0.0f;
    }

    // Starts in the delay stage, which hands over to the attack on the first
    // sample when there is no delay (the voice may set the parameters only
    // after the note starts).
    void noteOn()
    {
        stage = Stage::Delay;
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

    // Where the envelope is, for the graph's playhead: the stage (0 delay,
    // 1 attack, 2 hold, 3 decay, 4 sustain, 5 release) plus the fraction
    // of it done, or -1 while idle. Read only; changes nothing.
    float getDisplayPosition() const
    {
        const auto fraction = [this] (float seconds)
        {
            return (float) juce::jlimit (0.0, 0.999, position / juce::jmax (1.0, (double) seconds * sampleRate));
        };

        switch (stage)
        {
            case Stage::Delay:   return 0.0f + fraction (params.delay);
            case Stage::Attack:  return 1.0f + fraction (params.attack);
            case Stage::Hold:    return 2.0f + fraction (params.hold);
            case Stage::Decay:   return 3.0f + fraction (params.decay);
            case Stage::Sustain: return 4.0f;
            case Stage::Release: return 5.0f + fraction (params.release);
            case Stage::Idle:
            default:             return -1.0f;
        }
    }
    float getCurrentValue() const { return currentValue; }

    float getNextSample()
    {
        if (stage == Stage::Delay)
        {
            if (position < (double) params.delay * sampleRate)
            {
                position += 1.0;
                currentValue = 0.0f;
                return currentValue;
            }

            stage = Stage::Attack;
            position = 0.0;
        }

        switch (stage)
        {

            case Stage::Hold:
            {
                position += 1.0;
                currentValue = 1.0f;

                if (position >= (double) params.hold * sampleRate)
                {
                    stage = Stage::Decay;
                    position = 0.0;
                }

                break;
            }

            case Stage::Attack:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.attack * sampleRate);

                if (position >= length)
                {
                    currentValue = 1.0f;
                    stage = params.hold > 0.0f ? Stage::Hold : Stage::Decay;
                    position = 0.0;
                }
                else
                {
                    const auto progress = position / length;
                    currentValue = attackStart + (1.0f - attackStart) * (float) shaped (progress, length, 1.0);
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
                                   + (1.0f - params.sustain) * (float) shaped (1.0 - progress, length, -1.0);
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
                    currentValue = releaseStart * (float) shaped (1.0 - progress, length, -1.0);
                }

                break;
            }

            case Stage::Delay:
            case Stage::Idle:
            default:
                currentValue = 0.0f;
                break;
        }

        return currentValue;
    }

private:
    // progress^exponent. pow() is exact for an exponent of 1, so zero tension
    // skips the call. Otherwise the curve is computed every spanSteps
    // samples (1 / length of progress each, with the stage's direction)
    // and followed on a straight line in between, wherever the curve is
    // gentle enough that the line stays within maxLineError of it (the
    // error of a chord: |f''| span^2 / 8, f'' = e (e - 1) x^(e - 2)). Short
    // stages and the steep end near zero are computed every sample.
    double shaped (double progress, double length, double direction)
    {
        if (exponent == 1.0)
            return progress;

        if (lineValid && progress >= lineLow && progress <= lineHigh)
            return lineLowValue + (progress - lineLow) * lineSlope;

        const auto value = std::pow (progress, exponent);
        const auto end = juce::jlimit (0.0, 1.0, progress + direction * spanSteps / length);
        const auto low = juce::jmin (progress, end), high = juce::jmax (progress, end);
        lineValid = false;

        if (low > 0.0 && high > low)
        {
            const auto endValue = std::pow (end, exponent);
            const auto lowValue = progress < end ? value : endValue;
            const auto highValue = progress < end ? endValue : value;
            // x^(e - 2) = x^e / x^2, largest at one end of the span.
            const auto bend = std::abs (exponent * (exponent - 1.0))
                              * juce::jmax (lowValue / (low * low), highValue / (high * high));
            const auto span = high - low;

            if (bend * span * span * 0.125 <= maxLineError)
            {
                lineValid = true;
                lineLow = low;
                lineHigh = high;
                lineLowValue = lowValue;
                lineSlope = (highValue - lowValue) / span;
            }
        }

        return value;
    }

    static constexpr double spanSteps = 16.0;
    static constexpr double maxLineError = 1.0e-7;
    bool lineValid = false;
    double lineLow = 0.0, lineHigh = 0.0, lineLowValue = 0.0, lineSlope = 0.0;

    enum class Stage
    {
        Idle,
        Delay,
        Hold,
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
