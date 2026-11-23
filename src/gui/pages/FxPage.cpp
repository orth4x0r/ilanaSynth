#include "FxPage.h"

FxPage::FxPage (IlanaSynthAudioProcessor& p)
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
      compThreshold (p.apvts, "fx_comp_threshold", "THRESHOLD"),
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
      reverbType (p.apvts, "fx_reverb_type", "TYPE"),
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
      gateLength (p.apvts, "fx_gate_length", "LENGTH"),
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
      awAlgo (p.apvts, "fx_aw_algo", "TYPE"),
      awP1 (p.apvts, "fx_aw_p1", "1"), awP2 (p.apvts, "fx_aw_p2", "2"), awP3 (p.apvts, "fx_aw_p3", "3"),
      awP4 (p.apvts, "fx_aw_p4", "4"), awP5 (p.apvts, "fx_aw_p5", "5"),
      awMix (p.apvts, "fx_aw_mix", "MIX"),
      vocSource (p.apvts, "fx_voc_source", "MODULATOR"), vocBands (p.apvts, "fx_voc_bands", "BANDS"),
      vocWidth (p.apvts, "fx_voc_width", "WIDTH"), vocAttack (p.apvts, "fx_voc_attack", "ATTACK"),
      vocRelease (p.apvts, "fx_voc_release", "RELEASE"), vocFormant (p.apvts, "fx_voc_formant", "FORMANT"),
      vocUnvoiced (p.apvts, "fx_voc_unvoiced", "UNVOICED"), vocRate (p.apvts, "fx_voc_rate", "TALK RATE"),
      vocLevel (p.apvts, "fx_voc_level", "LEVEL"), vocMix (p.apvts, "fx_voc_mix", "MIX"),
      filterType (p.apvts, "fx_filter_type", "TYPE"), filterSlope (p.apvts, "fx_filter_slope", "SLOPE"),
      filterCutoff (p.apvts, "fx_filter_cutoff", "CUTOFF"), filterReso (p.apvts, "fx_filter_reso", "RESO"),
      filterDrive (p.apvts, "fx_filter_drive", "DRIVE"), filterMorph (p.apvts, "fx_filter_morph", "MORPH"),
      filterSpread (p.apvts, "fx_filter_spread", "SPREAD"),
      filterPicker (p, "fx_filter_type", fxColour (42), "Filter FX type"),
      filterCategories (p, "fx_filter_type", fxColour (42), "Filter FX type"),
      filterSlopePills (p.apvts, "fx_filter_slope", { "12 dB", "24 dB" }, fxColour (42),
                        "Slope: 12 dB per octave, or 24 dB for a steeper cut.")
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
            gateDiv, gatePattern, gateSteps, gateSwing, gateLength, gateSmooth, gateMix,
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
    slotGroups.push_back ({ &gateDiv, &gatePattern, &gateSteps, &gateSwing, &gateLength, &gateSmooth, &gateMix });
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
        controls.algo = std::make_unique<ComboControl> (p.apvts, prefix + "_algo", "TYPE");
        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
            controls.knobs[(size_t) k] = std::make_unique<KnobControl> (p.apvts, prefix + "_p" + juce::String (k + 1), juce::String (k + 1));
        controls.mix = std::make_unique<KnobControl> (p.apvts, prefix + "_mix", "MIX");
        // AW Delay: SYNC and DIV, as the main Delay's.
        if (juce::String (airwindows::categoryModules()[(size_t) c].id) == "awdelay")
        {
            controls.sync = std::make_unique<ToggleControl> (p.apvts, prefix + "_sync", "SYNC");
            controls.div = std::make_unique<ComboControl> (p.apvts, prefix + "_div", "DIV");
        }
        std::vector<juce::Component*> group { controls.algo.get() };
        if (controls.sync != nullptr)
            group.insert (group.end(), { controls.sync.get(), controls.div.get() });
        for (auto& knob : controls.knobs)
            group.push_back (knob.get());
        group.push_back (controls.mix.get());
        for (auto* item : group)
            addChildComponent (*item);
        slotGroups.push_back (group);
        controls.algo->setPopupOverride ([this, c] { showCategoryMenu (c); });
    }

    // FILTER (type 42): the voice's filter models on the rack.
    addAll (*this, filterType, filterSlope, filterCutoff, filterReso, filterDrive, filterMorph, filterSpread);
    slotGroups.push_back ({ &filterType, &filterSlope, &filterCutoff, &filterReso, &filterDrive, &filterSpread, &filterMorph });
    {
        // The tiles' short names, and the FILTER page's grouped list.
        auto& box = filterType.getComboBox();
        const auto selected = box.getSelectedId();
        const auto shortNames = FilterTypeGrid::shortNames();
        for (int index = 0; index < shortNames.size() && index < box.getNumItems(); ++index)
            box.changeItemText (index + 1, shortNames[index]);
        box.setSelectedId (selected, juce::dontSendNotification);
        filterType.setPopupOverride ([this] { showFilterTypeMenu(); });
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

    // The rack's controls in PLAY's packed style.
    for (auto& group : slotGroups)
        for (auto* item : group)
            packControl (item);

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
    updateKeepMark();
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
    patcherButton.setTooltip ("Patcher\nThe effects as boxes wired with cables (the PATCH tab): parallel branches, "
                              "loops and several of the same effect.");
    seriesButton.setDescription ("FX routing: series");
    parallelButton.setDescription ("FX routing: parallel");
    patcherButton.setDescription ("FX routing: patcher");
    for (auto* button : { &seriesButton, &parallelButton, &patcherButton })
    {
        IlanaTheme::makePill (*button, IlanaTheme::accent());
        button->setRadioGroupId (0x5e71a1);
        addChildComponent (*button); // (review 19: routing is chosen on PATCH; these only mirror fx_routing / fx_patcher)
    }
    routingOnPatchButton.setTooltip ("Routing\nHow the effects are wired (series, parallel, branches, loops) is drawn and edited as cables on the PATCH tab.");
    routingOnPatchButton.setDescription ("Open PATCH to change the FX routing");
    IlanaTheme::makePill (routingOnPatchButton, IlanaTheme::accent());
    routingOnPatchButton.onClick = [this] { if (onShowPatcher != nullptr) onShowPatcher(); };
    addAndMakeVisible (routingOnPatchButton);
    seriesButton.onClick = [this] { setRouting (false); };
    parallelButton.onClick = [this] { setRouting (true); };
    patcherButton.onClick = [this] { setPatcher(); };
    if (auto* routing = p.apvts.getParameter ("fx_routing"))
    {
        routingAttachment = std::make_unique<juce::ParameterAttachment> (
            *routing, [this] (float value) { routingChanged (value > 0.5f); }, nullptr);
        routingAttachment->sendInitialUpdate();
    }
    if (auto* patcher = p.apvts.getParameter ("fx_patcher"))
    {
        patcherAttachment = std::make_unique<juce::ParameterAttachment> (
            *patcher, [this] (float value)
            {
                patcherOn = value > 0.5f;
                routingChanged (parallelRouting, true);
            }, nullptr);
        patcherAttachment->sendInitialUpdate();
    }
    addChildComponent (copyChainButton); // (its action is in the CHAIN menu)
    tapGrid.setVisible (false);
    tapGrid.setName ("CUSTOM TAP GRID"); // (the UI test finds it by name)

    // The final stage after the rack.
    softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "ON"); // its name is drawn at its left (review 11, S11-14)
    softClip->showAsSwitch();
    softClip->setBareSwitch (true);
    clipGain = std::make_unique<StripKnob> (p, "master_clip_gain", "Clip Gain");
    // (In the top bar since UI review 31; CLIP GAIN's tooltip says where it acts.)
    clipGain->setTooltip ("CLIP GAIN\nThe level into the soft clipper, after the rack and before the master volume.");
    addAndMakeVisible (*softClip);
    addAndMakeVisible (*clipGain);
    outputMeter = std::make_unique<OutputMeter> (p);
    addAndMakeVisible (*outputMeter);

    // Every loaded module's controls live in one scrolling stack.
    for (auto& group : slotGroups)
        for (auto* control : group)
            stackContent.addChildComponent (control);

    gateGrid = std::make_unique<GateGrid> (p);

    // The classic chops (16ths ... Stutter) only chop with LENGTH under
    // 100 %: picking one while LENGTH is full opens each step half way.
    gatePattern.setOnPick ([&p] (int index)
    {
        auto* length = p.apvts.getParameter ("fx_gate_length");

        if (index < 9 || length == nullptr || length->convertFrom0to1 (length->getValue()) < 0.999f)
            return;

        p.performEdit ("Gate length", [length]
        {
            length->beginChangeGesture();
            length->setValueNotifyingHost (length->convertTo0to1 (0.5f));
            length->endChangeGesture();
        });
    });
    stackContent.addChildComponent (*gateGrid);
    stackContent.addChildComponent (tapGrid);
    stackContent.addChildComponent (eqCurve);
    stackContent.addChildComponent (loadIrButton);
    stackContent.addChildComponent (filterPicker);
    stackContent.addChildComponent (filterCategories);
    stackContent.addChildComponent (filterSlopePills);
    filterSlopePills.setStacked ("SLOPE");
    filterSlopePills.getProperties().set ("packedCell", true); // (placed as a packed knob: the UI test judges its cell)
    stackContent.painter = [this] (juce::Graphics& g) { paintStack (g); };
    stackContent.overPainter = [this] (juce::Graphics& g) { paintDragGhost (g); };
    stackContent.onDown = [this] (const juce::MouseEvent& event) { stackMouseDown (event); };
    stackContent.onDrag = [this] (const juce::MouseEvent& event) { stackMouseDrag (event); };
    stackContent.onUp = [this] (const juce::MouseEvent& event) { stackMouseUp (event); };
    stackContent.onMove = [this] (const juce::MouseEvent& event) { stackMouseMove (event); };
    stackContent.onKey = [this] (const juce::KeyPress& key) { return stackKeyPressed (key); };

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

        header.band.addItemList ({ "Full", "Low", "Mid", "High", "M/S Mid", "M/S Side" }, 1);
        header.band.setTooltip ("Band: the part of the signal this slot works on (the full signal, the LOW / MID / HIGH band, "
                                "or the Mid or Side of the stereo image); the rest passes around it. Banded slots next to "
                                "each other form a split group, with its crossovers above them.");
        if (auto* band = p.apvts.getParameter (prefix + "_band"))
            header.bandAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*band, header.band, nullptr);
        // (A packed menu, as the card's others: its name inside the box.)
        header.band.getProperties().set ("packedMenu", true);
        header.band.getProperties().set ("textPrefix", "BAND");
        stackContent.addChildComponent (header.band);

        if (auto* blend = p.apvts.getParameter (prefix + "_mix"))
            header.blendAttachment = std::make_unique<juce::SliderParameterAttachment> (*blend, header.blend, nullptr);
        stackContent.addChildComponent (header.blend);
        // The slot's dry / wet as a MIX knob in the card's row, where the
        // effect has no MIX of its own: one widget, one name and one
        // place for dry / wet on every card (UI review 8, S8-6).
        header.mix = std::make_unique<KnobControl> (p.apvts, prefix + "_mix", "MIX", IlanaTheme::accent(), false);
        packControl (header.mix.get());
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

        header.remove.setTooltip ("Take this instance out of the rack (the others keep their settings)");
        header.remove.onClick = [this, slot] { removeSlot (slot); };
        stackContent.addChildComponent (header.remove);
        header.edit.onClick = [this, slot] { editInstance (slot); };
        IlanaTheme::makePill (header.edit, IlanaTheme::accent());
        stackContent.addChildComponent (header.edit);

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

    // The UI test's way into a card's right-click menu (the menu itself
    // is modal): "fxCardMenu" (slot) lists "id|text|enabled" per item,
    // "fxCardAction" (slot, id) does what picking the item does.
    getProperties().set ("fxCardMenu", juce::var (juce::var::NativeFunction ([this] (const juce::var::NativeFunctionArgs& args) -> juce::var
    {
        juce::StringArray items;
        if (args.numArguments < 1)
            return items;
        const auto menu = buildTypeMenu ((int) args.arguments[0]);
        for (juce::PopupMenu::MenuItemIterator item (menu, true); item.next();)
            if (item.getItem().itemID != 0)
                items.add (juce::String (item.getItem().itemID) + "|" + item.getItem().text + "|" + (item.getItem().isEnabled ? "1" : "0"));
        return items;
    })));
    getProperties().set ("fxCardAction", juce::var (juce::var::NativeFunction ([this] (const juce::var::NativeFunctionArgs& args) -> juce::var
    {
        if (args.numArguments >= 2)
            typeMenuResult ((int) args.arguments[0], (int) args.arguments[1]);
        return {};
    })));

    updateVisibility();
    startTimerHz (30);
}

