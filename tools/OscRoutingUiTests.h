// Roadmap step 16: the voice's half of the PATCH tab (gui/pages/PatchPage.h)
// driven with the mouse: it shows the fixed chain as cables into VOICES and
// the rack after it, a cable drag hands the routing to the cables (one undo
// step), a loop between the filters and a cable across VOICES are refused,
// CHAIN gives the routing back, and the boxes sit on the grid without
// overlapping. Included by Snapshot.cpp after
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
        if (view->getName() == "PATCH")
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
    editor.showPage ("PATCH");
    settle (400);

    auto* view = findRoutingView (editor);
    expect (view != nullptr && visibleInTree (view) && ! view->getBounds().isEmpty(), "PATCH shows the cable view");
    if (view == nullptr)
        return;

    // The chain as cables into VOICES, then the rack: every box inside the
    // view, on the 10 px grid, none on another.
    {
        const auto& graph = view->getGraph();
        expect (! processor.areRouteCablesOn(), "Init plays the fixed chain");
        expect (hasCable (graph, "OSC 1", "FILTER 1") && hasCable (graph, "FILTER 1", "FILTER 2") && hasCable (graph, "FILTER 2", "VOICES"),
                "the page shows Init's chain: OSC 1 -> FILTER 1 -> FILTER 2 -> VOICES");
        expect (view->contentFits(), "every box fits the view");
        const auto* voices = graph.find (PatchGraph::inputNodeId);
        const auto* filter2 = nodeNamed (graph, "FILTER 2");
        expect (voices != nullptr && filter2 != nullptr && voices->x > filter2->x, "VOICES sits after the filters");
        auto effectsAfter = true;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.id < (1u << 20))
                effectsAfter = effectsAfter && voices != nullptr && node.x > voices->x;
        expect (effectsAfter, "the effects sit after VOICES");
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
            if (node.id >= (1u << 20) && node.kind - 1000 < 10)
                sameColumn = sameColumn && node.x == 0.0f;
        expect (sameColumn, "the oscillators line up in one column");
    }

    // A cable from OSC 1 to VOICES: the cables take over, OSC 1 now plays
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
                                      onScreen (*view, { PatchGraph::inputNodeId, 0 }, true));
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

    // A cable may not skip VOICES: an oscillator straight into OUT (after
    // the effects) is refused, and so is the voices' sum back into a filter.
    {
        const auto& graph = view->getGraph();
        const auto* osc1 = nodeNamed (graph, "OSC 1");
        const auto* f1 = nodeNamed (graph, "FILTER 1");
        if (osc1 != nullptr && f1 != nullptr)
        {
            const auto routeBefore = processor.getRoutePatch();
            const auto fxBefore = processor.getFxPatch();
            const auto cablesBefore = graph.cables().size();
            PatchCableUi::dragFromTo (*view, onScreen (*view, { osc1->id, 0 }, false), onScreen (*view, { PatchGraph::outputNodeId, 0 }, true));
            settle (200);
            PatchCableUi::dragFromTo (*view, onScreen (*view, { PatchGraph::inputNodeId, 0 }, false), onScreen (*view, { f1->id, 0 }, true));
            settle (200);
            expect (processor.getRoutePatch() == routeBefore && FxPatchData::samePatch (processor.getFxPatch(), fxBefore)
                        && view->getGraph().cables().size() == cablesBefore,
                    "a cable across VOICES is refused");
        }
    }

    // The effects' half: a cable from VOICES straight to OUT (a dry path
    // beside the rack) turns the FX PATCHER on, the voice's cables stay as
    // they were, and one undo puts the rack back.
    {
        processor.addFxEffect (9); // Delay
        settle (300);
        processor.getUndoManager().clearUndoHistory();
        const auto routeBefore = processor.getRoutePatch();
        const auto cablesWereOn = processor.areRouteCablesOn();
        expect (! processor.isFxPatcherOn(), "the rack plays in SERIES");
        PatchCableUi::dragFromTo (*view, onScreen (*view, { PatchGraph::inputNodeId, 0 }, false),
                                  onScreen (*view, { PatchGraph::outputNodeId, 0 }, true));
        settle (200);
        const auto fx = processor.getFxPatchGraph();
        expect (processor.isFxPatcherOn() && fx.findCable ({ PatchGraph::inputNodeId, 0 }, { PatchGraph::outputNodeId, 0 }) >= 0,
                "a cable from VOICES to OUT plays the FX PATCHER with it");
        expect (processor.getRoutePatch() == routeBefore && processor.areRouteCablesOn() == cablesWereOn, "the voice's cables are untouched");
        processor.apvts.copyState();
        processor.getUndoManager().undo();
        settle (200);
        expect (! processor.isFxPatcherOn(), "one undo puts the rack back");
    }

    // Mini windows: a double-click on an effect opens its window with its
    // controls; its x closes it.
    {
        processor.addFxEffect (9); // Delay
        settle (300);
        const auto& graph = view->getGraph();
        const PatchGraph::Node* delay = nullptr;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.id < (1u << 20) && node.kind == 9)
                delay = &node;
        expect (delay != nullptr, "the Delay has a box");
        if (delay != nullptr)
        {
            const auto centre = view->nodeBounds (*delay).getCentre() * view->getZoom();
            view->mouseDoubleClick (PatchCableUi::mouse (*view, centre, false));
            settle (200);
            std::vector<juce::Component*> windows;
            findAll<juce::Component> (editor, windows);
            juce::Component* window = nullptr;
            for (auto* component : windows)
                if (component->getName().startsWith ("FX WINDOW") && visibleInTree (component))
                    window = component;
            expect (window != nullptr, "a double-click opens the effect's window");
            if (window != nullptr)
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*window, knobs);
                auto timeKnob = false;
                for (auto* knob : knobs)
                    timeKnob = timeKnob || knob->getParameterId() == "fx_delay_time";
                expect (timeKnob, "the window has the Delay's TIME knob");
                expect (view->getBounds().contains (window->getBounds()), "the window sits inside the canvas");
                std::vector<juce::TextButton*> buttons;
                findAll<juce::TextButton> (*window, buttons);
                for (auto* button : buttons)
                    if (button->getTooltip() == "Close this window")
                        button->triggerClick();
                settle (300);
                auto open = false;
                windows.clear();
                findAll<juce::Component> (editor, windows);
                for (auto* component : windows)
                    open = open || (component->getName().startsWith ("FX WINDOW") && visibleInTree (component));
                expect (! open, "its x closes it");
            }
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

        // No modes (ilana 2026-10-12): no CHAIN / CABLES or SERIES /
        // PARALLEL / PATCHER on PATCH; RESET writes the standard routing
        // into the cables.
        std::vector<juce::Button*> buttons;
        findAll<juce::Button> (editor, buttons);
        juce::String modes;
        juce::Button* reset = nullptr;
        for (auto* button : buttons)
        {
            if (! visibleInTree (button) || button->findParentComponentOfClass<PatchCableView>() != nullptr)
                continue;
            const auto onPatch = view->getParentComponent() != nullptr && view->getParentComponent()->isParentOf (button);
            for (const auto* name : { "CHAIN", "CABLES", "SERIES", "PARALLEL", "PATCHER" })
                if (onPatch && button->getButtonText() == name)
                    modes << name << " ";
            if (onPatch && button->getButtonText() == "RESET")
                reset = button;
        }
        expect (modes.isEmpty(), "PATCH has no routing mode switches (" + modes + ")");
        expect (reset != nullptr, "PATCH has RESET");
        if (reset != nullptr)
            reset->triggerClick();
        settle (300);
        expect (processor.getRoutePatch() == processor.getChainAsCables(), "RESET puts the standard routing back as cables");
    }

    // While the rack plays, the page shows the rack as it is, not a graph
    // an earlier patcher session left behind.
    {
        processor.addFxEffect (2); // Drive
        settle (200);
        auto stale = processor.getFxPatchGraph();
        for (const auto& cable : std::vector<PatchGraph::Cable> (stale.cables()))
            stale.disconnect (cable.from, cable.to);
        stale.connect ({ PatchGraph::inputNodeId, 0 }, { PatchGraph::outputNodeId, 0 });
        processor.setFxPatchGraph (stale);
        processor.setFxPatcherOn (false);
        settle (400);
        const auto& graph = view->getGraph();
        auto effects = 0, fromVoices = 0;
        for (const auto& node : graph.nodes())
            if (! node.isFixed() && node.id < (1u << 20))
            {
                ++effects;
                fromVoices += graph.findCable ({ PatchGraph::inputNodeId, 0 }, { node.id, 0 }) >= 0 ? 1 : 0;
            }
        expect (effects > 0 && fromVoices > 0 && graph.findCable ({ PatchGraph::inputNodeId, 0 }, { PatchGraph::outputNodeId, 0 }) < 0,
                "with the patcher off the page shows the rack's own routing (" + juce::String (effects) + " effects, "
                    + juce::String (fromVoices) + " fed by VOICES, patcher " + (processor.isFxPatcherOn() ? "on" : "off") + ")");
    }

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
}
