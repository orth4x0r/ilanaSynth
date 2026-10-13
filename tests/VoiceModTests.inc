// Phase 1: new filter models and voice modes.
void runFilterModelTests()
{
    constexpr double sampleRate = 48000.0;

    // Self-oscillation: silence in, a sustained bounded tone out, for the
    // SVF (Morph), Airwindows' Y low-pass above 0.98 and the ladder at full
    // feedback.
    for (const auto type : { (int) FilterType::Morph, (int) FilterType::LowPass, (int) FilterType::LadderLow })
    {
        FilterUnit filter;
        filter.setType (type, false);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, 1000.0, 1.0));

        auto peak = 0.0f;
        auto lateEnergy = 0.0;
        auto finite = true;

        for (int i = 0; i < (int) sampleRate * 2; ++i)
        {
            const auto input = i < 32 ? 0.5f : 0.0f;
            const auto out = filter.process (input);
            finite = finite && std::isfinite (out);
            peak = juce::jmax (peak, std::abs (out));

            if (i > (int) sampleRate)
                lateEnergy += (double) out * out;
        }

        const auto lateRms = std::sqrt (lateEnergy / sampleRate);
        check (finite && peak < 4.0f && lateRms > 0.05,
               FilterType::getNames()[type] + " self-oscillates at max resonance and stays bounded (peak "
                   + juce::String (peak, 3) + ", late rms " + juce::String (lateRms, 3) + ")");
    }

    // At low level Airwindows' Y low-pass (24 dB) is the old SVF cascade
    // (its encode and decode curves are straight there; the difference is its
    // 20 kHz Butterworths), so the presets' tone balance carries over.
    {
        FilterUnit unit;
        unit.setType (FilterType::LowPass, true);
        const auto c = FilterUnit::makeCoefficients (FilterType::LowPass, sampleRate, 800.0, 0.7);
        unit.setCoefficients (c);

        Svf a, b;
        a.setMode (Svf::Mode::LowPass);
        b.setMode (Svf::Mode::LowPass);
        a.setCoefficients (Svf::makeCoefficients (sampleRate, 800.0, 0.7));
        b.setCoefficients (Svf::makeCoefficients (sampleRate, 800.0, 0.7));

        juce::Random random (7);
        auto maxDiff = 0.0f;

        for (int i = 0; i < 4800; ++i)
        {
            const auto x = (random.nextFloat() * 2.0f - 1.0f) * 0.002f;
            maxDiff = juce::jmax (maxDiff, std::abs (unit.process (x) - b.processSample (a.processSample (x))));
        }

        check (maxDiff < 0.002f * 0.05f, "24 dB Y low-pass matches the old SVF cascade at low level (max diff "
                                            + juce::String (maxDiff / 0.002f * 100.0f, 3) + " % of the input)");
    }

    // Ladder low-pass actually low-passes and high-pass actually high-passes.
    const auto toneGain = [sampleRate] (int type, double toneHz)
    {
        FilterUnit filter;
        filter.setType (type, true);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, 1000.0, 0.2));

        auto in = 0.0, out = 0.0;

        for (int i = 0; i < 24000; ++i)
        {
            const auto x = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * i / sampleRate);
            const auto y = filter.process (x);

            if (i > 4800)
            {
                in += (double) x * x;
                out += (double) y * y;
            }
        }

        return std::sqrt (out / in);
    };

    const auto lpLow = toneGain (FilterType::LadderLow, 100.0);
    const auto lpHigh = toneGain (FilterType::LadderLow, 8000.0);
    const auto hpLow = toneGain (FilterType::LadderHigh, 100.0);
    const auto hpHigh = toneGain (FilterType::LadderHigh, 8000.0);

    check (lpLow > 0.5 && lpHigh < 0.02, "ladder LP passes lows and cuts highs (100 Hz " + juce::String (lpLow, 3)
                                             + ", 8 kHz " + juce::String (lpHigh, 4) + ")");
    check (hpHigh > 0.5 && hpLow < 0.02, "ladder HP passes highs and cuts lows (100 Hz " + juce::String (hpLow, 4)
                                             + ", 8 kHz " + juce::String (hpHigh, 3) + ")");
}

