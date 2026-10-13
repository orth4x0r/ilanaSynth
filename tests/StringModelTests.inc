void runKarplusStrongTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (8));

    const auto renderString = [&table] (float decay, float damping, int excite, float sustain)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].stringMode = true;
        p.oscillators[0].stringExcite = excite;
        p.oscillators[0].stringDecay = decay;
        p.oscillators[0].stringDamping = damping;
        p.oscillators[0].stringSustain = sustain;
        p.oscillators[0].level = 0.9f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };

        voice.setParams (p);
        voice.startNote (45, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 48000);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 48000);

        std::vector<float> output ((size_t) 48000);
        auto finite = true;

        for (int i = 0; i < 48000; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto [burst, burstFinite] = renderString (0.75f, 0.35f, 0, 0.0f);
    const auto [sustained, sustainedFinite] = renderString (0.9f, 0.2f, 1, 0.5f);

    const auto rmsOf = [] (const std::vector<float>& values, int start, int count)
    {
        auto sum = 0.0;

        for (int i = start; i < start + count; ++i)
            sum += (double) values[(size_t) i] * (double) values[(size_t) i];

        return std::sqrt (sum / (double) count);
    };

    const auto burstEarly = rmsOf (burst, 2400, 4800);
    const auto burstLate = rmsOf (burst, 43200, 4800);

    check (burstFinite && burstEarly > 0.001f, "KS burst sounds (early rms " + juce::String (burstEarly, 5) + ")");
    check (burstLate < burstEarly * 0.9f,
           "KS burst decays (early " + juce::String (burstEarly, 5) + " late " + juce::String (burstLate, 5) + ")");

    const auto sustainedLate = rmsOf (sustained, 43200, 4800);

    check (sustainedFinite && sustainedLate > 0.001f,
           "KS sustained excitation keeps ringing (late rms " + juce::String (sustainedLate, 5) + ")");
}

