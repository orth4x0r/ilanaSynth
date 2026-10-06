#pragma once

// DX7 voices as ilanaSynth patches: the six operators become oscillators 1-6
// (sine, ratio or fixed tuning, the algorithm as the FM matrix) on the
// Operator EG, whose parameters take the voice's envelopes, scaling,
// sensitivities, pitch envelope and LFO (dsp/OperatorEgParams.h). Nothing
// else rides along: the patch is ordinary parameters. Used for the preloaded
// banks (Dx7Banks.h), File > Import DX7 bank, and patches saved by the old
// DX7 mode (their "Dx7" child, converted on load).

#include <array>
#include <cstring>
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

// The four macros named for the kind of sound (UI review 7: every voice
// showed BRIGHT, TONE, DRIFT, SPACE). Only the names change: the first
// still deepens the modulators, the second closes the filter, the third
// detunes and the fourth opens the reverb.
inline std::array<const char*, 4> macroNamesFor (const juce::String& soundCategory)
{
    if (soundCategory == "Keys")  return { "BARK", "DARKEN", "WOBBLE", "ROOM" };
    if (soundCategory == "Bass")  return { "GROWL", "DARKEN", "DETUNE", "ROOM" };
    if (soundCategory == "Pluck") return { "TWANG", "DARKEN", "DETUNE", "ROOM" };
    if (soundCategory == "Pad")   return { "SHIMMER", "DARKEN", "DRIFT", "SPACE" };
    if (soundCategory == "Brass") return { "BLARE", "DARKEN", "DETUNE", "HALL" };
    if (soundCategory == "Wind")  return { "BREATHY", "DARKEN", "DETUNE", "HALL" };
    if (soundCategory == "Drums") return { "CLANG", "DARKEN", "DETUNE", "ROOM" };
    if (soundCategory == "FX")    return { "METAL", "DARKEN", "DRIFT", "SPACE" };
    if (soundCategory == "Lead")  return { "EDGE", "DARKEN", "DETUNE", "SPACE" };
    return { macroNames[0], macroNames[1], macroNames[2], macroNames[3] };
}

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
    // A reverb at mix 0 for SPACE, switched on: KEEP DRY makes it the dry
    // signal exactly until SPACE opens it (review 7, I7-4).
    set ("fx_slot1", 13);
    set ("fx_reverb_on", 1);
    set ("fx_reverb_keep_dry", 1);
    set ("fx_reverb_mix", 0);
    set ("master", 3.0f); // Dexed's own loudness (E.PIANO 1 measured against it)

    for (int k = 1; k <= 6; ++k)
    {
        const std::string p = OscillatorIds::prefixes[(size_t) (k - 1)];
        const auto t = Dx7::tuning (v, k, 60);
        set (p + "_on", 1);
        set (p + "_mode", OscMode::fmOperator); // a DX7 operator: FM / DX7
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
    routeFrom (Mod::Source::ModWheel, "OP LFO Pitch Depth", 1.0f);
    routeFrom (Mod::Source::Aftertouch, "OP LFO Pitch Depth", 1.0f);
    return out;
}

