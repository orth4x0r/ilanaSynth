// Pre-1.1 states used OSC 3 as the sub; loading them moves a plain sub to
// the dedicated SUB and keeps a real OSC 3 at the same pitch.
void runOsc3MigrationTest()
{
    const auto loadOldState = [] (std::function<void (juce::ValueTree&)> edit)
    {
        IlanaSynthAudioProcessor source;
        auto state = source.apvts.copyState();
        state.removeProperty ("osc3Schema", nullptr);
        edit (state);

        juce::MemoryBlock data;
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        source.copyXmlToBinary (*xml, data);

        auto target = std::make_unique<IlanaSynthAudioProcessor>();
        target->setStateInformation (data.getData(), (int) data.getSize());
        return target;
    };

    const auto setParam = [] (juce::ValueTree& state, const juce::String& id, float value)
    {
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);

            if (child.getProperty ("id").toString() == id)
            {
                child.setProperty ("value", value, nullptr);
                return;
            }
        }

        juce::ValueTree child ("PARAM");
        child.setProperty ("id", id, nullptr);
        child.setProperty ("value", value, nullptr);
        state.appendChild (child, nullptr);
    };

    const auto value = [] (IlanaSynthAudioProcessor& processor, const char* id)
    {
        return processor.apvts.getRawParameterValue (id)->load();
    };

    // Plain sub: old table 0 (Shape) with a saw, two octaves down.
    auto plain = loadOldState ([&] (juce::ValueTree& state)
    {
        setParam (state, "sub_on", 1.0f);
        setParam (state, "sub_table", 0.0f);
        setParam (state, "sub_shape", 2.0f);
        setParam (state, "sub_octave", 1.0f);
        setParam (state, "sub_level", 0.4f);
    });

    check (value (*plain, "subosc_on") > 0.5f && value (*plain, "sub_on") < 0.5f && (int) value (*plain, "sub_shape") == 2
               && (int) value (*plain, "sub_octave") == 1 && std::abs (value (*plain, "subosc_level") - 0.4f) < 1.0e-4f,
           "old plain sub moves to the dedicated SUB");

    // Real OSC 3: old table 5 (factory table 1), one octave down, semi +3.
    auto real = loadOldState ([&] (juce::ValueTree& state)
    {
        setParam (state, "sub_on", 1.0f);
        setParam (state, "sub_table", 5.0f);
        setParam (state, "sub_octave", 0.0f);
        setParam (state, "sub_semi", 3.0f);
        setParam (state, "osc1_table", 17.0f); // "User 2" before v1.1
    });

    check ((int) value (*real, "osc1_table") == TableFactory::getNumFactoryTables() + 1,
           "an old user wavetable choice still points at the same user slot");

    check (value (*real, "sub_on") > 0.5f && value (*real, "subosc_on") < 0.5f
               && (int) value (*real, "sub_table") == 1 && (int) value (*real, "sub_semi") == -9,
           "old real OSC 3 keeps its table and pitch (table " + juce::String ((int) value (*real, "sub_table"))
               + ", semi " + juce::String ((int) value (*real, "sub_semi")) + ")");

    // New states are not migrated again.
    juce::MemoryBlock saved;
    real->getStateInformation (saved);
    IlanaSynthAudioProcessor again;
    again.setStateInformation (saved.getData(), (int) saved.getSize());
    check ((int) again.apvts.getRawParameterValue ("sub_semi")->load() == -9, "saved 1.1 state is not migrated twice");
}

void runSpectralWarpTests()
{
    // Each warp on a bright table: bounded, and different from the source.
    {
        Wavetable source;
        source.buildFromFrames (TableFactory::generate (10));

        for (int mode = SpectralWarp::Stretch; mode < SpectralWarp::Count; ++mode)
        {
            const auto amount = mode == SpectralWarp::Formant ? 0.9f : 0.7f;
            const auto warped = SpectralWarp::warpTable (source, mode, amount);
            auto peak = 0.0f;
            auto diff = 0.0;
            auto finite = true;

            for (int frame = 0; frame < warped->getNumFrames(); ++frame)
            {
                const auto* a = source.getFrameData (0, frame);
                const auto* b = warped->getFrameData (0, frame);

                for (int i = 0; i < Wavetable::frameSize; ++i)
                {
                    finite = finite && std::isfinite (b[i]);
                    peak = juce::jmax (peak, std::abs (b[i]));
                    diff += (double) (a[i] - b[i]) * (a[i] - b[i]);
                }
            }

            diff = std::sqrt (diff / (double) (warped->getNumFrames() * Wavetable::frameSize));
            check (finite && peak <= 1.05f && peak > 0.1f && diff > 0.02,
                   "Spectral warp " + SpectralWarp::getNames()[mode] + " is bounded and audible (peak "
                       + juce::String (peak, 3) + ", diff " + juce::String (diff, 3) + ")");
        }
    }

    const auto render = [] (int mode, bool offline)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (offline);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_table", 10.0f);
        set ("osc1_spectral", (float) mode);
        set ("osc1_spectral_amt", 0.8f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;
        auto waited = 0;

        // Realtime: let the worker build the warp before the note starts.
        for (; ! offline && waited < 400; ++waited)
        {
            buffer.clear();
            juce::MidiBuffer none;
            processor.processBlock (buffer, none);

            if (processor.isSpectralWarpReady (0))
                break;

            juce::Thread::sleep (5);
        }

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return std::make_pair (out, processor.isSpectralWarpReady (0));
    };

    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += (double) (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (sum / juce::jmax ((size_t) 1, a.size()));
    };

    const auto plain = render (SpectralWarp::Off, true).first;
    const auto offline = render (SpectralWarp::Stretch, true);
    const auto realtime = render (SpectralWarp::Stretch, false);

    check (offline.second && difference (plain, offline.first) > 0.01,
           "Offline renders build the spectral warp in place (diff " + juce::String (difference (plain, offline.first), 4) + ")");
    check (realtime.second && difference (offline.first, realtime.first) < 1.0e-4,
           "The background worker builds the same warp (diff " + juce::String (difference (offline.first, realtime.first), 6) + ")");
}

