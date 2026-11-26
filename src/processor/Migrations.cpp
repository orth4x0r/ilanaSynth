#include "ProcessorInternal.h"
#include "Migrations.h"
#include "../dsp/LfoSim.h"

// Every migration that keeps an old patch or factory recipe sounding as it
// was heard, and the table that runs them (see Migrations.h; to add one,
// docs/ARCHITECTURE.md, "Migrations").

// Before v1.1, OSC 3 doubled as the sub: its table list began with four sub
// shapes (Shape/Sine/PWM/Analog Saw), it always played an octave or two
// down, and the noise followed its route. A patch that used it as a plain
// sub moves to the dedicated SUB (same table, same level, same octave); any
// other use stays on OSC 3 with the octave drop folded into SEMI and its
// table renumbered. Works on plain stored values, before they reach the
// parameters, so old table numbers aren't clamped by the new list.
void IlanaSynthAudioProcessor::migrateLegacyOsc3 (const std::function<float (const juce::String&, float)>& get,
                                                  const std::function<void (const juce::String&, float)>& set)
{
    const auto table = juce::roundToInt (get ("sub_table", 0.0f));
    const auto shape = juce::roundToInt (get ("sub_shape", 1.0f));
    const auto octave = juce::roundToInt (get ("sub_octave", 0.0f));
    const auto semi = juce::roundToInt (get ("sub_semi", 0.0f));
    const auto route = get ("sub_route", 0.0f);

    auto modulated = false;
    const Mod::Destination osc3Targets[] { Mod::Destination::SubLevel, Mod::Destination::SubPitch, Mod::Destination::SubFrame,
                                           Mod::Destination::SubSampleStart, Mod::Destination::SubSampleEnd,
                                           Mod::Destination::SubDetune, Mod::Destination::SubPan, Mod::Destination::SubWarp,
                                           Mod::Destination::SubBlend, Mod::Destination::SubSpread };

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);

        if (get (prefix + "_src", 0.0f) < 0.5f || std::abs (get (prefix + "_amt", 0.0f)) < 1.0e-6f)
            continue;

        const auto destination = juce::roundToInt (get (prefix + "_dst", 0.0f));

        for (const auto target : osc3Targets)
            modulated = modulated || destination == (int) target;
    }

    const auto plainSub = table <= 3
                          && juce::roundToInt (get ("sub_mode", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_unison", 1.0f)) <= 1
                          && juce::roundToInt (get ("sub_warp", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_chord", 0.0f)) == 0
                          && semi == 0 && std::abs (get ("sub_fine", 0.0f)) < 0.01f
                          && std::abs (get ("sub_pan", 0.0f)) < 0.001f
                          && get ("sub_frame", 0.0f) < 0.001f
                          && ! modulated;

    const auto shapeForTable = table == 0 ? shape : (table == 1 ? 0 : (table == 2 ? 1 : 2));
    set ("subosc_route", route);

    if (plainSub)
    {
        set ("subosc_on", get ("sub_on", 1.0f));
        set ("subosc_level", get ("sub_level", 0.3f));
        set ("sub_shape", (float) shapeForTable);
        set ("sub_octave", (float) octave);
        set ("sub_on", 0.0f);
        set ("sub_table", 0.0f);
        return;
    }

    // OSC 3 was a real oscillator: keep it, in the new table numbering, at
    // the same pitch.
    const int shapeTables[] { 8, 6, 10 };
    set ("subosc_on", 0.0f);
    set ("sub_on", get ("sub_on", 1.0f));
    set ("sub_level", get ("sub_level", 0.3f));
    set ("sub_table", (float) (table >= 4 ? table - 4 : shapeTables[juce::jlimit (0, 2, shapeForTable)]));
    set ("sub_semi", (float) juce::jlimit (-24, 24, semi + (octave == 0 ? -12 : -24)));
}

int IlanaSynthAudioProcessor::migrateStringKnobs (const std::function<float (const juce::String&, float)>& get,
                                                 const std::function<void (const juce::String&, float)>& set)
{
    auto changed = 0;

    for (const auto* prefixText : OscillatorIds::prefixes)
    {
        const juce::String prefix (prefixText);
        const auto excite = juce::roundToInt (get (prefix + "_excite", 0.0f));
        const auto remap = [&] (const char* suffix, const std::function<float (float)>& convert)
        {
            const auto id = prefix + suffix;
            const auto old = get (id, 0.0f);

            if (old == 0.0f)
                return;

            const auto converted = juce::jlimit (0.0f, 1.0f, convert (old));

            if (converted != old)
            {
                set (id, converted);
                ++changed;
            }
        };

        // A position only mirrors past the middle of the string: the knob
        // now spans bridge to middle (the Piano already did, and its strike
        // keeps its scale; the bow clamped at the middle).
        const auto folded = [] (float v) { return 2.0f * juce::jmin (v, 1.0f - v); };
        const auto clamped = [] (float v) { return juce::jmin (1.0f, 2.0f * v); };

        if (excite != 7 && excite != 8 && excite != 9)
            remap ("_string_excite_pos", excite == 4 ? std::function<float (float)> (clamped) : std::function<float (float)> (folded));

        remap ("_string_pick_pos", folded);
        remap ("_string_pickup", folded);

        // The plucked string's stiffness (an allpass of coefficient -0.7 at
        // full) now reaches -0.95; the Piano's, the hammer's and the Modal
        // Piano's stiffness keep their own scale.
        if (excite != 4 && excite != 5 && excite != 9 && excite != 11)
            remap ("_string_stiffness", [] (float v) { return v * 0.7f / 0.95f; });

        // REGISTER's key scale doubled and its steps doubled again.
        remap ("_register", [] (float v) { return v * 0.25f; });
        // COUPLE grew 30-fold and takes the square, for finer low settings.
        remap ("_couple", [] (float v) { return std::sqrt (v / 30.0f); });
        // RATTLE lost its threshold and BUZZ's contact came down.
        remap ("_fret_rattle", [] (float v) { return v * 0.12f; });
        remap ("_bridge_buzz", [] (float v) { return v * 0.25f; });
    }

    return changed;
}

int IlanaSynthAudioProcessor::migrateDenseBoardMix (const std::function<float (const juce::String&, float)>& get,
                                                   const std::function<void (const juce::String&, float)>& set,
                                                   bool mixKnob)
{
    if (juce::roundToInt (get ("sb_model", 0.0f)) != 1)
        return 0;

    if (mixKnob)
        set ("sb_mix", get ("sb_mix", 0.5f) / DenseSoundboard::mixRange);

    // Routes and macros into MIX were heard at the old reach too (Toy Grand's and
    // Dense Board Piano's BOARD macro): their depths shrink by the same factor,
    // so turning a macro fades the wood as it did instead of cutting it off.
    const auto mixDestination = Mod::destinationForParamId ("sb_mix");
    for (int slot = 1; slot <= Mod::maxSlots && mixDestination > 0; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        if (juce::roundToInt (get (prefix + "_dst", 0.0f)) != mixDestination)
            continue;
        if (const auto depth = get (prefix + "_amt", 0.0f); depth != 0.0f)
            set (prefix + "_amt", depth / DenseSoundboard::mixRange);
    }
    return 1;
}

int IlanaSynthAudioProcessor::migrateDenseBoardBypass (const std::function<float (const juce::String&, float)>& get,
                                                      const std::function<void (const juce::String&, float)>& set,
                                                      bool classic)
{
    if ((juce::roundToInt (get ("sb_model", 0.0f)) == 1) == classic)
        return 0;

    constexpr auto reach = DenseSoundboard::colourReach;
    set ("sb_mix", reach + juce::jlimit (0.0f, 1.0f, get ("sb_mix", 0.5f)) * (1.0f - reach));

    const auto mixDestination = Mod::destinationForParamId ("sb_mix");
    for (int slot = 1; slot <= Mod::maxSlots && mixDestination > 0; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        if (juce::roundToInt (get (prefix + "_dst", 0.0f)) != mixDestination)
            continue;
        if (const auto depth = get (prefix + "_amt", 0.0f); depth != 0.0f)
            set (prefix + "_amt", depth * (1.0f - reach));
    }
    return 1;
}

int IlanaSynthAudioProcessor::migrateOperatorModes (const std::function<float (const juce::String&, float)>& get,
                                                   const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;

    for (const auto* prefixText : OscillatorIds::prefixes)
    {
        const juce::String prefix (prefixText);

        if (juce::roundToInt (get (prefix + "_mode", 0.0f)) != OscMode::wavetable)
            continue;

        const auto tuned = juce::roundToInt (get (prefix + "_tune", (float) OscTuning::Semitones)) != OscTuning::Semitones;
        const auto onOperatorEg = juce::roundToInt (get (prefix + "_amp_env", 0.0f)) == OperatorEg::envelopeChoice;

        if (tuned || onOperatorEg)
        {
            set (prefix + "_mode", (float) OscMode::fmOperator);
            ++moved;
        }
    }

    return moved;
}

int IlanaSynthAudioProcessor::migrateFilterEnvAmounts (const std::function<float (const juce::String&, float)>& get,
                                                       const std::function<void (const juce::String&, float)>& set)
{
    using Mod::Destination;
    using Mod::Source;
    auto moved = 0;

    // The first slot with no source and no destination.
    const auto freeSlot = [&get]
    {
        for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        {
            const auto prefix = "mod" + juce::String (slot);
            if (juce::roundToInt (get (prefix + "_src", 0.0f)) == 0 && juce::roundToInt (get (prefix + "_dst", 0.0f)) == 0)
                return slot;
        }
        return 0;
    };
    const auto route = [&] (int slot, Source source, Destination destination, Source via, float amount)
    {
        const auto prefix = "mod" + juce::String (slot);
        set (prefix + "_src", (float) (int) source);
        set (prefix + "_dst", (float) (int) destination);
        set (prefix + "_aux", (float) (int) via);
        set (prefix + "_amt", amount);
    };

    // VELOCITY (filter_velocity) scaled the amount of both filters' envelopes; it now scales the FILTER ENV source
    // itself, as the other envelopes' VELOCITY knobs do. So when Filter 1's amount becomes one route, the knob stays
    // and carries the velocity share; with other FILTER ENV routes already in the patch (they were never scaled) or
    // no amount to carry, the share is a second route via VELOCITY instead and the knob goes to 0.
    const auto velocity = juce::jlimit (0.0f, 1.0f, get ("filter_velocity", 0.5f));
    auto otherFilterEnvRoutes = 0;
    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        if (juce::roundToInt (get ("mod" + juce::String (slot) + "_src", 0.0f)) == (int) Source::FilterEnv
            && juce::roundToInt (get ("mod" + juce::String (slot) + "_dst", 0.0f)) != 0)
            ++otherFilterEnvRoutes;

    auto keepVelocityKnob = false, leftInPlace = false;

    // (The old default of Filter 1's amount was 1.5 octaves: a patch that
    // never stored one still had it.) The routes are written in the old
    // scale, 6 octaves at 100 %; migrateModDepthRanges, run after this on
    // every path, takes them to the knob-wide one.
    for (const auto filter : { 1, 2 })
    {
        const juce::String prefix (filter == 1 ? "f1" : "f2");
        const auto octaves = get (prefix + "_env", filter == 1 ? 1.5f : 0.0f);

        if (std::abs (octaves) < 1.0e-6f)
            continue;

        const auto first = freeSlot();
        if (first == 0)
        {
            leftInPlace = true;
            continue;
        }

        const auto source = filter == 1 ? Source::FilterEnv : Source::FilterEnv2;
        const auto destination = filter == 1 ? Destination::Filter1Cutoff : Destination::Filter2Cutoff;

        if (filter == 1 && otherFilterEnvRoutes == 0)
        {
            route (first, source, destination, Source::None, octaves / 6.0f);
            keepVelocityKnob = true;
        }
        else
        {
            route (first, source, destination, Source::None, octaves * (1.0f - velocity) / 6.0f);

            if (velocity > 1.0e-6f)
            {
                const auto second = freeSlot();
                if (second == 0)
                {
                    // No room for the velocity share: give the first route the
                    // whole amount rather than lose the sweep.
                    set ("mod" + juce::String (first) + "_amt", octaves / 6.0f);
                }
                else
                    route (second, source, destination, Source::Velocity, octaves * velocity / 6.0f);
            }
        }

        set (prefix + "_env", 0.0f);
        ++moved;
    }

    // The knob keeps its value only where it was folded into the one route; anywhere else the routes carry the share
    // (or a left amount still wants it), so it must not scale the patch's other FILTER ENV routes.
    if (keepVelocityKnob || leftInPlace)
        set ("filter_velocity", velocity);
    else if (velocity > 1.0e-6f)
        set ("filter_velocity", 0.0f);

    return moved;
}

int IlanaSynthAudioProcessor::migrateEnvelopeTension (const std::function<float (const juce::String&, float)>& get,
                                                      const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;
    juce::StringArray prefixes { "amp", "fe", "f2e", "me", "e4" };
    for (int env = 6; env <= 16; ++env)
        prefixes.add ("env" + juce::String (env));

    for (const auto& prefix : prefixes)
    {
        const auto tension = get (prefix + "_curve", 0.0f);

        if (std::abs (tension) < 1.0e-6f)
            continue;

        // The envelope bent each segment by (TENSION + its own), clamped.
        for (const auto* segment : { "_acurve", "_dcurve", "_rcurve" })
            set (prefix + segment, juce::jlimit (-1.0f, 1.0f, get (prefix + segment, 0.0f) + tension));

        set (prefix + "_curve", 0.0f);
        ++moved;
    }

    return moved;
}

int IlanaSynthAudioProcessor::migrateWarpEnvelopes (const std::function<float (const juce::String&, float)>& get,
                                                    const std::function<void (const juce::String&, float)>& set)
{
    using Mod::Source;
    auto moved = 0;
    const auto destinations = Mod::getDestinationNames();

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        const auto choice = juce::roundToInt (get (prefix + "_pd_env", 0.0f));

        if (choice <= 0)
            continue;

        // ENV 1 to ENV 5 are the amp, filter, filter 2, mod and ENV 5 envelopes,
        // then ENV 6 to 16, then the MSEG.
        const auto index = choice - 1;
        const auto source = index == 0 ? Source::AmpEnv : index == 1 ? Source::FilterEnv : index == 2 ? Source::FilterEnv2
                          : index == 3 ? Source::ModEnv : index == 4 ? Source::Env4
                          : index <= 15 ? (Source) ((int) Source::Env6 + (index - 5)) : Source::Mseg;
        const auto amount = get (prefix + "_pd_env_amt", 1.0f);
        const auto name = "Osc" + juce::String (osc + 1) + " Warp";
        const auto warpDestination = destinations.indexOf (name);
        const auto warp2Destination = destinations.indexOf (name + " 2");
        auto placed = true;

        const auto freeSlot = [&get]
        {
            for (int slot = 1; slot <= Mod::maxSlots; ++slot)
            {
                const auto slotPrefix = "mod" + juce::String (slot);
                if (juce::roundToInt (get (slotPrefix + "_src", 0.0f)) == 0 && juce::roundToInt (get (slotPrefix + "_dst", 0.0f)) == 0)
                    return slot;
            }
            return 0;
        };
        const auto route = [&] (int destination)
        {
            const auto slot = freeSlot();
            if (slot == 0 || destination < 0)
                return false;
            const auto slotPrefix = "mod" + juce::String (slot);
            set (slotPrefix + "_src", (float) (int) source);
            set (slotPrefix + "_dst", (float) destination);
            set (slotPrefix + "_aux", 0.0f);
            set (slotPrefix + "_amt", amount);
            return true;
        };

        // The envelope opened both stages of the chain. A stage that is not
        // set plays nothing, so it gets no route.
        if (juce::roundToInt (get (prefix + "_warp", 0.0f)) != 0)
            placed = route (warpDestination) && placed;
        if (juce::roundToInt (get (prefix + "_warp2", 0.0f)) != 0)
            placed = route (warp2Destination) && placed;

        if (! placed)
            continue; // no free slot: the old settings stay, and still play

        set (prefix + "_pd_env", 0.0f);
        set (prefix + "_pd_env_amt", 1.0f);
        ++moved;
    }

    return moved;
}

void IlanaSynthAudioProcessor::migrateRedundantKnobs (const std::function<float (const juce::String&, float)>& get,
                                                      const std::function<void (const juce::String&, float)>& set)
{
    migrateFilterEnvAmounts (get, set);
    migrateEnvelopeTension (get, set);
    migrateWarpEnvelopes (get, set);
}

int IlanaSynthAudioProcessor::migrateModDepthRanges (const std::function<float (const juce::String&, float)>& get,
                                                     const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        const auto factor = Mod::Range::legacyDepthFactor (juce::roundToInt (get (prefix + "_dst", 0.0f)));
        const auto depth = get (prefix + "_amt", 0.0f);

        if (factor != 1.0f && depth != 0.0f)
        {
            set (prefix + "_amt", depth * factor);
            ++moved;
        }
    }

    return moved;
}

void IlanaSynthAudioProcessor::migrateModDepthRanges()
{
    migrateModDepthRanges ([this] (const juce::String& id, float fallback)
                           {
                               if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                                   return ranged->convertFrom0to1 (ranged->getValue());
                               return fallback;
                           },
                           [this] (const juce::String& id, float value)
                           {
                               if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                                   ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
                           });
}

void IlanaSynthAudioProcessor::migrateOperatorModes()
{
    migrateOperatorModes ([this] (const juce::String& id, float fallback)
                          {
                              const auto* value = apvts.getRawParameterValue (id);
                              return value != nullptr ? value->load() : fallback;
                          },
                          [this] (const juce::String& id, float value)
                          {
                              if (auto* parameter = apvts.getParameter (id))
                                  parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                          });
}

int IlanaSynthAudioProcessor::migrateBodyLevel (const std::function<float (const juce::String&, float)>& get,
                                               const std::function<void (const juce::String&, float)>& set)
{
    // (Classic, body_type 0, keeps its own resonator and never changed.)
    if (juce::roundToInt (get ("body_type", 0.0f)) == 0 || get ("body_legacy_level", 0.0f) > 0.5f)
        return 0;
    set ("body_legacy_level", 1.0f);
    return 1;
}

int IlanaSynthAudioProcessor::migrateGlideTime (const std::function<float (const juce::String&, float)>& get,
                                               const std::function<void (const juce::String&, float)>& set)
{
    constexpr auto factor = 16.0f;  // the old glide's slowness: one coefficient step per 16 samples
    constexpr auto maxGlide = 2.0f; // the knob's range (ParameterLayout)
    constexpr auto skew = 0.13f;    // and its skew
    auto changed = 0;

    // At or under 1 ms GLIDE is off (Voice::startNote); those stay off.
    if (const auto glide = get ("glide", 0.0f); glide > 0.001f)
    {
        set ("glide", juce::jmin (glide * factor, maxGlide));
        ++changed;
    }

    // A route adds to the knob's skewed position, (seconds / 2) ^ skew; there
    // x16 in seconds is x16 ^ skew, so the routes and macros reach as far.
    const auto depthFactor = std::pow (factor, skew);
    const auto glideDestination = Mod::destinationForParamId ("glide");
    for (int slot = 1; slot <= Mod::maxSlots && glideDestination > 0; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        if (juce::roundToInt (get (prefix + "_dst", 0.0f)) != glideDestination)
            continue;
        if (const auto depth = get (prefix + "_amt", 0.0f); depth != 0.0f)
        {
            set (prefix + "_amt", juce::jlimit (-1.0f, 1.0f, depth * depthFactor));
            ++changed;
        }
    }
    return changed;
}

// ---- The migration table (see Migrations.h) ----

namespace
{
    using Processor = IlanaSynthAudioProcessor;
    using Missing = PatchValues::MissingId;
    using Duplicates = FactoryPatchValues::Duplicates;
    using Migrations::SavedStage;
    using Migrations::FactoryStage;

    void legacyOsc3 (PatchValues& values, int) { Processor::migrateLegacyOsc3 (values.getter(), values.setter()); }

    // Step 45: the grain position became its own parameter. Before it, the
    // cloud sat at Sample Start, so an older patch takes that value (as saved:
    // a string in a file, not through a float).
    void savedGrainPosition (PatchValues& values, int)
    {
        auto* tree = dynamic_cast<TreePatchValues*> (&values);
        if (tree == nullptr)
        {
            jassertfalse;
            return;
        }

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
            const auto start = tree->find (prefix + "_sample_start");

            if (! start.isValid() || ! start.hasProperty ("value"))
                continue;

            tree->findOrAdd (prefix + "_grain_pos").setProperty ("value", start.getProperty ("value"), nullptr);
        }
    }

    // 2026-10-23: Lorenz loops once per cycle of RATE (it looped 1.6 to 2.5
    // times). A free-running Lorenz LFO saved before gets the faster RATE
    // that moves it as before; a synced one keeps its division and now
    // loops on it.
    void lorenzRate (PatchValues& values, int)
    {
        for (int lfo = 1; lfo <= Processor::numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo);

            if (juce::roundToInt (values.get (prefix + "_shape", 0.0f)) != LfoSimShapes::Lorenz || values.get (prefix + "_sync", 0.0f) > 0.5f)
                continue;

            const auto rho = LfoSimInfo::value (LfoSimShapes::Lorenz, 1, values.get (prefix + "_p2", 0.5f));
            const auto scaled = (double) values.get (prefix + "_rate", 4.0f) * LfoSimInfo::lorenzLegacyRateScale (rho);
            values.set (prefix + "_rate", (float) juce::jlimit (0.01, 40.0, scaled));
        }
    }

    // Moves every saved choice at or above `from` of the given table
    // parameters up by `added` (every child with that id, as saved).
    template <typename IsTable>
    void moveTables (PatchValues& values, int from, int added, IsTable&& isTable)
    {
        auto* state = values.tree();
        if (state == nullptr)
        {
            jassertfalse;
            return;
        }

        for (int i = 0; i < state->getNumChildren(); ++i)
        {
            auto child = state->getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (isTable (id))
            {
                const auto table = juce::roundToInt ((float) child.getProperty ("value"));

                if (table >= from)
                    child.setProperty ("value", table + added, nullptr);
            }
        }
    }

    bool isAnyOscTable (const juce::String& id)
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            if (id == juce::String (prefix) + "_table")
                return true;
        return false;
    }

    // v1.1 added factory wavetables ahead of the four user slots, so older
    // saved choices of a user slot move up.
    void tablesV11 (PatchValues& values, int)
    {
        moveTables (values, 16, 40 - 16, // the v1.1 tables (the v1.3 ones are moved below)
                    [] (const juce::String& id) { return id == "osc1_table" || id == "osc2_table" || id == "sub_table"; });
    }

    // v1.3 (M10) added 80 factory tables ahead of the User tables, for all
    // six oscillators.
    void tablesV13 (PatchValues& values, int)
    {
        moveTables (values, 40, 120 - 40, isAnyOscTable); // the factory count then (step 19's tables are moved below)
    }

    // Step 19 appended 14 factory tables (Arca Tear ... Noise Choir) ahead of
    // the User tables, for all six oscillators.
    void tablesStep19 (PatchValues& values, int)
    {
        moveTables (values, 120, 134 - 120, isAnyOscTable); // the factory count then (a fixed number: later tables get their own schema step)
    }

    // v1.1 added explicit (FM) destinations ahead of the parameter
    // destinations, so older saved routings to those move up.
    void destinationsV11 (PatchValues& values, int)
    {
        auto* state = values.tree();
        if (state == nullptr)
        {
            jassertfalse;
            return;
        }

        const auto added = Mod::numExplicitDestinations - Mod::explicitDestinationsV10;

        for (int i = 0; i < state->getNumChildren(); ++i)
        {
            auto child = state->getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (id.startsWith ("mod") && id.endsWith ("_dst"))
            {
                const auto destination = juce::roundToInt ((float) child.getProperty ("value"));

                if (destination >= Mod::explicitDestinationsV10)
                    child.setProperty ("value", destination + added, nullptr);
            }
        }
    }

    void stringKnobs (PatchValues& values, int) { Processor::migrateStringKnobs (values.getter(), values.setter()); }
    void redundantKnobs (PatchValues& values, int) { Processor::migrateRedundantKnobs (values.getter(), values.setter()); }

    // Schema 2 was stamped by the 2026-11-05 build, which divided MIX but not
    // the routes and macros into it: those states get only the routes' part.
    void denseBoardMix (PatchValues& values, int stamp)
    {
        Processor::migrateDenseBoardMix (values.getter(), values.setter(), stamp < 2);
    }

    void denseBoardBypass (PatchValues& values, int) { Processor::migrateDenseBoardBypass (values.getter(), values.setter()); }
    void classicBoardBypass (PatchValues& values, int) { Processor::migrateDenseBoardBypass (values.getter(), values.setter(), true); }
    void modDepthRanges (PatchValues& values, int) { Processor::migrateModDepthRanges (values.getter(), values.setter()); }
    void operatorModes (PatchValues& values, int) { Processor::migrateOperatorModes (values.getter(), values.setter()); }
    void glideTime (PatchValues& values, int) { Processor::migrateGlideTime (values.getter(), values.setter()); }
    void bodyLevel (PatchValues& values, int) { Processor::migrateBodyLevel (values.getter(), values.setter()); }

    // Step 45 for a recipe: one written before Grain Position existed plays
    // its cloud at Sample Start. Unlike the saved-state row, a recipe that
    // sets its own position keeps it.
    void recipeGrainPosition (PatchValues& values, int)
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
            const auto positionId = prefix + "_grain_pos";
            const auto startId = prefix + "_sample_start";

            if (! values.contains (positionId) && values.contains (startId))
                values.set (positionId, values.get (startId, 0.0f));
        }
    }
}

