#pragma once

#include <juce_dsp/juce_dsp.h>

#include <complex>
#include <memory>
#include <vector>

class SpectralFreeze
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        window.resize ((size_t) fftSize);

        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) fftSize);

        inputRing.assign ((size_t) fftSize, 0.0f);
        outputRing.assign ((size_t) fftSize, 0.0f);
        spectrum.assign ((size_t) fftSize, {});
        frameSpectrum.assign ((size_t) fftSize, {});
        frozenMagnitude.assign ((size_t) (fftSize / 2 + 1), 0.0f);
        previousPhase.assign ((size_t) (fftSize / 2 + 1), 0.0f);
        phaseAccumulator.assign ((size_t) (fftSize / 2 + 1), 0.0f);
        inputWrite = 0;
        outputRead = 0;
        hopFill = 0;
    }

    void reset()
    {
        std::fill (inputRing.begin(), inputRing.end(), 0.0f);
        std::fill (outputRing.begin(), outputRing.end(), 0.0f);
        std::fill (frozenMagnitude.begin(), frozenMagnitude.end(), 0.0f);
        std::fill (previousPhase.begin(), previousPhase.end(), 0.0f);
        std::fill (phaseAccumulator.begin(), phaseAccumulator.end(), 0.0f);
        inputWrite = 0;
        outputRead = 0;
        hopFill = 0;
        hasFrozenData = false;
    }

    void process (float* data, int numSamples, bool frozen, float mix)
    {
        if (fft == nullptr)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            inputRing[(size_t) inputWrite] = data[i];
            inputWrite = (inputWrite + 1) % fftSize;

            const auto wet = outputRing[(size_t) outputRead];
            outputRing[(size_t) outputRead] = 0.0f;
            outputRead = (outputRead + 1) % fftSize;

            if (++hopFill >= hopSize)
            {
                hopFill = 0;
                processFrame (frozen);
            }

            data[i] = data[i] * (1.0f - mix) + wet * mix;
        }
    }

private:
    void processFrame (bool frozen)
    {
        const auto start = inputWrite;

        for (int i = 0; i < fftSize; ++i)
            spectrum[(size_t) i] = { inputRing[(size_t) ((start + i) % fftSize)] * window[(size_t) i], 0.0f };

        fft->perform (spectrum.data(), frameSpectrum.data(), false);

        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const auto& value = frameSpectrum[(size_t) bin];
            const auto magnitude = std::abs (value);
            const auto phase = std::arg (value);

            auto delta = (double) phase - (double) previousPhase[(size_t) bin];
            delta -= juce::MathConstants<double>::twoPi * std::floor (delta / juce::MathConstants<double>::twoPi + 0.5);

            previousPhase[(size_t) bin] = (float) phase;
            phaseAccumulator[(size_t) bin] += (float) delta;

            if (frozen)
            {
                if (! hasFrozenData)
                {
                    frozenMagnitude[(size_t) bin] = magnitude;
                    phaseAccumulator[(size_t) bin] = phase;
                }

                const auto frozenValue = std::polar (frozenMagnitude[(size_t) bin],
                                                     (float) phaseAccumulator[(size_t) bin]);
                frameSpectrum[(size_t) bin] = frozenValue;

                if (bin > 0 && bin < fftSize / 2)
                    frameSpectrum[(size_t) (fftSize - bin)] = std::conj (frozenValue);
            }
            else
            {
                frozenMagnitude[(size_t) bin] = magnitude;
                hasFrozenData = true;
            }
        }

        fft->perform (frameSpectrum.data(), spectrum.data(), true);

        for (int i = 0; i < fftSize; ++i)
            outputRing[(size_t) ((outputRead + i) % fftSize)] += spectrum[(size_t) i].real()
                                                                 * window[(size_t) i] / 1.5f;
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, inputRing, outputRing;
    std::vector<std::complex<float>> spectrum, frameSpectrum;
    std::vector<float> frozenMagnitude, previousPhase, phaseAccumulator;
    double sampleRate = 44100.0;
    int inputWrite = 0;
    int outputRead = 0;
    int hopFill = 0;
    bool hasFrozenData = false;
};
