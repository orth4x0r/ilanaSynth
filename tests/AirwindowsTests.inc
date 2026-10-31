// The Airwindows FX module (type 30) and its ported algorithms. Included by
// TableTest.cpp; ILANA_AIRWINDOWS_TEST=1 runs these alone.

#include "dsp/airwindows/AirwindowsModule.h"
#include "dsp/airwindows/DelaySync.h"

namespace AirwindowsTests
{
constexpr double sr = 48000.0;
constexpr int block = 256;

enum class Input { sine, noise, silence };

const char* inputName (Input input)
{
    return input == Input::sine ? "sine" : (input == Input::noise ? "noise" : "silence");
}

struct Result
{
    bool finite = true;
    float peak = 0.0f;
    float tailPeak = 0.0f; // the last half second
};

// One algorithm on its own, fresh, for `seconds`.
Result run (int algorithm, const std::array<float, 5>& knobs, Input input, double seconds = 1.5, double rate = sr)
{
    const auto& info = airwindows::registry()[(size_t) algorithm];
    auto instance = info.create();
    instance->prepare (rate);

    for (int k = 0; k < info.numKnobs; ++k)
        instance->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (knobs[(size_t) k]));

    juce::Random random (1234);
    std::vector<float> left (block), right (block);
    Result result;
    const auto total = (int) (seconds * rate);
    auto phase = 0.0;

    for (int start = 0; start < total; start += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float l = 0.0f, r = 0.0f;
            if (input == Input::sine)
            {
                l = r = (float) (0.5 * std::sin (phase));
                phase += juce::MathConstants<double>::twoPi * 220.0 / rate;
            }
            else if (input == Input::noise)
            {
                l = random.nextFloat() - 0.5f;
                r = random.nextFloat() - 0.5f;
            }
            left[(size_t) i] = l;
            right[(size_t) i] = r;
        }

        instance->process (left.data(), right.data(), block);

        for (int i = 0; i < block; ++i)
        {
            for (const auto x : { left[(size_t) i], right[(size_t) i] })
            {
                if (! std::isfinite (x))
                {
                    result.finite = false;
                    continue;
                }
                result.peak = std::max (result.peak, std::abs (x));
                if (start + i >= total - (int) (0.5 * rate))
                    result.tailPeak = std::max (result.tailPeak, std::abs (x));
            }
        }
    }

    return result;
}

