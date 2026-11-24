#include "Voice.h"
#include "FastMath.h"
#include "Generative.h"
#include "StretchRenderer.h"

#include <array>
#include <cmath>
#include "VoiceInternal.h"

Voice::Voice()
{
    filterDestinationMarks();
    if (fixedRandomSeeds().load())
    {
        random.setSeed (12345);
        driftRandom.setSeed (54321);
    }
}

// REPEATABLE RANDOM: every generator of the voice from a given seed (the
// processor's play start), so a render repeats.
void Voice::reseedRandom (juce::int64 salt)
{
    random.setSeed (12345 + salt);
    driftRandom.setSeed (54321 + salt);
    fmNoiseRandom.setSeed (31337 + salt);
    lfoPoolRandom.setSeed (27183 + salt);
    liveGrainRandom.setSeed (0x1f3a + salt);
    zoneRandom.setSeed (0x2b17 + salt);
    sampleStartRandom.setSeed (0x5a17 + salt);
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
    noiseEnableSmooth.reset (newRate, 0.02);
    subOscColourSmooth.reset (newRate, 0.02);
    noiseGenerator.prepare (newRate);
    fmNoiseGenerator.prepare (newRate);
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

    // One pluck-shaping scratch for all this voice's strings (they excite
    // one at a time, on the thread rendering this voice), sized here so a
    // pluck never allocates on the audio thread.
    stringScratch.assign ((size_t) KarplusStrong::bufferSizeFor (newRate), 0.0f);

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        oscBank[osc].setSampleRate (newRate);

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
        {
            stringFor (osc, u).setSharedScratch (&stringScratch);
            stringFor (osc, u).prepare (newRate);
            sampleUnison[osc][u].prepare (newRate);
            sampleLayer[osc][u].prepare (newRate);
            grains[osc][u].prepare (newRate);
        }
    }

    resonatorL.prepare (newRate);
    resonatorR.prepare (newRate);
    materialBodyL.prepare (newRate);
    materialBodyR.prepare (newRate);
    westGateL.prepare (newRate);
    westGateR.prepare (newRate);
    westFolder.reset();

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R,
                          &bothFilter1L, &bothFilter1R, &bothFilter2L, &bothFilter2R })
        filter->prepare (newRate);
    filter1Linked = filter2Linked = true;
}

