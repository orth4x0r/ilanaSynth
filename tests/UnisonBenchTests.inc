// The SIMD unison bank must sound like the per-voice oscillators it
// replaced: same phases, pitches, warps, frame morphs and FM, summed with the
// same gains and pans.
void runUnisonBankEquivalenceTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    struct Case
    {
        const char* name;
        int lanes;
        int warp;
        float warpAmount;
        int warp2;
        float frame;
        bool oversample;
        int fm; // 0 none, 1 phase modulation, 2 rate (through-zero), 3 exponential rate
    };

    const Case cases[] {
        { "1 lane, plain", 1, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 0 },
        { "16 lanes, plain", 16, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 0 },
        { "7 lanes, frame morph", 7, Warp::Off, 0.0f, Warp::Off, 0.37f, false, 0 },
        { "16 lanes, sync", 16, Warp::Sync, 0.5f, Warp::Off, 0.2f, false, 0 },
        { "16 lanes, PWM", 16, Warp::Pwm, 0.6f, Warp::Off, 0.0f, false, 0 },
        { "12 lanes, bend + PD chain", 12, Warp::BendPlus, 0.4f, Warp::PdRes2, 0.5f, false, 0 },
        { "16 lanes, High quality", 16, Warp::Off, 0.0f, Warp::Off, 0.6f, true, 0 },
        { "16 lanes, phase FM", 16, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 1 },
        { "9 lanes, through-zero FM, High", 9, Warp::Mirror, 0.3f, Warp::Off, 0.0f, true, 2 },
        { "16 lanes, exponential FM", 16, Warp::Off, 0.0f, Warp::Off, 0.8f, false, 3 },
    };

    constexpr double sampleRate = 44100.0;

    for (const auto& c : cases)
    {
        WavetableOscillator voices[UnisonBank::maxLanes];
        UnisonBank bank;
        float gains[UnisonBank::maxLanes], panL[UnisonBank::maxLanes], panR[UnisonBank::maxLanes];
        juce::Random random (1234);

        bank.setSampleRate (sampleRate);
        bank.setWavetable (&table);
        bank.setWarp (c.warp, c.warpAmount);
        bank.setWarp2 (c.warp2, 0.45f);

        for (int u = 0; u < c.lanes; ++u)
        {
            const auto frequency = 110.0 * std::exp2 ((u - c.lanes / 2) * 0.07 / 12.0 + (u % 3) * 0.3);
            const auto startPhase = (double) random.nextFloat();
            voices[u].setSampleRate (sampleRate);
            voices[u].setWavetable (&table);
            voices[u].setFrequency (frequency);
            voices[u].setWarp (c.warp, c.warpAmount);
            voices[u].setWarp2 (c.warp2, 0.45f);
            voices[u].resetPhase (startPhase);
            bank.setFrequency (u, frequency);
            bank.resetPhase (u, startPhase);
            gains[u] = 0.2f + 0.05f * (float) u;
            const auto angle = (float) u / (float) juce::jmax (1, c.lanes - 1) * juce::MathConstants<float>::halfPi;
            panL[u] = std::cos (angle);
            panR[u] = std::sin (angle);
        }

        bank.setWeights (gains, panL, panR, c.lanes);
        const auto frames = WavetableOscillator::frameReadFor (&table, c.frame);

        auto worst = 0.0f;
        auto peak = 0.0f;
        auto syncMismatches = 0;

        for (int i = 0; i < 8192; ++i)
        {
            const auto lfo = std::sin ((double) i * 0.003);
            const auto phaseMod = c.fm == 1 ? 0.8 * lfo : 0.0;
            const auto rate = c.fm == 2 ? 1.0 + 4.0 * 0.6 * lfo
                              : c.fm == 3 ? std::exp2 (juce::jlimit (-4.0, 4.0, 3.0 * lfo)) : 1.0;

            UnisonBank::Sums expected;

            for (int u = 0; u < c.lanes; ++u)
            {
                float raw;

                if (c.oversample)
                {
                    const auto first = voices[u].getNextSample (phaseMod, frames, rate * 0.5);
                    const auto second = voices[u].getNextSample (phaseMod, frames, rate * 0.5);
                    raw = 0.5f * (first + second);
                }
                else
                {
                    raw = voices[u].getNextSample (phaseMod, frames, rate);
                }

                expected.mono += raw * gains[u];
                expected.left += raw * gains[u] * panL[u];
                expected.right += raw * gains[u] * panR[u];
            }

            const auto actual = bank.render (phaseMod, frames, rate, c.oversample);
            worst = juce::jmax (worst, std::abs (actual.mono - expected.mono), std::abs (actual.left - expected.left),
                                std::abs (actual.right - expected.right));
            peak = juce::jmax (peak, std::abs (expected.mono));
            syncMismatches += bank.lane0Wrapped() != voices[0].wrappedThisSample() ? 1 : 0;
        }

        // Fixed-point phases differ from the old doubles by far less than a
        // cent, which shows as a tiny drift against the peak: -66 dB for
        // smooth reads, -54 dB where warps put hard edges in the wave.
        const auto tolerance = (c.warp == Warp::Off && c.warp2 == Warp::Off ? 5.0e-4f : 2.0e-3f) * juce::jmax (1.0f, peak);
        check (worst < tolerance && syncMismatches == 0 && peak > 0.05f,
               juce::String ("unison bank matches per-voice oscillators: ") + c.name + " (worst "
                   + juce::String (worst, 7) + ", peak " + juce::String (peak, 3) + ", sync misses "
                   + juce::String (syncMismatches) + ")");
    }

}

