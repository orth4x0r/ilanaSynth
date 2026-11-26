// Bug round 3 (2026-11-25): regressions for what it fixed.
// Included by TableTest.cpp; ILANA_TEST_ONLY=round3 runs only these.

void runRound3FixTests()
{
    const auto setPlain = [] (IlanaSynthAudioProcessor& p, const juce::String& id, float value)
    {
        if (auto* parameter = p.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    // Renders `blocks` blocks of 256 at 48 kHz, `events` giving each block's
    // MIDI; returns the level (dB) of the last `measured` blocks.
    const auto render = [] (IlanaSynthAudioProcessor& p, int blocks, int measured,
                            const std::function<void (int, juce::MidiBuffer&)>& events)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        double sum = 0.0;
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            events (block, midi);
            p.processBlock (buffer, midi);
            if (block >= blocks - measured)
                for (int i = 0; i < 256; ++i)
                    sum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        return 10.0 * std::log10 (sum / (juce::jmax (1, measured) * 256.0) + 1.0e-20);
    };
    const auto fresh = [&setPlain] (IlanaSynthAudioProcessor& p)
    {
        p.setNonRealtime (true);
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        setPlain (p, "amp_release", 0.02f);
        p.runAsyncUpdateForTest();
    };

    // Two routes that differ only in STEREO are not merged into one.
    {
        IlanaSynthAudioProcessor p;
        fresh (p);
        for (int slot = 0; slot < 2; ++slot)
        {
            p.setModSlotValue (slot, "src", (float) Mod::Source::Macro1);
            p.setModSlotValue (slot, "dst", (float) Mod::Destination::AmpLevel);
            p.setModSlotValue (slot, "amt", 0.2f);
        }
        check (p.canMergeModSlots (0, 1), "round3 merge: identical routes still merge");
        p.setModSlotValue (1, "stereo", 1.0f);
        check (! p.canMergeModSlots (0, 1), "round3 merge: a stereo route does not merge into a plain one");
    }

    // All Sound Off cuts at once, even under a long release.
    {
        const auto stop = [&] (bool cut)
        {
            IlanaSynthAudioProcessor p;
            fresh (p);
            setPlain (p, "amp_release", 8.0f);
            p.runAsyncUpdateForTest();
            return render (p, 40, 4, [cut] (int block, juce::MidiBuffer& midi)
            {
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
                if (block == 30)
                    midi.addEvent (cut ? juce::MidiMessage::allSoundOff (1) : juce::MidiMessage::allNotesOff (1), 0);
            });
        };
        const auto soft = stop (false), cut = stop (true);
        check (cut < soft - 20.0, "round3 midi: All Sound Off cuts a long release (" + juce::String (cut, 1) + " dB, All Notes Off "
                                      + juce::String (soft, 1) + " dB)");
    }

    // MPE: the master channel's sustain pedal holds a member channel's note,
    // and its All Notes Off releases it.
    {
        const auto held = [&] (bool pedal, bool allOff, bool mpe = true, int noteChannel = 3)
        {
            IlanaSynthAudioProcessor p;
            fresh (p);
            setPlain (p, "mpe_mode", mpe ? 1.0f : 0.0f);
            p.runAsyncUpdateForTest();
            return render (p, 60, 10, [pedal, allOff, noteChannel] (int block, juce::MidiBuffer& midi)
            {
                if (block == 2 && pedal)
                    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                if (block == 3)
                    midi.addEvent (juce::MidiMessage::noteOn (noteChannel, 60, (juce::uint8) 110), 0);
                if (block == 20)
                    midi.addEvent (juce::MidiMessage::noteOff (noteChannel, 60), 0);
                if (block == 30 && allOff)
                    midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            });
        };
        const auto plainPedal = held (true, false, false, 1);
        check (plainPedal > -60.0, "round3 mpe: (control) the pedal holds a note on its own channel (" + juce::String (plainPedal, 1) + " dB)");
        const auto unheld = held (false, false), pedalled = held (true, false), stopped = held (true, true);
        check (unheld < -90.0, "round3 mpe: a released member note ends (" + juce::String (unheld, 1) + " dB)");
        check (pedalled > -60.0, "round3 mpe: the master channel's pedal holds a member note (" + juce::String (pedalled, 1) + " dB)");
        check (stopped < -90.0, "round3 mpe: All Notes Off on the master channel ends member notes (" + juce::String (stopped, 1) + " dB)");
    }

    // Mono -> Legato with two keys down: releasing the top one falls back to
    // the other key (the held-key list was cleared on the switch).
    {
        IlanaSynthAudioProcessor p;
        fresh (p);
        setPlain (p, "voice_mode", 1.0f); // Mono
        p.runAsyncUpdateForTest();
        const auto level = render (p, 60, 10, [&setPlain, &p] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0);
            if (block == 5)
                midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110), 0);
            if (block == 10)
                setPlain (p, "voice_mode", 2.0f); // Legato
            if (block == 20)
                midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        });
        check (level > -60.0, "round3 mono: switching Mono to Legato keeps the held key sounding (" + juce::String (level, 1) + " dB)");
    }

    // SPECTRAL FREEZE: what it holds keeps its pitch when the input stops
    // (each bin turned by the live input's phase advance, 0 in silence, so
    // the hold became a buzz at the hop rate, 94 Hz at 48 kHz).
    {
        SpectralFreeze freeze;
        freeze.prepare (48000.0);
        constexpr double hz = 1000.0;
        std::vector<float> left (256), right (256);
        int crossings = 0, samples = 0;
        auto last = 0.0f;
        double energy = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            const auto frozen = block >= 100;
            for (int i = 0; i < 256; ++i)
            {
                const auto t = (double) (block * 256 + i) / 48000.0;
                left[(size_t) i] = right[(size_t) i] = block < 120 ? (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * hz * t)) : 0.0f;
            }
            freeze.process (left.data(), right.data(), 256, frozen, 1.0f);
            if (block >= 200)
                for (int i = 0; i < 256; ++i)
                {
                    const auto x = left[(size_t) i];
                    crossings += (last < 0.0f && x >= 0.0f) ? 1 : 0;
                    last = x;
                    energy += (double) x * x;
                    ++samples;
                }
        }
        const auto heldHz = (double) crossings * 48000.0 / (double) juce::jmax (1, samples);
        check (energy / juce::jmax (1, samples) > 1.0e-4, "round3 freeze: the hold sounds after the input stops");
        check (std::abs (heldHz - hz) < 60.0, "round3 freeze: the hold keeps the note's pitch in silence (" + juce::String (heldHz, 0) + " Hz)");
    }

    // SPECTRAL FREEZE with HOLD already on (a patch loaded that way): it
    // holds the first sound that comes in, instead of passing the input.
    {
        SpectralFreeze freeze;
        freeze.prepare (48000.0);
        constexpr double hz = 1000.0;
        std::vector<float> left (256), right (256);
        int crossings = 0, samples = 0;
        auto last = 0.0f;
        double energy = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                const auto t = (double) (block * 256 + i) / 48000.0;
                const auto on = block >= 20 && block < 60;
                left[(size_t) i] = right[(size_t) i] = on ? (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * hz * t)) : 0.0f;
            }
            freeze.process (left.data(), right.data(), 256, true, 1.0f);
            if (block >= 200)
                for (int i = 0; i < 256; ++i)
                {
                    const auto x = left[(size_t) i];
                    crossings += (last < 0.0f && x >= 0.0f) ? 1 : 0;
                    last = x;
                    energy += (double) x * x;
                    ++samples;
                }
        }
        const auto heldHz = (double) crossings * 48000.0 / (double) juce::jmax (1, samples);
        check (energy / juce::jmax (1, samples) > 1.0e-4, "round3 freeze: HOLD on from the start holds the first sound");
        check (std::abs (heldHz - hz) < 60.0, "round3 freeze: ... at its pitch (" + juce::String (heldHz, 0) + " Hz)");
    }

    // Strings have room for A0 at every rate (the piano clamped it 39 cents
    // sharp at 48 kHz: its line holds about 2.4 periods).
    for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        check ((double) KarplusStrong::pianoBufferSizeFor (rate) / 2.4 >= rate / 27.5,
               "round3 strings: A0 fits the string buffer at " + juce::String (rate, 0) + " Hz");

    // A factory preset clears the patch's IR file; a saved one brings it back
    // (save, undo and EDITED: the PatchExtras row).
    {
        IlanaSynthAudioProcessor p;
        fresh (p);
        p.setReverbIrPath ("/no/such/ir.wav"); // (a missing file: kept by the patch, the reverb stays dry)
        juce::MemoryBlock state;
        p.getStateInformation (state);
        p.loadFactoryPreset (1);
        check (p.getReverbIrPath().isEmpty(), "round3 ir: a factory preset has no IR file");
        p.setStateInformation (state.getData(), (int) state.getSize());
        check (p.getReverbIrPath() == "/no/such/ir.wav", "round3 ir: the saved patch's IR file comes back");
    }
}
