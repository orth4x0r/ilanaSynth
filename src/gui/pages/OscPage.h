// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class OscPage : public juce::Component,
                private juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater,
                private juce::Timer
{
    struct PhysicalControls
    {
        PhysicalControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : stiffness (state, prefix + "_string_stiffness", "STIFF"),
              pickup (state, prefix + "_string_pickup", "PICKUP"),
              excitePos (state, prefix + "_string_excite_pos", "EXCITE POS"),
              hardness (state, prefix + "_string_pick_hardness", "HARDNESS"),
              pickPos (state, prefix + "_string_pick_pos", "PICK POS"),
              slap (state, prefix + "_string_slap", "SLAP"),
              bowPressure (state, prefix + "_bow_pressure", "BOW PRESS"),
              bowSpeed (state, prefix + "_bow_speed", "BOW SPEED"),
              bridgeBuzz (state, prefix + "_bridge_buzz", "BRIDGE BUZZ"),
              fretRattle (state, prefix + "_fret_rattle", "FRET RATTLE"),
              hammer (state, prefix + "_hammer_hard", "HAMMER"),
              couple (state, prefix + "_couple", "COUPLING"),
              damper (state, prefix + "_damper", "DAMPER"),
              registerMap (state, prefix + "_register", "REGISTER"),
              epDistance (state, prefix + "_ep_distance", "DISTANCE"),
              epPosition (state, prefix + "_ep_position", "OFFSET"),
              fbGain (state, prefix + "_fb_gain", "AMP GAIN"),
              fbDistance (state, prefix + "_fb_distance", "DISTANCE") {}

        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &stiffness, &pickup, &excitePos, &hardness, &pickPos, &bowPressure, &bowSpeed,
                                &bridgeBuzz, &fretRattle, &hammer, &couple, &damper, &registerMap,
                                &epDistance, &epPosition, &fbGain, &fbDistance })
                knob->setIdentityColour (colour);
        }

        KnobControl stiffness, pickup, excitePos, hardness, pickPos;
        KnobControl bowPressure, bowSpeed, bridgeBuzz, fretRattle;
        KnobControl hammer, couple, damper, registerMap;
        KnobControl epDistance, epPosition; // M7.3 Tine / Reed pickup
        KnobControl fbGain, fbDistance;     // M8.5 feedback amp
        ToggleControl slap;
    };

    struct OscControls
    {
        OscControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : on (state, prefix + "_on", "ON"),
              mode (state, prefix + "_mode", "MODE"),
              table (state, prefix + "_table", "TABLE"),
              excite (state, prefix + "_excite", "EXCITE"),
              frame (state, prefix + "_frame", "FRAME"),
              level (state, prefix + "_level", "LEVEL"),
              pan (state, prefix + "_pan", "PAN"),
              semi (state, prefix + "_semi", "SEMI"),
              fine (state, prefix + "_fine", "FINE"),
              unison (state, prefix + "_unison", "UNISON"),
              detune (state, prefix + "_detune", "DETUNE"),
              spread (state, prefix + "_spread", "SPREAD"),
              stringDecay (state, prefix + "_string_decay", "DECAY"),
              stringDamp (state, prefix + "_string_damp", "DAMP"),
              stringSustain (state, prefix + "_string_sustain", "SUSTAIN"),
              sampleTuned (state, prefix + "_sample_tuned", "TUNED"),
              sampleLoop (state, prefix + "_sample_loop", "LOOP"),
              sampleReverse (state, prefix + "_sample_reverse", "REVERSE"),
              sampleStart (state, prefix + "_sample_start", "START"),
              sampleEnd (state, prefix + "_sample_end", "END"),
              sampleFadeIn (state, prefix + "_sample_fade_in", "FADE IN"),
              sampleFadeOut (state, prefix + "_sample_fade_out", "FADE OUT"),
              chord (state, prefix + "_chord", "CHORD"),
              ampEnv (state, prefix + "_amp_env", "AMP ENV"),
              warp (state, prefix + "_warp", "WARP"),
              uniMode (state, prefix + "_uni_mode", "UNI MODE"),
              warpAmt (state, prefix + "_warp_amt", "WARP AMT"),
              uniBlend (state, prefix + "_uni_blend", "BLEND"),
              spectral (state, prefix + "_spectral", "SPECTRAL"),
              spectralAmt (state, prefix + "_spectral_amt", "SPEC AMT"),
              grainPosition (state, prefix + "_sample_start", "POSITION"),
              grainSize (state, prefix + "_grain_size", "SIZE"),
              grainDensity (state, prefix + "_grain_density", "DENSITY"),
              grainSpray (state, prefix + "_grain_spray", "SPRAY"),
              grainPitch (state, prefix + "_grain_pitch", "PITCH RND"),
              grainSpread (state, prefix + "_grain_spread", "STEREO"),
              grainLive (state, prefix + "_grain_live", "LIVE"),
              warp2 (state, prefix + "_warp2", "WARP 2"),
              pdEnv (state, prefix + "_pd_env", "WARP ENV"),
              warp2Amt (state, prefix + "_warp2_amt", "WARP 2 AMT"),
              pdEnvAmt (state, prefix + "_pd_env_amt", "ENV AMT") {}

        // Every knob in the oscillator's own colour, as on PLAY and PHYSICAL.
        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &warp2Amt, &pdEnvAmt, &frame, &level, &pan, &semi, &fine, &unison, &detune, &spread,
                                &stringDecay, &stringDamp, &stringSustain, &sampleStart, &sampleEnd, &sampleFadeIn,
                                &sampleFadeOut, &warpAmt, &uniBlend, &spectralAmt, &grainPosition, &grainSize,
                                &grainDensity, &grainSpray, &grainPitch, &grainSpread })
                knob->setIdentityColour (colour);
        }

        ToggleControl on, sampleTuned, sampleLoop, sampleReverse;
        ComboControl mode, table, excite, chord, ampEnv, warp, uniMode, spectral;
        // M6: the PD chain's second stage and the warp (DCW) envelope.
        ComboControl warp2, pdEnv;
        KnobControl warp2Amt, pdEnvAmt;
        KnobControl frame, level, pan, semi, fine, unison, detune, spread;
        KnobControl stringDecay, stringDamp, stringSustain;
        KnobControl sampleStart, sampleEnd, sampleFadeIn, sampleFadeOut;
        KnobControl warpAmt, uniBlend, spectralAmt;
        KnobControl grainPosition, grainSize, grainDensity, grainSpray, grainPitch, grainSpread;
        ToggleControl grainLive; // M7.5: grains from the live input (ilanaSynth FX)
    };