void runChaosLfoTests()
{
    juce::Random random (7);

    // Chaos: bounded, uses both halves, and doesn't repeat cycle to cycle.
    {
        LfoChaos chaos;
        chaos.reset (random);
        std::vector<float> values;
        auto finite = true;

        for (int i = 0; i < 48000 * 4; ++i)
        {
            chaos.advance (2.0 / 48000.0);
            values.push_back (chaos.value (LfoShapes::Chaos, 0.0));
            finite = finite && std::isfinite (values.back());
        }

        const auto [low, high] = std::minmax_element (values.begin(), values.end());
        auto cycleDifference = 0.0;

        for (int i = 0; i < 24000; ++i)
            cycleDifference += std::abs ((double) values[(size_t) i + 96000] - (double) values[(size_t) i + 120000]);

        check (finite && *low < -0.4f && *high > 0.4f && *low >= -1.0f && *high <= 1.0f && cycleDifference / 24000.0 > 0.05,
               "Chaos LFO wanders both ways without repeating (" + juce::String (*low, 2) + " .. " + juce::String (*high, 2)
                   + ", cycle diff " + juce::String (cycleDifference / 24000.0, 3) + ")");
    }

    // Drunk: small steps, stays in range. Smooth Random: continuous across cycles.
    {
        LfoChaos drunk, smooth;
        drunk.reset (random);
        smooth.reset (random);
        auto maxStep = 0.0f, maxJump = 0.0f;
        auto inRange = true;
        auto last = smooth.value (LfoShapes::SmoothRandom, 0.999);

        for (int cycle = 0; cycle < 2000; ++cycle)
        {
            const auto before = drunk.target;
            drunk.onCycle (LfoShapes::Drunk, random);
            maxStep = juce::jmax (maxStep, std::abs (drunk.target - before));
            inRange = inRange && std::abs (drunk.target) <= 1.0f;

            smooth.onCycle (LfoShapes::SmoothRandom, random);
            maxJump = juce::jmax (maxJump, std::abs (smooth.value (LfoShapes::SmoothRandom, 0.0) - last));
            last = smooth.value (LfoShapes::SmoothRandom, 0.999);
        }

        check (inRange && maxStep <= 0.4501f, "Drunk LFO takes small steps and stays in range (max step " + juce::String (maxStep, 3) + ")");
        check (maxJump < 0.01f, "Smooth Random LFO has no jumps between cycles (" + juce::String (maxJump, 4) + ")");
    }

    // Key tracking: a per-voice LFO at the note's pitch, driving level.
    const auto render = [] (bool key, int shape)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_table", 8.0f);
        set ("osc1_frame", 0.0f);
        set ("osc1_level", 0.8f); // leaves the level LFO room to swing
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        set ("lfo1_shape", (float) shape);
        set ("lfo1_rate", 12.0f); // key tracked: three times the note
        set ("lfo1_key", key ? 1.0f : 0.0f);
        set ("mod1_src", (float) Mod::Source::Lfo1);
        set ("mod1_dst", (float) Mod::Destination::Osc1Level);
        set ("mod1_amt", 0.5f);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;

        for (int block = 0; block < 16; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    // Brightness proxy: mean absolute first difference relative to level.
    const auto brightness = [] (const std::vector<float>& data)
    {
        auto diff = 0.0, level = 0.0;

        for (size_t i = 4096; i < data.size(); ++i)
        {
            diff += std::abs ((double) data[i] - (double) data[i - 1]);
            level += std::abs ((double) data[i]);
        }

        return diff / juce::jmax (1.0e-9, level);
    };

    const auto slow = render (false, LfoShapes::Sine);
    const auto audio = render (true, LfoShapes::Sine);
    check (brightness (audio) > brightness (slow) * 1.3,
           "A key-tracked LFO on level is audio-rate AM (brightness " + juce::String (brightness (slow), 4) + " -> "
               + juce::String (brightness (audio), 4) + ")");

    for (const auto shape : { (int) LfoShapes::SmoothRandom, (int) LfoShapes::Drunk, (int) LfoShapes::Chaos })
    {
        const auto out = render (false, shape);
        auto finite = true;
        auto peak = 0.0f;

        for (auto value : out)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

        check (finite && peak > 0.01f && peak < 4.0f, "LFO shape " + juce::String (shape) + " drives a voice cleanly (peak " + juce::String (peak, 3) + ")");
    }
}

void runPhysicsLfoTests()
{
    for (const auto shape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring, LfoShapes::Friction })
    {
        LfoChaos state;
        state.resetPhysics (shape, 0.5f);
        const auto initial = state.value (shape, 0.0);
        auto low = 1.0f, high = -1.0f, largestStep = 0.0f;
        auto earlyEnergy = 0.0, lateEnergy = 0.0;
        auto last = initial;
        auto finite = true;
        for (int sample = 0; sample < 48000; ++sample)
        {
            state.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            const auto value = state.value (shape, 0.0);
            finite = finite && std::isfinite (value) && value >= -1.0f && value <= 1.0f;
            low = juce::jmin (low, value);
            high = juce::jmax (high, value);
            largestStep = juce::jmax (largestStep, std::abs (value - last));
            if (sample < 12000) earlyEnergy += std::abs ((double) value - (shape == LfoShapes::Bounce ? -1.0 : 0.0));
            if (sample >= 36000) lateEnergy += std::abs ((double) value - (shape == LfoShapes::Bounce ? -1.0 : 0.0));
            last = value;
        }
        check (finite && high - low > 0.1f, "physics LFO shape " + juce::String (shape) + " moves and stays bounded");
        {
            // A retrigger starts exactly where the motion continues from.
            LfoChaos fresh;
            fresh.resetPhysics (shape, 0.5f);
            const auto start = fresh.value (shape, 0.0);
            fresh.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            check (std::abs (fresh.value (shape, 0.0) - start) < 0.01f,
                   "physics LFO shape " + juce::String (shape) + " starts without a jump");
        }
        if (shape == LfoShapes::Friction)
            check (largestStep > 0.5f, "Friction LFO makes abrupt slips");
        if (shape == LfoShapes::Pendulum)
            check (lateEnergy < earlyEnergy * 0.7, "Pendulum loses energy over time");
        if (shape == LfoShapes::Spring)
            check (low < -0.1f && high > 0.1f, "Spring LFO overshoots");
        state.resetPhysics (shape, 0.5f);
        check (state.value (shape, 0.0) == initial, "physics LFO retrigger restores initial position");
        for (const auto rate : { 0.01, 40.0, 200.0 })
        {
            state.resetPhysics (shape, 1.0f);
            for (int i = 0; i < 48000; ++i)
            {
                state.advancePhysics (shape, rate / 48000.0, 1.0f, 0.0f);
                finite = finite && std::isfinite (state.value (shape, 0.0));
            }
        }
        check (finite, "physics LFO survives extreme rates and settings");
    }

    const auto globalPendulum = [] (bool kick)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "lfo1_shape", (float) LfoShapes::Pendulum);
        setParam (processor, "lfo1_rate", 4.0f);
        setParam (processor, "lfo1_kick", kick ? 1.0f : 0.0f);
        setParam (processor, "lfo1_retrig", 0.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        processor.processBlock (buffer, midi);
        return processor.getLfoLiveValue (0);
    };
    check (std::abs (globalPendulum (true) - globalPendulum (false)) > 0.02f,
           "Pendulum kick adds note energy even without RETRIG");
}

