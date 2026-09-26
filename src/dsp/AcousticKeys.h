#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <vector>

// M4 acoustic keys: the parts of a piano that are shared by every note and
// so live after the voices, once for the whole synth. All storage is
// allocated in prepare(); process calls never allocate.

namespace AcousticKeysDetail
{
// A two-pole resonator (one soundboard mode): unity peak gain, decay set as
// a T60 in seconds.
struct Mode
{
    float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f, x2 = 0.0f, x1 = 0.0f;

    void set (double sampleRate, double frequency, double t60)
    {
        frequency = juce::jlimit (20.0, sampleRate * 0.45, frequency);
        const auto radius = std::exp (-6.91 / (juce::jmax (0.005, t60) * sampleRate));
        const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        a1 = (float) (2.0 * radius * std::cos (w));
        a2 = (float) (-radius * radius);
        b0 = (float) ((1.0 - radius * radius) * 0.5);
    }

    void reset() { y1 = y2 = x1 = x2 = 0.0f; }

    float process (float x)
    {
        // Band-pass form: zeros at DC and Nyquist.
        const auto y = b0 * (x - x2) + a1 * y1 + a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};
} // namespace AcousticKeysDetail

// The soundboard: a bank of wooden modes the strings drive, with SIZE
// moving the modes (a bigger board is lower and rings longer) and TONE
// standing in for lid and mic position (closed and dark to open and bright).
class Soundboard
{
public:
    static constexpr int numModes = 18;

    void prepare (double rate)
    {
        sampleRate = juce::jmax (1.0, rate);
        lastSize = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& channel : modes)
            for (auto& mode : channel)
                mode.reset();

        for (auto& state : toneState)
            state[0] = state[1] = 0.0f;
        for (auto& channel : radiation)
            for (auto& filter : channel)
                filter.reset();
        for (auto& filter : boardBump)
            filter.reset();
    }

    void process (float* left, float* right, int numSamples, float mix, float tone, float size)
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        tone = juce::jlimit (0.0f, 1.0f, tone);
        updateModes (juce::jlimit (0.0f, 1.0f, size));

        // One-pole low-pass for the lid: its corner opens up with TONE.
        const auto corner = 450.0 + 6000.0 * (double) (tone * tone);
        const auto toneCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * corner / sampleRate));
        const auto brightness = 0.05f + 1.35f * tone; // how much of the unfiltered top end passes
        // A soundboard barely radiates the lowest fundamentals: the deep bass
        // is heard mostly through its overtones.
        updateRadiation();

        for (int i = 0; i < numSamples; ++i)
        {
            const float input[2] { left[i], right != nullptr ? right[i] : left[i] };
            const auto mono = 0.5f * (input[0] + input[1]);

            for (int channel = 0; channel < (right != nullptr ? 2 : 1); ++channel)
            {
                auto body = 0.0f;

                for (int m = 0; m < numModes; ++m)
                    body += modes[(size_t) channel][(size_t) m].process (mono) * modeGains[(size_t) channel][(size_t) m];

                // A soundboard barely radiates the lowest fundamentals (a real
                // grand's E1 fundamental is ~40 dB under its 2nd partial): a
                // steep 70 Hz low cut, so the deep bass is heard through its
                // overtones.
                auto out = input[channel] * (1.0f - 0.4f * mix) + body * mix * 2.2f;
                out = radiation[(size_t) channel][1].process (radiation[(size_t) channel][0].process (out));
                // The board radiates the lower middle best (a broad bump
                // around the tenor's fundamentals).
                out = boardBump[(size_t) channel].process (out);
                // Two one-poles: -12 dB/octave above the lid corner.
                auto& state = toneState[channel];
                state[0] += (out - state[0]) * toneCoefficient;
                state[1] += (state[0] - state[1]) * toneCoefficient;
                out = state[1] + (out - state[1]) * brightness;
                out = std::isfinite (out) ? juce::jlimit (-8.0f, 8.0f, out) : 0.0f;

                if (channel == 0)
                    left[i] = out;
                else
                    right[i] = out;
            }
        }
    }

