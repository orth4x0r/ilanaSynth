#include "ProcessorInternal.h"

// Portable sample presets (roadmap step 31): embedding a sample in the patch,
// collecting the files a patch plays into a folder (an SFZ with the audio it
// names), and finding or relinking files that have moved.

namespace
{
// 16-bit bytes an embedded copy of this audio takes in the patch.
juce::int64 embeddedBytes (const SampleData& data)
{
    if (data.zones.empty())
        return (juce::int64) data.getNumSamples() * data.getNumChannels() * 2;
    juce::int64 total = 0;
    for (const auto& zone : data.zones)
        if (zone.data != nullptr)
            total += embeddedBytes (*zone.data);
    return total;
}

// A name in `folder` that nothing there uses yet ("a.wav", "a_2.wav"...).
juce::File uniqueChild (const juce::File& folder, const juce::String& name, const std::set<juce::String>& taken)
{
    const auto stem = juce::File (name).getFileNameWithoutExtension();
    const auto extension = juce::File (name).getFileExtension();
    auto candidate = folder.getChildFile (name);
    for (int n = 2; (candidate.exists() || taken.count (candidate.getFullPathName()) > 0) && n < 10000; ++n)
        candidate = folder.getChildFile (stem + "_" + juce::String (n) + extension);
    return candidate;
}

// Swaps `sample=<from>` for `sample=<to>` in an SFZ's text, whole values only.
juce::String replaceSampleValue (juce::String text, const juce::String& from, const juce::String& to)
{
    for (const auto& spelling : { from, from.replaceCharacter ('/', '\\') })
    {
        const auto needle = "sample=" + spelling;
        juce::String out;
        auto at = 0;
        for (auto hit = text.indexOf (needle); hit >= 0; hit = text.indexOf (at, needle))
        {
            const auto after = hit + needle.length();
            const auto next = after < text.length() ? text[after] : (juce::juce_wchar) '\n';
            out << text.substring (at, hit);
            // A value ends at a space-then-opcode, a line end or a header; a longer name keeps going.
            const auto whole = next == ' ' || next == '\t' || next == '\r' || next == '\n' || next == '<';
            out << (whole ? "sample=" + to : needle);
            at = after;
        }
        out << text.substring (at);
        text = out;
    }
    return text;
}
} // namespace

juce::String IlanaSynthAudioProcessor::getSamplePath (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return {};
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return samplePaths[(size_t) oscIndex];
}

bool IlanaSynthAudioProcessor::isSampleFromFile (int oscIndex) const
{
    return getSamplePath (oscIndex).isNotEmpty();
}

std::vector<IlanaSynthAudioProcessor::MissingSample> IlanaSynthAudioProcessor::getMissingSamples() const
{
    std::vector<MissingSample> out;
    const juce::SpinLock::ScopedLockType lock (stateLock);
    for (int i = 0; i < numSampleOscs; ++i)
    {
        const auto index = (size_t) i;
        if (samplePaths[index].isEmpty() && embeddedSamples[index] == nullptr && missingSamplePaths[index].isNotEmpty())
            out.push_back ({ i, missingSamplePaths[index], {} });
        else if (samplePaths[index].isNotEmpty() && ! sfzMissingFiles[index].isEmpty())
            out.push_back ({ i, samplePaths[index], sfzMissingFiles[index] });
    }
    return out;
}

bool IlanaSynthAudioProcessor::takeMissingSamplesPrompt()
{
    const auto version = missingSamplesVersion.load();
    if (version == missingSamplesPrompted)
        return false;
    missingSamplesPrompted = version;
    return ! getMissingSamples().empty();
}

