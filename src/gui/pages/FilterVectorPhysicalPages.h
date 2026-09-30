// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// One filter: its type grid, slope switch and only the knobs its model uses.
class FilterPanel : public juce::Component,
                    private juce::Timer
{
public:
    FilterPanel (IlanaSynthAudioProcessor& p, int index, juce::Colour colourIn)
        : processorRef (p),
          prefix (index == 1 ? "f1" : "f2"),
          title ("FILTER " + juce::String (index)),
          colour (colourIn),
          grid (p.apvts, prefix + "_type", colourIn),
          slope (p.apvts, prefix + "_slope", colourIn),
          cutoff (p.apvts, prefix + "_cutoff", "CUTOFF", colourIn, false),
          reso (p.apvts, prefix + "_reso", "RESO", colourIn, false),
          drive (p.apvts, prefix + "_drive", "DRIVE", colourIn, false),
          env (p.apvts, prefix + "_env", "ENV AMT", colourIn, false),
          key (p.apvts, prefix + "_keytrack", "KEY TRK", colourIn, false),
          fm (p.apvts, prefix + "_fm", "AUDIO FM", colourIn, false),
          morph (p.apvts, prefix + "_morph", "MORPH", colourIn, false)
    {
        addAll (*this, grid, slope, cutoff, reso, drive, env, key, fm, morph);
        refreshType();
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour.withAlpha (0.35f));

        auto header = getLocalBounds().reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardTitle (g, header, title, colour);
        header.removeFromLeft (14);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));

        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true)), title);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (FilterType::getNames()[type], header.withTrimmedLeft (titleWidth + 10), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        auto header = area.removeFromTop (28);
        slope.setBounds (header.removeFromRight (96).reduced (0, 5));

        grid.setBounds (area.removeFromTop (juce::jlimit (52, 64, getHeight() / 4) + FilterTypeGrid::labelHeight));
        area.removeFromTop (4);
        area.removeFromBottom (6);

        std::vector<juce::Component*> knobs { &cutoff, &reso, &drive, &env, &key, &fm };

        if (morph.isVisible())
            knobs.push_back (&morph);

        layoutRow (area, knobs);
    }

private:
    // Only the classic models have a slope; FilterType::usesMorph says which use MORPH.
    void refreshType()
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            type = juce::jlimit (0, FilterType::Count - 1, (int) value->load());

        const auto hasSlope = type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
                              && FilterType::usesSlope (type);
        slope.setVisible (hasSlope);
        morph.setVisible (FilterType::usesMorph (type));
        resized();
        repaint();
    }

    void timerCallback() override
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            if ((int) value->load() != type)
                refreshType();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix, title;
    juce::Colour colour;
    FilterTypeGrid grid;
    SlopeSwitch slope;
    KnobControl cutoff, reso, drive, env, key, fm, morph;
    int type = -1;
};