void FxPage::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());

    // The top bar: the two chains as one switch, the routing (what it
    // means is its tooltip), and the OUTPUT stage.
    IlanaTheme::paintWell (g, chainSeg.toFloat(), (float) chainSeg.getHeight() * 0.5f);
    IlanaTheme::paintWell (g, routingSeg.toFloat(), (float) routingSeg.getHeight() * 0.5f);

    // The rail's ends: IN above the first effect, OUT under the last. In
    // PARALLEL two buses leave IN and reach OUT: the input's on the left,
    // which every effect taps, and the sum's on the right.
    const auto railX = (float) inColumn.getCentreX();
    if (! stackView.isVisible())
    {
        paintRailPill (g, inColumn, "IN");
        return paintOutput (g);
    }

    if (patcherOn)
    {
        const auto faint = IlanaTheme::Ui::text3.withAlpha (0.5f);
        paintRailLine (g, railX, (float) inColumn.getCentreY() + 11.0f, (float) stackView.getY() + 1.0f, faint, true);
        paintRailLine (g, railX, (float) stackView.getBottom() - 1.0f, (float) outColumn.getCentreY() - 11.0f, faint, true);
    }
    else if (parallelRouting)
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
        // (Two short lines beside the chips: "SUGGESTED" over "NEXT".)
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
        auto caption = suggestCaption.withSizeKeepingCentre (suggestCaption.getWidth(), 30);
        g.drawText ("SUGGESTED", caption.removeFromTop (15), juce::Justification::centredLeft, false); // off-grid
        g.drawText ("NEXT", caption, juce::Justification::centredLeft, false);
    }
}

void FxPage::paintOutput (juce::Graphics& g)
{
    if (! stackView.isVisible())
        paintRailPill (g, outColumn, "OUT");

    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    g.drawText ("SOFT CLIP", outputSoftLabel, juce::Justification::centredRight);
    g.setColour (IlanaTheme::Ui::line);
    g.fillRect (outputSeparator);

    if (library->isVisible())
    {
        // One hint line (the heading and its sentence were a 60 px band above the tiles).
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText ("The rack is empty. Pick an effect to start the chain; drag a row's name to reorder it.",
                    emptyHeading, juce::Justification::centredLeft, true);
    }
}

void FxPage::paintOverChildren (juce::Graphics& g)
{
    if (! stackView.isVisible())
        return;

    const auto background = IlanaTheme::Ui::bg;

    if (stackView.getViewPositionY() + stackView.getHeight() < stackContent.getHeight() - 2)
    {
        const auto foot = stackView.getBounds().removeFromBottom (28).toFloat(); // off-grid
        g.setGradientFill (juce::ColourGradient (background.withAlpha (0.0f), 0.0f, foot.getY(), background, 0.0f, foot.getBottom(), false));
        g.fillRect (foot);
    }

    if (stackView.getViewPositionY() > 2)
    {
        // The card cut off under the toolbar is hidden almost wholly, then
        // fades in: its half-drawn controls no longer show through (N16-5).
        const auto head = stackView.getBounds().removeFromTop (34).toFloat(); // off-grid
        auto fade = juce::ColourGradient (background, 0.0f, head.getY(), background.withAlpha (0.0f), 0.0f, head.getBottom(), false);
        fade.addColour (0.55, background.withAlpha (0.92f));
        g.setGradientFill (fade);
        g.fillRect (head);
    }
}

void FxPage::resized ()
{
    // A 12 px gutter at the right, as the other pages keep (N16-4).
    auto area = getLocalBounds().reduced (0, Space::s8).withTrimmedRight (Space::s12);

    // The top bar: IN above the rail, the two chains, the routing; then
    // the OUTPUT stage after the rack (SOFT CLIP, CLIP GAIN and the meter,
    // UI review 31: they had a strip of their own at the foot); at the
    // right the chain's file menu and the dice.
    auto toolbar = area.removeFromTop (toolbarHeight);
    const auto item = [] (juce::Rectangle<int> r) { return r.withSizeKeepingCentre (r.getWidth(), toolbarItemHeight); };
    inColumn = toolbar.removeFromLeft (railWidth);
    toolbar.removeFromLeft (railGap);
    chainSeg = toolbar.removeFromLeft (136).withSizeKeepingCentre (136, toolbarItemHeight + 4); // off-grid
    chainAButton.setBounds (chainSeg.withTrimmedLeft (3).removeFromLeft (64).reduced (0, Space::s2)); // off-grid
    chainBButton.setBounds (chainSeg.withTrimmedLeft (3 + 66).removeFromLeft (64).reduced (0, Space::s2)); // off-grid
    toolbar.removeFromLeft (10); // off-grid
    routingSeg = toolbar.removeFromLeft (172).withSizeKeepingCentre (172, toolbarItemHeight + 4); // off-grid
    routingOnPatchButton.setBounds (routingSeg.reduced (3, Space::s2)); // off-grid
    toolbar.removeFromLeft (10); // off-grid
    diceButton.setBounds (item (toolbar.removeFromRight (112))); // off-grid
    toolbar.removeFromRight (Space::s6);
    fileButton.setBounds (item (toolbar.removeFromRight (84))); // off-grid
    toolbar.removeFromRight (Space::s16);
    addButton.setVisible (false); // (+ ADD EFFECT is the add row at the foot)
    {
        // OUTPUT, right-aligned against the chain's menu: the meter, CLIP
        // GAIN, SOFT CLIP and its name, a rule before them.
        outputMeter->setBounds (toolbar.removeFromRight (110).withSizeKeepingCentre (110, 28)); // off-grid
        toolbar.removeFromRight (14); // off-grid
        clipGain->setBounds (toolbar.removeFromRight (136)); // off-grid
        toolbar.removeFromRight (14); // off-grid
        softClip->setBounds (toolbar.removeFromRight (40).withSizeKeepingCentre (40, 22)); // off-grid
        toolbar.removeFromRight (Space::s6);
        outputSoftLabel = toolbar.removeFromRight (70); // off-grid
        toolbar.removeFromRight (14); // off-grid
        outputSeparator = toolbar.removeFromRight (1).withSizeKeepingCentre (1, 26); // off-grid
    }
    copyChainButton.setVisible (false);
    area.removeFromTop (rowGap - stackTopMargin);

    // The foot: OUT at the rail's end, and beside it the add row. (The
    // stack's foot margin is wider than the gap: a rack that fits reaches
    // 4 px into the add row's band, so the row sits one gap under its last
    // card; one that scrolls ends a gap above it.)
    auto outRow = area.removeFromBottom (addTileHeight);
    const auto stackArea = area.withBottom (outRow.getY() + stackFootMargin - rowGap);
    const auto scrollArea = area.withBottom (outRow.getY() - rowGap);
    outColumn = outRow.withWidth (railWidth);

    // An empty rack shows the library in the stack's place (one view
    // of the chain, not a list beside the cards: S5-18, S6-25).
    const auto empty = stackPanelsWouldBeEmpty();
    library->setVisible (empty);
    stackView.setVisible (! empty);

    if (empty)
    {
        auto box = stackArea.reduced (Space::s12, 0);
        emptyHeading = box.removeFromTop (Space::s24);
        library->setBounds (box.withHeight (juce::jmin (box.getHeight(), FxLibraryView::preferredHeight() + 170)));
    }

    // The rows take their own height (one size whatever the rack holds); a long rack scrolls.
    availableStackHeight = stackArea.getHeight();
    stackView.setBounds (stackArea);
    layoutStack();
    if (! empty)
    {
        // (Two pixels over still count as fitting: the view reaches them into the add row's band, so the last level
        // tag is whole at rest instead of half under the foot's fade: polish pass 1.)
        const auto fits = stackNaturalHeight <= stackArea.getHeight() + 2;
        availableStackHeight = fits ? stackArea.getHeight() : scrollArea.getHeight();
        stackView.setBounds (fits ? stackArea.withHeight (stackNaturalHeight) : scrollArea);
        layoutStack();
        // Every card keeps one size whatever the rack holds (ilana 2026-11-17: a lone card grown
        // to the page looked odd), so a short rack ends above the foot: OUT and the add row
        // follow its last card, one gap under it, as they do under a full rack.
        if (fits)
        {
            outRow.setY (juce::jmin (outRow.getY(), stackView.getBottom() - stackFootMargin + rowGap));
            outColumn = outRow.withWidth (railWidth);
        }
    }

    // The add row: + ADD EFFECT, then the suggested next effects as chips
    // (the ones not in the rack, as many as the row holds; whole chips only).
    for (auto& suggest : suggestTiles)
        suggest->setVisible (false);
    suggestCaption = {};
    addTile.setVisible (! empty);
    if (! empty)
    {
        auto row = outRow.withTrimmedLeft (railWidth + railGap);
        const auto free = firstEmptySlot() >= 0;
        std::vector<int> offered;
        if (free)
            for (const auto type : FxSuggestTile::candidates())
                if (slotHoldingType (type) < 0 && (fxTwinOf (type) < 0 || slotHoldingType (fxTwinOf (type)) < 0))
                    offered.push_back (type);
        const auto fits = juce::jmax (0, (row.getWidth() - addTileMinWidth - suggestCaptionWidth + suggestChipGap) / (suggestChipWidth + suggestChipGap));
        const auto count = juce::jmin ((int) offered.size(), fits);
        if (count > 0)
        {
            auto chips = row.removeFromRight (suggestCaptionWidth + count * (suggestChipWidth + suggestChipGap));
            suggestCaption = chips.removeFromLeft (suggestCaptionWidth).withTrimmedLeft (suggestChipGap + 8);
            for (int i = 0; i < count; ++i)
            {
                chips.removeFromLeft (suggestChipGap);
                const auto bounds = chips.removeFromLeft (suggestChipWidth);
                for (auto& suggest : suggestTiles)
                    if (suggest->getType() == offered[(size_t) i])
                    {
                        suggest->setBounds (bounds);
                        suggest->setVisible (true);
                    }
            }
        }
        addTile.setBounds (row);
    }
    updateAddButton();
    repaint();
}

