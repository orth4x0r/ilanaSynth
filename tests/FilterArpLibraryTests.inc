// Phase 3: the extra filter models.
double filterToneGain (int type, double toneHz, double cutoff, double resonance, float morph, bool slope24 = true)
{
    constexpr double sampleRate = 48000.0;
    FilterUnit filter;
    filter.prepare (sampleRate);
    filter.setType (type, slope24);
    filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, cutoff, resonance, morph));

    auto in = 0.0, out = 0.0;

    for (int i = 0; i < 24000; ++i)
    {
        const auto x = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * i / sampleRate);
        const auto y = filter.process (x);

        if (i > 9600)
        {
            in += (double) x * x;
            out += (double) y * y;
        }
    }

    return std::sqrt (out / in);
}

void runExtraFilterTests()
{
    const auto names = FilterType::getNames();

    for (int type = FilterType::DiodeLow; type < FilterType::Count; ++type)
    {
        FilterUnit filter;
        filter.prepare (48000.0);
        filter.setType (type, true);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, 48000.0, 800.0, 1.0, 0.5f));

        juce::Random random (3);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 96000; ++i)
        {
            const auto out = filter.process ((random.nextFloat() * 2.0f - 1.0f) * 0.5f);
            finite = finite && std::isfinite (out);
            peak = juce::jmax (peak, std::abs (out));
        }

        check (finite && peak < 8.0f, names[type] + " stays bounded on noise at full resonance (peak "
                                          + juce::String (peak, 2) + ")");
    }

    for (const auto type : { (int) FilterType::DiodeLow, (int) FilterType::Ms20Low })
    {
        const auto low = filterToneGain (type, 100.0, 1000.0, 0.2, 0.0f);
        const auto high = filterToneGain (type, 8000.0, 1000.0, 0.2, 0.0f);
        check (low > 0.3 && high < low * 0.05, names[type] + " low-passes (100 Hz " + juce::String (low, 3)
                                                   + ", 8 kHz " + juce::String (high, 4) + ")");
    }

    // Comb +: peaks at the cutoff's harmonics, dips between them.
    const auto onHarmonic = filterToneGain (FilterType::CombPlus, 1000.0, 500.0, 0.8, 0.0f);
    const auto between = filterToneGain (FilterType::CombPlus, 750.0, 500.0, 0.8, 0.0f);
    check (onHarmonic > between * 3.0, "comb + rings at harmonics of its cutoff (" + juce::String (onHarmonic, 2)
                                           + " vs " + juce::String (between, 2) + ")");

    // Comb -: the opposite, peaks halfway between.
    const auto minusOn = filterToneGain (FilterType::CombMinus, 1000.0, 500.0, 0.8, 0.0f);
    const auto minusBetween = filterToneGain (FilterType::CombMinus, 750.0, 500.0, 0.8, 0.0f);
    check (minusBetween > minusOn * 3.0, "comb - peaks between the harmonics");

    // Morph walks from low-pass to high-pass.
    const auto morphLowAt0 = filterToneGain (FilterType::Morph, 100.0, 1000.0, 0.2, 0.0f);
    const auto morphHighAt0 = filterToneGain (FilterType::Morph, 8000.0, 1000.0, 0.2, 0.0f);
    const auto morphLowAt1 = filterToneGain (FilterType::Morph, 100.0, 1000.0, 0.2, 1.0f);
    const auto morphHighAt1 = filterToneGain (FilterType::Morph, 8000.0, 1000.0, 0.2, 1.0f);
    check (morphLowAt0 > morphHighAt0 * 20.0 && morphHighAt1 > morphLowAt1 * 20.0, "morph goes from low-pass to high-pass");

    // Formant 'A' has its first formant near 730 Hz: louder there than at 3.5 kHz.
    const auto formantPeak = filterToneGain (FilterType::Formant, 730.0, 1000.0, 0.8, 0.0f);
    const auto formantOff = filterToneGain (FilterType::Formant, 4500.0, 1000.0, 0.8, 0.0f);
    check (formantPeak > formantOff * 4.0, "formant filter peaks on the vowel's formant (" + juce::String (formantPeak, 2)
                                              + " vs " + juce::String (formantOff, 2) + ")");
}

