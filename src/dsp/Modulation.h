#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <vector>

namespace Mod
{
// Indices are stored in presets and sessions: only ever append.
enum class Source
{
    None = 0,
    Lfo1,
    Lfo2,
    ModEnv,
    FilterEnv,
    AmpEnv,
    Velocity,
    KeyTrack,
    Random,
    ModWheel,
    Aftertouch,
    Expression,
    Macro1,
    Macro2,
    Macro3,
    Macro4,
    ClockSh,
    Mseg,
    Env4,
    FilterEnv2,
    Lfo3,
    Lfo4,
    Env6, Env7, Env8, Env9, Env10, Env11, Env12, Env13,
    Env14, Env15, Env16,
    // The LFO pool (M4): LFO 5-16.
    Lfo5, Lfo6, Lfo7, Lfo8, Lfo9, Lfo10, Lfo11, Lfo12,
    Lfo13, Lfo14, Lfo15, Lfo16,
    // M7.5: the audio input's envelope follower (ilanaSynth FX).
    InputEnv,
    // M8.1: every LFO's second output (B), for LFO 1-16.
    Lfo1B, Lfo2B, Lfo3B, Lfo4B, Lfo5B, Lfo6B, Lfo7B, Lfo8B,
    Lfo9B, Lfo10B, Lfo11B, Lfo12B, Lfo13B, Lfo14B, Lfo15B, Lfo16B,
    // M8.5: the vector pad's position.
    VectorX, VectorY,
    Count
};

constexpr int numLfoSources = 16;

inline int lfoIndexFor (Source source)
{
    switch (source)
    {
        case Source::Lfo1: return 0;
        case Source::Lfo2: return 1;
        case Source::Lfo3: return 2;
        case Source::Lfo4: return 3;
        default:
            if (source >= Source::Lfo5 && source <= Source::Lfo16)
                return 4 + (int) source - (int) Source::Lfo5;
            return -1;
    }
}

// LFO index 0..15 whose output B this source is, or -1.
inline int lfoBIndexFor (Source source)
{
    return source >= Source::Lfo1B && source <= Source::Lfo16B ? (int) source - (int) Source::Lfo1B : -1;
}

inline Source lfoBSourceFor (int index)
{
    return (Source) ((int) Source::Lfo1B + juce::jlimit (0, numLfoSources - 1, index));
}

// The source for LFO index 0..15.
inline Source lfoSourceFor (int index)
{
    constexpr Source first[] { Source::Lfo1, Source::Lfo2, Source::Lfo3, Source::Lfo4 };
    return index < 4 ? first[juce::jlimit (0, 3, index)]
                     : (Source) ((int) Source::Lfo5 + juce::jlimit (0, numLfoSources - 5, index - 4));
}

// Sources that swing both ways (-1..1); the rest run 0..1.
inline bool isBipolarSource (Source source)
{
    switch (source)
    {
        case Source::Lfo1: case Source::Lfo2: case Source::Lfo3: case Source::Lfo4:
        case Source::KeyTrack: case Source::Random: case Source::ClockSh: case Source::Mseg:
            return true;
        default:
            return (source >= Source::Lfo5 && source <= Source::Lfo16)
                   || (source >= Source::Lfo1B && source <= Source::Lfo16B);
        // (VectorX / VectorY run 0..1.)
    }
}

// Sources that differ from voice to voice (as opposed to one value for the
// whole synth).
inline bool isPerVoiceSource (Source source)
{
    if (source >= Source::Env6 && source <= Source::Env16)
        return true;

    switch (source)
    {
        case Source::ModEnv: case Source::FilterEnv: case Source::AmpEnv: case Source::Velocity:
        case Source::KeyTrack: case Source::Random: case Source::Env4: case Source::FilterEnv2:
            return true;
        default:
            return false;
    }
}

// Explicit destinations, evaluated per voice (oscillators, filters,
// envelopes) or with bespoke scaling (the classic FX targets). Indices are
// stored in presets: only ever append.
enum class Destination
{
    None = 0,
    Osc1Pitch,
    Osc1Frame,
    Osc1Level,
    Osc2Pitch,
    Osc2Frame,
    Osc2Level,
    SubLevel,
    NoiseLevel,
    Filter1Cutoff,
    Filter1Reso,
    Filter2Cutoff,
    Filter2Reso,
    AmpLevel,
    Pan,
    Lfo1Rate,
    Lfo2Rate,
    FxDriveAmount,
    FxCrushMix,
    FxCombFreq,
    FxPhaserRate,
    FxChorusDepth,
    FxDelayMix,
    FxDelayFeedback,
    FxSmearMix,
    FxFreezeMix,
    FxReverbMix,
    FxReverbSize,
    SubPitch,
    SubFrame,
    Osc1SampleStart,
    Osc1SampleEnd,
    Osc2SampleStart,
    Osc2SampleEnd,
    SubSampleStart,
    SubSampleEnd,