void FxPage::updateAddButton ()
{
    const auto free = firstEmptySlot() >= 0;
    addButton.setEnabled (free);
    addTile.setEnabled (free);
    addTile.setQuiet (! free);
    addTile.setLabel (free ? "+ ADD EFFECT" : "EVERY SLOT IS IN USE");
    addTile.setTooltip (free ? "Add an effect: the library, every effect grouped by what it does. It goes in after the selected card."
                             : "Every slot is in use: remove an effect to add another.");
    addButton.setTooltip (addTile.getTooltip());
}

void FxPage::editInstance (int slot)
{
    const auto own = ! processorRef.isFxSlotInFront (slot);
    // (An instance's card has its own controls; an effect's only copy is its card.)
    if (own || (! processorRef.isFxInstance (slot) && slotHoldingType (getSlotType (slot), slot) < 0))
    {
        showSlot (slot);
        return;
    }
    processorRef.performEdit ((own ? "Edit " : "Make own ") + getSlotName (getSlotType (slot)), [this, slot, own]
    {
        if (own)
            processorRef.focusFxInstance (slot);
        else
            processorRef.makeFxInstance (slot);
    });
    showSlot (slot);
}

void FxPage::showSlot (int slot)
{
    updateVisibility();
    select (slot);
    scrollToSlot (slot);
    dropSlot = slot;
    dropFlash = 1.0f;
}

juce::String FxPage::windowMixName (int slot, int type) const
{
    StackPanel panel;
    panel.slot = slot;
    panel.type = type;
    return showsSlotMix (panel) ? juce::String (hasOwnMix (type) ? "BLEND" : "MIX") : juce::String();
}

bool FxPage::isIdleControl (int type, const juce::String& parameterId, const juce::AudioProcessorValueTreeState& state)
{
    const auto on = [&state] (const char* id)
    {
        const auto* value = state.getRawParameterValue (id);
        return value != nullptr && value->load() > 0.5f;
    };

    // The delay's times do nothing while SYNC is on, its DIV nothing
    // while it is off.
    if (type == 9)
    {
        if (parameterId == "fx_delay_time" || parameterId == "fx_delay_time_r")
            return on ("fx_delay_sync");
        if (parameterId == "fx_delay_div")
            return ! on ("fx_delay_sync");
    }

    // So do AW Delay's time knobs and DIV (its card's own SYNC).
    if (const auto c = airwindows::categoryForFxType (type); c >= 0 && parameterId.startsWith ("fx_awdelay_"))
    {
        if (parameterId == "fx_awdelay_div")
            return ! on ("fx_awdelay_sync");
        const auto& info = airwindows::categoryModules()[(size_t) c];
        const auto* choice = state.getRawParameterValue (juce::String ("fx_") + info.id + "_algo");
        const auto algorithm = info.algorithms[(size_t) juce::jlimit (0, (int) info.algorithms.size() - 1, choice != nullptr ? (int) choice->load() : 0)];
        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
            if (parameterId == "fx_awdelay_p" + juce::String (k + 1))
                return airwindows::delaysync::isTimeKnob (algorithm, k) && on ("fx_awdelay_sync");
    }
    return false;
}

juce::String FxPage::parameterIdOf (const juce::Component* item)
{
    if (const auto* knob = dynamic_cast<const KnobControl*> (item))
        return knob->getParameterId();
    if (const auto* toggle = dynamic_cast<const ToggleControl*> (item))
        return toggle->getParameterId();
    if (const auto* combo = dynamic_cast<const ComboControl*> (item))
        return combo->getParameterId();
    return {};
}

const char* FxPage::enableParamFor (int type)
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

int FxPage::getSlotType (int slot) const
{
    if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1)))
        return juce::jlimit (0, juce::jmax (0, slotNames.size() - 1), (int) value->load());

    return 0;
}

int FxPage::getSlotBand (int slot) const
{
    if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_band"))
        return (int) value->load();
    return 0;
}

juce::String FxPage::getSlotName (int type) const
{
    const auto name = fxTypeName (type);
    return name != "-" || ! juce::isPositiveAndBelow (type, slotNames.size()) ? name : slotNames[type];
}

int FxPage::slotHoldingType (int type, int except) const
{
    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        if (slot != except && getSlotType (slot) == type)
            return slot;
    return -1;
}

bool FxPage::stackPanelsWouldBeEmpty () const
{
    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        if (getSlotType (slot) > 0 && getSlotType (slot) < (int) slotGroups.size())
            return false;
    return true;
}

int FxPage::neighbourSlot (int slot, int direction) const
{
    for (auto other = slot + direction; juce::isPositiveAndBelow (other, IlanaSynthAudioProcessor::numFxSlots); other += direction)
        if (getSlotType (other) != 0)
            return other;
    return -1;
}

void FxPage::select (int slot)
{
    cardChosen = true;
    if (slot != selectedSlot)
    {
        selectedSlot = slot;
        stackContent.repaint();
    }
}

int FxPage::placeAfter (int added, int after)
{
    if (added < 0 || after < 0 || added == after)
        return added;
    const auto to = added > after ? after + 1 : after;
    shiftSlot (added, to);
    return to;
}

void FxPage::pickFromLibrary (int type, int replacingSlot)
{
    if (replacingSlot >= 0 && getSlotType (replacingSlot) == type)
        return;

    // One the rack holds already: another instance of it, with its own
    // engine and a copy of the settings shown now (roadmap step 17).
    auto slot = replacingSlot >= 0 ? replacingSlot : firstEmptySlot();
    if (slot < 0)
        return;

    const auto after = insertAfterSlot();
    processorRef.performEdit ((replacingSlot >= 0 ? "Change effect to " : "Add ") + getSlotName (type), [this, &slot, type, replacingSlot, after]
    {
        if (replacingSlot < 0)
        {
            slot = placeAfter (processorRef.addFxEffect (type), after);
            return;
        }
        const auto held = slotHoldingType (type, replacingSlot) >= 0;
        processorRef.assignFxSlot (replacingSlot + 1, type);
        if (held)
            processorRef.makeFxInstance (replacingSlot);
    });
    if (slot < 0)
        return;
    selectedSlot = slot;
    cardChosen = true;
    updateVisibility();
    scrollToSlot (slot);
    dropSlot = slot;
    dropFlash = 1.0f;

    // A new card slides down and fades in (review 26 flair).
    if (replacingSlot < 0)
        for (const auto& panel : stackPanels)
            if (panel.slot == slot && ! panel.bounds.isEmpty())
            {
                IlanaAnim::CardMotion::reveal (stackContent, panel.bounds, [] (juce::Graphics& g) { g.fillAll (IlanaTheme::Ui::bg); });
                break;
            }
}

void FxPage::removeSlot (int slot)
{
    // The card fades out where it stood (a ghost of it, while the cards
    // below close up at once); N16 leftover.
    for (const auto& panel : stackPanels)
        if (panel.slot == slot && ! panel.bounds.isEmpty() && IlanaAnim::showing (stackContent))
        {
            removeImage = stackContent.createComponentSnapshot (panel.bounds);
            removeBounds = panel.bounds;
            removeFade = 1.0f;
            break;
        }

    processorRef.performEdit ("Remove " + getSlotName (getSlotType (slot)), [this, slot] { processorRef.removeFxEffect (slot); });
    updateVisibility();
}

juce::Component* FxPage::calloutHost ()
{
    auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (editor == nullptr)
        return getTopLevelComponent();
    juce::Component* host = this;
    while (host->getParentComponent() != nullptr && host->getParentComponent() != editor)
        host = host->getParentComponent();
    return host->getParentComponent() == editor ? host : editor;
}

void FxPage::showLibrary (juce::Component& target, juce::Rectangle<int> area, int slot)
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
    openLibraryBox = &box; // (~FxPage takes pickerLook off it if the page goes first)
    box.setArrowSize (12.0f);
    box.updatePosition (pointAt, host->getLocalBounds()); // (with the picker's border)
}

namespace
{
// Copy chain / Paste chain: the same data Save chain writes, held in memory
// (shared by every editor of the process, so it carries to another patch).
juce::String& fxChainClipboard()
{
    static juce::String text;
    return text;
}
} // namespace

// The CHAIN button wears a dot while KEEP FX is on (loading a preset leaves the rack alone).
void FxPage::updateKeepMark ()
{
    fileButton.setButtonText (juce::String::fromUTF8 (keepRackOnPresetLoad() ? "CHAIN \xe2\x80\xa2 \xe2\x96\xbe" : "CHAIN \xe2\x96\xbe"));
    fileButton.setTooltip (keepRackOnPresetLoad() ? "This chain as a whole. KEEP FX is on: loading a preset leaves this rack as it is."
                                                  : "This chain as a whole: copy it, save it to a file, or load one into it");
}

