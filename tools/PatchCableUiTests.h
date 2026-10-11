// Roadmap step A: the cable view (gui/PatchCableView.h) driven with the
// mouse: a cable dragged from an output to an input, one lifted off an input
// and dropped on empty space, a node dragged onto the grid, the right-click
// menus, Delete, and a loop drawn as a loop. Included by Snapshot.cpp after
// its helpers; runs from runUiTests (ILANA_UITEST_ONLY=PG for just these).
// ILANA_SNAPSHOT_GRAPH=1 writes pictures of the view instead (see
// snapshotPatchCableView).
#pragma once

#include "gui/PatchCableView.h"
#include "gui/FxLibrary.h"
#include "processor/PatchGraphState.h"

namespace PatchCableUi
{
inline juce::MouseEvent mouse (juce::Component& component, juce::Point<float> position, bool down, bool popup = false)
{
    auto mods = down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys();
    if (popup)
        mods = juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier);
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1, false);
}

inline void dragFromTo (PatchCableView& view, juce::Point<float> from, juce::Point<float> to)
{
    view.mouseDown (mouse (view, from, true));
    for (int step = 1; step <= 6; ++step)
        view.mouseDrag (mouse (view, from + (to - from) * ((float) step / 6.0f), true));
    view.mouseUp (mouse (view, to, false));
}

// The view set up as an owner would: effect colours, a CPU readout and
// every edit counted.
struct Harness
{
    PatchGraph::Graph graph;
    PatchCableView view { graph };
    int cableEdits = 0, nodeEdits = 0, layoutEdits = 0;

    explicit Harness (PatchGraph::Graph g) : graph (std::move (g))
    {
        view.setSize (860, 360);
        view.colourOf = [] (const PatchGraph::Node& node) { return fxColour (node.kind); };
        view.cpuOf = [] (PatchGraph::NodeId id) { return 0.002f * (float) (id % 7); };
        view.onEdited = [this] (PatchCableView::Edit edit)
        {
            (edit == PatchCableView::Edit::cables ? cableEdits : edit == PatchCableView::Edit::nodes ? nodeEdits : layoutEdits)++;
        };
    }
};

// Three effects in a chain, a fourth in parallel with the middle one and a
// feedback cable around the last: the picture of what step 17 will offer.
inline PatchGraph::Graph demoGraph()
{
    PatchGraph::Graph graph;
    const auto drive = graph.addNode (2, 1, 1, 140.0f, 40.0f, "DRIVE", 1);
    const auto chorus = graph.addNode (7, 1, 1, 320.0f, 0.0f, "CHORUS", 2);
    const auto phaser = graph.addNode (6, 1, 1, 320.0f, 90.0f, "PHASER", 3);
    const auto delay = graph.addNode (9, 1, 1, 500.0f, 40.0f, "DELAY", 4);
    graph.moveNode (PatchGraph::inputNodeId, 0.0f, 40.0f);
    graph.moveNode (PatchGraph::outputNodeId, 680.0f, 40.0f);
    graph.connect ({ PatchGraph::inputNodeId, 0 }, { drive, 0 });
    graph.connect ({ drive, 0 }, { chorus, 0 });
    graph.connect ({ drive, 0 }, { phaser, 0 }, 0.7f);
    graph.connect ({ chorus, 0 }, { delay, 0 });
    graph.connect ({ phaser, 0 }, { delay, 0 });
    graph.connect ({ delay, 0 }, { drive, 0 }, 0.5f);
    graph.connect ({ delay, 0 }, { PatchGraph::outputNodeId, 0 });
    return graph;
}
} // namespace PatchCableUi

