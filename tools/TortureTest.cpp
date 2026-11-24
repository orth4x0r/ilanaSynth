// Dev tool: concurrency torture. One processor; the audio thread renders
// blocks of random length (1..2048) with random MIDI while the main thread
// (the message thread) edits the patch as fast as it can: factory presets,
// user wavetables, user samples, undo / redo, FX slot swaps and assigns, and
// random parameter changes. Exits 0 when no NaN/inf came out, no thread-check
// violation (src/ThreadChecks.h) was counted and nothing crashed. Run it under a sanitizer, which is the point:
//
//   cmake -B build-tsan -DILANA_SANITIZE=thread
//   cmake --build build-tsan --target ilanaTorture
//   build-tsan/ilanaTorture_artefacts/ilanaTorture
//
// ILANA_TORTURE_SECONDS=n sets the length (default 30); ILANA_TORTURE_SEED=n
// the random seed (default 1). The processor is prepared at 48 kHz with a
// 2048 block, so every render length fits.

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>

#include "PluginProcessor.h"
#include "ThreadChecks.h"
#include "dsp/SamplePlayer.h"
#include "dsp/WavetableDoc.h"

namespace
{
constexpr int maxBlock = 2048;

std::atomic<bool> running { true };
std::atomic<long long> blocksRendered { 0 };
std::atomic<long long> samplesRendered { 0 };
std::atomic<long long> badSamples { 0 };
std::atomic<float> peak { 0.0f };

void renderLoop (IlanaSynthAudioProcessor& processor, int seed)
{
    juce::Random random (seed);
    juce::AudioBuffer<float> buffer (2, maxBlock);
    std::vector<int> held;

    while (running.load())
    {
        const auto length = 1 + random.nextInt (maxBlock);
        juce::MidiBuffer midi;

        const auto events = random.nextInt (4);
        for (int e = 0; e < events; ++e)
        {
            const auto position = random.nextInt (length);
            const auto roll = random.nextInt (10);

            if (roll < 5 || held.empty())
            {
                const auto note = 24 + random.nextInt (72);
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (1 + random.nextInt (127))), position);
                held.push_back (note);
            }
            else if (roll < 9)
            {
                const auto which = (size_t) random.nextInt ((int) held.size());
                midi.addEvent (juce::MidiMessage::noteOff (1, held[which]), position);
                held.erase (held.begin() + (long) which);
            }
            else
            {
                midi.addEvent (juce::MidiMessage::allNotesOff (1), position);
                held.clear();
            }
        }

        buffer.setSize (2, maxBlock, false, false, true);
        buffer.clear();
        juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, length);
        processor.processBlock (block, midi);

        long long bad = 0;
        auto top = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            const auto* data = block.getReadPointer (c);
            for (int i = 0; i < length; ++i)
            {
                if (! std::isfinite (data[i]))
                    ++bad;
                else
                    top = juce::jmax (top, std::abs (data[i]));
            }
        }

        if (bad > 0)
            badSamples += bad;
        if (top > peak.load())
            peak.store (top);

        ++blocksRendered;
        samplesRendered += length;
    }
}