// Free-running physics LFOs must keep moving (they re-excite once settled)
// without jumps, and each bounce of the ball must be lower than the last.
void runPhysicsLfoMotionTest()
{
    for (const auto shape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring })
    {
        LfoChaos state;
        state.resetPhysics (shape, 0.5f);
        auto last = state.value (shape, 0.0);
        auto largestStep = 0.0f, lateLow = 1.0f, lateHigh = -1.0f;
        constexpr int seconds = 30;

        for (int sample = 0; sample < 48000 * seconds; ++sample)
        {
            state.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            const auto value = state.value (shape, 0.0);
            largestStep = juce::jmax (largestStep, std::abs (value - last));
            last = value;

            if (sample >= 48000 * (seconds - 1))
            {
                lateLow = juce::jmin (lateLow, value);
                lateHigh = juce::jmax (lateHigh, value);
            }
        }

        check (lateHigh - lateLow > 0.2f, "free-running physics LFO " + juce::String (shape) + " still moves after 30 s");
        check (largestStep < 0.05f, "physics LFO " + juce::String (shape) + " re-excites without a jump (largest step "
                                        + juce::String (largestStep, 4) + ")");
    }

    LfoChaos ball;
    ball.resetPhysics (LfoShapes::Bounce, 1.0f);
    std::vector<float> apexes;
    auto previous = ball.value (LfoShapes::Bounce, 0.0);
    auto rising = false;

    for (int sample = 0; sample < 48000 * 3 && apexes.size() < 5; ++sample)
    {
        ball.advancePhysics (LfoShapes::Bounce, 1.0 / 48000.0, 1.0f, 0.5f);
        const auto value = ball.value (LfoShapes::Bounce, 0.0);

        // Equal samples at the top of an arc keep the current direction.
        if (value > previous)
            rising = true;
        else if (value < previous)
        {
            if (rising)
                apexes.push_back (previous);

            rising = false;
        }

        previous = value;
    }

    auto lower = apexes.size() >= 4;

    for (size_t i = 1; i < apexes.size(); ++i)
        lower = lower && apexes[i] < apexes[i - 1];

    check (lower, "each bounce is lower than the last (" + juce::String ((int) apexes.size()) + " apexes)");
}

void runSympatheticTuningTest()
{
    // With no scale, an open tuning on the root instead of a semitone cluster.
    SympatheticStrings strings;
    strings.prepare (48000.0);
    strings.setTuning (0, 2, false, { 48, 55, 60, 64, 67, 72 });
    const int open[] { 50, 57, 62, 66, 69, 74 };
    auto matches = true;

    for (int i = 0; i < SympatheticStrings::maxStrings; ++i)
        matches = matches && strings.noteFor (i) == open[i];

    check (matches, "sympathetic strings use an open tuning on the root when no scale is set");

    // Each string rings at its note, low and high.
    for (const auto note : { 48, 84 })
    {
        SympatheticStrings single;
        single.prepare (48000.0);
        single.setTuning (0, 0, true, { note, note, note, note, note, note });
        std::vector<float> ring (48000);

        for (int i = 0; i < (int) ring.size(); ++i)
            ring[(size_t) i] = single.process (i < 64 ? 1.0f : 0.0f, 1, 1.0f, 1.0f);

        // Strongest frequency within +-60 cents of the note, in 0.5 cent steps.
        const auto expected = juce::MidiMessage::getMidiNoteInHertz (note);
        auto cents = 0.0, bestPower = -1.0;

        for (double offset = -60.0; offset <= 60.0; offset += 0.1)
        {
            const auto hz = expected * std::exp2 (offset / 1200.0);
            double re = 0.0, im = 0.0;

            for (int i = 2000; i < 40000; ++i)
            {
                const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
                const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - 2000) / 38000.0);
                re += window * ring[(size_t) i] * std::cos (angle);
                im += window * ring[(size_t) i] * std::sin (angle);
            }

            if (re * re + im * im > bestPower)
            {
                bestPower = re * re + im * im;
                cents = offset;
            }
        }
        check (std::abs (cents) < 0.5, "sympathetic string " + juce::String (note) + " rings in tune ("
                                            + juce::String (cents, 1) + " cents)");
    }
}

