#include "ProcessorInternal.h"

#include <functional>

#include "../dsp/MultiSample.h"

// M8.6: resample to oscillator ------------------------------------------------

IlanaSynthAudioProcessor::~IlanaSynthAudioProcessor()
{
    for (auto* parameter : getParameters())
        parameter->removeListener (&paramEpochListener);
    for (int slot = 1; slot <= numFxSlots; ++slot)
        if (auto* parameter = apvts.getParameter ("fx_slot" + juce::String (slot)))
            parameter->removeListener (&fxSlotTypeListener);

    if (bounceThread != nullptr)
    {
        bounceThread->cancel = true;
        bounceThread->stopThread (10000);
    }

    // The spectral worker reads the patch tables: stop it before they go.
    spectralCache.reset();
    // The stretch worker reads the oscillators' samples.
    stretchRenderer.reset();
}

bool IlanaSynthAudioProcessor::startBounce (const BounceRequest& request)
{
    if (bounceState.load() == BounceState::Rendering)
        return false;
    if (bounceThread != nullptr)
        bounceThread->stopThread (10000); // finished; just reap it

    pendingBounce = request;
    bounceGeneration = patchGeneration.load(); // handleAsyncUpdate drops the result if another patch was loaded since
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

std::vector<int> IlanaSynthAudioProcessor::BounceRequest::multiNotes() const
{
    std::vector<int> notes;
    const auto low = juce::jlimit (0, 127, juce::jmin (noteLow, noteHigh));
    const auto high = juce::jlimit (0, 127, juce::jmax (noteLow, noteHigh));
    const auto step = juce::jlimit (1, 12, spacing);
    for (int note = low; note <= high; note += step)
        notes.push_back (note);
    return notes;
}

std::vector<int> IlanaSynthAudioProcessor::BounceRequest::multiVelocities() const
{
    const auto layers = juce::jlimit (1, 4, velocityLayers);
    if (layers == 1)
        return { juce::jlimit (1, 127, velocity) };
    std::vector<int> velocities;
    for (int layer = 0; layer < layers; ++layer)
        velocities.push_back (juce::jlimit (1, 127, juce::roundToInt (127.0f * ((float) layer + 0.5f) / (float) layers)));
    return velocities;
}

namespace
{
constexpr double bounceRate = 48000.0;

} // namespace

// One note on a fresh copy of the patch: the raw render (two channels, the
// held part and the tail) or an empty buffer if cancelled. The caller trims.
juce::AudioBuffer<float> IlanaSynthAudioProcessor::playOneNote (const juce::ValueTree& state, int note, int velocity, double holdSeconds,
                                                                double tailSeconds, const std::function<void (float)>& progress,
                                                                const std::atomic<bool>* cancel)
{
    constexpr double bounceRate = 48000.0;
    constexpr int blockSize = 512;
    auto renderer = std::make_unique<IlanaSynthAudioProcessor>();
    renderer->applyFullState (state);
    renderer->flushAsyncUpdates(); // file-backed samples and tables, now
    renderer->setNonRealtime (true);
    renderer->prepareToPlay (bounceRate, blockSize);

    note = juce::jlimit (0, 127, note);
    const auto holdSamples = (int) (juce::jlimit (0.05, 30.0, holdSeconds) * bounceRate);
    const auto totalSamples = holdSamples + (int) (juce::jlimit (0.0, 30.0, tailSeconds) * bounceRate);
    const auto channels = juce::jmax (2, juce::jmax (renderer->getTotalNumInputChannels(), renderer->getTotalNumOutputChannels()));

    juce::AudioBuffer<float> out (2, totalSamples);
    juce::AudioBuffer<float> block (channels, blockSize);
    for (int start = 0; start < totalSamples; start += blockSize)
    {
        if (cancel != nullptr && cancel->load())
            return {};
        const auto count = juce::jmin (blockSize, totalSamples - start);
        juce::MidiBuffer midi;
        if (start == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)), 0);
        if (holdSamples >= start && holdSamples < start + blockSize)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), holdSamples - start);
        block.setSize (channels, count, false, false, true);
        block.clear();
        renderer->processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, juce::jmin (ch, block.getNumChannels() - 1), 0, count);
        if (progress)
            progress ((float) (start + count) / (float) totalSamples);
    }
    renderer->releaseResources();
    return out;
}