void Voice::syncSamplePlayers()
{
    const auto setup = [] (const VoiceParams::OscParams& osc, const SampleZone* zone, const SampleData* copy, bool sameSound,
                           SamplePlayer* players, GranularOsc* clouds, float startMod, float endMod, float level)
    {
        const auto start = juce::jlimit (0.0f, 0.98f, osc.sampleStart + startMod);
        const auto end = juce::jlimit (start + 0.01f, 1.0f, osc.sampleEnd + endMod);
        // A multisample plays the region the note picked, with its loop; a
        // key outside every zone, with OUTSIDE ZONES on Silence, nothing.
        const auto* sample = zone != nullptr ? zone->data.get()
                             : osc.sample != nullptr && ! osc.sample->zones.empty() && (osc.sampleSilenceOutside || osc.sample->isSliced()) ? nullptr
                                                                                                                 : osc.sample;
        // A rendered copy is the whole sample stretched, so the region's
        // start and end stay where they were; a region loop scales with it.
        const auto loopScale = copy != nullptr && sample != nullptr && sample->getNumSamples() > 0
                                   ? (double) copy->getNumSamples() / (double) sample->getNumSamples()
                                   : 1.0;

        SamplePlayer::Params sampleParams;
        sampleParams.sample = copy != nullptr ? copy : sample;
        sampleParams.keepPosition = sameSound;
        if (zone != nullptr)
        {
            sampleParams.gain = zone->gain * level;
            if (zone->loop)
            {
                sampleParams.loopStart = (int) std::lround (zone->loopStart * loopScale);
                sampleParams.loopEnd = (int) std::lround (zone->loopEnd * loopScale);
            }
        }
        sampleParams.loop = osc.sampleLoop;
        sampleParams.ownLoop = osc.sampleLoopOwn;
        sampleParams.loopFrom = osc.sampleLoopStart;
        sampleParams.loopTo = osc.sampleLoopEnd;
        sampleParams.loopCrossfade = osc.sampleLoopXfade;
        sampleParams.playback = osc.samplePlayback;
        sampleParams.pingPong = osc.samplePingPong;
        sampleParams.startRandom = osc.sampleStartRandom;
        sampleParams.startSafe = osc.sampleStartSafe;
        sampleParams.quality = osc.sampleQuality;
        // Step 32: a slice (or zone) can be reversed on its own.
        sampleParams.reverse = osc.sampleReverse != (zone != nullptr && zone->reverse);
        sampleParams.start = start;
        sampleParams.end = end;
        sampleParams.fadeIn = osc.sampleFadeIn;
        sampleParams.fadeOut = osc.sampleFadeOut;
        // TIME's engines: the copy plays through the live stretch too, which
        // at the copy's own speed passes it through, and covers bends,
        // cents and tempo changes mid-note.
        sampleParams.stretch = osc.sampleStretch || osc.sampleEngine >= 0;

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            if (! players[u].setParamsIsNoOp (sampleParams))
                players[u].setParams (sampleParams);

        if (osc.granularMode && clouds != nullptr)
        {
            GranularOsc::Params grainParams;
            grainParams.sample = sample;
            grainParams.position = juce::jlimit (0.0f, 1.0f, osc.grainPosition + startMod);
            grainParams.scan = osc.grainScan;
            grainParams.freeze = osc.grainFreeze;
            grainParams.sizeMs = osc.grainSizeMs;
            grainParams.density = osc.grainDensity;
            grainParams.spray = osc.grainSpray;
            grainParams.pitchSpray = osc.grainPitch;
            grainParams.spread = osc.grainSpread;
            grainParams.reverse = osc.sampleReverse;
            grainParams.live = osc.grainLive;
            grainParams.liveWrite = osc.liveWrite;

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
        {
            const auto& settings = params.oscillators[osc];
            const auto* sample = settings.sample;
            sampleZone[osc] = sample != nullptr && ! sample->zones.empty() ? sample->zoneFor (lastNote, lastVelocity, settings.sampleSilenceOutside || sample->isSliced(),
                                                                                                    settings.sampleRoundRobin, zonePick[osc])
                                                                                    : nullptr;
            // Step 37: near a velocity layer's edge, the layer across it too.
            layerKeep[osc] = 1.0f;
            layerGain[osc] = 0.0f;
            layerZone[osc] = sampleZone[osc] != nullptr && ! settings.granularMode
                                 ? sample->blendPartner (*sampleZone[osc], lastNote, lastVelocity, settings.sampleVelBlend,
                                                         settings.sampleRoundRobin, zonePick[osc], layerKeep[osc], layerGain[osc])
                                 : nullptr;
            const auto* origin = sampleZone[osc] != nullptr ? sampleZone[osc]->data.get() : sample;
            const auto* copy = settings.granularMode ? nullptr : stretchCopyFor (osc, origin);
            const auto sameSound = origin != nullptr && origin == stretchOrigin[osc];
            stretchOrigin[osc] = origin;
            stretchCopy[osc] = copy;
            setup (settings, sampleZone[osc], copy, sameSound, sampleUnison[osc], grains[osc],
                   blockMod (startDestinations[osc]), blockMod (endDestinations[osc]), layerKeep[osc]);
            const auto* layer = layerZone[osc] != nullptr ? layerZone[osc]->data.get() : nullptr;
            if (layer != nullptr)
                setup (settings, layerZone[osc], nullptr, layer == layerOrigin[osc], sampleLayer[osc], nullptr,
                       blockMod (startDestinations[osc]), blockMod (endDestinations[osc]), layerGain[osc]);
            layerOrigin[osc] = layer;
        }
    stretchNew = false;
}

// The copy of origin TIME's engine rendered for this note (asked for if it
// isn't there yet), or null to play the original.
const SampleData* Voice::stretchCopyFor (int osc, const SampleData* origin)
{
    const auto& settings = params.oscillators[osc];
    if (params.stretchRenderer == nullptr || settings.sampleEngine < 0 || origin == nullptr || origin->getNumSamples() < 2
        || ! settings.sampleMode)
        return nullptr;

    // The note's key: its length from the tempo and its semitones from the
    // note, picked once per note (and again if the sample or engine changes).
    if (stretchNew || origin != stretchOrigin[osc] || settings.sampleEngine != stretchEngine[osc])
    {
        const auto regionSeconds = origin->sampleRate > 0.0 ? (double) juce::jmax (0.0f, settings.sampleEnd - settings.sampleStart)
                                                                  * origin->getNumSamples() / origin->sampleRate
                                                            : 0.0;
        const auto tempo = SampleTempo::tempoRatio (settings.sampleSync, settings.sampleBpm, regionSeconds, params.hostBpm);
        stretchStep[osc] = StretchRenderer::lengthStep (1.0 / tempo);
        stretchEngine[osc] = settings.sampleEngine;
        stretchSemis[osc] = 0;
        if (settings.sampleTuned)
        {
            const auto* zone = sampleZone[osc];
            const auto frequency = settings.tuneMode == OscTuning::Semitones
                                       ? baseFrequency * std::exp2 ((settings.semitones + (osc == 2 ? params.subOctaveOffset : 0)
                                                                     + settings.cents / 100.0) / 12.0)
                                   : settings.tuneMode == OscTuning::Fixed
                                       ? oscFrequencyFactor (settings)
                                       : baseFrequency * settings.ratio * std::exp2 ((settings.semitones + settings.cents / 100.0) / 12.0);
            const auto root = zone != nullptr ? zone->rootHz() : plainRootHz (settings.sampleRoot);
            if (frequency > 0.0 && root > 0.0)
                stretchSemis[osc] = juce::jlimit (-48, 48, (int) std::lround (12.0 * std::log2 (frequency / root)));
        }
    }

    if (stretchStep[osc] == 0 && stretchSemis[osc] == 0)
        return nullptr; // the copy would be the original

    return params.stretchRenderer->get (osc, origin, settings.sampleEngine, stretchStep[osc], stretchSemis[osc]);
}

void Voice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    if (paramsSource != nullptr)
    {
        params = *paramsSource;
        voiceKnobsApplied = false;
    }
    paramsFresh = glidesSnap = true; // a note starts on the knobs' values, not eased in

    const auto mono = monoPending;
    const auto legato = mono && monoLegato;
    releaseSilentSamples = 0;
    releasePeak = 0.0f;
    heldAsleep = false;
    heldSilentSamples = 0;
    fadeTotal = fadeRemaining = 0;
    sounded = false;
    ampGainFade = 1.0f;
    const auto keepRunning = mono && monoKeepRunning;
    const auto glide = ! mono || monoGlide;
    monoPending = false;
    monoKeepRunning = false;

    baseFrequency = params.tuning != nullptr ? params.tuning->getFrequency (midiNoteNumber)
                                             : juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    // Stretch tuning (Railsback): bass a little flat, treble sharp, about
    // +/-35 cents at the ends of the keyboard when fully on.
    if (params.stretch > 0.0f)
    {
        const auto distance = (double) (midiNoteNumber - 60) / 40.0;
        const auto cents = (double) params.stretch * 35.0 * (distance < 0.0 ? -1.0 : 1.0) * distance * distance;
        baseFrequency *= std::exp2 (cents / 1200.0);
    }

    // A hand-tuned instrument is never quite in tune: each note lands up to
    // 40 cents off, a fresh draw every time (sharp or flat, or flat only).
    if (params.tuneLoose > 0.0f)
    {
        const auto draw = driftRandom.nextFloat();
        const auto shift = params.tuneLooseBoth ? draw * 2.0f - 1.0f : -draw;
        baseFrequency *= std::exp2 ((double) shift * (double) params.tuneLoose / 1200.0);
    }

    pitchNote = midiNoteNumber;
    keyTrackValue = juce::jlimit (-1.0f, 1.0f, (float) (midiNoteNumber - 60) / 48.0f);
    keyTrackOctaves = (float) (midiNoteNumber - 60) / 12.0f;

    // Poly: glide from the previous note played, not from this voice's own
    // last pitch (which depends on which voice the synth handed out). Off
    // until ilana decides: it changes how poly GLIDE presets sound.
    if (polyGlideFromLastNote && ! mono && params.glideTime > 0.001f && shared != nullptr && shared->lastNoteFrequency > 0.0)
    {
        currentFrequency = shared->lastNoteFrequency;
        hasPlayedNote = true;
    }
    // With GLIDE off the note starts at its own pitch. (It used to start at
    // this voice's last pitch and snap on the first sub-block, which was
    // inaudible for oscillators but not for what startNote sets up from the
    // pitch: the Modal Piano chose its recorded key, attack and board from
    // the voice's previous note, so a low key on a voice that last played
    // a high one rang as a few pitched-down partials, a sine thud.)
    else if (! hasPlayedNote || ! glide || params.glideTime <= 0.001f)
    {
        currentFrequency = baseFrequency;
        hasPlayedNote = true;
    }

    if (shared != nullptr)
    {
        shared->lastNoteFrequency = baseFrequency;

        // Mod wheel, expression and pressure as they are now, not as this
        // voice last heard them.
        const auto channel = playingChannel();
        modWheel = shared->modWheel[channel];
        expressionValue = shared->expression[channel];
        aftertouchValue = shared->pressure[channel];
        masterBendWheel = params.mpe ? shared->masterBendWheel : 0.0;
    }
    else
        masterBendWheel = 0.0;

    pitchWheelMoved (currentPitchWheelPosition);

    // Legato: the note just changes pitch; envelopes, phases and filters
    // carry on from where they are.
    if (legato)
        return;

    velocityLevel = velocity;
    lastNote = midiNoteNumber;
    stretchNew = true;
    lastVelocity = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
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

        // M8.1: own seeds, so the voice's random sequence is untouched.
        const auto& sim = params.lfos[lfo].sim;
        lfoSmoothers[lfo].reset();
        lfoSeenTriggers[lfo] = params.lfos[lfo].triggerCount;
        if (LfoSimShapes::isSim (sim.shape))
        {
            lfoSims[lfo].reset (sim, ++lfoSimNotes * 7919u + (std::uint32_t) midiNoteNumber * 131u
                                         + (std::uint32_t) (velocity * 1000.0f) + (std::uint32_t) lfo * 104729u);
            if (sim.shape == LfoSimShapes::Pendulum && params.lfos[lfo].kick)
                lfoSims[lfo].trigger (sim, 0, true, velocity);
            float a = 0.0f, b = 0.0f;
            lfoSims[lfo].next (sim, 0.0, a, b);
            lfoValues[lfo] = a;
            lfoValuesB[lfo] = b;
        }
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
        // The new key holds its note: a One Shot's hold on the last release
        // must not release it when the sample ends.
        oneShotHold = false;
        // The Operator EG was in its release (a staccato line): it starts
        // again, or the new note played the old one's fade and the voice
        // ended under the held key. A tied line keeps it running.
        if (dx7Playing && dx7Note.isReleased())
            startOperatorEg (midiNoteNumber);
        return;
    }

    randomValue = random.nextFloat() * 2.0f - 1.0f;
    voicePan = random.nextFloat() * 2.0f - 1.0f;

    // Per-note routes into the knobs (velocity, key, this note's RANDOM):
    // in place before the strings and players below start from them.
    renderStart = 0; // (the shared LFO buffers: read at their first sample here)
    applyVoiceKnobs (true);

    for (int u = 0; u < VoiceParams::maxUnison; ++u)
    {
        // The first eight keep their original spacing (eighths of a cycle) so
        // existing patches start the same; voices 9-16 sit in between.
        const auto basePhase = u < 8 ? (double) u / 8.0 : (double) (u - 8) / 8.0 + 1.0 / 16.0;
        const auto jitter = (double) random.nextFloat() * (double) params.unisonRandom;

        auto phase = basePhase + jitter;
        phase -= std::floor (phase);

        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
            oscBank[osc].resetPhase (u, phase);
    }

    for (auto& bank : oscBank)
        bank.switchTableHard();

    resonatorL.reset();
    resonatorR.reset();
    materialBodyL.reset();
    materialBodyR.reset();
    // M8.3: the gate is struck by the note (a vactrol keeps its state: a
    // new strike on a still-lit cell starts from where it is).
    westStrikeRemaining = params.west.on ? juce::jmax (1, (int) (WestCoastTuning::get().strikeSeconds * sampleRate)) : 0;
    bodyStrikePending = true;
    bodyTailSamplesRemaining = legacyMaterialBody()
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

        // A stereo string route: the twin (or each unison string) starts on its own side of it.
        const auto unison = juce::jlimit (1, VoiceParams::maxBufferedUnison, settings.unison);
        stringTwin[osc] = settings.stringStereo && unison == 1;
        twinAsleep[osc] = stringTwin[osc] && ! disableTwinSleep && stereoStringAtRest (osc);
        const auto count = stringTwin[osc] ? 2 : unison;
        if (stringTwin[osc])
            stringFor (osc, 1).matchNoise (stringFor (osc, 0));

        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
        {
            auto& string = stringFor (osc, u);
            string.setFrequency (pitch);
            configureString (string, settings, u < count ? stringSide (osc, u, count) : 0.0f);
            string.trigger (velocity);
        }
    }

    // Step 37: each oscillator's round-robin alternate, picked once per note.
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto mode = params.oscillators[osc].sampleRoundRobin;
        zonePick[osc] = mode == 1 && params.roundRobin != nullptr ? params.roundRobin->next (osc, midiNoteNumber)
                        : mode == 2                                ? (unsigned) zoneRandom.nextInt (1 << 30)
                                                                   : 0u;
    }

    syncSamplePlayers();

    const auto bufferedCount = [] (int unison) { return juce::jlimit (1, VoiceParams::maxBufferedUnison, unison); };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];

        if (settings.sampleMode && params.oscillatorEnabled[osc] && settings.sample != nullptr)
            for (int u = 0; u < bufferedCount (settings.unison); ++u)
            {
                // Step 35: its own generator, drawn only while a random start
                // is set, so older patches' random sequences don't shift.
                const auto startUnit = settings.sampleStartRandom > 0.0f ? sampleStartRandom.nextFloat() : 0.0f;
                sampleUnison[osc][u].trigger (startUnit);
                sampleLayer[osc][u].trigger (startUnit); // step 37: the blended layer starts with it
                // Live grains (ilanaSynth FX) draw their seeds from their own
                // seeded generator, so an input renders the same every time;
                // sample grains stay free, as the plugin's analog randomness is.
                grains[osc][u].reset ((juce::uint32) (midiNoteNumber * 7919 + u * 104729
                                                      + (settings.grainLive ? liveGrainRandom.nextInt() : random.nextInt())));
            }

        lastStringMode[osc] = settings.stringMode;
        lastSampleMode[osc] = settings.sampleMode;
        lastEnabled[osc] = params.oscillatorEnabled[osc];
        previousOsc[osc] = 0.0f;
    }

    driftValue = driftRandom.nextFloat() * 2.0f - 1.0f;
    driftTarget = driftValue;

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R,
                          &bothFilter1L, &bothFilter1R, &bothFilter2L, &bothFilter2R })
        filter->reset();
    filter1Linked = filter2Linked = true;
    filter1Open = filter2Open = false;
    filter2BypassMix = params.filter2Off ? 1.0f : 0.0f;
    filter1Fade = 0;
    filter1Ran = false;
    filter2Fade = 0;
    filter2Ran = false;
    openFilter1L.reset();
    openFilter1R.reset();
    openFilter2L.reset();
    openFilter2R.reset();

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        frameSmooth[osc].setCurrentAndTargetValue (params.oscillators[osc].frame);
        levelSmooth[osc].setCurrentAndTargetValue (params.oscillators[osc].level);
        oscEnableSmooth[osc].setCurrentAndTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
    }
    noiseSmooth.setCurrentAndTargetValue (params.noiseLevel);
    noiseEnableSmooth.setCurrentAndTargetValue (params.noiseEnabled ? 1.0f : 0.0f);
    subOscColourSmooth.setCurrentAndTargetValue (params.subOscColour);
    noiseGenerator.reset();
    fmNoiseGenerator.reset();
    noiseLow = 0.0f;
    warpNoiseLow = 0.0f;
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

    // The Operator EG: gains for the oscillators whose ENVELOPE picks it.
    dx7Playing = params.operatorEgUsed;
    if (dx7Playing)
        startOperatorEg (midiNoteNumber);
}

