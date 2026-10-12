// M8.6: resample to oscillator. Included by TableTest.cpp.

namespace M86
{
constexpr double sr = 48000.0;

juce::ValueTree stateOf (IlanaSynthAudioProcessor& processor)
{
    juce::MemoryBlock block;
    processor.getStateInformation (block);
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()));
    return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
}

// Plays one note for `seconds` and returns the left channel.
std::vector<float> play (IlanaSynthAudioProcessor& processor, int note, double seconds = 1.0)
{
    processor.prepareToPlay (sr, 256);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 256);
    for (int block = 0; block < (int) (seconds * sr / 256); ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 256; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

double pitchOf (const std::vector<float>& x)
{
    return fundamentalOf (std::vector<float> (x.begin() + (long) (0.2 * sr), x.end()), sr);
}

float getParamValue (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    if (const auto* value = processor.apvts.getRawParameterValue (id))
        return value->load();
    return -1.0f;
}

double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

// A plain bright patch: osc 1's saw through an open filter.
void plainPatch (IlanaSynthAudioProcessor& processor)
{
    setParam (processor, "osc2_on", 0.0f);
    setParam (processor, "sub_on", 0.0f);
    setParam (processor, "f1_cutoff", 5000.0f);
    setParam (processor, "amp_sustain", 1.0f);
    setParam (processor, "amp_release", 0.3f);
}

void runTests()
{
    using Request = IlanaSynthAudioProcessor::BounceRequest;

    // Into a sample: it plays back at the keyboard's pitch.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        plainPatch (processor);
        Request request;
        request.targetOsc = 1;
        request.note = 48;
        request.holdSeconds = 1.0;
        request.tailSeconds = 1.0;
        auto audio = IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request);
        check (audio != nullptr && std::abs (juce::Decibels::gainToDecibels (audio->buffer.getMagnitude (0, audio->getNumSamples())) + 1.0f) < 0.05f,
               "a bounce renders the note, normalised to -1 dBFS");
        check (audio != nullptr && audio->getNumSamples() > (int) (1.0 * sr) && audio->getNumSamples() < (int) (1.8 * sr),
               "the bounce keeps the release and trims the silence after it ("
                   + juce::String (audio != nullptr ? audio->getNumSamples() / sr : 0.0, 2) + " s)");

        juce::String message;
        const auto ok = processor.applyBounce (request, audio, message);
        check (ok && processor.isSampleEmbedded (1) && getParamValue (processor, "osc2_mode") == 2.0f
                   && getParamValue (processor, "osc2_on") > 0.5f && getParamValue (processor, "osc1_on") < 0.5f
                   && getParamValue (processor, "osc2_semi") == 12.0f,
               "the bounce lands on OSC 2 in Sample mode, shifted back to pitch, the others muted (" + message + ")");

        const auto c4 = pitchOf (play (processor, 60));
        const auto c5 = pitchOf (play (processor, 72));
        check (std::abs (cents (c4, 261.63)) < 15.0 && std::abs (cents (c5, 523.25)) < 15.0,
               "the bounced sample plays in tune across the keyboard (C4 " + juce::String (c4, 1) + " Hz, C5 " + juce::String (c5, 1) + " Hz)");

        // It travels with the patch: a fresh instance gets it back.
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        const auto* a = processor.getSampleForOsc (1);
        const auto* b = restored.getSampleForOsc (1);
        auto worst = 1.0f;
        if (a != nullptr && b != nullptr && a->getNumSamples() == b->getNumSamples() && a->getNumChannels() == b->getNumChannels())
        {
            worst = 0.0f;
            for (int ch = 0; ch < a->getNumChannels(); ++ch)
                for (int i = 0; i < a->getNumSamples(); ++i)
                    worst = juce::jmax (worst, std::abs (a->buffer.getSample (ch, i) - b->buffer.getSample (ch, i)));
        }
        check (restored.isSampleEmbedded (1) && worst < 1.0e-4f,
               "the bounced sample is saved inside the patch and restored (" + juce::String (saved.getSize() / 1024) + " KB state)");

        // A patch without it takes it away again.
        IlanaSynthAudioProcessor plain;
        juce::MemoryBlock plainState;
        plain.getStateInformation (plainState);
        restored.setStateInformation (plainState.getData(), (int) plainState.getSize());
        restored.flushAsyncUpdates();
        check (! restored.isSampleEmbedded (1) && restored.getSampleForOsc (1) == nullptr,
               "loading a patch without a bounce clears the bounced sample");
    }

    // As an exciter: the bounce on OSC 2 drives OSC 1's string through the
    // FM matrix (excite Osc In).
    {
        const auto render = [] (float fm)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (sr, 256);
            plainPatch (processor);
            Request request;
            request.targetOsc = 1;
            request.muteOthers = false;
            request.holdSeconds = 0.3;
            request.tailSeconds = 0.3;
            juce::String message;
            processor.applyBounce (request, IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request), message);
            setParam (processor, "osc1_mode", 1.0f);
            setParam (processor, "osc1_excite", 6.0f); // Osc In
            setParam (processor, "fm_amount", fm);
            return play (processor, 60, 1.0);
        };
        const auto without = render (0.0f), with = render (1.0f);
        auto difference = 0.0;
        for (size_t i = 0; i < without.size(); ++i)
            difference += (double) (with[i] - without[i]) * (with[i] - without[i]);
        difference = std::sqrt (difference / (double) without.size());
        check (difference > 0.01, "a bounced sample excites a string through Osc In (the string adds "
                                      + juce::String (juce::Decibels::gainToDecibels ((float) difference), 1) + " dB)");
    }

    // Into a wavetable: a patch table of single cycles.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        plainPatch (processor);
        setParam (processor, "f1_cutoff", 800.0f);
        Request request;
        request.targetOsc = 0;
        request.toTable = true;
        request.note = 57;
        auto audio = IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request);
        juce::String message;
        const auto ok = processor.applyBounce (request, audio, message);
        const auto choice = juce::roundToInt (getParamValue (processor, "osc1_table"));
        const auto slot = choice - TableFactory::getNumFactoryTables();
        const auto doc = processor.getUserTableDoc (juce::jmax (0, slot));
        check (ok && slot >= 0 && processor.isUserSlotEdited (slot) && doc.frames.size() > 4 && getParamValue (processor, "osc1_mode") == 0.0f,
               "a table bounce fills a free patch table with " + juce::String ((int) doc.frames.size()) + " frames (" + message + ")");
        const auto pitch = pitchOf (play (processor, 60));
        check (std::abs (cents (pitch, 261.63)) < 15.0, "the bounced table plays at the key's pitch (" + juce::String (pitch, 1) + " Hz)");
    }

    // With FX the reverb's tail is in the bounce; dry it is not.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        plainPatch (processor);
        processor.assignFxSlot (1, 13); // reverb
        setParam (processor, "fx_reverb_mix", 0.5f);
        Request request;
        request.holdSeconds = 0.5;
        request.tailSeconds = 3.0;
        const auto state = stateOf (processor);
        request.withFx = true;
        const auto wet = IlanaSynthAudioProcessor::renderBounce (state, request);
        request.withFx = false;
        const auto dry = IlanaSynthAudioProcessor::renderBounce (state, request);
        check (wet != nullptr && dry != nullptr && wet->getNumSamples() > dry->getNumSamples() + (int) (0.5 * sr),
               "with FX the bounce keeps the reverb tail (" + juce::String (wet != nullptr ? wet->getNumSamples() / sr : 0.0, 2) + " s vs "
                   + juce::String (dry != nullptr ? dry->getNumSamples() / sr : 0.0, 2) + " s dry)");
    }

    // Silence gives nothing, and says so.
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "osc1_on", 0.0f);
        setParam (processor, "osc2_on", 0.0f);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "noise_level", 0.0f);
        Request request;
        const auto audio = IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request);
        juce::String message;
        check (audio == nullptr && ! processor.applyBounce (request, audio, message) && message.contains ("silent"),
               "bouncing silence puts nothing on the oscillator");
    }

    // The background bounce, as the BOUNCE button runs it.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        plainPatch (processor);
        Request request;
        request.targetOsc = 3;
        request.holdSeconds = 0.5;
        request.tailSeconds = 0.5;
        check (processor.startBounce (request) && ! processor.startBounce (request), "a second bounce waits for the first");
        const auto started = juce::Time::getMillisecondCounter();
        while (processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering
               && juce::Time::getMillisecondCounter() - started < 60000)
        {
            juce::Thread::sleep (20);
            processor.flushAsyncUpdates();
        }
        check (processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Done && processor.isSampleEmbedded (3)
                   && getParamValue (processor, "osc4_mode") == 2.0f,
               "the background bounce finishes and lands on OSC 4 (" + processor.getBounceMessage() + ")");
    }
}

