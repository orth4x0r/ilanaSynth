// Dev tool: renders reference notes from an installed VST3 instrument's
// default (init) sound, for fitting modelled sounds (not shipped).
//
// usage: ilanaRefHost <plugin.vst3> <output folder> <name> [note:velocity ...]
// Each note is held for 4 s and released for 1 s at 48 kHz, written as
// <name>.v<velocity>.n<note>.wav (mono, 24-bit). The default notes are
// 40, 60 and 84 at velocities 40 and 120.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
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

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
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
