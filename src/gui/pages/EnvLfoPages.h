// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

#include "../PoolIndexRow.h"

namespace
{
class FilterPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit FilterPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          panel1 (p, 1, juce::Colour (0xffc86bff)),
          panel2 (p, 2, juce::Colour (0xff8f9dff)),
          westPanel (p),
          secondTabs ({ "FILTER 2", "WEST" }, { juce::Colour (0xff8f9dff), WestPanel::colour() }, false),
          flow (p),
          balance (p.apvts, "filter_balance", "BALANCE", IlanaTheme::accent(), true),
          resOn (p.apvts, "res_on", "ON"),
          resAmount (p.apvts, "res_amount", "AMOUNT", resonatorColour(), true),
          resDecay (p.apvts, "res_decay", "DECAY", resonatorColour(), true),
          resOffset (p.apvts, "res_offset", "OFFSET", resonatorColour(), true),
          resKeytrack (p.apvts, "res_keytrack", "KEY TRK", resonatorColour(), true),
          bodyType (p.apvts, "body_type", "BODY"),
          bodyMaterial (p.apvts, "body_material", "MATERIAL", resonatorColour(), true),
          bodySize (p.apvts, "body_size", "SIZE", resonatorColour(), true),
          bodyCouplingMode (p.apvts, "body_coupling_mode", "COUPLING"),
          bodyCoupling (p.apvts, "body_coupling", "COUPLE", resonatorColour(), true)
    {
        addChildComponent (westPanel);
        addAndMakeVisible (secondTabs);
        secondTabs.onSelect = [this] (int index)
        {
            panel2.setVisible (index == 0);
            westPanel.setVisible (index == 1);
        };
        // Open on WEST when a patch uses it in Filter 2's place.
        if (const auto* west = p.apvts.getRawParameterValue ("west_on"); west != nullptr && west->load() > 0.5f)
            secondTabs.setSelected (1, false);
        addAll (*this, filterDisplay, panel1, panel2, flow, balance,
                resOn, resAmount, resDecay, resOffset, resKeytrack,
                bodyType, bodyMaterial, bodySize, bodyCouplingMode, bodyCoupling);

        // What doesn't act right now dims (the one rule for every page:
        // UI review 4, V26): BALANCE in serial, the body's knobs while it is
        // off. MATERIAL and SIZE shape the modal bodies only (Classic is the
        // old comb bank); without the body only Strings coupling does
        // anything.
        const auto resonating = effectRules.isOn ("res_on");
        const auto modal = [this, resonating] { return resonating() && readValue ("body_type") > 0.5f; };
        const auto couplingMode = [this] { return juce::roundToInt (readValue ("body_coupling_mode")); };
        effectRules.add (balance, effectRules.isOn ("filters_parallel"), "the filters are in SERIAL");
        for (auto* knob : { &resAmount, &resDecay, &resOffset, &resKeytrack })
            effectRules.add (*knob, resonating, "BODY is off");
        effectRules.add (bodyType, resonating);
        effectRules.add (bodyMaterial, modal, "BODY is off or Classic");
        effectRules.add (bodySize, modal, "BODY is off or Classic");
        effectRules.add (bodyCouplingMode, [resonating, couplingMode] { return resonating() || couplingMode() == 3; });
        effectRules.add (bodyCoupling, [modal, couplingMode] { return couplingMode() == 3 || (couplingMode() != 0 && modal()); },
                         "COUPLING is Off, or needs a modal BODY");
        startTimerHz (8);
    }

    // Sections that aren't modulation sources take the accent, so a source's
    // colour always means that source.
    static juce::Colour resonatorColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintSectionTitle (g, "RESPONSE", { headingX, 12, 700, headingHeight }, "drag the markers to set cutoff and resonance");