public:
    std::function<void()> onModeChanged;

    explicit OscPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          subShape (p.apvts, "sub_shape", "SHAPE"),
          subOctave (p.apvts, "sub_octave", "OCTAVE"),
          noiseLevel (p.apvts, "noise_level", "NOISE")
          , symOn (p.apvts, "sym_on", "ON"), symManual (p.apvts, "sym_manual", "MANUAL")
          , symAmount (p.apvts, "sym_amount", "AMOUNT"), symDecay (p.apvts, "sym_decay", "DECAY")
          , symCount (p.apvts, "sym_count", "STRINGS")
          , sbOn (p.apvts, "sb_on", "BOARD"), sbModel (p.apvts, "sb_model", "MODEL"), sbMix (p.apvts, "sb_mix", "BODY MIX"), sbTone (p.apvts, "sb_tone", "TONE")
          , sbSize (p.apvts, "sb_size", "SIZE"), stretch (p.apvts, "stretch", "STRETCH")
          , pedalRes (p.apvts, "pedal_res", "PEDAL RES"), mechKey (p.apvts, "mech_key", "KEY NOISE")
          , mechDamper (p.apvts, "mech_damper", "DAMPER NOISE"), mechPedal (p.apvts, "mech_pedal", "PEDAL NOISE")
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            controls[(size_t) i] = std::make_unique<OscControls> (p.apvts, prefix);
            controls[(size_t) i]->setColour (oscColour (i));
            waveDisplays[(size_t) i] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison",
                prefix + "_spread", prefix + "_detune", false, juce::String {},
                prefix + "_mode", i, oscColour (i), false);
            loadButtons[(size_t) i] = std::make_unique<juce::TextButton> ("LOAD .WAV");
            editButtons[(size_t) i] = std::make_unique<juce::TextButton> ("EDIT");
            bounceButtons[(size_t) i] = std::make_unique<juce::TextButton> ("BOUNCE");
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            physical[(size_t) i] = std::make_unique<PhysicalControls> (p.apvts, prefix);
            physical[(size_t) i]->setColour (oscColour (i));
            auto& physicalControls = *physical[(size_t) i];
            addAll (*this, physicalControls.stiffness, physicalControls.pickup, physicalControls.excitePos,
                    physicalControls.hardness, physicalControls.pickPos, physicalControls.slap,
                    physicalControls.bowPressure, physicalControls.bowSpeed,
                    physicalControls.bridgeBuzz, physicalControls.fretRattle,
                    physicalControls.hammer, physicalControls.couple,
                    physicalControls.damper, physicalControls.registerMap,
                    physicalControls.epDistance, physicalControls.epPosition,
                    physicalControls.fbGain, physicalControls.fbDistance);
        }

        addAll (*this, symOn, symManual, symAmount, symDecay, symCount);
        addAll (*this, sbOn, sbModel, sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal);
        for (int i = 0; i < 6; ++i)
        {
            symNotes[(size_t) i] = std::make_unique<KnobControl> (p.apvts, "sym_note" + juce::String (i + 1),
                                                                    "NOTE " + juce::String (i + 1));
            addAndMakeVisible (*symNotes[(size_t) i]);
        }

        for (auto& item : controls)
        {
            auto& osc = *item;
            addAll (*this, osc.grainPosition, osc.grainSize, osc.grainDensity,
                    osc.grainSpray, osc.grainPitch, osc.grainSpread, osc.grainLive,
                    osc.spectral, osc.spectralAmt, osc.warp, osc.uniMode,
                    osc.warpAmt, osc.uniBlend, osc.warp2, osc.pdEnv, osc.warp2Amt, osc.pdEnvAmt);

            addAll (*this, osc.on, osc.mode, osc.table, osc.excite,
                    osc.frame, osc.level, osc.pan, osc.semi, osc.fine,
                    osc.unison, osc.detune, osc.spread, osc.stringDecay,
                    osc.stringDamp, osc.stringSustain, osc.sampleTuned,
                    osc.sampleLoop, osc.sampleReverse, osc.sampleStart,
                    osc.sampleEnd, osc.sampleFadeIn, osc.sampleFadeOut,
                    osc.chord, osc.ampEnv);
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            addAndMakeVisible (waveDisplay (i));
            setupLoadButton (loadButton (i),
                             juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table", 0);
            addAndMakeVisible (loadButton (i));
            auto& edit = *editButtons[(size_t) i];
            edit.setTooltip ("Edit this wavetable: draw frames, set harmonics, formulas and morphs.\n"
                             "A factory table is copied into one of the patch's 16 tables first.");
            edit.onClick = [this, i] { openTableEditor (i); };
            addAndMakeVisible (edit);
            auto& bounce = *bounceButtons[(size_t) i];
            bounce.setTooltip ("Resample: play the whole patch (one note, optionally with its effects) and put the "
                               "result on this oscillator, as a tuned sample or cut into a wavetable. "
                               "The bounce is saved inside the patch.");
            bounce.onClick = [this, i] { showBounceMenu (i); };
            addAndMakeVisible (bounce);
        }

        addAll (*this, subShape, subOctave, noiseLevel);
        sbOn.showAsSwitch();

        // The TABLE lists open the wavetable browser.
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto* control = &controls[(size_t) i]->table;
            const auto id = juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table";
            const auto colour = oscColour (i);
            control->setPopupOverride ([this, control, id, colour]
            {
                TableBrowser::show (processorRef, id, colour, control->getComboBox());
            });
        }

        // Sub and voice are cards like the rest of the page: title, switch on
        // the right, labelled knobs underneath.
        voiceSpread = std::make_unique<KnobControl> (p.apvts, "voice_spread", "SPREAD");
        unisonRandom = std::make_unique<KnobControl> (p.apvts, "unison_random", "UNI PHASE");
        drift = std::make_unique<KnobControl> (p.apvts, "drift", "DRIFT");
        addAll (*this, *voiceSpread, *unisonRandom, *drift);

        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subOscLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB LEVEL", IlanaTheme::accent(), true);
        noiseStrip = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        addAll (*this, *subOscOn, *subOscLevel, *noiseStrip);
        noiseLevel.setVisible (false);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on" })
            processorRef.apvts.addParameterListener (id, this);

        // Phase Plant style: remove any oscillator, add the next hidden one.
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            removeButtons[(size_t) i] = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
            removeButtons[(size_t) i]->setTooltip ("Remove this oscillator (switches it off and hides it)");
            removeButtons[(size_t) i]->onClick = [this, i]
            {
                processorRef.removeOscillator (i);
                updateModeVisibility();
                updateEnabled();
            };
            addAndMakeVisible (*removeButtons[(size_t) i]);
        }

        addButton.setButtonText ("+  ADD OSCILLATOR");
        addButton.setTooltip ("Add the next oscillator, switched on");
        addButton.onClick = [this]
        {
            for (int i = 0; i < OscillatorIds::count; ++i)
                if (! processorRef.isOscillatorShown (i))
                {
                    processorRef.addOscillator (i);
                    break;
                }

            updateModeVisibility();
            updateEnabled();
        };
        addAndMakeVisible (addButton);

        lastRevealVersion = processorRef.getRevealVersion();
        updateModeVisibility();
        updateEnabled();
        startTimerHz (5);
    }

    ~OscPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on" })
            processorRef.apvts.removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String&, float) override
    {
        // Parameter changes can arrive on the audio thread (host automation),
        // so defer the GUI work to the message thread.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        updateModeVisibility();
        updateEnabled();
    }

    // Patch loads change which oscillators are shown.
    void timerCallback() override
    {
        if (bouncingOsc >= 0)
            updateBounce();

        if (const auto version = processorRef.getRevealVersion(); version != lastRevealVersion)
        {
            lastRevealVersion = version;
            updateModeVisibility();
            updateEnabled();
        }
    }

    // The viewport's height: cards are sized as if three fill it, and more
    // than that scroll.
    void setAvailableHeight (int height) { availableHeight = height; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (int band = 0; band < OscillatorIds::count; ++band)
        {
            if (! shown[(size_t) band])
                continue;

            const auto bounds = bandBounds (band);
            const auto tint = oscColour (band);

            IlanaTheme::paintCard (g, bounds.toFloat(), 6.0f, tint);

            const auto strip = juce::Rectangle<float> ((float) bounds.getX() + 2.0f, (float) bounds.getY() + 6.0f,
                                                       3.0f, (float) bounds.getHeight() - 12.0f);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (strip, 1.5f);

            const std::array<const char*, 5> modeNames { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
            const auto mode = juce::jlimit (0, 4, getMode (band));

            const auto headerY = headerCentreY (band, bounds);
            IlanaTheme::paintTag (g, { (float) bounds.getX() + 17.0f, (float) headerY }, tint);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (band + 1),
                        juce::Rectangle<int> (bounds.getX() + 28, headerY - 8, 60, 16),
                        juce::Justification::centredLeft);

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (isFolded (band) ? juce::String ("OFF  -  ") + modeNames[(size_t) mode] + "  -  switch on to edit"
                                        : juce::String (modeNames[(size_t) mode]),
                        juce::Rectangle<int> (bounds.getX() + 80, headerY - 7, 400, 14),
                        juce::Justification::centredLeft);

            if (! controlBay[(size_t) band].isEmpty())
                IlanaTheme::paintRecessedPanel (g, controlBay[(size_t) band].toFloat(), 6.0f);

            if (! chainLabel[(size_t) band].isEmpty())
            {
                IlanaTheme::paintRecessedPanel (g, chainBay[(size_t) band].toFloat(), 6.0f);
                const auto label = chainLabel[(size_t) band];
                g.setColour (tint.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                g.drawText ("WARP CHAIN", label.withHeight (18), juce::Justification::centredLeft);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawFittedText ("second stage and\nthe DCW envelope", label.withTrimmedTop (18), juce::Justification::topLeft, 2);
            }
        }

        // Every card on the page titled the same way: a tag, the title, and a
        // quiet subtitle right after it.
        const auto cardTitle = [&g] (juce::Rectangle<int> header, const juce::String& title, const juce::String& subtitle,
                                     juce::Colour tag)
        {
            IlanaTheme::paintTag (g, { (float) header.getX() + 17.0f, (float) header.getCentreY() }, tag);
            const auto titleFont = IlanaTheme::font (IlanaTheme::TextSize::body, true);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (titleFont);
            auto area = header.withTrimmedLeft (28);
            g.drawText (title, area, juce::Justification::centredLeft);

            if (subtitle.isNotEmpty())
            {
                area.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (juce::Font (titleFont), title) + 16);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText (subtitle, area.withTrimmedRight (90), juce::Justification::centredLeft);
            }
        };

        if (! subStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, subStrip.toFloat(), 6.0f);
            cardTitle (subStrip.withHeight (symHeaderHeight), "SUB + NOISE", "a sub an octave or two under the oscillators, and noise", IlanaTheme::accent());
        }

        if (! voiceStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, voiceStrip.toFloat(), 6.0f);
            cardTitle (voiceStrip.withHeight (symHeaderHeight), "VOICE", "how the unison voices spread and drift", IlanaTheme::Ui::text2);
        }

        if (! symCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, symCard.toFloat(), 6.0f);
            cardTitle (symCard.withHeight (symHeaderHeight), "SYMPATHETIC STRINGS",
                       "shared drone strings that ring with everything you play", IlanaTheme::Ui::text2);
        }

        if (! keysCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, keysCard.toFloat(), 6.0f);
            cardTitle (keysCard.withHeight (symHeaderHeight), "ACOUSTIC KEYS",
                       "soundboard, tuning, sustain pedal (CC64) and the action's noises; for Physical oscillators with the Hammer",
                       IlanaTheme::Ui::text2);
        }
    }

    static juce::Colour oscColour (int index) { return IlanaTheme::oscColour (index); }

    // Height the page needs so every card keeps its minimum size; a Physical
    // card has an extra row of knobs, so it gets extra height.
    int getMinimumHeight() const
    {
        auto height = pageMargin * 2 + bandGap * 2 + stripHeight + symCardHeight();

        for (int band = 0; band < OscillatorIds::count; ++band)
            if (shown[(size_t) band])
                height += heightOfBand (band) + bandGap;

        return height + (anyHidden() ? addCardHeight + bandGap : 0) + keysCardHeight + bandGap;
    }

    void resized() override
    {
        // Size cards as the three-oscillator page did; extra ones scroll.
        const auto fitHeight = availableHeight > 0 ? availableHeight : getHeight();
        bandHeight = juce::jlimit (minBandHeight, 176,
                                   (fitHeight - pageMargin * 2 - bandGap * 4 - bandFitStripHeight - symCardHeight()) / 3);
        auto area = getLocalBounds().reduced (12, pageMargin);

        for (int band = 0; band < OscillatorIds::count; ++band)
        {
            if (! shown[(size_t) band])
                continue;

            layoutBand (area.removeFromTop (heightOfBand (band)), band);
            area.removeFromTop (bandGap);
        }

        addButton.setVisible (anyHidden());
        if (anyHidden())
        {
            addButton.setBounds (area.removeFromTop (addCardHeight));
            area.removeFromTop (bandGap);
        }

        // One strip: the sub and noise on the left, voice settings on the right.
        auto strip = area.removeFromTop (stripHeight);
        // Sub and voice share the page's eight-column grid (four each), as
        // do the sympathetic strings below.
        subStrip = strip.removeFromLeft (strip.getWidth() / 2 - 4);
        strip.removeFromLeft (8);
        voiceStrip = strip;

        // Header (title, and the sub's switch on the right like every card's),
        // then one row of labelled controls.
        {
            auto header = subStrip.withHeight (symHeaderHeight);
            subOscOn->setBounds (IlanaTheme::cardSwitchBounds (subStrip, header.getCentreY()));
            layoutRow (subStrip.withTrimmedTop (symHeaderHeight).reduced (8, 0).withTrimmedBottom (4),
                       { &subShape, &subOctave, subOscLevel.get(), noiseStrip.get() });
            layoutRow (voiceStrip.withTrimmedTop (symHeaderHeight).reduced (8, 0).withTrimmedBottom (4),
                       { voiceSpread.get(), unisonRandom.get(), drift.get(), nullptr });
        }

        // The shared sympathetic strings: a one-line header with their switch,
        // which opens into their settings (and the notes, in MANUAL).
        area.removeFromTop (bandGap);
        symCard = area.removeFromTop (symCardHeight());
        auto symArea = symCard;
        auto header = symArea.removeFromTop (symHeaderHeight);
        // ToggleControl keeps 13 px above its button for a label; place it so
        // the button itself sits centred on the header line.
        symOn.setBounds (IlanaTheme::cardSwitchBounds (symCard, header.getCentreY()));

        if (readBool ("sym_on"))
        {
            symArea.reduce (10, 0);
            // Exactly on the SUB card's four columns above.
            layoutRow (symCard.withTrimmedTop (symHeaderHeight).withHeight (symRowHeight).withWidth (subStrip.getWidth()).reduced (8, 0),
                       { &symAmount, &symDecay, &symCount, &symManual });
            symArea.removeFromTop (symRowHeight);

            if (readBool ("sym_manual"))
                layoutSlots (symArea.removeFromTop (symRowHeight),
                             { symNotes[0].get(), symNotes[1].get(), symNotes[2].get(),
                               symNotes[3].get(), symNotes[4].get(), symNotes[5].get() });
        }

        area.removeFromTop (bandGap);
        keysCard = area.removeFromTop (keysCardHeight);
        auto keysArea = keysCard.withTrimmedTop (symHeaderHeight).reduced (10, 0);
        layoutSlots (keysArea.removeFromTop (symRowHeight),
                     { &sbOn, &sbModel, &sbMix, &sbTone, &sbSize, &stretch, &pedalRes, &mechKey, &mechDamper, &mechPedal });
    }

