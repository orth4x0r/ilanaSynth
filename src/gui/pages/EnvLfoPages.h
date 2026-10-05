// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class FilterPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit FilterPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          panel1 (p, 1, FilterColours::filter (0)),
          panel2 (p, 2, FilterColours::filter (1)),
          westPanel (p),
          flow (p),
          balance (p.apvts, "filter_balance", "BALANCE", IlanaTheme::accent(), true),
          resOn (p.apvts, "res_on", "ON"),
          resAmount (p.apvts, "res_amount", "AMOUNT", resonatorColour(), true),
          resDecay (p.apvts, "res_decay", "DECAY", resonatorColour(), true),
          resOffset (p.apvts, "res_offset", "OFFSET", resonatorColour(), true),
          resKeytrack (p.apvts, "res_keytrack", "KEY TRK", resonatorColour(), true),
          bodyType (p.apvts, "body_type", "TYPE"),
          bodyMaterial (p.apvts, "body_material", "MATERIAL", resonatorColour(), true),
          bodySize (p.apvts, "body_size", "SIZE", resonatorColour(), true),
          bodyCouplingMode (p.apvts, "body_coupling_mode", "COUPLING"),
          bodyCoupling (p.apvts, "body_coupling", "COUPLE", resonatorColour(), true)
    {
        // The page (UI review 6): the response beside the signal flow, the
        // two filters side by side, then WEST and BODY, each its own card,
        // so nothing that shapes the sound hides behind a tab (I6-15/16).
        addAll (*this, filterDisplay, panel1, panel2, westPanel, flow, balance,
                resOn, resAmount, resDecay, resOffset, resKeytrack,
                bodyType, bodyMaterial, bodySize, bodyCouplingMode, bodyCoupling);

        // What doesn't act right now dims (the one rule for every page:
        // UI review 4, V26): BALANCE in serial, the body's knobs while it is
        // off. MATERIAL and SIZE shape the modal bodies only (Classic is the
        // old comb bank); without the body only Strings coupling does
        // anything.
        // UI review 7 (V7-34, S7-23): a module that is off draws every
        // control at the off alpha, as WEST and PLAY's oscillators do; the
        // rules below only judge the body's controls while it is on.
        const auto modal = [this] { return readValue ("body_type") > 0.5f; };
        const auto couplingMode = [this] { return juce::roundToInt (readValue ("body_coupling_mode")); };
        // (BALANCE is also disabled in serial, below: V6-18, S6-21.)
        effectRules.add (balance, effectRules.isOn ("filters_parallel"), "the filters are in SERIAL");
        bodyRules.add (bodyMaterial, modal, "BODY is Classic");
        bodyRules.add (bodySize, modal, "BODY is Classic");
        bodyRules.add (bodyCoupling, [modal, couplingMode] { return couplingMode() == 3 || (couplingMode() != 0 && modal()); },
                       "COUPLING is Off, or needs a modal BODY");
        startTimerHz (8);
    }

    // Sections that aren't modulation sources take the accent, so a source's
    // colour always means that source.
    static juce::Colour resonatorColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintSectionTitle (g, "RESPONSE", { headingX, 12, juce::jmax (0, filterDisplay.getRight() - headingX), headingHeight },
                           "cutoff across, resonance up and down");

        // Signal flow: a card like BODY beside it, the diagram and the
        // BALANCE knob inside; the subtitle says what BALANCE does now.
        {
            // (WEST in Filter 2's place is named as it: I7-32.)
            const auto active = readValue ("filters_parallel") > 0.5f;
            const juce::String second (filter2Replaced() ? "WEST" : "F2");
            IlanaTheme::paintCard (g, flowCard.toFloat(), 7.0f, IlanaTheme::Ui::text2.withAlpha (0.2f));
            IlanaTheme::paintCardHeader (g, flowCard.reduced (12, 0).removeFromTop (26), "SIGNAL FLOW",
                                         active ? "parallel: BALANCE mixes F1 and " + second : "serial: F1 into " + second,
                                         IlanaTheme::Ui::text2, 0);
        }

        IlanaTheme::paintCard (g, resonatorCard.toFloat(), 7.0f, resonatorColour().withAlpha (0.35f));
        IlanaTheme::paintCardTitle (g, resonatorCard.reduced (12, 0).removeFromTop (26), "BODY", resonatorColour());
        // The subtitle follows the title; the switch has the right of the header.
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("oscillator mix excites the body", resonatorCard.reduced (12, 0).removeFromTop (26).withTrimmedLeft (78),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        const auto panelHeight = juce::jlimit (128, 156, panel1.preferredHeight ((area.getWidth() - 10) / 2));
        // A module that is off folds to its header and its switch (UI review
        // 9, V9-4); the response and the flow take the room.
        westShown = westOn();
        bodyShown = bodyActive();
        const auto openHeight = juce::jlimit (166, 186, area.getHeight() * 9 / 25);
        const auto bottomHeight = westShown || bodyShown ? openHeight : foldedCardHeight;
        const auto topHeight = juce::jmax (120, area.getHeight() - panelHeight - bottomHeight - 16);

        // Top: the response (under its heading) and, beside it, the flow.
        auto top = area.removeFromTop (topHeight);
        flowCard = top.removeFromRight (juce::jlimit (330, 440, top.getWidth() * 2 / 5));
        top.removeFromRight (10);
        top.removeFromTop (headingHeight);
        filterDisplay.setBounds (top);
        {
            // The flow takes the card's width (its filters need it: I7-1);
            // BALANCE sits in its lower right corner, under OUT.
            const auto flowArea = flowCard.reduced (8, 0).withTrimmedTop (26).withTrimmedBottom (8);
            flow.setBounds (flowArea);
            const auto balanceWidth = 70;
            const auto balanceHeight = juce::jmin (juce::jmin (70, flowArea.getHeight() / 2), preferredControlHeight (&balance, balanceWidth - 6));
            const auto corner = flowArea.withTrimmedLeft (flowArea.getWidth() - balanceWidth).withTrimmedTop (flowArea.getHeight() - balanceHeight - 4)
                                    .withTrimmedBottom (4).withTrimmedRight (4);
            balance.setBounds (corner);
            flow.setReservedCorner (corner - flowArea.getPosition());
        }
        area.removeFromTop (8);

        auto panels = area.removeFromTop (panelHeight);
        panel1.setBounds (panels.removeFromLeft ((panels.getWidth() - 10) / 2));
        panels.removeFromLeft (10);
        panel2.setBounds (panels);

        area.removeFromTop (8);
        auto bottom = area.removeFromTop (bottomHeight);
        westPanel.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 5).withHeight (westShown ? bottomHeight : foldedCardHeight));

        bottom.removeFromLeft (10);
        resonatorCard = bottom.withHeight (bodyShown ? bottomHeight : foldedCardHeight);
        for (juce::Component* control : { (juce::Component*) &resAmount, (juce::Component*) &resDecay, (juce::Component*) &resOffset,
                                          (juce::Component*) &resKeytrack, (juce::Component*) &bodyType, (juce::Component*) &bodyMaterial,
                                          (juce::Component*) &bodySize, (juce::Component*) &bodyCouplingMode, (juce::Component*) &bodyCoupling })
            control->setVisible (bodyShown);
        // The on switch at the right of the header, as on the oscillator cards.
        resOn.setBounds (IlanaTheme::cardSwitchBounds (resonatorCard, resonatorCard.getY() + 13));
        // The grid WEST's card uses (S7-24): its menus' row, then its knobs,
        // so the two cards' rows line up side by side.
        auto resArea = resonatorCard.reduced (10, 0);
        resArea.removeFromTop (30);
        resArea.removeFromBottom (4);
        auto menuRow = resArea.removeFromTop (juce::jmin (52, resArea.getHeight() / 3));
        const auto menuWidth = (menuRow.getWidth() - menuRow.getWidth() * 2 / 5) / 3;
        bodyType.setBounds (menuRow.removeFromLeft (menuWidth).reduced (3, 2));
        bodyCouplingMode.setBounds (menuRow.removeFromLeft (menuWidth).reduced (3, 2));
        resArea.removeFromTop (2);
        layoutRow (resArea, { &resAmount, &resDecay, &bodyMaterial, &bodySize, &resOffset, &resKeytrack, &bodyCoupling });
    }

