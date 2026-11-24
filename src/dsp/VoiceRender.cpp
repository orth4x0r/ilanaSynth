#include "Voice.h"
#include "FastMath.h"
#include "Generative.h"
#include "StretchRenderer.h"

#include <array>
#include <cmath>
#include "VoiceInternal.h"

void Voice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    renderStart = startSample;
    if (! hasActiveAmpEnvelope())
    {
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        clearCurrentNote();
        return;
    }

    // Held sleep: nothing this note plays can sound again until the key
    // (or pedal) comes up; then it carries on from where it stopped.
    if (heldAsleep)
    {
        if (noteHeld && ! monoPending && fadeTotal == 0 && heldSleepAllowed() && ! filterSettingsMoved()
            && enabledOscillators() == sleptOscillators)
            return;
        heldAsleep = false;
        heldSilentSamples = 0;
    }

    sounded = true;
    if (brokenForTest && numSamples > 0)
        outputBuffer.getWritePointer (0)[startSample] += std::numeric_limits<float>::quiet_NaN();
    retargetGlides (numSamples);

    // Step 33: the One Shot that held the release has played out.
    if (oneShotHold && ! oneShotPlaying())
        releaseEnvelopes();

    // Block-rate modulation first: everything below can use it. The
    // per-sample values only clear destinations that are routed, so wipe
    // them all here too: a routing removed since the last block must not
    // leave its final value stuck on the target.
    std::fill (blockMods.begin(), blockMods.end(), 0.0f);
    std::fill (sampleMods.begin(), sampleMods.end(), 0.0f);

    // (MIDI splits a render, so the held sources are fixed within this one.)
    prepareModSlots();

    if (params.numModSlots > 0)
    {
        for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
            if (params.lfos[lfo].perVoice)
                lfoValues[lfo] = voiceLfoValue (lfo);

        evaluateMods (blockMods.data(), 0, lastAmpValue, lastFilterValue, lastFilter2Value, lastModValue, lastEnv4Value);
    }

    // Per-note routes into the knobs: this voice's own values for the block.
    applyVoiceKnobs (false);

    // Per-note routes into the spectral amounts: each voice's own value for the block.
    spectralBlock = params.anySpectralMods && params.numModSlots > 0 ? spectralMods : std::array<float, 6> {};

    const auto scaleTime = [] (float seconds, float mod, float octaves)
    {
        const auto clamp = Mod::Range::timeClampOctaves;
        return mod != 0.0f ? seconds * std::exp2 (juce::jlimit (-clamp, clamp, mod * octaves)) : seconds;
    };

    const auto modulatedEnvelope = [&scaleTime] (TensionAdsr::Parameters env, float a, float d, float s, float r)
    {
        env.attack = scaleTime (env.attack, a, Mod::Range::attackOctaves);
        env.decay = scaleTime (env.decay, d, Mod::Range::decayOctaves);
        env.sustain = juce::jlimit (0.0f, 1.0f, env.sustain + s);
        env.release = scaleTime (env.release, r, Mod::Range::releaseOctaves);
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
    numNeededExtraEnvs = 0;
    for (int env = 0; env < (int) extraEnvs.size(); ++env)
        if (params.extraEnvNeeded[(size_t) env])
        {
            extraEnvs[(size_t) env].setParameters (keyScaled (params.extraEnvs[(size_t) env], env + 5));
            neededExtraEnvs[numNeededExtraEnvs++] = env;
        }

    numPerVoiceLfos = 0;
    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
        if (params.lfos[lfo].perVoice)
            perVoiceLfos[numPerVoiceLfos++] = lfo;

    if (params.msegEnvNeeded)
        envMseg.setParams (params.msegShape.levels, params.msegShape.times,
                           params.msegShape.rateHz, params.msegShape.loop);

    for (int lfo = 0; lfo < VoiceParams::numLfos; ++lfo)
    {
        // At most 0.45 cycles a sample (as key tracking caps it): routes into
        // RATE sum without a limit, and at 2^36 the random and friction
        // shapes' catch-up loops never finished a block.
        lfoIncrements[lfo] = juce::jmin (0.45, params.lfos[lfo].baseIncrement
                                                   * std::exp2 ((double) blockMod (Mod::lfoRateDestinationFor (lfo)) * (double) Mod::Range::lfoRateOctaves));

        // Key tracked: RATE 4 Hz runs at the note's own pitch, 8 Hz an
        // octave above, 2 Hz an octave below.
        if (params.lfos[lfo].keyTrack)
            lfoIncrements[lfo] = juce::jmin (0.45, lfoIncrements[lfo] * currentFrequency / 4.0);

        // M8.1: SMOOTH, and the triggers counted by the processor.
        const auto& lfoSettings = params.lfos[lfo];
        lfoSmoothCoefficients[lfo] = LfoSmoother::coefficientFor (lfoSettings.smooth, lfoIncrements[lfo] * sampleRate, sampleRate);
        if (lfoSettings.triggerCount != lfoSeenTriggers[lfo])
        {
            lfoSeenTriggers[lfo] = lfoSettings.triggerCount;
            if (lfoSettings.perVoice && LfoSimShapes::isSim (lfoSettings.shape))
                lfoSims[lfo].trigger (lfoSettings.sim, ++lfoSimNotes * 7919u + (std::uint32_t) lfo);
        }
        lfoSims[lfo].sampleRate = sampleRate;
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

    if (params.west.on)
    {
        westFolder.setParams (params.west.fold, params.west.symmetry, params.west.stages);
    }

    // GLIDE is the one-pole's time constant in seconds. It steps once a
    // sub-block (16 samples), so each step takes the 16-sample coefficient.
    // (Until 2026-11 it took the one-sample coefficient, which made every
    // glide 16 times slower than the knob said; old patches and the factory
    // recipes load with GLIDE x16, migrateGlideTime, and glide as they did.)
    glideSamples = params.glideTime > 0.001f ? (float) (params.glideTime * sampleRate) : 0.0f;
    glideCoeff = glideSamples > 0.0f ? 1.0f - std::exp (-16.0f / glideSamples) : 1.0f;

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
            && (settings.stringExcite == (int) KarplusStrong::Excite::Hammer || settings.stringExcite == (int) KarplusStrong::Excite::Piano)
            && numOscUnison[osc] > 1)
        {
            const auto note = getCurrentlyPlayingNote();
            numOscUnison[osc] = juce::jmin (numOscUnison[osc], note < 35 ? 1 : (note < 47 ? 2 : 3));
        }
        // (The Modal Piano plays unison too, since 2026-11-03: ilana, "modal
        // piano unison dont work". Each voice is a whole recorded note, so
        // UNISON stacks detuned pianos, as on a wavetable.)

        // A stereo string route on one string: a second plays its right side.
        const auto twin = settings.stringMode && settings.stringStereo && numOscUnison[osc] == 1;
        if (twin && ! stringTwin[osc] && settings.stringMode == lastStringMode[osc])
        {
            // (A route added mid-note: the twin picks up where the string is.)
            stringFor (osc, 1) = stringFor (osc, 0);
        }
        if (! twin)
            twinAsleep[osc] = false;
        stringTwin[osc] = twin;
    }

    updateUnisonLayout();

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillatorEnabled[osc] || oscEnableSmooth[osc].getCurrentValue() > 0.0005f)
        {
            const auto& settings = params.oscillators[osc];
            const auto* table = settings.table;

            // A per-note route into SPEC AMT: this voice's own amount picks the table.
            if (params.spectralCache != nullptr && spectralBlock[(size_t) osc] != 0.0f && settings.spectralMode > 0)
                table = params.spectralCache->getForVoice (settings.spectralTable, settings.spectralMode,
                                                           juce::jlimit (0.0f, 1.0f, settings.spectralAmount + spectralBlock[(size_t) osc]),
                                                           settings.baseTable, table);

            // A warped copy fades from the last one (UnisonBank::setWavetable).
            oscBank[osc].setWavetable (table, table != settings.baseTable);
        }
        else if (oscBank[osc].getWavetable() != nullptr)
        {
            // Not rendering: a spectral copy this bank holds (or fades from)
            // can be freed before it plays again (SpectralCache keeps a
            // replaced copy 3 s), and its pitch updates still read the table.
            // It lets go, and takes its next table as it is, without a fade.
            oscBank[osc].setWavetable (nullptr);
        }

    const auto configureStrings = [this] (int osc)
    {
        const auto& settings = params.oscillators[osc];

        const auto count = stringsPlaying (osc);
        if (twinAsleep[osc] && stringTwin[osc] && ! stereoStringAtRest (osc))
        {
            // The route moves: the twin wakes where string 0 is.
            stringFor (osc, 1) = stringFor (osc, 0);
            twinAsleep[osc] = false;
        }
        for (int u = 0; u < (stringTwin[osc] && twinAsleep[osc] ? 1 : count); ++u)
        {
            configureString (stringFor (osc, u), settings, stringSide (osc, u, count));
        }
    };

    // Keep OSC 3's original transition order until the behaviour-preserving
    // checkpoint; its string transition is recorded before sample sync.
    configureStrings (2);

    // A string switched to, or switched on mid-note, is plucked.
    if (params.oscillators[2].stringMode && (! lastStringMode[2] || ! lastEnabled[2]) && params.oscillatorEnabled[2])
        for (int u = 0; u < stringsPlaying (2); ++u)
            stringFor (2, u).trigger (velocityLevel);

    lastStringMode[2] = params.oscillators[2].stringMode;

    // The pans, from the block-rate routes into them eased in (BlockGlide).
    {
        constexpr D panDestinations[] { D::Osc1Pan, D::Osc2Pan, D::SubPan,
                                        D::Osc4Pan, D::Osc5Pan, D::Osc6Pan };
        constexpr D spreadDestinations[] { D::Osc1Spread, D::Osc2Spread, D::SubSpread,
                                           D::Osc4Spread, D::Osc5Spread, D::Osc6Spread };
        const auto minGlide = (int) (0.0005 * sampleRate), maxGlide = (int) (0.025 * sampleRate);
        const auto target = [&] (BlockGlide& glide, float value)
        {
            if (glidesSnap)
                glide.snap (value);
            else
                glide.retarget (value, glideClock, minGlide, maxGlide);
        };
        target (panModGlide, juce::jlimit (-1.0f, 1.0f, blockMod (D::Pan)));
        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        {
            target (oscPanModGlide[(size_t) osc], blockMod (panDestinations[osc]) * Mod::Range::oscPan);
            target (spreadModGlide[(size_t) osc], blockMod (spreadDestinations[osc]));
        }
        target (driveModGlide[0], blockMod (D::Filter1Drive));
        target (driveModGlide[1], blockMod (D::Filter2Drive));
    }
    updatePans();

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        frameSmooth[osc].setTargetValue (params.oscillators[osc].frame);
        levelSmooth[osc].setTargetValue (params.oscillators[osc].level);
        oscEnableSmooth[osc].setTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
    }

    noiseSmooth.setTargetValue (params.noiseLevel);
    noiseEnableSmooth.setTargetValue (params.noiseEnabled ? 1.0f : 0.0f);
    subOscColourSmooth.setTargetValue (params.subOscColour);
    subOscLevelSmooth.setTargetValue (params.subOscLevel);
    subOscEnableSmooth.setTargetValue (params.subOscEnabled ? 1.0f : 0.0f);
    subOsc.setWavetable (params.subOscTable);

    bool modeChanged[VoiceParams::numOscillators] {}, enabledBefore[VoiceParams::numOscillators] {};
    std::copy (std::begin (lastEnabled), std::end (lastEnabled), std::begin (enabledBefore));
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
        {
            retriggerNewPlayers (sampleUnison[osc], lastSampleMode[osc], lastEnabled[osc],
                                 lastUnison[osc], numOscUnison[osc]);
            retriggerNewPlayers (sampleLayer[osc], lastSampleMode[osc], lastEnabled[osc],
                                 lastUnison[osc], numOscUnison[osc]);
        }

        lastSampleMode[osc] = settings.sampleMode;
        lastEnabled[osc] = params.oscillatorEnabled[osc];
        lastUnison[osc] = numOscUnison[osc];
    }

    for (int osc = 0; osc < 2; ++osc)
        configureStrings (osc);

    const auto switchedOn = [&] (int osc) { return ! enabledBefore[osc] && params.oscillatorEnabled[osc]; };
    if (params.oscillators[0].stringMode && (! lastStringMode[0] || switchedOn (0)))
        for (int u = 0; u < stringsPlaying (0); ++u)
            stringFor (0, u).trigger (velocityLevel);

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (modeChanged[osc])
        {
            oscEnableSmooth[osc].setCurrentAndTargetValue (0.0f);
            oscEnableSmooth[osc].setTargetValue (params.oscillatorEnabled[osc] ? 1.0f : 0.0f);
        }

    if (params.oscillators[1].stringMode && (! lastStringMode[1] || switchedOn (1)))
        for (int u = 0; u < stringsPlaying (1); ++u)
            stringFor (1, u).trigger (velocityLevel);

    for (int osc = 0; osc < 2; ++osc)
        lastStringMode[osc] = params.oscillators[osc].stringMode;

    for (int osc = 3; osc < VoiceParams::numOscillators; ++osc)
    {
        if (! params.oscillatorEnabled[osc])
            continue;

        configureStrings (osc);
        if (params.oscillators[osc].stringMode && (! lastStringMode[osc] || switchedOn (osc)))
            for (int u = 0; u < stringsPlaying (osc); ++u)
                stringFor (osc, u).trigger (velocityLevel);
        lastStringMode[osc] = params.oscillators[osc].stringMode;
    }

    const auto resonatorAmount = juce::jlimit (0.0f, 1.0f, params.resonatorAmount + blockMod (D::ResAmount));
    const auto resonatorDecay = juce::jlimit (0.0f, 1.0f, params.resonatorDecay + blockMod (D::ResDecay));

    resonatorL.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);
    resonatorR.setParams (params.resonatorOn ? resonatorAmount : 0.0f, resonatorDecay, 0.35f);

    applyDrives();
    const auto& drive1 = drive1Now; // (eased each sub-block while they move: advanceGlides)
    const auto& drive2 = drive2Now;

    glidesSnap = false;
    pansMoving = panModGlide.moving() || paramGlides[glideVoiceSpread].moving();
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        pansMoving = pansMoving || oscPanModGlide[(size_t) osc].moving() || spreadModGlide[(size_t) osc].moving()
                     || paramGlides[(size_t) (glideOscPan + osc)].moving() || paramGlides[(size_t) (glideOscSpread + osc)].moving();
    drivesMoving = driveModGlide[0].moving() || driveModGlide[1].moving()
                   || paramGlides[glideDrive1].moving() || paramGlides[glideDrive2].moving();
    glidesMoving = pansMoving || drivesMoving;
    for (int k = 0; k < numParamGlides && ! glidesMoving; ++k)
        glidesMoving = paramGlides[(size_t) k].moving();

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    const auto ampVelScale = 1.0f - params.ampVelocity + params.ampVelocity * velocityLevel;
    // The Operator EG supplies its operators' gains, so the amp envelope steps aside.
    bool alternateAmpRouting = dx7Playing;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        alternateAmpRouting = alternateAmpRouting
                              || (params.oscillatorEnabled[osc] && params.oscillators[osc].ampEnv != 0);

    // Only the modal body's tail left (the amp envelope is over, nothing
    // else rings it): render the body alone. Not with per-oscillator
    // envelopes or the Operator EG (their sources may still drive it) or a
    // live input ringing it.
    if (legacyMaterialBody() && bodyTailSamplesRemaining > 0
        && ! alternateAmpRouting && ! operatorEgOwnsVoice() && ! envelopesActive()
        && (params.liveInput == nullptr || params.inputToBody == 0.0f))
    {
        if (resonatorAmount > 0.001f)
            renderBodyTail (left, right, startSample, numSamples,
                            resonatorAmount * ampVelScale * juce::jlimit (0.0f, 2.0f, 1.0f + blockMod (D::AmpLevel)));
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        bodyTailSamplesRemaining = juce::jmax (0, bodyTailSamplesRemaining - numSamples);
        if (fadeTotal > 0 && fadeRemaining == 0)
            stopNote (0.0f, false);
        if (! hasActiveAmpEnvelope())
            clearCurrentNote();
        return;
    }

    auto* mods = sampleMods.data();

    constexpr D frameDestinations[] { D::Osc1Frame, D::Osc2Frame, D::SubFrame,
                                      D::Osc4Frame, D::Osc5Frame, D::Osc6Frame };
    constexpr D levelDestinations[] { D::Osc1Level, D::Osc2Level, D::SubLevel,
                                      D::Osc4Level, D::Osc5Level, D::Osc6Level };
    constexpr D warpDestinations[] { D::Osc1Warp, D::Osc2Warp, D::SubWarp,
                                     D::Osc4Warp, D::Osc5Warp, D::Osc6Warp };
    // The bus each source adds into: its _route (the fixed chain's buses),
    // or with the ROUTING cables on the bus of its cables (one per distinct
    // set of destinations, VoiceRouting.h).
    const auto& routing = params.routing;
    const auto bodyInChain = bodyIsInChain();
    const auto numBuses = routing.on ? routing.numBuses : (int) FilterRoute::Count;
    int routes[VoiceParams::numOscillators] {};
    bool active[VoiceParams::numOscillators] {};

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[osc];
        routes[osc] = routing.on ? routing.busOf[(size_t) osc] : juce::jlimit (0, FilterRoute::Count - 1, settings.route);
        active[osc] = params.oscillatorEnabled[osc]
                      && (settings.stringMode || settings.sampleMode || settings.table != nullptr);
    }

    const auto routeSubOsc = routing.on ? routing.busOf[(size_t) VoiceRouting::subNoiseSource]
                                        : juce::jlimit (0, FilterRoute::Count - 1, params.subOscRoute);
    bool bothActive = ! routing.on && routeSubOsc == FilterRoute::Both;
    for (int osc = 0; osc < VoiceParams::numOscillators && ! routing.on; ++osc)
        bothActive = bothActive || (active[osc] && routes[osc] == FilterRoute::Both);
    bothRouteActive = bothActive;
    const auto subOscActive = params.subOscEnabled && params.subOscTable != nullptr;
    const auto subOscFrames = WavetableOscillator::frameReadFor (params.subOscTable, 0.0f);

    // M5 operator extras. Each is skipped while unused, so older patches
    // render exactly as before.
    auto anyAltFeedback = false;
    auto anyNoiseOperator = false;
    auto anyWarpNoise = false; // an oscillator's WARP is FM Noise
    // The FM cells that involve OSC 4-6. A zero cell adds nothing, so unless
    // the extended cells are modulated only the non-zero ones are summed
    // (in the same order as the full matrix).
    int extendedFmCells[27][2];
    auto numExtendedFmCells = 0;

    for (int target = 0; target < VoiceParams::numOscillators; ++target)
        for (int source = target < 3 ? 3 : 0; source < VoiceParams::numOscillators; ++source)
            if (params.anyExtendedFmMods || params.fmMatrix[source][target] != 0.0f)
            {
                extendedFmCells[numExtendedFmCells][0] = source;
                extendedFmCells[numExtendedFmCells][1] = target;
                ++numExtendedFmCells;
            }

    // The oscillators whose feedback history the per-sample loop keeps
    // (DX7 and filtered feedback types), and those whose string drives a
    // body (the others' drive is zero).
    int historyOscs[VoiceParams::numOscillators] {}, numHistoryOscs = 0;
    int drivingOscs[VoiceParams::numOscillators] {}, numDrivingOscs = 0;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto type = params.oscillators[osc].feedbackType;
        if (type == FmFeedback::Dx7 || type == FmFeedback::Filtered)
            historyOscs[numHistoryOscs++] = osc;
        if (params.oscillators[osc].stringMode && ! params.oscillators[osc].sampleMode)
            drivingOscs[numDrivingOscs++] = osc;
    }

    // OSC 1-3 that are off with nothing still fading: the per-sample loop
    // skips them (their smoothers would return the same values).
    bool idleOsc[3] {};
    for (int osc = 0; osc < 3; ++osc)
        idleOsc[osc] = ! active[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f
                       && ! oscEnableSmooth[osc].isSmoothing() && ! frameSmooth[osc].isSmoothing()
                       && ! levelSmooth[osc].isSmoothing();

    // The per-sample loop's oscillators: OSC 1-3 that are idle and OSC 4-6
    // that are off and silent at the block's start are left out (nothing in
    // the loop would move them; their outputs are set after it).
    int visitOscs[VoiceParams::numOscillators] {}, numVisitOscs = 0;
    int skippedOscs[VoiceParams::numOscillators] {}, numSkippedOscs = 0;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto skip = osc < 3 ? idleOsc[osc]
                                  : ! active[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f;
        if (skip)
            skippedOscs[numSkippedOscs++] = osc;
        else
            visitOscs[numVisitOscs++] = osc;
    }
    const auto skipsOsc1 = idleOsc[1];
    // An oscillator skipped all block whose feedback state is at rest (zero)
    // stays there: its history update would leave it unchanged.
    {
        auto kept = 0;
        for (int k = 0; k < numHistoryOscs; ++k)
        {
            const auto osc = historyOscs[k];
            auto resting = feedbackHistory[osc] == 0.0f && ! std::signbit (feedbackHistory[osc])
                           && feedbackFiltered[osc] == 0.0f && ! std::signbit (feedbackFiltered[osc]);
            auto skippedAll = false;
            for (int v = 0; v < numSkippedOscs; ++v)
                skippedAll = skippedAll || skippedOscs[v] == osc;
            if (! (resting && skippedAll))
                historyOscs[kept++] = osc;
        }
        numHistoryOscs = kept;
    }

    // An operator whose FM input cells are all zero and unmodulated this
    // block gets nothing from its feedback type (every term is zero), so its
    // per-sample sum is skipped; its feedback filter still tracks.
    bool altFeedbackInput[VoiceParams::numOscillators] {};
    int altSources[VoiceParams::numOscillators][VoiceParams::numOscillators];
    int numAltSources[VoiceParams::numOscillators] {};
    auto anyAltFeedbackInput = false;
    const auto cellIsSilent = [this, mods] (int source, int target)
    {
        // (OSC 1 hears its own feedback and OSC 2 through the legacy
        // amounts, which the processor mirrors into the matrix.)
        const auto amount = target == 0 && source == 0 ? params.fmFeedback
                          : target == 0 && source == 1 ? params.fmAmount
                                                       : params.fmMatrix[source][target];
        if (amount != 0.0f || params.fmMatrix[source][target] != 0.0f)
            return false;
        // (FM AMOUNT or FEEDBACK easing in from zero: BlockGlide.)
        if (target == 0 && source <= 1 && paramGlides[(size_t) (source == 0 ? glideFmFeedback : glideFmAmount)].moving())
            return false;

        if (source >= 3 || target >= 3)
            return ! params.anyExtendedFmMods;

        static constexpr D legacy[3][3] { { D::FmFeedback, D::Fm1to2, D::Fm1to3 },
                                          { D::FmAmount, D::Fm2Feedback, D::Fm2to3 },
                                          { D::Fm3to1, D::Fm3to2, D::Fm3Feedback } };
        const auto destination = (int) legacy[source][target];
        for (int d = 0; d < params.numActiveDestinations; ++d)
            if (params.activeDestinations[d] == destination)
                return false;
        return mods[destination] == 0.0f;
    };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        anyAltFeedback = anyAltFeedback || (active[osc] && params.oscillators[osc].feedbackType != FmFeedback::Plain);
        anyNoiseOperator = anyNoiseOperator || (active[osc] && params.fmNoise[osc] > 0.0f);
        anyWarpNoise = anyWarpNoise || (active[osc] && (params.oscillators[osc].warpMode == Warp::NoiseFm
                                                        || params.oscillators[osc].warpMode2 == Warp::NoiseFm));

        // The other operators that feed it: a silent cell adds an exact zero,
        // so leaving it out of the sum changes nothing.
        if (active[osc] && params.oscillators[osc].feedbackType != FmFeedback::Plain)
            for (int source = 0; source < VoiceParams::numOscillators; ++source)
                if (! cellIsSilent (source, osc))
                {
                    altFeedbackInput[osc] = true;
                    if (source != osc)
                        altSources[osc][numAltSources[osc]++] = source;
                }
        anyAltFeedbackInput = anyAltFeedbackInput || altFeedbackInput[osc];
    }
    // The heard noise's colour, on the FM noise's scale (about 200 Hz to
    // white); 1 leaves it white.
    auto heardNoiseCoeff = 1.0f, heardNoiseGain = 1.0f;
    if (params.noiseColour < 0.999f)
    {
        const auto cutoff = juce::jmin (0.45 * sampleRate, 200.0 * std::pow (100.0, (double) juce::jlimit (0.0f, 1.0f, params.noiseColour)));
        heardNoiseCoeff = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate));
        heardNoiseGain = std::pow (heardNoiseCoeff / (2.0f - heardNoiseCoeff), -0.25f);
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

    // Each 16-sample chunk (the sub-block the coefficients follow) runs in
    // two passes: the sources (envelopes, modulation, oscillators, FM,
    // strings) for every sample, then the post chain (drive, filters, west
    // coast, body, amp) for every sample. Nothing in the post chain feeds the
    // sources within a chunk, so the result is the same as one pass per
    // sample, and the post chain runs as tight loops (Vital works in blocks
    // the same way). Two cases do feed back, and run one sample per chunk:
    // the body ringing the strings (coupling mode 2) and audio-rate filter FM
    // (the coefficients move every sample).
    constexpr int maxChunk = 16;
    const auto sampleFeedback = (params.resonatorOn && params.bodyType != 0 && params.bodyCouplingMode == 2
                                 && params.bodyCoupling > 0.0f)
                                || params.filter1Fm != 0.0f || params.filter2Fm != 0.0f
                                || [this]
                                   {
                                       for (int s = 0; s < params.numModSlots; ++s)
                                           if (params.modSlots[s].isActive()
                                               && (params.modSlots[s].destination == (int) D::Filter1Fm
                                                   || params.modSlots[s].destination == (int) D::Filter2Fm))
                                               return true;
                                       return false;
                                   }();
    float chunkBusL[maxChunk][VoiceRouting::maxBuses], chunkBusR[maxChunk][VoiceRouting::maxBuses];
    float chunkBodyL[maxChunk], chunkBodyR[maxChunk], chunkLive[maxChunk], chunkAmp[maxChunk];
    float chunkAmpLevel[maxChunk], chunkWest[maxChunk];

    // The post chain for one chunk, stage by stage: every stage only reads
    // the stage before it at the same sample, so running each over the chunk
    // in turn gives the per-sample result, and the filters run as blocks.
    const auto drive = [] (float value, float amount) { return amount > 1.0f ? FastMath::tanh (value * amount) : value; };

    // A mono source feeds both sides alike: drive it once (the same result).
    const auto drivePair = [&drive] (float l, float r, DriveAmounts amount, float& outL, float& outR)
    {
        outL = drive (l, amount.l);
        outR = r == l && amount.r == amount.l ? outL : drive (r, amount.r);
    };

    // M8.3: the west-coast voice's control (the LED's drive): a strike on
    // each note, or any mod source, on top of OPEN.
    const auto westProcess = [&] (float& l, float& r, float westSource)
    {
        const auto& w = params.west;
        auto control = w.open;
        if (w.source == 0)
        {
            if (westStrikeRemaining > 0)
            {
                control += 2.0f * w.strike * velocityLevel; // the LED overdriven
                --westStrikeRemaining;
            }
        }
        else
        {
            control += 2.0f * w.strike * westSource;
        }
        const auto gateMode = (LowPassGate::Mode) juce::jlimit (0, 2, w.mode);
        westGateL.setParams (gateMode, w.decay, w.resonance);
        westGateR.setParams (gateMode, w.decay, w.resonance);
        westFolder.process (l, r);
        westGateL.processPair (l, r, control, westGateR);
    };
    const auto westReplacesFilter2 = params.west.on && params.west.position == 1;

    // Roadmap step 16: the ROUTING cables. Each used stage runs in the
    // compiled order (VoiceRouting.h); its input is the sum of the buses and
    // stages cabled into it (Filter 1 and 2 behind their DRIVE, as in the
    // fixed chain), and its output goes to each stage or OUT it is cabled to.
    // WEST switched off passes its input on. Two filters into one place mix
    // as the fixed chain's PARALLEL does (BALANCE, 0.7071).
    const auto runCables = [&] (int n, float* outL, float* outR)
    {
        constexpr int numNodes = VoiceRouting::numStages + 1; // the stages, then OUT
        float nodeL[numNodes][maxChunk], nodeR[numNodes][maxChunk];
        for (int node = 0; node < numNodes; ++node)
        {
            std::fill (nodeL[node], nodeL[node] + n, 0.0f);
            std::fill (nodeR[node], nodeR[node] + n, 0.0f);
        }

        for (int bus = 0; bus < routing.numBuses; ++bus)
            for (int node = 0; node < numNodes; ++node)
                if ((routing.busMask[(size_t) bus] & VoiceRouting::bitOf (node)) != 0)
                    for (int s = 0; s < n; ++s)
                    {
                        nodeL[node][s] += chunkBusL[s][bus];
                        nodeR[node][s] += chunkBusR[s][bus];
                    }

        const auto gain1 = juce::jmin (1.0f, 1.0f - params.filterBalance) * 0.7071f;
        const auto gain2 = juce::jmin (1.0f, 1.0f + params.filterBalance) * 0.7071f;

        for (int k = 0; k < routing.numOrdered; ++k)
        {
            const auto stage = routing.order[(size_t) k];
            float inL[maxChunk], inR[maxChunk], stageL[maxChunk], stageR[maxChunk];

            if (stage == VoiceRouting::f1)
            {
                for (int s = 0; s < n; ++s)
                    drivePair (nodeL[stage][s], nodeR[stage][s], drive1, inL[s], inR[s]);
                if (filter1Open)
                    processOpenPairBlock (openFilter1L, openFilter1R, filter1Linked, inL, inR, stageL, stageR, n);
                else
                    processFilterPairBlock (filter1L, filter1R, filter1Linked, inL, inR, stageL, stageR, n);
                filter1Ran = true;

                if (filter1Fade > 0)
                {
                    float oldL[maxChunk], oldR[maxChunk];
                    if (filter1Open)
                        processFilterPairBlock (filter1L, filter1R, filter1FadeLinked, inL, inR, oldL, oldR, n);
                    else
                        processOpenPairBlock (openFilter1L, openFilter1R, filter1FadeLinked, inL, inR, oldL, oldR, n);
                    for (int s = 0; s < n && filter1Fade > 0; ++s, --filter1Fade)
                    {
                        const auto g = (float) filter1Fade / (float) filterFadeLength;
                        stageL[s] += (oldL[s] - stageL[s]) * g;
                        stageR[s] += (oldR[s] - stageR[s]) * g;
                    }
                }
            }
            else if (stage == VoiceRouting::f2)
            {
                for (int s = 0; s < n; ++s)
                    drivePair (nodeL[stage][s], nodeR[stage][s], drive2, inL[s], inR[s]);
                processFilter2Block (inL, inR, stageL, stageR, n);
            }
            else if (stage == VoiceRouting::west)
            {
                for (int s = 0; s < n; ++s)
                {
                    stageL[s] = nodeL[stage][s];
                    stageR[s] = nodeR[stage][s];
                    if (params.west.on)
                        westProcess (stageL[s], stageR[s], chunkWest[s]);
                }
            }
            else
            {
                // BODY: the Classic resonator rings on what comes in; a material
                // body is struck as ever and also rung by it (a share of the
                // audio), then mixed with the dry as BODY AMOUNT says. Off, it
                // passes the signal on.
                for (int s = 0; s < n; ++s)
                {
                    auto l = nodeL[stage][s];
                    auto r = nodeR[stage][s];
                    if (params.resonatorOn && resonatorAmount > 0.001f)
                    {
                        if (params.bodyType == 0)
                        {
                            const auto excite = chunkLive[s] * params.inputToBody;
                            l = resonatorL.process (l, excite);
                            r = resonatorR.process (r, excite);
                        }
                        else
                        {
                            const auto exciteEnvelope = alternateAmpRouting ? 1.0f : chunkAmp[s];
                            const auto wetL = materialBodyL.process (chunkBodyL[s] * exciteEnvelope + l * bodyAudioShare);
                            const auto wetR = materialBodyR.process (chunkBodyR[s] * exciteEnvelope + r * bodyAudioShare);
                            if (params.bodyCouplingMode == 2 && params.bodyCoupling > 0.0f)
                                for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                                    if (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode)
                                        for (int u = 0; u < stringsPlaying (osc); ++u)
                                            stringFor (osc, u).addBridgeInput (0.01f * params.bodyCoupling * (wetL + wetR));
                            l = l * (1.0f - resonatorAmount) + wetL * resonatorAmount;
                            r = r * (1.0f - resonatorAmount) + wetR * resonatorAmount;
                        }
                    }
                    stageL[s] = l;
                    stageR[s] = r;
                }
            }

            const auto destinations = routing.stageOut[(size_t) stage];
            const auto paired = stage == VoiceRouting::f1 || stage == VoiceRouting::f2 ? routing.filterPairInto : 0;
            for (int node = 0; node < numNodes; ++node)
            {
                if ((destinations & VoiceRouting::bitOf (node)) == 0)
                    continue;
                const auto gain = (paired & VoiceRouting::bitOf (node)) != 0 ? (stage == VoiceRouting::f1 ? gain1 : gain2) : 1.0f;
                for (int s = 0; s < n; ++s)
                {
                    nodeL[node][s] += stageL[s] * gain;
                    nodeR[node][s] += stageR[s] * gain;
                }
            }
        }

        std::copy (nodeL[VoiceRouting::out], nodeL[VoiceRouting::out] + n, outL);
        std::copy (nodeR[VoiceRouting::out], nodeR[VoiceRouting::out] + n, outR);
    };

    const auto postChunk = [&] (int chunkStart, int n)
    {
        float defaultL[maxChunk], defaultR[maxChunk], inL[maxChunk], inR[maxChunk];
        float f1L[maxChunk], f1R[maxChunk], in2L[maxChunk], in2R[maxChunk], outL[maxChunk], outR[maxChunk];

        if (routing.on)
        {
            runCables (n, outL, outR);
        }
        else
        {
            // Filter 1 hears the Default and Filter-1 buses.
            for (int s = 0; s < n; ++s)
            {
                const auto* busL = chunkBusL[s];
                const auto* busR = chunkBusR[s];
                drivePair (busL[FilterRoute::Default], busR[FilterRoute::Default], drive1, defaultL[s], defaultR[s]);
                inL[s] = defaultL[s];
                inR[s] = defaultR[s];
                if (busL[FilterRoute::Filter1] != 0.0f || busR[FilterRoute::Filter1] != 0.0f)
                    drivePair (busL[FilterRoute::Default] + busL[FilterRoute::Filter1],
                               busR[FilterRoute::Default] + busR[FilterRoute::Filter1], drive1, inL[s], inR[s]);
            }

            if (filter1Open)
                processOpenPairBlock (openFilter1L, openFilter1R, filter1Linked, inL, inR, f1L, f1R, n);
            else
                processFilterPairBlock (filter1L, filter1R, filter1Linked, inL, inR, f1L, f1R, n);
            filter1Ran = true;

            if (filter1Fade > 0)
            {
                float oldL[maxChunk], oldR[maxChunk];
                if (filter1Open)
                    processFilterPairBlock (filter1L, filter1R, filter1FadeLinked, inL, inR, oldL, oldR, n);
                else
                    processOpenPairBlock (openFilter1L, openFilter1R, filter1FadeLinked, inL, inR, oldL, oldR, n);
                for (int s = 0; s < n && filter1Fade > 0; ++s, --filter1Fade)
                {
                    const auto g = (float) filter1Fade / (float) filterFadeLength;
                    f1L[s] += (oldL[s] - f1L[s]) * g;
                    f1R[s] += (oldR[s] - f1R[s]) * g;
                }
            }

            if (params.filtersParallel)
            {
                // Filter 2 hears the (Filter-1-driven) Default bus plus its own.
                for (int s = 0; s < n; ++s)
                    drivePair (defaultL[s] + chunkBusL[s][FilterRoute::Filter2], defaultR[s] + chunkBusR[s][FilterRoute::Filter2],
                               drive2, in2L[s], in2R[s]);

                float f2L[maxChunk], f2R[maxChunk];
                if (westReplacesFilter2)
                {
                    for (int s = 0; s < n; ++s)
                    {
                        f2L[s] = in2L[s];
                        f2R[s] = in2R[s];
                        westProcess (f2L[s], f2R[s], chunkWest[s]);
                    }
                }
                else
                {
                    processFilter2Block (in2L, in2R, f2L, f2R, n);
                }

                // Balance fades one filter out; at the centre both are at full.
                const auto gain1 = juce::jmin (1.0f, 1.0f - params.filterBalance);
                const auto gain2 = juce::jmin (1.0f, 1.0f + params.filterBalance);
                for (int s = 0; s < n; ++s)
                {
                    outL[s] = (f1L[s] * gain1 + f2L[s] * gain2) * 0.7071f;
                    outR[s] = (f1R[s] * gain1 + f2R[s] * gain2) * 0.7071f;
                }
            }
            else
            {
                for (int s = 0; s < n; ++s)
                    drivePair (f1L[s] + chunkBusL[s][FilterRoute::Filter2], f1R[s] + chunkBusR[s][FilterRoute::Filter2],
                               drive2, in2L[s], in2R[s]);

                if (westReplacesFilter2)
                {
                    for (int s = 0; s < n; ++s)
                    {
                        outL[s] = in2L[s];
                        outR[s] = in2R[s];
                        westProcess (outL[s], outR[s], chunkWest[s]);
                    }
                }
                else
                {
                    processFilter2Block (in2L, in2R, outL, outR, n);
                }
            }
        }

        for (int s = 0; s < n; ++s)
        {
            const auto i = chunkStart + s;
            const auto* busL = chunkBusL[s];
            const auto* busR = chunkBusR[s];
            auto sampleL = outL[s];
            auto sampleR = outR[s];

            // (With the cables the bypass and WEST are cables, run above.)
            if (! routing.on)
            {
                sampleL += busL[FilterRoute::Direct];
                sampleR += busR[FilterRoute::Direct];
                if (params.west.on && params.west.position == 0)
                    westProcess (sampleL, sampleR, chunkWest[s]);
            }

            // A separate parallel pair keeps the existing serial/parallel paths
            // bit-identical whenever no oscillator selects Both.
            if (bothActive)
            {
                const auto both1L = bothFilter1L.process (drive (busL[FilterRoute::Both], drive1.l));
                const auto both1R = bothFilter1R.process (drive (busR[FilterRoute::Both], drive1.r));
                auto both2L = bothFilter2L.process (drive (busL[FilterRoute::Both], drive2.l));
                auto both2R = bothFilter2R.process (drive (busR[FilterRoute::Both], drive2.r));
                if (params.filter2Off) // Filter 2 off: its half of Both passes the bus
                {
                    both2L = busL[FilterRoute::Both];
                    both2R = busR[FilterRoute::Both];
                }
                sampleL += (both1L + both2L) * 0.7071f;
                sampleR += (both1R + both2R) * 0.7071f;
            }

            float bodyWetL = 0.0f, bodyWetR = 0.0f;
            if (bodyInChain)
            {
                // (The BODY stage ran above, among the cables.)
            }
            else if (params.resonatorOn && resonatorAmount > 0.001f && params.bodyType == 0)
            {
                // M7.5: the live input rings the Classic body too.
                const auto excite = chunkLive[s] * params.inputToBody;
                sampleL = resonatorL.process (sampleL, excite);
                sampleR = resonatorR.process (sampleR, excite);
            }
            else if (params.resonatorOn && resonatorAmount > 0.001f)
            {
                const auto exciteEnvelope = alternateAmpRouting ? 1.0f : chunkAmp[s];
                bodyWetL = materialBodyL.process (chunkBodyL[s] * exciteEnvelope);
                bodyWetR = materialBodyR.process (chunkBodyR[s] * exciteEnvelope);
                sampleL *= 1.0f - resonatorAmount;
                sampleR *= 1.0f - resonatorAmount;
                if (params.bodyCouplingMode == 2 && params.bodyCoupling > 0.0f)
                    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                        if (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode)
                            for (int u = 0; u < stringsPlaying (osc); ++u)
                                stringFor (osc, u).addBridgeInput (0.01f * params.bodyCoupling * (bodyWetL + bodyWetR));
            }

            const auto ampLevelMod = chunkAmpLevel[s];
            auto ampGain = (alternateAmpRouting ? 1.0f : chunkAmp[s]) * ampVelScale
                           * juce::jlimit (0.0f, 2.0f, 1.0f + ampLevelMod);

            // SUSTAIN VOICES: a voice over the cap fades out (startFadeOut).
            if (fadeTotal > 0)
            {
                ampGainFade = fadeRemaining > 0 ? (float) --fadeRemaining / (float) fadeTotal : 0.0f;
                ampGain *= ampGainFade;
            }

            if (params.resonatorOn && params.bodyType != 0 && resonatorAmount > 0.001f && ! bodyInChain)
            {
                const auto wetGain = resonatorAmount * ampVelScale
                                     * juce::jlimit (0.0f, 2.0f, 1.0f + ampLevelMod) * ampGainFade;
                left[startSample + i] += sampleL * ampGain + bodyWetL * wetGain;
                if (right != nullptr)
                    right[startSample + i] += sampleR * ampGain + bodyWetR * wetGain;
                else
                    left[startSample + i] += sampleR * ampGain + bodyWetR * wetGain;
            }
            else
            {
                left[startSample + i] += sampleL * ampGain;
                if (right != nullptr)
                    right[startSample + i] += sampleR * ampGain;
                else
                    left[startSample + i] += sampleR * ampGain;
                if (watchReleaseSilence || watchHeldSilence)
                    releasePeak = juce::jmax (releasePeak, std::abs (sampleL * ampGain), std::abs (sampleR * ampGain));
            }
        }
    };

    // Block-wise sources: within a chunk, every operator and string reads
    // the others only through their previous sample, so each one can run
    // over the whole chunk in turn (the ones it hears first), with the
    // per-sample work (envelopes, modulation, LFOs) done first and the buses
    // summed afterwards in the old order. Every value is computed as in the
    // per-sample loop, so the samples are the same. Cases where two
    // oscillators hear each other within the chunk (cross feedback, a
    // feedback loop in the matrix, the strings ringing each other) or that
    // read another oscillator's current sample (Warp FM / Ring, hard sync,
    // samples, grains, live input) keep the per-sample loop.
    int blockOrder[VoiceParams::numOscillators] {};
    const auto useBlockSources = [&]
    {
        if (! blockSourcesEnabled() || params.hardSync || params.anyExtendedFmMods
            || (params.bodyCouplingMode == 3 && params.bodyCoupling > 0.0f))
            return false;

        // It pays only with two or more wavetable / FM operators; strings and
        // single-oscillator patches measure faster on the per-sample loop.
        bool edge[VoiceParams::numOscillators][VoiceParams::numOscillators] {};
        auto numRendering = 0;
        for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        {
            const auto& settings = params.oscillators[osc];
            const auto mayRender = active[osc] || oscEnableSmooth[osc].getCurrentValue() > 0.0005f
                                   || oscEnableSmooth[osc].isSmoothing();
            if (mayRender && (settings.sampleMode || settings.granularMode || settings.liveMode
                              || settings.stringMode
                              || settings.warpMode == Warp::Fm || settings.warpMode == Warp::Ring
                              || settings.warpMode == Warp::NoiseFm
                              || settings.warpMode2 == Warp::Fm || settings.warpMode2 == Warp::Ring
                              || settings.warpMode2 == Warp::NoiseFm))
                return false;
            numRendering += mayRender ? 1 : 0;
            if (altFeedbackInput[osc] && settings.feedbackType == FmFeedback::Cross)
                return false;
            for (int k = 0; k < numAltSources[osc]; ++k)
                edge[altSources[osc][k]][osc] = true;
        }
        if (numRendering < 2)
            return false;
        for (int target = 0; target < 3; ++target)
            for (int source = 0; source < 3; ++source)
                if (! cellIsSilent (source, target))
                    edge[source][target] = true;
        for (int cell = 0; cell < numExtendedFmCells; ++cell)
            edge[extendedFmCells[cell][0]][extendedFmCells[cell][1]] = true;

        // Each oscillator after the ones it hears (a loop through two or
        // more of them can't be ordered).
        bool placed[VoiceParams::numOscillators] {};
        for (int slot = 0; slot < VoiceParams::numOscillators; ++slot)
        {
            auto next = -1;
            for (int osc = 0; osc < VoiceParams::numOscillators && next < 0; ++osc)
            {
                if (placed[osc])
                    continue;
                auto ready = true;
                for (int source = 0; source < VoiceParams::numOscillators; ++source)
                    ready = ready && (source == osc || placed[source] || ! edge[source][osc]);
                if (ready)
                    next = osc;
            }
            if (next < 0)
                return false;
            placed[next] = true;
            blockOrder[slot] = next;
        }
        return true;
    }();

    // The block-wise sources for one chunk (see useBlockSources).
    const auto renderSourcesBlock = [&] (int chunkStart, int chunkEnd)
    {
        constexpr int numOsc = VoiceParams::numOscillators;
        constexpr int maxU = VoiceParams::maxBufferedUnison;
        const auto n = chunkEnd - chunkStart;

        // Per sample, before the oscillators: envelopes, the Operator EG,
        // LFOs, modulation, and what the oscillators read of them.
        float selectedEnvs[maxChunk][numOsc], levelMods[maxChunk][numOsc], frameMods[maxChunk][numOsc];
        float legacyMods[maxChunk][9], dx7Gains[maxChunk][numOsc], fmNoise[maxChunk], ringMods[maxChunk];
        double dx7Rates[maxChunk];
        float subL[maxChunk], subR[maxChunk], noiseValue[maxChunk];
        bool subOn[maxChunk], noiseOn[maxChunk];
        static constexpr D legacyDestinations[9] { D::FmFeedback, D::Fm1to2, D::Fm1to3,
                                                   D::FmAmount, D::Fm2Feedback, D::Fm2to3,
                                                   D::Fm3to1, D::Fm3to2, D::Fm3Feedback };

        for (int i = chunkStart; i < chunkEnd; ++i)
        {
            const auto slot = i - chunkStart;
            const auto liveSample = params.liveInput != nullptr ? params.liveInput[startSample + i] : 0.0f;
            const auto ampValue = ampEnv.getNextSample();
            const auto control = (i & (modControlInterval - 1)) == 0;
            const auto filterValue = stepEnvelope (filterEnv, 1, control);
            const auto filter2Value = stepEnvelope (filter2Env, 2, control);
            const auto modValue = stepEnvelope (modEnv, 3, control);
            const auto env4Value = stepEnvelope (env4, 4, control);
            for (int index = 0; index < numNeededExtraEnvs; ++index)
            {
                const auto env = (size_t) neededExtraEnvs[index];
                extraEnvValues[env] = stepEnvelope (extraEnvs[env], (int) env + 5, control);
            }
            if (params.msegEnvNeeded)
                msegEnvValue = juce::jlimit (0.0f, 1.0f, envMseg.getNextValue());
            const float envelopeValues[17] { ampValue, filterValue, filter2Value, modValue, env4Value,
                                             extraEnvValues[0], extraEnvValues[1], extraEnvValues[2],
                                             extraEnvValues[3], extraEnvValues[4], extraEnvValues[5],
                                             extraEnvValues[6], extraEnvValues[7], extraEnvValues[8],
                                             extraEnvValues[9], extraEnvValues[10], msegEnvValue };

            float dx7Gain[6] {};
            auto dx7PitchRate = 1.0;
            if (dx7Playing)
            {
                if (dx7Count == 0)
                {
                    if (params.operatorEg != dx7Settings)
                    {
                        dx7Settings = params.operatorEg;
                        dx7Note.update (dx7Settings);
                    }
                    dx7Previous = dx7Current;
                    dx7Note.step();
                    dx7Current = dx7Note.getGains();
                }
                const auto t = (float) (dx7Count + 1) / (float) Dx7::block;
                for (int op = 0; op < 6; ++op)
                    dx7Gain[op] = dx7Previous[(size_t) op] + (dx7Current[(size_t) op] - dx7Previous[(size_t) op]) * t;
                dx7Count = (dx7Count + 1) % Dx7::block;
                if (const auto octaves = dx7Note.getPitchOctaves(); octaves != 0.0f)
                    dx7PitchRate = std::exp2 ((double) octaves);
            }

            advanceVoiceLfos (i);
            evaluateModsRated (mods, i, ampValue, filterValue, filter2Value, modValue, env4Value);

            if ((i & 15) == 0)
                updateSubBlock (mods, filterValue, filter2Value, envelopeValues, juce::jmin (16, numSamples - i));
            else if (params.filter1Fm != 0.0f || params.filter2Fm != 0.0f
                     || mods[(int) D::Filter1Fm] != 0.0f || mods[(int) D::Filter2Fm] != 0.0f)
                updateFilterCoefficients (mods, filterValue, filter2Value);

            for (int osc = 0; osc < numOsc; ++osc)
            {
                const auto ampEnvIndex = params.oscillators[osc].ampEnv;
                selectedEnvs[slot][osc] = envelopeValues[ampEnvIndex <= 16 ? ampEnvIndex : 0];
                levelMods[slot][osc] = mods[(int) levelDestinations[osc]];
                frameMods[slot][osc] = mods[(int) frameDestinations[osc]];
                dx7Gains[slot][osc] = dx7Gain[osc];
            }
            for (int cell = 0; cell < 9; ++cell)
                legacyMods[slot][cell] = mods[(int) legacyDestinations[cell]];
            dx7Rates[slot] = dx7PitchRate;
            ringMods[slot] = params.ringMod + mods[(int) D::RingMod];

            if (anyNoiseOperator)
                stepFmNoise (noiseCoeff);
            fmNoise[slot] = fmNoiseState;

            // The sub and the noise (their own smoothers and generator; the
            // noise's draws keep their place between the LFOs').
            const auto subOscLevel = subOscLevelSmooth.getNextValue();
            const auto enableSubOsc = subOscEnableSmooth.getNextValue();
            subOn[slot] = subOscLevel > 0.0f && params.subOscTable != nullptr && (subOscActive || enableSubOsc > 0.0005f);
            if (subOn[slot])
            {
                subOsc.setFramePosition (0.0f);
                const auto raw = colourSub (subOsc.getNextSample (0.0, subOscFrames), subOscColourSmooth.getNextValue());
                const auto gain = subOscLevel * enableSubOsc;
                subL[slot] = raw * gain * panGainSubOscL * (alternateAmpRouting ? ampValue : 1.0f);
                subR[slot] = raw * gain * panGainSubOscR * (alternateAmpRouting ? ampValue : 1.0f);
            }

            const auto noiseLevel = juce::jlimit (0.0f, 1.0f, noiseSmooth.getNextValue() + mods[(int) D::NoiseLevel])
                                    * noiseEnableSmooth.getNextValue();
            noiseOn[slot] = noiseLevel > 0.0f;
            if (noiseOn[slot])
            {
                auto value = noiseGenerator.process (params.noiseType, random.nextFloat() * 2.0f - 1.0f) * noiseLevel * 0.5f
                             * (alternateAmpRouting ? ampValue : 1.0f);
                if (heardNoiseCoeff < 1.0f)
                {
                    noiseLow += heardNoiseCoeff * (value - noiseLow);
                    value = noiseLow * heardNoiseGain;
                }
                noiseValue[slot] = value;
            }

            chunkLive[slot] = liveSample;
            chunkAmp[slot] = ampValue;
            chunkAmpLevel[slot] = mods[(int) D::AmpLevel];
            chunkWest[slot] = params.west.on && params.west.source != 0
                                  ? juce::jlimit (0.0f, 1.0f, sourceValue ((Mod::Source) params.west.source, i, ampValue, filterValue,
                                                                           filter2Value, modValue, env4Value))
                                  : 0.0f;

            lastAmpValue = ampValue;
            lastLifetimeValue = ampValue;
            if (operatorEgOwnsVoice())
                lastLifetimeValue = dx7Note.isActive() ? 1.0f : 0.0f;
            else if (alternateAmpRouting)
                for (int osc = 0; osc < numOsc; ++osc)
                    if (params.oscillatorEnabled[osc])
                        lastLifetimeValue = juce::jmax (lastLifetimeValue,
                                                        usesOperatorEg (osc) ? (dx7Note.isActive() ? 1.0f : 0.0f)
                                                        : envelopeValues[params.oscillators[osc].ampEnv <= 16 ? params.oscillators[osc].ampEnv : 0]);
            lastFilterValue = filterValue;
            lastFilter2Value = filter2Value;
            lastModValue = modValue;
            lastEnv4Value = env4Value;
        }

        // Each oscillator over the chunk, after the ones it hears. "prev"
        // holds each one's previousOsc after every sample (slot 0: before
        // the chunk); one not run yet is only read through a silent cell.
        float prev[numOsc][maxChunk + 1];
        float monoOut[numOsc][maxChunk] {};
        float driveOut[numOsc][maxChunk] {};
        float heardL[numOsc][maxChunk][maxU], heardR[numOsc][maxChunk][maxU];
        int heardCount[numOsc][maxChunk] {};
        for (int osc = 0; osc < numOsc; ++osc)
        {
            prev[osc][0] = previousOsc[osc];
            for (int slot = 1; slot <= n; ++slot)
                prev[osc][slot] = 0.0f;
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

        const auto modulated = [this] (D destination)
        {
            for (int d = 0; d < params.numActiveDestinations; ++d)
                if (params.activeDestinations[d] == (int) destination)
                    return true;
            return false;
        };

        for (int order = 0; order < numOsc; ++order)
        {
            const auto osc = blockOrder[order];
            const auto& settings = params.oscillators[osc];

            // The extended cells this one hears, in the matrix's order.
            int opCells[27];
            auto numOpCells = 0;
            for (int cell = 0; cell < numExtendedFmCells; ++cell)
                if (extendedFmCells[cell][1] == osc)
                    opCells[numOpCells++] = extendedFmCells[cell][0];

            // A table oscillator whose smoothers rest and whose level and
            // frame aren't modulated: the same enable, level and frame on
            // every sample of the chunk (a resting smoother's next value is
            // its target, with nothing to advance).
            const auto steady = ! oscEnableSmooth[osc].isSmoothing()
                                && ! levelSmooth[osc].isSmoothing() && ! frameSmooth[osc].isSmoothing()
                                && (osc != 2 || ! modulated (levelDestinations[osc])) && ! modulated (frameDestinations[osc])
                                && params.fmMode == 0 && ! anyNoiseOperator;
            // A modulated level (OSC 1-2, 4-6) is read per sample.
            const auto levelPerSample = modulated (levelDestinations[osc]);
            if (steady)
            {
                const auto skipped = (osc >= 3 && ! active[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f)
                                     || (osc < 3 && idleOsc[osc]);
                auto render = false;
                auto renderLevel = 0.0f, enable = 0.0f, levelTarget = 0.0f;
                WavetableOscillator::FrameRead frames {};
                if (! skipped)
                {
                    enable = oscEnableSmooth[osc].getNextValue();
                    const auto level = osc == 2 ? juce::jlimit (0.0f, 1.0f, levelSmooth[osc].getNextValue() + levelMods[0][osc]) : 0.0f;
                    render = (active[osc] || enable > 0.0005f) && (osc != 2 || level > 0.0f);
                    if (render)
                    {
                        const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth[osc].getNextValue() + frameMods[0][osc]);
                        levelTarget = levelSmooth[osc].getNextValue();
                        renderLevel = osc == 2 ? level : juce::jlimit (0.0f, 1.0f, levelTarget + levelMods[0][osc]);
                        frames = WavetableOscillator::frameReadFor (oscBank[osc].getWavetable(), frame, settings.frameBlend == FrameBlend::stepped);
                    }
                }
                const auto operatorEg = usesOperatorEg (osc);
                const auto egRate = operatorEg && settings.tuneMode != OscTuning::Fixed;
                const auto levelEnable = renderLevel * enable;
                const auto oversample = params.quality == 2;
                const auto out = params.oscOut[osc];
                const auto keyGain = keyLevelGain[osc];
                const auto feedbackType = settings.feedbackType;
                auto& bank = oscBank[osc];
                // Nothing to render or hear: its FM input isn't needed.
                if (skipped || ! render)
                {
                    for (int slot = 0; slot < n; ++slot)
                    {
                        monoOut[osc][slot] = 0.0f;
                        prev[osc][slot + 1] = 0.0f;
                        if (anyAltFeedback)
                        {
                            if (feedbackType == FmFeedback::Dx7)
                                feedbackHistory[osc] = 0.0f;
                            else if (feedbackType == FmFeedback::Filtered)
                            {
                                const auto average = 0.5f * (0.0f + feedbackHistory[osc]);
                                feedbackFiltered[osc] += feedbackCoeff[osc] * (average - feedbackFiltered[osc]);
                                feedbackHistory[osc] = 0.0f;
                            }
                        }
                    }
                    continue;
                }
                const auto single = render && bank.canRenderSingle (oversample);
                const auto singleRows = single ? bank.singleRead (frames) : UnisonBank::SingleRead {};

                // An operator that doesn't hear itself: its FM input for the
                // whole chunk first (every source it hears is done), then a
                // lean render loop: the same sums in the same order. (Its own
                // row reads as zero there: a silent cell's term is a zero
                // either way, and the input's + 0.0 drops the zero's sign.)
                // (With DX7 or filtered feedback the input is the sum over
                // its sources, its own term a zero while its cell is.)
                const auto altInput = anyAltFeedbackInput && altFeedbackInput[osc];
                auto selfFed = ! cellIsSilent (osc, osc);
                for (int k = 0; k < numOpCells; ++k)
                    selfFed = selfFed || opCells[k] == osc;
                for (int k = 0; k < numAltSources[osc]; ++k)
                    selfFed = selfFed || altSources[osc][k] == osc;
                if (render && ! selfFed)
                {
                    static constexpr float zeroRow[maxChunk + 1] {};
                    const float* rows[numOsc];
                    for (int source = 0; source < numOsc; ++source)
                        rows[source] = source == osc ? zeroRow : prev[source];

                    double fmIn[maxChunk];
                    if (altInput)
                    {
                        for (int slot = 0; slot < n; ++slot)
                            fmIn[slot] = 0.0;
                        for (int k = 0; k < numAltSources[osc]; ++k)
                        {
                            const auto source = altSources[osc][k];
                            const auto* row = rows[source];
                            if (source >= 3 || osc >= 3)
                            {
                                const auto amount = (double) params.fmMatrix[source][osc];
                                for (int slot = 0; slot < n; ++slot)
                                    fmIn[slot] += amount * (double) row[slot];
                            }
                            else
                                for (int slot = 0; slot < n; ++slot)
                                    fmIn[slot] += (double) (params.fmMatrix[source][osc] + legacyMods[slot][source * 3 + osc]) * (double) row[slot];
                        }
                    }
                    else if (osc == 0)
                        for (int slot = 0; slot < n; ++slot)
                        {
                            const auto* m = legacyMods[slot];
                            const auto fmAmount = params.fmAmount + m[3];
                            const auto fmFeedback = params.fmFeedback + m[0];
                            const auto fm3to1 = params.fmMatrix[2][0] + m[6];
                            fmIn[slot] = (double) (fmAmount * rows[1][slot] + fmFeedback * rows[0][slot]) + (double) (fm3to1 * rows[2][slot]);
                        }
                    else if (osc < 3)
                        for (int slot = 0; slot < n; ++slot)
                        {
                            const auto* m = legacyMods[slot];
                            fmIn[slot] = (double) ((params.fmMatrix[0][osc] + m[osc]) * rows[0][slot]
                                                   + (params.fmMatrix[1][osc] + m[3 + osc]) * rows[1][slot]
                                                   + (params.fmMatrix[2][osc] + m[6 + osc]) * rows[2][slot]);
                        }
                    else
                        for (int slot = 0; slot < n; ++slot)
                            fmIn[slot] = 0.0;
                    for (int k = 0; k < (altInput ? 0 : numOpCells); ++k)
                    {
                        const auto amount = (double) params.fmMatrix[opCells[k]][osc];
                        const auto* row = rows[opCells[k]];
                        for (int slot = 0; slot < n; ++slot)
                            fmIn[slot] += amount * (double) row[slot];
                    }

                    const auto constantGain = ! levelPerSample && ! alternateAmpRouting;
                    const auto fixedGain = levelEnable * 1.0f * keyGain * 1.0f;
                    const auto carrierScale = operatorEg ? dx7CarrierScale : 1.0f;
                    for (int slot = 0; slot < n; ++slot)
                    {
                        const auto rate = egRate ? dx7Rates[slot] : 1.0;
                        auto gain = fixedGain;
                        if (! constantGain)
                        {
                            const auto selectedEnv = operatorEg ? dx7Gains[slot][osc] * dx7LevelScale : selectedEnvs[slot][osc];
                            const auto slotLevelEnable = levelPerSample ? juce::jlimit (0.0f, 1.0f, levelTarget + levelMods[slot][osc]) * enable
                                                                        : levelEnable;
                            gain = slotLevelEnable * (alternateAmpRouting ? selectedEnv : 1.0f) * keyGain * 1.0f;
                        }
                        const auto sums = single ? bank.renderSingle (singleRows, fmIn[slot] + 0.0, rate)
                                                 : bank.render (fmIn[slot] + 0.0, frames, rate, oversample);
                        const auto oscMono = 0.0f + sums.mono * gain;
                        if (out)
                        {
                            const auto heard = operatorEg ? gain * carrierScale : gain;
                            heardL[osc][slot][0] = sums.left * heard;
                            heardR[osc][slot][0] = sums.right * heard;
                            heardCount[osc][slot] = 1;
                        }
                        monoOut[osc][slot] = oscMono;
                        const auto clamped = juce::jlimit (-2.0f, 2.0f, oscMono);
                        prev[osc][slot + 1] = clamped;
                        if (anyAltFeedback)
                        {
                            if (feedbackType == FmFeedback::Dx7)
                                feedbackHistory[osc] = clamped;
                            else if (feedbackType == FmFeedback::Filtered)
                            {
                                const auto average = 0.5f * (clamped + feedbackHistory[osc]);
                                feedbackFiltered[osc] += feedbackCoeff[osc] * (average - feedbackFiltered[osc]);
                                feedbackHistory[osc] = clamped;
                            }
                        }
                    }
                    continue;
                }

                for (int slot = 0; slot < n; ++slot)
                {
                    const auto* m = legacyMods[slot];
                    const auto* p = prev;
                    double fmInput = 0.0;
                    if (osc == 0)
                    {
                        const auto fmAmount = params.fmAmount + m[3];
                        const auto fmFeedback = params.fmFeedback + m[0];
                        const auto fm3to1 = params.fmMatrix[2][0] + m[6];
                        fmInput = (double) (fmAmount * p[1][slot] + fmFeedback * p[0][slot]) + (double) (fm3to1 * p[2][slot]);
                    }
                    else if (osc == 1)
                        fmInput = (double) ((params.fmMatrix[0][1] + m[1]) * p[0][slot]
                                            + (params.fmMatrix[1][1] + m[4]) * p[1][slot]
                                            + (params.fmMatrix[2][1] + m[7]) * p[2][slot]);
                    else if (osc == 2)
                        fmInput = (double) ((params.fmMatrix[0][2] + m[2]) * p[0][slot]
                                            + (params.fmMatrix[1][2] + m[5]) * p[1][slot]
                                            + (params.fmMatrix[2][2] + m[8]) * p[2][slot]);

                    for (int k = 0; k < numOpCells; ++k)
                        fmInput += (double) params.fmMatrix[opCells[k]][osc] * (double) p[opCells[k]][slot];

                    if (anyAltFeedbackInput)
                    {
                        if (altFeedbackInput[osc])
                        {
                            auto sum = 0.0;
                            for (int k = 0; k < numAltSources[osc]; ++k)
                            {
                                const auto source = altSources[osc][k];
                                const auto amount = source >= 3 || osc >= 3 ? params.fmMatrix[source][osc]
                                                                            : params.fmMatrix[source][osc] + m[source * 3 + osc];
                                sum += (double) amount * (double) p[source][slot];
                            }
                            const auto self = (double) (osc >= 3 ? params.fmMatrix[osc][osc] : params.fmMatrix[osc][osc] + m[osc * 4]);
                            if (feedbackType == FmFeedback::Filtered)
                                sum += self * (double) feedbackFiltered[osc];
                            else if (feedbackType == FmFeedback::Dx7)
                                sum += self * (0.5 * ((double) p[osc][slot] + (double) feedbackHistory[osc]));
                            fmInput = sum;
                        }
                        fmInput += 0.0;
                    }

                    auto oscMono = 0.0f;
                    if (render)
                    {
                        const auto rate = 1.0 * (egRate ? dx7Rates[slot] : 1.0);
                        const auto selectedEnv = operatorEg ? dx7Gains[slot][osc] * dx7LevelScale : selectedEnvs[slot][osc];
                        const auto slotLevelEnable = levelPerSample ? juce::jlimit (0.0f, 1.0f, levelTarget + levelMods[slot][osc]) * enable
                                                                    : levelEnable;
                        const auto gain = slotLevelEnable * (alternateAmpRouting ? selectedEnv : 1.0f) * keyGain * 1.0f;
                        const auto sums = single ? bank.renderSingle (singleRows, fmInput + 0.0, rate)
                                                 : bank.render (fmInput + 0.0, frames, rate, oversample);
                        oscMono += sums.mono * gain;
                        if (out)
                        {
                            const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                            heardL[osc][slot][0] = sums.left * heard;
                            heardR[osc][slot][0] = sums.right * heard;
                            heardCount[osc][slot] = 1;
                        }
                    }

                    monoOut[osc][slot] = oscMono;
                    const auto clamped = skipped ? 0.0f : juce::jlimit (-2.0f, 2.0f, oscMono);
                    prev[osc][slot + 1] = clamped;
                    if (anyAltFeedback)
                    {
                        if (feedbackType == FmFeedback::Dx7)
                            feedbackHistory[osc] = clamped;
                        else if (feedbackType == FmFeedback::Filtered)
                        {
                            const auto average = 0.5f * (clamped + feedbackHistory[osc]);
                            feedbackFiltered[osc] += feedbackCoeff[osc] * (average - feedbackFiltered[osc]);
                            feedbackHistory[osc] = clamped;
                        }
                    }
                }
                continue;
            }

            for (int slot = 0; slot < n; ++slot)
            {
                const auto* p = &prev[0][0];
                const auto previous = [&prev, slot] (int source) { return prev[source][slot]; };
                juce::ignoreUnused (p);
                const auto* m = legacyMods[slot];
                const auto amountAt = [this, m] (int source, int target)
                {
                    const auto amount = params.fmMatrix[source][target];
                    if (source >= 3 || target >= 3)
                        return amount;
                    return amount + m[source * 3 + target];
                };

                // The FM input, as the per-sample loop sums it.
                double fmInput = 0.0;
                if (osc == 0)
                {
                    const auto fmAmount = params.fmAmount + m[3];
                    const auto fmFeedback = params.fmFeedback + m[0];
                    const auto fm3to1 = params.fmMatrix[2][0] + m[6];
                    fmInput = (double) (fmAmount * previous (1) + fmFeedback * previous (0)) + (double) (fm3to1 * previous (2));
                }
                else if (osc == 1)
                    fmInput = (double) ((params.fmMatrix[0][1] + m[1]) * previous (0)
                                        + (params.fmMatrix[1][1] + m[4]) * previous (1)
                                        + (params.fmMatrix[2][1] + m[7]) * previous (2));
                else if (osc == 2)
                    fmInput = (double) ((params.fmMatrix[0][2] + m[2]) * previous (0)
                                        + (params.fmMatrix[1][2] + m[5]) * previous (1)
                                        + (params.fmMatrix[2][2] + m[8]) * previous (2));

                for (int cell = 0; cell < numExtendedFmCells; ++cell)
                    if (extendedFmCells[cell][1] == osc)
                        fmInput += (double) amountAt (extendedFmCells[cell][0], osc) * (double) previous (extendedFmCells[cell][0]);

                if (anyAltFeedbackInput)
                {
                    if (altFeedbackInput[osc])
                    {
                        auto sum = 0.0;
                        for (int k = 0; k < numAltSources[osc]; ++k)
                        {
                            const auto source = altSources[osc][k];
                            sum += (double) amountAt (source, osc) * (double) previous (source);
                        }
                        const auto self = (double) amountAt (osc, osc);
                        if (settings.feedbackType == FmFeedback::Filtered)
                            sum += self * (double) feedbackFiltered[osc];
                        else if (settings.feedbackType == FmFeedback::Dx7)
                            sum += self * (0.5 * ((double) previous (osc) + (double) feedbackHistory[osc]));
                        fmInput = sum;
                    }
                    fmInput += 0.0; // the (empty) cross term
                }

                if (anyNoiseOperator)
                    fmInput += (double) (params.fmNoise[osc] * fmNoise[slot]);

                auto oscMono = 0.0f;
                auto skipped = false;
                if (osc >= 3 && ! active[osc] && oscEnableSmooth[osc].getCurrentValue() <= 0.0005f)
                    skipped = true;
                else if (osc < 3 && idleOsc[osc])
                    skipped = true;

                if (! skipped)
                {
                    const auto enable = oscEnableSmooth[osc].getNextValue();
                    const auto level = osc == 2 ? juce::jlimit (0.0f, 1.0f, levelSmooth[osc].getNextValue() + levelMods[slot][osc])
                                                : 0.0f;
                    const auto shouldRender = (active[osc] || enable > 0.0005f) && (osc != 2 || level > 0.0f);

                    if (shouldRender)
                    {
                        const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth[osc].getNextValue() + frameMods[slot][osc]);
                        const auto renderLevel = osc == 2 ? level : juce::jlimit (0.0f, 1.0f,
                                                                                   levelSmooth[osc].getNextValue() + levelMods[slot][osc]);
                        const auto phase = fmPhase (fmInput) + 0.0;
                        const auto operatorEg = usesOperatorEg (osc);
                        const auto rate = fmRate (fmInput)
                                          * (operatorEg && settings.tuneMode != OscTuning::Fixed ? dx7Rates[slot] : 1.0);
                        const auto frames = WavetableOscillator::frameReadFor (oscBank[osc].getWavetable(), frame, settings.frameBlend == FrameBlend::stepped);
                        const auto selectedEnv = operatorEg ? dx7Gains[slot][osc] * dx7LevelScale : selectedEnvs[slot][osc];
                        const auto oscGain = renderLevel * enable * (alternateAmpRouting ? selectedEnv : 1.0f) * keyLevelGain[osc];

                        const auto sums = oscBank[osc].render (phase, frames, rate, params.quality == 2);
                        const auto gain = oscGain * 1.0f;
                        oscMono += sums.mono * gain;
                        if (params.oscOut[osc])
                        {
                            const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                            heardL[osc][slot][0] = sums.left * heard;
                            heardR[osc][slot][0] = sums.right * heard;
                            heardCount[osc][slot] = 1;
                        }
                    }
                    else
                    {
                        frameSmooth[osc].getNextValue();
                        if (osc != 2)
                            levelSmooth[osc].getNextValue();
                    }
                }

                monoOut[osc][slot] = oscMono;
                prev[osc][slot + 1] = juce::jlimit (-2.0f, 2.0f, oscMono);
                if (skipped)
                    prev[osc][slot + 1] = 0.0f;

                if (anyAltFeedback)
                {
                    if (settings.feedbackType == FmFeedback::Dx7)
                        feedbackHistory[osc] = prev[osc][slot + 1];
                    else if (settings.feedbackType == FmFeedback::Filtered)
                    {
                        const auto average = 0.5f * (prev[osc][slot + 1] + feedbackHistory[osc]);
                        feedbackFiltered[osc] += feedbackCoeff[osc] * (average - feedbackFiltered[osc]);
                        feedbackHistory[osc] = prev[osc][slot + 1];
                    }
                }
            }
        }

        for (int osc = 0; osc < numOsc; ++osc)
            previousOsc[osc] = prev[osc][n];

        // The buses, summed in the per-sample loop's order.
        for (int slot = 0; slot < n; ++slot)
        {
            float busL[VoiceRouting::maxBuses] {};
            float busR[VoiceRouting::maxBuses] {};

            for (int osc = 0; osc < numOsc; ++osc)
            {
                for (int u = 0; u < heardCount[osc][slot]; ++u)
                {
                    busL[routes[osc]] += heardL[osc][slot][u];
                    busR[routes[osc]] += heardR[osc][slot][u];
                }

                if (osc == 1)
                {
                    const auto ringMod = ringMods[slot];
                    if (ringMod > 0.0f && active[0] && active[1])
                        for (int bus = 0; bus < numBuses; ++bus)
                        {
                            busL[bus] += (busL[bus] * monoOut[1][slot] - busL[bus]) * ringMod;
                            busR[bus] += (busR[bus] * monoOut[1][slot] - busR[bus]) * ringMod;
                        }
                }
            }

            if (subOn[slot])
            {
                busL[routeSubOsc] += subL[slot];
                busR[routeSubOsc] += subR[slot];
            }
            if (noiseOn[slot])
            {
                busL[routeSubOsc] += noiseValue[slot];
                busR[routeSubOsc] += noiseValue[slot];
            }

            float bodyExciteL = 0.0f, bodyExciteR = 0.0f;
            if (params.resonatorOn && params.bodyType != 0 && resonatorAmount > 0.001f)
            {
                if (bodyStrikePending && params.bodyCouplingMode == 1 && params.bodyCoupling > 0.0f)
                {
                    bool hasString = false;
                    for (int osc = 0; osc < numOsc; ++osc)
                        hasString = hasString || (params.oscillatorEnabled[osc] && params.oscillators[osc].stringMode);
                    if (hasString)
                    {
                        materialBodyL.strike (params.bodyCoupling * velocityLevel);
                        materialBodyR.strike (params.bodyCoupling * velocityLevel);
                    }
                }
                bodyStrikePending = false;
                for (int bus = 0; bus < numBuses; ++bus)
                {
                    bodyExciteL += busL[bus];
                    bodyExciteR += busR[bus];
                }
                bodyExciteL += chunkLive[slot] * params.inputToBody;
                bodyExciteR += chunkLive[slot] * params.inputToBody;
                if (params.bodyCouplingMode == 1)
                {
                    // The mix reaches the body at 20 % and the string's bridge force at
                    // COUPLE; at COUPLE 0 that left the body 14 dB down. Now the mix's share
                    // rises to the whole as COUPLE falls (old patches: body_legacy_level).
                    const auto mixShare = params.bodyLegacyLevel ? 0.2f : 0.2f + 0.8f * (1.0f - params.bodyCoupling);
                    bodyExciteL *= mixShare;
                    bodyExciteR *= mixShare;
                    for (int osc = 0; osc < numOsc; ++osc)
                    {
                        const auto drive = driveOut[osc][slot];
                        bodyExciteL += drive * params.bodyCoupling * 0.2f;
                        bodyExciteR += drive * params.bodyCoupling * 0.2f;
                    }
                }
            }

            std::copy (busL, busL + numBuses, chunkBusL[slot]);
            std::copy (busR, busR + numBuses, chunkBusR[slot]);
            chunkBodyL[slot] = bodyExciteL;
            chunkBodyR[slot] = bodyExciteR;
        }
    };

    classifySlowLfos();
    classifySlowEnvelopes();

    // Watch a released note for silence (not one the modal body, the
    // Operator EG or a live input can still sound through).
    watchReleaseSilence = ! disableReleaseSilence && ! noteHeld && ! monoPending && ! dx7Playing && params.liveInput == nullptr
                          && ! legacyMaterialBody();
    if (! watchReleaseSilence)
        releaseSilentSamples = 0;
    watchHeldSilence = ! disableHeldSilence && noteHeld && ! monoPending && ! dx7Playing && params.liveInput == nullptr
                       && ! legacyMaterialBody() && heldSleepAllowed();
    if (! watchHeldSilence)
        heldSilentSamples = 0;
    heldSourcePeak = 0.0f;

    for (int chunkStart = 0; chunkStart < numSamples;)
    {
        const auto chunkEnd = sampleFeedback ? chunkStart + 1 : juce::jmin (numSamples, (chunkStart | (maxChunk - 1)) + 1);

    if (useBlockSources)
        renderSourcesBlock (chunkStart, chunkEnd);
    else
    for (int i = chunkStart; i < chunkEnd; ++i)
    {
        const auto liveSample = params.liveInput != nullptr ? params.liveInput[startSample + i] : 0.0f;
        const auto ampValue = ampEnv.getNextSample();
        const auto control = (i & (modControlInterval - 1)) == 0;
        const auto filterValue = stepEnvelope (filterEnv, 1, control);
        const auto filter2Value = stepEnvelope (filter2Env, 2, control);
        const auto modValue = stepEnvelope (modEnv, 3, control);
        const auto env4Value = stepEnvelope (env4, 4, control);
        for (int index = 0; index < numNeededExtraEnvs; ++index)
        {
            const auto env = (size_t) neededExtraEnvs[index];
            extraEnvValues[env] = stepEnvelope (extraEnvs[env], (int) env + 5, control);
        }
        if (params.msegEnvNeeded)
            msegEnvValue = juce::jlimit (0.0f, 1.0f, envMseg.getNextValue());
        const float envelopeValues[17] { ampValue, filterValue, filter2Value, modValue, env4Value,
                                         extraEnvValues[0], extraEnvValues[1], extraEnvValues[2],
                                         extraEnvValues[3], extraEnvValues[4], extraEnvValues[5],
                                         extraEnvValues[6], extraEnvValues[7], extraEnvValues[8],
                                         extraEnvValues[9], extraEnvValues[10], msegEnvValue };

        // The Operator EG: step the operator gains every 64 samples, ramp between.
        float dx7Gain[6] {};
        auto dx7PitchRate = 1.0;
        if (dx7Playing)
        {
            if (dx7Count == 0)
            {
                // Settings turned (or modulated) since the note began.
                if (params.operatorEg != dx7Settings)
                {
                    dx7Settings = params.operatorEg;
                    dx7Note.update (dx7Settings);
                }
                dx7Previous = dx7Current;
                dx7Note.step();
                dx7Current = dx7Note.getGains();
            }
            const auto t = (float) (dx7Count + 1) / (float) Dx7::block;
            for (int op = 0; op < 6; ++op)
                dx7Gain[op] = dx7Previous[(size_t) op] + (dx7Current[(size_t) op] - dx7Previous[(size_t) op]) * t;
            dx7Count = (dx7Count + 1) % Dx7::block;
            if (const auto octaves = dx7Note.getPitchOctaves(); octaves != 0.0f)
                dx7PitchRate = std::exp2 ((double) octaves);
        }

        advanceVoiceLfos (i);
        evaluateModsRated (mods, i, ampValue, filterValue, filter2Value, modValue, env4Value);

        if ((i & 15) == 0)
            updateSubBlock (mods, filterValue, filter2Value, envelopeValues, juce::jmin (16, numSamples - i));
        else if (params.filter1Fm != 0.0f || params.filter2Fm != 0.0f
                 || mods[(int) D::Filter1Fm] != 0.0f || mods[(int) D::Filter2Fm] != 0.0f)
            updateFilterCoefficients (mods, filterValue, filter2Value);

        // Keep the old operator order: 1, sync, 2, ring, then 3.
        float busL[VoiceRouting::maxBuses] {};
        float busR[VoiceRouting::maxBuses] {};
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

        for (int cell = 0; cell < numExtendedFmCells; ++cell)
        {
            const auto source = extendedFmCells[cell][0];
            const auto target = extendedFmCells[cell][1];
            fmInput[target] += (double) fmAmountAt (mods, source, target) * (double) previousOsc[source];
        }

        // Filtered and cross feedback replace an operator's plain self term.
        if (anyAltFeedbackInput)
        {
            double cross[VoiceParams::numOscillators] {};

            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
            {
                const auto type = params.oscillators[osc].feedbackType;

                if (! altFeedbackInput[osc])
                    continue;

                auto sum = 0.0;
                for (int k = 0; k < numAltSources[osc]; ++k)
                {
                    const auto source = altSources[osc][k];
                    sum += (double) fmAmountAt (mods, source, osc) * (double) previousOsc[source];
                }

                const auto self = (double) fmAmountAt (mods, osc, osc);

                if (type == FmFeedback::Filtered)
                    sum += self * (double) feedbackFiltered[osc];
                else if (type == FmFeedback::Dx7)
                {
                    // The DX7's own feedback: the plain average of the last
                    // two samples.
                    sum += self * (0.5 * ((double) previousOsc[osc] + (double) feedbackHistory[osc]));
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

        // FM Noise: the NOISE card's generator (its type and colour, not its level
        // or switch). One draw a sample, which the heard noise below reuses.
        auto warpNoiseDraw = 0.0f, warpNoiseHeard = 0.0f;
        if (anyWarpNoise)
        {
            warpNoiseDraw = noiseGenerator.process (params.noiseType, random.nextFloat() * 2.0f - 1.0f);
            warpNoiseHeard = warpNoiseDraw;

            if (heardNoiseCoeff < 1.0f)
            {
                warpNoiseLow += heardNoiseCoeff * (warpNoiseDraw - warpNoiseLow);
                warpNoiseHeard = warpNoiseLow * heardNoiseGain;
            }
        }

        // The noise operator: coloured noise into any oscillator's FM input.
        if (anyNoiseOperator)
        {
            stepFmNoise (noiseCoeff);

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

        for (int v = 0; v < numVisitOscs; ++v)
        {
            const auto osc = visitOscs[v];
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
            // WARP 2 as FM / Ring / FM Noise (appended choices; the oscillator itself skips them).
            const auto warp2Amount = settings.warpMode2 == Warp::Fm || settings.warpMode2 == Warp::Ring
                                             || settings.warpMode2 == Warp::NoiseFm
                                         ? juce::jlimit (0.0f, 1.0f, settings.warpAmount2
                                                         + (settings.pdEnv > 0 ? settings.pdEnvAmount * envelopeValues[juce::jlimit (0, 16, settings.pdEnv - 1)] : 0.0f))
                                         : 0.0f;
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
                                   + (settings.warpMode == Warp::Fm ? (double) (warpAmount * warpSource) : 0.0)
                                   + (settings.warpMode == Warp::NoiseFm ? (double) (warpAmount * warpNoiseHeard) : 0.0)
                                   + (settings.warpMode2 == Warp::Fm ? (double) (warp2Amount * warpSource) : 0.0)
                                   + (settings.warpMode2 == Warp::NoiseFm ? (double) (warp2Amount * warpNoiseHeard) : 0.0);
                // The Operator EG's pitch envelope and LFO move every ratio
                // operator it plays (a fixed-frequency one stays put, as on
                // the DX7).
                const auto operatorEg = usesOperatorEg (osc);
                const auto rate = fmRate (fmInput[osc])
                                  * (operatorEg && settings.tuneMode != OscTuning::Fixed ? dx7PitchRate : 1.0);
                const auto ring = (settings.warpMode == Warp::Ring
                                       ? 1.0f + (warpSource - 1.0f) * warpAmount : 1.0f)
                                  * (settings.warpMode2 == Warp::Ring
                                         ? 1.0f + (warpSource - 1.0f) * warp2Amount : 1.0f);
                const auto frames = WavetableOscillator::frameReadFor (oscBank[osc].getWavetable(), frame, settings.frameBlend == FrameBlend::stepped);

                auto stringSum = 0.0f;
                // The Operator EG's gain in place of an envelope (ENV 1 if the
                // note started before an oscillator picked it).
                const auto selectedEnv = operatorEg ? dx7Gain[osc] * dx7LevelScale
                                                    : envelopeValues[settings.ampEnv <= 16 ? settings.ampEnv : 0];
                const auto oscGain = renderLevel * enable * (alternateAmpRouting ? selectedEnv : 1.0f) * keyLevelGain[osc];
                const auto wavetable = ! settings.granularMode && ! settings.sampleMode && ! settings.stringMode
                                       && ! settings.liveMode;

                if (wavetable)
                {
                    const auto sums = oscBank[osc].render (phase, frames, rate, params.quality == 2);
                    const auto gain = oscGain * ring;
                    oscMono[osc] += sums.mono * gain;

                    if (params.oscOut[osc])
                    {
                        // Operator EG carriers are heard as Dexed scales them;
                        // the FM path above keeps the gain in cycles.
                        const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                        busL[routes[osc]] += sums.left * heard;
                        busR[routes[osc]] += sums.right * heard;
                    }
                }

                // Strings alone: the loop below without the other modes'
                // branches (the same sums in the same order).
                const auto stringsOnly = settings.stringMode && ! settings.liveMode && ! settings.granularMode
                                         && ! settings.sampleMode;
                if (stringsOnly)
                {
                    const auto trim = exciterTrimFor (settings.stringExcite, params.exciterLevels);
                    const auto match = trim != 1.0f;
                    const auto liveIn = params.inputToStrings > 0.0f;
                    const auto liveAmount = liveSample * params.inputToStrings;
                    const auto envFactor = alternateAmpRouting ? selectedEnv : 1.0f;
                    const auto keyGain = keyLevelGain[osc];
                    const auto fm = (float) fmInput[osc];
                    const auto out = params.oscOut[osc];
                    const auto route = routes[osc];
                    auto mono = oscMono[osc];
                    auto bl = busL[route], br = busR[route];
                    if (stringTwin[osc])
                    {
                        // A stereo route on one string: string 0 is the left
                        // side and its twin the right, each at the voice's
                        // own pan (identical while the route sits at 0).
                        auto& left = stringFor (osc, 0);
                        auto& right = stringFor (osc, 1);
                        if (liveIn)
                        {
                            left.addLiveInput (liveAmount);
                            right.addLiveInput (liveAmount);
                        }
                        // Both sides in one call (the same as two process() calls).
                        // A resting route: both sides are string 0.
                        float rawL, rawR;
                        if (twinAsleep[osc])
                            rawL = rawR = left.process (aftertouchValue, noteHeld, fm);
                        else
                            left.processPair (right, aftertouchValue, noteHeld, fm, rawL, rawR);
                        auto sideL = left.side(), sideR = twinAsleep[osc] ? sideL : right.side();
                        if (match)
                        {
                            rawL *= trim;
                            rawR *= trim;
                            sideL *= trim;
                            sideR *= trim;
                        }
                        if (watchHeldSilence)
                            heldSourcePeak = juce::jmax (heldSourcePeak, std::abs (rawL) + std::abs (sideL), std::abs (rawR) + std::abs (sideR));
                        const auto raw = 0.5f * (rawL + rawR);
                        stringSum += raw;

                        const auto gain = unisonGains[osc][0] * renderLevel * enable * envFactor * keyGain;
                        mono += raw * gain;
                        if (out)
                        {
                            const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                            bl += (rawL + sideL) * heard * panGainL[osc][0];
                            br += (rawR - sideR) * heard * panGainR[osc][0];
                        }
                    }
                    for (int u = 0; u < (stringTwin[osc] ? 0 : numOscUnison[osc]); ++u)
                    {
                        auto& string = stringFor (osc, u);
                        if (liveIn)
                            string.addLiveInput (liveAmount);
                        auto raw = string.process (aftertouchValue, noteHeld, fm);
                        auto side = string.side(); // (stereo exciters)
                        if (match)
                        {
                            raw *= trim;
                            side *= trim;
                        }
                        if (watchHeldSilence)
                            heldSourcePeak = juce::jmax (heldSourcePeak, std::abs (raw) + std::abs (side));
                        stringSum += raw;

                        const auto gain = unisonGains[osc][u] * renderLevel * enable * envFactor * keyGain;
                        mono += raw * gain;
                        if (out)
                        {
                            const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                            bl += (raw + side) * heard * panGainL[osc][u];
                            br += (raw - side) * heard * panGainR[osc][u];
                        }
                    }
                    oscMono[osc] = mono;
                    busL[route] = bl;
                    busR[route] = br;
                }

                for (int u = 0; u < (wavetable || stringsOnly ? 0 : numOscUnison[osc]); ++u)
                {
                    float raw = 0.0f;
                    float sampleL = 0.0f;
                    float sampleR = 0.0f;
                    float side = 0.0f; // (stereo exciters)

                    if (settings.liveMode)
                    {
                        raw = liveSample;
                    }
                    else if (settings.granularMode)
                    {
                        grains[osc][u].process (sampleL, sampleR);
                        raw = 0.5f * (sampleL + sampleR);
                    }
                    else if (settings.sampleMode)
                    {
                        sampleUnison[osc][u].process (sampleL, sampleR);
                        if (layerZone[osc] != nullptr)
                        {
                            float layerL = 0.0f, layerR = 0.0f;
                            sampleLayer[osc][u].process (layerL, layerR);
                            sampleL += layerL;
                            sampleR += layerR;
                        }
                        raw = 0.5f * (sampleL + sampleR);
                    }
                    else
                    {
                        if (params.inputToStrings > 0.0f)
                            stringFor (osc, u).addLiveInput (liveSample * params.inputToStrings);
                        auto& string = stringFor (osc, u);
                        raw = string.process (aftertouchValue, noteHeld, (float) fmInput[osc]);
                        side = string.side();
                        if (params.exciterLevels > 0)
                        {
                            const auto trim = exciterTrimFor (settings.stringExcite, params.exciterLevels);
                            raw *= trim;
                            side *= trim;
                        }
                        stringSum += raw;
                    }

                    const auto gain = unisonGains[osc][u] * renderLevel * enable
                                      * (alternateAmpRouting ? selectedEnv : 1.0f) * keyLevelGain[osc];
                    oscMono[osc] += raw * gain;

                    if (params.oscOut[osc])
                    {
                        // Heard at the Operator Env's carrier scale too, so
                        // picking it changes every mode's level alike.
                        const auto heard = operatorEg ? gain * dx7CarrierScale : gain;
                        busL[routes[osc]] += (settings.sampleMode ? sampleL : raw + side) * heard * panGainL[osc][u];
                        busR[routes[osc]] += (settings.sampleMode ? sampleR : raw - side) * heard * panGainR[osc][u];
                    }
                }

                // Coupled strings: the strings of one note share the bridge,
                // which soaks up their in-phase motion. The in-phase part of
                // the note dies fast (the prompt sound); as the detuned
                // strings drift apart, the rest rings on (the aftersound).
                if (settings.stringMode && ! settings.sampleMode && settings.couple > 0.0f && numOscUnison[osc] > 1)
                {
                    const auto count = juce::jmin (numOscUnison[osc], VoiceParams::maxBufferedUnison);
                    const auto bridge = stringSum / (float) juce::jmax (1, count);

                    for (int u = 0; u < count; ++u)
                        stringFor (osc, u).addBridgeInput (-settings.couple * settings.couple * 30.0f * PianoTuning::get().coupling * bridge);
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
                && oscBank[0].lane0Wrapped())
                for (int u = 0; u < numOscUnison[1]; ++u)
                    oscBank[1].resetPhase (u, 0.0);

            if (osc == 1)
            {
                const auto ringMod = params.ringMod + mods[(int) D::RingMod];

                if (ringMod > 0.0f && active[0] && active[1])
                    for (int bus = 0; bus < numBuses; ++bus)
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
        // The skipped oscillators' outputs, as the loop above would have
        // left them (nothing in it reads them).
        for (int v = 0; v < numSkippedOscs; ++v)
        {
            const auto osc = skippedOscs[v];
            if (osc == 1)
                previousOsc[0] = juce::jlimit (-2.0f, 2.0f, oscMono[0]);
            if (osc != 0 || skipsOsc1)
                previousOsc[osc] = 0.0f;
        }

        if (anyAltFeedback)
            for (int k = 0; k < numHistoryOscs; ++k)
                if (const auto osc = historyOscs[k]; params.oscillators[osc].feedbackType == FmFeedback::Dx7)
                    feedbackHistory[osc] = previousOsc[osc];
                else
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
            const auto raw = colourSub (subOsc.getNextSample (0.0, subOscFrames), subOscColourSmooth.getNextValue());
            const auto gain = subOscLevel * enableSubOsc;
            busL[routeSubOsc] += raw * gain * panGainSubOscL * (alternateAmpRouting ? ampValue : 1.0f);
            busR[routeSubOsc] += raw * gain * panGainSubOscR * (alternateAmpRouting ? ampValue : 1.0f);
        }

        const auto noiseLevel = juce::jlimit (0.0f, 1.0f, noiseSmooth.getNextValue() + mods[(int) D::NoiseLevel])
                                * noiseEnableSmooth.getNextValue();

        if (noiseLevel > 0.0f)
        {
            auto value = (anyWarpNoise ? warpNoiseDraw : noiseGenerator.process (params.noiseType, random.nextFloat() * 2.0f - 1.0f)) * noiseLevel * 0.5f
                         * (alternateAmpRouting ? ampValue : 1.0f);
            // NOISE COLOUR under white: a one-pole low-pass, part of the
            // lost level made up (white skips it, so older patches are
            // unchanged).
            if (heardNoiseCoeff < 1.0f)
            {
                noiseLow += heardNoiseCoeff * (value - noiseLow);
                value = noiseLow * heardNoiseGain;
            }
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
            for (int bus = 0; bus < numBuses; ++bus)
            {
                bodyExciteL += busL[bus];
                bodyExciteR += busR[bus];
            }
            // M7.5: the live input rings the body too.
            bodyExciteL += liveSample * params.inputToBody;
            bodyExciteR += liveSample * params.inputToBody;
            if (params.bodyCouplingMode == 1)
            {
                // The mix reaches the body at 20 % and the string's bridge force at
                // COUPLE; at COUPLE 0 that left the body 14 dB down. Now the mix's share
                // rises to the whole as COUPLE falls (old patches: body_legacy_level).
                const auto mixShare = params.bodyLegacyLevel ? 0.2f : 0.2f + 0.8f * (1.0f - params.bodyCoupling);
                bodyExciteL *= mixShare;
                bodyExciteR *= mixShare;
                for (int k = 0; k < numDrivingOscs; ++k)
                {
                    const auto drive = stringDrive[drivingOscs[k]];
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
                const auto count = stringsPlaying (osc);
                for (int u = 0; u < count; ++u)
                    stringFor (osc, u).addBridgeInput (0.015f * params.bodyCoupling * otherDrive / (float) otherCount);
            }
        }

        const auto slot = i - chunkStart;
        std::copy (busL, busL + numBuses, chunkBusL[slot]);
        std::copy (busR, busR + numBuses, chunkBusR[slot]);
        chunkBodyL[slot] = bodyExciteL;
        chunkBodyR[slot] = bodyExciteR;
        chunkLive[slot] = liveSample;
        chunkAmp[slot] = ampValue;
        chunkAmpLevel[slot] = mods[(int) D::AmpLevel];
        chunkWest[slot] = params.west.on && params.west.source != 0
                              ? juce::jlimit (0.0f, 1.0f, sourceValue ((Mod::Source) params.west.source, i, ampValue, filterValue,
                                                                       filter2Value, modValue, env4Value))
                              : 0.0f;

        lastAmpValue = ampValue;
        lastLifetimeValue = ampValue;
        if (operatorEgOwnsVoice())
            lastLifetimeValue = dx7Note.isActive() ? 1.0f : 0.0f;
        else if (alternateAmpRouting)
            for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
                if (params.oscillatorEnabled[osc])
                    lastLifetimeValue = juce::jmax (lastLifetimeValue,
                                                    usesOperatorEg (osc) ? (dx7Note.isActive() ? 1.0f : 0.0f)
                                                    : envelopeValues[params.oscillators[osc].ampEnv <= 16 ? params.oscillators[osc].ampEnv : 0]);
        lastFilterValue = filterValue;
        lastFilter2Value = filter2Value;
        lastModValue = modValue;
        lastEnv4Value = env4Value;
    }

    postChunk (chunkStart, chunkEnd - chunkStart);

        chunkStart = chunkEnd;
    }

    const auto samplePositionOf = [] (const SamplePlayer& player, const SampleData* data)
    {
        if (data == nullptr || data->getNumSamples() <= 0 || ! player.isActive())
            return -1.0f;

        return (float) juce::jlimit (0.0, 1.0, player.getSourcePosition() / (double) data->getNumSamples());
    };

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        lastSamplePosition[osc] = params.oscillators[osc].sampleMode
                                      ? samplePositionOf (sampleUnison[osc][0], sampleZone[osc] != nullptr ? sampleZone[osc]->data.get()
                                                                                                           : params.oscillators[osc].sample)
                                      : -1.0f;

    if (bodyTailSamplesRemaining > 0)
        bodyTailSamplesRemaining = juce::jmax (0, bodyTailSamplesRemaining - numSamples);

    if (fadeTotal > 0 && fadeRemaining == 0)
        stopNote (0.0f, false);

    // A released note whose output has stayed under -110 dB for about 90 ms
    // is over, even if its release time is not: free the voice now rather
    // than render silence (decayed strings, long releases).
    if (watchReleaseSilence)
    {
        releaseSilentSamples = releasePeak < releaseSilence ? releaseSilentSamples + numSamples : 0;
        releasePeak = 0.0f;
        if (releaseSilentSamples >= (int) (sampleRate * 0.09))
            stopNote (0.0f, false);
    }
    else if (watchHeldSilence)
    {
        // Silent at the output and at the strings themselves: an output
        // held at zero by the amp (its DELAY, an MSEG, a route into a
        // level) while the strings still ring is not a note that has died.
        const auto silent = releasePeak < releaseSilence && heldSourcePeak < releaseSilence;
        heldSilentSamples = silent ? heldSilentSamples + numSamples : 0;
        releasePeak = 0.0f;
        heldSourcePeak = 0.0f;
        if (heldSilentSamples >= (int) (sampleRate * 0.09))
        {
            heldAsleep = true;
            rememberFilterSettings();
        }
    }

    if (! hasActiveAmpEnvelope())
    {
        lastAmpValue = 0.0f;
        lastLifetimeValue = 0.0f;
        clearCurrentNote();
    }
}
