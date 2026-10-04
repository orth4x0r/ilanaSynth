// UI review 6, P1 (modulation): the depth rings and badges on a knob, the
// eight-macro strip and its cards, and the matrix's names, Uni/Bi, idle rows
// and merging. Included from Snapshot.cpp after its helpers (findAll,
// findChild, settle, expect, visibleInTree).
#pragma once

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

        expect (shown.size() == 8 && narrowest >= 80 && ! pageToggle && ! glideInStrip && findKnob ("master") != nullptr,
                "the strip shows all eight macros (" + juce::String (shown.size()) + ", narrowest " + juce::String (narrowest)
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
        expect (cutoffRow != nullptr && cutoffRow->getDestinationBox().getText() == juce::String (juce::CharPointer_UTF8 ("Filter 1 \xe2\x80\xba Cutoff"))
                    && cutoffRow->getSourceBox().getText() == ModNames::source ((int) Mod::Source::Macro1, &processor),
                "a matrix row names its destination 'Filter 1 > Cutoff' and its source as the strip does");

        std::vector<juce::TextButton*> buttons;
        if (cutoffRow != nullptr)
            findAll<juce::TextButton> (*cutoffRow, buttons);
        auto auxNone = false;
        for (auto* button : buttons)
            auxNone = auxNone || (button->isVisible() && button->getButtonText() == "Aux: none");
        expect (auxNone, "an unset VIA reads 'Aux: none'");

        expect (reverbRow != nullptr && reverbRow->getIdleReason().contains ("Reverb") && reverbRow->getSourceBox().getAlpha() < 0.9f
                    && cutoffRow != nullptr && cutoffRow->getIdleReason().isEmpty() && cutoffRow->getSourceBox().getAlpha() > 0.99f,
                "a row into the switched-off reverb is dimmed and says why ('"
                    + (reverbRow != nullptr ? reverbRow->getIdleReason() : juce::String()) + "'); the cutoff row is not");

        if (cutoffRow != nullptr)
        {
            auto& polarity = cutoffRow->getPolarity();
            const auto automatic = polarity.isEffectivelyBipolar(); // a macro: one way
            polarity.clickSegment (1);
            const auto bi = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (1);
            const auto back = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (0);
            const auto uni = (int) processor.readModSlot (1).polarity;
            polarity.clickSegment (0);
            expect (! automatic && bi == 2 && back == 0 && uni == 1 && (int) processor.readModSlot (1).polarity == 0,
                    "UNI | BI sets unipolar and bipolar, and a second click goes back to Auto");
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
        buttons.clear();
        findAll<juce::TextButton> (editor, buttons);
        for (auto* button : buttons)
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
}
