#include "EnvLfoPages.h"

#include "PageHelpers.h"

LfoRateControl::LfoRateControl (IlanaSynthAudioProcessor& p, int lfoIndex, juce::Colour accent, bool followsTheme)
    : processorRef (p),
      lfo (lfoIndex),
      rate (p.apvts, "lfo" + juce::String (lfoIndex + 1) + "_rate", "RATE", accent, followsTheme),
      division (p.apvts, "lfo" + juce::String (lfoIndex + 1) + "_div", "RATE", accent, followsTheme)
{
    division.setModulationTarget ("lfo" + juce::String (lfoIndex + 1) + "_rate");
    const juce::String tip ("LFO " + juce::String (lfoIndex + 1) + " rate (synced)\nSYNC is on, so RATE is a note value at the "
                            "host tempo: drag or scroll to step through them. Turn SYNC off for Hz. Modulation of RATE "
                            "still speeds it up or slows it down: drop a source here, or use the dots beside the dial.");
    division.setTooltip (tip);
    division.getSlider().setTooltip (tip);
    startTimerHz (10);
}

void LfoRateControl::addTo (juce::Component& parent)
{
    parent.addChildComponent (rate);
    parent.addChildComponent (division);
    refresh();
}

void LfoRateControl::setShown (bool shouldShow)
{
    shown = shouldShow;
    refresh();
}

void LfoRateControl::setBounds (juce::Rectangle<int> bounds)
{
    rate.setBounds (bounds);
    matchBounds();
}

bool LfoRateControl::isSynced () const
{
    const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_sync");
    return sync != nullptr && sync->load() > 0.5f;
}

void LfoRateControl::refresh ()
{
    if (! shown && ! rate.isVisible() && ! division.isVisible())
        return;

    const auto synced = isSynced();

    if (rate.isVisible() != (shown && ! synced))
        rate.setVisible (shown && ! synced);

    if (division.isVisible() != (shown && synced))
        division.setVisible (shown && synced);
}

LfoRateSlider::LfoRateSlider (IlanaSynthAudioProcessor& p, int lfoIndex, juce::Colour accent)
    : processorRef (p),
      lfo (lfoIndex),
      rate (p.apvts, "lfo" + juce::String (lfoIndex + 1) + "_rate"),
      division (p.apvts, "lfo" + juce::String (lfoIndex + 1) + "_div"),
      destination (modRingConfigFor ("lfo" + juce::String (lfoIndex + 1) + "_rate").destination)
{
    label.setText ("RATE", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
    label.setBorderSize ({ 0, 1, 0, 1 });
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    for (auto* control : { &rate, &division })
    {
        control->getSlider().setColour (juce::Slider::rotarySliderFillColourId, accent);
        control->getSlider().setColour (juce::Slider::trackColourId, accent);
        addChildComponent (*control);
    }

    const juce::String tip ("LFO " + juce::String (lfoIndex + 1) + " rate\nHz while free-running; a note value at the host tempo while SYNC is on. "
                            "Drop a modulation source here to modulate the rate.");
    rate.getSlider().setTooltip (tip);
    division.getSlider().setTooltip (tip);
    refresh();
    startTimerHz (10);
}

void LfoRateSlider::resized ()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (13));
    // The slider line is the menus' box height, centred in it.
    rate.setBounds (area);
    division.setBounds (area);
}

bool LfoRateSlider::isInterestedInDragSource (const SourceDetails& details)
{
    return destination != 0 && details.description.toString().startsWith ("modsource:");
}

void LfoRateSlider::itemDropped (const SourceDetails& details)
{
    const auto source = details.description.toString().fromFirstOccurrenceOf ("modsource:", false, false).getIntValue();
    processorRef.performEdit ("Add modulation", [this, source] { processorRef.assignModSlot (source, destination, 0.35f); });
}

bool LfoRateSlider::isSynced () const
{
    const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_sync");
    return sync != nullptr && sync->load() > 0.5f;
}

void LfoRateSlider::refresh ()
{
    const auto synced = isSynced();
    rate.setVisible (shown && ! synced);
    division.setVisible (shown && synced);
}

OperatorEnvOverview::OperatorEnvOverview (IlanaSynthAudioProcessor& p)
    : processorRef (p)
{
    setTooltip ("OP ENV\nThe Operator Env: each operator (oscillator) on it has its own, drawn here one over the other. "
                "Click to edit it.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (4);
}

void OperatorEnvOverview::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    IlanaTheme::paintWell (g, bounds, 6.0f);
    const auto plot = bounds.reduced (10.0f, 18.0f).withTrimmedTop (4.0f);
    const auto operators = OperatorPool::operatorsOnEnv (processorRef);

    for (size_t i = 0; i < operators.size(); ++i)
    {
        const auto osc = operators[i];
        const auto& shape = OperatorPool::envelopeShape (processorRef, FmOperatorInfo::prefixOf (osc), caches[(size_t) osc]);
        if (i == 0)
            OperatorPool::paintEnvelopeShape (g, plot, shape.values, OperatorPool::colour(), true);
        else
        {
            g.beginTransparencyLayer (0.55f);
            OperatorPool::paintEnvelopeShape (g, plot, shape.values, OperatorPool::colour(), false);
            g.endTransparencyLayer();
        }
    }

    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
    const auto count = (int) operators.size();
    g.drawText (count == 1 ? juce::String ("1 oscillator") : juce::String (count) + " oscillators", bounds.reduced (8.0f, 3.0f).removeFromTop (12.0f),
                juce::Justification::centredRight);
}

void OperatorEnvOverview::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasDraggedSinceMouseDown() && onClick != nullptr)
        onClick();
}

void OperatorEnvOverview::timerCallback ()
{
    if (IlanaAnim::showing (*this) && gate.check (processorRef.getUiEpoch()))
        repaint();
}