void runPhysicalStringTest()
{
    const auto render = [] (float stiffness, float pickup, float excitePos,
                            float hardness, float pickPos, bool slap)
    {
        KarplusStrong string (12345);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.12f, 0.99f);
        string.setPhysicalParams (stiffness, pickup, excitePos, hardness, pickPos, slap);
        string.trigger (1.0f);

        std::vector<float> samples (24000);
        for (auto& sample : samples)
            sample = string.process();
        return samples;
    };

    const auto legacy = render (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    const auto stiff = render (0.74f, 0.0f, 0.0f, 1.0f, 0.0f, false); // allpass -0.7: the partials sit well inside the search window
    const auto pickup = render (0.0f, 1.0f, 0.0f, 1.0f, 0.0f, false); // 1 = the middle: the 2nd harmonic's notch
    const auto excitation = render (0.0f, 0.0f, 1.0f, 1.0f, 0.0f, false);
    const auto soft = render (0.0f, 0.0f, 0.0f, 0.0f, 0.25f, false);
    const auto hard = render (0.0f, 0.0f, 0.0f, 1.0f, 0.25f, false);
    const auto slapped = render (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);

    const auto powerAt = [] (const std::vector<float>& samples, double hz)
    {
        double real = 0.0, imaginary = 0.0;
        for (int i = 1500; i < 22000; ++i)
        {
            const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
            real += (double) samples[(size_t) i] * std::cos (angle);
            imaginary += (double) samples[(size_t) i] * std::sin (angle);
        }
        return real * real + imaginary * imaginary;
    };

    const auto peakNear = [&powerAt] (const std::vector<float>& samples, int centre)
    {
        auto bestFrequency = 0;
        auto bestPower = 0.0;
        for (int hz = centre - 24; hz <= centre + 24; ++hz)
        {
            const auto power = powerAt (samples, (double) hz);
            if (power > bestPower)
            {
                bestPower = power;
                bestFrequency = hz;
            }
        }
        return std::pair<int, double> { bestFrequency, bestPower };
    };

    const auto legacyEighth = peakNear (legacy, 1760).first;
    const auto stiffEighth = peakNear (stiff, 1760).first;
    check (stiffEighth > legacyEighth + 2,
           "stiffness sharpens the eighth partial (" + juce::String (legacyEighth) + " -> " + juce::String (stiffEighth) + " Hz)");

    const auto harmonicRatio = [&peakNear] (const std::vector<float>& samples)
    {
        return peakNear (samples, 440).second / juce::jmax (1.0, peakNear (samples, 220).second);
    };
    check (harmonicRatio (pickup) < harmonicRatio (legacy) * 0.5,
           "pickup position creates a second-harmonic comb notch");
    check (harmonicRatio (excitation) < harmonicRatio (legacy) * 0.7,
           "excitation position creates a second-harmonic comb notch");

    const auto attackBrightness = [] (const std::vector<float>& samples)
    {
        auto energy = 0.0;
        for (int i = 1; i < 192; ++i)
        {
            const auto difference = (double) samples[(size_t) i] - samples[(size_t) (i - 1)];
            energy += difference * difference;
        }
        return energy;
    };
    check (attackBrightness (hard) > attackBrightness (soft) * 1.5,
           "pick hardness brightens the attack");

    // Softening must grow steadily as hardness drops from the neutral 1.0,
    // with no jump just below it.
    const auto slightlySoft = render (0.0f, 0.0f, 0.0f, 0.95f, 0.0f, false);
    const auto halfSoft = render (0.0f, 0.0f, 0.0f, 0.5f, 0.0f, false);
    const auto fullySoft = render (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
    check (attackBrightness (slightlySoft) > attackBrightness (legacy) * 0.8
               && attackBrightness (legacy) > attackBrightness (halfSoft)
               && attackBrightness (halfSoft) > attackBrightness (fullySoft),
           "pick hardness softens the attack monotonically below 1.0");

    // A quiet pluck gets a quiet slap.
    const auto slapAt = [] (float velocity)
    {
        KarplusStrong string (12345);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.12f, 0.99f);
        string.setPhysicalParams (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
        string.trigger (velocity);
        auto peak = 0.0f;
        for (int i = 0; i < 192; ++i)
            peak = juce::jmax (peak, std::abs (string.process()));
        return peak;
    };
    check (slapAt (0.2f) < slapAt (1.0f) * 0.5f, "slap follows velocity");
    // What the slap adds: the difference from the same pluck without it
    // (the plain pluck's own brightness moves with the loop's read
    // fraction, so a ratio of the two sat on its threshold).
    std::vector<float> added (slapped.size());
    for (size_t i = 0; i < added.size(); ++i)
        added[i] = slapped[i] - legacy[i];
    check (attackBrightness (added) > attackBrightness (legacy) * 0.2,
           "slap adds a short attack transient (adds " + juce::String (attackBrightness (added), 2) + " to "
               + juce::String (attackBrightness (legacy), 2) + ")");

    for (const auto& samples : { stiff, pickup, excitation, hard, slapped })
    {
        auto finite = true;
        auto peak = 0.0f;
        for (auto sample : samples)
        {
            finite = finite && std::isfinite (sample);
            peak = juce::jmax (peak, std::abs (sample));
        }
        check (finite && peak <= 8.0f, "physical string output stays finite and bounded");
    }

    for (double frequency : { 15.0, 440.0, 12000.0 })
    {
        KarplusStrong extreme (12345);
        extreme.prepare (48000.0);
        extreme.setFrequency (frequency);
        extreme.setParams (KarplusStrong::Excite::Noise, 1.0f, 0.0f, 1.0f);
        extreme.setPhysicalParams (1.0f, 1.0f, 1.0f, 1.0f, 1.0f, true);
        extreme.trigger (1.0f);
        auto finite = true;
        auto peak = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            const auto sample = extreme.process();
            finite = finite && std::isfinite (sample);
            peak = juce::jmax (peak, std::abs (sample));
        }
        check (finite && peak <= 8.0f, "extreme Physical settings stay bounded at " + juce::String (frequency, 0) + " Hz");
    }
}

