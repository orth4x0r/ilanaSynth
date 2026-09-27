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
} // namespace Demo