private:
    int bandHeight = 137;
    int availableHeight = 0;
    int lastRevealVersion = -1;
    std::array<bool, OscillatorIds::count> shown { true, true, true };
    static constexpr int bandGap = 6;
    static constexpr int addCardHeight = 40;

    bool anyHidden() const
    {
        return std::find (shown.begin(), shown.end(), false) != shown.end();
    }

    int numShown() const { return (int) std::count (shown.begin(), shown.end(), true); }
    // Dials the size of the oscillator cards' above.
    static constexpr int stripHeight = 28 + 13 + 42 + 16 + 12;
    // The oscillator cards are sized as when the sub and voice were a 50 px
    // strip; the taller cards just scroll a little further.
    static constexpr int bandFitStripHeight = 50;
    static constexpr int pageMargin = 6;
    static constexpr int symHeaderHeight = 28;
    static constexpr int symRowHeight = 64;
    static constexpr int keysCardHeight = 28 + 64 + 6;

    // The sympathetic card: just its header while the strings are off, one
    // row of settings when on, plus the notes in MANUAL.
    int symCardHeight() const
    {
        if (! readBool ("sym_on"))
            return symHeaderHeight;

        return symHeaderHeight + 6 + symRowHeight * (readBool ("sym_manual") ? 2 : 1);
    }
    static constexpr int minBandHeight = 124;
    static constexpr int physicalExtra = 140;

    static constexpr int warpChainExtra = 58;

    // A wavetable oscillator with a warp picked opens a row for the PD
    // chain's second stage and the warp envelope.
    bool showsWarpChain (int index) const
    {
        if (getMode (index) != 0)
            return false;

        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]);
        const auto* warp = processorRef.apvts.getRawParameterValue (prefix + "_warp");
        const auto* warp2 = processorRef.apvts.getRawParameterValue (prefix + "_warp2");
        return (warp != nullptr && warp->load() > 0.5f) || (warp2 != nullptr && warp2->load() > 0.5f);
    }

    // A switched-off oscillator folds to its title line.
    bool isFolded (int index) const
    {
        return ! readBool (juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]) + "_on");
    }

    static constexpr int foldedHeight = 40;

    int heightOfBand (int index) const
    {
        if (isFolded (index))
            return foldedHeight;

        return bandHeight + (getMode (index) == 1 ? physicalExtra : 0) + (showsWarpChain (index) ? warpChainExtra : 0);
    }

    bool readBool (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load() > 0.5f;

        return true;
    }

    juce::Rectangle<int> bandBounds (int index) const
    {
        const auto area = getLocalBounds().reduced (12, pageMargin);
        auto y = area.getY();

        for (int band = 0; band < index; ++band)
            if (shown[(size_t) band])
                y += heightOfBand (band) + bandGap;

        return { area.getX(), y, area.getWidth(), heightOfBand (index) };
    }

    WaveDisplay& waveDisplay (int index)
    {
        return *waveDisplays[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    juce::TextButton& loadButton (int index)
    {
        return *loadButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    static float slotWeight (juce::Component* item)
    {
        if (dynamic_cast<ComboControl*> (item) != nullptr)
            return 1.5f;

        if (dynamic_cast<ToggleControl*> (item) != nullptr)
            return 0.55f;

        return 1.1f;
    }

    static void layoutSlots (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
    {
        if (items.empty())
            return;

        const auto totalWidth = area.getWidth();
        auto totalWeight = 0.0f;

        for (auto* item : items)
            totalWeight += slotWeight (item);

        if (totalWeight <= 0.0f)
            return;

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto width = i + 1 == items.size()
                                   ? area.getWidth()
                                   : juce::jmax (1, (int) std::round ((float) totalWidth * slotWeight (items[i]) / totalWeight));

            auto cell = area.removeFromLeft (width).reduced (3);

            if (items[i] != nullptr)
                items[i]->setBounds (cell);
        }
    }

    int getMode (int oscIndex) const
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]) + "_mode";
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // The header line's centre: a folded card is just that line, centred.
    int headerCentreY (int index, juce::Rectangle<int> band) const
    {
        return isFolded (index) ? band.getCentreY() : band.getY() + 17;
    }

    void layoutBand (juce::Rectangle<int> band, int index)
    {
        auto titleStrip = band.reduced (8, 0).withHeight (18).withY (headerCentreY (index, band) - 9);
        removeButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (22).withSizeKeepingCentre (20, 15));
        titleStrip.removeFromRight (6);
        // The on switch keeps one place, left of the remove button, whether
        // the card is folded or open (it used to jump into the controls when
        // switched on); the card's buttons come before it.
        controls[(size_t) index]->on.setBounds (IlanaTheme::cardSwitchBounds (band, headerCentreY (index, band), true));
        titleStrip.removeFromRight (56 + 12);
        loadButton (index).setBounds (titleStrip.removeFromRight (86).withSizeKeepingCentre (86, 15));
        titleStrip.removeFromRight (4);
        if (getMode (index) == 0)
        {
            editButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (44).withSizeKeepingCentre (44, 15));
            titleStrip.removeFromRight (4);
        }
        bounceButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (bounceButtonWide == index ? 92 : 58).withSizeKeepingCentre (bounceButtonWide == index ? 92 : 58, 15));

        if (isFolded (index))
        {
            // Just the switch on the title line; the rest opens when it's on.
            controlBay[(size_t) index] = {};
            chainLabel[(size_t) index] = {};
            return;
        }

        auto content = band.reduced (8);
        content.removeFromTop (20);
        waveDisplay (index).setBounds (content.removeFromLeft (276));
        content.removeFromLeft (8);

        auto topRow = content.removeFromTop (38);
        content.removeFromTop (3);
        chainLabel[(size_t) index] = {};

        if (showsWarpChain (index))
        {
            auto chainRow = content.removeFromBottom (warpChainExtra).withTrimmedTop (4);
            chainBay[(size_t) index] = chainRow.expanded (4, 0);
            auto& chainControls = *controls[(size_t) index];
            chainLabel[(size_t) index] = chainRow.removeFromLeft (120).reduced (8, 6);
            layoutSlots (chainRow, { &chainControls.warp2, &chainControls.warp2Amt, &chainControls.pdEnv, &chainControls.pdEnvAmt });
        }

        auto bottomRow = content;

        const auto mode = getMode (index);
        const auto isSample = mode == 2;
        const auto isString = mode == 1;
        const auto isWavetable = mode == 0;
        const auto isGranular = mode == 3;


        std::vector<juce::Component*> top;
        std::vector<juce::Component*> bottom;

        const auto addTop = [&top] (juce::Component* item) { if (item != nullptr) top.push_back (item); };
        const auto addBottom = [&bottom] (juce::Component* item) { if (item != nullptr) bottom.push_back (item); };

        auto& osc = *controls[(size_t) index];

        if (isGranular)
        {
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            if (IlanaSynthAudioProcessor::isEffectBuild)
                layoutSlots (topRow, { &osc.mode, &osc.grainLive, &osc.sampleTuned, &osc.sampleReverse,
                                       &osc.uniMode, &osc.chord, &osc.ampEnv });
            else
                layoutSlots (topRow, { &osc.mode, &osc.sampleTuned, &osc.sampleReverse,
                                       &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (bottomRow, { &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                                      &osc.grainSpray, &osc.grainPitch, &osc.grainSpread,
                                      &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune });
            return;
        }

        if (mode == 4)
        {
            // M7.5 Live: the input itself, so no pitch or shape controls.
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.ampEnv });
            layoutSlots (bottomRow, { &osc.level, &osc.pan, nullptr, nullptr, nullptr, nullptr });
            return;
        }

        if (isString && isElectric (index))
        {
            // M7.3 Tine / Reed: the pickup and the hammer; the string's own
            // controls don't apply.
            auto& physicalControls = *physical[(size_t) index];
            auto middleRow = bottomRow.removeFromTop (bottomRow.getHeight() / 2);
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.excite, &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (middleRow, { &osc.stringDecay, &osc.stringDamp, &physicalControls.epDistance,
                                      &physicalControls.epPosition, &physicalControls.hammer, &physicalControls.damper });
            layoutSlots (bottomRow, { &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune, &osc.uniBlend, &osc.spread });
            return;
        }

        if (isString)
        {
            auto& physicalControls = *physical[(size_t) index];
            auto middleRow = bottomRow.removeFromTop (bottomRow.getHeight() / 3);
            auto extraRow = bottomRow.removeFromTop (bottomRow.getHeight() / 2);
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.excite, &physicalControls.slap,
                                   &osc.uniMode, &osc.chord, &osc.ampEnv });
            // The string's controls that apply to this exciter flow left to
            // right over two rows on the same eight columns as the row below,
            // so no row starts with holes where hidden controls would be.
            const auto feedbackExcite = processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_excite")->load() == 10.0f;
            std::vector<juce::Component*> flow;

            for (juce::Component* control : { (juce::Component*) &osc.stringDecay, (juce::Component*) &osc.stringDamp,
                                              (juce::Component*) &osc.stringSustain, (juce::Component*) &physicalControls.stiffness,
                                              (juce::Component*) &physicalControls.pickup, (juce::Component*) &physicalControls.excitePos,
                                              (juce::Component*) &physicalControls.hardness, (juce::Component*) &physicalControls.pickPos,
                                              (juce::Component*) &physicalControls.hammer,
                                              feedbackExcite ? (juce::Component*) &physicalControls.fbGain : (juce::Component*) &physicalControls.bowPressure,
                                              feedbackExcite ? (juce::Component*) &physicalControls.fbDistance : (juce::Component*) &physicalControls.bowSpeed,
                                              (juce::Component*) &physicalControls.bridgeBuzz, (juce::Component*) &physicalControls.fretRattle,
                                              (juce::Component*) &physicalControls.couple, (juce::Component*) &physicalControls.damper,
                                              (juce::Component*) &physicalControls.registerMap })
                if (control->isVisible())
                    flow.push_back (control);

            std::vector<juce::Component*> first (8, nullptr), second (8, nullptr);

            for (size_t i = 0; i < flow.size() && i < 16; ++i)
                (i < 8 ? first[i] : second[i - 8]) = flow[i];

            layoutSlots (middleRow, first);
            layoutSlots (extraRow, second);
            layoutSlots (bottomRow, { &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune, &osc.uniBlend, &osc.spread });
            return;
        }

        addTop (&osc.mode);
        addTop (isSample ? (juce::Component*) &osc.sampleTuned
                         : (isString ? (juce::Component*) &osc.excite : (juce::Component*) &osc.table));
        addTop (isSample ? (juce::Component*) &osc.sampleLoop : nullptr);
        addTop (isSample ? (juce::Component*) &osc.sampleReverse : nullptr);
        addTop (isWavetable ? (juce::Component*) &osc.warp : nullptr);
        addTop (isWavetable ? (juce::Component*) &osc.spectral : nullptr);
        addTop (&osc.uniMode);
        addTop (&osc.chord);
        addTop (&osc.ampEnv);

        addBottom (isSample ? (juce::Component*) &osc.sampleStart
                            : (isString ? (juce::Component*) &osc.stringDecay : (juce::Component*) &osc.frame));
        addBottom (isWavetable ? (juce::Component*) &osc.warpAmt : nullptr);
        addBottom (isWavetable ? (juce::Component*) &osc.spectralAmt : nullptr);
        addBottom (isSample ? (juce::Component*) &osc.sampleEnd
                            : (isString ? (juce::Component*) &osc.stringDamp : nullptr));
        addBottom (isSample ? (juce::Component*) &osc.sampleFadeIn
                            : (isString ? (juce::Component*) &osc.stringSustain : nullptr));
        addBottom (isSample ? (juce::Component*) &osc.sampleFadeOut : nullptr);
        addBottom (&osc.level);
        addBottom (&osc.pan);
        addBottom (&osc.semi);
        addBottom (&osc.fine);
        addBottom (&osc.unison);
        addBottom (&osc.detune);
        addBottom (&osc.uniBlend);
        addBottom (&osc.spread);

        controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
        layoutSlots (topRow, top);
        layoutSlots (bottomRow, bottom);
    }

    void setupLoadButton (juce::TextButton& button, const juce::String& tableId, int tableChoiceOffset)
    {
        button.setTooltip ("Load a wavetable (.wav of single-cycle frames), or turn any recording into a wavetable");
        button.onClick = [this, &button, tableId, tableChoiceOffset]
        {
            if (chooserOpen)
                return;

            juce::PopupMenu menu;
            menu.addItem (1, "Load wavetable file...");
            menu.addItem (2, "Make a wavetable from any audio...");
            menu.addSeparator();
            menu.addItem (3, "(Any audio: the pitch is detected and one cycle per frame is taken across the file)", false);

            juce::Component::SafePointer<OscPage> safeMenu (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                                [safeMenu, tableId, tableChoiceOffset] (int result)
                                {
                                    if (safeMenu != nullptr && (result == 1 || result == 2))
                                        safeMenu->chooseTable (tableId, tableChoiceOffset,
                                                               result == 2 ? Wavetable::LoadMode::Resynthesize
                                                                           : Wavetable::LoadMode::Automatic);
                                });
        };
    }

    void chooseTable (const juce::String& tableId, int tableChoiceOffset, Wavetable::LoadMode mode)
    {
        chooserOpen = true;

        for (int index = 0; index < OscillatorIds::count; ++index)
            loadButton (index).setEnabled (false);

        if (tableChooser == nullptr)
            tableChooser = std::make_unique<juce::FileChooser> (
                "Load Wavetable or Audio",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

        juce::Component::SafePointer<OscPage> safeThis (this);

        tableChooser->launchAsync (juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis, tableId, tableChoiceOffset, mode] (const juce::FileChooser& chooser)
                                   {
                                       if (safeThis == nullptr)
                                           return;

                                       safeThis->chooserOpen = false;
                                       safeThis->updateEnabled();

                                       const auto file = chooser.getResult();

                                       if (! file.existsAsFile())
                                           return;

                                       const auto factoryCount = TableFactory::getNumFactoryTables();
                                       const auto domain = juce::jmax (0, safeThis->readTableChoiceIndex (tableId) - tableChoiceOffset);
                                       const auto slot = domain >= factoryCount
                                                             ? juce::jlimit (0, IlanaSynthAudioProcessor::numUserSlots - 1,
                                                                             domain - factoryCount)
                                                             : 0;

                                       if (safeThis->processorRef.loadUserWavetable (slot, file, mode))
                                       {
                                           if (auto* parameter = safeThis->processorRef.apvts.getParameter (tableId))
                                               parameter->setValueNotifyingHost (
                                                   parameter->convertTo0to1 ((float) (tableChoiceOffset + factoryCount + slot)));
                                       }
                                   });
    }

    // Every child component that belongs to oscillator i.
    std::vector<juce::Component*> componentsOf (int i)
    {
        auto& osc = *controls[(size_t) i];
        auto& phys = *physical[(size_t) i];
        return { &osc.on, &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.mode, &osc.table,
                 &osc.excite, &osc.chord, &osc.ampEnv, &osc.warp, &osc.uniMode, &osc.spectral, &osc.frame,
                 &osc.level, &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune, &osc.spread,
                 &osc.stringDecay, &osc.stringDamp, &osc.stringSustain, &osc.sampleStart, &osc.sampleEnd,
                 &osc.sampleFadeIn, &osc.sampleFadeOut, &osc.warpAmt, &osc.uniBlend, &osc.spectralAmt,
                 &osc.grainPosition, &osc.grainSize, &osc.grainDensity, &osc.grainSpray, &osc.grainPitch,
                 &osc.grainSpread, &osc.grainLive, &osc.warp2, &osc.pdEnv, &osc.warp2Amt, &osc.pdEnvAmt, &phys.stiffness, &phys.pickup, &phys.excitePos, &phys.hardness,
                 &phys.pickPos, &phys.bowPressure, &phys.bowSpeed, &phys.bridgeBuzz, &phys.fretRattle,
                 &phys.hammer, &phys.couple, &phys.damper, &phys.registerMap, &phys.slap,
                 &phys.epDistance, &phys.epPosition, &phys.fbGain, &phys.fbDistance, &waveDisplay (i), &loadButton (i), editButtons[(size_t) i].get(),
                 removeButtons[(size_t) i].get(), bounceButtons[(size_t) i].get() };
    }

    void updateModeVisibility()
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            shown[(size_t) i] = processorRef.isOscillatorShown (i);

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            for (auto* component : componentsOf (i))
                component->setVisible (shown[(size_t) i]);

            if (! shown[(size_t) i])
                continue;

            // Keep one oscillator on the page.
            removeButtons[(size_t) i]->setVisible (numShown() > 1);

            if (isFolded (i))
            {
                for (auto* component : componentsOf (i))
                    if (component != &controls[(size_t) i]->on && component != removeButtons[(size_t) i].get())
                        component->setVisible (false);

                continue;
            }

            const auto mode = getMode (i);
            const auto stringVisible = mode == 1;
            auto& physicalControls = *physical[(size_t) i];

            for (juce::Component* control : { (juce::Component*) &physicalControls.stiffness,
                                              (juce::Component*) &physicalControls.pickup,
                                              (juce::Component*) &physicalControls.excitePos,
                                              (juce::Component*) &physicalControls.hardness,
                                              (juce::Component*) &physicalControls.pickPos,
                                              (juce::Component*) &physicalControls.slap,
                                              (juce::Component*) &physicalControls.bridgeBuzz,
                                              (juce::Component*) &physicalControls.fretRattle,
                                              (juce::Component*) &physicalControls.couple,
                                              (juce::Component*) &physicalControls.damper,
                                              (juce::Component*) &physicalControls.registerMap })
                control->setVisible (stringVisible);

            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            const auto bow = stringVisible
                             && processorRef.apvts.getRawParameterValue (prefix + "_excite")->load() == 4.0f;
            physicalControls.bowPressure.setVisible (bow);
            // M8.5: the Feedback exciter's amp; SUSTAIN is its FEEDBACK.
            const auto feedbackExcite = stringVisible && processorRef.apvts.getRawParameterValue (prefix + "_excite")->load() == 10.0f;
            physicalControls.fbGain.setVisible (feedbackExcite);
            physicalControls.fbDistance.setVisible (feedbackExcite);
            controls[(size_t) i]->stringSustain.setLabelText (feedbackExcite ? "FEEDBACK" : "SUSTAIN");
            physicalControls.bowSpeed.setVisible (bow);
            const auto electric = stringVisible && isElectric (i);
            const auto exciteChoice = processorRef.apvts.getRawParameterValue (prefix + "_excite")->load();
            physicalControls.hammer.setVisible (electric || (stringVisible && (exciteChoice == 5.0f || exciteChoice == 9.0f)));
            physicalControls.epDistance.setVisible (electric);
            physicalControls.epPosition.setVisible (electric);
            // M8.2: the Piano exciter's hammer and strings are physical; the
            // pick, pickup and buzz controls don't apply.
            if (stringVisible && exciteChoice == 9.0f)
                for (juce::Component* control : { (juce::Component*) &physicalControls.pickup,
                                                  (juce::Component*) &physicalControls.hardness,
                                                  (juce::Component*) &physicalControls.pickPos,
                                                  (juce::Component*) &physicalControls.slap,
                                                  (juce::Component*) &physicalControls.bridgeBuzz,
                                                  (juce::Component*) &physicalControls.fretRattle })
                    control->setVisible (false);
            if (electric)
                for (juce::Component* control : { (juce::Component*) &physicalControls.stiffness,
                                                  (juce::Component*) &physicalControls.pickup,
                                                  (juce::Component*) &physicalControls.excitePos,
                                                  (juce::Component*) &physicalControls.hardness,
                                                  (juce::Component*) &physicalControls.pickPos,
                                                  (juce::Component*) &physicalControls.slap,
                                                  (juce::Component*) &physicalControls.bridgeBuzz,
                                                  (juce::Component*) &physicalControls.fretRattle,
                                                  (juce::Component*) &physicalControls.couple,
                                                  (juce::Component*) &physicalControls.registerMap })
                    control->setVisible (false);

            auto& osc = *controls[(size_t) i];
            editButtons[(size_t) i]->setVisible (mode == 0);
            osc.table.setVisible (mode == 0);
            osc.frame.setVisible (mode == 0);
            osc.excite.setVisible (mode == 1);
            osc.stringDecay.setVisible (mode == 1);
            osc.stringDamp.setVisible (mode == 1);
            osc.stringSustain.setVisible (mode == 1 && ! isElectric (i));
            osc.sampleTuned.setVisible (mode >= 2);
            osc.sampleLoop.setVisible (mode == 2);
            osc.sampleReverse.setVisible (mode >= 2);
            osc.sampleStart.setVisible (mode == 2);
            osc.sampleEnd.setVisible (mode == 2);
            osc.sampleFadeIn.setVisible (mode == 2);
            osc.sampleFadeOut.setVisible (mode == 2);
            osc.warp.setVisible (mode == 0);
            osc.warpAmt.setVisible (mode == 0);
            osc.spectral.setVisible (mode == 0);
            osc.spectralAmt.setVisible (mode == 0);
            const auto chain = showsWarpChain (i);
            osc.warp2.setVisible (chain);
            osc.warp2Amt.setVisible (chain);
            osc.pdEnv.setVisible (chain);
            osc.pdEnvAmt.setVisible (chain);
            osc.grainPosition.setVisible (mode == 3);
            osc.grainSize.setVisible (mode == 3);
            osc.grainDensity.setVisible (mode == 3);
            osc.grainSpray.setVisible (mode == 3);
            osc.grainPitch.setVisible (mode == 3);
            osc.grainSpread.setVisible (mode == 3);
            osc.grainLive.setVisible (mode == 3 && IlanaSynthAudioProcessor::isEffectBuild);
            osc.uniBlend.setVisible (mode != 3);
            osc.spread.setVisible (mode != 3);
            // M7.5 Live: the input has no pitch, unison or table to show.
            if (mode == 4)
                for (auto* control : std::initializer_list<juce::Component*> { &osc.semi, &osc.fine, &osc.unison, &osc.detune,
                                                                               &osc.uniBlend, &osc.spread, &osc.uniMode, &osc.chord,
                                                                               &loadButton (i) })
                    control->setVisible (false);
        }

        const auto symOnNow = readBool ("sym_on");
        const auto manual = symOnNow && readBool ("sym_manual");

        for (auto* control : { (juce::Component*) &symAmount, (juce::Component*) &symDecay,
                               (juce::Component*) &symCount, (juce::Component*) &symManual })
            control->setVisible (symOnNow);

        for (auto& note : symNotes)
            note->setVisible (manual);

        resized();
        repaint();
        if (onModeChanged != nullptr)
            onModeChanged();
    }

    // M7.3: the Tine and Reed excites (7, 8) have their own controls.
    bool isElectric (int index) const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
        const auto excite = processorRef.apvts.getRawParameterValue (prefix + "_excite")->load();
        return excite == 7.0f || excite == 8.0f;
    }

    static void setGroupEnabled (std::initializer_list<juce::Component*> controls, bool enabled)
    {
        for (auto* control : controls)
        {
            control->setEnabled (enabled);
            control->setAlpha (enabled ? 1.0f : 0.3f);
        }
    }

    void updateEnabled()
    {
        // The sub's controls follow its switch; noise has its own level.
        const auto subIsOn = readBool ("subosc_on");
        for (auto* control : { static_cast<juce::Component*> (&subShape), static_cast<juce::Component*> (&subOctave),
                               static_cast<juce::Component*> (subOscLevel.get()) })
            if (control != nullptr && control->getAlpha() != (subIsOn ? 1.0f : 0.4f))
                control->setAlpha (subIsOn ? 1.0f : 0.4f);

        const auto boardOn = readBool ("sb_on");
        for (auto* control : { &sbMix, &sbTone, &sbSize })
            control->setAlpha (boardOn ? 1.0f : 0.4f);

        // Manual notes past STRINGS are not sounding.
        const auto stringCount = juce::roundToInt (processorRef.apvts.getRawParameterValue ("sym_count")->load());
        for (int i = 0; i < (int) symNotes.size(); ++i)
            symNotes[(size_t) i]->setAlpha (i < stringCount ? 1.0f : 0.4f);

        for (int index = 0; index < OscillatorIds::count; ++index)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            const auto enabled = readBool (prefix + "_on");
            auto& physicalControls = *physical[(size_t) index];
            setGroupEnabled ({ &physicalControls.stiffness, &physicalControls.pickup,
                               &physicalControls.excitePos, &physicalControls.hardness,
                               &physicalControls.pickPos, &physicalControls.slap,
                               &physicalControls.bowPressure, &physicalControls.bowSpeed,
                               &physicalControls.bridgeBuzz, &physicalControls.fretRattle,
                               &physicalControls.hammer, &physicalControls.couple,
                               &physicalControls.damper, &physicalControls.registerMap,
                               &physicalControls.epDistance, &physicalControls.epPosition }, enabled);

            auto& osc = *controls[(size_t) index];
            setGroupEnabled ({ &osc.mode, &osc.table, &osc.excite, &osc.frame, &osc.level,
                               &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune,
                               &osc.spread, &osc.stringDecay, &osc.stringDamp, &osc.stringSustain,
                               &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse,
                               &osc.sampleStart, &osc.sampleEnd, &osc.sampleFadeIn, &osc.sampleFadeOut,
                               &osc.chord, &osc.warp, &osc.warpAmt, &osc.spectral, &osc.spectralAmt,
                               &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                               &osc.grainSpray, &osc.grainPitch, &osc.grainSpread,
                               &osc.uniMode, &osc.uniBlend, &osc.ampEnv,
                               &osc.warp2, &osc.warp2Amt, &osc.pdEnv, &osc.pdEnvAmt }, enabled);

            // An amount whose stage or envelope is Off does nothing: dim it.
            if (enabled)
            {
                osc.warp2Amt.setAlpha (readChoice (prefix + "_warp2") > 0 ? 1.0f : 0.4f);
                osc.pdEnvAmt.setAlpha (readChoice (prefix + "_pd_env") > 0 ? 1.0f : 0.4f);
            }

            const auto alpha = enabled ? 1.0f : 0.3f;
            waveDisplay (index).setAlpha (alpha);
            waveDisplay (index).setEnabled (enabled);
            loadButton (index).setEnabled (! chooserOpen && enabled);
            loadButton (index).setAlpha (alpha);
            editButtons[(size_t) index]->setEnabled (enabled);
            editButtons[(size_t) index]->setAlpha (alpha);
        }
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

