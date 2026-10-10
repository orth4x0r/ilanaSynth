// Dev tool (not shipped): what the FX rack costs. Builds the engine as the
// effect (ILANA_FX=1), feeds it a steady noisy-tone input and times
// processBlock per block. Reports median and p99 as a share of one core
// (block time / block duration). Used for docs/ROUTING-CPU.md.
//
//   ilanaFxCpuBench                 every scenario
//   ILANA_BENCH_ONLY=types|series|parallel|overhead|sleep   one group
//   ILANA_BENCH_TYPELIST=12,13   effect types the types group measures
//   ILANA_BENCH_SLEEPTYPES=0,13  effect types the sleep group tests
//   ILANA_BENCH_BLOCKS=n            timed blocks per measurement (default 600)

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "PluginProcessor.h"

namespace
{
constexpr double sampleRate = 48000.0;
int blockSize = 512;
int timedBlocks = 600;

void setParam (IlanaSynthAudioProcessor& p, const juce::String& id, float value)
{
    if (auto* parameter = p.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void quiet (IlanaSynthAudioProcessor& p)
{
    p.loadFactoryPreset (0);
    for (const auto* prefix : { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" })
        setParam (p, juce::String (prefix) + "_on", 0.0f);
    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        p.assignFxSlot (slot, 0);
    setParam (p, "subosc_on", 0.0f);
    setParam (p, "noise_level", 0.0f);
    // A Live oscillator held by DRONE plays the input through the voice.
    setParam (p, "osc1_on", 1.0f);
    setParam (p, "osc1_mode", 4.0f);
    setParam (p, "osc1_level", 1.0f);
    setParam (p, "in_trigger", 2.0f);
    setParam (p, "master", 0.0f);
    setParam (p, "master_clip", 0.0f);
    setParam (p, "amp_velocity", 0.0f);
    setParam (p, "amp_attack", 0.001f);
    setParam (p, "amp_sustain", 1.0f);
}

struct Result { double median, p99; double outRms = 0; double mean = 0; };

Result measure (IlanaSynthAudioProcessor& p, bool silentInput = false, int warm = 200)
{
    p.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::Random random (7);
    std::vector<double> times;
    double phase = 0.0, sumSq = 0.0;
    for (int block = 0; block < warm + timedBlocks; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto x = silentInput ? 0.0f
                                       : 0.3f * (float) std::sin (phase) + 0.1f * (random.nextFloat() * 2.0f - 1.0f);
            phase += juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }
        juce::MidiBuffer midi;
        const auto start = std::chrono::steady_clock::now();
        p.processBlock (buffer, midi);
        const auto end = std::chrono::steady_clock::now();
        if (block >= warm)
        {
            times.push_back (std::chrono::duration<double> (end - start).count());
            for (int i = 0; i < blockSize; ++i) sumSq += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
    }
    double total = 0;
    for (const auto t : times) total += t;
    std::sort (times.begin(), times.end());
    const auto blockSeconds = blockSize / sampleRate;
    return { 100.0 * times[times.size() / 2] / blockSeconds, 100.0 * times[(size_t) (times.size() * 0.99)] / blockSeconds, std::sqrt (sumSq / (timedBlocks * blockSize)), 100.0 * total / (double) times.size() / blockSeconds };
}

void row (const juce::String& name, Result r, double base)
{
    std::cout << name.paddedRight (' ', 44).toStdString() << " median " << juce::String (r.median, 3).paddedLeft (' ', 8).toStdString()
              << " %   p99 " << juce::String (r.p99, 3).paddedLeft (' ', 8).toStdString()
              << " %   net " << juce::String (r.median - base, 3).paddedLeft (' ', 8).toStdString() << " %   mean " << juce::String (r.mean, 3).paddedLeft (' ', 8).toStdString() << " %   rms " << juce::String (r.outRms, 3).toStdString() << std::endl;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    if (auto v = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_BLOCKS", ""); v.isNotEmpty())
        timedBlocks = v.getIntValue();
    if (auto v = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_BLOCKSIZE", ""); v.isNotEmpty())
        blockSize = v.getIntValue();
    const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_ONLY", "");
    const auto want = [&only] (const char* g) { return only.isEmpty() || only == g; };

    const juce::StringArray names { "None", "Amp", "Drive", "Crush", "Comp", "Comb", "Phaser",
                                    "Chorus", "Haas", "Delay", "Stutter", "Smear", "Freeze", "Reverb",
                                    "Flanger", "Dimension", "Trance Gate", "TapeStop", "Tilt", "Utility",
                                    "OTT", "Limiter", "Widener", "Tremolo", "FreqShift", "RingMod",
                                    "Octaver", "Vowel", "Feedback", "EQ", "Airwindows", "Vocoder",
                                    "AW Tape", "AW Saturation", "AW Reverb", "AW Delay", "AW Modulation",
                                    "AW Dynamics", "AW EQ", "AW Console", "AW Lo-Fi", "AW Stereo" };

    IlanaSynthAudioProcessor p;
    quiet (p);
    const auto base = measure (p).median;
    std::cout << "engine, no FX, noise+tone input: " << base << " % of a core (block " << blockSize << " @ 48k)\n";

    if (want ("types"))
    {
        std::cout << "\n== one effect alone, default settings (net = minus the empty rack)\n";
        const auto typeList = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_TYPELIST", "");
        for (int type = 1; type < names.size(); ++type)
        {
            if (typeList.isNotEmpty() && ! juce::StringArray::fromTokens (typeList, ",", "").contains (juce::String (type)))
                continue;
            quiet (p);
            p.assignFxSlot (1, type);
            row (names[type], measure (p), base);
        }
    }

    if (want ("blocksize"))
    {
        std::cout << "\n== block size: the same racks at the host's block size (% of a core)\n";
    }

    if (want ("series") || want ("parallel"))
    {
        for (const int type : { 2, 9, 13, 7, 29 })
        {
            for (const int n : { 1, 2, 4, 8 })
            {
                for (const bool parallel : { false, true })
                {
                    if ((parallel && ! want ("parallel")) || (! parallel && ! want ("series"))) continue;
                    quiet (p);
                    for (int s = 1; s <= n; ++s) p.assignFxSlot (s, type);
                    setParam (p, "fx_routing", parallel ? 1.0f : 0.0f);
                    row (juce::String (n) + " x " + names[type] + (parallel ? " PARALLEL" : " SERIES"), measure (p), base);
                }
            }
        }
    }

    if (want ("overhead"))
    {
        std::cout << "\n== slot plumbing around one Chorus (blend/solo/band copy the buffer)\n";
        const auto plain = [&]{ quiet (p); p.assignFxSlot (1, 7); };
        plain(); row ("chorus mix 1.0 (in place)", measure (p), base);
        plain(); setParam (p, "fx_slot1_mix", 0.5f); row ("chorus mix 0.5 (scratch copy + blend)", measure (p), base);
        plain(); setParam (p, "fx_slot1_mix", 0.0f); row ("chorus mix 0.0 (still runs?)", measure (p), base);
        plain(); setParam (p, "fx_slot1_bypass", 1.0f); row ("chorus bypassed", measure (p), base);
        plain(); setParam (p, "fx_slot1_solo", 1.0f); row ("chorus solo", measure (p), base);
        plain(); setParam (p, "fx_slot1_band", 2.0f); row ("chorus band 2 (split filters)", measure (p), base);
        quiet (p); p.assignFxSlot (1, 19); row ("utility (cheapest real effect)", measure (p), base);
        quiet (p); for (int s = 1; s <= 10; ++s) p.assignFxSlot (s, 19); row ("10 x utility series (per-slot overhead)", measure (p), base);
    }

    if (want ("silentblocks"))
    {
        quiet (p);
        row ("measure(): signal", measure (p), base);
        quiet (p);
        row ("measure(): silent input", measure (p, true), base);
    }

    if (want ("sleep"))
    {
        std::cout << "\n== silence: does the rack stop working?\n";
        const auto silentAmp = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_SILENT_AMP", "0").getFloatValue();
        juce::Random random (11);
        std::vector<int> sleepTypes { 0, 13, 9, 7, 2 };
        if (auto v = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_SLEEPTYPES", ""); v.isNotEmpty())
        {
            sleepTypes.clear();
            for (const auto& t : juce::StringArray::fromTokens (v, ",", ""))
                sleepTypes.push_back (t.getIntValue());
        }
        for (const int type : sleepTypes)
        {
            quiet (p);
            if (type > 0) p.assignFxSlot (1, type);
            p.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            for (int b = 0; b < 100; ++b) // 1 s of signal
            {
                for (int i = 0; i < blockSize; ++i) { auto x = 0.3f * std::sin (0.03f * (float) (b * blockSize + i)); buffer.setSample (0, i, x); buffer.setSample (1, i, x); }
                midi.clear();
                const auto t0 = std::chrono::steady_clock::now();
                p.processBlock (buffer, midi);
                if (std::getenv ("ILANA_BENCH_VERBOSE") && b % 20 == 0)
                    std::cout << "  signal block " << b << ": " << 1e6 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() << " us\n";
            }
            std::vector<double> perSecond;
            for (int sec = 0; sec < 6; ++sec)
            {
                double total = 0;
                std::vector<double> each;
                const int blocks = (int) (sampleRate / blockSize);
                for (int b = 0; b < blocks; ++b)
                {
                    buffer.clear();
                    midi.clear();
                    if (silentAmp > 0.0f)
                        for (int i = 0; i < blockSize; ++i) { const auto x = silentAmp * (random.nextFloat() * 2.0f - 1.0f); buffer.setSample (0, i, x); buffer.setSample (1, i, x); }
                    const auto t0 = std::chrono::steady_clock::now();
                    p.processBlock (buffer, midi);
                    const auto dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                    total += dt;
                    each.push_back (dt);
                }
                std::sort (each.begin(), each.end());
                if (std::getenv ("ILANA_BENCH_VERBOSE"))
                    std::cout << "  sec " << sec << " median " << 1e6 * each[each.size() / 2] << " us, max " << 1e6 * each.back() << " us, sum of top 5 " << 1e6 * (each[each.size() - 1] + each[each.size() - 2] + each[each.size() - 3] + each[each.size() - 4] + each[each.size() - 5]) << " us\n";
                perSecond.push_back (100.0 * total);
            }
            juce::String line = names[type].paddedRight (' ', 12) + " silent input, % of core per second:";
            for (auto v : perSecond) line << " " << juce::String (v, 3);
            std::cout << line.toStdString() << std::endl;
        }
    }
    return 0;
}
