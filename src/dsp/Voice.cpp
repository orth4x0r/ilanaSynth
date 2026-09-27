#include "Voice.h"

#include <array>
#include <cmath>

namespace
{
int chordInterval (int mode, int voiceIndex)
{
    switch (mode)
    {
        case 1:  { static const int table[] = { 0, 12 }; return table[voiceIndex % 2]; }
        case 2:  { static const int table[] = { 0, 7 }; return table[voiceIndex % 2]; }
        case 3:  { static const int table[] = { 0, 7, 12 }; return table[voiceIndex % 3]; }
        case 4:  { static const int table[] = { 0, 4, 7, 12 }; return table[voiceIndex % 4]; }
        case 5:  { static const int table[] = { 0, 3, 7, 12 }; return table[voiceIndex % 4]; }
        case 6:  { static const int table[] = { 0, 5, 7, 12 }; return table[voiceIndex % 4]; }
        default: return 0;
    }
}

// Unison voices are spread over -1..1; each mode maps that position to a
// pitch offset. Classic is linear; Hypersaw bunches voices near the centre
// like the JP-8000 supersaw; Octaves and Fifths stack intervals too.
double unisonDetunePosition (int mode, float position)
{
    if (mode == UnisonMode::Hypersaw)
        return (position >= 0.0f ? 1.0 : -1.0) * std::pow ((double) std::abs (position), 1.5);

    return (double) position;
}

int unisonStackInterval (int mode, int voiceIndex, int numVoices)
{
    if (numVoices < 2)
        return 0;

    if (mode == UnisonMode::Octaves)
    {
        static const int table[] = { 0, 12, -12 };
        return table[voiceIndex % 3];
    }

    if (mode == UnisonMode::Fifths)
    {
        static const int table[] = { 0, 7, 12 };
        return table[voiceIndex % 3];
    }

    return 0;
}

float lfoShapeAt (int shape, double phase, float hold, const float* steps, const float* custom, int customSize)
{
    switch (shape)
    {
        case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        case 1: return (float) (1.0 - 4.0 * std::abs (phase - 0.5));
        case 2: return (float) (2.0 * phase - 1.0);
        case 3: return (float) (1.0 - 2.0 * phase);
        case 4: return phase < 0.5 ? 1.0f : -1.0f;
        case 5: return hold;
        case 7: return steps != nullptr ? steps[juce::jlimit (0, 15, (int) (phase * 16.0))] : 0.0f;
        default:
        {
            // Draw (6) and any table-driven shape.
            if (custom == nullptr || customSize <= 0)
                return 0.0f;

            const auto position = phase * (double) customSize;
            const auto index = (int) position % customSize;
            const auto next = (index + 1) % customSize;
            const auto frac = (float) (position - std::floor (position));
            return custom[index] + (custom[next] - custom[index]) * frac;
        }
    }
}

// Scales for explicit destinations: how far a full-depth modulation moves
// the target, in the target's own units.
constexpr float detuneRange = 50.0f;     // cents
constexpr float driveRange = 9.0f;       // drive multiplier
constexpr float envAmountRange = 5.0f;   // octaves
constexpr float timeOctaves = 5.0f;      // envelope times scale by 2^(mod * 5)
constexpr float lfoRateOctaves = 4.0f;
} // namespace

Voice::Voice()
{
#if ILANA_FINGERPRINT_BUILD
    random.setSeed (12345);
    driftRandom.setSeed (54321);
#endif
}

void Voice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate <= 0.0)
        return;

    sampleRate = newRate;

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        frameSmooth[osc].reset (newRate, 0.02);
        levelSmooth[osc].reset (newRate, 0.02);
        oscEnableSmooth[osc].reset (newRate, 0.02);
    }
    noiseSmooth.reset (newRate, 0.02);
    subOscLevelSmooth.reset (newRate, 0.02);
    subOscEnableSmooth.reset (newRate, 0.02);
    subOsc.setSampleRate (newRate);

    ampEnv.setSampleRate (newRate);
    filterEnv.setSampleRate (newRate);
    filter2Env.setSampleRate (newRate);
    modEnv.setSampleRate (newRate);
    env4.setSampleRate (newRate);
    for (auto& env : extraEnvs)
        env.setSampleRate (newRate);
    envMseg.prepare (newRate);

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        for (int u = 0; u < VoiceParams::maxUnison; ++u)
            oscUnison[osc][u].setSampleRate (newRate);

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
        {
            stringFor (osc, u).prepare (newRate);
            sampleUnison[osc][u].prepare (newRate);
            grains[osc][u].prepare (newRate);
        }
    }

    resonatorL.prepare (newRate);
    resonatorR.prepare (newRate);
    materialBodyL.prepare (newRate);
    materialBodyR.prepare (newRate);

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R,
                          &bothFilter1L, &bothFilter1R, &bothFilter2L, &bothFilter2R })
        filter->prepare (newRate);
}

void Voice::syncSamplePlayers()
{
    const auto setup = [] (const VoiceParams::OscParams& osc, SamplePlayer* players, GranularOsc* clouds,
                           float startMod, float endMod)
    {
        const auto start = juce::jlimit (0.0f, 0.98f, osc.sampleStart + startMod);
        const auto end = juce::jlimit (start + 0.01f, 1.0f, osc.sampleEnd + endMod);

        SamplePlayer::Params sampleParams;
        sampleParams.sample = osc.sample;
        sampleParams.loop = osc.sampleLoop;
        sampleParams.reverse = osc.sampleReverse;
        sampleParams.start = start;
        sampleParams.end = end;
        sampleParams.fadeIn = osc.sampleFadeIn;
        sampleParams.fadeOut = osc.sampleFadeOut;

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            players[u].setParams (sampleParams);

        if (osc.granularMode)
        {
            GranularOsc::Params grainParams;
            grainParams.sample = osc.sample;
            grainParams.position = juce::jlimit (0.0f, 1.0f, osc.sampleStart + startMod);
            grainParams.sizeMs = osc.grainSizeMs;
            grainParams.density = osc.grainDensity;
            grainParams.spray = osc.grainSpray;
            grainParams.pitchSpray = osc.grainPitch;
            grainParams.spread = osc.grainSpread;
            grainParams.reverse = osc.sampleReverse;

            for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
                clouds[u].setParams (grainParams);
        }
    };

    constexpr Mod::Destination startDestinations[] { Mod::Destination::Osc1SampleStart,
                                                      Mod::Destination::Osc2SampleStart,
                                                      Mod::Destination::SubSampleStart,
                                                      Mod::Destination::Osc4SampleStart, Mod::Destination::Osc5SampleStart,
                                                      Mod::Destination::Osc6SampleStart };
    constexpr Mod::Destination endDestinations[] { Mod::Destination::Osc1SampleEnd,
                                                    Mod::Destination::Osc2SampleEnd,
                                                    Mod::Destination::SubSampleEnd,
                                                    Mod::Destination::Osc4SampleEnd, Mod::Destination::Osc5SampleEnd,
                                                    Mod::Destination::Osc6SampleEnd };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (osc < 3 || params.oscillatorEnabled[osc]
            || oscEnableSmooth[osc].getCurrentValue() > 0.0005f)
            setup (params.oscillators[osc], sampleUnison[osc], grains[osc],
                   blockMod (startDestinations[osc]), blockMod (endDestinations[osc]));
}