void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void runVoiceModeTests()
{
    const auto render = [] (IlanaSynthAudioProcessor& processor, juce::MidiBuffer midi, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer blockMidi;

            if (block == 0)
                blockMidi = midi;

            processor.processBlock (buffer, blockMidi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 256));
        }

        return peak;
    };

    const auto chord = []
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 10);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 20);
        return midi;
    };

    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        render (processor, chord(), 20);
        check (processor.getActiveVoiceCount() == 3, "poly mode plays a 3-note chord on 3 voices ("
                                                         + juce::String (processor.getActiveVoiceCount()) + ")");
    }

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "poly_voices", 2.0f);
        processor.prepareToPlay (48000.0, 256);
        render (processor, chord(), 20);
        check (processor.getActiveVoiceCount() <= 2, "poly voice limit of 2 is respected ("
                                                         + juce::String (processor.getActiveVoiceCount()) + ")");
    }

    for (const auto mode : { 1, 2 })
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "voice_mode", (float) mode);
        processor.prepareToPlay (48000.0, 256);
        const auto peak = render (processor, chord(), 20);
        const auto name = mode == 1 ? juce::String ("mono") : juce::String ("legato");

        check (processor.getActiveVoiceCount() == 1 && peak > 0.01f,
               name + " mode plays a chord on one voice (" + juce::String (processor.getActiveVoiceCount()) + ")");

        // Release the top note while the others are held: the voice must keep
        // sounding (falls back to 64), then go quiet once everything is up.
        juce::MidiBuffer releaseTop;
        releaseTop.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        const auto fallbackPeak = render (processor, releaseTop, 10);

        juce::MidiBuffer releaseAll;
        releaseAll.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        releaseAll.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        render (processor, releaseAll, 400);

        check (fallbackPeak > 0.01f && processor.getActiveVoiceCount() == 0,
               name + " mode falls back to held notes, then releases (fallback peak "
                   + juce::String (fallbackPeak, 3) + ")");
    }

    // Legato keeps the amp envelope running between overlapping notes; mono
    // retriggers it. Slow attack makes the difference measurable.
    const auto levelAfterSecondNote = [&render] (int mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "voice_mode", (float) mode);
        setParam (processor, "amp_attack", 1.0f);
        processor.prepareToPlay (48000.0, 256);

        juce::MidiBuffer first;
        first.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        render (processor, first, 200); // ~1.07 s: attack complete

        juce::MidiBuffer second;
        second.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
        return render (processor, second, 2);
    };

    {
        IlanaSynthAudioProcessor processor;
        processor.setMacroName (2, "Grit");
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getMacroName (2) == "Grit" && restored.getMacroName (0) == "Macro 1",
               "macro names survive a host state round trip ('" + restored.getMacroName (2) + "')");

        restored.loadFactoryPreset (0);
        check (restored.getMacroName (2) == "Macro 3", "loading Init resets macro names");

        restored.loadFactoryPreset (1);
        check (restored.getMacroName (0) == "TONE", "legacy presets get default macro names ('" + restored.getMacroName (0) + "')");
        check (restored.getPresetCategory() == "Bass", "factory preset load sets its category");

        const auto file = juce::File::createTempFile ("ilanapreset");
        restored.setPresetMeta ("Keys", "warm, vintage");
        check (restored.savePresetToFile (file), "preset with metadata saves");

        IlanaSynthAudioProcessor reloaded;
        check (reloaded.loadPresetFromFile (file) && reloaded.getPresetCategory() == "Keys"
                   && reloaded.getPresetTags() == "warm, vintage",
               "preset category and tags survive a save/load ('" + reloaded.getPresetCategory() + "')");
        file.deleteFile();
    }

    const auto legatoLevel = levelAfterSecondNote (2);
    const auto monoLevel = levelAfterSecondNote (1);

    check (legatoLevel > monoLevel * 0.9f && legatoLevel > 0.05f,
           "legato does not retrigger the envelope (legato " + juce::String (legatoLevel, 3)
               + ", mono " + juce::String (monoLevel, 3) + ")");
}

