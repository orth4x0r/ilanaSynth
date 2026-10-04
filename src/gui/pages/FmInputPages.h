// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// An operator tab: the oscillator's colour dot and its name at normal
// contrast, so unselected tabs read as choices rather than disabled ones (UI
// review 6, V6-41); quiet only while the oscillator is off (I6-37), or the
// PITCH & LFO tab while no oscillator plays the Operator Env.
class OperatorPill : public juce::TextButton
{
public:
    OperatorPill (const juce::String& text, juce::Colour colourIn, bool withDot = true)
        : juce::TextButton (text), colour (colourIn), dot (withDot)
    {
        setClickingTogglesState (false);
    }

    void setQuiet (bool shouldBeQuiet)
    {
        if (quiet != shouldBeQuiet)
        {
            quiet = shouldBeQuiet;
            repaint();
        }
    }

    bool isQuiet() const { return quiet; }

    // Short: just the number ("OSC 3" as "3"), where the header is tight.
    void setShort (bool shouldBeShort)
    {
        if (shortText != shouldBeShort)
        {
            shortText = shouldBeShort;
            repaint();
        }
    }

    int getIdealWidth (bool asShort = false) const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont()), shownText (asShort)) + (dot ? 34 : 24);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto pill = getLocalBounds().toFloat().reduced (0.5f);
        const auto active = getToggleState();
        const auto radius = pill.getHeight() * 0.5f;
        const auto hover = over || down ? 1.0f : 0.0f;

        g.setColour (active ? colour.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.04f + 0.05f * hover));
        g.fillRoundedRectangle (pill, radius);
        g.setColour (active ? colour.withAlpha (0.8f) : colour.withAlpha (quiet ? 0.14f : 0.32f + 0.2f * hover));
        g.drawRoundedRectangle (pill.reduced (0.5f), radius, 1.0f);

        // The dot and name, centred together.
        const auto font = juce::Font (IlanaTheme::pillFont());
        const auto text = shownText (shortText);
        const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, text);
        const auto contentWidth = textWidth + (dot ? 11.0f : 0.0f);
        auto content = pill.withSizeKeepingCentre (juce::jmin (pill.getWidth() - 8.0f, contentWidth), pill.getHeight());
        if (dot)
        {
            IlanaTheme::paintOnDot (g, { content.getX() + 3.0f, content.getCentreY() }, colour, ! quiet);
            content.removeFromLeft (11.0f);
        }
        g.setColour (active  ? colour.interpolatedWith (juce::Colours::white, 0.2f)
                     : quiet ? IlanaTheme::Ui::text3
                             : IlanaTheme::Ui::text.withAlpha (0.82f + 0.18f * hover));
        g.setFont (font);
        g.drawText (text, content, juce::Justification::centredLeft, false);
    }

private:
    juce::String shownText (bool asShort) const
    {
        return asShort ? getButtonText().fromLastOccurrenceOf (" ", false, false) : getButtonText();
    }

    juce::Colour colour;
    bool dot = true, quiet = false, shortText = false;
};

