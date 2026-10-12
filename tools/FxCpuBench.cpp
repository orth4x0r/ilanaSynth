// Dev tool (not shipped): what the FX rack costs. Builds the engine as the
// effect (ILANA_FX=1), feeds it a steady noisy-tone input and times
// processBlock per block. Reports median and p99 as a share of one core
// (block time / block duration). Used for docs/ROUTING-CPU.md.
//
//   ilanaFxCpuBench                 every scenario
//   ILANA_BENCH_ONLY=types|series|parallel|overhead|sleep|graph|patcher   one group
//                                   (graph: the patch graph of roadmap step A,
//                                   its own overhead and the rack run through it)
//   ILANA_BENCH_TYPELIST=12,13   effect types the types group measures
//   ILANA_BENCH_SLEEPTYPES=0,13  effect types the sleep group tests
//   ILANA_BENCH_RACK=Delay          only the graph group's racks whose name has this
//   ILANA_BENCH_BLOCKS=n            timed blocks per measurement (default 600)

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "PluginProcessor.h"
#include "processor/PatchGraphRunner.h"

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

// Two ways of doing the same work, timed block by block in turn (so drift
// on the box hits both alike). Microseconds per block: median and mean.
struct Pair { double medianA, meanA, medianB, meanB; };

template <typename A, typename B>
Pair timeInTurn (A&& a, B&& b, juce::AudioBuffer<float>& buffer, int warm = 200)
{
    std::vector<double> ta, tb;
    double phase = 0.0;
    const auto fill = [&]
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = 0.3f * (float) std::sin (phase);
            phase += 0.0287;
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }
    };
    for (int block = 0; block < warm + timedBlocks; ++block)
    {
        fill();
        auto t0 = std::chrono::steady_clock::now();
        a (buffer);
        auto t1 = std::chrono::steady_clock::now();
        fill();
        auto t2 = std::chrono::steady_clock::now();
        b (buffer);
        auto t3 = std::chrono::steady_clock::now();
        if (block >= warm)
        {
            ta.push_back (1.0e6 * std::chrono::duration<double> (t1 - t0).count());
            tb.push_back (1.0e6 * std::chrono::duration<double> (t3 - t2).count());
        }
    }
    const auto stats = [] (std::vector<double>& t, double& median, double& mean)
    {
        mean = 0.0;
        for (const auto v : t) mean += v;
        mean /= (double) t.size();
        std::sort (t.begin(), t.end());
        median = t[t.size() / 2];
    };
    Pair pair {};
    stats (ta, pair.medianA, pair.meanA);
    stats (tb, pair.medianB, pair.meanB);
    return pair;
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
                                    "AW Dynamics", "AW EQ", "AW Console", "AW Lo-Fi", "AW Stereo", "Filter" };

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

    // FILTER (type 42): its models at full resonance, mono and spread.
    if (want ("filter"))
    {
        std::cout << "\n== FILTER (type 42), reso 0.8, drive 2 (net = minus the empty rack)\n";
        for (const int model : { (int) FilterType::LowPass, (int) FilterType::LadderLow, (int) FilterType::MoogDrive,
                                 (int) FilterType::VowelBank, (int) FilterType::CombPlus, (int) FilterType::AwPear, (int) FilterType::Disperser })
            for (const float spread : { 0.0f, 1.0f })
            {
                quiet (p);
                p.assignFxSlot (1, 42);
                setParam (p, "fx_filter_type", (float) model);
                setParam (p, "fx_filter_reso", 0.8f);
                setParam (p, "fx_filter_drive", 2.0f);
                setParam (p, "fx_filter_spread", spread);
                row ("Filter " + FilterType::getNames()[model] + (spread > 0.0f ? " spread" : ""), measure (p), base);
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

    if (want ("graph"))
    {
        const auto core = 1.0e6 * blockSize / sampleRate; // microseconds in one block = 100 % of a core
        const auto percent = [core] (double us) { return juce::String (100.0 * us / core, 3) + " %"; };

        // 1. The graph's own cost: 16 nodes that each do Utility's work (a
        //    gain), against the same 16 gains in a plain loop.
        std::cout << "\n== patch graph overhead: 16 gain nodes against a plain loop of 16 gains (us per block of " << blockSize << ")\n";
        const auto gainNode = [] (const PatchGraph::CompiledNode&, PatchGraph::NodeBuffers& io, int n)
        {
            for (int c = 0; c < io.out[0]->getNumChannels(); ++c)
                juce::FloatVectorOperations::multiply (io.out[0]->getWritePointer (c), 0.999f, n);
        };
        for (const auto* shape : { "series", "parallel", "series + loop" })
        {
            PatchGraph::Graph graph;
            std::vector<PatchGraph::NodeId> ids;
            for (int n = 0; n < 16; ++n)
                ids.push_back (graph.addNode (19, 1, 1));
            const juce::String name (shape);
            if (name == "parallel")
            {
                for (const auto id : ids)
                {
                    graph.connect ({ PatchGraph::inputNodeId, 0 }, { id, 0 });
                    graph.connect ({ id, 0 }, { PatchGraph::outputNodeId, 0 }, 1.0f / 16.0f);
                }
            }
            else
            {
                auto previous = PatchGraph::inputNodeId;
                for (const auto id : ids)
                {
                    graph.connect ({ previous, 0 }, { id, 0 });
                    previous = id;
                }
                graph.connect ({ previous, 0 }, { PatchGraph::outputNodeId, 0 });
                if (name.contains ("loop"))
                    graph.connect ({ ids.back(), 0 }, { ids.front(), 0 }, 0.5f);
            }
            const auto plan = PatchGraph::compile (graph);
            for (const auto measureCpu : { false, true })
            {
                PatchGraph::Runner runner;
                runner.setSettings ({ measureCpu, false });
                runner.prepare (sampleRate, blockSize, 2);
                runner.publish (plan);
                juce::AudioBuffer<float> buffer (2, blockSize), scratch (2, blockSize);
                const auto parallel = name == "parallel";
                const auto pair = timeInTurn ([&] (juce::AudioBuffer<float>& b) { runner.process (b, gainNode); },
                                              [&] (juce::AudioBuffer<float>& b)
                                              {
                                                  // The same work by hand: in place, or copy, gain and sum.
                                                  if (! parallel)
                                                  {
                                                      for (int n = 0; n < 16; ++n)
                                                          for (int c = 0; c < 2; ++c)
                                                              juce::FloatVectorOperations::multiply (b.getWritePointer (c), 0.999f, blockSize);
                                                      return;
                                                  }
                                                  for (int n = 0; n < 16; ++n)
                                                      for (int c = 0; c < 2; ++c)
                                                      {
                                                          juce::FloatVectorOperations::copyWithMultiply (scratch.getWritePointer (c), b.getReadPointer (c), 0.999f, blockSize);
                                                          juce::FloatVectorOperations::addWithMultiply (b.getWritePointer (c), scratch.getReadPointer (c), 1.0f / 16.0f, blockSize);
                                                      }
                                              },
                                              buffer);
                std::cout << ("16 nodes " + name + (measureCpu ? ", per-node CPU readout on" : ", readout off")).paddedRight (' ', 52).toStdString()
                          << " graph " << juce::String (pair.meanA, 2).paddedLeft (' ', 7).toStdString() << " us (" << percent (pair.meanA).toStdString()
                          << ")   plain " << juce::String (pair.meanB, 2).paddedLeft (' ', 7).toStdString()
                          << " us   per node " << juce::String ((pair.meanA - pair.meanB) / 16.0, 3).toStdString() << " us"
                          << "   [" << plan.numBuffers << " buffers, " << plan.copyCount() << " copies, chunk " << (plan.chunkSize > 0 ? plan.chunkSize : blockSize) << "]"
                          << std::endl;
            }
        }

        // 2. The real rack: the fixed loop (processEffects) against the same
        //    rack run through its migrated graph (processEffectsGraph).
        std::cout << "\n== the rack, fixed loop against its migrated graph (us per block, mean; same processor, blocks in turn)\n";
        struct Rack { const char* name; std::vector<int> types; bool parallel; };
        const std::vector<Rack> racks { { "10 x Utility series", { 19, 19, 19, 19, 19, 19, 19, 19, 19, 19 }, false },
                                        { "10 x Utility parallel", { 19, 19, 19, 19, 19, 19, 19, 19, 19, 19 }, true },
                                        { "4 x Drive series", { 2, 2, 2, 2 }, false },
                                        { "8 x Delay parallel", { 9, 9, 9, 9, 9, 9, 9, 9 }, true },
                                        { "Drive, Chorus, Delay, Reverb series", { 2, 7, 9, 13 }, false },
                                        { "Drive, Chorus, Delay, Reverb parallel", { 2, 7, 9, 13 }, true } };
        const auto rackFilter = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_RACK", ""); // one rack by name
        for (const auto& rack : racks)
        {
            if (rackFilter.isNotEmpty() && ! juce::String (rack.name).containsIgnoreCase (rackFilter))
                continue;
            quiet (p);
            for (size_t s = 0; s < rack.types.size(); ++s)
                p.assignFxSlot ((int) s + 1, rack.types[s]);
            setParam (p, "fx_routing", rack.parallel ? 1.0f : 0.0f);
            p.prepareToPlay (sampleRate, blockSize);
            PatchGraph::Runner runner;
            runner.prepare (sampleRate, blockSize, 2);
            runner.publish (PatchGraph::compile (p.getFxRackGraph()));
            juce::AudioBuffer<float> buffer (2, blockSize);
            const auto pair = timeInTurn ([&] (juce::AudioBuffer<float>& b) { p.processEffects (b); },
                                          [&] (juce::AudioBuffer<float>& b) { p.processEffectsGraph (b, runner); }, buffer);
            std::cout << juce::String (rack.name).paddedRight (' ', 40).toStdString()
                      << " fixed " << juce::String (pair.meanA, 2).paddedLeft (' ', 7).toStdString() << " us (" << percent (pair.meanA).toStdString()
                      << ")   graph " << juce::String (pair.meanB, 2).paddedLeft (' ', 7).toStdString() << " us (" << percent (pair.meanB).toStdString()
                      << ")   difference " << juce::String (pair.meanB - pair.meanA, 2).toStdString() << " us" << std::endl;
        }
    }

    if (want ("patcher"))
    {
        const auto core = 1.0e6 * blockSize / sampleRate;
        const auto percent = [core] (double us) { return juce::String (100.0 * us / core, 3) + " %"; };
        // 3. Roadmap step 17: the same four effects with PATCHER off (the
        //    rack's own graph) against PATCHER on with the same series wiring,
        //    then with a Reverb -> Drive loop cable at 50 % (64-sample chunks).
        std::cout << "\n== the FX patcher: Drive, Chorus, Delay, Reverb (us per block, mean; same processor, blocks in turn)\n";
        for (const auto loop : { false, true })
        {
            quiet (p);
            const std::vector<int> types { 2, 7, 9, 13 };
            for (size_t s = 0; s < types.size(); ++s)
                p.assignFxSlot ((int) s + 1, types[s]);
            setParam (p, "fx_routing", 0.0f);
            p.prepareToPlay (sampleRate, blockSize);
            p.setFxPatcherOn (true);
            auto graph = p.getFxPatchGraph();
            if (loop)
            {
                PatchGraph::NodeId drive = 0, reverb = 0;
                for (const auto& node : graph.nodes())
                {
                    if (node.ref == 1) drive = node.id;
                    if (node.ref == 4) reverb = node.id;
                }
                graph.connect ({ reverb, 0 }, { drive, 0 }, 0.5f);
            }
            if (loop)
                p.setFxPatchGraph (graph);
            p.compileFxPlansNow();
            juce::AudioBuffer<float> buffer (2, blockSize);
            const auto pair = timeInTurn ([&] (juce::AudioBuffer<float>& b) { setParam (p, "fx_patcher", 0.0f); p.processEffects (b); },
                                          [&] (juce::AudioBuffer<float>& b) { setParam (p, "fx_patcher", 1.0f); p.processEffects (b); }, buffer);
            setParam (p, "fx_patcher", 0.0f);
            std::cout << juce::String (loop ? "patcher, series + Reverb -> Drive loop" : "patcher, same series wiring").paddedRight (' ', 40).toStdString()
                      << " rack " << juce::String (pair.meanA, 2).paddedLeft (' ', 7).toStdString() << " us (" << percent (pair.meanA).toStdString()
                      << ")   patcher " << juce::String (pair.meanB, 2).paddedLeft (' ', 7).toStdString() << " us (" << percent (pair.meanB).toStdString()
                      << ")   difference " << juce::String (pair.meanB - pair.meanA, 2).toStdString() << " us" << std::endl;
        }
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