// M8.3: the WEST card: a wavefolder into a low-pass gate. It sits where
// Filter 2's card is (the FILTER 2 / WEST tabs), and runs after the filters
// or in Filter 2's place.
class WestPanel : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    explicit WestPanel (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          on (p.apvts, "west_on", "ON"),
          position (p.apvts, "west_pos", "PLACE"),
          mode (p.apvts, "west_mode", "GATE"),
          source (p.apvts, "west_src", "STRIKE BY"),
          fold (p.apvts, "west_fold", "FOLD", colour(), true),
          symmetry (p.apvts, "west_sym", "SYMMETRY", colour(), true),
          stages (p.apvts, "west_stages", "STAGES", colour(), true),
          decay (p.apvts, "west_decay", "DECAY", colour(), true),
          resonance (p.apvts, "west_res", "RESO", colour(), true),
          strike (p.apvts, "west_strike", "STRIKE", colour(), true),
          open (p.apvts, "west_open", "OPEN", colour(), true)
    {
        addAll (*this, on, position, mode, source, fold, symmetry, stages, decay, resonance, strike, open);
        startTimerHz (30);
    }

    // Not a modulation source: the accent, not a source's colour.
    static juce::Colour colour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour().withAlpha (0.35f));
        auto header = getLocalBounds().reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "WEST", "wavefolder into a low-pass gate", colour());

        // The fold's transfer curve and the gate's vactrol, lit by its level.
        const auto plot = picture.toFloat();
        IlanaTheme::paintWell (g, plot, 5.0f);
        const auto curveArea = plot.withWidth (plot.getWidth() * 0.62f).reduced (8.0f, 6.0f);
        juce::Path curve;
        const auto gain = 0.35 + 11.65 * (double) (read ("west_fold") * read ("west_fold"));
        const auto bias = 0.5 * (double) read ("west_sym");
        const auto stagesNow = juce::jlimit (1, 4, (int) read ("west_stages"));
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = -1.0 + 2.0 * i / 120.0;
            auto y = x * gain + bias;
            for (int s = 0; s < stagesNow; ++s)
            {
                y = Wavefolder::fold (y);
                if (s + 1 < stagesNow)
                    y *= 1.0 + 0.35 * gain / (double) stagesNow;
            }
            const auto px = curveArea.getX() + curveArea.getWidth() * (float) i / 120.0f;
            const auto py = curveArea.getCentreY() - (float) y * curveArea.getHeight() * 0.45f;
            if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
        }
        g.setColour (colour());
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        // How open the gate is right now: a level meter (a lit dot read as
        // an on/off switch).
        const auto level = juce::jlimit (0.0f, 1.0f, processorRef.getWestGateLevel());
        const auto cell = plot.withTrimmedLeft (plot.getWidth() * 0.66f).reduced (10.0f, 8.0f);
        auto meter = cell.withSizeKeepingCentre (8.0f, juce::jmin (cell.getHeight() - 20.0f, 64.0f)).withX (cell.getX() + 10.0f);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (meter, 3.0f);
        const auto lit = meter.withTop (meter.getBottom() - meter.getHeight() * level);
        if (level > 0.001f)
        {
            IlanaTheme::paintGlow (g, lit, 3.0f, colour(), 0.4f + 0.6f * level);
            g.setColour (colour());
            g.fillRoundedRectangle (lit, 3.0f);
        }
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("GATE", cell.withTrimmedLeft (26.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        area.removeFromTop (30);
        auto top = area.removeFromTop (area.getHeight() * 2 / 5);
        auto options = top.removeFromLeft (top.getWidth() / 2);
        picture = top.reduced (4, 2);
        const auto optionHeight = options.getHeight() / 2;
        auto row1 = options.removeFromTop (optionHeight);
        // Its on switch in the header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), 14));
        position.setBounds (row1.reduced (3, 1));
        auto row2 = options;
        mode.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (3, 1));
        source.setBounds (row2.reduced (3, 1));
        area.removeFromTop (4);
        layoutRow (area, { &fold, &symmetry, &stages, &decay, &resonance, &strike, &open });
    }

private:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto active = read ("west_on") > 0.5f;
        for (juce::Component* c : { (juce::Component*) &fold, (juce::Component*) &symmetry, (juce::Component*) &stages,
                                     (juce::Component*) &decay, (juce::Component*) &resonance, (juce::Component*) &strike,
                                     (juce::Component*) &open, (juce::Component*) &mode, (juce::Component*) &source,
                                     (juce::Component*) &position })
        {
            const auto alpha = active ? 1.0f : 0.4f;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint (picture);
    }

    IlanaSynthAudioProcessor& processorRef;
    ToggleControl on;
    ComboControl position, mode, source;
    KnobControl fold, symmetry, stages, decay, resonance, strike, open;
    juce::Rectangle<int> picture;
};

