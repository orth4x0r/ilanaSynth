// Polish-review repro tool (not part of the product build).
//   ilanaPolishRepro scratch    FX scratch-buffer leak: a big block first, then small ones (FX-1/FX-2)
//   ilanaPolishRepro arp        arp note numbers above 127 (TS-1)
//   ilanaPolishRepro state      state round-trips: sample/table paths, truncated state (S2/S10)
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iostream>
#include "PluginProcessor.h"
#include "dsp/Voice.h"

static std::vector<float> phrase (IlanaSynthAudioProcessor& p, bool bigWarmup, double seconds = 1.5)
{
    p.setNonRealtime (true);
    p.prepareToPlay (48000.0, 4096);
    juce::AudioBuffer<float> buf (2, 4096);
    juce::MidiBuffer none;
    // warm-up: 4096 silent samples either as one block or as 64 blocks
    juce::MidiBuffer chord;
    for (int n : { 48, 55, 64 }) chord.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    // the chord sounds during the warm-up so the FX slots are active and their scratch buffers grow
    if (bigWarmup) { buf.clear(); p.processBlock (buf, chord); }
    else for (int i = 0; i < 64; ++i) { buf.clear(); juce::AudioBuffer<float> v (buf.getArrayOfWritePointers(), 2, 64); p.processBlock (v, i == 0 ? chord : none); }
    std::vector<float> out;
    int total = (int) (seconds * 48000), pos = 0;
    while (pos < total)
    {
        juce::MidiBuffer midi;
        
        buf.clear();
        juce::AudioBuffer<float> v (buf.getArrayOfWritePointers(), 2, 64);
        p.processBlock (v, midi);
        for (int i = 0; i < 64; ++i) { out.push_back (v.getSample (0, i)); out.push_back (v.getSample (1, i)); }
        pos += 64;
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String mode (argc > 1 ? argv[1] : "scratch");
    if (mode == "scratch")
    {
        IlanaSynthAudioProcessor::forceVoiceThreads = 1;
        int bad = 0, n = 0; const bool ctl = argc > 2; // control: both runs use the big warm-up
        const int count = IlanaSynthAudioProcessor().getFactoryPresetNames().size();
        for (int i = 0; i < count; ++i)
        {
            IlanaSynthAudioProcessor a, b;
            a.setNonRealtime (true); b.setNonRealtime (true);
            a.prepareToPlay (48000.0, 4096); b.prepareToPlay (48000.0, 4096);
            a.loadFactoryPreset (i); b.loadFactoryPreset (i);
            auto x = phrase (a, true), y = phrase (b, ctl);
            double d = 0, e = 0;
            for (size_t k = 0; k < x.size(); ++k) { d += (x[k] - y[k]) * (x[k] - y[k]); e += y[k] * y[k]; }
            double rel = std::sqrt (d / (e + 1e-12));
            ++n;
            if (rel > 1e-3) { ++bad; std::cout << "DIFF " << i << " " << a.getFactoryPresetNames()[i] << " rel=" << rel << std::endl; }
        }
        std::cout << "checked " << n << " presets, " << bad << " differ after a big first block" << std::endl;
    }
    return 0;
}
