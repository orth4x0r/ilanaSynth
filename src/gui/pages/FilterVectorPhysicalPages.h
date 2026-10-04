// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// One filter: its type picker and slope in the header, then only the knobs
// its model uses (UI review 6: the type is a compact menu with arrows, not a
// 12-button grid, so the card is one row of knobs).
class FilterPanel : public juce::Component,
                    private juce::Timer
{
public:
    FilterPanel (IlanaSynthAudioProcessor& p, int index, juce::Colour colourIn)
        : processorRef (p),
          prefix (index == 1 ? "f1" : "f2"),
          title ("FILTER " + juce::String (index)),
          colour (colourIn),
          picker (p, prefix + "_type", colourIn, "Filter " + juce::String (index) + " type"),
          slope (p.apvts, prefix + "_slope", colourIn),
          cutoff (p.apvts, prefix + "_cutoff", "CUTOFF", colourIn, false),
          reso (p.apvts, prefix + "_reso", "RESO", colourIn, false),
          drive (p.apvts, prefix + "_drive", "DRIVE", colourIn, false),
          env (p.apvts, prefix + "_env", "ENV AMT", colourIn, false),
          key (p.apvts, prefix + "_keytrack", "KEY TRK", colourIn, false),
          fm (p.apvts, prefix + "_fm", "AUDIO FM", colourIn, false),
          morph (p.apvts, prefix + "_morph", "MORPH", colourIn, false)
    {
        addAll (*this, picker, slope, cutoff, reso, drive, env, key, fm, morph);
        refreshType();
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour.withAlpha (0.35f));

        const auto header = getLocalBounds().reduced (12, 0).removeFromTop (headerHeight);
        IlanaTheme::paintCardTitle (g, header, title, replaced ? IlanaTheme::Ui::text3 : colour);
    }

    // WEST in Filter 2's place: a note over the dimmed knobs says so.
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! replaced)
            return;

        const auto note = getLocalBounds().withTrimmedTop (headerHeight).reduced (24, 0).withSizeKeepingCentre (getWidth() - 48, 40);
        g.setColour (IlanaTheme::Ui::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (note.toFloat(), 6.0f);
        g.setColour (FilterColours::west().withAlpha (0.6f));
        g.drawRoundedRectangle (note.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawFittedText ("Replaced by WEST: its PLACE is Replace Filter 2.\nThese settings come back when WEST runs after the filters.",
                          note.reduced (10, 2), juce::Justification::centred, 2);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        auto header = area.removeFromTop (headerHeight);
        slope.setBounds (header.removeFromRight (96).reduced (0, 6));
        header.removeFromRight (8);
        // The picker after the title, as wide as it likes up to the slope.
        header.removeFromLeft (IlanaTheme::cardTitleWidth (title) - 4);
        picker.setBounds (header.removeFromLeft (juce::jmin (FilterTypePicker::idealWidth, header.getWidth())).reduced (0, 5));

        area.removeFromBottom (6);

        std::vector<juce::Component*> knobs { &cutoff, &reso, &drive, &env, &key, &fm };

        if (morph.isVisible())
            knobs.push_back (&morph);

        layoutRow (area, knobs);
    }

    static constexpr int headerHeight = 34;

    // The height the card wants at a width: the header and one knob row.
    int preferredHeight (int width) const
    {
        return headerHeight + preferredControlHeight (const_cast<KnobControl*> (&cutoff), (width - 20) / 7 - 6) + 18;
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

    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    void timerCallback() override
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            if ((int) value->load() != type)
                refreshType();

        // Filter 2's card while WEST takes its place: dimmed, and says why
        // (UI review 6, I6-16).
        const auto replacedNow = prefix == "f2" && read ("west_on") > 0.5f && juce::roundToInt (read ("west_pos")) == 1;
        if (replacedNow != replaced)
        {
            replaced = replacedNow;
            for (auto* child : getChildren())
                child->setAlpha (replaced ? IlanaTheme::dimmedAlpha * 0.6f : 1.0f);
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix, title;
    juce::Colour colour;
    FilterTypePicker picker;
    SlopeSwitch slope;
    KnobControl cutoff, reso, drive, env, key, fm, morph;
    int type = -1;
    bool replaced = false;
};

// M8.3: the WEST card: a wavefolder into a low-pass gate, after the filters
// or in Filter 2's place. Its own card on the FILTER page, beside BODY (UI
// review 6, I6-15 / I6-16: it was a tab of Filter 2's card), in its own
// colour.
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

    // Its own colour (not a source's, nor the accent the BODY wears).
    static juce::Colour colour() { return FilterColours::west(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour().withAlpha (0.35f));
        auto header = getLocalBounds().reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "WEST", juce::roundToInt (read ("west_pos")) == 1 ? "wavefolder and low-pass gate, in Filter 2's place"
                                                                                                    : "wavefolder and low-pass gate, after the filters",
                                     colour(), 60);

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
        // an on/off switch), named above it.
        const auto level = juce::jlimit (0.0f, 1.0f, processorRef.getWestGateLevel());
        auto cell = plot.withTrimmedLeft (plot.getWidth() * 0.66f).reduced (8.0f, 6.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("GATE", cell.removeFromTop (14.0f), juce::Justification::centredLeft);
        const auto meter = cell.withSizeKeepingCentre (cell.getWidth(), 7.0f);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (meter, 3.0f);
        const auto lit = meter.withWidth (meter.getWidth() * level);
        if (level > 0.001f)
        {
            IlanaTheme::paintGlow (g, lit, 3.0f, colour(), 0.4f + 0.6f * level);
            g.setColour (colour());
            g.fillRoundedRectangle (lit, 3.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        area.removeFromTop (30);
        area.removeFromBottom (4);
        // Its on switch in the header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), 14));

        // The menus in a row with the picture beside them, then the knobs.
        auto top = area.removeFromTop (juce::jmin (52, area.getHeight() / 3));
        picture = top.removeFromRight (top.getWidth() * 2 / 5).reduced (4, 2);
        const auto menuWidth = top.getWidth() / 3;
        for (auto* menu : { &position, &mode, &source })
            menu->setBounds (top.removeFromLeft (menuWidth).reduced (3, 2));
        area.removeFromTop (2);
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
            const auto alpha = active ? 1.0f : IlanaTheme::dimmedAlpha;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        // (The header says where WEST sits, so a change repaints it all.)
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    ToggleControl on;
    ComboControl position, mode, source;
    KnobControl fold, symmetry, stages, decay, resonance, strike, open;
    juce::Rectangle<int> picture;
};

// M8.5: the VECTOR page. The vector pad (four oscillators at the corners,
// moved by hand, by a path or by its wander) and EVOLVE (each macro
// drifting within a range; FREEZE keeps where they are).
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
          // The pad's own wander, apart from the oscillators' analog drift
          // (UI review 6, I6-25).
          drift (p.apvts, "vec_drift", "WANDER", colour(), true),
          driftRate (p.apvts, "vec_drift_rate", "WANDER RATE", colour(), true),
          chipX (ModNames::sourceUpper ((int) Mod::Source::VectorX), (int) Mod::Source::VectorX),
          chipY (ModNames::sourceUpper ((int) Mod::Source::VectorY), (int) Mod::Source::VectorY)
    {
        addAll (*this, pad, on, path, cornerA, cornerB, cornerC, cornerD, x, y, rate, drift, driftRate);
        path.showAsSwitch();

        // Vector X / Y as sources, to drag onto any knob, while the vector
        // plays (UI review 6, S36).
        chipX.valueProvider = [this] { return processorRef.getVectorPosition().x; };
        chipY.valueProvider = [this] { return processorRef.getVectorPosition().y; };
        addChildComponent (chipX);
        addChildComponent (chipY);

        // EVOLVE: a row for each macro that evolves, and "+ MACRO" for the
        // others (UI review 6, S36, V29).
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            evolveAmount.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve", "EVOLVE",
                                                                    evolveColour(), true));
            evolveRate.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve_rate", "RATE",
                                                                  evolveColour(), true));
            addChildComponent (*evolveAmount.back());
            addChildComponent (*evolveRate.back());
        }
        freeze.setButtonText ("FREEZE");
        freeze.setTooltip ("Keeps the macros where Evolve has taken them, and stops the drift.");
        freeze.onClick = [this] { processorRef.freezeEvolve(); };
        addAndMakeVisible (freeze);
        addMacro.setButtonText ("+  MACRO");
        addMacro.setTooltip ("Let another macro drift within a range");
        addMacro.onClick = [this] { showMacroMenu(); };
        addAndMakeVisible (addMacro);
        updateRows();
        startTimerHz (20);
    }

    // Not modulation sources, so not in a source's colour: the accent.
    static juce::Colour colour() { return IlanaTheme::accent(); }
    static juce::Colour evolveColour() { return IlanaTheme::accent(); }

    // The macros with an EVOLVE row (the UI test reads them).
    const std::vector<int>& getEvolveRows() const { return rows; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintCard (g, vectorCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, evolveCard.toFloat(), 7.0f, evolveColour().withAlpha (0.35f));

        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR",
                                     chipX.isVisible() ? "four oscillators at the corners; drag" : "four oscillators at the corners",
                                     colour(), vectorCard.getRight() - chipX.getX() + 6);

        header = evolveCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "EVOLVE", "macros drift in range", evolveColour(), 110);

        if (rows.empty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawFittedText ("No macro evolves. + MACRO lets one drift on its own within a range.",
                              addMacro.getBounds().translated (0, -50).withHeight (40).withX (evolveCard.getX() + 14)
                                  .withWidth (evolveCard.getWidth() - 28),
                              juce::Justification::centred, 2);
        }

        // Each macro: its name, where it is set and where it has drifted to,
        // with a hairline between rows.
        for (size_t r = 0; r < rows.size(); ++r)
        {
            const auto m = rows[r];
            const auto row = macroRows[r];
            if (r > 0)
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
        // The vector's on switch in its header, like every card's; the
        // source chips before it.
        on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14));
        const auto chipWidth = (int) std::ceil (juce::jmax (chipX.getNaturalWidth(), chipY.getNaturalWidth()));
        auto chips = juce::Rectangle<int> (on.getX() - 8 - 2 * chipWidth - 4, vectorCard.getY() + 3, 2 * chipWidth + 4, 22);
        chipX.setBounds (chips.removeFromLeft (chipWidth));
        chips.removeFromLeft (4);
        chipY.setBounds (chips.removeFromLeft (chipWidth));
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

        auto list = evolveCard.reduced (12, 0);
        list.removeFromTop (30);
        // FREEZE is an action on the whole card: in its header, at the right.
        freeze.setBounds (evolveCard.getRight() - 12 - 96, evolveCard.getY() + 4, 96, 20);
        list.removeFromBottom (10);
        const auto rowHeight = juce::jmin (96, list.getHeight() / juce::jmax (1, (int) rows.size() + 1));

        for (size_t r = 0; r < rows.size(); ++r)
        {
            const auto m = (size_t) rows[r];
            auto row = list.removeFromTop (rowHeight);
            macroRows[r] = row.withWidth (116).withTrimmedTop (8);
            row.removeFromLeft (120);
            // A gap under each row, so a row's labels don't read as the
            // values of the row above.
            row.removeFromBottom (8);
            evolveAmount[m]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
            evolveRate[m]->setBounds (row.reduced (2, 0));
        }

        if (rows.empty())
            list = list.withSizeKeepingCentre (list.getWidth(), 60);

        addMacro.setBounds (list.removeFromTop (40).withSizeKeepingCentre (120, 26));
    }