void runBridgeBuzzStabilityTest()
{
    // In-loop buzz may colour the string but never sustain or grow it.
    KarplusStrong string (12345);
    string.prepare (48000.0);
    string.setFrequency (110.0);
    string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.1f, 1.0f);
    string.setPhysicalParams (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    string.setBowAndBuzz (0.5f, 0.5f, 1.0f, 0.0f);
    string.trigger (1.0f);

    auto early = 0.0, late = 0.0;
    auto finite = true;

    for (int i = 0; i < 48000 * 6; ++i)
    {
        const auto value = string.process();
        finite = finite && std::isfinite (value);

        if (i < 4800)
            early = juce::jmax (early, (double) std::abs (value));

        if (i >= 48000 * 5)
            late = juce::jmax (late, (double) std::abs (value));
    }

    check (finite && late < early * 0.5, "full bridge buzz decays instead of sustaining (early "
                                             + juce::String (early, 3) + ", late " + juce::String (late, 3) + ")");
}

void runIntegerValueTextTest()
{
    IlanaSynthAudioProcessor processor;
    juce::RangedAudioParameter* note = processor.apvts.getParameter ("sym_note1");
    juce::RangedAudioParameter* count = processor.apvts.getParameter ("sym_count");

    check (note != nullptr && note->getText (note->convertTo0to1 (60.0f), 16) == "C3"
               && count != nullptr && count->getText (count->convertTo0to1 (3.0f), 16) == "3 strings",
           "integer parameters show their value text (note names, units)");
    check (note != nullptr && juce::roundToInt (note->convertFrom0to1 (note->getValueForText ("C#2"))) == 49
               && juce::roundToInt (note->convertFrom0to1 (note->getValueForText ("67"))) == 67,
           "note knobs accept typed note names and numbers");
}

void runMissingParameterDefaultTest()
{
    IlanaSynthAudioProcessor processor;
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "state for missing parameter test is readable");
    if (xml == nullptr) return;
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (child->getStringAttribute ("id") == "lfo1_phys_a"
            || child->getStringAttribute ("id") == "lfo1_phys_b"
            || child->getStringAttribute ("id") == "lfo1_kick")
            xml->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    for (const auto* id : { "lfo1_phys_a", "lfo1_phys_b", "lfo1_kick" })
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (1.0f));
    processor.setStateInformation (oldState.getData(), (int) oldState.getSize());
    const auto read = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    check (read ("lfo1_phys_a") == 0.5f && read ("lfo1_phys_b") == 0.5f && read ("lfo1_kick") == 0.0f,
           "missing new parameters load their declared defaults after a nondefault patch");
}

