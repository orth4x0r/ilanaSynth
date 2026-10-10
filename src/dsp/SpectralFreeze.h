#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>
#include <memory>
#include <vector>

// Both channels share one FFT pair: the left channel goes in the real part
// and the right one in the imaginary part of the same complex signal, and
// the two spectra are pulled apart again from the symmetry of a real
// signal's spectrum. That halves the FFT work (the cost of this effect is
// the FFTs, 4 of them per hop for a stereo pair done separately). Each
// channel keeps its own bin state, so the result is the same as two
// separate mono freezers up to float rounding.
class SpectralFreeze
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        window.resize ((size_t) fftSize);

        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) fftSize);

        // (Bins 2 up, a bin of 21 Hz at 44.1k, in numBands equal steps of pitch.)
        for (int band = 0; band < numBands; ++band)
        {
            bandLo[(size_t) band] = (int) std::floor (2.0 * std::pow ((double) (fftSize / 2) / 2.0, (double) band / numBands));
            bandHi[(size_t) band] = juce::jmax (bandLo[(size_t) band] + 1,
                                                (int) std::floor (2.0 * std::pow ((double) (fftSize / 2) / 2.0, (double) (band + 1) / numBands)));
        }

        spectrum.assign ((size_t) fftSize, {});
        frameSpectrum.assign ((size_t) fftSize, {});

        for (int ch = 0; ch < 2; ++ch)
        {
            inputRing[(size_t) ch].assign ((size_t) fftSize, 0.0f);
            outputRing[(size_t) ch].assign ((size_t) fftSize, 0.0f);
            half[(size_t) ch].assign ((size_t) numBins, {});
            frozenMagnitude[(size_t) ch].assign ((size_t) numBins, 0.0f);
            previousPhase[(size_t) ch].assign ((size_t) numBins, 0.0f);
            phaseAccumulator[(size_t) ch].assign ((size_t) numBins, 0.0f);
        }

        inputWrite = 0;
        outputRead = 0;
        hopFill = 0;
        lastMix = std::numeric_limits<float>::quiet_NaN();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (inputRing[(size_t) ch].begin(), inputRing[(size_t) ch].end(), 0.0f);
            std::fill (outputRing[(size_t) ch].begin(), outputRing[(size_t) ch].end(), 0.0f);
            std::fill (frozenMagnitude[(size_t) ch].begin(), frozenMagnitude[(size_t) ch].end(), 0.0f);
            std::fill (previousPhase[(size_t) ch].begin(), previousPhase[(size_t) ch].end(), 0.0f);
            std::fill (phaseAccumulator[(size_t) ch].begin(), phaseAccumulator[(size_t) ch].end(), 0.0f);
        }

        inputWrite = 0;
        outputRead = 0;
        hopFill = 0;
        hasFrozenData = false;
        lastMix = std::numeric_limits<float>::quiet_NaN();
    }

    // The held (or, while not holding, the live) spectrum of the left
    // channel as numBands log-spaced peaks, for the FREEZE card's picture.
    // Written once per hop by the audio thread, read by the editor; a stale
    // or torn read only moves a bar.
    static constexpr int numBands = 48;

    void getBands (std::array<float, numBands>& out) const
    {
        for (size_t i = 0; i < (size_t) numBands; ++i)
            out[i] = bands[i].load (std::memory_order_relaxed);
    }

    // One channel (the right one is silence and is not written back).
    void process (float* data, int numSamples, bool frozen, float mix)
    {
        process (data, nullptr, numSamples, frozen, mix);
    }

    // Both channels together; right may be null for a mono signal.
    void process (float* left, float* right, int numSamples, bool frozen, float mix)
    {
        if (fft == nullptr)
            return;

        // MIX eases across the block from the last one (no step at the block edge).
        const auto mixFrom = std::isfinite (lastMix) ? lastMix : mix;
        const auto mixStep = numSamples > 0 ? (mix - mixFrom) / (float) numSamples : 0.0f;
        lastMix = mix;

        for (int i = 0; i < numSamples; ++i)
        {
            inputRing[0][(size_t) inputWrite] = left[i];
            inputRing[1][(size_t) inputWrite] = right != nullptr ? right[i] : 0.0f;
            inputWrite = (inputWrite + 1) & (fftSize - 1);

            const auto wetL = outputRing[0][(size_t) outputRead];
            const auto wetR = outputRing[1][(size_t) outputRead];
            outputRing[0][(size_t) outputRead] = 0.0f;
            outputRing[1][(size_t) outputRead] = 0.0f;
            outputRead = (outputRead + 1) & (fftSize - 1);

            if (++hopFill >= hopSize)
            {
                hopFill = 0;
                processFrame (frozen);
            }

            const auto mixNow = mixFrom + mixStep * (float) i;
            left[i] = left[i] * (1.0f - mixNow) + wetL * mixNow;
            if (right != nullptr)
                right[i] = right[i] * (1.0f - mixNow) + wetR * mixNow;
        }
    }

