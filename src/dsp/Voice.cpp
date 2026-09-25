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

    frameSmooth1.reset (newRate, 0.02);
    frameSmooth2.reset (newRate, 0.02);
    subFrameSmooth.reset (newRate, 0.02);
    levelSmooth1.reset (newRate, 0.02);
    levelSmooth2.reset (newRate, 0.02);
    subSmooth.reset (newRate, 0.02);
    osc1EnableSmooth.reset (newRate, 0.02);
    osc2EnableSmooth.reset (newRate, 0.02);
    subEnableSmooth.reset (newRate, 0.02);
    noiseSmooth.reset (newRate, 0.02);
    subOscLevelSmooth.reset (newRate, 0.02);
    subOscEnableSmooth.reset (newRate, 0.02);
    subOsc.setSampleRate (newRate);

    ampEnv.setSampleRate (newRate);
    filterEnv.setSampleRate (newRate);
    filter2Env.setSampleRate (newRate);
    modEnv.setSampleRate (newRate);
    env4.setSampleRate (newRate);

    for (auto* oscs : { osc1Unison, osc2Unison, subUnison })
        for (int u = 0; u < VoiceParams::maxUnison; ++u)
            oscs[u].setSampleRate (newRate);

    for (auto* strings : { string1Unison, string2Unison, subStrings })
        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            strings[u].prepare (newRate);

    for (auto* players : { sample1Unison, sample2Unison, subSamples })
        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            players[u].prepare (newRate);

    for (auto* clouds : { grains1, grains2, grainsSub })
        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            clouds[u].prepare (newRate);

    resonatorL.prepare (newRate);
    resonatorR.prepare (newRate);

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R })
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

    setup (params.osc1, sample1Unison, grains1, blockMod (Mod::Destination::Osc1SampleStart),
           blockMod (Mod::Destination::Osc1SampleEnd));
    setup (params.osc2, sample2Unison, grains2, blockMod (Mod::Destination::Osc2SampleStart),
           blockMod (Mod::Destination::Osc2SampleEnd));
    setup (params.sub, subSamples, grainsSub, blockMod (Mod::Destination::SubSampleStart),
           blockMod (Mod::Destination::SubSampleEnd));
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

    // Per-voice LFOs restart with each articulated note.
    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        lfoPhases[lfo] = (double) juce::jlimit (0.0f, 1.0f, params.lfos[lfo].startPhase);
        lfoHolds[lfo] = random.nextFloat() * 2.0f - 1.0f;
        lfoChaos[lfo].reset (random);
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

        osc1Unison[u].resetPhase (phase);
        osc2Unison[u].resetPhase (phase);
        subUnison[u].resetPhase (phase);
    }

    resonatorL.reset();
    resonatorR.reset();

    if (params.osc1.stringMode && params.osc1Enabled)
    {
        const auto pitch = currentFrequency * std::exp2 ((params.osc1.semitones + params.osc1.cents / 100.0) / 12.0);
        for (auto& string : string1Unison)
        {
            string.setFrequency (pitch);
            string.setPhysicalParams (params.osc1.stringStiffness, params.osc1.stringPickup,
                                      params.osc1.stringExcitationPosition, params.osc1.stringPickHardness,
                                      params.osc1.stringPickPosition, params.osc1.stringSlap);
            string.trigger (velocity);
        }
    }

    if (params.osc2.stringMode && params.osc2Enabled)
    {
        const auto pitch = currentFrequency * std::exp2 ((params.osc2.semitones + params.osc2.cents / 100.0) / 12.0);
        for (auto& string : string2Unison)
        {
            string.setFrequency (pitch);
            string.setPhysicalParams (params.osc2.stringStiffness, params.osc2.stringPickup,
                                      params.osc2.stringExcitationPosition, params.osc2.stringPickHardness,
                                      params.osc2.stringPickPosition, params.osc2.stringSlap);
            string.trigger (velocity);
        }
    }

    if (params.sub.stringMode && params.subEnabled)
    {
        const auto pitch = currentFrequency * std::exp2 ((params.sub.semitones + params.subOctaveOffset
                                                         + params.sub.cents / 100.0) / 12.0);
        for (auto& string : subStrings)
        {
            string.setFrequency (pitch);
            string.setPhysicalParams (params.sub.stringStiffness, params.sub.stringPickup,
                                      params.sub.stringExcitationPosition, params.sub.stringPickHardness,
                                      params.sub.stringPickPosition, params.sub.stringSlap);
            string.trigger (velocity);
        }
    }

    syncSamplePlayers();

    const auto bufferedCount = [] (int unison) { return juce::jlimit (1, VoiceParams::maxBufferedUnison, unison); };

    if (params.osc1.sampleMode && params.osc1Enabled && params.osc1.sample != nullptr)
        for (int u = 0; u < bufferedCount (params.osc1.unison); ++u)
        {
            sample1Unison[u].trigger();
            grains1[u].reset ((juce::uint32) (midiNoteNumber * 7919 + u * 104729 + random.nextInt()));
        }

    if (params.osc2.sampleMode && params.osc2Enabled && params.osc2.sample != nullptr)
        for (int u = 0; u < bufferedCount (params.osc2.unison); ++u)
        {
            sample2Unison[u].trigger();
            grains2[u].reset ((juce::uint32) (midiNoteNumber * 7919 + u * 104729 + random.nextInt()));
        }

    if (params.sub.sampleMode && params.subEnabled && params.sub.sample != nullptr)
        for (int u = 0; u < bufferedCount (params.sub.unison); ++u)
        {
            subSamples[u].trigger();
            grainsSub[u].reset ((juce::uint32) (midiNoteNumber * 7919 + u * 104729 + random.nextInt()));
        }

    lastStringMode1 = params.osc1.stringMode;
    lastStringMode2 = params.osc2.stringMode;
    lastSubStringMode = params.sub.stringMode;
    lastSampleMode1 = params.osc1.sampleMode;
    lastSampleMode2 = params.osc2.sampleMode;
    lastSubSampleMode = params.sub.sampleMode;

    previousOsc1 = 0.0f;
    previousOsc2 = 0.0f;
    previousOsc3 = 0.0f;
    driftValue = driftRandom.nextFloat() * 2.0f - 1.0f;
    driftTarget = driftValue;

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R })
        filter->reset();

    frameSmooth1.setCurrentAndTargetValue (params.osc1.frame);
    frameSmooth2.setCurrentAndTargetValue (params.osc2.frame);
    subFrameSmooth.setCurrentAndTargetValue (params.sub.frame);
    levelSmooth1.setCurrentAndTargetValue (params.osc1.level);
    levelSmooth2.setCurrentAndTargetValue (params.osc2.level);
    subSmooth.setCurrentAndTargetValue (params.sub.level);
    noiseSmooth.setCurrentAndTargetValue (params.noiseLevel);
    osc1EnableSmooth.setCurrentAndTargetValue (params.osc1Enabled ? 1.0f : 0.0f);
    osc2EnableSmooth.setCurrentAndTargetValue (params.osc2Enabled ? 1.0f : 0.0f);
    subEnableSmooth.setCurrentAndTargetValue (params.subEnabled ? 1.0f : 0.0f);
    subOscLevelSmooth.setCurrentAndTargetValue (params.subOscLevel);
    subOscEnableSmooth.setCurrentAndTargetValue (params.subOscEnabled ? 1.0f : 0.0f);

    ampEnv.noteOn();
    filterEnv.noteOn();
    filter2Env.noteOn();
    modEnv.noteOn();
    env4.noteOn();
}

