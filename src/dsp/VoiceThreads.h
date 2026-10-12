#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

// Renders the sounding voices of one sub-block on several cores. Each voice
// renders into its own buffer (on whichever thread takes it), then the
// audio thread adds the buffers to the output in voice order, so the sum,
// and the sound, is the same as rendering them one after another into the
// output: with stereo output a voice adds exactly one value per sample and
// channel, and adding its own buffer adds that same value, in the same order.
//
// Nothing on the audio thread allocates or locks: the threads and buffers
// are made in prepare (off the audio thread), a sub-block is handed over
// through atomics, the workers sleep on an atomic wait between sub-blocks
// (a futex / WaitOnAddress, no mutex), and the audio thread renders voices
// too, then spins only until the last voice a worker took is done.
class VoiceThreads
{
public:
    using RenderFunction = std::function<void (int voice, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)>;

    ~VoiceThreads() { stop(); }

    // Off the audio thread. numWorkers 0 stops the threads (one core).
    void prepare (int numWorkers, int maxVoices, int maxSamples, RenderFunction newRender)
    {
        stop();
        render = std::move (newRender);
        capacity = juce::jmax (1, maxSamples);
        scratch.setSize (2 * juce::jmax (1, maxVoices), capacity, false, true, true);
        // The channels' pointers, taken once: AudioBuffer::getWritePointer
        // also marks the buffer not clear, a write the threads would race on.
        channelPointers.assign (scratch.getArrayOfWritePointers(), scratch.getArrayOfWritePointers() + scratch.getNumChannels());
        jobVoices.assign ((size_t) juce::jmax (1, maxVoices), 0);
        quit.store (false);
        for (int i = 0; i < numWorkers; ++i)
            workers.emplace_back ([this] { workerLoop(); });
    }

    void stop()
    {
        quit.store (true);
        generation.fetch_add (1);
        generation.notify_all();
        for (auto& worker : workers)
            if (worker.joinable())
                worker.join();
        workers.clear();
    }

    int getNumWorkers() const noexcept { return (int) workers.size(); }

    // Whether a sub-block of this shape can go to the threads (otherwise the
    // caller renders the voices one after another as before).
    bool canRender (const juce::AudioBuffer<float>& output, int startSample, int numSamples, int numVoices) const noexcept
    {
        return ! workers.empty() && output.getNumChannels() == 2 && startSample + numSamples <= capacity
               && numVoices <= (int) jobVoices.size() && numVoices >= 2;
    }

    // Audio thread: render the given voices (indices, in voice order) and
    // add them to the output in that order.
    void renderAndSum (const int* voices, int numVoices, juce::AudioBuffer<float>& output, int startSample, int numSamples)
    {
        // A worker still finishing the last sub-block may take one more
        // ticket from it; those are all past its count. So every field is
        // written before the counter is reset, and the counter carries the
        // count it was set with (high half), so no ticket is ever judged by
        // another sub-block's count.
        std::copy (voices, voices + numVoices, jobVoices.begin());
        jobStart.store (startSample, std::memory_order_relaxed);
        jobLength.store (numSamples, std::memory_order_relaxed);
        done.store (0, std::memory_order_relaxed);
        next.store ((std::uint64_t) numVoices << 32, std::memory_order_release);
        generation.fetch_add (1, std::memory_order_release);
        generation.notify_all();

        runJobs();

        while (done.load (std::memory_order_acquire) < numVoices)
            juce::Thread::yield();

        for (int k = 0; k < numVoices; ++k)
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::add (output.getWritePointer (channel, startSample),
                                                  channelPointers[(size_t) (2 * k + channel)] + startSample, numSamples);
    }

private:
    void workerLoop()
    {
        // seen is read before quit: a stop() that lands before this thread
        // first runs (prepare then prepare again, quickly) is either seen
        // here or has moved generation past seen, so the wait returns;
        // waiting first could sleep through it and hang stop()'s join.
        auto seen = generation.load();
        while (! quit.load())
        {
            generation.wait (seen);
            seen = generation.load();
            if (quit.load())
                return;
            runJobs();
        }
    }

    void runJobs()
    {
        // The same rounding of tiny values as the audio thread.
        juce::ScopedNoDenormals noDenormals;
        while (true)
        {
            // A ticket under its count belongs to a sub-block that can't end
            // before this item is done, so its fields stay put meanwhile (and
            // the acquire orders them after the audio thread wrote them).
            const auto ticket = next.fetch_add (1, std::memory_order_acq_rel);
            const auto k = (int) (ticket & 0xffffffffu);
            if (k >= (int) (ticket >> 32))
                return;
            const auto start = jobStart.load (std::memory_order_relaxed);
            const auto length = jobLength.load (std::memory_order_relaxed);
            float* channels[2] { channelPointers[(size_t) (2 * k)], channelPointers[(size_t) (2 * k + 1)] };
            juce::AudioBuffer<float> view (channels, 2, capacity);
            juce::FloatVectorOperations::clear (channels[0] + start, length);
            juce::FloatVectorOperations::clear (channels[1] + start, length);
            render (jobVoices[(size_t) k], view, start, length);
            done.fetch_add (1, std::memory_order_acq_rel);
        }
    }

    RenderFunction render;
    juce::AudioBuffer<float> scratch;
    std::vector<float*> channelPointers;
    int capacity = 0;
    std::vector<int> jobVoices;
    std::atomic<int> jobStart { 0 }, jobLength { 0 };
    std::atomic<std::uint64_t> next { 0 };
    std::atomic<int> done { 0 };
    std::atomic<std::uint32_t> generation { 0 };
    std::atomic<bool> quit { false };
    std::vector<std::thread> workers;
};