namespace
{
// Trims the silent tail (80 dB under the peak), fades the last 10 ms and
// folds identical channels into one. The gain is left alone.
juce::AudioBuffer<float> trimRender (const juce::AudioBuffer<float>& raw, double holdSeconds, float peak)
{
    const auto totalSamples = raw.getNumSamples();
    const auto holdSamples = juce::jmin (totalSamples, (int) (juce::jlimit (0.05, 30.0, holdSeconds) * bounceRate));
    auto length = totalSamples;
    while (length > holdSamples / 4 && std::abs (raw.getSample (0, length - 1)) < peak * 1.0e-4f
           && std::abs (raw.getSample (1, length - 1)) < peak * 1.0e-4f)
        --length;
    length = juce::jmin (totalSamples, length + (int) (0.01 * bounceRate));
    const auto fade = juce::jmin (length / 2, (int) (0.01 * bounceRate));
    auto mono = true;
    for (int i = 0; i < length && mono; ++i)
        mono = std::abs (raw.getSample (0, i) - raw.getSample (1, i)) < 1.0e-6f;

    juce::AudioBuffer<float> trimmed (mono ? 1 : 2, length);
    for (int ch = 0; ch < trimmed.getNumChannels(); ++ch)
    {
        trimmed.copyFrom (ch, 0, raw, ch, 0, length);
        trimmed.applyGainRamp (ch, length - fade, fade, 1.0f, 0.0f);
    }
    return trimmed;
}
} // namespace

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::renderBounce (const juce::ValueTree& source, const BounceRequest& request,
                                                                   std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
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

    auto result = std::make_shared<SampleData>();
    result->sampleRate = bounceRate;

    if (! request.multi)
    {
        const std::function<void (float)> report = [progress] (float fraction)
        {
            if (progress != nullptr)
                progress->store (fraction);
        };
        const auto raw = playOneNote (state, request.note, request.velocity, request.holdSeconds, request.tailSeconds, report, cancel);
        if (raw.getNumSamples() == 0)
            return nullptr;
        const auto peak = raw.getMagnitude (0, raw.getNumSamples());
        if (peak < 1.0e-5f || ! std::isfinite (peak))
            return nullptr;
        // Normalised to -1 dBFS.
        result->buffer = trimRender (raw, request.holdSeconds, peak);
        result->buffer.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);
        result->name = "Bounce " + juce::MidiMessage::getMidiNoteName (request.note, true, true, 4);
        return result;
    }

    // A multisample: one render per note and velocity layer, each a zone. The
    // key ranges meet halfway between neighbouring roots (the first and last
    // reach the keyboard's ends) and the velocity ranges split 1-127 evenly.
    const auto notes = request.multiNotes();
    const auto velocities = request.multiVelocities();
    const auto total = (int) (notes.size() * velocities.size());
    const auto layers = (int) velocities.size();
    std::vector<SampleZone> zones;
    float loudest = 0.0f;
    juce::int64 bytes = 0;
    auto done = 0;
    for (size_t n = 0; n < notes.size(); ++n)
        for (size_t v = 0; v < velocities.size(); ++v)
        {
            const std::function<void (float)> report = [&] (float fraction)
            {
                if (progress != nullptr)
                    progress->store (((float) done + fraction) / (float) total);
            };
            const auto raw = playOneNote (state, notes[n], velocities[v], request.holdSeconds, request.tailSeconds, report, cancel);
            if (raw.getNumSamples() == 0)
                return nullptr;
            ++done;
            const auto peak = raw.getMagnitude (0, raw.getNumSamples());
            if (! std::isfinite (peak))
                return nullptr;
            if (peak < 1.0e-5f)
                continue; // a silent note leaves its neighbours to cover the keys
            SampleZone zone;
            zone.data = std::make_shared<SampleData>();
            zone.data->sampleRate = bounceRate;
            zone.data->buffer = trimRender (raw, request.holdSeconds, peak);
            zone.data->name = juce::MidiMessage::getMidiNoteName (notes[n], true, true, 4);
            zone.rootNote = notes[n];
            zone.loKey = n == 0 ? 0 : (notes[n - 1] + notes[n]) / 2 + 1;
            zone.hiKey = n + 1 == notes.size() ? 127 : (notes[n] + notes[n + 1]) / 2;
            zone.loVel = v == 0 ? 1 : (int) (v * 127 / (size_t) layers) + 1;
            zone.hiVel = v + 1 == (size_t) layers ? 127 : (int) ((v + 1) * 127 / (size_t) layers);
            loudest = juce::jmax (loudest, peak);
            bytes += (juce::int64) zone.data->getNumSamples() * zone.data->getNumChannels() * 2;
            if (bytes > maxMultisampleBytes)
                return nullptr;
            zones.push_back (std::move (zone));
        }
    if (zones.empty() || loudest < 1.0e-5f)
        return nullptr;

    // One gain for all of them: the loudest render reaches -1 dBFS and the
    // others keep their place, as the patch played them.
    const auto gain = juce::Decibels::decibelsToGain (-1.0f) / loudest;
    for (auto& zone : zones)
        zone.data->buffer.applyGain (gain);
    result->zones = std::move (zones);
    MultiSample::finish (*result);
    result->name = "Resample " + juce::MidiMessage::getMidiNoteName (notes.front(), true, true, 4) + "-"
                   + juce::MidiMessage::getMidiNoteName (notes.back(), true, true, 4);
    return result;
}