// Phase 2: warps, unison, per-voice LFOs, the extended matrix.
float renderPeakAndCentroid (IlanaSynthAudioProcessor& processor, int note, int blocks, double& centroid,
                             std::vector<float>* capture = nullptr, int velocity = 100)
{
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> samples;
    auto peak = 0.0f;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);

        processor.processBlock (buffer, midi);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 512));

        for (int i = 0; i < 512; ++i)
            samples.push_back (buffer.getSample (0, i));
    }

    // Spectral centroid of the last 8192 samples.
    constexpr int order = 13;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work ((size_t) size * 2, 0.0f);
    const auto start = samples.size() - (size_t) size;

    for (int i = 0; i < size; ++i)
        work[(size_t) i] = samples[start + (size_t) i]
                           * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));

    fft.performFrequencyOnlyForwardTransform (work.data());
    double weighted = 0.0, total = 0.0;

    for (int bin = 1; bin < size / 2; ++bin)
    {
        weighted += bin * 48000.0 / size * work[(size_t) bin];
        total += work[(size_t) bin];
    }

    centroid = total > 0.0 ? weighted / total : 0.0;

    if (capture != nullptr)
        *capture = samples;

    return peak;
}

void runWarpTests()
{
    const auto names = Warp::getNames();
    double plainCentroid = 0.0;

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 8.0f); // Sine: warps add harmonics
        setParam (processor, "f1_cutoff", 20000.0f);
        processor.prepareToPlay (48000.0, 512);
        renderPeakAndCentroid (processor, 45, 40, plainCentroid);
    }

    for (int mode = 1; mode < Warp::Count; ++mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 8.0f);
        setParam (processor, "f1_cutoff", 20000.0f);
        setParam (processor, "osc1_warp", (float) mode);
        setParam (processor, "osc1_warp_amt", 0.7f);

        if (mode == Warp::Fm || mode == Warp::Ring)
        {
            setParam (processor, "osc2_on", 1.0f);
            setParam (processor, "osc2_level", 0.01f);
            setParam (processor, "osc2_semi", 7.0f);
        }

        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);

        check (std::isfinite (peak) && peak > 0.02f && peak < 4.0f && centroid > plainCentroid * 1.15,
               "warp " + names[mode] + " adds harmonics to a sine (centroid " + juce::String (plainCentroid, 0)
                   + " -> " + juce::String (centroid, 0) + " Hz, peak " + juce::String (peak, 2) + ")");
    }
}

void runPerVoiceSpectralTest()
{
    // A patch (user) table takes spectral warps too, not only the factory tables.
    {
        const auto render = [] (bool warped)
        {
            IlanaSynthAudioProcessor processor;
            processor.setNonRealtime (true);
            processor.setUserTable (0, WavetableDoc::fromFactory (10));
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_table", (float) TableFactory::getNumFactoryTables());
            setParam (processor, "osc1_spectral", warped ? (float) SpectralWarp::LowPass : 0.0f);
            setParam (processor, "osc1_spectral_amt", 0.8f);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            renderPeakAndCentroid (processor, 45, 40, centroid);
            return centroid;
        };

        const auto plain = render (false);
        const auto warped = render (true);
        check (warped < plain * 0.9,
               "a spectral warp works on a patch table (centroid " + juce::String (plain, 0) + " -> " + juce::String (warped, 0) + " Hz)");
    }

    // A velocity route into SPEC AMT is worked out per voice: a hard note and a soft note morph differently.
    const auto render = [] (int velocity, bool routed)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 10.0f);
        setParam (processor, "osc1_spectral", (float) SpectralWarp::LowPass);
        setParam (processor, "osc1_spectral_amt", 0.0f);

        if (routed)
        {
            setParam (processor, "mod1_src", (float) Mod::Source::Velocity);
            setParam (processor, "mod1_dst", (float) Mod::destinationForParamId ("osc1_spectral_amt"));
            setParam (processor, "mod1_amt", 0.8f);
        }

        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        renderPeakAndCentroid (processor, 45, 40, centroid, nullptr, velocity);
        return centroid;
    };

    const auto soft = render (20, true);
    const auto hard = render (127, true);
    const auto plain = render (127, false);
    check (hard < soft * 0.9 && hard < plain * 0.9,
           "a velocity route into SPEC AMT morphs each note by its own amount (centroid soft " + juce::String (soft, 0)
               + " Hz, hard " + juce::String (hard, 0) + " Hz, unrouted " + juce::String (plain, 0) + " Hz)");
}

