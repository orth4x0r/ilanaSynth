// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// One filter: its type picker and slope in the header, then only the knobs
// its model uses (UI review 6: the type is a compact menu with arrows, not a
// 12-button grid, so the card is one row of knobs).
class FilterPanel : public juce::Component,
                    public juce::SettableTooltipClient,
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
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour.withAlpha (passThrough ? 0.15f : 0.35f));

        const auto header = getLocalBounds().reduced (12, 0).removeFromTop (headerHeight);
        IlanaTheme::paintCardTitle (g, header, title, replaced ? IlanaTheme::Ui::text3 : passThrough ? colour.withAlpha (0.3f) : colour);

        // (A filter open at the top does nothing: the card steps back and its
        // tooltip says so; no state is spelled out in text without a switch,
        // and a pill that looked like the slope's buttons is gone: V10-10.)
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
        IlanaTheme::drawFitted (g, "Replaced by WEST (its PLACE is Replaces F2).\nThese settings come back when WEST runs after the filters.",
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
        // A filter that passes everything (open at 20 kHz) dims, as an off
        // module would, all but its type and CUTOFF, which bring it in (UI
        // review 8, V8-39).
        const auto openNow = FilterDisplay::isPassThrough (processorRef, prefix == "f1" ? 0 : 1);
        if (replacedNow != replaced || openNow != passThrough)
        {
            replaced = replacedNow;
            passThrough = openNow;
            for (auto* child : getChildren())
                child->setAlpha (replaced ? IlanaTheme::dimmedAlpha * 0.6f
                                          : passThrough && child != &cutoff && child != &picker ? openAlpha : 1.0f);
            setTooltip (passThrough && ! replaced ? title.substring (0, 1) + title.substring (1).toLowerCase()
                                                        + " is open: it passes everything. Turn CUTOFF down (or pick another type) to use it."
                                                  : juce::String());
            repaint();
        }
    }

public:
    bool isPassThrough() const { return passThrough; }
    static constexpr float openAlpha = 0.55f;

