#include "BuiltInTables.h"

#include <WavetableData.h>

#include <cmath>
#include <cstring>

namespace
{
struct Entry
{
    juce::String path;
    int frameSize = 0, frames = 0;
    size_t offset = 0, size = 0;
};

const std::vector<Entry>& entries()
{
    static const auto table = []
    {
        std::vector<Entry> result;
        const auto* data = reinterpret_cast<const unsigned char*> (WavetableData::builtinwavetables_bin);
        const auto length = (size_t) WavetableData::builtinwavetables_binSize;

        const auto u16 = [data] (size_t at) { return (int) (data[at] | (data[at + 1] << 8)); };
        const auto u32 = [data] (size_t at)
        {
            return (size_t) data[at] | ((size_t) data[at + 1] << 8) | ((size_t) data[at + 2] << 16) | ((size_t) data[at + 3] << 24);
        };

        if (length < 12 || std::memcmp (data, "ILBT", 4) != 0 || u32 (4) != 1)
            return result;

        auto at = (size_t) 12;
        for (auto i = u32 (8); i > 0 && at + 2 <= length; --i)
        {
            const auto pathLength = (size_t) u16 (at);
            if (at + 2 + pathLength + 12 > length)
                break;

            Entry entry;
            entry.path = juce::String::fromUTF8 (reinterpret_cast<const char*> (data + at + 2), (int) pathLength);
            at += 2 + pathLength;
            entry.frameSize = u16 (at);
            entry.frames = u16 (at + 2);
            entry.offset = u32 (at + 4);
            entry.size = u32 (at + 8);
            at += 12;

            if (entry.frameSize > 0 && entry.frames > 0 && entry.offset + entry.size <= length)
                result.push_back (std::move (entry));
        }

        return result;
    }();

    return table;
}

// The table's samples (frames back to back, as float) at their own frame size.
bool decode (const Entry& entry, std::vector<float>& samples)
{
    const auto* data = reinterpret_cast<const unsigned char*> (WavetableData::builtinwavetables_bin);
    const auto count = (size_t) entry.frameSize * (size_t) entry.frames;
    juce::MemoryInputStream source (data + entry.offset, entry.size, false);
    juce::GZIPDecompressorInputStream unzip (&source, false, juce::GZIPDecompressorInputStream::zlibFormat,
                                             (juce::int64) (count * sizeof (juce::int16)));
    std::vector<juce::int16> delta (count);

    if (unzip.read (delta.data(), (int) (count * sizeof (juce::int16))) != (int) (count * sizeof (juce::int16)))
        return false;

    samples.resize (count);
    auto previous = 0;
    for (size_t i = 0; i < count; ++i)
    {
        previous = (int) (juce::int16) (previous + delta[i]);
        samples[i] = (float) previous / 32768.0f;
    }

    return true;
}

// One frame stretched to the synth's frame size, the way Wavetable::readFrames does for a file.
std::vector<float> stretchFrame (const float* source, int frameLength)
{
    std::vector<float> frame ((size_t) Wavetable::frameSize);

    for (int i = 0; i < Wavetable::frameSize; ++i)
    {
        const auto position = (double) i * (double) frameLength / (double) Wavetable::frameSize;
        const auto index = juce::jlimit (0, frameLength - 1, (int) position);
        const auto next = juce::jmin (index + 1, frameLength - 1);
        const auto frac = (float) (position - (double) index);

        frame[(size_t) i] = source[index] + frac * (source[next] - source[index]);
    }

    return frame;
}
} // namespace

int BuiltInTables::count()
{
    return (int) entries().size();
}

juce::String BuiltInTables::path (int index)
{
    return index >= 0 && index < count() ? entries()[(size_t) index].path : juce::String();
}

juce::String BuiltInTables::name (int index)
{
    return path (index).fromLastOccurrenceOf ("/", false, false);
}

juce::String BuiltInTables::group (int index)
{
    const auto full = path (index);
    return full.contains ("/") ? full.upToLastOccurrenceOf ("/", false, false).replace ("/", " / ") : juce::String();
}

bool BuiltInTables::load (int index, WavetableDoc& doc)
{
    std::vector<float> samples;
    if (index < 0 || index >= count() || ! decode (entries()[(size_t) index], samples))
        return false;

    const auto& entry = entries()[(size_t) index];
    std::vector<std::vector<float>> frames;
    frames.reserve ((size_t) entry.frames);

    for (int frame = 0; frame < entry.frames; ++frame)
        frames.push_back (stretchFrame (samples.data() + (size_t) frame * (size_t) entry.frameSize, entry.frameSize));

    doc = WavetableDoc::fromFrames (name (index), std::move (frames));
    return true;
}

const BuiltInTables::Preview& BuiltInTables::preview (int index)
{
    // Only the browser (the message thread) asks.
    static std::vector<std::unique_ptr<Preview>> cache;
    static const Preview empty;

    if (index < 0 || index >= count())
        return empty;

    cache.resize ((size_t) count());
    auto& slot = cache[(size_t) index];

    if (slot == nullptr)
    {
        slot = std::make_unique<Preview>();
        std::vector<float> samples;
        const auto& entry = entries()[(size_t) index];

        if (decode (entry, samples))
        {
            // A table plays normalised to its loudest sample, so it is drawn that way.
            auto peak = 0.0f;
            for (const auto sample : samples)
                peak = juce::jmax (peak, std::abs (sample));
            const auto gain = peak > 1.0e-6f ? 1.0f / peak : 1.0f;

            const auto picture = [&] (int frame)
            {
                const auto stretched = stretchFrame (samples.data() + (size_t) frame * (size_t) entry.frameSize, entry.frameSize);
                std::vector<float> points (97);
                for (int i = 0; i <= 96; ++i)
                    points[(size_t) i] = gain * stretched[(size_t) juce::jmin (Wavetable::frameSize - 1, i * Wavetable::frameSize / 96)];
                return points;
            };

            // The loudest frame stands for a table that starts quiet.
            auto loudest = 0;
            auto loudestEnergy = -1.0;
            for (int frame = 0; frame < entry.frames; ++frame)
            {
                auto energy = 0.0;
                for (int i = 0; i < entry.frameSize; ++i)
                {
                    const auto value = (double) samples[(size_t) frame * (size_t) entry.frameSize + (size_t) i];
                    energy += value * value;
                }
                if (energy > loudestEnergy)
                {
                    loudestEnergy = energy;
                    loudest = frame;
                }
            }

            slot->first = picture (0);
            slot->middle = picture (loudest);
        }
    }

    return *slot;
}