// FM between six oscillators: the algorithms, the operator diagram and the
// selected operator's settings on the left; the full matrix of amounts on the
// right (rows = from, columns = to, plus the noise operator), with the FM
// style and, under it, OSC 1 and OSC 2's ring mod and hard sync.
//
// An oscillator on the Operator Env (a DX7 voice's operators) shows its
// envelope as a graph you drag, in the synth's words and units: ATTACK to
// RELEASE in ms, PEAK to END in dB, its output as LEVEL (UI review 6, I6-7 to
// I6-9). The voice's pitch envelope and LFO sit behind PITCH & LFO.
class FmPage : public juce::Component,
               private IlanaAnim::FrameTimer
{
    // One operator's M5 settings (tuning, key scaling, feedback style), the
    // oscillator controls that matter most when it is an operator, and its
    // Operator Env.
    struct OperatorControls
    {
        OperatorControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefixIn, juce::Colour colour)
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
              keyLevel (state, prefix + "_key_level", "KEY LVL", colour, false)
        {
            // The Operator Env, in OperatorEg::operatorFields() order, named
            // as the synth's own envelopes are (I6-7).
            static const char* const labels[] { "ATTACK", "DECAY 1", "DECAY 2", "RELEASE", "PEAK", "MID", "SUSTAIN", "END",
                                                 "SCALE KEY", "LOW DEPTH", "HIGH DEPTH", "LOW CURVE", "HIGH CURVE",
                                                 "KEY RATE", "AMP MOD", "VEL", "LEVEL" };
            for (size_t i = 0; i < OperatorEg::operatorFields().size(); ++i)
            {
                const auto& field = OperatorEg::operatorFields()[i];
                const auto id = prefix + field.suffix;
                if (field.choice)
                    eg.push_back (std::make_unique<ComboControl> (state, id, labels[i]));
                else
                    eg.push_back (std::make_unique<KnobControl> (state, id, labels[i], colour, false));
            }
            FmOperatorInfo::sectionEnvelopeMenu (ampEnv.getComboBox());
        }

        juce::String prefix;
        ComboControl tune, snap, feedbackType, ampEnv;
        KnobControl ratio, fixedHz, semi, fine, level, keyLevel;
        std::vector<std::unique_ptr<juce::Component>> eg;

        KnobControl* egKnob (size_t index) { return dynamic_cast<KnobControl*> (eg[index].get()); }

        std::vector<juce::Component*> all()
        {
            std::vector<juce::Component*> list { &tune, &snap, &feedbackType, &ampEnv, &ratio, &fixedHz, &semi, &fine, &level, &keyLevel };
            for (auto& control : eg)
                list.push_back (control.get());
            return list;
        }

        // The two tabs under the graph, on one nine-column grid: the stages
        // (times, then levels) and how the key speeds them up; then the
        // keyboard and velocity scaling, the LFO's amp depth and TRIM (the
        // oscillator's LEVEL).
        std::vector<juce::Component*> egStages()
        {
            return { eg[0].get(), eg[1].get(), eg[2].get(), eg[3].get(), eg[4].get(), eg[5].get(), eg[6].get(), eg[7].get(), eg[13].get() };
        }
        std::vector<juce::Component*> egScaling()
        {
            return { eg[8].get(), eg[9].get(), eg[11].get(), eg[10].get(), eg[12].get(), eg[15].get(), eg[14].get(), &level, nullptr };
        }
        static bool isStageControl (size_t index) { return index < 8 || index == 13; }
    };

    // The Operator Env's voice-wide half: its LFO and pitch envelope.
    struct VoiceControls
    {
        VoiceControls (juce::AudioProcessorValueTreeState& state, juce::Colour colour)
            : shape (state, "opeg_lfo_wave", "SHAPE"),
              retrig (state, "opeg_lfo_sync", "RETRIG"),
              rate (state, "opeg_lfo_speed", "RATE", colour, false),
              delay (state, "opeg_lfo_delay", "DELAY", colour, false),
              pitchDepth (state, "opeg_lfo_pmd", "PITCH DEPTH", colour, false),
              pitchSens (state, "opeg_lfo_pms", "PITCH SENS", colour, false),
              ampDepth (state, "opeg_lfo_amd", "AMP DEPTH", colour, false),
              scaleShift (state, OperatorEg::keyOffsetId, "SCALE SHIFT", colour, false)
        {
            retrig.showAsSwitch();
            static const char* const labels[] { "ATTACK", "DECAY 1", "DECAY 2", "RELEASE", "PITCH 1", "PITCH 2", "SUSTAIN", "END" };
            for (size_t i = 0; i < 8; ++i)
                stages[i] = std::make_unique<KnobControl> (state, OperatorEg::voiceFields()[i].suffix, labels[i], colour, false);
        }

        ComboControl shape;
        ToggleControl retrig;
        KnobControl rate, delay, pitchDepth, pitchSens, ampDepth, scaleShift;
        std::array<std::unique_ptr<KnobControl>, 8> stages;

        std::vector<juce::Component*> all()
        {
            std::vector<juce::Component*> list { &shape, &retrig, &rate, &delay, &pitchDepth, &pitchSens, &ampDepth, &scaleShift };
            for (auto& knob : stages)
                list.push_back (knob.get());
            return list;
        }
        std::vector<juce::Component*> lfoBottom() { return { &pitchDepth, &pitchSens, &ampDepth, &scaleShift }; }
        std::vector<juce::Component*> pitchStages()
        {
            std::vector<juce::Component*> list;
            for (auto& knob : stages)
                list.push_back (knob.get());
            list.push_back (nullptr);
            return list;
        }
    };

    // A rate knob reads as its stage's time (the rest of the envelope as
    // set); its 0-99 stays in the tooltip for DX7 users (I6-7).
    struct EgKnob
    {
        KnobControl* knob = nullptr;
        juce::String prefix, baseTooltip;
        int stage = 0, lastValue = -1;
        bool rate = false;
        OperatorEnv::Settings settings {};
    };

