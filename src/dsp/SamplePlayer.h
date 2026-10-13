#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "SampleRange.h"
#include "SampleTempo.h"

#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <vector>

struct SampleData;

// Float audio files can carry NaN, Inf or absurd values; a NaN would latch in
// the filters. Non-finite samples become 0 and the rest is held to +-16.
inline void scrubAudio (juce::AudioBuffer<float>& buffer) noexcept
{
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* p = buffer.getWritePointer (c);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            p[i] = std::isfinite (p[i]) ? juce::jlimit (-16.0f, 16.0f, p[i]) : 0.0f;
    }
}

// One region of a multisample (a SoundFont or SFZ instrument): the audio a
// range of keys and velocities plays, the key it was recorded at and its
// loop.
struct SampleZone
{
    std::shared_ptr<SampleData> data;
    int loKey = 0, hiKey = 127, loVel = 1, hiVel = 127;
    double rootNote = 60.0; // fractional: tuning folded in
    float gain = 1.0f;
    bool loop = false;
    int loopStart = 0, loopEnd = 0; // samples into data

    bool covers (int note, int velocity) const noexcept
    {
        return note >= loKey && note <= hiKey && velocity >= loVel && velocity <= hiVel;
    }

    double rootHz() const noexcept { return 440.0 * std::exp2 ((rootNote - 69.0) / 12.0); }
};

// The pitch a plain (zoneless) sample was recorded at, from the ROOT parameter.
// 60 gives the literal C3 frequency every older patch used, bit for bit.
inline double plainRootHz (float rootNote) noexcept
{
    return rootNote == 60.0f ? 261.6255653005986 : 440.0 * std::exp2 (((double) rootNote - 69.0) / 12.0);
}

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;
    // A multisample's regions. Empty for a plain sample; otherwise buffer
    // holds the region under middle C, for the displays.
    std::vector<SampleZone> zones;

    int getNumChannels() const noexcept { return buffer.getNumChannels(); }
    int getNumSamples() const noexcept { return buffer.getNumSamples(); }

    // The region a note plays: the one that covers it, else the nearest by
    // key (a velocity outside every range takes the closest layer).
    const SampleZone* zoneFor (int note, int velocity) const noexcept
    {
        const SampleZone* best = nullptr;
        auto bestDistance = 1 << 30;
        for (const auto& zone : zones)
        {
            if (zone.data == nullptr)
                continue;
            if (zone.covers (note, velocity))
                return &zone;
            const auto keyDistance = note < zone.loKey ? zone.loKey - note : (note > zone.hiKey ? note - zone.hiKey : 0);
            const auto velDistance = velocity < zone.loVel ? zone.loVel - velocity : (velocity > zone.hiVel ? velocity - zone.hiVel : 0);
            const auto distance = keyDistance * 128 + velDistance;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &zone;
            }
        }
        return best;
    }
};

class SamplePlayer
{
public:
    struct Params
    {
        const SampleData* sample = nullptr;
        bool loop = false;
        bool reverse = false;
        float start = 0.0f;
        float end = 1.0f;
        float fadeIn = 0.0f;
        float fadeOut = 0.0f;
        float gain = 1.0f;
        // A multisample region's own loop (samples); used when loopEnd > loopStart.
        int loopStart = 0, loopEnd = 0;
        // Roadmap step 27: with LOOP on, the loop is its own span (fractions of
        // the sample, held inside START..END): the note plays from START once
        // and then repeats loopFrom..loopTo, never going back to START. Off,
        // as before: LOOP wraps the whole played range.
        bool ownLoop = false;
        float loopFrom = 0.25f, loopTo = 0.75f;
        // Roadmap step 40: STRETCH keeps the pitch apart from the speed. The
        // read position moves at the time rate and short overlapping grains
        // read it at the pitch rate (setRates). Off, as before: one read at
        // one rate (setPlaybackRatio).
        bool stretch = false;
        // The same sound swapped for a rendered copy of it (or back): a
        // playing note carries on from the same place in it, with a short
        // crossfade, rather than starting again.
        bool keepPosition = false;
    };

