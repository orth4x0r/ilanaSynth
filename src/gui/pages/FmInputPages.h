// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// FM between six oscillators: the algorithms, the operator diagram and the
// selected operator's settings on the left; the full matrix of amounts on the
// right (rows = from, columns = to, plus the noise operator), with the FM
// style, each oscillator's output switch, ring mod and hard sync.
class FmPage : public juce::Component,
               private IlanaAnim::FrameTimer
{
    // One operator's M5 settings (tuning, key scaling, feedback style) and
    // the oscillator controls that matter most when it is an operator.
    struct OperatorControls
    {
        OperatorControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, juce::Colour colour)
            : tune (state, prefix + "_tune", "TUNING"),
              snap (state, prefix + "_ratio_snap", "SNAP"),
              feedbackType (state, prefix + "_fb_type", "FB TYPE"),
              ampEnv (state, prefix + "_amp_env", "ENVELOPE"),
              ratio (state, prefix + "_ratio", "RATIO", colour, false),
              fixedHz (state, prefix + "_fixed_hz", "FIXED", colour, false),
              semi (state, prefix + "_semi", "SEMI", colour, false),
              fine (state, prefix + "_fine", "FINE", colour, false),
              level (state, prefix + "_level", "LEVEL", colour, false),
              keyLevel (state, prefix + "_key_level", "KEY LVL", colour, false) {}

        ComboControl tune, snap, feedbackType, ampEnv;
        KnobControl ratio, fixedHz, semi, fine, level, keyLevel;

        std::vector<juce::Component*> all()
        {
            return { &tune, &snap, &feedbackType, &ampEnv, &ratio, &fixedHz, &semi, &fine, &level, &keyLevel };
        }
    };

