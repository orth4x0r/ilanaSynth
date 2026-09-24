#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

class PolyBlepOsc
{
public:
    enum class Shape
    {
        Sine = 0,
        Square,
        Saw
    };

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateIncrement();
    }

    void setFrequency (double newFrequency)
    {
        frequency = juce::jlimit (0.0, sampleRate * 0.45, newFrequency);
        updateIncrement();
    }

    void setShape (Shape newShape) { shape = newShape; }

    void resetPhase (double newPhase = 0.0) { phase = newPhase; }

    float getNextSample()
    {
        auto value = 0.0;

        switch (shape)
        {
            case Shape::Sine:
                value = std::sin (juce::MathConstants<double>::twoPi * phase);
                break;

            case Shape::Saw:
                value = 2.0 * phase - 1.0 - polyBlep (phase);
                break;

            case Shape::Square:
                value = (phase < 0.5 ? 1.0 : -1.0)
                        + polyBlep (phase)
                        - polyBlep (phase < 0.5 ? phase + 0.5 : phase - 0.5);
                break;
        }

        phase += increment;

        if (phase >= 1.0)
            phase -= 1.0;

        return (float) value;
    }

private:
    double polyBlep (double t) const
    {
        if (increment <= 0.0)
            return 0.0;

        if (t < increment)
        {
            const auto x = t / increment;
            return x + x - x * x - 1.0;
        }

        if (t > 1.0 - increment)
        {
            const auto x = (t - 1.0) / increment;
            return x * x + x + x + 1.0;
        }

        return 0.0;
    }

    void updateIncrement() { increment = sampleRate > 0.0 ? frequency / sampleRate : 0.0; }

    Shape shape = Shape::Square;
    double sampleRate = 44100.0;
    double frequency = 440.0;
    double phase = 0.0;
    double increment = 0.0;
};
