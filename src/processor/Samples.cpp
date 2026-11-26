#include "ProcessorInternal.h"
#include "../dsp/MultiSample.h"
#include "../dsp/PitchDetect.h"
#include "../SafeXml.h"
#include "../SamplePaths.h"
#include "../ThreadChecks.h"

// The oscillators' user samples: decoding a file (audio, SoundFont, SFZ), the
// zone map and slices kept with it, and handing it to the audio thread.
// (Moved out of PluginProcessor.cpp unchanged, code health before RC 1.0.)

const SampleData* IlanaSynthAudioProcessor::getSampleForOsc (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return nullptr;

    const auto factoryIndex = (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]);

    if (factoryIndex > 0 && factoryIndex <= (int) factorySamples.size())
        return factorySamples[(size_t) (factoryIndex - 1)].get();

    const SampleData* user = nullptr;

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        user = sampleSlots[(size_t) oscIndex].get();
    }

    // Granular with nothing loaded: grains from the vocal sample rather than silence.
    if (user == nullptr && ! factorySamples.empty())
        if ((int) getRawParam (stringParamIds[(size_t) oscIndex][0]) == 3) // osc mode: Granular
            return factorySamples[(size_t) juce::jmin (1, (int) factorySamples.size() - 1)].get();

    return user;
}

juce::String IlanaSynthAudioProcessor::detectSampleRoot (int oscIndex)
{
    const auto* sample = getSampleForOsc (oscIndex);
    if (sample == nullptr || sample->getNumSamples() < 2)
        return "Load a sample first.";
    if (! sample->zones.empty())
        return "A multisample keeps the root note of each zone.";

    const auto found = PitchDetect::detect (sample->buffer, sample->sampleRate);
    if (! found.found())
        return "No clear pitch found. Set ROOT by hand.";

    const auto note = juce::jlimit (0, 127, (int) std::lround (found.midiNote));
    const auto cents = juce::jlimit (-50, 50, (int) std::lround ((found.midiNote - note) * 100.0));
    const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) oscIndex]);
    const auto set = [this, &prefix] (const char* suffix, float value)
    {
        if (auto* parameter = apvts.getParameter (prefix + suffix))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    };
    // The cents go to the oscillator's FINE (its play-pitch offset), the same
    // correction a sample that is a little off its note needs.
    set ("_sample_root", (float) note);
    set ("_fine", (float) -cents);
    return "Root set to " + describeValue (prefix + "_sample_root", (float) note) + (cents != 0 ? ", FINE " + juce::String (-cents) + " ct" : juce::String())
           + " (" + juce::String (found.frequency, 1) + " Hz).";
}

std::shared_ptr<const SampleData> IlanaSynthAudioProcessor::getSharedSampleForOsc (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return nullptr;

    const auto factoryIndex = (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]);
    if (factoryIndex > 0 && factoryIndex <= (int) factorySamples.size())
        return factorySamples[(size_t) (factoryIndex - 1)];

    std::shared_ptr<const SampleData> user;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        user = sampleSlots[(size_t) oscIndex];
    }
    if (user == nullptr && ! factorySamples.empty())
        if ((int) getRawParam (stringParamIds[(size_t) oscIndex][0]) == 3)
            return factorySamples[(size_t) juce::jmin (1, (int) factorySamples.size() - 1)];
    return user;
}

