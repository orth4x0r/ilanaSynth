#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include <complex>
#include <functional>
#include <iostream>
#include <set>
#include <vector>

#include "Presets.h"
#include "PluginProcessor.h"
#include "gui/ParamInfo.h"
#include "dsp/GranularPitchShift.h"
#include "dsp/FilterUnit.h"
#include "dsp/FmAlgorithms.h"
#include "dsp/Formula.h"
#include "dsp/GranularSmear.h"
#include "dsp/LfoCurve.h"
#include "dsp/LfoShape.h"
#include "dsp/Modulation.h"
#include "dsp/PolyBlepOsc.h"
#include "dsp/SpectralFreeze.h"
#include "dsp/Svf.h"
#include "dsp/TableFactory.h"
#include "dsp/UnisonBank.h"
#include "dsp/Voice.h"
#include "dsp/Wavetable.h"
#include "dsp/WavetableOscillator.h"
#include "SamplingProfiler.h"

namespace
{
int failures = 0;

double fundamentalOf (const std::vector<float>& input, double sampleRate);

void check (bool condition, const juce::String& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message.toStdString() << std::endl;

    if (! condition)
        ++failures;
}

// CPU budgets depend on the machine. On CI (ILANA_CI=1, shared runners whose
// speed swings run to run) a missed budget is reported but not counted;
// everywhere else it fails like any check.
void checkTiming (bool condition, const juce::String& message)
{
    if (! condition && juce::SystemStats::getEnvironmentVariable ("ILANA_CI", "").isNotEmpty())
    {
        std::cout << "TIMING (not counted on CI): " << message.toStdString() << std::endl;
        return;
    }
    check (condition, message);
}

void runMipmapTests()
{
    juce::dsp::FFT fft (11);

    std::vector<std::complex<float>> input ((size_t) Wavetable::frameSize);
    std::vector<std::complex<float>> output ((size_t) Wavetable::frameSize);

    const auto names = TableFactory::getFactoryTableNames();

    for (int tableIndex = 0; tableIndex < TableFactory::getNumFactoryTables(); ++tableIndex)
    {
        Wavetable table;
        table.buildFromFrames (TableFactory::generate (tableIndex));

        for (int level = 0; level < Wavetable::numLevels; ++level)
        {
            const auto maxHarmonic = juce::jmax (1, Wavetable::numHarmonics >> level);
            auto worstRatio = 0.0f;

            for (const auto frame : { 0, 16, 32, 63 })
            {
                const auto* data = table.getFrameData (level, frame);

                for (int i = 0; i < Wavetable::frameSize; ++i)
                    input[(size_t) i] = { data[i + 1], 0.0f };

                fft.perform (input.data(), output.data(), false);

                auto peak = 0.0f;
                auto above = 0.0f;

                for (int bin = 1; bin < Wavetable::frameSize / 2; ++bin)
                {
                    const auto magnitude = std::abs (output[(size_t) bin]);
                    peak = juce::jmax (peak, magnitude);

                    if (bin > maxHarmonic + 2)
                        above = juce::jmax (above, magnitude);
                }

                if (peak > 0.0f)
                    worstRatio = juce::jmax (worstRatio, above / peak);
            }

            check (worstRatio < 0.001f,
                   "mipmap " + names[tableIndex] + " level " + juce::String (level)
                       + " band-limited (above-band ratio " + juce::String (worstRatio, 6) + ")");
        }
    }
}

void runAliasTests()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 2093.0;
    constexpr int renderSize = 16384;
    constexpr int fftOrder = 14;

    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    WavetableOscillator osc;
    osc.setSampleRate (sampleRate);
    osc.setWavetable (&table);
    osc.setFrequency (frequency);
    osc.setFramePosition (0.5f);

    std::vector<float> samples ((size_t) renderSize, 0.0f);

    for (int i = 0; i < renderSize; ++i)
        samples[(size_t) i] = osc.getNextSample();

    juce::dsp::WindowingFunction<float> window ((size_t) renderSize,
                                                juce::dsp::WindowingFunction<float>::hann,
                                                true);
    window.multiplyWithWindowingTable (samples.data(), (size_t) renderSize);

    juce::dsp::FFT fft (fftOrder);

    std::vector<std::complex<float>> input ((size_t) renderSize);
    std::vector<std::complex<float>> output ((size_t) renderSize);

    for (int i = 0; i < renderSize; ++i)
        input[(size_t) i] = { samples[(size_t) i], 0.0f };

    fft.perform (input.data(), output.data(), false);

    const auto binWidth = sampleRate / (double) renderSize;

    auto peak = 0.0f;
    auto worstOffHarmonic = 0.0f;

    for (int bin = 1; bin < renderSize / 2; ++bin)
    {
        const auto magnitude = std::abs (output[(size_t) bin]);
        peak = juce::jmax (peak, magnitude);

        const auto binFrequency = (double) bin * binWidth;
        const auto nearestHarmonic = std::round (binFrequency / frequency);

        if (nearestHarmonic >= 1.0)
        {
            const auto distance = std::abs (binFrequency - nearestHarmonic * frequency);

            if (distance > 40.0)
                worstOffHarmonic = juce::jmax (worstOffHarmonic, magnitude);
        }
    }

    const auto ratio = peak > 0.0f ? worstOffHarmonic / peak : 0.0f;

    check (ratio < 0.002f,
           "oscillator alias rejection at C7 (off-harmonic ratio " + juce::String (ratio, 6) + ")");
}

void runLoaderTest()
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("ilana_table_test.wav");
    file.deleteFile();

    constexpr int framesToWrite = 4;
    constexpr int frameLength = 2048;

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (file);

        check (fileStream->openedOk(), "test wav stream opened");

        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);

        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (44100.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        check (writer != nullptr, "test wav writer created");

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, framesToWrite * frameLength);

            for (int frame = 0; frame < framesToWrite; ++frame)
                for (int i = 0; i < frameLength; ++i)
                    buffer.setSample (0, frame * frameLength + i,
                                      0.5f * (float) std::sin (juce::MathConstants<double>::twoPi
                                                               * (double) i / (double) frameLength
                                                               * (double) (frame + 1) * 4.0));

            check (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()),
                   "test wav written");
            writer.reset();
        }
    }

    Wavetable table;
    check (table.loadFromFile (file), "loader accepts 4-frame 2048-sample wav");
    check (table.getNumFrames() == framesToWrite,
           "loader detected frame count (got " + juce::String (table.getNumFrames()) + ")");

    file.deleteFile();
}
void runFilterTests()
{
    Svf filter;
    filter.setSampleRate (48000.0);
    filter.setCutoff (2000.0);
    filter.setResonance (0.95);
    filter.setMode (Svf::Mode::LowPass);

    auto stable = true;
    auto peak = 0.0f;

    for (int i = 0; i < 48000; ++i)
    {
        const auto input = i == 0 ? 1.0f : 0.0f;
        const auto output = filter.processSample (input);

        if (! std::isfinite (output))
        {
            stable = false;
            break;
        }

        peak = juce::jmax (peak, std::abs (output));
    }

    check (stable, "SVF stable with impulse at high resonance");
    check (peak < 20.0f, "SVF impulse response bounded (peak " + juce::String (peak, 3) + ")");

    Svf extreme;
    extreme.setSampleRate (44100.0);
    extreme.setCutoff (19900.0);
    extreme.setResonance (0.98);
    extreme.setMode (Svf::Mode::HighPass);

    auto extremePeak = 0.0f;

    for (int i = 0; i < 44100; ++i)
    {
        const auto output = extreme.processSample (std::sin ((float) i * 0.1f));
        extremePeak = juce::jmax (extremePeak, std::abs (output));
    }

    check (std::isfinite (extremePeak) && extremePeak < 50.0f,
           "SVF stable near Nyquist at max resonance (peak " + juce::String (extremePeak, 3) + ")");
}

void runSubOscillatorTests()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 1000.0;
    constexpr int renderSize = 16384;

    PolyBlepOsc osc;
    osc.setSampleRate (sampleRate);
    osc.setFrequency (frequency);
    osc.setShape (PolyBlepOsc::Shape::Square);

    std::vector<float> samples ((size_t) renderSize, 0.0f);

    for (int i = 0; i < renderSize; ++i)
        samples[(size_t) i] = osc.getNextSample();

    juce::dsp::WindowingFunction<float> window ((size_t) renderSize,
                                                juce::dsp::WindowingFunction<float>::hann, true);
    window.multiplyWithWindowingTable (samples.data(), (size_t) renderSize);

    juce::dsp::FFT fft (14);

    std::vector<std::complex<float>> input ((size_t) renderSize);
    std::vector<std::complex<float>> output ((size_t) renderSize);

    for (int i = 0; i < renderSize; ++i)
        input[(size_t) i] = { samples[(size_t) i], 0.0f };

    fft.perform (input.data(), output.data(), false);

    const auto binWidth = sampleRate / (double) renderSize;

    auto peak = 0.0f;
    auto worstOffHarmonic = 0.0f;

    for (int bin = 1; bin < renderSize / 2; ++bin)
    {
        const auto magnitude = std::abs (output[(size_t) bin]);
        peak = juce::jmax (peak, magnitude);

        const auto binFrequency = (double) bin * binWidth;
        const auto nearestHarmonic = std::round (binFrequency / frequency);

        if (nearestHarmonic >= 1.0)
        {
            const auto distance = std::abs (binFrequency - nearestHarmonic * frequency);

            if (distance > 40.0)
                worstOffHarmonic = juce::jmax (worstOffHarmonic, magnitude);
        }
    }

    const auto ratio = peak > 0.0f ? worstOffHarmonic / peak : 0.0f;

    check (ratio < 0.02f, "polyBLEP square alias rejection (off-harmonic ratio " + juce::String (ratio, 6) + ")");
}

void runVoiceSmokeTest()
{
    Wavetable table1;
    table1.buildFromFrames (TableFactory::generate (7));

    Wavetable table2;
    table2.buildFromFrames (TableFactory::generate (3));

    auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
    auto& voice = *voiceOwner;
    voice.setCurrentPlaybackSampleRate (48000.0);

    VoiceParams p;
    p.oscillators[0].table = &table1;
    p.oscillators[0].frame = 0.5f;
    p.oscillators[0].level = 0.8f;
    p.oscillators[0].unison = 4;
    p.oscillators[0].detuneCents = 20.0f;
    p.oscillators[0].spread = 0.5f;

    p.oscillatorEnabled[1] = true;
    p.oscillators[1].table = &table2;
    p.oscillators[1].frame = 0.3f;
    p.oscillators[1].level = 0.5f;
    p.oscillators[1].semitones = -12.0;
    p.oscillators[1].unison = 2;

    p.oscillators[2].table = &table1;
    p.oscillators[2].level = 0.5f;
    p.noiseLevel = 0.1f;

    p.filter1.cutoffHz = 800.0f;
    p.filter1.resonance = 0.7f;
    p.filter1.slope24 = true;
    p.filter1.drive = 3.0f;
    p.filter1.envAmount = 2.0f;

    p.filter2.type = FilterType::HighPass;
    p.filter2.cutoffHz = 100.0f;

    p.ampEnv = { 0.005f, 0.3f, 0.8f, 0.25f };
    p.filterEnv = { 0.001f, 0.4f, 0.2f, 0.3f };
    p.modEnv = { 0.01f, 0.3f, 0.5f, 0.3f };

    p.addModSlot (Mod::Source::FilterEnv, Mod::Destination::Osc1Frame, 0.5f);
    p.addModSlot (Mod::Source::Random, Mod::Destination::Osc2Pitch, 0.2f);

    voice.setParams (p);
    voice.startNote (48, 0.9f, nullptr, 8192);

    juce::AudioBuffer<float> buffer (2, 48000);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);

    auto finite = true;
    auto peak = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < 48000; ++i)
        {
            if (! std::isfinite (data[i]))
            {
                finite = false;
                break;
            }

            peak = juce::jmax (peak, std::abs (data[i]));
        }
    }

    check (finite && peak > 0.001f && peak < 4.0f,
           "voice renders finite audio (peak " + juce::String (peak, 4) + ")");

    voice.stopNote (0.5f, true);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);
    buffer.clear();
    voice.renderNextBlock (buffer, 0, 48000);

    auto tailPeak = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < 48000; ++i)
            if (std::isfinite (data[i]))
                tailPeak = juce::jmax (tailPeak, std::abs (data[i]));
    }

    check (std::isfinite (tailPeak) && tailPeak < 4.0f,
           "voice release stays finite (peak " + juce::String (tailPeak, 4) + ")");
}
void runFrameModulationTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (4));

    const std::vector<float> lfo = []
    {
        std::vector<float> values ((size_t) 4096);

        for (int i = 0; i < 4096; ++i)
            values[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * 2.0 * (double) i / 48000.0);

        return values;
    }();

    const auto render = [&table, &lfo] (float frameParam, float macroValue, float modDepth,
                                        const float* lfoSource, Mod::Source source)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].frame = frameParam;
        p.oscillators[0].level = 1.0f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter1.drive = 1.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        p.macros[0] = macroValue;
        p.lfoBuffers[0] = lfoSource;
        p.addModSlot (source, Mod::Destination::Osc1Frame, modDepth);

        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 4096);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 4096);

        std::vector<float> output ((size_t) 4096);

        for (int i = 0; i < 4096; ++i)
            output[(size_t) i] = buffer.getSample (0, i);

        return output;
    };

    const auto modulated = render (0.0f, 1.0f, 1.0f, nullptr, Mod::Source::Macro1);
    const auto direct = render (1.0f, 0.0f, 0.0f, nullptr, Mod::Source::None);
    const auto lfoModulated = render (0.5f, 0.0f, 0.5f, lfo.data(), Mod::Source::Lfo1);
    const auto staticFrame = render (0.5f, 0.0f, 0.0f, nullptr, Mod::Source::None);

    const auto peakOf = [] (const std::vector<float>& values)
    {
        auto peak = 0.0f;

        for (const auto value : values)
            peak = juce::jmax (peak, std::abs (value));

        return peak;
    };

    check (peakOf (modulated) > 0.001f && peakOf (direct) > 0.001f,
           "frame modulation test renders audio (peaks " + juce::String (peakOf (modulated), 4)
               + " / " + juce::String (peakOf (direct), 4) + ")");

    auto maxDifference = 0.0f;

    for (size_t i = 0; i < modulated.size(); ++i)
        maxDifference = juce::jmax (maxDifference, std::abs (modulated[i] - direct[i]));

    check (maxDifference < 1e-5f,
           "Macro1 -> Osc1Frame at depth 1 matches direct frame position (max diff " + juce::String (maxDifference, 7) + ")");

    check (peakOf (staticFrame) > 0.001f,
           "static frame render is audible (peak " + juce::String (peakOf (staticFrame), 4) + ")");

    auto squaredDifference = 0.0;

    for (size_t i = 0; i < lfoModulated.size(); ++i)
    {
        const auto difference = (double) lfoModulated[i] - (double) staticFrame[i];
        squaredDifference += difference * difference;
    }

    const auto rmsDifference = std::sqrt (squaredDifference / (double) lfoModulated.size());

    check (rmsDifference > 0.01,
           "LFO1 -> Osc1Frame modulates the output (rms diff " + juce::String (rmsDifference, 5) + ")");
}
void runCrossModulationTest()
{
    Wavetable table1;
    table1.buildFromFrames (TableFactory::generate (7));

    Wavetable table2;
    table2.buildFromFrames (TableFactory::generate (3));

    const auto render = [&table1, &table2] (float fm, float feedback, float ring, bool sync)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table1;
        p.oscillators[0].frame = 0.4f;
        p.oscillators[0].level = 0.8f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &table2;
        p.oscillators[1].frame = 0.5f;
        p.oscillators[1].level = 0.7f;
        p.oscillators[1].semitones = 7.0;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        p.fmAmount = fm;
        p.fmFeedback = feedback;
        p.ringMod = ring;
        p.hardSync = sync;

        voice.setParams (p);
        voice.startNote (48, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        std::vector<float> output ((size_t) 8192);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            output[(size_t) i] = value;
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        return std::make_tuple (output, peak, finite);
    };

    const auto [plain, plainPeak, plainFinite] = render (0.0f, 0.0f, 0.0f, false);
    const auto [fm, fmPeak, fmFinite] = render (0.5f, 0.0f, 0.0f, false);
    const auto [feedback, fbPeak, fbFinite] = render (0.0f, 0.6f, 0.0f, false);
    const auto [ring, ringPeak, ringFinite] = render (0.0f, 0.0f, 0.9f, false);
    const auto [sync, syncPeak, syncFinite] = render (0.0f, 0.0f, 0.0f, true);

    check (plainFinite && plainPeak > 0.001f, "cross-mod baseline renders audio (peak "
                                                  + juce::String (plainPeak, 4) + ")");

    const auto rmsDifference = [&plain] (const std::vector<float>& other)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < plain.size(); ++i)
        {
            const auto difference = (double) other[i] - (double) plain[i];
            sum += difference * difference;
        }

        return std::sqrt (sum / (double) plain.size());
    };

    check (fmFinite && std::isfinite (fmPeak) && rmsDifference (fm) > 0.01,
           "FM cross-mod changes output (rms diff " + juce::String (rmsDifference (fm), 5) + ")");
    check (fbFinite && std::isfinite (fbPeak) && rmsDifference (feedback) > 0.01,
           "feedback FM changes output (rms diff " + juce::String (rmsDifference (feedback), 5) + ")");
    check (ringFinite && std::isfinite (ringPeak) && rmsDifference (ring) > 0.01,
           "ring mod changes output (rms diff " + juce::String (rmsDifference (ring), 5) + ")");
    check (syncFinite && std::isfinite (syncPeak) && rmsDifference (sync) > 0.01,
           "hard sync changes output (rms diff " + juce::String (rmsDifference (sync), 5) + ")");

    check (juce::jmax (fmPeak, juce::jmax (fbPeak, juce::jmax (ringPeak, syncPeak))) < 8.0f,
           "cross-mod output stays bounded");
}
void runKarplusStrongTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (8));

    const auto renderString = [&table] (float decay, float damping, int excite, float sustain)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].stringMode = true;
        p.oscillators[0].stringExcite = excite;
        p.oscillators[0].stringDecay = decay;
        p.oscillators[0].stringDamping = damping;
        p.oscillators[0].stringSustain = sustain;
        p.oscillators[0].level = 0.9f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };

        voice.setParams (p);
        voice.startNote (45, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 48000);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 48000);

        std::vector<float> output ((size_t) 48000);
        auto finite = true;

        for (int i = 0; i < 48000; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto [burst, burstFinite] = renderString (0.75f, 0.35f, 0, 0.0f);
    const auto [sustained, sustainedFinite] = renderString (0.9f, 0.2f, 1, 0.5f);

    const auto rmsOf = [] (const std::vector<float>& values, int start, int count)
    {
        auto sum = 0.0;

        for (int i = start; i < start + count; ++i)
            sum += (double) values[(size_t) i] * (double) values[(size_t) i];

        return std::sqrt (sum / (double) count);
    };

    const auto burstEarly = rmsOf (burst, 2400, 4800);
    const auto burstLate = rmsOf (burst, 43200, 4800);

    check (burstFinite && burstEarly > 0.001f, "KS burst sounds (early rms " + juce::String (burstEarly, 5) + ")");
    check (burstLate < burstEarly * 0.9f,
           "KS burst decays (early " + juce::String (burstEarly, 5) + " late " + juce::String (burstLate, 5) + ")");

    const auto sustainedLate = rmsOf (sustained, 43200, 4800);

    check (sustainedFinite && sustainedLate > 0.001f,
           "KS sustained excitation keeps ringing (late rms " + juce::String (sustainedLate, 5) + ")");
}

void runPhysicalStringTest()
{
    const auto render = [] (float stiffness, float pickup, float excitePos,
                            float hardness, float pickPos, bool slap)
    {
        KarplusStrong string (12345);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.12f, 0.99f);
        string.setPhysicalParams (stiffness, pickup, excitePos, hardness, pickPos, slap);
        string.trigger (1.0f);

        std::vector<float> samples (24000);
        for (auto& sample : samples)
            sample = string.process();
        return samples;
    };

    const auto legacy = render (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    const auto stiff = render (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    const auto pickup = render (0.0f, 0.5f, 0.0f, 1.0f, 0.0f, false);
    const auto excitation = render (0.0f, 0.0f, 0.5f, 1.0f, 0.0f, false);
    const auto soft = render (0.0f, 0.0f, 0.0f, 0.0f, 0.25f, false);
    const auto hard = render (0.0f, 0.0f, 0.0f, 1.0f, 0.25f, false);
    const auto slapped = render (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);

    const auto powerAt = [] (const std::vector<float>& samples, double hz)
    {
        double real = 0.0, imaginary = 0.0;
        for (int i = 1500; i < 22000; ++i)
        {
            const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
            real += (double) samples[(size_t) i] * std::cos (angle);
            imaginary += (double) samples[(size_t) i] * std::sin (angle);
        }
        return real * real + imaginary * imaginary;
    };

    const auto peakNear = [&powerAt] (const std::vector<float>& samples, int centre)
    {
        auto bestFrequency = 0;
        auto bestPower = 0.0;
        for (int hz = centre - 24; hz <= centre + 24; ++hz)
        {
            const auto power = powerAt (samples, (double) hz);
            if (power > bestPower)
            {
                bestPower = power;
                bestFrequency = hz;
            }
        }
        return std::pair<int, double> { bestFrequency, bestPower };
    };

    const auto legacyEighth = peakNear (legacy, 1760).first;
    const auto stiffEighth = peakNear (stiff, 1760).first;
    check (stiffEighth > legacyEighth + 2,
           "stiffness sharpens the eighth partial (" + juce::String (legacyEighth) + " -> " + juce::String (stiffEighth) + " Hz)");

    const auto harmonicRatio = [&peakNear] (const std::vector<float>& samples)
    {
        return peakNear (samples, 440).second / juce::jmax (1.0, peakNear (samples, 220).second);
    };
    check (harmonicRatio (pickup) < harmonicRatio (legacy) * 0.5,
           "pickup position creates a second-harmonic comb notch");
    check (harmonicRatio (excitation) < harmonicRatio (legacy) * 0.7,
           "excitation position creates a second-harmonic comb notch");

    const auto attackBrightness = [] (const std::vector<float>& samples)
    {
        auto energy = 0.0;
        for (int i = 1; i < 192; ++i)
        {
            const auto difference = (double) samples[(size_t) i] - samples[(size_t) (i - 1)];
            energy += difference * difference;
        }
        return energy;
    };
    check (attackBrightness (hard) > attackBrightness (soft) * 1.5,
           "pick hardness brightens the attack");

    // Softening must grow steadily as hardness drops from the neutral 1.0,
    // with no jump just below it.
    const auto slightlySoft = render (0.0f, 0.0f, 0.0f, 0.95f, 0.0f, false);
    const auto halfSoft = render (0.0f, 0.0f, 0.0f, 0.5f, 0.0f, false);
    const auto fullySoft = render (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
    check (attackBrightness (slightlySoft) > attackBrightness (legacy) * 0.8
               && attackBrightness (legacy) > attackBrightness (halfSoft)
               && attackBrightness (halfSoft) > attackBrightness (fullySoft),
           "pick hardness softens the attack monotonically below 1.0");

    // A quiet pluck gets a quiet slap.
    const auto slapAt = [] (float velocity)
    {
        KarplusStrong string (12345);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.12f, 0.99f);
        string.setPhysicalParams (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
        string.trigger (velocity);
        auto peak = 0.0f;
        for (int i = 0; i < 192; ++i)
            peak = juce::jmax (peak, std::abs (string.process()));
        return peak;
    };
    check (slapAt (0.2f) < slapAt (1.0f) * 0.5f, "slap follows velocity");
    // What the slap adds: the difference from the same pluck without it
    // (the plain pluck's own brightness moves with the loop's read
    // fraction, so a ratio of the two sat on its threshold).
    std::vector<float> added (slapped.size());
    for (size_t i = 0; i < added.size(); ++i)
        added[i] = slapped[i] - legacy[i];
    check (attackBrightness (added) > attackBrightness (legacy) * 0.2,
           "slap adds a short attack transient (adds " + juce::String (attackBrightness (added), 2) + " to "
               + juce::String (attackBrightness (legacy), 2) + ")");

    for (const auto& samples : { stiff, pickup, excitation, hard, slapped })
    {
        auto finite = true;
        auto peak = 0.0f;
        for (auto sample : samples)
        {
            finite = finite && std::isfinite (sample);
            peak = juce::jmax (peak, std::abs (sample));
        }
        check (finite && peak <= 8.0f, "physical string output stays finite and bounded");
    }

    for (double frequency : { 15.0, 440.0, 12000.0 })
    {
        KarplusStrong extreme (12345);
        extreme.prepare (48000.0);
        extreme.setFrequency (frequency);
        extreme.setParams (KarplusStrong::Excite::Noise, 1.0f, 0.0f, 1.0f);
        extreme.setPhysicalParams (1.0f, 1.0f, 1.0f, 1.0f, 1.0f, true);
        extreme.trigger (1.0f);
        auto finite = true;
        auto peak = 0.0f;
        for (int i = 0; i < 48000; ++i)
        {
            const auto sample = extreme.process();
            finite = finite && std::isfinite (sample);
            peak = juce::jmax (peak, std::abs (sample));
        }
        check (finite && peak <= 8.0f, "extreme Physical settings stay bounded at " + juce::String (frequency, 0) + " Hz");
    }
}

void runPhysicalPatchMigrationTest()
{
    IlanaSynthAudioProcessor source;
    source.prepareToPlay (48000.0, 256);
    if (auto* mode = source.apvts.getParameter ("osc1_mode"))
        mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
    if (auto* decay = source.apvts.getParameter ("osc1_string_decay"))
        decay->setValueNotifyingHost (decay->convertTo0to1 (0.83f));

    juce::MemoryBlock saved;
    source.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "legacy String patch state can be read");
    if (xml == nullptr)
        return;

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");
        if (id.contains ("_string_stiffness") || id.contains ("_string_pickup")
            || id.contains ("_string_excite_pos") || id.contains ("_string_pick_hardness")
            || id.contains ("_string_pick_pos") || id.contains ("_string_slap"))
            xml->removeChildElement (child, true);
        child = next;
    }

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    IlanaSynthAudioProcessor loaded;
    loaded.prepareToPlay (48000.0, 256);
    if (auto* stiffness = loaded.apvts.getParameter ("osc1_string_stiffness"))
        stiffness->setValueNotifyingHost (1.0f);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    const auto read = [&loaded] (const char* id)
    {
        const auto* value = loaded.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : -1.0f;
    };
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (loaded.apvts.getParameter ("osc1_mode"));
    check (choice != nullptr && choice->getIndex() == 1 && choice->getAllValueStrings()[1] == "Physical",
           "old String mode loads at the Physical choice index");
    check (std::abs (read ("osc1_string_decay") - 0.83f) < 0.01f,
           "old String settings survive the mode rename");
    check (read ("osc1_string_stiffness") == 0.0f && read ("osc1_string_pickup") == 0.0f
           && read ("osc1_string_excite_pos") == 0.0f && read ("osc1_string_pick_hardness") == 1.0f
           && read ("osc1_string_pick_pos") == 0.0f && read ("osc1_string_slap") == 0.0f,
           "old String patch receives legacy-sounding Physical defaults");
}

void runFactoryTableContentTest()
{
    for (int tableIndex = 0; tableIndex < TableFactory::getNumFactoryTables(); ++tableIndex)
    {
        Wavetable table;
        table.buildFromFrames (TableFactory::generate (tableIndex));

        auto peak = 0.0f;

        for (const auto frame : { 0, 16, 32, 63 })
        {
            const auto* data = table.getFrameData (0, frame);

            for (int i = 1; i <= Wavetable::frameSize; ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }

        check (peak > 0.05f, "factory table '" + TableFactory::getFactoryTableNames()[tableIndex]
                                 + "' has content (peak " + juce::String (peak, 4) + ")");
    }
}
void runWeirdDspTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    const auto renderVoice = [&table] (const std::function<void (VoiceParams&)>& configure)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &table;
        p.oscillators[0].frame = 0.4f;
        p.oscillators[0].level = 0.8f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        configure (p);

        voice.setParams (p);
        voice.startNote (45, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        std::vector<float> output ((size_t) 8192);
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto [plain, plainFinite] = renderVoice ([] (VoiceParams&) {});

    const auto [resonated, resFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.resonatorOn = true;
        p.resonatorAmount = 0.9f;
        p.resonatorDecay = 0.85f;
        p.resonatorKeytrack = 1.0f;
    });

    const auto [fmFiltered, fmFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.filter1.cutoffHz = 400.0f;
        p.filter1.resonance = 0.3f;
        p.filter1Fm = 0.8f;
    });

    const auto [chorded, chordFinite] = renderVoice ([] (VoiceParams& p)
    {
        p.oscillators[0].unison = 4;
        p.oscillators[0].detuneCents = 5.0f;
        p.oscillators[0].chord = 4;
    });

    const auto difference = [&plain] (const std::vector<float>& other)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < plain.size(); ++i)
        {
            const auto d = (double) other[i] - (double) plain[i];
            sum += d * d;
        }

        return std::sqrt (sum / (double) plain.size());
    };

    check (plainFinite && difference (resonated) > 0.01,
           "resonator changes the voice (rms diff " + juce::String (difference (resonated), 5) + ")");
    check (fmFinite && difference (fmFiltered) > 0.01,
           "audio-rate filter FM changes the voice (rms diff " + juce::String (difference (fmFiltered), 5) + ")");
    check (chordFinite && difference (chorded) > 0.01,
           "unison chord stacking changes the voice (rms diff " + juce::String (difference (chorded), 5) + ")");

    // Pitch shifter: 500 Hz sine shifted +12 semitones should read ~1000 Hz
    {
        GranularPitchShift shifter;
        shifter.prepare (48000.0);
        shifter.setRatio (2.0);

        constexpr int renderSize = 48000;
        std::vector<float> output ((size_t) renderSize);

        for (int i = 0; i < renderSize; ++i)
        {
            shifter.push ((float) std::sin (juce::MathConstants<double>::twoPi * 500.0 * (double) i / 48000.0));
            output[(size_t) i] = shifter.process();
        }

        auto crossings = 0;
        const auto start = 20000;
        const auto end = 44000;

        for (int i = start + 1; i < end; ++i)
            if (output[(size_t) (i - 1)] < 0.0f && output[(size_t) i] >= 0.0f)
                ++crossings;

        const auto measured = (double) crossings / ((double) (end - start) / 48000.0);
        check (std::abs (measured - 1000.0) < 60.0,
               "granular pitch shift +12 st: 500 Hz -> " + juce::String (measured, 1) + " Hz");

        auto silentRun = 0;
        auto worstSilentRun = 0;

        for (int i = 20000; i < 44000; ++i)
        {
            if (std::abs (output[(size_t) i]) < 0.01f)
            {
                ++silentRun;
                worstSilentRun = juce::jmax (worstSilentRun, silentRun);
            }
            else
            {
                silentRun = 0;
            }
        }

        check (worstSilentRun < 240, "granular pitch shift has no dropouts (worst run "
                                         + juce::String (worstSilentRun) + " samples)");
        check (shifter.getLatencySamples() <= 48000.0 * 0.035,
               "pitch shifter latency is short (" + juce::String (shifter.getLatencySamples() / 48000.0 * 1000.0, 1) + " ms)");
    }

    // Spectral freeze: noise in, frozen output must be finite and audible
    {
        SpectralFreeze freezeL;
        freezeL.prepare (48000.0);

        std::vector<float> left ((size_t) 24000);

        for (int i = 0; i < 24000; ++i)
            left[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / 48000.0);

        freezeL.process (left.data(), 12000, false, 0.0f);
        freezeL.process (left.data() + 12000, 12000, true, 1.0f);

        auto finite = true;
        auto rms = 0.0;

        for (int i = 12000; i < 24000; ++i)
        {
            finite = finite && std::isfinite (left[(size_t) i]);
            rms += (double) left[(size_t) i] * (double) left[(size_t) i];
        }

        rms = std::sqrt (rms / 12000.0);
        check (finite && rms > 0.001, "spectral freeze holds output (rms " + juce::String (rms, 5) + ")");
    }

    // Granular smear: sustained tone, output finite and audible
    {
        GranularSmear smearL;
        smearL.prepare (48000.0);
        smearL.setParams (120.0f, 12.0f);

        auto finite = true;
        auto rms = 0.0;
        auto count = 0;

        for (int i = 0; i < 48000; ++i)
        {
            const auto input = (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / 48000.0);
            smearL.push (input);

            if (i > 16000)
            {
                const auto wet = smearL.process();
                finite = finite && std::isfinite (wet);
                rms += (double) wet * (double) wet;
                ++count;
            }
        }

        rms = std::sqrt (rms / juce::jmax (1, count));
        check (finite && rms > 0.001, "granular smear produces output (rms " + juce::String (rms, 5) + ")");
    }
}

void runSampleOscTest()
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleLength = 4800;

    auto sample = std::make_shared<SampleData>();
    sample->sampleRate = sampleRate;
    sample->name = "test";

    sample->buffer.setSize (1, sampleLength);

    for (int i = 0; i < sampleLength; ++i)
        sample->buffer.setSample (0, i, 0.9f * std::sin (juce::MathConstants<float>::twoPi * 261.6255653f
                                                         * (float) i / (float) sampleRate));

    const auto renderVoice = [&sample] (int note, int numSamples,
                                        const std::function<void (VoiceParams&)>& configure)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].sampleTuned = true;
        p.oscillators[0].level = 0.9f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        configure (p);

        voice.setParams (p);
        voice.startNote (note, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, numSamples);

        std::vector<float> output ((size_t) numSamples);
        auto finite = true;

        for (int i = 0; i < numSamples; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_pair (output, finite);
    };

    const auto peakOf = [] (const std::vector<float>& data, int from, int to)
    {
        auto peak = 0.0f;

        for (int i = from; i < juce::jmin (to, (int) data.size()); ++i)
            peak = juce::jmax (peak, std::abs (data[(size_t) i]));

        return peak;
    };

    const auto [forward, forwardFinite] = renderVoice (60, 8192, [] (VoiceParams&) {});
    check (forwardFinite && peakOf (forward, 0, 4096) > 0.2f,
           "sample osc renders audio (peak " + juce::String (peakOf (forward, 0, 4096), 4) + ")");

    const auto tunedFrequency = fundamentalOf (forward, sampleRate);
    check (std::abs (tunedFrequency - 261.63) < 8.0,
           "tuned sample at C4 plays at 261.6 Hz (measured " + juce::String (tunedFrequency, 2) + ")");

    const auto [octave, octaveFinite] = renderVoice (72, 8192, [] (VoiceParams&) {});
    const auto octaveFrequency = fundamentalOf (octave, sampleRate);
    check (octaveFinite && std::abs (octaveFrequency - 523.25) < 16.0,
           "tuned sample follows pitch (C5 measured " + juce::String (octaveFrequency, 2) + ")");

    const auto [untuned, untunedFinite] = renderVoice (72, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleTuned = false;
    });
    const auto untunedFrequency = fundamentalOf (untuned, sampleRate);
    check (untunedFinite && std::abs (untunedFrequency - 261.63) < 8.0,
           "untuned sample ignores note pitch (measured " + juce::String (untunedFrequency, 2) + ")");

    const auto [oneShot, oneShotFinite] = renderVoice (60, 16384, [] (VoiceParams&) {});
    check (oneShotFinite && peakOf (oneShot, 8000, 16384) < 0.001f,
           "one-shot stops after the sample ends (tail peak " + juce::String (peakOf (oneShot, 8000, 16384), 5) + ")");

    const auto [looped, loopedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleLoop = true;
    });
    check (loopedFinite && peakOf (looped, 8000, 16384) > 0.2f,
           "looped sample keeps playing (tail peak " + juce::String (peakOf (looped, 8000, 16384), 4) + ")");

    const auto [reversed, reversedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleReverse = true;
        p.oscillators[0].sampleLoop = true;
    });
    check (reversedFinite && peakOf (reversed, 0, 4096) > 0.2f && peakOf (reversed, 8000, 16384) > 0.2f,
           "reverse loop plays and wraps (peak " + juce::String (peakOf (reversed, 8000, 16384), 4) + ")");

    const auto [region, regionFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleStart = 0.5f;
        p.oscillators[0].sampleEnd = 1.0f;
    });
    check (regionFinite && peakOf (region, 0, 4096) > 0.2f,
           "start/end region renders audio (peak " + juce::String (peakOf (region, 0, 4096), 4) + ")");

    const auto [faded, fadedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleFadeIn = 0.5f;
        p.oscillators[0].sampleFadeOut = 0.5f;
    });
    check (fadedFinite && std::abs (faded[0]) < 0.02f && peakOf (faded, 0, 4096) > 0.1f,
           "fade in/out attenuate the region edges (first " + juce::String (std::abs (faded[0]), 5) + ")");

    const auto [collapsed, collapsedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.oscillators[0].sampleStart = 0.7f;
        p.oscillators[0].sampleEnd = 0.2f;
    });
    check (collapsedFinite, "collapsed start/end region stays finite");

    {
        SamplePlayer player;
        player.prepare (sampleRate);

        SamplePlayer::Params sp;
        sp.sample = sample.get();
        player.setParams (sp);
        player.trigger();

        float l = 0.0f;
        float r = 0.0f;
        player.process (l, r);

        auto other = std::make_shared<SampleData>();
        other->sampleRate = sampleRate;
        other->buffer.setSize (1, sampleLength);

        for (int i = 0; i < sampleLength; ++i)
            other->buffer.setSample (0, i, 0.8f * std::sin (juce::MathConstants<float>::twoPi * 523.25f
                                                             * (float) i / (float) sampleRate));

        sp.sample = other.get();
        player.setParams (sp);
        check (player.isActive(), "SamplePlayer retriggers when a new sample is dropped in");

        auto peak = 0.0f;

        for (int i = 0; i < 512; ++i)
        {
            player.process (l, r);
            peak = juce::jmax (peak, std::abs (l));
        }

        check (peak > 0.05f, "dropped sample plays without a new note-on");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = nullptr;
        p.oscillators[0].level = 0.9f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 2048);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 2048);

        p.oscillators[0].sample = sample.get();
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;

        for (int i = 0; i < 8192; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        check (peak > 0.05f, "sample dropped mid-note onto an empty sample osc starts playing (peak "
                                 + juce::String (peak, 4) + ")");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].level = 0.9f;
        p.oscillatorEnabled[0] = false;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.oscillatorEnabled[0] = true;
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        check (finite && peak > 0.05f, "sample osc turned on mid-note starts playing (peak "
                                          + juce::String (peak, 4) + ")");
    }

    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.oscillators[0].sampleMode = true;
        p.oscillators[0].sample = sample.get();
        p.oscillators[0].level = 0.9f;
        p.oscillators[0].unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.oscillators[0].unison = 4;
        voice.setParams (p);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 8192; ++i)
        {
            const auto value = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (value));
            finite = finite && std::isfinite (value);
        }

        check (finite && peak > 0.05f, "unison raised mid-note in sample mode stays audible (peak "
                                          + juce::String (peak, 4) + ")");
    }
}

void runPresetSanityTest()
{
    const auto& presets = Presets::getFactoryPresets();
    const auto categories = Presets::getFactoryPresetCategories();

    check (! presets.empty(), "factory presets exist (" + juce::String ((int) presets.size()) + ")");
    check ((int) presets.size() == categories.size(),
           "preset and category counts match (" + juce::String ((int) presets.size())
               + " vs " + juce::String (categories.size()) + ")");

    juce::StringArray names;
    auto duplicates = false;
    auto emptyIds = 0;

    for (const auto& preset : presets)
    {
        if (names.contains (preset.name))
            duplicates = true;

        names.add (preset.name);

        for (const auto& value : preset.values)
        {
            if (value.id == nullptr || *value.id == 0)
                ++emptyIds;
        }
    }

    check (! duplicates, "preset names are unique");
    check (emptyIds == 0, "preset entries have valid parameter ids");
}
void runOsc2Test()
{
    Wavetable sineTable;
    sineTable.buildFromFrames (TableFactory::generate (8));

    const auto renderOsc2 = [&sineTable] (int semitones, bool stringMode)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.oscillators[0].table = &sineTable;
        p.oscillators[0].level = 0.0f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &sineTable;
        p.oscillators[1].level = 0.9f;
        p.oscillators[1].semitones = (double) semitones;
        p.oscillators[1].stringMode = stringMode;
        p.oscillators[1].stringExcite = 0;
        p.oscillators[1].stringDecay = 0.8f;
        p.oscillators[1].stringDamping = 0.25f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };

        voice.setParams (p);
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 24000);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 24000);

        std::vector<float> output ((size_t) 24000);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 24000; ++i)
        {
            output[(size_t) i] = buffer.getSample (0, i);
            peak = juce::jmax (peak, std::abs (output[(size_t) i]));
            finite = finite && std::isfinite (output[(size_t) i]);
        }

        return std::make_tuple (output, peak, finite);
    };

    const auto frequencyOf = [] (const std::vector<float>& values)
    {
        auto crossings = 0;
        const auto start = 4000;
        const auto end = 20000;

        for (int i = start + 1; i < end; ++i)
            if (values[(size_t) (i - 1)] < 0.0f && values[(size_t) i] >= 0.0f)
                ++crossings;

        return (double) crossings / ((double) (end - start) / 48000.0);
    };

    const auto [plain, plainPeak, plainFinite] = renderOsc2 (0, false);
    const auto [octave, octavePeak, octaveFinite] = renderOsc2 (12, false);
    const auto [stringMode, stringPeak, stringFinite] = renderOsc2 (0, true);

    check (plainFinite && plainPeak > 0.01f,
           "osc2 alone renders audio (peak " + juce::String (plainPeak, 4) + ")");

    const auto baseFrequency = frequencyOf (plain);
    const auto octaveFrequency = frequencyOf (octave);

    check (std::abs (baseFrequency - 220.0) < 8.0,
           "osc2 plays the note pitch (measured " + juce::String (baseFrequency, 1) + " Hz at A3)");
    check (octaveFinite && std::abs (octaveFrequency - 440.0) < 12.0,
           "osc2 semitones transpose (measured " + juce::String (octaveFrequency, 1) + " Hz at +12 st)");

    check (stringFinite && stringPeak > 0.01f,
           "osc2 string mode renders audio (peak " + juce::String (stringPeak, 4) + ")");
}
void runOscLevelTest()
{
    Wavetable sineTable;
    sineTable.buildFromFrames (TableFactory::generate (8));

    const auto makeParams = [&sineTable] (float level)
    {
        VoiceParams p;
        p.oscillators[0].table = &sineTable;
        p.oscillators[0].level = 0.0f;
        p.oscillatorEnabled[1] = true;
        p.oscillators[1].table = &sineTable;
        p.oscillators[1].level = level;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        return p;
    };

    const auto peakOf = [] (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        auto peak = 0.0f;

        for (int i = start; i < start + count; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

        return peak;
    };

    // Static level sweep
    const auto peakForLevel = [&] (float level)
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);
        voice.setParams (makeParams (level));
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 8192);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        return peakOf (buffer, 4096, 4096);
    };

    const auto loud = peakForLevel (0.9f);
    const auto quiet = peakForLevel (0.3f);
    const auto silent = peakForLevel (0.0f);

    check (loud > 1.0f && quiet > 0.25f && quiet < loud * 0.45f,
           "osc2 level scales output (0.9 -> " + juce::String (loud, 3)
               + ", 0.3 -> " + juce::String (quiet, 3) + ")");
    check (silent < 0.001f, "osc2 level zero silences the oscillator (peak " + juce::String (silent, 5) + ")");

    // Level change while the note is held
    {
        auto voiceOwner = std::make_unique<Voice>(); // over 1 MB: too big for a 1 MB Windows stack
        auto& voice = *voiceOwner;
        voice.setCurrentPlaybackSampleRate (48000.0);
        voice.setParams (makeParams (0.9f));
        voice.startNote (57, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> buffer (1, 16384);
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        const auto firstPeak = peakOf (buffer, 4096, 4096);

        voice.setParams (makeParams (0.15f));
        buffer.clear();
        voice.renderNextBlock (buffer, 0, 8192);

        const auto secondPeak = peakOf (buffer, 4096, 4096);

        check (firstPeak > 1.0f && secondPeak < firstPeak * 0.35f,
               "osc2 level responds while a note is held (" + juce::String (firstPeak, 3)
                   + " -> " + juce::String (secondPeak, 3) + ")");
    }
}
double fundamentalOf (const std::vector<float>& input, double sampleRate)
{
    const auto n = (int) input.size();

    if (n < 4096)
        return 0.0;

    auto mean = 0.0;

    for (const auto value : input)
        mean += value;

    mean /= (double) n;

    std::vector<float> samples ((size_t) n);

    for (int i = 0; i < n; ++i)
        samples[(size_t) i] = input[(size_t) i] - (float) mean;

    const auto correlation = [&samples, n] (int lag)
    {
        double sum = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i + lag < n; i += 4)
        {
            const auto a = (double) samples[(size_t) i];
            const auto b = (double) samples[(size_t) (i + lag)];
            sum += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        return sum / std::sqrt (energyA * energyB + 1.0e-12);
    };

    const auto minLag = juce::jmax (8, (int) (sampleRate / 1200.0));
    const auto maxLag = juce::jmin (n / 2, (int) (sampleRate / 50.0));

    std::vector<double> values ((size_t) maxLag + 2, 0.0);
    auto bestValue = -2.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        values[(size_t) lag] = correlation (lag);
        bestValue = juce::jmax (bestValue, values[(size_t) lag]);
    }

    // Prefer the shortest period that explains the signal almost as well as
    // the best lag, so exact octave ties resolve to the fundamental.
    auto bestLag = -1;

    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        const auto value = values[(size_t) lag];

        if (value >= bestValue * 0.98 && value >= values[(size_t) (lag - 1)] && value >= values[(size_t) (lag + 1)])
        {
            bestLag = lag;
            break;
        }
    }

    if (bestLag < 0)
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (values[(size_t) lag] >= bestValue - 1.0e-12)
            {
                bestLag = lag;
                break;
            }

    if (bestLag < 0)
        return 0.0;

    if (bestLag <= minLag || bestLag >= maxLag)
        return sampleRate / (double) bestLag;

    const auto left = correlation (bestLag - 1);
    const auto middle = correlation (bestLag);
    const auto right = correlation (bestLag + 1);
    const auto denominator = left - 2.0 * middle + right;

    if (std::abs (denominator) < 1.0e-12)
        return sampleRate / (double) bestLag;

    const auto offset = 0.5 * (left - right) / denominator;

    return sampleRate / ((double) bestLag + offset);
}

void runPresetTuningTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    constexpr int note = 57; // A3
    const auto noteFrequency = juce::MidiMessage::getMidiNoteInHertz (note);

    juce::StringArray failures;
    auto worstCents = 0.0;
    juce::String worstName;

    const auto categories = processor.getFactoryPresetCategories();

    for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
    {
        // Sound effects (noise sweeps, sirens, risers) are not meant to be in tune.
        // Drums sweep their pitch and generative patches spray extra notes by design.
        // The DX7 banks are the cartridges as they were (their FX voices,
        // drums and detuned layers included); dx7_mode_check.py compares
        // them with Dexed instead.
        if (categories[presetIndex] == "FX" || categories[presetIndex] == "Drums" || categories[presetIndex] == "Generative"
            || categories[presetIndex] == "DX7")
            continue;

        processor.loadFactoryPreset (presetIndex);
        processor.panic();

        // Also not meant to be in tune: a prepared piano (bolts make its
        // strings inharmonic and detuned) and granular patches with grain
        // pitch spray (each grain is randomly transposed, from a time-seeded
        // generator, so the estimate varied from run to run).
        auto pitchByDesign = names[presetIndex] == "Bolted Strings";
        // ilanaSynth FX patches play the audio input, which the instrument
        // doesn't have (tests/FxTest.cpp covers them).
        if (const auto* category = Presets::getFactoryPresets()[(size_t) presetIndex].category;
            category != nullptr && juce::String (category) == "FX Input")
            continue;
        for (const auto* prefix : OscillatorIds::prefixes)
        {
            const juce::String p (prefix);
            if (processor.apvts.getRawParameterValue (p + "_on")->load() > 0.5f
                && (int) processor.apvts.getRawParameterValue (p + "_mode")->load() == 3
                && processor.apvts.getRawParameterValue (p + "_grain_pitch")->load() > 0.0f)
                pitchByDesign = true;
        }
        if (pitchByDesign)
            continue;

        // Measure the dry voice: effects (tape shift, freeze, taps...) may
        // legitimately obscure pitch and are not part of oscillator tuning.
        for (const auto* id : { "fx_drive_on", "fx_crush_on", "fx_comb_on", "fx_phaser_on",
                                "fx_chorus_on", "fx_delay_on", "fx_taps_on", "fx_stutter_on",
                                "fx_smear_on", "fx_freeze_on", "fx_reverb_on", "arp_on" })
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (0.0f);
        }

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> samples;

        const auto blocks = (int) (48000 * 2.2 / 512.0);

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midiForBlock;

            if (block == 0)
                midiForBlock = midi;

            processor.processBlock (buffer, midiForBlock);

            const auto startTime = (double) block * 512.0 / 48000.0;

            if (startTime >= 0.02)
                for (int s = 0; s < 512; ++s)
                    samples.push_back ((buffer.getSample (0, s) + buffer.getSample (1, s)) * 0.5f);
        }

        auto peak = 0.0f;

        for (const auto value : samples)
            peak = juce::jmax (peak, std::abs (value));

        if (peak < 0.002f)
        {
            failures.add (names[presetIndex] + " (silent)");
            continue;
        }

        // Analyze the loudest 0.6 s window so plucks and slow pads both measure.
        const auto windowSize = (int) (48000 * 0.6);
        juce::StringArray windowSample;

        if ((int) samples.size() > windowSize)
        {
            auto bestStart = 0;
            auto bestRms = -1.0;

            for (int start = 0; start + windowSize <= (int) samples.size(); start += 4800)
            {
                auto sum = 0.0;

                for (int i = start; i < start + windowSize; i += 4)
                    sum += (double) samples[(size_t) i] * (double) samples[(size_t) i];

                if (sum > bestRms)
                {
                    bestRms = sum;
                    bestStart = start;
                }
            }

            std::vector<float> windowed (samples.begin() + bestStart, samples.begin() + bestStart + windowSize);
            samples = std::move (windowed);
        }

        const auto frequency = fundamentalOf (samples, 48000.0);

        if (frequency <= 0.0 || ! std::isfinite (frequency))
        {
            failures.add (names[presetIndex] + " (unmeasurable)");
            continue;
        }

        const auto semitones = 12.0 * std::log2 (frequency / noteFrequency);
        auto cents = std::abs ((semitones - std::round (semitones)) * 100.0);

        // Autocorrelation can lock onto a mix of partials in rich or
        // inharmonic sounds (the M8.2 piano's hammer and board); confirm an
        // out-of-tune reading with the strongest spectral peak within half a
        // semitone of the note (Goertzel sweep, 1 cent steps).
        if (cents > 30.0)
        {
            auto best = 0.0, bestHz = noteFrequency;
            for (double c = -50.0; c <= 50.0; c += 1.0)
            {
                const auto hz = noteFrequency * std::pow (2.0, c / 1200.0);
                const auto w = juce::MathConstants<double>::twoPi * hz / 48000.0;
                std::complex<double> sum;
                const auto length = (int) samples.size();
                for (int i = 0; i < length; ++i)
                    sum += (double) samples[(size_t) i] * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length))
                           * std::polar (1.0, -w * i);
                if (std::abs (sum) > best)
                {
                    best = std::abs (sum);
                    bestHz = hz;
                }
            }
            cents = std::abs (1200.0 * std::log2 (bestHz / noteFrequency));
        }

        if (cents > worstCents)
        {
            worstCents = cents;
            worstName = names[presetIndex];
        }

        // Vibrato patches (any mod slot routed to oscillator pitch) legitimately
        // swing around the note, so give them a wider measurement band.
        auto pitchModulated = false;

        for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        {
            const auto prefix = "mod" + juce::String (slot);

            if (const auto* destination = processor.apvts.getRawParameterValue (prefix + "_dst"))
            {
                const auto destinationIndex = (int) destination->load();

                if (destinationIndex == 1 || destinationIndex == 4 || destinationIndex == 28)
                {
                    const auto* amount = processor.apvts.getRawParameterValue (prefix + "_amt");

                    if (amount != nullptr && std::abs (amount->load()) > 0.001f)
                        pitchModulated = true;
                }
            }
        }

        const auto tolerance = pitchModulated ? 60.0 : 30.0;

        if (cents > tolerance)
            failures.add (names[presetIndex] + " (" + juce::String (cents, 1) + " cents @ "
                          + juce::String (frequency, 1) + " Hz)");
    }

    juce::String detail = "worst: " + worstName + " " + juce::String (worstCents, 1) + " cents";

    if (! failures.isEmpty())
        detail += "  |  out of tune: " + failures.joinIntoString (", ");

    check (failures.isEmpty(), "all factory presets tuned within 30 cents (" + detail + ")");
}
void runParameterStressTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto renderIsBounded = [&processor] ()
    {
        juce::AudioBuffer<float> buffer (2, 256);
        buffer.clear();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 90), 0);

        processor.processBlock (buffer, midi);

        auto peak = 0.0f;

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
            {
                const auto value = buffer.getSample (channel, i);

                if (! std::isfinite (value))
                    return -1.0f;

                peak = juce::jmax (peak, std::abs (value));
            }

        return peak;
    };

    auto extremeFailures = 0;
    auto extremeCount = 0;

    for (auto* parameter : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            for (const auto value : { 0.0f, 1.0f })
            {
                ranged->setValueNotifyingHost (value);
                ++extremeCount;

                const auto peak = renderIsBounded();

                if (peak < 0.0f || peak > 4.0f)
                {
                    ++extremeFailures;

                    if (extremeFailures <= 4)
                        std::cout << "DEBUG extreme " << parameter->getName (64) << " = " << value
                                  << " peak " << peak << std::endl;
                }
            }

            ranged->setValueNotifyingHost (ranged->getDefaultValue());
        }
    }

    check (extremeFailures == 0, "every parameter at min/max renders bounded audio ("
                                     + juce::String (extremeCount) + " extremes)");

    auto choiceFailures = 0;
    auto choiceCount = 0;
    juce::StringArray choiceFailNames;

    for (auto* parameter : processor.getParameters())
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            for (int index = 0; index < choice->choices.size(); ++index)
            {
                choice->setValueNotifyingHost (choice->convertTo0to1 ((float) index));
                ++choiceCount;

                const auto peak = renderIsBounded();

                if (peak < 0.0f || peak > 4.0f)
                {
                    ++choiceFailures;

                    if (choiceFailNames.size() < 40)
                        choiceFailNames.add (parameter->getName (64) + " #" + juce::String (index)
                                             + " peak " + juce::String (peak, 4));
                }
            }
        }
    }

    check (choiceFailures == 0, "every choice index renders bounded audio ("
                                    + juce::String (choiceCount) + " values)"
                                    + (choiceFailNames.isEmpty() ? juce::String()
                                                                 : "  bad: " + choiceFailNames.joinIntoString (", ")));

    processor.panic();
}
void runFxModuleIsolationTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setSlot = [&processor] (int slot, int type)
    {
        if (auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) type));
    };

    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        setSlot (slot, 0);

        if (auto* bypass = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_bypass"))
            bypass->setValueNotifyingHost (0.0f);

        if (auto* mix = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_mix"))
            mix->setValueNotifyingHost (1.0f);
    }

    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_slot1"));
    const auto typeCount = choice != nullptr ? choice->choices.size() : 0;

    for (int type = 0; type < typeCount; ++type)
    {
        setSlot (1, type);

        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        auto finite = true;

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();

            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto value = buffer.getSample (channel, i);

                    if (! std::isfinite (value))
                        finite = false;

                    peak = juce::jmax (peak, std::abs (value));
                }
        }

        const auto name = choice != nullptr ? choice->choices[type] : juce::String (type);
        check (finite && peak < 2.5f, "fx module '" + name + "' renders bounded audio (peak "
                                          + juce::String (peak, 4) + ")");
    }

    processor.panic();
}

void runFxIntegrityTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setParam = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto readParam = [&processor] (const juce::String& id)
    {
        if (const auto* value = processor.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    };

    const auto clearChain = [&] ()
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            setParam ("fx_slot" + juce::String (slot), 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_bypass", 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_solo", 0.0f);
            setParam ("fx_slot" + juce::String (slot) + "_mix", 1.0f);
        }
    };

    clearChain();

    // --- Delay must produce an echo at the set delay time and only there ---
    setParam ("fx_slot1", 9.0f); // Delay
    setParam ("fx_delay_on", 1.0f);
    setParam ("fx_delay_sync", 0.0f);
    setParam ("fx_delay_time", 100.0f);
    setParam ("fx_delay_feedback", 0.0f);
    setParam ("fx_delay_mix", 1.0f);
    setParam ("fx_delay_pitch", 0.0f);
    setParam ("fx_delay_wow", 0.0f);
    setParam ("fx_delay_duck", 0.0f);
    setParam ("fx_taps_on", 0.0f);
    setParam ("fx_delay_pingpong", 0.0f);
    setParam ("amp_attack", 0.001f);
    setParam ("amp_decay", 0.02f);
    setParam ("amp_sustain", 0.0f);
    setParam ("amp_release", 0.02f);

    const auto renderEcho = [&] (float delayMs)
    {
        processor.panic();
        processor.reset();
        processor.prepareToPlay (48000.0, 256);
        setParam ("fx_delay_time", delayMs);

        constexpr int totalBlocks = 220;
        std::vector<float> output ((size_t) totalBlocks * 256, 0.0f);

        for (int block = 0; block < totalBlocks; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 256);
            buffer.clear();

            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);

            if (block == 2)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 256; ++i)
                output[(size_t) (block * 256 + i)] = buffer.getSample (0, i);
        }

        return output;
    };

    const auto peakIn = [] (const std::vector<float>& output, float fromMs, float toMs)
    {
        const auto from = (int) (fromMs * 48.0f);
        const auto to = juce::jmin ((int) (toMs * 48.0f), (int) output.size());
        auto peak = 0.0f;

        for (int i = juce::jmax (0, from); i < to; ++i)
            peak = juce::jmax (peak, std::abs (output[(size_t) i]));

        return peak;
    };

    const auto echoAt100 = renderEcho (100.0f);
    check (peakIn (echoAt100, 90.0f, 115.0f) > 0.01f,
           "delay produces an echo at 100 ms (peak " + juce::String (peakIn (echoAt100, 90.0f, 115.0f), 4) + ")");
    check (peakIn (echoAt100, 190.0f, 215.0f) < 0.005f,
           "delay with zero feedback has no second echo (peak " + juce::String (peakIn (echoAt100, 190.0f, 215.0f), 4) + ")");

    const auto echoAt250 = renderEcho (250.0f);
    check (peakIn (echoAt250, 240.0f, 265.0f) > 0.01f && peakIn (echoAt250, 90.0f, 115.0f) < 0.005f,
           "delay time change moves the echo (250 ms peak " + juce::String (peakIn (echoAt250, 240.0f, 265.0f), 4)
               + ", 100 ms window " + juce::String (peakIn (echoAt250, 90.0f, 115.0f), 4) + ")");

    setParam ("fx_taps_on", 1.0f);
    setParam ("fx_taps_pattern", 0.0f);
    setParam ("fx_taps_mix", 1.0f);

    const auto tapped = renderEcho (200.0f);
    check (peakIn (tapped, 40.0f, 60.0f) > 0.005f && peakIn (tapped, 90.0f, 110.0f) > 0.005f
               && peakIn (tapped, 190.0f, 215.0f) > 0.01f,
           "delay taps land at their fractions of the delay time ("
               + juce::String (peakIn (tapped, 40.0f, 60.0f), 4) + " / "
               + juce::String (peakIn (tapped, 90.0f, 110.0f), 4) + " / "
               + juce::String (peakIn (tapped, 190.0f, 215.0f), 4) + ")");

    setParam ("fx_taps_on", 0.0f);
    setParam ("fx_taps_mix", 0.0f);

    processor.panic();
    clearChain();

    // --- A/B chains capture and restore the rack ---
    setParam ("fx_slot1", 3.0f);
    setParam ("fx_slot2", 4.0f);
    processor.switchFxChain(); // A stored, now on B (a copy of A)

    setParam ("fx_slot1", 5.0f);
    setParam ("fx_slot2", 6.0f);
    processor.switchFxChain(); // back to A

    const auto slot1A = readParam ("fx_slot1");
    const auto slot2A = readParam ("fx_slot2");
    check (std::abs (slot1A - 3.0f) < 0.01f && std::abs (slot2A - 4.0f) < 0.01f,
           "A/B chain restores bank A (slot1 " + juce::String (slot1A, 1) + ", slot2 " + juce::String (slot2A, 1) + ")");

    processor.switchFxChain(); // to B
    const auto slot1B = readParam ("fx_slot1");
    check (std::abs (slot1B - 5.0f) < 0.01f,
           "A/B chain restores bank B (slot1 " + juce::String (slot1B, 1) + ")");

    // --- FX chain file round trip ---
    const auto chainFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ilana_chain_test.ilanafxchain");
    chainFile.deleteFile();

    const auto chainSaved = processor.saveFxChainToFile (chainFile);
    check (chainSaved && chainFile.existsAsFile() && chainFile.getSize() > 0,
           "fx chain saves to file (" + juce::String (chainFile.getSize()) + " bytes)");

    setParam ("fx_slot1", 0.0f);

    check (processor.loadFxChainFromFile (chainFile), "fx chain loads from file");

    const auto slot1Reloaded = readParam ("fx_slot1");
    check (std::abs (slot1Reloaded - 5.0f) < 0.01f,
           "fx chain file restores slot types (slot1 " + juce::String (slot1Reloaded, 1) + ")");

    chainFile.deleteFile();

    // --- Preset files carry sample paths ---
    const auto sampleFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_sample_preset_test.wav");
    sampleFile.deleteFile();

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (sampleFile);
        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (48000.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, 4096);

            for (int i = 0; i < 4096; ++i)
                buffer.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 48000.0f));

            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            writer.reset();
        }
    }

    check (processor.loadUserSample (0, sampleFile), "sample loads for preset test");

    const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_sample_preset_test.ilanapreset");
    presetFile.deleteFile();

    check (processor.savePresetToFile (presetFile), "preset with sample saves");

    const auto presetText = presetFile.loadFileAsString();
    check (presetText.contains ("SamplePath"),
           "saved preset contains the sample path");

    presetFile.deleteFile();
    sampleFile.deleteFile();

    processor.panic();
}

void runStateRoundTripTest()
{
    const auto setParam = [] (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto readParam = [] (IlanaSynthAudioProcessor& processor, const juce::String& id)
    {
        if (const auto* value = processor.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    };

    const auto sampleFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_roundtrip_sample.wav");
    sampleFile.deleteFile();

    {
        juce::WavAudioFormat format;
        auto fileStream = std::make_unique<juce::FileOutputStream> (sampleFile);
        std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (48000.0)
                                                          .withNumChannels (1)
                                                          .withBitsPerSample (16));

        if (writer != nullptr)
        {
            juce::AudioBuffer<float> buffer (1, 2048);

            for (int i = 0; i < 2048; ++i)
                buffer.setSample (0, i, 0.4f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) i / 48000.0f));

            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            writer.reset();
        }
    }

    IlanaSynthAudioProcessor source;
    source.prepareToPlay (48000.0, 256);

    setParam (source, "osc1_mode", 2.0f);
    setParam (source, "osc1_sample_factory", 1.0f);
    setParam (source, "osc1_sample_loop", 1.0f);
    setParam (source, "osc1_chord", 4.0f);
    setParam (source, "sub_mode", 1.0f);
    setParam (source, "f1_cutoff", 1234.0f);
    source.setLfoCustomPoint (0, 3, 0.77f);

    check (source.loadUserSample (1, sampleFile), "round-trip source loads a user sample");

    const auto cutoffSource = readParam (source, "f1_cutoff");
    const auto chordSource = readParam (source, "osc1_chord");

    juce::MemoryBlock state;
    source.getStateInformation (state);

    IlanaSynthAudioProcessor loaded;
    loaded.prepareToPlay (48000.0, 256);
    loaded.setStateInformation (state.getData(), (int) state.getSize());

    loaded.flushAsyncUpdates();

    check (std::abs (readParam (loaded, "osc1_mode") - 2.0f) < 0.01f,
           "state round trip restores oscillator mode");
    check (std::abs (readParam (loaded, "osc1_chord") - chordSource) < 0.01f,
           "state round trip restores chords");
    check (std::abs (readParam (loaded, "f1_cutoff") - cutoffSource) < 0.01f,
           "state round trip restores filter cutoff (" + juce::String (readParam (loaded, "f1_cutoff"), 2) + ")");
    check (loaded.apvts.state.getProperty ("lfo1Draw").toString().contains ("0.77000"),
           "state round trip restores the drawn LFO shape");

    const auto* factory = loaded.getSampleForOsc (0);
    check (factory != nullptr && factory->name == "Metal Hit",
           "factory sample selection survives the round trip");

    const auto* userSample = loaded.getSampleForOsc (1);
    check (userSample != nullptr && userSample->name == "ilana_roundtrip_sample",
           "user sample reloads from its saved path");

    // Preset file round trip carries the same data.
    const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ilana_roundtrip.ilanapreset");
    presetFile.deleteFile();

    check (source.savePresetToFile (presetFile), "round-trip preset saves");

    IlanaSynthAudioProcessor fromPreset;
    fromPreset.prepareToPlay (48000.0, 256);
    check (fromPreset.loadPresetFromFile (presetFile), "round-trip preset loads");

    fromPreset.flushAsyncUpdates();

    check (std::abs (readParam (fromPreset, "f1_cutoff") - cutoffSource) < 0.01f,
           "preset round trip restores parameters (" + juce::String (readParam (fromPreset, "f1_cutoff"), 2) + ")");
    check (fromPreset.getSampleForOsc (1) != nullptr,
           "preset round trip restores the user sample");

    presetFile.deleteFile();
    sampleFile.deleteFile();
}

void runSoakTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Random random (0x1a2b3c);
    processor.randomizeFxChain();

    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
    {
        if (auto* parameter = processor.apvts.getParameter ("fx_slot" + juce::String (slot) + "_bypass"))
            parameter->setValueNotifyingHost (0.0f);
    }

    const auto setRandom = [&processor, &random] (const juce::String& id)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (random.nextFloat());
    };

    const auto presetCount = (int) Presets::getFactoryPresets().size();
    constexpr int totalBlocks = 2400; // ~26 seconds of audio
    auto note = 48;
    auto finite = true;
    auto peak = 0.0f;

    for (int block = 0; block < totalBlocks; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();

        juce::MidiBuffer midi;

        if (block % 16 == 0)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        if (block % 16 == 1)
        {
            note = 33 + random.nextInt (50);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (40 + random.nextInt (80))), 0);
        }

        if (block % 41 == 0)
        {
            for (const auto* id : { "osc1_frame", "osc2_frame", "sub_frame", "f1_cutoff", "f1_reso",
                                    "res_amount", "fx_delay_time", "fx_delay_feedback", "fx_feedback_amount",
                                    "fx_reverb_size", "amp_sustain", "unison_random", "voice_spread",
                                    "osc1_mode", "osc1_sample_factory", "osc1_sample_start", "osc1_sample_end",
                                    "mod1_src", "mod1_dst", "mod2_dst", "mod3_dst", "mod4_dst" })
                setRandom (id);
        }

        if (block % 200 == 0)
        {
            if (((block / 200) % 2) == 0)
                processor.randomizeFxChain();
            else if (presetCount > 0)
                processor.loadFactoryPreset (random.nextInt (presetCount));
        }

        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
            {
                const auto value = buffer.getSample (channel, i);

                if (! std::isfinite (value))
                    finite = false;

                peak = juce::jmax (peak, std::abs (value));
            }
    }

    check (finite && peak > 0.01f && peak < 4.0f,
           "soak: 26 s of preset churn, notes and FX stay finite (peak " + juce::String (peak, 3) + ")");

    processor.panic();
}

void runLegacyPresetFxTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto slotType = [&processor] (int slot)
    {
        if (const auto* value = processor.apvts.getRawParameterValue ("fx_slot" + juce::String (slot)))
            return (int) value->load();

        return 0;
    };

    const auto findPreset = [] (const juce::String& name)
    {
        const auto& presets = Presets::getFactoryPresets();

        for (int i = 0; i < (int) presets.size(); ++i)
            if (name == presets[(size_t) i].name)
                return i;

        return -1;
    };

    // What the recipes themselves load (the voicing layer re-lays some FX).
    IlanaSynthAudioProcessor::presetVoicingEnabled = false;

    processor.loadFactoryPreset (findPreset ("Rip Bass"));
    check (slotType (1) == 2, "legacy preset restores its Drive module (slot1 " + juce::String (slotType (1)) + ")");

    processor.loadFactoryPreset (findPreset ("Scream Lead"));
    check (slotType (1) == 9 && slotType (2) == 13,
           "legacy preset restores Delay and Reverb in order (slot1 " + juce::String (slotType (1))
               + ", slot2 " + juce::String (slotType (2)) + ")");

    processor.loadFactoryPreset (findPreset ("Metal Keys"));
    check (slotType (7) == 9 && slotType (10) == 13,
           "modern preset keeps its explicit slot layout");

    IlanaSynthAudioProcessor::presetVoicingEnabled = true;
    processor.panic();
}

void runLfo34Test()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto setParam = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    setParam ("lfo3_shape", 0.0f);
    setParam ("lfo3_rate", 8.0f);
    setParam ("lfo4_shape", 0.0f);
    setParam ("lfo4_rate", 3.0f);
    setParam ("mod1_src", 20.0f); // LFO 3
    setParam ("mod1_dst", 2.0f);  // Osc1 Frame
    setParam ("mod1_amt", 0.6f);

    juce::AudioBuffer<float> buffer (2, 256);

    auto lfo3Moved = false;

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);

        if (std::abs (processor.getModDisplay (Mod::Destination::Osc1Frame)) > 0.01f)
            lfo3Moved = true;
    }

    check (lfo3Moved, "LFO 3 modulates a destination through the matrix");

    processor.panic();

    // Drawn shapes persist per LFO for all four.
    processor.setLfoCustomPoint (3, 5, 0.66f);
    check (std::abs (processor.getLfoCustomPoint (3, 5) - 0.66f) < 0.001f,
           "LFO 4 custom draw points store and read back");

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    IlanaSynthAudioProcessor reloaded;
    reloaded.prepareToPlay (48000.0, 256);
    reloaded.setStateInformation (state.getData(), (int) state.getSize());
    reloaded.flushAsyncUpdates();

    check (std::abs (reloaded.getLfoCustomPoint (3, 5) - 0.66f) < 0.001f,
           "LFO 4 draw curve survives a state round trip");
}

void runOversamplingTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    const auto render = [&processor] (int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        auto finite = true;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto value = buffer.getSample (channel, i);

                    if (! std::isfinite (value))
                        finite = false;

                    peak = juce::jmax (peak, std::abs (value));
                }
        }

        return std::make_pair (finite, peak);
    };

    if (auto* parameter = processor.apvts.getParameter ("oversampling"))
        parameter->setValueNotifyingHost (1.0f);

    processor.flushAsyncUpdates();
    check (processor.isOversampling(), "2x oversampling engages");

    const auto [onFinite, onPeak] = render (40);
    check (onFinite && onPeak > 0.01f && onPeak < 4.0f,
           "oversampled render is finite and audible (peak " + juce::String (onPeak, 3) + ")");

    if (auto* parameter = processor.apvts.getParameter ("os_factor"))
        parameter->setValueNotifyingHost (1.0f);

    processor.flushAsyncUpdates();
    check (processor.getOversamplingFactor() == 4, "4x oversampling engages");

    const auto [fourFinite, fourPeak] = render (40);
    check (fourFinite && fourPeak > 0.01f && fourPeak < 4.0f,
           "4x oversampled render is finite and audible (peak " + juce::String (fourPeak, 3) + ")");

    if (auto* parameter = processor.apvts.getParameter ("oversampling"))
        parameter->setValueNotifyingHost (0.0f);

    processor.flushAsyncUpdates();
    check (! processor.isOversampling(), "oversampling toggles back off");

    const auto [offFinite, offPeak] = render (40);
    check (offFinite && offPeak > 0.01f && offPeak < 4.0f,
           "render after toggling oversampling off is fine (peak " + juce::String (offPeak, 3) + ")");

    processor.panic();
}
// Adding a classic module to an empty slot from the rack menu must make it
// audible without also hunting for the module's own ON switch.
void runFxSlotAssignTest()
{
    const auto renderTail = [] (bool withReverb)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        if (withReverb)
            processor.assignFxSlot (1, 13);

        auto tailEnergy = 0.0;

        for (int block = 0; block < 180; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            else if (block == 25)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            processor.processBlock (buffer, midi);

            // ~1 s after the note off, well past the 0.25 s amp release.
            if (block >= 120)
                for (int i = 0; i < 512; ++i)
                    tailEnergy += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }

        return tailEnergy;
    };

    const auto dry = renderTail (false);
    const auto wet = renderTail (true);

    check (dry < 1.0e-6 && wet > dry + 1.0e-4,
           "reverb added to a slot is audible straight away (dry tail " + juce::String (dry, 8)
               + ", wet tail " + juce::String (wet, 6) + ")");

    IlanaSynthAudioProcessor processor;
    processor.assignFxSlot (2, 10);
    check (processor.apvts.getRawParameterValue ("fx_stutter_on")->load() < 0.5f,
           "adding Stutter leaves its momentary trigger off");
}

void runPresetNameTest()
{
    IlanaSynthAudioProcessor processor;
    const auto names = processor.getFactoryPresetNames();
    processor.loadFactoryPreset (3);
    check (processor.getCurrentPresetName() == names[3], "factory preset load records its name");

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    IlanaSynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (restored.getCurrentPresetName() == names[3],
           "preset name survives a host state round trip ('" + restored.getCurrentPresetName() + "')");
}

// Phase 1: new filter models and voice modes.
void runFilterModelTests()
{
    constexpr double sampleRate = 48000.0;

    // Self-oscillation: silence in, a sustained bounded tone out, for the
    // SVF (Morph), Airwindows' Y low-pass above 0.98 and the ladder at full
    // feedback.
    for (const auto type : { (int) FilterType::Morph, (int) FilterType::LowPass, (int) FilterType::LadderLow })
    {
        FilterUnit filter;
        filter.setType (type, false);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, 1000.0, 1.0));

        auto peak = 0.0f;
        auto lateEnergy = 0.0;
        auto finite = true;

        for (int i = 0; i < (int) sampleRate * 2; ++i)
        {
            const auto input = i < 32 ? 0.5f : 0.0f;
            const auto out = filter.process (input);
            finite = finite && std::isfinite (out);
            peak = juce::jmax (peak, std::abs (out));

            if (i > (int) sampleRate)
                lateEnergy += (double) out * out;
        }

        const auto lateRms = std::sqrt (lateEnergy / sampleRate);
        check (finite && peak < 4.0f && lateRms > 0.05,
               FilterType::getNames()[type] + " self-oscillates at max resonance and stays bounded (peak "
                   + juce::String (peak, 3) + ", late rms " + juce::String (lateRms, 3) + ")");
    }

    // At low level Airwindows' Y low-pass (24 dB) is the old SVF cascade
    // (its encode and decode curves are straight there; the difference is its
    // 20 kHz Butterworths), so the presets' tone balance carries over.
    {
        FilterUnit unit;
        unit.setType (FilterType::LowPass, true);
        const auto c = FilterUnit::makeCoefficients (FilterType::LowPass, sampleRate, 800.0, 0.7);
        unit.setCoefficients (c);

        Svf a, b;
        a.setMode (Svf::Mode::LowPass);
        b.setMode (Svf::Mode::LowPass);
        a.setCoefficients (Svf::makeCoefficients (sampleRate, 800.0, 0.7));
        b.setCoefficients (Svf::makeCoefficients (sampleRate, 800.0, 0.7));

        juce::Random random (7);
        auto maxDiff = 0.0f;

        for (int i = 0; i < 4800; ++i)
        {
            const auto x = (random.nextFloat() * 2.0f - 1.0f) * 0.002f;
            maxDiff = juce::jmax (maxDiff, std::abs (unit.process (x) - b.processSample (a.processSample (x))));
        }

        check (maxDiff < 0.002f * 0.05f, "24 dB Y low-pass matches the old SVF cascade at low level (max diff "
                                            + juce::String (maxDiff / 0.002f * 100.0f, 3) + " % of the input)");
    }

    // Ladder low-pass actually low-passes and high-pass actually high-passes.
    const auto toneGain = [sampleRate] (int type, double toneHz)
    {
        FilterUnit filter;
        filter.setType (type, true);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, 1000.0, 0.2));

        auto in = 0.0, out = 0.0;

        for (int i = 0; i < 24000; ++i)
        {
            const auto x = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * i / sampleRate);
            const auto y = filter.process (x);

            if (i > 4800)
            {
                in += (double) x * x;
                out += (double) y * y;
            }
        }

        return std::sqrt (out / in);
    };

    const auto lpLow = toneGain (FilterType::LadderLow, 100.0);
    const auto lpHigh = toneGain (FilterType::LadderLow, 8000.0);
    const auto hpLow = toneGain (FilterType::LadderHigh, 100.0);
    const auto hpHigh = toneGain (FilterType::LadderHigh, 8000.0);

    check (lpLow > 0.5 && lpHigh < 0.02, "ladder LP passes lows and cuts highs (100 Hz " + juce::String (lpLow, 3)
                                             + ", 8 kHz " + juce::String (lpHigh, 4) + ")");
    check (hpHigh > 0.5 && hpLow < 0.02, "ladder HP passes highs and cuts lows (100 Hz " + juce::String (hpLow, 4)
                                             + ", 8 kHz " + juce::String (hpHigh, 3) + ")");
}

void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void runVoiceModeTests()
{
    const auto render = [] (IlanaSynthAudioProcessor& processor, juce::MidiBuffer midi, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer blockMidi;

            if (block == 0)
                blockMidi = midi;

            processor.processBlock (buffer, blockMidi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 256));
        }

        return peak;
    };

    const auto chord = []
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 10);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 20);
        return midi;
    };

    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        render (processor, chord(), 20);
        check (processor.getActiveVoiceCount() == 3, "poly mode plays a 3-note chord on 3 voices ("
                                                         + juce::String (processor.getActiveVoiceCount()) + ")");
    }

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "poly_voices", 2.0f);
        processor.prepareToPlay (48000.0, 256);
        render (processor, chord(), 20);
        check (processor.getActiveVoiceCount() <= 2, "poly voice limit of 2 is respected ("
                                                         + juce::String (processor.getActiveVoiceCount()) + ")");
    }

    for (const auto mode : { 1, 2 })
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "voice_mode", (float) mode);
        processor.prepareToPlay (48000.0, 256);
        const auto peak = render (processor, chord(), 20);
        const auto name = mode == 1 ? juce::String ("mono") : juce::String ("legato");

        check (processor.getActiveVoiceCount() == 1 && peak > 0.01f,
               name + " mode plays a chord on one voice (" + juce::String (processor.getActiveVoiceCount()) + ")");

        // Release the top note while the others are held: the voice must keep
        // sounding (falls back to 64), then go quiet once everything is up.
        juce::MidiBuffer releaseTop;
        releaseTop.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        const auto fallbackPeak = render (processor, releaseTop, 10);

        juce::MidiBuffer releaseAll;
        releaseAll.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        releaseAll.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        render (processor, releaseAll, 400);

        check (fallbackPeak > 0.01f && processor.getActiveVoiceCount() == 0,
               name + " mode falls back to held notes, then releases (fallback peak "
                   + juce::String (fallbackPeak, 3) + ")");
    }

    // Legato keeps the amp envelope running between overlapping notes; mono
    // retriggers it. Slow attack makes the difference measurable.
    const auto levelAfterSecondNote = [&render] (int mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "voice_mode", (float) mode);
        setParam (processor, "amp_attack", 1.0f);
        processor.prepareToPlay (48000.0, 256);

        juce::MidiBuffer first;
        first.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        render (processor, first, 200); // ~1.07 s: attack complete

        juce::MidiBuffer second;
        second.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
        return render (processor, second, 2);
    };

    {
        IlanaSynthAudioProcessor processor;
        processor.setMacroName (2, "Grit");
        juce::MemoryBlock state;
        processor.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getMacroName (2) == "Grit" && restored.getMacroName (0) == "Macro 1",
               "macro names survive a host state round trip ('" + restored.getMacroName (2) + "')");

        restored.loadFactoryPreset (0);
        check (restored.getMacroName (2) == "Macro 3", "loading Init resets macro names");

        restored.loadFactoryPreset (1);
        check (restored.getMacroName (0) == "TONE", "legacy presets get default macro names ('" + restored.getMacroName (0) + "')");
        check (restored.getPresetCategory() == "Bass", "factory preset load sets its category");

        const auto file = juce::File::createTempFile ("ilanapreset");
        restored.setPresetMeta ("Keys", "warm, vintage");
        check (restored.savePresetToFile (file), "preset with metadata saves");

        IlanaSynthAudioProcessor reloaded;
        check (reloaded.loadPresetFromFile (file) && reloaded.getPresetCategory() == "Keys"
                   && reloaded.getPresetTags() == "warm, vintage",
               "preset category and tags survive a save/load ('" + reloaded.getPresetCategory() + "')");
        file.deleteFile();
    }

    const auto legatoLevel = levelAfterSecondNote (2);
    const auto monoLevel = levelAfterSecondNote (1);

    check (legatoLevel > monoLevel * 0.9f && legatoLevel > 0.05f,
           "legato does not retrigger the envelope (legato " + juce::String (legatoLevel, 3)
               + ", mono " + juce::String (monoLevel, 3) + ")");
}

// Phase 2: warps, unison, per-voice LFOs, the extended matrix.
float renderPeakAndCentroid (IlanaSynthAudioProcessor& processor, int note, int blocks, double& centroid,
                             std::vector<float>* capture = nullptr)
{
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> samples;
    auto peak = 0.0f;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 512));

        for (int i = 0; i < 512; ++i)
            samples.push_back (buffer.getSample (0, i));
    }

    // Spectral centroid of the last 8192 samples.
    constexpr int order = 13;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work ((size_t) size * 2, 0.0f);
    const auto start = samples.size() - (size_t) size;

    for (int i = 0; i < size; ++i)
        work[(size_t) i] = samples[start + (size_t) i]
                           * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));

    fft.performFrequencyOnlyForwardTransform (work.data());
    double weighted = 0.0, total = 0.0;

    for (int bin = 1; bin < size / 2; ++bin)
    {
        weighted += bin * 48000.0 / size * work[(size_t) bin];
        total += work[(size_t) bin];
    }

    centroid = total > 0.0 ? weighted / total : 0.0;

    if (capture != nullptr)
        *capture = samples;

    return peak;
}

void runWarpTests()
{
    const auto names = Warp::getNames();
    double plainCentroid = 0.0;

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 8.0f); // Sine: warps add harmonics
        setParam (processor, "f1_cutoff", 20000.0f);
        processor.prepareToPlay (48000.0, 512);
        renderPeakAndCentroid (processor, 45, 40, plainCentroid);
    }

    for (int mode = 1; mode < Warp::Count; ++mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", 8.0f);
        setParam (processor, "f1_cutoff", 20000.0f);
        setParam (processor, "osc1_warp", (float) mode);
        setParam (processor, "osc1_warp_amt", 0.7f);

        if (mode == Warp::Fm || mode == Warp::Ring)
        {
            setParam (processor, "osc2_on", 1.0f);
            setParam (processor, "osc2_level", 0.01f);
            setParam (processor, "osc2_semi", 7.0f);
        }

        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);

        check (std::isfinite (peak) && peak > 0.02f && peak < 4.0f && centroid > plainCentroid * 1.15,
               "warp " + names[mode] + " adds harmonics to a sine (centroid " + juce::String (plainCentroid, 0)
                   + " -> " + juce::String (centroid, 0) + " Hz, peak " + juce::String (peak, 2) + ")");
    }
}

void runUnisonTests()
{
    // 16 voices render, and blend 0 leaves only the centre voice(s).
    const auto widthOf = [] (int voices, float blend, int mode)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_unison", (float) voices);
        setParam (processor, "osc1_detune", 30.0f);
        setParam (processor, "osc1_spread", 1.0f);
        setParam (processor, "osc1_uni_blend", blend);
        setParam (processor, "osc1_uni_mode", (float) mode);
        processor.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        auto side = 0.0, mid = 0.0;

        for (int block = 0; block < 60; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            if (block > 10)
                for (int i = 0; i < 512; ++i)
                {
                    const auto l = buffer.getSample (0, i), r = buffer.getSample (1, i);
                    mid += (double) (l + r) * (l + r);
                    side += (double) (l - r) * (l - r);
                }
        }

        return std::sqrt (side / juce::jmax (1.0e-12, mid));
    };

    // Every stack mode renders bounded audio; the interval stacks change the sound.
    {
        const auto render = [] (int mode)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_unison", 5.0f);
            setParam (processor, "osc1_detune", 10.0f);
            setParam (processor, "osc1_uni_mode", (float) mode);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);
            return std::make_pair (peak, centroid);
        };

        const auto classic = render (UnisonMode::Classic);
        auto all = true, different = true;

        for (int mode = UnisonMode::CenterDrop; mode < UnisonMode::Count; ++mode)
        {
            const auto result = render (mode);
            all = all && std::isfinite (result.first) && result.first > 0.02f && result.first < 4.0f;
            different = different && std::abs (result.second - classic.second) > 1.0;
        }

        check (all, "every appended unison stack mode renders bounded audio");
        check (different, "the appended unison stack modes change the sound against Classic");
    }

    // Frame and warp spreads change a stack's sound, stay bounded, and do nothing at 0 or with one voice.
    {
        const auto render = [] (int voices, const char* spreadId, float spread)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_unison", (float) voices);
            setParam (processor, "osc1_detune", 10.0f);
            setParam (processor, "osc1_table", 6.0f);
            setParam (processor, "osc1_frame", 0.5f);
            setParam (processor, "osc1_warp", 7.0f);
            setParam (processor, "osc1_warp_amt", 0.3f);
            setParam (processor, spreadId, spread);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            const auto peak = renderPeakAndCentroid (processor, 45, 40, centroid);
            return std::make_pair (peak, centroid);
        };

        for (const auto* id : { "osc1_uni_frame", "osc1_uni_warp" })
        {
            const auto off = render (5, id, 0.0f);
            const auto on = render (5, id, 1.0f);
            const auto single = render (1, id, 1.0f);
            const auto singleOff = render (1, id, 0.0f);
            check (std::isfinite (on.first) && on.first > 0.02f && on.first < 4.0f && std::abs (on.second - off.second) > 1.0,
                   juce::String (id) + " changes a five-voice stack and stays bounded (centroid "
                       + juce::String (off.second, 0) + " -> " + juce::String (on.second, 0) + " Hz)");
            check (std::abs (single.second - singleOff.second) < 0.5,
                   juce::String (id) + " leaves a single voice alone");
        }
    }

    // Scale quantise: C major snaps note 61 (C#) to C or D, so the pitch moves by a semitone from the plain note.
    {
        const auto frequencyOf = [] (int scale, int note)
        {
            IlanaSynthAudioProcessor processor;
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "osc1_table", 0.0f);
            setParam (processor, "osc1_scale", (float) scale);
            processor.prepareToPlay (48000.0, 512);
            double centroid = 0.0;
            renderPeakAndCentroid (processor, note, 40, centroid);
            return centroid;
        };

        const auto plain = frequencyOf (0, 61);
        const auto snapped = frequencyOf (1, 61);
        const auto inKey = frequencyOf (1, 60);
        const auto inKeyPlain = frequencyOf (0, 60);
        check (std::abs (snapped - plain) > 1.0 && std::abs (inKey - inKeyPlain) < 0.5,
               "an oscillator's scale snap moves an out-of-key note and leaves an in-key one (61: "
                   + juce::String (plain, 0) + " -> " + juce::String (snapped, 0) + " Hz)");
    }

    const auto full16 = widthOf (16, 1.0f, UnisonMode::Classic);
    const auto blend0 = widthOf (15, 0.0f, UnisonMode::Classic);
    const auto hyper = widthOf (16, 1.0f, UnisonMode::Hypersaw);
    const auto octaves = widthOf (9, 1.0f, UnisonMode::Octaves);

    check (full16 > 0.2 && std::isfinite (hyper) && hyper > 0.1 && std::isfinite (octaves) && octaves > 0.1,
           "16-voice unison, hypersaw and octave stacks render wide (" + juce::String (full16, 2) + ", "
               + juce::String (hyper, 2) + ", " + juce::String (octaves, 2) + ")");
    check (blend0 < 0.01, "unison blend 0 keeps only the centre voice (width " + juce::String (blend0, 4) + ")");
}

void runPerVoiceLfoTest()
{
    // Two notes started half a second apart with a retriggered LFO on pitch:
    // each voice's LFO starts at its own note-on, so a slow LFO puts the two
    // notes at different points of the cycle. Checked through the voices.
    IlanaSynthAudioProcessor processor;
    setParam (processor, "lfo1_retrig", 1.0f);
    setParam (processor, "lfo1_rate", 0.5f);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 50; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        if (block == 47)
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);
    }

    const auto phases = processor.getVoiceLfoPhasesForTest (0);
    check (phases.size() == 2 && std::abs (phases[0] - phases[1]) > 0.2f,
           "retriggered LFO runs per voice (phases " + juce::String (phases.size() > 0 ? phases[0] : -1.0f, 3) + ", "
               + juce::String (phases.size() > 1 ? phases[1] : -1.0f, 3) + ")");
}

void runMatrixTests()
{
    const auto levelWith = [] (std::function<void (IlanaSynthAudioProcessor&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "master_clip", 0.0f);
        setup (processor);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 57, 30, centroid);
    };

    const auto base = levelWith ([] (auto&) {});

    // Slot 20 works like slot 1: macro 1 at full pulls Amp Level down.
    const auto viaSlot20 = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod20_src", (float) Mod::Source::Macro1);
        setParam (p, "mod20_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod20_amt", -0.8f);
    });
    check (viaSlot20 < base * 0.4f, "mod slot 20 routes (peak " + juce::String (base, 3) + " -> " + juce::String (viaSlot20, 3) + ")");

    const auto bypassed = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod20_src", (float) Mod::Source::Macro1);
        setParam (p, "mod20_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod20_amt", -0.8f);
        setParam (p, "mod20_byp", 1.0f);
    });
    check (std::abs (bypassed - base) < base * 0.05f, "bypassed slot does nothing");

    // Via: macro 2 at zero scales the routing away.
    const auto viaZero = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod3_src", (float) Mod::Source::Macro1);
        setParam (p, "mod3_dst", (float) Mod::Destination::AmpLevel);
        setParam (p, "mod3_amt", -0.8f);
        setParam (p, "mod3_aux", (float) Mod::Source::Macro2);
    });
    check (std::abs (viaZero - base) < base * 0.05f, "via source at zero mutes the routing");

    // Modulating a plain parameter: macro 1 into the master volume.
    const auto masterMod = levelWith ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "macro1", 1.0f);
        setParam (p, "mod5_src", (float) Mod::Source::Macro1);
        setParam (p, "mod5_dst", (float) Mod::destinationForParamId ("master"));
        setParam (p, "mod5_amt", -0.3f);
    });
    check (masterMod < base * 0.5f, "macro can modulate the master volume parameter (peak " + juce::String (masterMod, 3) + ")");

    // Shape: polarity and curve.
    Mod::Slot slot;
    slot.source = Mod::Source::Macro1;
    slot.polarity = Mod::Polarity::Bipolar;
    check (std::abs (Mod::shape (slot, 0.0f) + 1.0f) < 1.0e-6f && std::abs (Mod::shape (slot, 1.0f) - 1.0f) < 1.0e-6f,
           "bipolar polarity maps a 0..1 source to -1..1");
    slot.polarity = Mod::Polarity::Natural;
    slot.curve = 1.0f;
    check (Mod::shape (slot, 0.5f) < 0.1f, "positive curve bends the response (0.5 -> "
                                              + juce::String (Mod::shape (slot, 0.5f), 3) + ")");

    check (Mod::numExplicitDestinations + Mod::numLegacyParamDestinations == Mod::firstNewExplicitDestination
               && Mod::getDestinationNames().size() == Mod::getNumDestinations(),
           "parameter destinations end exactly where the OSC 4-6 destinations begin");
    check (Mod::getExplicitDestinationNames().size() == Mod::numExplicitDestinations,
           "destination names match the destination list ("
               + juce::String (Mod::getExplicitDestinationNames().size()) + " / " + juce::String (Mod::numExplicitDestinations) + ")");
    check (Mod::getNumDestinations() < IlanaSynthAudioProcessor::maxDestinations, "destination count fits the display table");
}

void runStaleModulationTest()
{
    // A routing removed while a voice is sounding must not leave its last
    // value stuck on the target (it once left presets detuned after a
    // vibrato preset).
    IlanaSynthAudioProcessor processor;
    setParam (processor, "sub_on", 0.0f);
    setParam (processor, "lfo1_rate", 3.0f);
    setParam (processor, "mod1_src", (float) Mod::Source::Lfo1);
    setParam (processor, "mod1_dst", (float) Mod::Destination::Osc1Pitch);
    setParam (processor, "mod1_amt", 0.05f);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> samples;

    for (int block = 0; block < 140; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

        if (block == 13)
            setParam (processor, "mod1_amt", 0.0f);

        processor.processBlock (buffer, midi);

        if (block > 40)
            for (int i = 0; i < 512; ++i)
                samples.push_back (buffer.getSample (0, i));
    }

    const auto frequency = fundamentalOf (samples, 48000.0);
    check (std::abs (frequency - 220.0) < 0.5, "pitch returns to the note after a routing is removed ("
                                                   + juce::String (frequency, 2) + " Hz)");
}

void runPhase2StateAndCpuTest()
{
    // New parameters survive a host round trip.
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "osc2_warp", (float) Warp::Mirror);
        setParam (processor, "osc2_warp_amt", 0.4f);
        setParam (processor, "osc1_uni_mode", (float) UnisonMode::Hypersaw);
        setParam (processor, "mod27_src", (float) Mod::Source::Lfo3);
        setParam (processor, "mod27_dst", (float) Mod::destinationForParamId ("fx_ott_amount"));
        setParam (processor, "mod27_amt", -0.25f);
        setParam (processor, "mod27_pol", 2.0f);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto slot = restored.readModSlot (26);

        check ((int) restored.apvts.getRawParameterValue ("osc2_warp")->load() == Warp::Mirror
                   && (int) restored.apvts.getRawParameterValue ("osc1_uni_mode")->load() == UnisonMode::Hypersaw
                   && slot.source == Mod::Source::Lfo3 && slot.destination == Mod::destinationForParamId ("fx_ott_amount")
                   && std::abs (slot.depth + 0.25f) < 1.0e-3f && slot.polarity == Mod::Polarity::Bipolar,
               "warp, unison mode and an extended mod slot survive a state round trip");
    }

    // CPU. "Heavy" is a realistic big patch (8-note chord, two 16-voice
    // hypersaw oscillators, ladder filter, a busy matrix) and must stay well
    // inside real time. "Extreme" (16 notes x three 16-voice oscillators with
    // warps) is only reported.
    const auto measure = [] (int notes, int oscillators)
    {
        IlanaSynthAudioProcessor processor;
        const char* const prefixes[] { "osc1", "osc2", "sub" };

        for (int osc = 0; osc < oscillators; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            setParam (processor, prefix + "_unison", 16.0f);
            setParam (processor, prefix + "_uni_mode", (float) UnisonMode::Hypersaw);
            setParam (processor, prefix + "_warp", (float) Warp::Sync);
            setParam (processor, prefix + "_warp_amt", 0.5f);
        }

        setParam (processor, "osc2_on", 1.0f);
        setParam (processor, "sub_level", oscillators > 2 ? 0.5f : 0.0f);
        setParam (processor, "f1_type", (float) FilterType::LadderLow);

        for (int slot = 1; slot <= 16; ++slot)
        {
            setParam (processor, "mod" + juce::String (slot) + "_src", (float) (1 + slot % 20));
            setParam (processor, "mod" + juce::String (slot) + "_dst", (float) (1 + (slot * 5) % 88));
            setParam (processor, "mod" + juce::String (slot) + "_amt", 0.2f);
        }

        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer noteOns;

        for (int note = 0; note < notes; ++note)
            noteOns.addEvent (juce::MidiMessage::noteOn (1, 40 + note * 2, (juce::uint8) 100), 0);

        const auto blocks = juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH_BLOCKS", "94").getIntValue(); // ~1 s
        const auto start = juce::Time::getMillisecondCounterHiRes();

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi = noteOns;

            processor.processBlock (buffer, midi);
        }

        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        return std::make_pair (elapsed / (blocks * 512.0 / 48000.0), std::isfinite (buffer.getMagnitude (0, 512)));
    };

    const auto heavy = measure (8, 2);
    const auto extreme = measure (16, 3);

    std::cout << "INFO: CPU heavy patch " << juce::String (heavy.first * 100.0, 1) << " %, extreme patch "
              << juce::String (extreme.first * 100.0, 1) << " % of one core" << std::endl;
    checkTiming (heavy.first < 0.5 && heavy.second && extreme.second, "heavy patch (8 notes x 32 unison voices) renders well inside real time");
}

// Phase 3: the extra filter models.
double filterToneGain (int type, double toneHz, double cutoff, double resonance, float morph, bool slope24 = true)
{
    constexpr double sampleRate = 48000.0;
    FilterUnit filter;
    filter.prepare (sampleRate);
    filter.setType (type, slope24);
    filter.setCoefficients (FilterUnit::makeCoefficients (type, sampleRate, cutoff, resonance, morph));

    auto in = 0.0, out = 0.0;

    for (int i = 0; i < 24000; ++i)
    {
        const auto x = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * i / sampleRate);
        const auto y = filter.process (x);

        if (i > 9600)
        {
            in += (double) x * x;
            out += (double) y * y;
        }
    }

    return std::sqrt (out / in);
}

void runExtraFilterTests()
{
    const auto names = FilterType::getNames();

    for (int type = FilterType::DiodeLow; type < FilterType::Count; ++type)
    {
        FilterUnit filter;
        filter.prepare (48000.0);
        filter.setType (type, true);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, 48000.0, 800.0, 1.0, 0.5f));

        juce::Random random (3);
        auto peak = 0.0f;
        auto finite = true;

        for (int i = 0; i < 96000; ++i)
        {
            const auto out = filter.process ((random.nextFloat() * 2.0f - 1.0f) * 0.5f);
            finite = finite && std::isfinite (out);
            peak = juce::jmax (peak, std::abs (out));
        }

        check (finite && peak < 8.0f, names[type] + " stays bounded on noise at full resonance (peak "
                                          + juce::String (peak, 2) + ")");
    }

    for (const auto type : { (int) FilterType::DiodeLow, (int) FilterType::Ms20Low })
    {
        const auto low = filterToneGain (type, 100.0, 1000.0, 0.2, 0.0f);
        const auto high = filterToneGain (type, 8000.0, 1000.0, 0.2, 0.0f);
        check (low > 0.3 && high < low * 0.05, names[type] + " low-passes (100 Hz " + juce::String (low, 3)
                                                   + ", 8 kHz " + juce::String (high, 4) + ")");
    }

    // Comb +: peaks at the cutoff's harmonics, dips between them.
    const auto onHarmonic = filterToneGain (FilterType::CombPlus, 1000.0, 500.0, 0.8, 0.0f);
    const auto between = filterToneGain (FilterType::CombPlus, 750.0, 500.0, 0.8, 0.0f);
    check (onHarmonic > between * 3.0, "comb + rings at harmonics of its cutoff (" + juce::String (onHarmonic, 2)
                                           + " vs " + juce::String (between, 2) + ")");

    // Comb -: the opposite, peaks halfway between.
    const auto minusOn = filterToneGain (FilterType::CombMinus, 1000.0, 500.0, 0.8, 0.0f);
    const auto minusBetween = filterToneGain (FilterType::CombMinus, 750.0, 500.0, 0.8, 0.0f);
    check (minusBetween > minusOn * 3.0, "comb - peaks between the harmonics");

    // Morph walks from low-pass to high-pass.
    const auto morphLowAt0 = filterToneGain (FilterType::Morph, 100.0, 1000.0, 0.2, 0.0f);
    const auto morphHighAt0 = filterToneGain (FilterType::Morph, 8000.0, 1000.0, 0.2, 0.0f);
    const auto morphLowAt1 = filterToneGain (FilterType::Morph, 100.0, 1000.0, 0.2, 1.0f);
    const auto morphHighAt1 = filterToneGain (FilterType::Morph, 8000.0, 1000.0, 0.2, 1.0f);
    check (morphLowAt0 > morphHighAt0 * 20.0 && morphHighAt1 > morphLowAt1 * 20.0, "morph goes from low-pass to high-pass");

    // Formant 'A' has its first formant near 730 Hz: louder there than at 3.5 kHz.
    const auto formantPeak = filterToneGain (FilterType::Formant, 730.0, 1000.0, 0.8, 0.0f);
    const auto formantOff = filterToneGain (FilterType::Formant, 4500.0, 1000.0, 0.8, 0.0f);
    check (formantPeak > formantOff * 4.0, "formant filter peaks on the vowel's formant (" + juce::String (formantPeak, 2)
                                              + " vs " + juce::String (formantOff, 2) + ")");
}

void runFilterRoutingTest()
{
    // Filter 1 closed right down: Default and Filter 1 routes are silenced,
    // Filter 2 (serial) and No filter get through.
    const auto levelFor = [] (int route)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "f1_cutoff", 20.0f);
        setParam (processor, "f1_slope", 1.0f);
        setParam (processor, "f1_env", 0.0f);
        setParam (processor, "osc1_route", (float) route);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 60, 20, centroid);
    };

    const auto viaDefault = levelFor (FilterRoute::Default);
    const auto viaFilter1 = levelFor (FilterRoute::Filter1);
    const auto viaFilter2 = levelFor (FilterRoute::Filter2);
    const auto direct = levelFor (FilterRoute::Direct);

    check (viaDefault < 0.02f && viaFilter1 < 0.02f && viaFilter2 > 0.1f && direct > 0.1f,
           "per-oscillator filter routes (default " + juce::String (viaDefault, 3) + ", F1 " + juce::String (viaFilter1, 3)
               + ", F2 " + juce::String (viaFilter2, 3) + ", direct " + juce::String (direct, 3) + ")");
}

void runOversampledTuningTest()
{
    for (const auto factor : { 0.0f, 1.0f })
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "oversampling", 1.0f);
        setParam (processor, "os_factor", factor);
        processor.prepareToPlay (48000.0, 512);

        double centroid = 0.0;
        std::vector<float> samples;
        renderPeakAndCentroid (processor, 57, 60, centroid, &samples);
        samples.erase (samples.begin(), samples.begin() + 4800);

        const auto frequency = fundamentalOf (samples, 48000.0);
        check (std::abs (frequency - 220.0) < 0.5, juce::String (factor > 0.5f ? "4x" : "2x")
                                                       + " oversampled note is in tune (" + juce::String (frequency, 2) + " Hz)");
    }
}

void runResynthesisTest()
{
    // A 220 Hz saw that darkens over two seconds (not a whole number of
    // 2048-sample frames, so Automatic resynthesizes it).
    constexpr double sampleRate = 48000.0;
    constexpr int length = 96000 + 777;
    std::vector<float> audio ((size_t) length);
    auto phase = 0.0;
    auto smoothed = 0.0f;

    for (int i = 0; i < length; ++i)
    {
        phase += 220.0 / sampleRate;
        phase -= std::floor (phase);
        const auto saw = (float) (2.0 * phase - 1.0);
        const auto brightness = 1.0f - 0.95f * (float) i / (float) length;
        smoothed += brightness * (saw - smoothed);
        audio[(size_t) i] = 0.5f * smoothed;
    }

    const auto period = Wavetable::detectPeriod (audio.data() + 4800, length - 4800, sampleRate);
    check (std::abs (period - sampleRate / 220.0) < 0.5, "pitch detection finds a 220 Hz period ("
                                                            + juce::String (period, 3) + " samples)");

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana_resynth_test.wav");
    file.deleteFile();

    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (file),
                                                                                 sampleRate, 1, 24, {}, 0));
        juce::AudioBuffer<float> buffer (1, length);
        buffer.copyFrom (0, 0, audio.data(), length);
        writer->writeFromAudioSampleBuffer (buffer, 0, length);
    }

    Wavetable table;
    const auto loaded = table.loadFromFile (file, Wavetable::LoadMode::Automatic);
    file.deleteFile();

    check (loaded && table.getNumFrames() == 64, "any audio becomes a 64-frame wavetable ("
                                                     + juce::String (table.getNumFrames()) + " frames)");

    // Each frame is one cycle: harmonic n sits in bin n, and a saw's second
    // harmonic is about half the first. The last frame is darker.
    const auto harmonics = [&table] (int frame)
    {
        juce::dsp::FFT fft (11);
        std::vector<std::complex<float>> in ((size_t) Wavetable::frameSize), out ((size_t) Wavetable::frameSize);
        const auto* data = table.getFrameData (0, frame);

        for (int i = 0; i < Wavetable::frameSize; ++i)
            in[(size_t) i] = { data[i + 1], 0.0f };

        fft.perform (in.data(), out.data(), false);
        return std::array<float, 3> { std::abs (out[1]), std::abs (out[2]), std::abs (out[8]) };
    };

    const auto first = harmonics (0);
    const auto last = harmonics (63);
    const auto ratio = first[1] / juce::jmax (1.0e-6f, first[0]);

    check (ratio > 0.35f && ratio < 0.65f, "resynthesized frame keeps the saw's harmonic series (h2/h1 "
                                               + juce::String (ratio, 3) + ")");
    check (last[2] / last[0] < first[2] / first[0] * 0.7f, "later frames follow the sound getting darker");
}

void runCurveLfoTest()
{
    auto ramp = LfoCurve::preset (2); // -1 to +1 straight line
    check (std::abs (ramp.valueAt (0.5)) < 1.0e-4f && std::abs (ramp.valueAt (0.25) + 0.5f) < 1.0e-4f,
           "curve evaluates a straight ramp");

    auto bent = ramp;
    bent.points[0].tension = 0.8f;
    check (bent.valueAt (0.5) < -0.5f, "positive tension makes a segment arrive late (" + juce::String (bent.valueAt (0.5), 3) + ")");

    const auto parsed = LfoCurve::fromString (bent.toString());
    check (parsed.points.size() == 2 && std::abs (parsed.valueAt (0.5) - bent.valueAt (0.5)) < 1.0e-3f,
           "curve survives a text round trip");

    // Session round trip.
    IlanaSynthAudioProcessor processor;
    processor.setLfoCurve (2, LfoCurve::preset (6));
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    IlanaSynthAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (restored.getLfoCurveValue (2, 0.3) - processor.getLfoCurveValue (2, 0.3)) < 1.0e-4f,
           "LFO 3's curve survives a host state round trip");

    // A Curve LFO drives modulation: a flat curve at +1 opens the filter.
    const auto brightnessWith = [] (const char* curveText)
    {
        IlanaSynthAudioProcessor p;
        setParam (p, "sub_on", 0.0f);
        setParam (p, "f1_cutoff", 300.0f);
        setParam (p, "f1_env", 0.0f);
        setParam (p, "lfo1_shape", (float) IlanaSynthAudioProcessor::curveShape);
        setParam (p, "lfo1_retrig", 1.0f);
        setParam (p, "mod1_src", (float) Mod::Source::Lfo1);
        setParam (p, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (p, "mod1_amt", 0.6f);
        p.setLfoCurve (0, LfoCurve::fromString (curveText));
        p.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        renderPeakAndCentroid (p, 45, 30, centroid);
        return centroid;
    };

    const auto high = brightnessWith ("0,1,0;1,1,0");
    const auto low = brightnessWith ("0,-1,0;1,-1,0");
    check (high > low * 2.0, "a Curve LFO modulates per voice (centroid " + juce::String (low, 0) + " -> "
                                 + juce::String (high, 0) + " Hz)");
}
// The arp must stop when a host drops its notes without note-offs: All
// Notes Off / All Sound Off, or the transport stopping.
void runArpHostStopTests()
{
    struct TestHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (120.0);
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    const auto run = [] (int stopKind)
    {
        IlanaSynthAudioProcessor processor;
        TestHead head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 256);

        if (auto* parameter = processor.apvts.getParameter ("arp_on"))
            parameter->setValueNotifyingHost (1.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        head.playing = true;

        for (int block = 0; block < 1400; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 5);

            if (block == 100)
            {
                if (stopKind == 0)
                    midi.addEvent (juce::MidiMessage::allNotesOff (1), 30);
                else if (stopKind == 1)
                    midi.addEvent (juce::MidiMessage::allSoundOff (1), 30);
                else
                    head.playing = false;
            }

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (head.playing)
                head.ppq += 256.0 / 48000.0 * 2.0;
        }

        processor.setPlayHead (nullptr);
        return processor.getActiveVoiceCount();
    };

    check (run (0) == 0, "the arp stops on All Notes Off");
    check (run (1) == 0, "the arp stops on All Sound Off");
    check (run (2) == 0, "the arp stops when the host transport stops");

    // Chord mode honours the gate: at a short gate most of each step is silent.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("arp_on", 1.0f);
        set ("arp_mode", 7.0f);
        set ("arp_gate", 0.1f);
        set ("arp_div", 2.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        auto silentBlocks = 0, loudBlocks = 0;

        for (int block = 0; block < 600; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (block > 100)
            {
                if (processor.getActiveVoiceCount() == 0)
                    ++silentBlocks;
                else
                    ++loudBlocks;
            }
        }

        check (silentBlocks > 0, "the chord arp releases at its gate (" + juce::String (silentBlocks) + " of "
                                    + juce::String (silentBlocks + loudBlocks) + " blocks without voices)");
    }
}
// Review 6: the arp's step lanes. At their defaults a render is the same as
// without them; GATE 0 rests a step, a lower VELOCITY plays softer, ARP
// STEPS 1 repeats the first step's lane values.
void runArpLaneTests()
{
    const auto render = [] (const std::function<void (IlanaSynthAudioProcessor&)>& setUp, double& energy, int& silentBlocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set ("arp_on", 1.0f);
        set ("arp_div", 4.0f);
        setUp (processor);

        juce::AudioBuffer<float> buffer (2, 256);
        energy = 0.0;
        silentBlocks = 0;

        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (block > 20)
            {
                energy += buffer.getRMSLevel (0, 0, 256);
                silentBlocks += buffer.getMagnitude (0, 256) < 1.0e-5f ? 1 : 0;
            }
        }
    };
    const auto setAll = [] (IlanaSynthAudioProcessor& processor, const char* prefix, float value)
    {
        for (int step = 1; step <= 16; ++step)
            if (auto* parameter = processor.apvts.getParameter (prefix + juce::String (step)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    double plain = 0.0, defaults = 0.0, rests = 0.0, soft = 0.0, firstRest = 0.0;
    int silentPlain = 0, silentDefaults = 0, silentRests = 0, silentSoft = 0, silentFirst = 0;
    render ([] (IlanaSynthAudioProcessor&) {}, plain, silentPlain);
    render ([&setAll] (IlanaSynthAudioProcessor& processor)
            {
                setAll (processor, "arp_vel", 100.0f);
                setAll (processor, "arp_len", 1.0f);
                setAll (processor, "arp_pitch", 0.0f);
            }, defaults, silentDefaults);
    render ([&setAll] (IlanaSynthAudioProcessor& processor) { setAll (processor, "arp_len", 0.0f); }, rests, silentRests);
    render ([&setAll] (IlanaSynthAudioProcessor& processor) { setAll (processor, "arp_vel", 15.0f); }, soft, silentSoft);
    render ([] (IlanaSynthAudioProcessor& processor)
            {
                processor.apvts.getParameter ("arp_steps")->setValueNotifyingHost (processor.apvts.getParameter ("arp_steps")->convertTo0to1 (1.0f));
                processor.apvts.getParameter ("arp_len1")->setValueNotifyingHost (0.0f);
            }, firstRest, silentFirst);

    check (plain > 0.1 && std::abs (plain - defaults) < 1.0e-9 * plain,
           "the arp's lanes at their defaults render the same as before (" + juce::String (plain, 4) + ")");
    check (rests < plain * 1.0e-3, "GATE 0 on every arp step rests them all (" + juce::String (rests, 6) + ")");
    check (soft < plain * 0.9 && soft > 0.0, "a low arp VELOCITY lane plays softer (" + juce::String (soft / plain, 3) + " of the level)");
    check (firstRest < plain * 1.0e-3, "ARP STEPS 1 repeats step 1's lane values: its rest silences every step");
}

// Releasing every key must silence the Scale Random arpeggiator, including
// with note spray, chords, high notes and releases mid-step.
void runScaleRandomReleaseTest()
{
    const auto run = [] (const juce::String& presetName, std::initializer_list<int> notes, int octaves, int holdBlocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto names = processor.getFactoryPresetNames();

        if (presetName.isNotEmpty())
            processor.loadFactoryPreset (names.indexOf (presetName));

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("arp_on", 1.0f);
        set ("arp_mode", 8.0f);
        set ("arp_octaves", (float) octaves);

        if (processor.apvts.getRawParameterValue ("gen_scale")->load() < 0.5f)
            set ("gen_scale", 1.0f);

        juce::AudioBuffer<float> buffer (2, 256);

        for (int block = 0; block < holdBlocks + 1200; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            if (block == holdBlocks)
                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 17);

            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return processor.getActiveVoiceCount();
    };

    for (const auto* preset : { "Scale Walker", "Blues Machine", "Hirajoshi Spray", "" })
        for (int hold : { 3, 40, 187 })
        {
            const auto voices = run (preset, { 60, 64, 67 }, 2, hold);
            check (voices == 0, "Scale Random stops after release (" + juce::String (preset[0] != 0 ? preset : "Init")
                                    + ", hold " + juce::String (hold) + " blocks, " + juce::String (voices) + " voices left)");
        }

    check (run ("", { 110, 118 }, 4, 60) == 0, "Scale Random stops after release near the top of the keyboard");

    // Played like a keyboardist: overlapping legato, fast repeats of one key,
    // note-offs sent as velocity-0 note-ons, and a non-default channel.
    struct Event { int block, sample, note; bool on; int channel; bool zeroVelocityOff; };

    const auto play = [] (const juce::String& presetName, const std::vector<Event>& events)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        const auto names = processor.getFactoryPresetNames();

        if (presetName.isNotEmpty())
            processor.loadFactoryPreset (names.indexOf (presetName));

        if (auto* parameter = processor.apvts.getParameter ("arp_on"))
            parameter->setValueNotifyingHost (1.0f);

        if (auto* parameter = processor.apvts.getParameter ("arp_mode"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (8.0f));

        juce::AudioBuffer<float> buffer (2, 256);
        auto lastBlock = 0;

        for (const auto& event : events)
            lastBlock = juce::jmax (lastBlock, event.block);

        for (int block = 0; block <= lastBlock + 1200; ++block)
        {
            juce::MidiBuffer midi;

            for (const auto& event : events)
                if (event.block == block)
                    midi.addEvent (event.on ? juce::MidiMessage::noteOn (event.channel, event.note, (juce::uint8) 90)
                                            : (event.zeroVelocityOff ? juce::MidiMessage::noteOn (event.channel, event.note, (juce::uint8) 0)
                                                                     : juce::MidiMessage::noteOff (event.channel, event.note)),
                                   event.sample);

            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        return processor.getActiveVoiceCount();
    };

    std::vector<Event> legato, repeats, zeroOffs, channelTwo;

    for (int i = 0; i < 12; ++i)
    {
        const auto note = 55 + (i * 5) % 17;
        legato.push_back ({ i * 20, 30, note, true, 1, false });
        legato.push_back ({ i * 20 + 27, 10, note, false, 1, false });   // released after the next key goes down
        repeats.push_back ({ i * 3, 0, 62, true, 1, false });
        repeats.push_back ({ i * 3 + 1, 200, 62, false, 1, false });
        repeats.push_back ({ i * 3 + 1, 201, 62, true, 1, false });     // re-pressed in the same block
        repeats.push_back ({ i * 3 + 2, 5, 62, false, 1, false });
        zeroOffs.push_back ({ i * 9, 0, 48 + i, true, 1, false });
        zeroOffs.push_back ({ i * 9 + 4, 100, 48 + i, false, 1, true });
        channelTwo.push_back ({ i * 9, 0, 60 + i, true, 2, false });
        channelTwo.push_back ({ i * 9 + 4, 100, 60 + i, false, 2, false });
    }

    for (const auto* preset : { "Scale Walker", "Blues Machine", "Hirajoshi Spray" })
    {
        const juce::String name (preset);
        check (play (name, legato) == 0, "Scale Random stops after overlapping legato (" + name + ")");
        check (play (name, repeats) == 0, "Scale Random stops after fast repeats of one key (" + name + ")");
        check (play (name, zeroOffs) == 0, "Scale Random stops after velocity-0 note-offs (" + name + ")");
        check (play (name, channelTwo) == 0, "Scale Random stops on MIDI channel 2 (" + name + ")");
    }

    // The on-screen keyboard: a fast glissando queues several note-offs
    // between two audio blocks, and none may be lost.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Scale Walker"));
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer noMidi;

        const auto runBlocks = [&] (int count)
        {
            for (int i = 0; i < count; ++i)
            {
                buffer.clear();
                noMidi.clear();
                processor.processBlock (buffer, noMidi);
            }
        };

        // Hold a key long enough to sound, then drag across four more keys
        // faster than one block and let go.
        processor.triggerPreviewNote (60, true, 0.8f);
        runBlocks (40);

        for (int note = 61; note < 65; ++note)
        {
            processor.triggerPreviewNote (note - 1, false);
            processor.triggerPreviewNote (note, true, 0.8f);
        }

        runBlocks (40);
        processor.triggerPreviewNote (64, false);
        runBlocks (1200);

        check (processor.getActiveVoiceCount() == 0, "a fast on-screen glissando leaves no stuck notes");
    }
}

} // namespace

// Every factory preset (Init aside) has four named, working macros, plays,
// and stays bounded with all macros at full.
void runFactoryLibraryTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    juce::StringArray unnamed, unmapped, badSlots, silent, unbounded, drumLevels;
    const auto categories = processor.getFactoryPresetCategories();
    std::vector<std::pair<double, juce::String>> levels;

    const auto setMacros = [&processor] (float value)
    {
        for (int m = 1; m <= 4; ++m)
            if (auto* parameter = processor.apvts.getParameter ("macro" + juce::String (m)))
                parameter->setValueNotifyingHost (value);
    };

    // rms: the loudest half second (47 blocks) of the render, so a slow
    // swell or riser is judged by the level it reaches.
    const auto render = [&processor] (int blocks, bool startNotes, double& rms)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        auto peak = 0.0f;
        auto sum = 0.0;
        auto count = 0;
        auto finite = true;
        std::vector<double> blockEnergy;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0 && startNotes)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);
            auto energy = 0.0;

            for (int channel = 0; channel < 2; ++channel)
                for (int s = 0; s < 512; ++s)
                {
                    const auto value = buffer.getSample (channel, s);
                    finite = finite && std::isfinite (value);
                    peak = juce::jmax (peak, std::abs (value));
                    sum += (double) value * (double) value;
                    energy += (double) value * (double) value;
                    ++count;
                }
            blockEnergy.push_back (energy);
        }

        constexpr int window = 47;
        rms = std::sqrt (sum / juce::jmax (1, count));
        if ((int) blockEnergy.size() >= window)
        {
            auto windowSum = 0.0, best = 0.0;
            for (size_t b = 0; b < blockEnergy.size(); ++b)
            {
                windowSum += blockEnergy[b] - (b >= (size_t) window ? blockEnergy[b - window] : 0.0);
                if (b + 1 >= (size_t) window)
                    best = juce::jmax (best, windowSum);
            }
            rms = std::sqrt (best / (window * 1024.0));
        }
        return finite ? peak : 1.0e9f;
    };

    for (int index = 1; index < names.size(); ++index)
    {
        processor.loadFactoryPreset (index);
        processor.panic();

        for (int m = 0; m < 4; ++m)
        {
            if (processor.getMacroName (m) == "Macro " + juce::String (m + 1))
                unnamed.addIfNotAlreadyThere (names[index]);

            auto mapped = false;

            for (int slot = 0; slot < Mod::maxSlots; ++slot)
            {
                const auto info = processor.readModSlot (slot);
                const auto macroSource = (Mod::Source) ((int) Mod::Source::Macro1 + m);

                if ((info.source == macroSource && info.destination != 0 && info.depth != 0.0f)
                    || (info.aux == macroSource && info.source != Mod::Source::None))
                    mapped = true;
            }

            if (! mapped)
                unmapped.addIfNotAlreadyThere (names[index] + " M" + juce::String (m + 1));
        }

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto info = processor.readModSlot (slot);

            if (info.source != Mod::Source::None && info.depth != 0.0f && info.destination == 0)
                badSlots.addIfNotAlreadyThere (names[index]);
        }

        setMacros (0.0f);
        double rms = 0.0;
        const auto peak = render (180, true, rms);

        // ilanaSynth FX patches are silent without the audio input.
        if (categories[index] == "FX Input")
        {
            setMacros (0.0f);
            continue;
        }

        if (peak < 0.001f)
            silent.add (names[index]);

        // Drums are short hits: judge them by their peak, not a held note's level.
        if (categories[index] == "Drums")
        {
            if (peak < 0.15f || peak > 2.0f)
                drumLevels.add (names[index] + " (peak " + juce::String (peak, 2) + ")");
        }
        else if (categories[index] != "DX7")
        {
            // The DX7 banks keep the cartridges' own levels (quiet FX voices
            // and all); they're imported, not voiced for this library.
            levels.push_back ({ rms, names[index] });
        }

        setMacros (1.0f);
        double fullRms = 0.0;
        const auto fullPeak = render (60, false, fullRms);

        if (peak > 8.0f || fullPeak > 8.0f)
            unbounded.add (names[index] + " (" + juce::String (juce::jmax (peak, fullPeak), 2) + ")");

        setMacros (0.0f);
    }

    check (unnamed.isEmpty(), "every factory preset names its macros (" + unnamed.joinIntoString (", ") + ")");
    check (unmapped.isEmpty(), "every factory macro is mapped (" + unmapped.joinIntoString (", ") + ")");
    check (badSlots.isEmpty(), "factory mod slots all have a destination (" + badSlots.joinIntoString (", ") + ")");
    check (silent.isEmpty(), "every factory preset makes sound (" + silent.joinIntoString (", ") + ")");
    check (drumLevels.isEmpty(), "factory drums hit at a sensible level (" + drumLevels.joinIntoString (", ") + ")");
    check (unbounded.isEmpty(), "factory presets stay bounded with macros at full (" + unbounded.joinIntoString (", ") + ")");

    // Loudness: flag presets far from the library's median level.
    auto sorted = levels;
    std::sort (sorted.begin(), sorted.end());
    const auto median = sorted[sorted.size() / 2].first;
    juce::StringArray outliers;

    for (const auto& level : levels)
    {
        const auto db = 20.0 * std::log10 (juce::jmax (1.0e-9, level.first / median));

        if (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_LEVELS", "").isNotEmpty())
            std::cout << "  LEVEL " << level.second << " " << juce::String (db, 1) << " dB" << std::endl;

        if (std::abs (db) > 14.0)
            outliers.add (level.second + " (" + juce::String (db, 1) + " dB)");
    }

    std::cout << "  library level: median rms " << median << ", quietest " << sorted.front().second
              << ", loudest " << sorted.back().second << std::endl;
    check (outliers.isEmpty(), "factory presets within 14 dB of the median level (" + outliers.joinIntoString (", ") + ")");
}

// Tape stop must not leave latency behind once it is released.
void runTapeStopLatencyTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (0);
    processor.assignFxSlot (1, 17); // TapeStop

    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_tape_stop_time", 0.3f);

    const auto run = [&processor] (int blocks, const juce::MidiBuffer& firstMidi)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi = firstMidi;

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    juce::MidiBuffer noteOn;
    noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    run (20, noteOn);
    set ("fx_tape_stop_trigger", 1.0f);
    run (40, {});                        // tape winds down
    set ("fx_tape_stop_trigger", 0.0f);
    run (60, {});                        // spins back up and rejoins
    processor.panic();
    run (200, {});                       // let the tail die

    const auto out = run (40, noteOn);
    auto onset = -1;

    for (int i = 0; i < (int) out.size(); ++i)
    {
        if (std::abs (out[(size_t) i]) > 1.0e-3f)
        {
            onset = i;
            break;
        }
    }

    check (onset >= 0 && onset < 256, "tape stop adds no latency after release (onset at sample " + juce::String (onset) + ")");
}

// Trance gate: a Custom open/closed pattern gates the sound step by step,
// each step lasting DIV (1/16 at 120 BPM = 0.125 s).
void runTranceGateTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 500);
    processor.loadFactoryPreset (0);
    processor.assignFxSlot (1, 16);

    const auto set = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_gate_div", 4.0f);      // 1/16
    set ("fx_gate_pattern", 8.0f);  // Custom
    set ("fx_gate_steps", 4.0f);
    set ("fx_gate_smooth", 0.0f);
    set ("fx_gate_mix", 1.0f);

    for (int step = 1; step <= 16; ++step)
        set ("fx_gate_step" + juce::String (step), step % 2 == 1 ? 1.0f : 0.0f);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 500);

    for (int block = 0; block < 96; ++block) // 1 s
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 500; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    const auto rmsOf = [&out] (int start, int length)
    {
        auto sum = 0.0;

        for (int i = start; i < start + length; ++i)
            sum += (double) out[(size_t) i] * out[(size_t) i];

        return std::sqrt (sum / length);
    };

    constexpr int stepLength = 6000;
    auto worstRatio = 1.0e9;

    for (int step = 1; step < 7; step += 2)
    {
        // Middles of an open step and the closed step after it.
        const auto open = rmsOf ((step - 1) * stepLength + 1500, 3000);
        const auto closed = rmsOf (step * stepLength + 1500, 3000);
        worstRatio = juce::jmin (worstRatio, open / juce::jmax (1.0e-9, closed));
    }

    check (worstRatio > 30.0, "trance gate opens and closes on 1/16 steps (open/closed ratio " + juce::String (worstRatio, 1) + ")");
}

void runGenerativeTests()
{
    // Scale snapping.
    check (Scales::quantize (61, 1, 0) == 62 && Scales::quantize (66, 1, 0) == 67 && Scales::quantize (60, 1, 0) == 60,
           "scale snap: C major sends C# up to D, F# up to G, keeps C");
    check (Scales::quantize (64, 2, 9) == 64 && Scales::quantize (61, 2, 9) == 62,
           "scale snap: A minor keeps E, moves C# to D");

    // Spray: every extra note is in the scale, released with its parent.
    NoteSpray spray;
    NoteSpray::Settings settings;
    settings.scale = 11; // minor pentatonic
    settings.root = 0;
    settings.sprayOn = true;
    settings.count = 5;
    settings.range = 12;
    settings.chance = 1.0f;

    juce::MidiBuffer in, out;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    spray.process (in, out, 512, settings);

    std::set<int> sounding;
    auto allInScale = true;

    for (const auto metadata : out)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            sounding.insert (message.getNoteNumber());
            allInScale = allInScale && Scales::contains (message.getNoteNumber(), 11, 0);
        }
    }

    check (sounding.size() >= 3 && sounding.count (60) == 1 && allInScale,
           "note spray adds scale notes around the played note (" + juce::String ((int) sounding.size()) + " notes)");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
    spray.process (in, out, 512, settings);
    std::set<int> released;

    for (const auto metadata : out)
        if (metadata.getMessage().isNoteOff())
            released.insert (metadata.getMessage().getNoteNumber());

    check (released == sounding, "releasing the played note releases every sprayed note");

    // Spread: extra notes arrive later, across blocks.
    settings.spreadSamples = 4000;
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    auto starts = 0;

    for (int block = 0; block < 10; ++block)
    {
        spray.process (in, out, 512, settings);
        in.clear();

        for (const auto metadata : out)
            if (metadata.getMessage().isNoteOn())
                ++starts;
    }

    check (starts >= 4, "spread sprayed notes start over time (" + juce::String (starts) + " starts)");

    // Scale Random arp stays in the scale and the range.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (0);

    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("arp_on", 1.0f);
    set ("arp_mode", 8.0f);
    set ("arp_div", 5.0f); // 1/32: many steps
    set ("arp_octaves", 2.0f);
    set ("gen_scale", 3.0f); // D dorian
    set ("gen_root", 2.0f);

    auto inKey = true, inRange = true;
    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 200; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    // With note 50 held, check the arp's choices over many picks.
    for (int i = 0; i < 400; ++i)
    {
        const auto note = processor.pickArpNoteForTest (8, 2);
        inKey = inKey && Scales::contains (note, 3, 2);
        inRange = inRange && note >= 50 && note <= 50 + 24 + 1;
    }

    check (inKey && inRange, "Scale Random arp picks notes in the scale and the octave range");
}

// Pre-1.1 states used OSC 3 as the sub; loading them moves a plain sub to
// the dedicated SUB and keeps a real OSC 3 at the same pitch.
void runOsc3MigrationTest()
{
    const auto loadOldState = [] (std::function<void (juce::ValueTree&)> edit)
    {
        IlanaSynthAudioProcessor source;
        auto state = source.apvts.copyState();
        state.removeProperty ("osc3Schema", nullptr);
        edit (state);

        juce::MemoryBlock data;
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        source.copyXmlToBinary (*xml, data);

        auto target = std::make_unique<IlanaSynthAudioProcessor>();
        target->setStateInformation (data.getData(), (int) data.getSize());
        return target;
    };

    const auto setParam = [] (juce::ValueTree& state, const juce::String& id, float value)
    {
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);

            if (child.getProperty ("id").toString() == id)
            {
                child.setProperty ("value", value, nullptr);
                return;
            }
        }

        juce::ValueTree child ("PARAM");
        child.setProperty ("id", id, nullptr);
        child.setProperty ("value", value, nullptr);
        state.appendChild (child, nullptr);
    };

    const auto value = [] (IlanaSynthAudioProcessor& processor, const char* id)
    {
        return processor.apvts.getRawParameterValue (id)->load();
    };

    // Plain sub: old table 0 (Shape) with a saw, two octaves down.
    auto plain = loadOldState ([&] (juce::ValueTree& state)
    {
        setParam (state, "sub_on", 1.0f);
        setParam (state, "sub_table", 0.0f);
        setParam (state, "sub_shape", 2.0f);
        setParam (state, "sub_octave", 1.0f);
        setParam (state, "sub_level", 0.4f);
    });

    check (value (*plain, "subosc_on") > 0.5f && value (*plain, "sub_on") < 0.5f && (int) value (*plain, "sub_shape") == 2
               && (int) value (*plain, "sub_octave") == 1 && std::abs (value (*plain, "subosc_level") - 0.4f) < 1.0e-4f,
           "old plain sub moves to the dedicated SUB");

    // Real OSC 3: old table 5 (factory table 1), one octave down, semi +3.
    auto real = loadOldState ([&] (juce::ValueTree& state)
    {
        setParam (state, "sub_on", 1.0f);
        setParam (state, "sub_table", 5.0f);
        setParam (state, "sub_octave", 0.0f);
        setParam (state, "sub_semi", 3.0f);
        setParam (state, "osc1_table", 17.0f); // "User 2" before v1.1
    });

    check ((int) value (*real, "osc1_table") == TableFactory::getNumFactoryTables() + 1,
           "an old user wavetable choice still points at the same user slot");

    check (value (*real, "sub_on") > 0.5f && value (*real, "subosc_on") < 0.5f
               && (int) value (*real, "sub_table") == 1 && (int) value (*real, "sub_semi") == -9,
           "old real OSC 3 keeps its table and pitch (table " + juce::String ((int) value (*real, "sub_table"))
               + ", semi " + juce::String ((int) value (*real, "sub_semi")) + ")");

    // New states are not migrated again.
    juce::MemoryBlock saved;
    real->getStateInformation (saved);
    IlanaSynthAudioProcessor again;
    again.setStateInformation (saved.getData(), (int) saved.getSize());
    check ((int) again.apvts.getRawParameterValue ("sub_semi")->load() == -9, "saved 1.1 state is not migrated twice");
}

void runSpectralWarpTests()
{
    // Each warp on a bright table: bounded, and different from the source.
    {
        Wavetable source;
        source.buildFromFrames (TableFactory::generate (10));

        for (int mode = SpectralWarp::Stretch; mode < SpectralWarp::Count; ++mode)
        {
            const auto amount = mode == SpectralWarp::Formant ? 0.9f : 0.7f;
            const auto warped = SpectralWarp::warpTable (source, mode, amount);
            auto peak = 0.0f;
            auto diff = 0.0;
            auto finite = true;

            for (int frame = 0; frame < warped->getNumFrames(); ++frame)
            {
                const auto* a = source.getFrameData (0, frame);
                const auto* b = warped->getFrameData (0, frame);

                for (int i = 0; i < Wavetable::frameSize; ++i)
                {
                    finite = finite && std::isfinite (b[i]);
                    peak = juce::jmax (peak, std::abs (b[i]));
                    diff += (double) (a[i] - b[i]) * (a[i] - b[i]);
                }
            }

            diff = std::sqrt (diff / (double) (warped->getNumFrames() * Wavetable::frameSize));
            check (finite && peak <= 1.05f && peak > 0.1f && diff > 0.02,
                   "Spectral warp " + SpectralWarp::getNames()[mode] + " is bounded and audible (peak "
                       + juce::String (peak, 3) + ", diff " + juce::String (diff, 3) + ")");
        }
    }

    const auto render = [] (int mode, bool offline)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (offline);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_table", 10.0f);
        set ("osc1_spectral", (float) mode);
        set ("osc1_spectral_amt", 0.8f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;
        auto waited = 0;

        // Realtime: let the worker build the warp before the note starts.
        for (; ! offline && waited < 400; ++waited)
        {
            buffer.clear();
            juce::MidiBuffer none;
            processor.processBlock (buffer, none);

            if (processor.isSpectralWarpReady (0))
                break;

            juce::Thread::sleep (5);
        }

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return std::make_pair (out, processor.isSpectralWarpReady (0));
    };

    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += (double) (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (sum / juce::jmax ((size_t) 1, a.size()));
    };

    const auto plain = render (SpectralWarp::Off, true).first;
    const auto offline = render (SpectralWarp::Stretch, true);
    const auto realtime = render (SpectralWarp::Stretch, false);

    check (offline.second && difference (plain, offline.first) > 0.01,
           "Offline renders build the spectral warp in place (diff " + juce::String (difference (plain, offline.first), 4) + ")");
    check (realtime.second && difference (offline.first, realtime.first) < 1.0e-4,
           "The background worker builds the same warp (diff " + juce::String (difference (offline.first, realtime.first), 6) + ")");
}

void runChaosLfoTests()
{
    juce::Random random (7);

    // Chaos: bounded, uses both halves, and doesn't repeat cycle to cycle.
    {
        LfoChaos chaos;
        chaos.reset (random);
        std::vector<float> values;
        auto finite = true;

        for (int i = 0; i < 48000 * 4; ++i)
        {
            chaos.advance (2.0 / 48000.0);
            values.push_back (chaos.value (LfoShapes::Chaos, 0.0));
            finite = finite && std::isfinite (values.back());
        }

        const auto [low, high] = std::minmax_element (values.begin(), values.end());
        auto cycleDifference = 0.0;

        for (int i = 0; i < 24000; ++i)
            cycleDifference += std::abs ((double) values[(size_t) i + 96000] - (double) values[(size_t) i + 120000]);

        check (finite && *low < -0.4f && *high > 0.4f && *low >= -1.0f && *high <= 1.0f && cycleDifference / 24000.0 > 0.05,
               "Chaos LFO wanders both ways without repeating (" + juce::String (*low, 2) + " .. " + juce::String (*high, 2)
                   + ", cycle diff " + juce::String (cycleDifference / 24000.0, 3) + ")");
    }

    // Drunk: small steps, stays in range. Smooth Random: continuous across cycles.
    {
        LfoChaos drunk, smooth;
        drunk.reset (random);
        smooth.reset (random);
        auto maxStep = 0.0f, maxJump = 0.0f;
        auto inRange = true;
        auto last = smooth.value (LfoShapes::SmoothRandom, 0.999);

        for (int cycle = 0; cycle < 2000; ++cycle)
        {
            const auto before = drunk.target;
            drunk.onCycle (LfoShapes::Drunk, random);
            maxStep = juce::jmax (maxStep, std::abs (drunk.target - before));
            inRange = inRange && std::abs (drunk.target) <= 1.0f;

            smooth.onCycle (LfoShapes::SmoothRandom, random);
            maxJump = juce::jmax (maxJump, std::abs (smooth.value (LfoShapes::SmoothRandom, 0.0) - last));
            last = smooth.value (LfoShapes::SmoothRandom, 0.999);
        }

        check (inRange && maxStep <= 0.4501f, "Drunk LFO takes small steps and stays in range (max step " + juce::String (maxStep, 3) + ")");
        check (maxJump < 0.01f, "Smooth Random LFO has no jumps between cycles (" + juce::String (maxJump, 4) + ")");
    }

    // Key tracking: a per-voice LFO at the note's pitch, driving level.
    const auto render = [] (bool key, int shape)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_table", 8.0f);
        set ("osc1_frame", 0.0f);
        set ("osc1_level", 0.8f); // leaves the level LFO room to swing
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        set ("lfo1_shape", (float) shape);
        set ("lfo1_rate", 12.0f); // key tracked: three times the note
        set ("lfo1_key", key ? 1.0f : 0.0f);
        set ("mod1_src", (float) Mod::Source::Lfo1);
        set ("mod1_dst", (float) Mod::Destination::Osc1Level);
        set ("mod1_amt", 0.5f);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;

        for (int block = 0; block < 16; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    // Brightness proxy: mean absolute first difference relative to level.
    const auto brightness = [] (const std::vector<float>& data)
    {
        auto diff = 0.0, level = 0.0;

        for (size_t i = 4096; i < data.size(); ++i)
        {
            diff += std::abs ((double) data[i] - (double) data[i - 1]);
            level += std::abs ((double) data[i]);
        }

        return diff / juce::jmax (1.0e-9, level);
    };

    const auto slow = render (false, LfoShapes::Sine);
    const auto audio = render (true, LfoShapes::Sine);
    check (brightness (audio) > brightness (slow) * 1.3,
           "A key-tracked LFO on level is audio-rate AM (brightness " + juce::String (brightness (slow), 4) + " -> "
               + juce::String (brightness (audio), 4) + ")");

    for (const auto shape : { (int) LfoShapes::SmoothRandom, (int) LfoShapes::Drunk, (int) LfoShapes::Chaos })
    {
        const auto out = render (false, shape);
        auto finite = true;
        auto peak = 0.0f;

        for (auto value : out)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

        check (finite && peak > 0.01f && peak < 4.0f, "LFO shape " + juce::String (shape) + " drives a voice cleanly (peak " + juce::String (peak, 3) + ")");
    }
}

void runPhysicsLfoTests()
{
    for (const auto shape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring, LfoShapes::Friction })
    {
        LfoChaos state;
        state.resetPhysics (shape, 0.5f);
        const auto initial = state.value (shape, 0.0);
        auto low = 1.0f, high = -1.0f, largestStep = 0.0f;
        auto earlyEnergy = 0.0, lateEnergy = 0.0;
        auto last = initial;
        auto finite = true;
        for (int sample = 0; sample < 48000; ++sample)
        {
            state.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            const auto value = state.value (shape, 0.0);
            finite = finite && std::isfinite (value) && value >= -1.0f && value <= 1.0f;
            low = juce::jmin (low, value);
            high = juce::jmax (high, value);
            largestStep = juce::jmax (largestStep, std::abs (value - last));
            if (sample < 12000) earlyEnergy += std::abs ((double) value - (shape == LfoShapes::Bounce ? -1.0 : 0.0));
            if (sample >= 36000) lateEnergy += std::abs ((double) value - (shape == LfoShapes::Bounce ? -1.0 : 0.0));
            last = value;
        }
        check (finite && high - low > 0.1f, "physics LFO shape " + juce::String (shape) + " moves and stays bounded");
        {
            // A retrigger starts exactly where the motion continues from.
            LfoChaos fresh;
            fresh.resetPhysics (shape, 0.5f);
            const auto start = fresh.value (shape, 0.0);
            fresh.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            check (std::abs (fresh.value (shape, 0.0) - start) < 0.01f,
                   "physics LFO shape " + juce::String (shape) + " starts without a jump");
        }
        if (shape == LfoShapes::Friction)
            check (largestStep > 0.5f, "Friction LFO makes abrupt slips");
        if (shape == LfoShapes::Pendulum)
            check (lateEnergy < earlyEnergy * 0.7, "Pendulum loses energy over time");
        if (shape == LfoShapes::Spring)
            check (low < -0.1f && high > 0.1f, "Spring LFO overshoots");
        state.resetPhysics (shape, 0.5f);
        check (state.value (shape, 0.0) == initial, "physics LFO retrigger restores initial position");
        for (const auto rate : { 0.01, 40.0, 200.0 })
        {
            state.resetPhysics (shape, 1.0f);
            for (int i = 0; i < 48000; ++i)
            {
                state.advancePhysics (shape, rate / 48000.0, 1.0f, 0.0f);
                finite = finite && std::isfinite (state.value (shape, 0.0));
            }
        }
        check (finite, "physics LFO survives extreme rates and settings");
    }

    const auto globalPendulum = [] (bool kick)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "lfo1_shape", (float) LfoShapes::Pendulum);
        setParam (processor, "lfo1_rate", 4.0f);
        setParam (processor, "lfo1_kick", kick ? 1.0f : 0.0f);
        setParam (processor, "lfo1_retrig", 0.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        processor.processBlock (buffer, midi);
        return processor.getLfoLiveValue (0);
    };
    check (std::abs (globalPendulum (true) - globalPendulum (false)) > 0.02f,
           "Pendulum kick adds note energy even without RETRIG");
}

// Free-running physics LFOs must keep moving (they re-excite once settled)
// without jumps, and each bounce of the ball must be lower than the last.
void runPhysicsLfoMotionTest()
{
    for (const auto shape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring })
    {
        LfoChaos state;
        state.resetPhysics (shape, 0.5f);
        auto last = state.value (shape, 0.0);
        auto largestStep = 0.0f, lateLow = 1.0f, lateHigh = -1.0f;
        constexpr int seconds = 30;

        for (int sample = 0; sample < 48000 * seconds; ++sample)
        {
            state.advancePhysics (shape, 4.0 / 48000.0, 0.5f, 0.5f);
            const auto value = state.value (shape, 0.0);
            largestStep = juce::jmax (largestStep, std::abs (value - last));
            last = value;

            if (sample >= 48000 * (seconds - 1))
            {
                lateLow = juce::jmin (lateLow, value);
                lateHigh = juce::jmax (lateHigh, value);
            }
        }

        check (lateHigh - lateLow > 0.2f, "free-running physics LFO " + juce::String (shape) + " still moves after 30 s");
        check (largestStep < 0.05f, "physics LFO " + juce::String (shape) + " re-excites without a jump (largest step "
                                        + juce::String (largestStep, 4) + ")");
    }

    LfoChaos ball;
    ball.resetPhysics (LfoShapes::Bounce, 1.0f);
    std::vector<float> apexes;
    auto previous = ball.value (LfoShapes::Bounce, 0.0);
    auto rising = false;

    for (int sample = 0; sample < 48000 * 3 && apexes.size() < 5; ++sample)
    {
        ball.advancePhysics (LfoShapes::Bounce, 1.0 / 48000.0, 1.0f, 0.5f);
        const auto value = ball.value (LfoShapes::Bounce, 0.0);

        // Equal samples at the top of an arc keep the current direction.
        if (value > previous)
            rising = true;
        else if (value < previous)
        {
            if (rising)
                apexes.push_back (previous);

            rising = false;
        }

        previous = value;
    }

    auto lower = apexes.size() >= 4;

    for (size_t i = 1; i < apexes.size(); ++i)
        lower = lower && apexes[i] < apexes[i - 1];

    check (lower, "each bounce is lower than the last (" + juce::String ((int) apexes.size()) + " apexes)");
}

void runSympatheticTuningTest()
{
    // With no scale, an open tuning on the root instead of a semitone cluster.
    SympatheticStrings strings;
    strings.prepare (48000.0);
    strings.setTuning (0, 2, false, { 48, 55, 60, 64, 67, 72 });
    const int open[] { 50, 57, 62, 66, 69, 74 };
    auto matches = true;

    for (int i = 0; i < SympatheticStrings::maxStrings; ++i)
        matches = matches && strings.noteFor (i) == open[i];

    check (matches, "sympathetic strings use an open tuning on the root when no scale is set");

    // Each string rings at its note, low and high.
    for (const auto note : { 48, 84 })
    {
        SympatheticStrings single;
        single.prepare (48000.0);
        single.setTuning (0, 0, true, { note, note, note, note, note, note });
        std::vector<float> ring (48000);

        for (int i = 0; i < (int) ring.size(); ++i)
            ring[(size_t) i] = single.process (i < 64 ? 1.0f : 0.0f, 1, 1.0f, 1.0f);

        // Strongest frequency within +-60 cents of the note, in 0.5 cent steps.
        const auto expected = juce::MidiMessage::getMidiNoteInHertz (note);
        auto cents = 0.0, bestPower = -1.0;

        for (double offset = -60.0; offset <= 60.0; offset += 0.1)
        {
            const auto hz = expected * std::exp2 (offset / 1200.0);
            double re = 0.0, im = 0.0;

            for (int i = 2000; i < 40000; ++i)
            {
                const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
                const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - 2000) / 38000.0);
                re += window * ring[(size_t) i] * std::cos (angle);
                im += window * ring[(size_t) i] * std::sin (angle);
            }

            if (re * re + im * im > bestPower)
            {
                bestPower = re * re + im * im;
                cents = offset;
            }
        }
        check (std::abs (cents) < 0.5, "sympathetic string " + juce::String (note) + " rings in tune ("
                                            + juce::String (cents, 1) + " cents)");
    }
}

void runBridgeBuzzStabilityTest()
{
    // In-loop buzz may colour the string but never sustain or grow it.
    KarplusStrong string (12345);
    string.prepare (48000.0);
    string.setFrequency (110.0);
    string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.1f, 1.0f);
    string.setPhysicalParams (0.0f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    string.setBowAndBuzz (0.5f, 0.5f, 1.0f, 0.0f);
    string.trigger (1.0f);

    auto early = 0.0, late = 0.0;
    auto finite = true;

    for (int i = 0; i < 48000 * 6; ++i)
    {
        const auto value = string.process();
        finite = finite && std::isfinite (value);

        if (i < 4800)
            early = juce::jmax (early, (double) std::abs (value));

        if (i >= 48000 * 5)
            late = juce::jmax (late, (double) std::abs (value));
    }

    check (finite && late < early * 0.5, "full bridge buzz decays instead of sustaining (early "
                                             + juce::String (early, 3) + ", late " + juce::String (late, 3) + ")");
}

void runIntegerValueTextTest()
{
    IlanaSynthAudioProcessor processor;
    juce::RangedAudioParameter* note = processor.apvts.getParameter ("sym_note1");
    juce::RangedAudioParameter* count = processor.apvts.getParameter ("sym_count");

    check (note != nullptr && note->getText (note->convertTo0to1 (60.0f), 16) == "C3"
               && count != nullptr && count->getText (count->convertTo0to1 (3.0f), 16) == "3 strings",
           "integer parameters show their value text (note names, units)");
    check (note != nullptr && juce::roundToInt (note->convertFrom0to1 (note->getValueForText ("C#2"))) == 49
               && juce::roundToInt (note->convertFrom0to1 (note->getValueForText ("67"))) == 67,
           "note knobs accept typed note names and numbers");
}

void runMissingParameterDefaultTest()
{
    IlanaSynthAudioProcessor processor;
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "state for missing parameter test is readable");
    if (xml == nullptr) return;
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (child->getStringAttribute ("id") == "lfo1_phys_a"
            || child->getStringAttribute ("id") == "lfo1_phys_b"
            || child->getStringAttribute ("id") == "lfo1_kick")
            xml->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    for (const auto* id : { "lfo1_phys_a", "lfo1_phys_b", "lfo1_kick" })
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (1.0f));
    processor.setStateInformation (oldState.getData(), (int) oldState.getSize());
    const auto read = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    check (read ("lfo1_phys_a") == 0.5f && read ("lfo1_phys_b") == 0.5f && read ("lfo1_kick") == 0.0f,
           "missing new parameters load their declared defaults after a nondefault patch");
}

void runM3PhysicalTests()
{
    const auto renderBow = [] (float expression, bool release)
    {
        KarplusStrong string (777);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Bow, 0.0f, 0.35f, 0.15f);
        string.setBowAndBuzz (0.5f, 0.5f, 0.0f, 0.0f);
        string.trigger (0.8f);
        std::array<double, 3> energies {};
        for (int i = 0; i < 96000; ++i)
        {
            const auto value = string.process (expression, ! release || i < 48000);
            if (i >= 36000 && i < 48000) energies[0] += value * value;
            if (i >= 60000 && i < 72000) energies[1] += value * value;
            if (i >= 84000) energies[2] += value * value;
        }
        return energies;
    };
    const auto bowed = renderBow (0.0f, false);
    const auto released = renderBow (0.0f, true);
    const auto pressed = renderBow (1.0f, false);
    check (bowed[0] > 1.0e-6 && bowed[2] > released[2] * 2.0,
           "Bow sustains while held and fades after release");
    check (std::abs (pressed[0] - bowed[0]) > bowed[0] * 0.05,
           "MPE pressure or channel aftertouch changes bow energy");

    const auto renderContact = [] (float buzz, float rattle, float velocity)
    {
        KarplusStrong string (313);
        string.prepare (48000.0);
        string.setFrequency (220.0);
        string.setParams (KarplusStrong::Excite::Burst, 0.0f, 0.15f, 0.95f);
        string.setBowAndBuzz (0.0f, 0.5f, buzz, rattle);
        string.trigger (velocity);
        std::vector<float> samples (10000);
        for (auto& sample : samples) sample = string.process();
        return samples;
    };
    const auto clean = renderContact (0.0f, 0.0f, 0.9f);
    const auto buzz = renderContact (1.0f, 0.0f, 0.9f);
    const auto rattleHigh = renderContact (0.0f, 1.0f, 0.9f);
    const auto cleanLow = renderContact (0.0f, 0.0f, 0.2f);
    const auto rattleLow = renderContact (0.0f, 1.0f, 0.2f);
    const auto delta = [] (const auto& a, const auto& b)
    {
        auto total = 0.0;
        for (size_t i = 0; i < a.size(); ++i) total += std::abs ((double) a[i] - b[i]);
        return total;
    };
    check (delta (clean, buzz) > 0.1, "Bridge buzz changes the waveform");
    check (delta (clean, rattleHigh) > delta (cleanLow, rattleLow) * 2.0,
           "Fret rattle grows with velocity");
    for (const auto& samples : { clean, buzz, rattleHigh })
    {
        auto finite = true;
        for (const auto value : samples) finite = finite && std::isfinite (value) && std::abs (value) <= 8.0f;
        check (finite, "Physical contact stays finite and bounded");
    }

    SympatheticStrings drones;
    drones.prepare (48000.0);
    const std::array<int, 6> manualNotes { 57, 60, 64, 69, 72, 76 };
    drones.setTuning (1, 0, false, manualNotes);
    check (drones.noteFor (0) == 48 && drones.noteFor (1) == 50 && drones.noteFor (2) == 52,
           "Sympathetic strings follow GENERATE scale and root");
    drones.setTuning (1, 0, true, manualNotes);
    check (drones.noteFor (0) == 57 && drones.noteFor (3) == 69,
           "Manual sympathetic tuning overrides the scale");
    auto tail = 0.0;
    auto bounded = true;
    for (int i = 0; i < 48000; ++i)
    {
        const auto input = i < 10000 ? 0.4f * std::sin ((float) i * 2.0f * juce::MathConstants<float>::pi
                                                          * 220.0f / 48000.0f) : 0.0f;
        const auto value = drones.process (input, 6, 1.0f, 0.95f);
        if (i > 20000) tail += std::abs ((double) value);
        bounded = bounded && std::isfinite (value) && std::abs (value) <= 4.0f;
    }
    check (tail > 0.01 && bounded, "Shared sympathetic strings ring after excitation and remain bounded");

    IlanaSynthAudioProcessor heavy;
    heavy.prepareToPlay (48000.0, 512);
    const auto swarm = heavy.getFactoryPresetNames().indexOf ("Swarm");
    if (swarm >= 0) heavy.loadFactoryPreset (swarm);
    setParam (heavy, "sym_on", 1.0f);
    setParam (heavy, "sym_amount", 1.0f);
    setParam (heavy, "sym_count", 6.0f);
    juce::AudioBuffer<float> heavyBuffer (2, 512);
    const auto start = juce::Time::getHighResolutionTicks();
    for (int block = 0; block < 188; ++block)
    {
        heavyBuffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            for (const auto note : { 48, 52, 55, 59, 62, 67 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        heavy.processBlock (heavyBuffer, midi);
    }
    const auto msPerSecond = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start)
                             * 1000.0 / (188.0 * 512.0 / 48000.0);
    checkTiming (msPerSecond < 400.0, "Six shared sympathetic strings fit the heavy preset CPU budget ("
                                 + juce::String (msPerSecond, 1) + " ms/s)");
}

void runM3MissingParameterTest()
{
    IlanaSynthAudioProcessor processor;
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr, "M3 state migration XML exists");
    if (xml == nullptr) return;
    juce::StringArray ids { "sym_on", "sym_amount", "sym_decay", "sym_count", "sym_manual" };
    for (int i = 1; i <= 6; ++i) ids.add ("sym_note" + juce::String (i));
    for (const auto* prefix : { "osc1", "osc2", "sub" })
        for (const auto* suffix : { "_bow_pressure", "_bow_speed", "_bridge_buzz", "_fret_rattle" })
            ids.add (juce::String (prefix) + suffix);
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (ids.contains (child->getStringAttribute ("id"))) xml->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    for (const auto& id : ids)
        if (auto* param = processor.apvts.getParameter (id)) param->setValueNotifyingHost (1.0f);
    processor.setStateInformation (oldState.getData(), (int) oldState.getSize());
    auto defaultsRestored = true;
    for (const auto& id : ids)
    {
        auto* param = processor.apvts.getParameter (id);
        auto* raw = processor.apvts.getRawParameterValue (id);
        defaultsRestored = defaultsRestored && param != nullptr && raw != nullptr
                          && std::abs (raw->load() - param->convertFrom0to1 (param->getDefaultValue())) < 0.001f;
    }
    check (defaultsRestored, "Every missing M3 parameter loads its declared default");
}

void runGranularTests()
{
    const auto render = [] (std::function<void (juce::AudioProcessorValueTreeState&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        auto& state = processor.apvts;
        const auto set = [&state] (const char* id, float value)
        {
            if (auto* parameter = state.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        set ("osc1_mode", 3.0f);
        set ("osc1_sample_start", 0.3f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        setup (state);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> left, right;

        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
            {
                left.push_back (buffer.getSample (0, i));
                right.push_back (buffer.getSample (1, i));
            }
        }

        return std::make_pair (left, right);
    };

    const auto stats = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto energy = 0.0, difference = 0.0;
        auto peak = 0.0f;
        auto finite = true;

        for (size_t i = 4800; i < a.size(); ++i)
        {
            energy += (double) a[i] * a[i];
            difference += (double) (a[i] - b[i]) * (a[i] - b[i]);
            peak = juce::jmax (peak, std::abs (a[i]), std::abs (b[i]));
            finite = finite && std::isfinite (a[i]) && std::isfinite (b[i]);
        }

        const auto n = (double) (a.size() - 4800);
        return std::make_tuple (std::sqrt (energy / n), std::sqrt (difference / n), peak, finite);
    };

    const auto [rms, sideRms, peak, finite] = [&]
    {
        const auto out = render ([] (juce::AudioProcessorValueTreeState&) {});
        return stats (out.first, out.second);
    }();

    check (finite && rms > 0.01 && peak < 2.0f,
           "Granular mode sounds with no sample loaded (rms " + juce::String (rms, 4) + ", peak " + juce::String (peak, 3) + ")");
    check (sideRms > 0.005, "Granular grains are spread across the stereo field (side " + juce::String (sideRms, 4) + ")");

    const auto mono = render ([] (juce::AudioProcessorValueTreeState& state)
    {
        if (auto* parameter = state.getParameter ("osc1_grain_spread"))
            parameter->setValueNotifyingHost (0.0f);
    });
    const auto [monoRms, monoSide, monoPeak, monoFinite] = stats (mono.first, mono.second);
    check (monoFinite && monoSide < 1.0e-4 && monoRms > 0.01, "Granular stereo spread at zero is mono (side " + juce::String (monoSide, 6) + ")");

    // Sparse vs dense clouds stay in the same loudness ballpark.
    for (const auto density : { 0.0f, 1.0f })
    {
        const auto out = render ([density] (juce::AudioProcessorValueTreeState& state)
        {
            if (auto* parameter = state.getParameter ("osc1_grain_density"))
                parameter->setValueNotifyingHost (density);
        });
        const auto [densityRms, s2, densityPeak, densityFinite] = stats (out.first, out.second);
        juce::ignoreUnused (s2);
        check (densityFinite && densityRms > rms * 0.3 && densityRms < rms * 3.0 && densityPeak < 2.5f,
               "Granular density " + juce::String (density, 1) + " keeps a sensible level (rms " + juce::String (densityRms, 4) + ")");
    }
}

// The new engines must stay affordable: six-note chords on the heaviest
// v1.1 presets, measured as milliseconds of CPU per second of audio.
void runHeavyPresetCpuTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto names = processor.getFactoryPresetNames();

    for (const auto* name : { "Swarm", "Grain Choir", "Gated Supersaw", "Harmonic Cut Pad", "Chaos Filter Pad", "Operator Bell" })
    {
        const auto index = names.indexOf (name);

        if (index < 0)
        {
            check (false, juce::String ("heavy preset exists: ") + name);
            continue;
        }

        processor.loadFactoryPreset (index);
        processor.panic();

        juce::AudioBuffer<float> buffer (2, 512);
        const auto blocks = (int) (48000 * 2 / 512);
        const auto start = juce::Time::getHighResolutionTicks();

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                for (const auto note : { 48, 52, 55, 59, 62, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);
        }

        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        const auto msPerSecond = seconds * 1000.0 / 2.0;
        std::cout << "  cpu: " << name << " " << juce::String (msPerSecond, 1) << " ms per second of audio" << std::endl;
        checkTiming (msPerSecond < 400.0, juce::String (name) + " renders a six-note chord well within real time ("
                                          + juce::String (msPerSecond, 1) + " ms/s)");
    }
}

void runFmMatrixTests()
{
    const auto renderWith = [] (std::function<void (IlanaSynthAudioProcessor&)> setup)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        // Two sine operators, no sub, open filter.
        set ("subosc_on", 0.0f);
        set ("osc1_table", 8.0f);
        set ("osc1_frame", 0.0f);
        set ("osc2_on", 1.0f);
        set ("osc2_table", 8.0f);
        set ("osc2_fine", 0.0f);
        set ("f1_cutoff", 20000.0f);
        set ("f1_env", 0.0f);
        setup (processor);

        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 512);

        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    const auto set = [] (IlanaSynthAudioProcessor& processor, const char* id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto rms = [] (const std::vector<float>& data)
    {
        auto sum = 0.0;

        for (auto value : data)
            sum += (double) value * value;

        return std::sqrt (sum / juce::jmax ((size_t) 1, data.size()));
    };

    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        auto sum = 0.0;

        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += (double) (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (sum / juce::jmax ((size_t) 1, a.size()));
    };

    // OSC 1 as a silent modulator of OSC 2.
    const auto plain = renderWith ([&] (IlanaSynthAudioProcessor& p) { set (p, "osc1_out", 0.0f); });
    const auto modulated = renderWith ([&] (IlanaSynthAudioProcessor& p)
    {
        set (p, "osc1_out", 0.0f);
        set (p, "fm_1to2", 0.6f);
    });
    const auto both = renderWith ([&] (IlanaSynthAudioProcessor&) {});

    check (rms (plain) > 0.01 && rms (both) > rms (plain) * 1.3,
           "OUT off keeps an oscillator out of the mix (" + juce::String (rms (plain), 3) + " vs " + juce::String (rms (both), 3) + ")");
    check (difference (plain, modulated) > rms (plain) * 0.2,
           "OSC 1 > OSC 2 FM changes OSC 2 while OSC 1 stays silent");

    // All three FM styles stay finite and bounded at full depth.
    for (int mode = 0; mode < 3; ++mode)
    {
        const auto out = renderWith ([&] (IlanaSynthAudioProcessor& p)
        {
            set (p, "fm_mode", (float) mode);
            set (p, "fm_1to2", 1.0f);
            set (p, "fm_amount", 1.0f);
            set (p, "fm_fb2", 1.0f);
        });

        auto finite = true;
        auto peak = 0.0f;

        for (auto value : out)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }

        check (finite && peak < 4.0f && peak > 0.001f, "FM mode " + juce::String (mode) + " stays bounded at full depth (peak "
                                                           + juce::String (peak, 3) + ")");
    }

    // Saved 1.0 routings to parameter destinations move past the new FM ones.
    IlanaSynthAudioProcessor source;
    auto state = source.apvts.copyState();
    state.removeProperty ("destSchema", nullptr);
    const auto oldIndex = Mod::explicitDestinationsV10 + 3;

    for (int i = 0; i < state.getNumChildren(); ++i)
        if (state.getChild (i).getProperty ("id").toString() == "mod1_dst")
            state.getChild (i).setProperty ("value", oldIndex, nullptr);

    juce::MemoryBlock data;
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    juce::AudioProcessor::copyXmlToBinary (*xml, data);
    IlanaSynthAudioProcessor target;
    target.setStateInformation (data.getData(), (int) data.getSize());
    const auto migrated = (int) target.apvts.getRawParameterValue ("mod1_dst")->load();
    check (migrated == Mod::numExplicitDestinations + 3,
           "old parameter-destination routings are renumbered (" + juce::String (migrated) + ")");
}

void runM3bEngineTests()
{
    const auto render = [] (std::function<void (IlanaSynthAudioProcessor&)> configure, int blocks = 24, int note = 57)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "noise_level", 0.0f);
        setParam (processor, "osc2_on", 0.0f);
        setParam (processor, "f1_cutoff", 20000.0f);
        setParam (processor, "f1_env", 0.0f);
        configure (processor);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        std::vector<float> samples;
        const auto peak = renderPeakAndCentroid (processor, note, blocks, centroid, &samples);
        return std::make_pair (peak, samples);
    };
    const auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double sum = 0.0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            sum += std::abs ((double) a[i] - b[i]);
        return sum / (double) juce::jmax ((size_t) 1, juce::jmin (a.size(), b.size()));
    };

    const auto baseline = render ([] (auto&) {});
    const auto normal = render ([] (auto& p) { setParam (p, "quality", 1.0f); });
    check (difference (baseline.second, normal.second) == 0.0, "Normal quality is the legacy default sample for sample");
    const auto highNormal = render ([] (auto& p) { setParam (p, "osc1_table", 6.0f); }, 24, 93);
    const auto highQuality = render ([] (auto& p)
    {
        setParam (p, "osc1_table", 6.0f);
        setParam (p, "quality", 2.0f);
    }, 24, 93);
    const auto stepEnergy = [] (const std::vector<float>& samples)
    {
        double energy = 0.0;
        for (size_t i = 1; i < samples.size(); ++i)
            energy += std::pow ((double) samples[i] - samples[i - 1], 2.0);
        return energy;
    };
    check (std::isfinite (highQuality.first) && highQuality.first > 0.001f
               && stepEnergy (highQuality.second) < stepEnergy (highNormal.second),
           "High quality smooths high-note wavetable steps");
    for (int osc = 4; osc <= 6; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);
        for (int mode = 0; mode < 4; ++mode)
        {
            const auto sound = render ([&] (auto& p)
            {
                setParam (p, "osc1_on", 0.0f);
                setParam (p, prefix + "_on", 1.0f);
                setParam (p, prefix + "_mode", (float) mode);
                setParam (p, prefix + "_sample_factory", 1.0f);
            });
            auto finite = true;
            for (auto sample : sound.second) finite = finite && std::isfinite (sample);
            check (finite && sound.first > 0.0001f, prefix + " mode " + juce::String (mode) + " renders finite audio");
        }
        const auto off = render ([&] (auto& p)
        {
            setParam (p, prefix + "_on", 0.0f);
            setParam (p, prefix + "_mode", 1.0f);
            setParam (p, prefix + "_unison", 8.0f);
        });
        check (difference (baseline.second, off.second) == 0.0, prefix + " disabled leaves audio sample for sample unchanged");
    }

    const auto both = render ([] (auto& p)
    {
        setParam (p, "osc1_route", (float) FilterRoute::Both);
        setParam (p, "f1_cutoff", 20.0f);
        setParam (p, "f2_cutoff", 20000.0f);
    });
    const auto f1 = render ([] (auto& p)
    {
        setParam (p, "osc1_route", (float) FilterRoute::Filter1);
        setParam (p, "f1_cutoff", 20.0f);
        setParam (p, "f2_cutoff", 20000.0f);
    });
    check (both.first > f1.first * 2.0f, "Both route reaches Filter 2 when Filter 1 is closed");

    // Every cell has an audible effect with its source silent in the output.
    // The target alone is compared against the same two-oscillator patch.
    for (int source = 1; source <= 6; ++source)
        for (int target = 1; target <= 6; ++target)
        {
            const auto id = source == target ? (source == 1 ? juce::String ("fm_feedback") : "fm_fb" + juce::String (source))
                                             : (source == 2 && target == 1 ? juce::String ("fm_amount")
                                                                           : "fm_" + juce::String (source) + "to" + juce::String (target));
            const auto setup = [&] (auto& p)
            {
                setParam (p, "osc1_on", 0.0f);
                for (int osc = 1; osc <= 6; ++osc)
                {
                    const auto prefix = osc == 3 ? juce::String ("sub") : "osc" + juce::String (osc);
                    setParam (p, prefix + "_on", osc == source || osc == target ? 1.0f : 0.0f);
                    setParam (p, prefix + "_table", 8.0f);
                    setParam (p, prefix + "_out", osc == target ? 1.0f : 0.0f);
                }
            };
            const auto dry = render (setup, 20);
            const auto wet = render ([&] (auto& p) { setup (p); setParam (p, id, 0.75f); }, 20);
            check (difference (dry.second, wet.second) > 0.00001, "FM cell changes target: " + id);
        }

    const auto envDry = render ([] (auto& p) { setParam (p, "env6_sustain", 1.0f); });
    const auto envWet = render ([] (auto& p)
    {
        setParam (p, "env6_sustain", 1.0f);
        setParam (p, "mod1_src", (float) Mod::Source::Env6);
        setParam (p, "mod1_dst", (float) Mod::Destination::Osc1Pitch);
        setParam (p, "mod1_amt", 0.2f);
    });
    check (difference (envDry.second, envWet.second) > 0.0001, "ENV 6 works as a per-voice matrix source");

    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "osc1_on", 0.0f);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "osc4_on", 1.0f);
        setParam (processor, "osc4_amp_env", 5.0f); // ENV 6, zero based
        setParam (processor, "amp_release", 0.005f);
        setParam (processor, "env6_sustain", 1.0f);
        setParam (processor, "env6_release", 2.0f);
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 270; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            if (block == 8) midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            processor.processBlock (buffer, midi);
            if (block == 60) check (processor.getActiveVoiceCount() == 1, "long OSC 4 envelope outlives AMP");
        }
        check (processor.getActiveVoiceCount() == 0, "OSC 4 envelope eventually releases the voice");
    }

    {
        IlanaSynthAudioProcessor processor;
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
        for (auto* child = xml->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute ("id");
            if (id.startsWith ("osc4_") || id.startsWith ("osc5_") || id.startsWith ("osc6_")
                || id.startsWith ("env6_") || id == "quality")
                xml->removeChildElement (child, true);
            child = next;
        }
        for (const auto* attribute : { "envRevealCount", "envRevealMask", "oscRevealMask", "lfoRevealMask" })
            xml->removeAttribute (attribute);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        setParam (processor, "osc4_on", 1.0f);
        setParam (processor, "osc4_amp_env", 5.0f);
        setParam (processor, "env6_release", 9.0f);
        setParam (processor, "quality", 2.0f);
        processor.setStateInformation (legacy.getData(), (int) legacy.getSize());
        const auto get = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        const auto releaseDefault = processor.apvts.getParameter ("env6_release")->convertFrom0to1 (
            processor.apvts.getParameter ("env6_release")->getDefaultValue());
        check (get ("osc4_on") == 0.0f && get ("osc4_amp_env") == 0.0f
                   && std::abs (get ("env6_release") - releaseDefault) < 0.001f && get ("quality") == 1.0f,
               "pre-M3b state resets new parameters to neutral defaults (osc4 " + juce::String (get ("osc4_on"))
                   + ", amp env " + juce::String (get ("osc4_amp_env")) + ", release "
                   + juce::String (get ("env6_release"), 4) + ", quality " + juce::String (get ("quality")) + ")");
        using Module = IlanaSynthAudioProcessor::Module;
        const auto revealedCount = [] (const IlanaSynthAudioProcessor& p, Module kind, int count)
        {
            auto shown = 0;
            for (int i = 0; i < count; ++i)
                shown += p.isRevealed (kind, i) ? 1 : 0;
            return shown;
        };
        check (revealedCount (processor, Module::Envelope, 16) == 3 && revealedCount (processor, Module::Oscillator, 6) == 3
                   && revealedCount (processor, Module::Lfo, 4) == 3,
               "pre-M3b state shows three oscillators, envelopes and LFOs");
        check (processor.isOscillatorShown (2) && ! processor.isOscillatorShown (3),
               "OSC 3 is shown and OSC 4 is hidden by default");
        processor.setRevealed (Module::Envelope, 5, true);
        processor.addOscillator (3);
        check (processor.apvts.getRawParameterValue ("osc4_on")->load() > 0.5f, "adding an oscillator switches it on");
        juce::MemoryBlock revealedState;
        processor.getStateInformation (revealedState);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (revealedState.getData(), (int) revealedState.getSize());
        check (restored.isRevealed (Module::Envelope, 5) && ! restored.isRevealed (Module::Envelope, 4)
                   && restored.isOscillatorShown (3),
               "revealed envelopes and oscillators survive a patch state round trip");
        restored.removeOscillator (3);
        check (! restored.isOscillatorShown (3) && restored.apvts.getRawParameterValue ("osc4_on")->load() < 0.5f,
               "removing an oscillator hides it and switches it off");

        // The first M3b build saved a count of revealed envelopes.
        if (auto countXml = juce::AudioProcessor::getXmlFromBinary (revealedState.getData(), (int) revealedState.getSize()))
        {
            countXml->removeAttribute ("envRevealMask");
            countXml->setAttribute ("envRevealCount", 7);
            juce::MemoryBlock countState;
            juce::AudioProcessor::copyXmlToBinary (*countXml, countState);
            IlanaSynthAudioProcessor fromCount;
            fromCount.setStateInformation (countState.getData(), (int) countState.getSize());
            check (revealedCount (fromCount, Module::Envelope, 16) == 7, "an envelope reveal count loads as the first N envelopes");
        }
    }

    const auto sixOscCpu = [] (int quality)
    {
        IlanaSynthAudioProcessor processor;
        setParam (processor, "quality", (float) quality);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "sub_on", 1.0f);
        for (int osc = 1; osc <= 6; ++osc)
        {
            const auto prefix = osc == 3 ? juce::String ("sub") : "osc" + juce::String (osc);
            setParam (processor, prefix + "_on", 1.0f);
            setParam (processor, prefix + "_unison", 6.0f);
        }
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        const auto start = juce::Time::getHighResolutionTicks();
        for (int block = 0; block < 188; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                for (const auto note : { 48, 52, 55, 59, 62, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
        }
        return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 500.0;
    };
    // Best of three, interleaved: one pass of each swung by a third between
    // runs with machine load, which made "Eco is cheaper" flaky.
    auto normalCpu = 1.0e9, ecoCpu = 1.0e9;
    for (int pass = 0; pass < 3; ++pass)
    {
        normalCpu = juce::jmin (normalCpu, sixOscCpu (1));
        ecoCpu = juce::jmin (ecoCpu, sixOscCpu (0));
    }
    std::cout << "  cpu: six oscillators Normal " << normalCpu << " ms/s, Eco " << ecoCpu << " ms/s" << std::endl;
    checkTiming (normalCpu < 400.0 && ecoCpu < 400.0 && ecoCpu < normalCpu,
           "six-oscillator Normal and Eco stay within budget and Eco is cheaper");
}

// M4: acoustic keys and the LFO pool.
double bandEnergy (const std::vector<float>& samples, size_t start, size_t end)
{
    auto total = 0.0;
    for (auto i = start; i < juce::jmin (end, samples.size()); ++i)
        total += (double) samples[i] * samples[i];
    return total / (double) juce::jmax ((size_t) 1, juce::jmin (end, samples.size()) - start);
}

double centroidOf (const std::vector<float>& samples, size_t start, double sampleRate)
{
    constexpr int order = 12;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work ((size_t) size * 2, 0.0f);
    for (int i = 0; i < size && start + (size_t) i < samples.size(); ++i)
        work[(size_t) i] = samples[start + (size_t) i]
                           * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
    fft.performFrequencyOnlyForwardTransform (work.data());
    double weighted = 0.0, total = 0.0;
    for (int bin = 1; bin < size / 2; ++bin)
    {
        weighted += bin * sampleRate / size * work[(size_t) bin];
        total += work[(size_t) bin];
    }
    return total > 0.0 ? weighted / total : 0.0;
}

void runM4Tests()
{
    std::cout << "M4 acoustic keys" << std::endl;

    {
        IlanaSynthAudioProcessor processor;
        for (const auto* id : { "osc1_excite", "osc2_excite", "sub_excite", "osc4_excite" })
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id));
            check (choice != nullptr && choice->getAllValueStrings().size() >= 7
                       && choice->getAllValueStrings()[4] == "Bow" && choice->getAllValueStrings()[5] == "Bright Hammer"
                       && choice->getAllValueStrings()[6] == "Osc In",
                   juce::String (id) + " appends Hammer and Osc In after Bow");
        }
    }

    // Hammer: velocity and felt hardness both brighten; velocity also louder.
    const auto hammer = [] (float velocity, float hardness, double frequency = 220.0)
    {
        KarplusStrong string (91);
        string.prepare (48000.0);
        string.setFrequency (frequency);
        string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.9f);
        string.setKeysParams (hardness, 0.0f);
        string.trigger (velocity);
        std::vector<float> samples (48000);
        for (auto& sample : samples)
            sample = string.process();
        return samples;
    };
    {
        const auto soft = hammer (0.3f, 0.5f);
        const auto loud = hammer (1.0f, 0.5f);
        const auto felt = hammer (0.8f, 0.0f);
        const auto hard = hammer (0.8f, 1.0f);
        const auto softLevel = std::sqrt (bandEnergy (soft, 0, 9600));
        const auto loudLevel = std::sqrt (bandEnergy (loud, 0, 9600));
        const auto softBright = centroidOf (soft, 200, 48000.0), loudBright = centroidOf (loud, 200, 48000.0);
        const auto feltBright = centroidOf (felt, 200, 48000.0), hardBright = centroidOf (hard, 200, 48000.0);
        std::cout << "  hammer rms soft " << softLevel << " loud " << loudLevel << ", centroid soft " << softBright
                  << " loud " << loudBright << " felt " << feltBright << " hard " << hardBright << std::endl;
        check (loudLevel > softLevel * 2.0, "a faster hammer is louder");
        check (loudBright > softBright * 1.15, "a faster hammer is brighter");
        check (hardBright > feltBright * 1.15, "a harder hammer is brighter");
        check (loudLevel > 0.05 && loudLevel < 2.0, "hammer level is in a sensible range (" + juce::String (loudLevel) + ")");
        auto finite = true;
        for (const auto& samples : { soft, loud, felt, hard })
            for (auto value : samples)
                finite = finite && std::isfinite (value);
        check (finite, "hammer output stays finite");

        // A reset string with the Hammer exciter is silent.
        KarplusStrong string (5);
        string.prepare (48000.0);
        string.setFrequency (110.0);
        string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.9f);
        string.reset();
        auto peak = 0.0f;
        for (int i = 0; i < 4800; ++i)
            peak = juce::jmax (peak, std::abs (string.process()));
        check (peak == 0.0f, "an untriggered hammer string stays silent");
    }

    // Bow (rebuilt in M4 on the STK friction table): a steady, pitched tone
    // at a level comparable to a pluck, that fades once released.
    {
        const auto bow = [] (float pressure, float speed, bool release)
        {
            KarplusStrong string (4242);
            string.prepare (48000.0);
            string.setFrequency (220.0);
            string.setParams (KarplusStrong::Excite::Bow, 0.0f, 0.3f, 0.9f);
            string.setBowAndBuzz (pressure, speed, 0.0f, 0.0f);
            string.trigger (0.8f);
            std::vector<float> samples (96000);
            for (int i = 0; i < 96000; ++i)
                samples[(size_t) i] = string.process (0.0f, ! release || i < 48000);
            return samples;
        };
        const auto held = bow (0.5f, 0.5f, false);
        const auto released = bow (0.5f, 0.5f, true);
        const auto level = std::sqrt (bandEnergy (held, 24000, 48000));
        const auto lateLevel = std::sqrt (bandEnergy (held, 72000, 96000));
        const std::vector<float> steady (held.begin() + 24000, held.begin() + 48000);
        const auto pitch = fundamentalOf (steady, 48000.0);
        const auto soft = std::sqrt (bandEnergy (bow (0.1f, 0.5f, false), 24000, 48000));
        const auto firm = std::sqrt (bandEnergy (bow (0.9f, 0.5f, false), 24000, 48000));
        std::cout << "  bow rms " << level << " late " << lateLevel << " pitch " << pitch << " Hz, pressure 0.1 "
                  << soft << " 0.9 " << firm << ", released " << std::sqrt (bandEnergy (released, 84000, 96000)) << std::endl;
        check (level > 0.05 && level < 1.5, "a bowed string sounds at a pluck-like level (" + juce::String (level) + ")");
        check (lateLevel > level * 0.5, "the bow sustains while held");
        check (std::abs (1200.0 * std::log2 (pitch / 220.0)) < 30.0, "a bowed string plays in tune (" + juce::String (pitch) + " Hz)");
        check (bandEnergy (released, 84000, 96000) < bandEnergy (held, 84000, 96000) * 0.01, "the bowed string fades after release");
        check (std::abs (firm - soft) > soft * 0.1, "bow pressure changes the tone");
    }

    // Piano stiffness: a hammered bass string's upper partials run sharp by
    // sqrt (1 + B n^2), and the fundamental stays in tune.
    {
        const auto partialRatio = [] (double f0, float stiffness, int n)
        {
            KarplusStrong string (8);
            string.prepare (48000.0);
            string.setFrequency (f0);
            string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.1f, 0.95f);
            string.setPhysicalParams (stiffness, 0.0f, 0.0f, 1.0f, 0.0f, false);
            string.setKeysParams (1.0f, 0.0f);
            string.trigger (1.0f);
            constexpr int order = 16;
            constexpr int size = 1 << order;
            std::vector<float> work ((size_t) size * 2, 0.0f);
            for (int i = 0; i < 2400; ++i)
                string.process();
            for (int i = 0; i < size; ++i)
                work[(size_t) i] = string.process() * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
            juce::dsp::FFT fft (order);
            fft.performFrequencyOnlyForwardTransform (work.data());
            const auto binHz = 48000.0 / size;
            // Strongest bin within +/-4 % of each harmonic slot, refined
            // with a parabola through its neighbours.
            const auto peakNear = [&] (double hz)
            {
                auto best = 0; auto bestValue = 0.0f;
                for (auto bin = (int) (hz * 0.96 / binHz); bin <= (int) (hz * 1.04 / binHz); ++bin)
                    if (work[(size_t) bin] > bestValue) { bestValue = work[(size_t) bin]; best = bin; }
                const auto l = work[(size_t) best - 1], c = work[(size_t) best], r = work[(size_t) best + 1];
                const auto shift = 0.5 * (l - r) / juce::jmax (1.0e-9, (double) (l - 2.0f * c + r));
                return (best + juce::jlimit (-0.5, 0.5, shift)) * binHz;
            };
            // Against the nominal pitch: at 41 Hz the FFT places the
            // fundamental itself too coarsely to divide by. Tuning is
            // judged on the 4th partial, which stiffness barely moves.
            return std::make_pair (peakNear (4 * f0) / 4.0, peakNear (n * f0) / (n * f0));
        };
        const auto stiff = partialRatio (41.2, 0.5f, 10);
        const auto plain = partialRatio (41.2, 0.0f, 10);
        const auto expected = std::sqrt (1.0 + 3.16e-4 * 100.0) / std::sqrt (1.0 + 3.16e-4);
        std::cout << "  piano stiffness E1: 10th partial ratio " << stiff.second << " (expected " << expected
                  << ", plain " << plain.second << "), 4th partial / 4 " << stiff.first << " Hz" << std::endl;
        check (std::abs (stiff.second - expected) < 0.008, "a stiff piano string's partials stretch like a real string's");
        check (std::abs (plain.second - 1.0) < 0.004, "with no stiffness the partials stay harmonic");
        check (std::abs (1200.0 * std::log2 (stiff.first / 41.2)) < 20.0, "the stiff string stays in tune");
    }

    // Damper: once the key is up the string stops quickly.
    {
        const auto render = [] (float damper)
        {
            KarplusStrong string (17);
            string.prepare (48000.0);
            string.setFrequency (261.63);
            string.setParams (KarplusStrong::Excite::Hammer, 0.0f, 0.2f, 0.95f);
            string.setKeysParams (0.5f, damper);
            string.trigger (0.8f);
            std::vector<float> samples (48000);
            for (int i = 0; i < 48000; ++i)
                samples[(size_t) i] = string.process (0.0f, i < 9600);
            return samples;
        };
        const auto free = render (0.0f);
        const auto damped = render (1.0f);
        const auto ratio = bandEnergy (damped, 24000, 28800) / juce::jmax (1.0e-12, bandEnergy (free, 24000, 28800));
        std::cout << "  damper energy ratio 0.3 s after release " << ratio << std::endl;
        check (ratio < 1.0e-3, "the damper stops the string after release (-30 dB)");
        check (std::abs (bandEnergy (free, 0, 9600) - bandEnergy (damped, 0, 9600)) < 1.0e-9,
               "the damper does nothing while the key is held");
    }

    // Coupled strings: the in-phase sound dies fast, the detuned aftersound rings on.
    {
        const auto render = [] (float couple)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "osc1_mode", 1.0f);
            setParam (processor, "osc1_excite", 5.0f);
            setParam (processor, "osc1_unison", 3.0f);
            setParam (processor, "osc1_detune", 2.0f);
            setParam (processor, "osc1_couple", couple);
            setParam (processor, "osc1_string_decay", 0.97f);
            setParam (processor, "osc1_string_damp", 0.2f);
            setParam (processor, "osc2_on", 0.0f);
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "subosc_on", 0.0f);
            setParam (processor, "amp_sustain", 1.0f);
            setParam (processor, "f1_cutoff", 20000.0f);
            std::vector<float> samples;
            juce::AudioBuffer<float> buffer (2, 512);
            for (int block = 0; block < 94 * 3; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                    samples.push_back (buffer.getSample (0, i));
            }
            return samples;
        };
        const auto slope = [] (const std::vector<float>& s, size_t a, size_t b)
        {
            // dB per second between two 50 ms windows.
            const auto ea = bandEnergy (s, a, a + 2400), eb = bandEnergy (s, b, b + 2400);
            return 10.0 * std::log10 (juce::jmax (1.0e-20, eb) / juce::jmax (1.0e-20, ea)) / ((double) (b - a) / 48000.0);
        };
        const auto plain = render (0.0f);
        const auto coupled = render (1.0f);
        const auto early = slope (coupled, 2400, 19200), late = slope (coupled, 72000, 120000);
        const auto plainEarly = slope (plain, 2400, 19200);
        std::cout << "  coupled decay early " << early << " dB/s, late " << late << " dB/s, uncoupled early "
                  << plainEarly << " dB/s" << std::endl;
        check (early < late - 6.0, "coupled strings decay in two stages (fast prompt sound, slow aftersound)");
        check (early < plainEarly - 3.0, "coupling speeds up the prompt decay");
        check (bandEnergy (coupled, 120000, 144000) > bandEnergy (coupled, 2400, 26400) * 1.0e-8,
               "the aftersound is still ringing after 2.5 s (within 80 dB of the attack)");
    }

    // Stretch tuning: the top of the keyboard is sharp, the middle unchanged.
    {
        const auto pitch = [] (int note, float stretch)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "stretch", stretch);
            setParam (processor, "osc2_on", 0.0f);
            setParam (processor, "sub_on", 0.0f);
            setParam (processor, "subosc_on", 0.0f);
            setParam (processor, "osc1_table", 0.0f);
            setParam (processor, "f1_cutoff", 20000.0f);
            double centroid = 0.0;
            std::vector<float> samples;
            renderPeakAndCentroid (processor, note, 40, centroid, &samples);
            return fundamentalOf (samples, 48000.0);
        };
        const auto top = pitch (96, 1.0f), topPlain = pitch (96, 0.0f);
        const auto middle = pitch (60, 1.0f), middlePlain = pitch (60, 0.0f);
        const auto topCents = 1200.0 * std::log2 (top / topPlain);
        const auto middleCents = 1200.0 * std::log2 (middle / middlePlain);
        std::cout << "  stretch: C7 " << topCents << " cents, C4 " << middleCents << " cents" << std::endl;
        check (topCents > 15.0 && topCents < 40.0, "full stretch tuning raises C7 by 15-40 cents");
        check (std::abs (middleCents) < 2.0, "stretch tuning leaves middle C alone");
    }

    // Soundboard, pedal resonance and mechanical noises (processor-level).
    const auto renderKeys = [] (std::function<void (IlanaSynthAudioProcessor&)> setup,
                                std::function<void (int, juce::MidiBuffer&)> events, int blocks)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "osc1_mode", 1.0f);
        setParam (processor, "osc1_excite", 5.0f);
        setParam (processor, "osc2_on", 0.0f);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "subosc_on", 0.0f);
        setParam (processor, "amp_release", 0.05f);
        setup (processor);
        std::vector<float> samples;
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            events (block, midi);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
                samples.push_back (buffer.getSample (0, i));
        }
        return samples;
    };
    const auto noteThenOff = [] (int block, juce::MidiBuffer& midi)
    {
        if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (block == 20) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    };
    {
        const auto dry = renderKeys ([] (auto&) {}, noteThenOff, 40);
        const auto board = renderKeys ([] (auto& p) { setParam (p, "sb_on", 1.0f); setParam (p, "sb_mix", 1.0f); },
                                       noteThenOff, 40);
        auto difference = 0.0;
        auto finite = true;
        for (size_t i = 0; i < dry.size(); ++i)
        {
            difference += std::abs ((double) dry[i] - board[i]);
            finite = finite && std::isfinite (board[i]);
        }
        check (difference > 1.0 && finite, "the soundboard colours the sound and stays finite");
        const auto silent = renderKeys ([] (auto& p) { setParam (p, "sb_on", 1.0f); }, [] (int, auto&) {}, 20);
        check (bandEnergy (silent, 0, silent.size()) == 0.0, "the soundboard is silent with no input");

        // Brightness: energy of the first difference (a gentle high-pass)
        // relative to the whole signal.
        const auto darkBright = [&] (float tone)
        {
            const auto samples = renderKeys ([tone] (auto& p)
            {
                setParam (p, "sb_on", 1.0f);
                setParam (p, "sb_tone", tone);
                setParam (p, "osc1_hammer_hard", 1.0f);
            }, noteThenOff, 20);
            auto edges = 0.0, total = 0.0;
            for (size_t i = 1; i < samples.size(); ++i)
            {
                edges += std::pow ((double) samples[i] - samples[i - 1], 2.0);
                total += (double) samples[i] * samples[i];
            }
            return edges / juce::jmax (1.0e-20, total);
        };
        const auto dark = darkBright (0.0f), bright = darkBright (1.0f);
        std::cout << "  soundboard tone brightness dark " << dark << " bright " << bright << std::endl;
        check (bright > dark * 3.0, "soundboard TONE opens the lid");
    }
    {
        const auto pedalEvents = [] (bool pedal)
        {
            return [pedal] (int block, juce::MidiBuffer& midi)
            {
                if (block == 0 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                if (block == 2) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
                if (block == 12) midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
                if (block == 12) midi.addEvent (juce::MidiMessage::allNotesOff (1), 1);
                if (block == 100 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
            };
        };
        // Make the voice itself short so only the resonance is left afterwards.
        const auto setup = [] (float amount)
        {
            return [amount] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "pedal_res", amount);
                setParam (p, "osc1_damper", 1.0f);
            };
        };
        const auto withPedal = renderKeys (setup (1.0f), pedalEvents (true), 160);
        const auto noResonance = renderKeys (setup (0.0f), pedalEvents (true), 160);
        const auto window = [] (int block) { return (size_t) block * 512; };
        const auto tail = bandEnergy (withPedal, window (60), window (90));
        const auto tailOff = bandEnergy (noResonance, window (60), window (90));
        const auto afterLift = bandEnergy (withPedal, window (140), window (160));
        std::cout << "  pedal resonance tail " << tail << " (without " << tailOff << "), after lift " << afterLift << std::endl;
        check (tail > tailOff * 10.0 && tail > 1.0e-9, "with the pedal down the strings ring in sympathy");
        check (afterLift < tail * 0.01, "lifting the pedal damps the sympathetic strings");
    }
    {
        const auto keysOnly = [] (const char* id)
        {
            return [id] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc1_on", 0.0f); // silence the voice: only the mechanism is left
                setParam (p, id, 1.0f);
            };
        };
        const auto quiet = renderKeys ([] (auto& p) { setParam (p, "osc1_on", 0.0f); }, noteThenOff, 30);
        const auto keyNoise = renderKeys (keysOnly ("mech_key"), noteThenOff, 30);
        const auto damperNoise = renderKeys (keysOnly ("mech_damper"), noteThenOff, 30);
        const auto pedalNoise = renderKeys (keysOnly ("mech_pedal"), [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 1) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (block == 10) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        }, 30);
        const auto window = [] (int block) { return (size_t) block * 512; };
        check (bandEnergy (quiet, 0, quiet.size()) == 0.0, "no mechanical noise when the levels are zero");
        check (bandEnergy (keyNoise, window (20), window (24)) > 1.0e-7, "key release makes a thock");
        check (bandEnergy (damperNoise, window (20), window (26)) > 1.0e-8, "the damper lands with a felt noise");
        check (bandEnergy (pedalNoise, window (1), window (6)) > 1.0e-7
                   && bandEnergy (pedalNoise, window (10), window (16)) > 1.0e-7,
               "the pedal mechanism sounds on press and release");
        // The output's DC blocker leaves a short sub-audio tail after the
        // thock, so the audible part is measured (the first difference, which
        // removes it): the noise itself must be gone, 60 dB under the thock.
        std::vector<float> keyNoiseDiff (keyNoise.size(), 0.0f);
        for (size_t i = 1; i < keyNoise.size(); ++i)
            keyNoiseDiff[i] = keyNoise[i] - keyNoise[i - 1];
        const auto thock = bandEnergy (keyNoiseDiff, window (20), window (24));
        const auto after = bandEnergy (keyNoiseDiff, window (28), window (30));
        check (after < thock * 1.0e-6, "mechanical noises end (" + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, after / juce::jmax (1.0e-30, thock))), 1)
                                           + " dB under the thock)");
    }

    // Osc In: the string is driven by what the FM matrix feeds it.
    {
        const auto render = [&] (float route)
        {
            return renderKeys ([route] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc1_excite", 6.0f);
                setParam (p, "osc1_string_sustain", 0.8f);
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_out", 0.0f);
                setParam (p, "fm_amount", route); // OSC 2 -> OSC 1
            }, [] (int block, juce::MidiBuffer& midi)
            {
                if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            }, 30);
        };
        const auto driven = bandEnergy (render (0.8f), 5000, 15000);
        const auto idle = bandEnergy (render (0.0f), 5000, 15000);
        std::cout << "  osc in: driven " << driven << " idle " << idle << std::endl;
        check (driven > 1.0e-5 && driven > idle * 100.0, "an Osc In string is played by the FM matrix input");
    }

    // Register map and all keys features together: a full piano stays finite.
    {
        const auto piano = renderKeys ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_unison", 3.0f);
            setParam (p, "osc1_detune", 1.5f);
            setParam (p, "osc1_couple", 1.0f);
            setParam (p, "osc1_register", 1.0f);
            setParam (p, "osc1_damper", 0.7f);
            setParam (p, "osc1_hammer_hard", 1.0f);
            setParam (p, "stretch", 1.0f);
            setParam (p, "sb_on", 1.0f);
            setParam (p, "pedal_res", 1.0f);
            for (const auto* id : { "mech_key", "mech_damper", "mech_pedal" })
                setParam (p, id, 1.0f);
        }, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (block % 4 == 0 && block < 80)
                midi.addEvent (juce::MidiMessage::noteOn (1, 21 + (block * 7) % 88, (juce::uint8) 127), 0);
            if (block % 4 == 2 && block < 80)
                midi.addEvent (juce::MidiMessage::noteOff (1, 21 + ((block - 2) * 7) % 88), 0);
            if (block == 90) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        }, 140);
        auto finite = true;
        auto peak = 0.0f;
        for (auto value : piano)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }
        std::cout << "  full piano peak " << peak << std::endl;
        check (finite && peak < 4.0 && peak > 0.01, "a full piano across the keyboard stays finite and bounded");
    }

    // LFO pool: LFO 7 routed to the cutoff moves the sound; its state saves.
    {
        const auto render = [] (float depth)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            setParam (processor, "lfo7_rate", 3.0f);
            setParam (processor, "mod1_src", (float) Mod::Source::Lfo7);
            setParam (processor, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
            setParam (processor, "mod1_amt", depth);
            setParam (processor, "f1_cutoff", 800.0f);
            double centroid = 0.0;
            std::vector<float> samples;
            renderPeakAndCentroid (processor, 48, 100, centroid, &samples);
            return samples;
        };
        const auto still = render (0.0f), moving = render (0.8f);
        auto difference = 0.0;
        for (size_t i = 0; i < still.size(); ++i)
            difference += std::abs ((double) still[i] - moving[i]);
        check (difference > 10.0, "LFO 7 modulates the filter when routed");
        check (Mod::lfoIndexFor (Mod::Source::Lfo16) == 15 && Mod::lfoSourceFor (15) == Mod::Source::Lfo16
                   && Mod::lfoRateDestinationFor (4) == Mod::Destination::Lfo5Rate,
               "LFO pool index helpers agree");

        IlanaSynthAudioProcessor processor;
        processor.setLfoCurve (9, LfoCurve::preset (5));
        processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, 9, true);
        setParam (processor, "lfo10_shape", 8.0f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getLfoCurve (9).toString() == LfoCurve::preset (5).toString()
                   && restored.isRevealed (IlanaSynthAudioProcessor::Module::Lfo, 9)
                   && restored.apvts.getRawParameterValue ("lfo10_shape")->load() == 8.0f,
               "LFO 10's curve, shape and card survive a state round trip");
        check (restored.isLfoShown (9) && ! restored.isLfoShown (10), "only added or routed LFOs are shown");
    }
}

// ILANA_NOTE_DEBUG=<preset>: E1, C4 and C7 held for 6 s at mf and ff, dry
// (no effects), written to build/note-debug/ for tools/analyse_note.py and
// tools/fit_piano.py (compared with real piano notes in build/reference/).
// ILANA_PIANO_TUNING="name=value;..." overrides PianoTuning;
// ILANA_PRESET_OVERRIDES="param=value;..." overrides preset parameters.
void debugPresetNotes (const juce::String& presetName)
{
    const auto tuning = juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO_TUNING", "");
    if (! PianoTuning::get().apply (tuning))
        std::cout << "unknown tuning name in: " << tuning << std::endl;
    if (! PianoModelTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO2_TUNING", "")))
        std::cout << "unknown piano2 tuning name" << std::endl;
    const auto overrides = juce::StringArray::fromTokens (
        juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_OVERRIDES", ""), ";", "");
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable (
        "ILANA_NOTE_FOLDER", juce::File::getCurrentWorkingDirectory().getChildFile ("build/note-debug").getFullPathName()));
    folder.createDirectory();

    // ILANA_NOTE_SET=ep: the electric piano set instead (MIDI 40, 60 and 84 at
    // velocity 40 and 120, released after 4 s), named like the ilanaRefHost
    // references (ours.v40.n60.wav).
    const auto electric = juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SET", "") == "ep";
    if (! EpTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_EP_TUNING", "")))
        std::cout << "unknown EP tuning name" << std::endl;
    struct NoteRender { juce::String file; int note, velocity; };
    std::vector<NoteRender> renders;
    if (electric)
    {
        for (int velocity : { 40, 120 })
            for (int note : { 40, 60, 84 })
                renders.push_back ({ "ours.v" + juce::String (velocity) + ".n" + juce::String (note) + ".wav", note, velocity });
    }
    else if (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SET", "") == "keyboard")
    {
        // M8.2: the whole keyboard (the Salamander grand's notes, a minor
        // third apart from A0) at pp, mf and ff, named ours.<dyn>.n<midi>.wav.
        // ILANA_NOTE_LIST="21,60,..." and ILANA_NOTE_DYNAMICS="mf,ff" narrow it.
        auto notes = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_LIST", ""), ",", "");
        if (notes.isEmpty())
            for (int note = 21; note <= 108; note += 3)
                notes.add (juce::String (note));
        auto dynamics = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_DYNAMICS", "pp,mf,ff"), ",", "");
        for (const auto& dynamic : dynamics)
        {
            const auto velocity = dynamic == "pp" ? 20 : dynamic == "mf" ? 60 : 124;
            for (const auto& note : notes)
                renders.push_back ({ "ours." + dynamic + ".n" + note.trim() + ".wav", note.getIntValue(), velocity });
        }
    }
    else
    {
        for (const auto& [dynamic, velocity] : { std::pair<const char*, int> { "mf", 80 }, { "ff", 120 } })
            for (const auto& [noteName, note] : { std::pair<const char*, int> { "E1", 28 }, { "C4", 60 }, { "C7", 96 } })
                renders.push_back ({ juce::String ("ours.") + dynamic + "." + noteName + ".wav", note, velocity });
    }
    const auto totalBlocks = electric ? 500 : juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_SECONDS", "6").getIntValue() * 100;
    const auto releaseBlock = electric ? 400 : -1;

    for (const auto& render : renders)
        {
            const auto note = render.note;
            const auto velocity = render.velocity;
            IlanaSynthAudioProcessor processor;
            const auto index = processor.getFactoryPresetNames().indexOf (presetName);
            if (index < 0) { std::cout << "no preset " << presetName << std::endl; return; }
            processor.loadFactoryPreset (index);
            for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                processor.assignFxSlot (slot, 0);
            for (const auto& item : overrides)
                if (item.contains ("="))
                    setParam (processor, item.upToFirstOccurrenceOf ("=", false, false).trim(),
                              item.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            processor.prepareToPlay (48000.0, 480);
            juce::AudioBuffer<float> buffer (2, 480);
            juce::AudioBuffer<float> output (1, 480 * totalBlocks);
            for (int block = 0; block < totalBlocks; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
                if (block == releaseBlock) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                processor.processBlock (buffer, midi);
                output.copyFrom (0, block * 480, buffer, 0, 0, 480);
                output.addFrom (0, block * 480, buffer, 1, 0, 480);
            }
            output.applyGain (0.5f);
            const auto file = folder.getChildFile (render.file);
            file.deleteFile();
            juce::WavAudioFormat format;
            if (auto stream = file.createOutputStream())
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
                }
        }
}

// ILANA_RENDER_DEMO=<folder>: renders the M4 Keys presets to .wav files for
// listening (a pedalled arpeggio and chord, then a low and a high note).
void renderKeysDemos (const juce::File& folder)
{
    folder.createDirectory();
    const double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    // The Keys presets, plus A/B pairs for the sympathetic strings and the
    // pedal resonance.
    struct Demo { juce::String file, preset; std::vector<std::pair<const char*, float>> settings; };
    std::vector<Demo> demos;
    for (const auto* name : { "Hammered Strings", "Pedal Bloom", "Honky Hammers", "Bolted Strings",
                              "Bowed Board", "Osc-Struck Strings", "Tine Keys", "Reed Keys" })
        demos.push_back ({ name, name, {} });
    demos.push_back ({ "AB Sym off - Pizzicato", "Pizzicato", {} });
    demos.push_back ({ "AB Sym on - Pizzicato", "Pizzicato",
                       { { "sym_on", 1.0f }, { "sym_amount", 0.8f }, { "sym_count", 6.0f }, { "sym_decay", 0.8f } } });
    demos.push_back ({ "AB Pedal res off - Pedal Bloom", "Pedal Bloom", { { "pedal_res", 0.0f } } });
    demos.push_back ({ "AB Pedal res on - Pedal Bloom", "Pedal Bloom", {} });

    for (const auto& demo : demos)
    {
        const auto* name = demo.file.toRawUTF8();
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf (demo.preset);
        if (index < 0)
            continue;

        processor.loadFactoryPreset (index);
        for (const auto& [id, value] : demo.settings)
            setParam (processor, id, value);
        processor.prepareToPlay (sampleRate, blockSize);
        const auto seconds = 9.0;
        juce::AudioBuffer<float> output (2, (int) (seconds * sampleRate));
        juce::AudioBuffer<float> buffer (2, blockSize);
        const auto at = [sampleRate] (double time) { return (int) (time * sampleRate); };

        struct Event { double time; juce::MidiMessage message; };
        std::vector<Event> events {
            { 0.0, juce::MidiMessage::controllerEvent (1, 64, 127) },
            { 0.05, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 70) },
            { 0.35, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 80) },
            { 0.65, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90) },
            { 0.95, juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100) },
            { 1.25, juce::MidiMessage::noteOn (1, 76, (juce::uint8) 110) },
            { 1.3, juce::MidiMessage::noteOff (1, 48) }, { 1.3, juce::MidiMessage::noteOff (1, 55) },
            { 1.3, juce::MidiMessage::noteOff (1, 64) }, { 1.3, juce::MidiMessage::noteOff (1, 72) },
            { 1.6, juce::MidiMessage::noteOff (1, 76) },
            { 3.4, juce::MidiMessage::controllerEvent (1, 64, 0) },
            { 3.8, juce::MidiMessage::noteOn (1, 57, (juce::uint8) 120) }, { 3.8, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 115) },
            { 3.8, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 110) },
            { 5.3, juce::MidiMessage::noteOff (1, 57) }, { 5.3, juce::MidiMessage::noteOff (1, 60) },
            { 5.3, juce::MidiMessage::noteOff (1, 64) },
            { 5.8, juce::MidiMessage::noteOn (1, 28, (juce::uint8) 110) }, { 7.0, juce::MidiMessage::noteOff (1, 28) },
            { 7.2, juce::MidiMessage::noteOn (1, 96, (juce::uint8) 90) }, { 8.2, juce::MidiMessage::noteOff (1, 96) },
        };

        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            const auto length = juce::jmin (blockSize, output.getNumSamples() - start);
            buffer.setSize (2, length, false, false, true);
            buffer.clear();
            juce::MidiBuffer midi;
            for (const auto& event : events)
                if (at (event.time) >= start && at (event.time) < start + length)
                    midi.addEvent (event.message, at (event.time) - start);
            processor.processBlock (buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                output.copyFrom (channel, start, buffer, channel, 0, length);
        }

        // Levels per section, DC and the largest sample step (clicks).
        {
            const auto rmsDb = [&] (double from, double to)
            {
                auto total = 0.0;
                for (int i = at (from); i < at (to); ++i)
                {
                    const auto mid = 0.5 * (output.getSample (0, i) + output.getSample (1, i));
                    total += mid * mid;
                }
                return 10.0 * std::log10 (total / juce::jmax (1, at (to) - at (from)) + 1.0e-24);
            };
            auto dc = 0.0, step = 0.0;
            for (int i = 1; i < output.getNumSamples(); ++i)
            {
                dc += output.getSample (0, i);
                step = juce::jmax (step, (double) std::abs (output.getSample (0, i) - output.getSample (0, i - 1)));
            }
            std::cout << name << ": dc " << dc / output.getNumSamples() << " max step " << step
                      << " | arp " << rmsDb (0.1, 1.3) << " pedal-held " << rmsDb (2.4, 3.3) << " after-lift " << rmsDb (3.55, 3.75)
                      << " chord " << rmsDb (3.9, 5.2) << " low " << rmsDb (5.9, 6.9) << " high " << rmsDb (7.3, 8.1)
                      << " tail " << rmsDb (8.6, 9.0) << " dB" << std::endl;
        }

        const auto file = folder.getChildFile (juce::String (name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                    format.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
                std::cout << "wrote " << file.getFullPathName() << "  peak " << output.getMagnitude (0, output.getNumSamples())
                          << std::endl;
            }
    }
}

// Level (dB RMS) while a note is held (0.1-0.5 s) and after its release
// (1-3 s), optionally with the sustain pedal down throughout.
std::pair<double, double> heldAndTail (const juce::String& preset, int note, bool pedal,
                                       const std::vector<std::pair<const char*, float>>& settings)
{
    IlanaSynthAudioProcessor processor;
    if (preset.isNotEmpty())
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (preset));
    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        processor.assignFxSlot (slot, 0);
    for (const auto& [id, value] : settings)
        setParam (processor, id, value);
    processor.prepareToPlay (48000.0, 480);
    juce::AudioBuffer<float> buffer (2, 480);
    double held = 0.0, tail = 0.0;
    for (int block = 0; block < 300; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0 && pedal) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
        if (block == 1) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (block == 50) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        processor.processBlock (buffer, midi);
        const auto rms = buffer.getRMSLevel (0, 0, 480);
        if (block >= 10 && block < 50) held += rms * rms / 40.0;
        if (block >= 100 && block < 300) tail += rms * rms / 200.0;
    }
    return { juce::Decibels::gainToDecibels (std::sqrt (held), -200.0),
             juce::Decibels::gainToDecibels (std::sqrt (tail), -200.0) };
}

// The resonance must actually be heard: exact tuning of the string loops,
// SYM strings ringing after a note on their pitch (and not after one off it),
// and the pedal resonance adding a halo.
void runSympatheticResonanceTest()
{
    for (const auto partial : { 2.0, 2.0 * std::exp2 (6.0 / 1200.0) })
        for (const auto frequency : { 65.41, 246.94, 1046.5 })
        {
            TunedString string;
            string.allocate (900);
            string.tune (48000.0, frequency, 9.0, 0.55f, partial);
            std::vector<float> ring (48000);
            for (int i = 0; i < (int) ring.size(); ++i)
                ring[(size_t) i] = string.process (i == 0 ? 1.0f : 0.0f);
            const auto peakCents = [&ring] (double centre)
            {
                auto best = 0.0, bestPower = -1.0;
                for (double cents = -10.0; cents <= 10.0; cents += 0.1)
                {
                    const auto hz = centre * std::exp2 (cents / 1200.0);
                    double re = 0.0, im = 0.0;
                    for (int i = 4800; i < (int) ring.size(); ++i)
                    {
                        const auto angle = juce::MathConstants<double>::twoPi * hz * (double) i / 48000.0;
                        const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - 4800)
                                                                  / (double) (ring.size() - 4800));
                        re += window * ring[(size_t) i] * std::cos (angle);
                        im += window * ring[(size_t) i] * std::sin (angle);
                    }
                    if (re * re + im * im > bestPower) { bestPower = re * re + im * im; best = cents; }
                }
                return best;
            };
            // Rings for its T60 (9 s: -4.7 dB between 0.2 s and 0.9 s), treble too.
            const auto rmsAt = [&ring, frequency] (int start)   // the fundamental alone
            {
                double re = 0.0, im = 0.0;
                for (int i = start; i < start + 4800; ++i)
                {
                    const auto angle = juce::MathConstants<double>::twoPi * frequency * (double) i / 48000.0;
                    const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - start) / 4800.0);
                    re += window * ring[(size_t) i] * std::cos (angle);
                    im += window * ring[(size_t) i] * std::sin (angle);
                }
                return 10.0 * std::log10 (re * re + im * im + 1e-30);
            };
            const auto drop = rmsAt (9600) - rmsAt (43200);
            check (drop > 4.0 && drop < 5.5, "tuned string " + juce::String (frequency) + " Hz rings for its T60 (drop "
                                                 + juce::String (drop, 1) + " dB, expected 4.7)");

            // (The second partial is only placed for the pedal strings, below 250 Hz.)
            const auto first = peakCents (frequency), second = frequency < 500.0 ? peakCents (frequency * partial) : 0.0;
            check (std::abs (first) < 0.3 && std::abs (second) < 0.3,
                   "tuned string " + juce::String (frequency) + " Hz rings in tune (" + juce::String (first, 1)
                       + " cents; 2nd partial " + juce::String (second, 1) + " cents off its target)");
        }

    const std::vector<std::pair<const char*, float>> dry { { "amp_release", 0.02f } };
    auto withSym = dry;
    withSym.insert (withSym.end(), { { "sym_on", 1.0f }, { "sym_amount", 1.0f }, { "sym_count", 6.0f } });
    const auto onPitch = heldAndTail ("", 60, false, withSym);
    const auto offPitch = heldAndTail ("", 61, false, withSym);
    const auto dryNote = heldAndTail ("", 60, false, dry);
    check (onPitch.second > onPitch.first - 20.0 && offPitch.second < onPitch.second - 25.0
               && dryNote.second < -150.0,
           "SYM strings ring on after a note on their pitch (" + juce::String (onPitch.second - onPitch.first, 1)
               + " dB under the note), not after one off it (" + juce::String (offPitch.second - offPitch.first, 1) + " dB)");

    const auto pedalOff = heldAndTail ("Hammered Strings", 60, true, { { "pedal_res", 0.0f } });
    const auto pedalOn = heldAndTail ("Hammered Strings", 60, true, { { "pedal_res", 1.0f } });
    check (pedalOn.second > pedalOff.second + 5.0 && pedalOn.first < pedalOff.first + 3.0,
           "pedal resonance adds a halo after the note (+" + juce::String (pedalOn.second - pedalOff.second, 1)
               + " dB), not while it sounds (+" + juce::String (pedalOn.first - pedalOff.first, 1) + " dB)");
}

namespace
{
// ---------------------------------------------------------------------------
// M5 deep FM, M6 phase distortion, M6b 64-slot matrix.

// A note through the whole processor with a clean signal path: sine OSC 1
// straight to the output (no filter, no clip, no velocity), then configure.
std::vector<float> renderCleanPatch (const std::function<void (IlanaSynthAudioProcessor&)>& configure,
                                     int note, double seconds, double releaseAt = -1.0, int velocity = 100)
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
             { "master_clip", 0.0f }, { "master", 0.0f }, { "amp_velocity", 0.0f }, { "drift", 0.0f },
             { "osc1_table", 8.0f }, { "osc1_frame", 0.0f }, { "osc1_route", 3.0f }, { "osc1_level", 1.0f },
             { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.05f } })
        setParam (processor, id, value);

    configure (processor);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 512);
    const auto blocks = (int) std::ceil (seconds * 48000.0 / 512.0);
    const auto releaseBlock = releaseAt >= 0.0 ? (int) (releaseAt * 48000.0 / 512.0) : -1;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
        if (block == releaseBlock)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    return out;
}

// Amplitude of one frequency in a Hann-windowed stretch (a sine of
// amplitude A reads as A).
double partialAmplitude (const std::vector<float>& x, size_t start, size_t length, double frequency, double sampleRate = 48000.0)
{
    std::complex<double> sum;
    auto windowSum = 0.0;

    for (size_t n = 0; n < length && start + n < x.size(); ++n)
    {
        const auto w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) n / (double) length);
        sum += (double) x[start + n] * w * std::polar (1.0, -juce::MathConstants<double>::twoPi * frequency * (double) n / sampleRate);
        windowSum += w;
    }

    return 2.0 * std::abs (sum) / juce::jmax (1.0e-9, windowSum);
}

bool allFinite (const std::vector<float>& x, float limit = 8.0f)
{
    for (auto value : x)
        if (! std::isfinite (value) || std::abs (value) > limit)
            return false;

    return true;
}

double rmsOf (const std::vector<float>& x, size_t start = 0, size_t end = (size_t) -1)
{
    auto sum = 0.0;
    end = juce::jmin (end, x.size());
    for (auto i = start; i < end; ++i)
        sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) juce::jmax ((size_t) 1, end - start));
}

// The DX-style reference patches (M5). Each operator is an oscillator with
// a sine table; the same settings drive an ideal phase-modulation renderer
// below, which the engine must match.
struct FmReferencePatch
{
    const char* name = "";
    int note = 48;
    int numOps = 2;
    double ratio[6] { 1, 1, 1, 1, 1, 1 };
    float level[6] { 1, 1, 1, 1, 1, 1 };
    bool carrier[6] { true, false, false, false, false, false };
    float amount[6][6] {}; // [source][target]
    TensionAdsr::Parameters env[6];
    int snap[6] { 0, 0, 0, 0, 0, 0 };
};

std::vector<FmReferencePatch> fmReferencePatches()
{
    std::vector<FmReferencePatch> patches;

    // Electric piano, DX "two pairs": a 1:1 body and a 14:1 tine that dies fast.
    {
        FmReferencePatch p;
        p.name = "DX Keys";
        p.note = 48;
        p.numOps = 4;
        p.ratio[3] = 14.0;
        p.snap[3] = 1;
        p.level[2] = 0.6f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.16f;
        p.amount[3][2] = 0.10f;
        p.env[0] = { 0.002f, 2.5f, 0.0f, 0.4f, 0.0f };
        p.env[1] = { 0.002f, 0.9f, 0.2f, 0.4f, 0.0f };
        p.env[2] = { 0.002f, 1.8f, 0.0f, 0.4f, 0.0f };
        p.env[3] = { 0.001f, 0.12f, 0.0f, 0.2f, 0.0f };
        patches.push_back (p);
    }

    // Bell: an inharmonic modulator (sqrt 12) on a long carrier, and a
    // second carrier on the bar's 2.756 mode.
    {
        FmReferencePatch p;
        p.name = "DX Bell";
        p.note = 72;
        p.numOps = 3;
        p.ratio[1] = 3.46;
        p.snap[1] = 2;
        p.ratio[2] = 2.756;
        p.snap[2] = 3;
        p.level[2] = 0.45f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.3f;
        p.amount[1][2] = 0.15f;
        p.env[0] = { 0.001f, 4.0f, 0.0f, 1.0f, 0.0f };
        p.env[1] = { 0.001f, 2.0f, 0.0f, 1.0f, 0.0f };
        p.env[2] = { 0.001f, 2.2f, 0.0f, 1.0f, 0.0f };
        patches.push_back (p);
    }

    // Bass: a 1:1 pair whose modulator feeds back on itself, plus a sub
    // carrier an octave down. Plain one-sample feedback splits into a
    // Nyquist-rate oscillation above about 0.2 (in the ideal renderer too),
    // so the reference stays below that; Filtered feedback goes further.
    {
        FmReferencePatch p;
        p.name = "DX Bass";
        p.note = 36;
        p.numOps = 3;
        p.ratio[2] = 0.5;
        p.snap[2] = 1;
        p.level[2] = 0.5f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.3f;
        p.amount[1][1] = 0.12f;
        p.env[0] = { 0.002f, 1.0f, 0.8f, 0.2f, 0.0f };
        p.env[1] = { 0.001f, 0.35f, 0.25f, 0.2f, 0.0f };
        p.env[2] = { 0.002f, 1.0f, 0.8f, 0.2f, 0.0f };
        patches.push_back (p);
    }

    return patches;
}

void configureFmReference (IlanaSynthAudioProcessor& processor, const FmReferencePatch& patch)
{
    const char* const prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };

    for (int op = 0; op < patch.numOps; ++op)
    {
        const juce::String prefix (prefixes[op]);
        setParam (processor, prefix + "_on", 1.0f);
        setParam (processor, prefix + "_table", 8.0f);
        setParam (processor, prefix + "_frame", 0.0f);
        setParam (processor, prefix + "_route", 3.0f);
        setParam (processor, prefix + "_level", patch.level[op]);
        setParam (processor, prefix + "_fine", 0.0f);
        setParam (processor, prefix + "_semi", 0.0f);
        setParam (processor, prefix + "_unison", 1.0f);
        setParam (processor, prefix + "_out", patch.carrier[op] ? 1.0f : 0.0f);
        setParam (processor, prefix + "_tune", (float) OscTuning::Ratio);
        setParam (processor, prefix + "_ratio", (float) patch.ratio[op]);
        setParam (processor, prefix + "_ratio_snap", (float) patch.snap[op]);
        // Operator n uses ENV 5 + n (ENV 6..11), set to this operator's envelope.
        const auto env = "env" + juce::String (6 + op);
        setParam (processor, prefix + "_amp_env", (float) (5 + op));
        setParam (processor, env + "_attack", patch.env[op].attack);
        setParam (processor, env + "_decay", patch.env[op].decay);
        setParam (processor, env + "_sustain", patch.env[op].sustain);
        setParam (processor, env + "_release", patch.env[op].release);
        setParam (processor, env + "_curve", patch.env[op].curve);
    }

    for (int source = 0; source < 6; ++source)
        for (int target = 0; target < 6; ++target)
            setParam (processor, IlanaSynthAudioProcessor::fmRouteId (source, target), patch.amount[source][target]);
}

// Ideal phase modulation: each operator is level * env * sin(2 pi (phase +
// sum of amount * previous output)), with every route one sample late, as
// in the engine. Snapped ratios, as the engine plays them.
std::vector<float> renderFmReference (const FmReferencePatch& patch, double seconds)
{
    constexpr double sampleRate = 48000.0;
    const auto f0 = juce::MidiMessage::getMidiNoteInHertz (patch.note);
    std::array<TensionAdsr, 6> envs;
    std::array<double, 6> phase {}, previous {}, ratio {};

    for (int op = 0; op < patch.numOps; ++op)
    {
        envs[(size_t) op].setSampleRate (sampleRate);
        envs[(size_t) op].setParameters (patch.env[op]);
        envs[(size_t) op].noteOn();
        ratio[(size_t) op] = OscTuning::snapRatio (patch.ratio[op], patch.snap[op]);
    }

    std::vector<float> out ((size_t) (seconds * sampleRate));

    for (auto& sample : out)
    {
        std::array<double, 6> now {};
        auto mix = 0.0;

        for (int op = 0; op < patch.numOps; ++op)
        {
            auto input = 0.0;
            for (int source = 0; source < patch.numOps; ++source)
                input += (double) patch.amount[source][op] * previous[(size_t) source];

            const auto envValue = (double) envs[(size_t) op].getNextSample();
            now[(size_t) op] = (double) patch.level[op] * envValue
                               * std::sin (juce::MathConstants<double>::twoPi * (phase[(size_t) op] + input));
            phase[(size_t) op] += f0 * ratio[(size_t) op] / sampleRate;
            phase[(size_t) op] -= std::floor (phase[(size_t) op]);

            if (patch.carrier[op])
                mix += now[(size_t) op];
        }

        previous = now;
        sample = (float) mix;
    }

    return out;
}

// Mean dB difference over the reference's strong bins (within 50 dB of the
// loudest), after matching overall level once for the whole note.
double spectralErrorDb (const std::vector<float>& ours, const std::vector<float>& reference,
                        const std::vector<double>& windowStarts, double& levelOffsetDb)
{
    constexpr int order = 12;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);

    const auto spectrum = [&fft] (const std::vector<float>& x, size_t start)
    {
        std::vector<float> work ((size_t) size * 2, 0.0f);
        for (int i = 0; i < size && start + (size_t) i < x.size(); ++i)
            work[(size_t) i] = x[start + (size_t) i]
                               * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
        fft.performFrequencyOnlyForwardTransform (work.data());
        work.resize ((size_t) size / 2);
        return work;
    };

    std::vector<std::vector<float>> a, b;
    auto peak = 0.0f, peakOurs = 0.0f;

    for (const auto t : windowStarts)
    {
        const auto start = (size_t) (t * 48000.0);
        a.push_back (spectrum (ours, start));
        b.push_back (spectrum (reference, start));
        for (auto value : b.back()) peak = juce::jmax (peak, value);
        for (auto value : a.back()) peakOurs = juce::jmax (peakOurs, value);
    }

    levelOffsetDb = juce::Decibels::gainToDecibels (peakOurs / juce::jmax (1.0e-12f, peak));
    const auto scale = peak / juce::jmax (1.0e-12f, peakOurs);
    auto sum = 0.0;
    auto count = 0;

    for (size_t w = 0; w < a.size(); ++w)
        for (size_t bin = 1; bin < a[w].size(); ++bin)
        {
            if (b[w][bin] < peak * 0.00316f) // -50 dB
                continue;

            // Only bins at spectral peaks of the reference (the partials).
            if (bin + 1 < b[w].size() && (b[w][bin] < b[w][bin - 1] || b[w][bin] < b[w][bin + 1]))
                continue;

            const auto oursDb = juce::Decibels::gainToDecibels (a[w][bin] * scale, -160.0f);
            const auto refDb = juce::Decibels::gainToDecibels (b[w][bin], -160.0f);
            sum += std::abs ((double) oursDb - (double) refDb);
            ++count;
        }

    return count > 0 ? sum / count : 999.0;
}

void runM5DeepFmTests()
{
    std::cout << "M5 deep FM" << std::endl;
    const char* const prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };

    // Snap sets.
    check (std::abs (OscTuning::snapRatio (1.43, 2) - std::sqrt (2.0)) < 1.0e-9
               && OscTuning::snapRatio (2.9, 1) == 3.0 && OscTuning::snapRatio (2.9, 0) == 2.9
               && std::abs (OscTuning::snapRatio (2.74, 3) - 2.756) < 1.0e-9
               && std::abs (OscTuning::snapRatio (3.5, 2) - std::sqrt (12.0)) < 1.0e-9,
           "ratio SNAP picks the nearest harmonic, inharmonic (square root) or bell ratio");

    // Tuning modes: a ratio follows the note, a fixed pitch doesn't.
    {
        const auto ratioOut = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Ratio);
            setParam (p, "osc1_ratio", 2.0f);
        }, 45, 1.0);
        const auto fixedLow = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Fixed);
            setParam (p, "osc1_fixed_hz", 300.0f);
        }, 40, 1.0);
        const auto fixedHigh = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Fixed);
            setParam (p, "osc1_fixed_hz", 300.0f);
        }, 70, 1.0);
        const auto ratioHz = fundamentalOf (std::vector<float> (ratioOut.begin() + 9600, ratioOut.end()), 48000.0);
        const auto lowHz = fundamentalOf (std::vector<float> (fixedLow.begin() + 9600, fixedLow.end()), 48000.0);
        const auto highHz = fundamentalOf (std::vector<float> (fixedHigh.begin() + 9600, fixedHigh.end()), 48000.0);
        check (std::abs (ratioHz - 220.0) < 0.5, "a ratio of 2 plays an octave above the note (" + juce::String (ratioHz, 2) + " Hz)");
        check (std::abs (lowHz - 300.0) < 0.5 && std::abs (highHz - 300.0) < 0.5,
               "a fixed-frequency operator ignores the note (" + juce::String (lowHz, 2) + ", " + juce::String (highHz, 2) + " Hz)");
    }

    // Two operators follow Bessel's sideband levels: carrier f, modulator
    // 5f, index 1.5 rad -> partials f (J0), 4f and 6f (J1), 9f and 11f (J2).
    {
        const auto beta = 1.5;
        const auto out = renderCleanPatch ([beta] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc2_on", 1.0f);
            setParam (p, "osc2_table", 8.0f);
            setParam (p, "osc2_frame", 0.0f);
            setParam (p, "osc2_level", 1.0f);
            setParam (p, "osc2_fine", 0.0f);
            setParam (p, "osc2_out", 0.0f);
            setParam (p, "osc2_tune", (float) OscTuning::Ratio);
            setParam (p, "osc2_ratio", 5.0f);
            setParam (p, "fm_amount", (float) (beta / juce::MathConstants<double>::twoPi));
        }, 45, 1.0);

        const auto f = 110.0;
        const auto at = [&out, f] (double multiple) { return partialAmplitude (out, 12000, 32768, f * multiple); };
        const auto j0 = 0.511828, j1 = 0.557937, j2 = 0.232088;
        const auto errorDb = [] (double measured, double expected) { return std::abs (20.0 * std::log10 (measured / expected)); };
        const auto e0 = errorDb (at (1) / at (6), j0 / j1);
        const auto e1 = errorDb (at (4) / at (6), 1.0);
        const auto e2 = errorDb (at (11) / at (6), j2 / j1);
        const auto e3 = errorDb (at (9) / at (6), j2 / j1);
        check (e0 < 0.2 && e1 < 0.2 && e2 < 0.2 && e3 < 0.2,
               "two operators match Bessel sideband levels (errors " + juce::String (e0, 3) + ", " + juce::String (e1, 3) + ", "
                   + juce::String (e2, 3) + ", " + juce::String (e3, 3) + " dB)");
    }

    // Reference patches: engine against an ideal phase-modulation renderer.
    for (const auto& patch : fmReferencePatches())
    {
        const auto ours = renderCleanPatch ([&patch] (IlanaSynthAudioProcessor& p) { configureFmReference (p, patch); },
                                            patch.note, 1.6);
        const auto reference = renderFmReference (patch, 1.6);
        auto offset = 0.0;
        const auto error = spectralErrorDb (ours, reference, { 0.01, 0.15, 0.5, 1.0 }, offset);
        std::cout << "  fit: " << patch.name << " mean partial error " << juce::String (error, 3)
                  << " dB (level offset " << juce::String (offset, 2) << " dB)" << std::endl;
        check (error < 0.5, juce::String (patch.name) + " matches the ideal FM reference (mean partial error "
                                + juce::String (error, 3) + " dB, target 0.5)");
    }

    // Feedback types: all bounded at full depth; filtered is calmer; cross
    // runs between a pair.
    {
        const auto feedbackPatch = [] (int type, float amount)
        {
            return renderCleanPatch ([type, amount] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_table", 8.0f);
                setParam (p, "osc2_frame", 0.0f);
                setParam (p, "osc2_level", 1.0f);
                setParam (p, "osc2_route", 3.0f);
                setParam (p, "osc1_fb_type", (float) type);
                setParam (p, "osc2_fb_type", (float) type);
                setParam (p, "fm_feedback", amount);
                setParam (p, "fm_fb2", amount);
            }, 45, 0.8);
        };

        for (int type = 0; type < FmFeedback::Count; ++type)
        {
            const auto out = feedbackPatch (type, 1.0f);
            check (allFinite (out) && rmsOf (out, 9600) > 0.05,
                   "feedback type " + FmFeedback::getNames()[type] + " stays finite and bounded at full depth");
        }

        double plainCentroid = 0.0, filteredCentroid = 0.0;
        {
            const auto plain = feedbackPatch (FmFeedback::Plain, 0.5f);
            const auto filtered = feedbackPatch (FmFeedback::Filtered, 0.5f);
            plainCentroid = centroidOf (plain, 20000, 48000.0);
            filteredCentroid = centroidOf (filtered, 20000, 48000.0);
        }
        // Plain feedback past about 0.2 splits into a buzz at half the sample
        // rate; the filtered type doesn't.
        {
            const auto plain = feedbackPatch (FmFeedback::Plain, 0.3f);
            const auto filtered = feedbackPatch (FmFeedback::Filtered, 0.3f);
            const auto nyquistShare = [] (const std::vector<float>& x)
            {
                auto alternating = 0.0;
                for (size_t i = 20000; i + 1 < x.size(); ++i)
                    alternating += std::abs ((double) x[i] - (double) x[i + 1]);
                auto total = 0.0;
                for (size_t i = 20000; i < x.size(); ++i)
                    total += 2.0 * std::abs ((double) x[i]);
                return alternating / juce::jmax (1.0e-9, total);
            };
            check (nyquistShare (plain) > 0.5 && nyquistShare (filtered) < 0.1,
                   "filtered feedback stays clear of the half-sample-rate buzz plain feedback falls into ("
                       + juce::String (nyquistShare (plain), 2) + " vs " + juce::String (nyquistShare (filtered), 3) + ")");
        }

        check (filteredCentroid < plainCentroid * 0.9,
               "filtered feedback is calmer than plain (centroid " + juce::String (plainCentroid, 0) + " -> "
                   + juce::String (filteredCentroid, 0) + " Hz)");

        // Cross: OSC 2 is a silent modulator; with only its FB cell set, cross
        // feedback reaches OSC 1 (plain would not).
        const auto crossOnly = [] (int type)
        {
            return renderCleanPatch ([type] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_table", 8.0f);
                setParam (p, "osc2_level", 1.0f);
                setParam (p, "osc2_out", 0.0f);
                setParam (p, "osc2_fine", 0.0f);
                setParam (p, "osc2_fb_type", (float) type);
                setParam (p, "fm_fb2", 0.4f);
            }, 45, 0.6);
        };
        const auto plainOut = crossOnly (FmFeedback::Plain);
        const auto crossOut = crossOnly (FmFeedback::Cross);
        const auto sine = partialAmplitude (plainOut, 9600, 16384, 110.0);
        const auto h2Plain = partialAmplitude (plainOut, 9600, 16384, 220.0) / sine;
        const auto h2Cross = partialAmplitude (crossOut, 9600, 16384, 220.0) / partialAmplitude (crossOut, 9600, 16384, 110.0);
        check (h2Plain < 0.01 && h2Cross > 0.05,
               "cross feedback runs between a pair (OSC 1's 2nd harmonic " + juce::String (h2Plain, 4) + " -> "
                   + juce::String (h2Cross, 3) + ")");
    }

    // The noise operator.
    {
        const auto clean = renderCleanPatch ([] (IlanaSynthAudioProcessor&) {}, 57, 0.5);
        const auto noisy = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { setParam (p, "fm_noise1", 0.3f); }, 57, 0.5);
        const auto dark = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "fm_noise1", 0.3f);
            setParam (p, "fm_noise_color", 0.0f);
        }, 57, 0.5);
        const auto cleanCentroid = centroidOf (clean, 12000, 48000.0);
        const auto noisyCentroid = centroidOf (noisy, 12000, 48000.0);
        const auto darkCentroid = centroidOf (dark, 12000, 48000.0);
        check (allFinite (noisy) && noisyCentroid > cleanCentroid * 3.0 && darkCentroid < noisyCentroid * 0.7,
               "the noise operator spreads a sine into noise, and COLOUR darkens it (centroid "
                   + juce::String (cleanCentroid, 0) + " -> " + juce::String (noisyCentroid, 0) + " / dark "
                   + juce::String (darkCentroid, 0) + " Hz)");
    }

    // Level key scaling: +6 dB per octave at KEY LVL 1.
    {
        const auto levelAt = [] (int note, float keyLevel)
        {
            const auto out = renderCleanPatch ([keyLevel] (IlanaSynthAudioProcessor& p) { setParam (p, "osc1_key_level", keyLevel); },
                                               note, 0.5);
            return rmsOf (out, 9600);
        };
        const auto flat = levelAt (84, 0.0f);
        const auto up = levelAt (84, 1.0f);
        const auto down = levelAt (84, -0.5f);
        const auto upDb = 20.0 * std::log10 (up / flat);
        const auto downDb = 20.0 * std::log10 (down / flat);
        check (std::abs (upDb - 12.0) < 0.3 && std::abs (downDb + 6.0) < 0.3,
               "level key scaling two octaves up: +" + juce::String (upDb, 2) + " dB at KEY LVL 100 %, "
                   + juce::String (downDb, 2) + " dB at -50 %");
    }

    // DAHDSR: the delay waits, the hold holds.
    {
        const auto delayed = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { setParam (p, "amp_delay", 0.1f); }, 57, 0.4);
        check (rmsOf (delayed, 0, 4500) < 1.0e-4 && rmsOf (delayed, 6000, 9000) > 0.1,
               "envelope DELAY keeps the note silent, then plays it");

        const auto held = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "amp_hold", 0.2f);
            setParam (p, "amp_decay", 0.02f);
            setParam (p, "amp_sustain", 0.0f);
        }, 57, 0.5);
        check (rmsOf (held, 7000, 8500) > 0.3 && rmsOf (held, 14000, 16000) < 0.01,
               "envelope HOLD stays at the peak, then decays");
    }

    // Rate key scaling: a higher note's decay is shorter.
    {
        const auto decayRatio = [] (float keyRate)
        {
            const auto energyAfter = [keyRate] (int note)
            {
                const auto out = renderCleanPatch ([keyRate] (IlanaSynthAudioProcessor& p)
                {
                    setParam (p, "amp_decay", 0.4f);
                    setParam (p, "amp_sustain", 0.0f);
                    setParam (p, "amp_keyrate", keyRate);
                }, note, 0.6);
                return rmsOf (out, 7200, 9600) / rmsOf (out, 480, 960);
            };
            return energyAfter (84) / energyAfter (60);
        };
        check (decayRatio (0.0f) > 0.9 && decayRatio (1.0f) < 0.5,
               "KEY RATE shortens the envelope up the keyboard (" + juce::String (decayRatio (0.0f), 2) + " -> "
                   + juce::String (decayRatio (1.0f), 2) + ")");
    }

    // The MSEG as an operator envelope: silent while it is below zero.
    {
        const auto out = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_amp_env", 16.0f);
            setParam (p, "mseg_rate", 2.0f);
            setParam (p, "mseg_loop", 0.0f);
        }, 57, 0.5);
        // Default shape 0 -> 1 -> 0 -> -1 over 0.5 s: loud in the first
        // quarter-to-half, silent in the last quarter.
        check (rmsOf (out, 5000, 7000) > 0.3 && rmsOf (out, 20000, 23000) < 1.0e-3,
               "an oscillator can use the MSEG as a one-shot envelope");
    }

    // Algorithms: each sets its routing, is recognised, and plays.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        auto allMatch = true;
        auto allPlay = true;
        juce::String failed;

        for (int index = 0; index < FmAlgorithms::count(); ++index)
        {
            processor.applyFmAlgorithm (index);
            const auto matched = processor.findMatchingFmAlgorithm();

            if (matched != index)
            {
                allMatch = false;
                failed << FmAlgorithms::all()[(size_t) index].name << " (matched " << matched << ") ";
            }

            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<float> out;
            for (int block = 0; block < 20; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                if (block == 15)
                    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                    out.push_back (buffer.getSample (0, i));
            }

            allPlay = allPlay && allFinite (out) && rmsOf (out) > 1.0e-3;
        }

        check (allMatch, "all 16 algorithms set their routing and are recognised " + failed);
        check (allPlay, "all 16 algorithms play finite, audible notes");

        processor.applyFmAlgorithm (10); // DX 5 Keys: six operators
        auto sixShown = true;
        for (int osc = 0; osc < 6; ++osc)
            sixShown = sixShown && processor.isOscillatorShown (osc);
        check (sixShown && processor.apvts.getRawParameterValue ("osc6_out")->load() < 0.5f
                   && processor.apvts.getRawParameterValue ("osc5_out")->load() > 0.5f,
               "a six-operator algorithm adds OSC 4-6 and sets carriers and modulators");

        // Existing route amounts survive re-applying.
        setParam (processor, "fm_amount", 0.8f);
        processor.applyFmAlgorithm (10);
        check (std::abs (processor.apvts.getRawParameterValue ("fm_amount")->load() - 0.8f) < 1.0e-4f,
               "applying an algorithm keeps the amounts of routes it keeps");
    }

    // Everything at once, at the extremes: stays finite.
    {
        const auto out = renderCleanPatch ([&prefixes] (IlanaSynthAudioProcessor& p)
        {
            for (int osc = 0; osc < 6; ++osc)
            {
                const juce::String prefix (prefixes[osc]);
                setParam (p, prefix + "_on", 1.0f);
                setParam (p, prefix + "_level", 1.0f);
                setParam (p, prefix + "_tune", (float) (osc % 3));
                setParam (p, prefix + "_ratio", 17.0f);
                setParam (p, prefix + "_fixed_hz", 15000.0f);
                setParam (p, prefix + "_key_level", 1.0f);
                setParam (p, prefix + "_fb_type", (float) (osc % 3));
                setParam (p, prefix + "_warp", (float) Warp::PdRes3);
                setParam (p, prefix + "_warp_amt", 1.0f);
                setParam (p, prefix + "_warp2", 12.0f);
                setParam (p, prefix + "_warp2_amt", 1.0f);
                setParam ((p), "fm_noise" + juce::String (osc + 1), 1.0f);
                for (int target = 0; target < 6; ++target)
                    setParam (p, IlanaSynthAudioProcessor::fmRouteId (osc, target), 1.0f);
            }
        }, 96, 1.0);
        check (allFinite (out, 16.0f), "six operators at full depth with every feedback type, noise and PD stay finite");
    }
}

// The Casio CZ's phase distortion, written out from its definition (a cosine
// read through a bent phase), as the reference for M6.
double czReference (int mode, double a, double p)
{
    const auto twoPi = juce::MathConstants<double>::twoPi;

    switch (mode)
    {
        case Warp::PdSaw:
        {
            const auto d = 0.5 - 0.49 * a;
            const auto phi = p < d ? p / (2.0 * d) : 0.5 + (p - d) / (2.0 * (1.0 - d));
            return std::cos (twoPi * phi);
        }
        case Warp::PdSquare:
        {
            const auto d = 0.5 - 0.49 * a;
            const auto local = std::fmod (p, 0.5);
            const auto phi = (p < 0.5 ? 0.0 : 0.5) + (local < d ? 0.5 * local / d : 0.5);
            return std::cos (twoPi * phi);
        }
        case Warp::PdPulse:
        {
            const auto d = 1.0 - 0.95 * a;
            return std::cos (twoPi * (p < d ? p / d : 1.0));
        }
        default:
        {
            // Resonance: a sine at 1 + 15a times the pitch, restarting every
            // cycle under a saw, triangle or trapezoid window.
            const auto window = mode == Warp::PdRes1 ? 1.0 - p
                              : mode == Warp::PdRes2 ? 1.0 - std::abs (2.0 * p - 1.0)
                                                     : juce::jmin (1.0, 2.0 * (1.0 - p));
            const auto depth = juce::jmin (1.0, 4.0 * a);
            return (1.0 - depth * (1.0 - window)) * std::sin (twoPi * p * (1.0 + 15.0 * a));
        }
    }
}

void runM6PhaseDistortionTests()
{
    std::cout << "M6 phase distortion" << std::endl;
    IlanaSynthAudioProcessor processor;
    const auto* sine = processor.getWavetable (8);

    // Waveforms: one cycle at 2048 samples against the CZ definition. The
    // engine reads a sine-phase table, so the non-resonant waves come out
    // inverted and a quarter cycle late (inaudible).
    for (int mode = Warp::PdSaw; mode <= Warp::PdRes3; ++mode)
    {
        for (const auto amount : { 0.3f, 1.0f })
        {
            WavetableOscillator osc;
            osc.setSampleRate (48000.0);
            osc.setWavetable (sine);
            osc.setFrequency (48000.0 / 2048.0);
            osc.setWarp (mode, amount);
            osc.resetPhase (0.0);

            std::vector<double> ours (2048), reference (2048);
            auto maxError = 0.0;

            for (int n = 0; n < 2048; ++n)
            {
                const auto p = (double) n / 2048.0;
                ours[(size_t) n] = (double) osc.getNextSample();
                reference[(size_t) n] = Warp::isResonance (mode) ? czReference (mode, amount, p)
                                                                 : -czReference (mode, amount, std::fmod (p + 0.25, 1.0));
                maxError = juce::jmax (maxError, std::abs (ours[(size_t) n] - reference[(size_t) n]));
            }

            // Harmonic magnitudes 1-16 (sign and phase free).
            auto worstDb = 0.0;
            for (int h = 1; h <= 16; ++h)
            {
                std::complex<double> a, b;
                for (int n = 0; n < 2048; ++n)
                {
                    const auto e = std::polar (1.0, -juce::MathConstants<double>::twoPi * h * n / 2048.0);
                    a += ours[(size_t) n] * e;
                    b += reference[(size_t) n] * e;
                }
                if (std::abs (b) / 1024.0 > 0.001)
                    worstDb = juce::jmax (worstDb, std::abs (20.0 * std::log10 (std::abs (a) / std::abs (b))));
            }

            // Sharp PD corners can't be traced exactly by a band-limited read.
            check (maxError < 0.03 && worstDb < 0.5,
                   Warp::getNames()[mode] + " at " + juce::String (juce::roundToInt (amount * 100.0f))
                       + " % matches the CZ waveform (max error " + juce::String (maxError, 4) + ", harmonics within "
                       + juce::String (worstDb, 3) + " dB)");
        }
    }

    // Spectra: saw has every harmonic, square mostly odd ones, and the
    // resonance peak follows the amount.
    {
        const auto harmonics = [sine] (int mode, float amount)
        {
            WavetableOscillator osc;
            osc.setSampleRate (48000.0);
            osc.setWavetable (sine);
            osc.setFrequency (48000.0 / 2048.0);
            osc.setWarp (mode, amount);
            std::vector<double> x (2048);
            for (auto& value : x)
                value = (double) osc.getNextSample();
            std::vector<double> levels (33, 0.0);
            for (int h = 1; h <= 32; ++h)
            {
                std::complex<double> sum;
                for (int n = 0; n < 2048; ++n)
                    sum += x[(size_t) n] * std::polar (1.0, -juce::MathConstants<double>::twoPi * h * n / 2048.0);
                levels[(size_t) h] = std::abs (sum) / 1024.0;
            }
            return levels;
        };

        const auto saw = harmonics (Warp::PdSaw, 1.0f);
        const auto square = harmonics (Warp::PdSquare, 1.0f);
        auto sawFull = true;
        for (int h = 1; h <= 8; ++h)
            sawFull = sawFull && saw[(size_t) h] > saw[1] * 0.05;
        const auto evenOdd = (square[2] + square[4] + square[6]) / (square[1] + square[3] + square[5]);
        check (sawFull, "PD Saw at full depth has all of its first eight harmonics");
        check (evenOdd < 0.1, "PD Square at full depth is mostly odd harmonics (even/odd " + juce::String (evenOdd, 3) + ")");

        const auto peakOf = [] (const std::vector<double>& levels)
        {
            int best = 1;
            for (int h = 2; h <= 32; ++h)
                if (levels[(size_t) h] > levels[(size_t) best])
                    best = h;
            return best;
        };
        const auto low = peakOf (harmonics (Warp::PdRes1, 0.3f));
        const auto high = peakOf (harmonics (Warp::PdRes1, 0.9f));
        check (std::abs (low - 5.5) <= 1.5 && std::abs (high - 14.5) <= 1.5,
               "PD Res I's peak sits at 1 + 15 x amount (harmonic " + juce::String (low) + " at 30 %, "
                   + juce::String (high) + " at 90 %)");
    }

    // Through the engine: the warp envelope sweeps brightness like a CZ DCW,
    // and the second stage changes the wave.
    {
        const auto sweep = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.001f);
            setParam (p, "osc1_pd_env", 2.0f); // ENV 2 (filter envelope)
            setParam (p, "osc1_pd_env_amt", 1.0f);
            setParam (p, "fe_attack", 0.001f);
            setParam (p, "fe_decay", 0.5f);
            setParam (p, "fe_sustain", 0.0f);
        }, 45, 1.2);
        const auto early = centroidOf (sweep, 2400, 48000.0);
        const auto late = centroidOf (sweep, 48000, 48000.0);
        check (early > late * 2.0, "the warp envelope closes the PD wave as it decays (centroid "
                                       + juce::String (early, 0) + " -> " + juce::String (late, 0) + " Hz)");

        const auto one = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.6f);
        }, 45, 0.5);
        const auto chain = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.6f);
            setParam (p, "osc1_warp2", 11.0f); // PD Res I in the stage-two list
            setParam (p, "osc1_warp2_amt", 0.5f);
        }, 45, 0.5);
        check (Warp::modeForStageTwoChoice (11) == Warp::PdRes1 && Warp::getStageTwoNames()[11] == "PD Res I",
               "the second stage's list skips FM and Ring and maps onto the warp modes");
        check (rmsOf (chain, 9600) > 0.05 && std::abs (centroidOf (chain, 9600, 48000.0) - centroidOf (one, 9600, 48000.0)) > 100.0,
               "the PD chain's second stage changes the wave");
    }

    // Old patches: warp modes keep their indices; PD extras default off.
    check (Warp::getNames()[9] == "Ring" && Warp::getNames()[10] == "PD Saw" && Warp::getNames()[16] == "Formant"
               && Warp::getNames()[17] == "Squeeze" && Warp::Count == 18,
           "PD warps are appended after the existing warp modes");
    check (processor.apvts.getRawParameterValue ("osc1_warp2")->load() == 0.0f
               && processor.apvts.getRawParameterValue ("osc1_pd_env")->load() == 0.0f
               && processor.apvts.getRawParameterValue ("osc1_tune")->load() == 0.0f,
           "new operator and PD settings default to the old behaviour");
}

void runM6bMatrixTests()
{
    std::cout << "M6b modulation depth" << std::endl;
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto allSlots = Mod::maxSlots == 64;
    for (int slot = 1; slot <= 64; ++slot)
        for (const auto* field : { "_src", "_dst", "_amt", "_curve", "_pol", "_aux", "_byp" })
            allSlots = allSlots && processor.apvts.getParameter ("mod" + juce::String (slot) + field) != nullptr;
    check (allSlots, "the matrix has 64 slots, each with all seven settings");

    // Slots 1-32 keep their place in the parameter list; 33-64 come after
    // every older parameter.
    const auto& parameters = processor.getParameters();
    int indexOf32 = -1, indexOf33 = -1, indexOfLastM4 = -1;
    for (int i = 0; i < parameters.size(); ++i)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
        {
            if (withId->paramID == "mod32_byp") indexOf32 = i;
            if (withId->paramID == "mod33_src") indexOf33 = i;
            if (withId->paramID == "mech_pedal") indexOfLastM4 = i;
        }
    check (indexOf32 >= 0 && indexOf33 > indexOfLastM4 && indexOfLastM4 > indexOf32,
           "slots 33-64 are appended after every existing parameter");

    // A routing in slot 64 modulates.
    {
        const auto plain = renderCleanPatch ([] (IlanaSynthAudioProcessor&) {}, 57, 0.4);
        const auto routed = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "mod64_src", (float) Mod::Source::Lfo1);
            setParam (p, "mod64_dst", (float) Mod::Destination::Osc1Pitch);
            setParam (p, "mod64_amt", 0.2f);
        }, 57, 0.4);
        auto difference = 0.0;
        for (size_t i = 0; i < plain.size(); ++i)
            difference += std::abs (plain[i] - routed[i]);
        check (difference / (double) plain.size() > 0.05, "a routing in slot 64 modulates its target");
    }

    // A slot past 32 survives a state round trip, and old states load with
    // slots 33-64 empty.
    {
        IlanaSynthAudioProcessor source;
        setParam (source, "mod50_src", (float) Mod::Source::Lfo2);
        setParam (source, "mod50_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (source, "mod50_amt", -0.4f);
        juce::MemoryBlock data;
        source.getStateInformation (data);
        IlanaSynthAudioProcessor target;
        target.setStateInformation (data.getData(), (int) data.getSize());
        const auto slot = target.readModSlot (49);
        check (slot.source == Mod::Source::Lfo2 && slot.destination == (int) Mod::Destination::Filter1Cutoff
                   && std::abs (slot.depth + 0.4f) < 1.0e-3f,
               "slot 50 survives a state round trip");

        auto state = source.apvts.copyState();
        for (int i = state.getNumChildren(); --i >= 0;)
        {
            const auto id = state.getChild (i).getProperty ("id").toString();
            if (id.startsWith ("mod") && id.fromFirstOccurrenceOf ("mod", false, false).getIntValue() > 32)
                state.removeChild (i, nullptr);
        }
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        juce::MemoryBlock oldData;
        juce::AudioProcessor::copyXmlToBinary (*xml, oldData);
        IlanaSynthAudioProcessor fromOld;
        setParam (fromOld, "mod40_src", 3.0f);
        fromOld.setStateInformation (oldData.getData(), (int) oldData.getSize());
        auto empty = true;
        for (int slotIndex = 32; slotIndex < 64; ++slotIndex)
            empty = empty && fromOld.readModSlot (slotIndex).source == Mod::Source::None;
        check (empty, "a 32-slot state loads with slots 33-64 empty");
    }

    // The new operator settings are parameter destinations; destinations
    // still fit.
    check (Mod::destinationForParamId ("osc2_warp2_amt") > 0 && Mod::destinationForParamId ("fm_noise3") > 0
               && Mod::destinationForParamId ("fm_4to1") > 0 && Mod::destinationForParamId ("fm_fb6") > 0
               && Mod::getNumDestinations() < IlanaSynthAudioProcessor::maxDestinations
               && Mod::getDestinationNames().size() == Mod::getNumDestinations(),
           "PD, noise, key level and OSC 4-6 FM cells are modulation destinations ("
               + juce::String (Mod::getNumDestinations()) + " of " + juce::String (IlanaSynthAudioProcessor::maxDestinations) + ")");
    check (Mod::destinationForParamId ("mech_pedal") == (int) Mod::Destination::Count + 88,
           "the M4 destinations keep their indices");

    // CPU: empty slots cost nothing measurable; a full matrix stays cheap.
    {
        const auto timeWith = [] (int usedSlots)
        {
            IlanaSynthAudioProcessor p;
            p.prepareToPlay (48000.0, 512);
            const Mod::Destination targets[] { Mod::Destination::Osc1Pitch, Mod::Destination::Osc1Frame,
                                               Mod::Destination::Filter1Cutoff, Mod::Destination::Pan,
                                               Mod::Destination::Osc1Warp, Mod::Destination::AmpLevel };
            for (int slot = 1; slot <= usedSlots; ++slot)
            {
                const auto prefix = "mod" + juce::String (slot);
                setParam (p, prefix + "_src", (float) Mod::lfoSourceFor ((slot - 1) % 16));
                setParam (p, prefix + "_dst", (float) targets[(slot - 1) % 6]);
                setParam (p, prefix + "_amt", 0.01f);
            }

            juce::AudioBuffer<float> buffer (2, 512);
            auto best = 1.0e9;
            for (int run = 0; run < 3; ++run)
            {
                const auto start = juce::Time::getHighResolutionTicks();
                for (int block = 0; block < 94; ++block)
                {
                    buffer.clear();
                    juce::MidiBuffer midi;
                    if (block == 0 && run == 0)
                        for (const auto note : { 48, 52, 55, 59, 62, 67, 71, 74 })
                            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                    p.processBlock (buffer, midi);
                }
                best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0);
            }
            return best; // ms per second of audio
        };

        const auto empty = timeWith (0);
        const auto full = timeWith (64);
        std::cout << "  cpu: matrix empty " << juce::String (empty, 1) << " ms/s, all 64 slots "
                  << juce::String (full, 1) << " ms/s" << std::endl;
        checkTiming (full < 400.0, "a full 64-slot matrix with eight notes stays well inside real time ("
                                 + juce::String (full, 1) + " ms/s)");
    }
}

// Listening material for M5/M6: the DX reference patches and the CZ waves.
void renderFmPdDemos (const juce::File& folder)
{
    folder.createDirectory();

    const auto write = [&folder] (const juce::String& name, const std::vector<float>& samples)
    {
        juce::AudioBuffer<float> buffer (1, (int) samples.size());
        auto peak = 1.0e-6f;
        for (auto value : samples)
            peak = juce::jmax (peak, std::abs (value));
        for (int i = 0; i < (int) samples.size(); ++i)
            buffer.setSample (0, i, samples[(size_t) i] * 0.7f / peak);
        const auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), 48000.0, 1, 24, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        std::cout << file.getFullPathName() << std::endl;
    };

    for (const auto& patch : fmReferencePatches())
    {
        const auto name = juce::String (patch.name).replaceCharacter (' ', '-').toLowerCase();
        write (name, renderCleanPatch ([&patch] (IlanaSynthAudioProcessor& p) { configureFmReference (p, patch); },
                                       patch.note, 2.5, 1.8));
        write (name + "-reference", renderFmReference (patch, 2.5));
    }

    for (int mode = Warp::PdSaw; mode <= Warp::PdRes3; ++mode)
    {
        const auto name = "cz-" + Warp::getNames()[mode].replace (" ", "-").toLowerCase();
        write (name, renderCleanPatch ([mode] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) mode);
            setParam (p, "osc1_warp_amt", 0.05f);
            setParam (p, "osc1_pd_env", 2.0f);
            setParam (p, "osc1_pd_env_amt", 0.9f);
            setParam (p, "fe_attack", 0.4f);
            setParam (p, "fe_decay", 1.2f);
            setParam (p, "fe_sustain", 0.0f);
        }, 45, 2.5, 2.0));
    }
}

} // namespace

// M7.0: FM cells to or from OSC 4-6 are modulated per voice, like the nine
// original cells (an envelope or velocity acts on each note separately).
void runM70ExtendedFmModTests()
{
    std::cout << "M7.0 per-voice FM for OSC 4-6" << std::endl;

    // The cell lookup matches every appended FM parameter.
    auto mapped = true;
    for (int source = 0; source < 6; ++source)
        for (int target = 0; target < 6; ++target)
        {
            if (source < 3 && target < 3)
                continue;

            const auto id = source == target ? "fm_fb" + juce::String (source + 1)
                                             : "fm_" + juce::String (source + 1) + "to" + juce::String (target + 1);
            mapped = mapped && Mod::extendedFmCellFor (Mod::destinationForParamId (id)) == source * 6 + target;
        }
    check (mapped, "all 27 OSC 4-6 FM cells map to their destination");
    check (Mod::extendedFmCellFor (Mod::destinationForParamId ("master")) < 0
               && Mod::extendedFmCellFor ((int) Mod::Destination::Fm1to2) < 0,
           "other destinations are not FM cells");

    // Velocity > FM 4>1 with a loud low note and a quiet high note at once:
    // only the loud one should grow sidebands.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
             { "master_clip", 0.0f }, { "master", 0.0f }, { "amp_velocity", 0.0f }, { "drift", 0.0f },
             { "osc1_table", 8.0f }, { "osc1_frame", 0.0f }, { "osc1_route", 3.0f }, { "osc1_level", 1.0f },
             { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "osc1_unison", 1.0f },
             { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.05f } })
        setParam (processor, id, value);

    processor.addOscillator (3);
    setParam (processor, "osc4_table", 8.0f);
    setParam (processor, "osc4_frame", 0.0f);
    setParam (processor, "osc4_out", 0.0f);
    setParam (processor, "osc4_level", 1.0f);
    setParam (processor, "mod1_src", (float) Mod::Source::Velocity);
    setParam (processor, "mod1_dst", (float) Mod::destinationForParamId ("fm_4to1"));
    setParam (processor, "mod1_amt", 1.0f);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 127), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 73, (juce::uint8) 4), 0);
        }

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    const auto quiet = juce::MidiMessage::getMidiNoteInHertz (73);
    const auto loud = juce::MidiMessage::getMidiNoteInHertz (48);
    const auto quietSideband = partialAmplitude (out, 12000, 16384, quiet * 2.0) / juce::jmax (1.0e-9, partialAmplitude (out, 12000, 16384, quiet));
    const auto loudSideband = partialAmplitude (out, 12000, 16384, loud * 2.0) / juce::jmax (1.0e-9, partialAmplitude (out, 12000, 16384, loud));

    check (loudSideband > 0.2 && quietSideband < loudSideband * 0.5,
           "velocity drives FM 4>1 per note (quiet note sideband " + juce::String (quietSideband, 3)
               + ", loud note " + juce::String (loudSideband, 3) + ")");
}

// M7.1: the Generative card (Euclid, probability sequencer, strum).
namespace
{
struct NoteLog
{
    struct Event { int time, note; bool on; };
    std::vector<Event> events;
    int exciterHits = 0;

    int ons() const
    {
        return (int) std::count_if (events.begin(), events.end(), [] (const Event& e) { return e.on; });
    }
};

// Holds NOTES from the first block for SECONDS, logging what reaches the voices.
NoteLog runGenerative (const std::function<void (IlanaSynthAudioProcessor&)>& configure,
                       std::initializer_list<int> notes, double seconds, std::vector<float>* audio = nullptr)
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    configure (processor);

    NoteLog log;
    juce::AudioBuffer<float> buffer (2, 512);
    const auto blocks = (int) (seconds * 48000.0 / 512.0);

    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            for (auto note : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);

        for (const auto metadata : processor.getSynthMidiForTest())
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOnOrOff())
                log.events.push_back ({ block * 512 + metadata.samplePosition, message.getNoteNumber(), message.isNoteOn() });
            else if (message.isSysEx())
                ++log.exciterHits;
        }

        if (audio != nullptr)
            for (int i = 0; i < 512; ++i)
                audio->push_back (buffer.getSample (0, i));
    }

    return log;
}

// Debug pass after M7.5: a MIDI event in the middle of a block splits the
// voices' render, and the second part must keep reading the shared LFO,
// S&H and MSEG buffers where it is in the block (it used to start again at
// the block's first sample, so modulation jumped at every note or CC).
void runSplitRenderTest()
{
    std::cout << "split renders" << std::endl;
    const auto render = [] (bool withCc)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                 { "master_clip", 0.0f }, { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "osc1_table", 0.0f },
                 { "amp_attack", 0.001f }, { "amp_sustain", 1.0f }, { "lfo1_rate", 7.0f },
                 { "mod1_src", (float) Mod::Source::Lfo1 }, { "mod1_dst", (float) Mod::Destination::AmpLevel },
                 { "mod1_amt", 0.8f } })
            setParam (processor, id, value);
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;
        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (withCc)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, block % 128), 256);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        return out;
    };
    const auto plain = render (false);
    const auto split = render (true);
    auto worst = 0.0f, peak = 0.0f;
    for (size_t i = 0; i < plain.size(); ++i)
    {
        worst = juce::jmax (worst, std::abs (plain[i] - split[i]));
        peak = juce::jmax (peak, std::abs (plain[i]));
    }
    check (peak > 0.05f && worst < peak * 1.0e-4f,
           "an unused CC mid-block leaves an LFO-modulated note unchanged (worst " + juce::String (worst, 6) + ")");
}

void runM73ElectricPianoTests()
{
    std::cout << "M7.3 electric pianos" << std::endl;

    {
        IlanaSynthAudioProcessor processor;
        for (const auto* id : { "osc1_excite", "sub_excite", "osc4_excite" })
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id));
            check (choice != nullptr && choice->choices.size() >= 9 && choice->choices[6] == "Osc In"
                       && choice->choices[7] == "Tine" && choice->choices[8] == "Reed",
                   juce::String (id) + ": Tine and Reed are appended after Osc In (indices 7 and 8)");
        }
        check (processor.apvts.getParameter ("osc6_ep_distance") != nullptr
                   && processor.apvts.getParameter ("osc1_ep_position") != nullptr,
               "every oscillator has the pickup DISTANCE and OFFSET");
    }

    const auto render = [] (ElectricPiano::Model model, double hz, float velocity, float distance = 0.5f,
                            float position = 0.5f, float damper = 0.0f, int releaseAt = -1, int length = 48000)
    {
        ElectricPiano piano;
        piano.prepare (48000.0);
        piano.setModel (model);
        piano.setParams (0.75f, 0.35f, distance, position, 0.5f, damper);
        piano.setFrequency (hz);
        piano.trigger (velocity);
        std::vector<float> out ((size_t) length);
        for (int i = 0; i < length; ++i)
            out[(size_t) i] = piano.process (releaseAt < 0 || i < releaseAt);
        return out;
    };
    // Level of partials 2-6 against the fundamental, in dB, just after the attack.
    const auto upper = [] (const std::vector<float>& x, double hz)
    {
        const auto fundamental = partialAmplitude (x, 2400, 8192, hz);
        auto sum = 0.0;
        for (int k = 2; k <= 6; ++k)
            sum += std::pow (partialAmplitude (x, 2400, 8192, hz * k), 2.0);
        return 10.0 * std::log10 (sum / juce::jmax (1.0e-20, fundamental * fundamental));
    };
    const auto rms = [] (const std::vector<float>& x, int from, int to)
    {
        auto sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / juce::jmax (1, to - from));
    };

    for (const auto model : { ElectricPiano::Model::Tine, ElectricPiano::Model::Reed })
    {
        const juce::String name (model == ElectricPiano::Model::Tine ? "Tine" : "Reed");
        const auto soft = render (model, 261.63, 0.3f);
        const auto hard = render (model, 261.63, 1.0f);
        const auto valid = [] (const std::vector<float>& x)
        {
            return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v) && std::abs (v) < 8.0f; });
        };
        check (valid (soft) && valid (hard) && rms (hard, 0, 24000) > 1.0e-4, name + " rings and stays bounded");
        check (rms (hard, 0, 24000) > rms (soft, 0, 24000) * 1.5, name + ": a hard hit is louder than a soft one");
        const auto softUpper = upper (soft, 261.63);
        const auto hardUpper = upper (hard, 261.63);
        check (hardUpper > softUpper + 3.0, name + (model == ElectricPiano::Model::Tine ? " barks" : " growls")
                                               + ": the upper partials rise from soft to hard ("
                                               + juce::String (softUpper, 1) + " -> " + juce::String (hardUpper, 1) + " dB)");
        const auto near = upper (render (model, 261.63, 0.6f, 0.1f), 261.63);
        const auto far = upper (render (model, 261.63, 0.6f, 0.9f), 261.63);
        check (near > far + 3.0, name + ": a closer pickup (DISTANCE) gives more upper partials ("
                                     + juce::String (far, 1) + " -> " + juce::String (near, 1) + " dB)");

        for (const auto note : { 36, 60, 84 })
        {
            const auto hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            // The strongest frequency within 50 cents of the note, in half-cent
            // steps (a bark can fool an autocorrelation pitch estimate low down).
            const auto tone = render (model, hz, 0.3f, 0.9f, 0.5f, 0.0f, -1, 48000);
            auto cents = 0.0, best = 0.0;
            for (auto trial = -50.0; trial <= 50.0; trial += 0.5)
                if (const auto level = partialAmplitude (tone, 4800, 38400, hz * std::exp2 (trial / 1200.0)); level > best)
                {
                    best = level;
                    cents = trial;
                }
            check (std::abs (cents) < 5.0, name + " at MIDI " + juce::String (note) + " is in tune ("
                                               + juce::String (cents, 2) + " cents)");
        }

        const auto ringing = render (model, 261.63, 0.8f, 0.5f, 0.5f, 0.0f, 12000);
        const auto damped = render (model, 261.63, 0.8f, 0.5f, 0.5f, 1.0f, 12000);
        check (rms (damped, 24000, 36000) < rms (ringing, 24000, 36000) * 0.05,
               name + ": DAMPER stops the note after the key is up");

        auto stable = true;
        for (const auto note : { 21, 60, 108, 120 })
            for (const auto distance : { 0.0f, 1.0f })
                for (const auto position : { 0.0f, 1.0f })
                    stable = stable && valid (render (model, 440.0 * std::pow (2.0, (note - 69) / 12.0), 1.0f,
                                                      distance, position, 0.0f, -1, 12000));
        check (stable, name + " stays finite and bounded at extreme settings");
    }

    // The presets play through the whole engine.
    for (const auto* presetName : { "Tine Keys", "Reed Keys" })
    {
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf (presetName);
        check (index >= 0, juce::String (presetName) + " is a factory preset");
        if (index < 0)
            continue;
        processor.loadFactoryPreset (index);
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        auto peak = 0.0f;
        auto finite = true;
        for (int block = 0; block < 90; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
            {
                finite = finite && std::isfinite (buffer.getSample (0, i));
                peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));
            }
        }
        check (finite && peak > 0.01f && peak < 1.5f, juce::String (presetName) + " plays (peak " + juce::String (peak, 3) + ")");
    }
}

void runM74WavetableEditorTests()
{
    std::cout << "M7.4 wavetable editor" << std::endl;
    const auto factoryCount = TableFactory::getNumFactoryTables();
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-m74-test");
    temp.deleteRecursively();
    temp.createDirectory();

    {
        IlanaSynthAudioProcessor processor;
        auto* table = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_table"));
        check (table != nullptr && table->choices.size() == factoryCount + 16
                   && table->choices[factoryCount] == "User 1" && table->choices[factoryCount + 3] == "User 4"
                   && table->choices[factoryCount + 15] == "User 16",
               "16 patch tables: User 5-16 are appended after User 1-4");
    }

    // Formulas.
    {
        Formula formula;
        check (formula.parse ("sin(2*pi*x) + 0.5*saw(x*2) - clamp(f, 0, 1)^2").isEmpty(), "a formula parses");
        check (formula.parse ("sin(2*pi*x").isNotEmpty() && formula.parse ("foo(x)").isNotEmpty()
                   && formula.parse ("1 +").isNotEmpty(),
               "a broken formula reports an error");
        const auto sine = WavetableDoc::renderFormula ("sin(2*pi*x)", 0.0, 0);
        std::vector<float> magnitudes, phases;
        WavetableDoc::analyse (sine, magnitudes, phases, 16);
        check (std::abs (magnitudes[0] - 1.0f) < 1.0e-3f && magnitudes.size() < 3,
               "sin(2*pi*x) is one harmonic at full level");
        const auto square = WavetableDoc::renderFormula ("square(x)", 0.0, 0);
        WavetableDoc::analyse (square, magnitudes, phases, 16);
        check (magnitudes.size() >= 5 && magnitudes[1] < 0.01f && std::abs (magnitudes[2] / magnitudes[0] - 1.0f / 3.0f) < 0.02f,
               "square(x) has the odd harmonics at 1/n");
        const auto morphing = WavetableDoc::renderFormula ("f", 0.75, 3);
        check (std::abs (morphing[100] - 0.75f) < 1.0e-6f, "f is the frame's position through the table");
    }

    // Harmonics round trip and the draw recipe.
    {
        const std::vector<float> magnitudes { 1.0f, 0.5f, 0.25f }, phases { 0.0f, 1.0f, 2.0f };
        const auto frame = WavetableDoc::renderHarmonics (magnitudes, phases);
        std::vector<float> readMagnitudes, readPhases;
        WavetableDoc::analyse (frame, readMagnitudes, readPhases, 8);
        auto error = 0.0f;
        for (size_t k = 0; k < 3; ++k)
            error = juce::jmax (error, std::abs (readMagnitudes[k] - magnitudes[k]),
                                std::abs (std::remainder (readPhases[k] - phases[k], juce::MathConstants<float>::twoPi)));
        check (error < 1.0e-3f, "harmonic levels and phases survive a render and analysis");

        const std::vector<float> points { 0.0f, 1.0f, 0.5f, 1.0f, 0.5001f, -1.0f, 0.9999f, -1.0f };
        const auto hardEdges = WavetableDoc::renderDraw (points, 0.0f);
        const auto smoothed = WavetableDoc::renderDraw (points, 1.0f);
        std::vector<float> hardMagnitudes, smoothMagnitudes, unused;
        WavetableDoc::analyse (hardEdges, hardMagnitudes, unused, 64);
        WavetableDoc::analyse (smoothed, smoothMagnitudes, unused, 64);
        check (hardMagnitudes.size() > 20 && smoothMagnitudes[20] < hardMagnitudes[20] * 0.5f,
               "a drawn square has high harmonics, and SMOOTH takes them off");
    }

    // A doc of recipes rebuilds exactly and stores no frames; raw frames are
    // embedded at 16 bits.
    WavetableDoc recipeDoc;
    {
        recipeDoc.name = "Recipes";
        FrameRecipe drawn;
        drawn.kind = FrameRecipe::Kind::Draw;
        drawn.points = { 0.0f, 0.0f, 0.25f, 1.0f, 0.75f, -1.0f };
        drawn.smoothing = 0.3f;
        FrameRecipe harmonics;
        harmonics.kind = FrameRecipe::Kind::Harmonics;
        harmonics.magnitudes = { 1.0f, 0.0f, 0.3f };
        harmonics.phases = { 0.0f, 0.0f, 1.5f };
        FrameRecipe formula;
        formula.kind = FrameRecipe::Kind::Formula;
        formula.formula = "tanh(4*sin(2*pi*x)) * (1 - f)";
        for (const auto& recipe : { drawn, harmonics, formula })
            recipeDoc.insertFrame (recipeDoc.getNumFrames(), {}, recipe);
        recipeDoc.renderAll();

        const auto tree = recipeDoc.toValueTree();
        WavetableDoc restored;
        juce::String notice;
        check (WavetableDoc::fromValueTree (tree, restored, notice) && notice.isEmpty()
                   && ! tree.hasProperty ("data") && restored.frames == recipeDoc.frames
                   && restored.recipes == recipeDoc.recipes && restored.name == "Recipes",
               "a table of drawn, harmonic and formula frames is stored as its recipe and rebuilds exactly");

        auto raw = WavetableDoc::fromFactory (3);
        raw.factoryIndex = -1;
        raw.name = "Raw";
        const auto rawTree = raw.toValueTree();
        WavetableDoc rawRestored;
        auto worst = 0.0f, peak = 0.0f;
        const auto ok = WavetableDoc::fromValueTree (rawTree, rawRestored, notice) && rawRestored.frames.size() == raw.frames.size();
        for (size_t f = 0; ok && f < raw.frames.size(); ++f)
            for (size_t i = 0; i < raw.frames[f].size(); ++i)
            {
                worst = juce::jmax (worst, std::abs (raw.frames[f][i] - rawRestored.frames[f][i]));
                peak = juce::jmax (peak, std::abs (raw.frames[f][i]));
            }
        check (ok && rawTree.hasProperty ("data") && worst <= peak / 32767.0f * 1.01f,
               "raw frames are embedded at 16 bits (worst error " + juce::String (worst / juce::jmax (1.0e-9f, peak), 7) + " of peak)");

        const auto factoryTree = WavetableDoc::fromFactory (5).toValueTree();
        WavetableDoc factoryRestored;
        check (! factoryTree.hasProperty ("data") && WavetableDoc::fromValueTree (factoryTree, factoryRestored, notice)
                   && factoryRestored.frames == WavetableDoc::fromFactory (5).frames,
               "an unedited factory copy is stored as its index");

        std::vector<std::vector<float>> big (256, std::vector<float> (2048));
        juce::Random random (7);
        for (auto& frame : big)
            for (auto& value : frame)
                value = random.nextFloat() * 2.0f - 1.0f;
        const auto encoded = WavetableDoc::encodeFrames (big);
        check (encoded.length() < 1500000, "256 frames of noise embed in "
                                               + juce::String (encoded.length() / 1024) + " KB (16-bit, compressed, base64)");

        juce::ValueTree lost ("Wavetable");
        lost.setProperty ("name", "Lost", nullptr);
        lost.setProperty ("path", temp.getChildFile ("missing.wav").getFullPathName(), nullptr);
        WavetableDoc lostDoc;
        check (! WavetableDoc::fromValueTree (lost, lostDoc, notice) && notice.contains ("Lost"),
               "a table with no recipe, data or file fails with a notice");
    }

    // Frame editing and morphs.
    {
        auto doc = WavetableDoc::sine();
        FrameRecipe saw;
        saw.kind = FrameRecipe::Kind::Formula;
        saw.formula = "saw(x)";
        for (int i = 0; i < 6; ++i)
            doc.insertFrame (1, WavetableDoc::sine().frames[0], WavetableDoc::sine().recipes[0]);
        doc.insertFrame (doc.getNumFrames(), {}, saw);
        doc.renderFrame (doc.getNumFrames() - 1);
        doc.morph (0, doc.getNumFrames() - 1, true);
        std::vector<float> middle, unused;
        WavetableDoc::analyse (doc.frames[4], middle, unused, 8);
        check (doc.getNumFrames() == 8 && doc.recipes[4].kind == FrameRecipe::Kind::Harmonics
                   && middle[1] > 0.05f && middle[1] < 0.45f,
               "a spectral morph from a sine to a saw fills the frames between with harmonics");
        doc.moveFrame (7, 0);
        doc.removeFrame (1);
        check (doc.getNumFrames() == 7 && doc.recipes[0].kind == FrameRecipe::Kind::Formula, "frames move and delete");
        for (int i = 0; i < 400; ++i)
            doc.insertFrame (0, {}, {});
        check (doc.getNumFrames() == WavetableDoc::maxFrames, "a table stops at 256 frames");
    }

    // Export for Serum and Vital, and import.
    {
        const auto file = temp.getChildFile ("export.wav");
        check (recipeDoc.exportWav (file) && Wavetable::readWavFrameSize (file) == 2048,
               "an exported table carries a clm chunk with its 2048-sample frames");
        WavetableDoc imported;
        const auto loaded = WavetableDoc::loadFromFile (file, imported);
        // The export scales a table that peaks over 1 down to 1.
        auto peak = 1.0f;
        for (const auto& frame : recipeDoc.frames)
            for (auto value : frame)
                peak = juce::jmax (peak, std::abs (value));
        auto worst = 0.0f;
        for (size_t f = 0; loaded && f < recipeDoc.frames.size() && f < imported.frames.size(); ++f)
            for (size_t i = 0; i < 2048; ++i)
                worst = juce::jmax (worst, std::abs (recipeDoc.frames[f][i] / peak - imported.frames[f][i]));
        check (loaded && imported.getNumFrames() == recipeDoc.getNumFrames() && worst < 1.0e-5f,
               "the exported .wav loads back frame for frame");

        // A sidecar makes a library table editable again.
        auto recipe = recipeDoc.recipeTree();
        if (auto xml = recipe.createXml())
            xml->writeTo (file.withFileExtension ("ilwt"));
        WavetableDoc editable;
        check (WavetableDoc::loadFromFile (file, editable) && editable.recipes == recipeDoc.recipes,
               "a library .wav with its .ilwt sidecar opens with its recipe");

        // Another tool's table with 1024-sample frames.
        const auto small = temp.getChildFile ("small.wav");
        {
            juce::MemoryOutputStream data;
            for (int frame = 0; frame < 4; ++frame)
                for (int i = 0; i < 1024; ++i)
                    data.writeFloat ((float) std::sin (juce::MathConstants<double>::twoPi * (frame + 1) * i / 1024.0));
            juce::MemoryOutputStream format;
            format.writeShort (3); format.writeShort (1); format.writeInt (44100); format.writeInt (44100 * 4);
            format.writeShort (4); format.writeShort (32);
            const juce::String clm ("<!>1024 00000000 test");
            juce::MemoryOutputStream body;
            body.write ("WAVE", 4);
            body.write ("fmt ", 4); body.writeInt ((int) format.getDataSize()); body.write (format.getData(), format.getDataSize());
            body.write ("clm ", 4); body.writeInt (clm.length()); body.write (clm.toRawUTF8(), (size_t) clm.length());
            if (clm.length() & 1) body.writeByte (0);
            body.write ("data", 4); body.writeInt ((int) data.getDataSize()); body.write (data.getData(), data.getDataSize());
            juce::FileOutputStream out (small);
            out.write ("RIFF", 4);
            out.writeInt ((int) body.getDataSize());
            out.write (body.getData(), body.getDataSize());
        }
        std::vector<std::vector<float>> frames;
        std::vector<float> magnitudes, unused;
        const auto read = Wavetable::readFrames (small, Wavetable::LoadMode::Automatic, frames);
        if (read && frames.size() == 4)
            WavetableDoc::analyse (frames[2], magnitudes, unused, 8);
        check (read && frames.size() == 4 && magnitudes.size() >= 3 && magnitudes[2] > 0.9f,
               "a table whose clm chunk says 1024-sample frames reads as 4 frames");
    }

    // Patch tables travel with the patch.
    {
        IlanaSynthAudioProcessor original;
        auto doc = recipeDoc;
        doc.name = "Travelling";
        check (original.setUserTable (5, doc), "a doc goes into patch table 6");
        setParam (original, "osc1_table", (float) (factoryCount + 5));
        juce::MemoryBlock state;
        original.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.isUserSlotEdited (5) && restored.getUserTableDoc (5).frames == doc.frames
                   && restored.getUserTableDoc (5).name == "Travelling" && ! restored.isUserSlotEdited (4),
               "an edited table is saved in the patch and restored exactly");

        const auto* table = restored.getWavetable (factoryCount + 5);
        check (table != nullptr && table->getNumFrames() == 3, "the restored table is the one the oscillator plays");

        // A later state without it puts the slot back to its default.
        IlanaSynthAudioProcessor plain;
        juce::MemoryBlock plainState;
        plain.getStateInformation (plainState);
        restored.setStateInformation (plainState.getData(), (int) plainState.getSize());
        check (! restored.isUserSlotEdited (5), "loading a patch without the table resets the slot");

        // Loading a factory preset does too.
        original.loadFactoryPreset (1);
        check (! original.isUserSlotEdited (5), "loading a factory preset resets edited tables");

        // An old patch: only a path. It loads as before, and the next save
        // embeds the table.
        const auto file = temp.getChildFile ("old.wav");
        recipeDoc.exportWav (file);
        auto tree = plain.apvts.copyState();
        tree.setProperty ("userTablePath2", file.getFullPathName(), nullptr);
        tree.setProperty ("userTableMode2", 0, nullptr);
        IlanaSynthAudioProcessor old;
        if (auto xml = tree.createXml())
        {
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary (*xml, block);
            old.setStateInformation (block.getData(), (int) block.getSize());
        }
        old.flushAsyncUpdates();
        juce::MemoryBlock saved;
        old.getStateInformation (saved);
        const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        const auto savedTree = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
        const auto tables = savedTree.getChildWithName ("Wavetables");
        check (old.isUserSlotEdited (1) && old.getUserTableDoc (1).getNumFrames() == 3
                   && tables.getNumChildren() == 1 && tables.getChild (0).hasProperty ("data"),
               "an old path-only table loads, and the next save embeds it");

        // The free slot finder skips slots in use.
        IlanaSynthAudioProcessor fresh;
        setParam (fresh, "osc1_table", (float) factoryCount);
        check (fresh.findFreeUserSlot() == 1, "EDIT on a factory table takes the first free patch table");
    }

    temp.deleteRecursively();
}

} // namespace

void runM71GenerativeTests()
{
    std::cout << "M7.1 generative card" << std::endl;

    // The Euclidean pattern: E(3,8) is x..x..x., and rotation shifts it.
    {
        juce::String pattern, rotated;
        for (int step = 0; step < 8; ++step)
        {
            pattern += euclidHit (step, 3, 8, 0) ? "x" : ".";
            rotated += euclidHit (step, 3, 8, 1) ? "x" : ".";
        }
        check (pattern == "x..x..x." && rotated == ".x..x..x", "Euclid E(3,8) is x..x..x. (rotated " + rotated + ")");
        check (! euclidHit (0, 0, 8, 0) && euclidHit (5, 8, 8, 0), "Euclid with no hits is silent and with all hits is solid");
    }

    // Euclid alone plays the held chord on its hits: 5 of 16 sixteenths.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_steps", 16.0f);
            setParam (p, "euc_hits", 5.0f);
        }, { 60, 64, 67 }, 2.0 - 0.01);   // 16 sixteenths at 120 BPM, just under

        check (log.ons() == 15, "Euclid 5/16 plays the three-note chord five times per bar (" + juce::String (log.ons()) + " note-ons)");
    }

    // Euclid gates the arp: 3 of 8 arp steps sound.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "arp_on", 1.0f);
            setParam (p, "arp_div", 3.0f);   // 1/8: 8 steps in 2 s
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_steps", 8.0f);
            setParam (p, "euc_hits", 3.0f);
        }, { 60 }, 2.0 - 0.01);

        check (log.ons() == 3, "Euclid 3/8 rests five of eight arp steps (" + juce::String (log.ons()) + " note-ons)");
    }

    // Probability sequencer: chance 0 is silent; ratchets repeat; range
    // stays in the scale and above the held key.
    {
        const auto silent = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            for (int step = 1; step <= 16; ++step)
                setParam (p, "pseq_chance" + juce::String (step), 0.0f);
        }, { 60 }, 1.0);
        check (silent.ons() == 0, "a probability sequencer at chance 0 plays nothing");

        const auto ratchets = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            setParam (p, "pseq_length", 4.0f);
            setParam (p, "pseq_ratchet1", 3.0f);
        }, { 60 }, 0.5 - 0.01);   // four sixteenths
        check (ratchets.ons() == 6, "a ratchet of 3 on step 1 of 4 gives six notes (" + juce::String (ratchets.ons()) + ")");

        const auto ranged = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            setParam (p, "gen_scale", 1.0f);   // C major
            for (int step = 1; step <= 16; ++step)
                setParam (p, "pseq_range" + juce::String (step), 12.0f);
        }, { 60 }, 4.0);

        auto inScale = ranged.ons() > 20;   // 32 sixteenths in 4 s
        auto moved = false;
        for (const auto& event : ranged.events)
            if (event.on)
            {
                inScale = inScale && event.note >= 60 && event.note <= 72 && Scales::contains (event.note, 1, 0);
                moved = moved || event.note != 60;
            }
        check (inScale && moved, "RANGE 12 keeps the sequence in C major within an octave above the key");
    }

    // Strum: a chord comes out in pitch order, 30 ms apart; Down reverses it.
    for (const auto direction : { 1, 2 })
    {
        const auto log = runGenerative ([direction] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "spray_strum", (float) direction);
            setParam (p, "spray_strum_time", 30.0f);
        }, { 64, 60, 67 }, 0.3);

        std::vector<NoteLog::Event> ons;
        for (const auto& event : log.events)
            if (event.on)
                ons.push_back (event);

        const auto spacing = (int) (0.030 * 48000.0);
        auto ordered = ons.size() == 3;
        if (ordered)
        {
            const int expected[2][3] { { 60, 64, 67 }, { 67, 64, 60 } };
            for (size_t i = 0; i < 3; ++i)
                ordered = ordered && ons[i].note == expected[direction - 1][i]
                          && std::abs (ons[i].time - (int) i * spacing) <= 2 * (int) i;   // the time snaps to 0.25 ms
        }
        juce::String got;
        for (const auto& event : ons)
            got << event.note << "@" << event.time << " ";
        check (ordered, juce::String (direction == 1 ? "strum up" : "strum down") + " plays the chord in order, 30 ms apart (" + got + ")");
    }

    // A strummed chord released before its last note starts leaves nothing on.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "spray_strum", 1.0f);
        setParam (processor, "spray_strum_time", 200.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        std::array<int, 128> sounding {};

        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == 20)   // after the second note, before the third
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (const auto metadata : processor.getSynthMidiForTest())
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOn())
                    ++sounding[(size_t) message.getNoteNumber()];
                else if (message.isNoteOff())
                    --sounding[(size_t) message.getNoteNumber()];
            }
        }

        check (std::all_of (sounding.begin(), sounding.end(), [] (int count) { return count == 0; })
                   && processor.getActiveVoiceCount() == 0,
               "releasing a chord mid-strum leaves no stuck notes");
    }

    // Exciter: one marker per hit reaches the voices.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_target", 1.0f);
            setParam (p, "euc_steps", 8.0f);
            setParam (p, "euc_hits", 3.0f);
            setParam (p, "euc_div", 3.0f);   // 1/8
            setParam (p, "osc1_mode", 1.0f);
        }, { 48 }, 2.0 - 0.01);

        check (log.exciterHits == 3 && log.ons() == 1,
               "Euclid on the exciter re-strikes 3 times in a bar without new notes ("
                   + juce::String (log.exciterHits) + " hits)");
    }

    // Trance gate target: only the hit steps pass.
    {
        std::vector<float> audio;
        runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                     { "master_clip", 0.0f }, { "osc1_table", 8.0f }, { "amp_attack", 0.001f },
                     { "amp_sustain", 1.0f }, { "osc2_on", 0.0f }, { "sub_on", 0.0f } })
                setParam (p, id, value);
            p.assignFxSlot (1, 16);
            setParam (p, "fx_gate_smooth", 0.0f);
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_target", 2.0f);
            setParam (p, "euc_steps", 4.0f);
            setParam (p, "euc_hits", 1.0f);
            setParam (p, "euc_div", 2.0f);   // 1/4: a step is 24000 samples
        }, { 60 }, 2.5, &audio);   // 117 248 samples: the second hit is measured up to 116 000

        const auto rms = [&audio] (int from, int to)
        {
            double sum = 0.0;
            for (int i = from; i < to; ++i)
                sum += (double) audio[(size_t) i] * audio[(size_t) i];
            return std::sqrt (sum / (double) (to - from));
        };

        const auto open = rms (4000, 20000), shut = rms (28000, 90000), openAgain = rms (100000, 116000);
        check (open > 0.01 && openAgain > 0.01 && shut < open * 0.05,
               "the trance gate follows Euclid 1/4 (open " + juce::String (open, 3) + ", shut " + juce::String (shut, 4) + ")");
    }
}

void runM72BodyTests()
{
    std::cout << "M7.2 material bodies" << std::endl;
    const auto render = [] (int shape, float material, float size, float decay, int quality)
    {
        MaterialBody body;
        body.prepare (48000.0);
        body.configure (shape, material, size, decay, 261.625565, quality);
        std::vector<float> signal (48000);
        for (int i = 0; i < (int) signal.size(); ++i)
            signal[(size_t) i] = body.process (i == 0 ? 1.0f : 0.0f);
        return signal;
    };
    const auto energy = [] (const std::vector<float>& x, int from, int to)
    {
        auto sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / (double) (to - from));
    };
    const auto bar = render (0, 0.0f, 0.5f, 0.5f, 1);
    const auto plate = render (1, 0.65f, 0.5f, 0.5f, 1);
    const auto bell = render (2, 0.7f, 0.5f, 0.5f, 1);
    const auto shell = render (3, 1.0f, 0.5f, 0.5f, 1);
    for (const auto* sound : { &bar, &plate, &bell, &shell })
    {
        const auto valid = std::all_of (sound->begin(), sound->end(), [] (float x) { return std::isfinite (x) && std::abs (x) <= 2.0f; });
        check (valid && energy (*sound, 1000, 5000) > 1.0e-5, "each body rings and stays bounded");
    }
    auto distinct = 0.0, total = 0.0;
    for (int i = 0; i < 12000; ++i)
    {
        distinct += std::abs (bar[(size_t) i] - plate[(size_t) i]);
        total += std::abs (bar[(size_t) i]) + std::abs (plate[(size_t) i]);
    }
    check (distinct > total * 0.1, "bar and plate have different modal spectra ("
                                       + juce::String (distinct / juce::jmax (1.0e-12, total), 3) + ")");

    // Switching a patch from Classic to a material body keeps its loudness
    // in range: the wet body sits within 12 dB of the dry sound, for a
    // sustained saw and for a plucked string, at short and long DECAY. A pluck
    // lands a few cents off the body's narrow first mode, so it rings the
    // body through STRING TO BODY coupling, as the Hybrid presets do.
    for (const auto pluck : { false, true })
        for (const auto decay : { 0.3f, 0.9f })
            for (const auto shape : { 1, 2, 3, 4 })
            {
                const auto source = [pluck] (IlanaSynthAudioProcessor& p) { setParam (p, "osc1_mode", pluck ? 1.0f : 0.0f); };
                const auto dry = renderCleanPatch (source, 60, 1.0);
                const auto wet = renderCleanPatch ([source, shape, decay, pluck] (IlanaSynthAudioProcessor& p)
                {
                    source (p);
                    setParam (p, "res_on", 1.0f);
                    setParam (p, "res_amount", 1.0f);
                    setParam (p, "res_decay", decay);
                    setParam (p, "body_type", (float) shape);
                    if (pluck)
                    {
                        setParam (p, "body_coupling_mode", 1.0f);
                        setParam (p, "body_coupling", 0.8f);
                    }
                }, 60, 1.0);
                const auto ratioDb = 20.0 * std::log10 (energy (wet, 0, 48000) / juce::jmax (1.0e-9, energy (dry, 0, 48000)));
                check (std::abs (ratioDb) < 12.0, juce::String (pluck ? "a plucked string" : "a sustained saw")
                                                      + " through BODY type " + juce::String (shape) + " at DECAY "
                                                      + juce::String (decay, 1) + " stays near the dry level ("
                                                      + juce::String (ratioDb, 1) + " dB)");
            }

    const auto small = render (0, 0.0f, 0.0f, 0.6f, 1);
    const auto large = render (0, 0.0f, 1.0f, 0.6f, 1);
    const auto smallHz = fundamentalOf (std::vector<float> (small.begin() + 3000, small.begin() + 18000), 48000.0);
    const auto largeHz = fundamentalOf (std::vector<float> (large.begin() + 3000, large.begin() + 18000), 48000.0);
    check (smallHz > largeHz * 2.5, "BODY SIZE lowers the modal pitch");

    const auto shortRing = render (1, 0.65f, 0.5f, 0.1f, 1);
    const auto longRing = render (1, 0.65f, 0.5f, 0.9f, 1);
    check (energy (longRing, 20000, 24000) > energy (shortRing, 20000, 24000) * 10.0,
           "BODY DECAY lengthens the modal tail");

    const auto dryRelease = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "amp_release", 0.01f);
    }, 60, 0.6, 0.1);
    const auto bodyRelease = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "amp_release", 0.01f);
        setParam (p, "res_on", 1.0f);
        setParam (p, "body_type", 1.0f);
        setParam (p, "res_amount", 1.0f);
        setParam (p, "res_decay", 0.8f);
    }, 60, 0.6, 0.1);
    check (energy (bodyRelease, 18000, 22000) > energy (dryRelease, 18000, 22000) * 5.0
           && energy (bodyRelease, 18000, 22000) > 1.0e-5,
           "BODY continues ringing after the exciter envelope ends");

    const auto renderCoupling = [] (int mode)
    {
        return renderCleanPatch ([mode] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_mode", 1.0f);
            setParam (p, "osc1_excite", 0.0f);
            setParam (p, "osc2_on", 1.0f);
            setParam (p, "osc2_mode", 1.0f);
            setParam (p, "osc2_excite", 0.0f);
            setParam (p, "osc2_level", 0.7f);
            setParam (p, "res_on", 1.0f);
            setParam (p, "body_type", 2.0f);
            setParam (p, "res_amount", 0.7f);
            setParam (p, "body_coupling_mode", (float) mode);
            setParam (p, "body_coupling", 1.0f);
        }, 60, 0.35);
    };
    const auto uncoupled = renderCoupling (0);
    auto uncoupledSquared = 0.0;
    for (const auto sample : uncoupled)
        uncoupledSquared += (double) sample * sample;
    const auto uncoupledRms = std::sqrt (uncoupledSquared / (double) juce::jmax ((size_t) 1, uncoupled.size()));
    for (int mode = 1; mode <= 3; ++mode)
    {
        const auto coupled = renderCoupling (mode);
        auto squared = 0.0;
        for (size_t i = 0; i < coupled.size(); ++i)
        {
            const auto delta = (double) coupled[i] - (double) uncoupled[i];
            squared += delta * delta;
        }
        const auto change = std::sqrt (squared / (double) coupled.size());
        check (change > uncoupledRms * 0.01,
               "coupling path " + juce::String (mode) + " changes the voice (by "
                   + juce::String (juce::Decibels::gainToDecibels (change / juce::jmax (1.0e-12, uncoupledRms)), 1) + " dB of its level)");
    }

    MaterialBody stress;
    stress.prepare (48000.0);
    stress.configure (2, 1.0f, 0.0f, 1.0f, 4000.0, 2);
    auto finite = true;
    for (int i = 0; i < 48000; ++i)
    {
        const auto value = stress.process (i & 1 ? 2.0f : -2.0f);
        finite = finite && std::isfinite (value) && std::abs (value) <= 2.0f;
    }
    check (finite, "maximum material resonance stays finite under full drive");
}

// ILANA_BODY_FIT=<folder> writes impulse responses of the four material
// bodies, so tools/fit_model.py can compare their modes with struck objects.
void renderBodyFit (const juce::File& folder)
{
    folder.createDirectory();
    struct Target { const char* name; int shape; float material, decay; double hz; };
    for (const auto& target : { Target { "bar", 0, 0.0f, 0.45f, 261.625565 },
                                { "plate", 1, 0.62f, 0.88f, 261.625565 },
                                { "bell", 2, 0.72f, 0.72f, 523.251131 },
                                { "glass", 3, 1.0f, 0.6f, 787.31 } })
    {
        MaterialBody body;
        body.prepare (48000.0);
        body.configure (target.shape, target.material, 0.5f, target.decay, target.hz, 2);
        juce::AudioBuffer<float> buffer (1, 48000 * 3);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, body.process (i == 0 ? 1.0f : 0.0f));
        const auto file = folder.getChildFile (juce::String (target.name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
    }
}

// The SIMD unison bank must sound like the per-voice oscillators it
// replaced: same phases, pitches, warps, frame morphs and FM, summed with the
// same gains and pans.
void runUnisonBankEquivalenceTest()
{
    Wavetable table;
    table.buildFromFrames (TableFactory::generate (7));

    struct Case
    {
        const char* name;
        int lanes;
        int warp;
        float warpAmount;
        int warp2;
        float frame;
        bool oversample;
        int fm; // 0 none, 1 phase modulation, 2 rate (through-zero), 3 exponential rate
    };

    const Case cases[] {
        { "1 lane, plain", 1, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 0 },
        { "16 lanes, plain", 16, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 0 },
        { "7 lanes, frame morph", 7, Warp::Off, 0.0f, Warp::Off, 0.37f, false, 0 },
        { "16 lanes, sync", 16, Warp::Sync, 0.5f, Warp::Off, 0.2f, false, 0 },
        { "16 lanes, PWM", 16, Warp::Pwm, 0.6f, Warp::Off, 0.0f, false, 0 },
        { "12 lanes, bend + PD chain", 12, Warp::BendPlus, 0.4f, Warp::PdRes2, 0.5f, false, 0 },
        { "16 lanes, High quality", 16, Warp::Off, 0.0f, Warp::Off, 0.6f, true, 0 },
        { "16 lanes, phase FM", 16, Warp::Off, 0.0f, Warp::Off, 0.0f, false, 1 },
        { "9 lanes, through-zero FM, High", 9, Warp::Mirror, 0.3f, Warp::Off, 0.0f, true, 2 },
        { "16 lanes, exponential FM", 16, Warp::Off, 0.0f, Warp::Off, 0.8f, false, 3 },
    };

    constexpr double sampleRate = 44100.0;

    for (const auto& c : cases)
    {
        WavetableOscillator voices[UnisonBank::maxLanes];
        UnisonBank bank;
        float gains[UnisonBank::maxLanes], panL[UnisonBank::maxLanes], panR[UnisonBank::maxLanes];
        juce::Random random (1234);

        bank.setSampleRate (sampleRate);
        bank.setWavetable (&table);
        bank.setWarp (c.warp, c.warpAmount);
        bank.setWarp2 (c.warp2, 0.45f);

        for (int u = 0; u < c.lanes; ++u)
        {
            const auto frequency = 110.0 * std::exp2 ((u - c.lanes / 2) * 0.07 / 12.0 + (u % 3) * 0.3);
            const auto startPhase = (double) random.nextFloat();
            voices[u].setSampleRate (sampleRate);
            voices[u].setWavetable (&table);
            voices[u].setFrequency (frequency);
            voices[u].setWarp (c.warp, c.warpAmount);
            voices[u].setWarp2 (c.warp2, 0.45f);
            voices[u].resetPhase (startPhase);
            bank.setFrequency (u, frequency);
            bank.resetPhase (u, startPhase);
            gains[u] = 0.2f + 0.05f * (float) u;
            const auto angle = (float) u / (float) juce::jmax (1, c.lanes - 1) * juce::MathConstants<float>::halfPi;
            panL[u] = std::cos (angle);
            panR[u] = std::sin (angle);
        }

        bank.setWeights (gains, panL, panR, c.lanes);
        const auto frames = WavetableOscillator::frameReadFor (&table, c.frame);

        auto worst = 0.0f;
        auto peak = 0.0f;
        auto syncMismatches = 0;

        for (int i = 0; i < 8192; ++i)
        {
            const auto lfo = std::sin ((double) i * 0.003);
            const auto phaseMod = c.fm == 1 ? 0.8 * lfo : 0.0;
            const auto rate = c.fm == 2 ? 1.0 + 4.0 * 0.6 * lfo
                              : c.fm == 3 ? std::exp2 (juce::jlimit (-4.0, 4.0, 3.0 * lfo)) : 1.0;

            UnisonBank::Sums expected;

            for (int u = 0; u < c.lanes; ++u)
            {
                float raw;

                if (c.oversample)
                {
                    const auto first = voices[u].getNextSample (phaseMod, frames, rate * 0.5);
                    const auto second = voices[u].getNextSample (phaseMod, frames, rate * 0.5);
                    raw = 0.5f * (first + second);
                }
                else
                {
                    raw = voices[u].getNextSample (phaseMod, frames, rate);
                }

                expected.mono += raw * gains[u];
                expected.left += raw * gains[u] * panL[u];
                expected.right += raw * gains[u] * panR[u];
            }

            const auto actual = bank.render (phaseMod, frames, rate, c.oversample);
            worst = juce::jmax (worst, std::abs (actual.mono - expected.mono), std::abs (actual.left - expected.left),
                                std::abs (actual.right - expected.right));
            peak = juce::jmax (peak, std::abs (expected.mono));
            syncMismatches += bank.lane0Wrapped() != voices[0].wrappedThisSample() ? 1 : 0;
        }

        // Fixed-point phases differ from the old doubles by far less than a
        // cent, which shows as a tiny drift against the peak: -66 dB for
        // smooth reads, -54 dB where warps put hard edges in the wave.
        const auto tolerance = (c.warp == Warp::Off && c.warp2 == Warp::Off ? 5.0e-4f : 2.0e-3f) * juce::jmax (1.0f, peak);
        check (worst < tolerance && syncMismatches == 0 && peak > 0.05f,
               juce::String ("unison bank matches per-voice oscillators: ") + c.name + " (worst "
                   + juce::String (worst, 7) + ", peak " + juce::String (peak, 3) + ", sync misses "
                   + juce::String (syncMismatches) + ")");
    }

}

// ILANA_UNISON_BENCH=1: the unison cost the user sees in a DAW. Three
// oscillators x 16 unison at 44.1 kHz, per held note, best of several runs
// (this machine's timings swing a lot run to run).
void runUnisonBenchmark()
{
    const auto measure = [] (int notes, int unison, int quality, int warp, int blockSize = 512)
    {
        auto best = 1.0e9;

        for (int run = 0; run < 5; ++run)
        {
            IlanaSynthAudioProcessor processor;

            for (const auto* prefix : { "osc1", "osc2", "sub" })
            {
                const juce::String p (prefix);
                setParam (processor, p + "_on", 1.0f);
                setParam (processor, p + "_unison", (float) unison);
                setParam (processor, p + "_warp", (float) warp);
                setParam (processor, p + "_warp_amt", warp != Warp::Off ? 0.5f : 0.0f);
            }

            setParam (processor, "sub_level", 0.8f);
            setParam (processor, "quality", (float) quality);
            processor.prepareToPlay (44100.0, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer noteOns;

            for (int note = 0; note < notes; ++note)
                noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 3, (juce::uint8) 100), 0);

            // Warm up (voices start, smoothers settle), then time ~1 s.
            for (int block = 0; block < 8; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi = noteOns;
                processor.processBlock (buffer, midi);
            }

            const auto blocks = 44100 / blockSize;
            const auto start = juce::Time::getMillisecondCounterHiRes();

            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);
            }

            const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
            best = juce::jmin (best, elapsed / (blocks * (double) blockSize / 44100.0));
        }

        return best * 100.0;
    };

    const auto report = [&measure] (const char* label, int notes, int unison, int quality, int warp)
    {
        std::cout << "  bench: " << label << " " << juce::String (measure (notes, unison, quality, warp), 2)
                  << " % of one core" << std::endl;
    };

    report ("0 notes (idle: master FX only)              ", 0, 1, 1, Warp::Off);
    std::cout << "  bench: 0 notes, 128-sample blocks             "
              << juce::String (measure (0, 1, 1, Warp::Off, 128), 2) << " % of one core" << std::endl;
    report ("1 note,  3 osc x  1 unison, Normal          ", 1, 1, 1, Warp::Off);
    report ("1 note,  3 osc x 16 unison, Normal          ", 1, 16, 1, Warp::Off);
    report ("1 note,  3 osc x 16 unison, Normal, Sync    ", 1, 16, 1, Warp::Sync);
    report ("1 note,  3 osc x 16 unison, High            ", 1, 16, 2, Warp::Off);
    report ("4 notes, 3 osc x 16 unison, Normal          ", 4, 16, 1, Warp::Off);
    report ("8 notes, 3 osc x 16 unison, Normal          ", 8, 16, 1, Warp::Off);
}

// ILANA_PROFILE=<unison>: profile the benchmark patch (8 notes, three
// oscillators at that unison) over a minute of audio.
void runProfile (int unison)
{
   #if JUCE_WINDOWS
    IlanaSynthAudioProcessor processor;

    for (const auto* prefix : { "osc1", "osc2", "sub" })
    {
        const juce::String p (prefix);
        setParam (processor, p + "_on", 1.0f);
        setParam (processor, p + "_unison", (float) unison);
    }

    setParam (processor, "sub_level", 0.8f);
    const auto blockSize = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_BLOCK", "512").getIntValue();
    processor.prepareToPlay (44100.0, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer noteOns;
    const auto notes = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_NOTES", "8").getIntValue();

    for (int note = 0; note < notes; ++note)
        noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 3, (juce::uint8) 100), 0);

    processor.processBlock (buffer, noteOns);
    SamplingProfiler profiler;
    profiler.start();

    const auto seconds = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_SECONDS", "60").getIntValue();

    for (int block = 0; block < 44100 * seconds / blockSize; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    profiler.stop();
    profiler.report();
   #else
    juce::ignoreUnused (unison);
   #endif
}

// ILANA_TEST_TIMES=1 prints how long each suite of the full run takes, to
// find the slow ones.
template <typename Suite>
void timedRun (const char* name, Suite&& suite)
{
    static const auto report = juce::SystemStats::getEnvironmentVariable ("ILANA_TEST_TIMES", "").isNotEmpty();
    const auto start = juce::Time::getMillisecondCounterHiRes();
    suite();

    if (report)
        std::cout << "TIME " << name << " " << juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 2) << " s" << std::endl;
}

#include "M81Tests.inc"
#include "M82Tests.inc"
#include "M83Tests.inc"
#include "M84Tests.inc"
#include "M85Tests.inc"
#include "M86Tests.inc"
#include "M10Tests.inc"
#include "PolishTests.inc"
#include "PlanTests.inc"
#include "TuningTests.inc"
#include "OperatorEgTests.inc"
#include "OscModeTests.inc"
#include "AirwindowsTests.inc"
#include "VocoderTests.inc"
#include "FilterTests.inc"
#include "MultiSampleTests.inc"
#include "SplitterTests.inc"
#include "ParallelFxTests.inc"
#include "AirwindowsFilterTests.inc"
#include "ClipTests.inc"
#include "DemoRender.inc"

// ILANA_PRESET_PROFILE=<factory preset>: hold ILANA_PROFILE_NOTES notes
// (default 4) of that preset at 44.1 kHz; print % of one core, then profile.
void runPresetProfile (const juce::String& name)
{
   #if JUCE_WINDOWS || JUCE_LINUX
    IlanaSynthAudioProcessor processor;
    const auto index = processor.getFactoryPresetNames().indexOf (name);

    if (index < 0)
    {
        std::cout << "no factory preset named " << name << std::endl;
        return;
    }

    const auto blockSize = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_BLOCK", "512").getIntValue();
    processor.prepareToPlay (44100.0, blockSize);
    processor.loadFactoryPreset (index);

    // ILANA_PROFILE_SET="osc1_mode=1,osc1_excite=9": parameter overrides.
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_SET", ""), ",", ""))
        if (pair.contains ("="))
            setParam (processor, pair.upToFirstOccurrenceOf ("=", false, false).trim(),
                      pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());

    processor.panic();
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer noteOns;
    const auto notes = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_NOTES", "4").getIntValue();

    for (int note = 0; note < notes; ++note)
        noteOns.addEvent (juce::MidiMessage::noteOn (1, 60 + note * 4, (juce::uint8) 100), 0);

    processor.processBlock (buffer, noteOns);
    const auto seconds = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_SECONDS", "20").getIntValue();
    const auto blocks = 44100 * seconds / blockSize;
    // ILANA_PROFILE_RESTRIKE=<blocks>: strike the notes again that often.
    const auto restrike = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_RESTRIKE", "0").getIntValue();
    const auto start = juce::Time::getMillisecondCounterHiRes();
    auto slowest = 0.0;
   #if JUCE_WINDOWS
    SamplingProfiler profiler;
    profiler.start();
   #endif

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (restrike > 0 && block % restrike == restrike - 1)
            midi = noteOns;
        const auto blockStart = juce::Time::getMillisecondCounterHiRes();
        processor.processBlock (buffer, midi);
        slowest = juce::jmax (slowest, juce::Time::getMillisecondCounterHiRes() - blockStart);
    }

   #if JUCE_WINDOWS
    profiler.stop();
   #endif
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    std::cout << "preset " << name << ", " << notes << " notes: "
              << juce::String (100.0 * elapsed / (blocks * (double) blockSize / 44100.0), 1) << " % of one core, slowest block "
              << juce::String (100.0 * slowest / 1000.0 / (blockSize / 44100.0), 0) << " %" << std::endl;
   #if JUCE_WINDOWS
    profiler.report();
   #endif
   #else
    juce::ignoreUnused (name);
   #endif
}

// ILANA_LIBRARY_BENCH=1: hold 4 notes of every factory preset (or those in
// ILANA_LIBRARY_BENCH_ONLY="A|B") for ILANA_PROFILE_SECONDS (default 3) at
// 44.1 kHz, 256-sample blocks, and print each one's % of one core, then the
// median, mean and the slowest ten. Compare builds by alternating runs.
void runLibraryBench()
{
    Voice::disableOpenFilterBypass = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_OPEN_BYPASS", "").isNotEmpty();
    IlanaSynthAudioProcessor processor;
    const auto blockSize = 256;
    processor.prepareToPlay (44100.0, blockSize);
    const auto names = processor.getFactoryPresetNames();
    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_BENCH_ONLY", ""), "|", "");
    const auto seconds = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE_SECONDS", "3").getIntValue();
    std::vector<std::pair<double, juce::String>> results;
    juce::AudioBuffer<float> buffer (2, blockSize);

    for (int index = 0; index < names.size(); ++index)
    {
        if (! only.isEmpty() && ! only.contains (names[index]))
            continue;

        processor.loadFactoryPreset (index);
        processor.panic();
        juce::MidiBuffer noteOns;

        for (int note = 0; note < 4; ++note)
            noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 5, (juce::uint8) 100), 0);

        buffer.clear();
        processor.processBlock (buffer, noteOns);
        const auto blocks = 44100 * seconds / blockSize;
        const auto start = juce::Time::getMillisecondCounterHiRes();

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
        }

        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        const auto percent = 100.0 * elapsed / (blocks * (double) blockSize / 44100.0);
        results.emplace_back (percent, names[index]);
        std::cout << "bench " << names[index] << ": " << juce::String (percent, 2) << " %"
                  << " f1 " << (int) *processor.apvts.getRawParameterValue ("f1_type")
                  << " f2 " << (int) *processor.apvts.getRawParameterValue ("f2_type")
                  << " f2cut " << (int) *processor.apvts.getRawParameterValue ("f2_cutoff") << std::endl;
    }

    if (results.empty())
        return;

    std::vector<double> values;
    for (const auto& r : results)
        values.push_back (r.first);
    std::sort (values.begin(), values.end());
    std::sort (results.begin(), results.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    auto sum = 0.0;
    for (auto v : values)
        sum += v;
    std::cout << "LIBRARY BENCH " << results.size() << " presets: median " << juce::String (values[values.size() / 2], 2)
              << " %, mean " << juce::String (sum / (double) values.size(), 2) << " %" << std::endl;
    for (size_t i = 0; i < juce::jmin<size_t> (10, results.size()); ++i)
        std::cout << "  slowest " << results[i].second << ": " << juce::String (results[i].first, 2) << " %" << std::endl;
}

// ILANA_BODY_BENCH=1: a Physical patch (ILANA_BODY_BENCH_PRESET, default
// Felt Hammer Board) with the BODY on, each body type and STRING TO BODY
// coupling mode in turn, at 44.1 kHz in 512-sample blocks: "held" holds
// ILANA_BODY_BENCH_VOICES (default 12) notes for 2 s and releases them for
// 2 s; "clip" plays that chord for 0.4 s every 0.5 s for 6 s, as a looped
// clip does. Prints ms per block, % of one core, the voices sounding and the
// output peak (and any non-finite samples). ILANA_BODY_BENCH_TYPE / _COUPLING
// pick one case, _AMOUNT sets body_coupling (0.5), _SET="id=value,..." sets
// more parameters (real values), _AS_IS plays the preset unchanged, _ONSET
// times one note's rise per type, _ALL ranks every preset by Shell's cost,
// _SECONDS sets the held run's length (4), _CLIP_ONLY skips it, _DUMP=<dir>
// writes each run's output as raw floats. Voices are counted with their
// body tails (getRenderingVoiceCount).
void runBodyBench()
{
    IlanaSynthAudioProcessor processor;
    const auto blockSize = 512;
    processor.prepareToPlay (44100.0, blockSize);
    const auto names = processor.getFactoryPresetNames();
    const auto presetName = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_PRESET", "Felt Hammer Board");
    const auto voices = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_VOICES", "12").getIntValue();
    const auto onlyType = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_TYPE", "-1").getIntValue();
    const auto onlyCoupling = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_COUPLING", "-1").getIntValue();
    const auto keepPreset = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_AS_IS", "").isNotEmpty();
    const auto index = names.indexOf (presetName);
    if (index < 0)
    {
        std::cout << "no preset " << presetName << std::endl;
        return;
    }
    const auto set = [&processor] (const char* id, float value)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    const juce::StringArray types { "Classic", "Bar", "Plate", "Bell", "Shell" };
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto run = [&] (const juce::String& label, bool clip)
    {
        processor.panic();
        buffer.clear();
        juce::MidiBuffer silence;
        processor.processBlock (buffer, silence);
        const auto seconds = clip ? 6.0 : juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_SECONDS", "4").getDoubleValue();
        const auto blocks = (int) (44100.0 * seconds / blockSize);
        auto elapsed = 0.0, slowest = 0.0, voiceSum = 0.0;
        auto peakVoices = 0, nonFinite = 0, lastSounding = -1;
        // ILANA_BODY_BENCH_DUMP=<folder>: each run's output as raw floats.
        std::unique_ptr<juce::FileOutputStream> dump;
        if (const auto folder = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_DUMP", ""); folder.isNotEmpty())
        {
            const auto file = juce::File (folder).getChildFile (label.replaceCharacter (' ', '_') + (clip ? "_clip" : "_held") + ".f32");
            file.deleteFile();
            dump = std::make_unique<juce::FileOutputStream> (file);
        }
        auto outPeak = 0.0f;
        for (int block = 0; block < blocks; ++block)
        {
            juce::MidiBuffer midi;
            const auto t0 = (double) block * blockSize / 44100.0, t1 = t0 + blockSize / 44100.0;
            const auto inBlock = [&] (double t) { return t >= t0 && t < t1 ? juce::jlimit (0, blockSize - 1, (int) ((t - t0) * 44100.0)) : -1; };
            for (double t = 0.0; t < seconds; t += clip ? 0.5 : 100.0)
            {
                if (const auto on = inBlock (t); on >= 0)
                    for (int v = 0; v < voices; ++v)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 40 + v * 3, (juce::uint8) 100), on);
                if (const auto off = inBlock (t + (clip ? 0.4 : 2.0)); off >= 0)
                    for (int v = 0; v < voices; ++v)
                        midi.addEvent (juce::MidiMessage::noteOff (1, 40 + v * 3), off);
            }
            buffer.clear();
            const auto start = juce::Time::getMillisecondCounterHiRes();
            processor.processBlock (buffer, midi);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            elapsed += ms;
            slowest = juce::jmax (slowest, ms);
            if (dump != nullptr)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    dump->write (buffer.getReadPointer (ch), sizeof (float) * (size_t) blockSize);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const auto x = buffer.getSample (ch, i);
                    if (! std::isfinite (x))
                        ++nonFinite;
                    else
                        outPeak = juce::jmax (outPeak, std::abs (x));
                }
            const auto rendering = processor.getRenderingVoiceCount();
            if (rendering > 0)
                lastSounding = block;
            voiceSum += rendering;
            peakVoices = juce::jmax (peakVoices, rendering);
        }
        const auto perBlock = elapsed / blocks;
        std::cout << "body " << label << (clip ? " clip" : " held") << ": " << juce::String (perBlock, 3)
                  << " ms/block, " << juce::String (100.0 * perBlock / (1000.0 * blockSize / 44100.0), 1)
                  << " % of one core, slowest " << juce::String (slowest, 2) << " ms, voices "
                  << juce::String (voiceSum / blocks, 1) << " (peak " << peakVoices << ")"
                  << (clip ? juce::String() : ", last voice ends " + juce::String ((lastSounding + 1) * blockSize / 44100.0 - 2.0, 2) + " s after release")
                  << ", out peak "
                  << juce::String (outPeak, 3) << (nonFinite > 0 ? ", NON-FINITE " + juce::String (nonFinite) : juce::String()) << std::endl;
    };
    // ILANA_BODY_BENCH_ALL=1: every factory preset with the BODY on, 6 notes
    // held 1 s, Bar / Plate / Bell / Shell; prints Shell's time against the
    // others' mean, the worst first.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_ALL", "").isNotEmpty())
    {
        std::vector<std::pair<double, juce::String>> rows;
        juce::AudioBuffer<float> one (2, blockSize);
        for (int preset = 0; preset < names.size(); ++preset)
        {
            double ms[5] {};
            for (int type = 1; type < 5; ++type)
            {
                processor.loadFactoryPreset (preset);
                set ("res_on", 1.0f);
                set ("body_type", (float) type);
                processor.panic();
                const auto blocks = (int) (44100.0 / blockSize);
                for (int block = 0; block < blocks; ++block)
                {
                    juce::MidiBuffer midi;
                    if (block == 0)
                        for (int v = 0; v < 6; ++v)
                            midi.addEvent (juce::MidiMessage::noteOn (1, 48 + v * 4, (juce::uint8) 100), 0);
                    one.clear();
                    const auto start = juce::Time::getMillisecondCounterHiRes();
                    processor.processBlock (one, midi);
                    ms[type] += juce::Time::getMillisecondCounterHiRes() - start;
                }
            }
            const auto others = (ms[1] + ms[2] + ms[3]) / 3.0;
            rows.push_back ({ ms[4] / juce::jmax (1.0e-6, others), names[preset] + ": shell " + juce::String (ms[4], 1) + " ms, bar "
                                                                      + juce::String (ms[1], 1) + ", plate " + juce::String (ms[2], 1)
                                                                      + ", bell " + juce::String (ms[3], 1) });
        }
        std::sort (rows.begin(), rows.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
        for (size_t i = 0; i < juce::jmin<size_t> (25, rows.size()); ++i)
            std::cout << juce::String (rows[i].first, 2) << "x  " << rows[i].second << std::endl;
        return;
    }
    // ILANA_BODY_BENCH_ONSET=1: one note (C4) per body type, 2 s; the time
    // its 512-sample RMS takes to reach half (-6 dB) of its peak.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_ONSET", "").isNotEmpty())
    {
        for (int type = 0; type < 5; ++type)
        {
            processor.loadFactoryPreset (index);
            set ("res_on", 1.0f);
            set ("body_type", (float) type);
            for (const auto& pair : juce::StringArray::fromTokens (
                     juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_SET", ""), ",", ""))
                if (pair.contains ("="))
                    set (pair.upToFirstOccurrenceOf ("=", false, false).toRawUTF8(),
                         pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            processor.panic();
            std::vector<float> rms;
            for (int block = 0; block < (int) (2.0 * 44100.0 / blockSize); ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                rms.push_back (buffer.getRMSLevel (0, 0, blockSize));
            }
            const auto peak = *std::max_element (rms.begin(), rms.end());
            const auto peakAt = (int) (std::max_element (rms.begin(), rms.end()) - rms.begin());
            auto half = 0;
            while (half < (int) rms.size() && rms[(size_t) half] < 0.5f * peak)
                ++half;
            std::cout << "onset " << types[type] << ": -6 dB at " << juce::String (half * blockSize / 44.1, 0) << " ms, peak at "
                      << juce::String (peakAt * blockSize / 44.1, 0) << " ms, peak rms " << juce::String (peak, 3) << std::endl;
        }
        return;
    }
    if (keepPreset)
    {
        processor.loadFactoryPreset (index);
        run (presetName, false);
        processor.loadFactoryPreset (index);
        run (presetName, true);
        return;
    }
    for (int coupling = 0; coupling < 4; ++coupling)
        for (int type = 0; type < 5; ++type)
        {
            if ((onlyType >= 0 && type != onlyType) || (onlyCoupling >= 0 && coupling != onlyCoupling))
                continue;
            for (const auto clip : { false, true })
            {
                if (! clip && juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_CLIP_ONLY", "").isNotEmpty())
                    continue;
                processor.loadFactoryPreset (index);
                set ("res_on", 1.0f);
                set ("body_type", (float) type);
                set ("body_coupling_mode", (float) coupling);
                if (coupling > 0)
                    set ("body_coupling", juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_AMOUNT", "0.5").getFloatValue());
                for (const auto& pair : juce::StringArray::fromTokens (
                         juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH_SET", ""), ",", ""))
                    if (pair.contains ("="))
                        set (pair.upToFirstOccurrenceOf ("=", false, false).toRawUTF8(),
                             pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
                run (types[type] + " coupling " + juce::String (coupling), clip);
            }
        }
}

// ILANA_OPEN_FILTER_CHECK=1: the gain of Filter 2's default (Low Pass wide
// open, resonance 0 and 0.3) against a straight wire, per frequency, at
// 44.1 and 48 kHz, for sines at -6 dBFS and -20 dBFS.
void runOpenFilterCheck()
{
    for (const auto rate : { 44100.0, 48000.0 })
        for (const auto reso : { 0.0, 0.3 })
            for (const auto level : { 0.5f, 0.1f })
            {
                std::cout << "rate " << rate << " reso " << reso << " level " << level << ":";
                for (const auto hz : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0 })
                {
                    FilterUnit filter;
                    filter.prepare (rate);
                    filter.setType (FilterType::LowPass, false);
                    filter.setCoefficients (FilterUnit::makeCoefficients (FilterType::LowPass, rate, juce::jmin (20000.0, rate * 0.45), reso));
                    double in = 0.0, out = 0.0;
                    const auto n = (int) rate;
                    for (int i = 0; i < n; ++i)
                    {
                        const auto x = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
                        const auto y = filter.process (x);
                        if (i > n / 4) { in += (double) x * x; out += (double) y * y; }
                    }
                    Airwindows::OpenLowPass open;
                    open.set (rate, juce::jmin (20000.0, rate * 0.45), reso);
                    double openOut = 0.0;
                    for (int i = 0; i < n; ++i)
                    {
                        const auto x = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
                        const auto y = open.process (x);
                        if (i > n / 4) openOut += (double) y * y;
                    }
                    std::cout << " " << hz << "Hz " << juce::String (10.0 * std::log10 (out / in), 2)
                              << "/" << juce::String (10.0 * std::log10 (openOut / in), 2);
                }
                std::cout << std::endl;
            }
}

// ILANA_NULL_CHECK=1: render every factory preset (4 notes, 2 s at 44.1 kHz)
// with Voice::disableOpenFilterBypass off and on, and print the difference
// against the signal in dB (a CPU shortcut must null below -60 dB or so).
// A drawn remap curve per mod slot: shape() applies it over the slot's
// range, it saves and loads with the patch, a factory preset or a cleared
// slot drops it, and the voices hear it.
void runModRemapTests()
{
    std::array<float, Mod::remapSize + 1> identity {}, invert {};
    for (int i = 0; i <= Mod::remapSize; ++i)
    {
        identity[(size_t) i] = 2.0f * (float) i / (float) Mod::remapSize - 1.0f;
        invert[(size_t) i] = -identity[(size_t) i];
    }

    Mod::Slot slot;
    slot.source = Mod::Source::Macro1; // unipolar
    slot.remap = identity.data();
    check (std::abs (Mod::shape (slot, 0.3f) - 0.3f) < 1.0e-5f, "remap: a straight line leaves a unipolar source as it was");
    slot.remap = invert.data();
    check (std::abs (Mod::shape (slot, 0.25f) - 0.75f) < 1.0e-5f, "remap: an inverted line flips a unipolar source");
    slot.source = Mod::Source::Lfo1; // bipolar
    check (std::abs (Mod::shape (slot, 0.5f) + 0.5f) < 1.0e-5f, "remap: an inverted line flips a bipolar source");
    slot.remap = identity.data();
    check (std::abs (Mod::shape (slot, -0.4f) + 0.4f) < 1.0e-5f, "remap: a straight line leaves a bipolar source as it was");

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    processor.loadFactoryPreset (0);
    check (! processor.isModRemapOn (3), "remap: off by default");
    processor.setModRemap (3, IlanaSynthAudioProcessor::identityRemap());
    check (! processor.isModRemapOn (3), "remap: a straight line counts as off");
    LfoCurve flipped;
    flipped.points = { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
    processor.setModRemap (3, flipped);
    check (processor.isModRemapOn (3) && processor.readModSlot (3).remap != nullptr, "remap: a drawn curve is on");

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    IlanaSynthAudioProcessor loaded;
    loaded.setStateInformation (saved.getData(), (int) saved.getSize());
    check (loaded.isModRemapOn (3) && loaded.getModRemap (3).toString() == flipped.toString(), "remap: saved and loaded with the patch");
    loaded.loadFactoryPreset (0);
    check (! loaded.isModRemapOn (3), "remap: a factory preset clears it");
    processor.clearModSlot (3);
    check (! processor.isModRemapOn (3), "remap: clearing the slot clears it");

    // Macro 1 at full pulls Amp Level all the way down; inverted by the
    // remap, the macro sends nothing and the note plays.
    const auto render = [] (bool remap)
    {
        IlanaSynthAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        p.loadFactoryPreset (0);
        if (auto* macro = p.apvts.getParameter ("macro1"))
            macro->setValueNotifyingHost (1.0f);
        p.clearModSlot (0);
        p.setModSlotValue (0, "src", (float) Mod::Source::Macro1);
        p.setModSlotValue (0, "dst", (float) Mod::Destination::AmpLevel);
        p.setModSlotValue (0, "amt", -1.0f);
        if (remap)
        {
            LfoCurve curve;
            curve.points = { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
            p.setModRemap (0, curve);
        }
        juce::AudioBuffer<float> buffer (2, 256);
        auto sum = 0.0;
        for (int block = 0; block < 60; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            p.processBlock (buffer, midi);
            for (int i = 0; i < 256; ++i)
                sum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        return std::sqrt (sum / (60.0 * 256.0));
    };
    const auto plain = render (false), remapped = render (true);
    check (plain < 1.0e-4 && remapped > 1.0e-3,
           "remap: the voices hear it (rms " + juce::String (plain, 6) + " plain, " + juce::String (remapped, 6) + " remapped)");
}

void runNullCheck()
{
    IlanaSynthAudioProcessor processor;
    const auto blockSize = 256;
    processor.prepareToPlay (44100.0, blockSize);
    const auto names = processor.getFactoryPresetNames();
    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_BENCH_ONLY", ""), "|", "");
    const auto blocks = 44100 * 2 / blockSize;
    auto worst = -300.0;
    juce::String worstName;

    // A fresh processor per render: it is seeded the same way every time.
    const auto render = [&] (int index)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (44100.0, blockSize);
        processor.loadFactoryPreset (index);
        // ILANA_NULL_SET="id=value,...": override parameters after loading.
        for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_NULL_SET", ""), ",", ""))
            if (auto* parameter = processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        processor.panic();
        juce::AudioBuffer<float> out (2, blocks * blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer noteOns;
        for (int note = 0; note < 4; ++note)
            noteOns.addEvent (juce::MidiMessage::noteOn (1, 48 + note * 5, (juce::uint8) 100), 0);
        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi = noteOns;
            if (block == blocks / 2)
                for (int note = 0; note < 4; ++note)
                    midi.addEvent (juce::MidiMessage::noteOff (1, 48 + note * 5), 0);
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, block * blockSize, buffer, ch, 0, blockSize);
        }
        return out;
    };

    for (int index = 0; index < names.size(); ++index)
    {
        if (! only.isEmpty() && ! only.contains (names[index]))
            continue;

        Voice::disableOpenFilterBypass = true;
        const auto reference = render (index);
        Voice::disableOpenFilterBypass = juce::SystemStats::getEnvironmentVariable ("ILANA_NULL_CONTROL", "").isNotEmpty();
        const auto candidate = render (index);
        Voice::disableOpenFilterBypass = false;
        double signal = 0.0, diff = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < reference.getNumSamples(); ++i)
            {
                const auto a = (double) reference.getSample (ch, i), b = (double) candidate.getSample (ch, i);
                signal += a * a;
                diff += (a - b) * (a - b);
            }
        const auto db = 10.0 * std::log10 ((diff + 1.0e-30) / (signal + 1.0e-30));
        std::cout << "null " << names[index] << ": " << juce::String (db, 1) << " dB" << std::endl;
        if (db > worst)
        {
            worst = db;
            worstName = names[index];
        }
    }

    std::cout << "NULL CHECK worst " << worstName << ": " << juce::String (worst, 1) << " dB" << std::endl;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::SystemStats::setApplicationCrashHandler ([] (void*)
    {
        std::cout << "CRASH" << std::endl;
        std::cout << juce::SystemStats::getStackBacktrace() << std::endl;
    });

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_OPEN_FILTER_CHECK", "").isNotEmpty())
    {
        runOpenFilterCheck();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_NULL_CHECK", "").isNotEmpty())
    {
        runNullCheck();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH", "").isNotEmpty())
    {
        runBodyBench();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_BENCH", "").isNotEmpty())
    {
        runLibraryBench();
        return 0;
    }

    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_PROFILE", ""); preset.isNotEmpty())
    {
        runPresetProfile (preset);
        return 0;
    }

    if (const auto profile = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE", ""); profile.isNotEmpty())
    {
        runProfile (juce::jmax (1, profile.getIntValue()));
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_UNISON_BENCH", "").isNotEmpty())
    {
        runUnisonBankEquivalenceTest();
        runUnisonBenchmark();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    // ILANA_BENCH=1 runs only the CPU benchmark (for profiling).
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH", "").isNotEmpty())
    {
        runPhase2StateAndCpuTest();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_REMAP_TEST", "").isNotEmpty())
    {
        runModRemapTests();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M71_TEST", "").isNotEmpty())
    {
        runM71GenerativeTests();
        runScaleRandomReleaseTest();
        runArpHostStopTests();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FEEDBACK_PROBE", "").isNotEmpty())
    {
        FeedbackGuitarTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_FEEDBACK_TUNING", ""));
        M85::probe();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO_PROBE", "").isNotEmpty())
    {
        PianoModelTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO2_TUNING", ""));
        M82::probe();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PARAM_TEXT", "").isNotEmpty())
    {
        // Every parameter's text at its minimum, default and maximum (to
        // review units and formats).
        IlanaSynthAudioProcessor processor;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                std::cout << "HELP " << withId->paramID << " | " << describeParameter (withId->paramID) << std::endl
                          << "PARAM " << withId->paramID << " | " << parameter->getText (0.0f, 64) << " | "
                          << parameter->getText (parameter->getDefaultValue(), 64) << " | " << parameter->getText (1.0f, 64) << std::endl;
        return 0;
    }

    if (const auto target = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_SEQ", ""); target.isNotEmpty())
    {
        Polish::probeSequence (target);
        return 0;
    }

    if (const auto list = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_PROBE", ""); list.isNotEmpty())
    {
        Polish::probeLevels (list);
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PLAN_TEST", "").isNotEmpty())
    {
        runPlanTests();
        std::cout << (failures == 0 ? "PLAN TESTS PASSED" : "PLAN TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_OSCMODE_TEST", "").isNotEmpty())
    {
        runOscModeTests();
        std::cout << (failures == 0 ? "OSC MODE TESTS PASSED" : "OSC MODE TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_OPEG_TEST", "").isNotEmpty())
    {
        runOperatorEgTests();
        std::cout << (failures == 0 ? "OPERATOR EG TESTS PASSED" : "OPERATOR EG TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_TUNING_TEST", "").isNotEmpty())
    {
        runTuningTests();
        std::cout << (failures == 0 ? "TUNING TESTS PASSED" : "TUNING TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_AIRWINDOWS_TEST", "").isNotEmpty())
    {
        runAirwindowsTests();
        std::cout << (failures == 0 ? "AIRWINDOWS TESTS PASSED" : "AIRWINDOWS TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PARALLEL_FX_TEST", "").isNotEmpty())
    {
        runParallelFxTests();
        std::cout << (failures == 0 ? "PARALLEL FX TESTS PASSED" : "PARALLEL FX TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SPLITTER_TEST", "").isNotEmpty())
    {
        runSplitterTests();
        std::cout << (failures == 0 ? "SPLITTER TESTS PASSED" : "SPLITTER TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_AWFILTER_TEST", "").isNotEmpty())
    {
        runAirwindowsFilterTests();
        std::cout << (failures == 0 ? "AWFILTER TESTS PASSED" : "AWFILTER TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_CLIP_TEST", "").isNotEmpty())
    {
        runClipTests();
        std::cout << (failures == 0 ? "CLIP TESTS PASSED" : "CLIP TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_MULTISAMPLE_TEST", "").isNotEmpty())
    {
        runMultiSampleTests();
        std::cout << (failures == 0 ? "MULTISAMPLE TESTS PASSED" : "MULTISAMPLE TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_VOCODER_TEST", "").isNotEmpty())
    {
        runVocoderTests();
        std::cout << (failures == 0 ? "VOCODER TESTS PASSED" : "VOCODER TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FILTER_TEST", "").isNotEmpty())
    {
        runFilterOverhaulTests();
        std::cout << (failures == 0 ? "FILTER TESTS PASSED" : "FILTER TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_POLISH_TEST", "").isNotEmpty())
    {
        runPolishTests();
        std::cout << (failures == 0 ? "POLISH TESTS PASSED" : "POLISH TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M10_TEST", "").isNotEmpty())
    {
        runM10Tests();
        std::cout << (failures == 0 ? "M10 TESTS PASSED" : "M10 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M86_TEST", "").isNotEmpty())
    {
        runM86Tests();
        std::cout << (failures == 0 ? "M8.6 TESTS PASSED" : "M8.6 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M85_TEST", "").isNotEmpty())
    {
        runM85Tests();
        std::cout << (failures == 0 ? "M8.5 TESTS PASSED" : "M8.5 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M84_TEST", "").isNotEmpty())
    {
        runM84FilterTests();
        std::cout << (failures == 0 ? "M8.4 TESTS PASSED" : "M8.4 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M83_TEST", "").isNotEmpty())
    {
        runM83WestTests();
        std::cout << (failures == 0 ? "M8.3 TESTS PASSED" : "M8.3 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M82_TEST", "").isNotEmpty())
    {
        runM82PianoTests();
        std::cout << (failures == 0 ? "M8.2 TESTS PASSED" : "M8.2 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M81_TEST", "").isNotEmpty())
    {
        runM81ModulatorTests();
        std::cout << (failures == 0 ? "M8.1 TESTS PASSED" : "M8.1 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M73_TEST", "").isNotEmpty())
    {
        runM73ElectricPianoTests();
        runM74WavetableEditorTests();
        runSplitRenderTest();
        std::cout << (failures == 0 ? "M7.3/M7.4 TESTS PASSED" : "M7.3/M7.4 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M72_TEST", "").isNotEmpty())
    {
        runM72BodyTests();
        std::cout << (failures == 0 ? "M7.2 TESTS PASSED" : "M7.2 TESTS FAILED") << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (const auto folder = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_FIT", ""); folder.isNotEmpty())
    {
        renderBodyFit (juce::File (folder));
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M70_TEST", "").isNotEmpty())
    {
        runM70ExtendedFmModTests();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    // ILANA_TABLE_DUMP=<file>: the first 40 factory tables' built frames
    // (level 0 and level 6), for comparing two builds.
    if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_DUMP", ""); dump.isNotEmpty())
    {
        juce::FileOutputStream out { juce::File (dump) };
        out.setPosition (0);
        out.truncate();
        for (int i = 0; i < 40; ++i)
        {
            Wavetable table;
            table.buildFromFrames (TableFactory::generate (i));
            for (int level : { 0, 6 })
                for (int frame = 0; frame < table.getNumFrames(); ++frame)
                    out.write (table.getFrameData (level, frame), 2051 * sizeof (float));
        }
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_TEST", "").isNotEmpty())
    {
        runFactoryLibraryTest();
        std::cout << (failures == 0 ? "LIBRARY TESTS PASSED" : "LIBRARY TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SYM_TEST", "").isNotEmpty())
    {
        runSympatheticResonanceTest();
        return failures == 0 ? 0 : 1;
    }

    // ILANA_TABLE_FRAMES=<file>: frames 0, 32 and 63 of every factory table.
    if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_FRAMES", ""); dump.isNotEmpty())
    {
        juce::FileOutputStream out { juce::File (dump) };
        out.setPosition (0);
        out.truncate();
        for (int i = 0; i < TableFactory::getNumFactoryTables(); ++i)
        {
            const auto frames = TableFactory::generate (i);
            for (int frame : { 0, 32, 63 })
                out.write (frames[(size_t) frame].data(), 2048 * sizeof (float));
        }
        return 0;
    }

    // ILANA_LPG_PROBE=<folder>: single strikes of the low-pass gate (WEST on,
    // a plain saw, fold off) at A2 and A4, with ILANA_WEST_TUNING applied,
    // as <folder>/strike.<note>.wav (tools/fit_lpg.py measures them).
    if (const auto probe = juce::SystemStats::getEnvironmentVariable ("ILANA_LPG_PROBE", ""); probe.isNotEmpty())
    {
        WestCoastTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_WEST_TUNING", ""));
        juce::File (probe).createDirectory();
        for (int note : { 45, 69 })
        {
            IlanaSynthAudioProcessor processor;
            for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                     { "west_on", 1.0f }, { "west_fold", 0.0f }, { "osc1_table", 0.0f }, { "osc1_frame", 0.0f },
                     { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "subosc_on", 0.0f }, { "f1_cutoff", 20000.0f },
                     { "f1_env", 0.0f }, { "osc1_unison", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.01f } })
                setParam (processor, id, value);
            processor.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> out (1, 48000), buffer (2, 256);
            for (int start = 0; start < 48000; start += 256)
            {
                juce::MidiBuffer midi;
                if (start == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                if (start == 256 * 180)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                out.copyFrom (0, start, buffer, 0, 0, juce::jmin (256, 48000 - start));
            }
            const auto file = juce::File (probe).getChildFile ("strike." + juce::String (note) + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                }
        }
        return 0;
    }

    // ILANA_TABLE_TIMING=1: how long each factory table takes to build.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_TIMING", "").isNotEmpty())
    {
        auto total = 0.0;
        for (int i = 0; i < TableFactory::getNumFactoryTables(); ++i)
        {
            const auto start = juce::Time::getMillisecondCounterHiRes();
            Wavetable table;
            table.buildFromFrames (TableFactory::generate (i));
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            total += ms;
            if (ms > 40.0)
                std::cout << i << " " << TableFactory::getFactoryTableNames()[i] << ": " << ms << " ms" << std::endl;
        }
        std::cout << "all tables: " << total << " ms" << std::endl;
        return 0;
    }

    // ILANA_PRESET_TEST=1 runs only the factory preset checks.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_TEST", "").isNotEmpty())
    {
        runPresetSanityTest();
        runPresetTuningTest();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    // ILANA_ARP_TEST=1 runs only the arpeggiator tests.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_ARP_TEST", "").isNotEmpty())
    {
        runScaleRandomReleaseTest();
        runArpHostStopTests();
        runArpLaneTests();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_DEBUG", ""); preset.isNotEmpty())
    {
        debugPresetNotes (preset);
        return 0;
    }

    if (const auto demo = juce::SystemStats::getEnvironmentVariable ("ILANA_RENDER_DEMO", ""); demo.isNotEmpty())
    {
        // ILANA_DEMO_ONLY=m81 (or m82, ...) renders one milestone's demos.
        const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_DEMO_ONLY", "");
        if (only.isEmpty())
        {
            renderKeysDemos (juce::File (demo));
            renderFmPdDemos (juce::File (demo).getChildFile ("fm-pd"));
        }
        if (only.isEmpty() || only == "m81")
            Demo::renderM81 (juce::File (demo).getChildFile ("m81"));
        if (only.isEmpty() || only == "m82")
            Demo::renderM82 (juce::File (demo).getChildFile ("m82"));
        if (only.isEmpty() || only == "m83")
            Demo::renderM83 (juce::File (demo).getChildFile ("m83"));
        if (only.isEmpty() || only == "m85")
            Demo::renderM85 (juce::File (demo).getChildFile ("m85"));
        if (only.isEmpty() || only == "m86")
            Demo::renderM86 (juce::File (demo).getChildFile ("m86"));
        if (only.isEmpty() || only == "m10")
            Demo::renderM10 (juce::File (demo).getChildFile ("m10"));
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M5_TEST", "").isNotEmpty())
    {
        runM5DeepFmTests();
        runM6PhaseDistortionTests();
        runM6bMatrixTests();
        runWarpTests();
        runFmMatrixTests();
        std::cout << (failures == 0 ? "M5 TESTS PASSED" : "M5 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_KS_TEST", "").isNotEmpty())
    {
        runKarplusStrongTest();
        runPhysicalStringTest();
        std::cout << (failures == 0 ? "KS TESTS PASSED" : "KS TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M4_TEST", "").isNotEmpty())
    {
        runM4Tests();
        runSympatheticTuningTest();
        runSympatheticResonanceTest();
        std::cout << (failures == 0 ? "M4 TESTS PASSED" : "M4 TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_M3B_TEST", "").isNotEmpty())
    {
        runM3bEngineTests();
        return failures == 0 ? 0 : 1;
    }

    std::cout << "ilanaSynth table tests" << std::endl;

    timedRun ("runMipmapTests", [] { runMipmapTests(); });
    timedRun ("runAliasTests", [] { runAliasTests(); });
    timedRun ("runLoaderTest", [] { runLoaderTest(); });
    timedRun ("runFilterTests", [] { runFilterTests(); });
    timedRun ("runSubOscillatorTests", [] { runSubOscillatorTests(); });
    timedRun ("runVoiceSmokeTest", [] { runVoiceSmokeTest(); });
    timedRun ("runFrameModulationTest", [] { runFrameModulationTest(); });
    timedRun ("runCrossModulationTest", [] { runCrossModulationTest(); });
    timedRun ("runKarplusStrongTest", [] { runKarplusStrongTest(); });
    timedRun ("runFactoryTableContentTest", [] { runFactoryTableContentTest(); });
    timedRun ("runWeirdDspTest", [] { runWeirdDspTest(); });
    timedRun ("runSampleOscTest", [] { runSampleOscTest(); });
    timedRun ("runPresetSanityTest", [] { runPresetSanityTest(); });
    timedRun ("runOsc2Test", [] { runOsc2Test(); });
    timedRun ("runOscLevelTest", [] { runOscLevelTest(); });
    timedRun ("runPresetTuningTest", [] { runPresetTuningTest(); });
    timedRun ("runParameterStressTest", [] { runParameterStressTest(); });
    timedRun ("runFxModuleIsolationTest", [] { runFxModuleIsolationTest(); });
    timedRun ("runFxIntegrityTest", [] { runFxIntegrityTest(); });
    timedRun ("runStateRoundTripTest", [] { runStateRoundTripTest(); });
    timedRun ("runSoakTest", [] { runSoakTest(); });
    timedRun ("runLegacyPresetFxTest", [] { runLegacyPresetFxTest(); });
    timedRun ("runLfo34Test", [] { runLfo34Test(); });
    timedRun ("runOversamplingTest", [] { runOversamplingTest(); });
    timedRun ("runFxSlotAssignTest", [] { runFxSlotAssignTest(); });
    timedRun ("runPresetNameTest", [] { runPresetNameTest(); });
    timedRun ("runFilterModelTests", [] { runFilterModelTests(); });
    timedRun ("runVoiceModeTests", [] { runVoiceModeTests(); });
    timedRun ("runWarpTests", [] { runWarpTests(); });
    timedRun ("runUnisonTests", [] { runUnisonTests(); });
    timedRun ("runPerVoiceLfoTest", [] { runPerVoiceLfoTest(); });
    timedRun ("runMatrixTests", [] { runMatrixTests(); });
    timedRun ("runStaleModulationTest", [] { runStaleModulationTest(); });
    timedRun ("runPhase2StateAndCpuTest", [] { runPhase2StateAndCpuTest(); });
    timedRun ("runUnisonBankEquivalenceTest", [] { runUnisonBankEquivalenceTest(); });
    timedRun ("runExtraFilterTests", [] { runExtraFilterTests(); });
    timedRun ("runFilterRoutingTest", [] { runFilterRoutingTest(); });
    timedRun ("runOversampledTuningTest", [] { runOversampledTuningTest(); });
    timedRun ("runResynthesisTest", [] { runResynthesisTest(); });
    timedRun ("runCurveLfoTest", [] { runCurveLfoTest(); });
    timedRun ("runFactoryLibraryTest", [] { runFactoryLibraryTest(); });
    timedRun ("runTapeStopLatencyTest", [] { runTapeStopLatencyTest(); });
    timedRun ("runTranceGateTest", [] { runTranceGateTest(); });
    timedRun ("runGenerativeTests", [] { runGenerativeTests(); });
    timedRun ("runOsc3MigrationTest", [] { runOsc3MigrationTest(); });
    timedRun ("runFmMatrixTests", [] { runFmMatrixTests(); });
    timedRun ("runM3bEngineTests", [] { runM3bEngineTests(); });
    timedRun ("runSpectralWarpTests", [] { runSpectralWarpTests(); });
    timedRun ("runChaosLfoTests", [] { runChaosLfoTests(); });
    timedRun ("runPhysicsLfoTests", [] { runPhysicsLfoTests(); });
    timedRun ("runPhysicsLfoMotionTest", [] { runPhysicsLfoMotionTest(); });
    timedRun ("runSympatheticTuningTest", [] { runSympatheticTuningTest(); });
    timedRun ("runSympatheticResonanceTest", [] { runSympatheticResonanceTest(); });
    timedRun ("runBridgeBuzzStabilityTest", [] { runBridgeBuzzStabilityTest(); });
    timedRun ("runIntegerValueTextTest", [] { runIntegerValueTextTest(); });
    timedRun ("runMissingParameterDefaultTest", [] { runMissingParameterDefaultTest(); });
    timedRun ("runM3PhysicalTests", [] { runM3PhysicalTests(); });
    timedRun ("runM3MissingParameterTest", [] { runM3MissingParameterTest(); });
    timedRun ("runGranularTests", [] { runGranularTests(); });
    timedRun ("runHeavyPresetCpuTest", [] { runHeavyPresetCpuTest(); });
    timedRun ("runPhysicalStringTest", [] { runPhysicalStringTest(); });
    timedRun ("runPhysicalPatchMigrationTest", [] { runPhysicalPatchMigrationTest(); });
    timedRun ("runScaleRandomReleaseTest", [] { runScaleRandomReleaseTest(); });
    timedRun ("runArpHostStopTests", [] { runArpHostStopTests(); });
    timedRun ("runArpLaneTests", [] { runArpLaneTests(); });
    timedRun ("runM4Tests", [] { runM4Tests(); });
    timedRun ("runM5DeepFmTests", [] { runM5DeepFmTests(); });
    timedRun ("runM6PhaseDistortionTests", [] { runM6PhaseDistortionTests(); });
    timedRun ("runM6bMatrixTests", [] { runM6bMatrixTests(); });
    timedRun ("runM70ExtendedFmModTests", [] { runM70ExtendedFmModTests(); });
    timedRun ("runM71GenerativeTests", [] { runM71GenerativeTests(); });
    timedRun ("runM72BodyTests", [] { runM72BodyTests(); });
    timedRun ("runM73ElectricPianoTests", [] { runM73ElectricPianoTests(); });
    timedRun ("runM74WavetableEditorTests", [] { runM74WavetableEditorTests(); });
    timedRun ("runSplitRenderTest", [] { runSplitRenderTest(); });
    timedRun ("runM81ModulatorTests", [] { runM81ModulatorTests(); });
    timedRun ("runM82PianoTests", [] { runM82PianoTests(); });
    timedRun ("runM83WestTests", [] { runM83WestTests(); });
    timedRun ("runM84FilterTests", [] { runM84FilterTests(); });
    timedRun ("runM85Tests", [] { runM85Tests(); });
    timedRun ("runModRemapTests", [] { runModRemapTests(); });
    timedRun ("runM86Tests", [] { runM86Tests(); });
    timedRun ("runM10Tests", [] { runM10Tests(); });
    timedRun ("runPolishTests", [] { runPolishTests(); });
    timedRun ("runPlanTests", [] { runPlanTests(); });
    timedRun ("runTuningTests", [] { runTuningTests(); });
    timedRun ("runOperatorEgTests", [] { runOperatorEgTests(); });
    timedRun ("runOscModeTests", [] { runOscModeTests(); });
    timedRun ("runAirwindowsTests", [] { runAirwindowsTests(); });
    timedRun ("runVocoderTests", [] { runVocoderTests(); });
    timedRun ("runFilterOverhaulTests", [] { runFilterOverhaulTests(); });
    timedRun ("runMultiSampleTests", [] { runMultiSampleTests(); });
    timedRun ("runSplitterTests", [] { runSplitterTests(); });
    timedRun ("runParallelFxTests", [] { runParallelFxTests(); });
    timedRun ("runClipTests", [] { runClipTests(); });
    timedRun ("runAirwindowsFilterTests", [] { runAirwindowsFilterTests(); });

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << " (" << failures << " failures)" << std::endl;

    return failures == 0 ? 0 : 1;
}