inline void runPatchCableTests (IlanaSynthAudioProcessor& processor)
{
    using namespace PatchCableUi;
    using PatchGraph::inputNodeId;
    using PatchGraph::outputNodeId;

    // A cable dragged from Input's output to a node's input is made; a drop
    // on empty space makes nothing.
    {
        PatchGraph::Graph graph;
        const auto a = graph.addNode (2, 1, 1, 160.0f, 40.0f, "DRIVE");
        graph.moveNode (inputNodeId, 0.0f, 40.0f);
        graph.moveNode (outputNodeId, 340.0f, 40.0f);
        Harness h (graph);
        const auto out = h.view.portPosition ({ inputNodeId, 0 }, false);
        const auto in = h.view.portPosition ({ a, 0 }, true);
        dragFromTo (h.view, out, in);
        expect (h.graph.findCable ({ inputNodeId, 0 }, { a, 0 }) >= 0 && h.cableEdits == 1,
                "cable view: dragging Input's port onto DRIVE's input makes a cable");
        dragFromTo (h.view, h.view.portPosition ({ a, 0 }, false), { 600.0f, 300.0f });
        expect (h.graph.cables().size() == 1 && h.cableEdits == 1, "cable view: a cable dropped on empty space makes nothing");
        dragFromTo (h.view, h.view.portPosition ({ a, 0 }, false), h.view.portPosition ({ outputNodeId, 0 }, true));
        expect (h.graph.cables().size() == 2 && compile (h.graph).processCount() == 1, "cable view: DRIVE -> OUTPUT completes a chain that compiles");

        // Lifting the cable off OUTPUT's input and dropping it on nothing removes it.
        dragFromTo (h.view, h.view.portPosition ({ outputNodeId, 0 }, true), { 600.0f, 300.0f });
        expect (h.graph.findCable ({ a, 0 }, { outputNodeId, 0 }) < 0 && h.cableEdits == 3,
                "cable view: a cable lifted off an input and dropped on nothing is removed");

        // A node dragged lands on the 10 px grid and counts as a layout edit only.
        const auto revision = h.graph.revision();
        const auto grab = h.view.nodeBounds (*h.graph.find (a)).getCentre();
        dragFromTo (h.view, grab, grab + juce::Point<float> (33.0f, 17.0f));
        const auto* node = h.graph.find (a);
        expect (std::fmod (node->x, 10.0f) == 0.0f && std::fmod (node->y, 10.0f) == 0.0f && node->x == 190.0f && node->y == 60.0f
                    && h.graph.revision() == revision && h.layoutEdits == 1,
                "cable view: a node moves on the 10 px grid without a recompile (at " + juce::String (node->x) + ", " + juce::String (node->y) + ")");
    }

    // Right-click menus and Delete.
    {
        Harness h (demoGraph());
        const auto cables = h.graph.cables().size();
        const auto feedback = h.graph.feedbackCables();
        expect (feedback.size() == cables && feedback[5] && std::count (feedback.begin(), feedback.end(), true) == 1,
                "cable view: the demo's DELAY -> DRIVE cable is its one loop");

        // A point on the CHORUS -> DELAY cable: the middle of its ports (a
        // forward cable is a symmetric curve).
        const auto from = h.view.portPosition ({ 4, 0 }, false), to = h.view.portPosition ({ 6, 0 }, true);
        juce::StringArray items;
        int pick = 0;
        h.view.openMenu = [&items, &pick] (juce::PopupMenu& menu, std::function<void (int)> done)
        {
            items.clear();
            for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
                items.add (it.getItem().text);
            if (done)
                done (pick);
        };
        const auto mid = (from + to) * 0.5f;
        const auto under = h.view.cableAt (mid);
        expect (under >= 0 && under == h.graph.findCable ({ 4, 0 }, { 6, 0 }), "cable view: the cable under the pointer is CHORUS -> DELAY");
        const auto target = h.graph.cables()[(size_t) under];
        pick = 1; // Remove cable
        h.view.mouseDown (mouse (h.view, mid, true, true));
        expect (items.contains ("Remove cable") && items.contains ("100 %") && h.graph.findCable (target.from, target.to) < 0,
                "cable view: right-click a cable, Remove cable removes it (menu: " + items.joinIntoString (", ") + ")");

        // The loop cable's menu says what it costs.
        const auto loopIndex = [&h]
        {
            const auto fb = h.graph.feedbackCables();
            for (size_t i = 0; i < fb.size(); ++i)
                if (fb[i])
                    return (int) i;
            return -1;
        }();
        // The loop runs under the nodes; its lowest point is halfway along.
        const auto loopFrom = h.view.portPosition ({ 6, 0 }, false), loopTo = h.view.portPosition ({ 3, 0 }, true);
        auto lowest = 0.0f;
        for (const auto& n : h.graph.nodes())
            lowest = juce::jmax (lowest, h.view.nodeBounds (n).getBottom());
        const juce::Point<float> loopPoint ((loopFrom.x + loopTo.x) * 0.5f, lowest + 20.0f);
        expect (loopIndex >= 0 && h.view.cableAt (loopPoint) == loopIndex, "cable view: the loop cable is found under the nodes");
        pick = 12; // 50 %
        h.view.mouseDown (mouse (h.view, loopPoint, true, true));
        expect (items.contains ("Loop: 1.3 ms delay") && h.graph.cables()[(size_t) loopIndex].gain == 0.5f,
                "cable view: a loop cable's menu shows its 1.3 ms delay and sets its level");

        // Node menu: bypass, then Delete removes the selected node.
        const auto phaser = h.view.nodeBounds (*h.graph.find (5)).getCentre();
        pick = 30;
        h.view.mouseDown (mouse (h.view, phaser, true, true));
        expect (h.graph.find (5)->bypassed && h.nodeEdits == 1, "cable view: right-click a node, Bypass bypasses it");
        h.view.mouseDown (mouse (h.view, phaser, true));
        h.view.mouseUp (mouse (h.view, phaser, false));
        h.view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
        expect (h.graph.find (5) == nullptr && h.nodeEdits == 2, "cable view: Delete removes the selected node and its cables");

        // INPUT and OUTPUT can't be removed.
        h.view.mouseDown (mouse (h.view, h.view.nodeBounds (*h.graph.find (inputNodeId)).getCentre(), true));
        h.view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
        expect (h.graph.find (inputNodeId) != nullptr, "cable view: INPUT stays when Delete is pressed on it");
    }

    // A factory preset's rack, migrated, shows every effect with a cable path
    // from INPUT to OUTPUT, and the view paints without drawing outside itself.
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Neuro Wobble")));
        Harness h (processor.getFxRackGraph());
        auto right = 0.0f, bottom = 0.0f;
        for (const auto& node : h.graph.nodes())
        {
            right = juce::jmax (right, node.x + PatchCableView::nodeWidth);
            bottom = juce::jmax (bottom, node.y + PatchCableView::nodeHeight);
        }
        h.view.setSize (juce::jmax (860, (int) right + 60), juce::jmax (160, (int) bottom + 60));
        h.view.centreContent();
        auto inside = true;
        for (const auto& node : h.graph.nodes())
            inside = inside && h.view.getLocalBounds().toFloat().contains (h.view.nodeBounds (node));
        const auto image = h.view.createComponentSnapshot (h.view.getLocalBounds(), true, 1.0f);
        expect (h.graph.nodes().size() > 2 && inside && image.isValid() && compile (h.graph).ok,
                "cable view: Neuro Wobble's rack shows " + juce::String ((int) h.graph.nodes().size() - 2) + " effects inside the view");
    }
}