std::array<float, 5> defaultsOf (int algorithm)
{
    const auto& info = airwindows::registry()[(size_t) algorithm];
    std::array<float, 5> knobs { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    for (int k = 0; k < info.numKnobs; ++k)
        knobs[(size_t) k] = info.knobs[k].defaultValue;
    return knobs;
}

void runRegistryTests()
{
    const auto& list = airwindows::registry();
    check (list.size() >= 50 && list.size() <= 60, "Airwindows: 50-60 algorithms ported (" + juce::String ((int) list.size()) + ")");

    std::set<juce::String> names;
    auto knobsOk = true;
    for (const auto& info : list)
    {
        names.insert (info.name);
        auto instance = info.create();
        knobsOk = knobsOk && info.numKnobs >= 1 && info.numKnobs <= 5 && instance != nullptr
                  && juce::String (info.category).isNotEmpty();
        for (int k = 0; k < info.numKnobs; ++k)
            knobsOk = knobsOk && info.knobs[k].parameter < instance->getNumParameters()
                      && info.knobs[k].defaultValue >= 0.0f && info.knobs[k].defaultValue <= 1.0f
                      && juce::String (info.knobs[k].name).isNotEmpty();
    }
    check (names.size() == list.size(), "Airwindows: algorithm names are unique");
    check (knobsOk, "Airwindows: every algorithm has 1-5 named knobs, mapped to its own parameters, defaults in 0..1");

    // The choice lists the registry, in order.
    IlanaSynthAudioProcessor processor;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_aw_algo"));
    auto sameOrder = choice != nullptr && choice->choices.size() == (int) list.size();
    for (int i = 0; sameOrder && i < (int) list.size(); ++i)
        sameOrder = choice->choices[i] == list[(size_t) i].name;
    check (sameOrder, "Airwindows: fx_aw_algo lists the registry in order");
    check (! choice->isAutomatable(), "Airwindows: the algorithm choice is not host-automatable (addChoice)");
}

// Every algorithm at its defaults and at the knobs' extremes, with a sine,
// noise and silence: finite, bounded, and silence stays near silence (the
// little noise Chris adds is allowed).
void runAlgorithmTests()
{
    const auto& list = airwindows::registry();
    juce::StringArray unbounded, noisy;

    for (int a = 0; a < (int) list.size(); ++a)
    {
        const std::array<float, 5> settings[] { defaultsOf (a), { 0, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1 } };
        const char* settingNames[] { "defaults", "all 0", "all 1" };

        for (int s = 0; s < 3; ++s)
        {
            for (const auto input : { Input::sine, Input::noise, Input::silence })
            {
                const auto result = run (a, settings[s], input);
                const auto what = juce::String (list[(size_t) a].name) + " " + settingNames[s] + " " + inputName (input);

                if (! result.finite || result.peak >= 4.0f)
                    unbounded.add (what + " (peak " + juce::String (result.peak, 2) + ")");

                // -80 dBFS; Deckwrecka's Wreck is noise by design (-78 dB at
                // full), and TapeDust is a dust generator (-40 dB at full).
                const auto name = juce::String (list[(size_t) a].name);
                const auto floor = name == "Deckwrecka" ? 3.0e-4f : name == "TapeDust" ? 2.0e-2f : 1.0e-4f;
                if (input == Input::silence && result.peak >= floor)
                    noisy.add (what + " (" + juce::String (juce::Decibels::gainToDecibels (result.peak), 1) + " dB)");
            }
        }
    }

    check (unbounded.isEmpty(), "Airwindows: every algorithm's output is finite and under 4 ("
                                    + unbounded.joinIntoString (", ") + ")");
    check (noisy.isEmpty(), "Airwindows: silence in gives under -80 dBFS out (" + noisy.joinIntoString (", ") + ")");
}

// Under 44.1 kHz some of Chris's coefficients passed their stable range:
// Console7 went to NaN, Spiral2 to +37 dB, DeRez2 ran away (bug hunt
// 2026-10-31). Every algorithm at 22.05 and 32 kHz, at its defaults, its
// extremes and a few seeded mixes, stays finite and no louder than at 48 kHz
// (with room for 10 dB: a delay at full feedback peaks differently).
void runLowRateTests()
{
    const auto& list = airwindows::registry();
    juce::StringArray unbounded;
    juce::Random random (31);

    for (int a = 0; a < (int) list.size(); ++a)
    {
        std::vector<std::array<float, 5>> settings { defaultsOf (a), { 0, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1 } };
        for (int m = 0; m < 4; ++m)
        {
            std::array<float, 5> mix {};
            for (auto& k : mix)
                k = random.nextFloat() < 0.3f ? (random.nextBool() ? 1.0f : 0.0f) : random.nextFloat();
            settings.push_back (mix);
        }

        for (const auto rate : { 22050.0, 32000.0 })
            for (size_t s = 0; s < settings.size(); ++s)
                for (const auto input : { Input::sine, Input::noise })
                {
                    const auto result = run (a, settings[s], input, 1.5, rate);
                    if (result.finite && result.peak < 4.0f)
                        continue;
                    // Some settings are loud at every rate (a chorus at full
                    // feedback); only a rate that is far louder than 48 kHz is wrong.
                    const auto atNormalRate = run (a, settings[s], input);
                    if (! result.finite || result.peak > 3.0f * std::max (atNormalRate.peak, 1.0f))
                        unbounded.add (juce::String (list[(size_t) a].name) + " " + juce::String ((int) rate) + " Hz set "
                                       + juce::String ((int) s) + " " + inputName (input) + " (peak "
                                       + juce::String (result.peak, 2) + ", " + juce::String (atNormalRate.peak, 2) + " at 48 kHz)");
                }
    }

    check (unbounded.isEmpty(), "Airwindows: every algorithm stays finite and under 4 at 22.05 and 32 kHz ("
                                    + unbounded.joinIntoString (", ") + ")");
}

// Changing the algorithm starts it fresh: a reverb's tail does not come back
// when it is chosen again.
void runResetTest()
{
    airwindows::Module module;
    module.prepare (sr, block);
    const auto galactic = [] { for (int i = 0; i < airwindows::count(); ++i) if (juce::String (airwindows::registry()[(size_t) i].name) == "Galactic") return i; return 0; }();
    const auto other = galactic == 0 ? 1 : 0;
    std::array<float, 5> knobs { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f };
    std::vector<float> left (block), right (block);
    juce::Random random (7);

    const auto blocks = [&] (int algorithm, int count, bool noise)
    {
        auto peak = 0.0f;
        for (int b = 0; b < count; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                left[(size_t) i] = noise ? random.nextFloat() - 0.5f : 0.0f;
                right[(size_t) i] = noise ? random.nextFloat() - 0.5f : 0.0f;
            }
            module.process (left.data(), right.data(), block, algorithm, knobs, 1.0f);
            for (int i = 0; i < block; ++i)
                peak = std::max ({ peak, std::abs (left[(size_t) i]), std::abs (right[(size_t) i]) });
        }
        return peak;
    };

    blocks (galactic, 40, true);
    const auto ringing = blocks (galactic, 4, false);
    blocks (other, 4, false);
    const auto after = blocks (galactic, 4, false);
    check (ringing > 1.0e-3f && after < 1.0e-4f,
           "Airwindows: changing the algorithm resets its state (Galactic tail " + juce::String (ringing, 4)
               + ", after switching away and back " + juce::String (after, 6) + ")");
    check (module.getCurrentAlgorithm() == galactic, "Airwindows: the module runs the chosen algorithm");
}

