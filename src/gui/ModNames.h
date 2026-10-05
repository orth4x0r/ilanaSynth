#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../dsp/airwindows/Categories.h"
#include "../dsp/OperatorEgParams.h"

// One name per modulation source and destination for everything the editor
// shows (chips, knob cards, the matrix, menus, pool tags), so a source reads
// the same wherever it appears (UI review 6, V6-10 / S5-13). The saved and
// host-facing names (Mod::getSourceNames / getDestinationNames, the choice
// parameters' strings, PresetVoicing's lookups) are left exactly as they
// were: these are display names only.
namespace ModNames
{
// A routing's row in the matrix: set by the editor (it shows the MATRIX page
// and flashes the row); -1 just opens the page. LFO cards' DRIVES list calls
// it (review 9, V9-31).
inline std::function<void (int slot)>& openMatrixRow()
{
    static std::function<void (int)> hook;
    return hook;
}

// A source's one name, written as its chip is (UI-CONVENTIONS: module and
// source names are upper case in labels, chips and combos; review 8,
// I8-12): "FILT 2 ENV", "MOD WHEEL", "LFO 3 B". Tooltips and menus use it
// too, so a source never reads two ways. Nothing shortens it to a code. A
// macro with a patch name leads with that name, as its knob in the strip
// does, and only that name ("TONE", as the strip, the chips and the remap
// header write it: review 9, I9-8); the number follows only when two
// macros share a name, so the two can be told apart.
inline juce::String macroName (int macro, const IlanaSynthAudioProcessor* processor)
{
    const auto base = "Macro " + juce::String (macro + 1);
    const auto name = processor != nullptr ? processor->getMacroName (macro) : base;

    if (name.equalsIgnoreCase (base) || name.isEmpty())
        return base.toUpperCase();

    if (processor != nullptr)
        for (int other = 0; other < Mod::numMacros; ++other)
            if (other != macro && processor->getMacroName (other).equalsIgnoreCase (name))
                return name.toUpperCase() + " (M" + juce::String (macro + 1) + ")";

    return name.toUpperCase();
}

inline juce::String source (int sourceIndex, const IlanaSynthAudioProcessor* processor = nullptr)
{
    using S = Mod::Source;
    const auto s = (S) sourceIndex;

    if (const auto macro = Mod::macroIndexFor (s); macro >= 0)
        return macroName (macro, processor);

    if (const auto lfo = Mod::lfoIndexFor (s); lfo >= 0)
        return "LFO " + juce::String (lfo + 1);

    if (const auto lfo = Mod::lfoBIndexFor (s); lfo >= 0)
        return "LFO " + juce::String (lfo + 1) + " B";

    if (s >= S::Env6 && s <= S::Env16)
        return "ENV " + juce::String (6 + sourceIndex - (int) S::Env6);

    switch (s)
    {
        case S::None:       return "None";
        case S::AmpEnv:     return "AMP ENV";
        case S::FilterEnv:  return "FILT ENV";
        case S::FilterEnv2: return "FILT 2 ENV";
        case S::ModEnv:     return "MOD ENV";
        case S::Env4:       return "ENV 5";
        case S::Velocity:   return "VELOCITY";
        case S::KeyTrack:   return "KEY TRACK";
        case S::Random:     return "RANDOM";
        case S::ModWheel:   return "MOD WHEEL";
        case S::Aftertouch: return "PRESSURE";
        case S::Expression: return "EXPRESSION";
        case S::ClockSh:    return "CLOCKED S&H";
        case S::Mseg:       return "MSEG";
        case S::InputEnv:   return "INPUT ENV";
        case S::VectorX:    return "VECTOR X";
        case S::VectorY:    return "VECTOR Y";
        case S::OpLfo:      return "OP LFO";
        case S::OpPitchEnv: return "OP PITCH";
        default:            break;
    }

    return Mod::getSourceNames()[sourceIndex].toUpperCase();
}

// The name a chip or card title shows. A macro shows its own name alone
// there ("TONE"), as the macro strip does.
inline juce::String sourceUpper (int sourceIndex, const IlanaSynthAudioProcessor* processor = nullptr)
{
    if (const auto macro = Mod::macroIndexFor ((Mod::Source) sourceIndex); macro >= 0 && processor != nullptr)
        return processor->getMacroName (macro).toUpperCase();

    return source (sourceIndex);
}

// Every source in the order the pickers list them, grouped: LFOs (A then
// B, the MSEG and OP LFO with them), envelopes (with OP PITCH), macros, then
// performance and the rest.
enum class SourceGroup { lfo, lfoB, envelope, macro, performance };

inline SourceGroup groupOf (int sourceIndex)
{
    using S = Mod::Source;
    const auto s = (S) sourceIndex;

    if (Mod::lfoIndexFor (s) >= 0)
        return SourceGroup::lfo;
    if (Mod::lfoBIndexFor (s) >= 0)
        return SourceGroup::lfoB;
    if (Mod::macroIndexFor (s) >= 0)
        return SourceGroup::macro;
    // The MSEG and the Operator Env's LFO sit in the LFO row on MOD, its
    // pitch envelope in the envelope row (review 7, I7-16).
    if (s == S::Mseg || s == S::OpLfo)
        return SourceGroup::lfo;
    if (s == S::OpPitchEnv)
        return SourceGroup::envelope;
    if (s == S::AmpEnv || s == S::FilterEnv || s == S::FilterEnv2 || s == S::ModEnv || s == S::Env4
        || (s >= S::Env6 && s <= S::Env16))
        return SourceGroup::envelope;
    return SourceGroup::performance;
}

// The envelope pool's order: AMP, FILT, FILT 2, MOD, ENV 5, then ENV 6-16.
inline Mod::Source envelopeSourceFor (int poolIndex)
{
    using S = Mod::Source;
    constexpr S first[] { S::AmpEnv, S::FilterEnv, S::FilterEnv2, S::ModEnv, S::Env4 };
    return poolIndex < 5 ? first[juce::jlimit (0, 4, poolIndex)] : (S) ((int) S::Env6 + juce::jlimit (0, 10, poolIndex - 5));
}

inline int envelopePoolIndexFor (Mod::Source source)
{
    for (int i = 0; i < 16; ++i)
        if (envelopeSourceFor (i) == source)
            return i;
    return -1;
}

// Every source once, in the pickers' order: LFOs, their B outputs, the
// MSEG and OP LFO; the envelopes in pool order and OP PITCH; the macros;
// then performance and the rest.
inline const std::vector<int>& sourcesInMenuOrder()
{
    static const std::vector<int> order = []
    {
        using S = Mod::Source;
        std::vector<int> list;
        for (int lfo = 0; lfo < Mod::numLfoSources; ++lfo)
            list.push_back ((int) Mod::lfoSourceFor (lfo));
        for (int lfo = 0; lfo < Mod::numLfoSources; ++lfo)
            list.push_back ((int) Mod::lfoBSourceFor (lfo));
        list.push_back ((int) S::Mseg);
        list.push_back ((int) S::OpLfo);
        for (int env = 0; env < 16; ++env)
            list.push_back ((int) envelopeSourceFor (env));
        list.push_back ((int) S::OpPitchEnv);
        for (int macro = 0; macro < Mod::numMacros; ++macro)
            list.push_back ((int) Mod::macroSourceFor (macro));
        for (int i = 1; i < (int) S::Count; ++i)
            if (std::find (list.begin(), list.end(), i) == list.end())
                list.push_back (i);
        return list;
    }();
    return order;
}

// Fills a menu with every source, grouped into sub-menus; item IDs are
// source index + 1 (none: 1). `isTicked` marks sources (e.g. already routed).
inline void fillSourceMenu (juce::PopupMenu& menu, const IlanaSynthAudioProcessor* processor,
                            const std::function<bool (int)>& isTicked = nullptr, bool withNone = false)
{
    using S = Mod::Source;
    juce::PopupMenu lfos, lfoBs, envelopes, macros, performance;
    const auto add = [&] (juce::PopupMenu& target, int index)
    {
        target.addItem (index + 1, source (index, processor), true, isTicked != nullptr && isTicked (index));
    };

    // Filed where the MOD page's pools show them (I7-16): the MSEG and the
    // OP LFO after the LFOs, OP PITCH after the envelopes.
    for (int lfo = 0; lfo < Mod::numLfoSources; ++lfo)
    {
        add (lfos, (int) Mod::lfoSourceFor (lfo));
        add (lfoBs, (int) Mod::lfoBSourceFor (lfo));
    }
    lfos.addSeparator();
    add (lfos, (int) S::Mseg);
    add (lfos, (int) S::OpLfo);

    for (int env = 0; env < 16; ++env)
        add (envelopes, (int) envelopeSourceFor (env));
    envelopes.addSeparator();
    add (envelopes, (int) S::OpPitchEnv);

    for (int macro = 0; macro < Mod::numMacros; ++macro)
        add (macros, (int) Mod::macroSourceFor (macro));

    for (const auto s : { S::Velocity, S::KeyTrack, S::Random, S::ModWheel, S::Aftertouch, S::Expression,
                          S::ClockSh, S::VectorX, S::VectorY, S::InputEnv })
        if (s != S::InputEnv || IlanaSynthAudioProcessor::isEffectBuild)
            add (performance, (int) s);

    if (withNone)
    {
        menu.addItem (1, "None", true, isTicked != nullptr && isTicked (0));
        menu.addSeparator();
    }

    menu.addSubMenu ("LFOs", lfos);
    menu.addSubMenu ("LFO outputs B (chaos, physics)", lfoBs);
    menu.addSubMenu ("Envelopes", envelopes);
    menu.addSubMenu ("Macros", macros);
    menu.addSubMenu ("Performance and more", performance);
}

//==============================================================================
// Destinations, written by one formatter everywhere (the matrix, the knob
// cards, the macro card, the remap header, the hover line, tooltips): the
// module in capitals as its card names it, then the control as its knob is
// labelled, "MODULE › Control" (review 8: V8-13, I8-12, S8-11):
// "FILTER 1 › Cutoff", "OSC 2 › Semi", "FM › OSC 2 → OSC 1", "FX REVERB › Mix".
// The parts below keep the module's plain spelling ("Filter 1"), which the
// menus and the idle checks match against; full() writes it.

inline const juce::String& separator()
{
    static const juce::String text = juce::String::fromUTF8 (" \xe2\x80\xba ");
    return text;
}

inline const juce::String& arrow()
{
    static const juce::String text = juce::String::fromUTF8 (" \xe2\x86\x92 ");
    return text;
}

// "Osc3 Pitch" -> "OSC 3", "Pitch"; the module and the control.
struct DestinationName
{
    juce::String module, control;
    juce::String full() const { return control.isEmpty() ? module : module.toUpperCase() + separator() + control; }
};

namespace detail
{
// "Osc4" -> "OSC 4" inside a name.
inline juce::String oscNumbers (juce::String text)
{
    for (int osc = 1; osc <= 6; ++osc)
        text = text.replace ("Osc" + juce::String (osc), "OSC " + juce::String (osc));
    return text;
}

// The FX module a parameter id belongs to (the FX type index), its display
// name and the word its parameter names start with.
struct FxModule
{
    const char* idPrefix;
    int type;
    const char* module;
    const char* namePrefix;
};

inline const std::vector<FxModule>& fxModules()
{
    static const std::vector<FxModule> list {
        { "fx_amp_", 1, "Amp", "Amp " },         { "fx_drive_", 2, "Drive", "Drive " },
        { "fx_fold", 2, "Drive", "Drive " },      { "fx_crush_", 3, "Crush", "Crush " },
        { "fx_comp_", 4, "Compressor", "Comp " }, { "fx_comb_", 5, "Comb", "Comb " },
        { "fx_phaser_", 6, "Phaser", "Phaser " }, { "fx_chorus_", 7, "Chorus", "Chorus " },
        { "fx_haas_", 8, "Haas", "Haas " },       { "fx_delay_", 9, "Delay", "Delay " },
        { "fx_taps_", 9, "Delay", "" },           { "fx_stutter_", 10, "Stutter", "Stutter " },
        { "fx_smear_", 11, "Smear", "Smear " },   { "fx_freeze_", 12, "Freeze", "Freeze " },
        { "fx_reverb_", 13, "Reverb", "Reverb " }, { "fx_flanger_", 14, "Flanger", "Flanger " },
        { "fx_dim_", 15, "Dimension", "Dimension " }, { "fx_gate_", 16, "Trance Gate", "Gate " },
        { "fx_tape_stop_", 17, "Tape Stop", "Tape Stop " }, { "fx_tilt", 18, "Tilt", "Tilt " },
        { "fx_util_", 19, "Utility", "Utility " }, { "fx_ott_", 20, "OTT", "OTT " },
        { "fx_limit_", 21, "Limiter", "Limiter " }, { "fx_width", 22, "Widener", "Width " },
        { "fx_trem_", 23, "Tremolo", "Tremolo " }, { "fx_shifter_", 24, "Freq Shift", "Freq Shift " },
        { "fx_ring_", 25, "Ring Mod", "Ring " },  { "fx_octaver_", 26, "Octaver", "Octaver " },
        { "fx_vowel_", 27, "Vowel", "Vowel " },   { "fx_feedback_", 28, "Feedback", "Feedback " },
        { "fx_eq_", 29, "EQ", "EQ " },
    };
    return list;
}

inline const FxModule* fxModuleFor (const juce::String& id)
{
    for (const auto& module : fxModules())
        if (id.startsWith (module.idPrefix))
            return &module;
    return nullptr;
}

// The explicit destinations (Mod::Destination), module and control.
inline DestinationName explicitName (int destination)
{
    using D = Mod::Destination;
    const auto d = (D) destination;

    // The six oscillators' explicit targets, OSC 3 under its legacy "Sub".
    struct OscTargets { D pitch, frame, level, start, end, detune, pan, warp, blend, spread; };
    static const OscTargets oscs[] {
        { D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1SampleStart, D::Osc1SampleEnd, D::Osc1Detune, D::Osc1Pan, D::Osc1Warp, D::Osc1Blend, D::Osc1Spread },
        { D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2SampleStart, D::Osc2SampleEnd, D::Osc2Detune, D::Osc2Pan, D::Osc2Warp, D::Osc2Blend, D::Osc2Spread },
        { D::SubPitch, D::SubFrame, D::SubLevel, D::SubSampleStart, D::SubSampleEnd, D::SubDetune, D::SubPan, D::SubWarp, D::SubBlend, D::SubSpread },
        { D::Osc4Pitch, D::Osc4Frame, D::Osc4Level, D::Osc4SampleStart, D::Osc4SampleEnd, D::Osc4Detune, D::Osc4Pan, D::Osc4Warp, D::Osc4Blend, D::Osc4Spread },
        { D::Osc5Pitch, D::Osc5Frame, D::Osc5Level, D::Osc5SampleStart, D::Osc5SampleEnd, D::Osc5Detune, D::Osc5Pan, D::Osc5Warp, D::Osc5Blend, D::Osc5Spread },
        { D::Osc6Pitch, D::Osc6Frame, D::Osc6Level, D::Osc6SampleStart, D::Osc6SampleEnd, D::Osc6Detune, D::Osc6Pan, D::Osc6Warp, D::Osc6Blend, D::Osc6Spread },
    };

    for (int osc = 0; osc < 6; ++osc)
    {
        const auto& t = oscs[osc];
        const auto module = "OSC " + juce::String (osc + 1);
        const std::pair<D, const char*> fields[] { { t.pitch, "Pitch" }, { t.frame, "Frame" }, { t.level, "Level" },
                                                   { t.start, "Sample Start" }, { t.end, "Sample End" },
                                                   { t.detune, "Detune" }, { t.pan, "Pan" }, { t.warp, "Warp" },
                                                   { t.blend, "Unison Blend" }, { t.spread, "Spread" } };
        for (const auto& [target, control] : fields)
            if (d == target)
                return { module, control };
    }

    for (int lfo = 0; lfo < Mod::numLfoSources; ++lfo)
        if (d == Mod::lfoRateDestinationFor (lfo))
            return { "LFO " + juce::String (lfo + 1), "Rate" };

    switch (d)
    {
        case D::None:            return { "None", {} };
        case D::NoiseLevel:      return { "Noise", "Level" };
        case D::Filter1Cutoff:   return { "Filter 1", "Cutoff" };
        case D::Filter1Reso:     return { "Filter 1", "Reso" };
        case D::Filter1Drive:    return { "Filter 1", "Drive" };
        case D::Filter1Env:      return { "Filter 1", "Env Amt" };
        case D::Filter1Fm:       return { "Filter 1", "FM" };
        case D::Filter1Morph:    return { "Filter 1", "Morph" };
        case D::Filter2Cutoff:   return { "Filter 2", "Cutoff" };
        case D::Filter2Reso:     return { "Filter 2", "Reso" };
        case D::Filter2Drive:    return { "Filter 2", "Drive" };
        case D::Filter2Env:      return { "Filter 2", "Env Amt" };
        case D::Filter2Fm:       return { "Filter 2", "FM" };
        case D::Filter2Morph:    return { "Filter 2", "Morph" };
        case D::AmpLevel:        return { "Amp", "Level" };
        case D::Pan:             return { "Voice", "Pan" };
        case D::FxDriveAmount:   return { "FX Drive", "Amount" };
        case D::FxCrushMix:      return { "FX Crush", "Mix" };
        case D::FxCombFreq:      return { "FX Comb", "Freq" };
        case D::FxPhaserRate:    return { "FX Phaser", "Rate" };
        case D::FxChorusDepth:   return { "FX Chorus", "Depth" };
        case D::FxDelayMix:      return { "FX Delay", "Mix" };
        case D::FxDelayFeedback: return { "FX Delay", "Feedback" };
        case D::FxSmearMix:      return { "FX Smear", "Mix" };
        case D::FxFreezeMix:     return { "FX Freeze", "Mix" };
        case D::FxReverbMix:     return { "FX Reverb", "Mix" };
        case D::FxReverbSize:    return { "FX Reverb", "Size" };
        case D::FmAmount:        return { "FM", "OSC 2" + arrow() + "OSC 1" };
        case D::FmFeedback:      return { "FM", "OSC 1 Feedback" };
        case D::Fm1to2:          return { "FM", "OSC 1" + arrow() + "OSC 2" };
        case D::Fm1to3:          return { "FM", "OSC 1" + arrow() + "OSC 3" };
        case D::Fm2to3:          return { "FM", "OSC 2" + arrow() + "OSC 3" };
        case D::Fm3to1:          return { "FM", "OSC 3" + arrow() + "OSC 1" };
        case D::Fm3to2:          return { "FM", "OSC 3" + arrow() + "OSC 2" };
        case D::Fm2Feedback:     return { "FM", "OSC 2 Feedback" };
        case D::Fm3Feedback:     return { "FM", "OSC 3 Feedback" };
        case D::RingMod:         return { "Ring Mod", "Amount" };
        case D::Drift:           return { "Voice", "Analog Drift" };
        case D::MsegRate:        return { "MSEG", "Rate" };
        case D::ResAmount:       return { "Resonator", "Amount" };
        case D::ResDecay:        return { "Resonator", "Decay" };
        case D::ResOffset:       return { "Resonator", "Offset" };
        case D::AmpAttack:       return { "Amp Env", "Attack" };
        case D::AmpDecay:        return { "Amp Env", "Decay" };
        case D::AmpSustain:      return { "Amp Env", "Sustain" };
        case D::AmpRelease:      return { "Amp Env", "Release" };
        case D::FeAttack:        return { "Filt Env", "Attack" };
        case D::FeDecay:         return { "Filt Env", "Decay" };
        case D::FeSustain:       return { "Filt Env", "Sustain" };
        case D::FeRelease:       return { "Filt Env", "Release" };
        case D::MeAttack:        return { "Mod Env", "Attack" };
        case D::MeDecay:         return { "Mod Env", "Decay" };
        case D::MeSustain:       return { "Mod Env", "Sustain" };
        case D::MeRelease:       return { "Mod Env", "Release" };
        case D::F2eAttack:       return { "Filt 2 Env", "Attack" };
        case D::F2eDecay:        return { "Filt 2 Env", "Decay" };
        case D::F2eSustain:      return { "Filt 2 Env", "Sustain" };
        case D::F2eRelease:      return { "Filt 2 Env", "Release" };
        case D::E4Attack:        return { "Env 5", "Attack" };
        case D::E4Decay:         return { "Env 5", "Decay" };
        case D::E4Sustain:       return { "Env 5", "Sustain" };
        case D::E4Release:       return { "Env 5", "Release" };
        default:                 break;
    }

    return { Mod::getDestinationNames()[destination], {} };
}

// A parameter destination, from its id and saved name.
inline DestinationName paramName (const juce::String& id, juce::String name)
{
    // A host name ("OSC 1 Level", I8-40) read as the saved one ("Osc1 Level").
    for (int osc = 1; osc <= 6; ++osc)
        name = name.replace ("OSC " + juce::String (osc), "Osc" + juce::String (osc));

    const auto strip = [] (const juce::String& text, const juce::String& prefix)
    {
        return prefix.isNotEmpty() && text.startsWith (prefix) ? text.substring (prefix.length()) : text;
    };

    if (id.startsWith ("fx_slot"))
        return { "FX Slot " + juce::String (id.substring (7).getIntValue()), "Blend" };

    if (const auto* fx = fxModuleFor (id))
    {
        auto control = strip (name, fx->namePrefix);
        // A module with a single knob of its own name ("Tilt", "Width").
        if (control == name && name == juce::String (fx->namePrefix).trim())
            control = "Amount";
        if (id == "fx_shifter_shift")
            control = "Shift";
        return { "FX " + juce::String (fx->module), control };
    }

    // The Operator Env (review 7, I7-17): its stages as the operator card
    // labels them, under the oscillator; the pitch envelope and LFO as
    // their own modules, named as their pool cards are.
    if (id.contains ("_eg_"))
    {
        static const std::pair<const char*, const char*> fields[] {
            { "_eg_out", "OP ENV Level" },        { "_eg_r1", "OP ENV Attack" },
            { "_eg_r2", "OP ENV Decay 1" },       { "_eg_r3", "OP ENV Decay 2" },
            { "_eg_r4", "OP ENV Release" },       { "_eg_l1", "OP ENV Peak" },
            { "_eg_l2", "OP ENV Mid" },           { "_eg_l3", "OP ENV Sustain" },
            { "_eg_l4", "OP ENV End" },           { "_eg_rate_key", "OP ENV Key Rate" },
            { "_eg_break", "OP ENV Scale Key" },  { "_eg_ldepth", "OP ENV Low Depth" },
            { "_eg_rdepth", "OP ENV High Depth" }, { "_eg_ams", "OP ENV Amp Mod" },
            { "_eg_vel", "OP ENV Velocity" },
        };
        for (const auto& [suffix, control] : fields)
            if (id.endsWith (suffix))
                return { "OSC " + juce::String::charToString (name[3]), control };
    }

    if (id.startsWith ("opeg_"))
    {
        static const std::pair<const char*, DestinationName> fields[] {
            { "opeg_lfo_speed", { "OP LFO", "Rate" } },        { "opeg_lfo_delay", { "OP LFO", "Delay" } },
            { "opeg_lfo_pmd", { "OP LFO", "Pitch Depth" } },   { "opeg_lfo_amd", { "OP LFO", "Amp Depth" } },
            { "opeg_lfo_pms", { "OP LFO", "Pitch Sens" } },    { "opeg_pitch_r1", { "OP PITCH", "Attack" } },
            { "opeg_pitch_r2", { "OP PITCH", "Decay 1" } },    { "opeg_pitch_r3", { "OP PITCH", "Decay 2" } },
            { "opeg_pitch_r4", { "OP PITCH", "Release" } },    { "opeg_pitch_l1", { "OP PITCH", "Peak" } },
            { "opeg_pitch_l2", { "OP PITCH", "Mid" } },    { "opeg_pitch_l3", { "OP PITCH", "Sustain" } },
            { "opeg_pitch_l4", { "OP PITCH", "End" } },        { "opeg_key_offset", { "OP ENV", "Scale Shift" } },
        };
        for (const auto& [field, parts] : fields)
            if (id == field)
                return parts;
    }

    // The LFOs' own knobs: "LFO 3 › Shape 2", "LFO 3 › Smooth".
    if (id.startsWith ("lfo") && name.startsWith ("LFO"))
        return { "LFO " + juce::String (id.substring (3).getIntValue()), name.fromFirstOccurrenceOf (" ", false, false) };

    if (id.startsWith ("macro") && id.contains ("_evolve"))
        return { "Macro " + juce::String (id.substring (5).getIntValue()), id.endsWith ("_rate") ? "Evolve Rate" : "Evolve" };

    // Airwindows: "FX Airwindows › Knob 2", "FX AW Tape › Mix".
    if (id.startsWith ("fx_aw"))
    {
        const auto module = id.substring (3).upToFirstOccurrenceOf ("_", false, false);
        juce::String moduleName = "Airwindows";
        for (const auto& category : airwindows::categoryModules())
            if (module == category.id)
                moduleName = category.label;
        const auto field = id.fromLastOccurrenceOf ("_", false, false);
        return { "FX " + moduleName, field == "mix" ? juce::String ("Mix") : "Knob " + field.substring (1) };
    }

    // Envelope settings: "Amp Env › Delay", "Env 7 › Attack".
    {
        static const std::pair<const char*, const char*> envelopes[] {
            { "amp_", "Amp Env" }, { "fe_", "Filt Env" }, { "f2e_", "Filt 2 Env" }, { "me_", "Mod Env" }, { "e4_", "Env 5" }
        };
        static const std::pair<const char*, const char*> fields[] {
            { "attack", "Attack" }, { "decay", "Decay" }, { "sustain", "Sustain" }, { "release", "Release" },
            { "delay", "Delay" }, { "hold", "Hold" }, { "curve", "Curve" }, { "keyrate", "Key Rate" }, { "velocity", "Velocity" }
        };
        juce::String module;
        for (const auto& [prefix, envName] : envelopes)
            if (id.startsWith (prefix))
                module = envName;
        if (id.startsWith ("env") && juce::CharacterFunctions::isDigit (id[3]))
            module = "Env " + juce::String (id.substring (3).getIntValue());
        if (id == "filter_velocity")
            return { "Filt Env", "Velocity" };
        if (module.isNotEmpty())
            for (const auto& [field, control] : fields)
                if (id.endsWith (juce::String ("_") + field))
                    return { module, control };
    }

    if (id == "f1_keytrack")   return { "Filter 1", "Key Track" };
    if (id == "f2_keytrack")   return { "Filter 2", "Key Track" };
    if (id == "res_keytrack")  return { "Resonator", "Key Track" };
    if (id == "voice_spread")  return { "Voice", "Spread" };
    if (id == "unison_random") return { "Voice", "Unison Random" };
    if (id.startsWith ("sym_")) return { "Sympathetic", name.fromFirstOccurrenceOf (" ", false, false) };
    if (id == "vec_rate")       return { "Vector", "Path Rate" };
    if (id == "vec_drift")      return { "Vector", "Wander" };
    if (id == "vec_drift_rate") return { "Vector", "Wander Rate" };

    // "FM Osc4 > Osc5", "FM Osc2 Feedback", "FM Noise > Osc1", "FM Noise Colour".
    if (name.startsWith ("FM "))
        return { "FM", oscNumbers (name.substring (3)).replace (" > ", arrow()) };

    // "Osc3 String Decay" -> OSC 3 › String Decay.
    if (name.startsWith ("Osc") && juce::CharacterFunctions::isDigit (name[3]))
        return { "OSC " + juce::String::charToString (name[3]), name.substring (5) };

    if (id == "glide")             return { "Voice", "Glide" };
    if (id == "filter_balance")    return { "Filters", "Balance" };
    if (id == "subosc_level")      return { "Sub", "Level" };
    if (id == "master")            return { "Master", "Volume" };
    if (id == "master_clip_gain")  return { "Master", "Clip Gain" };
    if (id == "stretch")           return { "Keys", "Stretch Tuning" };
    if (id == "pedal_res")         return { "Keys", "Pedal Resonance" };
    if (id.startsWith ("mech_"))   return { "Keys", name };
    if (id == "body_coupling")     return { "Body", "Coupling" };
    if (id == "in_body")           return { "Input", "To Body" };
    if (id == "in_strings")        return { "Input", "To Strings" };

    for (const auto* module : { "Arp", "Spray", "Soundboard", "Body", "Input", "West", "Vector" })
        if (name.startsWith (juce::String (module) + " "))
            return { module, name.fromFirstOccurrenceOf (" ", false, false) };

    return { name, {} };
}
} // namespace detail

// A control the matrix used to name apart from its knob, named as the knob
// is labelled (V8-13: the knob says SEMI, so the route does too).
inline DestinationName asLabelled (DestinationName name)
{
    if (name.module.startsWith ("OSC ") && name.control == "Pitch")
        name.control = "Semi";
    return name;
}

inline DestinationName destinationParts (int destination)
{
    if (const auto param = Mod::paramDestinationIndex (destination); param >= 0)
    {
        const auto& entry = Mod::getParamDestinations()[(size_t) param];
        return asLabelled (detail::paramName (entry.id, entry.name));
    }

    return asLabelled (detail::explicitName (destination));
}

// "FILTER 1 › Cutoff". Cached: menus and the matrix ask for hundreds.
inline juce::String destination (int destination)
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (int d = 0; d < Mod::getNumDestinations(); ++d)
            result.add (destinationParts (d).full());
        return result;
    }();

    return juce::isPositiveAndBelow (destination, names.size()) ? names[destination] : juce::String();
}

