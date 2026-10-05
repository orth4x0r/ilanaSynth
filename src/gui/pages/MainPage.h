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

        // The oscillators as strips in their order, then one "+ ADD OSC" row
        // and SUB + NOISE (UI review 5, S8; review 6, V19; review 7, V7-4,
        // S7-2: no empty slots): a strip is a picture, MODE and TABLE (or
        // EXCITE), four knobs and the switch; full editing is on OSC.
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            waves[(size_t) osc] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison", prefix + "_spread",
                prefix + "_detune", false, juce::String {}, prefix + "_mode", osc, colours (osc), false);
            waves[(size_t) osc]->setCompact (true);
            oscColumn.addAndMakeVisible (*waves[(size_t) osc]);
            auto strip = std::make_unique<OscStrip>();
            const auto colour = colours (osc);

            // Menus without a label line (their values name them: "Wavetable",
            // "Neuro"), stacked beside the picture; the tooltip says which.
            strip->on = std::make_unique<ToggleControl> (p.apvts, prefix + "_on", "ON");
            strip->mode = std::make_unique<ComboControl> (p.apvts, prefix + "_mode", "");
            strip->excite = std::make_unique<ComboControl> (p.apvts, prefix + "_excite", "");
            groupExciteMenu (*strip->excite);
            strip->table = std::make_unique<ComboControl> (p.apvts, prefix + "_table", "");
            strip->table->setPopupOverride ([this, table = strip->table.get(), id = prefix + "_table", colour]
            {
                TableBrowser::show (processorRef, id, colour, table->getComboBox());
            });
            const auto knob = [&] (const juce::String& suffix, const juce::String& label)
            {
                const auto id = prefix + suffix;

                for (auto& existing : strip->allKnobs)
                    if (existing.first == id + label)
                        return existing.second.get();

                strip->allKnobs.push_back ({ id + label, std::make_unique<KnobControl> (p.apvts, id, label, colour, false) });
                strip->allKnobs.back().second->setSizeRole (IlanaTheme::KnobSize::minimum);
                oscColumn.addChildComponent (*strip->allKnobs.back().second);
                return strip->allKnobs.back().second.get();
            };

            // Four columns in one order for every mode (UI review 7, V7-26,
            // S7-3): the pitch (SEMI, or a wavetable's RATIO or FIXED), LEVEL,
            // then the mode's two main knobs. An operator on the Operator Env
            // shows its LEVEL in dB (the FM card's), the oscillator's own
            // level as TRIM, and FINE (I7-2, I7-19).
            strip->pitchKnobs = { knob ("_semi", "SEMI"), knob ("_ratio", "RATIO"), knob ("_fixed_hz", "FIXED") };
            strip->modeKnobs[0] = { knob ("_level", "LEVEL"), knob ("_frame", "FRAME"), knob ("_unison", "UNISON") };
            strip->modeKnobs[1] = { knob ("_level", "LEVEL"), knob ("_string_decay", "DECAY"), knob ("_string_damp", "DAMP") };
            strip->modeKnobs[2] = { knob ("_level", "LEVEL"), knob ("_sample_start", "START"), knob ("_sample_end", "END") };
            strip->modeKnobs[3] = { knob ("_level", "LEVEL"), knob ("_sample_start", "POSITION"), knob ("_grain_size", "SIZE") };
            // M7.5 Live: the input has no pitch or shape to set.
            strip->modeKnobs[4] = { knob ("_level", "LEVEL"), knob ("_pan", "PAN"), nullptr };
            strip->operatorEnvKnobs = { knob ("_eg_out", "LEVEL"), knob ("_level", "TRIM"), knob ("_fine", "FINE") };

            addAll (oscColumn, *strip->on, *strip->mode, *strip->table);
            oscColumn.addChildComponent (*strip->excite);

            strips.push_back (std::move (strip));
        }

        // One way to add an oscillator: one row after the strips (UI review
        // 6, S33; review 7, V7-4).
        addOscButton.setTooltip ("Add this oscillator, switched on");
        addOscButton.onClick = [this]
        {
            if (const auto next = firstEmptySlot(); next >= 0)
                processorRef.performEdit ("Add OSC " + juce::String (next + 1), [this, next] { processorRef.addOscillator (next); });

            updateStrips();
        };
        oscColumn.addChildComponent (addOscButton);

        // Sub and noise in the last slot: the sub's shape and octave stacked
        // as the oscillators' menus are, its level and the noise.
        // The switch is the sub's alone, and says so; the noise has its
        // own level and colour (V8-14, V8-15).
        subOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "SUB");
        subShape = std::make_unique<ComboControl> (p.apvts, "sub_shape", "");
        subOctave = std::make_unique<ComboControl> (p.apvts, "sub_octave", "");
        subLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB", subColour(), true);
        noiseLevel = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        subLevel->setSizeRole (IlanaTheme::KnobSize::minimum);
        noiseColour = std::make_unique<KnobControl> (p.apvts, "noise_color", "COLOUR", IlanaTheme::Ui::text2, false);
        noiseLevel->setSizeRole (IlanaTheme::KnobSize::minimum);
        noiseColour->setSizeRole (IlanaTheme::KnobSize::minimum);
        addAll (oscColumn, *subOn, *subShape, *subOctave, *subLevel, *noiseLevel, *noiseColour);

        // Right-click an oscillator's title for its menu (switch, remove).
        oscColumn.onClick = [this] (juce::Point<int> point, bool popup)
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (popup && shownStrips[(size_t) osc] && titleArea (oscCards[(size_t) osc]).contains (point))
                    showOscMenu (osc);
        };

        oscColumn.onPaint = [this] (juce::Graphics& g) { paintColumn (g); };
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

        // AMP ENV dims, and says why, while nothing plays it (an operator
        // patch on its Operator EG: UI review 6, V3, I6-2); EDIT OP ENV opens
        // the operator's envelope on FM.
        for (auto* control : { static_cast<juce::Component*> (envSets[0]->display.get()), envSets[0]->items[0].get(),
                               envSets[0]->items[1].get(), envSets[0]->items[2].get(), envSets[0]->items[3].get() })
            effectRules.add (*control, [this] { return shownAmpNote.isEmpty(); }, "nothing plays the amp envelope now");

        opEgButton.setButtonText ("EDIT OP ENV");
        opEgButton.setTooltip ("Open the Operator Env on the FM page");
        opEgButton.onClick = [this]
        {
            if (onEditOperator != nullptr)
                onEditOperator (juce::jmax (0, firstOperatorEg()));
        };
        addChildComponent (opEgButton);

        // LFOs: the cards plus the selected LFO's main controls.
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto set = std::make_unique<ControlSet>();
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_shape", "SHAPE"));
            LfoShapeMenu::apply (static_cast<ComboControl&> (*set->items.back()), p); // MOD's names and grouped list (I7-44)
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
            for (const auto* suffix : { "_mode", "_on", "_tune", "_amp_env" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        startTimerHz (8);
    }

    ~MainPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_tune", "_amp_env" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);
    }

    std::function<void (int)> onEditLfo, onEditEnvelope, onEditOperator;
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

        if (selectedEnv == 0 && ! ampNoteArea.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            IlanaTheme::drawFitted (g, shownAmpNote, ampNoteArea.withTrimmedRight (opEgButton.isVisible() ? 104 : 0).withTrimmedLeft (4),
                              juce::Justification::centredLeft, 2);
        }
        paintCard (g, lfoCard, "LFO", lfoColour (lfoTabs.getSelected()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 10);
        auto left = area.removeFromLeft ((int) ((float) area.getWidth() * 0.54f));
        area.removeFromLeft (10);
        auto right = area;

        // The added oscillators' strips and SUB + NOISE, with one slim
        // "+ ADD OSC" row between them while a slot is free (UI review 7,
        // V7-4, S7-2). A strip's height depends on the window only, not on
        // how many oscillators there are (three fit with the ADD row and
        // SUB + NOISE; past that the column scrolls), so adding one moves
        // nothing (UI review 8, S8-24, S8-8). A switched-off oscillator
        // folds to a slim strip with its switch (V8-17).
        oscView.setBounds (left);
        const auto addRow = firstEmptySlot() >= 0 ? addRowHeight + slotGap : 0;
        const auto slotHeight = juce::jlimit (minSlotHeight, maxSlotHeight, (left.getHeight() - addRowHeight - slotGap * 4) / 4);
        auto columnHeight = 0, lastWholeBottom = 0;
        std::vector<int> cardTops;
        const auto addCard = [&] (int height)
        {
            columnHeight += columnHeight > 0 ? slotGap : 0;
            cardTops.push_back (columnHeight);
            columnHeight += height;
            if (columnHeight <= left.getHeight())
                lastWholeBottom = columnHeight;
        };
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                addCard (isFolded (osc) ? foldedHeight : slotHeight);
        if (addRow > 0)
            addCard (addRowHeight);
        addCard (slotHeight);
        const auto scrolls = columnHeight > left.getHeight();
        // A scrolling column ends its view at a strip's foot, so no strip
        // shows cut in half (V13, S8).
        if (scrolls && lastWholeBottom > left.getHeight() / 2)
            oscView.setBounds (left.withHeight (lastWholeBottom));
        oscView.setSingleStepSizes (16, slotHeight + slotGap);
        // The column's foot is padded so that scrolled to its end the view
        // also starts at a strip's top, and the view snaps to a strip's top.
        const auto viewHeight = oscView.getHeight();
        auto paddedHeight = juce::jmax (columnHeight, left.getHeight());
        auto snappedY = 0;
        if (scrolls)
        {
            const auto lastTop = *std::find_if (cardTops.begin(), cardTops.end(),
                                                [&] (int top) { return columnHeight - top <= viewHeight; });
            paddedHeight = juce::jmax (columnHeight, lastTop + viewHeight);
            for (const auto top : cardTops)
                if (top <= juce::jmin (oscView.getViewPositionY() + slotHeight / 2, lastTop))
                    snappedY = top;
        }
        oscColumn.setSize (left.getWidth() - (scrolls ? oscView.getScrollBarThickness() + 3 : 0), paddedHeight);
        oscView.setViewPosition (0, snappedY);
        auto column = oscColumn.getLocalBounds();
        addOscButton.setVisible (false);
        addRowArea = {};

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            oscCards[(size_t) osc] = {};

            if (shownStrips[(size_t) osc])
            {
                oscCards[(size_t) osc] = column.removeFromTop (isFolded (osc) ? foldedHeight : slotHeight);
                column.removeFromTop (slotGap);
            }

            showStrip (osc);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

        if (const auto next = firstEmptySlot(); next >= 0)
        {
            addRowArea = column.removeFromTop (addRowHeight);
            column.removeFromTop (slotGap);
            addOscButton.setButtonText ("+  ADD OSC " + juce::String (next + 1));
            addOscButton.setBounds (addRowArea.withSizeKeepingCentre (juce::jmin (160, addRowArea.getWidth() - 24), addRowHeight - 8));
            addOscButton.setVisible (true);
        }

        subCard = column.removeFromTop (slotHeight);
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

            // An unused AMP ENV says so under its knobs.
            ampNoteArea = shownAmpNote.isNotEmpty() ? inner.removeFromBottom (28) : juce::Rectangle<int>();
            auto noteArea = ampNoteArea;
            opEgButton.setBounds (noteArea.removeFromRight (100).withSizeKeepingCentre (96, 22));

            for (auto& set : envSets)
            {
                set->display->setBounds (displayArea);
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(), set->items[3].get() });
            }

            updateVisibility();
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
        std::unique_ptr<ComboControl> mode, excite, table;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        // The first column by TUNING (semitones, ratio, fixed Hz); the other
        // three by mode, or the Operator Env's.
        std::array<juce::Component*, 3> pitchKnobs {};
        std::array<std::vector<juce::Component*>, 5> modeKnobs;
        std::vector<juce::Component*> operatorEnvKnobs;
        int shownMode = -1;
        int shownTuning = 0; // a wavetable's TUNING (the pitch knob)
        bool shownOn = true;
        juce::String role;
        bool opEg = false;
        OperatorEnvThumb thumb;

        std::vector<juce::Component*> knobs() const
        {
            const auto mode = juce::jmax (0, shownMode);
            std::vector<juce::Component*> result { mode == 4 ? nullptr : pitchKnobs[(size_t) (mode == 0 ? shownTuning : 0)] };
            const auto& rest = opEg ? operatorEnvKnobs : modeKnobs[(size_t) mode];
            result.insert (result.end(), rest.begin(), rest.end());
            return result;
        }
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

    // Tracks each slot's oscillator (added or not, its mode, its switch, and
    // whether it is an FM operator); a change lays the strips out again.
    void updateStrips()
    {
        auto changed = false;
        lastRevealVersion = processorRef.getRevealVersion();

        for (int index = 0; index < (int) strips.size(); ++index)
        {
            auto& strip = *strips[(size_t) index];
            const auto prefix = OscRole::prefix (index);
            const auto mode = juce::jlimit (0, 4, readInt (prefix + "_mode"));
            const auto on = readInt (prefix + "_on") > 0;
            const auto shown = processorRef.isOscillatorShown (index);
            const auto tuning = OscRole::tuning (processorRef, index);
            const auto role = OscRole::describe (processorRef, index);
            const auto opEg = mode == 0 && OscRole::usesOperatorEg (processorRef, index);

            if (role != strip.role || (opEg && strip.thumb.update (processorRef, index)))
            {
                strip.role = role;
                oscColumn.repaint();
            }

            if (mode != strip.shownMode || shown != shownStrips[(size_t) index] || on != strip.shownOn || tuning != strip.shownTuning
                || opEg != strip.opEg)
            {
                strip.shownMode = mode;
                strip.shownOn = on;
                strip.shownTuning = tuning;
                strip.opEg = opEg;
                shownStrips[(size_t) index] = shown;
                changed = true;
            }
        }

        if (changed)
            resized();
    }

    // A switched-off oscillator's strip folds to its title, what it plays
    // and its switch (UI review 8, V8-17); switching it on unfolds it.
    bool isFolded (int osc) const { return shownStrips[(size_t) osc] && ! strips[(size_t) osc]->shownOn; }

    // What a folded strip plays: "Wavetable · Basic".
    juce::String foldedSummary (int osc) const
    {
        const auto& strip = *strips[(size_t) osc];
        const auto mode = juce::jmax (0, strip.shownMode);
        auto text = strip.mode->getComboBox().getText();
        const auto detail = mode == 0 ? strip.table->getComboBox().getText()
                                       : mode == 1 ? strip.excite->getComboBox().getText() : juce::String();
        if (detail.isNotEmpty())
            text << juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) << detail;
        return text;
    }

    int firstEmptySlot() const
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (! shownStrips[(size_t) osc])
                return osc;

        return -1;
    }

    // A strip shows its mode's controls (an operator's when it is one); a
    // switched-off oscillator's stay in place, dimmed, until it is on.
    void showStrip (int index)
    {
        auto& strip = *strips[(size_t) index];
        const auto shown = shownStrips[(size_t) index];
        const auto mode = juce::jmax (0, strip.shownMode);
        const auto enabled = shown && strip.shownOn;

        for (auto& entry : strip.allKnobs)
            entry.second->setVisible (false);

        if (isFolded (index))
        {
            for (auto* control : { (juce::Component*) strip.mode.get(), (juce::Component*) strip.table.get(),
                                   (juce::Component*) strip.excite.get(), (juce::Component*) &wave (index) })
                control->setVisible (false);
            strip.on->setVisible (true);
            return;
        }

        std::vector<juce::Component*> controls { strip.mode.get(), strip.table.get(), strip.excite.get(), &wave (index) };

        if (strip.opEg)
            strip.thumb.update (processorRef, index);

        for (auto* item : strip.knobs())
            if (item != nullptr)
            {
                item->setVisible (shown);
                controls.push_back (item);
            }

        strip.table->setVisible (shown && mode == 0);
        strip.excite->setVisible (shown && mode == 1);
        strip.mode->setVisible (shown);
        strip.on->setVisible (shown);
        // An operator on the Operator Env shows its envelope instead.
        wave (index).setVisible (shown && ! strip.opEg);

        for (auto* control : controls)
        {
            control->setEnabled (enabled);
            control->setAlpha (enabled ? 1.0f : offAlpha);
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
        opEgButton.setVisible (selectedEnv == 0 && ! ampNoteArea.isEmpty() && firstOperatorEg() >= 0);
        repaint();
    }

    void timerCallback() override
    {
        // (Polled, not only on the reveal version: FM routes have no
        // listener here.)
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

        effectRules.apply();

        // The sub's controls follow its switch; noise has its own level.
        {
            const auto* subSwitch = processorRef.apvts.getRawParameterValue ("subosc_on");
            const auto alpha = subSwitch != nullptr && subSwitch->load() > 0.5f ? 1.0f : IlanaTheme::dimmedAlpha;
            for (auto* control : { static_cast<juce::Component*> (subShape.get()), static_cast<juce::Component*> (subOctave.get()),
                                   static_cast<juce::Component*> (subLevel.get()) })
                if (control->getAlpha() != alpha)
                    control->setAlpha (alpha);
            const auto* noise = processorRef.apvts.getRawParameterValue ("noise_level");
            const auto colourAlpha = noise != nullptr && noise->load() > 0.0005f ? 1.0f : IlanaTheme::dimmedAlpha;
            if (noiseColour->getAlpha() != colourAlpha)
                noiseColour->setAlpha (colourAlpha);
        }

        // AMP ENV says so when nothing plays it (UI review 6, V3, I6-2).
        const auto note = ampEnvUnusedNote();

        if (note != shownAmpNote)
        {
            shownAmpNote = note;
            resized();
            repaint (envCard);
        }
    }

    // Why the amp envelope does nothing now, or empty while something plays
    // it: an oscillator on ENVELOPE "Amp Env", the sub or the noise, or a
    // matrix route from it.
    juce::String ampEnvUnusedNote() const
    {
        if (readInt ("subosc_on") > 0 || (processorRef.apvts.getRawParameterValue ("noise_level") != nullptr
                                          && processorRef.apvts.getRawParameterValue ("noise_level")->load() > 0.0005f))
            return {};

        auto playing = 0, operatorEg = 0;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            if (! processorRef.isOscillatorShown (osc) || readInt (OscRole::prefix (osc) + "_on") == 0)
                continue;

            if (readInt (OscRole::prefix (osc) + "_amp_env") == 0)
                return {};

            ++playing;
            operatorEg += OscRole::usesOperatorEg (processorRef, osc) ? 1 : 0;
        }

        if (playing == 0)
            return {};

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.destination != 0 && (routing.source == Mod::Source::AmpEnv || routing.aux == Mod::Source::AmpEnv))
                return {};
        }

        return operatorEg == playing ? "Not used: the OSCs play their OP ENV"
                                     : "Not used: the OSCs play other envelopes";
    }

    int firstOperatorEg() const
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc) && OscRole::usesOperatorEg (processorRef, osc))
                return osc;

        return -1;
    }

    // The title line's centre on the right column's cards.
    static int titleCentreY (juce::Rectangle<int> card) { return card.getY() + 14; }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint)
    {
        if (card.isEmpty())
            return;

        const auto centreY = titleCentreY (card);
        IlanaTheme::paintCard (g, card.toFloat(), 6.0f, tint);
        IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) centreY }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (title, juce::Rectangle<int> (card.getX() + 24, centreY - 8, 200, 16), juce::Justification::centredLeft);
    }

    WaveDisplay& wave (int index) { return *waves[(size_t) index]; }

    // A strip's title column: the tag and name, then what it is.
    static juce::Rectangle<int> titleArea (juce::Rectangle<int> card) { return card.withWidth (8 + titleWidth); }

    // A strip, left to right: title, picture, the two menus stacked, four
    // knobs, the switch (centred on the strip, at the right like every card's).
    struct StripColumns
    {
        juce::Rectangle<int> picture, menus, knobs;
    };

    static StripColumns stripColumns (juce::Rectangle<int> card)
    {
        auto inner = card.reduced (8, 5);
        inner.removeFromLeft (titleWidth);
        inner.removeFromRight (switchWidth);
        StripColumns columns;
        // A scrolling column's bar comes out of the picture, not the knobs,
        // so a value such as "-30.9 dB" still fits under its knob.
        const auto squeeze = juce::jmax (0, minKnobsWidth - (inner.getWidth() - pictureWidth - 6 - menuWidth - 4));
        columns.picture = inner.removeFromLeft (juce::jmax (pictureWidth - 24, pictureWidth - squeeze));
        inner.removeFromLeft (6);
        columns.menus = inner.removeFromLeft (menuWidth).withSizeKeepingCentre (menuWidth, 24 + 4 + 24);
        inner.removeFromLeft (4);
        columns.knobs = inner;
        return columns;
    }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];

        if (! shownStrips[(size_t) index])
            return;

        strip.on->setBounds (IlanaTheme::cardSwitchBounds (card, card.getCentreY() - 1));

        if (isFolded (index))
            return;

        const auto columns = stripColumns (card);
        wave (index).setBounds (columns.picture);
        auto menus = columns.menus;
        strip.mode->setBounds (menus.removeFromTop (24));
        menus.removeFromTop (4);
        const auto mode = juce::jmax (0, strip.shownMode);

        if (mode == 0)
            strip.table->setBounds (menus);
        else if (mode == 1)
            strip.excite->setBounds (menus);

        layoutRow (columns.knobs, strip.knobs());
    }

    static juce::Colour subColour() { return IlanaTheme::accent(); }

    void layoutSubCard()
    {
        subOn->setBounds (IlanaTheme::cardSwitchBounds (subCard, subCard.getCentreY() - 1));
        const auto columns = stripColumns (subCard);
        auto menus = columns.menus;
        subShape->setBounds (menus.removeFromTop (24));
        menus.removeFromTop (4);
        subOctave->setBounds (menus);
        layoutRow (columns.knobs, { subLevel.get(), noiseLevel.get(), noiseColour.get() }); // three columns: COLOUR needs the width
    }

    // The strips' cards and titles (their controls draw themselves), and
    // the dim empty slots.
    void paintColumn (juce::Graphics& g)
    {
        const auto paintTitle = [&g] (juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, bool lit,
                                      const juce::String& line2, const juce::String& line3)
        {
            auto area = titleArea (card).reduced (0, 6);
            const auto top = area.removeFromTop (16);
            IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) top.getCentreY() }, lit ? tint : tint.withAlpha (0.4f));
            g.setColour (lit ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText (title, top.withTrimmedLeft (24), juce::Justification::centredLeft);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

            for (const auto& line : { line2, line3 })
                if (line.isNotEmpty())
                {
                    area.removeFromTop (2);
                    g.setColour (lit ? tint.interpolatedWith (IlanaTheme::Ui::text2, 0.35f) : IlanaTheme::Ui::text3);
                    g.drawText (line, area.removeFromTop (12).withTrimmedLeft (9), juce::Justification::centredLeft, true);
                }
        };

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto card = oscCards[(size_t) osc];
            const auto tint = OscPage::oscColour (osc);
            const auto name = "OSC " + juce::String (osc + 1);

            if (! shownStrips[(size_t) osc])
                continue;

            const auto& strip = *strips[(size_t) osc];
            IlanaTheme::paintCard (g, card.toFloat(), 6.0f, strip.shownOn ? tint : tint.withAlpha (0.3f));

            if (isFolded (osc))
            {
                // One line: the dimmed title, what it plays, the switch.
                const auto line = card.withSizeKeepingCentre (card.getWidth(), 16);
                IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) line.getCentreY() }, tint.withAlpha (0.4f));
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
                g.drawText (name, line.withX (card.getX() + 24).withWidth (titleWidth - 16), juce::Justification::centredLeft);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                const auto columns = stripColumns (card);
                g.drawText (foldedSummary (osc), line.withLeft (columns.picture.getX()).withRight (columns.knobs.getRight()),
                            juce::Justification::centredLeft, true);
                continue;
            }

            paintTitle (card, name, tint, strip.shownOn, strip.shownOn ? strip.role : juce::String(), {}); // off: the dimming says it (S8-12)

            if (strip.opEg)
            {
                // The picture is the Operator Env, not the wave: say so in
                // its corner (UI review 8, I8-37).
                const auto picture = stripColumns (card).picture;
                strip.thumb.paint (g, picture.toFloat(), tint, strip.shownOn);
                g.setColour (IlanaTheme::Ui::text2.withAlpha (strip.shownOn ? 1.0f : 0.5f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("OP ENV", picture.reduced (7, 5).removeFromTop (12), juce::Justification::topRight);
            }
        }

        // The next oscillator to add: a slim dashed row round its button.
        if (! addRowArea.isEmpty())
        {
            juce::Path outline, dashed;
            outline.addRoundedRectangle (addRowArea.toFloat().reduced (1.0f), 6.0f);
            const float dashes[] { 4.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (IlanaTheme::Ui::line.withAlpha (0.7f));
            g.fillPath (dashed);
        }

        if (! subCard.isEmpty())
        {
            IlanaTheme::paintCard (g, subCard.toFloat(), 6.0f, subColour());
            paintTitle (subCard, "SUB + NOISE", subColour(), true, {}, {});

            // The sub's shape where the oscillators show their picture.
            const auto picture = stripColumns (subCard).picture.toFloat();
            IlanaTheme::paintWell (g, picture, 6.0f);
            const auto shape = readInt ("sub_shape");
            const auto plot = picture.reduced (10.0f, 9.0f);
            juce::Path path;

            for (int i = 0; i <= 64; ++i)
            {
                const auto t = (float) i / 64.0f;
                const auto value = shape == 0 ? std::sin (t * juce::MathConstants<float>::twoPi)
                                              : shape == 1 ? (t < 0.5f ? 1.0f : -1.0f) : 1.0f - 2.0f * t;
                const juce::Point<float> point (plot.getX() + t * plot.getWidth(), plot.getCentreY() - value * plot.getHeight() * 0.45f);

                if (i == 0)
                    path.startNewSubPath (point);
                else
                    path.lineTo (point);
            }

            g.setColour (subColour().withAlpha (readInt ("subosc_on") > 0 ? 0.9f : 0.3f));
            g.strokePath (path, juce::PathStrokeType (1.4f));
        }
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
    std::array<bool, OscillatorIds::count> shownStrips {};
    int lastRevealVersion = -1;
    // The column: the strips and SUB + NOISE, all one height, and the
    // "+ ADD OSC" row.
    static constexpr int slotGap = 6, addRowHeight = 36;
    static constexpr int minSlotHeight = 104, maxSlotHeight = 140, foldedHeight = 40;
    static constexpr int titleWidth = 84, pictureWidth = 84, menuWidth = 112, switchWidth = 46, minKnobsWidth = 190;
    juce::Rectangle<int> addRowArea;
    static constexpr float offAlpha = 0.35f;
    // PLAY's envelope: which one, the envelopes on its tabs, those behind
    // "+N", and the pool last seen.
    int selectedEnv = 0;
    std::vector<int> envTabEnvs { 0 }, envHiddenEnvs;
    unsigned int lastShownEnvs = 0;
    EffectRules effectRules { processorRef };
    juce::Rectangle<int> subCard;
    std::unique_ptr<ToggleControl> subOn;
    std::unique_ptr<ComboControl> subShape, subOctave;
    std::unique_ptr<KnobControl> subLevel, noiseLevel, noiseColour;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waves;
    juce::String shownAmpNote;
    juce::Rectangle<int> ampNoteArea;
    juce::TextButton opEgButton;
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
