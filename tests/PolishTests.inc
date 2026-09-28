// The polish and debug rounds after M10: regressions for what they fixed.
// Included by TableTest.cpp.

namespace Polish
{
// Knobs without a step are continuous: cutoff used to move in 20 Hz steps
// (a thousandth of its range), so 440 Hz could not be set.
void runContinuousTest()
{
    IlanaSynthAudioProcessor processor;
    auto worst = 0.0f;
    for (const auto hz : { 40.0f, 440.0f, 1234.5f })
    {
        setParam (processor, "f1_cutoff", hz);
        worst = juce::jmax (worst, std::abs (processor.apvts.getRawParameterValue ("f1_cutoff")->load() - hz) / hz);
    }
    check (worst < 1.0e-4f, "cutoff is continuous (worst error " + juce::String (worst * 100.0f, 4) + " %)");
}

// Loading a patch stops the old one's voices and effect tails, easing to
// silence rather than stepping to it.
void runPatchCutTest()
{
    IlanaSynthAudioProcessor processor;
    const auto names = processor.getFactoryPresetNames();
    const auto categories = Presets::getFactoryPresetCategories();
    auto pad = -1;
    for (int i = 0; i < names.size() && pad < 0; ++i)
        if (categories[i] == "Pad")
            pad = i;
    processor.loadFactoryPreset (juce::jmax (0, pad));
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    auto maxStep = 0.0f, last = 0.0f;
    const auto run = [&] (int blocks, juce::MidiBuffer midi, bool measure)
    {
        auto peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();
            const auto* data = buffer.getReadPointer (0);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (measure)
                    maxStep = juce::jmax (maxStep, std::abs (data[i] - last));
                last = data[i];
            }
            peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
        }
        return peak;
    };

    juce::MidiBuffer on;
    for (const auto note : { 48, 55, 60, 64 })
        on.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
    const auto playing = run (190, on, false);   // about 1 s
    auto before = 0.0f;
    {
        // The largest sample-to-sample step the pad makes by itself.
        auto previous = last;
        for (int b = 0; b < 8; ++b)
        {
            buffer.clear();
            juce::MidiBuffer none;
            processor.processBlock (buffer, none);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                before = juce::jmax (before, std::abs (buffer.getSample (0, i) - previous));
                previous = buffer.getSample (0, i);
            }
        }
        last = previous;
    }

    // The step from the old patch's last sample to silence, eased (the
    // check needs a last sample far enough from zero to make a click).
    const auto lastBeforeCut = last;
    processor.loadFactoryPreset (0);
    const auto fade = run (4, {}, true);
    const auto after = run (40, {}, false);
    check (playing > 0.05f && after < 1.0e-3f,
           "loading a patch stops the old one's sound (" + juce::String (juce::Decibels::gainToDecibels (playing), 1) + " dB -> "
               + juce::String (juce::Decibels::gainToDecibels (after), 1) + " dB)");
    check (maxStep < 0.01f && std::abs (lastBeforeCut) > 0.01f,
           "and without a click (largest step " + juce::String (maxStep, 4) + " from " + juce::String (lastBeforeCut, 4)
               + "; the pad's own steps reach " + juce::String (before, 4) + ")");
    juce::ignoreUnused (fade);
}

