#include "FmInputPages.h"

#include "PageHelpers.h"

FmPage::FmPage (IlanaSynthAudioProcessor& p)
    : processorRef (p),
      diagram (p),
      algorithms (p),
      outputView (p),
      envelope (p, OperatorEnvEditor::Place::fmCard),
      pageTabs ({ "BASIC", "DX7 1-16", "DX7 17-32" }, { fmColour(), fmColour(), fmColour() }, false),
      mode (p.apvts, "fm_mode", "FM MODE"),
      hardSync (p.apvts, "hard_sync", "SYNC 2 TO 1"),
      effectRules (p)
{
        addAll (*this, diagram, algorithms, pageTabs, mode, hardSync);
        diagram.onSelectOperator = [this] (int op) { selectOperator (op); };
        addChildComponent (outputView);
        hardSync.showAsSwitch();
        ringMod = std::make_unique<KnobControl> (p.apvts, "ring_mod", "RING MOD", fmColour(), false);
        ringMod->setSizeRole (IlanaTheme::KnobSize::minimum);
        addAndMakeVisible (*ringMod);

        // The algorithm pages, on the ALGORITHMS heading's line; the heading
        // names the cell under the mouse.
        pageTabs.setTooltip ("BASIC: simple routings for any oscillators. DX7: the DX7's 32 algorithms, under their own numbers.");
        pageTabs.setSelected (algorithms.getPage(), false);
        pageTabs.onSelect = [this] (int page) { algorithms.setPage (page); };
        algorithms.onPageChanged = [this] { pageTabs.setSelected (algorithms.getPage(), false); };
        algorithms.onHoverChanged = [this] (const juce::String& name)
        {
            hoverAlgorithm = name;
            repaint (algorithmsTitle);
        };

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            for (int target = 0; target < OscillatorIds::count; ++target)
            {
                auto knob = std::make_unique<KnobControl> (p.apvts, FmDiagram::routeId (source, target), "",
                                                           FmDiagram::oscColour (source), false);
                // Send amounts are the page's small print (UI review 4, V22).
                knob->setSizeRole (IlanaTheme::KnobSize::mini);
                addAndMakeVisible (*knob);
                knobs[(size_t) source][(size_t) target] = std::move (knob);
            }

            for (int target = 0; target < OscillatorIds::count; ++target)
            {
                // (ilana, 2026-10-29: the depth on the cable itself, PATCH's way; the same parameter as the ROUTES
                // list's knob, so both automate and take modulation.)
                auto handle = std::make_unique<KnobControl> (p.apvts, FmDiagram::routeId (source, target), "", FmDiagram::oscColour (source), false);
                handle->setPackedDial (30.0f);
                addChildComponent (*handle);
                cableKnobs[(size_t) source][(size_t) target] = std::move (handle);
            }

            outs[(size_t) source] = std::make_unique<ToggleControl> (
                p.apvts, juce::String (OscillatorIds::prefixes[(size_t) source]) + "_out", "HEARD");
            addAndMakeVisible (*outs[(size_t) source]);

            noiseKnobs[(size_t) source] = std::make_unique<KnobControl> (p.apvts, "fm_noise" + juce::String (source + 1), "",
                                                                          noiseColour(), false);
            noiseKnobs[(size_t) source]->setSizeRole (IlanaTheme::KnobSize::mini);
            addAndMakeVisible (*noiseKnobs[(size_t) source]);

            const juce::String prefix (OscillatorIds::prefixes[(size_t) source]);
            operators[(size_t) source] = std::make_unique<OperatorControls> (p.apvts, prefix, FmDiagram::oscColour (source), source);
            for (auto* control : operators[(size_t) source]->all())
                addChildComponent (control);

            // FB TYPE only acts with a feedback route (I6-10).
            effectRules.add (operators[(size_t) source]->feedbackType,
                             effectRules.isAbove (FmDiagram::routeId (source, source), 0.0005f));
        }

        // The noise operator's colour, at the head of its row (V6-14).
        noiseColourKnob = std::make_unique<KnobControl> (p.apvts, "fm_noise_color", "", noiseColour(), false);
        noiseColourKnob->setCompact (true);
        noiseColourKnob->setSizeRole (IlanaTheme::KnobSize::mini);
        addAndMakeVisible (*noiseColourKnob);
        // Its noise type: the colour knob's own noise, or one of the NOISE card's colours.
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("fm_noise_type")))
            noiseTypeMenu.addItemList (choice->getAllValueStrings(), 1);
        noiseTypeMenu.setTooltip (describeParameter ("fm_noise_type"));
        addAndMakeVisible (noiseTypeMenu);
        noiseTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "fm_noise_type", noiseTypeMenu);

        addRouteButton.setButtonText ("+ ADD ROUTE");
        addRouteButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        addRouteButton.setColour (juce::TextButton::textColourOffId, fmColour());
        addRouteButton.setTooltip ("Add an FM route: pick the oscillator that modulates, then the one it modulates. Or drag from one box to another in the picture.");
        addRouteButton.onClick = [this] { showAddRouteMenu(); };
        addAndMakeVisible (addRouteButton);
        ringMod->setLabelText ("RING MOD");
        noiseColourKnob->setLabelText ("COLOUR");

        // The same heading as the open section ("EXTRAS", at the matrix's
        // foot) whatever the operator count (review 10, I10-5).
        moreButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("EXTRAS \xc2\xb7 RING MOD \xc2\xb7 SYNC \xc2\xb7 NOISE FM \xe2\x96\xbe")));
        moreButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        moreButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        moreButton.setTooltip ("RING MOD and SYNC 2 TO 1 (OSC 1 and OSC 2 only) and the NOISE FM row: classic FM extras a DX7 "
                               "voice doesn't use.");
        // One layout on every patch (review 11, I11-5): the line is always
        // there and opens or closes the extras; they stay open while one is
        // in use.
        moreButton.onClick = [this]
        {
            extrasOpen = ! extrasOpen;
            resized();
            repaint();
        };
        addAndMakeVisible (moreButton);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_tune", "_amp_env", "_on", "_mode" })
                tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + suffix));

        // The operator picker (the one oscillator picker, I8-10) and, past
        // it, the link to the voice's OP PITCH and OP LFO on MOD: a link,
        // not a seventh operator tab (V8-21), there only while an oscillator
        // plays the Operator Env.
        picker.onPick = [this] (int op) { selectOperator (op); };
        addAndMakeVisible (picker);
        styleFmLink (pitchLfoLink, juce::String::fromUTF8 ("OP PITCH \xc2\xb7 OP LFO"));
        pitchLfoLink.setTooltip ("The Operator Env's pitch envelope (OP PITCH) and LFO (OP LFO), for the whole voice: every "
                                 "oscillator on the Operator Env follows them. Click: their editors, by their chips in the dock.");
        pitchLfoLink.onClick = [] { FmOperatorInfo::openPitchAndLfo(); };
        addChildComponent (pitchLfoLink);
        // An oscillator of another type: one click makes it an FM / DX7
        // operator (the same sound, the operator's controls).
        // (An action, not a jump: a tinted pill.)
        makeOperator.setButtonText ("SWITCH TO FM / DX7");
        IlanaTheme::makePill (makeOperator, fmColour());
        makeOperator.setToggleState (true, juce::dontSendNotification);
        makeOperator.setTooltip ("Make this oscillator an FM / DX7 operator: it sounds the same, and gets ratio and fixed tuning and the OP ENV");
        makeOperator.onClick = [this]
        {
            OscRole::chooseMode (processorRef, selectedOperator, OscMode::fmOperator);
            updateOperatorVisibility();
            resized();
            repaint();
        };
        addChildComponent (makeOperator);
        addChildComponent (envelope);

        refreshShown();
        refreshFmInputs (true);
        // The classic FM extras (ring mod, sync, noise FM) are open on a patch
        // that isn't a DX7 voice: they are the controls it has to fill the
        // matrix card with (V14-5).
        extrasOpen = ! anyOperatorEnv();
        selectOperator (shown.empty() ? 0 : shown.front());
        startTimerHz (12);
        IlanaTheme::identityColourListeners (&p).add (this);
    }