void runStereoModulationTest()
{
    // A stereo route on a filter cutoff opens one side and closes the other; the same route unflagged moves both alike.
    const auto sideDifference = [] (bool stereo)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "f1_cutoff", 400.0f);
        setParam (processor, "mod1_src", (float) Mod::Source::Velocity);
        setParam (processor, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (processor, "mod1_amt", 1.0f);
        setParam (processor, "mod1_stereo", stereo ? 1.0f : 0.0f);
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        auto left = 0.0, right = 0.0;

        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 127), 0);

            processor.processBlock (buffer, midi);

            if (block > 10)
                for (int i = 0; i < 512; ++i)
                {
                    left += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
                    right += (double) buffer.getSample (1, i) * buffer.getSample (1, i);
                }
        }

        check (std::isfinite (left) && std::isfinite (right) && left + right > 0.0, "stereo-routed filter renders audio");
        return std::abs (std::log ((left + 1.0e-9) / (right + 1.0e-9)));
    };

    const auto off = sideDifference (false);
    const auto on = sideDifference (true);
    check (on > off + 0.3,
           "a stereo cutoff route makes the left and right levels differ (log ratio off " + juce::String (off, 2)
               + ", on " + juce::String (on, 2) + ")");
}

void runPerVoiceLfoSpectralTest()
{
    // An LFO that retriggers per note, routed into SPEC AMT, morphs each note from its own start; a free LFO does not.
    const auto brightness = [] (bool retrig, int delayBlocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 10.0f);
        setParam (processor, "osc1_spectral", (float) SpectralWarp::LowPass);
        setParam (processor, "osc1_spectral_amt", 0.4f);
        setParam (processor, "lfo1_rate", 2.0f);
        setParam (processor, "lfo1_retrig", retrig ? 1.0f : 0.0f);
        setParam (processor, "mod1_src", (float) Mod::Source::Lfo1);
        setParam (processor, "mod1_dst", (float) Mod::destinationForParamId ("osc1_spectral_amt"));
        setParam (processor, "mod1_amt", 0.9f);
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        auto energy = 0.0, change = 0.0;

        for (int block = 0; block < delayBlocks + 60; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == delayBlocks)
                midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            if (block >= delayBlocks + 40)
                for (int i = 1; i < 512; ++i)
                {
                    const auto x = (double) buffer.getSample (0, i), d = x - buffer.getSample (0, i - 1);
                    energy += x * x;
                    change += d * d;
                }
        }

        return std::sqrt (change / juce::jmax (1.0e-12, energy));
    };

    const auto retrigGap = std::abs (brightness (true, 0) - brightness (true, 23)) / brightness (true, 0);
    const auto freeGap = std::abs (brightness (false, 0) - brightness (false, 23)) / brightness (false, 0);
    check (retrigGap < 0.05 && freeGap > 0.1,
           "a retriggered LFO into SPEC AMT morphs from each note's start (late-note gap retrig " + juce::String (retrigGap, 3)
               + ", free " + juce::String (freeGap, 3) + ")");
}