std::vector<float> renderNote (IlanaSynthAudioProcessor& processor)
{
    processor.prepareToPlay (sr, block);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, block);
    for (int b = 0; b < (int) (0.5 * sr / block); ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < block; ++i)
        {
            out.push_back (buffer.getSample (0, i));
            out.push_back (buffer.getSample (1, i));
        }
    }
    return out;
}

// The module in a slot: assignFxSlot loads it, and each algorithm changes a
// played note at a non-trivial setting.
void runSlotTests()
{
    {
        IlanaSynthAudioProcessor processor;
        processor.assignFxSlot (1, 30);
        auto* slot = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_slot1"));
        check (slot != nullptr && slot->getIndex() == 30 && slot->getCurrentChoiceName() == "Airwindows"
                   && IlanaSynthAudioProcessor::numFxTypes >= 30,
               "Airwindows: assignFxSlot puts type 30, Airwindows, in the slot");

        // Saved and restored with the patch.
        setParam (processor, "fx_aw_algo", 5.0f);
        setParam (processor, "fx_aw_p2", 0.25f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check ((int) restored.apvts.getRawParameterValue ("fx_slot1")->load() == 30
                   && (int) restored.apvts.getRawParameterValue ("fx_aw_algo")->load() == 5
                   && std::abs (restored.apvts.getRawParameterValue ("fx_aw_p2")->load() - 0.25f) < 1.0e-6f,
               "Airwindows: the slot, algorithm and knobs survive a state round trip");
    }

    IlanaSynthAudioProcessor dryProcessor;
    const auto dry = renderNote (dryProcessor);
    auto dryPeak = 0.0f;
    for (const auto x : dry)
        dryPeak = std::max (dryPeak, std::abs (x));
    check (dryPeak > 0.01f, "Airwindows: the reference note sounds (" + juce::String (dryPeak, 3) + ")");

    juce::StringArray unchanged, broken;

    for (int a = 0; a < airwindows::count(); ++a)
    {
        IlanaSynthAudioProcessor processor;
        processor.assignFxSlot (1, 30);
        setParam (processor, "fx_aw_algo", (float) a);
        for (int k = 1; k <= 5; ++k)
            setParam (processor, "fx_aw_p" + juce::String (k), 0.3f);
        // The tape delays' Time runs the tape faster as it rises: at 0.3 the echo
        // lands after the half-second note, at 1 within it.
        const auto algorithmName = juce::String (airwindows::registry()[(size_t) a].name);
        if (algorithmName == "TapeDelay2" || algorithmName == "PitchDelay")
            setParam (processor, "fx_aw_p1", 1.0f);
        const auto wet = renderNote (processor);

        auto difference = 0.0f;
        auto finite = true;
        for (size_t i = 0; i < wet.size(); ++i)
        {
            finite = finite && std::isfinite (wet[i]);
            difference = std::max (difference, std::abs (wet[i] - dry[i]));
        }

        const auto name = juce::String (airwindows::registry()[(size_t) a].name);
        if (! finite)
            broken.add (name);
        else if (difference < 1.0e-3f)
            unchanged.add (name + " (" + juce::String (difference, 6) + ")");
    }

    // The category modules (types 32-41): each of their effects, in a slot.
    juce::StringArray categoryUnchanged;
    for (int c = 0; c < (int) airwindows::categoryModules().size(); ++c)
    {
        const auto& category = airwindows::categoryModules()[(size_t) c];
        const juce::String prefix = juce::String ("fx_") + category.id;
        for (int choice = 0; choice < (int) category.algorithms.size(); ++choice)
        {
            IlanaSynthAudioProcessor processor;
            processor.assignFxSlot (1, airwindows::firstCategoryFxType + c);
            setParam (processor, prefix + "_algo", (float) choice);
            for (int k = 1; k <= 5; ++k)
                setParam (processor, prefix + "_p" + juce::String (k), 0.3f);
            const auto algorithmName = juce::String (airwindows::registry()[(size_t) category.algorithms[(size_t) choice]].name);
            if (algorithmName == "TapeDelay2" || algorithmName == "PitchDelay")
                setParam (processor, prefix + "_p1", 1.0f);
            const auto wet = renderNote (processor);
            auto difference = 0.0f;
            auto finite = true;
            for (size_t i = 0; i < wet.size(); ++i)
            {
                finite = finite && std::isfinite (wet[i]);
                difference = std::max (difference, std::abs (wet[i] - dry[i]));
            }
            const auto what = juce::String (category.label) + "/" + algorithmName;
            if (! finite)
                broken.add (what);
            else if (difference < 1.0e-3f)
                categoryUnchanged.add (what);
        }
    }
    check (categoryUnchanged.isEmpty(), "Airwindows: every category module's effect changes the sound ("
                                            + categoryUnchanged.joinIntoString (", ") + ")");

    check (broken.isEmpty(), "Airwindows: every algorithm in a slot stays finite (" + broken.joinIntoString (", ") + ")");
    check (unchanged.isEmpty(), "Airwindows: every algorithm in a slot changes the sound at knobs 0.3 ("
                                    + unchanged.joinIntoString (", ") + ")");

    // An empty rack never runs the module: a fresh patch is unchanged by it.
    IlanaSynthAudioProcessor again;
    const auto repeat = renderNote (again);
    auto same = repeat.size() == dry.size();
    for (size_t i = 0; same && i < dry.size(); ++i)
        same = repeat[i] == dry[i];
    check (same, "Airwindows: a patch without the module renders as before (bit for bit)");
}

// AW Delay's tempo sync (airwindows/DelaySync.h): each echo with a time knob
// lands where the knob it is given says, at 44.1, 48 and 96 kHz; StarChild2
// has none; in a slot, SYNC is off in a fresh patch and on moves the echo.
void runDelaySyncTests()
{
    // The first echo of an impulse (after the dry click): its start in seconds.
    const auto firstEcho = [] (int algorithm, double rate, double seconds)
    {
        const auto& info = airwindows::registry()[(size_t) algorithm];
        const juce::String name (info.name);
        auto instance = info.create();
        instance->prepare (rate);
        std::array<float, 5> knobs {};
        for (int k = 0; k < info.numKnobs; ++k)
            knobs[(size_t) k] = info.knobs[k].defaultValue;
        if (name == "TapeDelay2" || name == "PitchDelay")
            knobs[1] = 0.0f; // no regen: one echo
        if (name == "PurestEcho")
            knobs = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // only the full-time tap
        if (name == "Doublelay")
            knobs = { 0.5f, 0.0f, 0.0f, 0.0f, 1.0f }; // no detune, no feedback, all wet
        for (int k = 0; k < 5; ++k)
            if (airwindows::delaysync::isTimeKnob (algorithm, k))
                knobs[(size_t) k] = airwindows::delaysync::knobFor (algorithm, seconds, rate);
        for (int k = 0; k < info.numKnobs; ++k)
            instance->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (knobs[(size_t) k]));

        const auto total = (int) (rate * (seconds + 0.5));
        std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
        for (int i = 0; i < 32; ++i)
            left[(size_t) i] = right[(size_t) i] = 0.8f;
        for (int start = 0; start < total; start += block)
            instance->process (left.data() + start, right.data() + start, std::min (block, total - start));

        const auto skip = (int) (rate * 0.03);
        auto peak = 0.0f;
        for (int i = skip; i < total; ++i)
            peak = std::max (peak, std::abs (left[(size_t) i]) + std::abs (right[(size_t) i]));
        for (int i = skip; i < total; ++i)
            if (std::abs (left[(size_t) i]) + std::abs (right[(size_t) i]) > peak * 0.25f)
                return i / rate;
        return -1.0;
    };

    juce::StringArray off;
    for (int a = 0; a < airwindows::count(); ++a)
    {
        if (! airwindows::delaysync::hasTime (a))
            continue;
        const juce::String name (airwindows::registry()[(size_t) a].name);
        // (PurestEcho's loop is samples, not seconds: 0.34 s at 96 kHz.)
        const std::vector<double> times = name == "PurestEcho" ? std::vector<double> { 0.1, 0.25 } : std::vector<double> { 0.125, 0.5, 0.9 };
        for (const auto rate : { 44100.0, 48000.0, 96000.0 })
            for (const auto seconds : times)
            {
                const auto heard = firstEcho (a, rate, seconds);
                if (std::abs (heard - seconds) > 0.004 + seconds * 0.02)
                    off.add (name + " " + juce::String (rate / 1000.0, 1) + "k " + juce::String (seconds, 3) + " s -> " + juce::String (heard, 4));
            }
    }
    check (off.isEmpty(), "AW Delay sync: each echo lands on the synced time (" + off.joinIntoString (", ") + ")");

    auto starChildHasTime = false, starChildFound = false;
    for (int a = 0; a < airwindows::count(); ++a)
        if (juce::String (airwindows::registry()[(size_t) a].name) == "StarChild2")
        {
            starChildFound = true;
            starChildHasTime = airwindows::delaysync::hasTime (a);
        }
    check (starChildFound && ! starChildHasTime, "AW Delay sync: StarChild2 has no time knob to sync");

    // In a slot: off by default, so an AW Delay renders as before; on, the echo moves.
    const auto awDelayType = [] {
        for (int c = 0; c < (int) airwindows::categoryModules().size(); ++c)
            if (juce::String (airwindows::categoryModules()[(size_t) c].id) == "awdelay")
                return airwindows::firstCategoryFxType + c;
        return -1;
    }();
    const auto render = [awDelayType] (int sync)
    {
        IlanaSynthAudioProcessor processor;
        processor.assignFxSlot (1, awDelayType);
        setParam (processor, "fx_awdelay_algo", 0.0f); // TapeDelay2
        setParam (processor, "fx_awdelay_p1", 1.0f);
        if (sync >= 0)
            setParam (processor, "fx_awdelay_sync", (float) sync);
        return renderNote (processor);
    };
    IlanaSynthAudioProcessor fresh;
    check (fresh.apvts.getRawParameterValue ("fx_awdelay_sync")->load() < 0.5f
               && (int) fresh.apvts.getRawParameterValue ("fx_awdelay_div")->load() == 3,
           "AW Delay sync: off in a fresh patch, DIV 1/8");
    const auto untouched = render (-1), syncedOff = render (0), syncedOn = render (1);
    auto same = untouched.size() == syncedOff.size(), moved = false;
    for (size_t i = 0; i < untouched.size() && i < syncedOn.size(); ++i)
    {
        same = same && untouched[i] == syncedOff[i];
        moved = moved || std::abs (untouched[i] - syncedOn[i]) > 1.0e-3f;
    }
    check (awDelayType > 0 && same, "AW Delay sync: SYNC off renders bit for bit as before");
    check (moved, "AW Delay sync: SYNC on changes the echo");
}

