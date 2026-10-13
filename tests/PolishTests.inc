#include <thread>
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
            // A different slice of the library for each setting, through one
            // processor (building one per preset was most of the time).
            IlanaSynthAudioProcessor processor;
            for (int index = 1 + (run++ % 12); index < names.size(); index += 12)
            {
                processor.loadFactoryPreset (index);
                processor.prepareToPlay (rate, block);
                const auto seconds = block == 1 ? 0.02 : 0.2; // long enough for the all-notes-off halfway
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

// Debug 3: a host's threads. The audio thread plays notes without a break
// while this (message) thread loads presets, restores and saves state and
// moves parameters: nothing may crash, and the output stays finite.
void runConcurrencyTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);
    std::atomic<bool> running { true };
    std::atomic<bool> finite { true };
    std::atomic<int> blocks { 0 };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, 128);
        int n = 0;
        while (running.load())
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (n % 40 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (n / 40) % 24, (juce::uint8) 100), 0);
            if (n % 40 == 30)
                midi.addEvent (juce::MidiMessage::noteOff (1, 48 + (n / 40) % 24), 0);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                    if (! std::isfinite (buffer.getSample (c, i)))
                        finite = false;
            ++n;
            blocks = n;
        }
    });

    const auto names = processor.getFactoryPresetNames();
    juce::Random random (77);
    juce::MemoryBlock state;
    for (int round = 0; round < 60; ++round)
    {
        processor.loadFactoryPreset (1 + random.nextInt (names.size() - 1));
        const auto& parameters = processor.getParameters();
        for (int k = 0; k < 20; ++k)
            parameters[random.nextInt (parameters.size())]->setValueNotifyingHost (random.nextFloat());
        if (round % 5 == 0)
        {
            processor.getStateInformation (state);
            processor.setStateInformation (state.getData(), (int) state.getSize());
        }
        const auto target = blocks.load() + 4;
        while (blocks.load() < target && running.load())
            std::this_thread::yield();
    }
    running = false;
    audio.join();
    check (finite.load() && blocks.load() > 200,
           "presets load, state restores and parameters move while the audio thread plays (" + juce::String (blocks.load()) + " blocks)");
}

// Debug 4: hostile saved state. A host hands back whatever it stored:
// truncated, bit-flipped, or with values that make no sense (NaN, huge,
// negative, words). The patch must load without crashing and play finite.
void runStateFuzzTest()
{
    IlanaSynthAudioProcessor source;
    const auto names = source.getFactoryPresetNames();
    source.loadFactoryPreset (names.indexOf ("Bounced Bell Grains")); // an embedded sample
    source.flushAsyncUpdates();
    juce::MemoryBlock good;
    source.getStateInformation (good);
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (good.getData(), (int) good.getSize()));
    const auto text = xml != nullptr ? xml->toString() : juce::String();

    juce::Random random (4242);
    auto badRuns = 0;
    juce::StringArray bad;
    for (int trial = 0; trial < 60; ++trial)
    {
        juce::MemoryBlock data;
        if (trial % 3 == 0)
        {
            // Raw bytes, truncated or flipped.
            data = good;
            if (random.nextBool())
                data.setSize ((size_t) random.nextInt ((int) data.getSize()));
            else
                for (int k = 0; k < 20; ++k)
                    static_cast<char*> (data.getData())[random.nextInt ((int) data.getSize())] ^= (char) (1 << random.nextInt (8));
        }
        else
        {
            // Valid XML with nonsense values.
            auto tree = juce::ValueTree::fromXml (text);
            static const char* const nonsense[] { "nan", "inf", "-inf", "1e30", "-1e30", "-7", "banana", "" };
            std::function<void (juce::ValueTree)> mangle = [&] (juce::ValueTree node)
            {
                for (int i = 0; i < node.getNumProperties(); ++i)
                    if (random.nextFloat() < 0.15f)
                        node.setProperty (node.getPropertyName (i), nonsense[random.nextInt (8)], nullptr);
                for (int c = node.getNumChildren(); --c >= 0;)
                {
                    if (random.nextFloat() < 0.05f)
                        node.removeChild (c, nullptr);
                    else
                        mangle (node.getChild (c));
                }
            };
            mangle (tree);
            if (auto out = tree.createXml())
                juce::AudioProcessor::copyXmlToBinary (*out, data);
        }

        // ILANA_FUZZ_TRACE=<dir>: write each trial's state there first, so a
        // crash leaves the state that caused it.
        if (const auto dir = juce::SystemStats::getEnvironmentVariable ("ILANA_FUZZ_TRACE", ""); dir.isNotEmpty())
        {
            juce::File (dir).getChildFile ("trial-" + juce::String (trial) + ".bin").replaceWithData (data.getData(), data.getSize());
            std::cout << "fuzz trial " << trial << std::endl;
        }

        IlanaSynthAudioProcessor target;
        target.setStateInformation (data.getData(), (int) data.getSize());
        target.flushAsyncUpdates();
        target.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        auto finite = true;
        for (int b = 0; b < 30; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            target.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    finite = finite && std::isfinite (buffer.getSample (c, i));
        }
        // Saving it again must work too.
        juce::MemoryBlock again;
        target.getStateInformation (again);
        if (! finite || again.getSize() == 0)
        {
            ++badRuns;
            bad.add (juce::String (trial) + (finite ? " (no state)" : " (not finite)"));
        }
    }
    check (badRuns == 0, "hostile saved states load, play finite and save again (" + bad.joinIntoString (", ") + ")");
}