void runAudioImportModesTest()
{
    // Vocode: each frame's harmonics follow the audio spectrum at that point (bin 100 early, bin 300 late).
    {
        std::vector<float> audio (16384);
        for (size_t i = 0; i < audio.size(); ++i)
        {
            const auto bin = i < audio.size() / 2 ? 100.0 : 300.0;
            audio[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * bin * (double) i / 2048.0);
        }

        std::vector<std::vector<float>> frames;
        Wavetable::vocode (audio, frames);
        check (frames.size() == 32, "vocode makes 32 frames");

        const auto peakHarmonic = [] (const std::vector<float>& frame)
        {
            TableFFT fft (11);
            std::vector<std::complex<float>> in (2048), out (2048);
            for (size_t i = 0; i < 2048; ++i)
                in[i] = frame[i];
            fft.perform (in.data(), out.data(), false);
            int best = 1;
            for (int k = 1; k < 1024; ++k)
                if (std::abs (out[(size_t) k]) > std::abs (out[(size_t) best]))
                    best = k;
            return best;
        };

        if (frames.size() == 32)
        {
            const auto early = peakHarmonic (frames.front()), late = peakHarmonic (frames.back());
            check (std::abs (early - 100) <= 2 && std::abs (late - 300) <= 2,
                   "vocode frames follow the spectrum (peak harmonic " + juce::String (early) + " -> " + juce::String (late) + ")");
            auto peak = 0.0f;
            for (const auto v : frames[10])
                peak = juce::jmax (peak, std::abs (v));
            check (peak > 0.85f && peak < 0.95f, "vocode frames are normalised");
        }
    }

    // Time slice: equal slices in time order, each squeezed to one cycle.
    {
        std::vector<float> audio (32 * 500);
        for (size_t i = 0; i < audio.size(); ++i)
            audio[i] = (float) i / (float) audio.size();

        std::vector<std::vector<float>> frames;
        Wavetable::timeSlice (audio, frames);
        check (frames.size() == 32 && frames[0].size() == 2048, "time slice makes 32 frames of 2048");

        if (frames.size() == 32)
            check (std::abs (frames[16][0] - 0.5f) < 0.01f && frames[31][0] > frames[1][0],
                   "time slice frames follow the file in time order");
    }
}

