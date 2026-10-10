// UI review 8, package R1 (operator editors and names): checks for its
// fixes. Included by Snapshot.cpp after its helpers (findAll, findChild,
// settle, expect, visibleInTree); runOperatorReview8Tests runs from
// runUiTests.
#pragma once

void runOperatorReview8Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    using D = Mod::Destination;
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (500);
    };
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto read = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };
    const auto knobFor = [&editor] (const juce::String& id, const juce::String& label = {}) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0
                && (label.isEmpty() || knob->getLabelText() == label))
                return knob;
        return nullptr;
    };
    const auto textOf = [] (KnobControl* knob)
    {
        return knob != nullptr ? knob->getSlider().getTextFromValue (knob->getSlider().getValue()) : juce::String();
    };
    const auto area = [&editor] (juce::Component* c) { return c == nullptr ? juce::Rectangle<int>() : editor.getLocalArea (c, c->getLocalBounds()); };
    // (On OSC every oscillator is a card, so "on OSC 1" means inside OSC 1's card.)
    const auto shownButtons = [&editor] (const juce::String& text, bool inFirstCard = false)
    {
        std::vector<juce::TextButton*> buttons, shown;
        findAll<juce::TextButton> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getWidth() > 0 && button->getButtonText() == text)
            {
                if (inFirstCard && ! editor.getOscCardBounds (0).contains (editor.getLocalArea (button, button->getLocalBounds()).getCentre()))
                    continue;
                shown.push_back (button);
            }
        return shown;
    };
    const auto shownEditor = [&editor] () -> OperatorEnvEditor*
    {
        std::vector<OperatorEnvEditor*> editors;
        findAll<OperatorEnvEditor> (editor, editors);
        for (auto* candidate : editors)
            if (visibleInTree (candidate) && candidate->getWidth() > 0)
                return candidate;
        return nullptr;
    };
    const auto shownPicker = [&editor] () -> OscPicker*
    {
        std::vector<OscPicker*> pickers;
        findAll<OscPicker> (editor, pickers);
        for (auto* picker : pickers)
            if (visibleInTree (picker) && picker->getWidth() > 0)
                return picker;
        return nullptr;
    };
    const auto oscPillButtons = [&editor]
    {
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        auto count = 0;
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getButtonText().matchesWildcard ("OSC ?", true))
                ++count;
        return count;
    };
    // The visible knobs and menus inside an area: none overlapping, none
    // narrower than its label.
    const auto layoutProblems = [&editor] (juce::Rectangle<int> within)
    {
        juce::StringArray problems;
        std::vector<juce::Component*> found;
        std::vector<KnobControl*> knobs;
        std::vector<ComboControl*> combos;
        findAll<KnobControl> (editor, knobs);
        findAll<ComboControl> (editor, combos);
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getWidth() > 0)
                found.push_back (knob);
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getWidth() > 0)
                found.push_back (combo);
        const auto label = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label));
        std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> inside;
        for (auto* component : found)
        {
            const auto bounds = editor.getLocalArea (component, component->getLocalBounds());
            if (! within.contains (bounds.getCentre()))
                continue;
            if (! within.contains (bounds))
                problems.add ("cut " + bounds.toString());
            if (auto* knob = dynamic_cast<KnobControl*> (component))
                if (juce::GlyphArrangement::getStringWidth (label, knob->getLabelText()) > (float) bounds.getWidth() + 1.0f)
                    problems.add ("squeezed " + knob->getLabelText());
            inside.push_back ({ component, bounds });
        }
        for (size_t a = 0; a < inside.size(); ++a)
            for (size_t b = a + 1; b < inside.size(); ++b)
                if (inside[a].second.intersects (inside[b].second))
                    problems.add ("overlap " + inside[a].second.toString() + " / " + inside[b].second.toString());
        return problems;
    };

    loadNamed ("E.PIANO 1 (ROM1A)");
    // FM opens on the operator picked last; the checks below read OSC 1.
    editor.showPage ("FM");
    settle (300);
    if (auto* picker = shownPicker())
        picker->pick (0);
    settle (200);

    // I10-1: an operator shows one level, OUTPUT (dB), on PLAY, FM and OSC; the
    // oscillator's own level (VOICE LEVEL) is on no operator page (I12-1).
    {
        editor.showPage ("MAIN");
        settle (400);
        const auto playOutput = textOf (knobFor ("osc2_eg_out", "OUTPUT")); // (a modulator's level is an OUTPUT too, I15-1)
        const auto playLevel = knobFor ("osc2_level") != nullptr;
        editor.showPage ("FM");
        settle (400);
        const auto fmOutput = textOf (knobFor ("osc1_eg_out", "OUTPUT"));
        const auto fmLevel = knobFor ("osc1_level") != nullptr;
        editor.showPage ("OSC");
        settle (400);
        const auto oscOutput = textOf (knobFor ("osc1_eg_out", "OUTPUT"));
        const auto voiceLevel = textOf (knobFor ("osc1_level", "VOICE LEVEL"));
        expect (playOutput.endsWith ("dB") && fmOutput.endsWith ("dB") && oscOutput.endsWith ("dB") && voiceLevel.isEmpty()
                    && ! playLevel && ! fmLevel && knobFor ("osc1_level") == nullptr,
                "an operator shows OUTPUT (dB) alone on PLAY, FM and OSC, no VOICE LEVEL (I12-1: " + playOutput + " / " + fmOutput
                    + " / " + oscOutput + ", " + voiceLevel + ")");
    }

    // I12-6: the OSC drawer on an operator voice has no STRINGS or SOUNDBOARD
    // tab; I12-3: a modulator says what it modulates and its level is still OUTPUT (I15-1).
    {
        editor.showPage ("OSC");
        settle (400);
        // The approved OSC design has no tabs: the bottom strip is one card whose
        // STRINGS and SOUNDBOARD dim in place while off, on an operator voice too
        // (their keys-only tabs are gone, so the old "no such tab" check is now
        // "no tabs at all, and both groups dimmed, not drawn live").
        std::vector<StateTabs*> rows;
        findAll<StateTabs> (editor, rows);
        auto tabs = 0;
        for (auto* row : rows)
            tabs += visibleInTree (row) ? 1 : 0;
        auto* amount = knobFor ("sym_amount");
        auto* board = knobFor ("sb_mix");
        expect (tabs == 0 && knobFor ("noise_level") != nullptr && amount != nullptr && amount->getAlpha() < 0.99f
                    && board != nullptr && board->getAlpha() < 0.99f,
                "OSC: the strip has no tabs, and STRINGS and SOUNDBOARD sit dimmed on an operator voice (I12-6)");
        editor.showPage ("MAIN");
        settle (300);
        expect (knobFor ("osc2_eg_out", "OUTPUT") != nullptr && knobFor ("osc2_eg_out", "DEPTH") == nullptr && knobFor ("osc1_eg_out", "OUTPUT") != nullptr,
                "PLAY: a modulator's level knob reads OUTPUT, never DEPTH (I15-1)");
    }

    // I8-2, I9-7: the oscillator's LEVEL is "Level" in the matrix whether
    // or not it plays the Operator Env; OUTPUT is "OP ENV Output".
    {
        const auto separator = ModNames::separator();
        juce::ComboBox box;
        MatrixMenus::fillDestinations (box);
        box.setSelectedId ((int) D::Osc2Level + 1, juce::dontSendNotification);
        ModNames::nameOperatorTrims (box, processor);
        const auto onEnv = ModNames::destination ((int) D::Osc2Level, processor);
        const auto boxText = box.getText();
        loadNamed ("Neuro Wobble");
        ModNames::nameOperatorTrims (box, processor);
        const auto plain = ModNames::destination ((int) D::Osc2Level, processor);
        expect (onEnv == "OSC 2" + separator + "Level" && boxText == onEnv && plain == "OSC 2" + separator + "Level"
                    && box.getText() == plain,
                "the matrix names an operator's LEVEL and a plain oscillator's \"Level\" alike (I8-2, I9-7: " + onEnv + ", " + plain
                    + "; menu " + boxText + ", " + box.getText() + ")");
        loadNamed ("E.PIANO 1 (ROM1A)");
    }

    // I8-6, S8-4, V8-5: one editor, one knob order (the graph's, left to
    // right) on FM and MOD; OSC shows a picture that opens it.
    editor.showPage ("FM");
    settle (400);
    {
        auto* fm = shownEditor();
        auto ordered = fm != nullptr;
        auto lastX = -1;
        juce::String order;
        if (fm != nullptr)
            for (int i = 0; i < OperatorEnvEditor::numStageControls; ++i)
            {
                auto* knob = fm->getKnob (0, i);
                const auto x = knob != nullptr && visibleInTree (knob) ? area (knob).getCentreX() : -1;
                ordered = ordered && x > lastX && knob->getLabelText() == OperatorEnvEditor::labelAt (i);
                order << (knob != nullptr ? knob->getLabelText() : juce::String ("-")) << "@" << x << " ";
                lastX = x;
            }
        std::vector<OperatorEnvEditor*> editors;
        findAll<OperatorEnvEditor> (editor, editors);
        expect (ordered && ! editors.empty() && editors.size() <= 2,
                "FM's operator card edits the Operator Env with the one editor, ATTACK to END in the graph's order (I8-6, V8-5: "
                    + juce::String ((int) editors.size()) + " editors; " + order.trim() + ")");

        // I8-3: a flat stage (E.Piano 1's RELEASE, SUSTAIN and END both
        // silent) still reads, and moves with, its rate.
        if (fm != nullptr)
            if (auto* release = fm->getKnob (0, 6))
            {
                const auto before = release->getSlider().getValue();
                release->getSlider().setValue (20.0);
                const auto slow = textOf (release);
                release->getSlider().setValue (80.0);
                const auto fast = textOf (release);
                release->getSlider().setValue (before);
                expect (slow != fast && slow != "1 ms", "a flat Operator Env stage's knob still reads its time (I8-3: " + slow + " / " + fast + ")");
            }

        // I8-7, V8-8: the ruler under the frame, the handles inside it.
        if (fm != nullptr)
        {
            auto& graph = fm->getGraph();
            auto inside = true;
            for (const auto point : graph.getHandlePositions())
                inside = inside && graph.getFrameBounds().reduced (2.0f).contains (point);
            expect (inside && graph.getFrameBounds().getBottom() < (float) graph.getHeight() - 10.0f,
                    "the OP ENV graph's ruler sits under its frame, the handles inside it (I8-7, V8-8)");
        }

        // I8-8, S8-3, V8-21: a link named OP PITCH and OP LFO, beside
        // the picker, not a seventh pill; I8-10: the operators on the
        // one picker.
        const auto link = shownButtons (juce::String::fromUTF8 ("EDIT OP PITCH \xc2\xb7 OP LFO \xe2\x80\xba"));
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        auto oldName = false;
        for (auto* button : buttons)
            oldName = oldName || button->getButtonText().contains ("PITCH & LFO");
        auto* picker = shownPicker();
        expect (link.size() == 1 && ! oldName && picker != nullptr && picker->getOscillators().size() == 6
                    && ! area (link.front()).intersects (area (picker)) && oscPillButtons() == 0,
                "FM: the operators on the one oscillator picker, OP PITCH · OP LFO a link past it (I8-8, I8-10, V8-21)");

        // I8-27, V8-32: words for the curves, a percentage for AMP MOD.
        const auto curves = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_eg_lcurve"));
        const auto curveName = curves != nullptr ? curves->choices[0] : juce::String();
        expect (curves != nullptr && ! curveName.startsWithChar ('-') && ! curveName.startsWithChar ('+') && curveName.length() > 4
                    && describeValue ("osc1_eg_ams", 0.0f) == "0%",
                "KEYS & VELOCITY: curves in words, AMP MOD 0% (I8-27, V8-32: " + curveName + ")");

        // The card at 100 % and 75 %: no knob or menu overlapping or cut,
        // none squeezed under its label.
        for (const auto width : { 1060, 795 })
        {
            editor.setSize (width, width * 720 / 1060);
            settle (400);
            if (auto* shown = shownEditor())
            {
                const auto card = area (shown).expanded (8, 34);
                const auto problems = layoutProblems (card);
                expect (problems.isEmpty(), "FM's operator card at " + juce::String (width) + " px: nothing overlaps, cut or squeezed ("
                                                + problems.joinIntoString (", ") + ")");
                // The picker keeps every operator and leaves the title room.
                auto* shownPick = shownPicker();
                expect (shownPick != nullptr && shownPick->getWidth() >= shownPick->getShortWidth(),
                        "FM's picker fits every operator at " + juce::String (width) + " px");
            }
        }
        editor.setSize (1060, 720);
        settle (300);
    }

    // OSC: the picture opens FM; no table tools; SEMI and OUTPUT / LEVEL in
    // FM's order; the caption names OP ENV (I8-15, I8-30, S8-9, V8-16).
    editor.showPage ("OSC");
    settle (400);
    {
        std::vector<OperatorEnvDisplay*> graphs;
        findAll<OperatorEnvDisplay> (editor, graphs);
        OperatorEnvDisplay* graph = nullptr;
        for (auto* candidate : graphs)
            if (visibleInTree (candidate) && candidate->getPrefix() == "osc1")
                graph = candidate;
        auto* semi = knobFor ("osc1_semi");
        auto* fine = knobFor ("osc1_fine");
        auto* level = knobFor ("osc1_eg_out", "OUTPUT");
        // One level in the row (OUTPUT); the oscillator's own (VOICE LEVEL) is not drawn (review 12, I12-1).
        const auto ordered = semi != nullptr && fine != nullptr && level != nullptr && knobFor ("osc1_level") == nullptr
                             && area (semi).getCentreX() < area (fine).getCentreX() && area (fine).getCentreX() < area (level).getCentreX();
        expect (graph != nullptr && graph->isReadOnly() && ordered,
                "OSC: an operator's OP ENV is a picture of the one editor; SEMI, FINE, OUTPUT as on FM, no VOICE LEVEL (S8-4, V8-5, I12-1)");
        expect (shownButtons ("RESAMPLE", true).empty() && shownButtons (juce::String ("EDIT TABLE ") + juce::String::fromUTF8 ("\xe2\x80\xba"), true).empty()
                    && shownButtons ("LOAD...", true).empty() && shownButtons ("3D", true).empty() && shownButtons ("SPEC", true).empty() && knobFor ("osc1_unison") == nullptr,
                "OSC: an operator has no RESAMPLE, EDIT, LOAD .WAV, 3D / SPEC or UNISON 1 (I8-15, S8-9, V8-16)");
        juce::String role;
        std::vector<juce::Component*> all;
        findAll<juce::Component> (editor, all);
        for (auto* component : all)
            if (visibleInTree (component) && component->getProperties().contains ("caption"))
                role = component->getProperties()["caption"].toString();
        expect (role.contains ("plays its OP ENV") && ! role.contains ("FM page"), "OSC: the caption reads \"plays its OP ENV\" (I8-30: " + role + ")");

        if (graph != nullptr && graph->onOpen != nullptr)
        {
            graph->onOpen();
            settle (400);
            expect (editor.getCurrentPageId() == "FM", "OSC: clicking the OP ENV picture opens it on FM (S8-4)");
        }
    }

    // PLAY: an operator names itself where the MODE menu sits (S8-9).
    editor.showPage ("MAIN");
    settle (400);
    {
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        auto modeShown = false;
        const auto modeName = processor.apvts.getParameter ("osc2_mode")->getName (64);
        for (auto* combo : combos)
            modeShown = modeShown || (visibleInTree (combo) && combo->getComboBox().getTooltip().startsWith (modeName + "\n"));
        expect (! modeShown, "PLAY: a DX7 operator strip has no \"Wavetable\" MODE menu (S8-9, V8-16)");
    }

    // MOD: OP PITCH's TRANSPOSE is a knob beside SCALE SHIFT (S8-19); the
    // OP ENV card's badge is short (S8-10).
    editor.showPage ("ENV/LFO");
    settle (500);
    if (auto* modPage = editor.getCurrentPage())
        if (auto* envCards = findChild<EnvThumbBar> (*modPage))
        {
            envCards->onSelect (17);
            settle (300);
            if (auto* pitch = shownEditor(); pitch != nullptr && pitch->isPitch())
            {
                pitch->getTabs().setSelected (1, true);
                settle (300);
                auto* slider = findChild<juce::Slider> (pitch->getTranspose());
                const auto knob = slider != nullptr && slider->isRotary() && visibleInTree (slider);
                const auto besideShift = visibleInTree (&pitch->getScaleShift())
                                         && std::abs (area (&pitch->getTranspose()).getCentreY() - area (&pitch->getScaleShift()).getCentreY()) < 12;
                const auto before = read ("osc2_semi");
                if (slider != nullptr)
                    slider->setValue (slider->getValue() + 2.0);
                settle (100);
                const auto moved = std::abs (read ("osc2_semi") - before - 2.0f) < 0.01f;
                if (slider != nullptr)
                    slider->setValue (slider->getValue() - 2.0);
                settle (100);
                expect (knob && besideShift && moved, "OP PITCH: TRANSPOSE is a knob in semitones beside SCALE SHIFT, moving every SEMI (S8-19)");

                // I8-28: the pitch graph has the ruler too, its handles inside.
                auto& graph = pitch->getGraph();
                auto inside = graph.isPitch();
                for (const auto point : graph.getHandlePositions())
                    inside = inside && graph.getFrameBounds().contains (point);
                expect (inside && graph.getFrameBounds().getBottom() < (float) graph.getHeight() - 10.0f,
                        "the OP PITCH graph has the ruler under its frame (I8-28)");
                pitch->getTabs().setSelected (0, true);
            }
            envCards->onSelect (16);
            settle (300);
            auto* pool = shownEditor();
            expect (pool != nullptr && shownPicker() != nullptr && shownPicker()->getOscillators().size() == 6 && oscPillButtons() == 0,
                    "MOD: OP ENV picks its oscillator on the one picker (I8-10)");
            const auto badge = juce::String ("6 ops");
            const auto fits = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), badge) < 40.0f;
            expect (fits, "the OP ENV card's badge is short enough for the narrowest card (S8-10)");
            envCards->onSelect (0);
            settle (200);
        }

    // On a patch without operators the OP ENV card is not in the pool (R3,
    // I8-5: the Operator Env's parts only where used). I8-39: the editor
    // itself, opened there anyway, greys its knobs and offers USE ON OSC.
    loadNamed ("Neuro Wobble");
    editor.showPage ("ENV/LFO");
    settle (400);
    if (auto* modPage = editor.getCurrentPage())
        if (auto* envCards = findChild<EnvThumbBar> (*modPage))
            expect (! envCards->isCardInPool (16), "OP ENV has no MOD pool card on a patch without operators (I8-5)");
    {
        OperatorEnvEditor unusedEditor (processor, OperatorEnvEditor::Place::pool);
        unusedEditor.setBounds (0, 0, 600, 240);
        unusedEditor.setVisible (false);
        unusedEditor.setVisible (true);
        auto* attack = unusedEditor.getKnob (0, 0);
        expect (unusedEditor.getUseButton().isVisible() && unusedEditor.getUseButton().getButtonText().startsWith ("USE ON OSC")
                    && attack != nullptr && attack->getAlpha() < 0.9f,
                "OP ENV unused: its knobs greyed, a USE ON OSC button offered (I8-39: button "
                    + juce::String (unusedEditor.getUseButton().isVisible() ? "shown " : "hidden ")
                    + unusedEditor.getUseButton().getButtonText()
                    + (attack != nullptr ? ", alpha " + juce::String (attack->getAlpha(), 2) : juce::String ()) + ")");
    }

    // I8-10: PHYSICAL picks its oscillator on the same picker.
    loadNamed ("Init");
    editor.showPage ("PHYSICAL");
    settle (400);
    expect (shownPicker() != nullptr && oscPillButtons() == 0, "PHYSICAL picks its oscillator on the one picker (I8-10)");

    // I8-40: host names as the pages write them.
    expect (processor.apvts.getParameter ("osc1_eg_r1")->getName (64) == "OSC 1 OP ENV Attack"
                && processor.apvts.getParameter ("sub_level")->getName (64).startsWith ("OSC 3")
                && dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_excite"))->choices[5] == "Bright Hammer",
            "host names read OSC 1 and Bright Hammer (I8-40: " + processor.apvts.getParameter ("osc1_eg_r1")->getName (64) + ")");
    expect (ModNames::destination ((int) D::Osc1Level) == "OSC 1" + ModNames::separator() + "Level",
            "the saved destination names are unchanged by the host names");

    editor.showPage ("MAIN");
    settle (200);
    juce::ignoreUnused (setParam);
}
