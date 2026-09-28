#pragma once

#include <juce_core/juce_core.h>

// M8.5: the Feedback exciter's amp, speaker and air-path constants, fitted
// by tools/fit_feedback.py. Re-fitted in M10 to two recordings of a guitar
// feeding back (build/reference/feedback: the fundamental swells 12-21 dB
// and settles after 2-2.5 s): score 184 -> 127. Gap: the fundamental wins in
// 6 of 15 note/distance cases (2 of 2 recorded) and the median bloom is
// 1.0 s. The test binary can override them
// (ILANA_FEEDBACK_TUNING="name=value;...").
struct FeedbackGuitarTuning
{
    float gainRangeDb = 46.0f, pickupLevel = 0.7f, loopGain = 0.011f; // fitted (tools/fit_feedback.py)
    float speakerLow = 45.0f, speakerHigh = 3000.0f;
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