    // v1.0 additions
    Osc1Detune,
    Osc2Detune,
    SubDetune,
    Osc1Pan,
    Osc2Pan,
    SubPan,
    Osc1Warp,
    Osc2Warp,
    SubWarp,
    Osc1Blend,
    Osc2Blend,
    SubBlend,
    Osc1Spread,
    Osc2Spread,
    SubSpread,
    Filter1Drive,
    Filter2Drive,
    Filter1Env,
    Filter2Env,
    Filter1Fm,
    Filter2Fm,
    FmAmount,
    FmFeedback,
    RingMod,
    Drift,
    Lfo3Rate,
    Lfo4Rate,
    MsegRate,
    ResAmount,
    ResDecay,
    ResOffset,
    AmpAttack,
    AmpDecay,
    AmpSustain,
    AmpRelease,
    FeAttack,
    FeDecay,
    FeSustain,
    FeRelease,
    MeAttack,
    MeDecay,
    MeSustain,
    MeRelease,
    F2eAttack,
    F2eDecay,
    F2eSustain,
    F2eRelease,
    E4Attack,
    E4Decay,
    E4Sustain,
    E4Release,
    Filter1Morph,
    Filter2Morph,

    // v1.1: the FM matrix (OSC 2 > 1 and OSC 1 feedback are FmAmount/FmFeedback)
    Fm1to2,
    Fm1to3,
    Fm2to3,
    Fm3to1,
    Fm3to2,
    Fm2Feedback,
    Fm3Feedback,
    // The 115 parameter destinations occupy indices 96..210 in saved states.
    // New explicit oscillator targets start after them to preserve every old
    // destination choice index.
    Osc4Pitch = 211, Osc4Frame, Osc4Level, Osc4SampleStart, Osc4SampleEnd,
    Osc4Detune, Osc4Pan, Osc4Warp, Osc4Blend, Osc4Spread,
    Osc5Pitch, Osc5Frame, Osc5Level, Osc5SampleStart, Osc5SampleEnd,
    Osc5Detune, Osc5Pan, Osc5Warp, Osc5Blend, Osc5Spread,
    Osc6Pitch, Osc6Frame, Osc6Level, Osc6SampleStart, Osc6SampleEnd,
    Osc6Detune, Osc6Pan, Osc6Warp, Osc6Blend, Osc6Spread,
    // The LFO pool (M4).
    Lfo5Rate, Lfo6Rate, Lfo7Rate, Lfo8Rate, Lfo9Rate, Lfo10Rate, Lfo11Rate, Lfo12Rate,
    Lfo13Rate, Lfo14Rate, Lfo15Rate, Lfo16Rate,
    Count
};

// The rate destination for LFO index 0..15.
inline Destination lfoRateDestinationFor (int index)
{
    constexpr Destination first[] { Destination::Lfo1Rate, Destination::Lfo2Rate,
                                    Destination::Lfo3Rate, Destination::Lfo4Rate };
    return index < 4 ? first[juce::jlimit (0, 3, index)]
                     : (Destination) ((int) Destination::Lfo5Rate + juce::jlimit (0, 11, index - 4));
}

// Explicit destinations before the FM matrix was added: saved patches that
// point at a parameter destination (numbered after these) are shifted on load.
constexpr int explicitDestinationsV10 = 89;

constexpr int numExplicitDestinations = 96;
static_assert ((int) Destination::Fm3Feedback + 1 == numExplicitDestinations);
constexpr int firstNewExplicitDestination = (int) Destination::Osc4Pitch;

inline bool isExplicitDestination (int destination)
{
    return destination < numExplicitDestinations
           || (destination >= firstNewExplicitDestination && destination < (int) Destination::Count);
}

// Every other continuous parameter (effects and a few globals) can also be
// modulated. These are applied once per block to the parameter itself, so
// per-voice sources follow the loudest voice. Stored after the explicit
// destinations in the destination index; only ever append.
struct ParamDestination
{
    const char* id;
    const char* name;
};

inline const std::vector<ParamDestination>& getParamDestinations()
{
    static std::vector<ParamDestination> list {
        { "master", "Master Volume" },
        { "master_clip_gain", "Clip Gain" },
        { "arp_gate", "Arp Gate" },
        { "fx_slot1_mix", "FX Slot 1 Blend" }, { "fx_slot2_mix", "FX Slot 2 Blend" },
        { "fx_slot3_mix", "FX Slot 3 Blend" }, { "fx_slot4_mix", "FX Slot 4 Blend" },
        { "fx_slot5_mix", "FX Slot 5 Blend" }, { "fx_slot6_mix", "FX Slot 6 Blend" },
        { "fx_slot7_mix", "FX Slot 7 Blend" }, { "fx_slot8_mix", "FX Slot 8 Blend" },
        { "fx_slot9_mix", "FX Slot 9 Blend" }, { "fx_slot10_mix", "FX Slot 10 Blend" },
        { "fx_amp_drive", "Amp Drive" }, { "fx_amp_bass", "Amp Bass" }, { "fx_amp_mid", "Amp Mid" },
        { "fx_amp_treble", "Amp Treble" }, { "fx_amp_level", "Amp Level" },
        { "fx_drive_mix", "Drive Mix" }, { "fx_fold", "Drive Fold" },
        { "fx_crush_bits", "Crush Bits" }, { "fx_crush_down", "Crush Downsample" },
        { "fx_comp_threshold", "Comp Threshold" }, { "fx_comp_ratio", "Comp Ratio" },
        { "fx_comp_attack", "Comp Attack" }, { "fx_comp_release", "Comp Release" },
        { "fx_comp_makeup", "Comp Makeup" }, { "fx_comp_mix", "Comp Mix" },
        { "fx_comb_feedback", "Comb Feedback" }, { "fx_comb_mix", "Comb Mix" },
        { "fx_phaser_depth", "Phaser Depth" }, { "fx_phaser_feedback", "Phaser Feedback" },
        { "fx_phaser_mix", "Phaser Mix" },
        { "fx_chorus_rate", "Chorus Rate" }, { "fx_chorus_mix", "Chorus Mix" },
        { "fx_haas_delay", "Haas Delay" }, { "fx_haas_mix", "Haas Width" },
        { "fx_delay_time", "Delay Time" }, { "fx_delay_time_r", "Delay Time R" },
        { "fx_delay_damping", "Delay Damping" }, { "fx_delay_pitch", "Tape Pitch" },
        { "fx_delay_wow", "Tape Wow" }, { "fx_delay_duck", "Delay Duck" }, { "fx_taps_mix", "Taps Mix" },
        { "fx_stutter_mix", "Stutter Mix" }, { "fx_stutter_pitch", "Stutter Pitch" },
        { "fx_smear_size", "Smear Size" }, { "fx_smear_density", "Smear Density" },
        { "fx_reverb_damping", "Reverb Damping" }, { "fx_reverb_width", "Reverb Width" },
        { "fx_flanger_rate", "Flanger Rate" }, { "fx_flanger_depth", "Flanger Depth" },
        { "fx_flanger_feedback", "Flanger Feedback" }, { "fx_flanger_mix", "Flanger Mix" },
        { "fx_dim_rate", "Dimension Rate" }, { "fx_dim_depth", "Dimension Depth" }, { "fx_dim_mix", "Dimension Mix" },
        { "fx_gate_smooth", "Gate Smooth" }, { "fx_gate_mix", "Gate Mix" },
        { "fx_tape_stop_time", "Tape Stop Time" }, { "fx_tape_stop_mix", "Tape Stop Mix" },
        { "fx_tilt", "Tilt" }, { "fx_tilt_level", "Tilt Level" }, { "fx_util_gain", "Utility Gain" },
        { "fx_ott_amount", "OTT Amount" }, { "fx_ott_mix", "OTT Mix" },
        { "fx_limit_ceiling", "Limiter Ceiling" }, { "fx_limit_release", "Limiter Release" },
        { "fx_width", "Width" }, { "fx_width_mix", "Width Mix" },
        { "fx_trem_rate", "Tremolo Rate" }, { "fx_trem_depth", "Tremolo Depth" },
        { "fx_shifter_shift", "Freq Shift" }, { "fx_shifter_mix", "Freq Shift Mix" },
        { "fx_ring_freq", "Ring Freq" }, { "fx_ring_mix", "Ring Mix" }, { "fx_octaver_mix", "Octaver Mix" },
        { "fx_vowel_morph", "Vowel Morph" }, { "fx_vowel_mix", "Vowel Mix" },
        { "fx_feedback_amount", "Feedback Amount" }, { "fx_feedback_delay", "Feedback Delay" },
        { "fx_feedback_tone", "Feedback Tone" }, { "fx_feedback_mix", "Feedback Mix" },
        { "fx_eq_low_gain", "EQ Low Gain" }, { "fx_eq_mid_freq", "EQ Mid Freq" }, { "fx_eq_mid_gain", "EQ Mid Gain" },
        { "fx_eq_high_gain", "EQ High Gain" },
        // Appended in v1.0 (keep order: presets store the index).
        { "glide", "Glide" },
        { "osc1_string_decay", "Osc1 String Decay" }, { "osc1_string_damp", "Osc1 String Damp" },
        { "osc1_string_sustain", "Osc1 String Sustain" },
        { "osc2_string_decay", "Osc2 String Decay" }, { "osc2_string_damp", "Osc2 String Damp" },
        { "osc2_string_sustain", "Osc2 String Sustain" },
        { "sub_string_decay", "Osc3 String Decay" }, { "sub_string_damp", "Osc3 String Damp" },
        { "sub_string_sustain", "Osc3 String Sustain" },
        { "filter_balance", "Filter Balance" },
        { "subosc_level", "Sub Level" },
        // Appended in v1.1.
        { "osc1_grain_size", "Osc1 Grain Size" }, { "osc1_grain_density", "Osc1 Grain Density" },
        { "osc1_grain_spray", "Osc1 Grain Spray" }, { "osc1_grain_pitch", "Osc1 Grain Pitch" },
        { "osc2_grain_size", "Osc2 Grain Size" }, { "osc2_grain_density", "Osc2 Grain Density" },
        { "osc2_grain_spray", "Osc2 Grain Spray" }, { "osc2_grain_pitch", "Osc2 Grain Pitch" },
        { "sub_grain_size", "Osc3 Grain Size" }, { "sub_grain_density", "Osc3 Grain Density" },
        { "sub_grain_spray", "Osc3 Grain Spray" }, { "sub_grain_pitch", "Osc3 Grain Pitch" },
        { "arp_chance", "Arp Chance" }, { "spray_chance", "Spray Chance" }, { "spray_spread", "Spray Spread" },
        { "fx_gate_swing", "Gate Swing" },
    };

    // M4: a second segment of parameter destinations, numbered after every
    // explicit destination (Destination::Count onwards) so all older indices
    // keep their meaning. Generated names live in static storage.
    static const bool extended = []
    {
        static std::vector<juce::String> storage;
        storage.reserve (512);
        const auto add = [] (const juce::String& id, const juce::String& name)
        {
            storage.push_back (id);
            const auto* idText = storage.back().toRawUTF8();
            storage.push_back (name);
            list.push_back ({ idText, storage.back().toRawUTF8() });
        };

        const char* prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };
        for (int osc = 0; osc < 6; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            const auto name = "Osc" + juce::String (osc + 1) + " ";
            add (prefix + "_hammer_hard", name + "Hammer");
            add (prefix + "_couple", name + "Coupling");
            add (prefix + "_damper", name + "Damper");
            add (prefix + "_register", name + "Register");
            add (prefix + "_string_stiffness", name + "Stiffness");
            add (prefix + "_bridge_buzz", name + "Bridge Buzz");
            add (prefix + "_fret_rattle", name + "Fret Rattle");
            add (prefix + "_bow_pressure", name + "Bow Pressure");
            add (prefix + "_bow_speed", name + "Bow Speed");
            add (prefix + "_string_excite_pos", name + "Excite Position");

            if (osc >= 3) // OSC 4-6 came after the legacy segment was fixed
            {
                add (prefix + "_string_decay", name + "String Decay");
                add (prefix + "_string_damp", name + "String Damp");
                add (prefix + "_string_sustain", name + "String Sustain");
                add (prefix + "_grain_size", name + "Grain Size");
                add (prefix + "_grain_density", name + "Grain Density");
                add (prefix + "_grain_spray", name + "Grain Spray");
                add (prefix + "_grain_pitch", name + "Grain Pitch");
            }
        }

        add ("stretch", "Stretch Tuning");
        add ("sb_mix", "Soundboard Mix");
        add ("sb_tone", "Soundboard Tone");
        add ("sb_size", "Soundboard Size");
        add ("pedal_res", "Pedal Resonance");
        add ("mech_key", "Key Noise");
        add ("mech_damper", "Damper Noise");
        add ("mech_pedal", "Pedal Noise");

        // M5/M6 (keep order: presets store the index).
        for (int osc = 0; osc < 6; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            const auto name = "Osc" + juce::String (osc + 1) + " ";
            add (prefix + "_warp2_amt", name + "Warp 2");
            add (prefix + "_pd_env_amt", name + "Warp Env");
            add (prefix + "_key_level", name + "Key Level");
        }

        for (int osc = 1; osc <= 6; ++osc)
            add ("fm_noise" + juce::String (osc), "FM Noise > Osc" + juce::String (osc));
        add ("fm_noise_color", "FM Noise Colour");

        // The FM cells added with OSC 4-6 (the nine original ones are
        // explicit, per-voice destinations).
        for (int source = 1; source <= 6; ++source)
            for (int target = 1; target <= 6; ++target)
                if (source > 3 || target > 3)
                    add (source == target ? "fm_fb" + juce::String (source)
                                          : "fm_" + juce::String (source) + "to" + juce::String (target),
                         source == target ? "FM Osc" + juce::String (source) + " Feedback"
                                          : "FM Osc" + juce::String (source) + " > Osc" + juce::String (target));

        // M7.2 BODY controls. Append only: older destination indices are
        // stored in patches and host automation.
        add ("body_material", "Body Material");
        add ("body_size", "Body Size");
        add ("body_coupling", "Body Coupling Amount");

        // M7.3 electric pianos, then M7.5 audio input (append only).
        for (int osc = 0; osc < 6; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            const auto name = "Osc" + juce::String (osc + 1) + " ";
            add (prefix + "_ep_distance", name + "Pickup Distance");
            add (prefix + "_ep_position", name + "Pickup Offset");
        }
        add ("in_gain", "Input Gain");
        add ("in_dry", "Input Dry");
        add ("in_body", "Input to Body");
        add ("in_strings", "Input to Strings");
        add ("in_threshold", "Input Threshold");

        // M8.3 west-coast voice (append only).
        add ("west_fold", "West Fold");
        add ("west_sym", "West Symmetry");
        add ("west_decay", "West Decay");
        add ("west_open", "West Open");
        add ("west_strike", "West Strike");

        // M8.5 (append only).
        for (int osc = 0; osc < 6; ++osc)
            add (juce::String (prefixes[osc]) + "_fb_gain", "Osc" + juce::String (osc + 1) + " Feedback Gain");
        add ("vec_x", "Vector X");
        add ("vec_y", "Vector Y");
        return true;
    }();
    juce::ignoreUnused (extended);

