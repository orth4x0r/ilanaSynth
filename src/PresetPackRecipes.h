#pragma once

// ilana's recipe pack (2026-11-01): presets grown from her own presets (content/user-presets)
// by tools/preset_gen.py, each with one or more of the newer features.
// Generated: edit the concepts in tools/preset_gen_concepts.py and re-bake,
// not this file. Appended after every earlier preset (indices never move).
// Included from PresetLibrary.h inside Presets::Library.

inline void addRecipePresets (const std::function<void (const FactoryPreset&)>& add)
{
    // her warm chord pad with chaos LFOs moving the frames and the filter, so it never loops (from "Wide Chord Pad - Warmed")
    {
        FactoryPreset p { "Lorenz Chord Pad", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.8f }, { "amp_release", 3.0f }, { "amp_sustain", 0.85f }, { "f1_cutoff", 4200.0f },
            { "filter_velocity", 0.3f }, { "fx_aw_p1", 0.608135f }, { "fx_dim_depth", 0.5f }, { "fx_dim_mix", 0.45f },
            { "fx_dim_rate", 0.2f }, { "fx_haas_delay", 18.0f }, { "fx_haas_mix", 0.5f }, { "fx_patcher", 1.0f },
            { "fx_reverb_mix", 0.4f }, { "fx_reverb_on", 1.0f }, { "fx_reverb_size", 0.9f }, { "fx_reverb_type", 1.0f },
            { "fx_slot1", 30.0f }, { "fx_slot2", 15.0f }, { "fx_slot3", 13.0f }, { "fx_slot4", 22.0f },
            { "fx_width", 1.6f }, { "fx_width_mix", 0.7f }, { "lfo1_div", 1.0f }, { "lfo1_shape", 20.0f },
            { "lfo1_sync", 1.0f }, { "lfo2_div", 0.0f }, { "lfo2_shape", 22.0f }, { "lfo2_sync", 1.0f },
            { "mod1_amt", 0.4f }, { "mod1_dst", 9.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 2.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.3f }, { "mod3_dst", 10.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.4f }, { "mod4_dst", 26.0f }, { "mod4_src", 15.0f },
            { "mod5_amt", 0.6679951f }, { "mod5_dst", 2.0f }, { "mod5_src", 1.0f }, { "mod6_amt", 0.150515f },
            { "mod6_dst", 9.0f }, { "mod6_src", 4.0f }, { "mod7_amt", 0.4169426f }, { "mod7_dst", 29.0f },
            { "mod7_src", 1.0f }, { "mod8_amt", 0.1911157f }, { "mod8_dst", 9.0f }, { "mod8_src", 2.0f },
            { "osc1_detune", 10.0f }, { "osc1_frame", 0.3f }, { "osc1_level", 0.6f }, { "osc1_spread", 1.0f },
            { "osc1_table", 117.0f }, { "osc1_unison", 3.0f }, { "osc2_fine", -7.0f }, { "osc2_spread", 1.0f },
            { "poly_voices", 16.0f }, { "sub_chord", 1.0f }, { "sub_level", 0.3f }, { "sub_on", 1.0f },
            { "sub_semi", -12.0f }, { "sub_table", 118.0f }, { "voice_spread", 0.5f }, { "master", -13.13f },
        };
        p.macroNames = { "TONE", "FRAME", "RESO", "SPACE" };
        add (p);
    }
    // big KEYS with its chaos LFOs kept and the comb filter tuned higher into a bell (from "big KEYS")
    {
        FactoryPreset p { "Chaos Bell Keys", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 0.5509813f }, { "amp_release", 0.3f }, { "amp_sustain", 0.2302225f },
            { "body_material", 0.434717f }, { "e4_attack", 0.001f }, { "e4_sustain", 1.0f }, { "f1_cutoff", 141.865f },
            { "f1_drive", 1.0f }, { "f1_keytrack", 1.0f }, { "f1_reso", 0.17496f }, { "f1_type", 14.0f },
            { "f2_cutoff", 2088.058f }, { "f2_reso", 0.5481359f }, { "f2_type", 21.0f }, { "fm_noise1", 0.683127f },
            { "fm_noise_color", 0.0f }, { "fx_awdelay_div", 9.0f }, { "fx_awdelay_mix", 0.285822f }, { "fx_awdelay_p2", 0.504582f },
            { "fx_awdelay_p3", 0.766523f }, { "fx_awdelay_sync", 1.0f }, { "fx_eq_low_freq", 93.1215f }, { "fx_eq_low_gain", -13.6004f },
            { "fx_slot1", 35.0f }, { "fx_slot2", 29.0f }, { "lfo1_div", 0.0f }, { "lfo1_rate", 0.175923f },
            { "lfo1_shape", 20.0f }, { "lfo1_sync", 1.0f }, { "lfo2_div", 0.0f }, { "lfo2_shape", 22.0f },
            { "lfo2_sync", 1.0f }, { "lfo3_shape", 17.0f }, { "lfo3_sync", 1.0f }, { "mod10_amt", 0.4f },
            { "mod10_dst", 9.0f }, { "mod10_src", 12.0f }, { "mod11_amt", 0.4f }, { "mod11_dst", 12.0f },
            { "mod11_src", 13.0f }, { "mod12_amt", 0.5f }, { "mod12_dst", 2.0f }, { "mod12_src", 14.0f },
            { "mod13_amt", -0.5f }, { "mod13_dst", 99.0f }, { "mod13_src", 15.0f }, { "mod1_amt", 0.60206f },
            { "mod1_dst", 9.0f }, { "mod1_src", 4.0f }, { "mod2_amt", 0.675945f }, { "mod2_dst", 2.0f },
            { "mod2_src", 1.0f }, { "mod3_amt", 1.0f }, { "mod3_dst", 5.0f }, { "mod3_src", 2.0f },
            { "mod4_amt", 0.638f }, { "mod4_dst", 10.0f }, { "mod4_src", 4.0f }, { "mod5_amt", 0.510547f },
            { "mod5_dst", 11.0f }, { "mod5_src", 1.0f }, { "mod6_amt", 0.92f }, { "mod6_dst", 429.0f },
            { "mod6_src", 2.0f }, { "mod7_amt", 0.711854f }, { "mod7_dst", 29.0f }, { "mod7_src", 20.0f },
            { "mod8_amt", 0.35f }, { "mod8_dst", 426.0f }, { "mod8_src", 1.0f }, { "mod9_amt", 0.35f },
            { "mod9_dst", 425.0f }, { "mod9_src", 2.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.342263f },
            { "osc1_level", 1.0f }, { "osc1_table", 711.0f }, { "osc2_fb_type", 1.0f }, { "osc2_frame", 0.707696f },
            { "osc2_on", 1.0f }, { "osc2_table", 702.0f }, { "osc4_fb_type", 1.0f }, { "osc4_on", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "sub_amp_env", 4.0f }, { "sub_detune", 39.2381f },
            { "sub_fb_type", 1.0f }, { "sub_frame", 0.333814f }, { "sub_level", 0.763019f }, { "sub_on", 1.0f },
            { "sub_shape", 1.0f }, { "sub_spectral", 11.0f }, { "sub_spectral_amt", 0.0f }, { "sub_uni_mode", 1.0f },
            { "sub_unison", 16.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "vec_drift", 0.468356f },
            { "vec_on", 1.0f }, { "vec_x", 0.511127f }, { "vec_y", 0.496304f }, { "master", 1.73f },
        };
        p.macroNames = { "TONE", "BELL", "FRAME", "DRY" };
        p.lfoCurves = { "0.0000,0.0000,-0.450;0.2500,1.0000,0.450;0.2500,-1.0000,0.000;0.5000,0.0000,-0.450;0.6250,0.5000,0.000;0.7500,-1.0000,0.450;1.0000,0.0000,0.000", "", "", "" };
        add (p);
    }
    // her trance supersaw pluck with slow random drift on the cutoff and the detune (from "supersaw plucked lead")
    {
        FactoryPreset p { "Chaos Saw Pluck", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 0.177347f }, { "amp_release", 0.5231192f }, { "amp_sustain", 0.0f },
            { "f1_cutoff", 2274.51f }, { "fe_decay", 1.184182f }, { "fe_sustain", 0.0f }, { "fx_awmod_algo", 2.0f },
            { "fx_awmod_p1", 0.543141f }, { "fx_awsat_p1", 0.296658f }, { "fx_delay_damping", 0.261716f }, { "fx_delay_feedback", 0.522152f },
            { "fx_delay_on", 1.0f }, { "fx_delay_pingpong", 1.0f }, { "fx_delay_sync", 1.0f }, { "fx_delay_time", 154.653f },
            { "fx_delay_time_r", 134.947f }, { "fx_patcher", 1.0f }, { "fx_reverb_on", 1.0f }, { "fx_reverb_size", 0.280536f },
            { "fx_reverb_type", 2.0f }, { "fx_slot1", 33.0f }, { "fx_slot2", 13.0f }, { "fx_slot3", 9.0f },
            { "fx_slot4", 36.0f }, { "fx_slot5", 7.0f }, { "fx_slot5_mix", 0.4035785f }, { "lfo3_shape", 18.0f },
            { "lfo3_sync", 1.0f }, { "lfo4_div", 0.0f }, { "lfo4_shape", 20.0f }, { "lfo4_sync", 1.0f },
            { "mod1_amt", 0.293789f }, { "mod1_dst", 9.0f }, { "mod1_src", 4.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 9.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 72.0f },
            { "mod3_src", 13.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 48.0f }, { "mod4_src", 14.0f },
            { "mod5_amt", 0.4f }, { "mod5_dst", 26.0f }, { "mod5_src", 15.0f }, { "mod6_amt", 0.1007609f },
            { "mod6_dst", 9.0f }, { "mod6_src", 20.0f }, { "mod7_amt", 0.2597598f }, { "mod7_dst", 37.0f },
            { "mod7_src", 21.0f }, { "osc1_detune", 34.1351f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f },
            { "osc1_level", 1.0f }, { "osc1_spread", 1.0f }, { "osc1_uni_frame", 0.565847f }, { "osc1_uni_mode", 1.0f },
            { "osc1_uni_warp", 0.51032f }, { "osc1_unison", 6.0f }, { "osc1_warp_amt", 0.370694f }, { "osc2_detune", 23.8579f },
            { "osc2_fb_type", 1.0f }, { "osc2_level", 0.806463f }, { "osc2_on", 1.0f }, { "osc2_semi", -12.0f },
            { "osc2_spread", 0.991811f }, { "osc2_table", 7.0f }, { "osc2_uni_frame", 0.396621f }, { "osc2_uni_mode", 1.0f },
            { "osc2_uni_warp", 0.468013f }, { "osc2_unison", 8.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 2.0f }, { "subosc_level", 0.3f },
            { "subosc_on", 1.0f }, { "master", -2.48f },
        };
        p.macroNames = { "CUTOFF", "DECAY", "WIDTH", "SPACE" };
        add (p);
    }
    // her reese with both envelopes moving the wavetable frames: the growl opens on every note (from "detuned reese plus saw bass")
    {
        FactoryPreset p { "Frame Snap Reese", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "f1_cutoff", 1198.31f }, { "f1_drive", 4.55868f },
            { "f1_reso", 0.306642f }, { "f1_type", 33.0f }, { "fe_decay", 0.2094538f }, { "fe_sustain", 0.2159671f },
            { "glide_legato", 1.0f }, { "me_attack", 0.008997455f }, { "me_decay", 0.4128236f }, { "me_sustain", 0.03262851f },
            { "mod10_amt", 0.6040464f }, { "mod10_dst", 5.0f }, { "mod10_src", 3.0f }, { "mod1_amt", 0.662f },
            { "mod1_dst", 2.0f }, { "mod1_src", 1.0f }, { "mod2_amt", 0.962f }, { "mod2_dst", 5.0f },
            { "mod2_src", 4.0f }, { "mod3_amt", 0.139845f }, { "mod3_dst", 9.0f }, { "mod3_src", 4.0f },
            { "mod4_amt", 0.139845f }, { "mod4_aux", 6.0f }, { "mod4_dst", 9.0f }, { "mod4_src", 4.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 51.0f }, { "mod5_src", 12.0f }, { "mod6_amt", 0.5f },
            { "mod6_dst", 9.0f }, { "mod6_src", 13.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 2.0f },
            { "mod7_src", 14.0f }, { "mod8_amt", 0.4f }, { "mod8_dst", 194.0f }, { "mod8_src", 15.0f },
            { "mod9_amt", 0.8599274f }, { "mod9_dst", 2.0f }, { "mod9_src", 4.0f }, { "osc1_detune", 23.1718f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.500518f }, { "osc1_level", 1.0f }, { "osc1_spread", 1.0f },
            { "osc1_table", 109.0f }, { "osc1_uni_mode", 1.0f }, { "osc1_unison", 7.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_fine", -7.0f }, { "osc2_frame", 0.0205971f }, { "osc2_on", 1.0f }, { "osc2_semi", -12.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 9.0f },
            { "sub_fb_type", 1.0f }, { "sub_shape", 2.0f }, { "subosc_level", 0.768586f }, { "subosc_on", 1.0f },
            { "voice_mode", 1.0f }, { "master", 0.59f },
        };
        p.macroNames = { "GROWL", "CUTOFF", "FRAME", "SUB" };
        add (p);
    }
    // her keys with harder hits opening the vowel and the frame further (from "ilanakeys")
    {
        FactoryPreset p { "Vowel Strike Keys", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_dcurve", -0.301694f }, { "amp_decay", 2.03548f }, { "amp_release", 0.3f },
            { "amp_sustain", 0.215905f }, { "body_coupling", 0.388629f }, { "body_coupling_mode", 1.0f }, { "body_material", 0.24095f },
            { "body_type", 4.0f }, { "f1_cutoff", 3262.18f }, { "f1_reso", 0.599766f }, { "f1_type", 27.0f },
            { "f2_cutoff", 346.241f }, { "f2_reso", 0.805685f }, { "f2_type", 26.0f }, { "filter_velocity", 0.5f },
            { "fx_amp_drive", 7.64275f }, { "fx_amp_mode", 1.0f }, { "fx_aw_algo", 52.0f }, { "fx_awdelay_p1", 0.953365f },
            { "fx_awdelay_p2", 0.344584f }, { "fx_delay_on", 1.0f }, { "fx_delay_sync", 1.0f }, { "fx_patcher", 1.0f },
            { "fx_reverb_on", 1.0f }, { "fx_slot1", 30.0f }, { "fx_slot2", 13.0f }, { "fx_slot3", 9.0f },
            { "me_attack", 0.001f }, { "me_decay", 0.6231958f }, { "me_sustain", 0.0f }, { "mod10_amt", 0.2789796f },
            { "mod10_dst", 2.0f }, { "mod10_src", 6.0f }, { "mod1_amt", 1.0f }, { "mod1_dst", 2.0f },
            { "mod1_pol", 1.0f }, { "mod1_src", 3.0f }, { "mod2_amt", 0.998f }, { "mod2_dst", 87.0f },
            { "mod2_src", 3.0f }, { "mod3_amt", 0.60206f }, { "mod3_dst", 11.0f }, { "mod3_pol", 2.0f },
            { "mod3_src", 5.0f }, { "mod4_amt", 0.150515f }, { "mod4_dst", 9.0f }, { "mod4_src", 4.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 87.0f }, { "mod5_src", 12.0f }, { "mod6_amt", 0.5f },
            { "mod6_dst", 76.0f }, { "mod6_src", 13.0f }, { "mod7_amt", 0.3f }, { "mod7_dst", 12.0f },
            { "mod7_src", 14.0f }, { "mod8_amt", 0.4f }, { "mod8_dst", 26.0f }, { "mod8_src", 15.0f },
            { "mod9_amt", 0.3945519f }, { "mod9_dst", 87.0f }, { "mod9_src", 6.0f }, { "noise_level", 0.00129578f },
            { "noise_on", 0.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.260899f }, { "osc1_level", 1.0f },
            { "osc1_table", 24.0f }, { "osc2_fb_type", 1.0f }, { "osc2_fine", -7.0f }, { "osc2_mode", 1.0f },
            { "osc2_on", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc4_on", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "res_amount", 0.510363f }, { "res_on", 1.0f }, { "sub_fb_type", 1.0f },
            { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "master", -2.36f },
        };
        p.macroNames = { "VOWEL", "STRIKE", "BITE", "SPACE" };
        add (p);
    }
    // her euclid random keys on another pattern, each hit also panned and voiced at random (from "euclidian random keys")
    {
        FactoryPreset p { "Euclid Vowel Glass", {} };
        p.category = "Generative";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "euc_hits", 8.0f }, { "euc_on", 1.0f },
            { "euc_rotate", 6.0f }, { "f1_cutoff", 1054.67f }, { "f1_keytrack", 1.0f }, { "f1_reso", 0.790558f },
            { "fx_awdelay_div", 9.0f }, { "fx_awdelay_mix", 0.334986f }, { "fx_awdelay_sync", 1.0f }, { "fx_awverb_algo", 2.0f },
            { "fx_awverb_mix", 0.948248f }, { "fx_chorus_depth", 0.266361f }, { "fx_chorus_mix", 0.412022f }, { "fx_chorus_on", 1.0f },
            { "fx_chorus_rate", 0.46223f }, { "fx_patcher", 1.0f }, { "fx_slot1", 7.0f }, { "fx_slot2", 27.0f },
            { "fx_slot3", 11.0f }, { "fx_slot4", 35.0f }, { "fx_slot5", 34.0f }, { "fx_slot5_mix", 0.3984702f },
            { "fx_smear_on", 1.0f }, { "fx_vowel_morph", 0.494232f }, { "mod10_amt", 0.4f }, { "mod10_dst", 9.0f },
            { "mod10_src", 14.0f }, { "mod11_amt", 0.5f }, { "mod11_dst", 103.0f }, { "mod11_src", 15.0f },
            { "mod12_amt", 0.5813862f }, { "mod12_dst", 39.0f }, { "mod12_src", 8.0f }, { "mod13_amt", -0.4829818f },
            { "mod13_dst", 40.0f }, { "mod13_src", 8.0f }, { "mod1_amt", 0.150515f }, { "mod1_dst", 9.0f },
            { "mod1_src", 4.0f }, { "mod2_amt", 0.475274f }, { "mod2_dst", 2.0f }, { "mod2_src", 8.0f },
            { "mod3_amt", 0.502326f }, { "mod3_dst", 5.0f }, { "mod3_src", 8.0f }, { "mod4_amt", 0.584f },
            { "mod4_dst", 42.0f }, { "mod4_src", 8.0f }, { "mod5_amt", 0.48f }, { "mod5_dst", 43.0f },
            { "mod5_src", 8.0f }, { "mod6_amt", 0.524996f }, { "mod6_dst", 9.0f }, { "mod6_src", 8.0f },
            { "mod7_amt", 0.602f }, { "mod7_dst", 173.0f }, { "mod7_src", 8.0f }, { "mod8_amt", 0.5f },
            { "mod8_dst", 173.0f }, { "mod8_src", 12.0f }, { "mod9_amt", 0.5f }, { "mod9_dst", 42.0f },
            { "mod9_src", 13.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.488014f }, { "osc1_level", 1.0f },
            { "osc1_table", 448.0f }, { "osc1_warp", 16.0f }, { "osc1_warp_amt", 0.465768f }, { "osc2_fb_type", 1.0f },
            { "osc2_frame", 0.462666f }, { "osc2_level", 1.0f }, { "osc2_on", 1.0f }, { "osc2_table", 400.0f },
            { "osc2_warp", 8.0f }, { "osc2_warp_amt", 0.49682f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f }, { "subosc_level", 0.3f },
            { "master", -6.9f },
        };
        p.macroNames = { "VOWEL", "WARP", "TONE", "SPACE" };
        add (p);
    }
    // her feedback and bowed strings with the mod envelope striking the west fold (from "nOta Pluck")
    {
        FactoryPreset p { "Feedback Bow Pluck", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 1.58653f }, { "amp_release", 0.5755852f }, { "amp_sustain", 0.05f },
            { "body_coupling_mode", 1.0f }, { "body_material", 0.571429f }, { "body_size", 0.782857f }, { "body_type", 5.0f },
            { "f1_cutoff", 31.3213f }, { "f1_keytrack", 1.0f }, { "fx_slot1", 34.0f }, { "fx_slot1_mix", 0.1250879f },
            { "me_attack", 0.001f }, { "me_decay", 0.2541384f }, { "me_sustain", 0.0f }, { "mod1_amt", 0.60206f },
            { "mod1_dst", 9.0f }, { "mod1_src", 4.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 414.0f },
            { "mod2_src", 12.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 268.0f }, { "mod3_src", 13.0f },
            { "mod4_amt", 0.4f }, { "mod4_dst", 395.0f }, { "mod4_src", 14.0f }, { "mod5_amt", 0.5f },
            { "mod5_dst", 99.0f }, { "mod5_src", 15.0f }, { "mod6_amt", 0.2182984f }, { "mod6_dst", 414.0f },
            { "mod6_src", 3.0f }, { "osc1_detune", 7.54381f }, { "osc1_excite", 10.0f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_fret_rattle", 0.202857f }, { "osc1_hammer_hard", 0.24f }, { "osc1_level", 1.0f },
            { "osc1_mode", 1.0f }, { "osc1_register", 0.302857f }, { "osc1_string_damp", 0.595714f }, { "osc1_string_decay", 0.851429f },
            { "osc1_string_excite_pos", 0.205714f }, { "osc1_string_stiffness", 0.394286f }, { "osc1_string_sustain", 0.2312825f }, { "osc1_table", 0.0f },
            { "osc1_uni_mode", 1.0f }, { "osc1_unison", 2.0f }, { "osc2_bridge_buzz", 0.268571f }, { "osc2_detune", 13.6727f },
            { "osc2_excite", 4.0f }, { "osc2_fb_type", 1.0f }, { "osc2_fret_rattle", 0.237143f }, { "osc2_level", 1.0f },
            { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_register", 0.225714f }, { "osc2_string_damp", 0.794286f },
            { "osc2_string_decay", 0.882857f }, { "osc2_string_excite_pos", 0.517143f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 2.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "pedal_res", 0.602857f },
            { "res_decay", 0.311429f }, { "res_on", 1.0f }, { "sb_model", 1.0f }, { "sb_on", 1.0f },
            { "sb_size", 0.745714f }, { "stretch", 0.574286f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "sym_amount", 0.428571f }, { "voice_spread", 0.634286f }, { "west_fold", 0.4862922f },
            { "west_on", 1.0f }, { "west_open", 0.405714f }, { "master", -0.61f },
        };
        p.macroNames = { "FOLD", "BUZZ", "BODY", "SPACE" };
        add (p);
    }
}
