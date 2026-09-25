#pragma once
#include "dsp/OscillatorIds.h"

// The v1.0 factory library. Each preset is written as a short recipe with
// the Builder below, and every one maps all four macros.
//
// Included from Presets.h after FactoryPreset is defined.

#include "dsp/Modulation.h"

#include <set>
#include <string>

namespace Presets
{
namespace Library
{
// Parameter ids built at runtime need stable storage for Value::id.
inline const char* intern (const std::string& text)
{
    static std::set<std::string> pool;
    return pool.insert (text).first->c_str();
}

using D = Mod::Destination;

// Modulation sources, by their index in the source list.
enum Src
{
    Lfo1 = 1, Lfo2, ModEnv, FiltEnv, AmpEnv, Vel, Key, Rnd, Wheel, AT, Expr,
    M1, M2, M3, M4, ClockSH, Mseg, Env4, FiltEnv2, Lfo3, Lfo4
};

// Wavetables (the same table index for all three oscillators).
enum Table
{
    Basic = 0, HardSync, Wavefold, FmMetal, FormantT, CombT, Pwm, DriveSaw,
    SineT, TriangleT, Analog, Vowel, Glass, Fractal, Riser, Digital,
    // v1.1
    SoftSaw, SquareSweep, SawOctaves, Organ, HarmonicWalk, OddToEven, Bitwise, Stepped,
    Choir, Throat, Talkbox, CombSweep, SpectralTilt, PartialCloud, FibonacciBell,
    Chebyshev, HardClip, Rectify, Bitcrush, PluckPosition, Breath, Wood, Logistic, LorenzT
};

enum Spectral { SpOff = 0, SpStretch, SpShift, SpOddEven, SpFormant, SpSmear, SpCut };
enum FmMode { FmPhase = 0, FmThroughZero, FmExp };
enum Scale { NoScale = 0, Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, HarmonicMinor,
             MelodicMinor, MajorPenta, MinorPenta, Blues, WholeTone, Hirajoshi, PhrygianDominant };
enum Note { C = 0, Cs, Dn, Ds, En, Fn, Fs, Gn, Gs, An, As, Bn };
enum SprayDir { SprayUp = 0, SprayDown, SprayBoth };
enum ArpMode { ArpUp = 0, ArpDown, ArpUpDown, ArpRandom, ArpDownUp, ArpConverge, ArpWalk, ArpChord, ArpScaleRandom };

enum Filter { LP = 0, BP, HP, Notch, LadderLP, LadderHP, DiodeLP, Ms20LP, CombPlus, CombMinus, Formant, Morph };
enum WarpMode { WOff = 0, WSync, WBendPlus, WBendMinus, WPwm, WMirror, WAsym, WQuantize, WFm, WRing };
enum Fx
{
    FxNone = 0, FxAmp, FxDrive, FxCrush, FxComp, FxComb, FxPhaser, FxChorus, FxHaas, FxDelay, FxStutter,
    FxSmear, FxFreeze, FxReverb, FxFlanger, FxDimension, FxGate, FxTapeStop, FxTilt, FxUtility, FxOtt,
    FxLimiter, FxWidener, FxTremolo, FxFreqShift, FxRingMod, FxOctaver, FxVowel, FxFeedback, FxEq
};
enum Div { D1_1 = 0, D1_2, D1_4, D1_8, D1_16, D1_32, D1_4T, D1_8T, D1_16T, D1_8D, D1_16D };
enum Verb { Room = 0, Hall, Plate, Shimmer, Spring, Gated };
enum LfoShape { Sine = 0, Tri, SawUp, SawDown, Square, SampleHold, DrawShape, StepsShape, CurveShape,
                SmoothRandom, Drunk, Chaos };
enum Uni { Classic = 0, Hypersaw, Octaves, Fifths };

inline int param (const char* id) { return Mod::destinationForParamId (id); }

struct Target
{
    int destination;
    float amount;

    Target (D d, float a) : destination ((int) d), amount (a) {}
    Target (int d, float a) : destination (d), amount (a) {}
};

class Builder
{
public:
    Builder (const char* name, const char* category)
    {
        preset.name = name;
        preset.category = category;

        // A clean slate: the defaults carry a sub and some filter envelope.
        set ("sub_on", 0);
        set ("f1_env", 0.0f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_reso", 0.0f);
        set ("f1_drive", 1.0f);
    }

    Builder& set (const char* id, float value)
    {
        for (auto& entry : preset.values)
        {
            if (std::string (entry.id) == id)
            {
                entry.value = value;
                return *this;
            }
        }

        preset.values.push_back ({ intern (id), value });
        return *this;
    }

    Builder& set (const std::string& id, float value) { return set (intern (id), value); }

    // Oscillators ------------------------------------------------------------
    Builder& osc (int index, int table, float frame, float level, int semi = 0, float fine = 0.0f)
    {
        const auto p = prefix (index);
        set (p + "_on", 1);

        set (p + "_table", (float) table);
        set (p + "_level", level);

        set (p + "_frame", frame);
        set (p + "_semi", (float) semi);
        set (p + "_fine", fine);
        return *this;
    }

    Builder& osc1 (int table, float frame, float level = 0.8f, int semi = 0, float fine = 0.0f) { return osc (1, table, frame, level, semi, fine); }
    Builder& osc2 (int table, float frame, float level = 0.6f, int semi = 0, float fine = 0.0f) { return osc (2, table, frame, level, semi, fine); }
    Builder& osc3 (int table, float frame, float level = 0.5f, int semi = 0, float fine = 0.0f) { return osc (3, table, frame, level, semi, fine); }

    // The dedicated sub: 0 sine, 1 square, 2 saw; octave 0 = -1, 1 = -2.
    Builder& sub (int shape, float level, int octave = 0)
    {
        set ("subosc_on", 1);
        set ("sub_shape", (float) shape);
        set ("subosc_level", level);
        set ("sub_octave", (float) octave);
        return *this;
    }

    Builder& noise (float level) { return set ("noise_level", level); }

    Builder& unison (int index, int voices, float detune, float spread, int mode = Classic, float blend = 1.0f)
    {
        const auto p = prefix (index);
        set (p + "_unison", (float) voices);
        set (p + "_detune", detune);
        set (p + "_spread", spread);
        set (p + "_uni_mode", (float) mode);
        set (p + "_uni_blend", blend);
        return *this;
    }

    Builder& warp (int index, int mode, float amount)
    {
        const auto p = prefix (index);
        set (p + "_warp", (float) mode);
        set (p + "_warp_amt", amount);
        return *this;
    }

    Builder& pan (int index, float value) { return set (prefix (index) + "_pan", value); }
    Builder& route (int index, int bus) { return set (prefix (index) + "_route", (float) bus); }
    Builder& chord (int index, int type) { return set (prefix (index) + "_chord", (float) type); }

    // Karplus-Strong string on an oscillator. excite: 0 burst, 1 noise, 2 saw, 3 pulse.
    Builder& string (int index, float level, int excite, float decay, float damp, float sustain = 0.0f)
    {
        const auto p = prefix (index);
        set (p + "_on", 1);
        set (p + "_mode", 1);
        set (p + "_level", level);
        set (p + "_excite", (float) excite);
        set (p + "_string_decay", decay);
        set (p + "_string_damp", damp);
        set (p + "_string_sustain", sustain);
        return *this;
    }

    // Built-in sample: 1 metal hit, 2 vocal ah, 3 sub tone, 4 vinyl loop, 5 noise rise.
    Builder& sample (int index, int factorySample, float level, bool loop = false)
    {
        const auto p = prefix (index);
        set (p + "_on", 1);
        set (p + "_mode", 2);
        set (p + "_level", level);
        set (p + "_sample_factory", (float) factorySample);
        set (p + "_sample_tuned", 1);
        set (p + "_sample_loop", loop ? 1.0f : 0.0f);
        return *this;
    }

    Builder& fm (float amount, float feedback = 0.0f) { set ("fm_amount", amount); return set ("fm_feedback", feedback); }
    Builder& ring (float amount) { return set ("ring_mod", amount); }
    Builder& sync() { return set ("hard_sync", 1); }
    Builder& drift (float amount) { return set ("drift", amount); }

    // Filters ----------------------------------------------------------------
    Builder& filter (int index, int type, float cutoff, float reso, float env = 0.0f, float drive = 1.0f,
                     float keytrack = 0.0f, bool slope24 = false)
    {
        const auto p = std::string (index == 1 ? "f1" : "f2");
        set (p + "_type", (float) type);
        set (p + "_cutoff", cutoff);
        set (p + "_reso", reso);
        set (p + "_env", env);
        set (p + "_drive", drive);
        set (p + "_keytrack", keytrack);
        set (p + "_slope", slope24 ? 1.0f : 0.0f);
        return *this;
    }

    Builder& filter1 (int type, float cutoff, float reso, float env = 0.0f, float drive = 1.0f, float keytrack = 0.0f, bool slope24 = false)
    {
        return filter (1, type, cutoff, reso, env, drive, keytrack, slope24);
    }

    Builder& filter2 (int type, float cutoff, float reso, float env = 0.0f, float drive = 1.0f, float keytrack = 0.0f, bool slope24 = false)
    {
        return filter (2, type, cutoff, reso, env, drive, keytrack, slope24);
    }

    Builder& morph (int index, float value) { return set (index == 1 ? "f1_morph" : "f2_morph", value); }
    Builder& parallel() { return set ("filters_parallel", 1); }
    Builder& filterVelocity (float amount) { return set ("filter_velocity", amount); }
    Builder& filterFm (int index, float amount) { return set (index == 1 ? "f1_fm" : "f2_fm", amount); }

    // Envelopes --------------------------------------------------------------
    Builder& env (const char* p, float a, float d, float s, float r, float curve = 0.0f)
    {
        const auto base = std::string (p);
        set (base + "_attack", a);
        set (base + "_decay", d);
        set (base + "_sustain", s);
        set (base + "_release", r);

        if (curve != 0.0f)
            set (base + "_curve", curve);

        return *this;
    }

    Builder& amp (float a, float d, float s, float r, float curve = 0.0f) { return env ("amp", a, d, s, r, curve); }
    Builder& fenv (float a, float d, float s, float r, float curve = 0.0f) { return env ("fe", a, d, s, r, curve); }
    Builder& menv (float a, float d, float s, float r, float curve = 0.0f) { return env ("me", a, d, s, r, curve); }
    Builder& f2env (float a, float d, float s, float r, float curve = 0.0f) { return env ("f2e", a, d, s, r, curve); }
    Builder& env4 (float a, float d, float s, float r, float curve = 0.0f) { return env ("e4", a, d, s, r, curve); }
    Builder& velocity (float amount) { return set ("amp_velocity", amount); }

    // Voicing ----------------------------------------------------------------
    Builder& mono (float glideTime = 0.0f) { set ("voice_mode", 1); return set ("glide", glideTime); }
    Builder& legato (float glideTime = 0.06f) { set ("voice_mode", 2); return set ("glide", glideTime); }
    Builder& voices (int count) { return set ("poly_voices", (float) count); }
    Builder& glide (float time) { return set ("glide", time); }
    Builder& bend (float semis) { return set ("bend_range", semis); }
    Builder& spreadVoices (float amount) { return set ("voice_spread", amount); }

    Builder& arp (int mode, int division, int octaves, float gate)
    {
        set ("arp_on", 1);
        set ("arp_mode", (float) mode);
        set ("arp_div", (float) division);
        set ("arp_octaves", (float) octaves);
        return set ("arp_gate", gate);
    }

    // LFOs -------------------------------------------------------------------
    Builder& lfo (int index, int shape, float rate, bool perVoice = false, float phase = 0.0f)
    {
        const auto p = "lfo" + std::to_string (index);
        set (p + "_shape", (float) shape);
        set (p + "_rate", rate);
        set (p + "_sync", 0);
        set (p + "_retrig", perVoice ? 1.0f : 0.0f);
        return set (p + "_phase", phase);
    }

    Builder& lfoSync (int index, int shape, int division, bool perVoice = false, float phase = 0.0f)
    {
        const auto p = "lfo" + std::to_string (index);
        set (p + "_shape", (float) shape);
        set (p + "_sync", 1);
        set (p + "_div", (float) division);
        set (p + "_retrig", perVoice ? 1.0f : 0.0f);
        return set (p + "_phase", phase);
    }

    // Drawable curve for an LFO (x,y,tension;...). Also selects the Curve shape.
    Builder& curve (int index, const char* points)
    {
        if (preset.lfoCurves.size() < 4)
            preset.lfoCurves.resize (4, nullptr);

        preset.lfoCurves[(size_t) index - 1] = points;
        return set ("lfo" + std::to_string (index) + "_shape", (float) CurveShape);
    }

    // Modulation -------------------------------------------------------------
    Builder& mod (int source, Target target, float curveAmount = 0.0f, int via = 0)
    {
        if (nextSlot > Mod::maxSlots)
            return *this;

        const auto p = "mod" + std::to_string (nextSlot++);
        set (p + "_src", (float) source);
        set (p + "_dst", (float) target.destination);
        set (p + "_amt", target.amount);

        if (curveAmount != 0.0f)
            set (p + "_curve", curveAmount);

        if (via != 0)
            set (p + "_aux", (float) via);

        return *this;
    }

    Builder& mod (int source, D destination, float amount, float curveAmount = 0.0f, int via = 0)
    {
        return mod (source, Target (destination, amount), curveAmount, via);
    }

    Builder& macro (int index, const char* name, std::initializer_list<Target> targets, float curveAmount = 0.0f)
    {
        if (preset.macroNames.size() < 4)
            preset.macroNames.resize (4, nullptr);

        preset.macroNames[(size_t) index - 1] = name;

        for (const auto& target : targets)
            mod (M1 + index - 1, target, curveAmount);

        return *this;
    }

    // Effects ----------------------------------------------------------------
    Builder& fx (std::initializer_list<int> types)
    {
        auto slot = 1;

        for (auto type : types)
            set ("fx_slot" + std::to_string (slot++), (float) type);

        return *this;
    }

    Builder& reverb (int type, float size, float mix, float damping = 0.5f)
    {
        set ("fx_reverb_on", 1);
        set ("fx_reverb_type", (float) type);
        set ("fx_reverb_size", size);
        set ("fx_reverb_damping", damping);
        return set ("fx_reverb_mix", mix);
    }

    Builder& delay (int division, float feedback, float mix, bool pingPong = false, float damping = 0.3f)
    {
        set ("fx_delay_on", 1);
        set ("fx_delay_sync", 1);
        set ("fx_delay_div", (float) division);
        set ("fx_delay_feedback", feedback);
        set ("fx_delay_pingpong", pingPong ? 1.0f : 0.0f);
        set ("fx_delay_damping", damping);
        return set ("fx_delay_mix", mix);
    }

    Builder& chorus (float rate, float depth, float mix)
    {
        set ("fx_chorus_on", 1);
        set ("fx_chorus_rate", rate);
        set ("fx_chorus_depth", depth);
        return set ("fx_chorus_mix", mix);
    }

    Builder& driveFx (float amount, float mix, float fold = 0.0f)
    {
        set ("fx_drive_on", 1);
        set ("fx_drive_amount", amount);
        set ("fx_drive_mix", mix);
        return set ("fx_fold", fold);
    }

    Builder& phaser (float rate, float depth, float feedback, float mix)
    {
        set ("fx_phaser_on", 1);
        set ("fx_phaser_rate", rate);
        set ("fx_phaser_depth", depth);
        set ("fx_phaser_feedback", feedback);
        return set ("fx_phaser_mix", mix);
    }

    Builder& eq (float lowGain, float midFreq, float midGain, float highGain, float lowFreq = 120.0f, float highFreq = 6000.0f, float q = 0.9f)
    {
        set ("fx_eq_low_freq", lowFreq);
        set ("fx_eq_low_gain", lowGain);
        set ("fx_eq_mid_freq", midFreq);
        set ("fx_eq_mid_gain", midGain);
        set ("fx_eq_mid_q", q);
        set ("fx_eq_high_freq", highFreq);
        return set ("fx_eq_high_gain", highGain);
    }

    Builder& ott (float amount, float mix) { set ("fx_ott_amount", amount); return set ("fx_ott_mix", mix); }
    Builder& limiter (float ceiling) { return set ("fx_limit_ceiling", ceiling); }
    Builder& crush (float bits, float down, float mix) { set ("fx_crush_on", 1); set ("fx_crush_bits", bits); set ("fx_crush_down", down); return set ("fx_crush_mix", mix); }
    Builder& comp (float threshold, float ratio, float makeup, float mix = 1.0f)
    {
        set ("fx_comp_threshold", threshold);
        set ("fx_comp_ratio", ratio);
        set ("fx_comp_makeup", makeup);
        return set ("fx_comp_mix", mix);
    }

    Builder& width (float amount, float mix) { set ("fx_width", amount); return set ("fx_width_mix", mix); }
    Builder& dimension (float rate, float depth, float mix) { set ("fx_dim_rate", rate); set ("fx_dim_depth", depth); return set ("fx_dim_mix", mix); }
    Builder& flanger (float rate, float depth, float feedback, float mix)
    {
        set ("fx_flanger_rate", rate);
        set ("fx_flanger_depth", depth);
        set ("fx_flanger_feedback", feedback);
        return set ("fx_flanger_mix", mix);
    }

    Builder& ampSim (int mode, float drive, float bass, float mid, float treble, float level)
    {
        set ("fx_amp_mode", (float) mode);
        set ("fx_amp_drive", drive);
        set ("fx_amp_bass", bass);
        set ("fx_amp_mid", mid);
        set ("fx_amp_treble", treble);
        return set ("fx_amp_level", level);
    }

    Builder& haas (float delayMs, float mix) { set ("fx_haas_delay", delayMs); return set ("fx_haas_mix", mix); }
    Builder& tremolo (float rate, float depth, int shape = 0) { set ("fx_trem_rate", rate); set ("fx_trem_depth", depth); return set ("fx_trem_shape", (float) shape); }
    Builder& gate (int division, int pattern, float smooth, float mix = 1.0f)
    {
        set ("fx_gate_div", (float) division);
        set ("fx_gate_pattern", (float) pattern);
        set ("fx_gate_smooth", smooth);
        return set ("fx_gate_mix", mix);
    }

    Builder& master (float db) { return set ("master", db); }

    // v1.1 --------------------------------------------------------------------
    // FM matrix route, oscillators 1..3 (same source and target = feedback).
    Builder& fmRoute (int from, int to, float amount)
    {
        static const char* ids[3][3] {
            { "fm_feedback", "fm_1to2", "fm_1to3" },
            { "fm_amount", "fm_fb2", "fm_2to3" },
            { "fm_3to1", "fm_3to2", "fm_fb3" }
        };

        return set (ids[from - 1][to - 1], amount);
    }

    Builder& fmMode (int mode) { return set ("fm_mode", (float) mode); }

    // Take an oscillator out of the mix: it still modulates.
    Builder& modOnly (int index) { return set (prefix (index) + "_out", 0.0f); }

    Builder& spectral (int index, int mode, float amount)
    {
        const auto p = prefix (index);
        set (p + "_spectral", (float) mode);
        return set (p + "_spectral_amt", amount);
    }

    // Granular cloud from a built-in sample (see sample() for the numbers).
    Builder& granular (int index, int factorySample, float level, float position, float sizeMs, float density,
                       float spray, float pitchSpray = 0.0f, float spread = 0.6f)
    {
        sample (index, factorySample, level, false);
        const auto p = prefix (index);
        set (p + "_mode", 3);
        set (p + "_sample_start", position);
        set (p + "_grain_size", sizeMs);
        set (p + "_grain_density", density);
        set (p + "_grain_spray", spray);
        set (p + "_grain_pitch", pitchSpray);
        return set (p + "_grain_spread", spread);
    }

    Builder& lfoKey (int index) { return set ("lfo" + std::to_string (index) + "_key", 1); }

    // Trance gate with its own steps: one character per step, '0'..'9' = level.
    Builder& gateSteps (int division, const char* steps, float smooth, float swing = 0.0f, float mix = 1.0f)
    {
        gate (division, 8, smooth, mix);
        const auto count = (int) std::char_traits<char>::length (steps);
        set ("fx_gate_steps", (float) count);
        set ("fx_gate_swing", swing);

        for (int step = 0; step < count && step < 16; ++step)
            set ("fx_gate_step" + std::to_string (step + 1), (float) (steps[step] - '0') / 9.0f);

        return *this;
    }

    Builder& scale (int scaleIndex, int root, bool snapPlayed = false)
    {
        set ("gen_scale", (float) scaleIndex);
        set ("gen_root", (float) root);
        return set ("gen_snap", snapPlayed ? 1.0f : 0.0f);
    }

    Builder& spray (int count, int range, int direction, float spreadMs, float chance = 1.0f, float velocity = 0.3f)
    {
        set ("spray_on", 1);
        set ("spray_count", (float) count);
        set ("spray_range", (float) range);
        set ("spray_direction", (float) direction);
        set ("spray_spread", spreadMs);
        set ("spray_chance", chance);
        return set ("spray_velocity", velocity);
    }

    Builder& arpChance (float chance) { return set ("arp_chance", chance); }

    operator FactoryPreset() const { return preset; }

private:
    static std::string prefix (int index)
    {
        return OscillatorIds::prefixes[(size_t) ((index >= 1 && index <= OscillatorIds::count) ? index - 1 : 2)];
    }

    FactoryPreset preset { "", {} };
    int nextSlot = 1;
};

// Common macro shapes -------------------------------------------------------

inline const char* sineCurve = "0,0,-0.45;0.25,1,0.45;0.5,0,-0.45;0.75,-1,0.45;1,0,0";
inline const char* pumpCurve = "0,-1,0.8;1,1,0";
inline const char* stairsCurve = "0,-1,0;0.249,-1,0;0.25,-0.33,0;0.499,-0.33,0;0.5,0.33,0;0.749,0.33,0;0.75,1,0;1,1,0";
inline const char* bounceCurve = "0,1,-0.6;0.4,-1,0.6;0.6,0.2,-0.6;0.8,-1,0.6;0.9,-0.4,-0.6;1,-1,0";
inline const char* wobbleCurve = "0,-1,0;0.25,1,0;0.375,-1,0;0.5,1,0;0.75,-1,0;1,-1,0";
inline const char* swellCurve = "0,-1,0.6;0.7,1,-0.5;1,-1,0";
inline const char* triplCurve = "0,1,-0.7;0.33,-1,0;0.333,1,-0.7;0.66,-1,0;0.667,1,-0.7;1,-1,0";

inline std::vector<FactoryPreset> build()
{
    using B = Builder;
    std::vector<FactoryPreset> list;
    const auto add = [&list] (const FactoryPreset& preset) { list.push_back (preset); };

    // ======================================================================
    // BASS
    // ======================================================================

    add (B ("Ladder Sub", "Bass")
             .osc1 (Analog, 0.2f, 0.8f).sub (0, 0.55f)
             .filter1 (LadderLP, 260.0f, 0.35f, 2.4f, 1.8f, 0.3f, true)
             .fenv (0.001f, 0.22f, 0.1f, 0.2f).amp (0.002f, 0.4f, 0.85f, 0.12f)
             .mono (0.03f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "RESO", { { D::Filter1Reso, 0.5f } })
             .macro (3, "PUNCH", { { D::Filter1Env, 0.4f }, { D::FeDecay, -0.3f } })
             .macro (4, "GRIT", { { D::Filter1Drive, 0.6f }, { D::FxDriveAmount, 0.4f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.3f).eq (2.0f, 400.0f, -2.0f, 0.0f, 80.0f));

    add (B ("Diode Acid", "Bass")
             .osc1 (Basic, 0.0f, 0.85f)
             .filter1 (DiodeLP, 420.0f, 0.72f, 3.2f, 2.5f, 0.2f)
             .fenv (0.001f, 0.18f, 0.0f, 0.15f).amp (0.001f, 0.3f, 0.8f, 0.08f)
             .mono (0.07f).set ("glide_legato", 1)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.5f } })
             .macro (2, "ACCENT", { { D::Filter1Env, 0.35f }, { D::Filter1Reso, 0.2f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.5f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f }, { D::FxDelayMix, 0.25f } })
             .fx ({ FxDrive, FxDelay }).driveFx (3.0f, 0.5f).delay (D1_8D, 0.3f, 0.12f, true));