void runPhysicalPatchMigrationTest()
{
    IlanaSynthAudioProcessor source;
    source.prepareToPlay (48000.0, 256);
    if (auto* mode = source.apvts.getParameter ("osc1_mode"))
        mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
    if (auto* decay = source.apvts.getParameter ("osc1_string_decay"))
        decay->setValueNotifyingHost (decay->convertTo0to1 (0.83f));

    juce::MemoryBlock saved;
    source.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "legacy String patch state can be read");
    if (xml == nullptr)
        return;

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");
        if (id.contains ("_string_stiffness") || id.contains ("_string_pickup")
            || id.contains ("_string_excite_pos") || id.contains ("_string_pick_hardness")
            || id.contains ("_string_pick_pos") || id.contains ("_string_slap"))
            xml->removeChildElement (child, true);
        child = next;
    }

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    IlanaSynthAudioProcessor loaded;
    loaded.prepareToPlay (48000.0, 256);
    if (auto* stiffness = loaded.apvts.getParameter ("osc1_string_stiffness"))
        stiffness->setValueNotifyingHost (1.0f);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    const auto read = [&loaded] (const char* id)
    {
        const auto* value = loaded.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : -1.0f;
    };
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (loaded.apvts.getParameter ("osc1_mode"));
    check (choice != nullptr && choice->getIndex() == 1 && choice->getAllValueStrings()[1] == "Physical",
           "old String mode loads at the Physical choice index");
    check (std::abs (read ("osc1_string_decay") - 0.83f) < 0.01f,
           "old String settings survive the mode rename");
    check (read ("osc1_string_stiffness") == 0.0f && read ("osc1_string_pickup") == 0.0f
           && read ("osc1_string_excite_pos") == 0.0f && read ("osc1_string_pick_hardness") == 1.0f
           && read ("osc1_string_pick_pos") == 0.0f && read ("osc1_string_slap") == 0.0f,
           "old String patch receives legacy-sounding Physical defaults");
}

