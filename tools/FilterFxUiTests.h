// UI review 7, package Q5 (FILTER and FX): checks for its fixes. Included
// by Snapshot.cpp after its helpers (findAll, findChild, settle, expect,
// visibleInTree); runFilterFxTests runs from runUiTests.
#pragma once

#include <set>

void runFilterFxTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (300);
    };
    const auto shownKnob = [&editor] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                return knob;
        return nullptr;
    };
    const auto shownCombo = [&editor, &processor] (const juce::String& id) -> ComboControl*
    {
        const auto name = processor.apvts.getParameter (id)->getName (64);
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getComboBox().getTooltip().startsWith (name + "\n"))
                return combo;
        return nullptr;
    };
    const auto shownButtons = [&editor] (const juce::String& text)
    {
        std::vector<juce::Button*> buttons, matching;
        findAll<juce::Button> (editor, buttons);
        for (auto* button : buttons)
            if (button->getButtonText() == text && visibleInTree (button) && ! button->getBounds().isEmpty())
                matching.push_back (button);
        return matching;
    };

    // I7-1, S7-1, I7-32, V7-35: SIGNAL FLOW keeps F1 and F2 at a usable
    // width with a soundboard, strings, WEST and BODY on, serial or
    // parallel, with a bypass, and with WEST in Filter 2's place; no two
    // blocks overlap and none leaves the view or runs under BALANCE.
    {
        loadNamed ("Felt Hammer Board");
        editor.showPage ("FILTER");
        settle (400);
        SignalFlow* flow = nullptr;
        std::vector<SignalFlow*> flows;
        findAll<SignalFlow> (editor, flows);
        for (auto* candidate : flows)
            if (visibleInTree (candidate))
                flow = candidate;
        // (BALANCE is in the strip too: the one inside the enlarged flow card is the one checked here.)
        KnobControl* balance = nullptr;
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == "filter_balance" && visibleInTree (knob) && flow != nullptr && flow->getParentComponent() != nullptr
                    && flow->getParentComponent()->isParentOf (knob))
                    balance = knob;
        }
        expect (flow != nullptr && balance != nullptr, "FILTER shows SIGNAL FLOW and BALANCE");

        const auto check = [&] (const juce::String& what)
        {
            if (flow == nullptr || balance == nullptr)
                return;
            const auto bounds = flow->getLocalBounds().toFloat();
            const auto filters = flow->getFilterBlocks();
            const auto blocks = flow->getBlocks();
            const auto corner = flow->getLocalArea (balance->getParentComponent(), balance->getBounds()).toFloat();
            juce::String problems;
            for (const auto& filter : filters)
                if (filter.getWidth() < 30.0f || ! bounds.contains (filter))
                    problems << "filter " << filter.toString() << "; ";
            for (size_t i = 0; i < blocks.size(); ++i)
            {
                if (! bounds.contains (blocks[i].second) || blocks[i].second.intersects (corner))
                    problems << blocks[i].first << " out of place; ";
                for (size_t j = i + 1; j < blocks.size(); ++j)
                    if (blocks[i].second.intersects (blocks[j].second))
                        problems << blocks[i].first << " / " << blocks[j].first << "; ";
            }
            expect (problems.isEmpty(), what + ": SIGNAL FLOW keeps F1 and F2 (30 px or more), no block overlaps another or BALANCE "
                                            + problems);
        };

        check ("Felt Hammer Board");
        for (const auto* id : { "sym_on", "sb_on", "west_on", "res_on" })
            setParam (id, 1.0f);
        settle (200);
        check ("WEST, BODY, strings and soundboard on");
        setParam ("filters_parallel", 1.0f);
        setParam ("osc1_route", 3.0f);
        settle (200);
        check ("the same in parallel, OSC 1 bypassing the filters");
        setParam ("west_pos", 1.0f);
        settle (200);
        check ("WEST in Filter 2's place");
        if (flow != nullptr)
        {
            juce::StringArray names;
            for (const auto& block : flow->getBlocks())
                names.add (block.first);
            expect (names.contains ("WEST") && ! names.contains ("F2"), "WEST in Filter 2's place is drawn there, F2 isn't ("
                                                                           + names.joinIntoString (", ") + ")");
        }

        // While WEST and BODY are off they are stubs, not stages of the chain.
        setParam ("west_on", 0.0f);
        setParam ("res_on", 0.0f);
        settle (200);
        if (flow != nullptr)
        {
            juce::StringArray names;
            for (const auto& block : flow->getBlocks())
                names.add (block.first);
            expect (names.contains ("WEST (off)") && names.contains ("BODY (off)") && names.contains ("F2"),
                    "switched-off WEST and BODY are stubs off the chain, and Filter 2 runs again (" + names.joinIntoString (", ") + ")");
        }
        check ("WEST and BODY off");
        loadNamed ("Felt Hammer Board");
    }

    // V7-34, S7-23: a switched-off module draws every control at the off
    // alpha; S7-24: BODY's menus sit on WEST's menu row.
    {
        loadNamed ("Init");
        editor.showPage ("FILTER");
        setParam ("west_on", 0.0f);
        setParam ("res_on", 0.0f);
        settle (400);
        const auto offAlpha = FilterColours::offAlpha + 0.01f;
        auto* fold = shownKnob ("west_fold");
        auto* amount = shownKnob ("res_amount");
        auto* decay = shownKnob ("res_decay");
        (void) offAlpha;
        const auto offDrawn = fold != nullptr && amount != nullptr && decay != nullptr
                              && fold->getAlpha() < 0.99f && amount->getAlpha() < 0.99f && decay->getAlpha() < 0.99f; // open, dimmed in place (the design)
        setParam ("west_on", 1.0f);
        setParam ("res_on", 1.0f);
        settle (400);
        fold = shownKnob ("west_fold");
        amount = shownKnob ("res_amount");
        decay = shownKnob ("res_decay");
        expect (offDrawn && fold != nullptr && amount != nullptr && decay != nullptr
                    && fold->getAlpha() > 0.99f && amount->getAlpha() > 0.99f && decay->getAlpha() > 0.99f,
                "WEST and BODY stay open and dimmed while off, and draw every knob in full while on");

        auto* place = shownCombo ("west_pos");
        auto* bodyType = shownCombo ("body_type");
        auto* westFold = shownKnob ("west_fold");
        auto* bodyAmount = shownKnob ("res_amount");
        expect (place != nullptr && bodyType != nullptr && westFold != nullptr && bodyAmount != nullptr
                    && editor.getLocalArea (place->getParentComponent(), place->getBounds()).getY()
                           == editor.getLocalArea (bodyType->getParentComponent(), bodyType->getBounds()).getY()
                    && editor.getLocalArea (westFold->getParentComponent(), westFold->getBounds()).getY()
                           == editor.getLocalArea (bodyAmount->getParentComponent(), bodyAmount->getBounds()).getY(),
                "BODY's menus and knobs line up with WEST's, row for row");
        expect (place != nullptr && place->getComboBox().getItemText (1) == "Replaces F2",
                "WEST's PLACE reads \"Replaces F2\" (fits the menu's font)");
        setParam ("west_on", 0.0f);
        setParam ("res_on", 0.0f);
    }

    // V7-37, I7-40, V7-18 / S7-11: BALANCE reads a value; PLAY's TYPE
    // names a filter as FILTER does; Init's two open filters' markers sit
    // clearly apart.
    {
        expect (describeValue ("filter_balance", 0.0f) == "Even", "BALANCE at the centre reads \"Even\", not \"F1 = F2\"");
        expect (FilterTypes::shortNames()[FilterType::LowPass] == "Low Pass" && FilterTypes::shortNames()[FilterType::AwZLow] == "Smooth LP"
                    && FilterTypes::shortNames().size() == FilterType::Count,
                "PLAY's filter TYPE uses FILTER's names (Low Pass, Smooth LP)");
        loadNamed ("Init");
        editor.showPage ("FILTER");
        settle (300);
        if (auto* display = findChild<FilterDisplay> (editor); display != nullptr)
        {
            const auto markers = display->getMarkerCentres();
            expect (markers[0].getDistanceFrom (markers[1]) >= 18.0f,
                    "Init: the two filter markers stand clearly apart (" + juce::String (markers[0].getDistanceFrom (markers[1])) + " px)");
        }
    }

    // The FX page.
    const auto loadFx = [&] (std::initializer_list<int> types)
    {
        auto slot = 1;
        for (auto type : types)
            processor.assignFxSlot (slot++, type);
        for (; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            processor.assignFxSlot (slot, 0);
        for (int s = 1; s <= IlanaSynthAudioProcessor::numFxSlots; ++s)
        {
            setParam ("fx_slot" + juce::String (s) + "_mix", 1.0f);
            setParam ("fx_slot" + juce::String (s) + "_band", 0.0f);
        }
        settle (400);
    };
    // (The slot's dry / wet: a MIX knob in the card's row since UI review
    // 8, S8-6.)
    const auto shownBlends = [&editor]
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        return (int) std::count_if (knobs.begin(), knobs.end(), [] (KnobControl* k)
                                    { return k->getParameterId().startsWith ("fx_slot") && k->getParameterId().endsWith ("_mix")
                                             && visibleInTree (k) && ! k->getBounds().isEmpty(); });
    };
    const auto shownBands = [&editor]
    {
        std::vector<juce::ComboBox*> boxes;
        findAll<juce::ComboBox> (editor, boxes);
        return (int) std::count_if (boxes.begin(), boxes.end(), [] (juce::ComboBox* b)
                                    { return visibleInTree (b) && ! b->getBounds().isEmpty() && b->getTooltip().startsWith ("Band:"); });
    };

    {
        loadNamed ("Init");
        editor.showPage ("FX");
        loadFx ({ 27, 2, 20 }); // Vowel, Drive, OTT: each has its own MIX

        // V7-7, S7-15, I7-38: one dry/wet per card; the band menu only on a
        // banded slot.
        expect (shownBlends() == 0 && shownBands() == 0,
                "cards with their own MIX show no BLEND, and full-band cards no band menu ("
                    + juce::String (shownBlends()) + " / " + juce::String (shownBands()) + ")");
        setParam ("fx_slot1_mix", 0.6f);
        settle (300);
        const auto blendBelowFull = shownBlends();
        setParam ("fx_slot1_mix", 1.0f);
        loadFx ({ 18, 2 }); // Tilt has no MIX
        expect (blendBelowFull == 1 && shownBlends() == 1, "BLEND shows on an effect without a MIX, and once it is set below 100 %");
        loadFx ({ 7, 2, 13 });
        setParam ("fx_slot2_band", 1.0f);
        setParam ("fx_slot3_band", 3.0f);
        settle (300);
        expect (shownBands() == 2, "the two banded slots show their band menus (" + juce::String (shownBands()) + ")");

        // S7-16: one + ADD EFFECT (the tile), not one in the toolbar too.
        loadFx ({ 7, 13 }); // Chorus (half), Reverb (full)
        std::vector<FxTypeButton*> titles;
        findAll<FxTypeButton> (editor, titles);
        auto chorusRight = 0, reverbRight = 0;
        for (auto* title : titles)
        {
            if (! visibleInTree (title))
                continue;
            for (auto* knob : { shownKnob ("fx_chorus_mix"), shownKnob ("fx_reverb_mix") })
                if (knob != nullptr && knob->getParentComponent() == title->getParentComponent())
                    (knob->getParameterId() == "fx_chorus_mix" ? chorusRight : reverbRight) = knob->getRight();
        }
        auto* stack = titles.empty() ? nullptr : titles.front()->getParentComponent();
        // (UI review 11, V11-4: one width per effect type, whatever sits next to
        // it: the displays keep their widths when the order changes.)
        const auto displayWidths = [&editor]
        {
            std::vector<FxDisplay*> displays;
            findAll<FxDisplay> (editor, displays);
            std::vector<int> widths;
            for (auto* d : displays)
                if (visibleInTree (d))
                    widths.push_back (d->getWidth());
            std::sort (widths.begin(), widths.end());
            return widths;
        };
        const auto chorusReverb = displayWidths();
        loadFx ({ 13, 7 });
        // (V13-1 supersedes the fixed width: a card fills its row, so a lone card
        // grows to the full rack; what stays is a readable picture either way.)
        const auto swapped = displayWidths();
        expect (stack != nullptr && chorusReverb.size() == 2 && swapped.size() == 2 && chorusReverb.front() >= 150 && swapped.front() >= 150,
                "an effect's picture stays readable wherever it sits in the chain (Chorus, Reverb in either order: " + juce::String (chorusReverb.front())
                    + " and " + juce::String (swapped.empty() ? 0 : swapped.front()) + " px at the least)");
        loadFx ({ 7, 13 });
        expect (shownButtons ("+  ADD EFFECT").size() == 1, "the rack has one + ADD EFFECT (the tile after the cards)");

        // V7-29, S7-16: OUTPUT follows the last card instead of the page's foot.
        loadFx ({ 27, 2, 20 });
        juce::Component* softClip = nullptr;
        std::vector<ToggleControl*> toggles;
        findAll<ToggleControl> (editor, toggles);
        for (auto* toggle : toggles)
            if (visibleInTree (toggle) && toggle->getTooltip().startsWith (processor.apvts.getParameter ("master_clip")->getName (64)))
                softClip = toggle;
        expect (softClip != nullptr && softClip->getParentComponent() != nullptr
                    && softClip->getBottom() <= softClip->getParentComponent()->getHeight(),
                "OUTPUT sits inside the page (the rows grow into spare height, V9-3)");

        // V7-22, V7-43: CHAIN 1 / 2 (not a second A/B), the header's dice for the chain.
        std::vector<DiceFxButton*> dice;
        findAll<DiceFxButton> (editor, dice);
        expect (shownButtons ("CHAIN 1").size() == 1 && shownButtons ("CHAIN 2").size() == 1 && shownButtons ("RACK A").empty()
                    && dice.size() == 1 && dice.front()->getButtonText() == "RANDOMISE FX" && dice.front()->getTooltip().startsWith ("Randomise FX"),
                "the FX toolbar has CHAIN 1 / CHAIN 2 and a RANDOMISE FX button (I14-9)");

        // V7-42: the dice never puts one effect in two slots.
        auto duplicates = 0;
        for (int roll = 0; roll < 30; ++roll)
        {
            processor.randomizeFxChain();
            std::set<int> seen;
            for (int s = 1; s <= IlanaSynthAudioProcessor::numFxSlots; ++s)
                if (const auto type = (int) processor.apvts.getRawParameterValue ("fx_slot" + juce::String (s))->load(); type > 0)
                    duplicates += seen.insert (type).second ? 0 : 1;
        }
        expect (duplicates == 0, "30 rolls of the FX dice put no effect in two slots (" + juce::String (duplicates) + ")");

        // I7-39: FX knobs wear their family's colour.
        loadFx ({ 13, 2 });
        auto* size = shownKnob ("fx_reverb_size");
        auto* amount = shownKnob ("fx_drive_amount");
        expect (size != nullptr && amount != nullptr
                    && size->getSlider().findColour (juce::Slider::rotarySliderFillColourId) == fxColour (13)
                    && amount->getSlider().findColour (juce::Slider::rotarySliderFillColourId) == fxColour (2),
                "FX knobs take their family's colour (reverb blue, drive orange)");

        // I7-28: the reverb card switches to its Airwindows model in place,
        // titled REVERB either way; the library offers the model beside it.
        std::vector<FxModelSwitch*> switches;
        findAll<FxModelSwitch> (editor, switches);
        FxModelSwitch* reverbSwitch = nullptr;
        for (auto* candidate : switches)
            if (visibleInTree (candidate) && reverbSwitch == nullptr)
                reverbSwitch = candidate;
        expect (reverbSwitch != nullptr, "the reverb card has a BUILT-IN / AIRWINDOWS model switch");
        if (reverbSwitch != nullptr && reverbSwitch->onSwitch != nullptr)
        {
            reverbSwitch->onSwitch();
            settle (400);
            const auto type = (int) processor.apvts.getRawParameterValue ("fx_slot1")->load();
            juce::String title;
            titles.clear();
            findAll<FxTypeButton> (editor, titles);
            for (auto* button : titles)
                if (visibleInTree (button) && button->getTooltip().startsWith ("Slot 1:"))
                    title = button->getTooltip();
            expect (type == 34 && title.contains ("Spaces"), "the model switch puts Airwindows' reverb (Spaces) in the same slot");
        }
        FxLibraryView library ([] (int) { return -1; }, [] (int) {});
        auto* aw = library.findButton (34);
        auto* more = library.findButton (30);
        expect (aw != nullptr && aw->getKind() == FxLibraryButton::Kind::airwindowsModel && aw->getTooltip().contains ("Reverb")
                    && more != nullptr && more->getKind() == FxLibraryButton::Kind::more && fxTwinOf (13) == 34 && fxTwinOf (34) == 13,
                "the library offers Spaces as Reverb's Airwindows model and the all-in-one module as More Airwindows");

        // I7-28: the all-in-one Airwindows card has a display.
        loadFx ({ 30 });
        std::vector<FxDisplay*> displays;
        findAll<FxDisplay> (editor, displays);
        expect (std::count_if (displays.begin(), displays.end(), [] (FxDisplay* d) { return visibleInTree (d); }) == 1,
                "the all-in-one Airwindows card shows its algorithm's picture");
        loadFx ({});
    }
    loadNamed ("Init");
}