void Voice::startOperatorEg (int midiNoteNumber)
{
    // Scaling follows the key moved by KEY OFFSET (a DX7 voice's
    // transpose; the oscillators get its pitch from their SEMI knobs).
    const auto& eg = params.operatorEg;
    const auto transposed = juce::jlimit (0, 127, midiNoteNumber + (int) eg[OperatorEg::keyOffsetByte] - 24);
    std::array<bool, 6> carriers {};
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        carriers[(size_t) osc] = params.oscOut[osc] && params.oscillators[osc].ampEnv == OperatorEg::envelopeChoice;
    dx7Note.start (eg, transposed, lastVelocity, sampleRate, carriers);
    dx7Settings = eg;
    dx7Previous.fill (0.0f);
    dx7Current = dx7Note.getGains();
    dx7Count = 0;
}

void Voice::resetForNewPatch()
{
    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R,
                          &bothFilter1L, &bothFilter1R, &bothFilter2L, &bothFilter2R })
        filter->reset();
    filter1Linked = filter2Linked = true;
    filter1Open = filter2Open = false;
    filter2BypassMix = params.filter2Off ? 1.0f : 0.0f;
    filter1Fade = 0;
    filter1Ran = false;
    filter2Fade = 0;
    filter2Ran = false;
    openFilter1L.reset();
    openFilter1R.reset();
    openFilter2L.reset();
    openFilter2R.reset();
    westGateL.reset();
    westGateR.reset();
    westFolder.reset();
    westStrikeRemaining = 0;
    resonatorL.reset();
    resonatorR.reset();
    materialBodyL.reset();
    materialBodyR.reset();
    bodyStrikePending = false;
    bodyTailSamplesRemaining = 0;

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        // A voice that has not played since its last reset still has silent
        // strings and samples: skip clearing their buffers, so a patch load
        // does not clear every voice's delay lines in one audio block.
        if (hasPlayedNote)
            for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            {
                stringFor (osc, u).reset();
                sampleUnison[osc][u].reset();
                sampleLayer[osc][u].reset();
            }
        feedbackHistory[osc] = feedbackFiltered[osc] = previousOsc[osc] = 0.0f;
    }
    fmNoiseState = 0.0f;

    // As a new instance: no note to glide from, and the seeded generators
    // start their sequences again (the analog ones stay free in the plugin,
    // as the constructor leaves them).
    if (fixedRandomSeeds().load())
    {
        random.setSeed (12345);
        driftRandom.setSeed (54321);
    }
    fmNoiseRandom.setSeed (31337);
    lfoPoolRandom.setSeed (27183);
    liveGrainRandom.setSeed (0x1f3a);
    zoneRandom.setSeed (0x2b17);
    sampleStartRandom.setSeed (0x5a17);
    lfoSimNotes = 0;
    driftValue = driftTarget = 0.0f;
    hasPlayedNote = false;

    if (shared != nullptr)
        shared->lastNoteFrequency = 0.0;
}