void FmPage::identityColoursChanged ()
{
    for (int source = 0; source < OscillatorIds::count; ++source)
    {
        const auto colour = FmDiagram::oscColour (source);
        for (auto& knob : knobs[(size_t) source])
            knob->setIdentityColour (colour);
        auto& controls = *operators[(size_t) source];
        for (auto* knob : { &controls.ratio, &controls.fixedHz, &controls.semi, &controls.fine, &controls.level,
                            &controls.keyLevel, &controls.egOut, &controls.feedback })
            knob->setIdentityColour (colour);
    }
    envelope.refreshColours();
    ampGraphOperator = -1; // the graph is made again in the operator's colour
    updateOperatorVisibility();
    resized();
    repaint();
}

void FmPage::selectOperator (int op)
{
    selectedOperator = juce::jlimit (0, OscillatorIds::count - 1, op);
    picker.setSelectedOsc (selectedOperator);
    diagram.setSelectedOperator (selectedOperator);
    envelope.selectOperator (selectedOperator);
    updateOperatorVisibility();
    resized();
    repaint();
}

void FmPage::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());
    paintSectionTitle (g, "ALGORITHMS", algorithmsTitle.withTrimmedRight (pageTabs.getWidth() + 8),
                       hoverAlgorithm.isNotEmpty() ? hoverAlgorithm : getAlgorithmLabel());
    IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

    // The selected operator's settings, the picker on the header line.
    const auto header = operatorCard.reduced (12, 0).withHeight (26);
    const auto reserve = operatorCard.getRight() - (makeOperator.isVisible() ? makeOperator.getX() : tabsLeft) + 8;
    const auto colour = FmDiagram::oscColour (selectedOperator);
    IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
    // On the Operator Env the caption says what it is: an operator, after the one name (I12-18, I13-1; the title
    // alone, as on every card, so it never reaches the picker: I14-1).
    IlanaTheme::paintCardHeader (g, header, "OSC " + juce::String (selectedOperator + 1),
                                 (usesOperatorEnv (selectedOperator) ? juce::String (juce::String::fromUTF8 ("operator \xc2\xb7 ")) : juce::String()) + operatorText(), colour, reserve);

    // An operator on another envelope: where that envelope is edited; an
    // oscillator of another type: what it takes part in, and the way to
    // the operator's own controls (FM / DX7).
    if (! usesOperatorEnv (selectedOperator))
    {
        // One short hint, the same on every type (I15-11); the longer sentence is ENVELOPE's tooltip. It stands on the
        // title line beside the make-operator button when the line has the room (it never shrinks to fit).
        // (The graph beside it is the envelope itself and takes drags, so the hint says so rather than sending
        // you to PLAY: UI review 29, B2.)
        const juce::String hint = isFmType (selectedOperator) ? (ampGraph != nullptr ? "Drag the points to edit this envelope."
                                                                                     : "Edit the envelope on PLAY.")
                                                               : "Ratio and OP ENV are for FM / DX7 oscillators.";
        const auto hintFont = IlanaTheme::font (IlanaTheme::TextSize::body);
        const auto hintWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (hintFont), hint);
        const auto titleEnd = header.getX() + IlanaTheme::cardTitleWidth ("OSC 0") + 240;
        const auto right = makeOperator.isVisible() ? makeOperator.getX() - Space::s12 : tabsLeft - Space::s12;
        if (right - hintWidth >= titleEnd)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (hintFont);
            g.drawText (hint, juce::Rectangle<int> (right - hintWidth, header.getY(), hintWidth, header.getHeight()),
                        juce::Justification::centredRight, false);
        }
    }

    paintRoutes (g);
}

void FmPage::visibilityChanged ()
{
    if (isVisible())
        algorithms.refreshMatch();
}

void FmPage::timerCallback ()
{
    if (! isVisible())
        return;

    const auto tuneChanged = [this]
    {
        auto changed = false;
        for (size_t i = 0; i < tuneValues.size(); ++i)
        {
            const auto value = tuneValues[i] != nullptr ? juce::roundToInt (tuneValues[i]->load()) : 0;
            changed = changed || value != lastTune[i];
            lastTune[i] = value;
        }
        return changed;
    }();

    // (Visible, not showing: an offscreen editor, as in the UI test,
    // keeps its page current too.)
    if (isVisible())
    {
        algorithms.refreshMatch();
        effectRules.apply();

        if (const auto label = getAlgorithmLabel(); label != lastAlgorithmLabel)
        {
            lastAlgorithmLabel = label;
            repaint (algorithmsTitle);
        }
    }

    if (routesSignature() != lastRoutesSignature)
    {
        layoutRoutes();
        repaint (matrixCard);
    }
    layoutCableKnobs();

    const auto inputsChanged = refreshFmInputs (false);
    const auto cellsChanged = refreshCellKnobs();
    if (inputsChanged || cellsChanged)
        repaint (matrixCard);
    if (inputsChanged)
        updateOperatorVisibility();

    if (anyOperatorEnv() != lastAnyOperatorEnv)
        extrasOpen = ! anyOperatorEnv();

    if (refreshShown() || tuneChanged || diagram.getMinimumHeight() != lastDiagramMinimum || diagram.getPreferredHeight() != lastDiagramPreferred
        || anyOperatorEnv() != lastAnyOperatorEnv || matrixExtrasShown() != lastExtrasShown)
    {
        updateOperatorVisibility();
        resized();
        repaint();
    }
    else if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
    {
        repaint (matrixCard);
        repaint (operatorCard.withHeight (26));
    }
}

