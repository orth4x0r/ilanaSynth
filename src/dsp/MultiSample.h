#pragma once

// SoundFont 2 (.sf2) and SFZ (.sfz) instruments for the Sample oscillator:
// each is read into a SampleData whose zones hold the regions (key and
// velocity ranges, root key, tuning, loop, level). One instrument per file:
// an SF2's first preset (lowest bank, then program); every region of an SFZ.
// Envelopes, filters and modulators in the files are ignored: the synth's own
// do that job.

#include <juce_audio_formats/juce_audio_formats.h>

#include <cctype>
#include <map>
#include <string>

#include "SamplePlayer.h"

namespace MultiSample
{
inline bool isMultiSampleFile (const juce::File& file)
{
    return file.hasFileExtension ("sf2;sfz");
}

// The region under middle C (else the first) goes in the SampleData's own
// buffer, for the displays.
inline void finish (SampleData& data)
{
    const auto* shown = data.zoneFor (60, 100);
    if (shown != nullptr && shown->data != nullptr)
    {
        data.buffer.makeCopyOf (shown->data->buffer);
        data.sampleRate = shown->data->sampleRate;
    }
}

// ---------------------------------------------------------------- SFZ

// "c4" / "C#3" / "60" -> 60 (middle C is c4, as SFZ has it).
inline int parseNote (const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();
    if (trimmed.isEmpty())
        return -1;
    if (juce::CharacterFunctions::isDigit (trimmed[0]) || trimmed[0] == '-')
        return trimmed.getIntValue();
    static const int semis[] { 9, 11, 0, 2, 4, 5, 7 }; // a..g
    const auto letter = trimmed[0];
    if (letter < 'a' || letter > 'g')
        return -1;
    auto note = semis[letter - 'a'];
    auto rest = trimmed.substring (1);
    if (rest.startsWithChar ('#')) { ++note; rest = rest.substring (1); }
    else if (rest.startsWithChar ('b')) { --note; rest = rest.substring (1); }
    return juce::jlimit (0, 127, note + (rest.getIntValue() + 1) * 12);
}

inline std::shared_ptr<SampleData> loadSfz (const juce::File& file, juce::String& error)
{
    const auto text = file.loadFileAsString();
    if (text.isEmpty())
    {
        error = "empty or unreadable file";
        return nullptr;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::map<juce::String, std::shared_ptr<SampleData>> loaded;
    auto defaultPath = file.getParentDirectory();

    const auto loadAudio = [&] (const juce::String& relative) -> std::shared_ptr<SampleData>
    {
        const auto path = relative.replaceCharacter ('\\', '/');
        const auto audioFile = juce::File::isAbsolutePath (path) ? juce::File (path) : defaultPath.getChildFile (path);
        const auto key = audioFile.getFullPathName();
        if (const auto found = loaded.find (key); found != loaded.end())
            return found->second;
        std::shared_ptr<SampleData> data;
        if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (audioFile) }; reader != nullptr && reader->lengthInSamples > 1)
        {
            const auto length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
            data = std::make_shared<SampleData>();
            data->sampleRate = reader->sampleRate;
            data->name = audioFile.getFileNameWithoutExtension();
            data->buffer.setSize (juce::jlimit (1, 2, (int) reader->numChannels), length);
            if (! reader->read (&data->buffer, 0, length, 0, true, true))
                data.reset();
        }
        loaded[key] = data;
        return data;
    };

    // Strip comments, then walk headers and opcodes. Values may contain
    // spaces (sample paths): a value runs to the next "name=" or header.
    juce::String clean;
    for (auto line : juce::StringArray::fromLines (text))
        clean << line.upToFirstOccurrenceOf ("//", false, false) << "\n";

    using Opcodes = std::map<juce::String, juce::String>;
    Opcodes global, master, group, region;
    juce::String header;
    auto data = std::make_shared<SampleData>();
    data->name = file.getFileNameWithoutExtension();