namespace
{
// "3:24" or "45 s", for the import messages.
juce::String describeDuration (double seconds)
{
    const auto whole = juce::roundToInt (std::floor (seconds));
    if (whole < 60)
        return juce::String (whole) + " s";
    return juce::String (whole / 60) + ":" + juce::String (whole % 60).paddedLeft ('0', 2);
}
} // namespace

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::decodeAudioFile (const juce::File& file, juce::String& error)
{
    error.clear();
    if (! file.existsAsFile())
    {
        error = "the file is not there any more.";
        return nullptr;
    }
    if (file.getSize() == 0)
    {
        error = "the file is empty.";
        return nullptr;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        if (formats.findFormatForFileExtension (file.getFileExtension()) == nullptr)
            error = "this kind of file (" + file.getFileExtension().toLowerCase() + ") can't be read here. WAV, AIFF, FLAC, OGG and MP3 can.";
        else
            error = "the audio in it could not be decoded. It may be damaged, or use a codec that isn't supported "
                    "(for example compressed WAV). Saving it again as a plain WAV usually helps.";
        return nullptr;
    }

    if (reader->lengthInSamples <= 1 || reader->sampleRate <= 0.0)
    {
        error = "it holds no audio.";
        return nullptr;
    }

    const auto maxSamples = (juce::int64) (reader->sampleRate * 120.0);
    const auto numSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);

    if (numSamples <= 1)
    {
        error = "it holds no audio.";
        return nullptr;
    }

    const auto numChannels = juce::jlimit (1, 2, (int) reader->numChannels);

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();
    data->buffer.setSize (numChannels, numSamples);

    if (! reader->read (&data->buffer, 0, numSamples, 0, true, true))
    {
        error = "reading the audio stopped part way. The file may be damaged.";
        return nullptr;
    }

    // Samples stop at two minutes: say so, with how long the file was.
    if (reader->lengthInSamples > maxSamples)
        data->loadWarning = "Cut at 2:00 of " + describeDuration ((double) reader->lengthInSamples / reader->sampleRate)
                            + ": samples stop at 120 seconds.";
    if (reader->numChannels > 2)
        data->loadWarning << (data->loadWarning.isEmpty() ? "" : " ") << "Only the first two of its "
                          << (int) reader->numChannels << " channels are played.";

    scrubAudio (data->buffer);
    return data;
}

IlanaSynthAudioProcessor::DecodedSample IlanaSynthAudioProcessor::decodeUserFile (const juce::File& file, int bank, int program)
{
    DecodedSample decoded;
    // Roadmap step 43: every way out says why, so a file that does nothing
    // is never a mystery.
    if (! file.existsAsFile())
    {
        decoded.error = "the file is not there any more.";
        return decoded;
    }
    if (file.getSize() == 0)
    {
        decoded.error = "the file is empty.";
        return decoded;
    }

    juce::String error;
    decoded.multi = MultiSample::isMultiSampleFile (file);
    // SoundFont and SFZ instruments: a multisample, its regions picked per note.
    if (decoded.multi)
    {
        MultiSample::SfzReport sfz;
        // Step 38: a SoundFont reloads the preset picked from its list.
        decoded.data = MultiSample::load (file, error, &sfz, bank, program);
        if (decoded.data == nullptr)
        {
            decoded.error = error.isNotEmpty() ? error + "." : juce::String ("the instrument could not be read.");
            return decoded;
        }
        for (const auto& missing : sfz.missing())
            decoded.missing.addIfNotAlreadyThere (missing.getFileName());
    }
    else
    {
        decoded.data = decodeAudioFile (file, error);
        if (decoded.data == nullptr)
            decoded.error = error;
    }
    return decoded;
}

juce::ValueTree IlanaSynthAudioProcessor::startSampleLoad (int oscIndex, const juce::File& file, bool fromUser)
{
    ++sampleEpoch;
    ++dataEpoch; // what the editor draws changes
    lastSampleError.clear();

    // A file chosen by hand starts clean; a reload (a patch, undo, a preset)
    // brings back the file's saved options (step 36: an edited zone map).
    juce::ValueTree options;
    if (fromUser)
        setSampleOptions (oscIndex, {});
    else if (const auto xml = SafeXml::parse (getSampleOptions (oscIndex)))
        if (const auto tree = juce::ValueTree::fromXml (*xml); tree.isValid() && tree.getProperty ("path").toString() == file.getFullPathName())
            options = tree;
    return options;
}