bool FmPage::refreshFmInputs (bool force)
{
    std::array<bool, OscillatorIds::count> nowIn {}, nowOn {};

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        nowIn[(size_t) osc] = FmDiagram::receivesFm (processorRef, osc);
        nowOn[(size_t) osc] = FmOperatorInfo::isPlaying (processorRef, osc);
    }

    if (nowIn == fmIn && nowOn == playing && ! force)
        return false;

    fmIn = nowIn;
    playing = nowOn;

    for (int target = 0; target < OscillatorIds::count; ++target)
    {
        for (int source = -1; source < OscillatorIds::count; ++source)
        {
            auto* knob = source < 0 ? noiseKnobs[(size_t) target].get() : knobs[(size_t) source][(size_t) target].get();
            const auto note = ! playing[(size_t) target] ? "OSC " + juce::String (target + 1) + " is off: switch it on on OSC"
                              : source >= 0 && ! playing[(size_t) source]
                                  ? "OSC " + juce::String (source + 1) + " is off: switch it on on OSC"
                                  : FmDiagram::fmInputNote (processorRef, target);
            const auto live = isLiveCell (source, target);
            knob->setEnabled (live);
            knob->setTooltip (live ? knob->getSlider().getTooltip() : note);
        }
    }

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        outs[(size_t) osc]->setAlpha (playing[(size_t) osc] ? 1.0f : 0.35f);
    }

    refreshCellKnobs (true);
    return true;
}

bool FmPage::refreshShown ()
{
    std::vector<int> nowShown;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (processorRef.isOscillatorShown (osc))
            nowShown.push_back (osc);

    if (nowShown == shown)
        return false;

    shown = nowShown;

    if (std::find (shown.begin(), shown.end(), selectedOperator) == shown.end() && ! shown.empty())
        selectedOperator = shown.front();

    return true;
}

void FmPage::resized ()
{
    auto area = getLocalBounds().reduced (12);

    // The matrix as wide as its cells (one cell size for every patch,
    // V8-35), at least as wide as its FM MODE line; the rest goes to
    // the diagram and the operator card.
    // (ilana, 2026-10-29, FM option A: the picture is the editor; at the right, the ROUTES list, FM MODE and the
    // extras in one narrow column.)
    matrixCard = area.removeFromRight (juce::jlimit (260, 300, area.getWidth() * 29 / 100));
    area.removeFromRight (10);

    // Left column: algorithms (one row, its pages on the heading's line),
    // the diagram, the selected operator. The diagram needs no heading
    // of its own: its nodes say what it is, and the height goes to it.
    algorithmsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
    const auto tabsWidth = pageTabs.getIdealWidth();
    pageTabs.setBounds (algorithmsTitle.getRight() - tabsWidth - 2, algorithmsTitle.getCentreY() - 10, tabsWidth, 20);
    algorithms.setBounds (area.removeFromTop (42));
    area.removeFromTop (8);

    // The operator card has one height on every patch, whatever the
    // algorithm, the operator picked, its envelope or the voice panel,
    // so nothing above it ever moves (I7-10, I8-11); the diagram sizes
    // its nodes to what is left.
    lastDiagramMinimum = diagram.getMinimumHeight();
    lastDiagramPreferred = diagram.getPreferredHeight();
    lastAnyOperatorEnv = anyOperatorEnv();
    lastExtrasShown = matrixExtrasShown();
    // (ilana, 2026-10-29: "osc + env section could prolly be smaller": a patch with no Operator Env gets the
    // one-row card; one height per patch, so switching operators never moves the picture.)
    // (UI review 31, 17: the diagram as tall as its rows need, at least 150, and the card takes the rest, so a
    // one-row algorithm gives its envelope the room; a deep stack keeps the card at its old height.)
    const auto cardHeight = anyOperatorEnv() ? operatorCardHeight : plainCardHeight;
    const auto diagramHeight = juce::jmax (150, diagram.getPreferredHeight());
    operatorCard = area.removeFromBottom (juce::jmax (150, juce::jmax (juce::jmin (cardHeight, area.getHeight() - 150), area.getHeight() - 8 - diagramHeight)));
    area.removeFromBottom (8);
    diagram.setBounds (area);

    layoutOperatorCard();
    layoutRoutes();
    layoutCableKnobs();
}

float FmPage::read (const juce::String& id) const
{
    if (const auto* value = processorRef.apvts.getRawParameterValue (id))
        return value->load();

    return 0.0f;
}

juce::String FmPage::getAlgorithmLabel () const
{
    if (const auto dx7 = processorRef.findMatchingDx7Algorithm(); dx7 > 0)
        return "DX7 ALGORITHM " + juce::String (dx7);

    const auto matching = processorRef.findMatchingFmAlgorithm();
    if (matching >= 0 && matching < FmAlgorithms::numBasic)
        return FmAlgorithmStrip::basicName (matching);
    if (matching >= 0 && matching < (int) FmAlgorithms::all().size())
        return FmAlgorithms::all()[(size_t) matching].name;

    // One that no tile matches names the nearest (V7-14).
    if (const auto nearTile = FmAlgorithmStrip::nearestBasic (processorRef); nearTile >= 0)
        return FmAlgorithmStrip::basicName (nearTile) + ", EDITED"; // (review 12, I12-9)

    for (const auto source : shown)
        for (const auto target : shown)
            if (read (FmDiagram::routeId (source, target)) > 0.001f)
                return "CUSTOM ROUTING";

    return {};
}

juce::String FmPage::operatorText () const
{
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    const auto prefix = FmOperatorInfo::prefixOf (selectedOperator);
    const auto tune = juce::roundToInt (read (prefix + "_tune"));
    auto text = tune == OscTuning::Ratio || tune == OscTuning::Fixed ? FmOperatorInfo::tuningText (processorRef, selectedOperator)
                                                                     : juce::String ("semitones");
    text << dot << (usesOperatorEnv (selectedOperator) ? "OP ENV" : operators[(size_t) selectedOperator]->ampEnv.getComboBox().getText());
    // OUT or MOD, as its node in the diagram says.
    text << dot << (read (prefix + "_out") > 0.5f ? "HEARD" : "MOD");
    if (! FmOperatorInfo::isPlaying (processorRef, selectedOperator))
        text << dot << "off";
    return text;
}

bool FmPage::anyOperatorEnv () const
{
    for (const auto op : shown)
        if (usesOperatorEnv (op))
            return true;
    return false;
}

bool FmPage::isLiveCell (int source, int target) const
{
    return fmIn[(size_t) target] && playing[(size_t) target] && (source < 0 || playing[(size_t) source]);
}

juce::String FmPage::noRoutesText () const
{
    for (const auto target : shown)
        if (fmIn[(size_t) target] && playing[(size_t) target])
            return "Drag one oscillator onto another in the picture, or press + ADD ROUTE.";
    return "No FM routes: these oscillators don't take FM in. Switch one to FM / DX7.";
}

