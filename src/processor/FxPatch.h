#pragma once

#include <array>
#include <utility>
#include <vector>

#include <juce_data_structures/juce_data_structures.h>

#include "PatchGraph.h"
#include "PatchGraphState.h"
#include "../dsp/airwindows/Categories.h"

// The FX patcher's part of a patch (roadmap step 17), kept outside the
// parameters: which rack slots are their own INSTANCE of an effect, and the
// patcher's cable graph.
//
// INSTANCES. The parameters hold one set of settings per effect type
// (fx_delay_time, fx_reverb_mix...), and the rack's slots share them, as they
// always did. A slot made its own instance gets its own engine (its own
// delay lines and filters, FxEngine.h) and, while its card isn't the one being
// edited, its own copy of its effect's settings (`values`). The type's
// parameters always hold the settings of the one card being edited; picking
// another instance's card swaps the two (FxPatcher.cpp, focusFxInstance), so
// every control, help line, MIDI learn and undo works on whichever instance is
// in front. A patch with no instances is exactly the rack it always was.
//
// GRAPH. With PATCHER on (fx_patcher), the slots are nodes of this graph
// (node.ref = slot number, 1..10) and cables decide what feeds what, loops
// included (PatchGraph.h). Off, the rack runs SERIES or PARALLEL as before.
namespace FxPatchData
{
inline constexpr int numSlots = 10;

struct Instance
{
    int type = 0;           // the effect it was made for; a slot whose type changes loses it
    bool own = false;       // runs on its own engine
    bool hasValues = false; // reads `values` (true) or the type's parameters (its card is in front)
    std::vector<std::pair<juce::String, float>> values;

    bool isDefault() const { return ! own && ! hasValues; }
    bool operator== (const Instance& other) const
    {
        return type == other.type && own == other.own && hasValues == other.hasValues && values == other.values;
    }
};

struct Patch
{
    std::array<Instance, numSlots> slots;
    PatchGraph::Graph graph;
    bool hasGraph = false;

    bool anyInstances() const
    {
        for (const auto& slot : slots)
            if (! slot.isDefault())
                return true;
        return false;
    }

    bool isDefault() const { return ! anyInstances() && ! hasGraph; }
};

// Which parameters are an effect type's own settings: the ones an instance
// keeps a copy of. By ID prefix; every effect's parameters start with one of
// its prefixes (tests/FxPatcherTests.inc checks no prefix catches another
// type's parameter).
inline std::vector<juce::String> paramPrefixes (int type)
{
    switch (type)
    {
        case 1:  return { "fx_amp_" };
        case 2:  return { "fx_drive_", "fx_fold" };
        case 3:  return { "fx_crush_" };
        case 4:  return { "fx_comp_" };
        case 5:  return { "fx_comb_" };
        case 6:  return { "fx_phaser_" };
        case 7:  return { "fx_chorus_" };
        case 8:  return { "fx_haas_" };
        case 9:  return { "fx_delay_", "fx_taps_" };
        case 10: return { "fx_stutter_" };
        case 11: return { "fx_smear_" };
        case 12: return { "fx_freeze_" };
        case 13: return { "fx_reverb_" };
        case 14: return { "fx_flanger_" };
        case 15: return { "fx_dim_" };
        case 16: return { "fx_gate_" };
        case 17: return { "fx_tape_stop_" };
        case 18: return { "fx_tilt" };
        case 19: return { "fx_util_" };
        case 20: return { "fx_ott_" };
        case 21: return { "fx_limit_" };
        case 22: return { "fx_width" };
        case 23: return { "fx_trem_" };
        case 24: return { "fx_shifter_" };
        case 25: return { "fx_ring_" };
        case 26: return { "fx_octaver_" };
        case 27: return { "fx_vowel_" };
        case 28: return { "fx_feedback_" };
        case 29: return { "fx_eq_" };
        case 30: return { "fx_aw_" };
        case 31: return { "fx_voc_" };
        case 42: return { "fx_filter_" };
        default: break;
    }
    if (const auto category = airwindows::categoryForFxType (type); category >= 0)
        return { juce::String ("fx_") + airwindows::categoryModules()[(size_t) category].id + "_" };
    return {};
}

inline bool isParamOf (int type, const juce::String& id)
{
    for (const auto& prefix : paramPrefixes (type))
        if (id.startsWith (prefix))
            return true;
    return false;
}

//==============================================================================
inline const juce::Identifier patchTag { "FxPatch" };
inline const juce::Identifier slotTag { "Instance" };

inline juce::ValueTree toValueTree (const Patch& patch)
{
    juce::ValueTree tree (patchTag);
    tree.setProperty ("version", 1, nullptr);
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& slot = patch.slots[(size_t) s];
        if (slot.isDefault())
            continue;
        juce::ValueTree child (slotTag);
        child.setProperty ("slot", s + 1, nullptr);
        child.setProperty ("type", slot.type, nullptr);
        child.setProperty ("own", slot.own, nullptr);
        if (slot.hasValues)
        {
            juce::String text;
            for (const auto& [id, value] : slot.values)
                text << id << "=" << juce::String (value, 7) << ";";
            child.setProperty ("values", text, nullptr);
        }
        tree.appendChild (child, nullptr);
    }
    if (patch.hasGraph)
        tree.appendChild (PatchGraph::toValueTree (patch.graph), nullptr);
    return tree;
}

// Anything out of range is dropped rather than trusted (a damaged patch
// still loads, without the part that was damaged).
inline Patch fromValueTree (const juce::ValueTree& tree)
{
    Patch patch;
    if (! tree.hasType (patchTag))
        return patch;

    for (const auto& child : tree)
    {
        if (child.hasType (PatchGraph::graphTag))
        {
            patch.graph = PatchGraph::fromValueTree (child);
            patch.hasGraph = true;
            continue;
        }
        if (! child.hasType (slotTag))
            continue;
        const auto s = (int) child.getProperty ("slot", 0) - 1;
        if (! juce::isPositiveAndBelow (s, numSlots))
            continue;
        auto& slot = patch.slots[(size_t) s];
        slot.type = (int) child.getProperty ("type", 0);
        slot.own = (bool) child.getProperty ("own", false);
        if (child.hasProperty ("values"))
        {
            slot.hasValues = true;
            for (const auto& token : juce::StringArray::fromTokens (child.getProperty ("values").toString(), ";", ""))
            {
                const auto id = token.upToFirstOccurrenceOf ("=", false, false);
                const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
                if (id.isNotEmpty() && isParamOf (slot.type, id) && std::isfinite (value))
                    slot.values.emplace_back (id, value);
            }
        }
    }
    return patch;
}

inline bool sameGraph (const PatchGraph::Graph& a, const PatchGraph::Graph& b)
{
    return PatchGraph::toValueTree (a).isEquivalentTo (PatchGraph::toValueTree (b));
}

inline bool samePatch (const Patch& a, const Patch& b)
{
    return a.slots == b.slots && a.hasGraph == b.hasGraph && (! a.hasGraph || sameGraph (a.graph, b.graph));
}
} // namespace FxPatchData