// A host sending NaN for parameters (JUCE's parameters don't clamp it):
// several presets with every parameter, or a random third of them, set to
// NaN still play finite.
void runNanParameterTest()
{
    juce::StringArray bad;
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    juce::Random random (99);
    for (int index = 1; index < names.size(); index += 37)
        for (const auto share : { 1.0f, 0.33f })
        {
            IlanaSynthAudioProcessor processor;
            processor.loadFactoryPreset (index);
            processor.prepareToPlay (48000.0, 256);
            for (auto* parameter : processor.getParameters())
                if (random.nextFloat() < share)
                    parameter->setValue (std::numeric_limits<float>::quiet_NaN());
            juce::AudioBuffer<float> buffer (2, 256);
            auto finite = true;
            for (int b = 0; b < 30; ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (b == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                        finite = finite && std::isfinite (buffer.getSample (c, i));
            }
            if (! finite)
                bad.add (names[index] + (share < 1.0f ? " (a third)" : " (all)"));
        }
    check (bad.isEmpty(), "NaN parameter values from a host still play finite (" + bad.joinIntoString (", ") + ")");
}

// Debug 5: a saved project comes back as it was. Save, restore into a new
// instance, save again: the two states must match exactly (every 15th
// preset, and the resampled ones with their embedded audio and tables).
void runStateRoundTripTest()
{
    IlanaSynthAudioProcessor probe;
    const auto names = probe.getFactoryPresetNames();
    juce::Array<int> indices;
    for (int index = 1; index < names.size(); index += 15)
        indices.add (index);
    for (const auto* name : { "Bounced Bell Grains", "Tabled Cello", "Frozen Feedback Table" })
        indices.addIfNotAlreadyThere (names.indexOf (name));

    juce::StringArray bad;
    for (const auto index : indices)
    {
        IlanaSynthAudioProcessor first;
        first.loadFactoryPreset (index);
        first.flushAsyncUpdates();
        juce::MemoryBlock saved;
        first.getStateInformation (saved);

        IlanaSynthAudioProcessor second;
        second.setStateInformation (saved.getData(), (int) saved.getSize());
        second.flushAsyncUpdates();
        juce::MemoryBlock again;
        second.getStateInformation (again);

        std::unique_ptr<juce::XmlElement> a (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
        std::unique_ptr<juce::XmlElement> b (juce::AudioProcessor::getXmlFromBinary (again.getData(), (int) again.getSize()));
        if (a == nullptr || b == nullptr || ! a->isEquivalentTo (b.get(), false))
        {
            // Name the first attribute that differs, for the report.
            juce::String where;
            if (a != nullptr && b != nullptr)
                for (auto* child : a->getChildIterator())
                {
                    auto* other = b->getChildByAttribute ("id", child->getStringAttribute ("id"));
                    if (child->hasAttribute ("id") && (other == nullptr || ! child->isEquivalentTo (other, false)))
                    {
                        where = child->getStringAttribute ("id") + " " + child->getStringAttribute ("value") + " -> "
                                + (other != nullptr ? other->getStringAttribute ("value") : juce::String ("missing"));
                        break;
                    }
                    if (! child->hasAttribute ("id") && b->getChildByName (child->getTagName()) != nullptr
                        && ! child->isEquivalentTo (b->getChildByName (child->getTagName()), false))
                    {
                        where = child->getTagName();
                        break;
                    }
                }
            bad.add (names[index] + (where.isNotEmpty() ? " [" + where + "]" : juce::String()));
        }
    }
    check (bad.isEmpty(), "a saved state restores and saves again unchanged (" + bad.joinIntoString (", ") + ")");
}

// A4 stays A4 at every sample rate and oversampling setting, for a
// wavetable (Init) and a Physical string (Nylon String): the fundamental
// from the autocorrelation of the held note.
void runTuningAcrossRatesTest()
{
    juce::StringArray bad;
    auto worst = 0.0;
    for (const auto* preset : { "Init", "Nylon String" })
        for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (const auto oversample : { 0, 2, 4 })
            {
                IlanaSynthAudioProcessor processor;
                const auto names = processor.getFactoryPresetNames();
                processor.loadFactoryPreset (juce::jmax (0, names.indexOf (preset)));
                setParam (processor, "oversampling", oversample > 0 ? 1.0f : 0.0f);
                setParam (processor, "os_factor", oversample == 4 ? 1.0f : 0.0f);
                processor.prepareToPlay (rate, 512);
                juce::AudioBuffer<float> buffer (2, 512);
                std::vector<float> mono;
                const auto total = (int) (rate * 0.5);
                for (int done = 0; done < total; done += 512)
                {
                    buffer.clear();
                    juce::MidiBuffer midi;
                    if (done == 0)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
                    processor.processBlock (buffer, midi);
                    for (int i = 0; i < 512; ++i)
                        mono.push_back (buffer.getSample (0, i) + buffer.getSample (1, i));
                }
                // 100 ms from 150 ms in (past the attack).
                const auto start = (size_t) (rate * 0.15), length = (size_t) (rate * 0.1);
                const auto minLag = (int) (rate / 480.0), maxLag = (int) (rate / 400.0);
                auto bestLag = minLag;
                auto best = -1.0e30;
                std::vector<double> scores ((size_t) maxLag + 2, 0.0);
                for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
                {
                    double sum = 0.0;
                    for (size_t i = start; i < start + length; ++i)
                        sum += (double) mono[i] * mono[i + (size_t) lag];
                    scores[(size_t) lag] = sum;
                    if (lag >= minLag && lag <= maxLag && sum > best)
                    {
                        best = sum;
                        bestLag = lag;
                    }
                }
                // Parabolic interpolation around the peak.
                const auto a = scores[(size_t) bestLag - 1], b = scores[(size_t) bestLag], c = scores[(size_t) bestLag + 1];
                const auto shift = (a - 2.0 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2.0 * b + c) : 0.0;
                const auto hz = rate / ((double) bestLag + shift);
                const auto cents = 1200.0 * std::log2 (hz / 440.0);
                worst = juce::jmax (worst, std::abs (cents));
                if (std::abs (cents) > 5.0)
                    bad.add (juce::String (preset) + " @" + juce::String (rate / 1000.0, 1) + "k os" + juce::String (oversample)
                             + ": " + juce::String (cents, 1) + " ct");
            }
    check (bad.isEmpty(), "A4 in tune within 5 cents at 44.1-192 kHz, oversampled or not (worst " + juce::String (worst, 2) + " ct"
               + (bad.isEmpty() ? juce::String() : "; " + bad.joinIntoString (", ")) + ")");
}

// Debug 6: every voice lets go. A chord held half a second, released, then
// up to 15 s of silence: all voices must end (a stuck one plays on and
// burns CPU). (The slowest, a drone, takes about 8 s.)
// ILANA_VOICE_REPORT=1 prints how long each preset takes.
void runVoicesEndTest()
{
    juce::StringArray stuck;
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    const auto categories = Presets::getFactoryPresetCategories();
    const auto report = juce::SystemStats::getEnvironmentVariable ("ILANA_VOICE_REPORT", "").isNotEmpty();
    auto slowest = 0.0;
    juce::String slowestName;
    // One processor for the whole library (building one per preset was most
    // of the time): each preset starts once the last one's voices are gone.
    // At 24 kHz: release times are in seconds, so half the rate halves the work.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (24000.0, 512);
    for (int index = 1; index < names.size(); ++index)
    {
        if (categories[index] == "FX Input")
            continue;
        processor.loadFactoryPreset (index);
        juce::AudioBuffer<float> buffer (2, 512);
        const auto blocksPerSecond = 24000.0 / 512.0;
        auto endedAt = -1.0;
        for (int block = 0; block < (int) (15.5 * blocksPerSecond); ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == (int) (0.5 * blocksPerSecond))
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            processor.processBlock (buffer, midi);
            if (block > (int) (0.5 * blocksPerSecond) && processor.getActiveVoiceCount() == 0)
            {
                endedAt = block / blocksPerSecond - 0.5;
                break;
            }
        }
        if (report)
            std::cout << "VOICES " << names[index] << ": " << (endedAt < 0 ? juce::String ("never") : juce::String (endedAt, 2) + " s") << std::endl;
        if (endedAt < 0)
            stuck.add (names[index]);
        else if (endedAt > slowest)
        {
            slowest = endedAt;
            slowestName = names[index];
        }
    }
    check (stuck.isEmpty(), "every preset's voices end within 15 s of note-off (slowest " + slowestName + " " + juce::String (slowest, 1)
               + " s" + (stuck.isEmpty() ? juce::String() : "; stuck: " + stuck.joinIntoString (", ")) + ")");
}

// A host stopping the transport sends all-notes-off (CC 123) or all-sound-
// off (CC 120) with the keys still "held" from the plugin's point of view:
// the arp, the probability sequencer and note spray must stop with it, not
// keep generating notes.
void runTransportStopTest()
{
    juce::StringArray bad;
    for (const auto* engine : { "arp_on", "pseq_on", "spray_on" })
        for (const auto controller : { 123, 120, 1 })
        {
            // CC 1 (mod wheel) is the control: the engine must still play.
            IlanaSynthAudioProcessor processor;
            setParam (processor, engine, 1.0f);
            processor.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            auto after = 0;
            for (int block = 0; block < 400; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    for (const auto note : { 48, 55, 60 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                if (block == 94) // one second in, keys never released
                    midi.addEvent (juce::MidiMessage::controllerEvent (1, controller, 0), 0);
                processor.processBlock (buffer, midi);
                // From 3 s on (well past any release), nothing may sound.
                if (block > 94 + 3 * 94)
                    after = juce::jmax (after, processor.getActiveVoiceCount());
            }
            if (controller == 1 ? after == 0 : after > 0)
                bad.add (juce::String (engine) + " CC" + juce::String (controller) + " (" + juce::String (after) + " voices)");
        }
    check (bad.isEmpty(), "arp, sequencer and spray stop on all-notes-off and all-sound-off (" + bad.joinIntoString (", ") + ")");
}

// A note that starts after the mod wheel, expression or pressure moved sees
// their current values (a voice only hears the controllers that arrive while
// it plays); the MPE master channel bends member notes; poly glide starts
// from the previous note, not from the reused voice's own last pitch.
void runControllerSeedTest()
{
    struct Sound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    IlanaSynth synth;
    synth.addSound (new Sound());
    for (int i = 0; i < 2; ++i)
        synth.addVoice (new Voice());
    synth.setCurrentPlaybackSampleRate (48000.0);

    const auto setAll = [&] (const std::function<void (VoiceParams&)>& edit)
    {
        VoiceParams p;
        edit (p);
        for (int i = 0; i < synth.getNumVoices(); ++i)
            static_cast<Voice*> (synth.getVoice (i))->setParams (p);
    };
    const auto playing = [&]() -> Voice*
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (synth.getVoice (i)->isVoiceActive())
                return static_cast<Voice*> (synth.getVoice (i));
        return nullptr;
    };

    setAll ([] (VoiceParams&) {});
    synth.handleController (1, 1, 127);
    synth.handleController (1, 11, 0);
    synth.handleChannelPressure (1, 64);
    synth.noteOn (1, 60, 0.8f);
    if (auto* voice = playing())
        check (std::abs (voice->getModWheel() - 1.0f) < 0.001f && voice->getExpression() < 0.001f
                   && std::abs (voice->getAftertouch() - 64.0f / 127.0f) < 0.001f,
               "a note starting after the wheel, expression and pressure moved sees them");

    synth.allNotesOff (0, false);
    synth.handleController (1, 1, 0);
    synth.noteOn (1, 62, 0.8f);
    if (auto* voice = playing())
        check (voice->getModWheel() == 0.0f, "a reused voice does not keep the old wheel value");

    synth.allNotesOff (0, false);
    setAll ([] (VoiceParams& p) { p.mpe = true; p.pitchBendRange = 48.0f; p.masterBendRange = 2.0f; });
    synth.handlePitchWheel (1, 16383);
    synth.noteOn (2, 64, 0.8f);
    if (auto* voice = playing())
        check (std::abs (voice->getBendSemitones() - 2.0) < 0.01, "MPE master-channel bend reaches a member-channel note");

    synth.allNotesOff (0, false);
    setAll ([] (VoiceParams& p) { p.glideTime = 0.3f; });
    synth.noteOn (1, 48, 0.8f);
    synth.noteOff (1, 48, 0.0f, true); // still releasing, so the next note takes the other voice
    synth.noteOn (1, 60, 0.8f);
    auto* second = static_cast<Voice*> (synth.getVoice (1));
    check (second->isVoiceActive() && std::abs (second->getCurrentFrequency() - juce::MidiMessage::getMidiNoteInHertz (48)) < 0.01,
           "poly glide starts from the previous note's pitch");
}

// State and presets (the state review): what a state leaves behind, what a
// load cancels, what EDITED and undo see.
void runStatePresetTest()
{
    const auto xmlOf = [] (IlanaSynthAudioProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()));
    };
    const auto load = [] (IlanaSynthAudioProcessor& p, const juce::XmlElement& xml, bool flush)
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        p.setStateInformation (block.getData(), (int) block.getSize());
        if (flush)
            p.flushAsyncUpdates();
    };

    IlanaSynthAudioProcessor processor;

    // S8: a state with no learned CCs clears the map.
    {
        auto state = xmlOf (processor);
        state->setAttribute ("midiCcMap", "74=f1_cutoff");
        load (processor, *state, true);
        check (xmlOf (processor)->getStringAttribute ("midiCcMap") == "74=f1_cutoff", "a learned CC map loads");
        state->setAttribute ("midiCcMap", "");
        load (processor, *state, true);
        const auto after = xmlOf (processor);
        check (after->hasAttribute ("midiCcMap") && after->getStringAttribute ("midiCcMap").isEmpty(),
               "a state with no learned CCs clears the map");
    }

    // S2: a remap the patch no longer has is not saved again.
    {
        auto state = xmlOf (processor);
        state->setAttribute ("mod1Remap", "0,1,0;1,0,0");
        load (processor, *state, true);
        check (processor.isModRemapOn (0) && xmlOf (processor)->hasAttribute ("mod1Remap"), "a remap loads");
        processor.loadFactoryPreset (3);
        processor.flushAsyncUpdates();
        check (! xmlOf (processor)->hasAttribute ("mod1Remap"), "a factory preset's save carries no remap of the last state");
    }

    // S9, S10: a drawn LFO shape a state lacks is the default sine, and nan is refused.
    {
        processor.setLfoCustomPoint (0, 3, 0.9f);
        auto state = xmlOf (processor);
        state->removeAttribute ("lfo1Draw");
        load (processor, *state, true);
        const auto sine = (float) std::sin (juce::MathConstants<double>::twoPi * 3.0 / (double) IlanaSynthAudioProcessor::lfoDrawSteps);
        check (std::abs (processor.getLfoCustomPoint (0, 3) - sine) < 1.0e-5f, "a state without a drawn LFO shape gets the default");

        state->setAttribute ("lfo1Draw", "nan,0.5,inf");
        state->setAttribute ("lfo1Curve", "nan,0,0;0,0,0;1,1,0");
        load (processor, *state, true);
        auto finite = std::isfinite (processor.getLfoCustomPoint (0, 0)) && std::isfinite (processor.getLfoCustomPoint (0, 2));
        for (const auto& point : processor.getLfoCurve (0).points)
            finite = finite && std::isfinite (point.x) && std::isfinite (point.y);
        check (finite && processor.getLfoCurve (0).points.size() >= 2 && processor.getLfoCustomPoint (0, 1) == 0.5f,
               "nan in a drawn LFO shape or curve is refused");
    }

    // S1, S3: a file sample survives a load of the same state before the async update;
    // a missing file leaves the slot empty (and the path in the next save).
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-state-test");
        folder.createDirectory();
        const auto file = folder.getChildFile ("sample.wav");
        file.deleteFile();
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
            auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (44100.0)
                                                                                     .withNumChannels (1)
                                                                                     .withBitsPerSample (16));
            if (writer != nullptr)
            {
                juce::AudioBuffer<float> buffer (1, 4410);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (0, i, 0.5f * std::sin (0.05f * (float) i));
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
        }

        check (processor.loadUserSample (1, file), "the test sample loads");
        const auto withSample = xmlOf (processor);
        check (withSample->getStringAttribute ("osc2SamplePath") == file.getFullPathName(), "the sample's path is saved");

        processor.loadFactoryPreset (5);
        load (processor, *withSample, false); // before any async update
        processor.flushAsyncUpdates();
        check (xmlOf (processor)->getStringAttribute ("osc2SamplePath") == file.getFullPathName(),
               "loading a state right after a factory preset keeps its sample");

        auto missing = xmlOf (processor);
        const auto gone = folder.getChildFile ("gone.wav").getFullPathName();
        missing->setAttribute ("osc2SamplePath", gone);
        const auto notices = processor.getTableNoticeVersion();
        load (processor, *missing, true);
        check (processor.getSampleForOsc (1) == nullptr && processor.getTableNoticeVersion() != notices,
               "a missing sample file empties the slot and says so");
        check (xmlOf (processor)->getStringAttribute ("osc2SamplePath") == gone, "the missing sample's path is kept for the next save");

        processor.loadFactoryPreset (5);
        check (! xmlOf (processor)->hasAttribute ("osc2SamplePath"), "a factory preset carries no sample path");
        file.deleteFile();
    }

    // S6: the EDITED hash sees tables, macro names and tuning.
    {
        IlanaSynthAudioProcessor other;
        const auto base = other.getPatchDataHash();
        other.setMacroName (0, "Edited");
        const auto named = other.getPatchDataHash();
        check (named != base, "a macro name is part of the EDITED hash");
        other.setUserTable (2, WavetableDoc::sine());
        const auto tabled = other.getPatchDataHash();
        check (tabled != named, "a patch table is part of the EDITED hash");
        juce::String error;
        if (other.loadTuningScale ("test\n 3\n 9/8\n 5/4\n 2/1\n", error))
            check (other.getPatchDataHash() != tabled, "a tuning is part of the EDITED hash");
    }

    // S5: undoing a factory load puts back the tables, macro names and samples it cleared.
    {
        IlanaSynthAudioProcessor other;
        other.setMacroName (1, "Mine");
        other.setUserTable (4, WavetableDoc::sine());
        other.getUndoManager().clearUndoHistory();
        other.beginEdit ("Load");
        other.loadFactoryPreset (7);
        other.endEdit();
        check (! other.isUserSlotEdited (4), "a factory load resets the patch tables");
        other.getUndoManager().undo();
        check (other.isUserSlotEdited (4) && other.getMacroName (1) == "Mine",
               "undoing a factory load puts back the patch tables and macro names");
    }

    // S4: a bounce that finishes after another patch loaded is dropped.
    {
        IlanaSynthAudioProcessor other;
        other.prepareToPlay (48000.0, 256);
        IlanaSynthAudioProcessor::BounceRequest request;
        request.targetOsc = 3;
        request.holdSeconds = 0.3;
        request.tailSeconds = 0.2;
        check (other.startBounce (request), "the bounce starts");
        other.loadFactoryPreset (7);
        const auto started = juce::Time::getMillisecondCounter();
        while (other.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering
               && juce::Time::getMillisecondCounter() - started < 60000)
        {
            juce::Thread::sleep (20);
            other.flushAsyncUpdates();
        }
        check (other.getBounceState() == IlanaSynthAudioProcessor::BounceState::Failed && ! other.isSampleEmbedded (3),
               "a bounce finishing after a patch load is discarded (" + other.getBounceMessage() + ")");
    }

    // S14: a keyboard map loaded before any scale is saved.
    {
        IlanaSynthAudioProcessor other;
        juce::String error;
        const auto ok = other.loadTuningMapping ("12\n0\n127\n60\n69\n440\n12\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n", error);
        if (ok)
        {
            const auto state = xmlOf (other);
            auto* tuning = state->getChildByName ("Tuning");
            check (tuning != nullptr && tuning->getStringAttribute ("kbm").isNotEmpty(), "a keyboard map without a scale is saved");
        }
    }
}

