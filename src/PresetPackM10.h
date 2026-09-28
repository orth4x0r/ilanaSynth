#pragma once

// M10: the v1.3 preset pack, appended after every earlier preset (their
// indices never move). Each one leans on a v1.2/v1.3 feature and maps all
// four macros. Included from PresetLibrary.h inside Presets::Library.

// Exciters, bodies, LFO simulations and the new filters, by index.
enum Excite { XBurst = 0, XNoise, XSaw, XPulse, XBow, XHammer, XOscIn, XTine, XReed, XPiano, XFeedback };
enum Body { BClassic = 0, BBar, BPlate, BBell, BShell };
enum SimShape { SimRandomHold = 16, SimSineRandom, SimPerlin, SimDrunkWalk, SimLorenz, SimRossler, SimDuffing, SimLogistic,
                SimHenon, SimDoublePendulum, SimBounce, SimPendulum, SimSpring, SimFriction };
enum Filter2 { LadderBP = 12, LadderDrive, Sem, OtaLP, OtaBP, Ms20HP, Steiner, PhaserNotch, CombDamped, CombMorph,
               VowelBank, Talking, TwinPeak };
enum Warp2 { W2Off = 0, W2Sync, W2BendPlus, W2BendMinus, W2Pwm, W2Mirror, W2Asym, W2Quantize, W2PdSaw, W2PdSquare,
             W2PdPulse, W2PdRes1, W2PdRes2, W2PdRes3 };
enum WarpPd { WPdSaw = 10, WPdSquare, WPdPulse, WPdRes1, WPdRes2, WPdRes3 };
// v1.3 factory tables (TableFactory, 40-119).
enum Table13
{
    SineToSaw = 40, TriToSquare, PulseSweep, HarmonicStack, SoftSquareT, SawToSquare,
    LadderSawT, LadderSquareT, AcidRamp, SkewTriangle, RoundedPwm, SoftSync, RcSaw, AnalogPulse, ResonantTri, OctavePulse,
    Fm11, Fm12, Fm13, Fm21, FmStack, CzSaw, CzSquare, CzReso, WaveScan, BitRamp,
    VowelsMale, VowelsFemale, VowelsChild, Whisper, Nasal, Yeah, Diphthong,
    PrimePartials, OddTilt, MovingComb, OctavePartials, FormantLadder, HarmonicBands, MirrorSpectrum,
    FoldSweep, FoldBias, SquareSync, RingSine, DrivenTri, Wavecrush, Grind,
    PluckStiff, Bowed, ReedT, BrassT, FluteT, MarimbaT, BellT, HollowWood,
    HenonT, RandomWalk, RosslerT, ShapedNoise, TentMap, DuffingT,
    TineT, ReedPiano, JazzOrgan, FullOrgan, ClavT, Harpsichord, ToyPiano,
    WarmSub, ReeseT, GrowlT, WobbleT, Kick808, NeuroT, DeepPulse,
    GlassHarmonics, AirPad, ShimmerStack, WarmPad, PadChoir
};

inline int src (Mod::Source source) { return (int) source; }

// An LFO running one of the M8.1 simulations, with its named knobs p1..p4.
inline Builder& sim (Builder& b, int lfo, int shape, float rate, std::initializer_list<float> knobs = {}, float smooth = 0.2f)
{
    const auto p = "lfo" + std::to_string (lfo);
    b.lfo (lfo, shape, rate);
    auto k = 1;
    for (auto value : knobs)
        b.set (p + "_p" + std::to_string (k++), value);
    return b.set (p + "_smooth", smooth);
}

// A Deep FM operator: oscillator index (1-6) on the Sine table at a ratio
// of the note; out = false makes it a pure modulator.
inline Builder& op (Builder& b, int index, float ratio, float level, bool out = true)
{
    const std::string p = OscillatorIds::prefixes[(size_t) juce::jlimit (1, 6, index) - 1];
    b.osc (index, SineT, 0.0f, level);
    b.set (p + "_tune", 1).set (p + "_ratio", ratio);
    return b.set (p + "_out", out ? 1.0f : 0.0f);
}

// An FM matrix cell from oscillator a to b (1-6; a == b is feedback).
inline Builder& fmCell (Builder& b, int from, int to, float amount)
{
    static const char* legacy[3][3] { { "fm_feedback", "fm_1to2", "fm_1to3" },
                                      { "fm_amount", "fm_fb2", "fm_2to3" },
                                      { "fm_3to1", "fm_3to2", "fm_fb3" } };
    if (from <= 3 && to <= 3)
        return b.set (legacy[from - 1][to - 1], amount);
    return b.set (from == to ? "fm_fb" + std::to_string (from) : "fm_" + std::to_string (from) + "to" + std::to_string (to), amount);
}

