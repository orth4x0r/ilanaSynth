// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// FM between six oscillators: the algorithms, the operator diagram and the
// selected operator's settings on the left; the full matrix of amounts on the
// right (rows = from, columns = to, plus the noise operator), with the FM
// style and, under it, OSC 1 and OSC 2's ring mod and hard sync.
//
// An oscillator on the Operator Env (a DX7 voice's operators) shows its
// envelope as a graph you drag, in the synth's words and units: ATTACK to
// RELEASE in ms, PEAK to END in dB, its output as LEVEL (UI review 6, I6-7 to
// I6-9). The envelope is the one editor MOD's OP ENV card shows too
// (OperatorEnvEditor, UI review 8, I8-6). The voice's pitch envelope and LFO
// are MOD's OP PITCH and OP LFO; the card's header links there (I8-8, S8-3,
// V8-21).
class FmPage : public juce::Component,
               private IlanaAnim::FrameTimer
{
    // One operator's settings: its tuning and feedback style, its ENVELOPE,
    // and its pitch and levels in one order on every page (RATIO, SEMI,
    // FINE, OUTPUT, LEVEL: UI review 8, V8-5). LEVEL is always the
    // oscillator's level (%); on the Operator Env, OUTPUT is its output level
    // (dB: review 9, I9-7, was LEVEL beside a TRIM); otherwise KEY SCALE is
    // its key scaling (KEY LVL until review 9, I9-26).
    struct OperatorControls
    {
        OperatorControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefixIn, juce::Colour colour, int index)
            : prefix (prefixIn),
              tune (state, prefix + "_tune", "TUNING"),
              snap (state, prefix + "_ratio_snap", "SNAP"),
              feedbackType (state, prefix + "_fb_type", "FB TYPE"),
              ampEnv (state, prefix + "_amp_env", FmOperatorInfo::envelopeLabel),
              ratio (state, prefix + "_ratio", "RATIO", colour, false),
              fixedHz (state, prefix + "_fixed_hz", "FIXED", colour, false),
              semi (state, prefix + "_semi", "SEMI", colour, false),
              fine (state, prefix + "_fine", "FINE", colour, false),
              level (state, prefix + "_level", "LEVEL", colour, false),
              keyLevel (state, prefix + "_key_level", "KEY SCALE", colour, false),
              egOut (state, prefix + "_eg_out", "OUTPUT", colour, false),
              // The operator's feedback, beside FB TYPE (it is also the matrix
              // diagonal's cell: review 10, I10-4).
              feedback (state, FmDiagram::routeId (index, index), "FEEDBACK", colour, false)
        {
            FmOperatorInfo::sectionEnvelopeMenu (ampEnv.getComboBox());
        }

        juce::String prefix;
        ComboControl tune, snap, feedbackType, ampEnv;
        KnobControl ratio, fixedHz, semi, fine, level, keyLevel, egOut, feedback;

        std::vector<juce::Component*> all()
        {
            return { &tune, &snap, &feedbackType, &ampEnv, &ratio, &fixedHz, &semi, &fine, &level, &keyLevel, &egOut, &feedback };
        }
    };