// ILANA_LEVEL_PROBE="A|B|...": each preset fresh, a C3-G3-C4 chord held
// 4 s at velocity 100; rms over 0-1 s and 1-4 s.
void probeLevels (const juce::String& list)
{
    for (const auto& name : juce::StringArray::fromTokens (list, "|", ""))
    {
        // ILANA_LEVEL_AFTER=1: prepared first and loaded after the preset
        // before it, as the library test does.
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf (name);
        const auto after = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_AFTER", "").isNotEmpty();
        if (after)
        {
            processor.prepareToPlay (48000.0, 512);
            processor.loadFactoryPreset (juce::jmax (0, index - 1));
            processor.panic();
        }
        processor.loadFactoryPreset (juce::jmax (0, index));
        if (after)
            processor.panic();
        else
            processor.prepareToPlay (48000.0, 512);
        // As the library test measures: every macro at 0.
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_MACROS0", "").isNotEmpty())
            for (int m = 1; m <= 4; ++m)
                processor.apvts.getParameter ("macro" + juce::String (m))->setValueNotifyingHost (0.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        double early = 0.0, late = 0.0;
        int earlyCount = 0, lateCount = 0;
        for (int block = 0; block < 375; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                {
                    const auto v = (double) buffer.getSample (c, i);
                    (block < 94 ? early : late) += v * v;
                    ++(block < 94 ? earlyCount : lateCount);
                }
        }
        std::cout << "LEVEL " << name << ": " << juce::Decibels::gainToDecibels (std::sqrt (early / earlyCount)) << " dB then "
                  << juce::Decibels::gainToDecibels (std::sqrt (late / lateCount)) << " dB" << std::endl;
    }
}

// ILANA_LEVEL_SEQ=<name>: the library test's sequence (one processor, every
// preset from 1 in turn, macros at 0) up to <name>; prints each rms.
void dumpState (IlanaSynthAudioProcessor& processor, const juce::String& file)
{
    juce::String out;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            out << withId->paramID << " " << parameter->getValue() << "\n";
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto* sample = processor.getSampleForOsc (osc);
        out << "sample" << osc << " " << (sample != nullptr ? sample->getNumSamples() : -1) << " embedded " << (int) processor.isSampleEmbedded (osc) << "\n";
    }
    juce::File (file).replaceWithText (out);
}

