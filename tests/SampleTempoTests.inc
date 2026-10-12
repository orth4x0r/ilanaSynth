// Roadmap step 40: a sample follows the host tempo (SYNC, BPM) and STRETCH
// keeps its pitch apart from its speed. Included by TableTest.cpp;
// ILANA_SAMPLE_TEMPO_TEST=1 runs only these.

namespace SampleTempoLab
{
struct Head : juce::AudioPlayHead
{
    double bpm = 120.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        return info;
    }
};

inline std::shared_ptr<SampleData> sineSample (double hz, double sampleRate, int length)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = sampleRate;
    data->buffer.setSize (1, length);
    for (int i = 0; i < length; ++i)
        data->buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate));
    return data;
}

// Runs a player until it stops (or the limit), returning what it played.
inline std::vector<float> runPlayer (const SampleData& sample, bool stretch, bool reverse, double timeRate, double pitchRate,
                                     int limit = 400000)
{
    SamplePlayer player;
    player.prepare (48000.0);
    SamplePlayer::Params params;
    params.sample = &sample;
    params.stretch = stretch;
    params.reverse = reverse;
    player.setParams (params);
    if (stretch)
        player.setRates (timeRate, pitchRate);
    else
        player.setPlaybackRatio (timeRate);
    player.trigger();

    std::vector<float> out;
    while (player.isActive() && (int) out.size() < limit)
    {
        float l = 0.0f, r = 0.0f;
        player.process (l, r);
        out.push_back (l);
    }
    return out;
}

inline double rmsRange (const std::vector<float>& x, size_t from, size_t count)
{
    double sum = 0.0;
    const auto to = juce::jmin (x.size(), from + count);
    for (auto i = from; i < to; ++i)
        sum += (double) x[i] * x[i];
    return to > from ? std::sqrt (sum / (double) (to - from)) : 0.0;
}

inline double centsOff (double measured, double expected) { return 1200.0 * std::log2 (juce::jmax (1.0e-9, measured) / expected); }

// The sample time the note last sounded (above -40 dB of the sine).
inline int lastSounding (const std::vector<float>& x)
{
    for (auto i = (int) x.size() - 1; i >= 0; --i)
        if (std::abs (x[(size_t) i]) > 0.005f)
            return i;
    return 0;
}