// ILANA_AW_PROBE=<name>: that algorithm's peaks with each knob at 0 and 1
// in turn (the others at their defaults), for each input.
void probe (const juce::String& name)
{
    for (int a = 0; a < airwindows::count(); ++a)
    {
        if (name != airwindows::registry()[(size_t) a].name)
            continue;

        const auto& info = airwindows::registry()[(size_t) a];
        // ILANA_AW_PROBE_VALUES="0.7,0.8": other knob values to try.
        std::vector<float> values { 0.0f, 1.0f };
        if (const auto list = juce::SystemStats::getEnvironmentVariable ("ILANA_AW_PROBE_VALUES", ""); list.isNotEmpty())
        {
            values.clear();
            for (const auto& token : juce::StringArray::fromTokens (list, ",", ""))
                values.push_back (token.getFloatValue());
        }
        for (int k = -1; k < info.numKnobs; ++k)
            for (const auto value : values)
            {
                auto knobs = defaultsOf (a);
                if (k >= 0)
                    knobs[(size_t) k] = value;
                juce::String line (k < 0 ? juce::String ("defaults") : juce::String (info.knobs[k].name) + "=" + juce::String (value, 2));
                for (const auto input : { Input::sine, Input::noise, Input::silence })
                {
                    const auto result = run (a, knobs, input);
                    line << "  " << inputName (input) << " peak " << juce::String (result.peak, 5) << " tail " << juce::String (result.tailPeak, 5)
                         << (result.finite ? "" : " NOT FINITE");
                }
                std::cout << line << std::endl;
                if (k < 0)
                    break;
            }
    }
}
} // namespace AirwindowsTests

void runAirwindowsTests()
{
    if (const auto name = juce::SystemStats::getEnvironmentVariable ("ILANA_AW_PROBE", ""); name.isNotEmpty())
    {
        AirwindowsTests::probe (name);
        return;
    }

    AirwindowsTests::runRegistryTests();
    AirwindowsTests::runAlgorithmTests();
    AirwindowsTests::runLowRateTests();
    AirwindowsTests::runResetTest();
    AirwindowsTests::runSlotTests();
    AirwindowsTests::runDelaySyncTests();
}
