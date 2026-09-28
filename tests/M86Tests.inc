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
} // namespace M86

void runM86Tests()
{
    M86::runTests();
}
