#pragma once

// DX7 voices as ilanaSynth patches: the six operators become oscillators 1-6
// (sine, ratio or fixed tuning, the algorithm as the FM matrix) on the
// Operator EG, whose parameters take the voice's envelopes, scaling,
// sensitivities, pitch envelope and LFO (dsp/OperatorEgParams.h). Nothing
// else rides along: the patch is ordinary parameters. Used for the preloaded
// banks (Dx7Banks.h), File > Import DX7 bank, and patches saved by the old
// DX7 mode (their "Dx7" child, converted on load).

#include <deque>
#include <string>
#include <vector>

#include "Dx7Banks.h"
#include "dsp/Dx7Engine.h"
#include "dsp/Modulation.h"
#include "dsp/OperatorEgParams.h"
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

// The voice's envelope side as Operator EG parameters (each one byte of the
// voice, see OperatorEgParams.h).
inline std::vector<Value> egValues (const Dx7::Voice& v)
{
    std::vector<Value> out;
    for (int k = 1; k <= 6; ++k)
    {
        const std::string p = OscillatorIds::prefixes[(size_t) (k - 1)];
        for (const auto& field : OperatorEg::operatorFields())
            out.push_back ({ intern (p + field.suffix), (float) Dx7::op (v, k)[field.offset] });
    }
    for (const auto& field : OperatorEg::voiceFields())
        out.push_back ({ field.suffix, (float) v[(size_t) field.offset] });
    out.push_back ({ OperatorEg::keyOffsetId, (float) ((int) v[(size_t) OperatorEg::keyOffsetByte] - 24) });
    return out;
}

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
        set (p + "_amp_env", (float) OperatorEg::envelopeChoice);
        set (p + "_fb_type", 3); // FmFeedback::Dx7: the DX7's two-sample average
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

    for (const auto& value : egValues (v))
        out.push_back (value);

    for (int s = 0; s < 6; ++s)
        for (int t = 0; t < 6; ++t)
            if (r.modulates[(size_t) s][(size_t) t])
                set (fmId (s + 1, t + 1), 1.0f);
    if (r.feedbackOp >= 0 && v[135] > 0)
        set (fmId (r.feedbackOp + 1, r.feedbackOp + 1), std::exp2 (-(8.0f - (float) v[135])));
    set (OperatorEg::dx7AlgorithmId, (float) (v[134] + 1));

    // Macros as mod slots: BRIGHT deepens the modulators, TONE closes the
    // filter, DRIFT detunes, SPACE opens the reverb.
    static const auto destinations = Mod::getDestinationNames();
    auto slot = 1;
    const auto routeFrom = [&] (Mod::Source source, const juce::String& destination, float amount)
    {
        const auto d = destinations.indexOf (destination);
        if (d <= 0)
            return;
        const auto prefix = "mod" + std::to_string (slot++);
        set (prefix + "_src", (float) (int) source);
        set (prefix + "_dst", (float) d);
        set (prefix + "_amt", amount);
    };
    const auto route = [&] (int macro, const juce::String& destination, float amount)
    {
        routeFrom ((Mod::Source) ((int) Mod::Source::Macro1 + macro), destination, amount);
    };
    // With no modulators (organs, alg 32) BRIGHT lifts the upper carriers.
    auto anyModulator = false;
    for (int k = 1; k <= 6; ++k)
        anyModulator = anyModulator || ! r.carrier[(size_t) (k - 1)];
    for (int k = 1; k <= 6; ++k)
        if (! r.carrier[(size_t) (k - 1)] || (! anyModulator && k > 1))
            route (0, "Osc" + juce::String (k) + " Level", 0.3f);
    route (1, "Filter1 Cutoff", -0.55f);
    route (2, "Drift", 0.6f);
    route (3, "Reverb Mix", 0.35f);
    // The wheel and pressure add vibrato, as a DX7's do with their range at
    // 99 on PITCH (the LFO's PITCH DEPTH, scaled by PITCH SENS). They move
    // nothing until touched.
    routeFrom (Mod::Source::ModWheel, "Op LFO Pitch Depth", 1.0f);
    routeFrom (Mod::Source::Aftertouch, "Op LFO Pitch Depth", 1.0f);
    return out;
}

// The preloaded banks as factory presets, "NAME (BANK)", category DX7.
inline std::vector<FactoryPreset> bankPresets()
{
    std::vector<FactoryPreset> list;
    for (const auto& bank : Dx7Banks::banks)
        for (int i = 0; i < 32; ++i)
        {
            const auto voice = Dx7::unpack (bank.data + i * 128);
            FactoryPreset preset { intern (Dx7::name (voice) + " (" + bank.label + ")"), values (voice) };
            preset.macroNames = { macroNames[0], macroNames[1], macroNames[2], macroNames[3] };
            preset.category = "DX7";
            list.push_back (std::move (preset));
        }
    return list;
}
} // namespace Dx7Import
} // namespace Presets
