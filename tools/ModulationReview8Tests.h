// UI review 8, package R4 (modulation): the stable chip bar, the one
// "MODULE › Control" formatter, the hover line's own strip, drop targets
// while a source is dragged, the matrix (grouped by source, one repeat
// count, rows that fill the page, quick remap shapes), quiet unassigned
// macros and the knob card's width. Included from Snapshot.cpp after its
// helpers (findAll, findChild, settle, expect, visibleInTree).
#pragma once

void runModulationReview8Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (name));
        settle (500);
    };
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
    const auto freeSlot = [&processor]
    {
        for (int slot = 0; slot < Mod::maxSlots; ++slot)
            if (processor.readModSlot (slot).source == Mod::Source::None && processor.readModSlot (slot).destination == 0)
                return slot;
        return -1;
    };

    // The bar's chips (not a tray's), by source, in the editor's space.
    const auto barChips = [&editor]
    {
        std::map<int, juce::Rectangle<int>> result;
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        for (auto* chip : chips)
            if (visibleInTree (chip) && chip->getWidth() > 0 && dynamic_cast<ModSourceTray*> (chip->getParentComponent()) == nullptr
                && findChild<juce::TabbedComponent> (editor) != nullptr
                && ! findChild<juce::TabbedComponent> (editor)->isParentOf (chip))
                result[chip->getSourceIndex()] = editor.getLocalArea (chip, chip->getLocalBounds());
        return result;
    };
    const auto foldedSources = [&editor]
    {
        std::vector<int> result;
        std::vector<ModSourceGroupChip*> groups;
        findAll<ModSourceGroupChip> (editor, groups);
        for (auto* group : groups)
            if (visibleInTree (group))
                result.insert (result.end(), group->getSources().begin(), group->getSources().end());
        return result;
    };
    using S = Mod::Source;
    const auto xOf = [] (const std::map<int, juce::Rectangle<int>>& chips, S source)
    {
        const auto found = chips.find ((int) source);
        return found == chips.end() ? -1 : found->second.getX();
    };

    editor.showPage ("MAIN");
    {
        juce::String widths;
        for (const auto* name : { "OP LFO", "LFO 1", "OP PITCH", "AMP ENV", "FILT ENV", "FILT 2 ENV", "VELOCITY", "KEY TRACK", "MOD WHEEL",
                                  "PRESSURE", "RANDOM" })
            widths << name << " " << ModSourceChip::layoutWidthFor (name) << ", ";
        std::cout << "NOTE: chip widths " << widths << "group +2 " << ModSourceGroupChip::layoutWidthFor ({}, 2) << std::endl;
    }

    // V8-3 / S8-2 / I8-9: the chip bar keeps its places across presets; the
    // Operator Env's chips lead their regions on a DX7 voice and are absent
    // elsewhere; no patch-level MSEG chip on a patch that doesn't route it.
    {
        loadNamed ("Neuro Wobble");
        const auto neuro = barChips();
        const auto neuroFolded = foldedSources();
        loadNamed ("Init");
        const auto init = barChips();
        loadNamed ("E.PIANO 1 (ROM1A)");
        const auto dx7 = barChips();
        const auto dx7Folded = foldedSources();

        const auto absent = [] (const std::map<int, juce::Rectangle<int>>& chips, const std::vector<int>& folded, S source)
        {
            return chips.count ((int) source) == 0 && std::find (folded.begin(), folded.end(), (int) source) == folded.end();
        };
        expect (absent (neuro, neuroFolded, S::OpLfo) && absent (neuro, neuroFolded, S::OpPitchEnv) && absent (neuro, neuroFolded, S::Mseg),
                "no OP LFO, OP PITCH or MSEG chip on a wavetable patch (V8-1, V8-2)");
        expect (dx7.count ((int) S::OpLfo) == 1 && dx7.count ((int) S::OpPitchEnv) == 1
                    && xOf (dx7, S::OpLfo) < xOf (dx7, S::Lfo1) && xOf (dx7, S::OpPitchEnv) < xOf (dx7, S::FilterEnv)
                    && absent (dx7, dx7Folded, S::AmpEnv),
                "a DX7 voice shows OP LFO and OP PITCH unfolded, first in their groups, and no unused AMP ENV (S8-2, I8-9, S9-17)");
        expect (xOf (neuro, S::Velocity) > 0 && xOf (neuro, S::Velocity) == xOf (init, S::Velocity)
                    && xOf (init, S::Velocity) == xOf (dx7, S::Velocity)
                    && xOf (neuro, S::AmpEnv) == xOf (init, S::AmpEnv) && xOf (neuro, S::Lfo1) == xOf (init, S::Lfo1)
                    && xOf (neuro, S::Lfo1) == xOf (dx7, S::OpLfo) && xOf (neuro, S::AmpEnv) == xOf (dx7, S::OpPitchEnv),
                "the chip regions start at the same x on every patch (VELOCITY " + juce::String (xOf (neuro, S::Velocity)) + " / "
                    + juce::String (xOf (init, S::Velocity)) + " / " + juce::String (xOf (dx7, S::Velocity)) + ", AMP ENV "
                    + juce::String (xOf (neuro, S::AmpEnv)) + " / " + juce::String (xOf (init, S::AmpEnv)) + ")");
        expect (neuro.count ((int) S::Random) == 1 && neuro.count ((int) S::Aftertouch) == 1 && neuro.count ((int) S::Lfo3) == 1
                    && neuro.count ((int) S::FilterEnv2) == 1,
                "Neuro Wobble's bar shows its three LFOs, three envelopes and all five performance sources");

        // Nothing overlaps; everything stays in the bar.
        std::vector<juce::Rectangle<int>> boxes;
        for (const auto& [source, box] : dx7)
            boxes.push_back (box);
        std::vector<ModSourceGroupChip*> groups;
        findAll<ModSourceGroupChip> (editor, groups);
        for (auto* group : groups)
            if (visibleInTree (group))
                boxes.push_back (editor.getLocalArea (group, group->getLocalBounds()));
        auto overlaps = 0, outside = 0;
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            outside += boxes[i].getX() < 0 || boxes[i].getRight() > editor.getWidth() ? 1 : 0;
            for (size_t j = i + 1; j < boxes.size(); ++j)
                overlaps += boxes[i].intersects (boxes[j]) ? 1 : 0;
        }
        expect (overlaps == 0 && outside == 0, "the chip bar's chips never overlap or leave the window (" + juce::String (overlaps) + ", "
                                                   + juce::String (outside) + ")");
    }

    // Routing a chip never moves the bar (V8-3).
    {
        loadNamed ("Neuro Wobble");
        const auto before = barChips();
        const auto slot = freeSlot();
        routeSlot (slot, S::Random, Mod::Destination::Filter1Reso, 0.2f);
        settle (300);
        const auto after = barChips();
        processor.clearModSlot (slot);
        settle (300);
        expect (before == after, "routing RANDOM moves no chip in the bar");
    }

    // VECTOR X / Y show unfolded while the vector is on (S8-20).
    {
        set ("vec_on", 1.0f);
        settle (300);
        const auto chips = barChips();
        set ("vec_on", 0.0f);
        settle (300);
        expect (chips.count ((int) S::VectorX) == 1 && chips.count ((int) S::VectorY) == 1,
                "VECTOR X and VECTOR Y are chips of their own in the bar while VECTOR is on (S8-20)");
    }

    // The hover line has its own strip: it covers no page and no chip
    // (S8-11, V8-31).
    {
        auto* line = findChild<InfoStrip> (editor);
        auto* tabs = findChild<juce::TabbedComponent> (editor);
        const auto chips = barChips();
        auto chipTop = editor.getHeight();
        for (const auto& [source, box] : chips)
            chipTop = juce::jmin (chipTop, box.getY());
        const auto lineBox = line != nullptr ? editor.getLocalArea (line, line->getLocalBounds()) : juce::Rectangle<int>();
        const auto pageBox = tabs != nullptr ? editor.getLocalArea (tabs, tabs->getLocalBounds()) : juce::Rectangle<int>();
        expect (line != nullptr && tabs != nullptr && lineBox.getHeight() >= 14 && lineBox.getY() >= pageBox.getBottom()
                    && lineBox.getBottom() <= chipTop,
                "the hover line sits in its own strip between the pages and the chips (" + lineBox.toString() + ", page "
                    + pageBox.toString() + ", chips at " + juce::String (chipTop) + ")");
    }

    // One formatter: "MODULE › Control", the control as its knob is labelled
    // (V8-13, I8-12, S8-11, S8-26).
    {
        const auto sep = juce::String::fromUTF8 (" \xe2\x80\xba ");
        expect (ModNames::destination ((int) Mod::Destination::Filter1Cutoff) == "FILTER 1" + sep + "Cutoff"
                    && ModNames::destination ((int) Mod::Destination::Osc1Pitch) == "OSC 1" + sep + "Semi"
                    && ModNames::destination ((int) Mod::Destination::FxReverbMix) == "FX REVERB" + sep + "Mix",
                "destinations read MODULE › Control (" + ModNames::destination ((int) Mod::Destination::Osc1Pitch) + ")");
        expect (ModNames::source ((int) S::ModWheel) == "MOD WHEEL" && ModNames::source ((int) S::FilterEnv2) == "FILT 2 ENV"
                    && ModNames::source ((int) S::Macro1, &processor) == processor.getMacroName (0).toUpperCase(),
                "sources read as their chips do, a named macro as TONE (I9-8) (" + ModNames::source ((int) S::Macro1, &processor) + ")");

        // Every modulatable knob on PLAY: its name's module in capitals.
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        juce::StringArray lower;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getRingDestination() != 0)
            {
                const auto name = ModNames::destination (knob->getRingDestination());
                const auto module = name.upToFirstOccurrenceOf (sep, false, false);
                if (name.contains (sep) && module != module.toUpperCase())
                    lower.add (name);
            }
        expect (lower.isEmpty(), "every knob's destination name has its module in capitals" + (lower.isEmpty() ? juce::String()
                                                                                                          : ": " + lower.joinIntoString (", ")));
    }

    // V8-10: while a source is dragged, every knob shows whether it takes
    // it: a modulatable one draws a target ring, one that can't is dimmed.
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        KnobControl* target = nullptr;
        KnobControl* refuses = nullptr;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getWidth() > 0 && ! knob->isCompact())
            {
                if (target == nullptr && knob->getRingDestination() != 0 && knob->getNumRoutings() == 0)
                    target = knob;
                if (refuses == nullptr && knob->getRingDestination() == 0)
                    refuses = knob;
            }
        const auto changes = [] (KnobControl* knob)
        {
            if (knob == nullptr)
                return false;
            const auto before = knob->createComponentSnapshot (knob->getLocalBounds(), false, 1.0f);
            knob->showDragTarget ((int) S::Lfo1);
            const auto during = knob->createComponentSnapshot (knob->getLocalBounds(), false, 1.0f);
            knob->showDragTarget (0);
            auto differ = 0;
            for (int y = 0; y < before.getHeight(); y += 2)
                for (int x = 0; x < before.getWidth(); x += 2)
                    differ += before.getPixelAt (x, y) != during.getPixelAt (x, y) ? 1 : 0;
            return differ > 10;
        };
        expect (changes (target) && changes (refuses), "a source drag marks every knob: a target ring, or dimmed when it can't take it");
        expect (modSourceBeingDragged (editor) == 0, "no source drag reads as none");
    }

    // V8-37: a macro routed nowhere reads quietly; the preset's own don't.
    {
        loadNamed ("Neuro Wobble");
        std::vector<StripKnob*> macros;
        findAll<StripKnob> (editor, macros);
        auto quiet = 0, named = 0;
        for (auto* macro : macros)
        {
            if (macro->getMacroIndex() < 0)
                continue;
            macro->refreshTargets();
            quiet += macro->isAssigned() ? 0 : 1;
            named += macro->isAssigned() && macro->getKnob().getAlpha() > 0.99f ? 1 : 0;
        }
        expect (quiet == 4 && named == 4, "Neuro's four unrouted macros read quietly, its four named ones don't (" + juce::String (quiet)
                                              + " quiet, " + juce::String (named) + " named)");
    }

    // S8-35: a knob's card is as wide as its content needs.
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        KnobControl* modulated = nullptr;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getNumRoutings() > 0 && knob->getParameterId() == "osc1_frame")
                modulated = knob;
        auto* card = ModHoverPopup::instance();
        if (modulated != nullptr && card != nullptr)
        {
            modulated->openModCard (true);
            settle (100);
            const auto width = card->getWidth();
            modulated->closeModCard();
            expect (card != nullptr && width >= 170 && width < 230, "FRAME's card fits its content (" + juce::String (width) + " px)");
        }
        else
        {
            expect (false, "Neuro's OSC 1 FRAME is modulated and has a card");
        }
    }

    // The matrix: grouped by source, one repeat count, rows fill the page,
    // quick shapes beside an open remap (S8-25, V8-20).
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MATRIX");
        settle (400);
        auto* page = editor.getCurrentPage();
        std::vector<MatrixRow*> rows;
        if (page != nullptr)
            findAll<MatrixRow> (*page, rows);
        std::vector<MatrixRow*> shown;
        for (auto* row : rows)
            if (row->isVisible())
                shown.push_back (row);
        std::sort (shown.begin(), shown.end(), [] (MatrixRow* a, MatrixRow* b) { return a->getY() < b->getY(); });
        const auto& order = ModNames::sourcesInMenuOrder();
        const auto rank = [&] (MatrixRow* row)
        {
            return (int) std::distance (order.begin(), std::find (order.begin(), order.end(), (int) processor.readModSlot (row->getSlotIndex()).source));
        };
        auto grouped = ! shown.empty();
        for (size_t i = 1; i < shown.size(); ++i)
            grouped = grouped && rank (shown[i - 1]) <= rank (shown[i]);
        expect (grouped, "the matrix opens with its rows grouped by source (S8-25)");

        // The rows (and the add row) fill the page's height when the dock's
        // note doesn't fit under them.
        auto* viewport = page != nullptr ? findChild<juce::Viewport> (*page) : nullptr;
        if (viewport != nullptr && ! shown.empty())
        {
            const auto lastBottom = shown.back()->getBottom() + shown.back()->getHeight();
            const auto gap = viewport->getHeight() - lastBottom;
            expect (gap < shown.back()->getHeight() * 2 + 12, "matrix rows fill the page instead of leaving its bottom empty ("
                                                                  + juce::String (gap) + " px left, rows " + juce::String (shown.back()->getHeight()) + " px)");
        }

        // A repeated routing: one count, of routings, naming the rows.
        const auto slot = freeSlot();
        const auto first = processor.readModSlot (shown.front()->getSlotIndex());
        routeSlot (slot, first.source, (Mod::Destination) first.destination, 0.1f);
        editor.showPage ("MAIN"); // (the page reads its rows as it's shown)
        settle (100);
        editor.showPage ("MATRIX");
        settle (400);
        page = editor.getCurrentPage();
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*page, buttons);
        juce::TextButton* merge = nullptr;
        for (auto* button : buttons)
            if (button->isVisible() && button->getButtonText().startsWith ("MERGE"))
                merge = button;
        juce::StringArray texts;
        for (auto* button : buttons)
            if (button->isVisible())
                texts.add (button->getButtonText());
        expect (merge != nullptr && merge->getButtonText() == "MERGE REPEATS" && merge->getTooltip().contains ("1 routing is repeated (rows "),
                "a repeated routing is counted once, by routing, with its rows (V8-20)"
                    + (merge != nullptr ? " (" + merge->getTooltip().upToFirstOccurrenceOf (".", false, false).fromFirstOccurrenceOf ("\n", false, false) + ")"
                                        : " (slot " + juce::String (slot) + ", buttons: " + texts.joinIntoString (", ") + ")"));
        processor.clearModSlot (slot);
        editor.showPage ("MAIN");
        settle (100);
        editor.showPage ("MATRIX");
        settle (300);
        page = editor.getCurrentPage();

        // Quick shapes beside an open remap: inside the dock, clear of the
        // editor, and a click sets the curve.
        rows.clear();
        findAll<MatrixRow> (*page, rows);
        MatrixRow* row = nullptr;
        for (auto* candidate : rows)
            if (candidate->isVisible() && row == nullptr)
                row = candidate;
        if (row != nullptr)
        {
            row->getCurve().openRemapEditor();
            settle (200);
            auto* remap = findChild<RemapEditor> (*page);
            std::vector<RemapShapeTile*> tiles;
            findAll<RemapShapeTile> (*page, tiles);
            auto visibleTiles = 0, clashes = 0;
            for (auto* tile : tiles)
                if (tile->isVisible())
                {
                    ++visibleTiles;
                    clashes += remap == nullptr || tile->getBounds().intersects (remap->getBounds()) || tile->getRight() > page->getWidth()
                               || tile->getWidth() < 30 ? 1 : 0;
                }
            juce::String applied;
            if (remap != nullptr && ! tiles.empty())
            {
                tiles[1]->triggerClick();
                settle (100);
                const auto curve = processor.getModRemap (row->getSlotIndex());
                applied = curve.points.size() == 2 && curve.points.front().y > 0.9f ? "Invert" : "?";
                processor.resetModRemap (row->getSlotIndex());
                remap->close();
                settle (100);
            }
            expect (visibleTiles == RemapEditor::getShapeNames().size() && clashes == 0 && applied == "Invert",
                    "the remap dock lists its quick shapes beside the curve, and a click applies one (" + juce::String (visibleTiles)
                        + " tiles, " + juce::String (clashes) + " clashes, " + applied + ")");
        }
        editor.showPage ("MAIN");
        settle (200);
    }
}
