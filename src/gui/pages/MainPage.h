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
            strip->on->setSwitchColour (colour); // (the card's switch in its colour, as the sheet's)
            strip->mode = std::make_unique<ComboControl> (p.apvts, prefix + "_mode", "");
            strip->excite = std::make_unique<ComboControl> (p.apvts, prefix + "_excite", "");
            groupExciteMenu (*strip->excite);
            strip->table = std::make_unique<ComboControl> (p.apvts, prefix + "_table", "");
            strip->table->setPopupOverride ([this, table = strip->table.get(), id = prefix + "_table", colour]
            {
                TableBrowser::show (processorRef, id, colour, table->getComboBox());
            });
            strip->warp = std::make_unique<ComboControl> (p.apvts, prefix + "_warp", "");
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
            // follows the FM card's order: RATIO, FINE, its OUTPUT in dB, the
            // oscillator's own LEVEL (I7-2, I7-19; review 8, V8-5; 9, I9-7).
            strip->pitchKnobs = { knob ("_semi", "SEMI"), knob ("_ratio", "RATIO"), knob ("_fixed_hz", "FIXED") };
            strip->modeKnobs[0] = { knob ("_level", "LEVEL"), knob ("_frame", "FRAME") };
            strip->warpKnob = knob ("_warp_amt", "WARP");
            strip->unisonKnob = knob ("_unison", "UNISON");
            strip->detuneKnob = knob ("_detune", "DETUNE");
            strip->panKnob = knob ("_pan", "PAN");
            strip->modeKnobs[1] = { knob ("_level", "LEVEL"), knob ("_string_decay", "DECAY"), knob ("_string_damp", "DAMP") };
            strip->modeKnobs[2] = { knob ("_level", "LEVEL"), knob ("_sample_start", "START"), knob ("_sample_end", "END") };
            strip->modeKnobs[3] = { knob ("_level", "LEVEL"), knob ("_sample_start", "POSITION"), knob ("_grain_size", "SIZE") };
            // M7.5 Live: the input has no pitch or shape to set.
            strip->modeKnobs[4] = { knob ("_level", "LEVEL"), knob ("_pan", "PAN"), nullptr };
            // One level on an operator, OUTPUT; the oscillator's VOICE LEVEL is on OSC (I10-1)
            strip->operatorEnvKnobs = { knob ("_fine", "FINE"), knob ("_eg_out", "OUTPUT") };

            // The slots by role, the same on every strip (review 12, I12-10).
            for (const auto& [combo, tip] : { std::pair<ComboControl*, const char*> { strip->mode.get(), "ENGINE\nWhat this oscillator plays: a wavetable, a string, a sample, grains or the live input." },
                                              { strip->table.get(), "SOURCE\nThe wavetable (or sample) this oscillator plays." },
                                              { strip->excite.get(), "SOURCE\nWhat excites the string: a hammer, a bow, a pluck..." },
                                              { strip->warp.get(), "WARP MODE\nHow the table is bent. Off leaves it as it is." } })
            {
                combo->setTooltip (tip);
                combo->getComboBox().setTooltip (tip);
            }

            addAll (oscColumn, *strip->on, *strip->mode, *strip->table, *strip->warp);
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
        oscColumn.addChildComponent (patchFlow);
        oscColumn.addChildComponent (outputView);

        // Sub and noise in the last slot: the sub's shape and octave stacked
        // as the oscillators' menus are, its level and the noise.
        // The switch is the sub's alone, and says so; the noise has its
        // own level and colour (V8-14, V8-15).
        subOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subShape = std::make_unique<ChoicePills> (p.apvts, "sub_shape", juce::StringArray { "Sine", "Square", "Saw" }, subColour(),
                                                   juce::StringArray { "Sin", "Sqr", "Saw" });
        subOctave = std::make_unique<ChoicePills> (p.apvts, "sub_octave", juce::StringArray { "-1 Oct", "-2 Oct" }, subColour());
        subLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB", subColour(), true);
        noiseLevel = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", noiseTint(), false);
        subLevel->setSizeRole (IlanaTheme::KnobSize::minimum);
        noiseColour = std::make_unique<KnobControl> (p.apvts, "noise_color", "COLOUR", noiseTint(), false);
        noiseLevel->setSizeRole (IlanaTheme::KnobSize::minimum);
        noiseColour->setSizeRole (IlanaTheme::KnobSize::minimum);
        addAll (oscColumn, *subOn, *subShape, *subOctave, *subLevel, *noiseLevel, *noiseColour);

        // Right-click an oscillator's title for its menu (switch, remove).
        oscColumn.onClick = [this] (juce::Point<int> point, bool popup)
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc] && ! isFolded (osc)
                    && (titleArea (oscCards[(size_t) osc]).reduced (0, 6).removeFromTop (16).contains (point) || editLinkArea (oscCards[(size_t) osc]).contains (point)))
                {
                    // Right-click: the menu; click: all of this oscillator's
                    // controls on OSC (review 9, S9-4: PLAY's strip is the
                    // quick copy).
                    if (popup)
                        showOscMenu (osc);
                    else if (onEditOscillator != nullptr)
                        onEditOscillator (osc);
                }
        };

        oscColumn.onHover = [this] (juce::Point<int> point)
        {
            auto now = -1;
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc] && ! isFolded (osc) && editLinkArea (oscCards[(size_t) osc]).contains (point))
                    now = osc;
            if (now != hoverEditLink)
            {
                hoverEditLink = now;
                oscColumn.repaint();
            }
        };
        oscColumn.onPaint = [this] (juce::Graphics& g) { paintColumn (g); };
        oscView.setViewedComponent (&oscColumn, false);
        oscView.setScrollBarsShown (true, false);
        oscView.setScrollBarThickness (6);
        addAndMakeVisible (oscView);

        // Filters: one set of controls per filter, swapped by the F1/F2 tabs.
        addAndMakeVisible (filterDisplay);
        // A DX7 voice has no filter: the response says so (a label over its well).
        filterOffNote.setText ("FILTER OFF\na DX7 voice has none: turn CUTOFF down to bring one in", juce::dontSendNotification);
        filterOffNote.setJustificationType (juce::Justification::centred);
        filterOffNote.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        filterOffNote.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        filterOffNote.setBorderSize ({ 0, 6, 0, 6 });
        filterOffNote.setMinimumHorizontalScale (0.8f);
        filterOffNote.setInterceptsMouseClicks (false, false);
        addChildComponent (filterOffNote);

        for (int f = 0; f < 2; ++f)
        {
            const juce::String prefix (f == 0 ? "f1" : "f2");
            auto set = std::make_unique<ControlSet>();
            // TYPE and SLOPE named and grouped as on the FILTER page: the
            // type grid's short names and families, and its 12 / 24 dB pills.
            auto type = std::make_unique<ComboControl> (p.apvts, prefix + "_type", "");
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
            type->setTooltip ("FILTER TYPE\nThe filter model; the FILTER tab shows them all in a grid.");
            type->getComboBox().setTooltip ("FILTER TYPE\nThe filter model; the FILTER tab shows them all in a grid.");
            type->setPopupOverride ([this, combo = &type->getComboBox(), id = prefix + "_type", f]
            {
                showFilterTypeMenu (*combo, id, f);
            });
            set->items.push_back (std::move (type));
            set->items.push_back (std::make_unique<SlopeField> (p.apvts, prefix + "_slope", filterColour (f)));
            effectRules.add (*set->items.back(), [this, f] { return filterHasSlope (f); }, "this filter type has no slope");

            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" },
                                      { "_drive", "DRIVE" }, { "_env", "ENV AMT" }, { "_keytrack", "KEY TRK" } })
            {
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, filterColour (f), false));
                static_cast<KnobControl&> (*set->items.back()).setSizeRole (IlanaTheme::KnobSize::minimum);
                // A DX7 voice has no filter: its knobs but CUTOFF, which
                // brings one in, dim and say why (review 14, I14-3).
                if (juce::String (spec.first) != "_cutoff")
                    effectRules.add (*set->items.back(), [this] { return ! operatorFilterOff; }, "a DX7 voice has no filter: turn CUTOFF down to bring one in");
            }

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
                static_cast<KnobControl&> (*set->items.back()).setSizeRole (envKnobDial);

            set->display->setSlim (true);
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

        styleJumpLink (opEgButton, "OP ENV");
        opEgButton.setTooltip ("Open the Operator Env's editor. Each operator plays its own copy; it shapes their levels, and AMP ENV is unused.");
        opEgButton.onClick = [this]
        {
            if (onEditOperator != nullptr)
                onEditOperator (juce::jmax (0, firstOperatorEg()));
        };
        addChildComponent (opEgButton);

        // OP ENV, first among the envelope tabs on a patch that plays it
        // (UI review 8, I8-18): its picture, and a link to its one editor.
        opEnvOverview.onClick = [this] { opEgButton.triggerClick(); };
        addChildComponent (opEnvOverview);

        // LFOs: the cards plus the selected LFO's main controls.
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto set = std::make_unique<ControlSet>();
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_shape", "SHAPE"));
            LfoShapeMenu::apply (static_cast<ComboControl&> (*set->items.back()), p); // MOD's names and grouped list (I7-44)
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_sync", "SYNC"));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_retrig", "RETRIG"));
            for (int i = 1; i <= 2; ++i) // (the LFO's switches in its colour, as the sheet's)
                static_cast<ToggleControl&> (*set->items[(size_t) i]).setSwitchColour (IlanaSynthAudioProcessor::lfoColour (lfo));

            for (auto& item : set->items)
                addChildComponent (*item);

            lfoSets.push_back (std::move (set));

            // RATE reads in Hz, or in note values while SYNC is on.
            lfoRates.push_back (std::make_unique<LfoRateSlider> (p, lfo, lfoColour (lfo)));
            lfoRates.back()->addTo (*this);
        }

        lfoThumbs.setFillWidth (true);
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
        // On OP ENV the header's EDIT opens the operator's editor, so no second
        // link sits under the paragraph (review 11, I11-14).
        envTabs.onOpen = [this]
        {
            if (selectedEnv == opEnvTab && onEditOperator != nullptr)
                onEditOperator (juce::jmax (0, firstOperatorEg()));
            else if (onEditEnvelope != nullptr)
                onEditEnvelope (selectedEnv);
        };
        lfoTabs.onOpen = [this] { if (onEditLfo != nullptr) onEditLfo (lfoTabs.getSelected()); };

        addAll (*this, filterTabs, envTabs, lfoTabs);
        refreshEnvTabs();
        updateVisibility();
        updateStrips();

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_tune", "_amp_env" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);
        for (const auto* id : { "subosc_on", "noise_level" })
            processorRef.apvts.addParameterListener (id, this);

        startTimerHz (8);
    }

    ~MainPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_tune", "_amp_env" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);
        for (const auto* id : { "subosc_on", "noise_level" })
            processorRef.apvts.removeParameterListener (id, this);
    }

    std::function<void (int)> onEditLfo, onEditEnvelope, onEditOperator, onEditOscillator;
    std::function<void (const juce::String&)> onOpenPage;

    static juce::Colour lfoColour (int index)
    {
        return IlanaSynthAudioProcessor::lfoColour (index);
    }

    static juce::Colour filterColour (int index) { return index == 0 ? juce::Colour (0xffc86bff) : juce::Colour (0xff8f9dff); }

    static juce::Colour envColour (int index) { return EnvSection::colourOf (index); }

    // The envelope PLAY shows (ENV 1-16, 0-based; opEnvTab for OP ENV).
    int getSelectedEnvelope() const { return selectedEnv; }

    // The OP ENV tab's id, after ENV 1-16 (as the MOD page numbers it).
    static constexpr int opEnvTab = EnvSection::opEnvId;

    static juce::String envTabTitle (int env) { return env == opEnvTab ? juce::String ("OP ENV") : envelopeTitle (env); }

    juce::Colour envTabColour (int env) const
    {
        if (env == opEnvTab)
            return OperatorPool::colour();
        // AMP ENV dims on a voice whose oscillators all play OP ENV.
        if (env == 0 && shownAmpNote.isNotEmpty())
            return IlanaTheme::Ui::text3;
        return envColour (env);
    }

    // PLAY's envelope tabs: the envelopes the MOD page's pool shows, as
    // many as fit the card's header, the selected one always among them,
    // and "+N" for the rest (a menu).
    void refreshEnvTabs()
    {
        std::vector<int> shown;
        if (operatorPoolShown (processorRef))
            shown.push_back (opEnvTab);
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
                names.add (envTabTitle (env));
            if (hidden > 0)
                names.add (juce::String (hidden) + " MORE"); // (as the MOD pool says it: V13-16)
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
            colours.push_back (envTabColour (env));
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
            menu.addItem (env + 1, envTabTitle (env));
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

        // A DX7 voice has no filter: the response says so instead of
        // drawing a flat line (I14-3).
        if (operatorFilterOff)
        {
            const auto well = filterDisplay.getBounds().toFloat();
            IlanaTheme::paintWell (g, well, 6.0f);
        }
        paintCard (g, envCard, "ENVELOPE", envTabColour (selectedEnv));

        // OP ENV: what plays it, beside its picture: each operator's OUTPUT as
        // a bar (V12-13; the sentence on what the Operator Env does is in
        // the tooltip of EDIT OP ENV).
        if (selectedEnv == opEnvTab && ! opEnvNoteArea.isEmpty())
        {
            const auto operators = OperatorPool::operatorsOnEnv (processorRef);
            const auto count = (int) operators.size();
            auto area = opEnvNoteArea;
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            // (Said shorter when the card is narrow: no cut word, V14-17.)
            const auto heading = area.removeFromTop (14);
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            auto sentence = juce::String (count) + (count == 1 ? " OSCILLATOR PLAYS THE OPERATOR ENV" : " OSCILLATORS PLAY THE OPERATOR ENV");
            for (const auto& shorter : { juce::String (count) + (count == 1 ? " OSCILLATOR PLAYS THE OP ENV" : " OSCILLATORS PLAY THE OP ENV"),
                                         juce::String (count) + (count == 1 ? " PLAYS THE OP ENV" : " PLAY THE OP ENV") })
                if (juce::GlyphArrangement::getStringWidthInt (font, sentence) > heading.getWidth())
                    sentence = shorter;
            IlanaTheme::drawFitted (g, sentence, heading, juce::Justification::centredLeft);
            area.removeFromTop (4);
            const auto rowHeight = juce::jmin (18, area.getHeight() / juce::jmax (1, count));
            for (int i = 0; i < count && rowHeight >= 8; ++i)
            {
                const auto osc = operators[(size_t) i];
                const auto* parameter = processorRef.apvts.getParameter (OscRole::prefix (osc) + "_eg_out");
                if (parameter == nullptr)
                    continue;
                auto row = area.removeFromTop (rowHeight);
                g.setColour (OscPage::oscColour (osc));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                g.drawText ("OSC " + juce::String (osc + 1), row.removeFromLeft (46), juce::Justification::centredLeft);
                const auto value = row.removeFromRight (62);
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText (parameter->getCurrentValueAsText(), value, juce::Justification::centredRight);
                const auto bar = row.withSizeKeepingCentre (row.getWidth() - 6, 6).toFloat();
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (OscPage::oscColour (osc).withAlpha (0.85f));
                g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * parameter->getValue())), 3.0f);
            }
        }

        paintCard (g, lfoCard, "LFO", lfoColour (lfoTabs.getSelected()));

        // How the selected LFO runs, in MOD's words (UI review 9, I9-19).
        if (! lfoCard.isEmpty() && shownLfoCaption.isNotEmpty())
        {
            const auto centreY = titleCentreY (lfoCard);
            const auto titleRight = lfoCard.getX() + 24
                                    + juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::body, true), "LFO") + 10;
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, shownLfoCaption, juce::Rectangle<int> (titleRight, centreY - 8, lfoCard.getRight() - 12 - titleRight, 16),
                                    juce::Justification::centredLeft, 1);
        }
    }

    // AMP ENV's note while nothing plays it, over the (dimmed) graph.
    void paintOverChildren (juce::Graphics& g) override
    {
        if (selectedEnv == 0 && ! ampNoteArea.isEmpty())
        {
            const auto room = ampNoteArea.withTrimmedRight (opEgButton.isVisible() ? 104 : 0);
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.8f));
            g.fillRoundedRectangle (ampNoteArea.toFloat(), 4.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            IlanaTheme::drawFitted (g, shownAmpNote, room.withTrimmedLeft (4), juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        // One spacing grid (the approved design): 14 px page gutter, a 10 px
        // gap between all cards in both columns, cards 30 px header and
        // 8 / 10 padding. The left column is 1.2 : 1 against the right.
        auto area = getLocalBounds().reduced (pageGutter, 12);
        const auto leftWidth = (int) ((float) (area.getWidth() - cardGap) * 1.2f / 2.2f + 0.5f);
        auto left = area.removeFromLeft (leftWidth);
        area.removeFromLeft (cardGap);
        auto right = area;

        // The oscillator strips and SUB + NOISE, with one slim "+ ADD OSC"
        // row between them while a slot is free (UI review 5-9). Open strips
        // are 146 px, SUB + NOISE 140, a switched-off one folds to 36, the
        // add row is 28 (the design's fixed heights); a column that is too
        // short (five or six operators) shares its height, and one with room
        // to spare grows its strips a little rather than end in a bare band,
        // the rest going to a live PATCH tile and OUTPUT view.
        oscView.setBounds (left);
        const auto addRow = firstEmptySlot() >= 0 ? addRowHeight + slotGap : 0;
        auto shown = 0, folded = 0;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
            {
                ++shown;
                folded += isFolded (osc) ? 1 : 0;
            }
        const auto cards = shown + (addRow > 0 ? 1 : 0) + 1;
        const auto openOscillators = shown - folded;
        folded += subFolded ? 1 : 0;
        const auto flexible = openOscillators + (subFolded ? 0 : 1); // the open strips and SUB + NOISE
        const auto free = left.getHeight() - folded * foldedHeight - (addRow > 0 ? addRowHeight : 0) - slotGap * (cards - 1);
        const auto wanted = openOscillators * oscHeightDesign + (subFolded ? 0 : subHeightDesign);
        auto oscHeight = oscHeightDesign, subHeight = subHeightDesign;
        if (flexible > 0 && wanted > free)
        {
            oscHeight = subHeight = juce::jmax (minSlotHeight, free / flexible);
        }
        else if (flexible > 0)
        {
            const auto spare = free - wanted - slotGap;
            auto extra = 0;
            if (spare < patchMinHeight)
                extra = (free - wanted) / flexible;
            else if (spare < patchMinHeight + slotGap + outputMinHeight && spare > maxPatchOnlyHeight)
                extra = (spare - maxPatchOnlyHeight) / flexible;
            extra = juce::jlimit (0, maxGrowth, extra);
            oscHeight += extra;
            subHeight += extra;
        }
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
                addCard (isFolded (osc) ? foldedHeight : oscHeight);
        if (addRow > 0)
            addCard (addRowHeight);
        addCard (subFolded ? foldedHeight : subHeight);
        const auto scrolls = columnHeight > left.getHeight();
        // A scrolling column ends its view at a strip's foot, so no strip
        // shows cut in half (V13, S8).
        if (scrolls && lastWholeBottom > left.getHeight() / 2)
            oscView.setBounds (left.withHeight (lastWholeBottom));
        oscView.setSingleStepSizes (16, oscHeight + slotGap);
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
                if (top <= juce::jmin (oscView.getViewPositionY() + oscHeight / 2, lastTop))
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
                oscCards[(size_t) osc] = column.removeFromTop (isFolded (osc) ? foldedHeight : oscHeight);
                column.removeFromTop (slotGap);
            }

            showStrip (osc);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

        if (const auto next = firstEmptySlot(); next >= 0)
        {
            addRowArea = column.removeFromTop (addRowHeight);
            column.removeFromTop (slotGap);
            addOscButton.setLabel ("+  ADD OSC " + juce::String (next + 1));
            addOscButton.setBounds (addRowArea);
            addOscButton.setVisible (true);
        }

        subCard = column.removeFromTop (subFolded ? foldedHeight : subHeight);
        layoutSubCard();

        // Height the strips don't take goes to a live PATCH tile (the signal
        // flow, as on FILTER) and, with room to spare, a live output view, so
        // the left column never ends in dead space.
        patchCard = outputCard = {};
        column.removeFromTop (slotGap);
        const auto spare = scrolls ? 0 : column.getHeight();
        const auto showPatch = spare >= patchMinHeight;
        const auto showOutput = spare >= patchMinHeight + slotGap + outputMinHeight;
        if (showPatch)
        {
            patchCard = column.removeFromTop (showOutput ? juce::jlimit (patchMinHeight, patchMinHeight + 30, spare / 2) : juce::jmin (spare, maxPatchOnlyHeight));
            column.removeFromTop (slotGap);
            if (showOutput)
                outputCard = column.removeFromTop (column.getHeight());
            patchFlow.setBounds (patchCard.withTrimmedTop (cardHeaderHeight).reduced (12, 0).withTrimmedBottom (12));
            outputView.setBounds (outputCard.withTrimmedTop (cardHeaderHeight - 6).reduced (6, 0).withTrimmedBottom (8));
        }
        patchFlow.setVisible (showPatch);
        outputView.setVisible (showOutput);
        oscColumn.repaint();

        // FILTER, ENVELOPE and LFO: three equal cards, a 10 px gap (172 px
        // each at the design's size).
        const auto rightCard = (right.getHeight() - 2 * cardGap) / 3;
        filterCard = right.removeFromTop (rightCard);
        right.removeFromTop (cardGap);
        envCard = right.removeFromTop (rightCard);
        right.removeFromTop (cardGap);
        lfoCard = right;

        // The tabs sit in the 30 px header, 22 px pills level with its title.
        const auto placeTabs = [] (CardTabs& tabs, juce::Rectangle<int> card)
        {
            auto header = card.withHeight (cardHeaderHeight).reduced (10, 4);
            tabs.setBounds (header.removeFromRight (tabs.getIdealWidth()));
        };

        placeTabs (filterTabs, filterCard);
        refreshEnvTabs();
        placeTabs (envTabs, envCard);
        placeTabs (lfoTabs, lfoCard);

        // FILTER: the response on the left at the card's full height, the
        // controls in a 3 x 2 grid on the right: CUTOFF, RESO, DRIVE, ENV AMT,
        // KEY TRK, then TYPE with the slope pills.
        {
            auto inner = filterCard.withTrimmedTop (cardHeaderHeight).reduced (cardPadX, cardPadY);
            filterDisplay.setBounds (inner.removeFromLeft (filterDisplayWidth));
            filterOffNote.setBounds (filterDisplay.getBounds().reduced (4, 20));
            inner.removeFromLeft (12);
            const auto cellWidth = inner.getWidth() / 3, cellHeight = inner.getHeight() / 2;
            const auto cell = [&] (int index) { return juce::Rectangle<int> (inner.getX() + (index % 3) * cellWidth, inner.getY() + (index / 3) * cellHeight,
                                                                           cellWidth, cellHeight); };

            for (auto& set : filterSets)
            {
                for (int knob = 0; knob < 5; ++knob)
                    set->items[(size_t) knob + 2]->setBounds (cell (knob).reduced (2, 0));

                const auto slot = cell (5);
                const auto width = juce::jmin (slot.getWidth() - 4, 96);
                const auto stack = slot.withSizeKeepingCentre (width, 24 + 6 + 22);
                set->items[0]->setBounds (stack.withHeight (24));
                set->items[1]->setBounds (stack.withTrimmedTop (30));
            }
        }

        // ENVELOPE: the graph across the card's full width, the four knobs in
        // one evenly spaced row under it.
        {
            auto inner = envCard.withTrimmedTop (cardHeaderHeight).reduced (cardPadX, cardPadY);
            // OP ENV: the picture takes the height its operators' bars leave.
            auto graphHeight = envGraphHeight;
            if (selectedEnv == opEnvTab)
            {
                const auto operators = (int) OperatorPool::operatorsOnEnv (processorRef).size();
                graphHeight = juce::jlimit (36, 100, inner.getHeight() - 8 - (18 + 12 * juce::jmax (1, operators)));
            }
            const auto displayArea = inner.removeFromTop (graphHeight);
            inner.removeFromTop (8);
            ampNoteArea = shownAmpNote.isNotEmpty() ? displayArea.withTrimmedTop (displayArea.getHeight() - 22).reduced (6, 2) : juce::Rectangle<int>();
            opEgButton.setBounds (ampNoteArea.removeFromRight (100).withSizeKeepingCentre (96, 20));

            for (auto& set : envSets)
            {
                set->display->setBounds (displayArea);
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(), set->items[3].get() });
            }

            // OP ENV: its picture where the graph is, a line on what plays
            // it under it, and EDIT OP ENV in the header.
            opEnvOverview.setBounds (displayArea);
            opEnvNoteArea = {};
            if (selectedEnv == opEnvTab)
                opEnvNoteArea = inner.reduced (4, 0);

            updateVisibility();
        }

        // LFO: three thumbnails (filled waves, live phase dot, route chip, the
        // selected one lit) over ONE row on a single baseline: SHAPE, RATE,
        // SYNC and RETRIG, each named above its control.
        {
            auto inner = lfoCard.withTrimmedTop (cardHeaderHeight).reduced (cardPadX, cardPadY);
            const auto cards = inner.removeFromTop (lfoThumbHeight);
            lfoThumbs.setViewWidth (cards.getWidth());
            const auto thumbWidth = lfoThumbs.getPreferredWidth();
            lfoThumbView.setBounds (cards);
            lfoThumbs.setSize (thumbWidth, cards.getHeight() - (thumbWidth > cards.getWidth() ? lfoThumbView.getScrollBarThickness() + 1 : 0));
            inner.removeFromTop (8);

            auto row = inner.removeFromBottom (13 + 24);
            for (size_t lfo = 0; lfo < lfoSets.size(); ++lfo)
            {
                auto& set = lfoSets[lfo];
                auto cells = row;
                const auto shape = cells.removeFromLeft (juce::jmin (132, cells.getWidth() / 3));
                cells.removeFromLeft (16);
                const auto retrig = cells.removeFromRight (64);
                cells.removeFromRight (16);
                const auto sync = cells.removeFromRight (56);
                cells.removeFromRight (16);
                set->items[0]->setBounds (shape);
                lfoRates[lfo]->setBounds (cells);
                set->items[1]->setBounds (sync);
                set->items[2]->setBounds (retrig);
            }
        }
    }