private:

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix, title;
    juce::Colour colour;
    FilterTypePicker picker;
    SlopeSwitch slope;
    KnobControl cutoff, reso, drive, env, key, fm, morph;
    int type = -1;
    bool replaced = false, passThrough = false;
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
          // (Its knobs in WEST's lime, not the accent: I8-22.)
          position (p.apvts, "west_pos", "PLACE"),
          mode (p.apvts, "west_mode", "GATE"),
          source (p.apvts, "west_src", "STRIKE BY"),
          fold (p.apvts, "west_fold", "FOLD", colour(), false),
          symmetry (p.apvts, "west_sym", "SYMMETRY", colour(), false),
          stages (p.apvts, "west_stages", "STAGES", colour(), false),
          decay (p.apvts, "west_decay", "DECAY", colour(), false),
          resonance (p.apvts, "west_res", "RESO", colour(), false),
          strike (p.apvts, "west_strike", "STRIKE", colour(), false),
          open (p.apvts, "west_open", "OPEN", colour(), false)
    {
        addAll (*this, on, position, mode, source, fold, symmetry, stages, decay, resonance, strike, open);

        // "Replace Filter 2" squeezed the menu's font (I7-43); the saved
        // choice string stays.
        {
            auto& box = position.getComboBox();
            const auto selected = box.getSelectedId();
            box.changeItemText (2, "Replaces F2");
            box.setSelectedId (selected, juce::dontSendNotification);
        }
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

        if (folded())
            return;

        // The fold's transfer curve and the gate's vactrol, lit by its level
        // (at the off alpha, as the controls, while WEST is off).
        const auto plot = picture.toFloat();
        g.beginTransparencyLayer (read ("west_on") > 0.5f ? 1.0f : FilterColours::offAlpha);
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
        g.endTransparencyLayer();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        area.removeFromTop (30);
        area.removeFromBottom (4);
        // Its on switch in the header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), 14));

        // Off, the page gives it a header's height: only the switch stays
        // (UI review 9, V9-4).
        for (juce::Component* c : { (juce::Component*) &position, (juce::Component*) &mode, (juce::Component*) &source,
                                     (juce::Component*) &fold, (juce::Component*) &symmetry, (juce::Component*) &stages,
                                     (juce::Component*) &decay, (juce::Component*) &resonance, (juce::Component*) &strike,
                                     (juce::Component*) &open })
            c->setVisible (! folded());
        if (folded())
            return;

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
    bool folded() const { return getHeight() < 80; }

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
            const auto alpha = active ? 1.0f : FilterColours::offAlpha;
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

// M8.5: the VECTOR page: the vector pad (four oscillators at the corners,
// moved by hand, by a path or by its wander). EVOLVE (a macro drifting
// within a range) moved onto the macro's own card in the bottom strip, and
// VECTOR X / Y are dragged from the source bar, their one home (review 8:
// I8-13, I8-14, S8-20).
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
          driftRate (p.apvts, "vec_drift_rate", "WANDER RATE", colour(), true)
    {
        addAll (*this, pad, on, path, cornerA, cornerB, cornerC, cornerD, x, y, rate, drift, driftRate);
        path.showAsSwitch();

        // The corners name the oscillators as the pad does ("OSC 1").
        for (auto* corner : { &cornerA, &cornerB, &cornerC, &cornerD })
        {
            auto& box = corner->getComboBox();
            // (Read first: it only matches while the shown text is the item's.)
            const auto selectedId = box.getSelectedId();
            for (int item = 0; item < box.getNumItems(); ++item)
                box.changeItemText (box.getItemId (item), box.getItemText (item).toUpperCase());
            box.setSelectedId (0, juce::dontSendNotification);
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }

        // Off, the whole block dims and says why, as every card's controls
        // do (one rule, EffectRules: UI review 7, V7-21).
        for (juce::Component* control : { (juce::Component*) &path, (juce::Component*) &cornerA, (juce::Component*) &cornerB,
                                          (juce::Component*) &cornerC, (juce::Component*) &cornerD, (juce::Component*) &x,
                                          (juce::Component*) &y, (juce::Component*) &drift, (juce::Component*) &driftRate })
            effectRules.add (*control, effectRules.isOn ("vec_on"), "VECTOR is off", [] { return FilterColours::offAlpha; });
        // At the module-off alpha while VECTOR is off, the lighter dim of one
        // idle control while only its PATH is (V7-34 with V7-21).
        effectRules.add (rate, [this] { return readParam ("vec_on") > 0.5f && readParam ("vec_path") > 0.5f; },
                         "VECTOR or its PATH is off",
                         [this] { return readParam ("vec_on") > 0.5f ? IlanaTheme::dimmedAlpha : FilterColours::offAlpha; });

        startTimerHz (20);
    }

    // Not modulation sources, so not in a source's colour: the accent.
    static juce::Colour colour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintCard (g, vectorCard.toFloat(), 7.0f, colour().withAlpha (0.35f));

        // The switch sits right after the title, where the eye is (V11-24: the
        // card is 1,500 px wide), the caption after it.
        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR", {}, colour());
        {
            auto caption = header.withTrimmedLeft (on.getRight() - header.getX() + 8);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText (readParam ("vec_on") > 0.5f ? "four oscillators at the corners; drag VECTOR X or Y from the source bar onto a knob"
                                                    : "four oscillators at the corners",
                        caption, juce::Justification::centredLeft, true);
        }

        // The controls in three boxes, as SEQ's GENERATE has them (UI review
        // 9, V9-7): where the four oscillators sit, where the point is and
        // how it moves.
        const struct { juce::Rectangle<int> box; const char* title; } boxes[] {
            { cornersBox, "CORNERS" }, { positionBox, "POSITION" }, { motionBox, "MOTION" } };
        for (const auto& part : boxes)
        {
            IlanaTheme::paintRecessedPanel (g, part.box.toFloat(), 5.0f);
            paintSubBoxTitle (g, part.box.reduced (10, 0).withHeight (boxHeaderHeight), part.title, {}, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        // The pad gets most of the page, square at the card's full height
        // (no band above it: V7-21); the controls take the width left.
        vectorCard = area;

        auto inner = vectorCard.reduced (12, 0);
        inner.removeFromTop (30);
        inner.removeFromBottom (12);
        // The pad fills the card's height; the controls take the width left.
        // The pad starts right under the header and the controls column is
        // as tall as the pad: the menus at its top, the knob rows at its
        // foot (UI review 8, V8-26, S8-38: no band above the pad, no empty
        // foot under the controls).
        // (The pad is not kept square: the page's spare width is the pad's,
        // not the boxes': V10-14.)
        const auto controlsWidth = 330;
        const auto padWidth = juce::jmax (200, inner.getWidth() - controlsWidth - 12);
        pad.setBounds (inner.removeFromLeft (padWidth));
        inner.removeFromLeft (12);
        // The vector's on switch in its header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14).withX (vectorCard.getX() + 12 + IlanaTheme::cardTitleWidth ("VECTOR") + 4));
        // Three boxes in the controls column, each as tall as what it holds
        // (12 px of padding, controls in a left-aligned row of fixed cells):
        // CORNERS (the four menus, two by two), POSITION (X, Y) and MOTION
        // (PATH and its rate, WANDER and its rate).
        constexpr int gap = 8, padding = 12, cellWidth = 92;
        const auto spare = juce::jmax (0, inner.getHeight() - 2 * gap - (boxHeaderHeight + 2 * 44 + 8) - 2 * (boxHeaderHeight + 82));
        const auto extra = juce::jmin (24, spare / 3);
        cornersBox = inner.removeFromTop (boxHeaderHeight + 2 * 44 + 8 + extra);
        inner.removeFromTop (gap);
        positionBox = inner.removeFromTop (boxHeaderHeight + 82 + extra);
        inner.removeFromTop (gap);
        motionBox = inner.removeFromTop (boxHeaderHeight + 82 + extra);

        auto corners = cornersBox.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 4).withHeight (2 * 44);
        auto combos1 = corners.removeFromTop (44);
        cornerA.setBounds (combos1.removeFromLeft (combos1.getWidth() / 2).reduced (3, 1));
        cornerB.setBounds (combos1.reduced (3, 1));
        auto combos2 = corners;
        cornerC.setBounds (combos2.removeFromLeft (combos2.getWidth() / 2).reduced (3, 1));
        cornerD.setBounds (combos2.reduced (3, 1));

        const auto packed = [&] (juce::Rectangle<int> box, int count)
        {
            auto row = box.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 2).withHeight (82);
            return row.withWidth (juce::jmin (row.getWidth(), count * cellWidth));
        };
        layoutRow (packed (positionBox, 2), { &x, &y });
        layoutRow (packed (motionBox, 4), { &path, &rate, &drift, &driftRate });
    }

