// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class SeqPage : public juce::Component,
                private juce::Timer
{
public:
    // The page is shown in two places: its step LFOs and MSEG under MOD,
    // its note generators (arp, Euclid, prob seq, generate) as SEQ.
    enum class Part { modulators, notes };

    SeqPage (IlanaSynthAudioProcessor& p, Part partIn)
        : part (partIn),
          step1 (p, 0, IlanaTheme::accent(), true),
          step2 (p, 1, juce::Colour (0xff35c8ff)),
          mseg (p),
          msegLoop (p.apvts, "mseg_loop", "LOOP"),
          msegRate (p.apvts, "mseg_rate", "RATE", msegColour(), false),
          clockDiv (p.apvts, "clock_div", "S&H CLOCK", msegColour(), false),
          processorRef (p),
          arpDisplay (p, arpColour()),
          arpOn (p.apvts, "arp_on", "ON"),
          arpMode (p.apvts, "arp_mode", "MODE"),
          arpDiv (p.apvts, "arp_div", "RATE"),
          arpOctaves (p.apvts, "arp_octaves", "OCTAVES", arpColour(), true),
          arpGate (p.apvts, "arp_gate", "GATE", arpColour(), true),
          arpChance (p.apvts, "arp_chance", "CHANCE", arpColour(), true),
          genScale (p.apvts, "gen_scale", "SCALE"),
          genRoot (p.apvts, "gen_root", "ROOT"),
          genSnap (p.apvts, "gen_snap", "SNAP PLAYED"),
          sprayOn (p.apvts, "spray_on", "ON"),
          sprayDirection (p.apvts, "spray_direction", "DIRECTION"),
          sprayStrum (p.apvts, "spray_strum", "MODE"),
          engineTabs ({ "ARP", "EUCLID", "PROB SEQ", "CLIP" }, { arpColour(), euclidColour(), pseqColour(), clipColour() }, false),
          euclidDisplay (p, euclidColour()),
          eucOn (p.apvts, "euc_on", "ON"),
          eucTarget (p.apvts, "euc_target", "TARGET"),
          eucDiv (p.apvts, "euc_div", "RATE"),
          eucSteps (p.apvts, "euc_steps", "STEPS", euclidColour(), true),
          eucHits (p.apvts, "euc_hits", "HITS", euclidColour(), true),
          eucRotate (p.apvts, "euc_rotate", "ROTATE", euclidColour(), true),
          eucGate (p.apvts, "euc_gate", "GATE", euclidColour(), true),
          pseqEditor (p, pseqColour()),
          pseqOn (p.apvts, "pseq_on", "ON"),
          pseqDiv (p.apvts, "pseq_div", "RATE"),
          pseqLength (p.apvts, "pseq_length", "LENGTH", pseqColour(), true),
          pseqGate (p.apvts, "pseq_gate", "GATE", pseqColour(), true),
          clipEditor (p, clipColour()),
          clipOn (p.apvts, "clip_on", "ON"),
          clipIndex (p.apvts, "clip_index", "CLIP"),
          clipMode (p.apvts, "clip_mode", "MODE"),
          clipBars (p, "LENGTH")
    {
        sprayCount = std::make_unique<KnobControl> (p.apvts, "spray_count", "NOTES", generateColour(), true);
        sprayRange = std::make_unique<KnobControl> (p.apvts, "spray_range", "RANGE", generateColour(), true);
        spraySpread = std::make_unique<KnobControl> (p.apvts, "spray_spread", "SPREAD", generateColour(), true);
        strumTime = std::make_unique<KnobControl> (p.apvts, "spray_strum_time", "TIME", generateColour(), true);
        sprayChance = std::make_unique<KnobControl> (p.apvts, "spray_chance", "CHANCE", generateColour(), true);
        sprayVelocity = std::make_unique<KnobControl> (p.apvts, "spray_velocity", "VEL RND", generateColour(), true);
        addAll (*this, arpChance, genScale, genRoot, genSnap, sprayOn, sprayDirection,
                *sprayCount, *sprayRange, *spraySpread, *sprayChance, *sprayVelocity, sprayStrum, *strumTime);

        // The Generative card: ARP, EUCLID and PROB SEQ share one card.
        addAll (*this, engineTabs, euclidDisplay, eucOn, eucTarget, eucDiv, eucSteps, eucHits, eucRotate, eucGate,
                pseqEditor, pseqOn, pseqDiv, pseqLength, pseqGate, clipEditor, clipOn, clipIndex, clipMode, clipBars,
                clipImport);
        clipImport.setButtonText ("Import MIDI...");
        clipImport.setTooltip ("Import MIDI\nReads the first track with notes of a .mid file into the chosen clip, "
                               "replacing its notes. The clip's length becomes the file's, in whole bars.");
        clipImport.onClick = [this] { importMidiFile(); };
        engineTabs.onSelect = [this] (int) { showEngineTab(); };

        addAndMakeVisible (step1);
        addAndMakeVisible (step2);

        // Each step row can edit any of the four LFOs.
        for (int row = 0; row < 2; ++row)
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            {
                auto& button = lfoButtons[(size_t) row][(size_t) lfo];
                button.setButtonText ("LFO " + juce::String (lfo + 1));
                button.setClickingTogglesState (true);
                button.setRadioGroupId (100 + row);
                IlanaTheme::makePill (button, IlanaSynthAudioProcessor::lfoColour (lfo));
                button.setTooltip ("Edit the steps of LFO " + juce::String (lfo + 1) + " in this row");
                button.onClick = [this, row, lfo]
                {
                    if (lfoButtons[(size_t) row][(size_t) lfo].getToggleState())
                        showLfo (row, lfo);
                };
                addAndMakeVisible (button);
            }
        }

        showLfo (0, 0);
        showLfo (1, 1);

        msegLoop.showAsSwitch();
        genSnap.showAsSwitch();
        addAll (*this, mseg, msegLoop, msegRate, clockDiv,
                arpDisplay, arpOn, arpMode, arpDiv, arpOctaves, arpGate);

        // Open on whichever part of the card is switched on.
        engineTabs.setSelected (readOn ("pseq_on") ? 2 : readOn ("euc_on") ? 1 : readOn ("clip_on") ? 3 : 0, false);
        showEngineTab();

        if (part == Part::notes)
        {
            for (auto* control : std::initializer_list<juce::Component*> { &step1, &step2, &mseg, &msegLoop, &msegRate, &clockDiv })
                control->setVisible (false);

            for (auto& row : lfoButtons)
                for (auto& button : row)
                    button.setVisible (false);
        }
        else
        {
            for (auto* control : std::initializer_list<juce::Component*> {
                     &engineTabs, &euclidDisplay, &eucOn, &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate,
                     &pseqEditor, &pseqOn, &pseqDiv, &pseqLength, &pseqGate, &clipEditor, &clipOn, &clipIndex, &clipMode, &clipBars,
                     &clipImport, &arpDisplay, &arpOn, &arpMode, &arpDiv,
                     &arpOctaves, &arpGate, &arpChance, &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum,
                     sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get() })
                control->setVisible (false);
        }

        startTimerHz (8);
    }

    static juce::Colour msegColour() { return juce::Colour (0xffe0e6f0); }
    // Not modulation sources: the accent (a source's colour always means
    // that source).
    static juce::Colour arpColour() { return IlanaTheme::accent(); }
    static juce::Colour generateColour() { return IlanaTheme::accent(); }
    static juce::Colour euclidColour() { return IlanaTheme::accent(); }
    static juce::Colour pseqColour() { return IlanaTheme::accent(); }
    static juce::Colour clipColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto title = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour)
        {
            IlanaTheme::paintTag (g, { (float) area.getX() + 3.0f, (float) area.getCentreY() }, colour);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText (text, area.withTrimmedLeft (14), juce::Justification::centredLeft);
        };

        if (part == Part::modulators)
        {
            // Two step rows, A and B, each tagged in the colour of the LFO it
            // edits; the subtitle says whether that LFO plays its steps.
            const auto stepNote = [this] (int lfo)
            {
                const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
                return shape != nullptr && juce::roundToInt (shape->load()) == LfoShapes::Steps
                           ? juce::String ("LFO ") + juce::String (lfo + 1)
                           : juce::String ("LFO ") + juce::String (lfo + 1) + " isn't playing these: set its SHAPE to Steps";
            };
            const auto pillsWidth = [this] (int rowIndex)
            {
                auto left = 100000;
                for (auto& button : lfoButtons[(size_t) rowIndex])
                    if (button.isVisible())
                        left = juce::jmin (left, button.getX());
                return left < 100000 ? stepTitle1.getRight() - left + 12 : 0;
            };
            IlanaTheme::paintCardHeader (g, stepTitle1, "STEPS A", stepNote (step1.getLfoIndex()),
                                         IlanaSynthAudioProcessor::lfoColour (step1.getLfoIndex()), pillsWidth (0));
            IlanaTheme::paintCardHeader (g, stepTitle2, "STEPS B", stepNote (step2.getLfoIndex()),
                                         IlanaSynthAudioProcessor::lfoColour (step2.getLfoIndex()), pillsWidth (1));

            IlanaTheme::paintCard (g, msegCard.toFloat(), 7.0f, msegColour().withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, msegCard.reduced (12, 0).removeFromTop (26), "MSEG",
                                         "drag points; drag the MSEG chip onto a knob to use it", msegColour(), 0);
            return;
        }

        const auto tab = engineTabs.getSelected();
        const auto tabColour = tab == 1 ? euclidColour() : tab == 2 ? pseqColour() : tab == 3 ? clipColour() : arpColour();
        IlanaTheme::paintCard (g, arpCard.toFloat(), 7.0f, tabColour.withAlpha (0.35f));
        IlanaTheme::paintCard (g, generateCard.toFloat(), 7.0f, generateColour().withAlpha (0.35f));
        IlanaTheme::paintCardHeader (g, generateCard.reduced (12, 0).removeFromTop (26), "GENERATE",
                                     "snap to a scale, spray and strum the notes", generateColour(), 0);

        {
            // The subtitle says what the tab shown does right now.
            const auto arpOnNow = readOn ("arp_on"), seqOnNow = readOn ("pseq_on"), euclidOnNow = readOn ("euc_on");
            const auto clipOnNow = readOn ("clip_on");
            juce::String hint;

            if (tab == 0)
                hint = seqOnNow && arpOnNow ? "the probability sequencer is playing instead" : "hold notes to play the pattern";
            else if (tab == 1)
                hint = (int) readValue ("euc_target") == 0 ? (arpOnNow || seqOnNow ? "rests the steps between hits" : "plays the held chord on each hit")
                     : (int) readValue ("euc_target") == 1 ? "re-strikes Physical strings on each hit"
                                                           : "drives the Trance Gate effect (add it in FX)";
            else if (tab == 2)
                hint = seqOnNow && arpOnNow ? "takes over from the arp while on" : "hold notes: each step rolls its chance";
            else
                hint = (int) readValue ("clip_mode") == 0 ? "hold a key: C3 plays the clip as written, other keys transpose it"
                                                          : "plays in sync with the host transport";

            if ((tab == 1 && ! euclidOnNow) || (tab == 3 && ! clipOnNow))
                hint = "switch it on (top right) to use it";

            IlanaTheme::paintCardHeader (g, arpCard.reduced (12, 0).removeFromTop (26), "GENERATIVE", hint, tabColour,
                                         arpCard.getRight() - engineTabs.getX() + 80);
        }
        // Generate's group headings: the label, then a hairline to the end
        // of the group.
        for (const auto& [area, text] : { std::pair<juce::Rectangle<int>, const char*> { scaleDivider, "SCALE" },
                                          std::pair<juce::Rectangle<int>, const char*> { strumDivider, "STRUM" },
                                          std::pair<juce::Rectangle<int>, const char*> { sprayDivider, "NOTE SPRAY" } })
        {
            if (area.isEmpty())
                continue;

            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = juce::GlyphArrangement::getStringWidthInt (font, text);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (font);
            g.drawText (text, area, juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (area.getX() + width + 10, area.getCentreY(), juce::jmax (0, area.getWidth() - width - 16), 1);
        }


    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        // Each row's LFO buttons at the right of its header, like a card's tabs.
        const auto layoutPicker = [this] (int rowIndex, juce::Rectangle<int> header)
        {
            auto& buttons = lfoButtons[(size_t) rowIndex];
            auto count = 0;

            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                count += buttons[(size_t) lfo].isVisible() ? 1 : 0;

            const auto width = juce::jmin (60, header.getWidth() / 2 / juce::jmax (1, count));
            auto strip = header.removeFromRight (width * count).reduced (0, 3);

            for (auto& button : buttons)
                if (button.isVisible())
                    button.setBounds (strip.removeFromLeft (width).reduced (2, 0));
        };

        // MOD: the two step rows and the MSEG. SEQ: arp and generate side
        // by side.
        if (part == Part::notes)
        {
            layoutNotes (area);
            return;
        }

        const auto msegHeight = juce::jlimit (170, 260, area.getHeight() * 2 / 5);
        const auto stepHeight = (area.getHeight() - msegHeight - 16) / 2;

        auto row = area.removeFromTop (stepHeight);
        stepTitle1 = row.removeFromTop (26).withTrimmedLeft (12);
        layoutPicker (0, stepTitle1);
        step1.setBounds (row);

        area.removeFromTop (8);
        row = area.removeFromTop (stepHeight);
        stepTitle2 = row.removeFromTop (26).withTrimmedLeft (12);
        layoutPicker (1, stepTitle2);
        step2.setBounds (row);

        area.removeFromTop (8);
        msegCard = area;

        auto msegArea = msegCard.reduced (10, 0);
        msegArea.removeFromTop (26);
        msegArea.removeFromBottom (8);
        auto msegControls = msegArea.removeFromRight (juce::jmin (180, msegArea.getWidth() / 3));
        mseg.setBounds (msegArea.reduced (0, 2));
        msegLoop.setBounds (msegControls.removeFromTop (40).reduced (8, 4));
        layoutRow (msegControls, { &msegRate, &clockDiv });
    }

    void layoutNotes (juce::Rectangle<int> right)
    {
        // The Generative card (arp, Euclid, probability sequencer) above
        // generate, both full width.
        // Generate gets the height for two rows of full-size knobs (as big
        // as the arp's), the pattern display above takes the rest.
        constexpr int knobRowHeight = 13 + 58 + 16 + 6;
        const auto generateHeight = juce::jlimit (160, (right.getHeight() - 8) / 2, 26 + knobRowHeight * 2 + 18 + 12);
        generateCard = right.removeFromBottom (generateHeight);
        right.removeFromBottom (8);
        arpCard = right;

        auto arpArea = arpCard.reduced (10, 0);
        auto header = arpArea.removeFromTop (26);
        // The shown engine's on switch in the header's switch place, its tabs
        // just left of it.
        const auto engineSwitch = IlanaTheme::cardSwitchBounds (arpCard, header.getCentreY());
        for (auto* toggle : { &arpOn, &eucOn, &pseqOn, &clipOn })
            toggle->setBounds (engineSwitch);
        header.setRight (engineSwitch.getX() - 8);
        engineTabs.setBounds (header.removeFromRight (engineTabs.getIdealWidth()).reduced (0, 4));
        engineHint = arpCard.reduced (12, 0).withHeight (26);
        arpArea.removeFromBottom (6);
        const auto display = arpArea.removeFromTop (juce::jmax (36, arpArea.getHeight() - knobRowHeight - 8)).reduced (0, 2);
        arpArea.removeFromTop (8);
        arpDisplay.setBounds (display);
        euclidDisplay.setBounds (display);
        pseqEditor.setBounds (display);
        clipEditor.setBounds (display);

        // Every engine's row on one six-column grid, packed from the left,
        // and the same grid runs through Generate below.
        layoutRow (arpArea, { &arpMode, &arpDiv, &arpOctaves, &arpGate, &arpChance, nullptr });
        layoutRow (arpArea, { &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate });
        layoutRow (arpArea, { &pseqDiv, &pseqLength, &pseqGate, nullptr, nullptr, nullptr });
        layoutRow (arpArea, { &clipIndex, &clipMode, nullptr, nullptr, nullptr, nullptr });
        // LENGTH and the import button continue the row on its grid.
        {
            const auto column = arpArea.getWidth() / 6;
            clipBars.setBounds (clipMode.getBounds().translated (column, 0));
            clipImport.setBounds (clipMode.getBounds().translated (column * 2, 0).withTrimmedTop (13));
        }

        // Generate: three groups side by side, each under its own heading:
        // SCALE (two columns), STRUM (one) and NOTE SPRAY (three, its
        // switch on the heading's line at the card switch place), rows of
        // full-size controls on the shared grid.
        auto generate = generateCard.reduced (10, 0);
        generate.removeFromTop (26);
        generate.removeFromBottom (6);

        const auto column = generate.getWidth() / 6;
        const auto rowHeight = juce::jmin (knobRowHeight, (generate.getHeight() - 22 - 6) / 2);
        auto block = generate.withSizeKeepingCentre (generate.getWidth(), juce::jmin (generate.getHeight(), 22 + rowHeight * 2 + 6));
        const auto headings = block.removeFromTop (22);
        const auto first = block.removeFromTop (rowHeight);
        block.removeFromTop (6);
        const auto second = block.removeFromTop (rowHeight);

        scaleDivider = headings.withWidth (column * 2).reduced (3, 0);
        strumDivider = headings.withTrimmedLeft (column * 2).withWidth (column).reduced (3, 0);
        sprayDivider = headings.withTrimmedLeft (column * 3).reduced (3, 0);
        // The spray's switch in the card's header, at the card switch place.
        sprayOn.setBounds (IlanaTheme::cardSwitchBounds (generateCard, generateCard.getY() + 13));

        layoutRow (first, { &genScale, &genRoot, &sprayStrum, &sprayDirection, sprayCount.get(), sprayRange.get() });
        layoutRow (second, { nullptr, nullptr, strumTime.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get() });
        // SNAP PLAYED centred under the SCALE group, level with the dials.
        {
            const auto dialDrop = juce::jlimit (28, 58, column - 6) / 2 - 12;
            const auto rowBand = second.withSizeKeepingCentre (second.getWidth(), juce::jmin (second.getHeight(), preferredControlHeight (strumTime.get(), column - 6) + 6));
            genSnap.setBounds (rowBand.withWidth (column * 2).withSizeKeepingCentre (column, rowBand.getHeight()).reduced (3).withTrimmedTop (dialDrop));
        }
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            return;

        // Bring the LFOs that are actually set to Steps into the two rows.
        std::vector<int> stepLfos;

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape"))
                if ((int) shape->load() == 7)
                    stepLfos.push_back (lfo);

        const auto shown = [this] (int lfo) { return step1.getLfoIndex() == lfo || step2.getLfoIndex() == lfo; };

        for (const auto lfo : stepLfos)
        {
            if (shown (lfo))
                continue;

            // Replace a row that is not showing a Steps LFO, preferring the second.
            const auto rowIsSteps = [&stepLfos] (int index)
            {
                return std::find (stepLfos.begin(), stepLfos.end(), index) != stepLfos.end();
            };

            if (! rowIsSteps (step2.getLfoIndex()))
                showLfo (1, lfo);
            else if (! rowIsSteps (step1.getLfoIndex()))
                showLfo (0, lfo);
        }
    }