void Voice::silenceAfterNonFinite()
{
    monoPending = monoKeepRunning = false;
    heldAsleep = false;
    oneShotHold = false;
    brokenForTest = false;
    stopNote (0.0f, false);
    // (resetForNewPatch also forgets the last note for GLIDE: the other
    // voices' next note still glides from it.)
    const auto lastNote = shared != nullptr ? shared->lastNoteFrequency : 0.0;
    resetForNewPatch();
    if (shared != nullptr)
        shared->lastNoteFrequency = lastNote;
}

void Voice::stopNote (float, bool allowTailOff)
{
    // The synth hard-stops a voice before reusing it; a mono note change
    // keeps the voice sounding instead.
    if (! allowTailOff && monoPending && monoKeepRunning)
        return;

    // Already released: the synth stops every voice on a note when that note
    // starts again, released ones too. Restarting the release here kept a
    // looped clip's old voices alive whenever the release outlasted the loop.
    if (allowTailOff && ! noteHeld)
        return;

    noteHeld = false;

    if (allowTailOff)
    {
        // Step 33: a Sustain Loop leaves its loop now; a One Shot still
        // playing holds the envelopes' release until it ends.
        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        {
            for (auto& player : sampleUnison[osc])
                player.release();
            for (auto& player : sampleLayer[osc])
                player.release();
        }
        if (oneShotPlaying())
        {
            oneShotHold = true;
            return;
        }
        releaseEnvelopes();
    }
    else
    {
        oneShotHold = false;
        bodyTailSamplesRemaining = 0;
        ampEnv.reset();
        filterEnv.reset();
        filter2Env.reset();
        modEnv.reset();
        env4.reset();
        for (auto& env : extraEnvs)
            env.reset();
        extraEnvValues.fill (0.0f); // the ENV page monitors these
        dx7Playing = false;
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        lastFilterValue = 0.0f;
        lastModValue = 0.0f;
        clearCurrentNote();
    }
}