private:
    // Balance only acts in parallel (disabled in serial, so it can't be
    // set by mistake); resonator knobs only when it is on.
    void timerCallback() override
    {
        const auto parallelNow = readValue ("filters_parallel") > 0.5f;
        const auto replacedNow = filter2Replaced();

        if (westOn() != westShown || bodyActive() != bodyShown)
            resized();

        if (parallelNow != wasParallel || replacedNow != wasReplaced)
        {
            wasParallel = parallelNow;
            wasReplaced = replacedNow;
            repaint (flowCard);
        }

        // (A rule dims it first, while it is still enabled.)
        effectRules.apply();

        // The body: off, everything at the off alpha (but COUPLING when it
        // couples the strings, which works without it); on, its own rules.
        if (readValue ("res_on") > 0.5f)
        {
            for (auto* control : std::initializer_list<juce::Component*> { &resAmount, &resDecay, &resOffset, &resKeytrack, &bodyType, &bodyCouplingMode })
                if (control->getAlpha() != 1.0f)
                    control->setAlpha (1.0f);
            for (auto* knob : { &resAmount, &resDecay, &resOffset, &resKeytrack })
                knob->setInactiveNote ({});
            bodyRules.apply();
        }
        else
        {
            const auto strings = juce::roundToInt (readValue ("body_coupling_mode")) == 3;
            for (auto* control : std::initializer_list<juce::Component*> { &resAmount, &resDecay, &resOffset, &resKeytrack, &bodyType,
                                                                           &bodyMaterial, &bodySize, &bodyCouplingMode, &bodyCoupling })
            {
                const auto live = strings && (control == &bodyCouplingMode || control == &bodyCoupling);
                const auto alpha = live ? 1.0f : FilterColours::offAlpha;
                if (control->getAlpha() != alpha)
                    control->setAlpha (alpha);
                if (auto* knob = dynamic_cast<KnobControl*> (control))
                    knob->setInactiveNote (live ? juce::String() : juce::String ("BODY is off"));
            }
        }

        // (Its tooltip keeps the rule's "no effect now" note.)
        if (balance.isEnabled() != parallelNow)
            balance.setEnabled (parallelNow);
    }

    float readValue (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool filter2Replaced() const { return readValue ("west_on") > 0.5f && juce::roundToInt (readValue ("west_pos")) == 1; }
    bool westOn() const { return readValue ("west_on") > 0.5f; }
    // The body shows while it is on, or while it couples the strings (which
    // works without it).
    bool bodyActive() const { return readValue ("res_on") > 0.5f || juce::roundToInt (readValue ("body_coupling_mode")) == 3; }

    static constexpr int foldedCardHeight = 40;
    bool westShown = true, bodyShown = true;

    IlanaSynthAudioProcessor& processorRef;
    FilterDisplay filterDisplay;
    FilterPanel panel1, panel2;
    WestPanel westPanel;
    SignalFlow flow;
    KnobControl balance;
    bool wasParallel = false, wasReplaced = false;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    ComboControl bodyType, bodyCouplingMode;
    KnobControl bodyMaterial, bodySize, bodyCoupling;
    juce::Rectangle<int> flowCard, resonatorCard;
    EffectRules effectRules { processorRef }, bodyRules { processorRef };
};

// An LFO's RATE (UI review 4, V12 and S22): Hz while free-running, note
// values (1/16, 1/8T...) while SYNC is on. Two knobs share one place, one
// on the RATE parameter and one on DIVISION, and SYNC picks which is shown;
// both values stay stored. Modulating RATE still acts while synced (the
// DSP scales the division's rate by it, up to 4 octaves each way), so the
// division knob stands in for RATE's modulation: it shows RATE's ring and
// dots, and a source dropped on it routes to RATE.
// The knobs are the page's children: lay out layoutItem() (the RATE knob)
// like any knob, then call matchBounds().
class LfoRateControl : private juce::Timer
{
public:
    LfoRateControl (IlanaSynthAudioProcessor& p, int lfoIndex, juce::Colour accent, bool followsTheme)
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

    void addTo (juce::Component& parent)
    {
        parent.addChildComponent (rate);
        parent.addChildComponent (division);
        refresh();
    }

    // Whether the page shows this LFO's controls at all.
    void setShown (bool shouldShow)
    {
        shown = shouldShow;
        refresh();
    }

    juce::Component* layoutItem() { return &rate; }

    void setBounds (juce::Rectangle<int> bounds)
    {
        rate.setBounds (bounds);
        matchBounds();
    }

    // The division knob follows the RATE knob's place.
    void matchBounds() { division.setBounds (rate.getBounds()); }

    KnobControl& getRateKnob() { return rate; }
    KnobControl& getDivisionKnob() { return division; }

    bool isSynced() const
    {
        const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_sync");
        return sync != nullptr && sync->load() > 0.5f;
    }

    // Follows SYNC (the timer does this too).
    void refresh()
    {
        const auto synced = isSynced();

        if (rate.isVisible() != (shown && ! synced))
            rate.setVisible (shown && ! synced);

        if (division.isVisible() != (shown && synced))
            division.setVisible (shown && synced);
    }

private:
    void timerCallback() override { refresh(); }

    IlanaSynthAudioProcessor& processorRef;
    int lfo = 0;
    bool shown = true;
    KnobControl rate, division;
};

// PLAY's OP ENV tab (UI review 8, I8-18): the Operator Env every operator
// plays, drawn as one overlay of the operators' envelopes. It is a picture
// and a link, not a second editor: a click opens the OP ENV editor.
class OperatorEnvOverview : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    explicit OperatorEnvOverview (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("OP ENV\nThe Operator Env: each operator (oscillator) on it has its own, drawn here one over the other. "
                    "Click to edit it.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (4);
    }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
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
        g.drawText (count == 1 ? juce::String ("1 operator") : juce::String (count) + " operators", bounds.reduced (8.0f, 3.0f).removeFromTop (12.0f),
                    juce::Justification::centredRight);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasDraggedSinceMouseDown() && onClick != nullptr)
            onClick();
    }

