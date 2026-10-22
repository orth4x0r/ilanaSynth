// Tempo sync for the AW Delay module's echoes (FX type 35): the 0..1 value of
// an algorithm's time knob that gives a delay of `seconds`, worked back from
// each plugin's own time maths (TapeDelay2.h, PitchDelay.h, PurestEcho.h,
// Doublelay.h), so the knob lands on the beat. A time past an algorithm's
// longest (or under its shortest) stops at that end. StarChild2 has no time
// knob, so it has nothing to sync.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Registry.h"

namespace airwindows
{
namespace delaysync
{
// Airwindows runs one "cycle" every floor(rate / 44.1 kHz) samples (1 to 4).
inline double cycleRate (double sampleRate)
{
    const auto cycleEnd = std::clamp ((int) std::floor (sampleRate / 44100.0), 1, 4);
    return sampleRate / cycleEnd;
}

// The tape loops: a head moving `speed` slots a cycle round an 88200-slot
// loop comes back after 88200 / speed cycles; speed = knob^4 * range + 1.
inline float tapeLoopKnob (double seconds, double sampleRate, double range)
{
    const auto speed = 88200.0 / std::max (1.0e-6, seconds * cycleRate (sampleRate));
    return (float) std::pow (std::clamp ((speed - 1.0) / range, 0.0, 1.0), 0.25);
}

// The algorithm's knob (0-based, as Registry.h lists them) that sets its
// delay time, or false. Doublelay has two (Delay L and Delay R).
inline bool isTimeKnob (const char* name, int knob)
{
    if (std::strcmp (name, "TapeDelay2") == 0 || std::strcmp (name, "PitchDelay") == 0 || std::strcmp (name, "PurestEcho") == 0)
        return knob == 0;
    if (std::strcmp (name, "Doublelay") == 0)
        return knob == 1 || knob == 2;
    return false;
}

inline bool isTimeKnob (int algorithm, int knob)
{
    return algorithm >= 0 && algorithm < count() && isTimeKnob (registry()[(size_t) algorithm].name, knob);
}

// Has the algorithm a time to sync at all?
inline bool hasTime (int algorithm)
{
    for (int knob = 0; knob < 5; ++knob)
        if (isTimeKnob (algorithm, knob))
            return true;
    return false;
}

// The time knob's value for a delay of `seconds` (call only for a time knob).
inline float knobFor (int algorithm, double seconds, double sampleRate)
{
    const char* id = registry()[(size_t) algorithm].name;

    if (std::strcmp (id, "TapeDelay2") == 0)
        return tapeLoopKnob (seconds, sampleRate, 25.0);
    if (std::strcmp (id, "PitchDelay") == 0)
        return tapeLoopKnob (seconds, sampleRate, 20.0);
    if (std::strcmp (id, "PurestEcho") == 0)
    {
        // A tap at loopLimit * knob^2 * 0.999 samples (no rate scaling).
        const auto loopLimit = (double) (int) (65535 * 0.499);
        return (float) std::sqrt (std::clamp (seconds * sampleRate / (loopLimit * 0.999), 0.0, 1.0));
    }
    if (std::strcmp (id, "Doublelay") == 0)
    {
        // knob * (int) (48000 * trim) cycles, trim = rate / cycleEnd / 48001.
        const auto trim = std::clamp (cycleRate (sampleRate) / 48001.0, 0.0, 0.99999);
        const auto longest = (double) (int) (48000.0 * trim);
        return (float) std::clamp (seconds * cycleRate (sampleRate) / std::max (1.0, longest), 0.0, 1.0);
    }
    return 0.0f;
}
} // namespace delaysync
} // namespace airwindows
