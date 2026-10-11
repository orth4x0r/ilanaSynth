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
// a cable that closes a loop gives back what it was given a while ago: its
// own delay (Cable::delayMs), one chunk at the least (a ring per loop, read
// before written). Every ring holds maxLoopDelayMs, so a new plan with the
// same loops takes the old rings over and a level or delay change doesn't
// cut the loop.
//
// The loop guard: what a ring takes passes a limiter with a ceiling of
// loopCeiling (0 dBFS), instant attack and a 200 ms release; the output of
// a graph with a loop passes another (the output guard), because an effect
// in a loop can still pile up what the loop feeds it (a Spring reverb at
// full size did, to +21 dB). A loop whose
// gain is over 1 (it happens easily: a Reverb that two branches feed twice)
// would otherwise grow until something clips, and stay there (ilana's
// Reverb, Smear and EQ patch, 2026-10-11, peaked at +24 dB and never
// stopped). Bounded, it holds at the ceiling while the loop stays patched
// and dies away like any loop once it's gone or turned down. The ring also
// zeroes NaN and flushes values under 1e-20, so a decaying loop doesn't sink
// into denormals even when the host has reset the CPU's flush-to-zero mode.
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

// Per loop, for the view: which cable (from and to, packed) and how hard the
// loop guard is holding it (1: not at all; under 1: the gain it applies).
struct LoopStats
{
    std::atomic<std::uint64_t> cable { 0 };
    std::atomic<float> guard { 1.0f };

    static std::uint64_t pack (Port from, Port to)
    {
        return ((std::uint64_t) from.node << 40) | ((std::uint64_t) (from.port & 0xff) << 32) | ((std::uint64_t) to.node << 8)
               | (std::uint64_t) (to.port & 0xff);
    }
};

class Runner
{
public:
    struct Settings
    {
        bool measureCpu = true;   // time each node (two clock reads per node per chunk)
        bool silenceSkip = false; // skip nodes whose input is silent and tail is spent (A5)
        bool loopGuards = true;   // the loop and output guards (off: a ring only clamps at +-8, for the exactness tests)
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
        const auto shortest = plan.chunkSize > 0 ? plan.chunkSize : 1;
        program->ringLength = plan.hasFeedback() ? shortest + (int) std::ceil (maxLoopDelayMs * 0.001 * sampleRate) : 1;
        for (auto& ring : program->rings)
            ring.setSize (channels, program->ringLength, false, true, false);
        program->loopDelay.resize (plan.loops.size());
        for (size_t r = 0; r < plan.loops.size(); ++r)
            program->loopDelay[r] = juce::jlimit (shortest, program->ringLength,
                                                  (int) std::lround ((double) plan.loops[r].delayMs * 0.001 * sampleRate));
        program->loopGuards.assign (plan.loops.size(), {});
        program->loopLeast.assign (plan.loops.size(), 1.0f);
        program->release = (float) std::exp (-(double) guardStep / (0.2 * juce::jmax (1.0, sampleRate)));
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

    // The loop guard's gain on the loop that `from` -> `to` closes (1 when
    // it isn't holding it, or there is no such loop). Any thread.
    float loopGuard (Port from, Port to) const
    {
        const auto key = LoopStats::pack (from, to);
        for (const auto& l : loopStats)
            if (l.cable.load (std::memory_order_relaxed) == key)
                return l.guard.load (std::memory_order_relaxed);
        return 1.0f;
    }

    // The output guard's gain (1 when idle); only a graph with a loop has one.
    float outputGuard() const { return outputGuardGain.load (std::memory_order_relaxed); }

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

        // The output guard (see the top): a graph with a loop never leaves
        // more than loopCeiling.
        if (program.plan.hasFeedback() && settings.loopGuards)
        {
            float* channelsOut[maxChannels] {};
            const auto count = juce::jmin (numChannels, maxChannels);
            for (int c = 0; c < count; ++c)
                channelsOut[c] = buffer.getWritePointer (c);
            const auto least = guardBlock (channelsOut, count, 0, numSamples, program.outputGuard, program.release);
            outputGuardGain.store (least, std::memory_order_relaxed);
            for (size_t r = 0; r < program.loopLeast.size() && r < loopStats.size(); ++r)
            {
                loopStats[r].guard.store (program.loopLeast[r], std::memory_order_relaxed);
                program.loopLeast[r] = 1.0f;
            }
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
        std::fill (active->loopGuards.begin(), active->loopGuards.end(), Guard {});
        active->outputGuard = {};
        active->ringPosition = 0;
        std::fill (active->sinceInput.begin(), active->sinceInput.end(), 0);
    }