    const auto flushRegion = [&]
    {
        if (header != "region")
            return;
        Opcodes merged = global;
        for (const auto& [k, v] : master) merged[k] = v;
        for (const auto& [k, v] : group) merged[k] = v;
        for (const auto& [k, v] : region) merged[k] = v;
        const auto get = [&merged] (const char* name, const juce::String& fallback = {})
        {
            const auto found = merged.find (name);
            return found != merged.end() ? found->second : fallback;
        };
        if (get ("sample").isEmpty())
            return;
        SampleZone zone;
        zone.data = loadAudio (get ("sample"));
        if (zone.data == nullptr)
            return;
        if (const auto key = parseNote (get ("key")); key >= 0)
            zone.loKey = zone.hiKey = key, zone.rootNote = key;
        if (const auto lo = parseNote (get ("lokey")); lo >= 0) zone.loKey = lo;
        if (const auto hi = parseNote (get ("hikey")); hi >= 0) zone.hiKey = hi;
        if (const auto centre = parseNote (get ("pitch_keycenter")); centre >= 0) zone.rootNote = centre;
        zone.loVel = juce::jlimit (1, 127, get ("lovel", "1").getIntValue());
        zone.hiVel = juce::jlimit (1, 127, get ("hivel", "127").getIntValue());
        zone.rootNote -= get ("tune", "0").getDoubleValue() / 100.0 + get ("transpose", "0").getDoubleValue();
        zone.gain = juce::Decibels::decibelsToGain (get ("volume", "0").getFloatValue());
        const auto length = zone.data->getNumSamples();
        const auto mode = get ("loop_mode", get ("loopmode"));
        zone.loopStart = juce::jlimit (0, length, get ("loop_start", get ("loopstart", "0")).getIntValue());
        zone.loopEnd = juce::jlimit (0, length, get ("loop_end", get ("loopend", juce::String (length))).getIntValue() + 1);
        zone.loop = (mode == "loop_continuous" || mode == "loop_sustain"
                     || (mode.isEmpty() && merged.count ("loop_start") + merged.count ("loopstart") > 0))
                    && zone.loopEnd - zone.loopStart > 16;
        data->zones.push_back (zone);
    };

    // std::string, so the walk is linear.
    const auto source = clean.toStdString();
    size_t pos = 0;
    const auto isSpace = [] (char c) { return c == ' ' || c == '\t' || c == '\r'; };
    const auto isWord = [] (char c) { return std::isalnum ((unsigned char) c) != 0 || c == '_'; };
    while (pos < source.size())
    {
        while (pos < source.size() && (isSpace (source[pos]) || source[pos] == '\n'))
            ++pos;
        if (pos >= source.size())
            break;
        if (source[pos] == '<')
        {
            flushRegion();
            const auto close = source.find ('>', pos);
            if (close == std::string::npos)
                break;
            header = juce::String (source.substr (pos + 1, close - pos - 1)).trim().toLowerCase();
            pos = close + 1;
            if (header == "region") region.clear();
            else if (header == "group") { group.clear(); }
            else if (header == "master") { master.clear(); group.clear(); }
            else if (header == "global") { global.clear(); master.clear(); group.clear(); }
            continue;
        }
        const auto equals = source.find ('=', pos);
        if (equals == std::string::npos)
            break;
        const auto name = juce::String (source.substr (pos, equals - pos)).trim().toLowerCase();
        // The value ends at the line's end, a header or the next "word=".
        auto end = equals + 1;
        while (end < source.size() && source[end] != '\n' && source[end] != '<')
        {
            if (isSpace (source[end]))
            {
                auto k = end;
                while (k < source.size() && isSpace (source[k]))
                    ++k;
                const auto wordStart = k;
                while (k < source.size() && isWord (source[k]))
                    ++k;
                if (k > wordStart && k < source.size() && source[k] == '=')
                    break;
            }
            ++end;
        }
        const auto value = juce::String (source.substr (equals + 1, end - equals - 1)).trim();
        pos = end;
        if (header == "control" && name == "default_path")
            defaultPath = file.getParentDirectory().getChildFile (value.replaceCharacter ('\\', '/'));
        else if (header == "region") region[name] = value;
        else if (header == "group") group[name] = value;
        else if (header == "master") master[name] = value;
        else if (header == "global") global[name] = value;
    }
    flushRegion();

    if (data->zones.empty())
    {
        error = "no playable regions";
        return nullptr;
    }
    finish (*data);
    return data;
}

// ---------------------------------------------------------------- SF2