inline void addM10Presets (const std::function<void (const FactoryPreset&)>& add)
{
    using B = Builder;

    // ======================================================================
    // Strings (M1/M3): 10
    // ======================================================================
    add (B ("Nylon Twelve", "Pluck")
             .string (1, 0.8f, XBurst, 0.72f, 0.45f).unison (1, 2, 6.0f, 0.5f)
             .set ("osc1_string_pick_hardness", 0.45f).set ("osc1_string_pick_pos", 0.18f).set ("osc1_couple", 0.3f)
             .string (2, 0.45f, XBurst, 0.66f, 0.4f).set ("osc2_semi", 12).set ("osc2_string_pick_hardness", 0.6f)
             .amp (0.001f, 3.0f, 1.0f, 0.4f).filter1 (LP, 9000.0f, 0.0f)
             .macro (1, "BRIGHT", { { param ("osc1_string_damp"), -0.3f }, { param ("osc2_string_damp"), -0.3f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, -0.45f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.2f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.25f, 0.3f).reverb (Room, 0.45f, 0.18f).master (2.0f));

    add (B ("Cello Section", "Pad")
             .string (1, 0.75f, XBow, 0.8f, 0.4f, 0.9f).unison (1, 3, 9.0f, 0.7f)
             .set ("osc1_bow_pressure", 0.55f).set ("osc1_bow_speed", 0.45f)
             .set ("res_on", 1).set ("body_type", BShell).set ("body_material", 0.3f).set ("res_amount", 0.5f)
             .amp (0.35f, 0.5f, 1.0f, 0.9f).filter1 (LP, 5200.0f, 0.1f).set ("osc1_semi", -12)
             .lfo (1, Sine, 5.2f, true).mod (Lfo1, D::Osc1Pitch, 0.012f, 0.0f, Wheel)
             .macro (1, "PRESSURE", { { param ("osc1_bow_pressure"), 0.35f } })
             .macro (2, "BODY", { { D::ResAmount, 0.4f } })
             .macro (3, "VIBRATO", { { D::Lfo1Rate, 0.2f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxEq, FxReverb }).eq (2.0f, 700.0f, -2.0f, -1.0f).reverb (Hall, 0.7f, 0.25f));

    add (B ("Harp Cascade", "Pluck")
             .string (1, 0.8f, XBurst, 0.8f, 0.25f).set ("osc1_string_pick_hardness", 0.3f).set ("osc1_string_pick_pos", 0.3f)
             .amp (0.001f, 4.0f, 1.0f, 1.2f)
             .scale (Lydian, C).spray (4, 12, SprayUp, 70.0f, 1.0f, 0.2f)
             .macro (1, "SPRAY", { { param ("spray_spread"), 0.4f } })
             .macro (2, "TONE", { { param ("osc1_string_damp"), -0.25f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.15f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.15f, true).reverb (Hall, 0.7f, 0.28f));

    add (B ("Viola Drone", "Drone")
             .string (1, 0.7f, XBow, 0.85f, 0.35f, 1.0f).set ("osc1_bow_pressure", 0.4f).set ("osc1_bow_speed", 0.6f)
             .string (2, 0.45f, XBow, 0.85f, 0.35f, 1.0f).set ("osc2_semi", 7).set ("osc2_bow_pressure", 0.35f)
             .set ("sym_on", 1).set ("sym_amount", 0.6f).set ("sym_count", 6).set ("sym_decay", 0.7f)
             .amp (1.2f, 0.5f, 1.0f, 2.5f).filter1 (LP, 4500.0f, 0.1f)
             .macro (1, "PRESSURE", { { param ("osc1_bow_pressure"), 0.4f }, { param ("osc2_bow_pressure"), 0.4f } })
             .macro (2, "FIFTH", { { D::Osc2Level, -0.45f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f));

    add (B ("Detuned Zither", "Pluck")
             .string (1, 0.7f, XBurst, 0.82f, 0.3f).unison (1, 3, 14.0f, 0.8f).set ("osc1_couple", 0.5f)
             .set ("osc1_string_stiffness", 0.3f).set ("osc1_string_pick_pos", 0.08f)
             .amp (0.001f, 5.0f, 1.0f, 1.5f)
             .macro (1, "DETUNE", { { D::Osc1Detune, 0.35f } })
             .macro (2, "STIFF", { { param ("osc1_string_stiffness"), 0.4f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.15f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Plate, 0.5f, 0.2f));

    add (B ("Slide Fiddle", "Lead")
             .string (1, 0.8f, XBow, 0.8f, 0.3f, 1.0f).set ("osc1_bow_pressure", 0.6f).set ("osc1_bow_speed", 0.55f)
             .legato (0.12f).bend (12.0f)
             .amp (0.06f, 0.4f, 1.0f, 0.3f).filter1 (LP, 7000.0f, 0.1f)
             .lfo (1, Sine, 5.8f, true).mod (Lfo1, D::Osc1Pitch, 0.015f, 0.0f, Wheel)
             .macro (1, "PRESSURE", { { param ("osc1_bow_pressure"), 0.35f } })
             .macro (2, "GLIDE", { { param ("glide"), 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxReverb }).ampSim (2, 0.3f, 0.5f, 0.6f, 0.5f, 0.6f).reverb (Room, 0.4f, 0.15f).master (-2.0f));

    add (B ("Muted Cello Pizz", "Pluck")
             .string (1, 0.85f, XBurst, 0.35f, 0.7f).set ("osc1_string_pick_hardness", 0.2f).set ("osc1_string_pick_pos", 0.5f)
             .set ("res_on", 1).set ("body_type", BShell).set ("body_material", 0.2f).set ("res_amount", 0.55f)
             .amp (0.001f, 0.8f, 0.0f, 0.3f).set ("osc1_semi", -12)
             .macro (1, "MUTE", { { param ("osc1_string_decay"), -0.25f } })
             .macro (2, "BODY", { { D::ResAmount, 0.35f } })
             .macro (3, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.35f, 0.15f).master (6.0f));

    add (B ("Baroque Lute", "Pluck")
             .string (1, 0.75f, XBurst, 0.62f, 0.5f).unison (1, 2, 3.0f, 0.4f).set ("osc1_couple", 0.4f)
             .set ("osc1_string_pick_pos", 0.12f).set ("osc1_string_pick_hardness", 0.5f)
             .set ("res_on", 1).set ("body_type", BShell).set ("body_material", 0.55f).set ("res_amount", 0.45f)
             .amp (0.001f, 2.5f, 1.0f, 0.6f)
             .macro (1, "BODY", { { D::ResAmount, 0.4f } })
             .macro (2, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.2f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f).master (6.0f));

    add (B ("Contrabass Growl", "Bass")
             .string (1, 0.9f, XBurst, 0.55f, 0.6f).set ("osc1_string_pick_hardness", 0.35f).set ("osc1_damper", 0.8f)
             .set ("osc1_bridge_buzz", 0.35f).set ("osc1_semi", -12)
             .amp (0.001f, 1.5f, 0.6f, 0.2f).filter1 (LP, 2500.0f, 0.1f).mono (0.02f)
             .macro (1, "BUZZ", { { param ("osc1_bridge_buzz"), 0.45f } })
             .macro (2, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.25f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxEq }).driveFx (1.5f, 0.25f).eq (2.0f, 300.0f, -2.0f, -2.0f).master (10.0f));

    add (B ("String Ensemble Wide", "Pad")
             .string (1, 0.65f, XBow, 0.85f, 0.35f, 1.0f).unison (1, 6, 12.0f, 1.0f).set ("osc1_bow_pressure", 0.45f)
             .string (2, 0.4f, XBow, 0.85f, 0.4f, 1.0f).set ("osc2_semi", 12).unison (2, 4, 10.0f, 1.0f)
             .amp (0.6f, 0.5f, 1.0f, 1.4f).filter1 (LP, 6000.0f, 0.1f)
             .macro (1, "SWELL", { { D::AmpAttack, 0.4f } })
             .macro (2, "OCTAVE", { { D::Osc2Level, -0.4f } })
             .macro (3, "WIDTH", { { D::Osc1Spread, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.3f));

    // ======================================================================
    // Oscillator engine layers (M3b, six oscillators): 8
    // ======================================================================
    add (B ("Six-Layer Stack", "Lead")
             .osc1 (SineToSaw, 0.8f, 0.5f).osc2 (LadderSawT, 0.7f, 0.45f, 0, 7.0f).osc3 (SawToSquare, 0.5f, 0.4f, -12)
             .osc (4, RcSaw, 0.6f, 0.4f, 12, -6.0f).osc (5, WarmPad, 0.3f, 0.35f, 7).osc (6, SineT, 0.0f, 0.4f, -24)
             .filter1 (LadderLP, 3200.0f, 0.3f, 1.2f, 1.5f, 0.3f, true).fenv (0.002f, 0.4f, 0.4f, 0.4f)
             .amp (0.004f, 0.4f, 0.9f, 0.35f).voices (8)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "UPPER", { { D::Osc4Level, 0.4f }, { D::Osc5Level, 0.4f } })
             .macro (3, "SUB", { { D::Osc6Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.3f, 0.25f).reverb (Plate, 0.5f, 0.15f));

    add (B ("Fifth Engine", "Lead")
             .osc1 (AnalogPulse, 0.3f, 0.6f).osc2 (AnalogPulse, 0.3f, 0.5f, 7)
             .osc (4, SoftSync, 0.4f, 0.4f, 12).osc (5, SoftSync, 0.4f, 0.35f, 19).set ("osc2_on", 1)
             .filter1 (OtaLP, 2600.0f, 0.35f, 1.5f, 1.3f, 0.4f).fenv (0.002f, 0.35f, 0.3f, 0.3f)
             .amp (0.003f, 0.3f, 0.9f, 0.25f).mono (0.04f)
             .lfo (1, Tri, 0.3f).mod (Lfo1, D::Osc4Frame, 0.4f).mod (Lfo1, D::Osc5Frame, -0.4f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "SYNC", { { D::Osc4Frame, 0.4f }, { D::Osc5Frame, 0.4f } })
             .macro (3, "WIDTH", { { D::Osc1Frame, 0.5f }, { D::Osc2Frame, 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.3f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.18f, true).reverb (Room, 0.4f, 0.12f));

    add (B ("Route Split Pad", "Pad")
             .osc1 (WarmPad, 0.4f, 0.6f).osc2 (AirPad, 0.5f, 0.45f, 12).osc (4, PadChoir, 0.3f, 0.4f, 7)
             .route (1, 1).route (2, 2).route (4, 3)
             .filter1 (Sem, 1800.0f, 0.25f).morph (1, 0.1f).filter2 (HP, 900.0f, 0.2f)
             .amp (0.8f, 0.5f, 1.0f, 1.6f)
             .lfo (1, Sine, 0.12f).mod (Lfo1, D::Filter1Cutoff, 0.15f)
             .macro (1, "WARMTH", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "AIR", { { D::Osc2Level, 0.35f }, { D::Filter2Cutoff, -0.2f } })
             .macro (3, "CHOIR", { { D::Osc4Level, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.4f, 0.35f).reverb (Hall, 0.85f, 0.32f));

    add (B ("Chorded Sixpack", "Chords")
             .osc1 (LadderSquareT, 0.5f, 0.6f).chord (1, 5).osc (4, RcSaw, 0.5f, 0.35f, 12).chord (4, 5)
             .osc (5, FluteT, 0.5f, 0.3f, 24).osc (6, WarmSub, 0.3f, 0.35f, -12)
             .filter1 (LadderLP, 2400.0f, 0.2f, 1.5f, 1.2f, 0.2f, true).fenv (0.002f, 0.5f, 0.2f, 0.4f)
             .amp (0.002f, 0.8f, 0.6f, 0.5f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "TOP", { { D::Osc5Level, 0.4f } })
             .macro (3, "BOTTOM", { { D::Osc6Level, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.4f, 0.3f, 0.25f).reverb (Room, 0.5f, 0.18f));

    add (B ("Layered Sync Choir", "Pad")
             .osc1 (VowelsFemale, 0.2f, 0.55f).osc2 (VowelsMale, 0.6f, 0.45f, -12).osc (4, SoftSync, 0.3f, 0.3f, 12)
             .warp (4, WSync, 0.3f)
             .filter1 (VowelBank, 1200.0f, 0.4f).morph (1, 0.2f).amp (0.5f, 0.4f, 1.0f, 1.2f)
             .lfo (1, Sine, 0.08f).mod (Lfo1, D::Osc1Frame, 0.4f).mod (Lfo1, D::Filter1Morph, 0.3f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.5f } })
             .macro (2, "SYNC", { { D::Osc4Warp, 0.4f } })
             .macro (3, "MOVE", { { D::Lfo1Rate, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.25f, 0.35f, 0.3f).reverb (Hall, 0.8f, 0.3f));

    add (B ("Panned Engine Sweep", "Pad")
             .osc1 (HarmonicBands, 0.0f, 0.5f).pan (1, -0.8f).osc2 (HarmonicBands, 0.3f, 0.5f).pan (2, 0.8f)
             .osc (4, MovingComb, 0.0f, 0.4f, 12).pan (4, -0.4f).osc (5, MovingComb, 0.5f, 0.4f, 12).pan (5, 0.4f)
             .filter1 (LP, 7000.0f, 0.1f).amp (0.6f, 0.5f, 1.0f, 1.5f)
             .lfo (1, Tri, 0.07f).lfo (2, Tri, 0.05f, false, 0.5f)
             .mod (Lfo1, D::Osc1Frame, 0.5f).mod (Lfo2, D::Osc2Frame, 0.5f).mod (Lfo1, D::Osc4Frame, 0.5f).mod (Lfo2, D::Osc5Frame, -0.5f)
             .macro (1, "SPEED", { { D::Lfo1Rate, 0.4f }, { D::Lfo2Rate, 0.4f } })
             .macro (2, "UPPER", { { D::Osc4Level, 0.3f }, { D::Osc5Level, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f).master (-10.0f));

    add (B ("Modulated Route Bass", "Bass")
             .osc1 (ReeseT, 0.3f, 0.7f).osc2 (GrowlT, 0.2f, 0.5f).osc (6, WarmSub, 0.2f, 0.6f, -12)
             .route (1, 1).route (2, 2).route (6, 0)
             .filter1 (LadderDrive, 500.0f, 0.4f, 2.0f, 2.0f).filter2 (Steiner, 800.0f, 0.4f).morph (2, 0.3f)
             .fenv (0.001f, 0.3f, 0.2f, 0.2f).amp (0.002f, 0.4f, 0.9f, 0.15f).mono (0.03f)
             .lfoSync (1, Sine, D1_8).mod (Lfo1, D::Osc1Frame, 0.4f).mod (Lfo1, D::Filter2Cutoff, 0.3f, 0.0f, M2)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "WOBBLE", {})
             .macro (3, "GROWL", { { D::Osc2Frame, 0.5f } })
             .macro (4, "DRIVE", { { D::Filter1Drive, 0.4f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.3f).eq (2.0f, 400.0f, -2.0f, 0.0f, 80.0f));

    add (B ("Six-Voice Drift", "Pad")
             .osc1 (SoftSaw, 0.3f, 0.45f).osc2 (SoftSaw, 0.3f, 0.45f, 0, 9.0f).osc3 (SoftSaw, 0.3f, 0.4f, 12, -7.0f)
             .osc (4, SoftSaw, 0.3f, 0.35f, 7, 4.0f).osc (5, SoftSaw, 0.3f, 0.3f, 19, -5.0f).osc (6, SineT, 0.0f, 0.4f, -12)
             .drift (0.6f).filter1 (LadderLP, 2400.0f, 0.2f, 0.0f, 1.2f, 0.2f, true).amp (0.9f, 0.5f, 1.0f, 1.8f)
             .macro (1, "DRIFT", { { D::Drift, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (3, "UPPER", { { D::Osc4Level, 0.3f }, { D::Osc5Level, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.4f, 0.3f).reverb (Hall, 0.85f, 0.3f));

    // ======================================================================
    // Chaos and physics modulators (M8.1, M2): 8
    // ======================================================================
    add (sim (B ("Lorenz Wash", "Pad")
                  .osc1 (ShimmerStack, 0.5f, 0.55f).osc2 (AirPad, 0.4f, 0.4f, 12)
                  .filter1 (Sem, 1500.0f, 0.3f).amp (1.0f, 0.5f, 1.0f, 2.0f), 1, SimLorenz, 0.3f, { 0.5f, 0.5f, 0.5f })
             .mod (Lfo1, D::Filter1Cutoff, 0.35f).mod (src (Mod::Source::Lfo1B), D::Osc1Frame, 0.4f)
             .macro (1, "CHAOS", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "DEPTH", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Morph, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.35f));

    add (sim (B ("Pendulum Swing Pad", "Pad")
                  .osc1 (WarmPad, 0.3f, 0.55f).osc2 (PadChoir, 0.5f, 0.4f, 12)
                  .filter1 (LadderLP, 2000.0f, 0.25f, 0.0f, 1.2f, 0.2f, true).amp (0.8f, 0.5f, 1.0f, 1.6f),
              1, SimPendulum, 0.4f, { 0.6f, 0.2f })
             .mod (Lfo1, D::Filter1Cutoff, 0.3f).mod (Lfo1, D::Pan, 0.3f)
             .macro (1, "SWING", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "DEPTH", { { D::Filter1Cutoff, 0.3f } })
             .macro (3, "CHOIR", { { D::Osc2Level, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f));

    add (sim (B ("Spring Bounce Bass", "Bass")
                  .osc1 (WobbleT, 0.2f, 0.75f).sub (0, 0.4f)
                  .filter1 (LadderLP, 600.0f, 0.4f, 1.5f, 1.5f, 0.2f, true).fenv (0.001f, 0.2f, 0.2f, 0.15f)
                  .amp (0.002f, 0.4f, 0.9f, 0.15f).mono (0.02f), 1, SimSpring, 1.2f, { 0.5f, 0.3f })
             .set ("lfo1_trigger", 0)
             .mod (Lfo1, D::Filter1Cutoff, 0.35f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .macro (1, "SPRING", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "SHAPE", { { D::Osc1Frame, 0.4f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive }).driveFx (2.0f, 0.3f));

    add (sim (B ("Double Pendulum Chaos", "Drone")
                  .osc1 (RosslerT, 0.3f, 0.5f).osc2 (DuffingT, 0.5f, 0.4f, 7)
                  .filter1 (TwinPeak, 900.0f, 0.5f).morph (1, 0.4f).amp (1.5f, 0.5f, 1.0f, 2.5f),
              1, SimDoublePendulum, 0.35f, { 0.7f, 0.5f })
             .mod (Lfo1, D::Filter1Cutoff, 0.35f).mod (src (Mod::Source::Lfo1B), D::Filter1Morph, 0.35f)
             .mod (Lfo1, D::Osc1Frame, 0.4f)
             .macro (1, "CHAOS", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "PEAKS", { { D::Filter1Morph, 0.4f } })
             .macro (3, "GROWL", { { D::Filter1Reso, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.35f));

    add (sim (B ("Henon Glitch", "FX")
                  .osc1 (BitRamp, 0.3f, 0.6f).osc2 (Wavecrush, 0.5f, 0.4f, 12)
                  .filter1 (Ms20LP, 2500.0f, 0.5f, 0.0f, 2.0f).amp (0.001f, 0.3f, 0.8f, 0.2f),
              1, SimHenon, 6.0f, { 0.6f, 0.5f }, 0.0f)
             .mod (Lfo1, D::Osc1Pitch, 0.25f).mod (Lfo1, D::Filter1Cutoff, 0.4f).mod (src (Mod::Source::Lfo1B), D::Osc2Frame, 0.5f)
             .macro (1, "RATE", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "PITCH", { { D::Osc1Pitch, 0.2f } })
             .macro (3, "CRUSH", { { D::Osc2Frame, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_16, 0.45f, 0.25f, true).reverb (Room, 0.4f, 0.15f));

    add (sim (B ("Rossler Tide", "Drone")
                  .osc1 (MirrorSpectrum, 0.3f, 0.5f).osc2 (OctavePartials, 0.5f, 0.4f, -12)
                  .filter1 (PhaserNotch, 1200.0f, 0.4f).morph (1, 0.5f).amp (2.0f, 0.5f, 1.0f, 3.0f),
              1, SimRossler, 0.15f, { 0.5f, 0.5f })
             .mod (Lfo1, D::Filter1Cutoff, 0.4f).mod (src (Mod::Source::Lfo1B), D::Osc1Frame, 0.4f)
             .macro (1, "TIDE", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "NOTCH", { { D::Filter1Morph, 0.4f } })
             .macro (3, "SPECTRUM", { { D::Osc1Frame, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));

    add (sim (B ("Stick-Slip Growl", "Bass")
                  .osc1 (GrowlT, 0.3f, 0.75f).sub (0, 0.35f)
                  .filter1 (OtaLP, 900.0f, 0.4f, 0.0f, 2.0f).amp (0.002f, 0.4f, 0.9f, 0.15f).mono (0.02f),
              1, SimFriction, 2.5f, { 0.6f, 0.4f })
             .mod (Lfo1, D::Osc1Frame, 0.5f).mod (Lfo1, D::Filter1Cutoff, 0.3f)
             .macro (1, "FRICTION", { { D::Lfo1Rate, 0.4f } })
             .macro (2, "GROWL", { { D::Osc1Frame, 0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive }).driveFx (2.5f, 0.35f));

    add (sim (B ("Bouncing Ball FX", "FX")
                  .osc1 (MarimbaT, 0.4f, 0.7f).filter1 (LP, 6000.0f, 0.1f).amp (0.001f, 0.5f, 1.0f, 0.3f),
              1, SimBounce, 1.0f, { 0.5f, 0.7f }, 0.0f)
             .set ("west_on", 1).set ("west_src", (float) src (Mod::Source::Lfo1B)).set ("west_fold", 0.2f).set ("west_decay", 0.6f)
             .mod (Lfo1, D::Osc1Pitch, 0.1f)
             .macro (1, "GRAVITY", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "FOLD", { { param ("west_fold"), 0.4f } })
             .macro (3, "DECAY", { { param ("west_decay"), 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.3f, 0.2f, true).reverb (Room, 0.4f, 0.15f).master (0.0f));

    // ======================================================================
    // Acoustic keys (M4, M8.2): 8
    // ======================================================================
    const auto grand = [] (const char* name, float hardness, float decay, float damp, float tone, float board, float pedal)
    {
        return B (name, "Keys")
            .piano (1, 0.8f, hardness, 3, 0.0f, 0.2946f, 0.8f, 1.0f, decay, damp, 0.5f)
            .set ("osc1_excite", XPiano).set ("osc2_on", 0).set ("sub_on", 0)
            .keysBody (0.8f, board, tone, 0.5f, pedal, 0.2f).set ("sb_model", 1)
            .filter1 (LP, 20000.0f, 0.0f).amp (0.001f, 5.0f, 1.0f, 1.0f).velocity (0.0f);
    };
    add (grand ("Upright Piano", 0.4f, 0.4f, 0.6f, 0.3f, 0.8f, 0.3f)
             .set ("sb_size", 0.3f)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "PEDAL", { { param ("pedal_res"), 0.45f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxEq, FxReverb }).eq (1.0f, 900.0f, 1.5f, -2.0f).reverb (Room, 0.4f, 0.14f));
    add (grand ("Bright Concert Grand", 0.75f, 0.55f, 0.4f, 0.05f, 1.0f, 0.45f)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "PEDAL", { { param ("pedal_res"), 0.45f } })
             .macro (3, "LID", { { param ("sb_tone"), 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxLimiter, FxReverb }).limiter (-0.5f).reverb (Hall, 0.7f, 0.2f));
    add (grand ("Pedal Sustain Bloom", 0.45f, 0.6f, 0.45f, 0.2f, 1.0f, 0.9f)
             .set ("mech_pedal", 0.4f)
             .macro (1, "BLOOM", { { param ("pedal_res"), 0.3f } })
             .macro (2, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.25f));
    add (grand ("Detuned Saloon", 0.65f, 0.45f, 0.4f, 0.2f, 0.9f, 0.3f)
             .set ("osc1_detune", 14.0f).set ("osc1_couple", 0.5f)
             .macro (1, "DETUNE", { { D::Osc1Detune, 0.35f } })
             .macro (2, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (3, "HONKY", { { D::Filter1Cutoff, -0.2f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxEq, FxReverb }).eq (-1.0f, 1400.0f, 3.0f, -1.0f).reverb (Room, 0.35f, 0.15f).master (-2.0f));
    add (grand ("Prepared Piano Bells", 0.8f, 0.5f, 0.35f, 0.1f, 0.8f, 0.2f)
             .set ("osc1_string_stiffness", 0.9f).set ("osc1_register", 0.3f)
             .set ("res_on", 1).set ("body_type", BBell).set ("body_material", 0.7f).set ("res_amount", 0.4f)
             .macro (1, "PREPARE", { { param ("osc1_string_stiffness"), -0.4f } })
             .macro (2, "BELL", { { D::ResAmount, 0.4f } })
             .macro (3, "HAMMER", { { param ("osc1_hammer_hard"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Plate, 0.5f, 0.2f).master (-2.0f));
    add (grand ("Soft Felt Piano", 0.1f, 0.45f, 0.75f, 0.55f, 0.9f, 0.5f)
             .set ("mech_key", 0.5f).set ("mech_damper", 0.5f)
             .macro (1, "FELT", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "NOISE", { { param ("mech_key"), 0.3f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxEq, FxReverb }).eq (1.0f, 2500.0f, -3.0f, -4.0f).reverb (Room, 0.5f, 0.22f));
    add (grand ("Toy Grand", 0.9f, 0.3f, 0.3f, 0.1f, 0.5f, 0.1f)
             .set ("osc1_register", 0.2f).set ("sb_size", 0.1f).set ("osc1_semi", 12)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.2f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (3, "BOARD", { { param ("sb_mix"), -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.3f, 0.15f).master (2.0f));
    add (grand ("Dense Board Piano", 0.5f, 0.55f, 0.45f, 0.1371f, 1.0f, 0.45f)
             .set ("sb_size", 0.8f)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "BOARD", { { param ("sb_mix"), -0.3f } })
             .macro (3, "SIZE", { { param ("sb_size"), -0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Hall, 0.6f, 0.18f).master (-2.0f));

    // ======================================================================
    // Electric pianos (M7.3): 6
    // ======================================================================
    const auto ep = [] (const char* name, int excite, float distance, float position, float damp)
    {
        return B (name, "Keys")
            .string (1, 0.8f, excite, 0.75f, damp).set ("osc2_on", 0).set ("sub_on", 0).set ("subosc_on", 0)
            .set ("osc1_damper", 0.85f).set ("osc1_ep_distance", distance).set ("osc1_ep_position", position)
            .filter1 (LP, 20000.0f, 0.0f).amp (0.001f, 10.0f, 1.0f, 0.4f).velocity (0.0f);
    };
    add (ep ("Vintage Tine", XTine, 0.55f, 0.5f, 0.45f)
             .macro (1, "BARK", { { param ("osc1_ep_distance"), -0.3f } })
             .macro (2, "TREMOLO", { { param ("fx_trem_depth"), 0.5f } })
             .macro (3, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxTremolo, FxReverb }).tremolo (4.5f, 0.3f).reverb (Room, 0.4f, 0.15f).master (-5.5f));
    add (ep ("Bark Rhodes", XTine, 0.2f, 0.65f, 0.3f)
             .set ("osc1_hammer_hard", 0.75f)
             .macro (1, "BARK", { { param ("osc1_ep_distance"), 0.3f } })
             .macro (2, "HAMMER", { { param ("osc1_hammer_hard"), 0.25f } })
             .macro (3, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .macro (4, "PHASE", { { param ("fx_phaser_mix"), 0.4f } })
             .fx ({ FxAmp, FxPhaser }).ampSim (0, 0.35f, 0.5f, 0.6f, 0.55f, 0.6f).phaser (0.4f, 0.5f, 0.3f, 0.25f).master (0.0f));
    add (ep ("Growl Reed", XReed, 0.25f, 0.6f, 0.3f)
             .set ("osc1_hammer_hard", 0.7f)
             .macro (1, "GROWL", { { param ("osc1_ep_distance"), 0.3f } })
             .macro (2, "HAMMER", { { param ("osc1_hammer_hard"), 0.25f } })
             .macro (3, "AMP", { { D::FxDriveAmount, 0.5f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxReverb }).ampSim (0, 0.4f, 0.5f, 0.65f, 0.5f, 0.55f).reverb (Room, 0.35f, 0.12f).master (3.0f));
    add (ep ("Chorused Tine", XTine, 0.5f, 0.45f, 0.5f)
             .macro (1, "CHORUS", { { D::FxChorusDepth, 0.4f } })
             .macro (2, "BARK", { { param ("osc1_ep_distance"), -0.3f } })
             .macro (3, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.6f, 0.45f, 0.5f).reverb (Plate, 0.45f, 0.18f).master (-5.5f));
    add (ep ("Distorted Tine Rock", XTine, 0.25f, 0.7f, 0.3f)
             .set ("osc1_hammer_hard", 0.8f)
             .macro (1, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .macro (2, "BARK", { { param ("osc1_ep_distance"), 0.3f } })
             .macro (3, "TONE", { { param ("fx_amp_treble"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxReverb }).ampSim (1, 0.55f, 0.55f, 0.6f, 0.6f, 0.45f).reverb (Room, 0.35f, 0.12f).master (-8.0f));
    add (ep ("Mellow Reed Bell", XReed, 0.7f, 0.35f, 0.6f)
             .set ("res_on", 1).set ("body_type", BBell).set ("body_material", 0.4f).set ("res_amount", 0.3f)
             .macro (1, "BELL", { { D::ResAmount, 0.35f } })
             .macro (2, "GROWL", { { param ("osc1_ep_distance"), -0.3f } })
             .macro (3, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Plate, 0.5f, 0.2f).master (3.0f));

    // ======================================================================
    // Deep FM (M5): 12
    // ======================================================================
    {
        auto b = B ("DX Stack Bass", "Bass");
        op (b, 1, 1.0f, 0.8f); op (b, 2, 1.0f, 0.6f, false); op (b, 3, 2.0f, 0.5f, false);
        fmCell (b, 2, 1, 0.45f); fmCell (b, 3, 2, 0.35f); fmCell (b, 1, 1, 0.15f);
        add (b.set ("osc2_amp_env", 3).menv (0.001f, 0.25f, 0.1f, 0.2f).set ("sub_on", 0)
                 .filter1 (LP, 6000.0f, 0.1f).amp (0.001f, 0.6f, 0.8f, 0.12f).mono (0.02f)
                 .mod (ModEnv, D::FmAmount, 0.3f)
                 .macro (1, "INDEX", { { D::FmAmount, 0.4f } })
                 .macro (2, "BITE", { { D::Fm3to2, 0.4f } })
                 .macro (3, "FEEDBACK", { { D::FmFeedback, 0.3f } })
                 .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
                 .fx ({ FxDrive, FxEq }).driveFx (1.5f, 0.2f).eq (2.0f, 400.0f, -1.0f, 0.0f, 80.0f));
    }
    {
        auto b = B ("DX Bell Pad", "Pad");
        op (b, 1, 1.0f, 0.6f); op (b, 2, 3.5f, 0.6f, false); op (b, 4, 1.0f, 0.5f); op (b, 5, 7.07f, 0.5f, false);
        fmCell (b, 2, 1, 0.35f); fmCell (b, 5, 4, 0.3f);
        add (b.set ("osc2_ratio_snap", 0).set ("osc5_ratio_snap", 0).set ("sub_on", 0).set ("osc4_fine", 6.0f)
                 .amp (0.4f, 1.5f, 0.6f, 2.0f).menv (0.3f, 2.0f, 0.2f, 1.5f).mod (ModEnv, D::FmAmount, 0.25f)
                 .macro (1, "BELL", { { D::FmAmount, 0.4f } })
                 .macro (2, "SHIMMER", { { param ("fm_5to4"), 0.3f } })
                 .macro (3, "SWELL", { { D::AmpAttack, 0.4f } })
                 .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
                 .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.85f, 0.3f).master (-10.0f));
    }
    {
        auto b = B ("Diamond Growl", "Bass");
        op (b, 1, 1.0f, 0.7f); op (b, 2, 1.0f, 0.5f, false); op (b, 3, 2.0f, 0.5f, false); op (b, 4, 3.0f, 0.4f, false);
        fmCell (b, 2, 1, 0.4f); fmCell (b, 3, 1, 0.3f); fmCell (b, 4, 2, 0.3f); fmCell (b, 4, 3, 0.3f);
        add (b.set ("sub_on", 0).set ("osc2_fb_type", 1).set ("fm_fb2", 0.4f)
                 .filter1 (LadderDrive, 900.0f, 0.3f, 1.5f, 2.5f).fenv (0.001f, 0.3f, 0.3f, 0.2f)
                 .amp (0.002f, 0.4f, 0.9f, 0.15f).mono (0.03f)
                 .lfoSync (1, Sine, D1_8).mod (Lfo1, D::FmAmount, 0.3f, 0.0f, M2)
                 .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
                 .macro (2, "WOBBLE", {})
                 .macro (3, "GROWL", { { D::FmAmount, 0.35f }, { param ("fm_4to2"), 0.3f } })
                 .macro (4, "DRIVE", { { D::Filter1Drive, 0.4f } })
                 .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.3f).eq (2.0f, 400.0f, -2.0f, 0.0f, 80.0f));
    }
    {
        auto b = B ("Two Pairs Keys", "Keys");
        op (b, 1, 1.0f, 0.6f); op (b, 2, 14.0f, 0.4f, false); op (b, 4, 1.0f, 0.5f); op (b, 5, 1.0f, 0.5f, false);
        fmCell (b, 2, 1, 0.25f); fmCell (b, 5, 4, 0.3f);
        add (b.set ("sub_on", 0).set ("osc4_fine", 5.0f).set ("osc1_key_level", -0.3f)
                 .set ("osc2_amp_env", 3).menv (0.001f, 0.35f, 0.0f, 0.3f)
                 .amp (0.001f, 2.5f, 0.3f, 0.6f)
                 .macro (1, "TINE", { { D::FmAmount, 0.35f } })
                 .macro (2, "BODY", { { param ("fm_5to4"), 0.35f } })
                 .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
                 .macro (4, "CHORUS", { { D::FxChorusDepth, 0.4f } })
                 .fx ({ FxChorus, FxReverb }).chorus (0.5f, 0.3f, 0.35f).reverb (Room, 0.4f, 0.15f));
    }
    {
        auto b = B ("Three Carrier Choir", "Pad");
        op (b, 1, 1.0f, 0.45f); op (b, 2, 2.0f, 0.4f); op (b, 3, 3.0f, 0.35f); op (b, 4, 1.0f, 0.5f, false);
        fmCell (b, 4, 1, 0.3f); fmCell (b, 4, 2, 0.25f); fmCell (b, 4, 3, 0.2f);
        add (b.set ("sub_on", 0).set ("osc4_fine", 3.0f)
                 .filter1 (VowelBank, 1200.0f, 0.3f).morph (1, 0.3f).amp (0.6f, 0.5f, 1.0f, 1.5f)
                 .lfo (1, Sine, 0.1f).mod (Lfo1, D::Filter1Morph, 0.35f)
                 .macro (1, "VOWEL", { { D::Filter1Morph, 0.45f } })
                 .macro (2, "INDEX", { { param ("fm_4to1"), 0.3f } })
                 .macro (3, "UPPER", { { D::Osc2Level, 0.3f }, { D::SubLevel, 0.3f } })
                 .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
                 .fx ({ FxChorus, FxReverb }).chorus (0.25f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.3f));
    }
    {
        auto b = B ("One Drives Three", "Lead");
        op (b, 1, 1.0f, 0.5f); op (b, 2, 1.0f, 0.45f); op (b, 3, 2.0f, 0.4f); op (b, 4, 1.0f, 0.5f, false);
        fmCell (b, 4, 1, 0.4f); fmCell (b, 4, 2, 0.3f); fmCell (b, 4, 3, 0.35f); fmCell (b, 4, 4, 0.2f);
        add (b.set ("sub_on", 0).set ("osc2_fine", 7.0f).set ("osc4_fb_type", 1)
                 .filter1 (LP, 7000.0f, 0.1f).amp (0.003f, 0.3f, 0.9f, 0.3f).mono (0.05f)
                 .lfo (1, Sine, 5.5f, true).mod (Lfo1, D::Osc1Pitch, 0.012f, 0.0f, Wheel)
                 .macro (1, "INDEX", { { param ("fm_4to1"), 0.4f }, { param ("fm_4to2"), 0.3f } })
                 .macro (2, "FEEDBACK", { { param ("fm_fb4"), 0.4f } })
                 .macro (3, "TONE", { { D::Filter1Cutoff, 0.35f } })
                 .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
                 .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.2f, true).reverb (Plate, 0.5f, 0.15f));
    }
    {
        auto b = B ("DX Keys Classic", "Keys");
        op (b, 1, 1.0f, 0.6f); op (b, 2, 1.0f, 0.5f, false); op (b, 3, 1.0f, 0.45f); op (b, 4, 14.0f, 0.4f, false);
        fmCell (b, 2, 1, 0.3f); fmCell (b, 4, 3, 0.35f); fmCell (b, 2, 2, 0.2f);
        add (b.set ("sub_on", 0).set ("sub_fine", 4.0f).set ("osc2_amp_env", 3).set ("osc4_amp_env", 3)
                 .menv (0.001f, 0.5f, 0.0f, 0.3f).set ("osc1_key_level", -0.25f)
                 .amp (0.001f, 3.0f, 0.2f, 0.5f)
                 .macro (1, "TINE", { { param ("fm_4to3"), 0.4f } })
                 .macro (2, "BODY", { { D::FmAmount, 0.35f } })
                 .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
                 .macro (4, "CHORUS", { { D::FxChorusDepth, 0.4f } })
                 .fx ({ FxChorus, FxReverb }).chorus (0.6f, 0.35f, 0.4f).reverb (Room, 0.4f, 0.15f));
    }
    {
        auto b = B ("Ratio Snap Bell", "Pluck");
        op (b, 1, 1.0f, 0.7f); op (b, 2, 2.76f, 0.5f, false); op (b, 4, 5.4f, 0.4f, false);
        fmCell (b, 2, 1, 0.4f); fmCell (b, 4, 1, 0.2f);
        add (b.set ("sub_on", 0).set ("osc2_ratio_snap", 3).set ("osc4_ratio_snap", 3)
                 .menv (0.001f, 1.5f, 0.0f, 1.0f).mod (ModEnv, D::FmAmount, 0.3f)
                 .amp (0.001f, 3.5f, 0.0f, 2.0f)
                 .macro (1, "STRIKE", { { D::FmAmount, 0.4f } })
                 .macro (2, "OVERTONE", { { param ("fm_4to1"), 0.3f } })
                 .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
                 .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
                 .fx ({ FxReverb }).reverb (Hall, 0.75f, 0.28f));
    }
    {
        auto b = B ("Fixed Freq Clang", "FX");
        op (b, 1, 1.0f, 0.7f); op (b, 2, 1.0f, 0.5f, false);
        fmCell (b, 2, 1, 0.5f);
        add (b.set ("osc2_tune", 2).set ("osc2_fixed_hz", 1330.0f).set ("sub_on", 0)
                 .menv (0.001f, 0.4f, 0.0f, 0.3f).mod (ModEnv, D::FmAmount, 0.3f)
                 .amp (0.001f, 1.2f, 0.0f, 0.8f)
                 .macro (1, "CLANG", { { D::FmAmount, 0.35f } })
                 .macro (2, "DECAY", { { D::AmpDecay, 0.3f } })
                 .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
                 .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
                 .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.3f, 0.2f, true).reverb (Plate, 0.6f, 0.25f));
    }
    {
        auto b = B ("Feedback Chain Bass", "Bass");
        op (b, 1, 1.0f, 0.8f); op (b, 2, 1.0f, 0.5f, false); op (b, 3, 1.0f, 0.5f, false);
        fmCell (b, 3, 2, 0.35f); fmCell (b, 2, 1, 0.4f); fmCell (b, 3, 3, 0.45f);
        add (b.set ("sub_fb_type", 1).set ("sub_on", 0)
                 .filter1 (LadderLP, 1600.0f, 0.2f, 1.2f, 1.5f, 0.2f, true).fenv (0.001f, 0.3f, 0.2f, 0.2f)
                 .amp (0.001f, 0.5f, 0.8f, 0.12f).mono (0.02f)
                 .macro (1, "FEEDBACK", { { D::Fm3Feedback, 0.4f } })
                 .macro (2, "INDEX", { { D::FmAmount, 0.35f } })
                 .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
                 .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
                 .fx ({ FxDrive }).driveFx (1.5f, 0.25f));
    }
    {
        auto b = B ("Six-Op Organ", "Keys");
        op (b, 1, 1.0f, 0.4f); op (b, 2, 2.0f, 0.35f); op (b, 3, 3.0f, 0.3f); op (b, 4, 4.0f, 0.25f);
        op (b, 5, 6.0f, 0.2f); op (b, 6, 0.5f, 0.35f);
        fmCell (b, 1, 1, 0.1f);
        add (b.set ("sub_on", 0).amp (0.005f, 0.2f, 1.0f, 0.08f)
                 .lfo (1, Sine, 6.5f).mod (Lfo1, D::Osc1Pitch, 0.004f)
                 .macro (1, "UPPER", { { D::Osc4Level, 0.3f }, { D::Osc5Level, 0.3f } })
                 .macro (2, "SUB", { { D::Osc6Level, 0.3f } })
                 .macro (3, "VIBRATO", { { param ("fx_chorus_mix"), 0.4f } })
                 .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
                 .fx ({ FxAmp, FxChorus, FxReverb }).ampSim (0, 0.3f, 0.5f, 0.55f, 0.5f, 0.6f).chorus (6.0f, 0.3f, 0.3f).reverb (Room, 0.4f, 0.15f));
    }
    {
        auto b = B ("Inharmonic Pluck", "Pluck");
        op (b, 1, 1.0f, 0.7f); op (b, 2, 1.41f, 0.5f, false); op (b, 3, 3.87f, 0.4f, false);
        fmCell (b, 2, 1, 0.4f); fmCell (b, 3, 2, 0.3f);
        add (b.set ("osc2_ratio_snap", 2).set ("sub_ratio_snap", 2).set ("sub_on", 0)
                 .menv (0.001f, 0.3f, 0.0f, 0.2f).mod (ModEnv, D::FmAmount, 0.35f)
                 .amp (0.001f, 1.0f, 0.0f, 0.5f).filter1 (LP, 9000.0f, 0.1f)
                 .macro (1, "METAL", { { D::FmAmount, 0.4f } })
                 .macro (2, "DEPTH", { { D::Fm3to2, 0.3f } })
                 .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
                 .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
                 .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.2f, true).reverb (Room, 0.4f, 0.15f));
    }

    // ======================================================================
    // Phase distortion (M6): 8
    // ======================================================================
    const auto pd = [] (const char* name, const char* category, int warp, float amount, int warp2, float amount2)
    {
        return B (name, category).osc1 (SineT, 0.0f, 0.8f).warp (1, warp, amount)
            .set ("osc1_warp2", (float) warp2).set ("osc1_warp2_amt", amount2)
            .set ("osc1_pd_env", 4).set ("osc1_pd_env_amt", 0.8f).set ("sub_on", 0);
    };
    add (pd ("PD Casio Lead", "Lead", WPdSaw, 0.3f, W2Off, 0.0f)
             .menv (0.005f, 0.6f, 0.4f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.25f).mono (0.04f)
             .lfo (1, Sine, 5.5f, true).mod (Lfo1, D::Osc1Pitch, 0.01f, 0.0f, Wheel)
             .macro (1, "DCW", { { D::Osc1Warp, 0.4f } })
             .macro (2, "ENV", { { param ("osc1_pd_env_amt"), -0.4f } })
             .macro (3, "DECAY", { { D::MeDecay, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxChorus, FxDelay }).chorus (0.5f, 0.3f, 0.25f).delay (D1_8D, 0.3f, 0.18f, true));
    add (pd ("Warp Stack Bass", "Bass", WPdSquare, 0.4f, W2PdSaw, 0.3f)
             .sub (0, 0.4f).menv (0.001f, 0.25f, 0.2f, 0.2f).amp (0.001f, 0.4f, 0.9f, 0.12f).mono (0.02f)
             .macro (1, "DCW", { { D::Osc1Warp, 0.4f } })
             .macro (2, "STAGE 2", { { param ("osc1_warp2_amt"), 0.4f } })
             .macro (3, "PUNCH", { { D::MeDecay, -0.3f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive }).driveFx (1.5f, 0.25f));
    add (pd ("Envelope Warp Pluck", "Pluck", WPdRes1, 0.2f, W2Off, 0.0f)
             .menv (0.001f, 0.35f, 0.0f, 0.3f).amp (0.001f, 1.2f, 0.0f, 0.5f)
             .macro (1, "RESO", { { D::Osc1Warp, 0.4f } })
             .macro (2, "ENV", { { param ("osc1_pd_env_amt"), -0.4f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8, 0.3f, 0.2f, true).reverb (Room, 0.4f, 0.15f));
    add (pd ("PD Formant Voice", "Lead", WPdRes2, 0.35f, W2Mirror, 0.3f)
             .menv (0.08f, 0.8f, 0.5f, 0.5f).amp (0.05f, 0.4f, 0.9f, 0.4f)
             .filter1 (VowelBank, 1000.0f, 0.3f).morph (1, 0.25f)
             .lfo (1, Tri, 0.3f).mod (Lfo1, D::Filter1Morph, 0.3f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.45f } })
             .macro (2, "RESO", { { D::Osc1Warp, 0.4f } })
             .macro (3, "MOVE", { { D::Lfo1Rate, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Plate, 0.6f, 0.22f).master (2.0f));
    add (pd ("Dual Stage Warp Lead", "Lead", WPdSaw, 0.5f, W2PdRes3, 0.4f)
             .menv (0.005f, 0.8f, 0.3f, 0.3f).amp (0.003f, 0.3f, 0.9f, 0.25f).mono (0.05f)
             .unison (1, 3, 10.0f, 0.5f)
             .macro (1, "STAGE 1", { { D::Osc1Warp, 0.35f } })
             .macro (2, "STAGE 2", { { param ("osc1_warp2_amt"), 0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.2f, true).reverb (Plate, 0.5f, 0.15f));
    add (pd ("PD Metallic Bell", "Pluck", WPdRes3, 0.6f, W2PdPulse, 0.4f)
             .menv (0.001f, 1.5f, 0.0f, 1.0f).amp (0.001f, 3.0f, 0.0f, 2.0f)
             .macro (1, "METAL", { { D::Osc1Warp, 0.35f } })
             .macro (2, "PULSE", { { param ("osc1_warp2_amt"), 0.4f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.75f, 0.28f).master (-9.0f));
    add (pd ("Warp Envelope Pad", "Pad", WPdSaw, 0.2f, W2PdSquare, 0.2f)
             .set ("osc1_pd_env_amt", 0.6f).menv (1.5f, 2.0f, 0.4f, 2.0f).amp (0.8f, 0.5f, 1.0f, 1.8f)
             .unison (1, 5, 14.0f, 0.8f)
             .macro (1, "DCW", { { D::Osc1Warp, 0.4f } })
             .macro (2, "SWEEP", { { D::MeAttack, 0.4f } })
             .macro (3, "WIDTH", { { D::Osc1Spread, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.3f).master (-16.0f));
    add (pd ("PD Growl Bass", "Bass", WPdPulse, 0.5f, W2Asym, 0.4f)
             .sub (0, 0.35f).amp (0.001f, 0.4f, 0.9f, 0.12f).mono (0.03f)
             .lfoSync (1, Sine, D1_8).mod (Lfo1, D::Osc1Warp, 0.35f, 0.0f, M2)
             .macro (1, "DCW", { { D::Osc1Warp, 0.35f } })
             .macro (2, "WOBBLE", {})
             .macro (3, "ASYM", { { param ("osc1_warp2_amt"), 0.4f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive, FxEq }).driveFx (2.0f, 0.3f).eq (2.0f, 400.0f, -2.0f, 0.0f, 80.0f));

    // ======================================================================
    // Generative (M7.1): 8
    // ======================================================================
    add (B ("Euclid Bass Sequence", "Generative")
             .osc1 (AcidRamp, 0.3f, 0.75f).sub (0, 0.4f)
             .filter1 (DiodeLP, 600.0f, 0.55f, 2.5f, 2.0f).fenv (0.001f, 0.2f, 0.0f, 0.1f).amp (0.001f, 0.3f, 0.7f, 0.1f)
             .set ("euc_on", 1).set ("euc_target", 0).set ("euc_steps", 16).set ("euc_hits", 7).set ("euc_div", D1_16).set ("euc_gate", 0.4f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "ACCENT", { { D::Filter1Env, 0.35f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay }).delay (D1_8D, 0.35f, 0.15f, true));
    add (B ("Prob Seq Bells", "Generative")
             .osc1 (BellT, 0.3f, 0.7f).amp (0.001f, 1.2f, 0.0f, 1.0f)
             .scale (Dorian, Dn).set ("pseq_on", 1).set ("pseq_div", D1_8).set ("pseq_length", 12)
             .set ("pseq_chance2", 0.5f).set ("pseq_chance5", 0.4f).set ("pseq_chance7", 0.6f).set ("pseq_chance10", 0.3f)
             .set ("pseq_range3", 12).set ("pseq_range8", 7).set ("pseq_ratchet11", 3)
             .macro (1, "TONE", { { D::Osc1Frame, 0.4f } })
             .macro (2, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (3, "ECHO", { { D::FxDelayMix, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.25f, true).reverb (Hall, 0.8f, 0.3f));
    add (B ("Strummed Pad", "Generative")
             .string (1, 0.7f, XBurst, 0.8f, 0.35f).unison (1, 2, 5.0f, 0.6f).set ("osc1_couple", 0.3f)
             .osc2 (WarmPad, 0.4f, 0.3f).amp (0.001f, 3.0f, 1.0f, 1.5f)
             .set ("spray_strum", 1).set ("spray_strum_time", 60.0f)
             .macro (1, "BRIGHT", { { param ("osc1_string_damp"), -0.3f } })
             .macro (2, "PAD", { { D::Osc2Level, 0.35f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.15f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.3f, 0.3f, 0.3f).reverb (Hall, 0.8f, 0.3f));
    add (B ("Ratchet Arp Machine", "Generative")
             .osc1 (SquareSync, 0.3f, 0.7f).filter1 (OtaLP, 2200.0f, 0.4f, 1.5f).fenv (0.001f, 0.15f, 0.0f, 0.1f)
             .amp (0.001f, 0.2f, 0.6f, 0.1f).arp (ArpUpDown, D1_16, 2, 0.5f)
             .set ("pseq_on", 1).set ("pseq_div", D1_16).set ("pseq_length", 16)
             .set ("pseq_ratchet4", 2).set ("pseq_ratchet8", 3).set ("pseq_ratchet12", 2).set ("pseq_chance6", 0.5f).set ("pseq_chance14", 0.4f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "SYNC", { { D::Osc1Frame, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay }).delay (D1_8D, 0.35f, 0.2f, true));
    add (B ("Euclid Trance Gate", "Generative")
             .osc1 (SawToSquare, 0.2f, 0.6f).unison (1, 7, 18.0f, 0.8f, Hypersaw).osc2 (WarmPad, 0.4f, 0.4f, 12)
             .filter1 (LadderLP, 3500.0f, 0.2f).amp (0.05f, 0.5f, 1.0f, 0.8f)
             .set ("euc_on", 1).set ("euc_target", 2).set ("euc_steps", 16).set ("euc_hits", 9).set ("euc_div", D1_16).set ("euc_gate", 0.6f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "TAIL", { { D::AmpRelease, 0.4f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.15f, true).reverb (Hall, 0.7f, 0.25f));
    add (B ("Scale Spray Keys", "Generative")
             .osc1 (TineT, 0.3f, 0.7f).amp (0.001f, 1.5f, 0.2f, 0.8f)
             .scale (MinorPenta, An, true).spray (3, 12, SprayBoth, 90.0f, 0.7f, 0.3f)
             .macro (1, "SPRAY", { { param ("spray_spread"), 0.4f } })
             .macro (2, "TONE", { { D::Osc1Frame, 0.35f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.2f, true).reverb (Hall, 0.75f, 0.28f));
    add (B ("Prob Seq Drone", "Generative")
             .osc1 (DuffingT, 0.3f, 0.6f).osc2 (WarmSub, 0.2f, 0.5f, -12).filter1 (TwinPeak, 700.0f, 0.5f).morph (1, 0.4f)
             .amp (0.4f, 0.5f, 1.0f, 1.5f)
             .set ("pseq_on", 1).set ("pseq_div", D1_4).set ("pseq_length", 8)
             .set ("pseq_range2", 7).set ("pseq_range5", 12).set ("pseq_chance3", 0.5f).set ("pseq_chance7", 0.5f)
             .macro (1, "PEAKS", { { D::Filter1Morph, 0.4f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "SHAPE", { { D::Osc1Frame, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));
    add (B ("Euclid Exciter Pluck", "Generative")
             .string (1, 0.8f, XBurst, 0.7f, 0.4f).set ("osc1_string_pick_hardness", 0.5f)
             .amp (0.001f, 1.0f, 1.0f, 0.8f)
             .set ("euc_on", 1).set ("euc_target", 1).set ("euc_steps", 12).set ("euc_hits", 5).set ("euc_div", D1_16)
             .macro (1, "TONE", { { param ("osc1_string_damp"), -0.3f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.2f } })
             .macro (3, "ECHO", { { D::FxDelayMix, 0.35f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.2f, true).reverb (Room, 0.5f, 0.2f));

    // ======================================================================
    // Bodies and hybrids (M7.2): 12
    // ======================================================================
    const auto body = [] (const char* name, const char* category, int type, float material, float size, float amount)
    {
        return B (name, category).set ("res_on", 1).set ("body_type", (float) type).set ("body_material", material)
            .set ("body_size", size).set ("res_amount", amount);
    };
    add (body ("Marimba Bar Body", "Pluck", BBar, 0.2f, 0.4f, 0.8f)
             .string (1, 0.7f, XHammer, 0.3f, 0.6f).set ("osc1_hammer_hard", 0.4f).amp (0.001f, 1.5f, 0.0f, 0.8f)
             .macro (1, "MALLET", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "MATERIAL", { { param ("body_material"), 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f).master (3.0f));
    add (body ("Vibraphone Plate", "Keys", BPlate, 0.8f, 0.4f, 0.7f)
             .osc1 (MarimbaT, 0.3f, 0.6f).amp (0.001f, 3.0f, 0.0f, 2.0f)
             .tremolo (5.0f, 0.3f)
             .macro (1, "MOTOR", { { param ("fx_trem_depth"), 0.5f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxTremolo, FxReverb }).reverb (Room, 0.5f, 0.2f));
    add (body ("Church Bell Shell", "FX", BBell, 0.9f, 0.8f, 0.9f)
             .string (1, 1.0f, XHammer, 0.6f, 0.3f).set ("osc1_hammer_hard", 0.9f).amp (0.001f, 6.0f, 0.0f, 4.0f)
             .macro (1, "STRIKE", { { param ("osc1_hammer_hard"), 0.1f } })
             .macro (2, "SIZE", { { param ("body_size"), 0.2f } })
             .macro (3, "DECAY", { { D::ResDecay, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.3f).master (12.0f));
    add (body ("Glass Chime Body", "Pluck", BBell, 1.0f, 0.15f, 0.7f)
             .osc1 (GlassHarmonics, 0.4f, 0.5f).amp (0.001f, 2.0f, 0.0f, 1.5f)
             .macro (1, "GLASS", { { D::Osc1Frame, 0.4f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "SIZE", { { param ("body_size"), 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f).master (-1.0f));
    add (body ("Coupled String Body", "Pluck", BShell, 0.5f, 0.5f, 0.6f)
             .string (1, 0.7f, XBurst, 0.7f, 0.4f).set ("body_coupling_mode", 1).set ("body_coupling", 0.4f)
             .amp (0.001f, 3.0f, 1.0f, 1.0f)
             .macro (1, "COUPLING", { { param ("body_coupling"), 0.4f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "MATERIAL", { { param ("body_material"), 0.4f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f).master (-1.0f));
    {
        auto b = body ("FM Struck Body", "Keys", BPlate, 0.6f, 0.4f, 0.6f);
        op (b, 1, 1.0f, 0.6f); op (b, 2, 3.0f, 0.4f, false);
        fmCell (b, 2, 1, 0.3f);
        add (b.set ("sub_on", 0).menv (0.001f, 0.3f, 0.0f, 0.2f).mod (ModEnv, D::FmAmount, 0.3f)
                 .amp (0.001f, 2.0f, 0.0f, 1.2f)
                 .macro (1, "STRIKE", { { D::FmAmount, 0.35f } })
                 .macro (2, "BODY", { { D::ResAmount, 0.3f } })
                 .macro (3, "MATERIAL", { { param ("body_material"), 0.4f } })
                 .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
                 .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f));
    }
    add (body ("Feedback Body Growl", "Lead", BShell, 0.3f, 0.6f, 0.5f)
             .string (1, 0.8f, XFeedback, 0.8f, 0.5f, 0.6f).set ("osc1_fb_gain", 0.5f).set ("osc1_fb_distance", 0.4f)
             .amp (0.001f, 0.5f, 1.0f, 0.6f)
             .macro (1, "FEEDBACK", { { param ("osc1_fb_gain"), 0.3f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxAmp, FxReverb }).ampSim (0, 0.4f, 0.5f, 0.6f, 0.5f, 0.5f).reverb (Room, 0.4f, 0.15f).master (6.0f));
    add (body ("West Body Pluck", "Pluck", BBar, 0.5f, 0.3f, 0.5f)
             .osc1 (FoldSweep, 0.2f, 0.7f).set ("west_on", 1).set ("west_fold", 0.4f).set ("west_decay", 0.8f)
             .amp (0.001f, 1.0f, 1.0f, 0.6f)
             .macro (1, "FOLD", { { param ("west_fold"), 0.4f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "DECAY", { { param ("west_decay"), 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.2f, true).reverb (Room, 0.4f, 0.15f));
    {
        auto b = body ("Deep FM Bell Body", "Pad", BBell, 0.8f, 0.6f, 0.6f);
        op (b, 1, 1.0f, 0.5f); op (b, 2, 3.5f, 0.4f, false); op (b, 4, 2.0f, 0.4f); op (b, 5, 7.0f, 0.4f, false);
        fmCell (b, 2, 1, 0.3f); fmCell (b, 5, 4, 0.3f);
        add (b.set ("osc2_ratio_snap", 0).set ("sub_on", 0).amp (0.5f, 2.0f, 0.6f, 2.5f)
                 .macro (1, "BELL", { { D::FmAmount, 0.35f } })
                 .macro (2, "BODY", { { D::ResAmount, 0.3f } })
                 .macro (3, "UPPER", { { D::Osc4Level, 0.3f } })
                 .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
                 .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.32f));
    }
    add (body ("Struck Board Pad", "Pad", BPlate, 0.4f, 0.9f, 0.5f)
             .osc1 (WarmPad, 0.4f, 0.5f).osc2 (AirPad, 0.5f, 0.3f, 12).amp (0.8f, 0.5f, 1.0f, 2.0f)
             .keysBody (0.0f, 0.8f, 0.4f, 0.7f, 0.0f, 0.0f)
             .macro (1, "BOARD", { { param ("sb_mix"), 0.3f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "AIR", { { D::Osc2Level, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f));
    add (body ("Sympathetic Body Drone", "Drone", BShell, 0.4f, 0.7f, 0.6f)
             .string (1, 0.6f, XBow, 0.85f, 0.35f, 1.0f).set ("osc1_bow_pressure", 0.4f)
             .set ("sym_on", 1).set ("sym_amount", 0.7f).set ("sym_count", 6).set ("sym_decay", 0.8f)
             .amp (1.5f, 0.5f, 1.0f, 3.0f)
             .macro (1, "PRESSURE", { { param ("osc1_bow_pressure"), 0.4f } })
             .macro (2, "BODY", { { D::ResAmount, 0.3f } })
             .macro (3, "SIZE", { { param ("body_size"), 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));
    add (body ("Dual Body Coupling", "Keys", BBar, 0.7f, 0.5f, 0.6f)
             .string (1, 0.7f, XHammer, 0.6f, 0.4f).set ("osc1_hammer_hard", 0.6f)
             .set ("body_coupling_mode", 3).set ("body_coupling", 0.5f).amp (0.001f, 3.0f, 0.0f, 1.5f)
             .macro (1, "COUPLING", { { param ("body_coupling"), 0.4f } })
             .macro (2, "MALLET", { { param ("osc1_hammer_hard"), 0.3f } })
             .macro (3, "MATERIAL", { { param ("body_material"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.5f, 0.2f));

    // ======================================================================
    // West coast and new filters (M8.3, M8.4): 10
    // ======================================================================
    const auto west = [] (const char* name, const char* category, float fold, float symmetry, int stages, int mode, float decay)
    {
        return B (name, category).set ("west_on", 1).set ("west_fold", fold).set ("west_sym", symmetry)
            .set ("west_stages", (float) stages).set ("west_mode", (float) mode).set ("west_decay", decay);
    };
    add (west ("West Folded Bass", "Bass", 0.45f, 0.1f, 2, 1, 1.5f)
             .osc1 (SineT, 0.0f, 0.8f).sub (0, 0.3f).set ("west_open", 0.6f).amp (0.001f, 0.5f, 0.9f, 0.15f).mono (0.02f)
             .menv (0.001f, 0.3f, 0.2f, 0.2f).mod (ModEnv, Target (param ("west_fold"), 0.35f))
             .macro (1, "FOLD", { { param ("west_fold"), 0.4f } })
             .macro (2, "SYMMETRY", { { param ("west_sym"), 0.5f } })
             .macro (3, "OPEN", { { param ("west_open"), 0.35f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxDrive }).driveFx (1.5f, 0.2f));
    add (west ("Buchla Bongo Melody", "Pluck", 0.35f, 0.0f, 2, 0, 0.8f)
             .osc1 (TriToSquare, 0.0f, 0.8f).set ("west_res", 0.35f).amp (0.001f, 1.0f, 1.0f, 0.5f)
             .scale (Dorian, Dn).spray (2, 7, SprayUp, 110.0f, 0.5f, 0.3f)
             .macro (1, "FOLD", { { param ("west_fold"), 0.4f } })
             .macro (2, "DECAY", { { param ("west_decay"), 0.3f } })
             .macro (3, "SYMMETRY", { { param ("west_sym"), 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.2f, true).reverb (Room, 0.5f, 0.2f));
    add (B ("Steiner Growl", "Bass")
             .osc1 (DriveSaw, 0.3f, 0.8f).sub (0, 0.35f)
             .filter1 (Steiner, 500.0f, 0.55f, 2.5f, 2.0f).morph (1, 0.15f).fenv (0.001f, 0.25f, 0.2f, 0.2f)
             .amp (0.002f, 0.4f, 0.9f, 0.15f).mono (0.03f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "INPUT", { { D::Filter1Morph, 0.5f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "DRIVE", { { D::Filter1Drive, 0.4f } })
             .fx ({ FxDrive }).driveFx (1.5f, 0.25f));
    add (B ("Vowel Bank Voice", "Lead")
             .osc1 (SoftSaw, 0.5f, 0.8f).unison (1, 3, 8.0f, 0.5f)
             .filter1 (VowelBank, 1000.0f, 0.45f).morph (1, 0.0f).amp (0.02f, 0.3f, 1.0f, 0.3f).legato (0.08f)
             .mod (Wheel, D::Filter1Morph, 0.8f)
             .lfo (1, Sine, 5.2f, true).mod (Lfo1, D::Osc1Pitch, 0.01f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.8f } })
             .macro (2, "SHARP", { { D::Filter1Reso, 0.3f } })
             .macro (3, "DETUNE", { { D::Osc1Detune, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Plate, 0.6f, 0.22f));
    add (B ("Talking Filter Lead", "Lead")
             .osc1 (SawToSquare, 0.2f, 0.8f).filter1 (Talking, 1000.0f, 0.5f)
             .amp (0.003f, 0.3f, 0.9f, 0.25f).mono (0.05f)
             .lfoSync (1, Tri, D1_2).mod (Lfo1, D::Filter1Morph, 0.45f)
             .macro (1, "TALK", { { D::Filter1Morph, 0.45f } })
             .macro (2, "SPEED", { { D::Lfo1Rate, 0.35f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay }).delay (D1_8D, 0.35f, 0.2f, true));
    add (B ("Twin Peak Pad", "Pad")
             .osc1 (WarmPad, 0.4f, 0.6f).osc2 (AirPad, 0.4f, 0.4f, 12)
             .filter1 (TwinPeak, 700.0f, 0.55f).morph (1, 0.4f).amp (0.8f, 0.5f, 1.0f, 1.8f)
             .lfo (1, Sine, 0.08f).mod (Lfo1, D::Filter1Cutoff, 0.25f).mod (Lfo1, D::Filter1Morph, 0.2f)
             .macro (1, "PEAKS", { { D::Filter1Morph, 0.45f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.3f));
    add (B ("OTA Squelch", "Bass")
             .osc1 (AcidRamp, 0.4f, 0.8f)
             .filter1 (OtaLP, 450.0f, 0.7f, 3.0f, 2.0f).fenv (0.001f, 0.18f, 0.0f, 0.12f)
             .amp (0.001f, 0.3f, 0.8f, 0.1f).mono (0.06f).set ("glide_legato", 1)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "ACCENT", { { D::Filter1Env, 0.35f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDrive, FxDelay }).driveFx (2.5f, 0.4f).delay (D1_8D, 0.3f, 0.12f, true));
    add (B ("Comb Morph Pluck", "Pluck")
             .osc1 (PluckStiff, 0.2f, 0.6f).noise (0.2f)
             .filter1 (CombMorph, 261.63f, 0.8f, 0.0f, 1.0f, 1.0f).morph (1, 0.6f)
             .amp (0.001f, 1.2f, 0.0f, 0.6f)
             .macro (1, "COMB", { { D::Filter1Morph, 0.4f } })
             .macro (2, "RING", { { D::Filter1Reso, 0.2f } })
             .macro (3, "NOISE", { { D::NoiseLevel, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.2f, true).reverb (Room, 0.4f, 0.15f));
    add (B ("Phaser Notch Sweep", "Pad")
             .osc1 (SawOctaves, 0.4f, 0.6f).unison (1, 5, 12.0f, 0.8f)
             .filter1 (PhaserNotch, 800.0f, 0.5f).morph (1, 0.5f).amp (0.6f, 0.5f, 1.0f, 1.5f)
             .lfo (1, Tri, 0.12f).mod (Lfo1, D::Filter1Cutoff, 0.4f)
             .macro (1, "SWEEP", { { D::Lfo1Rate, 0.35f } })
             .macro (2, "SPREAD", { { D::Filter1Morph, 0.4f } })
             .macro (3, "FEEDBACK", { { D::Filter1Reso, 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.3f));
    add (B ("SEM Classic Lead", "Lead")
             .osc1 (RcSaw, 0.6f, 0.7f).osc2 (AnalogPulse, 0.3f, 0.5f, 0, 6.0f)
             .filter1 (Sem, 2400.0f, 0.3f, 1.5f, 1.3f, 0.4f).morph (1, 0.0f).fenv (0.003f, 0.4f, 0.4f, 0.3f)
             .amp (0.003f, 0.3f, 0.9f, 0.25f).mono (0.05f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "MODE", { { D::Filter1Morph, 0.5f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.18f, true).reverb (Plate, 0.45f, 0.14f));

    // ======================================================================
    // Feedback guitar, Evolve and the vector pad (M8.5): 8
    // ======================================================================
    add (B ("Feedback Guitar Drone", "Drone")
             .string (1, 0.8f, XFeedback, 0.8f, 0.5f, 0.6f).set ("osc1_fb_gain", 0.5f).set ("osc1_fb_distance", 0.5f)
             .set ("osc1_semi", -12).amp (0.001f, 1.0f, 1.0f, 2.0f)
             .macro (1, "FEEDBACK", { { param ("osc1_string_sustain"), 0.3f } })
             .macro (2, "AMP", { { param ("osc1_fb_gain"), 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxAmp, FxReverb }).ampSim (0, 0.5f, 0.5f, 0.6f, 0.5f, 0.5f).reverb (Hall, 0.7f, 0.25f).master (4.0f));
    add (B ("Screaming Feedback Lead", "Lead")
             .string (1, 0.9f, XFeedback, 0.85f, 0.4f, 0.8f).set ("osc1_fb_gain", 0.7f).set ("osc1_fb_distance", 0.3f)
             .amp (0.001f, 0.5f, 1.0f, 0.4f).mono (0.04f).bend (2.0f)
             .lfo (1, Sine, 5.5f, true).mod (Lfo1, D::Osc1Pitch, 0.02f, 0.0f, Wheel)
             .macro (1, "FEEDBACK", { { param ("osc1_string_sustain"), 0.2f } })
             .macro (2, "GAIN", { { param ("osc1_fb_gain"), 0.3f } })
             .macro (3, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxAmp, FxDelay }).ampSim (1, 0.6f, 0.5f, 0.6f, 0.6f, 0.45f).delay (D1_4, 0.35f, 0.2f, true).master (2.0f));
    add (B ("Evolve Macro Pad", "Pad")
             .osc1 (HarmonicBands, 0.3f, 0.55f).osc2 (WarmPad, 0.3f, 0.45f, 12)
             .filter1 (LadderLP, 1800.0f, 0.3f).amp (0.9f, 0.5f, 1.0f, 2.0f)
             .set ("macro1", 0.5f).set ("macro1_evolve", 0.5f).set ("macro1_evolve_rate", 0.15f)
             .set ("macro2", 0.4f).set ("macro2_evolve", 0.4f).set ("macro2_evolve_rate", 0.08f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.5f } })
             .macro (2, "BANDS", { { D::Osc1Frame, 0.6f } })
             .macro (3, "AIR", { { D::Osc2Level, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.35f, 0.3f).reverb (Hall, 0.85f, 0.3f));
    const auto vector = [] (const char* name, const char* category)
    {
        return B (name, category).set ("vec_on", 1).set ("vec_a", 0).set ("vec_b", 1).set ("vec_c", 3).set ("vec_d", 4);
    };
    add (vector ("Vector Corner Morph", "Pad")
             .osc1 (VowelsMale, 0.3f, 0.6f).osc2 (GlassHarmonics, 0.4f, 0.6f).osc (4, WarmPad, 0.4f, 0.6f, -12).osc (5, ShimmerStack, 0.4f, 0.6f, 12)
             .amp (0.7f, 0.5f, 1.0f, 1.8f).mod (Wheel, Target (param ("vec_x"), 0.5f)).mod (AT, Target (param ("vec_y"), 0.5f))
             .macro (1, "X", { { param ("vec_x"), 0.5f } })
             .macro (2, "Y", { { param ("vec_y"), 0.5f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f).master (-10.0f));
    add (vector ("Vector Path Drift", "Pad")
             .osc1 (WarmPad, 0.3f, 0.6f).osc2 (Whisper, 0.4f, 0.6f).osc (4, OddTilt, 0.5f, 0.6f).osc (5, AirPad, 0.4f, 0.6f, 12)
             .set ("vec_path", 1).set ("vec_rate", 0.08f).set ("vec_drift", 0.3f).amp (0.8f, 0.5f, 1.0f, 2.0f)
             .macro (1, "X", { { param ("vec_x"), 0.4f } })
             .macro (2, "Y", { { param ("vec_y"), 0.4f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, -0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.3f, 0.3f).reverb (Hall, 0.85f, 0.3f));
    add (B ("Feedback Distance Growl", "Bass")
             .string (1, 0.9f, XFeedback, 0.7f, 0.6f, 0.5f).set ("osc1_fb_gain", 0.4f).set ("osc1_fb_distance", 0.7f)
             .set ("osc1_semi", -12).sub (0, 0.3f).amp (0.001f, 0.8f, 1.0f, 0.3f).mono (0.03f)
             .lfo (1, Tri, 0.3f).mod (Lfo1, Target (param ("osc1_fb_gain"), 0.2f))
             .macro (1, "GROWL", { { param ("osc1_fb_gain"), 0.3f } })
             .macro (2, "FEEDBACK", { { param ("osc1_string_sustain"), 0.3f } })
             .macro (3, "MOVE", { { D::Lfo1Rate, 0.3f } })
             .macro (4, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .fx ({ FxAmp }).ampSim (1, 0.5f, 0.6f, 0.5f, 0.5f, 0.5f).master (2.0f));
    add (vector ("Evolving Vector Drone", "Drone")
             .osc1 (RosslerT, 0.3f, 0.5f).osc2 (DuffingT, 0.4f, 0.5f).osc (4, WarmSub, 0.3f, 0.6f, -12).osc (5, HenonT, 0.4f, 0.5f, 7)
             .set ("vec_drift", 0.6f).set ("vec_drift_rate", 0.05f)
             .set ("macro1", 0.5f).set ("macro1_evolve", 0.6f).set ("macro1_evolve_rate", 0.05f)
             .filter1 (TwinPeak, 600.0f, 0.5f).morph (1, 0.3f).amp (1.5f, 0.5f, 1.0f, 3.0f)
             .macro (1, "PEAKS", { { D::Filter1Morph, 0.5f } })
             .macro (2, "Y", { { param ("vec_y"), 0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.9f, 0.35f));
    add (vector ("Vector Strike Pad", "Keys")
             .osc1 (BellT, 0.3f, 0.6f).osc2 (MarimbaT, 0.4f, 0.6f).osc (4, TineT, 0.3f, 0.6f).osc (5, GlassHarmonics, 0.3f, 0.6f, 12)
             .set ("vec_path", 1).set ("vec_rate", 0.3f).amp (0.001f, 2.0f, 0.4f, 1.2f)
             .macro (1, "Y", { { param ("vec_y"), 0.3f } })
             .macro (2, "X", { { param ("vec_x"), 0.3f } })
             .macro (3, "DECAY", { { D::AmpDecay, 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.3f, 0.18f, true).reverb (Hall, 0.75f, 0.28f));

    // ======================================================================
    // Resampled sources (M8.6): 6. Each bounces another factory preset
    // onto an oscillator when it loads, then plays it back its own way.
    // ======================================================================
    add (B ("Bounced Bell Grains", "Pad")
             .bounceFrom ("Church Bell Shell", 1, false, 60, 1.0f, 3.0f)
             .granular (1, 0, 0.8f, 0.15f, 180.0f, 0.7f, 0.3f, 0.0f, 0.8f).set ("sub_on", 0)
             .amp (0.8f, 0.5f, 1.0f, 2.0f).lfo (1, Tri, 0.05f).mod (Lfo1, D::Osc1SampleStart, 0.3f)
             .macro (1, "SCAN", { { D::Osc1SampleStart, 0.5f } })
             .macro (2, "SIZE", { { param ("osc1_grain_size"), 0.3f } })
             .macro (3, "SPRAY", { { param ("osc1_grain_spray"), 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f).master (0.0f));
    add (B ("Resampled FM Texture", "Pad")
             .bounceFrom ("DX Bell Pad", 1, false, 60, 2.0f, 2.0f)
             .granular (1, 0, 0.8f, 0.3f, 90.0f, 0.8f, 0.5f, 0.1f, 0.9f).set ("sub_on", 0)
             .amp (0.6f, 0.5f, 1.0f, 1.8f)
             .macro (1, "POSITION", { { D::Osc1SampleStart, 0.5f } })
             .macro (2, "PITCH", { { param ("osc1_grain_pitch"), 0.3f } })
             .macro (3, "DENSITY", { { param ("osc1_grain_density"), 0.3f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.3f, 0.3f).reverb (Hall, 0.85f, 0.3f));
    add (B ("Baked Chaos Loop", "Drone")
             .bounceFrom ("Lorenz Wash", 1, false, 60, 4.0f, 0.5f)
             .set ("osc1_sample_loop", 1).set ("osc1_sample_fade_in", 0.1f).set ("osc1_sample_fade_out", 0.1f).set ("sub_on", 0)
             .filter1 (Sem, 2000.0f, 0.3f).amp (1.0f, 0.5f, 1.0f, 2.0f)
             .macro (1, "START", { { D::Osc1SampleStart, 0.4f } })
             .macro (2, "END", { { D::Osc1SampleEnd, -0.4f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f));
    add (B ("Reversed Piano Wash", "Pad")
             .bounceFrom ("Grand Piano", 1, false, 60, 3.0f, 1.0f)
             .set ("osc1_sample_reverse", 1).set ("sub_on", 0)
             .filter1 (LP, 4000.0f, 0.1f).amp (0.05f, 0.5f, 1.0f, 1.5f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "START", { { D::Osc1SampleStart, 0.4f } })
             .macro (3, "ECHO", { { D::FxDelayMix, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_4, 0.35f, 0.2f, true).reverb (Hall, 0.9f, 0.35f).master (0.0f));
    add (B ("Frozen Feedback Table", "Lead")
             .bounceFrom ("Feedback Guitar Drone", 1, true, 48, 3.0f, 0.5f)
             .set ("sub_on", 0).filter1 (LadderLP, 3000.0f, 0.3f).amp (0.005f, 0.3f, 0.9f, 0.3f).mono (0.04f)
             .lfo (1, Tri, 0.2f).mod (Lfo1, D::Osc1Frame, 0.4f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.5f } })
             .macro (2, "SCAN", { { D::Lfo1Rate, 0.35f } })
             .macro (3, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay }).delay (D1_8D, 0.3f, 0.18f, true).master (-1.0f));
    add (B ("Tabled Cello", "Pad")
             .bounceFrom ("Cello Section", 1, true, 48, 3.0f, 0.5f)
             .set ("sub_on", 0).unison (1, 5, 12.0f, 0.8f).filter1 (LP, 5000.0f, 0.1f).amp (0.5f, 0.5f, 1.0f, 1.5f)
             .lfo (1, Sine, 0.1f).mod (Lfo1, D::Osc1Frame, 0.45f)
             .macro (1, "FRAME", { { D::Osc1Frame, 0.5f } })
             .macro (2, "WIDTH", { { D::Osc1Spread, 0.3f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.35f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f));

    // ======================================================================
    // ilanaSynth FX (M7.5): 10. In the instrument there is no input, so they
    // are silent there.
    // ======================================================================
    const auto live = [] (const char* name)
    {
        return B (name, "FX Input").set ("osc2_on", 0).set ("sub_on", 0).set ("subosc_on", 0)
            .set ("in_trigger", 2).velocity (0.0f).amp (0.01f, 1.0f, 1.0f, 0.5f).master (0.0f);
    };
    add (live ("Live Comb Wash").set ("osc1_mode", 4).set ("osc1_level", 1.0f)
             .filter1 (CombMorph, 220.0f, 0.7f).morph (1, 0.5f).lfo (1, Sine, 0.1f).mod (Lfo1, D::Filter1Cutoff, 0.3f)
             .macro (1, "COMB", { { D::Filter1Morph, 0.4f } })
             .macro (2, "PITCH", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.7f, 0.3f));
    add (live ("Live Feedback Loop").set ("osc1_on", 0).set ("in_dry", 0.7f)
             .macro (1, "FEEDBACK", { { param ("fx_feedback_amount"), 0.3f } })
             .macro (2, "DELAY", { { D::FxDelayMix, 0.4f } })
             .macro (3, "DRY", { { param ("in_dry"), -0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxFeedback, FxDelay, FxReverb }).delay (D1_8D, 0.5f, 0.35f, true).reverb (Hall, 0.7f, 0.25f));
    add (live ("Live Struck Bells").set ("osc1_on", 0).set ("in_note", 60).set ("in_dry", 0.4f).set ("in_body", 0.9f)
             .set ("res_on", 1).set ("res_amount", 0.9f).set ("res_decay", 0.8f).set ("body_type", BBell).set ("body_material", 0.8f)
             .macro (1, "BELLS", { { D::ResAmount, 0.5f } })
             .macro (2, "DECAY", { { D::ResDecay, 0.3f } })
             .macro (3, "MATERIAL", { { param ("body_material"), 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.8f, 0.25f));
    add (live ("Live West Fold").set ("osc1_mode", 4).set ("osc1_level", 1.0f)
             .set ("west_on", 1).set ("west_fold", 0.5f).set ("west_mode", 1).set ("west_open", 0.7f).set ("west_decay", 1.0f)
             .set ("west_src", (float) src (Mod::Source::InputEnv))
             .macro (1, "FOLD", { { param ("west_fold"), 0.4f } })
             .macro (2, "SYMMETRY", { { param ("west_sym"), 0.5f } })
             .macro (3, "OPEN", { { param ("west_open"), -0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.4f, 0.15f));
    add (live ("Live Vector Morph").set ("osc1_mode", 4).set ("osc1_level", 1.0f)
             .filter1 (VowelBank, 1000.0f, 0.4f).mod (src (Mod::Source::InputEnv), D::Filter1Morph, 0.6f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.5f } })
             .macro (2, "RESO", { { D::Filter1Reso, 0.3f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Plate, 0.5f, 0.2f));
    add (live ("Live Ring FM").set ("osc1_mode", 4).set ("osc1_level", 1.0f).osc2 (SineT, 0.0f, 0.0f).set ("osc2_out", 0)
             .set ("fm_amount", 0.0f).ring (0.6f).set ("fx_shifter_shift", 30.0f)
             .macro (1, "RING", { { D::RingMod, 0.4f } })
             .macro (2, "SHIFT", { { param ("fx_shifter_shift"), 0.3f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxFreqShift, FxReverb }).reverb (Room, 0.4f, 0.15f));
    add (live ("Live Granular Delay")
             .granular (1, 0, 0.9f, 0.4f, 220.0f, 0.5f, 0.5f, 0.2f, 0.9f)
             .set ("osc1_grain_live", 1).set ("osc1_sample_tuned", 0).set ("in_dry", 0.5f)
             .macro (1, "BACK", { { D::Osc1SampleStart, 0.5f } })
             .macro (2, "PITCH", { { param ("osc1_grain_pitch"), 0.4f } })
             .macro (3, "ECHO", { { D::FxDelayMix, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.45f, 0.3f, true).reverb (Hall, 0.85f, 0.3f));
    add (live ("Live Formant Filter").set ("osc1_mode", 4).set ("osc1_level", 1.0f)
             .filter1 (Talking, 1000.0f, 0.5f).lfoSync (1, Tri, D1_1).mod (Lfo1, D::Filter1Morph, 0.45f)
             .macro (1, "TALK", { { D::Filter1Morph, 0.45f } })
             .macro (2, "SPEED", { { D::Lfo1Rate, 0.3f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Plate, 0.5f, 0.2f));
    add (live ("Live Feedback Strings").set ("osc1_on", 0)
             .string (1, 0.7f, XFeedback, 0.85f, 0.4f, 0.4f).set ("osc1_fb_gain", 0.4f)
             .set ("in_note", 45).set ("in_strings", 0.5f).set ("in_dry", 0.5f)
             .macro (1, "RING", { { param ("in_strings"), 0.4f } })
             .macro (2, "FEEDBACK", { { param ("osc1_fb_gain"), 0.3f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.7f, 0.25f));
    add (live ("Live Twin Peak Wah").set ("osc1_mode", 4).set ("osc1_level", 1.0f)
             .set ("in_attack", 3.0f).set ("in_release", 150.0f)
             .filter1 (TwinPeak, 400.0f, 0.6f).morph (1, 0.35f).mod (src (Mod::Source::InputEnv), D::Filter1Cutoff, 0.55f)
             .macro (1, "RANGE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "PEAKS", { { D::Filter1Morph, 0.4f } })
             .macro (3, "DRY", { { param ("in_dry"), 0.5f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Room, 0.4f, 0.1f));
}
