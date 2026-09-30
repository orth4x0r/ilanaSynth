#pragma once

// Per-preset voicing: parameter changes and macro rewiring laid over a
// factory preset's recipe when it loads, before the level trims
// (src/PresetTrims.h). The diversity pass (docs/REVIEW-PLAN.md, step 3)
// writes its redesigns here so the recipes in PresetLibrary.h and friends
// stay readable as they were first written.
//
// One preset per line:
//
//   Name | id=value id=value ... | mN=NAME: target amount, target amount; mN=...
//
// - id=value sets a parameter (its real value, as the Builder writes it).
//   id+=value adds to the recipe's value (for ids whose default is 0, such
//   as osc1_semi: a recipe that leaves one at its default gets the value).
//   fx=a,b,c sets fx_slot1..10 to those FX types (the rest to none).
// - mN (1-4) rewires macro N: its old routings are removed, it takes the new
//   name, and each target (a parameter id, or a destination's display name
//   from Mod::getDestinationNames) gets a routing with that amount.
// - Blank lines and lines starting with # are skipped.
//
// ILANA_PRESET_VOICING=<file> replaces this table with the file's lines (for
// iterating without a rebuild; bake the result back in here).

namespace Presets
{
inline const char* getVoicingText()
{
    return R"VOICING(
# ---- Pads: archetypes (diversity pass 1) ----
# Dry and close: no reverb, short tails, a console or tape colour.
Warm Analog Pad | fx=7,30 fx_reverb_on=0 fx_aw_algo=0 fx_aw_p1=0.62 fx_aw_p2=0.5 fx_aw_p3=0.5 fx_aw_p4=0.35 fx_aw_p5=0.5 amp_release=0.9 fx_slot2_mix=0.1 | m4=TAPE: fx_slot2_mix 0.9
Hypersaw Pad | fx=20,22 fx_reverb_on=0 fx_ott_amount=0.45 fx_ott_mix=0.6 fx_width=1.8 f1_cutoff=12000 amp_attack=0.02 amp_release=0.6 amp_velocity=0 | m1=CUTOFF: Filter1 Cutoff -0.6; m4=FOCUS: Width -1.5
Pulse Width Pad | fx=7 fx_reverb_on=0 f1_cutoff=700 amp_release=0.7 amp_velocity=0 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 osc4_semi+=-12 osc5_semi+=-12 osc6_semi+=-12 | m4=CHORUS: fx_chorus_mix 0.5
Octave Stack Pad | fx=30,13 fx_reverb_type=2 fx_reverb_mix=0.2 fx_aw_algo=14 fx_aw_p1=0.7 fx_aw_p2=0.5 amp_release=1.0 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 osc4_semi+=12 osc5_semi+=12 osc6_semi+=12 osc1_table=8 osc2_table=8 f1_cutoff=20000 | m2=TONE: Filter1 Cutoff -1; m4=AIR: Reverb Mix 0.4
Six-Voice Drift | fx=7,2 fx_reverb_on=0 fx_drive_on=1 fx_drive_amount=0.25 fx_drive_mix=0.5 amp_release=1.2 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 osc4_semi+=-12 osc5_semi+=-12 osc6_semi+=-12 ring_mod=0.45 | m4=DRIVE: Drive Amount 0.5
Per-Voice Drift Pad | fx=30 fx_reverb_on=0 fx_aw_algo=38 fx_aw_p1=0.35 fx_aw_p2=0.6 fx_aw_p3=0.8 amp_release=1.0 fx_slot1_mix=0.45 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 osc4_semi+=12 osc5_semi+=12 osc6_semi+=12 osc1_table=22 osc2_table=22 fx=3,30 fx_crush_on=1 fx_crush_bits=10 fx_crush_down=2 fx_crush_mix=0.5 fx_slot2_mix=0.1 | m4=ENSEMBLE: fx_slot2_mix 0.9
# Airwindows spaces instead of the Hall.
Vowel Pad | fx=7,30 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.6 fx_aw_p2=0.6 fx_aw_p3=0.6 fx_aw_p4=1 fx_aw_p5=0.8 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 osc4_semi+=-12 osc5_semi+=-12 osc6_semi+=-12 fx_slot2_mix=0.1 | m4=GALAXY: fx_slot2_mix 0.9
Shimmer Glass Pad | fx=30 fx_reverb_on=0 fx_aw_algo=25 fx_aw_p1=0.8 amp_decay=3 amp_sustain=0.1 osc1_spectral=1 osc1_spectral_amt=0.5 fx_slot1_mix=0.1 | m4=CATHEDRAL: fx_slot1_mix 0.9
Dark Matter | fx=30,13 fx_reverb_on=1 fx_reverb_type=1 fx_reverb_mix=0.3 fx_aw_algo=29 fx_aw_p1=1 fx_aw_p2=0.3 fx_aw_p3=1 amp_attack=2.4 amp_decay=4 amp_sustain=0.07 | m4=SPACE: Reverb Mix 0.4
Lorenz Wash | fx=30 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.9 fx_aw_p2=0.3 fx_aw_p3=0.8 fx_aw_p4=1 fx_aw_p5=0.8 amp_attack=2.5 fx_slot1_mix=0.1 | m4=SPACE: fx_slot1_mix 0.9
Twin Peak Pad | fx=30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.2 fx_aw_p2=0.2 fx_aw_p3=0.5 fx_aw_p4=0.2 fx_aw_p5=0.8 amp_release=1.5 fx_slot1_mix=0.1 | m4=ROOM: fx_slot1_mix 0.9
Tabled Cello | fx=29,30 fx_reverb_on=0 fx_aw_algo=26 fx_aw_p1=0.3 fx_aw_p2=0.3 fx_aw_p3=0.6 fx_aw_p4=0.8 amp_attack=0.15 fx_slot2_mix=0.1 | m4=ROOM: fx_slot2_mix 0.9
Evolve Macro Pad | fx=22 fx_reverb_on=0 fx_width=0.3 fx_width_mix=1 amp_release=0.9 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 osc4_semi+=12 osc5_semi+=12 osc6_semi+=12 | m4=WIDTH: Osc1 Pan -0.8, Osc2 Pan 0.8, Osc3 Pan -0.5, Width 0.5
# Lo-fi, tape, vinyl.
Tape Strings | fx=30,22 fx_reverb_on=0 fx_aw_algo=37 fx_aw_p1=0.9 fx_width=0.35 fx_width_mix=1 amp_release=1.0 osc1_table=36 osc2_table=36 f1_type=1 f1_cutoff=1200 f1_reso=0.35 fx_slot1_mix=0.1 | m2=WOBBLE: Drift 0.6, Osc1 Detune 0.5; m4=WOW: fx_slot1_mix 0.9
Tape Choir | fx=7,30 fx_reverb_on=0 fx_delay_on=0 fx_aw_algo=1 fx_aw_p1=0.8 fx_aw_p2=0.3 fx_aw_p3=0.7 fx_aw_p4=0.8 fx_aw_p5=0.7 amp_release=1.2 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 osc4_semi+=-12 osc5_semi+=-12 osc6_semi+=-12 osc1_table=24 osc2_table=24 f1_type=10 | m4=CHORUS: fx_chorus_mix 0.6
Vector Path Drift | fx=30,9 fx_reverb_on=0 fx_aw_algo=31 fx_aw_p1=0.5 fx_aw_p2=0.6 fx_aw_p3=0.2 fx_aw_p4=0.5 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=9 fx_delay_feedback=0.35 fx_delay_mix=0.25 fx_delay_damping=0.6 | m4=ECHO: Delay Mix 0.4
Resampled FM Texture | fx=30 fx_reverb_on=0 fx_aw_algo=32 fx_aw_p1=0.35 fx_aw_p2=0.5 fx_aw_p3=0.6 fx_aw_p4=0.7 amp_release=1.0 fx_slot1_mix=0.1 | m4=DEREZ: fx_slot1_mix 0.9
Curve Motion Pad | fx=3,9 fx_reverb_on=0 fx_crush_on=1 fx_crush_bits=8 fx_crush_down=4 fx_crush_mix=0.4 amp_release=1.0 | m4=CRUSH: Crush Mix 0.5
Grain Choir | fx=30,13 fx_aw_algo=33 fx_aw_p1=0.35 fx_reverb_type=2 fx_reverb_mix=0.25 fx_slot1_mix=0.1 | m4=WRECK: fx_slot1_mix 0.9
# Driven, saturated.
Virus Pad | fx=30,6 fx_reverb_on=0 fx_aw_algo=36 fx_aw_p1=0.6 fx_aw_p2=0.6 fx_aw_p3=0.8 amp_attack=0.03 amp_release=0.8 osc1_table=1 osc2_table=1 osc1_warp=8 osc1_warp_amt=0.4 fx_slot1_mix=0.1 | m4=HEAT: fx_slot1_mix 0.9
Morph Pad | fx=2,6 fx_reverb_on=0 fx_drive_on=1 fx_drive_amount=0.45 fx_drive_mix=0.6 amp_release=1.2 osc1_table=30 osc2_table=30 f1_type=2 f1_cutoff=700 f1_reso=0.3 osc1_spectral=3 osc1_spectral_amt=0.6 | m4=DRIVE: Drive Amount 0.45
Chord Pad | fx=30,7 fx_reverb_on=0 fx_aw_algo=7 fx_aw_p1=0.7 fx_aw_p2=0.6 amp_attack=0.005 amp_decay=0.8 amp_sustain=0.3 amp_release=0.5 amp_velocity=0.7 fx_slot1_mix=0.1 | m2=MORPH: Osc1 Frame 1, Filter1 Reso 0.6; m4=TUBE: fx_slot1_mix 0.9
Harmonic Cut Pad | fx=30,13 fx_aw_algo=3 fx_aw_p1=0.5 fx_aw_p2=0.1 fx_aw_p3=0.8 fx_aw_p4=0.8 fx_reverb_type=0 fx_reverb_mix=0.2 fx_slot1_mix=0.45 osc1_table=19 osc2_table=19 amp_attack=0.01 fx=7 fx_reverb_on=0 amp_release=0.4 amp_velocity=0 | m4=CHORUS: fx_chorus_mix 0.5
# Fast attacks: blooms and stabs.
DX Bell Pad | amp_attack=0.005 fx_reverb_type=2 fx_reverb_mix=0.2 | m3=SWELL: Amp Attack 1, Filter1 Cutoff -0.4; m4=SHIMMER: Osc1 Detune 0.5, Osc1 Spread 0.5, FM Amount 0.4
Deep FM Bell Body | amp_attack=0.004 fx=30 fx_reverb_on=0 fx_aw_algo=26 fx_aw_p1=0.7 fx_aw_p2=0.7 fx_aw_p3=0.3 fx_aw_p4=0.8 fx_slot1_mix=0.1 | m4=SPACE: fx_slot1_mix 0.9
Struck Board Pad | amp_attack=0.003 fx_reverb_type=0 fx_reverb_mix=0.15 amp_release=1.4 | m4=ROOM: Reverb Mix 0.4
MSEG Sweep | amp_attack=0.01 fx=9 fx_reverb_on=0 amp_release=0.8 | m4=ECHO: Delay Mix 0.4
Reversed Piano Wash | fx=17,9,13 fx_reverb_type=5 fx_reverb_mix=0.3 | m4=GATED: Reverb Mix 0.4
# Width: one mono, one very wide.
Routed Duo Pad | fx=22,7,2 fx_drive_on=1 fx_drive_amount=0.35 fx_drive_mix=0.6 amp_attack=0.04 fx_reverb_on=0 fx_width=0 fx_width_mix=1 amp_release=1.0 osc1_table=37 osc2_table=37 osc1_warp=5 osc1_warp_amt=0.6 | m4=WIDTH: fx_width 1.2
Wide Chord Pad | fx=30,15,13 fx_aw_algo=30 fx_aw_p1=0.3 fx_aw_p2=0.9 fx_aw_p3=0.8 fx_aw_p4=0.5 fx_aw_p5=1 fx_reverb_mix=0.25 fx_slot1_mix=0.1 f1_type=34 f1_slope=0 f1_cutoff=1260 f1_reso=0.9 f1_morph=0.9 | m4=SPACE: fx_slot1_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Panned Engine Sweep | fx=30 fx_reverb_on=0 fx_aw_algo=29 fx_aw_p1=1 fx_aw_p2=0.4 fx_aw_p3=1 amp_release=1.2 amp_attack=0.02 amp_velocity=0 fx_slot1_mix=0.1 | m4=WIDTH: fx_slot1_mix 0.9
# Rhythm and motion.
Swing Gate Pad | fx=16,7 fx_reverb_on=0 amp_release=0.4 f1_type=8 f1_cutoff=500 f1_reso=0.6 | m4=CHORUS: fx_chorus_mix 0.5
Breathing Pad | fx=23,13 fx_trem_rate=0.4 fx_trem_depth=0.6 fx_reverb_type=2 fx_reverb_mix=0.2 | m4=BREATH: fx_trem_depth 0.4
Pendulum Swing Pad | fx=23 fx_reverb_on=0 fx_trem_rate=5.5 fx_trem_depth=0.5 fx_trem_shape=1 amp_release=1.0 f1_type=3 f1_cutoff=1500 f1_reso=0.5 | m4=TREMOLO: fx_trem_depth 0.5, fx_trem_rate 0.3
Phaser Notch Sweep | fx=6 fx_reverb_on=0 amp_release=1.0 amp_velocity=0 osc1_table=15 osc2_table=15 | m4=FEEDBACK: Filter1 Reso 0.4, Phaser Feedback 0.7, Phaser Mix 0.6
Sweep Pad | fx=14,13 fx_flanger_rate=0.08 fx_flanger_depth=0.7 fx_flanger_feedback=0.6 fx_flanger_mix=0.5 fx_reverb_mix=0.2 | m4=JET: fx_flanger_feedback 0.35
# Echo instead of reverb.
Layered Sync Choir | fx=7,9,19 fx_reverb_on=0 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=10 fx_delay_feedback=0.45 fx_delay_mix=0.3 fx_delay_pingpong=1 amp_attack=0.02 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 osc4_semi+=12 osc5_semi+=12 osc6_semi+=12 fx_util_gain=14 | m4=ECHO: Delay Mix 0.4, Delay Feedback 0.3
Three Carrier Choir | fx=9 fx_reverb_on=0 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=3 fx_delay_feedback=0.3 fx_delay_mix=0.25 fx_delay_damping=0.7 | m4=ECHO: Delay Mix 0.4
# Softer, velocity-free, plate rather than hall.
Comb Choir | fx=22,2 fx_reverb_on=0 fx_width=0.1 fx_width_mix=1 fx_drive_on=1 fx_drive_amount=0.3 fx_drive_mix=0.5 amp_attack=0.025 amp_release=0.4 amp_velocity=0 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 osc4_semi+=-12 osc5_semi+=-12 osc6_semi+=-12 | m4=HONK: Drive Amount 0.5
Morphing Strings | fx=22,13,19 fx_width=0.4 fx_width_mix=1 fx_reverb_type=2 fx_reverb_mix=0.22 amp_attack=2.6 osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 f1_cutoff=900 osc1_spectral=5 osc1_spectral_amt=0.6 fx_util_gain=8 |
Cello Section | fx=29,22,13 fx_width=0.2 fx_width_mix=1 fx_reverb_type=0 amp_velocity=0.6 | m3=VIBRATO: LFO1 Rate 0.3, Osc1 Pitch 0.06
String Ensemble Wide | fx=15,13 fx_reverb_type=2 amp_attack=1.8 f1_type=37 f1_slope=0 f1_cutoff=1500 f1_reso=0.9 f1_morph=0.9 |
Warp Envelope Pad | fx=7,13 fx_reverb_type=0 fx_reverb_mix=0.18 f1_type=35 f1_slope=0 f1_cutoff=1500 f1_reso=0.7 f1_morph=0.8 |
Chaos Filter Pad | fx=6,30,19 fx_reverb_on=0 fx_aw_algo=28 fx_aw_p1=0.7 fx_aw_p2=0.3 fx_aw_p3=0.7 fx_aw_p4=0.7 fx_aw_p5=0.8 osc1_table=38 osc2_table=38 fx_slot2_mix=0.1 fx_util_gain=6 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=0 | m4=SPACE: fx_slot2_mix 0.9; m2=CUTOFF: Filter1 Cutoff 0.6, Filter1 Morph 0.6
Metal Pad | fx=25,22 fx_reverb_on=0 fx_ring_freq=220 fx_ring_mix=0.3 fx_width=0.5 fx_width_mix=1 amp_release=0.6 amp_velocity=0 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=1 f1_morph=1 f1_keytrack=1 f1_env=0 | m4=RING: fx_ring_mix 0.5; m1=TONE: Filter1 Cutoff 0.6, Filter1 Morph 0.6
Freeze Pad | fx=30,13 fx_aw_algo=14 fx_aw_p1=0.7 fx_aw_p2=0.5 amp_attack=3.2 fx_reverb_type=3 fx_reverb_mix=0.35 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 osc4_semi+=12 osc5_semi+=12 osc6_semi+=12 f1_type=35 f1_slope=0 f1_cutoff=600 f1_reso=0.9 f1_morph=1 | m2=MORPH: Osc1 Frame 0.6, Filter1 Reso 0.4; m4=AIR: Reverb Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
LFO 4 Sweep | fx=13 fx_reverb_type=4 fx_reverb_mix=0.35 osc1_table=17 osc2_table=17 fm_amount=0.35 f1_type=32 f1_slope=0 f1_cutoff=1500 f1_reso=0.6 f1_morph=0.4 f1_env=0 | m4=SPRING: Reverb Mix 0.4; m1=TONE: Filter1 Cutoff 0.9, Filter1 Morph 0.8
Ladder Swell Pad | fx=30 fx_reverb_on=0 fx_aw_algo=11 fx_aw_p1=1 amp_attack=2.2 amp_release=1.4 osc1_warp=1 osc1_warp_amt=0.5 f1_type=29 f1_slope=0 f1_cutoff=600 f1_reso=0.9 f1_morph=1 | m4=RESO: Filter1 Reso 0.25; m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Vector Corner Morph | fx=26,13 fx_octaver_mix=0.3 fx_reverb_type=2 fx_reverb_mix=0.2 f1_type=36 f1_slope=0 f1_cutoff=1200 f1_reso=0.9 f1_morph=0.9 | m4=OCTAVE: fx_octaver_mix 0.5; m3=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6

Granular Pad | fx=2,3,22 fx_reverb_on=0 fx_drive_on=1 fx_drive_amount=0.5 fx_drive_mix=0.7 fx_crush_on=1 fx_crush_bits=6 fx_crush_down=3 fx_crush_mix=0.5 fx_width=0.4 fx_width_mix=1 amp_release=1.0 f1_type=30 f1_slope=0 f1_cutoff=600 f1_reso=0.9 f1_morph=1 f1_env=0 | m4=FILTH: Drive Amount 0.4, Crush Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
EQ Sculpted Pad | fx=29,22 fx_reverb_on=0 fx_width=0.3 fx_width_mix=1 amp_attack=0.03 amp_decay=1.2 amp_sustain=0.5 amp_release=0.5 amp_velocity=0 osc1_table=31 osc2_table=31 f1_type=31 f1_slope=0 f1_cutoff=600 f1_reso=0.85 f1_morph=1 | m1=LOW: EQ Low Gain 1, Filter1 Cutoff -0.3; m4=WIDTH: fx_width 1.2
Route Split Pad | fx=7,9 fx_reverb_on=0 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=7 fx_delay_feedback=0.4 fx_delay_mix=0.3 fx_delay_pingpong=1 amp_attack=0.06 amp_decay=1.2 amp_sustain=0.08 amp_release=0.8 | m4=ECHO: Delay Mix 0.4, Delay Feedback 0.3
Vocal Pad | fx=15,9 fx_reverb_on=0 amp_attack=0.3 amp_release=1.0 f1_type=36 f1_slope=0 f1_cutoff=750 f1_reso=0.9 f1_morph=0.9 | m4=ECHO: Delay Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Feedback Choir | amp_attack=2.8 fx_reverb_type=4 osc1_table=25 osc2_table=25 f1_type=33 f1_slope=0 f1_cutoff=960 f1_reso=0.25 f1_morph=0.9 |
MPE Glass | fx=30 fx_reverb_on=0 fx_aw_algo=25 fx_aw_p1=0.8 amp_attack=0.01 amp_decay=2.0 amp_sustain=0.25 fx_slot1_mix=0.1 f1_type=34 f1_slope=0 f1_cutoff=1500 f1_reso=0.9 f1_morph=1 | m4=CATHEDRAL: fx_slot1_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
# ---- Basses: archetypes (diversity pass 1) ----
# Airwindows saturation instead of the one Drive.
Neuro Wobble | fx=30 fx_drive_on=0 fx_aw_algo=35 fx_aw_p1=0.6 fx_aw_p2=0.8 fx_aw_p3=1 fx_slot1_mix=0.1 f1_type=29 f1_slope=0 f1_cutoff=300 f1_reso=0.8 f1_morph=1 | m3=SLAM: fx_slot1_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Rust Bass | fx=3,30 fx_drive_on=0 fx_aw_algo=33 fx_aw_p1=0.5 fx_slot2_mix=0.1 | m3=WRECK: fx_slot2_mix 0.9
Warp Wobble | fx=30,20,21 fx_drive_on=0 fx_aw_algo=5 fx_aw_p1=0.7 fx_aw_p2=0.1 fx_aw_p3=0.6 fx_aw_p4=0.8 fx_aw_p5=1 fx_slot1_mix=0.1 | m4=DIRT: fx_slot1_mix 0.9
Neon Bass | fx=30,22 fx_drive_on=0 fx_aw_algo=7 fx_aw_p1=0.6 fx_aw_p2=0.7 fx_width=1.8 fx_width_mix=0.7 f1_type=35 f1_slope=0 f1_cutoff=1100 f1_reso=0.85 f1_morph=0.2 | m4=WIDTH: fx_width 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
FM Growl | fx=30 fx_drive_on=0 fx_aw_algo=3 fx_aw_p1=0.6 fx_aw_p2=0.05 fx_aw_p3=0.8 fx_aw_p4=1 fx_slot1_mix=0.1 | m3=DENSITY: fx_slot1_mix 0.9
# Clean and pure: no drive at all.
Sub Destroyer | fx=4 fx_drive_on=0 amp_attack=0.005 amp_decay=0.35 amp_sustain=0 amp_release=0.2 osc1_table=8 | m4=LENGTH: Amp Decay 0.5
# Wide stereo basses.
Reese Ripper | fx=2,30 fx_aw_algo=29 fx_aw_p1=0.9 fx_aw_p2=0.6 fx_aw_p3=1 fx_slot2_mix=0.1 f1_type=36 f1_slope=0 f1_cutoff=300 f1_reso=0.9 f1_morph=0.9 | m4=WIDTH: fx_slot2_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Tape Reese | fx=30,7 fx_drive_on=0 fx_delay_on=0 fx_aw_algo=1 fx_aw_p1=0.7 fx_aw_p2=0.4 fx_aw_p3=0.6 fx_aw_p4=0.7 fx_aw_p5=0.5 fx_chorus_on=1 fx_chorus_mix=0.4 | m4=CHORUS: fx_chorus_mix 0.5
# Slow and swelling.
LFO 3 Wobble | amp_attack=0.09 fx=2,14,21 fx_flanger_rate=0.2 fx_flanger_depth=0.6 fx_flanger_feedback=0.5 fx_flanger_mix=0.4 | m4=JET: fx_flanger_mix 0.5
Morph Filter Bass | amp_attack=0.45 fx=6,2 fx_phaser_on=1 fx_phaser_rate=0.3 fx_phaser_depth=0.8 fx_phaser_feedback=0.5 fx_phaser_mix=0.5 voice_mode=0 f1_type=37 f1_slope=0 f1_cutoff=300 f1_reso=1 f1_morph=0.9 | m4=PHASE: fx_phaser_mix 0.5; m2=CUTOFF: Filter1 Cutoff 0.9, Filter1 Morph 0.8
Resonator Bass | amp_attack=0.04 amp_decay=0.9 amp_sustain=0.3 fx=2,13 fx_reverb_on=1 fx_reverb_type=0 fx_reverb_size=0.3 fx_reverb_mix=0.15 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=1 f1_morph=1 f1_keytrack=1 f1_env=3 | m4=ROOM: Reverb Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
# Bright, spaced and decaying.
Sync Stab Bass | f1_cutoff=5000 fx=2,9 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=9 fx_delay_feedback=0.3 fx_delay_mix=0.2 fx_delay_pingpong=1 | m4=ECHO: Delay Mix 0.4
Spring Bounce Bass | fx=2,13 fx_reverb_on=1 fx_reverb_type=4 fx_reverb_size=0.4 fx_reverb_mix=0.2 f1_type=29 f1_slope=0 f1_cutoff=700 f1_reso=0.7 f1_morph=0.3 | m4=SPRING: Reverb Mix 0.4; m2=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Crushed Trap | fx_crush_bits=5 fx_crush_down=6 | m4=SWELL: Amp Attack 1, Filter1 Cutoff -0.6
Acid Stab | amp_decay=0.25 amp_sustain=0 fx_delay_mix=0.3 |
Hyper Reese | fx=20,15 fx_dim_rate=0.3 fx_dim_depth=0.7 fx_dim_mix=0.6 amp_attack=0.03 | m4=WIDTH: fx_dim_mix 0.4
Through-Zero Growl | fx=30,21 fx_ott_mix=0 fx_aw_algo=23 fx_aw_p1=0.7 fx_aw_p2=0.4 fx_aw_p3=0.4 fx_aw_p4=0.6 fx_aw_p5=1 f1_type=34 f1_slope=0 f1_cutoff=480 f1_reso=1 f1_morph=0.9 | m4=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6

Shift Bass | fx=7,29 fx_drive_on=0 fx_chorus_on=1 fx_chorus_rate=0.5 fx_chorus_depth=0.5 fx_chorus_mix=0.5 f1_cutoff=3000 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 | m4=CHORUS: fx_chorus_mix 0.5
Modulated Route Bass | fx=8,7,29,19 fx_drive_on=0 fx_haas_delay=12 fx_haas_mix=0.6 fx_chorus_on=1 fx_chorus_mix=0.4 amp_attack=0.15 fx_util_gain=6 | m4=WIDTH: fx_haas_mix 0.4
MS-20 Growl | fx_reverb_mix=0.35 fx_reverb_type=2 amp_attack=0.03 | m3=SCREAM: Filter1 Reso 0.4, Filter1 Drive 0.8, Drive Amount 0.5
Wavefold Sub | fx=21 fx_ott_mix=0 amp_decay=0.18 amp_sustain=0 amp_release=0.1 | m4=LENGTH: Amp Decay 0.5
West Folded Bass | fx=30 fx_drive_on=0 fx_aw_algo=7 fx_aw_p1=0.65 fx_aw_p2=0.7 amp_velocity=0 |
Tape Sub | fx=30 fx_amp_on=0 fx_aw_algo=32 fx_aw_p1=0.45 fx_aw_p2=0.6 fx_aw_p3=0.7 fx_aw_p4=0.8 | m2=DIRT: Filter1 Drive 1
Diode Acid | amp_decay=0.3 amp_sustain=0.1 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 f1_type=32 f1_slope=0 f1_cutoff=300 f1_reso=0.95 f1_morph=0.9 | m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Clocked Bass | fx=30,16 fx_drive_on=0 fx_aw_algo=9 fx_aw_p1=0.6 fx_aw_p2=0.4 fx_aw_p3=1 fx_gate_div=4 fx_gate_mix=0.8 fx_slot1_mix=0.1 f1_type=34 f1_slope=0 f1_cutoff=300 f1_reso=0.9 f1_morph=1 | m3=COILS: fx_slot1_mix 0.9; m4=CHOP: fx_gate_mix 0.2; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Pluck Sub Bass | fx=29,4,9 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=9 fx_delay_feedback=0.25 fx_delay_mix=0.18 f1_env=0.6 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 | m4=ECHO: Delay Mix 0.4
Ladder Sub | fx=29 fx_drive_on=0 amp_velocity=0 amp_attack=0.06 |
Deep House Bass | fx=4,7 fx_reverb_on=0 fx_chorus_on=1 fx_chorus_mix=0.35 osc1_table=19 amp_velocity=0.7 f1_type=35 f1_slope=0 f1_cutoff=300 f1_reso=0.9 f1_morph=1 | m4=BODY: Amp Sustain 0.4; m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
DX Stack Bass | fx=29,30,7,9 fx_drive_on=0 fx_aw_algo=2 fx_aw_p1=0.7 fx_aw_p2=0.6 fx_chorus_on=1 fx_chorus_mix=0.4 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=3 fx_delay_feedback=0.3 fx_delay_mix=0.15 | m2=BITE: FM Amount 0.5, Osc2 Level 0.5; m4=ECHO: Delay Mix 0.4
Diamond Growl | fx=30,6 fx_drive_on=0 fx_aw_algo=6 fx_aw_p1=0.8 fx_phaser_on=1 fx_phaser_rate=0.25 fx_phaser_depth=0.7 fx_phaser_mix=0.5 amp_attack=0.06 f1_cutoff=2500 fx_slot1_mix=0.1 | m1=CUTOFF: Filter1 Cutoff -0.8; m4=DRIVE: Filter1 Drive 1, Filter1 Reso 0.3
Feedback Chain Bass | fx=25,2,30 fx_ring_freq=110 fx_ring_mix=0.3 fx_aw_algo=24 fx_aw_p1=0.5 fx_aw_p2=0.5 fx_aw_p3=0.5 fx_aw_p4=0.7 fx_aw_p5=0.35 amp_attack=0.34 amp_release=1.2 | m1=FEEDBACK: FM Osc3 Feedback 0.8, FM Feedback 0.5; m3=CUTOFF: Filter1 Cutoff -0.6; m4=RING: fx_ring_mix 0.5
Warp Stack Bass | fx=30 fx_drive_on=0 fx_aw_algo=32 fx_aw_p1=0.3 fx_aw_p2=0.6 fx_aw_p3=0.7 fx_aw_p4=0.7 f1_type=29 f1_slope=0 f1_cutoff=750 f1_reso=1 f1_morph=0.9 f1_keytrack=0.3 | m3=PUNCH: Mod Env Decay 0.4, Osc1 Warp 0.5; m4=DRIVE: Drive Amount 1, Filter1 Drive 1
Rip Bass | fx=30,23 fx_drive_on=0 fx_aw_algo=36 fx_aw_p1=0.7 fx_aw_p2=0.7 fx_aw_p3=1 fx_trem_rate=8 fx_trem_depth=0.6 fx_trem_shape=4 fx_slot1_mix=0.1 f1_type=32 f1_slope=0 f1_cutoff=400 f1_reso=0.8 f1_morph=0.5 f1_env=3 f1_drive=2.5 | m3=HEAT: fx_slot1_mix 0.9; m4=CHOP: fx_trem_depth 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Rubber Mono | fx=30 fx_drive_on=0 fx_aw_algo=22 fx_aw_p1=0.7 fx_aw_p2=0.5 fx_aw_p3=0.5 amp_attack=0.02 glide=0.12 | m4=PWM: Osc1 Warp 0.4
Lorenz Bass | fx=2,30 fx_aw_algo=28 fx_aw_p1=0.6 fx_aw_p2=0.4 fx_aw_p3=0.3 fx_aw_p4=0.5 fx_aw_p5=0.8 amp_attack=0.12 amp_release=0.8 fx_slot2_mix=0.1 | m4=SPACE: fx_slot2_mix 0.9
Steiner Growl | fx=2,27 fx_vowel_morph=0.3 fx_vowel_mix=0.6 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 | m4=VOWEL: fx_vowel_morph 0.5
Stick-Slip Growl | fx=30,24 fx_drive_on=0 fx_aw_algo=8 fx_aw_p1=0.75 fx_slot1_mix=0.5 fx_shifter_shift=3 fx_shifter_mix=0.3 | m4=SHIFT: fx_shifter_mix 0.5
Octave Hyper Bass | fx=20,22 fx_width=1.8 fx_width_mix=0.8 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 amp_attack=0.05 amp_velocity=0 f1_cutoff=2500 | m4=FOCUS: Width -1.5
# ---- Leads: archetypes (diversity pass 1) ----
# Dry and mono: no echo or reverb.
Ladder Lead | fx=2 fx_delay_on=0 fx_reverb_on=0 | m4=DRIVE: Drive Amount 0.4
Chip Lead | fx=3 fx_delay_on=0 amp_velocity=0 | m4=CRUNCH: Crush Mix 0.5
Sync Scream | fx=1,19 fx_delay_on=0 fx_reverb_on=0 fx_util_mono=1 amp_velocity=0 f1_type=34 f1_slope=0 f1_cutoff=1600 f1_reso=0.9 f1_morph=0.9 | m4=DRIVE: fx_amp_drive 0.4; m2=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
# Dark and mellow.
Soft Saw Lead | fx=30 fx_reverb_on=0 fx_aw_algo=10 fx_aw_p1=0.8 amp_attack=0.01 f1_type=34 f1_slope=0 f1_cutoff=800 f1_reso=0.9 f1_morph=1 | m3=SWELL: Amp Attack 1, Filt Env Attack 1, Filter1 Cutoff -0.4; m4=OPEN: Filter1 Cutoff 0.4; m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Theremin | f1_cutoff=1500 fx=30,13 fx_aw_algo=26 fx_aw_p1=0.4 fx_aw_p2=0.5 fx_aw_p3=0.6 fx_aw_p4=0.8 fx_reverb_on=0 fx_slot1_mix=0.1 | m4=ROOM: fx_slot1_mix 0.9
# Slow swells.
Supersaw Anthem | amp_attack=0.55 fx=7,13 fx_delay_on=0 fx_chorus_on=1 fx_chorus_mix=0.4 f1_type=36 f1_slope=0 f1_cutoff=1600 f1_reso=0.9 f1_morph=0.9 | m4=SPACE: Reverb Mix 0.4; m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
FM Brass Lead | amp_attack=0.9 amp_decay=1.5 amp_sustain=0.2 f1_cutoff=1200 fx_reverb_type=0 | m1=BRIGHT: Osc2 Level 0.6, FM Amount 0.4; m3=SWELL: Amp Attack -0.4
MPE Lead | amp_attack=0.7 fx=30 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.5 fx_aw_p2=0.6 fx_aw_p3=0.4 fx_aw_p4=0.8 fx_aw_p5=0.8 fx_slot1_mix=0.1 f1_type=37 f1_slope=0 f1_cutoff=1600 f1_reso=1 f1_morph=1 | m2=MORPH: Osc1 Frame 1, Filter1 Reso 0.6; m4=SPACE: fx_slot1_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
# Wide, spaced and decaying.
Glass Lead | amp_sustain=0.15 amp_decay=1.2 fx=30,9 fx_reverb_on=0 fx_aw_algo=29 fx_aw_p1=1 fx_aw_p2=0.3 fx_aw_p3=1 fx_slot1_mix=0.1 | m3=SHIMMER: Delay Mix 0.4, Delay Feedback 0.3; m4=WIDTH: fx_slot1_mix 0.9
Stereo Bend Lead | fx=9,30 fx_reverb_on=0 fx_aw_algo=28 fx_aw_p1=0.8 fx_aw_p2=0.2 fx_aw_p3=0.7 fx_aw_p4=0.6 fx_aw_p5=0.8 fx_slot2_mix=0.1 | m4=SPACE: fx_slot2_mix 0.9
Legato Glide Lead | fx=9,30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.5 fx_aw_p2=0.5 fx_aw_p3=0.4 fx_aw_p4=0.3 fx_aw_p5=0.8 fx_slot2_mix=0.1 | m4=CHAMBER: fx_slot2_mix 0.9
Formant Shifter | fx=30 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=25 fx_aw_p1=0.8 fx_slot1_mix=0.1 | m4=SPACE: fx_slot1_mix 0.9
Frozen Grain Lead | fx=30 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.7 fx_aw_p2=0.4 fx_aw_p3=0.7 fx_aw_p4=1 fx_aw_p5=0.8 fx_slot1_mix=0.1 | m4=GALAXY: fx_slot1_mix 0.9
# Airwindows drive.
Acid Squelch Lead | fx=30,9 fx_drive_on=0 fx_aw_algo=36 fx_aw_p1=0.65 fx_aw_p2=0.6 fx_aw_p3=1 fx_slot1_mix=0.1 | m2=SQUELCH: Filter1 Reso 0.25, Filter1 Env Amt 0.5, Filter1 Cutoff -0.2; m4=HEAT: fx_slot1_mix 0.9
Scream Lead | fx=30,9 fx_reverb_on=0 fx_aw_algo=7 fx_aw_p1=0.75 fx_aw_p2=0.7 f1_type=31 f1_slope=0 f1_cutoff=1500 f1_reso=0.8 f1_morph=0.8 | m4=ECHO: Delay Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Key FM Scream | fx=30,9 fx_drive_on=0 fx_reverb_on=0 fx_aw_algo=4 fx_aw_p1=0.5 fx_aw_p2=0.1 fx_aw_p3=0.8 fx_aw_p4=1 fx_slot1_mix=0.1 | m4=DRIVE: fx_slot1_mix 0.9
Octave Screamer | fx=30,21 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=35 fx_aw_p1=0.6 fx_aw_p2=0.8 fx_aw_p3=1 fx_slot1_mix=0.1 f1_type=31 f1_slope=0 f1_cutoff=900 f1_reso=0.85 f1_morph=0.4 f1_env=0 | m4=SLAM: fx_slot1_mix 0.9
# Lo-fi and rhythm.
Tape Lead | fx=30 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=0 fx_aw_p1=0.65 fx_aw_p2=0.5 fx_aw_p3=0.5 fx_aw_p4=0.8 fx_aw_p5=0.5 f1_type=35 f1_slope=0 f1_cutoff=1600 f1_reso=0.9 f1_morph=0.9 | m2=MORPH: Osc1 Frame 1, Filter1 Reso 0.6; m4=WOW: Drift 0.5; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Glitch Lead | fx=30,9 fx_crush_on=0 fx_aw_algo=32 fx_aw_p1=0.3 fx_aw_p2=0.5 fx_aw_p3=0.8 fx_aw_p4=0.8 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=1 f1_morph=1 f1_keytrack=1 f1_env=3 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
S&H Techno | fx=2,16 fx_delay_on=0 fx_gate_div=4 fx_gate_mix=0.7 | m4=CHOP: fx_gate_mix 0.3, Filter1 Cutoff -0.4
# The voted-down vowel leads: drier, less extreme.
Vowel Lead | fx_reverb_on=0 fx=27,2 fx_vowel_mix=0.35 |
Vowel Bank Voice | fx_reverb_mix=0.1 amp_attack=0.06 |
Talking Filter Lead | amp_attack=0.02 |
Vintage Mono Lead | fx=1 fx_delay_on=0 f1_cutoff=500 glide=0.08 amp_attack=0.012 | m4=DRIFT: Drift 0.5
SEM Classic Lead | amp_attack=0.25 fx=9 fx_reverb_on=0 fx_delay_mix=0.3 | m4=ECHO: Delay Mix 0.4
Talkbox Lead | fx=9,23 fx_reverb_on=0 fx_trem_rate=6 fx_trem_depth=0.4 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 | m4=TREMOLO: fx_trem_depth 0.5
Formant Scream II | fx=1,14 fx_reverb_on=0 fx_flanger_rate=0.3 fx_flanger_depth=0.8 fx_flanger_feedback=0.7 fx_flanger_mix=0.5 | m4=JET: fx_flanger_mix 0.5
Fifth Stack Lead | amp_attack=0.2 fx=7,30 fx_drive_on=0 fx_reverb_on=0 fx_chorus_on=1 fx_chorus_mix=0.5 fx_aw_algo=24 fx_aw_p1=0.5 fx_aw_p2=0.5 fx_aw_p3=0.5 fx_aw_p4=0.8 fx_aw_p5=0.8 fx_slot2_mix=0.1 | m4=SPACE: fx_slot2_mix 0.9
Six-Layer Stack | fx=20,22 fx_reverb_on=0 fx_ott_amount=0.5 fx_ott_mix=0.6 fx_width=1.9 fx_width_mix=0.8 | m4=WIDTH: Osc1 Pan -0.8, Osc2 Pan 0.8, Osc3 Pan -0.5, Width 0.3
Hollow Pulse Lead | fx=7,9 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=9 fx_delay_feedback=0.4 fx_delay_mix=0.3 fx_delay_pingpong=1 osc1_semi+=12 osc2_semi+=12 sub_semi+=12 f1_cutoff=8000 | m4=ECHO: Delay Mix 0.4
Fifth Engine | fx=30 fx_reverb_on=0 fx_delay_on=0 fx_aw_algo=32 fx_aw_p1=0.4 fx_aw_p2=0.5 fx_aw_p3=0.7 fx_aw_p4=0.8 | m4=SYNC: Osc1 Frame 0.5, Osc2 Frame 0.5
Ring Lead | amp_decay=0.5 amp_sustain=0.2 fx_reverb_type=0 fx_reverb_mix=0.2 | m3=INTERVAL: Osc2 Pitch 1, Osc2 Level 0.5, Ring Mod 0.3
Diode Screamer | fx=30,21 fx_delay_on=0 fx_reverb_on=0 fx_aw_algo=6 fx_aw_p1=0.9 amp_attack=0.2 amp_decay=0.8 amp_sustain=0.05 fx_slot1_mix=0.1 | m4=GRIT: Filter1 Drive 1, Filter1 Reso 0.3
# ---- Plucks: archetypes (diversity pass 1) ----
# Dry and close.
Ladder Pluck | fx=2 fx_delay_on=0 fx_reverb_on=0 fx_drive_on=1 fx_drive_amount=0.3 fx_drive_mix=0.5 | m4=DRIVE: Drive Amount 0.5
Diode Blip | fx=19 fx_delay_on=0 fx_reverb_on=0 fx_util_mono=1 amp_velocity=0 | m4=SNAP: Amp Decay 0.4
Muted Guitar | fx=1 fx_delay_on=0 | m4=AMP: fx_amp_drive 0.4
Pizzicato | fx=30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.25 fx_aw_p2=0.2 fx_aw_p3=0.5 fx_aw_p4=0.3 fx_aw_p5=0.8 fx_slot1_mix=0.1 | m4=HALL: fx_slot1_mix 0.9
Kalimba | fx=30 fx_reverb_on=0 fx_aw_algo=10 fx_aw_p1=0.85 | m1=TINE: Osc2 Level 0.6, FM Amount 0.4; m4=TAIL: Amp Release 0.7, Amp Decay 0.4
KS Marimba | fx=19 fx_reverb_on=0 amp_velocity=0.8 |
# Wide and shimmering.
Glass Mallet | fx=30,9 fx_reverb_on=0 fx_aw_algo=29 fx_aw_p1=1 fx_aw_p2=0.3 fx_aw_p3=1 fx_slot1_mix=0.1 | m4=WIDTH: fx_slot1_mix 0.9
Plucked Bell Stack | fx=30 fx_reverb_on=0 fx_aw_algo=25 fx_aw_p1=0.8 fx_slot1_mix=0.1 | m4=CATHEDRAL: fx_slot1_mix 0.9
Ratio Snap Bell | fx=30 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.6 fx_aw_p2=0.7 fx_aw_p3=0.6 fx_aw_p4=1 fx_aw_p5=0.8 fx_slot1_mix=0.1 | m4=GALAXY: fx_slot1_mix 0.9
PD Metallic Bell | fx=7,30 fx_reverb_on=0 fx_chorus_on=1 fx_chorus_mix=0.5 fx_aw_algo=28 fx_aw_p1=0.9 fx_aw_p2=0.2 fx_aw_p3=0.6 fx_aw_p4=0.7 fx_aw_p5=0.8 fx_slot2_mix=0.1 | m4=SPACE: fx_slot2_mix 0.9
# Slow-attack and bowed plucks.
Hypersaw Pluck | amp_attack=0.2 amp_decay=0.9 fx=15,9 fx_reverb_on=0 fx_dim_mix=0.6 | m4=ECHO: Delay Mix 0.4
Warp Pluck | amp_attack=0.35 fx=6,9 fx_reverb_on=0 fx_phaser_on=1 fx_phaser_rate=0.4 fx_phaser_depth=0.7 fx_phaser_mix=0.5 f1_type=29 f1_slope=0 f1_cutoff=1050 f1_reso=0.9 f1_morph=1 | m4=PHASE: fx_phaser_mix 0.5
Formant Pluck | amp_attack=0.5 amp_decay=1.2 fx=27,9 fx_reverb_on=0 fx_vowel_mix=0.4 | m4=VOWEL: fx_vowel_morph 0.5
# Lo-fi and driven.
Quad Mod Pluck | fx=30,9 fx_reverb_on=0 fx_aw_algo=32 fx_aw_p1=0.35 fx_aw_p2=0.5 fx_aw_p3=0.7 fx_aw_p4=0.7 f1_type=34 f1_slope=0 f1_cutoff=1800 f1_reso=0.6 f1_morph=0.8 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Resonator Pluck | fx=30,9 fx_reverb_on=0 fx_aw_algo=36 fx_aw_p1=0.7 fx_aw_p2=0.6 fx_aw_p3=0.8 fx_slot1_mix=0.1 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=3 | m4=HEAT: fx_slot1_mix 0.9; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Tap Pluck | fx=30 fx_reverb_on=0 fx_delay_on=0 fx_aw_algo=31 fx_aw_p1=0.5 fx_aw_p2=0.6 fx_aw_p3=0.2 fx_aw_p4=0.6 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=3.5 | m4=SWELL: Amp Attack 0.7; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Chord Stab | fx=30,9 fx_reverb_on=0 fx_aw_algo=0 fx_aw_p1=0.7 fx_aw_p2=0.5 fx_aw_p3=0.5 fx_aw_p4=0.8 fx_aw_p5=0.5 f1_type=35 f1_slope=0 f1_cutoff=1500 f1_reso=0.9 f1_morph=0.9 | m4=ECHO: Delay Mix 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Minor Chord Stab | fx=4,30 fx_reverb_on=0 fx_aw_algo=7 fx_aw_p1=0.7 fx_aw_p2=0.6 | m2=MORPH: Osc1 Frame 1, Filter1 Reso 0.6; m4=SWELL: Amp Attack 1, Filter1 Cutoff -0.6
# Echo, rhythm and motion.
Comb Harp | fx=21,9 fx_reverb_on=0 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=7 fx_delay_feedback=0.5 fx_delay_mix=0.3 fx_delay_pingpong=1 | m4=ECHO: Delay Mix 0.4
Metal Keys | fx=23,9 fx_reverb_on=0 fx_trem_rate=7 fx_trem_depth=0.5 fx_trem_shape=4 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.7 f1_morph=0.6 f1_keytrack=1 f1_env=0 | m4=TREMOLO: fx_trem_depth 0.5; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Arp Glass | fx=16,13 fx_gate_div=4 fx_gate_mix=0.6 fx_reverb_mix=0.25 f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=3 | m4=CHOP: fx_gate_mix 0.4, Filter1 Cutoff -0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Glass Keys | fx=14 fx_reverb_on=0 fx_flanger_rate=0.15 fx_flanger_depth=0.6 fx_flanger_feedback=0.5 fx_flanger_mix=0.5 | m4=JET: fx_flanger_mix 0.5
Smear Pluck | fx=11,30 fx_reverb_on=0 fx_aw_algo=26 fx_aw_p1=0.8 fx_aw_p2=0.8 fx_aw_p3=0.5 fx_aw_p4=0.6 f1_type=31 f1_slope=0 f1_cutoff=1500 f1_reso=0.7 f1_morph=0.8 | m4=SMEAR: Smear Mix 0.5; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
# Registers.
Resonant Drop | osc1_semi+=-12 osc2_semi+=-12 sub_semi+=-12 fx_reverb_on=0 fx=9 |
Nylon Twelve | fx=7,30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.3 fx_aw_p2=0.3 fx_aw_p3=0.5 fx_aw_p4=0.2 fx_aw_p5=0.3 |
# ---- Keys: archetypes (diversity pass 1; the fitted pianos keep their sound) ----
# Dry, mono organs and harpsichord.
Ladder Organ | fx=1,19 fx_reverb_on=0 fx_util_mono=1 | m3=GRIT: Filter1 Drive 1, Filter1 Reso 0.3
Six-Op Organ | fx=1,7 fx_reverb_on=0 f1_type=35 f1_slope=0 f1_cutoff=2500 f1_reso=0.5 f1_morph=0.7 | m4=DRIVE: Amp Drive 0.6
Harpsichord | fx=30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.2 fx_aw_p2=0.2 fx_aw_p3=0.6 fx_aw_p4=0.2 fx_aw_p5=0.8 fx_slot1_mix=0.1 | m1=BRIGHT: Osc1 String Damp -0.6, Filter1 Cutoff 0.4; m4=ROOM: fx_slot1_mix 0.9
Detuned Saloon | fx=29 fx_reverb_on=0 | m2=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8; m4=FELT: Osc1 String Damp 0.7, Osc1 Hammer -0.7
# Wide and echoing.
DX Keys Classic | fx=7,30,9 fx_reverb_on=0 fx_aw_algo=29 fx_aw_p1=0.9 fx_aw_p2=0.4 fx_aw_p3=1 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=9 fx_delay_feedback=0.35 fx_delay_mix=0.2 fx_delay_pingpong=1 | m1=TINE: Osc2 Level 0.6, FM Amount 0.4; m4=ECHO: Delay Mix 0.4
Two Pairs Keys | fx=15,13 fx_dim_mix=0.7 fx_reverb_type=2 | m4=CHORUS: Dimension Mix 0.3, Dimension Depth 0.6
Operator Bell | fx=30 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.6 fx_aw_p2=0.7 fx_aw_p3=0.5 fx_aw_p4=1 fx_aw_p5=0.8 fx_slot1_mix=0.1 | m1=BRIGHT: Osc2 Level 0.6, FM Amount 0.4; m4=GALAXY: fx_slot1_mix 0.9
Digital Bell Keys | fx=7,9 fx_reverb_on=0 fx_delay_on=1 fx_delay_sync=1 fx_delay_div=10 fx_delay_feedback=0.4 fx_delay_mix=0.25 fx_delay_pingpong=1 | m4=ECHO: Delay Mix 0.4
# Lo-fi.
Vinyl Dust Keys | fx=7,30 fx_crush_on=0 fx_reverb_on=0 fx_aw_algo=31 fx_aw_p1=0.4 fx_aw_p2=0.5 fx_aw_p3=0.25 fx_aw_p4=0.6 f1_type=34 f1_slope=0 f1_cutoff=900 f1_reso=0.9 f1_morph=0.9 | m4=TAIL: Amp Release 0.7, Amp Decay 0.4; m2=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Drunk Tape Keys | fx=30,7 fx_crush_on=0 fx_reverb_on=0 fx_aw_algo=37 fx_aw_p1=0.8 fx_slot1_mix=0.1 f1_type=33 f1_slope=0 f1_cutoff=900 f1_reso=0.5 f1_morph=0.9 | m1=WARBLE: fx_slot1_mix 0.9; m3=DUST: Noise Level 0.5; m4=TAIL: Amp Release 0.7, Amp Decay 0.4; m2=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Toy Piano | fx=30 fx_reverb_on=0 fx_aw_algo=32 fx_aw_p1=0.4 fx_aw_p2=0.6 fx_aw_p3=0.6 fx_aw_p4=0.7 | m4=TAIL: Amp Release 0.7, Amp Decay 0.4
# Airwindows drive.
Wurli Drive | fx=30,23 fx_amp_on=0 fx_reverb_on=0 fx_aw_algo=7 fx_aw_p1=0.7 fx_aw_p2=0.7 fx_slot1_mix=0.1 f1_type=31 f1_slope=0 f1_cutoff=1800 f1_reso=0.7 f1_morph=0.8 | m2=DRIVE: fx_slot1_mix 0.9; m4=TAIL: Amp Release 0.7, Amp Decay 0.4
Distorted Tine Rock | fx=30,13 fx_aw_algo=36 fx_aw_p1=0.7 fx_aw_p2=0.6 fx_aw_p3=1 fx_slot1_mix=0.1 | m1=DRIVE: fx_slot1_mix 0.9; m3=TONE: Filter1 Cutoff -0.7
Growl Reed | fx=30 fx_reverb_on=0 fx_aw_algo=8 fx_aw_p1=0.75 | m2=HAMMER: Osc1 Hammer -0.7, Filter1 Cutoff -0.4; m3=GRIT: Filter1 Drive 1, Filter1 Reso 0.3; m4=MELLOW: Filter1 Cutoff -0.8
# Other rooms.
Honky Hammers | fx=21,30 fx_reverb_on=0 fx_aw_algo=27 fx_aw_p1=0.3 fx_aw_p2=0.3 fx_aw_p3=0.4 fx_aw_p4=0.4 fx_aw_p5=0.3 | m3=MECHANICS: Key Noise 1, Damper Noise 1, Pedal Noise 1, Soundboard Mix 0.5
Toy Grand | fx=30 fx_reverb_on=0 fx_aw_algo=26 fx_aw_p1=0.3 fx_aw_p2=0.4 fx_aw_p3=0.6 fx_aw_p4=0.3 | m1=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8; m4=MELLOW: Filter1 Cutoff -0.8
Dense Board Piano | fx=30 fx_reverb_on=0 fx_aw_algo=28 fx_aw_p1=0.7 fx_aw_p2=0.3 fx_aw_p3=0.7 fx_aw_p4=0.6 fx_aw_p5=0.3 | m1=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8; m4=DRY: FX Slot 1 Blend -0.9
# Motion and slow attacks.
Chorused Tine | fx=6,13 fx_phaser_on=1 fx_phaser_rate=0.35 fx_phaser_depth=0.7 fx_phaser_mix=0.5 | m1=PHASE: fx_phaser_mix 0.5; m3=TONE: Filter1 Cutoff -0.7
Vintage Tine | fx=23,8 fx_reverb_on=0 fx_haas_delay=15 fx_haas_mix=0.6 | m3=TONE: Filter1 Cutoff -0.7; m4=WIDE: Haas Width 1, Haas Delay 1
Stretched Glass Keys | amp_attack=0.16 fx=7,30 fx_reverb_on=0 fx_aw_algo=25 fx_aw_p1=0.8 fx_slot2_mix=0.1 | m4=CATHEDRAL: fx_slot2_mix 0.9
Vector Strike Pad | amp_attack=0.4 fx=9,30 fx_reverb_on=0 fx_aw_algo=24 fx_aw_p1=0.6 fx_aw_p2=0.5 fx_aw_p3=0.6 fx_aw_p4=1 fx_aw_p5=0.8 fx_slot2_mix=0.1 | m4=GALAXY: fx_slot2_mix 0.9
Shimmer Pad | f1_type=37 f1_slope=0 f1_cutoff=1140 f1_reso=0.9 f1_morph=0.9 | m2=MORPH: Osc1 Frame 0.6, Filter1 Reso 0.4; m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Sub Tone 808 | | m1=TONE: Filter1 Cutoff 1, OTT Amount 0.6; m4=SWELL: Amp Attack 1, Osc1 Level -0.6
Feedback Drone | | m2=MORPH: Osc1 Frame 1, Filter1 Reso 0.6
Tape Wow Dub | | m2=MORPH: Osc1 Frame 0.6, Filter1 Reso 0.4
Sub 808 Arp | f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=3 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Vocal Chops | | m1=TONE: Filter1 Cutoff -1
FM Donk | | m1=FM: Osc2 Level 0.6, FM Amount 0.4
Ring Bass | f1_type=35 f1_slope=0 f1_cutoff=1600 f1_reso=0.9 f1_morph=0.9 f1_keytrack=0.3 f1_env=1.5 | m2=GRIT: Filter1 Drive 1, Filter1 Reso 0.3; m3=SUB: Sub Level 0.7
Distorted 808 | f1_type=33 f1_slope=0 f1_cutoff=900 f1_reso=0.6 f1_morph=0.9 | m2=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
FM Pluck | | m1=FM: Osc2 Level 0.6, FM Amount 0.4
FM E-Piano | f1_type=36 f1_slope=0 f1_cutoff=2100 f1_reso=0.9 f1_morph=0.9 | m1=TINE: Osc2 Level 0.6, FM Amount 0.4
Mellow Rhodes | f1_type=35 f1_slope=0 f1_cutoff=900 f1_reso=0.9 f1_morph=0.9 | m2=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Lo-Fi Chord | | m1=TONE: Filter1 Cutoff -0.6; m2=WOBBLE: Tape Wow 0.6, Drift 0.8
FM Sequence | | m1=FM: Osc2 Level 0.6, FM Amount 0.4
Metallic Hit | | m1=METAL: Osc2 Level 0.6, FM Amount 0.4
FM Kalimba | | m1=TINE: Osc2 Level 0.6, FM Amount 0.4; m3=WOOD: Filter1 Cutoff -1, Amp Decay -0.3
Pedal Bloom | | m1=BLOOM: Soundboard Mix 0.6, Soundboard Size 0.5, Pedal Resonance 1
Supersaw Bell Body | | m2=SIZE: Body Size 0.8, Res Amount 0.3
Tine Keys | | m3=TONE: Filter1 Cutoff -0.7
Reed Keys | | m2=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8; m3=TONE: Filter1 Cutoff -0.7
Felt Hammer Board | | m1=HAMMER: Osc1 Hammer 0.7, Osc1 String Damp -0.5
Rossler Tide | | m1=TIDE: LFO1 Rate 1, Filter1 Cutoff 0.5
Upright Piano | | m1=HAMMER: Osc1 Hammer 0.7, Osc1 String Damp -0.5
Bright Concert Grand | | m1=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8
Pedal Sustain Bloom | | m1=BLOOM: Pedal Resonance 1, Osc1 String Decay 0.4, Osc1 Coupling 0.7; m2=HAMMER: Osc1 Hammer 0.7, Osc1 String Damp -0.5
Prepared Piano Bells | | m3=FELT: Filter1 Cutoff -0.7, Osc1 Hammer -0.8
Soft Felt Piano | | m1=FELT: Filter1 Cutoff -0.8, Osc1 Hammer -0.8; m2=BOARD: Soundboard Mix 0.7, Soundboard Size 0.5, Soundboard Tone 0.4
Bark Rhodes | | m3=DRIVE: Amp Drive 0.6, Drive Amount 0.5
Mellow Reed Bell | | m3=TONE: Filter1 Cutoff -0.7
One Drives Three | | m3=TONE: Filter1 Cutoff -0.6
Inharmonic Pluck | | m2=DEPTH: FM Osc3 > Osc2 0.6, FM Amount 0.4
Euclid Trance Gate | | m2=TAIL: Amp Release 0.7, Delay Mix 0.3
Glass Chime Body | | m3=SIZE: Body Size 0.8, Res Amount 0.3
FM Struck Body | | m3=MATERIAL: Body Material 0.8, Res Amount 0.3
Sympathetic Body Drone | | m3=SIZE: Body Size 0.8, Res Amount 0.3
Comb Morph Pluck | | m2=RING: Filter1 Reso 0.2, Filter1 Morph 0.5
Oversampled Grind | | m3=DRIVE: Filter1 Drive 1, Filter1 Reso 0.3
Metal Hat | | m4=WIDTH: Osc1 Pan -0.8, Width 0.5, fx_slot1_mix 0.9

# ---- Filter characters (diversity pass 2): the Airwindows filters and the Disperser
Per-Voice Wobble | f1_type=37 f1_slope=0 f1_cutoff=500 f1_reso=0.9 f1_morph=0.7 f1_env=0 |
Dark Ladder Pluck Bass | f1_type=35 f1_slope=0 f1_cutoff=300 f1_reso=0.9 f1_morph=0.9 | m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
OTA Squelch | f1_type=36 f1_slope=0 f1_cutoff=450 f1_reso=0.7 f1_morph=0.6 f1_env=3 | m1=CUTOFF: Filter1 Cutoff 0.4, Filter1 Morph 0.6
FM Monster | f1_type=29 f1_slope=0 f1_cutoff=1600 f1_reso=0.9 f1_morph=1 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Resonant Scream | f1_type=32 f1_slope=0 f1_cutoff=800 f1_reso=0.9 f1_morph=0.9 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Rig Lead | f1_type=35 f1_slope=0 f1_cutoff=1600 f1_reso=0.8 f1_morph=0.9 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Celeste | f1_type=37 f1_slope=0 f1_cutoff=2200 f1_reso=0.9 f1_morph=0.9 |
Vibraphone | f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=0.95 f1_morph=1 f1_keytrack=1 f1_env=0 |
Virus Pluck | f1_type=38 f1_slope=0 f1_cutoff=261.63 f1_reso=1 f1_morph=1 f1_keytrack=1 f1_env=4 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
Hyper Pluck | f1_type=32 f1_slope=0 f1_cutoff=800 f1_reso=0.9 f1_morph=0.9 f1_env=3 | m1=TONE: Filter1 Cutoff 0.4, Filter1 Morph 0.6
)VOICING";
}
} // namespace Presets