        // Signal flow: a card like BODY beside it, the diagram and the
        // BALANCE knob inside; the subtitle says what BALANCE does now.
        {
            const auto* parallel = processorRef.apvts.getRawParameterValue ("filters_parallel");
            const auto active = parallel != nullptr && parallel->load() > 0.5f;
            IlanaTheme::paintCard (g, flowCard.toFloat(), 7.0f, IlanaTheme::Ui::text2.withAlpha (0.2f));
            IlanaTheme::paintCardHeader (g, flowCard.reduced (12, 0).removeFromTop (26), "SIGNAL FLOW",
                                         active ? "parallel: BALANCE mixes F1 and F2" : "serial: F1 into F2 (BALANCE is for parallel)",
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
        area.removeFromTop (headingHeight);

        const auto panelHeight = juce::jlimit (200, 260, area.getHeight() * 9 / 20);
        const auto bottomHeight = juce::jlimit (160, 180, area.getHeight() / 4);
        const auto displayHeight = juce::jmax (90, area.getHeight() - panelHeight - bottomHeight - 16);

        filterDisplay.setBounds (area.removeFromTop (displayHeight));
        area.removeFromTop (8);

        auto panels = area.removeFromTop (panelHeight);
        panel1.setBounds (panels.removeFromLeft ((panels.getWidth() - 10) / 2));
        panels.removeFromLeft (10);
        panel2.setBounds (panels);
        westPanel.setBounds (panels);
        // The FILTER 2 / WEST tabs, top right of that card, left of the
        // slope switch (96 px from the right).
        const auto tabWidth = secondTabs.getIdealWidth();
        secondTabs.setBounds (panels.getRight() - tabWidth - 118, panels.getY() + 5, tabWidth, 18);
        panel2.setVisible (secondTabs.getSelected() == 0);
        westPanel.setVisible (secondTabs.getSelected() == 1);
        secondTabs.toFront (false);

        area.removeFromTop (8);
        auto bottom = area.removeFromTop (bottomHeight);

        // A card level with BODY's, so the titles line up.
        flowCard = bottom.removeFromLeft (bottom.getWidth() / 2 - 5);
        auto flowArea = flowCard.reduced (8, 0).withTrimmedTop (26).withTrimmedBottom (8);
        auto balanceArea = flowArea.removeFromRight (92);
        const auto balanceHeight = preferredControlHeight (&balance, balanceArea.getWidth() - 6);
        balance.setBounds (balanceArea.withSizeKeepingCentre (balanceArea.getWidth(), juce::jmin (balanceArea.getHeight(), balanceHeight)));
        flowArea.removeFromRight (6);
        flow.setBounds (flowArea);

        bottom.removeFromLeft (10);
        resonatorCard = bottom;
        auto resArea = bottom.reduced (8, 0);
        // The on switch at the right of the header, as on the oscillator cards.
        resOn.setBounds (IlanaTheme::cardSwitchBounds (resonatorCard, resonatorCard.getY() + 13));
        resArea.removeFromTop (26);
        resArea.removeFromBottom (4);
        // Menus stacked on the left, the first label on the knobs' label line.
        auto menus = resArea.removeFromLeft (juce::jmin (150, resArea.getWidth() / 5));
        constexpr int menuHeight = 13 + 24 + 6;
        const auto knobBand = preferredControlHeight (&resAmount, resArea.getWidth() / 7 - 6) + 6;
        menus = menus.withTop (resArea.withSizeKeepingCentre (resArea.getWidth(), juce::jmin (resArea.getHeight(), knobBand)).getY() + 3)
                     .withHeight (menuHeight * 2);
        bodyType.setBounds (menus.removeFromTop (menuHeight).reduced (3, 0).withTrimmedBottom (6));
        bodyCouplingMode.setBounds (menus.reduced (3, 0).withTrimmedBottom (6));
        layoutRow (resArea, { &resAmount, &resDecay, &bodyMaterial, &bodySize, &resOffset, &resKeytrack, &bodyCoupling });
    }

private:
    // Balance only acts in parallel; resonator knobs only when it is on.
    void timerCallback() override
    {
        const auto parallelNow = readValue ("filters_parallel") > 0.5f;

        if (parallelNow != wasParallel)
        {
            wasParallel = parallelNow;
            repaint (flowCard);
        }

        effectRules.apply();
    }

    float readValue (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;
    FilterDisplay filterDisplay;
    FilterPanel panel1, panel2;
    WestPanel westPanel;
    CardTabs secondTabs;
    SignalFlow flow;
    KnobControl balance;
    bool wasParallel = false;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    ComboControl bodyType, bodyCouplingMode;
    KnobControl bodyMaterial, bodySize, bodyCoupling;
    juce::Rectangle<int> flowCard, resonatorCard;
    EffectRules effectRules { processorRef };
};

// Scrolls a sideways card bar so the given card is in view.
inline void scrollToCard (juce::Viewport& view, juce::Rectangle<int> card)
{
    if (card.isEmpty())
        return;

    const auto x = view.getViewPositionX();
    if (card.getX() < x)
        view.setViewPosition (card.getX(), 0);
    else if (card.getRight() > x + view.getViewWidth())
        view.setViewPosition (card.getRight() - view.getViewWidth(), 0);
}

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

class EnvSection : public juce::Component
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, []
          {
              std::vector<EnvThumbBar::Env> envs { EnvThumbBar::Env { "AMP ENV", "amp", Mod::Source::AmpEnv, modSourceColour ((int) Mod::Source::AmpEnv) },
                       EnvThumbBar::Env { "FILT ENV", "fe", Mod::Source::FilterEnv, juce::Colour (0xffc86bff) },
                       EnvThumbBar::Env { "FILT 2 ENV", "f2e", Mod::Source::FilterEnv2, juce::Colour (0xff8f9dff) },
                       EnvThumbBar::Env { "MOD ENV", "me", Mod::Source::ModEnv, juce::Colour (0xff8fff3b) },
                       EnvThumbBar::Env { "ENV 5", "e4", Mod::Source::Env4, juce::Colour (0xff5b8cff) } };
              for (int env = 6; env <= 16; ++env)
                  envs.push_back ({ "ENV " + juce::String (env), "env" + juce::String (env),
                                    (Mod::Source) ((int) Mod::Source::Env6 + env - 6), extraColour (env) });
              return envs;
          }()),
          ampDisplay (p, "amp", modSourceColour ((int) Mod::Source::AmpEnv), false),
          feDisplay (p, "fe", juce::Colour (0xffc86bff)),
          f2eDisplay (p, "f2e", juce::Colour (0xff8f9dff)),
          meDisplay (p, "me", juce::Colour (0xff8fff3b)),
          e4Display (p, "e4", juce::Colour (0xff5b8cff)),
          ampA (p.apvts, "amp_attack", "ATTACK", modSourceColour ((int) Mod::Source::AmpEnv), false), ampD (p.apvts, "amp_decay", "DECAY", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampS (p.apvts, "amp_sustain", "SUSTAIN", modSourceColour ((int) Mod::Source::AmpEnv), false), ampR (p.apvts, "amp_release", "RELEASE", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampVel (p.apvts, "amp_velocity", "VEL", modSourceColour ((int) Mod::Source::AmpEnv), false), ampCurve (p.apvts, "amp_curve", "TENSION", modSourceColour ((int) Mod::Source::AmpEnv), false),
          feA (p.apvts, "fe_attack", "ATTACK"), feD (p.apvts, "fe_decay", "DECAY"),
          feS (p.apvts, "fe_sustain", "SUSTAIN"), feR (p.apvts, "fe_release", "RELEASE"),
          feVel (p.apvts, "filter_velocity", "VEL"), feCurve (p.apvts, "fe_curve", "TENSION", juce::Colour (0xffc86bff), false),
          f2A (p.apvts, "f2e_attack", "ATTACK"), f2D (p.apvts, "f2e_decay", "DECAY"),
          f2S (p.apvts, "f2e_sustain", "SUSTAIN"), f2R (p.apvts, "f2e_release", "RELEASE"),
          f2Vel (p.apvts, "f2e_velocity", "VEL", juce::Colour (0xff8f9dff), false),
          f2Curve (p.apvts, "f2e_curve", "TENSION", juce::Colour (0xff8f9dff), false),
          meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
          meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
          meVel (p.apvts, "me_velocity", "VEL", juce::Colour (0xff8fff3b), false),
          meCurve (p.apvts, "me_curve", "TENSION", juce::Colour (0xff8fff3b), false),
          e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
          e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
          e4Vel (p.apvts, "e4_velocity", "VEL", juce::Colour (0xff5b8cff), false),
          e4Curve (p.apvts, "e4_curve", "TENSION", juce::Colour (0xff5b8cff), false)
    {
        // Every envelope one click away, above the cards (which scroll).
        indexRow.stateOf = [this] (int env)
        {
            const auto inUse = thumbs.isEnvelopeInUse (env);
            return PoolIndexRow::State { inUse || processorRef.isRevealed (IlanaSynthAudioProcessor::Module::Envelope, env), inUse };
        };
        indexRow.colourOf = [] (int env) { return colourOf (env); };
        indexRow.nameOf = [] (int env)
        {
            const juce::StringArray titles { "AMP ENV (1)", "FILT ENV (2)", "FILT 2 ENV (3)", "MOD ENV (4)", "ENV 5" };
            return env < 5 ? titles[env] : "ENV " + juce::String (env + 1);
        };
        indexRow.onPick = [this] (int env)
        {
            if (! indexRow.getState (env).shown)
            {
                processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, true);
                thumbs.refreshLayout();
            }
            select (env);
        };
        addAndMakeVisible (indexRow);

        // The cards keep one size and scroll sideways once there are more than five.
        thumbView.setViewedComponent (&thumbs, false);
        thumbView.setScrollBarsShown (false, true);
        thumbView.setScrollBarThickness (6);
        addAndMakeVisible (thumbView);

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
            const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), juce::Colour (0xffc86bff), juce::Colour (0xff8f9dff),
                                           juce::Colour (0xff8fff3b), juce::Colour (0xff5b8cff) };