EnvSection::EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
    : processorRef (p),
      settings (settingsRef),
      ampDisplay (p, "amp", modSourceColour ((int) Mod::Source::AmpEnv), false),
      feDisplay (p, "fe", IlanaTheme::envIdentityColour (1)),
      f2eDisplay (p, "f2e", IlanaTheme::envIdentityColour (2)),
      meDisplay (p, "me", IlanaTheme::envIdentityColour (3)),
      e4Display (p, "e4", IlanaTheme::envIdentityColour (4)),
      ampA (p.apvts, "amp_attack", "ATTACK", modSourceColour ((int) Mod::Source::AmpEnv), false), ampD (p.apvts, "amp_decay", "DECAY", modSourceColour ((int) Mod::Source::AmpEnv), false),
      ampS (p.apvts, "amp_sustain", "SUSTAIN", modSourceColour ((int) Mod::Source::AmpEnv), false), ampR (p.apvts, "amp_release", "RELEASE", modSourceColour ((int) Mod::Source::AmpEnv), false),
      ampVel (p.apvts, "amp_velocity", "VELOCITY", modSourceColour ((int) Mod::Source::AmpEnv), false),
      feA (p.apvts, "fe_attack", "ATTACK"), feD (p.apvts, "fe_decay", "DECAY"),
      feS (p.apvts, "fe_sustain", "SUSTAIN"), feR (p.apvts, "fe_release", "RELEASE"),
      feVel (p.apvts, "filter_velocity", "VELOCITY"),
      f2A (p.apvts, "f2e_attack", "ATTACK"), f2D (p.apvts, "f2e_decay", "DECAY"),
      f2S (p.apvts, "f2e_sustain", "SUSTAIN"), f2R (p.apvts, "f2e_release", "RELEASE"),
      f2Vel (p.apvts, "f2e_velocity", "VELOCITY", IlanaTheme::envIdentityColour (2), false),
      meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
      meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
      meVel (p.apvts, "me_velocity", "VELOCITY", IlanaTheme::envIdentityColour (3), false),
      e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
      e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
      e4Vel (p.apvts, "e4_velocity", "VELOCITY", IlanaTheme::envIdentityColour (4), false)
{
    addChildComponent (opEditor);

    addAll (*this, ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display,
            ampA, ampD, ampS, ampR, ampVel,
            feA, feD, feS, feR, feVel,
            f2A, f2D, f2S, f2R, f2Vel,
            meA, meD, meS, meR, meVel,
            e4A, e4D, e4S, e4R, e4Vel);

    units.push_back ({ &ampDisplay, { &ampA, &ampD, &ampS, &ampR, &ampVel } });
    units.push_back ({ &feDisplay, { &feA, &feD, &feS, &feR, &feVel } });
    units.push_back ({ &f2eDisplay, { &f2A, &f2D, &f2S, &f2R, &f2Vel } });
    units.push_back ({ &meDisplay, { &meA, &meD, &meS, &meR, &meVel } });
    units.push_back ({ &e4Display, { &e4A, &e4D, &e4S, &e4R, &e4Vel } });

    // M5 DAHDSR and rate key scaling: a second row on every envelope.
    {
        const char* const prefixes[] { "amp", "fe", "f2e", "me", "e4" };

        for (int env = 0; env < 5; ++env)
            addStageTwoKnobs (p, prefixes[env], colourOf (env), units[(size_t) env], false);
    }

    for (int env = 6; env <= 16; ++env)
    {
        const auto prefix = "env" + juce::String (env);
        const auto colour = extraColour (env);
        ExtraUnit extra;
        extra.display = std::make_unique<EnvelopeDisplay> (p, prefix, colour);
        addChildComponent (*extra.display);
        const char* const suffixes[] { "attack", "decay", "sustain", "release", "velocity" };
        const char* const labels[] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VELOCITY" };
        for (int control = 0; control < 5; ++control)
        {
            extra.knobs[(size_t) control] = std::make_unique<KnobControl> (
                p.apvts, prefix + "_" + suffixes[control], labels[control], colour, false);
            addChildComponent (*extra.knobs[(size_t) control]);
        }
        std::vector<juce::Component*> knobs;
        for (auto& knob : extra.knobs)
            knobs.push_back (knob.get());
        units.push_back ({ extra.display.get(), std::move (knobs) });
        addStageTwoKnobs (p, prefix, colour, units.back(), false);
        extraUnits.push_back (std::move (extra));
    }

    // The knobs in the graph's order, left to right (UI review 8,
    // V8-18): DELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, then VEL
    // (where the envelope has one) and KEY RATE. A segment's curve is
    // bent by its dot on the graph.
    for (auto& unit : units)
    {
        // Built as A D S R [VEL], DELAY HOLD KEY RATE.
        const auto k = unit.knobs;
        const auto n = k.size() - 3;
        if (k.size() >= 7)
        {
            unit.knobs = { k[n], k[0], k[n + 1], k[1], k[2], k[3] };
            for (auto i = (size_t) 4; i < n; ++i)
                unit.knobs.push_back (k[i]);
            unit.knobs.push_back (k[n + 2]);
        }
    }

    selected = juce::jlimit (0, opPitchId, settings.getIntValue ("envSelected", 0));

    // AMP ENV's controls dim on a DX7 voice, whose operators play the
    // Operator Env (UI review 7, I7-8, V7-24); its graph dims itself.
    for (auto* knob : units[0].knobs)
        if (knob != nullptr)
            ampRules.add (*knob, [&p] { return FmOperatorInfo::ampEnvelopeInUse (p); }, ampUnusedText());

    updateVisibility();
    startTimerHz (4);
}

void EnvSection::select (int index)
{
    selected = juce::jlimit (0, opPitchId, index);

    if (selected < opEnvId && ! envelopeShown (processorRef, selected))
    {
        processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, selected, true);
    }

    settings.setValue ("envSelected", selected);
    updateVisibility();
}

void EnvSection::setCompact (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    opEditor.setCompact (compact);
    for (auto& unit : units)
        if (auto* envelope = dynamic_cast<EnvelopeDisplay*> (unit.display))
            envelope->setRulerInside (compact);
    for (auto& unit : units)
        for (auto* knob : unit.knobs)
            if (auto* dial = dynamic_cast<KnobControl*> (knob))
                dial->setPackedDial (compact ? compactDial : 0.0f);
    resized();
    repaint();
}

void EnvSection::resized ()
{
    auto area = getLocalBounds();

    if (selected >= opEnvId)
    {
        panel = {};
        opEditor.setBounds (area);
        return;
    }

    if (compact)
    {
        panel = {};
        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        auto& unit = units[(size_t) unitIndex];
        unit.display->setBounds (area.removeFromLeft (area.getWidth() - compactGap - compactKnobsWidth));
        area.removeFromLeft (compactGap);
        area.removeFromTop (compactGraphRise);
        // (The knobs come in the graph's order: DELAY ATTACK HOLD DECAY SUSTAIN RELEASE VEL KEY RATE.)
        const std::array<size_t, 8> order { 1, 3, 4, 5, 0, 2, 6, 7 };
        for (size_t i = 0; i < order.size(); ++i)
            if (order[i] < unit.knobs.size() && unit.knobs[order[i]] != nullptr)
            {
                const auto row = (int) i / 4, column = (int) i % 4;
                const auto cell = juce::Rectangle<int> (area.getX() + column * compactPitch, area.getY() + row * compactRow + (compactRow - (int) compactDial - 10) / 2,
                                                        compactPitch, (int) compactDial + 10);
                unit.knobs[order[i]]->setBounds (KnobControl::packedBoundsFor (cell));
            }
        return;
    }

    const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
    units[(size_t) unitIndex].display->setBounds (area.removeFromLeft ((area.getWidth() - 10) * 40 / 100));
    area.removeFromLeft (10);

    // The card: a 30 px header, then 8 px of padding round the knobs; it ends
    // level with the graph's frame, the ruler hanging below both.
    panel = area.withTrimmedBottom (rulerBand);
    auto inner = panel.withTrimmedTop (30).reduced (10, 8);
    // All nine in one row in the graph's order (DELAY ATTACK HOLD DECAY
    // SUSTAIN RELEASE VEL CURVE KEY RATE); a row is as tall as its dial,
    // its label and its value need, with no empty space under it.
    layoutRow (inner, units[(size_t) unitIndex].knobs);
}