void runFilterRoutingTest()
{
    // Filter 1 closed right down: Default and Filter 1 routes are silenced,
    // Filter 2 (serial) and No filter get through.
    const auto levelFor = [] (int route)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "f1_cutoff", 20.0f);
        setParam (processor, "f1_slope", 1.0f);
        setParam (processor, "f1_env", 0.0f);
        setParam (processor, "osc1_route", (float) route);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 60, 20, centroid);
    };

    const auto viaDefault = levelFor (FilterRoute::Default);
    const auto viaFilter1 = levelFor (FilterRoute::Filter1);
    const auto viaFilter2 = levelFor (FilterRoute::Filter2);
    const auto direct = levelFor (FilterRoute::Direct);

    check (viaDefault < 0.02f && viaFilter1 < 0.02f && viaFilter2 > 0.1f && direct > 0.1f,
           "per-oscillator filter routes (default " + juce::String (viaDefault, 3) + ", F1 " + juce::String (viaFilter1, 3)
               + ", F2 " + juce::String (viaFilter2, 3) + ", direct " + juce::String (direct, 3) + ")");
}

void runOversampledTuningTest()
{
    for (const auto factor : { 0.0f, 1.0f })
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "oversampling", 1.0f);
        setParam (processor, "os_factor", factor);
        processor.prepareToPlay (48000.0, 512);

        double centroid = 0.0;
        std::vector<float> samples;
        renderPeakAndCentroid (processor, 57, 60, centroid, &samples);
        samples.erase (samples.begin(), samples.begin() + 4800);

        const auto frequency = fundamentalOf (samples, 48000.0);
        check (std::abs (frequency - 220.0) < 0.5, juce::String (factor > 0.5f ? "4x" : "2x")
                                                       + " oversampled note is in tune (" + juce::String (frequency, 2) + " Hz)");
    }
}

void runResynthesisTest()
{
    // A 220 Hz saw that darkens over two seconds (not a whole number of
    // 2048-sample frames, so Automatic resynthesizes it).
    constexpr double sampleRate = 48000.0;
    constexpr int length = 96000 + 777;
    std::vector<float> audio ((size_t) length);
    auto phase = 0.0;
    auto smoothed = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        phase += 220.0 / sampleRate;
        phase -= std::floor (phase);
        const auto saw = (float) (2.0 * phase - 1.0);
        const auto brightness = 1.0f - 0.95f * (float) i / (float) length;
        smoothed += brightness * (saw - smoothed);
        audio[(size_t) i] = 0.5f * smoothed;
    }

    const auto period = Wavetable::detectPeriod (audio.data() + 4800, length - 4800, sampleRate);
    check (std::abs (period - sampleRate / 220.0) < 0.5, "pitch detection finds a 220 Hz period ("
                                                            + juce::String (period, 3) + " samples)");

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana_resynth_test.wav");
    file.deleteFile();

    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (file),
                                                                                 sampleRate, 1, 24, {}, 0));
        juce::AudioBuffer<float> buffer (1, length);
        buffer.copyFrom (0, 0, audio.data(), length);
        writer->writeFromAudioSampleBuffer (buffer, 0, length);
    }

    Wavetable table;
    const auto loaded = table.loadFromFile (file, Wavetable::LoadMode::Automatic);
    file.deleteFile();

    check (loaded && table.getNumFrames() == 64, "any audio becomes a 64-frame wavetable ("
                                                     + juce::String (table.getNumFrames()) + " frames)");

    // Each frame is one cycle: harmonic n sits in bin n, and a saw's second
    // harmonic is about half the first. The last frame is darker.
    const auto harmonics = [&table] (int frame)
    {
        juce::dsp::FFT fft (11);
        std::vector<std::complex<float>> in ((size_t) Wavetable::frameSize), out ((size_t) Wavetable::frameSize);
        const auto* data = table.getFrameData (0, frame);

        for (int i = 0; i < Wavetable::frameSize; ++i)
            in[(size_t) i] = { data[i + 1], 0.0f };

        fft.perform (in.data(), out.data(), false);
        return std::array<float, 3> { std::abs (out[1]), std::abs (out[2]), std::abs (out[8]) };
    };

    const auto first = harmonics (0);
    const auto last = harmonics (63);
    const auto ratio = first[1] / juce::jmax (1.0e-6f, first[0]);

    check (ratio > 0.35f && ratio < 0.65f, "resynthesized frame keeps the saw's harmonic series (h2/h1 "
                                               + juce::String (ratio, 3) + ")");
    check (last[2] / last[0] < first[2] / first[0] * 0.7f, "later frames follow the sound getting darker");
}

