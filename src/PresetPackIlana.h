#pragma once

// ilana's pack: presets grown from her own presets (content/user-presets)
// by tools/preset_gen.py, each with one or more of the newer features.
// Generated: edit the concepts in tools/preset_gen_concepts.py and re-bake,
// not this file. Appended after every earlier preset (indices never move).
// Included from PresetLibrary.h inside Presets::Library.

inline void addIlanaPresets (const std::function<void (const FactoryPreset&)>& add)
{
    // her brass folded hard and squeezed through the Airwindows acid filter, mono (from "classic synth brass, no fx")
    {
        FactoryPreset p { "Acid Fold Lead", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "amp_sustain", 0.296808f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.244851f }, { "body_type", 2.0f }, { "drift", 0.293017f }, { "f1_cutoff", 821.4978f },
            { "f1_drive", 2.00576f }, { "f1_env", 5.0f }, { "f1_reso", 0.5834206f }, { "f1_type", 32.0f },
            { "fe_attack", 0.108571f }, { "fx_awverb_algo", 2.0f }, { "fx_awverb_mix", 0.24538f }, { "fx_awverb_p1", 1.0f },
            { "fx_slot1", 15.0f }, { "fx_slot2", 34.0f }, { "fx_slot3", 20.0f }, { "glide", 0.002f },
            { "glide_legato", 1.0f }, { "master_clip", 0.0f }, { "mod1_amt", 0.183048f }, { "mod1_dst", 9.0f },
            { "mod1_src", 6.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 9.0f }, { "mod2_src", 12.0f },
            { "mod3_amt", 0.4f }, { "mod3_dst", 414.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.4f },
            { "mod4_dst", 10.0f }, { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 100.0f },
            { "mod5_src", 15.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 1.0f },
            { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "poly_voices", 1.0f }, { "res_amount", 0.445812f }, { "res_on", 1.0f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.740836f }, { "sub_fb_type", 1.0f },
            { "sub_shape", 1.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "voice_mode", 2.0f },
            { "voice_spread", 0.523819f }, { "west_fold", 0.7702401f }, { "west_on", 1.0f }, { "west_open", 0.365268f },
            { "west_res", 0.322425f }, { "west_src", 4.0f }, { "west_stages", 3.0f }, { "west_strike", 0.564488f },
            { "master", -3.89f },
        };
        p.macroNames = { "ACID", "FOLD", "RESO", "SPACE" };
        add (p);
    }
    // her arped pluck bass played straight through a trance gate instead (from "ARPed pluck Bass")
    {
        FactoryPreset p { "Arp Gate Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.005999f }, { "amp_decay", 0.299705f }, { "amp_release", 0.254875f }, { "arp_gate", 0.05f },
            { "e4_attack", 0.05099f }, { "e4_decay", 0.399605f }, { "e4_release", 0.30485f }, { "f1_cutoff", 459.56f },
            { "f1_drive", 6.652f }, { "f1_fm", 0.394f }, { "f1_reso", 0.51548f }, { "f1_slope", 1.0f },
            { "f2_cutoff", 359.66f }, { "f2_drive", 5.95f }, { "f2_reso", 0.59486f }, { "f2_slope", 1.0f },
            { "f2_type", 3.0f }, { "f2e_attack", 0.010998f }, { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f },
            { "fe_decay", 0.499505f }, { "fe_release", 0.30485f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_rate", 0.7973f },
            { "fx_comb_feedback", 0.60043f }, { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f },
            { "fx_comp_release", 149.59f }, { "fx_delay_feedback", 0.3496f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f },
            { "fx_dim_rate", 0.40155f }, { "fx_drive_amount", 4.002f }, { "fx_drive_mix", 0.5f }, { "fx_feedback_amount", 0.6004f },
            { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f }, { "fx_flanger_rate", 0.50315f }, { "fx_gate_mix", 0.7864874f },
            { "fx_gate_smooth", 0.1961729f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f },
            { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_ring_freq", 220.956f }, { "fx_slot1", 20.0f },
            { "fx_slot2", 2.0f }, { "fx_slot3", 16.0f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f },
            { "fx_tape_stop_time", 0.59905f }, { "fx_trem_rate", 4.0001f }, { "lfo1_div", 0.0f }, { "lfo1_rate", 4.009f },
            { "lfo1_retrig", 1.0f }, { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f },
            { "lfo1_step14", -0.924f }, { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f },
            { "lfo1_step3", 0.708f }, { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f },
            { "lfo1_step8", 0.382f }, { "lfo1_sync", 1.0f }, { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f },
            { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f }, { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f },
            { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f }, { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f },
            { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f }, { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f },
            { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f }, { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f },
            { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f }, { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f },
            { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f }, { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f },
            { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f }, { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f },
            { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f }, { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f },
            { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f }, { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f },
            { "lfo4_step8", 0.382f }, { "master_clip_gain", 2.5f }, { "me_attack", 0.05099f }, { "me_decay", 0.399605f },
            { "me_release", 0.30485f }, { "mod10_amt", 0.5f }, { "mod10_dst", 100.0f }, { "mod10_src", 15.0f },
            { "mod1_amt", 1.0f }, { "mod1_dst", 2.0f }, { "mod1_src", 1.0f }, { "mod2_amt", -0.146f },
            { "mod2_dst", 9.0f }, { "mod2_src", 1.0f }, { "mod3_src", 1.0f }, { "mod4_amt", -0.308f },
            { "mod4_dst", 11.0f }, { "mod4_src", 1.0f }, { "mod5_amt", 0.35f }, { "mod5_dst", 5.0f },
            { "mod5_src", 1.0f }, { "mod6_amt", 0.35f }, { "mod6_dst", 29.0f }, { "mod6_src", 1.0f },
            { "mod7_amt", -0.8f }, { "mod7_dst", 154.0f }, { "mod7_src", 12.0f }, { "mod8_amt", 0.5f },
            { "mod8_dst", 9.0f }, { "mod8_src", 13.0f }, { "mod9_amt", 0.5f }, { "mod9_dst", 99.0f },
            { "mod9_src", 14.0f }, { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f }, { "mseg_time2", 0.25045f },
            { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f }, { "noise_level", 0.296f }, { "osc1_chord", 2.0f },
            { "osc1_frame", 0.488f }, { "osc1_table", 3.0f }, { "osc1_unison", 3.0f }, { "osc2_chord", 3.0f },
            { "osc2_detune", 10.0f }, { "osc2_fine", 0.0f }, { "osc2_frame", 0.213f }, { "osc2_level", 0.5f },
            { "osc2_on", 1.0f }, { "osc2_spread", 0.704f }, { "osc2_table", 5.0f }, { "osc2_unison", 4.0f },
            { "sub_chord", 3.0f }, { "sub_detune", 0.0f }, { "sub_frame", 0.011f }, { "sub_level", 0.4f },
            { "sub_on", 1.0f }, { "sub_semi", 24.0f }, { "sub_shape", 1.0f }, { "sub_spread", 0.522f },
            { "sub_table", 17.0f }, { "sub_unison", 5.0f }, { "unison_random", 0.136f }, { "voice_spread", 0.607f },
            { "master", -13.92f },
        };
        p.macroNames = { "CHOP", "CUTOFF", "SQUASH", "DRIVE" };
        add (p);
    }
    // her bowed pad sung through a vowel filter that drifts (from "phys_bowed pad")
    {
        FactoryPreset p { "Bowed Choir", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.155862f }, { "amp_release", 1.19077f }, { "amp_sustain", 0.514214f }, { "f1_cutoff", 1906.069f },
            { "f1_morph", 0.2098707f }, { "f1_type", 27.0f }, { "fx_slot1", 34.0f }, { "fx_slot1_mix", 0.4892254f },
            { "lfo1_rate", 0.1260745f }, { "mod1_amt", 0.548f }, { "mod1_dst", 272.0f }, { "mod1_src", 1.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 87.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.4f },
            { "mod3_dst", 270.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.4f }, { "mod4_dst", 9.0f },
            { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 99.0f }, { "mod5_src", 15.0f },
            { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_level", 1.0f },
            { "osc1_mode", 1.0f }, { "osc1_on", 0.0f }, { "osc1_table", 0.0f }, { "osc2_bow_pressure", 0.543769f },
            { "osc2_bow_speed", 0.844484f }, { "osc2_detune", 18.6045f }, { "osc2_excite", 4.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_register", 0.175076f }, { "osc2_semi", -12.0f },
            { "osc2_string_damp", 0.491758f }, { "osc2_string_decay", 0.819516f }, { "osc2_string_excite_pos", 0.0463437f }, { "osc2_string_stiffness", 0.7312f },
            { "osc2_string_sustain", 0.352727f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 8.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "master", 11.65f },
        };
        p.macroNames = { "VOWEL", "BOW", "TONE", "SPACE" };
        add (p);
    }
    // one bowed string, mono, with a few milliseconds of legato glide (from "phys_bowed pad")
    {
        FactoryPreset p { "Bowed Lead", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.02f }, { "amp_release", 0.2559225f }, { "amp_sustain", 0.514214f }, { "f1_cutoff", 20000.0f },
            { "fx_slot1", 35.0f }, { "fx_slot1_mix", 0.2181213f }, { "glide_legato", 1.0f }, { "mod1_amt", 0.548f },
            { "mod1_dst", 272.0f }, { "mod1_src", 1.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 270.0f },
            { "mod2_src", 12.0f }, { "mod3_amt", 0.4f }, { "mod3_dst", 271.0f }, { "mod3_src", 13.0f },
            { "mod4_amt", 0.4f }, { "mod4_dst", 188.0f }, { "mod4_src", 14.0f }, { "mod5_amt", 0.5f },
            { "mod5_dst", 99.0f }, { "mod5_src", 15.0f }, { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_on", 0.0f },
            { "osc1_table", 0.0f }, { "osc2_bow_pressure", 0.543769f }, { "osc2_bow_speed", 0.844484f }, { "osc2_detune", 18.6045f },
            { "osc2_excite", 4.0f }, { "osc2_fb_type", 1.0f }, { "osc2_mode", 1.0f }, { "osc2_on", 1.0f },
            { "osc2_register", 0.175076f }, { "osc2_string_damp", 0.491758f }, { "osc2_string_decay", 0.819516f }, { "osc2_string_excite_pos", 0.0463437f },
            { "osc2_string_stiffness", 0.7312f }, { "osc2_string_sustain", 0.352727f }, { "osc2_uni_mode", 1.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 1.0f }, { "sub_fb_type", 1.0f },
            { "sub_shape", 1.0f }, { "subosc_level", 0.3f }, { "voice_mode", 2.0f }, { "master", 6.52f },
        };
        p.macroNames = { "PRESSURE", "SPEED", "TONE", "ECHO" };
        add (p);
    }
    // her piano with a quiet bowed string an octave up swelling in behind it (from "real piano SOUND")
    {
        FactoryPreset p { "Bowed Piano", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 4.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.6f }, { "mod1_dst", 6.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 270.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 335.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f },
            { "mod4_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f }, { "osc1_excite", 9.0f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f },
            { "osc1_mode", 1.0f }, { "osc1_register", 0.15848f }, { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 1.0f },
            { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f },
            { "osc1_table", 0.0f }, { "osc2_bow_pressure", 0.4991273f }, { "osc2_bow_speed", 0.4431725f }, { "osc2_excite", 4.0f },
            { "osc2_fb_type", 1.0f }, { "osc2_level", 0.2172935f }, { "osc2_mode", 1.0f }, { "osc2_on", 1.0f },
            { "osc2_semi", 12.0f }, { "osc2_string_sustain", 0.4654793f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f }, { "sb_mix", 0.355458f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f },
            { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f }, { "west_fold", 0.432153f },
            { "west_on", 1.0f }, { "west_open", 0.712386f }, { "master", -0.69f },
        };
        p.macroNames = { "BOW", "PRESSURE", "BOARD", "SPACE" };
        add (p);
    }
    // her classic brass widened by an ensemble (from "classic synth brass, no fx")
    {
        FactoryPreset p { "Brass Section", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "amp_sustain", 0.296808f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.244851f }, { "body_type", 2.0f }, { "drift", 0.293017f }, { "f1_cutoff", 176.638f },
            { "f1_drive", 2.00576f }, { "f1_env", 5.0f }, { "f1_type", 29.0f }, { "fe_attack", 0.108571f },
            { "fx_awverb_algo", 2.0f }, { "fx_awverb_mix", 0.24538f }, { "fx_awverb_p1", 1.0f }, { "fx_slot1", 36.0f },
            { "fx_slot1_mix", 0.319476f }, { "fx_slot2", 34.0f }, { "fx_slot3", 20.0f }, { "master_clip", 0.0f },
            { "mod1_amt", 0.183048f }, { "mod1_dst", 9.0f }, { "mod1_src", 6.0f }, { "mod2_amt", 0.4f },
            { "mod2_dst", 53.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.4f }, { "mod3_dst", 414.0f },
            { "mod3_src", 13.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f }, { "mod4_src", 14.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 100.0f }, { "mod5_src", 15.0f }, { "osc1_detune", 9.010934f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 1.0f }, { "osc1_table", 0.0f },
            { "osc1_unison", 3.0f }, { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "poly_voices", 6.0f }, { "res_amount", 0.445812f }, { "res_on", 1.0f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.740836f }, { "sub_fb_type", 1.0f },
            { "sub_shape", 1.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "voice_spread", 0.523819f },
            { "west_fold", 0.277923f }, { "west_on", 1.0f }, { "west_open", 0.365268f }, { "west_res", 0.322425f },
            { "west_src", 4.0f }, { "west_stages", 3.0f }, { "west_strike", 0.564488f }, { "master", -6.11f },
        };
        p.macroNames = { "BRASS", "FOLD", "WIDTH", "SPACE" };
        add (p);
    }
    // her bowed pad chopped soft by the trance gate before the space (from "phys_bowed pad")
    {
        FactoryPreset p { "Breathing Bow", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.155862f }, { "amp_release", 1.19077f }, { "amp_sustain", 0.514214f }, { "f1_cutoff", 20000.0f },
            { "fx_gate_mix", 0.4182555f }, { "fx_gate_smooth", 0.7362496f }, { "fx_slot1", 16.0f }, { "fx_slot2", 34.0f },
            { "fx_slot2_mix", 0.3070939f }, { "mod1_amt", 0.548f }, { "mod1_dst", 272.0f }, { "mod1_src", 1.0f },
            { "mod2_amt", -0.7f }, { "mod2_dst", 154.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 153.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.4f }, { "mod4_dst", 270.0f },
            { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 100.0f }, { "mod5_src", 15.0f },
            { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_level", 1.0f },
            { "osc1_mode", 1.0f }, { "osc1_on", 0.0f }, { "osc1_table", 0.0f }, { "osc2_bow_pressure", 0.543769f },
            { "osc2_bow_speed", 0.844484f }, { "osc2_detune", 18.6045f }, { "osc2_excite", 4.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_register", 0.175076f }, { "osc2_semi", -12.0f },
            { "osc2_string_damp", 0.491758f }, { "osc2_string_decay", 0.819516f }, { "osc2_string_excite_pos", 0.0463437f }, { "osc2_string_stiffness", 0.7312f },
            { "osc2_string_sustain", 0.352727f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 8.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "master", 5.71f },
        };
        p.macroNames = { "BREATH", "SMOOTH", "BOW", "SPACE" };
        add (p);
    }
    // her patcher bass re-cabled: oscillator 2 through the WEST voice (from "patcher bass")
    {
        FactoryPreset p { "Cable Pluck Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 0.527841f }, { "amp_release", 0.3f }, { "amp_sustain", 0.172501f },
            { "amp_velocity", 0.660451f }, { "cable_f1", 8.0f }, { "cable_osc1", 7.0f }, { "cable_osc2", 7.0f },
            { "cable_osc3", 7.0f }, { "cable_subnoise", 5.0f }, { "f1_cutoff", 4054.94f }, { "f1_drive", 1.15242f },
            { "f1_env", 5.0f }, { "f1_reso", 0.0f }, { "f2_cutoff", 1853.36f }, { "f2_env", 5.0f },
            { "f2_type", 14.0f }, { "f2e_decay", 0.0391077f }, { "f2e_release", 0.005f }, { "f2e_sustain", 0.0f },
            { "fe_curve", 0.453138f }, { "fe_dcurve", -0.594353f }, { "fe_rcurve", -1.0f }, { "fe_sustain", 0.0746646f },
            { "fm_noise2", 0.0849634f }, { "fx_awsat_algo", 2.0f }, { "fx_patcher", 1.0f }, { "fx_routing", 1.0f },
            { "fx_slot1", 31.0f }, { "fx_slot2", 33.0f }, { "fx_voc_bands", 24.0f }, { "fx_voc_width", 0.476249f },
            { "mod1_amt", -0.814f }, { "mod1_dst", 5.0f }, { "mod1_src", 5.0f }, { "mod2_amt", 0.782f },
            { "mod2_dst", 428.0f }, { "mod2_src", 5.0f }, { "mod3_amt", 0.542f }, { "mod3_dst", 345.0f },
            { "mod3_src", 5.0f }, { "mod4_amt", -0.5f }, { "mod4_dst", 414.0f }, { "mod4_src", 12.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 6.0f }, { "mod5_src", 13.0f }, { "mod6_amt", 0.4f },
            { "mod6_dst", 9.0f }, { "mod6_src", 14.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 99.0f },
            { "mod7_src", 15.0f }, { "noise_type", 1.0f }, { "osc1_excite", 8.0f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_table", 0.0f },
            { "osc1_unison", 2.0f }, { "osc2_fb_type", 1.0f }, { "osc2_frame", 0.916574f }, { "osc2_level", 0.5848283f },
            { "osc2_on", 1.0f }, { "osc2_semi", 12.0f }, { "osc2_spectral", 2.0f }, { "osc2_spectral_amt", 0.0139103f },
            { "osc2_table", 37.0f }, { "osc2_warp2", 9.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "route_cables", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "west_fold", 1.0f }, { "west_mode", 1.0f },
            { "west_on", 1.0f }, { "west_stages", 1.0f }, { "west_strike", 0.6037403f }, { "master", -3.54f },
        };
        p.macroNames = { "FOLD", "BRIGHT", "TONE", "DIRT" };
        add (p);
    }
    // her piano's string under the probability sequencer, struck by a mallet (from "real piano SOUND")
    {
        FactoryPreset p { "Chance Mallets", {} };
        p.category = "Generative";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 1.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.5f }, { "mod1_dst", 396.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 185.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 335.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f },
            { "mod4_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f },
            { "osc1_register", 0.15848f }, { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 0.7840338f }, { "osc1_string_excite_pos", 0.739229f },
            { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "pseq_on", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f }, { "sb_mix", 0.355458f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f },
            { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f }, { "west_fold", 0.432153f },
            { "west_on", 1.0f }, { "west_open", 0.712386f }, { "master", -1.92f },
        };
        p.macroNames = { "BODY", "DAMP", "BOARD", "SPACE" };
        add (p);
    }
    // a round sub with the Disperser's zap on every note (from "Retro Bass")
    {
        FactoryPreset p { "Disperser Sub", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_decay", 2.39761f }, { "amp_release", 0.6047f }, { "amp_sustain", 0.0f },
            { "arp_gate", 0.5003f }, { "e4_attack", 0.05099f }, { "e4_decay", 0.399605f }, { "e4_release", 0.30485f },
            { "env10_attack", 0.05099f }, { "env10_decay", 0.399605f }, { "env10_release", 0.30485f }, { "env11_attack", 0.05099f },
            { "env11_decay", 0.399605f }, { "env11_release", 0.30485f }, { "env12_attack", 0.05099f }, { "env12_decay", 0.399605f },
            { "env12_release", 0.30485f }, { "env13_attack", 0.05099f }, { "env13_decay", 0.399605f }, { "env13_release", 0.30485f },
            { "env14_attack", 0.05099f }, { "env14_decay", 0.399605f }, { "env14_release", 0.30485f }, { "env15_attack", 0.05099f },
            { "env15_decay", 0.399605f }, { "env15_release", 0.30485f }, { "env16_attack", 0.05099f }, { "env16_decay", 0.399605f },
            { "env16_release", 0.30485f }, { "env6_attack", 0.05099f }, { "env6_decay", 0.399605f }, { "env6_release", 0.30485f },
            { "env7_attack", 0.05099f }, { "env7_decay", 0.399605f }, { "env7_release", 0.30485f }, { "env8_attack", 0.05099f },
            { "env8_decay", 0.399605f }, { "env8_release", 0.30485f }, { "env9_attack", 0.05099f }, { "env9_decay", 0.399605f },
            { "env9_release", 0.30485f }, { "f1_cutoff", 1798.22f }, { "f1_drive", 1.0f }, { "f1_env", 2.0f },
            { "f1_keytrack", 0.5f }, { "f1_reso", 0.1f }, { "f1_type", 4.0f }, { "f2_cutoff", 1766.759f },
            { "f2_env", 3.090574f }, { "f2_morph", 0.5989061f }, { "f2_type", 38.0f }, { "f2e_attack", 0.010998f },
            { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f }, { "fe_decay", 0.599405f }, { "fe_release", 0.50475f },
            { "fe_sustain", 0.1f }, { "filter_velocity", 0.8f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_depth", 0.15f },
            { "fx_chorus_mix", 0.2f }, { "fx_chorus_on", 1.0f }, { "fx_chorus_rate", 0.3998f }, { "fx_comb_feedback", 0.60043f },
            { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f }, { "fx_comp_release", 149.59f },
            { "fx_delay_feedback", 0.3496f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f }, { "fx_dim_rate", 0.40155f },
            { "fx_drive_amount", 2.995f }, { "fx_eq_high_freq", 5998.0f }, { "fx_eq_low_freq", 119.84f }, { "fx_eq_mid_freq", 1004.4f },
            { "fx_eq_mid_q", 0.902f }, { "fx_feedback_amount", 0.6004f }, { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f },
            { "fx_flanger_rate", 0.50315f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f },
            { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_reverb_mix", 0.18f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_size", 0.55f }, { "fx_reverb_type", 1.0f }, { "fx_ring_freq", 220.956f }, { "fx_slot1", 7.0f },
            { "fx_slot2", 37.0f }, { "fx_slot2_mix", 0.7859781f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f },
            { "fx_tape_stop_time", 0.59905f }, { "fx_trem_rate", 4.0001f }, { "glide_legato", 1.0f }, { "lfo10_rate", 4.009f },
            { "lfo10_step10", -0.382f }, { "lfo10_step11", -0.708f }, { "lfo10_step12", -0.924f }, { "lfo10_step14", -0.924f },
            { "lfo10_step15", -0.708f }, { "lfo10_step16", -0.382f }, { "lfo10_step2", 0.382f }, { "lfo10_step3", 0.708f },
            { "lfo10_step4", 0.924f }, { "lfo10_step6", 0.924f }, { "lfo10_step7", 0.708f }, { "lfo10_step8", 0.382f },
            { "lfo11_rate", 4.009f }, { "lfo11_step10", -0.382f }, { "lfo11_step11", -0.708f }, { "lfo11_step12", -0.924f },
            { "lfo11_step14", -0.924f }, { "lfo11_step15", -0.708f }, { "lfo11_step16", -0.382f }, { "lfo11_step2", 0.382f },
            { "lfo11_step3", 0.708f }, { "lfo11_step4", 0.924f }, { "lfo11_step6", 0.924f }, { "lfo11_step7", 0.708f },
            { "lfo11_step8", 0.382f }, { "lfo12_rate", 4.009f }, { "lfo12_step10", -0.382f }, { "lfo12_step11", -0.708f },
            { "lfo12_step12", -0.924f }, { "lfo12_step14", -0.924f }, { "lfo12_step15", -0.708f }, { "lfo12_step16", -0.382f },
            { "lfo12_step2", 0.382f }, { "lfo12_step3", 0.708f }, { "lfo12_step4", 0.924f }, { "lfo12_step6", 0.924f },
            { "lfo12_step7", 0.708f }, { "lfo12_step8", 0.382f }, { "lfo13_rate", 4.009f }, { "lfo13_step10", -0.382f },
            { "lfo13_step11", -0.708f }, { "lfo13_step12", -0.924f }, { "lfo13_step14", -0.924f }, { "lfo13_step15", -0.708f },
            { "lfo13_step16", -0.382f }, { "lfo13_step2", 0.382f }, { "lfo13_step3", 0.708f }, { "lfo13_step4", 0.924f },
            { "lfo13_step6", 0.924f }, { "lfo13_step7", 0.708f }, { "lfo13_step8", 0.382f }, { "lfo14_rate", 4.009f },
            { "lfo14_step10", -0.382f }, { "lfo14_step11", -0.708f }, { "lfo14_step12", -0.924f }, { "lfo14_step14", -0.924f },
            { "lfo14_step15", -0.708f }, { "lfo14_step16", -0.382f }, { "lfo14_step2", 0.382f }, { "lfo14_step3", 0.708f },
            { "lfo14_step4", 0.924f }, { "lfo14_step6", 0.924f }, { "lfo14_step7", 0.708f }, { "lfo14_step8", 0.382f },
            { "lfo15_rate", 4.009f }, { "lfo15_step10", -0.382f }, { "lfo15_step11", -0.708f }, { "lfo15_step12", -0.924f },
            { "lfo15_step14", -0.924f }, { "lfo15_step15", -0.708f }, { "lfo15_step16", -0.382f }, { "lfo15_step2", 0.382f },
            { "lfo15_step3", 0.708f }, { "lfo15_step4", 0.924f }, { "lfo15_step6", 0.924f }, { "lfo15_step7", 0.708f },
            { "lfo15_step8", 0.382f }, { "lfo16_rate", 4.009f }, { "lfo16_step10", -0.382f }, { "lfo16_step11", -0.708f },
            { "lfo16_step12", -0.924f }, { "lfo16_step14", -0.924f }, { "lfo16_step15", -0.708f }, { "lfo16_step16", -0.382f },
            { "lfo16_step2", 0.382f }, { "lfo16_step3", 0.708f }, { "lfo16_step4", 0.924f }, { "lfo16_step6", 0.924f },
            { "lfo16_step7", 0.708f }, { "lfo16_step8", 0.382f }, { "lfo1_rate", 4.009f }, { "lfo1_step10", -0.382f },
            { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f }, { "lfo1_step15", -0.708f },
            { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f }, { "lfo1_step4", 0.924f },
            { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f }, { "lfo2_rate", 4.009f },
            { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f }, { "lfo2_step14", -0.924f },
            { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f }, { "lfo2_step3", 0.708f },
            { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f }, { "lfo2_step8", 0.382f },
            { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f }, { "lfo3_step12", -0.924f },
            { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f }, { "lfo3_step2", 0.382f },
            { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f }, { "lfo3_step7", 0.708f },
            { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f }, { "lfo4_step11", -0.708f },
            { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f }, { "lfo4_step16", -0.382f },
            { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f }, { "lfo4_step6", 0.924f },
            { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f }, { "lfo5_rate", 4.009f }, { "lfo5_step10", -0.382f },
            { "lfo5_step11", -0.708f }, { "lfo5_step12", -0.924f }, { "lfo5_step14", -0.924f }, { "lfo5_step15", -0.708f },
            { "lfo5_step16", -0.382f }, { "lfo5_step2", 0.382f }, { "lfo5_step3", 0.708f }, { "lfo5_step4", 0.924f },
            { "lfo5_step6", 0.924f }, { "lfo5_step7", 0.708f }, { "lfo5_step8", 0.382f }, { "lfo6_rate", 4.009f },
            { "lfo6_step10", -0.382f }, { "lfo6_step11", -0.708f }, { "lfo6_step12", -0.924f }, { "lfo6_step14", -0.924f },
            { "lfo6_step15", -0.708f }, { "lfo6_step16", -0.382f }, { "lfo6_step2", 0.382f }, { "lfo6_step3", 0.708f },
            { "lfo6_step4", 0.924f }, { "lfo6_step6", 0.924f }, { "lfo6_step7", 0.708f }, { "lfo6_step8", 0.382f },
            { "lfo7_rate", 4.009f }, { "lfo7_step10", -0.382f }, { "lfo7_step11", -0.708f }, { "lfo7_step12", -0.924f },
            { "lfo7_step14", -0.924f }, { "lfo7_step15", -0.708f }, { "lfo7_step16", -0.382f }, { "lfo7_step2", 0.382f },
            { "lfo7_step3", 0.708f }, { "lfo7_step4", 0.924f }, { "lfo7_step6", 0.924f }, { "lfo7_step7", 0.708f },
            { "lfo7_step8", 0.382f }, { "lfo8_rate", 4.009f }, { "lfo8_step10", -0.382f }, { "lfo8_step11", -0.708f },
            { "lfo8_step12", -0.924f }, { "lfo8_step14", -0.924f }, { "lfo8_step15", -0.708f }, { "lfo8_step16", -0.382f },
            { "lfo8_step2", 0.382f }, { "lfo8_step3", 0.708f }, { "lfo8_step4", 0.924f }, { "lfo8_step6", 0.924f },
            { "lfo8_step7", 0.708f }, { "lfo8_step8", 0.382f }, { "lfo9_rate", 4.009f }, { "lfo9_step10", -0.382f },
            { "lfo9_step11", -0.708f }, { "lfo9_step12", -0.924f }, { "lfo9_step14", -0.924f }, { "lfo9_step15", -0.708f },
            { "lfo9_step16", -0.382f }, { "lfo9_step2", 0.382f }, { "lfo9_step3", 0.708f }, { "lfo9_step4", 0.924f },
            { "lfo9_step6", 0.924f }, { "lfo9_step7", 0.708f }, { "lfo9_step8", 0.382f }, { "me_attack", 0.05099f },
            { "me_decay", 0.399605f }, { "me_release", 0.30485f }, { "mod1_amt", 0.5f }, { "mod1_dst", 54.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 9.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f },
            { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f }, { "osc1_frame", 0.2f },
            { "osc1_grain_size", 80.07f }, { "osc1_level", 0.6f }, { "osc1_table", 10.0f }, { "osc2_fine", 0.0f },
            { "osc2_grain_size", 80.07f }, { "osc2_level", 0.4f }, { "osc2_on", 1.0f }, { "osc2_semi", 12.0f },
            { "osc2_table", 9.0f }, { "osc4_grain_size", 80.07f }, { "osc5_grain_size", 80.07f }, { "osc6_grain_size", 80.07f },
            { "poly_voices", 1.0f }, { "sub_grain_size", 80.07f }, { "sub_shape", 2.0f }, { "subosc_on", 1.0f },
            { "voice_mode", 2.0f }, { "master", -0.5f },
        };
        p.macroNames = { "ZAP", "CUTOFF", "CHORUS", "GLUE" };
        add (p);
    }
    // a struck west-coast gate, the probability sequencer adding the odd octave (from "caribbean keys")
    {
        FactoryPreset p { "Drop Pluck", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.0178825f }, { "amp_decay", 0.275633f }, { "amp_release", 0.546537f }, { "amp_sustain", 0.218845f },
            { "body_coupling_mode", 1.0f }, { "body_type", 1.0f }, { "f1_cutoff", 1021.49f }, { "f1_env", 5.0f },
            { "f1_keytrack", 1.0f }, { "f1_reso", 0.328883f }, { "f2_cutoff", 20.0f }, { "f2_keytrack", 1.0f },
            { "f2_reso", 1.0f }, { "f2_type", 8.0f }, { "fe_dcurve", -1.0f }, { "fe_decay", 0.902634f },
            { "fe_release", 0.005f }, { "fe_sustain", 0.0f }, { "fx_chorus_on", 1.0f }, { "fx_delay_on", 1.0f },
            { "fx_delay_time", 17.7332f }, { "fx_eq_low_freq", 77.3628f }, { "fx_eq_low_gain", -9.28034f }, { "fx_eq_mid_freq", 2868.41f },
            { "fx_eq_mid_gain", 10.7732f }, { "fx_ott_amount", 0.595381f }, { "fx_reverb_on", 1.0f }, { "fx_reverb_type", 2.0f },
            { "fx_slot1", 7.0f }, { "fx_slot2", 9.0f }, { "fx_slot3", 20.0f }, { "fx_slot4", 29.0f },
            { "fx_slot5", 13.0f }, { "master_clip", 0.0f }, { "mech_pedal", 1.0f }, { "mod1_amt", -0.482f },
            { "mod1_dst", 925.0f }, { "mod1_src", 6.0f }, { "mod2_amt", 0.722f }, { "mod2_dst", 3.0f },
            { "mod2_src", 6.0f }, { "mod3_amt", 0.956f }, { "mod3_dst", 2.0f }, { "mod3_src", 6.0f },
            { "mod4_amt", 0.6f }, { "mod4_dst", 414.0f }, { "mod4_src", 12.0f }, { "mod5_amt", 0.5f },
            { "mod5_dst", 416.0f }, { "mod5_src", 13.0f }, { "mod6_amt", 0.5f }, { "mod6_dst", 9.0f },
            { "mod6_src", 14.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 103.0f }, { "mod7_src", 15.0f },
            { "osc1_detune", 0.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 0.0f },
            { "osc1_sample_fade_in", 0.437644f }, { "osc1_sample_start", 1.0f }, { "osc1_table", 60.0f }, { "osc1_uni_mode", 3.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "pedal_res", 0.430505f }, { "pseq_on", 1.0f }, { "res_amount", 0.776179f }, { "res_on", 1.0f },
            { "sb_mix", 0.0f }, { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.7323f },
            { "sub_fb_type", 1.0f }, { "subosc_level", 0.770852f }, { "subosc_on", 1.0f }, { "voice_spread", 0.382004f },
            { "west_decay", 0.3619244f }, { "west_fold", 0.233059f }, { "west_on", 1.0f }, { "west_open", 0.728625f },
            { "west_res", 0.774146f }, { "west_stages", 4.0f }, { "west_strike", 0.96431f }, { "master", -9.53f },
        };
        p.macroNames = { "FOLD", "DECAY", "TONE", "SPACE" };
        add (p);
    }
    // her piano's string struck in a euclidean rhythm by a mallet (from "real piano SOUND")
    {
        FactoryPreset p { "Euclid Mallets", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 1.0f }, { "euc_hits", 6.0f }, { "euc_on", 1.0f },
            { "euc_steps", 14.0f }, { "euc_target", 1.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.4f }, { "mod1_dst", 9.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 396.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 185.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f },
            { "mod4_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f },
            { "osc1_register", 0.15848f }, { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 0.6120493f }, { "osc1_string_excite_pos", 0.739229f },
            { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "res_amount", 0.607374f }, { "res_on", 1.0f }, { "sb_mix", 0.355458f }, { "sb_model", 1.0f },
            { "sb_on", 1.0f }, { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f },
            { "subosc_on", 1.0f }, { "voice_spread", 0.287019f }, { "west_fold", 0.432153f }, { "west_on", 1.0f },
            { "west_open", 0.712386f }, { "master", -2.79f },
        };
        p.macroNames = { "BRIGHT", "BODY", "DAMP", "SPACE" };
        add (p);
    }
    // her piano with a soft felt hammer, a shell body and sympathetic strings (from "real piano SOUND")
    {
        FactoryPreset p { "Felt Upright", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 4.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.6f }, { "mod1_dst", 253.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 185.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 982.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f },
            { "mod4_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.6298301f }, { "osc1_excite", 9.0f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 0.3589413f }, { "osc1_level", 1.0f },
            { "osc1_mode", 1.0f }, { "osc1_register", 0.15848f }, { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 1.0f },
            { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f },
            { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f }, { "sb_mix", 0.4113468f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f },
            { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "sym_amount", 0.39057f }, { "sym_decay", 0.5006318f },
            { "sym_on", 1.0f }, { "voice_spread", 0.287019f }, { "west_fold", 0.432153f }, { "west_open", 0.712386f },
            { "master", 2.18f },
        };
        p.macroNames = { "HAMMER", "DAMPER", "SYMPATHY", "ROOM" };
        add (p);
    }
    // her FM arp walking instead of running, in Airwindows space (from "fm arp")
    {
        FactoryPreset p { "FM Walk", {} };
        p.category = "Generative";
        p.values = {
            { "amp_attack", 0.005999f }, { "amp_decay", 0.299705f }, { "amp_release", 0.254875f }, { "arp_div", 4.0f },
            { "arp_gate", 0.3046f }, { "arp_mode", 8.0f }, { "arp_octaves", 2.0f }, { "arp_on", 1.0f },
            { "e4_attack", 0.001f }, { "e4_decay", 0.419585f }, { "e4_release", 0.005f }, { "e4_sustain", 0.0f },
            { "env10_attack", 0.05099f }, { "env10_decay", 0.399605f }, { "env10_release", 0.30485f }, { "env11_attack", 0.05099f },
            { "env11_decay", 0.399605f }, { "env11_release", 0.30485f }, { "env12_attack", 0.05099f }, { "env12_decay", 0.399605f },
            { "env12_release", 0.30485f }, { "env13_attack", 0.05099f }, { "env13_decay", 0.399605f }, { "env13_release", 0.30485f },
            { "env14_attack", 0.05099f }, { "env14_decay", 0.399605f }, { "env14_release", 0.30485f }, { "env15_attack", 0.05099f },
            { "env15_decay", 0.399605f }, { "env15_release", 0.30485f }, { "env16_attack", 0.05099f }, { "env16_decay", 0.399605f },
            { "env16_release", 0.30485f }, { "env6_attack", 0.001f }, { "env6_curve", -0.636f }, { "env6_decay", 0.06494f },
            { "env6_release", 0.18491f }, { "env6_sustain", 0.0f }, { "env7_attack", 0.05099f }, { "env7_decay", 0.399605f },
            { "env7_release", 0.30485f }, { "env8_attack", 0.05099f }, { "env8_decay", 0.399605f }, { "env8_release", 0.30485f },
            { "env9_attack", 0.05099f }, { "env9_decay", 0.399605f }, { "env9_release", 0.30485f }, { "euc_gate", 0.5003f },
            { "f1_cutoff", 12008.0f }, { "f1_drive", 3.475f }, { "f1_reso", 0.0f }, { "f1_type", 4.0f },
            { "f2e_attack", 0.010998f }, { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f }, { "fe_decay", 0.499505f },
            { "fe_release", 0.30485f }, { "fm_3to2", 0.464f }, { "fm_amount", 1.0f }, { "fm_fb3", 1.0f },
            { "fx_amp_drive", 4.002f }, { "fx_chorus_rate", 0.7973f }, { "fx_comb_feedback", 0.60043f }, { "fx_comb_freq", 219.98f },
            { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f }, { "fx_comp_release", 149.59f }, { "fx_delay_div", 9.0f },
            { "fx_delay_duck", 0.201f }, { "fx_delay_feedback", 0.3496f }, { "fx_delay_mix", 0.239f }, { "fx_delay_on", 1.0f },
            { "fx_delay_sync", 1.0f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f }, { "fx_dim_rate", 0.40155f },
            { "fx_drive_amount", 2.995f }, { "fx_eq_high_freq", 5998.0f }, { "fx_eq_low_freq", 119.84f }, { "fx_eq_mid_freq", 1004.4f },
            { "fx_eq_mid_q", 0.902f }, { "fx_feedback_amount", 0.6004f }, { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f },
            { "fx_flanger_rate", 0.50315f }, { "fx_gate_pattern", 8.0f }, { "fx_gate_step10", 0.0f }, { "fx_gate_step12", 0.0f },
            { "fx_gate_step14", 0.0f }, { "fx_gate_step16", 0.0f }, { "fx_gate_step2", 0.0f }, { "fx_gate_step4", 0.0f },
            { "fx_gate_step6", 0.0f }, { "fx_gate_step8", 0.0f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f },
            { "fx_limit_release", 119.89f }, { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_reverb_mix", 0.27f },
            { "fx_reverb_on", 1.0f }, { "fx_ring_freq", 220.956f }, { "fx_slot1", 9.0f }, { "fx_slot2", 34.0f },
            { "fx_slot2_mix", 0.2036327f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f }, { "fx_tape_stop_time", 0.59905f },
            { "fx_trem_rate", 4.0001f }, { "lfo10_rate", 4.009f }, { "lfo10_step10", -0.382f }, { "lfo10_step11", -0.708f },
            { "lfo10_step12", -0.924f }, { "lfo10_step14", -0.924f }, { "lfo10_step15", -0.708f }, { "lfo10_step16", -0.382f },
            { "lfo10_step2", 0.382f }, { "lfo10_step3", 0.708f }, { "lfo10_step4", 0.924f }, { "lfo10_step6", 0.924f },
            { "lfo10_step7", 0.708f }, { "lfo10_step8", 0.382f }, { "lfo11_rate", 4.009f }, { "lfo11_step10", -0.382f },
            { "lfo11_step11", -0.708f }, { "lfo11_step12", -0.924f }, { "lfo11_step14", -0.924f }, { "lfo11_step15", -0.708f },
            { "lfo11_step16", -0.382f }, { "lfo11_step2", 0.382f }, { "lfo11_step3", 0.708f }, { "lfo11_step4", 0.924f },
            { "lfo11_step6", 0.924f }, { "lfo11_step7", 0.708f }, { "lfo11_step8", 0.382f }, { "lfo12_rate", 4.009f },
            { "lfo12_step10", -0.382f }, { "lfo12_step11", -0.708f }, { "lfo12_step12", -0.924f }, { "lfo12_step14", -0.924f },
            { "lfo12_step15", -0.708f }, { "lfo12_step16", -0.382f }, { "lfo12_step2", 0.382f }, { "lfo12_step3", 0.708f },
            { "lfo12_step4", 0.924f }, { "lfo12_step6", 0.924f }, { "lfo12_step7", 0.708f }, { "lfo12_step8", 0.382f },
            { "lfo13_rate", 4.009f }, { "lfo13_step10", -0.382f }, { "lfo13_step11", -0.708f }, { "lfo13_step12", -0.924f },
            { "lfo13_step14", -0.924f }, { "lfo13_step15", -0.708f }, { "lfo13_step16", -0.382f }, { "lfo13_step2", 0.382f },
            { "lfo13_step3", 0.708f }, { "lfo13_step4", 0.924f }, { "lfo13_step6", 0.924f }, { "lfo13_step7", 0.708f },
            { "lfo13_step8", 0.382f }, { "lfo14_rate", 4.009f }, { "lfo14_step10", -0.382f }, { "lfo14_step11", -0.708f },
            { "lfo14_step12", -0.924f }, { "lfo14_step14", -0.924f }, { "lfo14_step15", -0.708f }, { "lfo14_step16", -0.382f },
            { "lfo14_step2", 0.382f }, { "lfo14_step3", 0.708f }, { "lfo14_step4", 0.924f }, { "lfo14_step6", 0.924f },
            { "lfo14_step7", 0.708f }, { "lfo14_step8", 0.382f }, { "lfo15_rate", 4.009f }, { "lfo15_step10", -0.382f },
            { "lfo15_step11", -0.708f }, { "lfo15_step12", -0.924f }, { "lfo15_step14", -0.924f }, { "lfo15_step15", -0.708f },
            { "lfo15_step16", -0.382f }, { "lfo15_step2", 0.382f }, { "lfo15_step3", 0.708f }, { "lfo15_step4", 0.924f },
            { "lfo15_step6", 0.924f }, { "lfo15_step7", 0.708f }, { "lfo15_step8", 0.382f }, { "lfo16_rate", 4.009f },
            { "lfo16_step10", -0.382f }, { "lfo16_step11", -0.708f }, { "lfo16_step12", -0.924f }, { "lfo16_step14", -0.924f },
            { "lfo16_step15", -0.708f }, { "lfo16_step16", -0.382f }, { "lfo16_step2", 0.382f }, { "lfo16_step3", 0.708f },
            { "lfo16_step4", 0.924f }, { "lfo16_step6", 0.924f }, { "lfo16_step7", 0.708f }, { "lfo16_step8", 0.382f },
            { "lfo1_rate", 4.009f }, { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f },
            { "lfo1_step14", -0.924f }, { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f },
            { "lfo1_step3", 0.708f }, { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f },
            { "lfo1_step8", 0.382f }, { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f },
            { "lfo2_step12", -0.924f }, { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f },
            { "lfo2_step2", 0.382f }, { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f },
            { "lfo2_step7", 0.708f }, { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f },
            { "lfo3_step11", -0.708f }, { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f },
            { "lfo3_step16", -0.382f }, { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f },
            { "lfo3_step6", 0.924f }, { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f },
            { "lfo4_step10", -0.382f }, { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f },
            { "lfo4_step15", -0.708f }, { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f },
            { "lfo4_step4", 0.924f }, { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f },
            { "lfo5_rate", 4.009f }, { "lfo5_step10", -0.382f }, { "lfo5_step11", -0.708f }, { "lfo5_step12", -0.924f },
            { "lfo5_step14", -0.924f }, { "lfo5_step15", -0.708f }, { "lfo5_step16", -0.382f }, { "lfo5_step2", 0.382f },
            { "lfo5_step3", 0.708f }, { "lfo5_step4", 0.924f }, { "lfo5_step6", 0.924f }, { "lfo5_step7", 0.708f },
            { "lfo5_step8", 0.382f }, { "lfo6_rate", 4.009f }, { "lfo6_step10", -0.382f }, { "lfo6_step11", -0.708f },
            { "lfo6_step12", -0.924f }, { "lfo6_step14", -0.924f }, { "lfo6_step15", -0.708f }, { "lfo6_step16", -0.382f },
            { "lfo6_step2", 0.382f }, { "lfo6_step3", 0.708f }, { "lfo6_step4", 0.924f }, { "lfo6_step6", 0.924f },
            { "lfo6_step7", 0.708f }, { "lfo6_step8", 0.382f }, { "lfo7_rate", 4.009f }, { "lfo7_step10", -0.382f },
            { "lfo7_step11", -0.708f }, { "lfo7_step12", -0.924f }, { "lfo7_step14", -0.924f }, { "lfo7_step15", -0.708f },
            { "lfo7_step16", -0.382f }, { "lfo7_step2", 0.382f }, { "lfo7_step3", 0.708f }, { "lfo7_step4", 0.924f },
            { "lfo7_step6", 0.924f }, { "lfo7_step7", 0.708f }, { "lfo7_step8", 0.382f }, { "lfo8_rate", 4.009f },
            { "lfo8_step10", -0.382f }, { "lfo8_step11", -0.708f }, { "lfo8_step12", -0.924f }, { "lfo8_step14", -0.924f },
            { "lfo8_step15", -0.708f }, { "lfo8_step16", -0.382f }, { "lfo8_step2", 0.382f }, { "lfo8_step3", 0.708f },
            { "lfo8_step4", 0.924f }, { "lfo8_step6", 0.924f }, { "lfo8_step7", 0.708f }, { "lfo8_step8", 0.382f },
            { "lfo9_rate", 4.009f }, { "lfo9_step10", -0.382f }, { "lfo9_step11", -0.708f }, { "lfo9_step12", -0.924f },
            { "lfo9_step14", -0.924f }, { "lfo9_step15", -0.708f }, { "lfo9_step16", -0.382f }, { "lfo9_step2", 0.382f },
            { "lfo9_step3", 0.708f }, { "lfo9_step4", 0.924f }, { "lfo9_step6", 0.924f }, { "lfo9_step7", 0.708f },
            { "lfo9_step8", 0.382f }, { "master_clip", 0.0f }, { "me_attack", 0.05099f }, { "me_decay", 0.399605f },
            { "me_release", 0.30485f }, { "mod1_amt", 0.5f }, { "mod1_dst", 57.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.4f }, { "mod2_dst", 9.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f },
            { "mod4_src", 15.0f }, { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f }, { "mseg_time2", 0.25045f },
            { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f },
            { "osc1_grain_size", 80.07f }, { "osc1_table", 8.0f }, { "osc2_amp_env", 5.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_fine", 0.0f }, { "osc2_grain_size", 80.07f }, { "osc2_level", 1.0f }, { "osc2_on", 1.0f },
            { "osc2_out", 0.0f }, { "osc4_grain_size", 80.07f }, { "osc5_grain_size", 80.07f }, { "osc6_grain_size", 80.07f },
            { "poly_voices", 16.0f }, { "pseq_gate", 0.5003f }, { "spray_strum_time", 30.024f }, { "sub_amp_env", 4.0f },
            { "sub_detune", 0.0f }, { "sub_fb_type", 1.0f }, { "sub_grain_size", 80.07f }, { "sub_on", 1.0f },
            { "sub_shape", 1.0f }, { "sub_table", 8.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f },
            { "master", -9.87f },
        };
        p.macroNames = { "FM", "TONE", "ECHO", "SPACE" };
        add (p);
    }
    // her brass folded slowly by an LFO under a long space (from "classic synth brass, no fx")
    {
        FactoryPreset p { "Fold Drone", {} };
        p.category = "Drone";
        p.values = {
            { "amp_attack", 1.91304f }, { "amp_release", 3.183067f }, { "amp_sustain", 1.0f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.244851f }, { "body_type", 2.0f }, { "drift", 0.293017f }, { "f1_cutoff", 176.638f },
            { "f1_drive", 2.00576f }, { "f1_env", 5.0f }, { "f1_type", 29.0f }, { "fe_attack", 0.108571f },
            { "fx_awverb_algo", 2.0f }, { "fx_awverb_mix", 0.24538f }, { "fx_awverb_p1", 1.0f }, { "fx_slot1", 15.0f },
            { "fx_slot2", 34.0f }, { "fx_slot2_mix", 0.5722096f }, { "fx_slot3", 20.0f }, { "lfo1_rate", 0.07364897f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.183048f }, { "mod1_dst", 9.0f }, { "mod1_src", 6.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 414.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 417.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 9.0f },
            { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 100.0f }, { "mod5_src", 15.0f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 1.0f }, { "osc1_table", 0.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "poly_voices", 16.0f }, { "res_amount", 0.445812f }, { "res_on", 1.0f }, { "sb_model", 1.0f },
            { "sb_on", 1.0f }, { "sb_size", 0.740836f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "voice_spread", 0.523819f }, { "west_fold", 0.277923f },
            { "west_on", 1.0f }, { "west_open", 0.365268f }, { "west_res", 0.322425f }, { "west_src", 4.0f },
            { "west_stages", 3.0f }, { "west_strike", 0.564488f }, { "master", -4.83f },
        };
        p.macroNames = { "FOLD", "GLOW", "TONE", "SPACE" };
        add (p);
    }
    // her hammered strings held in a smear and a long Airwindows space (from "Hammered Strings Edited")
    {
        FactoryPreset p { "Frozen Hammers", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.1598909f }, { "amp_decay", 5.0f }, { "amp_release", 1.813207f }, { "amp_sustain", 1.0f },
            { "amp_velocity", 0.974f }, { "f1_cutoff", 18000.0f }, { "f1_drive", 1.0f }, { "f1_env", 0.0f },
            { "f1_reso", 0.0f }, { "fx_limit_ceiling", -0.5f }, { "fx_reverb_mix", 0.16f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_size", 0.55f }, { "fx_slot1", 11.0f }, { "fx_slot1_mix", 0.3068387f }, { "fx_slot2", 34.0f },
            { "fx_slot2_mix", 0.4268472f }, { "mech_damper", 0.25f }, { "mech_key", 0.25f }, { "mech_pedal", 0.25f },
            { "mod1_amt", 0.5f }, { "mod1_dst", 99.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 253.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 335.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f }, { "mod4_src", 15.0f },
            { "osc1_couple", 0.11225f }, { "osc1_damper", 0.0902858f }, { "osc1_detune", 0.9f }, { "osc1_excite", 9.0f },
            { "osc1_hammer_hard", 0.739f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.0526428f },
            { "osc1_spread", 0.35f }, { "osc1_string_damp", 0.352f }, { "osc1_string_decay", 1.0f }, { "osc1_string_excite_pos", 0.221714f },
            { "osc1_string_stiffness", 0.45f }, { "osc1_string_sustain", 0.605714f }, { "osc1_unison", 10.0f }, { "osc2_couple", 0.0289498f },
            { "osc2_excite", 5.0f }, { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_semi", 12.0f },
            { "osc2_string_damp", 0.866571f }, { "osc2_string_pickup", 0.278906f }, { "osc2_string_slap", 1.0f }, { "osc2_unison", 4.0f },
            { "pedal_res", 0.5f }, { "sb_mix", 0.786f }, { "sb_on", 1.0f }, { "sb_size", 0.751f },
            { "sb_tone", 0.0965f }, { "stretch", 0.8f }, { "master", 12.0f },
        };
        p.macroNames = { "SMEAR", "HAMMER", "BOARD", "SPACE" };
        add (p);
    }
    // her arped bass's chord stack an octave up, gated instead of arpeggiated (from "ARPed pluck Bass")
    {
        FactoryPreset p { "Gate Stab Chords", {} };
        p.category = "Chords";
        p.values = {
            { "amp_attack", 0.005999f }, { "amp_decay", 0.299705f }, { "amp_release", 0.254875f }, { "arp_gate", 0.05f },
            { "e4_attack", 0.05099f }, { "e4_decay", 0.399605f }, { "e4_release", 0.30485f }, { "f1_cutoff", 2008.21f },
            { "f1_drive", 6.652f }, { "f1_fm", 0.394f }, { "f1_reso", 0.51548f }, { "f1_slope", 1.0f },
            { "f2_cutoff", 4735.266f }, { "f2_drive", 5.95f }, { "f2_reso", 0.59486f }, { "f2_slope", 1.0f },
            { "f2_type", 3.0f }, { "f2e_attack", 0.010998f }, { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f },
            { "fe_decay", 0.499505f }, { "fe_release", 0.30485f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_rate", 0.7973f },
            { "fx_comb_feedback", 0.60043f }, { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f },
            { "fx_comp_release", 149.59f }, { "fx_delay_feedback", 0.3496f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f },
            { "fx_dim_rate", 0.40155f }, { "fx_drive_amount", 4.002f }, { "fx_drive_mix", 0.5f }, { "fx_feedback_amount", 0.6004f },
            { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f }, { "fx_flanger_rate", 0.50315f }, { "fx_gate_mix", 0.6008988f },
            { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f }, { "fx_phaser_feedback", 0.3996f },
            { "fx_phaser_rate", 0.50315f }, { "fx_ring_freq", 220.956f }, { "fx_slot1", 20.0f }, { "fx_slot2", 2.0f },
            { "fx_slot3", 16.0f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f }, { "fx_tape_stop_time", 0.59905f },
            { "fx_trem_rate", 4.0001f }, { "lfo1_div", 0.0f }, { "lfo1_rate", 4.009f }, { "lfo1_retrig", 1.0f },
            { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f },
            { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f },
            { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f },
            { "lfo1_sync", 1.0f }, { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f },
            { "lfo2_step12", -0.924f }, { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f },
            { "lfo2_step2", 0.382f }, { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f },
            { "lfo2_step7", 0.708f }, { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f },
            { "lfo3_step11", -0.708f }, { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f },
            { "lfo3_step16", -0.382f }, { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f },
            { "lfo3_step6", 0.924f }, { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f },
            { "lfo4_step10", -0.382f }, { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f },
            { "lfo4_step15", -0.708f }, { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f },
            { "lfo4_step4", 0.924f }, { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f },
            { "master_clip_gain", 2.5f }, { "me_attack", 0.05099f }, { "me_decay", 0.399605f }, { "me_release", 0.30485f },
            { "mod10_amt", 0.5f }, { "mod10_dst", 100.0f }, { "mod10_src", 15.0f }, { "mod1_amt", 1.0f },
            { "mod1_dst", 2.0f }, { "mod1_src", 1.0f }, { "mod2_amt", -0.146f }, { "mod2_dst", 9.0f },
            { "mod2_src", 1.0f }, { "mod3_src", 1.0f }, { "mod4_amt", -0.308f }, { "mod4_dst", 11.0f },
            { "mod4_src", 1.0f }, { "mod5_amt", 0.35f }, { "mod5_dst", 5.0f }, { "mod5_src", 1.0f },
            { "mod6_amt", 0.35f }, { "mod6_dst", 29.0f }, { "mod6_src", 1.0f }, { "mod7_amt", -0.8f },
            { "mod7_dst", 154.0f }, { "mod7_src", 12.0f }, { "mod8_amt", 0.5f }, { "mod8_dst", 9.0f },
            { "mod8_src", 13.0f }, { "mod9_amt", 0.5f }, { "mod9_dst", 99.0f }, { "mod9_src", 14.0f },
            { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f }, { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f },
            { "mseg_time4", 0.25045f }, { "noise_level", 0.296f }, { "osc1_chord", 2.0f }, { "osc1_frame", 0.488f },
            { "osc1_semi", 12.0f }, { "osc1_table", 3.0f }, { "osc1_unison", 3.0f }, { "osc2_chord", 3.0f },
            { "osc2_detune", 10.0f }, { "osc2_fine", 0.0f }, { "osc2_frame", 0.213f }, { "osc2_level", 0.5f },
            { "osc2_on", 1.0f }, { "osc2_semi", 12.0f }, { "osc2_spread", 0.704f }, { "osc2_table", 5.0f },
            { "osc2_unison", 4.0f }, { "sub_chord", 3.0f }, { "sub_detune", 0.0f }, { "sub_frame", 0.011f },
            { "sub_level", 0.4f }, { "sub_on", 1.0f }, { "sub_semi", 24.0f }, { "sub_shape", 1.0f },
            { "sub_spread", 0.522f }, { "sub_table", 17.0f }, { "sub_unison", 5.0f }, { "unison_random", 0.136f },
            { "voice_spread", 0.607f }, { "master", -14.85f },
        };
        p.macroNames = { "CHOP", "CUTOFF", "SQUASH", "DRIVE" };
        add (p);
    }
    // her FM arp held as chords and cut by a trance gate (from "fm arp")
    {
        FactoryPreset p { "Gated Glass Pluck", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.005999f }, { "amp_decay", 0.299705f }, { "amp_release", 0.250044f }, { "arp_div", 4.0f },
            { "arp_gate", 0.3046f }, { "e4_attack", 0.001f }, { "e4_decay", 0.419585f }, { "e4_release", 0.005f },
            { "e4_sustain", 0.0f }, { "env10_attack", 0.05099f }, { "env10_decay", 0.399605f }, { "env10_release", 0.30485f },
            { "env11_attack", 0.05099f }, { "env11_decay", 0.399605f }, { "env11_release", 0.30485f }, { "env12_attack", 0.05099f },
            { "env12_decay", 0.399605f }, { "env12_release", 0.30485f }, { "env13_attack", 0.05099f }, { "env13_decay", 0.399605f },
            { "env13_release", 0.30485f }, { "env14_attack", 0.05099f }, { "env14_decay", 0.399605f }, { "env14_release", 0.30485f },
            { "env15_attack", 0.05099f }, { "env15_decay", 0.399605f }, { "env15_release", 0.30485f }, { "env16_attack", 0.05099f },
            { "env16_decay", 0.399605f }, { "env16_release", 0.30485f }, { "env6_attack", 0.001f }, { "env6_curve", -0.636f },
            { "env6_decay", 0.06494f }, { "env6_release", 0.18491f }, { "env6_sustain", 0.0f }, { "env7_attack", 0.05099f },
            { "env7_decay", 0.399605f }, { "env7_release", 0.30485f }, { "env8_attack", 0.05099f }, { "env8_decay", 0.399605f },
            { "env8_release", 0.30485f }, { "env9_attack", 0.05099f }, { "env9_decay", 0.399605f }, { "env9_release", 0.30485f },
            { "euc_gate", 0.5003f }, { "f1_cutoff", 12008.0f }, { "f1_drive", 3.475f }, { "f1_reso", 0.0f },
            { "f1_type", 4.0f }, { "f2e_attack", 0.010998f }, { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f },
            { "fe_decay", 0.499505f }, { "fe_release", 0.30485f }, { "fm_3to2", 0.464f }, { "fm_amount", 1.0f },
            { "fm_fb3", 1.0f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_rate", 0.7973f }, { "fx_comb_feedback", 0.60043f },
            { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f }, { "fx_comp_release", 149.59f },
            { "fx_delay_div", 9.0f }, { "fx_delay_duck", 0.201f }, { "fx_delay_feedback", 0.3496f }, { "fx_delay_mix", 0.239f },
            { "fx_delay_on", 1.0f }, { "fx_delay_sync", 1.0f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f },
            { "fx_dim_rate", 0.40155f }, { "fx_drive_amount", 2.995f }, { "fx_eq_high_freq", 5998.0f }, { "fx_eq_low_freq", 119.84f },
            { "fx_eq_mid_freq", 1004.4f }, { "fx_eq_mid_q", 0.902f }, { "fx_feedback_amount", 0.6004f }, { "fx_feedback_delay", 11.989f },
            { "fx_flanger_feedback", 0.5004f }, { "fx_flanger_rate", 0.50315f }, { "fx_gate_mix", 0.7340085f }, { "fx_gate_pattern", 8.0f },
            { "fx_gate_smooth", 0.1360343f }, { "fx_gate_step10", 0.0f }, { "fx_gate_step12", 0.0f }, { "fx_gate_step14", 0.0f },
            { "fx_gate_step16", 0.0f }, { "fx_gate_step2", 0.0f }, { "fx_gate_step4", 0.0f }, { "fx_gate_step6", 0.0f },
            { "fx_gate_step8", 0.0f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f },
            { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_reverb_mix", 0.27f }, { "fx_reverb_on", 1.0f },
            { "fx_ring_freq", 220.956f }, { "fx_slot1", 16.0f }, { "fx_slot2", 34.0f }, { "fx_slot2_mix", 0.2523114f },
            { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f }, { "fx_tape_stop_time", 0.59905f }, { "fx_trem_rate", 4.0001f },
            { "lfo10_rate", 4.009f }, { "lfo10_step10", -0.382f }, { "lfo10_step11", -0.708f }, { "lfo10_step12", -0.924f },
            { "lfo10_step14", -0.924f }, { "lfo10_step15", -0.708f }, { "lfo10_step16", -0.382f }, { "lfo10_step2", 0.382f },
            { "lfo10_step3", 0.708f }, { "lfo10_step4", 0.924f }, { "lfo10_step6", 0.924f }, { "lfo10_step7", 0.708f },
            { "lfo10_step8", 0.382f }, { "lfo11_rate", 4.009f }, { "lfo11_step10", -0.382f }, { "lfo11_step11", -0.708f },
            { "lfo11_step12", -0.924f }, { "lfo11_step14", -0.924f }, { "lfo11_step15", -0.708f }, { "lfo11_step16", -0.382f },
            { "lfo11_step2", 0.382f }, { "lfo11_step3", 0.708f }, { "lfo11_step4", 0.924f }, { "lfo11_step6", 0.924f },
            { "lfo11_step7", 0.708f }, { "lfo11_step8", 0.382f }, { "lfo12_rate", 4.009f }, { "lfo12_step10", -0.382f },
            { "lfo12_step11", -0.708f }, { "lfo12_step12", -0.924f }, { "lfo12_step14", -0.924f }, { "lfo12_step15", -0.708f },
            { "lfo12_step16", -0.382f }, { "lfo12_step2", 0.382f }, { "lfo12_step3", 0.708f }, { "lfo12_step4", 0.924f },
            { "lfo12_step6", 0.924f }, { "lfo12_step7", 0.708f }, { "lfo12_step8", 0.382f }, { "lfo13_rate", 4.009f },
            { "lfo13_step10", -0.382f }, { "lfo13_step11", -0.708f }, { "lfo13_step12", -0.924f }, { "lfo13_step14", -0.924f },
            { "lfo13_step15", -0.708f }, { "lfo13_step16", -0.382f }, { "lfo13_step2", 0.382f }, { "lfo13_step3", 0.708f },
            { "lfo13_step4", 0.924f }, { "lfo13_step6", 0.924f }, { "lfo13_step7", 0.708f }, { "lfo13_step8", 0.382f },
            { "lfo14_rate", 4.009f }, { "lfo14_step10", -0.382f }, { "lfo14_step11", -0.708f }, { "lfo14_step12", -0.924f },
            { "lfo14_step14", -0.924f }, { "lfo14_step15", -0.708f }, { "lfo14_step16", -0.382f }, { "lfo14_step2", 0.382f },
            { "lfo14_step3", 0.708f }, { "lfo14_step4", 0.924f }, { "lfo14_step6", 0.924f }, { "lfo14_step7", 0.708f },
            { "lfo14_step8", 0.382f }, { "lfo15_rate", 4.009f }, { "lfo15_step10", -0.382f }, { "lfo15_step11", -0.708f },
            { "lfo15_step12", -0.924f }, { "lfo15_step14", -0.924f }, { "lfo15_step15", -0.708f }, { "lfo15_step16", -0.382f },
            { "lfo15_step2", 0.382f }, { "lfo15_step3", 0.708f }, { "lfo15_step4", 0.924f }, { "lfo15_step6", 0.924f },
            { "lfo15_step7", 0.708f }, { "lfo15_step8", 0.382f }, { "lfo16_rate", 4.009f }, { "lfo16_step10", -0.382f },
            { "lfo16_step11", -0.708f }, { "lfo16_step12", -0.924f }, { "lfo16_step14", -0.924f }, { "lfo16_step15", -0.708f },
            { "lfo16_step16", -0.382f }, { "lfo16_step2", 0.382f }, { "lfo16_step3", 0.708f }, { "lfo16_step4", 0.924f },
            { "lfo16_step6", 0.924f }, { "lfo16_step7", 0.708f }, { "lfo16_step8", 0.382f }, { "lfo1_rate", 4.009f },
            { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f },
            { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f },
            { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f },
            { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f },
            { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f },
            { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f },
            { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f },
            { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f },
            { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f },
            { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f },
            { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f },
            { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f },
            { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f }, { "lfo5_rate", 4.009f },
            { "lfo5_step10", -0.382f }, { "lfo5_step11", -0.708f }, { "lfo5_step12", -0.924f }, { "lfo5_step14", -0.924f },
            { "lfo5_step15", -0.708f }, { "lfo5_step16", -0.382f }, { "lfo5_step2", 0.382f }, { "lfo5_step3", 0.708f },
            { "lfo5_step4", 0.924f }, { "lfo5_step6", 0.924f }, { "lfo5_step7", 0.708f }, { "lfo5_step8", 0.382f },
            { "lfo6_rate", 4.009f }, { "lfo6_step10", -0.382f }, { "lfo6_step11", -0.708f }, { "lfo6_step12", -0.924f },
            { "lfo6_step14", -0.924f }, { "lfo6_step15", -0.708f }, { "lfo6_step16", -0.382f }, { "lfo6_step2", 0.382f },
            { "lfo6_step3", 0.708f }, { "lfo6_step4", 0.924f }, { "lfo6_step6", 0.924f }, { "lfo6_step7", 0.708f },
            { "lfo6_step8", 0.382f }, { "lfo7_rate", 4.009f }, { "lfo7_step10", -0.382f }, { "lfo7_step11", -0.708f },
            { "lfo7_step12", -0.924f }, { "lfo7_step14", -0.924f }, { "lfo7_step15", -0.708f }, { "lfo7_step16", -0.382f },
            { "lfo7_step2", 0.382f }, { "lfo7_step3", 0.708f }, { "lfo7_step4", 0.924f }, { "lfo7_step6", 0.924f },
            { "lfo7_step7", 0.708f }, { "lfo7_step8", 0.382f }, { "lfo8_rate", 4.009f }, { "lfo8_step10", -0.382f },
            { "lfo8_step11", -0.708f }, { "lfo8_step12", -0.924f }, { "lfo8_step14", -0.924f }, { "lfo8_step15", -0.708f },
            { "lfo8_step16", -0.382f }, { "lfo8_step2", 0.382f }, { "lfo8_step3", 0.708f }, { "lfo8_step4", 0.924f },
            { "lfo8_step6", 0.924f }, { "lfo8_step7", 0.708f }, { "lfo8_step8", 0.382f }, { "lfo9_rate", 4.009f },
            { "lfo9_step10", -0.382f }, { "lfo9_step11", -0.708f }, { "lfo9_step12", -0.924f }, { "lfo9_step14", -0.924f },
            { "lfo9_step15", -0.708f }, { "lfo9_step16", -0.382f }, { "lfo9_step2", 0.382f }, { "lfo9_step3", 0.708f },
            { "lfo9_step4", 0.924f }, { "lfo9_step6", 0.924f }, { "lfo9_step7", 0.708f }, { "lfo9_step8", 0.382f },
            { "master_clip", 0.0f }, { "me_attack", 0.05099f }, { "me_decay", 0.399605f }, { "me_release", 0.30485f },
            { "mod1_amt", -0.8f }, { "mod1_dst", 154.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 57.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.4f }, { "mod3_dst", 9.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f }, { "mod4_src", 15.0f },
            { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f }, { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f },
            { "mseg_time4", 0.25045f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_grain_size", 80.07f },
            { "osc1_table", 8.0f }, { "osc2_amp_env", 5.0f }, { "osc2_fb_type", 1.0f }, { "osc2_fine", 0.0f },
            { "osc2_grain_size", 80.07f }, { "osc2_level", 1.0f }, { "osc2_on", 1.0f }, { "osc2_out", 0.0f },
            { "osc4_grain_size", 80.07f }, { "osc5_grain_size", 80.07f }, { "osc6_grain_size", 80.07f }, { "poly_voices", 16.0f },
            { "pseq_gate", 0.5003f }, { "spray_strum_time", 30.024f }, { "sub_amp_env", 4.0f }, { "sub_detune", 0.0f },
            { "sub_fb_type", 1.0f }, { "sub_grain_size", 80.07f }, { "sub_on", 1.0f }, { "sub_shape", 1.0f },
            { "sub_table", 8.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "master", -13.62f },
        };
        p.macroNames = { "CHOP", "FM", "TONE", "SPACE" };
        add (p);
    }
    // a stiff, bright hammered string ringing a bell body (from "velocity warp physical pluck")
    {
        FactoryPreset p { "Glass Harp Keys", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 3.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 15.0f },
            { "fx_slot2_mix", 0.437444f }, { "master_clip", 0.0f }, { "mod1_amt", 0.832f }, { "mod1_dst", 414.0f },
            { "mod1_src", 6.0f }, { "mod2_amt", 0.878f }, { "mod2_dst", 5.0f }, { "mod2_src", 6.0f },
            { "mod3_amt", 0.586f }, { "mod3_dst", 6.0f }, { "mod3_src", 6.0f }, { "mod4_amt", 0.494f },
            { "mod4_dst", 43.0f }, { "mod4_src", 6.0f }, { "mod5_amt", 0.4f }, { "mod5_dst", 257.0f },
            { "mod5_src", 12.0f }, { "mod6_amt", -0.5f }, { "mod6_dst", 185.0f }, { "mod6_src", 13.0f },
            { "mod7_amt", 0.5f }, { "mod7_dst", 43.0f }, { "mod7_src", 14.0f }, { "mod8_amt", 0.5f },
            { "mod8_dst", 99.0f }, { "mod8_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f },
            { "osc1_excite", 5.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.7966111f }, { "osc1_string_damp", 0.1310164f },
            { "osc1_string_decay", 1.0f }, { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.5504754f },
            { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f }, { "osc2_level", 0.0f },
            { "osc2_on", 1.0f }, { "osc2_table", 19.0f }, { "osc2_warp", 2.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f },
            { "sb_mix", 0.355458f }, { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.842771f },
            { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f },
            { "west_fold", 0.0f }, { "west_on", 1.0f }, { "west_open", 0.712386f }, { "master", -0.57f },
        };
        p.macroNames = { "GLASS", "RING", "WARP", "SPACE" };
        add (p);
    }
    // her trance pad, polyphonic, through the Airwindows Holt filter swept by an LFO (from "trancey pad")
    {
        FactoryPreset p { "Holt Pad", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.648308f }, { "drift", 0.481143f }, { "f1_cutoff", 426.723f },
            { "f1_drive", 2.13772f }, { "f1_keytrack", 1.0f }, { "f1_morph", 0.156571f }, { "f1_reso", 0.3238555f },
            { "f1_type", 35.0f }, { "fx_awtape_p1", 0.359729f }, { "fx_awverb_mix", 0.180106f }, { "fx_gate_div", 5.0f },
            { "fx_gate_pattern", 8.0f }, { "fx_gate_step10", 0.0f }, { "fx_gate_step12", 0.0f }, { "fx_gate_step14", 0.0f },
            { "fx_gate_step2", 0.0f }, { "fx_gate_step4", 0.0f }, { "fx_gate_step8", 0.0f }, { "fx_slot1", 34.0f },
            { "fx_slot2", 11.0f }, { "fx_slot3", 32.0f }, { "fx_slot4", 16.0f }, { "fx_slot4_bypass", 1.0f },
            { "fx_smear_density", 18.006f }, { "fx_smear_mix", 0.306386f }, { "fx_smear_on", 1.0f }, { "glide_legato", 1.0f },
            { "lfo1_div", 4.0f }, { "lfo1_rate", 0.1908662f }, { "master_clip", 0.0f }, { "mod1_amt", 0.5f },
            { "mod1_dst", 9.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.4f }, { "mod2_dst", 10.0f },
            { "mod2_src", 13.0f }, { "mod3_amt", 0.4f }, { "mod3_dst", 414.0f }, { "mod3_src", 14.0f },
            { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f }, { "mod4_src", 15.0f }, { "osc1_chord", 1.0f },
            { "osc1_detune", 27.4571f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_level", 1.0f },
            { "osc1_spread", 1.0f }, { "osc1_table", 104.0f }, { "osc1_uni_mode", 1.0f }, { "osc1_unison", 6.0f },
            { "osc2_chord", 1.0f }, { "osc2_detune", 33.6286f }, { "osc2_fb_type", 1.0f }, { "osc2_frame", 0.571429f },
            { "osc2_on", 1.0f }, { "osc2_table", 62.0f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 4.0f },
            { "osc4_excite", 7.0f }, { "osc4_fb_type", 1.0f }, { "osc4_mode", 1.0f }, { "osc4_on", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 16.0f }, { "sb_model", 1.0f },
            { "sb_on", 1.0f }, { "sb_size", 0.964f }, { "sub_chord", 1.0f }, { "sub_detune", 21.6f },
            { "sub_fb_type", 1.0f }, { "sub_frame", 0.589011f }, { "sub_on", 1.0f }, { "sub_shape", 1.0f },
            { "sub_table", 26.0f }, { "sub_uni_mode", 1.0f }, { "sub_unison", 11.0f }, { "subosc_level", 0.3f },
            { "subosc_on", 1.0f }, { "sym_amount", 0.776571f }, { "vec_drift", 1.0f }, { "vec_drift_rate", 0.496205f },
            { "vec_on", 1.0f }, { "vec_path", 1.0f }, { "vec_px1", 1.0f }, { "vec_px2", 1.0f },
            { "vec_px3", 1.0f }, { "vec_px4", 0.0f }, { "vec_px5", 0.0f }, { "vec_px6", 0.935484f },
            { "vec_px7", 0.0f }, { "vec_px8", 0.541786f }, { "vec_py1", 0.444547f }, { "vec_py2", 0.0f },
            { "vec_py3", 1.0f }, { "vec_py4", 0.0f }, { "vec_py5", 0.535218f }, { "vec_py6", 0.696467f },
            { "vec_py7", 1.0f }, { "vec_py8", 0.300682f }, { "vec_rate", 0.38035f }, { "vec_x", 0.518443f },
            { "vec_y", 0.491868f }, { "voice_spread", 0.395428f }, { "west_decay", 1.10662f }, { "west_fold", 0.508886f },
            { "west_on", 1.0f }, { "west_open", 0.569107f }, { "west_res", 0.403492f }, { "west_src", 1.0f },
            { "west_stages", 3.0f }, { "west_strike", 0.925759f }, { "west_sym", 1.0f }, { "master", -6.11f },
        };
        p.macroNames = { "CUTOFF", "RESO", "FOLD", "SPACE" };
        add (p);
    }
    // a tine plucked over a bar body, no soundboard (from "velocity warp physical pluck")
    {
        FactoryPreset p { "Kalimba Bar", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 1.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 35.0f },
            { "fx_slot2_mix", 0.1868478f }, { "master_clip", 0.0f }, { "mod1_amt", 0.832f }, { "mod1_dst", 414.0f },
            { "mod1_src", 6.0f }, { "mod2_amt", 0.878f }, { "mod2_dst", 5.0f }, { "mod2_src", 6.0f },
            { "mod3_amt", 0.586f }, { "mod3_dst", 6.0f }, { "mod3_src", 6.0f }, { "mod4_amt", 0.494f },
            { "mod4_dst", 43.0f }, { "mod4_src", 6.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 257.0f },
            { "mod5_src", 12.0f }, { "mod6_amt", 0.5f }, { "mod6_dst", 396.0f }, { "mod6_src", 13.0f },
            { "mod7_amt", 0.6f }, { "mod7_dst", 100.0f }, { "mod7_src", 14.0f }, { "mod8_amt", 0.5f },
            { "mod8_dst", 99.0f }, { "mod8_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f },
            { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.5632676f }, { "osc1_string_damp", 0.119763f },
            { "osc1_string_decay", 0.6335834f }, { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f },
            { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f }, { "osc2_level", 0.0f },
            { "osc2_on", 1.0f }, { "osc2_table", 19.0f }, { "osc2_warp", 2.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f },
            { "sb_mix", 0.355458f }, { "sb_model", 1.0f }, { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f },
            { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f }, { "west_fold", 0.0f },
            { "west_on", 1.0f }, { "west_open", 0.712386f }, { "master", 1.25f },
        };
        p.macroNames = { "TINE", "BODY", "ECHO", "SPACE" };
        add (p);
    }
    // a slapped, stiff plucked string zapped by the Disperser (from "velocity warp physical pluck")
    {
        FactoryPreset p { "Koto Snap", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 1.0f }, { "f1_cutoff", 8767.301f }, { "f1_morph", 0.4182199f },
            { "f1_type", 38.0f }, { "fx_awdyn_p1", 0.454275f }, { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f },
            { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f }, { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f },
            { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f }, { "master_clip", 0.0f }, { "mod1_amt", 0.832f },
            { "mod1_dst", 414.0f }, { "mod1_src", 6.0f }, { "mod2_amt", 0.878f }, { "mod2_dst", 5.0f },
            { "mod2_src", 6.0f }, { "mod3_amt", 0.586f }, { "mod3_dst", 6.0f }, { "mod3_src", 6.0f },
            { "mod4_amt", 0.494f }, { "mod4_dst", 43.0f }, { "mod4_src", 6.0f }, { "mod5_amt", -0.5f },
            { "mod5_dst", 9.0f }, { "mod5_src", 12.0f }, { "mod6_amt", 0.4f }, { "mod6_dst", 257.0f },
            { "mod6_src", 13.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 43.0f }, { "mod7_src", 14.0f },
            { "mod8_amt", 0.5f }, { "mod8_dst", 99.0f }, { "mod8_src", 15.0f }, { "osc1_couple", 0.127451f },
            { "osc1_damper", 0.541f }, { "osc1_excite", 3.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f },
            { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.15848f },
            { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 0.5026265f }, { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f },
            { "osc1_string_stiffness", 0.3118861f }, { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_level", 0.0f }, { "osc2_on", 1.0f }, { "osc2_table", 19.0f }, { "osc2_warp", 2.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f },
            { "res_on", 1.0f }, { "sb_mix", 0.355458f }, { "sb_model", 1.0f }, { "sb_on", 1.0f },
            { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f }, { "subosc_on", 1.0f },
            { "voice_spread", 0.287019f }, { "west_fold", 0.0f }, { "west_on", 1.0f }, { "west_open", 0.712386f },
            { "master", -13.68f },
        };
        p.macroNames = { "ZAP", "SNAP", "WARP", "SPACE" };
        add (p);
    }
    // the DX piano's operators down two octaves with short envelopes: an FM bass (from "E.PIANO 1 (ROM3A) modded")
    {
        FactoryPreset p { "Operator Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_velocity", 0.0f }, { "body_coupling_mode", 1.0f }, { "body_type", 2.0f }, { "f1_cutoff", 8170.2f },
            { "f1_drive", 1.0f }, { "f1_env", 5.0f }, { "f1_reso", 0.0f }, { "f2_cutoff", 8000.0f },
            { "f2_reso", 0.47631f }, { "filters_parallel", 1.0f }, { "fm_4to3", 1.0f }, { "fm_6to5", 1.0f },
            { "fm_amount", 1.0f }, { "fm_dx7_algorithm", 16.0f }, { "fm_fb6", 0.25f }, { "fx_awlofi_p1", 0.703916f },
            { "fx_patcher", 1.0f }, { "fx_reverb_keep_dry", 1.0f }, { "fx_reverb_mix", 0.0f }, { "fx_reverb_on", 1.0f },
            { "fx_slot1", 33.0f }, { "fx_slot1_mix", 0.3510938f }, { "mod1_amt", 0.5f }, { "mod1_dst", 57.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.4f }, { "mod2_dst", 9.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.3f },
            { "mod4_dst", 3.0f }, { "mod4_src", 15.0f }, { "mod7_amt", 1.0f }, { "mod7_dst", 452.0f },
            { "mod7_src", 9.0f }, { "mod8_amt", 1.0f }, { "mod8_dst", 452.0f }, { "mod8_src", 10.0f },
            { "opeg_lfo_delay", 33.0f }, { "opeg_lfo_pms", 2.0f }, { "opeg_lfo_speed", 15.0f }, { "opeg_lfo_sync", 0.0f },
            { "opeg_lfo_wave", 4.0f }, { "opeg_pitch_r1", 94.0f }, { "opeg_pitch_r2", 67.0f }, { "opeg_pitch_r3", 95.0f },
            { "opeg_pitch_r4", 60.0f }, { "osc1_amp_env", 17.0f }, { "osc1_eg_break", 0.0f }, { "osc1_eg_l2", 75.0f },
            { "osc1_eg_l3", 0.0f }, { "osc1_eg_r1", 96.0f }, { "osc1_eg_r2", 25.0f }, { "osc1_eg_r3", 25.0f },
            { "osc1_eg_r4", 67.0f }, { "osc1_eg_rate_key", 3.0f }, { "osc1_eg_vel", 2.0f }, { "osc1_fb_type", 3.0f },
            { "osc1_fine", 4.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 0.5f }, { "osc1_mode", 5.0f },
            { "osc1_ratio_snap", 0.0f }, { "osc1_semi", -24.0f }, { "osc1_table", 8.0f }, { "osc1_tune", 1.0f },
            { "osc2_amp_env", 17.0f }, { "osc2_eg_break", 0.0f }, { "osc2_eg_l2", 75.0f }, { "osc2_eg_l3", 0.0f },
            { "osc2_eg_out", 58.0f }, { "osc2_eg_r1", 95.0f }, { "osc2_eg_r2", 74.75172f }, { "osc2_eg_r3", 35.0f },
            { "osc2_eg_r4", 78.0f }, { "osc2_eg_rate_key", 3.0f }, { "osc2_eg_vel", 7.0f }, { "osc2_fb_type", 3.0f },
            { "osc2_fine", 0.0f }, { "osc2_level", 0.5f }, { "osc2_mode", 5.0f }, { "osc2_on", 1.0f },
            { "osc2_out", 0.0f }, { "osc2_ratio", 14.0f }, { "osc2_ratio_snap", 0.0f }, { "osc2_semi", -24.0f },
            { "osc2_table", 8.0f }, { "osc2_tune", 1.0f }, { "osc4_amp_env", 17.0f }, { "osc4_eg_break", 0.0f },
            { "osc4_eg_l2", 95.0f }, { "osc4_eg_l3", 0.0f }, { "osc4_eg_out", 89.0f }, { "osc4_eg_r1", 95.0f },
            { "osc4_eg_r2", 29.0f }, { "osc4_eg_r3", 20.0f }, { "osc4_eg_r4", 50.0f }, { "osc4_eg_rate_key", 3.0f },
            { "osc4_eg_vel", 6.0f }, { "osc4_fb_type", 3.0f }, { "osc4_level", 0.5f }, { "osc4_mode", 5.0f },
            { "osc4_on", 1.0f }, { "osc4_out", 0.0f }, { "osc4_ratio_snap", 0.0f }, { "osc4_semi", -24.0f },
            { "osc4_table", 8.0f }, { "osc4_tune", 1.0f }, { "osc5_amp_env", 17.0f }, { "osc5_eg_break", 0.0f },
            { "osc5_eg_l2", 95.0f }, { "osc5_eg_l3", 0.0f }, { "osc5_eg_r1", 95.0f }, { "osc5_eg_r2", 20.0f },
            { "osc5_eg_r3", 20.0f }, { "osc5_eg_r4", 50.0f }, { "osc5_eg_rate_key", 3.0f }, { "osc5_fb_type", 3.0f },
            { "osc5_fine", -8.0f }, { "osc5_level", 0.5f }, { "osc5_mode", 5.0f }, { "osc5_on", 1.0f },
            { "osc5_ratio_snap", 0.0f }, { "osc5_semi", -24.0f }, { "osc5_table", 8.0f }, { "osc5_tune", 1.0f },
            { "osc6_amp_env", 17.0f }, { "osc6_eg_break", 41.0f }, { "osc6_eg_l2", 95.0f }, { "osc6_eg_l3", 0.0f },
            { "osc6_eg_out", 79.0f }, { "osc6_eg_r1", 95.0f }, { "osc6_eg_r2", 29.0f }, { "osc6_eg_r3", 20.0f },
            { "osc6_eg_r4", 50.0f }, { "osc6_eg_rate_key", 3.0f }, { "osc6_eg_rdepth", 19.0f }, { "osc6_eg_vel", 6.0f },
            { "osc6_fb_type", 3.0f }, { "osc6_fine", 8.0f }, { "osc6_level", 0.5f }, { "osc6_mode", 5.0f },
            { "osc6_on", 1.0f }, { "osc6_out", 0.0f }, { "osc6_ratio_snap", 0.0f }, { "osc6_semi", -24.0f },
            { "osc6_table", 8.0f }, { "osc6_tune", 1.0f }, { "res_amount", 0.659628f }, { "res_on", 1.0f },
            { "sub_amp_env", 17.0f }, { "sub_eg_break", 0.0f }, { "sub_eg_l2", 95.0f }, { "sub_eg_l3", 0.0f },
            { "sub_eg_r1", 95.0f }, { "sub_eg_r2", 20.0f }, { "sub_eg_r3", 20.0f }, { "sub_eg_r4", 50.0f },
            { "sub_eg_rate_key", 3.0f }, { "sub_eg_vel", 2.0f }, { "sub_fb_type", 3.0f }, { "sub_level", 0.5f },
            { "sub_mode", 5.0f }, { "sub_on", 1.0f }, { "sub_ratio_snap", 0.0f }, { "sub_table", 8.0f },
            { "sub_tune", 1.0f }, { "west_fold", 0.670749f }, { "west_on", 1.0f }, { "west_open", 0.290935f },
            { "west_sym", 0.401644f }, { "master", 5.9f },
        };
        p.macroNames = { "FM", "TONE", "DRIVE", "SUB" };
        add (p);
    }
    // the DX piano cut short and pushed through an Airwindows acid filter: a clavinet (from "E.PIANO 1 (ROM3A) modded")
    {
        FactoryPreset p { "Operator Clav", {} };
        p.category = "Keys";
        p.values = {
            { "amp_velocity", 0.0f }, { "body_coupling_mode", 1.0f }, { "body_type", 2.0f }, { "f1_cutoff", 3544.331f },
            { "f1_drive", 1.0f }, { "f1_env", 5.0f }, { "f1_reso", 0.4017287f }, { "f1_type", 32.0f },
            { "f2_cutoff", 8000.0f }, { "f2_reso", 0.47631f }, { "filters_parallel", 1.0f }, { "fm_4to3", 1.0f },
            { "fm_6to5", 1.0f }, { "fm_amount", 1.0f }, { "fm_dx7_algorithm", 5.0f }, { "fm_fb6", 0.25f },
            { "fx_awlofi_p1", 0.703916f }, { "fx_patcher", 1.0f }, { "fx_reverb_keep_dry", 1.0f }, { "fx_reverb_mix", 0.0f },
            { "fx_reverb_on", 1.0f }, { "fx_slot1", 33.0f }, { "fx_slot1_mix", 0.3639089f }, { "fx_slot2", 7.0f },
            { "fx_slot2_mix", 0.334891f }, { "mod1_amt", 0.5f }, { "mod1_dst", 9.0f }, { "mod1_src", 12.0f },
            { "mod2_amt", 0.4f }, { "mod2_dst", 10.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f },
            { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.6f }, { "mod4_dst", 100.0f },
            { "mod4_src", 15.0f }, { "mod7_amt", 1.0f }, { "mod7_dst", 452.0f }, { "mod7_src", 9.0f },
            { "mod8_amt", 1.0f }, { "mod8_dst", 452.0f }, { "mod8_src", 10.0f }, { "opeg_lfo_delay", 33.0f },
            { "opeg_lfo_pms", 2.0f }, { "opeg_lfo_speed", 15.0f }, { "opeg_lfo_sync", 0.0f }, { "opeg_lfo_wave", 4.0f },
            { "opeg_pitch_r1", 94.0f }, { "opeg_pitch_r2", 67.0f }, { "opeg_pitch_r3", 95.0f }, { "opeg_pitch_r4", 60.0f },
            { "osc1_amp_env", 17.0f }, { "osc1_eg_break", 0.0f }, { "osc1_eg_l2", 75.0f }, { "osc1_eg_l3", 0.0f },
            { "osc1_eg_r1", 96.0f }, { "osc1_eg_r2", 59.01706f }, { "osc1_eg_r3", 25.0f }, { "osc1_eg_r4", 67.0f },
            { "osc1_eg_rate_key", 3.0f }, { "osc1_eg_vel", 2.0f }, { "osc1_fb_type", 3.0f }, { "osc1_fine", 4.0f },
            { "osc1_frame", 0.0f }, { "osc1_level", 0.5f }, { "osc1_mode", 5.0f }, { "osc1_ratio_snap", 0.0f },
            { "osc1_table", 8.0f }, { "osc1_tune", 1.0f }, { "osc2_amp_env", 17.0f }, { "osc2_eg_break", 0.0f },
            { "osc2_eg_l2", 75.0f }, { "osc2_eg_l3", 0.0f }, { "osc2_eg_out", 58.0f }, { "osc2_eg_r1", 95.0f },
            { "osc2_eg_r2", 66.55481f }, { "osc2_eg_r3", 35.0f }, { "osc2_eg_r4", 78.0f }, { "osc2_eg_rate_key", 3.0f },
            { "osc2_eg_vel", 7.0f }, { "osc2_fb_type", 3.0f }, { "osc2_fine", 0.0f }, { "osc2_level", 0.5f },
            { "osc2_mode", 5.0f }, { "osc2_on", 1.0f }, { "osc2_out", 0.0f }, { "osc2_ratio", 14.0f },
            { "osc2_ratio_snap", 0.0f }, { "osc2_table", 8.0f }, { "osc2_tune", 1.0f }, { "osc4_amp_env", 17.0f },
            { "osc4_eg_break", 0.0f }, { "osc4_eg_l2", 95.0f }, { "osc4_eg_l3", 0.0f }, { "osc4_eg_out", 89.0f },
            { "osc4_eg_r1", 95.0f }, { "osc4_eg_r2", 69.74099f }, { "osc4_eg_r3", 20.0f }, { "osc4_eg_r4", 50.0f },
            { "osc4_eg_rate_key", 3.0f }, { "osc4_eg_vel", 6.0f }, { "osc4_fb_type", 3.0f }, { "osc4_level", 0.5f },
            { "osc4_mode", 5.0f }, { "osc4_on", 1.0f }, { "osc4_out", 0.0f }, { "osc4_ratio_snap", 0.0f },
            { "osc4_table", 8.0f }, { "osc4_tune", 1.0f }, { "osc5_amp_env", 17.0f }, { "osc5_eg_break", 0.0f },
            { "osc5_eg_l2", 95.0f }, { "osc5_eg_l3", 0.0f }, { "osc5_eg_r1", 95.0f }, { "osc5_eg_r2", 20.0f },
            { "osc5_eg_r3", 20.0f }, { "osc5_eg_r4", 50.0f }, { "osc5_eg_rate_key", 3.0f }, { "osc5_fb_type", 3.0f },
            { "osc5_fine", -8.0f }, { "osc5_level", 0.5f }, { "osc5_mode", 5.0f }, { "osc5_on", 1.0f },
            { "osc5_ratio_snap", 0.0f }, { "osc5_table", 8.0f }, { "osc5_tune", 1.0f }, { "osc6_amp_env", 17.0f },
            { "osc6_eg_break", 41.0f }, { "osc6_eg_l2", 95.0f }, { "osc6_eg_l3", 0.0f }, { "osc6_eg_out", 79.0f },
            { "osc6_eg_r1", 95.0f }, { "osc6_eg_r2", 29.0f }, { "osc6_eg_r3", 20.0f }, { "osc6_eg_r4", 50.0f },
            { "osc6_eg_rate_key", 3.0f }, { "osc6_eg_rdepth", 19.0f }, { "osc6_eg_vel", 6.0f }, { "osc6_fb_type", 3.0f },
            { "osc6_fine", 8.0f }, { "osc6_level", 0.5f }, { "osc6_mode", 5.0f }, { "osc6_on", 1.0f },
            { "osc6_out", 0.0f }, { "osc6_ratio_snap", 0.0f }, { "osc6_table", 8.0f }, { "osc6_tune", 1.0f },
            { "res_amount", 0.659628f }, { "res_on", 1.0f }, { "sub_amp_env", 17.0f }, { "sub_eg_break", 0.0f },
            { "sub_eg_l2", 95.0f }, { "sub_eg_l3", 0.0f }, { "sub_eg_r1", 95.0f }, { "sub_eg_r2", 20.0f },
            { "sub_eg_r3", 20.0f }, { "sub_eg_r4", 50.0f }, { "sub_eg_rate_key", 3.0f }, { "sub_eg_vel", 2.0f },
            { "sub_fb_type", 3.0f }, { "sub_level", 0.5f }, { "sub_mode", 5.0f }, { "sub_on", 1.0f },
            { "sub_ratio_snap", 0.0f }, { "sub_table", 8.0f }, { "sub_tune", 1.0f }, { "west_fold", 0.670749f },
            { "west_on", 1.0f }, { "west_open", 0.290935f }, { "west_sym", 0.401644f }, { "master", 5.4f },
        };
        p.macroNames = { "FUNK", "BITE", "DRIVE", "CHORUS" };
        add (p);
    }
    // the DX piano's operators held as a mono lead with saturation (from "E.PIANO 1 (ROM3A) modded")
    {
        FactoryPreset p { "Operator Lead", {} };
        p.category = "Lead";
        p.values = {
            { "amp_velocity", 0.0f }, { "body_coupling_mode", 1.0f }, { "body_type", 2.0f }, { "f1_cutoff", 8170.2f },
            { "f1_drive", 1.0f }, { "f1_env", 5.0f }, { "f1_reso", 0.0f }, { "f2_cutoff", 8000.0f },
            { "f2_reso", 0.47631f }, { "filters_parallel", 1.0f }, { "fm_4to3", 1.0f }, { "fm_6to5", 1.0f },
            { "fm_amount", 1.0f }, { "fm_dx7_algorithm", 5.0f }, { "fm_fb6", 0.25f }, { "fx_awlofi_p1", 0.703916f },
            { "fx_patcher", 1.0f }, { "fx_reverb_keep_dry", 1.0f }, { "fx_reverb_mix", 0.0f }, { "fx_reverb_on", 1.0f },
            { "fx_slot1", 33.0f }, { "fx_slot1_mix", 0.3152826f }, { "fx_slot2", 35.0f }, { "fx_slot2_mix", 0.2423767f },
            { "glide", 0.003f }, { "glide_legato", 1.0f }, { "mod1_amt", 0.5f }, { "mod1_dst", 57.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 99.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.4f }, { "mod3_dst", 9.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "mod7_amt", 1.0f }, { "mod7_dst", 452.0f },
            { "mod7_src", 9.0f }, { "mod8_amt", 1.0f }, { "mod8_dst", 452.0f }, { "mod8_src", 10.0f },
            { "opeg_lfo_delay", 33.0f }, { "opeg_lfo_pms", 2.0f }, { "opeg_lfo_speed", 15.0f }, { "opeg_lfo_sync", 0.0f },
            { "opeg_lfo_wave", 4.0f }, { "opeg_pitch_r1", 94.0f }, { "opeg_pitch_r2", 67.0f }, { "opeg_pitch_r3", 95.0f },
            { "opeg_pitch_r4", 60.0f }, { "osc1_amp_env", 17.0f }, { "osc1_eg_break", 0.0f }, { "osc1_eg_l2", 75.0f },
            { "osc1_eg_l3", 80.63977f }, { "osc1_eg_r1", 96.0f }, { "osc1_eg_r2", 25.0f }, { "osc1_eg_r3", 25.0f },
            { "osc1_eg_r4", 67.0f }, { "osc1_eg_rate_key", 3.0f }, { "osc1_eg_vel", 2.0f }, { "osc1_fb_type", 3.0f },
            { "osc1_fine", 4.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 0.5f }, { "osc1_mode", 5.0f },
            { "osc1_ratio_snap", 0.0f }, { "osc1_table", 8.0f }, { "osc1_tune", 1.0f }, { "osc2_amp_env", 17.0f },
            { "osc2_eg_break", 0.0f }, { "osc2_eg_l2", 75.0f }, { "osc2_eg_l3", 69.51791f }, { "osc2_eg_out", 58.0f },
            { "osc2_eg_r1", 95.0f }, { "osc2_eg_r2", 50.0f }, { "osc2_eg_r3", 35.0f }, { "osc2_eg_r4", 78.0f },
            { "osc2_eg_rate_key", 3.0f }, { "osc2_eg_vel", 7.0f }, { "osc2_fb_type", 3.0f }, { "osc2_fine", 0.0f },
            { "osc2_level", 0.5f }, { "osc2_mode", 5.0f }, { "osc2_on", 1.0f }, { "osc2_out", 0.0f },
            { "osc2_ratio", 14.0f }, { "osc2_ratio_snap", 0.0f }, { "osc2_table", 8.0f }, { "osc2_tune", 1.0f },
            { "osc4_amp_env", 17.0f }, { "osc4_eg_break", 0.0f }, { "osc4_eg_l2", 95.0f }, { "osc4_eg_l3", 81.9018f },
            { "osc4_eg_out", 89.0f }, { "osc4_eg_r1", 95.0f }, { "osc4_eg_r2", 29.0f }, { "osc4_eg_r3", 20.0f },
            { "osc4_eg_r4", 50.0f }, { "osc4_eg_rate_key", 3.0f }, { "osc4_eg_vel", 6.0f }, { "osc4_fb_type", 3.0f },
            { "osc4_level", 0.5f }, { "osc4_mode", 5.0f }, { "osc4_on", 1.0f }, { "osc4_out", 0.0f },
            { "osc4_ratio_snap", 0.0f }, { "osc4_table", 8.0f }, { "osc4_tune", 1.0f }, { "osc5_amp_env", 17.0f },
            { "osc5_eg_break", 0.0f }, { "osc5_eg_l2", 95.0f }, { "osc5_eg_l3", 0.0f }, { "osc5_eg_r1", 95.0f },
            { "osc5_eg_r2", 20.0f }, { "osc5_eg_r3", 20.0f }, { "osc5_eg_r4", 50.0f }, { "osc5_eg_rate_key", 3.0f },
            { "osc5_fb_type", 3.0f }, { "osc5_fine", -8.0f }, { "osc5_level", 0.5f }, { "osc5_mode", 5.0f },
            { "osc5_on", 1.0f }, { "osc5_ratio_snap", 0.0f }, { "osc5_table", 8.0f }, { "osc5_tune", 1.0f },
            { "osc6_amp_env", 17.0f }, { "osc6_eg_break", 41.0f }, { "osc6_eg_l2", 95.0f }, { "osc6_eg_l3", 0.0f },
            { "osc6_eg_out", 79.0f }, { "osc6_eg_r1", 95.0f }, { "osc6_eg_r2", 29.0f }, { "osc6_eg_r3", 20.0f },
            { "osc6_eg_r4", 50.0f }, { "osc6_eg_rate_key", 3.0f }, { "osc6_eg_rdepth", 19.0f }, { "osc6_eg_vel", 6.0f },
            { "osc6_fb_type", 3.0f }, { "osc6_fine", 8.0f }, { "osc6_level", 0.5f }, { "osc6_mode", 5.0f },
            { "osc6_on", 1.0f }, { "osc6_out", 0.0f }, { "osc6_ratio_snap", 0.0f }, { "osc6_table", 8.0f },
            { "osc6_tune", 1.0f }, { "poly_voices", 1.0f }, { "res_amount", 0.659628f }, { "res_on", 1.0f },
            { "sub_amp_env", 17.0f }, { "sub_eg_break", 0.0f }, { "sub_eg_l2", 95.0f }, { "sub_eg_l3", 0.0f },
            { "sub_eg_r1", 95.0f }, { "sub_eg_r2", 20.0f }, { "sub_eg_r3", 20.0f }, { "sub_eg_r4", 50.0f },
            { "sub_eg_rate_key", 3.0f }, { "sub_eg_vel", 2.0f }, { "sub_fb_type", 3.0f }, { "sub_level", 0.5f },
            { "sub_mode", 5.0f }, { "sub_on", 1.0f }, { "sub_ratio_snap", 0.0f }, { "sub_table", 8.0f },
            { "sub_tune", 1.0f }, { "voice_mode", 2.0f }, { "west_fold", 0.670749f }, { "west_on", 1.0f },
            { "west_open", 0.290935f }, { "west_sym", 0.401644f }, { "master", 9.18f },
        };
        p.macroNames = { "FM", "DRIVE", "TONE", "ECHO" };
        add (p);
    }
    // her DX electric piano re-voiced on another algorithm with a brighter tine ratio (from "E.PIANO 1 (ROM3A) modded")
    {
        FactoryPreset p { "Operator Tines", {} };
        p.category = "Keys";
        p.values = {
            { "amp_velocity", 0.0f }, { "body_coupling_mode", 1.0f }, { "body_type", 2.0f }, { "f1_cutoff", 8170.2f },
            { "f1_drive", 1.0f }, { "f1_env", 5.0f }, { "f1_reso", 0.0f }, { "f2_cutoff", 8000.0f },
            { "f2_reso", 0.47631f }, { "filters_parallel", 1.0f }, { "fm_4to3", 1.0f }, { "fm_6to5", 1.0f },
            { "fm_amount", 1.0f }, { "fm_dx7_algorithm", 5.0f }, { "fm_fb6", 0.25f }, { "fx_awlofi_p1", 0.703916f },
            { "fx_patcher", 1.0f }, { "fx_reverb_keep_dry", 1.0f }, { "fx_reverb_mix", 0.0f }, { "fx_reverb_on", 1.0f },
            { "fx_slot1", 15.0f }, { "fx_slot1_mix", 0.3669587f }, { "fx_slot2", 34.0f }, { "fx_slot2_mix", 0.247276f },
            { "mod1_amt", 0.5f }, { "mod1_dst", 57.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 99.0f }, { "mod2_src", 13.0f }, { "mod3_amt", -0.4f }, { "mod3_dst", 9.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f }, { "mod4_src", 15.0f },
            { "mod7_amt", 1.0f }, { "mod7_dst", 452.0f }, { "mod7_src", 9.0f }, { "mod8_amt", 1.0f },
            { "mod8_dst", 452.0f }, { "mod8_src", 10.0f }, { "opeg_lfo_delay", 33.0f }, { "opeg_lfo_pms", 2.0f },
            { "opeg_lfo_speed", 15.0f }, { "opeg_lfo_sync", 0.0f }, { "opeg_lfo_wave", 4.0f }, { "opeg_pitch_r1", 94.0f },
            { "opeg_pitch_r2", 67.0f }, { "opeg_pitch_r3", 95.0f }, { "opeg_pitch_r4", 60.0f }, { "osc1_amp_env", 17.0f },
            { "osc1_eg_break", 0.0f }, { "osc1_eg_l2", 75.0f }, { "osc1_eg_l3", 0.0f }, { "osc1_eg_r1", 96.0f },
            { "osc1_eg_r2", 25.0f }, { "osc1_eg_r3", 25.0f }, { "osc1_eg_r4", 67.0f }, { "osc1_eg_rate_key", 3.0f },
            { "osc1_eg_vel", 2.0f }, { "osc1_fb_type", 3.0f }, { "osc1_fine", 4.0f }, { "osc1_frame", 0.0f },
            { "osc1_level", 0.5f }, { "osc1_mode", 5.0f }, { "osc1_ratio_snap", 0.0f }, { "osc1_table", 8.0f },
            { "osc1_tune", 1.0f }, { "osc2_amp_env", 17.0f }, { "osc2_eg_break", 0.0f }, { "osc2_eg_l2", 75.0f },
            { "osc2_eg_l3", 0.0f }, { "osc2_eg_out", 71.49348f }, { "osc2_eg_r1", 95.0f }, { "osc2_eg_r2", 50.0f },
            { "osc2_eg_r3", 35.0f }, { "osc2_eg_r4", 78.0f }, { "osc2_eg_rate_key", 3.0f }, { "osc2_eg_vel", 7.0f },
            { "osc2_fb_type", 3.0f }, { "osc2_fine", 0.0f }, { "osc2_level", 0.5f }, { "osc2_mode", 5.0f },
            { "osc2_on", 1.0f }, { "osc2_out", 0.0f }, { "osc2_ratio", 4.0f }, { "osc2_ratio_snap", 0.0f },
            { "osc2_table", 8.0f }, { "osc2_tune", 1.0f }, { "osc4_amp_env", 17.0f }, { "osc4_eg_break", 0.0f },
            { "osc4_eg_l2", 95.0f }, { "osc4_eg_l3", 0.0f }, { "osc4_eg_out", 89.0f }, { "osc4_eg_r1", 95.0f },
            { "osc4_eg_r2", 29.0f }, { "osc4_eg_r3", 20.0f }, { "osc4_eg_r4", 50.0f }, { "osc4_eg_rate_key", 3.0f },
            { "osc4_eg_vel", 6.0f }, { "osc4_fb_type", 3.0f }, { "osc4_level", 0.5f }, { "osc4_mode", 5.0f },
            { "osc4_on", 1.0f }, { "osc4_out", 0.0f }, { "osc4_ratio_snap", 0.0f }, { "osc4_table", 8.0f },
            { "osc4_tune", 1.0f }, { "osc5_amp_env", 17.0f }, { "osc5_eg_break", 0.0f }, { "osc5_eg_l2", 95.0f },
            { "osc5_eg_l3", 0.0f }, { "osc5_eg_r1", 95.0f }, { "osc5_eg_r2", 20.0f }, { "osc5_eg_r3", 20.0f },
            { "osc5_eg_r4", 50.0f }, { "osc5_eg_rate_key", 3.0f }, { "osc5_fb_type", 3.0f }, { "osc5_fine", -8.0f },
            { "osc5_level", 0.5f }, { "osc5_mode", 5.0f }, { "osc5_on", 1.0f }, { "osc5_ratio_snap", 0.0f },
            { "osc5_table", 8.0f }, { "osc5_tune", 1.0f }, { "osc6_amp_env", 17.0f }, { "osc6_eg_break", 41.0f },
            { "osc6_eg_l2", 95.0f }, { "osc6_eg_l3", 0.0f }, { "osc6_eg_out", 79.0f }, { "osc6_eg_r1", 95.0f },
            { "osc6_eg_r2", 29.0f }, { "osc6_eg_r3", 20.0f }, { "osc6_eg_r4", 50.0f }, { "osc6_eg_rate_key", 3.0f },
            { "osc6_eg_rdepth", 19.0f }, { "osc6_eg_vel", 6.0f }, { "osc6_fb_type", 3.0f }, { "osc6_fine", 8.0f },
            { "osc6_level", 0.5f }, { "osc6_mode", 5.0f }, { "osc6_on", 1.0f }, { "osc6_out", 0.0f },
            { "osc6_ratio_snap", 0.0f }, { "osc6_table", 8.0f }, { "osc6_tune", 1.0f }, { "res_amount", 0.659628f },
            { "res_on", 1.0f }, { "sub_amp_env", 17.0f }, { "sub_eg_break", 0.0f }, { "sub_eg_l2", 95.0f },
            { "sub_eg_l3", 0.0f }, { "sub_eg_r1", 95.0f }, { "sub_eg_r2", 20.0f }, { "sub_eg_r3", 20.0f },
            { "sub_eg_r4", 50.0f }, { "sub_eg_rate_key", 3.0f }, { "sub_eg_vel", 2.0f }, { "sub_fb_type", 3.0f },
            { "sub_level", 0.5f }, { "sub_mode", 5.0f }, { "sub_on", 1.0f }, { "sub_ratio_snap", 0.0f },
            { "sub_table", 8.0f }, { "sub_tune", 1.0f }, { "west_fold", 0.670749f }, { "west_on", 1.0f },
            { "west_open", 0.290935f }, { "west_sym", 0.401644f }, { "master", 5.41f },
        };
        p.macroNames = { "TINE", "CHORUS", "TONE", "SPACE" };
        add (p);
    }
    // her mono trance pad thinned into a lead, echoes instead of the wash (from "trancey pad")
    {
        FactoryPreset p { "Pear Lead", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.648308f }, { "drift", 0.481143f }, { "f1_cutoff", 762.3428f },
            { "f1_drive", 2.13772f }, { "f1_keytrack", 1.0f }, { "f1_morph", 0.156571f }, { "f1_reso", 0.423963f },
            { "f1_type", 37.0f }, { "fx_awtape_p1", 0.359729f }, { "fx_awverb_mix", 0.180106f }, { "fx_gate_div", 5.0f },
            { "fx_gate_pattern", 8.0f }, { "fx_gate_step10", 0.0f }, { "fx_gate_step12", 0.0f }, { "fx_gate_step14", 0.0f },
            { "fx_gate_step2", 0.0f }, { "fx_gate_step4", 0.0f }, { "fx_gate_step8", 0.0f }, { "fx_slot1", 34.0f },
            { "fx_slot2", 35.0f }, { "fx_slot2_mix", 0.1722532f }, { "fx_slot3", 32.0f }, { "fx_slot4", 16.0f },
            { "fx_slot4_bypass", 1.0f }, { "fx_smear_density", 18.006f }, { "fx_smear_mix", 0.306386f }, { "fx_smear_on", 1.0f },
            { "glide", 0.002f }, { "glide_legato", 1.0f }, { "lfo1_div", 4.0f }, { "lfo1_shape", 3.0f },
            { "lfo1_sync", 1.0f }, { "master_clip", 0.0f }, { "mod1_amt", 0.5f }, { "mod1_dst", 9.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.4f }, { "mod2_dst", 414.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 48.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "osc1_detune", 27.4571f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_level", 1.0f }, { "osc1_spread", 1.0f }, { "osc1_table", 104.0f },
            { "osc1_uni_mode", 1.0f }, { "osc1_unison", 5.0f }, { "osc2_detune", 33.6286f }, { "osc2_fb_type", 1.0f },
            { "osc2_frame", 0.571429f }, { "osc2_on", 1.0f }, { "osc2_table", 62.0f }, { "osc2_uni_mode", 1.0f },
            { "osc2_unison", 5.0f }, { "osc4_excite", 7.0f }, { "osc4_fb_type", 1.0f }, { "osc4_mode", 1.0f },
            { "osc4_on", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 1.0f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.964f }, { "sub_detune", 21.6f },
            { "sub_fb_type", 1.0f }, { "sub_frame", 0.589011f }, { "sub_on", 1.0f }, { "sub_shape", 1.0f },
            { "sub_table", 26.0f }, { "sub_uni_mode", 1.0f }, { "sub_unison", 11.0f }, { "subosc_level", 0.3f },
            { "subosc_on", 1.0f }, { "sym_amount", 0.776571f }, { "vec_drift", 1.0f }, { "vec_drift_rate", 0.496205f },
            { "vec_on", 1.0f }, { "vec_path", 1.0f }, { "vec_px1", 1.0f }, { "vec_px2", 1.0f },
            { "vec_px3", 1.0f }, { "vec_px4", 0.0f }, { "vec_px5", 0.0f }, { "vec_px6", 0.935484f },
            { "vec_px7", 0.0f }, { "vec_px8", 0.541786f }, { "vec_py1", 0.444547f }, { "vec_py2", 0.0f },
            { "vec_py3", 1.0f }, { "vec_py4", 0.0f }, { "vec_py5", 0.535218f }, { "vec_py6", 0.696467f },
            { "vec_py7", 1.0f }, { "vec_py8", 0.300682f }, { "vec_rate", 0.38035f }, { "vec_x", 0.518443f },
            { "vec_y", 0.491868f }, { "voice_mode", 2.0f }, { "voice_spread", 0.395428f }, { "west_decay", 1.10662f },
            { "west_fold", 0.508886f }, { "west_on", 1.0f }, { "west_open", 0.569107f }, { "west_res", 0.403492f },
            { "west_src", 1.0f }, { "west_stages", 3.0f }, { "west_strike", 0.925759f }, { "west_sym", 1.0f },
            { "master", 0.63f },
        };
        p.macroNames = { "CUTOFF", "FOLD", "WIDTH", "ECHO" };
        add (p);
    }
    // her crazy bass's unison saw as a short pick, with a physical steel string under it (from "crazy bass")
    {
        FactoryPreset p { "Picked Steel", {} };
        p.category = "Pluck";
        p.values = {
            { "amp_attack", 0.005999f }, { "amp_decay", 0.4780639f }, { "amp_release", 0.30485f }, { "amp_sustain", 0.0f },
            { "arp_gate", 0.5003f }, { "bend_range", 0.0f }, { "e4_attack", 0.05099f }, { "e4_release", 0.30485f },
            { "f1_cutoff", 1772.18f }, { "f1_drive", 4.996f }, { "f1_env", 1.2f }, { "f1_reso", 0.39984f },
            { "f1_slope", 1.0f }, { "f1_type", 33.0f }, { "f2e_release", 0.30485f }, { "fe_decay", 0.499505f },
            { "fe_release", 0.30485f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_rate", 0.7973f }, { "fx_comb_feedback", 0.60043f },
            { "fx_comb_freq", 219.98f }, { "fx_comp_ratio", 4.002f }, { "fx_comp_release", 149.59f }, { "fx_delay_damping", 0.5f },
            { "fx_delay_feedback", 0.55005f }, { "fx_delay_mix", 0.35f }, { "fx_delay_on", 1.0f }, { "fx_delay_pitch", -7.0f },
            { "fx_delay_sync", 1.0f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f }, { "fx_delay_wow", 0.3f },
            { "fx_dim_rate", 0.40155f }, { "fx_drive_amount", 5.009f }, { "fx_drive_mix", 0.6f }, { "fx_feedback_amount", 0.6004f },
            { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f }, { "fx_flanger_rate", 0.50315f }, { "fx_haas_delay", 13.987f },
            { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f }, { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f },
            { "fx_slot1", 2.0f }, { "fx_slot2", 35.0f }, { "fx_slot2_mix", 0.2974826f }, { "fx_smear_density", 11.994f },
            { "fx_smear_size", 119.96f }, { "fx_tape_stop_time", 0.59905f }, { "fx_trem_rate", 4.0001f }, { "lfo1_rate", 4.009f },
            { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f },
            { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f },
            { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f },
            { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f },
            { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f },
            { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f },
            { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f },
            { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f },
            { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f },
            { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f },
            { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f },
            { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f },
            { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f }, { "me_attack", 0.05099f },
            { "me_decay", 0.399605f }, { "me_release", 0.30485f }, { "mod1_amt", 0.5f }, { "mod1_dst", 9.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 6.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f },
            { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f }, { "osc1_detune", 30.0f },
            { "osc1_frame", 0.3f }, { "osc1_spread", 0.6f }, { "osc1_unison", 4.0f }, { "osc2_excite", 3.0f },
            { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_string_slap", 1.0f }, { "osc2_unison", 3.0f },
            { "sub_level", 0.5f }, { "sub_on", 1.0f }, { "sub_shape", 1.0f }, { "master", -5.78f },
        };
        p.macroNames = { "PICK", "STRING", "DRIVE", "ECHO" };
        add (p);
    }
    // her patcher bass with the west folder pushed and an Airwindows acid filter (from "patcher bass")
    {
        FactoryPreset p { "Reed Growl Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 0.527841f }, { "amp_release", 0.3f }, { "amp_sustain", 0.172501f },
            { "amp_velocity", 0.660451f }, { "cable_f1", 8.0f }, { "cable_osc1", 7.0f }, { "cable_osc3", 7.0f },
            { "cable_subnoise", 5.0f }, { "f1_cutoff", 4054.94f }, { "f1_drive", 1.15242f }, { "f1_env", 5.0f },
            { "f1_reso", 0.0f }, { "f2_cutoff", 525.2304f }, { "f2_env", 5.0f }, { "f2_reso", 0.4545167f },
            { "f2_type", 32.0f }, { "f2e_decay", 0.0391077f }, { "f2e_release", 0.005f }, { "f2e_sustain", 0.0f },
            { "fe_curve", 0.453138f }, { "fe_dcurve", -0.594353f }, { "fe_rcurve", -1.0f }, { "fe_sustain", 0.0746646f },
            { "fm_noise2", 0.0849634f }, { "fx_awsat_algo", 2.0f }, { "fx_patcher", 1.0f }, { "fx_routing", 1.0f },
            { "fx_slot1", 31.0f }, { "fx_slot2", 33.0f }, { "fx_voc_bands", 24.0f }, { "fx_voc_width", 0.476249f },
            { "mod1_amt", -0.814f }, { "mod1_dst", 5.0f }, { "mod1_src", 5.0f }, { "mod2_amt", 0.782f },
            { "mod2_dst", 428.0f }, { "mod2_src", 5.0f }, { "mod3_amt", 0.542f }, { "mod3_dst", 345.0f },
            { "mod3_src", 5.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 414.0f }, { "mod4_src", 12.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 11.0f }, { "mod5_src", 13.0f }, { "mod6_amt", -0.4f },
            { "mod6_dst", 3.0f }, { "mod6_src", 14.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 99.0f },
            { "mod7_src", 15.0f }, { "noise_type", 1.0f }, { "osc1_excite", 8.0f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_table", 0.0f },
            { "osc1_unison", 2.0f }, { "osc2_fb_type", 1.0f }, { "osc2_frame", 0.916574f }, { "osc2_on", 1.0f },
            { "osc2_semi", 12.0f }, { "osc2_spectral", 2.0f }, { "osc2_spectral_amt", 0.0139103f }, { "osc2_table", 37.0f },
            { "osc2_warp2", 9.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "route_cables", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f }, { "subosc_level", 0.3f },
            { "subosc_on", 1.0f }, { "west_fold", 0.7585766f }, { "west_mode", 1.0f }, { "west_on", 1.0f },
            { "west_stages", 1.0f }, { "west_strike", 0.289397f }, { "master", -5.1f },
        };
        p.macroNames = { "GROWL", "ACID", "REED", "DIRT" };
        add (p);
    }
    // her patcher bass's reed an octave up as a breathy mono lead (from "patcher bass")
    {
        FactoryPreset p { "Reed Lead", {} };
        p.category = "Lead";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_decay", 1.0f }, { "amp_release", 0.3f }, { "amp_sustain", 0.642098f },
            { "amp_velocity", 0.660451f }, { "cable_f1", 8.0f }, { "cable_osc1", 7.0f }, { "cable_osc3", 7.0f },
            { "cable_subnoise", 5.0f }, { "f1_cutoff", 4054.94f }, { "f1_drive", 1.15242f }, { "f1_env", 5.0f },
            { "f1_reso", 0.0f }, { "f2_cutoff", 1853.36f }, { "f2_env", 5.0f }, { "f2_type", 14.0f },
            { "f2e_decay", 0.0391077f }, { "f2e_release", 0.005f }, { "f2e_sustain", 0.0f }, { "fe_curve", 0.453138f },
            { "fe_dcurve", -0.594353f }, { "fe_rcurve", -1.0f }, { "fe_sustain", 0.0746646f }, { "fm_noise2", 0.0849634f },
            { "fx_awsat_algo", 2.0f }, { "fx_patcher", 1.0f }, { "fx_routing", 1.0f }, { "fx_slot1", 31.0f },
            { "fx_slot2", 33.0f }, { "fx_voc_bands", 24.0f }, { "fx_voc_width", 0.476249f }, { "glide", 0.003f },
            { "glide_legato", 1.0f }, { "mod1_amt", -0.814f }, { "mod1_dst", 5.0f }, { "mod1_src", 5.0f },
            { "mod2_amt", 0.782f }, { "mod2_dst", 428.0f }, { "mod2_src", 5.0f }, { "mod3_amt", 0.542f },
            { "mod3_dst", 345.0f }, { "mod3_src", 5.0f }, { "mod4_amt", 0.4f }, { "mod4_dst", 3.0f },
            { "mod4_src", 12.0f }, { "mod5_amt", 0.4f }, { "mod5_dst", 414.0f }, { "mod5_src", 13.0f },
            { "mod6_amt", 0.4f }, { "mod6_dst", 9.0f }, { "mod6_src", 14.0f }, { "mod7_amt", 0.5f },
            { "mod7_dst", 99.0f }, { "mod7_src", 15.0f }, { "noise_type", 1.0f }, { "osc1_excite", 8.0f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f },
            { "osc1_semi", 12.0f }, { "osc1_table", 0.0f }, { "osc1_unison", 2.0f }, { "osc2_fb_type", 1.0f },
            { "osc2_frame", 0.916574f }, { "osc2_on", 1.0f }, { "osc2_semi", 24.0f }, { "osc2_spectral", 2.0f },
            { "osc2_spectral_amt", 0.0139103f }, { "osc2_table", 37.0f }, { "osc2_warp2", 9.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 1.0f }, { "route_cables", 1.0f },
            { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f }, { "subosc_level", 0.3f }, { "subosc_on", 1.0f },
            { "voice_mode", 2.0f }, { "west_fold", 1.0f }, { "west_mode", 1.0f }, { "west_on", 1.0f },
            { "west_stages", 1.0f }, { "west_strike", 0.289397f }, { "master", -5.56f },
        };
        p.macroNames = { "REED", "FOLD", "TONE", "DIRT" };
        add (p);
    }
    // her hammered strings blown by a reed exciter instead: a pump organ (from "Hammered Strings Edited")
    {
        FactoryPreset p { "Reed Pump Organ", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_release", 0.181789f }, { "amp_sustain", 1.0f },
            { "amp_velocity", 0.974f }, { "f1_cutoff", 18000.0f }, { "f1_drive", 1.0f }, { "f1_env", 0.0f },
            { "f1_reso", 0.0f }, { "fx_limit_ceiling", -0.5f }, { "fx_reverb_mix", 0.16f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_size", 0.55f }, { "fx_slot1", 36.0f }, { "fx_slot1_mix", 0.4690038f }, { "fx_slot2", 13.0f },
            { "mech_damper", 0.25f }, { "mech_key", 0.25f }, { "mech_pedal", 0.25f }, { "mod1_amt", -0.4f },
            { "mod1_dst", 186.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.6f }, { "mod2_dst", 48.0f },
            { "mod2_src", 13.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 335.0f }, { "mod3_src", 14.0f },
            { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "osc1_couple", 0.11225f },
            { "osc1_damper", 0.0902858f }, { "osc1_detune", 0.9f }, { "osc1_excite", 8.0f }, { "osc1_hammer_hard", 0.739f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.0526428f }, { "osc1_spread", 0.35f },
            { "osc1_string_damp", 0.352f }, { "osc1_string_decay", 1.0f }, { "osc1_string_excite_pos", 0.221714f }, { "osc1_string_stiffness", 0.45f },
            { "osc1_string_sustain", 0.8334767f }, { "osc1_unison", 2.0f }, { "osc2_couple", 0.0289498f }, { "osc2_excite", 5.0f },
            { "osc2_mode", 1.0f }, { "osc2_semi", 12.0f }, { "osc2_string_damp", 0.866571f }, { "osc2_string_pickup", 0.278906f },
            { "osc2_string_slap", 1.0f }, { "osc2_unison", 4.0f }, { "pedal_res", 0.5f }, { "sb_mix", 0.786f },
            { "sb_on", 1.0f }, { "sb_size", 0.751f }, { "sb_tone", 0.0965f }, { "stretch", 0.8f },
            { "master", 7.23f },
        };
        p.macroNames = { "BREATH", "WIDTH", "BOARD", "SPACE" };
        add (p);
    }
    // the bowed string held forever into sympathetic strings and the resonator (from "phys_bowed pad")
    {
        FactoryPreset p { "Resonant Drone", {} };
        p.category = "Drone";
        p.values = {
            { "amp_attack", 2.492689f }, { "amp_release", 3.026832f }, { "amp_sustain", 0.514214f }, { "f1_cutoff", 20000.0f },
            { "fx_slot1", 34.0f }, { "fx_slot1_mix", 0.4358083f }, { "mod1_amt", 0.548f }, { "mod1_dst", 272.0f },
            { "mod1_src", 1.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 982.0f }, { "mod2_src", 12.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 270.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.4f },
            { "mod4_dst", 271.0f }, { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 99.0f },
            { "mod5_src", 15.0f }, { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_on", 0.0f }, { "osc1_table", 0.0f },
            { "osc2_bow_pressure", 0.543769f }, { "osc2_bow_speed", 0.844484f }, { "osc2_detune", 18.6045f }, { "osc2_excite", 4.0f },
            { "osc2_fb_type", 1.0f }, { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_register", 0.175076f },
            { "osc2_semi", -12.0f }, { "osc2_string_damp", 0.491758f }, { "osc2_string_decay", 0.819516f }, { "osc2_string_excite_pos", 0.0463437f },
            { "osc2_string_stiffness", 0.7312f }, { "osc2_string_sustain", 0.352727f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 8.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.5223363f },
            { "res_on", 1.0f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f }, { "subosc_level", 0.3f },
            { "sym_amount", 0.7504728f }, { "sym_on", 1.0f }, { "master", 1.68f },
        };
        p.macroNames = { "SYMPATHY", "BOW", "SPEED", "SPACE" };
        add (p);
    }
    // the bowed pad pressed into a dense soundboard and Airwindows spaces (from "phys_bowed pad")
    {
        FactoryPreset p { "Soundboard Bloom", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.155862f }, { "amp_release", 1.19077f }, { "amp_sustain", 0.514214f }, { "f1_cutoff", 20000.0f },
            { "fx_slot1", 34.0f }, { "fx_slot1_mix", 0.3463435f }, { "mod1_amt", 0.548f }, { "mod1_dst", 272.0f },
            { "mod1_src", 1.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 335.0f }, { "mod2_src", 12.0f },
            { "mod3_amt", 0.4f }, { "mod3_dst", 270.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.4f },
            { "mod4_dst", 271.0f }, { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 99.0f },
            { "mod5_src", 15.0f }, { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_on", 0.0f }, { "osc1_table", 0.0f },
            { "osc2_bow_pressure", 0.543769f }, { "osc2_bow_speed", 0.844484f }, { "osc2_detune", 18.6045f }, { "osc2_excite", 4.0f },
            { "osc2_fb_type", 1.0f }, { "osc2_mode", 1.0f }, { "osc2_on", 1.0f }, { "osc2_register", 0.175076f },
            { "osc2_semi", -12.0f }, { "osc2_string_damp", 0.491758f }, { "osc2_string_decay", 0.819516f }, { "osc2_string_excite_pos", 0.0463437f },
            { "osc2_string_stiffness", 0.7312f }, { "osc2_string_sustain", 0.352727f }, { "osc2_uni_mode", 1.0f }, { "osc2_unison", 8.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "sb_mix", 0.5133683f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.7421494f }, { "sub_fb_type", 1.0f },
            { "sub_shape", 1.0f }, { "subosc_level", 0.3f }, { "master", 4.78f },
        };
        p.macroNames = { "BOARD", "BOW", "SPEED", "SPACE" };
        add (p);
    }
    // her strummed pad as a harp, every note spraying a few more up the scale (from "Strummed Pad modded")
    {
        FactoryPreset p { "Spray Harp", {} };
        p.category = "Generative";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_decay", 1.367766f }, { "amp_release", 1.5f }, { "amp_sustain", 0.0f },
            { "f1_cutoff", 20000.0f }, { "f1_drive", 1.0f }, { "f1_env", 0.0f }, { "f1_reso", 0.0f },
            { "fx_chorus_mix", 0.3f }, { "fx_chorus_on", 1.0f }, { "fx_chorus_rate", 0.3f }, { "fx_reverb_mix", 0.3f },
            { "fx_reverb_on", 1.0f }, { "fx_reverb_size", 0.8f }, { "fx_reverb_type", 1.0f }, { "fx_slot1", 7.0f },
            { "fx_slot2", 13.0f }, { "gen_scale", 1.0f }, { "mod1_amt", -0.5f }, { "mod1_dst", 208.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.4f }, { "mod2_dst", 987.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "osc1_couple", 0.1f }, { "osc1_detune", 5.0f },
            { "osc1_level", 0.7f }, { "osc1_mode", 1.0f }, { "osc1_spread", 0.6f }, { "osc1_string_decay", 0.8f },
            { "osc1_unison", 2.0f }, { "osc2_fine", 0.0f }, { "osc2_frame", 0.4f }, { "osc2_level", 0.3f },
            { "osc2_on", 1.0f }, { "osc2_table", 118.0f }, { "spray_count", 2.0f }, { "spray_on", 1.0f },
            { "spray_spread", 173.9674f }, { "spray_strum", 1.0f }, { "spray_strum_time", 60.0f }, { "master", -6.93f },
        };
        p.macroNames = { "SPRAY", "STRUM", "CHORUS", "SPACE" };
        add (p);
    }
    // her strummed pad ringing sympathetic strings into a hall (from "Strummed Pad modded")
    {
        FactoryPreset p { "Sympathetic Haze", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.1319156f }, { "amp_decay", 3.0f }, { "amp_release", 1.5f }, { "amp_sustain", 1.0f },
            { "f1_cutoff", 20000.0f }, { "f1_drive", 1.0f }, { "f1_env", 0.0f }, { "f1_reso", 0.0f },
            { "fx_chorus_mix", 0.3f }, { "fx_chorus_on", 1.0f }, { "fx_chorus_rate", 0.3f }, { "fx_reverb_mix", 0.3f },
            { "fx_reverb_on", 1.0f }, { "fx_reverb_size", 0.8f }, { "fx_reverb_type", 1.0f }, { "fx_slot1", 7.0f },
            { "fx_slot2", 34.0f }, { "fx_slot2_mix", 0.3794016f }, { "mod1_amt", 0.5f }, { "mod1_dst", 982.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.4f }, { "mod2_dst", 987.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "osc1_couple", 0.1f }, { "osc1_detune", 5.0f },
            { "osc1_level", 0.7f }, { "osc1_mode", 1.0f }, { "osc1_spread", 0.6f }, { "osc1_string_decay", 0.8f },
            { "osc1_unison", 2.0f }, { "osc2_fine", 0.0f }, { "osc2_frame", 0.4f }, { "osc2_level", 0.3f },
            { "osc2_on", 1.0f }, { "osc2_table", 118.0f }, { "spray_strum", 1.0f }, { "spray_strum_time", 60.0f },
            { "sym_amount", 0.6873908f }, { "sym_decay", 0.776374f }, { "sym_on", 1.0f }, { "master", -6.96f },
        };
        p.macroNames = { "SYMPATHY", "STRUM", "CHORUS", "SPACE" };
        add (p);
    }
    // the piano's string and board with a tine exciter on a bar body (from "real piano SOUND")
    {
        FactoryPreset p { "Tine Room", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.477952f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 1.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot1_mix", 0.2464766f },
            { "fx_slot2", 37.0f }, { "master_clip", 0.0f }, { "mod1_amt", 0.5f }, { "mod1_dst", 257.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 396.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", -0.5f }, { "mod3_dst", 417.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 99.0f }, { "mod4_src", 15.0f }, { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f },
            { "osc1_excite", 7.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f },
            { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.15848f }, { "osc1_string_damp", 0.119763f },
            { "osc1_string_decay", 1.0f }, { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.204045f },
            { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f },
            { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f }, { "res_on", 1.0f },
            { "sb_mix", 0.355458f }, { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.842771f },
            { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f },
            { "west_fold", 0.432153f }, { "west_on", 1.0f }, { "west_open", 0.8665079f }, { "master", -5.3f },
        };
        p.macroNames = { "BELL", "BODY", "WOOL", "SPACE" };
        add (p);
    }
    // a small high piano on a plate body, through tape and lo-fi (from "real piano SOUND")
    {
        FactoryPreset p { "Toy Box Piano", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "body_type", 2.0f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f },
            { "fx_awverb_algo", 4.0f }, { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f },
            { "fx_awverb_p3", 0.276992f }, { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 40.0f },
            { "fx_slot2_mix", 0.4182784f }, { "fx_slot3", 32.0f }, { "fx_slot3_mix", 0.4332937f }, { "master_clip", 0.0f },
            { "mod1_amt", 0.6f }, { "mod1_dst", 100.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 101.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 253.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f }, { "mod4_src", 15.0f },
            { "osc1_couple", 0.127451f }, { "osc1_damper", 0.541f }, { "osc1_excite", 9.0f }, { "osc1_fb_type", 1.0f },
            { "osc1_frame", 0.5f }, { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f },
            { "osc1_register", 0.968349f }, { "osc1_string_damp", 0.119763f }, { "osc1_string_decay", 0.6041727f }, { "osc1_string_excite_pos", 0.739229f },
            { "osc1_string_slap", 1.0f }, { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "res_amount", 0.607374f }, { "res_on", 1.0f }, { "sb_mix", 0.355458f }, { "sb_model", 1.0f },
            { "sb_size", 0.842771f }, { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f }, { "subosc_on", 1.0f },
            { "voice_spread", 0.287019f }, { "west_fold", 0.432153f }, { "west_on", 1.0f }, { "west_open", 0.712386f },
            { "master", -1.32f },
        };
        p.macroNames = { "LO-FI", "TAPE", "HAMMER", "SPACE" };
        add (p);
    }
    // the piano string plucked low and damped: an upright bass (from "real piano SOUND")
    {
        FactoryPreset p { "Upright Thump", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.3f }, { "body_coupling", 0.36755f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.937459f }, { "f1_cutoff", 20000.0f }, { "fx_awdyn_p1", 0.454275f }, { "fx_awverb_algo", 4.0f },
            { "fx_awverb_mix", 0.244251f }, { "fx_awverb_p1", 1.0f }, { "fx_awverb_p2", 0.419172f }, { "fx_awverb_p3", 0.276992f },
            { "fx_awverb_p4", 0.5f }, { "fx_slot1", 34.0f }, { "fx_slot2", 37.0f }, { "master_clip", 0.0f },
            { "mod1_amt", 0.5f }, { "mod1_dst", 185.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 396.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 417.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f }, { "mod4_src", 15.0f },
            { "osc1_couple", 0.127451f }, { "osc1_damper", 0.6935406f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f },
            { "osc1_hammer_hard", 1.0f }, { "osc1_level", 1.0f }, { "osc1_mode", 1.0f }, { "osc1_register", 0.1532056f },
            { "osc1_string_damp", 0.6649029f }, { "osc1_string_decay", 1.0f }, { "osc1_string_excite_pos", 0.739229f }, { "osc1_string_slap", 1.0f },
            { "osc1_string_stiffness", 0.524481f }, { "osc1_string_sustain", 0.507962f }, { "osc1_table", 0.0f }, { "osc2_fb_type", 1.0f },
            { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "res_amount", 0.607374f },
            { "res_on", 1.0f }, { "sb_mix", 0.355458f }, { "sb_model", 1.0f }, { "sb_size", 0.842771f },
            { "sub_fb_type", 1.0f }, { "subosc_level", 0.128614f }, { "subosc_on", 1.0f }, { "voice_spread", 0.287019f },
            { "west_fold", 0.432153f }, { "west_on", 1.0f }, { "west_open", 0.5077671f }, { "master", -3.54f },
        };
        p.macroNames = { "THUMP", "WOOD", "OPEN", "ROOM" };
        add (p);
    }
    // her trance pad made polyphonic, its vector path wandering slower (from "trancey pad")
    {
        FactoryPreset p { "Vector Drift Pad", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.003f }, { "amp_release", 0.648308f }, { "drift", 0.481143f }, { "f1_cutoff", 607.7957f },
            { "f1_drive", 2.13772f }, { "f1_keytrack", 1.0f }, { "f1_morph", 0.156571f }, { "f1_reso", 0.423963f },
            { "f1_type", 37.0f }, { "fx_awtape_p1", 0.359729f }, { "fx_awverb_mix", 0.180106f }, { "fx_gate_div", 5.0f },
            { "fx_gate_pattern", 8.0f }, { "fx_gate_step10", 0.0f }, { "fx_gate_step12", 0.0f }, { "fx_gate_step14", 0.0f },
            { "fx_gate_step2", 0.0f }, { "fx_gate_step4", 0.0f }, { "fx_gate_step8", 0.0f }, { "fx_slot1", 34.0f },
            { "fx_slot2", 11.0f }, { "fx_slot3", 32.0f }, { "fx_slot4", 16.0f }, { "fx_slot4_bypass", 1.0f },
            { "fx_smear_density", 18.006f }, { "fx_smear_mix", 0.306386f }, { "fx_smear_on", 1.0f }, { "glide_legato", 1.0f },
            { "lfo1_div", 4.0f }, { "lfo1_shape", 3.0f }, { "lfo1_sync", 1.0f }, { "master_clip", 0.0f },
            { "mod1_amt", 0.5f }, { "mod1_dst", 425.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f },
            { "mod2_dst", 9.0f }, { "mod2_src", 13.0f }, { "mod3_amt", 0.4f }, { "mod3_dst", 414.0f },
            { "mod3_src", 14.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 99.0f }, { "mod4_src", 15.0f },
            { "osc1_chord", 1.0f }, { "osc1_detune", 27.4571f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.5f },
            { "osc1_level", 1.0f }, { "osc1_spread", 1.0f }, { "osc1_table", 104.0f }, { "osc1_uni_mode", 1.0f },
            { "osc1_unison", 7.0f }, { "osc2_chord", 1.0f }, { "osc2_detune", 33.6286f }, { "osc2_fb_type", 1.0f },
            { "osc2_frame", 0.571429f }, { "osc2_on", 1.0f }, { "osc2_table", 62.0f }, { "osc2_uni_mode", 1.0f },
            { "osc2_unison", 6.0f }, { "osc4_excite", 7.0f }, { "osc4_fb_type", 1.0f }, { "osc4_mode", 1.0f },
            { "osc4_on", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f }, { "poly_voices", 16.0f },
            { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.964f }, { "sub_chord", 1.0f },
            { "sub_detune", 21.6f }, { "sub_fb_type", 1.0f }, { "sub_frame", 0.589011f }, { "sub_on", 1.0f },
            { "sub_shape", 1.0f }, { "sub_table", 26.0f }, { "sub_uni_mode", 1.0f }, { "sub_unison", 11.0f },
            { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "sym_amount", 0.776571f }, { "vec_drift", 1.0f },
            { "vec_drift_rate", 0.496205f }, { "vec_on", 1.0f }, { "vec_path", 1.0f }, { "vec_px1", 1.0f },
            { "vec_px2", 1.0f }, { "vec_px3", 1.0f }, { "vec_px4", 0.0f }, { "vec_px5", 0.0f },
            { "vec_px6", 0.935484f }, { "vec_px7", 0.0f }, { "vec_px8", 0.541786f }, { "vec_py1", 0.444547f },
            { "vec_py2", 0.0f }, { "vec_py3", 1.0f }, { "vec_py4", 0.0f }, { "vec_py5", 0.535218f },
            { "vec_py6", 0.696467f }, { "vec_py7", 1.0f }, { "vec_py8", 0.300682f }, { "vec_rate", 0.232168f },
            { "vec_x", 0.518443f }, { "vec_y", 0.491868f }, { "voice_spread", 0.395428f }, { "west_decay", 1.10662f },
            { "west_fold", 0.508886f }, { "west_on", 1.0f }, { "west_open", 0.569107f }, { "west_res", 0.403492f },
            { "west_src", 1.0f }, { "west_stages", 3.0f }, { "west_strike", 0.925759f }, { "west_sym", 1.0f },
            { "master", -3.99f },
        };
        p.macroNames = { "DRIFT", "CUTOFF", "FOLD", "SPACE" };
        add (p);
    }
    // her retro bass folded by the WEST voice (from "Retro Bass")
    {
        FactoryPreset p { "West Fold Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_decay", 2.39761f }, { "amp_release", 0.6047f }, { "amp_sustain", 0.0f },
            { "arp_gate", 0.5003f }, { "e4_attack", 0.05099f }, { "e4_decay", 0.399605f }, { "e4_release", 0.30485f },
            { "env10_attack", 0.05099f }, { "env10_decay", 0.399605f }, { "env10_release", 0.30485f }, { "env11_attack", 0.05099f },
            { "env11_decay", 0.399605f }, { "env11_release", 0.30485f }, { "env12_attack", 0.05099f }, { "env12_decay", 0.399605f },
            { "env12_release", 0.30485f }, { "env13_attack", 0.05099f }, { "env13_decay", 0.399605f }, { "env13_release", 0.30485f },
            { "env14_attack", 0.05099f }, { "env14_decay", 0.399605f }, { "env14_release", 0.30485f }, { "env15_attack", 0.05099f },
            { "env15_decay", 0.399605f }, { "env15_release", 0.30485f }, { "env16_attack", 0.05099f }, { "env16_decay", 0.399605f },
            { "env16_release", 0.30485f }, { "env6_attack", 0.05099f }, { "env6_decay", 0.399605f }, { "env6_release", 0.30485f },
            { "env7_attack", 0.05099f }, { "env7_decay", 0.399605f }, { "env7_release", 0.30485f }, { "env8_attack", 0.05099f },
            { "env8_decay", 0.399605f }, { "env8_release", 0.30485f }, { "env9_attack", 0.05099f }, { "env9_decay", 0.399605f },
            { "env9_release", 0.30485f }, { "f1_cutoff", 1798.22f }, { "f1_drive", 1.0f }, { "f1_env", 2.0f },
            { "f1_keytrack", 0.5f }, { "f1_reso", 0.1f }, { "f1_type", 4.0f }, { "f2e_attack", 0.010998f },
            { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f }, { "fe_decay", 0.599405f }, { "fe_release", 0.50475f },
            { "fe_sustain", 0.1f }, { "filter_velocity", 0.8f }, { "fx_amp_drive", 4.002f }, { "fx_chorus_depth", 0.15f },
            { "fx_chorus_mix", 0.2f }, { "fx_chorus_on", 1.0f }, { "fx_chorus_rate", 0.3998f }, { "fx_comb_feedback", 0.60043f },
            { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f }, { "fx_comp_release", 149.59f },
            { "fx_delay_feedback", 0.3496f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f }, { "fx_dim_rate", 0.40155f },
            { "fx_drive_amount", 2.995f }, { "fx_eq_high_freq", 5998.0f }, { "fx_eq_low_freq", 119.84f }, { "fx_eq_mid_freq", 1004.4f },
            { "fx_eq_mid_q", 0.902f }, { "fx_feedback_amount", 0.6004f }, { "fx_feedback_delay", 11.989f }, { "fx_flanger_feedback", 0.5004f },
            { "fx_flanger_rate", 0.50315f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f }, { "fx_limit_release", 119.89f },
            { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_reverb_mix", 0.18f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_size", 0.55f }, { "fx_reverb_type", 1.0f }, { "fx_ring_freq", 220.956f }, { "fx_slot1", 7.0f },
            { "fx_slot2", 13.0f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f }, { "fx_tape_stop_time", 0.59905f },
            { "fx_trem_rate", 4.0001f }, { "glide", 0.002f }, { "glide_legato", 1.0f }, { "lfo10_rate", 4.009f },
            { "lfo10_step10", -0.382f }, { "lfo10_step11", -0.708f }, { "lfo10_step12", -0.924f }, { "lfo10_step14", -0.924f },
            { "lfo10_step15", -0.708f }, { "lfo10_step16", -0.382f }, { "lfo10_step2", 0.382f }, { "lfo10_step3", 0.708f },
            { "lfo10_step4", 0.924f }, { "lfo10_step6", 0.924f }, { "lfo10_step7", 0.708f }, { "lfo10_step8", 0.382f },
            { "lfo11_rate", 4.009f }, { "lfo11_step10", -0.382f }, { "lfo11_step11", -0.708f }, { "lfo11_step12", -0.924f },
            { "lfo11_step14", -0.924f }, { "lfo11_step15", -0.708f }, { "lfo11_step16", -0.382f }, { "lfo11_step2", 0.382f },
            { "lfo11_step3", 0.708f }, { "lfo11_step4", 0.924f }, { "lfo11_step6", 0.924f }, { "lfo11_step7", 0.708f },
            { "lfo11_step8", 0.382f }, { "lfo12_rate", 4.009f }, { "lfo12_step10", -0.382f }, { "lfo12_step11", -0.708f },
            { "lfo12_step12", -0.924f }, { "lfo12_step14", -0.924f }, { "lfo12_step15", -0.708f }, { "lfo12_step16", -0.382f },
            { "lfo12_step2", 0.382f }, { "lfo12_step3", 0.708f }, { "lfo12_step4", 0.924f }, { "lfo12_step6", 0.924f },
            { "lfo12_step7", 0.708f }, { "lfo12_step8", 0.382f }, { "lfo13_rate", 4.009f }, { "lfo13_step10", -0.382f },
            { "lfo13_step11", -0.708f }, { "lfo13_step12", -0.924f }, { "lfo13_step14", -0.924f }, { "lfo13_step15", -0.708f },
            { "lfo13_step16", -0.382f }, { "lfo13_step2", 0.382f }, { "lfo13_step3", 0.708f }, { "lfo13_step4", 0.924f },
            { "lfo13_step6", 0.924f }, { "lfo13_step7", 0.708f }, { "lfo13_step8", 0.382f }, { "lfo14_rate", 4.009f },
            { "lfo14_step10", -0.382f }, { "lfo14_step11", -0.708f }, { "lfo14_step12", -0.924f }, { "lfo14_step14", -0.924f },
            { "lfo14_step15", -0.708f }, { "lfo14_step16", -0.382f }, { "lfo14_step2", 0.382f }, { "lfo14_step3", 0.708f },
            { "lfo14_step4", 0.924f }, { "lfo14_step6", 0.924f }, { "lfo14_step7", 0.708f }, { "lfo14_step8", 0.382f },
            { "lfo15_rate", 4.009f }, { "lfo15_step10", -0.382f }, { "lfo15_step11", -0.708f }, { "lfo15_step12", -0.924f },
            { "lfo15_step14", -0.924f }, { "lfo15_step15", -0.708f }, { "lfo15_step16", -0.382f }, { "lfo15_step2", 0.382f },
            { "lfo15_step3", 0.708f }, { "lfo15_step4", 0.924f }, { "lfo15_step6", 0.924f }, { "lfo15_step7", 0.708f },
            { "lfo15_step8", 0.382f }, { "lfo16_rate", 4.009f }, { "lfo16_step10", -0.382f }, { "lfo16_step11", -0.708f },
            { "lfo16_step12", -0.924f }, { "lfo16_step14", -0.924f }, { "lfo16_step15", -0.708f }, { "lfo16_step16", -0.382f },
            { "lfo16_step2", 0.382f }, { "lfo16_step3", 0.708f }, { "lfo16_step4", 0.924f }, { "lfo16_step6", 0.924f },
            { "lfo16_step7", 0.708f }, { "lfo16_step8", 0.382f }, { "lfo1_rate", 4.009f }, { "lfo1_step10", -0.382f },
            { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f }, { "lfo1_step15", -0.708f },
            { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f }, { "lfo1_step4", 0.924f },
            { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f }, { "lfo2_rate", 4.009f },
            { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f }, { "lfo2_step14", -0.924f },
            { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f }, { "lfo2_step3", 0.708f },
            { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f }, { "lfo2_step8", 0.382f },
            { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f }, { "lfo3_step12", -0.924f },
            { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f }, { "lfo3_step2", 0.382f },
            { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f }, { "lfo3_step7", 0.708f },
            { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f }, { "lfo4_step11", -0.708f },
            { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f }, { "lfo4_step16", -0.382f },
            { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f }, { "lfo4_step6", 0.924f },
            { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f }, { "lfo5_rate", 4.009f }, { "lfo5_step10", -0.382f },
            { "lfo5_step11", -0.708f }, { "lfo5_step12", -0.924f }, { "lfo5_step14", -0.924f }, { "lfo5_step15", -0.708f },
            { "lfo5_step16", -0.382f }, { "lfo5_step2", 0.382f }, { "lfo5_step3", 0.708f }, { "lfo5_step4", 0.924f },
            { "lfo5_step6", 0.924f }, { "lfo5_step7", 0.708f }, { "lfo5_step8", 0.382f }, { "lfo6_rate", 4.009f },
            { "lfo6_step10", -0.382f }, { "lfo6_step11", -0.708f }, { "lfo6_step12", -0.924f }, { "lfo6_step14", -0.924f },
            { "lfo6_step15", -0.708f }, { "lfo6_step16", -0.382f }, { "lfo6_step2", 0.382f }, { "lfo6_step3", 0.708f },
            { "lfo6_step4", 0.924f }, { "lfo6_step6", 0.924f }, { "lfo6_step7", 0.708f }, { "lfo6_step8", 0.382f },
            { "lfo7_rate", 4.009f }, { "lfo7_step10", -0.382f }, { "lfo7_step11", -0.708f }, { "lfo7_step12", -0.924f },
            { "lfo7_step14", -0.924f }, { "lfo7_step15", -0.708f }, { "lfo7_step16", -0.382f }, { "lfo7_step2", 0.382f },
            { "lfo7_step3", 0.708f }, { "lfo7_step4", 0.924f }, { "lfo7_step6", 0.924f }, { "lfo7_step7", 0.708f },
            { "lfo7_step8", 0.382f }, { "lfo8_rate", 4.009f }, { "lfo8_step10", -0.382f }, { "lfo8_step11", -0.708f },
            { "lfo8_step12", -0.924f }, { "lfo8_step14", -0.924f }, { "lfo8_step15", -0.708f }, { "lfo8_step16", -0.382f },
            { "lfo8_step2", 0.382f }, { "lfo8_step3", 0.708f }, { "lfo8_step4", 0.924f }, { "lfo8_step6", 0.924f },
            { "lfo8_step7", 0.708f }, { "lfo8_step8", 0.382f }, { "lfo9_rate", 4.009f }, { "lfo9_step10", -0.382f },
            { "lfo9_step11", -0.708f }, { "lfo9_step12", -0.924f }, { "lfo9_step14", -0.924f }, { "lfo9_step15", -0.708f },
            { "lfo9_step16", -0.382f }, { "lfo9_step2", 0.382f }, { "lfo9_step3", 0.708f }, { "lfo9_step4", 0.924f },
            { "lfo9_step6", 0.924f }, { "lfo9_step7", 0.708f }, { "lfo9_step8", 0.382f }, { "me_attack", 0.05099f },
            { "me_decay", 0.399605f }, { "me_release", 0.30485f }, { "mod1_amt", 0.5f }, { "mod1_dst", 414.0f },
            { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 417.0f }, { "mod2_src", 13.0f },
            { "mod3_amt", 0.5f }, { "mod3_dst", 9.0f }, { "mod3_src", 14.0f }, { "mod4_amt", 0.5f },
            { "mod4_dst", 99.0f }, { "mod4_src", 15.0f }, { "mseg_rate", 0.4988f }, { "mseg_time1", 0.25045f },
            { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f }, { "osc1_frame", 0.2f },
            { "osc1_grain_size", 80.07f }, { "osc1_level", 0.6f }, { "osc1_table", 10.0f }, { "osc2_fine", 0.0f },
            { "osc2_grain_size", 80.07f }, { "osc2_level", 0.4f }, { "osc2_on", 1.0f }, { "osc2_semi", 12.0f },
            { "osc2_table", 9.0f }, { "osc4_grain_size", 80.07f }, { "osc5_grain_size", 80.07f }, { "osc6_grain_size", 80.07f },
            { "poly_voices", 1.0f }, { "sub_grain_size", 80.07f }, { "sub_shape", 2.0f }, { "subosc_on", 1.0f },
            { "voice_mode", 2.0f }, { "west_decay", 0.6793051f }, { "west_fold", 0.4748677f }, { "west_mode", 1.0f },
            { "west_on", 1.0f }, { "west_open", 0.5709052f }, { "master", -13.18f },
        };
        p.macroNames = { "FOLD", "OPEN", "CUTOFF", "CHORUS" };
        add (p);
    }
    // her brass, slow and polyphonic, glowing through the west gate and an ensemble (from "classic synth brass, no fx")
    {
        FactoryPreset p { "West Glow Pad", {} };
        p.category = "Pad";
        p.values = {
            { "amp_attack", 0.483059f }, { "amp_release", 0.8196958f }, { "amp_sustain", 0.8764631f }, { "body_coupling_mode", 1.0f },
            { "body_material", 0.244851f }, { "body_type", 2.0f }, { "drift", 0.293017f }, { "f1_cutoff", 176.638f },
            { "f1_drive", 2.00576f }, { "f1_env", 5.0f }, { "f1_type", 29.0f }, { "fe_attack", 0.5622548f },
            { "fx_awverb_algo", 2.0f }, { "fx_awverb_mix", 0.24538f }, { "fx_awverb_p1", 1.0f }, { "fx_slot1", 36.0f },
            { "fx_slot1_mix", 0.4600787f }, { "fx_slot2", 34.0f }, { "fx_slot2_mix", 0.2671571f }, { "fx_slot3", 20.0f },
            { "master_clip", 0.0f }, { "mod1_amt", 0.183048f }, { "mod1_dst", 9.0f }, { "mod1_src", 6.0f },
            { "mod2_amt", 0.5f }, { "mod2_dst", 417.0f }, { "mod2_src", 12.0f }, { "mod3_amt", 0.4f },
            { "mod3_dst", 414.0f }, { "mod3_src", 13.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 9.0f },
            { "mod4_src", 14.0f }, { "mod5_amt", 0.5f }, { "mod5_dst", 100.0f }, { "mod5_src", 15.0f },
            { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f }, { "osc1_level", 1.0f }, { "osc1_table", 0.0f },
            { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f }, { "osc6_fb_type", 1.0f },
            { "poly_voices", 16.0f }, { "res_amount", 0.445812f }, { "res_on", 1.0f }, { "sb_model", 1.0f },
            { "sb_on", 1.0f }, { "sb_size", 0.740836f }, { "sub_fb_type", 1.0f }, { "sub_shape", 1.0f },
            { "subosc_level", 0.3f }, { "subosc_on", 1.0f }, { "voice_spread", 0.523819f }, { "west_fold", 0.277923f },
            { "west_on", 1.0f }, { "west_open", 0.527113f }, { "west_res", 0.322425f }, { "west_src", 4.0f },
            { "west_stages", 3.0f }, { "west_strike", 0.564488f }, { "master", -12.91f },
        };
        p.macroNames = { "GLOW", "FOLD", "TONE", "SPACE" };
        add (p);
    }
    // her caribbean keys as a struck low-pass gate on a bar body (from "caribbean keys")
    {
        FactoryPreset p { "West Marimba", {} };
        p.category = "Keys";
        p.values = {
            { "amp_attack", 0.0178825f }, { "amp_decay", 0.275633f }, { "amp_release", 0.546537f }, { "amp_sustain", 0.218845f },
            { "body_coupling", 0.4811948f }, { "body_coupling_mode", 1.0f }, { "body_type", 1.0f }, { "f1_cutoff", 196.057f },
            { "f1_env", 5.0f }, { "f1_keytrack", 1.0f }, { "f1_reso", 0.328883f }, { "f2_cutoff", 20.0f },
            { "f2_keytrack", 1.0f }, { "f2_reso", 1.0f }, { "f2_type", 8.0f }, { "fe_dcurve", -1.0f },
            { "fe_decay", 0.902634f }, { "fe_release", 0.005f }, { "fe_sustain", 0.0f }, { "fx_chorus_on", 1.0f },
            { "fx_delay_on", 1.0f }, { "fx_delay_time", 17.7332f }, { "fx_eq_low_freq", 77.3628f }, { "fx_eq_low_gain", -9.28034f },
            { "fx_eq_mid_freq", 2868.41f }, { "fx_eq_mid_gain", 10.7732f }, { "fx_ott_amount", 0.595381f }, { "fx_reverb_on", 1.0f },
            { "fx_reverb_type", 2.0f }, { "fx_slot1", 7.0f }, { "fx_slot2", 9.0f }, { "fx_slot3", 20.0f },
            { "fx_slot4", 29.0f }, { "fx_slot5", 13.0f }, { "master_clip", 0.0f }, { "mech_pedal", 1.0f },
            { "mod1_amt", -0.482f }, { "mod1_dst", 925.0f }, { "mod1_src", 6.0f }, { "mod2_amt", 0.722f },
            { "mod2_dst", 3.0f }, { "mod2_src", 6.0f }, { "mod3_amt", 0.956f }, { "mod3_dst", 2.0f },
            { "mod3_src", 6.0f }, { "mod4_amt", 0.5f }, { "mod4_dst", 414.0f }, { "mod4_src", 12.0f },
            { "mod5_amt", 0.5f }, { "mod5_dst", 417.0f }, { "mod5_src", 13.0f }, { "mod6_amt", 0.4f },
            { "mod6_dst", 396.0f }, { "mod6_src", 14.0f }, { "mod7_amt", 0.5f }, { "mod7_dst", 103.0f },
            { "mod7_src", 15.0f }, { "osc1_detune", 0.0f }, { "osc1_fb_type", 1.0f }, { "osc1_frame", 0.0f },
            { "osc1_level", 0.0f }, { "osc1_sample_fade_in", 0.437644f }, { "osc1_sample_start", 1.0f }, { "osc1_table", 60.0f },
            { "osc1_uni_mode", 3.0f }, { "osc2_fb_type", 1.0f }, { "osc4_fb_type", 1.0f }, { "osc5_fb_type", 1.0f },
            { "osc6_fb_type", 1.0f }, { "pedal_res", 0.430505f }, { "res_amount", 0.781723f }, { "res_on", 1.0f },
            { "sb_mix", 0.0f }, { "sb_model", 1.0f }, { "sb_on", 1.0f }, { "sb_size", 0.7323f },
            { "sub_fb_type", 1.0f }, { "subosc_level", 0.770852f }, { "subosc_on", 1.0f }, { "voice_spread", 0.382004f },
            { "west_decay", 0.6329722f }, { "west_fold", 0.216855f }, { "west_on", 1.0f }, { "west_open", 0.728625f },
            { "west_res", 0.774146f }, { "west_stages", 4.0f }, { "west_strike", 0.8020856f }, { "master", -4.41f },
        };
        p.macroNames = { "FOLD", "OPEN", "BODY", "SPACE" };
        add (p);
    }
    // her retro bass on the Airwindows Z low pass, a short legato glide (from "Retro Bass")
    {
        FactoryPreset p { "Z Ladder Bass", {} };
        p.category = "Bass";
        p.values = {
            { "amp_attack", 0.001f }, { "amp_decay", 2.39761f }, { "amp_release", 0.6047f }, { "amp_sustain", 0.0f },
            { "arp_gate", 0.5003f }, { "e4_attack", 0.05099f }, { "e4_decay", 0.399605f }, { "e4_release", 0.30485f },
            { "env10_attack", 0.05099f }, { "env10_decay", 0.399605f }, { "env10_release", 0.30485f }, { "env11_attack", 0.05099f },
            { "env11_decay", 0.399605f }, { "env11_release", 0.30485f }, { "env12_attack", 0.05099f }, { "env12_decay", 0.399605f },
            { "env12_release", 0.30485f }, { "env13_attack", 0.05099f }, { "env13_decay", 0.399605f }, { "env13_release", 0.30485f },
            { "env14_attack", 0.05099f }, { "env14_decay", 0.399605f }, { "env14_release", 0.30485f }, { "env15_attack", 0.05099f },
            { "env15_decay", 0.399605f }, { "env15_release", 0.30485f }, { "env16_attack", 0.05099f }, { "env16_decay", 0.399605f },
            { "env16_release", 0.30485f }, { "env6_attack", 0.05099f }, { "env6_decay", 0.399605f }, { "env6_release", 0.30485f },
            { "env7_attack", 0.05099f }, { "env7_decay", 0.399605f }, { "env7_release", 0.30485f }, { "env8_attack", 0.05099f },
            { "env8_decay", 0.399605f }, { "env8_release", 0.30485f }, { "env9_attack", 0.05099f }, { "env9_decay", 0.399605f },
            { "env9_release", 0.30485f }, { "f1_cutoff", 2195.168f }, { "f1_drive", 1.0f }, { "f1_env", 2.0f },
            { "f1_keytrack", 0.5f }, { "f1_morph", 0.2476747f }, { "f1_reso", 0.389856f }, { "f1_type", 29.0f },
            { "f2e_attack", 0.010998f }, { "f2e_decay", 0.399605f }, { "f2e_release", 0.30485f }, { "fe_decay", 0.599405f },
            { "fe_release", 0.50475f }, { "fe_sustain", 0.1f }, { "filter_velocity", 0.8f }, { "fx_amp_drive", 4.002f },
            { "fx_chorus_depth", 0.15f }, { "fx_chorus_mix", 0.2f }, { "fx_chorus_on", 1.0f }, { "fx_chorus_rate", 0.3998f },
            { "fx_comb_feedback", 0.60043f }, { "fx_comb_freq", 219.98f }, { "fx_comp_attack", 9.9901f }, { "fx_comp_ratio", 4.002f },
            { "fx_comp_release", 149.59f }, { "fx_delay_feedback", 0.3496f }, { "fx_delay_time", 374.813f }, { "fx_delay_time_r", 500.75f },
            { "fx_dim_rate", 0.40155f }, { "fx_drive_amount", 2.995f }, { "fx_eq_high_freq", 5998.0f }, { "fx_eq_low_freq", 119.84f },
            { "fx_eq_mid_freq", 1004.4f }, { "fx_eq_mid_q", 0.902f }, { "fx_feedback_amount", 0.6004f }, { "fx_feedback_delay", 11.989f },
            { "fx_flanger_feedback", 0.5004f }, { "fx_flanger_rate", 0.50315f }, { "fx_haas_delay", 13.987f }, { "fx_limit_ceiling", -0.288f },
            { "fx_limit_release", 119.89f }, { "fx_phaser_feedback", 0.3996f }, { "fx_phaser_rate", 0.50315f }, { "fx_reverb_mix", 0.18f },
            { "fx_reverb_on", 1.0f }, { "fx_reverb_size", 0.55f }, { "fx_reverb_type", 1.0f }, { "fx_ring_freq", 220.956f },
            { "fx_slot1", 7.0f }, { "fx_slot2", 13.0f }, { "fx_smear_density", 11.994f }, { "fx_smear_size", 119.96f },
            { "fx_tape_stop_time", 0.59905f }, { "fx_trem_rate", 4.0001f }, { "glide", 0.003f }, { "glide_legato", 1.0f },
            { "lfo10_rate", 4.009f }, { "lfo10_step10", -0.382f }, { "lfo10_step11", -0.708f }, { "lfo10_step12", -0.924f },
            { "lfo10_step14", -0.924f }, { "lfo10_step15", -0.708f }, { "lfo10_step16", -0.382f }, { "lfo10_step2", 0.382f },
            { "lfo10_step3", 0.708f }, { "lfo10_step4", 0.924f }, { "lfo10_step6", 0.924f }, { "lfo10_step7", 0.708f },
            { "lfo10_step8", 0.382f }, { "lfo11_rate", 4.009f }, { "lfo11_step10", -0.382f }, { "lfo11_step11", -0.708f },
            { "lfo11_step12", -0.924f }, { "lfo11_step14", -0.924f }, { "lfo11_step15", -0.708f }, { "lfo11_step16", -0.382f },
            { "lfo11_step2", 0.382f }, { "lfo11_step3", 0.708f }, { "lfo11_step4", 0.924f }, { "lfo11_step6", 0.924f },
            { "lfo11_step7", 0.708f }, { "lfo11_step8", 0.382f }, { "lfo12_rate", 4.009f }, { "lfo12_step10", -0.382f },
            { "lfo12_step11", -0.708f }, { "lfo12_step12", -0.924f }, { "lfo12_step14", -0.924f }, { "lfo12_step15", -0.708f },
            { "lfo12_step16", -0.382f }, { "lfo12_step2", 0.382f }, { "lfo12_step3", 0.708f }, { "lfo12_step4", 0.924f },
            { "lfo12_step6", 0.924f }, { "lfo12_step7", 0.708f }, { "lfo12_step8", 0.382f }, { "lfo13_rate", 4.009f },
            { "lfo13_step10", -0.382f }, { "lfo13_step11", -0.708f }, { "lfo13_step12", -0.924f }, { "lfo13_step14", -0.924f },
            { "lfo13_step15", -0.708f }, { "lfo13_step16", -0.382f }, { "lfo13_step2", 0.382f }, { "lfo13_step3", 0.708f },
            { "lfo13_step4", 0.924f }, { "lfo13_step6", 0.924f }, { "lfo13_step7", 0.708f }, { "lfo13_step8", 0.382f },
            { "lfo14_rate", 4.009f }, { "lfo14_step10", -0.382f }, { "lfo14_step11", -0.708f }, { "lfo14_step12", -0.924f },
            { "lfo14_step14", -0.924f }, { "lfo14_step15", -0.708f }, { "lfo14_step16", -0.382f }, { "lfo14_step2", 0.382f },
            { "lfo14_step3", 0.708f }, { "lfo14_step4", 0.924f }, { "lfo14_step6", 0.924f }, { "lfo14_step7", 0.708f },
            { "lfo14_step8", 0.382f }, { "lfo15_rate", 4.009f }, { "lfo15_step10", -0.382f }, { "lfo15_step11", -0.708f },
            { "lfo15_step12", -0.924f }, { "lfo15_step14", -0.924f }, { "lfo15_step15", -0.708f }, { "lfo15_step16", -0.382f },
            { "lfo15_step2", 0.382f }, { "lfo15_step3", 0.708f }, { "lfo15_step4", 0.924f }, { "lfo15_step6", 0.924f },
            { "lfo15_step7", 0.708f }, { "lfo15_step8", 0.382f }, { "lfo16_rate", 4.009f }, { "lfo16_step10", -0.382f },
            { "lfo16_step11", -0.708f }, { "lfo16_step12", -0.924f }, { "lfo16_step14", -0.924f }, { "lfo16_step15", -0.708f },
            { "lfo16_step16", -0.382f }, { "lfo16_step2", 0.382f }, { "lfo16_step3", 0.708f }, { "lfo16_step4", 0.924f },
            { "lfo16_step6", 0.924f }, { "lfo16_step7", 0.708f }, { "lfo16_step8", 0.382f }, { "lfo1_rate", 4.009f },
            { "lfo1_step10", -0.382f }, { "lfo1_step11", -0.708f }, { "lfo1_step12", -0.924f }, { "lfo1_step14", -0.924f },
            { "lfo1_step15", -0.708f }, { "lfo1_step16", -0.382f }, { "lfo1_step2", 0.382f }, { "lfo1_step3", 0.708f },
            { "lfo1_step4", 0.924f }, { "lfo1_step6", 0.924f }, { "lfo1_step7", 0.708f }, { "lfo1_step8", 0.382f },
            { "lfo2_rate", 4.009f }, { "lfo2_step10", -0.382f }, { "lfo2_step11", -0.708f }, { "lfo2_step12", -0.924f },
            { "lfo2_step14", -0.924f }, { "lfo2_step15", -0.708f }, { "lfo2_step16", -0.382f }, { "lfo2_step2", 0.382f },
            { "lfo2_step3", 0.708f }, { "lfo2_step4", 0.924f }, { "lfo2_step6", 0.924f }, { "lfo2_step7", 0.708f },
            { "lfo2_step8", 0.382f }, { "lfo3_rate", 4.009f }, { "lfo3_step10", -0.382f }, { "lfo3_step11", -0.708f },
            { "lfo3_step12", -0.924f }, { "lfo3_step14", -0.924f }, { "lfo3_step15", -0.708f }, { "lfo3_step16", -0.382f },
            { "lfo3_step2", 0.382f }, { "lfo3_step3", 0.708f }, { "lfo3_step4", 0.924f }, { "lfo3_step6", 0.924f },
            { "lfo3_step7", 0.708f }, { "lfo3_step8", 0.382f }, { "lfo4_rate", 4.009f }, { "lfo4_step10", -0.382f },
            { "lfo4_step11", -0.708f }, { "lfo4_step12", -0.924f }, { "lfo4_step14", -0.924f }, { "lfo4_step15", -0.708f },
            { "lfo4_step16", -0.382f }, { "lfo4_step2", 0.382f }, { "lfo4_step3", 0.708f }, { "lfo4_step4", 0.924f },
            { "lfo4_step6", 0.924f }, { "lfo4_step7", 0.708f }, { "lfo4_step8", 0.382f }, { "lfo5_rate", 4.009f },
            { "lfo5_step10", -0.382f }, { "lfo5_step11", -0.708f }, { "lfo5_step12", -0.924f }, { "lfo5_step14", -0.924f },
            { "lfo5_step15", -0.708f }, { "lfo5_step16", -0.382f }, { "lfo5_step2", 0.382f }, { "lfo5_step3", 0.708f },
            { "lfo5_step4", 0.924f }, { "lfo5_step6", 0.924f }, { "lfo5_step7", 0.708f }, { "lfo5_step8", 0.382f },
            { "lfo6_rate", 4.009f }, { "lfo6_step10", -0.382f }, { "lfo6_step11", -0.708f }, { "lfo6_step12", -0.924f },
            { "lfo6_step14", -0.924f }, { "lfo6_step15", -0.708f }, { "lfo6_step16", -0.382f }, { "lfo6_step2", 0.382f },
            { "lfo6_step3", 0.708f }, { "lfo6_step4", 0.924f }, { "lfo6_step6", 0.924f }, { "lfo6_step7", 0.708f },
            { "lfo6_step8", 0.382f }, { "lfo7_rate", 4.009f }, { "lfo7_step10", -0.382f }, { "lfo7_step11", -0.708f },
            { "lfo7_step12", -0.924f }, { "lfo7_step14", -0.924f }, { "lfo7_step15", -0.708f }, { "lfo7_step16", -0.382f },
            { "lfo7_step2", 0.382f }, { "lfo7_step3", 0.708f }, { "lfo7_step4", 0.924f }, { "lfo7_step6", 0.924f },
            { "lfo7_step7", 0.708f }, { "lfo7_step8", 0.382f }, { "lfo8_rate", 4.009f }, { "lfo8_step10", -0.382f },
            { "lfo8_step11", -0.708f }, { "lfo8_step12", -0.924f }, { "lfo8_step14", -0.924f }, { "lfo8_step15", -0.708f },
            { "lfo8_step16", -0.382f }, { "lfo8_step2", 0.382f }, { "lfo8_step3", 0.708f }, { "lfo8_step4", 0.924f },
            { "lfo8_step6", 0.924f }, { "lfo8_step7", 0.708f }, { "lfo8_step8", 0.382f }, { "lfo9_rate", 4.009f },
            { "lfo9_step10", -0.382f }, { "lfo9_step11", -0.708f }, { "lfo9_step12", -0.924f }, { "lfo9_step14", -0.924f },
            { "lfo9_step15", -0.708f }, { "lfo9_step16", -0.382f }, { "lfo9_step2", 0.382f }, { "lfo9_step3", 0.708f },
            { "lfo9_step4", 0.924f }, { "lfo9_step6", 0.924f }, { "lfo9_step7", 0.708f }, { "lfo9_step8", 0.382f },
            { "me_attack", 0.05099f }, { "me_decay", 0.399605f }, { "me_release", 0.30485f }, { "mod1_amt", 0.5f },
            { "mod1_dst", 9.0f }, { "mod1_src", 12.0f }, { "mod2_amt", 0.5f }, { "mod2_dst", 53.0f },
            { "mod2_src", 13.0f }, { "mod3_amt", 0.5f }, { "mod3_dst", 99.0f }, { "mod3_src", 14.0f },
            { "mod4_amt", 0.5f }, { "mod4_dst", 100.0f }, { "mod4_src", 15.0f }, { "mseg_rate", 0.4988f },
            { "mseg_time1", 0.25045f }, { "mseg_time2", 0.25045f }, { "mseg_time3", 0.25045f }, { "mseg_time4", 0.25045f },
            { "osc1_frame", 0.2f }, { "osc1_grain_size", 80.07f }, { "osc1_level", 0.6f }, { "osc1_table", 10.0f },
            { "osc2_fine", 0.0f }, { "osc2_grain_size", 80.07f }, { "osc2_level", 0.4f }, { "osc2_on", 1.0f },
            { "osc2_semi", 12.0f }, { "osc2_table", 9.0f }, { "osc4_grain_size", 80.07f }, { "osc5_grain_size", 80.07f },
            { "osc6_grain_size", 80.07f }, { "poly_voices", 1.0f }, { "sub_grain_size", 80.07f }, { "sub_shape", 2.0f },
            { "subosc_on", 1.0f }, { "voice_mode", 2.0f }, { "master", -7.46f },
        };
        p.macroNames = { "CUTOFF", "ENV", "CHORUS", "SPACE" };
        add (p);
    }
}
