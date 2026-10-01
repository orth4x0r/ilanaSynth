#pragma once

// DX7 voices as ilanaSynth patches: the six operators become oscillators 1-6
// (sine, ratio or fixed tuning, the algorithm as the FM matrix) and the
// voice itself rides along, so DX7 mode (dsp/Dx7Engine.h) gives every
// operator its DX7 envelope, scaling, pitch envelope and LFO. Used for the
// preloaded banks (Dx7Banks.h) and for File > Import DX7 bank.

#include <deque>
#include <string>
#include <vector>

#include "Dx7Banks.h"
#include "dsp/Dx7Engine.h"
#include "dsp/Modulation.h"
#include "dsp/OscillatorIds.h"

namespace Presets
{
namespace Dx7Import
{
// Parameter ids and names live for the program's life (Value holds a
// const char*). Only the message thread builds patches.
inline const char* intern (const std::string& text)
{
    static std::deque<std::string> store;
    for (const auto& s : store)
        if (s == text)
            return s.c_str();
    store.push_back (text);
    return store.back().c_str();
}

inline std::string fmId (int source, int target) // 1-based
{
    static const char* legacy[3][3] { { "fm_feedback", "fm_1to2", "fm_1to3" },
                                      { "fm_amount", "fm_fb2", "fm_2to3" },
                                      { "fm_3to1", "fm_3to2", "fm_fb3" } };
    if (source <= 3 && target <= 3)
        return legacy[source - 1][target - 1];
    return source == target ? "fm_fb" + std::to_string (source)
                            : "fm_" + std::to_string (source) + "to" + std::to_string (target);
}

// Each operator's level knob scales its DX7 gain by twice its value
// (Voice::dx7LevelScale), so 0.5 is the DX7's own level and BRIGHT can push
// the modulators past it.
constexpr float opLevel = 0.5f;
inline const char* const macroNames[4] { "BRIGHT", "TONE", "DRIFT", "SPACE" };

inline std::vector<Value> values (const Dx7::Voice& v)
{
    std::vector<Value> out;
    const auto set = [&out] (const std::string& id, float value) { out.push_back ({ intern (id), value }); };
    const auto r = Dx7::routing (v[134]);
    const auto transpose = (int) v[144] - 24;

    // A clean slate for what the defaults leave on: voice and filter.
    set ("voice_mode", 0);
    set ("glide", 0);
    set ("amp_velocity", 0);
    set ("f1_cutoff", 20000);
    set ("f1_reso", 0);
    set ("f1_drive", 1);
    set ("f1_env", 0);
    set ("f2_cutoff", 20000);
    set ("fx_slot1", 13); // a reverb at mix 0, for SPACE
    set ("fx_reverb_mix", 0);
    set ("master", 3.0f); // Dexed's own loudness (E.PIANO 1 measured against it)

    for (int k = 1; k <= 6; ++k)
    {
        const std::string p = OscillatorIds::prefixes[(size_t) (k - 1)];
        const auto t = Dx7::tuning (v, k, 60);
        set (p + "_on", 1);
        set (p + "_mode", 0);
        set (p + "_table", 8); // Sine
        set (p + "_frame", 0);
        set (p + "_level", opLevel);
        set (p + "_unison", 1);
        set (p + "_out", r.carrier[(size_t) (k - 1)] ? 1.0f : 0.0f);
        set (p + "_amp_env", 0);
        set (p + "_fb_type", 1); // Filtered: the DX7's two-sample average
        set (p + "_ratio_snap", 0);
        set (p + "_tune", t.fixed ? 2.0f : 1.0f);
        if (t.fixed)
            set (p + "_fixed_hz", (float) t.fixedHz);
        else
        {
            set (p + "_ratio", (float) t.ratio);
            if (transpose != 0)
                set (p + "_semi", (float) juce::jlimit (-24, 24, transpose));
        }
        set (p + "_fine", (float) juce::jlimit (-100.0, 100.0, t.detuneCents));
    }

    for (int s = 0; s < 6; ++s)
        for (int t = 0; t < 6; ++t)
            if (r.modulates[(size_t) s][(size_t) t])
                set (fmId (s + 1, t + 1), 1.0f);
    if (r.feedbackOp >= 0 && v[135] > 0)
        set (fmId (r.feedbackOp + 1, r.feedbackOp + 1), std::exp2 (-(8.0f - (float) v[135])));

    // Macros as mod slots: BRIGHT deepens the modulators, TONE closes the
    // filter, DRIFT detunes, SPACE opens the reverb.
    static const auto destinations = Mod::getDestinationNames();
    auto slot = 1;
    const auto route = [&] (int macro, const juce::String& destination, float amount)
    {
        const auto d = destinations.indexOf (destination);
        if (d <= 0)
            return;
        const auto prefix = "mod" + std::to_string (slot++);
        set (prefix + "_src", (float) ((int) Mod::Source::Macro1 + macro));
        set (prefix + "_dst", (float) d);
        set (prefix + "_amt", amount);
    };
    for (int k = 1; k <= 6; ++k)
        if (! r.carrier[(size_t) (k - 1)])
            route (0, "Osc" + juce::String (k) + " Level", 0.3f);
    route (1, "Filter1 Cutoff", -0.55f);
    route (2, "Drift", 0.6f);
    route (3, "Reverb Mix", 0.35f);
    return out;
}

// The preloaded banks as factory presets, "NAME (BANK)", category DX7.
inline std::vector<FactoryPreset> bankPresets()
{
    static std::deque<Dx7::Voice> voices;
    std::vector<FactoryPreset> list;
    for (const auto& bank : Dx7Banks::banks)
        for (int i = 0; i < 32; ++i)
        {
            voices.push_back (Dx7::unpack (bank.data + i * 128));
            const auto& voice = voices.back();
            FactoryPreset preset { intern (Dx7::name (voice) + " (" + bank.label + ")"), values (voice) };
            preset.macroNames = { macroNames[0], macroNames[1], macroNames[2], macroNames[3] };
            preset.category = "DX7";
            preset.dx7 = &voice;
            list.push_back (std::move (preset));
        }
    return list;
}
} // namespace Dx7Import
} // namespace Presets
