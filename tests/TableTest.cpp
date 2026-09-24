#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include <complex>
#include <iostream>
#include <set>
#include <vector>

#include "Presets.h"
#include "PluginProcessor.h"
#include "dsp/GranularPitchShift.h"
#include "dsp/FilterUnit.h"
#include "dsp/GranularSmear.h"
#include "dsp/LfoCurve.h"
#include "dsp/Modulation.h"
#include "dsp/PolyBlepOsc.h"
#include "dsp/SpectralFreeze.h"
#include "dsp/Svf.h"
#include "dsp/TableFactory.h"
#include "dsp/Voice.h"
#include "dsp/Wavetable.h"
#include "dsp/WavetableOscillator.h"

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

    Voice voice;
    voice.setCurrentPlaybackSampleRate (48000.0);

    VoiceParams p;
    p.osc1.table = &table1;
    p.osc1.frame = 0.5f;
    p.osc1.level = 0.8f;
    p.osc1.unison = 4;
    p.osc1.detuneCents = 20.0f;
    p.osc1.spread = 0.5f;

    p.osc2Enabled = true;
    p.osc2.table = &table2;
    p.osc2.frame = 0.3f;
    p.osc2.level = 0.5f;
    p.osc2.semitones = -12.0;
    p.osc2.unison = 2;

    p.sub.table = &table1;
    p.sub.level = 0.5f;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.osc1.table = &table;
        p.osc1.frame = frameParam;
        p.osc1.level = 1.0f;
        p.osc1.unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter1.resonance = 0.0f;
        p.filter1.drive = 1.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.filter2.resonance = 0.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        p.macros[0] = macroValue;
        p.lfo1 = lfoSource;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.osc1.table = &table1;
        p.osc1.frame = 0.4f;
        p.osc1.level = 0.8f;
        p.osc2Enabled = true;
        p.osc2.table = &table2;
        p.osc2.frame = 0.5f;
        p.osc2.level = 0.7f;
        p.osc2.semitones = 7.0;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.osc1.table = &table;
        p.osc1.stringMode = true;
        p.osc1.stringExcite = excite;
        p.osc1.stringDecay = decay;
        p.osc1.stringDamping = damping;
        p.osc1.stringSustain = sustain;
        p.osc1.level = 0.9f;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.osc1.table = &table;
        p.osc1.frame = 0.4f;
        p.osc1.level = 0.8f;
        p.osc1.unison = 1;
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
        p.osc1.unison = 4;
        p.osc1.detuneCents = 5.0f;
        p.osc1Chord = 4;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.osc1.sampleMode = true;
        p.osc1.sample = sample.get();
        p.osc1.sampleTuned = true;
        p.osc1.level = 0.9f;
        p.osc1.unison = 1;
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
        p.osc1.sampleTuned = false;
    });
    const auto untunedFrequency = fundamentalOf (untuned, sampleRate);
    check (untunedFinite && std::abs (untunedFrequency - 261.63) < 8.0,
           "untuned sample ignores note pitch (measured " + juce::String (untunedFrequency, 2) + ")");

    const auto [oneShot, oneShotFinite] = renderVoice (60, 16384, [] (VoiceParams&) {});
    check (oneShotFinite && peakOf (oneShot, 8000, 16384) < 0.001f,
           "one-shot stops after the sample ends (tail peak " + juce::String (peakOf (oneShot, 8000, 16384), 5) + ")");

    const auto [looped, loopedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.osc1.sampleLoop = true;
    });
    check (loopedFinite && peakOf (looped, 8000, 16384) > 0.2f,
           "looped sample keeps playing (tail peak " + juce::String (peakOf (looped, 8000, 16384), 4) + ")");

    const auto [reversed, reversedFinite] = renderVoice (60, 16384, [] (VoiceParams& p)
    {
        p.osc1.sampleReverse = true;
        p.osc1.sampleLoop = true;
    });
    check (reversedFinite && peakOf (reversed, 0, 4096) > 0.2f && peakOf (reversed, 8000, 16384) > 0.2f,
           "reverse loop plays and wraps (peak " + juce::String (peakOf (reversed, 8000, 16384), 4) + ")");

    const auto [region, regionFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.osc1.sampleStart = 0.5f;
        p.osc1.sampleEnd = 1.0f;
    });
    check (regionFinite && peakOf (region, 0, 4096) > 0.2f,
           "start/end region renders audio (peak " + juce::String (peakOf (region, 0, 4096), 4) + ")");

    const auto [faded, fadedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.osc1.sampleFadeIn = 0.5f;
        p.osc1.sampleFadeOut = 0.5f;
    });
    check (fadedFinite && std::abs (faded[0]) < 0.02f && peakOf (faded, 0, 4096) > 0.1f,
           "fade in/out attenuate the region edges (first " + juce::String (std::abs (faded[0]), 5) + ")");

    const auto [collapsed, collapsedFinite] = renderVoice (60, 8192, [] (VoiceParams& p)
    {
        p.osc1.sampleStart = 0.7f;
        p.osc1.sampleEnd = 0.2f;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.osc1.sampleMode = true;
        p.osc1.sample = nullptr;
        p.osc1.level = 0.9f;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 2048);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 2048);

        p.osc1.sample = sample.get();
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.osc1.sampleMode = true;
        p.osc1.sample = sample.get();
        p.osc1.level = 0.9f;
        p.osc1Enabled = false;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.osc1Enabled = true;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (sampleRate);

        VoiceParams p;
        p.osc1.sampleMode = true;
        p.osc1.sample = sample.get();
        p.osc1.level = 0.9f;
        p.osc1.unison = 1;
        p.filter1.cutoffHz = 20000.0f;
        p.filter2.cutoffHz = 20000.0f;
        p.ampEnv = { 0.001f, 1.0f, 1.0f, 0.1f };
        voice.setParams (p);
        voice.startNote (60, 1.0f, nullptr, 8192);

        juce::AudioBuffer<float> warmup (1, 1024);
        warmup.clear();
        voice.renderNextBlock (warmup, 0, 1024);

        p.osc1.unison = 4;
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
        Voice voice;
        voice.setCurrentPlaybackSampleRate (48000.0);

        VoiceParams p;
        p.osc1.table = &sineTable;
        p.osc1.level = 0.0f;
        p.osc2Enabled = true;
        p.osc2.table = &sineTable;
        p.osc2.level = 0.9f;
        p.osc2.semitones = (double) semitones;
        p.osc2.stringMode = stringMode;
        p.osc2.stringExcite = 0;
        p.osc2.stringDecay = 0.8f;
        p.osc2.stringDamping = 0.25f;
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
        p.osc1.table = &sineTable;
        p.osc1.level = 0.0f;
        p.osc2Enabled = true;
        p.osc2.table = &sineTable;
        p.osc2.level = level;
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
        Voice voice;
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
        Voice voice;
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
        if (categories[presetIndex] == "FX")
            continue;

        processor.loadFactoryPreset (presetIndex);
        processor.panic();

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
        const auto cents = std::abs ((semitones - std::round (semitones)) * 100.0);

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

    processor.loadFactoryPreset (findPreset ("Rip Bass"));
    check (slotType (1) == 2, "legacy preset restores its Drive module (slot1 " + juce::String (slotType (1)) + ")");

    processor.loadFactoryPreset (findPreset ("Scream Lead"));
    check (slotType (1) == 9 && slotType (2) == 13,
           "legacy preset restores Delay and Reverb in order (slot1 " + juce::String (slotType (1))
               + ", slot2 " + juce::String (slotType (2)) + ")");

    processor.loadFactoryPreset (findPreset ("Metal Keys"));
    check (slotType (7) == 9 && slotType (10) == 13,
           "modern preset keeps its explicit slot layout");

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

    check (processor.apvts.getParameter ("lfo3_rate") != nullptr
               && processor.apvts.getParameter ("lfo4_shape") != nullptr,
           "LFO 3 and 4 parameters exist");

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

    // Self-oscillation: silence in, a sustained bounded tone out, for both
    // the SVF above its old resonance cap and the ladder at full feedback.
    for (const auto type : { (int) FilterType::LowPass, (int) FilterType::LadderLow })
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

    // Below the old cap the SVF path must be bit-identical to the plain Svf,
    // so existing presets don't change.
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
            const auto x = random.nextFloat() * 2.0f - 1.0f;
            maxDiff = juce::jmax (maxDiff, std::abs (unit.process (x) - b.processSample (a.processSample (x))));
        }

        check (maxDiff == 0.0f, "24 dB SVF through FilterUnit matches the legacy cascade exactly");
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

    const auto full16 = widthOf (16, 1.0f, UnisonMode::Classic);
    const auto blend0 = widthOf (15, 0.0f, UnisonMode::Classic);
    const auto hyper = widthOf (16, 1.0f, UnisonMode::Hypersaw);
    const auto octaves = widthOf (9, 1.0f, UnisonMode::Octaves);

    check (full16 > 0.2 && std::isfinite (hyper) && hyper > 0.1 && std::isfinite (octaves),
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
    check (heavy.first < 0.5 && heavy.second && extreme.second, "heavy patch (8 notes x 32 unison voices) renders well inside real time");
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
} // namespace

