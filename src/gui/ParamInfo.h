#pragma once

#include <juce_core/juce_core.h>
#include "../dsp/OscillatorIds.h"
#include "../dsp/OperatorEgParams.h"

inline bool isOscParameter (const juce::String& id, const char* suffix, bool includeLegacyThird = true)
{
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if ((includeLegacyThird || osc != 2)
            && id == juce::String (OscillatorIds::prefixes[(size_t) osc]) + suffix)
            return true;

    return false;
}

// A number with the decimals its size calls for (under 1: two, under 10:
// one, else none), judged after rounding, so 0.998 shows as "1.0" like 1.0
// does and typed text reads back the same.
// A value rounded to a fixed number of decimals, never "-0.0".
// fx_aw_p1..p5 (the all-in-one) and fx_awsat_p1.. (the category modules).
inline bool isAirwindowsKnob (const juce::String& id)
{
    const auto tail = id.fromLastOccurrenceOf ("_", false, false);
    return id.startsWith ("fx_aw") && tail.length() == 2 && tail[0] == 'p' && juce::CharacterFunctions::isDigit (tail[1]);
}

inline juce::String describeFixed (float value, int decimals)
{
    const auto scale = std::pow (10.0f, (float) decimals);
    const auto rounded = std::round (value * scale) / scale;
    return juce::String (rounded == 0.0f ? 0.0f : rounded, decimals);
}

inline juce::String describeNumber (float value, int maxDecimals = 2)
{
    const auto rounded = [value] (int decimals) { const auto scale = std::pow (10.0f, (float) decimals); return std::round (value * scale) / scale; };
    if (maxDecimals >= 2 && std::abs (rounded (2)) < 1.0f)
        return juce::String (rounded (2), 2);
    if (maxDecimals >= 1 && std::abs (rounded (1)) < 10.0f)
        return juce::String (rounded (1), 1);
    return juce::String (juce::roundToInt (value));
}