//==============================================================================
// An oscillator's own level is TRIM on every page while it plays the
// Operator Env, whose output level is its LEVEL (UI review 8, I8-2): the
// matrix names it so then. The oscillator a level destination belongs to,
// or -1.
inline int levelDestinationOsc (int destination)
{
    using D = Mod::Destination;
    const D levels[] { D::Osc1Level, D::Osc2Level, D::SubLevel, D::Osc4Level, D::Osc5Level, D::Osc6Level };
    for (int osc = 0; osc < 6; ++osc)
        if (destination == (int) levels[osc])
            return osc;
    return -1;
}

inline bool playsOperatorEnv (const IlanaSynthAudioProcessor& processor, int osc)
{
    const char* const prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };
    if (! juce::isPositiveAndBelow (osc, 6))
        return false;
    const auto* value = processor.apvts.getRawParameterValue (juce::String (prefixes[osc]) + "_amp_env");
    return value != nullptr && juce::roundToInt (value->load()) == OperatorEg::envelopeChoice;
}

// "OSC 2 › Trim" while OSC 2 plays the Operator Env, else as destination().
inline DestinationName destinationParts (int destination, const IlanaSynthAudioProcessor& processor)
{
    if (const auto osc = levelDestinationOsc (destination); osc >= 0 && playsOperatorEnv (processor, osc))
        return { "OSC " + juce::String (osc + 1), "Trim" };
    return destinationParts (destination);
}