void runCurveLfoTest()
{
    auto ramp = LfoCurve::preset (2); // -1 to +1 straight line
    check (std::abs (ramp.valueAt (0.5)) < 1.0e-4f && std::abs (ramp.valueAt (0.25) + 0.5f) < 1.0e-4f,
           "curve evaluates a straight ramp");

    auto bent = ramp;
    bent.points[0].tension = 0.8f;
    check (bent.valueAt (0.5) < -0.5f, "positive tension makes a segment arrive late (" + juce::String (bent.valueAt (0.5), 3) + ")");

    const auto parsed = LfoCurve::fromString (bent.toString());
    check (parsed.points.size() == 2 && std::abs (parsed.valueAt (0.5) - bent.valueAt (0.5)) < 1.0e-3f,
           "curve survives a text round trip");

    // Session round trip.
    IlanaSynthAudioProcessor processor;
    processor.setLfoCurve (2, LfoCurve::preset (6));
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    IlanaSynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (restored.getLfoCurveValue (2, 0.3) - processor.getLfoCurveValue (2, 0.3)) < 1.0e-4f,
           "LFO 3's curve survives a host state round trip");

    // A Curve LFO drives modulation: a flat curve at +1 opens the filter.
    const auto brightnessWith = [] (const char* curveText)
    {
        IlanaSynthAudioProcessor p;
        setParam (p, "sub_on", 0.0f);
        setParam (p, "f1_cutoff", 300.0f);
        setParam (p, "f1_env", 0.0f);
        setParam (p, "lfo1_shape", (float) IlanaSynthAudioProcessor::curveShape);
        setParam (p, "lfo1_retrig", 1.0f);
        setParam (p, "mod1_src", (float) Mod::Source::Lfo1);
        setParam (p, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (p, "mod1_amt", 0.6f);
        p.setLfoCurve (0, LfoCurve::fromString (curveText));
        p.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        renderPeakAndCentroid (p, 45, 30, centroid);
        return centroid;
    };

    const auto high = brightnessWith ("0,1,0;1,1,0");
    const auto low = brightnessWith ("0,-1,0;1,-1,0");
    check (high > low * 2.0, "a Curve LFO modulates per voice (centroid " + juce::String (low, 0) + " -> "
                                 + juce::String (high, 0) + " Hz)");
}
// The arp must stop when a host drops its notes without note-offs: All
// Notes Off / All Sound Off, or the transport stopping.
void runArpHostStopTests()
{
    struct TestHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (120.0);
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    const auto run = [] (int stopKind)
    {
        IlanaSynthAudioProcessor processor;
        TestHead head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 256);

        if (auto* parameter = processor.apvts.getParameter ("arp_on"))
            parameter->setValueNotifyingHost (1.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        head.playing = true;

        for (int block = 0; block < 1400; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 5);

            if (block == 100)
            {
                if (stopKind == 0)
                    midi.addEvent (juce::MidiMessage::allNotesOff (1), 30);
                else if (stopKind == 1)
                    midi.addEvent (juce::MidiMessage::allSoundOff (1), 30);
                else
                    head.playing = false;
            }

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (head.playing)
                head.ppq += 256.0 / 48000.0 * 2.0;
        }

        processor.setPlayHead (nullptr);
        return processor.getActiveVoiceCount();
    };

    check (run (0) == 0, "the arp stops on All Notes Off");
    check (run (1) == 0, "the arp stops on All Sound Off");
    check (run (2) == 0, "the arp stops when the host transport stops");

    // Chord mode honours the gate: at a short gate most of each step is silent.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("arp_on", 1.0f);
        set ("arp_mode", 7.0f);
        set ("arp_gate", 0.1f);
        set ("arp_div", 2.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        auto silentBlocks = 0, loudBlocks = 0;

        for (int block = 0; block < 600; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (block > 100)
            {
                if (processor.getActiveVoiceCount() == 0)
                    ++silentBlocks;
                else
                    ++loudBlocks;
            }
        }

        check (silentBlocks > 0, "the chord arp releases at its gate (" + juce::String (silentBlocks) + " of "
                                    + juce::String (silentBlocks + loudBlocks) + " blocks without voices)");
    }
}
// Review 6: the arp's step lanes. At their defaults a render is the same as
// without them; GATE 0 rests a step, a lower VELOCITY plays softer, ARP
// STEPS 1 repeats the first step's lane values.
void runArpLaneTests()
{
    const auto render = [] (const std::function<void (IlanaSynthAudioProcessor&)>& setUp, double& energy, int& silentBlocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("arp_on", 1.0f);
        set ("arp_div", 4.0f);
        setUp (processor);

        juce::AudioBuffer<float> buffer (2, 256);
        energy = 0.0;
        silentBlocks = 0;

        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (block > 20)
            {
                energy += buffer.getRMSLevel (0, 0, 256);
                silentBlocks += buffer.getMagnitude (0, 256) < 1.0e-5f ? 1 : 0;
            }
        }
    };
    const auto setAll = [] (IlanaSynthAudioProcessor& processor, const char* prefix, float value)
    {
        for (int step = 1; step <= 16; ++step)
            if (auto* parameter = processor.apvts.getParameter (prefix + juce::String (step)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    double plain = 0.0, defaults = 0.0, rests = 0.0, soft = 0.0, firstRest = 0.0;
    int silentPlain = 0, silentDefaults = 0, silentRests = 0, silentSoft = 0, silentFirst = 0;
    render ([] (IlanaSynthAudioProcessor&) {}, plain, silentPlain);
    render ([&setAll] (IlanaSynthAudioProcessor& processor)
            {
                setAll (processor, "arp_vel", 100.0f);
                setAll (processor, "arp_len", 1.0f);
                setAll (processor, "arp_pitch", 0.0f);
            }, defaults, silentDefaults);
    render ([&setAll] (IlanaSynthAudioProcessor& processor) { setAll (processor, "arp_len", 0.0f); }, rests, silentRests);
    render ([&setAll] (IlanaSynthAudioProcessor& processor) { setAll (processor, "arp_vel", 15.0f); }, soft, silentSoft);
    render ([] (IlanaSynthAudioProcessor& processor)
            {
                processor.apvts.getParameter ("arp_steps")->setValueNotifyingHost (processor.apvts.getParameter ("arp_steps")->convertTo0to1 (1.0f));
                processor.apvts.getParameter ("arp_len1")->setValueNotifyingHost (0.0f);
            }, firstRest, silentFirst);

    check (plain > 0.1 && std::abs (plain - defaults) < 1.0e-9 * plain,
           "the arp's lanes at their defaults render the same as before (" + juce::String (plain, 4) + ")");
    check (rests < plain * 1.0e-3, "GATE 0 on every arp step rests them all (" + juce::String (rests, 6) + ")");
    check (soft < plain * 0.9 && soft > 0.0, "a low arp VELOCITY lane plays softer (" + juce::String (soft / plain, 3) + " of the level)");
    check (firstRest < plain * 1.0e-3, "ARP STEPS 1 repeats step 1's lane values: its rest silences every step");
}

// Releasing every key must silence the Scale Random arpeggiator, including
// with note spray, chords, high notes and releases mid-step.
void runScaleRandomReleaseTest()
{
    const auto run = [] (const juce::String& presetName, std::initializer_list<int> notes, int octaves, int holdBlocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto names = processor.getFactoryPresetNames();

        if (presetName.isNotEmpty())
            processor.loadFactoryPreset (names.indexOf (presetName));

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("arp_on", 1.0f);
        set ("arp_mode", 8.0f);
        set ("arp_octaves", (float) octaves);

        if (processor.apvts.getRawParameterValue ("gen_scale")->load() < 0.5f)
            set ("gen_scale", 1.0f);

        juce::AudioBuffer<float> buffer (2, 256);

        for (int block = 0; block < holdBlocks + 1200; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            if (block == holdBlocks)
                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 17);

            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return processor.getActiveVoiceCount();
    };

    for (const auto* preset : { "Scale Walker", "Blues Machine", "Hirajoshi Spray", "" })
        for (int hold : { 3, 40, 187 })
        {
            const auto voices = run (preset, { 60, 64, 67 }, 2, hold);
            check (voices == 0, "Scale Random stops after release (" + juce::String (preset[0] != 0 ? preset : "Init")
                                    + ", hold " + juce::String (hold) + " blocks, " + juce::String (voices) + " voices left)");
        }

    check (run ("", { 110, 118 }, 4, 60) == 0, "Scale Random stops after release near the top of the keyboard");

    // Played like a keyboardist: overlapping legato, fast repeats of one key,
    // note-offs sent as velocity-0 note-ons, and a non-default channel.
    struct Event { int block, sample, note; bool on; int channel; bool zeroVelocityOff; };

    const auto play = [] (const juce::String& presetName, const std::vector<Event>& events)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto names = processor.getFactoryPresetNames();

        if (presetName.isNotEmpty())
            processor.loadFactoryPreset (names.indexOf (presetName));

        if (auto* parameter = processor.apvts.getParameter ("arp_on"))
            parameter->setValueNotifyingHost (1.0f);

        if (auto* parameter = processor.apvts.getParameter ("arp_mode"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (8.0f));

        juce::AudioBuffer<float> buffer (2, 256);
        auto lastBlock = 0;

        for (const auto& event : events)
            lastBlock = juce::jmax (lastBlock, event.block);

        for (int block = 0; block <= lastBlock + 1200; ++block)
        {
            juce::MidiBuffer midi;

            for (const auto& event : events)
                if (event.block == block)
                    midi.addEvent (event.on ? juce::MidiMessage::noteOn (event.channel, event.note, (juce::uint8) 90)
                                            : (event.zeroVelocityOff ? juce::MidiMessage::noteOn (event.channel, event.note, (juce::uint8) 0)
                                                                     : juce::MidiMessage::noteOff (event.channel, event.note)),
                                   event.sample);

            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return processor.getActiveVoiceCount();
    };

    std::vector<Event> legato, repeats, zeroOffs, channelTwo;

    for (int i = 0; i < 12; ++i)
    {
        const auto note = 55 + (i * 5) % 17;
        legato.push_back ({ i * 20, 30, note, true, 1, false });
        legato.push_back ({ i * 20 + 27, 10, note, false, 1, false });   // released after the next key goes down
        repeats.push_back ({ i * 3, 0, 62, true, 1, false });
        repeats.push_back ({ i * 3 + 1, 200, 62, false, 1, false });
        repeats.push_back ({ i * 3 + 1, 201, 62, true, 1, false });     // re-pressed in the same block
        repeats.push_back ({ i * 3 + 2, 5, 62, false, 1, false });
        zeroOffs.push_back ({ i * 9, 0, 48 + i, true, 1, false });
        zeroOffs.push_back ({ i * 9 + 4, 100, 48 + i, false, 1, true });
        channelTwo.push_back ({ i * 9, 0, 60 + i, true, 2, false });
        channelTwo.push_back ({ i * 9 + 4, 100, 60 + i, false, 2, false });
    }

    for (const auto* preset : { "Scale Walker", "Blues Machine", "Hirajoshi Spray" })
    {
        const juce::String name (preset);
        check (play (name, legato) == 0, "Scale Random stops after overlapping legato (" + name + ")");
        check (play (name, repeats) == 0, "Scale Random stops after fast repeats of one key (" + name + ")");
        check (play (name, zeroOffs) == 0, "Scale Random stops after velocity-0 note-offs (" + name + ")");
        check (play (name, channelTwo) == 0, "Scale Random stops on MIDI channel 2 (" + name + ")");
    }

    // The on-screen keyboard: a fast glissando queues several note-offs
    // between two audio blocks, and none may be lost.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Scale Walker"));
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer noMidi;

        const auto runBlocks = [&] (int count)
        {
            for (int i = 0; i < count; ++i)
            {
                buffer.clear();
                noMidi.clear();
                processor.processBlock (buffer, noMidi);
            }
        };

        // Hold a key long enough to sound, then drag across four more keys
        // faster than one block and let go.
        processor.triggerPreviewNote (60, true, 0.8f);
        runBlocks (40);

        for (int note = 61; note < 65; ++note)
        {
            processor.triggerPreviewNote (note - 1, false);
            processor.triggerPreviewNote (note, true, 0.8f);
        }

        runBlocks (40);
        processor.triggerPreviewNote (64, false);
        runBlocks (1200);

        check (processor.getActiveVoiceCount() == 0, "a fast on-screen glissando leaves no stuck notes");
    }
}

} // namespace

// Every factory preset (Init aside) has four named, working macros, plays,
// and stays bounded with all macros at full.
void runFactoryLibraryTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    juce::StringArray unnamed, unmapped, badSlots, silent, unbounded, drumLevels;
    const auto categories = processor.getFactoryPresetCategories();
    std::vector<std::pair<double, juce::String>> levels;

    const auto setMacros = [&processor] (float value)
    {
        for (int m = 1; m <= 4; ++m)
            if (auto* parameter = processor.apvts.getParameter ("macro" + juce::String (m)))
                parameter->setValueNotifyingHost (value);
    };

    // rms: the loudest half second (47 blocks) of the render, so a slow
    // swell or riser is judged by the level it reaches.
    const auto render = [&processor] (int blocks, bool startNotes, double& rms)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        auto peak = 0.0f;
        auto sum = 0.0;
        auto count = 0;
        auto finite = true;
        std::vector<double> blockEnergy;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0 && startNotes)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);
            auto energy = 0.0;

            for (int channel = 0; channel < 2; ++channel)
                for (int s = 0; s < 512; ++s)
                {
                    const auto value = buffer.getSample (channel, s);
                    finite = finite && std::isfinite (value);
                    peak = juce::jmax (peak, std::abs (value));
                    sum += (double) value * (double) value;
                    energy += (double) value * (double) value;
                    ++count;
                }
            blockEnergy.push_back (energy);
        }

        constexpr int window = 47;
        rms = std::sqrt (sum / juce::jmax (1, count));
        if ((int) blockEnergy.size() >= window)
        {
            auto windowSum = 0.0, best = 0.0;
            for (size_t b = 0; b < blockEnergy.size(); ++b)
            {
                windowSum += blockEnergy[b] - (b >= (size_t) window ? blockEnergy[b - window] : 0.0);
                if (b + 1 >= (size_t) window)
                    best = juce::jmax (best, windowSum);
            }
            rms = std::sqrt (best / (window * 1024.0));
        }
        return finite ? peak : 1.0e9f;
    };

    for (int index = 1; index < names.size(); ++index)
    {
        processor.loadFactoryPreset (index);
        processor.panic();

        for (int m = 0; m < 4; ++m)
        {
            if (processor.getMacroName (m) == "Macro " + juce::String (m + 1))
                unnamed.addIfNotAlreadyThere (names[index]);

            auto mapped = false;

            for (int slot = 0; slot < Mod::maxSlots; ++slot)
            {
                const auto info = processor.readModSlot (slot);
                const auto macroSource = (Mod::Source) ((int) Mod::Source::Macro1 + m);

                if ((info.source == macroSource && info.destination != 0 && info.depth != 0.0f)
                    || (info.aux == macroSource && info.source != Mod::Source::None))
                    mapped = true;
            }

            if (! mapped)
                unmapped.addIfNotAlreadyThere (names[index] + " M" + juce::String (m + 1));
        }

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto info = processor.readModSlot (slot);

            if (info.source != Mod::Source::None && info.depth != 0.0f && info.destination == 0)
                badSlots.addIfNotAlreadyThere (names[index]);
        }

        setMacros (0.0f);
        double rms = 0.0;
        const auto peak = render (180, true, rms);

        // ilanaSynth FX patches are silent without the audio input.
        if (categories[index] == "FX Input")
        {
            setMacros (0.0f);
            continue;
        }

        if (peak < 0.001f)
            silent.add (names[index]);

        // Drums are short hits: judge them by their peak, not a held note's level.
        if (categories[index] == "Drums")
        {
            if (peak < 0.15f || peak > 2.0f)
                drumLevels.add (names[index] + " (peak " + juce::String (peak, 2) + ")");
        }
        else if (categories[index] != "DX7")
        {
            // The DX7 banks keep the cartridges' own levels (quiet FX voices
            // and all); they're imported, not voiced for this library.
            levels.push_back ({ rms, names[index] });
        }

        setMacros (1.0f);
        double fullRms = 0.0;
        const auto fullPeak = render (60, false, fullRms);

        if (peak > 8.0f || fullPeak > 8.0f)
            unbounded.add (names[index] + " (" + juce::String (juce::jmax (peak, fullPeak), 2) + ")");

        setMacros (0.0f);
    }

    check (unnamed.isEmpty(), "every factory preset names its macros (" + unnamed.joinIntoString (", ") + ")");
    check (unmapped.isEmpty(), "every factory macro is mapped (" + unmapped.joinIntoString (", ") + ")");
    check (badSlots.isEmpty(), "factory mod slots all have a destination (" + badSlots.joinIntoString (", ") + ")");
    check (silent.isEmpty(), "every factory preset makes sound (" + silent.joinIntoString (", ") + ")");
    check (drumLevels.isEmpty(), "factory drums hit at a sensible level (" + drumLevels.joinIntoString (", ") + ")");
    check (unbounded.isEmpty(), "factory presets stay bounded with macros at full (" + unbounded.joinIntoString (", ") + ")");

    // Loudness: flag presets far from the library's median level.
    auto sorted = levels;
    std::sort (sorted.begin(), sorted.end());
    const auto median = sorted[sorted.size() / 2].first;
    juce::StringArray outliers;

    for (const auto& level : levels)
    {
        const auto db = 20.0 * std::log10 (juce::jmax (1.0e-9, level.first / median));

        if (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_LEVELS", "").isNotEmpty())
            std::cout << "  LEVEL " << level.second << " " << juce::String (db, 1) << " dB" << std::endl;

        if (std::abs (db) > 14.0)
            outliers.add (level.second + " (" + juce::String (db, 1) + " dB)");
    }

    std::cout << "  library level: median rms " << median << ", quietest " << sorted.front().second
              << ", loudest " << sorted.back().second << std::endl;
    check (outliers.isEmpty(), "factory presets within 14 dB of the median level (" + outliers.joinIntoString (", ") + ")");
}