bool IlanaSynthAudioProcessor::embedSample (int oscIndex, juce::String& error)
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;

    std::shared_ptr<SampleData> data;
    juce::int64 others = 0;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        if (samplePaths[(size_t) oscIndex].isEmpty())
        {
            error = embeddedSamples[(size_t) oscIndex] != nullptr ? "This sample is already stored in the patch." : "No sample file is loaded.";
            return false;
        }
        for (int i = 0; i < numSampleOscs; ++i)
            if (i != oscIndex && embeddedSamples[(size_t) i] != nullptr)
                others += embeddedBytes (*embeddedSamples[(size_t) i]);
    }
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        data = sampleSlots[(size_t) oscIndex];
    }
    if (data == nullptr)
    {
        error = "The sample is not loaded (its file is missing).";
        return false;
    }
    if (const auto size = embeddedBytes (*data); others + size > maxEmbeddedSampleBytes)
    {
        error = "Too big to store in the patch (" + juce::String ((others + size) / (1024 * 1024)) + " MB; the limit is "
                + juce::String (maxEmbeddedSampleBytes / (1024 * 1024)) + " MB). Use Collect Samples instead.";
        return false;
    }

    setUserSample (oscIndex, data, {}); // no path: the patch carries the audio
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        sfzMissingFiles[(size_t) oscIndex].clear();
    }
    ++missingSamplesVersion;
    return true;
}

int IlanaSynthAudioProcessor::embedAllSamples (juce::String& error)
{
    auto count = 0;
    for (int i = 0; i < numSampleOscs; ++i)
    {
        if (! isSampleFromFile (i))
            continue;
        juce::String problem;
        if (embedSample (i, problem))
            ++count;
        else if (error.isEmpty())
            error = "Oscillator " + juce::String (i + 1) + ": " + problem;
    }
    return count;
}

