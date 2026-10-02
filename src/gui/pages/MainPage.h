// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// The overview: everything needed to shape a basic sound on one screen
// (oscillators, filter 1, amp envelope, LFOs). The other tabs hold the
// detail.
class MainPage : public juce::Component,
                 private juce::Timer,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater
{
public:
    explicit MainPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          lfoThumbs (p, [] (int index) { return lfoColour (index); }),
          filterTabs ({ "F1", "F2" }, { filterColour (0), filterColour (1) }, true),
          envTabs ({ envelopeTitle (0) }, { envColour (0) }, true),
          lfoTabs ({}, {}, true)
    {
        const auto colours = [] (int osc) { return IlanaTheme::oscColour (osc); };

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            waves[(size_t) osc] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison", prefix + "_spread",
                prefix + "_detune", false, juce::String {}, prefix + "_mode", osc, colours (osc), false);
            oscColumn.addAndMakeVisible (*waves[(size_t) osc]);
            auto strip = std::make_unique<OscStrip>();
            const auto colour = colours (osc);

            strip->on = std::make_unique<ToggleControl> (p.apvts, prefix + "_on", "ON");
            strip->mode = std::make_unique<ComboControl> (p.apvts, prefix + "_mode", "MODE");
            strip->excite = std::make_unique<ComboControl> (p.apvts, prefix + "_excite", "EXCITE");
            strip->table = std::make_unique<ComboControl> (p.apvts, prefix + "_table", "TABLE");
            strip->table->setPopupOverride ([this, table = strip->table.get(), id = prefix + "_table", colour]
            {
                TableBrowser::show (processorRef, id, colour, table->getComboBox());
            });
            strip->warp = std::make_unique<ComboControl> (p.apvts, prefix + "_warp", "WARP");
            // One row of knobs per oscillator mode: wavetable, string, sample, granular.
            const auto knob = [&] (const juce::String& suffix, const juce::String& label)
            {
                const auto id = prefix + suffix;

                for (auto& existing : strip->allKnobs)
                    if (existing.first == id + label)
                        return existing.second.get();

                strip->allKnobs.push_back ({ id + label, std::make_unique<KnobControl> (p.apvts, id, label, colour, false) });
                oscColumn.addChildComponent (*strip->allKnobs.back().second);
                return strip->allKnobs.back().second.get();
            };

            // The OSC page's knobs, a subset in the same order and with the
            // same names.
            strip->modeKnobs[0] = { knob ("_frame", "FRAME"), knob ("_warp_amt", "WARP AMT"), knob ("_level", "LEVEL"),
                                    knob ("_semi", "SEMI"), knob ("_unison", "UNISON"), knob ("_detune", "DETUNE") };
            strip->modeKnobs[1] = { knob ("_string_decay", "DECAY"), knob ("_string_damp", "DAMP"),
                                    knob ("_string_sustain", "SUSTAIN"), knob ("_level", "LEVEL"), knob ("_semi", "SEMI"),
                                    knob ("_unison", "UNISON") };
            strip->modeKnobs[2] = { knob ("_sample_start", "START"), knob ("_sample_end", "END"),
                                    knob ("_sample_fade_out", "FADE OUT"), knob ("_level", "LEVEL"), knob ("_semi", "SEMI"),
                                    knob ("_unison", "UNISON") };
            strip->modeKnobs[3] = { knob ("_sample_start", "POSITION"), knob ("_grain_size", "SIZE"),
                                    knob ("_grain_density", "DENSITY"), knob ("_grain_spray", "SPRAY"), knob ("_level", "LEVEL"),
                                    knob ("_semi", "SEMI") };
            // M7.5 Live: the input has no pitch or shape to set.
            strip->modeKnobs[4] = { knob ("_level", "LEVEL"), knob ("_pan", "PAN") };

            // A wavetable's WARP AMT does nothing while WARP is Off, nor DETUNE
            // with one unison voice.
            effectRules.add (*strip->modeKnobs[0][1], effectRules.choiceIsNot (prefix + "_warp", 0), "WARP is Off");
            effectRules.add (*strip->modeKnobs[0][5], effectRules.isAbove (prefix + "_unison", 1.5f), "UNISON is 1");

            addAll (oscColumn, *strip->on, *strip->mode, *strip->table, *strip->warp);
            oscColumn.addChildComponent (*strip->excite);

            // Switching an oscillator on by hand keeps its card open when
            // the column folds cards to fit.
            strip->on->addMouseListener (this, true);

            strips.push_back (std::move (strip));
        }

        // The ADD button sits in a card's header (the PATCH tile's, or the
        // last oscillator's), not as a bar of its own.
        addOscButton.setButtonText ("+  ADD OSC");
        addOscButton.setTooltip ("Add the next oscillator, switched on");
        addOscButton.onClick = [this]
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (! processorRef.isOscillatorShown (osc))
                {
                    processorRef.addOscillator (osc);
                    pinnedCard = osc;
                    break;
                }

            updateStrips();
        };
        oscColumn.addChildComponent (addOscButton);
        oscColumn.addChildComponent (patchFlow);

        // Sub and noise under the oscillators: the rest of the sources, laid
        // out like the filter card (menus stacked left, knobs on the
        // oscillators' knob grid).
        subOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subShape = std::make_unique<ComboControl> (p.apvts, "sub_shape", "SHAPE");
        subOctave = std::make_unique<ComboControl> (p.apvts, "sub_octave", "OCTAVE");
        subLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB LEVEL", subColour(), true);
        noiseLevel = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        addAll (oscColumn, *subOn, *subShape, *subOctave, *subLevel, *noiseLevel);
        oscColumn.addChildComponent (outputView);
        // A folded SUB + NOISE opens on a click (the noise has no switch of
        // its own); an opened one closes again the same way while both are off.
        // A card folded to fit opens on a click on its title line (another
        // one folds instead). Right-click an oscillator's title for its menu.
        oscColumn.onClick = [this] (juce::Point<int> point, bool popup)
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
            {
                const auto& card = oscCards[(size_t) osc];

                if (! shownStrips[(size_t) osc] || ! card.contains (point) || point.y >= card.getY() + 30)
                    continue;

                if (popup)
                    showOscMenu (osc);
                else if (autoFolded[(size_t) osc])
                {
                    pinnedCard = osc;
                    resized();
                }

                return;
            }

            if (popup || ! subCard.contains (point) || point.y >= subCard.getY() + 30 || subOn == nullptr
                || subOn->getBounds().contains (point))
                return;

            if (subAutoFolded)
            {
                pinnedCard = subCardId;
                resized();
            }
            else if (subIsIdle())
            {
                subExpanded = ! subExpanded;
                pinnedCard = subExpanded ? subCardId : pinnedCard;
                resized();
            }
        };

        oscColumn.onPaint = [this] (juce::Graphics& g)
        {
            if (! subCard.isEmpty())
            {
                const auto folded = subFolded || subAutoFolded;
                paintCard (g, subCard, "SUB + NOISE", subColour(), folded);

                if (folded)
                {
                    g.setColour (IlanaTheme::Ui::text3);
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                    g.drawText (subAutoFolded ? "folded to fit  -  click to open" : "OFF  -  switch on, or click to reach the noise",
                                subCard.withTrimmedLeft (110).withHeight (16).withY (titleCentreY (subCard, true) - 8),
                                juce::Justification::centredLeft);
                }
            }

            if (! patchCard.isEmpty())
                paintCard (g, patchCard, "PATCH", IlanaTheme::accent());

            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc])
                {
                    const auto folded = ! isOpen (osc);
                    paintCard (g, oscCards[(size_t) osc], "OSC " + juce::String (osc + 1), OscPage::oscColour (osc), folded);

                    if (folded)
                    {
                        static const char* const modeNames[] { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
                        const juce::String mode (modeNames[juce::jlimit (0, 4, readInt (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode"))]);
                        g.setColour (IlanaTheme::Ui::text3);
                        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive));
                        g.drawText (autoFolded[(size_t) osc] ? mode + "  -  folded to fit  -  click to open"
                                                             : "OFF  -  " + mode + "  -  switch on to edit",
                                    oscCards[(size_t) osc].withTrimmedLeft (80).withHeight (18).withY (titleCentreY (oscCards[(size_t) osc], true) - 9),
                                    juce::Justification::centredLeft);
                    }
                }
        };
        oscView.setViewedComponent (&oscColumn, false);
        oscView.setScrollBarsShown (true, false);
        oscView.setScrollBarThickness (6);
        addAndMakeVisible (oscView);

        // Filters: one set of controls per filter, swapped by the F1/F2 tabs.
        addAndMakeVisible (filterDisplay);

        for (int f = 0; f < 2; ++f)
        {
            const juce::String prefix (f == 0 ? "f1" : "f2");
            auto set = std::make_unique<ControlSet>();
            // TYPE and SLOPE named and grouped as on the FILTER page: the
            // type grid's short names and families, and its 12 / 24 dB pills.
            auto type = std::make_unique<ComboControl> (p.apvts, prefix + "_type", "TYPE");
            {
                // (Same item ids, so the attachment still matches them.)
                auto& box = type->getComboBox();
                const auto shortNames = FilterTypeGrid::shortNames();
                const auto names = FilterType::getNames();
                box.clear (juce::dontSendNotification);
                for (int index = 0; index < names.size(); ++index)
                    box.addItem (index < shortNames.size() ? shortNames[index] : names[index], index + 1);
                box.setSelectedId (readInt (prefix + "_type") + 1, juce::dontSendNotification);
            }
            type->setPopupOverride ([this, combo = &type->getComboBox(), id = prefix + "_type", f]
            {
                showFilterTypeMenu (*combo, id, f);
            });
            set->items.push_back (std::move (type));
            set->items.push_back (std::make_unique<SlopeField> (p.apvts, prefix + "_slope", filterColour (f)));
            effectRules.add (*set->items.back(), [this, f] { return filterHasSlope (f); }, "this filter type has no slope");

            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" },
                                      { "_drive", "DRIVE" }, { "_env", "ENV AMT" }, { "_keytrack", "KEY TRK" } })
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, filterColour (f), false));

            for (auto& item : set->items)
                addChildComponent (*item);

            filterSets.push_back (std::move (set));
        }

        // Envelopes: graph plus ADSR per envelope, swapped by the tabs (one
        // per envelope the MOD page's pool shows: UI review 6, S6-34).
        const char* const envPrefixes[] { "amp", "fe", "f2e", "me", "e4" };

        for (int e = 0; e < 16; ++e)
        {
            const juce::String prefix (e < 5 ? juce::String (envPrefixes[e]) : "env" + juce::String (e + 1));
            auto set = std::make_unique<ControlSet>();
            set->display = std::make_unique<EnvelopeDisplay> (p, prefix, envColour (e), e == 0);

            for (const auto& spec : { std::pair<const char*, const char*> { "_attack", "ATTACK" }, { "_decay", "DECAY" },
                                      { "_sustain", "SUSTAIN" }, { "_release", "RELEASE" } })
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, envColour (e), e == 0));

            addChildComponent (*set->display);

            for (auto& item : set->items)
                addChildComponent (*item);

            envSets.push_back (std::move (set));
        }

        // LFOs: the cards plus the selected LFO's main controls.
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto set = std::make_unique<ControlSet>();
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_shape", "SHAPE"));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_sync", "SYNC"));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_retrig", "RETRIG"));

            for (auto& item : set->items)
                addChildComponent (*item);

            lfoSets.push_back (std::move (set));

            // RATE reads in Hz, or in note values while SYNC is on.
            lfoRates.push_back (std::make_unique<LfoRateControl> (p, lfo, lfoColour (lfo), false));
            lfoRates.back()->addTo (*this);
        }

        lfoThumbs.onSelect = [this] (int index) { lfoTabs.setSelected (index, true); };
        lfoThumbView.setViewedComponent (&lfoThumbs, false);
        lfoThumbView.setScrollBarsShown (false, true);
        lfoThumbView.setScrollBarThickness (6);
        addAndMakeVisible (lfoThumbView);
        lfoThumbs.onLayoutChanged = [this] { resized(); };

        filterTabs.onSelect = [this] (int) { updateVisibility(); };
        envTabs.onSelect = [this] (int index) { envTabPicked (index); };
        lfoTabs.onSelect = [this] (int index)
        {
            lfoThumbs.setSelected (index);
            updateVisibility();
        };

        filterTabs.onOpen = [this] { if (onOpenPage != nullptr) onOpenPage ("FILTER"); };
        envTabs.onOpen = [this] { if (onEditEnvelope != nullptr) onEditEnvelope (selectedEnv); };
        lfoTabs.onOpen = [this] { if (onEditLfo != nullptr) onEditLfo (lfoTabs.getSelected()); };

        addAll (*this, filterTabs, envTabs, lfoTabs);
        refreshEnvTabs();
        updateVisibility();
        updateStrips();

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        startTimerHz (8);
    }

    ~MainPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);
    }

    std::function<void (int)> onEditLfo, onEditEnvelope;
    std::function<void (const juce::String&)> onOpenPage;

    static juce::Colour lfoColour (int index)
    {
        return IlanaSynthAudioProcessor::lfoColour (index);
    }

    static juce::Colour filterColour (int index) { return index == 0 ? juce::Colour (0xffc86bff) : juce::Colour (0xff8f9dff); }

    static juce::Colour envColour (int index) { return EnvSection::colourOf (index); }

    // The envelope PLAY shows (ENV 1-16, 0-based).
    int getSelectedEnvelope() const { return selectedEnv; }

    // PLAY's envelope tabs: the envelopes the MOD page's pool shows, as
    // many as fit the card's header, the selected one always among them,
    // and "+N" for the rest (a menu).
    void refreshEnvTabs()
    {
        std::vector<int> shown;
        for (int env = 0; env < (int) envSets.size(); ++env)
            if (envelopeShown (processorRef, env))
                shown.push_back (env);
        if (shown.empty())
            shown.push_back (0);
        if (std::find (shown.begin(), shown.end(), selectedEnv) == shown.end())
            selectedEnv = shown.front();

        const auto room = envCard.isEmpty() ? 1000 : envCard.getWidth() - 16 - 110 /* the card's title */;
        const auto namesFor = [] (const std::vector<int>& envs, int hidden)
        {
            juce::StringArray names;
            for (const auto env : envs)
                names.add (envelopeTitle (env));
            if (hidden > 0)
                names.add ("+" + juce::String (hidden));
            return names;
        };

        auto tabs = shown;
        auto hidden = 0;
        while (tabs.size() > 1)
        {
            CardTabs probe (namesFor (tabs, hidden), {}, true);
            if (probe.getIdealWidth() <= room)
                break;
            // Drop the last one that isn't selected.
            for (auto it = tabs.rbegin(); it != tabs.rend(); ++it)
                if (*it != selectedEnv)
                {
                    tabs.erase (std::next (it).base());
                    ++hidden;
                    break;
                }
        }

        std::vector<juce::Colour> colours;
        for (const auto env : tabs)
            colours.push_back (envColour (env));
        if (hidden > 0)
            colours.push_back (IlanaTheme::Ui::text3);

        envTabEnvs = tabs;
        envHiddenEnvs.clear();
        for (const auto env : shown)
            if (std::find (tabs.begin(), tabs.end(), env) == tabs.end())
                envHiddenEnvs.push_back (env);

        envTabs.setNames (namesFor (tabs, hidden), colours);
        envTabs.setSelected ((int) (std::find (tabs.begin(), tabs.end(), selectedEnv) - tabs.begin()), false);
    }

    void envTabPicked (int index)
    {
        if (index < (int) envTabEnvs.size())
        {
            selectedEnv = envTabEnvs[(size_t) index];
            updateVisibility();
            return;
        }

        // "+N": the envelopes that didn't fit.
        envTabs.setSelected ((int) (std::find (envTabEnvs.begin(), envTabEnvs.end(), selectedEnv) - envTabEnvs.begin()), false);
        juce::PopupMenu menu;
        for (const auto env : envHiddenEnvs)
            menu.addItem (env + 1, envelopeTitle (env));
        juce::Component::SafePointer<MainPage> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&envTabs), [safeThis] (int id)
        {
            if (safeThis == nullptr || id <= 0)
                return;
            safeThis->selectedEnv = id - 1;
            safeThis->refreshEnvTabs();
            safeThis->resized();
            safeThis->updateVisibility();
        });
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintCard (g, filterCard, "FILTER", filterColour (filterTabs.getSelected()));
        paintCard (g, envCard, "ENVELOPE", envColour (selectedEnv));
        paintCard (g, lfoCard, "LFO", lfoColour (lfoTabs.getSelected()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 10);
        auto left = area.removeFromLeft ((int) ((float) area.getWidth() * 0.54f));
        area.removeFromLeft (10);
        auto right = area;

        // Open cards keep the three-oscillator size (no taller than their
        // controls need); switched-off ones fold to a title line.
        oscView.setBounds (left);
        const auto anyHidden = std::find (shownStrips.begin(), shownStrips.end(), false) != shownStrips.end();
        const auto oscHeight = juce::jmin ((left.getHeight() - 16) / 3, openOscHeight);
        subFolded = subIsIdle() && ! subExpanded;
        updateFolds (left.getHeight(), oscHeight);
        const auto subHeight = subFolded || subAutoFolded ? foldedHeight : subCardHeight;
        const auto columnHeight = neededHeight (oscHeight);

        const auto scrolls = columnHeight > left.getHeight();
        oscColumn.setSize (left.getWidth() - (scrolls ? oscView.getScrollBarThickness() + 3 : 0),
                           juce::jmax (columnHeight, left.getHeight()));
        auto column = oscColumn.getLocalBounds();
        auto lastShown = -1;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            showStrip (osc);

            if (! shownStrips[(size_t) osc])
            {
                oscCards[(size_t) osc] = {};
                continue;
            }

            oscCards[(size_t) osc] = column.removeFromTop (isOpen (osc) ? oscHeight : foldedHeight);
            column.removeFromTop (8);
            layoutStrip (osc, oscCards[(size_t) osc]);
            lastShown = osc;
        }

        // Height the column doesn't need goes to a tile that shows the patch
        // live (the signal flow, clickable as on FILTER, with the ADD button
        // in its header) and, with room to spare, a live output view; the
        // sub card ends level with the LFO card rather than leaving a gap.
        const auto leftover = scrolls ? 0 : juce::jmax (0, column.getHeight() - subHeight);
        const auto showOutput = leftover >= (anyHidden ? patchWithOutputHeight + 8 : 8) + outputMinHeight;
        const auto showPatch = anyHidden && leftover >= patchMinHeight + 8;
        auto tile = juce::Rectangle<int>();
        outputCard = {};

        if (showOutput)
        {
            const auto patchHeight = showPatch ? juce::jlimit (patchWithOutputHeight, patchWithOutputHeight + 30, leftover / 2) : 0;
            tile = column.removeFromTop (patchHeight);
            column.removeFromTop (showPatch ? 8 : 0);
            outputCard = column.removeFromTop (leftover - patchHeight - (showPatch ? 16 : 8));
            column.removeFromTop (8);
        }
        else if (showPatch)
        {
            tile = column.removeFromTop (leftover - 8);
            column.removeFromTop (8);
        }

        patchCard = showPatch ? tile : juce::Rectangle<int>();
        patchFlow.setVisible (showPatch);
        addOscButton.setVisible (anyHidden && (showPatch || lastShown >= 0));

        if (showPatch)
        {
            auto header = tile.reduced (8, 0).withHeight (26).reduced (0, 3);
            addOscButton.setBounds (header.removeFromRight (104));
            patchFlow.setBounds (tile.withTrimmedTop (30).reduced (12, 0).withTrimmedBottom (12));
        }
        else if (lastShown >= 0)
        {
            // In the last oscillator's title line, left of its switch.
            const auto& card = oscCards[(size_t) lastShown];
            const auto centreY = titleCentreY (card, ! isOpen (lastShown));
            const auto switchBounds = IlanaTheme::cardSwitchBounds (card, centreY);
            addOscButton.setBounds (switchBounds.getX() - 96, centreY - 9, 88, 18);
        }

        subCard = column.removeFromTop (subHeight + (showPatch || showOutput ? 0 : leftover));
        outputView.setVisible (! outputCard.isEmpty());

        if (! outputCard.isEmpty())
            outputView.setBounds (outputCard);

        layoutSubCard();
        oscColumn.repaint();

        const auto lfoHeight = juce::jlimit (132, 170, right.getHeight() / 3);
        const auto remaining = right.getHeight() - lfoHeight - 16;
        filterCard = right.removeFromTop ((int) ((float) remaining * 0.53f));
        right.removeFromTop (8);
        envCard = right.removeFromTop (remaining - filterCard.getHeight());
        right.removeFromTop (8);
        lfoCard = right;

        const auto placeTabs = [] (CardTabs& tabs, juce::Rectangle<int> card)
        {
            auto header = card.reduced (8, 0).withHeight (26).reduced (0, 5);
            tabs.setBounds (header.removeFromRight (tabs.getIdealWidth()));
        };

        placeTabs (filterTabs, filterCard);
        refreshEnvTabs();
        placeTabs (envTabs, envCard);
        placeTabs (lfoTabs, lfoCard);

        {
            auto inner = filterCard.reduced (10).withTrimmedTop (18);
            filterDisplay.setBounds (inner.removeFromTop (juce::jmax (50, inner.getHeight() - 96)));
            inner.removeFromTop (4);

            for (auto& set : filterSets)
            {
                auto row = inner;
                auto combos = row.removeFromLeft (104);
                set->items[0]->setBounds (combos.removeFromTop (combos.getHeight() / 2).reduced (0, 1));
                set->items[1]->setBounds (combos.reduced (0, 1));
                layoutRow (row, { set->items[2].get(), set->items[3].get(), set->items[4].get(), set->items[5].get(), set->items[6].get() });
            }
        }

        {
            auto inner = envCard.reduced (10).withTrimmedTop (18);
            auto displayArea = inner.removeFromLeft (juce::jmin (230, inner.getWidth() / 2));
            inner.removeFromLeft (6);

            for (auto& set : envSets)
            {
                set->display->setBounds (displayArea);
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(), set->items[3].get() });
            }
        }

        {
            auto inner = lfoCard.reduced (10).withTrimmedTop (18);
            // The selected LFO's controls: one row, labels above like the
            // filter's and envelope's (a small dial fits the card).
            constexpr int controlRow = 13 + 30 + 16 + 6;
            // (12 px between the cards and the row, so the row's labels
            // don't crowd the cards.)
            auto cards = inner.removeFromTop (juce::jmax (40, inner.getHeight() - controlRow - 8));
            lfoThumbs.setViewWidth (cards.getWidth());
            const auto thumbWidth = lfoThumbs.getPreferredWidth();
            lfoThumbView.setBounds (cards);
            lfoThumbs.setSize (thumbWidth, cards.getHeight() - (thumbWidth > cards.getWidth() ? lfoThumbView.getScrollBarThickness() + 1 : 0));
            inner.removeFromTop (12);

            for (size_t lfo = 0; lfo < lfoSets.size(); ++lfo)
            {
                auto& set = lfoSets[lfo];
                layoutRow (inner, { set->items[0].get(), lfoRates[lfo]->layoutItem(), set->items[1].get(), set->items[2].get() });
                lfoRates[lfo]->matchBounds();
            }
        }
    }