void runUnisonTests()
{
    // 16 voices render, and blend 0 leaves only the centre voice(s).
    const auto widthOf = [] (int voices, float blend, int mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_unison", (float) voices);
        setParam (processor, "osc1_detune", 30.0f);
        setParam (processor, "osc1_spread", 1.0f);
        setParam (processor, "osc1_uni_blend", blend);
        setParam (processor, "osc1_uni_mode", (float) mode);
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        auto side = 0.0, mid = 0.0;

        for (int block = 0; block < 60; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            if (block > 10)
                for (int i = 0; i < 512; ++i)
                {
                    const auto l = buffer.getSample (0, i), r = buffer.getSample (1, i);
                    mid += (double) (l + r) * (l + r);
                    side += (double) (l - r) * (l - r);
                }
        }

        return std::sqrt (side / juce::jmax (1.0e-12, mid));
    };

    // Every stack mode renders bounded audio; the interval stacks change the sound.
    {
        const auto render = [] (int mode)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_unison", 5.0f);
            setParam (processor, "osc1_detune", 10.0f);
            setParam (processor, "osc1_uni_mode", (float) mode);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);
            return std::make_pair (peak, centroid);
        };

        const auto classic = render (UnisonMode::Classic);
        auto all = true, different = true;

        for (int mode = UnisonMode::CenterDrop; mode < UnisonMode::Count; ++mode)
        {
            const auto result = render (mode);
            all = all && std::isfinite (result.first) && result.first > 0.02f && result.first < 4.0f;
            different = different && std::abs (result.second - classic.second) > 1.0;
        }

        check (all, "every appended unison stack mode renders bounded audio");
        check (different, "the appended unison stack modes change the sound against Classic");
    }

    // Frame and warp spreads change a stack's sound, stay bounded, and do nothing at 0 or with one voice.
    {
        const auto render = [] (int voices, const char* spreadId, float spread)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_unison", (float) voices);
            setParam (processor, "osc1_detune", 10.0f);
            setParam (processor, "osc1_table", 6.0f);
            setParam (processor, "osc1_frame", 0.5f);
            setParam (processor, "osc1_warp", 7.0f);
            setParam (processor, "osc1_warp_amt", 0.3f);
            setParam (processor, spreadId, spread);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);
            return std::make_pair (peak, centroid);
        };

        for (const auto* id : { "osc1_uni_frame", "osc1_uni_warp" })
        {
            const auto off = render (5, id, 0.0f);
            const auto on = render (5, id, 1.0f);
            const auto single = render (1, id, 1.0f);
            const auto singleOff = render (1, id, 0.0f);
            check (std::isfinite (on.first) && on.first > 0.02f && on.first < 4.0f && std::abs (on.second - off.second) > 1.0,
                   juce::String (id) + " changes a five-voice stack and stays bounded (centroid "
                       + juce::String (off.second, 0) + " -> " + juce::String (on.second, 0) + " Hz)");
            check (std::abs (single.second - singleOff.second) < 0.5,
                   juce::String (id) + " leaves a single voice alone");
        }
    }

    // Scale quantise: C major snaps note 61 (C#) to C or D, so the pitch moves by a semitone from the plain note.
    {
        const auto frequencyOf = [] (int scale, int note)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_table", 0.0f);
            setParam (processor, "osc1_scale", (float) scale);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            renderPeakAndCentroid (processor, note, 40, centroid);
            return centroid;
        };

        const auto plain = frequencyOf (0, 61);
        const auto snapped = frequencyOf (1, 61);
        const auto inKey = frequencyOf (1, 60);
        const auto inKeyPlain = frequencyOf (0, 60);
        check (std::abs (snapped - plain) > 1.0 && std::abs (inKey - inKeyPlain) < 0.5,
               "an oscillator's scale snap moves an out-of-key note and leaves an in-key one (61: "
                   + juce::String (plain, 0) + " -> " + juce::String (snapped, 0) + " Hz)");
    }

    const auto full16 = widthOf (16, 1.0f, UnisonMode::Classic);
    const auto blend0 = widthOf (15, 0.0f, UnisonMode::Classic);
    const auto hyper = widthOf (16, 1.0f, UnisonMode::Hypersaw);
    const auto octaves = widthOf (9, 1.0f, UnisonMode::Octaves);

    check (full16 > 0.2 && std::isfinite (hyper) && hyper > 0.1 && std::isfinite (octaves) && octaves > 0.1,
           "16-voice unison, hypersaw and octave stacks render wide (" + juce::String (full16, 2) + ", "
               + juce::String (hyper, 2) + ", " + juce::String (octaves, 2) + ")");
    check (blend0 < 0.01, "unison blend 0 keeps only the centre voice (width " + juce::String (blend0, 4) + ")");
}

void runPerVoiceLfoTest()
{
    // Two notes started half a second apart with a retriggered LFO on pitch:
    // each voice's LFO starts at its own note-on, so a slow LFO puts the two
    // notes at different points of the cycle. Checked through the voices.
    IlanaSynthAudioProcessor processor;
    setParam (processor, "lfo1_retrig", 1.0f);
    setParam (processor, "lfo1_rate", 0.5f);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 50; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        if (block == 47)
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);
    }

    const auto phases = processor.getVoiceLfoPhasesForTest (0);
    check (phases.size() == 2 && std::abs (phases[0] - phases[1]) > 0.2f,
           "retriggered LFO runs per voice (phases " + juce::String (phases.size() > 0 ? phases[0] : -1.0f, 3) + ", "
               + juce::String (phases.size() > 1 ? phases[1] : -1.0f, 3) + ")");
}

