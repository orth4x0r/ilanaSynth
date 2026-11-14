// Step 18 (ilana, 2026-10-10): drop-down selectors that sometimes opened no
// list. Every visible select of every page is clicked the way a hand does
// (a press and, a few milliseconds later, a release), three times in a row:
// each click must call the select's list opener, and Return on the focused select must too.
// Included by Snapshot.cpp after its helpers; runs from runUiTests.
#pragma once

void runDropdownTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (300);

    const auto event = [] (juce::Component& component, bool up)
    {
        const auto position = component.getLocalBounds().getCentre().toFloat();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                                 up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1, false);
    };
    // (A real menu window can't open on a headless X server, so each select's
    // own list opener is swapped for a counter: what is checked is the shared
    // click handling, which decides whether the opener is called at all.)
    int tried = 0;
    juce::StringArray stuck;

    for (const char* page : { "MAIN", "VECTOR", "OSC", "FILTER", "ENV/LFO", "MATRIX", "FM", "ARP/SEQ", "FX" })
    {
        editor.showPage (page);
        settle (350);

        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);

        for (auto* control : combos)
        {
            auto& box = control->getComboBox();

            if (! visibleInTree (&box) || ! box.isEnabled() || box.getWidth() < 8 || box.getHeight() < 8)
                continue;

            auto* combo = dynamic_cast<ComboControl::PopupCombo*> (&box);

            if (combo == nullptr || combo->popupOverride == nullptr)
                continue;

            ++tried;
            int opened = 0;
            control->setPopupOverride ([&opened] { ++opened; });
            const auto name = juce::String (page) + ": " + (control->getLabelText().isNotEmpty() ? control->getLabelText() : box.getText());

            // Three quick clicks (a press, then a release a few ms later).
            for (int click = 0; click < 3; ++click)
            {
                const auto before = opened;
                box.mouseDown (event (box, false));
                settle (20);
                box.mouseUp (event (box, true));
                settle (250);

                if (opened != before + 1)
                {
                    stuck.add (name + " (click " + juce::String (click + 1) + ", opened " + juce::String (opened - before) + "x)");
                    break;
                }
            }

            // Return twice in a few ms (a key repeat) used to leave the base
            // class's "menu active" flag set (the second open was dropped by
            // the debounce), after which no click opened anything. The mouse
            // release's own request takes the same road in a real click, but
            // can't be sent from here (it hit-tests against a screen).
            box.grabKeyboardFocus();
            const auto before = opened;
            box.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
            settle (20);
            box.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
            settle (250);

            if (opened != before + 1)
                stuck.add (name + " (Return opened " + juce::String (opened - before) + "x)");

            box.mouseDown (event (box, false));
            settle (20);
            box.mouseUp (event (box, true));
            settle (250);

            if (opened != before + 2)
                stuck.add (name + " (click after Return)");
        }
    }

    // The GRANULAR / SAMPLE card's SOURCE box (2026-11-13, ilana: "granular
    // source drop down will sometimes fail to appear"): its menu is its own,
    // so the base class's "menu active" flag, once set by the first click,
    // was never cleared and every later click did nothing.
    {
        if (auto* mode = processor.apvts.getParameter ("osc1_mode"))
            mode->setValueNotifyingHost (mode->convertTo0to1 (3.0f)); // (granular)
        int boxes = 0;

        for (const char* page : { "OSC", "MAIN" })
        {
            editor.showPage (page);
            settle (350);

            std::vector<SampleNameBox*> names;
            findAll<SampleNameBox> (editor, names);

            for (auto* box : names)
            {
                if (! visibleInTree (box) || box->getWidth() < 8 || box->getHeight() < 8)
                    continue;

                ++boxes;
                int opened = 0;
                const auto keep = box->onPopup;
                box->onPopup = [&opened] { ++opened; };
                const auto name = juce::String (page) + ": SOURCE";
                const auto click = [&]
                {
                    const auto before = opened;
                    box->mouseDown (event (*box, false));
                    settle (20);
                    box->mouseUp (event (*box, true));
                    settle (250);
                    return opened == before + 1 && ! box->isPopupActive();
                };

                for (int n = 0; n < 4; ++n)
                    if (! click())
                    {
                        stuck.add (name + " (click " + juce::String (n + 1) + " opened " + juce::String (opened) + " so far)");
                        break;
                    }

                const auto before = opened;
                box->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
                settle (250);

                if (opened != before + 1 || ! click())
                    stuck.add (name + " (Return then click)");

                box->onPopup = keep;
            }
        }

        expect (boxes > 0, "dropdowns: found the granular SOURCE box (" + juce::String (boxes) + ")");
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
        settle (300);
    }

#if ILANA_VULKAN
    // Menus that closed as they opened only while Vulkan drew the editor
    // (2026-11-14, ilana: "vulkan off works fine"): game-overlay and capture
    // layers (Steam overlay, OBS) are kept out of the editor's instance, and
    // the host's environment is put back after.
    {
        const auto unset = juce::String ("<unset>");
        const auto read = [&unset] (const juce::String& name) { return juce::SystemStats::getEnvironmentVariable (name, unset); };
        juce::StringArray before, inside;

        for (const auto& [name, value] : ilana::vulkan::ScopedOverlayLayersOff::variables())
            before.add (read (name));
        {
            ilana::vulkan::ScopedOverlayLayersOff off;
            for (const auto& [name, value] : ilana::vulkan::ScopedOverlayLayersOff::variables())
                inside.add (read (name));
        }
        juce::StringArray after;
        for (const auto& [name, value] : ilana::vulkan::ScopedOverlayLayersOff::variables())
            after.add (read (name));

        expect (inside.joinIntoString ("|").contains ("steam_overlay") && inside.contains ("1"),
                "vulkan: overlay layers are switched off while the instance is made (" + inside.joinIntoString ("|") + ")");
        expect (before == after, "vulkan: the host's environment is put back (" + after.joinIntoString ("|") + ")");
    }
#endif

    expect (tried > 6, "dropdowns: found the selects with their own list (" + juce::String (tried) + ")");
    expect (stuck.isEmpty(), "dropdowns: every select opens its list on every click (" + juce::String (stuck.size()) + " failed: " + stuck.joinIntoString ("; ") + ")");
}
