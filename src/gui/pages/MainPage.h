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
          envTabs ({ "AMP ENV", "FILT ENV", "FILT 2 ENV", "MOD ENV", "ENV 5" },
                   { envColour (0), envColour (1), envColour (2), envColour (3), envColour (4) }, true),
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

            strip->modeKnobs[0] = { knob ("_frame", "FRAME"), knob ("_warp_amt", "WARP"), knob ("_level", "LEVEL"),
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

            strip->remove = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
            strip->remove->setTooltip ("Remove this oscillator (switches it off and hides it)");
            strip->remove->onClick = [this, osc]
            {
                processorRef.removeOscillator (osc);
                updateStrips();
            };

            addAll (oscColumn, *strip->on, *strip->mode, *strip->table, *strip->warp, *strip->remove);
            oscColumn.addChildComponent (*strip->excite);

            strips.push_back (std::move (strip));
        }

        addOscButton.setButtonText ("+  ADD OSCILLATOR");
        addOscButton.setTooltip ("Add the next oscillator, switched on");
        addOscButton.onClick = [this]
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (! processorRef.isOscillatorShown (osc))
                {
                    processorRef.addOscillator (osc);
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

        oscColumn.onPaint = [this] (juce::Graphics& g)
        {
            if (! subCard.isEmpty())
                paintCard (g, subCard, "SUB + NOISE", subColour());

            if (! patchCard.isEmpty())
                paintCard (g, patchCard, "PATCH", IlanaTheme::accent());

            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc])
                {
                    const auto folded = ! strips[(size_t) osc]->shownOn;
                    paintCard (g, oscCards[(size_t) osc], "OSC " + juce::String (osc + 1), OscPage::oscColour (osc), folded);

                    if (folded)
                    {
                        static const char* const modeNames[] { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
                        g.setColour (IlanaTheme::Ui::text3);
                        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                        g.drawText (juce::String ("OFF  -  ") + modeNames[juce::jlimit (0, 4, readInt (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode"))]
                                        + "  -  switch on to edit",
                                    oscCards[(size_t) osc].withTrimmedLeft (80).withHeight (16).withY (titleCentreY (oscCards[(size_t) osc], true) - 8),
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
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_type", "TYPE"));
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_slope", "SLOPE"));

            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" },
                                      { "_drive", "DRIVE" }, { "_env", "ENV AMT" }, { "_keytrack", "KEY TRK" } })
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, filterColour (f), false));

            for (auto& item : set->items)
                addChildComponent (*item);

            filterSets.push_back (std::move (set));
        }

        // Envelopes: graph plus ADSR per envelope, swapped by the tabs.
        const char* const envPrefixes[] { "amp", "fe", "f2e", "me", "e4" };

        for (int e = 0; e < 5; ++e)
        {
            const juce::String prefix (envPrefixes[e]);
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
            set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_rate", "RATE", lfoColour (lfo), false));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_sync", "SYNC"));
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_div", "DIV"));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_retrig", "RETRIG"));

            for (auto& item : set->items)
                addChildComponent (*item);

            lfoSets.push_back (std::move (set));
        }

        lfoThumbs.onSelect = [this] (int index) { lfoTabs.setSelected (index, true); };
        lfoThumbView.setViewedComponent (&lfoThumbs, false);
        lfoThumbView.setScrollBarsShown (false, true);
        lfoThumbView.setScrollBarThickness (6);
        addAndMakeVisible (lfoThumbView);
        lfoThumbs.onLayoutChanged = [this] { resized(); };

        filterTabs.onSelect = [this] (int) { updateVisibility(); };
        envTabs.onSelect = [this] (int) { updateVisibility(); };
        lfoTabs.onSelect = [this] (int index)
        {
            lfoThumbs.setSelected (index);
            updateVisibility();
        };

        filterTabs.onOpen = [this] { if (onOpenPage != nullptr) onOpenPage ("FILTER"); };
        envTabs.onOpen = [this] { if (onEditEnvelope != nullptr) onEditEnvelope (envTabs.getSelected()); };
        lfoTabs.onOpen = [this] { if (onEditLfo != nullptr) onEditLfo (lfoTabs.getSelected()); };

        addAll (*this, filterTabs, envTabs, lfoTabs);
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

    static juce::Colour envColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xffc86bff);
            case 2: return juce::Colour (0xff8f9dff);
            case 3: return juce::Colour (0xff8fff3b);
            case 4: return juce::Colour (0xff5b8cff);
            default: return modSourceColour ((int) Mod::Source::AmpEnv);
        }
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintCard (g, filterCard, "FILTER", filterColour (filterTabs.getSelected()));
        paintCard (g, envCard, "ENVELOPE", envColour (envTabs.getSelected()));
        paintCard (g, lfoCard, "LFO", lfoColour (lfoTabs.getSelected()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 10);
        auto left = area.removeFromLeft ((int) ((float) area.getWidth() * 0.54f));
        area.removeFromLeft (10);
        auto right = area;

        // Cards keep the three-oscillator size; added ones scroll.
        oscView.setBounds (left);
        const auto anyHidden = std::find (shownStrips.begin(), shownStrips.end(), false) != shownStrips.end();

        // Switched-off oscillators fold to a title line.
        auto numOpen = 0, numFolded = 0;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                (strips[(size_t) osc]->shownOn ? numOpen : numFolded) += 1;

        const auto baseHeight = (left.getHeight() - 16) / 3;
        const auto spare = left.getHeight() - (numOpen + numFolded - 1) * 8 - numFolded * foldedHeight
                           - (anyHidden ? addButtonHeight + 8 : 0);
        // Open cards keep the three-card size: taller ones only spread their
        // rows apart.
        const auto oscHeight = baseHeight;
        juce::ignoreUnused (spare);
        auto columnHeight = (anyHidden ? addButtonHeight : -8) + 8 + subCardHeight;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                columnHeight += (strips[(size_t) osc]->shownOn ? oscHeight : foldedHeight) + 8;

        const auto scrolls = columnHeight > left.getHeight();
        oscColumn.setSize (left.getWidth() - (scrolls ? oscView.getScrollBarThickness() + 3 : 0),
                           juce::jmax (columnHeight, left.getHeight()));
        auto column = oscColumn.getLocalBounds();

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            if (! shownStrips[(size_t) osc])
            {
                oscCards[(size_t) osc] = {};
                continue;
            }

            oscCards[(size_t) osc] = column.removeFromTop (strips[(size_t) osc]->shownOn ? oscHeight : foldedHeight);
            column.removeFromTop (8);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

        // Height the column doesn't need goes to the ADD tile (a drop zone
        // for the next oscillator), so the sub card ends level with the LFO
        // card rather than leaving a gap under it.
        const auto leftover = scrolls ? 0 : juce::jmax (0, column.getHeight() - (anyHidden ? addButtonHeight + 8 : 0) - subCardHeight);
        // A tall tile shows the patch live (the signal flow, clickable as on
        // FILTER) with the ADD button in its header; a short one is the button.
        auto tile = column.removeFromTop (anyHidden ? addButtonHeight + leftover : 0);
        const auto showPatch = tile.getHeight() >= patchMinHeight;
        patchCard = showPatch ? tile : juce::Rectangle<int>();
        patchFlow.setVisible (showPatch);

        if (showPatch)
        {
            auto header = tile.reduced (8, 0).withHeight (26).reduced (0, 3);
            addOscButton.setButtonText ("+  ADD OSC");
            addOscButton.setBounds (header.removeFromRight (104));
            patchFlow.setBounds (tile.withTrimmedTop (30).reduced (12, 0).withTrimmedBottom (12));
        }
        else
        {
            addOscButton.setButtonText ("+  ADD OSCILLATOR");
            addOscButton.setBounds (tile);
        }

        addOscButton.setVisible (anyHidden);
        column.removeFromTop (anyHidden ? 8 : 0);
        subCard = column.removeFromTop (subCardHeight + (anyHidden ? 0 : leftover));
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

            for (auto& set : lfoSets)
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(),
                                    set->items[3].get(), set->items[4].get() });
        }
    }