void runMatrixTests()
{
    const auto levelWith = [] (std::function<void (IlanaSynthAudioProcessor&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "master_clip", 0.0f);
        setup (processor);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 57, 30, centroid);
    };

    const auto base = levelWith ([] (auto&) {});

    // Slot 20 works like slot 1: macro 1 at full pulls Amp Level down.
    const auto viaSlot20 = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod20_src", (float) Mod::Source::Macro1);
        setParam (p, "mod20_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod20_amt", -0.8f);
    });
    check (viaSlot20 < base * 0.4f, "mod slot 20 routes (peak " + juce::String (base, 3) + " -> " + juce::String (viaSlot20, 3) + ")");

    const auto bypassed = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod20_src", (float) Mod::Source::Macro1);
        setParam (p, "mod20_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod20_amt", -0.8f);
        setParam (p, "mod20_byp", 1.0f);
    });
    check (std::abs (bypassed - base) < base * 0.05f, "bypassed slot does nothing");

    // Via: macro 2 at zero scales the routing away.
    const auto viaZero = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod3_src", (float) Mod::Source::Macro1);
        setParam (p, "mod3_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod3_amt", -0.8f);
        setParam (p, "mod3_aux", (float) Mod::Source::Macro2);
    });
    check (std::abs (viaZero - base) < base * 0.05f, "via source at zero mutes the routing");

    // Modulating a plain parameter: macro 1 into the master volume.
    const auto masterMod = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod5_src", (float) Mod::Source::Macro1);
        setParam (p, "mod5_dst", (float) Mod::destinationForParamId ("master"));
        setParam (p, "mod5_amt", -0.3f);
    });
    check (masterMod < base * 0.5f, "macro can modulate the master volume parameter (peak " + juce::String (masterMod, 3) + ")");

    // Shape: polarity and curve.
    Mod::Slot slot;
    slot.source = Mod::Source::Macro1;
    slot.polarity = Mod::Polarity::Bipolar;
    check (std::abs (Mod::shape (slot, 0.0f) + 1.0f) < 1.0e-6f && std::abs (Mod::shape (slot, 1.0f) - 1.0f) < 1.0e-6f,
           "bipolar polarity maps a 0..1 source to -1..1");
    slot.polarity = Mod::Polarity::Natural;
    slot.curve = 1.0f;
    check (Mod::shape (slot, 0.5f) < 0.1f, "positive curve bends the response (0.5 -> "
                                              + juce::String (Mod::shape (slot, 0.5f), 3) + ")");

    check (Mod::numExplicitDestinations + Mod::numLegacyParamDestinations == Mod::firstNewExplicitDestination
               && Mod::getDestinationNames().size() == Mod::getNumDestinations(),
           "parameter destinations end exactly where the OSC 4-6 destinations begin");
    check (Mod::getExplicitDestinationNames().size() == Mod::numExplicitDestinations,
           "destination names match the destination list ("
               + juce::String (Mod::getExplicitDestinationNames().size()) + " / " + juce::String (Mod::numExplicitDestinations) + ")");
    check (Mod::getNumDestinations() < IlanaSynthAudioProcessor::maxDestinations, "destination count fits the display table");
}

void runStaleModulationTest()
{
    // A routing removed while a voice is sounding must not leave its last
    // value stuck on the target (it once left presets detuned after a
    // vibrato preset).
    IlanaSynthAudioProcessor processor;
    setParam (processor, "sub_on", 0.0f);
    setParam (processor, "lfo1_rate", 3.0f);
    setParam (processor, "mod1_src", (float) Mod::Source::Lfo1);
    setParam (processor, "mod1_dst", (float) Mod::Destination::Osc1Pitch);
    setParam (processor, "mod1_amt", 0.05f);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> samples;

    for (int block = 0; block < 140; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

        if (block == 13)
            setParam (processor, "mod1_amt", 0.0f);

        processor.processBlock (buffer, midi);

        if (block > 40)
            for (int i = 0; i < 512; ++i)
                samples.push_back (buffer.getSample (0, i));
    }

    const auto frequency = fundamentalOf (samples, 48000.0);
    check (std::abs (frequency - 220.0) < 0.5, "pitch returns to the note after a routing is removed ("
                                                   + juce::String (frequency, 2) + " Hz)");
}

