// Roadmap step 16: reroutable oscillators (dsp/VoiceRouting.h, the
// route_cables and cable_* parameters, processor/OscRouting.cpp). Included
// by TableTest.cpp after FxPatcherTests.inc; ILANA_OSC_ROUTING_TEST=1 runs
// only these (with every factory preset in the migration check; the whole
// suite checks every sixth).

namespace OscRoutingLab
{
using namespace VoiceRouting;

inline void setPlain (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// A held note through the whole processor, the left channel.
inline std::vector<float> renderNote (IlanaSynthAudioProcessor& processor, int note = 60, int blocks = 40)
{
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> out;
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

inline float worstOf (const std::vector<float>& a, const std::vector<float>& b)
{
    auto worst = 0.0f;
    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
        worst = juce::jmax (worst, std::abs (a[i] - b[i]));
    return worst;
}

inline float peakOf (const std::vector<float>& x)
{
    auto peak = 0.0f;
    for (const auto v : x)
        peak = juce::jmax (peak, std::abs (v));
    return peak;
}

inline double db (double amplitude) { return 20.0 * std::log10 (amplitude + 1.0e-12); }

// One patch: OSC 1 (with the clean patch's settings) and cables.
inline std::vector<float> renderCables (const Patch& patch, const std::function<void (IlanaSynthAudioProcessor&)>& extra, int note = 60)
{
    return renderCleanPatch ([&] (IlanaSynthAudioProcessor& processor)
    {
        extra (processor);
        processor.setRouteCables (patch, true);
    }, note, 0.6);
}

inline Patch only (std::uint8_t osc1, std::uint8_t f1Out, std::uint8_t f2Out, std::uint8_t westOut)
{
    Patch patch;
    patch.source[0] = osc1;
    patch.stage[f1] = f1Out;
    patch.stage[f2] = f2Out;
    patch.stage[west] = westOut;
    return patch;
}
} // namespace OscRoutingLab

void runOscRoutingTests (bool everyPreset)
{
    using namespace OscRoutingLab;

    // The compiler.
    {
        const auto table = compile (defaultPatch());
        check (table.on && table.numOrdered == 2 && table.order[0] == f1 && table.order[1] == f2 && table.used[f1] && table.used[f2]
                   && ! table.used[west],
               "default cables: Filter 1 then Filter 2, WEST unused");
        check (table.numBuses == 1 && table.busMask[0] == toF1, "default cables: every source shares one bus into Filter 1");
        check (table.stageOut[f1] == toF2 && table.stageOut[f2] == toOut && table.filterPairInto == 0, "default cables: F1 -> F2 -> OUT");

        // WEST before the filters.
        Patch westFirst;
        westFirst.source.fill (toWest);
        westFirst.stage = { toF2, toOut, toF1 };
        const auto w = compile (westFirst);
        check (w.numOrdered == 3 && w.order[0] == west && w.order[1] == f1 && w.order[2] == f2, "WEST -> F1 -> F2 runs in that order");

        // Filter 2 before Filter 1.
        Patch reversed;
        reversed.source.fill (toF2);
        reversed.stage = { toOut, toF1, toOut };
        const auto r = compile (reversed);
        check (r.numOrdered == 2 && r.order[0] == f2 && r.order[1] == f1, "F2 -> F1 runs Filter 2 first");

        // A loop is refused, and only its forward edge kept.
        Patch loop = reversed;
        loop.stage[f1] = toF2 | toOut;
        check (hasLoop (loop) && ! hasLoop (withoutLoops (loop)) && (withoutLoops (loop).stage[f1] & toF2) != 0
                   && (withoutLoops (loop).stage[f2] & toF1) == 0,
               "a loop between the filters is found and cut to F1 -> F2");

        // Unused stages and muted sources.
        Patch sparse;
        sparse.source[0] = toOut;
        sparse.source[1] = toF1 | toOut;
        sparse.stage = { 0, toOut, toOut }; // F1 goes nowhere; F2 and WEST hear nothing
        const auto s = compile (sparse);
        check (! s.used[f1] && ! s.used[f2] && ! s.used[west] && s.numOrdered == 0, "stages that hear nothing or go nowhere are not run");
        check (s.busOf[0] == s.busOf[1] && s.busMask[(size_t) s.busOf[0]] == toOut, "a cable into a stage that is not run is dropped");
        check (s.busOf[2] == mutedBus && s.busOf[6] == mutedBus, "a source without cables writes the muted bus");

        // Two filters into OUT mix as PARALLEL does.
        const auto p = compile (fromChain ({}, true, false, 0));
        check (p.filterPairInto == toOut && p.busMask[0] == (toF1 | toF2), "the PARALLEL chain as cables: both filters into OUT, mixed as a pair");
        check (compile (fromChain ({}, true, true, 0)).filterPairInto == toWest, "PARALLEL with WEST after: the pair mixes into WEST");

        // The chain as cables, route by route.
        const auto chain = fromChain ({ 0, 1, 2, 3, 4, 0, 3 }, false, true, 0);
        check (chain.source[0] == toF1 && chain.source[1] == toF1 && chain.source[2] == toF2 && chain.source[3] == toWest
                   && chain.source[4] == (toF1 | toF2) && chain.source[6] == toWest && chain.stage[f1] == toF2 && chain.stage[f2] == toWest
                   && chain.stage[west] == toOut,
               "the fixed chain's routes become cables (No filter into WEST while WEST is after the filters)");
        const auto replaced = fromChain ({ 2, 0, 0, 0, 0, 0, 0 }, false, true, 1);
        check (replaced.source[0] == toWest && replaced.stage[f1] == toWest && replaced.stage[west] == toOut,
               "WEST in Filter 2's place: Filter 2's cables go to WEST");
    }

    // The parameters: appended, off by default, the defaults the default chain.
    {
        IlanaSynthAudioProcessor processor;
        check (processor.apvts.getRawParameterValue ("route_cables") != nullptr && ! processor.areRouteCablesOn(),
               "route_cables exists and is off by default");
        check (processor.getRoutePatch() == defaultPatch(), "the cable_* defaults are the default chain");
        check (processor.getChainAsCables() == defaultPatch(), "a new patch's chain is the default cables");

        Patch loop = defaultPatch();
        loop.stage[f2] = toF1;
        processor.setRouteCables (loop, true);
        check (processor.areRouteCablesOn() && ! hasLoop (processor.getRoutePatch()), "a loop never reaches the parameters");

        setPlain (processor, "west_on", 0.0f);
        Patch intoWest = defaultPatch();
        intoWest.source[0] = toWest;
        processor.setRouteCables (intoWest, true);
        check (processor.apvts.getRawParameterValue ("west_on")->load() > 0.5f, "a cable into WEST switches WEST on");
    }

    // Sound: a saw through a dark Filter 1, or around it straight OUT.
    {
        const auto dark = [] (IlanaSynthAudioProcessor& processor)
        {
            setPlain (processor, "osc1_table", 10.0f);
            setPlain (processor, "f1_type", 0.0f);
            setPlain (processor, "f1_cutoff", 300.0f);
            setPlain (processor, "f1_slope", 1.0f);
        };
        const auto viaFilter = renderCables (only (toF1, toOut, toOut, toOut), dark);
        const auto direct = renderCables (only (toOut, toOut, toOut, toOut), dark);
        const auto both = renderCables (only (toF1 | toOut, toOut, toOut, toOut), dark);
        const auto none = renderCables (only (0, toOut, toOut, toOut), dark);
        const auto harmonic = 261.63 * 8.0;
        const auto start = (size_t) 9600, length = (size_t) 8192;
        const auto filtered = partialAmplitude (viaFilter, start, length, harmonic);
        const auto open = partialAmplitude (direct, start, length, harmonic);
        check (db (open) - db (filtered) > 30.0, "OSC 1 cabled straight OUT skips the filter (8th harmonic "
                                                     + juce::String (db (open) - db (filtered), 1) + " dB louder)");
        auto worst = 0.0f;
        for (size_t i = 0; i < both.size(); ++i)
            worst = juce::jmax (worst, std::abs (both[i] - (viaFilter[i] + direct[i])));
        check (worst < 1.0e-4f, "two cables play in parallel: F1 + OUT is the sum of each alone (worst " + juce::String (worst, 7) + ")");
        check (peakOf (none) == 0.0f && peakOf (direct) > 0.1f, "an oscillator with no cable is silent");
    }

    // Order: WEST's fold before a dark filter is darkened by it; after it, not.
    {
        const auto westPatch = [] (IlanaSynthAudioProcessor& processor)
        {
            setPlain (processor, "f1_type", 0.0f);
            setPlain (processor, "f1_cutoff", 300.0f);
            setPlain (processor, "f1_slope", 1.0f);
            setPlain (processor, "west_on", 1.0f);
            setPlain (processor, "west_fold", 1.0f);
            setPlain (processor, "west_mode", 2.0f);
            setPlain (processor, "west_open", 1.0f);
        };
        const auto westFirst = renderCables (only (toWest, toOut, toOut, toF1), westPatch, 48);
        const auto westLast = renderCables (only (toF1, toWest, toOut, toOut), westPatch, 48);
        const auto fundamental = 130.81;
        const auto start = (size_t) 9600, length = (size_t) 8192;
        auto first = 0.0, last = 0.0;
        for (const auto h : { 3.0, 5.0, 7.0 })
        {
            first += partialAmplitude (westFirst, start, length, fundamental * h);
            last += partialAmplitude (westLast, start, length, fundamental * h);
        }
        check (peakOf (westFirst) > 0.01f && db (last) - db (first) > 10.0,
               "cables set the order: WEST after Filter 1 keeps its fold's harmonics, before it they are filtered ("
                   + juce::String (db (last) - db (first), 1) + " dB)");
    }

    // Migration: every factory preset, its chain played as cables, renders as
    // the chain does except in the corners the cables don't model (see
    // VoiceRouting::fromChain). Off, nothing changes at all (the fingerprint
    // check, tests/fingerprints-linux.csv).
    {
        IlanaSynthAudioProcessor names;
        const auto presets = names.getFactoryPresetNames();
        const auto stride = everyPreset ? 1 : 6;
        auto checked = 0, same = 0, close = 0, corners = 0, unrepeatable = 0, failed = 0;
        juce::String firstFailure, cornerNames;
        for (int index = 0; index < presets.size(); index += stride)
        {
            auto corner = false;
            const auto render = [&] (bool cables)
            {
                IlanaSynthAudioProcessor processor;
                processor.loadFactoryPreset (index);
                if (cables)
                {
                    const auto read = [&] (const juce::String& id) { return processor.apvts.getRawParameterValue (id)->load(); };
                    const auto parallel = read ("filters_parallel") > 0.5f;
                    corner = read ("west_on") > 0.5f && juce::roundToInt (read ("west_pos")) == 1 && read ("f2_drive") > 1.0f;
                    corner = corner || (parallel && read ("f1_drive") > 1.0f);
                    for (int osc = 0; osc < OscillatorIds::count; ++osc)
                        corner = corner || juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_route")) == 4;
                    corner = corner || juce::roundToInt (read ("subosc_route")) == 4;
                    processor.setRouteCables (processor.getChainAsCables(), true);
                }
                return renderNote (processor, 60, 24);
            };
            ++checked;
            const auto chain = render (false);
            const auto again = render (false);
            const auto cables = render (true);
            if (worstOf (chain, again) > 0.0f)
            {
                ++unrepeatable;
                continue;
            }
            const auto worst = worstOf (chain, cables);
            const auto relative = worst / juce::jmax (1.0e-6f, peakOf (chain));
            if (worst == 0.0f)
                ++same;
            else if (relative < 1.0e-4f)
                ++close;
            else if (corner)
            {
                ++corners;
                cornerNames << (cornerNames.isEmpty() ? "" : ", ") << presets[index];
            }
            else
            {
                ++failed;
                if (firstFailure.isEmpty())
                    firstFailure = presets[index] + " (" + juce::String (relative, 6) + " of its peak)";
            }
        }
        check (failed == 0, "migration: " + juce::String (checked) + " factory presets play their chain as cables: " + juce::String (same)
                                + " sample for sample, " + juce::String (close) + " within rounding, " + juce::String (corners)
                                + " in an unmodelled corner, " + juce::String (failed) + " differ (" + firstFailure + ")");
        if (unrepeatable > 0)
            std::cout << "  (" << unrepeatable << " presets are not repeatable run to run, so not compared)" << std::endl;
        if (corners > 0)
            std::cout << "  (corners: " << cornerNames << ")" << std::endl;
    }
}
