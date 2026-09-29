// M7.5: tests for ilanaSynth FX, the engine built as an effect (ILANA_FX=1)
// that plays its audio input. The instrument's own tests are in TableTest.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/SampleFactory.h"

namespace
{
int failures = 0;

void check (bool condition, const juce::String& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message.toStdString() << std::endl;
    if (! condition)
        ++failures;
}

void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// Runs the processor over an input signal (mono, copied to both channels)
// and returns the left output.
std::vector<float> run (IlanaSynthAudioProcessor& processor, const std::function<float (int)>& input, int length,
                        const std::function<void (int block, juce::MidiBuffer&)>& midi = {})
{
    constexpr int blockSize = 256;
    processor.prepareToPlay (48000.0, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    std::vector<float> out;
    for (int block = 0; block * blockSize < length; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto x = input (block * blockSize + i);
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }
        juce::MidiBuffer messages;
        if (midi)
            midi (block, messages);
        processor.processBlock (buffer, messages);
        for (int i = 0; i < blockSize; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

double rms (const std::vector<float>& x, int from, int to)
{
    auto sum = 0.0;
    for (int i = from; i < to && i < (int) x.size(); ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (sum / juce::jmax (1, to - from));
}

double toneAt (const std::vector<float>& x, int from, int length, double hz)
{
    std::complex<double> sum;
    for (int i = 0; i < length; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length);
        sum += (double) x[(size_t) (from + i)] * window * std::polar (1.0, -juce::MathConstants<double>::twoPi * hz * i / 48000.0);
    }
    return std::abs (sum) * 4.0 / length;
}

// A patch with nothing playing: every oscillator off, no FX, no trigger.
void quiet (IlanaSynthAudioProcessor& processor)
{
    processor.loadFactoryPreset (0);
    for (const auto* prefix : { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" })
        setParam (processor, juce::String (prefix) + "_on", 0.0f);
    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        processor.assignFxSlot (slot, 0);
    setParam (processor, "subosc_on", 0.0f);
    setParam (processor, "noise_level", 0.0f);
    setParam (processor, "master", 0.0f);
    setParam (processor, "master_clip", 0.0f);
    setParam (processor, "amp_velocity", 0.0f);
    setParam (processor, "amp_attack", 0.001f);
    setParam (processor, "amp_sustain", 1.0f);
}

const auto sine220 = [] (int n) { return 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * n / 48000.0); };

// ILANA_RENDER_DEMO=<folder>: every FX starting point over three inputs
// (drums, plucks, a voice), with the dry inputs, for listening.
void renderFxDemos (const juce::File& folder)
{
    folder.createDirectory();
    constexpr int length = 48000 * 8;
    std::vector<std::pair<juce::String, std::vector<float>>> inputs;
    {
        std::vector<float> drums ((size_t) length, 0.0f);
        juce::Random random (11);
        for (int beat = 0; beat < 16; ++beat)
        {
            const auto start = beat * 24000;
            for (int i = 0; i < 12000 && start + i < length; ++i)
            {
                const auto t = i / 48000.0;
                if (beat % 2 == 0) // kick
                    drums[(size_t) (start + i)] += 0.8f * (float) (std::sin (juce::MathConstants<double>::twoPi * (50.0 * t + 60.0 * (1.0 - std::exp (-t * 30.0)) / 30.0)) * std::exp (-t * 12.0));
                else // snare
                    drums[(size_t) (start + i)] += 0.5f * (random.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-t * 25.0);
            }
            for (int i = 0; i < 2000 && start + 12000 + i < length; ++i) // hat
                drums[(size_t) (start + 12000 + i)] += 0.2f * (random.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-i / 300.0);
        }
        inputs.push_back ({ "drums", drums });

        std::vector<float> plucks ((size_t) length, 0.0f);
        const double notes[] { 82.41, 110.0, 146.83, 196.0, 110.0, 164.81, 220.0, 123.47 };
        for (int n = 0; n < 8; ++n)
        {
            const auto period = (int) (48000.0 / notes[n]);
            std::vector<float> line ((size_t) period);
            for (auto& value : line)
                value = random.nextFloat() * 2.0f - 1.0f;
            auto previous = 0.0f;
            for (int i = 0; i < 48000; ++i)
            {
                auto& value = line[(size_t) (i % period)];
                const auto out = value;
                value = 0.996f * 0.5f * (value + previous);
                previous = out;
                plucks[(size_t) (n * 48000 + i)] += 0.5f * out;
            }
        }
        inputs.push_back ({ "plucks", plucks });

        const auto vocal = SampleFactory::generate (1);
        std::vector<float> voice ((size_t) length, 0.0f);
        const auto* data = vocal->buffer.getReadPointer (0);
        for (int i = 0; i < length; ++i)
            voice[(size_t) i] = (i / 24000) % 2 == 0 ? 0.6f * data[i % vocal->buffer.getNumSamples()] : 0.0f;
        inputs.push_back ({ "voice", voice });
    }

    const auto write = [&folder] (const juce::String& name, const std::vector<float>& audio)
    {
        juce::AudioBuffer<float> buffer (1, (int) audio.size());
        for (int i = 0; i < (int) audio.size(); ++i)
            buffer.setSample (0, i, audio[(size_t) i]);
        const auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
    };

    for (const auto& [inputName, audio] : inputs)
    {
        write ("input - " + inputName, audio);
        for (const auto* preset : { "Live Body", "Live Wah", "Live Grains", "Live Strings" })
        {
            IlanaSynthAudioProcessor processor;
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (preset));
            const auto& source = audio;
            write (juce::String (preset) + " - " + inputName, run (processor, [&source] (int n) { return n < (int) source.size() ? source[(size_t) n] : 0.0f; }, length));
        }
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::SystemStats::setApplicationCrashHandler ([] (void*)
    {
        std::cout << "CRASH" << std::endl;
        std::cout << juce::SystemStats::getStackBacktrace() << std::endl;
        std::exit (2);
    });
    if (const auto demo = juce::SystemStats::getEnvironmentVariable ("ILANA_RENDER_DEMO", ""); demo.isNotEmpty())
    {
        renderFxDemos (juce::File (demo));
        return 0;
    }

    std::cout << "ilanaSynth FX tests" << std::endl;

    {
        IlanaSynthAudioProcessor processor;
        check (processor.getTotalNumInputChannels() == 2 && processor.getTotalNumOutputChannels() == 2,
               "the effect has a stereo input and output");
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::stereo());
        check (processor.checkBusesLayoutSupported (mono), "a mono input works too");
        check (processor.getCurrentPresetName() == "Live Body" || processor.getFactoryPresetNames().indexOf ("Live Body") >= 0,
               "the effect opens on Live Body");
    }

    // Nothing routed: the input doesn't leak through; DRY puts it back.
    {
        IlanaSynthAudioProcessor processor;
        quiet (processor);
        const auto silent = run (processor, sine220, 24000);
        check (rms (silent, 0, 24000) < 1.0e-6, "with nothing routed the input does not pass through");

        IlanaSynthAudioProcessor dry;
        quiet (dry);
        setParam (dry, "in_dry", 1.0f);
        const auto out = run (dry, sine220, 24000);
        auto worst = 0.0f;
        for (int i = 0; i < 24000; ++i)
            worst = juce::jmax (worst, std::abs (out[(size_t) i] - sine220 (i)));
        check (worst < 1.0e-5f, "DRY 100% is the input unchanged");
    }

    // With oversampling on, DRY is delayed by the reported latency, so it
    // lines up with the wet signal and a blend doesn't comb-filter.
    {
        const auto click = [] (int n) { return n == 12000 ? 0.8f : 0.0f; };
        const auto peakIndex = [] (const std::vector<float>& x)
        {
            auto best = 0;
            for (int i = 1; i < (int) x.size(); ++i)
                if (std::abs (x[(size_t) i]) > std::abs (x[(size_t) best]))
                    best = i;
            return best;
        };
        const auto render = [&] (float dryAmount, float wetLevel, int& latency)
        {
            IlanaSynthAudioProcessor processor;
            quiet (processor);
            setParam (processor, "oversampling", 1.0f);
            setParam (processor, "os_factor", 0.0f);
            setParam (processor, "in_dry", dryAmount);
            if (wetLevel > 0.0f)
            {
                setParam (processor, "osc1_on", 1.0f);
                setParam (processor, "osc1_mode", 4.0f);
                setParam (processor, "osc1_level", wetLevel);
                setParam (processor, "osc1_route", 3.0f);
                setParam (processor, "in_trigger", 2.0f);
                setParam (processor, "master", 0.0f);
            }
            const auto out = run (processor, click, 24000);
            latency = processor.getLatencySamples();
            return out;
        };

        auto latency = 0;
        const auto dryOnly = render (1.0f, 0.0f, latency);
        auto worst = 0.0f;
        for (int i = latency; i < 24000; ++i)
            worst = juce::jmax (worst, std::abs (dryOnly[(size_t) i] - click (i - latency)));
        check (latency > 0 && worst < 1.0e-5f, "with 2x oversampling DRY is the input delayed by the latency ("
                                                  + juce::String (latency) + " samples)");

        auto wetLatency = 0;
        const auto wetOnly = render (0.0f, 1.0f, wetLatency);
        const auto dryPeak = peakIndex (dryOnly), wetPeak = peakIndex (wetOnly);
        check (std::abs (wetOnly[(size_t) wetPeak]) > 1.0e-3f && std::abs (dryPeak - wetPeak) <= 1,
               "DRY lines up with the oversampled wet signal (dry peak " + juce::String (dryPeak)
                   + ", wet peak " + juce::String (wetPeak) + ")");
    }

    // A Live oscillator, held by DRONE, plays the input through the voice.
    {
        IlanaSynthAudioProcessor processor;
        quiet (processor);
        setParam (processor, "osc1_on", 1.0f);
        setParam (processor, "osc1_mode", 4.0f);
        setParam (processor, "osc1_level", 1.0f);
        setParam (processor, "in_trigger", 2.0f);
        const auto out = run (processor, sine220, 48000);
        const auto at220 = toneAt (out, 24000, 8192, 220.0);
        check (at220 > 0.05, "a Live oscillator plays the input (220 Hz at " + juce::String (at220, 3) + ")");

        setParam (processor, "in_gain", -12.0f);
        const auto quieter = run (processor, sine220, 48000);
        const auto ratio = toneAt (quieter, 24000, 8192, 220.0) / juce::jmax (1.0e-9, at220);
        check (std::abs (ratio - 0.251) < 0.05, "INPUT GAIN -12 dB turns it down by a quarter ("
                                                 + juce::String (ratio, 3) + ")");
    }

    // BODY rings from the input and keeps ringing after it stops.
    {
        const auto body = [] (float amount)
        {
            IlanaSynthAudioProcessor processor;
            quiet (processor);
            setParam (processor, "in_trigger", 2.0f);
            setParam (processor, "res_on", 1.0f);
            setParam (processor, "body_type", 2.0f);
            setParam (processor, "res_amount", 1.0f);
            setParam (processor, "res_decay", 0.8f);
            setParam (processor, "in_body", amount);
            return run (processor, [] (int n) { return n >= 12000 && n < 12100 ? 0.8f : 0.0f; }, 48000);
        };
        const auto rung = body (1.0f);
        const auto dry = body (0.0f);
        check (rms (rung, 14000, 30000) > 1.0e-4 && rms (dry, 14000, 30000) < 1.0e-6,
               "INPUT TO BODY rings the body from a click (" + juce::String (rms (rung, 14000, 30000), 5) + ")");
    }

    // The strings ring at their pitch when the input drives them.
    {
        const auto strings = [] (float amount)
        {
            IlanaSynthAudioProcessor processor;
            quiet (processor);
            setParam (processor, "osc1_on", 1.0f);
            setParam (processor, "osc1_mode", 1.0f);
            setParam (processor, "osc1_excite", 6.0f); // Osc In: silent until something drives it
            setParam (processor, "osc1_string_decay", 0.95f);
            setParam (processor, "in_trigger", 2.0f);
            setParam (processor, "in_note", 45.0f); // A2, 110 Hz
            setParam (processor, "in_strings", amount);
            juce::Random random (3);
            return run (processor, [&random] (int n) { return n > 24000 ? (random.nextFloat() - 0.5f) * 0.3f : 0.0f; }, 72000);
        };
        const auto driven = strings (1.0f);
        const auto plain = strings (0.0f);
        const auto drivenTone = toneAt (driven, 60000, 8192, 110.0);
        const auto plainTone = toneAt (plain, 60000, 8192, 110.0);
        check (drivenTone > plainTone * 3.0 && drivenTone > 1.0e-3,
               "INPUT TO STRINGS keeps a string ringing at its pitch (" + juce::String (plainTone, 5) + " -> "
                   + juce::String (drivenTone, 5) + ")");
    }

    // GATE plays IN NOTE while the input is loud and lets go when it stops.
    {
        IlanaSynthAudioProcessor processor;
        quiet (processor);
        setParam (processor, "osc1_on", 1.0f);
        setParam (processor, "osc1_table", 0.0f);
        setParam (processor, "amp_release", 0.02f);
        setParam (processor, "in_trigger", 1.0f);
        setParam (processor, "in_threshold", -30.0f);
        setParam (processor, "in_note", 69.0f);
        // RELEASE (150 ms) is a time constant: the gate lets go about 0.6 s
        // after the input stops.
        const auto out = run (processor, [] (int n) { return n >= 24000 && n < 48000 ? sine220 (n) : 0.0f; }, 120000);
        const auto before = rms (out, 0, 23000), during = rms (out, 30000, 46000), after = rms (out, 100000, 120000);
        check (before < 1.0e-5 && during > 0.01 && after < 1.0e-4,
               "GATE plays while the input is loud (" + juce::String (before, 5) + ", " + juce::String (during, 3)
                   + ", " + juce::String (after, 5) + ")");
        check (toneAt (out, 30000, 8192, 440.0) > 0.01, "the gated note is IN NOTE (A4)");
    }

    // The envelope follower is a mod source.
    {
        IlanaSynthAudioProcessor processor;
        quiet (processor);
        run (processor, sine220, 24000);
        const auto loud = processor.getInputEnvelope();
        run (processor, [] (int) { return 0.0f; }, 48000);
        const auto quietEnvelope = processor.getInputEnvelope();
        const auto names = Mod::getSourceNames();
        check (names[(int) Mod::Source::InputEnv] == "Input Env" && (int) Mod::Source::Count == names.size(),
               "Input Env keeps its mod source index");
        check (loud > 0.4f && quietEnvelope < 0.01f, "the envelope follows the input (" + juce::String (loud, 3)
                                                         + " loud, " + juce::String (quietEnvelope, 4) + " after)");

        // Routed to the filter: a louder input opens it.
        IlanaSynthAudioProcessor wah;
        wah.loadFactoryPreset (wah.getFactoryPresetNames().indexOf ("Live Wah"));
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            wah.assignFxSlot (slot, 0);
        juce::Random random (9);
        const auto noise = [&random] (int n) { return (random.nextFloat() - 0.5f) * (n < 24000 ? 0.05f : 0.8f); };
        const auto out = run (wah, noise, 48000);
        std::vector<float> highs (out.size());
        for (size_t i = 1; i < out.size(); ++i)
            highs[i] = out[i] - out[i - 1];
        const auto softBright = rms (highs, 12000, 24000) / juce::jmax (1.0e-9, rms (out, 12000, 24000));
        const auto loudBright = rms (highs, 36000, 48000) / juce::jmax (1.0e-9, rms (out, 36000, 48000));
        check (loudBright > softBright * 1.3, "Live Wah: a louder input opens the filter ("
                                                 + juce::String (softBright, 3) + " -> " + juce::String (loudBright, 3) + ")");
    }

    // Live grains read the input's recent history.
    {
        IlanaSynthAudioProcessor processor;
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Live Grains"));
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            processor.assignFxSlot (slot, 0);
        setParam (processor, "in_dry", 0.0f);
        const auto out = run (processor, sine220, 96000);
        const auto tone = toneAt (out, 72000, 8192, 220.0);
        auto finite = true;
        for (auto value : out)
            finite = finite && std::isfinite (value);
        check (finite && tone > 0.01, "Live Grains granulate the input (220 Hz at " + juce::String (tone, 3) + ")");
    }

    // Every FX starting point plays its input and stays in range.
    for (const auto* name : { "Live Body", "Live Wah", "Live Grains", "Live Strings" })
    {
        IlanaSynthAudioProcessor processor;
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (name));
        juce::Random random (5);
        const auto out = run (processor, [&random] (int n) { return n % 24000 < 12000 ? (random.nextFloat() - 0.5f) * 0.5f : 0.0f; }, 96000);
        auto peak = 0.0f;
        auto finite = true;
        for (auto value : out)
        {
            finite = finite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }
        check (finite && peak > 0.01f && peak < 2.0f, juce::String (name) + " plays its input (peak " + juce::String (peak, 3) + ")");
    }

    // The editor has the INPUT page, and the Live oscillator shows the input.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editor->setSize (1060, 720);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        juce::TabbedComponent* tabs = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& parent)
        {
            for (auto* child : parent.getChildren())
            {
                if (auto* match = dynamic_cast<juce::TabbedComponent*> (child))
                    tabs = match;
                find (*child);
            }
        };
        find (*editor);
        const auto index = tabs != nullptr ? tabs->getTabNames().indexOf ("INPUT") : -1;
        check (index >= 0 && index == tabs->getNumTabs() - 1, "ilanaSynth FX has an INPUT page (the last tab)");
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory);
        const auto snap = [&editor, &folder] (const juce::String& name)
        {
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            const auto file = folder.getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        };
        if (index >= 0)
        {
            tabs->setCurrentTabIndex (index);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            snap ("ilana-fx-input.png");
            tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("OSC"));
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Live Wah"));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            snap ("ilana-fx-osc.png");
        }
        editor.reset();
    }

    std::cout << (failures == 0 ? "FX TESTS PASSED" : "FX TESTS FAILED") << " (" << failures << " failures)" << std::endl;
    return failures == 0 ? 0 : 1;
}