void FxPage::showFileMenu ()
{
    const auto directory = processorRef.getUserPresetDirectory();
    const auto saved = directory.findChildFiles (juce::File::findFiles, false, "*.ilanafxchain");
    juce::Array<juce::File> files (saved);
    files.sort();

    juce::PopupMenu menu;
    menu.addItem (3, processorRef.isShowingChainA() ? "Copy to chain 2..." : "Copy to chain 1...");
    menu.addSeparator();
    menu.addItem (4, "Copy chain");
    menu.addItem (5, "Paste chain", fxChainClipboard().isNotEmpty());
    menu.addSeparator();
    menu.addItem (1, "Save chain...");
    menu.addItem (2, "Load chain...");
    menu.addSeparator();
    menu.addItem (6, "Keep this FX rack when loading presets", true, keepRackOnPresetLoad());

    if (! files.isEmpty())
    {
        menu.addSeparator();
        for (int i = 0; i < files.size() && i < 64; ++i)
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension());
    }

    juce::Component::SafePointer<FxPage> safeThis (this);
    menu.showMenuAsync (IlanaTheme::menuOptions (&fileButton),
                        [safeThis, files] (int result)
                        {
                            if (safeThis == nullptr)
                                return;
                            if (result == 1)
                                safeThis->saveChain();
                            else if (result == 2)
                                safeThis->loadChain();
                            else if (result == 3 && safeThis->copyChainButton.onClick != nullptr)
                                safeThis->copyChainButton.onClick();
                            else if (result == 6)
                            {
                                keepRackOnPresetLoad() = ! keepRackOnPresetLoad();
                                safeThis->updateKeepMark();
                            }
                            else if (result == 4)
                            {
                                if (const auto xml = safeThis->processorRef.makeFxChainXml())
                                    fxChainClipboard() = xml->toString (juce::XmlElement::TextFormat().singleLine());
                            }
                            else if (result == 5)
                            {
                                if (const auto xml = juce::XmlDocument::parse (fxChainClipboard()))
                                {
                                    safeThis->processorRef.getUndoManager().beginNewTransaction ("Paste FX chain");
                                    safeThis->processorRef.applyFxChainXml (*xml);
                                    safeThis->updateVisibility();
                                    safeThis->repaint();
                                }
                            }
                            else if (result >= 100 && result - 100 < files.size())
                            {
                                safeThis->processorRef.getUndoManager().beginNewTransaction ("Load FX chain");
                                safeThis->processorRef.loadFxChainFromFile (files[result - 100]);
                                safeThis->updateVisibility();
                                safeThis->repaint();
                            }
                        });
}

void FxPage::showTypeMenu (int slot, juce::Component* target)
{
    juce::Component::SafePointer<FxPage> safeThis (this);
    buildTypeMenu (slot).showMenuAsync (IlanaTheme::menuOptions (target != nullptr ? target : this),
                                        [safeThis, slot] (int result)
                                        {
                                            if (safeThis != nullptr && result > 0)
                                                safeThis->typeMenuResult (slot, result);
                                        });
}

juce::PopupMenu FxPage::buildTypeMenu (int slot)
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

    // One effect's own actions (UI review 31): a copy with its own
    // settings after it, its settings to and from the clipboard (onto
    // the same effect), and its defaults.
    menu.addSeparator();
    const auto loaded = getSlotType (slot) != 0;
    menu.addItem (1301, "Duplicate", loaded && firstEmptySlot() >= 0);
    menu.addItem (1302, "Copy settings", loaded);
    menu.addItem (1303, settingsClip.type > 0 && settingsClip.type != getSlotType (slot)
                            ? "Paste settings (copied from " + getSlotName (settingsClip.type) + ")"
                            : juce::String ("Paste settings"),
                  loaded && settingsClip.type == getSlotType (slot));
    menu.addItem (1304, "Reset to default", loaded);
    menu.addItem (1003, "Remove from the rack", loaded);
    return menu;
}

void FxPage::typeMenuResult (int slot, int result)
{
    auto& page = *this;
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
    else if (result == 1301)
        page.duplicateSlot (slot);
    else if (result == 1302)
        page.copySettings (slot);
    else if (result == 1303)
        page.pasteSettings (slot);
    else if (result == 1304)
        page.resetSettings (slot);

    page.updateVisibility();
    page.repaint();
}

void FxPage::moveSlot (int from, int to)
{
    if (from == to || ! juce::isPositiveAndBelow (from, IlanaSynthAudioProcessor::numFxSlots)
        || ! juce::isPositiveAndBelow (to, IlanaSynthAudioProcessor::numFxSlots))
        return;

    processorRef.performEdit ("Move " + getSlotName (getSlotType (from)), [this, from, to] { shiftSlot (from, to); });

    selectedSlot = to;
    dropSlot = to;
    dropFlash = 1.0f;
    updateVisibility();
    repaint();
}

void FxPage::shiftSlot (int from, int to)
{
    const auto step = to > from ? 1 : -1;
    for (auto slot = from; slot != to; slot += step)
        swapSlots (slot, slot + step);
}

void FxPage::duplicateSlot (int slot)
{
    const auto type = getSlotType (slot);
    if (type == 0 || firstEmptySlot() < 0)
        return;
    auto added = -1;
    processorRef.performEdit ("Duplicate " + getSlotName (type), [this, slot, type, &added]
    {
        if (! processorRef.isFxSlotInFront (slot))
            processorRef.focusFxInstance (slot);
        added = placeAfter (processorRef.addFxEffect (type), slot);
    });
    if (added < 0)
        return;
    selectedSlot = added;
    cardChosen = true;
    showSlot (added);
}

std::vector<std::pair<juce::String, float>> FxPage::settingsOf (int slot) const
{
    const auto type = getSlotType (slot);
    if (! processorRef.isFxSlotInFront (slot))
        return processorRef.getFxPatch().slots[(size_t) slot].values;
    std::vector<std::pair<juce::String, float>> values;
    for (auto* parameter : processorRef.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter); ranged != nullptr && FxPatchData::isParamOf (type, ranged->paramID))
            values.emplace_back (ranged->paramID, ranged->convertFrom0to1 (ranged->getValue()));
    return values;
}

void FxPage::applySettings (int slot, const juce::String& name, const std::vector<std::pair<juce::String, float>>& values)
{
    const auto front = processorRef.isFxSlotInFront (slot);
    processorRef.performEdit (name, [this, slot, front, &values]
    {
        for (const auto& [id, value] : values)
        {
            if (! front)
            {
                processorRef.setFxInstanceValue (slot, id, value);
                continue;
            }
            if (auto* parameter = processorRef.apvts.getParameter (id))
                if (const auto normalised = parameter->convertTo0to1 (value); parameter->getValue() != normalised)
                    parameter->setValueNotifyingHost (normalised);
        }
    });
    updateVisibility();
    stackContent.repaint();
}

void FxPage::copySettings (int slot)
{
    settingsClip.type = getSlotType (slot);
    settingsClip.values = settingsOf (slot);
}

void FxPage::pasteSettings (int slot)
{
    if (settingsClip.type > 0 && settingsClip.type == getSlotType (slot))
        applySettings (slot, "Paste " + getSlotName (settingsClip.type) + " settings", settingsClip.values);
}

void FxPage::resetSettings (int slot)
{
    const auto type = getSlotType (slot);
    if (type == 0)
        return;
    auto values = settingsOf (slot);
    for (auto& [id, value] : values)
        if (auto* parameter = processorRef.apvts.getParameter (id))
            value = parameter->convertFrom0to1 (parameter->getDefaultValue());
    applySettings (slot, "Reset " + getSlotName (type), values);
}

void FxPage::saveChain ()
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

void FxPage::loadChain ()
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

void FxPage::setRouting (bool parallel)
{
    const auto patcher = processorRef.isFxPatcherOn();
    if (routingAttachment == nullptr || (parallel == parallelRouting && ! patcher))
        return routingChanged (parallelRouting);

    processorRef.performEdit (parallel ? "FX routing: parallel" : "FX routing: series", [this, parallel, patcher]
    {
        if (patcher)
            processorRef.setFxPatcherOn (false);
        if (parallel != parallelRouting)
            routingAttachment->setValueAsCompleteGesture (parallel ? 1.0f : 0.0f);
    });
}

void FxPage::setPatcher ()
{
    if (processorRef.isFxPatcherOn())
        routingChanged (parallelRouting, true);
    else
        processorRef.performEdit ("FX routing: patcher", [this] { processorRef.setFxPatcherOn (true); });
    if (onShowPatcher != nullptr)
        onShowPatcher();
}

void FxPage::routingChanged (bool parallel, bool force)
{
    const auto patcher = patcherOn;
    seriesButton.setToggleState (! parallel && ! patcher, juce::dontSendNotification);
    parallelButton.setToggleState (parallel && ! patcher, juce::dontSendNotification);
    patcherButton.setToggleState (patcher, juce::dontSendNotification);
    if (parallel == parallelRouting && ! force)
        return;
    parallelRouting = parallel;
    // (What the routing means, which was a sentence in the top bar: UI review 31.)
    routingOnPatchButton.setTooltip ("Routing\n" + routingHintText()
                                     + "\nHow the effects are wired (series, parallel, branches, loops) is drawn and edited as cables on the PATCH tab.");
    repaint();
    stackContent.repaint();
}

juce::String FxPage::routingHintText () const
{
    if (patcherOn)
        return "The cables on PATCH decide what feeds what.";
    return parallelRouting ? "Parallel: every effect hears the input, and the outputs are summed." : "Effects run top to bottom. ROUTING ON PATCH rewires them.";
}

void FxPage::chainABChanged ()
{
    chainAButton.setToggleState (processorRef.isShowingChainA(), juce::dontSendNotification);
    chainBButton.setToggleState (! processorRef.isShowingChainA(), juce::dontSendNotification);
    dropSlot = -2; // every card flashes
    dropFlash = 1.0f;
    updateVisibility();
    repaint();
}

void FxPage::updateVisibility ()
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
    for (juce::Component* part : { (juce::Component*) &filterPicker, (juce::Component*) &filterCategories, (juce::Component*) &filterSlopePills })
        part->setVisible (false); // (placeCard shows them on the FILTER card in front)

    library->refresh();
    // (A patch load sets which chain shows: the pills follow it here.)
    chainAButton.setToggleState (processorRef.isShowingChainA(), juce::dontSendNotification);
    chainBButton.setToggleState (! processorRef.isShowingChainA(), juce::dontSendNotification);
    copyChainButton.setButtonText (processorRef.isShowingChainA() ? "COPY TO 2" : "COPY TO 1");
    copyChainButton.setTooltip (processorRef.isShowingChainA() ? "Copy this whole chain over chain 2" : "Copy this whole chain over chain 1");

    resized();
}