std::vector<std::vector<float>> makeFrames (juce::Random& random)
{
    const auto frames = 2 + random.nextInt (3);
    std::vector<std::vector<float>> out ((size_t) frames, std::vector<float> ((size_t) Wavetable::frameSize));
    for (int f = 0; f < frames; ++f)
    {
        const auto harmonic = 1 + random.nextInt (8);
        for (int i = 0; i < Wavetable::frameSize; ++i)
            out[(size_t) f][(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * harmonic * i / Wavetable::frameSize) * 0.8f;
    }
    return out;
}

std::shared_ptr<SampleData> makeSample (juce::Random& random)
{
    auto data = std::make_shared<SampleData>();
    const auto length = 500 + random.nextInt (20000);
    data->buffer.setSize (1 + random.nextInt (2), length);
    for (int c = 0; c < data->buffer.getNumChannels(); ++c)
        for (int i = 0; i < length; ++i)
            data->buffer.setSample (c, i, random.nextFloat() * 2.0f - 1.0f);
    data->sampleRate = 22050.0 + random.nextInt (4) * 11025.0;
    data->name = "torture";
    return data;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto seconds = juce::jmax (1, juce::SystemStats::getEnvironmentVariable ("ILANA_TORTURE_SECONDS", "30").getIntValue());
    const auto seed = juce::SystemStats::getEnvironmentVariable ("ILANA_TORTURE_SEED", "1").getIntValue();

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, maxBlock);
    const auto presetCount = (int) processor.getFactoryPresetNames().size();
    const auto parameters = processor.getParameters();

    std::thread audio ([&processor, seed] { renderLoop (processor, seed + 1000); });

    juce::Random random (seed);
    long long steps = 0;
    long long counts[8] {};
    const auto end = juce::Time::getMillisecondCounterHiRes() + 1000.0 * seconds;
    auto lastReport = juce::Time::getMillisecondCounterHiRes();

    while (juce::Time::getMillisecondCounterHiRes() < end)
    {
        const auto step = random.nextInt (8);
        ++counts[step];

        switch (step)
        {
            case 0:
                processor.loadFactoryPreset (random.nextInt (juce::jmax (1, presetCount)));
                break;
            case 1:
                processor.setUserTable (random.nextInt (4), WavetableDoc::fromFrames ("torture", makeFrames (random)));
                break;
            case 2:
                // path empty: embedded in the patch, nothing read from disk
                processor.setUserSample (random.nextInt (OscillatorIds::count), random.nextInt (6) == 0 ? nullptr : makeSample (random), {});
                break;
            case 3:
                processor.getUndoManager().undo();
                break;
            case 4:
                processor.getUndoManager().redo();
                break;
            case 5:
                processor.performEdit ("torture swap", [&] { processor.swapFxSlots (random.nextInt (8), random.nextInt (8)); });
                break;
            case 6:
                processor.performEdit ("torture assign", [&] { processor.assignFxSlot (1 + random.nextInt (8), random.nextInt (IlanaSynthAudioProcessor::numFxTypes)); });
                break;
            default:
                if (! parameters.isEmpty())
                    if (auto* parameter = parameters[random.nextInt (parameters.size())])
                        parameter->setValueNotifyingHost (random.nextFloat());
                break;
        }

        ++steps;
        // Let timers and async updates run, as the real message thread would.
       #if JUCE_MODAL_LOOPS_PERMITTED
        juce::MessageManager::getInstance()->runDispatchLoopUntil (random.nextInt (3));
       #else
        juce::Thread::sleep (random.nextInt (3));
       #endif

        if (juce::Time::getMillisecondCounterHiRes() - lastReport > 5000.0)
        {
            lastReport = juce::Time::getMillisecondCounterHiRes();
            std::cout << "... " << steps << " edits, " << blocksRendered.load() << " blocks" << std::endl;
        }
    }

    running = false;
    audio.join();

    std::cout << "ilanaTorture: " << seconds << " s, " << blocksRendered.load() << " blocks ("
              << samplesRendered.load() << " samples), " << steps << " edits\n"
              << "  presets " << counts[0] << ", tables " << counts[1] << ", samples " << counts[2]
              << ", undo " << counts[3] << ", redo " << counts[4] << ", fx swaps " << counts[5]
              << ", fx assigns " << counts[6] << ", parameters " << counts[7] << "\n"
              << "  peak " << peak.load() << ", non-finite samples " << badSamples.load() << std::endl;

#if ILANA_THREAD_CHECKS
    const auto violations = IlanaThreads::violationCount.load();
    std::cout << "  thread-check violations " << violations << std::endl;
#else
    const auto violations = 0;
#endif
    const auto ok = badSamples.load() == 0 && blocksRendered.load() > 0 && violations == 0;
    std::cout << (ok ? "TORTURE PASSED" : "TORTURE FAILED") << std::endl;
    return ok ? 0 : 1;
}
