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

    // 6. Copy, Paste and Init on modules, each one undo step.
    {
        using K = IlanaSynthAudioProcessor::ModuleKind;
        const auto set = [&processor] (const juce::String& id, float plain)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
        };
        const auto get = [&processor] (const juce::String& id) { return processor.apvts.getRawParameterValue (id)->load(); };

        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
        settle (100);
        set ("lfo1_smooth", 0.4f);
        auto curve = processor.getLfoCurve (0);
        curve.points = { { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.3f }, { 1.0f, -1.0f, 0.0f } };
        processor.setLfoCurve (0, curve);
        const auto lfo = processor.copyModule (K::Lfo, 0);
        processor.performEdit ("Paste LFO 1 onto LFO 3", [&] { processor.pasteModule (lfo, 2); });
        expect (std::abs (get ("lfo3_smooth") - 0.4f) < 0.01f && processor.getLfoCurve (2).points.size() == 3,
                "pasting LFO 1 onto LFO 3 copies its knobs and its drawn curve");
        processor.getUndoManager().undo();
        expect (std::abs (get ("lfo3_smooth") - 0.4f) > 0.01f && processor.getLfoCurve (2).points.size() != 3, "one undo takes the paste back");

        set ("amp_attack", 1.2f);
        const auto amp = processor.copyModule (K::Envelope, 0);
        processor.performEdit ("Paste", [&] { processor.pasteModule (amp, 5); });
        expect (std::abs (get ("env6_attack") - 1.2f) < 0.01f, "the amp envelope pastes onto ENV 6 (" + juce::String (get ("env6_attack")) + " s)");

        set ("f1_cutoff", 640.0f);
        set ("f1_reso", 0.7f);
        processor.performEdit ("Paste", [&] { processor.pasteModule (processor.copyModule (K::Filter, 0), 1); });
        expect (std::abs (get ("f2_cutoff") - 640.0f) < 2.0f && std::abs (get ("f2_reso") - 0.7f) < 0.01f, "Filter 1 pastes onto Filter 2");

        set ("osc1_semi", 7.0f);
        processor.performEdit ("Paste", [&] { processor.pasteModule (processor.copyModule (K::Oscillator, 0), 1); });
        expect (std::abs (get ("osc2_semi") - 7.0f) < 0.01f, "OSC 1 pastes onto OSC 2 (" + juce::String (get ("osc2_semi")) + " st)");

        processor.performEdit ("Init", [&] { processor.initModule (K::Filter, 0); });
        auto* cutoffParameter = processor.apvts.getParameter ("f1_cutoff");
        expect (std::abs (get ("f1_cutoff") - cutoffParameter->convertFrom0to1 (cutoffParameter->getDefaultValue())) < 1.0f
                    && std::abs (get ("f2_cutoff") - 640.0f) < 2.0f,
                "Init Filter 1 resets Filter 1 only");
        processor.performEdit ("Init", [&] { processor.initModule (K::Lfo, 0); });
        expect (processor.getLfoCurve (0).points.size() == LfoCurve {}.points.size(), "Init LFO 1 gives its curve back");

    }

    // 7. The LFO paint brush and your own shapes.
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
        settle (100);
        if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
            shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));
        LfoDisplay display (processor, 0, juce::Colours::orange);
        display.setSize (400, 200);
        display.setGridDivisions (8);
        display.setBrush (LfoDisplay::brushStep);
        const auto plot = juce::Rectangle<float> (0.0f, 0.0f, 400.0f, 200.0f).reduced (10.0f, 36.0f);
        processor.beginEdit ("paint");
        display.paintAt ({ 400.0f * 2.5f / 8.0f, plot.getY() + 4.0f }); // cell 2, near the top
        processor.endEdit();
        const auto painted = processor.getLfoCurve (0);
        auto inCell = 0;
        auto high = true;
        for (const auto& point : painted.points)
            if (point.x >= 0.25f && point.x < 0.375f)
            {
                ++inCell;
                high = high && point.y > 0.5f;
            }
        expect (inCell == 2 && high, "the Step brush stamps a high step into the cell under the pointer (" + painted.toString() + ")");

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-wf-shape.ilanalfo");
        expect (display.saveShapeTo (file) && file.loadFileAsString() == painted.toString(), "Save shape writes the curve's points");
        processor.performEdit ("Init", [&] { processor.initModule (IlanaSynthAudioProcessor::ModuleKind::Lfo, 0); });
        display.loadShapeFrom (file);
        expect (processor.getLfoCurve (0).toString() == painted.toString(), "a saved shape loads back");
        file.deleteFile();
    }

    // 8. A .mid dropped on SEQ imports into the chosen clip; EXPORT MIDI drags.
    {
        editor.showPage ("ARP/SEQ");
        settle (300);
        Clip source;
        source.bars = 1;
        source.notes.push_back ({ 0.0f, 0.5f, 64, 100 });
        source.notes.push_back ({ 1.0f, 0.5f, 67, 100 });
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-wf-clip.mid");
        juce::String error;
        expect (ClipState::exportMidi (source, file, error), "a test clip writes as MIDI (" + error + ")");

        std::vector<juce::Component*> all;
        findAll<juce::Component> (editor, all);
        juce::FileDragAndDropTarget* target = nullptr;
        for (auto* component : all)
            if (auto* t = dynamic_cast<juce::FileDragAndDropTarget*> (component); t != nullptr && component != &editor && visibleInTree (component)
                                                                                   && t->isInterestedInFileDrag ({ file.getFullPathName() }))
                target = t;
        expect (target != nullptr, "the SEQ page takes a dropped .mid");

        if (target != nullptr)
        {
            const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) processor.apvts.getRawParameterValue ("clip_index")->load());
            target->filesDropped ({ file.getFullPathName() }, 10, 10);
            settle (100);
            const auto clip = processor.getClipState().getClip (index);
            expect (clip.notes.size() == 2 && clip.notes[1].note == 67, "the dropped file's notes land in the chosen clip ("
                                                                             + juce::String ((int) clip.notes.size()) + " notes)");
        }

        // EXPORT MIDI is the drag handle: a drag writes the clip to a file first.
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        auto dragHandle = false;
        for (auto* button : buttons)
            if (button->getButtonText() == "EXPORT MIDI" && button->getTooltip().contains ("Drag this"))
                dragHandle = true;
        expect (dragHandle, "EXPORT MIDI says it drags the clip into a track");
        file.deleteFile();
        editor.showPage ("MAIN");
        settle (200);
    }
}