namespace Sf2
{
struct Chunk
{
    juce::String id;
    const char* data = nullptr;
    juce::uint32 size = 0;
};

inline juce::uint16 u16 (const char* p) { return (juce::uint16) ((juce::uint8) p[0] | ((juce::uint8) p[1] << 8)); }
inline juce::uint32 u32 (const char* p) { return (juce::uint32) u16 (p) | ((juce::uint32) u16 (p + 2) << 16); }

// The chunks inside a LIST (or the RIFF form), after its 4-byte type.
inline std::vector<Chunk> children (const char* data, juce::uint32 size)
{
    std::vector<Chunk> chunks;
    juce::uint32 offset = 4;
    while (offset + 8 <= size)
    {
        Chunk chunk;
        chunk.id = juce::String (data + offset, 4);
        chunk.size = u32 (data + offset + 4);
        chunk.data = data + offset + 8;
        if (offset + 8 + chunk.size > size)
            break;
        chunks.push_back (chunk);
        offset += 8 + chunk.size + (chunk.size & 1);
    }
    return chunks;
}

inline juce::String listType (const Chunk& chunk) { return chunk.size >= 4 ? juce::String (chunk.data, 4) : juce::String(); }

enum Gen
{
    startAddrsOffset = 0, endAddrsOffset = 1, startloopAddrsOffset = 2, endloopAddrsOffset = 3, startAddrsCoarseOffset = 4,
    endAddrsCoarseOffset = 12, instrument = 41, keyRange = 43, velRange = 44, startloopAddrsCoarseOffset = 45,
    initialAttenuation = 48, endloopAddrsCoarseOffset = 50, coarseTune = 51, fineTune = 52, sampleID = 53, sampleModes = 54,
    overridingRootKey = 58
};

struct Generators
{
    std::map<int, juce::uint16> values;
    bool has (int gen) const { return values.count (gen) > 0; }
    int get (int gen, int fallback = 0) const
    {
        const auto found = values.find (gen);
        return found != values.end() ? (int) (juce::int16) found->second : fallback;
    }
    int lo (int gen, int fallback) const { const auto f = values.find (gen); return f != values.end() ? (f->second & 0xff) : fallback; }
    int hi (int gen, int fallback) const { const auto f = values.find (gen); return f != values.end() ? (f->second >> 8) : fallback; }
};

// The generator lists of the zones bag[first..last).
inline std::vector<Generators> zonesOf (const char* bags, const char* gens, juce::uint32 numGens, int first, int last)
{
    std::vector<Generators> zones;
    for (int b = first; b < last; ++b)
    {
        Generators zone;
        const auto from = u16 (bags + b * 4), to = u16 (bags + (b + 1) * 4);
        for (juce::uint32 g = from; g < to && g < numGens; ++g)
            zone.values[u16 (gens + g * 4)] = u16 (gens + g * 4 + 2);
        zones.push_back (std::move (zone));
    }
    return zones;
}
} // namespace Sf2

