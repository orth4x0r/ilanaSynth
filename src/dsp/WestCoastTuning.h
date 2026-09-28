#pragma once

#include <juce_core/juce_core.h>

// M8.3: the low-pass gate's vactrol and filter constants, fitted to the
// published measurements of the Buchla 292's vactrol (Parker and D'Angelo,
// DAFx 2013: lights in about 12 ms, 63 % dark after about 250 ms, slowing
// as it darkens) by tools/fit_lpg.py. The test binary can override them
// (ILANA_WEST_TUNING="name=value;...").
struct WestCoastTuning
{
    float riseSeconds = 0.012f;
    float fallSeconds = 0.325f, fallSlowing = 2.4f; // fitted: 63 % dark at 248 ms, 90 % at 992 ms
    // Cutoff from a closed gate to an open one, how it follows the cell, and
    // the gain's curve.
    float closedHz = 30.0f, openHz = 18000.0f, cutoffCurve = 0.5f, gainCurve = 2.5f;
    // A strike: how long the LED is lit (seconds).
    float strikeSeconds = 0.01f;

    static WestCoastTuning& get()
    {
        static WestCoastTuning tuning;
        return tuning;
    }

    bool apply (const juce::String& text)
    {
        auto ok = true;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto name = item.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            if (name == "riseSeconds") riseSeconds = value;
            else if (name == "fallSeconds") fallSeconds = value;
            else if (name == "fallSlowing") fallSlowing = value;
            else if (name == "closedHz") closedHz = value;
            else if (name == "openHz") openHz = value;
            else if (name == "cutoffCurve") cutoffCurve = value;
            else if (name == "gainCurve") gainCurve = value;
            else if (name == "strikeSeconds") strikeSeconds = value;
            else ok = ok && name.isEmpty();
        }
        return ok;
    }
};