private:
    struct OscStrip
    {
        std::unique_ptr<ToggleControl> on;
        std::unique_ptr<ComboControl> mode, excite, table, warp;
        std::unique_ptr<juce::TextButton> remove;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        std::array<std::vector<juce::Component*>, 5> modeKnobs;
        int shownMode = -1;
        bool shownOn = true;
    };

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { updateStrips(); }

    int readInt (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // Shows the added oscillators, the controls for each one's mode, and dims
    // a switched-off one.
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

                // A switched-off oscillator folds to its title and switch.
                const auto open = shown && on;

                for (auto& entry : strip.allKnobs)
                    entry.second->setVisible (false);

                for (auto* item : strip.modeKnobs[(size_t) mode])
                    item->setVisible (open);

                strip.table->setVisible (open && mode == 0);
                strip.warp->setVisible (open && mode == 0);
                strip.excite->setVisible (open && mode == 1);
                strip.on->setVisible (shown);
                strip.mode->setVisible (open);
                wave (index).setVisible (open);
            }
        }

        // Keep one oscillator on the page.
        const auto numShown = (int) std::count (shownStrips.begin(), shownStrips.end(), true);
        for (int index = 0; index < (int) strips.size(); ++index)
            strips[(size_t) index]->remove->setVisible (shownStrips[(size_t) index] && numShown > 1);

        if (changed)
            resized();
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
        showSets (envSets, envTabs.getSelected());
        showSets (lfoSets, lfoTabs.getSelected());
        repaint();
    }

    // Rate and division trade places with SYNC, like on the full page.
    void timerCallback() override
    {
        if (processorRef.getRevealVersion() != lastRevealVersion)
            updateStrips();

        // The sub's controls follow its switch; noise has its own level.
        {
            const auto* subSwitch = processorRef.apvts.getRawParameterValue ("subosc_on");
            const auto alpha = subSwitch != nullptr && subSwitch->load() > 0.5f ? 1.0f : 0.4f;
            for (auto* control : { static_cast<juce::Component*> (subShape.get()), static_cast<juce::Component*> (subOctave.get()),
                                   static_cast<juce::Component*> (subLevel.get()) })
                if (control->getAlpha() != alpha)
                    control->setAlpha (alpha);
        }

        // (Kept up to date while hidden too, so the page never opens stale.)
        const auto lfo = lfoTabs.getSelected();
        const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_sync");
        const auto synced = sync != nullptr && sync->load() > 0.5f;
        auto& items = lfoSets[(size_t) lfo]->items;

        for (auto [index, active] : { std::pair<int, bool> { 1, ! synced }, { 3, synced } })
        {
            const auto alpha = active ? 1.0f : 0.35f;

            if (items[(size_t) index]->getAlpha() != alpha)
                items[(size_t) index]->setAlpha (alpha);
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
        auto title = inner.removeFromTop (18).withY (titleCentreY (card, ! strip.shownOn) - 9);
        strip.remove->setBounds (title.removeFromRight (22).withSizeKeepingCentre (20, 15));
        title.removeFromRight (6);
        strip.on->setBounds (IlanaTheme::cardSwitchBounds (card, title.getCentreY(), true));
        inner.removeFromTop (2);

        if (! strip.shownOn)
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

    void layoutSubCard()
    {
        auto inner = subCard.reduced (10, 8);
        inner.removeFromTop (18);
        subOn->setBounds (IlanaTheme::cardSwitchBounds (subCard, titleCentreY (subCard)));
        inner.removeFromTop (2);

        // One row across the whole card: the sub's menus and level, and the
        // noise.
        layoutRow (inner, { subShape.get(), subOctave.get(), subLevel.get(), noiseLevel.get() });
    }

    // The oscillator cards scroll inside this column.
    struct Column : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint != nullptr) onPaint (g); }
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
    static constexpr int addButtonHeight = 36;
    static constexpr int foldedHeight = 36;
    static constexpr int subCardHeight = 8 + 20 + 13 + 58 + 16 + 12;
    juce::Rectangle<int> subCard;
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
    std::array<juce::Rectangle<int>, OscillatorIds::count> oscCards;
    juce::Rectangle<int> filterCard, envCard, lfoCard;
};
} // namespace
