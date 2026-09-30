// Dev tool: renders reference notes from an installed VST3 instrument's
// default (init) sound, for fitting modelled sounds (not shipped).
//
// usage: ilanaRefHost <plugin.vst3> <output folder> <name> [note:velocity ...]
//        ilanaRefHost --presets <plugin.vst3> <preset folder> <output folder>
// The second form loads each .vital preset and renders the preset critic's
// test phrases (see tools/preset_critic.py), as reference renders.
// Each note is held for 4 s and released for 1 s at 48 kHz, written as
// <name>.v<velocity>.n<note>.wav (mono, 24-bit). The default notes are
// 40, 60 and 84 at velocities 40 and 120.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
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
    void idle (juce::AudioPluginInstance& plugin, int blocks)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, plugin.getTotalNumOutputChannels()), blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            plugin.processBlock (buffer, midi);
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
        // Let tails die away before the next phrase.
        idle (plugin, (int) (3.0 * sampleRate / blockSize));
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

    // ilanaRefHost --presets <plugin.vst3> <preset folder> <out folder>
    // Loads every .vital file (the plugin's state is the preset's JSON) and
    // renders the critic's phrases into <out>/<n>/, with an index.csv.
    int run (juce::AudioPluginInstance& plugin, const juce::File& presetFolder, const juce::File& folder)
    {
        folder.createDirectory();
        std::ofstream index (folder.getChildFile ("index.csv").getFullPathName().toStdString());
        index << "index,name,category,home_note,macro1,macro2,macro3,macro4\n";
        auto files = presetFolder.findChildFiles (juce::File::findFiles, true, "*.vital");
        files.sort();
        int count = 0;
        for (const auto& file : files)
        {
            const auto json = file.loadFileAsString();
            const auto parsed = juce::JSON::parse (json);
            const auto category = categoryFor (parsed.getProperty ("preset_style", "").toString());
            if (category.isEmpty() || ! juce::StringArray { "Bass", "Pad", "Lead", "Keys", "Pluck" }.contains (category))
                continue;

            juce::MemoryBlock state (json.toRawUTF8(), json.getNumBytesAsUTF8());
            plugin.setStateInformation (state.getData(), (int) state.getSize());
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

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    const auto presetMode = argc >= 5 && juce::String (argv[1]) == "--presets";
    if (presetMode)
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
