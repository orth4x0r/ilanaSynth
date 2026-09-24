#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace Presets
{
struct Value
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    std::vector<Value> values;
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init", {} },

        { "Rip Bass", {
            { "osc1_table", 7 }, { "osc1_frame", 0.25f }, { "osc1_level", 0.85f },
            { "osc1_unison", 3 }, { "osc1_detune", 12.0f }, { "osc1_spread", 0.4f },
            { "sub_level", 0.5f }, { "sub_shape", 1 }, { "sub_octave", 1 },
            { "f1_slope", 1 }, { "f1_cutoff", 400.0f }, { "f1_reso", 0.5f },
            { "f1_drive", 3.0f }, { "f1_env", 2.5f }, { "f1_keytrack", 0.3f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.25f }, { "fe_sustain", 0.0f }, { "fe_release", 0.2f },
            { "amp_decay", 0.5f }, { "amp_sustain", 0.6f }, { "amp_release", 0.15f },
            { "amp_velocity", 0.5f }, { "filter_velocity", 0.7f },
            { "glide", 0.05f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 5.0f }, { "fx_drive_mix", 0.6f },
        } },

        { "Scream Lead", {
            { "osc1_table", 4 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.8f },
            { "osc1_unison", 4 }, { "osc1_detune", 18.0f }, { "osc1_spread", 0.7f },
            { "sub_level", 0.2f },
            { "f1_type", 1 }, { "f1_cutoff", 2500.0f }, { "f1_reso", 0.6f },
            { "f1_drive", 4.0f }, { "f1_env", 1.2f },
            { "fe_decay", 1.5f }, { "fe_sustain", 0.4f },
            { "glide", 0.08f },
            { "lfo1_rate", 5.5f },
            { "mod1_src", 1 }, { "mod1_dst", 1 }, { "mod1_amt", 0.01f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.2f },
        } },

        { "FM Monster", {
            { "osc1_table", 3 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.8f },
            { "osc2_on", 1 }, { "osc2_table", 7 }, { "osc2_frame", 0.3f }, { "osc2_level", 0.5f },
            { "osc2_semi", -12 }, { "osc2_unison", 2 }, { "osc2_detune", 10.0f },
            { "sub_level", 0.4f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.4f }, { "f1_drive", 5.0f }, { "f1_env", 2.0f },
            { "lfo1_sync", 1 }, { "lfo1_div", 2 },
            { "mod1_src", 1 }, { "mod1_dst", 2 }, { "mod1_amt", 0.4f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
        } },

        { "Hard Sync Stab", {
            { "osc1_table", 1 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.9f },
            { "osc1_unison", 2 }, { "osc1_detune", 8.0f },
            { "f1_cutoff", 8000.0f }, { "f1_reso", 0.3f }, { "f1_drive", 3.0f },
            { "fe_decay", 0.15f }, { "fe_sustain", 0.0f },
            { "amp_decay", 0.2f }, { "amp_sustain", 0.0f }, { "amp_release", 0.15f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.15f },
        } },

        { "Sub Destroyer", {
            { "osc1_table", 7 }, { "osc1_frame", 0.15f }, { "osc1_level", 0.7f },
            { "sub_level", 0.8f }, { "sub_shape", 2 }, { "sub_octave", 1 },
            { "f1_slope", 1 }, { "f1_cutoff", 300.0f }, { "f1_reso", 0.2f }, { "f1_drive", 6.0f },
            { "amp_sustain", 1.0f }, { "amp_release", 0.4f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 7.0f }, { "fx_drive_mix", 0.7f },
        } },

        { "Comb Pluck", {
            { "osc1_table", 5 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.85f },
            { "f1_cutoff", 4000.0f }, { "f1_reso", 0.4f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.18f }, { "fe_sustain", 0.0f }, { "fe_release", 0.2f },
            { "amp_decay", 0.4f }, { "amp_sustain", 0.0f }, { "amp_release", 0.25f },
            { "mod1_src", 4 }, { "mod1_dst", 2 }, { "mod1_amt", 0.5f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.3f },
        } },

        { "Neuro Wobble", {
            { "osc1_table", 7 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 25.0f }, { "osc1_spread", 0.6f },
            { "sub_level", 0.5f },
            { "f1_slope", 1 }, { "f1_cutoff", 1200.0f }, { "f1_reso", 0.65f }, { "f1_drive", 4.0f },
            { "lfo1_sync", 1 }, { "lfo1_div", 3 },
            { "mod1_src", 1 }, { "mod1_dst", 9 }, { "mod1_amt", 0.35f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 6.0f }, { "fx_drive_mix", 0.65f },
        } },

        { "Scream Wobble", {
            { "osc1_table", 4 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.8f },
            { "osc1_unison", 2 }, { "osc1_detune", 14.0f },
            { "f1_type", 1 }, { "f1_cutoff", 1800.0f }, { "f1_reso", 0.5f }, { "f1_drive", 3.0f },
            { "lfo1_sync", 1 }, { "lfo1_div", 2 },
            { "mod1_src", 1 }, { "mod1_dst", 2 }, { "mod1_amt", 0.5f },
            { "mod2_src", 1 }, { "mod2_dst", 9 }, { "mod2_amt", 0.3f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "Metal Pad", {
            { "osc1_table", 3 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.7f },
            { "osc2_on", 1 }, { "osc2_table", 4 }, { "osc2_frame", 0.6f }, { "osc2_level", 0.5f },
            { "osc2_unison", 3 }, { "osc2_detune", 20.0f }, { "osc2_spread", 0.8f },
            { "f1_cutoff", 3000.0f }, { "f1_drive", 2.0f },
            { "amp_attack", 0.8f }, { "amp_release", 3.0f }, { "amp_sustain", 0.8f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.8f }, { "fx_reverb_mix", 0.45f },
        } },

        { "PWM Pulse", {
            { "osc1_table", 6 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 10.0f },
            { "sub_level", 0.3f },
            { "f1_cutoff", 9000.0f }, { "f1_reso", 0.25f },
            { "lfo1_rate", 0.6f },
            { "mod1_src", 1 }, { "mod1_dst", 2 }, { "mod1_amt", 0.35f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.4f },
        } },

        { "Doom Drone", {
            { "osc1_table", 7 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.75f },
            { "osc1_unison", 5 }, { "osc1_detune", 30.0f }, { "osc1_spread", 0.8f },
            { "sub_level", 1.0f }, { "sub_shape", 1 }, { "sub_octave", 1 },
            { "noise_level", 0.15f },
            { "f1_slope", 1 }, { "f1_cutoff", 700.0f }, { "f1_reso", 0.3f }, { "f1_drive", 8.0f },
            { "amp_attack", 0.3f }, { "amp_sustain", 1.0f }, { "amp_release", 4.0f },
            { "glide", 0.3f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 8.0f }, { "fx_drive_mix", 0.7f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Reese Ripper", {
            { "osc1_table", 7 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.75f },
            { "osc1_unison", 4 }, { "osc1_detune", 28.0f }, { "osc1_spread", 0.7f },
            { "osc2_on", 1 }, { "osc2_table", 7 }, { "osc2_frame", 0.5f }, { "osc2_level", 0.75f },
            { "osc2_unison", 4 }, { "osc2_detune", 22.0f }, { "osc2_spread", 0.7f },
            { "sub_level", 0.6f },
            { "f1_slope", 1 }, { "f1_cutoff", 900.0f }, { "f1_reso", 0.4f }, { "f1_drive", 5.0f },
            { "f1_env", 1.5f }, { "f1_keytrack", 0.25f },
            { "amp_sustain", 0.9f }, { "amp_release", 0.4f },
            { "glide", 0.06f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 5.0f }, { "fx_drive_mix", 0.55f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.25f },
        } },

        { "Acid Stab", {
            { "osc1_table", 0 }, { "osc1_frame", 0.45f }, { "osc1_level", 0.9f },
            { "f1_slope", 1 }, { "f1_cutoff", 600.0f }, { "f1_reso", 0.75f }, { "f1_drive", 5.0f },
            { "f1_env", 3.5f }, { "f1_keytrack", 0.5f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.2f }, { "fe_sustain", 0.0f }, { "fe_release", 0.15f },
            { "amp_decay", 0.3f }, { "amp_sustain", 0.2f }, { "amp_release", 0.12f },
            { "filter_velocity", 0.8f },
            { "glide", 0.04f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 6.0f }, { "fx_drive_mix", 0.6f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.3f },
        } },

        { "Virus Hypersaw", {
            { "osc1_table", 7 }, { "osc1_frame", 0.1f }, { "osc1_level", 0.85f },
            { "osc1_unison", 7 }, { "osc1_detune", 22.0f }, { "osc1_spread", 0.8f },
            { "sub_level", 0.35f },
            { "f1_cutoff", 9000.0f }, { "f1_reso", 0.2f }, { "f1_drive", 2.0f }, { "f1_env", 0.8f },
            { "amp_attack", 0.02f }, { "amp_release", 0.5f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.35f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_mix", 0.22f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "Virus Pluck", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.85f }, { "osc1_string_damp", 0.3f },
            { "osc1_level", 0.9f },
            { "sub_level", 0.2f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.2f }, { "f1_drive", 2.0f },
            { "fe_decay", 0.3f }, { "fe_sustain", 0.0f },
            { "amp_decay", 0.6f }, { "amp_sustain", 0.2f }, { "amp_release", 0.3f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 440.0f }, { "fx_comb_feedback", 0.55f }, { "fx_comb_mix", 0.35f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.28f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "KS Bell", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.96f }, { "osc1_string_damp", 0.12f },
            { "osc1_level", 0.85f },
            { "f1_cutoff", 12000.0f }, { "f1_reso", 0.1f },
            { "amp_decay", 0.8f }, { "amp_sustain", 0.0f }, { "amp_release", 1.5f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 660.0f }, { "fx_comb_feedback", 0.6f }, { "fx_comb_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.7f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Virus Pad", {
            { "osc1_table", 7 }, { "osc1_frame", 0.2f }, { "osc1_level", 0.7f },
            { "osc1_unison", 4 }, { "osc1_detune", 18.0f }, { "osc1_spread", 0.9f },
            { "sub_level", 0.25f },
            { "f1_cutoff", 4500.0f }, { "f1_reso", 0.15f }, { "f1_drive", 1.5f },
            { "amp_attack", 0.8f }, { "amp_sustain", 0.85f }, { "amp_release", 3.0f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.25f }, { "fx_phaser_depth", 0.7f },
            { "fx_phaser_feedback", 0.5f }, { "fx_phaser_mix", 0.5f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.8f }, { "fx_reverb_mix", 0.35f },
        } },

        { "String Drone", {
            { "osc1_mode", 1 }, { "osc1_excite", 1 }, { "osc1_string_sustain", 0.3f },
            { "osc1_string_decay", 0.92f }, { "osc1_string_damp", 0.25f }, { "osc1_level", 0.7f },
            { "osc2_on", 1 }, { "osc2_mode", 1 }, { "osc2_excite", 2 }, { "osc2_string_sustain", 0.15f },
            { "osc2_semi", 12 }, { "osc2_level", 0.5f },
            { "f1_cutoff", 3500.0f }, { "f1_reso", 0.2f },
            { "amp_attack", 0.4f }, { "amp_sustain", 1.0f }, { "amp_release", 4.0f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.12f }, { "fx_phaser_depth", 0.6f }, { "fx_phaser_mix", 0.45f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.4f },
            { "drift", 0.3f },
        } },

        { "Tape Reese", {
            { "osc1_table", 7 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.8f },
            { "osc1_unison", 4 }, { "osc1_detune", 30.0f }, { "osc1_spread", 0.6f },
            { "sub_level", 0.5f },
            { "f1_slope", 1 }, { "f1_cutoff", 700.0f }, { "f1_reso", 0.4f }, { "f1_drive", 5.0f }, { "f1_env", 1.2f },
            { "amp_sustain", 0.9f }, { "amp_release", 0.3f },
            { "glide", 0.05f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 5.0f }, { "fx_drive_mix", 0.6f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.55f },
            { "fx_delay_damping", 0.5f }, { "fx_delay_pitch", -7.0f }, { "fx_delay_wow", 0.3f }, { "fx_delay_mix", 0.35f },
        } },

        { "Resonator Bass", {
            { "osc1_table", 7 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.8f },
            { "osc1_unison", 2 }, { "osc1_detune", 12.0f },
            { "sub_level", 0.6f },
            { "f1_cutoff", 500.0f }, { "f1_reso", 0.3f }, { "f1_drive", 4.0f },
            { "amp_decay", 0.4f }, { "amp_sustain", 0.7f }, { "amp_release", 0.2f },
            { "res_on", 1 }, { "res_amount", 0.7f }, { "res_decay", 0.75f },
            { "res_offset", -7.0f }, { "res_keytrack", 0.4f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
        } },

        { "FM Growl", {
            { "osc1_table", 3 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.8f },
            { "osc2_on", 1 }, { "osc2_table", 0 }, { "osc2_frame", 0.2f }, { "osc2_level", 0.8f },
            { "osc2_semi", -12 },
            { "fm_amount", 0.55f }, { "fm_feedback", 0.25f }, { "f1_fm", 0.25f },
            { "f1_cutoff", 1200.0f }, { "f1_reso", 0.45f }, { "f1_drive", 4.0f }, { "f1_env", 1.5f },
            { "fe_decay", 0.4f }, { "fe_sustain", 0.3f },
            { "amp_sustain", 0.8f }, { "amp_release", 0.25f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
        } },

        { "Resonant Scream", {
            { "osc1_table", 4 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 15.0f }, { "osc1_spread", 0.6f },
            { "fm_amount", 0.35f }, { "osc2_on", 1 }, { "osc2_table", 9 }, { "osc2_level", 0.4f }, { "osc2_semi", 12 },
            { "f1_type", 1 }, { "f1_cutoff", 1800.0f }, { "f1_reso", 0.55f }, { "f1_drive", 3.0f }, { "f1_env", 1.0f },
            { "fe_decay", 1.0f }, { "fe_sustain", 0.3f },
            { "res_on", 1 }, { "res_amount", 0.5f }, { "res_decay", 0.8f }, { "res_offset", 3.0f }, { "res_keytrack", 0.7f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.25f },
        } },

        { "Tape Lead", {
            { "osc1_table", 10 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.85f },
            { "osc1_unison", 4 }, { "osc1_detune", 20.0f }, { "osc1_spread", 0.7f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.2f }, { "f1_drive", 2.5f },
            { "amp_attack", 0.01f }, { "amp_release", 0.6f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.5f },
            { "fx_delay_damping", 0.4f }, { "fx_delay_pitch", 5.0f }, { "fx_delay_wow", 0.25f }, { "fx_delay_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "Glitch Lead", {
            { "osc1_table", 1 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 10.0f },
            { "f1_cutoff", 5000.0f }, { "f1_reso", 0.3f }, { "f1_drive", 3.5f },
            { "amp_decay", 0.35f }, { "amp_sustain", 0.5f }, { "amp_release", 0.2f },
            { "fx_crush_on", 1 }, { "fx_crush_bits", 8.0f }, { "fx_crush_down", 3.0f }, { "fx_crush_mix", 0.5f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 7 }, { "fx_delay_feedback", 0.4f }, { "fx_delay_mix", 0.25f },
            { "fx_taps_on", 1 }, { "fx_taps_pattern", 4 }, { "fx_taps_mix", 0.5f },
        } },

        { "Smear Pluck", {
            { "osc1_table", 5 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.85f },
            { "f1_cutoff", 5000.0f }, { "f1_reso", 0.3f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.18f }, { "fe_sustain", 0.0f }, { "fe_release", 0.2f },
            { "amp_decay", 0.4f }, { "amp_sustain", 0.0f }, { "amp_release", 0.35f },
            { "mod1_src", 4 }, { "mod1_dst", 2 }, { "mod1_amt", 0.5f },
            { "fx_smear_on", 1 }, { "fx_smear_size", 180.0f }, { "fx_smear_density", 18.0f }, { "fx_smear_mix", 0.55f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.35f },
        } },

        { "Tap Pluck", {
            { "osc1_table", 6 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.85f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.25f },
            { "amp_decay", 0.3f }, { "amp_sustain", 0.1f }, { "amp_release", 0.25f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_feedback", 0.45f }, { "fx_delay_mix", 0.3f },
            { "fx_taps_on", 1 }, { "fx_taps_pattern", 1 }, { "fx_taps_mix", 0.6f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "Freeze Pad", {
            { "osc1_table", 10 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.75f },
            { "osc1_unison", 4 }, { "osc1_detune", 15.0f }, { "osc1_spread", 0.8f },
            { "sub_level", 0.25f },
            { "f1_cutoff", 3500.0f }, { "f1_reso", 0.2f },
            { "amp_attack", 1.2f }, { "amp_sustain", 0.85f }, { "amp_release", 4.0f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.15f }, { "fx_phaser_depth", 0.6f }, { "fx_phaser_mix", 0.45f },
            { "fx_freeze_on", 1 }, { "fx_freeze_mix", 0.45f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Chord Pad", {
            { "osc1_table", 10 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.7f },
            { "osc1_unison", 4 }, { "osc1_detune", 10.0f }, { "osc1_spread", 0.8f },
            { "osc1_chord", 4 },
            { "sub_level", 0.25f },
            { "f1_cutoff", 4000.0f }, { "f1_reso", 0.15f },
            { "amp_attack", 0.9f }, { "amp_sustain", 0.85f }, { "amp_release", 3.0f },
            { "voice_spread", 0.5f }, { "unison_random", 0.8f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.35f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.4f },
        } },

        { "MSEG Sweep", {
            { "osc1_table", 7 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.8f },
            { "osc1_unison", 2 }, { "osc1_detune", 14.0f },
            { "f1_cutoff", 900.0f }, { "f1_reso", 0.5f }, { "f1_drive", 3.0f },
            { "amp_attack", 0.05f }, { "amp_sustain", 0.9f }, { "amp_release", 1.5f },
            { "mseg_rate", 0.25f }, { "mseg_loop", 1 },
            { "mseg_level2", 0.8f }, { "mseg_level3", -0.2f }, { "mseg_level4", 0.5f },
            { "mseg_time2", 0.4f }, { "mseg_time3", 0.15f },
            { "mod1_src", 17 }, { "mod1_dst", 9 }, { "mod1_amt", 0.6f },
            { "mod2_src", 17 }, { "mod2_dst", 2 }, { "mod2_amt", 0.3f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.3f },
        } },

        { "Spectral Drone", {
            { "osc1_table", 4 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.7f },
            { "osc1_unison", 2 }, { "osc1_detune", 18.0f },
            { "sub_level", 0.4f }, { "noise_level", 0.1f },
            { "f1_cutoff", 2500.0f }, { "f1_reso", 0.2f }, { "f1_drive", 2.0f },
            { "amp_attack", 0.5f }, { "amp_sustain", 1.0f }, { "amp_release", 6.0f },
            { "fx_freeze_on", 1 }, { "fx_freeze_mix", 0.55f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.1f }, { "fx_phaser_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Granular Cloud", {
            { "osc1_table", 5 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.75f },
            { "osc1_unison", 3 }, { "osc1_detune", 22.0f }, { "osc1_spread", 0.8f },
            { "f1_cutoff", 4500.0f }, { "f1_reso", 0.2f },
            { "amp_attack", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 5.0f },
            { "voice_spread", 0.4f }, { "drift", 0.4f },
            { "fx_smear_on", 1 }, { "fx_smear_size", 300.0f }, { "fx_smear_density", 30.0f }, { "fx_smear_mix", 0.7f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 2 }, { "fx_delay_feedback", 0.5f }, { "fx_delay_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Clock Weirdo", {
            { "osc1_table", 2 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 16.0f },
            { "f1_cutoff", 1500.0f }, { "f1_reso", 0.6f }, { "f1_drive", 3.0f },
            { "amp_attack", 0.005f }, { "amp_sustain", 0.8f }, { "amp_release", 0.3f },
            { "clock_div", 4 },
            { "mod1_src", 16 }, { "mod1_dst", 9 }, { "mod1_amt", 0.5f },
            { "mod2_src", 16 }, { "mod2_dst", 2 }, { "mod2_amt", 0.4f },
            { "mod3_src", 16 }, { "mod3_dst", 14 }, { "mod3_amt", 0.6f },
            { "fx_stutter_on", 0 }, { "fx_stutter_div", 8 }, { "fx_stutter_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.25f },
        } },

        { "KS Marimba", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.8f }, { "osc1_string_damp", 0.25f },
            { "osc1_level", 0.9f },
            { "f1_cutoff", 8000.0f },
            { "amp_decay", 0.5f }, { "amp_sustain", 0.0f }, { "amp_release", 0.5f },
            { "res_on", 1 }, { "res_amount", 0.6f }, { "res_decay", 0.5f }, { "res_keytrack", 1.0f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Step Sequence", {
            { "osc1_table", 7 }, { "osc1_frame", 0.25f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 12.0f },
            { "f1_cutoff", 1000.0f }, { "f1_reso", 0.5f }, { "f1_drive", 3.5f },
            { "amp_sustain", 0.8f }, { "amp_release", 0.2f },
            { "lfo1_shape", 7 }, { "lfo1_sync", 1 }, { "lfo1_div", 4 },
            { "lfo1_step1", 1.0f }, { "lfo1_step3", 0.6f }, { "lfo1_step5", -0.4f },
            { "lfo1_step7", 0.8f }, { "lfo1_step9", 0.2f }, { "lfo1_step11", -0.8f },
            { "lfo1_step13", 0.5f }, { "lfo1_step15", -0.2f },
            { "mod1_src", 1 }, { "mod1_dst", 9 }, { "mod1_amt", 0.55f },
            { "mod2_src", 1 }, { "mod2_dst", 2 }, { "mod2_amt", 0.3f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.25f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
        } },

        { "Chord Stab", {
            { "osc1_table", 0 }, { "osc1_frame", 0.2f }, { "osc1_level", 0.85f },
            { "osc1_unison", 6 }, { "osc1_detune", 8.0f }, { "osc1_spread", 0.6f },
            { "osc1_chord", 4 },
            { "f1_cutoff", 5000.0f }, { "f1_reso", 0.2f }, { "f1_drive", 2.5f }, { "f1_env", 1.5f },
            { "fe_decay", 0.2f }, { "fe_sustain", 0.0f },
            { "amp_decay", 0.3f }, { "amp_sustain", 0.0f }, { "amp_release", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.2f },
        } },

        { "MPE Lead", {
            { "osc1_table", 10 }, { "osc1_frame", 0.45f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 12.0f }, { "osc1_spread", 0.6f },
            { "f1_cutoff", 7000.0f }, { "f1_reso", 0.25f }, { "f1_drive", 2.0f },
            { "amp_attack", 0.01f }, { "amp_sustain", 0.85f }, { "amp_release", 0.6f },
            { "mpe_mode", 1 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.25f },
        } },

        { "Tape Drone", {
            { "osc1_table", 7 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.7f },
            { "osc1_unison", 5 }, { "osc1_detune", 35.0f }, { "osc1_spread", 0.9f },
            { "sub_level", 0.5f },
            { "f1_cutoff", 1800.0f }, { "f1_reso", 0.35f }, { "f1_drive", 3.0f },
            { "amp_attack", 0.4f }, { "amp_sustain", 1.0f }, { "amp_release", 5.0f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 2 }, { "fx_delay_feedback", 0.8f },
            { "fx_delay_damping", 0.6f }, { "fx_delay_pingpong", 1 }, { "fx_delay_pitch", -12.0f },
            { "fx_delay_wow", 0.4f }, { "fx_delay_mix", 0.45f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Neon Bass", {
            { "osc1_table", 7 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.85f },
            { "osc1_unison", 3 }, { "osc1_detune", 14.0f }, { "osc1_spread", 0.5f },
            { "sub_level", 0.6f },
            { "f1_slope", 1 }, { "f1_cutoff", 800.0f }, { "f1_reso", 0.5f }, { "f1_drive", 5.0f },
            { "f1_env", 1.8f }, { "f1_fm", 0.2f },
            { "fe_decay", 0.3f }, { "fe_sustain", 0.2f },
            { "amp_sustain", 0.85f }, { "amp_release", 0.2f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 5.0f }, { "fx_drive_mix", 0.6f },
        } },

        { "Glass Keys", {
            { "osc1_table", 3 }, { "osc1_frame", 0.2f }, { "osc1_level", 0.75f },
            { "osc2_on", 1 }, { "osc2_table", 8 }, { "osc2_level", 0.6f }, { "osc2_semi", 12 },
            { "fm_amount", 0.3f },
            { "f1_cutoff", 9000.0f }, { "f1_reso", 0.15f },
            { "amp_decay", 0.8f }, { "amp_sustain", 0.1f }, { "amp_release", 0.6f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.7f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Vocal Chop", {
            { "osc1_table", 4 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 12.0f },
            { "f1_type", 1 }, { "f1_cutoff", 2200.0f }, { "f1_reso", 0.35f }, { "f1_drive", 3.0f },
            { "fe_decay", 0.2f }, { "fe_sustain", 0.0f },
            { "amp_decay", 0.25f }, { "amp_sustain", 0.0f }, { "amp_release", 0.3f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_mix", 0.25f },
            { "fx_taps_on", 1 }, { "fx_taps_pattern", 3 }, { "fx_taps_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.25f },
        } },

        { "Tape Choir", {
            { "osc1_table", 10 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.75f },
            { "osc1_unison", 4 }, { "osc1_detune", 22.0f }, { "osc1_spread", 0.9f },
            { "f1_cutoff", 5000.0f }, { "f1_reso", 0.15f },
            { "amp_attack", 0.6f }, { "amp_sustain", 0.9f }, { "amp_release", 2.5f },
            { "unison_random", 0.8f }, { "voice_spread", 0.4f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.45f },
            { "fx_delay_pitch", 7.0f }, { "fx_delay_wow", 0.3f }, { "fx_delay_mix", 0.35f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Metal Drip", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.95f }, { "osc1_string_damp", 0.1f },
            { "osc1_level", 0.8f },
            { "res_on", 1 }, { "res_amount", 0.8f }, { "res_decay", 0.9f }, { "res_offset", 7.0f }, { "res_keytrack", 0.0f },
            { "f1_cutoff", 6000.0f },
            { "amp_decay", 1.5f }, { "amp_sustain", 0.0f }, { "amp_release", 2.0f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 880.0f }, { "fx_comb_feedback", 0.6f }, { "fx_comb_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.8f }, { "fx_reverb_mix", 0.35f },
        } },

        { "Hyper Pluck", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.7f }, { "osc1_string_damp", 0.2f },
            { "osc1_level", 0.9f },
            { "f1_cutoff", 8000.0f }, { "f1_reso", 0.2f },
            { "amp_decay", 0.45f }, { "amp_sustain", 0.0f }, { "amp_release", 0.4f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 440.0f }, { "fx_comb_feedback", 0.5f }, { "fx_comb_mix", 0.35f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "S&H Techno", {
            { "osc1_table", 7 }, { "osc1_frame", 0.25f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 12.0f },
            { "f1_cutoff", 1200.0f }, { "f1_reso", 0.55f }, { "f1_drive", 4.0f },
            { "amp_sustain", 0.8f }, { "amp_release", 0.25f },
            { "clock_div", 4 },
            { "mod1_src", 16 }, { "mod1_dst", 9 }, { "mod1_amt", 0.6f },
            { "mod2_src", 16 }, { "mod2_dst", 2 }, { "mod2_amt", 0.35f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_mix", 0.25f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 5.0f }, { "fx_drive_mix", 0.55f },
        } },

        { "Morph Pad", {
            { "osc1_table", 10 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.7f },
            { "osc1_unison", 4 }, { "osc1_detune", 10.0f }, { "osc1_spread", 0.8f }, { "osc1_chord", 4 },
            { "f1_cutoff", 3000.0f }, { "f1_reso", 0.25f },
            { "amp_attack", 1.0f }, { "amp_sustain", 0.85f }, { "amp_release", 3.5f },
            { "mseg_rate", 0.2f },
            { "mod1_src", 17 }, { "mod1_dst", 9 }, { "mod1_amt", 0.45f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.18f }, { "fx_phaser_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Rust Bass", {
            { "osc1_table", 2 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 18.0f },
            { "sub_level", 0.5f },
            { "f1_slope", 1 }, { "f1_cutoff", 500.0f }, { "f1_reso", 0.6f }, { "f1_drive", 8.0f },
            { "f1_env", 2.0f }, { "fe_decay", 0.35f }, { "fe_sustain", 0.2f },
            { "amp_sustain", 0.8f }, { "amp_release", 0.25f },
            { "fx_crush_on", 1 }, { "fx_crush_bits", 6.0f }, { "fx_crush_down", 2.0f }, { "fx_crush_mix", 0.4f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 6.0f }, { "fx_drive_mix", 0.6f },
        } },

        { "Glass Bell", {
            { "osc1_mode", 1 }, { "osc1_excite", 0 }, { "osc1_string_decay", 0.98f }, { "osc1_string_damp", 0.08f },
            { "osc1_level", 0.85f },
            { "f1_cutoff", 12000.0f },
            { "amp_decay", 1.2f }, { "amp_sustain", 0.0f }, { "amp_release", 2.5f },
            { "res_on", 1 }, { "res_amount", 0.5f }, { "res_decay", 0.6f }, { "res_keytrack", 1.0f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 1320.0f }, { "fx_comb_feedback", 0.5f }, { "fx_comb_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.75f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Warp Stab", {
            { "osc1_table", 1 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.85f },
            { "osc1_unison", 2 }, { "osc1_detune", 10.0f },
            { "f1_cutoff", 7000.0f }, { "f1_reso", 0.25f }, { "f1_drive", 3.5f },
            { "amp_decay", 0.4f }, { "amp_sustain", 0.2f }, { "amp_release", 0.3f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_feedback", 0.5f },
            { "fx_delay_pitch", -5.0f }, { "fx_delay_wow", 0.2f }, { "fx_delay_mix", 0.3f },
            { "fx_crush_on", 1 }, { "fx_crush_bits", 10.0f }, { "fx_crush_mix", 0.3f },
        } },

        { "Sub Ritual", {
            { "osc1_table", 8 }, { "osc1_frame", 0.0f }, { "osc1_level", 0.5f },
            { "osc1_unison", 3 }, { "osc1_detune", 8.0f },
            { "sub_level", 1.0f }, { "sub_shape", 1 }, { "sub_octave", 1 },
            { "f1_cutoff", 600.0f }, { "f1_reso", 0.2f }, { "f1_drive", 3.0f },
            { "amp_attack", 0.8f }, { "amp_sustain", 1.0f }, { "amp_release", 5.0f },
            { "drift", 0.4f },
            { "fx_phaser_on", 1 }, { "fx_phaser_rate", 0.1f }, { "fx_phaser_mix", 0.35f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Rig Lead", {
            { "osc1_table", 10 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.85f },
            { "osc1_unison", 3 }, { "osc1_detune", 15.0f }, { "osc1_spread", 0.6f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.25f },
            { "amp_attack", 0.01f }, { "amp_sustain", 0.85f }, { "amp_release", 0.5f },
            { "fx_slot1", 1 }, { "fx_slot2", 4 }, { "fx_slot3", 2 }, { "fx_slot4", 0 }, { "fx_slot5", 0 },
            { "fx_slot6", 0 }, { "fx_slot7", 7 }, { "fx_slot8", 8 }, { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_amp_mode", 0 }, { "fx_amp_drive", 6.0f }, { "fx_amp_bass", 1.1f }, { "fx_amp_mid", 0.9f },
            { "fx_amp_treble", 1.2f }, { "fx_amp_level", 0.8f },
            { "fx_comp_threshold", -20.0f }, { "fx_comp_ratio", 4.0f }, { "fx_comp_attack", 12.0f },
            { "fx_comp_release", 160.0f }, { "fx_comp_makeup", 6.0f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 3.0f }, { "fx_drive_mix", 0.4f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.25f },
            { "fx_haas_delay", 16.0f }, { "fx_haas_mix", 0.5f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.45f },
            { "fx_delay_damping", 0.4f }, { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 0 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.22f },
        } },

        { "Shimmer Pad", {
            { "osc1_table", 10 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.7f },
            { "osc1_unison", 4 }, { "osc1_detune", 18.0f }, { "osc1_spread", 0.9f },
            { "f1_cutoff", 3800.0f }, { "f1_reso", 0.15f },
            { "amp_attack", 0.9f }, { "amp_sustain", 0.85f }, { "amp_release", 4.0f },
            { "fx_slot1", 1 }, { "fx_slot2", 4 }, { "fx_slot3", 0 }, { "fx_slot4", 0 }, { "fx_slot5", 0 },
            { "fx_slot6", 0 }, { "fx_slot7", 7 }, { "fx_slot8", 0 }, { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_amp_mode", 2 }, { "fx_amp_drive", 1.5f }, { "fx_amp_level", 1.0f },
            { "fx_comp_threshold", -16.0f }, { "fx_comp_ratio", 3.0f }, { "fx_comp_attack", 20.0f },
            { "fx_comp_release", 250.0f }, { "fx_comp_makeup", 4.0f }, { "fx_comp_mix", 0.8f },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.3f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 2 }, { "fx_delay_feedback", 0.4f },
            { "fx_delay_damping", 0.5f }, { "fx_delay_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.45f },
        } },

        { "Metal Keys", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_sample_loop", 0 },
            { "osc1_sample_factory", 1 },
            { "osc1_level", 0.9f }, { "osc1_unison", 2 }, { "osc1_detune", 6.0f }, { "osc1_spread", 0.5f },
            { "f1_cutoff", 7000.0f }, { "f1_reso", 0.2f }, { "f1_env", 1.4f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.35f }, { "fe_sustain", 0.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.8f }, { "amp_sustain", 0.0f }, { "amp_release", 0.6f },
            { "fx_slot7", 9 }, { "fx_slot8", 0 }, { "fx_slot9", 0 }, { "fx_slot10", 13 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_feedback", 0.35f },
            { "fx_delay_mix", 0.28f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.55f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Vocal Pad", {
            { "osc1_mode", 2 }, { "osc1_sample_loop", 1 }, { "osc1_level", 0.75f },
            { "osc1_sample_factory", 2 },
            { "osc1_unison", 3 }, { "osc1_detune", 9.0f }, { "osc1_spread", 0.8f },
            { "osc1_sample_fade_in", 0.25f }, { "osc1_sample_fade_out", 0.25f },
            { "f1_cutoff", 5200.0f }, { "f1_reso", 0.12f },
            { "amp_attack", 1.2f }, { "amp_sustain", 0.9f }, { "amp_release", 5.0f },
            { "lfo1_rate", 0.35f },
            { "mod1_src", 1 }, { "mod1_dst", 2 }, { "mod1_amt", 0.08f },
            { "fx_slot7", 15 }, { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_dim_rate", 0.25f }, { "fx_dim_depth", 0.6f }, { "fx_dim_mix", 0.5f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 2 }, { "fx_delay_feedback", 0.3f },
            { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Sub Tone 808", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.95f },
            { "osc1_sample_factory", 3 },
            { "sub_on", 0 },
            { "f1_cutoff", 900.0f }, { "f1_reso", 0.1f },
            { "amp_attack", 0.002f }, { "amp_decay", 1.6f }, { "amp_sustain", 0.25f }, { "amp_release", 0.4f },
            { "glide", 0.06f },
            { "master_clip", 1 },
            { "fx_slot1", 20 }, { "fx_slot10", 21 },
            { "fx_ott_amount", 0.5f }, { "fx_ott_mix", 0.6f },
            { "fx_limit_ceiling", -0.6f },
        } },

        { "Feedback Drone", {
            { "osc1_table", 8 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.55f },
            { "osc1_unison", 3 }, { "osc1_detune", 7.0f }, { "osc1_spread", 0.7f },
            { "res_on", 1 }, { "res_amount", 0.5f }, { "res_decay", 0.8f }, { "res_keytrack", 0.6f },
            { "f1_cutoff", 4200.0f },
            { "amp_attack", 1.5f }, { "amp_sustain", 0.9f }, { "amp_release", 6.0f },
            { "fx_slot1", 28 }, { "fx_slot2", 9 }, { "fx_slot10", 13 },
            { "fx_feedback_amount", 0.78f }, { "fx_feedback_delay", 32.0f }, { "fx_feedback_tone", 0.45f },
            { "fx_feedback_mix", 0.5f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 5 }, { "fx_delay_feedback", 0.35f },
            { "fx_delay_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.45f },
        } },

        { "Frequency Shift", {
            { "osc1_table", 1 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.8f },
            { "osc1_unison", 2 }, { "osc1_detune", 12.0f },
            { "f1_cutoff", 6500.0f }, { "f1_reso", 0.3f },
            { "amp_attack", 0.01f }, { "amp_sustain", 0.8f }, { "amp_release", 0.4f },
            { "fx_slot1", 24 }, { "fx_slot10", 13 },
            { "fx_shifter_shift", 9.0f }, { "fx_shifter_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.25f },
        } },

        { "Vowel Lead", {
            { "osc1_table", 4 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.85f },
            { "osc1_unison", 3 }, { "osc1_detune", 14.0f }, { "osc1_spread", 0.6f },
            { "f1_cutoff", 5500.0f }, { "f1_reso", 0.25f }, { "f1_drive", 2.5f },
            { "amp_attack", 0.005f }, { "amp_sustain", 0.85f }, { "amp_release", 0.35f },
            { "mod1_src", 1 }, { "mod1_dst", 1 }, { "mod1_amt", 0.02f },
            { "fx_slot1", 27 }, { "fx_slot2", 2 }, { "fx_slot10", 13 },
            { "fx_vowel_morph", 0.35f }, { "fx_vowel_mix", 0.75f },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.5f }, { "fx_reverb_mix", 0.2f },
        } },

        { "Reverse Stutter", {
            { "osc1_table", 7 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.8f },
            { "osc1_unison", 2 }, { "osc1_detune", 10.0f },
            { "f1_cutoff", 6000.0f }, { "f1_reso", 0.2f },
            { "amp_attack", 0.001f }, { "amp_sustain", 0.9f }, { "amp_release", 0.3f },
            { "fx_slot1", 10 }, { "fx_slot2", 9 }, { "fx_slot10", 13 },
            { "fx_stutter_on", 1 }, { "fx_stutter_div", 3 }, { "fx_stutter_reverse", 1 },
            { "fx_stutter_pitch", -12.0f }, { "fx_stutter_mix", 0.85f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_feedback", 0.3f },
            { "fx_delay_mix", 0.2f },
            { "fx_reverb_on", 1 }, { "fx_reverb_mix", 0.2f },
        } },

        { "Granular Pad", {
            { "osc1_table", 5 }, { "osc1_frame", 0.45f }, { "osc1_level", 0.7f },
            { "osc1_unison", 4 }, { "osc1_detune", 16.0f }, { "osc1_spread", 0.9f },
            { "f1_cutoff", 4800.0f }, { "f1_reso", 0.15f },
            { "amp_attack", 1.8f }, { "amp_sustain", 0.85f }, { "amp_release", 5.0f },
            { "fx_slot1", 11 }, { "fx_slot10", 13 },
            { "fx_smear_on", 1 }, { "fx_smear_size", 140.0f }, { "fx_smear_density", 0.4f }, { "fx_smear_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.45f },
        } },

        { "Odd Octaver", {
            { "osc1_table", 2 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.8f },
            { "f1_cutoff", 5000.0f }, { "f1_drive", 3.0f },
            { "amp_attack", 0.002f }, { "amp_sustain", 0.8f }, { "amp_release", 0.4f },
            { "fx_slot1", 26 }, { "fx_slot2", 22 }, { "fx_slot10", 21 },
            { "fx_octaver_mix", 0.55f },
            { "fx_width", 0.7f }, { "fx_width_mix", 0.6f },
            { "fx_limit_ceiling", -0.8f },
        } },

        { "Wide Chord Pad", {
            { "osc1_table", 0 }, { "osc1_frame", 0.3f }, { "osc1_level", 0.6f },
            { "osc1_chord", 4 }, { "osc1_unison", 3 }, { "osc1_detune", 10.0f }, { "osc1_spread", 0.8f },
            { "sub_level", 0.3f }, { "sub_chord", 1 },
            { "f1_cutoff", 4200.0f },
            { "amp_attack", 0.8f }, { "amp_sustain", 0.85f }, { "amp_release", 4.0f },
            { "voice_spread", 0.5f },
            { "fx_slot1", 22 }, { "fx_slot2", 15 }, { "fx_slot3", 8 }, { "fx_slot10", 13 },
            { "fx_width", 0.8f }, { "fx_width_mix", 0.7f },
            { "fx_dim_rate", 0.2f }, { "fx_dim_depth", 0.5f }, { "fx_dim_mix", 0.45f },
            { "fx_haas_delay", 18.0f }, { "fx_haas_mix", 0.5f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Minor Chord Stab", {
            { "osc1_table", 9 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.8f },
            { "osc1_chord", 5 }, { "sub_level", 0.4f }, { "sub_chord", 1 },
            { "f1_cutoff", 4500.0f }, { "f1_reso", 0.3f }, { "f1_env", 2.0f },
            { "fe_decay", 0.4f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.5f }, { "amp_sustain", 0.0f }, { "amp_release", 0.4f },
            { "fx_slot1", 4 }, { "fx_slot10", 13 },
            { "fx_comp_threshold", -18.0f }, { "fx_comp_ratio", 4.0f }, { "fx_comp_makeup", 5.0f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Ring Bell", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.8f },
            { "osc1_sample_factory", 1 },
            { "f1_cutoff", 9000.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 1.2f }, { "amp_sustain", 0.0f }, { "amp_release", 0.8f },
            { "fx_slot1", 25 }, { "fx_slot2", 9 }, { "fx_slot10", 13 },
            { "fx_ring_freq", 380.0f }, { "fx_ring_mix", 0.35f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.4f },
            { "fx_delay_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.7f }, { "fx_reverb_mix", 0.35f },
        } },

        { "Tape Wow Dub", {
            { "osc1_table", 10 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.65f },
            { "osc1_unison", 2 }, { "osc1_detune", 8.0f },
            { "sub_level", 0.45f },
            { "f1_slope", 1 }, { "f1_cutoff", 1600.0f }, { "f1_reso", 0.35f }, { "f1_env", 1.2f },
            { "fe_decay", 0.8f }, { "fe_sustain", 0.2f },
            { "amp_sustain", 0.7f }, { "amp_release", 1.2f },
            { "fx_slot1", 9 }, { "fx_slot10", 13 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.72f },
            { "fx_delay_pitch", -2.0f }, { "fx_delay_wow", 0.7f }, { "fx_delay_mix", 0.4f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.7f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Clocked Bass", {
            { "osc1_table", 6 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.85f },
            { "sub_level", 0.5f }, { "sub_shape", 2 },
            { "f1_cutoff", 1200.0f }, { "f1_reso", 0.45f }, { "f1_env", 1.8f },
            { "fe_attack", 0.001f }, { "fe_decay", 0.3f }, { "fe_sustain", 0.0f },
            { "amp_sustain", 0.7f }, { "amp_release", 0.25f },
            { "clock_div", 3 },
            { "mod1_src", 9 }, { "mod1_dst", 2 }, { "mod1_amt", 0.3f },
            { "fx_slot1", 2 }, { "fx_slot10", 21 },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.45f },
            { "fx_limit_ceiling", -0.8f },
        } },

        { "MPE Glass", {
            { "osc1_mode", 2 }, { "osc1_sample_loop", 1 }, { "osc1_level", 0.7f },
            { "osc1_sample_factory", 2 },
            { "osc1_unison", 2 }, { "osc1_detune", 5.0f }, { "osc1_spread", 0.6f },
            { "osc1_sample_fade_in", 0.2f }, { "osc1_sample_fade_out", 0.3f },
            { "mpe_mode", 1 },
            { "f1_cutoff", 6500.0f },
            { "amp_attack", 0.6f }, { "amp_sustain", 0.9f }, { "amp_release", 3.5f },
            { "fx_slot1", 7 }, { "fx_slot10", 13 },
            { "fx_chorus_on", 1 }, { "fx_chorus_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Crushed Trap", {
            { "osc1_table", 7 }, { "osc1_frame", 0.7f }, { "osc1_level", 0.8f },
            { "sub_level", 0.6f },
            { "f1_cutoff", 3200.0f }, { "f1_drive", 2.5f },
            { "amp_attack", 0.001f }, { "amp_sustain", 0.75f }, { "amp_release", 0.3f },
            { "fx_slot1", 3 }, { "fx_slot2", 20 }, { "fx_slot10", 21 },
            { "fx_crush_on", 1 }, { "fx_crush_bits", 9.0f }, { "fx_crush_down", 3.0f }, { "fx_crush_mix", 0.4f },
            { "fx_ott_amount", 0.45f }, { "fx_ott_mix", 0.55f },
            { "fx_limit_ceiling", -0.7f },
        } },

        { "Formant Scream II", {
            { "osc1_table", 4 }, { "osc1_frame", 0.7f }, { "osc1_level", 0.8f },
            { "osc1_unison", 3 }, { "osc1_detune", 20.0f }, { "osc1_spread", 0.7f },
            { "f1_fm", 0.35f }, { "f1_cutoff", 3000.0f }, { "f1_reso", 0.5f }, { "f1_drive", 4.0f },
            { "amp_sustain", 0.85f }, { "amp_release", 0.4f },
            { "lfo1_rate", 3.5f },
            { "mod1_src", 1 }, { "mod1_dst", 1 }, { "mod1_amt", 0.01f },
            { "fx_slot1", 1 }, { "fx_slot2", 6 }, { "fx_slot10", 13 },
            { "fx_amp_mode", 1 }, { "fx_amp_drive", 5.0f }, { "fx_amp_level", 0.85f },
            { "fx_phaser_on", 1 }, { "fx_phaser_mix", 0.35f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.5f }, { "fx_reverb_mix", 0.25f },
        } },

        { "Self Osc Drone", {
            { "osc1_table", 8 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.5f },
            { "res_on", 1 }, { "res_amount", 0.6f }, { "res_decay", 0.9f }, { "res_offset", 7.0f },
            { "f1_cutoff", 5200.0f },
            { "amp_attack", 2.0f }, { "amp_sustain", 0.9f }, { "amp_release", 8.0f },
            { "fx_slot1", 28 }, { "fx_slot2", 5 }, { "fx_slot10", 13 },
            { "fx_feedback_amount", 0.85f }, { "fx_feedback_delay", 74.0f }, { "fx_feedback_tone", 0.3f },
            { "fx_feedback_mix", 0.55f },
            { "fx_comb_on", 1 }, { "fx_comb_freq", 220.0f }, { "fx_comb_feedback", 0.6f }, { "fx_comb_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.95f }, { "fx_reverb_mix", 0.5f },
        } },

        { "Tremolo Keys", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.85f },
            { "osc1_sample_factory", 1 },
            { "f1_cutoff", 8000.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_sustain", 0.0f }, { "amp_release", 0.5f },
            { "fx_slot1", 23 }, { "fx_slot2", 8 }, { "fx_slot10", 13 },
            { "fx_trem_rate", 5.5f }, { "fx_trem_depth", 0.8f }, { "fx_trem_shape", 0 },
            { "fx_haas_delay", 14.0f }, { "fx_haas_mix", 0.45f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.55f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Resonator Bell", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.75f },
            { "osc1_sample_factory", 1 },
            { "res_on", 1 }, { "res_amount", 0.65f }, { "res_decay", 0.75f }, { "res_keytrack", 1.0f },
            { "f1_cutoff", 9000.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 1.4f }, { "amp_sustain", 0.0f }, { "amp_release", 1.0f },
            { "fx_slot10", 13 },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.75f }, { "fx_reverb_mix", 0.4f },
        } },

        { "LFO 4 Sweep", {
            { "osc1_table", 5 }, { "osc1_frame", 0.35f }, { "osc1_level", 0.75f },
            { "osc1_unison", 3 }, { "osc1_detune", 14.0f }, { "osc1_spread", 0.7f },
            { "f1_slope", 1 }, { "f1_cutoff", 900.0f }, { "f1_reso", 0.55f },
            { "amp_attack", 0.6f }, { "amp_sustain", 0.85f }, { "amp_release", 3.0f },
            { "lfo4_rate", 0.25f },
            { "mod1_src", 21 }, { "mod1_dst", 9 }, { "mod1_amt", 0.45f },
            { "fx_slot10", 13 },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.85f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Quad Mod Pluck", {
            { "osc1_table", 2 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.85f },
            { "f1_cutoff", 4500.0f }, { "f1_reso", 0.3f }, { "f1_env", 1.6f },
            { "fe_decay", 0.35f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.6f }, { "amp_sustain", 0.0f }, { "amp_release", 0.4f },
            { "lfo1_rate", 6.0f }, { "lfo2_rate", 3.0f }, { "lfo3_rate", 0.8f }, { "lfo4_rate", 0.3f },
            { "mod1_src", 1 }, { "mod1_dst", 9 }, { "mod1_amt", 0.25f },
            { "mod2_src", 2 }, { "mod2_dst", 2 }, { "mod2_amt", 0.3f },
            { "mod3_src", 20 }, { "mod3_dst", 14 }, { "mod3_amt", 0.4f },
            { "mod4_src", 21 }, { "mod4_dst", 22 }, { "mod4_amt", 0.4f },
            { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.4f },
            { "fx_delay_mix", 0.3f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.3f },
        } },

        { "LFO 3 Wobble", {
            { "osc1_table", 10 }, { "osc1_frame", 0.6f }, { "osc1_level", 0.85f },
            { "sub_level", 0.5f },
            { "f1_slope", 1 }, { "f1_cutoff", 700.0f }, { "f1_reso", 0.6f }, { "f1_drive", 3.0f },
            { "amp_sustain", 0.8f }, { "amp_release", 0.3f },
            { "lfo3_sync", 1 }, { "lfo3_div", 2 },
            { "mod1_src", 20 }, { "mod1_dst", 9 }, { "mod1_amt", 0.5f },
            { "fx_slot1", 2 }, { "fx_slot10", 21 },
            { "fx_drive_on", 1 }, { "fx_drive_amount", 4.0f }, { "fx_drive_mix", 0.5f },
            { "fx_limit_ceiling", -0.7f },
        } },

        { "Arp Glass", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.8f },
            { "osc1_sample_factory", 1 },
            { "f1_cutoff", 8000.0f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.7f }, { "amp_sustain", 0.0f }, { "amp_release", 0.4f },
            { "arp_on", 1 }, { "arp_mode", 2 }, { "arp_div", 3 }, { "arp_octaves", 2 }, { "arp_gate", 0.35f },
            { "fx_slot10", 13 },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.7f }, { "fx_reverb_mix", 0.4f },
        } },

        { "Resonator Pluck", {
            { "osc1_table", 1 }, { "osc1_frame", 0.5f }, { "osc1_level", 0.8f },
            { "res_on", 1 }, { "res_amount", 0.7f }, { "res_decay", 0.6f }, { "res_keytrack", 1.0f },
            { "f1_cutoff", 6000.0f }, { "f1_env", 1.2f },
            { "fe_decay", 0.3f },
            { "amp_attack", 0.001f }, { "amp_decay", 0.5f }, { "amp_sustain", 0.0f }, { "amp_release", 0.35f },
            { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 4 }, { "fx_delay_feedback", 0.35f },
            { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Sub 808 Arp", {
            { "osc1_mode", 2 }, { "osc1_sample_tuned", 1 }, { "osc1_level", 0.95f },
            { "osc1_sample_factory", 3 },
            { "sub_on", 0 },
            { "f1_cutoff", 1200.0f },
            { "amp_attack", 0.002f }, { "amp_decay", 1.2f }, { "amp_sustain", 0.2f }, { "amp_release", 0.3f },
            { "arp_on", 1 }, { "arp_mode", 0 }, { "arp_div", 4 }, { "arp_octaves", 1 }, { "arp_gate", 0.6f },
            { "fx_slot1", 20 }, { "fx_slot10", 21 },
            { "fx_ott_amount", 0.5f }, { "fx_ott_mix", 0.55f },
            { "fx_limit_ceiling", -0.6f },
        } },

        { "Vocal Chops", {
            { "osc1_mode", 2 }, { "osc1_sample_loop", 1 }, { "osc1_level", 0.8f },
            { "osc1_sample_factory", 2 },
            { "osc1_sample_start", 0.1f }, { "osc1_sample_end", 0.6f },
            { "f1_cutoff", 5500.0f },
            { "amp_attack", 0.005f }, { "amp_sustain", 0.9f }, { "amp_release", 0.6f },
            { "fx_slot1", 10 }, { "fx_slot2", 9 }, { "fx_slot10", 13 },
            { "fx_stutter_on", 1 }, { "fx_stutter_div", 4 }, { "fx_stutter_reverse", 1 },
            { "fx_stutter_mix", 0.6f },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 3 }, { "fx_delay_feedback", 0.35f },
            { "fx_delay_mix", 0.25f },
            { "fx_reverb_on", 1 }, { "fx_reverb_size", 0.6f }, { "fx_reverb_mix", 0.3f },
        } },

        { "Tape LFO Drift", {
            { "osc1_table", 8 }, { "osc1_frame", 0.4f }, { "osc1_level", 0.7f },
            { "osc1_unison", 2 }, { "osc1_detune", 10.0f },
            { "f1_cutoff", 2600.0f }, { "f1_reso", 0.25f },
            { "amp_attack", 0.9f }, { "amp_sustain", 0.85f }, { "amp_release", 4.0f },
            { "lfo3_rate", 0.12f },
            { "mod1_src", 20 }, { "mod1_dst", 22 }, { "mod1_amt", 0.5f },
            { "fx_slot9", 9 }, { "fx_slot10", 13 },
            { "fx_delay_on", 1 }, { "fx_delay_sync", 1 }, { "fx_delay_div", 5 }, { "fx_delay_feedback", 0.6f },
            { "fx_delay_pitch", -3.0f }, { "fx_delay_wow", 0.6f }, { "fx_delay_mix", 0.35f },
            { "fx_reverb_on", 1 }, { "fx_reverb_type", 3 }, { "fx_reverb_size", 0.9f }, { "fx_reverb_mix", 0.45f },
        } },
    };

    return presets;
}

inline juce::StringArray getFactoryPresetCategories()
{
    return { "Init", "Bass", "Lead", "Lead", "Lead", "Bass", "Pluck", "Bass", "Lead", "Pad", "Pluck", "Drone", "Bass", "Bass",
             "Lead", "Pluck", "Pluck", "Pad", "Drone",
             "Bass", "Bass", "Bass", "Lead", "Lead", "Lead", "Pluck", "Pluck", "Pad", "Pad", "Pad",
             "Drone", "Drone", "Lead", "Pluck", "Lead", "Pluck", "Lead", "Drone",
             "Bass", "Pluck", "Pluck", "Pad", "Drone", "Pluck", "Lead", "Pad", "Bass", "Pluck", "Lead", "Drone",
             "Lead", "Pad",
             "Pluck", "Pad", "Bass", "Drone", "Lead", "Lead", "FX", "Pad", "Lead", "Pad", "Pluck", "Pluck",
             "Drone", "Bass", "Pad", "Bass", "Lead", "Drone", "Pluck", "Pluck",
             "Pad", "Pluck", "Bass", "Pluck", "Pluck", "Bass", "FX", "Drone" };
}
} // namespace Presets