private:
    void timerCallback() override
    {
        if (isShowing() && gate.check (processorRef.getUiEpoch()))
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<OperatorPool::ShapeCache, (size_t) OscillatorIds::count> caches;
    IlanaAnim::ChangeGate gate;
};

// ENV 1-16's names on the MOD page (cards, panel title), 0-based.
inline juce::String envelopeTitle (int env)
{
    const juce::StringArray titles { "AMP ENV", "FILT ENV", "FILT 2 ENV", "ENV 4" };
    return env < 4 ? titles[env] : "ENV " + juce::String (env + 1);
}

class EnvSection : public juce::Component,
                   private juce::Timer
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, []
          {
              std::vector<EnvThumbBar::Env> envs;
              const char* const prefixes[] { "amp", "fe", "f2e", "me", "e4" };
              for (int env = 0; env < 16; ++env)
                  envs.push_back ({ envelopeTitle (env), env < 5 ? juce::String (prefixes[env]) : "env" + juce::String (env + 1),
                                    envelopeSource (env), colourOf (env) });
              return envs;
          }()),
          ampDisplay (p, "amp", modSourceColour ((int) Mod::Source::AmpEnv), false),
          feDisplay (p, "fe", juce::Colour (0xffc86bff)),
          f2eDisplay (p, "f2e", juce::Colour (0xff8f9dff)),
          meDisplay (p, "me", juce::Colour (0xff8fff3b)),
          e4Display (p, "e4", juce::Colour (0xff5b8cff)),
          ampA (p.apvts, "amp_attack", "ATTACK", modSourceColour ((int) Mod::Source::AmpEnv), false), ampD (p.apvts, "amp_decay", "DECAY", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampS (p.apvts, "amp_sustain", "SUSTAIN", modSourceColour ((int) Mod::Source::AmpEnv), false), ampR (p.apvts, "amp_release", "RELEASE", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampVel (p.apvts, "amp_velocity", "VEL", modSourceColour ((int) Mod::Source::AmpEnv), false), ampCurve (p.apvts, "amp_curve", curveLabel, modSourceColour ((int) Mod::Source::AmpEnv), false),
          feA (p.apvts, "fe_attack", "ATTACK"), feD (p.apvts, "fe_decay", "DECAY"),
          feS (p.apvts, "fe_sustain", "SUSTAIN"), feR (p.apvts, "fe_release", "RELEASE"),
          feVel (p.apvts, "filter_velocity", "VEL"), feCurve (p.apvts, "fe_curve", curveLabel, juce::Colour (0xffc86bff), false),
          f2A (p.apvts, "f2e_attack", "ATTACK"), f2D (p.apvts, "f2e_decay", "DECAY"),
          f2S (p.apvts, "f2e_sustain", "SUSTAIN"), f2R (p.apvts, "f2e_release", "RELEASE"),
          f2Vel (p.apvts, "f2e_velocity", "VEL", juce::Colour (0xff8f9dff), false),
          f2Curve (p.apvts, "f2e_curve", curveLabel, juce::Colour (0xff8f9dff), false),
          meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
          meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
          meVel (p.apvts, "me_velocity", "VEL", juce::Colour (0xff8fff3b), false),
          meCurve (p.apvts, "me_curve", curveLabel, juce::Colour (0xff8fff3b), false),
          e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
          e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
          e4Vel (p.apvts, "e4_velocity", "VEL", juce::Colour (0xff5b8cff), false),
          e4Curve (p.apvts, "e4_curve", curveLabel, juce::Colour (0xff5b8cff), false)
    {
        // One card per envelope in the patch, the Operator Env's two, and a
        // "+" (no 1-16 ruler: UI review 6, V6-8). Past what fits, cards fold
        // into a "N MORE" card; the pool never scrolls (UI review 7, V7-17).
        addAndMakeVisible (thumbs);

        // OP ENV and OP PITCH, pool members edited below like the others
        // (UI review 7, I7-7), only on a patch that plays the Operator Env
        // (a DX7 voice), and there first in the row (UI review 8, I8-5 /
        // S8-1 / V8-1). OP PITCH stays, greyed, while the matrix still
        // routes it on a patch that no longer plays the Operator Env.
        {
            const auto anyOperator = [&p] { return FmOperatorInfo::anyOperatorEnv (p); };
            EnvThumbBar::ExtraCard opEnv;
            opEnv.title = "OP ENV";
            opEnv.colour = OperatorPool::colour();
            opEnv.isShown = [&p] { return operatorPoolShown (p); };
            opEnv.pinnedFirst = true;
            opEnv.tooltip = "OP ENV\nThe Operator Env: the DX7 envelope each oscillator on it plays (its level). Click to edit it "
                            "below, an operator at a time. It shapes its operators only, so it isn't a modulation source (its dashed edge: "
                            "nothing to drag).";
            opEnv.paintShape = [this] (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
            {
                const auto onEnv = OperatorPool::operatorsOnEnv (processorRef);
                const auto osc = opEditor.isPitch() || onEnv.empty() ? (onEnv.empty() ? 0 : onEnv.front()) : opEditor.getSelectedOperator();
                OperatorPool::paintEnvelopeShape (g, plot, OperatorPool::envelopeShape (processorRef, FmOperatorInfo::prefixOf (osc), opEnvShape).values,
                                                  OperatorPool::colour(), active);
            };
            opEnv.targets = [&p]
            {
                const auto count = (int) OperatorPool::operatorsOnEnv (p).size();
                // Short enough for the narrowest card (UI review 8, S8-10).
                return count == 0 ? juce::String() : juce::String (count) + (count == 1 ? " op" : " ops");
            };
            thumbs.addExtraCard (std::move (opEnv));

            EnvThumbBar::ExtraCard opPitch;
            opPitch.title = "OP PITCH";
            opPitch.source = Mod::Source::OpPitchEnv;
            opPitch.colour = OperatorPool::colour();
            opPitch.isShown = [&p] { return operatorSourceShown (p, Mod::Source::OpPitchEnv); };
            opPitch.isActive = anyOperator;
            opPitch.pinnedFirst = true;
            opPitch.tooltip = "OP PITCH\nThe Operator Env's pitch envelope, for the whole voice. Click to edit it below; drag it onto "
                              "a knob to modulate that knob too (source: OP PITCH).";
            opPitch.paintShape = [this] (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
            {
                OperatorPool::paintEnvelopeShape (g, plot, OperatorPool::envelopeShape (processorRef, {}, opPitchShape).values,
                                                  OperatorPool::colour(), active);
            };
            opPitch.targets = [this, &p]
            {
                juce::StringArray fixed;
                if (OperatorPool::envelopeShape (p, {}, opPitchShape).moves && FmOperatorInfo::anyOperatorEnv (p))
                    fixed.add ("Op pitch");
                return describeModTargets (processorRef, Mod::Source::OpPitchEnv, fixed);
            };
            thumbs.addExtraCard (std::move (opPitch));
        }
        addChildComponent (opEditor);

        addAll (*this, ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display,
                ampA, ampD, ampS, ampR, ampVel, ampCurve,
                feA, feD, feS, feR, feVel, feCurve,
                f2A, f2D, f2S, f2R, f2Vel, f2Curve,
                meA, meD, meS, meR, meVel, meCurve,
                e4A, e4D, e4S, e4R, e4Vel, e4Curve);

        units.push_back ({ &ampDisplay, { &ampA, &ampD, &ampS, &ampR, &ampVel, &ampCurve } });
        units.push_back ({ &feDisplay, { &feA, &feD, &feS, &feR, &feVel, &feCurve } });
        units.push_back ({ &f2eDisplay, { &f2A, &f2D, &f2S, &f2R, &f2Vel, &f2Curve } });
        units.push_back ({ &meDisplay, { &meA, &meD, &meS, &meR, &meVel, &meCurve } });
        units.push_back ({ &e4Display, { &e4A, &e4D, &e4S, &e4R, &e4Vel, &e4Curve } });

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
            const char* const suffixes[] { "attack", "decay", "sustain", "release", "velocity", "curve" };
            const char* const labels[] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VEL", curveLabel };
            for (int control = 0; control < 6; ++control)
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
        // V8-18): DELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, then what
        // shapes them all: VEL, CURVE (every segment at once; a segment's
        // dot bends that one alone) and KEY RATE.
        for (auto& unit : units)
        {
            // Built as A D S R VEL CURVE, DELAY HOLD KEY RATE.
            const auto k = unit.knobs;
            if (k.size() == 9)
                unit.knobs = { k[6], k[0], k[7], k[1], k[2], k[3], k[4], k[5], k[8] };
        }

        selected = juce::jlimit (0, opPitchId, settings.getIntValue ("envSelected", 0));

        // AMP ENV's controls dim on a DX7 voice, whose operators play the
        // Operator Env (UI review 7, I7-8, V7-24); its graph dims itself.
        for (auto* knob : units[0].knobs)
            if (knob != nullptr)
                ampRules.add (*knob, [&p] { return FmOperatorInfo::ampEnvelopeInUse (p); }, ampUnusedText());

        thumbs.onSelect = [this] (int index) { select (index); };
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };
        // The Operator Env, absent from a patch that doesn't play it, is
        // one pick away under "+" (UI review 8, S8-1 / V8-1).
        thumbs.plusOffer = [this] { return operatorEnvOffer(); };
        thumbs.onPlusOffer = [this] { addOperatorEnv(); };

        updateVisibility();
        startTimerHz (4);
    }

    // The "+" menu's Operator Env item; empty while the patch plays it.
    juce::String operatorEnvOffer() const
    {
        if (operatorPoolShown (processorRef))
            return {};
        return "OP ENV (DX7): OSC " + juce::String (operatorEnvTarget() + 1) + " plays the Operator Env";
    }

    // Puts the Operator Env in the patch: the first playing oscillator's
    // ENVELOPE becomes OP ENV (its cards then appear), and OP ENV opens.
    void addOperatorEnv()
    {
        if (auto* param = processorRef.apvts.getParameter (FmOperatorInfo::prefixOf (operatorEnvTarget()) + "_amp_env"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) OperatorEg::envelopeChoice));
            param->endChangeGesture();
        }

        thumbs.refreshLayout();
        select (opEnvId);
        resized();
        repaint();
    }

    // The bend of an envelope's segments has one name everywhere it is
    // set (UI review 6, I6-22): CURVE, as on the LFO and the matrix.
    static constexpr const char* curveLabel = "CURVE";

    // OP ENV and OP PITCH after ENV 1-16.
    static constexpr int opEnvId = 16;
    static constexpr int opPitchId = 17;

    // One wording for "unused" on every page (UI review 8, S8-17), PLAY's
    // ENVELOPE card and the MOD page's AMP ENV and OP cards alike.
    static const char* ampUnusedText() { return "unused: the oscillators play OP ENV"; }

    // Shows an envelope, adding its card when the patch doesn't have it yet.
    void select (int index)
    {
        selected = juce::jlimit (0, opPitchId, index);

        if (selected < opEnvId && ! envelopeShown (processorRef, selected))
        {
            processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, selected, true);
            thumbs.refreshLayout();
        }

        settings.setValue ("envSelected", selected);
        updateVisibility();
    }

    // What the section needs below its cards: the panel's header and one
    // row of full-size knobs.
    static constexpr int cardHeight = 48;
    static constexpr int panelHeightNeeded = 6 + 20 + (13 + IlanaTheme::KnobSize::main + 16) + 6 + 12;

    void resized() override
    {
        auto area = getLocalBounds();

        thumbs.setViewWidth (area.getWidth());
        thumbs.setBounds (area.removeFromTop (cardHeight));
        area.removeFromTop (8);

        if (selected >= opEnvId)
        {
            panel = {};
            opEditor.setBounds (area);
            return;
        }

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (area.getWidth() * 47 / 100 /* the LFO display above splits at the same place */).reduced (2));
        area.removeFromLeft (8);

        // Same panel shape as the LFOs: heading, then the stage knobs.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        // First row: the ADSR, velocity and curve as before; second row:
        // delay, hold and key rate.
        const auto& knobs = units[(size_t) unitIndex].knobs;
        const std::vector<juce::Component*> first (knobs.begin(), knobs.begin() + juce::jmin ((int) knobs.size(), 6));
        const std::vector<juce::Component*> second (knobs.begin() + (int) first.size(), knobs.end());
        // Two rows when both fit full-size knobs (the six stages, then VEL,
        // CURVE and KEY RATE); otherwise one row of all of them in the
        // graph's order, so the dials stay as big as the LFO's rather than
        // shrinking to fit two short rows.
        constexpr int fullRow = 13 + 58 + 16 + 6;

        if (second.empty() || inner.getHeight() < fullRow * 2)
        {
            std::vector<juce::Component*> all (first);
            all.insert (all.end(), second.begin(), second.end());
            layoutRow (inner, all);
            return;
        }

        auto rows = inner.withSizeKeepingCentre (inner.getWidth(), fullRow * 2);
        layoutRow (rows.removeFromTop (fullRow), first);
        layoutRow (rows.withWidth (rows.getWidth() * (int) second.size() / 6), second);
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty() || selected >= opEnvId)
            return;

        const auto colour = colourOf (selected);
        const auto unusedAmp = selected == 0 && ! FmOperatorInfo::ampEnvelopeInUse (processorRef);

        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (unusedAmp ? 0.15f : 0.35f));
        auto header = panel.reduced (12, 0).withHeight (26);
        IlanaTheme::paintCardHeader (g, header, envelopeTitle (selected),
                                     unusedAmp ? juce::String (ampUnusedText())
                                               : "drag the graph or the knobs; a segment's dot bends it, CURVE bends all",
                                     colour, 0);
    }

    int getSelected() const { return selected; }
    EnvThumbBar& getThumbs() { return thumbs; }
    OperatorEnvEditor& getOperatorEditor() { return opEditor; }

    // ENV 1-16's colours, 0-based.
    static juce::Colour colourOf (int env)
    {
        const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), juce::Colour (0xffc86bff), juce::Colour (0xff8f9dff),
                                       juce::Colour (0xff8fff3b), juce::Colour (0xff5b8cff) };
        return env < 5 ? colours[juce::jlimit (0, 4, env)] : extraColour (env + 1);
    }

