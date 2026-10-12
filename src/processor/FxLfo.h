#pragma once

#include <array>
#include <cmath>

#include <juce_core/juce_core.h>

#include "../dsp/LfoShape.h"

// The FX patcher's LFO boxes (ilana, 2026-10-12: "add lfo effects module to
// modulate fx in the patcher"). An LFO box sits on the PATCH tab's canvas and
// its cables go to one effect box each, moving one or two of that effect's
// knobs. A cable belongs to a slot, not to an effect type, so two Delays can
// move differently (the mod matrix's FX destinations move every instance of
// a type at once).
//
// Safety: an LFO never writes a parameter. It adds an offset to what the
// running slot reads (IlanaSynthAudioProcessor::readParam, inside runFxSlot),
// in the parameter's 0..1 range and clamped there, so every value it can make
// is one the knob can be turned to; take the cable away and the slot reads the
// knob as it is. Only continuous parameters are offered (no switches, menus or
// algorithm choices: an Airwindows algorithm must not be made on the audio
// thread). Loops stay bounded by the patcher's loop guard and each effect's
// own limits (tests/FxLfoTests.inc runs every effect type in a loop with an
// LFO on every knob it offers).
//
// The shapes and their random state are the synth's own (LfoShape.h). The
// LFOs step every `chunk` samples: the effects read their knobs once per call,
// so the rack runs in chunks of that size while any LFO cable is in.
namespace FxLfo
{
inline constexpr int maxLfos = 8;
inline constexpr int maxRoutes = 32;
inline constexpr int maxTargets = 2; // per cable: one LFO into one effect moves up to two knobs
inline constexpr int chunk = 128;
inline constexpr float minRateHz = 0.02f;
inline constexpr float maxRateHz = 20.0f;

// The shapes offered, by their index in the synth's LFO shape list (the ones
// that need no table of their own: Draw, Steps and Curve are a patch's LFO
// tables, the physics shapes need note kicks).
inline constexpr std::array<int, 9> shapes { LfoShapes::Sine, LfoShapes::Triangle, LfoShapes::SawUp, LfoShapes::SawDown,
                                             LfoShapes::Square, LfoShapes::SampleHold, LfoShapes::SmoothRandom,
                                             LfoShapes::Drunk, LfoShapes::Chaos };

inline juce::StringArray shapeNames()
{
    return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random", "Drunk", "Chaos" };
}

inline int shapeOf (int choice) { return shapes[(size_t) juce::jlimit (0, (int) shapes.size() - 1, choice)]; }

// Tempo divisions: the synth's list with three longer ones in front (a slow
// sweep across a phrase is what FX modulation is mostly for).
inline juce::StringArray divisionNames()
{
    return { "8 Bars", "4 Bars", "2 Bars", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
}

inline double divisionBeats (int index)
{
    static constexpr double beats[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

inline constexpr int numDivisions = 14;
inline constexpr int defaultDivision = 3; // 1/1: one bar

struct Settings
{
    int shape = 0;           // a choice in `shapes`
    bool sync = false;       // the tempo decides (division), not rateHz
    float rateHz = 0.5f;
    int division = defaultDivision;
    float depth = 1.0f;      // 0..1, scales every cable from it
    bool bipolar = true;     // -1..1 around the knob; off: 0..1, up from it
    float phase = 0.0f;      // 0..1, where the cycle starts (with SYNC, against the bar)

    bool operator== (const Settings& o) const
    {
        return shape == o.shape && sync == o.sync && rateHz == o.rateHz && division == o.division && depth == o.depth
               && bipolar == o.bipolar && phase == o.phase;
    }
    bool operator!= (const Settings& o) const { return ! (*this == o); }

    // Everything in range (a damaged or hand-edited patch).
    Settings sanitised() const
    {
        Settings s = *this;
        const auto finite = [] (float v, float fallback) { return std::isfinite (v) ? v : fallback; };
        s.shape = juce::jlimit (0, (int) shapes.size() - 1, shape);
        s.rateHz = juce::jlimit (minRateHz, maxRateHz, finite (rateHz, 0.5f));
        s.division = juce::jlimit (0, numDivisions - 1, division);
        s.depth = juce::jlimit (0.0f, 1.0f, finite (depth, 1.0f));
        s.phase = juce::jlimit (0.0f, 1.0f, finite (phase, 0.0f));
        return s;
    }
};

inline juce::String rateText (const Settings& s)
{
    if (s.sync)
        return divisionNames()[s.division];
    return s.rateHz < 1.0f ? juce::String (s.rateHz, 2) + " Hz" : juce::String (s.rateHz, 1) + " Hz";
}

// One LFO's running state (audio thread). Its own random generator, seeded by
// its number, so a render is the same every time and no other random
// sequence in the synth shifts.
struct State
{
    double phase = 0.0;      // 0..1 within the cycle
    LfoChaos chaos;
    float hold = 0.0f;       // Sample & Hold's value for this cycle
    int lastShape = -1;
    juce::Random random;

    void reset (int number)
    {
        random.setSeed (0x5f1f0 + number * 7919);
        chaos.reset (random);
        hold = random.nextFloat() * 2.0f - 1.0f;
        phase = 0.0;
        lastShape = -1;
    }

    // A new cycle began: the random shapes pick their next value.
    void onCycle (int shape)
    {
        if (shape == LfoShapes::SampleHold)
            hold = random.nextFloat() * 2.0f - 1.0f;
        else if (shape == LfoShapes::SmoothRandom || shape == LfoShapes::Drunk)
            chaos.onCycle (shape, random);
    }

    // The shape's value now, -1..1.
    float shapeValue (int shape) const
    {
        float v = 0.0f;
        if (shape == LfoShapes::SampleHold)
            v = hold;
        else if (LfoShapes::isStateful (shape))
            v = chaos.value (shape, phase);
        else
            v = lfoShapeValue (shape, phase);
        return std::isfinite (v) ? juce::jlimit (-1.0f, 1.0f, v) : 0.0f;
    }

    // Moves on by `samples`. `cyclesPerSample` from the rate (free) or the
    // tempo; `syncedPhase` >= 0 puts the phase there instead (the host is
    // playing: the cycle follows its bar position).
    void advance (const Settings& settings, double cyclesPerSample, int samples, double syncedPhase)
    {
        const auto shape = shapeOf (settings.shape);
        if (shape != lastShape)
        {
            if (shape == LfoShapes::Chaos || LfoShapes::isStateful (shape))
                chaos.reset (random);
            lastShape = shape;
        }
        if (shape == LfoShapes::Chaos)
            for (int i = 0; i < samples; ++i)
                chaos.advance (cyclesPerSample);

        auto next = syncedPhase >= 0.0 ? syncedPhase : phase + cyclesPerSample * (double) samples;
        if (! std::isfinite (next))
            next = 0.0;
        const auto wrapped = next - std::floor (next);
        // (Synced: a jump backwards, or a whole cycle passed, is a new cycle.)
        if (syncedPhase >= 0.0 ? (wrapped < phase - 1.0e-9 || cyclesPerSample * (double) samples >= 1.0) : next >= 1.0)
            onCycle (shape);
        phase = wrapped;
    }
};

// The value an LFO puts on its cables: the shape, made unipolar if asked, times DEPTH.
inline float output (const Settings& settings, float shapeValue)
{
    const auto v = settings.bipolar ? shapeValue : 0.5f * (shapeValue + 1.0f);
    return juce::jlimit (-1.0f, 1.0f, v * settings.depth);
}

// A short name for a knob on a cable's tag: "Delay Time" on a Delay is "TIME".
inline juce::String shortName (const juce::String& parameterName, const juce::String& effectName)
{
    auto name = parameterName.trim();
    for (const auto& prefix : { effectName, effectName.upToFirstOccurrenceOf (" ", false, false) })
        if (prefix.isNotEmpty() && name.startsWithIgnoreCase (prefix + " ") && name.length() > prefix.length() + 1)
        {
            name = name.substring (prefix.length() + 1).trim();
            break;
        }
    if (name.startsWithIgnoreCase ("FX ") && name.length() > 3) // ("Filter FX Cutoff")
        name = name.substring (3).trim();
    return name.toUpperCase();
}
} // namespace FxLfo
