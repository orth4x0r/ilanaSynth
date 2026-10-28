// Duplicating and reordering oscillators (processor/OscSlots.cpp, ilana
// 2026-10-26). Included by TableTest.cpp after OscRoutingTests.inc;
// ILANA_OSC_SLOT_TEST=1 runs only these.

namespace OscSlotLab
{
inline float plain (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    auto* parameter = processor.apvts.getParameter (id);
    return parameter != nullptr ? parameter->convertFrom0to1 (parameter->getValue()) : -999.0f;
}

inline int routesInto (IlanaSynthAudioProcessor& processor, int destination)
{
    auto count = 0;
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
        count += processor.readModSlot (slot).destination == destination ? 1 : 0;
    return count;
}

// OSC 1 a sine at full level, OSC 2 a quieter sine an octave up that
// FM-modulates it, an LFO on OSC 1's pitch and one on OSC 2's grain position.
inline void twoOscPatch (IlanaSynthAudioProcessor& p)
{
    setParam (p, "osc2_on", 1.0f);
    setParam (p, "osc2_table", 8.0f);
    setParam (p, "osc2_frame", 0.0f);
    setParam (p, "osc2_route", 3.0f);
    setParam (p, "osc2_level", 0.4f);
    setParam (p, "osc2_semi", 12.0f);
    setParam (p, "osc2_detune", 0.0f);
    setParam (p, "osc2_unison", 1.0f);
    setParam (p, "osc1_unison", 1.0f);
    setParam (p, "osc1_semi", 0.0f);
    setParam (p, "fm_amount", 0.3f); // OSC 2 > OSC 1
    p.assignModSlot ((int) Mod::Source::Lfo1, (int) Mod::Destination::Osc1Pitch, 0.05f);
    p.assignModSlot ((int) Mod::Source::Lfo2, Mod::destinationForParamId ("osc2_grain_pos"), 0.3f);
}

inline double slotRms (const std::vector<float>& x)
{
    double sum = 0.0;
    for (const auto v : x)
        sum += (double) v * v;
    return std::sqrt (sum / juce::jmax ((size_t) 1, x.size()));
}
} // namespace OscSlotLab