void EnvSection::paint (juce::Graphics& g)
{
    if (panel.isEmpty() || selected >= opEnvId)
        return;

    const auto colour = colourOf (selected);
    const auto unusedAmp = selected == 0 && ! FmOperatorInfo::ampEnvelopeInUse (processorRef);

    IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (unusedAmp ? 0.15f : 0.35f));
    auto header = panel.reduced (12, 0).withHeight (30);
    IlanaTheme::paintCardHeader (g, header, envelopeTitle (selected),
                                 unusedAmp ? juce::String (ampUnusedText())
                                           : "drag the graph or the knobs",
                                 colour, 0);
}

juce::Colour EnvSection::colourOf (int env)
{
    const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), IlanaTheme::envIdentityColour (1), IlanaTheme::envIdentityColour (2),
                                   IlanaTheme::envIdentityColour (3), IlanaTheme::envIdentityColour (4) };
    return env < 5 ? colours[juce::jlimit (0, 4, env)] : extraColour (env + 1);
}

juce::Colour EnvSection::extraColour (int env)
{
    return modSourceColour ((int) Mod::Source::Env6 + env - 6);
}

void EnvSection::timerCallback ()
{
    const auto ampUsed = FmOperatorInfo::ampEnvelopeInUse (processorRef);
    if (ampUsed != ampWasUsed)
    {
        ampWasUsed = ampUsed;
        if (! ampUsed && selected == 0)
        {
            select (opEnvId);
            return;
        }
        repaint();
    }

    ampRules.apply();

    if (IlanaAnim::showing (*this) && selectionGone())
        updateVisibility();
}

void EnvSection::addStageTwoKnobs (IlanaSynthAudioProcessor& p, const juce::String& prefix, juce::Colour colour, Unit& unit,
                           bool followsTheme)
{
    const char* const suffixes[] { "_delay", "_hold", "_keyrate" };
    const char* const labels[] { "DELAY", "HOLD", "KEY RATE" };

    for (int i = 0; i < 3; ++i)
    {
        auto knob = std::make_unique<KnobControl> (p.apvts, prefix + suffixes[i], labels[i], colour, followsTheme);
        addChildComponent (*knob);
        unit.knobs.push_back (knob.get());
        stageTwoKnobs.push_back (std::move (knob));
    }
}

bool EnvSection::selectionGone () const
{
    if (selected < opEnvId)
        return ! envelopeShown (processorRef, selected);
    return selected == opEnvId ? ! operatorPoolShown (processorRef)
                               : ! (selected == opPitchId && operatorSourceShown (processorRef, Mod::Source::OpPitchEnv));
}

void EnvSection::updateVisibility ()
{
    // A remembered selection can point at an envelope this patch doesn't
    // show: the first card shown instead.
    if (selectionGone())
        for (int env = 0; env < (int) units.size(); ++env)
            if (envelopeShown (processorRef, env))
            {
                selected = env;
                break;
            }

    for (int i = 0; i < (int) units.size(); ++i)
    {
        const auto visible = i == selected;
        units[(size_t) i].display->setVisible (visible);

        for (auto* knob : units[(size_t) i].knobs)
            if (knob != nullptr)
                knob->setVisible (visible);
    }

    if (selected >= opEnvId)
        opEditor.setPitch (selected == opPitchId);
    opEditor.setVisible (selected >= opEnvId);

    resized();
    repaint();
}

ClockShView::ClockShView (IlanaSynthAudioProcessor& p)
    : processorRef (p)
{
    setTooltip ("Clocked S&H\nA new random value on every step of DIVISION at the host tempo. The trace is its value "
                "over the last few seconds.");
    history.fill (0.0f);
    startTimerHz (30);
}

void ClockShView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    IlanaTheme::paintWell (g, bounds, 6.0f);
    const auto plot = bounds.reduced (12.0f, 14.0f);
    const auto centreY = plot.getCentreY();
    const auto half = plot.getHeight() * 0.45f;

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

    juce::Path path;
    for (int i = 0; i < size; ++i)
    {
        const auto value = history[(size_t) ((head + i) % size)];
        const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (size - 1);
        const auto y = centreY - juce::jlimit (-1.0f, 1.0f, value) * half;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
        {
            path.lineTo (x, path.getCurrentPosition().y);
            path.lineTo (x, y);
        }
    }

    g.setColour (colour());
    g.strokePath (path, juce::PathStrokeType (1.7f));
    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
    g.drawText ("the last 4 s", bounds.reduced (12.0f, 2.0f).removeFromBottom (12.0f), juce::Justification::centredRight);
}

void ClockShView::timerCallback ()
{
    if (! IlanaAnim::showing (*this))
        return;

    history[(size_t) head] = processorRef.getSourceDisplayValue ((int) Mod::Source::ClockSh);
    head = (head + 1) % size;
    repaint();
}

LfoSection::LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
    : processorRef (p),
      settings (settingsRef),
      msegEditor (p),
      msegLoop (p.apvts, "mseg_loop", "LOOP"),
      msegRate (p.apvts, "mseg_rate", "RATE", MsegEditor::colour(), false),
      clockView (p),
      clockDiv (p.apvts, "clock_div", "DIVISION"),
      opLfoEditor (p)
{
    addChildComponent (opLfoEditor);

    msegLoop.showAsSwitch();
    msegMove.onClick = [this] { moveMsegToLfo(); };
    msegMove.setTooltip ("Move to an LFO\nDraws this MSEG on the first free LFO (SHAPE \xe2\x80\xba MSEG, same points and RATE) "
                         "and moves its routes there, so one editor draws every MSEG. Undo puts it back.");
    addChildComponent (msegMove);
    addChildComponent (msegEditor);
    addChildComponent (msegLoop);
    addChildComponent (msegRate);
    addChildComponent (clockView);
    addChildComponent (clockDiv);

    for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
    {
        auto display = std::make_unique<LfoDisplay> (p, lfo, lfoColour (lfo), false);
        addAndMakeVisible (*display);
        displays.push_back (std::move (display));

        auto controls = std::make_unique<Controls> (p, lfo + 1, lfoColour (lfo), false);
        controls->rate.addTo (*this);
        addAll (*this, controls->shape, controls->sync, controls->retrig, controls->key,
                controls->phase, controls->physA, controls->physB, controls->kick);
        // RATE shows the division while synced; the menu isn't needed.
        addChildComponent (controls->div);
        addAll (*this, controls->smooth, controls->stereo, controls->seed, controls->trigger, controls->axis, controls->loop);
        for (auto& knob : controls->sim)
            addChildComponent (*knob);
        addChildComponent (controls->fire);
        controls->fire.onClick = [this, lfo] { fire (lfo); };
        // The grouped list (display only; the saved index is the
        // parameter's): picking a simulated shape loads its defaults.
        LfoShapeMenu::apply (controls->shape, p, [this, lfo] (int) { shapePicked (lfo); });
        controlsList.push_back (std::move (controls));
    }

    selected = juce::jlimit (0, opLfoId, settings.getIntValue ("lfoSelected", 0));

    updateVisibility();
    for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
        processorRef.apvts.addParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
    startTimerHz (10);
}

