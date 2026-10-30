namespace
{
int failures = 0;

double fundamentalOf (const std::vector<float>& input, double sampleRate);

void check (bool condition, const juce::String& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message.toStdString() << std::endl;

    if (! condition)
        ++failures;
}

// CPU budgets depend on the machine. On CI (ILANA_CI=1, shared runners whose
// speed swings run to run) a missed budget is reported but not counted;
// everywhere else it fails like any check.
void checkTiming (bool condition, const juce::String& message)
{
    if (! condition && juce::SystemStats::getEnvironmentVariable ("ILANA_CI", "").isNotEmpty())
    {
        std::cout << "TIMING (not counted on CI): " << message.toStdString() << std::endl;
        return;
    }
    check (condition, message);
}

// Route polarity (2026-10-10): Negative is Unipolar turned over, and the
// amount reads as the share of the range a route covers.
void runPolarityTests()
{
    Mod::Slot slot;
    slot.source = Mod::Source::Lfo1; // swings both ways
    slot.destination = 1;
    slot.depth = 0.5f;
    const auto unipolar = [&slot] (Mod::Polarity polarity, float value) { slot.polarity = polarity; return Mod::shape (slot, value); };
    check (std::abs (unipolar (Mod::Polarity::Unipolar, 1.0f) - 1.0f) < 1.0e-6f && std::abs (unipolar (Mod::Polarity::Unipolar, -1.0f)) < 1.0e-6f,
           "Unipolar maps an LFO's -1..1 to 0..1");
    check (std::abs (unipolar (Mod::Polarity::Negative, 1.0f) + 1.0f) < 1.0e-6f && std::abs (unipolar (Mod::Polarity::Negative, -1.0f)) < 1.0e-6f
               && std::abs (unipolar (Mod::Polarity::Negative, 0.0f) + 0.5f) < 1.0e-6f,
           "Negative maps an LFO's -1..1 to 0..-1");
    slot.source = Mod::Source::AmpEnv; // rises 0..1
    check (std::abs (unipolar (Mod::Polarity::Negative, 0.75f) + 0.75f) < 1.0e-6f, "Negative turns an envelope over");
    check (Mod::visualDepth (slot) < 0.0f && Mod::storedDepth (slot, -0.4f) > 0.0f, "a Negative route reads as a negative depth");
    check (Mod::shownPercent (0.5f, true) == 100 && Mod::shownPercent (0.5f, false) == 50 && std::abs (Mod::depthFromShownPercent (100.0f, true) - 0.5f) < 1.0e-6f,
           "a bipolar 100% is half the stored depth: the whole range");
}

void runMipmapTests()
{
    juce::dsp::FFT fft (11);

    std::vector<std::complex<float>> input ((size_t) Wavetable::frameSize);
    std::vector<std::complex<float>> output ((size_t) Wavetable::frameSize);

    const auto names = TableFactory::getFactoryTableNames();

    for (int tableIndex = 0; tableIndex < TableFactory::getNumFactoryTables(); ++tableIndex)
    {
        Wavetable table;
        table.buildFromFrames (TableFactory::generate (tableIndex));

        for (int level = 0; level < Wavetable::numLevels; ++level)
        {
            const auto maxHarmonic = juce::jmax (1, Wavetable::numHarmonics >> level);
            auto worstRatio = 0.0f;

            for (const auto frame : { 0, 16, 32, 63 })
            {
                const auto* data = table.getFrameData (level, frame);

                for (int i = 0; i < Wavetable::frameSize; ++i)
                    input[(size_t) i] = { data[i + 1], 0.0f };

                fft.perform (input.data(), output.data(), false);

                auto peak = 0.0f;
                auto above = 0.0f;

                for (int bin = 1; bin < Wavetable::frameSize / 2; ++bin)
                {
                    const auto magnitude = std::abs (output[(size_t) bin]);
                    peak = juce::jmax (peak, magnitude);

                    if (bin > maxHarmonic + 2)
                        above = juce::jmax (above, magnitude);
                }

                if (peak > 0.0f)
                    worstRatio = juce::jmax (worstRatio, above / peak);
            }

            check (worstRatio < 0.001f,
                   "mipmap " + names[tableIndex] + " level " + juce::String (level)
                       + " band-limited (above-band ratio " + juce::String (worstRatio, 6) + ")");
        }
    }
}

void runAliasTests()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 2093.0;
    constexpr int renderSize = 16384;
    constexpr int fftOrder = 14;

    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    WavetableOscillator osc;
    osc.setSampleRate (sampleRate);
    osc.setWavetable (&table);
    osc.setFrequency (frequency);
    osc.setFramePosition (0.5f);

    std::vector<float> samples ((size_t) renderSize, 0.0f);

    for (int i = 0; i < renderSize; ++i)
        samples[(size_t) i] = osc.getNextSample();

    juce::dsp::WindowingFunction<float> window ((size_t) renderSize,
                                                juce::dsp::WindowingFunction<float>::hann,
                                                true);
    window.multiplyWithWindowingTable (samples.data(), (size_t) renderSize);

    juce::dsp::FFT fft (fftOrder);

    std::vector<std::complex<float>> input ((size_t) renderSize);
    std::vector<std::complex<float>> output ((size_t) renderSize);

    for (int i = 0; i < renderSize; ++i)
        input[(size_t) i] = { samples[(size_t) i], 0.0f };

    fft.perform (input.data(), output.data(), false);

    const auto binWidth = sampleRate / (double) renderSize;

    auto peak = 0.0f;
    auto worstOffHarmonic = 0.0f;

    for (int bin = 1; bin < renderSize / 2; ++bin)
    {
        const auto magnitude = std::abs (output[(size_t) bin]);
        peak = juce::jmax (peak, magnitude);

        const auto binFrequency = (double) bin * binWidth;
        const auto nearestHarmonic = std::round (binFrequency / frequency);

        if (nearestHarmonic >= 1.0)
        {
            const auto distance = std::abs (binFrequency - nearestHarmonic * frequency);

            if (distance > 40.0)
                worstOffHarmonic = juce::jmax (worstOffHarmonic, magnitude);
        }
    }

    const auto ratio = peak > 0.0f ? worstOffHarmonic / peak : 0.0f;

    check (ratio < 0.002f,
           "oscillator alias rejection at C7 (off-harmonic ratio " + juce::String (ratio, 6) + ")");
}

