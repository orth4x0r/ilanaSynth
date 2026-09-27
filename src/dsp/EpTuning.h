#pragma once

#include <juce_core/juce_core.h>

#include <array>

// M7.3 electric pianos: the constants of the tine (Rhodes-style) and reed
// (Wurlitzer-style) models, fitted to the default sounds of Arturia's
// Stage-73 V2 and Wurli V2 (tools/fit_ep.py, references rendered by
// ilanaRefHost into build/reference/ep/). ILANA_EP_TUNING="tine.name=value;
// reed.name=value;..." overrides them for the fitter.
struct EpModelTuning
{
    // Hammer contact (ms at C4, medium velocity and hardness): shorter for
    // harder hits and higher notes, which lets more of the upper modes in.
    float contactMs = 1.36f, contactVelocity = 0.8062f, contactRegister = 0.5512f, contactHardness = 0.6f;
    // Displacement at the pickup (relative to the pickup distance) at full
    // velocity, at MIDI 40 and 84, and how it falls with softer playing.
    float ampLow = 2.736f, ampHigh = 0.6239f, velocityFloor = 0.03594f, velocityCurve = 1.569f;
    // Pickup: distance scale and the tine's offset from the pole piece (or
    // the reed's rest position inside the pickup slot).
    float distance = 1.0f, offset = 0.5888f, offsetRange = 1.0f;
    // How the offset moves per octave above C4 (the treble tines sit more
    // centred on their pickups: odd harmonics).
    float offsetRegister = 0.0f;
    // Fundamental T60 (s at C4) and how it shortens per octave.
    float t60 = 13.79f, t60Register = 0.693f;
    // Tone bar (tine) or reed body: a second mode at the fundamental,
    // detuned and slower, which gives the bloom and beating.
    float barLevel = 0.02109f, barDetune = 1.5f, barT60 = 1.6f;
    // The tip moves on an arc: a small component at twice the fundamental.
    float h2Level = 0.06708f, h2T60 = 0.5165f;
    // Clamped-free beam modes (the ping): ratio, level and T60 (s at C4).
    float beam2Ratio = 6.267f, beam2Level = 0.7426f, beam2T60 = 0.6f;
    float beam3Ratio = 17.55f, beam3Level = 0.65f, beam3T60 = 0.03304f;
    // Output: tone low-pass (Hz at the default DAMP), preamp high-pass (Hz),
    // preamp drive and bias (reed), and gain.
    float toneCut = 2361.0f, highPass = 30.0f, drive = 0.0f, bias = 0.0f, gain = 1.0f;
    // Output level tilt across the keyboard, in dB per octave above C4.
    float gainTilt = 3.429f;
    // T60 (s) of the damper felt once the key is released.
    float damperT60 = 0.12f;
};

struct EpTuning
{
    EpModelTuning tine, reed;

    EpTuning()
    {
        // The reed (Wurlitzer), fitted to Wurli V2 (error 135); the tine
        // above was fitted to Stage-73 V2 (error 124). Both started near 2000.
        // The rest of the reed shares the tine's defaults.
        reed.contactMs = 1.263f;
        reed.contactVelocity = 0.8449f;
        reed.contactRegister = 0.4864f;
        reed.ampLow = 0.9806f;
        reed.ampHigh = 0.1475f;
        reed.velocityFloor = 0.2464f;
        reed.velocityCurve = 0.5878f;
        reed.offset = 0.1224f;
        reed.offsetRegister = 0.002799f;
        reed.t60 = 7.784f;
        reed.t60Register = 1.236f;
        reed.barLevel = 0.0f;
        reed.h2Level = 0.1706f;
        reed.h2T60 = 0.741f;
        reed.beam2Level = 0.07632f;
        reed.beam2T60 = 0.3254f;
        reed.beam3Level = 0.018f;
        reed.beam3T60 = 0.03482f;
        reed.toneCut = 1.22e+04f;
        reed.gainTilt = 3.859f;
        reed.highPass = 67.98f;
        reed.drive = 1.96f;
        reed.bias = -0.38f;
        reed.offsetRange = 0.6f;
        reed.damperT60 = 0.08f;
    }

    static EpTuning& get()
    {
        static EpTuning tuning;
        return tuning;
    }

    bool apply (const juce::String& text)
    {
        auto ok = true;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto key = item.upToFirstOccurrenceOf ("=", false, false).trim();
            if (key.isEmpty())
                continue;
            const auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            auto* model = key.startsWith ("tine.") ? &tine : key.startsWith ("reed.") ? &reed : nullptr;
            const auto name = key.fromFirstOccurrenceOf (".", false, false);
            auto found = false;
            if (model != nullptr)
                for (auto& field : fields())
                    if (name == field.first)
                    {
                        model->*(field.second) = value;
                        found = true;
                    }
            ok = ok && found;
        }
        return ok;
    }

    using Field = std::pair<const char*, float EpModelTuning::*>;
    static const std::array<Field, 32>& fields()
    {
        static const std::array<Field, 32> list {
            Field { "contactMs", &EpModelTuning::contactMs }, Field { "contactVelocity", &EpModelTuning::contactVelocity },
            Field { "contactRegister", &EpModelTuning::contactRegister }, Field { "contactHardness", &EpModelTuning::contactHardness },
            Field { "ampLow", &EpModelTuning::ampLow }, Field { "ampHigh", &EpModelTuning::ampHigh },
            Field { "velocityFloor", &EpModelTuning::velocityFloor }, Field { "velocityCurve", &EpModelTuning::velocityCurve },
            Field { "distance", &EpModelTuning::distance }, Field { "offset", &EpModelTuning::offset },
            Field { "offsetRange", &EpModelTuning::offsetRange },
            Field { "t60", &EpModelTuning::t60 }, Field { "t60Register", &EpModelTuning::t60Register },
            Field { "barLevel", &EpModelTuning::barLevel }, Field { "barDetune", &EpModelTuning::barDetune },
            Field { "barT60", &EpModelTuning::barT60 },
            Field { "h2Level", &EpModelTuning::h2Level }, Field { "h2T60", &EpModelTuning::h2T60 },
            Field { "beam2Ratio", &EpModelTuning::beam2Ratio }, Field { "beam2Level", &EpModelTuning::beam2Level },
            Field { "beam2T60", &EpModelTuning::beam2T60 },
            Field { "beam3Ratio", &EpModelTuning::beam3Ratio }, Field { "beam3Level", &EpModelTuning::beam3Level },
            Field { "beam3T60", &EpModelTuning::beam3T60 },
            Field { "toneCut", &EpModelTuning::toneCut }, Field { "highPass", &EpModelTuning::highPass },
            Field { "drive", &EpModelTuning::drive }, Field { "bias", &EpModelTuning::bias },
            Field { "gain", &EpModelTuning::gain }, Field { "damperT60", &EpModelTuning::damperT60 },
            Field { "gainTilt", &EpModelTuning::gainTilt }, Field { "offsetRegister", &EpModelTuning::offsetRegister },
        };
        return list;
    }
};
