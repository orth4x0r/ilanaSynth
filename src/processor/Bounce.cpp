#include "ProcessorInternal.h"

// M8.6: resample to oscillator ------------------------------------------------

IlanaSynthAudioProcessor::~IlanaSynthAudioProcessor()
{
    for (auto* parameter : getParameters())
        parameter->removeListener (&paramEpochListener);

    if (bounceThread != nullptr)
    {
        bounceThread->cancel = true;
        bounceThread->stopThread (10000);
    }
}

bool IlanaSynthAudioProcessor::startBounce (const BounceRequest& request)
{
    if (bounceState.load() == BounceState::Rendering)
        return false;
    if (bounceThread != nullptr)
        bounceThread->stopThread (10000); // finished; just reap it

    pendingBounce = request;
    bounceProgress = 0.0f;
    bounceState = BounceState::Rendering;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        bounceMessage = "Bouncing...";
        bounceReady = false;
        bounceResult.reset();
    }
    bounceThread = std::make_unique<BounceThread> (*this, buildFullState(), request);
    bounceThread->startThread (juce::Thread::Priority::low);
    return true;
}

juce::String IlanaSynthAudioProcessor::getBounceMessage() const
{
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return bounceMessage;
}

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::renderBounce (const juce::ValueTree& source, const BounceRequest& request,
                                                                   std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    constexpr double rate = 48000.0;
    constexpr int blockSize = 512;

    // Dry: every effect off, in the copy only.
    auto state = source.createCopy();
    if (! request.withFx)
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();
            if (id.startsWith ("fx_") && id.endsWith ("_on"))
                child.setProperty ("value", 0.0f, nullptr);
            else if (id.startsWith ("fx_slot") && id.endsWith ("_bypass"))
                child.setProperty ("value", 1.0f, nullptr);
        }

    auto renderer = std::make_unique<IlanaSynthAudioProcessor>();
    renderer->applyFullState (state);
    renderer->flushAsyncUpdates(); // file-backed samples and tables, now
    renderer->setNonRealtime (true);
    renderer->prepareToPlay (rate, blockSize);

    const auto note = juce::jlimit (0, 127, request.note);
    const auto holdSamples = (int) (juce::jlimit (0.05, 30.0, request.holdSeconds) * rate);
    const auto totalSamples = holdSamples + (int) (juce::jlimit (0.0, 30.0, request.tailSeconds) * rate);
    const auto channels = juce::jmax (2, juce::jmax (renderer->getTotalNumInputChannels(), renderer->getTotalNumOutputChannels()));

    auto result = std::make_shared<SampleData>();
    result->sampleRate = rate;
    result->buffer.setSize (2, totalSamples);
    juce::AudioBuffer<float> block (channels, blockSize);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        if (cancel != nullptr && cancel->load())
            return nullptr;
        const auto count = juce::jmin (blockSize, totalSamples - start);
        juce::MidiBuffer midi;
        if (start == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, request.velocity)), 0);
        if (holdSamples >= start && holdSamples < start + blockSize)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), holdSamples - start);
        block.setSize (channels, count, false, false, true);
        block.clear();
        renderer->processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            result->buffer.copyFrom (ch, start, block, juce::jmin (ch, block.getNumChannels() - 1), 0, count);
        if (progress != nullptr)
            progress->store ((float) (start + count) / (float) totalSamples);
    }
    renderer->releaseResources();

    // Trim the silent tail (80 dB under the peak), fade the last 10 ms and
    // normalise to -1 dBFS. Identical channels become one.
    const auto peak = result->buffer.getMagnitude (0, totalSamples);
    if (peak < 1.0e-5f || ! std::isfinite (peak))
        return nullptr;
    auto length = totalSamples;
    while (length > holdSamples / 4 && std::abs (result->buffer.getSample (0, length - 1)) < peak * 1.0e-4f
           && std::abs (result->buffer.getSample (1, length - 1)) < peak * 1.0e-4f)
        --length;
    length = juce::jmin (totalSamples, length + (int) (0.01 * rate));
    const auto fade = juce::jmin (length / 2, (int) (0.01 * rate));
    auto mono = true;
    for (int i = 0; i < length && mono; ++i)
        mono = std::abs (result->buffer.getSample (0, i) - result->buffer.getSample (1, i)) < 1.0e-6f;

    juce::AudioBuffer<float> trimmed (mono ? 1 : 2, length);
    for (int ch = 0; ch < trimmed.getNumChannels(); ++ch)
    {
        trimmed.copyFrom (ch, 0, result->buffer, ch, 0, length);
        trimmed.applyGainRamp (ch, length - fade, fade, 1.0f, 0.0f);
    }
    trimmed.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);
    result->buffer = std::move (trimmed);
    result->name = "Bounce " + juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    return result;
}