    static constexpr float silence = 3.0e-6f; // -110 dB, the rack's sleep level
    static constexpr float loopCeiling = 1.0f;  // the loop guard's (see the top)
    static constexpr int maxChannels = 8;       // a loop carries this many
    static constexpr int guardStep = 16;        // the guards set their gain this often (samples)

    struct Guard
    {
        float envelope = 0.0f;
        float gain = 1.0f;
    };

private:
    struct Program
    {
        Plan plan;
        int chunk = 0;
        std::vector<juce::AudioBuffer<float>> buffers; // [0] unused: the host's
        std::vector<juce::AudioBuffer<float>> rings;
        int ringLength = 1;
        int ringPosition = 0;
        std::vector<int> loopDelay;       // per ring, in samples (chunk..ringLength)
        std::vector<Guard> loopGuards;    // per ring
        std::vector<float> loopLeast;     // per ring, its least gain this block (for loopStats)
        Guard outputGuard;
        float release = 0.0f;             // the guards' envelope, per guardStep samples
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
        takeOverLoops (*next, active);
        retired.store (active, std::memory_order_release);
        active = next;
        for (size_t n = 0; n < stats.size(); ++n)
        {
            stats[n].id.store (n < active->plan.nodes.size() ? active->plan.nodes[n].id : 0, std::memory_order_relaxed);
            stats[n].cpu.store (0.0f, std::memory_order_relaxed);
            stats[n].sleeping.store (false, std::memory_order_relaxed);
        }
        for (size_t r = 0; r < loopStats.size(); ++r)
        {
            const auto* loop = r < active->plan.loops.size() ? &active->plan.loops[r] : nullptr;
            loopStats[r].cable.store (loop != nullptr ? LoopStats::pack (loop->from, loop->to) : 0, std::memory_order_relaxed);
            loopStats[r].guard.store (1.0f, std::memory_order_relaxed);
        }
        outputGuardGain.store (1.0f, std::memory_order_relaxed);
    }

    // The guards' limiter on samples [start, start + length) of `count`
    // channels, in place. Most blocks are under the ceiling and finite: then
    // nothing changes but a block gone quiet (under 1e-15) is zeroed, so a
    // dying loop never sinks into denormals. (Below the ceiling the
    // envelope's exact value doesn't matter: a sample over it sets it.)
    // Otherwise the gain is worked out every guardStep samples from their
    // peak and glides there, and a clip catches what the glide lets over
    // the ceiling; NaN becomes 0. Returns the least gain it applied.
    static float guardBlock (float* const* channels, int count, int start, int length, Guard& guard, float release)
    {
        auto poison = 0.0f;
        const auto peak = peakOf (channels, count, start, length, poison);
        if (poison == 0.0f && peak <= loopCeiling && guard.envelope <= loopCeiling && guard.gain == 1.0f)
        {
            guard.envelope = peak;
            if (peak < 1.0e-15f)
                for (int c = 0; c < count; ++c)
                    juce::FloatVectorOperations::clear (channels[c] + start, length);
            return 1.0f;
        }

        auto least = 1.0f;
        for (int from = start; from < start + length; from += guardStep)
        {
            const auto n = juce::jmin (guardStep, start + length - from);
            auto bad = 0.0f;
            const auto stepPeak = peakOf (channels, count, from, n, bad);
            guard.envelope = juce::jmax (stepPeak, guard.envelope * release);
            const auto target = guard.envelope > loopCeiling ? loopCeiling / guard.envelope : 1.0f;
            for (int c = 0; c < count; ++c)
            {
                auto* x = channels[c] + from;
                if (bad != 0.0f)
                    for (int i = 0; i < n; ++i)
                        x[i] = std::isfinite (x[i]) ? x[i] : (x[i] == x[i] ? std::copysign (loopCeiling, x[i]) : 0.0f);
                auto gain = guard.gain;
                const auto step = (target - guard.gain) / (float) n;
                for (int i = 0; i < n; ++i)
                {
                    gain += step;
                    x[i] *= gain;
                }
                juce::FloatVectorOperations::clip (x, x, -loopCeiling, loopCeiling, n);
            }
            guard.gain = target;
            least = juce::jmin (least, target);
        }
        return least;
    }

