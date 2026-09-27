#include "Wavetable.h"

#include <cstring>

#include <juce_dsp/juce_dsp.h>

#include <cmath>

// A wavetable .wav's frame size from its metadata: Serum's "clm " chunk
// ("<!>2048 ...", also written by Vital and ilanaSynth) or the "uhWT" chunk
// (both 2048 unless clm says otherwise). 0 if the file has neither.
int Wavetable::readWavFrameSize (const juce::File& file)
{
    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return 0;
    char header[12] {};
    if (stream.read (header, 12) != 12 || std::memcmp (header, "RIFF", 4) != 0 || std::memcmp (header + 8, "WAVE", 4) != 0)
        return 0;
    while (! stream.isExhausted())
    {
        char id[4] {};
        if (stream.read (id, 4) != 4)
            break;
        const auto size = (juce::int64) (juce::uint32) stream.readInt();
        const auto next = stream.getPosition() + size + (size & 1);
        if (std::memcmp (id, "clm ", 4) == 0 && size >= 3)
        {
            juce::MemoryBlock block;
            stream.readIntoMemoryBlock (block, (size_t) std::min<juce::int64> (size, 256));
            const auto text = block.toString();
            if (text.startsWith ("<!>"))
            {
                const auto tagged = text.substring (3).getIntValue();
                return tagged >= 16 && tagged <= 65536 ? tagged : 2048;
            }
            return 2048;
        }
        if (std::memcmp (id, "uhWT", 4) == 0)
            return 2048;
        if (! stream.setPosition (next))
            break;
    }
    return 0;
}

bool Wavetable::loadFromFile (const juce::File& file, LoadMode mode)
{
    std::vector<std::vector<float>> frames;
    if (! readFrames (file, mode, frames))
        return false;
    name = file.getFileNameWithoutExtension();
    buildFromFrames (frames);
    return true;
}

bool Wavetable::readFrames (const juce::File& file, LoadMode mode, std::vector<std::vector<float>>& frames)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    // Resynthesis only needs the first stretch of long recordings.
    const auto maxSamples = (juce::int64) (reader->sampleRate * 30.0);
    const auto totalSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);

    int frameLength = 0;
    int frameCount = 0;

    // A table made by Serum, Vital or ilanaSynth says its frame size.
    if (const auto tagged = file.hasFileExtension ("wav") ? readWavFrameSize (file) : 0;
        tagged > 0 && totalSamples % tagged == 0 && totalSamples / tagged <= 256)
    {
        frameLength = tagged;
        frameCount = totalSamples / tagged;
        if (mode == LoadMode::Automatic)
            mode = LoadMode::Frames;
    }

    for (const auto candidate : { 2048, 1024, 512, 4096, 256 })
    {
        if (frameLength == 0 && totalSamples % candidate == 0)
        {
            const auto count = totalSamples / candidate;

            if (count >= 1 && count <= 256)
            {
                frameLength = candidate;
                frameCount = count;
                break;
            }
        }
    }

    const auto numChannels = juce::jmax (1, (int) reader->numChannels);
    juce::AudioBuffer<float> buffer (numChannels, totalSamples);

    if (! reader->read (&buffer, 0, totalSamples, 0, true, true))
        return false;

    std::vector<float> mono ((size_t) totalSamples, 0.0f);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* input = buffer.getReadPointer (channel);

        for (int i = 0; i < totalSamples; ++i)
            mono[(size_t) i] += input[i];
    }

    const auto channelScale = 1.0f / (float) numChannels;

    for (auto& sample : mono)
        sample *= channelScale;

    const auto sliceAsFrames = mode == LoadMode::Frames || (mode == LoadMode::Automatic && frameLength > 0);

    frames.clear();

    if (sliceAsFrames)
    {
        if (frameLength == 0)
        {
            frameLength = totalSamples;
            frameCount = 1;
        }

        frames.assign ((size_t) frameCount, std::vector<float> ((size_t) frameSize, 0.0f));

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const auto* source = mono.data() + (size_t) frame * (size_t) frameLength;

            for (int i = 0; i < frameSize; ++i)
            {
                const auto position = (double) i * (double) frameLength / (double) frameSize;
                const auto index = juce::jlimit (0, frameLength - 1, (int) position);
                const auto next = juce::jmin (index + 1, frameLength - 1);
                const auto frac = (float) (position - (double) index);

                frames[(size_t) frame][(size_t) i] = source[index] + frac * (source[next] - source[index]);
            }
        }
    }
    else
    {
        resynthesize (mono, reader->sampleRate, frames);
    }

    return ! frames.empty();
}

