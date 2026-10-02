// Dev tool: renders every factory preset through the full processor and
// prints a per-preset fingerprint (RMS, peak, spectral centroid, stereo
// width) as CSV. Run it before and after an engine change and diff the two
// files with tools/compare_fingerprints.py to catch presets that changed.
//
//   ilanaFingerprint out.csv

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <complex>
#include <fstream>
#include <iostream>

#include "PluginProcessor.h"
#include "dsp/FilterUnit.h"
#include "dsp/Voice.h"

namespace
{
struct Fingerprint
{
    double rmsDb = -120.0;
    double peak = 0.0;
    double centroid = 0.0;
    double side = 0.0;
};

Fingerprint render (IlanaSynthAudioProcessor& processor, int presetIndex,
                    std::function<void (IlanaSynthAudioProcessor&)> tweak = nullptr)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    processor.loadFactoryPreset (presetIndex);
    if (tweak != nullptr)
        tweak (processor);
    processor.panic();

    // Clear effect tails left from the previous preset.
    processor.releaseResources();
    processor.prepareToPlay (sampleRate, blockSize);

    const auto totalSeconds = 3.0;
    const auto totalBlocks = (int) (totalSeconds * sampleRate / blockSize);
    std::vector<float> mid, sideSignal;

    juce::AudioBuffer<float> buffer (2, blockSize);