bool Voice::oneShotPlaying() const
{
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        // Granular reads the sample through its cloud, never through these
        // players, so they would never reach the end and hold the release.
        if (params.oscillators[osc].sampleMode && ! params.oscillators[osc].granularMode
            && params.oscillators[osc].samplePlayback == SamplePlayer::oneShot
            && params.oscillatorEnabled[osc])
            for (const auto& player : sampleUnison[osc])
                if (player.holdsRelease())
                    return true;
    return false;
}

void Voice::releaseEnvelopes()
{
    oneShotHold = false;
    {
        if (legacyMaterialBody())
            bodyTailSamplesRemaining = (int) (sampleRate * juce::jmin (10.0,
                1.5 * (0.08 + 7.92 * (double) params.resonatorDecay * (double) params.resonatorDecay)));
        if (dx7Playing)
            dx7Note.keyUp();
        ampEnv.noteOff();
        filterEnv.noteOff();
        filter2Env.noteOff();
        modEnv.noteOff();
        env4.noteOff();
        for (auto& env : extraEnvs)
            env.noteOff();
    }
}

void Voice::pitchWheelMoved (int newValue)
{
    // MPE: channel 1 is the master channel, its bend reaches every voice
    // through setMasterBend; a note on it has no member bend of its own.
    memberBendWheel = params.mpe && playingChannel() == 1 ? 0.0 : ((double) newValue - 8192.0) / 8192.0;
    updateBend();
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

    if (LfoSimShapes::isSim (lfoParams.shape))
        return lfoValues[lfo];

    if (LfoShapes::isStateful (lfoParams.shape))
        return lfoChaos[lfo].value (lfoParams.shape, lfoPhases[lfo]);

    return lfoShapeAt (lfoParams.shape, lfoPhases[lfo], lfoHolds[lfo], lfoParams.steps,
                       lfoParams.custom, lfoParams.customSize);
}

// An LFO is read at the control rate when only slow routes hear it (see
// evaluateModsRated) and its value is a function of its phase (and hold):
// then its shape is worked out only at the samples those routes are
// evaluated, while its phase still moves every sample. Vital runs an LFO at
// a control rate unless an audio-rate route needs it.
void Voice::classifySlowLfos()
{
    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        const auto& settings = params.lfos[lfo];
        const auto shape = settings.shape;
        lfoSlow[lfo] = modSlotsPrepared && settings.perVoice && ! settings.needsB
                       && ! LfoSimShapes::isSim (shape) && ! LfoShapes::isStateful (shape) && ! LfoShapes::isPhysics (shape)
                       && shape != LfoShapes::Chaos && ! (lfoSmoothCoefficients[lfo] < 1.0f);
    }
    for (int k = 0; k < numGroupSlots[1]; ++k)
    {
        const auto& slot = params.modSlots[groupSlots[1][(size_t) k]];
        for (const auto source : { slot.source, slot.aux })
        {
            if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
                lfoSlow[lfo] = false;
            if (const auto lfo = Mod::lfoBIndexFor (source); lfo >= 0)
                lfoSlow[lfo] = false;
        }
    }
    // The WEST gate reads its source every sample (like a per-sample route).
    if (params.west.on && params.west.source != 0)
    {
        if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) params.west.source); lfo >= 0)
            lfoSlow[lfo] = false;
        if (const auto lfo = Mod::lfoBIndexFor ((Mod::Source) params.west.source); lfo >= 0)
            lfoSlow[lfo] = false;
    }
}