    void prepare (double newSampleRate)
    {
        grainLength = juce::jmax (64, 2 * juce::roundToInt (0.023 * newSampleRate));
        window.resize ((size_t) grainLength);
        // A periodic Hann: two grains half a length apart sum to exactly one.
        for (int i = 0; i < grainLength; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / grainLength));
    }

    void reset()
    {
        active = false;
        tail.remaining = 0;
        position = regionStart;
    }

    void trigger()
    {
        if (params.sample == nullptr || params.sample->getNumSamples() <= 1)
        {
            active = false;
            return;
        }

        position = params.reverse ? regionEnd - 1.0 : regionStart;
        active = true;
        tail.remaining = 0;

        if (params.stretch)
            startGrains();
    }

    void setParams (const Params& newParams)
    {
        const auto newSample = newParams.sample != params.sample;
        const auto newStretch = newParams.stretch && ! params.stretch;
        const auto* oldSample = params.sample;
        // The old length was kept, not read through oldSample: an idle voice's
        // pointer may belong to a copy the stretch pool has since freed.
        const auto oldLength = lastLength;
        params = newParams;
        lastLength = params.sample != nullptr ? params.sample->getNumSamples() : 0;

        if (params.sample == nullptr || params.sample->getNumSamples() <= 1)
        {
            active = false;
            return;
        }

        const auto length = (double) params.sample->getNumSamples();
        regionStart = juce::jlimit (0.0, length - 1.0, (double) juce::jlimit (0.0f, 0.999f, params.start) * length);
        regionEnd = juce::jlimit (regionStart + 1.0, length, (double) juce::jlimit (0.0f, 1.0f, params.end) * length);
        regionLength = regionEnd - regionStart;

        ownLoopOn = false;
        if (params.ownLoop && params.loop)
        {
            const auto resolved = SampleRange::resolveLoop (regionStart, regionEnd, (double) params.loopFrom * length,
                                                            (double) params.loopTo * length, minLoopLength);
            ownLoopOn = resolved.valid;
            ownLoopStart = resolved.from;
            ownLoopEnd = resolved.to;
        }

        if (newSample && params.keepPosition && oldSample != nullptr && oldLength > 1)
        {
            if (active)
            {
                if (params.stretch)
                {
                    tail = { oldSample, grains, tailLength() };
                    position *= (double) params.sample->getNumSamples() / (double) oldLength;
                    startGrains();
                }
                else
                {
                    position *= (double) params.sample->getNumSamples() / (double) oldLength;
                }
            }
        }
        else if (newSample)
            trigger();
        else if (newStretch && active)
            startGrains();
    }

    void setPlaybackRatio (double newRatio)
    {
        ratio = juce::jlimit (0.0001, 64.0, std::abs (newRatio));
    }

    // STRETCH: the read position moves at timeRate source samples per output
    // sample, the grains read at pitchRate. (Each includes the sample rates' ratio.)
    void setRates (double timeRate, double pitchRate)
    {
        ratio = juce::jlimit (0.0001, 64.0, std::abs (timeRate));
        pitch = juce::jlimit (0.0001, 64.0, std::abs (pitchRate));
    }

    // The grains' length in output samples (about 46 ms); set by prepare.
    int getGrainLength() const noexcept { return grainLength; }

    bool isActive() const noexcept { return active; }

    double getSourcePosition() const noexcept { return position; }

    void process (float& left, float& right)
    {
        left = 0.0f;
        right = 0.0f;

        if (! active || params.sample == nullptr)
            return;

        // The loop's own span (roadmap step 27): the first pass through the
        // attack is not repeated. Reversed, the note runs the other way and
        // wraps when it falls below the loop's start.
        if (ownLoopOn)
        {
            const auto length = ownLoopEnd - ownLoopStart;
            if (params.reverse)
            {
                if (position < ownLoopStart)
                    position = ownLoopEnd - std::fmod (ownLoopStart - position, length);
            }
            else if (position >= ownLoopEnd)
                position = ownLoopStart + std::fmod (position - ownLoopStart, length);
        }
        // A region's sustain loop: forward only, inside the played range.
        else if (params.loopEnd > params.loopStart && ! params.reverse && position >= (double) params.loopEnd
            && params.loopEnd <= regionEnd && params.loopStart >= regionStart)
            position -= (double) (params.loopEnd - params.loopStart);

        if (position >= regionEnd || position < regionStart)
        {
            if (! params.loop)
            {
                // START modulated above the playhead moves it up; only the
                // end (or a reverse note's start) finishes a one-shot.
                if (! params.reverse && position < regionStart)
                    position = regionStart;
                else
                {
                    active = false;
                    return;
                }
            }
            else if (params.reverse)
            {
                while (position < regionStart)
                    position += regionLength;
            }
            else
            {
                while (position >= regionEnd)
                    position -= regionLength;
            }
        }

        if (params.stretch)
        {
            processStretched (left, right);
            return;
        }

        const auto* sample = params.sample;
        const auto numSamples = sample->getNumSamples();
        const auto index = juce::jlimit (0, numSamples - 1, (int) std::floor (position));
        const auto next = juce::jmin (index + 1, numSamples - 1);
        const auto frac = (float) (position - (double) index);

        const auto readChannel = [&] (int channel)
        {
            const auto* data = sample->buffer.getReadPointer (juce::jmin (channel, sample->getNumChannels() - 1));
            return data[index] + (data[next] - data[index]) * frac;
        };

        const auto readL = readChannel (0);
        const auto readR = sample->getNumChannels() > 1 ? readChannel (1) : readL;

        const auto posSinceStart = params.reverse ? (regionEnd - position) : (position - regionStart);
        const auto posToEnd = params.reverse ? (position - regionStart) : (regionEnd - position);

        auto gain = 1.0;

        const auto fadeInLength = (double) params.fadeIn * regionLength;

        if (fadeInLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posSinceStart / fadeInLength);

        const auto fadeOutLength = (double) params.fadeOut * regionLength;

        if (fadeOutLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posToEnd / fadeOutLength);

        gain *= (double) params.gain;
        left = readL * (float) gain;
        right = readR * (float) gain;

        position += params.reverse ? -ratio : ratio;
    }

