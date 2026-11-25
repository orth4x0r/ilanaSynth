#include "SeqPage.h"
#include "../ConfirmOverlay.h"

#include "PageHelpers.h"

void ClipDragButton::mouseDrag (const juce::MouseEvent& event)
{
    juce::TextButton::mouseDrag (event);

    if (dragging || event.getDistanceFromDragStart() < 6 || makeFile == nullptr)
        return;

    dragging = true;
    justDragged = true;

    if (const auto file = makeFile(); file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void ClipDragButton::mouseDown (const juce::MouseEvent& event)
{
    dragging = justDragged = false;
    juce::TextButton::mouseDown (event);
}

void ClipDragButton::mouseEnter (const juce::MouseEvent& event)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    juce::TextButton::mouseEnter (event);
}

SeqPage::SeqPage (IlanaSynthAudioProcessor& p, Part partIn)
    : part (partIn),
      step1 (p, 0, IlanaTheme::accent(), true),
      step2 (p, 1, juce::Colour (0xff35c8ff)),
      processorRef (p),
      arpLanes (p, arpColour()),
      arpMode (p.apvts, "arp_mode", "MODE"),
      arpDiv (p.apvts, "arp_div", "RATE"),
      arpOctaves (p.apvts, "arp_octaves", "OCTAVES", arpColour(), false),
      arpGate (p.apvts, "arp_gate", "GATE", SeqColours::stepLength(), false),
      arpChance (p.apvts, "arp_chance", "CHANCE", arpColour(), false),
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
      eucSteps (p.apvts, "euc_steps", "STEPS", euclidColour(), false),
      eucHits (p.apvts, "euc_hits", "HITS", euclidColour(), false),
      eucRotate (p.apvts, "euc_rotate", "ROTATE", euclidColour(), false),
      eucGate (p.apvts, "euc_gate", "GATE", euclidColour(), false),
      pseqEditor (p, pseqColour()),
      pseqDiv (p.apvts, "pseq_div", "RATE"),
      // STEPS, as the arp's (review 7, I7-29).
      pseqLength (p.apvts, "pseq_length", "STEPS", pseqColour(), false),
      pseqGate (p.apvts, "pseq_gate", "GATE", pseqColour(), false),
      clipEditor (p, clipColour()),
      clipIndex (p.apvts, "clip_index", "SLOT"),
      clipMode (p.apvts, "clip_mode", "MODE"),
      clipBars (p, "LENGTH"),
      clipGrid (clipEditor, "GRID"),
      arpSteps (p.apvts, "arp_steps", "STEPS", arpColour(), false),
      noteChain (p, IlanaTheme::accent()),
      scaleSwitch (p, "gen_scale", 1, "SNAP TO KEY"),
      strumSwitch (p, "spray_strum", 1, "STRUM"),
      clipZoom (clipEditor, "ZOOM")
{
    // (Each GENERATE box in its own colour: STRUM's and SPRAY's rings, UI review 31, item 40.)
    sprayCount = std::make_unique<KnobControl> (p.apvts, "spray_count", "NOTES", SeqColours::spray(), false);
    sprayRange = std::make_unique<KnobControl> (p.apvts, "spray_range", "RANGE", SeqColours::spray(), false);
    spraySpread = std::make_unique<KnobControl> (p.apvts, "spray_spread", "SPREAD", SeqColours::spray(), false);
    strumTime = std::make_unique<KnobControl> (p.apvts, "spray_strum_time", "TIME", SeqColours::strum(), false);
    // Not a second bare CHANCE on the page (the arp has one).
    sprayChance = std::make_unique<KnobControl> (p.apvts, "spray_chance", "CHANCE", SeqColours::spray(), false);
    sprayVelocity = std::make_unique<KnobControl> (p.apvts, "spray_velocity", "VELOCITY", SeqColours::spray(), false);
    // The boxes' switches lit in their colours, as an FX row's.
    scaleSwitch.getButton().getProperties().set ("switchColour", (int) SeqColours::snap().getARGB());
    strumSwitch.getButton().getProperties().set ("switchColour", (int) SeqColours::strum().getARGB());
    sprayOn.setSwitchColour (SeqColours::spray());
    // (NOTES and RANGE take no ring: they keep the ring knobs' spacing, so the SPRAY row shares one line.)
    for (auto* knob : { sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get() })
        knob->setMatchRingSpacing (true);
    addAll (*this, arpChance, genScale, genRoot, genSnap, sprayOn, sprayDirection,
            *sprayCount, *sprayRange, *spraySpread, *sprayChance, *sprayVelocity, sprayStrum, *strumTime);

    // The Generative card: ARP, EUCLID and PROB SEQ share one card.
    addAll (*this, engineTabs, euclidDisplay, eucTarget, eucDiv, eucSteps, eucHits, eucRotate, eucGate,
            pseqEditor, pseqDiv, pseqLength, pseqGate, clipEditor, clipIndex, clipMode, clipBars,
            clipGrid, clipImport, clipExport, clipPrint, clipDraw);
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
            menu.showMenuAsync (IlanaTheme::menuOptions (box.getComponent()).withMinimumWidth (box->getWidth()),
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
    clipExport.setTooltip ("Export MIDI\nDrag this onto a MIDI track in your DAW to drop the chosen clip there, or click to save "
                           "it as a .mid file (the clip's own length and notes, 120 BPM). A .mid dropped on this page imports.");
    clipExport.onClick = [this]
    {
        if (! std::exchange (clipExport.justDragged, false))
            exportMidiFile();
    };
    clipExport.makeFile = [this] { return writeClipForDrag(); };
    clipPrint.setButtonText ("PRINT TO CLIP");
    clipPrint.setTooltip (printTooltip (true));
    clipPrint.onClick = [this] { printToClip(); };
    clipQuantise.setButtonText ("QUANTISE");
    clipQuantise.setTooltip ("Quantise\nMoves the selected notes' starts (every note's, with none selected) to the nearest "
                             "GRID line, in one undo step. Q in the roll does the same.");
    clipQuantise.onClick = [this] { clipEditor.quantise(); };
    clipExpand.setTooltip ("Expand\nGives the piano roll the page: GENERATE folds to its title line until you collapse "
                           "the roll again (or click GENERATE's title).");
    clipExpand.onClick = [this] { setClipExpanded (! clipExpanded); };
    clipExpand.getProperties().remove ("pill"); // a link in the title, not a mode (S12-3)
    clipExpand.setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
    clipExpand.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text);
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

    genSnap.showAsSwitch();
    addAll (*this, arpLanes, arpMode, arpDiv, arpOctaves, arpGate);

    // Open on whichever part of the card is switched on.
    engineTabs.setSelected (readOn ("pseq_on") ? 2 : readOn ("euc_on") ? 1 : readOn ("clip_on") ? 3 : 0, false);
    updateEngineSwitches();
    showEngineTab();

    if (part == Part::notes)
    {
        for (auto* control : std::initializer_list<juce::Component*> { &step1, &step2 })
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
                 &clipGrid, &clipImport, &clipExport, &clipPrint, &clipDraw, &arpLanes, &arpMode, &arpDiv,
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

void SeqPage::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());

    if (clipEditor.isVisible() && ! clipDivider.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (clipDivider);
        g.fillRect (clipDivider2);
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
                                 "the pattern, then what shapes the keys you play", generateColour(), 12);

    // Three boxes, each a part with its own switch: its title, a quiet
    // note on what it does now, the switch at the right.
    const auto tuningOn = readOn ("tuning_on");
    const auto engineColour = tab == 1 ? euclidColour() : tab == 2 ? pseqColour() : arpColour();
    const struct { juce::Rectangle<int> box; const char* title; bool on; juce::String note; juce::Colour colour; } boxes[] {
        { snapBox, "SNAP TO KEY", scaleSwitch.isOn(),
          ! scaleSwitch.isOn() ? "off" : tuningOn ? "snaps in 12-TET steps" : readOn ("gen_snap") ? "snaps every note" : "snaps generated notes",
          SeqColours::snap() },
        // (No note while on: the switch says so, and "spreads chords" never fitted the box: UI review 31, item 2.)
        { strumBox, "STRUM", strumSwitch.isOn(), strumSwitch.isOn() ? juce::String() : juce::String ("off"),
          SeqColours::strum() },
        { sprayBox, "SPRAY", readOn ("spray_on"), readOn ("spray_on") ? "throws extra notes" : "off", SeqColours::spray() },
        { engineBox, engineTabs.getSelected() == 1 ? "EUCLID" : engineTabs.getSelected() == 2 ? "PROB SEQ" : "ARP", true, "the pattern",
          engineColour }
    };

    for (const auto& part : boxes)
    {
        if (part.box.isEmpty())
            continue;

        IlanaTheme::paintRecessedPanel (g, part.box.toFloat(), 5.0f);
        // The box's colour as a 2 px edge along its top, as the osc cards wear theirs (dim while it is off).
        g.setColour (part.colour.withAlpha (part.on ? 0.9f : 0.35f));
        g.fillRoundedRectangle (part.box.toFloat().reduced (5.0f, 0.0f).withHeight (2.0f), 1.0f);
        auto header = part.box.reduced (10, 0).removeFromTop (boxHeaderHeight);
        paintSubBoxTitle (g, header, part.title, part.note, part.on, 44); // (44: the switch)
    }
}

juce::String SeqPage::generateSummary () const
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

void SeqPage::mouseUp (const juce::MouseEvent& event)
{
    // A folded GENERATE opens again from its title line.
    if (generateFolded() && generateCard.contains (event.getPosition()))
        setClipExpanded (false);
}

void SeqPage::resized ()
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

    // MOD: the two step rows. SEQ: arp and generate side
    // by side.
    if (part == Part::notes)
    {
        layoutNotes (area);
        return;
    }

    // (The patch's MSEG is edited with the LFOs, where it shows while used.)
    const auto stepHeight = (area.getHeight() - 8) / 2;

    auto row = area.removeFromTop (stepHeight);
    stepTitle1 = row.removeFromTop (26).withTrimmedLeft (12);
    layoutPicker (0, stepTitle1);
    step1.setBounds (row);

    area.removeFromTop (8);
    row = area;
    stepTitle2 = row.removeFromTop (26).withTrimmedLeft (12);
    layoutPicker (1, stepTitle2);
    step2.setBounds (row);
}