void Voice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const auto mono = monoPending;
    const auto legato = mono && monoLegato;
    const auto keepRunning = mono && monoKeepRunning;
    const auto glide = ! mono || monoGlide;
    monoPending = false;
    monoKeepRunning = false;

    baseFrequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    // Stretch tuning (Railsback): bass a little flat, treble sharp, about
    // +/-35 cents at the ends of the keyboard when fully on.
    if (params.stretch > 0.0f)
    {
        const auto distance = (double) (midiNoteNumber - 60) / 40.0;
        const auto cents = (double) params.stretch * 35.0 * (distance < 0.0 ? -1.0 : 1.0) * distance * distance;
        baseFrequency *= std::exp2 (cents / 1200.0);
    }

    keyTrackValue = juce::jlimit (-1.0f, 1.0f, (float) (midiNoteNumber - 60) / 48.0f);
    keyTrackOctaves = (float) (midiNoteNumber - 60) / 12.0f;

    if (! hasPlayedNote || ! glide)
    {
        currentFrequency = baseFrequency;
        hasPlayedNote = true;
    }

    pitchWheelMoved (currentPitchWheelPosition);

    // Legato: the note just changes pitch; envelopes, phases and filters
    // carry on from where they are.
    if (legato)
        return;

    velocityLevel = velocity;
    noteHeld = true;

    // Per-voice LFOs restart with each articulated note.
    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        lfoPhases[lfo] = (double) juce::jlimit (0.0f, 1.0f, params.lfos[lfo].startPhase);
        // LFO 5-16 use their own generator so the voice's random sequence
        // (pans, drift, noise) stays as it was before the LFO pool.
        auto& lfoRandom = lfo < 4 ? random : lfoPoolRandom;
        lfoHolds[lfo] = lfoRandom.nextFloat() * 2.0f - 1.0f;
        lfoChaos[lfo].reset (lfoRandom);
        lfoChaos[lfo].resetPhysics (params.lfos[lfo].shape, params.lfos[lfo].physA);
        if (params.lfos[lfo].shape == LfoShapes::Pendulum && params.lfos[lfo].kick)
            lfoChaos[lfo].kick (velocity);
    }

    if (keepRunning)
    {
        // Mono retrigger: restart envelopes from their current level and keep
        // oscillator phases and filter state so there is no click.
        ampEnv.retrigger();
        filterEnv.retrigger();
        filter2Env.retrigger();
        modEnv.retrigger();
        env4.retrigger();
        for (auto& env : extraEnvs)
            env.retrigger();
        envMseg.reset();
        return;
    }

    randomValue = random.nextFloat() * 2.0f - 1.0f;
    voicePan = random.nextFloat() * 2.0f - 1.0f;

    for (int u = 0; u < VoiceParams::maxUnison; ++u)
    {
        // The first eight keep their original spacing (eighths of a cycle) so
        // existing patches start the same; voices 9-16 sit in between.
        const auto basePhase = u < 8 ? (double) u / 8.0 : (double) (u - 8) / 8.0 + 1.0 / 16.0;
        const auto jitter = (double) random.nextFloat() * (double) params.unisonRandom;

        auto phase = basePhase + jitter;
        phase -= std::floor (phase);

        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
            oscUnison[osc][u].resetPhase (phase);
    }

    resonatorL.reset();
    resonatorR.reset();
    materialBodyL.reset();
    materialBodyR.reset();
    bodyStrikePending = true;
    bodyTailSamplesRemaining = params.resonatorOn && params.bodyType != 0
                                   ? (int) (sampleRate * juce::jmin (10.0,
                                       1.5 * (0.08 + 7.92 * (double) params.resonatorDecay * (double) params.resonatorDecay)))
                                   : 0;

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];

        if (! settings.stringMode || ! params.oscillatorEnabled[osc])
            continue;

        const auto pitch = settings.tuneMode == OscTuning::Semitones
                               ? currentFrequency
                                     * std::exp2 ((settings.semitones + (osc == 2 ? params.subOctaveOffset : 0)
                                                   + settings.cents / 100.0) / 12.0)
                               : oscFrequencyFactor (settings);

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
        {
            auto& string = stringFor (osc, u);
            string.setFrequency (pitch);
            configureString (string, settings);
            string.trigger (velocity);
        }
    }

    syncSamplePlayers();

    const auto bufferedCount = [] (int unison) { return juce::jlimit (1, VoiceParams::maxBufferedUnison, unison); };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];

        if (settings.sampleMode && params.oscillatorEnabled[osc] && settings.sample != nullptr)
            for (int u = 0; u < bufferedCount (settings.unison); ++u)
            {
                sampleUnison[osc][u].trigger();
                grains[osc][u].reset ((juce::uint32) (midiNoteNumber * 7919 + u * 104729 + random.nextInt()));
            }

        lastStringMode[osc] = settings.stringMode;
        lastSampleMode[osc] = settings.sampleMode;
        previousOsc[osc] = 0.0f;
    }

    driftValue = driftRandom.nextFloat() * 2.0f - 1.0f;
    driftTarget = driftValue;

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R,
                          &bothFilter1L, &bothFilter1R, &bothFilter2L, &bothFilter2R })
        filter->reset();

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        frameSmooth[osc].setCurrentAndTargetValue (params.oscillators[osc].frame);
        levelSmooth[osc].setCurrentAndTargetValue (params.oscillators[osc].level);
        oscEnableSmooth[osc].setCurrentAndTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
    }
    noiseSmooth.setCurrentAndTargetValue (params.noiseLevel);
    subOscLevelSmooth.setCurrentAndTargetValue (params.subOscLevel);
    subOscEnableSmooth.setCurrentAndTargetValue (params.subOscEnabled ? 1.0f : 0.0f);

    ampEnv.noteOn();
    filterEnv.noteOn();
    filter2Env.noteOn();
    modEnv.noteOn();
    env4.noteOn();
    for (auto& env : extraEnvs)
        env.noteOn();

    envMseg.reset();
    msegEnvValue = 0.0f;
    fmNoiseState = 0.0f;

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        feedbackHistory[osc] = feedbackFiltered[osc] = 0.0f;
}

void Voice::stopNote (float, bool allowTailOff)
{
    // The synth hard-stops a voice before reusing it; a mono note change
    // keeps the voice sounding instead.
    if (! allowTailOff && monoPending && monoKeepRunning)
        return;

    noteHeld = false;

    if (allowTailOff)
    {
        if (params.resonatorOn && params.bodyType != 0)
            bodyTailSamplesRemaining = (int) (sampleRate * juce::jmin (10.0,
                1.5 * (0.08 + 7.92 * (double) params.resonatorDecay * (double) params.resonatorDecay)));
        ampEnv.noteOff();
        filterEnv.noteOff();
        filter2Env.noteOff();
        modEnv.noteOff();
        env4.noteOff();
        for (auto& env : extraEnvs)
            env.noteOff();
    }
    else
    {
        bodyTailSamplesRemaining = 0;
        ampEnv.reset();
        filterEnv.reset();
        filter2Env.reset();
        modEnv.reset();
        env4.reset();
        for (auto& env : extraEnvs)
            env.reset();
        extraEnvValues.fill (0.0f); // the ENV page monitors these
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        lastFilterValue = 0.0f;
        lastModValue = 0.0f;
        clearCurrentNote();
    }
}

void Voice::pitchWheelMoved (int newValue)
{
    bendSemitones = ((double) newValue - 8192.0) / 8192.0 * (double) params.pitchBendRange;
}

void Voice::controllerMoved (int controllerNumber, int newValue)
{
    const auto value = (float) newValue / 127.0f;

    if (controllerNumber == 1)
        modWheel = value;
    else if (controllerNumber == 11)
        expressionValue = value;
}

void Voice::channelPressureChanged (int newValue)
{
    aftertouchValue = (float) newValue / 127.0f;
}

void Voice::aftertouchChanged (int newValue)
{
    aftertouchValue = (float) newValue / 127.0f;
}

float Voice::voiceLfoValue (int lfo) const
{
    const auto& lfoParams = params.lfos[lfo];

    if (LfoShapes::isStateful (lfoParams.shape))
        return lfoChaos[lfo].value (lfoParams.shape, lfoPhases[lfo]);

    return lfoShapeAt (lfoParams.shape, lfoPhases[lfo], lfoHolds[lfo], lfoParams.steps,
                       lfoParams.custom, lfoParams.customSize);
}

void Voice::advanceVoiceLfos()
{
    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        if (! params.lfos[lfo].perVoice)
            continue;

        const auto shape = params.lfos[lfo].shape;

        if (shape == LfoShapes::Chaos)
            lfoChaos[lfo].advance (lfoIncrements[lfo]);
        else if (LfoShapes::isPhysics (shape))
            lfoChaos[lfo].advancePhysics (shape, lfoIncrements[lfo], params.lfos[lfo].physA, params.lfos[lfo].physB);

        lfoValues[lfo] = voiceLfoValue (lfo);

        auto next = lfoPhases[lfo] + lfoIncrements[lfo];

        if (next >= 1.0)
        {
            next -= std::floor (next);
            auto& lfoRandom = lfo < 4 ? random : lfoPoolRandom;
            lfoHolds[lfo] = lfoRandom.nextFloat() * 2.0f - 1.0f;

            if (LfoShapes::isStateful (shape) && ! LfoShapes::isPhysics (shape))
                lfoChaos[lfo].onCycle (shape, lfoRandom);
        }

        lfoPhases[lfo] = next;
    }
}

void Voice::reExcite (float level)
{
    if (! isVoiceActive() || ! isKeyDown())
        return;

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillators[osc].stringMode && params.oscillatorEnabled[osc])
            for (int u = 0; u < juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison); ++u)
                stringFor (osc, u).trigger (juce::jlimit (0.0f, 1.0f, level * velocityLevel * 1.25f));
}

