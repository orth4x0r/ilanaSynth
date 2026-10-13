void runPresetTuningTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    constexpr int note = 57; // A3
    const auto noteFrequency = juce::MidiMessage::getMidiNoteInHertz (note);

    juce::StringArray failures;
    auto worstCents = 0.0;
    juce::String worstName;

    const auto categories = processor.getFactoryPresetCategories();

    for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
    {
        // Sound effects (noise sweeps, sirens, risers) are not meant to be in tune.
        // Drums sweep their pitch and generative patches spray extra notes by design.
        // The DX7 banks are the cartridges as they were (their FX voices,
        // drums and detuned layers included); dx7_mode_check.py compares
        // them with Dexed instead.
        if (categories[presetIndex] == "FX" || categories[presetIndex] == "Drums" || categories[presetIndex] == "Generative"
            || categories[presetIndex] == "DX7")
            continue;

        processor.loadFactoryPreset (presetIndex);
        processor.panic();

        // Also not meant to be in tune: a prepared piano (bolts make its
        // strings inharmonic and detuned) and granular patches with grain
        // pitch spray (each grain is randomly transposed, from a time-seeded
        // generator, so the estimate varied from run to run).
        auto pitchByDesign = names[presetIndex] == "Bolted Strings";
        // ilanaSynth FX patches play the audio input, which the instrument
        // doesn't have (tests/FxTest.cpp covers them).
        if (const auto* category = Presets::getFactoryPresets()[(size_t) presetIndex].category;
            category != nullptr && juce::String (category) == "FX Input")
            continue;
        for (const auto* prefix : OscillatorIds::prefixes)
        {
            const juce::String p (prefix);
            if (processor.apvts.getRawParameterValue (p + "_on")->load() > 0.5f
                && (int) processor.apvts.getRawParameterValue (p + "_mode")->load() == 3
                && processor.apvts.getRawParameterValue (p + "_grain_pitch")->load() > 0.0f)
                pitchByDesign = true;
        }
        if (pitchByDesign)
            continue;

        // Measure the dry voice: effects (tape shift, freeze, taps...) may
        // legitimately obscure pitch and are not part of oscillator tuning.
        for (const auto* id : { "fx_drive_on", "fx_crush_on", "fx_comb_on", "fx_phaser_on",
                                "fx_chorus_on", "fx_delay_on", "fx_taps_on", "fx_stutter_on",
                                "fx_smear_on", "fx_freeze_on", "fx_reverb_on", "arp_on" })
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (0.0f);
        }

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> samples;

        const auto blocks = (int) (48000 * 2.2 / 512.0);

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midiForBlock;

            if (block == 0)
                midiForBlock = midi;

            processor.processBlock (buffer, midiForBlock);

            const auto startTime = (double) block * 512.0 / 48000.0;

            if (startTime >= 0.02)
                for (int s = 0; s < 512; ++s)
                    samples.push_back ((buffer.getSample (0, s) + buffer.getSample (1, s)) * 0.5f);
        }

        auto peak = 0.0f;

        for (const auto value : samples)
            peak = juce::jmax (peak, std::abs (value));

        if (peak < 0.002f)
        {
            failures.add (names[presetIndex] + " (silent)");
            continue;
        }

        // Analyze the loudest 0.6 s window so plucks and slow pads both measure.
        const auto windowSize = (int) (48000 * 0.6);
        juce::StringArray windowSample;

        if ((int) samples.size() > windowSize)
        {
            auto bestStart = 0;
            auto bestRms = -1.0;

            for (int start = 0; start + windowSize <= (int) samples.size(); start += 4800)
            {
                auto sum = 0.0;

                for (int i = start; i < start + windowSize; i += 4)
                    sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

                if (sum > bestRms)
                {
                    bestRms = sum;
                    bestStart = start;
                }
            }

            std::vector<float> windowed (samples.begin() + bestStart, samples.begin() + bestStart + windowSize);
            samples = std::move (windowed);
        }

        const auto frequency = fundamentalOf (samples, 48000.0);

        if (frequency <= 0.0 || ! std::isfinite (frequency))
        {
            failures.add (names[presetIndex] + " (unmeasurable)");
            continue;
        }

        const auto semitones = 12.0 * std::log2 (frequency / noteFrequency);
        auto cents = std::abs ((semitones - std::round (semitones)) * 100.0);

        // Autocorrelation can lock onto a mix of partials in rich or
        // inharmonic sounds (the M8.2 piano's hammer and board); confirm an
        // out-of-tune reading with the strongest spectral peak within half a
        // semitone of the note (Goertzel sweep, 1 cent steps).
        if (cents > 30.0)
        {
            auto best = 0.0, bestHz = noteFrequency;
            for (double c = -50.0; c <= 50.0; c += 1.0)
            {
                const auto hz = noteFrequency * std::pow (2.0, c / 1200.0);
                const auto w = juce::MathConstants<double>::twoPi * hz / 48000.0;
                std::complex<double> sum;
                const auto length = (int) samples.size();
                for (int i = 0; i < length; ++i)
                    sum += (double) samples[(size_t) i] * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length))
                           * std::polar (1.0, -w * i);
                if (std::abs (sum) > best)
                {
                    best = std::abs (sum);
                    bestHz = hz;
                }
            }
            cents = std::abs (1200.0 * std::log2 (bestHz / noteFrequency));
        }

        if (cents > worstCents)
        {
            worstCents = cents;
            worstName = names[presetIndex];
        }

        // Vibrato patches (any mod slot routed to oscillator pitch) legitimately
        // swing around the note, so give them a wider measurement band.
        auto pitchModulated = false;

        for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        {
            const auto prefix = "mod" + juce::String (slot);

            if (const auto* destination = processor.apvts.getRawParameterValue (prefix + "_dst"))
            {
                const auto destinationIndex = (int) destination->load();

                if (destinationIndex == 1 || destinationIndex == 4 || destinationIndex == 28)
                {
                    const auto* amount = processor.apvts.getRawParameterValue (prefix + "_amt");

                    if (amount != nullptr && std::abs (amount->load()) > 0.001f)
                        pitchModulated = true;
                }
            }
        }

        const auto tolerance = pitchModulated ? 60.0 : 30.0;

        if (cents > tolerance)
            failures.add (names[presetIndex] + " (" + juce::String (cents, 1) + " cents @ "
                          + juce::String (frequency, 1) + " Hz)");
    }

    juce::String detail = "worst: " + worstName + " " + juce::String (worstCents, 1) + " cents";

    if (! failures.isEmpty())
        detail += "  |  out of tune: " + failures.joinIntoString (", ");

    check (failures.isEmpty(), "all factory presets tuned within 30 cents (" + detail + ")");
}
void runParameterStressTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto renderIsBounded = [&processor] ()
    {
        juce::AudioBuffer<float> buffer (2, 256);
        buffer.clear();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 90), 0);

        processor.processBlock (buffer, midi);

        auto peak = 0.0f;

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
            {
                const auto value = buffer.getSample (channel, i);

                if (! std::isfinite (value))
                    return -1.0f;

                peak = juce::jmax (peak, std::abs (value));
            }

        return peak;
    };

    auto extremeFailures = 0;
    auto extremeCount = 0;

    for (auto* parameter : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            for (const auto value : { 0.0f, 1.0f })
            {
                ranged->setValueNotifyingHost (value);
                ++extremeCount;

                const auto peak = renderIsBounded();

                if (peak < 0.0f || peak > 4.0f)
                {
                    ++extremeFailures;

                    if (extremeFailures <= 4)
                        std::cout << "DEBUG extreme " << parameter->getName (64) << " = " << value
                                  << " peak " << peak << std::endl;
                }
            }

            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            renderIsBounded(); // let the smoothed master/FX ramps settle before the next parameter
        }
    }

    check (extremeFailures == 0, "every parameter at min/max renders bounded audio ("
                                     + juce::String (extremeCount) + " extremes)");

    auto choiceFailures = 0;
    auto choiceCount = 0;
    juce::StringArray choiceFailNames;

    for (auto* parameter : processor.getParameters())
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            for (int index = 0; index < choice->choices.size(); ++index)
            {
                choice->setValueNotifyingHost (choice->convertTo0to1 ((float) index));
                ++choiceCount;

                const auto peak = renderIsBounded();

                if (peak < 0.0f || peak > 4.0f)
                {
                    ++choiceFailures;

                    if (choiceFailNames.size() < 40)
                        choiceFailNames.add (parameter->getName (64) + " #" + juce::String (index)
                                             + " peak " + juce::String (peak, 4));
                }
            }
        }
    }

    check (choiceFailures == 0, "every choice index renders bounded audio ("
                                    + juce::String (choiceCount) + " values)"
                                    + (choiceFailNames.isEmpty() ? juce::String()
                                                                 : "  bad: " + choiceFailNames.joinIntoString (", ")));

    processor.panic();
}
void runFxModuleIsolationTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setSlot = [&processor] (int slot, int type)
    {
        if (auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) type));
    };

    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        setSlot (slot, 0);

        if (auto* bypass = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_bypass"))
            bypass->setValueNotifyingHost (0.0f);

        if (auto* mix = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_mix"))
            mix->setValueNotifyingHost (1.0f);
    }

    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_slot1"));
    const auto typeCount = choice != nullptr ? choice->choices.size() : 0;

    for (int type = 0; type < typeCount; ++type)
    {
        setSlot (1, type);

        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        auto finite = true;

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();

            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto value = buffer.getSample (channel, i);

                    if (! std::isfinite (value))
                        finite = false;

                    peak = juce::jmax (peak, std::abs (value));
                }
        }

        const auto name = choice != nullptr ? choice->choices[type] : juce::String (type);
        check (finite && peak < 2.5f, "fx module '" + name + "' renders bounded audio (peak "
                                          + juce::String (peak, 4) + ")");
    }

    processor.panic();
}

void runFxIntegrityTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setParam = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto readParam = [&processor] (const juce::String& id)
    {
        if (const auto* value = processor.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    };

    const auto clearChain = [&] ()
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            setParam ("fx_slot" + juce::String (slot), 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_bypass", 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_solo", 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_mix", 1.0f);
        }
    };

    clearChain();

    // --- Delay must produce an echo at the set delay time and only there ---
    setParam ("fx_slot1", 9.0f); // Delay
    setParam ("fx_delay_on", 1.0f);
    setParam ("fx_delay_sync", 0.0f);
    setParam ("fx_delay_time", 100.0f);
    setParam ("fx_delay_feedback", 0.0f);
    setParam ("fx_delay_mix", 1.0f);
    setParam ("fx_delay_pitch", 0.0f);
    setParam ("fx_delay_wow", 0.0f);
    setParam ("fx_delay_duck", 0.0f);
    setParam ("fx_taps_on", 0.0f);
    setParam ("fx_delay_pingpong", 0.0f);
    setParam ("amp_attack", 0.001f);
    setParam ("amp_decay", 0.02f);
    setParam ("amp_sustain", 0.0f);
    setParam ("amp_release", 0.02f);

    const auto renderEcho = [&] (float delayMs)
    {
        processor.panic();
        processor.reset();
        processor.prepareToPlay (48000.0, 256);
        setParam ("fx_delay_time", delayMs);

        constexpr int totalBlocks = 220;
        std::vector<float> output ((size_t) totalBlocks * 256, 0.0f);

        for (int block = 0; block < totalBlocks; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 256);
            buffer.clear();

            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);

            if (block == 2)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 256; ++i)
                output[(size_t) (block * 256 + i)] = buffer.getSample (0, i);
        }

        return output;
    };

    const auto peakIn = [] (const std::vector<float>& output, float fromMs, float toMs)
    {
        const auto from = (int) (fromMs * 48.0f);
        const auto to = juce::jmin ((int) (toMs * 48.0f), (int) output.size());
        auto peak = 0.0f;

        for (int i = juce::jmax (0, from); i < to; ++i)
            peak = juce::jmax (peak, std::abs (output[(size_t) i]));

        return peak;
    };

    const auto echoAt100 = renderEcho (100.0f);
    check (peakIn (echoAt100, 90.0f, 115.0f) > 0.01f,
           "delay produces an echo at 100 ms (peak " + juce::String (peakIn (echoAt100, 90.0f, 115.0f), 4) + ")");
    check (peakIn (echoAt100, 190.0f, 215.0f) < 0.005f,
           "delay with zero feedback has no second echo (peak " + juce::String (peakIn (echoAt100, 190.0f, 215.0f), 4) + ")");

    const auto echoAt250 = renderEcho (250.0f);
    check (peakIn (echoAt250, 240.0f, 265.0f) > 0.01f && peakIn (echoAt250, 90.0f, 115.0f) < 0.005f,
           "delay time change moves the echo (250 ms peak " + juce::String (peakIn (echoAt250, 240.0f, 265.0f), 4)
               + ", 100 ms window " + juce::String (peakIn (echoAt250, 90.0f, 115.0f), 4) + ")");

    setParam ("fx_taps_on", 1.0f);
    setParam ("fx_taps_pattern", 0.0f);
    setParam ("fx_taps_mix", 1.0f);

    const auto tapped = renderEcho (200.0f);
    check (peakIn (tapped, 40.0f, 60.0f) > 0.005f && peakIn (tapped, 90.0f, 110.0f) > 0.005f
               && peakIn (tapped, 190.0f, 215.0f) > 0.01f,
           "delay taps land at their fractions of the delay time ("
               + juce::String (peakIn (tapped, 40.0f, 60.0f), 4) + " / "
               + juce::String (peakIn (tapped, 90.0f, 110.0f), 4) + " / "
               + juce::String (peakIn (tapped, 190.0f, 215.0f), 4) + ")");

    setParam ("fx_taps_on", 0.0f);
    setParam ("fx_taps_mix", 0.0f);

    processor.panic();
    clearChain();

    // --- A/B chains capture and restore the rack ---
    setParam ("fx_slot1", 3.0f);
    setParam ("fx_slot2", 4.0f);
    processor.switchFxChain(); // A stored, now on B (a copy of A)

    setParam ("fx_slot1", 5.0f);
    setParam ("fx_slot2", 6.0f);
    processor.switchFxChain(); // back to A

    const auto slot1A = readParam ("fx_slot1");
    const auto slot2A = readParam ("fx_slot2");
    check (std::abs (slot1A - 3.0f) < 0.01f && std::abs (slot2A - 4.0f) < 0.01f,
           "A/B chain restores bank A (slot1 " + juce::String (slot1A, 1) + ", slot2 " + juce::String (slot2A, 1) + ")");

    processor.switchFxChain(); // to B
    const auto slot1B = readParam ("fx_slot1");
    check (std::abs (slot1B - 5.0f) < 0.01f,
           "A/B chain restores bank B (slot1 " + juce::String (slot1B, 1) + ")");

    // --- FX chain file round trip ---
    const auto chainFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ilana_chain_test.ilanafxchain");
    chainFile.deleteFile();

    const auto chainSaved = processor.saveFxChainToFile (chainFile);
    check (chainSaved && chainFile.existsAsFile() && chainFile.getSize() > 0,
           "fx chain saves to file (" + juce::String (chainFile.getSize()) + " bytes)");

    setParam ("fx_slot1", 0.0f);

    check (processor.loadFxChainFromFile (chainFile), "fx chain loads from file");

    const auto slot1Reloaded = readParam ("fx_slot1");
    check (std::abs (slot1Reloaded - 5.0f) < 0.01f,
           "fx chain file restores slot types (slot1 " + juce::String (slot1Reloaded, 1) + ")");

    chainFile.deleteFile();

    // --- Preset files carry sample paths ---
    const auto sampleFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_sample_preset_test.wav");
    sampleFile.deleteFile();

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (sampleFile);
        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (48000.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, 4096);

            for (int i = 0; i < 4096; ++i)
                buffer.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 48000.0f));

            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            writer.reset();
        }
    }

    check (processor.loadUserSample (0, sampleFile), "sample loads for preset test");

    const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_sample_preset_test.ilanapreset");
    presetFile.deleteFile();

    check (processor.savePresetToFile (presetFile), "preset with sample saves");

    const auto presetText = presetFile.loadFileAsString();
    check (presetText.contains ("SamplePath"),
           "saved preset contains the sample path");

    presetFile.deleteFile();
    sampleFile.deleteFile();

    processor.panic();
}