void FxPage::syncInstanceCards (const std::vector<StackPanel>& cards)
{
    const auto patch = processorRef.getFxPatch();
    std::array<bool, IlanaSynthAudioProcessor::numFxSlots> wanted {};
    for (const auto& panel : cards)
        if (panel.instance)
        {
            wanted[(size_t) panel.slot] = true;
            auto& card = instanceCards[(size_t) panel.slot];
            if (card.shadow == nullptr || card.shadow->getType() != panel.type)
                buildInstanceCard (panel.slot, panel.type, patch);
        }

    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        auto& card = instanceCards[(size_t) slot];
        if (! wanted[(size_t) slot] && card.shadow != nullptr)
        {
            dropInstanceCard (slot);
            continue;
        }
        card.visible = wanted[(size_t) slot];
        // (An Airwindows module shows the knobs its algorithm uses, as on the type's card.)
        for (size_t i = 0; i < card.group.size(); ++i)
        {
            card.group[i]->setVisible (card.visible && card.originals[i]->isVisible());
            if (auto* knob = dynamic_cast<KnobControl*> (card.group[i]))
                if (auto* original = dynamic_cast<KnobControl*> (card.originals[i]); original != nullptr && knob->getLabelText() != original->getLabelText())
                    knob->setLabelText (original->getLabelText());
        }
    }
}

void FxPage::buildInstanceCard (int slot, int type, const FxPatchData::Patch& patch)
{
    dropInstanceCard (slot);
    auto& card = instanceCards[(size_t) slot];
    std::vector<juce::String> ids;
    for (const auto& entry : patch.slots[(size_t) slot].values)
        ids.push_back (entry.first);
    card.shadow = std::make_unique<FxInstance::Shadow> (processorRef, slot, type, ids);
    auto& state = card.shadow->getState();

    for (auto* item : slotGroups[(size_t) type])
    {
        std::unique_ptr<juce::Component> copy;
        if (auto* knob = dynamic_cast<KnobControl*> (item); knob != nullptr && state.getParameter (knob->getParameterId()) != nullptr)
        {
            auto made = std::make_unique<KnobControl> (state, knob->getParameterId(), knob->getLabelText());
            made->setIdentityColour (fxColour (type));
            packControl (made.get());
            copy = std::move (made);
        }
        else if (auto* combo = dynamic_cast<ComboControl*> (item); combo != nullptr && state.getParameter (combo->getParameterId()) != nullptr)
        {
            auto made = std::make_unique<ComboControl> (state, combo->getParameterId(), combo->getLabelText());
            packControl (made.get());
            copy = std::move (made);
        }
        else if (auto* toggle = dynamic_cast<ToggleControl*> (item); toggle != nullptr && state.getParameter (toggle->getParameterId()) != nullptr)
        {
            auto made = std::make_unique<ToggleControl> (state, toggle->getParameterId(), toggle->getButton().getButtonText());
            packControl (made.get());
            copy = std::move (made);
        }
        if (copy == nullptr)
            continue;
        stackContent.addChildComponent (*copy);
        card.group.push_back (copy.get());
        card.originals.push_back (item);
        card.controls.push_back (std::move (copy));
    }
}

void FxPage::packControl (juce::Component* item, const DialGrid& grid)
{
    if (auto* knob = dynamic_cast<KnobControl*> (item))
    {
        knob->setPackedDial (grid.dial);
        knob->setPackedValueSize (grid.value);
        knob->setPackedValueRoom (12);
        knob->setPackedNameWrap (true);
    }
    else if (auto* combo = dynamic_cast<ComboControl*> (item))
        combo->setPackedMenu (true);
    else if (auto* toggle = dynamic_cast<ToggleControl*> (item))
        toggle->setPackedName (ToggleControl::NamePlace::right);
}

auto FxPage::packedPlanFor (int knobs, int sides, int room, int pitch)
     -> PackedPlan
{
    PackedPlan plan;
    plan.pitch = pitch;
    plan.knobColumns = knobs;
    plan.sideColumns = (sides + 2) / 3;
    if (plan.width() <= room || knobs < 2)
        return plan;
    plan.knobRows = 2;
    plan.knobColumns = (knobs + 1) / 2;
    plan.sidePerColumn = 5;
    plan.sideColumns = (sides + 4) / 5;
    return plan;
}

bool FxPage::filterFxHasSlope () const
{
    const auto type = filterFxType();
    return ! filterFxMorphs() && type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
           && FilterType::usesSlope (type);
}

int FxPage::controlsRoom () const
{
    const auto width = stackView.getWidth() > 0 ? stackView.getWidth() : 990;
    return width - (railWidth + railGap) - 20 - leftWidth - 10 - minDisplayWidth - 10;
}

auto FxPage::packedPlanFor (const StackPanel& panel) const
     -> PackedPlan
{
    auto knobs = 0, sides = 0;
    for (const auto* item : cardItems (panel))
        (isSideItem (item) ? sides : knobs) += 1;
    sides += (showsBand (panel.slot) ? 1 : 0) + (panel.type == 13 && ! panel.duplicate && ! panel.instance ? 1 : 0);
    return packedPlanFor (knobs, sides, controlsRoom(), dialGrid().pitch);
}

std::vector<juce::Component*> FxPage::rowItems (int type, ToggleControl** power) const
{
    return rowItems (slotGroups[(size_t) type], type, power);
}

const std::vector<juce::Component*>& FxPage::groupFor (const StackPanel& panel) const
{
    if (panel.instance)
        if (const auto& card = instanceCards[(size_t) panel.slot]; card.shadow != nullptr)
            return card.group;
    return slotGroups[(size_t) panel.type];
}