// Every factory preset (Init aside) has four named, working macros, plays,
// and stays bounded with all macros at full.
void runFactoryLibraryTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    juce::StringArray unnamed, unmapped, badSlots, silent, unbounded;
    std::vector<std::pair<double, juce::String>> levels;

    const auto setMacros = [&processor] (float value)
    {
        for (int m = 1; m <= 4; ++m)
            if (auto* parameter = processor.apvts.getParameter ("macro" + juce::String (m)))
                parameter->setValueNotifyingHost (value);
    };

    const auto render = [&processor] (int blocks, bool startNotes, double& rms)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        auto peak = 0.0f;
        auto sum = 0.0;
        auto count = 0;
        auto finite = true;

        for (int block = 0; block < blocks; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (block == 0 && startNotes)
                for (const auto note : { 48, 55, 60 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int s = 0; s < 512; ++s)
                {
                    const auto value = buffer.getSample (channel, s);
                    finite = finite && std::isfinite (value);
                    peak = juce::jmax (peak, std::abs (value));
                    sum += (double) value * (double) value;
                    ++count;
                }
        }

        rms = std::sqrt (sum / juce::jmax (1, count));
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
        const auto peak = render (90, true, rms);

        if (peak < 0.001f)
            silent.add (names[index]);

        levels.push_back ({ rms, names[index] });

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
    check (unbounded.isEmpty(), "factory presets stay bounded with macros at full (" + unbounded.joinIntoString (", ") + ")");

    // Loudness: flag presets far from the library's median level.
    auto sorted = levels;
    std::sort (sorted.begin(), sorted.end());
    const auto median = sorted[sorted.size() / 2].first;
    juce::StringArray outliers;

    for (const auto& level : levels)
    {
        const auto db = 20.0 * std::log10 (juce::jmax (1.0e-9, level.first / median));

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

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // ILANA_BENCH=1 runs only the CPU benchmark (for profiling).
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH", "").isNotEmpty())
    {
        runPhase2StateAndCpuTest();
        return 0;
    }

    std::cout << "ilanaSynth table tests" << std::endl;

    runMipmapTests();
    runAliasTests();
    runLoaderTest();
    runFilterTests();
    runSubOscillatorTests();
    runVoiceSmokeTest();
    runFrameModulationTest();
    runCrossModulationTest();
    runKarplusStrongTest();
    runFactoryTableContentTest();
    runWeirdDspTest();
    runSampleOscTest();
    runPresetSanityTest();
    runOsc2Test();
    runOscLevelTest();
    runPresetTuningTest();
    runParameterStressTest();
    runFxModuleIsolationTest();
    runFxIntegrityTest();
    runStateRoundTripTest();
    runSoakTest();
    runLegacyPresetFxTest();
    runLfo34Test();
    runOversamplingTest();
    runFxSlotAssignTest();
    runPresetNameTest();
    runFilterModelTests();
    runVoiceModeTests();
    runWarpTests();
    runUnisonTests();
    runPerVoiceLfoTest();
    runMatrixTests();
    runStaleModulationTest();
    runPhase2StateAndCpuTest();
    runExtraFilterTests();
    runFilterRoutingTest();
    runOversampledTuningTest();
    runResynthesisTest();
    runCurveLfoTest();
    runFactoryLibraryTest();
    runTapeStopLatencyTest();
    runTranceGateTest();
    runGenerativeTests();
    runOsc3MigrationTest();
    runFmMatrixTests();

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << " (" << failures << " failures)" << std::endl;

    return failures == 0 ? 0 : 1;
}