private:
    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool evolves (int macro) const { return readParam ("macro" + juce::String (macro + 1) + "_evolve") > 0.0005f; }

    // A row for each macro that evolves, or was added here.
    void updateRows()
    {
        std::vector<int> wanted;

        for (int m = 0; m < Mod::numMacros; ++m)
            if (evolves (m) || added[(size_t) m])
                wanted.push_back (m);

        addMacro.setVisible ((int) wanted.size() < Mod::numMacros);

        if (wanted == rows)
            return;

        rows = wanted;

        for (int m = 0; m < Mod::numMacros; ++m)
        {
            const auto shown = std::find (rows.begin(), rows.end(), m) != rows.end();
            evolveAmount[(size_t) m]->setVisible (shown);
            evolveRate[(size_t) m]->setVisible (shown);
        }

        resized();
        repaint();
    }

    void showMacroMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Evolve a macro");

        for (int m = 0; m < Mod::numMacros; ++m)
            if (std::find (rows.begin(), rows.end(), m) == rows.end())
                menu.addItem (m + 1, processorRef.getMacroName (m));

        juce::Component::SafePointer<VectorPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addMacro), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->showMacro (result - 1);
        });
    }

public:
    // Gives a macro its EVOLVE row (the "+ MACRO" menu; the UI test).
    void showMacro (int macro)
    {
        added[(size_t) juce::jlimit (0, Mod::numMacros - 1, macro)] = true;
        updateRows();
    }

