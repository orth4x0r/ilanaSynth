// Dev tool: plays a stress phrase through the full processor (sustain pedal,
// fast notes, the same key retriggered, a 40-note chord that steals voices,
// a preset change under a held note, all-notes-off, then silence) and
// checks that the output stays finite and bounded, that every voice ends
// in the silence, and that Multi-Core Voices on and off render the same
// samples. Blocks come in random lengths, like a host that splits blocks.
//
//   ilanaStress [user-preset-folder]
//
// ILANA_STRESS_ONLY=text runs only the presets whose names contain it;
// ILANA_STRESS_QUICK=1 skips the sample-rate and block-size sweep;
// ILANA_STRESS_SAME=0|1 makes both runs one-core (or both multi-core);
// ILANA_STRESS_REALTIME=1 renders as live (tables built in the background,
// so runs no longer match sample for sample; for ThreadSanitizer);
// ILANA_STRESS_SECONDS=n lengthens the phrase.

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>

#include "PluginProcessor.h"
#include "dsp/Voice.h"

namespace
{
struct Preset
{
    juce::String name;
    int factoryIndex = -1;
    juce::File file;
};

void load (IlanaSynthAudioProcessor& processor, const Preset& preset)
{
    if (preset.factoryIndex >= 0)
        processor.loadFactoryPreset (preset.factoryIndex);
    else
        processor.loadPresetFromFile (preset.file);
}

struct Result
{
    std::vector<float> samples;
    bool finite = true;
    float peak = 0.0f;
    double peakTime = 0.0;
    int voicesLeft = 0;
    float tailPeak = 0.0f;
};

// One MIDI event at a time in seconds.
struct Event
{
    double time;
    juce::MidiMessage message;
    bool switchPreset = false;
};

std::vector<Event> stressPhrase()
{
    std::vector<Event> events;
    const auto add = [&events] (double t, juce::MidiMessage m) { events.push_back ({ t, m }); };
    add (0.0, juce::MidiMessage::controllerEvent (1, 64, 127));
    for (int k = 0; k < 40; ++k)
    {
        const auto t = k * 0.05;
        const auto key = 36 + (k * 7) % 48;
        add (t, juce::MidiMessage::noteOn (1, key, (juce::uint8) (40 + (k * 13) % 87)));
        add (t + 0.04, juce::MidiMessage::noteOff (1, key));
    }
    add (2.0, juce::MidiMessage::controllerEvent (1, 64, 0));
    // The same key, fast, sometimes without a note-off in between.
    for (int k = 0; k < 20; ++k)
    {
        const auto t = 2.2 + k * 0.025;
        add (t, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
        if (k % 3 != 0)
            add (t + 0.015, juce::MidiMessage::noteOff (1, 60));
    }
    add (2.8, juce::MidiMessage::noteOff (1, 60));
    // More notes than voices: stealing.
    for (int k = 0; k < 40; ++k)
        add (3.0, juce::MidiMessage::noteOn (1, 30 + k * 2, (juce::uint8) 90));
    add (3.5, juce::MidiMessage::pitchWheel (1, 12000));
    add (3.8, juce::MidiMessage::pitchWheel (1, 8192));
    for (int k = 0; k < 40; ++k)
        add (4.0, juce::MidiMessage::noteOff (1, 30 + k * 2));
    // A preset change under a held note, then the note ends.
    add (4.5, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100));
    events.push_back ({ 5.0, juce::MidiMessage::noteOn (1, 0, (juce::uint8) 0), true });
    add (5.2, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100));
    add (5.5, juce::MidiMessage::noteOff (1, 48));
    add (5.6, juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100));
    add (6.0, juce::MidiMessage::allNotesOff (1));
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    return events;
}