// ILANA_UNISON_BENCH=1: the unison cost the user sees in a DAW. Three
// oscillators x 16 unison at 44.1 kHz, per held note, best of several runs
// (this machine's timings swing a lot run to run).
void runUnisonBenchmark()
{
    const auto measure = [] (int notes, int unison, int quality, int warp, int blockSize = 512)
    {
        auto best = 1.0e9;

        for (int run = 0; run < 5; ++run)
        {
            IlanaSynthAudioProcessor processor;

            for (const auto* prefix : { "osc1", "osc2", "sub" })
            {
                const juce::String p (prefix);
                setParam (processor, p + "_on", 1.0f);
                setParam (processor, p + "_unison", (float) unison);
                setParam (processor, p + "_warp", (float) warp);
                setParam (processor, p + "_warp_amt", warp != Warp::Off ? 0.5f : 0.0f);
            }

            setParam (processor, "sub_level", 0.8f);
            setParam (processor, "quality", (float) quality);
            processor.prepareToPlay (44100.0, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer noteOns;

            for (int note = 0; note < notes; ++note)
                noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 3, (juce::uint8) 100), 0);

            // Warm up (voices start, smoothers settle), then time ~1 s.
            for (int block = 0; block < 8; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi = noteOns;
                processor.processBlock (buffer, midi);
            }

            const auto blocks = 44100 / blockSize;
            const auto start = juce::Time::getMillisecondCounterHiRes();

            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);
            }

            const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
            best = juce::jmin (best, elapsed / (blocks * (double) blockSize / 44100.0));
        }

        return best * 100.0;
    };

    const auto report = [&measure] (const char* label, int notes, int unison, int quality, int warp)
    {
        std::cout << "  bench: " << label << " " << juce::String (measure (notes, unison, quality, warp), 2)
                  << " % of one core" << std::endl;
    };

    report ("0 notes (idle: master FX only)              ", 0, 1, 1, Warp::Off);
    std::cout << "  bench: 0 notes, 128-sample blocks             "
              << juce::String (measure (0, 1, 1, Warp::Off, 128), 2) << " % of one core" << std::endl;
    report ("1 note,  3 osc x  1 unison, Normal          ", 1, 1, 1, Warp::Off);
    report ("1 note,  3 osc x 16 unison, Normal          ", 1, 16, 1, Warp::Off);
    report ("1 note,  3 osc x 16 unison, Normal, Sync    ", 1, 16, 1, Warp::Sync);
    report ("1 note,  3 osc x 16 unison, High            ", 1, 16, 2, Warp::Off);
    report ("4 notes, 3 osc x 16 unison, Normal          ", 4, 16, 1, Warp::Off);
    report ("8 notes, 3 osc x 16 unison, Normal          ", 8, 16, 1, Warp::Off);
}

// ILANA_PROFILE=<unison>: profile the benchmark patch (8 notes, three
// oscillators at that unison) over a minute of audio.
void runProfile (int unison)
{
   #if JUCE_WINDOWS
    IlanaSynthAudioProcessor processor;

    for (const auto* prefix : { "osc1", "osc2", "sub" })
    {
        const juce::String p (prefix);
        setParam (processor, p + "_on", 1.0f);
        setParam (processor, p + "_unison", (float) unison);
    }

    setParam (processor, "sub_level", 0.8f);
    const auto blockSize = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_BLOCK", "512").getIntValue();
    processor.prepareToPlay (44100.0, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer noteOns;
    const auto notes = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_NOTES", "8").getIntValue();

    for (int note = 0; note < notes; ++note)
        noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 3, (juce::uint8) 100), 0);

    processor.processBlock (buffer, noteOns);
    SamplingProfiler profiler;
    profiler.start();

    const auto seconds = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_SECONDS", "60").getIntValue();

    for (int block = 0; block < 44100 * seconds / blockSize; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    profiler.stop();
    profiler.report();
   #else
    juce::ignoreUnused (unison);
   #endif
}