float FmPage::cellAmount (int source, int target) const
{
    return read (source < 0 ? "fm_noise" + juce::String (target + 1) : FmDiagram::routeId (source, target));
}

KnobControl& FmPage::cellKnob (int source, int target)
{
    return source < 0 ? *noiseKnobs[(size_t) target] : *knobs[(size_t) source][(size_t) target];
}

bool FmPage::refreshCellKnobs (bool force)
{
    auto changed = false;
    for (const auto target : shown)
    {
        for (int source = -1; source < OscillatorIds::count; ++source)
        {
            if (source >= 0 && std::find (shown.begin(), shown.end(), source) == shown.end())
                continue;
            auto& knob = cellKnob (source, target);
            const auto used = cellAmount (source, target) > 0.0005f;
            const auto alpha = ! isLiveCell (source, target) ? (used ? 0.22f : 0.0f)
                               : used || knob.isMouseOverOrDragging (true) ? 1.0f
                                                                          : 0.0f;
            if (knob.getAlpha() != alpha || force)
            {
                changed = changed || knob.getAlpha() != alpha;
                knob.setAlpha (alpha);
            }
        }
    }
    return changed;
}

void FmPage::updateOperatorVisibility ()
{
    // The picker offers the shown oscillators, lit while they play; the
    // link to OP PITCH and OP LFO shows while any plays the Operator Env.
    picker.setOscillators (shown, [this] (int op) { return playing[(size_t) op]; },
                           [] (int op) { return "Edit OSC " + juce::String (op + 1) + " as an operator"; });
    picker.setSelectedOsc (selectedOperator);
    pitchLfoLink.setVisible (anyOperatorEnv());

    const auto opEnv = usesOperatorEnv (selectedOperator);
    makeOperator.setVisible (! isFmType (selectedOperator) && ! opEnv);
    for (int op = 0; op < OscillatorIds::count; ++op)
    {
        auto& controls = *operators[(size_t) op];
        const auto selected = op == selectedOperator;
        // Ratio, Fixed Hz and OP ENV are the FM / DX7 type's (a
        // Wavetable's ENVELOPE has no OP ENV).
        OscRole::showOperatorChoices (&controls.tune.getComboBox(), &controls.ampEnv.getComboBox(), isFmType (op),
                                      OscRole::mode (processorRef, op) != OscMode::wavetable,
                                      juce::roundToInt (read (FmOperatorInfo::prefixOf (op) + "_amp_env")));
        const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) op]) + "_tune"));

        for (auto* control : controls.all())
            control->setVisible (selected);

        if (selected)
        {
            const auto tip = isFmType (op) ? juce::String ("Envelope\nEdit it on PLAY (ENVELOPE's EDIT), or pick OP ENV for a DX7 envelope here.")
                                           : juce::String ("Envelope\nEdit it on PLAY (ENVELOPE's EDIT). Ratio tuning and OP ENV are FM / DX7's: switch this oscillator to FM / DX7 to use them.");
            controls.ampEnv.setTooltip (tip);
            controls.ampEnv.getComboBox().setTooltip (tip);
        }
        controls.ratio.setVisible (selected && tune == OscTuning::Ratio);
        controls.snap.setVisible (selected && tune == OscTuning::Ratio);
        controls.fixedHz.setVisible (selected && tune == OscTuning::Fixed);
        // One level name (I6-8, I8-1; review 9, I9-7): LEVEL is the
        // oscillator's level (%) on every patch and page; on the
        // Operator Env its output level is OUTPUT (dB, the DX7's OUTPUT
        // LEVEL), as on PLAY and OSC; the envelope's own scaling
        // replaces KEY SCALE.
        controls.keyLevel.setVisible (selected && ! opEnv);
        controls.egOut.setVisible (selected && opEnv);
        if (const auto name = juce::String (OscRole::outputKnobName (processorRef, op)); controls.egOut.getLabelText() != name)
            controls.egOut.setLabelText (name);
        // One level on an operator (review 10, I10-1): OUTPUT. The
        // oscillator's own level (VOICE LEVEL) is on the OSC page.
        controls.level.setVisible (selected && ! opEnv);
    }

    envelope.setVisible (opEnv);

    // Another envelope's graph for an operator not on the Operator Env
    // (none for MSEG, which has no stages to draw).
    const auto ampEnvelope = opEnv ? -1 : juce::roundToInt (read (FmOperatorInfo::prefixOf (selectedOperator) + "_amp_env"));
    const auto graphed = ampEnvelope >= 0 && ampEnvelope < 16 ? ampEnvelope : -1;
    if (graphed != ampGraphEnvelope || selectedOperator != ampGraphOperator)
    {
        ampGraphEnvelope = graphed;
        ampGraphOperator = selectedOperator;
        ampGraph.reset();
        if (graphed >= 0)
        {
            static const char* const prefixes[] { "amp", "fe", "f2e", "me", "e4" };
            const auto prefix = graphed < 5 ? juce::String (prefixes[graphed]) : "env" + juce::String (graphed + 1);
            ampGraph = std::make_unique<EnvelopeDisplay> (processorRef, prefix, FmDiagram::oscColour (selectedOperator));
            addAndMakeVisible (*ampGraph);
        }
    }
}

void FmPage::setOperatorCardStyle (OperatorControls& controls, bool grid)
{
    for (auto* knob : { &controls.ratio, &controls.fixedHz, &controls.semi, &controls.fine, &controls.level,
                        &controls.keyLevel, &controls.feedback })
    {
        knob->setInlineKnob (grid);
        if (! grid)
            knob->setPackedDial (0.0f); // (the grid's are PLAY's rings: layoutPlainOperator)
    }
    controls.feedbackType.setCompactLayout (grid);
}