private:
    void updateModes (float size)
    {
        if (std::abs (size - lastSize) < 0.001f)
            return;

        lastSize = size;

        // Roughly the low modes of a grand's soundboard, spaced irregularly.
        constexpr double frequencies[numModes] { 92.0, 118.0, 147.0, 181.0, 223.0, 270.0, 330.0, 402.0, 488.0,
                                                 590.0, 712.0, 860.0, 1040.0, 1260.0, 1530.0, 1850.0, 2250.0, 2750.0 };
        const auto scale = std::exp2 (0.6 - 1.2 * (double) size); // small board: up to +0.6 oct
        juce::Random random (4242);

        for (int m = 0; m < numModes; ++m)
        {
            const auto t60 = (0.28 - 0.2 * (double) m / (double) numModes) * (0.6 + 0.9 * (double) size);

            for (int channel = 0; channel < 2; ++channel)
            {
                // Each side of the board sees a slightly different mode set.
                const auto detune = 1.0 + ((double) random.nextFloat() - 0.5) * 0.04;
                modes[(size_t) channel][(size_t) m].set (sampleRate, frequencies[m] * scale * detune, t60);
                const auto sign = random.nextBool() ? 1.0f : -1.0f;
                modeGains[(size_t) channel][(size_t) m] = sign * (0.6f + 0.4f * random.nextFloat())
                                                          / std::sqrt ((float) (m + 2));
            }
        }
    }

    std::array<std::array<AcousticKeysDetail::Mode, numModes>, 2> modes;
    std::array<std::array<float, numModes>, 2> modeGains {};
    float toneState[2][2] {};

    // 4th-order Butterworth-ish high-pass (two RBJ sections).
    struct HighPass
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double rate, double frequency, double q)
        {
            const auto w = juce::MathConstants<double>::twoPi * frequency / rate;
            const auto alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
            b0 = (1.0 + c) / 2.0 / a0; b1 = -(1.0 + c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
        }
        void reset() { z1 = z2 = 0.0; }
        float process (float x)
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };
    std::array<std::array<HighPass, 2>, 2> radiation;

    // Peaking EQ (RBJ) for the radiation bump.
    struct Peak
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double rate, double frequency, double q, double gainDb)
        {
            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto w = juce::MathConstants<double>::twoPi * frequency / rate;
            const auto alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha / a;
            b0 = (1.0 + alpha * a) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - alpha * a) / a0;
            a1 = -2.0 * c / a0; a2 = (1.0 - alpha / a) / a0;
        }
        void reset() { z1 = z2 = 0.0; }
        float process (float x)
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };
    std::array<Peak, 2> boardBump;
    double radiationRate = 0.0;
    void updateRadiation()
    {
        if (radiationRate == sampleRate)
            return;
        radiationRate = sampleRate;
        for (auto& channel : radiation)
        {
            channel[0].set (sampleRate, 70.0, 0.5412);
            channel[1].set (sampleRate, 70.0, 1.3066);
        }
        for (auto& filter : boardBump)
            filter.set (sampleRate, 260.0, 0.8, 5.0);
    }
    double sampleRate = 48000.0;
    float lastSize = -1.0f;
};

// With the sustain pedal down every damper lifts, so the whole keyboard's
// strings ring in sympathy. Two octaves of undamped strings (C2 to B3, whose
// overtones cover the rest of the keyboard) are shared by all voices.
// Pressing the pedal lifts the dampers off strings that are still moving,
// which gives the soft "bloom".
class PedalResonance
{
public:
    static constexpr int numStrings = 24;

