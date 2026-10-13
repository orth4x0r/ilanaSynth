// M4: acoustic keys and the LFO pool.
double bandEnergy (const std::vector<float>& samples, size_t start, size_t end)
{
    auto total = 0.0;
    for (auto i = start; i < juce::jmin (end, samples.size()); ++i)
        total += (double) samples[i] * samples[i];
    return total / (double) juce::jmax ((size_t) 1, juce::jmin (end, samples.size()) - start);
}

double centroidOf (const std::vector<float>& samples, size_t start, double sampleRate)
{
    constexpr int order = 12;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work ((size_t) size * 2, 0.0f);
    for (int i = 0; i < size && start + (size_t) i < samples.size(); ++i)
        work[(size_t) i] = samples[start + (size_t) i]
                           * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
    fft.performFrequencyOnlyForwardTransform (work.data());
    double weighted = 0.0, total = 0.0;
    for (int bin = 1; bin < size / 2; ++bin)
    {
        weighted += bin * sampleRate / size * work[(size_t) bin];
        total += work[(size_t) bin];
    }
    return total > 0.0 ? weighted / total : 0.0;
}

void runM4Tests()
{
    std::cout << "M4 acoustic keys" << std::endl;

    {
        IlanaSynthAudioProcessor processor;
        for (const auto* id : { "osc1_excite", "osc2_excite", "sub_excite", "osc4_excite" })
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id));
            check (choice != nullptr && choice->getAllValueStrings().size() >= 7
                       && choice->getAllValueStrings()[4] == "Bow" && choice->getAllValueStrings()[5] == "Bright Hammer"
                       && choice->getAllValueStrings()[6] == "Osc In",
                   juce::String (id) + " appends Hammer and Osc In after Bow");
        }
    }

    // Hammer: velocity and felt hardness both brighten; velocity also louder.
    const auto hammer = [] (float velocity, float hardness, double frequency = 220.0)
    {
        KarplusStrong string (91);
        string.prepare (48000.0);
        string.setFrequency (frequency);
        string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.9f);
        string.setKeysParams (hardness, 0.0f);
        string.trigger (velocity);
        std::vector<float> samples (48000);
        for (auto& sample : samples)
            sample = string.process();
        return samples;
    };
    {
        const auto soft = hammer (0.3f, 0.5f);
        const auto loud = hammer (1.0f, 0.5f);
        const auto felt = hammer (0.8f, 0.0f);
        const auto hard = hammer (0.8f, 1.0f);
        const auto softLevel = std::sqrt (bandEnergy (soft, 0, 9600));
        const auto loudLevel = std::sqrt (bandEnergy (loud, 0, 9600));
        const auto softBright = centroidOf (soft, 200, 48000.0), loudBright = centroidOf (loud, 200, 48000.0);
        const auto feltBright = centroidOf (felt, 200, 48000.0), hardBright = centroidOf (hard, 200, 48000.0);
        std::cout << "  hammer rms soft " << softLevel << " loud " << loudLevel << ", centroid soft " << softBright
                  << " loud " << loudBright << " felt " << feltBright << " hard " << hardBright << std::endl;
        check (loudLevel > softLevel * 2.0, "a faster hammer is louder");
        check (loudBright > softBright * 1.15, "a faster hammer is brighter");
        check (hardBright > feltBright * 1.15, "a harder hammer is brighter");
        check (loudLevel > 0.05 && loudLevel < 2.0, "hammer level is in a sensible range (" + juce::String (loudLevel) + ")");
        auto finite = true;
        for (const auto& samples : { soft, loud, felt, hard })
            for (auto value : samples)
                finite = finite && std::isfinite (value);
        check (finite, "hammer output stays finite");

        // A reset string with the Hammer exciter is silent.
        KarplusStrong string (5);
        string.prepare (48000.0);
        string.setFrequency (110.0);
        string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.9f);
        string.reset();
        auto peak = 0.0f;
        for (int i = 0; i < 4800; ++i)
            peak = juce::jmax (peak, std::abs (string.process()));
        check (peak == 0.0f, "an untriggered hammer string stays silent");
    }

    // Bow (rebuilt in M4 on the STK friction table): a steady, pitched tone
    // at a level comparable to a pluck, that fades once released.
    {
        const auto bow = [] (float pressure, float speed, bool release)
        {
            KarplusStrong string (4242);
            string.prepare (48000.0);
            string.setFrequency (220.0);
            string.setParams (KarplusStrong::Excite::Bow, 0.0f, 0.3f, 0.9f);
            string.setBowAndBuzz (pressure, speed, 0.0f, 0.0f);
            string.trigger (0.8f);
            std::vector<float> samples (96000);
            for (int i = 0; i < 96000; ++i)
                samples[(size_t) i] = string.process (0.0f, ! release || i < 48000);
            return samples;
        };
        const auto held = bow (0.5f, 0.5f, false);
        const auto released = bow (0.5f, 0.5f, true);
        const auto level = std::sqrt (bandEnergy (held, 24000, 48000));
        const auto lateLevel = std::sqrt (bandEnergy (held, 72000, 96000));
        const std::vector<float> steady (held.begin() + 24000, held.begin() + 48000);
        const auto pitch = fundamentalOf (steady, 48000.0);
        const auto soft = std::sqrt (bandEnergy (bow (0.1f, 0.5f, false), 24000, 48000));
        const auto firm = std::sqrt (bandEnergy (bow (0.9f, 0.5f, false), 24000, 48000));
        std::cout << "  bow rms " << level << " late " << lateLevel << " pitch " << pitch << " Hz, pressure 0.1 "
                  << soft << " 0.9 " << firm << ", released " << std::sqrt (bandEnergy (released, 84000, 96000)) << std::endl;
        check (level > 0.05 && level < 1.5, "a bowed string sounds at a pluck-like level (" + juce::String (level) + ")");
        check (lateLevel > level * 0.5, "the bow sustains while held");
        check (std::abs (1200.0 * std::log2 (pitch / 220.0)) < 30.0, "a bowed string plays in tune (" + juce::String (pitch) + " Hz)");
        check (bandEnergy (released, 84000, 96000) < bandEnergy (held, 84000, 96000) * 0.01, "the bowed string fades after release");
        check (std::abs (firm - soft) > soft * 0.1, "bow pressure changes the tone");
    }

    // Piano stiffness: a hammered bass string's upper partials run sharp by
    // sqrt (1 + B n^2), and the fundamental stays in tune.
    {
        const auto partialRatio = [] (double f0, float stiffness, int n)
        {
            KarplusStrong string (8);
            string.prepare (48000.0);
            string.setFrequency (f0);
            string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.1f, 0.95f);
            string.setPhysicalParams (stiffness, 0.0f, 0.0f, 1.0f, 0.0f, false);
            string.setKeysParams (1.0f, 0.0f);
            string.trigger (1.0f);
            constexpr int order = 16;
            constexpr int size = 1 << order;
            std::vector<float> work ((size_t) size * 2, 0.0f);
            for (int i = 0; i < 2400; ++i)
                string.process();
            for (int i = 0; i < size; ++i)
                work[(size_t) i] = string.process() * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
            juce::dsp::FFT fft (order);
            fft.performFrequencyOnlyForwardTransform (work.data());
            const auto binHz = 48000.0 / size;
            // Strongest bin within +/-4 % of each harmonic slot, refined
            // with a parabola through its neighbours.
            const auto peakNear = [&] (double hz)
            {
                auto best = 0; auto bestValue = 0.0f;
                for (auto bin = (int) (hz * 0.96 / binHz); bin <= (int) (hz * 1.04 / binHz); ++bin)
                    if (work[(size_t) bin] > bestValue) { bestValue = work[(size_t) bin]; best = bin; }
                const auto l = work[(size_t) best - 1], c = work[(size_t) best], r = work[(size_t) best + 1];
                const auto shift = 0.5 * (l - r) / juce::jmax (1.0e-9, (double) (l - 2.0f * c + r));
                return (best + juce::jlimit (-0.5, 0.5, shift)) * binHz;
            };
            // Against the nominal pitch: at 41 Hz the FFT places the
            // fundamental itself too coarsely to divide by. Tuning is
            // judged on the 4th partial, which stiffness barely moves.
            return std::make_pair (peakNear (4 * f0) / 4.0, peakNear (n * f0) / (n * f0));
        };
        const auto stiff = partialRatio (41.2, 0.5f, 10);
        const auto plain = partialRatio (41.2, 0.0f, 10);
        const auto expected = std::sqrt (1.0 + 3.16e-4 * 100.0) / std::sqrt (1.0 + 3.16e-4);
        std::cout << "  piano stiffness E1: 10th partial ratio " << stiff.second << " (expected " << expected
                  << ", plain " << plain.second << "), 4th partial / 4 " << stiff.first << " Hz" << std::endl;
        check (std::abs (stiff.second - expected) < 0.008, "a stiff piano string's partials stretch like a real string's");
        check (std::abs (plain.second - 1.0) < 0.004, "with no stiffness the partials stay harmonic");
        check (std::abs (1200.0 * std::log2 (stiff.first / 41.2)) < 20.0, "the stiff string stays in tune");
    }

    // Damper: once the key is up the string stops quickly.
    {
        const auto render = [] (float damper)
        {
            KarplusStrong string (17);
            string.prepare (48000.0);
            string.setFrequency (261.63);
            string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.95f);
            string.setKeysParams (0.5f, damper);
            string.trigger (0.8f);
            std::vector<float> samples (48000);
            for (int i = 0; i < 48000; ++i)
                samples[(size_t) i] = string.process (0.0f, i < 9600);
            return samples;
        };
        const auto free = render (0.0f);
        const auto damped = render (1.0f);
        const auto ratio = bandEnergy (damped, 24000, 28800) / juce::jmax (1.0e-12, bandEnergy (free, 24000, 28800));
        std::cout << "  damper energy ratio 0.3 s after release " << ratio << std::endl;
        check (ratio < 1.0e-3, "the damper stops the string after release (-30 dB)");
        check (std::abs (bandEnergy (free, 0, 9600) - bandEnergy (damped, 0, 9600)) < 1.0e-9,
               "the damper does nothing while the key is held");
    }

    // Coupled strings: the in-phase sound dies fast, the detuned aftersound rings on.
    {
        const auto render = [] (float couple)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "osc1_mode", 1.0f);
            setParam (processor, "osc1_excite", 5.0f);
            setParam (processor, "osc1_unison", 3.0f);
            setParam (processor, "osc1_detune", 2.0f);
            setParam (processor, "osc1_couple", couple);
            setParam (processor, "osc1_string_decay", 0.97f);
            setParam (processor, "osc1_string_damp", 0.2f);
            setParam (processor, "osc2_on", 0.0f);
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "subosc_on", 0.0f);
            setParam (processor, "amp_sustain", 1.0f);
            setParam (processor, "f1_cutoff", 20000.0f);
            std::vector<float> samples;
            juce::AudioBuffer<float> buffer (2, 512);
            for (int block = 0; block < 94 * 3; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                    samples.push_back (buffer.getSample (0, i));
            }
            return samples;
        };
        const auto slope = [] (const std::vector<float>& s, size_t a, size_t b)
        {
            // dB per second between two 50 ms windows.
            const auto ea = bandEnergy (s, a, a + 2400), eb = bandEnergy (s, b, b + 2400);
            return 10.0 * std::log10 (juce::jmax (1.0e-20, eb) / juce::jmax (1.0e-20, ea)) / ((double) (b - a) / 48000.0);
        };
        const auto plain = render (0.0f);
        const auto coupled = render (1.0f);
        const auto early = slope (coupled, 2400, 19200), late = slope (coupled, 72000, 120000);
        const auto plainEarly = slope (plain, 2400, 19200);
        std::cout << "  coupled decay early " << early << " dB/s, late " << late << " dB/s, uncoupled early "
                  << plainEarly << " dB/s" << std::endl;
        check (early < late - 6.0, "coupled strings decay in two stages (fast prompt sound, slow aftersound)");
        check (early < plainEarly - 3.0, "coupling speeds up the prompt decay");
        check (bandEnergy (coupled, 120000, 144000) > bandEnergy (coupled, 2400, 26400) * 1.0e-8,
               "the aftersound is still ringing after 2.5 s (within 80 dB of the attack)");
    }

    // Stretch tuning: the top of the keyboard is sharp, the middle unchanged.
    {
        const auto pitch = [] (int note, float stretch)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "stretch", stretch);
            setParam (processor, "osc2_on", 0.0f);
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "subosc_on", 0.0f);
            setParam (processor, "osc1_table", 0.0f);
            setParam (processor, "f1_cutoff", 20000.0f);
            double centroid = 0.0;
            std::vector<float> samples;
            renderPeakAndCentroid (processor, note, 40, centroid, &samples);
            return fundamentalOf (samples, 48000.0);
        };
        const auto top = pitch (96, 1.0f), topPlain = pitch (96, 0.0f);
        const auto middle = pitch (60, 1.0f), middlePlain = pitch (60, 0.0f);
        const auto topCents = 1200.0 * std::log2 (top / topPlain);
        const auto middleCents = 1200.0 * std::log2 (middle / middlePlain);
        std::cout << "  stretch: C7 " << topCents << " cents, C4 " << middleCents << " cents" << std::endl;
        check (topCents > 15.0 && topCents < 40.0, "full stretch tuning raises C7 by 15-40 cents");
        check (std::abs (middleCents) < 2.0, "stretch tuning leaves middle C alone");
    }

    // Soundboard, pedal resonance and mechanical noises (processor-level).
    const auto renderKeys = [] (std::function<void (IlanaSynthAudioProcessor&)> setup,
                                std::function<void (int, juce::MidiBuffer&)> events, int blocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "osc1_mode", 1.0f);
        setParam (processor, "osc1_excite", 5.0f);
        setParam (processor, "osc2_on", 0.0f);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "amp_release", 0.05f);
        setup (processor);
        std::vector<float> samples;
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            events (block, midi);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
                samples.push_back (buffer.getSample (0, i));
        }
        return samples;
    };
    const auto noteThenOff = [] (int block, juce::MidiBuffer& midi)
    {
        if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (block == 20) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    };
    {
        const auto dry = renderKeys ([] (auto&) {}, noteThenOff, 40);
        const auto board = renderKeys ([] (auto& p) { setParam (p, "sb_on", 1.0f); setParam (p, "sb_mix", 1.0f); },
                                       noteThenOff, 40);
        auto difference = 0.0;
        auto finite = true;
        for (size_t i = 0; i < dry.size(); ++i)
        {
            difference += std::abs ((double) dry[i] - board[i]);
            finite = finite && std::isfinite (board[i]);
        }
        check (difference > 1.0 && finite, "the soundboard colours the sound and stays finite");
        const auto silent = renderKeys ([] (auto& p) { setParam (p, "sb_on", 1.0f); }, [] (int, auto&) {}, 20);
        check (bandEnergy (silent, 0, silent.size()) == 0.0, "the soundboard is silent with no input");

        // Brightness: energy of the first difference (a gentle high-pass)
        // relative to the whole signal.
        const auto darkBright = [&] (float tone)
        {
            const auto samples = renderKeys ([tone] (auto& p)
            {
                setParam (p, "sb_on", 1.0f);
                setParam (p, "sb_tone", tone);
                setParam (p, "osc1_hammer_hard", 1.0f);
            }, noteThenOff, 20);
            auto edges = 0.0, total = 0.0;
            for (size_t i = 1; i < samples.size(); ++i)
            {
                edges += std::pow ((double) samples[i] - samples[i - 1], 2.0);
                total += (double) samples[i] * samples[i];
            }
            return edges / juce::jmax (1.0e-20, total);
        };
        const auto dark = darkBright (0.0f), bright = darkBright (1.0f);
        std::cout << "  soundboard tone brightness dark " << dark << " bright " << bright << std::endl;
        check (bright > dark * 3.0, "soundboard TONE opens the lid");
    }
    {
        const auto pedalEvents = [] (bool pedal)
        {
            return [pedal] (int block, juce::MidiBuffer& midi)
            {
                if (block == 0 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                if (block == 2) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
                if (block == 12) midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
                if (block == 12) midi.addEvent (juce::MidiMessage::allNotesOff (1), 1);
                if (block == 100 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
            };
        };
        // Make the voice itself short so only the resonance is left afterwards.
        const auto setup = [] (float amount)
        {
            return [amount] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "pedal_res", amount);
                setParam (p, "osc1_damper", 1.0f);
            };
        };
        const auto withPedal = renderKeys (setup (1.0f), pedalEvents (true), 160);
        const auto noResonance = renderKeys (setup (0.0f), pedalEvents (true), 160);
        const auto window = [] (int block) { return (size_t) block * 512; };
        const auto tail = bandEnergy (withPedal, window (60), window (90));
        const auto tailOff = bandEnergy (noResonance, window (60), window (90));
        const auto afterLift = bandEnergy (withPedal, window (140), window (160));
        std::cout << "  pedal resonance tail " << tail << " (without " << tailOff << "), after lift " << afterLift << std::endl;
        check (tail > tailOff * 10.0 && tail > 1.0e-9, "with the pedal down the strings ring in sympathy");
        check (afterLift < tail * 0.01, "lifting the pedal damps the sympathetic strings");
    }
    {
        const auto keysOnly = [] (const char* id)
        {
            return [id] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc1_on", 0.0f); // silence the voice: only the mechanism is left
                setParam (p, id, 1.0f);
            };
        };
        const auto quiet = renderKeys ([] (auto& p) { setParam (p, "osc1_on", 0.0f); }, noteThenOff, 30);
        const auto keyNoise = renderKeys (keysOnly ("mech_key"), noteThenOff, 30);
        const auto damperNoise = renderKeys (keysOnly ("mech_damper"), noteThenOff, 30);
        const auto pedalNoise = renderKeys (keysOnly ("mech_pedal"), [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 1) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (block == 10) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        }, 30);
        const auto window = [] (int block) { return (size_t) block * 512; };
        check (bandEnergy (quiet, 0, quiet.size()) == 0.0, "no mechanical noise when the levels are zero");
        check (bandEnergy (keyNoise, window (20), window (24)) > 1.0e-7, "key release makes a thock");
        check (bandEnergy (damperNoise, window (20), window (26)) > 1.0e-8, "the damper lands with a felt noise");
        check (bandEnergy (pedalNoise, window (1), window (6)) > 1.0e-7
                   && bandEnergy (pedalNoise, window (10), window (16)) > 1.0e-7,
               "the pedal mechanism sounds on press and release");
        // The output's DC blocker leaves a short sub-audio tail after the
        // thock, so the audible part is measured (the first difference, which
        // removes it): the noise itself must be gone, 60 dB under the thock.
        std::vector<float> keyNoiseDiff (keyNoise.size(), 0.0f);
        for (size_t i = 1; i < keyNoise.size(); ++i)
            keyNoiseDiff[i] = keyNoise[i] - keyNoise[i - 1];
        const auto thock = bandEnergy (keyNoiseDiff, window (20), window (24));
        const auto after = bandEnergy (keyNoiseDiff, window (28), window (30));
        check (after < thock * 1.0e-6, "mechanical noises end (" + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, after / juce::jmax (1.0e-30, thock))), 1)
                                           + " dB under the thock)");
    }

    // Osc In: the string is driven by what the FM matrix feeds it.
    {
        const auto render = [&] (float route)
        {
            return renderKeys ([route] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc1_excite", 6.0f);
                setParam (p, "osc1_string_sustain", 0.8f);
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_out", 0.0f);
                setParam (p, "fm_amount", route); // OSC 2 -> OSC 1
            }, [] (int block, juce::MidiBuffer& midi)
            {
                if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            }, 30);
        };
        const auto driven = bandEnergy (render (0.8f), 5000, 15000);
        const auto idle = bandEnergy (render (0.0f), 5000, 15000);
        std::cout << "  osc in: driven " << driven << " idle " << idle << std::endl;
        check (driven > 1.0e-5 && driven > idle * 100.0, "an Osc In string is played by the FM matrix input");
    }

    // Register map and all keys features together: a full piano stays finite.
    {
        const auto piano = renderKeys ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_unison", 3.0f);
            setParam (p, "osc1_detune", 1.5f);
            setParam (p, "osc1_couple", 1.0f);
            setParam (p, "osc1_register", 1.0f);
            setParam (p, "osc1_damper", 0.7f);
            setParam (p, "osc1_hammer_hard", 1.0f);
            setParam (p, "stretch", 1.0f);
            setParam (p, "sb_on", 1.0f);
            setParam (p, "pedal_res", 1.0f);
            for (const auto* id : { "mech_key", "mech_damper", "mech_pedal" })
                setParam (p, id, 1.0f);
        }, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (block % 4 == 0 && block < 80)
                midi.addEvent (juce::MidiMessage::noteOn (1, 21 + (block * 7) % 88, (juce::uint8) 127), 0);
            if (block % 4 == 2 && block < 80)
                midi.addEvent (juce::MidiMessage::noteOff (1, 21 + ((block - 2) * 7) % 88), 0);
            if (block == 90) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        }, 140);
        auto finite = true;
        auto peak = 0.0f;
        for (auto value : piano)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }
        std::cout << "  full piano peak " << peak << std::endl;
        check (finite && peak < 4.0 && peak > 0.01, "a full piano across the keyboard stays finite and bounded");
    }

    // LFO pool: LFO 7 routed to the cutoff moves the sound; its state saves.
    {
        const auto render = [] (float depth)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "lfo7_rate", 3.0f);
            setParam (processor, "mod1_src", (float) Mod::Source::Lfo7);
            setParam (processor, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
            setParam (processor, "mod1_amt", depth);
            setParam (processor, "f1_cutoff", 800.0f);
            double centroid = 0.0;
            std::vector<float> samples;
            renderPeakAndCentroid (processor, 48, 100, centroid, &samples);
            return samples;
        };
        const auto still = render (0.0f), moving = render (0.8f);
        auto difference = 0.0;
        for (size_t i = 0; i < still.size(); ++i)
            difference += std::abs ((double) still[i] - moving[i]);
        check (difference > 10.0, "LFO 7 modulates the filter when routed");
        check (Mod::lfoIndexFor (Mod::Source::Lfo16) == 15 && Mod::lfoSourceFor (15) == Mod::Source::Lfo16
                   && Mod::lfoRateDestinationFor (4) == Mod::Destination::Lfo5Rate,
               "LFO pool index helpers agree");

        IlanaSynthAudioProcessor processor;
        processor.setLfoCurve (9, LfoCurve::preset (5));
        processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, 9, true);
        setParam (processor, "lfo10_shape", 8.0f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getLfoCurve (9).toString() == LfoCurve::preset (5).toString()
                   && restored.isRevealed (IlanaSynthAudioProcessor::Module::Lfo, 9)
                   && restored.apvts.getRawParameterValue ("lfo10_shape")->load() == 8.0f,
               "LFO 10's curve, shape and card survive a state round trip");
        check (restored.isLfoShown (9) && ! restored.isLfoShown (10), "only added or routed LFOs are shown");
    }
}