void Voice::evaluateMods (float* mods, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    for (int d = 0; d < params.numActiveDestinations; ++d)
        mods[params.activeDestinations[d]] = 0.0f;

    if (params.anyExtendedFmMods)
        fmCellMods.fill (0.0f);

    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];
        auto* target = Mod::isExplicitDestination (slot.destination) ? &mods[slot.destination] : nullptr;

        if (target == nullptr && params.anyExtendedFmMods)
            if (const auto cell = Mod::extendedFmCellFor (slot.destination); cell >= 0)
                target = &fmCellMods[(size_t) cell];

        if (target == nullptr)
            continue;

        auto value = Mod::shape (slot, sourceValue (slot.source, sampleIndex, ampValue, filterValue,
                                                    filter2Value, modValue, env4Value));

        if (slot.aux != Mod::Source::None)
            value *= Mod::auxScale (slot.aux, sourceValue (slot.aux, sampleIndex, ampValue, filterValue,
                                                            filter2Value, modValue, env4Value));

        *target += slot.depth * value;
    }
}

void Voice::updateUnisonLayout()
{
    const auto layout = [] (const VoiceParams::OscParams& osc, int numUnison, int chord, float detuneCents,
                            float blend, double* offsets, float* gains)
    {
        auto power = 0.0f;
        const auto centreLow = (numUnison - 1) / 2;
        const auto centreHigh = numUnison / 2;

        for (int u = 0; u < numUnison; ++u)
        {
            const auto position = numUnison > 1
                                      ? juce::jlimit (-1.0f, 1.0f, (float) u / (float) (numUnison - 1) * 2.0f - 1.0f)
                                      : 0.0f;

            offsets[u] = (double) chordInterval (chord, u)
                         + (double) unisonStackInterval (osc.unisonMode, u, numUnison)
                         + unisonDetunePosition (osc.unisonMode, position) * (double) detuneCents / 100.0;

            const auto isCentre = u == centreLow || u == centreHigh;
            gains[u] = isCentre ? 1.0f : juce::jlimit (0.0f, 1.0f, blend);
            power += gains[u] * gains[u];
        }

        const auto norm = 1.0f / std::sqrt (juce::jmax (1.0e-6f, power));

        for (int u = 0; u < numUnison; ++u)
            gains[u] *= norm;
    };

    constexpr Mod::Destination detuneDestinations[] { Mod::Destination::Osc1Detune,
                                                       Mod::Destination::Osc2Detune,
                                                       Mod::Destination::SubDetune,
                                                       Mod::Destination::Osc4Detune, Mod::Destination::Osc5Detune,
                                                       Mod::Destination::Osc6Detune };
    constexpr Mod::Destination blendDestinations[] { Mod::Destination::Osc1Blend,
                                                      Mod::Destination::Osc2Blend,
                                                      Mod::Destination::SubBlend,
                                                      Mod::Destination::Osc4Blend, Mod::Destination::Osc5Blend,
                                                      Mod::Destination::Osc6Blend };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        if (osc >= 3 && numOscUnison[osc] == 0)
            continue;
        const auto& settings = params.oscillators[osc];
        layout (settings, numOscUnison[osc], settings.chord,
                juce::jlimit (0.0f, 100.0f, settings.detuneCents + blockMod (detuneDestinations[osc]) * detuneRange),
                settings.unisonBlend + blockMod (blendDestinations[osc]),
                unisonOffset[osc], unisonGains[osc]);
    }
}