void Voice::stopNote (float, bool allowTailOff)
{
    // The synth hard-stops a voice before reusing it; a mono note change
    // keeps the voice sounding instead.
    if (! allowTailOff && monoPending && monoKeepRunning)
        return;

    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
        filter2Env.noteOff();
        modEnv.noteOff();
        env4.noteOff();
    }
    else
    {
        ampEnv.reset();
        filterEnv.reset();
        filter2Env.reset();
        modEnv.reset();
        env4.reset();
        lastAmpValue = 0.0f;
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
            lfoHolds[lfo] = random.nextFloat() * 2.0f - 1.0f;

            if (LfoShapes::isStateful (shape) && ! LfoShapes::isPhysics (shape))
                lfoChaos[lfo].onCycle (shape, random);
        }

        lfoPhases[lfo] = next;
    }
}

void Voice::evaluateMods (float* mods, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    for (int d = 0; d < params.numActiveDestinations; ++d)
        mods[params.activeDestinations[d]] = 0.0f;

    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];

        if (slot.destination >= Mod::numExplicitDestinations)
            continue;

        auto value = Mod::shape (slot, sourceValue (slot.source, sampleIndex, ampValue, filterValue,
                                                    filter2Value, modValue, env4Value));

        if (slot.aux != Mod::Source::None)
            value *= Mod::auxScale (slot.aux, sourceValue (slot.aux, sampleIndex, ampValue, filterValue,
                                                            filter2Value, modValue, env4Value));

        mods[slot.destination] += slot.depth * value;
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

    layout (params.osc1, numOsc1Unison, params.osc1Chord,
            juce::jlimit (0.0f, 100.0f, params.osc1.detuneCents + blockMod (Mod::Destination::Osc1Detune) * detuneRange),
            params.osc1.unisonBlend + blockMod (Mod::Destination::Osc1Blend), unisonOffset1, unisonGains1);
    layout (params.osc2, numOsc2Unison, params.osc2Chord,
            juce::jlimit (0.0f, 100.0f, params.osc2.detuneCents + blockMod (Mod::Destination::Osc2Detune) * detuneRange),
            params.osc2.unisonBlend + blockMod (Mod::Destination::Osc2Blend), unisonOffset2, unisonGains2);
    layout (params.sub, numSubUnison, params.sub.chord,
            juce::jlimit (0.0f, 100.0f, params.sub.detuneCents + blockMod (Mod::Destination::SubDetune) * detuneRange),
            params.sub.unisonBlend + blockMod (Mod::Destination::SubBlend), unisonOffsetSub, unisonGainsSub);
}