// Like the slow LFOs: an envelope (other than ENV 1) whose value only
// reaches what is read at the control points (slow mod routes, the
// filters' sub-block update) is worked out only there and just moves on in
// between. Not one that is an oscillator's amp or warp envelope, the WEST
// gate's source, a per-sample route's source, or (ENV 2 and 3) the filters'
// envelope while filter FM updates them every sample.
void Voice::classifySlowEnvelopes()
{
    const auto envelopeOf = [] (Mod::Source source)
    {
        switch (source)
        {
            case Mod::Source::FilterEnv:  return 1;
            case Mod::Source::FilterEnv2: return 2;
            case Mod::Source::ModEnv:     return 3;
            case Mod::Source::Env4:       return 4;
            default: break;
        }
        if (source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
            return 5 + ((int) source - (int) Mod::Source::Env6);
        return -1;
    };

    for (auto& slow : envSlow)
        slow = modSlotsPrepared && ! disableSlowEnvelopes;
    envSlow[0] = envSlow[16] = false;
    if (modFilterFm)
        envSlow[1] = envSlow[2] = false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        if (! params.oscillatorEnabled[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f)
            continue;
        const auto& settings = params.oscillators[osc];
        envSlow[(size_t) juce::jlimit (0, 16, settings.ampEnv)] = false;
        if (settings.pdEnv > 0)
            envSlow[(size_t) juce::jlimit (0, 16, settings.pdEnv - 1)] = false;
    }
    if (params.west.on && params.west.source != 0)
        if (const auto env = envelopeOf ((Mod::Source) params.west.source); env >= 0)
            envSlow[(size_t) env] = false;
    for (int k = 0; k < numGroupSlots[1]; ++k)
    {
        const auto& slot = params.modSlots[groupSlots[1][(size_t) k]];
        for (const auto source : { slot.source, slot.aux })
            if (const auto env = envelopeOf (source); env >= 0)
                envSlow[(size_t) env] = false;
    }
}

void Voice::advanceVoiceLfos (int sampleIndex)
{
    for (int index = 0; index < numPerVoiceLfos; ++index)
    {
        const auto lfo = perVoiceLfos[index];
        const auto shape = params.lfos[lfo].shape;

        if (LfoSimShapes::isSim (shape))
        {
            float a = 0.0f, b = 0.0f;
            lfoSims[lfo].next (params.lfos[lfo].sim, lfoIncrements[lfo], a, b);
            if (lfoSmoothCoefficients[lfo] < 1.0f)
                lfoSmoothers[lfo].process (a, b, lfoSmoothCoefficients[lfo]);
            lfoValues[lfo] = a;
            lfoValuesB[lfo] = b;
            auto next = lfoPhases[lfo] + lfoIncrements[lfo];
            lfoPhases[lfo] = next - std::floor (next);
            continue;
        }

        if (shape == LfoShapes::Chaos)
            lfoChaos[lfo].advance (lfoIncrements[lfo]);
        else if (LfoShapes::isPhysics (shape))
            lfoChaos[lfo].advancePhysics (shape, lfoIncrements[lfo], params.lfos[lfo].physA, params.lfos[lfo].physB);

        if (! lfoSlow[lfo] || (sampleIndex & (modControlInterval - 1)) == 0)
            lfoValues[lfo] = voiceLfoValue (lfo);

        if (lfoSmoothCoefficients[lfo] < 1.0f || params.lfos[lfo].needsB)
        {
            auto a = lfoValues[lfo];
            auto b = a;
            if (params.lfos[lfo].needsB && ! LfoShapes::isStateful (shape) && shape != LfoShapes::SampleHold)
                b = lfoShapeAt (shape, lfoPhases[lfo] + 0.25 - std::floor (lfoPhases[lfo] + 0.25), lfoHolds[lfo],
                                params.lfos[lfo].steps, params.lfos[lfo].custom, params.lfos[lfo].customSize);
            if (lfoSmoothCoefficients[lfo] < 1.0f)
                lfoSmoothers[lfo].process (a, b, lfoSmoothCoefficients[lfo]);
            lfoValues[lfo] = a;
            lfoValuesB[lfo] = b;
        }

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

// The per-note routes into plain knobs (VoiceParams::voiceKnobs): summed
// for this voice, at its note-on and then once a block, and written into its
// own copy of the knobs, as the synth moves the knob for a synth-wide route
// (an offset in the knob's 0..1 travel, clamped to it). A RANDOM, velocity
// or key route thus holds for the whole note: a new note draws its own and
// leaves the held ones alone. The knob's value is kept, to put back first.
void Voice::applyVoiceKnobs (bool noteStart)
{
    const auto knob = [this] (int k) -> float&
    {
        return *reinterpret_cast<float*> (reinterpret_cast<char*> (&params) + params.voiceKnobs[(size_t) k].offset);
    };

    if (voiceKnobsApplied)
    {
        for (int k = 0; k < params.numVoiceKnobs; ++k)
            knob (k) = voiceKnobBase[(size_t) k];
        voiceKnobsApplied = false;
    }

    if (params.numVoiceKnobs == 0 || params.numModSlots == 0)
        return;

    // At note-on the envelopes have not started: they read 0.
    const auto read = [&] (Mod::Source source)
    {
        if (noteStart && source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
            return 0.0f;
        return noteStart ? sourceValue (source, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f)
                         : sourceValue (source, 0, lastAmpValue, lastFilterValue, lastFilter2Value, lastModValue, lastEnv4Value);
    };

    std::array<float, VoiceParams::maxVoiceKnobs> sums {};
    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];
        if (! isVoiceKnobSlot (slot))
            continue;
        auto k = 0;
        while (k < params.numVoiceKnobs && params.voiceKnobs[(size_t) k].destination != slot.destination)
            ++k;
        if (k == params.numVoiceKnobs)
            continue;

        auto value = Mod::shape (slot, read (slot.source));
        if (slot.aux != Mod::Source::None)
            value *= Mod::auxScale (slot.aux, read (slot.aux));
        sums[(size_t) k] += slot.depth * value;
    }

    for (int k = 0; k < params.numVoiceKnobs; ++k)
    {
        auto& value = knob (k);
        voiceKnobBase[(size_t) k] = value;
        const auto* range = params.voiceKnobs[(size_t) k].range;
        if (sums[(size_t) k] == 0.0f || range == nullptr)
            continue;
        const auto moved = range->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, range->convertTo0to1 (range->snapToLegalValue (value)) + sums[(size_t) k]));
        if (std::isfinite (moved))
            value = moved;
    }
    voiceKnobsApplied = true;
}