double Wavetable::detectPeriod (const float* audio, int numSamples, double sampleRate)
{
    // YIN: cumulative-mean-normalised difference function, first dip under
    // the threshold, refined with a parabola.
    const auto minPeriod = juce::jmax (2, (int) (sampleRate / 2000.0));
    const auto maxPeriod = juce::jmin (numSamples / 2 - 2, (int) (sampleRate / 30.0));
    const auto window = juce::jmin (numSamples - maxPeriod - 2, (int) (sampleRate * 0.05));

    if (maxPeriod <= minPeriod || window < minPeriod * 2)
        return 0.0;

    std::vector<double> difference ((size_t) maxPeriod + 2, 0.0);

    for (int lag = 1; lag <= maxPeriod + 1; ++lag)
    {
        auto sum = 0.0;

        for (int i = 0; i < window; ++i)
        {
            const auto delta = (double) audio[i] - (double) audio[i + lag];
            sum += delta * delta;
        }

        difference[(size_t) lag] = sum;
    }

    std::vector<double> normalised ((size_t) maxPeriod + 2, 1.0);
    auto running = 0.0;

    for (int lag = 1; lag <= maxPeriod + 1; ++lag)
    {
        running += difference[(size_t) lag];
        normalised[(size_t) lag] = running > 0.0 ? difference[(size_t) lag] * (double) lag / running : 1.0;
    }

    auto best = -1;

    for (int lag = minPeriod; lag <= maxPeriod; ++lag)
    {
        if (normalised[(size_t) lag] < 0.15)
        {
            while (lag + 1 <= maxPeriod && normalised[(size_t) lag + 1] < normalised[(size_t) lag])
                ++lag;

            best = lag;
            break;
        }
    }

    if (best < 0)
    {
        // No confident dip: take the global minimum if it is still clear.
        auto lowest = 1.0;

        for (int lag = minPeriod; lag <= maxPeriod; ++lag)
            if (normalised[(size_t) lag] < lowest)
            {
                lowest = normalised[(size_t) lag];
                best = lag;
            }

        if (lowest > 0.35)
            return 0.0;
    }

    const auto a = normalised[(size_t) best - 1];
    const auto b = normalised[(size_t) best];
    const auto c = normalised[(size_t) best + 1];
    const auto denominator = a - 2.0 * b + c;
    const auto offset = std::abs (denominator) > 1.0e-12 ? 0.5 * (a - c) / denominator : 0.0;

    return (double) best + juce::jlimit (-1.0, 1.0, offset);
}

bool Wavetable::resynthesize (const std::vector<float>& audio, double sampleRate,
                              std::vector<std::vector<float>>& frames, int maxFrames)
{
    frames.clear();
    const auto numSamples = (int) audio.size();

    if (numSamples < 64)
        return false;

    // Analyse just after the attack, where the pitch has settled.
    const auto analysisStart = juce::jmin (numSamples / 4, (int) (sampleRate * 0.1));
    auto period = detectPeriod (audio.data() + analysisStart, numSamples - analysisStart, sampleRate);
    const auto pitched = period > 0.0;

    if (! pitched)
        period = juce::jmin ((double) numSamples, 2048.0);

    const auto sampleAt = [&audio, numSamples] (double position)
    {
        const auto index = juce::jlimit (0, numSamples - 1, (int) position);
        const auto next = juce::jmin (index + 1, numSamples - 1);
        const auto frac = (float) (position - (double) index);
        return audio[(size_t) index] + frac * (audio[(size_t) next] - audio[(size_t) index]);
    };

    const auto usable = (double) numSamples - period * 2.0;
    const auto count = juce::jlimit (1, maxFrames, usable > 0.0 ? (int) (usable / period) : 1);

    for (int frame = 0; frame < count; ++frame)
    {
        auto start = count > 1 ? usable * (double) frame / (double) (count - 1) : 0.0;

        // Line each cycle up on a rising zero crossing so frames morph
        // smoothly instead of jumping phase.
        if (pitched)
        {
            for (auto search = (int) start; search < (int) (start + period) && search + 1 < numSamples; ++search)
            {
                const auto a = audio[(size_t) search];
                const auto b = audio[(size_t) search + 1];

                if (a <= 0.0f && b > 0.0f)
                {
                    start = (double) search + (double) (-a / (b - a));
                    break;
                }
            }
        }

        std::vector<float> cycle ((size_t) frameSize);

        for (int i = 0; i < frameSize; ++i)
            cycle[(size_t) i] = sampleAt (start + period * (double) i / (double) frameSize);

        frames.push_back (std::move (cycle));
    }

    return pitched;
}