LfoSection::~LfoSection ()
{
    for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
    {
        processorRef.apvts.removeParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
        // Let go of a FIRE pressed just before closing (its release timer
        // won't run once this is gone, and FIRE only acts on a press).
        if (auto* fireParameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo) + "_fire"))
            if (fireParameter->getValue() > 0.5f)
                fireParameter->setValueNotifyingHost (0.0f);
    }
    cancelPendingUpdate();
}

void LfoSection::setCompact (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    opLfoEditor.setCompact (compact);
    for (auto& display : displays)
        display->setRulerInside (compact);
    const auto dial = compact ? compactDial : 0.0f;
    for (size_t lfo = 0; lfo < controlsList.size(); ++lfo)
    {
        auto& c = *controlsList[lfo];
        for (auto* menu : { &c.shape, &c.trigger, &c.axis })
            menu->setPackedMenu (compact);
        c.rate.getRateKnob().setPackedDial (dial);
        c.rate.getRateKnob().setPackedValueRoom (compact ? 12 : 0);
        c.rate.getDivisionKnob().setPackedDial (dial);
        for (auto* knob : { &c.phase, &c.physA, &c.physB, &c.smooth, &c.stereo, &c.seed })
            knob->setPackedDial (dial);
        for (auto& knob : c.sim)
            knob->setPackedDial (dial);
        if (compact && pills.size() <= lfo)
        {
            std::array<std::unique_ptr<PillToggle>, 5> row;
            ToggleControl* toggles[] { &c.sync, &c.retrig, &c.key, &c.loop, &c.kick };
            for (size_t i = 0; i < row.size(); ++i)
            {
                row[i] = std::make_unique<PillToggle> (processorRef.apvts, toggles[i]->getParameterId(), toggles[i]->getButton().getButtonText(),
                                                       lfoColour ((int) lfo), toggles[i]->getButton().getTooltip());
                addChildComponent (*row[i]);
            }
            pills.push_back (std::move (row));
        }
    }
    msegRate.setPackedDial (dial);
    msegLoop.setPackedName (compact ? ToggleControl::NamePlace::right : ToggleControl::NamePlace::none);
    clockDiv.setPackedMenu (compact);
    updateVisibility();
}

int LfoSection::compactHeight () const
{
    if (selected == opLfoId)
        return titleSkip > 0 ? OperatorLfoEditor::compactHeightInTitle : OperatorLfoEditor::compactHeight;
    if (selected == msegId)
        return compactRowsTop (2) + compactRow;
    if (selected == clockId)
        return 72;
    const auto shape = shapeOf (selected);
    const auto simulated = LfoSimShapes::isSim (shape);
    return compactRowsTop (simulated ? 3 : 2) + compactRow * ((compactKnobs (selected).size() + 3) / 4);
}

bool LfoSection::compactWantsWideGraph () const
{
    if (selected == msegId)
        return true;
    if (selected < 0 || selected >= IlanaSynthAudioProcessor::numLfos)
        return false;
    const auto shape = shapeOf (selected);
    return shape >= 6 && shape <= 8;
}

void LfoSection::setCompactTitleRow (int skip, int row, int rightInset)
{
    titleSkip = skip;
    titleRow = row;
    titleRightInset = rightInset;
    opLfoEditor.setCompactTitleRow (skip, row, rightInset);
    resized();
}

bool LfoSection::hitTest (int x, int y)
{
    if (titleSkip <= 0 || y >= titleSkip)
        return true;
    // (A child's own hit test counts: OP LFO's editor reaches up over the row too, with only its SHAPE there.)
    for (auto* child : getChildren())
        if (child->isVisible() && child->getBounds().contains (x, y) && child->hitTest (x - child->getX(), y - child->getY()))
            return true;
    return false;
}

void LfoSection::resized ()
{
    auto area = getLocalBounds();

    if (selected == opLfoId)
    {
        panel = {};
        opLfoEditor.setBounds (area);
        return;
    }

    if (compact)
    {
        layoutCompact (area);
        return;
    }

    // The same split for every shape: the graph 112 : 100 against the
    // card, 10 px between (the design's).
    const auto displayArea = area.removeFromLeft ((area.getWidth() - 10) * 112 / 212);
    area.removeFromLeft (10);

    // One grid for every shape and module (UI review 8, I8-17 / S8-7 /
    // V8-6): the left column holds SHAPE, then the switches, then
    // TRIGGER / OUTPUT / FIRE; the right one the knobs, four to a row,
    // RATE always first and SMOOTH second. Flipping SHAPE adds or takes
    // away knobs and rows but never moves a control that stays. What
    // the grid leaves free at the bottom lists what the LFO drives.
    panel = area;
    // (A graph with a time ruler: the card ends level with its frame, P-4.)
    if (selected != msegId && selected != clockId)
    {
        const auto rulerShape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (juce::jlimit (0, (int) displays.size() - 1, selected) + 1) + "_shape")->load();
        if (! LfoSimShapes::isSim (rulerShape))
            panel = panel.withTrimmedBottom (lfoRulerBand);
    }
    const auto inner = panel.withTrimmedTop (30).reduced (10, 8);
    const auto grid = gridFor (inner);
    infoArea = {};

    if (selected == msegId)
    {
        msegEditor.setBounds (displayArea);
        msegMove.setBounds (grid.left[0].reduced (3, 1).withTrimmedTop (13).withHeight (24));
        msegLoop.setBounds (grid.switchSlot (0, 4));
        grid.place ({ &msegRate });
        infoArea = grid.freeBelow (false, false);
        return;
    }

    if (selected == clockId)
    {
        clockView.setBounds (displayArea);
        clockDiv.setBounds (grid.left[0].reduced (3, 1));
        infoArea = grid.freeBelow (false, false);
        return;
    }

    const auto displayIndex = juce::jlimit (0, (int) displays.size() - 1, selected);
    const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (displayIndex + 1) + "_shape")->load();
    const auto simulated = LfoSimShapes::isSim (shape);
    displays[(size_t) displayIndex]->setBounds (displayArea);
    auto& c = *controlsList[(size_t) displayIndex];

    c.shape.setBounds (grid.left[0].reduced (3, 1));

    // The switches: SYNC, RETRIG and KEY always in the same three places,
    // then the shape's own (LOOP, KICK).
    std::vector<juce::Component*> switches { &c.sync, &c.retrig, &c.key };
    if (simulated && LfoSimInfo::get (shape).usesLoop)
        switches.push_back (&c.loop);
    if (shape == LfoShapes::Pendulum || shape == LfoSimShapes::Pendulum)
        switches.push_back (&c.kick);
    for (size_t i = 0; i < switches.size(); ++i)
        switches[i]->setBounds (grid.lfoSwitchSlot ((int) i));

    if (simulated)
    {
        const auto& info = LfoSimInfo::get (shape);
        labelSimulated (c, shape);
        auto third = grid.left[2];
        const auto comboWidth = (third.getWidth() - 56) / 2;
        c.trigger.setBounds (third.removeFromLeft (comboWidth).reduced (3, 1));
        if (info.usesAxis)
            c.axis.setBounds (third.removeFromLeft (comboWidth).reduced (3, 1));
        else
            third.removeFromLeft (comboWidth);
        c.fire.setBounds (third.removeFromRight (56).reduced (3, 1).withTrimmedTop (13).withHeight (24));

        std::vector<juce::Component*> knobs { c.rate.layoutItem(), &c.smooth };
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (info.params[(size_t) param].name != nullptr)
                knobs.push_back (c.sim[(size_t) param].get());
        if (info.usesStereo)
            knobs.push_back (&c.stereo);
        if (info.usesSeed)
            knobs.push_back (&c.seed);
        grid.place (knobs);
        infoArea = grid.freeBelow (true, knobs.size() > (size_t) Grid::perRow);
    }
    else
    {
        std::vector<juce::Component*> knobs { c.rate.layoutItem(), &c.smooth, &c.phase };
        if (LfoShapes::isPhysics (shape))
            knobs.insert (knobs.end(), { nullptr, &c.physA, &c.physB });
        grid.place (knobs);
        infoArea = grid.freeBelow (false, knobs.size() > (size_t) Grid::perRow);
    }

    c.rate.matchBounds();
}