void SeqPage::layoutNotes (juce::Rectangle<int> right)
{
    // PATTERN (the note engines) above GENERATE, both full width.
    // GENERATE takes one row of full-size knobs in its three boxes;
    // PATTERN the rest, and while the clip roll is expanded GENERATE
    // folds to its title line and the roll takes the page.
    constexpr int knobRowHeight = 13 + 58 + 6; // (no bare band under the rings: Impeccable re-score toward 35)
    const auto folded = generateFolded();
    const auto generateHeight = folded ? 34 : 26 + boxHeaderHeight + 66 + 10; // (a row of 40 px rings, their names under them)
    generateCard = right.removeFromBottom (generateHeight);
    right.removeFromBottom (8);
    arpCard = right;

    auto arpArea = arpCard.reduced (10, 0);
    auto header = arpArea.removeFromTop (26);
    // The tabs at the header's right (the switches are in the engines'
    // rows), the note path between them and the title.
    header.removeFromRight (2);
    engineTabs.setBounds (header.removeFromRight (engineTabs.getIdealWidth()).reduced (0, 4));
    // The roll's EXPAND / COLLAPSE, a link at the title (S12-3), not a button in the row.
    if (engineTabs.getSelected() == 3)
    {
        const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)), clipExpand.getButtonText()) + 24;
        header.removeFromRight (8);
        clipExpand.setBounds (header.removeFromRight (width).withSizeKeepingCentre (width, 22));
        header.removeFromRight (6);
    }
    noteChain.setBounds (header.withTrimmedLeft (IlanaTheme::cardTitleWidth ("PATTERN") - 4).withTrimmedRight (12).reduced (0, 3));

    arpArea.removeFromBottom (6);
    // The note engines' controls stand in one row under the lanes, which take
    // the card's width (ilana 2026-10-28, Impeccable re-score: the old column of
    // two beside them stood 73 % empty below its four knobs).
    const auto controls = arpArea.removeFromBottom (knobRowHeight);
    arpArea.removeFromBottom (12); // (the row's gap above it as wide as the one under its names)
    // (ilana, 2026-10-29: the engine's controls stand in the GENERATE row, the lanes take their height; the clip's
    // row stays here.)
    if (engineTabs.getSelected() != 3)
        arpArea.setBottom (controls.getBottom());
    const auto rowDisplay = arpArea.reduced (0, 2);
    arpLanes.setBounds (rowDisplay);
    euclidDisplay.setBounds (rowDisplay);
    pseqEditor.setBounds (rowDisplay);

    // Every engine's controls on one grid of fixed cells: the shared ones
    // first, in one order (RATE, STEPS, GATE), then the engine's own;
    // menus and knobs on one label line (review 8, I8-23, S8-33, V8-22).
    // (A knob sits at its cell's left edge, so its label and a menu's start
    // on the same grid line: V11-5.)
    const auto leftAligned = [] (juce::Rectangle<int> row, const std::vector<juce::Component*>& items)
    {
        layoutRow (row, items, true);
        for (auto* item : items)
            if (auto* knob = dynamic_cast<KnobControl*> (item))
                knob->placeAsRing (row.withX (item->getX()).withWidth (juce::jmin (item->getWidth(), seqKnobWidth)));
    };
    // One row, the cells sharing the card's width (at least a cell each), every
    // control on one label line; a menu keeps its own width at its cell's left
    // edge, never stretched across a wide cell (PROB SEQ's lone RATE was).
    const auto engineRow = [&] (const std::vector<juce::Component*>& items)
    {
        leftAligned (controls, items);
        for (auto* item : items)
            if (dynamic_cast<ComboControl*> (item) != nullptr)
            {
                if (item->getWidth() > seqMenuWidth)
                    item->setBounds (item->getBounds().withWidth (seqMenuWidth));
                // (ilana's layout, 2026-10-29: the menus 7 px lower, their boxes level with the rings.)
                item->setBounds (item->getBounds().withTrimmedTop (engineMenuDrop));
            }
    };
    engineRow ({ &arpDiv, &arpMode, &arpSteps, &arpGate, &arpOctaves, &arpChance });
    engineRow ({ &eucDiv, &eucTarget, &eucSteps, &eucGate, &eucHits, &eucRotate });
    engineRow ({ &pseqDiv, &pseqLength, &pseqGate });

    // The clip's row is menus and buttons only (ten columns): the
    // piano roll takes the height the other engines' knobs need.
    {
        constexpr int menuHeight = 13 + 24;
        const auto clipRow = controls.withTrimmedTop (controls.getHeight() - menuHeight - 6);
        clipEditor.setBounds (rowDisplay.withBottom (clipRow.getY() - 6));
        // Three groups with a gap between them (review 12, S12-3): what
        // plays (SLOT, MODE, LENGTH), how you edit (GRID, ZOOM, DRAW,
        // QUANTISE), the files (IMPORT, EXPORT, PRINT). COLLAPSE is in the title.
        constexpr int groupGap = 22;
        const auto column = (clipRow.getWidth() - 2 * groupGap) / 10;
        auto rest = clipRow;
        const auto playsRow = rest.removeFromLeft (column * 3);
        rest.removeFromLeft (groupGap);
        const auto editRow = rest.removeFromLeft (column * 4);
        rest.removeFromLeft (groupGap);
        const auto filesRow = rest.removeFromLeft (column * 3);
        layoutRow (playsRow, { &clipIndex, &clipMode, &clipBars });
        layoutRow (editRow, { &clipGrid, &clipZoom, nullptr, nullptr });

        // The buttons line up with the menus' boxes.
        auto cell = clipZoom.getBounds().translated (column, 0).withTrimmedTop (13).withHeight (24);
        clipDraw.setBounds (cell);
        clipQuantise.setBounds (cell.translated (column, 0));
        // (An 8 px gap between the buttons, as the toolbar's others have: S13-8, flush with filesRow.)
        const auto fileGap = Space::s8;
        const auto fileW = (filesRow.getWidth() - fileGap * 2) / 3;
        clipImport.setBounds (cell.withX (filesRow.getX()).withWidth (fileW));
        clipExport.setBounds (clipImport.getBounds().translated (fileW + fileGap, 0));
        clipPrint.setBounds (clipExport.getBounds().translated (fileW + fileGap, 0));
        clipDivider = juce::Rectangle<int> (editRow.getX() - groupGap / 2, playsRow.getY() + 8, 1, playsRow.getHeight() - 8);
        clipDivider2 = juce::Rectangle<int> (filesRow.getX() - groupGap / 2, playsRow.getY() + 8, 1, playsRow.getHeight() - 8);
    }

    layoutGenerate();
}