    // The largest magnitude among finite samples; `poison` turns NaN if any
    // is NaN or infinite. (Eight lanes, so the compiler can vectorise
    // without reordering sums.)
    static float peakOf (const float* const* channels, int count, int start, int length, float& poison)
    {
        float peaks[8] {}, poisons[8] {};
        for (int c = 0; c < count; ++c)
        {
            const auto* x = channels[c] + start;
            int i = 0;
            for (; i + 8 <= length; i += 8)
                for (int j = 0; j < 8; ++j)
                {
                    const auto magnitude = std::abs (x[i + j]);
                    peaks[j] = peaks[j] < magnitude ? magnitude : peaks[j];
                    poisons[j] += x[i + j] * 0.0f;
                }
            for (; i < length; ++i)
            {
                peaks[0] = juce::jmax (peaks[0], std::abs (x[i]));
                poisons[0] += x[i] * 0.0f;
            }
        }
        auto peak = 0.0f;
        for (int j = 0; j < 8; ++j)
        {
            peak = juce::jmax (peak, peaks[j]);
            poison += poisons[j];
        }
        return std::isfinite (peak) ? peak : 0.0f;
    }

    // Audio thread: a new program keeps what the old one's loops still hold,
    // for every loop it has too (the same cable), so editing a patch with a
    // loop in it doesn't cut the loop. Swapping buffers allocates nothing.
    static void takeOverLoops (Program& next, Program* old)
    {
        if (old == nullptr || ! old->plan.hasFeedback() || ! next.plan.hasFeedback() || old->ringLength != next.ringLength)
            return;
        auto any = false;
        for (size_t r = 0; r < next.plan.loops.size(); ++r)
            for (size_t o = 0; o < old->plan.loops.size(); ++o)
            {
                const auto& a = next.plan.loops[r];
                const auto& b = old->plan.loops[o];
                if (a.from == b.from && a.to == b.to && next.rings[r].getNumChannels() == old->rings[o].getNumChannels())
                {
                    std::swap (next.rings[r], old->rings[o]);
                    next.loopGuards[r] = old->loopGuards[o];
                    next.outputGuard = old->outputGuard;
                    any = true;
                    break;
                }
            }
        if (any)
            next.ringPosition = old->ringPosition;
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
                    const auto start = (program.ringPosition - program.loopDelay[(size_t) op.ring] + program.ringLength) % program.ringLength;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto* dst = views[op.dst].getWritePointer (c);
                        const auto* src = ring.getReadPointer (c);
                        auto pos = start;
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
                    // The loop guard (see the top): the chunk into the ring,
                    // then the guard on it there (one or two runs: it may wrap).
                    auto& ring = program.rings[(size_t) op.ring];
                    const auto count = juce::jmin (numChannels, maxChannels);
                    float* dst[maxChannels] {};
                    const auto first = juce::jmin (length, program.ringLength - program.ringPosition);
                    for (int c = 0; c < count; ++c)
                    {
                        dst[c] = ring.getWritePointer (c);
                        const auto* src = views[op.src].getReadPointer (c);
                        juce::FloatVectorOperations::copy (dst[c] + program.ringPosition, src, first);
                        if (first < length)
                            juce::FloatVectorOperations::copy (dst[c], src + first, length - first);
                    }
                    if (! settings.loopGuards)
                    {
                        for (int c = 0; c < count; ++c)
                        {
                            auto* x = dst[c];
                            for (int i = 0, pos = program.ringPosition; i < length; ++i, pos = pos + 1 == program.ringLength ? 0 : pos + 1)
                            {
                                const auto v = std::abs (x[pos]) < 1.0e-20f ? 0.0f : juce::jlimit (-8.0f, 8.0f, x[pos]);
                                x[pos] = v == v ? v : 0.0f;
                            }
                        }
                        break;
                    }
                    auto& guard = program.loopGuards[(size_t) op.ring];
                    auto& least = program.loopLeast[(size_t) op.ring];
                    least = juce::jmin (least, guardBlock (dst, count, program.ringPosition, first, guard, program.release));
                    if (first < length)
                        least = juce::jmin (least, guardBlock (dst, count, 0, length - first, guard, program.release));
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
    std::array<LoopStats, maxFeedbackCables> loopStats;
    std::atomic<float> outputGuardGain { 1.0f };
    std::atomic<float> graphCpu { 0.0f };
};
} // namespace PatchGraph