void Wavetable::buildFromFrames (const std::vector<std::vector<float>>& frames)
{
    if (frames.empty())
        return;

    numFrames = (int) frames.size();
    buildLevels (frames);
}

void Wavetable::buildLevels (const std::vector<std::vector<float>>& frames)
{
    using Complex = std::complex<float>;

    levels.assign ((size_t) numLevels,
                   std::vector<std::vector<float>> ((size_t) numFrames,
                                                    std::vector<float> ((size_t) frameSize + 3, 0.0f)));

    juce::dsp::FFT fft ((int) std::log2 ((double) frameSize));

    std::vector<Complex> timeInput ((size_t) frameSize);
    std::vector<Complex> spectrum ((size_t) frameSize);
    std::vector<Complex> levelSpectrum ((size_t) frameSize);
    std::vector<Complex> levelTime ((size_t) frameSize);

    auto peak = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const auto& source = frames[(size_t) frame];

        for (int i = 0; i < frameSize; ++i)
        {
            const auto value = i < (int) source.size() ? source[(size_t) i] : 0.0f;
            timeInput[(size_t) i] = Complex (value, 0.0f);
        }

        fft.perform (timeInput.data(), spectrum.data(), false);

        for (int level = 0; level < numLevels; ++level)
        {
            const auto maxHarmonic = juce::jmax (1, numHarmonics >> level);

            levelSpectrum = spectrum;
            levelSpectrum[0] = Complex (0.0f, 0.0f);

            for (int bin = 1; bin < frameSize / 2; ++bin)
            {
                if (bin > maxHarmonic)
                {
                    levelSpectrum[(size_t) bin] = Complex (0.0f, 0.0f);
                    levelSpectrum[(size_t) (frameSize - bin)] = Complex (0.0f, 0.0f);
                }
            }

            fft.perform (levelSpectrum.data(), levelTime.data(), true);

            auto& target = levels[(size_t) level][(size_t) frame];

            for (int i = 0; i < frameSize; ++i)
            {
                const auto value = levelTime[(size_t) i].real();
                target[(size_t) i + 1] = value;
                peak = juce::jmax (peak, std::abs (value));
            }

            target[0] = target[(size_t) frameSize];
            target[(size_t) frameSize + 1] = target[1];
            target[(size_t) frameSize + 2] = target[2];
        }
    }

    if (peak > 0.0f)
    {
        const auto scale = 1.0f / peak;

        for (auto& level : levels)
            for (auto& frame : level)
                for (auto& sample : frame)
                    sample *= scale;
    }

    framePointers.clear();

    for (const auto& level : levels)
        for (const auto& frame : level)
            framePointers.push_back (frame.data());
}

int Wavetable::getLevelForFrequency (double frequency, double sampleRate) const noexcept
{
    const auto nyquist = sampleRate * 0.5;
    auto maxHarmonics = (double) numHarmonics;
    auto level = 0;

    while (level < numLevels - 1 && maxHarmonics * frequency > nyquist)
    {
        maxHarmonics *= 0.5;
        ++level;
    }

    return level;
}