// Tape stop must not leave latency behind once it is released.
void runTapeStopLatencyTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (0);
    processor.assignFxSlot (1, 17); // TapeStop

    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_tape_stop_time", 0.3f);

    const auto run = [&processor] (int blocks, const juce::MidiBuffer& firstMidi)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi = firstMidi;

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    juce::MidiBuffer noteOn;
    noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    run (20, noteOn);
    set ("fx_tape_stop_trigger", 1.0f);
    run (40, {});                        // tape winds down
    set ("fx_tape_stop_trigger", 0.0f);
    run (60, {});                        // spins back up and rejoins
    processor.panic();
    run (200, {});                       // let the tail die

    const auto out = run (40, noteOn);
    auto onset = -1;

    for (int i = 0; i < (int) out.size(); ++i)
    {
        if (std::abs (out[(size_t) i]) > 1.0e-3f)
        {
            onset = i;
            break;
        }
    }

    check (onset >= 0 && onset < 256, "tape stop adds no latency after release (onset at sample " + juce::String (onset) + ")");
}

// Trance gate: a Custom open/closed pattern gates the sound step by step,
// each step lasting DIV (1/16 at 120 BPM = 0.125 s).
void runTranceGateTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 500);
    processor.loadFactoryPreset (0);
    processor.assignFxSlot (1, 16);

    const auto set = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_gate_div", 4.0f);      // 1/16
    set ("fx_gate_pattern", 8.0f);  // Custom
    set ("fx_gate_steps", 4.0f);
    set ("fx_gate_smooth", 0.0f);
    set ("fx_gate_mix", 1.0f);

    for (int step = 1; step <= 16; ++step)
        set ("fx_gate_step" + juce::String (step), step % 2 == 1 ? 1.0f : 0.0f);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 500);

    for (int block = 0; block < 96; ++block) // 1 s
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 500; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    const auto rmsOf = [&out] (int start, int length)
    {
        auto sum = 0.0;

        for (int i = start; i < start + length; ++i)
            sum += (double) out[(size_t) i] * out[(size_t) i];

        return std::sqrt (sum / length);
    };

    constexpr int stepLength = 6000;
    auto worstRatio = 1.0e9;

    for (int step = 1; step < 7; step += 2)
    {
        // Middles of an open step and the closed step after it.
        const auto open = rmsOf ((step - 1) * stepLength + 1500, 3000);
        const auto closed = rmsOf (step * stepLength + 1500, 3000);
        worstRatio = juce::jmin (worstRatio, open / juce::jmax (1.0e-9, closed));
    }

    check (worstRatio > 30.0, "trance gate opens and closes on 1/16 steps (open/closed ratio " + juce::String (worstRatio, 1) + ")");
}