void Voice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

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

    using D = Mod::Destination;
    ampEnv.setParameters (modulatedEnvelope (params.ampEnv, blockMod (D::AmpAttack), blockMod (D::AmpDecay),
                                             blockMod (D::AmpSustain), blockMod (D::AmpRelease)));
    filterEnv.setParameters (modulatedEnvelope (params.filterEnv, blockMod (D::FeAttack), blockMod (D::FeDecay),
                                                blockMod (D::FeSustain), blockMod (D::FeRelease)));
    filter2Env.setParameters (modulatedEnvelope (params.filter2Env, blockMod (D::F2eAttack), blockMod (D::F2eDecay),
                                                 blockMod (D::F2eSustain), blockMod (D::F2eRelease)));
    modEnv.setParameters (modulatedEnvelope (params.modEnv, blockMod (D::MeAttack), blockMod (D::MeDecay),
                                             blockMod (D::MeSustain), blockMod (D::MeRelease)));
    env4.setParameters (modulatedEnvelope (params.env4, blockMod (D::E4Attack), blockMod (D::E4Decay),
                                           blockMod (D::E4Sustain), blockMod (D::E4Release)));

    const D rateDestinations[] { D::Lfo1Rate, D::Lfo2Rate, D::Lfo3Rate, D::Lfo4Rate };

    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        lfoIncrements[lfo] = params.lfos[lfo].baseIncrement
                             * std::exp2 ((double) blockMod (rateDestinations[lfo]) * (double) lfoRateOctaves);

        // Key tracked: RATE 4 Hz runs at the note's own pitch, 8 Hz an
        // octave above, 2 Hz an octave below.
        if (params.lfos[lfo].keyTrack)
            lfoIncrements[lfo] = juce::jmin (0.45, lfoIncrements[lfo] * currentFrequency / 4.0);
    }

    glideCoeff = params.glideTime > 0.001f
                     ? 1.0f - std::exp (-1.0f / (float) (params.glideTime * sampleRate))
                     : 1.0f;

    const auto unisonLimit = [] (const VoiceParams::OscParams& osc)
    {
        return (osc.stringMode || osc.sampleMode) ? VoiceParams::maxBufferedUnison : VoiceParams::maxUnison;
    };

    numOsc1Unison = juce::jlimit (1, unisonLimit (params.osc1), params.osc1.unison);
    numOsc2Unison = juce::jlimit (1, unisonLimit (params.osc2), params.osc2.unison);
    numSubUnison = juce::jlimit (1, unisonLimit (params.sub), params.sub.unison);

    updateUnisonLayout();

    for (int u = 0; u < numOsc1Unison; ++u)
        osc1Unison[u].setWavetable (params.osc1.table);

    for (int u = 0; u < numOsc2Unison; ++u)
        osc2Unison[u].setWavetable (params.osc2.table);

    for (int u = 0; u < numSubUnison; ++u)
        subUnison[u].setWavetable (params.sub.table);

    for (int u = 0; u < juce::jmin (numSubUnison, VoiceParams::maxBufferedUnison); ++u)
    {
        subStrings[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.sub.stringExcite)),
                                 params.sub.stringSustain, params.sub.stringDamping, params.sub.stringDecay);
        subStrings[u].setPhysicalParams (params.sub.stringStiffness, params.sub.stringPickup,
                                         params.sub.stringExcitationPosition, params.sub.stringPickHardness,
                                         params.sub.stringPickPosition, params.sub.stringSlap);
    }

    if (params.sub.stringMode && ! lastSubStringMode && params.subEnabled)
        for (int u = 0; u < juce::jmin (numSubUnison, VoiceParams::maxBufferedUnison); ++u)
            subStrings[u].trigger (velocityLevel);

    lastSubStringMode = params.sub.stringMode;

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

    computePans (params.osc1, numOsc1Unison, blockMod (D::Osc1Pan), blockMod (D::Osc1Spread), panGain1L, panGain1R);
    computePans (params.osc2, numOsc2Unison, blockMod (D::Osc2Pan), blockMod (D::Osc2Spread), panGain2L, panGain2R);
    computePans (params.sub, numSubUnison, blockMod (D::SubPan), blockMod (D::SubSpread), panGainSubL, panGainSubR);
    computePans (VoiceParams::OscParams {}, 1, 0.0f, 0.0f, &panGainSubOscL, &panGainSubOscR);

    frameSmooth1.setTargetValue (params.osc1.frame);
    frameSmooth2.setTargetValue (params.osc2.frame);
    subFrameSmooth.setTargetValue (params.sub.frame);
    levelSmooth1.setTargetValue (params.osc1.level);
    levelSmooth2.setTargetValue (params.osc2.level);
    subSmooth.setTargetValue (params.sub.level);
    noiseSmooth.setTargetValue (params.noiseLevel);
    osc1EnableSmooth.setTargetValue (params.osc1Enabled ? 1.0f : 0.0f);
    osc2EnableSmooth.setTargetValue (params.osc2Enabled ? 1.0f : 0.0f);
    subEnableSmooth.setTargetValue (params.subEnabled ? 1.0f : 0.0f);
    subOscLevelSmooth.setTargetValue (params.subOscLevel);
    subOscEnableSmooth.setTargetValue (params.subOscEnabled ? 1.0f : 0.0f);
    subOsc.setWavetable (params.subOscTable);

    const auto osc1Active = params.osc1Enabled && (params.osc1.stringMode || params.osc1.sampleMode || params.osc1.table != nullptr);
    const auto osc2Active = params.osc2Enabled && (params.osc2.stringMode || params.osc2.sampleMode || params.osc2.table != nullptr);
    const auto subActive = params.subEnabled && (params.sub.stringMode || params.sub.sampleMode || params.sub.table != nullptr);

    const auto modeChanged1 = params.osc1.stringMode != lastStringMode1 || params.osc1.sampleMode != lastSampleMode1;
    const auto modeChanged2 = params.osc2.stringMode != lastStringMode2 || params.osc2.sampleMode != lastSampleMode2;
    const auto modeChangedSub = params.sub.stringMode != lastSubStringMode || params.sub.sampleMode != lastSubSampleMode;

    syncSamplePlayers();

    const auto sampleChanged1 = params.osc1.sampleMode && params.osc1Enabled && params.osc1.sample != nullptr;
    const auto sampleChanged2 = params.osc2.sampleMode && params.osc2Enabled && params.osc2.sample != nullptr;
    const auto sampleChangedSub = params.sub.sampleMode && params.subEnabled && params.sub.sample != nullptr;

    const auto retriggerNewPlayers = [] (SamplePlayer* players, bool wasSampling, bool wasEnabled,
                                         int previousCount, int count)
    {
        count = juce::jmin (count, VoiceParams::maxBufferedUnison);

        for (int u = (! wasSampling || ! wasEnabled) ? 0 : previousCount; u < count; ++u)
            players[u].trigger();
    };

    if (sampleChanged1)
        retriggerNewPlayers (sample1Unison, lastSampleMode1, lastEnabled1, lastUnison1, numOsc1Unison);

    if (sampleChanged2)
        retriggerNewPlayers (sample2Unison, lastSampleMode2, lastEnabled2, lastUnison2, numOsc2Unison);

    if (sampleChangedSub)
        retriggerNewPlayers (subSamples, lastSubSampleMode, lastEnabledSub, lastUnisonSub, numSubUnison);

    lastSampleMode1 = params.osc1.sampleMode;
    lastSampleMode2 = params.osc2.sampleMode;
    lastSubSampleMode = params.sub.sampleMode;
    lastEnabled1 = params.osc1Enabled;
    lastEnabled2 = params.osc2Enabled;
    lastEnabledSub = params.subEnabled;
    lastUnison1 = numOsc1Unison;
    lastUnison2 = numOsc2Unison;
    lastUnisonSub = numSubUnison;

    for (int u = 0; u < juce::jmin (numOsc1Unison, VoiceParams::maxBufferedUnison); ++u)
    {
        string1Unison[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.osc1.stringExcite)),
                                    params.osc1.stringSustain, params.osc1.stringDamping, params.osc1.stringDecay);
        string1Unison[u].setPhysicalParams (params.osc1.stringStiffness, params.osc1.stringPickup,
                                            params.osc1.stringExcitationPosition, params.osc1.stringPickHardness,
                                            params.osc1.stringPickPosition, params.osc1.stringSlap);
    }

    for (int u = 0; u < juce::jmin (numOsc2Unison, VoiceParams::maxBufferedUnison); ++u)
    {
        string2Unison[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.osc2.stringExcite)),
                                    params.osc2.stringSustain, params.osc2.stringDamping, params.osc2.stringDecay);
        string2Unison[u].setPhysicalParams (params.osc2.stringStiffness, params.osc2.stringPickup,
                                            params.osc2.stringExcitationPosition, params.osc2.stringPickHardness,
                                            params.osc2.stringPickPosition, params.osc2.stringSlap);
    }

    if (params.osc1.stringMode && ! lastStringMode1)
        for (int u = 0; u < juce::jmin (numOsc1Unison, VoiceParams::maxBufferedUnison); ++u)
            string1Unison[u].trigger (velocityLevel);

    if (modeChanged1)
    {
        osc1EnableSmooth.setCurrentAndTargetValue (0.0f);
        osc1EnableSmooth.setTargetValue (params.osc1Enabled ? 1.0f : 0.0f);
    }

    if (modeChanged2)
    {
        osc2EnableSmooth.setCurrentAndTargetValue (0.0f);
        osc2EnableSmooth.setTargetValue (params.osc2Enabled ? 1.0f : 0.0f);
    }

    if (modeChangedSub)
    {
        subEnableSmooth.setCurrentAndTargetValue (0.0f);
        subEnableSmooth.setTargetValue (params.subEnabled ? 1.0f : 0.0f);
    }

    if (params.osc2.stringMode && ! lastStringMode2)
        for (int u = 0; u < juce::jmin (numOsc2Unison, VoiceParams::maxBufferedUnison); ++u)
            string2Unison[u].trigger (velocityLevel);

    lastStringMode1 = params.osc1.stringMode;
    lastStringMode2 = params.osc2.stringMode;

    const auto resonatorAmount = juce::jlimit (0.0f, 1.0f, params.resonatorAmount + blockMod (D::ResAmount));
    const auto resonatorDecay = juce::jlimit (0.0f, 1.0f, params.resonatorDecay + blockMod (D::ResDecay));

    resonatorL.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);
    resonatorR.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);

    const auto drive1 = juce::jlimit (1.0f, 10.0f, params.filter1.drive + blockMod (D::Filter1Drive) * driveRange);
    const auto drive2 = juce::jlimit (1.0f, 10.0f, params.filter2.drive + blockMod (D::Filter2Drive) * driveRange);

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    const auto ampVelScale = 1.0f - params.ampVelocity + params.ampVelocity * velocityLevel;
    auto* mods = sampleMods.data();

    const auto route1 = juce::jlimit (0, FilterRoute::Count - 1, params.osc1.route);
    const auto route2 = juce::jlimit (0, FilterRoute::Count - 1, params.osc2.route);
    const auto routeSub = juce::jlimit (0, FilterRoute::Count - 1, params.sub.route);
    const auto routeSubOsc = juce::jlimit (0, FilterRoute::Count - 1, params.subOscRoute);
    const auto subOscActive = params.subOscEnabled && params.subOscTable != nullptr;
    const auto subOscFrames = WavetableOscillator::frameReadFor (params.subOscTable, 0.0f);

    const auto warp1Mode = params.osc1.warpMode;
    const auto warp2Mode = params.osc2.warpMode;
    const auto warpSubMode = params.sub.warpMode;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto ampValue = ampEnv.getNextSample();
        const auto filterValue = filterEnv.getNextSample();
        const auto filter2Value = filter2Env.getNextSample();
        const auto modValue = modEnv.getNextSample();
        const auto env4Value = env4.getNextSample();

        advanceVoiceLfos();
        evaluateMods (mods, i, ampValue, filterValue, filter2Value, modValue, env4Value);

        if ((i & 15) == 0)
            updateSubBlock (mods, filterValue, filter2Value);
        else if (params.filter1Fm != 0.0f || params.filter2Fm != 0.0f
                 || mods[(int) D::Filter1Fm] != 0.0f || mods[(int) D::Filter2Fm] != 0.0f)
            updateFilterCoefficients (mods, filterValue, filter2Value);

        // One stereo bus per filter route; with every oscillator on Default
        // only bus 0 is used and the signal flow is exactly the classic one.
        float busL[FilterRoute::Count] {};
        float busR[FilterRoute::Count] {};
        auto osc1Mono = 0.0f;
        auto osc2Mono = 0.0f;
        auto osc3Mono = 0.0f;

        // FM matrix: each oscillator hears the others (and itself) from the
        // previous sample. Phase mode offsets the phase (as FM always did);
        // through-zero and exponential bend the frequency instead.
        const auto fmAmount = params.fmAmount + mods[(int) D::FmAmount];
        const auto fmFeedback = params.fmFeedback + mods[(int) D::FmFeedback];
        const auto fm3to1 = params.fmMatrix[2][0] + mods[(int) D::Fm3to1];
        const auto fmInput1 = (double) (fmAmount * previousOsc2 + fmFeedback * previousOsc1) + (double) (fm3to1 * previousOsc3);
        const auto fmInput2 = (double) ((params.fmMatrix[0][1] + mods[(int) D::Fm1to2]) * previousOsc1
                                        + (params.fmMatrix[1][1] + mods[(int) D::Fm2Feedback]) * previousOsc2
                                        + (params.fmMatrix[2][1] + mods[(int) D::Fm3to2]) * previousOsc3);
        const auto fmInput3 = (double) ((params.fmMatrix[0][2] + mods[(int) D::Fm1to3]) * previousOsc1
                                        + (params.fmMatrix[1][2] + mods[(int) D::Fm2to3]) * previousOsc2
                                        + (params.fmMatrix[2][2] + mods[(int) D::Fm3Feedback]) * previousOsc3);

        const auto fmPhase = [this] (double input) { return params.fmMode == 0 ? input : 0.0; };
        const auto fmRate = [this] (double input)
        {
            if (params.fmMode == 1)
                return 1.0 + 4.0 * input;               // through-zero: up to +-4x the pitch

            if (params.fmMode == 2)
                return std::exp2 (juce::jlimit (-4.0, 4.0, 2.0 * input)); // exponential: up to +-2 octaves

            return 1.0;
        };

        auto phaseModulation = fmPhase (fmInput1);
        const auto rate1 = fmRate (fmInput1);

        const auto warp1Amount = juce::jlimit (0.0f, 1.0f, params.osc1.warpAmount + mods[(int) D::Osc1Warp]);

        if (warp1Mode == Warp::Fm)
            phaseModulation += (double) (warp1Amount * previousOsc2);

        const auto enable1 = osc1EnableSmooth.getNextValue();

        if (osc1Active || enable1 > 0.0005f)
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth1.getNextValue() + mods[(int) D::Osc1Frame]);
            const auto level = juce::jlimit (0.0f, 1.0f, levelSmooth1.getNextValue() + mods[(int) D::Osc1Level]);
            const auto ring = warp1Mode == Warp::Ring ? 1.0f + (previousOsc2 - 1.0f) * warp1Amount : 1.0f;
            const auto frames = WavetableOscillator::frameReadFor (params.osc1.table, frame);

            for (int u = 0; u < numOsc1Unison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.osc1.granularMode)
                {
                    grains1[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.osc1.sampleMode)
                {
                    sample1Unison[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.osc1.stringMode)
                {
                    raw = string1Unison[u].process();
                }
                else
                {
                    osc1Unison[u].setFramePosition (frame);
                    raw = osc1Unison[u].getNextSample (phaseModulation, frames, rate1) * ring;
                }

                const auto gain = unisonGains1[u] * level * enable1;
                osc1Mono += raw * gain;

                if (params.oscOut[0])
                {
                    busL[route1] += (params.osc1.sampleMode ? sampleL : raw) * gain * panGain1L[u];
                    busR[route1] += (params.osc1.sampleMode ? sampleR : raw) * gain * panGain1R[u];
                }
            }
        }
        else
        {
            frameSmooth1.getNextValue();
            levelSmooth1.getNextValue();
        }

        if (params.hardSync && osc2Active && osc1Active && ! params.osc1.stringMode && ! params.osc2.stringMode
            && ! params.osc1.sampleMode && ! params.osc2.sampleMode
            && osc1Unison[0].wrappedThisSample())
        {
            for (int u = 0; u < numOsc2Unison; ++u)
                osc2Unison[u].resetPhase();
        }

        const auto enable2 = osc2EnableSmooth.getNextValue();
        const auto warp2Amount = juce::jlimit (0.0f, 1.0f, params.osc2.warpAmount + mods[(int) D::Osc2Warp]);

        if (osc2Active || enable2 > 0.0005f)
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth2.getNextValue() + mods[(int) D::Osc2Frame]);
            const auto level = juce::jlimit (0.0f, 1.0f, levelSmooth2.getNextValue() + mods[(int) D::Osc2Level]);
            const auto phaseMod2 = (warp2Mode == Warp::Fm ? (double) (warp2Amount * osc1Mono) : 0.0) + fmPhase (fmInput2);
            const auto rate2 = fmRate (fmInput2);
            const auto ring = warp2Mode == Warp::Ring ? 1.0f + (osc1Mono - 1.0f) * warp2Amount : 1.0f;
            const auto frames = WavetableOscillator::frameReadFor (params.osc2.table, frame);

            for (int u = 0; u < numOsc2Unison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.osc2.granularMode)
                {
                    grains2[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.osc2.sampleMode)
                {
                    sample2Unison[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.osc2.stringMode)
                {
                    raw = string2Unison[u].process();
                }
                else
                {
                    osc2Unison[u].setFramePosition (frame);
                    raw = osc2Unison[u].getNextSample (phaseMod2, frames, rate2) * ring;
                }

                const auto gain = unisonGains2[u] * level * enable2;
                osc2Mono += raw * gain;

                if (params.oscOut[1])
                {
                    busL[route2] += (params.osc2.sampleMode ? sampleL : raw) * gain * panGain2L[u];
                    busR[route2] += (params.osc2.sampleMode ? sampleR : raw) * gain * panGain2R[u];
                }
            }
        }
        else
        {
            frameSmooth2.getNextValue();
            levelSmooth2.getNextValue();
        }

        const auto ringMod = params.ringMod + mods[(int) D::RingMod];

        if (ringMod > 0.0f && osc1Active && osc2Active)
        {
            for (int bus = 0; bus < FilterRoute::Count; ++bus)
            {
                busL[bus] += (busL[bus] * osc2Mono - busL[bus]) * ringMod;
                busR[bus] += (busR[bus] * osc2Mono - busR[bus]) * ringMod;
            }
        }

        previousOsc1 = juce::jlimit (-2.0f, 2.0f, osc1Mono);
        previousOsc2 = juce::jlimit (-2.0f, 2.0f, osc2Mono);

        const auto subLevel = juce::jlimit (0.0f, 1.0f, subSmooth.getNextValue() + mods[(int) D::SubLevel]);
        const auto enableSub = subEnableSmooth.getNextValue();

        if (subLevel > 0.0f && (subActive || enableSub > 0.0005f))
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, subFrameSmooth.getNextValue() + mods[(int) D::SubFrame]);
            const auto warpSubAmount = juce::jlimit (0.0f, 1.0f, params.sub.warpAmount + mods[(int) D::SubWarp]);
            const auto phaseModSub = (warpSubMode == Warp::Fm ? (double) (warpSubAmount * osc1Mono) : 0.0) + fmPhase (fmInput3);
            const auto rateSub = fmRate (fmInput3);
            const auto ring = warpSubMode == Warp::Ring ? 1.0f + (osc1Mono - 1.0f) * warpSubAmount : 1.0f;
            const auto frames = WavetableOscillator::frameReadFor (params.sub.table, frame);

            for (int u = 0; u < numSubUnison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.sub.granularMode)
                {
                    grainsSub[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.sub.sampleMode)
                {
                    subSamples[u].process (sampleL, sampleR);
                    raw = 0.5f * (sampleL + sampleR);
                }
                else if (params.sub.stringMode)
                {
                    raw = subStrings[u].process();
                }
                else
                {
                    subUnison[u].setFramePosition (frame);
                    raw = subUnison[u].getNextSample (phaseModSub, frames, rateSub) * ring;
                }

                const auto gain = unisonGainsSub[u] * subLevel * enableSub;
                osc3Mono += raw * gain;

                if (params.oscOut[2])
                {
                    busL[routeSub] += (params.sub.sampleMode ? sampleL : raw) * gain * panGainSubL[u];
                    busR[routeSub] += (params.sub.sampleMode ? sampleR : raw) * gain * panGainSubR[u];
                }
            }
        }
        else
        {
            subFrameSmooth.getNextValue();
        }

        previousOsc3 = juce::jlimit (-2.0f, 2.0f, osc3Mono);

        // Dedicated sub: plain table at frame 0, centre pan.
        const auto subOscLevel = subOscLevelSmooth.getNextValue();
        const auto enableSubOsc = subOscEnableSmooth.getNextValue();

        if (subOscLevel > 0.0f && params.subOscTable != nullptr && (subOscActive || enableSubOsc > 0.0005f))
        {
            subOsc.setFramePosition (0.0f);
            const auto raw = subOsc.getNextSample (0.0, subOscFrames);
            const auto gain = subOscLevel * enableSubOsc;
            busL[routeSubOsc] += raw * gain * panGainSubOscL;
            busR[routeSubOsc] += raw * gain * panGainSubOscR;
        }

        const auto noiseLevel = juce::jlimit (0.0f, 1.0f, noiseSmooth.getNextValue() + mods[(int) D::NoiseLevel]);

        if (noiseLevel > 0.0f)
        {
            const auto value = (random.nextFloat() * 2.0f - 1.0f) * noiseLevel * 0.5f;
            busL[routeSubOsc] += value;
            busR[routeSubOsc] += value;
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

        if (params.resonatorOn && resonatorAmount > 0.001f)
        {
            outL = resonatorL.process (outL);
            outR = resonatorR.process (outR);
        }

        const auto ampGain = ampValue * ampVelScale
                             * juce::jlimit (0.0f, 2.0f, 1.0f + mods[(int) D::AmpLevel]);

        left[startSample + i] += outL * ampGain;

        if (right != nullptr)
            right[startSample + i] += outR * ampGain;
        else
            left[startSample + i] += outR * ampGain;

        lastAmpValue = ampValue;
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

    lastSamplePosition1 = params.osc1.sampleMode ? samplePositionOf (sample1Unison[0], params.osc1.sample) : -1.0f;
    lastSamplePosition2 = params.osc2.sampleMode ? samplePositionOf (sample2Unison[0], params.osc2.sample) : -1.0f;
    lastSamplePositionSub = params.sub.sampleMode ? samplePositionOf (subSamples[0], params.sub.sample) : -1.0f;

    if (! ampEnv.isActive())
        clearCurrentNote();
}