private:
    struct Grain
    {
        double read = 0.0; // source position
        int age = 0;       // output samples since it started
        bool on = false;
    };

    // Both channels' sum at a whole sample, for the grain search.
    static float monoAt (const SampleData& sample, int index) noexcept
    {
        if (index < 0 || index >= sample.getNumSamples())
            return 0.0f;
        auto sum = sample.buffer.getSample (0, index);
        if (sample.getNumChannels() > 1)
            sum += sample.buffer.getSample (1, index);
        return sum;
    }

    void startGrains()
    {
        if (grainLength <= 0)
            prepare (44100.0);
        // The first grain starts at its peak, so a note's attack is not faded
        // in; the second starts with it, reading the same audio.
        grains[0] = { position, grainLength / 2, true };
        grains[1] = { position, 0, true };
        nextGrain = 0; // the older grain is the next to be replaced
        hopCountdown = grainLength / 2;
    }

    // WSOLA: the new grain starts near the read position, where its audio
    // best continues what the running grain plays now (a short correlation
    // search, coarse then fine), so overlapping grains add in phase. About
    // 10 multiply-adds per output sample at 44.1 kHz.
    double alignedStart (double target, double continuation) const noexcept
    {
        const auto& sample = *params.sample;
        const auto numSamples = sample.getNumSamples();
        const auto rate = juce::jmax (8000.0, sample.sampleRate);
        const auto direction = params.reverse ? -1 : 1;
        const auto searchRange = (int) (0.010 * rate);
        const auto span = (int) (0.008 * rate);
        const auto coarse = juce::jmax (1, (int) (rate / 5512.5)); // 8 at 44.1 kHz
        const auto fine = juce::jmax (1, coarse / 4);
        const auto from = (int) std::floor (continuation);
        const auto* left = sample.buffer.getReadPointer (0);
        const auto* right = sample.buffer.getReadPointer (sample.getNumChannels() > 1 ? 1 : 0);

        // Correlation of the window at start with the one at from, over the
        // first window's level. Indices outside the sample read silence.
        const auto score = [&] (int start, int step)
        {
            auto cross = 0.0f, energy = 1.0e-9f;
            const auto last = (span - 1) / step * step;
            const auto inside = [numSamples] (int a, int b) { return juce::jmin (a, b) >= 0 && juce::jmax (a, b) < numSamples; };
            if (inside (start, start + last * direction) && inside (from, from + last * direction))
            {
                for (int j = 0; j < span; j += step)
                {
                    const auto a = start + j * direction, b = from + j * direction;
                    const auto x = left[a] + right[a];
                    cross += x * (left[b] + right[b]);
                    energy += x * x;
                }
            }
            else
            {
                for (int j = 0; j < span; j += step)
                {
                    const auto x = monoAt (sample, start + j * direction);
                    cross += x * monoAt (sample, from + j * direction);
                    energy += x * x;
                }
            }
            return cross / std::sqrt (energy);
        };

        const auto centre = (int) std::floor (target);
        auto best = centre;
        auto bestScore = -1.0e30f;
        for (int offset = -searchRange; offset <= searchRange; offset += coarse)
            if (const auto value = score (centre + offset, coarse); value > bestScore)
            {
                bestScore = value;
                best = centre + offset;
            }

        // (Fine scores are compared only with each other: the step changes the scale.)
        const auto rough = best;
        bestScore = -1.0e30f;
        for (int offset = -coarse + 1; offset < coarse; ++offset)
            if (const auto value = score (rough + offset, fine); value > bestScore)
            {
                bestScore = value;
                best = rough + offset;
            }

        return (double) best + (target - std::floor (target));
    }

    void processStretched (float& left, float& right)
    {
        const auto& sample = *params.sample;
        const auto direction = params.reverse ? -1.0 : 1.0;

        if (hopCountdown <= 0)
        {
            // The younger grain is at its peak: the older one has ended.
            const auto& running = grains[(size_t) (1 - nextGrain)];
            grains[(size_t) nextGrain] = { alignedStart (position, running.read), 0, true };
            nextGrain = 1 - nextGrain;
            hopCountdown = grainLength / 2;
        }

        float sumL = 0.0f, sumR = 0.0f;
        const auto numSamples = sample.getNumSamples();
        const auto* dataL = sample.buffer.getReadPointer (0);
        const auto* dataR = sample.buffer.getReadPointer (sample.getNumChannels() > 1 ? 1 : 0);
        for (auto& grain : grains)
        {
            if (! grain.on)
                continue;
            if (grain.age >= grainLength)
            {
                grain.on = false;
                continue;
            }
            const auto weight = window[(size_t) grain.age];
            // Linear read; silence outside the sample.
            const auto index = (int) std::floor (grain.read);
            if (index >= 0 && index < numSamples)
            {
                const auto next = juce::jmin (index + 1, numSamples - 1);
                const auto frac = (float) (grain.read - (double) index);
                sumL += (dataL[index] + (dataL[next] - dataL[index]) * frac) * weight;
                sumR += (dataR[index] + (dataR[next] - dataR[index]) * frac) * weight;
            }
            grain.read += direction * pitch;
            ++grain.age;
        }

        // After a swap to a rendered copy: the old sound's grains fade out.
        if (tail.remaining > 0)
        {
            const auto fade = (float) tail.remaining / (float) tailLength();
            const auto& old = *tail.sample;
            const auto* oldL = old.buffer.getReadPointer (0);
            const auto* oldR = old.buffer.getReadPointer (old.getNumChannels() > 1 ? 1 : 0);
            for (auto& grain : tail.grains)
            {
                if (! grain.on || grain.age >= grainLength)
                {
                    grain.on = false;
                    continue;
                }
                const auto index = (int) std::floor (grain.read);
                if (index >= 0 && index < old.getNumSamples())
                {
                    const auto next = juce::jmin (index + 1, old.getNumSamples() - 1);
                    const auto frac = (float) (grain.read - (double) index);
                    const auto weight = window[(size_t) grain.age] * fade;
                    sumL += (oldL[index] + (oldL[next] - oldL[index]) * frac) * weight;
                    sumR += (oldR[index] + (oldR[next] - oldR[index]) * frac) * weight;
                }
                grain.read += direction * pitch;
                ++grain.age;
            }
            --tail.remaining;
        }

        const auto posSinceStart = params.reverse ? (regionEnd - position) : (position - regionStart);
        const auto posToEnd = params.reverse ? (position - regionStart) : (regionEnd - position);

        auto gain = 1.0;
        const auto fadeInLength = (double) params.fadeIn * regionLength;
        if (fadeInLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posSinceStart / fadeInLength);
        const auto fadeOutLength = (double) params.fadeOut * regionLength;
        if (fadeOutLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posToEnd / fadeOutLength);
        gain *= (double) params.gain;

        left = sumL * (float) gain;
        right = sumR * (float) gain;
        position += direction * ratio;
        --hopCountdown;
    }

    // The old sound's grains after a swap, fading out (they read the old
    // sample, which stays alive at least seconds after it's let go).
    struct Tail
    {
        const SampleData* sample = nullptr;
        std::array<Grain, 2> grains {};
        int remaining = 0;
    };

    int tailLength() const noexcept { return juce::jmax (1, grainLength / 4); }

    Params params;
    int lastLength = 0; // params.sample's length, kept so the old sample is never dereferenced
    Tail tail;
    std::vector<float> window;
    std::array<Grain, 2> grains {};
    int grainLength = 0, nextGrain = 0, hopCountdown = 0;
    double pitch = 1.0;
    double position = 0.0;
    double regionStart = 0.0;
    double regionEnd = 0.0;
    double regionLength = 0.0;
    // The loop's own span in sample frames, when one is in use.
    static constexpr double minLoopLength = 16.0;
    double ownLoopStart = 0.0, ownLoopEnd = 0.0;
    bool ownLoopOn = false;
    double ratio = 1.0;
    bool active = false;
};
