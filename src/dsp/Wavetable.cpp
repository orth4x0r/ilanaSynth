#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>

bool Wavetable::loadFromFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    const auto totalSamples = (int) reader->lengthInSamples;

    int frameLength = 0;
    int frameCount = 0;

    for (const auto candidate : { 2048, 1024, 512, 4096, 256 })
    {
        if (totalSamples % candidate == 0)
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

    if (frameLength == 0)
    {
        frameLength = totalSamples;
        frameCount = 1;
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

    std::vector<std::vector<float>> frames ((size_t) frameCount, std::vector<float> ((size_t) frameSize, 0.0f));

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

    name = file.getFileNameWithoutExtension();
    buildFromFrames (frames);
    return true;
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