// M8.5: the VECTOR page. The vector pad (four oscillators at the corners,
// moved by hand, by a path or by drift) and EVOLVE (each macro drifting
// within a range; FREEZE keeps where they are).
class VectorPage : public juce::Component,
                   private IlanaAnim::FrameTimer
{
public:
    explicit VectorPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          pad (p),
          on (p.apvts, "vec_on", "ON"),
          path (p.apvts, "vec_path", "PATH"),
          cornerA (p.apvts, "vec_a", "TOP LEFT"),
          cornerB (p.apvts, "vec_b", "TOP RIGHT"),
          cornerC (p.apvts, "vec_c", "BOTTOM LEFT"),
          cornerD (p.apvts, "vec_d", "BOTTOM RIGHT"),
          x (p.apvts, "vec_x", "X", colour(), true),
          y (p.apvts, "vec_y", "Y", colour(), true),
          rate (p.apvts, "vec_rate", "PATH RATE", colour(), true),
          drift (p.apvts, "vec_drift", "DRIFT", colour(), true),
          driftRate (p.apvts, "vec_drift_rate", "DRIFT RATE", colour(), true)
    {
        addAll (*this, pad, on, path, cornerA, cornerB, cornerC, cornerD, x, y, rate, drift, driftRate);
        path.showAsSwitch();
        // The EVOLVE and RATE names head their columns once, on the first row.
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            evolveAmount.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve", m == 0 ? "EVOLVE" : "",
                                                                    evolveColour(), true));
            evolveRate.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve_rate", m == 0 ? "RATE" : "",
                                                                  evolveColour(), true));
            addAndMakeVisible (*evolveAmount.back());
            addAndMakeVisible (*evolveRate.back());
        }
        freeze.setButtonText ("FREEZE");
        freeze.setTooltip ("Keeps the macros where Evolve has taken them, and stops the drift.");
        freeze.onClick = [this] { processorRef.freezeEvolve(); };
        addAndMakeVisible (freeze);
        startTimerHz (20);
    }

    // Not modulation sources, so not in a source's colour: the accent.
    static juce::Colour colour() { return IlanaTheme::accent(); }
    static juce::Colour evolveColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintCard (g, vectorCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, evolveCard.toFloat(), 7.0f, evolveColour().withAlpha (0.35f));

        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR", "four oscillators at the corners; Vector X / Y are mod sources", colour());

        header = evolveCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "EVOLVE", "macros drift in range", evolveColour(), 110);

        // Each macro: its name, where it is set and where it has drifted to,
        // with a hairline between rows.
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            const auto row = macroRows[(size_t) m];
            if (m > 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRect (evolveCard.getX() + 12, row.getY() - 10, evolveCard.getWidth() - 24, 1);
            }
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (processorRef.getMacroName (m).toUpperCase(), row.withWidth (110).withHeight (18), juce::Justification::centredLeft);
            const auto bar = juce::Rectangle<float> ((float) row.getX(), (float) row.getY() + 24.0f, 100.0f, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (bar, 3.0f);
            const auto set = readParam ("macro" + juce::String (m + 1));
            const auto now = processorRef.macroValue (m);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRect (bar.getX() + bar.getWidth() * set - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
            g.setColour (evolveColour());
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ bar.getX() + bar.getWidth() * now, bar.getCentreY() }));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        // The pad gets most of the page: EVOLVE's rows need little width.
        vectorCard = area.removeFromLeft (area.getWidth() * 70 / 100);
        area.removeFromLeft (10);
        evolveCard = area;

        auto inner = vectorCard.reduced (12, 0);
        inner.removeFromTop (30);
        inner.removeFromBottom (12);
        // The pad fills the card's height; the controls take the width left.
        const auto controlsWidth = 250;
        const auto side = juce::jmax (200, juce::jmin (inner.getHeight(), inner.getWidth() - controlsWidth - 12));
        pad.setBounds (inner.removeFromLeft (side).withSizeKeepingCentre (side, side));
        inner.removeFromLeft (12);
        const auto controlsHeight = 40 + 44 + 44 + 6 + 112 * 2 + 18;
        inner = inner.withSizeKeepingCentre (inner.getWidth(), juce::jmin (inner.getHeight(), controlsHeight));
        // The vector's on switch in its header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14));
        auto toggles = inner.removeFromTop (40);
        path.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2).reduced (3, 1));
        auto combos1 = inner.removeFromTop (44);
        cornerA.setBounds (combos1.removeFromLeft (combos1.getWidth() / 2).reduced (3, 1));
        cornerB.setBounds (combos1.reduced (3, 1));
        auto combos2 = inner.removeFromTop (44);
        cornerC.setBounds (combos2.removeFromLeft (combos2.getWidth() / 2).reduced (3, 1));
        cornerD.setBounds (combos2.reduced (3, 1));
        // Knob rows sized to the knobs, with a gap between, so each label
        // sits with its own knob rather than under the row above's values.
        inner.removeFromTop (6);
        const auto knobHeight = juce::jmin (112, inner.getHeight() / 2 - 8);
        layoutRow (inner.removeFromTop (knobHeight), { &x, &y, &rate });
        inner.removeFromTop (18);
        layoutRow (inner.removeFromTop (knobHeight), { &drift, &driftRate, nullptr }); // on the row above's grid

        auto rows = evolveCard.reduced (12, 0);
        rows.removeFromTop (30);
        // FREEZE is an action on the whole card: in its header, at the right.
        freeze.setBounds (evolveCard.getRight() - 12 - 96, evolveCard.getY() + 4, 96, 20);
        const auto rowHeight = rows.getHeight() / Mod::numMacros;
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            auto row = rows.removeFromTop (rowHeight);
            macroRows[(size_t) m] = row.withWidth (116).withTrimmedTop (8);
            row.removeFromLeft (120);
            // A gap under each row, so a row's labels don't read as the
            // values of the row above.
            row.removeFromBottom (8);
            evolveAmount[(size_t) m]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
            evolveRate[(size_t) m]->setBounds (row.reduced (2, 0));
        }
    }

