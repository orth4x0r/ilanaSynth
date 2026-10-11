// Roadmap step 16: the FILTER tab's ROUTING page (gui/pages/OscRoutingPage.h)
// driven with the mouse: it shows the fixed chain as cables, a cable drag
// hands the routing to the cables (one undo step), a loop between the
// filters is refused, CHAIN gives the routing back, and the boxes sit on the
// grid without overlapping. Included by Snapshot.cpp after
// PatchCableUiTests.h; runs from runUiTests (ILANA_UITEST_ONLY=OR for just
// these). ILANA_SNAPSHOT_ROUTING=1 writes pictures of the page instead.
#pragma once

#include "dsp/VoiceRouting.h"

namespace OscRoutingUi
{
inline PatchCableView* findRoutingView (juce::Component& editor)
{
    std::vector<PatchCableView*> views;
    findAll<PatchCableView> (editor, views);
    for (auto* view : views)
        if (view->getName() == "OSC ROUTING")
            return view;
    return nullptr;
}

inline const PatchGraph::Node* nodeNamed (const PatchGraph::Graph& graph, const juce::String& label)
{
    for (const auto& node : graph.nodes())
        if (juce::String (node.label) == label)
            return &node;
    return nullptr;
}

inline juce::Point<float> onScreen (PatchCableView& view, PatchGraph::Port port, bool input)
{
    return view.portPosition (port, input) * view.getZoom();
}

inline bool hasCable (const PatchGraph::Graph& graph, const juce::String& from, const juce::String& to)
{
    const auto* a = nodeNamed (graph, from);
    const auto* b = nodeNamed (graph, to);
    return a != nullptr && b != nullptr && graph.findCable ({ a->id, 0 }, { b->id, 0 }) >= 0;
}
} // namespace OscRoutingUi

inline void runOscRoutingUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    using namespace OscRoutingUi;
    using namespace VoiceRouting;

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (300);
    editor.showPage ("ROUTING");
    settle (400);

    auto* view = findRoutingView (editor);
    expect (view != nullptr && visibleInTree (view) && ! view->getBounds().isEmpty(), "FILTER > ROUTING shows the cable view");
    if (view == nullptr)
        return;

    // The chain as cables: every box inside the view, on the 10 px grid, none
    // on another; no INPUT box (the oscillators are the inputs).
    {
        const auto& graph = view->getGraph();
        expect (! processor.areRouteCablesOn(), "Init plays the fixed chain");
        expect (hasCable (graph, "OSC 1", "FILTER 1") && hasCable (graph, "FILTER 1", "FILTER 2") && hasCable (graph, "FILTER 2", "OUT"),
                "the page shows Init's chain: OSC 1 -> FILTER 1 -> FILTER 2 -> OUT");
        expect (view->contentFits(), "every box fits the view");
        expect (view->nodeAt (view->nodeBounds (*graph.find (PatchGraph::inputNodeId)).getCentre()) == nullptr
                    || view->nodeAt (view->nodeBounds (*graph.find (PatchGraph::inputNodeId)).getCentre())->isInput() == false,
                "the INPUT box is hidden");
        juce::String overlapping, offGrid;
        for (const auto& a : graph.nodes())
        {
            if (a.isInput())
                continue;
            if (std::fmod (a.x, 10.0f) != 0.0f || std::fmod (a.y, 10.0f) != 0.0f)
                offGrid << a.label << " ";
            for (const auto& b : graph.nodes())
                if (a.id < b.id && ! b.isInput() && view->nodeBounds (a).expanded (4.0f).intersects (view->nodeBounds (b)))
                    overlapping << a.label << "/" << b.label << " ";
        }
        expect (overlapping.isEmpty(), "no box sits on another (" + overlapping + ")");
        expect (offGrid.isEmpty(), "every box is on the 10 px grid (" + offGrid + ")");
        auto sameColumn = true;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.kind < 10)
                sameColumn = sameColumn && node.x == 0.0f;
        expect (sameColumn, "the oscillators line up in one column");
    }

    // A cable from OSC 1 to OUT: the cables take over, OSC 1 now plays
    // through Filter 1 and straight out, and one undo puts it all back.
    {
        processor.apvts.copyState();
        processor.getUndoManager().clearUndoHistory();
        const auto& graph = view->getGraph();
        const auto* osc1 = nodeNamed (graph, "OSC 1");
        expect (osc1 != nullptr, "OSC 1 has a box");
        if (osc1 != nullptr)
        {
            PatchCableUi::dragFromTo (*view, onScreen (*view, { osc1->id, 0 }, false),
                                      onScreen (*view, { PatchGraph::outputNodeId, 0 }, true));
            settle (200);
            expect (processor.areRouteCablesOn(), "a cable edit switches the routing to CABLES");
            expect (processor.getRoutePatch().source[0] == (toF1 | toOut), "OSC 1 now goes to Filter 1 and OUT (mask "
                                                                                + juce::String (processor.getRoutePatch().source[0]) + ")");
            processor.apvts.copyState();
            processor.getUndoManager().undo();
            settle (200);
            expect (! processor.areRouteCablesOn() && processor.getRoutePatch() == defaultPatch(), "one undo puts the chain back");
        }
    }

    // A loop between the filters is refused: FILTER 2's output onto FILTER 1.
    {
        const auto& graph = view->getGraph();
        const auto* f1 = nodeNamed (graph, "FILTER 1");
        const auto* f2 = nodeNamed (graph, "FILTER 2");
        if (f1 != nullptr && f2 != nullptr)
        {
            const auto before = processor.getRoutePatch();
            const auto wasOn = processor.areRouteCablesOn();
            PatchCableUi::dragFromTo (*view, onScreen (*view, { f2->id, 0 }, false), onScreen (*view, { f1->id, 0 }, true));
            settle (200);
            expect (processor.getRoutePatch() == before && processor.areRouteCablesOn() == wasOn, "a cable closing a loop is refused");
        }
    }

    // WEST in front of the filters by cables, then CHAIN gives the routing back.
    {
        Patch westFirst = defaultPatch();
        westFirst.source.fill (toWest);
        westFirst.stage[west] = toF1;
        processor.performEdit ("test", [&] { processor.setRouteCables (westFirst, true); });
        settle (400);
        const auto& graph = view->getGraph();
        const auto* w = nodeNamed (graph, "WEST");
        const auto* f1 = nodeNamed (graph, "FILTER 1");
        expect (w != nullptr && f1 != nullptr && w->x < f1->x, "WEST cabled before Filter 1 sits left of it");
        expect (processor.apvts.getRawParameterValue ("west_on")->load() > 0.5f, "a cable into WEST switches it on");

        std::vector<juce::Button*> buttons;
        findAll<juce::Button> (editor, buttons);
        for (auto* button : buttons)
            if (button->getButtonText() == "CHAIN" && visibleInTree (button))
            {
                button->triggerClick();
                break;
            }
        settle (300);
        expect (! processor.areRouteCablesOn(), "CHAIN switches the cables off");
        expect (processor.getRoutePatch() == westFirst, "the cables are kept while CHAIN plays");
    }

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
}