const double phraseSeconds = juce::jmax (8.0, juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_SECONDS", "16").getDoubleValue());

// Renders the phrase; a second prepareToPlay at rateChangeAt (seconds,
// negative for none) switches to newRate and newBlock under held notes.
Result run (const Preset& preset, const Preset& switchTo, double sampleRate, int maxBlock, bool randomBlocks,
            int threads, double rateChangeAt = -1.0, double newRate = 0.0, int newBlock = 0)
{
    IlanaSynthAudioProcessor::forceVoiceThreads = threads;
    IlanaSynthAudioProcessor processor;
    // Rendering offline: tables are built on the spot rather than in the
    // background, so two runs can be compared sample for sample.
    processor.setNonRealtime (juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_REALTIME", "").isEmpty());
    processor.prepareToPlay (sampleRate, maxBlock);
    load (processor, preset);
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, maxBlock);

    const auto events = stressPhrase();
    size_t nextEvent = 0;
    juce::Random random (1234);
    Result result;
    juce::AudioBuffer<float> buffer (2, juce::jmax (maxBlock, newBlock));
    double rate = sampleRate;
    int block = maxBlock;
    double seconds = 0.0;
    bool rateChanged = false;

    while (seconds < phraseSeconds)
    {
        if (! rateChanged && rateChangeAt >= 0.0 && seconds >= rateChangeAt)
        {
            rateChanged = true;
            rate = newRate;
            block = newBlock;
            processor.prepareToPlay (rate, block);
        }
        const auto n = randomBlocks ? 1 + random.nextInt (block) : block;
        const auto blockEnd = seconds + n / rate;
        juce::MidiBuffer midi;
        while (nextEvent < events.size() && events[nextEvent].time < blockEnd)
        {
            const auto& event = events[nextEvent++];
            if (event.switchPreset)
            {
                load (processor, switchTo);
                continue;
            }
            const auto offset = juce::jlimit (0, n - 1, (int) ((event.time - seconds) * rate));
            midi.addEvent (event.message, offset);
        }
        buffer.clear();
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, n);
        processor.processBlock (view, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < n; ++i)
            {
                const auto v = view.getSample (channel, i);
                if (! std::isfinite (v))
                    result.finite = false;
                else
                {
                    if (std::abs (v) > result.peak)
                    {
                        result.peak = std::abs (v);
                        result.peakTime = seconds + i / rate;
                    }
                    if (seconds > phraseSeconds - 1.0)
                        result.tailPeak = juce::jmax (result.tailPeak, std::abs (v));
                }
            }
        for (int i = 0; i < n; ++i)
        {
            result.samples.push_back (view.getSample (0, i));
            result.samples.push_back (view.getSample (1, i));
        }
        seconds = blockEnd;
    }
    result.voicesLeft = processor.getRenderingVoiceCount();
    return result;
}

int failures = 0;