void runLoaderTest()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("ilana_table_test.wav");
    file.deleteFile();

    constexpr int framesToWrite = 4;
    constexpr int frameLength = 2048;

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (file);

        check (fileStream->openedOk(), "test wav stream opened");

        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);

        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (44100.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        check (writer != nullptr, "test wav writer created");

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, framesToWrite * frameLength);

            for (int frame = 0; frame < framesToWrite; ++frame)
                for (int i = 0; i < frameLength; ++i)
                    buffer.setSample (0, frame * frameLength + i,
                                      0.5f * (float) std::sin (juce::MathConstants<double>::twoPi
                                                               * (double) i / (double) frameLength
                                                               * (double) (frame + 1) * 4.0));

            check (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()),
                   "test wav written");
            writer.reset();
        }
    }

    Wavetable table;
    check (table.loadFromFile (file), "loader accepts 4-frame 2048-sample wav");
    check (table.getNumFrames() == framesToWrite,
           "loader detected frame count (got " + juce::String (table.getNumFrames()) + ")");

    file.deleteFile();
}
void runFilterTests()
{
    Svf filter;
    filter.setSampleRate (48000.0);
    filter.setCutoff (2000.0);
    filter.setResonance (0.95);
    filter.setMode (Svf::Mode::LowPass);

    auto stable = true;
    auto peak = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        const auto input = i == 0 ? 1.0f : 0.0f;
        const auto output = filter.processSample (input);

        if (! std::isfinite (output))
        {
            stable = false;
            break;
        }

        peak = juce::jmax (peak, std::abs (output));
    }

    check (stable, "SVF stable with impulse at high resonance");
    check (peak < 20.0f, "SVF impulse response bounded (peak " + juce::String (peak, 3) + ")");

    Svf extreme;
    extreme.setSampleRate (44100.0);
    extreme.setCutoff (19900.0);
    extreme.setResonance (0.98);
    extreme.setMode (Svf::Mode::HighPass);

    auto extremePeak = 0.0f;

    for (int i = 0; i < 44100; ++i)
    {
        const auto output = extreme.processSample (std::sin ((float) i * 0.1f));
        extremePeak = juce::jmax (extremePeak, std::abs (output));
    }

    check (std::isfinite (extremePeak) && extremePeak < 50.0f,
           "SVF stable near Nyquist at max resonance (peak " + juce::String (extremePeak, 3) + ")");
}