bool IlanaSynthAudioProcessor::finishSampleLoad (int oscIndex, const juce::File& file, DecodedSample decoded,
                                                 const juce::ValueTree& options, bool fromUser)
{
    if (decoded.data == nullptr)
    {
        lastSampleError = "Couldn't load " + file.getFileName() + ": " + decoded.error;
        return false;
    }

    auto data = std::move (decoded.data);
    // Step 32: a plain sample saved sliced is cut again the same way; the
    // zone edits after it refer to its slices.
    if (options.isValid() && ! decoded.multi && options.getChildWithName ("Slices").isValid())
        data = SampleSlicer::slice (*data, SampleSlicer::read (options.getChildWithName ("Slices")));
    if (options.isValid())
        data = applyZoneOptions (std::move (data), options, file);

    setUserSample (oscIndex, std::move (data), file.getFullPathName());
    if (decoded.multi)
    {
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            sfzMissingFiles[(size_t) oscIndex] = decoded.missing;
        }
        ++missingSamplesVersion;
    }
    useLoadedSample (oscIndex, fromUser);
    return true;
}

bool IlanaSynthAudioProcessor::loadUserSample (int oscIndex, const juce::File& file, bool fromUser)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
    {
        ++sampleEpoch;
        ++dataEpoch;
        lastSampleError.clear();
        return false;
    }

    ++sampleRequests[(size_t) oscIndex]; // (a load still decoding in the background is out of date)
    const auto options = startSampleLoad (oscIndex, file, fromUser);
    auto decoded = decodeUserFile (file, (int) options.getProperty ("bank", -1), (int) options.getProperty ("program", -1));
    return finishSampleLoad (oscIndex, file, std::move (decoded), options, fromUser);
}

void IlanaSynthAudioProcessor::loadUserSampleInBackground (int oscIndex, const juce::File& file,
                                                           std::function<void (SampleLoadResult)> done)
{
    ILANA_ASSERT_MESSAGE_THREAD();
    // (Shared: a thread that can't be started destroys the job, and the
    // fallback below still has to answer.)
    auto finished = std::make_shared<std::function<void (SampleLoadResult)>> (std::move (done));
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
    {
        if (*finished != nullptr)
            (*finished) (SampleLoadResult::failed);
        return;
    }

    const auto request = ++sampleRequests[(size_t) oscIndex];
    auto job = [this, life = getLifetime(), oscIndex, file, request, done = finished]
    {
        // (Held until the result is posted: the destructor waits for it.)
        const auto alive = life.lock();
        if (alive == nullptr || isShuttingDown())
            return;

        auto decoded = std::make_shared<DecodedSample>();
        if (sampleRequests[(size_t) oscIndex].load() == request)
        {
            *decoded = decodeUserFile (file, -1, -1);
            // (The attacks a random start can land on, found here rather
            // than on the message thread: setUserSample skips found ones.)
            if (decoded->data != nullptr)
            {
                decoded->data->findOnsets();
                for (auto& zone : decoded->data->zones)
                    if (zone.data != nullptr)
                        zone.data->findOnsets();
            }
        }

        if (isShuttingDown()) // (a long decode outlived the plug-in's window)
            return;
        juce::MessageManager::callAsync ([this, life, oscIndex, file, request, decoded, done]
        {
            const auto stillAlive = life.lock();
            if (stillAlive == nullptr || isShuttingDown())
                return;

            // A newer load, a patch or a preset took the oscillator meanwhile.
            if (sampleRequests[(size_t) oscIndex].load() != request)
            {
                if (*done != nullptr)
                    (*done) (SampleLoadResult::replaced);
                return;
            }

            const auto options = startSampleLoad (oscIndex, file, true);
            const auto loaded = finishSampleLoad (oscIndex, file, std::move (*decoded), options, true);
            if (*done != nullptr)
                (*done) (loaded ? SampleLoadResult::loaded : SampleLoadResult::failed);
        });
    };

    if (! juce::Thread::launch (std::move (job)))
    {
        // (No thread to be had: load here, as before.)
        const auto loaded = loadUserSample (oscIndex, file, true);
        if (*finished != nullptr)
            (*finished) (loaded ? SampleLoadResult::loaded : SampleLoadResult::failed);
    }
}

