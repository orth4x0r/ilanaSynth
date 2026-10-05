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
          haasDelay (p.apvts, "fx_haas_delay", "DELAY"),
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
          smearSize (p.apvts, "fx_smear_size", "GRAIN"),
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
          // Q1's dry-stays-dry mode (review 7, I7-4): the DX7 voices' SPACE.
          // Says what it does to MIX: the wet goes on top of the dry, and MIX
          // then reads WET (UI review 8, V8-33).
          reverbKeepDry (p.apvts, "fx_reverb_keep_dry", "WET ON TOP"),
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
          shifterShift (p.apvts, "fx_shifter_shift", "SHIFT"),
          shifterMix (p.apvts, "fx_shifter_mix", "MIX"),
          ringFreq (p.apvts, "fx_ring_freq", "FREQ"),
          ringMix (p.apvts, "fx_ring_mix", "MIX"),
          octaverMix (p.apvts, "fx_octaver_mix", "MIX"),
          vowelMorph (p.apvts, "fx_vowel_morph", "MORPH"),
          vowelMix (p.apvts, "fx_vowel_mix", "MIX"),
          delayTimeR (p.apvts, "fx_delay_time_r", "TIME RIGHT"),
          delayDuck (p.apvts, "fx_delay_duck", "DUCK"),
          stutterReverse (p.apvts, "fx_stutter_reverse", "REVERSE"),
          stutterPitch (p.apvts, "fx_stutter_pitch", "PITCH"),
          feedbackAmount (p.apvts, "fx_feedback_amount", "AMOUNT"),
          feedbackDelay (p.apvts, "fx_feedback_delay", "DELAY"),
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
                reverbOn, reverbType, reverbSize, reverbDamping, reverbWidth, reverbMix, reverbKeepDry,
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
        slotGroups.push_back ({ &reverbOn, &reverbType, &reverbSize, &reverbDamping, &reverbWidth, &reverbKeepDry, &reverbMix });
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

        // The Airwindows category modules (types 32-41): an effect picker,
        // five knobs named after the chosen effect's own, and a mix.
        for (int c = 0; c < (int) airwindows::categoryModules().size(); ++c)
        {
            const juce::String prefix = juce::String ("fx_") + airwindows::categoryModules()[(size_t) c].id;
            auto& controls = awCategories.emplace_back();
            controls.algo = std::make_unique<ComboControl> (p.apvts, prefix + "_algo", "ALGORITHM");
            for (int k = 0; k < airwindows::Module::numKnobs; ++k)
                controls.knobs[(size_t) k] = std::make_unique<KnobControl> (p.apvts, prefix + "_p" + juce::String (k + 1), juce::String (k + 1));
            controls.mix = std::make_unique<KnobControl> (p.apvts, prefix + "_mix", "MIX");
            std::vector<juce::Component*> group { controls.algo.get() };
            for (auto& knob : controls.knobs)
                group.push_back (knob.get());
            group.push_back (controls.mix.get());
            for (auto* item : group)
                addChildComponent (*item);
            slotGroups.push_back (group);
            controls.algo->setPopupOverride ([this, c] { showCategoryMenu (c); });
        }

        // The menus show Airwindows' names as words ("To Tape 6"); the saved
        // choice strings stay the plugins' own.
        const auto rename = [] (juce::ComboBox& box, const std::vector<int>& algorithms)
        {
            const auto selected = box.getSelectedId(); // (read before: it compares the shown text)
            for (int i = 0; i < (int) algorithms.size(); ++i)
                box.changeItemText (i + 1, airwindowsDisplayName (airwindows::registry()[(size_t) algorithms[(size_t) i]].name));
            box.setSelectedId (selected, juce::dontSendNotification);
        };
        std::vector<int> everyAlgorithm;
        for (int i = 0; i < airwindows::count(); ++i)
            everyAlgorithm.push_back (i);
        rename (awAlgo.getComboBox(), everyAlgorithm);
        for (size_t c = 0; c < awCategories.size(); ++c)
            rename (awCategories[c].algo->getComboBox(), airwindows::categoryModules()[c].algorithms);

        // The vocoder's "Auto" says what it does (I6-29; the saved choice stays "Auto").
        {
            auto& box = vocSource.getComboBox();
            const auto selected = box.getSelectedId();
            box.changeItemText (1, "Input, else Talk");
            box.setSelectedId (selected, juce::dontSendNotification);
        }

        // Every FX knob in its family's colour, as the library and the card
        // (I7-39); which effects have a MIX of their own (V7-7, S7-15).
        for (size_t type = 0; type < slotGroups.size(); ++type)
            for (auto* item : slotGroups[type])
                if (auto* knob = dynamic_cast<KnobControl*> (item))
                {
                    knob->setIdentityColour (fxColour ((int) type));
                    if (type < ownMix.size() && knob->getParameterId().endsWith ("_mix") && knob->getParameterId() != "fx_taps_mix")
                        ownMix[type] = true;
                }

        diceButton.setTooltip ("Randomise the FX chain: new effects (each once) in every slot of this chain. Undo brings the old chain back.");
        diceButton.onClick = [this]
        {
            processorRef.getUndoManager().beginNewTransaction ("Dice FX chain");
            processorRef.randomizeFxChain();
        };
        // Chains 1 and 2, named apart from the header's A / B compare (V7-22, S7-16).
        chainAButton.setTooltip ("Chain 1: one of two FX chains this patch stores. Click to hear and edit it; chain 2 keeps its own settings.");
        chainBButton.setTooltip ("Chain 2: one of two FX chains this patch stores. Click to hear and edit it; chain 1 keeps its own settings.");
        copyChainButton.setTooltip ("Copy this chain over the other one");
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
            // The other rack is stored outside the parameters, so undo can't
            // bring it back: ask first.
            const auto other = juce::String (processorRef.isShowingChainA() ? "2" : "1");
            juce::Component::SafePointer<FxPage> safeThis (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Copy to chain " + other,
                                                "Replace chain " + other + " with a copy of this one?",
                                                "COPY", "CANCEL", this,
                                                juce::ModalCallbackFunction::create ([safeThis] (int result)
                                                {
                                                    if (result != 1 || safeThis == nullptr)
                                                        return;
                                                    safeThis->processorRef.getUndoManager().beginNewTransaction ("Copy FX chain");
                                                    safeThis->processorRef.copyFxChainToOtherBank();
                                                    safeThis->chainABChanged();
                                                }));
        };
        // The rack's one + ADD EFFECT is the tile after the last card (S7-16).
        addEffectTile.setTooltip ("Add an effect to the next empty slot: the library, every effect grouped by what it does");
        addEffectTile.onClick = [this] { showLibrary (addEffectTile, addEffectTile.getLocalBounds()); };
        stackContent.addChildComponent (addEffectTile);
        fileButton.setTooltip ("Save this chain to a file, or load one into it");
        fileButton.onClick = [this] { showFileMenu(); };
        // The chain's own actions are quiet.
        for (auto* quiet : { &copyChainButton, &fileButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        }
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

        addAndMakeVisible (diceButton);
        addAndMakeVisible (fileButton);
        IlanaTheme::makePill (chainAButton, IlanaTheme::accent());
        IlanaTheme::makePill (chainBButton, IlanaTheme::accent());
        addAndMakeVisible (chainAButton);
        addAndMakeVisible (chainBButton);
        addAndMakeVisible (copyChainButton);
        tapGrid.setVisible (false);
        tapGrid.setName ("CUSTOM TAP GRID"); // (the UI test finds it by name)

        // The final stage after the rack.
        softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "SOFT CLIP");
        softClip->showAsSwitch();
        clipGain = std::make_unique<StripKnob> (p, "master_clip_gain", "Clip Gain");
        addAndMakeVisible (*softClip);
        addAndMakeVisible (*clipGain);

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
        stackContent.overPainter = [this] (juce::Graphics& g) { paintDragGhost (g); };
        stackContent.onDown = [this] (const juce::MouseEvent& event) { stackMouseDown (event); };
        stackContent.onDrag = [this] (const juce::MouseEvent& event) { stackMouseDrag (event); };
        stackContent.onUp = [this] (const juce::MouseEvent& event) { stackMouseUp (event); };
        stackContent.onMove = [this] (const juce::MouseEvent& event) { stackMouseMove (event); };

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            slotSwitches[(size_t) slot] = std::make_unique<SlotSwitch> (p, slot);
            stackContent.addChildComponent (*slotSwitches[(size_t) slot]);

            // Each card's header: its type menu, blend, solo and band.
            auto& header = cardHeaders[(size_t) slot];
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            header.type.onClick = [this, slot]
            {
                select (slot);
                showTypeMenu (slot, &cardHeaders[(size_t) slot].type);
            };
            stackContent.addChildComponent (header.type);

            header.solo.setClickingTogglesState (true);
            header.solo.setTooltip ("Solo: hear only this slot's effect (wet only)");
            IlanaTheme::makePill (header.solo, IlanaTheme::accent());
            if (auto* solo = p.apvts.getParameter (prefix + "_solo"))
                header.soloAttachment = std::make_unique<juce::ButtonParameterAttachment> (*solo, header.solo, nullptr);
            stackContent.addChildComponent (header.solo);

            header.band.addItemList ({ "FULL BAND", "LOW BAND", "MID BAND", "HIGH BAND", "M/S MID", "M/S SIDE" }, 1);
            header.band.setTooltip ("Band: the part of the signal this slot works on (the full signal, the LOW / MID / HIGH band, "
                                    "or the Mid or Side of the stereo image); the rest passes around it. Banded slots next to "
                                    "each other form a split group, with its crossovers above them.");
            if (auto* band = p.apvts.getParameter (prefix + "_band"))
                header.bandAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*band, header.band, nullptr);
            stackContent.addChildComponent (header.band);

            if (auto* blend = p.apvts.getParameter (prefix + "_mix"))
                header.blendAttachment = std::make_unique<juce::SliderParameterAttachment> (*blend, header.blend, nullptr);
            stackContent.addChildComponent (header.blend);
            // The slot's dry / wet as a MIX knob in the card's row, where the
            // effect has no MIX of its own: one widget, one name and one
            // place for dry / wet on every card (UI review 8, S8-6).
            header.mix = std::make_unique<KnobControl> (p.apvts, prefix + "_mix", "MIX", IlanaTheme::accent(), false);
            stackContent.addChildComponent (*header.mix);

            header.model.onSwitch = [this, slot]
            {
                const auto other = fxTwinOf (getSlotType (slot));
                if (other > 0 && slotHoldingType (other, slot) < 0)
                    processorRef.performEdit ("Change effect to " + getSlotName (other), [this, slot, other] { processorRef.assignFxSlot (slot + 1, other); });
                updateVisibility();
            };
            stackContent.addChildComponent (header.model);

            header.remove.setTooltip ("Take this duplicate out of the rack (the first card keeps the settings)");
            header.remove.onClick = [this, slot] { removeSlot (slot); };
            stackContent.addChildComponent (header.remove);

            displays[(size_t) slot] = std::make_unique<FxDisplay> (p);
            stackContent.addChildComponent (*displays[(size_t) slot]);
        }

        for (auto& strip : crossoverStrips)
        {
            strip = std::make_unique<CrossoverStrip> (p);
            stackContent.addChildComponent (*strip);
        }

        stackView.setViewedComponent (&stackContent, false);
        stackView.setScrollBarsShown (true, false);
        stackView.setScrollBarThickness (12);
        addAndMakeVisible (stackView);

        // The library fills the page while the rack is empty; + ADD EFFECT
        // opens a copy of it once there is a chain.
        library = std::make_unique<FxLibraryView> ([this] (int type) { return slotHoldingType (type); },
                                                   [this] (int type) { pickFromLibrary (type); });
        addChildComponent (*library);

        updateVisibility();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        if (! outputStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, outputStrip.toFloat(), 6.0f);
            IlanaTheme::paintCardTitle (g, outputStrip.withWidth (110).withTrimmedLeft (14), "OUTPUT", IlanaTheme::Ui::text2);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            // Read in order after its controls, like a card's subtitle.
            g.drawText ("after the rack, before the master volume",
                        outputStrip.withLeft (clipGain != nullptr ? clipGain->getRight() + 24 : outputStrip.getX()).withTrimmedRight (14),
                        juce::Justification::centredLeft);
        }

        if (library->isVisible())
        {
            auto heading = emptyHeading;
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            g.drawText ("The rack is empty", heading.removeFromTop (26), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Pick an effect to start the chain. Each one goes into the next slot; drag a card's header to reorder them.",
                        heading.removeFromTop (20), juce::Justification::centredLeft);
        }
    }

    // Cards cut by the foot or the head of a scrolling rack fade out there,
    // so they read as more beyond rather than clipped hard by OUTPUT or
    // under the CHAIN row (UI review 8, S8-14, V8-19; 9, V9-23).
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! stackView.isVisible())
            return;

        const auto background = IlanaTheme::Ui::bg;

        if (stackView.getViewPositionY() + stackView.getHeight() < stackContent.getHeight() - 2)
        {
            const auto foot = stackView.getBounds().removeFromBottom (28).toFloat();
            g.setGradientFill (juce::ColourGradient (background.withAlpha (0.0f), 0.0f, foot.getY(), background, 0.0f, foot.getBottom(), false));
            g.fillRect (foot);
        }

        if (stackView.getViewPositionY() > 2)
        {
            const auto head = stackView.getBounds().removeFromTop (14).toFloat();
            g.setGradientFill (juce::ColourGradient (background, 0.0f, head.getY(), background.withAlpha (0.0f), 0.0f, head.getBottom(), false));
            g.fillRect (head);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        // The toolbar: the two chains on the left, the dice and the
        // chain's file on the right.
        auto toolbar = area.removeFromTop (24);
        {
            auto left = toolbar;
            left.removeFromLeft (headingX - area.getX() - 2);
            chainAButton.setBounds (left.removeFromLeft (76).reduced (0, 2));
            left.removeFromLeft (4);
            chainBButton.setBounds (left.removeFromLeft (76).reduced (0, 2));
            left.removeFromLeft (8);
            copyChainButton.setBounds (left.removeFromLeft (92).reduced (0, 3));
            // The chain's dice and its file with the chain buttons, as one
            // toolbar, not across the page (UI review 8, S8-40).
            left.removeFromLeft (16);
            diceButton.setBounds (left.removeFromLeft (64).reduced (0, 3));
            left.removeFromLeft (6);
            fileButton.setBounds (left.removeFromLeft (104).reduced (0, 3));
        }

        area.removeFromTop (8);

        constexpr int outputHeight = 46, outputGap = 6;
        auto stackArea = area.withTrimmedBottom (outputHeight + outputGap);

        // An empty rack shows the library in the stack's place (one view
        // of the chain, not a list beside the cards: S5-18, S6-25).
        const auto empty = stackPanelsWouldBeEmpty();
        library->setVisible (empty);
        stackView.setVisible (! empty);

        if (empty)
        {
            auto box = stackArea.reduced (12, 0);
            emptyHeading = box.removeFromTop (60).withTrimmedTop (6);
            library->setBounds (box.withHeight (juce::jmin (box.getHeight(), FxLibraryView::preferredHeight())));
        }

        // The cards take their own height, and OUTPUT follows the last of
        // them (V7-29, S7-16: no empty band above it).
        stackView.setBounds (stackArea);
        rowExtra = 0;
        layoutStack();
        if (! empty && stackNaturalHeight < stackArea.getHeight())
        {
            // The rack's spare height goes to its rows (a taller graph, the
            // knobs centred in it), so the page doesn't end in a blank band
            // (UI review 9, V9-3).
            rowExtra = juce::jlimit (0, 260, (stackArea.getHeight() - stackNaturalHeight) / juce::jmax (1, stackRows));
            stackView.setBounds (stackArea);
            layoutStack();
            stackView.setBounds (stackArea.withHeight (juce::jmin (stackArea.getHeight(), stackNaturalHeight)));
            layoutStack();
        }

        outputStrip = empty ? area.withTop (area.getBottom() - outputHeight)
                            : juce::Rectangle<int> (area.getX(), stackView.getBottom() + outputGap, area.getWidth(), outputHeight);
        {
            auto strip = outputStrip.reduced (6, 3);
            strip.removeFromLeft (104); // the tagged OUTPUT title
            softClip->setBounds (strip.removeFromLeft (120));
            strip.removeFromLeft (10);
            clipGain->setBounds (strip.removeFromLeft (160));
        }
        repaint();
    }

