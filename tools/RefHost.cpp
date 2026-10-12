// Dev tool: renders reference notes from an installed VST3 instrument's
// default (init) sound, for fitting modelled sounds (not shipped).
//
// usage: ilanaRefHost <plugin.vst3> <output folder> <name> [note:velocity ...]
//        ilanaRefHost --presets <plugin.vst3> <preset folder> <output folder>
//        ilanaRefHost --train <ilanaSynth.vst3> <preset folder>...
// The second form loads each .vital preset and renders the preset critic's
// test phrases (see tools/preset_critic.py), as reference renders.
// The third plays every .ilanapreset in the folders (and the init sound)
// through ilanaSynth, writing nothing: the training run of a profile-guided
// build (tools/pgo-windows.ps1).
// Each note is held for 4 s and released for 1 s at 48 kHz, written as
// <name>.v<velocity>.n<note>.wav (mono, 24-bit). The default notes are
// 40, 60 and 84 at velocities 40 and 120.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
#include <map>
#include <iostream>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480;

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
    {
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                    format.createWriterFor (stream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
    }

    // Runs the plugin with no input for a while, so it finishes loading
    // samples or settling after a note.
    void idle (juce::AudioPluginInstance& plugin, int blocks, bool sleep = true)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, plugin.getTotalNumOutputChannels()), blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            plugin.processBlock (buffer, midi);
            if (sleep)
                juce::Thread::sleep (2);
        }
    }
}

namespace presets
{
    struct NoteEvent
    {
        double on, off;
        int note, velocity;
    };