private:
    // ENV 6-16 in their mod source colours.
    static juce::Colour extraColour (int env)
    {
        return modSourceColour ((int) Mod::Source::Env6 + env - 6);
    }

    // A patch loaded, or an envelope removed, can take away the one shown.
    // A patch whose operators all play the Operator Env (a DX7 voice)
    // opens on OP ENV rather than the unused AMP ENV (UI review 7, I7-8).
    void timerCallback() override
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

        if (isShowing() && selectionGone())
            updateVisibility();
    }

    struct ExtraUnit
    {
        std::unique_ptr<EnvelopeDisplay> display;
        std::array<std::unique_ptr<KnobControl>, 6> knobs;
    };
    struct Unit
    {
        juce::Component* display = nullptr;
        std::vector<juce::Component*> knobs;
    };

    void addStageTwoKnobs (IlanaSynthAudioProcessor& p, const juce::String& prefix, juce::Colour colour, Unit& unit,
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

    std::vector<std::unique_ptr<KnobControl>> stageTwoKnobs;

    // Which oscillator "+ › OP ENV" puts on the Operator Env: the first
    // playing one (OSC 1 when none plays).
    int operatorEnvTarget() const
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (FmOperatorInfo::isPlaying (processorRef, osc))
                return osc;
        return 0;
    }

    // The selection points at a card the pool doesn't have (an envelope
    // removed, or OP ENV / OP PITCH on a patch without the Operator Env).
    bool selectionGone() const
    {
        return selected < opEnvId ? ! envelopeShown (processorRef, selected) : ! thumbs.isCardInPool (selected);
    }

    void updateVisibility()
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

        thumbs.setSelected (selected);
        resized();
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    EnvThumbBar thumbs;
    OperatorEnvEditor opEditor { processorRef };
    OperatorPool::ShapeCache opEnvShape, opPitchShape;
    EffectRules ampRules { processorRef };
    bool ampWasUsed = true;
    EnvelopeDisplay ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display;
    KnobControl ampA, ampD, ampS, ampR, ampVel, ampCurve;
    KnobControl feA, feD, feS, feR, feVel, feCurve;
    KnobControl f2A, f2D, f2S, f2R, f2Vel, f2Curve;
    KnobControl meA, meD, meS, meR, meVel, meCurve;
    KnobControl e4A, e4D, e4S, e4R, e4Vel, e4Curve;
    std::vector<Unit> units;
    std::vector<ExtraUnit> extraUnits;
    int selected = 0;
};