void SeqPage::layoutGenerate ()
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

    struct Item { juce::Component* control; float weight; float pixels = 0.0f; };
    // EUCLID's TARGET needs the width of "Trance Gate" (it read "Not..."): EUCLID's own rings take the ARP's
    // ring pitch and SPRAY's rings a little less, so the boxes keep their sizes (UI review 31, item 2; SNAP TO
    // KEY, STRUM and the DIRECTION menus have no slack: their names and values fill them).
    const auto euclidShown = engineTabs.getSelected() == 1;
    const auto sprayKnob = euclidShown ? 0.82f : 0.85f;
    std::vector<std::vector<Item>> groups {
        { { &genRoot, 0.8f }, { &genScale, 1.2f }, { &genSnap, 1.35f } },
        { { &sprayStrum, 1.0f }, { strumTime.get(), 1.0f } },
        { { &sprayDirection, 1.05f }, { sprayCount.get(), sprayKnob }, { sprayRange.get(), sprayKnob },
          { spraySpread.get(), sprayKnob }, { sprayChance.get(), sprayKnob }, { sprayVelocity.get(), sprayKnob } }
    };
    // (MODE is wider, so "UpDown", "Converge" and "Scale Random" read whole; SPRAY's five rings give the width
    // back: UI review 29, B1. The rings' cells get the width their ring needs, below.)
    const std::vector<Item> arpItems { { &arpDiv, 0.9f }, { &arpMode, 1.6f }, { &arpSteps, 0.84f }, { &arpGate, 0.84f },
                                       { &arpOctaves, 0.84f }, { &arpChance, 0.84f } };
    engineBox = {};
    if (engineTabs.getSelected() != 3)
    {
        const auto engine = engineTabs.getSelected();
        std::vector<Item> items;
        if (engine == 0)
            items = arpItems;
        else if (engine == 1)
            items = { { &eucDiv, 1.0f }, { &eucTarget, 1.55f }, { &eucSteps, 0.7f }, { &eucGate, 0.7f }, { &eucHits, 0.7f }, { &eucRotate, 0.7f } };
        else
            items = { { &pseqDiv, 1.0f }, { &pseqLength, 0.8f }, { &pseqGate, 0.8f } };
        groups.insert (groups.begin(), items);
    }
    const auto merged = groups.size() == 4;
    constexpr int gap = 6, padding = 4;

    // Every menu's value reads at one size (polish pass 3): a menu's text area is its width less the arrow, and a
    // value wider than that is shrunk to fit, so RATE, ROOT and the DIRECTION menus (narrow cells) read small beside
    // KEY and MODE. Each menu gets the width its longest value needs at the select text size (a very long list is
    // judged by its first 52 px), taken from the knobs' share.
    const auto nameFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label, true).withKerningFactor (0.07f));
    // A ring's cell: the ring, the cell's 3 px a side and a hair, so no ring is cut (UI audit 9).
    constexpr float ringCell = 44.0f; // (the ring 40 and its cell's 3 px a side, less what a ring's own margin gives; VELOCITY's name fits)
    const auto fit = [&] (Item& item)
    {
        if (auto* menu = dynamic_cast<ComboControl*> (item.control))
        {
            const juce::Font font (IlanaTheme::font (IlanaLookAndFeel::selectText));
            auto widest = 0.0f;
            const auto& box = menu->getComboBox();
            for (int i = 0; i < box.getNumItems(); ++i)
                widest = juce::jmax (widest, (float) juce::GlyphArrangement::getStringWidth (font, box.getItemText (i)));
            item.pixels = juce::jmin (widest, 52.0f) + 23.0f + 3.0f + 6.0f; // (the arrow's 23 px, the label's borders, the cell's 3 px a side)
            // The name reads whole at its own size (a label squeezes its text to fit: DIRECTION did).
            item.pixels = juce::jmax (item.pixels, (float) juce::GlyphArrangement::getStringWidthInt (nameFont, menu->getLabelText()) + 5.0f + 6.0f);
        }
        // (OCTAVES' and SNAP PLAYED's names keep their room too.)
        else if (item.control == &arpOctaves)
            item.pixels = 47.0f;
        else if (item.control == &genSnap)
            item.pixels = (float) juce::GlyphArrangement::getStringWidthInt (nameFont, "SNAP PLAYED") + 6.0f + 4.0f;

        for (juce::Component* ring : std::initializer_list<juce::Component*> { &arpSteps, &arpGate, &arpOctaves, &arpChance, &eucSteps, &eucHits, &eucRotate, &eucGate, &pseqLength, &pseqGate,
                           sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get() })
            if (item.control == ring)
                item.pixels = juce::jmax (item.pixels, ringCell);
    };
    for (auto& group : groups)
        for (auto& item : group)
            fit (item);
    // (STRUM's title and its switch need 130 px: its TIME knob takes up what DIRECTION leaves.)
    const auto padStrum = [] (std::vector<std::vector<Item>>& gs)
    {
        auto& strum = gs[gs.size() == 4 ? 2 : 1];
        strum.back().pixels = juce::jmax (strum.back().pixels, 130.0f - (float) padding * 2.0f - strum.front().pixels);
    };
    padStrum (groups);
    // (The engine's box keeps its title's room: PROB SEQ's two rings left "the pattern" cut.)
    if (merged)
    {
        auto total = 0.0f;
        for (const auto& item : groups[0])
            total += item.pixels > 0.0f ? item.pixels : item.weight * 50.0f;
        groups[0].back().pixels = juce::jmax (groups[0].back().pixels, groups[0].back().pixels + 190.0f - (float) padding * 2.0f - total);
    }
    // A menu keeps its weight's share unless its values need more: those are pinned to their width, the rest share what is left.
    const auto solve = [&] (std::vector<std::vector<Item>>& gs, std::vector<bool>& pinned)
    {
        const auto count = (int) gs.size();
        const auto available = (float) (area.getWidth() - gap * (count - 1) - padding * 2 * count);
        std::vector<Item*> flat;
        for (auto& group : gs)
            for (auto& item : group)
                flat.push_back (&item);
        pinned.assign (flat.size(), false);
        auto result = 0.0f;
        for (auto pass = 0; pass < 8; ++pass)
        {
            auto weights = 0.0f, fixed = 0.0f;
            for (size_t i = 0; i < flat.size(); ++i)
                (pinned[i] ? fixed : weights) += pinned[i] ? flat[i]->pixels : flat[i]->weight;
            result = (available - fixed) / juce::jmax (0.1f, weights);
            auto changed = false;
            for (size_t i = 0; i < flat.size(); ++i)
                if (! pinned[i] && flat[i]->pixels > flat[i]->weight * result)
                    pinned[i] = true, changed = true;
            if (! changed)
                break;
        }
        return result;
    };
    std::vector<bool> pinned;
    auto unit = solve (groups, pinned);
    // (UI audit 17's natural widths were tried and dropped: ilana, 2026-11-17, liked the cards sharing the row.)
    // (Capped, so the boxes stay a group at the left rather than spreading over the card.)
    unit = juce::jmin (118.0f, unit);
    // (What a menu or ring needs stays whole at this unit.)
    for (auto& group : groups)
        for (auto& item : group)
            if (item.pixels > item.weight * unit)
                item.weight = item.pixels / unit;
    std::vector<juce::Rectangle<int>*> boxes { &snapBox, &strumBox, &sprayBox };
    std::vector<juce::Component*> switches { &scaleSwitch, &strumSwitch, &sprayOn };
    if (merged)
    {
        boxes.insert (boxes.begin(), &engineBox);
        switches.insert (switches.begin(), nullptr);
    }
    auto x = (float) area.getX();
    const auto extra = 0.0f;
    std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> placed;
    for (size_t g = 0; g < groups.size(); ++g)
    {
        auto weight = 0.0f;
        for (const auto& item : groups[g])
            weight += item.weight;

        const auto width = weight * unit + (float) padding * 2.0f + extra;
        auto& box = *boxes[g];
        box = juce::Rectangle<int> (juce::roundToInt (x), area.getY(), juce::roundToInt (width), area.getHeight());
        x += width + (float) gap;

        // The switch closes the box's title line (a bare switch: the
        // title names it).
        const auto title = box.withHeight (boxHeaderHeight);
        if (switches[g] != nullptr)
            switches[g]->setBounds (juce::Rectangle<int> (title.getRight() - 8 - 40, title.getCentreY() - 23, 40, 13 + 20));

        // The controls: every one a thing on the rings' middle line with its name under it on the rings' name
        // line (UI polish 2026-11-25: menus had their names above while the rings had theirs below).
        auto row = box.reduced (padding, 0).withTrimmedTop (boxHeaderHeight).withTrimmedBottom (4);
        auto left = (float) row.getX();
        for (const auto& item : groups[g])
        {
            const auto cell = juce::Rectangle<int> (juce::roundToInt (left), row.getY(), juce::roundToInt (item.weight * unit),
                                                    row.getHeight()).reduced (3, 0);
            left += item.weight * unit;
            placed.push_back ({ item.control, cell });
        }
    }

    // Rings first: they set the middle line and the name line the menus and switches follow.
    int middle = -1, nameTop = -1;
    float nameUnits = 0.0f;
    for (const auto& [control, cell] : placed)
        if (auto* knob = dynamic_cast<KnobControl*> (control))
        {
            knob->placeAsRing (cell, 40);
            if (middle < 0)
            {
                middle = knob->getY() + juce::roundToInt (knob->getDialCentre().y);
                nameTop = knob->getY() + knob->getPackedNameArea().getY();
                nameUnits = knob->packedNameUnits();
            }
        }
    for (const auto& [control, cell] : placed)
    {
        if (dynamic_cast<KnobControl*> (control) != nullptr)
            continue;
        if (middle < 0)
        {
            control->setBounds (cell);
            continue;
        }
        constexpr int well = 21; // (under 22 px a select's value takes the body size, no bigger than the rings' numbers)
        if (auto* menu = dynamic_cast<ComboControl*> (control))
        {
            const auto top = middle - well / 2;
            menu->setNameBelow (nameUnits, juce::jmax (0, nameTop - (top + well) - 1), well);
            menu->setBounds (cell.withTop (top).withBottom (juce::jmax (top + well, nameTop + 14)));
        }
        else if (auto* toggle = dynamic_cast<ToggleControl*> (control))
        {
            toggle->setPackedName (ToggleControl::NamePlace::below);
            toggle->setBelowNameSize (nameUnits);
            toggle->setBounds (cell.withTop (middle - 7).withBottom (juce::jmax (middle + 9, nameTop + 14)));
            toggle->setNameDrop (juce::jmax (0, nameTop - (middle - 7 + 15 + 1)));
        }
        else
            control->setBounds (cell);
    }
}