void Voice::updateSubBlock (const float* mods, float filterEnvValue, float filter2EnvValue)
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
    const auto warp1 = juce::jlimit (0.0f, 1.0f, params.osc1.warpAmount + mods[(int) D::Osc1Warp]);
    const auto warp2 = juce::jlimit (0.0f, 1.0f, params.osc2.warpAmount + mods[(int) D::Osc2Warp]);
    const auto warpSub = juce::jlimit (0.0f, 1.0f, params.sub.warpAmount + mods[(int) D::SubWarp]);

    const auto pitch1 = std::exp2 ((params.osc1.semitones + params.osc1.cents / 100.0 + bendSemitones
                                    + (double) mods[(int) D::Osc1Pitch] * 48.0) / 12.0);
    const auto baseFreq1 = driftedFrequency * pitch1;

    for (int u = 0; u < numOsc1Unison; ++u)
    {
        const auto frequencyU = baseFreq1 * std::exp2 (unisonOffset1[u] / 12.0);
        osc1Unison[u].setFrequency (frequencyU);
        osc1Unison[u].setWarp (params.osc1.warpMode, warp1);

        if (u < VoiceParams::maxBufferedUnison)
        {
            string1Unison[u].setFrequency (frequencyU);
            sampleRatio1[u] = params.osc1.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
            grains1[u].setPlaybackRatio (sampleRatio1[u]);
            sample1Unison[u].setPlaybackRatio ((params.osc1.sample != nullptr ? params.osc1.sample->sampleRate / sampleRate : 1.0)
                                               * sampleRatio1[u]);
        }
    }

    const auto pitch2 = std::exp2 ((params.osc2.semitones + params.osc2.cents / 100.0 + bendSemitones
                                    + (double) mods[(int) D::Osc2Pitch] * 48.0) / 12.0);
    const auto baseFreq2 = driftedFrequency * pitch2;

    for (int u = 0; u < numOsc2Unison; ++u)
    {
        const auto frequencyU = baseFreq2 * std::exp2 (unisonOffset2[u] / 12.0);
        osc2Unison[u].setFrequency (frequencyU);
        osc2Unison[u].setWarp (params.osc2.warpMode, warp2);

        if (u < VoiceParams::maxBufferedUnison)
        {
            string2Unison[u].setFrequency (frequencyU);
            sampleRatio2[u] = params.osc2.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
            grains2[u].setPlaybackRatio (sampleRatio2[u]);
            sample2Unison[u].setPlaybackRatio ((params.osc2.sample != nullptr ? params.osc2.sample->sampleRate / sampleRate : 1.0)
                                               * sampleRatio2[u]);
        }
    }

    if (params.sub.level > 0.0f)
    {
        const auto subPitch = std::exp2 ((params.sub.semitones + (double) params.subOctaveOffset
                                          + params.sub.cents / 100.0 + bendSemitones
                                          + (double) mods[(int) D::SubPitch] * 48.0) / 12.0);

        for (int u = 0; u < numSubUnison; ++u)
        {
            const auto frequencyU = driftedFrequency * subPitch * std::exp2 (unisonOffsetSub[u] / 12.0);
            subUnison[u].setFrequency (frequencyU);
            subUnison[u].setWarp (params.sub.warpMode, warpSub);

            if (u < VoiceParams::maxBufferedUnison)
            {
                subStrings[u].setFrequency (frequencyU);
                sampleRatioSub[u] = params.sub.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
                grainsSub[u].setPlaybackRatio (sampleRatioSub[u]);
                subSamples[u].setPlaybackRatio ((params.sub.sample != nullptr ? params.sub.sample->sampleRate / sampleRate : 1.0)
                                                * sampleRatioSub[u]);
            }
        }
    }

    if (params.subOscLevel > 0.0f)
        subOsc.setFrequency (driftedFrequency * std::exp2 (((double) params.subOscOctave + bendSemitones) / 12.0));

    if (params.resonatorOn && params.resonatorAmount + blockMod (D::ResAmount) > 0.001f)
    {
        const auto offset = juce::jlimit (-24.0f, 24.0f, params.resonatorOffset + blockMod (D::ResOffset) * 12.0f);
        resonatorL.setTuning (driftedFrequency, offset, params.resonatorKeytrack);
        resonatorR.setTuning (driftedFrequency, offset, params.resonatorKeytrack);
    }

    updateFilterCoefficients (mods, filterEnvValue, filter2EnvValue);
}