bool IlanaSynthAudioProcessor::applyBounce (const BounceRequest& request, std::shared_ptr<SampleData> audio, juce::String& message)
{
    if (audio == nullptr)
    {
        message = "The bounce was silent: nothing was put on the oscillator.";
        return false;
    }

    const auto osc = juce::jlimit (0, OscillatorIds::count - 1, request.targetOsc);
    const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto reset = [this] (const juce::String& id)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    };
    const auto oscName = "OSC " + juce::String (osc + 1);

    if (request.toTable)
    {
        const auto slot = findFreeUserSlot();
        if (slot < 0)
        {
            message = "All " + juce::String (numUserSlots) + " patch table slots are in use: free one to bounce into a table.";
            return false;
        }
        // The held part, mono, cut into single cycles.
        const auto held = juce::jmin (audio->getNumSamples(), (int) (juce::jmax (0.05, request.holdSeconds) * audio->sampleRate));
        std::vector<float> mono ((size_t) held);
        for (int ch = 0; ch < audio->getNumChannels(); ++ch)
            for (int i = 0; i < held; ++i)
                mono[(size_t) i] += audio->buffer.getSample (ch, i) / (float) audio->getNumChannels();
        std::vector<std::vector<float>> frames;
        Wavetable::resynthesize (mono, audio->sampleRate, frames, 64);
        if (frames.empty())
        {
            message = "The bounce could not be cut into wavetable frames.";
            return false;
        }
        const auto frameCount = (int) frames.size();
        if (! setUserTable (slot, WavetableDoc::fromFrames (audio->name, std::move (frames))))
        {
            message = "The bounced table could not be built.";
            return false;
        }
        set (prefix + "_table", (float) (TableFactory::getNumFactoryTables() + slot));
        set (prefix + "_mode", 0.0f);
        set (prefix + "_semi", 0.0f);
        reset (prefix + "_frame");
        message = "Bounced into " + oscName + " as patch table User " + juce::String (slot + 1) + " ("
                  + juce::String (frameCount) + " frames): sweep FRAME to play through it.";
    }
    else
    {
        const auto seconds = (double) audio->getNumSamples() / audio->sampleRate;
        setUserSample (osc, audio, {});
        set (prefix + "_mode", 2.0f);
        set (prefix + "_sample_factory", 0.0f);
        set (prefix + "_sample_tuned", 1.0f);
        // Tuned samples play at pitch on C4: shift the bounced note back.
        set (prefix + "_semi", (float) juce::jlimit (-24, 24, 60 - request.note));
        for (const auto* id : { "_sample_loop", "_sample_reverse", "_sample_start", "_sample_end", "_sample_fade_in", "_sample_fade_out" })
            reset (prefix + id);
        message = "Bounced into " + oscName + " as a " + juce::String (seconds, 1) + " s sample, saved inside the patch.";
    }

    for (const auto* id : { "_unison", "_chord", "_spectral", "_spectral_amt", "_warp", "_warp_amt" })
        reset (prefix + id);
    set (prefix + "_fine", 0.0f); // the bounce already holds the patch's tuning
    set (prefix + "_on", 1.0f);
    if (request.muteOthers)
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (i != osc)
                set (juce::String (OscillatorIds::prefixes[(size_t) i]) + "_on", 0.0f);
    return true;
}

juce::ValueTree IlanaSynthAudioProcessor::encodeSample (const SampleData& data)
{
    juce::ValueTree tree ("Sample");
    tree.setProperty ("name", data.name, nullptr);
    tree.setProperty ("rate", data.sampleRate, nullptr);
    tree.setProperty ("channels", data.getNumChannels(), nullptr);
    tree.setProperty ("length", data.getNumSamples(), nullptr);
    juce::MemoryOutputStream raw;
    {
        juce::GZIPCompressorOutputStream zip (raw, 9);
        for (int i = 0; i < data.getNumSamples(); ++i)
            for (int ch = 0; ch < data.getNumChannels(); ++ch)
                zip.writeShort ((short) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, data.buffer.getSample (ch, i)) * 32767.0f));
    }
    tree.setProperty ("data", raw.getMemoryBlock().toBase64Encoding(), nullptr);
    return tree;
}

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::decodeSample (const juce::ValueTree& tree)
{
    const auto rate = (double) tree.getProperty ("rate", 0.0);
    const auto channels = (int) tree.getProperty ("channels", 0);
    const auto length = (int) tree.getProperty ("length", 0);
    if (rate < 1000.0 || rate > 400000.0 || channels < 1 || channels > 2 || length < 2 || length > (int) (rate * 120.0))
        return nullptr;
    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (tree.getProperty ("data").toString()))
        return nullptr;
    juce::MemoryInputStream in (block, false);
    juce::GZIPDecompressorInputStream zip (in);
    auto data = std::make_shared<SampleData>();
    data->sampleRate = rate;
    data->name = tree.getProperty ("name").toString();
    data->buffer.setSize (channels, length);
    for (int i = 0; i < length; ++i)
        for (int ch = 0; ch < channels; ++ch)
        {
            if (zip.isExhausted())
                return nullptr;
            data->buffer.setSample (ch, i, (float) zip.readShort() / 32767.0f);
        }
    return data;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    // A new plugin instance opens on the Init patch rather than the raw
    // parameter defaults (the tests construct the processor directly and keep
    // the defaults). A saved session's state replaces it as usual.
    auto* processor = new IlanaSynthAudioProcessor();
    if (processor->getCurrentPresetName().isEmpty())
        processor->loadFactoryPreset (0);
    return processor;
}