void IlanaSynthAudioProcessor::cancelBackgroundSampleLoads()
{
    for (auto& request : sampleRequests)
        ++request;
}

SampleSlicer::Settings IlanaSynthAudioProcessor::getSampleSlices (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs || (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0)
        return {};
    std::shared_ptr<SampleData> current;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        current = sampleSlots[(size_t) oscIndex];
    }
    return current != nullptr ? SampleSlicer::settingsOf (*current) : SampleSlicer::Settings {};
}

bool IlanaSynthAudioProcessor::setSampleSlices (int oscIndex, const SampleSlicer::Settings& settings, const juce::String& undoName)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs || (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0)
        return false;
    std::shared_ptr<SampleData> current;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        current = sampleSlots[(size_t) oscIndex];
    }
    // A plain sample, or one already sliced (cut again from the whole).
    if (current == nullptr || current->getNumSamples() < 2 || (! current->zones.empty() && current->sliceMode == 0))
        return false;

    const auto whole = SampleSlicer::wholeOf (*current);
    auto data = SampleSlicer::slice (*whole, settings);

    juce::String path;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        path = samplePaths[(size_t) oscIndex];
    }

    beginEdit (undoName);
    if (path.isNotEmpty())
    {
        // Saved with the file's options: how it is cut. Edits to the old
        // slices (the zone map) don't carry over to new ones.
        juce::ValueTree options ("SampleOptions");
        if (const auto xml = SafeXml::parse (getSampleOptions (oscIndex)))
            if (const auto old = juce::ValueTree::fromXml (*xml); old.isValid() && old.getProperty ("path").toString() == path)
                options = old.createCopy();
        options.setProperty ("path", path, nullptr);
        options.removeChild (options.getChildWithName ("Zones"), nullptr);
        options.removeChild (options.getChildWithName ("Slices"), nullptr);
        if (data->sliceMode != 0)
        {
            juce::ValueTree slices ("Slices");
            SampleSlicer::write (slices, SampleSlicer::settingsOf (*data));
            options.appendChild (slices, nullptr);
        }
        setUserSample (oscIndex, data, path);
        setSampleOptions (oscIndex, options.getNumChildren() > 0 || options.getNumProperties() > 1 ? options.toXmlString() : juce::String());
    }
    else
        setUserSample (oscIndex, data, {}); // kept in the patch: its slices travel with it
    endEdit();
    return true;
}

std::pair<int, int> IlanaSynthAudioProcessor::getSoundFontPreset (int oscIndex) const
{
    if (const auto xml = SafeXml::parse (getSampleOptions (oscIndex)))
        if (const auto tree = juce::ValueTree::fromXml (*xml); tree.isValid() && tree.getProperty ("path").toString() == getSamplePath (oscIndex))
            return { (int) tree.getProperty ("bank", -1), (int) tree.getProperty ("program", -1) };
    return { -1, -1 };
}

bool IlanaSynthAudioProcessor::selectSoundFontPreset (int oscIndex, int bank, int program)
{
    const juce::File file (getSamplePath (oscIndex));
    if (! file.hasFileExtension ("sf2"))
        return false;
    // A new preset starts with its own zones: an edited map belonged to the old one.
    juce::ValueTree options ("SampleOptions");
    options.setProperty ("path", file.getFullPathName(), nullptr);
    options.setProperty ("bank", bank, nullptr);
    options.setProperty ("program", program, nullptr);
    const auto previous = getSampleOptions (oscIndex);
    setSampleOptions (oscIndex, options.toXmlString());
    if (loadUserSample (oscIndex, file, false))
        return true;
    setSampleOptions (oscIndex, previous);
    return false;
}