void FmPage::layoutPlainOperator (OperatorControls& controls)
{
    for (auto& each : operators)
    {
        setOperatorCardStyle (*each, true);
        each->feedbackType.setCompactLayout (false); // (a menu in the menus' row, the same look)
        each->feedbackType.setLabelText ("FEEDBACK");
        for (auto* knob : { &each->ratio, &each->fixedHz, &each->semi, &each->fine, &each->egOut, &each->feedback })
            knob->setSizeRole (IlanaTheme::KnobSize::main);
    }

    auto inner = operatorCard.reduced (10, 0);
    inner.removeFromTop (30);
    inner.removeFromBottom (8);
    // TUNING, SNAP, ENVELOPE and FB TYPE in one row of menus, the knobs in one row under them, the hint at the
    // foot; the envelope's graph fills the height at the right.
    // One even column grid for the knobs: a single row of five (a Semitones oscillator: SEMI, FINE, LEVEL, KEY SCALE,
    // FEEDBACK), or three over three when RATIO / FIXED makes six. The width a row of five does not need goes to the
    // envelope's graph.
    const auto sixKnobs = juce::roundToInt (read (OscillatorIds::prefixes[(size_t) selectedOperator] + juce::String ("_tune"))) != OscTuning::Semitones;
    const auto gridWidth = juce::jmin (sixKnobs ? 340 : 5 * 62 + 10, inner.getWidth() * 62 / 100);
    auto left = inner.removeFromLeft (gridWidth);
    inner.removeFromLeft (10);
    if (ampGraph != nullptr)
        ampGraph->setBounds (inner);

    const auto tune = juce::roundToInt (read (OscillatorIds::prefixes[(size_t) selectedOperator] + juce::String ("_tune")));
    std::vector<juce::Component*> menus { &controls.tune };
    if (tune == OscTuning::Ratio)
        menus.push_back (&controls.snap);
    menus.push_back (&controls.ampEnv);
    menus.push_back (&controls.feedbackType);

    std::vector<juce::Component*> knobsInRow;
    if (tune == OscTuning::Ratio)
        knobsInRow.push_back (&controls.ratio);
    if (tune == OscTuning::Fixed)
        knobsInRow.push_back (&controls.fixedHz);
    for (auto* knob : { &controls.semi, &controls.fine, &controls.level, &controls.keyLevel, &controls.feedback })
        knobsInRow.push_back (knob);

    constexpr int menuHeight = 40, gap = 6, ringRowHeight = 112, gridRowHeight = 100;
    const auto count = (int) knobsInRow.size();
    const auto twoRows = count > 5 && left.getHeight() - menuHeight - gap >= 140;
    const auto columns = twoRows ? 3 : count;
    const auto rowsHere = twoRows ? (count + 2) / 3 : 1;
    // The menus and the dials are one block, centred in the left area (height and width), so the room around it is even.
    const auto knobsHeight = twoRows ? rowsHere * gridRowHeight : ringRowHeight;
    const auto cellWidth = left.getWidth() / juce::jmax (1, columns);
    const auto blockWidth = cellWidth * columns;
    auto block = left.withSizeKeepingCentre (left.getWidth(), juce::jmin (left.getHeight(), menuHeight + gap + knobsHeight));
    const auto menuRow = block.removeFromTop (menuHeight).withSizeKeepingCentre (blockWidth, menuHeight);
    block.removeFromTop (gap);
    block = block.withSizeKeepingCentre (blockWidth, block.getHeight());
    const auto cellHeight = twoRows ? gridRowHeight : block.getHeight();
    auto nameUnits = 0.0f;
    for (int i = 0; i < count; ++i)
        if (auto* knob = dynamic_cast<KnobControl*> (knobsInRow[(size_t) i]))
        {
            knob->placeAsRing (juce::Rectangle<int> (block.getX() + (i % columns) * cellWidth, block.getY() + (i / columns) * cellHeight,
                                                     cellWidth, cellHeight).reduced (2, 0), twoRows ? 46 : count <= 5 ? 60 : 44);
            nameUnits = nameUnits > 0.0f ? nameUnits : knob->packedNameUnits();
        }
    // The menus as SEQ GENERATE's (UI polish 2026-11-26): wells on one line over the rings, the same left and right
    // edges, each name under its well at the rings' name size.
    const auto menuW = menuRow.getWidth() / juce::jmax (1, (int) menus.size());
    for (size_t i = 0; i < menus.size(); ++i)
    {
        const auto cell = menuRow.withX (menuRow.getX() + (int) i * menuW).withWidth (menuW).reduced (3, 0); // off-grid (SEQ's cells' 3 px a side)
        if (auto* menu = dynamic_cast<ComboControl*> (menus[i]))
            menu->setNameBelow (nameUnits, 3);
        menus[i]->setBounds (cell);
    }
    // Another type's way to the operator's controls: on the title line, left of the oscillator picker.
    if (makeOperator.isVisible())
    {
        const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont()), makeOperator.getButtonText()) + 26;
        const auto line = operatorCard.reduced (12, 0).withHeight (26).withSizeKeepingCentre (operatorCard.getWidth() - 24, 22);
        makeOperator.setBounds (juce::Rectangle<int> (tabsLeft - Space::s12 - width, line.getY(), width, line.getHeight()));
    }
}

void FmPage::layoutOperatorCard ()
{
    auto inner = operatorCard.reduced (10, 0);

    // The header line: the picker right-aligned, the link past it.
    {
        auto tabs = operatorCard.reduced (12, 0).withHeight (26).withSizeKeepingCentre (operatorCard.getWidth() - 24, 22);
        if (pitchLfoLink.isVisible())
        {
            const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont()), pitchLfoLink.getButtonText()) + 26;
            pitchLfoLink.setBounds (tabs.removeFromRight (width).withSizeKeepingCentre (width, 22));
            tabs.removeFromRight (12);
        }
        // Names while the title and a line about the operator fit
        // beside them, else just the numbers.
        const auto room = tabs.getWidth() - 110; // (review 11, I11-2: OSC n, not a bare number)
        // Every operator always shows: the line about it gives way.
        const auto width = juce::jmin (tabs.getWidth(), picker.getQuietWidth() <= room ? picker.getQuietWidth() : picker.getShortWidth());
        picker.setBounds (tabs.removeFromRight (width));
        tabsLeft = picker.getX();
    }
    inner.removeFromTop (30);
    inner.removeFromBottom (6);

    auto& controls = *operators[(size_t) selectedOperator];
    const auto opEnv = usesOperatorEnv (selectedOperator);
    for (auto& each : operators)
        setOperatorCardStyle (*each, false);
    if (! opEnv)
    {
        for (auto& each : operators)
        {
            each->ampEnv.setPackedMenu (false);
            each->ampEnv.setLabelText (FmOperatorInfo::envelopeLabel);
        }
        layoutPlainOperator (controls);
        return;
    }
    layoutOperatorEnvCard (controls, inner);
}