void LfoSection::paint (juce::Graphics& g)
{
    if (panel.isEmpty() || selected == opLfoId)
        return;

    auto header = panel.reduced (12, 0).withHeight (30);

    if (selected == msegId || selected == clockId)
    {
        const auto colour = selected == msegId ? MsegEditor::colour() : ClockShView::colour();
        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));
        // What plays it: oscillators using it as their ENVELOPE (or
        // warp envelope), and routes from its card.
        auto playedBy = selected == msegId ? envelopeUses (16, 17) : juce::StringArray();
        playedBy.add (describeModTargets (processorRef, selected == msegId ? Mod::Source::Mseg : Mod::Source::ClockSh));
        playedBy.removeEmptyStrings();
        IlanaTheme::paintCardHeader (g, header, selected == msegId ? "MSEG" : "CLOCKED S&H",
                                     selected == msegId ? "the patch's own MSEG, from older presets"
                                                        : "a new random value on every step of DIVISION",
                                     colour, 0);

        if (selected == msegId)
        {
            // Where the MOVE button's label would be on an LFO's SHAPE.
            if (msegMove.isVisible())
            {
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
                g.drawText ("AS AN LFO", msegMove.getBounds().withY (msegMove.getY() - 15).withHeight (13), juce::Justification::centredLeft);
            }

            // What its LOOP means, and why it has no MOVE button.
            juce::StringArray lines;
            lines.add (read ("mseg_loop") > 0.5f ? "LOOP on: it cycles at RATE."
                                                 : "LOOP off: it runs once and holds (once per note as an ENVELOPE).");
            if (! playedBy.isEmpty())
                lines.add ("Played by: " + playedBy.joinIntoString (", ") + ".");
            if (! msegMove.isVisible())
                lines.add (read ("mseg_loop") <= 0.5f ? "A one-shot MSEG stays here: an LFO always cycles."
                           : ! modSourceRouted (processorRef, Mod::Source::Mseg) ? "It plays as an ENVELOPE, which an LFO can't, so it stays here."
                                                                                  : "Every LFO is in use, so it stays here.");
            else
                lines.add (juce::String (juce::CharPointer_UTF8 ("MOVE TO LFO draws it on a free LFO (SHAPE \xe2\x80\xba MSEG) and moves its routes there.")));
            paintInfoLines (g, lines, colour);
        }
        else
            paintRoutes (g, Mod::Source::ClockSh, colour);
        return;
    }

    const auto colour = lfoColour (selected);
    IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));
    IlanaTheme::paintCardHeader (g, header, "LFO " + juce::String (selected + 1), LfoShapeMenu::runCaption (processorRef, selected), colour, 0);
    paintRoutes (g, Mod::lfoSourceFor (selected), colour);
}

juce::Colour LfoSection::lfoColour (int index)
{
    return IlanaSynthAudioProcessor::lfoColour (index);
}

void LfoSection::select (int index)
{
    selected = juce::jlimit (0, opLfoId, index);

    if (selected < msegId && ! processorRef.isLfoShown (selected))
    {
        processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, selected, true);
    }

    settings.setValue ("lfoSelected", selected);
    updateVisibility();
}

int LfoSection::shapeOf (int lfo) const
{
    const auto index = juce::jlimit (0, (int) displays.size() - 1, lfo);
    return (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (index + 1) + "_shape")->load();
}

std::vector<juce::Component*> LfoSection::compactKnobs (int lfo) const
{
    if (lfo < 0 || lfo >= (int) controlsList.size())
        return {};
    auto& c = *controlsList[(size_t) lfo];
    const auto shape = shapeOf (lfo);
    std::vector<juce::Component*> knobs { c.rate.layoutItem(), &c.smooth };
    if (LfoSimShapes::isSim (shape))
    {
        const auto& info = LfoSimInfo::get (shape);
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (info.params[(size_t) param].name != nullptr)
                knobs.push_back (c.sim[(size_t) param].get());
        if (info.usesStereo)
            knobs.push_back (&c.stereo);
        if (info.usesSeed)
            knobs.push_back (&c.seed);
    }
    else
    {
        knobs.push_back (&c.phase);
        if (LfoShapes::isPhysics (shape))
            knobs.insert (knobs.end(), { &c.physA, &c.physB });
    }
    return knobs;
}

