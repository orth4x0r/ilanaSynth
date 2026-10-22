// The PATCH tab's LFO boxes (src/gui/pages/PatchLfo.h) driven the way ilana
// asked for them (2026-10-12): "+ LFO" adds a box, a cable dragged from it
// onto an effect's box goes to that effect, a right-click on a knob in the
// effect's window assigns it, and the inspector takes the cable and the box
// away again. The audio cables are never touched. Included by Snapshot.cpp
// after OscRoutingUiTests.h; runs from runUiTests (ILANA_UITEST_ONLY=FL for
// just these). ILANA_SNAPSHOT_FXLFO=1 writes pictures instead.
#pragma once

namespace FxLfoUi
{
inline juce::Button* buttonNamed (juce::Component& root, const juce::String& text)
{
    std::vector<juce::Button*> buttons;
    findAll<juce::Button> (root, buttons);
    for (auto* button : buttons)
        if (visibleInTree (button) && button->getButtonText() == text)
            return button;
    return nullptr;
}

// A knob `id` in the floating window titled `window` ("FX WINDOW DELAY 2").
inline KnobControl* windowKnob (juce::Component& root, const juce::String& window, const juce::String& id)
{
    std::vector<KnobControl*> knobs;
    findAll<KnobControl> (root, knobs);
    for (auto* knob : knobs)
        if (knob->getParameterId() == id && visibleInTree (knob))
            for (auto* parent = knob->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                if (parent->getName().equalsIgnoreCase (window))
                    return knob;
    return nullptr;
}

// Runs the item named `text` in a menu (or its submenus); false if none.
inline bool pick (const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().text == text && it.getItem().action != nullptr)
        {
            it.getItem().action();
            return true;
        }
    return false;
}

inline bool hasItem (const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().text == text)
            return true;
    return false;
}
} // namespace FxLfoUi