void runFactoryTableContentTest()
{
    for (int tableIndex = 0; tableIndex < TableFactory::getNumFactoryTables(); ++tableIndex)
    {
        Wavetable table;
        table.buildFromFrames (TableFactory::generate (tableIndex));

        auto peak = 0.0f;

        for (const auto frame : { 0, 16, 32, 63 })
        {
            const auto* data = table.getFrameData (0, frame);

            for (int i = 1; i <= Wavetable::frameSize; ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }

        check (peak > 0.05f, "factory table '" + TableFactory::getFactoryTableNames()[tableIndex]
                                 + "' has content (peak " + juce::String (peak, 4) + ")");
    }
}
void runWeirdDspTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    const auto renderVoice = [&table] (const std::function<void (VoiceParams&)>& configure)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].frame = 0.4f;
        p.oscillators[0].level = 0.8f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        configure (p);

        voice.setParams (p);
        voice.startNote (45, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        std::vector<float> output ((size_t) 8192);
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto [plain, plainFinite] = renderVoice ([] (VoiceParams&) {});

    const auto [resonated, resFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.resonatorOn = true;
        p.resonatorAmount = 0.9f;
        p.resonatorDecay = 0.85f;
        p.resonatorKeytrack = 1.0f;
    });

    const auto [fmFiltered, fmFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.filter1.cutoffHz = 400.0f;
        p.filter1.resonance = 0.3f;
        p.filter1Fm = 0.8f;
    });

    const auto [chorded, chordFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.oscillators[0].unison = 4;
        p.oscillators[0].detuneCents = 5.0f;
        p.oscillators[0].chord = 4;
    });

    const auto difference = [&plain] (const std::vector<float>& other)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < plain.size(); ++i)
        {
            const auto d = (double) other[i] - (double) plain[i];
            sum += d * d;
        }

        return std::sqrt (sum / (double) plain.size());
    };

    check (plainFinite && difference (resonated) > 0.01,
           "resonator changes the voice (rms diff " + juce::String (difference (resonated), 5) + ")");
    check (fmFinite && difference (fmFiltered) > 0.01,
           "audio-rate filter FM changes the voice (rms diff " + juce::String (difference (fmFiltered), 5) + ")");
    check (chordFinite && difference (chorded) > 0.01,
           "unison chord stacking changes the voice (rms diff " + juce::String (difference (chorded), 5) + ")");

    // Pitch shifter: 500 Hz sine shifted +12 semitones should read ~1000 Hz
    {
        GranularPitchShift shifter;
        shifter.prepare (48000.0);
        shifter.setRatio (2.0);

        constexpr int renderSize = 48000;
        std::vector<float> output ((size_t) renderSize);

        for (int i = 0; i < renderSize; ++i)
        {
            shifter.push ((float) std::sin (juce::MathConstants<double>::twoPi * 500.0 * (double) i / 48000.0));
            output[(size_t) i] = shifter.process();
        }

        auto crossings = 0;
        const auto start = 20000;
        const auto end = 44000;

        for (int i = start + 1; i < end; ++i)
            if (output[(size_t) (i - 1)] < 0.0f && output[(size_t) i] >= 0.0f)
                ++crossings;

        const auto measured = (double) crossings / ((double) (end - start) / 48000.0);
        check (std::abs (measured - 1000.0) < 60.0,
               "granular pitch shift +12 st: 500 Hz -> " + juce::String (measured, 1) + " Hz");

        auto silentRun = 0;
        auto worstSilentRun = 0;

        for (int i = 20000; i < 44000; ++i)
        {
            if (std::abs (output[(size_t) i]) < 0.01f)
            {
                ++silentRun;
                worstSilentRun = juce::jmax (worstSilentRun, silentRun);
            }
            else
            {
                silentRun = 0;
            }
        }

        check (worstSilentRun < 240, "granular pitch shift has no dropouts (worst run "
                                         + juce::String (worstSilentRun) + " samples)");
        check (shifter.getLatencySamples() <= 48000.0 * 0.035,
               "pitch shifter latency is short (" + juce::String (shifter.getLatencySamples() / 48000.0 * 1000.0, 1) + " ms)");
    }

    // Spectral freeze: noise in, frozen output must be finite and audible
    {
        SpectralFreeze freezeL;
        freezeL.prepare (48000.0);

        std::vector<float> left ((size_t) 24000);

        for (int i = 0; i < 24000; ++i)
            left[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / 48000.0);

        freezeL.process (left.data(), 12000, false, 0.0f);
        freezeL.process (left.data() + 12000, 12000, true, 1.0f);

        auto finite = true;
        auto rms = 0.0;

        for (int i = 12000; i < 24000; ++i)
        {
            finite = finite && std::isfinite (left[(size_t) i]);
            rms += (double) left[(size_t) i] * (double) left[(size_t) i];
        }

        rms = std::sqrt (rms / 12000.0);
        check (finite && rms > 0.001, "spectral freeze holds output (rms " + juce::String (rms, 5) + ")");
    }

    // Granular smear: sustained tone, output finite and audible
    {
        GranularSmear smearL;
        smearL.prepare (48000.0);
        smearL.setParams (120.0f, 12.0f);

        auto finite = true;
        auto rms = 0.0;
        auto count = 0;

        for (int i = 0; i < 48000; ++i)
        {
            const auto input = (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / 48000.0);
            smearL.push (input);

            if (i > 16000)
            {
                const auto wet = smearL.process();
                finite = finite && std::isfinite (wet);
                rms += (double) wet * (double) wet;
                ++count;
            }
        }

        rms = std::sqrt (rms / juce::jmax (1, count));
        check (finite && rms > 0.001, "granular smear produces output (rms " + juce::String (rms, 5) + ")");
    }
}

