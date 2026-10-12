// UI review 6, P1 (modulation): the depth rings and badges on a knob, the
// eight-macro strip and its cards, and the matrix's names, Uni/Bi, idle rows
// and merging. Included from Snapshot.cpp after its helpers (findAll,
// findChild, settle, expect, visibleInTree).
#pragma once

void runReview7ModulationTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor);

void runModulationTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto set = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto routeSlot = [&set] (int slot, Mod::Source source, Mod::Destination destination, float depth)
    {
        const auto prefix = "mod" + juce::String (slot + 1);
        set (prefix + "_src", (float) source);
        set (prefix + "_dst", (float) destination);
        set (prefix + "_amt", depth);
    };
    const auto clearHistory = [&processor]
    {
        processor.apvts.copyState();
        processor.getUndoManager().clearUndoHistory();
    };
    const auto undoSteps = [&processor]
    {
        processor.apvts.copyState();
        return processor.getUndoManager().getUndoDescriptions();
    };
    const auto findKnob = [&editor] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 30)
                return knob;
        return nullptr;
    };
    const auto mouseAt = [] (juce::Component& component, juce::Point<float> at, juce::Point<float> downAt, int clicks = 1)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(), downAt,
                                 juce::Time::getCurrentTime(), clicks, at != downAt);
    };

    processor.loadFactoryPreset (0); // Init: an empty matrix
    editor.showPage ("FILTER");
    settle (300);

    // Rings: one per routing on the knob, drawn round the dial; dragging one
    // sets its depth as one undo step (V5-1, V6-2).
    {
        routeSlot (0, Mod::Source::Lfo1, Mod::Destination::Filter1Cutoff, 0.2f);
        routeSlot (1, Mod::Source::Lfo2, Mod::Destination::Filter1Cutoff, -0.4f);
        settle (300);
        auto* cutoff = findKnob ("f1_cutoff");
        if (cutoff != nullptr)
            cutoff->syncRoutings();
        expect (cutoff != nullptr && cutoff->getNumRoutings() == 2 && cutoff->getNumRings() == 2,
                "a knob with two routings draws two depth rings");

        // The rings clear the value arc and each other, stay inside the
        // knob, and a knob too narrow for badges beside them shows none
        // (the rings are the legend; only routings past three get a badge).
        if (cutoff != nullptr && cutoff->getNumRings() == 2)
        {
            const auto centre = cutoff->getDialCentre();
            const auto outer = cutoff->getRingRadius (1) + 1.0f;
            auto& strip = cutoff->getDotStrip();
            const auto stripRight = strip.isVisible() ? strip.getRight() : 0;
            expect (cutoff->getRingRadius (0) >= cutoff->getDialRadius() + 1.5f
                        && cutoff->getRingRadius (1) - cutoff->getRingRadius (0) >= 2.5f
                        && centre.x - outer >= 0.0f && centre.x + outer <= (float) cutoff->getWidth()
                        && (! strip.isVisible() || (strip.getX() >= (int) (centre.x + outer) && stripRight <= cutoff->getWidth())),
                    "the rings sit outside the dial (" + juce::String (cutoff->getDialRadius(), 1) + " px), "
                        + juce::String (cutoff->getRingRadius (1) - cutoff->getRingRadius (0), 1)
                        + " px apart, inside the knob, with the badges (if any) beside them");
        }

        if (cutoff != nullptr && cutoff->getNumRings() == 2)
        {
            highlightedModSource() = 0;
            pinnedModSource() = 0;
            auto& overlay = cutoff->getRingOverlay();
            const auto start = overlay.getLocalPoint (cutoff, cutoff->getRingPoint (0, 0.3f));
            const auto centre = overlay.getLocalPoint (cutoff, cutoff->getBounds().getCentre().toFloat() - cutoff->getPosition().toFloat());
            expect (overlay.isVisible() && overlay.hitTest ((int) start.x, (int) start.y)
                        && ! overlay.hitTest ((int) centre.x, (int) centre.y),
                    "a press on a ring is the ring's, a press on the dial is the knob's");

            clearHistory();
            overlay.mouseDown (mouseAt (overlay, start, start));
            overlay.mouseDrag (mouseAt (overlay, start.translated (0.0f, -50.0f), start));
            overlay.mouseUp (mouseAt (overlay, start.translated (0.0f, -50.0f), start));
            settle (100);
            const auto dragged = processor.readModSlot (0).depth;
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            settle (100);
            expect (std::abs (dragged - 0.5f) < 0.02f && steps.size() == 1 && std::abs (processor.readModSlot (0).depth - 0.2f) < 1.0e-3f
                        && std::abs (processor.readModSlot (1).depth + 0.4f) < 1.0e-3f,
                    "dragging LFO 1's ring up 50 px takes it from +20% to +" + juce::String (juce::roundToInt (dragged * 100.0f))
                        + "% as one undo step, and leaves LFO 2's alone");

            const auto other = overlay.getLocalPoint (cutoff, cutoff->getRingPoint (1, 0.7f));
            cutoff->syncRoutings();
            overlay.mouseDoubleClick (mouseAt (overlay, other, other, 2));
            settle (100);
            expect (processor.readModSlot (1).source == Mod::Source::Lfo2 && std::abs (processor.readModSlot (1).depth) < 1.0e-4f,
                    "double-clicking the outer ring zeroes LFO 2's depth and keeps the routing");
        }

        // Badges: 14 px or more, and past what fits a "+N" badge (S5-5, S6-11).
        const Mod::Source more[] { Mod::Source::Lfo3, Mod::Source::Lfo4, Mod::Source::ModWheel, Mod::Source::Velocity,
                                   Mod::Source::Random, Mod::Source::Aftertouch, Mod::Source::KeyTrack };
        for (int i = 0; i < 7; ++i)
            routeSlot (2 + i, more[i], Mod::Destination::Filter1Cutoff, 0.1f);
        settle (300);
        cutoff = findKnob ("f1_cutoff");
        if (cutoff != nullptr)
        {
            cutoff->syncRoutings();
            auto& strip = cutoff->getDotStrip();
            expect (ModDotStrip::dotSize >= 14 && cutoff->getNumRoutings() == 9 && cutoff->getNumRings() == 3 && strip.hasOverflow()
                        && strip.getNumShown() < 9 && strip.getHeight() <= cutoff->getHeight(),
                    "nine routings: three rings, " + juce::String (strip.getNumShown()) + " badges and a +"
                        + juce::String (9 - strip.getNumShown()) + " that fit the knob (" + juce::String (cutoff->getNumRings()) + " rings, strip "
                        + juce::String (strip.getHeight()) + " px on a " + juce::String (cutoff->getHeight()) + " px knob)");
        }
        else
        {
            expect (false, "the filter page shows CUTOFF");
        }

        for (int i = 0; i < 9; ++i)
            processor.clearModSlot (i);
        settle (200);
    }

    // The strip: all eight macros, MASTER, no page toggle, and GLIDE in the
    // VOICES menu (V5-12, S5-27).
    {
        std::vector<StripKnob*> strip;
        findAll<StripKnob> (editor, strip);
        juce::Array<int> shown;
        auto glideInStrip = false;
        auto narrowest = 1000;
        for (auto* knob : strip)
        {
            if (! visibleInTree (knob))
                continue;
            if (knob->getMacroIndex() >= 0)
            {
                shown.addIfNotAlreadyThere (knob->getMacroIndex());
                narrowest = juce::jmin (narrowest, knob->getWidth());
            }
            glideInStrip = glideInStrip || knob->getKnob().getParameterId() == "glide";
        }

        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        auto pageToggle = false;
        for (auto* button : buttons)
            pageToggle = pageToggle || (visibleInTree (button) && button->getButtonText().contains ("5-8"));

        expect (shown.size() >= 4 && shown.size() <= 8 && narrowest >= 80 && ! pageToggle && ! glideInStrip && findKnob ("master") != nullptr,
                "the strip shows the macros in use, at least four (S10-10: " + juce::String (shown.size()) + ", narrowest " + juce::String (narrowest)
                    + " px) and MASTER, with no 5-8 toggle and no GLIDE");

        GlideMenuItem glide (processor);
        int width = 0, height = 0;
        glide.getIdealSize (width, height);
        glide.setSize (width, height);
        std::vector<ValueSliderControl*> sliders;
        findAll<ValueSliderControl> (glide, sliders);
        expect (sliders.size() == 1 && sliders[0]->getWidth() > 150, "GLIDE has a slider row for the VOICES menu");
    }

    // A macro's card lists where it goes and warns about a target whose
    // module is off; the strip marks the macro (V6-26, S6-38).
    {
        set ("fx_reverb_on", 0.0f);
        routeSlot (0, Mod::Source::Macro1, Mod::Destination::FxReverbMix, 0.5f);
        routeSlot (1, Mod::Source::Macro1, Mod::Destination::Filter1Cutoff, 0.3f);
        settle (700);

        StripKnob* macro1 = nullptr;
        std::vector<StripKnob*> strip;
        findAll<StripKnob> (editor, strip);
        for (auto* knob : strip)
            if (knob->getMacroIndex() == 0)
                macro1 = knob;

        auto* card = ModHoverPopup::instance();
        if (macro1 != nullptr && card != nullptr)
        {
            macro1->openCard();
            settle (100);
            juce::StringArray warnings;
            for (int row = 0; row < card->getNumRows(); ++row)
                if (card->getRowWarning (row).isNotEmpty())
                    warnings.add (card->getRowWarning (row));
            expect (card->isShowingSource() && card->getNumRows() == 2 && warnings.size() == 1 && warnings[0].contains ("Reverb")
                        && macro1->getNumIdleTargets() == 1,
                    "MACRO 1's card lists its two targets and warns '" + warnings.joinIntoString ("; ") + "'; the strip marks it");
            card->close();

            processor.assignFxSlot (1, 13); // Reverb (which switches it on)
            set ("fx_reverb_on", 0.0f);
            settle (100);
            const auto inRackButOff = ModNames::whyDestinationIsIdle (processor, (int) Mod::Destination::FxReverbMix);
            set ("fx_reverb_on", 1.0f);
            settle (700);
            expect (inRackButOff.contains ("switched off") && macro1->getNumIdleTargets() == 0
                        && ModNames::whyDestinationIsIdle (processor, (int) Mod::Destination::FxReverbMix).isEmpty(),
                    "with the reverb in the rack but off the card still warns ('" + inRackButOff
                        + "'); switching it on clears the macro's warning");
            processor.assignFxSlot (1, 0);
        }
        else
        {
            expect (false, "the strip has MACRO 1 and the editor a mod card");
        }

        set ("fx_reverb_on", 0.0f);
    }

    // The matrix: display names, Aux: none, Uni/Bi, dimmed idle rows and
    // merging repeated rows (V5-8, S5-9, S6-29, S6-30, I6-14).
    {
        editor.showPage ("MATRIX");
        settle (400);

        std::vector<MatrixRow*> rows;
        findAll<MatrixRow> (editor, rows);
        const auto rowFor = [&rows] (int slot) -> MatrixRow*
        {
            for (auto* row : rows)
                if (row->getSlotIndex() == slot && visibleInTree (row))
                    return row;
            return nullptr;
        };

        auto* reverbRow = rowFor (0);
        auto* cutoffRow = rowFor (1);
        expect (cutoffRow != nullptr && cutoffRow->getDestinationBox().getText() == juce::String (juce::CharPointer_UTF8 ("FILTER 1 \xe2\x80\xba Cutoff"))
                    && cutoffRow->getSourceBox().getText() == ModNames::source ((int) Mod::Source::Macro1, &processor),
                "a matrix row names its destination 'Filter 1 > Cutoff' and its source as the strip does");

        // An unset VIA is a quiet dashed "+" (review 7, I7-45), not a
        // pill of text on every row.
        std::vector<DashedAddButton*> buttons;
        if (cutoffRow != nullptr)
            findAll<DashedAddButton> (*cutoffRow, buttons);
        auto viaPlus = false;
        for (auto* button : buttons)
            viaPlus = viaPlus || (button->isVisible() && button->getButtonText() == "+" && button->getWidth() <= 60);
        expect (viaPlus, "an unset VIA is a small dashed '+'");

        expect (reverbRow != nullptr && reverbRow->getIdleReason().contains ("Reverb") && reverbRow->getSourceBox().getAlpha() < 0.9f
                    && cutoffRow != nullptr && cutoffRow->getIdleReason().isEmpty() && cutoffRow->getSourceBox().getAlpha() > 0.99f,
                "a row into the switched-off reverb is dimmed and says why ('"
                    + (reverbRow != nullptr ? reverbRow->getIdleReason() : juce::String()) + "'); the cutoff row is not");

        if (cutoffRow != nullptr)
        {
            auto& polarity = cutoffRow->getPolarity();
            const auto automatic = polarity.isEffectivelyBipolar(); // a macro: one way
            polarity.clickSegment (2);
            const auto bi = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (2);
            const auto back = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (0);
            const auto uni = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (0);
            polarity.clickSegment (1);
            const auto negative = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (1);
            expect (! automatic && bi == 2 && back == 0 && uni == 1 && negative == 3 && (int) processor.readModSlot (1).polarity == 0,
                    "+ | - | +/- set positive, negative and bipolar, and a second click goes back to Auto");
        }

        // Two rows with the same source and destination: MERGE REPEATS
        // folds them into one with the summed depth, as one undo step.
        routeSlot (3, Mod::Source::Lfo4, Mod::Destination::Filter1Reso, 0.2f);
        routeSlot (4, Mod::Source::Lfo4, Mod::Destination::Filter1Reso, 0.3f);
        editor.showPage ("MAIN"); // (the page reads its rows as it's shown)
        settle (100);
        editor.showPage ("MATRIX");
        settle (400);
        juce::TextButton* merge = nullptr;
        std::vector<juce::TextButton*> textButtons;
        findAll<juce::TextButton> (editor, textButtons);
        for (auto* button : textButtons)
            if (visibleInTree (button) && button->getButtonText().startsWith ("MERGE REPEATS"))
                merge = button;
        auto* repeated = rowFor (4);
        expect (merge != nullptr && repeated != nullptr && repeated->isDuplicate() && merge->getTooltip().isNotEmpty(),
                "two rows routing LFO 4 to RESO are marked as repeats and MERGE REPEATS is offered, with a tooltip ("
                    + juce::String (merge != nullptr ? 1 : 0) + juce::String (repeated != nullptr ? 1 : 0)
                    + juce::String (repeated != nullptr && repeated->isDuplicate() ? 1 : 0) + ")");

        if (merge != nullptr)
        {
            clearHistory();
            merge->triggerClick();
            settle (300);
            const auto merged = processor.readModSlot (3);
            const auto emptied = processor.readModSlot (4).source == Mod::Source::None;
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            settle (300);
            expect (merged.source == Mod::Source::Lfo4 && std::abs (merged.depth - 0.5f) < 1.0e-3f && emptied && steps.size() == 1
                        && std::abs (processor.readModSlot (4).depth - 0.3f) < 1.0e-3f,
                    "MERGE REPEATS sums the two rows into one (+" + juce::String (juce::roundToInt (merged.depth * 100.0f))
                        + "%) as one undo step, and undo splits them again");
        }

        // A remapped repeat isn't the same sound, so it isn't merged.
        juce::String why;
        auto bent = IlanaSynthAudioProcessor::identityRemap();
        bent.points = { { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.6f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        processor.setModRemap (4, bent);
        const auto refused = processor.isModRemapOn (4) && ! processor.canMergeModSlots (3, 4, &why);
        processor.resetModRemap (4);
        expect (refused, "a repeat with its own remap is not merged (" + why + ")");

        for (int i = 0; i < 5; ++i)
            processor.clearModSlot (i);
        settle (200);
    }

    editor.showPage ("MAIN");
    settle (200);
    runReview7ModulationTests (processor, editor);
}

// UI review 7 (Q2, modulation): every knob takes a source or says it
// can't (I7-5); destination names (I7-17) and where the source menus file
// the Operator Env's sources (I7-16); the chip bar's folding, tray and hover
// line (V7-6, V7-39, S7-8, S7-20); VIA, the add row and the remap title
// (I7-45, V7-20, V7-40, S7-35); badges on hover (V7-27), the macro's
// warning sign (V7-28) and the pinned source's ring (S7-33).
void runReview7ModulationTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto set = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto findKnob = [&editor] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 30)
                return knob;
        return nullptr;
    };
    const auto names = processor.getFactoryPresetNames();
    const auto load = [&] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, names.indexOf (name)));
        settle (300);
    };

    // Every destination names a real parameter, once; the names are the
    // pages' ("OP LFO › Pitch Depth"), never the retired "Op Env" (Q1's
    // one name: Operator Env, short OP ENV, review 7 I7-6).
    {
        juce::StringArray missing, seen, repeated;
        for (const auto& entry : Mod::getParamDestinations())
        {
            if (processor.apvts.getParameter (entry.id) == nullptr)
                missing.add (entry.id);
            if (seen.contains (entry.id))
                repeated.add (entry.id);
            seen.add (entry.id);
        }
        expect (missing.isEmpty() && repeated.isEmpty() && Mod::getNumDestinations() < IlanaSynthAudioProcessor::maxDestinations,
                "every parameter destination names one parameter (" + juce::String (Mod::getNumDestinations()) + " destinations"
                    + (missing.isEmpty() ? juce::String() : "; missing: " + missing.joinIntoString (", "))
                    + (repeated.isEmpty() ? juce::String() : "; repeated: " + repeated.joinIntoString (", ")) + ")");

        const auto nameOf = [] (const char* id) { return ModNames::destination (Mod::destinationForParamId (id)); };
        const auto arrow = juce::String::fromUTF8 (" \xe2\x80\xba ");
        juce::StringArray old;
        for (int d = 0; d < Mod::getNumDestinations(); ++d)
            if (ModNames::destination (d).contains ("Op Env") || ModNames::destination (d).containsIgnoreCase ("Op EG")
                || ModNames::destination (d).contains ("Op LFO") || ModNames::destination (d).contains ("Op Pitch"))
                old.add (ModNames::destination (d));
        expect (nameOf ("opeg_lfo_pmd") == "OP LFO" + arrow + "Pitch Depth" && nameOf ("opeg_pitch_l1") == "OP PITCH" + arrow + "Peak"
                    && nameOf ("osc1_eg_r2") == "OSC 1" + arrow + "OP ENV Decay 1"
                    && ModNames::destination ((int) Mod::Destination::Drift) == "VOICE" + arrow + "Analog Drift"
                    && nameOf ("lfo3_phys_a") == "LFO 3" + arrow + "Physics A" && nameOf ("macro2_evolve") == "MACRO 2" + arrow + "Evolve"
                    && nameOf ("fx_awtape_p2") == "FX AW TAPE" + arrow + "Knob 2" && nameOf ("env7_hold") == "ENV 7" + arrow + "Hold"
                    && old.isEmpty(),
                "new destinations read 'Module › Control' (" + nameOf ("opeg_lfo_pmd") + ", " + nameOf ("osc1_eg_r2") + ", "
                    + nameOf ("fx_awtape_p2") + ")" + (old.isEmpty() ? juce::String() : "; old names: " + old.joinIntoString (", ")));
    }

    // The source menus file the MSEG and Op LFO with the LFOs, Op Pitch
    // with the envelopes, as the MOD page's rows do (I7-16).
    {
        using S = Mod::Source;
        const auto& order = ModNames::sourcesInMenuOrder();
        const auto at = [&order] (S s) { return (int) (std::find (order.begin(), order.end(), (int) s) - order.begin()); };
        expect (ModNames::groupOf ((int) S::Mseg) == ModNames::SourceGroup::lfo && ModNames::groupOf ((int) S::OpLfo) == ModNames::SourceGroup::lfo
                    && ModNames::groupOf ((int) S::OpPitchEnv) == ModNames::SourceGroup::envelope
                    && at (S::OpLfo) == at (S::Mseg) + 1 && at (S::Mseg) > at (S::Lfo16) && at (S::OpPitchEnv) > at (S::Env16)
                    && at (S::OpPitchEnv) < at (S::Macro1) && (int) order.size() == (int) S::Count - 1,
                "the source menus put MSEG and Op LFO after the LFOs, Op Pitch after the envelopes");
    }

    // Knobs: a source on one that can't take it is refused with a note,
    // and its track and tooltip say so; the special systems' knobs take
    // sources now (I7-5).
    {
        load ("E.PIANO 1 (ROM1A)");
        editor.showPage ("FM");
        settle (400);
        const auto anyKnob = [&editor] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id)
                    return knob;
            return nullptr;
        };
        auto* decay1 = anyKnob ("osc1_eg_r2");
        auto* peak = anyKnob ("osc1_eg_l1");
        expect (decay1 != nullptr && decay1->isModulatable() && peak != nullptr && peak->isModulatable(),
                "the Operator Env's DECAY 1 and PEAK take a source");

        // Every knob on every page either takes a source or is drawn and
        // labelled as one that can't (no silent drop targets).
        juce::StringArray silent;
        auto refusing = 0;
        for (const auto& preset : { juce::String ("E.PIANO 1 (ROM1A)"), juce::String ("Neuro Wobble") })
        {
            load (preset);
            set ("fx_slot4", 32.0f);
            set ("lfo2_shape", 20.0f);
            settle (200);
            for (const auto& page : editor.getPageIds())
            {
                editor.showPage (page);
                settle (200);
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (editor, knobs);
                for (auto* knob : knobs)
                    if (! knob->isModulatable())
                    {
                        const auto marked = (bool) knob->getSlider().getProperties().getWithDefault ("notModulatable", false);
                        const auto source = knob->getParameterId().startsWith ("macro"); // (a macro is a source)
                        if (! source && (! marked || ! knob->getTooltip().contains ("Not modulatable")))
                            silent.addIfNotAlreadyThere (knob->getParameterId());
                        refusing += source ? 0 : 1;
                    }
            }
        }
        expect (silent.isEmpty(), "no knob takes a dropped source silently: each one that can't has a dotted track and says so ("
                                      + juce::String (refusing) + " marked" + (silent.isEmpty() ? juce::String() : "; unmarked: " + silent.joinIntoString (", ")) + ")");

        for (const auto* id : { "lfo2_p1", "lfo2_phys_a", "macro1_evolve", "fx_awtape_p1", "fx_awtape_mix", "env6_attack", "amp_hold",
                                "osc1_ratio", "opeg_pitch_r1", "opeg_lfo_delay", "fm_1to2" })
            if (modRingConfigFor (id).destination == 0)
                silent.add (id);
        expect (silent.isEmpty(), "the chaos and physics LFO knobs, EVOLVE, Airwindows, envelope extras, RATIO, Op Pitch / Op LFO "
                                  "and the FM cells take a source" + (silent.isEmpty() ? juce::String() : " (not: " + silent.joinIntoString (", ") + ")"));

        // A drop on one that can't: no routing, a note.
        load ("Neuro Wobble");
        editor.showPage ("FX");
        set ("fx_slot4", 31.0f); // the vocoder (its knobs stay unmodulated, on purpose)
        settle (400);
        KnobControl* refuser = nullptr;
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (visibleInTree (knob) && ! knob->isModulatable() && ! knob->getParameterId().startsWith ("macro"))
                refuser = refuser != nullptr ? refuser : knob;
        auto routingsBefore = 0, routingsAfter = 0;
        for (int i = 0; i < Mod::maxSlots; ++i)
            routingsBefore += processor.readModSlot (i).source != Mod::Source::None ? 1 : 0;
        if (refuser != nullptr)
        {
            const juce::DragAndDropTarget::SourceDetails drag (juce::var ("modsource:" + juce::String ((int) Mod::Source::Lfo3)), nullptr, {});
            refuser->itemDropped (drag);
            settle (50);
        }
        for (int i = 0; i < Mod::maxSlots; ++i)
            routingsAfter += processor.readModSlot (i).source != Mod::Source::None ? 1 : 0;
        expect (refuser != nullptr && routingsAfter == routingsBefore && FloatingNote::shownText().endsWith ("can't be modulated"),
                "a source dropped on " + (refuser != nullptr ? refuser->getParameterId() : juce::String ("(none)"))
                    + " is refused with a note ('" + FloatingNote::shownText() + "')");
        set ("fx_slot4", 0.0f);
        settle (100);
    }

    // The chip bar: on a DX7 voice with LFO 1-3 in the pool, the rare chips
    // (Op LFO, Op Pitch, MSEG, Random, Pressure) fold before LFO 2 / 3, the
    // bar folds alike at 75 % and 100 %, the tray opens over its chip, and
    // the hover line never covers the chips.
    {
        using Module = IlanaSynthAudioProcessor::Module;
        load ("E.PIANO 1 (ROM1A)");
        for (int i = 0; i < 3; ++i)
            processor.setRevealed (Module::Lfo, i, true);
        for (int i = 0; i < 4; ++i)
            processor.setRevealed (Module::Envelope, i, true);
        editor.showPage ("MAIN");
        settle (600);
        const auto barState = [&editor]
        {
            std::vector<ModSourceChip*> found;
            findAll<ModSourceChip> (editor, found);
            juce::StringArray shown;
            for (auto* chip : found)
                if (chip->isVisible() && dynamic_cast<ModSourceTray*> (chip->getParentComponent()) == nullptr)
                    shown.add (chip->getSourceName());
            std::vector<ModSourceGroupChip*> groups;
            findAll<ModSourceGroupChip> (editor, groups);
            for (auto* group : groups)
                if (group->isVisible())
                    shown.add (group->getLabel());
            return shown;
        };
        auto* top = editor.getTopLevelComponent();
        const auto before = top->getBounds();
        top->setSize (1060, 720);
        settle (300);
        const auto full = barState();
        top->setSize (795, 540);
        settle (300);
        const auto small = barState();
        top->setBounds (before);
        settle (300);
        expect (full.contains ("LFO 1") && full.contains ("LFO 2") && full.contains ("LFO 3") && full.contains ("FILT ENV")
                    && full.contains ("OP LFO") == small.contains ("OP LFO") && full == small,
                "a DX7 voice keeps LFO 1-3 and its envelopes in the bar, folding alike at 100 % and 75 % ("
                    + full.joinIntoString (" | ") + (full == small ? juce::String() : "  vs  " + small.joinIntoString (" | ")) + ")");

        // The bar files Op LFO with the LFOs and Op Pitch with the envelopes.
        std::vector<ModSourceChip*> found;
        findAll<ModSourceChip> (editor, found);
        int lfo1X = -1, lfo3X = -1, opLfoX = -1, filtEnvX = -1, opPitchX = -1, velocityX = -1;
        for (auto* chip : found)
            if (dynamic_cast<ModSourceTray*> (chip->getParentComponent()) == nullptr)
            {
                const auto x = chip->getX();
                switch ((Mod::Source) chip->getSourceIndex())
                {
                    case Mod::Source::Lfo1:       lfo1X = x; break;
                    case Mod::Source::Lfo3:       lfo3X = x; break;
                    case Mod::Source::OpLfo:      opLfoX = x; break;
                    case Mod::Source::FilterEnv:  filtEnvX = x; break;
                    case Mod::Source::OpPitchEnv: opPitchX = x; break;
                    case Mod::Source::Velocity:   velocityX = x; break;
                    default: break;
                }
            }
        // (Review 8: the Operator Env's chips lead their groups on a DX7 voice.)
        expect (opLfoX < lfo1X && lfo1X < lfo3X && lfo3X < opPitchX && opPitchX < filtEnvX && filtEnvX < velocityX,
                "the bar's order is OP LFO, LFOs, OP PITCH, envelopes, then performance");

        // A crowded bar: the tray opens over its group chip.
        std::vector<bool> lfoBefore;
        for (int i = 0; i < 16; ++i)
        {
            lfoBefore.push_back (processor.isRevealed (Module::Lfo, i));
            processor.setRevealed (Module::Lfo, i, true);
        }
        settle (600);
        std::vector<ModSourceGroupChip*> groups;
        findAll<ModSourceGroupChip> (editor, groups);
        ModSourceGroupChip* lfoGroup = nullptr;
        for (auto* group : groups)
            if (group->isVisible() && group->getGroupName() == "LFOs")
                lfoGroup = group;
        auto anchored = false;
        if (lfoGroup != nullptr && lfoGroup->onOpen != nullptr)
        {
            lfoGroup->onOpen (*lfoGroup);
            settle (100);
            if (auto* tray = findChild<ModSourceTray> (editor); tray != nullptr && tray->isVisible())
            {
                const auto chipArea = tray->getParentComponent()->getLocalArea (lfoGroup, lfoGroup->getLocalBounds());
                anchored = tray->getX() <= chipArea.getX() && tray->getRight() > chipArea.getX()
                           && (tray->getX() >= chipArea.getX() - 12 || tray->getRight() >= tray->getParentComponent()->getWidth() - 12);
                tray->close();
            }
        }
        for (int i = 0; i < 16; ++i)
            processor.setRevealed (Module::Lfo, i, lfoBefore[(size_t) i]);
        expect (anchored, "a group chip's tray opens over that chip, not at the window's edge");

        // The hover line sits in the dock's macro row, under the chips, not over them.
        auto& line = editor.getHoverLine();
        auto overChips = false;
        for (auto* chip : found)
            if (chip->isVisible() && chip->getParentComponent() == line.getParentComponent())
                overChips = overChips || chip->getBounds().intersects (line.getBounds());
        expect (! overChips && lfoGroup != nullptr && line.getBounds().getY() >= lfoGroup->getBottom() - 2,
                "the hover line sits under the source chips, not over them");

        // Its title is the matrix's name for the knob.
        editor.showPage ("MAIN");
        settle (200);
        if (auto* frame = findKnob ("osc1_level"))
        {
            line.restOn (&frame->getSlider());
            const auto title = line.getShownTitle();
            line.restOn (nullptr);
            expect (title == ModNames::destination ((int) Mod::Destination::Osc1Level) || title.isEmpty(),
                    "the hover line names a knob as the matrix does ('" + title + "')");
        }
    }

    // MATRIX: VIA is a "+" (the add row covers the rest in the Snapshot
    // tests), and the remap header names the routing, not a slot.
    {
        load ("Neuro Wobble");
        editor.showPage ("MATRIX");
        settle (400);
        std::vector<MatrixRow*> rows;
        findAll<MatrixRow> (editor, rows);
        MatrixRow* first = nullptr;
        for (auto* row : rows)
            if (row->isVisible() && (first == nullptr || row->getY() < first->getY()))
                first = row;
        if (first != nullptr)
        {
            first->getCurve().onOpenRemap (first->getSlotIndex());
            settle (200);
        }
        std::vector<RemapEditor*> remaps;
        findAll<RemapEditor> (editor, remaps);
        const auto title = remaps.empty() ? juce::String() : remaps[0]->titleText();
        expect (title.contains (juce::String::fromUTF8 ("\xe2\x86\x92")) && ! title.containsIgnoreCase ("slot")
                    && ! title.containsIgnoreCase ("row"),
                "the remap editor's title names its routing ('" + title + "')");
        if (first != nullptr)
            first->getCurve().onOpenRemap (first->getSlotIndex());
        settle (100);
    }

    // Badges beside the rings only while the knob is hovered (V7-27); a
    // pinned source's ring takes the whole band (S7-33).
    {
        processor.loadFactoryPreset (0);
        editor.showPage ("FILTER");
        settle (300);
        set ("mod1_src", (float) Mod::Source::Lfo1);
        set ("mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        set ("mod1_amt", 0.3f);
        settle (300);
        if (auto* cutoff = findKnob ("f1_cutoff"))
        {
            cutoff->syncRoutings();
            settle (100);
            const auto quiet = ! cutoff->areBadgesShown();
            cutoff->setBadgesShown (true);
            const auto hovered = cutoff->areBadgesShown() || cutoff->getDotStrip().getNumShown() == 0;
            cutoff->setBadgesShown (false);
            expect (quiet && hovered, "a knob's badges show only while it is hovered (the rings are the legend)");

            auto& overlay = cutoff->getRingOverlay();
            const auto centre = cutoff->getDialCentre();
            const auto probe = overlay.getLocalPoint (cutoff, centre.getPointOnCircumference (cutoff->getRingRadius (0) - 3.5f,
                                                                                           juce::MathConstants<float>::pi * 1.6f)).roundToInt();
            const auto loose = overlay.hitTest (probe.x, probe.y);
            pinnedModSource() = (int) Mod::Source::Lfo1;
            const auto pinned = overlay.hitTest (probe.x, probe.y);
            pinnedModSource() = 0;
            expect (! loose && pinned, "a pinned source's ring takes a wider band than a loose one ("
                                           + juce::String (cutoff->getNumRings()) + " rings)");
        }
        else
        {
            expect (false, "FILTER shows CUTOFF");
        }
        processor.clearModSlot (0);
    }

    // The macro's warning sign explains itself: its tooltip names the
    // target that can't be heard (V7-28).
    {
        load ("E.PIANO 1 (ROM1A)");
        editor.showPage ("MAIN");
        std::vector<StripKnob*> macros;
        findAll<StripKnob> (editor, macros);
        StripKnob* warned = nullptr;
        for (int wait = 0; wait < 20 && warned == nullptr; ++wait)
        {
            settle (100);
            for (auto* macro : macros)
                if (macro->getNumIdleTargets() > 0)
                    warned = macro;
        }
        if (warned != nullptr)
            warned->createComponentSnapshot (warned->getLocalBounds()); // (lays the sign out)
        expect (warned == nullptr || (warned->getTooltip().contains ("can't be heard") && ! warned->getIdleMarkBounds().isEmpty()),
                "a macro with a silent target says which in its tooltip ('"
                    + (warned != nullptr ? warned->getTooltip().upToFirstOccurrenceOf ("\n", false, false) : juce::String ("none")) + "')");
    }

    // Review 16 (V15-17, V15-18): every visible macro tile has one shape and its whole
    // name; the chip row's fold reads "N MORE" like the pools.
    {
        std::vector<StripKnob*> tiles;
        findAll<StripKnob> (editor, tiles);
        juce::Rectangle<int> first;
        auto same = true, whole = true;
        for (auto* tile : tiles)
            if (tile->getMacroIndex() >= 0 && tile->isVisible())
            {
                if (first.isEmpty())
                    first = tile->getLocalBounds();
                same = same && tile->getWidth() == first.getWidth() && tile->getHeight() == first.getHeight();
                const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true).withKerningFactor (0.07f);
                const auto room = tile->getWidth() - 40 - (tile->isEvolving() ? 16 : 0) - 2;
                whole = whole && (float) juce::GlyphArrangement::getStringWidthInt (font, processor.getMacroName (tile->getMacroIndex()).toUpperCase()) <= (float) room + 0.5f;
            }
        expect (same, "every macro tile has the same size");
        expect (whole, "a macro's name has room in its tile (no cut)");
        std::vector<ModSourceGroupChip*> groups;
        findAll<ModSourceGroupChip> (editor, groups);
        for (auto* group : groups)
            expect (group->getLabel().endsWith (" MORE"), "the chip row's fold reads 'N MORE' ('" + group->getLabel() + "')");
    }

    editor.showPage ("MAIN");
    settle (200);
}