    void prepare (double rate)
    {
        sampleRate = juce::jmax (1.0, rate);
        const auto capacity = (size_t) std::ceil (sampleRate / 60.0) + 4;

        for (int s = 0; s < numStrings; ++s)
        {
            auto& string = strings[(size_t) s];
            string.delay.assign (capacity, 0.0f);
            const auto frequency = juce::MidiMessage::getMidiNoteInHertz (36 + s);
            string.period = juce::jlimit (2.0, (double) capacity - 2.0,
                                          sampleRate / frequency - (1.0 - loopFilter) / loopFilter);
            string.pan = (s % 2 == 0) ? 0.3f : 0.7f;
        }

        reset();
    }

    void reset()
    {
        for (auto& string : strings)
        {
            std::fill (string.delay.begin(), string.delay.end(), 0.0f);
            string.write = 0;
            string.filter = 0.0f;
        }

        bloomRemaining = 0;
        energy = 0.0f;
    }

    void setPedal (bool down, float amount)
    {
        if (down && ! pedalDown && amount > 0.0f)
        {
            bloomRemaining = (int) (sampleRate * 0.03);
            bloomLevel = 0.02f * amount;
        }

        pedalDown = down;
    }

    bool isRinging() const { return pedalDown || energy > 1.0e-6f; }

    void process (float* left, float* right, int numSamples, float amount)
    {
        // Dampers down: strings stop within a few hundred milliseconds.
        const auto feedback = pedalDown ? 0.9975f : 0.9f;
        auto blockEnergy = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            auto input = 0.5f * (left[i] + (right != nullptr ? right[i] : left[i]));
            input = pedalDown ? juce::jlimit (-2.0f, 2.0f, input) * 0.012f * amount : 0.0f;

            if (bloomRemaining > 0)
            {
                input += (random.nextFloat() * 2.0f - 1.0f) * bloomLevel;
                --bloomRemaining;
            }

            auto outLeft = 0.0f, outRight = 0.0f;

            for (auto& string : strings)
            {
                const auto size = (int) string.delay.size();
                auto read = (double) string.write - string.period;
                if (read < 0.0)
                    read += size;
                const auto index = (int) read;
                const auto fraction = (float) (read - index);
                const auto delayed = string.delay[(size_t) index]
                                     + (string.delay[(size_t) ((index + 1) % size)] - string.delay[(size_t) index]) * fraction;
                string.filter += (delayed - string.filter) * (float) loopFilter;
                string.delay[(size_t) string.write] = juce::jlimit (-4.0f, 4.0f, string.filter * feedback + input);
                string.write = (string.write + 1) % size;
                outLeft += delayed * (1.0f - string.pan);
                outRight += delayed * string.pan;
                blockEnergy += delayed * delayed;
            }

            const auto gain = 1.6f * amount / (float) numStrings;
            left[i] += std::isfinite (outLeft) ? outLeft * gain : 0.0f;
            if (right != nullptr)
                right[i] += std::isfinite (outRight) ? outRight * gain : 0.0f;
        }

        energy = blockEnergy / (float) juce::jmax (1, numSamples * numStrings);
    }

private:
    struct String
    {
        std::vector<float> delay;
        int write = 0;
        double period = 200.0;
        float filter = 0.0f, pan = 0.5f;
    };

    static constexpr double loopFilter = 0.45;
    std::array<String, numStrings> strings;
    juce::Random random { 7331 };
    double sampleRate = 48000.0;
    bool pedalDown = false;
    int bloomRemaining = 0;
    float bloomLevel = 0.0f, energy = 0.0f;
};

// Mechanical noises: the key returning (a wooden "thock"), the damper felt
// landing on the strings, and the pedal mechanism. Short synthesised events
// from a small pool, placed in the stereo field by note.
class MechanicalNoise
{
public:
    enum class Kind { KeyRelease, Damper, PedalDown, PedalUp };

    void prepare (double rate)
    {
        sampleRate = juce::jmax (1.0, rate);
        reset();
    }

    void reset()
    {
        for (auto& event : events)
            event.remaining = 0;
    }

