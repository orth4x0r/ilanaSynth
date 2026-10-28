// The RC 1.0 workflow fixes (ilana 2026-10-28, "go on all of em"): the
// header arrows walk the browser's list, a dropped preset file loads, your
// own presets rename in the browser. Included by Snapshot.cpp; runs from
// runUiTests (ILANA_UITEST_ONLY=WF for just these). Preset files go to a
// temporary folder, never the user's.
#pragma once

inline void runWorkflowUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("ilanaWorkflowTest-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    tempDir.createDirectory();
    IlanaSynthAudioProcessor::userPresetDirectoryOverride = tempDir;
    const auto cutoff = [&processor] { return processor.apvts.getRawParameterValue ("f1_cutoff")->load(); };
    const auto setCutoff = [&processor] (float hz)
    {
        if (auto* parameter = processor.apvts.getParameter ("f1_cutoff"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (hz));
    };

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    editor.showPage ("MAIN");
    settle (300);

    // 1. The arrows step through the list the browser shows.
    {
        editor.stepPreset (1); // makes the (closed) browser
        settle (200);
        auto* panel = findChild<PresetPanel> (editor);
        expect (panel != nullptr, "the arrows make the browser to walk its list");

        if (panel != nullptr)
        {
            panel->setSortMode (PresetPanel::sortByName);
            panel->selectFilter ("");
            panel->setSearchText ("bass");
            settle (100); // (the search box tells the list asynchronously)
            const auto listed = panel->getListedNames();
            expect (listed.size() >= 3, "a search for bass lists a few presets (" + juce::String (listed.size()) + ")");

            if (listed.size() >= 3)
            {
                processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
                settle (200);
                editor.stepPreset (1);
                settle (200);
                const auto first = processor.getCurrentPresetName();
                editor.stepPreset (1);
                settle (200);
                const auto second = processor.getCurrentPresetName();
                editor.stepPreset (-1);
                editor.stepPreset (-1);
                settle (200);
                const auto wrapped = processor.getCurrentPresetName();
                expect (first == listed[0] && second == listed[1] && wrapped == listed[listed.size() - 1],
                        "the arrows walk the browser's list and wrap (" + first + ", " + second + ", " + wrapped + "; list "
                            + listed[0] + ", " + listed[1] + " ... " + listed[listed.size() - 1] + ")");
            }

            panel->setSearchText ("");
        }
    }

    // 2. A preset file dropped on the window loads.
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
        settle (100);
        setCutoff (777.0f);
        const auto file = tempDir.getChildFile ("Dropped One.ilanapreset");
        processor.savePresetToFile (file);
        setCutoff (3000.0f);
        expect (editor.isInterestedInFileDrag ({ file.getFullPathName() }), "the window takes a dropped .ilanapreset");
        editor.filesDropped ({ file.getFullPathName() }, 10, 10);
        settle (200);
        expect (std::abs (cutoff() - 777.0f) < 5.0f && processor.getCurrentPresetName() == "Dropped One",
                "a dropped preset file loads (" + processor.getCurrentPresetName() + ", " + juce::String (cutoff()) + " Hz)");
    }

    // 3. Rename or edit info rewrites your own preset's file.
    {
        auto& sheet = editor.getSaveOverlay();
        const auto own = tempDir.getChildFile ("Dropped One.ilanapreset");
        sheet.showForEdit (own);
        settle (100);
        expect (sheet.isShowing() && sheet.isEditing() && sheet.getNameField().getText() == "Dropped One",
                "Rename or edit info opens on the preset's own name");
        sheet.getNameField().setText ("Renamed One", true);
        sheet.getCommentField().setText ("New words.", false);
        sheet.save();
        settle (200);
        const auto renamed = tempDir.getChildFile ("Renamed One.ilanapreset");
        expect (! sheet.isShowing() && ! own.existsAsFile() && renamed.existsAsFile() && processor.getCurrentPresetName() == "Renamed One",
                "a new name renames the file, leaving no copy behind");
        const auto info = processor.getPresetInfo (processor.getAllPresetNames().indexOf ("Renamed One"));
        expect (info.comment == "New words.", "the edited comment is saved (" + info.comment + ")");

        // Its own name again: the same file, rewritten, nothing asked.
        sheet.showForEdit (renamed);
        sheet.getCommentField().setText ("Other words.", false);
        sheet.save();
        settle (200);
        // (Read from the file: the browser's info cache keys on the file's
        // time, which a rewrite in the same second leaves alone.)
        IlanaSynthAudioProcessor other;
        other.loadPresetFromFile (renamed);
        const struct { juce::String comment; } again { other.getPresetComment() };
        expect (! sheet.isShowing() && renamed.existsAsFile() && again.comment == "Other words.",
                "keeping the name rewrites the file in place (" + juce::String (sheet.isShowing() ? "still open: " + sheet.getNote() : "closed")
                    + ", " + again.comment + ")");
    }

    IlanaSynthAudioProcessor::userPresetDirectoryOverride = juce::File();
    tempDir.deleteRecursively();
    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    editor.showPage ("MAIN");
    settle (300);

    // 4. A double-click resets a knob (and opens no typing box); typing is
    // the right-click menu's "Type a value...".
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        KnobControl* knob = nullptr;
        for (auto* candidate : knobs)
            if (candidate->getParameterId() == "f1_cutoff" && visibleInTree (candidate) && candidate->isPacked())
                knob = candidate;
        expect (knob != nullptr, "PLAY's packed CUTOFF knob is there for the double-click check");

        if (knob != nullptr)
        {
            auto& slider = knob->getSlider();
            slider.setPopupDisplayEnabled (false, false, nullptr);
            setCutoff (500.0f);
            const auto centre = slider.getLocalBounds().getCentre().toFloat();
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), centre, juce::ModifierKeys::leftButtonModifier,
                                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &slider, &slider, now, centre, now, 2, false);
            slider.mouseDoubleClick (click);
            settle (100);
            std::vector<juce::TextEditor*> boxes;
            findAll<juce::TextEditor> (*knob, boxes);
            auto typing = false;
            for (auto* box : boxes)
                typing = typing || box->isVisible();
            auto* parameter = processor.apvts.getParameter ("f1_cutoff");
            expect (std::abs (cutoff() - parameter->convertFrom0to1 (parameter->getDefaultValue())) < 1.0f && ! typing,
                    "a double-click puts CUTOFF back to its default, with no typing box (" + juce::String (cutoff()) + " Hz)");

            juce::StringArray items;
            const auto menu = knob->buildModMenu();
            for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
                items.add (it.getItem().text);
            expect (items.contains (juce::String (juce::CharPointer_UTF8 ("Type a value\xe2\x80\xa6"))) && items.contains ("Reset to default  (double-click)"),
                    "the knob's menu offers Type a value... and names the double-click reset (" + items.joinIntoString (" | ") + ")");
        }
    }

    // 5. Every source chip has a right-click menu.
    {
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        auto all = ! chips.empty();
        for (auto* chip : chips)
            all = all && chip->onMenu != nullptr;
        expect (all, "every source chip opens a menu of its routes on right-click (" + juce::String ((int) chips.size()) + " chips)");
    }

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
}