public:
    explicit FmPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          diagram (p),
          algorithms (p),
          envelope (p, OperatorEnvEditor::Place::fmCard),
          pageTabs ({ "BASIC", "DX7 1-16", "DX7 17-32" }, { fmColour(), fmColour(), fmColour() }, false),
          mode (p.apvts, "fm_mode", "FM MODE"),
          hardSync (p.apvts, "hard_sync", "SYNC 2 TO 1"),
          effectRules (p)
    {
        addAll (*this, diagram, algorithms, pageTabs, mode, hardSync);
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

            outs[(size_t) source] = std::make_unique<ToggleControl> (
                p.apvts, juce::String (OscillatorIds::prefixes[(size_t) source]) + "_out", "OUT");
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

        // The same heading as the open section ("EXTRAS", at the matrix's
        // foot) whatever the operator count (review 10, I10-5).
        moreButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("EXTRAS  \xc2\xb7  RING MOD  \xc2\xb7  SYNC  \xc2\xb7  NOISE FM  \xe2\x80\xba")));
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
            for (const auto* suffix : { "_tune", "_amp_env", "_on" })
                tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + suffix));

        // The operator picker (the one oscillator picker, I8-10) and, past
        // it, the link to the voice's OP PITCH and OP LFO on MOD: a link,
        // not a seventh operator tab (V8-21), there only while an oscillator
        // plays the Operator Env.
        picker.onPick = [this] (int op) { selectOperator (op); };
        addAndMakeVisible (picker);
        styleFmLink (pitchLfoLink, juce::String::fromUTF8 ("OP PITCH \xc2\xb7 OP LFO"));
        pitchLfoLink.setTooltip ("The Operator Env's pitch envelope (OP PITCH) and LFO (OP LFO), for the whole voice: every "
                                 "oscillator on the Operator Env follows them. Edited in MOD's pools, with TRANSPOSE and SCALE SHIFT.");
        pitchLfoLink.onClick = [] { FmOperatorInfo::openPitchAndLfo(); };
        addChildComponent (pitchLfoLink);
        addChildComponent (envelope);

        refreshShown();
        refreshFmInputs (true);
        selectOperator (shown.empty() ? 0 : shown.front());
        startTimerHz (12);
    }

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }
    static juce::Colour noiseColour() { return IlanaTheme::Ui::text2; }

    int getSelectedOperator() const { return selectedOperator; }

    void selectOperator (int op)
    {
        selectedOperator = juce::jlimit (0, OscillatorIds::count - 1, op);
        picker.setSelectedOsc (selectedOperator);
        envelope.selectOperator (selectedOperator);
        updateOperatorVisibility();
        resized();
        repaint();
    }

    const FmDiagram& getDiagram() const { return diagram; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        paintSectionTitle (g, "ALGORITHMS", algorithmsTitle.withTrimmedRight (pageTabs.getWidth() + 8),
                           hoverAlgorithm.isNotEmpty() ? hoverAlgorithm : getAlgorithmLabel());
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        // The selected operator's settings, the picker on the header line.
        const auto header = operatorCard.reduced (12, 0).withHeight (26);
        const auto reserve = operatorCard.getRight() - tabsLeft + 8;
        const auto colour = FmDiagram::oscColour (selectedOperator);
        IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
        IlanaTheme::paintCardHeader (g, header, "OSC " + juce::String (selectedOperator + 1), operatorText(), colour, reserve);

        // An operator on another envelope: where that envelope is edited.
        if (! usesOperatorEnv (selectedOperator))
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            const auto envelopeName = operators[(size_t) selectedOperator]->ampEnv.getComboBox().getText();
            IlanaTheme::drawFitted (g, "Edit " + envelopeName + " on MOD or PLAY, or pick OP ENV for a DX7 envelope here.",
                                    ampHint, juce::Justification::centredLeft, 1);
        }

        paintMatrix (g);
    }

    void visibilityChanged() override
    {
        if (isVisible())
            algorithms.refreshMatch();
    }

    // The matrix follows the oscillators added to the patch.
    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
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

        const auto inputsChanged = refreshFmInputs (false);
        const auto cellsChanged = refreshCellKnobs();
        if (inputsChanged || cellsChanged)
            repaint (matrixCard);
        if (inputsChanged)
            updateOperatorVisibility();

        if (refreshShown() || tuneChanged || diagram.getMinimumHeight() != lastDiagramMinimum
            || anyOperatorEnv() != lastAnyOperatorEnv || matrixExtrasShown() != lastExtrasShown)
        {
            updateOperatorVisibility();
            resized();
            repaint();
        }
        else if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
        {
            repaint (matrixCard);
            repaint (operatorCard.withHeight (26));
        }
    }

    // Columns of oscillators that ignore FM (sample, granular, a string
    // not set to Osc In) are greyed out and can't be edited; routes already
    // there stay in the patch but do nothing. A switched-off oscillator's
    // row and column are greyed the same way (I6-37, V6-13).
    bool refreshFmInputs (bool force)
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

    bool refreshShown()
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

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        // The matrix as wide as its cells (one cell size for every patch,
        // V8-35), at least as wide as its FM MODE line; the rest goes to
        // the diagram and the operator card.
        const auto count = juce::jmax (1, (int) shown.size());
        matrixCard = area.removeFromRight (juce::jlimit (minimumMatrixWidth, area.getWidth() * 44 / 100,
                                                         20 + headWidth + count * cellWidth));
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
        lastAnyOperatorEnv = anyOperatorEnv();
        lastExtrasShown = matrixExtrasShown();
        operatorCard = area.removeFromBottom (juce::jmax (150, juce::jmin (operatorCardHeight, area.getHeight() - 150)));
        area.removeFromBottom (8);
        diagram.setBounds (area);

        layoutOperatorCard();
        layoutMatrix();
    }

    juce::Rectangle<int> getOperatorCardBounds() const { return operatorCard; }
    juce::Rectangle<int> getMatrixCardBounds() const { return matrixCard; }
    juce::Rectangle<int> getMatrixCellBounds (int source, int target) const
    {
        return source < 0 ? noiseCells[(size_t) target] : cells[(size_t) source][(size_t) target];
    }
    bool areMatrixExtrasShown() const { return ringMod->isVisible(); }