void Voice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! hasActiveAmpEnvelope())
    {
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        clearCurrentNote();
        return;
    }

    // Block-rate modulation first: everything below can use it. The
    // per-sample values only clear destinations that are routed, so wipe
    // them all here too: a routing removed since the last block must not
    // leave its final value stuck on the target.
    std::fill (blockMods.begin(), blockMods.end(), 0.0f);
    std::fill (sampleMods.begin(), sampleMods.end(), 0.0f);

    if (params.numModSlots > 0)
    {
        for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
            if (params.lfos[lfo].perVoice)
                lfoValues[lfo] = voiceLfoValue (lfo);

        evaluateMods (blockMods.data(), 0, lastAmpValue, lastFilterValue, lastFilter2Value, lastModValue, lastEnv4Value);
    }

    const auto scaleTime = [] (float seconds, float mod)
    {
        return mod != 0.0f ? seconds * std::exp2 (juce::jlimit (-2.0f, 2.0f, mod) * timeOctaves) : seconds;
    };

    const auto modulatedEnvelope = [&scaleTime] (TensionAdsr::Parameters env, float a, float d, float s, float r)
    {
        env.attack = scaleTime (env.attack, a);
        env.decay = scaleTime (env.decay, d);
        env.sustain = juce::jlimit (0.0f, 1.0f, env.sustain + s);
        env.release = scaleTime (env.release, r);
        return env;
    };

    // Rate key scaling (M5): every stage time shrinks up the keyboard, as a
    // struck or plucked note dies faster the higher it is.
    const auto keyScaled = [this] (TensionAdsr::Parameters env, int index)
    {
        const auto rate = params.envKeyRate[(size_t) index];

        if (rate == 0.0f)
            return env;

        const auto factor = std::exp2 (-rate * keyTrackOctaves);
        env.attack *= factor;
        env.decay *= factor;
        env.release *= factor;
        env.delay *= factor;
        env.hold *= factor;
        return env;
    };

    using D = Mod::Destination;
    ampEnv.setParameters (keyScaled (modulatedEnvelope (params.ampEnv, blockMod (D::AmpAttack), blockMod (D::AmpDecay),
                                                        blockMod (D::AmpSustain), blockMod (D::AmpRelease)), 0));
    filterEnv.setParameters (keyScaled (modulatedEnvelope (params.filterEnv, blockMod (D::FeAttack), blockMod (D::FeDecay),
                                                           blockMod (D::FeSustain), blockMod (D::FeRelease)), 1));
    filter2Env.setParameters (keyScaled (modulatedEnvelope (params.filter2Env, blockMod (D::F2eAttack), blockMod (D::F2eDecay),
                                                            blockMod (D::F2eSustain), blockMod (D::F2eRelease)), 2));
    modEnv.setParameters (keyScaled (modulatedEnvelope (params.modEnv, blockMod (D::MeAttack), blockMod (D::MeDecay),
                                                        blockMod (D::MeSustain), blockMod (D::MeRelease)), 3));
    env4.setParameters (keyScaled (modulatedEnvelope (params.env4, blockMod (D::E4Attack), blockMod (D::E4Decay),
                                                      blockMod (D::E4Sustain), blockMod (D::E4Release)), 4));
    for (int env = 0; env < (int) extraEnvs.size(); ++env)
        if (params.extraEnvNeeded[(size_t) env])
            extraEnvs[(size_t) env].setParameters (keyScaled (params.extraEnvs[(size_t) env], env + 5));

    if (params.msegEnvNeeded)
        envMseg.setParams (params.msegShape.levels, params.msegShape.times,
                           params.msegShape.rateHz, params.msegShape.loop);

    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        lfoIncrements[lfo] = params.lfos[lfo].baseIncrement
                             * std::exp2 ((double) blockMod (Mod::lfoRateDestinationFor (lfo)) * (double) lfoRateOctaves);

        // Key tracked: RATE 4 Hz runs at the note's own pitch, 8 Hz an
        // octave above, 2 Hz an octave below.
        if (params.lfos[lfo].keyTrack)
            lfoIncrements[lfo] = juce::jmin (0.45, lfoIncrements[lfo] * currentFrequency / 4.0);
    }

    // Level key scaling: KEY LVL 1 is +6 dB per octave above C3 (and -6 dB
    // per octave below); negative tilts the other way. Modulating operators
    // use it to keep FM brightness even across the keyboard. Per block, so
    // turning or modulating it reaches held notes.
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto keyLevel = params.oscillators[osc].keyLevel;
        keyLevelGain[osc] = keyLevel != 0.0f
                                ? juce::jlimit (0.0f, 4.0f, juce::Decibels::decibelsToGain (keyLevel * 6.0f * keyTrackOctaves, -120.0f))
                                : 1.0f;
    }

    glideCoeff = params.glideTime > 0.001f
                     ? 1.0f - std::exp (-1.0f / (float) (params.glideTime * sampleRate))
                     : 1.0f;

    const auto unisonLimit = [this] (const VoiceParams::OscParams& osc)
    {
        const auto normalLimit = (osc.stringMode || osc.sampleMode) ? VoiceParams::maxBufferedUnison
                                                                    : VoiceParams::maxUnison;
        return params.quality == 0 ? juce::jmin (4, normalLimit) : normalLimit;
    };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];
        numOscUnison[osc] = osc >= 3 && ! params.oscillatorEnabled[osc]
                                  && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f
                                ? 0 : juce::jlimit (1, unisonLimit (settings), settings.unison);

        // A grand's lowest notes have one string each, the low bass two,
        // and three start around the tenor: with the register map on, a
        // hammered note uses no more strings than that (detuned unison
        // strings in the bass beat audibly, which a real one cannot).
        if (settings.stringMode && settings.registerMap > 0.0f
            && settings.stringExcite == (int) KarplusStrong::Excite::Hammer && numOscUnison[osc] > 1)
        {
            const auto note = getCurrentlyPlayingNote();
            numOscUnison[osc] = juce::jmin (numOscUnison[osc], note < 35 ? 1 : (note < 47 ? 2 : 3));
        }
    }

    updateUnisonLayout();

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillatorEnabled[osc] || oscEnableSmooth[osc].getCurrentValue() > 0.0005f)
            for (int u = 0; u < numOscUnison[osc]; ++u)
                oscUnison[osc][u].setWavetable (params.oscillators[osc].table);

    const auto configureStrings = [this] (int osc)
    {
        const auto& settings = params.oscillators[osc];

        for (int u = 0; u < juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison); ++u)
        {
            configureString (stringFor (osc, u), settings);
        }
    };

    // Keep OSC 3's original transition order until the behaviour-preserving
    // checkpoint; its string transition is recorded before sample sync.
    configureStrings (2);

    if (params.oscillators[2].stringMode && ! lastStringMode[2] && params.oscillatorEnabled[2])
        for (int u = 0; u < juce::jmin (numOscUnison[2], VoiceParams::maxBufferedUnison); ++u)
            stringFor (2, u).trigger (velocityLevel);

    lastStringMode[2] = params.oscillators[2].stringMode;

    const auto panMod = juce::jlimit (-1.0f, 1.0f, blockMod (D::Pan));

    const auto computePans = [this, panMod] (const VoiceParams::OscParams& osc, int numUnison, float oscPanMod,
                                             float spreadMod, float* gainsL, float* gainsR)
    {
        const auto spread = juce::jlimit (0.0f, 1.0f, osc.spread + spreadMod);

        for (int u = 0; u < numUnison; ++u)
        {
            const auto spreadOffset = numUnison > 1
                                          ? spread * ((float) u / (float) (numUnison - 1) * 2.0f - 1.0f)
                                          : 0.0f;
            const auto pan = juce::jlimit (-1.0f, 1.0f, osc.pan + oscPanMod + spreadOffset + panMod
                                                            + params.voiceSpread * voicePan);
            const auto angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            gainsL[u] = std::cos (angle);
            gainsR[u] = std::sin (angle);
        }
    };

    constexpr D panDestinations[] { D::Osc1Pan, D::Osc2Pan, D::SubPan,
                                    D::Osc4Pan, D::Osc5Pan, D::Osc6Pan };
    constexpr D spreadDestinations[] { D::Osc1Spread, D::Osc2Spread, D::SubSpread,
                                       D::Osc4Spread, D::Osc5Spread, D::Osc6Spread };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (osc < 3 || numOscUnison[osc] > 0)
            computePans (params.oscillators[osc], numOscUnison[osc],
                         blockMod (panDestinations[osc]), blockMod (spreadDestinations[osc]),
                         panGainL[osc], panGainR[osc]);

    computePans (VoiceParams::OscParams {}, 1, 0.0f, 0.0f, &panGainSubOscL, &panGainSubOscR);

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        frameSmooth[osc].setTargetValue (params.oscillators[osc].frame);
        levelSmooth[osc].setTargetValue (params.oscillators[osc].level);
        oscEnableSmooth[osc].setTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
    }

    noiseSmooth.setTargetValue (params.noiseLevel);
    subOscLevelSmooth.setTargetValue (params.subOscLevel);
    subOscEnableSmooth.setTargetValue (params.subOscEnabled ? 1.0f : 0.0f);
    subOsc.setWavetable (params.subOscTable);

    bool modeChanged[VoiceParams::numOscillators] {};
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        modeChanged[osc] = params.oscillators[osc].stringMode != lastStringMode[osc]
                           || params.oscillators[osc].sampleMode != lastSampleMode[osc];

    syncSamplePlayers();

    const auto retriggerNewPlayers = [] (SamplePlayer* players, bool wasSampling, bool wasEnabled,
                                         int previousCount, int count)
    {
        count = juce::jmin (count, VoiceParams::maxBufferedUnison);

        for (int u = (! wasSampling || ! wasEnabled) ? 0 : previousCount; u < count; ++u)
            players[u].trigger();
    };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];
        const auto sampling = settings.sampleMode && params.oscillatorEnabled[osc] && settings.sample != nullptr;

        if (sampling)
            retriggerNewPlayers (sampleUnison[osc], lastSampleMode[osc], lastEnabled[osc],
                                 lastUnison[osc], numOscUnison[osc]);

        lastSampleMode[osc] = settings.sampleMode;
        lastEnabled[osc] = params.oscillatorEnabled[osc];
        lastUnison[osc] = numOscUnison[osc];
    }

    for (int osc = 0; osc < 2; ++osc)
        configureStrings (osc);

    if (params.oscillators[0].stringMode && ! lastStringMode[0])
        for (int u = 0; u < juce::jmin (numOscUnison[0], VoiceParams::maxBufferedUnison); ++u)
            stringFor (0, u).trigger (velocityLevel);

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (modeChanged[osc])
        {
            oscEnableSmooth[osc].setCurrentAndTargetValue (0.0f);
            oscEnableSmooth[osc].setTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
        }

    if (params.oscillators[1].stringMode && ! lastStringMode[1])
        for (int u = 0; u < juce::jmin (numOscUnison[1], VoiceParams::maxBufferedUnison); ++u)
            stringFor (1, u).trigger (velocityLevel);

    for (int osc = 0; osc < 2; ++osc)
        lastStringMode[osc] = params.oscillators[osc].stringMode;

    for (int osc = 3; osc < VoiceParams::numOscillators; ++osc)
    {
        if (! params.oscillatorEnabled[osc])
            continue;

        configureStrings (osc);
        if (params.oscillators[osc].stringMode && ! lastStringMode[osc])
            for (int u = 0; u < juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison); ++u)
                stringFor (osc, u).trigger (velocityLevel);
        lastStringMode[osc] = params.oscillators[osc].stringMode;
    }

    const auto resonatorAmount = juce::jlimit (0.0f, 1.0f, params.resonatorAmount + blockMod (D::ResAmount));
    const auto resonatorDecay = juce::jlimit (0.0f, 1.0f, params.resonatorDecay + blockMod (D::ResDecay));

    resonatorL.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);
    resonatorR.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);

    const auto drive1 = juce::jlimit (1.0f, 10.0f, params.filter1.drive + blockMod (D::Filter1Drive) * driveRange);
    const auto drive2 = juce::jlimit (1.0f, 10.0f, params.filter2.drive + blockMod (D::Filter2Drive) * driveRange);

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    const auto ampVelScale = 1.0f - params.ampVelocity + params.ampVelocity * velocityLevel;
    bool alternateAmpRouting = false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        alternateAmpRouting = alternateAmpRouting
                              || (params.oscillatorEnabled[osc] && params.oscillators[osc].ampEnv != 0);
    auto* mods = sampleMods.data();

    constexpr D frameDestinations[] { D::Osc1Frame, D::Osc2Frame, D::SubFrame,
                                      D::Osc4Frame, D::Osc5Frame, D::Osc6Frame };
    constexpr D levelDestinations[] { D::Osc1Level, D::Osc2Level, D::SubLevel,
                                      D::Osc4Level, D::Osc5Level, D::Osc6Level };
    constexpr D warpDestinations[] { D::Osc1Warp, D::Osc2Warp, D::SubWarp,
                                     D::Osc4Warp, D::Osc5Warp, D::Osc6Warp };
    int routes[VoiceParams::numOscillators] {};
    bool active[VoiceParams::numOscillators] {};

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];
        routes[osc] = juce::jlimit (0, FilterRoute::Count - 1, settings.route);
        active[osc] = params.oscillatorEnabled[osc]
                      && (settings.stringMode || settings.sampleMode || settings.table != nullptr);
    }

    const auto routeSubOsc = juce::jlimit (0, FilterRoute::Count - 1, params.subOscRoute);
    bool bothActive = routeSubOsc == FilterRoute::Both;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        bothActive = bothActive || (active[osc] && routes[osc] == FilterRoute::Both);
    const auto subOscActive = params.subOscEnabled && params.subOscTable != nullptr;
    const auto subOscFrames = WavetableOscillator::frameReadFor (params.subOscTable, 0.0f);

    // M5 operator extras. Each is skipped while unused, so older patches
    // render exactly as before.
    auto anyAltFeedback = false;
    auto anyNoiseOperator = false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        anyAltFeedback = anyAltFeedback || (active[osc] && params.oscillators[osc].feedbackType != FmFeedback::Plain);
        anyNoiseOperator = anyNoiseOperator || (active[osc] && params.fmNoise[osc] > 0.0f);
    }
    const auto noiseCutoff = juce::jmin (0.45 * sampleRate,
                                         200.0 * std::pow (100.0, (double) juce::jlimit (0.0f, 1.0f, params.fmNoiseColour)));
    const auto noiseCoeff = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * noiseCutoff / sampleRate));

    // The matrix amount from source to target, with the legacy cells' own
    // modulation destinations.
    const auto fmAmountAt = [this] (const float* sampleMods, int source, int target)
    {
        const auto amount = params.fmMatrix[source][target];

        if (source >= 3 || target >= 3)
            return params.anyExtendedFmMods ? juce::jlimit (0.0f, 1.0f, amount + fmCellMods[(size_t) (source * 6 + target)])
                                            : amount;

        static constexpr D legacy[3][3] { { D::FmFeedback, D::Fm1to2, D::Fm1to3 },
                                          { D::FmAmount, D::Fm2Feedback, D::Fm2to3 },
                                          { D::Fm3to1, D::Fm3to2, D::Fm3Feedback } };
        return amount + sampleMods[(int) legacy[source][target]];
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const auto ampValue = ampEnv.getNextSample();
        const auto filterValue = filterEnv.getNextSample();
        const auto filter2Value = filter2Env.getNextSample();
        const auto modValue = modEnv.getNextSample();
        const auto env4Value = env4.getNextSample();
        for (int env = 0; env < (int) extraEnvs.size(); ++env)
            if (params.extraEnvNeeded[(size_t) env])
                extraEnvValues[(size_t) env] = extraEnvs[(size_t) env].getNextSample();
        if (params.msegEnvNeeded)
            msegEnvValue = juce::jlimit (0.0f, 1.0f, envMseg.getNextValue());
        const float envelopeValues[17] { ampValue, filterValue, filter2Value, modValue, env4Value,
                                         extraEnvValues[0], extraEnvValues[1], extraEnvValues[2],
                                         extraEnvValues[3], extraEnvValues[4], extraEnvValues[5],
                                         extraEnvValues[6], extraEnvValues[7], extraEnvValues[8],
                                         extraEnvValues[9], extraEnvValues[10], msegEnvValue };

        advanceVoiceLfos();
        evaluateMods (mods, i, ampValue, filterValue, filter2Value, modValue, env4Value);

        if ((i & 15) == 0)
            updateSubBlock (mods, filterValue, filter2Value, envelopeValues);
        else if (params.filter1Fm != 0.0f || params.filter2Fm != 0.0f
                 || mods[(int) D::Filter1Fm] != 0.0f || mods[(int) D::Filter2Fm] != 0.0f)
            updateFilterCoefficients (mods, filterValue, filter2Value);

        // Keep the old operator order: 1, sync, 2, ring, then 3.
        float busL[FilterRoute::Count] {};
        float busR[FilterRoute::Count] {};
        float oscMono[VoiceParams::numOscillators] {};
        float stringDrive[VoiceParams::numOscillators] {};

        const auto fmAmount = params.fmAmount + mods[(int) D::FmAmount];
        const auto fmFeedback = params.fmFeedback + mods[(int) D::FmFeedback];
        const auto fm3to1 = params.fmMatrix[2][0] + mods[(int) D::Fm3to1];
        double fmInput[VoiceParams::numOscillators] {
            (double) (fmAmount * previousOsc[1] + fmFeedback * previousOsc[0]) + (double) (fm3to1 * previousOsc[2]),
            (double) ((params.fmMatrix[0][1] + mods[(int) D::Fm1to2]) * previousOsc[0]
                      + (params.fmMatrix[1][1] + mods[(int) D::Fm2Feedback]) * previousOsc[1]
                      + (params.fmMatrix[2][1] + mods[(int) D::Fm3to2]) * previousOsc[2]),
            (double) ((params.fmMatrix[0][2] + mods[(int) D::Fm1to3]) * previousOsc[0]
                      + (params.fmMatrix[1][2] + mods[(int) D::Fm2to3]) * previousOsc[1]
                      + (params.fmMatrix[2][2] + mods[(int) D::Fm3Feedback]) * previousOsc[2])
        };

        for (int target = 0; target < VoiceParams::numOscillators; ++target)
            for (int source = target < 3 ? 3 : 0; source < VoiceParams::numOscillators; ++source)
                fmInput[target] += (double) fmAmountAt (mods, source, target) * (double) previousOsc[source];

        // Filtered and cross feedback replace an operator's plain self term.
        if (anyAltFeedback)
        {
            double cross[VoiceParams::numOscillators] {};

            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
            {
                const auto type = params.oscillators[osc].feedbackType;

                if (type == FmFeedback::Plain || ! active[osc])
                    continue;

                auto sum = 0.0;
                for (int source = 0; source < VoiceParams::numOscillators; ++source)
                    if (source != osc)
                        sum += (double) fmAmountAt (mods, source, osc) * (double) previousOsc[source];

                const auto self = (double) fmAmountAt (mods, osc, osc);

                if (type == FmFeedback::Filtered)
                {
                    sum += self * (double) feedbackFiltered[osc];
                }
                else
                {
                    const auto partner = FmFeedback::partnerOf (osc);
                    sum += self * (double) previousOsc[partner];
                    cross[partner] += self * (double) previousOsc[osc];
                }

                fmInput[osc] = sum;
            }

            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                fmInput[osc] += cross[osc];
        }

        // The noise operator: coloured noise into any oscillator's FM input.
        if (anyNoiseOperator)
        {
            fmNoiseState += noiseCoeff * ((fmNoiseRandom.nextFloat() * 2.0f - 1.0f) - fmNoiseState);

            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                fmInput[osc] += (double) (params.fmNoise[osc] * fmNoiseState);
        }

        const auto fmPhase = [this] (double input) { return params.fmMode == 0 ? input : 0.0; };
        const auto fmRate = [this] (double input)
        {
            if (params.fmMode == 1)
                return 1.0 + 4.0 * input;

            if (params.fmMode == 2)
                return std::exp2 (juce::jlimit (-4.0, 4.0, 2.0 * input));

            return 1.0;
        };

        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        {
            if (osc >= 3 && ! active[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f)
            {
                previousOsc[osc] = 0.0f;
                continue;
            }
            const auto& settings = params.oscillators[osc];
            const auto warpAmount = settings.pdEnv > 0
                                        ? juce::jlimit (0.0f, 1.0f, settings.warpAmount + mods[(int) warpDestinations[osc]]
                                                                        + settings.pdEnvAmount * envelopeValues[juce::jlimit (0, 16, settings.pdEnv - 1)])
                                        : juce::jlimit (0.0f, 1.0f, settings.warpAmount + mods[(int) warpDestinations[osc]]);
            const auto enable = oscEnableSmooth[osc].getNextValue();
            const auto level = osc == 2 ? juce::jlimit (0.0f, 1.0f,
                                                        levelSmooth[osc].getNextValue() + mods[(int) levelDestinations[osc]])
                                        : 0.0f;
            const auto shouldRender = (active[osc] || enable > 0.0005f) && (osc != 2 || level > 0.0f);

            if (shouldRender)
            {
                const auto frame = juce::jlimit (0.0f, 1.0f,
                                                 frameSmooth[osc].getNextValue() + mods[(int) frameDestinations[osc]]);
                const auto renderLevel = osc == 2 ? level : juce::jlimit (0.0f, 1.0f,
                                                   levelSmooth[osc].getNextValue() + mods[(int) levelDestinations[osc]]);
                const auto warpSource = osc == 0 ? previousOsc[1] : oscMono[0];
                const auto phase = fmPhase (fmInput[osc])
                                   + (settings.warpMode == Warp::Fm ? (double) (warpAmount * warpSource) : 0.0);
                const auto rate = fmRate (fmInput[osc]);
                const auto ring = settings.warpMode == Warp::Ring
                                      ? 1.0f + (warpSource - 1.0f) * warpAmount : 1.0f;
                const auto frames = WavetableOscillator::frameReadFor (settings.table, frame);

                auto stringSum = 0.0f;

                for (int u = 0; u < numOscUnison[osc]; ++u)
                {
                    float raw = 0.0f;
                    float sampleL = 0.0f;
                    float sampleR = 0.0f;

                    if (settings.granularMode)
                    {
                        grains[osc][u].process (sampleL, sampleR);
                        raw = 0.5f * (sampleL + sampleR);
                    }
                    else if (settings.sampleMode)
                    {
                        sampleUnison[osc][u].process (sampleL, sampleR);
                        raw = 0.5f * (sampleL + sampleR);
                    }
                    else if (settings.stringMode)
                    {
                        raw = stringFor (osc, u).process (aftertouchValue, noteHeld, (float) fmInput[osc]);
                        stringSum += raw;
                    }
                    else
                    {
                        oscUnison[osc][u].setFramePosition (frame);
                        if (params.quality == 2)
                        {
                            const auto first = oscUnison[osc][u].getNextSample (phase, frames, rate * 0.5);
                            const auto second = oscUnison[osc][u].getNextSample (phase, frames, rate * 0.5);
                            raw = 0.5f * (first + second) * ring;
                        }
                        else
                        {
                            raw = oscUnison[osc][u].getNextSample (phase, frames, rate) * ring;
                        }
                    }

                    const auto selectedEnv = envelopeValues[juce::jlimit (0, 16, settings.ampEnv)];
                    const auto gain = unisonGains[osc][u] * renderLevel * enable
                                      * (alternateAmpRouting ? selectedEnv : 1.0f) * keyLevelGain[osc];
                    oscMono[osc] += raw * gain;

                    if (params.oscOut[osc])
                    {
                        busL[routes[osc]] += (settings.sampleMode ? sampleL : raw) * gain * panGainL[osc][u];
                        busR[routes[osc]] += (settings.sampleMode ? sampleR : raw) * gain * panGainR[osc][u];
                    }
                }

                // Coupled strings: the strings of one note share the bridge,
                // which soaks up their in-phase motion. The in-phase part of
                // the note dies fast (the prompt sound); as the detuned
                // strings drift apart, the rest rings on (the aftersound).
                if (settings.stringMode && ! settings.sampleMode && settings.couple > 0.0f)
                {
                    const auto count = juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison);
                    const auto bridge = stringSum / (float) juce::jmax (1, count);

                    for (int u = 0; u < count; ++u)
                        stringFor (osc, u).addBridgeInput (-settings.couple * PianoTuning::get().coupling * bridge);
                }
                if (settings.stringMode && ! settings.sampleMode)
                    stringDrive[osc] = stringSum / (float) juce::jmax (1, numOscUnison[osc]);
            }
            else
            {
                frameSmooth[osc].getNextValue();

                if (osc != 2)
                    levelSmooth[osc].getNextValue();
            }

            if (osc == 0 && params.hardSync && active[1] && active[0]
                && ! params.oscillators[0].stringMode && ! params.oscillators[1].stringMode
                && ! params.oscillators[0].sampleMode && ! params.oscillators[1].sampleMode
                && oscUnison[0][0].wrappedThisSample())
                for (int u = 0; u < numOscUnison[1]; ++u)
                    oscUnison[1][u].resetPhase();

            if (osc == 1)
            {
                const auto ringMod = params.ringMod + mods[(int) D::RingMod];

                if (ringMod > 0.0f && active[0] && active[1])
                    for (int bus = 0; bus < FilterRoute::Count; ++bus)
                    {
                        busL[bus] += (busL[bus] * oscMono[1] - busL[bus]) * ringMod;
                        busR[bus] += (busR[bus] * oscMono[1] - busR[bus]) * ringMod;
                    }

                previousOsc[0] = juce::jlimit (-2.0f, 2.0f, oscMono[0]);
                previousOsc[1] = juce::jlimit (-2.0f, 2.0f, oscMono[1]);
            }
            else if (osc >= 2)
            {
                previousOsc[osc] = juce::jlimit (-2.0f, 2.0f, oscMono[osc]);
            }
        }

        if (anyAltFeedback)
            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                if (params.oscillators[osc].feedbackType == FmFeedback::Filtered)
                {
                    // The average of two samples (the DX7's feedback filter)
                    // through a gentle one-pole: calm, saw-like feedback.
                    const auto average = 0.5f * (previousOsc[osc] + feedbackHistory[osc]);
                    feedbackFiltered[osc] += feedbackCoeff[osc] * (average - feedbackFiltered[osc]);
                    feedbackHistory[osc] = previousOsc[osc];
                }

        // Dedicated sub: plain table at frame 0, centre pan.
        const auto subOscLevel = subOscLevelSmooth.getNextValue();
        const auto enableSubOsc = subOscEnableSmooth.getNextValue();

        if (subOscLevel > 0.0f && params.subOscTable != nullptr && (subOscActive || enableSubOsc > 0.0005f))
        {
            subOsc.setFramePosition (0.0f);
            const auto raw = subOsc.getNextSample (0.0, subOscFrames);
            const auto gain = subOscLevel * enableSubOsc;
            busL[routeSubOsc] += raw * gain * panGainSubOscL * (alternateAmpRouting ? ampValue : 1.0f);
            busR[routeSubOsc] += raw * gain * panGainSubOscR * (alternateAmpRouting ? ampValue : 1.0f);
        }

        const auto noiseLevel = juce::jlimit (0.0f, 1.0f, noiseSmooth.getNextValue() + mods[(int) D::NoiseLevel]);

        if (noiseLevel > 0.0f)
        {
            const auto value = (random.nextFloat() * 2.0f - 1.0f) * noiseLevel * 0.5f
                               * (alternateAmpRouting ? ampValue : 1.0f);
            busL[routeSubOsc] += value;
            busR[routeSubOsc] += value;
        }

        float bodyExciteL = 0.0f, bodyExciteR = 0.0f;
        if (params.resonatorOn && params.bodyType != 0 && resonatorAmount > 0.001f)
        {
            if (bodyStrikePending && params.bodyCouplingMode == 1 && params.bodyCoupling > 0.0f)
            {
                bool hasString = false;
                for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                    hasString = hasString || (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode);
                if (hasString)
                {
                    materialBodyL.strike (params.bodyCoupling * velocityLevel);
                    materialBodyR.strike (params.bodyCoupling * velocityLevel);
                }
            }
            bodyStrikePending = false;
            for (int bus = 0; bus < FilterRoute::Count; ++bus)
            {
                bodyExciteL += busL[bus];
                bodyExciteR += busR[bus];
            }
            if (params.bodyCouplingMode == 1)
            {
                bodyExciteL *= 0.2f;
                bodyExciteR *= 0.2f;
                for (const auto drive : stringDrive)
                {
                    bodyExciteL += drive * params.bodyCoupling * 0.2f;
                    bodyExciteR += drive * params.bodyCoupling * 0.2f;
                }
            }
        }

        if (params.bodyCouplingMode == 3 && params.bodyCoupling > 0.0f)
        {
            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
            {
                if (! params.oscillatorEnabled[osc] || ! params.oscillators[osc].stringMode)
                    continue;
                auto otherDrive = 0.0f;
                auto otherCount = 0;
                for (int other = 0; other < VoiceParams::numOscillators; ++other)
                    if (other != osc && params.oscillatorEnabled[other] && params.oscillators[other].stringMode)
                    {
                        otherDrive += stringDrive[other];
                        ++otherCount;
                    }
                if (otherCount == 0)
                    continue;
                const auto count = juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison);
                for (int u = 0; u < count; ++u)
                    stringFor (osc, u).addBridgeInput (0.015f * params.bodyCoupling * otherDrive / (float) otherCount);
            }
        }

        const auto drive = [] (float value, float amount) { return amount > 1.0f ? std::tanh (value * amount) : value; };

        // Filter 1 hears the Default and Filter-1 buses.
        const auto defaultL = drive (busL[FilterRoute::Default], drive1);
        const auto defaultR = drive (busR[FilterRoute::Default], drive1);
        const auto hasF1Bus = busL[FilterRoute::Filter1] != 0.0f || busR[FilterRoute::Filter1] != 0.0f;
        const auto inL = hasF1Bus ? drive (busL[FilterRoute::Default] + busL[FilterRoute::Filter1], drive1) : defaultL;
        const auto inR = hasF1Bus ? drive (busR[FilterRoute::Default] + busR[FilterRoute::Filter1], drive1) : defaultR;

        auto f1L = filter1L.process (inL);
        auto f1R = filter1R.process (inR);

        float outL, outR;

        if (params.filtersParallel)
        {
            // Filter 2 hears the (Filter-1-driven) Default bus plus its own.
            const auto in2L = drive (defaultL + busL[FilterRoute::Filter2], drive2);
            const auto in2R = drive (defaultR + busR[FilterRoute::Filter2], drive2);

            // Balance fades one filter out; at the centre both are at full.
            const auto gain1 = juce::jmin (1.0f, 1.0f - params.filterBalance);
            const auto gain2 = juce::jmin (1.0f, 1.0f + params.filterBalance);

            outL = (f1L * gain1 + filter2L.process (in2L) * gain2) * 0.7071f;
            outR = (f1R * gain1 + filter2R.process (in2R) * gain2) * 0.7071f;
        }
        else
        {
            const auto f2inL = drive (f1L + busL[FilterRoute::Filter2], drive2);
            const auto f2inR = drive (f1R + busR[FilterRoute::Filter2], drive2);

            outL = filter2L.process (f2inL);
            outR = filter2R.process (f2inR);
        }

        outL += busL[FilterRoute::Direct];
        outR += busR[FilterRoute::Direct];

        // A separate parallel pair keeps the existing serial/parallel paths
        // bit-identical whenever no oscillator selects Both.
        if (bothActive)
        {
            const auto both1L = bothFilter1L.process (drive (busL[FilterRoute::Both], drive1));
            const auto both1R = bothFilter1R.process (drive (busR[FilterRoute::Both], drive1));
            const auto both2L = bothFilter2L.process (drive (busL[FilterRoute::Both], drive2));
            const auto both2R = bothFilter2R.process (drive (busR[FilterRoute::Both], drive2));
            outL += (both1L + both2L) * 0.7071f;
            outR += (both1R + both2R) * 0.7071f;
        }

        float bodyWetL = 0.0f, bodyWetR = 0.0f;
        if (params.resonatorOn && resonatorAmount > 0.001f && params.bodyType == 0)
        {
            outL = resonatorL.process (outL);
            outR = resonatorR.process (outR);
        }
        else if (params.resonatorOn && resonatorAmount > 0.001f)
        {
            const auto exciteEnvelope = alternateAmpRouting ? 1.0f : ampValue;
            bodyWetL = materialBodyL.process (bodyExciteL * exciteEnvelope);
            bodyWetR = materialBodyR.process (bodyExciteR * exciteEnvelope);
            outL *= 1.0f - resonatorAmount;
            outR *= 1.0f - resonatorAmount;
            if (params.bodyCouplingMode == 2 && params.bodyCoupling > 0.0f)
                for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                    if (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode)
                        for (int u = 0; u < juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison); ++u)
                            stringFor (osc, u).addBridgeInput (0.01f * params.bodyCoupling * (bodyWetL + bodyWetR));
        }

        const auto ampGain = (alternateAmpRouting ? 1.0f : ampValue) * ampVelScale
                             * juce::jlimit (0.0f, 2.0f, 1.0f + mods[(int) D::AmpLevel]);

        if (params.resonatorOn && params.bodyType != 0 && resonatorAmount > 0.001f)
        {
            const auto wetGain = resonatorAmount * ampVelScale
                                 * juce::jlimit (0.0f, 2.0f, 1.0f + mods[(int) D::AmpLevel]);
            left[startSample + i] += outL * ampGain + bodyWetL * wetGain;
            if (right != nullptr)
                right[startSample + i] += outR * ampGain + bodyWetR * wetGain;
            else
                left[startSample + i] += outR * ampGain + bodyWetR * wetGain;
        }
        else
        {
            left[startSample + i] += outL * ampGain;
            if (right != nullptr)
                right[startSample + i] += outR * ampGain;
            else
                left[startSample + i] += outR * ampGain;
        }

        lastAmpValue = ampValue;
        lastLifetimeValue = ampValue;
        if (alternateAmpRouting)
            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                if (params.oscillatorEnabled[osc])
                    lastLifetimeValue = juce::jmax (lastLifetimeValue,
                                                    envelopeValues[juce::jlimit (0, 16, params.oscillators[osc].ampEnv)]);
        lastFilterValue = filterValue;
        lastFilter2Value = filter2Value;
        lastModValue = modValue;
        lastEnv4Value = env4Value;
    }

    const auto samplePositionOf = [] (const SamplePlayer& player, const SampleData* data)
    {
        if (data == nullptr || data->getNumSamples() <= 0 || ! player.isActive())
            return -1.0f;

        return (float) juce::jlimit (0.0, 1.0, player.getSourcePosition() / (double) data->getNumSamples());
    };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        lastSamplePosition[osc] = params.oscillators[osc].sampleMode
                                      ? samplePositionOf (sampleUnison[osc][0], params.oscillators[osc].sample) : -1.0f;

    if (bodyTailSamplesRemaining > 0)
        bodyTailSamplesRemaining = juce::jmax (0, bodyTailSamplesRemaining - numSamples);

    if (! hasActiveAmpEnvelope())
    {
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        clearCurrentNote();
    }
}

