#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include <complex>
#include <iostream>
#include <vector>

#include "Presets.h"
#include "PluginProcessor.h"
#include "dsp/GranularPitchShift.h"
#include "dsp/GranularSmear.h"
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

    p.filter2.mode = Svf::Mode::HighPass;
    p.filter2.cutoffHz = 100.0f;

    p.ampEnv = { 0.005f, 0.3f, 0.8f, 0.25f };
    p.filterEnv = { 0.001f, 0.4f, 0.2f, 0.3f };
    p.modEnv = { 0.01f, 0.3f, 0.5f, 0.3f };

    p.numModSlots = 2;
    p.modSlots[0] = { Mod::Source::FilterEnv, Mod::Destination::Osc1Frame, 0.5f };
    p.modSlots[1] = { Mod::Source::Random, Mod::Destination::Osc2Pitch, 0.2f };

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
        p.numModSlots = 1;
        p.modSlots[0] = { source, Mod::Destination::Osc1Frame, modDepth };

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

    for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
    {
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

        for (int slot = 1; slot <= 8; ++slot)
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
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

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

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << " (" << failures << " failures)" << std::endl;

    return failures == 0 ? 0 : 1;
}