void LfoSection::layoutCompact (juce::Rectangle<int> area)
{
    panel = {};
    infoArea = {};
    const auto title = area.removeFromTop (titleSkip).withHeight (titleRow);
    auto column = area.removeFromRight (compactColumn);
    area.removeFromRight (compactGap);
    const auto line = [&column] (int index)
    {
        return juce::Rectangle<int> (column.getX(), column.getY() + index * (compactLine + compactLineGap), column.getWidth(), compactLine);
    };
    const auto inTitle = titleSkip > 0;
    const auto placeKnobs = [&column, inTitle] (const std::vector<juce::Component*>& knobs, int top)
    {
        // (SHAPE in the title row: fewer than four knobs share the column's width.)
        const auto pitch = inTitle && ! knobs.empty() && knobs.size() < 4 ? column.getWidth() / (int) knobs.size() : compactPitch;
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            const auto row = (int) i / 4, slot = (int) i % 4;
            const juce::Rectangle<int> cell (column.getX() + slot * pitch, column.getY() + top + row * compactRow + (compactRow - (int) compactDial - 10) / 2,
                                             pitch, (int) compactDial + 10);
            knobs[i]->setBounds (KnobControl::packedBoundsFor (cell));
        }
    };

    if (selected == msegId)
    {
        msegEditor.setBounds (area);
        msegMove.setBounds (line (0));
        msegLoop.setBounds (line (1));
        placeKnobs ({ &msegRate }, compactRowsTop (2));
        return;
    }
    if (selected == clockId)
    {
        clockView.setBounds (area);
        clockDiv.setBounds (line (0));
        return;
    }

    const auto index = juce::jlimit (0, (int) displays.size() - 1, selected);
    const auto shape = shapeOf (index);
    const auto simulated = LfoSimShapes::isSim (shape);
    displays[(size_t) index]->setBounds (area);
    auto& c = *controlsList[(size_t) index];
    if (inTitle)
    {
        // (The switches and knobs centred beside the graph, no empty tail under them.)
        const auto used = compactRowsTop (simulated ? 2 : 1) + compactRow * (((int) compactKnobs (index).size() + 3) / 4);
        column.removeFromTop (juce::jmax (0, (column.getHeight() - used) / 2));
    }
    // (In the title row, SHAPE is as wide as the column and ends clear of the close button.)
    const auto lines = inTitle ? 0 : 1;
    c.shape.setBounds (inTitle ? juce::Rectangle<int> (title.getRight() - titleRightInset - compactColumn, title.getCentreY() - compactLine / 2, compactColumn, compactLine)
                               : line (0));

    if ((size_t) index < pills.size())
    {
        std::vector<PillToggle*> shown;
        for (auto& pill : pills[(size_t) index])
            if (pill->isVisible())
                shown.push_back (pill.get());
        // Packed from the left, or (SHAPE in the title row) one row of buttons filling the column, as on
        // PLAY's LFO card: centred word-wide over the knobs, RETRIG and KEY read as SMOOTH's and START's
        // names (UI review 27).
        const auto share = shown.empty() ? 0 : (column.getWidth() + 4) / (int) shown.size();
        auto x = column.getX();
        for (size_t i = 0; i < shown.size(); ++i)
        {
            const auto textW = (int) std::ceil (shown[i]->textWidth()) + 8;
            const auto fill = inTitle && textW <= share - 4;
            const auto left = fill ? column.getX() + (int) i * share : x;
            const auto width = fill ? share - 4 : textW;
            shown[i]->setBounds ({ left, line (lines).getY(), width, compactLine });
            x = left + width + 2;
        }
    }

    if (simulated)
    {
        labelSimulated (c, shape);
        auto third = line (lines + 1);
        c.fire.setBounds (third.removeFromRight (40));
        third.removeFromRight (6);
        if (LfoSimInfo::get (shape).usesAxis)
        {
            c.trigger.setBounds (third.removeFromLeft ((third.getWidth() - 4) / 2));
            third.removeFromLeft (4);
            c.axis.setBounds (third);
        }
        else
            c.trigger.setBounds (third);
    }

    placeKnobs (compactKnobs (index), compactRowsTop ((simulated ? 3 : 2) - (inTitle ? 1 : 0)));
    c.rate.matchBounds();
}

void LfoSection::labelRunSwitch (ToggleControl& toggle, bool simulated)
{
    const juce::String text (simulated ? "VOICE" : "RETRIG");
    if (toggle.getButton().getButtonText() == text)
        return;
    toggle.getButton().setButtonText (text);
    // The parameter's name stays the first line, as on every control.
    const auto name = toggle.getButton().getTooltip().upToFirstOccurrenceOf ("\n", false, false);
    const auto tooltip = name + (simulated ? "\nPer voice. On: each voice runs its own simulation, started by its note. Off: one run "
                                             "shared by every voice, restarted as TRIGGER says (a restart moves the held notes too)."
                                           : "\nRetrigger. On: each voice runs its own LFO, restarted from START by its note. Off: "
                                             "one LFO shared by every voice, running free.");
    toggle.setTooltip (tooltip);
    toggle.getButton().setTooltip (tooltip);
    toggle.repaint();
}

float LfoSection::read (const juce::String& id) const
{
    const auto* value = processorRef.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

juce::StringArray LfoSection::envelopeUses (int ampChoice, int warpChoice) const
{
    juce::StringArray uses;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        if (processorRef.isOscillatorShown (osc) && juce::roundToInt (read (prefix + "_amp_env")) == ampChoice)
            uses.add ("Osc" + juce::String (osc + 1) + " Amp");
        if (juce::roundToInt (read (prefix + "_pd_env")) == warpChoice)
            uses.add ("Osc" + juce::String (osc + 1) + " Warp");
    }
    return uses;
}

void LfoSection::shapePicked (int lfo)
{
    const auto shape = controlsList[(size_t) lfo]->shape.getComboBox().getSelectedId() - 1;
    if (! LfoSimShapes::isSim (shape))
        return;
    const auto& info = LfoSimInfo::get (shape);
    processorRef.performEdit ("LFO shape defaults", [this, lfo, &info]
    {
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_p" + juce::String (param + 1)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (info.params[(size_t) param].defaultValue);
                parameter->endChangeGesture();
            }
    });
}

void LfoSection::fire (int lfo)
{
    if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_fire"))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (1.0f);
        parameter->endChangeGesture();
        // Through a SafePointer: closing the plugin within the 60 ms would
        // otherwise leave this writing to a deleted parameter.
        juce::Component::SafePointer<juce::Component> safeThis (this);
        juce::Timer::callAfterDelay (60, [safeThis, parameter]
        {
            if (safeThis != nullptr)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (0.0f);
                parameter->endChangeGesture();
            }
        });
    }
    displays[(size_t) lfo]->triggerPreview();
}

void LfoSection::labelSimulated (Controls& c, int shape)
{
    if (c.labelledShape == shape)
        return;
    c.labelledShape = shape;
    const auto& info = LfoSimInfo::get (shape);
    for (int param = 0; param < LfoSimInfo::numParams; ++param)
    {
        auto& knob = *c.sim[(size_t) param];
        if (info.params[(size_t) param].name != nullptr)
            knob.setLabelText (info.params[(size_t) param].name);
        knob.getSlider().textFromValueFunction = [shape, param] (double value) { return LfoSimInfo::text (shape, param, (float) value); };
        knob.getSlider().updateText();
    }
}

