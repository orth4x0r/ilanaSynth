#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

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
        default: return 0.0f;
    }
}