inline juce::String destination (int destination, const IlanaSynthAudioProcessor& processor)
{
    return levelDestinationOsc (destination) >= 0 ? destinationParts (destination, processor).full()
                                                  : ModNames::destination (destination);
}

// A destination list's six level items, renamed as the oscillators play
// (only when that changes: the lists are long).
inline void nameOperatorTrims (juce::ComboBox& combo, const IlanaSynthAudioProcessor& processor)
{
    using D = Mod::Destination;
    const D levels[] { D::Osc1Level, D::Osc2Level, D::SubLevel, D::Osc4Level, D::Osc5Level, D::Osc6Level };
    auto mask = 0;
    for (int osc = 0; osc < 6; ++osc)
        mask |= playsOperatorEnv (processor, osc) ? 1 << osc : 0;

    auto& properties = combo.getProperties();
    if (properties.contains ("operatorTrims") && (int) properties["operatorTrims"] == mask)
        return;

    const auto wasNamed = properties.contains ("operatorTrims") ? (int) properties["operatorTrims"] : 0;
    properties.set ("operatorTrims", mask);
    for (int osc = 0; osc < 6; ++osc)
        if (((mask ^ wasNamed) >> osc & 1) != 0)
        {
            const auto id = (int) levels[osc] + 1;
            const auto name = destination ((int) levels[osc], processor);
            // Read before renaming: the box reports an item selected only
            // while its label still matches the item's text.
            const auto selected = combo.getSelectedId() == id;
            combo.changeItemText (id, name);
            if (selected)
                combo.setText (name, juce::dontSendNotification);
        }
}

