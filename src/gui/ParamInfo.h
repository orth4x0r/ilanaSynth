#pragma once

#include <juce_core/juce_core.h>
#include "../dsp/OscillatorIds.h"

inline bool isOscParameter (const juce::String& id, const char* suffix, bool includeLegacyThird = true)
{
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if ((includeLegacyThird || osc != 2)
            && id == juce::String (OscillatorIds::prefixes[(size_t) osc]) + suffix)
            return true;

    return false;
}

inline juce::String describeValue (const juce::String& id, float value)
{
    const auto asPercent = [value] { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; };
    const auto asMilliseconds = [value] { return juce::String (juce::roundToInt (value)) + " ms"; };
    const auto asSeconds = [value]
    {
        return value < 1.0f ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms"
                            : juce::String (value, 2) + " s";
    };
    const auto asHertz = [value]
    {
        const auto magnitude = std::abs (value);

        // Note juce::String (value, 0) means "default precision", not "no
        // decimals", so whole numbers go through roundToInt.
        return magnitude >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                    : (magnitude < 10.0f ? juce::String (value, magnitude < 1.0f ? 2 : 1)
                                                         : juce::String (juce::roundToInt (value))) + " Hz";
    };

    if (id == "fx_gate_swing" || (id.startsWith ("fx_gate_step") && id != "fx_gate_steps"))
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";

    if (id == "fx_gate_steps")
        return juce::String (juce::roundToInt (value)) + " steps";

    if (id == "spray_count")
        return juce::String (juce::roundToInt (value)) + " notes";
    if (id == "sym_count")
        return juce::String (juce::roundToInt (value)) + " strings";
    if (id == "sym_decay")
        return asPercent();
    if (id.startsWith ("sym_note"))
    {
        // Note name with C3 = MIDI 60, as on the keyboard strip.
        static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto note = juce::roundToInt (value);
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 2);
    }

    if (id == "spray_range")
        return juce::String (juce::roundToInt (value)) + " st";

    if (id == "spray_spread")
        return juce::String (juce::roundToInt (value)) + " ms";

    if (id.startsWith ("fm_") && id != "fm_mode")
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";

    if (id == "spray_chance" || id == "spray_velocity" || id == "arp_chance")
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";

    if (id == "filter_balance")
    {
        if (std::abs (value) < 0.01f)
            return "F1 = F2";

        return value < 0.0f ? "F1 +" + juce::String (juce::roundToInt (-value * 100.0f)) + " %"
                            : "F2 +" + juce::String (juce::roundToInt (value * 100.0f)) + " %";
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
        return juce::String (value, value == std::floor (value) ? 0 : 1)
               + (id.endsWith ("_fine") || id.endsWith ("_detune") ? " ct" : " st");

    if (id == "master" || id == "master_clip_gain" || id == "fx_limit_ceiling" || id == "fx_tilt_level"
        || id == "fx_comp_makeup" || id == "fx_comp_threshold" || id == "fx_util_gain"
        || (id.startsWith ("fx_eq_") && id.endsWith ("_gain")))
        return juce::String (value, 1) + " dB";

    if (id == "fx_drive_amount" || id == "fx_amp_drive")
        return juce::String (value, value < 10.0f ? 1 : 0) + "x";

    if (id.endsWith ("_mix") || id.endsWith ("_amount") || id.endsWith ("_level") || id.endsWith ("_width")
        || id.endsWith ("_spread") || id.endsWith ("_sustain") || id.endsWith ("_velocity")
        || id.endsWith ("_damping") || id.endsWith ("_feedback") || id.endsWith ("_curve")
        || id.endsWith ("_frame") || id.endsWith ("_start") || id.endsWith ("_end")
        || id.endsWith ("_fade_in") || id.endsWith ("_fade_out") || id == "drift" || id == "ring_mod"
        || id == "fm_amount" || id == "fm_feedback" || id == "fx_fold" || id == "res_amount"
        || id == "res_keytrack" || id == "fx_tilt" || id == "fx_shifter_mix"
        || id == "noise_level" || id == "unison_random" || id == "voice_spread"
        || id.startsWith ("macro") || id.startsWith ("mseg_level")
        || id == "res_decay" || id.endsWith ("_string_decay") || id.endsWith ("_string_damp")
        || id.endsWith ("_string_stiffness") || id.endsWith ("_string_pickup") || id.endsWith ("_string_excite_pos")
        || id.endsWith ("_string_pick_hardness") || id.endsWith ("_string_pick_pos")
        || id.endsWith ("_phys_a") || id.endsWith ("_phys_b")
        || id.endsWith ("_bow_pressure") || id.endsWith ("_bow_speed")
        || id.endsWith ("_bridge_buzz") || id.endsWith ("_fret_rattle")
        || id == "sym_amount" || id == "sym_decay"
        || id.endsWith ("_warp_amt") || id.endsWith ("_spectral_amt") || id.endsWith ("_grain_density") || id.endsWith ("_grain_spray")
        || id.endsWith ("_grain_pitch") || id.endsWith ("_grain_spread") || id.endsWith ("_uni_blend") || id.endsWith ("_phase") || id.endsWith ("_morph"))
        return asPercent();

    return juce::String (value, value == std::floor (value) ? 0 : 2);
}