// In the order they run. The saved and factory columns differ where the two
// loads always did; those differences are deliberate (each recipe was heard
// with them) and kept as they are.
const std::vector<Migrations::Row>& Migrations::table()
{
    static const std::vector<Row> rows {
        // Before v1.1 OSC 3 doubled as the sub (migrateLegacyOsc3).
        { "osc3Schema", 1, 2, legacyOsc3,
          SavedStage::beforeDefaults, Missing::add, FactoryStage::legacyRecipes, Missing::add, Duplicates::first },
        // Recipes get their grain position in their own row below.
        { "grainPosSchema", 1, 2, savedGrainPosition,
          SavedStage::beforeDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        { "lorenzSchema", 1, 2, lorenzRate,
          SavedStage::beforeDefaults, Missing::add, FactoryStage::none, Missing::skip, Duplicates::first },
        // Recipes name their tables and destinations in today's numbers.
        { "tableSchema", 1, 2, tablesV11,
          SavedStage::beforeDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        { "tableSchema", 2, 3, tablesV13,
          SavedStage::beforeDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        { "tableSchema", 3, 4, tablesStep19,
          SavedStage::beforeDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        { "destSchema", 1, 2, destinationsV11,
          SavedStage::beforeDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        // A patch saved before the string knobs were rescaled keeps its sound.
        { "stringSchema", 1, 2, stringKnobs,
          SavedStage::afterDefaults, Missing::skip, FactoryStage::afterTrims, Missing::skip, Duplicates::all },
        // ENV AMOUNT on Filter 1 or 2, an envelope TENSION or a WARP ENV become
        // matrix routes and curves with the same sound (the knobs are gone;
        // Filter 1's ENV AMOUNT defaulted to 1.5 octaves in the recipes).
        { "redundantKnobsSchema", 1, 2, redundantKnobs,
          SavedStage::afterDefaults, Missing::skip, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // The Dense board's MIX reaches further (2026-11-05); from 1 or 2.
        { "boardMixSchema", 2, 3, denseBoardMix,
          SavedStage::afterDefaults, Missing::add, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // ...and its colour, on even at MIX 0 %, fades in on the knob's first quarter.
        { "boardMixSchema", 3, 4, denseBoardBypass,
          SavedStage::afterDefaults, Missing::add, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // The Classic board's MIX got the same bypass at 0 % a day later.
        { "boardMixSchema", 4, 5, classicBoardBypass,
          SavedStage::afterDefaults, Missing::add, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // A route saved before 100 % meant the knob's whole range (after the
        // rows above, whose cutoff routes are old-scale). Recipes: on the live
        // parameters, after the default macros (loadFactoryPreset).
        { "modRangeSchema", 1, 2, modDepthRanges,
          SavedStage::afterDefaults, Missing::skip, FactoryStage::none, Missing::skip, Duplicates::first },
        // An operator from before the FM / DX7 type loads as one (it renders the same).
        // Stamped since 2026-11-26: it ran on every load, so a Ratio-tuned or
        // Operator-EG oscillator switched to WAVETABLE came back as FM / DX7.
        { "operatorModeSchema", 1, 2, operatorModes,
          SavedStage::afterDefaults, Missing::skip, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // GLIDE took 16 times its knob's time (the recipes' and every baked
        // pack's too): x16, so they glide as they were heard.
        { "glideSchema", 1, 2, glideTime,
          SavedStage::afterDefaults, Missing::skip, FactoryStage::afterTrims, Missing::skip, Duplicates::all },
        // A material body from before its levels changed (2026-11-13) keeps the old body.
        { "bodyLevelSchema", 1, 2, bodyLevel,
          SavedStage::afterDefaults, Missing::add, FactoryStage::afterTrims, Missing::add, Duplicates::first },
        // Step 45 for recipes (see the saved-state row above).
        { "grainPosSchema", 1, 2, recipeGrainPosition,
          SavedStage::none, Missing::skip, FactoryStage::afterTrims, Missing::add, Duplicates::first },
    };
    return rows;
}

void Migrations::runSaved (juce::ValueTree& state, SavedStage stage)
{
    TreePatchValues values (state);

    for (const auto& row : table())
    {
        if (row.saved != stage)
            continue;

        const auto stamp = row.schemaKey != nullptr ? (int) state.getProperty (row.schemaKey, 1) : 0;
        if (row.schemaKey != nullptr && stamp >= row.toVersion)
            continue;

        values.setMissing (row.savedMissing);
        row.run (values, stamp);

        if (row.schemaKey != nullptr)
            state.setProperty (row.schemaKey, row.toVersion, nullptr);
    }
}

void Migrations::runFactory (FactoryPatchValues::Values& values, FactoryStage stage)
{
    FactoryPatchValues patch (values);
    const auto& rows = table();

    for (size_t r = 0; r < rows.size(); ++r)
    {
        const auto& row = rows[r];
        if (row.factory != stage)
            continue;

        // A recipe starts at version 1 of every key and reaches each row's
        // toVersion as the recipe rows above run.
        auto stamp = 1;
        if (row.schemaKey != nullptr)
            for (size_t above = 0; above < r; ++above)
                if (rows[above].factory != FactoryStage::none && rows[above].schemaKey != nullptr
                    && juce::String (rows[above].schemaKey) == row.schemaKey)
                    stamp = rows[above].toVersion;

        if (row.schemaKey != nullptr && stamp >= row.toVersion)
            continue;

        patch.setMissing (row.factoryMissing);
        patch.setDuplicates (row.factoryDuplicates);
        row.run (patch, stamp);
    }
}