void runM3PhysicalTests()
{
    const auto renderBow = [] (float expression, bool release)
    {
        KarplusStrong string (777);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Bow, 0.0f, 0.35f, 0.15f);
        string.setBowAndBuzz (0.5f, 0.5f, 0.0f, 0.0f);
        string.trigger (0.8f);
        std::array<double, 3> energies {};
        for (int i = 0; i < 96000; ++i)
        {
            const auto value = string.process (expression, ! release || i < 48000);
            if (i >= 36000 && i < 48000) energies[0] += value * value;
            if (i >= 60000 && i < 72000) energies[1] += value * value;
            if (i >= 84000) energies[2] += value * value;
        }
        return energies;
    };
    const auto bowed = renderBow (0.0f, false);
    const auto released = renderBow (0.0f, true);
    const auto pressed = renderBow (1.0f, false);
    check (bowed[0] > 1.0e-6 && bowed[2] > released[2] * 2.0,
           "Bow sustains while held and fades after release");
    check (std::abs (pressed[0] - bowed[0]) > bowed[0] * 0.05,
           "MPE pressure or channel aftertouch changes bow energy");

    const auto renderContact = [] (float buzz, float rattle, float velocity)
    {
        KarplusStrong string (313);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.15f, 0.95f);
        string.setBowAndBuzz (0.0f, 0.5f, buzz, rattle);
        string.trigger (velocity);
        std::vector<float> samples (10000);
        for (auto& sample : samples) sample = string.process();
        return samples;
    };
    const auto clean = renderContact (0.0f, 0.0f, 0.9f);
    const auto buzz = renderContact (1.0f, 0.0f, 0.9f);
    const auto rattleHigh = renderContact (0.0f, 1.0f, 0.9f);
    const auto cleanLow = renderContact (0.0f, 0.0f, 0.2f);
    const auto rattleLow = renderContact (0.0f, 1.0f, 0.2f);
    const auto delta = [] (const auto& a, const auto& b)
    {
        auto total = 0.0;
        for (size_t i = 0; i < a.size(); ++i) total += std::abs ((double) a[i] - b[i]);
        return total;
    };
    check (delta (clean, buzz) > 0.1, "Bridge buzz changes the waveform");
    check (delta (clean, rattleHigh) > delta (cleanLow, rattleLow) * 2.0,
           "Fret rattle grows with velocity");
    for (const auto& samples : { clean, buzz, rattleHigh })
    {
        auto finite = true;
        for (const auto value : samples) finite = finite && std::isfinite (value) && std::abs (value) <= 8.0f;
        check (finite, "Physical contact stays finite and bounded");
    }

    SympatheticStrings drones;
    drones.prepare (48000.0);
    const std::array<int, 6> manualNotes { 57, 60, 64, 69, 72, 76 };
    drones.setTuning (1, 0, false, manualNotes);
    check (drones.noteFor (0) == 48 && drones.noteFor (1) == 50 && drones.noteFor (2) == 52,
           "Sympathetic strings follow GENERATE scale and root");
    drones.setTuning (1, 0, true, manualNotes);
    check (drones.noteFor (0) == 57 && drones.noteFor (3) == 69,
           "Manual sympathetic tuning overrides the scale");
    auto tail = 0.0;
    auto bounded = true;
    for (int i = 0; i < 48000; ++i)
    {
        const auto input = i < 10000 ? 0.4f * std::sin ((float) i * 2.0f * juce::MathConstants<float>::pi
                                                          * 220.0f / 48000.0f) : 0.0f;
        const auto value = drones.process (input, 6, 1.0f, 0.95f);
        if (i > 20000) tail += std::abs ((double) value);
        bounded = bounded && std::isfinite (value) && std::abs (value) <= 4.0f;
    }
    check (tail > 0.01 && bounded, "Shared sympathetic strings ring after excitation and remain bounded");

    IlanaSynthAudioProcessor heavy;
    heavy.prepareToPlay (48000.0, 512);
    const auto swarm = heavy.getFactoryPresetNames().indexOf ("Swarm");
    if (swarm >= 0) heavy.loadFactoryPreset (swarm);
    setParam (heavy, "sym_on", 1.0f);
    setParam (heavy, "sym_amount", 1.0f);
    setParam (heavy, "sym_count", 6.0f);
    juce::AudioBuffer<float> heavyBuffer (2, 512);
    const auto start = juce::Time::getHighResolutionTicks();
    for (int block = 0; block < 188; ++block)
    {
        heavyBuffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            for (const auto note : { 48, 52, 55, 59, 62, 67 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        heavy.processBlock (heavyBuffer, midi);
    }
    const auto msPerSecond = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start)
                             * 1000.0 / (188.0 * 512.0 / 48000.0);
    checkTiming (msPerSecond < 400.0, "Six shared sympathetic strings fit the heavy preset CPU budget ("
                                 + juce::String (msPerSecond, 1) + " ms/s)");
}

void runM3MissingParameterTest()
{
    IlanaSynthAudioProcessor processor;
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "M3 state migration XML exists");
    if (xml == nullptr) return;
    juce::StringArray ids { "sym_on", "sym_amount", "sym_decay", "sym_count", "sym_manual" };
    for (int i = 1; i <= 6; ++i) ids.add ("sym_note" + juce::String (i));
    for (const auto* prefix : { "osc1", "osc2", "sub" })
        for (const auto* suffix : { "_bow_pressure", "_bow_speed", "_bridge_buzz", "_fret_rattle" })
            ids.add (juce::String (prefix) + suffix);
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (ids.contains (child->getStringAttribute ("id"))) xml->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    for (const auto& id : ids)
        if (auto* param = processor.apvts.getParameter (id)) param->setValueNotifyingHost (1.0f);
    processor.setStateInformation (oldState.getData(), (int) oldState.getSize());
    auto defaultsRestored = true;
    for (const auto& id : ids)
    {
        auto* param = processor.apvts.getParameter (id);
        auto* raw = processor.apvts.getRawParameterValue (id);
        defaultsRestored = defaultsRestored && param != nullptr && raw != nullptr
                          && std::abs (raw->load() - param->convertFrom0to1 (param->getDefaultValue())) < 0.001f;
    }
    check (defaultsRestored, "Every missing M3 parameter loads its declared default");
}