void runStateRoundTripTest()
{
    const auto setParam = [] (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto readParam = [] (IlanaSynthAudioProcessor& processor, const juce::String& id)
    {
        if (const auto* value = processor.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    };

    const auto sampleFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_roundtrip_sample.wav");
    sampleFile.deleteFile();

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (sampleFile);
        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (48000.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, 2048);

            for (int i = 0; i < 2048; ++i)
                buffer.setSample (0, i, 0.4f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) i / 48000.0f));

            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            writer.reset();
        }
    }

    IlanaSynthAudioProcessor source;
    source.prepareToPlay (48000.0, 256);

    setParam (source, "osc1_mode", 2.0f);
    setParam (source, "osc1_sample_factory", 1.0f);
    setParam (source, "osc1_sample_loop", 1.0f);
    setParam (source, "osc1_chord", 4.0f);
    setParam (source, "sub_mode", 1.0f);
    setParam (source, "f1_cutoff", 1234.0f);
    source.setLfoCustomPoint (0, 3, 0.77f);

    check (source.loadUserSample (1, sampleFile), "round-trip source loads a user sample");

    const auto cutoffSource = readParam (source, "f1_cutoff");
    const auto chordSource = readParam (source, "osc1_chord");

    juce::MemoryBlock state;
    source.getStateInformation (state);

    IlanaSynthAudioProcessor loaded;
    loaded.prepareToPlay (48000.0, 256);
    loaded.setStateInformation (state.getData(), (int) state.getSize());

    loaded.flushAsyncUpdates();

    check (std::abs (readParam (loaded, "osc1_mode") - 2.0f) < 0.01f,
           "state round trip restores oscillator mode");
    check (std::abs (readParam (loaded, "osc1_chord") - chordSource) < 0.01f,
           "state round trip restores chords");
    check (std::abs (readParam (loaded, "f1_cutoff") - cutoffSource) < 0.01f,
           "state round trip restores filter cutoff (" + juce::String (readParam (loaded, "f1_cutoff"), 2) + ")");
    check (loaded.apvts.state.getProperty ("lfo1Draw").toString().contains ("0.77000"),
           "state round trip restores the drawn LFO shape");

    const auto* factory = loaded.getSampleForOsc (0);
    check (factory != nullptr && factory->name == "Metal Hit",
           "factory sample selection survives the round trip");

    const auto* userSample = loaded.getSampleForOsc (1);
    check (userSample != nullptr && userSample->name == "ilana_roundtrip_sample",
           "user sample reloads from its saved path");

    // Preset file round trip carries the same data.
    const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_roundtrip.ilanapreset");
    presetFile.deleteFile();

    check (source.savePresetToFile (presetFile), "round-trip preset saves");

    IlanaSynthAudioProcessor fromPreset;
    fromPreset.prepareToPlay (48000.0, 256);
    check (fromPreset.loadPresetFromFile (presetFile), "round-trip preset loads");

    fromPreset.flushAsyncUpdates();

    check (std::abs (readParam (fromPreset, "f1_cutoff") - cutoffSource) < 0.01f,
           "preset round trip restores parameters (" + juce::String (readParam (fromPreset, "f1_cutoff"), 2) + ")");
    check (fromPreset.getSampleForOsc (1) != nullptr,
           "preset round trip restores the user sample");

    presetFile.deleteFile();
    sampleFile.deleteFile();
}

void runSoakTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Random random (0x1a2b3c);
    processor.randomizeFxChain();

    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        if (auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_bypass"))
            parameter->setValueNotifyingHost (0.0f);
    }

    const auto setRandom = [&processor, &random] (const juce::String& id)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (random.nextFloat());
    };

    const auto presetCount = (int) Presets::getFactoryPresets().size();
    constexpr int totalBlocks = 2400; // ~26 seconds of audio
    auto note = 48;
    auto finite = true;
    auto peak = 0.0f;

    for (int block = 0; block < totalBlocks; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();

        juce::MidiBuffer midi;

        if (block % 16 == 0)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        if (block % 16 == 1)
        {
            note = 33 + random.nextInt (50);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (40 + random.nextInt (80))), 0);
        }

        if (block % 41 == 0)
        {
            for (const auto* id : { "osc1_frame", "osc2_frame", "sub_frame", "f1_cutoff", "f1_reso",
                                    "res_amount", "fx_delay_time", "fx_delay_feedback", "fx_feedback_amount",
                                    "fx_reverb_size", "amp_sustain", "unison_random", "voice_spread",
                                    "osc1_mode", "osc1_sample_factory", "osc1_sample_start", "osc1_sample_end",
                                    "mod1_src", "mod1_dst", "mod2_dst", "mod3_dst", "mod4_dst" })
                setRandom (id);
        }

        if (block % 200 == 0)
        {
            if (((block / 200) % 2) == 0)
                processor.randomizeFxChain();
            else if (presetCount > 0)
                processor.loadFactoryPreset (random.nextInt (presetCount));
        }

        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
            {
                const auto value = buffer.getSample (channel, i);

                if (! std::isfinite (value))
                    finite = false;

                peak = juce::jmax (peak, std::abs (value));
            }
    }

    check (finite && peak > 0.01f && peak < 4.0f,
           "soak: 26 s of preset churn, notes and FX stay finite (peak " + juce::String (peak, 3) + ")");

    processor.panic();
}

void runLegacyPresetFxTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto slotType = [&processor] (int slot)
    {
        if (const auto* value = processor.apvts.getRawParameterValue ("fx_slot" + juce::String (slot)))
            return (int) value->load();

        return 0;
    };

    const auto findPreset = [] (const juce::String& name)
    {
        const auto& presets = Presets::getFactoryPresets();

        for (int i = 0; i < (int) presets.size(); ++i)
            if (name == presets[(size_t) i].name)
                return i;

        return -1;
    };

    // What the recipes themselves load (the voicing layer re-lays some FX).
    IlanaSynthAudioProcessor::presetVoicingEnabled = false;

    processor.loadFactoryPreset (findPreset ("Rip Bass"));
    check (slotType (1) == 2, "legacy preset restores its Drive module (slot1 " + juce::String (slotType (1)) + ")");

    processor.loadFactoryPreset (findPreset ("Scream Lead"));
    check (slotType (1) == 9 && slotType (2) == 13,
           "legacy preset restores Delay and Reverb in order (slot1 " + juce::String (slotType (1))
               + ", slot2 " + juce::String (slotType (2)) + ")");

    processor.loadFactoryPreset (findPreset ("Metal Keys"));
    check (slotType (7) == 9 && slotType (10) == 13,
           "modern preset keeps its explicit slot layout");

    IlanaSynthAudioProcessor::presetVoicingEnabled = true;
    processor.panic();
}