bool IlanaSynthAudioProcessor::applyBounce (const BounceRequest& request, std::shared_ptr<SampleData> audio, juce::String& message)
{
    if (audio == nullptr)
    {
        message = request.multi ? "The resample was silent, or too big to keep inside the patch (over about 3 minutes of audio): "
                                  "use fewer notes or velocity layers, or shorter lengths. Nothing was put on the oscillator."
                                : "The bounce was silent: nothing was put on the oscillator.";
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

    if (request.toTable && ! request.multi)
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
        // Tuned samples play at pitch on C4: shift the bounced note back. A
        // multisample's zones each carry their root note.
        set (prefix + "_semi", audio->zones.empty() ? (float) juce::jlimit (-24, 24, 60 - request.note) : 0.0f);
        for (const auto* id : { "_sample_loop", "_sample_reverse", "_sample_start", "_sample_end", "_sample_fade_in", "_sample_fade_out",
                                "_sample_sync", "_sample_bpm", "_sample_stretch",
                                "_sample_loop_own", "_sample_loop_start", "_sample_loop_end",
                                "_sample_loop_xfade", "_sample_snap", "_sample_playback" })
            reset (prefix + id);
        if (! audio->zones.empty())
        {
            auto totalSeconds = 0.0;
            for (const auto& zone : audio->zones)
                totalSeconds += (double) zone.data->getNumSamples() / zone.data->sampleRate;
            message = "Resampled into " + oscName + " as a multisample of " + juce::String ((int) audio->zones.size()) + " zones ("
                      + juce::String (juce::roundToInt (totalSeconds)) + " s of audio), saved inside the patch.";
        }
        else
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
    // A table bounced onto an operator keeps its tuning by ratio or its
    // Operator EG, as before: it stays FM / DX7.
    migrateOperatorModes();
    return true;
}

bool IlanaSynthAudioProcessor::exportSampleWav (int oscIndex, const juce::File& target, bool playedRange, juce::String& error) const
{
    const auto sample = getSharedSampleForOsc (oscIndex);
    if (sample == nullptr || sample->getNumSamples() < 2 || sample->sampleRate <= 0.0)
    {
        error = "There is no sample on this oscillator to export.";
        return false;
    }

    const auto length = sample->getNumSamples();
    auto from = 0, to = length;
    const juce::String prefix (OscillatorIds::prefixes[(size_t) oscIndex]);
    const auto plain = [this] (const juce::String& id)
    {
        const auto* value = apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };
    if (playedRange)
    {
        // The same rounding as the player's region.
        const auto start = juce::jlimit (0.0f, 0.999f, plain (prefix + "_sample_start"));
        from = juce::jlimit (0, length - 1, (int) ((double) start * length));
        to = juce::jlimit (from + 1, length, (int) ((double) juce::jlimit (0.0f, 1.0f, plain (prefix + "_sample_end")) * length));
    }

    const auto frames = to - from;
    juce::AudioBuffer<float> out (sample->getNumChannels(), frames);
    for (int ch = 0; ch < out.getNumChannels(); ++ch)
        out.copyFrom (ch, 0, sample->buffer, ch, from, frames);
    if (playedRange)
    {
        if (plain (prefix + "_sample_reverse") > 0.5f)
            out.reverse (0, frames);
        // FADE IN / OUT are fractions of the played range, as the player has them.
        const auto fadeIn = (int) (plain (prefix + "_sample_fade_in") * (float) frames);
        const auto fadeOut = (int) (plain (prefix + "_sample_fade_out") * (float) frames);
        if (fadeIn > 1)
            out.applyGainRamp (0, juce::jmin (fadeIn, frames), 0.0f, 1.0f);
        if (fadeOut > 1)
            out.applyGainRamp (frames - juce::jmin (fadeOut, frames), juce::jmin (fadeOut, frames), 1.0f, 0.0f);
    }

    // Write beside the target first, so a failed write never leaves half a file in its place.
    juce::TemporaryFile temp (target);
    {
        std::unique_ptr<juce::OutputStream> stream (temp.getFile().createOutputStream());
        if (stream == nullptr)
        {
            error = "Couldn't write to " + target.getParentDirectory().getFullPathName() + ".";
            return false;
        }
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sample->sampleRate, (unsigned int) out.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
        {
            error = "This sample rate can't be written as a WAV.";
            return false;
        }
        stream.release(); // the writer owns it now
        if (! writer->writeFromAudioSampleBuffer (out, 0, frames))
        {
            error = "Writing the WAV stopped part way (is the disk full?).";
            return false;
        }
    }
    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Couldn't replace " + target.getFileName() + ".";
        return false;
    }
    error.clear();
    return true;
}

