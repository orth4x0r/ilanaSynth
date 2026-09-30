// SoundFont (SF2) and SFZ multisamples in the Sample oscillator: the loaders
// read regions, roots, levels and loops, and a note plays the region that
// covers it at the right pitch. Included by TableTest.cpp;
// ILANA_MULTISAMPLE_TEST=1 runs only these.

#include "dsp/MultiSample.h"

namespace MultiSampleLab
{
inline juce::AudioBuffer<float> sine (double hz, double sampleRate, int length)
{
    juce::AudioBuffer<float> buffer (1, length);
    for (int i = 0; i < length; ++i)
        buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate));
    return buffer;
}

inline void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (new juce::FileOutputStream (file), sampleRate, 1, 16, {}, 0));
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

// A minimal SoundFont: one preset, one instrument, two sine zones (A3 below
// 69, A5 from 69, the upper one looped), 16-bit mono at 32 kHz.
inline void writeSf2 (const juce::File& file)
{
    juce::MemoryOutputStream smpl;
    const auto lowLength = 32000, highLength = 8000;
    const auto addSine = [&smpl] (double hz, int length)
    {
        for (int i = 0; i < length; ++i)
            smpl.writeShort ((short) (16000.0 * std::sin (juce::MathConstants<double>::twoPi * hz * i / 32000.0)));
        for (int i = 0; i < 46; ++i) // the spec's zero padding after each sample
            smpl.writeShort (0);
    };
    addSine (220.0, lowLength);
    addSine (880.0, highLength);

    const auto chunk = [] (juce::MemoryOutputStream& out, const char* id, const juce::MemoryBlock& data)
    {
        out.write (id, 4);
        out.writeInt ((int) data.getSize());
        out.write (data.getData(), data.getSize());
        if (data.getSize() & 1)
            out.writeByte (0);
    };
    const auto name20 = [] (juce::MemoryOutputStream& out, const char* name)
    {
        char buffer[20] {};
        std::strncpy (buffer, name, 19);
        out.write (buffer, 20);
    };
    const auto gen = [] (juce::MemoryOutputStream& out, int oper, int amount)
    {
        out.writeShort ((short) oper);
        out.writeShort ((short) amount);
    };
    const auto range = [] (int lo, int hi) { return lo | (hi << 8); };

    juce::MemoryOutputStream phdr, pbag, pgen, pmod, inst, ibag, igen, imod, shdr;
    name20 (phdr, "Test Keys"); phdr.writeShort (0); phdr.writeShort (0); phdr.writeShort (0); phdr.writeInt (0); phdr.writeInt (0); phdr.writeInt (0);
    name20 (phdr, "EOP"); phdr.writeShort (0); phdr.writeShort (0); phdr.writeShort (1); phdr.writeInt (0); phdr.writeInt (0); phdr.writeInt (0);
    pbag.writeShort (0); pbag.writeShort (0);
    pbag.writeShort (1); pbag.writeShort (0);
    gen (pgen, MultiSample::Sf2::instrument, 0);
    gen (pgen, 0, 0);
    for (int i = 0; i < 10; ++i) pmod.writeByte (0);
    name20 (inst, "Test Inst"); inst.writeShort (0);
    name20 (inst, "EOI"); inst.writeShort (2);
    ibag.writeShort (0); ibag.writeShort (0);
    ibag.writeShort (3); ibag.writeShort (0);
    ibag.writeShort (8); ibag.writeShort (0);
    gen (igen, MultiSample::Sf2::keyRange, range (0, 68));
    gen (igen, MultiSample::Sf2::overridingRootKey, 57);
    gen (igen, MultiSample::Sf2::sampleID, 0);
    gen (igen, MultiSample::Sf2::keyRange, range (69, 127));
    gen (igen, MultiSample::Sf2::initialAttenuation, 60); // 6 dB * 0.4 = 2.4 dB down
    gen (igen, MultiSample::Sf2::sampleModes, 1);
    gen (igen, MultiSample::Sf2::coarseTune, 0);
    gen (igen, MultiSample::Sf2::sampleID, 1);
    gen (igen, 0, 0);
    for (int i = 0; i < 10; ++i) imod.writeByte (0);
    const auto header = [&shdr, &name20] (const char* name, int start, int end, int loopStart, int loopEnd, int pitch)
    {
        name20 (shdr, name);
        shdr.writeInt (start); shdr.writeInt (end); shdr.writeInt (loopStart); shdr.writeInt (loopEnd);
        shdr.writeInt (32000); shdr.writeByte ((char) pitch); shdr.writeByte (0); shdr.writeShort (0); shdr.writeShort (1);
    };
    header ("low", 0, lowLength, 1000, 2000, 60);
    const auto highStart = lowLength + 46;
    // The loop is 1600 samples: 44 whole cycles of 880 Hz at 32 kHz.
    header ("high", highStart, highStart + highLength, highStart + 4000, highStart + 5600, 81);
    header ("EOS", 0, 0, 0, 0, 0);

    juce::MemoryOutputStream sdta, pdta, body;
    sdta.write ("sdta", 4);
    chunk (sdta, "smpl", smpl.getMemoryBlock());
    pdta.write ("pdta", 4);
    chunk (pdta, "phdr", phdr.getMemoryBlock()); chunk (pdta, "pbag", pbag.getMemoryBlock());
    chunk (pdta, "pmod", pmod.getMemoryBlock()); chunk (pdta, "pgen", pgen.getMemoryBlock());
    chunk (pdta, "inst", inst.getMemoryBlock()); chunk (pdta, "ibag", ibag.getMemoryBlock());
    chunk (pdta, "imod", imod.getMemoryBlock()); chunk (pdta, "igen", igen.getMemoryBlock());
    chunk (pdta, "shdr", shdr.getMemoryBlock());
    body.write ("sfbk", 4);
    chunk (body, "LIST", sdta.getMemoryBlock());
    chunk (body, "LIST", pdta.getMemoryBlock());

    juce::MemoryOutputStream riff;
    chunk (riff, "RIFF", body.getMemoryBlock());
    file.replaceWithData (riff.getData(), riff.getDataSize());
}

