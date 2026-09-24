#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

inline float lfoPreviewValue (int shape, double phase);

inline float lfoShapeValue (int shape, double phase)
{
    const auto wrapped = phase - std::floor (phase);

    switch (shape)
    {
        case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * wrapped);
        case 1: return (float) (1.0 - 4.0 * std::abs (wrapped - 0.5));
        case 2: return (float) (2.0 * wrapped - 1.0);
        case 3: return (float) (1.0 - 2.0 * wrapped);
        case 4: return wrapped < 0.5 ? 1.0f : -1.0f;
        default: break;
    }

    return shape >= 9 && shape <= 11 ? lfoPreviewValue (shape, phase) : 0.0f;
}

// The LFO shape list, in parameter order.
namespace LfoShapes
{
enum
{
    Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, Draw, Steps, Curve,
    SmoothRandom, // glides from one random value to the next each cycle
    Drunk,        // a random walk: each cycle wanders a little from the last
    Chaos,        // a Lorenz attractor: never repeats, never settles
    Count
};

inline bool isStateful (int shape) { return shape >= SmoothRandom && shape <= Chaos; }
} // namespace LfoShapes

// State for the shapes that aren't a function of phase alone.
struct LfoChaos
{
    float previous = 0.0f, target = 0.0f, chaosOut = 0.0f;
    double x = 1.0, y = 1.0, z = 20.0;

    void reset (juce::Random& random)
    {
        previous = target = random.nextFloat() * 2.0f - 1.0f;
        x = (double) random.nextFloat() * 10.0 - 5.0;
        y = (double) random.nextFloat() * 10.0 - 5.0;
        z = 20.0 + (double) random.nextFloat() * 5.0;
        chaosOut = juce::jlimit (-1.0f, 1.0f, (float) (x / 19.0));
    }

    // Once per cycle.
    void onCycle (int shape, juce::Random& random)
    {
        previous = target;

        if (shape == LfoShapes::Drunk)
        {
            auto next = target + (random.nextFloat() * 2.0f - 1.0f) * 0.45f;

            if (next > 1.0f)
                next = 2.0f - next;
            else if (next < -1.0f)
                next = -2.0f - next;

            target = next;
        }
        else
        {
            target = random.nextFloat() * 2.0f - 1.0f;
        }
    }

    // Once per sample, Chaos only. About two orbits per cycle of the rate.
    void advance (double cyclesPerSample)
    {
        const auto dt = juce::jlimit (0.0, 0.01, cyclesPerSample * 1.2);
        const auto dx = 10.0 * (y - x);
        const auto dy = x * (28.0 - z) - y;
        const auto dz = x * y - (8.0 / 3.0) * z;
        x += dx * dt;
        y += dy * dt;
        z += dz * dt;
        chaosOut = juce::jlimit (-1.0f, 1.0f, (float) (x / 19.0));
    }

    float value (int shape, double phase) const
    {
        if (shape == LfoShapes::Chaos)
            return chaosOut;

        const auto t = (float) (phase - std::floor (phase));
        return previous + (target - previous) * t * t * (3.0f - 2.0f * t);
    }
};

// A representative picture of a stateful shape across one display cycle
// (four random segments, or a stretch of the attractor).
inline float lfoPreviewValue (int shape, double phase)
{
    const auto wrapped = phase - std::floor (phase);

    if (shape == LfoShapes::Chaos)
    {
        static const std::vector<float> trace = []
        {
            std::vector<float> values (256);
            double x = 1.0, y = 1.0, z = 20.0;

            for (int i = 0; i < 3000 + 256 * 12; ++i)
            {
                const auto dx = 10.0 * (y - x), dy = x * (28.0 - z) - y, dz = x * y - (8.0 / 3.0) * z;
                x += dx * 0.002;
                y += dy * 0.002;
                z += dz * 0.002;

                if (i >= 3000 && (i - 3000) % 12 == 0)
                    values[(size_t) (i - 3000) / 12] = juce::jlimit (-1.0f, 1.0f, (float) (x / 19.0));
            }

            return values;
        }();

        const auto position = wrapped * 255.0;
        const auto index = (int) position;
        const auto frac = (float) (position - (double) index);
        return trace[(size_t) index] + (trace[(size_t) juce::jmin (255, index + 1)] - trace[(size_t) index]) * frac;
    }

    static const float points[] { 0.1f, 0.85f, -0.4f, 0.3f, -0.9f };
    static const float walk[] { -0.2f, 0.15f, 0.45f, 0.2f, 0.6f };
    const auto* values = shape == LfoShapes::Drunk ? walk : points;
    const auto position = wrapped * 4.0;
    const auto index = juce::jlimit (0, 3, (int) position);
    const auto t = (float) (position - (double) index);
    return values[index] + (values[index + 1] - values[index]) * t * t * (3.0f - 2.0f * t);
}
