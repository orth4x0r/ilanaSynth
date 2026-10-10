// Dev tool: renders single notes of the Physical oscillator with chosen
// parameters, for auditing what each string / exciter knob does
// (tools/knob_audit.py drives it).
//
//   ilanaKnobAudit <jobs.txt> <out folder>
//
// Each job line: name|note|velocity|hold seconds|total seconds|id=value;id=value
// writes <out>/<name>.wav (mono, 48 kHz, 24 bit) from the Init patch with
// osc1 in string mode plus the listed parameters.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <fstream>
#include <iostream>

#include "PluginProcessor.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    else
        std::cerr << "unknown parameter " << id << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (argc < 3)
    {
        std::cerr << "usage: ilanaKnobAudit <jobs.txt> <out folder>" << std::endl;
        return 1;
    }

    const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    folder.createDirectory();
    std::ifstream jobs (argv[1]);
    IlanaSynthAudioProcessor processor;
    processor.setNonRealtime (true);

    std::string line;
    while (std::getline (jobs, line))
    {
        juce::StringArray fields;
        fields.addTokens (juce::String (line), "|", "");
        if (fields.size() < 6)
            continue;
        const auto name = fields[0];
        const auto note = fields[1].getIntValue();
        const auto velocity = fields[2].getIntValue();
        const auto hold = fields[3].getDoubleValue();
        const auto total = fields[4].getDoubleValue();

        processor.prepareToPlay (sampleRate, blockSize);
        processor.loadFactoryPreset (0); // Init
        setParam (processor, "osc1_mode", 1.0f);
        for (const auto& item : juce::StringArray::fromTokens (fields[5], ";", ""))
            if (item.contains ("="))
                setParam (processor, item.upToFirstOccurrenceOf ("=", false, false),
                          item.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
        processor.panic();
        processor.releaseResources();
        processor.prepareToPlay (sampleRate, blockSize);

        const auto blocks = (int) std::ceil (total * sampleRate / blockSize);
        juce::AudioBuffer<float> out (1, blocks * blockSize), buffer (2, blockSize);
        const auto offBlock = (int) std::llround (hold * sampleRate);
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            const auto start = block * blockSize;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
            if (offBlock >= start && offBlock < start + blockSize)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), offBlock - start);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                out.setSample (0, start + i, 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        }

        const auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (new juce::FileOutputStream (file), sampleRate, 1, 24, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    }
    return 0;
}