// ILANA_SNAPSHOT_GRAPH: pictures of the cable view, at twice the size:
// graph-demo.png (a chain, a parallel branch at 70 % and a loop),
// graph-series.png and graph-parallel.png (a factory rack migrated both
// ways) and graph-dragging.png (a cable on its way to an input).
inline void snapshotPatchCableView (IlanaSynthAudioProcessor& processor, const juce::File& outDir)
{
    using namespace PatchCableUi;
    const auto write = [&outDir] (PatchCableView& view, const juce::String& name)
    {
        const auto image = view.createComponentSnapshot (view.getLocalBounds(), true, 2.0f);
        juce::FileOutputStream stream (outDir.getChildFile (name));
        stream.setPosition (0);
        stream.truncate();
        juce::PNGImageFormat().writeImageToStream (image, stream);
    };

    {
        Harness h (demoGraph());
        h.view.setSize (860, 240);
        h.view.centreContent();
        write (h.view, "graph-demo.png");

        // A cable on its way from PHASER to OUTPUT.
        const auto from = h.view.portPosition ({ 5, 0 }, false);
        h.view.mouseDown (mouse (h.view, from, true));
        h.view.mouseDrag (mouse (h.view, h.view.portPosition ({ PatchGraph::outputNodeId, 0 }, true).translated (-2.0f, 1.0f), true));
        write (h.view, "graph-dragging.png");
        h.view.mouseUp (mouse (h.view, { 5.0f, 5.0f }, false));
    }

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Neuro Wobble")));
    for (const auto parallel : { false, true })
    {
        if (auto* routing = processor.apvts.getParameter ("fx_routing"))
            routing->setValueNotifyingHost (parallel ? 1.0f : 0.0f);
        Harness h (processor.getFxRackGraph());
        auto right = 0.0f, bottom = 0.0f;
        for (const auto& node : h.graph.nodes())
        {
            right = juce::jmax (right, node.x + PatchCableView::nodeWidth);
            bottom = juce::jmax (bottom, node.y + PatchCableView::nodeHeight);
        }
        h.view.setSize (juce::jmax (860, (int) right + 60), juce::jmax (parallel ? 240 : 140, (int) bottom + 80));
        h.view.centreContent();
        write (h.view, parallel ? "graph-parallel.png" : "graph-series.png");
    }
}
