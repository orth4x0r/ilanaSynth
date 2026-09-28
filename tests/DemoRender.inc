// M8 listening demos (ILANA_RENDER_DEMO=<folder>). Included by TableTest.cpp.

namespace Demo
{
struct Event
{
    double time;
    juce::MidiMessage message;
};

// Renders the default patch (or a factory preset) with `settings` over the
// events and writes <folder>/<name>.wav (48 kHz, 24-bit stereo). Returns the
// peak level.
float render (const juce::File& folder, const juce::String& name, const juce::String& preset,
              const std::vector<std::pair<juce::String, float>>& settings, const std::vector<Event>& events, double seconds,
              std::function<void (IlanaSynthAudioProcessor&, double)> perBlock = nullptr)
{
    folder.createDirectory();
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    IlanaSynthAudioProcessor processor;
    if (preset.isNotEmpty())
        if (const auto index = processor.getFactoryPresetNames().indexOf (preset); index >= 0)
            processor.loadFactoryPreset (index);
    for (const auto& [id, value] : settings)
        setParam (processor, id, value);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> output (2, (int) (seconds * sampleRate));
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto at = [] (double time) { return (int) (time * sampleRate); };

    for (int start = 0; start < output.getNumSamples(); start += blockSize)
    {
        const auto length = juce::jmin (blockSize, output.getNumSamples() - start);
        buffer.setSize (2, length, false, false, true);
        buffer.clear();
        juce::MidiBuffer midi;
        for (const auto& event : events)
            if (at (event.time) >= start && at (event.time) < start + length)
                midi.addEvent (event.message, at (event.time) - start);
        if (perBlock != nullptr)
            perBlock (processor, (double) start / sampleRate);
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom (channel, start, buffer, channel, 0, length);
    }

    const auto peak = output.getMagnitude (0, output.getNumSamples());
    const auto file = folder.getChildFile (name + ".wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
        }
    std::cout << "demo " << name << ": peak " << juce::Decibels::gainToDecibels (peak) << " dB" << std::endl;
    return peak;
}

std::vector<Event> held (std::initializer_list<int> notes, double from, double to, int velocity = 100)
{
    std::vector<Event> events;
    for (auto note : notes)
    {
        events.push_back ({ from, juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity) });
        events.push_back ({ to, juce::MidiMessage::noteOff (1, note) });
    }
    return events;
}

std::vector<std::pair<juce::String, float>> route (int slot, Mod::Source source, int destination, float amount)
{
    const auto prefix = "mod" + juce::String (slot);
    return { { prefix + "_src", (float) source }, { prefix + "_dst", (float) destination }, { prefix + "_amt", amount } };
}

std::vector<Event> operator+ (std::vector<Event> a, const std::vector<Event>& b)
{
    a.insert (a.end(), b.begin(), b.end());
    return a;
}

std::vector<std::pair<juce::String, float>> operator+ (std::vector<std::pair<juce::String, float>> a,
                                                       const std::vector<std::pair<juce::String, float>>& b)
{
    a.insert (a.end(), b.begin(), b.end());
    return a;
}

std::vector<std::pair<juce::String, float>> shapeDefaults (int lfo, int shape)
{
    std::vector<std::pair<juce::String, float>> settings { { "lfo" + juce::String (lfo) + "_shape", (float) shape } };
    const auto& info = LfoSimInfo::get (shape);
    for (int param = 0; param < LfoSimInfo::numParams; ++param)
        settings.push_back ({ "lfo" + juce::String (lfo) + "_p" + juce::String (param + 1), info.params[(size_t) param].defaultValue });
    return settings;
}

