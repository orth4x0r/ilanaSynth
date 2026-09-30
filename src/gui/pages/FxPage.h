// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class FxPage : public juce::Component,
               private IlanaAnim::FrameTimer
{
public:
    explicit FxPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          tapGrid (p),
          ampMode (p.apvts, "fx_amp_mode", "MODE"),
          ampDrive (p.apvts, "fx_amp_drive", "DRIVE"),
          ampBass (p.apvts, "fx_amp_bass", "BASS"),
          ampMid (p.apvts, "fx_amp_mid", "MID"),
          ampTreble (p.apvts, "fx_amp_treble", "TREBLE"),
          ampLevel (p.apvts, "fx_amp_level", "LEVEL"),
          driveOn (p.apvts, "fx_drive_on", "ON"),
          driveAmount (p.apvts, "fx_drive_amount", "AMOUNT"),
          driveMix (p.apvts, "fx_drive_mix", "MIX"),
          foldAmount (p.apvts, "fx_fold", "FOLD"),
          crushOn (p.apvts, "fx_crush_on", "ON"),
          crushBits (p.apvts, "fx_crush_bits", "BITS"),
          crushDown (p.apvts, "fx_crush_down", "DOWN"),
          crushMix (p.apvts, "fx_crush_mix", "MIX"),
          compThreshold (p.apvts, "fx_comp_threshold", "THRESH"),
          compRatio (p.apvts, "fx_comp_ratio", "RATIO"),
          compAttack (p.apvts, "fx_comp_attack", "ATTACK"),
          compRelease (p.apvts, "fx_comp_release", "RELEASE"),
          compMakeup (p.apvts, "fx_comp_makeup", "MAKEUP"),
          compMix (p.apvts, "fx_comp_mix", "MIX"),
          combOn (p.apvts, "fx_comb_on", "ON"),
          combFreq (p.apvts, "fx_comb_freq", "FREQ"),
          combFeedback (p.apvts, "fx_comb_feedback", "FEEDBACK"),
          combMix (p.apvts, "fx_comb_mix", "MIX"),
          phaserOn (p.apvts, "fx_phaser_on", "ON"),
          phaserRate (p.apvts, "fx_phaser_rate", "RATE"),
          phaserDepth (p.apvts, "fx_phaser_depth", "DEPTH"),
          phaserFeedback (p.apvts, "fx_phaser_feedback", "FEEDBACK"),
          phaserMix (p.apvts, "fx_phaser_mix", "MIX"),
          chorusOn (p.apvts, "fx_chorus_on", "ON"),
          chorusRate (p.apvts, "fx_chorus_rate", "RATE"),
          chorusDepth (p.apvts, "fx_chorus_depth", "DEPTH"),
          chorusMix (p.apvts, "fx_chorus_mix", "MIX"),
          haasDelay (p.apvts, "fx_haas_delay", "DELAY MS"),
          haasMix (p.apvts, "fx_haas_mix", "WIDTH"),
          delayOn (p.apvts, "fx_delay_on", "ON"),
          delayTime (p.apvts, "fx_delay_time", "TIME"),
          delaySync (p.apvts, "fx_delay_sync", "SYNC"),
          delayDiv (p.apvts, "fx_delay_div", "DIV"),
          delayFeedback (p.apvts, "fx_delay_feedback", "FEEDBACK"),
          delayDamping (p.apvts, "fx_delay_damping", "DAMPING"),
          delayMix (p.apvts, "fx_delay_mix", "MIX"),
          delayPitch (p.apvts, "fx_delay_pitch", "TAPE PITCH"),
          delayWow (p.apvts, "fx_delay_wow", "TAPE WOW"),
          delayPingPong (p.apvts, "fx_delay_pingpong", "PING-PONG"),
          tapsOn (p.apvts, "fx_taps_on", "TAPS"),
          tapsPattern (p.apvts, "fx_taps_pattern", "PATTERN"),
          tapsMix (p.apvts, "fx_taps_mix", "TAPS MIX"),
          stutterOn (p.apvts, "fx_stutter_on", "ENGAGE"),
          stutterDiv (p.apvts, "fx_stutter_div", "DIV"),
          stutterMix (p.apvts, "fx_stutter_mix", "MIX"),
          smearOn (p.apvts, "fx_smear_on", "ON"),
          smearSize (p.apvts, "fx_smear_size", "GRAIN MS"),
          smearDensity (p.apvts, "fx_smear_density", "DENSITY"),
          smearMix (p.apvts, "fx_smear_mix", "MIX"),
          freezeOn (p.apvts, "fx_freeze_on", "HOLD"),
          freezeMix (p.apvts, "fx_freeze_mix", "MIX"),
          reverbOn (p.apvts, "fx_reverb_on", "ON"),
          reverbType (p.apvts, "fx_reverb_type", "ALGORITHM"),
          reverbSize (p.apvts, "fx_reverb_size", "SIZE"),
          reverbDamping (p.apvts, "fx_reverb_damping", "DAMPING"),
          reverbWidth (p.apvts, "fx_reverb_width", "WIDTH"),
          reverbMix (p.apvts, "fx_reverb_mix", "MIX"),
          flangerRate (p.apvts, "fx_flanger_rate", "RATE"),
          flangerDepth (p.apvts, "fx_flanger_depth", "DEPTH"),
          flangerFeedback (p.apvts, "fx_flanger_feedback", "FEEDBACK"),
          flangerMix (p.apvts, "fx_flanger_mix", "MIX"),
          dimRate (p.apvts, "fx_dim_rate", "RATE"),
          dimDepth (p.apvts, "fx_dim_depth", "DEPTH"),
          dimMix (p.apvts, "fx_dim_mix", "MIX"),
          gateDiv (p.apvts, "fx_gate_div", "DIV"),
          gatePattern (p.apvts, "fx_gate_pattern", "PATTERN"),
          gateSteps (p.apvts, "fx_gate_steps", "STEPS"),
          gateSwing (p.apvts, "fx_gate_swing", "SWING"),
          gateSmooth (p.apvts, "fx_gate_smooth", "SMOOTH"),
          gateMix (p.apvts, "fx_gate_mix", "MIX"),
          tapeStopTrigger (p.apvts, "fx_tape_stop_trigger", "STOP"),
          tapeStopTime (p.apvts, "fx_tape_stop_time", "TIME"),
          tapeStopMix (p.apvts, "fx_tape_stop_mix", "MIX"),
          tiltAmount (p.apvts, "fx_tilt", "TILT"),
          tiltLevel (p.apvts, "fx_tilt_level", "LEVEL"),
          utilGain (p.apvts, "fx_util_gain", "GAIN"),
          utilMono (p.apvts, "fx_util_mono", "MONO"),
          utilInvert (p.apvts, "fx_util_invert", "INVERT"),
          ottAmount (p.apvts, "fx_ott_amount", "AMOUNT"),
          ottMix (p.apvts, "fx_ott_mix", "MIX"),
          limitCeiling (p.apvts, "fx_limit_ceiling", "CEILING"),
          limitRelease (p.apvts, "fx_limit_release", "RELEASE"),
          widthAmount (p.apvts, "fx_width", "WIDTH"),
          widthMix (p.apvts, "fx_width_mix", "MIX"),
          tremRate (p.apvts, "fx_trem_rate", "RATE"),
          tremDepth (p.apvts, "fx_trem_depth", "DEPTH"),
          tremShape (p.apvts, "fx_trem_shape", "SHAPE"),
          shifterShift (p.apvts, "fx_shifter_shift", "SHIFT HZ"),
          shifterMix (p.apvts, "fx_shifter_mix", "MIX"),
          ringFreq (p.apvts, "fx_ring_freq", "FREQ"),
          ringMix (p.apvts, "fx_ring_mix", "MIX"),
          octaverMix (p.apvts, "fx_octaver_mix", "MIX"),
          vowelMorph (p.apvts, "fx_vowel_morph", "MORPH"),
          vowelMix (p.apvts, "fx_vowel_mix", "MIX"),
          delayTimeR (p.apvts, "fx_delay_time_r", "TIME R"),
          delayDuck (p.apvts, "fx_delay_duck", "DUCK"),
          stutterReverse (p.apvts, "fx_stutter_reverse", "REVERSE"),
          stutterPitch (p.apvts, "fx_stutter_pitch", "PITCH"),
          feedbackAmount (p.apvts, "fx_feedback_amount", "AMOUNT"),
          feedbackDelay (p.apvts, "fx_feedback_delay", "DELAY MS"),
          feedbackTone (p.apvts, "fx_feedback_tone", "TONE"),
          feedbackMix (p.apvts, "fx_feedback_mix", "MIX"),
          eqLowFreq (p.apvts, "fx_eq_low_freq", "LOW FREQ"), eqLowGain (p.apvts, "fx_eq_low_gain", "LOW GAIN"),
          eqMidFreq (p.apvts, "fx_eq_mid_freq", "MID FREQ"), eqMidGain (p.apvts, "fx_eq_mid_gain", "MID GAIN"),
          eqMidQ (p.apvts, "fx_eq_mid_q", "MID Q"),
          eqHighFreq (p.apvts, "fx_eq_high_freq", "HIGH FREQ"), eqHighGain (p.apvts, "fx_eq_high_gain", "HIGH GAIN"),
          eqCurve (p),
          awAlgo (p.apvts, "fx_aw_algo", "ALGORITHM"),
          awP1 (p.apvts, "fx_aw_p1", "1"), awP2 (p.apvts, "fx_aw_p2", "2"), awP3 (p.apvts, "fx_aw_p3", "3"),
          awP4 (p.apvts, "fx_aw_p4", "4"), awP5 (p.apvts, "fx_aw_p5", "5"),
          awMix (p.apvts, "fx_aw_mix", "MIX"),
          vocSource (p.apvts, "fx_voc_source", "MODULATOR"), vocBands (p.apvts, "fx_voc_bands", "BANDS"),
          vocWidth (p.apvts, "fx_voc_width", "WIDTH"), vocAttack (p.apvts, "fx_voc_attack", "ATTACK"),
          vocRelease (p.apvts, "fx_voc_release", "RELEASE"), vocFormant (p.apvts, "fx_voc_formant", "FORMANT"),
          vocUnvoiced (p.apvts, "fx_voc_unvoiced", "UNVOICED"), vocRate (p.apvts, "fx_voc_rate", "TALK RATE"),
          vocLevel (p.apvts, "fx_voc_level", "LEVEL"), vocMix (p.apvts, "fx_voc_mix", "MIX")
    {
        addAll (*this, eqLowFreq, eqLowGain, eqMidFreq, eqMidGain, eqMidQ, eqHighFreq, eqHighGain);
        addChildComponent (eqCurve);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("fx_slot1")))
            slotNames = choice->getAllValueStrings();

        addAll (*this, ampMode, ampDrive, ampBass, ampMid, ampTreble, ampLevel,
                driveOn, driveAmount, driveMix, foldAmount,
                crushOn, crushBits, crushDown, crushMix,
                compThreshold, compRatio, compAttack, compRelease, compMakeup, compMix,
                combOn, combFreq, combFeedback, combMix,
                phaserOn, phaserRate, phaserDepth, phaserFeedback, phaserMix,
                chorusOn, chorusRate, chorusDepth, chorusMix, haasDelay, haasMix,
                delayOn, delayTime, delaySync, delayDiv, delayFeedback, delayDamping, delayMix,
                delayPitch, delayWow, delayPingPong, tapsOn, tapsPattern, tapsMix,
                stutterOn, stutterDiv, stutterMix,
                smearOn, smearSize, smearDensity, smearMix, freezeOn, freezeMix,
                reverbOn, reverbType, reverbSize, reverbDamping, reverbWidth, reverbMix,
                flangerRate, flangerDepth, flangerFeedback, flangerMix,
                dimRate, dimDepth, dimMix,
                gateDiv, gatePattern, gateSteps, gateSwing, gateSmooth, gateMix,
                tapeStopTrigger, tapeStopTime, tapeStopMix,
                tiltAmount, tiltLevel,
                utilGain, utilMono, utilInvert,
                ottAmount, ottMix,
                limitCeiling, limitRelease,
                widthAmount, widthMix,
                tremRate, tremDepth, tremShape,
                shifterShift, shifterMix,
                ringFreq, ringMix,
                octaverMix,
                vowelMorph, vowelMix,
                delayTimeR, delayDuck,
                stutterReverse, stutterPitch,
                feedbackAmount, feedbackDelay, feedbackTone, feedbackMix);

        slotGroups.push_back ({});
        slotGroups.push_back ({ &ampMode, &ampDrive, &ampBass, &ampMid, &ampTreble, &ampLevel });
        slotGroups.push_back ({ &driveOn, &driveAmount, &driveMix, &foldAmount });
        slotGroups.push_back ({ &crushOn, &crushBits, &crushDown, &crushMix });
        slotGroups.push_back ({ &compThreshold, &compRatio, &compAttack, &compRelease, &compMakeup, &compMix });
        slotGroups.push_back ({ &combOn, &combFreq, &combFeedback, &combMix });
        slotGroups.push_back ({ &phaserOn, &phaserRate, &phaserDepth, &phaserFeedback, &phaserMix });
        slotGroups.push_back ({ &chorusOn, &chorusRate, &chorusDepth, &chorusMix });
        slotGroups.push_back ({ &haasDelay, &haasMix });
        slotGroups.push_back ({ &delayOn, &delayTime, &delayTimeR, &delaySync, &delayDiv, &delayFeedback,
                                &delayDamping, &delayPingPong, &delayMix, &delayPitch, &delayWow, &delayDuck,
                                &tapsOn, &tapsPattern, &tapsMix });
        slotGroups.push_back ({ &stutterOn, &stutterDiv, &stutterMix, &stutterReverse, &stutterPitch });
        slotGroups.push_back ({ &smearOn, &smearSize, &smearDensity, &smearMix });
        slotGroups.push_back ({ &freezeOn, &freezeMix });
        slotGroups.push_back ({ &reverbOn, &reverbType, &reverbSize, &reverbDamping, &reverbWidth, &reverbMix });
        slotGroups.push_back ({ &flangerRate, &flangerDepth, &flangerFeedback, &flangerMix });
        slotGroups.push_back ({ &dimRate, &dimDepth, &dimMix });
        slotGroups.push_back ({ &gateDiv, &gatePattern, &gateSteps, &gateSwing, &gateSmooth, &gateMix });
        slotGroups.push_back ({ &tapeStopTrigger, &tapeStopTime, &tapeStopMix });
        slotGroups.push_back ({ &tiltAmount, &tiltLevel });
        slotGroups.push_back ({ &utilGain, &utilMono, &utilInvert });
        slotGroups.push_back ({ &ottAmount, &ottMix });
        slotGroups.push_back ({ &limitCeiling, &limitRelease });
        slotGroups.push_back ({ &widthAmount, &widthMix });
        slotGroups.push_back ({ &tremRate, &tremDepth, &tremShape });
        slotGroups.push_back ({ &shifterShift, &shifterMix });
        slotGroups.push_back ({ &ringFreq, &ringMix });
        slotGroups.push_back ({ &octaverMix });
        slotGroups.push_back ({ &vowelMorph, &vowelMix });
        slotGroups.push_back ({ &feedbackAmount, &feedbackDelay, &feedbackTone, &feedbackMix });
        slotGroups.push_back ({ &eqLowFreq, &eqLowGain, &eqMidFreq, &eqMidGain, &eqMidQ, &eqHighFreq, &eqHighGain });
        slotGroups.push_back ({ &awAlgo, &awP1, &awP2, &awP3, &awP4, &awP5, &awMix });
        slotGroups.push_back ({ &vocSource, &vocBands, &vocWidth, &vocAttack, &vocRelease, &vocFormant, &vocUnvoiced,
                                &vocRate, &vocLevel, &vocMix });
        // Airwindows: the algorithms grouped by family, not one long list.
        awAlgo.setPopupOverride ([this] { showAirwindowsMenu(); });

        prevSlotButton.onClick = [this] { moveSelectedSlot (-1); };
        nextSlotButton.onClick = [this] { moveSelectedSlot (1); };
        diceButton.onClick = [this] { processorRef.randomizeFxChain(); };
        saveChainButton.onClick = [this] { saveChain(); };
        loadChainButton.onClick = [this] { loadChain(); };
        chainAButton.onClick = [this]
        {
            if (! processorRef.isShowingChainA())
            {
                processorRef.switchFxChain();
                chainABChanged();
            }
        };
        chainBButton.onClick = [this]
        {
            if (processorRef.isShowingChainA())
            {
                processorRef.switchFxChain();
                chainABChanged();
            }
        };
        copyChainButton.onClick = [this]
        {
            processorRef.copyFxChainToOtherBank();
            chainABChanged();
        };
        loadIrButton.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Load Impulse Response (.wav)",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav");

            juce::Component::SafePointer<FxPage> safeThis (this);

            fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                          | juce::FileBrowserComponent::canSelectFiles,
                                      [safeThis] (const juce::FileChooser& chooser)
                                      {
                                          const auto file = chooser.getResult();

                                          if (file.existsAsFile() && safeThis != nullptr)
                                              safeThis->processorRef.loadReverbIr (file);
                                      });
        };

        addAndMakeVisible (prevSlotButton);
        addAndMakeVisible (nextSlotButton);
        addAndMakeVisible (diceButton);
        addAndMakeVisible (saveChainButton);
        addAndMakeVisible (loadChainButton);
        addAndMakeVisible (loadIrButton);
        IlanaTheme::makePill (chainAButton, IlanaTheme::accent());
        IlanaTheme::makePill (chainBButton, IlanaTheme::accent());
        addAndMakeVisible (chainAButton);
        addAndMakeVisible (chainBButton);
        addAndMakeVisible (copyChainButton);
        addAndMakeVisible (tapGrid);
        tapGrid.setVisible (false);

        // The final stage after the rack.
        softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "SOFT CLIP");
        softClip->showAsSwitch();
        clipGain = std::make_unique<StripKnob> (p, "master_clip_gain", "Clip Gain");
        addAndMakeVisible (*softClip);
        addAndMakeVisible (*clipGain);

        slotBlend.setSliderStyle (juce::Slider::LinearHorizontal);
        slotBlend.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        slotBlend.setTooltip ("Parallel blend for the selected slot: 0 is dry only, 1 is the full effect.");
        addAndMakeVisible (slotBlend);

        // Every loaded module's controls live in one scrolling stack.
        for (auto& group : slotGroups)
            for (auto* control : group)
                stackContent.addChildComponent (control);

        gateGrid = std::make_unique<GateGrid> (p);
        stackContent.addChildComponent (*gateGrid);
        stackContent.addChildComponent (tapGrid);
        stackContent.addChildComponent (eqCurve);
        stackContent.addChildComponent (loadIrButton);
        stackContent.painter = [this] (juce::Graphics& g) { paintStack (g); };
        stackContent.onClick = [this] (juce::Point<int> position)
        {
            if (addEffectCard.contains (position))
            {
                if (const auto slot = firstEmptySlot(); slot >= 0)
                    showTypeMenu (slot);

                return;
            }

            for (const auto& panel : stackPanels)
            {
                if (panel.bounds.contains (position))
                {
                    selectedSlot = panel.slot;
                    bindBlend();
                    repaint();
                    stackContent.repaint();
                }
            }
        };

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            slotSwitches[(size_t) slot] = std::make_unique<SlotSwitch> (p, slot);
            stackContent.addChildComponent (*slotSwitches[(size_t) slot]);
        }

        stackView.setViewedComponent (&stackContent, false);
        stackView.setScrollBarsShown (true, false);
        stackView.setScrollBarThickness (8);
        addAndMakeVisible (stackView);

        // Quick picks for an empty rack, every effect grouped by what it
        // does: one click adds it to the first empty slot.
        for (const auto& group : quickAddGroups())
        {
            auto label = std::make_unique<juce::Label> ("", group.title);
            label->setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)));
            label->setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
            label->setJustificationType (juce::Justification::centredRight);
            addChildComponent (*label);
            quickAddLabels.push_back (std::move (label));
        }

        for (const auto& pick : quickAddPicks())
        {
            auto button = std::make_unique<juce::TextButton> (pick.second);
            const auto type = pick.first;
            button->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised.interpolatedWith (fxColour (type), 0.08f));
            button->setColour (juce::TextButton::textColourOffId, fxColour (type).interpolatedWith (juce::Colours::white, 0.35f));
            button->setTooltip ("Add " + juce::String (pick.second).toLowerCase() + " to the first empty slot");
            button->onClick = [this, type]
            {
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                {
                    if (getSlotType (slot) == 0)
                    {
                        processorRef.assignFxSlot (slot + 1, type);
                        selectedSlot = slot;
                        updateVisibility();
                        return;
                    }
                }
            };
            addChildComponent (*button);
            quickAddButtons.push_back (std::move (button));
        }

        updateVisibility();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto type = getSlotType (selectedSlot);

        paintSectionTitle (g, "CHAIN", { headingX, 17, 200, 14 }); // on the toolbar's centre line

        if (! outputStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, outputStrip.toFloat(), 6.0f);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            IlanaTheme::paintCardTitle (g, outputStrip.withWidth (110).withTrimmedLeft (14), "OUTPUT", IlanaTheme::Ui::text2);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            // Read in order after its controls, like a card's subtitle.
            g.drawText ("after the rack, before the master volume",
                        outputStrip.withLeft (clipGain != nullptr ? clipGain->getRight() + 24 : outputStrip.getX()).withTrimmedRight (14),
                        juce::Justification::centredLeft);
        }

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("BLEND", juce::Rectangle<int> (236, 15, 40, 18), juce::Justification::centredRight);

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto row = rowBounds (slot);

            if (row.isEmpty())
                continue;

            const auto selected = slot == selectedSlot;
            const auto slotType = getSlotType (slot);
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            const auto bypassed = isModuleOff (slot);
            const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;
            const auto dragSource = cardDragActive && slot == selectedSlot;
            const auto dim = bypassed ? 0.45f : 1.0f;

            const auto typeColour = fxColour (slotType);

            if (dragSource)
                g.setColour (IlanaTheme::Ui::bg.withAlpha (0.5f));
            else if (slotType != 0)
            {
                // Loaded modules wear their family colour.
                juce::ColourGradient rowGradient (IlanaTheme::Ui::raised.interpolatedWith (typeColour, selected ? 0.2f : 0.1f).withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  IlanaTheme::Ui::raised.interpolatedWith (typeColour, selected ? 0.1f : 0.04f).withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }
            else
            {
                juce::ColourGradient rowGradient (IlanaTheme::Ui::raised.withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  IlanaTheme::Ui::panel.withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }

            g.fillRoundedRectangle (row.toFloat(), 6.0f);

            if (slotType != 0 && ! dragSource)
            {
                g.setColour (typeColour.withAlpha (dim));
                g.fillRoundedRectangle (row.toFloat().withWidth (4.0f).reduced (0.0f, 6.0f).translated (2.0f, 0.0f), 2.0f);
            }

            if (! selected && ! dragSource && rowHover[(size_t) slot] > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f * rowHover[(size_t) slot]));
                g.fillRoundedRectangle (row.toFloat(), 6.0f);
            }
            g.setColour ((selected ? (slotType != 0 ? typeColour : IlanaTheme::accent()) : IlanaTheme::Ui::track).withMultipliedAlpha (dim));
            g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 6.0f, selected ? 1.6f : 1.0f);

            if (dragSource)
                continue;

            g.setColour (juce::Colours::white.withAlpha (0.35f * dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (juce::String (slot + 1), row.reduced (8, 0).removeFromLeft (18), juce::Justification::centredLeft);

            const juce::Rectangle<float> led ((float) row.getRight() - 20.0f, (float) row.getCentreY() - 3.0f, 7.0f, 7.0f);

            if (slotType != 0 && ! bypassed)
            {
                g.setColour (typeColour);
                g.fillEllipse (led);
            }
            else if (slotType != 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawEllipse (led, 1.0f);
            }

            g.setColour ((slotType != 0 ? IlanaTheme::Ui::text
                                        : IlanaTheme::accent().interpolatedWith (juce::Colours::white, 0.55f)
                                              .withAlpha (0.42f + 0.45f * rowHover[(size_t) slot]))
                             .withMultipliedAlpha (dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, slotType != 0));
            // A slot on one band of the signal says which.
            static const char* const bandTags[] { "", "  LOW", "  MID", "  HIGH", "  M", "  S" };
            const auto bandTag = slotType != 0 ? bandTags[juce::jlimit (0, 5, getSlotBand (slot))] : "";
            g.drawFittedText (slotType != 0 ? getSlotName (slotType) + bandTag : juce::String ("+  add effect"),
                              row.reduced (30, 6).withTrimmedRight (18), 1, juce::Justification::centredLeft);

            if (slotType != 0 && ! bypassed)
            {
                const auto cpu = processorRef.getFxSlotCpu (slot);

                if (cpu > 0.0005f)
                {
                    g.setColour (IlanaTheme::Ui::text3);
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                    g.drawText (juce::String (cpu * 100.0f, 1) + "%",
                                juce::Rectangle<int> (row.getRight() - 78, row.getY() + 6, 26, 10),
                                juce::Justification::centredRight);
                }
            }

            if (slotType != 0 && soloed)
            {
                const juce::Rectangle<float> badge ((float) row.getRight() - 52.0f, (float) row.getY() + 6.0f, 16.0f, 10.0f);
                g.setColour (IlanaTheme::accent().withAlpha (0.9f));
                g.fillRoundedRectangle (badge, 2.5f);
                g.setColour (juce::Colours::black.withAlpha (0.85f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("S", badge, juce::Justification::centred);
            }

            // Mini meter strip along the row bottom
            const auto meter = juce::Rectangle<float> ((float) row.getX() + 30.0f, (float) row.getBottom() - 8.0f,
                                                       (float) row.getWidth() - 58.0f, 2.5f);

            if (slotType == 4) // Comp: gain reduction
            {
                const auto reduction = juce::jlimit (0.0f, 1.0f, 1.0f - processorRef.getCompGainReduction());
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (IlanaTheme::accent().withAlpha (0.85f));
                g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * reduction), 1.2f);
            }
            else if (slotType == 9) // Delay: time bar
            {
                const auto division = juce::jlimit (0.0f, 1.0f, processorRef.apvts.getParameter ("fx_delay_div")->getValue());
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * division), 1.2f);
            }
            else if (slotType == 12 || slotType == 10 || slotType == 17) // Freeze / Stutter / TapeStop
            {
                const auto lit = processorRef.apvts.getParameter (slotType == 12 ? "fx_freeze_on"
                                                                                 : (slotType == 10 ? "fx_stutter_on"
                                                                                                   : "fx_tape_stop_trigger"))->getValue() > 0.5f;
                g.setColour (lit ? IlanaTheme::accent() : juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
            }
            else if (slotType == 22) // Widener: stereo correlation
            {
                auto correlation = 0.0f;

                if (updateCorrelation())
                    correlation = lastCorrelation;

                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.fillRect (juce::Rectangle<float> (1.0f, meter.getHeight()).withCentre ({ meter.getCentreX(), meter.getCentreY() }));
                g.setColour (std::abs (correlation) > 0.9f ? juce::Colours::red.withAlpha (0.7f)
                                                           : IlanaTheme::accent().withAlpha (0.8f));
                g.fillRoundedRectangle (juce::Rectangle<float> (meter.getWidth() * 0.5f * correlation,
                                                                meter.getHeight())
                                            .withCentre ({ meter.getCentreX(), meter.getCentreY() }),
                                        1.2f);
            }
            else if (slotType == 6 || slotType == 7 || slotType == 14 || slotType == 15
                     || slotType == 23 || slotType == 11 || slotType == 8 || slotType == 24 || slotType == 25)
            {
                const auto x = meter.getX() + meter.getWidth() * (0.5f + 0.45f * std::sin (meterPhase * 0.5f));
                g.setColour (IlanaTheme::accent().withAlpha (0.75f));
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ x, meter.getCentreY() }));
            }
            else if (slotType != 0)
            {
                g.setColour (IlanaTheme::accent().withAlpha (0.45f * dim));
                g.fillRoundedRectangle (meter, 1.2f);
            }

        // Slot change flash
        if (slot == dropSlot && dropFlash > 0.01f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (dropFlash * 0.5f));
                g.drawRoundedRectangle (row.toFloat().expanded (dropFlash * 4.0f), 8.0f, 2.0f);
            }
        }

        // Chain bank switch sweep
        if (chainSweep > 0.01f)
        {
            const auto top = (float) rowsTop;
            const auto height = (float) (rowHeight * numVisibleRows());
            const auto y = top + (1.0f - chainSweep) * height;
            const auto alpha = chainSweep * 0.8f;

            g.setColour (IlanaTheme::accent().withAlpha (alpha * 0.22f));
            g.fillRoundedRectangle (14.0f, y - 12.0f, 300.0f, 24.0f, 6.0f);
            g.setColour (IlanaTheme::accent().withAlpha (alpha));
            g.fillRect (14.0f, y - 1.0f, 300.0f, 2.0f);
        }

        // Drag ghost
        if (cardDragActive && dragImage.isValid())
        {
            const auto ghost = juce::Rectangle<float> ((float) dragImage.getWidth(),
                                                       (float) dragImage.getHeight())
                                   .withPosition ((float) dragPosition.x - dragGrabOffset.x,
                                                  (float) dragPosition.y - dragGrabOffset.y);

            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (ghost.translated (3.0f, 3.0f), 6.0f);
            g.setOpacity (0.8f);
            g.drawImageAt (dragImage, (int) ghost.getX(), (int) ghost.getY());
            g.setOpacity (1.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.8f));
            g.drawRoundedRectangle (ghost, 6.0f, 1.5f);
        }

        juce::ignoreUnused (type);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("Click a slot to add or jump to it, drag to reorder, right-click to change it.",
                    juce::Rectangle<int> (stackView.getX(), stackView.getY() - 20, 600, 16), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto toolbar = area.removeFromTop (24);
        loadChainButton.setBounds (toolbar.removeFromRight (86).reduced (0, 3));
        toolbar.removeFromRight (4);
        saveChainButton.setBounds (toolbar.removeFromRight (86).reduced (0, 3));
        toolbar.removeFromRight (4);
        diceButton.setBounds (toolbar.removeFromRight (50).reduced (0, 3));
        toolbar.removeFromRight (12);
        nextSlotButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));
        toolbar.removeFromRight (4);
        prevSlotButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));
        toolbar.removeFromRight (12);
        copyChainButton.setBounds (toolbar.removeFromRight (104).reduced (0, 3));
        toolbar.removeFromRight (6);
        chainBButton.setBounds (toolbar.removeFromRight (58).reduced (0, 3));
        toolbar.removeFromRight (4);
        chainAButton.setBounds (toolbar.removeFromRight (58).reduced (0, 3));

        area.removeFromTop (6);

        auto chainColumn = area.removeFromLeft (300);
        rowsTop = chainColumn.getY() + 22; // level with the first effect card (under the hint line)

        area.removeFromLeft (22);

        auto panel = area;
        outputStrip = panel.removeFromBottom (46);
        panel.removeFromBottom (6);
        {
            auto strip = outputStrip.reduced (6, 3);
            strip.removeFromLeft (104); // the tagged OUTPUT title
            softClip->setBounds (strip.removeFromLeft (120));
            strip.removeFromLeft (10);
            clipGain->setBounds (strip.removeFromLeft (160));
        }

        panel.removeFromTop (22); // hint line
        stackView.setBounds (panel);
        layoutStack();
        slotBlend.setBounds (280, 15, 150, 18);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto row = rowBounds (slot);

            if (! row.contains (event.getPosition()))
                continue;

            selectedSlot = slot;
            // A drag reorders among the loaded slots only (fixed for the
            // drag, so it can't walk into the hidden rows).
            dragLimit = juce::jmax (0, numVisibleRows() - 2);
            bindBlend();
            scrollToSlot (slot);
            stackContent.repaint();
            repaint();

            // Empty slots have nothing to bypass or drag, so any click picks a module.
            if (event.mods.isPopupMenu() || getSlotType (slot) == 0)
            {
                showTypeMenu (slot);
                return;
            }

            const juce::Rectangle<int> ledZone (row.getRight() - 28, row.getY(), 26, row.getHeight());

            if (ledZone.contains (event.getPosition()))
            {
                if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
                    parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);

                return;
            }

            const juce::Rectangle<int> soloZone (row.getRight() - 60, row.getY(), 32, row.getHeight());

            if (soloZone.contains (event.getPosition()))
            {
                if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_solo"))
                    parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);

                return;
            }

            dragImage = createComponentSnapshot (row);
            dragGrabOffset = event.getPosition() - juce::Point<int> (row.getX(), row.getY());
            dragPosition = event.getPosition();
            dragReady = true;
            return;
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragReady)
            return;

        dragPosition = event.getPosition();

        if (! cardDragActive && event.getDistanceFromDragStart() > 4)
        {
            cardDragActive = true;

            // The picks would sit over the dragged row's ghost.
            for (auto& button : quickAddButtons)
                button->setVisible (false);

            for (auto& label : quickAddLabels)
                label->setVisible (false);
        }

        if (! cardDragActive)
            return;

        const auto target = juce::jlimit (0, dragLimit, (event.getPosition().y - rowsTop) / rowHeight);

        if (target != selectedSlot)
            moveSelectedSlotTo (target);

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragReady = false;

        if (cardDragActive)
        {
            dropSlot = selectedSlot;
            dropFlash = 1.0f;
        }

        const auto wasDragging = cardDragActive;
        cardDragActive = false;
        dragImage = {};

        if (wasDragging)
            layoutStack();

        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            if (rowBounds (slot).contains (event.getPosition()))
            {
                selectedSlot = slot;
                showTypeMenu (slot);
                return;
            }
        }
    }