void runLfo34Test()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setParam = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    setParam ("lfo3_shape", 0.0f);
    setParam ("lfo3_rate", 8.0f);
    setParam ("lfo4_shape", 0.0f);
    setParam ("lfo4_rate", 3.0f);
    setParam ("mod1_src", 20.0f); // LFO 3
    setParam ("mod1_dst", 2.0f);  // Osc1 Frame
    setParam ("mod1_amt", 0.6f);

    juce::AudioBuffer<float> buffer (2, 256);

    auto lfo3Moved = false;

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);

        if (std::abs (processor.getModDisplay (Mod::Destination::Osc1Frame)) > 0.01f)
            lfo3Moved = true;
    }

    check (lfo3Moved, "LFO 3 modulates a destination through the matrix");

    processor.panic();

    // Drawn shapes persist per LFO for all four.
    processor.setLfoCustomPoint (3, 5, 0.66f);
    check (std::abs (processor.getLfoCustomPoint (3, 5) - 0.66f) < 0.001f,
           "LFO 4 custom draw points store and read back");

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    IlanaSynthAudioProcessor reloaded;
    reloaded.prepareToPlay (48000.0, 256);
    reloaded.setStateInformation (state.getData(), (int) state.getSize());
    reloaded.flushAsyncUpdates();

    check (std::abs (reloaded.getLfoCustomPoint (3, 5) - 0.66f) < 0.001f,
           "LFO 4 draw curve survives a state round trip");
}

void runOversamplingTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto render = [&processor] (int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        auto finite = true;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto value = buffer.getSample (channel, i);

                    if (! std::isfinite (value))
                        finite = false;

                    peak = juce::jmax (peak, std::abs (value));
                }
        }

        return std::make_pair (finite, peak);
    };

    if (auto* parameter = processor.apvts.getParameter ("oversampling"))
        parameter->setValueNotifyingHost (1.0f);

    processor.flushAsyncUpdates();
    check (processor.isOversampling(), "2x oversampling engages");

    const auto [onFinite, onPeak] = render (40);
    check (onFinite && onPeak > 0.01f && onPeak < 4.0f,
           "oversampled render is finite and audible (peak " + juce::String (onPeak, 3) + ")");

    if (auto* parameter = processor.apvts.getParameter ("os_factor"))
        parameter->setValueNotifyingHost (1.0f);

    processor.flushAsyncUpdates();
    check (processor.getOversamplingFactor() == 4, "4x oversampling engages");

    const auto [fourFinite, fourPeak] = render (40);
    check (fourFinite && fourPeak > 0.01f && fourPeak < 4.0f,
           "4x oversampled render is finite and audible (peak " + juce::String (fourPeak, 3) + ")");

    if (auto* parameter = processor.apvts.getParameter ("oversampling"))
        parameter->setValueNotifyingHost (0.0f);

    processor.flushAsyncUpdates();
    check (! processor.isOversampling(), "oversampling toggles back off");

    const auto [offFinite, offPeak] = render (40);
    check (offFinite && offPeak > 0.01f && offPeak < 4.0f,
           "render after toggling oversampling off is fine (peak " + juce::String (offPeak, 3) + ")");

    processor.panic();
}
// Adding a classic module to an empty slot from the rack menu must make it
// audible without also hunting for the module's own ON switch.
void runFxSlotAssignTest()
{
    const auto renderTail = [] (bool withReverb)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        if (withReverb)
            processor.assignFxSlot (1, 13);

        auto tailEnergy = 0.0;

        for (int block = 0; block < 180; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            else if (block == 25)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            processor.processBlock (buffer, midi);

            // ~1 s after the note off, well past the 0.25 s amp release.
            if (block >= 120)
                for (int i = 0; i < 512; ++i)
                    tailEnergy += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }

        return tailEnergy;
    };

    const auto dry = renderTail (false);
    const auto wet = renderTail (true);

    check (dry < 1.0e-6 && wet > dry + 1.0e-4,
           "reverb added to a slot is audible straight away (dry tail " + juce::String (dry, 8)
               + ", wet tail " + juce::String (wet, 6) + ")");

    IlanaSynthAudioProcessor processor;
    processor.assignFxSlot (2, 10);
    check (processor.apvts.getRawParameterValue ("fx_stutter_on")->load() < 0.5f,
           "adding Stutter leaves its momentary trigger off");
}

void runPresetNameTest()
{
    IlanaSynthAudioProcessor processor;
    const auto names = processor.getFactoryPresetNames();
    processor.loadFactoryPreset (3);
    check (processor.getCurrentPresetName() == names[3], "factory preset load records its name");

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    IlanaSynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (restored.getCurrentPresetName() == names[3],
           "preset name survives a host state round trip ('" + restored.getCurrentPresetName() + "')");
}