// ILANA_NOTE_DEBUG=<preset>: E1, C4 and C7 held for 6 s at mf and ff, dry
// (no effects), written to build/note-debug/ for tools/analyse_note.py and
// tools/fit_piano.py (compared with real piano notes in build/reference/).
// ILANA_PIANO_TUNING="name=value;..." overrides PianoTuning;
// ILANA_PRESET_OVERRIDES="param=value;..." overrides preset parameters.
void debugPresetNotes (const juce::String& presetName)
{
    const auto tuning = juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO_TUNING", "");
    if (! PianoTuning::get().apply (tuning))
        std::cout << "unknown tuning name in: " << tuning << std::endl;
    if (! PianoModelTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO2_TUNING", "")))
        std::cout << "unknown piano2 tuning name" << std::endl;
    const auto overrides = juce::StringArray::fromTokens (
        juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_OVERRIDES", ""), ";", "");
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable (
        "ILANA_NOTE_FOLDER", juce::File::getCurrentWorkingDirectory().getChildFile ("build/note-debug").getFullPathName()));
    folder.createDirectory();

    // ILANA_NOTE_SET=ep: the electric piano set instead (MIDI 40, 60 and 84 at
    // velocity 40 and 120, released after 4 s), named like the ilanaRefHost
    // references (ours.v40.n60.wav).
    const auto electric = juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SET", "") == "ep";
    if (! EpTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_EP_TUNING", "")))
        std::cout << "unknown EP tuning name" << std::endl;
    struct NoteRender { juce::String file; int note, velocity; };
    std::vector<NoteRender> renders;
    if (electric)
    {
        for (int velocity : { 40, 120 })
            for (int note : { 40, 60, 84 })
                renders.push_back ({ "ours.v" + juce::String (velocity) + ".n" + juce::String (note) + ".wav", note, velocity });
    }
    else if (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SET", "") == "keyboard")
    {
        // M8.2: the whole keyboard (the Salamander grand's notes, a minor
        // third apart from A0) at pp, mf and ff, named ours.<dyn>.n<midi>.wav.
        // ILANA_NOTE_LIST="21,60,..." and ILANA_NOTE_DYNAMICS="mf,ff" narrow it.
        auto notes = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_LIST", ""), ",", "");
        if (notes.isEmpty())
            for (int note = 21; note <= 108; note += 3)
                notes.add (juce::String (note));
        auto dynamics = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_DYNAMICS", "pp,mf,ff"), ",", "");
        for (const auto& dynamic : dynamics)
        {
            const auto velocity = dynamic == "pp" ? 20 : dynamic == "mf" ? 60 : 124;
            for (const auto& note : notes)
                renders.push_back ({ "ours." + dynamic + ".n" + note.trim() + ".wav", note.getIntValue(), velocity });
        }
    }
    else
    {
        for (const auto& [dynamic, velocity] : { std::pair<const char*, int> { "mf", 80 }, { "ff", 120 } })
            for (const auto& [noteName, note] : { std::pair<const char*, int> { "E1", 28 }, { "C4", 60 }, { "C7", 96 } })
                renders.push_back ({ juce::String ("ours.") + dynamic + "." + noteName + ".wav", note, velocity });
    }
    const auto totalBlocks = electric ? 500 : juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SECONDS", "6").getIntValue() * 100;
    const auto releaseBlock = electric ? 400 : -1;

    for (const auto& render : renders)
        {
            const auto note = render.note;
            const auto velocity = render.velocity;
            IlanaSynthAudioProcessor processor;
            const auto index = processor.getFactoryPresetNames().indexOf (presetName);
            if (index < 0) { std::cout << "no preset " << presetName << std::endl; return; }
            processor.loadFactoryPreset (index);
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                processor.assignFxSlot (slot, 0);
            for (const auto& item : overrides)
                if (item.contains ("="))
                    setParam (processor, item.upToFirstOccurrenceOf ("=", false, false).trim(),
                              item.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            processor.prepareToPlay (48000.0, 480);
            juce::AudioBuffer<float> buffer (2, 480);
            juce::AudioBuffer<float> output (1, 480 * totalBlocks);
            for (int block = 0; block < totalBlocks; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
                if (block == releaseBlock) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                processor.processBlock (buffer, midi);
                output.copyFrom (0, block * 480, buffer, 0, 0, 480);
                output.addFrom (0, block * 480, buffer, 1, 0, 480);
            }
            output.applyGain (0.5f);
            const auto file = folder.getChildFile (render.file);
            file.deleteFile();
            juce::WavAudioFormat format;
            if (auto stream = file.createOutputStream())
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
                }
        }
}

// ILANA_RENDER_DEMO=<folder>: renders the M4 Keys presets to .wav files for
// listening (a pedalled arpeggio and chord, then a low and a high note).
void renderKeysDemos (const juce::File& folder)
{
    folder.createDirectory();
    const double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    // The Keys presets, plus A/B pairs for the sympathetic strings and the
    // pedal resonance.
    struct Demo { juce::String file, preset; std::vector<std::pair<const char*, float>> settings; };
    std::vector<Demo> demos;
    for (const auto* name : { "Hammered Strings", "Pedal Bloom", "Honky Hammers", "Bolted Strings",
                              "Bowed Board", "Osc-Struck Strings", "Tine Keys", "Reed Keys" })
        demos.push_back ({ name, name, {} });
    demos.push_back ({ "AB Sym off - Pizzicato", "Pizzicato", {} });
    demos.push_back ({ "AB Sym on - Pizzicato", "Pizzicato",
                       { { "sym_on", 1.0f }, { "sym_amount", 0.8f }, { "sym_count", 6.0f }, { "sym_decay", 0.8f } } });
    demos.push_back ({ "AB Pedal res off - Pedal Bloom", "Pedal Bloom", { { "pedal_res", 0.0f } } });
    demos.push_back ({ "AB Pedal res on - Pedal Bloom", "Pedal Bloom", {} });

    for (const auto& demo : demos)
    {
        const auto* name = demo.file.toRawUTF8();
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf (demo.preset);
        if (index < 0)
            continue;

        processor.loadFactoryPreset (index);
        for (const auto& [id, value] : demo.settings)
            setParam (processor, id, value);
        processor.prepareToPlay (sampleRate, blockSize);
        const auto seconds = 9.0;
        juce::AudioBuffer<float> output (2, (int) (seconds * sampleRate));
        juce::AudioBuffer<float> buffer (2, blockSize);
        const auto at = [sampleRate] (double time) { return (int) (time * sampleRate); };

        struct Event { double time; juce::MidiMessage message; };
        std::vector<Event> events {
            { 0.0, juce::MidiMessage::controllerEvent (1, 64, 127) },
            { 0.05, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 70) },
            { 0.35, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 80) },
            { 0.65, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90) },
            { 0.95, juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100) },
            { 1.25, juce::MidiMessage::noteOn (1, 76, (juce::uint8) 110) },
            { 1.3, juce::MidiMessage::noteOff (1, 48) }, { 1.3, juce::MidiMessage::noteOff (1, 55) },
            { 1.3, juce::MidiMessage::noteOff (1, 64) }, { 1.3, juce::MidiMessage::noteOff (1, 72) },
            { 1.6, juce::MidiMessage::noteOff (1, 76) },
            { 3.4, juce::MidiMessage::controllerEvent (1, 64, 0) },
            { 3.8, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 120) }, { 3.8, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 115) },
            { 3.8, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110) },
            { 5.3, juce::MidiMessage::noteOff (1, 57) }, { 5.3, juce::MidiMessage::noteOff (1, 60) },
            { 5.3, juce::MidiMessage::noteOff (1, 64) },
            { 5.8, juce::MidiMessage::noteOn (1, 28, (juce::uint8) 110) }, { 7.0, juce::MidiMessage::noteOff (1, 28) },
            { 7.2, juce::MidiMessage::noteOn (1, 96, (juce::uint8) 90) }, { 8.2, juce::MidiMessage::noteOff (1, 96) },
        };

        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            const auto length = juce::jmin (blockSize, output.getNumSamples() - start);
            buffer.setSize (2, length, false, false, true);
            buffer.clear();
            juce::MidiBuffer midi;
            for (const auto& event : events)
                if (at (event.time) >= start && at (event.time) < start + length)
                    midi.addEvent (event.message, at (event.time) - start);
            processor.processBlock (buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom (channel, start, buffer, channel, 0, length);
        }

        // Levels per section, DC and the largest sample step (clicks).
        {
            const auto rmsDb = [&] (double from, double to)
            {
                auto total = 0.0;
                for (int i = at (from); i < at (to); ++i)
                {
                    const auto mid = 0.5 * (output.getSample (0, i) + output.getSample (1, i));
                    total += mid * mid;
                }
                return 10.0 * std::log10 (total / juce::jmax (1, at (to) - at (from)) + 1.0e-24);
            };
            auto dc = 0.0, step = 0.0;
            for (int i = 1; i < output.getNumSamples(); ++i)
            {
                dc += output.getSample (0, i);
                step = juce::jmax (step, (double) std::abs (output.getSample (0, i) - output.getSample (0, i - 1)));
            }
            std::cout << name << ": dc " << dc / output.getNumSamples() << " max step " << step
                      << " | arp " << rmsDb (0.1, 1.3) << " pedal-held " << rmsDb (2.4, 3.3) << " after-lift " << rmsDb (3.55, 3.75)
                      << " chord " << rmsDb (3.9, 5.2) << " low " << rmsDb (5.9, 6.9) << " high " << rmsDb (7.3, 8.1)
                      << " tail " << rmsDb (8.6, 9.0) << " dB" << std::endl;
        }

        const auto file = folder.getChildFile (juce::String (name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                    format.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
                std::cout << "wrote " << file.getFullPathName() << "  peak " << output.getMagnitude (0, output.getNumSamples())
                          << std::endl;
            }
    }
}