private:
    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto active = readParam ("vec_on") > 0.5f;
        for (juce::Component* c : { (juce::Component*) &path, (juce::Component*) &cornerA, (juce::Component*) &cornerB,
                                     (juce::Component*) &cornerC, (juce::Component*) &cornerD, (juce::Component*) &x,
                                     (juce::Component*) &y, (juce::Component*) &rate, (juce::Component*) &drift,
                                     (juce::Component*) &driftRate, (juce::Component*) &pad })
        {
            const auto alpha = active ? 1.0f : IlanaTheme::dimmedAlpha;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        rate.setAlpha (active && readParam ("vec_path") > 0.5f ? 1.0f : IlanaTheme::dimmedAlpha);

        if (chipX.isVisible() != active)
        {
            chipX.setVisible (active);
            chipY.setVisible (active);
            repaint (vectorCard);
        }

        updateRows();
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint (evolveCard);
    }

    IlanaSynthAudioProcessor& processorRef;
    VectorPadDisplay pad;
    ToggleControl on, path;
    ComboControl cornerA, cornerB, cornerC, cornerD;
    KnobControl x, y, rate, drift, driftRate;
    ModSourceChip chipX, chipY;
    std::vector<std::unique_ptr<KnobControl>> evolveAmount, evolveRate;
    juce::TextButton freeze, addMacro;
    juce::Rectangle<int> vectorCard, evolveCard;
    std::array<juce::Rectangle<int>, Mod::numMacros> macroRows;
    std::array<bool, Mod::numMacros> added {};
    std::vector<int> rows;
};