std::vector<juce::Component*> FxPage::rowItems (const std::vector<juce::Component*>& group, int type, ToggleControl** power) const
{
    std::vector<juce::Component*> items;
    ToggleControl* found = nullptr;

    for (auto* item : group)
    {
        auto* toggle = dynamic_cast<ToggleControl*> (item);

        if (isAirwindowsType (type) && ! item->isVisible())
            continue;

        // (AW Delay's SYNC is a setting beside its DIV, not the card's switch.)
        if (toggle != nullptr && toggle->isSwitch() && found == nullptr && toggle->getParameterId() != "fx_awdelay_sync")
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

std::vector<juce::Component*> FxPage::cardItems (const StackPanel& panel, ToggleControl** power) const
{
    auto items = rowItems (groupFor (panel), panel.type, power);
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

std::vector<juce::Component*> FxPage::gridItems (const StackPanel& panel, ToggleControl** power)
{
    auto items = cardItems (panel, power);
    if (showsBand (panel.slot))
        items.insert (items.begin(), &cardHeaders[(size_t) panel.slot].band);
    if (panel.type == 13 && ! panel.duplicate && ! panel.instance)
        items.push_back (&loadIrButton);
    return items;
}

int FxPage::airwindowsIndexFor (int type) const
{
    if (type == 30)
        return airwindowsAlgorithm();
    if (const auto c = airwindows::categoryForFxType (type); c >= 0)
        return airwindows::categoryModules()[(size_t) c].algorithms[(size_t) categoryChoice (c)];
    return -1;
}

juce::String FxPage::cardInfoText (int type) const
{
    if (const auto index = airwindowsIndexFor (type); index >= 0)
        return FxInfoText::forAirwindows (airwindows::registry()[(size_t) index].name);
    return FxInfoText::forType (type);
}

bool FxPage::showsSlotMix (const StackPanel& panel) const
{
    if (panel.duplicate)
        return false;
    if (! hasOwnMix (panel.type))
        return true;
    const auto* blend = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (panel.slot + 1) + "_mix");
    return blend != nullptr && blend->load() < 0.995f;
}

juce::String FxPage::cardTitle (int type) const
{
    const auto base = isAirwindowsType (type) && hasModelSwitch (type) ? fxTwinOf (type) : type;
    return getSlotName (base).toUpperCase();
}

int FxPage::naturalHeight (const StackPanel& panel) const
{
    if (panel.duplicate)
        return duplicateHeight;

    const auto rows = packedPlanFor (panel).knobRows;
    return (rows == 1 ? rowHeightStandard : 2 * rowPad + rows * dialGrid().rowHeight) + (hasTapGrid (panel) ? tapGridHeight : 0);
}

void FxPage::layoutStack ()
{
    stackPanels.clear();
    splitGroups.clear();

    for (auto& slotSwitch : slotSwitches)
        slotSwitch->setVisible (false);
    for (auto& header : cardHeaders)
        for (juce::Component* item : { (juce::Component*) &header.type, (juce::Component*) &header.solo, (juce::Component*) &header.band,
                                       (juce::Component*) &header.remove, (juce::Component*) &header.edit, (juce::Component*) &header.chip,
                                       (juce::Component*) header.mix.get() })
            item->setVisible (false);
    for (auto& display : displays)
        display->setVisible (false);
    for (auto& strip : crossoverStrips)
        strip->setVisible (false);

    // The cards in chain order.
    std::vector<StackPanel> cards;
    // A type's controls go on the card in front (step 17: the instance
    // being edited, else the first); its other cards are compact.
    std::array<int, 64> front {};
    front.fill (-1);
    for (int slot = IlanaSynthAudioProcessor::numFxSlots - 1; slot >= 0; --slot)
        if (const auto type = getSlotType (slot); type > 0 && type < 64)
            if (front[(size_t) type] < 0 || processorRef.isFxSlotInFront (slot))
                front[(size_t) type] = slot;

    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        const auto type = getSlotType (slot);

        if (type <= 0 || type >= (int) slotGroups.size())
            continue;

        StackPanel panel;
        panel.slot = slot;
        panel.type = type;
        panel.instance = front[(size_t) type] != slot && isOwnCard (slot);
        panel.duplicate = front[(size_t) type] != slot && ! panel.instance;
        cards.push_back (panel);
    }
    syncInstanceCards (cards);

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

    const auto viewHeight = availableStackHeight;
    auto overhead = stackTopMargin + stackTailMargin + juce::jmax (0, (int) cards.size() - 1) * rowGap;
    for (const auto& run : runs)
        overhead += run.kind != 0 ? splitHeaderHeight + splitInsetRight : 0;

    // Every card keeps the size it has in a rack of five (ilana 2026-11-17: "have them all be
    // one size", the size at four or five effects): the compact dials at the card's natural
    // height, never grown into a short rack's spare height nor compressed in a long one (that scrolls).
    std::vector<int> heights;
    for (const auto& card : cards)
        heights.push_back (naturalHeight (card));

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

    stackNaturalHeight = y - rowGap + stackTailMargin;
    stackRows = rowsPlaced;
    stackContent.setSize (width, juce::jmax (stackNaturalHeight, stackView.getHeight()));
    stackContent.repaint();
}

auto FxPage::leftBlockFor (juce::Rectangle<int> card)
     -> LeftBlock
{
    auto block = card.withTrimmedLeft (10).withWidth (leftWidth).withSizeKeepingCentre (leftWidth, 48); // off-grid
    block.setX (card.getX() + 10);
    auto line1 = block.removeFromTop (22); // off-grid
    block.removeFromTop (Space::s6);
    auto line2 = block.removeFromTop (20); // off-grid

    LeftBlock result;
    result.number = line1.removeFromLeft (18).withSizeKeepingCentre (18, 18); // off-grid
    line1.removeFromLeft (Space::s4);
    result.name = line1;
    result.solo = line2.removeFromLeft (Space::s24);
    line2.removeFromLeft (Space::s6);
    result.power = line2.removeFromLeft (36); // off-grid
    line2.removeFromLeft (Space::s6);
    result.chip = line2.withHeight (20); // off-grid
    return result;
}

void FxPage::placeCard (StackPanel panel, juce::Rectangle<int> bounds)
{
    panel.bounds = bounds;
    const auto slot = panel.slot;
    const auto type = panel.type;
    auto& header = cardHeaders[(size_t) slot];
    // A tall card (a short rack's) shows the slot's IN and OUT levels live
    // under its left block, the two centred together (UI review 31: the
    // block alone stood in a column of air). A row without a display has
    // them in its picture's place already.
    // On a card as tall as a lone effect's, the block goes to the top and
    // the levels stand as two tall bars under it, the column's height.
    panel.meters = {};
    auto blockArea = bounds;
    if (! panel.duplicate && bounds.getHeight() >= tallCardHeight && hasCardDisplay (type))
    {
        blockArea = bounds.withHeight (48 + 2 * 16);
        const auto block = leftBlockFor (blockArea);
        panel.meters = juce::Rectangle<int>::leftTopRightBottom (block.number.getX(), block.chip.getBottom() + 24,
                                                                 block.number.getX() + leftWidth - 4, bounds.getBottom() - 16);
    }
    else if (! panel.duplicate && bounds.getHeight() >= 150 && hasCardDisplay (type))
    {
        blockArea = bounds.withTrimmedBottom (leftMetersHeight + 16);
        const auto block = leftBlockFor (blockArea);
        panel.meters = { block.number.getX(), block.chip.getBottom() + 16, leftWidth - 4, leftMetersHeight };
    }
    const auto left = leftBlockFor (blockArea);
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

    auto inner = bounds.reduced (10, rowPad).withTrimmedLeft (leftWidth + 10); // off-grid

    if (panel.duplicate)
    {
        // Duplicates share settings but bypass on their own.
        slotSwitches[(size_t) slot]->setBare (true);
        slotSwitches[(size_t) slot]->setColour (fxColour (panel.type));
        slotSwitches[(size_t) slot]->setVisible (true);
        slotSwitches[(size_t) slot]->setBounds (left.power.withHeight (20)); // off-grid
        auto& remove = header.remove;
        remove.setBounds (juce::Rectangle<int> (bounds.getRight() - 12 - 84, bounds.getCentreY() - 12, 84, 24));
        remove.setVisible (true);
        // A duplicate that shares the first's settings (an older patch):
        // MAKE OWN. (An instance with its own settings has a full card.)
        auto& edit = header.edit;
        edit.setButtonText ("MAKE OWN");
        edit.setTooltip ("Give this copy its own settings and engine, so it can sound different from the first");
        edit.setBounds (remove.getBounds().translated (-100, 0).withWidth (90)); // off-grid
        edit.setVisible (true);
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
        power->setSwitchColour (fxColour (panel.type));
        power->setBounds (left.power.withHeight (20)); // off-grid
    }
    else
    {
        slotSwitches[(size_t) slot]->setBare (true);
        slotSwitches[(size_t) slot]->setColour (fxColour (panel.type));
        slotSwitches[(size_t) slot]->setVisible (true);
        slotSwitches[(size_t) slot]->setBounds (left.power.withHeight (20)); // off-grid
    }

    // The delay's tap grid under the row while TAPS is on.
    if (hasTapGrid (panel))
    {
        tapGrid.setBounds (inner.removeFromBottom (tapGridHeight - 4).withTrimmedBottom (Space::s2).withTrimmedTop (Space::s2));
        inner.removeFromBottom (Space::s6); // (the 32 px dials' names reach 4 px under their row: they keep clear of the grid)
    }

    // The controls at the right on PLAY's grid, the picture taking the rest.
    std::vector<juce::Component*> knobs, sides;
    // FILTER in front: the categories and the picker in a block at the left of its knobs.
    const auto filterFront = type == 42 && ! panel.instance;
    const auto& grid = dialGrid();
    for (auto* item : items)
    {
        packControl (item, grid);
        if (filterFront && (item == &filterType || item == &filterSlope || (item == &filterMorph && ! filterFxMorphs())))
        {
            item->setVisible (false);
            continue;
        }
        (isSideItem (item) ? sides : knobs).push_back (item);
    }
    if (filterFront && filterFxHasSlope())
        knobs.insert (knobs.begin() + juce::jmin ((int) knobs.size(), 4), &filterSlopePills); // (where MORPH goes on a model that morphs)
    auto plan = packedPlanFor ((int) knobs.size(), (int) sides.size(), inner.getWidth() - minDisplayWidth - 10 - (filterFront ? filterTypeBlockWidth + sideColumnGap : 0), grid.pitch);
    // A row grown tall takes its knobs in as many rows as its height holds, so they fill it and
    // the picture widens: four or more knobs in a grid of two columns or more, two or three one under the other
    // (no row of one beside a row of two). The side columns hold as many as the rows' height.
    {
        const auto count = (int) knobs.size();
        // (The last row's air under its name may overhang the card's padding, not the tap grid under it.)
        const auto fit = juce::jmax (plan.knobRows, (inner.getHeight() + (hasTapGrid (panel) ? 0 : 8)) / grid.rowHeight);
        auto rows = plan.knobRows;
        if (count >= 4)
            rows = juce::jmax (rows, juce::jmin (fit, (count + 1) / 2)); // (two columns at least: no tall single file)
        else if (count >= 2 && fit >= count)
            rows = count;
        if (rows != plan.knobRows)
        {
            plan.knobColumns = (count + rows - 1) / rows;
            plan.knobRows = (count + plan.knobColumns - 1) / plan.knobColumns;
            plan.sidePerColumn = juce::jmax (5, (plan.knobRows * grid.rowHeight + sideGap) / (sideRow + sideGap));
            plan.sideColumns = ((int) sides.size() + plan.sidePerColumn - 1) / plan.sidePerColumn;
        }
    }
    const auto blockWidth = filterFront ? filterTypeBlockWidth + sideColumnGap : 0;
    auto knobsArea = plan.width() > 0 ? inner.removeFromRight (plan.width() + blockWidth) : juce::Rectangle<int>();
    if (plan.width() > 0)
        inner.removeFromRight (10); // off-grid

    panel.well = {};
    if (hasCardPicture (type))
    {
        const auto displayArea = inner;
        // (The EQ curve and the gate's pattern edit the type's own
        // parameters: an instance's card shows its sentence there.)
        if (type == 29 && ! panel.instance)
            eqCurve.setBounds (displayArea);
        else if (type == 16 && ! panel.instance)
            gateGrid->setBounds (displayArea);
        else if (FxDisplay::hasDisplay (type))
        {
            auto& display = *displays[(size_t) slot];
            display.setValueSource (panel.instance ? &instanceCards[(size_t) slot].shadow->getState() : nullptr);
            display.setSlot (slot);
            display.setType (type, fxColour (type));
            display.setBounds (displayArea);
            display.setVisible (true);
        }
        else
            panel.well = displayArea;
    }

    // Side columns: their items from the top of the knob rows' band, one under the other.
    // (On a tall card the rows stand apart, up to twice the grid, so the
    // knobs share the card's height rather than sitting in a band of air.)
    const auto roomPerRow = knobsArea.getHeight() / juce::jmax (1, plan.knobRows);
    const auto rowPitch = juce::jlimit (grid.rowHeight, juce::roundToInt ((float) grid.rowHeight * 1.3f), roomPerRow);
    const auto band = knobsArea.withSizeKeepingCentre (knobsArea.getWidth(), plan.knobRows * rowPitch);
    auto sideArea = band.withWidth (plan.sideColumns * (sideWidth + sideColumnGap));
    const auto sideHeight = juce::jmin ((int) sides.size(), plan.sidePerColumn) * (sideRow + sideGap) - sideGap;
    for (size_t i = 0; i < sides.size(); ++i)
    {
        const auto column = (int) i / plan.sidePerColumn, row = (int) i % plan.sidePerColumn;
        const auto top = band.getCentreY() - sideHeight / 2;
        const auto cell = juce::Rectangle<int> (sideArea.getX() + column * (sideWidth + sideColumnGap), top + row * (sideRow + sideGap), sideWidth, sideRow);
        sides[i]->setBounds (sides[i] == &loadIrButton ? cell.withSizeKeepingCentre (sideWidth, sideRow + 2) : cell);
    }

    if (filterFront)
    {
        // The categories over the picker, centred on the knobs' band.
        const auto block = juce::Rectangle<int> (sideArea.getRight(), band.getCentreY() - (sideRow + sideGap + 22) / 2, filterTypeBlockWidth, sideRow + sideGap + 22);
        filterCategories.setBounds (block.withHeight (sideRow));
        filterPicker.setBounds (block.withTrimmedTop (sideRow + sideGap));
        for (juce::Component* part : { (juce::Component*) &filterCategories, (juce::Component*) &filterPicker })
            part->setVisible (true);
        filterSlopePills.setVisible (filterFxHasSlope());
        sideArea = sideArea.withWidth (sideArea.getWidth() + blockWidth);
    }

    // Knobs: rows on the grid's pitch, the dial with its value inside, the name under.
    const auto knobsX = sideArea.getRight();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto column = (int) i % juce::jmax (1, plan.knobColumns), row = (int) i / juce::jmax (1, plan.knobColumns);
        // (Each row's dial and name centred on the row, as the menus beside them are.)
        const auto rowCentre = band.getY() + rowPitch / 2 + row * rowPitch;
        const auto cell = juce::Rectangle<int> (knobsX + column * grid.pitch, rowCentre - ((int) grid.dial + 10) / 2, grid.pitch, (int) grid.dial + 10 + 13);
        knobs[i]->setBounds (KnobControl::packedBoundsFor (cell));
    }

    stackPanels.push_back (panel);
}