void Voice::updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue)
{
    using D = Mod::Destination;

    const auto velocityEnvScale = 1.0f - params.filterVelocity + params.filterVelocity * velocityLevel;

    const auto fmOctaves1 = (double) (params.filter1Fm + mods[(int) D::Filter1Fm]) * (double) previousOsc2 * 4.0;
    const auto fmOctaves2 = (double) (params.filter2Fm + mods[(int) D::Filter2Fm]) * (double) previousOsc2 * 4.0;

    const auto keyOctaves1 = (double) params.filter1.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves1 = (double) (params.filter1.envAmount + mods[(int) D::Filter1Env] * envAmountRange)
                             * (double) filterEnvValue * (double) velocityEnvScale;
    const auto cutoff1 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter1.cutoffHz
                                           * std::exp2 (keyOctaves1 + envOctaves1 + fmOctaves1
                                                        + (double) mods[(int) D::Filter1Cutoff] * 6.0));
    const auto reso1 = juce::jlimit (0.0f, 1.0f, params.filter1.resonance + mods[(int) D::Filter1Reso]);

    for (auto* filter : { &filter1L, &filter1R })
        filter->setType (params.filter1.type, params.filter1.slope24);

    const auto morph1 = juce::jlimit (0.0f, 1.0f, params.filter1.morph + mods[(int) D::Filter1Morph]);
    const auto coefficients1 = FilterUnit::makeCoefficients (params.filter1.type, sampleRate, cutoff1, reso1, morph1);
    filter1L.setCoefficients (coefficients1);
    filter1R.setCoefficients (coefficients1);

    const auto keyOctaves2 = (double) params.filter2.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves2 = (double) (params.filter2.envAmount + mods[(int) D::Filter2Env] * envAmountRange)
                             * (double) filter2EnvValue * (double) velocityEnvScale;
    const auto cutoff2 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter2.cutoffHz
                                           * std::exp2 (keyOctaves2 + envOctaves2 + fmOctaves2
                                                        + (double) mods[(int) D::Filter2Cutoff] * 6.0));
    const auto reso2 = juce::jlimit (0.0f, 1.0f, params.filter2.resonance + mods[(int) D::Filter2Reso]);

    for (auto* filter : { &filter2L, &filter2R })
        filter->setType (params.filter2.type, params.filter2.slope24);

    const auto morph2 = juce::jlimit (0.0f, 1.0f, params.filter2.morph + mods[(int) D::Filter2Morph]);
    const auto coefficients2 = FilterUnit::makeCoefficients (params.filter2.type, sampleRate, cutoff2, reso2, morph2);
    filter2L.setCoefficients (coefficients2);
    filter2R.setCoefficients (coefficients2);
}

float Voice::sourceValue (Mod::Source source, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    const auto lfoSource = [this, sampleIndex] (int lfo, const float* shared)
    {
        if (params.lfos[lfo].perVoice)
            return lfoValues[lfo];

        return shared != nullptr ? shared[sampleIndex] : 0.0f;
    };

    switch (source)
    {
        case Mod::Source::Lfo1:       return lfoSource (0, params.lfo1);
        case Mod::Source::Lfo2:       return lfoSource (1, params.lfo2);
        case Mod::Source::Lfo3:       return lfoSource (2, params.lfo3);
        case Mod::Source::Lfo4:       return lfoSource (3, params.lfo4);
        case Mod::Source::ModEnv:     return modValue;
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
        case Mod::Source::Env4:       return env4Value;
        case Mod::Source::FilterEnv2: return filter2Value;
        case Mod::Source::None:
        case Mod::Source::Count:
        default:                      return 0.0f;
    }
}