void runPhase2StateAndCpuTest()
{
    // New parameters survive a host round trip.
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "osc2_warp", (float) Warp::Mirror);
        setParam (processor, "osc2_warp_amt", 0.4f);
        setParam (processor, "osc1_uni_mode", (float) UnisonMode::Hypersaw);
        setParam (processor, "mod27_src", (float) Mod::Source::Lfo3);
        setParam (processor, "mod27_dst", (float) Mod::destinationForParamId ("fx_ott_amount"));
        setParam (processor, "mod27_amt", -0.25f);
        setParam (processor, "mod27_pol", 2.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto slot = restored.readModSlot (26);

        check ((int) restored.apvts.getRawParameterValue ("osc2_warp")->load() == Warp::Mirror
                   && (int) restored.apvts.getRawParameterValue ("osc1_uni_mode")->load() == UnisonMode::Hypersaw
                   && slot.source == Mod::Source::Lfo3 && slot.destination == Mod::destinationForParamId ("fx_ott_amount")
                   && std::abs (slot.depth + 0.25f) < 1.0e-3f && slot.polarity == Mod::Polarity::Bipolar,
               "warp, unison mode and an extended mod slot survive a state round trip");
    }

    // CPU. "Heavy" is a realistic big patch (8-note chord, two 16-voice
    // hypersaw oscillators, ladder filter, a busy matrix) and must stay well
    // inside real time. "Extreme" (16 notes x three 16-voice oscillators with
    // warps) is only reported.
    const auto measure = [] (int notes, int oscillators)
    {
        IlanaSynthAudioProcessor processor;
        const char* const prefixes[] { "osc1", "osc2", "sub" };

        for (int osc = 0; osc < oscillators; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            setParam (processor, prefix + "_unison", 16.0f);
            setParam (processor, prefix + "_uni_mode", (float) UnisonMode::Hypersaw);
            setParam (processor, prefix + "_warp", (float) Warp::Sync);
            setParam (processor, prefix + "_warp_amt", 0.5f);
        }

        setParam (processor, "osc2_on", 1.0f);
        setParam (processor, "sub_level", oscillators > 2 ? 0.5f : 0.0f);
        setParam (processor, "f1_type", (float) FilterType::LadderLow);

        for (int slot = 1; slot <= 16; ++slot)
        {
            setParam (processor, "mod" + juce::String (slot) + "_src", (float) (1 + slot % 20));
            setParam (processor, "mod" + juce::String (slot) + "_dst", (float) (1 + (slot * 5) % 88));
            setParam (processor, "mod" + juce::String (slot) + "_amt", 0.2f);
        }

        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer noteOns;

        for (int note = 0; note < notes; ++note)
            noteOns.addEvent (juce::MidiMessage::noteOn (1, 40 + note * 2, (juce::uint8) 100), 0);

        const auto blocks = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_BLOCKS", "94").getIntValue(); // ~1 s
        const auto start = juce::Time::getMillisecondCounterHiRes();

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi = noteOns;

            processor.processBlock (buffer, midi);
        }

        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        return std::make_pair (elapsed / (blocks * 512.0 / 48000.0), std::isfinite (buffer.getMagnitude (0, 512)));
    };

    const auto heavy = measure (8, 2);
    const auto extreme = measure (16, 3);

    std::cout << "INFO: CPU heavy patch " << juce::String (heavy.first * 100.0, 1) << " %, extreme patch "
              << juce::String (extreme.first * 100.0, 1) << " % of one core" << std::endl;
    checkTiming (heavy.first < 0.5 && heavy.second && extreme.second, "heavy patch (8 notes x 32 unison voices) renders well inside real time");
}