void runPitchShiftInterpolationTest()
{
    // The Octaver's 0.5 ratio on a 3 kHz sine: with whole-sample reads every
    // sample played twice and mirrored the 1.5 kHz output up near 22.5 kHz.
    constexpr double sampleRate = 48000.0;
    constexpr int renderSize = 16384;

    GranularPitchShift shifter;
    shifter.prepare (sampleRate);
    shifter.setRatio (0.5);

    std::vector<float> samples ((size_t) renderSize, 0.0f);

    for (int i = 0; i < 8192 + renderSize; ++i)
    {
        shifter.push ((float) std::sin (juce::MathConstants<double>::twoPi * 3000.0 * (double) i / sampleRate));
        const auto out = shifter.process();

        if (i >= 8192)
            samples[(size_t) (i - 8192)] = out;
    }

    juce::dsp::WindowingFunction<float> window ((size_t) renderSize,
                                                juce::dsp::WindowingFunction<float>::hann, true);
    window.multiplyWithWindowingTable (samples.data(), (size_t) renderSize);

    std::vector<float> fftData ((size_t) renderSize * 2, 0.0f);
    std::copy (samples.begin(), samples.end(), fftData.begin());
    juce::dsp::FFT (14).performFrequencyOnlyForwardTransform (fftData.data());

    const auto binWidth = sampleRate / (double) renderSize;
    auto inBand = 0.0, image = 0.0;

    for (int bin = 1; bin < renderSize / 2; ++bin)
    {
        const auto power = (double) fftData[(size_t) bin] * (double) fftData[(size_t) bin];
        const auto hz = (double) bin * binWidth;

        if (hz < 6000.0)
            inBand += power;
        else if (hz > 12000.0)
            image += power;
    }

    const auto imageDb = 10.0 * std::log10 (image / juce::jmax (1.0e-20, inBand) + 1.0e-20);
    check (imageDb < -60.0, "pitch shift 0.5 image above 12 kHz " + juce::String (imageDb, 1) + " dB (under -60)");
}