int FxPage::firstEmptySlot () const
{
    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        if (getSlotType (slot) == 0)
            return slot;

    return -1;
}

void FxPage::paintSplitGroup (juce::Graphics& g, const SplitGroup& group)
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
    IlanaTheme::paintCardTitle (g, heading.withWidth (150), group.frequency ? "SPLIT BANDS" : "MID / SIDE", IlanaTheme::accent()); // off-grid

    if (! group.frequency)
    {
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("These slots work on the middle or the sides of the stereo image; the rest passes around them",
                    heading.withTrimmedLeft (150).withTrimmedRight (14), juce::Justification::centredLeft, true); // off-grid
    }
}

juce::Colour FxPage::firstRailColour () const
{
    return stackPanels.empty() ? IlanaTheme::Ui::line : fxColour (stackPanels.front().type);
}

juce::Colour FxPage::lastRailColour () const
{
    return stackPanels.empty() ? IlanaTheme::Ui::line : fxColour (stackPanels.back().type);
}

void FxPage::paintRailPill (juce::Graphics& g, juce::Rectangle<int> column, const juce::String& text)
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

void FxPage::paintRailLine (juce::Graphics& g, float x, float from, float to, juce::Colour colour, bool dashed)
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

void FxPage::paintRailArrow (juce::Graphics& g, float x, float tipY, juce::Colour colour, bool dim)
{
    juce::Path arrow;
    arrow.addTriangle (x - 4.0f, tipY - 6.0f, x + 4.0f, tipY - 6.0f, x, tipY);
    g.setColour (colour.withAlpha (dim ? 0.45f : 0.9f));
    g.fillPath (arrow);
}

void FxPage::paintRail (juce::Graphics& g)
{
    if (patcherOn)
        return paintPatcherRail (g);
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

void FxPage::paintPatcherRail (juce::Graphics& g)
{
    const auto x = (float) railWidth * 0.5f;
    paintRailLine (g, x, 0.0f, (float) stackContent.getHeight(), IlanaTheme::Ui::text3.withAlpha (0.5f), true);
    for (const auto& panel : stackPanels)
        paintRailNode (g, x, (float) panel.bounds.getCentreY(), fxColour (panel.type), isModuleOff (panel.slot));
}

void FxPage::paintRailNode (juce::Graphics& g, float x, float centre, juce::Colour colour, bool off)
{
    const auto node = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, centre });
    g.setColour (off ? IlanaTheme::Ui::raised : colour);
    g.fillEllipse (node);
    g.setColour (colour.withAlpha (off ? 0.6f : 1.0f));
    g.drawEllipse (node.reduced (1.0f), 2.0f);
}

void FxPage::paintParallelRail (juce::Graphics& g)
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

void FxPage::paintRailRung (juce::Graphics& g, float from, float to, float y, juce::Colour colour, bool dim)
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

void FxPage::paintRailChip (juce::Graphics& g, const StackPanel& panel, float x)
{
    {
        const auto off = isModuleOff (panel.slot);
        const auto chipCentre = juce::Point<float> (x, (float) panel.bounds.getBottom() + (float) rowGap * 0.5f);
        const auto slot = panel.slot;
        const auto text = off ? juce::String ("BYPASS") : railText (slot);
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
        const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (font), text);
        const auto chipWidth = (off ? 0.0f : 21.0f) + textWidth + 12.0f;
        auto chip = juce::Rectangle<float> (chipWidth, 16.0f).withCentre (chipCentre.translated (0.0f, (float) -Space::s2));
        // Keep the whole pill inside the rail column (it was cut by the window edge).
        chip.setX (juce::jlimit (1.0f, juce::jmax (1.0f, (float) railWidth + (float) railGap * 0.5f - chip.getWidth()), chip.getX()));
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

juce::String FxPage::railText (int slot) const
{
    const auto in = processorRef.getFxSlotInLevel (slot);
    if (in < 1.0e-4f)
        return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94 dB"));
    const auto db = juce::Decibels::gainToDecibels (processorRef.getFxSlotOutLevel (slot) / in, -60.0f);
    if (std::abs (db) < 0.05f)
        return "0.0 dB";
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

float FxPage::railFill (int slot) const
{
    const auto db = juce::Decibels::gainToDecibels (processorRef.getFxSlotOutLevel (slot), -60.0f);
    return juce::jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f);
}

void FxPage::paintStack (juce::Graphics& g)
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
            auto text = panel.bounds.withTrimmedLeft (leftWidth + 20).withTrimmedRight (210).reduced (Space::s4, Space::s6); // off-grid
            const auto first = slotHoldingType (panel.type, panel.slot);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            IlanaTheme::drawFitted (g, "Copy of " + getSlotName (panel.type) + ": it shares slot " + juce::String (first + 1) + "'s settings.",
                                    text.removeFromTop (20), juce::Justification::centredLeft); // off-grid
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, "MAKE OWN gives it its own settings, so the two can differ.",
                                    text, juce::Justification::topLeft, 2);
        }

        if (! panel.meters.isEmpty())
            paintLevelMeters (g, panel, panel.meters, off);

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

void FxPage::paintLevelMeters (juce::Graphics& g, const StackPanel& panel, juce::Rectangle<int> meters, bool off)
{
    const auto slot = panel.slot;
    const auto in = off ? 0.0f : processorRef.getFxSlotInLevel (slot), out = off ? 0.0f : processorRef.getFxSlotOutLevel (slot);
    const auto draw = [&] (juce::Rectangle<int> row, const char* name, float level)
    {
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (name, row.removeFromLeft (30), juce::Justification::centredLeft); // off-grid
        const auto bar = row.toFloat().withSizeKeepingCentre ((float) row.getWidth(), 6.0f);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (bar, 3.0f);
        const auto fill = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 48.0f) / 48.0f);
        g.setColour (fxColour (panel.type).withAlpha (0.85f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fill), 3.0f);
    };
    if (meters.getHeight() <= meters.getWidth())
    {
        const auto half = meters.getHeight() / 2;
        draw (meters.removeFromTop (half), "IN", in);
        draw (meters, "OUT", out);
        return;
    }

    // Standing: IN and OUT side by side, filled from the foot, with
    // lines at -12, -24 and -36 dB and the names under them.
    auto area = meters;
    auto names = area.removeFromBottom (18); // off-grid
    constexpr int gap = 14; // off-grid
    const auto each = (area.getWidth() - gap) / 2;
    for (int i = 0; i < 2; ++i)
    {
        const auto bar = area.withX (area.getX() + i * (each + gap)).withWidth (each).toFloat();
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (bar, 5.0f);
        const auto fill = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (i == 0 ? in : out, -60.0f) + 48.0f) / 48.0f);
        g.setColour (fxColour (panel.type).withAlpha (0.85f));
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * fill), 5.0f);
        g.setColour (IlanaTheme::Ui::text3.withAlpha (0.35f));
        for (const auto db : { 12.0f, 24.0f, 36.0f })
            g.fillRect (bar.getX() + 4.0f, bar.getY() + bar.getHeight() * db / 48.0f, bar.getWidth() - 8.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (i == 0 ? "IN" : "OUT", names.withX ((int) bar.getX()).withWidth (each), juce::Justification::centredBottom);
    }
}