void runSampleRangeTest()
{
    // Step 30: the zoom window and the typed START / END positions.
    using namespace SampleRange;
    const auto near = [] (double a, double b) { return std::abs (a - b) < 1.0e-9; };

    check (near (*parsePosition ("500", 44100.0, 44100), 0.5), "a plain number is milliseconds");
    check (near (*parsePosition ("500 ms", 44100.0, 44100), *parsePosition ("500", 44100.0, 44100)), "ms is the default unit");
    check (near (*parsePosition ("0.25 s", 44100.0, 44100), 0.25), "seconds");
    check (near (*parsePosition ("0.25s", 44100.0, 44100), 0.25), "seconds without a space");
    check (near (*parsePosition ("11025 smp", 44100.0, 44100), 0.25), "sample frames");
    check (near (*parsePosition ("22050 samples", 44100.0, 44100), 0.5), "sample frames, spelled out");
    check (near (*parsePosition ("5 s", 44100.0, 44100), 1.0), "a position past the end clamps to the end");
    check (! parsePosition ("", 44100.0, 44100).has_value(), "empty text is no position");
    check (! parsePosition ("abc", 44100.0, 44100).has_value(), "words are no position");
    check (! parsePosition ("-5", 44100.0, 44100).has_value(), "a negative time is no position");
    check (! parsePosition ("1.2.3", 44100.0, 44100).has_value(), "two decimal points are no position");
    check (! parsePosition ("12 furlongs", 44100.0, 44100).has_value(), "an unknown unit is no position");
    check (std::abs (*parsePosition (formatPosition (0.37, 48000.0, 96000), 48000.0, 96000) - 0.37) < 1.0e-4, "a formatted position reads back");

    View view;
    check (view.isFull(), "the view starts on the whole sample");
    const auto in = zoomed (view, 0.25, 4.0, 44100);
    check (near (in.span(), 0.25) && near (in.fromView (0.25), 0.25), "zooming in 4x about a quarter keeps that point put");
    const auto edge = zoomed (view, 1.0, 8.0, 44100);
    check (near (edge.hi, 1.0) && near (edge.span(), 0.125), "zooming about the right edge stays inside the sample");
    const auto deep = zoomed (view, 0.5, 1.0e9, 44100);
    check (near (deep.span(), 32.0 / 44100.0), "the window never gets under 32 samples");
    const auto out = zoomed (in, 0.5, 0.001, 44100);
    check (out.isFull(), "zooming far out fits the whole sample again");
    const auto slid = panned (in, 100.0);
    check (near (slid.hi, 1.0) && near (slid.span(), in.span()), "panning stops at the end");
    const auto home = panned (in, -100.0);
    check (near (home.lo, 0.0), "panning stops at the start");
    const auto middle = centred (in, 0.9);
    check (std::abs ((middle.lo + middle.hi) * 0.5 - 0.9) < 0.07 && middle.hi <= 1.0 + 1.0e-12, "centring on a position moves the window to it");
}