// M8.7: the PHYSICAL page. The big view of one physical oscillator (the
// first in Physical mode unless another is picked): its string moving, an
// electric piano's pickup, and every string control, built from the same
// list as its OSC card (UI review 6, S13, I6-18). The body and the
// soundboard are edited in one place each (FILTER, OSC > ACOUSTIC KEYS):
// here they are a summary with links (S14, I6-17).
class PhysicalPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit PhysicalPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p)
    {
        addAndMakeVisible (view);
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

        bodyLink.setButtonText ("FILTER");
        bodyLink.setTooltip ("The resonator body is edited on the FILTER page");
        bodyLink.onClick = [this]
        {
            if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                editor->showPage ("FILTER");
        };
        boardLink.setButtonText ("ACOUSTIC KEYS");
        boardLink.setTooltip ("The soundboard is edited on OSC, under ACOUSTIC KEYS");
        boardLink.onClick = [this] { showAcousticKeys(); };
        for (auto* button : { &bodyLink, &boardLink })
        {
            button->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
            addAndMakeVisible (*button);
        }

        choose (firstPhysical(), false);
        startTimerHz (5);
    }

    ~PhysicalPage() override { pickup.reset(); }

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
        title (viewCard, "PHYSICAL", "OSC " + juce::String (chosen + 1) + "'s string, moving as you play", colour());
        title (stringCard, "OSC " + juce::String (chosen + 1), "same controls as its OSC card", colour());
        title (bodyCard, "BODY", "", colour());

        for (const auto& [area, name] : rowLabels)
        {
            g.setColour (colour().withAlpha (0.8f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (name, area, juce::Justification::centredLeft);
        }

        // The body and the soundboard: what they are set to, and where.
        const auto summary = [&] (juce::Rectangle<int> line, const juce::String& name, bool isOn, const juce::String& choice)
        {
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (name, line.removeFromLeft (130), juce::Justification::centredLeft);
            g.setColour (isOn ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText (isOn ? "on, " + choice : juce::String ("off"), line, juce::Justification::centredLeft, true);
        };
        summary (bodyLine.withRight (bodyLink.getX() - 6), "RESONATOR BODY", readParam ("res_on") > 0.5f, choiceName ("body_type"));
        summary (boardLine.withRight (boardLink.getX() - 6), "SOUNDBOARD", readParam ("sb_on") > 0.5f, choiceName ("sb_model"));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        const auto physical = isPhysical (chosen);
        rowLabels.clear();

        for (juce::Component* c : { (juce::Component*) &bodyLink, (juce::Component*) &boardLink })
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
        viewCard = area.removeFromLeft (area.getWidth() * 50 / 100);
        area.removeFromLeft (10);
        bodyCard = area.removeFromBottom (96);
        area.removeFromBottom (10);
        stringCard = area;

        auto inner = viewCard.reduced (10, 0);
        inner.removeFromTop (30);
        auto picker = inner.removeFromTop (30);
        for (auto& button : oscButtons)
            if (button.isVisible())
                button.setBounds (picker.removeFromLeft (juce::jmin (84, picker.getWidth() / OscillatorIds::count)).reduced (3, 3));
        inner.removeFromTop (6);
        inner.removeFromBottom (10);

        // A tine or reed: its pickup under the string (S13).
        if (pickup != nullptr && pickup->isVisible())
        {
            pickup->setBounds (inner.removeFromBottom (inner.getHeight() * 32 / 100));
            inner.removeFromBottom (8);
        }

        view.setBounds (inner);

        // The rows, each named above its controls, on a grid of five.
        auto controls = stringCard.reduced (10, 0);
        controls.removeFromTop (30);
        controls.removeFromBottom (8);
        auto lines = 0;
        for (const auto& row : layoutRows)
            lines += ((int) row.second.size() + columns - 1) / columns;
        const auto lineHeight = juce::jlimit (60, 96, (controls.getHeight() - (int) layoutRows.size() * 20) / juce::jmax (1, lines));

        for (const auto& [name, items] : layoutRows)
        {
            rowLabels.push_back ({ controls.removeFromTop (20).reduced (4, 0), name });

            for (size_t first = 0; first < items.size(); first += (size_t) columns)
            {
                auto line = controls.removeFromTop (lineHeight);
                const auto cell = line.getWidth() / columns;

                for (size_t k = first; k < juce::jmin (items.size(), first + (size_t) columns); ++k)
                    items[k]->setBounds (line.removeFromLeft (cell).reduced (2, 2));
            }
        }

        auto body = bodyCard.reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8);
        bodyLine = body.removeFromTop (body.getHeight() / 2);
        boardLine = body;
        bodyLink.setBounds (bodyLine.removeFromRight (120).withSizeKeepingCentre (120, 22));
        boardLink.setBounds (boardLine.removeFromRight (120).withSizeKeepingCentre (120, 22));
        bodyLine = bodyLine.withRight (bodyLink.getRight());
        boardLine = boardLine.withRight (boardLink.getRight());
    }

    int getChosenOscillator() const { return chosen; }

    // The controls shown for the chosen oscillator's string, in order (the
    // UI test compares them with the OSC card's).
    juce::StringArray getControlIds() const { return controlIds; }

private:
    static constexpr int columns = 5;

    juce::String prefix() const { return OscillatorIds::prefixes[(size_t) chosen]; }

    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    juce::String choiceName (const juce::String& id) const
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (id)))
            return choice->getCurrentChoiceName();
        return {};
    }

    bool isPhysical (int osc) const
    {
        return juce::roundToInt (readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode")) == 1;
    }

    bool anyPhysical() const
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (isPhysical (i) && processorRef.isOscillatorShown (i))
                return true;
        return false;
    }

    int firstPhysical() const
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (isPhysical (i) && processorRef.isOscillatorShown (i))
                return i;
        return 0;
    }

    void showAcousticKeys()
    {
        auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>();
        auto* section = findParentComponentOfClass<SectionPage>();

        if (editor == nullptr || section == nullptr)
            return;

        if (auto* viewport = dynamic_cast<OscPageViewport*> (section->getPage (section->indexOf ("OSC"))))
            if (auto* page = viewport->getPage())
                page->selectShared (3);

        editor->showPage ("OSC");
    }

    void choose (int osc, bool byHand)
    {
        chosen = juce::jlimit (0, OscillatorIds::count - 1, osc);
        pickedByHand = pickedByHand || byHand;
        const auto id = prefix();
        view.setOscillator (id);
        view.setColour (colour());

        // The string's controls, rebound to the chosen oscillator, from
        // the list its OSC card uses.
        layoutRows.clear();
        controls.clear();
        controlIds.clear();
        if (excite == nullptr || excitePrefix != id) // not while its own menu may be calling back
        {
            excite = std::make_unique<ComboControl> (processorRef.apvts, id + "_excite", "EXCITE");
            addAndMakeVisible (*excite);
            excitePrefix = id;
        }
        shownExcite = juce::roundToInt (readParam (id + "_excite"));

        for (const auto& [name, specs] : physicalControlRows (shownExcite))
        {
            std::vector<juce::Component*> items;

            for (const auto& spec : specs)
            {
                const juce::String suffix (spec.suffix);

                if (processorRef.apvts.getParameter (id + suffix) == nullptr)
                    continue;

                controlIds.add (id + suffix);

                if (suffix == "_excite")
                {
                    items.push_back (excite.get());
                    continue;
                }

                if (suffix == "_string_slap")
                    controls.push_back (std::make_unique<ToggleControl> (processorRef.apvts, id + suffix, spec.label));
                else
                {
                    auto knob = std::make_unique<KnobControl> (processorRef.apvts, id + suffix, spec.label, colour(), false);
                    knob->setSizeRole (IlanaTheme::KnobSize::small);
                    controls.push_back (std::move (knob));
                }

                addAndMakeVisible (*controls.back());
                items.push_back (controls.back().get());
            }

            layoutRows.push_back ({ name, items });
        }

        // A tine or reed's pickup curve, as on its OSC card.
        const auto electric = shownExcite == 7 || shownExcite == 8;
        if (electric && (pickup == nullptr || pickupPrefix != id))
        {
            pickup = std::make_unique<WaveDisplay> (processorRef, id + "_table", id + "_frame", id + "_unison", id + "_spread",
                                                    id + "_detune", false, juce::String {}, id + "_mode", chosen, colour(), false);
            addChildComponent (*pickup);
            pickupPrefix = id;
        }
        if (pickup != nullptr)
            pickup->setVisible (electric);

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
        for (auto& control : controls)
            control->setVisible (physical);
        if (pickup != nullptr)
            pickup->setVisible (physical && (shownExcite == 7 || shownExcite == 8));
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto& button = oscButtons[(size_t) i];
            button.setVisible (processorRef.isOscillatorShown (i));
            button.setAlpha (isPhysical (i) ? 1.0f : 0.5f);
            button.setTooltip (isPhysical (i) ? juce::String() : "OSC " + juce::String (i + 1) + " is not Physical");
        }

        // The PHYSICAL tab greys while no oscillator has a string (UI
        // review 6, V23, S37); it still opens, to offer the switch.
        if (auto* section = findParentComponentOfClass<SectionPage>())
            section->switcher.setItemDimmed (section->indexOf ("PHYSICAL"),
                                             anyPhysical() ? juce::String()
                                                           : juce::String ("No oscillator is Physical: pick Physical in an oscillator's MODE menu "
                                                                           "(OSC page) to give it a string to edit here"));
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

        if (const auto body = readParam ("res_on") + 2.0f * readParam ("sb_on") + 4.0f * readParam ("body_type") + 64.0f * readParam ("sb_model");
            body != shownBody)
        {
            shownBody = body;
            repaint (bodyCard);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    PhysicalView view;
    std::array<juce::TextButton, OscillatorIds::count> oscButtons;
    juce::TextButton makePhysical, bodyLink, boardLink;
    std::unique_ptr<ComboControl> excite;
    juce::String excitePrefix, pickupPrefix;
    std::unique_ptr<WaveDisplay> pickup;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::pair<juce::String, std::vector<juce::Component*>>> layoutRows;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> rowLabels;
    juce::StringArray controlIds;
    int chosen = 0, shownExcite = -1;
    float shownBody = -1.0f;
    bool pickedByHand = false, lastPhysical = false;
    juce::Rectangle<int> emptyCard, viewCard, stringCard, bodyCard, bodyLine, boardLine;
};
} // namespace
