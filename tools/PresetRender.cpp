// Dev tool: renders every factory preset through a fixed set of test phrases
// to .wav files, for the preset critic (tools/preset_critic.py).
//
//   ilanaPresetRender <out folder> [first index] [count]
//
// ILANA_RENDER_CATEGORY=Pad renders only that category's presets (for
// iterating on one category; the critic and diversity tools work per
// category).
//
// For each preset <out>/<index>/ holds:
//   note.wav     the home note (C2 for basses, C4 otherwise) held 2 s, then 1.5 s of release
//   chord.wav    a triad plus octave on the home note, held 2 s, then 1.5 s of release
//   line.wav     an eight-note phrase in eighths at 120 BPM
//   vel.wav      the home note at velocities 20, 60, 100 and 127
//   macroN_lo/hi.wav  the home note held 1.5 s with macro N at 0 and at 1
// and <out>/index.csv lists index, name, category, home note and macro names.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <fstream>
#include <iostream>

#include "PluginProcessor.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

struct NoteEvent
{
    double on, off;
    int note, velocity;
};

void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// Loads the preset fresh (no tails from the last render), applies the tweak,
// plays the events and returns the stereo result.
juce::AudioBuffer<float> render (IlanaSynthAudioProcessor& processor, int preset, const std::vector<NoteEvent>& events,
                                 double seconds, const std::function<void (IlanaSynthAudioProcessor&)>& tweak = {})
{
    processor.loadFactoryPreset (preset);
    if (tweak)
        tweak (processor);
    processor.panic();
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);

    const auto totalBlocks = (int) std::ceil (seconds * sampleRate / blockSize);
    juce::AudioBuffer<float> out (2, totalBlocks * blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);

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
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            out.copyFrom (channel, start, buffer, channel, 0, blockSize);
    }
    return out;
}

void write (const juce::File& file, const juce::AudioBuffer<float>& audio)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (new juce::FileOutputStream (file), sampleRate, 2, 24, {}, 0));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

juce::String csvField (const juce::String& text)
{
    return "\"" + text.replace ("\"", "'") + "\"";
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cerr << "usage: ilanaPresetRender <out folder> [first index] [count]" << std::endl;
        return 1;
    }

    const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    folder.createDirectory();

    IlanaSynthAudioProcessor processor;
    processor.setNonRealtime (true);
    processor.prepareToPlay (sampleRate, blockSize);

    const auto names = processor.getFactoryPresetNames();
    const auto categories = processor.getFactoryPresetCategories();
    const auto first = argc > 2 ? juce::jlimit (0, names.size(), juce::String (argv[2]).getIntValue()) : 0;
    const auto count = argc > 3 ? juce::String (argv[3]).getIntValue() : names.size();
    const auto last = juce::jmin (names.size(), first + juce::jmax (0, count));

    const auto indexFile = folder.getChildFile ("index.csv");
    const auto append = first > 0 && indexFile.existsAsFile();
    std::ofstream index (indexFile.getFullPathName().toStdString(), append ? std::ios::app : std::ios::trunc);
    if (! append)
        index << "index,name,category,home_note,macro1,macro2,macro3,macro4\n";

    const auto onlyCategory = juce::SystemStats::getEnvironmentVariable ("ILANA_RENDER_CATEGORY", "");

    for (int preset = first; preset < last; ++preset)
    {
        const auto category = categories[preset];
        if (onlyCategory.isNotEmpty() && category != onlyCategory)
            continue;
        const auto home = category == "Bass" ? 36 : 60;
        const auto out = folder.getChildFile (juce::String (preset));
        out.createDirectory();

        {
            // params.txt: the loaded preset's parameters that differ from
            // their defaults (what the recipe, voicing and trims set).
            processor.loadFactoryPreset (preset);
            juce::String text;
            for (auto* parameter : processor.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    if (std::abs (ranged->getValue() - ranged->getDefaultValue()) > 1.0e-6f)
                        text << ranged->getParameterID() << "=" << ranged->convertFrom0to1 (ranged->getValue()) << "\n";
            for (int macro = 0; macro < 4; ++macro)
                text << "#macro" << (macro + 1) << "=" << processor.getMacroName (macro) << "\n";
            out.getChildFile ("params.txt").replaceWithText (text);
        }

        write (out.getChildFile ("note.wav"), render (processor, preset, { { 0.0, 2.0, home, 100 } }, 3.5));
        write (out.getChildFile ("chord.wav"),
               render (processor, preset,
                       { { 0.0, 2.0, home, 90 }, { 0.0, 2.0, home + 4, 90 }, { 0.0, 2.0, home + 7, 90 }, { 0.0, 2.0, home + 12, 90 } },
                       3.5));

        std::vector<NoteEvent> line;
        const int steps[] { 0, 3, 5, 7, 10, 7, 5, 3 };
        for (int i = 0; i < 8; ++i)
            line.push_back ({ i * 0.25, i * 0.25 + 0.2, home + steps[i], 100 });
        write (out.getChildFile ("line.wav"), render (processor, preset, line, 3.5));

        std::vector<NoteEvent> velocities;
        const int levels[] { 20, 60, 100, 127 };
        for (int i = 0; i < 4; ++i)
            velocities.push_back ({ i * 1.0, i * 1.0 + 0.6, home, levels[i] });
        write (out.getChildFile ("vel.wav"), render (processor, preset, velocities, 4.5));

        juce::StringArray macroNames;
        for (int macro = 0; macro < 4; ++macro)
        {
            const auto id = "macro" + juce::String (macro + 1);
            for (const auto high : { false, true })
                write (out.getChildFile ("macro" + juce::String (macro + 1) + (high ? "_hi.wav" : "_lo.wav")),
                       render (processor, preset, { { 0.0, 1.5, home, 100 } }, 2.0,
                               [&id, high] (IlanaSynthAudioProcessor& p) { setParam (p, id, high ? 1.0f : 0.0f); }));
            macroNames.add (processor.getMacroName (macro));
        }

        index << preset << "," << csvField (names[preset]).toStdString() << "," << csvField (category).toStdString() << ","
              << home;
        for (const auto& name : macroNames)
            index << "," << csvField (name).toStdString();
        index << "\n";
        index.flush();
        std::cout << preset << " " << names[preset].toStdString() << std::endl;
    }

    std::cout << "rendered " << (last - first) << " presets to " << folder.getFullPathName().toStdString() << std::endl;
    return 0;
}