void runSampleOscTest()
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleLength = 4800;

    auto sample = std::make_shared<SampleData>();
    sample->sampleRate = sampleRate;
    sample->name = "test";

    sample->buffer.setSize (1, sampleLength);

    for (int i = 0; i < sampleLength; ++i)
        sample->buffer.setSample (0, i, 0.9f * std::sin (juce::MathConstants<float>::twoPi * 261.6255653f
                                                         * (float) i / (float) sampleRate));

    const auto renderVoice = [&sample] (int note, int numSamples,
                                        const std::function<void (VoiceParams&)>& configure)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].sampleTuned = true;
        p.oscillators[0].level = 0.9f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        configure (p);

        voice.setParams (p);
        voice.startNote (note, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, numSamples);

        std::vector<float> output ((size_t) numSamples);
        auto finite = true;

        for (int i = 0; i < numSamples; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto peakOf = [] (const std::vector<float>& data, int from, int to)
    {
        auto peak = 0.0f;

        for (int i = from; i < juce::jmin (to, (int) data.size()); ++i)
            peak = juce::jmax (peak, std::abs (data[(size_t) i]));

        return peak;
    };

    const auto [forward, forwardFinite] = renderVoice (60, 8192, [] (VoiceParams&) {});
    check (forwardFinite && peakOf (forward, 0, 4096) > 0.2f,
           "sample osc renders audio (peak " + juce::String (peakOf (forward, 0, 4096), 4) + ")");

    const auto tunedFrequency = fundamentalOf (forward, sampleRate);
    check (std::abs (tunedFrequency - 261.63) < 8.0,
           "tuned sample at C4 plays at 261.6 Hz (measured " + juce::String (tunedFrequency, 2) + ")");

    const auto [octave, octaveFinite] = renderVoice (72, 8192, [] (VoiceParams&) {});
    const auto octaveFrequency = fundamentalOf (octave, sampleRate);
    check (octaveFinite && std::abs (octaveFrequency - 523.25) < 16.0,
           "tuned sample follows pitch (C5 measured " + juce::String (octaveFrequency, 2) + ")");

    const auto [untuned, untunedFinite] = renderVoice (72, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleTuned = false;
    });
    const auto untunedFrequency = fundamentalOf (untuned, sampleRate);
    check (untunedFinite && std::abs (untunedFrequency - 261.63) < 8.0,
           "untuned sample ignores note pitch (measured " + juce::String (untunedFrequency, 2) + ")");

    const auto [oneShot, oneShotFinite] = renderVoice (60, 16384, [] (VoiceParams&) {});
    check (oneShotFinite && peakOf (oneShot, 8000, 16384) < 0.001f,
           "one-shot stops after the sample ends (tail peak " + juce::String (peakOf (oneShot, 8000, 16384), 5) + ")");

    const auto [looped, loopedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleLoop = true;
    });
    check (loopedFinite && peakOf (looped, 8000, 16384) > 0.2f,
           "looped sample keeps playing (tail peak " + juce::String (peakOf (looped, 8000, 16384), 4) + ")");

    const auto [reversed, reversedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleReverse = true;
        p.oscillators[0].sampleLoop = true;
    });
    check (reversedFinite && peakOf (reversed, 0, 4096) > 0.2f && peakOf (reversed, 8000, 16384) > 0.2f,
           "reverse loop plays and wraps (peak " + juce::String (peakOf (reversed, 8000, 16384), 4) + ")");

    const auto [region, regionFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleStart = 0.5f;
        p.oscillators[0].sampleEnd = 1.0f;
    });
    check (regionFinite && peakOf (region, 0, 4096) > 0.2f,
           "start/end region renders audio (peak " + juce::String (peakOf (region, 0, 4096), 4) + ")");

    const auto [faded, fadedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleFadeIn = 0.5f;
        p.oscillators[0].sampleFadeOut = 0.5f;
    });
    check (fadedFinite && std::abs (faded[0]) < 0.02f && peakOf (faded, 0, 4096) > 0.1f,
           "fade in/out attenuate the region edges (first " + juce::String (std::abs (faded[0]), 5) + ")");

    const auto [collapsed, collapsedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleStart = 0.7f;
        p.oscillators[0].sampleEnd = 0.2f;
    });
    check (collapsedFinite, "collapsed start/end region stays finite");

    {
        SamplePlayer player;
        player.prepare (sampleRate);

        SamplePlayer::Params sp;
        sp.sample = sample.get();
        player.setParams (sp);
        player.trigger();

        float l = 0.0f;
        float r = 0.0f;
        player.process (l, r);

        auto other = std::make_shared<SampleData>();
        other->sampleRate = sampleRate;
        other->buffer.setSize (1, sampleLength);

        for (int i = 0; i < sampleLength; ++i)
            other->buffer.setSample (0, i, 0.8f * std::sin (juce::MathConstants<float>::twoPi * 523.25f
                                                             * (float) i / (float) sampleRate));

        sp.sample = other.get();
        player.setParams (sp);
        check (player.isActive(), "SamplePlayer retriggers when a new sample is dropped in");

        auto peak = 0.0f;

        for (int i = 0; i < 512; ++i)
        {
            player.process (l, r);
            peak = juce::jmax (peak, std::abs (l));
        }

        check (peak > 0.05f, "dropped sample plays without a new note-on");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = nullptr;
        p.oscillators[0].level = 0.9f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 2048);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 2048);

        p.oscillators[0].sample = sample.get();
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;

        for (int i = 0; i < 8192; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        check (peak > 0.05f, "sample dropped mid-note onto an empty sample osc starts playing (peak "
                                 + juce::String (peak, 4) + ")");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].level = 0.9f;
        p.oscillatorEnabled[0] = false;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.oscillatorEnabled[0] = true;
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        check (finite && peak > 0.05f, "sample osc turned on mid-note starts playing (peak "
                                          + juce::String (peak, 4) + ")");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].level = 0.9f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.oscillators[0].unison = 4;
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        check (finite && peak > 0.05f, "unison raised mid-note in sample mode stays audible (peak "
                                          + juce::String (peak, 4) + ")");
    }
}