juce::String IlanaSynthAudioProcessor::getSampleOptions (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return {};
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return sampleOptions[(size_t) oscIndex];
}

void IlanaSynthAudioProcessor::setSampleOptions (int oscIndex, const juce::String& options)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return;
    ++dataEpoch;
    const juce::SpinLock::ScopedLockType lock (stateLock);
    sampleOptions[(size_t) oscIndex] = options;
}

void IlanaSynthAudioProcessor::retargetSampleOptions (int oscIndex, const juce::String& newPath)
{
    if (const auto xml = SafeXml::parse (getSampleOptions (oscIndex)))
        if (auto tree = juce::ValueTree::fromXml (*xml); tree.isValid())
        {
            tree.setProperty ("path", newPath, nullptr);
            setSampleOptions (oscIndex, tree.toXmlString());
        }
}

std::shared_ptr<SampleData> IlanaSynthAudioProcessor::applyZoneOptions (std::shared_ptr<SampleData> loaded, const juce::ValueTree& options,
                                                                        const juce::File& file)
{
    const auto map = options.getChildWithName ("Zones");
    if (! map.isValid() || map.getNumChildren() == 0 || loaded == nullptr)
        return loaded;

    // Each saved zone: one of the file's own (by its index) or an added
    // audio file, with its edited keys, velocities, root and level.
    std::map<juce::String, std::shared_ptr<SampleData>> added;
    added[file.getFullPathName()] = loaded->zones.empty() ? loaded : nullptr;
    std::vector<SampleZone> zones;
    juce::StringArray missing;
    for (const auto& entry : map)
    {
        SampleZone zone;
        const auto origin = (int) entry.getProperty ("origin", -1);
        const auto path = entry.getProperty ("file").toString();
        if (origin >= 0 && origin < (int) loaded->zones.size())
            zone = loaded->zones[(size_t) origin];
        else if (path.isNotEmpty())
        {
            auto& audio = added[path];
            if (audio == nullptr)
            {
                juce::String error;
                audio = decodeAudioFile (juce::File (path), error);
            }
            if (audio == nullptr)
            {
                missing.addIfNotAlreadyThere (juce::File (path).getFileName());
                continue;
            }
            zone.data = audio;
            zone.file = path;
        }
        else
            continue;
        zone.loKey = juce::jlimit (0, 127, (int) entry.getProperty ("loKey", zone.loKey));
        zone.hiKey = juce::jlimit (zone.loKey, 127, (int) entry.getProperty ("hiKey", zone.hiKey));
        zone.loVel = juce::jlimit (1, 127, (int) entry.getProperty ("loVel", zone.loVel));
        zone.hiVel = juce::jlimit (zone.loVel, 127, (int) entry.getProperty ("hiVel", zone.hiVel));
        zone.rootNote = juce::jlimit (-24.0, 151.0, (double) entry.getProperty ("root", zone.rootNote));
        zone.gain = juce::jlimit (0.0f, 8.0f, (float) (double) entry.getProperty ("gain", (double) zone.gain));
        zone.reverse = (bool) entry.getProperty ("reverse", zone.reverse);
        zones.push_back (std::move (zone));
    }
    if (zones.empty())
        return loaded;

    auto data = std::make_shared<SampleData>();
    data->name = loaded->name;
    data->zones = std::move (zones);
    data->loadWarning = loaded->loadWarning;
    if (! missing.isEmpty())
        data->loadWarning << (data->loadWarning.isEmpty() ? "" : " ") << "Zones whose files are gone were left out ("
                          << missing.joinIntoString (", ") << ").";
    MultiSample::finish (*data);
    data->copySlicingFrom (*loaded); // step 32: a sliced sample still shows the whole
    return data;
}

std::vector<SampleZone> IlanaSynthAudioProcessor::getSampleZones (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs || (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0)
        return {};
    std::shared_ptr<SampleData> user;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        user = sampleSlots[(size_t) oscIndex];
    }
    return user != nullptr ? user->zones : std::vector<SampleZone> {};
}