// A sine's frequency from its rising zero crossings (the autocorrelation
// estimator can take a pure sine's second period).
inline double sineFrequency (const std::vector<float>& x, double sampleRate)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const auto t = (double) (i - 1) + (double) (-x[i - 1] / (x[i] - x[i - 1]));
            if (first < 0.0)
                first = t;
            last = t;
            ++count;
        }
    return count > 1 ? (count - 1) * sampleRate / (last - first) : 0.0;
}

// Plays one note on OSC 1 in Sample mode (the rest silent, the filter open)
// and returns the output after the attack.
inline std::vector<float> play (IlanaSynthAudioProcessor& processor, int note, int blocks)
{
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> out;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

inline void setUp (IlanaSynthAudioProcessor& processor)
{
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (0);
    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    set ("osc1_mode", 2.0f);
    set ("osc1_sample_tuned", 1.0f);
    set ("osc1_sample_loop", 0.0f);
    set ("f1_cutoff", 20000.0f);
    set ("amp_sustain", 1.0f);
    set ("subosc_on", 0.0f);
    set ("subosc_level", 0.0f);
    set ("noise_level", 0.0f);
}
} // namespace MultiSampleLab

void runMultiSampleTests()
{
    using namespace MultiSampleLab;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-multisample-test");
    folder.deleteRecursively();
    folder.createDirectory();

    // SFZ: two regions (one path with a space, a group volume, a loop).
    writeWav (folder.getChildFile ("low.wav"), sine (220.0, 44100.0, 44100), 44100.0);
    writeWav (folder.getChildFile ("high tone.wav"), sine (880.0, 44100.0, 22050), 44100.0);
    const auto sfz = folder.getChildFile ("test.sfz");
    sfz.replaceWithText ("// test instrument\n<control> default_path=./\n<group> volume=-6\n"
                         "<region> sample=low.wav lokey=0 hikey=c5 pitch_keycenter=a3\n"
                         "<region> sample=high tone.wav lokey=69 hikey=127 pitch_keycenter=81 tune=-50 "
                         "loop_mode=loop_continuous loop_start=1000 loop_end=21000\n");
    juce::String error;
    const auto sfzData = MultiSample::load (sfz, error);
    check (sfzData != nullptr && sfzData->zones.size() == 2, "SFZ: two regions read (" + error + ")");
    if (sfzData != nullptr && sfzData->zones.size() == 2)
    {
        const auto& low = sfzData->zones[0];
        const auto& high = sfzData->zones[1];
        check (low.loKey == 0 && low.hiKey == 72 && std::abs (low.rootNote - 57.0) < 1e-9, "SFZ: note names (c5 = 72, a3 = 57)");
        check (std::abs (high.rootNote - 81.5) < 1e-9 && high.loop && high.loopStart == 1000 && high.loopEnd == 21001,
               "SFZ: tune folds into the root; the loop is read");
        check (std::abs (juce::Decibels::gainToDecibels (low.gain) + 6.0f) < 0.01f, "SFZ: a group's volume reaches its regions");
        check (sfzData->zoneFor (60, 100) == &low && sfzData->zoneFor (100, 100) == &high, "SFZ: notes pick their region");
    }

    // SF2: one preset, two zones.
    const auto sf2 = folder.getChildFile ("test.sf2");
    writeSf2 (sf2);
    const auto sf2Data = MultiSample::load (sf2, error);
    check (sf2Data != nullptr && sf2Data->zones.size() == 2, "SF2: two zones read (" + error + ")");
    if (sf2Data != nullptr && sf2Data->zones.size() == 2)
    {
        const auto& low = sf2Data->zones[0];
        const auto& high = sf2Data->zones[1];
        check (sf2Data->name == "Test Keys", "SF2: the preset's name");
        check (low.hiKey == 68 && std::abs (low.rootNote - 57.0) < 1e-9 && ! low.loop, "SF2: the overriding root key; no loop");
        check (high.loKey == 69 && std::abs (high.rootNote - 81.0) < 1e-9 && high.loop && high.loopStart == 4000 && high.loopEnd == 5600,
               "SF2: original pitch, loop points relative to the sample");
        check (std::abs (juce::Decibels::gainToDecibels (high.gain) + 2.4f) < 0.01f, "SF2: attenuation in centibels (x0.4)");
        check (low.data->sampleRate == 32000.0 && low.data->getNumSamples() == 32000, "SF2: the sample's own rate and length");
    }

    // Playing: each note plays its region at its pitch.
    for (const auto& [file, label] : { std::pair<juce::File, const char*> { sfz, "SFZ" }, { sf2, "SF2" } })
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor);
        check (processor.loadUserSample (0, file), juce::String (label) + ": loads into OSC 1");
        for (const auto note : { 48, 60, 76, 88 })
        {
            const auto out = play (processor, note, 60);
            const std::vector<float> tail (out.begin() + 4096, out.begin() + 4096 + 16384);
            const auto measured = sineFrequency (tail, 48000.0);
            const auto expected = juce::MidiMessage::getMidiNoteInHertz (note) * (juce::String (label) == "SFZ" && note >= 69 ? std::exp2 (-0.5 / 12.0) : 1.0);
            const auto cents = 1200.0 * std::log2 (measured / expected);
            check (std::abs (cents) < 5.0, juce::String (label) + ": note " + juce::String (note) + " at pitch ("
                                               + juce::String (cents, 1) + " cents)");
            // Let the note go.
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            processor.processBlock (buffer, off);
            for (int b = 0; b < 200; ++b) { juce::MidiBuffer none; processor.processBlock (buffer, none); }
        }
    }

    // The SF2's looped zone sustains past its 0.25 s sample; the unlooped one ends.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor);
        processor.loadUserSample (0, sf2);
        const auto looped = play (processor, 81, 200); // ~2.1 s
        const auto late = rmsOf (looped, looped.size() - 9600);
        check (late > 0.01, "SF2: a looped zone sustains (rms " + juce::String (late, 4) + ")");
    }

    // Plain samples are unchanged by the zones: a WAV still has none.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor);
        processor.loadUserSample (0, folder.getChildFile ("low.wav"));
        check (processor.getSampleForOsc (0) != nullptr && processor.getSampleForOsc (0)->zones.empty(), "a WAV is a plain sample");
    }

    folder.deleteRecursively();
}