inline std::shared_ptr<SampleData> loadSf2 (const juce::File& file, juce::String& error)
{
    using namespace Sf2;
    juce::MemoryBlock block;
    if (! file.loadFileAsData (block) || block.getSize() < 12)
    {
        error = "unreadable file";
        return nullptr;
    }
    const auto* raw = static_cast<const char*> (block.getData());
    if (juce::String (raw, 4) != "RIFF" || juce::String (raw + 8, 4) != "sfbk")
    {
        error = "not a SoundFont";
        return nullptr;
    }

    const char* smpl = nullptr;
    juce::uint32 smplSize = 0;
    std::map<juce::String, Chunk> pdta;
    for (const auto& list : children (raw + 8, (juce::uint32) juce::jmin<size_t> (u32 (raw + 4), block.getSize() - 8)))
    {
        if (list.id != "LIST")
            continue;
        for (const auto& chunk : children (list.data, list.size))
        {
            if (listType (list) == "sdta" && chunk.id == "smpl")
                smpl = chunk.data, smplSize = chunk.size;
            else if (listType (list) == "pdta")
                pdta[chunk.id] = chunk;
        }
    }
    for (const char* id : { "phdr", "pbag", "pgen", "inst", "ibag", "igen", "shdr" })
        if (pdta.count (id) == 0)
        {
            error = juce::String ("missing ") + id;
            return nullptr;
        }
    if (smpl == nullptr)
    {
        error = "no sample data";
        return nullptr;
    }

    const auto& phdr = pdta["phdr"];
    const auto numPresets = (int) (phdr.size / 38) - 1; // the last is the terminator
    if (numPresets < 1)
    {
        error = "no presets";
        return nullptr;
    }
    // The first preset: lowest bank, then lowest program.
    auto chosen = 0;
    for (int p = 1; p < numPresets; ++p)
    {
        const auto* a = phdr.data + p * 38;
        const auto* b = phdr.data + chosen * 38;
        if (u16 (a + 22) < u16 (b + 22) || (u16 (a + 22) == u16 (b + 22) && u16 (a + 20) < u16 (b + 20)))
            chosen = p;
    }
    const auto presetName = juce::String (phdr.data + chosen * 38, 20).trim();
    const auto presetZones = zonesOf (pdta["pbag"].data, pdta["pgen"].data, pdta["pgen"].size / 4,
                                      u16 (phdr.data + chosen * 38 + 24), u16 (phdr.data + (chosen + 1) * 38 + 24));

    const auto& inst = pdta["inst"];
    const auto& shdr = pdta["shdr"];
    const auto numInstruments = (int) (inst.size / 22) - 1;
    const auto numSampleHeaders = (int) (shdr.size / 46) - 1;
    const auto numFrames = smplSize / 2;
    std::map<juce::int64, std::shared_ptr<SampleData>> audio; // by sample header and range

    const auto readSample = [&] (int header, int start, int end) -> std::shared_ptr<SampleData>
    {
        const auto* h = shdr.data + header * 46;
        const auto key = ((juce::int64) header << 42) ^ ((juce::int64) start << 21) ^ (juce::int64) end;
        if (const auto found = audio.find (key); found != audio.end())
            return found->second;
        std::shared_ptr<SampleData> data;
        const auto length = end - start;
        if (start >= 0 && end <= (int) numFrames && length > 1)
        {
            // A left sample linked to a right one plays as a stereo pair.
            const auto type = u16 (h + 44);
            const auto link = (int) u16 (h + 42);
            const auto stereo = (type & 4) != 0 && link < numSampleHeaders;
            const auto partnerStart = stereo ? (int) u32 (shdr.data + link * 46 + 20) : 0;
            data = std::make_shared<SampleData>();
            data->sampleRate = juce::jmax (1000.0, (double) u32 (h + 36));
            data->name = juce::String (h, 20).trim();
            data->buffer.setSize (stereo ? 2 : 1, length);
            for (int channel = 0; channel < data->buffer.getNumChannels(); ++channel)
            {
                const auto offset = channel == 0 ? start : partnerStart + (start - (int) u32 (h + 20));
                auto* out = data->buffer.getWritePointer (channel);
                for (int i = 0; i < length; ++i)
                {
                    const auto frame = offset + i;
                    out[i] = frame >= 0 && frame < (int) numFrames ? (float) (juce::int16) u16 (smpl + frame * 2) / 32768.0f : 0.0f;
                }
            }
        }
        audio[key] = data;
        return data;
    };

    auto data = std::make_shared<SampleData>();
    data->name = presetName.isNotEmpty() ? presetName : file.getFileNameWithoutExtension();

    // The preset's global zone (no instrument) applies to all its zones.
    Generators presetGlobal;
    if (! presetZones.empty() && ! presetZones.front().has (instrument))
        presetGlobal = presetZones.front();

    for (const auto& presetZone : presetZones)
    {
        if (! presetZone.has (instrument))
            continue;
        const auto instrumentIndex = presetZone.get (instrument);
        if (instrumentIndex < 0 || instrumentIndex >= numInstruments)
            continue;
        const auto pLoKey = presetZone.lo (keyRange, presetGlobal.lo (keyRange, 0));
        const auto pHiKey = presetZone.hi (keyRange, presetGlobal.hi (keyRange, 127));
        const auto pLoVel = presetZone.lo (velRange, presetGlobal.lo (velRange, 0));
        const auto pHiVel = presetZone.hi (velRange, presetGlobal.hi (velRange, 127));
        const auto presetTune = presetZone.get (coarseTune, presetGlobal.get (coarseTune)) * 100
                                + presetZone.get (fineTune, presetGlobal.get (fineTune));
        const auto presetAttenuation = presetZone.get (initialAttenuation, presetGlobal.get (initialAttenuation));

        const auto instZones = zonesOf (pdta["ibag"].data, pdta["igen"].data, pdta["igen"].size / 4,
                                        u16 (inst.data + instrumentIndex * 22 + 20), u16 (inst.data + (instrumentIndex + 1) * 22 + 20));
        Generators instGlobal;
        if (! instZones.empty() && ! instZones.front().has (sampleID))
            instGlobal = instZones.front();

        for (const auto& zoneGens : instZones)
        {
            if (! zoneGens.has (sampleID))
                continue;
            const auto get = [&] (int gen, int fallback = 0) { return zoneGens.has (gen) ? zoneGens.get (gen) : instGlobal.get (gen, fallback); };
            const auto header = get (sampleID);
            if (header < 0 || header >= numSampleHeaders)
                continue;
            const auto* h = shdr.data + header * 46;
            const auto type = u16 (h + 44);
            if ((type & 0x8000) != 0) // ROM samples
                continue;
            // The right half of a stereo pair is played with its left one.
            if ((type & 2) != 0)
            {
                const auto link = (int) u16 (h + 42);
                auto hasLeft = false;
                for (const auto& other : instZones)
                    hasLeft = hasLeft || (other.has (sampleID) && other.get (sampleID) == link);
                if (hasLeft)
                    continue;
            }

            const auto start = (int) u32 (h + 20) + get (startAddrsOffset) + get (startAddrsCoarseOffset) * 32768;
            const auto end = (int) u32 (h + 24) + get (endAddrsOffset) + get (endAddrsCoarseOffset) * 32768;
            const auto loopStart = (int) u32 (h + 28) + get (startloopAddrsOffset) + get (startloopAddrsCoarseOffset) * 32768;
            const auto loopEnd = (int) u32 (h + 32) + get (endloopAddrsOffset) + get (endloopAddrsCoarseOffset) * 32768;

            SampleZone zone;
            zone.data = readSample (header, start, end);
            if (zone.data == nullptr)
                continue;
            const auto lo = [&] (int gen, int fallback) { return zoneGens.has (gen) ? zoneGens.lo (gen, fallback) : instGlobal.lo (gen, fallback); };
            const auto hi = [&] (int gen, int fallback) { return zoneGens.has (gen) ? zoneGens.hi (gen, fallback) : instGlobal.hi (gen, fallback); };
            zone.loKey = juce::jmax (pLoKey, lo (keyRange, 0));
            zone.hiKey = juce::jmin (pHiKey, hi (keyRange, 127));
            zone.loVel = juce::jmax (1, juce::jmax (pLoVel, lo (velRange, 0)));
            zone.hiVel = juce::jmin (pHiVel, hi (velRange, 127));
            if (zone.loKey > zone.hiKey || zone.loVel > zone.hiVel)
                continue;
            const auto overriding = get (overridingRootKey, -1);
            const auto originalPitch = (int) (juce::uint8) h[40];
            zone.rootNote = (double) (overriding >= 0 ? overriding : (originalPitch <= 127 ? originalPitch : 60))
                            - (double) (juce::int8) h[41] / 100.0
                            - (double) (get (coarseTune) * 100 + get (fineTune) + presetTune) / 100.0;
            // Attenuation in centibels (SoundFont players scale it by 0.4).
            zone.gain = juce::Decibels::decibelsToGain (-0.04f * (float) (get (initialAttenuation) + presetAttenuation));
            const auto modes = get (sampleModes) & 3;
            zone.loop = (modes == 1 || modes == 3) && loopEnd - loopStart > 16;
            zone.loopStart = juce::jlimit (0, zone.data->getNumSamples(), loopStart - start);
            zone.loopEnd = juce::jlimit (0, zone.data->getNumSamples(), loopEnd - start);
            data->zones.push_back (zone);
        }
    }

    if (data->zones.empty())
    {
        error = "the first preset has no playable zones";
        return nullptr;
    }
    finish (*data);
    return data;
}

inline std::shared_ptr<SampleData> load (const juce::File& file, juce::String& error)
{
    return file.hasFileExtension ("sfz") ? loadSfz (file, error) : loadSf2 (file, error);
}
} // namespace MultiSample