// Level (dB RMS) while a note is held (0.1-0.5 s) and after its release
// (1-3 s), optionally with the sustain pedal down throughout.
std::pair<double, double> heldAndTail (const juce::String& preset, int note, bool pedal,
                                       const std::vector<std::pair<const char*, float>>& settings)
{
    IlanaSynthAudioProcessor processor;
    if (preset.isNotEmpty())
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (preset));
    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        processor.assignFxSlot (slot, 0);
    for (const auto& [id, value] : settings)
        setParam (processor, id, value);
    processor.prepareToPlay (48000.0, 480);
    juce::AudioBuffer<float> buffer (2, 480);
    double held = 0.0, tail = 0.0;
    for (int block = 0; block < 300; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        if (block == 1) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (block == 50) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        processor.processBlock (buffer, midi);
        const auto rms = buffer.getRMSLevel (0, 0, 480);
        if (block >= 10 && block < 50) held += rms * rms / 40.0;
        if (block >= 100 && block < 300) tail += rms * rms / 200.0;
    }
    return { juce::Decibels::gainToDecibels (std::sqrt (held), -200.0),
             juce::Decibels::gainToDecibels (std::sqrt (tail), -200.0) };
}

// The resonance must actually be heard: exact tuning of the string loops,
// SYM strings ringing after a note on their pitch (and not after one off it),
// and the pedal resonance adding a halo.
void runSympatheticResonanceTest()
{
    for (const auto partial : { 2.0, 2.0 * std::exp2 (6.0 / 1200.0) })
        for (const auto frequency : { 65.41, 246.94, 1046.5 })
        {
            TunedString string;
            string.allocate (900);
            string.tune (48000.0, frequency, 9.0, 0.55f, partial);
            std::vector<float> ring (48000);
            for (int i = 0; i < (int) ring.size(); ++i)
                ring[(size_t) i] = string.process (i == 0 ? 1.0f : 0.0f);
            const auto peakCents = [&ring] (double centre)
            {
                auto best = 0.0, bestPower = -1.0;
                for (double cents = -10.0; cents <= 10.0; cents += 0.1)
                {
                    const auto hz = centre * std::exp2 (cents / 1200.0);
                    double re = 0.0, im = 0.0;
                    for (int i = 4800; i < (int) ring.size(); ++i)
                    {
                        const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
                        const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - 4800)
                                                                  / (double) (ring.size() - 4800));
                        re += window * ring[(size_t) i] * std::cos (angle);
                        im += window * ring[(size_t) i] * std::sin (angle);
                    }
                    if (re * re + im * im > bestPower) { bestPower = re * re + im * im; best = cents; }
                }
                return best;
            };
            // Rings for its T60 (9 s: -4.7 dB between 0.2 s and 0.9 s), treble too.
            const auto rmsAt = [&ring, frequency] (int start)   // the fundamental alone
            {
                double re = 0.0, im = 0.0;
                for (int i = start; i < start + 4800; ++i)
                {
                    const auto angle = juce::MathConstants<double>::twoPi * frequency * (double) i / 48000.0;
                    const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - start) / 4800.0);
                    re += window * ring[(size_t) i] * std::cos (angle);
                    im += window * ring[(size_t) i] * std::sin (angle);
                }
                return 10.0 * std::log10 (re * re + im * im + 1e-30);
            };
            const auto drop = rmsAt (9600) - rmsAt (43200);
            check (drop > 4.0 && drop < 5.5, "tuned string " + juce::String (frequency) + " Hz rings for its T60 (drop "
                                                 + juce::String (drop, 1) + " dB, expected 4.7)");

            // (The second partial is only placed for the pedal strings, below 250 Hz.)
            const auto first = peakCents (frequency), second = frequency < 500.0 ? peakCents (frequency * partial) : 0.0;
            check (std::abs (first) < 0.3 && std::abs (second) < 0.3,
                   "tuned string " + juce::String (frequency) + " Hz rings in tune (" + juce::String (first, 1)
                       + " cents; 2nd partial " + juce::String (second, 1) + " cents off its target)");
        }

    const std::vector<std::pair<const char*, float>> dry { { "amp_release", 0.02f } };
    auto withSym = dry;
    withSym.insert (withSym.end(), { { "sym_on", 1.0f }, { "sym_amount", 1.0f }, { "sym_count", 6.0f } });
    const auto onPitch = heldAndTail ("", 60, false, withSym);
    const auto offPitch = heldAndTail ("", 61, false, withSym);
    const auto dryNote = heldAndTail ("", 60, false, dry);
    check (onPitch.second > onPitch.first - 20.0 && offPitch.second < onPitch.second - 25.0
               && dryNote.second < -150.0,
           "SYM strings ring on after a note on their pitch (" + juce::String (onPitch.second - onPitch.first, 1)
               + " dB under the note), not after one off it (" + juce::String (offPitch.second - offPitch.first, 1) + " dB)");

    const auto pedalOff = heldAndTail ("Hammered Strings", 60, true, { { "pedal_res", 0.0f } });
    const auto pedalOn = heldAndTail ("Hammered Strings", 60, true, { { "pedal_res", 1.0f } });
    check (pedalOn.second > pedalOff.second + 5.0 && pedalOn.first < pedalOff.first + 3.0,
           "pedal resonance adds a halo after the note (+" + juce::String (pedalOn.second - pedalOff.second, 1)
               + " dB), not while it sounds (+" + juce::String (pedalOn.first - pedalOff.first, 1) + " dB)");
}