public:
    // M7.4: EDIT. A user table is edited in place; a factory table is first
    // copied into a free patch table, which the oscillator then plays.
    // M8.6: the BOUNCE menu. The choices stay set for the next bounce.
    void showBounceMenu (int index)
    {
        if (bouncingOsc >= 0)
            return;
        juce::PopupMenu menu;
        menu.addSectionHeader ("Bounce the patch into OSC " + juce::String (index + 1));
        menu.addItem (1, "As a sample (tuned, one note)");
        menu.addItem (2, "As a wavetable (cut into single cycles)");
        menu.addSeparator();
        menu.addItem (3, "Include the effects", true, bounceRequest.withFx);
        menu.addItem (4, "Mute the other oscillators", true, bounceRequest.muteOthers);
        juce::PopupMenu notes, lengths;
        for (int note : { 36, 48, 60, 72 })
            notes.addItem (100 + note, juce::MidiMessage::getMidiNoteName (note, true, true, 4), true, bounceRequest.note == note);
        for (double hold : { 0.5, 1.0, 2.0, 4.0, 8.0 })
            lengths.addItem (300 + (int) (hold * 2.0), juce::String (hold, hold < 1.0 ? 1 : 0) + " s held + 2 s release",
                             true, std::abs (bounceRequest.holdSeconds - hold) < 1.0e-3);
        menu.addSubMenu ("Note", notes);
        menu.addSubMenu ("Length", lengths);

        juce::Component::SafePointer<OscPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bounceButtons[(size_t) index].get()),
                            [safe, index] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;
                                auto& request = safe->bounceRequest;
                                if (result == 1 || result == 2)
                                {
                                    request.targetOsc = index;
                                    request.toTable = result == 2;
                                    request.tailSeconds = 2.0;
                                    if (safe->processorRef.startBounce (request))
                                    {
                                        safe->bouncingOsc = index;
                                        safe->updateBounce();
                                    }
                                    return;
                                }
                                if (result == 3) request.withFx = ! request.withFx;
                                if (result == 4) request.muteOthers = ! request.muteOthers;
                                if (result >= 100 && result < 300) request.note = result - 100;
                                if (result >= 300) request.holdSeconds = (result - 300) / 2.0;
                                safe->showBounceMenu (index); // keep choosing
                            });
    }

    void updateBounce()
    {
        const auto state = processorRef.getBounceState();
        auto& button = *bounceButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, bouncingOsc)];
        if (state == IlanaSynthAudioProcessor::BounceState::Rendering)
        {
            button.setButtonText ("BOUNCING " + juce::String (juce::roundToInt (processorRef.getBounceProgress() * 100.0f)) + "%");
            if (bounceButtonWide != bouncingOsc)
            {
                bounceButtonWide = bouncingOsc;
                resized();
            }
            for (auto& other : bounceButtons)
                other->setEnabled (false);
            return;
        }
        button.setButtonText ("BOUNCE");
        for (auto& other : bounceButtons)
            other->setEnabled (true);
        bouncingOsc = -1;
        bounceButtonWide = -1;
        resized();
        const auto message = processorRef.getBounceMessage();
        if (state == IlanaSynthAudioProcessor::BounceState::Failed)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Bounce", message);
        else
            button.setTooltip (message);
        updateModeVisibility();
        updateEnabled();
    }

    void openTableEditor (int index)
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) index]) + "_table";
        const auto factoryCount = TableFactory::getNumFactoryTables();
        const auto choice = readTableChoiceIndex (id);
        auto slot = choice - factoryCount;

        if (slot < 0)
        {
            slot = processorRef.findFreeUserSlot();
            if (slot < 0)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Wavetable editor",
                                                        "All 16 patch tables are in use. Pick a User table to edit it.");
                return;
            }
            auto doc = WavetableDoc::fromFactory (choice);
            doc.name << " edit";
            processorRef.setUserTable (slot, doc);
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (factoryCount + slot)));
                parameter->endChangeGesture();
            }
        }

        if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
            editor->openWavetableEditor (slot, oscColour (index));
    }

