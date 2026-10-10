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
          driveType (p.apvts, "fx_drive_type", "CLIP"),
          foldType (p.apvts, "fx_fold_type", "FOLD TYPE"),
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
                driveOn, driveAmount, driveMix, foldAmount, driveType, foldType,
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
        slotGroups.push_back ({ &driveOn, &driveAmount, &driveMix, &foldAmount, &driveType, &foldType });
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

        // The rack's row grid: knobs with their name and value beside the dial,
        // menus 18 px under a small name, switches with their name beside them.
        for (auto& group : slotGroups)
            for (auto* item : group)
            {
                if (auto* knob = dynamic_cast<KnobControl*> (item))
                    knob->setInlineKnob (true);
                else if (auto* combo = dynamic_cast<ComboControl*> (item))
                    combo->setCompactLayout (true);
                else if (auto* toggle = dynamic_cast<ToggleControl*> (item))
                    toggle->setInlineLabel (true);
            }

        diceButton.setTooltip ("Randomise FX\nRandomises the FX chain: new effects (each once) in every slot of this chain. Undo brings the old chain back.");
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
        addButton.onClick = [this] { showLibrary (addButton, addButton.getLocalBounds()); };
        addTile.onClick = [this] { showLibrary (addTile, addTile.getLocalBounds().withSizeKeepingCentre (120, addTile.getHeight())); };
        addChildComponent (addTile);
        for (const auto type : FxSuggestTile::candidates())
        {
            auto tile = std::make_unique<FxSuggestTile> (type);
            tile->onClick = [this, type] { pickFromLibrary (type); };
            addChildComponent (*tile);
            suggestTiles.push_back (std::move (tile));
        }
        fileButton.setTooltip ("This chain as a whole: copy it over the other chain, save it to a file, or load one into it");
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
        addAndMakeVisible (addButton);
        addAndMakeVisible (fileButton);
        IlanaTheme::makePill (chainAButton, IlanaTheme::accent());
        IlanaTheme::makePill (chainBButton, IlanaTheme::accent());
        addAndMakeVisible (chainAButton);
        addAndMakeVisible (chainBButton);

        // SERIES / PARALLEL: the rack's routing (fx_routing), one undo step a
        // click. A mode, as the slot types, so not automatable (no MIDI learn).
        seriesButton.setTooltip ("Series\nEach effect feeds the next one, top to bottom.");
        parallelButton.setTooltip ("Parallel\nEvery effect hears the rack's input; their outputs are summed "
                                   "(averaged, so effects that leave the sound alone give it back unchanged).");
        seriesButton.setDescription ("FX routing: series");
        parallelButton.setDescription ("FX routing: parallel");
        for (auto* button : { &seriesButton, &parallelButton })
        {
            IlanaTheme::makePill (*button, IlanaTheme::accent());
            button->setRadioGroupId (0x5e71a1);
            addAndMakeVisible (*button);
        }
        seriesButton.onClick = [this] { setRouting (false); };
        parallelButton.onClick = [this] { setRouting (true); };
        if (auto* routing = p.apvts.getParameter ("fx_routing"))
        {
            routingAttachment = std::make_unique<juce::ParameterAttachment> (
                *routing, [this] (float value) { routingChanged (value > 0.5f); }, nullptr);
            routingAttachment->sendInitialUpdate();
        }
        addChildComponent (copyChainButton); // (its action is in the CHAIN menu)
        tapGrid.setVisible (false);
        tapGrid.setName ("CUSTOM TAP GRID"); // (the UI test finds it by name)

        // The final stage after the rack.
        softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "ON"); // its name is drawn at its left (review 11, S11-14)
        softClip->showAsSwitch();
        softClip->setBareSwitch (true);
        clipGain = std::make_unique<StripKnob> (p, "master_clip_gain", "Clip Gain");
        addAndMakeVisible (*softClip);
        addAndMakeVisible (*clipGain);
        outputMeter = std::make_unique<OutputMeter> (p);
        addAndMakeVisible (*outputMeter);

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
                // The picker, as + ADD EFFECT's, swapping this slot's effect
                // (its bypass, solo, band and move are on the card and its
                // right-click menu).
                select (slot);
                auto& button = cardHeaders[(size_t) slot].type;
                showLibrary (button, button.getLocalBounds(), slot);
            };
            stackContent.addChildComponent (header.type);

            header.solo.setClickingTogglesState (true);
            header.solo.setTooltip ("SOLO: hear only this slot's effect (wet only)");
            IlanaTheme::makePill (header.solo, IlanaTheme::accent());
            if (auto* solo = p.apvts.getParameter (prefix + "_solo"))
                header.soloAttachment = std::make_unique<juce::ButtonParameterAttachment> (*solo, header.solo, nullptr);
            stackContent.addChildComponent (header.solo);

            header.band.addItemList ({ "Full Band", "Low Band", "Mid Band", "High Band", "M/S Mid", "M/S Side" }, 1);
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
            header.mix->setInlineKnob (true);
            stackContent.addChildComponent (*header.mix);

            header.model.onSwitch = [this, slot]
            {
                const auto other = fxTwinOf (getSlotType (slot));
                if (other > 0 && slotHoldingType (other, slot) < 0)
                    processorRef.performEdit ("Change effect to " + getSlotName (other), [this, slot, other] { processorRef.assignFxSlot (slot + 1, other); });
                updateVisibility();
            };
            // (The model switch's logic stays; the row shows it as the engine tag.)
            header.chip.onClick = [&header] { if (header.model.onSwitch != nullptr) header.model.onSwitch(); };
            stackContent.addChildComponent (header.chip);

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

        // The top bar: the two chains as one switch, the routing and what it means.
        IlanaTheme::paintWell (g, chainSeg.toFloat(), (float) chainSeg.getHeight() * 0.5f);
        IlanaTheme::paintWell (g, routingSeg.toFloat(), (float) routingSeg.getHeight() * 0.5f);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (routingHintText(), routingHint, juce::Justification::centredLeft, true);

        // The rail's ends: IN above the first effect, OUT under the last. In
        // PARALLEL two buses leave IN and reach OUT: the input's on the left,
        // which every effect taps, and the sum's on the right.
        const auto railX = (float) inColumn.getCentreX();
        if (! stackView.isVisible())
        {
            paintRailPill (g, inColumn, "IN");
            return paintOutput (g);
        }

        if (parallelRouting)
        {
            const auto inX = (float) inColumn.getX() + (float) parallelInBusX;
            const auto outX = (float) outColumn.getX() + (float) parallelOutBusX;
            paintRailLine (g, inX, (float) inColumn.getCentreY() + 8.0f, (float) stackView.getY() + 1.0f, railGood(), false);
            paintRailLine (g, outX, (float) stackView.getBottom() - 1.0f, (float) outColumn.getCentreY() - 8.0f, railGood(), false);
            paintRailArrow (g, outX, (float) outColumn.getCentreY() - 8.0f, railGood(), false);
        }
        else
        {
            paintRailLine (g, railX, (float) inColumn.getCentreY() + 11.0f, (float) stackView.getY() + 1.0f, firstRailColour(), false);
            paintRailLine (g, railX, (float) stackView.getBottom() - 1.0f, (float) outColumn.getCentreY() - 11.0f, lastRailColour(), false);
            paintRailArrow (g, railX, (float) outColumn.getCentreY() - 11.0f, lastRailColour(), false);
        }
        paintRailPill (g, inColumn, "IN");
        paintRailPill (g, outColumn, "OUT");
        paintOutput (g);

        if (! suggestCaption.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
            g.drawText ("SUGGESTED NEXT", suggestCaption, juce::Justification::bottomLeft, false);
        }
    }

    void paintOutput (juce::Graphics& g)
    {
        if (outputStrip.isEmpty())
            return;

        if (! stackView.isVisible())
            paintRailPill (g, outColumn, "OUT");

        IlanaTheme::paintCard (g, outputStrip.toFloat(), 8.0f, IlanaTheme::Ui::line);
        IlanaTheme::paintCardTitle (g, outputTitle, "OUTPUT", IlanaTheme::Ui::text2);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText ("SOFT CLIP", outputSoftLabel, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (outputSeparator);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("after the rack, before the master volume", outputHint, juce::Justification::centredRight, true);

        if (library->isVisible())
        {
            auto heading = emptyHeading;
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            g.drawText ("The rack is empty", heading.removeFromTop (26), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Pick an effect to start the chain. Each one goes into the next slot; drag a row's name to reorder them.",
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
            // The card cut off under the toolbar is hidden almost wholly, then
            // fades in: its half-drawn controls no longer show through (N16-5).
            const auto head = stackView.getBounds().removeFromTop (34).toFloat();
            auto fade = juce::ColourGradient (background, 0.0f, head.getY(), background.withAlpha (0.0f), 0.0f, head.getBottom(), false);
            fade.addColour (0.55, background.withAlpha (0.92f));
            g.setGradientFill (fade);
            g.fillRect (head);
        }
    }

    void resized() override
    {
        // A 12 px gutter at the right, as the other pages keep (N16-4).
        auto area = getLocalBounds().reduced (0, 8).withTrimmedRight (12);

        // The top bar: IN above the rail, the two chains, the routing; at the
        // right the chain's file menu, + ADD and the dice.
        auto toolbar = area.removeFromTop (toolbarHeight);
        inColumn = toolbar.removeFromLeft (railWidth);
        toolbar.removeFromLeft (railGap);
        chainSeg = toolbar.removeFromLeft (136);
        chainAButton.setBounds (chainSeg.withTrimmedLeft (3).removeFromLeft (64).reduced (0, 2));
        chainBButton.setBounds (chainSeg.withTrimmedLeft (3 + 66).removeFromLeft (64).reduced (0, 2));
        toolbar.removeFromLeft (10);
        routingSeg = toolbar.removeFromLeft (152);
        seriesButton.setBounds (routingSeg.withTrimmedLeft (3).removeFromLeft (66).reduced (0, 2));
        parallelButton.setBounds (routingSeg.withTrimmedLeft (3 + 68).removeFromLeft (78).reduced (0, 2));
        toolbar.removeFromLeft (10);
        diceButton.setBounds (toolbar.removeFromRight (112).reduced (0, 2));
        toolbar.removeFromRight (6);
        addButton.setBounds (toolbar.removeFromRight (62).reduced (0, 2));
        toolbar.removeFromRight (6);
        fileButton.setBounds (toolbar.removeFromRight (84).reduced (0, 2));
        routingHint = toolbar.withTrimmedRight (10);
        copyChainButton.setVisible (false);
        area.removeFromTop (rowGap - stackTopMargin);

        auto outRow = area.removeFromBottom (outputHeight);
        area.removeFromBottom (rowGap);
        auto stackArea = area;
        addTile.setVisible (false);

        // An empty rack shows the library in the stack's place (one view
        // of the chain, not a list beside the cards: S5-18, S6-25).
        const auto empty = stackPanelsWouldBeEmpty();
        library->setVisible (empty);
        stackView.setVisible (! empty);

        if (empty)
        {
            auto box = stackArea.reduced (12, 0);
            emptyHeading = box.removeFromTop (60).withTrimmedTop (6);
            library->setBounds (box.withHeight (juce::jmin (box.getHeight(), FxLibraryView::preferredHeight() + 170)));
        }

        // The rows take their own height (stretched to 88 at most, squeezed to
        // 64 when the rack is long, scrolling beyond); OUTPUT follows the last
        // of them (V7-29, S7-16: no empty band above it). A short rack with a
        // free slot ends in a dashed "+ ADD EFFECT" row that takes the rest of
        // the height, so OUTPUT closes the page where the design puts it
        // (design round 2: no empty band under OUTPUT either).
        const auto offersTile = ! empty && firstEmptySlot() >= 0;
        // The add area: the + ADD EFFECT target, then a row of suggested next
        // effects (N16-1). A tight rack keeps the target alone, and a full one
        // scrolls with neither.
        const auto fullReserve = addTileHeight + suggestCaptionHeight + suggestMinHeight + rowGap;
        growRows = offersTile;
        availableStackHeight = stackArea.getHeight() - (offersTile ? fullReserve : 0);
        stackView.setBounds (stackArea);
        layoutStack();
        auto tile = offersTile && stackNaturalHeight <= availableStackHeight;
        auto suggestions = tile;
        if (offersTile && ! tile)
        {
            availableStackHeight = stackArea.getHeight() - (addTileMinimum + rowGap);
            layoutStack();
            tile = stackNaturalHeight <= availableStackHeight;
        }
        if (offersTile && ! tile)
        {
            growRows = false;
            availableStackHeight = stackArea.getHeight();
            layoutStack();
        }
        for (auto& suggest : suggestTiles)
            suggest->setVisible (false);
        suggestCaption = {};
        if (! empty)
        {
            stackView.setBounds (stackArea.withHeight (juce::jmin (stackArea.getHeight(), stackNaturalHeight)));
            layoutStack();
            // (The chip under the last row reaches past the gap, so the view's foot margin
            // is 4 px more than it: OUTPUT closes up on it unless the rack scrolls.)
            outRow.setY (stackNaturalHeight < stackArea.getHeight() ? stackView.getBottom() - (stackFootMargin - rowGap)
                                                                    : stackView.getBottom() + rowGap);
            if (tile)
            {
                const auto top = stackView.getBottom() - (stackFootMargin - rowGap);
                const auto left = stackArea.getX() + railWidth + railGap, width = stackContent.getWidth() - railWidth - railGap;
                const auto block = stackArea.getBottom() - top;
                addTile.setBounds (left, top, width, suggestions ? addTileHeight : block);
                addTile.setVisible (true);
                if (suggestions)
                {
                    std::vector<int> offered;
                    for (const auto type : FxSuggestTile::candidates())
                        if (slotHoldingType (type) < 0 && (fxTwinOf (type) < 0 || slotHoldingType (fxTwinOf (type)) < 0))
                            offered.push_back (type);
                    constexpr int gap = 10, minWidth = 190;
                    // One row of tiles, or two when the rack leaves the height for them.
                    const auto perRow = juce::jmax (1, std::min ({ 4, (int) offered.size(), (width + gap) / (minWidth + gap) }));
                    const auto room = stackArea.getBottom() - addTile.getBottom() - suggestCaptionHeight;
                    const auto rows = (int) offered.size() > perRow ? juce::jlimit (1, 2, (room + gap) / (suggestMinHeight + gap)) : 1;
                    const auto count = juce::jmin ((int) offered.size(), perRow * rows);
                    const auto tileHeight = juce::jlimit (suggestMinHeight, suggestMaxHeight, (room - gap * (rows - 1)) / rows);
                    suggestCaption = { left, addTile.getBottom() + 6, width, suggestCaptionHeight - 6 };
                    const auto top = addTile.getBottom() + suggestCaptionHeight;
                    for (int i = 0; i < count; ++i)
                        for (auto& suggest : suggestTiles)
                            if (suggest->getType() == offered[(size_t) i])
                            {
                                // (One column grid for every row: a short last row keeps the tiles' width.)
                                const auto each = (width - gap * (perRow - 1)) / perRow;
                                suggest->setBounds (left + (i % perRow) * (each + gap), top + (i / perRow) * (tileHeight + gap), each, tileHeight);
                                suggest->setVisible (true);
                            }
                    if (count == 0)
                        suggestCaption = {};
                }
                outRow.setY (stackArea.getBottom() + rowGap);
            }
        }

        outputStrip = outRow.withTrimmedLeft (railWidth + railGap);
        outColumn = outRow.withWidth (railWidth);
        {
            auto strip = outputStrip.reduced (14, 0);
            outputTitle = strip.removeFromLeft (64);
            strip.removeFromLeft (18);
            outputSoftLabel = strip.removeFromLeft (68);
            strip.removeFromLeft (6);
            softClip->setBounds (strip.removeFromLeft (40).withSizeKeepingCentre (40, 22));
            strip.removeFromLeft (18);
            outputSeparator = strip.removeFromLeft (1).withSizeKeepingCentre (1, 26);
            strip.removeFromLeft (18);
            clipGain->setBounds (strip.removeFromLeft (160).reduced (0, 3));
            outputMeter->setBounds (strip.removeFromRight (130).withSizeKeepingCentre (130, 30));
            strip.removeFromRight (14);
            outputHint = strip;
        }
        updateAddButton();
        repaint();
    }

    void updateAddButton()
    {
        const auto free = firstEmptySlot() >= 0;
        addButton.setEnabled (free);
        addTile.setTooltip ("Add an effect after the last one: the library, every effect grouped by what it does");
        addButton.setTooltip (free ? "Add an effect to the next empty slot: the library, every effect grouped by what it does"
                                   : "Every slot is in use: remove an effect to add another.");
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

    // A library pick: a new effect goes into the first empty slot, or into
    // replacingSlot in place of its effect (a card's type button); one the
    // rack already holds is shown instead.
    void pickFromLibrary (int type, int replacingSlot = -1)
    {
        if (replacingSlot >= 0 && getSlotType (replacingSlot) == type)
            return;

        if (const auto holder = slotHoldingType (type); holder >= 0)
        {
            select (holder);
            scrollToSlot (holder);
            dropSlot = holder;
            dropFlash = 1.0f;
            return;
        }

        const auto slot = replacingSlot >= 0 ? replacingSlot : firstEmptySlot();
        if (slot < 0)
            return;

        processorRef.performEdit ((replacingSlot >= 0 ? "Change effect to " : "Add ") + getSlotName (type),
                                  [this, slot, type] { processorRef.assignFxSlot (slot + 1, type); });
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

    // The editor's scaled content (the page's ancestor under the editor), so
    // the call-out is drawn at the UI zoom like the page around it; it was
    // added to the unscaled window and came out at half size, its text about
    // 7 px, on a zoomed editor (ilana's PC test).
    juce::Component* calloutHost()
    {
        auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
        if (editor == nullptr)
            return getTopLevelComponent();
        juce::Component* host = this;
        while (host->getParentComponent() != nullptr && host->getParentComponent() != editor)
            host = host->getParentComponent();
        return host->getParentComponent() == editor ? host : editor;
    }

    // The library in a call-out over the page: adding (slot -1) or swapping a
    // slot's effect (its card's type button).
    void showLibrary (juce::Component& target, juce::Rectangle<int> area, int slot = -1)
    {
        auto* host = calloutHost();
        if (host == nullptr)
            return;

        juce::Component::SafePointer<FxPage> safeThis (this);
        auto view = std::make_unique<FxLibraryView> ([safeThis] (int type) { return safeThis != nullptr ? safeThis->slotHoldingType (type) : -1; },
                                                     [safeThis, slot] (int type)
                                                     {
                                                         if (safeThis != nullptr)
                                                             safeThis->pickFromLibrary (type, slot);
                                                     });
        auto* raw = view.get();
        view->afterPick = [raw]
        {
            if (auto* box = raw->findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        if (slot >= 0)
        {
            view->setReplacing (slot, getSlotType (slot));
            view->onRemove = [safeThis, slot]
            {
                if (safeThis != nullptr)
                    safeThis->removeSlot (slot);
            };
        }
        view->setPadding (FxLibraryView::calloutPadding);
        view->setSize (juce::jmin (FxLibraryView::calloutWidth, host->getWidth() - 2 * 14 - 8),
                       FxLibraryView::preferredHeight() + FxLibraryView::calloutPadding.getTopAndBottom());
        view->setName ("FX LIBRARY"); // (the UI test finds it by name)

        // The page behind it dims while it is open.
        host->addAndMakeVisible (pickerDimmer);
        pickerDimmer.setBounds (host->getLocalBounds());
        pickerDimmer.toFront (false);
        view->onGone = [safeThis]
        {
            if (safeThis != nullptr)
                if (auto* parent = safeThis->pickerDimmer.getParentComponent())
                    parent->removeChildComponent (&safeThis->pickerDimmer);
        };

        const auto pointAt = host->getLocalArea (&target, area);
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (view), pointAt, host);
        box.setLookAndFeel (&pickerLook);
        box.setArrowSize (12.0f);
        box.updatePosition (pointAt, host->getLocalBounds()); // (with the picker's border)
    }

    void showFileMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (3, processorRef.isShowingChainA() ? "Copy to chain 2..." : "Copy to chain 1...");
        menu.addSeparator();
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
                                else if (result == 3 && safeThis->copyChainButton.onClick != nullptr)
                                    safeThis->copyChainButton.onClick();
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

    // The routing switch: a click is one named undo step; the parameter
    // (a click, undo, a patch or chain load) sets the switch and the rail.
    void setRouting (bool parallel)
    {
        if (routingAttachment == nullptr || parallel == parallelRouting)
            return routingChanged (parallelRouting);

        processorRef.performEdit (parallel ? "FX routing: parallel" : "FX routing: series",
                                  [this, parallel] { routingAttachment->setValueAsCompleteGesture (parallel ? 1.0f : 0.0f); });
    }

    void routingChanged (bool parallel)
    {
        seriesButton.setToggleState (! parallel, juce::dontSendNotification);
        parallelButton.setToggleState (parallel, juce::dontSendNotification);
        if (parallel == parallelRouting)
            return;
        parallelRouting = parallel;
        repaint();
        stackContent.repaint();
    }

    juce::String routingHintText() const
    {
        return parallelRouting ? "every effect hears the input, outputs are summed" : "each effect feeds the next one";
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

    // One row per loaded slot, in chain order (the design's signal rail:
    // the rail at the left, then a headless row: number, name, solo, power
    // and engine; the effect's live picture; its knobs). A module type
    // loaded twice shares its settings, so later copies say so and offer
    // REMOVE.
    struct StackPanel
    {
        int slot = 0, type = 0;
        bool duplicate = false;
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> well;   // the picture slot of a row with no display: a sentence on what the effect does
        juce::Rectangle<int> handle; // the left block: where a drag reorders the chain
        juce::Rectangle<int> number; // the slot's number badge
    };

    // Banded slots next to each other, drawn as one bracketed group: a
    // LOW / MID / HIGH split with its crossovers, or a MID / SIDE one.
    struct SplitGroup
    {
        juce::Rectangle<int> bounds;
        bool frequency = true;
    };

    // Row geometry (px of the 1060 x 720 design): the rail is 56 wide, a row
    // is 80 high (64 when the rack is long, 88 when it is short: its picture and
    // its dials grow with it), its left block 156 wide, its knobs
    // 116 x 44 cells with the dial's name and value beside it.
    // The parallel ladder's buses, x in the rail (inside the IN / OUT pills).
    static constexpr int parallelInBusX = 13, parallelOutBusX = 43;
    static constexpr int railWidth = 56, railGap = 10, rowGap = 10, rowPad = 8, leftWidth = 156, cellHeight = 44;
    static constexpr int rowHeightStandard = 80, rowHeightCompact = 64, rowHeightMost = 88, duplicateHeight = 56, addTileMinimum = 40;
    // The add area under a short rack: the target, a caption, the suggested tiles; and how far a card's picture and dials may grow into spare height.
    static constexpr int addTileHeight = 36, suggestCaptionHeight = 22, suggestMinHeight = 70, suggestMaxHeight = 104, rowGrowMax = 90;
    static constexpr int minDisplayWidth = 150, knobCellWidth = 116, tapGridHeight = 56;
    static constexpr int splitHeaderHeight = 38, splitInsetLeft = 18, splitInsetRight = 6;
    static constexpr int toolbarHeight = 28, outputHeight = 44, stackTopMargin = 4, stackFootMargin = 14;

    static int splitKind (int band) { return band >= 1 && band <= 3 ? 1 : (band >= 4 ? 2 : 0); }

    // The knob grid: up to five across, as many rows as the controls need.
    static int columnsFor (int count) { return count <= 5 ? juce::jmax (1, count) : (count == 6 ? 3 : (count <= 8 ? 4 : 5)); }
    static int rowsFor (int count) { return count <= 0 ? 1 : (count + columnsFor (count) - 1) / columnsFor (count); }

    // The controls a card lays out in its rows (its on switch goes in the
    // left block; an Airwindows knob the algorithm doesn't use is skipped).
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

        // One on switch per module, at the row's switch place: its own
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

    // A row's controls: its module's, then the slot's MIX where the
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

    // Everything the knob grid holds: the controls, the band menu of a banded
    // slot (first) and the reverb's LOAD IR (last).
    std::vector<juce::Component*> gridItems (const StackPanel& panel, ToggleControl** power = nullptr)
    {
        auto items = cardItems (panel, power);
        if (showsBand (panel.slot))
            items.insert (items.begin(), &cardHeaders[(size_t) panel.slot].band);
        if (panel.type == 13 && ! panel.duplicate)
            items.push_back (&loadIrButton);
        return items;
    }

    int gridCount (const StackPanel& panel) const
    {
        return (int) cardItems (panel).size() + (showsBand (panel.slot) ? 1 : 0) + (panel.type == 13 && ! panel.duplicate ? 1 : 0);
    }

    static bool hasCardDisplay (int type) { return type == 29 || type == 16 || FxDisplay::hasDisplay (type); }

    // Every row has a picture in its middle: the effect's display, or for one
    // without (FREEZE, the WIDENER, most Airwindows modules) a sentence on
    // what it does with its in and out levels beside it, so no row is half
    // blank (UI review 13, V13-1, I13-3).
    static bool hasCardPicture (int type) { return type > 0; }

    // The Airwindows algorithm a module type is running (its registry index), or -1.
    int airwindowsIndexFor (int type) const
    {
        if (type == 30)
            return airwindowsAlgorithm();
        if (const auto c = airwindows::categoryForFxType (type); c >= 0)
            return airwindows::categoryModules()[(size_t) c].algorithms[(size_t) categoryChoice (c)];
        return -1;
    }

    juce::String cardInfoText (int type) const
    {
        if (const auto index = airwindowsIndexFor (type); index >= 0)
            return FxInfoText::forAirwindows (airwindows::registry()[(size_t) index].name);
        return FxInfoText::forType (type);
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

    // A row's height: 80 for one line of knobs, taller for more (the grid is
    // 44 per line), plus the delay's tap grid while TAPS is on.
    int naturalHeight (const StackPanel& panel) const
    {
        if (panel.duplicate)
            return duplicateHeight;

        const auto rows = rowsFor (gridCount (panel));
        return (rows == 1 ? rowHeightStandard : 2 * rowPad + rows * cellHeight) + (panel.type == 9 && tapsEnabled() ? tapGridHeight : 0);
    }

    // The least a row compresses to when the rack is long: a single line of
    // knobs drops to 64; taller grids keep their lines at 40.
    int minimumHeight (const StackPanel& panel) const
    {
        if (panel.duplicate)
            return duplicateHeight;

        const auto rows = rowsFor (gridCount (panel));
        return (rows == 1 ? rowHeightCompact : 2 * rowPad + rows * 40) + (panel.type == 9 && tapsEnabled() ? tapGridHeight : 0);
    }

    bool isStretchy (const StackPanel& panel) const { return ! panel.duplicate && rowsFor (gridCount (panel)) == 1; }

    void layoutStack()
    {
        stackPanels.clear();
        splitGroups.clear();

        for (auto& slotSwitch : slotSwitches)
            slotSwitch->setVisible (false);
        for (auto& header : cardHeaders)
            for (juce::Component* item : { (juce::Component*) &header.type, (juce::Component*) &header.solo, (juce::Component*) &header.band,
                                           (juce::Component*) &header.remove, (juce::Component*) &header.chip,
                                           (juce::Component*) header.mix.get() })
                item->setVisible (false);
        for (auto& display : displays)
            display->setVisible (false);
        for (auto& strip : crossoverStrips)
            strip->setVisible (false);

        // The cards in chain order.
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

        // A run of cards with the same kind of band: plain, or one split group.
        struct Run { size_t from, to; int kind; };
        std::vector<Run> runs;
        for (size_t i = 0; i < cards.size();)
        {
            const auto kind = splitKind (getSlotBand (cards[i].slot));
            auto end = i + 1;
            while (end < cards.size() && splitKind (getSlotBand (cards[end].slot)) == kind)
                ++end;
            runs.push_back ({ i, end, kind });
            i = end;
        }

        // Heights: every row at its natural height; a short rack stretches its
        // single-line rows (to 88 at most), a long one compresses them (to 64)
        // before the stack scrolls.
        const auto viewHeight = availableStackHeight;
        auto overhead = stackTopMargin + stackFootMargin + juce::jmax (0, (int) cards.size() - 1) * rowGap;
        for (const auto& run : runs)
            overhead += run.kind != 0 ? splitHeaderHeight + splitInsetRight : 0;

        std::vector<int> heights;
        auto naturalTotal = overhead;
        auto stretchy = 0;
        for (const auto& card : cards)
        {
            heights.push_back (naturalHeight (card));
            naturalTotal += heights.back();
            stretchy += isStretchy (card) ? 1 : 0;
        }

        if (naturalTotal < viewHeight && (stretchy > 0 || growRows))
        {
            // Spare height goes to the cards: a short rack's single-line rows
            // stretch to 88; with a free slot under them (growRows) every
            // card's picture and dials grow into it, up to a cap, so the add
            // area under them stays small (N16-1).
            const auto natural = heights;
            auto extra = viewHeight - naturalTotal;
            for (auto pass = 0; pass < 6 && extra > 0; ++pass)
            {
                std::vector<size_t> growable;
                for (size_t i = 0; i < cards.size(); ++i)
                {
                    const auto cap = growRows ? natural[i] + (cards[i].duplicate ? 0 : rowGrowMax)
                                              : (isStretchy (cards[i]) ? rowHeightMost : natural[i]);
                    if (! cards[i].duplicate && heights[i] < cap)
                        growable.push_back (i);
                }
                if (growable.empty())
                    break;
                // (An even share: rows of one kind stay the same height; the odd pixel stays unspent.)
                const auto share = extra / (int) growable.size();
                if (share < 1)
                    break;
                for (const auto i : growable)
                {
                    const auto cap = growRows ? natural[i] + rowGrowMax : (isStretchy (cards[i]) ? rowHeightMost : natural[i]);
                    const auto add = juce::jmin (share, cap - heights[i], extra);
                    heights[i] += add;
                    extra -= add;
                }
            }
        }
        else if (naturalTotal > viewHeight)
        {
            auto deficit = naturalTotal - viewHeight;
            for (auto pass = 0; pass < 3 && deficit > 0; ++pass)
                for (size_t i = 0; i < cards.size() && deficit > 0; ++i)
                {
                    const auto room = heights[i] - minimumHeight (cards[i]);
                    if (room <= 0 || ! isStretchy (cards[i]))
                        continue;
                    const auto take = juce::jmin (room, juce::jmax (1, (deficit + stretchy - 1) / juce::jmax (1, stretchy)));
                    heights[i] -= take;
                    deficit -= take;
                }
        }

        auto contentHeight = overhead;
        for (auto height : heights)
            contentHeight += height;
        // (A scroll bar takes its width from the rows, not the rail.)
        const auto scrolls = contentHeight > viewHeight;
        const auto width = juce::jmax (100, stackView.getWidth() - (scrolls ? stackView.getScrollBarThickness() + 2 : 0));
        const auto cardX = railWidth + railGap;

        auto y = stackTopMargin; // (room for the first card's glow)
        rowsPlaced = 0;
        size_t stripIndex = 0;

        for (const auto& run : runs)
        {
            if (run.kind == 0)
            {
                for (auto k = run.from; k < run.to; ++k)
                {
                    placeCard (cards[k], { cardX, y, width - cardX, heights[k] });
                    y += heights[k] + rowGap;
                    ++rowsPlaced;
                }
                continue;
            }

            SplitGroup group;
            group.frequency = run.kind == 1;
            const auto top = y;

            if (group.frequency && stripIndex < crossoverStrips.size())
            {
                std::array<bool, 3> used {};
                for (auto k = run.from; k < run.to; ++k)
                    if (const auto band = getSlotBand (cards[k].slot); band >= 1 && band <= 3)
                        used[(size_t) band - 1] = true;
                auto& strip = *crossoverStrips[stripIndex++];
                strip.setBounds (juce::Rectangle<int> (cardX + splitInsetLeft + 150, top + 6, width - cardX - splitInsetLeft - 150 - 14, 20));
                strip.setBandsUsed (used);
                strip.setVisible (true);
            }

            y = top + splitHeaderHeight;
            for (auto k = run.from; k < run.to; ++k)
            {
                placeCard (cards[k], { cardX + splitInsetLeft, y, width - cardX - splitInsetLeft - splitInsetRight, heights[k] });
                y += heights[k] + rowGap;
                ++rowsPlaced;
            }
            group.bounds = { cardX, top, width - cardX, y - rowGap + splitInsetRight - top };
            y = group.bounds.getBottom() + rowGap;
            splitGroups.push_back (group);
        }

        updateModuleDimming();

        stackNaturalHeight = y - rowGap + stackFootMargin;
        stackRows = rowsPlaced;
        stackContent.setSize (width, juce::jmax (stackNaturalHeight, stackView.getHeight()));
        stackContent.repaint();
    }

    // The card's left block: its number and name on one line, SOLO, the
    // power switch and the engine tag under them.
    struct LeftBlock { juce::Rectangle<int> number, name, solo, power, chip; };

    static LeftBlock leftBlockFor (juce::Rectangle<int> card)
    {
        auto block = card.withTrimmedLeft (10).withWidth (leftWidth).withSizeKeepingCentre (leftWidth, 48);
        block.setX (card.getX() + 10);
        auto line1 = block.removeFromTop (22);
        block.removeFromTop (6);
        auto line2 = block.removeFromTop (20);

        LeftBlock result;
        result.number = line1.removeFromLeft (18).withSizeKeepingCentre (18, 18);
        line1.removeFromLeft (4);
        result.name = line1;
        result.solo = line2.removeFromLeft (24);
        line2.removeFromLeft (6);
        result.power = line2.removeFromLeft (36);
        line2.removeFromLeft (6);
        result.chip = line2.withHeight (20);
        return result;
    }

    void placeCard (StackPanel panel, juce::Rectangle<int> bounds)
    {
        panel.bounds = bounds;
        const auto slot = panel.slot;
        const auto type = panel.type;
        auto& header = cardHeaders[(size_t) slot];
        const auto left = leftBlockFor (bounds);
        panel.number = left.number;
        panel.handle = bounds.withWidth (leftWidth + 20);

        header.type.setTitle (cardTitle (type), isModuleOff (slot) ? IlanaTheme::Ui::text3 : fxColour (type));
        header.type.setBounds (left.name.withWidth (juce::jmin (left.name.getWidth(), header.type.preferredWidth())));
        header.type.setVisible (true);
        header.type.setTooltip ("Slot " + juce::String (slot + 1) + ": " + getSlotName (type)
                                + ". Click to change the effect, move or remove it; drag the number or name to reorder."
                                + (cardInfoText (type).isNotEmpty() ? "\n" + cardInfoText (type) : juce::String())
                                + (isAirwindowsType (type) ? "\n" + airwindowsBadgeTip (type) : juce::String()));
        header.solo.setBounds (left.solo);
        header.solo.setVisible (true);

        // The engine: BUILT-IN or AIRWINDOWS, and where there is a twin a click
        // swaps the slot to it (each model keeps its own settings).
        {
            const auto airwindows = isAirwindowsType (type);
            const auto twin = hasModelSwitch (type) ? fxTwinOf (type) : -1;
            const auto free = twin > 0 && slotHoldingType (twin, slot) < 0;
            header.chip.setState (airwindows ? "AIRWINDOWS" : "BUILT-IN", fxColour (type), free);
            header.chip.setBounds (left.chip.withWidth (juce::jmin (left.chip.getWidth(), 88)));
            header.chip.setTooltip (twin > 0
                                        ? "Model: " + getSlotName (airwindows ? twin : type) + " built in, or its Airwindows model, "
                                              + getSlotName (airwindows ? type : twin) + ". Click the other to swap this slot's effect (each keeps its own settings)."
                                              + (slotHoldingType (twin, slot) >= 0 ? "\nThe other model is in slot " + juce::String (slotHoldingType (twin, slot) + 1) + " already." : juce::String())
                                              + "\n" + airwindowsBadgeTip (airwindows ? type : twin)
                                        : (airwindows ? airwindowsBadgeTip (type) : "This effect is part of the synth (no Airwindows model)."));
            header.chip.setVisible (true);
        }

        auto inner = bounds.reduced (10, rowPad).withTrimmedLeft (leftWidth + 10);

        if (panel.duplicate)
        {
            // Duplicates share settings but bypass on their own.
            slotSwitches[(size_t) slot]->setBare (true);
            slotSwitches[(size_t) slot]->setVisible (true);
            slotSwitches[(size_t) slot]->setBounds (left.power.withHeight (20));
            auto& remove = header.remove;
            remove.setBounds (juce::Rectangle<int> (bounds.getRight() - 12 - 84, bounds.getCentreY() - 12, 84, 24));
            remove.setVisible (true);
            panel.well = {};
            stackPanels.push_back (panel);
            return;
        }

        ToggleControl* power = nullptr;
        const auto items = gridItems (panel, &power);
        if (showsSlotMix (panel))
            header.mix->setVisible (true);
        if (showsBand (slot))
            header.band.setVisible (true);

        if (power != nullptr)
        {
            power->setInlineLabel (false);
            power->setBareSwitch (true);
            power->setBounds (left.power.withHeight (20));
        }
        else
        {
            slotSwitches[(size_t) slot]->setBare (true);
            slotSwitches[(size_t) slot]->setVisible (true);
            slotSwitches[(size_t) slot]->setBounds (left.power.withHeight (20));
        }

        // The delay's tap grid under the row while TAPS is on.
        if (type == 9 && tapsEnabled())
            tapGrid.setBounds (inner.removeFromBottom (tapGridHeight - 4).withTrimmedBottom (2).withTrimmedTop (2));

        // The knobs in their cells at the right, the picture taking the rest.
        const auto count = (int) items.size();
        const auto columns = columnsFor (count), rows = rowsFor (count);
        const auto room = inner.getWidth() - minDisplayWidth - 10;
        const auto cellWidth = juce::jlimit (92, knobCellWidth, count > 0 ? room / juce::jmax (1, columns) : knobCellWidth);
        auto knobsArea = count > 0 ? inner.removeFromRight (columns * cellWidth) : juce::Rectangle<int>();
        if (count > 0)
            inner.removeFromRight (10);

        panel.well = {};
        if (hasCardPicture (type))
        {
            const auto displayArea = inner;
            if (type == 29)
                eqCurve.setBounds (displayArea);
            else if (type == 16)
                gateGrid->setBounds (displayArea);
            else if (FxDisplay::hasDisplay (type))
            {
                auto& display = *displays[(size_t) slot];
                display.setType (type, fxColour (type));
                display.setBounds (displayArea);
                display.setVisible (true);
            }
            else
                panel.well = displayArea;
        }

        const auto rowHeight = knobsArea.getHeight() / juce::jmax (1, rows);
        for (int i = 0; i < count; ++i)
        {
            auto cell = juce::Rectangle<int> (knobsArea.getX() + (i % columns) * cellWidth, knobsArea.getY() + (i / columns) * rowHeight,
                                              cellWidth, rowHeight);
            auto* item = items[(size_t) i];

            // The dial grows with the row (a short rack's rows are taller), so
            // a tall row's knobs fill it instead of floating in a band.
            if (auto* knob = dynamic_cast<KnobControl*> (item))
                knob->setInlineDial (juce::jlimit (40, 56, rowHeight - 16));

            if (dynamic_cast<ComboControl*> (item) != nullptr)
                item->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 30));
            else if (dynamic_cast<ToggleControl*> (item) != nullptr)
                item->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 24));
            else if (item == &cardHeaders[(size_t) slot].band)
                item->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 10, 22));
            else if (item == &loadIrButton)
                item->setBounds (cell.withSizeKeepingCentre (86, 22));
            else
                item->setBounds (cell.reduced (2, 0));
        }

        stackPanels.push_back (panel);
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

    static juce::Colour railGood() { return IlanaTheme::oscColour (2); }

    // The first and the last loaded effects' colours (the rail's ends).
    juce::Colour firstRailColour() const
    {
        return stackPanels.empty() ? IlanaTheme::Ui::line : fxColour (stackPanels.front().type);
    }
    juce::Colour lastRailColour() const
    {
        return stackPanels.empty() ? IlanaTheme::Ui::line : fxColour (stackPanels.back().type);
    }

    // A node on the rail: IN or OUT, a pill in the signal's green.
    static void paintRailPill (juce::Graphics& g, juce::Rectangle<int> column, const juce::String& text)
    {
        const auto pill = juce::Rectangle<float> (36.0f, 22.0f).withCentre (column.toFloat().getCentre());
        g.setColour (railGood().withAlpha (0.18f));
        g.fillRoundedRectangle (pill, 11.0f);
        g.setColour (railGood().withAlpha (0.7f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 11.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (text, pill, juce::Justification::centred);
    }

    // The rail's line in a slot's colour: solid while the slot works, dashed
    // and faint while it is bypassed or off.
    static void paintRailLine (juce::Graphics& g, float x, float from, float to, juce::Colour colour, bool dashed)
    {
        g.setColour (colour.withAlpha (dashed ? 0.45f : 0.8f));
        if (dashed)
        {
            const float pattern[] { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (x, from, x, to), pattern, 2, 2.0f);
        }
        else
            g.fillRect (x - 1.0f, from, 2.0f, to - from);
    }

    static void paintRailArrow (juce::Graphics& g, float x, float tipY, juce::Colour colour, bool dim)
    {
        juce::Path arrow;
        arrow.addTriangle (x - 4.0f, tipY - 6.0f, x + 4.0f, tipY - 6.0f, x, tipY);
        g.setColour (colour.withAlpha (dim ? 0.45f : 0.9f));
        g.fillPath (arrow);
    }

    // The signal rail down the left of the rows: one node per effect in its
    // colour, the gain change of each slot in a chip between it and the next,
    // and a dashed node and BYPASS for a slot that is off.
    void paintRail (juce::Graphics& g)
    {
        if (parallelRouting)
            return paintParallelRail (g);

        const auto x = (float) railWidth * 0.5f;

        for (size_t i = 0; i < stackPanels.size(); ++i)
        {
            const auto& panel = stackPanels[i];
            const auto colour = fxColour (panel.type);
            const auto off = isModuleOff (panel.slot);
            const auto top = (float) panel.bounds.getY();
            const auto nextTop = i + 1 < stackPanels.size() ? (float) stackPanels[i + 1].bounds.getY() : (float) panel.bounds.getBottom() + (float) rowGap;
            const auto centre = (float) panel.bounds.getCentreY();

            paintRailLine (g, x, top - (i == 0 ? 6.0f : 0.0f), nextTop, colour, off);
            paintRailArrow (g, x, nextTop, colour, off);
            paintRailNode (g, x, centre, colour, off);
            paintRailChip (g, panel, x);
        }
    }

    // A slot's node on the rail: filled in its colour, hollow while it is off.
    static void paintRailNode (juce::Graphics& g, float x, float centre, juce::Colour colour, bool off)
    {
        const auto node = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, centre });
        g.setColour (off ? IlanaTheme::Ui::raised : colour);
        g.fillEllipse (node);
        g.setColour (colour.withAlpha (off ? 0.6f : 1.0f));
        g.drawEllipse (node.reduced (1.0f), 2.0f);
    }

    // PARALLEL: a ladder. The input's bus runs down the left from IN, the
    // sum's down the right to OUT; each effect's node sits on a rung between
    // them (dashed while it is off), so every effect hears the input and
    // every output joins the sum. The chips read each branch's own gain.
    void paintParallelRail (juce::Graphics& g)
    {
        if (stackPanels.empty())
            return;

        const auto x = (float) railWidth * 0.5f;
        const auto inX = (float) parallelInBusX, outX = (float) parallelOutBusX;
        const auto firstCentre = (float) stackPanels.front().bounds.getCentreY();
        const auto lastCentre = (float) stackPanels.back().bounds.getCentreY();

        paintRailLine (g, inX, 0.0f, lastCentre + 1.0f, railGood(), false);
        paintRailLine (g, outX, firstCentre - 1.0f, (float) stackContent.getHeight(), railGood(), false);

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto off = isModuleOff (panel.slot);
            const auto centre = (float) panel.bounds.getCentreY();

            paintRailRung (g, inX + 1.0f, x - 6.0f, centre, colour, off);
            paintRailRung (g, x + 6.0f, outX - 1.0f, centre, colour, off);
            paintRailNode (g, x, centre, colour, off);
        }

        for (const auto& panel : stackPanels)
            paintRailChip (g, panel, x);
    }

    // A rung of the parallel ladder, left to right, its arrow at the right end.
    static void paintRailRung (juce::Graphics& g, float from, float to, float y, juce::Colour colour, bool dim)
    {
        g.setColour (colour.withAlpha (dim ? 0.45f : 0.8f));
        if (dim)
        {
            const float pattern[] { 3.0f, 2.0f };
            g.drawDashedLine (juce::Line<float> (from, y, to - 4.0f, y), pattern, 2, 2.0f);
        }
        else
            g.fillRect (from, y - 1.0f, to - 4.0f - from, 2.0f);

        juce::Path arrow;
        arrow.addTriangle (to - 5.0f, y - 3.5f, to - 5.0f, y + 3.5f, to, y);
        g.setColour (colour.withAlpha (dim ? 0.45f : 0.9f));
        g.fillPath (arrow);
    }

    // The chip in the gap under a row: what the slot did to the level.
    void paintRailChip (juce::Graphics& g, const StackPanel& panel, float x)
    {
        {
            const auto off = isModuleOff (panel.slot);
            const auto chipCentre = juce::Point<float> (x, (float) panel.bounds.getBottom() + (float) rowGap * 0.5f);
            const auto slot = panel.slot;
            const auto text = off ? juce::String ("BYPASS") : railText (slot);
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (font), text);
            const auto chipWidth = (off ? 0.0f : 21.0f) + textWidth + 12.0f;
            const auto chip = juce::Rectangle<float> (chipWidth, 16.0f).withCentre (chipCentre);
            g.setColour (IlanaTheme::Ui::well);
            g.fillRoundedRectangle (chip, 8.0f);
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (chip.reduced (0.5f), 8.0f, 1.0f);

            auto inside = chip.reduced (6.0f, 0.0f);
            if (! off)
            {
                const auto bar = inside.removeFromLeft (16.0f).withSizeKeepingCentre (16.0f, 4.0f);
                inside.removeFromLeft (5.0f);
                g.setColour (IlanaTheme::Ui::track);
                g.fillRoundedRectangle (bar, 2.0f);
                const auto fill = railFill (slot);
                g.setColour (fill > 0.97f ? juce::Colour (0xffff4f5e) : railGood());
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fill), 2.0f);
            }
            g.setColour (off ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text2);
            g.setFont (font);
            g.drawText (text, inside, juce::Justification::centred);
        }
    }

    // The slot's gain change as the chip reads it: "+2.4 dB", or a dash while
    // nothing passes through (nothing to compare).
    juce::String railText (int slot) const
    {
        const auto in = processorRef.getFxSlotInLevel (slot);
        if (in < 1.0e-4f)
            return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94 dB"));
        const auto db = juce::Decibels::gainToDecibels (processorRef.getFxSlotOutLevel (slot) / in, -60.0f);
        if (std::abs (db) < 0.05f)
            return "0.0 dB";
        return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    // The level leaving the slot, 0 to 1 over -48 to 0 dB, for the chip's bar.
    float railFill (int slot) const
    {
        const auto db = juce::Decibels::gainToDecibels (processorRef.getFxSlotOutLevel (slot), -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f);
    }

    void paintStack (juce::Graphics& g)
    {
        for (const auto& group : splitGroups)
            paintSplitGroup (g, group);

        paintRail (g);

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto bounds = panel.bounds.toFloat();
            const auto selected = panel.slot == selectedSlot;
            const auto off = isModuleOff (panel.slot);

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

            // The slot's number, a small badge before the name.
            const auto badge = panel.number.toFloat();
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (badge, 5.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true, true));
            g.drawText (juce::String (panel.slot + 1), badge, juce::Justification::centred);

            if (panel.duplicate)
            {
                auto text = panel.bounds.withTrimmedLeft (leftWidth + 20).withTrimmedRight (110).reduced (4, 6);
                const auto first = slotHoldingType (panel.type, panel.slot);
                g.setColour (IlanaTheme::Ui::text);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
                IlanaTheme::drawFitted (g, "Duplicate " + getSlotName (panel.type) + ": it shares slot " + juce::String (first + 1) + "'s settings.",
                                        text.removeFromTop (20), juce::Justification::centredLeft);
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                IlanaTheme::drawFitted (g, "The rack keeps one set of settings per effect. Remove this slot, or keep it to run the same sound twice.",
                                        text, juce::Justification::topLeft, 2);
            }

            // The picture slot of a row without a display: what the effect
            // does, and its level in and out beside it, live.
            if (! panel.well.isEmpty() && ! panel.duplicate)
                paintInfoWell (g, panel, off);

            // A card that just arrived or moved flashes once.
            if ((panel.slot == dropSlot || dropSlot == -2) && dropFlash > 0.01f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (dropFlash * 0.5f));
                g.drawRoundedRectangle (bounds.expanded (dropFlash * 3.0f), 9.0f, 2.0f);
            }
        }
    }

    void paintInfoWell (juce::Graphics& g, const StackPanel& panel, bool off)
    {
        const auto well = panel.well.toFloat();
        IlanaTheme::paintWell (g, well, 6.0f);
        auto inner = panel.well.reduced (8, 0);

        // IN and OUT meters at the right, the live reading of what the
        // effect does to the level.
        if (inner.getWidth() > 230)
        {
            auto meters = inner.removeFromRight (86).reduced (0, 8);
            inner.removeFromRight (10);
            const auto slot = panel.slot;
            const auto in = off ? 0.0f : processorRef.getFxSlotInLevel (slot), out = off ? 0.0f : processorRef.getFxSlotOutLevel (slot);
            const auto draw = [&] (juce::Rectangle<int> row, const char* name, float level)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText (name, row.removeFromLeft (22), juce::Justification::centredLeft);
                const auto bar = row.toFloat().withSizeKeepingCentre ((float) row.getWidth(), 6.0f);
                g.setColour (IlanaTheme::Ui::track);
                g.fillRoundedRectangle (bar, 3.0f);
                const auto fill = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 48.0f) / 48.0f);
                g.setColour (fxColour (panel.type).withAlpha (0.85f));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fill), 3.0f);
            };
            const auto half = meters.getHeight() / 2;
            draw (meters.removeFromTop (half), "IN", in);
            draw (meters, "OUT", out);
        }

        auto caption = inner.removeFromTop (19).withTrimmedTop (3);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        const auto index = airwindowsIndexFor (panel.type);
        IlanaTheme::drawFitted (g, index >= 0 ? airwindowsDisplayName (airwindows::registry()[(size_t) index].name).toUpperCase() : juce::String ("WHAT IT DOES"),
                                caption, juce::Justification::centredLeft);
        g.setColour (off ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        IlanaTheme::drawFitted (g, cardInfoText (panel.type), inner.withTrimmedBottom (4), juce::Justification::topLeft, juce::jmax (2, inner.getHeight() / 13));
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

            // The number and name block is the row's handle: a drag reorders the chain.
            if (panel.handle.contains (position))
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
            overHeader = overHeader || panel.handle.contains (event.getPosition());

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
        if (! IlanaAnim::showing (*this))
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
        // The rail's gain readouts and the info wells' meters follow the audio.
        railTicks += ticks;
        if (railTicks > 4.0f)
        {
            railTicks = 0.0f;
            juce::String readings;
            for (const auto& panel : stackPanels)
                readings << railText (panel.slot) << (int) (railFill (panel.slot) * 40.0f) << ",";
            if (readings != lastReadings)
            {
                lastReadings = readings;
                stackContent.repaint (0, 0, railWidth, stackContent.getHeight());
                for (const auto& panel : stackPanels)
                    if (! panel.well.isEmpty())
                        stackContent.repaint (panel.well);
            }
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
    std::unique_ptr<OutputMeter> outputMeter;
    juce::Rectangle<int> outputStrip, emptyHeading, inColumn, outColumn, chainSeg, routingSeg, routingHint;
    juce::Rectangle<int> outputTitle, outputSoftLabel, outputSeparator, outputHint;
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;

    juce::Viewport stackView;
    FxStackContent stackContent;
    std::array<std::unique_ptr<SlotSwitch>, IlanaSynthAudioProcessor::numFxSlots> slotSwitches;
    std::unique_ptr<FxLibraryView> library;
    FxPickerLook pickerLook;
    FxPickerDimmer pickerDimmer;
    // (At most five split groups: each needs a plain card or the end after it.)
    std::array<std::unique_ptr<CrossoverStrip>, 5> crossoverStrips;

    // Per slot: the card header's type menu, blend, solo, band and (on a
    // duplicate) REMOVE, and the card's display.
    struct CardHeader
    {
        FxTypeButton type;
        juce::TextButton solo { "S" };
        juce::ComboBox band;
        BlendSlider blend;
        std::unique_ptr<KnobControl> mix;
        juce::TextButton remove { "REMOVE" };
        FxModelSwitch model;
        FxEngineChip chip;
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

            // The row's picture and tag dim with it.
            {
                auto& header = cardHeaders[(size_t) panel.slot];
                for (juce::Component* part : { (juce::Component*) displays[(size_t) panel.slot].get(), (juce::Component*) &eqCurve,
                                               (juce::Component*) gateGrid.get(), (juce::Component*) &tapGrid, (juce::Component*) &header.chip })
                {
                    const auto isOwn = (part == &eqCurve && panel.type != 29) || (part == gateGrid.get() && panel.type != 16)
                                       || (part == &tapGrid && panel.type != 9);
                    if (! isOwn && part->getAlpha() != alpha)
                    {
                        part->setAlpha (alpha);
                        changed = true;
                    }
                }
            }

            ToggleControl* power = nullptr;
            rowItems (panel.type, &power);

            for (auto* item : slotGroups[(size_t) panel.type])
            {
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                const auto isPower = toggle != nullptr && toggle == power;
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
    float cpuTicks = 0.0f, railTicks = 0.0f;
    juce::String lastReadings;
    std::array<int, IlanaSynthAudioProcessor::numFxSlots> lastTypes {};
    juce::String lastSignature;
    // The toolbar: CHAIN 1 / 2, named apart from the header's A / B
    // compare; the dice as the header's, for the chain (V7-43).
    DiceFxButton diceButton;
    juce::TextButton fileButton { juce::String::fromUTF8 ("CHAIN \xe2\x96\xbe") };
    juce::TextButton loadIrButton { "LOAD IR" };
    juce::TextButton chainAButton { "CHAIN 1" };
    juce::TextButton chainBButton { "CHAIN 2" };
    juce::TextButton seriesButton { "SERIES" }, parallelButton { "PARALLEL" };
    std::unique_ptr<juce::ParameterAttachment> routingAttachment;
    bool parallelRouting = false;
    juce::TextButton copyChainButton { "COPY TO 2" };
    juce::TextButton addButton { "+ ADD" };
    DashedAddButton addTile { "+  ADD EFFECT", "+  ADD EFFECT" };
    std::vector<std::unique_ptr<FxSuggestTile>> suggestTiles;
    juce::Rectangle<int> suggestCaption;
    bool growRows = false;
    std::array<bool, 64> ownMix {};
    int stackNaturalHeight = 0, stackRows = 0, rowsPlaced = 0, availableStackHeight = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    ComboControl ampMode;
    KnobControl ampDrive, ampBass, ampMid, ampTreble, ampLevel;
    ToggleControl driveOn;
    KnobControl driveAmount, driveMix, foldAmount;
    ComboControl driveType, foldType;
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
    // ("Pressre", "Feedbk"), and say in the plugin's words what the card's
    // own modules call AMOUNT, LOW CUT or OUTPUT: the card shows the whole
    // word, in the rack's vocabulary (UI review 9, I9-15 / S9-5).
    static juce::String airwindowsKnobLabel (const char* name)
    {
        static const std::map<juce::String, juce::String> full {
            { "Compres", "Compress" }, { "Feedbk", "Feedback" }, { "Pressre", "Pressure" },
            { "Mewines", "Mewiness" }, { "filters Q", "Filter Q" }, { "HeadBmp", "Head Bump" }, { "Head B", "Head Bump" },
            { "Gv Wear", "Groove Wear" }, { "RmSize", "Size" }, { "MakeupGn", "Makeup Gain" }, { "Mid HiP", "Mid Low Cut" },
            { "SideHiP", "Side Low Cut" }, { "MonoBs", "Mono Bass" }, { "NonLin", "Nonlinear" }, { "FMDepth", "FM Depth" },
            { "FMSpeed", "FM Rate" }, { "BitShift", "Bit Shift" }, { "Gnd", "Ground" }, { "11K tap", "11K Tap" },
            { "15K tap", "15K Tap" }, { "22K tap", "22K Tap" },
            // The rack's words for the same controls.
            { "Density", "Amount" }, { "Highpass", "Low Cut" }, { "Highpas", "Low Cut" }, { "Lowpass", "High Cut" },
            { "Out Level", "Output" }, { "Output Level", "Output" }, { "Output Gain", "Output" }, { "Output Trim", "Output" },
            { "Input Gain", "Input" }, { "Input Trim", "Input" }, { "Regen", "Feedback" }, { "Speed", "Rate" },
            { "Reso", "Resonance" }, { "Rez", "Resonance" }, { "Damping", "Damping" } };
        const juce::String text (name);
        const auto found = full.find (text);
        return (found != full.end() ? found->second : text).toUpperCase();
    }

    // The knobs whose plugin value is a plain gain (the code multiplies the
    // signal by it, 0 to 1): these read in dB. The rest have no unit in the
    // plugin and read 0 to 100 % of their range.
    static bool isAirwindowsGain (const airwindows::Info& info, int k)
    {
        static const std::map<juce::String, juce::String> gains {
            { "Density", "Out Level" }, { "Drive", "Out Level" }, { "Spiral2", "Output" }, { "Air", "Output Level" },
            { "Holt2", "Output" }, { "DrumSlam", "Output" }, { "Density3", "Output" }, { "Desk4", "Output Trim" } };
        const auto found = gains.find (juce::String (info.name));
        return found != gains.end() && found->second == juce::String (info.knobs[k].name)
               && info.knobs[k].lo == 0.0f && info.knobs[k].hi == 1.0f;
    }

    // The knobs whose plugin value maps to a real unit in the algorithm's own
    // code (read from the ports: the same formulas, at the host's rate); the
    // text says it in ms, Hz or dB instead of a bare percentage (I14-2). Empty
    // for the rest.
    std::function<juce::String (double)> airwindowsUnitText (const airwindows::Info& info, int k) const
    {
        const auto sampleRate = processorRef.getSampleRate() > 1000.0 ? processorRef.getSampleRate() : 44100.0;
        const auto knob = info.knobs[k];
        const auto key = juce::String (info.name) + "|" + knob.name;
        const auto hz = [] (double f) { return f >= 1000.0 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        const auto db = [] (double d) { return (d >= 0.0 ? "+" : "") + juce::String (d, 1) + " dB"; };
        const auto ms = [] (double m) { return m >= 1000.0 ? juce::String (m / 1000.0, 2) + " s" : juce::String (juce::roundToInt (m)) + " ms"; };

        // The delays: the tape's speed sets the time (the ring is 88200 samples), the regen is squared.
        if (key == "TapeDelay2|Time" || key == "PitchDelay|Time")
        {
            const auto scale = key == "TapeDelay2|Time" ? 25.0 : 20.0;
            return [=] (double v) { return ms (88200.0 / (std::pow (knob.toPlugin ((float) v), 4.0) * scale + 1.0) / sampleRate * 1000.0); };
        }
        if (key == "TapeDelay2|Regen" || key == "PitchDelay|Regen")
            return [=] (double v) { return juce::String (juce::roundToInt (100.0 * std::pow (knob.toPlugin ((float) v), 2.0))) + " %"; };
        if (key == "TapeDelay2|Freq" || key == "PitchDelay|Freq")
            return [=] (double v) { return hz ((std::pow (knob.toPlugin ((float) v), 3.0) * 0.4 + 0.0001) * sampleRate); };
        if (key == "Baxandall2|Treble" || key == "Baxandall2|Bass")
            return [=] (double v) { return db (knob.toPlugin ((float) v) * 48.0 - 24.0); };
        if (key == "Logical4|MakeupGn")
            return [=] (double v) { return db (knob.toPlugin ((float) v) * 40.0 - 20.0); };
        if (key == "Dirt|Lowpass")
            return [=] (double v) { return hz (knob.toPlugin ((float) v) * 25000.0); };
        if (key == "Isolator2|Freq")
            return [=] (double v) { return hz (std::pow (knob.toPlugin ((float) v), 2.0 * std::sqrt (sampleRate / 44100.0)) * 0.4999 * sampleRate); };
        if (key == "Pressure5|Output")
            return [=] (double v) { const auto p = knob.toPlugin ((float) v); return p <= 0.0005f ? juce::String ("-inf dB") : db (40.0 * std::log10 (2.0 * p)); };
        return {};
    }

    // A gain knob's text in dB; the others keep the parameter's own %.
    void setAirwindowsUnit (KnobControl& knob, const airwindows::Info& info, int k)
    {
        auto& slider = knob.getSlider();
        if (awPercentText.find (&slider) == awPercentText.end())
            awPercentText[&slider] = slider.textFromValueFunction;

        if (isAirwindowsGain (info, k))
            slider.textFromValueFunction = [] (double value)
            {
                return value <= 0.0005 ? juce::String ("-inf dB")
                                       : juce::String (juce::Decibels::gainToDecibels (value), 1) + " dB";
            };
        else if (const auto unit = airwindowsUnitText (info, k))
            slider.textFromValueFunction = unit;
        else
            slider.textFromValueFunction = awPercentText[&slider];
        slider.updateText();
        knob.repaint();
    }

    std::map<juce::Slider*, std::function<juce::String (double)>> awPercentText;

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
    juce::String airwindowsKnobTip (const airwindows::Info& info, int k) const
    {
        return airwindowsKnobLabel (info.knobs[k].name) + " (Airwindows " + juce::String (info.name) + ": \""
               + juce::String (info.knobs[k].name) + "\")\n"
               + (isAirwindowsGain (info, k) ? "The plugin's output gain, in dB."
                  : airwindowsUnitText (info, k) ? "In the unit its algorithm works in (worked out from the plugin's code)."
                                                 : "The plugin's own control, shown as 0 to 100 % of its range: Airwindows gives it no unit.");
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
                setAirwindowsUnit (*knobs[k], info, k);
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
                setAirwindowsUnit (*controls.knobs[(size_t) k], info, k);
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