void FmPage::layoutOperatorEnvCard (OperatorControls& controls, juce::Rectangle<int> inner)
{
    for (auto& each : operators)
    {
        setOperatorCardStyle (*each, false);
        for (auto* menu : { &each->tune, &each->snap, &each->ampEnv, &each->feedbackType })
            menu->setNameBelow (0.0f, 0); // (the plain card's names under the wells go back above)
        each->feedbackType.setCompactLayout (false);
        each->feedbackType.setLabelText ("TYPE"); // (beside FEEDBACK's knob; the tooltip names it in full)
        each->ampEnv.setPackedMenu (false);
        each->ampEnv.setLabelText ({}); // (just the box: the panel's first line writes ENVELOPE before it)
        // One dial size across the card: the stage knobs' (the left column's cells are wider, so they would grow).
        for (auto* knob : { &each->ratio, &each->fixedHz, &each->semi, &each->fine, &each->egOut, &each->feedback })
            knob->setSizeRole (IlanaTheme::KnobSize::mini);
    }

    const auto leftWidth = juce::jlimit (200, 250, inner.getWidth() * 33 / 100);
    auto left = inner.removeFromLeft (leftWidth);
    inner.removeFromLeft (15);
    envelope.setBounds (inner);
    // The ENVELOPE menu at the right of the panel's first line (the editor leaves the room).
    {
        auto room = envelope.getHostArea().translated (envelope.getX(), envelope.getY());
        const auto width = juce::jmin (room.getWidth(), OperatorEnvEditor::envelopeMenuWidth);
        controls.ampEnv.setBounds (room.removeFromRight (width).withSizeKeepingCentre (width, 24));
        controls.ampEnv.toFront (false); // (over the envelope's panel)
    }

    const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) selectedOperator]) + "_tune"));
    auto menus = left.removeFromTop (40);
    std::vector<juce::Component*> menuRow { &controls.tune, tune == OscTuning::Ratio ? static_cast<juce::Component*> (&controls.snap) : nullptr };
    layoutRow (menus, menuRow);

    std::vector<juce::Component*> dials;
    if (tune == OscTuning::Ratio)
        dials.push_back (&controls.ratio);
    if (tune == OscTuning::Fixed)
        dials.push_back (&controls.fixedHz);
    for (auto* knob : { &controls.semi, &controls.fine, &controls.egOut, &controls.feedback })
        dials.push_back (knob);
    std::vector<juce::Component*> rowOne, rowTwo;
    for (size_t i = 0; i < dials.size(); ++i)
        (i < 3 ? rowOne : rowTwo).push_back (dials[i]);
    rowTwo.push_back (&controls.feedbackType);
    rowOne.resize (3, nullptr);
    rowTwo.resize (3, nullptr);
    // (Two equal bands under the menus: each row sits centred in its band, so the gaps are even.)
    // (UI review 31, 17: PLAY's rings, the value inside, as the plain card and the stage knobs beside them.)
    const auto band = left.getHeight() / 2;
    const auto placeRow = [] (juce::Rectangle<int> rowArea, const std::vector<juce::Component*>& items)
    {
        const auto width = rowArea.getWidth() / juce::jmax (1, (int) items.size());
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto cellArea = juce::Rectangle<int> (rowArea.getX() + (int) i * width, rowArea.getY(), width, rowArea.getHeight());
            if (auto* knob = dynamic_cast<KnobControl*> (items[i]))
                knob->placeAsRing (cellArea.reduced (2, 0).withSizeKeepingCentre (cellArea.getWidth() - 4, juce::jmin (cellArea.getHeight(), OperatorEnvEditor::ringRowHeight)),
                                   OperatorEnvEditor::ringDial);
            else if (items[i] != nullptr)
                layoutRow (cellArea, { items[i] }, true);
        }
    };
    placeRow (left.removeFromTop (band), rowOne);
    placeRow (left, rowTwo);
}

auto FmPage::activeRoutes () const
     -> std::vector<Route>
{
    std::vector<Route> routes;
    for (const auto target : shown)
    {
        for (const auto source : shown)
            if (cellAmount (source, target) > 0.0005f)
                routes.push_back ({ source, target });
        if (cellAmount (-1, target) > 0.0005f)
            routes.push_back ({ -1, target });
    }
    // Into OSC 1 first, then OSC 2, and so on; within a target, the modulators in order, feedback and noise last.
    return routes;
}

juce::String FmPage::routesSignature () const
{
    juce::String signature;
    for (const auto& route : activeRoutes())
        signature << route.source << ">" << route.target << (isLiveCell (route.source, route.target) ? "" : "x") << ",";
    return signature;
}

juce::String FmPage::routeName (const Route& route) const
{
    const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");
    const auto from = route.source < 0 ? juce::String ("NOISE") : "OSC " + juce::String (route.source + 1);
    return from + arrow + (route.source == route.target ? juce::String ("itself") : "OSC " + juce::String (route.target + 1));
}

juce::TextButton& FmPage::removeButtonFor (const Route& route)
{
    auto& button = removeButtons[{ route.source, route.target }];
    if (button == nullptr)
    {
        button = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text3);
        button->setName ("removeRoute");
        button->setTooltip ("Remove this route");
        button->onClick = [this, route]
        {
            const auto id = route.source < 0 ? "fm_noise" + juce::String (route.target + 1) : FmDiagram::routeId (route.source, route.target);
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                processorRef.getUndoManager().beginNewTransaction ("Remove FM route");
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (0.0f);
                parameter->endChangeGesture();
            }
        };
        addChildComponent (*button);
    }
    return *button;
}

void FmPage::showAddRouteMenu ()
{
    juce::PopupMenu menu;
    juce::Component::SafePointer<FmPage> safeThis (this);
    const auto addInto = [safeThis] (int source, int target)
    {
        if (safeThis == nullptr)
            return;
        const auto id = source < 0 ? "fm_noise" + juce::String (target + 1) : FmDiagram::routeId (source, target);
        if (auto* parameter = safeThis->processorRef.apvts.getParameter (id))
        {
            safeThis->processorRef.getUndoManager().beginNewTransaction ("Add FM route");
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (0.5f);
            parameter->endChangeGesture();
        }
    };
    for (int source = -1; source < OscillatorIds::count; ++source)
    {
        if (source >= 0 && std::find (shown.begin(), shown.end(), source) == shown.end())
            continue;
        juce::PopupMenu into;
        for (const auto target : shown)
        {
            const auto name = source == target ? juce::String ("itself (feedback)") : "OSC " + juce::String (target + 1);
            const auto used = cellAmount (source, target) > 0.0005f;
            into.addItem (name, isLiveCell (source, target) && ! used, used, [addInto, source, target] { addInto (source, target); });
        }
        menu.addSubMenu (source < 0 ? juce::String ("NOISE into") : "OSC " + juce::String (source + 1) + " into", into);
    }
    menu.showMenuAsync (IlanaTheme::menuOptions (addRouteButton));
}