private:
    struct OscStrip
    {
        std::unique_ptr<ToggleControl> on;
        std::unique_ptr<ComboControl> mode, excite, table, warp;
        juce::Component *warpKnob = nullptr, *detuneKnob = nullptr, *unisonKnob = nullptr, *panKnob = nullptr;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        // The first column by TUNING (semitones, ratio, fixed Hz); the other
        // three by mode, or the Operator Env's.
        std::array<juce::Component*, 3> pitchKnobs {};
        std::array<std::vector<juce::Component*>, 5> modeKnobs;
        std::vector<juce::Component*> operatorEnvKnobs;
        int shownMode = -1;
        int shownTuning = 0; // a wavetable's TUNING (the pitch knob)
        bool shownOn = true;
        juce::String role, tableTip;
        bool opEg = false;
        OperatorEnvThumb thumb;

        // Six columns on every card, in one order whatever the mode, so a
        // physical, sample, granular or live oscillator's knobs stand in the
        // wavetable's columns (ilana's PC test: a second mode "regressed to
        // the previous design" with four knobs spread over the row): the
        // pitch, LEVEL, the mode's two main knobs, UNISON and DETUNE. A null
        // is an empty column. An operator on the Operator Env keeps the FM
        // card's order (RATIO, FINE, OUTPUT; I7-2, V8-5) and its PAN.
        // Every strip has its six columns at any height (V13-4): five or six
        // strips shrink the picture, not the controls.
        std::vector<juce::Component*> knobs() const
        {
            const auto mode = juce::jmax (0, shownMode);
            auto* pitch = pitchKnobs[(size_t) (mode == 0 ? shownTuning : 0)];

            if (opEg)
                return { pitch, operatorEnvKnobs[0], operatorEnvKnobs[1], panKnob, nullptr, nullptr };
            if (mode == 4)
                return { nullptr, modeKnobs[4][0], modeKnobs[4][1], nullptr, nullptr, nullptr };

            const auto& own = modeKnobs[(size_t) mode];
            // (A wavetable's third knob is WARP, after FRAME.)
            return { pitch, own[0], own[1], mode == 0 ? warpKnob : own[2], unisonKnob, detuneKnob };
        }
    };

    // A short choice as pills (SUB's shape and octave), as SLOPE's two: the
    // options all in view, one click each, no list to open.
    class ChoicePills : public ParamBoundComponent
    {
    public:
        ChoicePills (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray labelsIn, juce::Colour colourIn,
                     juce::StringArray shortLabelsIn = {})
            : ParamBoundComponent (state, id), labels (std::move (labelsIn)), shortLabels (std::move (shortLabelsIn)), colour (colourIn)
        {
            setTooltip (state.getParameter (id)->getName (40));
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const auto width = bounds.getWidth() / (float) labels.size();

            // A narrow pill (a low card) says it shorter rather than cut.
            const auto fits = [&] (const juce::String& text) { return juce::GlyphArrangement::getStringWidth (IlanaTheme::pillFont(), text) <= width - 10.0f; };
            auto shown = labels;
            if (shortLabels.size() == labels.size() && ! std::all_of (labels.begin(), labels.end(), fits))
                shown = shortLabels;

            for (int option = 0; option < labels.size(); ++option)
                IlanaTheme::paintPill (g, bounds.withWidth (width).withX (bounds.getX() + width * (float) option).reduced (2.0f, 1.0f),
                                       shown[option], colour, option == current);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            setValue (juce::jlimit (0, labels.size() - 1, (int) (event.position.x / (float) getWidth() * (float) labels.size())));
        }

    private:
        juce::StringArray labels, shortLabels;
        juce::Colour colour;
    };

    // SLOPE as on the FILTER page: its 12 dB / 24 dB pills, under a label
    // placed like a menu's.
    struct SlopeField : public juce::Component
    {
        SlopeField (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour colour)
            : pills (state, id, colour)
        {
            pills.setTooltip ("SLOPE\n12 or 24 dB per octave.");
            addAndMakeVisible (pills);
        }

        void resized() override
        {
            pills.setBounds (getLocalBounds());
        }

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
            const auto role = OscRole::roleLine (processorRef, index);
            const auto opEg = mode == 0 && OscRole::usesOperatorEg (processorRef, index);

            // A modulator's OUTPUT is a depth (I12-3).
            if (auto* outKnob = dynamic_cast<KnobControl*> (strip.operatorEnvKnobs[1]))
                if (const auto name = juce::String (OscRole::outputKnobName (processorRef, index)); outKnob->getLabelText() != name)
                    outKnob->setLabelText (name);

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

        // SUB + NOISE folds like a switched-off oscillator while the sub is off
        // and there is no noise (as on OSC; V12-7).
        const auto* noise = processorRef.apvts.getRawParameterValue ("noise_level");
        const auto subFold = readInt ("subosc_on") == 0 && (noise == nullptr || noise->load() < 0.0005f);
        if (subFold != subFolded)
        {
            subFolded = subFold;
            changed = true;
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
                                   (juce::Component*) strip.excite.get(), (juce::Component*) strip.warp.get(), (juce::Component*) &wave (index) })
                control->setVisible (false);
            strip.on->setVisible (true);
            return;
        }

        std::vector<juce::Component*> controls { strip.mode.get(), strip.table.get(), strip.excite.get(), strip.warp.get(), &wave (index) };

        if (strip.opEg)
            strip.thumb.update (processorRef, index);

        for (auto* item : strip.knobs())
            if (item != nullptr)
            {
                item->setVisible (shown);
                controls.push_back (item);
            }

        strip.table->setVisible (shown && mode == 0);
        strip.warp->setVisible (shown && mode == 0 && ! strip.opEg && roomy (oscCards[(size_t) index]));
        strip.excite->setVisible (shown && mode == 1);
        // An operator names itself where the MODE menu goes (its mode is
        // on OSC): no "Wavetable" on a DX7 voice (UI review 8, S8-9, V8-16).
        strip.mode->setVisible (shown && ! strip.opEg);
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
        opEnvOverview.setVisible (selectedEnv == opEnvTab);
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
            auto shownEnvs = operatorPoolShown (processorRef) ? (1u << opEnvTab) : 0u;
            for (int env = 0; env < (int) envSets.size(); ++env)
                shownEnvs |= envelopeShown (processorRef, env) ? (1u << env) : 0u;
            if (shownEnvs != lastShownEnvs)
            {
                lastShownEnvs = shownEnvs;
                resized();
                updateVisibility();
            }
        }

        // An operator voice with both filters wide open has none.
        {
            auto off = false;
            for (int osc = 0; osc < OscillatorIds::count && ! off; ++osc)
                off = processorRef.isOscillatorShown (osc) && readInt (OscRole::prefix (osc) + "_mode") == 0 && OscRole::usesOperatorEg (processorRef, osc);
            off = off && FilterDisplay::isPassThrough (processorRef, 0, true) && FilterDisplay::isPassThrough (processorRef, 1, true);
            if (off != operatorFilterOff)
            {
                operatorFilterOff = off;
                filterDisplay.setVisible (! off);
                filterOffNote.setVisible (off);
                repaint (filterCard);
            }
        }

        effectRules.apply();

        // The OP ENV card's bars follow the operators' OUTPUTs.
        if (selectedEnv == opEnvTab && ! opEnvNoteArea.isEmpty())
        {
            auto sum = 0.0f;
            for (const auto osc : OperatorPool::operatorsOnEnv (processorRef))
                if (const auto* parameter = processorRef.apvts.getParameter (OscRole::prefix (osc) + "_eg_out"))
                    sum += parameter->getValue() * (float) (osc + 1);
            if (sum != lastOperatorOutputs)
            {
                lastOperatorOutputs = sum;
                repaint (envCard);
            }
        }

        // The LFO card's caption and run switch follow its shape (I9-1 / I9-19).
        {
            const auto lfo = lfoTabs.getSelected();
            const auto caption = LfoShapeMenu::runCaption (processorRef, lfo);
            if (caption != shownLfoCaption)
            {
                shownLfoCaption = caption;
                repaint (lfoCard);
            }
            if (juce::isPositiveAndBelow (lfo, (int) lfoSets.size()))
                if (auto* retrig = dynamic_cast<ToggleControl*> (lfoSets[(size_t) lfo]->items[2].get()))
                {
                    const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
                    LfoSection::labelRunSwitch (*retrig, shape != nullptr && LfoSimShapes::isSim (juce::roundToInt (shape->load())));
                }
        }

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
            // A voice whose oscillators all play OP ENV opens on its tab,
            // not on the unused AMP ENV (UI review 8, I8-18).
            if (note == EnvSection::ampUnusedText() && selectedEnv == 0)
                selectedEnv = opEnvTab;
            shownAmpNote = note;
            resized();
            updateVisibility();
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

        // One wording with the MOD page (UI review 8, S8-17).
        return operatorEg == playing ? juce::String (EnvSection::ampUnusedText())
                                     : juce::String ("unused: the oscillators play other envelopes");
    }

    juce::String shownLfoCaption;

    int firstOperatorEg() const
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc) && OscRole::usesOperatorEg (processorRef, osc))
                return osc;

        return -1;
    }

    // The title line's centre on the right column's cards.
    static int titleCentreY (juce::Rectangle<int> card) { return card.getY() + cardHeaderHeight / 2; }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint)
    {
        if (card.isEmpty())
            return;

        const auto centreY = titleCentreY (card);
        IlanaTheme::paintCard (g, card.toFloat(), 6.0f, tint);
        IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) centreY }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::cardTitleFont());
        g.drawText (title, juce::Rectangle<int> (card.getX() + 24, centreY - 8, 200, 16), juce::Justification::centredLeft);
    }

    WaveDisplay& wave (int index) { return *waves[(size_t) index]; }

    // A strip's title column: the tag and name, then what it is.
    // A strip with the height for it is laid out as a header line (title,
    // role, switch) over a body: the picture takes the left, the menus sit
    // in a row over the knobs; a low one (four or more, a small window)
    // carries its menus in the header line (see stripColumns).
    static bool roomy (juce::Rectangle<int> card) { return card.getHeight() >= roomyHeight; }
    static int dialSize (juce::Rectangle<int> card)
    {
        return roomy (card) ? 38 : IlanaTheme::KnobSize::minimum;
    }
    static juce::Rectangle<int> titleArea (juce::Rectangle<int> card) { return card.withWidth (8 + titleWidth); }
    // The strip's "EDIT ›": in the header line, left of the switch and level with it.
    static juce::Rectangle<int> editLinkArea (juce::Rectangle<int> card)
    {
        return { card.getRight() - switchWidth - 12 - editLinkWidth, card.getY() + cardHeaderHeight / 2 - 8, editLinkWidth, 16 };
    }

    // A card's body: the picture, the menus (a row, or a low card's header
    // line) and the knobs.
    struct StripColumns
    {
        juce::Rectangle<int> picture, menus, knobs;
    };

    // Every open card is the design's (mockup "play"): a header line (tag,
    // name, what it does, EDIT and the switch) over a body with the picture
    // at the left. A tall card has its menus in a row over the knobs; a low
    // one (four or more oscillators, a small window) carries them in its
    // header line, between the title and EDIT, as the OSC page's cards do,
    // and its knobs are one row: stacked (name, dial, value) while the body
    // has the height, inline (the dial with its name and value beside it,
    // as on OSC) below that. ilana's PC test: four oscillators fell back to
    // the old strip (a title column, stacked menus, the switch mid-card).
    static constexpr int headerMenuHeight = 22, oscHeaderMenus = 100 + 6 + 96, subHeaderMenus = 156 + 6 + 104;
    static constexpr int stackedKnobsHeight = 13 + IlanaTheme::KnobSize::minimum + 14, lowPadY = 5;

    static juce::Rectangle<int> headerMenusArea (juce::Rectangle<int> card, bool oscillator)
    {
        const auto width = oscillator ? oscHeaderMenus : subHeaderMenus;
        const auto right = card.getRight() - switchWidth - 12 - (oscillator ? editLinkWidth + 8 : 0);
        return { right - width, card.getY() + (cardHeaderHeight - headerMenuHeight) / 2, width, headerMenuHeight };
    }

    // A low card's knobs stand inline once the body is too low for name, dial and value stacked.
    static bool inlineKnobs (juce::Rectangle<int> card)
    {
        return ! roomy (card) && card.getHeight() - cardHeaderHeight - 2 * lowPadY < stackedKnobsHeight;
    }

    static StripColumns stripColumns (juce::Rectangle<int> card, bool oscillator = true)
    {
        StripColumns columns;

        if (roomy (card))
        {
            auto inner = card.withTrimmedTop (cardHeaderHeight).reduced (cardPadX, cardPadY);
            columns.picture = inner.removeFromLeft (stripPictureWidth);
            inner.removeFromLeft (10);
            columns.menus = inner.removeFromTop (stripMenuHeight);
            columns.knobs = inner;
            return columns;
        }

        auto inner = card.withTrimmedTop (cardHeaderHeight).reduced (cardPadX, lowPadY);
        columns.menus = headerMenusArea (card, oscillator);
        // The picture keeps a landscape well (a saw in a portrait box read as
        // a tick, V10-2), narrower on an inline row.
        columns.picture = inner.removeFromLeft (inlineKnobs (card) ? lowPictureWidth - 30 : lowPictureWidth);
        inner.removeFromLeft (10);
        columns.knobs = inner;
        return columns;
    }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];

        if (! shownStrips[(size_t) index])
            return;

        strip.on->setBounds (IlanaTheme::cardSwitchBounds (card, ! isFolded (index) ? card.getY() + cardHeaderHeight / 2 : card.getCentreY() - 1));

        if (isFolded (index))
            return;

        auto columns = stripColumns (card);
        auto row = strip.knobs();
        // An inline row needs the card's width for its names and values
        // ("POSITION", "-30.9 dB"): the picture gives way, so the six columns
        // still line up from card to card. An operator keeps its envelope
        // picture (I8-37) and spreads its four knobs over the row.
        if (inlineKnobs (card) && ! strip.opEg)
        {
            columns.knobs.setLeft (columns.picture.getX());
            columns.picture = {};
        }
        else if (strip.opEg)
            while (! row.empty() && row.back() == nullptr)
                row.pop_back();
        wave (index).setBounds (columns.picture);
        auto menus = columns.menus;
        const auto mode = juce::jmax (0, strip.shownMode);

        // A compact strip has no warp menu (its WARP knob stays): the table
        // menu's tooltip says what the warp is, so the choice is not gone (V12-14).
        if (strip.tableTip.isEmpty())
            strip.tableTip = strip.table->getTooltip();
        {
            const auto tip = strip.tableTip + (mode == 0 && ! roomy (card) && strip.warp->getComboBox().getSelectedItemIndex() > 0
                                                   ? "\nWarp: " + strip.warp->getComboBox().getText() + " (set on OSC)." : juce::String());
            strip.table->setTooltip (tip);
            strip.table->getComboBox().setTooltip (tip);
        }

        if (roomy (card))
        {
            // The design's menus: 112 / 96 / 68 of a 276 px row, 6 px apart.
            const auto count = mode == 0 && ! strip.opEg ? 3 : mode <= 1 ? 2 : 1;
            const auto room = menus.getWidth() - 6 * (count - 1);
            const auto cell = room / count;
            if (mode == 0 && count == 3)
            {
                strip.mode->setBounds (menus.removeFromLeft (room * 112 / 276));
                menus.removeFromLeft (6);
                strip.table->setBounds (menus.removeFromLeft (room * 96 / 276));
                menus.removeFromLeft (6);
                strip.warp->setBounds (menus);
            }
            else
            {
                strip.mode->setBounds (menus.removeFromLeft (cell));
                menus.removeFromLeft (6);
                if (mode == 0)
                    strip.table->setBounds (menus);
                else if (mode == 1)
                    strip.excite->setBounds (menus);
            }
        }
        else
        {
            // In the header line: the engine, then the table or the exciter.
            strip.mode->setBounds (menus.removeFromLeft (100));
            menus.removeFromLeft (6);

            if (mode == 0)
                strip.table->setBounds (menus);
            else if (mode == 1)
                strip.excite->setBounds (menus);
        }

        // A tall strip (one open oscillator) gets bigger dials rather than a
        // gap above and below a row of small ones (V13-3).
        const auto dial = dialSize (card);
        const auto inlineRow = inlineKnobs (card);
        for (auto& entry : strip.allKnobs)
        {
            entry.second->setInlineKnob (inlineRow);
            entry.second->setInlineDial (inlineDial);
            if (entry.second->getMaxDial() != dial)
                entry.second->setSizeRole (dial);
        }

        if (! inlineRow)
        {
            layoutRow (columns.knobs, strip.knobs());
            return;
        }

        // Equal cells, each knob's 26 px dial at its left (as the OSC cards').
        placeInline (columns.knobs, row);
    }

    // An inline row: equal cells, a dial at each cell's left with its name
    // and value beside it (a null is an empty cell).
    static constexpr int inlineDial = 26;
    static void placeInline (juce::Rectangle<int> area, const std::vector<juce::Component*>& row)
    {
        if (row.empty())
            return;
        const auto cells = area.withSizeKeepingCentre (area.getWidth(), 28);
        const auto cellWidth = (float) cells.getWidth() / (float) row.size();
        for (size_t i = 0; i < row.size(); ++i)
            if (row[i] != nullptr)
            {
                const auto left = cells.getX() + juce::roundToInt ((float) i * cellWidth);
                const auto right = cells.getX() + juce::roundToInt ((float) (i + 1) * cellWidth) - 2;
                row[i]->setBounds (juce::Rectangle<int> (left, cells.getY(), right - left, cells.getHeight()).expanded (0, 3)); // (room for the rings)
            }
    }

    static juce::Colour subColour() { return IlanaTheme::accent(); }
    // The noise knobs wear the SUB colour at half strength, so COLOUR does not read as disabled (V13-15).
    static juce::Colour noiseTint() { return subColour().interpolatedWith (IlanaTheme::Ui::text2, 0.2f); } // (mostly the sub's orange: COLOUR read disabled in grey, V14-14)

    void layoutSubCard()
    {
        subOn->setBounds (IlanaTheme::cardSwitchBounds (subCard, ! subFolded ? subCard.getY() + cardHeaderHeight / 2 : subCard.getCentreY() - 1));
        for (auto* item : { (juce::Component*) subShape.get(), (juce::Component*) subOctave.get(), (juce::Component*) subLevel.get(),
                            (juce::Component*) noiseLevel.get(), (juce::Component*) noiseColour.get() })
            item->setVisible (! subFolded);
        if (subFolded)
            return;
        const auto columns = stripColumns (subCard, false);
        auto menus = columns.menus;

        if (roomy (subCard))
        {
            subShape->setBounds (menus.removeFromLeft (menus.getWidth() * 3 / 5));
            menus.removeFromLeft (6);
            subOctave->setBounds (menus);
        }
        else
        {
            // In the header line, as the oscillators' menus.
            subShape->setBounds (menus.removeFromLeft (156));
            menus.removeFromLeft (6);
            subOctave->setBounds (menus);
        }

        const auto inlineRow = inlineKnobs (subCard);
        for (auto* knob : { subLevel.get(), noiseLevel.get(), noiseColour.get() })
        {
            knob->setInlineKnob (inlineRow);
            knob->setInlineDial (inlineDial);
            if (knob->getMaxDial() != (roomy (subCard) ? 34 : dialSize (subCard)))
                knob->setSizeRole (roomy (subCard) ? 34 : dialSize (subCard));
        }

        if (! inlineRow)
        {
            layoutRow (columns.knobs, { subLevel.get(), noiseLevel.get(), noiseColour.get() }); // three columns: COLOUR needs the width
            return;
        }

        placeInline (columns.knobs, { subLevel.get(), noiseLevel.get(), noiseColour.get() });
    }

    // The strips' cards and titles (their controls draw themselves), and
    // the dim empty slots.
    void paintColumn (juce::Graphics& g)
    {
        // One line across a card's top: tag, name, what it is. `reserve` is
        // the width the header's own controls take before EDIT (a low card's
        // menus); a caption too long for the rest says the same shorter
        // ("out, \u2192 1": V14-17).
        const auto paintTitle = [&g] (juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, bool lit,
                                      const juce::String& line2, int reserve = 0)
        {
            const auto line = card.withHeight (headerHeight + 10).reduced (0, 5).withTrimmedRight (switchWidth + 20 + editLinkWidth + 8 + reserve);
            IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) line.getCentreY() }, lit ? tint : tint.withAlpha (0.4f));
            g.setColour (lit ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            const auto nameFont = juce::Font (IlanaTheme::cardTitleFont());
            g.setFont (nameFont);
            g.drawText (title, line.withTrimmedLeft (24), juce::Justification::centredLeft);

            if (line2.isNotEmpty())
            {
                // (The card caption: the sheet's .cap, 8 px after the title.)
                const auto room = line.withTrimmedLeft (24 + juce::GlyphArrangement::getStringWidthInt (nameFont, title) + 8);
                const auto captionFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body));
                auto text = IlanaTheme::captionFragment (line2);
                if (juce::GlyphArrangement::getStringWidth (captionFont, text) > (float) room.getWidth())
                    text = text.replace ("to output, modulates ", juce::String::fromUTF8 ("out, \xe2\x86\x92 "))
                               .replace ("modulates ", juce::String::fromUTF8 ("\xe2\x86\x92 "));
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (captionFont);
                g.drawText (text, room, juce::Justification::centredLeft, true);
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
                g.drawText (foldedSummary (osc), line.withLeft (card.getX() + 8 + compactTitleWidth).withRight (card.getRight() - switchWidth - 12),
                            juce::Justification::centredLeft, true);
                continue;
            }

            paintTitle (card, name, tint, strip.shownOn, strip.shownOn ? strip.role : juce::String(), roomy (card) ? 0 : oscHeaderMenus + 8); // off: the dimming says it (S8-12)

            // The title opens the oscillator's full page, and so does the
            // link beside the switch, worded like every other jump.
            g.setColour (hoverEditLink == osc ? tint.brighter (0.25f) : tint); // (the card's colour: the sheet's .lk)
            g.setFont (IlanaTheme::linkFont());
            g.drawText (juce::String ("EDIT ") + juce::String::fromUTF8 ("\xe2\x80\xba"), editLinkArea (card), juce::Justification::centredRight);

            if (strip.opEg)
            {
                // The picture is the Operator Env, not the wave: say so in
                // its corner (UI review 8, I8-37).
                const auto picture = stripColumns (card).picture;
                strip.thumb.paint (g, picture.toFloat(), tint, strip.shownOn);
                // (Under the title, at the card's foot, clear of the curve.)
                g.setColour (IlanaTheme::Ui::text3.withAlpha (strip.shownOn ? 1.0f : 0.5f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("OP ENV", juce::Rectangle<int> (picture.getX() + 6, picture.getBottom() - 16, 60, 12), juce::Justification::centredLeft);
                // An operator has no MODE menu: its place labels the wave under it (S12-13).
                g.setColour (strip.shownOn ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                IlanaTheme::drawFitted (g, "WAVE", roomy (card) ? stripColumns (card).menus.withHeight (24).withTrimmedLeft (8)
                                                                  : stripColumns (card).menus.withWidth (100).withTrimmedLeft (8),
                                        juce::Justification::centredLeft, 1);
            }
        }

        if (! patchCard.isEmpty())
        {
            IlanaTheme::paintCard (g, patchCard.toFloat(), 6.0f, IlanaTheme::accent());
            paintTitle (patchCard, "PATCH", IlanaTheme::accent(), true, {});
        }

        if (! outputCard.isEmpty())
        {
            IlanaTheme::paintCard (g, outputCard.toFloat(), 6.0f, IlanaTheme::Ui::text2);
            paintTitle (outputCard, "OUTPUT", IlanaTheme::Ui::text2, true, {});
        }

        if (! subCard.isEmpty() && subFolded)
        {
            // Folded like an off oscillator: the dimmed title, what it would play, the switch.
            IlanaTheme::paintCard (g, subCard.toFloat(), 6.0f, subColour().withAlpha (0.3f));
            const auto line = subCard.withSizeKeepingCentre (subCard.getWidth(), 16);
            IlanaTheme::paintTag (g, { (float) subCard.getX() + 15.0f, (float) line.getCentreY() }, subColour().withAlpha (0.4f));
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("SUB + NOISE", line.withX (subCard.getX() + 24).withWidth (juce::jmax (titleWidth, 100)), juce::Justification::centredLeft);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            static const char* const shapes[] { "Sine", "Square", "Saw" };
            g.drawText (juce::String (shapes[juce::jlimit (0, 2, readInt ("sub_shape"))]) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 sub off, no noise")),
                        line.withLeft (subCard.getX() + 8 + titleWidth).withRight (subCard.getRight() - switchWidth - 12), juce::Justification::centredLeft, true);
        }
        else if (! subCard.isEmpty())
        {
            IlanaTheme::paintCard (g, subCard.toFloat(), 6.0f, subColour());
            paintTitle (subCard, "SUB + NOISE", subColour(), true, {}, roomy (subCard) ? 0 : subHeaderMenus + 8 - editLinkWidth - 8);

            // The sub's shape where the oscillators show their picture.
            const auto picture = stripColumns (subCard, false).picture.toFloat();
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
        std::function<void (juce::Point<int>)> onHover;
        void mouseMove (const juce::MouseEvent& event) override { if (onHover != nullptr) onHover (event.getPosition()); }
        void mouseExit (const juce::MouseEvent&) override { if (onHover != nullptr) onHover ({ -1, -1 }); }
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
    DashedAddButton addOscButton { "+  ADD OSC", "+  ADD OSC" };
    std::array<bool, OscillatorIds::count> shownStrips {};
    int lastRevealVersion = -1, hoverEditLink = -1;
    // The column: the strips and SUB + NOISE, all one height, and the
    // "+ ADD OSC" row.
    // The design's one spacing grid and fixed heights (mockup panelsA.js, play).
    static constexpr int pageGutter = 14, cardGap = 10, slotGap = cardGap, cardHeaderHeight = 30, cardPadX = 10, cardPadY = 8;
    static constexpr int oscHeightDesign = 146, subHeightDesign = 140, addRowHeight = 28, foldedHeight = 36, maxGrowth = 54;
    static constexpr int filterDisplayWidth = 170, envGraphHeight = 56, lfoThumbHeight = 74, envKnobDial = 34, stripPictureWidth = 190, stripMenuHeight = 26;
    static constexpr int minSlotHeight = 68, roomyHeight = 112, headerHeight = 20, editLinkWidth = 56;
    static constexpr int titleWidth = 84, pictureWidth = 100, menuWidth = 96, switchWidth = 46, minKnobsWidth = 244;
    static constexpr int compactTitleWidth = 64, lowPictureWidth = 100;
    juce::Rectangle<int> addRowArea;
    static constexpr float offAlpha = 0.55f;
    // PLAY's envelope: which one, the envelopes on its tabs, those behind
    // "+N", and the pool last seen.
    int selectedEnv = 0;
    std::vector<int> envTabEnvs { 0 }, envHiddenEnvs;
    unsigned int lastShownEnvs = 0;
    EffectRules effectRules { processorRef };
    juce::Rectangle<int> subCard, patchCard, outputCard;
    SignalFlow patchFlow { processorRef };
    OutputView outputView { processorRef };
    static constexpr int patchMinHeight = 150, outputMinHeight = 100, maxPatchOnlyHeight = 170, // (taller tiles, V14-10: the nodes at a legible size, the output view with room for both traces)
                                   maxGrownSlotHeight = 200;
    std::unique_ptr<ToggleControl> subOn;
    bool subFolded = false;
    float lastOperatorOutputs = 0.0f;
    std::unique_ptr<ChoicePills> subShape, subOctave;
    std::unique_ptr<KnobControl> subLevel, noiseLevel, noiseColour;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waves;
    juce::String shownAmpNote;
    juce::Rectangle<int> ampNoteArea;
    juce::TextButton opEgButton;
    OperatorEnvOverview opEnvOverview { processorRef };
    juce::Rectangle<int> opEnvNoteArea;
    FilterDisplay filterDisplay;
    juce::Label filterOffNote;
    bool operatorFilterOff = false;
    juce::Viewport lfoThumbView;
    LfoThumbBar lfoThumbs;
    CardTabs filterTabs, envTabs, lfoTabs;
    std::vector<std::unique_ptr<OscStrip>> strips;
    std::vector<std::unique_ptr<ControlSet>> filterSets, envSets, lfoSets;
    std::vector<std::unique_ptr<LfoRateSlider>> lfoRates;
    std::array<juce::Rectangle<int>, OscillatorIds::count> oscCards;
    juce::Rectangle<int> filterCard, envCard, lfoCard;
};
} // namespace