    // The preset critic's test phrases (tools/PresetRender.cpp), played on
    // the plugin in whatever state it is in; stereo, 48 kHz.
    juce::AudioBuffer<float> play (juce::AudioPluginInstance& plugin, const std::vector<NoteEvent>& events, double seconds)
    {
        const int channels = juce::jmax (2, plugin.getTotalNumOutputChannels());
        const auto totalBlocks = (int) std::ceil (seconds * sampleRate / blockSize);
        juce::AudioBuffer<float> out (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> buffer (channels, blockSize);
        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            const auto start = block * blockSize;
            for (const auto& event : events)
            {
                const auto on = (int) std::llround (event.on * sampleRate);
                const auto off = (int) std::llround (event.off * sampleRate);
                if (on >= start && on < start + blockSize)
                    midi.addEvent (juce::MidiMessage::noteOn (1, event.note, (juce::uint8) event.velocity), on - start);
                if (off >= start && off < start + blockSize)
                    midi.addEvent (juce::MidiMessage::noteOff (1, event.note), off - start);
            }
            plugin.processBlock (buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                out.copyFrom (channel, start, buffer, channel, 0, blockSize);
        }
        // Let tails die away before the next phrase (no need to wait in real time).
        idle (plugin, (int) (3.0 * sampleRate / blockSize), false);
        return out;
    }

    juce::AudioProcessorParameter* findParameter (juce::AudioPluginInstance& plugin, const juce::String& name)
    {
        for (auto* parameter : plugin.getParameters())
            if (parameter->getName (64).equalsIgnoreCase (name))
                return parameter;
        return nullptr;
    }

    // Vital's preset style to the critic's categories (Vital has no Pluck;
    // its Sequence presets are mostly short, plucked patterns).
    juce::String categoryFor (const juce::String& style)
    {
        if (style == "Sequence")
            return "Pluck";
        return style;
    }

    // Surge's patch folders (Basses, Leads, ...) to the critic's categories.
    juce::String categoryForFolder (const juce::String& folder)
    {
        static const std::pair<const char*, const char*> map[] { { "Basses", "Bass" }, { "Leads", "Lead" }, { "Pads", "Pad" },
                                                                 { "Keys", "Keys" }, { "Plucks", "Pluck" } };
        for (const auto& [from, to] : map)
            if (folder == from)
                return to;
        return {};
    }

    // Surge's hosted state is the .fxp's patch chunk (from byte 60, 'sub3').
    bool loadSurgePatch (juce::AudioPluginInstance& plugin, const juce::File& file)
    {
        juce::MemoryBlock fxp;
        if (! file.loadFileAsData (fxp) || fxp.getSize() <= 64 || std::memcmp (static_cast<const char*> (fxp.getData()) + 60, "sub3", 4) != 0)
            return false;
        juce::MemoryBlock hostState;
        plugin.getStateInformation (hostState);
        auto xml = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
        auto* component = xml != nullptr ? xml->getChildByName ("IComponent") : nullptr;
        if (component == nullptr)
            return false;
        const juce::MemoryBlock chunk (static_cast<const char*> (fxp.getData()) + 60, fxp.getSize() - 60);
        component->deleteAllTextElements();
        component->addTextElement (chunk.toBase64Encoding());
        juce::MemoryBlock newState;
        juce::AudioProcessor::copyXmlToBinary (*xml, newState);
        plugin.setStateInformation (newState.getData(), (int) newState.getSize());
        return true;
    }

    void writeBigEndian (juce::MemoryBlock& block, size_t offset, std::uint32_t value)
    {
        auto* bytes = static_cast<std::uint8_t*> (block.getData()) + offset;
        bytes[0] = (std::uint8_t) (value >> 24);
        bytes[1] = (std::uint8_t) (value >> 16);
        bytes[2] = (std::uint8_t) (value >> 8);
        bytes[3] = (std::uint8_t) value;
    }

    // JUCE's VST3 host keeps a plugin's state as XML with the component
    // state in base64. Vital's is a VST2-style chunk: a 16-byte 'VstW'
    // header, then an opaque 'CcnK'/'FBCh' bank (size at byte 20, data size
    // at 172, data from 176) whose data is the preset's JSON. The preset is
    // put in that frame, keeping the plugin's own header.
    bool loadVitalPreset (juce::AudioPluginInstance& plugin, const juce::String& json)
    {
        juce::MemoryBlock hostState;
        plugin.getStateInformation (hostState);
        auto xml = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
        auto* component = xml != nullptr ? xml->getChildByName ("IComponent") : nullptr;
        if (component == nullptr)
            return false;

        juce::MemoryBlock current;
        current.fromBase64Encoding (component->getAllSubText());
        constexpr size_t dataStart = 176;
        if (current.getSize() < dataStart || std::memcmp (current.getData(), "VstW", 4) != 0)
            return false;

        const auto* oldData = static_cast<const char*> (current.getData()) + dataStart;
        const auto oldSize = current.getSize() - dataStart;
        const auto trailingNull = oldSize > 0 && oldData[oldSize - 1] == 0;

        juce::MemoryBlock chunk (current.getData(), dataStart);
        chunk.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
        if (trailingNull)
            chunk.append ("\0", 1);
        writeBigEndian (chunk, 20, (std::uint32_t) (chunk.getSize() - 24));
        writeBigEndian (chunk, 172, (std::uint32_t) (chunk.getSize() - dataStart));

        component->deleteAllTextElements();
        component->addTextElement (chunk.toBase64Encoding());
        juce::MemoryBlock newState;
        juce::AudioProcessor::copyXmlToBinary (*xml, newState);
        plugin.setStateInformation (newState.getData(), (int) newState.getSize());
        return true;
    }

    // ilanaRefHost --presets <plugin.vst3> <preset folder> <out folder>
    // Loads every .vital file (the plugin's state is the preset's JSON) and
    // renders the critic's phrases into <out>/<n>/, with an index.csv.
    int run (juce::AudioPluginInstance& plugin, const juce::File& presetFolder, const juce::File& folder)
    {
        folder.createDirectory();
        std::ofstream index (folder.getChildFile ("index.csv").getFullPathName().toStdString());
        index << "index,name,category,home_note,macro1,macro2,macro3,macro4\n";
        auto files = presetFolder.findChildFiles (juce::File::findFiles, true, "*.vital;*.fxp");
        files.sort();

        // Each file's category; at most ILANA_REF_LIMIT (40) per category,
        // spread evenly through the sorted list.
        std::vector<std::pair<juce::File, juce::String>> chosen;
        {
            std::map<juce::String, std::vector<juce::File>> byCategory;
            for (const auto& file : files)
            {
                const auto category = file.hasFileExtension ("vital")
                                          ? categoryFor (juce::JSON::parse (file.loadFileAsString()).getProperty ("preset_style", "").toString())
                                          : categoryForFolder (file.getParentDirectory().getFileName());
                if (juce::StringArray { "Bass", "Pad", "Lead", "Keys", "Pluck" }.contains (category))
                    byCategory[category].push_back (file);
            }
            const auto limit = juce::SystemStats::getEnvironmentVariable ("ILANA_REF_LIMIT", "40").getIntValue();
            for (auto& [category, list] : byCategory)
            {
                const auto take = juce::jmin ((int) list.size(), limit);
                for (int i = 0; i < take; ++i)
                    chosen.push_back ({ list[(size_t) (i * (int) list.size() / take)], category });
            }
        }

        int count = 0;
        for (const auto& [file, category] : chosen)
        {
            const auto loaded = file.hasFileExtension ("vital") ? loadVitalPreset (plugin, file.loadFileAsString())
                                                                : loadSurgePatch (plugin, file);
            if (! loaded)
            {
                std::cout << "could not load " << file.getFileName() << std::endl;
                continue;
            }
            idle (plugin, 100);

            const auto home = category == "Bass" ? 36 : 60;
            const auto out = folder.getChildFile (juce::String (count));
            out.createDirectory();
            writeWav (out.getChildFile ("note.wav"), play (plugin, { { 0.0, 2.0, home, 100 } }, 3.5));
            writeWav (out.getChildFile ("chord.wav"),
                      play (plugin, { { 0.0, 2.0, home, 90 }, { 0.0, 2.0, home + 4, 90 }, { 0.0, 2.0, home + 7, 90 }, { 0.0, 2.0, home + 12, 90 } }, 3.5));
            std::vector<NoteEvent> line;
            const int steps[] { 0, 3, 5, 7, 10, 7, 5, 3 };
            for (int i = 0; i < 8; ++i)
                line.push_back ({ i * 0.25, i * 0.25 + 0.2, home + steps[i], 100 });
            writeWav (out.getChildFile ("line.wav"), play (plugin, line, 3.5));
            std::vector<NoteEvent> velocities;
            const int levels[] { 20, 60, 100, 127 };
            for (int i = 0; i < 4; ++i)
                velocities.push_back ({ i * 1.0, i * 1.0 + 0.6, home, levels[i] });
            writeWav (out.getChildFile ("vel.wav"), play (plugin, velocities, 4.5));

            for (int macro = 1; macro <= 4; ++macro)
            {
                auto* parameter = findParameter (plugin, "Macro " + juce::String (macro));
                const auto original = parameter != nullptr ? parameter->getValue() : 0.0f;
                for (const auto high : { false, true })
                {
                    if (parameter != nullptr)
                        parameter->setValueNotifyingHost (high ? 1.0f : 0.0f);
                    idle (plugin, 20);
                    writeWav (out.getChildFile ("macro" + juce::String (macro) + (high ? "_hi.wav" : "_lo.wav")),
                              play (plugin, { { 0.0, 1.5, home, 100 } }, 2.0));
                }
                if (parameter != nullptr)
                    parameter->setValueNotifyingHost (original);
            }

            index << count << ",\"" << file.getFileNameWithoutExtension().replace ("\"", "'").toStdString() << "\",\""
                  << category.toStdString() << "\"," << home << ",,,,\n";
            index.flush();
            std::cout << count << " " << category << " " << file.getFileNameWithoutExtension() << std::endl;
            ++count;
        }
        std::cout << "rendered " << count << " presets" << std::endl;
        return 0;
    }
}

namespace train
{
    // JUCE's VST3 host wraps the plugin's own state (for ilanaSynth, the
    // preset's XML in JUCE's binary form) as base64 in IComponent.
    bool loadIlanaPreset (juce::AudioPluginInstance& plugin, const juce::File& file)
    {
        auto preset = juce::XmlDocument::parse (file);
        juce::MemoryBlock hostState;
        plugin.getStateInformation (hostState);
        auto xml = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), (int) hostState.getSize());
        auto* component = xml != nullptr ? xml->getChildByName ("IComponent") : nullptr;
        if (preset == nullptr || component == nullptr)
            return false;
        juce::MemoryBlock chunk;
        juce::AudioProcessor::copyXmlToBinary (*preset, chunk);
        component->deleteAllTextElements();
        component->addTextElement (chunk.toBase64Encoding());
        juce::MemoryBlock newState;
        juce::AudioProcessor::copyXmlToBinary (*xml, newState);
        plugin.setStateInformation (newState.getData(), (int) newState.getSize());
        return true;
    }

    // What playing looks like: a pedalled run, a held chord, a few fast
    // repeats, then the release. Returns the peak level.
    float play (juce::AudioPluginInstance& plugin)
    {
        auto peak = 0.0f;
        std::vector<presets::NoteEvent> events;
        for (int i = 0; i < 16; ++i)
            events.push_back ({ i * 0.12, i * 0.12 + 0.1, 48 + (i * 5) % 24, 50 + (i * 17) % 70 });
        for (const auto note : { 48, 55, 60, 64, 67, 72 })
            events.push_back ({ 2.2, 4.2, note, 90 });
        for (int i = 0; i < 6; ++i)
            events.push_back ({ 4.5 + i * 0.08, 4.5 + i * 0.08 + 0.05, 62, 110 });
        const int channels = juce::jmax (2, plugin.getTotalNumOutputChannels());
        juce::AudioBuffer<float> buffer (channels, blockSize);
        const auto totalBlocks = (int) (7.0 * sampleRate / blockSize);
        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            const auto start = block * blockSize;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (start <= (int) (2.0 * sampleRate) && (int) (2.0 * sampleRate) < start + blockSize)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), (int) (2.0 * sampleRate) - start);
            for (const auto& event : events)
            {
                const auto on = (int) std::llround (event.on * sampleRate);
                const auto off = (int) std::llround (event.off * sampleRate);
                if (on >= start && on < start + blockSize)
                    midi.addEvent (juce::MidiMessage::noteOn (1, event.note, (juce::uint8) event.velocity), on - start);
                if (off >= start && off < start + blockSize)
                    midi.addEvent (juce::MidiMessage::noteOff (1, event.note), off - start);
            }
            plugin.processBlock (buffer, midi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, blockSize));
        }
        idle (plugin, (int) (2.0 * sampleRate / blockSize), false);
        return peak;
    }

    int run (juce::AudioPluginInstance& plugin, const juce::StringArray& folders)
    {
        std::cout << "init sound: peak " << play (plugin) << std::endl;
        int count = 0;
        for (const auto& folder : folders)
            for (const auto& file : juce::File (folder).findChildFiles (juce::File::findFiles, true, "*.ilanapreset"))
            {
                if (! loadIlanaPreset (plugin, file))
                {
                    std::cout << "could not load " << file.getFileName() << std::endl;
                    continue;
                }
                idle (plugin, 20, false);
                const auto peak = play (plugin);
                std::cout << "played " << file.getFileNameWithoutExtension() << ": peak " << peak << std::endl;
                ++count;
            }
        std::cout << "trained on " << count << " presets and the init sound" << std::endl;
        return 0;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto presetMode = argc >= 5 && juce::String (argv[1]) == "--presets";
    const auto trainMode = argc >= 3 && juce::String (argv[1]) == "--train";
    if (presetMode || trainMode)
    {
        juce::VST3PluginFormat vst3;
        juce::OwnedArray<juce::PluginDescription> types;
        vst3.findAllTypesForFile (types, juce::String (argv[2]));
        juce::AudioPluginFormatManager formats;
        formats.addFormat (new juce::VST3PluginFormat());
        juce::String error;
        auto plugin = types.isEmpty() ? nullptr : formats.createPluginInstance (*types[0], sampleRate, blockSize, error);
        if (plugin == nullptr)
        {
            std::cout << "could not load " << argv[2] << ": " << error << std::endl;
            return 1;
        }
        plugin->enableAllBuses();
        plugin->setPlayConfigDetails (0, juce::jmax (2, plugin->getTotalNumOutputChannels()), sampleRate, blockSize);
        plugin->prepareToPlay (sampleRate, blockSize);
        plugin->setNonRealtime (true);
        idle (*plugin, 300);
        if (trainMode)
        {
            juce::StringArray folders;
            for (int i = 3; i < argc; ++i)
                folders.add (juce::String (argv[i]));
            const auto result = train::run (*plugin, folders);
            plugin->releaseResources();
            return result;
        }
        if (juce::String (argv[3]) == "--inspect")
        {
            juce::MemoryBlock state;
            plugin->getStateInformation (state);
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
            {
                std::cout << "host state tag " << xml->getTagName() << std::endl;
                for (auto* child : xml->getChildIterator())
                {
                    juce::MemoryBlock data;
                    data.fromBase64Encoding (child->getAllSubText());
                    std::cout << "  " << child->getTagName() << ": " << data.getSize() << " bytes, starts "
                              << juce::String::toHexString (data.getData(), juce::jmin (96, (int) data.getSize()))
                              << " '" << juce::String::fromUTF8 ((const char*) data.getData(), juce::jmin (48, (int) data.getSize())) << "'" << std::endl;
                }
            }
            else
            {
                std::cout << "state is not XML: " << state.getSize() << " bytes, starts "
                          << juce::String::toHexString (state.getData(), juce::jmin (16, (int) state.getSize())) << std::endl;
            }
            return 0;
        }
        const auto result = presets::run (*plugin, juce::File (juce::String (argv[3])),
                                          juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[4])));
        plugin->releaseResources();
        return result;
    }

    if (argc < 4)
    {
        std::cout << "usage: ilanaRefHost <plugin.vst3> <output folder> <name> [note:velocity ...]" << std::endl;
        return 1;
    }
    const juce::File pluginFile { juce::String (argv[1]) };
    const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));
    const juce::String name { argv[3] };
    folder.createDirectory();

    std::vector<std::pair<int, int>> notes;
    for (int i = 4; i < argc; ++i)
    {
        const juce::String item (argv[i]);
        notes.push_back ({ item.upToFirstOccurrenceOf (":", false, false).getIntValue(),
                           item.fromFirstOccurrenceOf (":", false, false).getIntValue() });
    }
    if (notes.empty())
        for (int velocity : { 40, 120 })
            for (int note : { 40, 60, 84 })
                notes.push_back ({ note, velocity });

    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> types;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, pluginFile.getFullPathName());
    if (types.isEmpty())
    {
        std::cout << "no plugin found in " << pluginFile.getFullPathName() << std::endl;
        return 1;
    }

    juce::String error;
    auto plugin = formats.createPluginInstance (*types[0], sampleRate, blockSize, error);
    if (plugin == nullptr)
    {
        std::cout << "could not load: " << error << std::endl;
        return 1;
    }
    std::cout << "loaded " << plugin->getName() << ", " << plugin->getTotalNumOutputChannels() << " outputs" << std::endl;

    plugin->enableAllBuses();
    plugin->setPlayConfigDetails (0, juce::jmax (2, plugin->getTotalNumOutputChannels()), sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);
    plugin->setNonRealtime (true);
    // Give sample-based or lazily-initialised plugins time to get ready.
    idle (*plugin, 300);

    const int channels = juce::jmax (2, plugin->getTotalNumOutputChannels());
    const int heldBlocks = (int) (4.0 * sampleRate / blockSize);
    const int totalBlocks = (int) (5.0 * sampleRate / blockSize);
    for (const auto& [note, velocity] : notes)
    {
        juce::AudioBuffer<float> buffer (channels, blockSize);
        juce::AudioBuffer<float> output (1, blockSize * totalBlocks);
        for (int block = 0; block < totalBlocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
            if (block == heldBlocks) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            plugin->processBlock (buffer, midi);
            output.copyFrom (0, block * blockSize, buffer, 0, 0, blockSize);
            output.addFrom (0, block * blockSize, buffer, 1, 0, blockSize);
        }
        output.applyGain (0.5f);
        const auto file = folder.getChildFile (name + ".v" + juce::String (velocity) + ".n" + juce::String (note) + ".wav");
        writeWav (file, output);
        std::cout << file.getFileName() << " peak " << output.getMagnitude (0, output.getNumSamples()) << std::endl;
        idle (*plugin, 200);
    }
    plugin->releaseResources();
    plugin.reset();
    return 0;
}