inline juce::String describeValue (const juce::String& id, float value)
{
    // The vocoder card.
    if (id == "fx_voc_bands")
        return juce::String (juce::roundToInt (value)) + " bands";
    if (id == "fx_voc_width")
        return "x" + describeNumber (value, 2);
    if (id == "fx_voc_attack" || id == "fx_voc_release")
        return describeNumber (value, 1) + " ms";
    if (id == "fx_voc_formant")
        return (std::round (value * 10.0f) > 0.0f ? "+" : "") + describeFixed (value, 1) + " st";
    if (id == "fx_voc_rate")
        return describeNumber (value, 1) + " Hz";
    if (id == "fx_voc_level")
        return describeFixed (value, 1) + " dB";
    if (id == "fx_voc_unvoiced" || id == "fx_voc_mix")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    // Counts and ratios in their own words.
    if (id == "arp_octaves")
        return juce::String (juce::roundToInt (value)) + " oct";
    if (id == "west_stages")
        return juce::String (juce::roundToInt (value)) + (juce::roundToInt (value) == 1 ? " stage" : " stages");
    if (id == "fx_comp_ratio")
        return describeNumber (value, 1) + ":1";
    if (id == "fx_crush_bits")
        return juce::String (juce::roundToInt (value)) + " bits";
    if (id == "fx_crush_down")
        return juce::String (juce::roundToInt (value)) + "x";
    if (id == "fx_smear_density")
        return juce::String (juce::roundToInt (value)) + " grains";
    if (id.startsWith ("lfo") && id.endsWith ("_seed"))
        return juce::roundToInt (value) == 0 ? juce::String ("Free") : "#" + juce::String (juce::roundToInt (value));
    // Amounts from -1 to 1 as signed percent (mod depths, step values).
    if ((id.startsWith ("mod") && id.endsWith ("_amt")) || (id.startsWith ("lfo") && id.contains ("_step")))
        return (juce::roundToInt (value * 100.0f) > 0 ? "+" : "") + juce::String (juce::roundToInt (value * 100.0f)) + "%";
    // Plain 0..1 (or 0..2) amounts as percent.
    if (id == "fx_feedback_tone" || id == "fx_flanger_depth" || id == "fx_dim_depth" || id == "fx_gate_smooth"
        || id == "fx_trem_depth" || id == "fx_delay_duck" || id == "fx_chorus_depth" || id == "fx_delay_wow"
        || id == "fx_phaser_depth" || id == "fx_reverb_size" || id.startsWith ("fx_taps_step")
        || id == "fx_amp_bass" || id == "fx_amp_mid" || id == "fx_amp_treble"
        || id.endsWith ("_fb_gain") || id.endsWith ("_fb_distance")
        || id == "vec_x" || id == "vec_y" || id.startsWith ("vec_px") || id.startsWith ("vec_py"))
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    // Stereo position: C, or how far left or right.
    if (id.endsWith ("_pan"))
    {
        const auto percent = juce::roundToInt (value * 100.0f);
        return percent == 0 ? juce::String ("C") : (percent < 0 ? "L " : "R ") + juce::String (std::abs (percent));
    }
    // 0 is not a position: the exciter's own spot, or no pick filtering.
    if (id.endsWith ("_string_excite_pos") && juce::roundToInt (value * 100.0f) == 0)
        return "Auto";
    if (id.endsWith ("_string_pick_pos") && juce::roundToInt (value * 100.0f) == 0)
        return "Off";

    // The filters' knobs in their own units.
    if (id == "f1_reso" || id == "f2_reso" || id == "arp_gate")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    if (id == "f1_drive" || id == "f2_drive")
        return describeNumber (value, 1) + "x";
    if (id == "f1_env" || id == "f2_env")
        return (std::round (value * 10.0f) > 0.0f ? "+" : "") + describeFixed (value, 1) + " oct";
    if (id == "f1_keytrack" || id == "f2_keytrack" || id == "f1_fm" || id == "f2_fm")
        return (juce::roundToInt (value * 100.0f) > 0 ? "+" : "") + juce::String (juce::roundToInt (value * 100.0f)) + "%";

    // M8.3: the WEST card.
    if (id == "west_decay")
        return describeFixed (value, 2) + "x";
    if (id == "west_strike" || id == "west_open" || id == "west_fold" || id == "west_res")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    if (id == "west_sym")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    const auto asPercent = [value] { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; };
    const auto asMilliseconds = [value] { return juce::String (juce::roundToInt (value)) + " ms"; };
    const auto asSeconds = [value]
    {
        // Judged after rounding, so 0.9996 s shows as "1.00 s" like 1 s does.
        return juce::roundToInt (value * 1000.0f) < 1000 ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms"
                                                         : juce::String (std::round (value * 100.0f) / 100.0f, 2) + " s";
    };
    const auto asHertz = [value]
    {
        const auto magnitude = std::abs (value);

        // Note juce::String (value, 0) means "default precision", not "no
        // decimals", so whole numbers go through roundToInt.
        return std::round (magnitude) >= 1000.0f ? juce::String (std::round (value / 10.0f) / 100.0f, 2) + " kHz"
                                                 : describeNumber (value) + " Hz";
    };

    if (id == "vec_drift")
        return asPercent();

    if (id == "fx_gate_swing" || (id.startsWith ("fx_gate_step") && id != "fx_gate_steps"))
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    if (id == "fx_gate_steps")
        return juce::String (juce::roundToInt (value)) + " steps";

    // M7.1 generative card.
    if (id == "euc_steps")
        return juce::String (juce::roundToInt (value)) + " steps";
    if (id == "euc_hits")
        return juce::String (juce::roundToInt (value)) + " hits";
    if (id == "euc_rotate")
        return "+" + juce::String (juce::roundToInt (value));
    if (id == "euc_gate" || id == "pseq_gate" || id.startsWith ("pseq_chance"))
        return asPercent();
    if (id == "pseq_length")
        return juce::String (juce::roundToInt (value)) + " steps";
    if (id.startsWith ("pseq_range"))
        return juce::roundToInt (value) == 0 ? juce::String ("root") : "+" + juce::String (juce::roundToInt (value)) + " st";
    if (id.startsWith ("pseq_ratchet"))
        return juce::String (juce::roundToInt (value)) + "x";
    if (id == "spray_strum_time")
        return asMilliseconds();

    if (id == "spray_count")
        return juce::String (juce::roundToInt (value)) + " notes";
    if (id == "sym_count")
        return juce::String (juce::roundToInt (value)) + " strings";
    if (id == "sym_decay")
        return asPercent();
    // M7.5 audio input.
    if (id == "in_attack" || id == "in_release")
        return describeNumber (value, 1) + " ms";
    if (id == "in_gain" || id == "in_threshold")
        return describeFixed (value, 1) + " dB";
    if (id == "in_dry" || id == "in_body" || id == "in_strings")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    if (id.startsWith ("sym_note") || id == "in_note")
    {
        // Note name with C3 = MIDI 60, as on the keyboard strip.
        static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto note = juce::roundToInt (value);
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 2);
    }

    if (id == "spray_range")
        return juce::String (juce::roundToInt (value)) + " st";

    // M5 operators, M6 phase distortion, DAHDSR extras.
    if (isOscParameter (id, "_ratio"))
        return "x" + (std::abs (std::round (value * 1000.0f) / 1000.0f) < 10.0f ? juce::String (std::round (value * 1000.0f) / 1000.0f, 3)
                                                                                  : juce::String (std::round (value * 100.0f) / 100.0f, 2));
    if (isOscParameter (id, "_fixed_hz"))
        return std::round (value) >= 1000.0f ? juce::String (std::round (value / 10.0f) / 100.0f, 2) + " kHz"
                                             : (std::round (value * 100.0f) / 100.0f < 100.0f ? juce::String (std::round (value * 100.0f) / 100.0f, 2)
                                                                                           : juce::String (std::round (value * 10.0f) / 10.0f, 1)) + " Hz";
    if (isOscParameter (id, "_key_level"))
        return (std::round (value * 60.0f) > 0.0f ? "+" : "") + describeFixed (value * 6.0f, 1) + " dB/oct";
    // The Operator EG's 0-99 values are plain numbers (DELAY isn't seconds).
    if (id == "opeg_lfo_delay" || id == "opeg_lfo_speed")
        return juce::String (juce::roundToInt (value));
    // DX7 break points: 0 is A-1, 39 is C3 (middle C).
    if (isOscParameter (id, "_eg_break"))
        return juce::MidiMessage::getMidiNoteName (juce::roundToInt (value) + 21, true, true, 3);
    if (id == OperatorEg::keyOffsetId)
        return (juce::roundToInt (value) > 0 ? "+" : "") + juce::String (juce::roundToInt (value)) + " st";
    if (isOscParameter (id, "_warp2_amt"))
        return asPercent();
    if (isOscParameter (id, "_pd_env_amt"))
        return (juce::roundToInt (value * 100.0f) > 0 ? "+" : "") + juce::String (juce::roundToInt (value * 100.0f)) + "%";
    if (id.endsWith ("_delay") && ! id.startsWith ("fx_"))
        return value <= 0.0005f ? juce::String ("Off") : asSeconds();
    if (id.endsWith ("_hold"))
        return value <= 0.0005f ? juce::String ("Off") : asSeconds();
    if (id.endsWith ("_keyrate"))
        return asPercent();

    if (id == "spray_spread")
        return juce::String (juce::roundToInt (value)) + " ms";

    if (id.startsWith ("fm_") && id != "fm_mode")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    if (id == "spray_chance" || id == "spray_velocity" || id == "arp_chance")
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";

    if (id == "filter_balance")
    {
        if (juce::roundToInt (value * 100.0f) == 0)
            return "F1 = F2";

        return value < 0.0f ? "F1 +" + juce::String (juce::roundToInt (-value * 100.0f)) + "%"
                            : "F2 +" + juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }

    if (id.endsWith ("_cutoff") || id.endsWith ("_freq") || id.endsWith ("_rate")
        || id == "lfo1_rate" || id == "lfo2_rate" || id == "mseg_rate" || id == "fx_shifter_shift")
        return asHertz();

    if (id == "fx_delay_time" || id == "fx_delay_time_r" || id == "fx_feedback_delay"
        || id == "fx_smear_size" || id == "fx_tape_stop_time" || id == "fx_haas_delay"
        || id == "fx_comp_attack" || id == "fx_comp_release" || id == "fx_limit_release")
        return asMilliseconds();

    if (id.endsWith ("_grain_size"))
        return juce::String (juce::roundToInt (value)) + " ms";

    if (id == "poly_voices")
        return juce::String (juce::roundToInt (value));

    if (id.endsWith ("_attack") || id == "glide"
        || ((id.endsWith ("_decay") || id.endsWith ("_release"))
            && ! id.endsWith ("_string_decay") && id != "res_decay"))
        return asSeconds();

    if (id.endsWith ("_semi") || id.endsWith ("_fine") || id.endsWith ("_detune")
        || id == "fx_delay_pitch" || id == "fx_stutter_pitch" || id == "res_offset" || id == "bend_range")
        return juce::String (std::round (value * 10.0f) / 10.0f, std::round (value * 10.0f) == 10.0f * std::round (value) ? 0 : 1)
               + (id.endsWith ("_fine") || id.endsWith ("_detune") ? " ct" : " st");

    if (id == "master" || id == "master_clip_gain" || id == "fx_limit_ceiling" || id == "fx_tilt_level"
        || id == "fx_comp_makeup" || id == "fx_comp_threshold" || id == "fx_util_gain"
        || (id.startsWith ("fx_eq_") && id.endsWith ("_gain")))
        return describeFixed (value, 1) + " dB";

    if (id == "fx_drive_amount" || id == "fx_amp_drive")
        return describeNumber (value, 1) + "x";

    if (id.endsWith ("_mix") || id.endsWith ("_amount") || id.endsWith ("_level") || id.endsWith ("_width")
        || id.endsWith ("_spread") || id.endsWith ("_sustain") || id.endsWith ("_velocity")
        || id.endsWith ("_damping") || id.endsWith ("_feedback") || id.endsWith ("_curve")
        || id.endsWith ("_frame") || id.endsWith ("_start") || id.endsWith ("_end")
        || id.endsWith ("_fade_in") || id.endsWith ("_fade_out") || id == "drift" || id == "ring_mod"
        || id == "fm_amount" || id == "fm_feedback" || id == "fx_fold" || id == "res_amount"
        || id == "res_keytrack" || id == "fx_tilt" || id == "fx_shifter_mix" || isAirwindowsKnob (id)
        || id == "noise_level" || id == "unison_random" || id == "voice_spread"
        || id == "body_material" || id == "body_size" || id == "body_coupling"
        || id.startsWith ("macro") || id.startsWith ("mseg_level")
        || id == "res_decay" || id.endsWith ("_string_decay") || id.endsWith ("_string_damp")
        || id.endsWith ("_string_stiffness") || id.endsWith ("_string_pickup") || id.endsWith ("_string_excite_pos")
        || id.endsWith ("_string_pick_hardness") || id.endsWith ("_string_pick_pos")
        || id.endsWith ("_phys_a") || id.endsWith ("_phys_b")
        || (id.startsWith ("lfo") && (id.endsWith ("_smooth") || id.endsWith ("_stereo")))
        || id.endsWith ("_bow_pressure") || id.endsWith ("_bow_speed")
        || id.endsWith ("_bridge_buzz") || id.endsWith ("_fret_rattle")
        || id.endsWith ("_hammer_hard") || id.endsWith ("_couple") || id.endsWith ("_damper") || id.endsWith ("_register")
        || id.endsWith ("_ep_distance") || id.endsWith ("_ep_position")
        || id == "stretch" || id.startsWith ("sb_") || id == "pedal_res" || id.startsWith ("mech_")
        || id == "sym_amount" || id == "sym_decay"
        || id.endsWith ("_warp_amt") || id.endsWith ("_spectral_amt") || id.endsWith ("_grain_density") || id.endsWith ("_grain_spray")
        || id.endsWith ("_grain_pitch") || id.endsWith ("_grain_spread") || id.endsWith ("_uni_blend") || id.endsWith ("_phase") || id.endsWith ("_morph"))
        return asPercent();

    // Whole numbers without decimals, judged after rounding to two places
    // (0.997 is "1", as 1.0 is), so text and value round-trip.
    const auto rounded = std::round (value * 100.0f) / 100.0f;
    return rounded == std::floor (rounded) ? juce::String (juce::roundToInt (rounded)) : juce::String (rounded, 2);
}