// Roadmap step 41: multisample resampling.
void runMultisampleTests()
{
    using Request = IlanaSynthAudioProcessor::BounceRequest;

    // The grid: notes from low to high by the spacing, velocities by layer.
    {
        Request request;
        request.noteLow = 36;
        request.noteHigh = 84;
        request.spacing = 12;
        check (request.multiNotes() == std::vector<int> ({ 36, 48, 60, 72, 84 }), "the multisample grid: C2 to C6 every octave is five notes");
        request.spacing = 3;
        check (request.multiNotes().size() == 17, "the multisample grid: every minor third from C2 to C6 is 17 notes");
        request.noteLow = 84;
        request.noteHigh = 36; // swapped ends are fine
        check (request.multiNotes().front() == 36 && request.multiNotes().back() == 84, "the multisample grid: swapped ends are put right");
        request.velocityLayers = 1;
        request.velocity = 100;
        check (request.multiVelocities() == std::vector<int> ({ 100 }), "one velocity layer plays the request's velocity");
        request.velocityLayers = 2;
        check (request.multiVelocities() == std::vector<int> ({ 32, 95 }), "two velocity layers sit at 32 and 95");
        request.velocityLayers = 4;
        check (request.numCaptures() == 4 * 17, "captures are notes times layers");
    }

    // A resample: zones tile the keys and velocities, share one gain, play in tune.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        plainPatch (processor);
        Request request;
        request.targetOsc = 1;
        request.multi = true;
        request.noteLow = 48;
        request.noteHigh = 72;
        request.spacing = 12;
        request.velocityLayers = 2;
        request.holdSeconds = 0.4;
        request.tailSeconds = 0.4;
        const auto started = juce::Time::getMillisecondCounter();
        auto audio = IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request);
        std::cout << "  [resample] 6 captures of 0.8 s: " << (juce::Time::getMillisecondCounter() - started) << " ms" << std::endl;
        check (audio != nullptr && audio->zones.size() == 6, "a multisample resample makes one zone per note and velocity layer");
        if (audio != nullptr && audio->zones.size() == 6)
        {
            // Zones are in note order, then layer: 48 low, 48 high, 60 low ...
            check (audio->zones[0].loKey == 0 && audio->zones[0].hiKey == 54 && audio->zones[2].loKey == 55 && audio->zones[2].hiKey == 66
                       && audio->zones[4].loKey == 67 && audio->zones[4].hiKey == 127,
                   "the key ranges meet halfway between the roots and reach both ends of the keyboard");
            check (audio->zones[0].loVel == 1 && audio->zones[0].hiVel == 63 && audio->zones[1].loVel == 64 && audio->zones[1].hiVel == 127,
                   "the velocity layers split 1-127 evenly");
            check (audio->zones[2].rootNote == 60.0 && audio->zones[4].rootNote == 72.0, "each zone's root is its note");
            auto loudest = 0.0f, quietest = 10.0f;
            for (const auto& zone : audio->zones)
            {
                const auto peak = zone.data->buffer.getMagnitude (0, zone.data->getNumSamples());
                loudest = juce::jmax (loudest, peak);
                quietest = juce::jmin (quietest, peak);
            }
            check (std::abs (juce::Decibels::gainToDecibels (loudest) + 1.0f) < 0.05f, "the loudest render reaches -1 dBFS");
            check (quietest < loudest * 0.98f, "the renders share one gain (the soft layer stays softer: "
                                                   + juce::String (juce::Decibels::gainToDecibels (quietest / loudest), 1) + " dB)");
            check (audio->getNumSamples() > 0 && audio->zones.size() > 1, "the shown region is rebuilt from the zones");
        }

        juce::String message;
        const auto ok = processor.applyBounce (request, audio, message);
        check (ok && processor.isSampleEmbedded (1) && getParamValue (processor, "osc2_mode") == 2.0f
                   && getParamValue (processor, "osc2_semi") == 0.0f && getParamValue (processor, "osc2_sample_tuned") > 0.5f,
               "the multisample lands on OSC 2 in tuned Sample mode with no shift (" + message + ")");
        for (const int note : { 48, 53, 60, 66, 72, 79 })
        {
            const auto expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            const auto heard = pitchOf (play (processor, note, 0.7));
            check (std::abs (cents (heard, expected)) < 15.0,
                   "the multisample plays in tune at note " + juce::String (note) + " (" + juce::String (heard, 1) + " Hz, wanted "
                       + juce::String (expected, 1) + ")");
        }

        // It travels with the patch, zones and all.
        juce::MemoryBlock saved, savedAgain;
        processor.getStateInformation (saved);
        processor.getStateInformation (savedAgain);
        check (saved.getSize() == savedAgain.getSize(), "saving twice gives the same state (the encoding is reused)");
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        restored.flushAsyncUpdates();
        const auto* a = processor.getSampleForOsc (1);
        const auto* b = restored.getSampleForOsc (1);
        auto same = a != nullptr && b != nullptr && a->zones.size() == b->zones.size();
        auto worst = 0.0f;
        for (size_t z = 0; same && z < a->zones.size(); ++z)
        {
            const auto &x = a->zones[z], &y = b->zones[z];
            same = x.loKey == y.loKey && x.hiKey == y.hiKey && x.loVel == y.loVel && x.hiVel == y.hiVel && x.rootNote == y.rootNote
                   && x.data->getNumSamples() == y.data->getNumSamples() && x.data->getNumChannels() == y.data->getNumChannels();
            for (int ch = 0; same && ch < x.data->getNumChannels(); ++ch)
                for (int i = 0; i < x.data->getNumSamples(); ++i)
                    worst = juce::jmax (worst, std::abs (x.data->buffer.getSample (ch, i) - y.data->buffer.getSample (ch, i)));
        }
        check (same && worst < 1.0e-4f && restored.isSampleEmbedded (1),
               "the multisample is saved inside the patch and restored, zones and audio (" + juce::String (saved.getSize() / 1024) + " KB state)");
        const auto heard = pitchOf (play (restored, 66, 0.7));
        check (std::abs (cents (heard, 440.0 * std::pow (2.0, (66 - 69) / 12.0))) < 15.0, "the restored multisample plays in tune");

        // Replacing it with a plain sample drops the zones from the saved state.
        processor.setUserSample (1, nullptr, {});
        juce::MemoryBlock cleared;
        processor.getStateInformation (cleared);
        IlanaSynthAudioProcessor fresh;
        fresh.setStateInformation (cleared.getData(), (int) cleared.getSize());
        fresh.flushAsyncUpdates();
        check (! fresh.isSampleEmbedded (1), "clearing the oscillator clears the saved multisample");

        // CPU: an eight-note chord on the multisample against a plain sample
        // of the same length.
        const auto chord = [] (IlanaSynthAudioProcessor& p)
        {
            p.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer on;
            for (int k = 0; k < 8; ++k)
                on.addEvent (juce::MidiMessage::noteOn (1, 48 + k * 3, (juce::uint8) 100), 0);
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int blockIndex = 0; blockIndex < 400; ++blockIndex)
            {
                juce::MidiBuffer none;
                p.processBlock (buffer, blockIndex == 0 ? on : none);
            }
            return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        };
        IlanaSynthAudioProcessor multi, single;
        for (auto* p : { &multi, &single })
        {
            plainPatch (*p);
            setParam (*p, "osc1_on", 0.0f);
            setParam (*p, "osc2_on", 1.0f);
            setParam (*p, "osc2_mode", 2.0f);
            setParam (*p, "osc2_sample_tuned", 1.0f);
        }
        multi.setUserSample (1, audio, {});
        auto plainAudio = std::make_shared<SampleData>();
        plainAudio->sampleRate = sr;
        plainAudio->buffer.makeCopyOf (audio->zones[2].data->buffer);
        single.setUserSample (1, plainAudio, {});
        multi.flushAsyncUpdates();
        single.flushAsyncUpdates();
        const auto tMulti = chord (multi);
        const auto tSingle = chord (single);
        std::cout << "  [resample CPU] 8-note chord, 400 blocks of 512: multisample " << juce::String (tMulti * 1000.0, 1) << " ms, plain sample "
                  << juce::String (tSingle * 1000.0, 1) << " ms" << std::endl;
        check (tMulti < tSingle * 1.5 + 0.01, "a multisample costs about what a plain sample does");
    }

    // A silent patch gives nothing.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (sr, 256);
        for (const auto* id : { "osc1_on", "osc2_on", "sub_on" })
            setParam (processor, id, 0.0f);
        Request request;
        request.multi = true;
        request.noteLow = 60;
        request.noteHigh = 60;
        request.holdSeconds = 0.2;
        request.tailSeconds = 0.1;
        check (IlanaSynthAudioProcessor::renderBounce (stateOf (processor), request) == nullptr, "a silent patch resamples to nothing");
    }
}
} // namespace M86

void runM86Tests()
{
    M86::runTests();
    M86::runMultisampleTests();
}