public:
    explicit FmPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          diagram (p),
          algorithms (p),
          egGraph (p),
          voice (p.apvts, fmColour()),
          pageTabs ({ "BASIC", "DX7 1-16", "DX7 17-32" }, { fmColour(), fmColour(), fmColour() }, false),
          mode (p.apvts, "fm_mode", "FM MODE"),
          hardSync (p.apvts, "hard_sync", "SYNC 2 TO 1"),
          effectRules (p)
    {
        addAll (*this, diagram, algorithms, pageTabs, mode, hardSync);
        addChildComponent (envTabs);
        envTabs.setTooltip ("STAGES: the envelope's times and levels (or drag the graph). KEYS & VELOCITY: how the key and "
                            "velocity scale this operator, its LFO amp depth and its TRIM.");
        envTabs.onSelect = [this] (int)
        {
            updateOperatorVisibility();
            resized();
        };
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
            operators[(size_t) source] = std::make_unique<OperatorControls> (p.apvts, prefix, FmDiagram::oscColour (source));
            for (auto* control : operators[(size_t) source]->all())
                addChildComponent (control);

            auto button = std::make_unique<OperatorPill> ("OSC " + juce::String (source + 1), FmDiagram::oscColour (source));
            button->onClick = [this, source] { selectOperator (source); };
            addAndMakeVisible (*button);
            operatorButtons[(size_t) source] = std::move (button);

            // FB TYPE only acts with a feedback route (I6-10).
            effectRules.add (operators[(size_t) source]->feedbackType,
                             effectRules.isAbove (FmDiagram::routeId (source, source), 0.0005f));
            setUpEgKnobs (*operators[(size_t) source]);
            setUpLevelText (source);
        }

        // The noise operator's colour, at the head of its row (V6-14).
        noiseColourKnob = std::make_unique<KnobControl> (p.apvts, "fm_noise_color", "", noiseColour(), false);
        noiseColourKnob->setCompact (true);
        noiseColourKnob->setSizeRole (IlanaTheme::KnobSize::mini);
        addAndMakeVisible (*noiseColourKnob);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_tune", "_amp_env", "_on" })
                tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + suffix));

        // The Operator Env's graph, and the voice's pitch envelope and LFO
        // behind PITCH & LFO: a header button apart from the operator tabs,
        // always there (quiet while nothing plays the Operator Env).
        addChildComponent (egGraph);
        for (auto* control : voice.all())
            addChildComponent (control);
        for (size_t i = 0; i < 4; ++i)
            egKnobs.push_back ({ voice.stages[i].get(), {}, voice.stages[i]->getTooltip(), (int) i, -1, true });
        for (size_t i = 4; i < 8; ++i)
            egKnobs.push_back ({ voice.stages[i].get(), {}, voice.stages[i]->getTooltip(), (int) i - 4, -1, false });
        for (size_t i = 0; i < 4; ++i)
            setRateText (*voice.stages[i], {}, (int) i);
        voiceButton.setTooltip ("The Operator Env's pitch envelope and LFO, for the whole voice: every oscillator on the "
                                "Operator Env follows them. Also in MOD's envelope and LFO pools.");
        voiceButton.onClick = [this] { selectVoicePage(); };
        addAndMakeVisible (voiceButton);

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
        voicePage = false;

        for (int i = 0; i < OscillatorIds::count; ++i)
            operatorButtons[(size_t) i]->setToggleState (i == selectedOperator, juce::dontSendNotification);
        voiceButton.setToggleState (false, juce::dontSendNotification);

        updateOperatorVisibility();
        resized();
        repaint();
    }

    // PITCH & LFO: the Operator Env's settings for the whole voice.
    void selectVoicePage()
    {
        voicePage = true;
        for (auto& button : operatorButtons)
            button->setToggleState (false, juce::dontSendNotification);
        voiceButton.setToggleState (true, juce::dontSendNotification);
        updateOperatorVisibility();
        resized();
        repaint();
    }

    bool isVoicePageShown() const { return voicePage; }
    const FmDiagram& getDiagram() const { return diagram; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        paintSectionTitle (g, "ALGORITHMS", algorithmsTitle.withTrimmedRight (pageTabs.getWidth() + 8),
                           hoverAlgorithm.isNotEmpty() ? hoverAlgorithm : getAlgorithmLabel());
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        // The selected operator's settings, its tabs on the header line.
        const auto header = operatorCard.reduced (12, 0).withHeight (26);
        const auto reserve = operatorCard.getRight() - tabsLeft + 8;
        if (voicePage)
        {
            IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, header, "PITCH & LFO",
                                         anyOperatorEnv() ? "the whole voice" : "for oscillators on the Operator Env",
                                         fmColour(), reserve);
        }
        else
        {
            const auto colour = FmDiagram::oscColour (selectedOperator);
            IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, header, "OSC " + juce::String (selectedOperator + 1), operatorText(), colour, reserve);
        }

        // A rule between the operator tabs and PITCH & LFO.
        if (voiceButton.isVisible() && voiceButton.getX() > tabsLeft)
        {
            g.setColour (IlanaTheme::Ui::line);
            g.drawVerticalLine (voiceButton.getX() - 7, (float) voiceButton.getY() + 2.0f, (float) voiceButton.getBottom() - 2.0f);
        }

        if (envTabs.isVisible())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText ("Drag the graph's points: across for time, up or down for level.", envTabLine,
                        juce::Justification::centredRight, true);
        }

        if (voicePage)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText ("SCALE SHIFT moves only the keys the operators' scaling follows: transpose with each oscillator's "
                        "SEMI, or the octave on PLAY.",
                        voiceHint, juce::Justification::centredLeft, true);
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
            if (egGraph.isVisible())
                egGraph.refresh();
            refreshEgKnobs();
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

        if (refreshShown() || tuneChanged || diagram.getMinimumHeight() != lastDiagramMinimum)
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
            operatorButtons[(size_t) osc]->setQuiet (! playing[(size_t) osc]);
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

        matrixCard = area.removeFromRight (area.getWidth() * 44 / 100);
        area.removeFromRight (10);

        // Left column: algorithms (one row, its pages on the heading's line),
        // the diagram, the selected operator. The diagram needs no heading
        // of its own: its nodes say what it is, and the height goes to it.
        algorithmsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
        const auto tabsWidth = pageTabs.getIdealWidth();
        pageTabs.setBounds (algorithmsTitle.getRight() - tabsWidth - 2, algorithmsTitle.getCentreY() - 10, tabsWidth, 20);
        algorithms.setBounds (area.removeFromTop (42));
        area.removeFromTop (8);

        // The operator card takes what it needs and the diagram never less
        // than its stack of operators needs (UI review 6, I6-1).
        lastDiagramMinimum = diagram.getMinimumHeight();
        const auto egShown = ! voicePage && usesOperatorEnv (selectedOperator);
        const auto wanted = voicePage ? 262 : egShown ? 262 : 176;
        operatorCard = area.removeFromBottom (juce::jmax (150, juce::jmin (wanted, area.getHeight() - lastDiagramMinimum - 8)));
        area.removeFromBottom (8);
        diagram.setBounds (area);

        layoutOperatorCard();
        layoutMatrix();
    }

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
        if (matching >= 0 && matching < (int) FmAlgorithms::all().size())
            return FmAlgorithms::all()[(size_t) matching].name;

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
        text << dot << (usesOperatorEnv (selectedOperator) ? "Operator Env" : operators[(size_t) selectedOperator]->ampEnv.getComboBox().getText());
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

    // The ENVELOPE choice, the rate knobs' times and every Op Env knob's
    // DX7 value, kept current.
    void setUpEgKnobs (OperatorControls& controls)
    {
        for (size_t i = 0; i < 17; ++i)
            if (auto* knob = controls.egKnob (i))
                egKnobs.push_back ({ knob, controls.prefix, knob->getTooltip(), (int) (i % 4), -1, i < 4 });
        for (int stage = 0; stage < 4; ++stage)
            setRateText (*controls.egKnob ((size_t) stage), controls.prefix, stage);
    }

    void setRateText (KnobControl& knob, const juce::String& prefix, int stage)
    {
        auto& slider = knob.getSlider();
        const auto original = slider.valueFromTextFunction;
        slider.textFromValueFunction = [this, prefix, stage] (double value)
        {
            auto settings = OperatorEnv::read (processorRef, prefix);
            settings.rates[(size_t) stage] = juce::jlimit (0, 99, juce::roundToInt (value));
            const auto run = OperatorEnv::run (settings, stage);
            return OperatorEnv::formatSeconds (run.stageSeconds (stage), run.endless[(size_t) stage]);
        };
        // "250 ms" or "1.5 s" types a time; a bare number is the DX7 value.
        slider.valueFromTextFunction = [this, prefix, stage, original] (const juce::String& text)
        {
            const auto trimmed = text.trim().toLowerCase();
            if (trimmed.endsWith ("ms") || trimmed.endsWith ("s"))
            {
                const auto seconds = trimmed.getDoubleValue() * (trimmed.endsWith ("ms") ? 0.001 : 1.0);
                return (double) OperatorEnv::rateForSeconds (OperatorEnv::read (processorRef, prefix), stage, seconds);
            }
            return original != nullptr ? original (text) : trimmed.getDoubleValue();
        };
        slider.updateText();
    }

    // LEVEL reads in % on Amp Env; on the Operator Env it is TRIM, 0 dB at
    // 50 % (the level a DX7 voice imports at), its output level being the
    // operator's LEVEL (I6-8, S6-5).
    void setUpLevelText (int op)
    {
        auto& slider = operators[(size_t) op]->level.getSlider();
        const auto original = slider.textFromValueFunction;
        slider.textFromValueFunction = [this, op, original] (double value)
        {
            if (! usesOperatorEnv (op))
                return original != nullptr ? original (value) : juce::String (value);
            if (value <= 0.0)
                return juce::String ("-inf dB");
            const auto db = juce::Decibels::gainToDecibels (value / 0.5);
            return (db > 0.05 ? "+" : "") + describeFixed ((float) db, 1) + " dB";
        };
        slider.updateText();
    }

    void refreshEgKnobs()
    {
        for (auto& entry : egKnobs)
        {
            if (! entry.knob->isVisible())
                continue;
            const auto value = juce::roundToInt (entry.knob->getSlider().getValue());
            if (value != entry.lastValue)
            {
                entry.lastValue = value;
                const auto tooltip = entry.baseTooltip + "\nDX7 value: " + juce::String (value);
                entry.knob->setTooltip (tooltip);
                entry.knob->getSlider().setTooltip (tooltip);
            }
            // A stage's time depends on the levels around it.
            if (entry.rate)
            {
                const auto settings = OperatorEnv::read (processorRef, entry.prefix);
                if (settings != entry.settings)
                {
                    entry.settings = settings;
                    entry.knob->getSlider().updateText();
                }
            }
        }
    }

    void updateOperatorVisibility()
    {
        voiceButton.setQuiet (! anyOperatorEnv());

        for (int op = 0; op < OscillatorIds::count; ++op)
        {
            const auto isShown = std::find (shown.begin(), shown.end(), op) != shown.end();
            operatorButtons[(size_t) op]->setVisible (isShown);

            auto& controls = *operators[(size_t) op];
            const auto selected = op == selectedOperator && ! voicePage;
            const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) op]) + "_tune"));
            const auto opEnv = usesOperatorEnv (op);

            for (auto* control : controls.all())
                control->setVisible (selected);

            controls.ratio.setVisible (selected && tune == OscTuning::Ratio);
            controls.snap.setVisible (selected && tune == OscTuning::Ratio);
            controls.fixedHz.setVisible (selected && tune == OscTuning::Fixed);
            // One level story (I6-8): on the Operator Env its output level
            // is LEVEL and the oscillator's LEVEL a TRIM beside the scaling;
            // the envelope's own scaling replaces KEY LVL.
            controls.keyLevel.setVisible (selected && ! opEnv);
            const auto stagesTab = envTabs.getSelected() == 0;
            for (size_t i = 0; i < controls.eg.size(); ++i)
                controls.eg[i]->setVisible (selected && opEnv && (i == 16 || OperatorControls::isStageControl (i) == stagesTab));
            controls.level.setVisible (selected && (! opEnv || ! stagesTab));
            if (controls.level.getLabelText() != (opEnv ? "TRIM" : "LEVEL"))
            {
                controls.level.setLabelText (opEnv ? "TRIM" : "LEVEL");
                controls.level.getSlider().updateText();
            }
        }

        for (auto* control : voice.all())
            control->setVisible (voicePage);
        envTabs.setVisible (! voicePage && usesOperatorEnv (selectedOperator));
        egGraph.setVisible (voicePage || usesOperatorEnv (selectedOperator));
        if (voicePage)
            egGraph.setSource ({}, fmColour());
        else
            egGraph.setSource (OscillatorIds::prefixes[(size_t) selectedOperator], FmDiagram::oscColour (selectedOperator));
    }

    void layoutOperatorCard()
    {
        auto inner = operatorCard.reduced (10, 0);

        // The tabs on the header line, right-aligned: the shown oscillators,
        // then PITCH & LFO past a rule (I6-38).
        {
            auto tabs = operatorCard.reduced (12, 0).withHeight (26).withSizeKeepingCentre (operatorCard.getWidth() - 24, 20);
            const auto voiceWidth = voiceButton.getIdealWidth();
            voiceButton.setBounds (tabs.removeFromRight (voiceWidth));
            tabs.removeFromRight (14);
            // "OSC 1" while the title and a line about the operator fit
            // beside them, else just the numbers.
            const auto widthOf = [this] (bool asShort)
            {
                auto width = 0;
                for (const auto op : shown)
                    width += operatorButtons[(size_t) op]->getIdealWidth (asShort) + 4;
                return width;
            };
            const auto asShort = widthOf (false) > tabs.getWidth() - 260;
            auto x = tabs.getRight() - widthOf (asShort);
            tabsLeft = x;
            for (const auto op : shown)
            {
                auto& button = *operatorButtons[(size_t) op];
                button.setShort (asShort);
                const auto w = button.getIdealWidth (asShort);
                button.setBounds (x, tabs.getY(), w, tabs.getHeight());
                x += w + 4;
            }
        }
        inner.removeFromTop (30);
        inner.removeFromBottom (6);

        // Rows of the Operator Env's knobs along the bottom, on one grid.
        const auto rowHeight = [&inner] (int rows, int above) { return juce::jlimit (56, 76, (inner.getHeight() - above) / rows); };
        const auto layoutGrid = [] (juce::Rectangle<int> row, std::vector<juce::Component*> items, size_t columns)
        {
            items.resize (columns, nullptr);
            layoutRow (row, items);
        };

        if (voicePage)
        {
            voiceHint = inner.removeFromBottom (16).withTrimmedLeft (2);
            const auto height = rowHeight (3, 0);
            auto stages = inner.removeFromBottom (height);
            inner.removeFromBottom (4);
            layoutGrid (stages, voice.pitchStages(), 9);
            egGraph.setBounds (inner.removeFromRight (inner.getWidth() * 4 / 9).withTrimmedLeft (8).reduced (0, 2));
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            // SHAPE two columns wide, so its wave's name fits.
            layoutRow (top, { &voice.shape, nullptr, &voice.retrig, &voice.rate, &voice.delay });
            voice.shape.setBounds (voice.shape.getBounds().withWidth (voice.shape.getWidth() + top.getWidth() / 5));
            layoutRow (inner, voice.lfoBottom());
            return;
        }

        auto& controls = *operators[(size_t) selectedOperator];
        const auto opEnv = usesOperatorEnv (selectedOperator);
        if (opEnv)
        {
            auto row = inner.removeFromBottom (rowHeight (3, 20));
            envTabLine = inner.removeFromBottom (22);
            envTabs.setBounds (envTabLine.removeFromLeft (envTabs.getIdealWidth()).withSizeKeepingCentre (envTabs.getIdealWidth(), 20));
            envTabLine.removeFromLeft (12);
            inner.removeFromBottom (4);
            layoutGrid (row, envTabs.getSelected() == 0 ? controls.egStages() : controls.egScaling(), 9);
            // The graph beside the tuning menus and knobs.
            egGraph.setBounds (inner.removeFromRight (inner.getWidth() / 3).withTrimmedLeft (8).reduced (0, 2));
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
            bottom.push_back (controls.eg[16].get());
        }
        else
        {
            bottom.push_back (&controls.level);
            bottom.push_back (&controls.keyLevel);
        }

        // Menus and knobs on one grid (as many columns as the longer row), so
        // each menu sits over a knob.
        const auto columns = juce::jmax (top.size(), bottom.size());
        top.resize (columns, nullptr);
        bottom.resize (columns, nullptr);
        layoutRow (topRow, top);
        layoutRow (inner, bottom);
    }

    void layoutMatrix()
    {
        auto inner = matrixCard.reduced (10, 0);
        inner.removeFromTop (26);
        inner.removeFromBottom (8);

        // The FM style across the top, a note on what sets the depth beside
        // it; OSC 1 and OSC 2's ring mod and sync in a row under the matrix
        // (V6-14).
        auto top = inner.removeFromTop (44);
        mode.setBounds (top.removeFromLeft (juce::jmin (180, top.getWidth() / 3)).reduced (3, 2));
        topNote = top.withTrimmedLeft (12).withTrimmedTop (13);
        inner.removeFromTop (6);

        pairRow = inner.removeFromBottom (62);
        inner.removeFromBottom (6);
        {
            auto row = pairRow.reduced (4, 0);
            hardSync.setBounds (row.removeFromRight (100).withSizeKeepingCentre (100, 37));
            ringMod->setBounds (row.removeFromRight (80));
            pairText = row.withTrimmedRight (8);
        }

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            const auto sourceShown = std::find (shown.begin(), shown.end(), source) != shown.end();
            outs[(size_t) source]->setVisible (sourceShown);
            noiseKnobs[(size_t) source]->setVisible (sourceShown);

            for (int target = 0; target < OscillatorIds::count; ++target)
                knobs[(size_t) source][(size_t) target]->setVisible (
                    sourceShown && std::find (shown.begin(), shown.end(), target) != shown.end());
        }

        const auto count = juce::jmax (1, (int) shown.size());
        auto heads = inner.removeFromTop (18);
        heads.removeFromLeft (headWidth);
        const auto columnWidth = heads.getWidth() / count;

        for (const auto i : shown)
            columnHeads[(size_t) i] = heads.removeFromLeft (columnWidth);

        inner.removeFromTop (4);

        const auto rowHeight = inner.getHeight() / (count + 1);

        // Short cells (six oscillators) get compact knobs, their values
        // drawn in the cell's corner: the knob's own value box overlapped it.
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
            auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
            auto head = row.removeFromLeft (headWidth);
            rowHeads[(size_t) source] = layoutHead (head);
            // The row's name, then its OUT switch with that name above it.
            const auto& block = rowHeads[(size_t) source];
            outs[(size_t) source]->setBounds (block.withTrimmedTop (20).withHeight (13 + juce::jmin (22, block.getHeight() - 33)));

            layoutCells (row,
                         [this, source] (int target) -> KnobControl& { return *knobs[(size_t) source][(size_t) target]; },
                         [this, source] (int target, juce::Rectangle<int> cell) { cells[(size_t) source][(size_t) target] = cell; });
        }

        auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
        auto head = row.removeFromLeft (headWidth);
        noiseHead = layoutHead (head);
        // NOISE, then its colour: a small dial with its name and value.
        noiseColourKnob->setBounds (noiseHead.getX() - 2, noiseHead.getY() + 20, 30, 30);
        layoutCells (row,
                     [this] (int target) -> KnobControl& { return *noiseKnobs[(size_t) target]; },
                     [this] (int target, juce::Rectangle<int> cell) { noiseCells[(size_t) target] = cell; });
    }

    void paintMatrix (juce::Graphics& g)
    {
        IlanaTheme::paintCardHeader (g, matrixCard.reduced (12, 0).withHeight (26), "FM MATRIX", "rows modulate columns", fmColour(), 0);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawFittedText (anyOperatorEnv() ? "On the Operator Env a modulator's depth is its LEVEL; a cell scales it. "
                                             "Hover a dot to add a route."
                                           : "Each cell is how deeply its row modulates its column. Hover a dot to add a route.",
                          topNote, juce::Justification::topLeft, 2, 1.0f);

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

            paintCell (-1, source, noiseCells[(size_t) source].toFloat(), noiseColour());
        }

        // Each amount under its knob (compact cells), or a DX7 feedback's
        // number in the corner (I6-40).
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto target : shown)
        {
            for (int source = -1; source < OscillatorIds::count; ++source)
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

        // Column and row headings: an off oscillator says so (I6-37).
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

        for (const auto i : shown)
        {
            const auto name = "OSC " + juce::String (i + 1);
            const auto live = fmIn[(size_t) i] && playing[(size_t) i];
            g.setColour (FmDiagram::oscColour (i).withAlpha (live ? 1.0f : 0.4f));
            g.drawText (! playing[(size_t) i] ? name + ": OFF" : ! fmIn[(size_t) i] ? name + ": NO FM IN" : "TO " + name,
                        columnHeads[(size_t) i], juce::Justification::centred);
            g.setColour (playing[(size_t) i] ? FmDiagram::oscColour (i) : IlanaTheme::Ui::text3);
            g.drawText (playing[(size_t) i] ? name : name + ": OFF", rowHeads[(size_t) i].withHeight (18),
                        juce::Justification::centredLeft);
        }

        g.setColour (noiseColour());
        g.drawText ("NOISE", noiseHead.withHeight (18), juce::Justification::centredLeft);
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
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("OSC 1 \xc3\x97 OSC 2")), pairText.withHeight (pairText.getHeight() / 2).translated (0, 4),
                    juce::Justification::bottomLeft);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawFittedText ("RING MOD multiplies OSC 1 by OSC 2. SYNC restarts OSC 2 with each cycle of OSC 1.",
                          pairText.withTrimmedTop (pairText.getHeight() / 2 + 6), juce::Justification::topLeft, 2, 1.0f);
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

    static constexpr int headWidth = 76;

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {}, playing {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    OperatorEnvDisplay egGraph;
    VoiceControls voice;
    std::vector<EgKnob> egKnobs;
    OperatorPill voiceButton { "PITCH & LFO", fmColour(), false };
    CardTabs pageTabs;
    CardTabs envTabs { { "STAGES", "KEYS & VELOCITY" }, { fmColour(), fmColour() }, false };
    bool voicePage = false;
    ComboControl mode;
    ToggleControl hardSync;
    EffectRules effectRules;
    std::unique_ptr<KnobControl> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    bool compactCells = false;
    std::unique_ptr<KnobControl> noiseColourKnob;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    std::array<std::unique_ptr<OperatorPill>, OscillatorIds::count> operatorButtons;
    std::array<juce::Rectangle<int>, OscillatorIds::count> columnHeads, rowHeads, noiseCells;
    std::array<std::array<juce::Rectangle<int>, OscillatorIds::count>, OscillatorIds::count> cells;
    juce::Rectangle<int> matrixCard, operatorCard, algorithmsTitle, noiseHead, topNote, envTabLine, pairRow, pairText, voiceHint;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count * 3> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0, tabsLeft = 0, lastDiagramMinimum = 0;
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