private:
    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

public:
    // The ALGORITHMS heading's note: the DX7 algorithm a patch matches by its
    // number, else a basic routing's name, else CUSTOM ROUTING (UI review 6,
    // I6-6 and I6-41).
    juce::String getAlgorithmLabel() const
    {
        if (const auto dx7 = processorRef.findMatchingDx7Algorithm(); dx7 > 0)
            return "DX7 ALGORITHM " + juce::String (dx7);

        const auto matching = processorRef.findMatchingFmAlgorithm();
        if (matching >= 0 && matching < FmAlgorithms::numBasic)
            return FmAlgorithmStrip::basicName (matching);
        if (matching >= 0 && matching < (int) FmAlgorithms::all().size())
            return FmAlgorithms::all()[(size_t) matching].name;

        // One that no tile matches names the nearest (V7-14).
        if (const auto near = FmAlgorithmStrip::nearestBasic (processorRef); near >= 0)
            return "CUSTOM, NEAR " + FmAlgorithmStrip::basicName (near);

        for (const auto source : shown)
            for (const auto target : shown)
                if (read (FmDiagram::routeId (source, target)) > 0.001f)
                    return "CUSTOM ROUTING";

        return {};
    }

private:
    // "x1.00 · Operator Env · OUT": the operator in a line (V6-33).
    juce::String operatorText() const
    {
        const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
        const auto prefix = FmOperatorInfo::prefixOf (selectedOperator);
        const auto tune = juce::roundToInt (read (prefix + "_tune"));
        auto text = tune == OscTuning::Ratio || tune == OscTuning::Fixed ? FmOperatorInfo::tuningText (processorRef, selectedOperator)
                                                                         : juce::String ("semitones");
        text << dot << (usesOperatorEnv (selectedOperator) ? "OP ENV" : operators[(size_t) selectedOperator]->ampEnv.getComboBox().getText());
        // OUT or MOD, as its node in the diagram says.
        text << dot << (read (prefix + "_out") > 0.5f ? "OUT" : "MOD");
        if (! FmOperatorInfo::isPlaying (processorRef, selectedOperator))
            text << dot << "off";
        return text;
    }

    bool usesOperatorEnv (int op) const { return FmOperatorInfo::usesOperatorEnv (processorRef, op); }

    bool anyOperatorEnv() const
    {
        for (const auto op : shown)
            if (usesOperatorEnv (op))
                return true;
        return false;
    }

    // A cell's knob can be edited: its source plays, and its target plays and
    // takes FM (source -1: the noise row).
    bool isLiveCell (int source, int target) const
    {
        return fmIn[(size_t) target] && playing[(size_t) target] && (source < 0 || playing[(size_t) source]);
    }

    float cellAmount (int source, int target) const
    {
        return read (source < 0 ? "fm_noise" + juce::String (target + 1) : FmDiagram::routeId (source, target));
    }

    KnobControl& cellKnob (int source, int target)
    {
        return source < 0 ? *noiseKnobs[(size_t) target] : *knobs[(size_t) source][(size_t) target];
    }

    // An empty route is a dot that turns into its knob under the mouse, so
    // the routes in use stand out from the grid (V6-15, S6-17); a dead one is
    // a faint knob or nothing.
    bool refreshCellKnobs (bool force = false)
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

    void updateOperatorVisibility()
    {
        // The picker offers the shown oscillators, lit while they play; the
        // link to OP PITCH and OP LFO shows while any plays the Operator Env.
        picker.setOscillators (shown, [this] (int op) { return playing[(size_t) op]; },
                               [] (int op) { return "Edit OSC " + juce::String (op + 1) + " as an operator"; });
        picker.setSelectedOsc (selectedOperator);
        pitchLfoLink.setVisible (anyOperatorEnv());

        const auto opEnv = usesOperatorEnv (selectedOperator);
        for (int op = 0; op < OscillatorIds::count; ++op)
        {
            auto& controls = *operators[(size_t) op];
            const auto selected = op == selectedOperator;
            const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) op]) + "_tune"));

            for (auto* control : controls.all())
                control->setVisible (selected);

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

    void layoutOperatorCard()
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
            const auto room = tabs.getWidth() - 70; // (review 11, I11-2: OSC n, not a bare number)
            // Every operator always shows: the line about it gives way.
            const auto width = juce::jmin (tabs.getWidth(), picker.getQuietWidth() <= room ? picker.getQuietWidth() : picker.getShortWidth());
            picker.setBounds (tabs.removeFromRight (width));
            tabsLeft = picker.getX();
        }
        inner.removeFromTop (30);
        inner.removeFromBottom (6);

        auto& controls = *operators[(size_t) selectedOperator];
        const auto opEnv = usesOperatorEnv (selectedOperator);
        // On the Operator Env, the envelope's editor takes the card: its
        // graph at the right, its tabs and knobs along the bottom; the
        // operator's own controls go in the space it leaves.
        if (opEnv)
        {
            envelope.setBounds (inner);
            inner = envelope.getHostArea().translated (inner.getX(), inner.getY());
        }
        else
        {
            // Its envelope's graph where the Operator Env's would be, and a
            // line under the knobs on where that envelope is edited.
            ampHint = inner.removeFromBottom (16).withTrimmedLeft (2);
            inner.removeFromBottom (4);
            const auto graph = inner.removeFromRight (inner.getWidth() / 3).withTrimmedLeft (8).reduced (0, 2);
            if (ampGraph != nullptr)
                ampGraph->setBounds (graph);
            ampHint.setRight (graph.getX() - 8);
            // The menus and knobs at their natural height, not stretched.
            inner = inner.withHeight (juce::jmin (inner.getHeight(), 46 + 4 + 96));
        }
        const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) selectedOperator]) + "_tune"));

        auto topRow = inner.removeFromTop (46);
        std::vector<juce::Component*> top { &controls.tune };
        if (tune == OscTuning::Ratio)
            top.push_back (&controls.snap);
        top.push_back (&controls.feedbackType);
        top.push_back (&controls.ampEnv);

        inner.removeFromTop (4);
        std::vector<juce::Component*> bottom;
        if (tune == OscTuning::Ratio)
            bottom.push_back (&controls.ratio);
        if (tune == OscTuning::Fixed)
            bottom.push_back (&controls.fixedHz);
        bottom.push_back (&controls.semi);
        bottom.push_back (&controls.fine);
        if (opEnv)
        {
            bottom.push_back (&controls.egOut);
        }
        else
        {
            bottom.push_back (&controls.level);
            bottom.push_back (&controls.keyLevel);
        }

        bottom.push_back (&controls.feedback);

        // Menus and knobs on a grid (as many columns as the longer row), so
        // the menus line up with the knobs; the menus' row has one column
        // fewer, since FEEDBACK (I10-4) left them too narrow for "Semitones".
        const auto columns = juce::jmax (top.size(), bottom.size());
        top.resize (juce::jmax (top.size(), columns - 1), nullptr);
        bottom.resize (columns, nullptr);
        layoutRow (topRow, top);
        layoutRow (inner, bottom);
    }

    // RING MOD, SYNC 2 TO 1 and the NOISE FM row mean nothing to a DX7
    // voice: on a patch whose oscillators play the Operator Env they fold
    // behind a MORE line unless one is in use (S8-21).
    bool matrixExtrasInUse() const
    {
        auto used = read ("ring_mod") > 0.0005f || read ("hard_sync") > 0.5f;
        for (const auto osc : shown)
            used = used || read ("fm_noise" + juce::String (osc + 1)) > 0.0005f;
        return used;
    }

    bool matrixExtrasShown() const { return extrasOpen || matrixExtrasInUse(); }

    void layoutMatrix()
    {
        const auto extras = matrixExtrasShown();
        auto inner = matrixCard.reduced (10, 0);
        inner.removeFromTop (26);

        // The FM style across the top, a note on what sets the depth beside
        // it; OSC 1 and OSC 2's ring mod and sync in a row under the matrix
        // (V6-14).
        auto top = inner.removeFromTop (44);
        mode.setBounds (top.removeFromLeft (juce::jmin (180, top.getWidth() / 2)).reduced (3, 2));
        topNote = top.withTrimmedLeft (12).withTrimmedTop (13);
        inner.removeFromTop (6);

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            const auto sourceShown = std::find (shown.begin(), shown.end(), source) != shown.end();
            outs[(size_t) source]->setVisible (sourceShown);
            noiseKnobs[(size_t) source]->setVisible (sourceShown && extras);

            for (int target = 0; target < OscillatorIds::count; ++target)
                knobs[(size_t) source][(size_t) target]->setVisible (
                    sourceShown && std::find (shown.begin(), shown.end(), target) != shown.end());
        }
        noiseColourKnob->setVisible (extras);
        ringMod->setVisible (extras);
        hardSync.setVisible (extras);
        moreButton.setVisible (true);
        // (A closing "‹" in place of "›" while they are open.)
        moreButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("EXTRAS  \xc2\xb7  RING MOD  \xc2\xb7  SYNC  \xc2\xb7  NOISE FM  "))
                                  + juce::String (juce::CharPointer_UTF8 (extras ? "\xe2\x80\xb9" : "\xe2\x80\xba")));

        // One cell size for every patch: the grid (row names and cells)
        // centred across the card, the card as tall as what it holds.
        const auto count = juce::jmax (1, (int) shown.size());
        const auto rows = count + (extras ? 1 : 0);
        const auto bottomHeight = extras ? 62 + 6 : 28;
        // (Three oscillators or fewer draw larger cells, so the matrix of a
        // small patch fills its card: UI review 9, V9-8.)
        const auto small = count <= 3;
        // (Cells are sized for their count, not stretched to the card: a
        // three-oscillator matrix has modest cells, a six-operator one larger
        // than its minimum, so neither is a grid of empty boxes or leaves the
        // card's foot bare: V10-13.)
        const auto rowHeight = juce::jmin (small ? 72 : 66, (inner.getHeight() - 22 - bottomHeight - 8) / rows);
        const auto columnWidth = juce::jmin (small ? 84 : 76, (inner.getWidth() - headWidth) / count);
        const auto gridWidth = headWidth + columnWidth * count;
        // The grid centred between the FM MODE line and the bottom row.
        const auto gridHeight = 22 + rowHeight * rows;
        auto grid = juce::Rectangle<int> (inner.getX() + (inner.getWidth() - gridWidth) / 2,
                                          inner.getY(), gridWidth, gridHeight);

        matrixGridBottom = grid.getBottom();
        auto heads = grid.removeFromTop (18);
        heads.removeFromLeft (headWidth);
        for (const auto i : shown)
            columnHeads[(size_t) i] = heads.removeFromLeft (columnWidth);

        grid.removeFromTop (4);

        // Short cells get compact knobs, their values drawn in the cell's
        // corner: the knob's own value box overlapped it.
        compactCells = rowHeight < 82;
        const auto layoutCells = [&] (juce::Rectangle<int> row, auto&& knobFor, auto&& storeCell)
        {
            for (const auto target : shown)
            {
                auto cell = row.removeFromLeft (columnWidth).reduced (4, 0);
                storeCell (target, cell);
                auto& knob = knobFor (target);
                knob.setCompact (compactCells);
                if (compactCells)
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 22, IlanaTheme::KnobSize::mini);
                    knob.setBounds (cell.withSizeKeepingCentre (knobSize, knobSize).translated (0, -6));
                }
                else
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 8, IlanaTheme::KnobSize::mini + 30);
                    knob.setBounds (cell.withSizeKeepingCentre (knobSize, knobSize + 4));
                }
            }
        };

        // A row's name above its OUT button, centred in the row head (they
        // overlapped the next row's name in short rows).
        const auto layoutHead = [] (juce::Rectangle<int> head)
        {
            return head.withSizeKeepingCentre (head.getWidth(), juce::jmin (60, head.getHeight()));
        };

        for (const auto source : shown)
        {
            auto row = grid.removeFromTop (rowHeight).reduced (0, 3);
            auto head = row.removeFromLeft (headWidth);
            rowHeads[(size_t) source] = layoutHead (head);
            // The row's name, then its OUT switch with that name above it.
            const auto& block = rowHeads[(size_t) source];
            outs[(size_t) source]->setBounds (block.withTrimmedTop (20).withHeight (13 + juce::jmin (22, block.getHeight() - 33)));

            layoutCells (row,
                         [this, source] (int target) -> KnobControl& { return *knobs[(size_t) source][(size_t) target]; },
                         [this, source] (int target, juce::Rectangle<int> cell) { cells[(size_t) source][(size_t) target] = cell; });
        }

        noiseHead = {};
        for (auto& cell : noiseCells)
            cell = {};
        if (extras)
        {
            auto row = grid.removeFromTop (rowHeight).reduced (0, 3);
            auto head = row.removeFromLeft (headWidth);
            noiseHead = layoutHead (head);
            // NOISE, then its colour: a small dial with its name and value.
            noiseColourKnob->setBounds (noiseHead.getX() - 2, noiseHead.getY() + 20, 30, 30);
            layoutCells (row,
                         [this] (int target) -> KnobControl& { return *noiseKnobs[(size_t) target]; },
                         [this] (int target, juce::Rectangle<int> cell) { noiseCells[(size_t) target] = cell; });
        }

        // Along the card's bottom: OSC 1 x OSC 2's pair controls, or the
        // MORE line that opens them.
        // (Right under the grid, not at the card's foot, so nothing floats:
        // UI review 9, V9-8.)
        const auto gridBottom = juce::jmin (inner.getBottom() - 8, matrixGridBottom + 12);
        inner.removeFromBottom (8);
        pairRow = {};
        pairText = {};
        if (extras)
        {
            pairRow = juce::Rectangle<int> (inner.getX(), juce::jmin (inner.getBottom() - 62, gridBottom), inner.getWidth(), 62);
            inner.setBottom (pairRow.getY());
            auto row = pairRow.reduced (4, 0);
            hardSync.setBounds (row.removeFromRight (100).withSizeKeepingCentre (100, 37));
            ringMod->setBounds (row.removeFromRight (80));
            pairText = row.withTrimmedRight (8);
            const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)),
                                                                           moreButton.getButtonText()) + 28;
            moreButton.setBounds (juce::Rectangle<int> (pairText.getX(), pairText.getY() + 2, juce::jmin (pairText.getWidth(), width), 22));
        }
        else
        {
            const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)),
                                                                           moreButton.getButtonText()) + 28;
            moreButton.setBounds (juce::Rectangle<int> (inner.getX() + 4, juce::jmin (inner.getBottom() - 22, gridBottom), juce::jmin (inner.getWidth() - 8, width), 22));
        }
    }

    void paintMatrix (juce::Graphics& g)
    {
        IlanaTheme::paintCardHeader (g, matrixCard.reduced (12, 0).withHeight (26), "FM MATRIX", "rows modulate columns", fmColour(), 0);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        IlanaTheme::drawFitted (g, anyOperatorEnv() ? juce::String (juce::CharPointer_UTF8 ("Depth = the modulating operator's OUTPUT (on its card) "
                                                                             "\xc3\x97 this cell. Hover a dot to add a route."))
                                           : juce::String ("Each cell is how deeply its row modulates its column. Hover a dot to add a route."),
                          topNote, juce::Justification::topLeft, 3);

        // Matrix cells: tinted by the source, brighter the deeper the route;
        // an empty one is a dot until the mouse is over it.
        const auto paintCell = [&g, this] (int source, int target, juce::Rectangle<float> cell, juce::Colour colour)
        {
            const auto amount = cellAmount (source, target);
            if (! isLiveCell (source, target))
            {
                // This oscillator is off or ignores FM: a flat, empty cell.
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
                return;
            }

            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (0.04f + 0.22f * amount));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (amount > 0.001f ? 0.55f : 0.12f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);

            if (cellKnob (source, target).getAlpha() < 0.01f)
            {
                const auto centre = cellKnob (source, target).getBounds().toFloat().getCentre();
                g.setColour (colour.withAlpha (0.12f));
                g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (centre), 1.0f);
                g.setColour (colour.withAlpha (0.5f));
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (centre));
            }
        };

        for (const auto source : shown)
        {
            for (const auto target : shown)
            {
                const auto cell = cells[(size_t) source][(size_t) target].toFloat();
                paintCell (source, target, cell, FmDiagram::oscColour (source));

                // Feedback: a loop in the corner, clear of the knob (V6-15).
                if (source == target && isLiveCell (source, target))
                    paintFeedbackGlyph (g, cell.reduced (6.0f, 5.0f).withSize (11.0f, 11.0f), FmDiagram::oscColour (source));
            }

            if (! noiseHead.isEmpty())
                paintCell (-1, source, noiseCells[(size_t) source].toFloat(), noiseColour());
        }

        // Each amount under its knob (compact cells), or a DX7 feedback's
        // number in the corner (I6-40).
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto target : shown)
        {
            for (int source = noiseHead.isEmpty() ? 0 : -1; source < OscillatorIds::count; ++source)
            {
                if (source >= 0 && std::find (shown.begin(), shown.end(), source) == shown.end())
                    continue;
                const auto& knob = cellKnob (source, target);
                const auto cell = source < 0 ? noiseCells[(size_t) target] : cells[(size_t) source][(size_t) target];
                const auto amount = cellAmount (source, target);
                const auto feedback = source == target ? dx7FeedbackText (source) : juce::String();
                if (compactCells && knob.getAlpha() > 0.01f)
                {
                    const auto id = source < 0 ? "fm_noise" + juce::String (target + 1) : FmDiagram::routeId (source, target);
                    const auto value = describeValue (id, amount);
                    g.setColour (juce::Colours::white.withAlpha (amount > 0.001f ? 0.85f : 0.4f) .withMultipliedAlpha (knob.getAlpha()));
                    g.drawText (feedback.isNotEmpty() ? feedback + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + value : value,
                                cell.withTrimmedBottom (2).removeFromBottom (14), juce::Justification::centred);
                }
                else if (! compactCells && feedback.isNotEmpty())
                {
                    g.setColour (FmDiagram::oscColour (source).withAlpha (0.8f));
                    g.drawText (feedback, cell.reduced (6, 4).withTrimmedLeft (14).withHeight (12), juce::Justification::centredLeft);
                }
            }
        }

        // Column and row headings: an off oscillator is dimmed, with no
        // "OFF" away from its switch (I6-37; review 8, I8-20).
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

        for (const auto i : shown)
        {
            const auto name = "OSC " + juce::String (i + 1);
            const auto live = fmIn[(size_t) i] && playing[(size_t) i];
            g.setColour (FmDiagram::oscColour (i).withAlpha (live ? 1.0f : 0.4f));
            // (Every column says TO; one that can't take FM or is off is dimmed: V9-28.)
            g.drawText ("TO " + name, columnHeads[(size_t) i], juce::Justification::centred);
            g.setColour (playing[(size_t) i] ? FmDiagram::oscColour (i) : IlanaTheme::Ui::text3);
            g.drawText (name, rowHeads[(size_t) i].withHeight (18),
                        juce::Justification::centredLeft);
        }

        if (noiseHead.isEmpty())
            return;

        g.setColour (noiseColour());
        // Noise as a modulator, with its own colour: the NOISE you hear (SUB +
        // NOISE on PLAY and OSC) is a separate, white source (V7-30).
        g.drawText ("NOISE FM", noiseHead.withHeight (18), juce::Justification::centredLeft);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text3);
        const auto colourText = juce::Rectangle<int> (noiseColourKnob->getRight() + 2, noiseColourKnob->getY() + 2,
                                                      noiseHead.getRight() - noiseColourKnob->getRight() - 2, 13);
        g.drawText ("COLOUR", colourText, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text2);
        g.drawText (describeValue ("fm_noise_color", read ("fm_noise_color")), colourText.translated (0, 13),
                    juce::Justification::centredLeft);

        // OSC 1 x OSC 2: the two pair controls, said in a line.
        g.setColour (IlanaTheme::Ui::line);
        g.drawHorizontalLine (pairRow.getY() - 3, (float) pairRow.getX() + 4.0f, (float) pairRow.getRight() - 4.0f);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        IlanaTheme::drawFitted (g, "RING MOD and SYNC are for OSC 1 and OSC 2 only: RING MOD multiplies OSC 1 by OSC 2, SYNC restarts OSC 2 with each cycle of OSC 1.",
                          pairText.withTrimmedTop (pairText.getHeight() / 2 + 4), juce::Justification::topLeft, 3);
    }

    // "FB 6" for a DX7 feedback at one of the DX7's own steps (they import
    // as 2^(n - 8)), else empty.
    juce::String dx7FeedbackText (int osc) const
    {
        if (juce::roundToInt (read (FmOperatorInfo::prefixOf (osc) + "_fb_type")) != FmFeedback::Dx7)
            return {};
        const auto amount = read (FmDiagram::routeId (osc, osc));
        if (amount <= 0.0f)
            return {};
        const auto steps = 8.0f + std::log2 (amount);
        const auto step = juce::roundToInt (steps);
        return std::abs (steps - (float) step) < 0.02f && step >= 1 && step <= 7 ? "FB " + juce::String (step) : juce::String();
    }

    static void paintFeedbackGlyph (juce::Graphics& g, juce::Rectangle<float> box, juce::Colour colour)
    {
        // A loop back into itself: most of a circle and its arrowhead.
        juce::Path loop;
        const auto centre = box.getCentre();
        const auto radius = box.getWidth() * 0.4f;
        loop.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, 0.5f, juce::MathConstants<float>::twoPi - 0.2f, true);
        g.setColour (colour.withAlpha (0.8f));
        g.strokePath (loop, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto tip = centre + juce::Point<float> (std::sin (-0.2f), -std::cos (-0.2f)) * radius;
        juce::Path head;
        head.addTriangle (tip.x - 3.2f, tip.y - 2.6f, tip.x + 1.0f, tip.y, tip.x - 3.2f, tip.y + 2.6f);
        g.fillPath (head);
    }

    int matrixGridBottom = 0;
    static constexpr int headWidth = 76;
    static constexpr int operatorCardHeight = 240;
    // Matrix cells: one size for every patch, about the size six
    // oscillators leave (V8-6, V8-35).
    static constexpr int cellWidth = 56, cellHeight = 52, minimumMatrixWidth = 410;

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {}, playing {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    OperatorEnvEditor envelope;
    OscPicker picker;
    juce::TextButton pitchLfoLink;
    CardTabs pageTabs;
    ComboControl mode;
    ToggleControl hardSync;
    EffectRules effectRules;
    std::unique_ptr<KnobControl> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    bool compactCells = false, extrasOpen = false, lastExtrasShown = true;
    juce::TextButton moreButton;
    // An operator on another envelope shows that envelope where an Operator
    // Env operator shows its own (one card anatomy, I8-11, V8-6).
    std::unique_ptr<EnvelopeDisplay> ampGraph;
    int ampGraphEnvelope = -1, ampGraphOperator = -1;
    std::unique_ptr<KnobControl> noiseColourKnob;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    std::array<juce::Rectangle<int>, OscillatorIds::count> columnHeads, rowHeads, noiseCells;
    std::array<std::array<juce::Rectangle<int>, OscillatorIds::count>, OscillatorIds::count> cells;
    juce::Rectangle<int> matrixCard, operatorCard, algorithmsTitle, noiseHead, topNote, pairRow, pairText, ampHint;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count * 3> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0, tabsLeft = 0, lastDiagramMinimum = 0;
    bool lastAnyOperatorEnv = false;
    juce::String lastAlgorithmLabel, hoverAlgorithm;
};

// M7.5: ilanaSynth FX's INPUT page: the input's level and envelope, its
// gain and trigger, where it goes, and quick starts.
class InputPage : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    explicit InputPage (IlanaSynthAudioProcessor& p)
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

    static juce::Colour inputColour() { return juce::Colour (0xff5fd3ff); }
    static juce::Colour routeColour() { return juce::Colour (0xffffb454); }

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

    void resized() override
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

private:
    void timerCallback() override
    {
        const auto level = processorRef.getInputLevel();
        const auto envelope = processorRef.getInputEnvelope();
        if (std::abs (level - shownLevel) > 1.0e-4f || std::abs (envelope - shownEnvelope) > 1.0e-4f)
        {
            shownLevel = level;
            shownEnvelope = envelope;
            repaint (meter);
        }
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    KnobControl gain, dry;
    ComboControl trigger;
    KnobControl threshold, note, attack, release, toBody, toStrings;
    std::array<juce::TextButton, 4> quickStarts;
    juce::Rectangle<int> inputCard, routeCard, startCard, meter, helpArea;
    float shownLevel = 0.0f, shownEnvelope = 0.0f;
};
} // namespace