    return list;
}

// The parameter destinations saved before M4 (indices 96..210).
constexpr int numLegacyParamDestinations = 115;

// Destination index of entry i of getParamDestinations().
inline int paramDestinationFor (int i)
{
    return i < numLegacyParamDestinations ? numExplicitDestinations + i
                                          : (int) Destination::Count + (i - numLegacyParamDestinations);
}

inline int getNumDestinations()
{
    return (int) Destination::Count + (int) getParamDestinations().size() - numLegacyParamDestinations;
}

// Entry of getParamDestinations() for a destination index, or -1.
inline int paramDestinationIndex (int destination)
{
    if (destination >= numExplicitDestinations && destination < numExplicitDestinations + numLegacyParamDestinations)
        return destination - numExplicitDestinations;

    const auto index = numLegacyParamDestinations + destination - (int) Destination::Count;
    return destination >= (int) Destination::Count && index < (int) getParamDestinations().size() ? index : -1;
}

inline int destinationForParamId (const juce::String& id)
{
    const auto& list = getParamDestinations();

    for (int i = 0; i < (int) list.size(); ++i)
        if (id == list[(size_t) i].id)
            return paramDestinationFor (i);

    return 0;
}

// The 27 FM cells added with OSC 4-6 are parameter destinations, but each
// voice evaluates them itself (per note, per sample) like the nine original
// cells, so envelopes and velocity work per note. Returns the cell as
// source * 6 + target (0-based), or -1 for any other destination.
inline int extendedFmCellFor (int destination)
{
    struct Layout
    {
        int first = 0;
        std::array<int, 27> cells {};
    };

    static const auto layout = []
    {
        Layout result;
        result.first = destinationForParamId ("fm_1to4");
        auto count = 0;

        // The order they are appended in getParamDestinations().
        for (int source = 0; source < 6; ++source)
            for (int target = 0; target < 6; ++target)
                if (source >= 3 || target >= 3)
                    result.cells[(size_t) count++] = source * 6 + target;

        return result;
    }();

    const auto index = destination - layout.first;
    return layout.first > 0 && juce::isPositiveAndBelow (index, 27) ? layout.cells[(size_t) index] : -1;
}