// The Clocked S&H source's picture: its value over the last few seconds,
// as held steps, so its DIVISION can be seen (it has no cycle of its own).
class ClockShView : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    explicit ClockShView (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Clocked S&H\nA new random value on every step of DIVISION at the host tempo. The trace is its value "
                    "over the last few seconds.");
        history.fill (0.0f);
        startTimerHz (30);
    }

    static juce::Colour colour() { return modSourceColour ((int) Mod::Source::ClockSh); }

    void paint (juce::Graphics& g) override
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

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        history[(size_t) head] = processorRef.getSourceDisplayValue ((int) Mod::Source::ClockSh);
        head = (head + 1) % size;
        repaint();
    }

    static constexpr int size = 120;
    IlanaSynthAudioProcessor& processorRef;
    std::array<float, (size_t) size> history {};
    int head = 0;
};

class LfoSection : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private IlanaAnim::FrameTimer
{
public:
    // The cards past the LFOs: the patch's MSEG while it is used, the
    // Clocked S&H once routed (edited here, beside the LFOs: UI review 6,
    // V5-7 / I6-21), and the Operator Env's LFO (UI review 7, I7-7).
    static constexpr int msegId = IlanaSynthAudioProcessor::numLfos;
    static constexpr int clockId = IlanaSynthAudioProcessor::numLfos + 1;
    static constexpr int opLfoId = IlanaSynthAudioProcessor::numLfos + 2;

    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, [] (int index) { return lfoColour (index); }),
          msegEditor (p),
          msegLoop (p.apvts, "mseg_loop", "LOOP"),
          msegRate (p.apvts, "mseg_rate", "RATE", MsegEditor::colour(), false),
          clockView (p),
          clockDiv (p.apvts, "clock_div", "DIVISION"),
          opLfoEditor (p)
    {
        // One card per LFO in the patch, the modulators edited beside them,
        // then a "+" (no 1-16 ruler: UI review 6, V6-8). Past what fits,
        // cards fold into a "N MORE" card; the pool never scrolls (V7-17).
        addAndMakeVisible (thumbs);
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

        // The MSEG module: MSEG is an LFO shape now (SHAPE › MSEG: UI review
        // 7, S7-7 / V7-19; review 8, I8-4 / S8-5 / V8-2), so the patch's own
        // older one has a card only while the patch uses it (a route, an
        // oscillator's ENVELOPE), and its panel moves its routes onto an LFO.
        LfoThumbBar::ExtraCard mseg;
        mseg.title = "MSEG";
        mseg.source = Mod::Source::Mseg;
        mseg.colour = MsegEditor::colour();
        mseg.isShown = [&p] { return msegModuleInUse (p); };
        mseg.tooltip = "MSEG\nThe patch's own MSEG, from older presets (as an oscillator's ENVELOPE it plays once per note). "
                       "Click to edit it below; MOVE TO LFO there draws it on an LFO instead (SHAPE \xe2\x80\xba MSEG), the "
                       "one drawn-shape editor.";
        mseg.valueAt = [this] (double phase) { return msegValueAt ((float) phase); };
        mseg.phase = [this] { return (double) processorRef.getMsegPhase(); };
        mseg.rateText = [this] { return describeValue ("mseg_rate", read ("mseg_rate")); };
        mseg.fixedUses = [this] { return envelopeUses (16, 17); };
        thumbs.addExtraCard (std::move (mseg));

        LfoThumbBar::ExtraCard clock;
        clock.title = "CLOCKED S&H";
        clock.source = Mod::Source::ClockSh;
        clock.colour = ClockShView::colour();
        clock.isShown = [this] { return ! modSlotsUsing (processorRef, { Mod::Source::ClockSh }).empty(); };
        clock.valueAt = [] (double phase)
        {
            static const float held[] { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.4f, -0.1f };
            return held[juce::jlimit (0, 7, (int) (phase * 8.0))];
        };
        clock.stepped = true;
        clock.rateText = [this]
        {
            auto* parameter = processorRef.apvts.getParameter ("clock_div");
            return parameter != nullptr ? parameter->getCurrentValueAsText() : juce::String();
        };
        thumbs.addExtraCard (std::move (clock));

        LfoThumbBar::ExtraCard opLfo;
        opLfo.title = "OP LFO";
        opLfo.source = Mod::Source::OpLfo;
        opLfo.colour = OperatorPool::colour();
        // Only on a patch that plays the Operator Env, and there first (UI
        // review 8, I8-5); greyed while only an old route keeps it.
        opLfo.isShown = [&p] { return operatorSourceShown (p, Mod::Source::OpLfo); };
        opLfo.isActive = [&p] { return FmOperatorInfo::anyOperatorEnv (p); };
        opLfo.pinnedFirst = true;
        opLfo.tooltip = "OP LFO\nThe Operator Env's LFO (the DX7's), for the whole voice. Click to edit it below; drag it onto a "
                        "knob to modulate that knob too (source: OP LFO).";
        opLfo.valueAt = [this] (double phase)
        {
            return OperatorPool::lfoWaveValue (juce::roundToInt (read ("opeg_lfo_wave")), phase);
        };
        opLfo.stepped = false;
        opLfo.rateText = [this] { return describeValue ("opeg_lfo_speed", read ("opeg_lfo_speed")); };
        opLfo.fixedUses = [this]
        {
            juce::StringArray uses;
            if (read ("opeg_lfo_pmd") > 0.5f && read ("opeg_lfo_pms") > 0.5f)
                uses.add ("Op pitch");
            if (read ("opeg_lfo_amd") > 0.5f)
                uses.add ("Op amp");
            return uses;
        };
        thumbs.addExtraCard (std::move (opLfo));
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

        thumbs.onSelect = [this] (int index)
        {
            selected = index;
            settings.setValue ("lfoSelected", selected);
            updateVisibility();
        };

        updateVisibility();
        for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
            processorRef.apvts.addParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
        startTimerHz (10);
    }

    ~LfoSection() override
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

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { lastShape = -1; timerCallback(); }

    static constexpr int cardHeight = 54;

    void resized() override
    {
        auto area = getLocalBounds();

        thumbs.setViewWidth (area.getWidth());
        thumbs.setBounds (area.removeFromTop (cardHeight));
        area.removeFromTop (8);

        if (selected == opLfoId)
        {
            panel = {};
            opLfoEditor.setBounds (area);
            return;
        }

        // The same split for every shape (and as the envelopes below).
        const auto displayArea = area.removeFromLeft (area.getWidth() * OperatorPool::graphPercent / 100).reduced (2);
        area.removeFromLeft (8);

        // One grid for every shape and module (UI review 8, I8-17 / S8-7 /
        // V8-6): the left column holds SHAPE, then the switches, then
        // TRIGGER / OUTPUT / FIRE; the right one the knobs, four to a row,
        // RATE always first and SMOOTH second. Flipping SHAPE adds or takes
        // away knobs and rows but never moves a control that stays. What
        // the grid leaves free at the bottom lists what the LFO drives.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
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

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty() || selected == opLfoId)
            return;

        auto header = panel.reduced (12, 0).withHeight (26);

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

    static juce::Colour lfoColour (int index)
    {
        return IlanaSynthAudioProcessor::lfoColour (index);
    }

    // Shows an LFO (or the MSEG, or OP LFO), adding its card when the patch
    // doesn't have it yet.
    void select (int index)
    {
        selected = juce::jlimit (0, opLfoId, index);

        if (selected < msegId && ! processorRef.isLfoShown (selected))
        {
            processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, selected, true);
            thumbs.refreshLayout();
        }

        settings.setValue ("lfoSelected", selected);
        updateVisibility();
    }

    // (PLAY's LFO card labels its switch the same way.) RETRIG on a plain shape (one LFO per voice, restarted by its note);
    // PER VOICE on a simulated one, whose TRIGGER says when it restarts.
    static void labelRunSwitch (ToggleControl& toggle, bool simulated)
    {
        const juce::String text (simulated ? "PER VOICE" : "RETRIG");
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

    // The UI test reaches these through here.
    int getSelected() const { return selected; }
    LfoRateControl& getRateControl (int lfo) { return controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, lfo)]->rate; }
    LfoThumbBar& getThumbs() { return thumbs; }
    juce::Rectangle<int> getPanelBounds() const { return panel; }
    OperatorLfoEditor& getOperatorEditor() { return opLfoEditor; }