private:
    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto active = readParam ("vec_on") > 0.5f;
        for (juce::Component* c : { (juce::Component*) &path, (juce::Component*) &cornerA, (juce::Component*) &cornerB,
                                     (juce::Component*) &cornerC, (juce::Component*) &cornerD, (juce::Component*) &x,
                                     (juce::Component*) &y, (juce::Component*) &rate, (juce::Component*) &drift,
                                     (juce::Component*) &driftRate, (juce::Component*) &pad })
        {
            const auto alpha = active ? 1.0f : 0.45f;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        rate.setAlpha (active && readParam ("vec_path") > 0.5f ? 1.0f : 0.45f);
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint (evolveCard);
    }

    IlanaSynthAudioProcessor& processorRef;
    VectorPadDisplay pad;
    ToggleControl on, path;
    ComboControl cornerA, cornerB, cornerC, cornerD;
    KnobControl x, y, rate, drift, driftRate;
    std::vector<std::unique_ptr<KnobControl>> evolveAmount, evolveRate;
    juce::TextButton freeze;
    juce::Rectangle<int> vectorCard, evolveCard;
    std::array<juce::Rectangle<int>, Mod::numMacros> macroRows;
};

// M8.7: the PHYSICAL page. The animated string, its exciter and the body
// for one oscillator (the first in Physical mode unless another is picked),
// with that oscillator's string controls and the shared body and
// soundboard switches beside it.
class PhysicalPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit PhysicalPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p),
          resOn (p.apvts, "res_on", "BODY"),
          bodyType (p.apvts, "body_type", "BODY TYPE"),
          sbOn (p.apvts, "sb_on", "SOUNDBOARD"),
          sbModel (p.apvts, "sb_model", "BOARD MODEL")
    {
        addAll (*this, view, resOn, bodyType, sbOn, sbModel);
        // Two named switches side by side (the body and the soundboard are
        // separate), rather than one card switch that reads as "all off".
        resOn.showAsSwitch();
        sbOn.showAsSwitch();
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto& button = oscButtons[(size_t) i];
            button.setButtonText ("OSC " + juce::String (i + 1));
            button.setClickingTogglesState (false);
            IlanaTheme::makePill (button, IlanaTheme::oscColour (i));
            button.onClick = [this, i] { choose (i, true); };
            addAndMakeVisible (button);
        }
        makePhysical.setButtonText ("SWITCH TO PHYSICAL");
        makePhysical.setTooltip ("Puts this oscillator in Physical mode.");
        makePhysical.onClick = [this]
        {
            if (auto* parameter = processorRef.apvts.getParameter (prefix() + "_mode"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
        };
        addChildComponent (makePhysical);
        choose (firstPhysical(), false);
        startTimerHz (5);
    }

    // The page belongs to the chosen oscillator: its identity colour.
    juce::Colour colour() const { return IlanaTheme::oscColour (chosen); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        const auto title = [&g] (juce::Rectangle<int> card, const juce::String& name, const juce::String& note, juce::Colour tag)
        {
            IlanaTheme::paintCardHeader (g, card.reduced (12, 0).removeFromTop (28), name, note, tag);
        };

        // Not a Physical oscillator: one centred card that says so and
        // offers the switch, rather than an empty picture beside it.
        if (! isPhysical (chosen))
        {
            IlanaTheme::paintCard (g, emptyCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
            title (emptyCard, "PHYSICAL", "a string, what excites it and its body", colour());
            static const char* const plays[] { "a wavetable", "a string", "a sample", "grains", "the live input" };
            const auto mode = juce::jlimit (0, 4, juce::roundToInt (readParam (prefix() + "_mode")));
            auto message = makePhysical.getBounds().withHeight (44).translated (0, -58).withWidth (emptyCard.getWidth() - 28)
                                                  .withX (emptyCard.getX() + 14);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (chosen + 1) + " plays " + plays[mode] + ", so it has no string.",
                        message.removeFromTop (22), juce::Justification::centred);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("Switch it to Physical to edit its string, exciter and body here.", message, juce::Justification::centred);
            return;
        }

        IlanaTheme::paintCard (g, viewCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, stringCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, bodyCard.toFloat(), 7.0f, colour().withAlpha (0.25f));
        title (viewCard, "PHYSICAL", "the string, what excites it and the body, from OSC " + juce::String (chosen + 1) + "'s settings", colour());
        title (stringCard, "OSC " + juce::String (chosen + 1) + " STRING", "", colour());
        title (bodyCard, "BODY", "a body and a soundboard for every string", colour());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        const auto physical = isPhysical (chosen);

        for (juce::Component* c : { (juce::Component*) &resOn, (juce::Component*) &bodyType,
                                    (juce::Component*) &sbOn, (juce::Component*) &sbModel })
            c->setVisible (physical);

        // Not physical: the view still shows, in its own preview look, what
        // the switch gives (drawn from the oscillator's string settings).
        view.setInterceptsMouseClicks (physical, physical);

        if (! physical)
        {
            emptyCard = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 900), juce::jmin (area.getHeight(), 600));
            auto inner = emptyCard.reduced (14, 0);
            inner.removeFromTop (38);
            auto shownButtons = 0;
            for (auto& button : oscButtons)
                shownButtons += button.isVisible() ? 1 : 0;
            auto picker = inner.removeFromTop (30).withSizeKeepingCentre (juce::jmax (1, shownButtons) * 96, 30);
            for (auto& button : oscButtons)
                if (button.isVisible())
                    button.setBounds (picker.removeFromLeft (96).reduced (3, 3));
            makePhysical.setBounds (juce::Rectangle<int> (240, 34).withCentre ({ emptyCard.getCentreX(), emptyCard.getBottom() - 40 }));
            inner.removeFromTop (6);
            inner.removeFromBottom (130); // the message and the switch
            view.setBounds (inner);
            return;
        }

        emptyCard = {};
        viewCard = area.removeFromLeft (area.getWidth() * 60 / 100);
        area.removeFromLeft (10);
        bodyCard = area.removeFromBottom (juce::jmin (150, area.getHeight() / 3));
        area.removeFromBottom (10);
        stringCard = area;

        auto inner = viewCard.reduced (10, 0);
        inner.removeFromTop (30);
        auto picker = inner.removeFromTop (30);
        const auto buttonWidth = picker.getWidth() / OscillatorIds::count;
        for (auto& button : oscButtons)
            button.setBounds (picker.removeFromLeft (buttonWidth).reduced (3, 3));
        inner.removeFromTop (6);
        view.setBounds (inner.withTrimmedBottom (10));

        auto controls = stringCard.reduced (10, 0);
        controls.removeFromTop (30);
        makePhysical.setBounds (juce::Rectangle<int> (220, 34).withCentre (controls.getCentre().translated (0, 20)));
        if (excite != nullptr)
        {
            excite->setBounds (controls.removeFromTop (44).reduced (3, 1));
            controls.removeFromTop (4);
            const auto rows = (int) (knobs.size() + 3) / 4;
            const auto rowHeight = juce::jmin (120, controls.getHeight() / juce::jmax (1, rows));
            for (int row = 0; row < rows; ++row)
            {
                std::vector<juce::Component*> items;
                for (size_t k = (size_t) row * 4; k < juce::jmin (knobs.size(), (size_t) row * 4 + 4); ++k)
                    items.push_back (knobs[k].get());
                while (items.size() < 4)
                    items.push_back (nullptr);
                auto rowArea = controls.removeFromTop (rowHeight);
                const auto cell = rowArea.getWidth() / 4;
                for (auto* item : items)
                {
                    auto slot = rowArea.removeFromLeft (cell);
                    if (item != nullptr)
                        item->setBounds (slot.reduced (2, 6));
                }
            }
        }

        // The body's switch in its header; its type, and the soundboard's
        // switch and model, in one row under it.
        auto body = bodyCard.reduced (10, 0).withTrimmedTop (30).withTrimmedBottom (6);
        layoutRow (body, { &resOn, &bodyType, &sbOn, &sbModel });
    }

    int getChosenOscillator() const { return chosen; }