void FxPage::paintInfoWell (juce::Graphics& g, const StackPanel& panel, bool off)
{
    const auto well = panel.well.toFloat();
    IlanaTheme::paintWell (g, well, 6.0f);
    auto inner = panel.well.reduced (Space::s8, 0);

    // IN and OUT meters at the right, the live reading of what the
    // effect does to the level.
    if (inner.getWidth() > 230)
    {
        const auto meters = inner.removeFromRight (86).reduced (0, Space::s8); // off-grid
        inner.removeFromRight (10); // off-grid
        paintLevelMeters (g, panel, meters, off);
    }

    auto caption = inner.removeFromTop (19).withTrimmedTop (3); // off-grid
    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    const auto index = airwindowsIndexFor (panel.type);
    IlanaTheme::drawFitted (g, index >= 0 ? airwindowsDisplayName (airwindows::registry()[(size_t) index].name).toUpperCase() : juce::String ("WHAT IT DOES"),
                            caption, juce::Justification::centredLeft);
    g.setColour (off ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    IlanaTheme::drawFitted (g, cardInfoText (panel.type), inner.withTrimmedBottom (Space::s4), juce::Justification::topLeft, juce::jmax (2, inner.getHeight() / 13));
}

void FxPage::paintDragGhost (juce::Graphics& g)
{
    if (removeFade > 0.01f && removeImage.isValid())
    {
        g.setOpacity (removeFade * 0.6f);
        g.drawImageAt (removeImage, removeBounds.getX(), removeBounds.getY());
        g.setOpacity (1.0f);
    }

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

int FxPage::slotAt (juce::Point<int> position) const
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

void FxPage::stackMouseDown (const juce::MouseEvent& event)
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

void FxPage::stackMouseDrag (const juce::MouseEvent& event)
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

void FxPage::stackMouseUp (const juce::MouseEvent&)
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

bool FxPage::rackHasKeys () const
{
    const auto* focused = juce::Component::getCurrentlyFocusedComponent();
    return focused == nullptr || focused == &stackContent;
}

bool FxPage::stackKeyPressed (const juce::KeyPress& key)
{
    // Delete or Backspace takes the selected card out (one undo step, as
    // PATCH does), while the rack itself has the keys (a click on a card).
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && dragSlot < 0
        && rackHasKeys() && getSlotType (selectedSlot) != 0)
    {
        const auto next = neighbourSlot (selectedSlot, 1) >= 0 ? neighbourSlot (selectedSlot, 1) : neighbourSlot (selectedSlot, -1);
        removeSlot (selectedSlot);
        if (next >= 0)
            select (next);
        return true;
    }

    if (key.getKeyCode() != juce::KeyPress::escapeKey || dragSlot < 0)
        return false;

    dragSlot = dropTarget = -1;
    dragActive = false;
    dragImage = {};
    stackContent.repaint();
    return true;
}

void FxPage::stackMouseMove (const juce::MouseEvent& event)
{
    auto overHeader = false;
    for (const auto& panel : stackPanels)
        overHeader = overHeader || panel.handle.contains (event.getPosition());

    stackContent.setMouseCursor (overHeader ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void FxPage::scrollToSlot (int slot)
{
    for (const auto& panel : stackPanels)
        if (panel.slot == slot)
        {
            const auto view = stackView.getViewArea();
            if (! view.contains (panel.bounds))
                stackView.setViewPosition (0, juce::jmax (0, panel.bounds.getY() - 4));
        }
}

void FxPage::visibilityChanged ()
{
    if (! IlanaAnim::showing (*this))
    {
        dragSlot = dropTarget = -1;
        dragActive = false;
        dragImage = {};
    }
}

void FxPage::timerCallback ()
{
    const auto ticks = frameTicks();
    const auto flashing = dropFlash > 0.01f;
    dropFlash = IlanaAnim::decay (dropFlash, 0.85f, ticks);

    if (removeFade > 0.01f)
    {
        removeFade = IlanaAnim::decay (removeFade, 0.82f, ticks);
        stackContent.repaint (removeBounds);

        if (removeFade <= 0.01f)
        {
            removeFade = 0.0f;
            removeImage = {};
        }
    }

    const auto showingA = processorRef.isShowingChainA();

    if (chainAButton.getToggleState() != showingA)
    {
        chainAButton.setToggleState (showingA, juce::dontSendNotification);
        chainBButton.setToggleState (! showingA, juce::dontSendNotification);
    }

    // Instance cards follow their settings changed elsewhere (undo, a preset).
    if (std::any_of (instanceCards.begin(), instanceCards.end(), [] (const InstanceCard& card) { return card.shadow != nullptr; }))
    {
        const auto patch = processorRef.getFxPatch();
        for (auto& card : instanceCards)
            if (card.shadow != nullptr)
                card.shadow->refresh (patch);
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
        signature << type << ":" << getSlotBand (slot) << (blend != nullptr && blend->load() < 0.995f ? "b" : "")
                  << (processorRef.isFxSlotInFront (slot) ? "" : "v") << (processorRef.isFxInstance (slot) ? "o" : "") << ",";
    }
    signature += processorRef.isFxPatcherOn() ? ",P" : ",-";

    signature += juce::String (airwindowsAlgorithm()); // its knobs differ
    for (int c = 0; c < (int) awCategories.size(); ++c)
        signature += "," + juce::String (categoryChoice (c));
    signature += tapsEnabled() ? ",taps" : ",-"; // the tap grid comes and goes
    signature += ",f" + juce::String ((filterFxMorphs() ? 2 : 0) + (filterFxHasSlope() ? 1 : 0)); // FILTER's MORPH or SLOPE
    signature += showingA ? ",A" : ",B";

    if (signature != lastSignature)
    {
        lastSignature = signature;
        if (getSlotType (selectedSlot) == 0)
            for (const auto& panel : stackPanels)
                if (getSlotType (panel.slot) != 0)
                {
                    selectedSlot = panel.slot;
                    cardChosen = false;
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
            {
                if (! panel.well.isEmpty())
                    stackContent.repaint (panel.well);
                if (! panel.meters.isEmpty())
                    stackContent.repaint (panel.meters);
            }
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

void FxPage::dropInstanceCard (int slot)
{
    if (displays[(size_t) slot] != nullptr)
        displays[(size_t) slot]->setValueSource (nullptr);
    instanceCards[(size_t) slot].clear();
}

bool FxPage::isModuleOff (int slot) const
{
    const auto prefix = "fx_slot" + juce::String (slot + 1);
    if (processorRef.apvts.getRawParameterValue (prefix + "_bypass")->load() > 0.5f)
        return true;

    if (const auto* id = enableParamFor (getSlotType (slot)))
    {
        // An instance's own card: its own switch.
        if (const auto& card = instanceCards[(size_t) slot]; card.shadow != nullptr && card.visible)
            if (const auto* value = card.shadow->getState().getRawParameterValue (id))
                return value->load() < 0.5f;
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load() < 0.5f;
    }

    return false;
}

bool FxPage::updateModuleDimming ()
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
                const auto isOwn = (part == &eqCurve && (panel.type != 29 || panel.instance)) || (part == gateGrid.get() && (panel.type != 16 || panel.instance))
                                   || (part == &tapGrid && (panel.type != 9 || panel.instance));
                if (! isOwn && part->getAlpha() != alpha)
                {
                    part->setAlpha (alpha);
                    changed = true;
                }
            }
        }

        if (panel.type == 42 && ! panel.instance)
            for (juce::Component* part : { (juce::Component*) &filterPicker, (juce::Component*) &filterCategories, (juce::Component*) &filterSlopePills })
                if (part->getAlpha() != alpha)
                {
                    part->setAlpha (alpha);
                    changed = true;
                }

        ToggleControl* power = nullptr;
        const auto& group = groupFor (panel);
        rowItems (group, panel.type, &power);
        // (An instance's own card reads its own settings.)
        const auto& card = instanceCards[(size_t) panel.slot];
        const auto& state = panel.instance && card.shadow != nullptr ? card.shadow->getState() : processorRef.apvts;

        for (auto* item : group)
        {
            auto* toggle = dynamic_cast<ToggleControl*> (item);
            const auto isPower = toggle != nullptr && toggle == power;
            auto itemAlpha = alpha;

            if (! off && isIdleControl (panel.type, parameterIdOf (item), state))
                itemAlpha = IlanaTheme::dimmedAlpha;

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

int FxPage::airwindowsAlgorithm () const
{
    if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_aw_algo"))
        return juce::jlimit (0, airwindows::count() - 1, (int) value->load());

    return 0;
}

juce::String FxPage::airwindowsKnobLabel (const char* name)
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

bool FxPage::isAirwindowsGain (const airwindows::Info& info, int k)
{
    static const std::map<juce::String, juce::String> gains {
        { "Density", "Out Level" }, { "Drive", "Out Level" }, { "Spiral2", "Output" }, { "Air", "Output Level" },
        { "Holt2", "Output" }, { "DrumSlam", "Output" }, { "Density3", "Output" }, { "Desk4", "Output Trim" } };
    const auto found = gains.find (juce::String (info.name));
    return found != gains.end() && found->second == juce::String (info.knobs[k].name)
           && info.knobs[k].lo == 0.0f && info.knobs[k].hi == 1.0f;
}

std::function<juce::String (double)> FxPage::airwindowsUnitText (const airwindows::Info& info, int k) const
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
        return [=] (double v) { return juce::String (juce::roundToInt (100.0 * std::pow (knob.toPlugin ((float) v), 2.0))) + "%"; };
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

void FxPage::setAirwindowsUnit (KnobControl& knob, const airwindows::Info& info, int k)
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

juce::String FxPage::airwindowsDisplayName (const char* name)
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

juce::String FxPage::airwindowsTooltip (const airwindows::Info& info)
{
    return "Airwindows " + juce::String (info.name) + " (" + info.category + ")\nClick to choose another algorithm.";
}

juce::String FxPage::airwindowsKnobTip (const airwindows::Info& info, int k) const
{
    return airwindowsKnobLabel (info.knobs[k].name) + " (Airwindows " + juce::String (info.name) + ": \""
           + juce::String (info.knobs[k].name) + "\")\n"
           + (isAirwindowsGain (info, k) ? "The plugin's output gain, in dB."
              : airwindowsUnitText (info, k) ? "In the unit its algorithm works in (worked out from the plugin's code)."
                                             : "The plugin's own control, shown as 0 to 100 % of its range: Airwindows gives it no unit.");
}

void FxPage::setKnobTip (KnobControl& knob, const juce::String& tip)
{
    knob.setTooltip (tip);
    knob.getSlider().setTooltip (tip);
}

void FxPage::updateAirwindowsKnobs (bool loaded)
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

void FxPage::showAirwindowsMenu ()
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
    menu.showMenuAsync (IlanaTheme::menuOptions (&awAlgo),
                        [safeThis] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->chooseAirwindows (result - 1);
                        });
}

void FxPage::chooseAirwindows (int algorithm)
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

    // One undo step for the pick and its knobs' defaults.
    processorRef.performEdit (airwindowsDisplayName (info.name), [&]
    {
        for (int k = 0; k < info.numKnobs; ++k)
            set ("fx_aw_p" + juce::String (k + 1), info.knobs[k].defaultValue);
        set ("fx_aw_algo", (float) algorithm);
    });
    updateVisibility();
    repaint();
}

int FxPage::categoryChoice (int c) const
{
    const auto& info = airwindows::categoryModules()[(size_t) c];
    if (const auto* value = processorRef.apvts.getRawParameterValue (juce::String ("fx_") + info.id + "_algo"))
        return juce::jlimit (0, (int) info.algorithms.size() - 1, (int) value->load());
    return 0;
}

void FxPage::updateCategoryKnobs (int c, bool loaded)
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

    // (StarChild2 has no time to sync.)
    if (controls.sync != nullptr)
    {
        const auto syncs = loaded && airwindows::delaysync::hasTime (category.algorithms[(size_t) categoryChoice (c)]);
        controls.sync->setVisible (syncs);
        controls.div->setVisible (syncs);
    }

    controls.algo->getComboBox().setTooltip (airwindowsTooltip (info));
}

void FxPage::showFilterTypeMenu ()
{
    auto* parameter = processorRef.apvts.getParameter ("fx_filter_type");
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
    menu.showMenuAsync (IlanaTheme::menuOptions (&filterType),
                        [safeThis = juce::Component::SafePointer<FxPage> (this), parameter] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;
                            safeThis->processorRef.performEdit ("Filter FX type", [parameter, result]
                            {
                                parameter->beginChangeGesture();
                                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                parameter->endChangeGesture();
                            });
                        });
}

void FxPage::showCategoryMenu (int c)
{
    const auto& category = airwindows::categoryModules()[(size_t) c];
    const auto chosen = categoryChoice (c);
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String (category.label).toUpperCase());
    for (int i = 0; i < (int) category.algorithms.size(); ++i)
        menu.addItem (i + 1, airwindowsDisplayName (airwindows::registry()[(size_t) category.algorithms[(size_t) i]].name), true, i == chosen);

    juce::Component::SafePointer<FxPage> safeThis (this);
    menu.showMenuAsync (IlanaTheme::menuOptions (awCategories[(size_t) c].algo.get()),
                        [safeThis, c] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->chooseCategoryEffect (c, result - 1);
                        });
}

void FxPage::chooseCategoryEffect (int c, int choice)
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

    processorRef.performEdit (airwindowsDisplayName (info.name), [&]
    {
        for (int k = 0; k < info.numKnobs; ++k)
            set (prefix + "_p" + juce::String (k + 1), info.knobs[k].defaultValue);
        set (prefix + "_algo", (float) choice);
    });
    updateVisibility();
    repaint();
}