private:
    // atan2 to about 1e-5 rad (the phases are only differenced and
    // re-accumulated, so that error does not build up); the library one was
    // a large share of this effect's time.
    static float fastAtan2 (float y, float x)
    {
        const auto ax = std::abs (x), ay = std::abs (y);
        const auto hi = juce::jmax (ax, ay), lo = juce::jmin (ax, ay);

        if (hi == 0.0f)
            return 0.0f;

        const auto a = lo / hi;
        const auto sq = a * a;
        auto r = a * (0.9998660f + sq * (-0.3302995f + sq * (0.1801410f + sq * (-0.0851330f + sq * 0.0208351f))));

        if (ay > ax)
            r = juce::MathConstants<float>::halfPi - r;
        if (x < 0.0f)
            r = juce::MathConstants<float>::pi - r;

        return y < 0.0f ? -r : r;
    }

    void processFrame (bool frozen)
    {
        const auto start = inputWrite;

        for (int i = 0; i < fftSize; ++i)
        {
            const auto at = (size_t) ((start + i) & (fftSize - 1));
            const auto w = window[(size_t) i];
            spectrum[(size_t) i] = { inputRing[0][at] * w, inputRing[1][at] * w };
        }

        fft->perform (spectrum.data(), frameSpectrum.data(), false);

        // Pull the two real signals' spectra apart: with Z = L + iR,
        // L[k] = (Z[k] + conj Z[-k]) / 2 and R[k] = (Z[k] - conj Z[-k]) / 2i.
        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto a = frameSpectrum[(size_t) bin];
            const auto b = std::conj (frameSpectrum[(size_t) ((fftSize - bin) & (fftSize - 1))]);
            const auto sum = a + b, diff = a - b;
            half[0][(size_t) bin] = 0.5f * sum;
            half[1][(size_t) bin] = { 0.5f * diff.imag(), -0.5f * diff.real() };
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& values = half[(size_t) ch];
            auto& held = frozenMagnitude[(size_t) ch];
            auto& lastPhase = previousPhase[(size_t) ch];
            auto& accumulated = phaseAccumulator[(size_t) ch];

            for (int bin = 0; bin < numBins; ++bin)
            {
                const auto value = values[(size_t) bin];
                const auto magnitude = std::sqrt (value.real() * value.real() + value.imag() * value.imag());
                const auto phase = fastAtan2 (value.imag(), value.real());

                // Both phases are in -pi..pi, so one turn is the most to take off.
                constexpr auto pi = juce::MathConstants<float>::pi;
                constexpr auto twoPi = juce::MathConstants<float>::twoPi;
                auto delta = phase - lastPhase[(size_t) bin];
                delta += delta > pi ? -twoPi : (delta < -pi ? twoPi : 0.0f);

                lastPhase[(size_t) bin] = phase;

                // (Kept within one turn, so the sine and cosine below stay exact however long it holds.)
                auto total = accumulated[(size_t) bin] + delta;
                total += total > pi ? -twoPi : (total < -pi ? twoPi : 0.0f);
                accumulated[(size_t) bin] = total;

                if (frozen)
                {
                    if (! hasFrozenData)
                    {
                        held[(size_t) bin] = magnitude;
                        accumulated[(size_t) bin] = phase;
                    }

                    const auto angle = accumulated[(size_t) bin];
                    values[(size_t) bin] = { held[(size_t) bin] * std::cos (angle), held[(size_t) bin] * std::sin (angle) };
                }
                else
                {
                    held[(size_t) bin] = magnitude;
                }
            }
        }

        if (! frozen)
            hasFrozenData = true;

        for (int band = 0; band < numBands; ++band)
        {
            auto peak = 0.0f;
            for (int bin = bandLo[(size_t) band]; bin < bandHi[(size_t) band] && bin < numBins; ++bin)
                peak = juce::jmax (peak, frozenMagnitude[0][(size_t) bin]);
            bands[(size_t) band].store (peak, std::memory_order_relaxed);
        }

        // Back to one complex spectrum, Z = L + iR, rebuilt from the halves
        // (the first and last bins keep only their real part, as the
        // real-valued output before did).
        const auto& left = half[0];
        const auto& right = half[1];
        spectrum[0] = { left[0].real(), right[0].real() };
        spectrum[(size_t) (fftSize / 2)] = { left[(size_t) (fftSize / 2)].real(), right[(size_t) (fftSize / 2)].real() };

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const auto l = left[(size_t) bin], r = right[(size_t) bin];
            spectrum[(size_t) bin] = { l.real() - r.imag(), l.imag() + r.real() };
            spectrum[(size_t) (fftSize - bin)] = { l.real() + r.imag(), r.real() - l.imag() };
        }

        fft->perform (spectrum.data(), frameSpectrum.data(), true);

        for (int i = 0; i < fftSize; ++i)
        {
            const auto at = (size_t) ((outputRead + i) & (fftSize - 1));
            const auto scale = window[(size_t) i] / 1.5f;
            outputRing[0][at] += frameSpectrum[(size_t) i].real() * scale;
            outputRing[1][at] += frameSpectrum[(size_t) i].imag() * scale;
        }
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::array<std::vector<float>, 2> inputRing, outputRing;
    std::vector<std::complex<float>> spectrum, frameSpectrum;
    std::array<std::vector<std::complex<float>>, 2> half;
    std::array<std::vector<float>, 2> frozenMagnitude, previousPhase, phaseAccumulator;
    std::array<int, numBands> bandLo {}, bandHi {};
    double sampleRate = 44100.0;
    int inputWrite = 0;
    int outputRead = 0;
    int hopFill = 0;
    bool hasFrozenData = false;
    float lastMix = std::numeric_limits<float>::quiet_NaN(); // MIX of the last block (eased from)
    std::array<std::atomic<float>, numBands> bands {};
};