inline juce::String describeParameter (const juce::String& id)
{
    if (id == "quality")
        return "Eco caps each oscillator at four unison voices; Normal preserves the original engine; "
               "High averages two wavetable reads per sample for smoother highs.";
    if (isOscParameter (id, "_amp_env"))
        return "Envelope controlling this oscillator's level. ENV 1 is the original AMP envelope.";
    if (id.startsWith ("env") && juce::isPositiveAndBelow (id.substring (3).getIntValue() - 6, 11))
    {
        if (id.endsWith ("_velocity")) return "How strongly note velocity scales this envelope when used as a source.";
        if (id.endsWith ("_curve")) return "Envelope tension: positive reaches the target early, negative reaches it late.";
        return "ADSR stage for this per-voice envelope. Assign it in the matrix or as an oscillator amp envelope.";
    }
    if (id == "sym_on") return "Enable the shared drone strings after the voices and before effects.";
    if (id == "sym_amount") return "How much the shared strings ring in the mix.";
    if (id == "sym_decay") return "How long the sympathetic strings ring after the excitation stops.";
    if (id == "sym_count") return "Number of shared drone strings, from one to six.";
    if (id == "sym_manual") return "Tune the strings note by note instead of from the GENERATE scale and root. "
                                   "With no scale set they use an open tuning on the root (root, fifth, octave, third).";
    if (id.startsWith ("sym_note")) return "Manual tuning for this drone string, used when MANUAL is on.";
    if (id.endsWith ("_bow_pressure")) return "Bow grip on the string. MPE pressure and channel aftertouch add to it.";
    if (id.endsWith ("_bow_speed")) return "Bow travel speed; changes the sustained tone and scrape.";
    if (id.endsWith ("_bridge_buzz")) return "Nonlinear bridge contact, from clean to sitar-like buzz.";
    if (id.endsWith ("_fret_rattle")) return "Velocity-scaled fret contact noise. Zero is clean.";
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
        return "Wavetable, Physical string, a sample you drag onto the card, or Granular: a cloud of tiny "
               "grains read from that sample (a vocal until you load your own).";

    if (isOscParameter (id, "_on"))
        return "Turns this oscillator on and off. Switching fades in/out so it stays click-free.";

    if (isOscParameter (id, "_excite", false))
        return "String excitation: Burst plucks, Noise/Saw/Pulse sustain the string.";

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
        return "Excitation position on the string. Changes which harmonics ring.";

    if (id.endsWith ("_string_pick_hardness"))
        return "Pick hardness. Harder picks give a brighter attack.";

    if (id.endsWith ("_string_pick_pos"))
        return "Where the pick strikes the string. Changes the attack spectrum.";

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
        return "Randomizes unison start phases for a softer, phasey attack.";

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
        return "How much the Filter Envelope moves the cutoff.";

    if (id == "f1_keytrack" || id == "f2_keytrack")
        return "Cutoff follows the played note (1.0 = full tracking).";

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
               "skews it, Quantize steps it, FM and Ring use another oscillator (OSC 2 for OSC 1, OSC 1 for the others).";

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
    if (id.startsWith ("lfo1_") || id.startsWith ("lfo2_") || id.startsWith ("lfo3_") || id.startsWith ("lfo4_"))
    {
        if (id.endsWith ("_shape"))
            return "Waveform. Draw = design your own, Steps = 16-step sequencer. Smooth Random glides to a new "
                   "random value each cycle, Drunk wanders a little from where it was, Chaos follows a Lorenz "
                   "attractor that never repeats. Bounce, Pendulum, Spring and Friction simulate motion.";

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

        return "Low frequency modulator - assign it in the MATRIX tab.";
    }

    // Mod matrix (patterned)
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
            return "MSEG point level. Drag the editor on the ARP/SEQ tab.";

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
        return "Assign in the MATRIX tab, or drive it with MIDI CC 20-23.";

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

    // Resonator
    if (id == "res_on")
        return "Three tuned combs per voice: body or metal.";

    if (id == "res_amount")
        return "Wet amount of the resonator bank.";

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
        return "Trigger the tape-stop: the signal pitches down and halts.";

    if (id == "fx_tape_stop_time" || id == "fx_tape_stop_mix")
        return "Tape stop timing and blend.";

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