auto LfoSection::gridFor (juce::Rectangle<int> inner)
     -> Grid
{
    Grid grid;
    auto left = inner.removeFromLeft (inner.getWidth() * 45 / 100);
    inner.removeFromLeft (6);
    for (auto& row : grid.left)
        row = left.removeFromTop (juce::jmin (Grid::rowHeight, left.getHeight()));
    grid.right = inner;
    constexpr int smallestKnob = 13 + IlanaTheme::KnobSize::minimum + 16;
    grid.knobHeight = (inner.getHeight() - Grid::rowGap) / 2;
    grid.twoRows = grid.knobHeight >= smallestKnob;
    // One row of knobs is as tall as its label, dial and value need (the
    // design's 38 px dial): what the LFO drives takes the rest.
    grid.oneRowHeight = juce::jmin (grid.knobHeight, 13 + 46 + 16);
    if (! grid.twoRows)
        grid.knobHeight = inner.getHeight();
    return grid;
}

void LfoSection::paintInfoLines (juce::Graphics& g, const juce::StringArray& lines, juce::Colour) const
{
    if (infoArea.isEmpty())
        return;
    auto area = infoArea.reduced (3, 0);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
    g.setColour (IlanaTheme::Ui::text3);
    for (const auto& line : lines)
    {
        if (area.getHeight() < 13)
            break;
        g.drawText (line, area.removeFromTop (14), juce::Justification::centredLeft, true);
    }
}

void LfoSection::paintRoutes (juce::Graphics& g, Mod::Source source, juce::Colour colour) const
{
    routeHits.clear();

    if (infoArea.getHeight() < 28)
        return;

    juce::StringArray routes;
    std::vector<int> routeSlots;
    const auto sourceB = Mod::lfoBIndexFor (source) < 0 && Mod::lfoIndexFor (source) >= 0 ? Mod::lfoBSourceFor (Mod::lfoIndexFor (source))
                                                                                           : Mod::Source::None;
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processorRef.readModSlot (slot);
        if (! routing.isActive())
            continue;
        const auto viaB = sourceB != Mod::Source::None && routing.source == sourceB;
        if (routing.source != source && ! viaB && routing.aux != source)
            continue;
        const auto depth = juce::roundToInt (routing.depth * 100.0f);
        routes.add ((viaB ? "OUT 2 " : routing.source != source ? "VIA " : "") + ModNames::destination (routing.destination) + "  "
                    + (depth > 0 ? "+" : "") + juce::String (depth) + "%");
        routeSlots.push_back (slot);
    }

    // The design's group: a thin rim, "DRIVES" in the source's colour at the
    // left and the number of routes at the right.
    g.setColour (IlanaTheme::Ui::well);
    g.fillRoundedRectangle (infoArea.toFloat(), 8.0f);
    g.setColour (IlanaTheme::Ui::line);
    g.drawRoundedRectangle (infoArea.toFloat().reduced (0.5f), 8.0f, 1.0f);

    // A short, wide band (under two rows of knobs): the title at the left,
    // the routes in columns beside it. Otherwise the title on top and the
    // routes one to a line.
    const auto compact = infoArea.getHeight() < 60 && infoArea.getWidth() >= 300;
    auto area = infoArea.reduced (10, 4);
    const auto countText = juce::String (routes.size()) + (routes.size() == 1 ? " ROUTE" : " ROUTES");
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
    if (compact)
    {
        auto titleCell = area.removeFromLeft (70);
        area.removeFromLeft (6);
        const auto half = titleCell.getHeight() / 2;
        g.setColour (colour);
        g.drawText ("DRIVES", titleCell.removeFromTop (half), juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text3);
        g.drawText (countText, titleCell, juce::Justification::centredLeft);
    }
    else
    {
        auto title = area.removeFromTop (16);
        g.setColour (colour);
        g.drawText ("DRIVES", title, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text3);
        g.drawText (countText, title, juce::Justification::centredRight);
        area.removeFromTop (2);
    }

    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
    if (routes.isEmpty())
    {
        // (A short band holds one short line at a readable size: the long
        // text shrank to about 6 px under the simulated shapes, review 21.)
        const auto shortBand = infoArea.getHeight() < 60;
        g.setFont (IlanaTheme::font (shortBand ? IlanaTheme::TextSize::label : IlanaTheme::TextSize::body));
        IlanaTheme::drawFitted (g, shortBand ? (compact ? "Nothing yet. Drag this card onto any knob to use it." : "Drag this onto any knob.")
                                             : "Nothing yet. Drag this card onto any knob to make it move that knob; every route it drives is listed here, with its depth.",
                                compact || shortBand ? area : area.withTrimmedRight (area.getWidth() / 3), juce::Justification::centredLeft,
                                shortBand ? 1 : 3);
        return;
    }

    // One route to a cell: its target, a bar of its depth (the width is the
    // room the cell leaves, so a short list is not a corner of text), and
    // the number at the right (V12-3). The name keeps all it needs; the bar
    // takes what is left (N16-6: "FM > OSC 2 > OS..." was cut by the bar).
    const auto lineHeight = compact ? 14 : 16;
    const auto columns = compact ? juce::jmax (1, area.getWidth() / 170) : 1;
    const auto rowsFit = juce::jmax (1, area.getHeight() / lineHeight);
    const auto capacity = columns * rowsFit;
    const auto cellWidth = area.getWidth() / columns;
    for (int i = 0; i < routes.size() && i < capacity; ++i)
    {
        const auto more = i == capacity - 1 && routes.size() > capacity;
        auto text = routes[i];
        const auto cell = juce::Rectangle<int> (area.getX() + (i % columns) * cellWidth, area.getY() + (i / columns) * lineHeight,
                                                cellWidth - 8, lineHeight);
        routeHits.push_back ({ cell, more ? -1 : routeSlots[(size_t) i] });
        const auto hovered = (int) routeHits.size() - 1 == hoverRoute;
        g.setColour (colour.withAlpha (hovered ? 1.0f : 0.85f));

        if (more)
        {
            g.drawText ("+" + juce::String (routes.size() - capacity + 1) + " more (MATRIX)", cell, juce::Justification::centredLeft, true);
            break;
        }

        // "FILTER 1 > Cutoff  +70%": the name, then the depth as a bar.
        const auto depthText = text.fromLastOccurrenceOf ("  ", false, false);
        const auto nameText = text.upToLastOccurrenceOf ("  ", false, false);
        auto row = cell;
        const auto valueArea = row.removeFromRight (compact ? 38 : 44);
        const auto nameWidth = juce::jmin (row.getWidth() - 16,
                                           juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), nameText) + 10);
        g.drawText (nameText, row.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);
        g.setColour (IlanaTheme::Ui::text);
        g.drawText (depthText, valueArea, juce::Justification::centredRight, true);
        g.setColour (colour.withAlpha (hovered ? 1.0f : 0.85f));
        const auto depth = juce::jlimit (0.0f, 1.0f, std::abs (depthText.retainCharacters ("0123456789").getFloatValue()) / 100.0f);
        const auto bar = row.withTrimmedLeft (4).withTrimmedRight (4).withSizeKeepingCentre (row.getWidth() - 8, 5).toFloat();
        if (bar.getWidth() > 10.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (colour.withAlpha (hovered ? 0.9f : 0.6f));
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * depth)), 2.0f);
        }

        // A row is a link to its matrix row: underlined under the pointer.
        if (hovered)
            g.fillRect (cell.getX(), cell.getBottom() - 2, juce::jmin (nameWidth, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), nameText)), 1);
    }
}