void runVoiceSmokeTest()
{
    Wavetable table1;
    table1.buildFromFrames (TableFactory::generate (7));

    Wavetable table2;
    table2.buildFromFrames (TableFactory::generate (3));

    auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
    auto& voice = *voiceOwner;
    voice.setCurrentPlaybackSampleRate (48000.0);

    VoiceParams p;
    p.oscillators[0].table = &table1;
    p.oscillators[0].frame = 0.5f;
    p.oscillators[0].level = 0.8f;
    p.oscillators[0].unison = 4;
    p.oscillators[0].detuneCents = 20.0f;
    p.oscillators[0].spread = 0.5f;

    p.oscillatorEnabled[1] = true;
    p.oscillators[1].table = &table2;
    p.oscillators[1].frame = 0.3f;
    p.oscillators[1].level = 0.5f;
    p.oscillators[1].semitones = -12.0;
    p.oscillators[1].unison = 2;

    p.oscillators[2].table = &table1;
    p.oscillators[2].level = 0.5f;
    p.noiseLevel = 0.1f;

    p.filter1.cutoffHz = 800.0f;
    p.filter1.resonance = 0.7f;
    p.filter1.slope24 = true;
    p.filter1.drive = 3.0f;
    p.filter1.envAmount = 2.0f;

    p.filter2.type = FilterType::HighPass;
    p.filter2.cutoffHz = 100.0f;

    p.ampEnv = { 0.005f, 0.3f, 0.8f, 0.25f };
    p.filterEnv = { 0.001f, 0.4f, 0.2f, 0.3f };
    p.modEnv = { 0.01f, 0.3f, 0.5f, 0.3f };

    p.addModSlot (Mod::Source::FilterEnv, Mod::Destination::Osc1Frame, 0.5f);
    p.addModSlot (Mod::Source::Random, Mod::Destination::Osc2Pitch, 0.2f);

    voice.setParams (p);
    voice.startNote (48, 0.9f, nullptr, 8192);

    juce::AudioBuffer<float> buffer (2, 48000);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);

    auto finite = true;
    auto peak = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < 48000; ++i)
        {
            if (! std::isfinite (data[i]))
            {
                finite = false;
                break;
            }

            peak = juce::jmax (peak, std::abs (data[i]));
        }
    }

    check (finite && peak > 0.001f && peak < 4.0f,
           "voice renders finite audio (peak " + juce::String (peak, 4) + ")");

    voice.stopNote (0.5f, true);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);

    auto tailPeak = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < 48000; ++i)
            if (std::isfinite (data[i]))
                tailPeak = juce::jmax (tailPeak, std::abs (data[i]));
    }

    check (std::isfinite (tailPeak) && tailPeak < 4.0f,
           "voice release stays finite (peak " + juce::String (tailPeak, 4) + ")");
}
void runFrameModulationTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (4));

    const std::vector<float> lfo = []
    {
        std::vector<float> values ((size_t) 4096);

        for (int i = 0; i < 4096; ++i)
            values[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * 2.0 * (double) i / 48000.0);

        return values;
    }();

    const auto render = [&table, &lfo] (float frameParam, float macroValue, float modDepth,
                                        const float* lfoSource, Mod::Source source)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].frame = frameParam;
        p.oscillators[0].level = 1.0f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter1.drive = 1.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        p.macros[0] = macroValue;
        p.lfoBuffers[0] = lfoSource;
        p.addModSlot (source, Mod::Destination::Osc1Frame, modDepth);

        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 4096);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 4096);

        std::vector<float> output ((size_t) 4096);

        for (int i = 0; i < 4096; ++i)
            output[(size_t) i] = buffer.getSample (0, i);

        return output;
    };

    const auto modulated = render (0.0f, 1.0f, 1.0f, nullptr, Mod::Source::Macro1);
    const auto direct = render (1.0f, 0.0f, 0.0f, nullptr, Mod::Source::None);
    const auto lfoModulated = render (0.5f, 0.0f, 0.5f, lfo.data(), Mod::Source::Lfo1);
    const auto staticFrame = render (0.5f, 0.0f, 0.0f, nullptr, Mod::Source::None);

    const auto peakOf = [] (const std::vector<float>& values)
    {
        auto peak = 0.0f;

        for (const auto value : values)
            peak = juce::jmax (peak, std::abs (value));

        return peak;
    };

    check (peakOf (modulated) > 0.001f && peakOf (direct) > 0.001f,
           "frame modulation test renders audio (peaks " + juce::String (peakOf (modulated), 4)
               + " / " + juce::String (peakOf (direct), 4) + ")");

    auto maxDifference = 0.0f;

    for (size_t i = 0; i < modulated.size(); ++i)
        maxDifference = juce::jmax (maxDifference, std::abs (modulated[i] - direct[i]));

    check (maxDifference < 1e-5f,
           "Macro1 -> Osc1Frame at depth 1 matches direct frame position (max diff " + juce::String (maxDifference, 7) + ")");

    check (peakOf (staticFrame) > 0.001f,
           "static frame render is audible (peak " + juce::String (peakOf (staticFrame), 4) + ")");

    auto squaredDifference = 0.0;

    for (size_t i = 0; i < lfoModulated.size(); ++i)
    {
        const auto difference = (double) lfoModulated[i] - (double) staticFrame[i];
        squaredDifference += difference * difference;
    }

    const auto rmsDifference = std::sqrt (squaredDifference / (double) lfoModulated.size());

    check (rmsDifference > 0.01,
           "LFO1 -> Osc1Frame modulates the output (rms diff " + juce::String (rmsDifference, 5) + ")");
}
void runCrossModulationTest()
{
    Wavetable table1;
    table1.buildFromFrames (TableFactory::generate (7));

    Wavetable table2;
    table2.buildFromFrames (TableFactory::generate (3));

    const auto render = [&table1, &table2] (float fm, float feedback, float ring, bool sync)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table1;
        p.oscillators[0].frame = 0.4f;
        p.oscillators[0].level = 0.8f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &table2;
        p.oscillators[1].frame = 0.5f;
        p.oscillators[1].level = 0.7f;
        p.oscillators[1].semitones = 7.0;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        p.fmAmount = fm;
        p.fmFeedback = feedback;
        p.ringMod = ring;
        p.hardSync = sync;

        voice.setParams (p);
        voice.startNote (48, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        std::vector<float> output ((size_t) 8192);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            output[(size_t) i] = value;
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        return std::make_tuple (output, peak, finite);
    };

    const auto [plain, plainPeak, plainFinite] = render (0.0f, 0.0f, 0.0f, false);
    const auto [fm, fmPeak, fmFinite] = render (0.5f, 0.0f, 0.0f, false);
    const auto [feedback, fbPeak, fbFinite] = render (0.0f, 0.6f, 0.0f, false);
    const auto [ring, ringPeak, ringFinite] = render (0.0f, 0.0f, 0.9f, false);
    const auto [sync, syncPeak, syncFinite] = render (0.0f, 0.0f, 0.0f, true);

    check (plainFinite && plainPeak > 0.001f, "cross-mod baseline renders audio (peak "
                                                  + juce::String (plainPeak, 4) + ")");

    const auto rmsDifference = [&plain] (const std::vector<float>& other)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < plain.size(); ++i)
        {
            const auto difference = (double) other[i] - (double) plain[i];
            sum += difference * difference;
        }

        return std::sqrt (sum / (double) plain.size());
    };

    check (fmFinite && std::isfinite (fmPeak) && rmsDifference (fm) > 0.01,
           "FM cross-mod changes output (rms diff " + juce::String (rmsDifference (fm), 5) + ")");
    check (fbFinite && std::isfinite (fbPeak) && rmsDifference (feedback) > 0.01,
           "feedback FM changes output (rms diff " + juce::String (rmsDifference (feedback), 5) + ")");
    check (ringFinite && std::isfinite (ringPeak) && rmsDifference (ring) > 0.01,
           "ring mod changes output (rms diff " + juce::String (rmsDifference (ring), 5) + ")");
    check (syncFinite && std::isfinite (syncPeak) && rmsDifference (sync) > 0.01,
           "hard sync changes output (rms diff " + juce::String (rmsDifference (sync), 5) + ")");

    check (juce::jmax (fmPeak, juce::jmax (fbPeak, juce::jmax (ringPeak, syncPeak))) < 8.0f,
           "cross-mod output stays bounded");
}