juce::ValueTree IlanaSynthAudioProcessor::encodeSample (const SampleData& data)
{
    juce::ValueTree tree ("Sample");
    tree.setProperty ("name", data.name, nullptr);
    if (! data.zones.empty())
    {
        // A multisample (roadmap step 41): its zones, each a sample of its
        // own; the displayed region is rebuilt on loading.
        for (const auto& zone : data.zones)
        {
            if (zone.data == nullptr)
                continue;
            auto child = encodeSample (*zone.data);
            child.setProperty ("zone", true, nullptr);
            child.setProperty ("loKey", zone.loKey, nullptr);
            child.setProperty ("hiKey", zone.hiKey, nullptr);
            child.setProperty ("loVel", zone.loVel, nullptr);
            child.setProperty ("hiVel", zone.hiVel, nullptr);
            child.setProperty ("root", zone.rootNote, nullptr);
            child.setProperty ("gain", (double) zone.gain, nullptr);
            child.setProperty ("loop", zone.loop, nullptr);
            child.setProperty ("loopStart", zone.loopStart, nullptr);
            child.setProperty ("loopEnd", zone.loopEnd, nullptr);
            tree.appendChild (child, nullptr);
        }
        return tree;
    }
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

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::decodeSample (const juce::ValueTree& tree, int depth)
{
    if (tree.getNumChildren() > 0)
    {
        if (depth > 0) // a multisample's regions are plain samples, never multisamples again
            return nullptr;
        auto data = std::make_shared<SampleData>();
        data->name = tree.getProperty ("name").toString();
        for (int i = 0; i < tree.getNumChildren() && (int) data->zones.size() < 256; ++i)
        {
            const auto child = tree.getChild (i);
            auto audio = decodeSample (child, depth + 1);
            if (audio == nullptr)
                continue;
            SampleZone zone;
            zone.data = std::move (audio);
            zone.loKey = juce::jlimit (0, 127, (int) child.getProperty ("loKey", 0));
            zone.hiKey = juce::jlimit (0, 127, (int) child.getProperty ("hiKey", 127));
            zone.loVel = juce::jlimit (1, 127, (int) child.getProperty ("loVel", 1));
            zone.hiVel = juce::jlimit (1, 127, (int) child.getProperty ("hiVel", 127));
            zone.rootNote = juce::jlimit (0.0, 127.0, (double) child.getProperty ("root", 60.0));
            zone.gain = juce::jlimit (0.0f, 8.0f, (float) (double) child.getProperty ("gain", 1.0));
            const auto length = zone.data->getNumSamples();
            zone.loopStart = juce::jlimit (0, length, (int) child.getProperty ("loopStart", 0));
            zone.loopEnd = juce::jlimit (0, length, (int) child.getProperty ("loopEnd", 0));
            zone.loop = (bool) child.getProperty ("loop", false) && zone.loopEnd - zone.loopStart > 16;
            data->zones.push_back (std::move (zone));
        }
        if (data->zones.empty())
            return nullptr;
        MultiSample::finish (*data);
        return data;
    }
    const auto rate = (double) tree.getProperty ("rate", 0.0);
    const auto channels = (int) tree.getProperty ("channels", 0);
    const auto length = (int) tree.getProperty ("length", 0);
    if (rate < 1000.0 || rate > 400000.0 || channels < 1 || channels > 2 || length < 2 || length > (int) (rate * 120.0))
        return nullptr;
    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (tree.getProperty ("data").toString()))
        return nullptr;
    // Deflate never packs better than about 1000:1: a claimed length far beyond
    // that is false, and is refused before the buffer is allocated.
    if ((juce::int64) length * channels * 2 > (juce::int64) block.getSize() * 1100)
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
