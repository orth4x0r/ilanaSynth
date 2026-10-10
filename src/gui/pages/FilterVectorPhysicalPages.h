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
        for (auto* knob : { &cutoff, &reso, &drive, &env, &key, &fm, &morph })
            knob->setSizeRole (38);

        // Filter 2 stays open (20 kHz) on almost every patch: its switch says
        // so and brings it in or sends it back to open, as WEST and BODY
        // have theirs in the header (V14-12). No parameter: the switch is
        // read from the response (is it passing everything), and OFF parks
        // the cutoff at the top, remembering where it was.
        if (prefix == "f2")
        {
            f2Switch.setClickingTogglesState (false);
            f2Switch.getProperties().set ("switch", true);
            f2Switch.setTooltip ("Filter 2 on or off. Off is wide open (CUTOFF at the top), so it passes everything; ON brings its CUTOFF back.");
            f2Switch.onClick = [this] { toggleOpen(); };
            addAndMakeVisible (f2Switch);
        }
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
        // The design's matched card: a 30 px header (title, F2's switch), then
        // the type menu with the slope pills, then the six knobs.
        auto header = getLocalBounds().reduced (10, 0).removeFromTop (headerHeight);
        if (prefix == "f2")
        {
            // (cardSwitchBounds is for a ToggleControl with a label's 13 px above the pill.)
            f2Switch.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), headerHeight / 2).withTrimmedTop (13).withHeight (20));
        }
        auto area = getLocalBounds().withTrimmedTop (headerHeight).reduced (10, 8);
        auto row = area.removeFromTop (26);
        picker.setBounds (row.removeFromLeft (juce::jmin (FilterTypePicker::idealWidth, row.getWidth() - 110)));
        row.removeFromLeft (8);
        slope.setBounds (row.removeFromLeft (104).withSizeKeepingCentre (104, 24));
        area.removeFromTop (6);

        std::vector<juce::Component*> knobs { &cutoff, &reso, &drive, &env, &key, &fm };

        if (morph.isVisible())
            knobs.push_back (&morph);

        layoutRow (area, knobs);
    }

    static constexpr int headerHeight = 30;

    // The height the card wants at a width: the header and one knob row.
    int preferredHeight (int width) const
    {
        juce::ignoreUnused (width);
        return 150;
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
                                          : passThrough && child != &cutoff && child != &picker && child != &f2Switch ? openAlpha : 1.0f);
            setTooltip (passThrough && ! replaced ? title.substring (0, 1) + title.substring (1).toLowerCase()
                                                        + " is open: it passes everything. Turn CUTOFF down (or pick another type) to use it."
                                                  : juce::String());
            repaint();
        }

        if (prefix == "f2")
        {
            const auto onNow = ! passThrough || replaced;
            // (It slides as every switch does: the look-and-feel's shared
            // animator eases it, as no "switchAmount" is set here.)
            if (onNow != f2Switch.getToggleState())
            {
                f2Switch.setToggleState (onNow, juce::dontSendNotification);
                f2Switch.repaint();
            }
        }
    }

    // Filter 2's switch: open it down to its remembered CUTOFF, or park the
    // CUTOFF at the top (the part of its sound that was a closed filter is
    // remembered for this session).
    void toggleOpen()
    {
        if (auto* parameter = processorRef.apvts.getParameter (prefix + "_cutoff"))
        {
            const auto current = parameter->convertFrom0to1 (parameter->getValue());
            const auto target = passThrough ? juce::jmin (rememberedCutoff, 19000.0f) : 20000.0f;
            if (! passThrough)
                rememberedCutoff = current;
            processorRef.performEdit (parameter->getName (64), [parameter, target]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (target));
                parameter->endChangeGesture();
            });
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
    juce::TextButton f2Switch;
    float rememberedCutoff = 8000.0f;
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
        auto header = getLocalBounds().reduced (12, 0).removeFromTop (30);
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
        on.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), 15));

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
        auto top = area.removeFromTop (40);
        picture = top.removeFromRight (top.getWidth() * 2 / 5).reduced (4, 1);
        const auto menuWidth = top.getWidth() / 3;
        for (auto* menu : { &position, &mode, &source })
            menu->setBounds (top.removeFromLeft (menuWidth).reduced (3, 1));
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
        addAll (*this, pad, cornerWaves[0], cornerWaves[1], cornerWaves[2], cornerWaves[3], on, path, cornerA, cornerB, cornerC, cornerD, x, y, rate, drift, driftRate);
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

        // The switch is at the header's right like every card's own module
        // (UI-CONVENTIONS; V13-10), the caption after the title.
        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR", readParam ("vec_on") > 0.5f ? "four oscillators at the corners; drag VECTOR X or Y from the source bar onto a knob"
                                                                                       : "four oscillators at the corners", colour());

        // The controls in three boxes, as SEQ's GENERATE has them (UI review
        // 9, V9-7): where the four oscillators sit, where the point is and
        // how it moves.
        const struct { juce::Rectangle<int> box; const char* title; } boxes[] {
            { cornersBox, "CORNERS" }, { motionBox, "POSITION AND MOTION" } };
        for (const auto& part : boxes)
        {
            IlanaTheme::paintRecessedPanel (g, part.box.toFloat(), 5.0f);
            paintSubBoxTitle (g, part.box.reduced (10, 0).withHeight (boxHeaderHeight), part.title, {}, false, 0, colour());
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
        // (The pad is a square: it takes the card's height and the controls
        // the rest of the width, so no bare strip stands beside it, V12-3.)
        const auto padWidth = juce::jlimit (200, juce::jmax (200, inner.getWidth() - 330 - 12), inner.getHeight());
        pad.setBounds (inner.removeFromLeft (padWidth));
        inner.removeFromLeft (12);
        // The vector's on switch in its header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14));
        // Two boxes fill the controls column's height (no empty foot): CORNERS
        // (the four menus, two by two, each over a picture of its oscillator,
        // V14-2) over POSITION AND MOTION in one row (X, Y, PATH, its rate,
        // WANDER and its rate).
        constexpr int gap = 8, padding = 12;
        const auto motionHeight = boxHeaderHeight + 2 + 100 + 12;
        cornersBox = inner.removeFromTop (juce::jmax (boxHeaderHeight + 2 * 90, inner.getHeight() - gap - motionHeight));
        inner.removeFromTop (gap);
        motionBox = inner;

        auto corners = cornersBox.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 2).withTrimmedBottom (8);
        const auto rowHeight = corners.getHeight() / 2;
        auto row1 = corners.removeFromTop (rowHeight);
        auto row2 = corners;
        const auto cell = [this] (juce::Rectangle<int> area, ComboControl& menu, VectorCornerWave& wave)
        {
            area = area.reduced (3, 2);
            menu.setBounds (area.removeFromTop (46));
            area.removeFromTop (2);
            wave.setBounds (area);
        };
        cell (row1.removeFromLeft (row1.getWidth() / 2), cornerA, cornerWaves[0]);
        cell (row1, cornerB, cornerWaves[1]);
        cell (row2.removeFromLeft (row2.getWidth() / 2), cornerC, cornerWaves[2]);
        cell (row2, cornerD, cornerWaves[3]);

        for (auto* knob : { &x, &y, &rate, &drift, &driftRate })
            knob->setSizeRole (juce::jmin (64, juce::jmax (IlanaTheme::KnobSize::main, (motionBox.getHeight() - boxHeaderHeight) - 40)));
        auto knobs = motionBox.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 2).withTrimmedBottom (6);
        layoutRow (knobs, { &x, &y, &path, &rate, &drift, &driftRate });
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
        {
            shown |= processorRef.isOscillatorShown (osc) ? 1 << osc : 0;
            // (A switched-off oscillator is a different menu text: bit 8 up.)
            shown |= processorRef.isOscillatorShown (osc) && readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on") < 0.5f ? 1 << (osc + 8) : 0;
        }

        if (shown != shownOscillators)
        {
            shownOscillators = shown;
            for (auto* corner : { &cornerA, &cornerB, &cornerC, &cornerD })
            {
                auto& box = corner->getComboBox();
                const auto selected = box.getSelectedId();
                for (int osc = 0; osc < OscillatorIds::count; ++osc)
                {
                    // Worded as the pad's corners and every label: "OSC 4: none", "OSC 3: off" (V12-8).
                    box.changeItemText (osc + 1, "OSC " + juce::String (osc + 1) + ((shown >> osc) & 1 ? ((shown >> (osc + 8)) & 1 ? ": off" : "") : ": none"));
                    // A corner can't sound an oscillator that isn't there: the
                    // choice is greyed unless it is already the corner's (V11-25).
                    box.setItemEnabled (osc + 1, ((shown >> osc) & 1) != 0 || osc + 1 == selected);
                }
                box.setSelectedId (selected, juce::dontSendNotification);
            }
        }

        for (auto* corner : { &cornerA, &cornerB, &cornerC, &cornerD })
        {
            const auto id = corner->getComboBox().getSelectedId();
            if (id >= 1 && id <= OscillatorIds::count)
                corner->setTint (IlanaTheme::oscColour (id - 1));
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
    std::array<VectorCornerWave, 4> cornerWaves { VectorCornerWave (processorRef, 0), VectorCornerWave (processorRef, 1),
                                                  VectorCornerWave (processorRef, 2), VectorCornerWave (processorRef, 3) };
    ToggleControl on, path;
    ComboControl cornerA, cornerB, cornerC, cornerD;
    KnobControl x, y, rate, drift, driftRate;
    juce::Rectangle<int> vectorCard, cornersBox, motionBox;
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
// The PHYSICAL page's two read-outs (the approved design): the string's
// PARTIALS (their levels from the strike point, DAMP and STIFF) and the BODY +
// BOARD RESPONSE (the body's and soundboard's resonances from their settings).
// Pictures of the settings, like the string view above them.
class PhysicalReadout : public juce::Component
{
public:
    enum class Kind { partials, response };

    PhysicalReadout (IlanaSynthAudioProcessor& p, Kind kindIn) : processorRef (p), kind (kindIn) {}

    void setOscillator (const juce::String& newPrefix, juce::Colour newColour) { prefix = newPrefix; colour = newColour; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text3);
        const auto title = kind == Kind::partials ? juce::String ("PARTIALS") : juce::String ("BODY + BOARD RESPONSE");
        const auto note = kind == Kind::partials ? juce::String ("16") : juce::String ("100 Hz - 8 kHz");
        const auto header = bounds.reduced (8.0f, 5.0f).withHeight (14.0f).toNearestInt();
        const auto noteWidth = juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::tiny, true), note) + 4;
        IlanaTheme::drawFitted (g, title, header.withTrimmedRight (noteWidth + 6), juce::Justification::centredLeft, 1);
        g.drawText (note, header, juce::Justification::centredRight);

        auto plot = bounds.reduced (10.0f, 8.0f).withTrimmedTop (16.0f);

        // A faint grid.
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());

        if (kind == Kind::partials)
        {
            const auto damp = read (prefix + "_string_damp");
            const auto stiff = read (prefix + "_string_stiffness");
            const auto pos = juce::jlimit (0.05f, 0.5f, read (prefix + "_string_excite_pos") > 0.001f ? read (prefix + "_string_excite_pos") : 0.25f);
            constexpr int count = 16;
            const auto slot = plot.getWidth() / (float) count;
            for (int n = 1; n <= count; ++n)
            {
                const auto comb = 0.3f + 0.7f * std::abs (std::sin (juce::MathConstants<float>::pi * (float) n * pos));
                const auto level = juce::jlimit (0.03f, 1.0f, comb / std::pow ((float) n, 0.55f + 1.4f * damp) * (1.0f + 0.4f * stiff * (float) (n % 3)));
                const auto bar = juce::Rectangle<float> (plot.getX() + slot * (float) (n - 1) + slot * 0.2f, plot.getBottom() - plot.getHeight() * level,
                                                         slot * 0.6f, plot.getHeight() * level);
                g.setColour (colour.withAlpha (0.45f + 0.55f * level));
                g.fillRoundedRectangle (bar, 1.0f);
            }
            return;
        }

        const auto bodyOn = read ("res_on") > 0.5f;
        const auto boardOn = read ("sb_on") > 0.5f;
        const auto bodyAmount = bodyOn ? 0.25f + 0.75f * read ("res_amount") : 0.12f;
        const auto boardMix = boardOn ? 0.25f + 0.75f * read ("sb_mix") : 0.0f;
        const auto bodySize = read ("body_size"), boardSize = read ("sb_size");
        const auto peak = [] (float x, float centre, float width) { return std::exp (-std::pow ((x - centre) / width, 2.0f)); };
        juce::Path curve;
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = (float) i / 120.0f;
            const auto y = 0.1f + bodyAmount * (0.5f * peak (x, 0.12f + 0.25f * (1.0f - bodySize), 0.05f) + 0.38f * peak (x, 0.32f + 0.2f * (1.0f - bodySize), 0.06f)
                                                + 0.3f * peak (x, 0.55f + 0.1f * (1.0f - bodySize), 0.08f))
                           + boardMix * (0.3f * peak (x, 0.2f + 0.3f * (1.0f - boardSize), 0.07f) + 0.2f * peak (x, 0.8f - 0.2f * boardSize, 0.08f));
            const auto point = juce::Point<float> (plot.getX() + x * plot.getWidth(), plot.getBottom() - juce::jmin (1.0f, y) * plot.getHeight());
            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }
        auto filled (curve);
        filled.lineTo (plot.getRight(), plot.getBottom());
        filled.lineTo (plot.getX(), plot.getBottom());
        filled.closeSubPath();
        g.setColour (colour.withAlpha (0.16f));
        g.fillPath (filled);
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

