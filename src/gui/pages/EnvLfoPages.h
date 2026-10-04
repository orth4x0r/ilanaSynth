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
                           "drag across for cutoff, up and down for resonance");

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
        const auto bottomHeight = juce::jlimit (166, 186, area.getHeight() * 9 / 25);
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
        westPanel.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 5));

        bottom.removeFromLeft (10);
        resonatorCard = bottom;
        // The on switch at the right of the header, as on the oscillator cards.
        resOn.setBounds (IlanaTheme::cardSwitchBounds (resonatorCard, resonatorCard.getY() + 13));
        // The grid WEST's card uses (S7-24): its menus' row, then its knobs,
        // so the two cards' rows line up side by side.
        auto resArea = bottom.reduced (10, 0);
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

// ENV 1-16's names on the MOD page (cards, panel title), 0-based.
inline juce::String envelopeTitle (int env)
{
    const juce::StringArray titles { "AMP ENV", "FILT ENV", "FILT 2 ENV", "MOD ENV" };
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
        // (UI review 7, I7-7): greyed with "unused" while no oscillator
        // plays the Operator Env.
        {
            const auto anyOperator = [&p] { return FmOperatorInfo::anyOperatorEnv (p); };
            EnvThumbBar::ExtraCard opEnv;
            opEnv.title = "OP ENV";
            opEnv.colour = OperatorPool::colour();
            opEnv.isActive = anyOperator;
            opEnv.tooltip = "OP ENV\nThe Operator Env: the DX7 envelope each oscillator on it plays (its level). Click to edit it "
                            "below, an operator at a time. It shapes its operators only, so it isn't a modulation source.";
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
                return count == 0 ? juce::String() : juce::String (count) + (count == 1 ? " operator" : " operators");
            };
            thumbs.addExtraCard (std::move (opEnv));

            EnvThumbBar::ExtraCard opPitch;
            opPitch.title = "OP PITCH";
            opPitch.source = Mod::Source::OpPitchEnv;
            opPitch.colour = OperatorPool::colour();
            opPitch.isActive = anyOperator;
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

        selected = juce::jlimit (0, opPitchId, settings.getIntValue ("envSelected", 0));

        // AMP ENV's controls dim on a DX7 voice, whose operators play the
        // Operator Env (UI review 7, I7-8, V7-24); its graph dims itself.
        for (auto* knob : units[0].knobs)
            if (knob != nullptr)
                ampRules.add (*knob, [&p] { return FmOperatorInfo::ampEnvelopeInUse (p); },
                              "unused: every oscillator plays the Operator Env (OP ENV)");

        thumbs.onSelect = [this] (int index) { select (index); };
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

        updateVisibility();
        startTimerHz (4);
    }

    // The bend of an envelope's segments has one name everywhere it is
    // set (UI review 6, I6-22): CURVE, as on the LFO and the matrix.
    static constexpr const char* curveLabel = "CURVE";

    // OP ENV and OP PITCH after ENV 1-16.
    static constexpr int opEnvId = 16;
    static constexpr int opPitchId = 17;

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
        // Two rows when both fit full-size knobs; otherwise one row of all
        // of them, so the dials stay as big as the LFO's rather than
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
                                     unusedAmp ? "unused: every oscillator plays the Operator Env (edit it on OP ENV)"
                                               : "drag the graph or the knobs; the dot on a segment sets its curve",
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

        if (isShowing() && selected < opEnvId && ! envelopeShown (processorRef, selected))
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

    void updateVisibility()
    {
        // A remembered selection can point at an envelope this patch doesn't
        // show: the first card shown instead.
        if (selected < opEnvId && ! envelopeShown (processorRef, selected))
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

        // The MSEG module: every LFO can draw an MSEG now (SHAPE › MSEG:
        // UI review 7, S7-7 / V7-19), so the patch's own four-point one has
        // a card only while something uses it (a route, an oscillator's
        // ENVELOPE) or it was opened from its chip.
        LfoThumbBar::ExtraCard mseg;
        mseg.title = "MSEG";
        mseg.source = Mod::Source::Mseg;
        mseg.colour = MsegEditor::colour();
        mseg.isShown = [this] { return msegOpened || msegInUse(); };
        mseg.tooltip = "MSEG\nThe patch's four-point MSEG, for older patches and as an oscillator's ENVELOPE (once per note). "
                       "Click to edit it below; drag it onto a knob to modulate that knob. Any LFO can be a drawn MSEG too: "
                       "SHAPE \xe2\x80\xba MSEG.";
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
        opLfo.isActive = [&p] { return FmOperatorInfo::anyOperatorEnv (p); };
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
            msegOpened = msegOpened || index == msegId;
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

        // Control panel: the options on a top row, the knobs filling the
        // rest (UI review 7, V7-29 / S7-26: no half-empty panel).
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        constexpr int optionsHeight = 13 + 24 + 14;

        if (selected == msegId)
        {
            msegEditor.setBounds (displayArea);
            // LOOP where an LFO's SYNC is, RATE where its RATE is; a line
            // under them says what plays it.
            auto options = inner.removeFromTop (optionsHeight);
            msegLoop.setBounds (options.withWidth (options.getWidth() / 3).reduced (3, 1));
            inner.removeFromBottom (18);
            layoutRow (inner.withWidth (inner.getWidth() / 3), { &msegRate });
            return;
        }

        if (selected == clockId)
        {
            clockView.setBounds (displayArea);
            auto options = inner.removeFromTop (optionsHeight);
            clockDiv.setBounds (options.removeFromLeft (options.getWidth() / 2).reduced (3, 1));
            return;
        }

        const auto displayIndex = juce::jlimit (0, (int) displays.size() - 1, selected);
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (displayIndex + 1) + "_shape")->load();
        const auto simulated = LfoSimShapes::isSim (shape);
        displays[(size_t) displayIndex]->setBounds (displayArea);
        auto& c = *controlsList[(size_t) displayIndex];

        if (simulated)
        {
            layoutSimulated (c, inner, shape);
            return;
        }

        // SHAPE and its switches on the top row; RATE, START and SMOOTH
        // (and a physics shape's two) filling the rest, centred.
        auto options = inner.removeFromTop (optionsHeight);
        c.shape.setBounds (options.removeFromLeft (options.getWidth() / 2).reduced (3, 1));
        const auto physicsKick = shape == LfoShapes::Pendulum;
        const auto toggleWidth = options.getWidth() / (physicsKick ? 4 : 3);
        c.sync.setBounds (options.removeFromLeft (toggleWidth).reduced (3, 1));
        c.retrig.setBounds (options.removeFromLeft (toggleWidth).reduced (3, 1));
        c.key.setBounds (options.removeFromLeft (toggleWidth).reduced (3, 1));
        if (physicsKick)
            c.kick.setBounds (options.reduced (3, 1));

        inner.removeFromTop (6);
        if (LfoShapes::isPhysics (shape))
            layoutRow (inner, { c.rate.layoutItem(), &c.phase, &c.smooth, &c.physA, &c.physB });
        else
            layoutRow (inner, { c.rate.layoutItem(), &c.phase, &c.smooth });

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
            const auto subtitle = playedBy.isEmpty() ? juce::String ("the patch's four-point MSEG; drag its card onto a knob")
                                                     : "played by: " + playedBy.joinIntoString (", ");
            IlanaTheme::paintCardHeader (g, header, selected == msegId ? "MSEG" : "CLOCKED S&H",
                                         selected == msegId ? subtitle : "a new random value on every step of DIVISION; drives "
                                                                             + playedBy.joinIntoString (", "),
                                         colour, 0);

            // What the MSEG's LOOP means, and where the drawn shapes are.
            if (selected == msegId)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawText (juce::String (read ("mseg_loop") > 0.5f ? "LOOP on: it cycles at RATE" : "LOOP off: it runs once and holds (once per note as an ENVELOPE)")
                                + juce::String (juce::CharPointer_UTF8 (". Any LFO can draw its own: SHAPE \xe2\x80\xba MSEG.")),
                            panel.reduced (14, 6).removeFromBottom (14), juce::Justification::centredLeft, true);
            }
            return;
        }

        const auto colour = lfoColour (selected);
        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));

        const auto* retrig = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_retrig");
        const auto* key = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_key");
        IlanaTheme::paintCardHeader (g, header, "LFO " + juce::String (selected + 1),
                                     key != nullptr && key->load() > 0.5f         ? "per voice, rate follows the note (4 Hz = its pitch)"
                                     : retrig != nullptr && retrig->load() > 0.5f ? "runs per voice, restarts on each note"
                                                                                  : "free-running, shared by all voices",
                                     colour, 0);
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
        msegOpened = msegOpened || selected == msegId;

        if (selected < msegId && ! processorRef.isLfoShown (selected))
        {
            processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, selected, true);
            thumbs.refreshLayout();
        }

        settings.setValue ("lfoSelected", selected);
        updateVisibility();
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
              , axis (state, "lfo" + juce::String (lfo) + "_axis", "OUTPUT A AXIS")
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

    // The MSEG module plays a part: routed, or an oscillator's ENVELOPE or
    // warp envelope.
    bool msegInUse() const
    {
        return ! modSlotsUsing (processorRef, { Mod::Source::Mseg }).empty() || ! envelopeUses (16, 17).isEmpty();
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

    // Simulated shapes: combos and switches stacked on the left, the named
    // knobs (RATE, SMOOTH, the shape's own, STEREO and SEED) in two rows on
    // the right, each row tall enough for a knob, its name and its value
    // (UI review 6, V6-11 / S6-12: the values were drawn over the dials).
    void layoutSimulated (Controls& c, juce::Rectangle<int> inner, int shape)
    {
        const auto& info = LfoSimInfo::get (shape);
        labelSimulated (c, shape);

        // Options on the left: SHAPE, then TRIGGER / OUTPUT, then the
        // switches and FIRE (RATE shows the division while synced).
        auto options = inner.removeFromLeft (inner.getWidth() * 42 / 100);
        inner.removeFromLeft (6);
        const auto rowHeight = juce::jmin (options.getHeight() / 3, 13 + 24 + 14);
        c.shape.setBounds (options.removeFromTop (rowHeight).reduced (3, 1));
        auto combos = options.removeFromTop (rowHeight);
        const auto comboWidth = combos.getWidth() / 2;
        c.trigger.setBounds (combos.removeFromLeft (comboWidth).reduced (3, 1));
        if (info.usesAxis)
            c.axis.setBounds (combos.reduced (3, 1));

        auto switches = options.removeFromTop (rowHeight);
        std::vector<juce::Component*> row { &c.sync, &c.retrig, &c.key };
        if (info.usesLoop)
            row.push_back (&c.loop);
        if (shape == LfoSimShapes::Pendulum)
            row.push_back (&c.kick);
        const auto toggleWidth = switches.getWidth() / ((int) row.size() + 1);
        for (auto* component : row)
            component->setBounds (switches.removeFromLeft (toggleWidth).reduced (2, 1));
        c.fire.setBounds (switches.reduced (2, 1).withTrimmedTop (13).withHeight (24));

        std::vector<juce::Component*> knobs { c.rate.layoutItem(), &c.smooth };
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (info.params[(size_t) param].name != nullptr)
                knobs.push_back (c.sim[(size_t) param].get());
        if (info.usesStereo)
            knobs.push_back (&c.stereo);
        if (info.usesSeed)
            knobs.push_back (&c.seed);

        // Two rows on one grid (a gap between them, so the second row's
        // names don't read as the first row's values); one row when a row
        // couldn't hold a knob's name, dial and value.
        constexpr int rowGap = 8;
        constexpr int smallestKnob = 13 + IlanaTheme::KnobSize::minimum + 16;
        const auto twoRows = (inner.getHeight() - rowGap) / 2 >= smallestKnob;
        const auto perRow = twoRows ? (size_t) juce::jmax (3, ((int) knobs.size() + 1) / 2) : knobs.size();
        std::vector<juce::Component*> first (perRow, nullptr), second (perRow, nullptr);
        for (size_t k = 0; k < knobs.size() && k < perRow * 2; ++k)
            (k < perRow ? first[k] : second[k - perRow]) = knobs[k];

        if (twoRows)
        {
            const auto knobHeight = (inner.getHeight() - rowGap) / 2;
            layoutRow (inner.removeFromTop (knobHeight), first);
            inner.removeFromTop (rowGap);
            layoutRow (inner.removeFromTop (knobHeight), second);
        }
        else
            layoutRow (inner, first);

        c.rate.matchBounds();
    }

    void updateVisibility()
    {
        // A remembered selection can point at a card this patch doesn't
        // show: the first LFO shown instead, else the MSEG.
        if ((selected < msegId && ! processorRef.isLfoShown (selected)) || (selected >= msegId && ! thumbs.isCardInPool (selected)))
        {
            selected = opLfoId;
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (processorRef.isLfoShown (lfo))
                {
                    selected = lfo;
                    break;
                }
        }

        opLfoEditor.setVisible (selected == opLfoId);
        msegEditor.setVisible (selected == msegId);
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
    bool msegOpened = false;
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