void runTests()
{
    timedRun ("Polish::runControllerSeedTest", [] { runControllerSeedTest(); });
    timedRun ("Polish::runTransportStopTest", [] { runTransportStopTest(); });
    timedRun ("Polish::runVoicesEndTest", [] { runVoicesEndTest(); });
    timedRun ("Polish::runTuningAcrossRatesTest", [] { runTuningAcrossRatesTest(); });
    timedRun ("Polish::runStateRoundTripTest", [] { runStateRoundTripTest(); });
    timedRun ("Polish::runStateFuzzTest", [] { runStateFuzzTest(); });
    timedRun ("Polish::runNanParameterTest", [] { runNanParameterTest(); });
    timedRun ("Polish::runConcurrencyTest", [] { runConcurrencyTest(); });
    timedRun ("Polish::runMonoTest", [] { runMonoTest(); });
    timedRun ("Polish::runHostEdgeTest", [] { runHostEdgeTest(); });
    timedRun ("Polish::runContinuousTest", [] { runContinuousTest(); });
    timedRun ("Polish::runRepeatableLoadTest", [] { runRepeatableLoadTest(); });
    timedRun ("Polish::runPatchCutTest", [] { runPatchCutTest(); });
    timedRun ("Polish::runStatePresetTest", [] { runStatePresetTest(); });
}
} // namespace Polish

void runPolishTests()
{
    Polish::runTests();
}