// M8.1: the new modulators on a plain saw chord through the filter.
void renderM81 (const juce::File& folder)
{
    using D = Mod::Destination;
    const std::vector<std::pair<juce::String, float>> base { { "f1_cutoff", 900.0f }, { "f1_reso", 0.45f } };
    const auto chord = held ({ 48, 55, 60, 64 }, 0.05, 9.5);

    // Bounce: the ball's impacts strike the filter, retriggered per note.
    {
        auto events = held ({ 48, 60 }, 0.05, 3.0);
        for (const auto& e : held ({ 50, 62 }, 3.2, 6.0)) events.push_back (e);
        for (const auto& e : held ({ 43, 55 }, 6.2, 9.5)) events.push_back (e);
        render (folder, "01 Bounce - impacts on cutoff", "",
                base + shapeDefaults (1, LfoSimShapes::Bounce)
                    + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 1.5f } }
                    + route (1, Mod::Source::Lfo1B, (int) D::Filter1Cutoff, 0.8f) + route (2, Mod::Source::Lfo1, (int) D::Pan, 0.3f),
                events, 10.0);
    }
    render (folder, "02 Pendulum - swing on pan, speed on cutoff", "",
            base + shapeDefaults (1, LfoSimShapes::Pendulum)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 1.0f }, { "lfo1_p4", 0.6f }, { "lfo1_p3", 0.15f } }
                + route (1, Mod::Source::Lfo1, (int) D::Pan, 0.9f) + route (2, Mod::Source::Lfo1B, (int) D::Filter1Cutoff, 0.5f),
            chord, 10.0);
    render (folder, "03 Spring - plucked every beat", "",
            base + shapeDefaults (1, LfoSimShapes::Spring)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_trigger", 2.0f }, { "lfo1_div", 2.0f }, { "lfo1_rate", 1.0f },
                                                                { "lfo1_p2", 0.75f } }
                + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.5f),
            chord, 10.0);
    render (folder, "04 Friction - stick-slip on cutoff", "",
            base + shapeDefaults (1, LfoSimShapes::Friction)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 2.0f } }
                + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.5f) + route (2, Mod::Source::Lfo1B, (int) D::AmpLevel, -0.3f),
            chord, 10.0);
    render (folder, "05 Lorenz - X on cutoff, Y on pan", "",
            base + shapeDefaults (1, LfoSimShapes::Lorenz)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 0.4f } }
                + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.6f) + route (2, Mod::Source::Lfo1B, (int) D::Pan, 0.7f),
            chord, 10.0);
    render (folder, "06 Double pendulum - tip on cutoff and pan", "",
            base + shapeDefaults (1, LfoSimShapes::DoublePendulum)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 1.0f } }
                + route (1, Mod::Source::Lfo1, (int) D::Pan, 0.8f) + route (2, Mod::Source::Lfo1B, (int) D::Filter1Cutoff, 0.6f),
            chord, 10.0);
    render (folder, "07 Duffing - drive into chaos", "",
            base + shapeDefaults (1, LfoSimShapes::Duffing)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 1.0f } }
                + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.6f),
            chord, 10.0,
            [] (IlanaSynthAudioProcessor& processor, double time) { setParam (processor, "lfo1_p1", (float) juce::jmin (1.0, 0.2 + time / 12.0)); });
    render (folder, "08 Logistic map - r sweeps period 2 to chaos", "",
            base + shapeDefaults (1, LfoSimShapes::Logistic)
                + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 8.0f } }
                + route (1, Mod::Source::Lfo1, (int) D::Osc1Pitch, 0.25f),
            held ({ 60 }, 0.05, 9.5), 10.0,
            [] (IlanaSynthAudioProcessor& processor, double time) { setParam (processor, "lfo1_p1", (float) juce::jmin (1.0, 0.3 + time / 12.0)); });

    // The random family, 2.5 s each: S&H, Sine Random, Perlin, Drunk Walk.
    {
        const int shapes[] { LfoSimShapes::RandomHold, LfoSimShapes::SineRandom, LfoSimShapes::Perlin, LfoSimShapes::DrunkWalk };
        render (folder, "09 Random family - S&H, sine, Perlin, drunk (2.5 s each)", "",
                base + shapeDefaults (1, LfoSimShapes::RandomHold)
                    + std::vector<std::pair<juce::String, float>> { { "lfo1_rate", 6.0f }, { "lfo1_stereo", 0.8f } }
                    + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.6f) + route (2, Mod::Source::Lfo1B, (int) D::Pan, 0.8f),
                chord, 10.0,
                [shapes] (IlanaSynthAudioProcessor& processor, double time)
                {
                    const auto shape = shapes[juce::jlimit (0, 3, (int) (time / 2.5))];
                    setParam (processor, "lfo1_shape", (float) shape);
                    if (shape == LfoSimShapes::Perlin || shape == LfoSimShapes::DrunkWalk)
                        setParam (processor, "lfo1_p1", LfoSimInfo::get (shape).params[0].defaultValue);
                });
    }
    render (folder, "10 SMOOTH - classic square, smooth 0 then 0.4", "",
            base + std::vector<std::pair<juce::String, float>> { { "lfo1_shape", 4.0f }, { "lfo1_rate", 3.0f } }
                + route (1, Mod::Source::Lfo1, (int) D::Filter1Cutoff, 0.5f),
            chord, 10.0,
            [] (IlanaSynthAudioProcessor& processor, double time) { setParam (processor, "lfo1_smooth", time < 5.0 ? 0.0f : 0.4f); });
}
// M8.2: the reworked piano, and A/B files against the reference grand.
void renderM82 (const juce::File& folder)
{
    folder.createDirectory();
    std::vector<Event> scale;
    auto time = 0.1;
    for (int dynamic = 0; dynamic < 3; ++dynamic)
        for (int note : { 36, 43, 48, 55, 60, 67, 72, 79, 84, 91, 96 })
        {
            const auto velocity = dynamic == 0 ? 25 : dynamic == 1 ? 64 : 120;
            scale.push_back ({ time, juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity) });
            scale.push_back ({ time + 0.9, juce::MidiMessage::noteOff (1, note) });
            time += 0.45;
        }
    for (const auto* preset : { "Grand Piano", "Hammered Strings" })
        render (folder, juce::String ("01 Scale pp mf ff - ") + preset, preset, {}, scale, time + 1.5);

    // A pedalled progression, then deep bass notes (the bark).
    std::vector<Event> music { { 0.0, juce::MidiMessage::controllerEvent (1, 64, 127) } };
    const int chords[4][4] { { 48, 55, 64, 72 }, { 45, 52, 60, 69 }, { 41, 53, 60, 65 }, { 43, 55, 62, 71 } };
    for (int c = 0; c < 4; ++c)
    {
        const auto at = 0.1 + c * 1.6;
        for (int n = 0; n < 4; ++n)
            music.push_back ({ at + n * 0.12, juce::MidiMessage::noteOn (1, chords[c][n], (juce::uint8) (70 + 12 * n)) });
        music.push_back ({ at + 1.5, juce::MidiMessage::controllerEvent (1, 64, 0) });
        music.push_back ({ at + 1.55, juce::MidiMessage::controllerEvent (1, 64, 127) });
        for (int n = 0; n < 4; ++n)
            music.push_back ({ at + 1.52, juce::MidiMessage::noteOff (1, chords[c][n]) });
    }
    music.push_back ({ 6.6, juce::MidiMessage::controllerEvent (1, 64, 0) });
    for (int i = 0; i < 3; ++i)
    {
        const int bass[] { 21, 28, 33 };
        music.push_back ({ 7.0 + i * 1.8, juce::MidiMessage::noteOn (1, bass[i], (juce::uint8) 120) });
        music.push_back ({ 7.0 + i * 1.8 + 1.6, juce::MidiMessage::noteOff (1, bass[i]) });
    }
    for (const auto* preset : { "Grand Piano", "Hammered Strings" })
        render (folder, juce::String ("02 Chords and bass - ") + preset, preset, {}, music, 12.5);

    // A/B: the reference grand's note, then ours (dry, no reverb), for a
    // spread of notes at each dynamic.
    const auto reference = juce::File::getCurrentWorkingDirectory().getChildFile ("build/reference/piano/SalamanderGrandPiano");
    if (! reference.isDirectory())
        return;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (const auto* dynamic : { "pp", "mf", "ff" })
    {
        const auto velocity = juce::String (dynamic) == "pp" ? 20 : juce::String (dynamic) == "mf" ? 60 : 124;
        std::vector<float> left, right;
        for (int note : { 24, 36, 48, 60, 72, 84, 96 })
        {
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (
                    reference.getChildFile ("piano_" + juce::String (note) + "_" + dynamic + ".wav"))))
            {
                juce::AudioBuffer<float> ref (2, 48000 * 2);
                ref.clear();
                reader->read (&ref, 0, 48000 * 2, 0, true, true);
                for (int i = 0; i < ref.getNumSamples(); ++i)
                {
                    const auto fade = i > 48000 * 2 - 4800 ? (float) (48000 * 2 - i) / 4800.0f : 1.0f;
                    left.push_back (ref.getSample (0, i) * fade);
                    right.push_back (ref.getSample (1, i) * fade);
                }
            }
            IlanaSynthAudioProcessor processor;
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Grand Piano"));
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                processor.assignFxSlot (slot, 0);
            processor.prepareToPlay (48000.0, 480);
            juce::AudioBuffer<float> buffer (2, 480);
            for (int block = 0; block < 200; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
                if (block == 180)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 480; ++i)
                {
                    left.push_back (buffer.getSample (0, i));
                    right.push_back (buffer.getSample (1, i));
                }
            }
        }
        juce::AudioBuffer<float> out (2, (int) left.size());
        for (int i = 0; i < (int) left.size(); ++i)
        {
            out.setSample (0, i, left[(size_t) i]);
            out.setSample (1, i, right[(size_t) i]);
        }
        const auto file = folder.getChildFile (juce::String ("03 AB reference then ours - ") + dynamic + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 48000.0, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
            }
        std::cout << "demo AB " << dynamic << " written" << std::endl;
    }
}
// M8.3: the west-coast voice.
void renderM83 (const juce::File& folder)
{
    using S = std::vector<std::pair<juce::String, float>>;
    const S west { { "west_on", 1.0f }, { "f1_cutoff", 18000.0f }, { "amp_sustain", 1.0f }, { "amp_release", 1.5f } };
    std::vector<Event> notes;
    const int melody[] { 48, 55, 60, 63, 58, 55, 51, 53, 48, 60, 67, 65, 63, 60, 58, 55 };
    for (int n = 0; n < 16; ++n)
    {
        notes.push_back ({ 0.1 + n * 0.3, juce::MidiMessage::noteOn (1, melody[n], (juce::uint8) (60 + (n * 37) % 60)) });
        notes.push_back ({ 0.1 + n * 0.3 + 0.25, juce::MidiMessage::noteOff (1, melody[n]) });
    }
    render (folder, "01 Bongo - struck gate, folded", "", west + S { { "west_fold", 0.45f }, { "west_stages", 2.0f } }, notes, 5.5);
    render (folder, "02 Bounce strikes the gate (LFO 1 B)", "",
            west + shapeDefaults (1, LfoSimShapes::Bounce)
                + S { { "west_src", (float) Mod::Source::Lfo1B }, { "lfo1_rate", 1.2f }, { "lfo1_trigger", 0.0f }, { "west_fold", 0.5f } },
            held ({ 43, 55 }, 0.05, 3.0) + held ({ 46, 58 }, 3.2, 6.5), 7.0);
    render (folder, "03 Fold sweep 0 to 100 percent (gate held open)", "",
            west + S { { "west_open", 1.0f }, { "west_strike", 0.0f }, { "west_stages", 1.0f } },
            held ({ 45 }, 0.05, 7.8), 8.0,
            [] (IlanaSynthAudioProcessor& p, double t) { setParam (p, "west_fold", (float) juce::jmin (1.0, t / 7.5)); });
    render (folder, "04 Symmetry sweep (even harmonics)", "",
            west + S { { "west_open", 1.0f }, { "west_strike", 0.0f }, { "west_fold", 0.5f } },
            held ({ 45 }, 0.05, 7.8), 8.0,
            [] (IlanaSynthAudioProcessor& p, double t) { setParam (p, "west_sym", (float) juce::jlimit (-1.0, 1.0, -1.0 + t / 3.75)); });
    render (folder, "05 Stages 1 2 3 4 (2 s each)", "",
            west + S { { "west_open", 1.0f }, { "west_strike", 0.0f }, { "west_fold", 0.55f } },
            held ({ 45 }, 0.05, 7.8), 8.0,
            [] (IlanaSynthAudioProcessor& p, double t) { setParam (p, "west_stages", (float) (1 + juce::jmin (3, (int) (t / 2.0)))); });
    for (int mode = 0; mode < 3; ++mode)
    {
        const char* names[] { "Combo", "Low Pass", "VCA" };
        render (folder, juce::String ("06 Gate mode - ") + names[mode], "", west + S { { "west_mode", (float) mode }, { "west_fold", 0.4f } }, notes, 5.5);
    }
    render (folder, "07 WEST in place of Filter 2, after Filter 1", "",
            west + S { { "west_pos", 1.0f }, { "west_fold", 0.35f }, { "f1_cutoff", 2500.0f }, { "f1_reso", 0.5f } }, notes, 5.5);
}
// M8.5: feedback guitar, Evolve and the vector pad.
void renderM85 (const juce::File& folder)
{
    using S = std::vector<std::pair<juce::String, float>>;
    const S guitar { { "osc1_mode", 1.0f }, { "osc1_excite", 10.0f }, { "osc1_string_sustain", 0.6f }, { "osc1_string_decay", 0.8f },
                     { "osc1_string_damp", 0.5f }, { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "amp_sustain", 1.0f },
                     { "amp_release", 1.0f }, { "f1_cutoff", 16000.0f } };
    for (float distance : { 0.2f, 0.5f, 0.8f })
        render (folder, "01 Feedback guitar - amp distance " + juce::String (distance, 1), "",
                guitar + S { { "osc1_fb_distance", distance }, { "osc1_fb_gain", 0.6f } },
                held ({ 45 }, 0.05, 5.5) + held ({ 52 }, 6.0, 11.5), 12.0);
    render (folder, "02 Feedback guitar - FEEDBACK 0 then 0.8", "", guitar + S { { "osc1_string_sustain", 0.0f } },
            held ({ 40 }, 0.05, 4.5) + held ({ 40 }, 5.0, 11.0), 11.5,
            [] (IlanaSynthAudioProcessor& p, double t) { setParam (p, "osc1_string_sustain", t < 4.8 ? 0.0f : 0.8f); });

    // Evolve: a filter macro drifting under a held chord.
    render (folder, "03 Evolve - macro 1 on cutoff, drifting", "",
            S { { "f1_cutoff", 900.0f }, { "f1_reso", 0.4f }, { "macro1", 0.5f }, { "macro1_evolve", 0.5f }, { "macro1_evolve_rate", 0.4f } }
                + route (1, Mod::Source::Macro1, (int) Mod::Destination::Filter1Cutoff, 0.7f),
            held ({ 48, 55, 60, 64 }, 0.05, 11.5), 12.0);

    // The vector pad travelling its path over four oscillators.
    render (folder, "04 Vector pad - path over four oscillators", "",
            S { { "vec_on", 1.0f }, { "vec_path", 1.0f }, { "vec_rate", 0.15f }, { "osc2_on", 1.0f }, { "osc2_semi", 7.0f },
                { "sub_on", 1.0f }, { "osc4_on", 1.0f }, { "osc4_semi", 12.0f }, { "f1_cutoff", 6000.0f } },
            held ({ 48, 60 }, 0.05, 11.5), 12.0);
}
// M8.6: bounces, rendered in the first block and played back.
void renderM86 (const juce::File& folder)
{
    using S = std::vector<std::pair<juce::String, float>>;
    using Request = IlanaSynthAudioProcessor::BounceRequest;
    const auto bounce = [] (IlanaSynthAudioProcessor& p, const Request& request)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()));
        auto audio = IlanaSynthAudioProcessor::renderBounce (juce::ValueTree::fromXml (*xml), request);
        juce::String message;
        p.applyBounce (request, audio, message);
        std::cout << "  " << message << std::endl;
    };

    // A pad with its reverb, bounced at C4 into OSC 2 and played as chords.
    render (folder, "01 Bounce - reverb pad into a sample, played as chords", "",
            S { { "osc1_unison", 5.0f }, { "osc1_detune", 25.0f }, { "f1_cutoff", 2500.0f }, { "amp_attack", 0.3f },
                { "amp_release", 1.5f }, { "fx_reverb_mix", 0.4f } },
            held ({ 48, 52, 55 }, 0.05, 3.5) + held ({ 45, 48, 52 }, 4.0, 7.5) + held ({ 41, 45, 48, 53 }, 8.0, 11.5), 12.5,
            [bounce] (IlanaSynthAudioProcessor& p, double t)
            {
                if (t > 0.0)
                    return;
                p.assignFxSlot (1, 13);
                Request request;
                request.targetOsc = 1;
                request.holdSeconds = 2.0;
                request.tailSeconds = 3.0;
                bounce (p, request);
                // Played back dry: the reverb is already in the sample.
                setParam (p, "fx_reverb_on", 0.0f);
                setParam (p, "amp_attack", 0.005f);
                setParam (p, "f1_cutoff", 16000.0f);
            });

    // A filter-swept saw, cut into a wavetable; an LFO scans the frames.
    render (folder, "02 Bounce - filter sweep into a wavetable, LFO on FRAME", "",
            S { { "f1_cutoff", 300.0f }, { "f1_env", 4.0f }, { "f1_reso", 0.5f }, { "fe_decay", 1.5f }, { "fe_sustain", 0.0f } },
            held ({ 48 }, 0.05, 3.9) + held ({ 55 }, 4.0, 7.9) + held ({ 43, 50 }, 8.0, 11.5), 12.0,
            [bounce] (IlanaSynthAudioProcessor& p, double t)
            {
                if (t > 0.0)
                    return;
                Request request;
                request.toTable = true;
                request.note = 48;
                request.holdSeconds = 2.0;
                bounce (p, request);
                setParam (p, "f1_cutoff", 12000.0f);
                setParam (p, "f1_env", 0.0f);
                setParam (p, "f1_reso", 0.2f);
                for (const auto& [id, value] : route (1, Mod::Source::Lfo1, (int) Mod::Destination::Osc1Frame, 1.0f))
                    setParam (p, id, value);
                setParam (p, "lfo1_rate", 0.25f);
            });

    // The Grand Piano bounced at C4 and played as a sampler: fast, and
    // through a low-pass it never had.
    render (folder, "03 Bounce - Grand Piano into a sample, through a low-pass", "Grand Piano",
            S {}, held ({ 48, 55, 60, 64 }, 0.05, 2.9) + held ({ 36, 60 }, 3.0, 5.9) + held ({ 67, 72, 76 }, 6.0, 9.0), 10.0,
            [bounce] (IlanaSynthAudioProcessor& p, double t)
            {
                if (t > 0.0)
                    return;
                Request request;
                request.targetOsc = 0;
                request.holdSeconds = 3.0;
                request.tailSeconds = 1.0;
                bounce (p, request);
                setParam (p, "f1_cutoff", 1800.0f);
                setParam (p, "f1_reso", 0.35f);
            });
}
// M10: every pack preset playing a phrase that suits its category.
void renderM10 (const juce::File& folder)
{
    IlanaSynthAudioProcessor probe;
    const auto names = probe.getFactoryPresetNames();
    const auto categories = Presets::getFactoryPresetCategories();
    const auto first = names.indexOf ("Nylon Twelve");
    for (int i = juce::jmax (0, first); i < names.size(); ++i)
    {
        const auto category = categories[i];
        if (category == "FX Input")
            continue;
        std::vector<Event> events;
        auto seconds = 7.0;
        if (category == "Bass")
            for (int k = 0; k < 8; ++k)
                events = events + held ({ (int[]) { 36, 36, 43, 36, 39, 36, 46, 43 }[k] }, 0.05 + k * 0.4, 0.05 + k * 0.4 + 0.3);
        else if (category == "Lead" || category == "Pluck")
            for (int k = 0; k < 8; ++k)
                events = events + held ({ (int[]) { 60, 63, 67, 70, 72, 70, 67, 63 }[k] }, 0.05 + k * 0.45, 0.05 + k * 0.45 + 0.38);
        else if (category == "Generative")
            events = held ({ 50, 57, 62 }, 0.05, 6.0);
        else if (category == "Drone" || category == "FX")
            events = held ({ 45 }, 0.05, 5.5);
        else // Pad, Keys, Chords
            events = held ({ 48, 55, 60, 64 }, 0.05, 3.0) + held ({ 45, 52, 57, 60 }, 3.3, 6.2);
        const auto name = juce::String (i - first + 1).paddedLeft ('0', 3) + " " + category + " - " + names[i];
        render (folder, name, names[i], {}, events, seconds);
    }
}
} // namespace Demo