bool IlanaSynthAudioProcessor::setSampleZones (int oscIndex, std::vector<SampleZone> zones, const juce::String& undoName)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs || zones.empty() || (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0)
        return false;
    std::shared_ptr<SampleData> current;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        current = sampleSlots[(size_t) oscIndex];
    }
    if (current == nullptr)
        return false;
    zones.erase (std::remove_if (zones.begin(), zones.end(), [] (const SampleZone& zone) { return zone.data == nullptr; }), zones.end());
    if (zones.empty())
        return false;
    for (auto& zone : zones)
    {
        zone.loKey = juce::jlimit (0, 127, zone.loKey);
        zone.hiKey = juce::jlimit (zone.loKey, 127, zone.hiKey);
        zone.loVel = juce::jlimit (1, 127, zone.loVel);
        zone.hiVel = juce::jlimit (zone.loVel, 127, zone.hiVel);
    }

    auto data = std::make_shared<SampleData>();
    data->name = current->name;
    data->loadWarning = current->loadWarning;
    data->zones = zones;
    MultiSample::finish (*data);
    data->copySlicingFrom (*current); // step 32: a sliced sample still shows the whole
    // (finish numbers zones by place only when they came from no file: an
    // edited zone keeps the origin it was read with.)

    juce::String path;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        path = samplePaths[(size_t) oscIndex];
    }

    beginEdit (undoName);
    if (path.isNotEmpty())
    {
        // Saved as edits to the file: which of its zones (or which added
        // file) each one plays, with its keys, velocities, root and level.
        juce::ValueTree options ("SampleOptions");
        if (const auto xml = SafeXml::parse (getSampleOptions (oscIndex)))
            if (const auto old = juce::ValueTree::fromXml (*xml); old.isValid() && old.getProperty ("path").toString() == path)
                options = old.createCopy();
        options.setProperty ("path", path, nullptr);
        options.removeChild (options.getChildWithName ("Zones"), nullptr);
        juce::ValueTree map ("Zones");
        for (const auto& zone : zones)
        {
            juce::ValueTree entry ("Zone");
            if (zone.file.isNotEmpty())
                entry.setProperty ("file", zone.file, nullptr);
            else
                entry.setProperty ("origin", zone.origin, nullptr);
            entry.setProperty ("loKey", zone.loKey, nullptr);
            entry.setProperty ("hiKey", zone.hiKey, nullptr);
            entry.setProperty ("loVel", zone.loVel, nullptr);
            entry.setProperty ("hiVel", zone.hiVel, nullptr);
            entry.setProperty ("root", zone.rootNote, nullptr);
            entry.setProperty ("gain", (double) zone.gain, nullptr);
            if (zone.reverse)
                entry.setProperty ("reverse", true, nullptr);
            map.appendChild (entry, nullptr);
        }
        options.appendChild (map, nullptr);
        setUserSample (oscIndex, data, path);
        setSampleOptions (oscIndex, options.toXmlString());
    }
    else
        setUserSample (oscIndex, data, {}); // kept in the patch: the zones travel with it
    endEdit();
    return true;
}