void SeqPage::visibilityChanged ()
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

void SeqPage::showLfo (int row, int lfo)
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

bool SeqPage::playsSteps (int lfo) const
{
    const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
    return shape != nullptr && juce::roundToInt (shape->load()) == LfoShapes::Steps;
}

void SeqPage::useSteps (int row)
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

void SeqPage::updateUseButtons ()
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

void SeqPage::timerCallback ()
{
    if (! isVisible())
        return;

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

    if (const auto hasChord = ! processorRef.getPrintChord().empty(); hasChord != clipPrint.isEnabled())
    {
        clipPrint.setEnabled (hasChord);
        clipPrint.setTooltip (printTooltip (hasChord));
    }

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

    if (part == Part::modulators)
    {
        repaint (stepTitle1); // their notes follow the LFOs' shapes
        repaint (stepTitle2);
    }
}

float SeqPage::readValue (const char* id) const
{
    const auto* value = processorRef.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

void SeqPage::showEngineTab ()
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
                                                                    &clipGrid, &clipImport, &clipExport, &clipPrint, &clipZoom, &clipDraw, &clipQuantise, &clipExpand })
        control->setVisible (tab == 3);

    // An expanded roll folds GENERATE only while CLIP is shown.
    if (! getLocalBounds().isEmpty())
        resized();

    repaint();
}

