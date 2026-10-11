#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

#include "PatchGraph.h"

// Runs a compiled patch graph (PatchGraph.h) on the audio thread
// (docs/ROUTING-CPU.md, rules A1, A3, A5, A9, A11, A12).
//
//   Message thread: build a Plan with PatchGraph::compile, hand it to
//   publish(). The runner wraps it in a Program that owns every scratch
//   buffer and feedback ring, sized for the largest block, so the audio
//   thread never allocates. Call collectGarbage() now and then (publish does
//   too) to free programs the audio thread has let go of.
//
//   Audio thread: process(buffer, callback). It picks up a newly published
//   program with one atomic exchange, then walks the op list. Node work is
//   the caller's: callback (const CompiledNode&, NodeBuffers&, numSamples).
//   An in-place node finds its one buffer in both in[0] and out[0].
//
// A graph with a feedback cable renders in chunks of plan.chunkSize samples;
// a cable that closes a loop gives back what it was given one chunk ago
// (a ring per loop, read before written). The ring flushes values under
// 1e-20 to zero and clamps at +-8, so a runaway loop can't fill the output
// with infinities and a decaying loop doesn't sink into denormals even when
// the host has reset the CPU's flush-to-zero mode.
namespace PatchGraph
{
struct NodeBuffers
{
    juce::AudioBuffer<float>* in[maxPorts] {};
    juce::AudioBuffer<float>* out[maxPorts] {};
    int numInputs = 0;
    int numOutputs = 0;
};

// Per running node, for the view: its id, the CPU share of its own work
// (smoothed like the rack's fxSlotCpu) and whether the silence skip has put it
// to sleep. Read from any thread.
struct NodeStats
{
    std::atomic<NodeId> id { 0 };
    std::atomic<float> cpu { 0.0f };
    std::atomic<bool> sleeping { false };
};

class Runner
{
public:
    struct Settings
    {
        bool measureCpu = true;   // time each node (two clock reads per node per chunk)
        bool silenceSkip = false; // skip nodes whose input is silent and tail is spent (A5)
    };

    // Optional, for silenceSkip: how many samples a node of this kind keeps
    // sounding after its input stops. Negative: it may sound on its own
    // (never skipped). Called on the message thread when a plan is published.
    using TailFunction = int (*) (const CompiledNode&, double sampleRate);

    Runner() = default;

    ~Runner()
    {
        delete pending.exchange (nullptr);
        delete retired.exchange (nullptr);
        delete active;
    }

    // Message thread. Changing the size or rate rebuilds the current program
    // with new buffers (and fresh rings), the next process() picks it up.
    void prepare (double newSampleRate, int maxBlockSize, int numChannels)
    {
        sampleRate = newSampleRate;
        maxBlock = juce::jmax (1, maxBlockSize);
        channels = juce::jmax (1, numChannels);
        if (lastPlan != nullptr)
        {
            const auto plan = *lastPlan; // (publish replaces lastPlan)
            publish (plan);
        }
    }

    void setSettings (Settings newSettings) { settings = newSettings; }
    void setTailFunction (TailFunction function) { tailOf = function; }

    // Message thread: makes `plan` the one the audio thread runs from its
    // next block on.
    void publish (const Plan& plan)
    {
        collectGarbage();
        lastPlan = std::make_unique<Plan> (plan);
        auto program = std::make_unique<Program>();
        program->plan = plan;
        const auto chunk = plan.chunkSize > 0 ? juce::jmin (plan.chunkSize, maxBlock) : maxBlock;
        program->chunk = chunk;
        program->buffers.resize ((size_t) juce::jmax (1, plan.numBuffers));
        for (size_t b = 1; b < program->buffers.size(); ++b)
            program->buffers[b].setSize (channels, chunk, false, true, false);
        program->rings.resize (plan.loops.size());
        for (auto& ring : program->rings)
            ring.setSize (channels, plan.chunkSize > 0 ? plan.chunkSize : 1, false, true, false);
        program->ringLength = plan.chunkSize > 0 ? plan.chunkSize : 1;
        program->tails.resize (plan.nodes.size(), -1);
        program->sinceInput.resize (plan.nodes.size(), 0);
        if (tailOf != nullptr)
            for (size_t n = 0; n < plan.nodes.size(); ++n)
                program->tails[n] = tailOf (plan.nodes[n], sampleRate);

        delete pending.exchange (program.release());
    }

    // Message thread: frees what the audio thread has finished with.
    void collectGarbage()
    {
        delete retired.exchange (nullptr);
    }

    // The plan the message thread last published (not necessarily running yet).
    const Plan* publishedPlan() const { return lastPlan.get(); }