    add (B ("MS-20 Growl", "Bass")
             .osc1 (DriveSaw, 0.5f, 0.8f).osc2 (Pwm, 0.3f, 0.5f, -12).sub (1, 0.3f)
             .filter1 (Ms20LP, 380.0f, 0.6f, 2.0f, 4.0f, 0.2f, true)
             .fenv (0.001f, 0.35f, 0.25f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.1f)
             .lfoSync (1, Sine, D1_8)
             .mono()
             .macro (1, "GROWL", { { Target (D::Filter1Cutoff, 0.45f) }, { D::Osc1Frame, 0.4f } })
             .macro (2, "WOBBLE", {})
             .macro (3, "SCREAM", { { D::Filter1Reso, 0.35f }, { D::Filter1Drive, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .mod (Lfo1, Target (D::Filter1Cutoff, 0.3f), 0.0f, M2)
             .fx ({ FxDrive, FxReverb }).driveFx (3.5f, 0.45f).reverb (Room, 0.3f, 0.0f));

    add (B ("Hyper Reese", "Bass")
             .osc1 (DriveSaw, 0.6f, 0.75f).unison (1, 7, 22.0f, 0.6f, Hypersaw, 0.8f).sub (0, 0.5f)
             .filter1 (LadderLP, 700.0f, 0.3f, 0.0f, 2.0f, 0.2f, true)
             .amp (0.003f, 0.4f, 0.9f, 0.15f)
             .lfo (1, Sine, 0.18f)
             .mono (0.02f)
             .mod (Lfo1, D::Filter1Cutoff, 0.12f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.5f } })
             .macro (2, "DETUNE", { { D::Osc1Detune, 0.35f } })
             .macro (3, "DRIVE", { { D::Filter1Drive, 0.5f }, { param ("fx_ott_amount"), 0.3f } })
             .macro (4, "WIDTH", { { D::Osc1Spread, 0.4f } })
             .fx ({ FxOtt, FxEq }).ott (0.35f, 0.5f).eq (1.5f, 300.0f, -3.0f, -2.0f));

    add (B ("Comb Bass", "Bass")
             .osc1 (Basic, 0.2f, 0.7f).sub (0, 0.5f).noise (0.12f)
             .filter1 (CombPlus, 261.63f, 0.78f, 0.0f, 1.0f, 1.0f)
             .filter2 (LP, 1800.0f, 0.2f, 2.0f, 1.0f, 0.3f)
             .f2env (0.001f, 0.25f, 0.2f, 0.2f).mod (FiltEnv2, D::Filter2Cutoff, 0.35f)
             .amp (0.001f, 0.5f, 0.6f, 0.15f)
             .mono()
             .macro (1, "TONE", { { D::Filter2Cutoff, 0.45f } })
             .macro (2, "RING", { { D::Filter1Reso, 0.2f } })
             .macro (3, "NOISE", { { D::NoiseLevel, 0.4f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.4f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.3f).eq (2.0f, 800.0f, -2.0f, 0.0f));

    add (B ("Wavefold Sub", "Bass")
             .osc1 (Wavefold, 0.25f, 0.8f).sub (0, 0.6f)
             .filter1 (LP, 2400.0f, 0.1f, 1.5f, 1.0f, 0.3f, true)
             .fenv (0.001f, 0.3f, 0.3f, 0.2f).amp (0.002f, 0.4f, 0.9f, 0.12f)
             .menv (0.001f, 0.3f, 0.0f, 0.2f).mod (ModEnv, D::Osc1Frame, 0.4f)
             .mono (0.02f)
             .macro (1, "FOLD", { { D::Osc1Frame, 0.6f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "PUNCH", { { D::MeDecay, 0.3f } })
             .macro (4, "SUB", { { D::SubLevel, 0.35f } })
             .fx ({ FxOtt, FxLimiter }).ott (0.3f, 0.4f).limiter (-0.8f));

    add (B ("FM Donk", "Bass")
             .osc1 (SineT, 0.0f, 0.85f).osc2 (SineT, 0.0f, 0.0f, 12)
             .fm (0.45f, 0.05f)
             .menv (0.001f, 0.12f, 0.0f, 0.1f).mod (ModEnv, D::FmAmount, 0.5f)
             .filter1 (LP, 4000.0f, 0.0f, 0.0f)
             .amp (0.001f, 0.35f, 0.0f, 0.12f)
             .mono()
             .macro (1, "FM", { { D::FmAmount, 0.35f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f }, { D::MeDecay, 0.3f } })
             .macro (3, "FEEDBACK", { { D::FmFeedback, 0.4f } })
             .macro (4, "TONE", { { D::Filter1Cutoff, -0.5f } })
             .fx ({ FxComp, FxEq }).comp (-20.0f, 4.0f, 6.0f).eq (2.0f, 500.0f, -3.0f, 1.0f));

    add (B ("Sync Stab Bass", "Bass")
             .osc1 (Basic, 0.0f, 0.8f).warp (1, WSync, 0.35f).sub (1, 0.4f)
             .filter1 (LadderLP, 1400.0f, 0.3f, 1.8f, 2.0f, 0.2f)
             .fenv (0.001f, 0.2f, 0.2f, 0.15f).amp (0.001f, 0.25f, 0.7f, 0.1f)
             .menv (0.001f, 0.25f, 0.0f, 0.1f).mod (ModEnv, D::Osc1Warp, 0.4f)
             .mono()
             .macro (1, "SYNC", { { D::Osc1Warp, 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "SWEEP", { { D::MeDecay, 0.4f } })
             .macro (4, "GRIT", { { D::Filter1Drive, 0.6f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.5f, 0.35f).eq (1.0f, 900.0f, 2.0f, 0.0f));

    add (B ("Neuro Formant", "Bass")
             .osc1 (DriveSaw, 0.5f, 0.8f).unison (1, 3, 10.0f, 0.3f).sub (0, 0.45f)
             .filter1 (Formant, 900.0f, 0.55f, 0.0f, 2.0f)
             .lfoSync (1, CurveShape, D1_4).curve (1, wobbleCurve)
             .mod (Lfo1, D::Filter1Morph, 0.5f)
             .amp (0.002f, 0.3f, 0.9f, 0.1f)
             .mono()
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.5f } })
             .macro (2, "TALK", {})
             .macro (3, "SHIFT", { { D::Filter1Cutoff, 0.3f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f }, { param ("fx_ott_amount"), 0.3f } })
             .mod (Lfo1, Target (D::Filter1Morph, 0.4f), 0.0f, M2)
             .fx ({ FxDrive, FxOtt, FxLimiter }).driveFx (3.0f, 0.4f).ott (0.4f, 0.5f).limiter (-0.8f));

    add (B ("Pluck Sub Bass", "Bass")
             .osc1 (Analog, 0.3f, 0.6f).sub (0, 0.8f)
             .filter1 (LadderLP, 500.0f, 0.2f, 2.5f, 1.5f, 0.3f, true)
             .fenv (0.001f, 0.12f, 0.0f, 0.1f).amp (0.001f, 0.6f, 0.4f, 0.12f)
             .mono()
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "PLUCK", { { D::Filter1Env, 0.4f } })
             .macro (3, "SUSTAIN", { { D::AmpSustain, 0.5f } })
             .macro (4, "WARMTH", { { param ("fx_eq_low_gain"), 0.2f } })
             .fx ({ FxEq, FxComp }).eq (1.0f, 300.0f, -1.5f, -2.0f).comp (-18.0f, 3.0f, 3.0f));

    add (B ("Octave Hyper Bass", "Bass")
             .osc1 (Basic, 0.0f, 0.7f).unison (1, 6, 12.0f, 0.5f, Octaves, 0.6f).sub (0, 0.5f)
             .filter1 (LP, 900.0f, 0.3f, 2.0f, 2.0f, 0.2f, true)
             .fenv (0.001f, 0.25f, 0.3f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.12f)
             .mono (0.02f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "OCTAVES", { { D::Osc1Blend, 0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "DRIVE", { { D::Filter1Drive, 0.6f } })
             .fx ({ FxOtt, FxLimiter }).ott (0.3f, 0.4f).limiter (-0.8f));

    add (B ("Rubber Mono", "Bass")
             .osc1 (Pwm, 0.4f, 0.8f).warp (1, WBendPlus, 0.3f).sub (0, 0.4f)
             .filter1 (LadderLP, 600.0f, 0.45f, 2.2f, 1.8f, 0.2f, true)
             .fenv (0.001f, 0.28f, 0.2f, 0.15f).amp (0.001f, 0.3f, 0.85f, 0.1f)
             .legato (0.08f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "BEND", { { D::Osc1Warp, 0.5f } })
             .macro (3, "GLIDE", { { param ("glide"), 0.25f } })
             .macro (4, "PWM", { { D::Osc1Frame, 0.5f } })
             .fx ({ FxDrive }).driveFx (2.0f, 0.3f));

    add (B ("Ring Bass", "Bass")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (Basic, 0.0f, 0.0f, 7).warp (1, WRing, 0.35f).sub (0, 0.5f)
             .filter1 (LP, 1600.0f, 0.2f, 1.5f, 1.5f, 0.3f)
             .fenv (0.001f, 0.25f, 0.2f, 0.2f).amp (0.001f, 0.35f, 0.8f, 0.12f)
             .mono()
             .macro (1, "RING", { { D::Osc1Warp, 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DETUNE", { { D::Osc2Pitch, 0.004f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.4f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.25f).eq (1.5f, 600.0f, -2.0f, 0.0f));

    add (B ("Deep House Bass", "Bass")
             .osc1 (Analog, 0.5f, 0.7f).osc2 (SineT, 0.0f, 0.5f, -12)
             .filter1 (LadderLP, 450.0f, 0.25f, 1.6f, 1.5f, 0.4f, true)
             .fenv (0.003f, 0.35f, 0.2f, 0.25f).amp (0.003f, 0.5f, 0.6f, 0.2f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "ENV", { { D::Filter1Env, 0.35f } })
             .macro (3, "BODY", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.25f } })
             .fx ({ FxComp, FxReverb }).comp (-16.0f, 3.0f, 3.0f).reverb (Room, 0.25f, 0.0f));

    add (B ("Warp Wobble", "Bass")
             .osc1 (DriveSaw, 0.3f, 0.8f).warp (1, WMirror, 0.2f).unison (1, 2, 8.0f, 0.3f).sub (0, 0.5f)
             .filter1 (Ms20LP, 800.0f, 0.4f, 0.0f, 3.0f, 0.3f, true)
             .lfoSync (1, CurveShape, D1_4).curve (1, triplCurve)
             .mod (Lfo1, D::Filter1Cutoff, 0.35f)
             .amp (0.002f, 0.3f, 0.9f, 0.1f)
             .mono()
             .macro (1, "FILTER", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DEPTH", { { D::Filter1Cutoff, 0.2f } })
             .macro (3, "WARP", { { D::Osc1Warp, 0.5f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxOtt, FxLimiter }).driveFx (3.0f, 0.5f).ott (0.3f, 0.4f).limiter (-0.8f));

    add (B ("Morph Filter Bass", "Bass")
             .osc1 (Analog, 0.6f, 0.8f).sub (1, 0.35f)
             .filter1 (Morph, 700.0f, 0.5f, 1.5f, 2.0f, 0.2f)
             .fenv (0.001f, 0.3f, 0.2f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.1f)
             .lfoSync (1, Tri, D1_2).mod (Lfo1, D::Filter1Morph, 0.35f)
             .mono()
             .macro (1, "MORPH", { { D::Filter1Morph, 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.35f } })
             .macro (4, "DRIVE", { { D::Filter1Drive, 0.5f } })
             .fx ({ FxDrive }).driveFx (2.0f, 0.3f));

    add (B ("Tape Sub", "Bass")
             .osc1 (SineT, 0.0f, 0.9f).osc2 (TriangleT, 0.0f, 0.25f, 12)
             .filter1 (LP, 900.0f, 0.0f, 0.0f)
             .amp (0.002f, 0.8f, 0.7f, 0.2f)
             .drift (0.25f)
             .mono (0.04f)
             .macro (1, "HARMONICS", { { D::Osc2Level, 0.4f } })
             .macro (2, "SATURATE", { { param ("fx_amp_drive"), 0.3f } })
             .macro (3, "DRIFT", { { D::Drift, 0.5f } })
             .macro (4, "DECAY", { { D::AmpSustain, -0.5f } })
             .fx ({ FxAmp, FxEq }).ampSim (0, 2.0f, 1.2f, 0.8f, 0.7f, 0.9f).eq (1.0f, 200.0f, 0.0f, -4.0f));

    add (B ("Distorted 808", "Bass")
             .sample (1, 3, 0.95f).set ("sub_on", 0)
             .filter1 (LP, 3000.0f, 0.0f, 0.0f)
             .amp (0.002f, 1.4f, 0.25f, 0.3f)
             .glide (0.08f).mono()
             .macro (1, "DIST", { { D::FxDriveAmount, 0.5f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "PUNCH", { { param ("fx_ott_amount"), 0.4f } })
             .fx ({ FxDrive, FxOtt, FxLimiter }).driveFx (4.0f, 0.5f).ott (0.3f, 0.4f).limiter (-0.8f));

    add (B ("Dark Ladder Pluck Bass", "Bass")
             .osc1 (Pwm, 0.5f, 0.75f).osc2 (Analog, 0.4f, 0.5f, 0, -8.0f).sub (0, 0.4f)
             .filter1 (LadderLP, 250.0f, 0.55f, 3.0f, 2.0f, 0.4f, true)
             .fenv (0.001f, 0.16f, 0.0f, 0.12f).amp (0.001f, 0.4f, 0.5f, 0.1f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.4f } })
             .macro (4, "DETUNE", { { D::Osc2Pitch, 0.004f } })
             .fx ({ FxEq }).eq (1.0f, 400.0f, -1.0f, 0.0f));

    add (B ("Per-Voice Wobble", "Bass")
             .osc1 (Digital, 0.4f, 0.8f).sub (0, 0.5f)
             .filter1 (LadderLP, 600.0f, 0.5f, 0.0f, 2.5f, 0.2f, true)
             .lfo (1, Sine, 3.0f, true)
             .mod (Lfo1, D::Filter1Cutoff, 0.35f).mod (Vel, D::Lfo1Rate, 0.3f)
             .amp (0.002f, 0.3f, 0.9f, 0.1f)
             .macro (1, "RATE", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "DEPTH", { { D::Filter1Cutoff, 0.25f } })
             .macro (3, "FRAME", { { D::Osc1Frame, 0.5f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.4f } })
             .fx ({ FxDrive, FxLimiter }).driveFx (3.0f, 0.4f).limiter (-0.8f));

    // ======================================================================
    // LEAD
    // ======================================================================

    add (B ("Supersaw Anthem", "Lead")
             .osc1 (Basic, 0.0f, 0.7f).unison (1, 9, 24.0f, 0.85f, Hypersaw, 0.75f)
             .osc2 (Basic, 0.0f, 0.35f, 12).unison (2, 5, 18.0f, 0.8f, Hypersaw, 0.6f)
             .filter1 (LP, 7000.0f, 0.15f, 0.8f, 1.0f, 0.3f)
             .fenv (0.005f, 0.6f, 0.6f, 0.4f).amp (0.005f, 0.4f, 0.9f, 0.45f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, -0.5f } })
             .macro (2, "DETUNE", { { D::Osc1Detune, 0.3f }, { D::Osc2Detune, 0.3f } })
             .macro (3, "OCTAVE", { { D::Osc2Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f }, { D::FxDelayMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.18f, true).reverb (Hall, 0.7f, 0.22f));

    add (B ("Ladder Lead", "Lead")
             .osc1 (Analog, 0.4f, 0.8f).osc2 (Analog, 0.4f, 0.6f, 0, 7.0f)
             .filter1 (LadderLP, 1800.0f, 0.45f, 1.8f, 2.0f, 0.5f, true)
             .fenv (0.005f, 0.5f, 0.4f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.25f)
             .legato (0.07f)
             .lfo (1, Sine, 5.5f)
             .mod (Lfo1, Target (D::Osc1Pitch, 0.015f), 0.0f, Wheel).mod (Lfo1, Target (D::Osc2Pitch, 0.015f), 0.0f, Wheel)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "RESO", { { D::Filter1Reso, 0.4f } })
             .macro (3, "GLIDE", { { param ("glide"), 0.2f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDrive, FxDelay, FxReverb }).driveFx (2.0f, 0.25f).delay (D1_4, 0.35f, 0.15f).reverb (Plate, 0.5f, 0.15f));

    add (B ("Self-Osc Whistle", "Lead")
             .osc1 (SineT, 0.0f, 0.15f).noise (0.05f)
             .filter1 (LP, 1046.5f, 0.995f, 0.0f, 1.0f, 1.0f)
             .amp (0.03f, 0.3f, 0.85f, 0.4f)
             .legato (0.1f)
             .lfo (1, Sine, 5.0f).mod (Lfo1, D::Filter1Cutoff, 0.01f)
             .macro (1, "PITCH", { { D::Filter1Cutoff, 0.2f } })
             .macro (2, "BREATH", { { D::NoiseLevel, 0.25f } })
             .macro (3, "VIBRATO", {})
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .mod (Lfo1, D::Filter1Cutoff, 0.02f, 0.0f, M3)
             .fx ({ FxLimiter, FxDelay, FxReverb }).limiter (-6.0f).delay (D1_4, 0.3f, 0.12f).reverb (Hall, 0.6f, 0.2f));

    add (B ("Sync Scream", "Lead")
             .osc1 (Basic, 0.0f, 0.8f).warp (1, WSync, 0.4f).unison (1, 3, 12.0f, 0.5f)
             .filter1 (LP, 6000.0f, 0.2f, 0.0f, 2.0f)
             .menv (0.001f, 0.8f, 0.3f, 0.4f).mod (ModEnv, D::Osc1Warp, 0.4f)
             .amp (0.002f, 0.3f, 0.9f, 0.25f)
             .legato (0.04f)
             .mod (Wheel, D::Osc1Warp, 0.4f)
             .macro (1, "SYNC", { { D::Osc1Warp, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (3, "DRIVE", { { param ("fx_amp_drive"), 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxAmp, FxDelay, FxReverb }).ampSim (0, 3.0f, 0.9f, 1.1f, 1.0f, 0.8f).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.5f, 0.15f));

    add (B ("FM Brass Lead", "Lead")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 0, 1.0f)
             .fm (0.3f, 0.15f)
             .menv (0.04f, 0.5f, 0.5f, 0.3f).mod (ModEnv, D::FmAmount, 0.3f)
             .filter1 (LP, 5000.0f, 0.1f, 1.0f, 1.0f, 0.4f)
             .fenv (0.03f, 0.4f, 0.6f, 0.3f).amp (0.03f, 0.3f, 0.85f, 0.3f)
             .mod (Vel, D::FmAmount, 0.25f)
             .macro (1, "BRIGHT", { { D::FmAmount, 0.3f } })
             .macro (2, "BITE", { { D::FmFeedback, 0.3f } })
             .macro (3, "SWELL", { { D::AmpAttack, 0.3f }, { D::MeAttack, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.2f, 0.25f).reverb (Hall, 0.6f, 0.15f));

    add (B ("Hollow Pulse Lead", "Lead")
             .osc1 (Pwm, 0.5f, 0.8f).warp (1, WPwm, 0.3f)
             .filter1 (LP, 3500.0f, 0.2f, 1.0f, 1.2f, 0.5f)
             .fenv (0.002f, 0.4f, 0.5f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.3f)
             .lfo (1, Tri, 0.4f).mod (Lfo1, D::Osc1Warp, 0.2f)
             .legato (0.05f)
             .macro (1, "WIDTH", { { D::Osc1Warp, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "PWM SPEED", { { D::Lfo1Rate, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxChorus, FxDelay }).chorus (0.7f, 0.25f, 0.3f).delay (D1_8, 0.35f, 0.15f, true));

    add (B ("Talkbox Lead", "Lead")
             .osc1 (DriveSaw, 0.3f, 0.8f).unison (1, 2, 8.0f, 0.4f)
             .filter1 (Formant, 1100.0f, 0.5f, 0.0f, 1.5f)
             .amp (0.01f, 0.3f, 0.9f, 0.3f)
             .lfo (1, Sine, 0.3f).mod (Lfo1, D::Filter1Morph, 0.25f)
             .legato (0.05f)
             .mod (Wheel, D::Filter1Morph, 0.5f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.5f } })
             .macro (2, "SHIFT", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "MOVE", { { D::Lfo1Rate, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.3f, 0.12f).reverb (Plate, 0.5f, 0.15f));

    add (B ("Chip Lead", "Lead")
             .osc1 (Pwm, 0.25f, 0.7f).warp (1, WQuantize, 0.5f)
             .filter1 (LP, 12000.0f, 0.0f)
             .amp (0.001f, 0.2f, 0.8f, 0.1f)
             .lfo (1, Square, 12.0f)
             .mod (Lfo1, Target (D::Osc1Pitch, 0.05f), 0.0f, Wheel)
             .mono (0.02f)
             .macro (1, "CRUNCH", { { D::Osc1Warp, 0.5f } })
             .macro (2, "DUTY", { { D::Osc1Frame, 0.5f } })
             .macro (3, "BITS", { { D::FxCrushMix, 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxCrush, FxDelay }).crush (8.0f, 2.0f, 0.3f).delay (D1_8, 0.3f, 0.12f, true));

    add (B ("Diode Screamer", "Lead")
             .osc1 (DriveSaw, 0.6f, 0.8f).osc2 (DriveSaw, 0.6f, 0.6f, 0, 10.0f)
             .filter1 (DiodeLP, 2400.0f, 0.65f, 1.2f, 4.0f, 0.5f)
             .fenv (0.002f, 0.5f, 0.4f, 0.3f).amp (0.002f, 0.3f, 0.9f, 0.25f)
             .legato (0.05f)
             .mod (AT, D::Filter1Cutoff, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.35f } })
             .macro (2, "SCREAM", { { D::Filter1Reso, 0.3f } })
             .macro (3, "DRIVE", { { D::Filter1Drive, 0.5f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.25f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb, FxLimiter }).delay (D1_8D, 0.35f, 0.12f, true).reverb (Plate, 0.4f, 0.12f).limiter (-1.0f));

    add (B ("Glass Lead", "Lead")
             .osc1 (Glass, 0.3f, 0.75f).osc2 (SineT, 0.0f, 0.3f, 12)
             .filter1 (LP, 8000.0f, 0.1f, 0.0f)
             .amp (0.01f, 0.4f, 0.8f, 0.5f)
             .lfo (1, Sine, 0.2f).mod (Lfo1, D::Osc1Frame, 0.25f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.5f } })
             .macro (2, "SPARKLE", { { D::Osc2Level, 0.4f } })
             .macro (3, "SHIMMER", { { D::FxReverbMix, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4T, 0.4f, 0.18f, true).reverb (Shimmer, 0.8f, 0.25f));

    add (B ("Soft Saw Lead", "Lead")
             .osc1 (Analog, 0.5f, 0.75f).unison (1, 3, 10.0f, 0.5f).sub (0, 0.2f)
             .filter1 (LP, 2600.0f, 0.15f, 0.8f, 1.0f, 0.5f, true)
             .fenv (0.02f, 0.5f, 0.6f, 0.4f).amp (0.02f, 0.4f, 0.85f, 0.4f)
             .legato (0.05f).drift (0.2f)
             .lfo (1, Sine, 5.0f).mod (Lfo1, Target (D::Osc1Pitch, 0.01f), 0.0f, Wheel)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (3, "ATTACK", { { D::AmpAttack, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.6f, 0.2f, 0.3f).reverb (Hall, 0.55f, 0.15f));

    add (B ("Ring Lead", "Lead")
             .osc1 (Basic, 0.2f, 0.7f).osc2 (SineT, 0.0f, 0.0f, 7).ring (0.35f)
             .filter1 (LadderLP, 3000.0f, 0.3f, 1.0f, 1.5f, 0.4f)
             .fenv (0.003f, 0.4f, 0.4f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.3f)
             .legato (0.04f)
             .macro (1, "RING", { { D::RingMod, 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "INTERVAL", { { D::Osc2Pitch, 0.1f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.12f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Fifth Stack Lead", "Lead")
             .osc1 (DriveSaw, 0.4f, 0.75f).unison (1, 4, 10.0f, 0.6f, Fifths, 0.5f)
             .filter1 (LP, 4000.0f, 0.2f, 1.0f, 1.5f, 0.4f)
             .fenv (0.003f, 0.5f, 0.5f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.3f)
             .legato (0.05f)
             .macro (1, "FIFTHS", { { D::Osc1Blend, 0.5f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDrive, FxReverb }).driveFx (2.0f, 0.25f).reverb (Hall, 0.6f, 0.15f));

    add (B ("Theremin", "Lead")
             .osc1 (SineT, 0.0f, 0.8f).warp (1, WAsym, 0.15f)
             .filter1 (LP, 6000.0f, 0.0f)
             .amp (0.08f, 0.3f, 1.0f, 0.5f)
             .legato (0.18f)
             .lfo (1, Sine, 6.0f, true).mod (Lfo1, D::Osc1Pitch, 0.012f)
             .macro (1, "VIBRATO", {})
             .macro (2, "TONE", { { D::Osc1Warp, 0.4f } })
             .macro (3, "GLIDE", { { param ("glide"), 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .mod (Lfo1, Target (D::Osc1Pitch, 0.02f), 0.0f, M1)
             .fx ({ FxReverb }).reverb (Hall, 0.7f, 0.2f));

    add (B ("Acid Squelch Lead", "Lead")
             .osc1 (Pwm, 0.2f, 0.8f)
             .filter1 (Ms20LP, 900.0f, 0.75f, 3.0f, 3.0f, 0.3f, true)
             .fenv (0.001f, 0.25f, 0.1f, 0.15f).amp (0.001f, 0.3f, 0.85f, 0.1f)
             .mono (0.06f).set ("glide_legato", 1)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "SQUELCH", { { D::Filter1Reso, 0.2f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDrive, FxDelay }).driveFx (3.0f, 0.4f).delay (D1_8D, 0.4f, 0.15f, true));

    add (B ("Stereo Bend Lead", "Lead")
             .osc1 (Digital, 0.3f, 0.7f).warp (1, WBendMinus, 0.3f).pan (1, -0.3f)
             .osc2 (Digital, 0.35f, 0.6f, 0, 6.0f).warp (2, WBendPlus, 0.3f).pan (2, 0.3f)
             .filter1 (LP, 5000.0f, 0.15f, 0.8f, 1.2f, 0.4f)
             .fenv (0.003f, 0.5f, 0.5f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.3f)
             .legato (0.04f)
             .macro (1, "BEND", { { D::Osc1Warp, 0.4f }, { D::Osc2Warp, 0.4f } })
             .macro (2, "FRAME", { { D::Osc1Frame, 0.4f }, { D::Osc2Frame, 0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.5f, 0.12f));

    add (B ("Vintage Mono Lead", "Lead")
             .osc1 (Analog, 0.5f, 0.75f).osc2 (Pwm, 0.4f, 0.55f, -12, 5.0f)
             .filter1 (LadderLP, 1400.0f, 0.4f, 2.2f, 2.2f, 0.6f, true)
             .fenv (0.003f, 0.6f, 0.35f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.2f)
             .mono (0.05f).drift (0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "EMPHASIS", { { D::Filter1Reso, 0.35f } })
             .macro (3, "CONTOUR", { { D::Filter1Env, 0.3f } })
             .macro (4, "DRIFT", { { D::Drift, 0.5f } })
             .fx ({ FxAmp, FxDelay }).ampSim (0, 2.0f, 1.0f, 1.1f, 0.9f, 0.9f).delay (D1_4, 0.3f, 0.1f));

    add (B ("Octave Screamer", "Lead")
             .osc1 (HardSync, 0.4f, 0.8f).unison (1, 4, 8.0f, 0.5f, Octaves, 0.5f)
             .filter1 (LP, 5000.0f, 0.25f, 0.0f, 2.5f)
             .amp (0.002f, 0.3f, 0.9f, 0.25f)
             .legato (0.03f)
             .lfo (1, Sine, 0.25f).mod (Lfo1, D::Osc1Frame, 0.2f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "OCTAVES", { { D::Osc1Blend, 0.4f } })
             .macro (3, "DRIVE", { { D::Filter1Drive, 0.5f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb, FxLimiter }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.4f, 0.12f).limiter (-1.0f));

    add (B ("Pan Flute", "Lead")
             .osc1 (SineT, 0.0f, 0.7f).osc2 (TriangleT, 0.0f, 0.25f, 12).noise (0.08f)
             .filter1 (BP, 1600.0f, 0.2f, 0.0f, 1.0f, 0.8f)
             .parallel().filter2 (LP, 5000.0f, 0.0f, 0.0f, 1.0f, 0.5f)
             .amp (0.06f, 0.3f, 0.9f, 0.3f)
             .lfo (1, Sine, 5.0f, true).mod (Lfo1, D::Osc1Pitch, 0.006f)
             .legato (0.04f)
             .macro (1, "BREATH", { { D::NoiseLevel, 0.25f } })
             .macro (2, "BRIGHT", { { D::Filter2Cutoff, 0.3f } })
             .macro (3, "OVERBLOW", { { D::Osc2Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.7f, 0.2f));

    // ======================================================================
    // PLUCK
    // ======================================================================

    add (B ("Ladder Pluck", "Pluck")
             .osc1 (Analog, 0.3f, 0.8f).osc2 (Analog, 0.3f, 0.5f, 12, 4.0f)
             .filter1 (LadderLP, 600.0f, 0.35f, 3.0f, 1.5f, 0.5f, true)
             .fenv (0.001f, 0.22f, 0.0f, 0.2f).amp (0.001f, 0.5f, 0.0f, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DECAY", { { D::FeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.25f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Room, 0.4f, 0.15f));

    add (B ("Hypersaw Pluck", "Pluck")
             .osc1 (Basic, 0.0f, 0.7f).unison (1, 7, 20.0f, 0.8f, Hypersaw, 0.7f)
             .filter1 (LP, 800.0f, 0.2f, 3.2f, 1.2f, 0.4f, true)
             .fenv (0.001f, 0.25f, 0.0f, 0.25f).amp (0.001f, 0.45f, 0.0f, 0.35f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DECAY", { { D::FeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f }, { D::FxDelayMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.18f, true).reverb (Hall, 0.6f, 0.2f));

    add (B ("Comb Harp", "Pluck")
             .osc1 (Basic, 0.0f, 0.1f).noise (0.0f)
             .menv (0.001f, 0.012f, 0.0f, 0.01f).mod (ModEnv, D::NoiseLevel, 1.0f)
             .filter1 (CombPlus, 261.63f, 0.95f, 0.0f, 1.0f, 1.0f)
             .filter2 (LP, 5000.0f, 0.0f, 0.0f, 1.0f, 0.5f)
             .amp (0.001f, 2.0f, 0.0f, 1.0f)
             .macro (1, "RING", { { D::Filter1Reso, 0.05f } })
             .macro (2, "TONE", { { D::Filter2Cutoff, 0.4f } })
             .macro (3, "PICK", { { D::MeDecay, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-3.0f).reverb (Hall, 0.6f, 0.2f));

    add (B ("Nylon String", "Pluck")
             .string (1, 0.85f, 0, 0.7f, 0.45f)
             .filter1 (LP, 7000.0f, 0.0f, 0.0f, 1.0f, 0.3f)
             .amp (0.001f, 2.0f, 0.0f, 0.5f)
             .velocity (0.6f).master (-2.0f)
             .macro (1, "DECAY", { { param ("osc1_string_decay"), 0.25f } })
             .macro (2, "DAMP", { { param ("osc1_string_damp"), 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxEq, FxReverb }).eq (0.0f, 2500.0f, 1.5f, -1.0f).reverb (Room, 0.4f, 0.15f));

    add (B ("Pizzicato", "Pluck")
             .string (1, 0.8f, 3, 0.35f, 0.6f)
             .osc2 (TriangleT, 0.0f, 0.2f)
             .filter1 (LP, 5000.0f, 0.0f)
             .amp (0.001f, 0.4f, 0.0f, 0.3f)
             .velocity (0.6f)
             .macro (1, "LENGTH", { { param ("osc1_string_decay"), 0.3f }, { D::AmpDecay, 0.3f } })
             .macro (2, "BODY", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.65f, 0.2f));

    add (B ("Diode Blip", "Pluck")
             .osc1 (Pwm, 0.3f, 0.8f)
             .filter1 (DiodeLP, 900.0f, 0.6f, 3.0f, 2.0f, 0.5f)
             .fenv (0.001f, 0.12f, 0.0f, 0.1f).amp (0.001f, 0.2f, 0.0f, 0.15f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "SNAP", { { D::FeDecay, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_16, 0.45f, 0.2f, true).reverb (Room, 0.3f, 0.12f));

    add (B ("FM Pluck", "Pluck")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 19)
             .fm (0.35f)
             .menv (0.001f, 0.2f, 0.0f, 0.2f).mod (ModEnv, D::FmAmount, 0.4f)
             .filter1 (LP, 9000.0f, 0.0f)
             .amp (0.001f, 0.6f, 0.0f, 0.4f)
             .mod (Vel, D::FmAmount, 0.2f)
             .macro (1, "FM", { { D::FmAmount, 0.3f } })
             .macro (2, "DECAY", { { D::MeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (3, "METAL", { { D::FmFeedback, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.25f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.5f, 0.18f));

    add (B ("Kalimba", "Pluck")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 24, 30.0f).fm (0.2f)
             .menv (0.001f, 0.06f, 0.0f, 0.06f).mod (ModEnv, D::FmAmount, 0.4f)
             .filter1 (LP, 8000.0f, 0.0f)
             .amp (0.001f, 1.0f, 0.0f, 0.6f, -0.4f)
             .macro (1, "TINE", { { D::FmAmount, 0.3f } })
             .macro (2, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.45f, 0.18f));

    add (B ("Warp Pluck", "Pluck")
             .osc1 (Digital, 0.5f, 0.8f).warp (1, WBendPlus, 0.5f)
             .menv (0.001f, 0.2f, 0.0f, 0.2f).mod (ModEnv, D::Osc1Warp, -0.45f)
             .filter1 (LP, 3500.0f, 0.2f, 1.8f, 1.0f, 0.4f)
             .fenv (0.001f, 0.25f, 0.0f, 0.2f).amp (0.001f, 0.45f, 0.0f, 0.3f)
             .macro (1, "WARP", { { D::Osc1Warp, 0.4f } })
             .macro (2, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.4f }, { D::FeDecay, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.4f, 0.18f, true).reverb (Plate, 0.5f, 0.15f));

    add (B ("Formant Pluck", "Pluck")
             .osc1 (DriveSaw, 0.3f, 0.8f)
             .filter1 (Formant, 1000.0f, 0.5f, 0.0f, 1.2f)
             .menv (0.001f, 0.3f, 0.0f, 0.3f).mod (ModEnv, D::Filter1Morph, 0.5f)
             .amp (0.001f, 0.4f, 0.0f, 0.3f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.4f } })
             .macro (2, "TALK", { { D::MeDecay, 0.4f } })
             .macro (3, "SHIFT", { { D::Filter1Cutoff, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Muted Guitar", "Pluck")
             .string (1, 0.85f, 1, 0.3f, 0.7f)
             .filter1 (LP, 3500.0f, 0.1f, 0.0f, 1.0f, 0.4f)
             .amp (0.001f, 0.35f, 0.0f, 0.15f).master (4.0f)
             .macro (1, "MUTE", { { param ("osc1_string_damp"), 0.3f } })
             .macro (2, "RING", { { param ("osc1_string_decay"), 0.3f }, { D::AmpDecay, 0.3f } })
             .macro (3, "AMP", { { param ("fx_amp_drive"), 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxAmp, FxDelay }).ampSim (2, 2.0f, 1.0f, 1.1f, 0.9f, 1.0f).delay (D1_8D, 0.3f, 0.12f));

    add (B ("Glass Mallet", "Pluck")
             .osc1 (Glass, 0.5f, 0.7f).osc2 (SineT, 0.0f, 0.35f, 12)
             .filter1 (LP, 6000.0f, 0.1f, 1.0f, 1.0f, 0.5f)
             .fenv (0.001f, 0.3f, 0.0f, 0.3f).amp (0.001f, 0.8f, 0.0f, 0.6f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "BELL", { { D::Osc2Level, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4T, 0.35f, 0.15f, true).reverb (Plate, 0.6f, 0.2f));

    add (B ("Resonant Drop", "Pluck")
             .osc1 (Basic, 0.2f, 0.75f).unison (1, 2, 6.0f, 0.4f)
             .filter1 (LP, 300.0f, 0.85f, 4.0f, 1.5f, 0.5f, true)
             .fenv (0.001f, 0.35f, 0.0f, 0.2f, 0.6f).amp (0.001f, 0.5f, 0.0f, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DROP", { { D::FeDecay, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.12f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Plucked Bell Stack", "Pluck")
             .osc1 (FmMetal, 0.3f, 0.6f).osc2 (SineT, 0.0f, 0.5f, 12)
             .filter1 (LP, 6000.0f, 0.1f, 1.0f, 1.0f, 0.5f)
             .fenv (0.001f, 0.4f, 0.0f, 0.4f).amp (0.001f, 1.2f, 0.0f, 0.8f)
             .macro (1, "METAL", { { D::Osc1Frame, 0.4f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.6f, 0.2f));

    // ======================================================================
    // PAD
    // ======================================================================

    add (B ("Warm Analog Pad", "Pad")
             .osc1 (Analog, 0.5f, 0.6f).unison (1, 5, 14.0f, 0.8f)
             .osc2 (Analog, 0.4f, 0.4f, -12).unison (2, 3, 10.0f, 0.7f)
             .filter1 (LadderLP, 1400.0f, 0.2f, 0.8f, 1.2f, 0.3f, true)
             .fenv (1.2f, 1.5f, 0.6f, 2.0f).amp (0.8f, 1.0f, 0.9f, 2.5f)
             .drift (0.3f)
             .lfo (1, Sine, 0.15f).mod (Lfo1, D::Filter1Cutoff, 0.1f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "MOVEMENT", { { D::Lfo1Rate, 0.3f } })
             .macro (3, "ATTACK", { { D::AmpAttack, -0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.3f, 0.35f).reverb (Hall, 0.75f, 0.3f));

    add (B ("Vowel Pad", "Pad")
             .osc1 (DriveSaw, 0.4f, 0.6f).unison (1, 5, 12.0f, 0.8f)
             .filter1 (Formant, 800.0f, 0.4f)
             .lfo (1, Tri, 0.08f).mod (Lfo1, D::Filter1Morph, 0.45f)
             .amp (1.0f, 1.0f, 0.9f, 2.5f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.5f } })
             .macro (2, "SPEED", { { D::Lfo1Rate, 0.4f } })
             .macro (3, "SHIFT", { { D::Filter1Cutoff, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Hypersaw Pad", "Pad")
             .osc1 (Basic, 0.0f, 0.6f).unison (1, 9, 20.0f, 0.9f, Hypersaw, 0.8f)
             .filter1 (LP, 3000.0f, 0.1f, 0.5f, 1.0f, 0.3f)
             .fenv (0.6f, 1.2f, 0.7f, 1.5f).amp (0.5f, 1.0f, 0.9f, 2.0f)
             .voices (12)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (3, "ATTACK", { { D::AmpAttack, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f));

    add (B ("Comb Choir", "Pad")
             .osc1 (Basic, 0.0f, 0.4f).unison (1, 3, 10.0f, 0.7f).noise (0.15f)
             .filter1 (CombMinus, 261.63f, 0.8f, 0.0f, 1.0f, 1.0f)
             .filter2 (LP, 3500.0f, 0.1f)
             .amp (0.9f, 1.0f, 0.9f, 2.5f)
             .lfo (1, Sine, 0.12f).mod (Lfo1, D::Filter2Cutoff, 0.15f)
             .macro (1, "RESONANCE", { { D::Filter1Reso, 0.15f } })
             .macro (2, "AIR", { { D::NoiseLevel, 0.3f } })
             .macro (3, "TONE", { { D::Filter2Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-2.0f).reverb (Hall, 0.85f, 0.35f));

    add (B ("Curve Motion Pad", "Pad")
             .osc1 (Glass, 0.3f, 0.6f).unison (1, 4, 12.0f, 0.8f)
             .osc2 (Fractal, 0.4f, 0.4f, 12)
             .filter1 (LP, 2000.0f, 0.25f, 0.0f, 1.0f, 0.3f, true)
             .lfoSync (1, CurveShape, D1_1).curve (1, bounceCurve)
             .mod (Lfo1, D::Filter1Cutoff, 0.25f).mod (Lfo1, D::Osc2Frame, 0.3f)
             .amp (0.6f, 1.0f, 0.9f, 2.0f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "MOTION", {})
             .macro (3, "SPARKLE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .mod (Lfo1, Target (D::Osc1Frame, 0.4f), 0.0f, M2)
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.4f, 0.15f, true).reverb (Hall, 0.8f, 0.3f));

    add (B ("Morphing Strings", "Pad")
             .osc1 (Analog, 0.6f, 0.6f).unison (1, 6, 14.0f, 0.8f)
             .filter1 (Morph, 1800.0f, 0.3f, 0.0f, 1.0f, 0.3f)
             .lfo (1, Sine, 0.1f).mod (Lfo1, D::Filter1Morph, 0.3f)
             .amp (0.5f, 1.0f, 0.9f, 1.8f)
             .lfo (2, Sine, 5.0f, true).mod (Lfo2, Target (D::Osc1Pitch, 0.008f), 0.0f, Wheel)
             .macro (1, "MORPH", { { D::Filter1Morph, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "ATTACK", { { D::AmpAttack, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.35f, 0.3f, 0.3f).reverb (Hall, 0.75f, 0.3f));

    add (B ("Dark Matter", "Pad")
             .osc1 (Fractal, 0.3f, 0.6f).unison (1, 4, 16.0f, 0.8f)
             .osc2 (FmMetal, 0.2f, 0.3f, -12)
             .filter1 (LadderLP, 900.0f, 0.4f, 0.0f, 1.8f, 0.2f, true)
             .lfo (1, Sine, 0.05f).mod (Lfo1, D::Osc1Frame, 0.3f)
             .lfo (2, SampleHold, 0.4f).mod (Lfo2, D::Filter1Cutoff, 0.08f)
             .amp (1.5f, 1.0f, 0.9f, 3.0f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "METAL", { { D::Osc2Level, 0.4f } })
             .macro (3, "UNREST", {})
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .mod (Lfo2, Target (D::Filter1Cutoff, 0.2f), 0.0f, M3)
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (B ("Per-Voice Drift Pad", "Pad")
             .osc1 (Analog, 0.4f, 0.55f).unison (1, 3, 8.0f, 0.8f)
             .osc2 (TriangleT, 0.0f, 0.4f, 12)
             .filter1 (LP, 2200.0f, 0.2f, 0.0f, 1.0f, 0.3f)
             .lfo (1, Sine, 0.3f, true, 0.0f).mod (Lfo1, D::Filter1Cutoff, 0.12f).mod (Lfo1, D::Pan, 0.3f)
             .lfo (2, Tri, 0.21f, true, 0.5f).mod (Lfo2, D::Osc1Frame, 0.2f)
             .amp (0.7f, 1.0f, 0.9f, 2.2f)
             .spreadVoices (0.5f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DRIFT", { { D::Lfo1Rate, 0.3f }, { D::Lfo2Rate, 0.3f } })
             .macro (3, "SHINE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Shimmer Glass Pad", "Pad")
             .osc1 (Glass, 0.2f, 0.55f).unison (1, 4, 10.0f, 0.8f)
             .osc2 (SineT, 0.0f, 0.3f, 19)
             .filter1 (LP, 5000.0f, 0.1f)
             .lfo (1, Sine, 0.07f).mod (Lfo1, D::Osc1Frame, 0.35f)
             .amp (1.2f, 1.0f, 0.9f, 3.0f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "OVERTONE", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "SHIMMER", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Shimmer, 0.85f, 0.35f));

    add (B ("Sweep Pad", "Pad")
             .osc1 (Basic, 0.0f, 0.55f).unison (1, 6, 16.0f, 0.8f)
             .filter1 (LadderLP, 500.0f, 0.5f, 0.0f, 1.2f, 0.2f, true)
             .lfo (1, Tri, 0.06f).mod (Lfo1, D::Filter1Cutoff, 0.35f)
             .amp (0.8f, 1.0f, 0.9f, 2.0f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "SWEEP", { { D::Lfo1Rate, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxPhaser, FxReverb }).phaser (0.2f, 0.5f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Octave Stack Pad", "Pad")
             .osc1 (TriangleT, 0.0f, 0.6f).unison (1, 6, 8.0f, 0.8f, Octaves, 0.6f)
             .filter1 (LP, 3000.0f, 0.1f)
             .amp (0.9f, 1.0f, 0.9f, 2.5f)
             .macro (1, "OCTAVES", { { D::Osc1Blend, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Pulse Width Pad", "Pad")
             .osc1 (Pwm, 0.5f, 0.6f).warp (1, WPwm, 0.2f).unison (1, 3, 8.0f, 0.8f)
             .osc2 (Pwm, 0.5f, 0.45f, 0, 7.0f).warp (2, WPwm, 0.2f)
             .filter1 (LP, 2400.0f, 0.15f, 0.4f, 1.0f, 0.3f)
             .fenv (0.8f, 1.2f, 0.7f, 1.5f).amp (0.6f, 1.0f, 0.9f, 2.0f)
             .lfo (1, Tri, 0.25f).mod (Lfo1, D::Osc1Warp, 0.25f)
             .lfo (2, Tri, 0.19f, false, 0.3f).mod (Lfo2, D::Osc2Warp, 0.25f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "PWM", { { D::Osc1Warp, 0.3f }, { D::Osc2Warp, 0.3f } })
             .macro (3, "SPEED", { { D::Lfo1Rate, 0.3f }, { D::Lfo2Rate, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.3f, 0.35f).reverb (Hall, 0.75f, 0.3f));

    add (B ("Breathing Pad", "Pad")
             .osc1 (Vowel, 0.3f, 0.6f).unison (1, 4, 10.0f, 0.8f).noise (0.06f)
             .filter1 (LP, 1500.0f, 0.2f, 0.0f, 1.0f, 0.3f)
             .lfoSync (1, CurveShape, D1_1).curve (1, swellCurve)
             .mod (Lfo1, D::Filter1Cutoff, 0.3f).mod (Lfo1, D::AmpLevel, 0.15f)
             .amp (0.8f, 1.0f, 0.9f, 2.5f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "BREATH", { { D::NoiseLevel, 0.2f } })
             .macro (3, "VOWEL", { { D::Osc1Frame, 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.35f));

    add (B ("Routed Duo Pad", "Pad")
             .osc1 (Analog, 0.4f, 0.55f).unison (1, 4, 12.0f, 0.8f).route (1, 1)
             .osc2 (Glass, 0.3f, 0.45f, 12).unison (2, 3, 8.0f, 0.8f).route (2, 2)
             .filter1 (LadderLP, 900.0f, 0.3f, 0.0f, 1.2f, 0.3f, true)
             .filter2 (HP, 900.0f, 0.2f)
             .lfo (1, Sine, 0.1f).mod (Lfo1, D::Filter1Cutoff, 0.2f).mod (Lfo1, D::Filter2Cutoff, -0.2f)
             .amp (0.7f, 1.0f, 0.9f, 2.2f)
             .macro (1, "LOW BODY", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "HIGH AIR", { { D::Filter2Cutoff, -0.4f } })
             .macro (3, "BALANCE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Tape Strings", "Pad")
             .osc1 (Analog, 0.7f, 0.6f).unison (1, 5, 14.0f, 0.8f)
             .filter1 (LP, 3000.0f, 0.1f)
             .amp (0.4f, 1.0f, 0.9f, 1.5f)
             .drift (0.4f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (2, "WOBBLE", { { param ("fx_delay_wow"), 0.5f } })
             .macro (3, "DRIFT", { { D::Drift, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDimension, FxDelay, FxReverb }).dimension (0.3f, 0.6f, 0.5f)
             .delay (D1_4, 0.3f, 0.12f).set ("fx_delay_wow", 0.3f).reverb (Hall, 0.7f, 0.25f));

    // ======================================================================
    // KEYS
    // ======================================================================

    add (B ("FM E-Piano", "Keys")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 12)
             .fm (0.3f)
             .menv (0.001f, 0.7f, 0.1f, 0.4f).mod (ModEnv, D::FmAmount, 0.35f)
             .filter1 (LP, 7000.0f, 0.0f)
             .amp (0.001f, 1.8f, 0.2f, 0.5f)
             .mod (Vel, D::FmAmount, 0.3f)
             .macro (1, "TINE", { { D::FmAmount, 0.3f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.3f }, { D::MeDecay, 0.3f } })
             .macro (3, "TREMOLO", { { param ("fx_trem_depth"), 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxTremolo, FxChorus, FxReverb }).tremolo (4.5f, 0.0f).chorus (0.5f, 0.2f, 0.25f).reverb (Room, 0.45f, 0.15f));

    add (B ("Ladder Organ", "Keys")
             .osc1 (SineT, 0.0f, 0.6f).chord (1, 1).osc2 (TriangleT, 0.0f, 0.4f, 19)
             .filter1 (LadderLP, 4000.0f, 0.1f)
             .amp (0.005f, 0.2f, 1.0f, 0.08f)
             .macro (1, "DRAWBAR", { { D::Osc2Level, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (3, "ROTARY", { { param ("fx_chorus_mix"), 0.5f } })
             .macro (4, "DRIVE", { { param ("fx_amp_drive"), 0.3f } })
             .fx ({ FxAmp, FxChorus, FxReverb }).ampSim (0, 1.5f, 1.0f, 1.0f, 0.9f, 0.9f).chorus (5.0f, 0.2f, 0.2f).reverb (Room, 0.4f, 0.12f));

    add (B ("Warm Clav", "Keys")
             .osc1 (Pwm, 0.2f, 0.8f)
             .filter1 (BP, 1800.0f, 0.4f, 1.0f, 1.5f, 0.5f)
             .fenv (0.001f, 0.2f, 0.2f, 0.1f).amp (0.001f, 0.5f, 0.2f, 0.08f)
             .velocity (0.6f).master (-2.0f)
             .macro (1, "PICKUP", { { D::Filter1Cutoff, 0.35f } })
             .macro (2, "FUNK", { { D::Filter1Env, 0.35f } })
             .macro (3, "WAH", { { D::Filter1Reso, 0.3f } })
             .macro (4, "PHASE", { { param ("fx_phaser_mix"), 0.4f } })
             .fx ({ FxPhaser, FxAmp }).phaser (0.4f, 0.4f, 0.3f, 0.0f).ampSim (2, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f));

    add (B ("Toy Piano", "Keys")
             .osc1 (FmMetal, 0.2f, 0.6f).osc2 (SineT, 0.0f, 0.5f)
             .filter1 (LP, 6000.0f, 0.0f)
             .amp (0.001f, 0.9f, 0.0f, 0.4f, -0.5f)
             .macro (1, "METAL", { { D::Osc1Level, 0.3f } })
             .macro (2, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.4f, 0.15f));

    add (B ("Mellow Rhodes", "Keys")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (TriangleT, 0.0f, 0.2f, 12).warp (1, WAsym, 0.1f)
             .filter1 (LP, 3000.0f, 0.0f, 1.0f, 1.0f, 0.3f)
             .fenv (0.001f, 0.5f, 0.2f, 0.4f).amp (0.001f, 2.2f, 0.25f, 0.5f)
             .mod (Vel, D::Osc1Warp, 0.3f)
             .macro (1, "BARK", { { D::Osc1Warp, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "TREMOLO", { { param ("fx_trem_depth"), 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxTremolo, FxChorus, FxReverb }).tremolo (5.0f, 0.0f).chorus (0.6f, 0.2f, 0.2f).reverb (Room, 0.45f, 0.15f));

    add (B ("Digital Bell Keys", "Keys")
             .osc1 (Digital, 0.3f, 0.6f).osc2 (SineT, 0.0f, 0.4f, 12, 3.0f)
             .filter1 (LP, 8000.0f, 0.0f)
             .amp (0.001f, 1.5f, 0.1f, 0.8f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "BELL", { { D::Osc2Level, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.2f, 0.2f).reverb (Plate, 0.5f, 0.2f));

    add (B ("Harpsichord", "Keys")
             .string (1, 0.7f, 3, 0.55f, 0.2f).osc2 (Basic, 0.0f, 0.25f, 12)
             .filter1 (HP, 200.0f, 0.0f)
             .amp (0.001f, 1.5f, 0.0f, 0.3f)
             .macro (1, "BRIGHT", { { param ("osc1_string_damp"), -0.3f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, 0.3f } })
             .macro (3, "LENGTH", { { param ("osc1_string_decay"), 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.5f, 0.15f));

    add (B ("Vibraphone", "Keys")
             .osc1 (SineT, 0.0f, 0.7f).osc2 (SineT, 0.0f, 0.3f, 24).chord (1, 0)
             .filter1 (LP, 6000.0f, 0.0f)
             .amp (0.001f, 2.5f, 0.0f, 1.2f, -0.3f)
             .macro (1, "MOTOR", { { param ("fx_trem_depth"), 0.5f } })
             .macro (2, "OVERTONE", { { D::Osc2Level, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxTremolo, FxReverb }).tremolo (5.5f, 0.3f).reverb (Room, 0.5f, 0.18f));

    // ======================================================================
    // CHORDS
    // ======================================================================

    add (B ("House Organ Chord", "Chords")
             .osc1 (Basic, 0.3f, 0.6f).chord (1, 5).osc2 (SineT, 0.0f, 0.4f, -12)
             .filter1 (LP, 2500.0f, 0.2f, 1.2f, 1.2f, 0.3f)
             .fenv (0.001f, 0.25f, 0.2f, 0.2f).amp (0.001f, 0.35f, 0.3f, 0.15f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (3, "BODY", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxComp, FxReverb }).comp (-18.0f, 3.0f, 3.0f).reverb (Room, 0.4f, 0.12f));

    add (B ("Stab Ladder Chord", "Chords")
             .osc1 (Analog, 0.4f, 0.55f).chord (1, 4).unison (1, 2, 8.0f, 0.6f)
             .filter1 (LadderLP, 700.0f, 0.4f, 3.0f, 1.5f, 0.3f, true)
             .fenv (0.001f, 0.2f, 0.0f, 0.15f).amp (0.001f, 0.3f, 0.0f, 0.2f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DECAY", { { D::FeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Room, 0.4f, 0.15f));

    add (B ("Dub Chord", "Chords")
             .osc1 (Pwm, 0.3f, 0.6f).chord (1, 5)
             .filter1 (LP, 1600.0f, 0.3f, 1.0f, 1.2f, 0.3f)
             .fenv (0.001f, 0.2f, 0.1f, 0.2f).amp (0.001f, 0.35f, 0.0f, 0.25f).master (-2.0f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "FEEDBACK", { { D::FxDelayFeedback, 0.25f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.55f, 0.35f, true, 0.5f).reverb (Spring, 0.5f, 0.2f));

    add (B ("Hypersaw Stab", "Chords")
             .osc1 (Basic, 0.0f, 0.55f).chord (1, 4).unison (1, 5, 18.0f, 0.8f, Hypersaw, 0.7f)
             .filter1 (LP, 2000.0f, 0.2f, 2.5f, 1.2f, 0.3f)
             .fenv (0.001f, 0.25f, 0.1f, 0.2f).amp (0.001f, 0.35f, 0.1f, 0.25f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.3f }, { D::FeDecay, 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.6f, 0.2f));

    add (B ("Lo-Fi Chord", "Chords")
             .osc1 (TriangleT, 0.0f, 0.6f).chord (1, 5).osc2 (SineT, 0.0f, 0.3f, 12)
             .filter1 (LP, 2200.0f, 0.1f)
             .amp (0.01f, 1.2f, 0.4f, 0.5f)
             .drift (0.4f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (2, "WOBBLE", { { param ("fx_delay_wow"), 0.5f } })
             .macro (3, "CRUSH", { { D::FxCrushMix, 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxCrush, FxDelay, FxReverb }).crush (10.0f, 2.0f, 0.2f).delay (D1_4, 0.3f, 0.15f).set ("fx_delay_wow", 0.4f).reverb (Room, 0.5f, 0.2f));

    add (B ("Sus Pad Chord", "Chords")
             .osc1 (Analog, 0.4f, 0.5f).chord (1, 6).unison (1, 3, 10.0f, 0.8f)
             .filter1 (LP, 2500.0f, 0.1f, 0.5f, 1.0f, 0.3f)
             .fenv (0.5f, 1.0f, 0.7f, 1.0f).amp (0.4f, 1.0f, 0.9f, 1.8f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "ATTACK", { { D::AmpAttack, 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.3f, 0.3f).reverb (Hall, 0.75f, 0.3f));

    add (B ("Power Chord", "Chords")
             .osc1 (DriveSaw, 0.5f, 0.6f).chord (1, 3).unison (1, 2, 8.0f, 0.7f)
             .filter1 (LP, 4000.0f, 0.1f)
             .amp (0.003f, 0.4f, 0.9f, 0.3f)
             .macro (1, "GAIN", { { param ("fx_amp_drive"), 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxReverb }).ampSim (1, 5.0f, 1.0f, 1.2f, 0.8f, 0.6f).reverb (Room, 0.4f, 0.12f));

    add (B ("Minor Glass Chord", "Chords")
             .osc1 (Glass, 0.4f, 0.5f).chord (1, 5).osc2 (SineT, 0.0f, 0.25f, 12)
             .filter1 (LP, 5000.0f, 0.1f, 1.0f, 1.0f, 0.3f)
             .fenv (0.001f, 0.4f, 0.2f, 0.4f).amp (0.001f, 1.2f, 0.2f, 0.8f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (3, "SHINE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.15f, true).reverb (Plate, 0.6f, 0.25f));

    // ======================================================================
    // ARP
    // ======================================================================

    add (B ("Ladder Arp", "Arp")
             .osc1 (Analog, 0.3f, 0.75f)
             .filter1 (LadderLP, 700.0f, 0.4f, 2.5f, 1.5f, 0.5f, true)
             .fenv (0.001f, 0.15f, 0.0f, 0.1f).amp (0.001f, 0.25f, 0.0f, 0.15f)
             .arp (0, D1_16, 2, 0.5f)
             .lfo (1, Sine, 0.1f).mod (Lfo1, D::Filter1Cutoff, 0.2f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.2f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Random Glass Arp", "Arp")
             .osc1 (Glass, 0.3f, 0.7f).osc2 (SineT, 0.0f, 0.3f, 12)
             .filter1 (LP, 6000.0f, 0.1f)
             .amp (0.001f, 0.4f, 0.0f, 0.3f)
             .arp (3, D1_16, 3, 0.4f)
             .lfo (1, SampleHold, 8.0f).mod (Lfo1, D::Osc1Frame, 0.3f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "SPARKLE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f }, { D::FxDelayMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.18f, true).reverb (Shimmer, 0.7f, 0.25f));

    add (B ("Acid Arp", "Arp")
             .osc1 (Basic, 0.0f, 0.8f)
             .filter1 (DiodeLP, 600.0f, 0.7f, 3.0f, 2.5f, 0.3f)
             .fenv (0.001f, 0.15f, 0.0f, 0.1f).amp (0.001f, 0.2f, 0.7f, 0.08f)
             .arp (2, D1_16, 2, 0.6f)
             .lfo (1, Tri, 0.08f).mod (Lfo1, D::Filter1Cutoff, 0.25f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "ACCENT", { { D::Filter1Env, 0.3f } })
             .macro (3, "DIRT", { { D::FxDriveAmount, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDrive, FxDelay }).driveFx (3.0f, 0.4f).delay (D1_8D, 0.4f, 0.15f, true));

    add (B ("FM Sequence", "Arp")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 7)
             .fm (0.3f)
             .lfoSync (1, StepsShape, D1_16).mod (Lfo1, D::FmAmount, 0.3f)
             .filter1 (LP, 8000.0f, 0.0f)
             .amp (0.001f, 0.25f, 0.0f, 0.2f)
             .arp (0, D1_16, 1, 0.5f)
             .macro (1, "FM", { { D::FmAmount, 0.3f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "FEEDBACK", { { D::FmFeedback, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8T, 0.4f, 0.2f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Pluck Walker", "Arp")
             .string (1, 0.8f, 0, 0.55f, 0.35f)
             .filter1 (LP, 6000.0f, 0.0f)
             .amp (0.001f, 1.0f, 0.0f, 0.4f)
             .arp (6, D1_8, 2, 0.6f)
             .macro (1, "DAMP", { { param ("osc1_string_damp"), 0.3f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.25f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4T, 0.35f, 0.18f, true).reverb (Hall, 0.5f, 0.15f));

    add (B ("Trance Gate Arp", "Arp")
             .osc1 (Basic, 0.0f, 0.65f).unison (1, 7, 18.0f, 0.8f, Hypersaw, 0.7f)
             .filter1 (LP, 3000.0f, 0.2f, 1.5f, 1.0f, 0.3f)
             .fenv (0.001f, 0.2f, 0.3f, 0.2f).amp (0.001f, 0.3f, 0.6f, 0.2f)
             .arp (2, D1_16, 2, 0.45f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f }, { D::FxDelayMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.2f, true).reverb (Hall, 0.7f, 0.2f));

    add (B ("Bouncing Ball Arp", "Arp")
             .osc1 (TriangleT, 0.0f, 0.7f).osc2 (Glass, 0.3f, 0.3f, 12)
             .filter1 (LP, 5000.0f, 0.1f)
             .amp (0.001f, 0.3f, 0.0f, 0.25f)
             .arp (5, D1_16T, 2, 0.4f)
             .lfoSync (1, CurveShape, D1_2).curve (1, bounceCurve).mod (Lfo1, D::Filter1Cutoff, 0.2f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "SHINE", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.35f, 0.15f, true).reverb (Plate, 0.5f, 0.2f));

    add (B ("Chord Arp", "Arp")
             .osc1 (Pwm, 0.3f, 0.6f).chord (1, 4)
             .filter1 (LadderLP, 1200.0f, 0.3f, 2.0f, 1.5f, 0.3f, true)
             .fenv (0.001f, 0.18f, 0.0f, 0.15f).amp (0.001f, 0.25f, 0.0f, 0.2f)
             .arp (7, D1_8, 1, 0.5f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.2f, true).reverb (Room, 0.4f, 0.12f));

    // ======================================================================
    // DRONE
    // ======================================================================

    add (B ("Comb Cathedral", "Drone")
             .osc1 (Basic, 0.0f, 0.3f).noise (0.3f)
             .filter1 (CombPlus, 130.8f, 0.9f, 0.0f, 1.0f, 1.0f)
             .filter2 (LP, 2500.0f, 0.1f)
             .lfo (1, Sine, 0.04f).mod (Lfo1, D::Filter2Cutoff, 0.2f)
             .amp (2.0f, 1.0f, 1.0f, 4.0f)
             .macro (1, "RESONANCE", { { D::Filter1Reso, 0.08f } })
             .macro (2, "TONE", { { D::Filter2Cutoff, 0.4f } })
             .macro (3, "AIR", { { D::NoiseLevel, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-3.0f).reverb (Hall, 0.95f, 0.4f));

    add (B ("Self-Osc Choir", "Drone")
             .osc1 (SineT, 0.0f, 0.1f).noise (0.08f)
             .filter1 (Formant, 800.0f, 0.6f)
             .filter2 (BP, 523.25f, 0.99f, 0.0f, 1.0f, 1.0f)
             .lfo (1, Sine, 0.06f).mod (Lfo1, D::Filter1Morph, 0.4f)
             .amp (2.5f, 1.0f, 1.0f, 4.0f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.4f } })
             .macro (2, "BREATH", { { D::NoiseLevel, 0.2f } })
             .macro (3, "PITCH", { { D::Filter2Cutoff, 0.1f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-4.0f).reverb (Hall, 0.9f, 0.4f));

    add (B ("Ring Drone", "Drone")
             .osc1 (Fractal, 0.3f, 0.6f).osc2 (SineT, 0.0f, 0.0f, 0, 3.0f).warp (1, WRing, 0.5f)
             .filter1 (LadderLP, 1200.0f, 0.4f, 0.0f, 1.5f)
             .lfo (1, Sine, 0.03f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .lfo (2, Tri, 0.05f).mod (Lfo2, D::Filter1Cutoff, 0.2f)
             .amp (1.2f, 1.0f, 1.0f, 4.0f).master (-2.0f)
             .macro (1, "RING", { { D::Osc1Warp, 0.4f } })
             .macro (2, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (B ("Diode Rumble", "Drone")
             .osc1 (DriveSaw, 0.5f, 0.6f).unison (1, 4, 20.0f, 0.8f).sub (0, 0.4f)
             .filter1 (DiodeLP, 300.0f, 0.55f, 0.0f, 3.0f)
             .lfo (1, SampleHold, 0.3f).mod (Lfo1, D::Filter1Cutoff, 0.15f)
             .amp (2.0f, 1.0f, 1.0f, 4.0f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "UNREST", { { D::Lfo1Rate, 0.4f } })
             .macro (3, "DRIVE", { { D::Filter1Drive, 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.3f));

    add (B ("Stairway Drone", "Drone")
             .osc1 (Glass, 0.2f, 0.5f).unison (1, 3, 8.0f, 0.8f).osc2 (Vowel, 0.3f, 0.4f, 7)
             .filter1 (LP, 2500.0f, 0.2f)
             .lfoSync (1, CurveShape, D1_1).curve (1, stairsCurve).mod (Lfo1, D::Osc1Frame, 0.4f)
             .lfo (2, Sine, 0.04f).mod (Lfo2, D::Osc2Frame, 0.3f)
             .amp (2.0f, 1.0f, 1.0f, 4.0f)
             .macro (1, "STEPS", { { D::Osc1Frame, 0.3f } })
             .macro (2, "FIFTH", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.5f, 0.2f, true).reverb (Shimmer, 0.9f, 0.35f));

    add (B ("Tidal Drone", "Drone")
             .osc1 (Analog, 0.5f, 0.5f).unison (1, 6, 12.0f, 0.9f).osc2 (Analog, 0.5f, 0.4f, -12)
             .filter1 (Morph, 800.0f, 0.4f)
             .lfo (1, Sine, 0.03f).mod (Lfo1, D::Filter1Morph, 0.4f)
             .lfo (2, Sine, 0.07f).mod (Lfo2, D::Filter1Cutoff, 0.2f)
             .amp (3.0f, 1.0f, 1.0f, 5.0f)
             .macro (1, "MORPH", { { D::Filter1Morph, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "TIDE", { { D::Lfo1Rate, 0.3f }, { D::Lfo2Rate, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.95f, 0.35f));

    add (B ("Bowed Metal", "Drone")
             .string (1, 0.6f, 1, 0.9f, 0.3f, 0.8f).osc2 (FmMetal, 0.4f, 0.25f)
             .filter1 (LP, 4000.0f, 0.2f)
             .lfo (1, Sine, 0.05f).mod (Lfo1, D::Osc2Frame, 0.3f)
             .amp (1.5f, 1.0f, 1.0f, 4.0f)
             .macro (1, "BOW", { { param ("osc1_string_sustain"), 0.2f } })
             .macro (2, "METAL", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    // ======================================================================
    // FX
    // ======================================================================

    add (B ("Riser Sweep", "FX")
             .osc1 (Riser, 0.0f, 0.6f).unison (1, 5, 20.0f, 0.9f).noise (0.2f)
             .filter1 (LP, 400.0f, 0.4f, 0.0f, 1.0f)
             .env4 (4.0f, 0.1f, 1.0f, 1.0f).mod (Env4, D::Filter1Cutoff, 0.8f).mod (Env4, D::Osc1Frame, 1.0f)
             .mod (Env4, D::Osc1Pitch, 0.1f)
             .amp (2.0f, 1.0f, 1.0f, 1.5f)
             .macro (1, "LENGTH", { { D::E4Attack, 0.4f } })
             .macro (2, "NOISE", { { D::NoiseLevel, 0.3f } })
             .macro (3, "PITCH", { { D::Osc1Pitch, 0.1f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.5f, 0.2f, true).reverb (Hall, 0.9f, 0.3f));

    add (B ("Laser Zap", "FX")
             .osc1 (Basic, 0.0f, 0.7f)
             .menv (0.001f, 0.2f, 0.0f, 0.1f, 0.6f).mod (ModEnv, D::Osc1Pitch, 0.6f)
             .filter1 (LP, 8000.0f, 0.3f)
             .amp (0.001f, 0.3f, 0.0f, 0.2f)
             .macro (1, "DROP", { { D::MeDecay, 0.4f } })
             .macro (2, "DEPTH", { { D::Osc1Pitch, 0.2f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_16, 0.4f, 0.2f, true).reverb (Room, 0.4f, 0.15f));

    add (B ("Noise Sweep", "FX")
             .noise (0.9f).osc1 (Basic, 0.0f, 0.0f)
             .filter1 (BP, 1000.0f, 0.6f).master (-1.0f)
             .lfo (1, Tri, 0.12f).mod (Lfo1, D::Filter1Cutoff, 0.5f)
             .amp (0.5f, 1.0f, 1.0f, 2.0f)
             .macro (1, "SWEEP", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "FOCUS", { { D::Filter1Reso, 0.3f } })
             .macro (3, "CENTRE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxPhaser, FxReverb }).phaser (0.3f, 0.5f, 0.4f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Alarm Siren", "FX")
             .osc1 (Pwm, 0.3f, 0.6f)
             .lfo (1, Tri, 0.8f).mod (Lfo1, D::Osc1Pitch, 0.15f)
             .filter1 (LP, 5000.0f, 0.2f)
             .amp (0.01f, 0.2f, 1.0f, 0.3f)
             .macro (1, "SPEED", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "DEPTH", { { D::Osc1Pitch, 0.1f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.4f, 0.15f, true).reverb (Hall, 0.6f, 0.2f));

    add (B ("Metallic Hit", "FX")
             .osc1 (FmMetal, 0.6f, 0.7f).osc2 (SineT, 0.0f, 0.0f, 17, 13.0f).fm (0.4f, 0.3f)
             .filter1 (CombMinus, 400.0f, 0.7f, 0.0f, 1.0f, 0.5f)
             .amp (0.001f, 0.8f, 0.0f, 0.6f)
             .macro (1, "METAL", { { D::FmAmount, 0.3f } })
             .macro (2, "RING", { { D::Filter1Reso, 0.2f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-2.0f).reverb (Plate, 0.6f, 0.25f));

    add (B ("Glitch Gate", "FX")
             .osc1 (Digital, 0.5f, 0.7f).unison (1, 3, 12.0f, 0.7f)
             .filter1 (LP, 4000.0f, 0.3f)
             .lfoSync (1, SampleHold, D1_16).mod (Lfo1, D::Osc1Frame, 0.4f).mod (Lfo1, D::Filter1Cutoff, 0.2f)
             .amp (0.001f, 0.3f, 1.0f, 0.2f)
             .macro (1, "CHAOS", { { D::Osc1Frame, 0.3f } })
             .macro (2, "GATE", { { param ("fx_gate_mix"), 0.0f }, { param ("fx_gate_smooth"), 0.4f } })
             .macro (3, "CRUSH", { { D::FxCrushMix, 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxGate, FxCrush, FxDelay }).gate (D1_16, 4, 0.1f).crush (6.0f, 4.0f, 0.2f).delay (D1_16, 0.4f, 0.15f, true));

    add (B ("Down Lifter", "FX")
             .osc1 (Riser, 1.0f, 0.6f).unison (1, 5, 20.0f, 0.9f).noise (0.2f)
             .filter1 (LP, 12000.0f, 0.3f)
             .menv (0.001f, 3.0f, 0.0f, 1.0f).mod (ModEnv, D::Osc1Frame, -1.0f).mod (ModEnv, D::Osc1Pitch, 0.1f)
             .mod (ModEnv, D::Filter1Cutoff, 0.3f)
             .amp (0.001f, 3.0f, 0.0f, 1.0f)
             .macro (1, "LENGTH", { { D::MeDecay, 0.4f }, { D::AmpDecay, 0.4f } })
             .macro (2, "NOISE", { { D::NoiseLevel, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (B ("Freq Shift Wash", "FX")
             .osc1 (Glass, 0.5f, 0.5f).unison (1, 4, 12.0f, 0.8f)
             .filter1 (LP, 5000.0f, 0.2f)
             .amp (0.5f, 1.0f, 1.0f, 3.0f)
             .macro (1, "SHIFT", { { param ("fx_shifter_shift"), 0.05f } })
             .macro (2, "BLEND", { { param ("fx_shifter_mix"), 0.4f } })
             .macro (3, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxFreqShift, FxDelay, FxReverb }).set ("fx_shifter_shift", 30.0f).set ("fx_shifter_mix", 0.4f)
             .delay (D1_4, 0.6f, 0.25f, true).reverb (Hall, 0.9f, 0.35f));

    // ======================================================================
    // More keys, arps, drones and textures
    // ======================================================================

    add (B ("Wurli Drive", "Keys")
             .osc1 (TriangleT, 0.0f, 0.75f).osc2 (SineT, 0.0f, 0.3f, 12).warp (1, WAsym, 0.2f)
             .filter1 (LP, 3500.0f, 0.1f, 1.0f, 1.0f, 0.3f)
             .fenv (0.001f, 0.4f, 0.2f, 0.3f).amp (0.001f, 1.6f, 0.25f, 0.4f)
             .mod (Vel, D::Osc1Warp, 0.35f)
             .macro (1, "BARK", { { D::Osc1Warp, 0.4f } })
             .macro (2, "DRIVE", { { param ("fx_amp_drive"), 0.4f } })
             .macro (3, "TREMOLO", { { param ("fx_trem_depth"), 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxTremolo, FxReverb }).ampSim (0, 2.0f, 1.0f, 1.1f, 0.9f, 0.9f).tremolo (5.0f, 0.0f).reverb (Room, 0.4f, 0.12f));

    add (B ("Celeste", "Keys")
             .osc1 (SineT, 0.0f, 0.6f).osc2 (SineT, 0.0f, 0.35f, 24, 4.0f)
             .fm (0.1f).menv (0.001f, 0.05f, 0.0f, 0.05f).mod (ModEnv, D::FmAmount, 0.3f)
             .filter1 (LP, 9000.0f, 0.0f)
             .amp (0.001f, 1.8f, 0.0f, 1.0f, -0.4f)
             .macro (1, "SPARKLE", { { D::Osc2Level, 0.3f } })
             .macro (2, "STRIKE", { { D::FmAmount, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Plate, 0.55f, 0.2f));

    add (B ("Soft Synth Piano", "Keys")
             .osc1 (Analog, 0.2f, 0.6f).osc2 (TriangleT, 0.0f, 0.4f, 12)
             .filter1 (LadderLP, 1800.0f, 0.1f, 2.0f, 1.0f, 0.5f)
             .fenv (0.001f, 0.6f, 0.1f, 0.5f).amp (0.001f, 2.4f, 0.0f, 0.6f)
             .filterVelocity (0.8f)
             .macro (1, "BRIGHT", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "HAMMER", { { D::Filter1Env, 0.3f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.15f, 0.2f).reverb (Hall, 0.55f, 0.18f));

    add (B ("Pulse Arp Down", "Arp")
             .osc1 (Pwm, 0.4f, 0.7f).warp (1, WPwm, 0.2f)
             .filter1 (LP, 2500.0f, 0.3f, 1.5f, 1.2f, 0.4f)
             .fenv (0.001f, 0.15f, 0.0f, 0.1f).amp (0.001f, 0.2f, 0.0f, 0.15f)
             .arp (1, D1_16, 3, 0.5f)
             .lfo (1, Tri, 0.2f).mod (Lfo1, D::Osc1Warp, 0.25f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "PWM", { { D::Osc1Warp, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.2f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Converge Bells", "Arp")
             .osc1 (FmMetal, 0.2f, 0.5f).osc2 (SineT, 0.0f, 0.5f)
             .filter1 (LP, 7000.0f, 0.0f)
             .amp (0.001f, 0.6f, 0.0f, 0.5f)
             .arp (5, D1_16, 2, 0.4f)
             .macro (1, "METAL", { { D::Osc1Level, 0.3f } })
             .macro (2, "GATE", { { param ("arp_gate"), 0.4f } })
             .macro (3, "FRAME", { { D::Osc1Frame, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f }, { D::FxDelayMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8T, 0.4f, 0.18f, true).reverb (Plate, 0.6f, 0.22f));

    add (B ("Deep Space Drone", "Drone")
             .osc1 (SineT, 0.0f, 0.5f).osc2 (Glass, 0.3f, 0.3f, 7).unison (2, 4, 10.0f, 0.9f).sub (0, 0.4f)
             .filter1 (LP, 2000.0f, 0.2f)
             .lfo (1, Sine, 0.02f).mod (Lfo1, D::Osc2Frame, 0.4f)
             .amp (1.5f, 1.0f, 1.0f, 5.0f)
             .macro (1, "GLOW", { { D::Osc2Level, 0.3f } })
             .macro (2, "FRAME", { { D::Osc2Frame, 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_2, 0.6f, 0.2f, true).reverb (Shimmer, 0.95f, 0.4f));

    add (B ("Oversampled Grind", "Drone")
             .osc1 (DriveSaw, 0.7f, 0.6f).unison (1, 3, 15.0f, 0.8f).osc2 (Wavefold, 0.5f, 0.4f, -12)
             .filter1 (DiodeLP, 500.0f, 0.6f, 0.0f, 5.0f)
             .lfo (1, Tri, 0.05f).mod (Lfo1, D::Filter1Cutoff, 0.3f)
             .amp (1.0f, 1.0f, 1.0f, 3.0f)
             .set ("oversampling", 1).set ("os_factor", 1)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "FOLD", { { D::Osc2Frame, 0.4f } })
             .macro (3, "DRIVE", { { D::Filter1Drive, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb, FxLimiter }).reverb (Hall, 0.85f, 0.3f).limiter (-1.0f));

    add (B ("Stutter Hit", "FX")
             .osc1 (DriveSaw, 0.5f, 0.7f).unison (1, 3, 15.0f, 0.7f).sub (0, 0.4f)
             .filter1 (LP, 3000.0f, 0.2f)
             .amp (0.001f, 1.2f, 0.0f, 0.5f)
             .macro (1, "STUTTER", { { param ("fx_stutter_mix"), 0.8f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "LENGTH", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxStutter, FxReverb }).set ("fx_stutter_on", 1).set ("fx_stutter_div", (float) D1_16).set ("fx_stutter_mix", 0.0f).reverb (Room, 0.5f, 0.15f));

    add (B ("Radio Tuning", "FX")
             .osc1 (Digital, 0.5f, 0.4f).noise (0.3f)
             .filter1 (BP, 1500.0f, 0.5f)
             .lfo (1, SampleHold, 3.0f).mod (Lfo1, D::Filter1Cutoff, 0.3f).mod (Lfo1, D::Osc1Pitch, 0.05f)
             .amp (0.1f, 1.0f, 1.0f, 1.0f).master (-2.0f)
             .macro (1, "SEARCH", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "STATIC", { { D::NoiseLevel, 0.3f } })
             .macro (3, "RING", { { param ("fx_ring_mix"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxRingMod, FxReverb }).set ("fx_ring_freq", 400.0f).set ("fx_ring_mix", 0.0f).reverb (Spring, 0.5f, 0.2f));

    add (B ("Ladder Swell Pad", "Pad")
             .osc1 (Analog, 0.5f, 0.55f).unison (1, 5, 12.0f, 0.8f).osc2 (Pwm, 0.5f, 0.35f, 12)
             .filter1 (LadderLP, 300.0f, 0.5f, 3.5f, 1.3f, 0.3f, true)
             .fenv (2.0f, 2.0f, 0.5f, 2.0f, 0.5f).amp (1.0f, 1.0f, 0.9f, 2.5f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "SWELL", { { D::FeAttack, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("EQ Sculpted Pad", "Pad")
             .osc1 (Fractal, 0.4f, 0.55f).unison (1, 5, 14.0f, 0.9f).osc2 (Vowel, 0.3f, 0.35f, 12)
             .filter1 (LP, 6000.0f, 0.1f)
             .lfo (1, Sine, 0.08f).mod (Lfo1, D::Osc1Frame, 0.3f)
             .amp (0.8f, 1.0f, 0.9f, 2.5f)
             .macro (1, "LOW", { { param ("fx_eq_low_gain"), 0.25f } })
             .macro (2, "PRESENCE", { { param ("fx_eq_mid_gain"), 0.25f } })
             .macro (3, "AIR", { { param ("fx_eq_high_gain"), 0.25f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxEq, FxReverb }).eq (-3.0f, 2500.0f, -2.0f, 1.0f, 150.0f, 8000.0f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Sitar Pluck", "Pluck")
             .string (1, 0.75f, 2, 0.7f, 0.2f)
             .filter1 (BP, 2200.0f, 0.3f, 0.0f, 1.0f, 0.6f).parallel().filter2 (LP, 8000.0f, 0.0f)
             .amp (0.001f, 2.0f, 0.0f, 0.6f).master (-3.0f)
             .macro (1, "BUZZ", { { D::Filter1Reso, 0.3f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.2f } })
             .macro (3, "TONE", { { D::Filter2Cutoff, -0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.18f));

    add (B ("Legato Glide Lead", "Lead")
             .osc1 (DriveSaw, 0.4f, 0.75f).unison (1, 3, 9.0f, 0.5f).osc2 (SineT, 0.0f, 0.3f, -12)
             .filter1 (LadderLP, 2500.0f, 0.35f, 1.2f, 2.0f, 0.5f, true)
             .fenv (0.003f, 0.5f, 0.5f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.3f)
             .legato (0.15f).bend (12.0f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GLIDE", { { param ("glide"), 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.15f, true).reverb (Plate, 0.5f, 0.15f));


    // ======================================================================
    // v1.1: drums, FM, spectral, granular, chaos, trance gate, generative
    // ======================================================================

    // DRUMS ------------------------------------------------------------------

    add (B ("Chaos Kick", "Drums")
             .osc1 (SineT, 0.0f, 0.95f).sub (0, 0.35f)
             .menv (0.0f, 0.07f, 0.0f, 0.05f, -0.6f).mod (ModEnv, D::Osc1Pitch, 0.5f)
             .env4 (0.0f, 0.012f, 0.0f, 0.01f).mod (Env4, D::NoiseLevel, 0.5f)
             .amp (0.0f, 0.42f, 0.0f, 0.1f, -0.4f)
             .filter1 (LP, 9000.0f, 0.0f)
             .voices (1)
             .macro (1, "PUNCH", { { D::Osc1Pitch, 0.15f }, { D::MeDecay, 0.3f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.5f } })
             .macro (3, "CLICK", { { D::NoiseLevel, 0.3f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.6f } })
             .fx ({ FxDrive, FxComp, FxLimiter }).driveFx (2.2f, 0.5f).comp (-18.0f, 4.0f, 6.0f).limiter (-0.5f).master (0.0f));

    add (B ("FM Snare", "Drums")
             .osc1 (SineT, 0.0f, 0.6f, 12).osc2 (SineT, 0.0f, 0.0f, 19).modOnly (2)
             .fmRoute (2, 1, 0.55f).fmMode (FmThroughZero)
             .noise (0.55f)
             .menv (0.0f, 0.05f, 0.0f, 0.04f).mod (ModEnv, D::Osc1Pitch, 0.18f)
             .filter1 (BP, 2400.0f, 0.25f, 0.0f, 1.2f)
             .amp (0.0f, 0.22f, 0.0f, 0.12f, -0.5f).master (-2.0f)
             .macro (1, "SNAP", { { D::NoiseLevel, 0.3f } })
             .macro (2, "BODY", { { D::FmAmount, -0.4f }, { D::Osc1Level, 0.2f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxComp, FxReverb }).comp (-16.0f, 3.0f, 3.0f).reverb (Room, 0.25f, 0.08f));

    add (B ("Metal Hat", "Drums")
             .osc1 (FmMetal, 0.8f, 0.9f, 24).osc2 (SineT, 0.0f, 0.0f, 31).modOnly (2)
             .fmRoute (2, 1, 0.8f).ring (0.4f).noise (0.7f)
             .filter1 (HP, 4500.0f, 0.2f).master (0.0f)
             .amp (0.0f, 0.06f, 0.0f, 0.05f, -0.6f)
             .lfo (1, SampleHold, 30.0f).mod (Lfo1, D::Filter1Cutoff, 0.05f)
             .macro (1, "OPEN", { { D::AmpDecay, 0.6f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "METAL", { { D::FmAmount, 0.2f }, { D::RingMod, 0.4f } })
             .macro (4, "WIDTH", { { param ("fx_width"), 0.4f } })
             .fx ({ FxWidener }).width (1.4f, 1.0f));

    add (B ("Granular Clap", "Drums")
             .granular (1, 5, 0.9f, 0.55f, 18.0f, 0.95f, 0.6f, 0.3f, 0.9f)
             .noise (0.25f)
             .filter1 (BP, 1400.0f, 0.3f, 0.0f, 1.3f)
             .amp (0.0f, 0.2f, 0.0f, 0.15f, -0.4f)
             .macro (1, "SPREAD", { { param ("osc1_grain_spray"), 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (3, "TAIL", { { D::AmpDecay, 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxComp, FxReverb }).comp (-18.0f, 4.0f, 6.0f).reverb (Plate, 0.3f, 0.12f).master (0.0f));

    add (B ("Drunk Tom", "Drums")
             .osc1 (SineT, 0.0f, 0.9f).osc2 (TriangleT, 0.0f, 0.3f, 7)
             .menv (0.0f, 0.12f, 0.0f, 0.1f, -0.4f).mod (ModEnv, D::Osc1Pitch, 0.25f).mod (ModEnv, D::Osc2Pitch, 0.25f)
             .lfo (1, Drunk, 3.0f).mod (Lfo1, D::Osc1Pitch, 0.02f)
             .amp (0.0f, 0.5f, 0.0f, 0.2f, -0.3f)
             .filter1 (LP, 5000.0f, 0.1f)
             .macro (1, "PITCH", { { D::Osc1Pitch, 0.12f }, { D::Osc2Pitch, 0.12f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (3, "WANDER", { { D::Lfo1Rate, 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDrive, FxReverb }).driveFx (1.5f, 0.3f).reverb (Room, 0.4f, 0.1f));

    add (B ("Spectral Rim", "Drums")
             .osc1 (FibonacciBell, 0.4f, 0.8f, 24).spectral (1, SpStretch, 0.55f)
             .osc2 (Wood, 0.3f, 0.5f, 31)
             .amp (0.0f, 0.09f, 0.0f, 0.08f, -0.5f).master (-2.0f)
             .filter1 (HP, 400.0f, 0.1f)
             .macro (1, "STRETCH", { { D::Osc1Frame, 0.4f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.5f } })
             .macro (3, "WOOD", { { D::Osc2Level, 0.3f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay }).delay (D1_16D, 0.25f, 0.08f, true));

    // FM ---------------------------------------------------------------------

    add (B ("Operator Bell", "Keys")
             .osc1 (SineT, 0.0f, 0.75f).osc2 (SineT, 0.0f, 0.0f, 24, 3.0f).osc3 (SineT, 0.0f, 0.3f, 12)
             .modOnly (2).fmRoute (2, 1, 0.4f).fmRoute (2, 3, 0.25f).fmRoute (3, 3, 0.1f)
             .menv (0.0f, 1.4f, 0.1f, 0.8f).mod (ModEnv, D::FmAmount, 0.35f).mod (ModEnv, D::Fm2to3, 0.25f)
             .amp (0.001f, 2.4f, 0.0f, 1.2f).velocity (0.6f)
             .macro (1, "BRIGHT", { { D::FmAmount, 0.3f }, { D::Fm2to3, 0.2f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f }, { D::MeDecay, 0.4f } })
             .macro (3, "SHIMMER", { { D::Fm3Feedback, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.3f, 0.2f).reverb (Hall, 0.7f, 0.22f));

    add (B ("Through-Zero Growl", "Bass")
             .osc1 (Basic, 0.2f, 0.8f).osc2 (SineT, 0.0f, 0.0f, -12).osc3 (SineT, 0.0f, 0.0f, 7).modOnly (2).modOnly (3)
             .fmRoute (2, 1, 0.5f).fmRoute (3, 2, 0.3f).fmMode (FmThroughZero).sub (0, 0.45f)
             .lfoSync (1, Sine, D1_8).mod (Lfo1, D::FmAmount, 0.3f)
             .filter1 (LadderLP, 1600.0f, 0.3f, 0.0f, 2.2f, 0.2f, true)
             .amp (0.002f, 0.3f, 0.9f, 0.1f).mono (0.02f)
             .macro (1, "GROWL", { { D::FmAmount, 0.35f } })
             .macro (2, "WOBBLE", { { D::Lfo1Rate, 0.4f } })
             .macro (3, "CHAIN", { { D::Fm3to2, 0.4f } })
             .macro (4, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .fx ({ FxOtt, FxLimiter }).ott (0.3f, 0.4f).limiter (-0.8f));

    add (B ("Feedback Choir", "Pad")
             .osc1 (Choir, 0.3f, 0.6f).unison (1, 4, 14.0f, 0.8f).osc2 (SineT, 0.0f, 0.35f, 12)
             .fmRoute (2, 2, 0.35f).fmRoute (2, 1, 0.12f).fmMode (FmExp)
             .lfo (1, SmoothRandom, 0.2f).mod (Lfo1, D::Osc1Frame, 0.3f).mod (Lfo1, D::Fm2Feedback, 0.15f)
             .filter1 (LP, 3200.0f, 0.2f)
             .amp (1.2f, 1.5f, 0.85f, 2.5f)
             .macro (1, "VOWEL", { { D::Osc1Frame, 0.4f } })
             .macro (2, "FEEDBACK", { { D::Fm2Feedback, 0.3f } })
             .macro (3, "FM", { { D::FmAmount, 0.25f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.4f, 0.3f).reverb (Hall, 0.85f, 0.3f));

    add (B ("Key FM Scream", "Lead")
             .osc1 (DriveSaw, 0.3f, 0.75f).unison (1, 3, 8.0f, 0.5f).osc2 (SineT, 0.0f, 0.3f)
             .lfo (1, Sine, 8.0f).lfoKey (1).mod (Lfo1, D::Osc2Level, 0.4f)
             .fmRoute (2, 1, 0.2f)
             .menv (0.002f, 0.6f, 0.2f, 0.3f).mod (ModEnv, D::FmAmount, 0.25f)
             .filter1 (LadderLP, 3200.0f, 0.35f, 1.0f, 2.5f, 0.5f, true)
             .fenv (0.002f, 0.4f, 0.4f, 0.3f).amp (0.002f, 0.3f, 0.9f, 0.25f).legato (0.05f)
             .macro (1, "SCREAM", { { D::FmAmount, 0.35f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "AM", { { D::Osc2Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDrive, FxDelay, FxReverb }).driveFx (2.0f, 0.3f).delay (D1_8D, 0.35f, 0.12f, true).reverb (Plate, 0.4f, 0.1f));

    // SPECTRAL ---------------------------------------------------------------

    add (B ("Stretched Glass Keys", "Keys")
             .osc1 (Glass, 0.2f, 0.75f).spectral (1, SpStretch, 0.35f).osc2 (SineT, 0.0f, 0.3f, 12)
             .filter1 (LP, 7000.0f, 0.15f, 0.8f, 1.0f, 0.4f)
             .fenv (0.001f, 0.8f, 0.3f, 0.6f).amp (0.001f, 1.6f, 0.4f, 0.9f).velocity (0.5f)
             .menv (0.0f, 0.9f, 0.0f, 0.5f).mod (ModEnv, D::Osc1Frame, 0.3f)
             .macro (1, "MORPH", { { D::Osc1Frame, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.3f, 0.25f).reverb (Plate, 0.6f, 0.2f));

    add (B ("Formant Shifter", "Lead")
             .osc1 (Talkbox, 0.3f, 0.8f).spectral (1, SpFormant, 0.7f).unison (1, 2, 6.0f, 0.4f)
             .lfo (1, Tri, 0.35f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .filter1 (LP, 5000.0f, 0.2f).amp (0.01f, 0.3f, 0.9f, 0.3f).legato (0.08f)
             .macro (1, "VOWEL", { { D::Osc1Frame, 0.4f } })
             .macro (2, "SPEED", { { D::Lfo1Rate, 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.35f, 0.15f, true).reverb (Hall, 0.5f, 0.15f));

    add (B ("Harmonic Cut Pad", "Pad")
             .osc1 (PartialCloud, 0.5f, 0.6f).spectral (1, SpCut, 0.45f).unison (1, 5, 16.0f, 0.9f)
             .osc2 (SoftSaw, 0.3f, 0.4f, 12).spectral (2, SpOddEven, 0.25f)
             .lfo (1, Chaos, 0.08f).mod (Lfo1, D::Osc1Frame, 0.3f).mod (Lfo1, D::Pan, 0.2f)
             .filter1 (LP, 4000.0f, 0.15f)
             .amp (1.5f, 2.0f, 0.8f, 3.0f)
             .macro (1, "DRIFT", { { D::Osc1Frame, 0.4f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.5f, 0.3f).reverb (Shimmer, 0.8f, 0.3f));

    add (B ("Smeared Organ", "Keys")
             .osc1 (Organ, 0.6f, 0.75f).spectral (1, SpSmear, 0.3f).osc2 (Organ, 0.2f, 0.35f, 12)
             .filter1 (LP, 6000.0f, 0.1f)
             .amp (0.01f, 0.4f, 0.9f, 0.3f)
             .lfo (1, Sine, 5.5f).mod (Lfo1, D::Osc1Pitch, 0.002f)
             .macro (1, "DRAWBARS", { { D::Osc1Frame, 0.4f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, 0.3f } })
             .macro (3, "LESLIE", { { param ("fx_chorus_mix"), 0.4f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxChorus, FxReverb }).driveFx (1.5f, 0.3f).chorus (5.0f, 0.2f, 0.25f).reverb (Room, 0.4f, 0.12f));

    add (B ("Shift Bass", "Bass")
             .osc1 (SawOctaves, 0.3f, 0.8f).spectral (1, SpShift, 0.2f).sub (0, 0.55f)
             .filter1 (LadderLP, 900.0f, 0.35f, 1.5f, 2.0f, 0.2f, true)
             .fenv (0.001f, 0.25f, 0.2f, 0.15f).amp (0.002f, 0.3f, 0.85f, 0.1f).mono (0.02f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "FRAME", { { D::Osc1Frame, 0.5f } })
             .macro (3, "PUNCH", { { D::Filter1Env, 0.3f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.2f, 0.35f).eq (2.0f, 500.0f, -2.0f, 0.0f));

    // GRANULAR ---------------------------------------------------------------

    add (B ("Grain Choir", "Pad")
             .granular (1, 2, 0.8f, 0.4f, 140.0f, 0.75f, 0.25f, 0.03f, 0.8f).unison (1, 2, 8.0f, 0.8f)
             .osc2 (Choir, 0.4f, 0.25f)
             .lfo (1, SmoothRandom, 0.15f).mod (Lfo1, D::Osc1SampleStart, 0.15f)
             .filter1 (LP, 5000.0f, 0.1f)
             .amp (0.8f, 1.5f, 0.85f, 2.5f)
             .macro (1, "SCRUB", { { D::Osc1SampleStart, 0.4f } })
             .macro (2, "SPRAY", { { param ("osc1_grain_spray"), 0.5f } })
             .macro (3, "SHIMMER", { { param ("osc1_grain_pitch"), 0.2f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.4f, 0.25f).reverb (Hall, 0.85f, 0.3f));

    add (B ("Swarm", "Drone")
             .granular (1, 1, 0.7f, 0.3f, 45.0f, 1.0f, 0.8f, 0.05f, 1.0f)
             .granular (2, 4, 0.4f, 0.5f, 220.0f, 0.4f, 0.6f, 0.2f, 1.0f)
             .lfo (1, Chaos, 0.1f).mod (Lfo1, D::Osc1SampleStart, 0.3f).mod (Lfo1, D::Filter1Cutoff, 0.15f)
             .filter1 (BP, 1800.0f, 0.3f, 0.0f, 1.2f, 0.5f)
             .amp (2.0f, 2.0f, 0.9f, 4.0f)
             .macro (1, "SWARM", { { param ("osc1_grain_pitch"), 0.5f } })
             .macro (2, "DENSITY", { { param ("osc1_grain_density"), -0.6f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (B ("Vinyl Dust Keys", "Keys")
             .osc1 (SoftSaw, 0.2f, 0.7f).osc2 (SineT, 0.0f, 0.35f, 12)
             .granular (3, 4, 0.18f, 0.2f, 60.0f, 0.5f, 0.9f, 0.0f, 1.0f)
             .filter1 (LP, 2800.0f, 0.15f, 0.6f, 1.2f, 0.4f)
             .fenv (0.001f, 0.9f, 0.3f, 0.5f).amp (0.002f, 1.2f, 0.5f, 0.6f).velocity (0.5f)
             .macro (1, "DUST", { { D::SubLevel, 0.3f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "WOBBLE", { { param ("fx_chorus_mix"), 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxCrush, FxReverb }).chorus (0.6f, 0.4f, 0.3f).crush (12.0f, 2.0f, 0.25f).reverb (Room, 0.5f, 0.18f));

    add (B ("Frozen Grain Lead", "Lead")
             .granular (1, 2, 0.85f, 0.35f, 60.0f, 0.85f, 0.05f, 0.0f, 0.4f)
             .osc2 (Vowel, 0.3f, 0.35f).legato (0.07f)
             .lfo (1, Sine, 5.0f).mod (Lfo1, D::Osc1Pitch, 0.004f)
             .filter1 (LP, 4500.0f, 0.2f)
             .amp (0.01f, 0.3f, 0.9f, 0.3f)
             .macro (1, "SCRUB", { { D::Osc1SampleStart, 0.4f } })
             .macro (2, "GRAIN", { { param ("osc1_grain_size"), 0.4f } })
             .macro (3, "VIBRATO", { { D::Osc1Pitch, 0.004f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.4f, 0.15f, true).reverb (Plate, 0.5f, 0.15f));

    // CHAOS / DRUNK ----------------------------------------------------------

    add (B ("Lorenz Bass", "Bass")
             .osc1 (LorenzT, 0.3f, 0.8f).sub (0, 0.5f)
             .lfo (1, Chaos, 1.2f).mod (Lfo1, D::Osc1Frame, 0.35f).mod (Lfo1, D::Filter1Cutoff, 0.15f)
             .filter1 (LadderLP, 1100.0f, 0.4f, 0.0f, 2.2f, 0.2f, true)
             .amp (0.002f, 0.3f, 0.9f, 0.12f).mono (0.02f)
             .macro (1, "CHAOS", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "FRAME", { { D::Osc1Frame, 0.3f } })
             .macro (4, "DIRT", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxOtt }).driveFx (2.0f, 0.3f).ott (0.3f, 0.3f));

    add (B ("Drunk Tape Keys", "Keys")
             .osc1 (Analog, 0.2f, 0.7f).osc2 (TriangleT, 0.0f, 0.4f, 12)
             .lfo (1, Drunk, 1.5f).mod (Lfo1, D::Osc1Pitch, 0.004f).mod (Lfo1, D::Osc2Pitch, 0.004f)
             .lfo (2, SmoothRandom, 0.5f).mod (Lfo2, D::Filter1Cutoff, 0.1f)
             .filter1 (LP, 2600.0f, 0.15f, 0.5f, 1.2f, 0.4f)
             .fenv (0.002f, 0.7f, 0.3f, 0.5f).amp (0.002f, 1.0f, 0.6f, 0.5f)
             .macro (1, "WARBLE", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DUST", { { D::FxCrushMix, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxCrush, FxChorus, FxReverb }).crush (12.0f, 1.0f, 0.0f).chorus (0.3f, 0.4f, 0.3f).reverb (Room, 0.4f, 0.15f));

    add (B ("Chaos Filter Pad", "Pad")
             .osc1 (SawOctaves, 0.4f, 0.55f).unison (1, 6, 18.0f, 0.9f, Hypersaw)
             .osc2 (SquareSweep, 0.3f, 0.35f, -12)
             .lfo (1, Chaos, 0.25f).mod (Lfo1, D::Filter1Cutoff, 0.25f)
             .lfo (2, SmoothRandom, 0.3f).mod (Lfo2, D::Osc2Frame, 0.4f)
             .filter1 (LP, 1800.0f, 0.4f, 0.0f, 1.3f, 0.3f)
             .amp (0.9f, 1.5f, 0.85f, 2.0f)
             .macro (1, "CHAOS", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.35f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxPhaser, FxReverb }).phaser (0.2f, 0.5f, 0.4f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    // TRANCE GATE --------------------------------------------------------------

    add (B ("Gated Supersaw", "Chords")
             .osc1 (Analog, 0.3f, 0.6f).unison (1, 7, 24.0f, 0.9f, Hypersaw).osc2 (Analog, 0.3f, 0.4f, 12).unison (2, 5, 20.0f, 0.9f, Hypersaw)
             .chord (1, 2)
             .filter1 (LP, 5000.0f, 0.2f)
             .amp (0.005f, 0.4f, 0.9f, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "SMOOTH", { { param ("fx_gate_smooth"), 0.5f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxGate, FxDelay, FxReverb }).gateSteps (D1_16, "9090990990909909", 0.15f)
             .delay (D1_8D, 0.4f, 0.15f, true).reverb (Hall, 0.7f, 0.2f));

    add (B ("Swing Gate Pad", "Pad")
             .osc1 (Choir, 0.4f, 0.6f).unison (1, 4, 12.0f, 0.9f).osc2 (SoftSaw, 0.3f, 0.35f, 7)
             .filter1 (LP, 4000.0f, 0.2f)
             .amp (0.4f, 1.0f, 0.9f, 1.5f)
             .macro (1, "SWING", { { param ("fx_gate_swing"), 0.4f } })
             .macro (2, "GATE", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxGate, FxChorus, FxReverb }).gateSteps (D1_16, "95309530", 0.3f, 0.25f)
             .chorus (0.4f, 0.4f, 0.3f).reverb (Plate, 0.7f, 0.25f));

    add (B ("Stutter Stab", "Chords")
             .osc1 (DriveSaw, 0.4f, 0.7f).unison (1, 3, 10.0f, 0.6f).chord (1, 3)
             .filter1 (LadderLP, 2200.0f, 0.3f, 1.4f, 1.8f, 0.3f)
             .fenv (0.001f, 0.3f, 0.2f, 0.2f).amp (0.001f, 0.6f, 0.5f, 0.3f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "GATE", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "PUNCH", { { D::Filter1Env, 0.35f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxGate, FxOtt, FxReverb }).gateSteps (D1_32, "9900990099909000", 0.05f)
             .ott (0.3f, 0.3f).reverb (Room, 0.4f, 0.15f));

    // GENERATIVE ---------------------------------------------------------------

    add (B ("Pentatonic Rain", "Generative")
             .osc1 (Glass, 0.3f, 0.6f).osc2 (SineT, 0.0f, 0.35f, 12)
             .scale (MinorPenta, An).spray (4, 24, SprayUp, 900.0f, 0.8f, 0.4f)
             .filter1 (LP, 6000.0f, 0.1f, 0.5f, 1.0f, 0.4f)
             .fenv (0.001f, 0.5f, 0.2f, 0.5f).amp (0.001f, 1.2f, 0.0f, 1.0f).velocity (0.6f)
             .macro (1, "DENSITY", { { param ("spray_chance"), -0.5f } })
             .macro (2, "SPREAD", { { param ("spray_spread"), 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.25f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.45f, 0.2f, true).reverb (Hall, 0.85f, 0.3f));

    add (B ("Scale Walker", "Generative")
             .osc1 (PluckPosition, 0.4f, 0.75f).osc2 (SineT, 0.0f, 0.3f, -12)
             .scale (Dorian, Dn).arp (ArpScaleRandom, D1_16, 2, 0.5f).arpChance (0.75f)
             .filter1 (LadderLP, 2600.0f, 0.3f, 1.2f, 1.5f, 0.4f)
             .fenv (0.001f, 0.2f, 0.1f, 0.2f).amp (0.001f, 0.35f, 0.2f, 0.25f)
             .macro (1, "CHANCE", { { param ("arp_chance"), 0.25f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "PLUCK", { { D::FeDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.2f, true).reverb (Plate, 0.5f, 0.15f));

    add (B ("Hirajoshi Spray", "Generative")
             .string (1, 0.8f, 0, 0.7f, 0.3f)
             .scale (Hirajoshi, En).spray (3, 12, SprayBoth, 350.0f, 0.9f, 0.5f)
             .filter1 (LP, 7000.0f, 0.1f)
             .amp (0.001f, 1.5f, 0.0f, 1.0f).master (-2.0f)
             .macro (1, "DENSITY", { { param ("spray_chance"), -0.5f } })
             .macro (2, "STRUM", { { param ("spray_spread"), 0.3f } })
             .macro (3, "DAMP", { { param ("osc1_string_damp"), 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.7f, 0.25f));

    add (B ("Blues Machine", "Generative")
             .osc1 (Organ, 0.4f, 0.7f).osc2 (Basic, 0.2f, 0.25f, 12)
             .scale (Blues, Gn, true).arp (ArpScaleRandom, D1_8T, 1, 0.6f).arpChance (0.85f)
             .spray (1, 12, SprayUp, 0.0f, 0.3f, 0.3f)
             .filter1 (LP, 3500.0f, 0.2f, 0.6f, 1.3f, 0.3f)
             .fenv (0.001f, 0.3f, 0.3f, 0.2f).amp (0.002f, 0.4f, 0.6f, 0.2f)
             .macro (1, "CHANCE", { { param ("arp_chance"), 0.15f } })
             .macro (2, "HARMONY", { { param ("spray_chance"), 0.6f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxChorus, FxReverb }).driveFx (1.6f, 0.3f).chorus (4.0f, 0.2f, 0.2f).reverb (Room, 0.5f, 0.15f));

    // FX -----------------------------------------------------------------------

    add (B ("Granular Riser", "FX")
             .granular (1, 5, 0.8f, 0.1f, 90.0f, 0.8f, 0.3f, 0.1f, 1.0f)
             .env4 (4.0f, 0.1f, 1.0f, 1.0f).mod (Env4, D::Osc1SampleStart, 0.8f).mod (Env4, D::Osc1Pitch, 0.25f)
             .mod (Env4, D::Filter1Cutoff, 0.5f)
             .filter1 (HP, 200.0f, 0.3f)
             .amp (2.0f, 0.1f, 1.0f, 1.5f).master (0.0f)
             .macro (1, "RISE", { { D::E4Attack, 0.5f } })
             .macro (2, "SPRAY", { { param ("osc1_grain_spray"), 0.5f } })
             .macro (3, "PITCH", { { D::Osc1Pitch, 0.2f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (B ("Lorenz Radio", "FX")
             .osc1 (Logistic, 0.5f, 0.6f).spectral (1, SpSmear, 0.6f).noise (0.2f)
             .lfo (1, Chaos, 3.0f).mod (Lfo1, D::Osc1Pitch, 0.3f).mod (Lfo1, D::Filter1Cutoff, 0.3f)
             .lfo (2, Drunk, 2.0f).mod (Lfo2, D::Osc1Frame, 0.5f)
             .filter1 (BP, 1500.0f, 0.5f, 0.0f, 1.5f)
             .amp (0.1f, 1.0f, 0.9f, 1.0f)
             .macro (1, "CHAOS", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "STATIC", { { D::NoiseLevel, 0.3f } })
             .macro (3, "BAND", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxRingMod, FxDelay }).delay (D1_8, 0.5f, 0.2f, true));

    add (B ("Audio Rate Siren", "FX")
             .osc1 (SineT, 0.0f, 0.7f).osc2 (Basic, 0.0f, 0.3f)
             .lfo (1, Tri, 2.0f).lfoKey (1).mod (Lfo1, D::Osc1Level, 0.5f)
             .lfo (2, Sine, 0.3f).mod (Lfo2, D::Osc1Pitch, 0.15f).mod (Lfo2, D::Osc2Pitch, 0.15f)
             .filter1 (LP, 5000.0f, 0.3f)
             .amp (0.05f, 1.0f, 0.9f, 0.8f)
             .macro (1, "SPEED", { { D::Lfo2Rate, 0.4f } })
             .macro (2, "AM", { { D::Osc1Level, 0.3f } })
             .macro (3, "BUZZ", { { D::Osc2Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.6f, 0.2f));

    // PLUCKS / ARPS --------------------------------------------------------------

    add (B ("FM Kalimba", "Pluck")
             .osc1 (SineT, 0.0f, 0.8f).osc2 (SineT, 0.0f, 0.0f, 31).modOnly (2)
             .fmRoute (2, 1, 0.35f)
             .menv (0.0f, 0.15f, 0.0f, 0.1f).mod (ModEnv, D::FmAmount, 0.4f)
             .amp (0.001f, 1.2f, 0.0f, 0.8f, -0.3f).velocity (0.5f)
             .macro (1, "TINE", { { D::FmAmount, 0.3f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.4f } })
             .macro (3, "WOOD", { { D::MeDecay, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f));

    add (B ("Bitwise Arp", "Arp")
             .osc1 (Bitwise, 0.3f, 0.7f).osc2 (Stepped, 0.5f, 0.35f, 12)
             .arp (ArpUpDown, D1_16, 2, 0.4f)
             .lfo (1, Drunk, 2.0f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .filter1 (LP, 3500.0f, 0.3f, 1.0f, 1.3f, 0.3f)
             .fenv (0.001f, 0.2f, 0.1f, 0.15f).amp (0.001f, 0.25f, 0.3f, 0.2f)
             .macro (1, "BITS", { { D::Osc1Frame, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "WANDER", { { D::Lfo1Rate, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.45f, 0.2f, true).reverb (Plate, 0.5f, 0.15f));

    add (B ("Throat Drone", "Drone")
             .osc1 (Throat, 0.3f, 0.6f).unison (1, 3, 6.0f, 0.7f).osc2 (Breath, 0.5f, 0.3f, -12)
             .spectral (2, SpFormant, 0.35f)
             .lfo (1, SmoothRandom, 0.1f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .lfo (2, Chaos, 0.05f).mod (Lfo2, D::Filter1Cutoff, 0.2f)
             .filter1 (LP, 2500.0f, 0.3f)
             .amp (2.0f, 2.0f, 0.9f, 4.0f)
             .macro (1, "VOWEL", { { D::Osc1Frame, 0.4f } })
             .macro (2, "BREATH", { { D::Osc2Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    return list;
}
} // namespace Library
} // namespace Presets