private:
    struct OscStrip
    {
        std::unique_ptr<ToggleControl> on;
        std::unique_ptr<ComboControl> mode, excite, table, warp;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        std::array<std::vector<juce::Component*>, 5> modeKnobs;
        int shownMode = -1;
        bool shownOn = true;
        bool shownOpen = false;
    };

    // SLOPE as on the FILTER page: its 12 dB / 24 dB pills, under a label
    // placed like a menu's.
    struct SlopeField : public juce::Component
    {
        SlopeField (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour colour)
            : pills (state, id, colour)
        {
            label.setText ("SLOPE", juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
            label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (label);
            addAndMakeVisible (pills);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            label.setBounds (area.removeFromTop (13));
            pills.setBounds (area.removeFromTop (24));
        }

        juce::Label label;
        SlopeSwitch pills;
    };

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { updateStrips(); }

    int readInt (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // Tracks the added oscillators, each one's mode and switch; the layout
    // then shows the controls of the open ones.
    void updateStrips()
    {
        auto changed = false;
        lastRevealVersion = processorRef.getRevealVersion();

        for (int index = 0; index < (int) strips.size(); ++index)
        {
            auto& strip = *strips[(size_t) index];
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            const auto mode = juce::jlimit (0, 4, readInt (prefix + "_mode"));
            const auto on = readInt (prefix + "_on") > 0;
            const auto shown = processorRef.isOscillatorShown (index);

            if (mode != strip.shownMode || shown != shownStrips[(size_t) index] || on != strip.shownOn)
            {
                strip.shownMode = mode;
                strip.shownOn = on;
                shownStrips[(size_t) index] = shown;
                changed = true;
            }
        }

        if (changed)
            resized();
    }

    // An oscillator card shows its controls when the oscillator is on and
    // the column hasn't folded it to fit.
    bool isOpen (int osc) const
    {
        return shownStrips[(size_t) osc] && strips[(size_t) osc]->shownOn && ! autoFolded[(size_t) osc];
    }

    void showStrip (int index)
    {
        auto& strip = *strips[(size_t) index];
        const auto open = isOpen (index);
        const auto mode = juce::jmax (0, strip.shownMode);

        for (auto& entry : strip.allKnobs)
            entry.second->setVisible (false);

        for (auto* item : strip.modeKnobs[(size_t) mode])
            item->setVisible (open);

        strip.table->setVisible (open && mode == 0);
        strip.warp->setVisible (open && mode == 0);
        strip.excite->setVisible (open && mode == 1);
        strip.on->setVisible (shownStrips[(size_t) index]);
        strip.mode->setVisible (open);
        wave (index).setVisible (open);
        strip.shownOpen = open;
    }

    // The column's height with the cards as folded now.
    int neededHeight (int oscHeight) const
    {
        auto height = (subFolded || subAutoFolded ? foldedHeight : subCardHeight);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                height += (isOpen (osc) ? oscHeight : foldedHeight) + 8;

        return height;
    }

    // No card is cut mid-knob (UI review 4, V13 and S8): while the column
    // overflows, its lowest open card folds to its title line (SUB + NOISE
    // first, then the oscillators from the last; the first stays open, as
    // does the card last opened by hand). Only when nothing more can fold
    // does the column scroll.
    void updateFolds (int available, int oscHeight)
    {
        autoFolded.fill (false);
        subAutoFolded = false;

        std::vector<int> candidates;

        if (! subFolded && pinnedCard != subCardId)
            candidates.push_back (subCardId);

        auto first = -1;
        for (int osc = 0; osc < OscillatorIds::count && first < 0; ++osc)
            if (shownStrips[(size_t) osc] && strips[(size_t) osc]->shownOn)
                first = osc;

        for (int osc = OscillatorIds::count - 1; osc > first; --osc)
            if (shownStrips[(size_t) osc] && strips[(size_t) osc]->shownOn && pinnedCard != osc)
                candidates.push_back (osc);

        const auto setFolded = [this] (int card, bool folded)
        {
            if (card == subCardId)
                subAutoFolded = folded;
            else
                autoFolded[(size_t) card] = folded;
        };

        std::vector<int> folded;

        for (const auto card : candidates)
        {
            if (neededHeight (oscHeight) <= available)
                break;

            setFolded (card, true);
            folded.push_back (card);
        }

        // A card folded early may fit again in the room a later one left.
        for (auto card = folded.rbegin(); card != folded.rend(); ++card)
        {
            setFolded (*card, false);

            if (neededHeight (oscHeight) > available)
                setFolded (*card, true);
        }
    }

    // An oscillator title's right-click menu (it took the place of the
    // small remove button beside the switch).
    void showOscMenu (int osc)
    {
        const auto numShown = (int) std::count (shownStrips.begin(), shownStrips.end(), true);
        const auto on = strips[(size_t) osc]->shownOn;
        juce::PopupMenu menu;
        menu.addSectionHeader ("OSC " + juce::String (osc + 1));
        menu.addItem (1, on ? "Switch off" : "Switch on");
        menu.addItem (2, "Remove oscillator", numShown > 1);
        juce::Component::SafePointer<MainPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&oscColumn),
                            [safe, osc, on] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;

                                auto& p = safe->processorRef;
                                const auto name = "OSC " + juce::String (osc + 1);

                                if (result == 2)
                                    p.performEdit ("Remove " + name, [&p, osc] { p.removeOscillator (osc); });
                                else if (auto* parameter = p.apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on"))
                                    p.performEdit (name + (on ? " off" : " on"), [parameter, on]
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (on ? 0.0f : 1.0f);
                                        parameter->endChangeGesture();
                                    });

                                if (result == 1 && ! on)
                                    safe->pinnedCard = osc;

                                safe->updateStrips();
                            });
    }

    // PLAY's TYPE list, grouped as the FILTER page's grid: its pages, then
    // its families, under the tiles' names.
    void showFilterTypeMenu (juce::ComboBox& combo, const juce::String& id, int filterIndex)
    {
        auto* parameter = processorRef.apvts.getParameter (id);

        if (parameter == nullptr)
            return;

        const auto current = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
        const auto names = FilterTypeGrid::shortNames();
        juce::PopupMenu menu;

        for (int page = 0; page < FilterTypeGrid::getNumPages(); ++page)
        {
            juce::PopupMenu pageMenu;
            auto holdsCurrent = false;

            for (const auto& family : FilterTypeGrid::getFamilies (page))
            {
                pageMenu.addSectionHeader (family.first);

                for (const auto type : family.second)
                {
                    pageMenu.addItem (type + 1, names[type], true, type == current);
                    holdsCurrent = holdsCurrent || type == current;
                }
            }

            menu.addSubMenu (FilterTypeGrid::getPageName (page), pageMenu, true, nullptr, holdsCurrent);
        }

        juce::Component::SafePointer<MainPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&combo),
                            [safe, parameter, filterIndex] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;

                                safe->processorRef.performEdit ("Filter " + juce::String (filterIndex + 1) + " type", [parameter, result]
                                {
                                    parameter->beginChangeGesture();
                                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                    parameter->endChangeGesture();
                                });
                            });
    }

    // As on the FILTER page: only the classic models have a slope.
    bool filterHasSlope (int filterIndex) const
    {
        const auto type = juce::jlimit (0, FilterType::Count - 1, readInt (filterIndex == 0 ? "f1_type" : "f2_type"));
        return type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
               && FilterType::usesSlope (type);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // A hand on an oscillator's switch: that card stays open if it
        // turns on.
        for (int osc = 0; osc < (int) strips.size(); ++osc)
            if (auto* toggle = strips[(size_t) osc]->on.get();
                toggle != nullptr && (event.eventComponent == toggle || toggle->isParentOf (event.eventComponent)) && ! strips[(size_t) osc]->shownOn)
                pinnedCard = osc;
    }

    struct ControlSet
    {
        std::unique_ptr<EnvelopeDisplay> display;
        std::vector<std::unique_ptr<juce::Component>> items;
    };

    void updateVisibility()
    {
        const auto showSets = [] (std::vector<std::unique_ptr<ControlSet>>& sets, int selected)
        {
            for (int i = 0; i < (int) sets.size(); ++i)
            {
                const auto visible = i == selected;

                if (sets[(size_t) i]->display != nullptr)
                    sets[(size_t) i]->display->setVisible (visible);

                for (auto& item : sets[(size_t) i]->items)
                    item->setVisible (visible);
            }
        };

        showSets (filterSets, filterTabs.getSelected());
        showSets (envSets, selectedEnv);
        showSets (lfoSets, lfoTabs.getSelected());
        for (int lfo = 0; lfo < (int) lfoRates.size(); ++lfo)
            lfoRates[(size_t) lfo]->setShown (lfo == lfoTabs.getSelected());
        repaint();
    }

    void timerCallback() override
    {
        if (processorRef.getRevealVersion() != lastRevealVersion)
            updateStrips();

        // The envelope tabs follow the pool (an envelope added, removed, or
        // put to use by a route or an oscillator).
        {
            auto shownEnvs = 0u;
            for (int env = 0; env < (int) envSets.size(); ++env)
                shownEnvs |= envelopeShown (processorRef, env) ? (1u << env) : 0u;
            if (shownEnvs != lastShownEnvs)
            {
                lastShownEnvs = shownEnvs;
                resized();
                updateVisibility();
            }
        }

        // SUB + NOISE folds to one line while both are off (and opens again the
        // moment either is used).
        if ((subIsIdle() && ! subExpanded) != subFolded)
            resized();

        effectRules.apply();

        // The sub's controls follow its switch; noise has its own level.
        {
            const auto* subSwitch = processorRef.apvts.getRawParameterValue ("subosc_on");
            const auto alpha = subSwitch != nullptr && subSwitch->load() > 0.5f ? 1.0f : IlanaTheme::dimmedAlpha;
            for (auto* control : { static_cast<juce::Component*> (subShape.get()), static_cast<juce::Component*> (subOctave.get()),
                                   static_cast<juce::Component*> (subLevel.get()) })
                if (control->getAlpha() != alpha)
                    control->setAlpha (alpha);
        }
    }

    // The title line's centre: a folded card is just that line, centred;
    // the switch and remove button share it.
    static int titleCentreY (juce::Rectangle<int> card, bool folded = false)
    {
        return folded ? card.getCentreY() : card.getY() + 14;
    }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, bool folded = false)
    {
        if (card.isEmpty())
            return;

        const auto centreY = titleCentreY (card, folded);
        IlanaTheme::paintCard (g, card.toFloat(), 6.0f, tint);
        IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) centreY }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (title, juce::Rectangle<int> (card.getX() + 24, centreY - 8, 200, 16), juce::Justification::centredLeft);
    }

    WaveDisplay& wave (int index) { return *waves[(size_t) index]; }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];
        auto inner = card.reduced (10, 8);
        const auto open = isOpen (index);
        auto title = inner.removeFromTop (18).withY (titleCentreY (card, ! open) - 9);
        strip.on->setBounds (IlanaTheme::cardSwitchBounds (card, title.getCentreY()));
        inner.removeFromTop (2);

        if (! open)
            return;

        wave (index).setBounds (inner.removeFromLeft (juce::jmin (170, inner.getWidth() / 3)));
        inner.removeFromLeft (8);

        auto combos = inner.removeFromTop (40);
        const auto mode = juce::jmax (0, strip.shownMode);
        const auto third = combos.getWidth() / 3;
        strip.mode->setBounds (combos.removeFromLeft (third).reduced (3, 0));

        if (mode == 0)
        {
            strip.table->setBounds (combos.removeFromLeft (third).reduced (3, 0));
            strip.warp->setBounds (combos.reduced (3, 0));
        }
        else if (mode == 1)
        {
            strip.excite->setBounds (combos.removeFromLeft (third).reduced (3, 0));
        }

        layoutRow (inner, strip.modeKnobs[(size_t) mode]);
    }

    static juce::Colour subColour() { return IlanaTheme::accent(); }

    // Sub off and noise at zero: nothing there to show.
    bool subIsIdle() const
    {
        const auto* subSwitch = processorRef.apvts.getRawParameterValue ("subosc_on");
        const auto* noise = processorRef.apvts.getRawParameterValue ("noise_level");
        return subSwitch != nullptr && noise != nullptr && subSwitch->load() < 0.5f && noise->load() < 0.0005f;
    }

    void layoutSubCard()
    {
        auto inner = subCard.reduced (10, 8);
        inner.removeFromTop (18);
        subOn->setBounds (IlanaTheme::cardSwitchBounds (subCard, titleCentreY (subCard, subFolded)));
        inner.removeFromTop (2);

        for (auto* control : { static_cast<juce::Component*> (subShape.get()), static_cast<juce::Component*> (subOctave.get()),
                               static_cast<juce::Component*> (subLevel.get()), static_cast<juce::Component*> (noiseLevel.get()) })
            control->setVisible (! subFolded);

        if (subFolded)
            return;

        // One row across the whole card: the sub's menus and level, and the
        // noise.
        layoutRow (inner, { subShape.get(), subOctave.get(), subLevel.get(), noiseLevel.get() });
    }

    // The oscillator cards scroll inside this column.
    struct Column : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void (juce::Point<int>, bool)> onClick; // where, and whether it is a right-click
        void paint (juce::Graphics& g) override { if (onPaint != nullptr) onPaint (g); }
        void mouseUp (const juce::MouseEvent& event) override
        {
            if (onClick != nullptr && ! event.mouseWasDraggedSinceMouseDown())
                onClick (event.getPosition(), event.mods.isPopupMenu());
        }
    };

    IlanaSynthAudioProcessor& processorRef;
    juce::Viewport oscView;
    Column oscColumn;
    juce::TextButton addOscButton;
    SignalFlow patchFlow { processorRef };
    juce::Rectangle<int> patchCard;
    static constexpr int patchMinHeight = 90;
    std::array<bool, OscillatorIds::count> shownStrips {};
    int lastRevealVersion = -1;
    // PLAY's envelope: which one, the envelopes on its tabs, those behind
    // "+N", and the pool last seen.
    int selectedEnv = 0;
    std::vector<int> envTabEnvs { 0 }, envHiddenEnvs;
    unsigned int lastShownEnvs = 0;
    static constexpr int foldedHeight = 36;
    // An open oscillator card: title, menus and a row of main-size knobs.
    static constexpr int openOscHeight = 8 + 18 + 2 + 40 + 13 + IlanaTheme::KnobSize::main + 16 + 10;
    // Cards the column folded to fit, and the one opened by hand (an
    // oscillator's index, or subCardId), which stays open.
    std::array<bool, OscillatorIds::count> autoFolded {};
    bool subAutoFolded = false;
    static constexpr int subCardId = 100;
    int pinnedCard = -1;
    EffectRules effectRules { processorRef };
    static constexpr int subCardHeight = 8 + 20 + 13 + 58 + 16 + 12;
    juce::Rectangle<int> subCard, outputCard;
    OutputView outputView { processorRef };
    static constexpr int outputMinHeight = 48;
    static constexpr int patchWithOutputHeight = 114; // the signal flow needs this much to keep its rows apart
    bool subFolded = false;   // SUB + NOISE shown as one line (both off)
    bool subExpanded = false; // the user opened the folded card to reach the noise knob
    std::unique_ptr<ToggleControl> subOn;
    std::unique_ptr<ComboControl> subShape, subOctave;
    std::unique_ptr<KnobControl> subLevel, noiseLevel;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waves;
    FilterDisplay filterDisplay;
    juce::Viewport lfoThumbView;
    LfoThumbBar lfoThumbs;
    CardTabs filterTabs, envTabs, lfoTabs;
    std::vector<std::unique_ptr<OscStrip>> strips;
    std::vector<std::unique_ptr<ControlSet>> filterSets, envSets, lfoSets;
    std::vector<std::unique_ptr<LfoRateControl>> lfoRates;
    std::array<juce::Rectangle<int>, OscillatorIds::count> oscCards;
    juce::Rectangle<int> filterCard, envCard, lfoCard;
};
} // namespace