    // note: MIDI note for placement and pitch (-1 for the pedal).
    void trigger (Kind kind, int note, float level, int sampleOffset)
    {
        if (level <= 0.0f)
            return;

        auto* slot = &events[0];

        for (auto& event : events)
        {
            if (event.remaining <= 0)
            {
                slot = &event;
                break;
            }

            if (event.remaining < slot->remaining)
                slot = &event;
        }

        auto& event = *slot;
        event.kind = kind;
        event.delay = juce::jmax (0, sampleOffset);
        event.elapsed = 0;
        event.phase = 0.0;
        event.band = 0.0f;
        event.low = 0.0f;
        event.pan = note >= 0 ? juce::jlimit (0.15f, 0.85f, 0.5f + (float) (note - 64) / 90.0f) : 0.5f;

        const auto variation = 0.85f + 0.3f * random.nextFloat();

        switch (kind)
        {
            case Kind::KeyRelease:
                event.length = (int) (sampleRate * 0.035);
                event.frequency = (110.0 + (double) juce::jmax (0, note) * 0.6) * variation;
                event.level = level * 0.14f;
                event.noiseLevel = level * 0.05f;
                event.noiseCutoff = 0.45f;
                break;
            case Kind::Damper:
                event.length = (int) (sampleRate * 0.09);
                event.frequency = 0.0;
                event.level = 0.0f;
                // Bass dampers are bigger felt blocks: louder.
                event.noiseLevel = level * 0.05f * (1.2f - 0.5f * juce::jlimit (0.0f, 1.0f, (float) (note - 21) / 87.0f));
                event.noiseCutoff = 0.08f;
                break;
            case Kind::PedalDown:
                event.length = (int) (sampleRate * 0.12);
                event.frequency = 52.0 * variation;
                event.level = level * 0.2f;
                event.noiseLevel = level * 0.04f;
                event.noiseCutoff = 0.05f;
                break;
            case Kind::PedalUp:
                event.length = (int) (sampleRate * 0.18);
                event.frequency = 64.0 * variation;
                event.level = level * 0.12f;
                event.noiseLevel = level * 0.07f;
                event.noiseCutoff = 0.1f;
                break;
        }

        event.remaining = event.length + event.delay;
    }

    void process (float* left, float* right, int numSamples)
    {
        for (auto& event : events)
        {
            if (event.remaining <= 0)
                continue;

            for (int i = 0; i < numSamples && event.remaining > 0; ++i, --event.remaining)
            {
                if (event.delay > 0)
                {
                    --event.delay;
                    continue;
                }

                const auto t = (float) event.elapsed / (float) juce::jmax (1, event.length);
                // Fast attack, then an exponential fall.
                const auto envelope = juce::jmin (1.0f, (float) event.elapsed / (float) (sampleRate * 0.002))
                                      * std::exp (-5.0f * t);
                auto value = 0.0f;

                if (event.level > 0.0f)
                {
                    value += (float) std::sin (event.phase) * event.level * envelope;
                    event.phase += juce::MathConstants<double>::twoPi * event.frequency / sampleRate;
                }

                // Band-limited noise: a one-pole low-pass minus a slower one.
                const auto noise = random.nextFloat() * 2.0f - 1.0f;
                event.band += (noise - event.band) * event.noiseCutoff;
                event.low += (event.band - event.low) * event.noiseCutoff * 0.25f;
                value += (event.band - event.low) * event.noiseLevel * envelope * 2.0f;

                ++event.elapsed;
                left[i] += value * (1.0f - event.pan) * 1.4f;
                if (right != nullptr)
                    right[i] += value * event.pan * 1.4f;
            }
        }
    }

private:
    struct Event
    {
        Kind kind = Kind::KeyRelease;
        int remaining = 0, delay = 0, elapsed = 0, length = 1;
        double phase = 0.0, frequency = 100.0;
        float level = 0.0f, noiseLevel = 0.0f, noiseCutoff = 0.2f, pan = 0.5f;
        float band = 0.0f, low = 0.0f;
    };

    std::array<Event, 16> events;
    juce::Random random { 9001 };
    double sampleRate = 48000.0;
};