private:
    int readTableChoiceIndex (const juce::String& tableId) const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId)))
            return param->getIndex();

        return 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waveDisplays;
    std::array<juce::Rectangle<int>, OscillatorIds::count> controlBay {};
    std::array<juce::Rectangle<int>, OscillatorIds::count> chainLabel {}, chainBay {};

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, removeButtons, editButtons, bounceButtons;
    IlanaSynthAudioProcessor::BounceRequest bounceRequest;
    int bouncingOsc = -1, bounceButtonWide = -1;
    juce::TextButton addButton;
    std::unique_ptr<juce::FileChooser> tableChooser;
    std::array<std::unique_ptr<PhysicalControls>, OscillatorIds::count> physical;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<KnobControl> voiceSpread, unisonRandom, drift;
    juce::Rectangle<int> voiceStrip;
    juce::Rectangle<int> symCard;
    ToggleControl symOn, symManual;
    KnobControl symAmount, symDecay, symCount;

    // Acoustic keys (M4): soundboard, stretch tuning, pedal resonance and
    // the mechanism's noises, shared by every voice.
    juce::Rectangle<int> keysCard;
    ToggleControl sbOn;
    ComboControl sbModel;
    KnobControl sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal;
    std::array<std::unique_ptr<KnobControl>, 6> symNotes;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<KnobControl> subOscLevel, noiseStrip;
    juce::Rectangle<int> subStrip;

    std::array<std::unique_ptr<OscControls>, OscillatorIds::count> controls;
    ComboControl subShape, subOctave;
    KnobControl noiseLevel;
};

class OscPageViewport : public juce::Viewport
{
public:
    explicit OscPageViewport (IlanaSynthAudioProcessor& processor)
    {
        setScrollBarsShown (true, false);
        auto* page = new OscPage (processor);
        page->onModeChanged = [this] { resized(); };
        setViewedComponent (page, true);
    }

    void resized() override
    {
        juce::Viewport::resized();
        if (auto* page = dynamic_cast<OscPage*> (getViewedComponent()))
        {
            // Scroll only when the cards cannot fit at their minimum height.
            page->setAvailableHeight (getHeight());
            const auto needed = page->getMinimumHeight();
            const auto scrolls = needed > getHeight();
            page->setSize (juce::jmax (1, getWidth() - (scrolls ? getScrollBarThickness() : 0)),
                           juce::jmax (getHeight(), needed));
        }
    }
};
} // namespace