float Voice::bowedNoiseWeight() const noexcept
{
    if (! isVoiceActive())
        return 0.0f;
    auto bow = 0.0f;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode
            && params.oscillators[osc].stringExcite == (int) KarplusStrong::Excite::ModalBowed)
            for (int u = 0; u < stringsPlaying (osc); ++u)
                bow = juce::jmax (bow, stringFor (osc, u).bowedNoiseWeight());
    if (bow <= 0.0f)
        return 0.0f;
    const auto ampVelScale = 1.0f - params.ampVelocity + params.ampVelocity * velocityLevel;
    return bow * juce::jmax (0.0f, ampVelScale) * juce::jlimit (0.0f, 1.0f, lastAmpValue);
}

void Voice::setBowedNoiseShare (float share) noexcept
{
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        for (int u = 0; u < VoiceParams::maxBufferedUnison; ++u)
            stringFor (osc, u).setBowedNoiseShare (share);
}

void Voice::reExcite (float level)
{
    if (! isVoiceActive() || ! isKeyDown())
        return;

    // A struck string sounds again: a sleeping held note wakes.
    heldAsleep = false;
    heldSilentSamples = 0;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillators[osc].stringMode && params.oscillatorEnabled[osc])
            for (int u = 0; u < stringsPlaying (osc); ++u)
                stringFor (osc, u).trigger (juce::jlimit (0.0f, 1.0f, level * velocityLevel * 1.25f));
}

void Voice::evaluateMods (float* mods, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    for (int d = 0; d < params.numActiveDestinations; ++d)
        mods[params.activeDestinations[d]] = 0.0f;
    mods[(int) Mod::Destination::Count] = mods[(int) Mod::Destination::Count + 1] = 0.0f;

    if (params.anyExtendedFmMods)
        fmCellMods.fill (0.0f);
    if (params.anySpectralMods)
        spectralMods.fill (0.0f);

    if (modSlotsPrepared)
    {
        for (int s = 0; s < params.numModSlots; ++s)
        {
            const auto targetIndex = slotTargets[(size_t) s];
            if (targetIndex == -1)
                continue;
            auto* target = targetIndex >= 0 ? &mods[targetIndex]
                           : targetIndex <= -1000 ? &spectralMods[(size_t) (-1000 - targetIndex)]
                                                  : &fmCellMods[(size_t) (-2 - targetIndex)];

            if (slotHeld[(size_t) s])
            {
                *target += slotAmounts[(size_t) s];
                continue;
            }

            const auto& slot = params.modSlots[s];
            auto value = Mod::shape (slot, sourceValue (slot.source, sampleIndex, ampValue, filterValue,
                                                        filter2Value, modValue, env4Value));

            if (slot.aux != Mod::Source::None)
                value *= Mod::auxScale (slot.aux, sourceValue (slot.aux, sampleIndex, ampValue, filterValue,
                                                                filter2Value, modValue, env4Value));

            *target += slot.depth * value;
        }

        return;
    }

    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];
        auto* target = Mod::isExplicitDestination (slot.destination) ? &mods[slot.destination] : nullptr;

        if (const auto stereoTarget = Mod::stereoTargetFor (slot); stereoTarget >= 0)
            target = &mods[stereoTarget];

        if (target == nullptr && params.anyExtendedFmMods)
            if (const auto cell = Mod::extendedFmCellFor (slot.destination); cell >= 0)
                target = &fmCellMods[(size_t) cell];

        if (target == nullptr && params.anySpectralMods && Mod::isVoiceSpectralSlot (slot))
            target = &spectralMods[(size_t) Mod::spectralOscFor (slot.destination)];

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

// What the per-sample loops read every sample (the rest, pitch included, is
// read once a sub-block by updateSubBlock); with filter FM on,
// updateFilterCoefficients reads the filters' settings every sample too.
bool Voice::isPerSampleDestination (int destination, bool filterFm)
{
    using D = Mod::Destination;
    if (destination >= (int) D::Count) // the stereo cutoffs follow the cutoffs
        return filterFm;
    switch ((D) destination)
    {
        case D::Filter1Cutoff: case D::Filter1Env: case D::Filter1Reso: case D::Filter1Morph:
        case D::Filter2Cutoff: case D::Filter2Env: case D::Filter2Reso: case D::Filter2Morph:
            return filterFm;
        case D::FmAmount: case D::FmFeedback: case D::RingMod:
        case D::Fm1to2: case D::Fm1to3: case D::Fm2to3: case D::Fm3to1:
        case D::Fm3to2: case D::Fm2Feedback: case D::Fm3Feedback:
        case D::Filter1Fm: case D::Filter2Fm:
        case D::Osc1Frame: case D::Osc2Frame: case D::SubFrame:
        case D::Osc4Frame: case D::Osc5Frame: case D::Osc6Frame:
        case D::AmpLevel: case D::Osc1Level: case D::Osc2Level: case D::SubLevel:
        case D::Osc4Level: case D::Osc5Level: case D::Osc6Level: case D::NoiseLevel:
            return true;
        default:
            return false;
    }
}

// One group's slots (prepareModSlots), in slot order: evaluateMods' sums
// for those destinations, to the bit.
void Voice::evaluateModGroup (int group, float* mods, int sampleIndex, float ampValue, float filterValue,
                              float filter2Value, float modValue, float env4Value) const
{
    const auto& destinations = groupDestinations[(size_t) group];
    for (int d = 0; d < numGroupDestinations[(size_t) group]; ++d)
        mods[destinations[(size_t) d]] = 0.0f;

    if (group == 1 && params.anyExtendedFmMods)
        fmCellMods.fill (0.0f);

    const auto& slots = groupSlots[(size_t) group];
    for (int k = 0; k < numGroupSlots[(size_t) group]; ++k)
    {
        const auto s = (size_t) slots[(size_t) k];
        const auto targetIndex = slotTargets[s];
        auto* target = targetIndex >= 0 ? &mods[targetIndex] : &fmCellMods[(size_t) (-2 - targetIndex)];

        if (slotHeld[s])
        {
            *target += slotAmounts[s];
            continue;
        }

        const auto& slot = params.modSlots[s];
        auto value = Mod::shape (slot, sourceValue (slot.source, sampleIndex, ampValue, filterValue,
                                                    filter2Value, modValue, env4Value));

        if (slot.aux != Mod::Source::None)
            value *= Mod::auxScale (slot.aux, sourceValue (slot.aux, sampleIndex, ampValue, filterValue,
                                                            filter2Value, modValue, env4Value));

        *target += slot.depth * value;
    }
}