bool IlanaSynthAudioProcessor::addSampleZone (int oscIndex, const juce::File& file, int loKey, int hiKey, int loVel, int hiVel, int rootKey,
                                              juce::String& error)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;
    auto audio = decodeAudioFile (file, error);
    if (audio == nullptr)
    {
        error = "Couldn't add " + file.getFileName() + ": " + error;
        return false;
    }
    audio->findOnsets();

    std::shared_ptr<SampleData> current;
    juce::String path;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        current = (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0 ? nullptr : sampleSlots[(size_t) oscIndex];
    }
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        path = samplePaths[(size_t) oscIndex];
    }

    std::vector<SampleZone> zones;
    if (current != nullptr && ! current->zones.empty())
        zones = current->zones;
    else if (current != nullptr && current->getNumSamples() > 1)
    {
        // A plain sample becomes the map's first zone: every key, rooted where it plays now.
        SampleZone first;
        first.data = current;
        first.file = path;
        first.rootNote = getRawParam (sampleRootIds[(size_t) oscIndex]); // its ROOT (step 29)
        zones.push_back (first);
    }

    SampleZone zone;
    zone.data = audio;
    zone.file = file.getFullPathName();
    zone.loKey = juce::jlimit (0, 127, juce::jmin (loKey, hiKey));
    zone.hiKey = juce::jlimit (zone.loKey, 127, juce::jmax (loKey, hiKey));
    zone.loVel = juce::jlimit (1, 127, juce::jmin (loVel, hiVel));
    zone.hiVel = juce::jlimit (zone.loVel, 127, juce::jmax (loVel, hiVel));
    zone.rootNote = juce::jlimit (0, 127, rootKey);
    zones.push_back (zone);

    if (current == nullptr)
    {
        // Nothing loaded: the added file is the whole multisample.
        auto data = std::make_shared<SampleData>();
        data->name = file.getFileNameWithoutExtension();
        data->zones = zones;
        MultiSample::finish (*data);
        beginEdit ("Add a zone");
        setUserSample (oscIndex, data, {});
        endEdit();
        if (auto* parameter = apvts.getParameter (sampleFactoryIds[(size_t) oscIndex]))
            if ((int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) > 0)
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
        return true;
    }
    return setSampleZones (oscIndex, std::move (zones), "Add a zone");
}

void IlanaSynthAudioProcessor::useLoadedSample (int oscIndex, bool fromUser)
{
    // A chosen or dropped file replaces a factory sample source, as a bounce does.
    // (Not on a state restore: a patch may keep a path next to a factory pick.)
    if (! fromUser || (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]) == 0)
        return;

    if (auto* parameter = apvts.getParameter (sampleFactoryIds[(size_t) oscIndex]))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
}

void IlanaSynthAudioProcessor::setUserSample (int oscIndex, std::shared_ptr<SampleData> data, const juce::String& path)
{
    ++sampleEpoch;
    ++dataEpoch; // what the editor draws changes
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return;
    ++sampleRequests[(size_t) oscIndex]; // (the latest sample wins over one still decoding)

    // Step 35: the attacks a random start can land on, found once here
    // (every user sample and multisample region comes in this way).
    if (data != nullptr)
    {
        data->findOnsets();
        for (auto& zone : data->zones)
            if (zone.data != nullptr)
                zone.data->findOnsets();
    }

    const auto embedded = data != nullptr && path.isEmpty() ? data : nullptr;
    // The sample retired three loads ago is freed here, after the lock is
    // let go (the audio thread waits on it in getSampleForOsc).
    std::shared_ptr<SampleData> dropped;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        auto& retired = retiredSamples[(size_t) oscIndex];
        auto& index = retiredIndex[(size_t) oscIndex];
        dropped = std::move (retired[(size_t) index]);
        retired[(size_t) index] = std::move (sampleSlots[(size_t) oscIndex]);
        index = (index + 1) % (int) retired.size();
        sampleSlots[(size_t) oscIndex] = std::move (data);
    }
    retireQueue.retire (std::move (dropped)); // (freed once the audio thread moved past it: B3)

    const juce::SpinLock::ScopedLockType lock (stateLock);
    samplePaths[(size_t) oscIndex] = path;
    embeddedSamples[(size_t) oscIndex] = embedded;
    sfzMissingFiles[(size_t) oscIndex].clear();
    encodedSamples[(size_t) oscIndex] = {};
    // The latest sample wins over a clear or reload still queued.
    pendingSampleClear[(size_t) oscIndex] = false;
    pendingSamplePaths[(size_t) oscIndex].clear();
}

bool IlanaSynthAudioProcessor::isSampleEmbedded (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return embeddedSamples[(size_t) oscIndex] != nullptr;
}
