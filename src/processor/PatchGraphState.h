#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "PatchGraph.h"

// Saving a patch graph inside a patch, and building the graph that stands
// for a patch made before graphs existed (roadmap step A's preset
// migration). Nothing in the shipping plugin saves a graph yet: step 17 adds
// the FX patcher's graph to the state, and loads a patch without one through
// rackGraph() below, which renders exactly as the fixed rack does
// (PatchGraphTests.inc proves it on every factory preset).
namespace PatchGraph
{
inline const juce::Identifier graphTag { "PatchGraph" };
inline const juce::Identifier nodeTag { "Node" };
inline const juce::Identifier cableTag { "Cable" };

inline juce::ValueTree toValueTree (const Graph& graph)
{
    juce::ValueTree tree (graphTag);
    tree.setProperty ("version", 1, nullptr);
    for (const auto& node : graph.nodes())
    {
        if (node.isFixed())
        {
            // Only their place in the view.
            juce::ValueTree child (nodeTag);
            child.setProperty ("id", (int) node.id, nullptr);
            child.setProperty ("x", node.x, nullptr);
            child.setProperty ("y", node.y, nullptr);
            tree.appendChild (child, nullptr);
            continue;
        }
        juce::ValueTree child (nodeTag);
        child.setProperty ("id", (int) node.id, nullptr);
        child.setProperty ("kind", node.kind, nullptr);
        child.setProperty ("ref", node.ref, nullptr);
        child.setProperty ("ins", node.numInputs, nullptr);
        child.setProperty ("outs", node.numOutputs, nullptr);
        child.setProperty ("x", node.x, nullptr);
        child.setProperty ("y", node.y, nullptr);
        if (node.bypassed)
            child.setProperty ("bypass", true, nullptr);
        if (node.keepAlive)
            child.setProperty ("keep", true, nullptr);
        if (! node.label.empty())
            child.setProperty ("label", juce::String (node.label), nullptr);
        tree.appendChild (child, nullptr);
    }
    for (const auto& cable : graph.cables())
    {
        juce::ValueTree child (cableTag);
        child.setProperty ("from", (int) cable.from.node, nullptr);
        child.setProperty ("fromPort", cable.from.port, nullptr);
        child.setProperty ("to", (int) cable.to.node, nullptr);
        child.setProperty ("toPort", cable.to.port, nullptr);
        if (cable.gain != 1.0f)
            child.setProperty ("gain", cable.gain, nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

// A saved graph back. Anything out of range (a bad id, a port that doesn't
// exist, a cable to a node that isn't there, a gain that isn't finite) is
// dropped rather than trusted, so a damaged patch still loads.
inline Graph fromValueTree (const juce::ValueTree& tree)
{
    Graph graph;
    if (! tree.hasType (graphTag))
        return graph;

    for (const auto& child : tree)
    {
        if (! child.hasType (nodeTag))
            continue;
        const auto id = (int) child.getProperty ("id", 0);
        const auto x = (float) (double) child.getProperty ("x", 0.0);
        const auto y = (float) (double) child.getProperty ("y", 0.0);
        if (id == (int) inputNodeId || id == (int) outputNodeId)
        {
            graph.moveNode ((NodeId) id, std::isfinite (x) ? x : 0.0f, std::isfinite (y) ? y : 0.0f);
            continue;
        }
        if (id <= (int) outputNodeId)
            continue;
        Node node;
        node.id = (NodeId) id;
        node.kind = (int) child.getProperty ("kind", 0);
        node.ref = (int) child.getProperty ("ref", 0);
        node.numInputs = (int) child.getProperty ("ins", 1);
        node.numOutputs = (int) child.getProperty ("outs", 1);
        node.x = std::isfinite (x) ? x : 0.0f;
        node.y = std::isfinite (y) ? y : 0.0f;
        node.bypassed = (bool) child.getProperty ("bypass", false);
        node.keepAlive = (bool) child.getProperty ("keep", false);
        node.label = child.getProperty ("label").toString().toStdString();
        graph.restoreNode (node);
    }

    for (const auto& child : tree)
    {
        if (! child.hasType (cableTag))
            continue;
        const Port from { (NodeId) (int) child.getProperty ("from", 0), (int) child.getProperty ("fromPort", 0) };
        const Port to { (NodeId) (int) child.getProperty ("to", 0), (int) child.getProperty ("toPort", 0) };
        auto gain = (float) (double) child.getProperty ("gain", 1.0);
        if (! std::isfinite (gain))
            gain = 1.0f;
        graph.connect (from, to, juce::jlimit (-16.0f, 16.0f, gain));
    }
    return graph;
}

//==============================================================================
// The fixed FX rack as a graph. Each slot that holds an effect becomes a
// node (kind = the FX type, ref = the slot number, 1..numSlots); a bypassed
// slot is a bypassed node. SERIES chains them Input -> slot 1 -> ... ->
// Output. PARALLEL feeds every working slot from Input and sums them into
// Output at 1 / N (1 / number soloed while any slot is soloed; the others
// keep running for their tails and meters, unheard), which is exactly what
// processEffectsParallel does. Nodes sit in one row (series) or one column
// (parallel) on the cable view's 10 px grid, evenly spaced.
struct RackSlot
{
    int type = 0;        // 0: empty
    bool bypassed = false;
    bool solo = false;
    std::string label;
};

inline Graph rackGraph (const std::vector<RackSlot>& slots, bool parallel)
{
    Graph graph;
    // 130 px nodes, 50 px apart in a row (90 in parallel, room for the level
    // tags), rows 70 px apart; INPUT is 90 px wide.
    constexpr float step = 180.0f, rowStep = 70.0f;

    std::vector<NodeId> ids;
    auto active = 0, soloed = 0, placed = 0;
    for (size_t s = 0; s < slots.size(); ++s)
    {
        const auto& slot = slots[s];
        if (slot.type == 0)
        {
            ids.push_back (0);
            continue;
        }
        const auto index = (float) placed++;
        const auto id = parallel ? graph.addNode (slot.type, 1, 1, step, rowStep * index, slot.label, (int) s + 1)
                                 : graph.addNode (slot.type, 1, 1, step * (index + 1), 0.0f, slot.label, (int) s + 1);
        graph.setBypassed (id, slot.bypassed);
        ids.push_back (id);
        if (! slot.bypassed)
        {
            ++active;
            soloed += slot.solo ? 1 : 0;
        }
    }

    if (! parallel)
    {
        auto previous = inputNodeId;
        for (const auto id : ids)
        {
            if (id == 0)
                continue;
            graph.connect ({ previous, 0 }, { id, 0 });
            previous = id;
        }
        graph.connect ({ previous, 0 }, { outputNodeId, 0 });
        graph.moveNode (inputNodeId, 40.0f, 0.0f);
        graph.moveNode (outputNodeId, step * (float) (placed + 1), 0.0f);
        return graph;
    }

    // Parallel: bypassed slots don't run in the old rack either; they stay
    // as unconnected bypassed nodes so the view still shows them.
    const auto gain = active > 0 ? 1.0f / (float) (soloed > 0 ? soloed : active) : 1.0f;
    for (size_t s = 0; s < slots.size(); ++s)
    {
        const auto id = ids[s];
        if (id == 0 || slots[s].bypassed)
            continue;
        graph.connect ({ inputNodeId, 0 }, { id, 0 });
        if (soloed == 0 || slots[s].solo)
            graph.connect ({ id, 0 }, { outputNodeId, 0 }, gain);
        else
            graph.setKeepAlive (id, true);
    }
    if (active == 0)
        graph.connect ({ inputNodeId, 0 }, { outputNodeId, 0 });

    const auto middle = rowStep * (float) juce::jmax (0, placed - 1) * 0.5f;
    graph.moveNode (inputNodeId, 0.0f, middle);
    graph.moveNode (outputNodeId, step + 130.0f + 90.0f, middle);
    return graph;
}
} // namespace PatchGraph
