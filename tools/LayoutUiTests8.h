// UI review 8, package R5 (PLAY / OSC / PHYSICAL / VECTOR / FILTER / FX
// layout): checks for its fixes, each layout fix with a geometric check.
// Included by Snapshot.cpp after its helpers (findAll, findChild, settle,
// expect, visibleInTree); runLayoutReview8Tests runs from runUiTests.
#pragma once

void runLayoutReview8Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (400);
    };
    const auto knobFor = [&editor] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                return knob;
        return nullptr;
    };
    const auto toggleFor = [&editor, &processor] (const juce::String& id) -> ToggleControl*
    {
        const auto name = processor.apvts.getParameter (id)->getName (64);
        std::vector<ToggleControl*> toggles;
        findAll<ToggleControl> (editor, toggles);
        for (auto* toggle : toggles)
            if (visibleInTree (toggle) && ! toggle->getBounds().isEmpty() && toggle->getButton().getTooltip().startsWith (name))
                return toggle;
        return nullptr;
    };
    const auto area = [&editor] (juce::Component* c)
    {
        return c == nullptr ? juce::Rectangle<int>() : editor.getLocalArea (c, c->getLocalBounds());
    };
    const auto buttonNamed = [&editor] (const juce::String& text) -> juce::Button*
    {
        std::vector<juce::Button*> buttons;
        findAll<juce::Button> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && ! button->getBounds().isEmpty() && button->getButtonText() == text)
                return button;
        return nullptr;
    };
    // No two of these overlap, and each label fits its control's width.
    const auto overlaps = [&] (const std::vector<juce::Component*>& controls)
    {
        juce::String found;
        for (size_t i = 0; i < controls.size(); ++i)
            for (size_t j = i + 1; j < controls.size(); ++j)
                if (controls[i] != nullptr && controls[j] != nullptr && area (controls[i]).intersects (area (controls[j])))
                    found << area (controls[i]).toString() << " / " << area (controls[j]).toString() << "  ";
        return found;
    };

    // PLAY (S8-24, S8-8, V8-17): a strip's height doesn't change when an
    // oscillator is added; a switched-off oscillator folds to a slim strip
    // with its switch and nothing else; the strips stay tall enough that a
    // knob's rings clear its value.
    loadNamed ("Neuro Wobble");
    editor.showPage ("MAIN");
    settle (400);
    {
        const juce::String osc3 (OscillatorIds::prefixes[2]);
        auto* level1 = knobFor ("osc1_level");
        auto* level2 = knobFor ("osc2_level");
        const auto pitch = area (level2).getY() - area (level1).getY();
        const auto folded = knobFor (osc3 + "_level") == nullptr && knobFor (osc3 + "_frame") == nullptr && toggleFor (osc3 + "_on") != nullptr;
        expect (folded, "PLAY: switched-off OSC 3 folds to its switch (no knobs or menus)");
        auto* add = buttonNamed ("+  ADD OSC 4");
        const auto addGap = add != nullptr && level2 != nullptr ? area (add).getY() - area (level2).getBottom() : -1;
        expect (add != nullptr && addGap > 0 && addGap < 140,
                "PLAY: the folded OSC 3 is a slim strip between OSC 2 and + ADD OSC 4 (" + juce::String (addGap) + " px)");
        expect (level1 != nullptr && level1->getHeight() >= 52,
                "PLAY: a strip's knobs keep their full height, rings clear of the value (" + juce::String (level1 != nullptr ? level1->getHeight() : 0) + " px)");

        if (add != nullptr)
            add->triggerClick();
        settle (400);
        auto* level1After = knobFor ("osc1_level");
        auto* level2After = knobFor ("osc2_level");
        const auto pitchAfter = area (level2After).getY() - area (level1After).getY();
        expect (pitch > 0 && pitchAfter > 0 && pitchAfter <= pitch && level1After != nullptr && level1After->getHeight() <= level1->getHeight(),
                "PLAY: adding OSC 4 shares the column, so the strips never grow (V9-1: " + juce::String (pitch) + " / " + juce::String (pitchAfter) + " px)");
        processor.performEdit ("Remove OSC 4", [&processor] { processor.removeOscillator (3); });
        settle (300);

        // PLAY's picture is the cycle whatever view OSC last chose (S8-34).
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (editor, waves);
        auto compactSpec = 0, compactChecked = 0;
        for (auto* wave : waves)
            if (wave->isCompact())
            {
                wave->setViewMode (2);
                ++compactChecked;
                compactSpec += wave->getViewMode() != 0 ? 1 : 0;
                wave->setViewMode (0);
            }
        expect (compactChecked > 0 && compactSpec == 0, "PLAY: the strips' pictures stay on the cycle in SPEC view");
    }

    // A DX7 voice (six strips): the same strip height as Neuro's three, the
    // column scrolling instead of squeezing them.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (300);
        const auto neuroHeight = knobFor ("osc1_semi") != nullptr ? knobFor ("osc1_semi")->getHeight() : -1;
        const auto neuroWidth = knobFor ("osc1_level") != nullptr ? knobFor ("osc1_level")->getWidth() : -1;
        loadNamed ("E.PIANO 1 (ROM1A)");
        editor.showPage ("MAIN");
        settle (400);
        auto* trim = knobFor ("osc1_eg_out"); // (an operator's one level on PLAY)
        expect (trim != nullptr && trim->getHeight() <= neuroHeight && trim->getHeight() >= 48,
                "PLAY: six operator strips share the column, no scrolling, knobs still readable (V9-2: " + juce::String (trim != nullptr ? trim->getHeight() : 0) + " / "
                    + juce::String (neuroHeight) + " px)");
        // The scroll bar comes out of the picture: the knobs keep their
        // width, so "-30.9 dB" fits under LEVEL.
        expect (trim != nullptr && trim->getWidth() >= neuroWidth - 1,
                "PLAY: a scrolling column keeps the knobs' width (" + juce::String (trim != nullptr ? trim->getWidth() : 0) + " / "
                    + juce::String (neuroWidth) + " px)");
    }

    // FILTER (V8-9): Init's two open filters, both at 20 kHz: on FILTER
    // their markers stack on their own cutoff line, clearly apart; on PLAY's
    // short graph they stand side by side with a gap. V8-39: a filter that
    // passes everything dims its card, all but CUTOFF.
    loadNamed ("Init");
    editor.showPage ("FILTER");
    settle (400);
    {
        FilterDisplay* display = nullptr;
        std::vector<FilterDisplay*> displays;
        findAll<FilterDisplay> (editor, displays);
        for (auto* candidate : displays)
            if (visibleInTree (candidate))
                display = candidate;
        if (display != nullptr)
        {
            const auto m = display->getMarkerCentres();
            expect (std::abs (m[0].x - m[1].x) < 1.0f && std::abs (m[0].y - m[1].y) >= 18.0f,
                    "FILTER: Init's markers stack on the 20 kHz line, apart (" + m[0].toString() + " / " + m[1].toString() + ")");
        }
        else
            expect (false, "FILTER shows the response graph");

        expect (FilterDisplay::isPassThrough (processor, 1), "Init's Filter 2 (Low Pass at 20 kHz) passes everything");
        auto* reso = knobFor ("f2_reso");
        auto* cutoff = knobFor ("f2_cutoff");
        expect (reso != nullptr && cutoff != nullptr && reso->getAlpha() < 0.7f && cutoff->getAlpha() > 0.99f,
                "FILTER: an open Filter 2 dims its card but not CUTOFF");
        setParam ("f2_cutoff", 800.0f);
        settle (400);
        expect (! FilterDisplay::isPassThrough (processor, 1) && reso != nullptr && reso->getAlpha() > 0.99f,
                "FILTER: Filter 2 at 800 Hz is no longer dimmed");
        setParam ("f2_cutoff", 20000.0f);
        setParam ("west_on", 1.0f);
        settle (400);
        auto* fold = knobFor ("west_fold");
        expect (fold != nullptr && fold->getSlider().findColour (juce::Slider::rotarySliderFillColourId) == FilterColours::west(),
                "WEST's knobs wear WEST's lime (I8-22)");
        setParam ("west_on", 0.0f);

        editor.showPage ("MAIN");
        settle (300);
        displays.clear();
        findAll<FilterDisplay> (editor, displays);
        for (auto* candidate : displays)
            if (visibleInTree (candidate))
            {
                const auto m = candidate->getMarkerCentres();
                expect (m[0].getDistanceFrom (m[1]) >= 22.0f, "PLAY: the open filters' markers stand apart (" + juce::String (m[0].getDistanceFrom (m[1])) + " px)");
            }
    }

    // SIGNAL FLOW names (V8-27): no abbreviations; strings and soundboard
    // on two lines in a block tall enough for them.
    {
        loadNamed ("Felt Hammer Board");
        setParam ("sym_on", 1.0f);
        editor.showPage ("FILTER");
        settle (400);
        if (auto* flow = findChild<SignalFlow> (editor); flow != nullptr)
        {
            juce::String post;
            juce::Rectangle<float> box;
            for (const auto& [name, bounds] : flow->getBlocks())
                if (name == "PHYSICAL")
                {
                    post = name;
                    box = bounds;
                }
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto widest = juce::GlyphArrangement::getStringWidth (font, "PHYSICAL");
            expect (post == "PHYSICAL" && box.getHeight() >= 14.0f && box.getWidth() - 8.0f >= widest,
                    "SIGNAL FLOW: strings and soundboard together are one PHYSICAL node, never a joined name that wraps (I11-8; " + box.toString() + ")");
        }
        setParam ("sym_on", 0.0f);
    }

    // PHYSICAL (V8-23, V8-6): BODY and SOUNDBOARD each with a switch and
    // their main controls, none overlapping; the empty state at the page's
    // margins.
    {
        loadNamed ("Felt Hammer Board");
        editor.showPage ("PHYSICAL");
        settle (400);
        setParam ("res_on", 1.0f);
        setParam ("sb_on", 1.0f);
        settle (300);
        auto* bodyOn = toggleFor ("res_on");
        auto* boardOn = toggleFor ("sb_on");
        auto* amount = knobFor ("res_amount");
        auto* mix = knobFor ("sb_mix");
        auto* bodyLink = buttonNamed (juce::CharPointer_UTF8 ("EDIT BODY \xe2\x80\xba"));
        auto* boardLink = buttonNamed (juce::CharPointer_UTF8 ("EDIT SOUNDBOARD \xe2\x80\xba"));
        const auto clash = overlaps ({ bodyOn, boardOn, amount, knobFor ("res_decay"), mix, bodyLink, boardLink });
        expect (bodyOn != nullptr && boardOn != nullptr && amount != nullptr && mix != nullptr && bodyLink != nullptr && boardLink != nullptr
                    && clash.isEmpty(),
                "PHYSICAL: BODY and SOUNDBOARD have their switch, main controls and a link, none overlapping " + clash);
        auto* page = editor.getCurrentPage();
        if (bodyOn != nullptr && page != nullptr)
        {
            const auto wasOn = processor.apvts.getRawParameterValue ("res_on")->load();
            const auto boardWas = processor.apvts.getRawParameterValue ("sb_on")->load();
            setParam ("res_on", 0.0f);
            setParam ("sb_on", 0.0f);
            settle (300);
            expect (amount != nullptr && ! amount->isVisible() && mix != nullptr && ! mix->isVisible(),
                    "PHYSICAL: BODY and SOUNDBOARD off fold to their switches (V10-9)");
            setParam ("res_on", wasOn);
            setParam ("sb_on", boardWas);
        }

        setParam ("osc1_mode", 0.0f);
        settle (500);
        auto* view = page != nullptr ? findChild<PhysicalView> (*page) : nullptr;
        auto* makePhysical = buttonNamed ("SWITCH TO PHYSICAL");
        expect (view != nullptr && page != nullptr && view->getWidth() > page->getWidth() - 60 && view->getAlpha() < 0.6f && makePhysical != nullptr,
                "PHYSICAL: not physical, the dimmed preview spans the page at its margins");
        setParam ("osc1_mode", 1.0f);
        settle (300);
    }

    // VECTOR (V8-26, S8-38): the pad right under the header; the controls
    // column as tall as the pad (no empty foot), nothing overlapping.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("VECTOR");
        settle (400);
        auto* pad = findChild<VectorPadDisplay> (editor);
        auto* page = editor.getCurrentPage();
        auto* wander = knobFor ("vec_drift");
        if (pad != nullptr && page != nullptr && wander != nullptr)
        {
            const auto padArea = area (pad);
            const auto pageTop = area (page).getY();
            expect (padArea.getY() - pageTop <= 12 + 30 + 2, "VECTOR: the pad starts under the header (" + juce::String (padArea.getY() - pageTop) + " px)");
            expect (padArea.getBottom() - area (wander).getBottom() < 130,
                    "VECTOR: the knob rows reach the pad's foot (" + juce::String (padArea.getBottom() - area (wander).getBottom()) + " px)");
            const auto clash = overlaps ({ knobFor ("vec_x"), knobFor ("vec_y"), knobFor ("vec_rate"), wander, knobFor ("vec_drift_rate"), pad });
            expect (clash.isEmpty(), "VECTOR: no two controls overlap " + clash);
        }
        else
            expect (false, "VECTOR shows its pad and WANDER");
    }

    // FX (S8-6, V8-38, S8-14, V8-19, S8-40, I8-33).
    {
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
        const auto soloRights = [&]
        {
            std::vector<int> rights;
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (editor, buttons);
            for (auto* button : buttons)
                if (visibleInTree (button) && ! button->getBounds().isEmpty() && button->getButtonText() == "SOLO")
                    rights.push_back (area (button).getRight());
            std::sort (rights.begin(), rights.end());
            return rights;
        };

        loadNamed ("Init");
        editor.showPage ("FX");
        loadFx ({ 18, 2 }); // Tilt (no MIX of its own), Drive
        auto* tiltMix = knobFor ("fx_slot1_mix");
        expect (tiltMix != nullptr && tiltMix->getLabelText() == "MIX" && knobFor ("fx_slot2_mix") == nullptr,
                "FX: an effect without its own MIX gets the slot's MIX as a knob in its row; Drive keeps its own");

        loadFx ({ 20, 2, 20 }); // OTT, Drive, a duplicate OTT
        expect (knobFor ("fx_slot3_mix") == nullptr, "FX: the duplicate card has no slot MIX (V8-38)");

        loadFx ({ 7, 13 }); // Chorus (fits half), Reverb (full): no lone half card
        const auto lone = soloRights();
        expect (lone.size() == 2, "FX: Chorus and Reverb both show (cards keep their own widths, V11-4)");

        loadFx ({ 7, 2, 13 });
        setParam ("fx_slot2_band", 1.0f);
        setParam ("fx_slot3_band", 3.0f);
        settle (400);
        auto* driveAmount = knobFor ("fx_drive_amount");
        auto* reverbSize = knobFor ("fx_reverb_size");
        expect (driveAmount != nullptr && reverbSize != nullptr && soloRights().size() == 3,
                "FX: a split group lays out its cards");
        setParam ("fx_slot2_band", 0.0f);
        setParam ("fx_slot3_band", 0.0f);

        // The toolbar in one group at the left.
        auto* copy = buttonNamed ("COPY TO 2");
        auto* file = buttonNamed ("SAVE / LOAD CHAIN");
        expect (copy != nullptr && file != nullptr && area (file).getX() - area (copy).getRight() < 200,
                "FX: the dice and SAVE / LOAD CHAIN sit with the chain buttons (S8-40)");

        // The all-in-one Airwindows module isn't offered to a new rack.
        loadFx ({ 2 });
        if (auto* add = buttonNamed ("+  ADD EFFECT"); add != nullptr)
        {
            add->triggerClick();
            settle (80); // (a call-out closes itself soon under xvfb)
            FxLibraryView* callout = nullptr;
            std::vector<FxLibraryView*> views;
            findAll<FxLibraryView> (editor, views);
            for (auto* view : views)
                if (view->getName() == "FX LIBRARY" && visibleInTree (view))
                    callout = view;
            auto* all = callout != nullptr ? callout->findButton (30) : nullptr;
            expect (all != nullptr && ! all->isVisible(), "FX: + ADD EFFECT doesn't offer AIRWINDOWS (ALL) to a rack without it (I8-33)");
            if (callout != nullptr)
                if (auto* box = callout->findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            settle (300);
        }
        else
            expect (false, "FX: + ADD EFFECT is on the page");
        loadNamed ("Init");
    }
}
