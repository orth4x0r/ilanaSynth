// UI review 9, package T2 (workflow: voice, SEQ, matrix, browser, macros):
// checks for its fixes. Included by Snapshot.cpp after its helpers (findAll,
// findChild, settle, expect, visibleInTree); runReview9T2Tests runs from
// runUiTests.
#pragma once

void runReview9T2Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (300);
    };
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto readParam = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };
    const auto click = [] (juce::Component& target, juce::Point<float> position)
    {
        const auto event = [&] (bool down)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, &target, &target, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1,
                                     false);
        };
        target.mouseDown (event (true));
        target.mouseUp (event (false));
    };
    const auto cardTabsWith = [&editor] (const juce::String& name) -> CardTabs*
    {
        std::vector<CardTabs*> all;
        findAll<CardTabs> (editor, all);
        for (auto* tabs : all)
            if (tabs->getNames().contains (name) && visibleInTree (tabs))
                return tabs;
        return nullptr;
    };
    const auto buttonNamed = [&editor] (const juce::String& text) -> juce::TextButton*
    {
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getButtonText() == text)
                return button;
        return nullptr;
    };
    const auto inEditor = [&editor] (juce::Component* component)
    {
        return component != nullptr && component->getParentComponent() != nullptr
                   ? editor.getLocalArea (component->getParentComponent(), component->getBounds())
                   : juce::Rectangle<int>();
    };

    loadNamed ("Neuro Wobble");

    // S9-1, S9-2: the OSC page's tab for unison spread is UNISON; VOICE holds
    // voice mode, voices, bend range and glide as visible controls.
    {
        editor.showPage ("OSC");
        settle (300);
        StateTabs* shared = nullptr;
        auto voiceIndex = -1;
        std::vector<StateTabs*> rows;
        findAll<StateTabs> (editor, rows);
        for (auto* tabs : rows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
            {
                if (tabs->getItem (i).name == "SUB + NOISE")
                    shared = tabs;
            }

        for (auto* tabs : rows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
                if (tabs->getItem (i).name == "VOICE" && tabs == shared)
                    voiceIndex = i;

        // (Review 14, V14-9: SPREAD & DRIFT's controls moved into the VOICE tab, which fills the drawer.)
        auto spreadTab = false;
        if (shared != nullptr)
            for (int i = 0; i < shared->getNumItems(); ++i)
                spreadTab = spreadTab || shared->getItem (i).name == "SPREAD & DRIFT";
        expect (shared != nullptr && ! spreadTab && voiceIndex >= 0
                    && shared->getItem (voiceIndex).tooltip.containsIgnoreCase ("glide")
                    && shared->getItem (voiceIndex).tooltip.containsIgnoreCase ("spread"),
                "OSC's strip has a VOICE tab holding the voice settings and the spread and drift (S9-1, S9-2, S10-3, V14-9)");

        // S10-3, S10-4: UNISON is the card's alone; the global tabs sit after the
        // sub and noise, behind a group gap.
        if (shared != nullptr)
        {
            auto unisonTabs = 0;
            for (int i = 0; i < shared->getNumItems(); ++i)
                unisonTabs += shared->getItem (i).name == "UNISON" ? 1 : 0;
            expect (unisonTabs == 0 && shared->getItem (0).name == "VOICE" && shared->getItem (1).name == "SUB + NOISE"
                        && voiceIndex == 0 && shared->getItem (voiceIndex).state == "POLY",
                    "the strip's tabs: VOICE first (naming its mode), then SUB + NOISE, no UNISON tab (S10-3, S10-4, review 11 S11-1)");
        }

        if (shared != nullptr && voiceIndex >= 0 && shared->onSelect != nullptr)
        {
            shared->setSelected (voiceIndex);
            shared->onSelect (voiceIndex);
            settle (300);
            const auto knobFor = [&editor] (const char* id) -> KnobControl*
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0)
                        return knob;
                return nullptr;
            };
            ComboControl* modeCombo = nullptr;
            std::vector<ComboControl*> combos;
            findAll<ComboControl> (editor, combos);
            for (auto* combo : combos)
                if (visibleInTree (combo) && combo->getWidth() > 0 && combo->getTooltip().startsWith ("Voice Mode"))
                    modeCombo = combo;
            ToggleControl* legato = nullptr;
            std::vector<ToggleControl*> toggles;
            findAll<ToggleControl> (editor, toggles);
            for (auto* toggle : toggles)
                if (visibleInTree (toggle) && toggle->getWidth() > 0 && toggle->getButton().getTooltip().startsWith ("Glide Legato"))
                    legato = toggle;
            expect (modeCombo != nullptr && knobFor ("poly_voices") != nullptr && knobFor ("bend_range") != nullptr
                        && knobFor ("glide") != nullptr && legato != nullptr,
                    "the VOICE tab shows mode, voices, bend range, glide and legato glide without a menu");

            if (modeCombo != nullptr)
            {
                modeCombo->getComboBox().setSelectedItemIndex (1, juce::sendNotificationSync);
                settle (100);
                expect (juce::roundToInt (readParam ("voice_mode")) == 1, "the VOICE tab's mode switch sets MONO");
                modeCombo->getComboBox().setSelectedItemIndex (0, juce::sendNotificationSync);
            }

            // Back on the first section for the other pages.
            shared->setSelected (0);
            shared->onSelect (0);
        }
    }

    // S10-1: the header's VOICES opens the OSC page on its VOICE tab (one voice
    // panel, no nested menu).
    {
        editor.showPage ("MAIN");
        settle (200);
        expect (editor.showVoicePanel != nullptr, "the header's VOICES has a destination");
        if (editor.showVoicePanel != nullptr)
            editor.showVoicePanel();
        settle (300);
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        KnobControl* voices = nullptr;
        for (auto* knob : knobs)
            if (knob->getParameterId() == "poly_voices" && visibleInTree (knob) && knob->getWidth() > 0)
                voices = knob;
        expect (editor.getCurrentPageId() == "OSC" && voices != nullptr,
                "VOICES jumps to OSC > VOICE, whose controls are the voice settings (S10-1)");
        editor.showPage ("MAIN");
    }

    // S10-2: with STRUM and SNAP TO KEY off, their menus read the choice the
    // switch brings back, never "Off" (the switch is the only off).
    {
        editor.showPage ("ARP/SEQ");
        settle (300);
        setParam ("spray_strum", 0.0f);
        setParam ("gen_scale", 0.0f);
        settle (400);
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        auto checked = 0, offs = 0;
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getWidth() > 0
                && (combo->getTooltip().startsWith ("Strum") || combo->getTooltip().startsWith ("Snap To Key")))
            {
                ++checked;
                offs += combo->getComboBox().getText() == "Off" ? 1 : 0;
            }
        expect (checked == 2 && offs == 0, "STRUM and SNAP TO KEY menus don't read Off beside their switches (" + juce::String (checked) + " found, "
                                               + juce::String (offs) + " Off) (S10-2)");
        editor.showPage ("MAIN");
    }

    // S10-15: characters a file name can't hold are dropped as they are typed.
    {
        SavePresetOverlay overlay (processor);
        overlay.setSize (900, 700);
        auto& field = overlay.getNameField();
        field.clear();
        field.insertTextAtCaret ("A:B/C");
        expect (field.getText() == "ABC", "the save dialog's name drops ':' and '/' as they are typed (" + field.getText() + ") (S10-15)");
    }

    // V9-27: Ctrl+Shift+1-3 pick the page inside a tab (S9-21).
    {
        editor.showPage ("MAIN");
        settle (200);
        const auto shortcut = [&editor] (int digit)
        {
            return editor.keyPressed (juce::KeyPress ((int) '0' + digit, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0));
        };
        const auto ok2 = shortcut (2);
        settle (200);
        const auto vector = editor.getCurrentPageId();
        const auto ok1 = shortcut (1);
        settle (200);
        expect (ok2 && ok1 && vector == "VECTOR" && editor.getCurrentPageId() == "MAIN",
                "Ctrl+Shift+2 / 1 pick PLAY's VECTOR and OVERVIEW pages (" + vector + ")");
    }

    // S9-3: drawing in an engine that is off switches it on, in one undo step.
    {
        editor.showPage ("ARP/SEQ");
        settle (300);
        setParam ("arp_on", 0.0f);
        setParam ("pseq_on", 0.0f);
        setParam ("euc_on", 0.0f);

        if (auto* tabs = cardTabsWith ("PROB SEQ"))
        {
            tabs->setSelected (0, true);
            settle (200);
            std::vector<ArpLanesEditor*> lanes;
            findAll<ArpLanesEditor> (editor, lanes);
            ArpLanesEditor* arpLanes = nullptr;
            for (auto* lane : lanes)
                if (visibleInTree (lane) && lane->getWidth() > 0)
                    arpLanes = lane;

            if (arpLanes != nullptr)
            {
                expect (arpLanes->getTooltip().contains ("STEP LEN") && ! arpLanes->getTooltip().contains (juce::String (juce::CharPointer_UTF8 ("GATE \xc3\x97"))),
                        "the arp's step-length lane is STEP LEN, not a second GATE (I9-13)");
                processor.getUndoManager().clearUndoHistory();
                click (*arpLanes, arpLanes->cellCentre (ArpLanesEditor::velocity, 2));
                settle (100);
                expect (readParam ("arp_on") > 0.5f, "drawing in the arp's lanes while it is off switches the ARP on (S9-3)");
            }
            else
                expect (false, "the ARP tab shows its lanes");

            tabs->setSelected (1, true);
            settle (200);
            std::vector<EuclidDisplay*> rings;
            findAll<EuclidDisplay> (editor, rings);
            for (auto* ring : rings)
                if (visibleInTree (ring) && ring->getWidth() > 0)
                {
                    click (*ring, ring->getLocalBounds().getCentre().toFloat());
                    settle (100);
                    expect (readParam ("euc_on") > 0.5f, "dragging the Euclid ring while it is off switches EUCLID on");
                }

            tabs->setSelected (2, true);
            settle (200);
            std::vector<ProbSeqEditor*> editors;
            findAll<ProbSeqEditor> (editor, editors);
            for (auto* seq : editors)
                if (visibleInTree (seq) && seq->getWidth() > 0)
                {
                    click (*seq, { (float) seq->getWidth() * 0.5f, (float) seq->getHeight() * 0.3f });
                    settle (100);
                    expect (readParam ("pseq_on") > 0.5f, "drawing in PROB SEQ while it is off switches it on");

                    // S9-15: its three controls sit centred, not packed left.
                    std::vector<juce::Component*> mine;
                    for (const auto* id : { "pseq_div", "pseq_length", "pseq_gate" })
                    {
                        std::vector<KnobControl*> knobs;
                        findAll<KnobControl> (editor, knobs);
                        std::vector<ComboControl*> boxes;
                        findAll<ComboControl> (editor, boxes);
                        for (auto* knob : knobs)
                            if (knob->getParameterId() == id && visibleInTree (knob))
                                mine.push_back (knob);
                        for (auto* box : boxes)
                            if (visibleInTree (box) && box->getTooltip().startsWith ("Prob Seq Rate") && juce::String (id) == "pseq_div")
                                mine.push_back (box);
                    }
                    juce::Rectangle<int> together;
                    for (auto* c : mine)
                        together = together.isEmpty() ? inEditor (c) : together.getUnion (inEditor (c));
                    const auto page = editor.getLocalArea (seq->getParentComponent(), seq->getBounds());
                    expect (mine.size() == 3 && ! together.intersects (page),
                            "PROB SEQ's three controls sit left-aligned under its steps (" + juce::String (together.getX()) + " vs "
                                + juce::String (page.getX()) + ", S9-15, S10-9)");
                }

            tabs->setSelected (0, true);
        }
        else
            expect (false, "SEQ has its engine tabs");
    }

    // I9-21: every note-path caption is short enough not to truncate.
    {
        NoteChainView chain (processor, IlanaTheme::accent());
        chain.setSize (700, 22);
        auto longest = 0;
        juce::String longestNote;

        for (int arp = 0; arp < 2; ++arp)
            for (int seq = 0; seq < 2; ++seq)
                for (int euclid = 0; euclid < 2; ++euclid)
                    for (int clip = 0; clip < 2; ++clip)
                        for (int target = 0; target < 3; ++target)
                            for (int host = 0; host < 2; ++host)
                                for (int shown = 0; shown < 4; ++shown)
                                {
                                    setParam ("arp_on", (float) arp);
                                    setParam ("pseq_on", (float) seq);
                                    setParam ("euc_on", (float) euclid);
                                    setParam ("clip_on", (float) clip);
                                    setParam ("euc_target", (float) target);
                                    setParam ("clip_mode", (float) host);
                                    chain.setShownEngine (shown);
                                    juce::String note;
                                    chain.stages (note);
                                    if (note.length() > longest)
                                        longest = note.length(), longestNote = note;
                                }

        expect (longest <= 38 && longest > 0, "the SEQ chain's captions stay short enough for 75 % ('" + longestNote + "', " + juce::String (longest) + " chars; I9-21)");
        for (const auto* id : { "arp_on", "pseq_on", "euc_on", "clip_on", "euc_target", "clip_mode" })
            setParam (id, 0.0f);
    }

    // S9-10: a clip leaves the synth as a .mid file and reads back the same.
    {
        Clip clip;
        clip.bars = 3;
        clip.notes = { { 0.0f, 1.0f, 60, 100 }, { 1.5f, 0.5f, 64, 80 }, { 4.0f, 2.0f, 67, 127 }, { 9.0f, 3.0f, 72, 20 } };
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-clip-export-test.mid");
        juce::String error;
        Clip back;
        const auto written = ClipState::exportMidi (clip, file, error);
        const auto read = written && ClipState::importMidi (file, back, error);
        auto same = read && back.bars == clip.bars && back.notes.size() == clip.notes.size();
        for (size_t i = 0; same && i < clip.notes.size(); ++i)
        {
            const auto& a = clip.notes[i];
            const auto& b = back.notes[i];
            same = a.note == b.note && a.velocity == b.velocity && std::abs (a.start - b.start) < 0.01f && std::abs (a.length - b.length) < 0.02f;
        }
        file.deleteFile();
        expect (same, "EXPORT MIDI writes a clip that IMPORT MIDI reads back unchanged (" + error + ")");

        if (auto* tabs = cardTabsWith ("CLIP"))
        {
            editor.showPage ("ARP/SEQ");
            tabs->setSelected (3, true);
            settle (300);
            auto* exportButton = buttonNamed ("EXPORT MIDI");
            auto* importButton = buttonNamed ("IMPORT MIDI");
            expect (exportButton != nullptr && importButton != nullptr && ! inEditor (exportButton).intersects (inEditor (importButton))
                        && inEditor (exportButton).getWidth() >= 70,
                    "the clip row has an EXPORT MIDI button beside IMPORT MIDI");
            tabs->setSelected (0, true);
        }
    }

    // S9-11: the clip's VEL lane has its scale, as the arp's does.
    {
        if (auto* tabs = cardTabsWith ("CLIP"))
        {
            tabs->setSelected (3, true);
            settle (300);
            std::vector<ClipEditor*> clips;
            findAll<ClipEditor> (editor, clips);
            auto checked = false;
            for (auto* roll : clips)
                if (visibleInTree (roll) && roll->getWidth() > 0)
                {
                    checked = true;
                    expect (roll->yForVelocity (1) - roll->yForVelocity (127) >= 20.0f, "the clip's VEL lane is tall enough for its scale (127 / 64 / 1)");
                }
            expect (checked, "the clip roll is shown on the CLIP tab");
            tabs->setSelected (0, true);
        }
    }

    // I9-8, I9-12, I9-20: macros are named one way, say what is wrong in
    // words, and + ASSIGN is a button.
    {
        loadNamed ("Neuro Wobble");
        const auto name = ModNames::source ((int) Mod::Source::Macro1, &processor);
        const auto saved = processor.getMacroName (1);
        processor.setMacroName (1, processor.getMacroName (0));
        const auto twin = ModNames::source ((int) Mod::Source::Macro2, &processor);
        processor.setMacroName (1, saved);
        expect (name == processor.getMacroName (0).toUpperCase() && ! name.contains ("(M") && twin.endsWith ("(M2)"),
                "a macro is TONE everywhere; the number follows only when two share a name ('" + name + "', '" + twin + "')");

        std::vector<StripKnob*> strip;
        findAll<StripKnob> (editor, strip);
        StripKnob* assign = nullptr;
        for (auto* knob : strip)
            if (knob->getMacroIndex() >= 4)
            {
                knob->refreshTargets();
                if (! knob->isAssigned() && assign == nullptr)
                    assign = knob;
            }

        if (assign != nullptr)
        {
            assign->createComponentSnapshot (assign->getLocalBounds()); // paints it, which places the button
            const auto bounds = assign->getAssignBounds();
            const auto macro = assign->getMacroIndex();
            expect (! bounds.isEmpty() && bounds.getWidth() >= 36 && bounds.getHeight() >= 14,
                    "an unassigned macro's + ASSIGN is a button-sized target (" + bounds.toString() + ")");
            auto usedBefore = 0;
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                usedBefore += processor.readModSlot (slot).source == Mod::macroSourceFor (macro) ? 1 : 0;
            click (*assign, bounds.getCentre().toFloat());
            settle (400);
            auto usedAfter = 0;
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                usedAfter += processor.readModSlot (slot).source == Mod::macroSourceFor (macro) ? 1 : 0;
            expect (usedAfter == usedBefore + 1 && editor.getCurrentPageId() == "MATRIX",
                    "clicking + ASSIGN adds a routing from that macro and opens the matrix");
            // Undo puts the matrix back.
            processor.getUndoManager().undo();
        }
        else
            expect (false, "Neuro Wobble has an unassigned macro 5-8");

        // The warning's words: the title says what is wrong.
        loadNamed ("Neuro Wobble");
        setParam ("fx_reverb_on", 0.0f);
        processor.setModSlotValue (40, "src", (float) Mod::Source::Macro5);
        processor.setModSlotValue (40, "dst", (float) Mod::Destination::FxReverbMix);
        processor.setModSlotValue (40, "amt", 0.5f);
        settle (500);
        strip.clear();
        findAll<StripKnob> (editor, strip);
        for (auto* knob : strip)
            if (knob->getMacroIndex() == 4)
            {
                knob->refreshTargets();
                const auto tip = knob->getTooltip();
                expect (knob->getNumIdleTargets() == 1 && tip.upToFirstOccurrenceOf ("\n", false, false).contains ("no effect now")
                            && tip.containsIgnoreCase ("reverb"),
                        "a macro whose only target is off says so in words: '" + tip.upToFirstOccurrenceOf ("\n", false, false) + "' (I9-12)");
            }
        processor.clearModSlot (40);
    }

    // S9-6, S9-7: matrix rows stay compact and no empty REMAP placeholder
    // takes the list's room.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MATRIX");
        settle (400);
        std::vector<MatrixRow*> rows;
        findAll<MatrixRow> (editor, rows);
        auto tallest = 0, shown = 0;
        juce::Rectangle<int> lastRow;
        for (auto* row : rows)
            if (visibleInTree (row) && row->getHeight() > 0)
            {
                tallest = juce::jmax (tallest, row->getHeight());
                ++shown;
                lastRow = inEditor (row);
            }
        std::vector<juce::Viewport*> viewports;
        findAll<juce::Viewport> (editor, viewports);
        auto viewportBottom = 0;
        for (auto* viewport : viewports)
            if (visibleInTree (viewport) && viewport->getHeight() > 100 && viewport->getWidth() > 600)
                viewportBottom = juce::jmax (viewportBottom, editor.getLocalArea (viewport->getParentComponent(), viewport->getBounds()).getBottom());
        expect (shown >= 8 && tallest <= 34, "matrix rows are at most 34 px tall (" + juce::String (tallest) + " px, " + juce::String (shown) + " rows; S9-7)");
        expect (viewportBottom >= editor.getHeight() - 140,
                "the matrix list takes the page down to the macro strip, with no REMAP placeholder under it (bottom " + juce::String (viewportBottom) + "; S9-6)");
    }

    loadNamed ("Neuro Wobble");
}