void check (const juce::String& what, const Result& r)
{
    juce::StringArray problems;
    if (! r.finite)
        problems.add ("NaN/inf");
    if (r.peak > 4.0f)
        problems.add ("peak " + juce::String (r.peak, 2) + " at " + juce::String (r.peakTime, 2) + " s");
    if (r.voicesLeft > 0)
        problems.add (juce::String (r.voicesLeft) + " voices still on after 10 s of silence (tail peak " + juce::String (r.tailPeak, 6) + ")");
    if (problems.isEmpty())
        return;
    ++failures;
    std::cout << "PROBLEM " << what << ": " << problems.joinIntoString ("; ") << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    // The same switches as ilanaFingerprint, to tell a new problem from an old one.
    Voice::disableReleaseSilence = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_RELEASE_SILENCE", "").isNotEmpty();
    Voice::disableSlowEnvelopes = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_SLOW_ENVELOPES", "").isNotEmpty();
    IlanaSynthAudioProcessor::disableFxSleep = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_FX_SLEEP", "").isNotEmpty();
    const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_ONLY", "");
    const auto quick = juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_QUICK", "").isNotEmpty();
    const auto sameValue = juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_SAME", "");
    const auto same = sameValue.isNotEmpty();
    const auto sameThreads = sameValue.getIntValue();

    std::vector<Preset> presets;
    {
        IlanaSynthAudioProcessor processor;
        const auto names = processor.getFactoryPresetNames();
        for (int i = 0; i < names.size(); ++i)
            presets.push_back ({ names[i], i, {} });
    }
    if (argc > 1)
        for (const auto& file : juce::File (argv[1]).findChildFiles (juce::File::findFiles, false, "*.ilanapreset"))
            presets.push_back ({ file.getFileNameWithoutExtension(), -1, file });

    // prepareToPlay again and again with no audio in between (hosts do
    // this when settings change): each one stops and restarts the voice
    // threads, sometimes before they have started.
    {
        IlanaSynthAudioProcessor::forceVoiceThreads = 1;
        IlanaSynthAudioProcessor processor;
        for (int i = 0; i < 300; ++i)
        {
            processor.prepareToPlay (i % 2 == 0 ? 48000.0 : 44100.0, 64 << (i % 4));
            if (i % 3 == 0)
                processor.releaseResources();
        }
        std::cout << "ok 300 prepareToPlay calls in a row" << std::endl;
    }

    int runs = 0, mismatches = 0;
    for (size_t p = 0; p < presets.size(); ++p)
    {
        const auto& preset = presets[p];
        if (only.isNotEmpty() && ! preset.name.containsIgnoreCase (only))
            continue;
        const auto& next = presets[(p + 1) % presets.size()];

        // Random block lengths up to 512 at 48 kHz, one core vs several.
        // With ILANA_STRESS_SAME, two runs alike: a race shows as two
        // multi-core runs that differ, state left over between runs as two
        // one-core runs that differ.
        const auto single = run (preset, next, 48000.0, 512, true, same ? sameThreads : 0);
        const auto multi = run (preset, next, 48000.0, 512, true, same ? sameThreads : 1);
        runs += 2;
        check (preset.name + " (48 kHz, random blocks, one core)", single);
        // ILANA_STRESS_DUMP=folder: the one-core render as raw stereo floats.
        if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_STRESS_DUMP", ""); dump.isNotEmpty())
            juce::File (dump).getChildFile (juce::File::createLegalFileName (preset.name) + ".f32")
                .replaceWithData (single.samples.data(), single.samples.size() * sizeof (float));
        if (single.samples != multi.samples)
        {
            ++mismatches;
            ++failures;
            double maxDiff = 0.0;
            size_t first = 0;
            for (size_t i = 0; i < juce::jmin (single.samples.size(), multi.samples.size()); ++i)
            {
                const auto d = (double) std::abs (single.samples[i] - multi.samples[i]);
                if (d > 0.0 && maxDiff == 0.0)
                    first = i;
                maxDiff = juce::jmax (maxDiff, d);
            }
            std::cout << "PROBLEM " << preset.name << ": multi-core output differs (max " << maxDiff << ", from "
                      << first / 2 / 48000.0 << " s)" << std::endl;
        }

        if (! quick)
        {
            struct Shape { double rate; int block; bool random; };
            for (const auto shape : { Shape { 44100.0, 16, false }, Shape { 96000.0, 2048, false }, Shape { 44100.0, 4096, true } })
            {
                const auto r = run (preset, next, shape.rate, shape.block, shape.random, 1);
                ++runs;
                check (preset.name + " (" + juce::String (shape.rate / 1000.0, 1) + " kHz, block " + juce::String (shape.block) + (shape.random ? " random" : "") + ")", r);
            }
            // The sample rate and block size change under held notes.
            const auto r = run (preset, next, 44100.0, 256, false, 1, 3.2, 96000.0, 64);
            ++runs;
            check (preset.name + " (44.1 -> 96 kHz mid-phrase)", r);
        }
        std::cout << "ok " << preset.name << " peak " << single.peak << std::endl;
    }
    std::cout << runs << " runs, " << mismatches << " multi-core mismatches, " << failures << " problems" << std::endl;
    return failures == 0 ? 0 : 1;
}