enum class Polarity
{
    Natural = 0,   // the source's own range (LFOs swing, envelopes rise)
    Unipolar,      // 0..1: only pushes the knob one way
    Bipolar        // -1..1: swings either side of the knob
};

struct Slot
{
    Source source = Source::None;
    int destination = 0;
    float depth = 0.0f;
    float curve = 0.0f;
    Polarity polarity = Polarity::Natural;
    Source aux = Source::None;
    bool bypass = false;

    bool isActive() const
    {
        return ! bypass && source != Source::None && destination != 0 && depth != 0.0f;
    }
};

// M6b: 64 slots. Slots 1-32 keep their parameter IDs; 33-64 are appended.
constexpr int maxSlots = 64;

// Shapes a raw source value by the slot's polarity and curve.
inline float shape (const Slot& slot, float value)
{
    const auto bipolarSource = isBipolarSource (slot.source);

    if (slot.polarity == Polarity::Unipolar && bipolarSource)
        value = 0.5f * (value + 1.0f);
    else if (slot.polarity == Polarity::Bipolar && ! bipolarSource)
        value = 2.0f * value - 1.0f;

    if (slot.curve != 0.0f)
    {
        const auto exponent = std::exp2 (slot.curve * 3.0f);
        value = value >= 0.0f ? std::pow (value, exponent) : -std::pow (-value, exponent);
    }

    return value;
}

