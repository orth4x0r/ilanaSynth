// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// An operator (DX7) voice with both filters wide open has none: PLAY's FILTER OFF
// note and the FILTER page's state say so (a macro sitting at 0 pulls nothing).
inline bool operatorVoiceFilterOff (const IlanaSynthAudioProcessor& p)
{
    auto operatorVoice = false;
    for (int osc = 0; osc < OscillatorIds::count && ! operatorVoice; ++osc)
        operatorVoice = p.isOscillatorShown (osc) && OscRole::isOperator (p, osc) && OscRole::usesOperatorEg (p, osc);
    return operatorVoice && FilterDisplay::isPassThrough (p, 0, true) && FilterDisplay::isPassThrough (p, 1, true);
}

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
        g.setColour (IlanaTheme::Ui::panel); // opaque: the dimmed labels under it must not show through
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
        const auto voiceNow = operatorVoiceFilterOff (processorRef);
        const auto openNow = FilterDisplay::isPassThrough (processorRef, prefix == "f1" ? 0 : 1, voiceNow);
        if (replacedNow != replaced || openNow != passThrough || voiceNow != voiceOff)
        {
            replaced = replacedNow;
            passThrough = openNow;
            voiceOff = voiceNow;
            // (A DX7 voice has no filter: the whole card steps back, CUTOFF and
            // the type menu a little less, since they bring one in. N16-2.)
            for (auto* child : getChildren())
                child->setAlpha (replaced ? IlanaTheme::dimmedAlpha * 0.6f
                                          : voiceOff ? (child == &cutoff || child == &picker || child == &f2Switch ? 0.8f : IlanaTheme::dimmedAlpha * 0.6f)
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
    bool replaced = false, passThrough = false, voiceOff = false;
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
        const auto westOn = read ("west_on") > 0.5f;
        IlanaTheme::paintCardHeader (g, header, "WEST", ! westOn ? juce::String (juce::CharPointer_UTF8 ("off \xc2\xb7 switch on to fold and gate"))
                                                       : juce::roundToInt (read ("west_pos")) == 1 ? juce::String ("wavefolder and low-pass gate, in Filter 2's place")
                                                                                                    : juce::String ("wavefolder and low-pass gate, after the filters"),
                                     colour(), 60);

        if (folded())
            return;

        // The fold's transfer curve and the gate's vactrol, lit by its level
        // (at the off alpha, as the controls, while WEST is off).
        const auto plot = picture.toFloat();
        g.beginTransparencyLayer (read ("west_on") > 0.5f ? 1.0f : FilterColours::cardOffAlpha);
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
            const auto alpha = active ? 1.0f : FilterColours::cardOffAlpha;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        // (The header says where WEST sits, so a change repaints it all.)
        if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
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

} // namespace