private:
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

    // An effect's name, from the library's table (the card title and the
    // menus use it; the parameter's choice strings stay as saved).
    juce::String getSlotName (int type) const
    {
        const auto name = fxTypeName (type);
        return name != "-" || ! juce::isPositiveAndBelow (type, slotNames.size()) ? name : slotNames[type];
    }

    // The slot already holding this type, or -1. A module type has one set
    // of settings, so the rack takes each type once.
    int slotHoldingType (int type, int except = -1) const
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (slot != except && getSlotType (slot) == type)
                return slot;
        return -1;
    }

    bool stackPanelsWouldBeEmpty() const
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) > 0 && getSlotType (slot) < (int) slotGroups.size())
                return false;
        return true;
    }

    // The loaded slot before / after this one in the chain, or -1.
    int neighbourSlot (int slot, int direction) const
    {
        for (auto other = slot + direction; juce::isPositiveAndBelow (other, IlanaSynthAudioProcessor::numFxSlots); other += direction)
            if (getSlotType (other) != 0)
                return other;
        return -1;
    }

    void select (int slot)
    {
        if (slot != selectedSlot)
        {
            selectedSlot = slot;
            stackContent.repaint();
        }
    }

    // A library pick: a new effect goes into the first empty slot; one the
    // rack already holds is shown instead.
    void pickFromLibrary (int type)
    {
        if (const auto holder = slotHoldingType (type); holder >= 0)
        {
            select (holder);
            scrollToSlot (holder);
            dropSlot = holder;
            dropFlash = 1.0f;
            return;
        }

        const auto slot = firstEmptySlot();
        if (slot < 0)
            return;

        processorRef.performEdit ("Add " + getSlotName (type), [this, slot, type] { processorRef.assignFxSlot (slot + 1, type); });
        selectedSlot = slot;
        updateVisibility();
        scrollToSlot (slot);
        dropSlot = slot;
        dropFlash = 1.0f;
    }

    void removeSlot (int slot)
    {
        processorRef.performEdit ("Remove " + getSlotName (getSlotType (slot)), [this, slot]
        {
            if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
        });
        updateVisibility();
    }

    // The library in a call-out over the page (in the editor's window).
    void showLibrary (juce::Component& target, juce::Rectangle<int> area)
    {
        juce::Component::SafePointer<FxPage> safeThis (this);
        auto view = std::make_unique<FxLibraryView> ([safeThis] (int type) { return safeThis != nullptr ? safeThis->slotHoldingType (type) : -1; },
                                                     [safeThis] (int type)
                                                     {
                                                         if (safeThis != nullptr)
                                                             safeThis->pickFromLibrary (type);
                                                     });
        auto* raw = view.get();
        view->afterPick = [raw]
        {
            if (auto* box = raw->findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        view->setSize (FxLibraryView::columns * 136 + (FxLibraryView::columns - 1) * 10, FxLibraryView::preferredHeight());
        view->setName ("FX LIBRARY"); // (the UI test finds it by name)

        auto* parent = getTopLevelComponent();
        juce::CallOutBox::launchAsynchronously (std::move (view),
                                                parent != nullptr ? parent->getLocalArea (&target, area) : target.localAreaToGlobal (area),
                                                parent);
    }

    void showFileMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Save chain...");
        menu.addItem (2, "Load chain...");
        juce::Component::SafePointer<FxPage> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileButton),
                            [safeThis] (int result)
                            {
                                if (safeThis == nullptr)
                                    return;
                                if (result == 1)
                                    safeThis->saveChain();
                                else if (result == 2)
                                    safeThis->loadChain();
                            });
    }

    void showTypeMenu (int slot, juce::Component* target = nullptr)
    {
        juce::PopupMenu menu;
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        const auto bypassed = processorRef.apvts.getParameter (prefix + "_bypass")->getValue() > 0.5f;
        const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;

        // The library's groups and names; a type in another slot is greyed
        // here (picking it would only mirror that slot's settings).
        for (const auto& group : fxLibraryGroups())
        {
            // The all-in-one Airwindows module is kept for the patches that
            // use it; new ones pick an effect's AIRWINDOWS model instead (UI
            // review 8, I8-33).
            if (group.column < 0 && slotHoldingType (30, -1) < 0)
                continue;
            menu.addSectionHeader (group.title);
            for (const auto& entry : group.entries)
                for (const auto type : { entry.type, entry.twin })
                {
                    if (type < 0)
                        continue;
                    const auto holder = slotHoldingType (type, slot);
                    auto text = type == entry.twin ? juce::String (entry.name) + ": Airwindows model (" + entry.twinName + ")"
                                                   : juce::String (entry.name);
                    if (holder >= 0)
                        text << "  (in slot " << (holder + 1) << ")";
                    menu.addItem (type + 1, text, holder < 0, getSlotType (slot) == type);
                }
        }

        menu.addSeparator();
        menu.addItem (1001, "Bypass", true, bypassed);
        menu.addItem (1002, "Solo (wet only)", true, soloed);

        // Splitters: the slot works on one band, the rest passes around it.
        juce::PopupMenu bands;
        const juce::StringArray bandNames { "Full signal", "Low band", "Mid band", "High band", "Mid (M/S)", "Side (M/S)" };
        for (int b = 0; b < bandNames.size(); ++b)
            bands.addItem (1100 + b, bandNames[b], true, getSlotBand (slot) == b);
        menu.addSubMenu ("Band", bands);

        menu.addSeparator();
        menu.addItem (1201, "Move earlier", neighbourSlot (slot, -1) >= 0);
        menu.addItem (1202, "Move later", neighbourSlot (slot, 1) >= 0);
        menu.addItem (1003, "Remove from the rack", getSlotType (slot) != 0);

        juce::Component::SafePointer<FxPage> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target != nullptr ? target : this),
                            [safeThis, slot] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                auto& page = *safeThis;
                                const auto slotPrefix = "fx_slot" + juce::String (slot + 1);

                                if (result >= 1 && result <= 1000)
                                {
                                    page.processorRef.performEdit ("Change effect to " + page.getSlotName (result - 1),
                                                                   [&page, slot, result] { page.processorRef.assignFxSlot (slot + 1, result - 1); });
                                }
                                else if (result == 1001 || result == 1002)
                                {
                                    if (auto* parameter = page.processorRef.apvts.getParameter (slotPrefix + (result == 1001 ? "_bypass" : "_solo")))
                                        page.processorRef.performEdit (result == 1001 ? "Bypass effect" : "Solo effect", [parameter]
                                        {
                                            parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
                                        });
                                }
                                else if (result >= 1100 && result < 1110)
                                {
                                    if (auto* parameter = page.processorRef.apvts.getParameter (slotPrefix + "_band"))
                                        page.processorRef.performEdit ("Effect band", [parameter, result]
                                        {
                                            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1100)));
                                        });
                                }
                                else if (result == 1201 || result == 1202)
                                {
                                    page.moveSlot (slot, page.neighbourSlot (slot, result == 1201 ? -1 : 1));
                                }
                                else if (result == 1003)
                                {
                                    page.removeSlot (slot);
                                }

                                page.updateVisibility();
                                page.repaint();
                            });
    }

    // A slot is its module plus its bypass, solo, blend and band; they move
    // together so a moved slot keeps how it was set up.
    void swapSlots (int a, int b)
    {
        for (const auto* suffix : { "", "_bypass", "_solo", "_mix", "_band" })
        {
            auto* first = processorRef.apvts.getParameter ("fx_slot" + juce::String (a + 1) + suffix);
            auto* second = processorRef.apvts.getParameter ("fx_slot" + juce::String (b + 1) + suffix);

            if (first != nullptr && second != nullptr)
            {
                const auto firstValue = first->getValue();
                const auto secondValue = second->getValue();
                first->setValueNotifyingHost (secondValue);
                second->setValueNotifyingHost (firstValue);
            }
        }
    }

    // Moves a slot to another's place, the slots between shifting over by
    // one (as one undo step).
    void moveSlot (int from, int to)
    {
        if (from == to || ! juce::isPositiveAndBelow (from, IlanaSynthAudioProcessor::numFxSlots)
            || ! juce::isPositiveAndBelow (to, IlanaSynthAudioProcessor::numFxSlots))
            return;

        processorRef.performEdit ("Move " + getSlotName (getSlotType (from)), [this, from, to]
        {
            const auto step = to > from ? 1 : -1;
            for (auto slot = from; slot != to; slot += step)
                swapSlots (slot, slot + step);
        });

        selectedSlot = to;
        dropSlot = to;
        dropFlash = 1.0f;
        updateVisibility();
        repaint();
    }

    void saveChain()
    {
        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();

        fileChooser = std::make_unique<juce::FileChooser> ("Save FX chain",
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

        fileChooser = std::make_unique<juce::FileChooser> ("Load FX chain", directory, "*.ilanafxchain");

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
        dropSlot = -2; // every card flashes
        dropFlash = 1.0f;
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
        for (int c = 0; c < (int) awCategories.size(); ++c)
            updateCategoryKnobs (c, shown[(size_t) (airwindows::firstCategoryFxType + c)]);
        // The custom tap grid only while TAPS is on.
        tapGrid.setVisible (shown[9] && tapsEnabled());
        gateGrid->setVisible (shown[16]);
        eqCurve.setVisible (shown[29]);
        loadIrButton.setVisible (shown[13]);

        library->refresh();
        copyChainButton.setButtonText (processorRef.isShowingChainA() ? "COPY TO 2" : "COPY TO 1");
        copyChainButton.setTooltip (processorRef.isShowingChainA() ? "Copy this whole chain over chain 2" : "Copy this whole chain over chain 1");

        resized();
    }

    bool tapsEnabled() const { return processorRef.apvts.getRawParameterValue ("fx_taps_on")->load() > 0.5f; }
    bool delaySynced() const { return processorRef.apvts.getRawParameterValue ("fx_delay_sync")->load() > 0.5f; }

    // One card per loaded slot, in chain order. A module type loaded twice
    // shares its settings, so later copies say so and offer REMOVE.
    struct StackPanel
    {
        int slot = 0, type = 0;
        bool duplicate = false, half = false;
        juce::Rectangle<int> bounds;
    };

    // Banded slots next to each other, drawn as one bracketed group: a
    // LOW / MID / HIGH split with its crossovers, or a MID / SIDE one.
    struct SplitGroup
    {
        juce::Rectangle<int> bounds;
        bool frequency = true;
    };

    // Card geometry: a 30 px header (number, type menu, blend, solo, band,
    // on switch), then rows of controls sized to their knobs, with the
    // family's display on the left where the module has one. Cards whose
    // controls fit in half the width sit two to a row (S6-26).
    static constexpr int cardHeaderHeight = 30, cardRowHeight = 96, cardPadding = 6, stackTopMargin = 8, cardGap = 8;
    static constexpr int splitHeaderHeight = 38, splitInsetLeft = 18, splitInsetRight = 6;
    static constexpr int blendWidth = 104, soloWidth = 52, bandWidth = 100, knobColumn = 84;

    static int splitKind (int band) { return band >= 1 && band <= 3 ? 1 : (band >= 4 ? 2 : 0); }

    // The controls a card lays out in its rows (its on switch goes in the
    // header; an Airwindows knob the algorithm doesn't use is skipped).
    std::vector<juce::Component*> rowItems (int type, ToggleControl** power = nullptr) const
    {
        std::vector<juce::Component*> items;
        ToggleControl* found = nullptr;

        for (auto* item : slotGroups[(size_t) type])
        {
            auto* toggle = dynamic_cast<ToggleControl*> (item);

            if (isAirwindowsType (type) && ! item->isVisible())
                continue;

            if (toggle != nullptr && toggle->isSwitch() && found == nullptr)
                found = toggle;
            else
                items.push_back (item);
        }

        // One on switch per module, at the header's switch place: its own
        // on parameter, or else the slot's bypass (and the toggle joins the rows).
        if (found != nullptr && enableParamFor (type) == nullptr)
        {
            items.insert (items.begin(), found);
            found = nullptr;
        }

        if (power != nullptr)
            *power = found;
        return items;
    }

    // A card's row: its module's controls, then the slot's MIX where the
    // effect has none of its own (in the family's colour), or BLEND on an
    // old patch that set the slot's dry / wet beside the effect's MIX.
    std::vector<juce::Component*> cardItems (const StackPanel& panel, ToggleControl** power = nullptr) const
    {
        auto items = rowItems (panel.type, power);
        if (showsSlotMix (panel))
        {
            auto& knob = *cardHeaders[(size_t) panel.slot].mix;
            const auto name = hasOwnMix (panel.type) ? "BLEND" : "MIX";
            if (knob.getLabelText() != name)
                knob.setLabelText (name);
            knob.setIdentityColour (fxColour (panel.type));
            items.push_back (&knob);
        }
        return items;
    }

    static bool hasCardDisplay (int type) { return type == 29 || FxDisplay::hasDisplay (type); }

    static int displayWidthFor (int type, bool half)
    {
        if (half)
            return FxDisplay::kindFor (type) == FxDisplay::Kind::dynamics ? 160 : 130;
        return type == 29 ? 250 : (type == 9 ? 220 : (FxDisplay::kindFor (type) == FxDisplay::Kind::dynamics || type == 13 ? 200 : 190));
    }

    // The header's controls, right to left from the on switch.
    int headerControlsWidth (const StackPanel& panel) const
    {
        return 8 + 48 + (showsBand (panel.slot) ? bandWidth + 6 : 0) + soloWidth + 6 + 4
               + (panel.type == 13 && ! panel.duplicate ? 82 : 0);
    }

    int titleWidth (const StackPanel& panel) const
    {
        return 34 + IlanaTheme::cardTitleWidth (cardTitle (panel.type)) + 4
               + (hasModelSwitch (panel.type) ? FxModelSwitch::preferredWidth + 12 : (showsAirwindowsBadge (panel.type) ? 96 : 0));
    }

    // The band menu only where a slot works on a band (inside a split
    // group); the type menu sets it otherwise (S7-15).
    bool showsBand (int slot) const { return getSlotBand (slot) != 0; }

    // The slot's MIX knob only where the effect has no MIX of its own, or
    // once it is set below 100 % (V7-7, S7-15: one dry/wet per card); never
    // on a duplicate card, which does nothing of its own (V8-38).
    bool hasOwnMix (int type) const { return juce::isPositiveAndBelow (type, (int) ownMix.size()) && ownMix[(size_t) type]; }

    bool showsSlotMix (const StackPanel& panel) const
    {
        if (panel.duplicate)
            return false;
        if (! hasOwnMix (panel.type))
            return true;
        const auto* blend = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (panel.slot + 1) + "_mix");
        return blend != nullptr && blend->load() < 0.995f;
    }

    // An effect and its Airwindows model switch on the card (I7-28); the
    // card is titled by the effect ("REVERB", either model).
    static bool hasModelSwitch (int type) { return fxTwinOf (type) > 0; }
    juce::String cardTitle (int type) const
    {
        const auto base = isAirwindowsType (type) && hasModelSwitch (type) ? fxTwinOf (type) : type;
        return getSlotName (base).toUpperCase();
    }

    // The width the card needs: its header, and its display and knobs side
    // by side in one row (two rows take the full width).
    int naturalWidth (const StackPanel& panel) const
    {
        const auto header = titleWidth (panel) + headerControlsWidth (panel);
        if (panel.duplicate)
            return header;

        const auto items = cardItems (panel);
        if (items.size() > 8 || panel.type == 16)
            return std::numeric_limits<int>::max();

        return juce::jmax (header, 20 + (hasCardDisplay (panel.type) ? displayWidthFor (panel.type, true) + 12 : 0) + (int) items.size() * knobColumn);
    }

    int cardHeight (const StackPanel& panel) const
    {
        if (panel.duplicate)
            return cardHeaderHeight + 66;

        const auto rows = cardItems (panel).size() > 8 ? 2 : 1;
        auto height = cardHeaderHeight + rows * cardRowHeight + (rows == 1 ? rowExtra : 0) + cardPadding;
        if (panel.type == 9 && tapsEnabled())
            height += 56;
        if (panel.type == 16)
            height += 78;
        return height;
    }

    void layoutStack()
    {
        stackPanels.clear();
        splitGroups.clear();

        for (auto& slotSwitch : slotSwitches)
            slotSwitch->setVisible (false);
        for (auto& header : cardHeaders)
            for (juce::Component* item : { (juce::Component*) &header.type, (juce::Component*) &header.solo, (juce::Component*) &header.band,
                                           (juce::Component*) &header.blend, (juce::Component*) &header.remove, (juce::Component*) &header.model,
                                           (juce::Component*) header.mix.get() })
                item->setVisible (false);
        for (auto& display : displays)
            display->setVisible (false);
        for (auto& strip : crossoverStrips)
            strip->setVisible (false);

        const auto width = juce::jmax (100, stackView.getWidth() - stackView.getScrollBarThickness() - 4);

        // The cards in chain order, each marked half width where it fits.
        std::vector<StackPanel> cards;
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
            cards.push_back (panel);
        }

        auto y = stackTopMargin; // (room for the first card's glow)
        rowsPlaced = 0;
        size_t stripIndex = 0;
        addEffectCard = {};

        for (size_t i = 0; i < cards.size();)
        {
            // A run of cards with the same kind of band: plain, or one split group.
            const auto kind = splitKind (getSlotBand (cards[i].slot));
            auto end = i + 1;
            while (end < cards.size() && splitKind (getSlotBand (cards[end].slot)) == kind)
                ++end;

            if (kind == 0)
            {
                y = flowCards (cards, i, end, 0, width, y);
            }
            else
            {
                SplitGroup group;
                group.frequency = kind == 1;
                const auto top = y;

                if (group.frequency && stripIndex < crossoverStrips.size())
                {
                    std::array<bool, 3> used {};
                    for (auto k = i; k < end; ++k)
                        if (const auto band = getSlotBand (cards[k].slot); band >= 1 && band <= 3)
                            used[(size_t) band - 1] = true;
                    auto& strip = *crossoverStrips[stripIndex++];
                    // (Clear of the first cards' on switches, whose label space rises above them.)
                    strip.setBounds (juce::Rectangle<int> (splitInsetLeft + 150, top + 6, width - splitInsetLeft - 150 - 14, 20));
                    strip.setBandsUsed (used);
                    strip.setVisible (true);
                }

                y = flowCards (cards, i, end, splitInsetLeft, width - splitInsetLeft - splitInsetRight, top + splitHeaderHeight);
                group.bounds = { 0, top, width, y - cardGap + splitInsetRight - top };
                y = group.bounds.getBottom() + cardGap;
                splitGroups.push_back (group);
            }

            i = end;
        }

        updateModuleDimming();

        // A quiet tile after the last effect (or beside a lone half card at
        // the end) while the rack has room: the one + ADD EFFECT.
        if (addEffectCard.isEmpty() && ! stackPanels.empty() && firstEmptySlot() >= 0)
        {
            addEffectCard = { 0, y, width, DashedAddButton::standardHeight };
            y += DashedAddButton::standardHeight + cardGap;
        }
        addEffectTile.setBounds (addEffectCard);
        addEffectTile.setVisible (! addEffectCard.isEmpty());

        stackNaturalHeight = y;
        stackRows = rowsPlaced;
        stackContent.setSize (width, juce::jmax (y, stackView.getHeight()));
        stackContent.repaint();
    }

    // Cards from..to in rows: two half cards side by side (the same
    // height), anything else across the whole width. One rule for widths
    // (UI review 8, S8-14, V8-19): cards pair up two to a row only at the
    // top level and only in pairs; a half card with no partner takes the
    // whole width, except the chain's last with + ADD EFFECT beside it;
    // inside a split group every card is full width.
    int flowCards (std::vector<StackPanel>& cards, size_t from, size_t to, int x, int width, int y, bool addTileAtEnd = false)
    {
        const auto halfWidth = (width - cardGap) / 2;
        const auto inGroup = x > 0;

        for (auto k = from; k < to; ++k)
            cards[k].half = ! inGroup && naturalWidth (cards[k]) <= halfWidth;

        // A lone half card (no half neighbour to pair with) goes full width.
        for (auto k = from; k < to;)
        {
            if (cards[k].half && k + 1 < to && cards[k + 1].half)
            {
                k += 2;
                continue;
            }
            if (cards[k].half && ! (k + 1 == to && addTileAtEnd))
                cards[k].half = false;
            ++k;
        }

        for (auto k = from; k < to;)
        {
            if (cards[k].half && k + 1 < to && cards[k + 1].half)
            {
                const auto height = juce::jmax (cardHeight (cards[k]), cardHeight (cards[k + 1]));
                placeCard (cards[k], { x, y, halfWidth, height });
                placeCard (cards[k + 1], { x + halfWidth + cardGap, y, width - halfWidth - cardGap, height });
                ++rowsPlaced;
                k += 2;
                y += height + cardGap;
            }
            else if (cards[k].half)
            {
                const auto height = cardHeight (cards[k]);
                placeCard (cards[k], { x, y, halfWidth, height });
                if (k + 1 == to && addTileAtEnd)
                    addEffectCard = { x + halfWidth + cardGap, y, width - halfWidth - cardGap, height };
                ++rowsPlaced;
                ++k;
                y += height + cardGap;
            }
            else
            {
                cards[k].half = false;
                const auto height = cardHeight (cards[k]);
                placeCard (cards[k], { x, y, width, height });
                ++rowsPlaced;
                ++k;
                y += height + cardGap;
            }
        }

        return y;
    }

    void placeCard (StackPanel panel, juce::Rectangle<int> bounds)
    {
        panel.bounds = bounds;
        layoutCardHeader (panel);
        const auto slot = panel.slot;
        const auto type = panel.type;

        if (panel.duplicate)
        {
            // Duplicates share settings but bypass on their own.
            slotSwitches[(size_t) slot]->setVisible (true);
            slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
            auto& remove = cardHeaders[(size_t) slot].remove;
            remove.setBounds (panel.bounds.getRight() - 12 - 84, panel.bounds.getBottom() - 12 - 24, 84, 24);
            remove.setVisible (true);
            stackPanels.push_back (panel);
            return;
        }

        auto body = panel.bounds.reduced (10, 0);
        body.removeFromTop (cardHeaderHeight);

        ToggleControl* power = nullptr;
        const auto items = cardItems (panel, &power);
        if (showsSlotMix (panel))
            cardHeaders[(size_t) slot].mix->setVisible (true);

        if (power != nullptr)
            power->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
        else
        {
            slotSwitches[(size_t) slot]->setVisible (true);
            slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
        }

        const auto rows = items.size() > 8 ? 2 : 1;
        const auto extra = rows == 1 ? rowExtra : 0;
        auto rowsArea = body.removeFromTop (rows * cardRowHeight + extra);

        // The family's picture, left of the knobs.
        if (hasCardDisplay (type))
        {
            const auto displayWidth = juce::jmin (rowsArea.getWidth() / 3, displayWidthFor (type, panel.half));
            const auto displayArea = rowsArea.removeFromLeft (displayWidth).reduced (0, 6);
            rowsArea.removeFromLeft (12);

            if (type == 29)
                eqCurve.setBounds (displayArea);
            else
            {
                auto& display = *displays[(size_t) slot];
                display.setType (type, fxColour (type));
                display.setBounds (displayArea);
                display.setVisible (true);
            }
        }

        if (items.size() > 8)
        {
            const auto half = (int) (items.size() + 1) / 2;
            layoutRow (rowsArea.removeFromTop (cardRowHeight), std::vector<juce::Component*> (items.begin(), items.begin() + half));
            layoutRow (rowsArea.removeFromTop (cardRowHeight), std::vector<juce::Component*> (items.begin() + half, items.end()));
        }
        else
        {
            // A short row is centred in its space, not pinned left.
            // Menus (an Airwindows algorithm's long name) take a wider column.
            auto menus = 0;
            for (auto* item : items)
                menus += dynamic_cast<ComboControl*> (item) != nullptr ? 1 : 0;
            const auto maxWidth = juce::jmin (rowsArea.getWidth(), (int) items.size() * (panel.half ? 110 : 150) + menus * 40);
            // A row led by a menu (an ALGORITHM) starts at the row's left, so
            // the menu stays put when the card switches model (UI review 8,
            // I8-33); a row of knobs is centred.
            const auto ledByMenu = ! items.empty() && dynamic_cast<ComboControl*> (items.front()) != nullptr;
            auto row = rowsArea.removeFromTop (cardRowHeight + extra);
            layoutRow (ledByMenu ? row.withWidth (maxWidth) : row.withSizeKeepingCentre (maxWidth, row.getHeight()), items, false, 1.4f);
        }

        body.removeFromTop (cardPadding);

        if (type == 9 && tapsEnabled())
            tapGrid.setBounds (body.removeFromTop (56).reduced (0, 2).withTrimmedBottom (4));

        if (type == 16)
            gateGrid->setBounds (body.removeFromTop (78).reduced (0, 2).withTrimmedBottom (4));

        stackPanels.push_back (panel);
    }

    // A card's header, right to left from the on switch: the band, SOLO,
    // the slot's BLEND and, on the reverb, LOAD IR; on the left the slot's
    // number, then the title as the type menu.
    void layoutCardHeader (const StackPanel& panel)
    {
        auto& header = cardHeaders[(size_t) panel.slot];
        const auto top = panel.bounds.getY();
        const auto lineY = top + 5;

        header.type.setTitle (cardTitle (panel.type), isModuleOff (panel.slot) ? IlanaTheme::Ui::text3 : fxColour (panel.type));
        header.type.setBounds (panel.bounds.getX() + 34, top + 4, header.type.preferredWidth(), 22);
        header.type.setVisible (true);
        header.type.setTooltip ("Slot " + juce::String (panel.slot + 1) + ": " + getSlotName (panel.type)
                                + ". Click to change the effect, move or remove it; drag the header to reorder."
                                + (isAirwindowsType (panel.type) ? "\n" + airwindowsBadgeTip (panel.type) : juce::String()));

        auto right = IlanaTheme::cardSwitchBounds (panel.bounds, top + 14).getX() - 8;
        if (showsBand (panel.slot))
        {
            header.band.setBounds (right - bandWidth, lineY, bandWidth, 20);
            header.band.setVisible (true);
            right -= bandWidth + 6;
        }
        header.solo.setBounds (right - soloWidth, lineY, soloWidth, 20);
        header.solo.setVisible (true);
        right -= soloWidth + 6;
        // (The slot's dry / wet is a MIX knob in the row now, S8-6.)
        header.blend.setBounds (right, lineY, 0, 20); // (the subtitle stops here)
        right -= 4;

        if (panel.type == 13 && ! panel.duplicate)
        {
            loadIrButton.setBounds (right - 76, lineY + 1, 76, 18);
            right -= 82;
        }

        // The model switch after the title, where it fits.
        if (hasModelSwitch (panel.type) && ! panel.duplicate)
        {
            const auto switchX = header.type.getRight() + 6;
            if (switchX + FxModelSwitch::preferredWidth <= right - 4)
            {
                const auto other = fxTwinOf (panel.type);
                const auto holder = slotHoldingType (other, panel.slot);
                header.model.setBounds (switchX, top + 7, FxModelSwitch::preferredWidth, 16);
                header.model.setState (isAirwindowsType (panel.type), holder < 0, fxColour (panel.type));
                header.model.setTooltip ("Model: " + getSlotName (isAirwindowsType (panel.type) ? other : panel.type) + " built in, or its Airwindows model, "
                                         + getSlotName (isAirwindowsType (panel.type) ? panel.type : other) + ". Click the other to swap this slot's effect "
                                         "(each keeps its own settings)."
                                         + (holder >= 0 ? "\nThe other model is in slot " + juce::String (holder + 1) + " already." : juce::String())
                                         + "\n" + airwindowsBadgeTip (isAirwindowsType (panel.type) ? panel.type : other));
                header.model.setVisible (true);
            }
        }
    }

    int firstEmptySlot() const
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) == 0)
                return slot;

        return -1;
    }

    void paintSplitGroup (juce::Graphics& g, const SplitGroup& group)
    {
        const auto bounds = group.bounds.toFloat();
        g.setColour (IlanaTheme::Ui::panel.withAlpha (0.45f));
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (IlanaTheme::Ui::line.brighter (0.15f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        // The bracket down the left edge, around the group's cards.
        const auto x = bounds.getX() + 7.0f;
        const auto top = bounds.getY() + (float) splitHeaderHeight;
        const auto bottom = bounds.getBottom() - 10.0f;
        juce::Path bracket;
        bracket.startNewSubPath (x + 6.0f, top);
        bracket.lineTo (x, top);
        bracket.lineTo (x, bottom);
        bracket.lineTo (x + 6.0f, bottom);
        g.setColour (IlanaTheme::accent().withAlpha (0.75f));
        g.strokePath (bracket, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

        const auto heading = group.bounds.withHeight (splitHeaderHeight).withTrimmedLeft (splitInsetLeft - 4);
        IlanaTheme::paintCardTitle (g, heading.withWidth (150), group.frequency ? "SPLIT BANDS" : "MID / SIDE", IlanaTheme::accent());

        if (! group.frequency)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("these slots work on the middle or the sides of the stereo image; the rest passes around them",
                        heading.withTrimmedLeft (150).withTrimmedRight (14), juce::Justification::centredLeft, true);
        }
    }

    void paintStack (juce::Graphics& g)
    {
        for (const auto& group : splitGroups)
            paintSplitGroup (g, group);

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto bounds = panel.bounds.toFloat();
            const auto selected = panel.slot == selectedSlot;
            const auto off = isModuleOff (panel.slot);
            const auto& header = cardHeaders[(size_t) panel.slot];

            if (panel.slot == dragSlot && dragActive)
            {
                // Where the dragged card came from: an empty outline.
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
                continue;
            }

            if (selected && ! off)
                IlanaTheme::paintGlow (g, bounds, 8.0f, colour, 1.0f);

            IlanaTheme::paintCard (g, bounds, 8.0f, off ? IlanaTheme::Ui::line : colour);

            if (selected)
            {
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);
            }

            // The slot's number, a small badge before the title.
            const auto badge = juce::Rectangle<float> (18.0f, 16.0f).withPosition (bounds.getX() + 11.0f, bounds.getY() + 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (badge, 4.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true, true));
            g.drawText (juce::String (panel.slot + 1), badge, juce::Justification::centred);

            // After the title: the Airwindows badge, "off", the slot's CPU,
            // as far as the header's controls leave room.
            auto subtitle = juce::Rectangle<int> (header.type.getRight() + 6, panel.bounds.getY(), 0, cardHeaderHeight);
            subtitle.setRight ((panel.type == 13 && ! panel.duplicate ? loadIrButton.getX() : header.blend.getX()) - 8);
            if (header.model.isVisible())
                subtitle.setLeft (header.model.getRight() + 8);

            if (showsAirwindowsBadge (panel.type) && ! hasModelSwitch (panel.type) && subtitle.getWidth() >= 88)
            {
                const auto tag = subtitle.removeFromLeft (84).toFloat().withSizeKeepingCentre (84.0f, 16.0f);
                g.setColour (colour.withAlpha (0.14f));
                g.fillRoundedRectangle (tag, 8.0f);
                g.setColour (colour.withAlpha (0.55f));
                g.drawRoundedRectangle (tag.reduced (0.5f), 8.0f, 1.0f);
                g.setColour (colour.interpolatedWith (juce::Colours::white, 0.4f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("AIRWINDOWS", tag, juce::Justification::centred);
                subtitle.removeFromLeft (8);
            }

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            if (off)
            {
                g.drawText ("off", subtitle, juce::Justification::centredLeft, true);
            }
            // The slot's CPU only while the card is under the mouse: beside
            // every name it was noise, and a number that came and went on
            // some cards read as an error (UI review 8, V8-34; 9, V9-21).
            else if (const auto cpu = processorRef.getFxSlotCpu (panel.slot);
                     cpu > 0.0005f && stackContent.isMouseOver (true) && panel.bounds.contains (stackContent.getMouseXYRelative())
                     && subtitle.getWidth() >= 70)
            {
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true));
                g.drawText ("CPU " + juce::String (cpu * 100.0f, 1) + "%", subtitle, juce::Justification::centredLeft, true);
            }

            if (panel.duplicate)
            {
                auto text = panel.bounds.withTrimmedTop (cardHeaderHeight).reduced (16, 6);
                const auto first = slotHoldingType (panel.type, panel.slot);
                g.setColour (IlanaTheme::Ui::text);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
                g.drawText ("Duplicate " + getSlotName (panel.type) + " (not supported): it shares slot " + juce::String (first + 1) + "'s settings",
                            text.removeFromTop (20), juce::Justification::centredLeft, true);
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                IlanaTheme::drawFitted (g, "The rack keeps one set of settings per effect, so this slot runs slot " + juce::String (first + 1)
                                      + "'s " + getSlotName (panel.type) + " again (older patches can do this). "
                                      + "Remove it, or keep it for the same sound.",
                                  text.withTrimmedRight (100), juce::Justification::topLeft, 2);
            }

            // A card that just arrived or moved flashes once.
            if ((panel.slot == dropSlot || dropSlot == -2) && dropFlash > 0.01f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (dropFlash * 0.5f));
                g.drawRoundedRectangle (bounds.expanded (dropFlash * 3.0f), 9.0f, 2.0f);
            }
        }
    }

    // While a card is dragged by its header: where it will land, and its
    // ghost under the pointer.
    void paintDragGhost (juce::Graphics& g)
    {
        if (! dragActive)
            return;

        for (const auto& panel : stackPanels)
            if (panel.slot == dropTarget && dropTarget != dragSlot)
            {
                g.setColour (IlanaTheme::accent().withAlpha (0.9f));
                g.drawRoundedRectangle (panel.bounds.toFloat().reduced (1.0f), 8.0f, 2.0f);
            }

        if (dragImage.isValid())
        {
            const auto ghost = juce::Rectangle<float> ((float) dragImage.getWidth(), (float) dragImage.getHeight())
                                   .withPosition ((float) (dragPosition.x - dragGrabOffset.x), (float) (dragPosition.y - dragGrabOffset.y));
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (ghost.translated (4.0f, 4.0f), 8.0f);
            g.setOpacity (0.85f);
            g.drawImageAt (dragImage, (int) ghost.getX(), (int) ghost.getY());
            g.setOpacity (1.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.8f));
            g.drawRoundedRectangle (ghost, 8.0f, 1.5f);
        }
    }

    // The card under a point, else the nearest one.
    int slotAt (juce::Point<int> position) const
    {
        auto best = -1;
        auto bestDistance = std::numeric_limits<int>::max();

        for (const auto& panel : stackPanels)
        {
            const juce::Point<int> nearest (juce::jlimit (panel.bounds.getX(), panel.bounds.getRight(), position.x),
                                            juce::jlimit (panel.bounds.getY(), panel.bounds.getBottom(), position.y));
            const auto distance = nearest.getDistanceSquaredFrom (position);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = panel.slot;
            }
        }

        return best;
    }

    void stackMouseDown (const juce::MouseEvent& event)
    {
        const auto position = event.getPosition();

        for (const auto& panel : stackPanels)
        {
            if (! panel.bounds.contains (position))
                continue;

            select (panel.slot);

            if (event.mods.isPopupMenu())
            {
                showTypeMenu (panel.slot);
                return;
            }

            // The header is the card's handle: a drag reorders the chain.
            if (position.y < panel.bounds.getY() + cardHeaderHeight)
            {
                dragSlot = panel.slot;
                dragImage = stackContent.createComponentSnapshot (panel.bounds);
                dragGrabOffset = position - panel.bounds.getPosition();
                dragPosition = position;
            }

            return;
        }
    }

    void stackMouseDrag (const juce::MouseEvent& event)
    {
        if (dragSlot < 0)
            return;

        dragPosition = event.getPosition();

        if (! dragActive && event.getDistanceFromDragStart() > 4)
            dragActive = true;

        if (! dragActive)
            return;

        dropTarget = slotAt (dragPosition);
        const auto inView = event.getEventRelativeTo (&stackView);
        stackView.autoScroll (inView.x, inView.y, 24, 10);
        stackContent.repaint();
    }

    void stackMouseUp (const juce::MouseEvent&)
    {
        const auto from = dragSlot, to = dropTarget;
        const auto moved = dragActive && from >= 0 && to >= 0 && to != from;
        dragSlot = dropTarget = -1;
        dragActive = false;
        dragImage = {};

        if (moved)
            moveSlot (from, to);

        stackContent.repaint();
    }

    void stackMouseMove (const juce::MouseEvent& event)
    {
        auto overHeader = false;
        for (const auto& panel : stackPanels)
            overHeader = overHeader || panel.bounds.withHeight (cardHeaderHeight).contains (event.getPosition());

        stackContent.setMouseCursor (overHeader ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void scrollToSlot (int slot)
    {
        for (const auto& panel : stackPanels)
            if (panel.slot == slot)
            {
                const auto view = stackView.getViewArea();
                if (! view.contains (panel.bounds))
                    stackView.setViewPosition (0, juce::jmax (0, panel.bounds.getY() - 4));
            }
    }

    void visibilityChanged() override
    {
        if (! isShowing())
        {
            dragSlot = dropTarget = -1;
            dragActive = false;
            dragImage = {};
        }
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto ticks = frameTicks();
        const auto flashing = dropFlash > 0.01f;
        dropFlash = IlanaAnim::decay (dropFlash, 0.85f, ticks);

        const auto showingA = processorRef.isShowingChainA();

        if (chainAButton.getToggleState() != showingA)
        {
            chainAButton.setToggleState (showingA, juce::dontSendNotification);
            chainBButton.setToggleState (! showingA, juce::dontSendNotification);
        }

        juce::String signature;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = getSlotType (slot);

            if (type != lastTypes[(size_t) slot])
            {
                lastTypes[(size_t) slot] = type;
                dropSlot = slot;
                dropFlash = 1.0f;
            }

            // (BLEND shows once it is below 100 % on a card with its own MIX.)
            const auto* blend = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_mix");
            signature << type << ":" << getSlotBand (slot) << (blend != nullptr && blend->load() < 0.995f ? "b" : "") << ",";
        }

        signature += juce::String (airwindowsAlgorithm()); // its knobs differ
        for (int c = 0; c < (int) awCategories.size(); ++c)
            signature += "," + juce::String (categoryChoice (c));
        signature += tapsEnabled() ? ",taps" : ",-"; // the tap grid comes and goes
        signature += showingA ? ",A" : ",B";

        if (signature != lastSignature)
        {
            lastSignature = signature;
            if (getSlotType (selectedSlot) == 0)
                for (const auto& panel : stackPanels)
                    if (getSlotType (panel.slot) != 0)
                    {
                        selectedSlot = panel.slot;
                        break;
                    }
            updateVisibility();
        }

        // Cards repaint for a flash, a module switched on or off, and now
        // and then for their CPU readouts.
        auto dirty = flashing;
        if (changeGate.check (processorRef.getUiEpoch()))
        {
            dirty = updateModuleDimming() || dirty;
            if (const auto mixName = reverbKeepDry.getButton().getToggleState() ? "WET" : "MIX"; reverbMix.getLabelText() != mixName)
                reverbMix.setLabelText (mixName);
        }
        cpuTicks += ticks;
        if (cpuTicks > 30.0f)
        {
            cpuTicks = 0.0f;
            dirty = true;
        }

        if (dirty)
            stackContent.repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    TapGrid tapGrid;
    std::unique_ptr<GateGrid> gateGrid;
    std::unique_ptr<ToggleControl> softClip;
    std::unique_ptr<StripKnob> clipGain;
    juce::Rectangle<int> outputStrip, emptyHeading;
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;

    juce::Viewport stackView;
    FxStackContent stackContent;
    juce::Rectangle<int> addEffectCard;
    std::array<std::unique_ptr<SlotSwitch>, IlanaSynthAudioProcessor::numFxSlots> slotSwitches;
    std::unique_ptr<FxLibraryView> library;
    // (At most five split groups: each needs a plain card or the end after it.)
    std::array<std::unique_ptr<CrossoverStrip>, 5> crossoverStrips;

    // Per slot: the card header's type menu, blend, solo, band and (on a
    // duplicate) REMOVE, and the card's display.
    struct CardHeader
    {
        FxTypeButton type;
        juce::TextButton solo { "SOLO" };
        juce::ComboBox band;
        BlendSlider blend;
        std::unique_ptr<KnobControl> mix;
        juce::TextButton remove { "REMOVE" };
        FxModelSwitch model;
        std::unique_ptr<juce::ButtonParameterAttachment> soloAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> bandAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> blendAttachment;
    };
    std::array<CardHeader, IlanaSynthAudioProcessor::numFxSlots> cardHeaders;
    std::array<std::unique_ptr<FxDisplay>, IlanaSynthAudioProcessor::numFxSlots> displays;

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

            // (A switched-off card at the off alpha, as WEST and BODY: V7-34.)
            const auto off = isModuleOff (panel.slot);
            const auto alpha = off ? FilterColours::offAlpha : 1.0f;
            cardHeaders[(size_t) panel.slot].type.setTitle (cardTitle (panel.type), off ? IlanaTheme::Ui::text3 : fxColour (panel.type));

            for (auto* item : slotGroups[(size_t) panel.type])
            {
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                const auto isPower = toggle != nullptr && toggle->isSwitch() && item->getY() < panel.bounds.getY() + 20;
                auto itemAlpha = alpha;

                // The delay's times do nothing while SYNC is on, its DIV
                // nothing while it is off.
                if (panel.type == 9 && ! off)
                {
                    const auto synced = delaySynced();
                    if (((item == &delayTime || item == &delayTimeR) && synced) || (item == &delayDiv && ! synced))
                        itemAlpha = IlanaTheme::dimmedAlpha;
                }

                if (! isPower && item->getAlpha() != itemAlpha)
                {
                    item->setAlpha (itemAlpha);
                    changed = true;
                }
            }

            if (auto& mix = *cardHeaders[(size_t) panel.slot].mix; mix.getAlpha() != alpha)
            {
                mix.setAlpha (alpha);
                changed = true;
            }
        }

        return changed;
    }

    std::vector<StackPanel> stackPanels;
    std::vector<SplitGroup> splitGroups;
    int selectedSlot = 0;
    int dragSlot = -1, dropTarget = -1;
    bool dragActive = false;
    juce::Image dragImage;
    juce::Point<int> dragPosition;
    juce::Point<int> dragGrabOffset;
    float dropFlash = 0.0f;
    int dropSlot = -1;
    float cpuTicks = 0.0f;
    std::array<int, IlanaSynthAudioProcessor::numFxSlots> lastTypes {};
    juce::String lastSignature;
    // The toolbar: CHAIN 1 / 2, named apart from the header's A / B
    // compare; the dice as the header's, for the chain (V7-43).
    DiceFxButton diceButton;
    juce::TextButton fileButton { "SAVE / LOAD" };
    juce::TextButton loadIrButton { "LOAD IR" };
    juce::TextButton chainAButton { "CHAIN 1" };
    juce::TextButton chainBButton { "CHAIN 2" };
    juce::TextButton copyChainButton { "COPY TO 2" };
    DashedAddButton addEffectTile { "+  ADD EFFECT", "+  ADD EFFECT" };
    std::array<bool, 64> ownMix {};
    int stackNaturalHeight = 0, stackRows = 0, rowsPlaced = 0, rowExtra = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

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
    ToggleControl reverbKeepDry;
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

    // Airwindows' knob names are cut to fit its 8-character plugin labels
    // ("Pressre", "Feedbk"); the card has room for the whole word.
    static juce::String airwindowsKnobLabel (const char* name)
    {
        static const std::map<juce::String, juce::String> full {
            { "Compres", "Compress" }, { "Feedbk", "Feedback" }, { "Highpas", "Highpass" }, { "Pressre", "Pressure" },
            { "Mewines", "Mewiness" }, { "filters Q", "Filter Q" }, { "HeadBmp", "Head Bump" }, { "Head B", "Head Bump" },
            { "Gv Wear", "Groove Wear" }, { "RmSize", "Room Size" }, { "MakeupGn", "Makeup Gain" }, { "Mid HiP", "Mid Highpass" },
            { "SideHiP", "Side Highpass" }, { "MonoBs", "Mono Bass" }, { "NonLin", "Nonlinear" }, { "FMDepth", "FM Depth" },
            { "FMSpeed", "FM Speed" }, { "BitShift", "Bit Shift" }, { "Gnd", "Ground" }, { "11K tap", "11K Tap" },
            { "15K tap", "15K Tap" }, { "22K tap", "22K Tap" } };
        const juce::String text (name);
        const auto found = full.find (text);
        return (found != full.end() ? found->second : text).toUpperCase();
    }

    // Airwindows' plugin names as words: "ToTape6" reads "To Tape 6",
    // "Console7Channel" "Console 7 Channel". Display only: the saved choice
    // strings stay the plugins' own (and show in the tooltip).
    static juce::String airwindowsDisplayName (const char* name)
    {
        static const std::map<juce::String, juce::String> special { { "kCathedral", "Cathedral" }, { "DeRez2", "DeRez 2" },
                                                                    { "StereoFX", "Stereo FX" } };
        const juce::String raw (name);
        if (const auto found = special.find (raw); found != special.end())
            return found->second;

        juce::String out;
        for (int i = 0; i < raw.length(); ++i)
        {
            const auto c = raw[i];
            if (i > 0)
            {
                const auto previous = raw[i - 1];
                const auto upperAfterLower = juce::CharacterFunctions::isUpperCase (c) && juce::CharacterFunctions::isLowerCase (previous);
                const auto digitAfterLetter = juce::CharacterFunctions::isDigit (c) && juce::CharacterFunctions::isLetter (previous);
                const auto letterAfterDigit = juce::CharacterFunctions::isLetter (c) && juce::CharacterFunctions::isDigit (previous);
                if (upperAfterLower || digitAfterLetter || letterAfterDigit)
                    out << ' ';
            }
            out << juce::String::charToString (c);
        }
        return out;
    }

    // The algorithm's own Dry/Wet stays where it is (as chosen, usually
    // 100 %): the module's MIX is the one wet/dry control on the card.
    static bool isAirwindowsDryWet (const char* name) { return juce::String (name) == "Dry/Wet"; }

    static juce::String airwindowsTooltip (const airwindows::Info& info)
    {
        return "Airwindows " + juce::String (info.name) + " (" + info.category + ")\nClick to choose another algorithm.";
    }

    // An Airwindows knob's tooltip: the plugin's name for it, and why it
    // reads in % (the plugins give their controls no unit: UI review 8,
    // I8-34).
    static juce::String airwindowsKnobTip (const airwindows::Info& info, int k)
    {
        return airwindowsKnobLabel (info.knobs[k].name) + " (Airwindows " + juce::String (info.name) + ")\n"
               "The plugin's own control, shown as 0 to 100 % of its range: Airwindows gives it no unit.";
    }

    static void setKnobTip (KnobControl& knob, const juce::String& tip)
    {
        knob.setTooltip (tip);
        knob.getSlider().setTooltip (tip);
    }

    // The chosen algorithm's knobs carry its own names; the rest hide.
    void updateAirwindowsKnobs (bool loaded)
    {
        const auto& info = airwindows::registry()[(size_t) airwindowsAlgorithm()];
        KnobControl* knobs[] { &awP1, &awP2, &awP3, &awP4, &awP5 };

        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
        {
            const auto used = k < info.numKnobs && ! isAirwindowsDryWet (info.knobs[k].name);
            if (used)
            {
                knobs[k]->setLabelText (airwindowsKnobLabel (info.knobs[k].name));
                setKnobTip (*knobs[k], airwindowsKnobTip (info, k));
            }
            knobs[k]->setVisible (loaded && used);
        }

        awAlgo.getComboBox().setTooltip (airwindowsTooltip (info));
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
                    family.addItem (i + 1, airwindowsDisplayName (list[(size_t) i].name), true, i == chosen);
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

    // ---- The Airwindows category modules (types 32-41) ----
    struct AwCategoryControls
    {
        std::unique_ptr<ComboControl> algo;
        std::array<std::unique_ptr<KnobControl>, airwindows::Module::numKnobs> knobs;
        std::unique_ptr<KnobControl> mix;
    };
    std::vector<AwCategoryControls> awCategories;

    static bool isAirwindowsType (int type) { return type == 30 || airwindows::categoryForFxType (type) >= 0; }
    // The badge after the title; "Airwindows (all)" already says it.
    static bool showsAirwindowsBadge (int type) { return isAirwindowsType (type) && type != 30; }

    int categoryChoice (int c) const
    {
        const auto& info = airwindows::categoryModules()[(size_t) c];
        if (const auto* value = processorRef.apvts.getRawParameterValue (juce::String ("fx_") + info.id + "_algo"))
            return juce::jlimit (0, (int) info.algorithms.size() - 1, (int) value->load());
        return 0;
    }

    void updateCategoryKnobs (int c, bool loaded)
    {
        const auto& category = airwindows::categoryModules()[(size_t) c];
        const auto& info = airwindows::registry()[(size_t) category.algorithms[(size_t) categoryChoice (c)]];
        auto& controls = awCategories[(size_t) c];
        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
        {
            const auto used = k < info.numKnobs && ! isAirwindowsDryWet (info.knobs[k].name);
            if (used)
            {
                controls.knobs[(size_t) k]->setLabelText (airwindowsKnobLabel (info.knobs[k].name));
                setKnobTip (*controls.knobs[(size_t) k], airwindowsKnobTip (info, k));
            }
            controls.knobs[(size_t) k]->setVisible (loaded && used);
        }

        controls.algo->getComboBox().setTooltip (airwindowsTooltip (info));
    }

    // The category's effects; picking one sets its knobs to the plugin's
    // own defaults.
    void showCategoryMenu (int c)
    {
        const auto& category = airwindows::categoryModules()[(size_t) c];
        const auto chosen = categoryChoice (c);
        juce::PopupMenu menu;
        menu.addSectionHeader (juce::String (category.label).toUpperCase());
        for (int i = 0; i < (int) category.algorithms.size(); ++i)
            menu.addItem (i + 1, airwindowsDisplayName (airwindows::registry()[(size_t) category.algorithms[(size_t) i]].name), true, i == chosen);

        juce::Component::SafePointer<FxPage> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (awCategories[(size_t) c].algo.get()),
                            [safeThis, c] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->chooseCategoryEffect (c, result - 1);
                            });
    }

    void chooseCategoryEffect (int c, int choice)
    {
        const auto& category = airwindows::categoryModules()[(size_t) c];
        choice = juce::jlimit (0, (int) category.algorithms.size() - 1, choice);
        const auto& info = airwindows::registry()[(size_t) category.algorithms[(size_t) choice]];
        processorRef.preloadAirwindowsCategory (c, choice);
        const juce::String prefix = juce::String ("fx_") + category.id;

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
            set (prefix + "_p" + juce::String (k + 1), info.knobs[k].defaultValue);
        set (prefix + "_algo", (float) choice);
        updateVisibility();
        repaint();
    }
};
} // namespace