    const NodeStats* statsFor (NodeId id) const
    {
        for (const auto& s : stats)
            if (s.id.load (std::memory_order_relaxed) == id)
                return &s;
        return nullptr;
    }

    float totalCpu() const { return graphCpu.load (std::memory_order_relaxed); }

    // Audio thread: takes up a newly published program, then the plan that
    // process() will run, or null before the first one.
    const Plan* activePlan()
    {
        adoptPending();
        return active != nullptr ? &active->plan : nullptr;
    }

    // Audio thread. Leaves the graph's output in `buffer`. With no program
    // yet the buffer passes through untouched.
    template <typename Callback>
    void process (juce::AudioBuffer<float>& buffer, Callback&& callback)
    {
        adoptPending();
        if (active == nullptr || ! active->plan.ok)
            return;

        auto& program = *active;
        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = juce::jmin (buffer.getNumChannels(), channels);
        const auto chunk = program.chunk;
        const auto startTicks = settings.measureCpu ? juce::Time::getHighResolutionTicks() : 0;

        for (int start = 0; start < numSamples; start += chunk)
        {
            const auto length = juce::jmin (chunk, numSamples - start);
            program.host.setDataToReferTo (buffer.getArrayOfWritePointers(), numChannels, start, length);
            runChunk (program, length, numChannels, callback);
        }

        if (settings.measureCpu)
        {
            const auto elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
            const auto available = (double) numSamples / juce::jmax (1.0, sampleRate);
            const auto usage = (float) juce::jlimit (0.0, 1.0, elapsed / juce::jmax (1.0e-9, available));
            graphCpu.store (graphCpu.load (std::memory_order_relaxed) * 0.9f + usage * 0.1f, std::memory_order_relaxed);
        }
    }

    // Audio thread: clears every ring and tail counter (a patch load).
    void reset()
    {
        adoptPending();
        if (active == nullptr)
            return;
        for (auto& ring : active->rings)
            ring.clear();
        active->ringPosition = 0;
        std::fill (active->sinceInput.begin(), active->sinceInput.end(), 0);
    }

    static constexpr float silence = 3.0e-6f; // -110 dB, the rack's sleep level

private:
    struct Program
    {
        Plan plan;
        int chunk = 0;
        std::vector<juce::AudioBuffer<float>> buffers; // [0] unused: the host's
        std::vector<juce::AudioBuffer<float>> rings;
        int ringLength = 1;
        int ringPosition = 0;
        std::vector<int> tails;
        std::vector<int> sinceInput;
        juce::AudioBuffer<float> host; // refers to the host block, never owns
        juce::AudioBuffer<float> views[maxBuffers]; // chunk-length views of the buffers
    };

    void adoptPending()
    {
        if (pending.load (std::memory_order_acquire) == nullptr)
            return;
        // Only swap once the message thread has collected the last one we
        // retired: the audio thread never frees.
        if (retired.load (std::memory_order_acquire) != nullptr)
            return;
        auto* next = pending.exchange (nullptr, std::memory_order_acq_rel);
        if (next == nullptr)
            return;
        retired.store (active, std::memory_order_release);
        active = next;
        for (size_t n = 0; n < stats.size(); ++n)
        {
            stats[n].id.store (n < active->plan.nodes.size() ? active->plan.nodes[n].id : 0, std::memory_order_relaxed);
            stats[n].cpu.store (0.0f, std::memory_order_relaxed);
            stats[n].sleeping.store (false, std::memory_order_relaxed);
        }
    }

    static float peakOf (const juce::AudioBuffer<float>& b, int numChannels, int length)
    {
        auto peak = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (b.getReadPointer (c), length);
            peak = juce::jmax (peak, -range.getStart(), range.getEnd());
        }
        return peak;
    }