private:
    struct Controls
    {
        Controls (IlanaSynthAudioProcessor& p, int lfo, juce::Colour accent, bool followsTheme)
            : Controls (p, p.apvts, lfo, accent, followsTheme) {}

        Controls (IlanaSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state, int lfo, juce::Colour accent, bool followsTheme)
            : shape (state, "lfo" + juce::String (lfo) + "_shape", "SHAPE"),
              rate (p, lfo - 1, accent, followsTheme),
              sync (state, "lfo" + juce::String (lfo) + "_sync", "SYNC"),
              div (state, "lfo" + juce::String (lfo) + "_div", "DIVISION"),
              retrig (state, "lfo" + juce::String (lfo) + "_retrig", "RETRIG"),
              key (state, "lfo" + juce::String (lfo) + "_key", "KEY"),
              phase (state, "lfo" + juce::String (lfo) + "_phase", "START", accent, followsTheme)
              , physA (state, "lfo" + juce::String (lfo) + "_phys_a", "HEIGHT", accent, followsTheme)
              , physB (state, "lfo" + juce::String (lfo) + "_phys_b", "BOUNCE", accent, followsTheme)
              , kick (state, "lfo" + juce::String (lfo) + "_kick", "KICK")
              , smooth (state, "lfo" + juce::String (lfo) + "_smooth", "SMOOTH", accent, followsTheme)
              , stereo (state, "lfo" + juce::String (lfo) + "_stereo", "STEREO", accent, followsTheme)
              , seed (state, "lfo" + juce::String (lfo) + "_seed", "SEED", accent, followsTheme)
              , trigger (state, "lfo" + juce::String (lfo) + "_trigger", "TRIGGER")
              , axis (state, "lfo" + juce::String (lfo) + "_axis", "OUT 1 AXIS")
              , loop (state, "lfo" + juce::String (lfo) + "_loop", "LOOP")
        {
            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                sim.push_back (std::make_unique<KnobControl> (state, "lfo" + juce::String (lfo) + "_p" + juce::String (param + 1),
                                                              "P" + juce::String (param + 1), accent, followsTheme));
            fire.setButtonText ("FIRE");
            fire.setTooltip ("Fire\nTriggers the LFO now, as a new note or TRIGGER would: drops the ball, plucks the "
                             "spring, restarts a seeded sequence.");
        }

        ComboControl shape;
        LfoRateControl rate;
        ToggleControl sync;
        ComboControl div;
        ToggleControl retrig;
        ToggleControl key;
        KnobControl phase;
        KnobControl physA, physB;
        ToggleControl kick;
        // M8.1
        KnobControl smooth, stereo, seed;
        ComboControl trigger, axis;
        ToggleControl loop;
        juce::TextButton fire;
        std::vector<std::unique_ptr<KnobControl>> sim;
        int labelledShape = -1;
    };

    float read (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    // The level the MSEG plays at `phase` (0..1), as Mseg::valueAt does,
    // for its card.
    float msegValueAt (float phase) const
    {
        float levels[4], times[4], total = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            levels[i] = read ("mseg_level" + juce::String (i + 1));
            times[i] = juce::jmax (0.01f, read ("mseg_time" + juce::String (i + 1)));
            total += times[i];
        }

        auto cumulative = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const auto duration = times[i] / total;
            if (phase < cumulative + duration || i == 3)
            {
                const auto local = juce::jlimit (0.0f, 1.0f, duration > 0.0001f ? (phase - cumulative) / duration : 0.0f);
                const auto to = i < 3 ? levels[i + 1] : (read ("mseg_loop") > 0.5f ? levels[0] : levels[3]);
                return levels[i] + (to - levels[i]) * local;
            }
            cumulative += duration;
        }
        return levels[3];
    }

    // Oscillators that play (or warp with) an envelope choice, for the
    // card's "what it drives".
    juce::StringArray envelopeUses (int ampChoice, int warpChoice) const
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

    // Choosing a simulated shape from the menu loads its knobs' defaults
    // (presets and automation keep whatever they set).
    void shapePicked (int lfo)
    {
        const auto shape = controlsList[(size_t) lfo]->shape.getComboBox().getSelectedId() - 1;
        if (! LfoSimShapes::isSim (shape))
            return;
        const auto& info = LfoSimInfo::get (shape);
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_p" + juce::String (param + 1)))
                parameter->setValueNotifyingHost (info.params[(size_t) param].defaultValue);
    }

    // FIRE: a momentary press of the LFO's fire parameter.
    void fire (int lfo)
    {
        if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_fire"))
        {
            parameter->setValueNotifyingHost (1.0f);
            // Through a SafePointer: closing the plugin within the 60 ms would
            // otherwise leave this writing to a deleted parameter.
            juce::Component::SafePointer<juce::Component> safeThis (this);
            juce::Timer::callAfterDelay (60, [safeThis, parameter]
            {
                if (safeThis != nullptr)
                    parameter->setValueNotifyingHost (0.0f);
            });
        }
        displays[(size_t) lfo]->triggerPreview();
    }

    // Names and value text of a simulated shape's knobs.
    void labelSimulated (Controls& c, int shape)
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

    // The panel's grid (UI review 8, I8-17): three option rows on the left
    // (SHAPE; the switches; TRIGGER, OUTPUT and FIRE), up to two rows of
    // four knobs on the right.
    struct Grid
    {
        static constexpr int perRow = 4;
        static constexpr int rowHeight = 13 + 24 + 14;
        static constexpr int rowGap = 8;
        std::array<juce::Rectangle<int>, 3> left;
        juce::Rectangle<int> right;
        int knobHeight = 0, oneRowHeight = 0;
        bool twoRows = true;

        // Switch `index` of `slots` across the second left row.
        juce::Rectangle<int> switchSlot (int index, int slots) const
        {
            const auto width = left[1].getWidth() / slots;
            return left[1].withX (left[1].getX() + width * index).withWidth (width).reduced (2, 1);
        }

        // The LFO's switches in fixed places of unequal width: RETRIG's slot is
        // wide enough for "PER VOICE" on a simulated shape (V10-6 / I10-6), the
        // others for their names.
        juce::Rectangle<int> lfoSwitchSlot (int index) const
        {
            static constexpr float weights[] { 1.0f, 1.7f, 1.0f, 1.1f, 1.1f };
            auto before = 0.0f, total = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                total += weights[i];
                before += i < index ? weights[i] : 0.0f;
            }
            const auto unit = (float) left[1].getWidth() / total;
            return left[1].withX (left[1].getX() + juce::roundToInt (before * unit)).withWidth (juce::roundToInt (weights[index] * unit)).reduced (2, 1);
        }

        // Knobs in reading order, `perRow` to a row (a null keeps a place
        // empty); all in one row when two rows wouldn't fit.
        void place (const std::vector<juce::Component*>& knobs) const
        {
            if (! twoRows)
            {
                layoutRow (right, knobs);
                return;
            }

            // (Knobs for one row only take its taller height, so what the
            // LFO drives has a band no wider than its text needs: V12-3.)
            const auto height = knobs.size() <= (size_t) perRow ? oneRowHeight : knobHeight;
            for (size_t row = 0; row < 2 && row * (size_t) perRow < knobs.size(); ++row)
            {
                std::vector<juce::Component*> items ((size_t) perRow, nullptr);
                for (size_t k = 0; k < (size_t) perRow && row * (size_t) perRow + k < knobs.size(); ++k)
                    items[k] = knobs[row * (size_t) perRow + k];
                layoutRow (right.withTrimmedTop ((int) row * (height + rowGap)).withHeight (height), items);
            }
        }

        // What the grid leaves free at the bottom: under the second option
        // row (or the third, when used), across the knobs too while their
        // second row is empty.
        juce::Rectangle<int> freeBelow (bool thirdRowUsed, bool secondKnobRowUsed) const
        {
            const auto top = (thirdRowUsed ? left[2] : left[1]).getBottom() + 4;
            const auto bottom = right.getBottom();
            if (bottom - top < 24)
                return {};
            if (secondKnobRowUsed || ! twoRows)
                return { left[0].getX(), top, left[0].getWidth(), bottom - top };
            return { left[0].getX(), juce::jmax (top, right.getY() + oneRowHeight + 4), right.getRight() - left[0].getX(),
                     bottom - juce::jmax (top, right.getY() + oneRowHeight + 4) };
        }
    };

    static Grid gridFor (juce::Rectangle<int> inner)
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
        grid.oneRowHeight = grid.knobHeight;
        if (! grid.twoRows)
            grid.knobHeight = inner.getHeight();
        return grid;
    }

    // The free band's lines, small, one under the other.
    void paintInfoLines (juce::Graphics& g, const juce::StringArray& lines, juce::Colour) const
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

    // What a source drives, listed in the free band (UI review 8, S8-7 /
    // V8-6: the panel's room says something rather than sitting empty):
    // "DRIVES" then each route's target and depth, two columns.
    void paintRoutes (juce::Graphics& g, Mod::Source source, juce::Colour colour) const
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

        // The list sits in a recessed well, so a short list is not a bare band.
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (infoArea.toFloat(), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (infoArea.toFloat().reduced (0.5f), 5.0f, 1.0f);

        auto area = infoArea.reduced (8, 3);
        auto title = area.removeFromTop (14);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text3);
        g.drawText ("DRIVES", title, juce::Justification::centredLeft);

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        if (routes.isEmpty())
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            IlanaTheme::drawFitted (g, "Nothing yet. Drag this card onto any knob to make it move that knob; every route it drives is listed here, with its depth.",
                                    area.withTrimmedRight (area.getWidth() / 3), juce::Justification::topLeft, 3);
            return;
        }

        // One route to a line: its target, a bar of its depth (the width is the
        // room the panel leaves, so a short list is not a corner of text), and
        // the number at the right (V12-3).
        constexpr int lineHeight = 16;
        const auto capacity = juce::jmax (1, area.getHeight() / lineHeight);
        for (int i = 0; i < routes.size() && i < capacity; ++i)
        {
            const auto more = i == capacity - 1 && routes.size() > capacity;
            auto text = routes[i];
            const auto cell = juce::Rectangle<int> (area.getX(), area.getY() + i * lineHeight, area.getWidth() - 8, lineHeight);
            routeHits.push_back ({ cell, more ? -1 : routeSlots[(size_t) i] });
            const auto hovered = (int) routeHits.size() - 1 == hoverRoute;
            g.setColour (colour.withAlpha (hovered ? 1.0f : 0.85f));

            if (more)
            {
                g.drawText ("+" + juce::String (routes.size() - capacity + 1) + " more (MATRIX)", cell, juce::Justification::centredLeft, true);
                break;
            }

            // "FILTER 1 › Cutoff  +70%": the name, then the depth as a bar.
            const auto depthText = text.fromLastOccurrenceOf ("  ", false, false);
            const auto nameText = text.upToLastOccurrenceOf ("  ", false, false);
            auto row = cell;
            const auto valueArea = row.removeFromRight (44);
            const auto nameWidth = juce::jmin (row.getWidth() * 55 / 100,
                                               juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), nameText) + 10);
            g.drawText (nameText, row.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);
            g.drawText (depthText, valueArea, juce::Justification::centredRight, true);
            const auto depth = juce::jlimit (0.0f, 1.0f, std::abs (depthText.retainCharacters ("0123456789").getFloatValue()) / 100.0f);
            const auto bar = row.withTrimmedLeft (6).withTrimmedRight (6).withSizeKeepingCentre (row.getWidth() - 12, 4).toFloat();
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

    // The DRIVES rows jump to their matrix rows (V9-31).
    int routeAt (juce::Point<int> position) const
    {
        for (size_t i = 0; i < routeHits.size(); ++i)
            if (routeHits[i].first.contains (position))
                return (int) i;
        return -1;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto hit = routeAt (event.getPosition());
        if (hit != hoverRoute)
        {
            hoverRoute = hit;
            setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint (infoArea);
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverRoute >= 0)
        {
            hoverRoute = -1;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint (infoArea);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (const auto hit = routeAt (event.getPosition()); hit >= 0 && ModNames::openMatrixRow() != nullptr)
            ModNames::openMatrixRow() (routeHits[(size_t) hit].second);
    }

    // What a DRIVES row stands for (the UI test): its matrix slot, or -1.
    int getNumRouteRows() const { return (int) routeHits.size(); }
    int getRouteSlot (int row) const { return juce::isPositiveAndBelow (row, (int) routeHits.size()) ? routeHits[(size_t) row].second : -2; }
    juce::Point<int> getRouteCentre (int row) const { return routeHits[(size_t) row].first.getCentre(); }

    // MOVE TO LFO: what the old MSEG module's routes do moves onto an LFO
    // drawn the same (SHAPE › MSEG), the one drawn-shape editor (UI review
    // 8, I8-4 / V8-2). Patch loads do it already (review 9, I9-2); the
    // button stays for a patch whose MSEG became movable since.
    bool msegMovable() const
    {
        return processorRef.legacyMsegTargetLfo() >= 0;
    }

    void moveMsegToLfo()
    {
        const auto lfo = processorRef.legacyMsegTargetLfo();
        if (lfo < 0)
            return;

        processorRef.performEdit ("Move MSEG to LFO " + juce::String (lfo + 1), [this] { processorRef.moveLegacyMsegToLfo(); });
        thumbs.refreshLayout();
        select (lfo);
    }

    void updateVisibility()
    {
        // A remembered selection can point at a card this patch doesn't
        // show: the first LFO shown instead, else the MSEG.
        if ((selected < msegId && ! processorRef.isLfoShown (selected)) || (selected >= msegId && ! thumbs.isCardInPool (selected)))
        {
            selected = thumbs.isCardInPool (opLfoId) ? opLfoId : thumbs.isCardInPool (msegId) ? msegId : 0;
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
        }

        thumbs.setSelected (selected);
        resized();
        repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        // A card that went (a patch load, the Clocked S&H unrouted).
        if ((selected < msegId && ! processorRef.isLfoShown (selected)) || (selected >= msegId && ! thumbs.isCardInPool (selected)))
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

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    LfoThumbBar thumbs;
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    MsegEditor msegEditor;
    ToggleControl msegLoop;
    KnobControl msegRate;
    ClockShView clockView;
    ComboControl clockDiv;
    OperatorLfoEditor opLfoEditor;
    juce::TextButton msegMove;
    juce::Rectangle<int> infoArea; // the grid's free band (routes, the MSEG's notes)
    mutable std::vector<std::pair<juce::Rectangle<int>, int>> routeHits; // DRIVES rows and their matrix slots
    int hoverRoute = -1;
    int selected = 0;
    int lastShape = -1;
};

class EnvLfoPage : public juce::Component
{
public:
    EnvLfoPage (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : lfoSection (p, settingsRef),
          envSection (p, settingsRef)
    {
        addAndMakeVisible (lfoSection);
        addAndMakeVisible (envSection);
    }

    void selectLfo (int index) { lfoSection.select (index); }
    void selectEnvelope (int index) { envSection.select (index); }

    // The UI test reaches the sections through here.
    LfoSection& getLfoSection() { return lfoSection; }
    EnvSection& getEnvSection() { return envSection; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        // Headings on the card-title line every page uses (12 px down).
        paintSectionTitle (g, "LFO", juce::Rectangle<int> (headingX, 12, 200, headingHeight));
        paintSectionTitle (g, "ENVELOPES", juce::Rectangle<int> (headingX, lfoBottom + 4, 200, headingHeight));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (headingHeight);

        // The envelopes take what one row of full-size knobs needs (their
        // graph beside it); the LFOs get the rest, so a chaos shape's two
        // rows of knobs fit with their values clear of the dials.
        const auto shared = area.getHeight() - headingHeight - 8;
        const auto envHeight = juce::jlimit (EnvSection::cardHeight + 8 + EnvSection::panelHeightNeeded,
                                             juce::jmax (EnvSection::cardHeight + 8 + EnvSection::panelHeightNeeded, shared / 2),
                                             shared * 42 / 100);
        auto lfoArea = area.removeFromTop (shared - envHeight);
        lfoBottom = lfoArea.getBottom();
        area.removeFromTop (headingHeight + 8);
        envSection.setBounds (area);
        lfoSection.setBounds (lfoArea);
    }

private:
    LfoSection lfoSection;
    EnvSection envSection;
    int lfoBottom = 0;
};
} // namespace
