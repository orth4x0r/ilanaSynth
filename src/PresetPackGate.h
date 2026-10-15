#pragma once

// Roadmap step 25: the trance gate pack. Thirteen presets built around the
// Trance Gate effect, appended after every earlier preset (their indices never
// move). The gate sits first in each rack, so reverb and delay tails ring out
// into the gaps it makes. Included from Presets.h after the DX7 banks, inside
// Presets::Library; uses the enums of PresetLibrary.h and PresetPackM10.h.

inline void addGatePresets (const std::function<void (const FactoryPreset&)>& add)
{
    using B = Builder;

    // ======================================================================
    // Rhythmic chords and pads
    // ======================================================================

    // 3+3+2 over two beats: every onset hits hard and leaves a soft second step.
    // ilana's own version (2026-10-15, her "Tresillo Gate_edited"), baked as the default.
    {
        FactoryPreset p { "Tresillo Gate", {} };
        p.category = "Chords";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 0.3999999f }, { "amp_release", 0.4999999f }, { "amp_sustain", 0.9f },
            { "f1_cutoff", 3200.0f }, { "f1_drive", 1.4f }, { "f1_env", 1.2f }, { "f1_keytrack", 0.4f },
            { "f1_type", 4.0f }, { "fe_attack", 0.002000001f }, { "fe_decay", 0.35f }, { "fe_sustain", 0.5f },
            { "fx_delay_div", 9.0f }, { "fx_delay_feedback", 0.4f }, { "fx_delay_mix", 0.2f }, { "fx_delay_on", 1.0f },
            { "fx_delay_pingpong", 1.0f }, { "fx_delay_sync", 1.0f }, { "fx_gate_pattern", 8.0f }, { "fx_gate_smooth", 0.12f },
            { "fx_gate_step10", 0.4444444f }, { "fx_gate_step11", 0.0f }, { "fx_gate_step13", 0.4444444f }, { "fx_gate_step14", 0.0f },
            { "fx_gate_step16", 0.4444444f }, { "fx_gate_step2", 0.4444444f }, { "fx_gate_step3", 0.0f }, { "fx_gate_step5", 0.4444444f },
            { "fx_gate_step6", 0.0f }, { "fx_gate_step8", 0.4444444f }, { "fx_reverb_mix", 0.22f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_size", 0.75f }, { "fx_reverb_type", 1.0f }, { "fx_slot1", 16.0f }, { "fx_slot2", 9.0f },
            { "fx_slot3", 13.0f }, { "mod1_amt", 0.45f }, { "mod1_dst", 9.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.1f }, { "mod2_dst", 87.0f }, { "mod2_src", 12.0f }, { "mod3_amt", -0.85f },
            { "mod3_dst", 154.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.6f }, { "mod4_dst", 153.0f },
            { "mod4_src", 14.0f }, { "mod5_amt", 0.3f }, { "mod5_dst", 22.0f }, { "mod5_src", 15.0f },
            { "mod6_amt", 0.25f }, { "mod6_dst", 26.0f }, { "mod6_src", 15.0f }, { "osc1_detune", 22.46148f },
            { "osc1_frame", 0.25f }, { "osc1_level", 0.55f }, { "osc1_spread", 0.95f }, { "osc1_table", 45.0f },
            { "osc1_uni_mode", 1.0f }, { "osc1_unison", 9.0f }, { "osc2_detune", 13.83299f }, { "osc2_fine", 0.0f },
            { "osc2_frame", 0.3f }, { "osc2_level", 0.35f }, { "osc2_on", 1.0f }, { "osc2_semi", 12.0f },
            { "osc2_spread", 0.8f }, { "osc2_table", 16.0f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 5.0f },
            { "master", -7.5f },
        };
        p.macroNames = { "CUTOFF", "CHOP", "SMOOTH", "SPACE" };
        add (p);
    }

    // A seven-step pattern in a four-beat bar: the chop slides against the grid.
    add (B ("Seven Over Four", "Pad")
             .osc1 (WarmPad, 0.4f, 0.6f).unison (1, 7, 20.0f, 0.9f, Fifths)
             .osc2 (SoftSaw, 0.5f, 0.3f, -12).unison (2, 3, 9.0f, 0.6f)
             .filter1 (LadderLP, 4200.0f, 0.2f, 0.8f, 1.2f, 0.4f).fenv (0.01f, 0.8f, 0.6f, 0.6f)
             .amp (0.02f, 0.8f, 0.9f, 1.0f)
             .master (-5.5f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "SWING", { { param ("fx_gate_swing"), 0.8f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxGate, FxChorus, FxReverb }).gateSteps (D1_16, "9409060", 0.18f)
             .chorus (0.3f, 0.45f, 0.3f).reverb (Plate, 0.8f, 0.28f));

    // Sidechain from nowhere: a duck on every beat that swells back in.
    add (B ("Pump Pad", "Pad")
             .osc1 (SawToSquare, 0.55f, 0.55f).unison (1, 11, 30.0f, 1.0f, Hypersaw)
             .osc2 (AirPad, 0.3f, 0.4f, 12).unison (2, 4, 12.0f, 0.8f)
             .filter1 (LadderLP, 5200.0f, 0.15f, 0.5f, 1.1f, 0.3f)
             .amp (0.05f, 0.6f, 1.0f, 1.2f)
             .master (-8.0f)
             .macro (1, "PUMP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (2, "RECOVER", { { param ("fx_gate_smooth"), 0.7f } })
             .macro (3, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxGate, FxWidener, FxReverb }).gateSteps (D1_16, "1469", 0.5f)
             .width (1.6f, 0.5f).reverb (Hall, 0.8f, 0.3f));

    // A choir that breathes: stepped swells over one bar of eighths.
    add (B ("Breathing Choir", "Pad")
             .osc1 (Choir, 0.35f, 0.6f).unison (1, 5, 14.0f, 0.9f)
             .osc2 (VowelsFemale, 0.2f, 0.35f, 12).unison (2, 3, 10.0f, 0.7f)
             .filter1 (Formant, 1400.0f, 0.3f, 0.3f, 1.1f, 0.3f).morph (1, 0.4f)
             .lfo (1, Sine, 0.12f).mod (Lfo1, D::Filter1Morph, 0.35f).mod (Lfo1, D::Osc2Frame, 0.3f)
             .amp (0.15f, 0.8f, 0.9f, 1.4f)
             .master (-4.5f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.45f } })
             .macro (2, "BREATH", { { param ("fx_gate_mix"), -0.85f } })
             .macro (3, "WIDTH", { { D::Osc1Detune, 0.4f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxGate, FxChorus, FxReverb }).gateSteps (D1_8, "14689864", 0.85f)
             .chorus (0.25f, 0.4f, 0.3f).reverb (Shimmer, 0.9f, 0.35f));

    // Gate first, a huge hall after it: the tails fill every gap.
    add (B ("Gated Cathedral", "Pad")
             .osc1 (SoftSaw, 0.4f, 0.5f).unison (1, 9, 22.0f, 0.95f, Hypersaw).chord (1, 3)
             .osc2 (ShimmerStack, 0.3f, 0.3f, 12)
             .filter1 (LadderLP, 3600.0f, 0.2f, 0.8f, 1.2f, 0.4f).fenv (0.01f, 0.6f, 0.6f, 0.5f)
             .amp (0.01f, 0.8f, 0.9f, 0.8f)
             .master (-10.0f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "SIZE", { { D::FxReverbSize, 0.3f } })
             .macro (4, "TAIL", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxGate, FxReverb }).gateSteps (D1_8, "90930500", 0.2f)
             .reverb (Hall, 0.7f, 0.5f, 0.35f));

    // 1/16 chops against a drifting triplet arp: the two grids phase against each other.
    add (B ("Triplet Arp Through Gate", "Arp")
             .osc1 (Analog, 0.3f, 0.6f).unison (1, 5, 14.0f, 0.8f, Hypersaw)
             .osc2 (SquareSweep, 0.4f, 0.25f, 12)
             .filter1 (LadderLP, 2800.0f, 0.3f, 1.8f, 1.3f, 0.4f).fenv (0.001f, 0.18f, 0.2f, 0.15f)
             .amp (0.001f, 0.25f, 0.5f, 0.2f)
             .master (-4.0f)
             .arp (ArpUpDown, D1_16T, 2, 0.6f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "RESO", { { D::Filter1Reso, 0.4f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.2f } })
             .fx ({ FxGate, FxDelay, FxReverb }).gate (D1_16, 5, 0.1f)
             .delay (D1_8D, 0.4f, 0.22f, true).reverb (Plate, 0.6f, 0.2f));

    // Ghost notes: a swung 1/16 strings line with quiet in-between steps.
    add (B ("Ghost Note Strings", "Chords")
             .osc1 (SoftSaw, 0.7f, 0.5f).unison (1, 7, 16.0f, 0.9f, Octaves).chord (1, 4)
             .osc2 (SawOctaves, 0.3f, 0.3f, 0)
             .filter1 (Ms20LP, 3400.0f, 0.3f, 1.0f, 1.3f, 0.4f).fenv (0.003f, 0.5f, 0.5f, 0.4f)
             .amp (0.004f, 0.5f, 0.85f, 0.5f)
             .master (-6.5f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "GHOSTS", { { param ("fx_gate_mix"), -0.7f } })
             .macro (3, "SWING", { { param ("fx_gate_swing"), 0.9f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxGate, FxChorus, FxReverb }).gateSteps (D1_16, "9262926392629472", 0.15f, 0.3f)
             .chorus (0.5f, 0.35f, 0.3f).reverb (Plate, 0.65f, 0.22f));

    // Five dotted sixteenths in a four-beat bar, with a dotted echo on top.
    add (B ("Dotted Five Drone", "Drone")
             .osc1 (AirPad, 0.5f, 0.55f).unison (1, 5, 10.0f, 0.9f)
             .osc2 (WarmSub, 0.1f, 0.4f, -12)
             .filter1 (Morph, 1800.0f, 0.3f, 0.2f, 1.1f, 0.2f).morph (1, 0.3f)
             .lfo (1, SmoothRandom, 0.25f).mod (Lfo1, D::Filter1Cutoff, 0.18f)
             .amp (0.3f, 1.0f, 1.0f, 2.0f)
             .master (-4.0f)
             .macro (1, "TONE", { { D::Filter1Cutoff, 0.4f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.85f } })
             .macro (3, "SOFT", { { param ("fx_gate_smooth"), 0.6f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f }, { D::FxReverbMix, 0.25f } })
             .fx ({ FxGate, FxDelay, FxReverb }).gateSteps (D1_16D, "96038", 0.3f)
             .delay (D1_8D, 0.5f, 0.3f, true).reverb (Hall, 0.9f, 0.3f));

    // ======================================================================
    // Bass, builds and the generative gates
    // ======================================================================

    // A half-time bass cut into stabs; the filter wobble runs a bar slower.
    add (B ("Half Time Gate Bass", "Bass")
             .osc1 (ReeseT, 0.3f, 0.6f).unison (1, 3, 14.0f, 0.5f)
             .osc2 (WarmSub, 0.0f, 0.5f, -12)
             .filter1 (LadderDrive, 700.0f, 0.3f, 1.0f, 2.0f, 0.3f)
             .lfoSync (1, Tri, D1_2).mod (Lfo1, D::Filter1Cutoff, 0.3f)
             .fenv (0.001f, 0.25f, 0.3f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.15f)
             .master (-9.5f)
             .mono (0.02f)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "GRIT", { { param ("fx_drive_mix"), 0.4f } })
             .macro (4, "WOBBLE", { { D::Lfo1Rate, 0.3f } })
             .fx ({ FxGate, FxDrive, FxOtt }).gateSteps (D1_16, "9500950095009590", 0.08f)
             .driveFx (3.5f, 0.45f).ott (0.3f, 0.35f));

    // The gate plays the Euclid rhythm: seven hits in sixteen, rotated.
    add (B ("Euclid Gate Stack", "Generative")
             .osc1 (SawToSquare, 0.3f, 0.55f).unison (1, 9, 20.0f, 0.95f, Hypersaw).chord (1, 6)
             .osc2 (WarmPad, 0.4f, 0.4f, 12).unison (2, 3, 10.0f, 0.6f)
             .filter1 (LadderLP, 3800.0f, 0.2f, 0.8f, 1.2f, 0.4f).fenv (0.003f, 0.4f, 0.6f, 0.4f)
             .amp (0.005f, 0.5f, 1.0f, 0.5f)
             .master (-7.5f)
             .set ("euc_on", 1).set ("euc_target", 2).set ("euc_steps", 16).set ("euc_hits", 7)
             .set ("euc_rotate", 2).set ("euc_div", D1_16)
             .macro (1, "CUTOFF", { { D::Filter1Cutoff, 0.45f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.85f } })
             .macro (3, "SMOOTH", { { param ("fx_gate_smooth"), 0.6f } })
             .macro (4, "SPACE", { { D::FxDelayMix, 0.3f }, { D::FxReverbMix, 0.25f } })
             .fx ({ FxGate, FxDelay, FxReverb }).gate (D1_16, 0, 0.15f)
             .delay (D1_8D, 0.45f, 0.22f, true).reverb (Hall, 0.8f, 0.25f));

    // The Build pattern opens one more step every half-second: a four-second riser.
    add (B ("Gate Build Riser", "FX")
             .osc1 (Riser, 0.5f, 0.55f).unison (1, 7, 24.0f, 1.0f, Hypersaw)
             .osc2 (SawToSquare, 0.6f, 0.3f, 12)
             .filter1 (LadderLP, 600.0f, 0.3f, 3.0f, 1.3f, 0.2f).fenv (4.0f, 1.0f, 1.0f, 0.5f)
             .amp (0.05f, 0.5f, 1.0f, 0.6f)
             .master (-7.0f)
             .macro (1, "RISE", { { D::Filter1Cutoff, 0.5f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "EDGE", { { param ("fx_gate_smooth"), 0.6f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxGate, FxReverb }).gate (D1_32, 6, 0.08f).set ("fx_gate_steps", 8)
             .reverb (Hall, 0.85f, 0.3f));

    // ======================================================================
    // Odd ones
    // ======================================================================

    // A bell clanging through 1/32 ghost notes, crushed and ring-modulated: Arca by way of Vital.
    add (B ("Stutter Bell Machine", "FX")
             .osc1 (FmMetal, 0.35f, 0.55f).unison (1, 3, 7.0f, 0.8f)
             .osc2 (FibonacciBell, 0.3f, 0.35f, 7)
             .fm (0.25f, 0.1f).ring (0.2f)
             .filter1 (Formant, 3800.0f, 0.35f, 0.6f, 1.3f, 0.3f).morph (1, 0.5f)
             .lfoSync (1, SampleHold, D1_32).mod (Lfo1, D::Osc1Frame, 0.3f)
             .amp (0.001f, 0.4f, 0.8f, 0.5f)
             .master (-5.5f)
             .macro (1, "CRUSH", { { D::FxCrushMix, 0.6f }, { param ("fx_crush_bits"), -0.6f } })
             .macro (2, "RING", { { D::RingMod, 0.4f } })
             .macro (3, "CHOP", { { param ("fx_gate_mix"), -0.8f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.4f }, { D::FxReverbMix, 0.25f } })
             .fx ({ FxGate, FxCrush, FxDelay, FxReverb }).gateSteps (D1_32, "9209070990209009", 0.0f)
             .crush (9.0f, 2.0f, 0.3f).delay (D1_16T, 0.45f, 0.3f, true).reverb (Spring, 0.6f, 0.2f));

    // The Random pattern deals a new set of steps every bar: a choir that never repeats.
    add (B ("Random Gate Choir", "Pad")
             .osc1 (VowelsFemale, 0.3f, 0.6f).unison (1, 5, 12.0f, 0.9f)
             .osc2 (Choir, 0.6f, 0.4f, -12)
             .filter1 (Talking, 1600.0f, 0.4f, 0.4f, 1.2f, 0.3f).morph (1, 0.4f)
             .lfo (1, Chaos, 0.3f).mod (Lfo1, D::Filter1Morph, 0.4f).mod (Lfo1, D::Osc1Frame, 0.25f)
             .amp (0.08f, 0.8f, 0.9f, 1.0f)
             .master (-10.5f)
             .macro (1, "VOWEL", { { D::Filter1Morph, 0.45f } })
             .macro (2, "CHOP", { { param ("fx_gate_mix"), -0.85f } })
             .macro (3, "SMOOTH", { { param ("fx_gate_smooth"), 0.7f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.4f } })
             .fx ({ FxGate, FxFreqShift, FxReverb }).gate (D1_16, 4, 0.1f)
             .set ("fx_shifter_shift", 6.0f).set ("fx_shifter_mix", 0.25f)
             .reverb (Hall, 0.85f, 0.32f));
}