            for (int env = 0; env < 5; ++env)
                addStageTwoKnobs (p, prefixes[env], colours[env], units[(size_t) env], false);
        }

        for (int env = 6; env <= 16; ++env)
        {
            const auto prefix = "env" + juce::String (env);
            const auto colour = extraColour (env);
            ExtraUnit extra;
            extra.display = std::make_unique<EnvelopeDisplay> (p, prefix, colour);
            addChildComponent (*extra.display);
            const char* const suffixes[] { "attack", "decay", "sustain", "release", "velocity", "curve" };
            const char* const labels[] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VEL", "TENSION" };
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

        selected = juce::jlimit (0, (int) units.size() - 1, settings.getIntValue ("envSelected", 0));

        thumbs.onSelect = [this] (int index)
        {
            selected = index;
            settings.setValue ("envSelected", selected);
            updateVisibility();
        };
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

        updateVisibility();
    }

    void select (int index)
    {
        selected = juce::jlimit (0, (int) units.size() - 1, index);
        settings.setValue ("envSelected", selected);
        updateVisibility();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        indexRow.setBounds (area.removeFromTop (indexRowHeight));
        area.removeFromTop (4);
        thumbs.setViewWidth (area.getWidth());
        const auto thumbWidth = thumbs.getPreferredWidth();
        const auto scrolls = thumbWidth > area.getWidth();
        thumbView.setBounds (area.removeFromTop (48 + (scrolls ? thumbView.getScrollBarThickness() + 2 : 0)));
        thumbs.setSize (thumbWidth, 48);
        area.removeFromTop (8);

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (area.getWidth() * 47 / 100 /* the LFO display above splits at the same place */).reduced (2));
        area.removeFromLeft (8);

        // Same panel shape as the LFOs: heading, then the stage knobs.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        // First row: the ADSR, velocity and tension as before; second row:
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
        if (panel.isEmpty())
            return;

        const juce::StringArray titles { "AMP ENV", "FILT ENV", "FILT 2 ENV", "MOD ENV", "ENV 5" };
        const auto index = juce::jlimit (0, 4, selected);
        const auto colour = colourOf (selected);

        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));
        auto header = panel.reduced (12, 0).withHeight (26);
        IlanaTheme::paintCardHeader (g, header, selected < 5 ? titles[index] : "ENV " + juce::String (selected + 1),
                                     "drag the graph or the knobs", colour);
    }

    // The UI test reaches the row through here.
    PoolIndexRow& getIndexRow() { return indexRow; }
    int getSelected() const { return selected; }