public:
    explicit FmPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          diagram (p),
          algorithms (p),
          mode (p.apvts, "fm_mode", "FM MODE"),
          hardSync (p.apvts, "hard_sync", "HARD SYNC 1>2")
    {
        addAll (*this, diagram, algorithms, mode, hardSync);
        hardSync.showAsSwitch();
        ringMod = std::make_unique<KnobControl> (p.apvts, "ring_mod", "RING MOD", fmColour(), false);
        addAndMakeVisible (*ringMod);

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            for (int target = 0; target < OscillatorIds::count; ++target)
            {
                auto knob = std::make_unique<KnobControl> (p.apvts, FmDiagram::routeId (source, target), "",
                                                           FmDiagram::oscColour (source), false);
                addAndMakeVisible (*knob);
                knobs[(size_t) source][(size_t) target] = std::move (knob);
            }

            outs[(size_t) source] = std::make_unique<ToggleControl> (
                p.apvts, juce::String (OscillatorIds::prefixes[(size_t) source]) + "_out", "OUT");
            addAndMakeVisible (*outs[(size_t) source]);

            noiseKnobs[(size_t) source] = std::make_unique<KnobControl> (p.apvts, "fm_noise" + juce::String (source + 1), "",
                                                                          noiseColour(), false);
            addAndMakeVisible (*noiseKnobs[(size_t) source]);

            const juce::String prefix (OscillatorIds::prefixes[(size_t) source]);
            operators[(size_t) source] = std::make_unique<OperatorControls> (p.apvts, prefix, FmDiagram::oscColour (source));
            for (auto* control : operators[(size_t) source]->all())
                addChildComponent (control);

            auto button = std::make_unique<juce::TextButton> ("OSC " + juce::String (source + 1));
            button->setClickingTogglesState (false);
            IlanaTheme::makePill (*button, FmDiagram::oscColour (source));
            button->onClick = [this, source] { selectOperator (source); };
            addAndMakeVisible (*button);
            operatorButtons[(size_t) source] = std::move (button);
        }

        noiseColourKnob = std::make_unique<KnobControl> (p.apvts, "fm_noise_color", "NOISE COLOUR", noiseColour(), false);
        addAndMakeVisible (*noiseColourKnob);

        for (const auto* prefix : OscillatorIds::prefixes)
            tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + "_tune"));

        refreshShown();
        refreshFmInputs (true);
        selectOperator (0);
        startTimerHz (12);
    }

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }
    static juce::Colour noiseColour() { return IlanaTheme::Ui::text2; }

    int getSelectedOperator() const { return selectedOperator; }

    void selectOperator (int op)
    {
        selectedOperator = juce::jlimit (0, OscillatorIds::count - 1, op);

        for (int i = 0; i < OscillatorIds::count; ++i)
            operatorButtons[(size_t) i]->setToggleState (i == selectedOperator, juce::dontSendNotification);

        updateOperatorVisibility();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        paintSectionTitle (g, "ALGORITHMS", algorithmsTitle);
        paintSectionTitle (g, "OPERATORS", operatorsTitle);
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        // The selected operator's settings.
        {
            const auto colour = FmDiagram::oscColour (selectedOperator);
            IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, operatorCard.reduced (12, 0).withHeight (26),
                                         "OSC " + juce::String (selectedOperator + 1) + " AS AN OPERATOR", soundingText(), colour, 0);
        }

        IlanaTheme::paintCardHeader (g, matrixCard.reduced (12, 0).withHeight (26), "FM MATRIX", "rows modulate columns", fmColour(), 0);

        // Matrix cells: tinted by the source, brighter the deeper the route.
        const auto paintCell = [&g] (juce::Rectangle<float> cell, float amount, juce::Colour colour)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (0.04f + 0.22f * amount));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (amount > 0.001f ? 0.55f : 0.12f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
        };

        for (const auto source : shown)
        {
            for (const auto target : shown)
            {
                const auto cell = cells[(size_t) source][(size_t) target].toFloat();
                const auto colour = FmDiagram::oscColour (source);

                if (! fmIn[(size_t) target])
                {
                    // This oscillator ignores FM: a flat, empty cell.
                    g.setColour (juce::Colours::black.withAlpha (0.3f));
                    g.fillRoundedRectangle (cell, 6.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.05f));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
                    continue;
                }

                paintCell (cell, read (FmDiagram::routeId (source, target)), colour);

                if (source == target)
                {
                    g.setColour (colour.withAlpha (0.6f));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    const auto type = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_fb_type"));
                    g.drawText (type == FmFeedback::Filtered ? "FB~" : type == FmFeedback::Cross ? "FB<>" : "FB",
                                cell.reduced (6.0f, 4.0f).toNearestInt(), juce::Justification::topLeft);
                }
            }

            if (fmIn[(size_t) source])
                paintCell (noiseCells[(size_t) source].toFloat(), read ("fm_noise" + juce::String (source + 1)), noiseColour());
        }

        // Compact cells: each amount under its knob.
        if (compactCells)
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            const auto value = [&g, this] (juce::Rectangle<int> cell, const juce::String& id)
            {
                const auto amount = read (id);
                g.setColour (juce::Colours::white.withAlpha (amount > 0.001f ? 0.85f : 0.4f));
                g.drawText (describeValue (id, amount), cell.removeFromBottom (15), juce::Justification::centred);
            };
            for (const auto source : shown)
            {
                for (const auto target : shown)
                    if (fmIn[(size_t) target])
                        value (cells[(size_t) source][(size_t) target], FmDiagram::routeId (source, target));
                if (fmIn[(size_t) source])
                    value (noiseCells[(size_t) source], "fm_noise" + juce::String (source + 1));
            }
        }

        // Column and row headings.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

        for (const auto i : shown)
        {
            g.setColour (FmDiagram::oscColour (i).withAlpha (fmIn[(size_t) i] ? 1.0f : 0.4f));
            g.drawText (fmIn[(size_t) i] ? "TO OSC " + juce::String (i + 1) : "OSC " + juce::String (i + 1) + ": NO FM IN",
                        columnHeads[(size_t) i], juce::Justification::centred);
            g.setColour (FmDiagram::oscColour (i));
            g.drawText ("OSC " + juce::String (i + 1), rowHeads[(size_t) i].withHeight (18), juce::Justification::centredLeft);
        }

        g.setColour (noiseColour());
        g.drawText ("NOISE", noiseHead.withHeight (18), juce::Justification::centredLeft);
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

        if (isShowing())
            algorithms.refreshMatch();

        if (refreshFmInputs (false))
            repaint (matrixCard);

        if (refreshShown() || tuneChanged)
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
    // there stay in the patch but do nothing.
    bool refreshFmInputs (bool force)
    {
        std::array<bool, OscillatorIds::count> now {};

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            now[(size_t) osc] = FmDiagram::receivesFm (processorRef, osc);

        if (now == fmIn && ! force)
            return false;

        fmIn = now;

        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            const auto on = fmIn[(size_t) target];
            const auto note = FmDiagram::fmInputNote (processorRef, target);

            std::vector<juce::Component*> column { noiseKnobs[(size_t) target].get() };
            for (int source = 0; source < OscillatorIds::count; ++source)
                column.push_back (knobs[(size_t) source][(size_t) target].get());

            for (auto* control : column)
            {
                control->setEnabled (on);
                control->setAlpha (on ? 1.0f : 0.22f);

                if (auto* tooltipClient = dynamic_cast<juce::SettableTooltipClient*> (control))
                {
                    if (! on)
                        tooltipClient->setTooltip (note);
                    else if (auto* knob = dynamic_cast<KnobControl*> (control))
                        tooltipClient->setTooltip (knob->getSlider().getTooltip());
                }
            }
        }

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

        matrixCard = area.removeFromRight (area.getWidth() * 48 / 100);
        area.removeFromRight (10);

        // Left column: algorithms, the diagram, the selected operator; its
        // heading on the FM MATRIX card's header line.
        algorithmsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
        algorithms.setBounds (area.removeFromTop (area.getWidth() >= 16 * 38 ? 48 : 80));
        area.removeFromTop (6);
        operatorsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
        operatorCard = area.removeFromBottom (juce::jmin (176, area.getHeight() / 2));
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

    // "sounds at x1.414 of the note" and the like, after SNAP.
    juce::String soundingText() const
    {
        // DX7 mode ignores ENVELOPE: the voice's own envelopes play the operator.
        const juce::String dx7 (processorRef.getDx7Voice() != nullptr ? ", envelope from the DX7 voice" : "");
        return soundingTuneText() + dx7;
    }

    juce::String soundingTuneText() const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) selectedOperator]);
        const auto tune = juce::roundToInt (read (prefix + "_tune"));

        if (tune == OscTuning::Ratio)
            return "sounds at x" + juce::String (processorRef.getSnappedRatio (selectedOperator), 3) + " the note";

        if (tune == OscTuning::Fixed)
            return "fixed at " + describeValue (prefix + "_fixed_hz", read (prefix + "_fixed_hz"));

        return "tuned in semitones";
    }

    void updateOperatorVisibility()
    {
        for (int op = 0; op < OscillatorIds::count; ++op)
        {
            const auto isShown = std::find (shown.begin(), shown.end(), op) != shown.end();
            operatorButtons[(size_t) op]->setVisible (isShown);

            auto& controls = *operators[(size_t) op];
            const auto selected = op == selectedOperator;
            const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) op]) + "_tune"));

            for (auto* control : controls.all())
                control->setVisible (selected);

            controls.ratio.setVisible (selected && tune == OscTuning::Ratio);
            controls.snap.setVisible (selected && tune == OscTuning::Ratio);
            controls.fixedHz.setVisible (selected && tune == OscTuning::Fixed);
        }
    }

    void layoutOperatorCard()
    {
        auto inner = operatorCard.reduced (10, 0);
        inner.removeFromTop (26);

        auto tabs = inner.removeFromTop (22);
        const auto tabWidth = juce::jmin (64, tabs.getWidth() / OscillatorIds::count);

        for (int op = 0; op < OscillatorIds::count; ++op)
            if (std::find (shown.begin(), shown.end(), op) != shown.end())
                operatorButtons[(size_t) op]->setBounds (tabs.removeFromLeft (tabWidth).reduced (2, 0));

        inner.removeFromTop (6);
        auto& controls = *operators[(size_t) selectedOperator];
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
        for (auto* item : { &controls.semi, &controls.fine, &controls.level, &controls.keyLevel })
            bottom.push_back (item);

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

        // Mode, ring mod, the noise colour and sync across the top, labels
        // above like every card's controls (smaller dials: the cells below
        // need the height).
        layoutRow (inner.removeFromTop (72), { &mode, ringMod.get(), noiseColourKnob.get(), &hardSync });
        inner.removeFromTop (6);

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
        heads.removeFromLeft (70);
        const auto columnWidth = heads.getWidth() / count;

        for (const auto i : shown)
            columnHeads[(size_t) i] = heads.removeFromLeft (columnWidth);

        inner.removeFromTop (4);

        const auto rowHeight = inner.getHeight() / (count + 1);

        // Short cells (six oscillators) get compact knobs, their values
        // drawn in the cell's corner: the knob's own value box overlapped it.
        compactCells = rowHeight < 82;
        const auto layoutRow = [&] (juce::Rectangle<int> row, auto&& knobFor, auto&& storeCell)
        {
            for (const auto target : shown)
            {
                auto cell = row.removeFromLeft (columnWidth).reduced (4, 0);
                storeCell (target, cell);
                auto& knob = knobFor (target);
                knob.setCompact (compactCells);
                if (compactCells)
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 22, 110);
                    knob.setBounds (cell.withSizeKeepingCentre (knobSize, knobSize).translated (0, -6));
                }
                else
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 8, 110);
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
            auto head = row.removeFromLeft (70);
            rowHeads[(size_t) source] = layoutHead (head);
            // The row's name, then its OUT switch with that name above it.
            const auto& block = rowHeads[(size_t) source];
            outs[(size_t) source]->setBounds (block.withTrimmedTop (20).withHeight (13 + juce::jmin (22, block.getHeight() - 33)));

            layoutRow (row,
                       [this, source] (int target) -> KnobControl& { return *knobs[(size_t) source][(size_t) target]; },
                       [this, source] (int target, juce::Rectangle<int> cell) { cells[(size_t) source][(size_t) target] = cell; });
        }

        auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
        auto head = row.removeFromLeft (70);
        noiseHead = layoutHead (head);
        layoutRow (row,

                   [this] (int target) -> KnobControl& { return *noiseKnobs[(size_t) target]; },
                   [this] (int target, juce::Rectangle<int> cell) { noiseCells[(size_t) target] = cell; });
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    ComboControl mode;
    ToggleControl hardSync;
    std::unique_ptr<KnobControl> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    bool compactCells = false;
    std::unique_ptr<KnobControl> noiseColourKnob;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> operatorButtons;
    std::array<juce::Rectangle<int>, OscillatorIds::count> columnHeads, rowHeads, noiseCells;
    std::array<std::array<juce::Rectangle<int>, OscillatorIds::count>, OscillatorIds::count> cells;
    juce::Rectangle<int> matrixCard, operatorCard, algorithmsTitle, operatorsTitle, noiseHead;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0;
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