void runOscSlotTests()
{
    using namespace OscSlotLab;
    using D = Mod::Destination;

    // A swap trades settings, FM routes and mod routes, and sounds the same.
    {
        IlanaSynthAudioProcessor p;
        twoOscPatch (p);
        p.swapOscillators (0, 1);
        check (plain (p, "osc1_semi") == 12.0f && plain (p, "osc2_semi") == 0.0f
                   && std::abs (plain (p, "osc1_level") - 0.4f) < 1.0e-4f,
               "a swap trades the two oscillators' settings");
        check (std::abs (plain (p, "fm_1to2") - 0.3f) < 1.0e-4f && plain (p, "fm_amount") < 1.0e-6f,
               "a swap moves the FM route with its oscillators (OSC 2 > 1 becomes OSC 1 > 2)");
        check (routesInto (p, (int) D::Osc2Pitch) == 1 && routesInto (p, (int) D::Osc1Pitch) == 0
                   && routesInto (p, Mod::destinationForParamId ("osc1_grain_pos")) == 1,
               "a swap moves the mod routes into the oscillators' knobs");
    }
    {
        const auto before = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { twoOscPatch (p); }, 57, 0.5);
        const auto swapped = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            twoOscPatch (p);
            p.swapOscillators (0, 1);
        }, 57, 0.5);
        const auto twice = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            twoOscPatch (p);
            p.swapOscillators (0, 1);
            p.swapOscillators (1, 0);
        }, 57, 0.5);
        auto worst = 0.0f, worstTwice = 0.0f;
        for (size_t i = 0; i < before.size(); ++i)
        {
            worst = juce::jmax (worst, std::abs (before[i] - swapped[i]));
            worstTwice = juce::jmax (worstTwice, std::abs (before[i] - twice[i]));
        }
        check (slotRms (before) > 0.01 && worst < 1.0e-3f,
               "a swapped patch sounds the same (worst " + juce::String (worst, 5) + ", rms " + juce::String (slotRms (before), 4) + ")");
        check (worstTwice == 0.0f, "swapping twice gives back the same sound (worst " + juce::String (worstTwice, 6) + ")");
    }

    // Duplicate: the copy lands right after the original, with its routes.
    {
        IlanaSynthAudioProcessor p;
        twoOscPatch (p);
        setParam (p, "osc1_semi", 5.0f);
        check (p.isOscillatorShown (0) && p.isOscillatorShown (1) && ! p.isOscillatorShown (2),
               "a new patch shows OSC 1 and 2");
        const auto copy = p.duplicateOscillator (0);
        check (copy == 1, "the copy of OSC 1 is OSC 2 (got " + juce::String (copy) + ")");
        check (p.isOscillatorShown (2) && plain (p, "sub_semi") == 12.0f && std::abs (plain (p, "sub_level") - 0.4f) < 1.0e-4f,
               "the old OSC 2 moves down to OSC 3");
        check (plain (p, "osc2_semi") == 5.0f && plain (p, "osc2_on") > 0.5f
                   && std::abs (plain (p, "osc2_level") - plain (p, "osc1_level")) < 1.0e-6f,
               "the copy has the original's settings");
        check (routesInto (p, (int) D::Osc1Pitch) == 1 && routesInto (p, (int) D::Osc2Pitch) == 1
                   && routesInto (p, Mod::destinationForParamId ("sub_grain_pos")) == 1,
               "the copy gets its own copy of the original's mod routes, the moved oscillator keeps its own");
        check (std::abs (plain (p, "fm_3to1") - 0.3f) < 1.0e-4f && std::abs (plain (p, "fm_3to2") - 0.3f) < 1.0e-4f
                   && plain (p, "fm_1to2") < 1.0e-6f && plain (p, "fm_amount") < 1.0e-6f,
               "the copy hears the same FM as the original, and the two don't modulate each other");
    }
    {
        const auto single = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { p.removeOscillator (1); }, 57, 0.3);
        const auto doubled = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            p.removeOscillator (1);
            p.duplicateOscillator (0);
        }, 57, 0.3);
        const auto ratio = slotRms (doubled) / juce::jmax (1.0e-9, slotRms (single));
        check (ratio > 1.5, "a duplicated sine plays louder than one (x" + juce::String (ratio, 2) + ")");
    }
    {
        IlanaSynthAudioProcessor p;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            p.addOscillator (osc);
        check (p.duplicateOscillator (0) == -1, "with all six shown there is no slot to duplicate into");
    }

    // A stale removed oscillator's routes don't come back with a copy.
    {
        IlanaSynthAudioProcessor p;
        p.assignModSlot ((int) Mod::Source::Lfo3, (int) D::Osc2Pitch, 0.5f);
        p.removeOscillator (1);
        p.duplicateOscillator (0);
        check (routesInto (p, (int) D::Osc2Pitch) == 0, "a duplicate clears the routes the slot kept from a removed oscillator");
    }

    // Move: OSC 1 to the end of three.
    {
        IlanaSynthAudioProcessor p;
        p.addOscillator (2);
        setParam (p, "osc1_semi", 1.0f);
        setParam (p, "osc2_semi", 2.0f);
        setParam (p, "sub_semi", 3.0f);
        const auto landed = p.moveOscillator (0, 2);
        check (landed == 2 && plain (p, "osc1_semi") == 2.0f && plain (p, "osc2_semi") == 3.0f && plain (p, "sub_semi") == 1.0f,
               "moving OSC 1 last shifts the others up");
    }

    // Every per-oscillator setting exists on all six (or the swap would drop it).
    {
        IlanaSynthAudioProcessor p;
        juce::StringArray missing;
        for (auto* parameter : p.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                if (withId->paramID.startsWith ("osc1_"))
                    for (const auto* prefix : OscillatorIds::prefixes)
                        if (p.apvts.getParameter (juce::String (prefix) + withId->paramID.substring (4)) == nullptr)
                            missing.add (juce::String (prefix) + withId->paramID.substring (4));
        if (! missing.isEmpty())
            std::cout << "  (per-oscillator settings missing on some slots: " << missing.joinIntoString (", ") << ")" << std::endl;
        check (missing.isEmpty(), "every OSC 1 setting exists on all six oscillators");
    }
}