private:
    juce::String prefix() const { return OscillatorIds::prefixes[(size_t) chosen]; }

    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool isPhysical (int osc) const
    {
        return juce::roundToInt (readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode")) == 1;
    }

    int firstPhysical() const
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (isPhysical (i) && processorRef.isOscillatorShown (i))
                return i;
        return 0;
    }

    void choose (int osc, bool byHand)
    {
        chosen = juce::jlimit (0, OscillatorIds::count - 1, osc);
        pickedByHand = pickedByHand || byHand;
        const auto id = prefix();
        view.setOscillator (id);
        view.setColour (colour());

        // The string's controls, rebound to the chosen oscillator.
        knobs.clear();
        if (excite == nullptr || excitePrefix != id) // not while its own menu may be calling back
        {
            excite = std::make_unique<ComboControl> (processorRef.apvts, id + "_excite", "EXCITE");
            addAndMakeVisible (*excite);
            excitePrefix = id;
        }
        // The knobs this exciter uses (as on the OSC card).
        shownExcite = juce::roundToInt (readParam (id + "_excite"));
        const auto hammer = shownExcite == 5 || shownExcite == 9, feedback = shownExcite == 10, bow = shownExcite == 4;
        std::vector<std::pair<const char*, const char*>> knobIds {
            { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_string_sustain", feedback ? "FEEDBACK" : "SUSTAIN" },
            { "_string_stiffness", "STIFF" }, { "_string_excite_pos", "EXCITE POS" }, { "_string_pickup", "PICKUP" } };
        if (hammer)
            knobIds.push_back ({ "_hammer_hard", "HAMMER" });
        else if (bow)
            knobIds.insert (knobIds.end(), { { "_bow_pressure", "PRESSURE" }, { "_bow_speed", "SPEED" } });
        else if (feedback)
            knobIds.insert (knobIds.end(), { { "_fb_gain", "AMP GAIN" }, { "_fb_distance", "DISTANCE" } });
        else
            knobIds.insert (knobIds.end(), { { "_string_pick_hardness", "HARDNESS" }, { "_bridge_buzz", "BUZZ" } });
        for (const auto& [suffix, label] : knobIds)
            if (processorRef.apvts.getParameter (id + suffix) != nullptr)
            {
                knobs.push_back (std::make_unique<KnobControl> (processorRef.apvts, id + suffix, label, colour(), false));
                addAndMakeVisible (*knobs.back());
            }
        for (int i = 0; i < OscillatorIds::count; ++i)
            oscButtons[(size_t) i].setToggleState (i == chosen, juce::dontSendNotification);
        updateAvailability();
        resized();
        repaint();
    }

    void updateAvailability()
    {
        const auto physical = isPhysical (chosen);
        makePhysical.setVisible (! physical);
        if (excite != nullptr)
            excite->setVisible (physical);
        for (auto& knob : knobs)
            knob->setVisible (physical);
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto& button = oscButtons[(size_t) i];
            button.setVisible (processorRef.isOscillatorShown (i));
            button.setAlpha (isPhysical (i) ? 1.0f : 0.5f);
        }
        const auto resonator = readParam ("res_on") > 0.5f;
        bodyType.setAlpha (resonator ? 1.0f : 0.45f);
        sbModel.setAlpha (readParam ("sb_on") > 0.5f ? 1.0f : 0.45f);
    }

    void timerCallback() override
    {
        // Follow the patch: a preset with its Physical string on another
        // oscillator moves the view there, unless one was picked by hand.
        if (! pickedByHand && ! isPhysical (chosen) && isPhysical (firstPhysical()))
            choose (firstPhysical(), false);
        else if (juce::roundToInt (readParam (prefix() + "_excite")) != shownExcite)
            choose (chosen, false);
        if (lastPhysical != isPhysical (chosen))
        {
            lastPhysical = isPhysical (chosen);
            resized();
            repaint();
        }
        updateAvailability();
    }

    IlanaSynthAudioProcessor& processorRef;
    PhysicalView view;
    ToggleControl resOn;
    ComboControl bodyType;
    ToggleControl sbOn;
    ComboControl sbModel;
    std::array<juce::TextButton, OscillatorIds::count> oscButtons;
    juce::TextButton makePhysical;
    std::unique_ptr<ComboControl> excite;
    juce::String excitePrefix;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    int chosen = 0, shownExcite = -1;
    bool pickedByHand = false, lastPhysical = false;
    juce::Rectangle<int> emptyCard, viewCard, stringCard, bodyCard;
};
} // namespace