void FmPage::layoutRoutes ()
{
    lastRoutesSignature = routesSignature();
    auto inner = matrixCard.reduced (12, 0);
    inner.removeFromTop (30);
    inner.removeFromBottom (10);

    for (int source = 0; source < OscillatorIds::count; ++source)
    {
        outs[(size_t) source]->setVisible (false);
        noiseKnobs[(size_t) source]->setVisible (false);
        for (auto& knob : knobs[(size_t) source])
            knob->setVisible (false);
    }
    for (auto& entry : removeButtons)
        entry.second->setVisible (false);
    moreButton.setVisible (false);

    // From the top (as the mock-up ilana okayed): the list, + ADD ROUTE, FM MODE, then the extras.
    routeRows.clear();
    // (UI review 31, 17: rows close to the MATRIX page's, 32 for PLAY's 28 ring with its value inside, so the OUTPUT graph
    // under them keeps its height.)
    constexpr int rowHeight = 32, routeDial = 28, extrasRowHeight = 58, titleHeight = 18;
    const auto routes = activeRoutes();
    const auto roomForRows = juce::jmax (0, (inner.getHeight() - 26 - 14 - 44 - 18 - titleHeight - extrasRowHeight) / rowHeight);
    for (size_t i = 0; i < routes.size() && (int) i < roomForRows; ++i)
    {
        const auto& route = routes[i];
        auto row = inner.removeFromTop (rowHeight);
        routeRows.push_back ({ route, row });
        auto& knob = cellKnob (route.source, route.target);
        knob.setVisible (true);
        knob.setCompact (false);
        knob.setPackedDial ((float) routeDial);
        auto remove = row.removeFromRight (22);
        auto& button = removeButtonFor (route);
        button.setVisible (true);
        button.setBounds (remove.withSizeKeepingCentre (22, 22));
        knob.setBounds (KnobControl::packedBoundsFor (row.removeFromRight (cell).withSizeKeepingCentre (cell, routeDial + 10).translated (0, 5)));
    }
    inner.removeFromTop (4);
    addRouteButton.setBounds (inner.removeFromTop (26).withWidth (juce::jmin (inner.getWidth(), 130)));
    // (Polish pass 6: FM MODE and EXTRAS stack right under the list, a 10 px gap each; OUTPUT takes what is left.)
    inner.removeFromTop (routes.empty() ? 32 : 10);

    mode.setBounds (inner.removeFromTop (38));
    inner.removeFromTop (10);

    extrasTitle = inner.removeFromTop (titleHeight);
    auto extras = inner.removeFromTop (extrasRowHeight);
    // (Cells as wide as what they hold: SYNC 2 TO 1's name and the noise type's longest word.)
    constexpr std::array<float, 4> weights { 0.9f, 1.3f, 1.1f, 0.9f };
    const auto unit = (float) extras.getWidth() / (weights[0] + weights[1] + weights[2] + weights[3]);
    const auto cellAt = [&] (int i)
    {
        auto x = (float) extras.getX();
        for (int k = 0; k < i; ++k)
            x += weights[(size_t) k] * unit;
        return juce::Rectangle<int> (juce::roundToInt (x), extras.getY(), juce::roundToInt (weights[(size_t) i] * unit), extras.getHeight());
    };
    ringMod->setVisible (true);
    hardSync.setVisible (true);
    noiseTypeMenu.setVisible (true);
    noiseColourKnob->setVisible (true);
    ringMod->setCompact (false);
    ringMod->placeAsRing (cellAt (0), 30);
    noiseColourKnob->setCompact (false);
    noiseColourKnob->placeAsRing (cellAt (3), 30);
    // One row like the knobs': the switch and the menu on the rings' middle line, every name on the
    // knob names' line.
    const auto middle = ringMod->getY() + juce::roundToInt (ringMod->getDialCentre().y);
    const auto nameTop = ringMod->getY() + ringMod->getPackedNameArea().getY();
    hardSync.setPackedName (ToggleControl::NamePlace::below);
    hardSync.setBelowNameSize (ringMod->packedNameUnits());
    hardSync.setBounds (cellAt (1).withTop (middle - 7).withBottom (extras.getBottom()));
    hardSync.setNameDrop (juce::jmax (0, nameTop - (middle - 7 + 15 + 1)));
    noiseTypeMenu.setBounds (cellAt (2).withSizeKeepingCentre (cellAt (2).getWidth() - 6, 24).withY (middle - 12));
    noiseTypeTitle = cellAt (2).withTop (nameTop).withHeight (14);

    // The rest: the sound's wave and spectrum, live, so a route's depth shows as sidebands as it turns (no bare
    // foot under a short list).
    inner.removeFromTop (10);
    outputTitle = inner.removeFromTop (titleHeight);
    outputView.setVisible (inner.getHeight() >= 70);
    outputView.setBounds (inner.withTrimmedTop (4));
    if (! outputView.isVisible())
        outputTitle = {};
    refreshCellKnobs (true);
}

void FmPage::layoutCableKnobs ()
{
    for (int source = 0; source < OscillatorIds::count; ++source)
        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            auto& knob = *cableKnobs[(size_t) source][(size_t) target];
            const auto handle = diagram.getRouteHandle (source, target);
            knob.setVisible (handle.has_value() && isVisible());
            if (! handle.has_value())
                continue;
            knob.setEnabled (isLiveCell (source, target));
            const auto centre = diagram.getPosition().toFloat() + *handle;
            const auto bounds = KnobControl::packedBoundsFor (juce::Rectangle<int> (cell, 30 + 8).withCentre (centre.toInt().translated (0, 4)));
            if (knob.getBounds() != bounds)
                knob.setBounds (bounds);
            knob.toFront (false);
        }
}

void FmPage::paintRoutes (juce::Graphics& g)
{
    const auto routes = activeRoutes();
    IlanaTheme::paintCardHeader (g, matrixCard.reduced (12, 0).withHeight (26), "ROUTES",
                                 routes.empty() ? juce::String ("none yet") : juce::String ((int) routes.size()) + (routes.size() == 1 ? " route" : " routes"),
                                 fmColour(), 0);
    for (const auto& [route, row] : routeRows)
    {
        const auto live = isLiveCell (route.source, route.target);
        const auto colour = route.source < 0 ? noiseColour() : FmDiagram::oscColour (route.source);
        g.setColour (colour.withAlpha (live ? 1.0f : 0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, (float) row.getHeight() - 12.0f).withPosition ((float) row.getX(), (float) row.getY() + 6.0f), 1.5f);
        g.setColour (live ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        IlanaTheme::drawFitted (g, routeName (route), row.withTrimmedLeft (12).withTrimmedRight (22 + cell + 4), juce::Justification::centredLeft, 1);
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
    }
    if (routes.empty())
    {
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        IlanaTheme::drawFitted (g, noRoutesText(), addRouteButton.getBounds().translated (0, 28).withWidth (matrixCard.getWidth() - 24).withHeight (32),
                                juce::Justification::topLeft, 2);
    }
    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true).withKerningFactor (0.08f));
    g.drawText ("EXTRAS", extrasTitle, juce::Justification::bottomLeft, false);
    g.drawText ("OUTPUT", outputTitle, juce::Justification::bottomLeft, false);
    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (ringMod->packedNameUnits(), true));
    g.drawText ("NOISE TYPE", noiseTypeTitle, juce::Justification::centredTop, false);
}

bool FmPage::matrixExtrasInUse () const
{
    auto used = read ("ring_mod") > 0.0005f || read ("hard_sync") > 0.5f;
    for (const auto osc : shown)
        used = used || read ("fm_noise" + juce::String (osc + 1)) > 0.0005f;
    return used;
}

