#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "SampleOnsets.h"
#include "SamplePlayer.h"

// Roadmap step 32: slice mode. A plain sample is cut by its transients, into
// equal parts or at hand-placed markers; each slice becomes a zone on its own
// key (from the base key up), so the zone map edits its key, root (tuning),
// level and direction, and every playback setting applies per slice.
namespace SampleSlicer
{
enum Mode { off = 0, transients = 1, equal = 2, markers = 3 };

constexpr int minSliceFrames = 64;

struct Settings
{
    int mode = off;
    int count = 8;              // equal parts
    float sensitivity = 0.5f;   // transients: 0 fewer .. 1 more
    int baseKey = 36;           // the first slice's key (C1)
    std::vector<float> markers; // markers mode: the cuts, fractions of the sample
};

// The cuts (frames, rising, from 0 to the length; at least two slices'
// worth of room each). Never more slices than keys from baseKey up.
inline std::vector<int> boundaries (const SampleData& whole, const Settings& settings)
{
    const auto length = whole.getNumSamples();
    std::vector<int> cuts;
    if (length < minSliceFrames * 2)
        return { 0, juce::jmax (0, length) };

    if (settings.mode == transients)
    {
        const float* channels[2] { whole.buffer.getReadPointer (0), whole.buffer.getReadPointer (juce::jmin (1, whole.getNumChannels() - 1)) };
        cuts = SampleOnsets::find (channels, juce::jmin (2, whole.getNumChannels()), length, whole.sampleRate,
                                   juce::jlimit (0.0f, 1.0f, settings.sensitivity));
    }
    else if (settings.mode == equal)
    {
        const auto parts = juce::jlimit (1, 128, settings.count);
        for (int i = 1; i < parts; ++i)
            cuts.push_back ((int) ((juce::int64) length * i / parts));
    }
    else if (settings.mode == markers)
    {
        for (const auto marker : settings.markers)
            cuts.push_back (juce::roundToInt (juce::jlimit (0.0f, 1.0f, marker) * (float) length));
    }

    cuts.push_back (0);
    std::sort (cuts.begin(), cuts.end());
    std::vector<int> result;
    for (const auto cut : cuts)
        if (cut >= 0 && cut < length - minSliceFrames && (result.empty() || cut - result.back() >= minSliceFrames))
            result.push_back (cut);
    if (result.empty() || result.front() != 0)
        result.insert (result.begin(), 0);
    const auto maxSlices = juce::jmax (1, 128 - juce::jlimit (0, 127, settings.baseKey));
    if ((int) result.size() > maxSlices)
        result.resize ((size_t) maxSlices);
    result.push_back (length);
    return result;
}

// The whole sample cut into slices: a zone per slice on consecutive keys,
// each rooted on its own key (so TUNED plays a slice at its pitch there).
// Settings off: the plain sample back.
inline std::shared_ptr<SampleData> slice (const SampleData& whole, const Settings& settings)
{
    auto data = std::make_shared<SampleData>();
    data->buffer = whole.buffer;
    data->sampleRate = whole.sampleRate;
    data->name = whole.name;
    data->loadWarning = whole.loadWarning;
    if (settings.mode == off || whole.getNumSamples() < minSliceFrames * 2)
    {
        data->findOnsets();
        return data;
    }

    data->sliceMode = settings.mode;
    data->sliceCount = juce::jlimit (1, 128, settings.count);
    data->sliceSensitivity = juce::jlimit (0.0f, 1.0f, settings.sensitivity);
    data->sliceBaseKey = juce::jlimit (0, 127, settings.baseKey);
    data->sliceMarkers = settings.markers;
    data->sliceStarts = boundaries (whole, settings);

    for (size_t i = 0; i + 1 < data->sliceStarts.size(); ++i)
    {
        const auto from = data->sliceStarts[i];
        const auto length = data->sliceStarts[i + 1] - from;
        auto part = std::make_shared<SampleData>();
        part->sampleRate = whole.sampleRate;
        part->name = whole.name + " " + juce::String ((int) i + 1);
        part->buffer.setSize (whole.getNumChannels(), length);
        for (int c = 0; c < whole.getNumChannels(); ++c)
            part->buffer.copyFrom (c, 0, whole.buffer, c, from, length);
        part->findOnsets();

        SampleZone zone;
        zone.data = std::move (part);
        zone.loKey = zone.hiKey = data->sliceBaseKey + (int) i;
        zone.rootNote = (double) zone.loKey;
        zone.origin = (int) i;
        data->zones.push_back (std::move (zone));
    }
    return data;
}

inline Settings settingsOf (const SampleData& data)
{
    Settings settings;
    settings.mode = data.sliceMode;
    settings.count = data.sliceCount;
    settings.sensitivity = data.sliceSensitivity;
    settings.baseKey = data.sliceBaseKey;
    settings.markers = data.sliceMarkers;
    return settings;
}

// The whole sample a sliced one was cut from (or the plain one itself).
inline std::shared_ptr<SampleData> wholeOf (const SampleData& data)
{
    auto whole = std::make_shared<SampleData>();
    whole->buffer = data.buffer;
    whole->sampleRate = data.sampleRate;
    whole->name = data.name;
    whole->loadWarning = data.loadWarning;
    return whole;
}

// Saved with the patch (in a file's options, or with embedded audio).
inline void write (juce::ValueTree& tree, const Settings& settings)
{
    tree.setProperty ("sliceMode", settings.mode, nullptr);
    tree.setProperty ("sliceCount", settings.count, nullptr);
    tree.setProperty ("sliceSensitivity", (double) settings.sensitivity, nullptr);
    tree.setProperty ("sliceBaseKey", settings.baseKey, nullptr);
    juce::StringArray markers;
    for (const auto marker : settings.markers)
        markers.add (juce::String (marker, 6));
    tree.setProperty ("sliceMarkers", markers.joinIntoString (" "), nullptr);
}

inline Settings read (const juce::ValueTree& tree)
{
    Settings settings;
    settings.mode = juce::jlimit (0, 3, (int) tree.getProperty ("sliceMode", 0));
    settings.count = juce::jlimit (1, 128, (int) tree.getProperty ("sliceCount", 8));
    settings.sensitivity = juce::jlimit (0.0f, 1.0f, (float) (double) tree.getProperty ("sliceSensitivity", 0.5));
    settings.baseKey = juce::jlimit (0, 127, (int) tree.getProperty ("sliceBaseKey", 36));
    for (const auto& marker : juce::StringArray::fromTokens (tree.getProperty ("sliceMarkers").toString(), " ", {}))
        if (marker.isNotEmpty())
            settings.markers.push_back (juce::jlimit (0.0f, 1.0f, marker.getFloatValue()));
    return settings;
}
} // namespace SampleSlicer
