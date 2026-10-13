#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

// Roadmap step 42: the sample browser's logic without any component code, so
// the tests can reach it. The lists (favourites, recent files, added folders)
// are kept as portable path strings and saved as one newline-separated string
// each in the editor's settings; the folder listing filters and sorts; the
// preview gain levels every audition to the same loudness.
namespace SampleBrowserModel
{
constexpr int maxRecent = 30;
constexpr double previewSeconds = 10.0;
constexpr float previewTargetDb = -18.0f; // RMS of every preview, in dBFS
constexpr float previewMaxBoostDb = 36.0f;

// ---------------------------------------------------------------- paths

// A path in the lists: under the user's home folder it is "~/relative/path"
// (forward slashes), so the lists survive a moved account or a synced settings
// file; any other path is kept as it is.
inline juce::String toPortablePath (const juce::File& file)
{
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    if (home != juce::File() && file.isAChildOf (home))
        return "~/" + file.getRelativePathFrom (home).replaceCharacter ('\\', '/');

    return file.getFullPathName();
}

inline juce::File fromPortablePath (const juce::String& path)
{
    if (path.startsWith ("~/"))
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (path.substring (2));

    return juce::File (path);
}

// ---------------------------------------------------------------- lists

inline juce::String serialiseList (const juce::StringArray& list)
{
    return list.joinIntoString ("\n");
}

// Empty lines and repeats are dropped.
inline juce::StringArray parseList (const juce::String& text)
{
    juce::StringArray list;

    for (const auto& line : juce::StringArray::fromLines (text))
        if (line.trim().isNotEmpty())
            list.addIfNotAlreadyThere (line.trim());

    return list;
}

// Adds or removes the path; true when it is in the list afterwards.
inline bool toggleEntry (juce::StringArray& list, const juce::String& path)
{
    if (list.contains (path))
    {
        list.removeString (path);
        return false;
    }

    list.add (path);
    return true;
}

// Most recent first, no repeats, at most maxRecent.
inline void addRecent (juce::StringArray& list, const juce::String& path)
{
    list.removeString (path);
    list.insert (0, path);

    while (list.size() > maxRecent)
        list.remove (list.size() - 1);
}

// Entries whose file is gone leave the list; returns whether any did.
inline bool dropMissing (juce::StringArray& list)
{
    auto changed = false;

    for (int i = list.size(); --i >= 0;)
        if (! fromPortablePath (list[i]).exists())
        {
            list.remove (i);
            changed = true;
        }

    return changed;
}

// ---------------------------------------------------------------- files

inline bool isAudioName (const juce::String& name)
{
    static const char* extensions[] { ".wav", ".aif", ".aiff", ".flac", ".ogg", ".mp3", ".sf2", ".sfz" };
    const auto lower = name.toLowerCase();

    for (const auto* extension : extensions)
        if (lower.endsWith (extension))
            return true;

    return false;
}

// SoundFont and SFZ instruments load but have no single audio to audition.
inline bool isPreviewable (const juce::File& file)
{
    return isAudioName (file.getFileName()) && ! file.hasFileExtension ("sf2;sfz");
}

struct Entry
{
    juce::File file;
    bool isFolder = false;
};

// The folder's subfolders first, then its audio files, each group by name
// (numbers in order); hidden items are left out. The filter keeps the entries
// whose name has every word of it (case does not matter).
inline std::vector<Entry> listFolder (const juce::File& folder, const juce::String& filter = {})
{
    std::vector<Entry> entries;
    const auto words = juce::StringArray::fromTokens (filter.trim().toLowerCase(), " ", "");

    for (const auto& item : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto file = item.getFile();

        if (file.isHidden() || file.getFileName().startsWithChar ('.'))
            continue;

        const auto isFolder = item.isDirectory();

        if (! isFolder && ! isAudioName (file.getFileName()))
            continue;

        const auto name = file.getFileName().toLowerCase();
        auto matches = true;

        for (const auto& word : words)
            matches = matches && name.contains (word);

        if (matches)
            entries.push_back ({ file, isFolder });
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
    {
        if (a.isFolder != b.isFolder)
            return a.isFolder;

        return a.file.getFileName().compareNatural (b.file.getFileName()) < 0;
    });

    return entries;
}

// ---------------------------------------------------------------- level

// The RMS over every channel and the largest absolute sample.
struct Level
{
    float rms = 0.0f, peak = 0.0f;
};

inline Level measureLevel (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    auto peak = 0.0f;
    const auto count = (double) buffer.getNumSamples() * (double) buffer.getNumChannels();

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        const auto* data = buffer.getReadPointer (c);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += (double) data[i] * (double) data[i];
            peak = std::max (peak, std::abs (data[i]));
        }
    }

    return { count > 0.0 ? (float) std::sqrt (sum / count) : 0.0f, peak };
}

// The gain that brings a preview to previewTargetDb RMS: at most
// previewMaxBoostDb of boost, and never more than keeps the peak under full
// scale (a spiky sound then plays a little quieter than the target). Silence
// gets 1.
inline float previewGain (float rms, float peak)
{
    if (! (rms > 1.0e-6f))
        return 1.0f;

    auto gain = juce::Decibels::decibelsToGain (previewTargetDb) / rms;
    gain = std::min (gain, juce::Decibels::decibelsToGain (previewMaxBoostDb));

    if (peak > 1.0e-6f)
        gain = std::min (gain, 0.98f / peak);

    return gain;
}

// The level note under the browser: what the preview did to the file.
inline juce::String levelNote (float gain)
{
    const auto db = juce::Decibels::gainToDecibels (gain, -100.0f);

    if (std::abs (db) < 0.05f)
        return "Preview at its own level";

    return juce::String ("Preview level matched to ") + juce::String ((int) previewTargetDb) + " dB ("
           + (db > 0.0f ? "+" : "-") + juce::String (std::abs (db), 1) + " dB)";
}
} // namespace SampleBrowserModel