inline void runFxLfoUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    using namespace FxLfoUi;
    constexpr int delay = 9;
    constexpr PatchGraph::NodeId lfoNode = (1u << 21) + 1u; // (PatchPage::lfoNodeId + 1)

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
    processor.addFxEffect (delay);
    processor.addFxEffect (delay);
    processor.setFxPatcherOn (true);
    editor.showPage ("PATCH");
    settle (400);

    auto* view = OscRoutingUi::findRoutingView (editor);
    expect (view != nullptr, "FX LFO: PATCH shows the cable view");
    if (view == nullptr)
        return;
    const auto audioCables = processor.getFxPatchGraph().cables().size();
    const auto boxOfSlot = [view] (int slot) -> const PatchGraph::Node*
    {
        auto seen = 0;
        for (const auto& node : view->getGraph().nodes())
            if (! node.isFixed() && node.id < (1u << 20) && node.kind == delay && seen++ == slot)
                return &node;
        return nullptr;
    };

    // Both copies' windows open (the one not in front works on a shadow of
    // its own settings; opening it read freed memory before 2026-10-12).
    for (int k : { 0, 1 })
        if (const auto* box = boxOfSlot (k))
        {
            view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (*box).getCentre() * view->getZoom(), false));
            settle (300);
        }
    expect (windowKnob (editor, "FX WINDOW DELAY 1", "fx_delay_time") != nullptr && windowKnob (editor, "FX WINDOW DELAY 2", "fx_delay_time") != nullptr,
            "FX LFO: both Delays' windows open");


    // "+ LFO" adds FX LFO 1 as a box on the canvas.
    auto* add = buttonNamed (editor, "+ LFO");
    expect (add != nullptr, "FX LFO: the toolbar has + LFO");
    if (add == nullptr)
        return;
    add->triggerClick();
    settle (200);
    expect (processor.getFxPatch().lfos.size() == 1 && view->getGraph().find (lfoNode) != nullptr, "FX LFO: + LFO adds a box");

    // A cable dragged from it onto Delay 2's box goes to Delay 2 (slot 1).
    const auto* second = boxOfSlot (1);
    expect (second != nullptr, "FX LFO: Delay 2 has a box");
    if (second == nullptr)
        return;
    const auto secondId = second->id;
    const auto from = view->portPosition ({ lfoNode, 0 }, false) * view->getZoom();
    const auto to = view->nodeBounds (*second).getCentre() * view->getZoom();
    PatchCableUi::dragFromTo (*view, from, to);
    settle (200);
    expect (processor.getFxPatch().hasLfoCable (1, 1) && processor.getFxPatch().lfoCables.size() == 1,
            "FX LFO: a cable dragged onto Delay 2's box goes to Delay 2");
    expect (processor.getFxPatchGraph().cables().size() == audioCables, "FX LFO: the cable is not an audio cable");

    // Right-click TIME in Delay 2's window: "Assign to FX LFO 1".
    if (const auto* box = view->getGraph().find (secondId))
        view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (*box).getCentre() * view->getZoom(), false));
    settle (300);
    auto* time = windowKnob (editor, "FX WINDOW DELAY 2", "fx_delay_time");
    expect (time != nullptr, "FX LFO: Delay 2's window opens with its TIME knob");
    if (time != nullptr)
    {
        expect (pick (time->buildModMenu(), "Assign to FX LFO 1"), "FX LFO: TIME's right-click menu offers Assign to FX LFO 1");
        settle (100);
        const auto routes = processor.getFxPatch().lfoRoutes;
        expect (routes.size() == 1 && routes[0].slot == 1 && routes[0].param == "fx_delay_time",
                "FX LFO: assigning moves Delay 2's TIME only");
        const auto menu = time->buildModMenu();
        expect (hasItem (menu, "Remove from FX LFO 1") && ! hasItem (menu, "Assign to FX LFO 1"),
                "FX LFO: an assigned knob's menu offers Remove (not Assign)");
    }

    // Delay 1 has no LFO cable: its knobs offer a new LFO, not FX LFO 1.
    if (const auto* first = boxOfSlot (0))
        view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (*first).getCentre() * view->getZoom(), false));
    settle (300);
    if (auto* other = windowKnob (editor, "FX WINDOW DELAY 1", "fx_delay_time"))
    {
        const auto menu = other->buildModMenu();
        expect (! hasItem (menu, "Assign to FX LFO 1") && hasItem (menu, "Assign to a new LFO"),
                "FX LFO: an effect with no LFO cable offers a new LFO, not another effect's");
    }
    else
        expect (false, "FX LFO: Delay 1's window opens");

    // The cable selected: REMOVE CABLE takes it and its knob away.
    view->selectCable (view->getGraph().findCable ({ lfoNode, 0 }, { secondId, 0 }));
    settle (200);
    if (auto* remove = buttonNamed (editor, "REMOVE CABLE"))
        remove->triggerClick();
    settle (200);
    expect (processor.getFxPatch().lfoCables.empty() && processor.getFxPatch().lfoRoutes.empty(),
            "FX LFO: REMOVE CABLE takes the cable and its knobs out");

    // The LFO's own window: SMOOTH and RETRIG are in it, and Drawn / Steps are
    // drawn on its picture (a drag sets the points).
    {
        const auto* lfoBox = view->getGraph().find (lfoNode);
        if (lfoBox != nullptr)
            view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (*lfoBox).getCentre() * view->getZoom(), false));
        settle (300);
        std::function<juce::Component* (juce::Component&)> window = [&] (juce::Component& parent) -> juce::Component*
        {
            for (auto* child : parent.getChildren())
            {
                if (child->isVisible() && child->getName().equalsIgnoreCase ("FX WINDOW FX LFO 1"))
                    return child;
                if (auto* found = window (*child))
                    return found;
            }
            return nullptr;
        };
        auto* lfoWindow = window (editor);
        expect (lfoWindow != nullptr, "FX LFO: double-clicking the box opens its window");
        if (lfoWindow != nullptr)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*lfoWindow, knobs);
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (*lfoWindow, buttons);
            const auto has = [&] (const juce::String& text)
            {
                for (auto* knob : knobs)
                    if (knob->getLabelText() == text && visibleInTree (knob))
                        return true;
                for (auto* control : buttons)
                    if (control->getButtonText() == text && visibleInTree (control))
                        return true;
                return false;
            };
            expect (has ("SMOOTH"), "FX LFO: the window has SMOOTH");
            expect (has ("RETRIG") || buttons.size() > 3, "FX LFO: the window has RETRIG");

            // Drawn: the shape on the picture, then a drag across it.
            auto settings = processor.getFxPatch().lfos[0].settings;
            settings.shape = 9;
            processor.setFxLfoSettings (1, settings);
            settle (300);
            juce::Component* picture = nullptr;
            for (auto* child : lfoWindow->getChildren())
                if (child->getHeight() == 124 && child->isVisible())
                    picture = child;
            expect (picture != nullptr, "FX LFO: the window has its picture");
            if (picture != nullptr)
            {
                const auto w = (float) picture->getWidth(), h = (float) picture->getHeight();
                const auto top = juce::Point<float> (w * 0.35f, h * 0.30f);
                picture->mouseDown (PatchCableUi::mouse (*picture, top, true));
                for (int k = 1; k <= 6; ++k)
                    picture->mouseDrag (PatchCableUi::mouse (*picture, top + juce::Point<float> ((float) k * 8.0f, 0.0f), true, false, top));
                picture->mouseUp (PatchCableUi::mouse (*picture, top + juce::Point<float> (48.0f, 0.0f), false));
                settle (200);
                const auto drawn = processor.getFxPatch().lfos[0].settings.drawn;
                const auto changed = std::count_if (drawn.begin(), drawn.end(), [] (float v) { return v > 0.35f; });
                expect (drawn != FxLfo::defaultDrawn() && changed >= 4, "FX LFO: dragging across a Drawn picture sets points (" + juce::String ((int) changed) + ")");
            }
        }
    }

    // The box selected: REMOVE takes it away.
    const auto centre = [view] { const auto* node = view->getGraph().find (lfoNode); return node != nullptr ? view->nodeBounds (*node).getCentre() * view->getZoom() : juce::Point<float>(); };
    view->mouseDown (PatchCableUi::mouse (*view, centre(), true));
    view->mouseUp (PatchCableUi::mouse (*view, centre(), false));
    settle (200);
    if (auto* remove = buttonNamed (editor, "REMOVE"))
        remove->triggerClick();
    settle (200);
    expect (processor.getFxPatch().lfos.empty() && view->getGraph().find (lfoNode) == nullptr, "FX LFO: REMOVE takes the box away");
    expect (processor.getFxPatchGraph().cables().size() == audioCables, "FX LFO: the audio cables are as they were");

    // A window dims as the RACK card does (ilana, 2026-10-22): SYNC on greys
    // TIME and TIME RIGHT, off greys DIV; the slot switched off greys all.
    {
        if (const auto* first = boxOfSlot (0))
            view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (*first).getCentre() * view->getZoom(), false));
        settle (300);
        const auto inWindow = [] (juce::Component* item)
        {
            for (auto* parent = item->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                if (parent->getName().equalsIgnoreCase ("FX WINDOW DELAY 1"))
                    return true;
            return false;
        };
        const auto alphaOf = [&editor, inWindow] (const juce::String& id)
        {
            std::vector<juce::Component*> all;
            findAll<juce::Component> (editor, all);
            for (auto* item : all)
                if (const auto* knob = dynamic_cast<KnobControl*> (item); ((knob != nullptr && knob->getParameterId() == id)
                         || (dynamic_cast<ComboControl*> (item) != nullptr && static_cast<ComboControl*> (item)->getParameterId() == id))
                     && visibleInTree (item) && inWindow (item))
                    return item->getAlpha();
            return -1.0f;
        };
        ToggleControl* sync = nullptr;
        std::vector<ToggleControl*> toggles;
        findAll<ToggleControl> (editor, toggles);
        for (auto* toggle : toggles)
            if (toggle->getParameterId() == "fx_delay_sync" && visibleInTree (toggle) && inWindow (toggle))
                sync = toggle;
        expect (sync != nullptr, "FX window: Delay 1's window has SYNC");
        if (sync != nullptr)
        {
            const auto setSync = [sync] (bool on)
            {
                if (sync->getButton().getToggleState() != on)
                    sync->getButton().triggerClick();
                settle (300);
            };
            setSync (true);
            expect (alphaOf ("fx_delay_time") == IlanaTheme::dimmedAlpha && alphaOf ("fx_delay_time_r") == IlanaTheme::dimmedAlpha
                        && alphaOf ("fx_delay_div") == 1.0f && alphaOf ("fx_delay_feedback") == 1.0f,
                    "FX window: SYNC on dims the Delay's TIME and TIME RIGHT, not DIV");
            setSync (false);
            expect (alphaOf ("fx_delay_time") == 1.0f && alphaOf ("fx_delay_div") == IlanaTheme::dimmedAlpha,
                    "FX window: SYNC off dims the Delay's DIV, not TIME");
        }
        const auto bypass = [&processor] (float value)
        {
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                if (auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_bypass"))
                    parameter->setValueNotifyingHost (value);
            settle (300);
        };
        bypass (1.0f);
        // (A component keeps its alpha in 8 bits.)
        const auto offAlpha = [] (float alpha) { return std::abs (alpha - FilterColours::offAlpha) < 0.01f; };
        expect (offAlpha (alphaOf ("fx_delay_time")) && offAlpha (alphaOf ("fx_delay_feedback")),
                "FX window: a switched-off slot dims all its controls");
        bypass (0.0f);

        // AW Delay's own SYNC (TapeDelay2: its first knob is the time).
        processor.addFxEffect (35);
        settle (200);
        const auto setValue = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        setValue ("fx_awdelay_algo", 0.0f);
        setValue ("fx_awdelay_sync", 1.0f);
        settle (300);
        for (const auto& node : view->getGraph().nodes())
            if (! node.isFixed() && node.kind == 35)
                view->mouseDoubleClick (PatchCableUi::mouse (*view, view->nodeBounds (node).getCentre() * view->getZoom(), false));
        settle (300);
        const auto awAlpha = [&editor] (const juce::String& id)
        {
            std::vector<juce::Component*> all;
            findAll<juce::Component> (editor, all);
            for (auto* item : all)
                if (const auto* knob = dynamic_cast<KnobControl*> (item); ((knob != nullptr && knob->getParameterId() == id)
                         || (dynamic_cast<ComboControl*> (item) != nullptr && static_cast<ComboControl*> (item)->getParameterId() == id))
                     && visibleInTree (item))
                    for (auto* parent = item->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
                        if (parent->getName().startsWith ("FX WINDOW"))
                            return item->getAlpha();
            return -1.0f;
        };
        expect (awAlpha ("fx_awdelay_p1") == IlanaTheme::dimmedAlpha && awAlpha ("fx_awdelay_div") == 1.0f,
                "FX window: AW Delay's SYNC dims its time knob, not DIV");
        setValue ("fx_awdelay_sync", 0.0f);
        settle (300);
        expect (awAlpha ("fx_awdelay_p1") == 1.0f && awAlpha ("fx_awdelay_div") == IlanaTheme::dimmedAlpha,
                "FX window: AW Delay's SYNC off dims DIV, not its time knob");
    }

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
}