void Voice::configureString (KarplusStrong& string, const VoiceParams::OscParams& settings) const
{
    auto stiffness = settings.stringStiffness;
    auto damping = settings.stringDamping;
    auto decay = settings.stringDecay;

    // Register map: short, stiff, bright treble strings; long, looser bass
    // strings that ring longer.
    if (settings.registerMap > 0.0f)
    {
        const auto amount = settings.registerMap;
        const auto t = keyTrackValue; // -1 at C2, +1 at C6
        // Wound bass strings are less stiff than the plain treble ones.
        stiffness = juce::jlimit (0.0f, 1.0f, stiffness + amount * (0.08f + 0.35f * juce::jmax (0.0f, t)
                                                                         - 0.1f * juce::jmax (0.0f, -t)));
        damping = juce::jlimit (0.0f, 1.0f, damping - amount * 0.25f * t);
        decay = juce::jlimit (0.0f, 1.0f, decay - amount * 0.05f * t);
    }

    string.setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 6, settings.stringExcite)),
                      settings.stringSustain, damping, decay);
    string.setPhysicalParams (stiffness, settings.stringPickup,
                              settings.stringExcitationPosition, settings.stringPickHardness,
                              settings.stringPickPosition, settings.stringSlap);
    string.setBowAndBuzz (settings.bowPressure, settings.bowSpeed, settings.bridgeBuzz, settings.fretRattle);
    string.setKeysParams (settings.hammerHardness, settings.damper);
}