InputPage::InputPage (IlanaSynthAudioProcessor& p)
    : processorRef (p),
      gain (p.apvts, "in_gain", "GAIN", inputColour(), false),
      dry (p.apvts, "in_dry", "DRY", inputColour(), false),
      trigger (p.apvts, "in_trigger", "TRIGGER"),
      threshold (p.apvts, "in_threshold", "THRESHOLD", inputColour(), false),
      note (p.apvts, "in_note", "NOTE", inputColour(), false),
      attack (p.apvts, "in_attack", "ATTACK", inputColour(), false),
      release (p.apvts, "in_release", "RELEASE", inputColour(), false),
      toBody (p.apvts, "in_body", "TO BODY", routeColour(), false),
      toStrings (p.apvts, "in_strings", "TO STRINGS", routeColour(), false)
{
    addAll (*this, gain, dry, trigger, threshold, note, attack, release, toBody, toStrings);
    const char* names[] { "Live Body", "Live Wah", "Live Grains", "Live Strings" };
    const char* tips[] { "The input rings a metal body (BODY section)",
                         "A Live oscillator through a filter the input's envelope opens",
                         "A granular oscillator reading the last seconds of the input",
                         "The input keeps two strings ringing, E and B" };
    for (int i = 0; i < 4; ++i)
    {
        auto& button = quickStarts[(size_t) i];
        button.setButtonText (juce::String (names[i]).toUpperCase());
        button.setTooltip (tips[i]);
        button.onClick = [this, name = juce::String (names[i])]
        {
            const auto index = processorRef.getFactoryPresetNames().indexOf (name);
            if (index >= 0)
                processorRef.loadFactoryPreset (index);
        };
        addAndMakeVisible (button);
    }
    startTimerHz (30);
}

void InputPage::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());
    const auto title = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour)
    {
        IlanaTheme::paintTag (g, { (float) area.getX() + 3.0f, (float) area.getCentreY() }, colour);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (text, area.withTrimmedLeft (14), juce::Justification::centredLeft);
    };
    IlanaTheme::paintCard (g, inputCard.toFloat(), 7.0f, inputColour().withAlpha (0.35f));
    IlanaTheme::paintCard (g, routeCard.toFloat(), 7.0f, routeColour().withAlpha (0.35f));
    IlanaTheme::paintCard (g, startCard.toFloat(), 7.0f, IlanaTheme::accent().withAlpha (0.35f));
    title (inputCard.reduced (12, 0).removeFromTop (26), "INPUT", inputColour());
    title (routeCard.reduced (12, 0).removeFromTop (26), "ROUTING", routeColour());
    title (startCard.reduced (12, 0).removeFromTop (26), "QUICK START", IlanaTheme::accent());

    // Level (peak) and envelope, with the gate threshold marked.
    IlanaTheme::paintWell (g, meter.toFloat(), 5.0f);
    const auto bar = [this, &g] (juce::Rectangle<float> area, float value, juce::Colour colour, const juce::String& label)
    {
        const auto db = value > 1.0e-5f ? juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (value) / 60.0f) : 0.0f;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (area.withWidth (area.getWidth() * db), 3.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (label, area.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        juce::ignoreUnused (this);
    };
    auto area = meter.toFloat().reduced (10.0f, 8.0f);
    const auto rowHeight = (area.getHeight() - 6.0f) / 2.0f;
    const auto levelRow = area.removeFromTop (rowHeight);
    area.removeFromTop (6.0f);
    bar (levelRow, shownLevel, inputColour().withAlpha (0.8f), "LEVEL");
    bar (area, shownEnvelope, routeColour().withAlpha (0.8f), "ENVELOPE (Input Env in the matrix)");
    if (readChoice ("in_trigger") == 1)
    {
        const auto thresholdDb = processorRef.apvts.getRawParameterValue ("in_threshold")->load();
        const auto x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, 1.0f + thresholdDb / 60.0f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (x - 1.0f, levelRow.getY(), 2.0f, area.getBottom() - levelRow.getY());
    }

    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    const juce::String help[] {
        "TRIGGER: Off plays only on MIDI notes; Gate plays NOTE while the input is over THRESHOLD; Drone holds NOTE down.",
        "OSC: set an oscillator's MODE to Live to play the input through the filters, FM and effects,",
        "or turn on LIVE in Granular mode to granulate its last three seconds (POSITION is how far back).",
        "BODY: TO BODY rings the BODY section (switch it on in FILTER). TO STRINGS drives Physical strings, tines and reeds.",
        "DRY adds the untouched input back at the end. Input Env modulates anything from the MATRIX."
    };
    auto text = helpArea;
    for (const auto& line : help)
        g.drawText (line, text.removeFromTop (20), juce::Justification::centredLeft);
}

void InputPage::resized ()
{
    auto area = getLocalBounds().reduced (12);
    auto top = area.removeFromTop (juce::jmin (230, area.getHeight() / 2));
    inputCard = top.removeFromLeft (top.getWidth() * 3 / 5).reduced (0, 0);
    top.removeFromLeft (10);
    routeCard = top;
    area.removeFromTop (10);
    startCard = area;

    auto inside = inputCard.reduced (12, 0);
    inside.removeFromTop (28);
    meter = inside.removeFromTop (58);
    inside.removeFromTop (8);
    auto knobs = inside.removeFromTop (juce::jmin (100, inside.getHeight() - 6));
    const auto width = knobs.getWidth() / 7;
    for (auto* control : std::initializer_list<juce::Component*> { &gain, &trigger, &threshold, &note, &attack, &release, &dry })
        control->setBounds (knobs.removeFromLeft (width).reduced (3, 0));

    auto routes = routeCard.reduced (12, 0);
    routes.removeFromTop (34);
    routes = routes.removeFromTop (juce::jmin (110, routes.getHeight()));
    toBody.setBounds (routes.removeFromLeft (routes.getWidth() / 2).reduced (6, 0));
    toStrings.setBounds (routes.reduced (6, 0));

    auto start = startCard.reduced (14, 0);
    start.removeFromTop (34);
    auto buttons = start.removeFromTop (34);
    const auto buttonWidth = buttons.getWidth() / 4;
    for (auto& button : quickStarts)
        button.setBounds (buttons.removeFromLeft (buttonWidth).reduced (4, 2));
    start.removeFromTop (14);
    helpArea = start;
}

void InputPage::timerCallback ()
{
    if (! isVisible())
        return;

    const auto level = processorRef.getInputLevel();
    const auto envelope = processorRef.getInputEnvelope();
    if (std::abs (level - shownLevel) > 1.0e-4f || std::abs (envelope - shownEnvelope) > 1.0e-4f)
    {
        shownLevel = level;
        shownEnvelope = envelope;
        repaint (meter);
    }
}

int InputPage::readChoice (const juce::String& id) const
{
    const auto* value = processorRef.apvts.getRawParameterValue (id);
    return value != nullptr ? juce::roundToInt (value->load()) : 0;
}