private:
    float read (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;
    Kind kind;
    juce::String prefix { "osc1" };
    juce::Colour colour { IlanaTheme::accent() };
};

class PhysicalPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit PhysicalPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p),
          partials (p, PhysicalReadout::Kind::partials),
          response (p, PhysicalReadout::Kind::response),
          bodyOn (p.apvts, "res_on", "ON"),
          boardOn (p.apvts, "sb_on", "ON"),
          bodyType (p.apvts, "body_type", "TYPE"),
          boardModel (p.apvts, "sb_model", "MODEL"),
          bodyAmount (p.apvts, "res_amount", "AMOUNT", IlanaTheme::accent(), true),
          bodyDecay (p.apvts, "res_decay", "DECAY", IlanaTheme::accent(), true),
          boardMix (p.apvts, "sb_mix", "MIX", IlanaTheme::accent(), true),
          bodyCouplingMode (p.apvts, "body_coupling_mode", "COUPLING"),
          bodyMaterial (p.apvts, "body_material", "MATERIAL", IlanaTheme::accent(), true),
          bodySize (p.apvts, "body_size", "SIZE", IlanaTheme::accent(), true),
          bodyOffset (p.apvts, "res_offset", "OFFSET", IlanaTheme::accent(), true),
          bodyKeytrack (p.apvts, "res_keytrack", "KEY TRK", IlanaTheme::accent(), true),
          bodyCoupling (p.apvts, "body_coupling", "COUPLE", IlanaTheme::accent(), true),
          boardTone (p.apvts, "sb_tone", "TONE", IlanaTheme::accent(), true),
          boardSize (p.apvts, "sb_size", "SIZE", IlanaTheme::accent(), true),
          boardStretch (p.apvts, "stretch", "STRETCH", IlanaTheme::accent(), true)
    {
        addAndMakeVisible (view);
        addAndMakeVisible (partials);
        addAndMakeVisible (response);
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
            processorRef.setOscillatorMode (chosen, OscMode::physical);
        };
        addChildComponent (makePhysical);

        styleJumpLink (bodyLink, "BODY");
        bodyLink.setButtonText (juce::String ("EDIT ") + juce::String::fromUTF8 ("\xe2\x80\xba"));
        bodyLink.setTooltip ("The same controls are on the FILTER page");
        bodyLink.onClick = [this]
        {
            if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                editor->showPage ("FILTER");
        };
        styleJumpLink (boardLink, "SOUNDBOARD");
        boardLink.setButtonText (juce::String ("EDIT ") + juce::String::fromUTF8 ("\xe2\x80\xba"));
        boardLink.setTooltip ("All of the soundboard's controls are on OSC, under SOUNDBOARD");
        boardLink.onClick = [this] { showAcousticKeys(); };
        for (auto* button : { &bodyLink, &boardLink })
            addAndMakeVisible (*button);

        for (auto* knob : { &bodyAmount, &bodyDecay, &boardMix, &bodyMaterial, &bodySize, &bodyOffset, &bodyKeytrack, &bodyCoupling,
                            &boardTone, &boardSize, &boardStretch })
            knob->setSizeRole (IlanaTheme::KnobSize::compact);
        addAll (*this, bodyOn, boardOn, bodyType, boardModel, bodyAmount, bodyDecay, boardMix, bodyCouplingMode, bodyMaterial, bodySize,
                bodyOffset, bodyKeytrack, bodyCoupling, boardTone, boardSize, boardStretch);
        // Off, a module's controls dim, as on its own card (and stay drawn,
        // so the box is the same size on or off: V13-9).
        for (auto* control : { (juce::Component*) &bodyType, (juce::Component*) &bodyAmount, (juce::Component*) &bodyDecay,
                               (juce::Component*) &bodyCouplingMode, (juce::Component*) &bodyOffset, (juce::Component*) &bodyKeytrack })
            effectRules.add (*control, effectRules.isOn ("res_on"), "BODY is off", [] { return FilterColours::offAlpha; });
        // MATERIAL and SIZE shape the modal bodies only; COUPLE needs a
        // coupling mode (as on FILTER's BODY card).
        const auto modalOn = [this] { return readParam ("res_on") > 0.5f && readParam ("body_type") > 0.5f; };
        const auto couplingOn = [this]
        {
            const auto mode = juce::roundToInt (readParam ("body_coupling_mode"));
            return readParam ("res_on") > 0.5f && (mode == 3 || (mode != 0 && readParam ("body_type") > 0.5f));
        };
        effectRules.add (bodyMaterial, modalOn, "BODY is off or Classic", [] { return FilterColours::offAlpha; });
        effectRules.add (bodySize, modalOn, "BODY is off or Classic", [] { return FilterColours::offAlpha; });
        effectRules.add (bodyCoupling, couplingOn, "COUPLING is Off, or needs a modal BODY", [] { return FilterColours::offAlpha; });
        for (auto* control : { (juce::Component*) &boardModel, (juce::Component*) &boardMix, (juce::Component*) &boardTone,
                               (juce::Component*) &boardSize, (juce::Component*) &boardStretch })
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

        IlanaTheme::paintCard (g, viewCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        title (viewCard, "PHYSICAL", isPhysical (chosen) ? "OSC " + juce::String (chosen + 1) + "'s string, moving as you play"
                                                         : "OSC " + juce::String (chosen + 1) + "'s string as Physical would play it", colour());

        // Not a Physical oscillator: the page keeps its shape (the string, its
        // controls dimmed) and one line on the picker's row says why and
        // offers the switch, rather than a page of its own (V14-3).
        if (! isPhysical (chosen))
        {
            static const char* const plays[] { "a wavetable", "a string", "a sample", "grains", "the live input", "FM / DX7" };
            const auto mode = juce::jlimit (0, OscMode::count - 1, juce::roundToInt (readParam (prefix() + "_mode")));
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            // An FM operator is not offered a string: switching it would take it out of the FM voice (V12-24).
            IlanaTheme::drawFitted (g, isOperatorVoice (chosen)
                                           ? "OSC " + juce::String (chosen + 1) + " is an FM operator: pick another for a string"
                                           : "OSC " + juce::String (chosen + 1) + " plays " + plays[mode],
                                    messageArea, juce::Justification::centredLeft, 1);
        }

        for (const auto& [area, name] : rowLabels)
        {
            g.setColour (name == "BODY" || name == "SOUNDBOARD" ? IlanaTheme::accent() : colour());
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            IlanaTheme::drawFitted (g, name, area, juce::Justification::centredLeft, 1);
        }

        if (! hintArea.isEmpty() && physicalHint().isNotEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, physicalHint(), hintArea, juce::Justification::centredLeft, 3);
        }

        g.setColour (IlanaTheme::Ui::line.withAlpha (0.5f));
        for (const auto& line : separators)
            g.fillRect (line);
    }

    void resized() override
    {
        // The approved design: the picker row (28 px), then ONE card: the
        // string view on top, and four rows STRING, EXCITER, BODY and
        // SOUNDBOARD on one column grid (a 96 px gutter and nine columns),
        // PARTIALS beside the string rows and BODY + BOARD RESPONSE beside
        // the soundboard row.
        auto area = getLocalBounds().reduced (14, 12);
        const auto physical = isPhysical (chosen);
        rowLabels.clear();
        separators.clear();

        view.setInterceptsMouseClicks (physical, physical);
        view.setAlpha (physical ? 1.0f : 0.75f);
        messageArea = {};

        auto picker = area.removeFromTop (28);
        area.removeFromTop (10);
        viewCard = area;
        const auto pickerWidth = juce::jmin (picker.getWidth(), oscPicker.getIdealWidth());
        oscPicker.setBounds (picker.withWidth (pickerWidth));
        if (! physical)
        {
            auto rest = picker.withTrimmedLeft (pickerWidth + 16);
            makePhysical.setBounds (rest.removeFromRight (190));
            messageArea = rest.withTrimmedRight (12);
        }

        constexpr int rowHeight = 76, rowPad = 3, gutter = 96, columnGap = 6; // (rowPad: a row's labels clear the row above's values, A16-8)
        auto inner = viewCard.withTrimmedTop (30).reduced (10, 8);
        auto grid = inner.removeFromBottom (4 * rowHeight);
        inner.removeFromBottom (8);

        // A tine or reed: its pickup under the string (S13).
        if (pickup != nullptr && pickup->isVisible())
        {
            pickup->setBounds (inner.removeFromBottom (inner.getHeight() * 32 / 100));
            inner.removeFromBottom (8);
        }
        view.setBounds (inner);

        const auto columnWidth = (float) (grid.getWidth() - gutter - 9 * columnGap) / 9.0f;
        const auto columnX = [&] (int column) { return grid.getX() + gutter + columnGap + juce::roundToInt ((float) column * (columnWidth + (float) columnGap)); };
        const auto rowTop = [&] (int row) { return grid.getY() + row * rowHeight; };
        const auto rowY = [&] (int row) { return rowTop (row) + rowPad; };
        const auto cell = [&] (int column, int row, int span = 1)
        {
            return juce::Rectangle<int> (columnX (column), rowY (row), juce::roundToInt (columnWidth * (float) span + (float) (columnGap * (span - 1))), rowHeight - 2 * rowPad);
        };
        const auto place = [&] (juce::Component* item, int column, int row)
        {
            const auto box = cell (column, row);
            if (dynamic_cast<ComboControl*> (item) != nullptr)
                item->setBounds (box.withSizeKeepingCentre (box.getWidth(), 13 + 24));
            else
                item->setBounds (box);
        };
        const auto gutterName = [&] (const juce::String& name, int row)
        {
            rowLabels.push_back ({ juce::Rectangle<int> (grid.getX(), rowY (row) + 4, gutter, 16), name });
        };
        const auto separator = [&] (int row, int toColumn)
        {
            if (row > 0)
                separators.push_back (juce::Rectangle<int> (grid.getX(), rowTop (row), columnX (toColumn) - columnGap - grid.getX(), 1));
        };

        // STRING and EXCITER rows. The exciter's SLAP switch sits in its gutter.
        auto widest = 7;
        for (size_t line = 0; line < layoutRows.size() && line < 2; ++line)
        {
            const auto row = (int) line;
            gutterName (layoutRows[line].first, row);
            auto column = 0;
            for (auto* item : layoutRows[line].second)
            {
                if (dynamic_cast<ToggleControl*> (item) != nullptr)
                {
                    item->setBounds (grid.getX(), rowY (row) + 24, 70, 13 + 24);
                    continue;
                }
                place (item, column++, row);
            }
            widest = juce::jmax (widest, column);
        }
        // (Rows may be wider than the default seven: the readout takes what is left.)
        const auto stringColumns = 7;
        juce::ignoreUnused (stringColumns);
        int usedColumns = 0;
        for (size_t line = 0; line < layoutRows.size() && line < 2; ++line)
        {
            int count = 0;
            for (auto* item : layoutRows[line].second)
                count += dynamic_cast<ToggleControl*> (item) != nullptr ? 0 : 1;
            usedColumns = juce::jmax (usedColumns, count);
        }
        const auto partialsFrom = juce::jmax (usedColumns, layoutRows.size() > 1 && layoutRows[1].second.size() > 0 ? 3 : 0);
        partials.setVisible (partialsFrom <= 8);
        if (partialsFrom <= 8)
            partials.setBounds (cell (partialsFrom, 0, 9 - partialsFrom).withHeight (2 * rowHeight - 2 * rowPad));
        separator (1, partialsFrom <= 8 ? partialsFrom : 9);

        // The EXCITER row's empty slots (a hammer has three controls, not
        // seven) say what the exciter does instead of standing empty (A16-4).
        hintArea = {};
        if (layoutRows.size() > 1)
        {
            auto count = 0;
            for (auto* item : layoutRows[1].second)
                count += dynamic_cast<ToggleControl*> (item) != nullptr ? 0 : 1;
            const auto to = partialsFrom <= 8 ? partialsFrom : 9;
            if (to - count >= 2)
                hintArea = cell (count, 1, to - count).reduced (10, 14);
        }

        // BODY and SOUNDBOARD: name, switch and EDIT in the gutter.
        const auto gutterBlock = [&] (const juce::String& name, int row, ToggleControl& power, juce::TextButton& link)
        {
            gutterName (name, row);
            power.setBounds (grid.getX(), rowY (row) + 22, 40, 13 + 20);
            link.setBounds (grid.getX() + 46, rowY (row) + 35, 50, 22);
        };
        separator (2, 9);
        gutterBlock ("BODY", 2, bodyOn, bodyLink);
        const auto bodyItems = std::vector<juce::Component*> { &bodyType, &bodyCouplingMode, &bodyAmount, &bodyDecay, &bodyMaterial, &bodySize,
                                                                &bodyOffset, &bodyKeytrack, &bodyCoupling };
        for (size_t i = 0; i < bodyItems.size(); ++i)
            place (bodyItems[i], (int) i, 2);
        separator (3, 9);
        gutterBlock ("SOUNDBOARD", 3, boardOn, boardLink);
        const auto boardItems = std::vector<juce::Component*> { &boardModel, &boardMix, &boardTone, &boardSize, &boardStretch };
        for (size_t i = 0; i < boardItems.size(); ++i)
            place (boardItems[i], (int) i, 3);
        response.setBounds (cell (5, 3, 4));
        partials.setOscillator (prefix(), colour());
        response.setOscillator (prefix(), IlanaTheme::accent());
    }

    int getChosenOscillator() const { return chosen; }

    // Opens on an oscillator's string (OSC's EDIT STRING ›, UI review 9, I9-3).
    void showOscillator (int osc) { choose (osc, true); }

    // The controls shown for the chosen oscillator's string, in order (the
    // UI test compares them with the OSC card's).
    juce::StringArray getControlIds() const { return controlIds; }