bool Voice::hasActiveAmpEnvelope() const
{
    if (params.resonatorOn && params.bodyType != 0 && bodyTailSamplesRemaining > 0)
        return true;
    bool anyOscillator = false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        if (! params.oscillatorEnabled[osc])
            continue;
        anyOscillator = true;
        const auto selected = juce::jlimit (0, 16, params.oscillators[osc].ampEnv);
        // The MSEG envelope lives as long as ENV 1 (it has no release).
        const auto active = selected == 0 || selected == 16 ? ampEnv.isActive()
                            : selected == 1 ? filterEnv.isActive()
                            : selected == 2 ? filter2Env.isActive()
                            : selected == 3 ? modEnv.isActive()
                            : selected == 4 ? env4.isActive()
                                            : extraEnvs[(size_t) (selected - 5)].isActive();
        if (active)
            return true;
    }
    return (params.subOscEnabled || params.noiseLevel > 0.0f || ! anyOscillator) && ampEnv.isActive();
}

// A ratio or fixed-pitch operator's frequency before pitch modulation. Semi
// and fine still apply on top; a fixed pitch ignores the note, bend and glide.
double Voice::oscFrequencyFactor (const VoiceParams::OscParams& settings) const
{
    const auto offset = std::exp2 ((settings.semitones + settings.cents / 100.0) / 12.0);

    if (settings.tuneMode == OscTuning::Fixed)
        return juce::jlimit (0.5, 20000.0, settings.fixedHz) * offset;

    return currentFrequency * settings.ratio * offset;
}