private:
    // ENV 6-16 in their mod source colours.
    static juce::Colour extraColour (int env)
    {
        return modSourceColour ((int) Mod::Source::Env6 + env - 6);
    }

    // ENV 1-16's colours, 0-based.
    static juce::Colour colourOf (int env)
    {
        const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), juce::Colour (0xffc86bff), juce::Colour (0xff8f9dff),
                                       juce::Colour (0xff8fff3b), juce::Colour (0xff5b8cff) };
        return env < 5 ? colours[juce::jlimit (0, 4, env)] : extraColour (env + 1);
    }

    static constexpr int indexRowHeight = 18;

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


    static void layoutFixed (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
    {
        const auto width = juce::jmax (1, area.getWidth() / (int) items.size());

        for (auto* item : items)
        {
            auto cell = area.removeFromLeft (width).reduced (3);

            if (item != nullptr)
                item->setBounds (cell);
        }
    }

    void updateVisibility()
    {
        for (int i = 0; i < (int) units.size(); ++i)
        {
            const auto visible = i == selected;
            units[(size_t) i].display->setVisible (visible);

            for (auto* knob : units[(size_t) i].knobs)
                if (knob != nullptr)
                    knob->setVisible (visible);
        }

        thumbs.setSelected (selected);
        indexRow.setSelected (selected);
        resized();

        scrollToCard (thumbView, thumbs.boundsOfCard (selected));

        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    PoolIndexRow indexRow { 16, "ENV" };
    juce::Viewport thumbView;
    EnvThumbBar thumbs;
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

class LfoSection : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private IlanaAnim::FrameTimer
{
public:
    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, [] (int index) { return lfoColour (index); })
    {
        // Every LFO one click away, above the cards (which scroll).
        indexRow.stateOf = [this] (int lfo) { return PoolIndexRow::State { processorRef.isLfoShown (lfo), thumbs.isLfoRouted (lfo) }; };
        indexRow.colourOf = [] (int lfo) { return lfoColour (lfo); };
        indexRow.nameOf = [] (int lfo) { return "LFO " + juce::String (lfo + 1); };
        indexRow.onPick = [this] (int lfo)
        {
            if (! processorRef.isLfoShown (lfo))
            {
                processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
                thumbs.refreshLayout();
            }
            select (lfo);
        };
        addAndMakeVisible (indexRow);

        // Cards keep one size and scroll sideways past four.
        thumbView.setViewedComponent (&thumbs, false);
        thumbView.setScrollBarsShown (false, true);
        thumbView.setScrollBarThickness (6);
        addAndMakeVisible (thumbView);
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

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
            groupShapeMenu (controls->shape.getComboBox());
            controls->shape.getComboBox().onChange = [this, lfo] { if (userPickingShape) shapePicked (lfo); };
            controls->shape.setPopupOverride ([this, lfo]
            {
                auto& combo = controlsList[(size_t) lfo]->shape.getComboBox();
                juce::PopupMenu menu (*combo.getRootMenu());
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&combo).withMinimumWidth (combo.getWidth())
                                        .withItemThatMustBeVisible (combo.getSelectedId()),
                                    [this, lfo] (int id)
                                    {
                                        if (id <= 0)
                                            return;
                                        userPickingShape = true;
                                        controlsList[(size_t) lfo]->shape.getComboBox().setSelectedId (id, juce::sendNotificationSync);
                                        userPickingShape = false;
                                    });
            });
            controlsList.push_back (std::move (controls));
        }

        selected = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, settings.getIntValue ("lfoSelected", 0));

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

    void resized() override
    {
        auto area = getLocalBounds();

        indexRow.setBounds (area.removeFromTop (18));
        area.removeFromTop (4);
        thumbs.setViewWidth (area.getWidth());
        const auto thumbWidth = thumbs.getPreferredWidth();
        const auto scrolls = thumbWidth > area.getWidth();
        thumbView.setBounds (area.removeFromTop (58 + (scrolls ? thumbView.getScrollBarThickness() + 2 : 0)));
        thumbs.setSize (thumbWidth, 58);
        area.removeFromTop (8);

        const auto displayIndex = juce::jlimit (0, (int) displays.size() - 1, selected);
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (displayIndex + 1) + "_shape")->load();
        const auto simulated = LfoSimShapes::isSim (shape);
        displays[(size_t) displayIndex]->setBounds (area.removeFromLeft (area.getWidth() * 47 / 100).reduced (2)); // the same split for every shape (and as the envelopes below)
        area.removeFromLeft (8);

        // Control panel: options across the top, knobs underneath.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        auto& c = *controlsList[(size_t) displayIndex];

        if (simulated)
        {
            layoutSimulated (c, inner, shape);
            return;
        }

        // Options stacked on the left, the two knobs full height on the right.
        auto options = inner.removeFromLeft (inner.getWidth() / 2);
        const auto rowHeight = options.getHeight() / 3;
        c.shape.setBounds (options.removeFromTop (rowHeight).reduced (3, 1));
        auto toggles = options.removeFromTop (rowHeight);
        const auto toggleWidth = toggles.getWidth() / 3;
        c.sync.setBounds (toggles.removeFromLeft (toggleWidth).reduced (3, 1));
        c.retrig.setBounds (toggles.removeFromLeft (toggleWidth).reduced (3, 1));
        c.key.setBounds (toggles.reduced (3, 1));
        c.kick.setBounds (options.withWidth (options.getWidth() / 3).reduced (3, 1));

        inner.removeFromLeft (8);
        if (LfoShapes::isPhysics (shape))
        {
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            c.rate.setBounds (top.removeFromLeft (top.getWidth() / 3).reduced (2));
            c.phase.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (2));
            c.smooth.setBounds (top.reduced (2));
            c.physA.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (2));
            c.physB.setBounds (inner.reduced (2));
        }
        else
        {
            c.rate.setBounds (inner.removeFromLeft (inner.getWidth() / 3).reduced (3, 0));
            c.phase.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (3, 0));
            c.smooth.setBounds (inner.reduced (3, 0));
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const auto colour = lfoColour (selected);
        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));

        auto header = panel.reduced (12, 0).withHeight (26);
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

    void select (int index)
    {
        selected = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, index);
        settings.setValue ("lfoSelected", selected);
        updateVisibility();
    }

    // The UI test reaches these through here.
    PoolIndexRow& getIndexRow() { return indexRow; }
    int getSelected() const { return selected; }
    LfoRateControl& getRateControl (int lfo) { return controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, lfo)]->rate; }

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

    // The shape menu with section headings. The attachment maps menu
    // positions to the parameter, so the items stay in parameter order.
    static void groupShapeMenu (juce::ComboBox& combo)
    {
        juce::StringArray names;
        for (int i = 0; i < combo.getNumItems(); ++i)
            names.add (combo.getItemText (i));
        const auto selected = combo.getSelectedId();
        combo.clear (juce::dontSendNotification);
        const auto add = [&] (const juce::String& heading, int first, int last)
        {
            combo.addSectionHeading (heading);
            for (int i = first; i <= last && i < names.size(); ++i)
                combo.addItem (names[i], i + 1);
        };
        add ("Waves", 0, LfoShapes::SmoothRandom - 1);
        add ("Classic (M2)", LfoShapes::SmoothRandom, LfoShapes::Friction);
        add ("Random", LfoSimShapes::RandomHold, LfoSimShapes::DrunkWalk);
        add ("Chaos", LfoSimShapes::Lorenz, LfoSimShapes::DoublePendulum);
        add ("Physics", LfoSimShapes::Bounce, LfoSimShapes::Friction);
        combo.setSelectedId (selected, juce::dontSendNotification);
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

    // Simulated shapes: combos and switches on two rows, then a row of
    // named knobs (RATE, SMOOTH, the shape's own, STEREO and SEED).
    void layoutSimulated (Controls& c, juce::Rectangle<int> inner, int shape)
    {
        const auto& info = LfoSimInfo::get (shape);
        labelSimulated (c, shape);

        // Options on the left: SHAPE, then TRIGGER / OUTPUT, then the
        // switches and FIRE (RATE shows the division while synced).
        auto options = inner.removeFromLeft (inner.getWidth() * 45 / 100);
        inner.removeFromLeft (6);
        const auto rowHeight = options.getHeight() / 3;
        c.shape.setBounds (options.removeFromTop (rowHeight).reduced (3, 1));
        auto combos = options.removeFromTop (rowHeight);
        const auto comboWidth = combos.getWidth() / 2;
        c.trigger.setBounds (combos.removeFromLeft (comboWidth).reduced (3, 1));
        if (info.usesAxis)
            c.axis.setBounds (combos.reduced (3, 1));

        std::vector<juce::Component*> row { &c.sync, &c.retrig, &c.key };
        if (info.usesLoop)
            row.push_back (&c.loop);
        if (shape == LfoSimShapes::Pendulum)
            row.push_back (&c.kick);
        const auto toggleWidth = options.getWidth() / ((int) row.size() + 1);
        for (auto* component : row)
            component->setBounds (options.removeFromLeft (toggleWidth).reduced (2, 1));
        c.fire.setBounds (options.reduced (2, 1).withTrimmedTop (13).withHeight (juce::jmin (24, juce::jmax (16, options.getHeight() - 14))));

        // Knobs on the right, two rows of four.
        std::vector<juce::Component*> knobs { c.rate.layoutItem(), &c.smooth };
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (info.params[(size_t) param].name != nullptr)
                knobs.push_back (c.sim[(size_t) param].get());
        if (info.usesStereo)
            knobs.push_back (&c.stereo);
        if (info.usesSeed)
            knobs.push_back (&c.seed);
        // Two rows on one grid (a gap between them, so the second row's
        // names don't read as the first row's values).
        const auto perRow = (size_t) juce::jmax (3, ((int) knobs.size() + 1) / 2);
        constexpr int rowGap = 8;
        const auto knobHeight = (inner.getHeight() - rowGap) / 2;
        std::vector<juce::Component*> first (perRow, nullptr), second (perRow, nullptr);
        for (size_t k = 0; k < knobs.size() && k < perRow * 2; ++k)
            (k < perRow ? first[k] : second[k - perRow]) = knobs[k];
        layoutRow (inner.removeFromTop (knobHeight), first);
        inner.removeFromTop (rowGap);
        layoutRow (inner.removeFromTop (knobHeight), second);
        c.rate.matchBounds();
    }

    void updateVisibility()
    {
        // A remembered selection can point at an LFO this patch doesn't show.
        if (! processorRef.isLfoShown (selected))
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (processorRef.isLfoShown (lfo))
                {
                    selected = lfo;
                    break;
                }

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
            c.phase.setVisible (visible);
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
        indexRow.setSelected (selected);
        resized();
        scrollToCard (thumbView, thumbs.boundsOfCard (selected));
        repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
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
    PoolIndexRow indexRow { IlanaSynthAudioProcessor::numLfos, "LFO" };
    juce::Viewport thumbView;
    LfoThumbBar thumbs;
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    int selected = 0;
    int lastShape = -1;
    bool userPickingShape = false;
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

        const auto lfoHeight = (area.getHeight() - headingHeight - 8) / 2;
        auto lfoArea = area.removeFromTop (lfoHeight);
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