void probeSequence (const juce::String& target)
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto names = processor.getFactoryPresetNames();
    juce::AudioBuffer<float> buffer (2, 512);
    const auto from = names.indexOf (juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_FROM", ""));
    for (int index = juce::jmax (1, from); index < names.size(); ++index)
    {
        if (from > 0 && index > from && names[index] != target)
            continue;
        processor.loadFactoryPreset (index);
        processor.panic();
        for (int m = 1; m <= 4; ++m)
            processor.apvts.getParameter ("macro" + juce::String (m))->setValueNotifyingHost (0.0f);
        if (names[index] == target)
            if (const auto file = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_DUMP", ""); file.isNotEmpty())
                dumpState (processor, file);
        double sum = 0.0;
        for (int block = 0; block < 90; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    sum += (double) buffer.getSample (c, i) * buffer.getSample (c, i);
        }
        const auto db = juce::Decibels::gainToDecibels (std::sqrt (sum / (90.0 * 1024.0)));
        if (names[index] == target || index + 3 >= names.indexOf (target))
            std::cout << "SEQ " << names[index] << ": " << db << " dB" << std::endl;
        for (int m = 1; m <= 4; ++m)
            processor.apvts.getParameter ("macro" + juce::String (m))->setValueNotifyingHost (1.0f);
        for (int block = 0; block < 60; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
        }
        if (names[index] == target)
            return;
    }
}

// A patch sounds the same whatever played before it: Bounced Bell Grains
// scans its sample with a free 0.05 Hz LFO, which started wherever the last
// patch left it (18 dB quieter in its first second after Vector Strike Pad).
void runRepeatableLoadTest()
{
    const auto renderAfter = [] (const juce::String& before)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        const auto names = processor.getFactoryPresetNames();
        juce::AudioBuffer<float> buffer (2, 512);
        const auto play = [&] (int blocks)
        {
            double sum = 0.0;
            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    for (const auto note : { 48, 55, 60 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        sum += (double) buffer.getSample (c, i) * buffer.getSample (c, i);
            }
            return juce::Decibels::gainToDecibels (std::sqrt (sum / (blocks * 1024.0)));
        };
        if (before.isNotEmpty())
        {
            processor.loadFactoryPreset (names.indexOf (before));
            play (200);
        }
        processor.loadFactoryPreset (names.indexOf ("Bounced Bell Grains"));
        processor.panic();
        return play (90);
    };
    const auto fresh = renderAfter ({});
    const auto after = renderAfter ("Vector Strike Pad");
    // (Grains keep their free randomness, worth a dB or so either way.)
    check (std::abs (fresh - after) < 2.0,
           "a patch sounds the same whatever played before it (" + juce::String (fresh, 1) + " dB fresh, "
               + juce::String (after, 1) + " dB after another)");
}

// Debug 2: hosts' edge cases. Every 12th preset at four sample rates and
// odd block sizes (including 0, 1 and more than announced), with MIDI edge
// cases (velocity-0 note-ons, all-notes-off, extreme bends, sustain):
// output stays finite and bounded.
void runHostEdgeTest()
{
    juce::StringArray bad;
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    int run = 0;
    for (const auto rate : { 22050.0, 44100.0, 96000.0, 192000.0 })
        for (const auto block : { 1, 17, 480 })
        {
            // A different slice of the library for each setting.
            for (int index = 1 + (run++ % 12); index < names.size(); index += 12)
            {
                IlanaSynthAudioProcessor processor;
                processor.loadFactoryPreset (index);
                processor.prepareToPlay (rate, block);
                const auto seconds = block == 1 ? 0.05 : 0.4;
                const auto total = (int) (rate * seconds);
                juce::AudioBuffer<float> buffer (2, juce::jmax (block * 3, 1));
                auto peak = 0.0f;
                auto finite = true;
                auto done = 0;
                for (int b = 0; done < total; ++b)
                {
                    // Every fifth block is longer than announced; one is empty.
                    const auto length = b == 3 ? 0 : (b % 5 == 4 ? block * 3 : block);
                    buffer.setSize (2, length, false, false, true);
                    buffer.clear();
                    juce::MidiBuffer midi;
                    if (b == 0)
                        for (const auto note : { 36, 60, 64, 67, 96 })
                            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
                    if (b == 1)
                    {
                        midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
                        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 0), 0); // a note-off
                    }
                    if (b == 2)
                        midi.addEvent (juce::MidiMessage::pitchWheel (1, 0), 0);
                    if (done > total / 2 && done - length <= total / 2)
                        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                    processor.processBlock (buffer, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < length; ++i)
                        {
                            const auto v = buffer.getSample (c, i);
                            finite = finite && std::isfinite (v);
                            peak = juce::jmax (peak, std::abs (v));
                        }
                    done += juce::jmax (1, length);
                }
                if (! finite || peak > 8.0f)
                    bad.add (names[index] + " @" + juce::String (rate / 1000.0, 1) + "k/" + juce::String (block)
                             + (finite ? " peak " + juce::String (peak, 1) : " not finite"));
            }
        }
    check (bad.isEmpty(), "presets stay finite and bounded at 22-192 kHz and odd block sizes (" + bad.joinIntoString (", ") + ")");
}

// A mono output bus (the processor accepts one): every preset renders
// into a one-channel buffer without touching a second channel.
void runMonoTest()
{
    juce::StringArray bad;
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    for (int index = 1; index < names.size(); index += 5)
    {
        IlanaSynthAudioProcessor processor;
        auto layout = processor.getBusesLayout();
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
        if (! processor.setBusesLayout (layout))
        {
            bad.add ("mono layout refused");
            break;
        }
        processor.loadFactoryPreset (index);
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (1, 256);
        auto finite = true;
        for (int b = 0; b < 40; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 256; ++i)
                finite = finite && std::isfinite (buffer.getSample (0, i));
        }
        if (! finite)
            bad.add (names[index]);
    }
    check (bad.isEmpty(), "presets render into a mono output (" + bad.joinIntoString (", ") + ")");
}

void runTests()
{
    runMonoTest();
    runHostEdgeTest();
    runContinuousTest();
    runRepeatableLoadTest();
    runPatchCutTest();
}
} // namespace Polish

void runPolishTests()
{
    Polish::runTests();
}