int LfoSection::routeAt (juce::Point<int> position) const
{
    for (size_t i = 0; i < routeHits.size(); ++i)
        if (routeHits[i].first.contains (position))
            return (int) i;
    return -1;
}

void LfoSection::mouseMove (const juce::MouseEvent& event)
{
    const auto hit = routeAt (event.getPosition());
    if (hit != hoverRoute)
    {
        hoverRoute = hit;
        setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint (infoArea);
    }
}

void LfoSection::mouseExit (const juce::MouseEvent&)
{
    if (hoverRoute >= 0)
    {
        hoverRoute = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint (infoArea);
    }
}

void LfoSection::mouseDown (const juce::MouseEvent& event)
{
    if (const auto hit = routeAt (event.getPosition()); hit >= 0 && ModNames::openMatrixRow() != nullptr)
        ModNames::openMatrixRow() (routeHits[(size_t) hit].second);
}

bool LfoSection::msegMovable () const
{
    return processorRef.legacyMsegTargetLfo() >= 0;
}

void LfoSection::moveMsegToLfo ()
{
    const auto lfo = processorRef.legacyMsegTargetLfo();
    if (lfo < 0)
        return;

    processorRef.performEdit ("Move MSEG to LFO " + juce::String (lfo + 1), [this] { processorRef.moveLegacyMsegToLfo(); });
    select (lfo);
}

bool LfoSection::cardInPool (int id) const
{
    if (id == msegId)
        return msegModuleInUse (processorRef);
    if (id == clockId)
        return ! modSlotsUsing (processorRef, { Mod::Source::ClockSh }).empty();
    return id == opLfoId && operatorSourceShown (processorRef, Mod::Source::OpLfo);
}

void LfoSection::updateVisibility ()
{
    // A remembered selection can point at a card this patch doesn't
    // show: the first LFO shown instead, else the MSEG.
    if ((selected < msegId && ! processorRef.isLfoShown (selected)) || (selected >= msegId && ! cardInPool (selected)))
    {
        selected = cardInPool (opLfoId) ? opLfoId : cardInPool (msegId) ? msegId : 0;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (processorRef.isLfoShown (lfo))
            {
                selected = lfo;
                break;
            }
    }

    opLfoEditor.setVisible (selected == opLfoId);
    msegEditor.setVisible (selected == msegId);
    if (const auto lfo = processorRef.legacyMsegTargetLfo(); lfo >= 0)
        msegMove.setButtonText ("MOVE TO LFO " + juce::String (lfo + 1));
    msegMove.setVisible (selected == msegId && msegMovable());
    msegLoop.setVisible (selected == msegId);
    msegRate.setVisible (selected == msegId);
    clockView.setVisible (selected == clockId);
    clockDiv.setVisible (selected == clockId);

    for (int lfo = 0; lfo < (int) displays.size(); ++lfo)
    {
        displays[(size_t) lfo]->setVisible (lfo == selected);
        auto& c = *controlsList[(size_t) lfo];
        const auto visible = lfo == selected;
        c.shape.setVisible (visible);
        c.rate.setShown (visible);
        c.sync.setVisible (visible);
        c.div.setVisible (false);
        c.retrig.setVisible (visible);
        c.key.setVisible (visible);
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape")->load();
        const auto simulated = LfoSimShapes::isSim (shape);
        const auto& info = LfoSimInfo::get (simulated ? shape : LfoSimShapes::RandomHold);
        // One trigger model (UI review 9, I9-1): the switch says where
        // the LFO runs; when it restarts is RETRIG's note on a plain
        // shape and TRIGGER's choice on a simulated one.
        labelRunSwitch (c.retrig, simulated);
        c.phase.setVisible (visible && ! simulated);
        c.physA.setVisible (visible && LfoShapes::isPhysics (shape));
        c.physB.setVisible (visible && LfoShapes::isPhysics (shape));
        c.kick.setVisible (visible && (shape == LfoShapes::Pendulum || shape == LfoSimShapes::Pendulum));
        c.smooth.setVisible (visible);
        c.trigger.setVisible (visible && simulated);
        c.axis.setVisible (visible && simulated && info.usesAxis);
        c.loop.setVisible (visible && simulated && info.usesLoop);
        c.stereo.setVisible (visible && simulated && info.usesStereo);
        c.seed.setVisible (visible && simulated && info.usesSeed);
        c.fire.setVisible (visible && simulated);
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            c.sim[(size_t) param]->setVisible (visible && simulated && info.params[(size_t) param].name != nullptr);

        // Compact: the pills stand in for the switches, with their names (RETRIG reads VOICE on a simulated shape).
        if (compact && (size_t) lfo < pills.size())
        {
            ToggleControl* toggles[] { &c.sync, &c.retrig, &c.key, &c.loop, &c.kick };
            for (size_t i = 0; i < 5; ++i)
            {
                pills[(size_t) lfo][i]->setVisible (toggles[i]->isVisible());
                pills[(size_t) lfo][i]->setText (toggles[i]->getButton().getButtonText());
                pills[(size_t) lfo][i]->setTooltip (toggles[i]->getButton().getTooltip());
                toggles[i]->setVisible (false);
            }
        }
    }

    resized();
    repaint();

    if (compact && (compactHeight() != lastCompactHeight || compactWantsWideGraph() != lastCompactWide))
    {
        lastCompactHeight = compactHeight();
        lastCompactWide = compactWantsWideGraph();
        if (onCompactSizeChanged != nullptr)
            onCompactSizeChanged();
    }
}

void LfoSection::timerCallback ()
{
    // A card that went (a patch load, the Clocked S&H unrouted).
    if ((selected < msegId && ! processorRef.isLfoShown (selected)) || (selected >= msegId && ! cardInPool (selected)))
        updateVisibility();

    if (selected >= msegId)
    {
        // MOVE TO LFO comes and goes with the MSEG's routes and LOOP.
        if (selected == msegId && msegMove.isVisible() != msegMovable())
            updateVisibility();
        if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
            repaint (panel);
        return;
    }

    auto& c = *controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, selected)];
    const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_shape")->load();
    if (shape != lastShape)
    {
        lastShape = shape;
        const juce::String labelsA[] { "HEIGHT", "SWING", "STIFF", "DRIVE" };
        const juce::String labelsB[] { "BOUNCE", "DAMP", "DAMP", "STICK" };
        if (LfoShapes::isPhysics (shape))
        {
            c.physA.setLabelText (labelsA[shape - LfoShapes::Bounce]);
            c.physB.setLabelText (labelsB[shape - LfoShapes::Bounce]);
        }
        updateVisibility();
    }

    if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
        repaint (panel);
}