inline juce::String describeParameter (const juce::String& id)
{
    // M8.1: the simulated LFO shapes (any of the 16 LFOs).
    if (id.startsWith ("lfo"))
    {
        const auto suffix = id.fromFirstOccurrenceOf ("_", false, false);
        if (suffix == "smooth")
            return "Glide on the LFO's output, as a fraction of a cycle. Turns S&H, Steps and Square into slewed "
                   "random or glide. Works on every shape.";
        if (suffix == "trigger")
            return "What restarts a simulated shape: Note (each note), Free (never), Beat (each DIVISION of the host's "
                   "beat) or Generative (Euclid's hits, else the probability sequencer's steps). FIRE triggers it by hand.";
        if (suffix == "axis")
            return "Which axis of the attractor output A carries; output B carries the next one (X then Y, Y then Z, "
                   "Z then X). Mix puts X and Z together on A, and Y on B. The graph names both.";
        if (suffix == "loop")
            return "Physics objects: start again once settled, instead of resting until the next trigger.";
        if (suffix == "seed")
            return "Which random sequence the shape plays. Free (0): every voice and every note gets its own. "
                   "1-999: the same repeatable sequence everywhere, restarting on each trigger (or FIRE).";
        if (suffix == "stereo")
            return "Random shapes: how far output B departs from A (route B to the other side, or another target).";
        if (suffix == "fire")
            return "Triggers the LFO now.";
        if (suffix.length() == 2 && suffix[0] == 'p')
            return "The chosen shape's own parameter; its name and unit show on the knob (gravity, length, sigma...).";
    }

    // M7.1 generative card.
    if (id == "euc_on")
        return "Euclidean rhythm: HITS spread as evenly as possible over STEPS.";
    if (id == "euc_target")
        return "Notes: gates the arp's steps (with the arp off, plays the held chord on each hit). "
               "Exciter: re-strikes Physical strings on each hit. Trance Gate: the gate effect follows the rhythm.";
    if (id == "euc_steps")
        return "Length of the rhythm in steps.";
    if (id == "euc_hits")
        return "How many steps play, spread evenly over the length.";
    if (id == "euc_rotate")
        return "Shifts the rhythm's start by this many steps.";
    if (id == "euc_div")
        return "Length of one step, synced to the host tempo.";
    if (id == "euc_gate")
        return "How long each hit holds its notes, as a share of the step.";
    if (id == "pseq_on")
        return "Probability sequencer: plays the held keys through 16 steps, each with a chance, a pitch range and "
               "a ratchet, snapped to the scale. While on, it takes over from the arpeggiator.";
    if (id == "pseq_div")
        return "Length of one step, synced to the host tempo.";
    if (id == "pseq_length")
        return "How many of the 16 steps loop.";
    if (id == "pseq_gate")
        return "How long each note holds, as a share of its step (or ratchet).";
    if (id.startsWith ("pseq_chance"))
        return "The chance this step plays.";
    if (id.startsWith ("pseq_range"))
        return "How far above the held note this step may land, in semitones, snapped to the scale.";
    if (id.startsWith ("pseq_ratchet"))
        return "Repeats within the step: 1 plays once, 4 plays four quick notes.";
    if (id == "spray_strum")
        return "Strums chords (and sprayed notes): notes starting together are spread out, lowest first (Up) or highest first (Down).";
    if (id == "spray_strum_time")
        return "Time between strummed notes.";

    if (id == "quality")
        return "Eco caps each oscillator at four unison voices; Normal preserves the original engine; "
               "High averages two wavetable reads per sample for smoother highs.";
    if (isOscParameter (id, "_amp_env"))
        return "Envelope controlling this oscillator's level (as an FM operator, how deep it modulates). "
               "ENV 1 is the original AMP envelope; MSEG runs the MSEG shape once per note; Op EG is the Operator EG, the "
               "DX7's envelope generator, set on the FM page (rates, levels, keyboard scaling, velocity).";

    // The Operator EG (FM page), DX7 style: 0-99 values as on a DX7.
    for (int stage = 1; stage <= 4; ++stage)
    {
        if (isOscParameter (id, ("_eg_r" + juce::String (stage)).toRawUTF8()))
            return "Operator EG rate " + juce::String (stage) + " (0-99): how fast it moves to level " + juce::String (stage)
                   + (stage == 4 ? ", after the key is released" : "") + ". 99 is instant, 0 never moves.";
        if (isOscParameter (id, ("_eg_l" + juce::String (stage)).toRawUTF8()))
            return "Operator EG level " + juce::String (stage) + " (0-99, about 0.75 dB a step near the top)"
                   + (stage == 3 ? ": held while the key is down." : stage == 4 ? ": where the release ends." : ".");
        if (id == "opeg_pitch_r" + juce::String (stage))
            return "Operator EG pitch envelope rate " + juce::String (stage) + " (0-99)"
                   + (stage == 4 ? ", after the key is released." : ".");
        if (id == "opeg_pitch_l" + juce::String (stage))
            return "Operator EG pitch envelope level " + juce::String (stage) + ": 50 is the note, 0 and 99 about four "
                   "octaves down and up. Moves every ratio oscillator on the Operator EG.";
    }
    if (isOscParameter (id, "_eg_out"))
        return "Operator EG output level (0-99, as on a DX7): how loud a carrier is, how deep a modulator modulates. "
               "LEVEL scales it on top (0.5 is this level).";
    if (isOscParameter (id, "_eg_break"))
        return "Keyboard scaling break point: the key where LEFT and RIGHT DEPTH start (39 is C3, as on a DX7).";
    if (isOscParameter (id, "_eg_ldepth"))
        return "How much the level changes below the break point, along L CURVE.";
    if (isOscParameter (id, "_eg_rdepth"))
        return "How much the level changes above the break point, along R CURVE.";
    if (isOscParameter (id, "_eg_lcurve") || isOscParameter (id, "_eg_rcurve"))
        return "Keyboard scaling curve away from the break point: -LIN and -EXP get quieter, +EXP and +LIN louder.";
    if (isOscParameter (id, "_eg_rate_key"))
        return "Rate scaling (0-7): higher notes run this operator's envelope faster.";
    if (isOscParameter (id, "_eg_vel"))
        return "Velocity sensitivity (0-7): how much softer playing lowers this operator's level.";
    if (isOscParameter (id, "_eg_ams"))
        return "Amp modulation sensitivity (0-3): how much the Operator EG LFO's AMP DEPTH moves this operator.";
    if (id == "opeg_lfo_speed")
        return "Operator EG LFO speed (0-99, about 0.06 to 49 Hz), the DX7's LFO.";
    if (id == "opeg_lfo_delay")
        return "Operator EG LFO delay (0-99): how long after the key the LFO fades in.";
    if (id == "opeg_lfo_pmd")
        return "Operator EG LFO pitch depth: vibrato on every ratio oscillator on the Operator EG (scaled by PITCH SENS).";
    if (id == "opeg_lfo_amd")
        return "Operator EG LFO amp depth: tremolo or wah on the oscillators whose AMS is above 0.";
    if (id == "opeg_lfo_sync")
        return "Operator EG LFO key sync: each note starts the LFO from the top of its cycle.";
    if (id == "opeg_lfo_wave")
        return "Operator EG LFO wave: triangle, saw down, saw up, square, sine or sample and hold.";
    if (id == "opeg_lfo_pms")
        return "Operator EG LFO pitch sensitivity (0-7): how far PITCH DEPTH bends the pitch.";
    if (id == OperatorEg::keyOffsetId)
        return "Moves the key the Operator EG's keyboard and rate scaling follow, in semitones (a DX7 voice's "
               "transpose; the pitch itself is in each oscillator's SEMI).";
    if (isOscParameter (id, "_tune"))
        return "How the operator is tuned: in semitones (as before), as a ratio of the played note, "
               "or at a fixed frequency that ignores the keyboard (for drums and formants). SEMI and FINE still apply.";
    if (isOscParameter (id, "_ratio"))
        return "Frequency ratio to the played note, used when TUNING is Ratio. SNAP pulls it to the nearest ratio of a set.";
    if (isOscParameter (id, "_ratio_snap"))
        return "Ratio set: Harmonic (whole numbers, clean tones), Inharmonic (square roots: metallic), "
               "Bell (partials of a tuned bell and a struck bar), or Free.";
    if (isOscParameter (id, "_fixed_hz"))
        return "The operator's frequency when TUNING is Fixed Hz.";
    if (isOscParameter (id, "_key_level"))
        return "Level key scaling: louder (positive) or quieter (negative) towards the top of the keyboard, "
               "up to 6 dB per octave from C3. On a modulator it keeps FM brightness even across the keys.";
    if (isOscParameter (id, "_fb_type"))
        return "Feedback style for this operator's FB cell: Plain, Filtered (smoothed like a DX7, calmer at high "
               "amounts), Cross (the amount runs between this oscillator and its pair: 1-2, 3-4, 5-6), or DX7 "
               "(the DX7's own: the average of the last two samples).";
    if (isOscParameter (id, "_warp2"))
        return "The second stage of the PD chain: a second warp applied after the first.";
    if (isOscParameter (id, "_warp2_amt"))
        return "How far the second warp stage bends the wave.";
    if (isOscParameter (id, "_pd_env"))
        return "An envelope that opens the warp, like the Casio CZ's DCW: it adds to both warp amounts.";
    if (isOscParameter (id, "_pd_env_amt"))
        return "How far the warp envelope moves the warp amounts (negative closes them).";
    if (id.startsWith ("fm_noise") && id != "fm_noise_color")
        return "The noise operator: how much noise frequency-modulates this oscillator (breath, grit, cymbals).";
    if (id == "fm_noise_color")
        return "Noise operator colour: dark rumble to full white noise.";
    if (id.endsWith ("_delay") && ! id.startsWith ("fx_"))
        return "DAHDSR: a wait after the note starts before the attack.";
    if (id.endsWith ("_hold"))
        return "DAHDSR: how long the envelope holds at the peak before the decay.";
    if (id.endsWith ("_keyrate"))
        return "Rate key scaling: every stage gets shorter up the keyboard (at 100%, half as long per octave above C3).";
    if (id.startsWith ("env") && juce::isPositiveAndBelow (id.substring (3).getIntValue() - 6, 11))
    {
        if (id.endsWith ("_velocity")) return "How strongly note velocity scales this envelope when used as a source.";
        if (id.endsWith ("_curve")) return "Envelope tension: positive reaches the target early, negative reaches it late.";
        return "ADSR stage for this per-voice envelope. Drag its chip onto a knob, route it in MATRIX, or pick it as an oscillator amp envelope.";
    }
    if (id == "sym_on") return "Turns on the shared drone strings after the voices and before effects.";
    if (id == "sym_amount") return "How much the shared strings ring in the mix.";
    if (id == "sym_decay") return "How long the sympathetic strings ring after the excitation stops: from a quarter second to 12 seconds.";
    if (id == "sym_count") return "Number of shared drone strings, from one to six.";
    if (id == "sym_manual") return "Tunes the strings note by note instead of from the GENERATE scale and root. "
                                   "With no scale set they use an open tuning on the root (root, fifth, octave, third).";
    if (id.startsWith ("sym_note")) return "Manual tuning for this drone string, used when MANUAL is on.";
    if (id.endsWith ("_bow_pressure")) return "Bow grip on the string. MPE pressure and channel aftertouch add to it.";
    if (id.endsWith ("_bow_speed")) return "Bow travel speed; changes the sustained tone and scrape.";
    if (id.endsWith ("_bridge_buzz")) return "Nonlinear bridge contact, from clean to sitar-like buzz.";
    if (id.endsWith ("_fret_rattle")) return "Velocity-scaled fret contact noise. Zero is clean.";
    if (isOscParameter (id, "_hammer_hard"))
        return "Hammer felt hardness (Hammer and Piano exciters). Harder felt and faster keys give a shorter contact and a brighter tone.";
    if (id == "in_gain") return "ilanaSynth FX: the input's level into the engine (DRY is not affected).";
    if (id == "in_dry") return "ilanaSynth FX: the untouched input, added back at the end.";
    if (id == "in_body") return "ilanaSynth FX: how hard the input rings the BODY section (switch BODY on, any type but Classic).";
    if (id == "in_strings") return "ilanaSynth FX: how hard the input drives Physical strings, tines and reeds while a note plays.";
    if (id == "in_trigger") return "ilanaSynth FX: Off plays on MIDI notes only; Gate plays NOTE while the input is over THRESHOLD; Drone holds NOTE down.";
    if (id == "in_threshold") return "ilanaSynth FX: the level that opens the gate (it closes 6 dB below).";
    if (id == "in_note") return "ilanaSynth FX: the note the gate or drone plays. Strings, bodies and oscillators tune to it.";
    if (id == "in_attack") return "ilanaSynth FX: how fast the input envelope (Input Env) rises.";
    if (id == "in_release") return "ilanaSynth FX: how fast the input envelope falls (a time constant).";
    if (isOscParameter (id, "_grain_live"))
        return "ilanaSynth FX: grains read the last three seconds of the input instead of the sample; POSITION is how far back.";
    if (isOscParameter (id, "_ep_distance"))
        return "Tine / Reed: how close the pickup sits. Closer makes hard notes bark (tine) or growl (reed).";
    if (isOscParameter (id, "_ep_position"))
        return "Tine / Reed: the pickup's offset from the tine or reed. It changes the balance of even and odd harmonics.";
    if (isOscParameter (id, "_couple"))
        return "The note's strings share the bridge: set UNISON to 2-3 and a small DETUNE for a fast first decay "
               "and a long, beating aftersound, as in a piano.";
    if (isOscParameter (id, "_damper")) return "How quickly the dampers stop the string once the key (and the sustain pedal) is up.";
    if (isOscParameter (id, "_register"))
        return "Changes the string across the keyboard: stiffer and brighter in the treble, looser and longer in the bass.";
    if (id == "stretch") return "Piano stretch tuning: bass slightly flat, treble slightly sharp, as a tuner does for real pianos.";
    if (id == "tuning_on") return "Plays the Scala scale (and keyboard mapping) loaded from the settings menu instead of 12-TET. STRETCH still applies on top.";
    if (id == "clip_on") return "Clip sequencer: plays the notes of the chosen clip. Off leaves the keyboard alone.";
    if (id == "clip_index") return "Which of the eight clips plays and is shown in the piano roll.";
    if (id == "clip_mode")
        return "Key transpose: a held key plays the clip, transposed from C3 (the clip as written), and gates it, like the arp. "
               "Host play: the clip plays in sync with the host's transport while it runs.";
    if (id == "sb_on") return "A soundboard body after the voices: wooden modes driven by the strings.";
    if (id == "west_on") return "The west-coast voice: a wavefolder into a low-pass gate (a vactrol-driven filter and amplifier in one).";
    if (id == "west_pos") return "After Filters: WEST processes the filters' output. Replace Filter 2: WEST takes Filter 2's place.";
    if (id == "west_fold") return "How hard the wavefolder folds: 0 is almost clean, 100% folds about a dozen times.";
    if (id == "west_sym") return "Offsets the fold: even harmonics, a hollower or reedier tone.";
    if (id == "west_stages") return "Folders in a row: more stages, denser harmonics.";
    if (id == "west_mode") return "Combo: filter and amplifier together (the classic bongo). Low Pass: the filter only. VCA: the level only.";
    if (id == "west_decay") return "How long the vactrol takes to go dark, as a multiple of its own (about 250 ms to 63%, slower as it darkens).";
    if (id == "west_res") return "Resonance of the gate's filter.";
    if (id == "west_strike") return "How hard the gate is struck (times velocity for a note strike, or the chosen source's level).";
    if (id == "west_open") return "Holds the gate partly open, so notes sustain under the strikes.";
    if (id == "west_src") return "What strikes the gate: each note, or any mod source (an envelope, or LFO n B with Bounce for its impacts).";
    if (id == "sb_model") return "Classic: the M4 board. Dense: 48 wooden modes a side and the colour measured from a real grand (for the Piano exciter).";
    if (id == "sb_mix") return "How much soundboard resonance is heard.";
    if (id == "sb_tone") return "Lid and mic position: closed and dark to open and bright.";
    if (id == "sb_size") return "Soundboard size: a bigger board is lower and rings longer.";
    if (id == "pedal_res")
        return "With the sustain pedal (CC64) down, the whole keyboard's strings ring in sympathy, shared by all voices.";
    if (id == "mech_key") return "The wooden thock of a key returning when you let it go.";
    if (id == "mech_damper") return "The felt of a damper landing on the strings.";
    if (id == "mech_pedal") return "The sustain pedal mechanism, on press and release.";
    // Oscillators (patterned)
    if (isOscParameter (id, "_table", false))
        return "Factory wavetable. Load your own with LOAD WAVETABLE (.wav).";

    if (isOscParameter (id, "_frame", false))
        return "Morphs through the table's frames. Modulate for movement.";

    if (isOscParameter (id, "_level", false))
        return "Oscillator output level.";

    if (isOscParameter (id, "_pan", false))
        return "Stereo position of the oscillator.";

    if (isOscParameter (id, "_semi", false))
        return "Pitch offset in semitones. With Hard Sync this sets the sync ratio.";

    if (isOscParameter (id, "_fine", false))
        return "Fine pitch offset in cents.";

    if (isOscParameter (id, "_unison", false))
        return "Stacked detuned copies of the oscillator. More = bigger, costlier.";

    if (isOscParameter (id, "_detune", false))
        return "Unison spread in cents. 20-40 is a classic supersaw.";

    if (isOscParameter (id, "_spread", false))
        return "Stereo spread of the unison stack.";

    if (isOscParameter (id, "_mode", false))
        return "Wavetable, Physical (a string, or the Tine and Reed electric pianos), a sample you drag onto the card, "
               "Granular: a cloud of tiny grains read from that sample (a vocal until you load your own), "
               "or Live: the audio coming into ilanaSynth FX.";

    if (isOscParameter (id, "_on"))
        return "Turns this oscillator on and off. Switching fades in/out so it stays click-free.";

    if (isOscParameter (id, "_excite", false))
        return "String excitation: Burst plucks, Noise/Saw/Pulse sustain the string. "
               "Tine and Reed swap the string for an electric piano: a Rhodes-style tine or a Wurlitzer-style reed. "
               "Piano strikes the string with a real felt hammer (it brightens as it compresses), with two polarisations "
               "and the bass's bark; Hammer (classic) is the M4 model, kept for old patches.";

    if (isOscParameter (id, "_string_decay", false))
        return "How long the string rings.";

    if (isOscParameter (id, "_string_damp", false))
        return "Loop damping - higher is darker and more muted.";

    if (isOscParameter (id, "_string_sustain", false))
        return "Level of the continuous excitation (ignore for Burst).";

    if (id.endsWith ("_string_stiffness"))
        return "String stiffness. Higher values push upper harmonics sharp.";

    if (id.endsWith ("_string_pickup"))
        return "Pickup position. Move it to change the string's harmonic notches.";

    if (id.endsWith ("_string_excite_pos"))
        return "Excitation position on the string. Changes which harmonics ring. Auto (at 0) uses the exciter's natural spot, about an eighth of the string.";

    if (id.endsWith ("_string_pick_hardness"))
        return "Pick hardness. Harder picks give a brighter attack.";

    if (id.endsWith ("_string_pick_pos"))
        return "Where the pick strikes the string. Changes the attack spectrum. Off at 0.";

    if (id.endsWith ("_string_slap"))
        return "Adds a short noisy slap to the start of each pluck.";

    if (id == "sub_level")
        return "Oscillator 3 level.";

    if (id == "subosc_level")
        return "Sub oscillator an octave (or two) below the note.";

    if (id == "subosc_on")
        return "Turns the sub oscillator on.";

    if (id == "subosc_route")
        return "Where the sub and the noise enter the filters.";

    if (id == "sub_shape")
        return "Sub oscillator shape: Sine for clean weight, Square/Saw for grit.";

    if (id == "sub_octave")
        return "How far below the played note the sub sits.";

    if (id == "noise_level")
        return "White noise level - hats, breath, destruction.";

    // Cross modulation
    if (id == "fm_amount")
        return "Osc2 phase-modulates Osc1. Raise Osc2 Level to hear it.";

    if (id == "fm_feedback")
        return "Osc1 modulates its own phase. Noisy, chaotic FM.";

    if (id == "ring_mod")
        return "Ring-modulates Osc1 with Osc2 - metallic and clangy.";

    if (id == "hard_sync")
        return "Resets Osc2 phase every Osc1 cycle. Tune with Osc2 SEMI for screams.";

    if (id == "f1_fm" || id == "f2_fm")
        return "Audio-rate cutoff modulation from Osc2. Growl and filter-FM.";

    // Chords & voice
    if (isOscParameter (id, "_chord", false))
        return "Spreads a chord across the unison voices. Needs Unison above 1.";

    if (id == "voice_spread")
        return "Random stereo position per voice - instant width for pads.";

    if (id == "unison_random")
        return "Randomises unison start phases for a softer, phasey attack.";

    if (id == "mpe_mode")
        return "Per-note bend and pressure via MIDI channels, +-48 st bend range.";

    if (id == "drift")
        return "Analog-style per-voice pitch instability.";

    // Filters
    if (id == "f1_type" || id == "f2_type")
        return "SVF: clean Low / Band / High Pass and Notch.  Ladder: Moog-style, saturating and self-oscillating.  "
               "Diode: 303-style squelch.  MS-20: gnarly Sallen-Key.  Comb +/-: tuned to the cutoff, metallic "
               "to hollow.  Formant: vowels (MORPH picks A-E-I-O-U).  Morph: sweeps LP > BP > HP with MORPH.";

    if (id == "f1_morph" || id == "f2_morph")
        return "Formant: moves through the vowels A, E, I, O, U.  Morph: blends low-pass into band-pass into "
               "high-pass.  Try an LFO on it.";

    if (id.startsWith ("fx_eq_"))
        return "Three-band EQ: low shelf, a sweepable mid peak (Q sets its width) and a high shelf.  "
               "Drag the points on the curve too.";

    if (id.endsWith ("_route"))
        return "Which filter this oscillator goes through.  Default follows the Serial / Parallel switch; "
               "Filter 2 skips Filter 1; No filter goes straight to the amp.";

    if (id == "f1_slope" || id == "f2_slope")
        return "12 dB is gentle, 24 dB is steep and aggressive.";

    if (id == "f1_cutoff" || id == "f2_cutoff")
        return "Filter frequency. Drag the marker on the response display too.";

    if (id == "f1_reso" || id == "f2_reso")
        return "Emphasis at the cutoff. High values scream; near the top the filter rings on its own "
               "(self-oscillation) and can be played as a sine.";

    if (id == "f1_drive" || id == "f2_drive")
        return "Saturates the signal going into the filter.";

    if (id == "f1_env" || id == "f2_env")
        return "How far the filter envelope moves the cutoff, in octaves (negative closes it).";

    if (id == "f1_keytrack" || id == "f2_keytrack")
        return "Cutoff follows the played note (100% = full tracking: an octave up the keyboard moves it an octave).";

    if (id == "fm_mode")
        return "Phase: classic FM. Through-Zero: bends the pitch, even backwards. Exponential: pitch FM in octaves.";

    if (isOscParameter (id, "_out"))
        return "Off makes this oscillator a silent modulator: it still drives FM but isn't heard.";

    if (id.startsWith ("fm_fb") || id == "fm_feedback")
        return "The oscillator modulates itself: brighter, then noisier.";

    if (id.startsWith ("fm_") && id != "fm_mode")
        return "How hard one oscillator frequency-modulates another. Try whole-number pitch ratios for bells and keys.";

    if (id == "gen_scale")
        return "Scale that sprayed notes and the Scale Random arp snap to.";

    if (id == "gen_snap")
        return "Also snap the notes you play to the scale.";

    if (id == "spray_on")
        return "Each played note throws extra notes around itself.";

    if (id == "spray_count")
        return "How many extra notes each played note can spray.";

    if (id == "spray_range")
        return "How far (in semitones) the extra notes can land from the played note.";

    if (id == "spray_spread")
        return "Spreads the extra notes out in time, up to this long after the played note.";

    if (id == "spray_chance")
        return "Chance that each extra note actually plays.";

    if (id == "spray_velocity")
        return "Randomises the extra notes' velocity.";

    if (id == "arp_chance")
        return "Chance that each arp step plays; lower it for gaps and generative patterns.";

    if (id == "fx_gate_steps")
        return "Trance gate pattern length. DIV sets the length of each step.";

    if (id == "fx_gate_swing")
        return "Pushes every second step later for a shuffled gate.";

    if (id == "filter_balance")
        return "Parallel mode: fades between Filter 1 and Filter 2. Centre plays both.";

    if (id == "filters_parallel")
        return "Serial: F1 into F2. Parallel: both filters summed.";

    // Envelopes
    if (id == "amp_attack" || id == "fe_attack" || id == "me_attack")
        return "Time to reach full level. Longer = softer entry.";

    if (id == "amp_decay" || id == "fe_decay" || id == "me_decay")
        return "Time to fall from the peak to the sustain level.";

    if (id == "amp_sustain" || id == "fe_sustain" || id == "me_sustain")
        return "Held level while the note is down.";

    if (id == "amp_release" || id == "fe_release" || id == "me_release")
        return "Time to fade after the note is released.";

    if (id == "amp_velocity" || id == "filter_velocity")
        return "How strongly velocity scales the envelope.";

    if (id == "glide")
        return "Portamento time between notes.";

    if (id.endsWith ("_warp"))
        return "Bends how the oscillator reads its wavetable: Sync squeezes cycles in, Bend pushes the wave "
               "forwards or back, PWM squashes it into part of the cycle, Mirror plays it there and back, Asym "
               "skews it, Quantize steps it, FM and Ring use another oscillator (OSC 2 for OSC 1, OSC 1 for the others). "
               "PD modes are Casio CZ phase distortion (try the Sine table): Saw, Square and Pulse bend a sine into "
               "those shapes; Res I-III are the CZ's resonant waves, a sweepable formant.";

    if (id.endsWith ("_warp_amt"))
        return "How hard the warp bends the wave. Modulate it for movement.";

    if (id.endsWith ("_grain_size"))
        return "Length of each grain. Short grains buzz and blur into texture; long ones keep the sample recognisable.";

    if (id.endsWith ("_grain_density"))
        return "How many grains overlap: from sparse, stuttering single grains to a thick, continuous cloud.";

    if (id.endsWith ("_grain_spray"))
        return "Random scatter of each grain's start around the position. More spray smears the sample into a cloud.";

    if (id.endsWith ("_grain_pitch"))
        return "Random detune per grain, up to an octave either way. A little shimmers; a lot sounds like a swarm.";

    if (id.endsWith ("_grain_spread"))
        return "How far grains are scattered across the stereo field.";

    if (id.endsWith ("_spectral"))
        return "Reshapes the wavetable's harmonics rather than its waveform: Stretch pulls them apart into bells and "
               "metal, Shift moves them all up for hollow ring-mod tones, Odd/Even keeps odd then even harmonics, "
               "Formant moves the vowel without changing pitch, Smear blurs them into a haze, Harmonic Cut keeps only "
               "the lowest. Works on the factory tables.";

    if (id.endsWith ("_spectral_amt"))
        return "How far the spectral warp goes. For Formant, the middle leaves the vowel where it was. "
               "Changes rebuild the table in the background, so this isn't a modulation target.";

    if (id.endsWith ("_uni_mode"))
        return "How unison voices are spread: Classic evenly, Hypersaw bunched around the centre like a supersaw, "
               "Octaves and Fifths also stack intervals for huge chords.";

    if (id.endsWith ("_uni_blend"))
        return "Level of the detuned unison voices against the centre one. Lower keeps the pitch focused.";

    if (id.startsWith ("lfo") && id.endsWith ("_phase"))
        return "Where the LFO starts in its cycle when a note retriggers it.";

    if (id.startsWith ("lfo") && id.endsWith ("_retrig"))
        return "On: every note gets its own LFO, starting from the start phase (per-voice). "
               "Off: one free-running LFO shared by all notes.";

    if (id == "voice_mode")
        return "Poly plays chords. Mono is one voice that retriggers the envelopes on every note. "
               "Legato is one voice where overlapping notes only slide the pitch - classic for basses and leads.";

    if (id == "poly_voices")
        return "Maximum voices sounding at once in Poly mode. Fewer voices save CPU and make old notes "
               "give way sooner.";

    if (id == "glide_legato")
        return "Only glide when notes overlap (mono modes). Detached notes jump straight to pitch.";

    if (id == "bend_range")
        return "Pitch bend range in semitones.";

    // LFOs (patterned)
    // Every LFO (1-16), not only the first four.
    if (id.startsWith ("lfo") && id.length() > 4 && juce::CharacterFunctions::isDigit (id[3]) && id.contains ("_"))
    {
        if (id.endsWith ("_shape"))
            return "Waveform. Draw = design your own, Steps = 16-step sequencer. Random: S&H, Sine Random, Perlin and "
                   "Drunk Walk, with a seed and stereo. Chaos: Lorenz, Rossler, Duffing, the logistic and Henon maps and "
                   "a double pendulum, solved properly. Physics: Bounce, Pendulum, Spring and Friction with real "
                   "parameters. Each has two outputs (LFO n and LFO n B). The M2 versions stay as \"classic\".";

        if (id.endsWith ("_phys_a"))
            return "Physics shape: Bounce height, Pendulum swing, Spring stiffness, or Friction drive.";

        if (id.endsWith ("_phys_b"))
            return "Physics shape: Bounce bounciness, Pendulum or Spring damping, or Friction stick time.";

        if (id.endsWith ("_kick"))
            return "Adds a velocity-scaled kick to a Pendulum on each note.";

        if (id.endsWith ("_key"))
            return "Key tracks the rate: the LFO runs per voice at the note's pitch times RATE / 4 Hz, so 4 Hz "
                   "follows the note exactly. Route it to an oscillator level, an FM route or ring mod for audio-rate growl.";

        if (id.endsWith ("_rate"))
            return "Speed when Sync is off.";

        if (id.endsWith ("_sync"))
            return "Locks the LFO to host tempo.";

        if (id.endsWith ("_div"))
            return "Tempo-synced rate.";

        if (id.endsWith ("_retrig"))
            return "Restarts the LFO phase on every note.";

        if (id.contains ("_step"))
            return "Step value when the shape is Steps.";

        return "Low frequency modulator - assign it in MOD > MATRIX.";
    }

    // Mod matrix (patterned)
    if (id.startsWith ("mod") && id.endsWith ("_pol"))
        return "Polarity: Auto keeps the source's own range (LFOs, key, random and MSEG swing both ways; envelopes, velocity, macros and the wheel push one way); Unipolar moves 0 to +depth; Bipolar moves either side of the knob.";
    if (id.startsWith ("mod") && id.endsWith ("_aux"))
        return "Via: a second source that scales this route (e.g. the mod wheel fading in an LFO). None = always full.";
    if (id.startsWith ("mod") && id.endsWith ("_byp"))
        return "Bypass: switches this route off without losing its settings.";

    // Singles that had no help.
    if (id.endsWith ("_sample_factory"))
        return "Factory sample this oscillator plays in Sample or Granular mode (a loaded or dropped wav replaces it).";
    if (id == "gen_root")
        return "Root note of the GENERATE scale (scale snap, note spray, Euclid and the probability sequencer).";
    if (id == "spray_direction")
        return "Which way sprayed notes go from the one played: up, down or both.";
    if (id == "os_factor")
        return "How much OVERSAMPLE raises the voice rate: 2x, or 4x for the cleanest highs at more CPU.";
    if (id.startsWith ("me_") && id.endsWith ("_velocity"))
        return "How much note velocity scales the MOD envelope.";
    if (id.endsWith ("_fb_gain"))
        return "Feedback guitar: how loud the amp pushes sound back into the string (more sustains and blooms into feedback).";
    if (id.endsWith ("_fb_distance"))
        return "Feedback guitar: how far the string is from the amp; changes which harmonic the feedback locks onto.";
    if (id.startsWith ("fx_taps_step"))
        return "Level of this step in the Taps delay's custom pattern.";
    if (id == "vec_on")
        return "Vector pad on: the four corner oscillators crossfade by where the dot is (X / Y are also mod sources).";
    if (id == "vec_x" || id == "vec_y")
        return id == "vec_x" ? "Vector position left to right." : "Vector position bottom to top.";
    if (id == "vec_a" || id == "vec_b" || id == "vec_c" || id == "vec_d")
        return "The oscillator at this corner of the vector pad.";
    if (id == "vec_path")
        return "Moves the dot round the drawn path at PATH RATE (X / Y then offset the whole path).";
    if (id == "vec_rate")
        return "How fast the dot goes round the path.";
    if (id.startsWith ("vec_px") || id.startsWith ("vec_py"))
        return "A point of the vector path; drag the points on the pad.";
    if (id == "vec_drift")
        return "Lets the dot wander on its own around where it is.";
    if (id == "vec_drift_rate")
        return "How fast the drift wanders.";

    if (id.startsWith ("mod") && (id.endsWith ("_src") || id.endsWith ("_dst") || id.endsWith ("_amt")))
    {
        if (id.endsWith ("_src"))
            return "Modulation source.";

        if (id.endsWith ("_dst"))
            return "Target of the modulation.";

        return "Depth. Negative values invert the modulation.";
    }

    // MSEG
    if (id.startsWith ("mseg_"))
    {
        if (id.endsWith ("_level1") || id.endsWith ("_level2") || id.endsWith ("_level3") || id.endsWith ("_level4"))
            return "MSEG point level. Drag the editor in MOD > STEPS & MSEG.";

        if (id.endsWith ("_time1") || id.endsWith ("_time2") || id.endsWith ("_time3") || id.endsWith ("_time4"))
            return "MSEG segment length (relative).";

        if (id == "mseg_rate")
            return "Speed of the multi-segment envelope.";

        if (id == "mseg_loop")
            return "Loop the shape, or hold the last point.";

        return "Multi-segment envelope mod source.";
    }

    if (id == "clock_div")
        return "Tempo-synced rate of the Clocked S&H random source.";

    // Macros
    if (id.startsWith ("macro"))
        return "Assign in MOD > MATRIX, or drive it with MIDI CC 20-23.";

    // Arp
    if (id == "arp_on")
        return "Plays held notes as a pattern. Off = normal playing.";

    if (id == "arp_mode")
        return "Up, Down, UpDown or Random.";

    if (id == "arp_div")
        return "Step size, synced to host tempo.";

    if (id == "arp_octaves")
        return "How many octaves the pattern climbs.";

    if (id == "arp_gate")
        return "Note length within each step.";

    // BODY
    if (id == "res_on")
        return "Turns the BODY section on. Classic keeps the original three-comb resonator.";

    if (id == "body_type")
        return "Classic, bar, plate, bell or shell. Material bodies use the oscillator mix as their exciter.";

    if (id == "body_material")
        return "Morph from wood through metal to glass: brightness, modal tuning and ring time.";

    if (id == "body_size")
        return "Larger bodies ring at lower modal frequencies.";

    if (id == "body_coupling_mode")
        return "Transfer energy from a struck string to the body, from the body back to a string, or between strings.";

    if (id == "body_coupling")
        return "Strength of the selected coupling path.";

    if (id == "res_amount")
        return "Wet amount of the selected body.";

    if (id == "res_decay")
        return "How long the resonances ring.";

    if (id == "res_offset")
        return "Resonator tuning in semitones. Inharmonic = metallic.";

    if (id == "res_keytrack")
        return "1.0 = follows the note, 0 = fixed pitch.";

    // Effects
    if (id == "fx_drive_on" || id == "fx_drive_amount" || id == "fx_drive_mix")
        return "Saturation stage - warm to destroyed.";

    if (id == "fx_fold")
        return "Wavefolder: folds peaks back for harsh, buzzy harmonics.";

    if (id == "fx_crush_on" || id == "fx_crush_bits" || id == "fx_crush_down" || id == "fx_crush_mix")
        return "Bit crusher: fewer bits and downsample for lo-fi digital filth.";

    if (id == "fx_comb_on" || id == "fx_comb_freq" || id == "fx_comb_feedback" || id == "fx_comb_mix")
        return "Tuned comb resonator. Try harmonic frequencies with Physical mode.";

    if (id == "fx_phaser_on" || id == "fx_phaser_rate" || id == "fx_phaser_depth"
        || id == "fx_phaser_feedback" || id == "fx_phaser_mix")
        return "Classic sweeping phaser - the Virus move.";

    if (id == "fx_chorus_on" || id == "fx_chorus_rate" || id == "fx_chorus_depth" || id == "fx_chorus_mix")
        return "Thickening chorus for width.";

    if (id == "fx_delay_on" || id == "fx_delay_time" || id == "fx_delay_sync" || id == "fx_delay_div"
        || id == "fx_delay_feedback" || id == "fx_delay_damping" || id == "fx_delay_pingpong" || id == "fx_delay_mix")
        return "Main delay. Damping darkens repeats.";

    if (id == "fx_delay_pitch")
        return "Pitch-shifts the delay feedback - the Sophie trick.";

    if (id == "fx_delay_wow")
        return "Tape-style pitch drift of the delay time.";

    if (id == "fx_taps_on" || id == "fx_taps_pattern" || id == "fx_taps_mix")
        return "Multi-tap rhythmic echoes riding on the delay time.";

    if (id == "fx_stutter_on" || id == "fx_stutter_div" || id == "fx_stutter_mix")
        return "Loops the current beat-slice. Flip on/off in time for glitches.";

    if (id == "fx_smear_on" || id == "fx_smear_size" || id == "fx_smear_density" || id == "fx_smear_mix")
        return "Grain cloud: smears the input into a hazy texture.";

    if (id == "fx_freeze_on" || id == "fx_freeze_mix")
        return "Spectral freeze: holds the current spectrum into a drone.";

    if (id == "fx_reverb_on" || id == "fx_reverb_size" || id == "fx_reverb_damping"
        || id == "fx_reverb_width" || id == "fx_reverb_mix")
        return "Space and depth.";

    if (id.endsWith ("_bypass"))
        return "Bypasses this rack slot without losing its settings.";

    if (id.endsWith ("_solo"))
        return "Solo (wet only): hear just this slot's output.";

    if (id.startsWith ("fx_slot") && id.endsWith ("_mix"))
        return "Parallel blend for this slot: 1 = fully through the effect, 0 = dry.";

    if (id.startsWith ("fx_slot") && id.endsWith ("_band"))
        return "Splitter: the part of the signal this slot works on (Full, a Low, Mid or High band, or the Mid or Side). "
               "The rest passes around it. Slots on the same band in a row are that band's chain.";

    if (id == "fx_split_low" || id == "fx_split_high")
        return juce::String ("Splitter crossover between the ") + (id == "fx_split_low" ? "Low and Mid" : "Mid and High")
               + " bands (24 dB/oct).";

    if (id.startsWith ("fx_slot"))
        return "Rack slot: choose which effect lives at this position.";

    if (id.startsWith ("fx_amp_"))
    {
        if (id == "fx_amp_mode")
            return "Tube is warm and asymmetric, Fuzz is hard, Clean is soft.";

        if (id == "fx_amp_drive")
            return "Input gain into the amp - the main dirt control.";

        if (id == "fx_amp_bass" || id == "fx_amp_mid" || id == "fx_amp_treble")
            return "Tone stack band gain.";

        return "Amp output level.";
    }

    if (id.startsWith ("fx_comp_"))
    {
        if (id == "fx_comp_threshold")
            return "Level where compression starts.";

        if (id == "fx_comp_ratio")
            return "How hard peaks are squashed.";

        if (id == "fx_comp_attack")
            return "How fast the compressor clamps down.";

        if (id == "fx_comp_release")
            return "How fast it lets go.";

        if (id == "fx_comp_makeup")
            return "Output gain to compensate for squashing.";

        return "Dry/wet blend.";
    }

    if (id == "fx_haas_delay")
        return "Delays the right channel - width without chorus.";

    if (id == "fx_haas_mix")
        return "How much of the delayed side you hear.";

    if (id == "fx_reverb_type")
        return "Room, Hall, Plate, Shimmer, Spring, Gated, or IR (load your own impulse).";

    if (id.startsWith ("fx_stutter_"))
    {
        if (id == "fx_stutter_reverse")
            return "Plays the captured loop backwards.";

        if (id == "fx_stutter_pitch")
            return "Pitches the captured loop up or down, tape style.";

        return "Stutter: captures and repeats a slice in time.";
    }

    if (id.startsWith ("fx_feedback_"))
    {
        if (id == "fx_feedback_amount")
            return "How much of the loop feeds back into itself.";

        if (id == "fx_feedback_delay")
            return "Length of the feedback loop in milliseconds.";

        if (id == "fx_feedback_tone")
            return "Damps the feedback loop - lower is darker.";

        return "Mix of the recirculating feedback loop.";
    }

    if (id.startsWith ("fx_flanger_"))
        return "Flanger: whooshing comb sweep from a very short modulated delay.";

    if (id.startsWith ("fx_dim_"))
        return "Dimension: lush slow modulated delays - width without wobble.";

    if (id.startsWith ("fx_gate_"))
        return "Tempo-synced trance gate with shaped patterns and smoothing.";

    if (id == "fx_tape_stop_trigger")
        return "Triggers the tape stop: the signal pitches down and halts.";

    if (id == "fx_tape_stop_time" || id == "fx_tape_stop_mix")
        return "Tape stop timing and blend.";

    if (id.startsWith ("fx_aw") && id.endsWith ("_algo") && id != "fx_aw_algo")
        return "An Airwindows category module: pick one of its effects (Chris Johnson's, MIT). Changing it sets the knobs to that effect's own defaults.";

    if (isAirwindowsKnob (id) && ! id.startsWith ("fx_aw_"))
        return "An Airwindows knob: what it does follows the chosen effect (its name is the knob's label).";

    if (id.startsWith ("fx_aw_"))
    {
        if (id == "fx_aw_algo")
            return "Airwindows: Chris Johnson's algorithms (MIT) - tape, consoles, EQ, dynamics, reverbs, stereo, lo-fi. Changing it starts the new one fresh.";

        if (id == "fx_aw_mix")
            return "Blend of the Airwindows algorithm with the dry signal.";

        return "An Airwindows knob: what it does follows the chosen algorithm (its name is the knob's label), 0 to 1 as in the plugin.";
    }

    if (id == "fx_tilt" || id == "fx_tilt_level")
        return "Single-knob tone tilt: darker on one side, brighter on the other.";

    if (id.startsWith ("fx_util_"))
        return "Utility: gain trim, mono maker and polarity invert.";

    if (id.startsWith ("fx_ott_"))
        return "OTT-style three-band up/down compression. Loud, bright, modern.";

    if (id.startsWith ("fx_limit_"))
        return "Brickwall-style limiter ceiling and release.";

    if (id == "fx_width" || id == "fx_width_mix")
        return "Stereo width via mid/side. 0 = mono, 2 = extra wide.";

    if (id.startsWith ("fx_trem_"))
        return "Tempo-free tremolo with selectable LFO shapes.";

    if (id == "fx_shifter_shift" || id == "fx_shifter_mix")
        return "Frequency shifter (Hz, not semitones) - inharmonic and metallic.";

    if (id == "fx_ring_freq" || id == "fx_ring_mix")
        return "Ring modulation with an internal carrier oscillator.";

    if (id == "fx_octaver_mix")
        return "Octaver: blends in an octave-down granulated layer.";

    if (id == "fx_vowel_morph" || id == "fx_vowel_mix")
        return "Vowel filter: morphs A-E-I-O-U formant shapes.";

    if (id.startsWith ("fx_voc_"))
    {
        if (id == "fx_voc_source")
            return "Where the voice comes from. Input: the audio input (ilanaSynth FX). Talk: a built-in vowel sweep of the "
                   "synth's own sound, for the instrument. Auto: Input when the host gives one, else Talk.";
        if (id == "fx_voc_bands")
            return "Number of vocoder bands, log-spaced 100 Hz to 8 kHz. More is clearer speech, fewer is rougher.";
        if (id == "fx_voc_width")
            return "Band sharpness. Low: smooth and blurred, high: narrow, ringing and more robotic.";
        if (id == "fx_voc_attack")
            return "How fast each band follows a rise in the voice. Short keeps consonants clear.";
        if (id == "fx_voc_release")
            return "How fast each band lets go. Long smears the words, short is choppy.";
        if (id == "fx_voc_formant")
            return "Shifts the synth's bands against the voice's, in semitones: the vowel character moves up or down "
                   "(chipmunk to giant) without changing the notes.";
        if (id == "fx_voc_unvoiced")
            return "Noise fed into the bands above 3 kHz, so S, T and F still come through when the synth has little up there.";
        if (id == "fx_voc_rate")
            return "Speed of the built-in vowel sweep (Talk).";
        if (id == "fx_voc_level")
            return "Output level of the vocoded signal.";
        return "Blend of the vocoded signal with the dry synth.";
    }

    if (id == "fx_delay_time_r")
        return "Independent right-channel delay time.";

    if (id == "fx_delay_duck")
        return "Ducks the delay repeats whenever the dry signal plays.";

    if (id.startsWith ("sub_"))
    {
        if (id == "sub_level") return "Level of OSC 3.";
        if (id == "sub_shape") return "Sub oscillator shape: sine, square or saw.";
        if (id == "sub_octave") return "Sub oscillator octave: one or two octaves down.";
        if (id == "sub_mode")
            return "Wavetable, Physical string, a sample you drag onto the card, or Granular: a cloud of tiny "
               "grains read from that sample (a vocal until you load your own).";
        if (id == "sub_table") return "Wavetable for OSC 3.";
        if (id == "sub_frame") return "Morphs through the table's frames.";
        if (id == "sub_pan") return "Stereo position of OSC 3.";
        if (id == "sub_semi") return "OSC 3 pitch offset in semitones.";
        if (id == "sub_fine") return "Fine pitch offset in cents.";
        if (id == "sub_unison") return "Stacked detuned copies of OSC 3.";
        if (id == "sub_detune") return "Unison spread in cents.";
        if (id == "sub_spread") return "Stereo spread of the unison stack.";
        if (id == "sub_excite") return "String excitation.";
        if (id.startsWith ("sub_string_")) return "Physical string parameter for OSC 3.";
        if (id == "sub_chord") return "Spreads a chord across the unison voices.";

        return "OSC 3 / sub oscillator.";
    }

    if (id.startsWith ("f2e_"))
        return "Filter 2 has its own envelope.";

    if (id.startsWith ("e4_"))
        return "Envelope 4 - a free mod source for the matrix.";

    if (id.endsWith ("_curve"))
        return "Envelope tension: positive is snappy, negative is slow then fast.";

    if (id == "master")
        return "Final output level.";

    if (id == "master_clip")
        return "Soft-clips the output so extreme patches can't hard-clip.";

    if (id == "master_clip_gain")
        return "Input gain into the soft clipper. Drive it to squash peaks harder.";

    if (id == "oversampling")
        return "Renders the oscillators, filters and envelopes at 2x and downsamples, cutting aliasing from FM, sync and wavefolding. Uses more CPU.";

    if (id.endsWith ("_sample_tuned"))
        return "Tuned follows the played note; untuned plays the file at its original pitch.";

    if (id.endsWith ("_sample_loop"))
        return "Loops the start/end region instead of playing once.";

    if (id.endsWith ("_sample_reverse"))
        return "Plays the region backwards. With loop on, it keeps looping backwards.";

    if (id.endsWith ("_sample_start"))
        return "Start of the playback region, as a fraction of the sample.";

    if (id.endsWith ("_sample_end"))
        return "End of the playback region, as a fraction of the sample.";

    if (id.endsWith ("_sample_fade_in"))
        return "Fades in over this fraction of the region, so loops and one-shots never click.";

    if (id.endsWith ("_sample_fade_out"))
        return "Fades out over this fraction of the region.";

    return {};
}