private:
    void showLfo (int row, int lfo)
    {
        auto& editor = row == 0 ? step1 : step2;
        auto& other = row == 0 ? step2 : step1;

        // Both rows showing the same LFO would just duplicate it; swap instead.
        if (other.getLfoIndex() == lfo && editor.getLfoIndex() != lfo)
        {
            const auto previous = editor.getLfoIndex();
            other.setLfoIndex (previous);
            lfoButtons[row == 0 ? 1 : 0][(size_t) previous].setToggleState (true, juce::dontSendNotification);
        }

        editor.setLfoIndex (lfo);
        lfoButtons[(size_t) row][(size_t) lfo].setToggleState (true, juce::dontSendNotification);
        repaint (stepTitle1.getUnion (stepTitle2)); // the titles name the LFOs
    }

    // Arp controls step back while the arp is off.
    void timerCallback() override
    {
        // The step-row pickers list the patch's LFOs (and whatever either row
        // shows), the same list in both rows.
        auto pickersChanged = false;
        for (int row = 0; row < (part == Part::modulators ? 2 : 0); ++row)
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            {
                auto& button = lfoButtons[(size_t) row][(size_t) lfo];
                const auto shown = processorRef.isLfoShown (lfo) || step1.getLfoIndex() == lfo || step2.getLfoIndex() == lfo;
                if (button.isVisible() != shown)
                {
                    button.setVisible (shown);
                    pickersChanged = true;
                }
            }
        if (pickersChanged)
            resized();

        const auto* on = processorRef.apvts.getRawParameterValue ("arp_on");
        const auto alpha = on != nullptr && on->load() > 0.5f ? 1.0f : 0.45f;

        for (juce::Component* control : { static_cast<juce::Component*> (&arpMode), static_cast<juce::Component*> (&arpDiv),
                                          static_cast<juce::Component*> (&arpOctaves), static_cast<juce::Component*> (&arpGate),
                                          static_cast<juce::Component*> (&arpChance) })
            if (control->getAlpha() != alpha)
                control->setAlpha (alpha);

        const auto dim = [] (std::initializer_list<juce::Component*> controls, bool on)
        {
            for (auto* control : controls)
                if (control->getAlpha() != (on ? 1.0f : 0.45f))
                    control->setAlpha (on ? 1.0f : 0.45f);
        };

        dim ({ &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate }, readOn ("euc_on"));
        dim ({ &pseqDiv, &pseqLength, &pseqGate }, readOn ("pseq_on"));
        dim ({ &clipIndex, &clipMode, &clipBars, &clipImport }, readOn ("clip_on"));
        clipBars.refresh();
        dim ({ strumTime.get() }, (int) readValue ("spray_strum") != 0);
        repaint (engineHint);
        repaint (stepTitle1); // their notes follow the LFOs' shapes
        repaint (stepTitle2);

        const auto* spray = processorRef.apvts.getRawParameterValue ("spray_on");
        const auto sprayAlpha = spray != nullptr && spray->load() > 0.5f ? 1.0f : 0.45f;

        for (juce::Component* control : { static_cast<juce::Component*> (&sprayDirection), static_cast<juce::Component*> (sprayCount.get()),
                                          static_cast<juce::Component*> (sprayRange.get()), static_cast<juce::Component*> (spraySpread.get()),
                                          static_cast<juce::Component*> (sprayChance.get()), static_cast<juce::Component*> (sprayVelocity.get()) })
            if (control->getAlpha() != sprayAlpha)
                control->setAlpha (sprayAlpha);
    }

    float readValue (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool readOn (const char* id) const { return readValue (id) > 0.5f; }

    void showEngineTab()
    {
        if (part == Part::modulators)
            return;

        const auto tab = engineTabs.getSelected();

        for (auto* control : std::initializer_list<juce::Component*> { &arpDisplay, &arpOn, &arpMode, &arpDiv, &arpOctaves,
                                                                        &arpGate, &arpChance })
            control->setVisible (tab == 0);

        for (auto* control : std::initializer_list<juce::Component*> { &euclidDisplay, &eucOn, &eucTarget, &eucDiv, &eucSteps,
                                                                        &eucHits, &eucRotate, &eucGate })
            control->setVisible (tab == 1);

        for (auto* control : std::initializer_list<juce::Component*> { &pseqEditor, &pseqOn, &pseqDiv, &pseqLength, &pseqGate })
            control->setVisible (tab == 2);

        for (auto* control : std::initializer_list<juce::Component*> { &clipEditor, &clipOn, &clipIndex, &clipMode, &clipBars,
                                                                        &clipImport })
            control->setVisible (tab == 3);

        repaint();
    }

    // Reads a .mid file into the chosen clip (IMPORT MIDI).
    void importMidiFile()
    {
        clipChooser = std::make_unique<juce::FileChooser> ("Import MIDI into the clip",
                                                           juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                           "*.mid;*.midi");
        juce::Component::SafePointer<SeqPage> safeThis (this);

        clipChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (safeThis == nullptr || ! file.existsAsFile())
                                          return;

                                      Clip imported;
                                      juce::String error;

                                      if (! ClipState::importMidi (file, imported, error))
                                      {
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", error);
                                          return;
                                      }

                                      auto& processor = safeThis->processorRef;
                                      const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) safeThis->readValue ("clip_index"));
                                      processor.getClipState().setClip (index, std::move (imported));
                                      processor.clipsEdited();
                                      safeThis->clipEditor.reload (true);
                                      safeThis->clipBars.refresh();
                                  });
    }

    Part part;
    StepEditor step1, step2;
    MsegEditor mseg;
    ToggleControl msegLoop;
    KnobControl msegRate;
    KnobControl clockDiv;
    IlanaSynthAudioProcessor& processorRef;
    ArpDisplay arpDisplay;
    ToggleControl arpOn;
    ComboControl arpMode, arpDiv;
    KnobControl arpOctaves, arpGate, arpChance;
    ComboControl genScale, genRoot;
    ToggleControl genSnap, sprayOn;
    ComboControl sprayDirection, sprayStrum;
    std::unique_ptr<KnobControl> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity, strumTime;
    CardTabs engineTabs;
    EuclidDisplay euclidDisplay;
    ToggleControl eucOn;
    ComboControl eucTarget, eucDiv;
    KnobControl eucSteps, eucHits, eucRotate, eucGate;
    ProbSeqEditor pseqEditor;
    ToggleControl pseqOn;
    ComboControl pseqDiv;
    KnobControl pseqLength, pseqGate;
    ClipEditor clipEditor;
    ToggleControl clipOn;
    ComboControl clipIndex, clipMode;
    ClipBarsControl clipBars;
    juce::TextButton clipImport;
    std::unique_ptr<juce::FileChooser> clipChooser;
    juce::Rectangle<int> engineHint;
    juce::Rectangle<int> sprayDivider, scaleDivider, strumDivider;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, msegCard, arpCard, generateCard;
};
} // namespace
