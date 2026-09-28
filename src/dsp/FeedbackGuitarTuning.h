#pragma once

#include <juce_core/juce_core.h>

// M8.5: the Feedback exciter's amp, speaker and air-path constants, fitted
// by tools/fit_feedback.py (bloom time and the harmonic it settles on). The
// test binary can override them (ILANA_FEEDBACK_TUNING="name=value;...").
struct FeedbackGuitarTuning
{
    float gainRangeDb = 46.0f, pickupLevel = 1.0f, loopGain = 0.011f; // fitted (tools/fit_feedback.py)
    float speakerLow = 90.0f, speakerHigh = 2000.0f;
    float coneHz = 110.0f, coneDb = 4.0f, presenceHz = 2500.0f, presenceDb = 4.0f;
    // The amp and the room's own latency, in samples (48 kHz).
    float latencySamples = 0.0f;

    static FeedbackGuitarTuning& get()
    {
        static FeedbackGuitarTuning tuning;
        return tuning;
    }

    bool apply (const juce::String& text)
    {
        auto ok = true;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto name = item.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            float* fields[] { &gainRangeDb, &pickupLevel, &loopGain, &speakerLow, &speakerHigh, &coneHz, &coneDb,
                              &presenceHz, &presenceDb, &latencySamples };
            const char* names[] { "gainRangeDb", "pickupLevel", "loopGain", "speakerLow", "speakerHigh", "coneHz", "coneDb",
                                  "presenceHz", "presenceDb", "latencySamples" };
            auto found = name.isEmpty();
            for (size_t i = 0; i < std::size (names); ++i)
                if (name == names[i])
                {
                    *fields[i] = value;
                    found = true;
                }
            ok = ok && found;
        }
        return ok;
    }
};
