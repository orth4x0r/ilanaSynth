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
          // The strum's Off / Up / Down is its DIRECTION; the spray's own
          // direction (up, down or both from the played note) is its PITCH.
          sprayDirection (p.apvts, "spray_direction", "PITCH"),
          sprayStrum (p.apvts, "spray_strum", "DIRECTION"),
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
          clipBars (p, "LENGTH"),
          clipGrid (clipEditor, "GRID")
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
                clipGrid, clipImport);
        clipImport.setButtonText ("IMPORT MIDI");
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

        // A row whose LFO plays another shape offers to switch it to Steps
        // (UI review 4, S21): one click, one undo step.
        for (int row = 0; row < 2; ++row)
        {
            auto& button = useButtons[(size_t) row];
            button.onClick = [this, row] { useSteps (row); };
            addChildComponent (button);
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
                     &clipGrid, &clipImport, &arpDisplay, &arpOn, &arpMode, &arpDiv,
                     &arpOctaves, &arpGate, &arpChance, &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum,
                     sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get() })
                control->setVisible (false);
        }

        // What doesn't act right now dims (one rule for every page: UI review
        // 4, V26): the arp's card body, display included, while it is off;
        // each engine's settings while it is off; the spray's while SPRAY is
        // off; ROOT and SNAP PLAYED while there is no scale (ROOT still tunes
        // the sympathetic strings).
        const auto arpOnNow = effectRules.isOn ("arp_on");
        for (juce::Component* control : { static_cast<juce::Component*> (&arpDisplay), static_cast<juce::Component*> (&arpMode),
                                          static_cast<juce::Component*> (&arpDiv), static_cast<juce::Component*> (&arpOctaves),
                                          static_cast<juce::Component*> (&arpGate), static_cast<juce::Component*> (&arpChance) })
            effectRules.add (*control, arpOnNow, "the ARP is off");
        for (juce::Component* control : { static_cast<juce::Component*> (&eucTarget), static_cast<juce::Component*> (&eucDiv),
                                          static_cast<juce::Component*> (&eucSteps), static_cast<juce::Component*> (&eucHits),
                                          static_cast<juce::Component*> (&eucRotate), static_cast<juce::Component*> (&eucGate) })
            effectRules.add (*control, effectRules.isOn ("euc_on"), "EUCLID is off");
        for (juce::Component* control : { static_cast<juce::Component*> (&pseqDiv), static_cast<juce::Component*> (&pseqLength),
                                          static_cast<juce::Component*> (&pseqGate) })
            effectRules.add (*control, effectRules.isOn ("pseq_on"), "the sequencer is off");
        for (juce::Component* control : { static_cast<juce::Component*> (&clipIndex), static_cast<juce::Component*> (&clipMode),
                                          static_cast<juce::Component*> (&clipBars), static_cast<juce::Component*> (&clipGrid),
                                          static_cast<juce::Component*> (&clipImport) })
            effectRules.add (*control, effectRules.isOn ("clip_on"), "CLIP is off");
        effectRules.add (*strumTime, effectRules.choiceIsNot ("spray_strum", 0), "STRUM is Off");
        for (juce::Component* control : { static_cast<juce::Component*> (&sprayDirection), static_cast<juce::Component*> (sprayCount.get()),
                                          static_cast<juce::Component*> (sprayRange.get()), static_cast<juce::Component*> (spraySpread.get()),
                                          static_cast<juce::Component*> (sprayChance.get()), static_cast<juce::Component*> (sprayVelocity.get()) })
            effectRules.add (*control, effectRules.isOn ("spray_on"), "SPRAY is off");
        const auto hasScale = effectRules.choiceIsNot ("gen_scale", 0);
        const auto tunesStrings = [this] { return readOn ("sym_on") && ! readOn ("sym_manual"); };
        effectRules.add (genSnap, hasScale, "SCALE is Off");
        effectRules.add (genRoot, [hasScale, tunesStrings] { return hasScale() || tunesStrings(); }, "SCALE is Off");

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
                return playsSteps (lfo) ? juce::String ("LFO ") + juce::String (lfo + 1) + " plays these steps"
                                        : juce::String ("LFO ") + juce::String (lfo + 1) + " isn't playing these";
            };
            // The pills, and the Use button left of them while it shows.
            const auto pillsWidth = [this] (int rowIndex)
            {
                auto left = 100000;
                for (auto& button : lfoButtons[(size_t) rowIndex])
                    if (button.isVisible())
                        left = juce::jmin (left, button.getX());
                if (useButtons[(size_t) rowIndex].isVisible())
                    left = juce::jmin (left, useButtons[(size_t) rowIndex].getX());
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
                hint = juce::String ("double-click adds a note, drag selects; ")
                     + ((int) readValue ("clip_mode") == 0 ? "hold a key: C3 plays the clip as written, others transpose it"
                                                           : "plays in sync with the host transport");

            if ((tab == 1 && ! euclidOnNow) || (tab == 3 && ! clipOnNow))
                hint = "switch it on (top right) to use it";

            IlanaTheme::paintCardHeader (g, arpCard.reduced (12, 0).removeFromTop (26), "PATTERN", hint, tabColour,
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
            // NOTE SPRAY's rule stops short of its switch.
            const auto end = area == sprayDivider ? sprayOn.getX() - 8 : area.getRight() - 6;
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (font);
            g.drawText (text, area, juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (area.getX() + width + 10, area.getCentreY(), juce::jmax (0, end - area.getX() - width - 10), 1);
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

            header.removeFromRight (8);
            auto& use = useButtons[(size_t) rowIndex];
            const auto useWidth = juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::body, true),
                                                                              use.getButtonText()) + 24;
            use.setBounds (header.removeFromRight (useWidth).reduced (0, 3));
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

        // Every engine's row on one six-column grid, packed from the left,
        // and the same grid runs through Generate below.
        layoutRow (arpArea, { &arpMode, &arpDiv, &arpOctaves, &arpGate, &arpChance, nullptr });
        layoutRow (arpArea, { &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate });
        layoutRow (arpArea, { &pseqDiv, &pseqLength, &pseqGate, nullptr, nullptr, nullptr });
        // The clip's row is menus only: the piano roll takes the height the
        // other engines' knobs need.
        {
            const auto spare = juce::jmax (0, arpArea.getHeight() - (13 + 24 + 6) - 4);
            clipEditor.setBounds (display.withHeight (display.getHeight() + spare));
            layoutRow (arpArea.withTrimmedTop (spare), { &clipIndex, &clipMode, nullptr, nullptr, nullptr, nullptr });
        }
        // LENGTH and the import button continue the row on its grid.
        {
            const auto column = arpArea.getWidth() / 6;
            clipBars.setBounds (clipMode.getBounds().translated (column, 0));
            clipImport.setBounds (clipMode.getBounds().translated (column * 2, 0).withTrimmedTop (13));
            clipGrid.setBounds (clipMode.getBounds().translated (column * 3, 0));
        }

        // Generate: three groups side by side, each under its own heading
        // and rule: SCALE (two columns), STRUM (one) and NOTE SPRAY (three,
        // its switch at the end of its own rule: it switches the spray only,
        // so the card has no master switch to contradict STRUM's Off). Two
        // rows of full-size controls on the shared grid, every name in a row
        // on one line.
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
        // The spray's switch closes its heading's line (a bare switch: the
        // heading names it).
        sprayOn.setBounds (juce::Rectangle<int> (40, 13 + 20).withCentre ({ sprayDivider.getRight() - 20, sprayDivider.getCentreY() - 6 }));

        layoutRow (first, { &genScale, &genRoot, &sprayStrum, &sprayDirection, sprayCount.get(), sprayRange.get() }, true);
        layoutRow (second, { &genSnap, nullptr, strumTime.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get() }, true);
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
        updateUseButtons();
        repaint (stepTitle1.getUnion (stepTitle2)); // the titles name the LFOs
    }

    bool playsSteps (int lfo) const
    {
        const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
        return shape != nullptr && juce::roundToInt (shape->load()) == LfoShapes::Steps;
    }

    // Sets the row's LFO to the Steps shape.
    void useSteps (int row)
    {
        const auto lfo = (row == 0 ? step1 : step2).getLfoIndex();

        if (auto* shape = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_shape"))
            processorRef.performEdit ("LFO " + juce::String (lfo + 1) + " shape", [shape]
            {
                shape->beginChangeGesture();
                shape->setValueNotifyingHost (shape->convertTo0to1 ((float) LfoShapes::Steps));
                shape->endChangeGesture();
            });

        updateUseButtons();
    }

    // Each row's Use button shows while its LFO plays another shape, and
    // the steps step back until they play.
    void updateUseButtons()
    {
        if (part != Part::modulators)
            return;

        auto relayout = false;

        for (int row = 0; row < 2; ++row)
        {
            auto& editor = row == 0 ? step1 : step2;
            auto& button = useButtons[(size_t) row];
            const auto lfo = editor.getLfoIndex();
            const auto playing = playsSteps (lfo);
            const auto text = "Use on LFO " + juce::String (lfo + 1);

            if (button.getButtonText() != text)
            {
                button.setButtonText (text);
                button.setTooltip ("Set LFO " + juce::String (lfo + 1) + "'s SHAPE to Steps, so it plays this row");
                relayout = true;
            }

            if (button.isVisible() == playing)
            {
                button.setVisible (! playing);
                relayout = true;
            }

            const auto alpha = playing ? 1.0f : IlanaTheme::dimmedAlpha;

            if (editor.getAlpha() != alpha)
                editor.setAlpha (alpha);
        }

        if (relayout && ! getLocalBounds().isEmpty())
        {
            resized();
            repaint (stepTitle1.getUnion (stepTitle2));
        }
    }

public:
    // The UI test reaches the rows through these.
    juce::TextButton& getUseButton (int row) { return useButtons[(size_t) juce::jlimit (0, 1, row)]; }
    juce::Component& getStepRow (int row) { return row == 0 ? static_cast<juce::Component&> (step1) : step2; }

private:

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

        updateUseButtons();

        effectRules.apply();
        clipBars.refresh();
        repaint (engineHint);
        repaint (stepTitle1); // their notes follow the LFOs' shapes
        repaint (stepTitle2);
    }

public:
    // The UI test counts the dimmed controls.
    int getNumInactive() const { return effectRules.numInactive(); }

private:

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
                                                                        &clipGrid, &clipImport })
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
                                      processor.performEdit ("Import MIDI clip", [&]
                                      {
                                          processor.getClipState().setClip (index, std::move (imported));
                                          processor.clipsEdited();
                                      });
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
    ClipGridControl clipGrid;
    juce::TextButton clipImport;
    std::unique_ptr<juce::FileChooser> clipChooser;
    juce::Rectangle<int> engineHint;
    juce::Rectangle<int> sprayDivider, scaleDivider, strumDivider;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    std::array<juce::TextButton, 2> useButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, msegCard, arpCard, generateCard;
    EffectRules effectRules { processorRef };
};
} // namespace
