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
    int cableEdits = 0, nodeEdits = 0, layoutEdits = 0, loopEdits = 0, gestures = 0;

    explicit Harness (PatchGraph::Graph g) : graph (std::move (g))
    {
        view.setSize (860, 360);
        view.colourOf = [] (const PatchGraph::Node& node) { return fxColour (node.kind); };
        view.cpuOf = [] (PatchGraph::NodeId id) { return 0.002f * (float) (id % 7); };
        view.onEdited = [this] (PatchCableView::Edit edit)
        {
            (edit == PatchCableView::Edit::cables ? cableEdits : edit == PatchCableView::Edit::nodes ? nodeEdits
                                                              : edit == PatchCableView::Edit::loop ? loopEdits : layoutEdits)++;
        };
        view.onGesture = [this] (bool starting) { gestures += starting ? 1 : 0; };
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

    // Cable motion (step 47): still at rest, swings when a box is dragged,
    // stays bounded and settles; the graph and its compiled plan never see it.
    {
        Harness h (demoGraph());
        const auto revision = h.graph.revision();
        expect (! h.view.isAnimating() && h.view.largestSwing() == 0.0f, "cable motion: a fresh patcher is at rest");

        const auto a = h.graph.nodes()[1].id;
        const auto grab = h.view.nodeBounds (*h.graph.find (a)).getCentre();
        h.view.mouseDown (mouse (h.view, grab, true));
        auto peak = 0.0f;
        for (int step = 1; step <= 30; ++step)
        {
            h.view.mouseDrag (mouse (h.view, grab + juce::Point<float> (0.0f, 4.0f * (float) step), true));
            h.view.advanceMotion (1.0f / 60.0f);
            peak = juce::jmax (peak, h.view.largestSwing());
        }
        expect (peak > 3.0f && peak <= 40.0f && h.view.isAnimating(),
                "cable motion: dragging a box swings its cables (peak " + juce::String (peak, 1) + " px, at most 40)");
        h.view.mouseUp (mouse (h.view, grab + juce::Point<float> (0.0f, 120.0f), false));

        auto frames = 0;
        while (h.view.advanceMotion (1.0f / 60.0f) && frames < 600)
            ++frames;
        expect (! h.view.isAnimating() && h.view.largestSwing() == 0.0f && frames < 240,
                "cable motion: the cables settle and stop (" + juce::String (frames) + " frames)");
        expect (h.graph.revision() == revision, "cable motion: the swing never touches the graph");

        // Cost of one paint of the whole patch, plain curves against hanging ones.
        juce::Image image (juce::Image::ARGB, 860, 360, true);
        for (const auto motion : { false, true })
        {
            h.view.setMotion (motion);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 200; ++i)
            {
                juce::Graphics g (image);
                h.view.paintEntireComponent (g, false);
            }
            std::cout << "  cable view paint, motion " << (motion ? "on " : "off") << ": "
                      << juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 200.0, 3) << " ms\n";
        }
        h.view.setMotion (true);
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
        expect (items.contains ("Loop: 1.3 ms delay (set it below)") && h.graph.cables()[(size_t) loopIndex].gain == 0.5f,
                "cable view: a loop cable's menu shows its 1.3 ms delay and sets its level (menu: " + items.joinIntoString (", ") + ")");

        // The loop's tag (halfway along, where loopPoint is): dragging it up
        // raises the level live, in one gesture, up to the 70 % top; the
        // wheel nudges it by 1 %; its delay shows on the tag.
        const auto tag = h.view.loopTagBounds (h.graph.cables()[(size_t) loopIndex]);
        expect (tag.contains (loopPoint) && h.view.loopTagAt (loopPoint) == loopIndex, "cable view: the loop's tag sits halfway along it");
        const auto edits = h.loopEdits;
        h.view.mouseDown (mouse (h.view, tag.getCentre(), true));
        h.view.mouseDrag (mouse (h.view, tag.getCentre().translated (0.0f, -20.0f), true));
        const auto raised = h.graph.cables()[(size_t) loopIndex].gain;
        h.view.mouseDrag (mouse (h.view, tag.getCentre().translated (0.0f, -400.0f), true));
        h.view.mouseUp (mouse (h.view, tag.getCentre().translated (0.0f, -400.0f), false));
        expect (std::abs (raised - 0.6f) < 1.0e-4f && h.graph.cables()[(size_t) loopIndex].gain == 0.7f && h.loopEdits == edits + 2 && h.gestures == 1
                    && h.view.getSelectedCable() == loopIndex,
                "cable view: dragging a loop's tag sets its level live, up to 70 %, in one gesture (" + juce::String (raised) + ", "
                    + juce::String (h.loopEdits - edits) + " edits, " + juce::String (h.gestures) + " gestures)");
        juce::MouseWheelDetails wheel {};
        wheel.deltaY = -0.1f;
        h.view.mouseWheelMove (mouse (h.view, tag.getCentre(), false), wheel);
        expect (std::abs (h.graph.cables()[(size_t) loopIndex].gain - 0.69f) < 1.0e-4f && h.gestures == 2, "cable view: the wheel on a loop's tag nudges its level by 1 %");
        h.graph.setCableDelay (loopIndex, 250.0f);
        expect (h.view.loopDelayText (h.graph.cables()[(size_t) loopIndex]) == "250 ms" && h.view.loopDelayText ({}) == "1.3 ms",
                "cable view: a loop's tag shows its delay (the shortest when unset)");

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

    // Navigation (ilana 2026-10-12): scroll and two-finger swipes pan by
    // their delta, ctrl + scroll and a pinch zoom around the pointer, keys,
    // middle-drag, all inside the zoom range and reported to the owner.
    {
        Harness h (demoGraph());
        auto navigations = 0;
        h.view.onNavigated = [&navigations] { ++navigations; };
        h.view.setZoom (1.0f);
        h.view.centreContent();
        const auto under = [&h] (juce::Point<float> at) { return at / h.view.getZoom() - h.view.getOrigin(); };
        const juce::Point<float> pointer (300.0f, 150.0f);
        const auto pinned = under (pointer);

        juce::MouseWheelDetails wheel {};
        wheel.deltaY = 0.5f;
        h.view.mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pointer, juce::ModifierKeys (juce::ModifierKeys::ctrlModifier),
                                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &h.view, &h.view, juce::Time::getCurrentTime(), pointer,
                                                 juce::Time::getCurrentTime(), 1, false),
                               wheel);
        expect (h.view.getZoom() > 1.3f && under (pointer).getDistanceFrom (pinned) < 0.01f && navigations == 1,
                "cable view: ctrl + scroll zooms in around the pointer (" + juce::String (h.view.getZoom(), 3) + ")");

        const auto before = h.view.getOrigin();
        const auto zoom = h.view.getZoom();
        wheel = {};
        wheel.deltaY = -0.1f;
        wheel.deltaX = 0.05f;
        h.view.mouseWheelMove (mouse (h.view, pointer, false), wheel);
        const auto moved = (h.view.getOrigin() - before) * zoom;
        expect (std::abs (moved.y + 36.0f) < 0.01f && std::abs (moved.x - 18.0f) < 0.01f && h.view.getZoom() == zoom,
                "cable view: a two-finger swipe pans by its delta in both directions, without zooming (" + moved.toString() + ")");

        const auto pinnedAgain = under (pointer);
        h.view.mouseMagnify (mouse (h.view, pointer, false), 0.5f);
        expect (std::abs (h.view.getZoom() - zoom * 0.5f) < 1.0e-4f && under (pointer).getDistanceFrom (pinnedAgain) < 0.01f,
                "cable view: a pinch zooms around the fingers");
        h.view.mouseMagnify (mouse (h.view, pointer, false), 100.0f);
        expect (h.view.getZoom() == PatchCableView::maxViewZoom, "cable view: zoom stops at its largest");
        h.view.mouseMagnify (mouse (h.view, pointer, false), 0.0001f);
        expect (h.view.getZoom() == PatchCableView::minViewZoom, "cable view: and at its smallest");

        h.view.keyPressed (juce::KeyPress ('0'));
        const auto actual = h.view.getZoom();
        h.view.keyPressed (juce::KeyPress ('+'));
        const auto zoomedIn = h.view.getZoom();
        h.view.keyPressed (juce::KeyPress ('-'));
        expect (actual == 1.0f && std::abs (zoomedIn - 1.25f) < 1.0e-4f && std::abs (h.view.getZoom() - 1.0f) < 1.0e-4f,
                "cable view: 0 is 100 %, + and - step the zoom");
        auto fits = 0;
        h.view.onFitRequested = [&fits] { ++fits; };
        h.view.keyPressed (juce::KeyPress ('f'));
        expect (fits == 1, "cable view: F asks the owner to fit");

        // The middle button pans from on top of a box, and the box stays put.
        const auto* delay = h.graph.find (4);
        const auto boxAt = juce::Point<float> (delay->x, delay->y);
        const auto grab = h.view.nodeBounds (*delay).getCentre() * h.view.getZoom();
        const auto originBefore = h.view.getOrigin();
        const auto middle = [&h] (juce::Point<float> at, bool down)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                     down ? juce::ModifierKeys (juce::ModifierKeys::middleButtonModifier) : juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &h.view, &h.view, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 1, false);
        };
        h.view.mouseDown (middle (grab, true));
        h.view.mouseDrag (middle (grab + juce::Point<float> (40.0f, 25.0f), true).withNewPosition (grab + juce::Point<float> (40.0f, 25.0f)));
        h.view.mouseUp (middle (grab + juce::Point<float> (40.0f, 25.0f), false));
        const auto* still = h.graph.find (4);
        expect (juce::Point<float> (still->x, still->y) == boxAt && (h.view.getOrigin() - originBefore).getDistanceFrom ({ 40.0f, 25.0f }) < 0.01f,
                "cable view: a middle-button drag pans the view, even from a box");

        // Zoomed in, the map shows; a click in it moves the view there.
        h.view.setZoom (2.5f);
        h.view.centreContent();
        const auto map = h.view.mapBounds();
        expect (! map.isEmpty() && h.view.getLocalBounds().toFloat().contains (map), "cable view: while part of the patch is off screen a map shows");
        if (! map.isEmpty())
        {
            const auto originAtMap = h.view.getOrigin();
            h.view.mouseDown (mouse (h.view, map.getBottomLeft().translated (10.0f, -10.0f), true));
            h.view.mouseUp (mouse (h.view, map.getBottomLeft().translated (10.0f, -10.0f), false));
            expect (h.view.getOrigin() != originAtMap, "cable view: a click in the map moves the view there");
        }
        h.view.fitContent (1.0f, 0.3f);
        expect (h.view.mapBounds().isEmpty(), "cable view: no map while the whole patch is in view");
    }

    // Reordering by dropping a box on a cable (ilana: "reorder everything
    // freely"): DRIVE -> EQ, and CHORUS (unwired) dropped on that cable goes
    // between them; then DRIVE dropped on CHORUS -> EQ moves behind CHORUS,
    // the chain it left joined up. Over empty space a box only moves.
    {
        PatchGraph::Graph graph;
        const auto drive = graph.addNode (2, 1, 1, 160.0f, 40.0f, "DRIVE", 1);
        const auto eq = graph.addNode (12, 1, 1, 560.0f, 40.0f, "EQ", 2);
        const auto chorus = graph.addNode (7, 1, 1, 360.0f, 200.0f, "CHORUS", 3);
        graph.moveNode (inputNodeId, 0.0f, 40.0f);
        graph.moveNode (outputNodeId, 760.0f, 40.0f);
        graph.connect ({ inputNodeId, 0 }, { drive, 0 });
        graph.connect ({ drive, 0 }, { eq, 0 }, 0.5f);
        graph.connect ({ eq, 0 }, { outputNodeId, 0 });
        Harness h (graph);
        h.view.setSize (900, 320);
        h.view.setMotion (false);
        h.view.setZoom (1.0f);
        h.view.centreContent();
        const auto onCable = [&h] (PatchGraph::NodeId from, PatchGraph::NodeId to)
        {
            return (h.view.portPosition ({ from, 0 }, false) + h.view.portPosition ({ to, 0 }, true)) * 0.5f;
        };
        dragFromTo (h.view, h.view.nodeBounds (*h.graph.find (chorus)).getCentre(), onCable (drive, eq));
        const auto first = h.graph.findCable ({ drive, 0 }, { chorus, 0 });
        expect (first >= 0 && h.graph.cables()[(size_t) first].gain == 0.5f && h.graph.findCable ({ chorus, 0 }, { eq, 0 }) >= 0
                    && h.graph.findCable ({ drive, 0 }, { eq, 0 }) < 0 && h.cableEdits == 1 && h.layoutEdits == 0,
                "cable view: a box dropped on a cable goes into it (keeping the cable's level)");

        h.graph.moveNode (chorus, 360.0f, 40.0f);
        h.graph.moveNode (drive, 360.0f, 200.0f);
        h.view.graphChanged();
        dragFromTo (h.view, h.view.nodeBounds (*h.graph.find (drive)).getCentre(), onCable (chorus, eq));
        expect (h.graph.findCable ({ inputNodeId, 0 }, { chorus, 0 }) >= 0 && h.graph.findCable ({ chorus, 0 }, { drive, 0 }) >= 0
                    && h.graph.findCable ({ drive, 0 }, { eq, 0 }) >= 0 && h.graph.cables().size() == 4 && compile (h.graph).ok,
                "cable view: dropping a wired box on another cable moves it there and joins up where it was");

        const auto cables = h.graph.cables().size();
        dragFromTo (h.view, h.view.nodeBounds (*h.graph.find (drive)).getCentre(), { 760.0f, 290.0f });
        expect (h.graph.cables().size() == cables && h.layoutEdits == 1, "cable view: a box dropped on empty space only moves");
    }
}