private:
    // Rows up to the last loaded slot, plus one to add to: empty slots
    // beyond that are not drawn.
    int numVisibleRows() const
    {
        auto last = -1;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) != 0)
                last = slot;

        return juce::jmin (IlanaSynthAudioProcessor::numFxSlots, last + 2);
    }

    juce::Rectangle<int> rowBounds (int slot) const
    {
        if (slot >= numVisibleRows())
            return {};

        return juce::Rectangle<int> (14, rowsTop + slot * rowHeight, 300, rowHeight - 4);
    }

    bool updateCorrelation()
    {
        if (correlationL.size() < 2048)
        {
            correlationL.assign (2048, 0.0f);
            correlationR.assign (2048, 0.0f);
        }

        processorRef.copyScopeData (correlationL.data(), correlationR.data(), 2048);

        auto sumLR = 0.0;
        auto sumLL = 0.0;
        auto sumRR = 0.0;

        for (int i = 0; i < 2048; ++i)
        {
            sumLR += (double) correlationL[(size_t) i] * (double) correlationR[(size_t) i];
            sumLL += (double) correlationL[(size_t) i] * (double) correlationL[(size_t) i];
            sumRR += (double) correlationR[(size_t) i] * (double) correlationR[(size_t) i];
        }

        const auto denominator = std::sqrt (sumLL * sumRR);

        if (denominator < 1.0e-9)
            return false;

        lastCorrelation = juce::jlimit (-1.0f, 1.0f, (float) (sumLR / denominator));
        return true;
    }

    int getSlotType (int slot) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1)))
            return juce::jlimit (0, juce::jmax (0, slotNames.size() - 1), (int) value->load());

        return 0;
    }

    int getSlotBand (int slot) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_band"))
            return (int) value->load();
        return 0;
    }

    juce::String getSlotName (int type) const
    {
        return juce::isPositiveAndBelow (type, slotNames.size()) ? slotNames[type] : juce::String ("-");
    }

    void showTypeMenu (int slot)
    {
        juce::PopupMenu menu;
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        const auto bypassed = processorRef.apvts.getParameter (prefix + "_bypass")->getValue() > 0.5f;
        const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;

        menu.addItem (1003, "Clear (None)", true, getSlotType (slot) == 0);
        menu.addSeparator();

        for (int i = 1; i < slotNames.size(); ++i)
            menu.addItem (i + 1, slotNames[i], true, getSlotType (slot) == i);

        menu.addSeparator();
        menu.addItem (1001, "Bypass", true, bypassed);
        menu.addItem (1002, "Solo (wet only)", true, soloed);

        // Splitters: the slot works on one band, the rest passes around it.
        juce::PopupMenu bands;
        const juce::StringArray bandNames { "Full signal", "Low band", "Mid band", "High band", "Mid (M/S)", "Side (M/S)" };
        for (int b = 0; b < bandNames.size(); ++b)
            bands.addItem (1100 + b, bandNames[b], true, getSlotBand (slot) == b);
        bands.addSeparator();
        bands.addItem (1110, "Set crossovers...");
        menu.addSubMenu ("Band", bands);

        juce::Component::SafePointer<FxPage> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis, slot] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                const auto slotPrefix = "fx_slot" + juce::String (slot + 1);

                                if (result >= 1 && result <= 1000)
                                {
                                    safeThis->processorRef.assignFxSlot (slot + 1, result - 1);
                                }
                                else if (result == 1001 || result == 1002)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (
                                            slotPrefix + (result == 1001 ? "_bypass" : "_solo")))
                                        parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
                                }
                                else if (result >= 1100 && result < 1110)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (slotPrefix + "_band"))
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1100)));
                                }
                                else if (result == 1110)
                                {
                                    safeThis->showCrossovers();
                                }
                                else if (result == 1003)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (slotPrefix))
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
                                }

                                safeThis->updateVisibility();
                                safeThis->repaint();
                            });
    }

    // The splitters' two crossovers, as knobs in a callout.
    void showCrossovers()
    {
        struct Crossovers : public juce::Component
        {
            explicit Crossovers (juce::AudioProcessorValueTreeState& state)
                : low (state, "fx_split_low", "LOW / MID", IlanaTheme::accent(), false),
                  high (state, "fx_split_high", "MID / HIGH", IlanaTheme::accent(), false)
            {
                addAndMakeVisible (low);
                addAndMakeVisible (high);
                setSize (220, 120);
            }
            void resized() override
            {
                auto area = getLocalBounds().reduced (6);
                low.setBounds (area.removeFromLeft (area.getWidth() / 2));
                high.setBounds (area);
            }
            KnobControl low, high;
        };
        juce::CallOutBox::launchAsynchronously (std::make_unique<Crossovers> (processorRef.apvts),
                                                getScreenBounds().withSizeKeepingCentre (10, 10), nullptr);
    }

    void moveSelectedSlot (int direction)
    {
        const auto target = selectedSlot + direction;

        if (target < 0 || target >= IlanaSynthAudioProcessor::numFxSlots)
            return;

        // A slot is its module plus its bypass, solo and blend settings; move
        // them together so a dragged slot keeps how it was set up.
        for (const auto* suffix : { "", "_bypass", "_solo", "_mix", "_band" })
        {
            auto* current = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1) + suffix);
            auto* other = processorRef.apvts.getParameter ("fx_slot" + juce::String (target + 1) + suffix);

            if (current != nullptr && other != nullptr)
            {
                const auto currentValue = current->getValue();
                const auto otherValue = other->getValue();

                current->setValueNotifyingHost (otherValue);
                other->setValueNotifyingHost (currentValue);
            }
        }

        selectedSlot = target;
        updateVisibility();
        repaint();
    }

    void moveSelectedSlotTo (int target)
    {
        while (selectedSlot != target)
        {
            const auto direction = target > selectedSlot ? 1 : -1;
            moveSelectedSlot (direction);
        }
    }

    void saveChain()
    {
        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();

        fileChooser = std::make_unique<juce::FileChooser> ("Save FX Chain",
                                                           directory.getChildFile ("My Chain.ilanafxchain"),
                                                           "*.ilanafxchain");

        juce::Component::SafePointer<FxPage> safeThis (this);

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file != juce::File() && safeThis != nullptr)
                                          safeThis->processorRef.saveFxChainToFile (file.withFileExtension ("ilanafxchain"));
                                  });
    }

    void loadChain()
    {
        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();

        fileChooser = std::make_unique<juce::FileChooser> ("Load FX Chain", directory, "*.ilanafxchain");

        juce::Component::SafePointer<FxPage> safeThis (this);

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file.existsAsFile() && safeThis != nullptr)
                                      {
                                          safeThis->processorRef.loadFxChainFromFile (file);
                                          safeThis->updateVisibility();
                                          safeThis->repaint();
                                      }
                                  });
    }

    void chainABChanged()
    {
        chainAButton.setToggleState (processorRef.isShowingChainA(), juce::dontSendNotification);
        chainBButton.setToggleState (! processorRef.isShowingChainA(), juce::dontSendNotification);
        chainSweep = 1.0f;
        updateVisibility();
        repaint();
    }

    void updateVisibility()
    {
        std::array<bool, 64> shown {};

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = getSlotType (slot);

            if (type > 0 && type < 64)
                shown[(size_t) type] = true;
        }

        for (int type = 0; type < (int) slotGroups.size(); ++type)
            for (auto* control : slotGroups[(size_t) type])
            {
                control->setVisible (shown[(size_t) type]);
                control->setAlpha (1.0f);
            }

        updateAirwindowsKnobs (shown[30]);
        tapGrid.setVisible (shown[9]);
        gateGrid->setVisible (shown[16]);
        eqCurve.setVisible (shown[29]);
        loadIrButton.setVisible (shown[13]);

        bindBlend();
        resized();
    }

    void bindBlend()
    {
        if (boundBlendSlot != selectedSlot)
        {
            boundBlendSlot = selectedSlot;
            slotBlendAttachment.reset();

            if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1) + "_mix"))
                slotBlendAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameter, slotBlend, nullptr);
        }
    }

    // One panel per loaded slot, in chain order. A module type loaded twice
    // shares its settings, so later copies get a short note instead.
    std::vector<std::unique_ptr<juce::TextButton>> quickAddButtons;
    std::vector<std::unique_ptr<juce::Label>> quickAddLabels;

    void layoutStack()
    {
        stackPanels.clear();

        for (auto& slotSwitch : slotSwitches)
            slotSwitch->setVisible (false);
        const auto width = juce::jmax (100, stackView.getWidth() - stackView.getScrollBarThickness() - 4);
        auto y = 0;
        std::array<bool, 64> placed {};

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = getSlotType (slot);

            if (type <= 0 || type >= (int) slotGroups.size())
                continue;

            StackPanel panel;
            panel.slot = slot;
            panel.type = type;
            panel.duplicate = placed[(size_t) type];
            placed[(size_t) type] = true;

            const auto& group = slotGroups[(size_t) type];
            // (The on switch goes in the header, so it doesn't count.)
            const auto rowItems = std::count_if (group.begin(), group.end(), [type] (juce::Component* item)
            {
                if (type == 30 && ! item->isVisible())
                    return false;
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                return toggle == nullptr || ! toggle->isSwitch();
            });
            const auto rows = rowItems > 8 ? 2 : 1;
            auto height = 30 + (panel.duplicate ? 30 : rows * 112 + 8);

            if (! panel.duplicate && type == 29)
                height += 130;

            if (! panel.duplicate && type == 9)
                height += 58;

            if (! panel.duplicate && type == 16)
                height += 78;

            panel.bounds = { 0, y, width, height };
            y += height + 8;

            // Duplicates share settings but bypass on their own.
            if (panel.duplicate)
            {
                slotSwitches[(size_t) slot]->setVisible (true);
                slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
            }

            if (! panel.duplicate)
            {
                auto body = panel.bounds.reduced (10, 0);
                body.removeFromTop (30);

                // The module's on switch sits in its header (left of SLOT n),
                // as on the oscillator and body cards; the rest fill the rows.
                std::vector<juce::Component*> items;
                ToggleControl* power = nullptr;

                for (auto* item : group)
                {
                    auto* toggle = dynamic_cast<ToggleControl*> (item);

                    if (type == 30 && ! item->isVisible())
                        continue; // an Airwindows knob the algorithm doesn't use

                    if (toggle != nullptr && toggle->isSwitch() && power == nullptr)
                        power = toggle;
                    else
                        items.push_back (item);
                }

                // One on switch per module, always at the header's switch
                // place: its own on parameter, or else the slot's bypass.
                if (enableParamFor (type) != nullptr && power != nullptr)
                    power->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
                else
                {
                    if (power != nullptr)
                        items.insert (items.begin(), power);

                    slotSwitches[(size_t) slot]->setVisible (true);
                    slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
                }

                if (items.size() > 8)
                {
                    const auto half = (int) (items.size() + 1) / 2;
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (items.begin(), items.begin() + half));
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (items.begin() + half, items.end()));
                }
                else
                {
                    // A short row is centred in the card, not pinned left.
                    const auto maxWidth = juce::jmin (body.getWidth(), (int) items.size() * 120);
                    layoutRow (body.removeFromTop (112).withSizeKeepingCentre (maxWidth, 112), items);
                }

                if (type == 29)
                    eqCurve.setBounds (body.removeFromTop (126).reduced (0, 2));

                if (type == 9)
                    tapGrid.setBounds (body.removeFromTop (54).reduced (0, 2));

                if (type == 16)
                    gateGrid->setBounds (body.removeFromTop (74).reduced (0, 2));

                if (type == 13)
                    loadIrButton.setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, 0).getX() - 8 - 96, panel.bounds.getY() + 5, 96, 18);
            }

            stackPanels.push_back (panel);
        }

        updateModuleDimming();

        // Like PLAY's ADD OSCILLATOR: a quiet card after the last effect, while
        // the rack has room and the library isn't showing beside it (one
        // way in besides the slot rows, not two).
        addEffectCard = {};

        if (! stackPanels.empty() && firstEmptySlot() >= 0 && ! libraryFits())
        {
            addEffectCard = { 0, y, width, 44 };
            y += 44 + 8;
        }

        stackContent.setSize (width, juce::jmax (y, stackView.getHeight()));
        stackContent.repaint();

        // The library sits under the rack's rows in every state (three
        // across, a small heading per group) while it fits.
        size_t pick = 0;
        auto column = libraryColumn();
        const auto fits = libraryFits();

        for (size_t group = 0; group < quickAddGroups().size(); ++group)
        {
            quickAddLabels[group]->setVisible (fits);

            if (fits)
            {
                quickAddLabels[group]->setJustificationType (juce::Justification::bottomLeft);
                quickAddLabels[group]->setBounds (column.removeFromTop (libraryHeadingHeight).withTrimmedLeft (2));
            }

            const auto count = quickAddGroups()[group].count;

            for (int i = 0; i < count; ++i, ++pick)
            {
                auto& button = *quickAddButtons[pick];
                button.setVisible (fits);

                if (fits)
                {
                    if (i % libraryAcross == 0 && i > 0)
                        column.removeFromTop (libraryButtonHeight);

                    const auto cellWidth2 = column.getWidth() / libraryAcross;
                    button.setBounds (juce::Rectangle<int> (column.getX() + (i % libraryAcross) * cellWidth2, column.getY(),
                                                            cellWidth2, libraryButtonHeight).reduced (2, 2));
                }
            }

            if (fits)
                column.removeFromTop (libraryButtonHeight + 2);
        }
    }

    struct QuickAddGroup
    {
        const char* title;
        int count;
    };

    // Groups, in order, and how many of quickAddPicks() each takes.
    static const std::vector<QuickAddGroup>& quickAddGroups()
    {
        static const std::vector<QuickAddGroup> groups { { "SPACE", 7 }, { "DRIVE", 6 }, { "MOTION", 9 },
                                                         { "RHYTHM", 3 }, { "TONE & LEVEL", 6 } };
        return groups;
    }

    static const std::vector<std::pair<int, const char*>>& quickAddPicks()
    {
        static const std::vector<std::pair<int, const char*>> picks {
            { 13, "REVERB" }, { 9, "DELAY" }, { 15, "DIMENSION" }, { 11, "SMEAR" }, { 12, "FREEZE" }, { 8, "HAAS" }, { 22, "WIDENER" },
            { 2, "DRIVE" }, { 1, "AMP" }, { 3, "CRUSH" }, { 26, "OCTAVER" }, { 28, "FEEDBACK" }, { 30, "AIRWINDOWS" },
            { 7, "CHORUS" }, { 6, "PHASER" }, { 14, "FLANGER" }, { 23, "TREMOLO" }, { 24, "FREQ SHIFT" }, { 25, "RING MOD" },
            { 27, "VOWEL" }, { 5, "COMB" }, { 31, "VOCODER" },
            { 16, "TRANCE GATE" }, { 10, "STUTTER" }, { 17, "TAPE STOP" },
            { 29, "EQ" }, { 18, "TILT" }, { 4, "COMP" }, { 20, "OTT" }, { 21, "LIMITER" }, { 19, "UTILITY" }
        };
        return picks;
    }

    int firstEmptySlot() const
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) == 0)
                return slot;

        return -1;
    }

    // Where the effect library goes: under the rack's slot rows.
    juce::Rectangle<int> libraryColumn() const
    {
        auto column = juce::Rectangle<int> (14, rowsTop + numVisibleRows() * rowHeightOfSlots + 14, 300, 0);
        column.setBottom (getHeight() - 12);
        return column;
    }

    static constexpr int libraryHeadingHeight = 14, libraryButtonHeight = 22, libraryAcross = 3;

    bool libraryFits() const
    {
        auto needed = 0;

        for (const auto& group : quickAddGroups())
            needed += libraryHeadingHeight + (group.count + libraryAcross - 1) / libraryAcross * libraryButtonHeight + 2;

        return needed <= libraryColumn().getHeight();
    }

    void paintStack (juce::Graphics& g)
    {
        if (! addEffectCard.isEmpty())
        {
            const auto card = addEffectCard.toFloat().reduced (0.5f);
            g.setColour (IlanaTheme::Ui::panel.withAlpha (0.6f));
            g.fillRoundedRectangle (card, 8.0f);
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (card, 8.0f, 1.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("+  ADD EFFECT", addEffectCard, juce::Justification::centred);
        }

        if (stackPanels.empty())
        {
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            const auto middle = stackContent.getHeight() / 2 - 40;
            g.drawText ("The rack is empty", stackContent.getLocalBounds().withY (middle).withHeight (24),
                        juce::Justification::centred);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Click an effect in the library on the left to add it, or click the slot.",
                        stackContent.getLocalBounds().withY (middle + 28).withHeight (20), juce::Justification::centred);
            return;
        }

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto bounds = panel.bounds.toFloat();
            const auto selected = panel.slot == selectedSlot;
            const auto off = isModuleOff (panel.slot);

            if (selected && ! off)
                IlanaTheme::paintGlow (g, bounds, 8.0f, colour, 1.0f);

            IlanaTheme::paintCard (g, bounds, 8.0f, off ? IlanaTheme::Ui::line : colour);

            if (selected)
            {
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);
            }

            auto header = panel.bounds.withHeight (28).reduced (12, 0);
            IlanaTheme::paintCardHeader (g, header, getSlotName (panel.type).toUpperCase(),
                                         juce::String ("slot ") + juce::String (panel.slot + 1) + (off ? "  -  off" : ""),
                                         off ? IlanaTheme::Ui::text3 : colour, panel.type == 13 ? 220 : 110);

            if (panel.duplicate)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText ("Shares its settings with the first " + getSlotName (panel.type) + " above.",
                            panel.bounds.withTrimmedTop (28).reduced (16, 0).withHeight (26), juce::Justification::centredLeft);
            }
        }
    }

    void scrollToSlot (int slot)
    {
        for (const auto& panel : stackPanels)
            if (panel.slot == slot)
                stackView.setViewPosition (0, juce::jmax (0, panel.bounds.getY() - 4));
    }

    void visibilityChanged() override
    {
        if (! isShowing())
        {
            cardDragActive = false;
            dragReady = false;
            dragImage = {};
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        auto hovered = -1;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (rowBounds (slot).contains (event.getPosition()))
                hovered = slot;

        if (hovered != hoveredRow)
        {
            hoveredRow = hovered;
            repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto ticks = frameTicks();
        // The modulation effects' swinging dots move while there is sound.
        if (processorRef.getOutputPeak() > 1.0e-4f)
            meterPhase += 0.12f * ticks;
        dropFlash = IlanaAnim::decay (dropFlash, 0.85f, ticks);
        chainSweep = IlanaAnim::decay (chainSweep, 0.9f, ticks);
        paramsAppear = juce::jmin (1.0f, paramsAppear + 0.1f * ticks);
        auto hoverMoving = false;

        const auto showingA = processorRef.isShowingChainA();

        if (chainAButton.getToggleState() != showingA)
        {
            chainAButton.setToggleState (showingA, juce::dontSendNotification);
            chainBButton.setToggleState (! showingA, juce::dontSendNotification);
        }

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto target = slot == hoveredRow ? 1.0f : 0.0f;
            hoverMoving = hoverMoving || std::abs (rowHover[(size_t) slot] - target) > 0.005f;
            rowHover[(size_t) slot] = IlanaAnim::approach (rowHover[(size_t) slot], target, 0.25f, ticks);

            const auto type = getSlotType (slot);

            if (type != lastTypes[(size_t) slot])
            {
                lastTypes[(size_t) slot] = type;
                dropSlot = slot;
                dropFlash = 1.0f;
            }
        }

        juce::String signature;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            signature += juce::String (getSlotType (slot)) + ",";

        signature += juce::String (airwindowsAlgorithm()); // its knobs differ

        if (signature != lastSignature)
        {
            lastSignature = signature;

            // The selection stays on a drawn row.
            if (selectedSlot >= numVisibleRows())
            {
                selectedSlot = juce::jmax (0, numVisibleRows() - 1);
                bindBlend();
            }

            updateVisibility();
        }

        // The rows animate (meters, hover, flashes); the rest of the page is
        // still, so only the rack column repaints unless a row is dragged.
        const auto animating = hoverMoving || dropFlash > 0.01f || chainSweep > 0.01f || paramsAppear < 1.0f;

        if (cardDragActive)
            repaint();
        else if (animating || changeGate.check (processorRef.getUiEpoch()))
        {
            repaint (juce::Rectangle<int> (0, rowsTop - 10, 336, numVisibleRows() * rowHeight + 20));
            // A module switched on or off: its card and controls follow.
            if (updateModuleDimming())
                stackContent.repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoveredRow != -1)
        {
            hoveredRow = -1;
            repaint();
        }
    }

    int dragLimit = 0;
    IlanaSynthAudioProcessor& processorRef;
    TapGrid tapGrid;
    std::unique_ptr<GateGrid> gateGrid;
    std::unique_ptr<ToggleControl> softClip;
    std::unique_ptr<StripKnob> clipGain;
    juce::Rectangle<int> outputStrip;
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;

    struct StackPanel
    {
        int slot = 0, type = 0;
        bool duplicate = false;
        juce::Rectangle<int> bounds;
    };

    juce::Viewport stackView;
    FxStackContent stackContent;
    juce::Rectangle<int> addEffectCard;
    std::array<std::unique_ptr<SlotSwitch>, IlanaSynthAudioProcessor::numFxSlots> slotSwitches;

    // The module's own on parameter, where it has one.
    static const char* enableParamFor (int type)
    {
        switch (type)
        {
            case 2:  return "fx_drive_on";
            case 3:  return "fx_crush_on";
            case 5:  return "fx_comb_on";
            case 6:  return "fx_phaser_on";
            case 7:  return "fx_chorus_on";
            case 9:  return "fx_delay_on";
            case 11: return "fx_smear_on";
            case 13: return "fx_reverb_on";
            default: return nullptr;
        }
    }

    // Off: the slot is bypassed, or the module's own switch is off.
    bool isModuleOff (int slot) const
    {
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        if (processorRef.apvts.getRawParameterValue (prefix + "_bypass")->load() > 0.5f)
            return true;

        if (const auto* id = enableParamFor (getSlotType (slot)))
            if (const auto* value = processorRef.apvts.getRawParameterValue (id))
                return value->load() < 0.5f;

        return false;
    }

    // An effect that is off dims its controls (the arcs go grey).
    bool updateModuleDimming()
    {
        auto changed = false;

        for (const auto& panel : stackPanels)
        {
            if (panel.duplicate)
                continue;

            const auto alpha = isModuleOff (panel.slot) ? IlanaTheme::dimmedAlpha : 1.0f;

            for (auto* item : slotGroups[(size_t) panel.type])
            {
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                const auto isPower = toggle != nullptr && toggle->isSwitch() && item->getY() < panel.bounds.getY() + 20;

                if (! isPower && item->getAlpha() != alpha)
                {
                    item->setAlpha (alpha);
                    changed = true;
                }
            }
        }

        return changed;
    }
    std::vector<StackPanel> stackPanels;
    int selectedSlot = 0;
    static constexpr int rowHeight = 39;
    static constexpr int rowHeightOfSlots = rowHeight;
    int rowsTop = 60;
    bool dragReady = false;
    bool cardDragActive = false;
    juce::Image dragImage;
    juce::Point<int> dragPosition;
    juce::Point<int> dragGrabOffset;
    float dropFlash = 0.0f;
    int dropSlot = -1;
    int hoveredRow = -1;
    std::array<float, IlanaSynthAudioProcessor::numFxSlots> rowHover {};
    std::array<int, IlanaSynthAudioProcessor::numFxSlots> lastTypes {};
    float paramsAppear = 1.0f;
    float chainSweep = 0.0f;
    std::vector<float> correlationL, correlationR;
    float lastCorrelation = 0.0f;
    juce::String lastSignature;
    juce::TextButton prevSlotButton { "<" };
    juce::TextButton nextSlotButton { ">" };
    juce::TextButton diceButton { "DICE" };
    juce::TextButton saveChainButton { "SAVE CHAIN" };
    juce::TextButton loadChainButton { "LOAD CHAIN" };
    juce::TextButton loadIrButton { "LOAD IR" };
    // RACK A / B, named apart from the header's COMPARE A/B.
    juce::TextButton chainAButton { "RACK A" };
    juce::TextButton chainBButton { "RACK B" };
    juce::TextButton copyChainButton { "COPY TO OTHER" };
    juce::Slider slotBlend;
    std::unique_ptr<juce::SliderParameterAttachment> slotBlendAttachment;
    int boundBlendSlot = -1;
    std::unique_ptr<juce::FileChooser> fileChooser;
    float meterPhase = 0.0f;

    ComboControl ampMode;
    KnobControl ampDrive, ampBass, ampMid, ampTreble, ampLevel;
    ToggleControl driveOn;
    KnobControl driveAmount, driveMix, foldAmount;
    ToggleControl crushOn;
    KnobControl crushBits, crushDown, crushMix;
    KnobControl compThreshold, compRatio, compAttack, compRelease, compMakeup, compMix;
    ToggleControl combOn;
    KnobControl combFreq, combFeedback, combMix;
    ToggleControl phaserOn;
    KnobControl phaserRate, phaserDepth, phaserFeedback, phaserMix;
    ToggleControl chorusOn;
    KnobControl chorusRate, chorusDepth, chorusMix, haasDelay, haasMix;
    ToggleControl delayOn;
    KnobControl delayTime;
    ToggleControl delaySync;
    ComboControl delayDiv;
    KnobControl delayFeedback, delayDamping, delayMix, delayPitch, delayWow;
    ToggleControl delayPingPong;
    ToggleControl tapsOn;
    ComboControl tapsPattern;
    KnobControl tapsMix;
    ToggleControl stutterOn;
    ComboControl stutterDiv;
    KnobControl stutterMix;
    ToggleControl smearOn;
    KnobControl smearSize, smearDensity, smearMix;
    ToggleControl freezeOn;
    KnobControl freezeMix;
    ToggleControl reverbOn;
    ComboControl reverbType;
    KnobControl reverbSize, reverbDamping, reverbWidth, reverbMix;
    KnobControl flangerRate, flangerDepth, flangerFeedback, flangerMix;
    KnobControl dimRate, dimDepth, dimMix;
    ComboControl gateDiv, gatePattern;
    KnobControl gateSteps, gateSwing, gateSmooth, gateMix;
    ToggleControl tapeStopTrigger;
    KnobControl tapeStopTime, tapeStopMix;
    KnobControl tiltAmount, tiltLevel;
    KnobControl utilGain;
    ToggleControl utilMono, utilInvert;
    KnobControl ottAmount, ottMix;
    KnobControl limitCeiling, limitRelease;
    KnobControl widthAmount, widthMix;
    KnobControl tremRate, tremDepth;
    ComboControl tremShape;
    KnobControl shifterShift, shifterMix;
    KnobControl ringFreq, ringMix;
    KnobControl octaverMix;
    KnobControl vowelMorph, vowelMix;
    KnobControl delayTimeR, delayDuck;
    ToggleControl stutterReverse;
    KnobControl stutterPitch;
    KnobControl feedbackAmount, feedbackDelay, feedbackTone, feedbackMix;
    KnobControl eqLowFreq, eqLowGain, eqMidFreq, eqMidGain, eqMidQ, eqHighFreq, eqHighGain;
    EqCurve eqCurve;
    ComboControl awAlgo;
    KnobControl awP1, awP2, awP3, awP4, awP5, awMix;
    ComboControl vocSource;
    KnobControl vocBands, vocWidth, vocAttack, vocRelease, vocFormant, vocUnvoiced, vocRate, vocLevel, vocMix;

    int airwindowsAlgorithm() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_aw_algo"))
            return juce::jlimit (0, airwindows::count() - 1, (int) value->load());

        return 0;
    }

    // The chosen algorithm's knobs carry its own names; the rest hide.
    void updateAirwindowsKnobs (bool loaded)
    {
        const auto& info = airwindows::registry()[(size_t) airwindowsAlgorithm()];
        KnobControl* knobs[] { &awP1, &awP2, &awP3, &awP4, &awP5 };

        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
        {
            const auto used = k < info.numKnobs;
            if (used)
                knobs[k]->setLabelText (juce::String (info.knobs[k].name).toUpperCase());
            knobs[k]->setVisible (loaded && used);
        }
    }

    // The algorithms by family; picking one sets its knobs to the plugin's
    // own defaults.
    void showAirwindowsMenu()
    {
        juce::PopupMenu menu;
        const auto& list = airwindows::registry();
        const auto chosen = airwindowsAlgorithm();
        juce::StringArray categories;

        for (const auto& info : list)
            categories.addIfNotAlreadyThere (info.category);

        for (const auto& category : categories)
        {
            juce::PopupMenu family;
            auto holdsChosen = false;

            for (int i = 0; i < (int) list.size(); ++i)
            {
                if (category == list[(size_t) i].category)
                {
                    family.addItem (i + 1, list[(size_t) i].name, true, i == chosen);
                    holdsChosen = holdsChosen || i == chosen;
                }
            }

            menu.addSubMenu (category, family, true, nullptr, holdsChosen);
        }

        juce::Component::SafePointer<FxPage> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&awAlgo),
                            [safeThis] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->chooseAirwindows (result - 1);
                            });
    }

    void chooseAirwindows (int algorithm)
    {
        algorithm = juce::jlimit (0, airwindows::count() - 1, algorithm);
        const auto& info = airwindows::registry()[(size_t) algorithm];
        processorRef.preloadAirwindows (algorithm);

        const auto set = [this] (const juce::String& id, float value)
        {
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                parameter->endChangeGesture();
            }
        };

        for (int k = 0; k < info.numKnobs; ++k)
            set ("fx_aw_p" + juce::String (k + 1), info.knobs[k].defaultValue);

        set ("fx_aw_algo", (float) algorithm);
        updateVisibility();
        repaint();
    }
};
} // namespace
