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
          arpLanes (p, arpColour()),
          arpMode (p.apvts, "arp_mode", "MODE"),
          arpDiv (p.apvts, "arp_div", "RATE"),
          arpOctaves (p.apvts, "arp_octaves", "OCTAVES", arpColour(), true),
          arpGate (p.apvts, "arp_gate", "GATE", arpColour(), true),
          arpChance (p.apvts, "arp_chance", "CHANCE", arpColour(), true),
          // GENERATE's scale is SNAP TO KEY's KEY (review 6, I6-30): not the
          // tuning's Scala scale.
          genScale (p.apvts, "gen_scale", "KEY"),
          genRoot (p.apvts, "gen_root", "ROOT"),
          genSnap (p.apvts, "gen_snap", "SNAP PLAYED"),
          sprayOn (p.apvts, "spray_on", "ON"),
          // Each in its own box (STRUM, SPRAY), so both are DIRECTION: the
          // strum's order, the spray's side of the played note.
          sprayDirection (p.apvts, "spray_direction", "DIRECTION"),
          sprayStrum (p.apvts, "spray_strum", "DIRECTION"),
          engineTabs ({ "ARP", "EUCLID", "PROB SEQ", "CLIP" }, { arpColour(), euclidColour(), pseqColour(), clipColour() }, false),
          euclidDisplay (p, euclidColour()),
          eucTarget (p.apvts, "euc_target", "TARGET"),
          eucDiv (p.apvts, "euc_div", "RATE"),
          eucSteps (p.apvts, "euc_steps", "STEPS", euclidColour(), true),
          eucHits (p.apvts, "euc_hits", "HITS", euclidColour(), true),
          eucRotate (p.apvts, "euc_rotate", "ROTATE", euclidColour(), true),
          eucGate (p.apvts, "euc_gate", "GATE", euclidColour(), true),
          pseqEditor (p, pseqColour()),
          pseqDiv (p.apvts, "pseq_div", "RATE"),
          // STEPS, as the arp's (review 7, I7-29).
          pseqLength (p.apvts, "pseq_length", "STEPS", pseqColour(), true),
          pseqGate (p.apvts, "pseq_gate", "GATE", pseqColour(), true),
          clipEditor (p, clipColour()),
          clipIndex (p.apvts, "clip_index", "SLOT"),
          clipMode (p.apvts, "clip_mode", "MODE"),
          clipBars (p, "LENGTH"),
          clipGrid (clipEditor, "GRID"),
          arpSteps (p.apvts, "arp_steps", "STEPS", arpColour(), true),
          noteChain (p, IlanaTheme::accent()),
          scaleSwitch (p, "gen_scale", 1, "SNAP TO KEY"),
          strumSwitch (p, "spray_strum", 1, "STRUM"),
          clipZoom (clipEditor, "ZOOM")
    {
        sprayCount = std::make_unique<KnobControl> (p.apvts, "spray_count", "NOTES", generateColour(), true);
        sprayRange = std::make_unique<KnobControl> (p.apvts, "spray_range", "RANGE", generateColour(), true);
        spraySpread = std::make_unique<KnobControl> (p.apvts, "spray_spread", "SPREAD", generateColour(), true);
        strumTime = std::make_unique<KnobControl> (p.apvts, "spray_strum_time", "TIME", generateColour(), true);
        // Not a second bare CHANCE on the page (the arp has one).
        sprayChance = std::make_unique<KnobControl> (p.apvts, "spray_chance", "SPRAY %", generateColour(), true);
        sprayVelocity = std::make_unique<KnobControl> (p.apvts, "spray_velocity", "VEL RND", generateColour(), true);
        addAll (*this, arpChance, genScale, genRoot, genSnap, sprayOn, sprayDirection,
                *sprayCount, *sprayRange, *spraySpread, *sprayChance, *sprayVelocity, sprayStrum, *strumTime);

        // The Generative card: ARP, EUCLID and PROB SEQ share one card.
        addAll (*this, engineTabs, euclidDisplay, eucTarget, eucDiv, eucSteps, eucHits, eucRotate, eucGate,
                pseqEditor, pseqDiv, pseqLength, pseqGate, clipEditor, clipIndex, clipMode, clipBars,
                clipGrid, clipImport, clipExport, clipDraw);
        // One gate for SNAP TO KEY and STRUM: their switches do "off", so
        // the menus beside them list only what is on (review 9, S9-9).
        for (auto* control : { &genScale, &sprayStrum })
        {
            juce::Component::SafePointer<juce::ComboBox> box (&control->getComboBox());
            control->setPopupOverride ([box]
            {
                if (box == nullptr)
                    return;

                juce::PopupMenu menu;
                for (int i = 1; i < box->getNumItems(); ++i)
                    menu.addItem (box->getItemId (i), box->getItemText (i), true, box->getSelectedItemIndex() == i);
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (box.getComponent()).withMinimumWidth (box->getWidth()),
                                    [box] (int id)
                                    {
                                        if (id > 0 && box != nullptr)
                                            box->setSelectedId (id, juce::sendNotificationSync);
                                    });
            });
        }
        clipImport.setButtonText ("IMPORT MIDI");
        clipImport.setTooltip ("Import MIDI\nReads the first track with notes of a .mid file into the chosen clip, "
                               "replacing its notes. The clip's length becomes the file's, in whole bars.");
        clipImport.onClick = [this] { importMidiFile(); };
        clipExport.setButtonText ("EXPORT MIDI");
        clipExport.setTooltip ("Export MIDI\nWrites the chosen clip as a .mid file (the clip's own length and notes, 120 BPM), "
                               "to drag into a track in your DAW.");
        clipExport.onClick = [this] { exportMidiFile(); };
        clipQuantise.setButtonText ("QUANTISE");
        clipQuantise.setTooltip ("Quantise\nMoves the selected notes' starts (every note's, with none selected) to the nearest "
                                 "GRID line, in one undo step. Q in the roll does the same.");
        clipQuantise.onClick = [this] { clipEditor.quantise(); };
        clipExpand.setTooltip ("Expand\nGives the piano roll the page: GENERATE folds to its title line until you collapse "
                               "the roll again (or click GENERATE's title).");
        clipExpand.onClick = [this] { setClipExpanded (! clipExpanded); };
        clipDraw.setTooltip ("Draw (D)\nA click on empty space places a note (at the last length used); a drag paints a run "
                             "of them along the GRID, at the pointer's pitch. Alt+drag stretches the placed note instead. "
                             "Off: double-click places a note and a drag selects.");
        clipDraw.setClickingTogglesState (true);
        clipDraw.onClick = [this] { clipEditor.setDrawMode (clipDraw.getToggleState()); };
        clipEditor.onDrawModeChanged = [this] (bool on) { clipDraw.setToggleState (on, juce::dontSendNotification); };
        clipDraw.setToggleState (clipEditor.isDrawMode(), juce::dontSendNotification);
        addAll (*this, arpSteps, noteChain, scaleSwitch, strumSwitch, clipZoom, clipQuantise, clipExpand);
        noteChain.onOpenEngine = [this] (int engine) { engineTabs.setSelected (engine, true); };
        engineTabs.onSelect = [this] (int) { showEngineTab(); };
        // Each engine's power is the switch in its tab (review 7, V7-11 and
        // I7-36): the tab's fill says which engine is shown, the switch
        // whether it plays. No second switch in the engine's row.
        engineTabs.onToggle = [this] (int engine) { toggleEngine (engine); };

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
                arpLanes, arpMode, arpDiv, arpOctaves, arpGate);

        // Open on whichever part of the card is switched on.
        engineTabs.setSelected (readOn ("pseq_on") ? 2 : readOn ("euc_on") ? 1 : readOn ("clip_on") ? 3 : 0, false);
        updateEngineSwitches();
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
                     &engineTabs, &euclidDisplay, &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate,
                     &pseqEditor, &pseqDiv, &pseqLength, &pseqGate, &clipEditor, &clipIndex, &clipMode, &clipBars,
                     &clipGrid, &clipImport, &clipExport, &clipDraw, &arpLanes, &arpMode, &arpDiv,
                     &arpOctaves, &arpGate, &arpChance, &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum,
                     sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get(),
                     &arpSteps, &noteChain, &scaleSwitch, &strumSwitch, &clipZoom, &clipQuantise, &clipExpand })
                control->setVisible (false);
        }

        // What doesn't act right now dims (one rule for every page: UI review
        // 4, V26): the arp's settings while it is off or PROB SEQ plays
        // instead (its lanes step back by themselves); each engine's settings
        // while it is off; the spray's while SPRAY is off; ROOT and SNAP
        // PLAYED while there is no key (ROOT still tunes the sympathetic
        // strings). The clip's editing tools stay lit: they edit the clip
        // whether it plays or not.
        const auto arpPlays = [this] { return readOn ("arp_on") && ! readOn ("pseq_on"); };
        for (juce::Component* control : { static_cast<juce::Component*> (&arpMode), static_cast<juce::Component*> (&arpDiv),
                                          static_cast<juce::Component*> (&arpOctaves), static_cast<juce::Component*> (&arpGate),
                                          static_cast<juce::Component*> (&arpChance), static_cast<juce::Component*> (&arpSteps) })
            effectRules.add (*control, arpPlays, "the ARP isn't playing (it is off, or PROB SEQ plays instead)");
        for (juce::Component* control : { static_cast<juce::Component*> (&eucTarget), static_cast<juce::Component*> (&eucDiv),
                                          static_cast<juce::Component*> (&eucSteps), static_cast<juce::Component*> (&eucHits),
                                          static_cast<juce::Component*> (&eucRotate), static_cast<juce::Component*> (&eucGate) })
            effectRules.add (*control, effectRules.isOn ("euc_on"), "EUCLID is off");
        for (juce::Component* control : { static_cast<juce::Component*> (&pseqDiv), static_cast<juce::Component*> (&pseqLength),
                                          static_cast<juce::Component*> (&pseqGate) })
            effectRules.add (*control, effectRules.isOn ("pseq_on"), "the sequencer is off");
        effectRules.add (clipMode, effectRules.isOn ("clip_on"), "CLIP is off");
        effectRules.add (*strumTime, effectRules.choiceIsNot ("spray_strum", 0), "STRUM is off");
        for (juce::Component* control : { static_cast<juce::Component*> (&sprayDirection), static_cast<juce::Component*> (sprayCount.get()),
                                          static_cast<juce::Component*> (sprayRange.get()), static_cast<juce::Component*> (spraySpread.get()),
                                          static_cast<juce::Component*> (sprayChance.get()), static_cast<juce::Component*> (sprayVelocity.get()) })
            effectRules.add (*control, effectRules.isOn ("spray_on"), "SPRAY is off");
        const auto hasScale = effectRules.choiceIsNot ("gen_scale", 0);
        const auto tunesStrings = [this] { return readOn ("sym_on") && ! readOn ("sym_manual"); };
        effectRules.add (genSnap, hasScale, "SNAP TO KEY is off");
        effectRules.add (genRoot, [hasScale, tunesStrings] { return hasScale() || tunesStrings(); }, "SNAP TO KEY is off");

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

        if (clipEditor.isVisible() && ! clipDivider.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (clipDivider);
        }

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

        // PATTERN's header: the title, then the note path (NoteChainView),
        // then the engines' tabs, each with its engine's switch.
        IlanaTheme::paintCardTitle (g, arpCard.reduced (12, 0).removeFromTop (26), "PATTERN", tabColour);

        const auto summary = generateSummary();

        if (generateFolded())
        {
            IlanaTheme::paintCardHeader (g, generateCard.reduced (12, 0).removeFromTop (generateCard.getHeight()), "GENERATE",
                                         summary + "   (folded while the roll is expanded: click to show)", generateColour(), 12);
            return;
        }

        IlanaTheme::paintCardHeader (g, generateCard.reduced (12, 0).removeFromTop (26), "GENERATE",
                                     "shapes the keys you play before the pattern plays them", generateColour(), 12);

        // Three boxes, each a part with its own switch: its title, a quiet
        // note on what it does now, the switch at the right.
        const auto tuningOn = readOn ("tuning_on");
        const struct { juce::Rectangle<int> box; const char* title; bool on; juce::String note; } boxes[] {
            { snapBox, "SNAP TO KEY", scaleSwitch.isOn(),
              ! scaleSwitch.isOn() ? "off" : tuningOn ? "snaps in 12-TET steps" : readOn ("gen_snap") ? "snaps every note" : "snaps generated notes" },
            { strumBox, "STRUM", strumSwitch.isOn(), strumSwitch.isOn() ? "spreads chords" : "off" },
            { sprayBox, "SPRAY", readOn ("spray_on"), readOn ("spray_on") ? "throws extra notes" : "off" }
        };

        for (const auto& part : boxes)
        {
            if (part.box.isEmpty())
                continue;

            IlanaTheme::paintRecessedPanel (g, part.box.toFloat(), 5.0f);
            auto header = part.box.reduced (10, 0).removeFromTop (boxHeaderHeight);
            paintSubBoxTitle (g, header, part.title, part.note, part.on, 44); // (44: the switch)
        }
    }

    // GENERATE in one line, for its folded title.
    juce::String generateSummary() const
    {
        const auto choiceText = [this] (const char* id)
        {
            if (auto* parameter = processorRef.apvts.getParameter (id))
                return parameter->getCurrentValueAsText();
            return juce::String();
        };
        // Only the parts that are on: no "off" in text away from a switch
        // (review 8, I8-20).
        juce::StringArray parts;
        if (scaleSwitch.isOn())
            parts.add ("SNAP TO KEY " + choiceText ("gen_root") + " " + choiceText ("gen_scale"));
        if (strumSwitch.isOn())
            parts.add ("STRUM " + choiceText ("spray_strum"));
        if (readOn ("spray_on"))
            parts.add ("SPRAY");
        return parts.isEmpty() ? juce::String ("nothing shapes the keys")
                               : parts.joinIntoString (juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")));
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        // A folded GENERATE opens again from its title line.
        if (generateFolded() && generateCard.contains (event.getPosition()))
            setClipExpanded (false);
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
        // PATTERN (the note engines) above GENERATE, both full width.
        // GENERATE takes one row of full-size knobs in its three boxes;
        // PATTERN the rest, and while the clip roll is expanded GENERATE
        // folds to its title line and the roll takes the page.
        constexpr int knobRowHeight = 13 + 58 + 16 + 6;
        const auto folded = generateFolded();
        const auto generateHeight = folded ? 34 : 26 + boxHeaderHeight + (13 + 48 + 16) + 16;
        generateCard = right.removeFromBottom (generateHeight);
        right.removeFromBottom (8);
        arpCard = right;

        auto arpArea = arpCard.reduced (10, 0);
        auto header = arpArea.removeFromTop (26);
        // The tabs at the header's right (the switches are in the engines'
        // rows), the note path between them and the title.
        header.removeFromRight (2);
        engineTabs.setBounds (header.removeFromRight (engineTabs.getIdealWidth()).reduced (0, 4));
        noteChain.setBounds (header.withTrimmedLeft (IlanaTheme::cardTitleWidth ("PATTERN") - 4).withTrimmedRight (12).reduced (0, 3));

        arpArea.removeFromBottom (6);
        const auto controls = arpArea.removeFromBottom (knobRowHeight);
        arpArea.removeFromBottom (6);
        const auto display = arpArea.reduced (0, 2);
        arpLanes.setBounds (display);
        euclidDisplay.setBounds (display);
        pseqEditor.setBounds (display);

        // Every engine's row on one grid of fixed cells, packed from the left
        // (the switches are in the tabs): the shared controls first, in one
        // order (RATE, STEPS, GATE), then the engine's own; menus and knobs
        // on one label line (review 8, I8-23, S8-33, V8-22). A cell is as
        // wide for a menu as for a knob, and a row with fewer controls ends
        // sooner instead of spreading over the card (V10-5, S10-9).
        const auto packed = [&controls] (int count) { return controls.withWidth (juce::jmin (controls.getWidth(), count * seqCellWidth)); };
        // (A knob sits at its cell's left edge, so its label and a menu's start
        // on the same grid line: V11-5.)
        const auto leftAligned = [] (juce::Rectangle<int> row, const std::vector<juce::Component*>& items)
        {
            layoutRow (row, items, true);
            for (auto* item : items)
                if (dynamic_cast<KnobControl*> (item) != nullptr)
                    item->setBounds (item->getBounds().withWidth (juce::jmin (item->getWidth(), seqKnobWidth)));
        };
        leftAligned (packed (6), { &arpDiv, &arpSteps, &arpGate, &arpMode, &arpOctaves, &arpChance });
        leftAligned (packed (6), { &eucDiv, &eucSteps, &eucGate, &eucTarget, &eucHits, &eucRotate });
        leftAligned (packed (3), { &pseqDiv, &pseqLength, &pseqGate });

        // The clip's row is menus and buttons only (ten columns): the
        // piano roll takes the height the other engines' knobs need.
        {
            constexpr int menuHeight = 13 + 24;
            const auto clipRow = controls.withTrimmedTop (controls.getHeight() - menuHeight - 6);
            clipEditor.setBounds (display.withBottom (clipRow.getY() - 6));
            layoutRow (clipRow, { &clipIndex, &clipMode, &clipBars, &clipGrid, &clipZoom, nullptr, nullptr, nullptr, nullptr, nullptr });

            // The buttons line up with the menus' boxes.
            const auto column = clipRow.getWidth() / 10;
            auto cell = clipZoom.getBounds().translated (column, 0).withTrimmedTop (13).withHeight (24);
            clipDraw.setBounds (cell);
            clipQuantise.setBounds (cell.translated (column, 0));
            clipImport.setBounds (cell.translated (column * 2, 0));
            clipExport.setBounds (cell.translated (column * 3, 0));
            clipExpand.setBounds (cell.translated (column * 4, 0));
            // The roll's own tools (DRAW, QUANTISE) apart from the file and
            // page buttons (review 11, S11-5).
            clipDivider = juce::Rectangle<int> (clipImport.getX() - 5, cell.getY() + 2, 1, cell.getHeight() - 4);
        }

        layoutGenerate();
    }

    // GENERATE: three boxes side by side, SNAP TO KEY (ROOT, KEY, SNAP
    // PLAYED), STRUM (DIRECTION, TIME) and SPRAY (DIRECTION and five knobs),
    // each with its switch on its own title line and its controls inside.
    // Every name in the row on one line; menus get a little more width.
    void layoutGenerate()
    {
        const auto folded = generateFolded();

        for (auto* control : generateControls())
            control->setVisible (! folded && part == Part::notes);

        if (folded)
        {
            snapBox = strumBox = sprayBox = {};
            return;
        }

        auto area = generateCard.reduced (10, 0);
        area.removeFromTop (26);
        area.removeFromBottom (8);

        struct Item { juce::Component* control; float weight; };
        const std::vector<std::vector<Item>> groups {
            { { &genRoot, 0.8f }, { &genScale, 1.45f }, { &genSnap, 1.0f } },
            { { &sprayStrum, 1.05f }, { strumTime.get(), 0.9f } },
            { { &sprayDirection, 1.05f }, { sprayCount.get(), 1.0f }, { sprayRange.get(), 1.0f },
              { spraySpread.get(), 1.0f }, { sprayChance.get(), 1.0f }, { sprayVelocity.get(), 1.0f } }
        };
        constexpr int gap = 8, padding = 6;
        auto total = 0.0f;
        for (const auto& group : groups)
            for (const auto& item : group)
                total += item.weight;

        // (Capped, so the boxes stay a group at the left rather than spreading over the card.)
        const auto unit = juce::jmin (118.0f, (float) (area.getWidth() - gap * 2 - padding * 2 * 3) / total);
        std::array<juce::Rectangle<int>*, 3> boxes { &snapBox, &strumBox, &sprayBox };
        std::array<juce::Component*, 3> switches { &scaleSwitch, &strumSwitch, &sprayOn };
        auto x = (float) area.getX();

        for (size_t g = 0; g < groups.size(); ++g)
        {
            auto weight = 0.0f;
            for (const auto& item : groups[g])
                weight += item.weight;

            const auto width = weight * unit + (float) padding * 2.0f;
            auto& box = *boxes[g];
            box = juce::Rectangle<int> (juce::roundToInt (x), area.getY(), juce::roundToInt (width), area.getHeight());
            x += width + (float) gap;

            // The switch closes the box's title line (a bare switch: the
            // title names it).
            const auto title = box.withHeight (boxHeaderHeight);
            switches[g]->setBounds (juce::Rectangle<int> (title.getRight() - 8 - 40, title.getCentreY() - 23, 40, 13 + 20));

            // The controls: names on one line at the band's top.
            auto row = box.reduced (padding, 0).withTrimmedTop (boxHeaderHeight).withTrimmedBottom (4);
            auto left = (float) row.getX();
            for (const auto& item : groups[g])
            {
                const auto cell = juce::Rectangle<int> (juce::roundToInt (left), row.getY(), juce::roundToInt (item.weight * unit),
                                                        row.getHeight()).reduced (3, 0);
                left += item.weight * unit;
                const auto preferred = preferredControlHeight (item.control, cell.getWidth());
                auto bounds = cell.withHeight (preferred > 0 ? juce::jmin (cell.getHeight(), preferred) : cell.getHeight());
                // (Every box's names on the common top line, menus or knobs: V11-5.)
                item.control->setBounds (bounds);
            }
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

        // The engine tabs light while their engine is on (review 6, I6-15).
        if (part == Part::notes)
        {
            updateEngineSwitches();

            // The switch is the only "off": while it is off the menu beside it
            // reads the choice it will bring back (dimmed), not "Off" (S10-2).
            for (auto [combo, toggle] : { std::pair<ComboControl*, ChoiceSwitch*> { &genScale, &scaleSwitch }, { &sprayStrum, &strumSwitch } })
            {
                auto& box = combo->getComboBox();
                const auto text = box.getItemText (juce::jlimit (1, juce::jmax (1, box.getNumItems() - 1), toggle->getRemembered()));
                if (box.getNumItems() > 1 && box.getItemText (0) != text)
                {
                    const auto wasOff = box.getSelectedItemIndex() == 0;
                    box.changeItemText (box.getItemId (0), text);
                    if (wasOff)
                        box.setText (text, juce::dontSendNotification);
                }
            }

            // The boxes' titles and notes follow their switches.
            const auto signature = (scaleSwitch.isOn() ? 1 : 0) | (strumSwitch.isOn() ? 2 : 0) | (readOn ("spray_on") ? 4 : 0)
                                 | (readOn ("tuning_on") ? 8 : 0) | (readOn ("gen_snap") ? 1 << 20 : 0) | (juce::roundToInt (readValue ("gen_scale")) << 4)
                                 | (juce::roundToInt (readValue ("gen_root")) << 9) | (juce::roundToInt (readValue ("spray_strum")) << 14);
            if (signature != boxSignature)
            {
                boxSignature = signature;
                repaint (generateCard);
            }
        }

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
        noteChain.setShownEngine (tab);

        for (auto* control : std::initializer_list<juce::Component*> { &arpLanes, &arpMode, &arpDiv, &arpOctaves,
                                                                        &arpGate, &arpChance, &arpSteps })
            control->setVisible (tab == 0);

        for (auto* control : std::initializer_list<juce::Component*> { &euclidDisplay, &eucTarget, &eucDiv, &eucSteps,
                                                                        &eucHits, &eucRotate, &eucGate })
            control->setVisible (tab == 1);

        for (auto* control : std::initializer_list<juce::Component*> { &pseqEditor, &pseqDiv, &pseqLength, &pseqGate })
            control->setVisible (tab == 2);

        for (auto* control : std::initializer_list<juce::Component*> { &clipEditor, &clipIndex, &clipMode, &clipBars,
                                                                        &clipGrid, &clipImport, &clipExport, &clipZoom, &clipDraw, &clipQuantise, &clipExpand })
            control->setVisible (tab == 3);

        // An expanded roll folds GENERATE only while CLIP is shown.
        if (! getLocalBounds().isEmpty())
            resized();

        repaint();
    }

    // The clip roll's EXPAND: GENERATE folds to its title line (a view
    // setting, not saved).
    void setClipExpanded (bool expanded)
    {
        clipExpanded = expanded;
        clipExpand.setButtonText (expanded ? "COLLAPSE" : "EXPAND");
        resized();
        repaint();
    }

    void updateEngineSwitches()
    {
        const char* const engineSwitches[] { "arp_on", "euc_on", "pseq_on", "clip_on" };
        for (int engine = 0; engine < 4; ++engine)
            engineTabs.setTabOn (engine, readOn (engineSwitches[engine]));
    }

    // A tab's switch: the engine's on parameter, one undo step.
    void toggleEngine (int engine)
    {
        const char* const ids[] { "arp_on", "euc_on", "pseq_on", "clip_on" };
        const char* const names[] { "ARP", "EUCLID", "PROB SEQ", "CLIP" };

        if (! juce::isPositiveAndBelow (engine, 4))
            return;

        if (auto* parameter = processorRef.apvts.getParameter (ids[engine]))
        {
            const auto on = parameter->getValue() < 0.5f;
            processorRef.performEdit (juce::String (names[engine]) + (on ? " on" : " off"), [parameter, on]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
                parameter->endChangeGesture();
            });
        }

        updateEngineSwitches();
        effectRules.apply();
        repaint();
    }

public:
    // The UI test turns the engines on and off through their tabs' switches.
    CardTabs& getEngineTabs() { return engineTabs; }

private:
    bool generateFolded() const { return part == Part::notes && clipExpanded && engineTabs.getSelected() == 3; }

    std::vector<juce::Component*> generateControls()
    {
        return { &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum, sprayCount.get(), sprayRange.get(),
                 spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get(), &scaleSwitch, &strumSwitch };
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

    // Writes the chosen clip to a .mid file (EXPORT MIDI).
    void exportMidiFile()
    {
        const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) readValue ("clip_index"));
        const auto clip = processorRef.getClipState().getClip (index);

        if (clip.notes.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export MIDI", "This clip has no notes yet.");
            return;
        }

        const auto name = processorRef.getCurrentPresetName().replaceCharacters ("\\/:*?\"<>|", "---------").trim();
        clipChooser = std::make_unique<juce::FileChooser> ("Export the clip as MIDI",
                                                           juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                               .getChildFile ((name.isEmpty() ? juce::String ("ilanaSynth") : name)
                                                                              + " clip " + juce::String (index + 1) + ".mid"),
                                                           "*.mid");
        juce::Component::SafePointer<SeqPage> safeThis (this);

        clipChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safeThis, clip] (const juce::FileChooser& chooser)
                                  {
                                      auto file = chooser.getResult();

                                      if (safeThis == nullptr || file == juce::File())
                                          return;

                                      if (! file.hasFileExtension ("mid;midi"))
                                          file = file.withFileExtension ("mid");

                                      juce::String error;

                                      if (! ClipState::exportMidi (clip, file, error))
                                          juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export MIDI", error);
                                  });
    }

    Part part;
    StepEditor step1, step2;
    MsegEditor mseg;
    ToggleControl msegLoop;
    KnobControl msegRate;
    KnobControl clockDiv;
    IlanaSynthAudioProcessor& processorRef;
    ArpLanesEditor arpLanes;
    ComboControl arpMode, arpDiv;
    KnobControl arpOctaves, arpGate, arpChance;
    ComboControl genScale, genRoot;
    ToggleControl genSnap, sprayOn;
    ComboControl sprayDirection, sprayStrum;
    std::unique_ptr<KnobControl> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity, strumTime;
    CardTabs engineTabs;
    EuclidDisplay euclidDisplay;
    ComboControl eucTarget, eucDiv;
    KnobControl eucSteps, eucHits, eucRotate, eucGate;
    ProbSeqEditor pseqEditor;
    ComboControl pseqDiv;
    KnobControl pseqLength, pseqGate;
    ClipEditor clipEditor;
    ComboControl clipIndex, clipMode;
    ClipBarsControl clipBars;
    ClipGridControl clipGrid;
    juce::TextButton clipImport, clipExport;
    KnobControl arpSteps;
    NoteChainView noteChain;
    ChoiceSwitch scaleSwitch, strumSwitch;
    ClipZoomControl clipZoom;
    juce::TextButton clipQuantise, clipExpand { "EXPAND" }, clipDraw { "DRAW" };
    bool clipExpanded = false;
    juce::Rectangle<int> clipDivider;
    int boxSignature = -1;
    static constexpr int boxHeaderHeight = 24, seqCellWidth = 112, seqKnobWidth = 88;
    juce::Rectangle<int> snapBox, strumBox, sprayBox;
    std::unique_ptr<juce::FileChooser> clipChooser;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    std::array<juce::TextButton, 2> useButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, msegCard, arpCard, generateCard;
    EffectRules effectRules { processorRef };
};
} // namespace