// The kind of sound a DX7 voice is, for the browser (UI review 6: DX7
// voices sit under Keys, Bass, Pad... with the bank as a filter). From the
// name first: the keyword that starts earliest wins (the longer one on a
// tie, so BASSOON is not a bass and HARPSICH is not a harp), so "STRG-CHIME"
// is strings and "CHIME-STRG" a chime. A name with no keyword falls back on
// the carriers' envelopes when the voice is known: no sustain is a pluck, a
// slow attack a pad, anything else a lead.
inline const char* soundCategory (const juce::String& voiceName, const Dx7::Voice* voice = nullptr)
{
    static const std::pair<const char*, const char*> keywords[] {
        { "BASSOON", "Wind" }, { "BASS", "Bass" }, { "FRETLESS", "Bass" },
        { "B.DRM", "Drums" }, { "DRUM", "Drums" }, { "SNAR", "Drums" }, { "TIMPANI", "Drums" }, { "BLOCK", "Drums" },
        { "COW BELL", "Drums" }, { "COWBELL", "Drums" }, { "STEEL DRUM", "Pluck" },
        { "PIANO", "Keys" }, { "PNO", "Keys" }, { "E.P", "Keys" }, { "GRAND", "Keys" }, { "HONKY", "Keys" },
        { "CLAV", "Keys" }, { "CLV", "Keys" }, { "HARPSI", "Keys" }, { "ORGAN", "Keys" }, { "ORG-", "Keys" },
        { "PIPES", "Keys" }, { "CALIOPE", "Keys" }, { "ACCORDION", "Keys" }, { "CELESTE", "Keys" }, { "VIBE", "Keys" },
        { "MARIM", "Keys" }, { "XYLOPHONE", "Keys" }, { "GLOKEN", "Keys" }, { "CHIME", "Keys" }, { "ORCH-CHIME", "Keys" },
        { "BELL", "Keys" }, { "T.BL", "Keys" },
        { "GUIT", "Pluck" }, { "GTR", "Pluck" }, { "KOTO", "Pluck" }, { "SITAR", "Pluck" }, { "LUTE", "Pluck" },
        { "BANJO", "Pluck" }, { "HARP", "Pluck" }, { "PIZZ", "Pluck" }, { "PLUCK", "Pluck" },
        { "STRING", "Pad" }, { "STRG", "Pad" }, { "STGS", "Pad" }, { "STG", "Pad" }, { "ORCH", "Pad" }, { "VOICE", "Pad" },
        { "VOX", "Pad" }, { "CHOIR", "Pad" }, { "SHIMMER", "Pad" }, { "EVOLUTION", "Pad" }, { "WATER", "Pad" }, { "PAD", "Pad" },
        { "VIOLA", "Pad" }, { "BOW", "Pad" },
        { "BRASS", "Brass" }, { "BRS", "Brass" }, { "HORN", "Brass" }, { "TRUMPET", "Brass" }, { "TBONE", "Brass" },
        { "SAX", "Wind" }, { "FLUTE", "Wind" }, { "PICCOLO", "Wind" }, { "OBOE", "Wind" }, { "CLARINET", "Wind" },
        { "RECORDER", "Wind" }, { "HARMONICA", "Wind" }, { "HRMNCA", "Wind" }, { "LEAD", "Lead" }, { "SAW", "Lead" },
        { "TRAIN", "FX" }, { "TAKE OFF", "FX" }, { "LASER", "FX" }, { "EXPLOSION", "FX" }, { "HELENS", "FX" },
        { "PRIX", "FX" }, { "GRAND PRIX", "FX" }, { "PLUCK BASS", "Bass" }, { "WASP", "FX" }, { "DESCENT", "FX" }, { "OCTAVE WAR", "FX" }, { "GOTCHA", "FX" }, { "BOAR", "FX" },
        { "ERUPT", "FX" }, { "THUNDER", "FX" }, { "ENCOUNTER", "FX" }, { "RUMBLE", "FX" }, { "SWP", "FX" },
        { "SWEEP", "FX" }, { "RISE", "FX" }, { "WHISL", "FX" }, { "WHISTLE", "FX" }, { "FLEXATONE", "FX" },
        { "GONG", "FX" }, { "ECHO", "FX" }, { "WOBBLE", "FX" }
    };

    const auto upper = voiceName.toUpperCase();
    const char* best = nullptr;
    int bestAt = 1 << 30, bestLength = 0;

    for (const auto& [word, category] : keywords)
    {
        const auto at = upper.indexOf (word);
        const auto length = (int) std::strlen (word);

        if (at >= 0 && (at < bestAt || (at == bestAt && length > bestLength)))
        {
            best = category;
            bestAt = at;
            bestLength = length;
        }
    }

    if (best != nullptr || voice == nullptr)
        return best != nullptr ? best : "Keys";

    // The heard operators' envelopes: R1 is the attack rate and L3 the
    // sustain level (0-99), output level at byte 16.
    const auto r = Dx7::routing ((*voice)[134]);
    auto sustain = 0, slowestAttack = 99;

    for (int k = 1; k <= 6; ++k)
    {
        const auto* o = Dx7::op (*voice, k);

        if (! r.carrier[(size_t) (k - 1)] || o[16] < 50)
            continue;

        sustain = juce::jmax (sustain, (int) o[6]);
        slowestAttack = juce::jmin (slowestAttack, (int) o[0]);
    }

    return sustain < 40 ? "Pluck" : (slowestAttack < 55 ? "Pad" : "Lead");
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
            preset.category = "DX7";
            preset.browseCategory = soundCategory (Dx7::name (voice), &voice);
            const auto named = macroNamesFor (preset.browseCategory);
            preset.macroNames = { named[0], named[1], named[2], named[3] };
            list.push_back (std::move (preset));
        }
    return list;
}
} // namespace Dx7Import
} // namespace Presets