inline void set (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace SampleTempoLab

void runSampleTempoTests()
{
    using namespace SampleTempoLab;

    // The tempo maths.
    check (SampleTempo::tempoRatio (SampleTempo::off, 90.0, 2.0, 140.0) == 1.0, "tempo: SYNC Off keeps the sample's own speed");
    check (std::abs (SampleTempo::tempoRatio (SampleTempo::bpm, 100.0, 2.0, 150.0) - 1.5) < 1e-12, "tempo: BPM 100 at host 150 plays 1.5x");
    // 1 bar (4 beats) lasting 2 s is 120 BPM; at host 90 it plays at 0.75x.
    check (std::abs (SampleTempo::tempoRatio (4, 0.0, 2.0, 90.0) - 0.75) < 1e-12, "tempo: a 2 s region as 1 bar is 120 BPM");
    check (SampleTempo::tempoRatio (4, 0.0, 0.0, 120.0) == 1.0, "tempo: an empty region keeps the sample's speed");
    check (SampleTempo::syncNames().size() == 9, "tempo: nine SYNC choices");

    // The player alone: a 1 s, 440 Hz sine at 48 kHz.
    const auto sine = sineSample (440.0, 48000.0, 48000);
    const auto expectLength = [] (const std::vector<float>& out, int expected, const juce::String& what)
    {
        check (std::abs ((int) out.size() - expected) < expected / 100 + 64,
               what + ": lasts " + juce::String ((int) out.size()) + " samples (expected " + juce::String (expected) + ")");
    };
    const auto expectPitch = [] (const std::vector<float>& out, double expected, const juce::String& what)
    {
        const std::vector<float> middle (out.begin() + (long) (out.size() / 4), out.begin() + (long) (3 * out.size() / 4));
        const auto cents = centsOff (MultiSampleLab::sineFrequency (middle, 48000.0), expected);
        check (std::abs (cents) < 5.0, what + ": at pitch (" + juce::String (cents, 2) + " cents)");
    };
    const auto expectSteady = [] (const std::vector<float>& out, const juce::String& what)
    {
        // Every 1024-sample window holds the sine's level: no dip or bump where grains meet.
        auto finite = true;
        auto peak = 0.0f;
        for (const auto x : out)
        {
            finite = finite && std::isfinite (x);
            peak = juce::jmax (peak, std::abs (x));
        }
        auto worst = 0.0;
        for (size_t from = 2048; from + 2048 < out.size(); from += 1024)
            worst = juce::jmax (worst, std::abs (juce::Decibels::gainToDecibels (rmsRange (out, from, 1024) / (0.5 / std::sqrt (2.0)))));
        check (finite && peak < 0.6f, what + ": finite and bounded (peak " + juce::String (peak, 3) + ")");
        check (worst < 1.5, what + ": a steady level (worst window " + juce::String (worst, 2) + " dB off)");
    };

    {
        const auto out = runPlayer (*sine, true, false, 1.0, 1.0);
        expectLength (out, 48000, "stretch 1:1");
        expectPitch (out, 440.0, "stretch 1:1");
        expectSteady (out, "stretch 1:1");
        check (std::abs (out[12]) > 0.05f, "stretch: the attack is not faded in");
    }
    {
        const auto out = runPlayer (*sine, true, false, 1.0, 1.5);
        expectLength (out, 48000, "stretch, a fifth up");
        expectPitch (out, 660.0, "stretch, a fifth up");
        expectSteady (out, "stretch, a fifth up");
    }
    {
        const auto out = runPlayer (*sine, true, false, 0.5, 1.0);
        expectLength (out, 96000, "stretch, half speed");
        expectPitch (out, 440.0, "stretch, half speed");
        expectSteady (out, "stretch, half speed");
    }
    {
        const auto out = runPlayer (*sine, true, false, 2.0, 0.5);
        expectLength (out, 24000, "stretch, double speed an octave down");
        expectPitch (out, 220.0, "stretch, double speed an octave down");
        expectSteady (out, "stretch, double speed an octave down");
    }
    {
        const auto out = runPlayer (*sine, true, true, 0.7, 1.0);
        expectLength (out, 68571, "stretch, reversed at 0.7x");
        expectPitch (out, 440.0, "stretch, reversed at 0.7x");
        expectSteady (out, "stretch, reversed at 0.7x");
    }
    {
        const auto out = runPlayer (*sine, false, false, 2.0, 1.0);
        expectLength (out, 24000, "repitch 2x");
        expectPitch (out, 880.0, "repitch 2x");
    }

    // The processor: a 1 s sine on C4 (the tuned sample's root), as 2 beats
    // (120 BPM), played by a host at 240 BPM.
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-sample-tempo-test");
    folder.deleteRecursively();
    folder.createDirectory();
    const auto c4 = juce::MidiMessage::getMidiNoteInHertz (60);
    const auto wav = folder.getChildFile ("two-beats.wav");
    MultiSampleLab::writeWav (wav, MultiSampleLab::sine (c4, 48000.0, 48000), 48000.0);

    // time: TIME's choice (0 Repitch, 1 Stretch, 2 Signalsmith, 3 Bungee,
    // 4 PGHI, 5 Paulstretch). Offline, so the engines' copies render in place.
    const auto render = [&wav] (double hostBpm, int sync, float bpm, int time, int note, int blocks = 160)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        Head head;
        head.bpm = hostBpm;
        processor.setPlayHead (&head);
        MultiSampleLab::setUp (processor);
        processor.loadUserSample (0, wav);
        set (processor, "osc1_sample_sync", (float) sync);
        set (processor, "osc1_sample_bpm", bpm);
        set (processor, "osc1_sample_stretch", (float) time);
        const auto out = MultiSampleLab::play (processor, note, blocks);
        processor.setPlayHead (nullptr);
        return out;
    };
    const auto sounding = [] (const std::vector<float>& out, double expectedSeconds, const juce::String& what)
    {
        const auto seconds = lastSounding (out) / 48000.0;
        check (std::abs (seconds - expectedSeconds) < 0.03, what + ": sounds " + juce::String (seconds, 3) + " s (expected "
                                                                 + juce::String (expectedSeconds, 3) + " s)");
    };
    const auto pitched = [] (const std::vector<float>& out, double expected, const juce::String& what)
    {
        const std::vector<float> part (out.begin() + 4096, out.begin() + 4096 + 9600);
        const auto cents = centsOff (MultiSampleLab::sineFrequency (part, 48000.0), expected);
        check (std::abs (cents) < 5.0, what + ": at pitch (" + juce::String (cents, 2) + " cents)");
    };

    {
        const auto out = render (240.0, 3, 120.0f, false, 60);
        sounding (out, 0.5, "SYNC 2 beats at host 240, Repitch");
        pitched (out, 2.0 * c4, "SYNC 2 beats at host 240, Repitch");
    }
    {
        const auto out = render (240.0, 3, 120.0f, true, 60);
        sounding (out, 0.5, "SYNC 2 beats at host 240, Stretch");
        pitched (out, c4, "SYNC 2 beats at host 240, Stretch");
    }
    {
        const auto out = render (240.0, 3, 120.0f, true, 67);
        sounding (out, 0.5, "SYNC 2 beats at host 240, Stretch, G4");
        pitched (out, juce::MidiMessage::getMidiNoteInHertz (67), "SYNC 2 beats at host 240, Stretch, G4");
    }
    {
        const auto out = render (240.0, SampleTempo::bpm, 120.0f, true, 60);
        sounding (out, 0.5, "SYNC BPM 120 at host 240, Stretch");
    }
    {
        const auto out = render (90.0, SampleTempo::bpm, 120.0f, true, 60, 200);
        sounding (out, 4.0 / 3.0, "SYNC BPM 120 at host 90, Stretch");
        pitched (out, c4, "SYNC BPM 120 at host 90, Stretch");
    }
    {
        // Pitch without tempo: SYNC Off, STRETCH on, an octave up keeps 1 s.
        const auto out = render (240.0, SampleTempo::off, 120.0f, true, 72);
        sounding (out, 1.0, "SYNC Off, Stretch, C5");
        pitched (out, 2.0 * c4, "SYNC Off, Stretch, C5");
    }
    // TIME's engines, rendered copies (offline: in place). A tolerance per
    // engine for where the sound ends: Paulstretch's long windows smear it.
    const char* engineNames[] { "Signalsmith", "Bungee", "PGHI", "Paulstretch" };
    for (int engine = 0; engine < 4; ++engine)
    {
        const juce::String name (engineNames[engine]);
        const auto time = 2 + engine;
        const auto tail = engine == 3 ? 0.3 : 0.06;
        const auto soundsFor = [tail] (const std::vector<float>& out, double expectedSeconds, const juce::String& what)
        {
            const auto seconds = lastSounding (out) / 48000.0;
            check (seconds > expectedSeconds - tail && seconds < expectedSeconds + tail,
                   what + ": sounds " + juce::String (seconds, 3) + " s (expected " + juce::String (expectedSeconds, 3) + " s)");
        };
        // The pitch over the middle half of the sound (away from the edges a
        // stretcher smears); Paulstretch's random phases blur it a little.
        const auto pitchedMiddle = [engine] (const std::vector<float>& out, double seconds, double expected, const juce::String& what)
        {
            const auto from = (size_t) (0.25 * seconds * 48000.0), to = (size_t) (0.75 * seconds * 48000.0);
            const std::vector<float> part (out.begin() + (long) from, out.begin() + (long) juce::jmin (to, out.size()));
            const auto cents = centsOff (MultiSampleLab::sineFrequency (part, 48000.0), expected);
            check (std::abs (cents) < (engine == 3 ? 25.0 : 5.0), what + ": at pitch (" + juce::String (cents, 2) + " cents)");
        };
        const auto bounded = [] (const std::vector<float>& out, const juce::String& what)
        {
            auto peak = 0.0f;
            auto finite = true;
            for (const auto x : out)
            {
                finite = finite && std::isfinite (x);
                peak = juce::jmax (peak, std::abs (x));
            }
            check (finite && peak < 2.0f && peak > 0.05f, what + ": finite, bounded and sounding (peak " + juce::String (peak, 3) + ")");
        };
        {
            const auto out = render (240.0, 3, 120.0f, time, 60);
            bounded (out, name + ", 2 beats at host 240");
            soundsFor (out, 0.5, name + ", 2 beats at host 240");
            pitchedMiddle (out, 0.5, c4, name + ", 2 beats at host 240");
        }
        {
            const auto out = render (80.0, 3, 120.0f, time, 67, 260);
            bounded (out, name + ", 2 beats at host 80, G4");
            soundsFor (out, 1.5, name + ", 2 beats at host 80, G4");
            pitchedMiddle (out, 1.5, juce::MidiMessage::getMidiNoteInHertz (67), name + ", 2 beats at host 80, G4");
        }
        {
            // SYNC Off: the engine as a pitch shifter, an octave up keeps 1 s.
            const auto out = render (120.0, SampleTempo::off, 120.0f, time, 72);
            soundsFor (out, 1.0, name + ", SYNC Off, C5");
            pitchedMiddle (out, 1.0, 2.0 * c4, name + ", SYNC Off, C5");
        }
    }

    {
        // Live (not offline): the first note plays the plain stretch while the
        // copy renders in the background; once it's there, notes play it. A
        // note held through the swap stays bounded.
        IlanaSynthAudioProcessor processor;
        Head head;
        head.bpm = 240.0;
        processor.setPlayHead (&head);
        MultiSampleLab::setUp (processor);
        processor.loadUserSample (0, wav);
        set (processor, "osc1_sample_loop", 1.0f);
        set (processor, "osc1_sample_sync", 3.0f);
        set (processor, "osc1_sample_stretch", 2.0f); // Signalsmith
        const auto first = MultiSampleLab::play (processor, 60, 8);
        juce::ignoreUnused (first);
        auto peak = 0.0f;
        auto finite = true;
        std::vector<float> held;
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 400; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
                held.push_back (buffer.getSample (0, i));
            juce::Thread::sleep (2); // a render's worth of wall time passes
        }
        for (const auto x : held)
        {
            finite = finite && std::isfinite (x);
            peak = juce::jmax (peak, std::abs (x));
        }
        check (finite && peak < 2.0f, "live: a held note through the swap stays bounded (peak " + juce::String (peak, 3) + ")");
        check (rmsRange (held, held.size() - 24000, 24000) > 0.01, "live: the held note keeps sounding after the swap");
        for (int wait = 0; wait < 400 && processor.getStretchRendersPending() > 0; ++wait)
            juce::Thread::sleep (10);
        check (processor.getStretchRendersPending() == 0, "live: the copy finished rendering");
        processor.setPlayHead (nullptr);
    }

    {
        // At the sample's own tempo, SYNC changes nothing (bit for bit).
        const auto synced = render (120.0, 3, 120.0f, false, 64);
        const auto plain = render (120.0, SampleTempo::off, 120.0f, false, 64);
        auto diff = 0.0f;
        for (size_t i = 0; i < juce::jmin (synced.size(), plain.size()); ++i)
            diff = juce::jmax (diff, std::abs (synced[i] - plain[i]));
        check (diff == 0.0f, "SYNC at the sample's own tempo plays exactly as Off (max diff " + juce::String (diff, 8) + ")");
    }
    {
        // A looped sample at the extremes stays bounded.
        IlanaSynthAudioProcessor processor;
        Head head;
        head.bpm = 999.0;
        processor.setPlayHead (&head);
        MultiSampleLab::setUp (processor);
        processor.loadUserSample (0, wav);
        set (processor, "osc1_sample_loop", 1.0f);
        set (processor, "osc1_sample_sync", 2.0f); // 1 beat: 60 BPM, so 16x (the limit)
        set (processor, "osc1_sample_stretch", 1.0f);
        set (processor, "osc1_unison", 8.0f);
        const auto out = MultiSampleLab::play (processor, 96, 300);
        processor.setPlayHead (nullptr);
        auto peak = 0.0f;
        auto finite = true;
        for (const auto x : out)
        {
            finite = finite && std::isfinite (x);
            peak = juce::jmax (peak, std::abs (x));
        }
        check (finite && peak < 4.0f, "a looped, stretched sample at 16x and 8 voices stays bounded (peak " + juce::String (peak, 3) + ")");
        check (rmsRange (out, out.size() - 48000, 48000) > 0.01, "the looped, stretched sample keeps sounding");
    }

    folder.deleteRecursively();

    // ILANA_SAMPLE_TEMPO_DEMO=<folder>: a made-up 2-bar drum and bass loop
    // at 120 BPM (SYNC 2 Bars), played at other host tempos and notes, for
    // listening.
    if (const auto demo = juce::SystemStats::getEnvironmentVariable ("ILANA_SAMPLE_TEMPO_DEMO", ""); demo.isNotEmpty())
    {
        const juce::File out (demo);
        out.createDirectory();
        constexpr double rate = 48000.0;
        const auto length = (int) (4.0 * rate); // 8 beats at 120 BPM
        juce::AudioBuffer<float> loop (1, length);
        loop.clear();
        juce::Random random (40);
        const auto beat = (int) (0.5 * rate);
        for (int b = 0; b < 8; ++b)
        {
            const auto at = b * beat;
            for (int i = 0; i < beat && at + i < length; ++i)
            {
                const auto t = i / rate;
                auto x = 0.0;
                if (b % 2 == 0) // kick
                    x += 0.8 * std::exp (-t * 18.0) * std::sin (juce::MathConstants<double>::twoPi * (50.0 * t + 60.0 * (1.0 - std::exp (-t * 30.0)) / 30.0));
                else // snare
                    x += 0.5 * std::exp (-t * 22.0) * (random.nextFloat() * 2.0 - 1.0) + 0.3 * std::exp (-t * 30.0) * std::sin (juce::MathConstants<double>::twoPi * 190.0 * t);
                for (const auto offset : { 0, beat / 2 }) // hats on the eighths
                    if (i >= offset)
                        x += 0.15 * std::exp (-(i - offset) / rate * 60.0) * (random.nextFloat() * 2.0 - 1.0);
                // bass: root, fifth, octave, fifth
                const double notes[] { 55.0, 82.41, 110.0, 82.41 };
                x += 0.25 * std::sin (juce::MathConstants<double>::twoPi * notes[(b / 2) % 4] * t) * std::exp (-t * 3.0);
                loop.addSample (0, at + i, (float) (0.6 * x));
            }
        }
        const auto source = out.getChildFile ("loop-120bpm.wav");
        MultiSampleLab::writeWav (source, loop, rate);

        const auto renderTo = [&source, &out] (const juce::String& name, double hostBpm, int time, int note, int sync)
        {
            IlanaSynthAudioProcessor processor;
            processor.setNonRealtime (true);
            Head head;
            head.bpm = hostBpm;
            processor.setPlayHead (&head);
            MultiSampleLab::setUp (processor);
            processor.loadUserSample (0, source);
            set (processor, "osc1_sample_loop", 1.0f);
            set (processor, "osc1_sample_sync", (float) sync);
            set (processor, "osc1_sample_stretch", (float) time);
            const auto audio = MultiSampleLab::play (processor, note, (int) (8.0 * 48000.0 / 512.0));
            processor.setPlayHead (nullptr);
            juce::AudioBuffer<float> buffer (1, (int) audio.size());
            for (size_t i = 0; i < audio.size(); ++i)
                buffer.setSample (0, (int) i, audio[i]);
            MultiSampleLab::writeWav (out.getChildFile (name + ".wav"), buffer, 48000.0);
        };
        renderTo ("1-host-120-as-recorded", 120.0, false, 60, 5);
        renderTo ("2-host-90-stretch", 90.0, true, 60, 5);
        renderTo ("3-host-150-stretch", 150.0, true, 60, 5);
        renderTo ("4-host-150-repitch", 150.0, false, 60, 5);
        renderTo ("5-host-120-stretch-up-5-semitones", 120.0, true, 65, 5);
        renderTo ("6-host-120-repitch-up-5-semitones", 120.0, false, 65, SampleTempo::off);
        const char* engines[] { "signalsmith", "bungee", "pghi", "paulstretch" };
        for (int engine = 0; engine < 4; ++engine)
        {
            const juce::String name (engines[engine]);
            renderTo ("7-" + name + "-host-90", 90.0, 2 + engine, 60, 5);
            renderTo ("8-" + name + "-host-150", 150.0, 2 + engine, 60, 5);
            renderTo ("9-" + name + "-host-120-up-5-semitones", 120.0, 2 + engine, 65, 5);
        }
        renderTo ("10-paulstretch-host-30-pad", 30.0, 5, 60, 5);
        std::cout << "Sample tempo demo written to " << out.getFullPathName() << std::endl;
    }

    // ILANA_SAMPLE_TEMPO_BENCH=1: the cost of one stretched player against
    // a plain one (ns per output sample, best of five).
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SAMPLE_TEMPO_BENCH", "").isNotEmpty())
    {
        const auto stereo = std::make_shared<SampleData>();
        stereo->sampleRate = 44100.0;
        stereo->buffer.setSize (2, 441000);
        juce::Random random (7);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 441000; ++i)
                stereo->buffer.setSample (c, i, 0.3f * (float) std::sin (0.031 * i + c) + 0.1f * (random.nextFloat() - 0.5f));
        for (const auto stretch : { false, true })
        {
            auto best = 1.0e30;
            for (int run = 0; run < 5; ++run)
            {
                const auto start = juce::Time::getHighResolutionTicks();
                const auto out = runPlayer (*stereo, stretch, false, 0.9 * 44100.0 / 48000.0, 1.12 * 44100.0 / 48000.0);
                const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
                best = juce::jmin (best, seconds * 1.0e9 / (double) out.size());
            }
            std::cout << "BENCH sample player " << (stretch ? "stretch" : "repitch") << ": " << juce::String (best, 2) << " ns/sample" << std::endl;
        }

        // TIME's engines: the time to render a 4 s stereo 44.1 kHz copy, 1.25x
        // longer and up 3 semitones (on the worker thread, not the audio one).
        OfflineStretch::Audio input;
        input.sampleRate = 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            const auto* read = stereo->buffer.getReadPointer (c);
            input.channels.emplace_back (read, read + 4 * 44100);
        }
        const char* engines[] { "Signalsmith", "Bungee", "PGHI", "Paulstretch" };
        for (int engine = 0; engine < 4; ++engine)
        {
            const auto start = juce::Time::getHighResolutionTicks();
            const auto copy = OfflineStretch::render ((OfflineStretch::Engine) engine, input, 1.25, std::exp2 (3.0 / 12.0));
            const auto ms = 1000.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
            std::cout << "BENCH render " << engines[engine] << ": " << juce::String (ms, 1) << " ms for 4 s stereo ("
                      << copy.numFrames() << " frames)" << std::endl;
        }
    }
}