void SeqPage::setClipExpanded (bool expanded)
{
    clipExpanded = expanded;
    clipExpand.setButtonText (expanded ? "COLLAPSE" : "EXPAND");
    resized();
    repaint();
}

void SeqPage::updateEngineSwitches ()
{
    const char* const engineSwitches[] { "arp_on", "euc_on", "pseq_on", "clip_on" };
    for (int engine = 0; engine < 4; ++engine)
        engineTabs.setTabOn (engine, readOn (engineSwitches[engine]));
}

void SeqPage::toggleEngine (int engine)
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

std::vector<juce::Component*> SeqPage::generateControls ()
{
    return { &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum, sprayCount.get(), sprayRange.get(),
             spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get(), &scaleSwitch, &strumSwitch };
}

void SeqPage::importMidiFile ()
{
    clipChooser = std::make_unique<juce::FileChooser> ("Import MIDI into the clip",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.mid;*.midi");
    juce::Component::SafePointer<SeqPage> safeThis (this);

    clipChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis != nullptr && chooser.getResult().existsAsFile())
                                      safeThis->importMidi (chooser.getResult());
                              });
}

bool SeqPage::importMidi (const juce::File& file)
{
    Clip imported;
    juce::String error;

    if (! ClipState::importMidi (file, imported, error))
    {
        ConfirmOverlay::tell (this, "Import MIDI", error);
        return false;
    }

    const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) readValue ("clip_index"));
    processorRef.performEdit ("Import MIDI clip", [&]
    {
        processorRef.getClipState().setClip (index, std::move (imported));
        processorRef.clipsEdited();
    });
    clipEditor.reload (true);
    clipBars.refresh();
    return true;
}