void runPresetSanityTest()
{
    const auto& presets = Presets::getFactoryPresets();
    const auto categories = Presets::getFactoryPresetCategories();

    check (! presets.empty(), "factory presets exist (" + juce::String ((int) presets.size()) + ")");
    check ((int) presets.size() == categories.size(),
           "preset and category counts match (" + juce::String ((int) presets.size())
               + " vs " + juce::String (categories.size()) + ")");

    juce::StringArray names;
    auto duplicates = false;
    auto emptyIds = 0;

    for (const auto& preset : presets)
    {
        if (names.contains (preset.name))
            duplicates = true;

        names.add (preset.name);

        for (const auto& value : preset.values)
        {
            if (value.id == nullptr || *value.id == 0)
                ++emptyIds;
        }
    }

    check (! duplicates, "preset names are unique");
    check (emptyIds == 0, "preset entries have valid parameter ids");
}
void runOsc2Test()
{
    Wavetable sineTable;
    sineTable.buildFromFrames (TableFactory::generate (8));

    const auto renderOsc2 = [&sineTable] (int semitones, bool stringMode)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &sineTable;
        p.oscillators[0].level = 0.0f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &sineTable;
        p.oscillators[1].level = 0.9f;
        p.oscillators[1].semitones = (double) semitones;
        p.oscillators[1].stringMode = stringMode;
        p.oscillators[1].stringExcite = 0;
        p.oscillators[1].stringDecay = 0.8f;
        p.oscillators[1].stringDamping = 0.25f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };

        voice.setParams (p);
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 24000);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 24000);

        std::vector<float> output ((size_t) 24000);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 24000; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (output[(size_t) i]));
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_tuple (output, peak, finite);
    };

    const auto frequencyOf = [] (const std::vector<float>& values)
    {
        auto crossings = 0;
        const auto start = 4000;
        const auto end = 20000;

        for (int i = start + 1; i < end; ++i)
            if (values[(size_t) (i - 1)] < 0.0f && values[(size_t) i] >= 0.0f)
                ++crossings;

        return (double) crossings / ((double) (end - start) / 48000.0);
    };

    const auto [plain, plainPeak, plainFinite] = renderOsc2 (0, false);
    const auto [octave, octavePeak, octaveFinite] = renderOsc2 (12, false);
    const auto [stringMode, stringPeak, stringFinite] = renderOsc2 (0, true);

    check (plainFinite && plainPeak > 0.01f,
           "osc2 alone renders audio (peak " + juce::String (plainPeak, 4) + ")");

    const auto baseFrequency = frequencyOf (plain);
    const auto octaveFrequency = frequencyOf (octave);

    check (std::abs (baseFrequency - 220.0) < 8.0,
           "osc2 plays the note pitch (measured " + juce::String (baseFrequency, 1) + " Hz at A3)");
    check (octaveFinite && std::abs (octaveFrequency - 440.0) < 12.0,
           "osc2 semitones transpose (measured " + juce::String (octaveFrequency, 1) + " Hz at +12 st)");

    check (stringFinite && stringPeak > 0.01f,
           "osc2 string mode renders audio (peak " + juce::String (stringPeak, 4) + ")");
}
void runOscLevelTest()
{
    Wavetable sineTable;
    sineTable.buildFromFrames (TableFactory::generate (8));

    const auto makeParams = [&sineTable] (float level)
    {
        VoiceParams p;
        p.oscillators[0].table = &sineTable;
        p.oscillators[0].level = 0.0f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &sineTable;
        p.oscillators[1].level = level;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        return p;
    };

    const auto peakOf = [] (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        auto peak = 0.0f;

        for (int i = start; i < start + count; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        return peak;
    };

    // Static level sweep
    const auto peakForLevel = [&] (float level)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);
        voice.setParams (makeParams (level));
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        return peakOf (buffer, 4096, 4096);
    };

    const auto loud = peakForLevel (0.9f);
    const auto quiet = peakForLevel (0.3f);
    const auto silent = peakForLevel (0.0f);

    check (loud > 1.0f && quiet > 0.25f && quiet < loud * 0.45f,
           "osc2 level scales output (0.9 -> " + juce::String (loud, 3)
               + ", 0.3 -> " + juce::String (quiet, 3) + ")");
    check (silent < 0.001f, "osc2 level zero silences the oscillator (peak " + juce::String (silent, 5) + ")");

    // Level change while the note is held
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);
        voice.setParams (makeParams (0.9f));
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 16384);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        const auto firstPeak = peakOf (buffer, 4096, 4096);

        voice.setParams (makeParams (0.15f));
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        const auto secondPeak = peakOf (buffer, 4096, 4096);

        check (firstPeak > 1.0f && secondPeak < firstPeak * 0.35f,
               "osc2 level responds while a note is held (" + juce::String (firstPeak, 3)
                   + " -> " + juce::String (secondPeak, 3) + ")");
    }
}
double fundamentalOf (const std::vector<float>& input, double sampleRate)
{
    const auto n = (int) input.size();

    if (n < 4096)
        return 0.0;

    auto mean = 0.0;

    for (const auto value : input)
        mean += value;

    mean /= (double) n;

    std::vector<float> samples ((size_t) n);

    for (int i = 0; i < n; ++i)
        samples[(size_t) i] = input[(size_t) i] - (float) mean;

    const auto correlation = [&samples, n] (int lag)
    {
        double sum = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i + lag < n; i += 4)
        {
            const auto a = (double) samples[(size_t) i];
            const auto b = (double) samples[(size_t) (i + lag)];
            sum += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        return sum / std::sqrt (energyA * energyB + 1.0e-12);
    };

    const auto minLag = juce::jmax (8, (int) (sampleRate / 1200.0));
    const auto maxLag = juce::jmin (n / 2, (int) (sampleRate / 50.0));

    std::vector<double> values ((size_t) maxLag + 2, 0.0);
    auto bestValue = -2.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        values[(size_t) lag] = correlation (lag);
        bestValue = juce::jmax (bestValue, values[(size_t) lag]);
    }

    // Prefer the shortest period that explains the signal almost as well as
    // the best lag, so exact octave ties resolve to the fundamental.
    auto bestLag = -1;

    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        const auto value = values[(size_t) lag];

        if (value >= bestValue * 0.98 && value >= values[(size_t) (lag - 1)] && value >= values[(size_t) (lag + 1)])
        {
            bestLag = lag;
            break;
        }
    }

    if (bestLag < 0)
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (values[(size_t) lag] >= bestValue - 1.0e-12)
            {
                bestLag = lag;
                break;
            }

    if (bestLag < 0)
        return 0.0;

    if (bestLag <= minLag || bestLag >= maxLag)
        return sampleRate / (double) bestLag;

    const auto left = correlation (bestLag - 1);
    const auto middle = correlation (bestLag);
    const auto right = correlation (bestLag + 1);
    const auto denominator = left - 2.0 * middle + right;

    if (std::abs (denominator) < 1.0e-12)
        return sampleRate / (double) bestLag;

    const auto offset = 0.5 * (left - right) / denominator;

    return sampleRate / ((double) bestLag + offset);
}