//==============================================================================
// Whether the module a destination belongs to is doing anything now: empty
// when it is, else why not ("FX Reverb is switched off", "OSC 3 is off").
// The matrix dims such rows and the macro card warns about them.
inline juce::String whyDestinationIsIdle (const IlanaSynthAudioProcessor& processor, int destination)
{
    if (destination <= 0)
        return {};

    const auto read = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };

    const auto parts = destinationParts (destination);
    const auto param = Mod::paramDestinationIndex (destination);
    const auto id = param >= 0 ? juce::String (Mod::getParamDestinations()[(size_t) param].id) : juce::String();

    // Effects: in a slot that isn't bypassed, and (for the classic modules)
    // switched on.
    const auto fxIdle = [&] (int type, const juce::String& moduleName, const char* enableId) -> juce::String
    {
        auto inRack = false, playing = false;

        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (juce::roundToInt (read ("fx_slot" + juce::String (slot))) == type)
            {
                inRack = true;
                playing = playing || read ("fx_slot" + juce::String (slot) + "_bypass") < 0.5f;
            }

        if (! inRack)
            return moduleName + " is not in the FX rack";
        if (! playing)
            return moduleName + " is bypassed";
        if (enableId != nullptr && read (enableId) < 0.5f)
            return moduleName + " is switched off";
        return {};
    };

    const auto enableFor = [] (int type) -> const char*
    {
        switch (type)
        {
            case 2:  return "fx_drive_on";
            case 3:  return "fx_crush_on";
            case 5:  return "fx_comb_on";
            case 6:  return "fx_phaser_on";
            case 7:  return "fx_chorus_on";
            case 9:  return "fx_delay_on";
            case 11: return "fx_smear_on";
            case 13: return "fx_reverb_on";
            default: return nullptr; // (Stutter and Freeze are momentary triggers)
        }
    };

    if (param >= 0)
    {
        if (id.startsWith ("fx_slot") && id.endsWith ("_mix"))
        {
            const auto slot = id.substring (7).getIntValue();
            if (juce::roundToInt (read ("fx_slot" + juce::String (slot))) == 0)
                return "FX slot " + juce::String (slot) + " is empty";
            if (read ("fx_slot" + juce::String (slot) + "_bypass") > 0.5f)
                return "FX slot " + juce::String (slot) + " is bypassed";
            return {};
        }

        if (const auto* fx = detail::fxModuleFor (id))
            return fxIdle (fx->type, parts.module, enableFor (fx->type));

        if (id.startsWith ("fx_aw"))
        {
            const auto module = id.substring (3).upToFirstOccurrenceOf ("_", false, false);
            auto type = 30;
            for (int c = 0; c < (int) airwindows::categoryModules().size(); ++c)
                if (module == airwindows::categoryModules()[(size_t) c].id)
                    type = airwindows::firstCategoryFxType + c;
            return fxIdle (type, parts.module, nullptr);
        }

        // The Operator Env's settings act only on an oscillator that plays
        // it; its pitch envelope and LFO while any oscillator does.
        const auto playsOperatorEnv = [&read] (int osc)
        {
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, 5, osc)]);
            return read (prefix + "_on") > 0.5f && juce::roundToInt (read (prefix + "_amp_env")) == OperatorEg::envelopeChoice;
        };
        if (id.startsWith ("opeg_"))
        {
            for (int osc = 0; osc < 6; ++osc)
                if (playsOperatorEnv (osc))
                    return {};
            return "no oscillator plays the Operator Env";
        }
        if (id.contains ("_eg_") && parts.module.startsWith ("OSC "))
        {
            const auto osc = parts.module.getTrailingIntValue() - 1;
            if (read (juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, 5, osc)]) + "_on") > 0.5f && ! playsOperatorEnv (osc))
                return parts.module + " doesn't play the Operator Env";
        }

        if (id.startsWith ("west_"))
            return read ("west_on") > 0.5f ? juce::String() : juce::String ("WEST is off");

        if (id.startsWith ("arp_"))
            return read ("arp_on") > 0.5f ? juce::String() : juce::String ("the arpeggiator is off");

        if (id.startsWith ("sb_"))
            return read ("sb_on") > 0.5f ? juce::String() : juce::String ("the soundboard is off");

        if (id.startsWith ("spray_"))
            return read ("spray_on") > 0.5f ? juce::String() : juce::String ("Spray is off");

        if (id.startsWith ("vec_"))
            return read ("vec_on") > 0.5f ? juce::String() : juce::String ("the vector pad is off");

        if (id == "subosc_level")
            return read ("subosc_on") > 0.5f ? juce::String() : juce::String ("the sub oscillator is off");
    }
    else
    {
        using D = Mod::Destination;
        static const std::pair<D, int> classicFx[] {
            { D::FxDriveAmount, 2 }, { D::FxCrushMix, 3 }, { D::FxCombFreq, 5 }, { D::FxPhaserRate, 6 },
            { D::FxChorusDepth, 7 }, { D::FxDelayMix, 9 }, { D::FxDelayFeedback, 9 }, { D::FxSmearMix, 11 },
            { D::FxFreezeMix, 12 }, { D::FxReverbMix, 13 }, { D::FxReverbSize, 13 }
        };

        for (const auto& [target, type] : classicFx)
            if ((D) destination == target)
                return fxIdle (type, parts.module, enableFor (type));

        if ((D) destination == D::Filter2Cutoff || (D) destination == D::Filter2Reso || (D) destination == D::Filter2Drive
            || (D) destination == D::Filter2Env || (D) destination == D::Filter2Fm || (D) destination == D::Filter2Morph)
            if (read ("west_on") > 0.5f && juce::roundToInt (read ("west_pos")) == 1)
                return "WEST replaces Filter 2";
    }

    // Oscillators (and the FM routes between them): the oscillator is on.
    const auto oscIdle = [&] (int osc) -> juce::String
    {
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, 5, osc)]);
        return read (prefix + "_on") > 0.5f ? juce::String() : "OSC " + juce::String (osc + 1) + " is off";
    };

    if (parts.module.startsWith ("OSC "))
    {
        const auto osc = parts.module.getTrailingIntValue() - 1;
        if (auto why = oscIdle (osc); why.isNotEmpty())
            return why;

        // Settings of one oscillator mode only act in that mode.
        const auto mode = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, 5, osc)]) + "_mode"));
        const auto control = parts.control;
        const auto needs = [&] (int wanted, const char* modeName) -> juce::String
        {
            return mode == wanted ? juce::String() : "OSC " + juce::String (osc + 1) + " isn't " + modeName;
        };

        if (id.contains ("_string_") || id.contains ("_bow_") || id.contains ("_hammer_") || id.endsWith ("_couple")
            || id.endsWith ("_damper") || id.endsWith ("_register") || id.endsWith ("_bridge_buzz") || id.endsWith ("_fret_rattle")
            || id.contains ("_ep_"))
            return needs (1, "physical");
        if (id.contains ("_grain_"))
            return needs (3, "granular");
        if (control == "Sample Start" || control == "Sample End")
            return needs (2, "a sample");
        return {};
    }

    if (parts.module == "FM")
    {
        // "OSC a → OSC b" or "OSC a Feedback": every oscillator named is on.
        const auto text = parts.control;
        for (int osc = 0; osc < 6; ++osc)
            if (text.contains ("OSC " + juce::String (osc + 1)))
                if (auto why = oscIdle (osc); why.isNotEmpty())
                    return why;
    }

    return {};
}
} // namespace ModNames