juce::String SeqPage::printTooltip (bool hasChord)
{
    if (! hasChord)
        return "Print to clip\nPlay a chord first: this prints what the generators would play for the chord you hold (or played last).";

    return "Print to clip\nWrites what KEYS > STRUM > EUCLID > PROB SEQ > ARP play for the chord you hold (the last one you played, "
           "if none) into the chosen clip, over its length, replacing its notes, in one undo step. Then drag it out with EXPORT MIDI.";
}

// PRINT TO CLIP: the generators' notes for the chord, offline, into the chosen clip.
void SeqPage::printToClip ()
{
    const auto chord = processorRef.getPrintChord();

    if (chord.empty())
        return;

    const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) readValue ("clip_index"));
    auto printed = processorRef.printGeneratorChain (chord, processorRef.getClipState().getClip (index).bars);
    processorRef.performEdit ("Print to clip", [&]
    {
        processorRef.getClipState().setClip (index, std::move (printed));
        processorRef.clipsEdited();
    });
    clipEditor.reload (true);
    clipBars.refresh();
}

bool SeqPage::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (part != Part::notes)
        return false;

    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void SeqPage::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi"))
        {
            importMidi (juce::File (path));
            return;
        }
}

juce::File SeqPage::writeClipForDrag ()
{
    const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) readValue ("clip_index"));
    const auto clip = processorRef.getClipState().getClip (index);

    if (clip.notes.empty())
        return {};

    const auto name = processorRef.getCurrentPresetName().replaceCharacters ("\\/:*?\"<>|", "---------").trim();
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilanaSynth MIDI");
    folder.createDirectory();
    const auto file = folder.getChildFile ((name.isEmpty() ? juce::String ("ilanaSynth") : name) + " clip " + juce::String (index + 1) + ".mid");
    juce::String error;
    return ClipState::exportMidi (clip, file, error) ? file : juce::File();
}

void SeqPage::exportMidiFile ()
{
    const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) readValue ("clip_index"));
    const auto clip = processorRef.getClipState().getClip (index);

    if (clip.notes.empty())
    {
        ConfirmOverlay::tell (this, "Export MIDI", "This clip has no notes yet.");
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
                                      ConfirmOverlay::tell (safeThis.getComponent(), "Export MIDI", error);
                              });
}