    for (int block = 0; block < totalBlocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        const auto time = block * blockSize / sampleRate;

        // A short chord, then a lower single note: covers poly, glide and
        // release behaviour.
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 80), 0);
        }

        if (block == (int) (0.9 * sampleRate / blockSize))
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 1);
        }

        if (block == (int) (2.0 * sampleRate / blockSize))
            midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);

        processor.processBlock (buffer, midi);

        if (time >= 0.0)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto l = buffer.getSample (0, i);
                const auto r = buffer.getSample (1, i);
                mid.push_back (0.5f * (l + r));
                sideSignal.push_back (0.5f * (l - r));
            }
        }
    }

    Fingerprint result;
    double sum = 0.0, sideSum = 0.0;

    // ILANA_FINGERPRINT_DUMP=dir: also write each render's mid signal
    // (raw float32, 48 kHz) as dir/<index>.f32, for looking at one closely.
    const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_FINGERPRINT_DUMP", "");
    if (dump.isNotEmpty())
        juce::File (dump).getChildFile (juce::String (presetIndex) + ".f32").replaceWithData (mid.data(), mid.size() * sizeof (float));

    for (size_t i = 0; i < mid.size(); ++i)
    {
        sum += (double) mid[i] * mid[i];
        sideSum += (double) sideSignal[i] * sideSignal[i];
        result.peak = juce::jmax (result.peak, (double) std::abs (mid[i]) + std::abs (sideSignal[i]));
    }

    const auto rms = std::sqrt (sum / (double) juce::jmax ((size_t) 1, mid.size()));
    result.rmsDb = 20.0 * std::log10 (juce::jmax (1.0e-6, rms));
    result.side = std::sqrt (sideSum / juce::jmax (1.0e-12, sum));

    // Average magnitude spectrum over 8192-sample frames.
    constexpr int order = 13;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> accum (size / 2, 0.0f);
    std::vector<float> work ((size_t) size * 2);

    for (size_t start = 0; start + size <= mid.size(); start += size / 2)
    {
        std::fill (work.begin(), work.end(), 0.0f);

        for (int i = 0; i < size; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
            work[(size_t) i] = mid[start + (size_t) i] * window;
        }

        fft.performFrequencyOnlyForwardTransform (work.data());

        for (int bin = 0; bin < size / 2; ++bin)
            accum[(size_t) bin] += work[(size_t) bin];
    }

    double weighted = 0.0, total = 0.0;

    for (int bin = 1; bin < size / 2; ++bin)
    {
        const auto frequency = bin * sampleRate / size;
        weighted += frequency * accum[(size_t) bin];
        total += accum[(size_t) bin];
    }

    result.centroid = total > 0.0 ? weighted / total : 0.0;
    return result;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cerr << "usage: ilanaFingerprint out.csv" << std::endl;
        return 1;
    }

    std::ofstream out (argv[1]);
    out << "index,name,rms_db,peak,centroid_hz,side_ratio\n";

    // ILANA_NO_OPEN_BYPASS=1: run Filter 2 in full even when it is wide open
    // (to compare against builds from before the bypass).
    Voice::disableOpenFilterBypass = juce::SystemStats::getEnvironmentVariable ("ILANA_NO_OPEN_BYPASS", "").isNotEmpty();

    IlanaSynthAudioProcessor processor;
    processor.setNonRealtime (true);
    processor.prepareToPlay (48000.0, 256);

    const auto names = processor.getFactoryPresetNames();

    // ILANA_PRESET_PARAMS=file: write each factory preset's loaded values
    // instead (one JSON object per line: name, categories, macro names and
    // every parameter away from its default), for tools/tag_presets.py.
    if (const auto paramsPath = juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_PARAMS", ""); paramsPath.isNotEmpty())
    {
        std::ofstream params (paramsPath.toStdString());
        const auto categories = processor.getFactoryPresetCategories();
        const auto browse = processor.getAllPresetCategories();

        for (int i = 0; i < names.size(); ++i)
        {
            processor.loadFactoryPreset (i);
            auto* line = new juce::DynamicObject();
            line->setProperty ("index", i);
            line->setProperty ("name", names[i]);
            line->setProperty ("category", categories[i]);
            line->setProperty ("browse", browse[i]);
            juce::Array<juce::var> macros;
            for (int m = 0; m < 4; ++m)
                macros.add (processor.getMacroName (m));
            line->setProperty ("macros", macros);
            auto* values = new juce::DynamicObject();
            for (auto* parameter : processor.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    if (std::abs (ranged->getValue() - ranged->getDefaultValue()) > 1.0e-6f)
                        values->setProperty (ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()));
            line->setProperty ("params", juce::var (values));
            params << juce::JSON::toString (juce::var (line), true).toStdString() << "\n";
        }

        std::cout << "wrote " << names.size() << " presets' values to " << paramsPath << std::endl;
        return 0;
    }

    // ILANA_FINGERPRINT_ONLY=a,b: only presets whose names contain one of
    // these (for quick iteration; the filter model rows are always written).
    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_FINGERPRINT_ONLY", ""), ",", "");
    const auto wanted = [&only] (const juce::String& name)
    {
        if (only.isEmpty())
            return true;
        for (const auto& part : only)
            if (part.isNotEmpty() && name.contains (part))
                return true;
        return false;
    };

    for (int i = 0; i < names.size(); ++i)
    {
        if (! wanted (names[i]))
            continue;
        const auto print = render (processor, i);
        out << i << ",\"" << names[i].toStdString() << "\"," << print.rmsDb << "," << print.peak << ","
            << print.centroid << "," << print.side << "\n";
    }

    // M8.4: every filter model on Init (Filter 1, cutoff 1.5 kHz, reso and
    // morph halfway), so a model change shows up too.
    const auto filterNames = FilterType::getNames();
    for (int type = 0; type < FilterType::Count; ++type)
    {
        const auto print = render (processor, 0, [type] (IlanaSynthAudioProcessor& p)
        {
            const auto set = [&p] (const char* id, float value)
            {
                if (auto* parameter = p.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("f1_type", (float) type);
            set ("f1_cutoff", 1500.0f);
            set ("f1_reso", 0.5f);
            set ("f1_morph", 0.5f);
        });
        out << (names.size() + type) << ",\"Filter model: " << filterNames[type].toStdString() << "\"," << print.rmsDb << ","
            << print.peak << "," << print.centroid << "," << print.side << "\n";
    }

    std::cout << "wrote " << names.size() << " fingerprints to " << argv[1] << std::endl;
    return 0;
}
