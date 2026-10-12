#pragma once

#include <cmath>

// A knob that modulation moves once a block, gliding to each new value
// through two one-pole stages: the value and its slope stay continuous, so
// neither a jump nor a kink lands on the block boundary (a physical model
// hears both as a click). It rests exactly on a value it has reached, so an
// unmodulated knob keeps its value bit for bit.
template <typename T>
struct KnobGlide
{
    T mid {}, value {};

    void snap (T target) { mid = value = target; }
    bool moving (T target) const { return value != target || mid != target; }

    void step (T target, T coefficient, T tolerance)
    {
        mid += (target - mid) * coefficient;
        value += (mid - value) * coefficient;
        if (std::abs (target - value) < tolerance && std::abs (target - mid) < tolerance)
            snap (target);
    }

    // Each stage's coefficient for a time constant in seconds.
    static T coefficientFor (double seconds, double sampleRate)
    {
        return (T) (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }
};