// ILANA_SNAPSHOT_GRAPH: pictures of the cable view, at twice the size:
// graph-demo.png (a chain, a parallel branch at 70 % and a loop),
// graph-series.png and graph-parallel.png (a factory rack migrated both
// ways), graph-dragging.png (a cable on its way to an input) and, for the
// cable motion of step 47, motion-1-rest.png (the hanging cables), then
// motion-2..4 (a box thrown up 70 px: just after, a swing later, and
// settled again).
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

    {
        Harness h (demoGraph());
        h.view.setSize (860, 240);
        h.view.centreContent();
        write (h.view, "motion-1-rest.png");

        const auto grab = h.view.nodeBounds (*h.graph.find (3)).getCentre();
        h.view.mouseDown (mouse (h.view, grab, true));
        for (int step = 1; step <= 14; ++step)
        {
            h.view.mouseDrag (mouse (h.view, grab + juce::Point<float> (0.0f, -5.0f * (float) step), true));
            h.view.advanceMotion (1.0f / 60.0f);
        }
        write (h.view, "motion-2-dragged.png");
        h.view.mouseUp (mouse (h.view, grab + juce::Point<float> (0.0f, -70.0f), false));
        for (int frame = 0; frame < 12; ++frame)
            h.view.advanceMotion (1.0f / 60.0f);
        write (h.view, "motion-3-swinging.png");
        while (h.view.advanceMotion (1.0f / 60.0f)) {}
        write (h.view, "motion-4-settled.png");
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