// Aux ("via") sources scale a slot's amount, always as 0..1.
inline float auxScale (Source aux, float value)
{
    if (aux == Source::None)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, isBipolarSource (aux) ? 0.5f * (value + 1.0f) : value);
}

inline juce::StringArray getSourceNames()
{
    juce::StringArray names { "None", "LFO 1", "LFO 2", "Mod Env", "Filter Env", "Amp Env", "Velocity",
             "Key Track", "Random", "Mod Wheel", "Aftertouch", "Expression",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Clocked S&H", "MSEG",
             "Env 5", "Filter 2 Env", "LFO 3", "LFO 4" };
    for (int env = 6; env <= 16; ++env)
        names.add ("Env " + juce::String (env));
    for (int lfo = 5; lfo <= numLfoSources; ++lfo)
        names.add ("LFO " + juce::String (lfo));
    names.add ("Input Env");
    for (int lfo = 1; lfo <= numLfoSources; ++lfo)
        names.add ("LFO " + juce::String (lfo) + " B");
    names.add ("Vector X");
    names.add ("Vector Y");
    return names;
}

inline juce::StringArray getExplicitDestinationNames()
{
    return { "None", "Osc1 Pitch", "Osc1 Frame", "Osc1 Level", "Osc2 Pitch", "Osc2 Frame",
             "Osc2 Level", "Osc3 Level", "Noise Level", "Filter1 Cutoff", "Filter1 Reso",
             "Filter2 Cutoff", "Filter2 Reso", "Amp Level", "Pan", "LFO1 Rate", "LFO2 Rate",
             "Drive Amount", "Crush Mix", "Comb Freq", "Phaser Rate", "Chorus Depth",
             "Delay Mix", "Delay Feedback", "Smear Mix", "Freeze Mix", "Reverb Mix", "Reverb Size",
             "Osc3 Pitch", "Osc3 Frame", "Osc1 Sample Start", "Osc1 Sample End",
             "Osc2 Sample Start", "Osc2 Sample End", "Osc3 Sample Start", "Osc3 Sample End",
             "Osc1 Detune", "Osc2 Detune", "Osc3 Detune", "Osc1 Pan", "Osc2 Pan", "Osc3 Pan",
             "Osc1 Warp", "Osc2 Warp", "Osc3 Warp", "Osc1 Unison Blend", "Osc2 Unison Blend",
             "Osc3 Unison Blend", "Osc1 Spread", "Osc2 Spread", "Osc3 Spread",
             "Filter1 Drive", "Filter2 Drive", "Filter1 Env Amt", "Filter2 Env Amt",
             "Filter1 FM", "Filter2 FM", "FM Amount", "FM Feedback", "Ring Mod", "Drift",
             "LFO3 Rate", "LFO4 Rate", "MSEG Rate", "Res Amount", "Res Decay", "Res Offset",
             "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release",
             "Filt Env Attack", "Filt Env Decay", "Filt Env Sustain", "Filt Env Release",
             "Mod Env Attack", "Mod Env Decay", "Mod Env Sustain", "Mod Env Release",
             "F2 Env Attack", "F2 Env Decay", "F2 Env Sustain", "F2 Env Release",
             "Env4 Attack", "Env4 Decay", "Env4 Sustain", "Env4 Release",
             "Filter1 Morph", "Filter2 Morph",
             "FM Osc1 > Osc2", "FM Osc1 > Osc3", "FM Osc2 > Osc3", "FM Osc3 > Osc1", "FM Osc3 > Osc2",
             "FM Osc2 Feedback", "FM Osc3 Feedback" };
}

inline juce::StringArray getDestinationNames()
{
    auto names = getExplicitDestinationNames();
    const auto& params = getParamDestinations();

    for (int i = 0; i < numLegacyParamDestinations; ++i)
        names.add (params[(size_t) i].name);

    for (int osc = 4; osc <= 6; ++osc)
        for (const auto* target : { "Pitch", "Frame", "Level", "Sample Start", "Sample End",
                                    "Detune", "Pan", "Warp", "Unison Blend", "Spread" })
            names.add ("Osc" + juce::String (osc) + " " + target);

    for (int lfo = 5; lfo <= numLfoSources; ++lfo)
        names.add ("LFO" + juce::String (lfo) + " Rate");

    for (int i = numLegacyParamDestinations; i < (int) params.size(); ++i)
        names.add (params[(size_t) i].name);

    return names;
}
} // namespace Mod