    template <typename Callback>
    void runChunk (Program& program, int length, int numChannels, Callback& callback)
    {
        auto& views = program.views;
        const auto& plan = program.plan;
        views[0].setDataToReferTo (program.host.getArrayOfWritePointers(), numChannels, 0, length);
        for (int b = 1; b < plan.numBuffers; ++b)
            views[b].setDataToReferTo (program.buffers[(size_t) b].getArrayOfWritePointers(), numChannels, 0, length);

        for (const auto& op : plan.ops)
        {
            switch (op.type)
            {
                case Op::clear:
                    for (int c = 0; c < numChannels; ++c)
                        juce::FloatVectorOperations::clear (views[op.dst].getWritePointer (c), length);
                    break;

                case Op::copy:
                    for (int c = 0; c < numChannels; ++c)
                    {
                        if (op.gain == 1.0f)
                            juce::FloatVectorOperations::copy (views[op.dst].getWritePointer (c), views[op.src].getReadPointer (c), length);
                        else
                            juce::FloatVectorOperations::copyWithMultiply (views[op.dst].getWritePointer (c), views[op.src].getReadPointer (c), op.gain, length);
                    }
                    break;

                case Op::mix:
                    for (int c = 0; c < numChannels; ++c)
                    {
                        if (op.gain == 1.0f)
                            juce::FloatVectorOperations::add (views[op.dst].getWritePointer (c), views[op.src].getReadPointer (c), length);
                        else
                            juce::FloatVectorOperations::addWithMultiply (views[op.dst].getWritePointer (c), views[op.src].getReadPointer (c), op.gain, length);
                    }
                    break;

                case Op::feedbackRead:
                {
                    const auto& ring = program.rings[(size_t) op.ring];
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto* dst = views[op.dst].getWritePointer (c);
                        const auto* src = ring.getReadPointer (c);
                        auto pos = program.ringPosition;
                        for (int i = 0; i < length; ++i)
                        {
                            dst[i] = src[pos];
                            if (++pos == program.ringLength)
                                pos = 0;
                        }
                    }
                    break;
                }

                case Op::feedbackWrite:
                {
                    auto& ring = program.rings[(size_t) op.ring];
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto* dst = ring.getWritePointer (c);
                        const auto* src = views[op.src].getReadPointer (c);
                        auto pos = program.ringPosition;
                        for (int i = 0; i < length; ++i)
                        {
                            auto x = src[i];
                            x = std::abs (x) < 1.0e-20f ? 0.0f : juce::jlimit (-8.0f, 8.0f, x);
                            dst[pos] = x == x ? x : 0.0f; // NaN -> 0
                            if (++pos == program.ringLength)
                                pos = 0;
                        }
                    }
                    break;
                }

                case Op::process:
                    runNode (program, op.node, length, numChannels, callback);
                    break;
            }
        }

        if (plan.hasFeedback())
            program.ringPosition = (program.ringPosition + length) % program.ringLength;
    }

    template <typename Callback>
    void runNode (Program& program, int index, int length, int numChannels, Callback& callback)
    {
        const auto& node = program.plan.nodes[(size_t) index];
        NodeBuffers io;
        io.numInputs = node.numInputs;
        io.numOutputs = node.numOutputs;
        for (int p = 0; p < node.numInputs; ++p)
            io.in[p] = &program.views[node.inputs[p]];
        for (int p = 0; p < node.numOutputs; ++p)
            io.out[p] = &program.views[node.outputs[p]];

        auto* stat = index < (int) stats.size() ? &stats[(size_t) index] : nullptr;

        if (settings.silenceSkip && program.tails[(size_t) index] >= 0)
        {
            auto peak = 0.0f;
            for (int p = 0; p < node.numInputs; ++p)
                peak = juce::jmax (peak, peakOf (*io.in[p], numChannels, length));
            auto& since = program.sinceInput[(size_t) index];
            since = peak < silence ? since + length : 0;
            if (since > program.tails[(size_t) index] + length)
            {
                // Silent in, tail spent: the node's output is silence.
                for (int p = 0; p < node.numOutputs; ++p)
                    for (int c = 0; c < numChannels; ++c)
                        juce::FloatVectorOperations::clear (io.out[p]->getWritePointer (c), length);
                if (stat != nullptr)
                {
                    stat->sleeping.store (true, std::memory_order_relaxed);
                    stat->cpu.store (stat->cpu.load (std::memory_order_relaxed) * 0.9f, std::memory_order_relaxed);
                }
                return;
            }
            if (stat != nullptr)
                stat->sleeping.store (false, std::memory_order_relaxed);
        }

        if (! settings.measureCpu || stat == nullptr)
        {
            callback (node, io, length);
            return;
        }

        const auto start = juce::Time::getHighResolutionTicks();
        callback (node, io, length);
        const auto elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        const auto available = (double) length / juce::jmax (1.0, sampleRate);
        const auto usage = (float) juce::jlimit (0.0, 1.0, elapsed / juce::jmax (1.0e-9, available));
        stat->cpu.store (stat->cpu.load (std::memory_order_relaxed) * 0.9f + usage * 0.1f, std::memory_order_relaxed);
    }

    Settings settings;
    TailFunction tailOf = nullptr;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int channels = 2;
    std::unique_ptr<Plan> lastPlan;

    std::atomic<Program*> pending { nullptr };
    std::atomic<Program*> retired { nullptr };
    Program* active = nullptr; // audio thread only

    std::array<NodeStats, maxNodes> stats;
    std::atomic<float> graphCpu { 0.0f };
};
} // namespace PatchGraph