IlanaSynthAudioProcessor::CollectResult IlanaSynthAudioProcessor::collectSamples (const juce::File& folder, const std::function<bool (float)>& progress)
{
    CollectResult result;
    if (! folder.createDirectory().wasOk())
    {
        result.error = "Could not create " + folder.getFullPathName();
        return result;
    }

    std::vector<std::pair<int, juce::String>> jobs;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        for (int i = 0; i < numSampleOscs; ++i)
            if (samplePaths[(size_t) i].isNotEmpty())
                jobs.push_back ({ i, samplePaths[(size_t) i] });
    }

    std::map<juce::String, juce::String> done; // source path -> where it went
    std::set<juce::String> taken;
    const auto report = [&] (size_t job, float within) { return ! progress || progress (((float) job + within) / (float) jobs.size()); };

    for (size_t job = 0; job < jobs.size(); ++job)
    {
        const auto osc = jobs[job].first;
        const juce::File source (jobs[job].second);
        if (! report (job, 0.0f))
        {
            result.cancelled = true;
            return result;
        }
        if (! source.existsAsFile())
        {
            result.missing.add (source.getFullPathName());
            continue;
        }

        juce::String destination;
        if (const auto found = done.find (source.getFullPathName()); found != done.end())
            destination = found->second;
        else
        {
            juce::File target;
            if (source.hasFileExtension ("sfz"))
            {
                // The instrument gets a folder of its own: the SFZ and the audio it names.
                const auto home = uniqueChild (folder, source.getFileNameWithoutExtension(), taken);
                taken.insert (home.getFullPathName());
                home.createDirectory();
                target = home.getChildFile (source.getFileName());

                MultiSample::SfzReport files;
                files.scanOnly = true;
                juce::String ignored;
                MultiSample::loadSfz (source, ignored, &files);

                const auto parent = source.getParentDirectory();
                auto flatten = false;
                for (const auto& entry : files.files)
                {
                    const auto relative = entry.file.getRelativePathFrom (parent);
                    if (! entry.found)
                        continue;
                    if (juce::File::isAbsolutePath (entry.written) || relative.startsWith ("..") || juce::File::isAbsolutePath (relative))
                        flatten = true;
                }

                auto text = source.loadFileAsString();
                std::set<juce::String> usedHere;
                for (size_t n = 0; n < files.files.size(); ++n)
                {
                    const auto& entry = files.files[n];
                    if (! entry.found)
                    {
                        result.missing.add (entry.file.getFullPathName());
                        continue;
                    }
                    auto copy = flatten ? uniqueChild (home.getChildFile ("samples"), entry.file.getFileName(), usedHere)
                                        : home.getChildFile (entry.file.getRelativePathFrom (parent));
                    usedHere.insert (copy.getFullPathName());
                    copy.getParentDirectory().createDirectory();
                    if (! entry.file.copyFileTo (copy))
                    {
                        result.error = "Could not copy " + entry.file.getFullPathName();
                        return result;
                    }
                    ++result.files;
                    if (flatten)
                        text = replaceSampleValue (text, entry.written, "samples/" + copy.getFileName());
                    if (! report (job, (float) (n + 1) / (float) (files.files.size() + 1)))
                    {
                        result.cancelled = true;
                        return result;
                    }
                }
                if (flatten)
                {
                    // default_path would prefix the flattened names.
                    for (auto at = text.indexOf ("default_path="); at >= 0; at = text.indexOf (at + 1, "default_path="))
                    {
                        auto end = at + 13;
                        while (end < text.length() && text[end] != '\n' && text[end] != '\r')
                            ++end;
                        text = text.substring (0, at + 13) + text.substring (end);
                    }
                    if (! target.replaceWithText (text))
                    {
                        result.error = "Could not write " + target.getFullPathName();
                        return result;
                    }
                }
                else if (! source.copyFileTo (target))
                {
                    result.error = "Could not copy " + source.getFullPathName();
                    return result;
                }
                ++result.files;
            }
            else
            {
                target = source.getParentDirectory() == folder ? source : uniqueChild (folder, source.getFileName(), taken);
                if (target != source && ! source.copyFileTo (target))
                {
                    result.error = "Could not copy " + source.getFullPathName();
                    return result;
                }
                taken.insert (target.getFullPathName());
                ++result.files;
            }
            destination = target.getFullPathName();
            done[source.getFullPathName()] = destination;
        }

        // The patch now plays the copy (the audio in memory is the same).
        const juce::SpinLock::ScopedLockType lock (stateLock);
        if (samplePaths[(size_t) osc] == jobs[job].second)
            samplePaths[(size_t) osc] = destination;
        ++result.oscillators;
    }
    ++dataEpoch;
    return result;
}

bool IlanaSynthAudioProcessor::relinkSample (int oscIndex, const juce::File& file)
{
    if (! file.existsAsFile() || ! loadUserSample (oscIndex, file, false))
        return false;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        missingSamplePaths[(size_t) oscIndex].clear();
    }
    ++missingSamplesVersion;
    return true;
}

std::vector<std::pair<int, juce::File>> IlanaSynthAudioProcessor::findMissingSampleFiles (const juce::File& folder, bool recursive,
                                                                                         const std::function<bool()>& shouldStop) const
{
    std::map<juce::String, std::vector<int>> wanted; // lower-case file name -> oscillators
    for (const auto& missing : getMissingSamples())
        if (missing.partial.isEmpty())
            wanted[juce::File (missing.path).getFileName().toLowerCase()].push_back (missing.osc);

    std::vector<std::pair<int, juce::File>> found;
    if (wanted.empty() || ! folder.isDirectory())
        return found;

    constexpr int maxEntries = 400000; // a whole drive is not a search
    auto seen = 0;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, recursive, "*", juce::File::findFiles))
    {
        if ((++seen & 255) == 0 && (seen > maxEntries || (shouldStop && shouldStop())))
            break;
        const auto hit = wanted.find (entry.getFile().getFileName().toLowerCase());
        if (hit == wanted.end())
            continue;
        for (const auto osc : hit->second)
            found.push_back ({ osc, entry.getFile() });
        wanted.erase (hit);
        if (wanted.empty())
            break;
    }
    return found;
}
