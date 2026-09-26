#pragma once

#include <juce_core/juce_core.h>

#include <array>

// The piano model's internal constants (Hammer exciter, soundboard), in one
// place. They were fitted (together with the Grand Piano preset's knobs) to
// real grand piano recordings (University of Iowa
// MIS, E1/C4/C7 at mf and ff) with tools/fit_piano.py; the test binary can
// override them (ILANA_PIANO_TUNING="name=value;...") for that fitting. The
// plugin only ever uses the defaults.
struct PianoTuning
{
    // Hammer contact: (contactBase + contactSlope * (1 - hardness)) ms, times
    // (220 / f) ^ contactRegister. Above trebleFrequency the felt stays on
    // for trebleContact cycles.
    float contactBase = 0.1f, contactSlope = 1.9514f, contactRegister = 0.43f;
    float trebleFrequency = 800.0f, trebleContact = 1.0f;
    // Felt pulse sharpness (t e^-t below the treble): 1 + shapeHardness * h.
    float shapeHardness = 1.2317f;
    // Reflection from the near end at the strike point (the strike notch).
    float strikeReflection = 0.4762f, strikePosition = 0.1143f;
    // Velocity response: level ^ velocityCurve.
    float velocityCurve = 4.5681f;
    // Hardness from the HAMMER knob and velocity.
    float hardnessKnob = 0.75f, hardnessVelocity = 0.5f;
    // Decay: the fundamental's T60 = t60Scale * t60Range ^ DECAY at C4,
    // times (261.6 / f) ^ t60Register. DAMP sets the T60 at 2 kHz:
    // lossTop * (lossBottom / lossTop) ^ DAMP seconds.
    float t60Scale = 0.5f, t60Range = 50.0f, t60Register = 0.7434f;
    float lossTop = 54.0f, lossBottom = 0.3f;
    // Coupled strings: bridge loss per pass (times COUPLING).
    float coupling = 0.0064f;
    // Two polarisations: the prompt sound's T60 as a fraction of DECAY's,
    // and the aftersound's share of the strike.
    float promptRatio = 0.2601f, aftersound = 0.4088f;
    // Board knock and felt thump, heard directly.
    float knock = 0.0418f, thump = 0.0f;
    // Soundboard radiation: a 4th-order high-pass, and a broad bump.
    float radiationCut = 94.4704f, bumpFrequency = 224.9766f, bumpGain = 4.0106f, bumpQ = 1.1896f;

    static PianoTuning& get()
    {
        static PianoTuning tuning;
        return tuning;
    }

    // "name=value;name=value": for the fitting tool only.
    bool apply (const juce::String& text)
    {
        auto ok = true;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto name = item.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            auto found = false;
            for (auto& field : fields())
                if (name == field.first)
                {
                    this->*(field.second) = value;
                    found = true;
                }
            ok = ok && (found || name.isEmpty());
        }
        return ok;
    }

private:
    using Field = std::pair<const char*, float PianoTuning::*>;
    static const std::array<Field, 25>& fields()
    {
        static const std::array<Field, 25> list {
            Field { "contactBase", &PianoTuning::contactBase }, Field { "contactSlope", &PianoTuning::contactSlope },
            Field { "contactRegister", &PianoTuning::contactRegister }, Field { "trebleFrequency", &PianoTuning::trebleFrequency },
            Field { "trebleContact", &PianoTuning::trebleContact }, Field { "shapeHardness", &PianoTuning::shapeHardness },
            Field { "strikeReflection", &PianoTuning::strikeReflection }, Field { "strikePosition", &PianoTuning::strikePosition },
            Field { "velocityCurve", &PianoTuning::velocityCurve }, Field { "hardnessKnob", &PianoTuning::hardnessKnob },
            Field { "hardnessVelocity", &PianoTuning::hardnessVelocity }, Field { "t60Scale", &PianoTuning::t60Scale },
            Field { "t60Range", &PianoTuning::t60Range }, Field { "t60Register", &PianoTuning::t60Register },
            Field { "lossTop", &PianoTuning::lossTop }, Field { "lossBottom", &PianoTuning::lossBottom },
            Field { "coupling", &PianoTuning::coupling }, Field { "knock", &PianoTuning::knock },
            Field { "thump", &PianoTuning::thump }, Field { "radiationCut", &PianoTuning::radiationCut },
            Field { "bumpFrequency", &PianoTuning::bumpFrequency }, Field { "bumpGain", &PianoTuning::bumpGain },
            Field { "bumpQ", &PianoTuning::bumpQ },
            Field { "promptRatio", &PianoTuning::promptRatio }, Field { "aftersound", &PianoTuning::aftersound },
        };
        return list;
    }
};
