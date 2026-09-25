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

    return shape >= 9 && shape <= 15 ? lfoPreviewValue (shape, phase) : 0.0f;
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
    Bounce, Pendulum, Spring, Friction,
    Count
};

inline bool isStateful (int shape) { return shape >= SmoothRandom && shape < Count; }
inline bool isPhysics (int shape) { return shape >= Bounce && shape <= Friction; }
} // namespace LfoShapes

// State for the shapes that aren't a function of phase alone.
struct LfoChaos
{
    float previous = 0.0f, target = 0.0f, chaosOut = 0.0f;
    double x = 1.0, y = 1.0, z = 20.0;
    double position = 1.0, velocity = 0.0, drive = 0.0;
    float physicsOut = 0.0f;

    void resetPhysics (int shape, float a)
    {
        position = shape == LfoShapes::Bounce ? 0.2 + 0.8 * (double) a
                 : shape == LfoShapes::Friction ? -1.0 : 1.0;
        velocity = drive = 0.0;
        physicsOut = (float) (shape == LfoShapes::Bounce ? position * 2.0 - 1.0 : position);
    }

    void kick (float amount)
    {
        velocity = juce::jlimit (-4.0, 4.0, velocity + (double) amount * 2.0);
    }

    void advancePhysics (int shape, double cyclesPerSample, float a, float b)
    {
        const auto dt = juce::jlimit (0.0, 0.02, cyclesPerSample * 2.0);
        if (shape == LfoShapes::Bounce)
        {
            velocity -= 5.0 * dt;
            position += velocity * dt;
            if (position < 0.0)
            {
                position = -position;
                velocity = -velocity * (0.15 + 0.83 * (double) b);
                if (velocity < 0.015) position = velocity = 0.0;
            }
            physicsOut = (float) (position * 2.0 - 1.0);
        }
        else if (shape == LfoShapes::Pendulum)
        {
            velocity += (-4.0 * std::sin (position) - (0.05 + 2.0 * (double) b) * velocity) * dt;
            position += velocity * dt;
            physicsOut = (float) std::sin (position * (0.2 + 1.3 * (double) a));
        }
        else if (shape == LfoShapes::Spring)
        {
            velocity += (-(2.0 + 18.0 * (double) a) * position - (0.05 + 5.0 * (double) b) * velocity) * dt;
            position += velocity * dt;
            physicsOut = (float) position;
        }
        else if (shape == LfoShapes::Friction)
        {
            drive += dt * (0.2 + 2.0 * (double) a);
            if (drive >= 0.25 + 1.5 * (double) b)
            {
                drive = 0.0;
                position = -position * 0.9;
            }
            position += ((position < 0.0 ? -1.0 : 1.0) - position) * dt * 0.1;
            physicsOut = (float) position;
        }
        physicsOut = std::isfinite (physicsOut) ? juce::jlimit (-1.0f, 1.0f, physicsOut) : 0.0f;
    }

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
        if (LfoShapes::isPhysics (shape))
            return physicsOut;
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

    if (LfoShapes::isPhysics (shape))
    {
        if (shape == LfoShapes::Bounce)
        {
            const auto time = wrapped * 3.0;
            auto remaining = time, height = 1.0;
            for (int bounce = 0; bounce < 8; ++bounce)
            {
                const auto duration = std::sqrt (2.0 * height / 5.0) * (bounce == 0 ? 1.0 : 2.0);
                if (remaining < duration)
                {
                    const auto v = bounce == 0 ? 0.0 : std::sqrt (10.0 * height);
                    return (float) juce::jlimit (-1.0, 1.0, 2.0 * juce::jmax (0.0, height + v * remaining - 2.5 * remaining * remaining) - 1.0);
                }
                remaining -= duration;
                height *= 0.45;
            }
            return -1.0f;
        }
        if (shape == LfoShapes::Pendulum)
            return (float) (std::exp (-wrapped * 2.0) * std::cos (wrapped * 12.0));
        if (shape == LfoShapes::Spring)
            return (float) (std::exp (-wrapped * 3.0) * std::cos (wrapped * 18.0));
        return wrapped < 0.22 ? -1.0f : wrapped < 0.45 ? 0.9f : wrapped < 0.78 ? -0.8f : 0.75f;
    }

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
