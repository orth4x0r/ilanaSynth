#pragma once

#include <juce_core/juce_core.h>

// M8.3: the low-pass gate's vactrol and filter constants. Re-fitted in M10
// to recorded strikes of a Make Noise Optomix (a vactrol gate of the Buchla
// 292 family; build/reference/lpg, tools/fit_lpg.py): falls 10 / 20 / 30 dB
// after 45 / 135 / 280 ms against the recordings' 52 / 97 / 367 ms (their
// tails carry some room). The first fit used the published vactrol figures
// (Parker and D'Angelo, DAFx 2013). The test binary can override them
// (ILANA_WEST_TUNING="name=value;...").
struct WestCoastTuning
{
    float riseSeconds = 0.012f;
    float fallSeconds = 0.2f, fallSlowing = 6.5f; // fitted to the Optomix strikes
    // Cutoff from a closed gate to an open one, how it follows the cell, and
    // the gain's curve.
    float closedHz = 30.0f, openHz = 18000.0f, cutoffCurve = 0.5f, gainCurve = 2.8f;
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