void Voice::updateSubBlock (const float* mods, float filterEnvValue, float filter2EnvValue, const float* envelopeValues)
{
    using D = Mod::Destination;

    currentFrequency += (baseFrequency - currentFrequency) * (double) glideCoeff;

    auto driftFactor = 1.0;
    const auto drift = juce::jlimit (0.0f, 1.0f, params.drift + mods[(int) D::Drift]);

    if (drift > 0.0f)
    {
        if (driftRandom.nextFloat() > 0.995f)
            driftTarget = driftRandom.nextFloat() * 2.0f - 1.0f;

        driftValue += (driftTarget - driftValue) * 0.002f;
        driftFactor = std::exp2 ((double) driftValue * (double) drift * 0.25 / 12.0);
    }

    const auto driftedFrequency = currentFrequency * driftFactor;

    // Warp amounts move at sub-block rate: the table-level choice behind them
    // is too costly to redo every sample.
    constexpr D warpDestinations[] { D::Osc1Warp, D::Osc2Warp, D::SubWarp,
                                     D::Osc4Warp, D::Osc5Warp, D::Osc6Warp };
    constexpr D pitchDestinations[] { D::Osc1Pitch, D::Osc2Pitch, D::SubPitch,
                                      D::Osc4Pitch, D::Osc5Pitch, D::Osc6Pitch };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];

        if (osc >= 3 && ! params.oscillatorEnabled[osc]
            && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f)
            continue;

        if (osc == 2 && settings.level <= 0.0f)
            continue;

        // The warp envelope (the CZ's DCW) opens both stages of the PD chain.
        const auto envelopeWarp = settings.pdEnv > 0
                                      ? settings.pdEnvAmount * envelopeValues[juce::jlimit (0, 16, settings.pdEnv - 1)]
                                      : 0.0f;
        const auto warp = settings.pdEnv > 0
                              ? juce::jlimit (0.0f, 1.0f, settings.warpAmount + mods[(int) warpDestinations[osc]] + envelopeWarp)
                              : juce::jlimit (0.0f, 1.0f, settings.warpAmount + mods[(int) warpDestinations[osc]]);
        const auto warp2 = juce::jlimit (0.0f, 1.0f, settings.warpAmount2 + envelopeWarp);

        double baseFreq;

        if (settings.tuneMode == OscTuning::Semitones)
        {
            const auto pitch = std::exp2 ((settings.semitones + (osc == 2 ? (double) params.subOctaveOffset : 0.0)
                                           + settings.cents / 100.0 + bendSemitones
                                           + (double) mods[(int) pitchDestinations[osc]] * 48.0) / 12.0);
            baseFreq = driftedFrequency * pitch;
        }
        else
        {
            // Ratio operators follow the note (with bend, glide and drift);
            // fixed ones follow only their own pitch modulation.
            const auto modulation = std::exp2 ((double) mods[(int) pitchDestinations[osc]] * 4.0);
            baseFreq = settings.tuneMode == OscTuning::Fixed
                           ? oscFrequencyFactor (settings) * modulation
                           : settings.ratio * std::exp2 ((settings.semitones + settings.cents / 100.0 + bendSemitones) / 12.0)
                                 * driftedFrequency * modulation;
        }

        if (settings.feedbackType == FmFeedback::Filtered)
            feedbackCoeff[osc] = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi
                                                           * juce::jlimit (500.0, 0.45 * sampleRate, 8.0 * baseFreq)
                                                           / sampleRate));

        for (int u = 0; u < numOscUnison[osc]; ++u)
        {
            const auto frequencyU = baseFreq * std::exp2 (unisonOffset[osc][u] / 12.0);
            oscUnison[osc][u].setFrequency (frequencyU);
            oscUnison[osc][u].setWarp (settings.warpMode, warp);
            oscUnison[osc][u].setWarp2 (settings.warpMode2, warp2);

            if (u < VoiceParams::maxBufferedUnison)
            {
                stringFor (osc, u).setFrequency (frequencyU);
                sampleRatio[osc][u] = settings.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
                grains[osc][u].setPlaybackRatio (sampleRatio[osc][u]);
                sampleUnison[osc][u].setPlaybackRatio ((settings.sample != nullptr ? settings.sample->sampleRate / sampleRate : 1.0)
                                                       * sampleRatio[osc][u]);
            }
        }
    }

    if (params.subOscLevel > 0.0f)
        subOsc.setFrequency (driftedFrequency * std::exp2 (((double) params.subOscOctave + bendSemitones) / 12.0));

    if (params.resonatorOn && params.resonatorAmount + blockMod (D::ResAmount) > 0.001f)
    {
        const auto offset = juce::jlimit (-24.0f, 24.0f, params.resonatorOffset + blockMod (D::ResOffset) * 12.0f);
        if (params.bodyType == 0)
        {
            resonatorL.setTuning (driftedFrequency, offset, params.resonatorKeytrack);
            resonatorR.setTuning (driftedFrequency, offset, params.resonatorKeytrack);
        }
        else
        {
            const auto noteHz = (driftedFrequency * (double) params.resonatorKeytrack
                                 + 220.0 * (1.0 - (double) params.resonatorKeytrack))
                                * std::exp2 ((double) offset / 12.0);
            const auto decay = juce::jlimit (0.0f, 1.0f, params.resonatorDecay + blockMod (D::ResDecay));
            materialBodyL.configure (params.bodyType - 1, params.bodyMaterial, params.bodySize,
                                     decay, noteHz, params.quality, -1.0f);
            materialBodyR.configure (params.bodyType - 1, params.bodyMaterial, params.bodySize,
                                     decay, noteHz, params.quality, 1.0f);
        }
    }

    updateFilterCoefficients (mods, filterEnvValue, filter2EnvValue);
}