void Voice::evaluateModsRated (float* mods, int sampleIndex, float ampValue, float filterValue,
                               float filter2Value, float modValue, float env4Value)
{
    if (! modSlotsPrepared)
    {
        evaluateMods (mods, sampleIndex, ampValue, filterValue, filter2Value, modValue, env4Value);
        return;
    }

    // Group 0, the routes read once a sub-block (updateSubBlock, at the same
    // samples) or only held sources: worked out only there, holding in
    // between, so the same values where they are read. Group 1, moving
    // sources into what is read every sample: every sample.
    if ((sampleIndex & (modControlInterval - 1)) == 0)
        evaluateModGroup (0, mods, sampleIndex, ampValue, filterValue, filter2Value, modValue, env4Value);
    if (numGroupSlots[1] > 0)
        evaluateModGroup (1, mods, sampleIndex, ampValue, filterValue, filter2Value, modValue, env4Value);
}

void Voice::prepareModSlots()
{
    stereoRouted = {};

    // Set only from MIDI and the block's parameters, never inside a render.
    const auto held = [] (Mod::Source source)
    {
        switch (source)
        {
            case Mod::Source::None:
            case Mod::Source::Velocity:
            case Mod::Source::KeyTrack:
            case Mod::Source::Random:
            case Mod::Source::ModWheel:
            case Mod::Source::Aftertouch:
            case Mod::Source::Expression:
            case Mod::Source::Macro1:
            case Mod::Source::Macro2:
            case Mod::Source::Macro3:
            case Mod::Source::Macro4:
            case Mod::Source::Macro5:
            case Mod::Source::Macro6:
            case Mod::Source::Macro7:
            case Mod::Source::Macro8:
            case Mod::Source::VectorX:
            case Mod::Source::VectorY:
                return true;
            default:
                return false;
        }
    };

    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];
        auto targetIndex = Mod::isExplicitDestination (slot.destination) ? slot.destination : -1;

        if (targetIndex == -1 && params.anyExtendedFmMods)
            if (const auto cell = Mod::extendedFmCellFor (slot.destination); cell >= 0)
                targetIndex = -2 - cell;

        if (targetIndex == -1 && params.anySpectralMods && Mod::isVoiceSpectralSlot (slot))
            targetIndex = -1000 - Mod::spectralOscFor (slot.destination);

        // A stereo route into a filter cutoff: summed apart from the cutoff itself.
        if (const auto stereoTarget = Mod::stereoTargetFor (slot); stereoTarget >= 0)
        {
            targetIndex = stereoTarget;
            stereoRouted[(size_t) (stereoTarget - (int) Mod::Destination::Count)] = true;
        }

        slotTargets[(size_t) s] = targetIndex;
        slotHeld[(size_t) s] = held (slot.source) && held (slot.aux);

        if (slotHeld[(size_t) s] && targetIndex != -1)
        {
            auto value = Mod::shape (slot, sourceValue (slot.source, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

            if (slot.aux != Mod::Source::None)
                value *= Mod::auxScale (slot.aux, sourceValue (slot.aux, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

            slotAmounts[(size_t) s] = slot.depth * value;
        }
    }

    // Each routed destination's group (see evaluateModsRated): per sample
    // when it is read every sample and a moving source feeds it; the OSC 4-6
    // FM cells always.
    std::array<bool, (size_t) Mod::Destination::Count + Mod::numStereoDestinations> moving {};
    auto filterFm = params.filter1Fm != 0.0f || params.filter2Fm != 0.0f;
    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto targetIndex = slotTargets[(size_t) s];
        if (targetIndex == (int) Mod::Destination::Filter1Fm || targetIndex == (int) Mod::Destination::Filter2Fm)
            filterFm = true;
        if (targetIndex >= 0 && ! slotHeld[(size_t) s])
            moving[(size_t) targetIndex] = true;
    }
    modFilterFm = filterFm;
    const auto groupOf = [&] (int targetIndex)
    {
        return (targetIndex < -1 && targetIndex > -1000) || (moving[(size_t) targetIndex] && isPerSampleDestination (targetIndex, filterFm)) ? 1 : 0;
    };

    numGroupSlots = {};
    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto targetIndex = slotTargets[(size_t) s];
        if (targetIndex == -1 || targetIndex <= -1000) // (spectral routes: read once a block, not in a group)
            continue;
        const auto group = (size_t) groupOf (targetIndex);
        groupSlots[group][(size_t) numGroupSlots[group]++] = s;
    }
    numGroupDestinations = {};
    for (int d = 0; d < params.numActiveDestinations; ++d)
    {
        const auto destination = params.activeDestinations[d];
        const auto group = (size_t) groupOf (destination);
        groupDestinations[group][(size_t) numGroupDestinations[group]++] = destination;
    }
    for (int f = 0; f < Mod::numStereoDestinations; ++f)
        if (stereoRouted[(size_t) f])
        {
            const auto destination = (int) Mod::Destination::Count + f;
            const auto group = (size_t) groupOf (destination);
            groupDestinations[group][(size_t) numGroupDestinations[group]++] = destination;
        }

    modSlotsPrepared = true;
}

void Voice::updateUnisonLayout()
{
    const auto layout = [] (const VoiceParams::OscParams& osc, int numUnison, int chord, float detuneCents,
                            float blend, double* offsets, double* ratios, float* gains)
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
        {
            gains[u] *= norm;
            ratios[u] = std::exp2 (offsets[u] / 12.0);
        }
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
                juce::jlimit (0.0f, 200.0f, settings.detuneCents + blockMod (detuneDestinations[osc]) * Mod::Range::detuneCents),
                settings.unisonBlend + blockMod (blendDestinations[osc]),
                unisonOffset[osc], unisonRatio[osc], unisonGains[osc]);
    }
}