void runGranularTests()
{
    const auto render = [] (std::function<void (juce::AudioProcessorValueTreeState&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        auto& state = processor.apvts;
        const auto set = [&state] (const char* id, float value)
        {
            if (auto* parameter = state.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_mode", 3.0f);
        set ("osc1_grain_pos", 0.3f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        setup (state);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> left, right;

        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
            {
                left.push_back (buffer.getSample (0, i));
                right.push_back (buffer.getSample (1, i));
            }
        }

        return std::make_pair (left, right);
    };

    const auto stats = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto energy = 0.0, difference = 0.0;
        auto peak = 0.0f;
        auto finite = true;

        for (size_t i = 4800; i < a.size(); ++i)
        {
            energy += (double) a[i] * a[i];
            difference += (double) (a[i] - b[i]) * (a[i] - b[i]);
            peak = juce::jmax (peak, std::abs (a[i]), std::abs (b[i]));
            finite = finite && std::isfinite (a[i]) && std::isfinite (b[i]);
        }

        const auto n = (double) (a.size() - 4800);
        return std::make_tuple (std::sqrt (energy / n), std::sqrt (difference / n), peak, finite);
    };

    const auto [rms, sideRms, peak, finite] = [&]
    {
        const auto out = render ([] (juce::AudioProcessorValueTreeState&) {});
        return stats (out.first, out.second);
    }();

    check (finite && rms > 0.01 && peak < 2.0f,
           "Granular mode sounds with no sample loaded (rms " + juce::String (rms, 4) + ", peak " + juce::String (peak, 3) + ")");
    check (sideRms > 0.005, "Granular grains are spread across the stereo field (side " + juce::String (sideRms, 4) + ")");

    const auto mono = render ([] (juce::AudioProcessorValueTreeState& state)
    {
        if (auto* parameter = state.getParameter ("osc1_grain_spread"))
            parameter->setValueNotifyingHost (0.0f);
    });
    const auto [monoRms, monoSide, monoPeak, monoFinite] = stats (mono.first, mono.second);
    check (monoFinite && monoSide < 1.0e-4 && monoRms > 0.01, "Granular stereo spread at zero is mono (side " + juce::String (monoSide, 6) + ")");

    // Sparse vs dense clouds stay in the same loudness ballpark.
    for (const auto density : { 0.0f, 1.0f })
    {
        const auto out = render ([density] (juce::AudioProcessorValueTreeState& state)
        {
            if (auto* parameter = state.getParameter ("osc1_grain_density"))
                parameter->setValueNotifyingHost (density);
        });
        const auto [densityRms, s2, densityPeak, densityFinite] = stats (out.first, out.second);
        juce::ignoreUnused (s2);
        check (densityFinite && densityRms > rms * 0.3 && densityRms < rms * 3.0 && densityPeak < 2.5f,
               "Granular density " + juce::String (density, 1) + " keeps a sensible level (rms " + juce::String (densityRms, 4) + ")");
    }
}

// The new engines must stay affordable: six-note chords on the heaviest
// v1.1 presets, measured as milliseconds of CPU per second of audio.
void runHeavyPresetCpuTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto names = processor.getFactoryPresetNames();

    for (const auto* name : { "Swarm", "Grain Choir", "Gated Supersaw", "Harmonic Cut Pad", "Chaos Filter Pad", "Operator Bell" })
    {
        const auto index = names.indexOf (name);

        if (index < 0)
        {
            check (false, juce::String ("heavy preset exists: ") + name);
            continue;
        }

        processor.loadFactoryPreset (index);
        processor.panic();

        juce::AudioBuffer<float> buffer (2, 512);
        const auto blocks = (int) (48000 * 2 / 512);
        const auto start = juce::Time::getHighResolutionTicks();

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                for (const auto note : { 48, 52, 55, 59, 62, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);
        }

        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        const auto msPerSecond = seconds * 1000.0 / 2.0;
        std::cout << "  cpu: " << name << " " << juce::String (msPerSecond, 1) << " ms per second of audio" << std::endl;
        checkTiming (msPerSecond < 400.0, juce::String (name) + " renders a six-note chord well within real time ("
                                          + juce::String (msPerSecond, 1) + " ms/s)");
    }
}

void runFmMatrixTests()
{
    const auto renderWith = [] (std::function<void (IlanaSynthAudioProcessor&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        // Two sine operators, no sub, open filter.
        set ("subosc_on", 0.0f);
        set ("osc1_table", 8.0f);
        set ("osc1_frame", 0.0f);
        set ("osc2_on", 1.0f);
        set ("osc2_table", 8.0f);
        set ("osc2_fine", 0.0f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        setup (processor);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 512);

        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    const auto set = [] (IlanaSynthAudioProcessor& processor, const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto rms = [] (const std::vector<float>& data)
    {
        auto sum = 0.0;

        for (auto value : data)
            sum += (double) value * value;

        return std::sqrt (sum / juce::jmax ((size_t) 1, data.size()));
    };

    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += (double) (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (sum / juce::jmax ((size_t) 1, a.size()));
    };

    // OSC 1 as a silent modulator of OSC 2.
    const auto plain = renderWith ([&] (IlanaSynthAudioProcessor& p) { set (p, "osc1_out", 0.0f); });
    const auto modulated = renderWith ([&] (IlanaSynthAudioProcessor& p)
    {
        set (p, "osc1_out", 0.0f);
        set (p, "fm_1to2", 0.6f);
    });
    const auto both = renderWith ([&] (IlanaSynthAudioProcessor&) {});

    check (rms (plain) > 0.01 && rms (both) > rms (plain) * 1.3,
           "OUT off keeps an oscillator out of the mix (" + juce::String (rms (plain), 3) + " vs " + juce::String (rms (both), 3) + ")");
    check (difference (plain, modulated) > rms (plain) * 0.2,
           "OSC 1 > OSC 2 FM changes OSC 2 while OSC 1 stays silent");

    // All three FM styles stay finite and bounded at full depth.
    for (int mode = 0; mode < 3; ++mode)
    {
        const auto out = renderWith ([&] (IlanaSynthAudioProcessor& p)
        {
            set (p, "fm_mode", (float) mode);
            set (p, "fm_1to2", 1.0f);
            set (p, "fm_amount", 1.0f);
            set (p, "fm_fb2", 1.0f);
        });

        auto finite = true;
        auto peak = 0.0f;

        for (auto value : out)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

        check (finite && peak < 4.0f && peak > 0.001f, "FM mode " + juce::String (mode) + " stays bounded at full depth (peak "
                                                           + juce::String (peak, 3) + ")");
    }

    // Saved 1.0 routings to parameter destinations move past the new FM ones.
    IlanaSynthAudioProcessor source;
    auto state = source.apvts.copyState();
    state.removeProperty ("destSchema", nullptr);
    const auto oldIndex = Mod::explicitDestinationsV10 + 3;

    for (int i = 0; i < state.getNumChildren(); ++i)
        if (state.getChild (i).getProperty ("id").toString() == "mod1_dst")
            state.getChild (i).setProperty ("value", oldIndex, nullptr);

    juce::MemoryBlock data;
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    juce::AudioProcessor::copyXmlToBinary (*xml, data);
    IlanaSynthAudioProcessor target;
    target.setStateInformation (data.getData(), (int) data.getSize());
    const auto migrated = (int) target.apvts.getRawParameterValue ("mod1_dst")->load();
    check (migrated == Mod::numExplicitDestinations + 3,
           "old parameter-destination routings are renumbered (" + juce::String (migrated) + ")");
}

void runM3bEngineTests()
{
    const auto render = [] (std::function<void (IlanaSynthAudioProcessor&)> configure, int blocks = 24, int note = 57)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "noise_level", 0.0f);
        setParam (processor, "osc2_on", 0.0f);
        setParam (processor, "f1_cutoff", 20000.0f);
        setParam (processor, "f1_env", 0.0f);
        configure (processor);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        std::vector<float> samples;
        const auto peak = renderPeakAndCentroid (processor, note, blocks, centroid, &samples);
        return std::make_pair (peak, samples);
    };
    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double sum = 0.0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += std::abs ((double) a[i] - b[i]);
        return sum / (double) juce::jmax ((size_t) 1, juce::jmin (a.size(), b.size()));
    };

    const auto baseline = render ([] (auto&) {});
    const auto normal = render ([] (auto& p) { setParam (p, "quality", 1.0f); });
    check (difference (baseline.second, normal.second) == 0.0, "Normal quality is the legacy default sample for sample");
    const auto highNormal = render ([] (auto& p) { setParam (p, "osc1_table", 6.0f); }, 24, 93);
    const auto highQuality = render ([] (auto& p)
    {
        setParam (p, "osc1_table", 6.0f);
        setParam (p, "quality", 2.0f);
    }, 24, 93);
    const auto stepEnergy = [] (const std::vector<float>& samples)
    {
        double energy = 0.0;
        for (size_t i = 1; i < samples.size(); ++i)
            energy += std::pow ((double) samples[i] - samples[i - 1], 2.0);
        return energy;
    };
    check (std::isfinite (highQuality.first) && highQuality.first > 0.001f
               && stepEnergy (highQuality.second) < stepEnergy (highNormal.second),
           "High quality smooths high-note wavetable steps");
    for (int osc = 4; osc <= 6; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);
        for (int mode = 0; mode < 4; ++mode)
        {
            const auto sound = render ([&] (auto& p)
            {
                setParam (p, "osc1_on", 0.0f);
                setParam (p, prefix + "_on", 1.0f);
                setParam (p, prefix + "_mode", (float) mode);
                setParam (p, prefix + "_sample_factory", 1.0f);
            });
            auto finite = true;
            for (auto sample : sound.second) finite = finite && std::isfinite (sample);
            check (finite && sound.first > 0.0001f, prefix + " mode " + juce::String (mode) + " renders finite audio");
        }
        const auto off = render ([&] (auto& p)
        {
            setParam (p, prefix + "_on", 0.0f);
            setParam (p, prefix + "_mode", 1.0f);
            setParam (p, prefix + "_unison", 8.0f);
        });
        check (difference (baseline.second, off.second) == 0.0, prefix + " disabled leaves audio sample for sample unchanged");
    }

    const auto both = render ([] (auto& p)
    {
        setParam (p, "osc1_route", (float) FilterRoute::Both);
        setParam (p, "f1_cutoff", 20.0f);
        setParam (p, "f2_cutoff", 20000.0f);
    });
    const auto f1 = render ([] (auto& p)
    {
        setParam (p, "osc1_route", (float) FilterRoute::Filter1);
        setParam (p, "f1_cutoff", 20.0f);
        setParam (p, "f2_cutoff", 20000.0f);
    });
    check (both.first > f1.first * 2.0f, "Both route reaches Filter 2 when Filter 1 is closed");

    // Every cell has an audible effect with its source silent in the output.
    // The target alone is compared against the same two-oscillator patch.
    for (int source = 1; source <= 6; ++source)
        for (int target = 1; target <= 6; ++target)
        {
            const auto id = source == target ? (source == 1 ? juce::String ("fm_feedback") : "fm_fb" + juce::String (source))
                                             : (source == 2 && target == 1 ? juce::String ("fm_amount")
                                                                           : "fm_" + juce::String (source) + "to" + juce::String (target));
            const auto setup = [&] (auto& p)
            {
                setParam (p, "osc1_on", 0.0f);
                for (int osc = 1; osc <= 6; ++osc)
                {
                    const auto prefix = osc == 3 ? juce::String ("sub") : "osc" + juce::String (osc);
                    setParam (p, prefix + "_on", osc == source || osc == target ? 1.0f : 0.0f);
                    setParam (p, prefix + "_table", 8.0f);
                    setParam (p, prefix + "_out", osc == target ? 1.0f : 0.0f);
                }
            };
            const auto dry = render (setup, 20);
            const auto wet = render ([&] (auto& p) { setup (p); setParam (p, id, 0.75f); }, 20);
            check (difference (dry.second, wet.second) > 0.00001, "FM cell changes target: " + id);
        }

    const auto envDry = render ([] (auto& p) { setParam (p, "env6_sustain", 1.0f); });
    const auto envWet = render ([] (auto& p)
    {
        setParam (p, "env6_sustain", 1.0f);
        setParam (p, "mod1_src", (float) Mod::Source::Env6);
        setParam (p, "mod1_dst", (float) Mod::Destination::Osc1Pitch);
        setParam (p, "mod1_amt", 0.2f);
    });
    check (difference (envDry.second, envWet.second) > 0.0001, "ENV 6 works as a per-voice matrix source");

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "osc1_on", 0.0f);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "osc4_on", 1.0f);
        setParam (processor, "osc4_amp_env", 5.0f); // ENV 6, zero based
        setParam (processor, "amp_release", 0.005f);
        setParam (processor, "env6_sustain", 1.0f);
        setParam (processor, "env6_release", 2.0f);
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 270; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            if (block == 8) midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            processor.processBlock (buffer, midi);
            if (block == 60) check (processor.getActiveVoiceCount() == 1, "long OSC 4 envelope outlives AMP");
        }
        check (processor.getActiveVoiceCount() == 0, "OSC 4 envelope eventually releases the voice");
    }

    {
        IlanaSynthAudioProcessor processor;
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
        for (auto* child = xml->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute ("id");
            if (id.startsWith ("osc4_") || id.startsWith ("osc5_") || id.startsWith ("osc6_")
                || id.startsWith ("env6_") || id == "quality")
                xml->removeChildElement (child, true);
            child = next;
        }
        for (const auto* attribute : { "envRevealCount", "envRevealMask", "oscRevealMask", "lfoRevealMask" })
            xml->removeAttribute (attribute);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        setParam (processor, "osc4_on", 1.0f);
        setParam (processor, "osc4_amp_env", 5.0f);
        setParam (processor, "env6_release", 9.0f);
        setParam (processor, "quality", 2.0f);
        processor.setStateInformation (legacy.getData(), (int) legacy.getSize());
        const auto get = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        const auto releaseDefault = processor.apvts.getParameter ("env6_release")->convertFrom0to1 (
            processor.apvts.getParameter ("env6_release")->getDefaultValue());
        check (get ("osc4_on") == 0.0f && get ("osc4_amp_env") == 0.0f
                   && std::abs (get ("env6_release") - releaseDefault) < 0.001f && get ("quality") == 1.0f,
               "pre-M3b state resets new parameters to neutral defaults (osc4 " + juce::String (get ("osc4_on"))
                   + ", amp env " + juce::String (get ("osc4_amp_env")) + ", release "
                   + juce::String (get ("env6_release"), 4) + ", quality " + juce::String (get ("quality")) + ")");
        using Module = IlanaSynthAudioProcessor::Module;
        const auto revealedCount = [] (const IlanaSynthAudioProcessor& p, Module kind, int count)
        {
            auto shown = 0;
            for (int i = 0; i < count; ++i)
                shown += p.isRevealed (kind, i) ? 1 : 0;
            return shown;
        };
        check (revealedCount (processor, Module::Envelope, 16) == 3 && revealedCount (processor, Module::Oscillator, 6) == 3
                   && revealedCount (processor, Module::Lfo, 4) == 3,
               "pre-M3b state shows three oscillators, envelopes and LFOs");
        check (processor.isOscillatorShown (2) && ! processor.isOscillatorShown (3),
               "OSC 3 is shown and OSC 4 is hidden by default");
        processor.setRevealed (Module::Envelope, 5, true);
        processor.addOscillator (3);
        check (processor.apvts.getRawParameterValue ("osc4_on")->load() > 0.5f, "adding an oscillator switches it on");
        juce::MemoryBlock revealedState;
        processor.getStateInformation (revealedState);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (revealedState.getData(), (int) revealedState.getSize());
        check (restored.isRevealed (Module::Envelope, 5) && ! restored.isRevealed (Module::Envelope, 4)
                   && restored.isOscillatorShown (3),
               "revealed envelopes and oscillators survive a patch state round trip");
        restored.removeOscillator (3);
        check (! restored.isOscillatorShown (3) && restored.apvts.getRawParameterValue ("osc4_on")->load() < 0.5f,
               "removing an oscillator hides it and switches it off");

        // The first M3b build saved a count of revealed envelopes.
        if (auto countXml = juce::AudioProcessor::getXmlFromBinary (revealedState.getData(), (int) revealedState.getSize()))
        {
            countXml->removeAttribute ("envRevealMask");
            countXml->setAttribute ("envRevealCount", 7);
            juce::MemoryBlock countState;
            juce::AudioProcessor::copyXmlToBinary (*countXml, countState);
            IlanaSynthAudioProcessor fromCount;
            fromCount.setStateInformation (countState.getData(), (int) countState.getSize());
            check (revealedCount (fromCount, Module::Envelope, 16) == 7, "an envelope reveal count loads as the first N envelopes");
        }
    }

    const auto sixOscCpu = [] (int quality)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "quality", (float) quality);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "sub_on", 1.0f);
        for (int osc = 1; osc <= 6; ++osc)
        {
            const auto prefix = osc == 3 ? juce::String ("sub") : "osc" + juce::String (osc);
            setParam (processor, prefix + "_on", 1.0f);
            setParam (processor, prefix + "_unison", 6.0f);
        }
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        const auto start = juce::Time::getHighResolutionTicks();
        for (int block = 0; block < 188; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 52, 55, 59, 62, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
        }
        return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 500.0;
    };
    // Best of three, interleaved: one pass of each swung by a third between
    // runs with machine load, which made "Eco is cheaper" flaky.
    auto normalCpu = 1.0e9, ecoCpu = 1.0e9;
    for (int pass = 0; pass < 3; ++pass)
    {
        normalCpu = juce::jmin (normalCpu, sixOscCpu (1));
        ecoCpu = juce::jmin (ecoCpu, sixOscCpu (0));
    }
    std::cout << "  cpu: six oscillators Normal " << normalCpu << " ms/s, Eco " << ecoCpu << " ms/s" << std::endl;
    checkTiming (normalCpu < 400.0 && ecoCpu < 400.0 && ecoCpu < normalCpu,
           "six-oscillator Normal and Eco stay within budget and Eco is cheaper");
}