void Voice::updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue)
{
    using D = Mod::Destination;

    const auto velocityEnvScale = 1.0f - params.filterVelocity + params.filterVelocity * velocityLevel;

    const auto fmOctaves1 = (double) (params.filter1Fm + mods[(int) D::Filter1Fm]) * (double) previousOsc[1] * 4.0;
    const auto fmOctaves2 = (double) (params.filter2Fm + mods[(int) D::Filter2Fm]) * (double) previousOsc[1] * 4.0;

    const auto keyOctaves1 = (double) params.filter1.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves1 = (double) (params.filter1.envAmount + mods[(int) D::Filter1Env] * envAmountRange)
                             * (double) filterEnvValue * (double) velocityEnvScale;
    const auto cutoff1 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter1.cutoffHz
                                           * std::exp2 (keyOctaves1 + envOctaves1 + fmOctaves1
                                                        + (double) mods[(int) D::Filter1Cutoff] * 6.0));
    const auto reso1 = juce::jlimit (0.0f, 1.0f, params.filter1.resonance + mods[(int) D::Filter1Reso]);

    for (auto* filter : { &filter1L, &filter1R, &bothFilter1L, &bothFilter1R })
        filter->setType (params.filter1.type, params.filter1.slope24);

    const auto morph1 = juce::jlimit (0.0f, 1.0f, params.filter1.morph + mods[(int) D::Filter1Morph]);
    const auto coefficients1 = FilterUnit::makeCoefficients (params.filter1.type, sampleRate, cutoff1, reso1, morph1);
    filter1L.setCoefficients (coefficients1);
    filter1R.setCoefficients (coefficients1);
    bothFilter1L.setCoefficients (coefficients1);
    bothFilter1R.setCoefficients (coefficients1);

    const auto keyOctaves2 = (double) params.filter2.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves2 = (double) (params.filter2.envAmount + mods[(int) D::Filter2Env] * envAmountRange)
                             * (double) filter2EnvValue * (double) velocityEnvScale
                             * (double) velocityScaleFor (params.filter2EnvVelocity);
    const auto cutoff2 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter2.cutoffHz
                                           * std::exp2 (keyOctaves2 + envOctaves2 + fmOctaves2
                                                        + (double) mods[(int) D::Filter2Cutoff] * 6.0));
    const auto reso2 = juce::jlimit (0.0f, 1.0f, params.filter2.resonance + mods[(int) D::Filter2Reso]);

    for (auto* filter : { &filter2L, &filter2R, &bothFilter2L, &bothFilter2R })
        filter->setType (params.filter2.type, params.filter2.slope24);

    const auto morph2 = juce::jlimit (0.0f, 1.0f, params.filter2.morph + mods[(int) D::Filter2Morph]);
    const auto coefficients2 = FilterUnit::makeCoefficients (params.filter2.type, sampleRate, cutoff2, reso2, morph2);
    filter2L.setCoefficients (coefficients2);
    filter2R.setCoefficients (coefficients2);
    bothFilter2L.setCoefficients (coefficients2);
    bothFilter2R.setCoefficients (coefficients2);
}

float Voice::sourceValue (Mod::Source source, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    if (source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
    {
        const auto index = (size_t) ((int) source - (int) Mod::Source::Env6);
        const auto velocityScale = 1.0f - params.extraEnvVelocity[index]
                                   + params.extraEnvVelocity[index] * velocityLevel;
        return extraEnvValues[index] * velocityScale;
    }
    const auto lfoSource = [this, sampleIndex] (int lfo, const float* shared)
    {
        if (params.lfos[lfo].perVoice)
            return lfoValues[lfo];

        return shared != nullptr ? shared[sampleIndex] : 0.0f;
    };

    if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
        return lfoSource (lfo, params.lfoBuffers[lfo]);

    switch (source)
    {
        case Mod::Source::ModEnv:     return modValue * velocityScaleFor (params.modEnvVelocity);
        case Mod::Source::FilterEnv:  return filterValue;
        case Mod::Source::AmpEnv:     return ampValue;
        case Mod::Source::Velocity:   return velocityLevel;
        case Mod::Source::KeyTrack:   return keyTrackValue;
        case Mod::Source::Random:     return randomValue;
        case Mod::Source::ModWheel:   return modWheel;
        case Mod::Source::Aftertouch: return aftertouchValue;
        case Mod::Source::Expression: return expressionValue;
        case Mod::Source::Macro1:     return params.macros[0];
        case Mod::Source::Macro2:     return params.macros[1];
        case Mod::Source::Macro3:     return params.macros[2];
        case Mod::Source::Macro4:     return params.macros[3];
        case Mod::Source::ClockSh:    return params.clockSh != nullptr ? params.clockSh[sampleIndex] : 0.0f;
        case Mod::Source::Mseg:       return params.mseg != nullptr ? params.mseg[sampleIndex] : 0.0f;
        case Mod::Source::Env4:       return env4Value * velocityScaleFor (params.env4Velocity);
        case Mod::Source::FilterEnv2: return filter2Value * velocityScaleFor (params.filter2EnvVelocity);
        case Mod::Source::None:
        case Mod::Source::Count:
        default:                      return 0.0f;
    }
}
