#pragma once

// The modal pack (2026-11-26, RC 1.0 countdown; ilana: "we need to do this"):
// presets for the three modal exciters, which had five between them and none
// for the Modal Guitar. Each engine first as the real instrument at its
// SIZE / SCALE marks (ModalMarks.h), then pushed past it: the giant bodies
// past SIZE x2, bodies that change per note, the engines through crushers,
// drives, gates and echoes. Every macro moves something you hear at once
// (the engine's own knobs where it can: PLUCK, SIZE, GRIP, WOLF, DECAY).
//
// The engines' knobs as they were fitted: DECAY, STIFF and HAMMER / PLUCK
// at the middle, DAMP at 0 (ModalPiano.h); the Modal Bowed at its own
// defaults, as the Modal Violin. FILTER 1 open, the AMP holding at 1 so the
// engine's own decay and DAMPER set the tail, velocity left to the engine's
// layers (amp velocity 0), as Modal Grand. Appended after every earlier
// preset (indices never move). Included from PresetLibrary.h inside
// Presets::Library.

inline void addModalPresets (const std::function<void (const FactoryPreset&)>& add)
{
    using B = Builder;

    // The Modal Guitar on OSC 1 as fitted: SIZE / SCALE the box and the string.
    const auto guitar = [] (const char* name, const char* category, float size, float scale)
    {
        return B (name, category)
            .string (1, 0.8f, 12, 0.5f, 0.0f)
            .set ("osc1_string_stiffness", 0.5f).set ("osc1_hammer_hard", 0.5f).set ("osc1_damper", 0.35f)
            .set ("osc1_modal_size", size).set ("osc1_modal_scale", scale)
            .set ("osc2_on", 0).set ("sub_on", 0)
            .filter1 (LP, 20000.0f, 0.0f)
            .amp (0.001f, 5.0f, 1.0f, 0.8f).velocity (0.0f);
    };

    // The Modal Bowed on OSC 1 at its defaults (as the Modal Violin).
    const auto bowed = [] (const char* name, const char* category, float size, float scale)
    {
        return B (name, category)
            .set ("osc1_on", 1).set ("osc1_mode", 1).set ("osc1_excite", 13)
            .set ("osc1_modal_size", size).set ("osc1_modal_scale", scale)
            .set ("osc2_on", 0).set ("sub_on", 0).set ("subosc_on", 0)
            .filter1 (LP, 20000.0f, 0.0f)
            .amp (0.0f, 0.3f, 1.0f, 5.0f);
    };

    // The Modal Piano on OSC 1 as Modal Grand plays it.
    const auto piano = [] (const char* name, const char* category, float size, float scale, float hammer)
    {
        return B (name, category)
            .piano (1, 0.8f, hammer, 1, 0.0f, 0.0f, 0.35f, 0.0f, 0.5f, 0.0f, 0.5f)
            .set ("osc1_excite", 11)
            .set ("osc1_modal_size", size).set ("osc1_modal_scale", scale)
            .set ("osc2_on", 0).set ("sub_on", 0)
            .filter1 (LP, 20000.0f, 0.0f)
            .amp (0.001f, 5.0f, 1.0f, 1.0f).velocity (0.0f);
    };

    // ---- Modal Guitar: the real ones ------------------------------------------

    // The fitted nylon-string guitar, in a small room.
    add (guitar ("Nylon Classical", "Pluck", 1.0f, 1.0f)
             .macro (1, "PLUCK", { { param ("osc1_hammer_hard"), 0.45f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.4f } })
             .macro (3, "SIZE", { { param ("osc1_modal_size"), 0.2f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.45f, 0.14f)
             .master (3.3f));

    // A tenor ukulele: the small box and the short string, plucked hard.
    add (guitar ("Tenor Uke", "Pluck", 0.63f, 0.66f)
             .set ("osc1_hammer_hard", 0.7f)
             .macro (1, "PLUCK", { { param ("osc1_hammer_hard"), -0.6f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.4f } })
             .macro (3, "SIZE", { { param ("osc1_modal_size"), 0.2f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.35f, 0.12f)
             .master (7.1f));

    // The guitarron: the Mexican bass guitar, its big box and long string,
    // played where a bass plays.
    add (guitar ("Guitarron Bass", "Bass", 1.3f, 1.1f)
             .set ("osc1_semi", -12.0f).set ("osc1_string_decay", 0.42f)
             .macro (1, "PLUCK", { { param ("osc1_hammer_hard"), -0.45f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.45f } })
             .macro (3, "SIZE", { { param ("osc1_modal_size"), 0.2f } })
             .macro (4, "MUTE", { { param ("osc1_string_damp"), 0.8f }, { param ("osc1_string_decay"), -0.3f } })
             .fx ({ FxComp }).comp (-18.0f, 3.0f, 3.0f)
             .master (5.2f));

    // ---- Modal Guitar: past the real ones ---------------------------------------

    // Every note a different guitar: a random SIZE and SCALE per note, from a
    // ukulele to a guitarron (per-voice knobs, latched at the note's start).
    add (guitar ("Shapeshifter Guitar", "Pluck", 0.9f, 1.0f)
             .mod (Rnd, Target (param ("osc1_modal_size"), 0.22f))
             .mod (Rnd, Target (param ("osc1_modal_scale"), 0.18f))
             .macro (1, "SHIFT", { { param ("osc1_modal_size"), 0.2f } })
             .macro (2, "PLUCK", { { param ("osc1_hammer_hard"), 0.45f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.35f, 0.18f, true).reverb (Hall, 0.55f, 0.15f)
             .master (7.6f));

    // The giant box: SIZE x4 leaves physics, a hollow body the size of a room
    // that blooms and rings on after the key is let go.
    add (guitar ("Cathedral Guitar", "Pad", 4.0f, 1.0f)
             .set ("osc1_damper", 0.1f).set ("osc1_string_decay", 0.7f)
             .amp (0.001f, 5.0f, 1.0f, 3.0f)
             .macro (1, "SIZE", { { param ("osc1_modal_size"), -0.25f } })
             .macro (2, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (3, "PLUCK", { { param ("osc1_hammer_hard"), -0.45f } })
             .macro (4, "SHIMMER", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.2f, 0.3f, 0.25f).reverb (Shimmer, 0.8f, 0.25f)
             .master (7.4f));

    // Liquid drum and bass guitar: a bright pick on a parlour box, a dotted
    // ping-pong echo and a wide chorus.
    add (guitar ("Liquid Nylon", "Pluck", 0.96f, 1.0f)
             .set ("osc1_hammer_hard", 0.65f).set ("osc1_string_decay", 0.4f)
             .macro (1, "PLUCK", { { param ("osc1_hammer_hard"), -0.6f } })
             .macro (2, "ECHO", { { D::FxDelayMix, 0.4f } })
             .macro (3, "WIDTH", { { param ("fx_chorus_mix"), 0.5f } })
             .macro (4, "DECAY", { { param ("osc1_string_decay"), 0.45f } })
             .fx ({ FxChorus, FxDelay, FxReverb }).chorus (0.5f, 0.35f, 0.3f)
             .delay (D1_8D, 0.45f, 0.28f, true).reverb (Plate, 0.5f, 0.15f)
             .master (6.2f));

    // A soprano uke through an 8-bit crusher and a drive: a toy guitar off a
    // broken console.
    add (guitar ("Crushed Uke", "Pluck", 0.47f, 0.51f)
             .set ("osc1_hammer_hard", 0.8f).set ("osc1_string_decay", 0.35f)
             .macro (1, "BITS", { { param ("fx_crush_bits"), -0.5f } })
             .macro (2, "DRIVE", { { D::FxDriveAmount, 0.5f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.45f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.4f } })
             .fx ({ FxCrush, FxDrive, FxDelay }).crush (8.0f, 3.0f, 0.7f).driveFx (2.0f, 0.4f)
             .delay (D1_16, 0.3f, 0.15f, true)
             .master (11.0f));

    // ---- Modal Bowed ----------------------------------------------------------

    // The double bass, bowed (SIZE x3.1, the longest string SCALE has).
    add (bowed ("Modal Double Bass", "Bass", 3.1f, 2.0f)
             .macro (1, "GRIP", { { param ("osc1_bow_pressure"), 0.35f } })
             .macro (2, "SPEED", { { param ("osc1_bow_speed"), 0.35f } })
             .macro (3, "WOLF", { { param ("osc1_bridge_buzz"), 0.5f } })
             .macro (4, "BODY", { { param ("osc1_modal_size"), -0.1f } })
             .master (-3.3f));

    // The cello plucked: PIZZ on, a short room.
    add (bowed ("Pizzicato Cello", "Pluck", 2.1f, 2.0f)
             .set ("osc1_string_slap", 1)
             .amp (0.0f, 0.3f, 1.0f, 0.6f)
             .macro (1, "POSITION", { { param ("osc1_string_excite_pos"), 0.5f } })
             .macro (2, "DAMP", { { param ("osc1_string_damp"), -0.35f } })
             .macro (3, "BODY", { { param ("osc1_modal_size"), -0.15f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Room, 0.4f, 0.15f)
             .master (10.3f));

    // A violin string on a body a quarter of its size: bowed glass, with a
    // slow swell and a shimmer.
    add (bowed ("Bowed Glass", "Pad", 0.25f, 1.0f)
             .set ("osc1_bow_pressure", 0.4f)
             .amp (0.25f, 0.3f, 1.0f, 2.5f)
             .macro (1, "GLASS", { { param ("osc1_modal_size"), 0.25f } })
             .macro (2, "GRIP", { { param ("osc1_bow_pressure"), 0.4f } })
             .macro (3, "SWELL", { { D::AmpAttack, 0.4f } })
             .macro (4, "SHIMMER", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.15f, 0.25f, 0.25f).reverb (Shimmer, 0.75f, 0.3f)
             .master (-5.7f));

    // A cello string on a body the size of a water tank (SIZE x4): the
    // cistern, low and blooming.
    add (bowed ("Cistern Drone", "Drone", 4.0f, 2.0f)
             .set ("osc1_bow_speed", 0.35f)
             .amp (0.4f, 0.3f, 1.0f, 4.0f)
             .lfo (1, SmoothRandom, 0.15f).mod (Lfo1, Target (param ("osc1_bow_pressure"), 0.15f))
             .macro (1, "GRIP", { { param ("osc1_bow_pressure"), 0.55f } })
             .macro (2, "WOLF", { { param ("osc1_bridge_buzz"), 0.8f } })
             .macro (3, "SIZE", { { param ("osc1_modal_size"), -0.25f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.85f, 0.3f, 0.6f)
             .master (-4.0f));

    // A viola section in a hall: a slow bow and a chorus.
    add (bowed ("Modal Chamber Strings", "Pad", 1.17f, 1.13f)
             .amp (0.15f, 0.3f, 1.0f, 2.0f)
             .macro (1, "GRIP", { { param ("osc1_bow_pressure"), 0.35f } })
             .macro (2, "SPEED", { { param ("osc1_bow_speed"), 0.35f } })
             .macro (3, "BODY", { { param ("osc1_modal_size"), 0.2f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxChorus, FxReverb }).chorus (0.25f, 0.3f, 0.3f).reverb (Hall, 0.7f, 0.22f)
             .master (-5.7f));

    // A cello played into a fuzz amp, the bow digging in, the wolf tone
    // up: the scream of a distorted string.
    add (bowed ("Fuzz Cello", "Lead", 2.1f, 2.0f)
             .set ("osc1_bow_pressure", 0.65f).set ("osc1_bridge_buzz", 0.3f)
             .macro (1, "FUZZ", { { param ("fx_amp_drive"), 0.4f } })
             .macro (2, "GRIP", { { param ("osc1_bow_pressure"), 0.3f } })
             .macro (3, "TONE", { { param ("fx_amp_treble"), 0.45f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxAmp, FxReverb }).ampSim (1, 0.5f, 0.5f, 0.6f, 0.45f, 0.5f).reverb (Room, 0.5f, 0.12f)
             .master (-0.4f));

    // ---- Modal Piano --------------------------------------------------------

    // An upright, played soft and damped, with a tape-like wobble: a bedroom piano.
    add (piano ("Bedroom Upright", "Keys", 0.55f, 0.58f, 0.3f)
             .set ("osc1_string_damp", 0.25f)
             .macro (1, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (2, "DAMP", { { param ("osc1_string_damp"), 0.5f } })
             .macro (3, "WOBBLE", { { param ("fx_chorus_mix"), 0.5f }, { param ("fx_chorus_rate"), 0.3f } })
             .macro (4, "ROOM", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxChorus, FxTilt, FxReverb }).chorus (0.6f, 0.2f, 0.25f).set ("fx_tilt", -0.3f)
             .reverb (Room, 0.4f, 0.15f)
             .master (9.3f));

    // The grand on a plate the size of a hall (SIZE x4): it blooms and rings.
    add (piano ("Giant Plate Piano", "Keys", 4.0f, 1.0f, 0.5f)
             .set ("osc1_damper", 0.1f)
             .amp (0.001f, 5.0f, 1.0f, 3.0f)
             .macro (1, "SIZE", { { param ("osc1_modal_size"), -0.25f } })
             .macro (2, "HAMMER", { { param ("osc1_hammer_hard"), 0.4f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.3f } })
             .macro (4, "SPACE", { { D::FxReverbMix, 0.3f } })
             .fx ({ FxReverb }).reverb (Plate, 0.6f, 0.15f)
             .master (9.7f));

    // The grand, crushed and gated in sixteenths: a piano off a broken game.
    add (piano ("Crushed Grand Gate", "Keys", 1.0f, 1.0f, 0.65f)
             .macro (1, "BITS", { { param ("fx_crush_bits"), -0.5f } })
             .macro (2, "GATE", { { param ("fx_gate_mix"), -0.8f } })
             .macro (3, "DECAY", { { param ("osc1_string_decay"), 0.4f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxCrush, FxGate, FxDelay }).crush (6.0f, 4.0f, 0.6f)
             .gateSteps (D1_16, "9090990909909099", 0.15f)
             .delay (D1_8D, 0.3f, 0.15f, true)
             .master (12.0f));

    // ---- Hybrids: two modal engines on one note -------------------------------

    // A nylon pluck with a viola bowing under it on OSC 2: the bow swells in
    // as the pluck dies.
    add (guitar ("Plucked And Bowed", "Pad", 1.0f, 1.0f)
             .set ("osc1_level", 0.7f)
             .set ("osc2_on", 1).set ("osc2_mode", 1).set ("osc2_excite", 13).set ("osc2_level", 0.55f)
             .set ("osc2_modal_size", 1.17f).set ("osc2_modal_scale", 1.13f)
             .set ("osc2_amp_env", 4) // ENV 5 (the e4 envelope): the bow's slow swell
             .env4 (0.7f, 0.5f, 1.0f, 2.0f)
             .amp (0.001f, 5.0f, 1.0f, 2.0f)
             .macro (1, "BOW", { { D::Osc2Level, 0.4f } })
             .macro (2, "SIZE", { { param ("osc1_modal_size"), 0.25f } })
             .macro (3, "SWELL", { { D::E4Attack, 0.4f } })
             .macro (4, "HALL", { { D::FxReverbMix, 0.35f } })
             .fx ({ FxReverb }).reverb (Hall, 0.65f, 0.2f)
             .master (-4.5f));

    // The trance pluck with a guitar's pick: a supersaw on OSC 2 and the
    // Modal Guitar's attack on top, through a fast filter.
    add (guitar ("Nylon Saw Pluck", "Pluck", 0.96f, 1.0f)
             .set ("osc1_hammer_hard", 0.7f)
             .osc2 (Basic, 0.0f, 0.4f).unison (2, 7, 25.0f, 0.8f, Hypersaw)
             .filter1 (LP, 900.0f, 0.15f, 3.5f, 1.2f, 0.4f).fenv (0.001f, 0.25f, 0.0f, 0.25f)
             .amp (0.001f, 0.6f, 0.0f, 0.35f)
             .macro (1, "NYLON", { { D::Osc1Level, 0.3f }, { D::Osc2Level, -0.3f } })
             .macro (2, "CUTOFF", { { D::Filter1Cutoff, 0.4f } })
             .macro (3, "DECAY", { { D::FeDecay, 0.4f }, { D::AmpDecay, 0.3f } })
             .macro (4, "ECHO", { { D::FxDelayMix, 0.35f } })
             .fx ({ FxDelay, FxReverb }).delay (D1_8D, 0.4f, 0.22f, true).reverb (Hall, 0.6f, 0.15f)
             .master (0.0f));
}