private:
    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    void timerCallback() override
    {
        // A corner menu names an oscillator the patch hasn't added as such
        // (the pad's caption says "none"; UI review 9, I9-17).
        auto shown = 0;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            shown |= processorRef.isOscillatorShown (osc) ? 1 << osc : 0;

        if (shown != shownOscillators)
        {
            shownOscillators = shown;
            for (auto* corner : { &cornerA, &cornerB, &cornerC, &cornerD })
            {
                auto& box = corner->getComboBox();
                const auto selected = box.getSelectedId();
                for (int osc = 0; osc < OscillatorIds::count; ++osc)
                {
                    box.changeItemText (osc + 1, "OSC " + juce::String (osc + 1) + ((shown >> osc) & 1 ? "" : " (not added)"));
                    // A corner can't sound an oscillator that isn't there: the
                    // choice is greyed unless it is already the corner's (V11-25).
                    box.setItemEnabled (osc + 1, ((shown >> osc) & 1) != 0 || osc + 1 == selected);
                }
                box.setSelectedId (selected, juce::dontSendNotification);
            }
        }

        const auto active = readParam ("vec_on") > 0.5f;
        if (const auto alpha = active ? 1.0f : FilterColours::offAlpha; pad.getAlpha() != alpha)
            pad.setAlpha (alpha);
        effectRules.apply();

        if (shownActive != active)
        {
            shownActive = active;
            repaint (vectorCard);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    VectorPadDisplay pad;
    ToggleControl on, path;
    ComboControl cornerA, cornerB, cornerC, cornerD;
    KnobControl x, y, rate, drift, driftRate;
    juce::Rectangle<int> vectorCard, cornersBox, positionBox, motionBox;
    static constexpr int boxHeaderHeight = 24;
    bool shownActive = false;
    int shownOscillators = -1;
};

// M8.7: the PHYSICAL page. The big view of one physical oscillator (the
// first in Physical mode unless another is picked): its string moving
// across the page, an electric piano's pickup, and under it its string and
// exciter controls in a compact band, built from the same list as its OSC
// card (UI review 6, S13, I6-18; review 7, V7-33, V7-29, V7-32). The body
// and the soundboard keep their full editors on FILTER and OSC > ACOUSTIC
// KEYS; here each has its switch and main controls, with a link to the
// rest (S14, I6-17; review 8, V8-23).
class PhysicalPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit PhysicalPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p),
          bodyOn (p.apvts, "res_on", "ON"),
          boardOn (p.apvts, "sb_on", "ON"),
          bodyType (p.apvts, "body_type", "TYPE"),
          boardModel (p.apvts, "sb_model", "MODEL"),
          bodyAmount (p.apvts, "res_amount", "AMOUNT", IlanaTheme::accent(), true),
          bodyDecay (p.apvts, "res_decay", "DECAY", IlanaTheme::accent(), true),
          boardMix (p.apvts, "sb_mix", "MIX", IlanaTheme::accent(), true)
    {
        addAndMakeVisible (view);
        // The one oscillator picker (UI review 8, I8-10).
        oscPicker.onPick = [this] (int i) { choose (i, true); };
        addAndMakeVisible (oscPicker);
        makePhysical.setButtonText ("SWITCH TO PHYSICAL");
        makePhysical.setTooltip ("Puts this oscillator in Physical mode.");
        // The theme's outlined button (a tinted pill), not a flat default one.
        IlanaTheme::makePill (makePhysical, IlanaTheme::accent());
        makePhysical.setToggleState (true, juce::dontSendNotification);
        makePhysical.onClick = [this]
        {
            if (auto* parameter = processorRef.apvts.getParameter (prefix() + "_mode"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
        };
        addChildComponent (makePhysical);

        styleJumpLink (bodyLink, "BODY");
        bodyLink.setTooltip ("All of the body's controls are on the FILTER page");
        bodyLink.onClick = [this]
        {
            if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                editor->showPage ("FILTER");
        };
        styleJumpLink (boardLink, "SOUNDBOARD");
        boardLink.setTooltip ("All of the soundboard's controls are on OSC, under SOUNDBOARD");
        boardLink.onClick = [this] { showAcousticKeys(); };
        for (auto* button : { &bodyLink, &boardLink })
            addAndMakeVisible (*button);

        for (auto* knob : { &bodyAmount, &bodyDecay, &boardMix })
            knob->setSizeRole (IlanaTheme::KnobSize::small);
        addAll (*this, bodyOn, boardOn, bodyType, boardModel, bodyAmount, bodyDecay, boardMix);
        // Off, a module's controls dim, as on its own card.
        for (auto* control : { (juce::Component*) &bodyType, (juce::Component*) &bodyAmount, (juce::Component*) &bodyDecay })
            effectRules.add (*control, effectRules.isOn ("res_on"), "BODY is off", [] { return FilterColours::offAlpha; });
        for (auto* control : { (juce::Component*) &boardModel, (juce::Component*) &boardMix })
            effectRules.add (*control, effectRules.isOn ("sb_on"), "the SOUNDBOARD is off", [] { return FilterColours::offAlpha; });

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
            auto message = makePhysical.getBounds().withHeight (44).translated (0, -60).withWidth (emptyCard.getWidth() - 28)
                                                  .withX (emptyCard.getX() + 14);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            // One sentence, once (the preview above it carries no label of its own: I10-12).
            IlanaTheme::drawFitted (g, "OSC " + juce::String (chosen + 1) + " plays " + plays[mode] + ". Switch it to Physical to hear this string.",
                                    message, juce::Justification::centred, 2);
            return;
        }

        IlanaTheme::paintCard (g, viewCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, stringCard.toFloat(), 7.0f, colour().withAlpha (0.25f));
        title (viewCard, "PHYSICAL", "OSC " + juce::String (chosen + 1) + "'s string, moving as you play", colour());

        for (const auto& [area, name] : rowLabels)
        {
            g.setColour (colour().withAlpha (0.8f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (name, area, juce::Justification::centredLeft);
        }

        // The body and the soundboard under the controls: each named, with
        // its switch and main controls after the name.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (bodyLine.getX(), bodyLine.getY() - 4, stringCard.getRight() - 12 - bodyLine.getX(), 1);
        for (const auto& [line, name, isOn] : { std::tuple<juce::Rectangle<int>, const char*, bool> { bodyLine, "BODY", readParam ("res_on") > 0.5f },
                                                { boardLine, "SOUNDBOARD", readParam ("sb_on") > 0.5f } })
            paintSubBoxTitle (g, line.withHeight (18), name, {}, isOn); // (the one box title: I10-7)
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        const auto physical = isPhysical (chosen);
        rowLabels.clear();

        for (juce::Component* c : { (juce::Component*) &bodyLink, (juce::Component*) &boardLink, (juce::Component*) &bodyOn,
                                    (juce::Component*) &boardOn, (juce::Component*) &bodyType, (juce::Component*) &boardModel,
                                    (juce::Component*) &bodyAmount, (juce::Component*) &bodyDecay, (juce::Component*) &boardMix })
            c->setVisible (physical);

        // Not physical: the view still shows, in its own preview look and
        // dimmed, what the switch would give (drawn from the oscillator's
        // string settings), on a card at the page's own margins with the
        // picker where the physical card has it (UI review 8, V8-24, V8-6).
        view.setInterceptsMouseClicks (physical, physical);
        view.setAlpha (physical ? 1.0f : 0.4f);

        if (! physical)
        {
            // Card height only (V11-15): the sentence, the button and a small
            // preview, not a page-sized dim picture.
            emptyCard = area.withHeight (juce::jmin (area.getHeight(), 360));
            auto inner = emptyCard.reduced (10, 0);
            inner.removeFromTop (30);
            const auto pickerWidth = juce::jmin (inner.getWidth(), oscPicker.getIdealWidth());
            oscPicker.setBounds (inner.removeFromTop (30).withSizeKeepingCentre (pickerWidth, 26));
            makePhysical.setBounds (juce::Rectangle<int> (240, 34).withCentre ({ emptyCard.getCentreX(), emptyCard.getBottom() - 36 }));
            inner.removeFromTop (6);
            inner.removeFromBottom (124); // the message and the switch
            view.setBounds (inner);
            return;
        }

        emptyCard = {};

        // The controls in a band under the view: STRING, then EXCITER on
        // the same line when it fits, each group named over its first
        // control; the body line at the foot.
        const auto columns = juce::jlimit (8, 16, (area.getWidth() - 24) / 76);
        // A menu takes two columns, so its text has room (V10-6: "PianoHammer"
        // was squeezed into one).
        const auto unitsOf = [] (juce::Component* item) { return dynamic_cast<ComboControl*> (item) != nullptr ? 2 : 1; };
        std::vector<std::vector<std::pair<juce::String, std::vector<juce::Component*>>>> lines;
        auto used = columns;
        for (const auto& row : layoutRows)
        {
            for (size_t first = 0; first < row.second.size();)
            {
                // As many of the row's controls as fit the columns.
                size_t count = 0;
                auto units = 0;
                while (first + count < row.second.size() && units + unitsOf (row.second[first + count]) <= columns)
                    units += unitsOf (row.second[first + count++]);
                count = juce::jmax ((size_t) 1, count);
                units = juce::jmax (units, 1);
                if (used + (used > 0 ? 1 : 0) + units > columns)
                {
                    lines.emplace_back();
                    used = 0;
                }
                used += (used > 0 ? 1 : 0) + units;
                lines.back().push_back ({ first == 0 ? row.first : juce::String(),
                                          { row.second.begin() + (long) first, row.second.begin() + (long) (first + count) } });
                first += count;
            }
        }
        const auto lineHeight = 18 + 84;
        // BODY and SOUNDBOARD fold to their name and switch while both are
        // off, as WEST and BODY do on FILTER (V10-9).
        const auto bodyOpen = readParam ("res_on") > 0.5f, boardOpen = readParam ("sb_on") > 0.5f;
        const auto bodyBlockHeight = bodyOpen || boardOpen ? bodyLineHeight : foldedBodyLineHeight;
        stringCard = area.removeFromBottom (12 + (int) lines.size() * lineHeight + 8 + bodyBlockHeight + 8);
        area.removeFromBottom (10);
        viewCard = area;

        {
            auto band = stringCard.reduced (12, 0);
            band.removeFromTop (12);
            const auto cell = band.getWidth() / columns;

            for (const auto& line : lines)
            {
                auto strip = band.removeFromTop (lineHeight);
                auto labels = strip.removeFromTop (18);

                for (size_t group = 0; group < line.size(); ++group)
                {
                    if (group > 0)
                    {
                        strip.removeFromLeft (cell);
                        labels.removeFromLeft (cell);
                    }
                    const auto& [name, items] = line[group];
                    auto units = 0;
                    for (auto* item : items)
                        units += unitsOf (item);
                    rowLabels.push_back ({ labels.withWidth (cell * units).reduced (4, 0), name });
                    labels.removeFromLeft (cell * units);

                    for (auto* item : items)
                        item->setBounds (strip.removeFromLeft (cell * unitsOf (item)).reduced (2, 2));
                }
            }

            band.removeFromTop (8);
            auto body = band.removeFromTop (bodyBlockHeight);
            bodyLine = body.removeFromLeft (body.getWidth() / 2);
            boardLine = body.withTrimmedLeft (16);
            // One header for every box (review 11, I11-6, I11-14): the name at
            // the left, its link and then its switch at the right, the main
            // controls on the line under it.
            const auto group = [] (juce::Rectangle<int> line, ToggleControl& power, ComboControl& menu,
                                   std::initializer_list<KnobControl*> knobs, juce::TextButton& link, int linkWidth, bool open)
            {
                auto header = line.removeFromTop (24);
                // (The bare switch keeps a 13 px label band over its pill.)
                power.setBounds (header.getRight() - 40, header.getY() - 13 - 1, 40, 13 + 20);
                menu.setVisible (open);
                link.setVisible (open);
                for (auto* knob : knobs)
                    knob->setVisible (open);
                if (! open)
                    return;
                link.setBounds (juce::Rectangle<int> (header.getRight() - 40 - 8 - linkWidth, header.getY(), linkWidth, 22));
                menu.setBounds (line.removeFromLeft (124).withSizeKeepingCentre (124, 44).translated (0, 4));
                line.removeFromLeft (8);
                for (auto* knob : knobs)
                    knob->setBounds (line.removeFromLeft (76));
            };
            group (bodyLine, bodyOn, bodyType, { &bodyAmount, &bodyDecay }, bodyLink, 120, bodyOpen);
            group (boardLine, boardOn, boardModel, { &boardMix }, boardLink, 176, boardOpen);
        }

        auto inner = viewCard.reduced (10, 0);
        inner.removeFromTop (30);
        auto picker = inner.removeFromTop (30);
        oscPicker.setBounds (picker.withWidth (juce::jmin (picker.getWidth(), oscPicker.getIdealWidth())).withSizeKeepingCentre (
            juce::jmin (picker.getWidth(), oscPicker.getIdealWidth()), 26));
        inner.removeFromTop (6);
        inner.removeFromBottom (10);

        // A tine or reed: its pickup under the string (S13).
        if (pickup != nullptr && pickup->isVisible())
        {
            pickup->setBounds (inner.removeFromBottom (inner.getHeight() * 32 / 100));
            inner.removeFromBottom (8);
        }

        view.setBounds (inner);
    }

    int getChosenOscillator() const { return chosen; }

    // Opens on an oscillator's string (OSC's EDIT STRING ›, UI review 9, I9-3).
    void showOscillator (int osc) { choose (osc, true); }

    // The controls shown for the chosen oscillator's string, in order (the
    // UI test compares them with the OSC card's).
    juce::StringArray getControlIds() const { return controlIds; }

private:
    static constexpr int bodyLineHeight = 96, foldedBodyLineHeight = 30;

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
                page->selectShared (OscPage::sharedKeys);

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
            groupExciteMenu (*excite);
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

        oscPicker.setSelectedOsc (chosen);
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
        std::vector<int> shownOscs;
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (processorRef.isOscillatorShown (i))
                shownOscs.push_back (i);
        // Lit while Physical; the others greyed, to switch over.
        oscPicker.setOscillators (shownOscs, [this] (int i) { return isPhysical (i); },
                                  [this] (int i) { return isPhysical (i) ? "Edit OSC " + juce::String (i + 1) + "'s string"
                                                                         : "OSC " + juce::String (i + 1) + " is not Physical"; });
        oscPicker.setSelectedOsc (chosen);

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
        effectRules.apply();

        if (const auto body = readParam ("res_on") + 2.0f * readParam ("sb_on") + 4.0f * readParam ("body_type") + 64.0f * readParam ("sb_model");
            body != shownBody)
        {
            shownBody = body;
            resized(); // (a block folds while off)
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    PhysicalView view;
    OscPicker oscPicker;
    juce::TextButton makePhysical, bodyLink, boardLink;
    ToggleControl bodyOn, boardOn;
    ComboControl bodyType, boardModel;
    KnobControl bodyAmount, bodyDecay, boardMix;
    EffectRules effectRules { processorRef };
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
    juce::Rectangle<int> emptyCard, viewCard, stringCard, bodyLine, boardLine;
};
// OSC's EDIT STRING ›: the PHYSICAL page, on that oscillator.
void showPhysicalString (juce::Component& from, int osc)
{
    auto* editor = from.findParentComponentOfClass<IlanaSynthAudioProcessorEditor>();
    auto* section = from.findParentComponentOfClass<SectionPage>();
    if (editor == nullptr || section == nullptr)
        return;

    if (auto* page = dynamic_cast<PhysicalPage*> (section->getPage (section->indexOf ("PHYSICAL"))))
        page->showOscillator (osc);
    editor->showPage ("PHYSICAL");
}

} // namespace