private:
    static constexpr int bodyLineHeight = 108, boxHeaderHeight = 24, stringTitleHeight = 28, linkWidth = 150;

    juce::String prefix() const { return OscillatorIds::prefixes[(size_t) chosen]; }

    // One sentence on what the chosen exciter does, for the EXCITER row's free slots.
    juce::String physicalHint() const
    {
        static const char* const hints[] {
            "Burst: a short click of noise plucks the string. HARDNESS and PICK POS shape it.",
            "Noise: a longer noise burst rubs the string. HARDNESS and PICK POS shape it.",
            "Saw: a saw wave drives the string. HARDNESS and PICK POS shape it.",
            "Pulse: a pulse wave drives the string. HARDNESS and PICK POS shape it.",
            "Bow: a steady bow keeps the string sounding. BOW PRESS and BOW SPEED set the grip.",
            "Bright Hammer: a hard felt strike. HAMMER sets how bright the upper partials are.",
            "Osc In: another oscillator drives the string.",
            "Tine: a struck tine and its pickup. DISTANCE and OFFSET set the pickup.",
            "Reed: a struck reed and its pickup. DISTANCE and OFFSET set the pickup.",
            "Piano Hammer: a felt strike at EXCITE POS. HAMMER sets how bright the upper partials are.",
            "Feedback: the amp pushes sound back at the string. AMP GAIN and DISTANCE set the loop." };
        const auto excite = juce::jlimit (0, 10, juce::roundToInt (readParam (prefix() + "_excite")));
        return hints[excite];
    }

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
                    knob->setSizeRole (IlanaTheme::KnobSize::compact);
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

    // An oscillator that plays as an FM operator: the FM / DX7 type.
    bool isOperatorVoice (int osc) const
    {
        return juce::roundToInt (readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode")) == OscMode::fmOperator
               && FmOperatorInfo::isPlaying (processorRef, osc);
    }

    void updateAvailability()
    {
        const auto physical = isPhysical (chosen);
        makePhysical.setVisible (! physical && ! isOperatorVoice (chosen));
        // A non-physical oscillator's string controls stay, dimmed.
        const auto stringAlpha = physical ? 1.0f : FilterColours::offAlpha;
        if (excite != nullptr)
            excite->setAlpha (stringAlpha);
        for (auto& control : controls)
            control->setAlpha (stringAlpha);
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
        partials.repaint();
        response.repaint();

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
    PhysicalReadout partials, response;
    OscPicker oscPicker;
    juce::TextButton makePhysical, bodyLink, boardLink;
    ToggleControl bodyOn, boardOn;
    ComboControl bodyType, boardModel;
    KnobControl bodyAmount, bodyDecay, boardMix;
    ComboControl bodyCouplingMode;
    KnobControl bodyMaterial, bodySize, bodyOffset, bodyKeytrack, bodyCoupling, boardTone, boardSize, boardStretch;
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
    juce::Rectangle<int> messageArea, viewCard, hintArea;
    std::vector<juce::Rectangle<int>> separators;
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