void runGenerativeTests()
{
    // Scale snapping.
    check (Scales::quantize (61, 1, 0) == 62 && Scales::quantize (66, 1, 0) == 67 && Scales::quantize (60, 1, 0) == 60,
           "scale snap: C major sends C# up to D, F# up to G, keeps C");
    check (Scales::quantize (64, 2, 9) == 64 && Scales::quantize (61, 2, 9) == 62,
           "scale snap: A minor keeps E, moves C# to D");

    // Spray: every extra note is in the scale, released with its parent.
    NoteSpray spray;
    NoteSpray::Settings settings;
    settings.scale = 11; // minor pentatonic
    settings.root = 0;
    settings.sprayOn = true;
    settings.count = 5;
    settings.range = 12;
    settings.chance = 1.0f;

    juce::MidiBuffer in, out;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    spray.process (in, out, 512, settings);

    std::set<int> sounding;
    auto allInScale = true;

    for (const auto metadata : out)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            sounding.insert (message.getNoteNumber());
            allInScale = allInScale && Scales::contains (message.getNoteNumber(), 11, 0);
        }
    }

    check (sounding.size() >= 3 && sounding.count (60) == 1 && allInScale,
           "note spray adds scale notes around the played note (" + juce::String ((int) sounding.size()) + " notes)");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
    spray.process (in, out, 512, settings);
    std::set<int> released;

    for (const auto metadata : out)
        if (metadata.getMessage().isNoteOff())
            released.insert (metadata.getMessage().getNoteNumber());

    check (released == sounding, "releasing the played note releases every sprayed note");

    // Spread: extra notes arrive later, across blocks.
    settings.spreadSamples = 4000;
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    auto starts = 0;

    for (int block = 0; block < 10; ++block)
    {
        spray.process (in, out, 512, settings);
        in.clear();

        for (const auto metadata : out)
            if (metadata.getMessage().isNoteOn())
                ++starts;
    }

    check (starts >= 4, "spread sprayed notes start over time (" + juce::String (starts) + " starts)");

    // Scale Random arp stays in the scale and the range.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (0);

    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("arp_on", 1.0f);
    set ("arp_mode", 8.0f);
    set ("arp_div", 5.0f); // 1/32: many steps
    set ("arp_octaves", 2.0f);
    set ("gen_scale", 3.0f); // D dorian
    set ("gen_root", 2.0f);

    auto inKey = true, inRange = true;
    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 200; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    // With note 50 held, check the arp's choices over many picks.
    for (int i = 0; i < 400; ++i)
    {
        const auto note = processor.pickArpNoteForTest (8, 2);
        inKey = inKey && Scales::contains (note, 3, 2);
        inRange = inRange && note >= 50 && note <= 50 + 24 + 1;
    }

    check (inKey && inRange, "Scale Random arp picks notes in the scale and the octave range");
}
