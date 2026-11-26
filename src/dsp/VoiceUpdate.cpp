#include "Voice.h"
#include "FastMath.h"
#include "Generative.h"
#include "StretchRenderer.h"

#include <array>
#include <cmath>
#include "VoiceInternal.h"

void Voice::configureString (KarplusStrong& string, const VoiceParams::OscParams& settings, float side) const
{
    auto stiffness = settings.stringStiffness;
    auto damping = settings.stringDamping;
    auto decay = settings.stringDecay;
    auto excitePosition = settings.stringExcitationPosition;
    auto hammer = settings.hammerHardness;
    auto bowPressure = settings.bowPressure;

    // Stereo string routes: this string's side of them (every knob is 0..1).
    if (side != 0.0f)
    {
        namespace S = Mod::StereoString;
        const auto& offsets = settings.stringStereoOffsets;
        const auto moved = [side] (float value, float offset) { return juce::jlimit (0.0f, 1.0f, value + side * offset); };
        decay = moved (decay, offsets[S::decay]);
        damping = moved (damping, offsets[S::damping]);
        stiffness = moved (stiffness, offsets[S::stiffness]);
        excitePosition = moved (excitePosition, offsets[S::excitePosition]);
        hammer = moved (hammer, offsets[S::hammer]);
        bowPressure = moved (bowPressure, offsets[S::bowPressure]);
    }

    // Register map: short, stiff, bright treble strings; long, looser bass
    // strings that ring longer.
    if (settings.registerMap > 0.0f)
    {
        // The knob's whole travel: bass (-1, C2 and below) to treble (+1, C6
        // and above), twice as steep as before so the range is audible.
        const auto amount = settings.registerMap * 2.0f;
        const auto t = juce::jlimit (-1.0f, 1.0f, keyTrackValue * 2.0f);
        // Wound bass strings are less stiff than the plain treble ones.
        stiffness = juce::jlimit (0.0f, 1.0f, stiffness + amount * (0.16f + 0.35f * juce::jmax (0.0f, t)
                                                                         - 0.1f * juce::jmax (0.0f, -t)));
        damping = juce::jlimit (0.0f, 1.0f, damping - amount * 0.25f * t);
        decay = juce::jlimit (0.0f, 1.0f, decay - amount * 0.05f * t);
    }

    string.setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, KarplusStrong::numExcites - 1, settings.stringExcite)),
                      settings.stringSustain, damping, decay);
    string.setPhysicalParams (stiffness, settings.stringPickup,
                              excitePosition, settings.stringPickHardness,
                              settings.stringPickPosition, settings.stringSlap);
    string.setBowAndBuzz (bowPressure, settings.bowSpeed, settings.bridgeBuzz, settings.fretRattle);
    string.setSplit (settings.stringSplit);
    string.setClick (settings.stringClick);
    string.setKeysParams (hammer, settings.damper);
    string.setEco (params.quality == 0);
    string.setFeedbackParams (settings.fbGain, settings.fbDistance);
    string.setModalShape (settings.modalSize, settings.modalScale);
    if (string.isElectric())
        string.setElectricParams (settings.epDistance, settings.epPosition);
}

bool Voice::operatorEgOwnsVoice() const
{
    if (! dx7Playing)
        return false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillatorEnabled[osc] && params.oscillators[osc].ampEnv != OperatorEg::envelopeChoice)
            return false;
    return true;
}

bool Voice::hasActiveAmpEnvelope() const
{
    if (operatorEgOwnsVoice())
        return dx7Note.isActive();
    // The modal body rings on after the envelopes (up to 1.5 x its decay,
    // 10 s at most), but only while it can be heard: once its level is under
    // -110 dB (its wet gain is at most 2: under -104 dB out per voice) the
    // tail is over, and the voice ends with the envelopes. A body that never
    // rang (BODY AMOUNT at 0) holds no voice at all.
    constexpr auto bodySilence = 3.0e-6f;
    if (legacyMaterialBody() && bodyTailSamplesRemaining > 0
        && (materialBodyL.level() > bodySilence || materialBodyR.level() > bodySilence))
        return true;
    return envelopesActive();
}

bool Voice::envelopesActive() const
{
    bool anyOscillator = false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        if (! params.oscillatorEnabled[osc])
            continue;
        anyOscillator = true;
        if (usesOperatorEg (osc))
        {
            if (dx7Note.isActive())
                return true;
            continue;
        }
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
    return (params.subOscEnabled || (params.noiseEnabled && params.noiseLevel > 0.0f) || ! anyOscillator) && ampEnv.isActive();
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

// (samples: how many the sub-block runs, 16 but at a block's end.)
void Voice::updateSubBlock (const float* mods, float filterEnvValue, float filter2EnvValue, const float* envelopeValues, int samples)
{
    using D = Mod::Destination;

    if (glidesMoving)
        advanceGlides (samples);

    const auto driftedFrequency = advanceGlideAndDrift (mods[(int) D::Drift], samples);

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

        // OSC 3 at LEVEL 0 is skipped, unless a route opens its level (it
        // then plays and needs its pitch, warp and unison set up).
        if (osc == 2 && settings.level <= 0.0f && mods[(int) D::SubLevel] <= 0.0f)
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
            // Scale quantise: the note plus SEMI snaps to the nearest scale note.
            auto scaleShift = 0.0;
            if (settings.scale > 0)
            {
                const auto target = pitchNote + (int) std::lround (settings.semitones
                                                                 + (osc == 2 ? (double) params.subOctaveOffset : 0.0));
                scaleShift = (double) (Scales::quantize (target, settings.scale, settings.scaleRoot) - target);
            }

            const auto pitch = std::exp2 ((settings.semitones + scaleShift + (osc == 2 ? (double) params.subOctaveOffset : 0.0)
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

        oscBank[osc].setWarp (settings.warpMode, warp);
        oscBank[osc].setWarp2 (settings.warpMode2, warp2);
        oscBank[osc].setSpreads (settings.frameSpread, settings.warpSpread);
        {
            // Stereo FRAME / WARP routes (2026-10-30): the bank plays its voices apart, one voice as a left / right twin.
            const auto frameIndex = Mod::StereoIndex::frame + osc, warpIndex = Mod::StereoIndex::warp + osc;
            const auto stereoFrame = stereoRouted[(size_t) frameIndex], stereoWarp = stereoRouted[(size_t) warpIndex];
            oscBank[osc].setStereo (stereoFrame || stereoWarp,
                                    stereoFrame ? mods[(int) Mod::Destination::Count + frameIndex] : 0.0f,
                                    stereoWarp ? mods[(int) Mod::Destination::Count + warpIndex] : 0.0f);
        }

        oscBank[osc].setFrequencies (baseFreq, unisonRatio[osc], numOscUnison[osc]);

        for (int u = 0; u < numOscUnison[osc]; ++u)
        {
            const auto frequencyU = baseFreq * unisonRatio[osc][u];

            if (u < VoiceParams::maxBufferedUnison)
            {
                stringFor (osc, u).setFrequency (frequencyU);
                if (stringTwin[osc])
                    stringFor (osc, 1).setFrequency (frequencyU);
                const auto* zone = sampleZone[osc];
                const auto* played = zone != nullptr ? zone->data.get() : settings.sample;
                sampleRatio[osc][u] = settings.sampleTuned ? frequencyU / (zone != nullptr ? zone->rootHz() : plainRootHz (settings.sampleRoot)) : 1.0;
                grains[osc][u].setPlaybackRatio (sampleRatio[osc][u]);
                const auto rateRatio = played != nullptr ? played->sampleRate / sampleRate : 1.0;
                // Step 40: SYNC speeds the sample to the host's tempo; with
                // STRETCH the note sets only the pitch. Neither: as before.
                if (settings.sampleSync == SampleTempo::off && ! settings.sampleStretch && settings.sampleEngine < 0)
                {
                    sampleUnison[osc][u].setPlaybackRatio (rateRatio * sampleRatio[osc][u]);
                }
                else
                {
                    const auto regionSeconds = played != nullptr && played->sampleRate > 0.0
                                                   ? (double) juce::jmax (0.0f, settings.sampleEnd - settings.sampleStart)
                                                         * played->getNumSamples() / played->sampleRate
                                                   : 0.0;
                    const auto tempo = SampleTempo::tempoRatio (settings.sampleSync, settings.sampleBpm, regionSeconds, params.hostBpm);
                    if (stretchCopy[osc] != nullptr)
                        // The copy is already the note's length and pitch: the
                        // live stretch adds only what changed since (bend,
                        // cents, tempo) and the length's rounding.
                        sampleUnison[osc][u].setRates (rateRatio * tempo * StretchRenderer::lengthForStep (stretchStep[osc]),
                                                       rateRatio * sampleRatio[osc][u] / std::exp2 (stretchSemis[osc] / 12.0));
                    else if (settings.sampleStretch || settings.sampleEngine >= 0)
                        sampleUnison[osc][u].setRates (rateRatio * tempo, rateRatio * sampleRatio[osc][u]);
                    else
                        sampleUnison[osc][u].setPlaybackRatio (rateRatio * sampleRatio[osc][u] * tempo);
                }

                // Step 37: the blended velocity layer, at its own root and rate.
                if (const auto* layer = layerZone[osc]; layer != nullptr && layer->data->sampleRate > 0.0)
                {
                    const auto layerRate = layer->data->sampleRate / sampleRate;
                    const auto layerPitch = settings.sampleTuned ? frequencyU / layer->rootHz() : 1.0;
                    const auto layerSeconds = (double) juce::jmax (0.0f, settings.sampleEnd - settings.sampleStart)
                                              * layer->data->getNumSamples() / layer->data->sampleRate;
                    const auto layerTempo = settings.sampleSync == SampleTempo::off
                                                ? 1.0
                                                : SampleTempo::tempoRatio (settings.sampleSync, settings.sampleBpm, layerSeconds, params.hostBpm);
                    if (settings.sampleStretch || settings.sampleEngine >= 0)
                        sampleLayer[osc][u].setRates (layerRate * layerTempo, layerRate * layerPitch);
                    else
                        sampleLayer[osc][u].setPlaybackRatio (layerRate * layerPitch * layerTempo);
                }
            }
        }
    }

    if (params.subOscLevel > 0.0f)
        subOsc.setFrequency (driftedFrequency * std::exp2 (((double) params.subOscOctave + bendSemitones) / 12.0));

    tuneBody (driftedFrequency * bodyPitchFactor (mods));

    updateFilterCoefficients (mods, filterEnvValue, filter2EnvValue);
}

float& Voice::glidedParam (int which) noexcept
{
    switch (which)
    {
        case glideFmAmount:    return params.fmAmount;
        case glideFmFeedback:  return params.fmFeedback;
        case glideRingMod:     return params.ringMod;
        case glideCutoff1:     return params.filter1.cutoffHz;
        case glideReso1:       return params.filter1.resonance;
        case glideDrive1:      return params.filter1.drive;
        case glideCutoff2:     return params.filter2.cutoffHz;
        case glideReso2:       return params.filter2.resonance;
        case glideDrive2:      return params.filter2.drive;
        case glideVoiceSpread: return params.voiceSpread;
        default: break;
    }
    return which < glideOscSpread ? params.oscillators[which - glideOscPan].pan
                                  : params.oscillators[which - glideOscSpread].spread;
}

// A render's start: the knobs the processor just sent become the glides'
// targets, and params holds where each glide is (BlockGlide).
void Voice::retargetGlides (int numSamples)
{
    glideClock += numSamples;
    if (paramsFresh)
    {
        paramsFresh = false;
        const auto minGlide = (int) (0.0005 * sampleRate), maxGlide = (int) (0.025 * sampleRate);
        for (int k = 0; k < numParamGlides; ++k)
        {
            auto& glide = paramGlides[(size_t) k];
            if (glidesSnap)
                glide.snap (glidedParam (k));
            else
                glide.retarget (glidedParam (k), glideClock, minGlide, maxGlide);
        }
    }
    for (int k = 0; k < numParamGlides; ++k)
        if (paramGlides[(size_t) k].moving())
            glidedParam (k) = paramGlides[(size_t) k].value;
    // (The processor mirrors FM AMOUNT and FEEDBACK into the matrix.)
    if (paramGlides[glideFmAmount].moving())
        params.fmMatrix[1][0] = params.fmAmount;
    if (paramGlides[glideFmFeedback].moving())
        params.fmMatrix[0][0] = params.fmFeedback;
}

// One sub-block of the glides; the pans and drives follow theirs.
void Voice::advanceGlides (int samples)
{
    auto still = true;
    for (int k = 0; k < numParamGlides; ++k)
    {
        auto& glide = paramGlides[(size_t) k];
        if (! glide.moving())
            continue;
        glide.advance (samples);
        glidedParam (k) = glide.value;
        still = still && ! glide.moving();
    }
    params.fmMatrix[1][0] = params.fmAmount;
    params.fmMatrix[0][0] = params.fmFeedback;
    for (auto* glide : { &panModGlide, &driveModGlide[0], &driveModGlide[1] })
    {
        glide->advance (samples);
        still = still && ! glide->moving();
    }
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        oscPanModGlide[(size_t) osc].advance (samples);
        spreadModGlide[(size_t) osc].advance (samples);
        still = still && ! oscPanModGlide[(size_t) osc].moving() && ! spreadModGlide[(size_t) osc].moving();
    }
    if (pansMoving)
        updatePans();
    if (drivesMoving)
        applyDrives();
    if (still)
        glidesMoving = pansMoving = drivesMoving = false;
}

// Each oscillator's unison pans (and the sub's), from the glided knobs and routes.
void Voice::updatePans()
{
    const auto panMod = panModGlide.value;

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

    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (osc < 3 || numOscUnison[osc] > 0)
            computePans (params.oscillators[osc], numOscUnison[osc], oscPanModGlide[(size_t) osc].value,
                         spreadModGlide[(size_t) osc].value, panGainL[osc], panGainR[osc]);

    computePans (VoiceParams::OscParams {}, 1, 0.0f, 0.0f, &panGainSubOscL, &panGainSubOscR);

    // Each wavetable unison voice's blend gain and pan.
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        oscBank[osc].setWeights (unisonGains[osc], panGainL[osc], panGainR[osc], numOscUnison[osc]);
}

// The filters' drives, from the glided DRIVE knobs and routes.
void Voice::applyDrives()
{
    const auto driveAmount1 = juce::jlimit (1.0f, 10.0f, params.filter1.drive + driveModGlide[0].value * driveRange);
    const auto driveAmount2 = juce::jlimit (1.0f, 10.0f, params.filter2.drive + driveModGlide[1].value * driveRange);
    // 303 Acid and Moog Drive take the drive inside (into their feedback
    // loop's input); every other model is driven by a tanh in front.
    const auto inside1 = FilterType::drivesInside (params.filter1.type);
    const auto inside2 = FilterType::drivesInside (params.filter2.type);
    // A stereo route into a drive: the left side gets more, the right less (2026-10-30).
    const auto stereoDrive = [this] (float amount, int index)
    {
        const auto shift = stereoRouted[(size_t) index] ? blockMod ((Mod::Destination) ((int) Mod::Destination::Count + index)) * driveRange : 0.0f;
        return DriveAmounts { juce::jlimit (1.0f, 10.0f, amount + shift), juce::jlimit (1.0f, 10.0f, amount - shift) };
    };
    const auto drives1 = stereoDrive (driveAmount1, Mod::StereoIndex::drive1);
    const auto drives2 = stereoDrive (driveAmount2, Mod::StereoIndex::drive2);
    for (auto* filter : { &filter1L, &bothFilter1L })
        filter->setDrive (inside1 ? drives1.l : 1.0f);
    for (auto* filter : { &filter1R, &bothFilter1R })
        filter->setDrive (inside1 ? drives1.r : 1.0f);
    for (auto* filter : { &filter2L, &bothFilter2L })
        filter->setDrive (inside2 ? drives2.l : 1.0f);
    for (auto* filter : { &filter2R, &bothFilter2R })
        filter->setDrive (inside2 ? drives2.r : 1.0f);
    drive1Now = inside1 ? DriveAmounts {} : drives1;
    drive2Now = inside2 ? DriveAmounts {} : drives2;
}

// One sub-block's glide and drift over its samples (the drift's random walk
// steps here, so it stays in step whether the voice renders in full or only
// its body's tail).
double Voice::advanceGlideAndDrift (float driftMod, int samples)
{
    const auto glideStep = samples == 16 || glideSamples <= 0.0f ? glideCoeff
                                                                 : 1.0f - std::exp (-(float) samples / glideSamples);
    currentFrequency += (baseFrequency - currentFrequency) * (double) glideStep;

    auto driftFactor = 1.0;
    const auto drift = juce::jlimit (0.0f, 1.0f, params.drift + driftMod);

    if (drift > 0.0f)
    {
        // The walk was tuned as 16 samples at 48 kHz a step: a new target
        // about 15 times a second, a slew of about 0.17 s. Other rates, the
        // oversampled voices and shorter steps scale the chance and the slew
        // by the time the step covers (exactly the old numbers at 48 kHz).
        const auto steps = (double) samples * 3000.0 / sampleRate;
        auto threshold = 0.995f, slew = 0.002f;
        if (steps != 1.0)
        {
            threshold = (float) (1.0 - 0.005 * steps);
            slew = (float) (1.0 - std::pow (0.998, steps));
        }
        if (driftRandom.nextFloat() > threshold)
            driftTarget = driftRandom.nextFloat() * 2.0f - 1.0f;

        driftValue += (driftTarget - driftValue) * slew;
        driftFactor = std::exp2 ((double) driftValue * (double) drift * 0.25 / 12.0);
    }

    return currentFrequency * driftFactor;
}

// The physical body as a stage among the routing cables (VoiceRouting::body).
bool Voice::bodyIsInChain() const
{
    return params.routing.on && params.routing.used[(size_t) VoiceRouting::body];
}

// A material body outside the chain: struck by the strings, ringing beside
// the filters and after the amp, as in every older patch.
// Held sleep is only for notes whose every sound dies away by itself once
// struck: physical strings with a one-off excitation (no bow, external
// drive, feedback amp or SUSTAIN), the Modal Piano, nothing else sounding
// (wavetable, sample, granular or live oscillators, noise) and no live
// input into the strings.
bool Voice::heldSleepAllowed() const
{
    using E = KarplusStrong::Excite;
    if (params.noiseEnabled && params.noiseLevel > 0.0f)
        return false;
    if (params.inputToStrings > 0.0f)
        return false;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        if (params.fmNoise[osc] > 0.0f)
            return false;
        if (! params.oscillatorEnabled[osc])
            continue;
        const auto& settings = params.oscillators[osc];
        if (! settings.stringMode || settings.liveMode || settings.granularMode || settings.sampleMode)
            return false;
        const auto excite = (E) settings.stringExcite;
        if (excite == E::Bow || excite == E::ModalBowed || excite == E::External || excite == E::Feedback || settings.stringSustain > 0.0f)
            return false;
    }
    // A route into a filter or the WEST gate could push it into
    // self-oscillation while the note sleeps (its source doesn't run then).
    const auto& filterDestinations = filterDestinationMarks();
    for (int slot = 0; slot < params.numModSlots; ++slot)
    {
        const auto d = params.modSlots[slot].destination;
        if (! params.modSlots[slot].bypass && d >= 0 && d < (int) filterDestinations.size() && filterDestinations[(size_t) d])
            return false;
    }
    return true;
}

// The destinations that move a filter or the WEST gate (held sleep). Built
// once, by the first Voice (on the message thread), not on the audio thread.
const std::vector<bool>& Voice::filterDestinationMarks()
{
    static const auto marks = []
    {
        using D = Mod::Destination;
        std::vector<bool> table ((size_t) Mod::getNumDestinations(), false);
        for (const auto d : { D::Filter1Cutoff, D::Filter1Reso, D::Filter2Cutoff, D::Filter2Reso, D::Filter1Drive, D::Filter2Drive,
                              D::Filter1Env, D::Filter2Env, D::Filter1Fm, D::Filter2Fm, D::Filter1Morph, D::Filter2Morph })
            table[(size_t) d] = true;
        const auto& list = Mod::getParamDestinations();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            const juce::String id (list[(size_t) i].id);
            const auto d = Mod::paramDestinationFor (i);
            if (d >= 0 && d < (int) table.size() && (id.startsWith ("f1_") || id.startsWith ("f2_") || id.startsWith ("west_") || id.startsWith ("filter")))
                table[(size_t) d] = true;
        }
        return table;
    }();
    return marks;
}

void Voice::rememberFilterSettings()
{
    sleptFilter1 = params.filter1;
    sleptFilter2 = params.filter2;
    sleptWest = params.west;
    sleptFilterFm[0] = params.filter1Fm;
    sleptFilterFm[1] = params.filter2Fm;
    sleptOscillators = enabledOscillators();
}

// Which oscillators are on, one bit each: one switched on (or back on) wakes
// a sleeping note, as its string may still hold what it last played.
int Voice::enabledOscillators() const
{
    auto mask = 0;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
        if (params.oscillatorEnabled[osc])
            mask |= 1 << osc;
    return mask;
}

bool Voice::filterSettingsMoved() const
{
    const auto filterMoved = [] (const VoiceParams::FilterParams& a, const VoiceParams::FilterParams& b)
    {
        return a.type != b.type || a.slope24 != b.slope24 || a.cutoffHz != b.cutoffHz || a.resonance != b.resonance
               || a.drive != b.drive || a.envAmount != b.envAmount || a.keyTrack != b.keyTrack || a.morph != b.morph;
    };
    const auto& w = params.west;
    const auto& v = sleptWest;
    return filterMoved (params.filter1, sleptFilter1) || filterMoved (params.filter2, sleptFilter2)
           || params.filter1Fm != sleptFilterFm[0] || params.filter2Fm != sleptFilterFm[1]
           || w.on != v.on || w.position != v.position || w.fold != v.fold || w.symmetry != v.symmetry || w.stages != v.stages
           || w.mode != v.mode || w.decay != v.decay || w.resonance != v.resonance || w.strike != v.strike || w.open != v.open
           || w.source != v.source;
}

bool Voice::legacyMaterialBody() const
{
    return params.resonatorOn && params.bodyType != 0 && ! bodyIsInChain();
}

// The body sits on the pitch the oscillators play, not only on the note: the
// pitch bend and the leading oscillator's fine tune and pitch modulation (a
// vibrato) move it too. A tuned body's first mode is a fraction of a hertz
// wide at long DECAY, so a bent or vibrato note used to fall off it and lose
// most of the body (HANDOFF open issue, ilana 2026-11-26: "body could be an
// issue tho so fix that"). Whole-semitone offsets (SEMI, OCT) stay out: the
// body keeps the note's register, as before.
double Voice::bodyPitchFactor (const float* modValues) const
{
    using D = Mod::Destination;
    constexpr D pitchDestinations[] { D::Osc1Pitch, D::Osc2Pitch, D::SubPitch,
                                      D::Osc4Pitch, D::Osc5Pitch, D::Osc6Pitch };
    auto semitones = bendSemitones;
    for (int osc = 0; osc < VoiceParams::numOscillators; ++osc)
    {
        const auto& settings = params.oscillators[(size_t) osc];
        if (! params.oscillatorEnabled[(size_t) osc] || settings.tuneMode == OscTuning::Fixed
            || (osc == 2 && settings.level <= 0.0f))
            continue;
        semitones += settings.cents / 100.0 + (double) modValues[(int) pitchDestinations[osc]] * 48.0;
        break;
    }
    return semitones == 0.0 ? 1.0 : std::exp2 (semitones / 12.0);
}

void Voice::tuneBody (double driftedFrequency)
{
    using D = Mod::Destination;

    if (params.resonatorOn && params.resonatorAmount + blockMod (D::ResAmount) > 0.001f)
    {
        const auto offset = juce::jlimit (-24.0f, 24.0f, params.resonatorOffset + blockMod (D::ResOffset) * Mod::Range::resOffsetSemitones);
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
            // A fixed-pitch body (the guitar) ignores KEY TRACK and OFFSET; it
            // levels itself on the note actually played.
            const auto fixedPitch = BodyTuning::shapes[(size_t) juce::jlimit (0, 4, params.bodyType - 1)].fixedPitch;
            const auto bodyNote = fixedPitch ? driftedFrequency : noteHz;
            materialBodyL.configure (params.bodyType - 1, params.bodyMaterial, params.bodySize,
                                     decay, bodyNote, params.quality, -1.0f, params.bodyLegacyLevel);
            materialBodyR.configure (params.bodyType - 1, params.bodyMaterial, params.bodySize,
                                     decay, bodyNote, params.quality, 1.0f, params.bodyLegacyLevel);
        }
    }
}

// The body's tail alone: every envelope is over, so the sources and filters
// add exact zeros (their output and the body's drive are both scaled by the
// finished amp envelope) and only the body rings. The same sums as the full
// render, without computing the zeros. AMP LEVEL modulation is read once per
// block here (per sample in the full render).
void Voice::renderBodyTail (float* left, float* right, int startSample, int numSamples, float wetGain)
{
    using D = Mod::Destination;
    for (int i = 0; i < numSamples; ++i)
    {
        if ((i & 15) == 0)
            tuneBody (advanceGlideAndDrift (blockMod (D::Drift), juce::jmin (16, numSamples - i)) * bodyPitchFactor (blockMods.data()));
        if (fadeTotal > 0)
            ampGainFade = fadeRemaining > 0 ? (float) --fadeRemaining / (float) fadeTotal : 0.0f;
        const auto wetL = materialBodyL.process (0.0f) * wetGain * ampGainFade;
        const auto wetR = materialBodyR.process (0.0f) * wetGain * ampGainFade;
        if (right != nullptr)
        {
            left[startSample + i] += wetL;
            right[startSample + i] += wetR;
        }
        else
        {
            left[startSample + i] += wetL;
            left[startSample + i] += wetR;
        }
    }
}

void Voice::updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue)
{
    using D = Mod::Destination;

    const auto velocityEnvScale = 1.0f - params.filterVelocity + params.filterVelocity * velocityLevel;

    const auto fmOctaves1 = (double) (params.filter1Fm + mods[(int) D::Filter1Fm] * Mod::Range::filterFm) * (double) previousOsc[1] * 4.0;
    const auto fmOctaves2 = (double) (params.filter2Fm + mods[(int) D::Filter2Fm] * Mod::Range::filterFm) * (double) previousOsc[1] * 4.0;

    const auto keyOctaves1 = (double) params.filter1.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves1 = (double) (params.filter1.envAmount + mods[(int) D::Filter1Env] * Mod::Range::filterEnvOctaves)
                             * (double) filterEnvValue * (double) velocityEnvScale;
    const auto cutoff1 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter1.cutoffHz
                                           * std::exp2 (keyOctaves1 + envOctaves1 + fmOctaves1
                                                        + (double) mods[(int) D::Filter1Cutoff] * (double) Mod::Range::cutoffOctaves));
    const auto reso1 = juce::jlimit (0.0f, 1.0f, params.filter1.resonance + mods[(int) D::Filter1Reso]);

    // Filter 1 left wide open (the piano presets) gets the same bypass as
    // Filter 2 below, but only while nothing moves it: a swept Filter 1
    // crossing 19 kHz would restart from rest on each crossing.
    // Macros, the wheel and per-note sources (velocity, key) are fine: they
    // cross rarely, and a crossing fades between the two.
    const auto filter1Static = [this]
    {
        if (params.filter1.envAmount != 0.0f)
            return false;
        for (int i = 0; i < params.numModSlots; ++i)
        {
            const auto& slot = params.modSlots[i];
            const auto destination = slot.destination;
            if (destination != (int) D::Filter1Cutoff && destination != (int) D::Filter1Env
                && destination != (int) D::Filter1Reso && destination != (int) D::Filter1Fm)
                continue;
            using S = Mod::Source;
            const auto source = slot.source;
            const auto userOrNote = source == S::Velocity || source == S::KeyTrack || source == S::ModWheel
                                    || source == S::Aftertouch || source == S::Expression
                                    || (source >= S::Macro1 && source <= S::Macro4)
                                    || (source >= S::Macro5 && source <= S::Macro8);
            if (! userOrNote || destination == (int) D::Filter1Fm)
                return false;
        }
        return true;
    };
    const auto open1 = ! disableOpenFilterBypass && params.filter1.type == FilterType::LowPass
                       && ! params.filter1.slope24 && cutoff1 >= openFilterHz && reso1 <= 0.3f && fmOctaves1 == 0.0
                       && ! filterStereo (0) && filter1Static();

    if (open1 != filter1Open)
    {
        // The new model starts from rest; once Filter 1 has played, the old
        // one keeps its state and fades out (postChunk).
        filter1Fade = filter1Ran ? filterFadeLength : 0;
        filter1FadeLinked = filter1Linked;
        filter1Open = open1;
        if (open1)
        {
            openFilter1L.reset();
            openFilter1R.reset();
            openCutoff1 = -1.0;
        }
        else
        {
            filter1L.reset();
            filter1R.reset();
        }
        filter1Linked = true;
    }

    if (open1 && (cutoff1 != openCutoff1 || reso1 != openReso1))
    {
        openCutoff1 = cutoff1;
        openReso1 = reso1;
        openFilter1L.set (sampleRate, cutoff1, reso1);
        openFilter1R = openFilter1L;
    }

    if (! open1 || bothRouteActive)
    {
        for (auto* filter : { &filter1L, &filter1R, &bothFilter1L, &bothFilter1R })
            filter->setType (params.filter1.type, params.filter1.slope24);

        const auto morph1 = juce::jlimit (0.0f, 1.0f, params.filter1.morph + mods[(int) D::Filter1Morph]);
        const auto coefficients1 = FilterUnit::makeCoefficients (params.filter1.type, sampleRate, cutoff1, reso1, morph1);

        if (filterStereo (0))
        {
            // A stereo route: the left cutoff (resonance, morph) moves up as the right moves down.
            const auto shift = (double) mods[(int) D::Count + Mod::StereoIndex::cutoff1] * (double) Mod::Range::cutoffOctaves;
            const auto resoShift = mods[(int) D::Count + Mod::StereoIndex::reso1];
            const auto morphShift = mods[(int) D::Count + Mod::StereoIndex::morph1];
            const auto left = FilterUnit::makeCoefficients (params.filter1.type, sampleRate,
                                                            juce::jlimit (20.0, sampleRate * 0.45, cutoff1 * std::exp2 (shift)),
                                                            juce::jlimit (0.0f, 1.0f, reso1 + resoShift), juce::jlimit (0.0f, 1.0f, morph1 + morphShift));
            const auto right = FilterUnit::makeCoefficients (params.filter1.type, sampleRate,
                                                             juce::jlimit (20.0, sampleRate * 0.45, cutoff1 * std::exp2 (-shift)),
                                                             juce::jlimit (0.0f, 1.0f, reso1 - resoShift), juce::jlimit (0.0f, 1.0f, morph1 - morphShift));

            if (filter1Linked)
            {
                filter1R = filter1L;
                filter1Linked = false;
            }

            filter1L.setCoefficients (left);
            filter1R.setCoefficients (right);
            bothFilter1L.setCoefficients (left);
            bothFilter1R.setCoefficients (right);
        }
        else
        {
            filter1L.setCoefficients (coefficients1);
            filter1R.setCoefficients (coefficients1);
            bothFilter1L.setCoefficients (coefficients1);
            bothFilter1R.setCoefficients (coefficients1);
        }
    }

    const auto keyOctaves2 = (double) params.filter2.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves2 = (double) (params.filter2.envAmount + mods[(int) D::Filter2Env] * Mod::Range::filterEnvOctaves)
                             * (double) filter2EnvValue * (double) velocityEnvScale
                             * (double) velocityScaleFor (params.filter2EnvVelocity);
    const auto cutoff2 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter2.cutoffHz
                                           * std::exp2 (keyOctaves2 + envOctaves2 + fmOctaves2
                                                        + (double) mods[(int) D::Filter2Cutoff] * (double) Mod::Range::cutoffOctaves));
    const auto reso2 = juce::jlimit (0.0f, 1.0f, params.filter2.resonance + mods[(int) D::Filter2Reso]);

    // Filter 2 is a Low Pass left wide open on 361 of the 371 factory
    // presets. While it stays there (above 19 kHz, resonance up to 0.3, no
    // audio-rate FM) a linear copy of it runs instead (OpenLowPass: the same
    // response, a third of the cost; Vital likewise skips work a stage can't
    // be heard doing). Moving between the two starts the new one from rest.
    const auto open2 = ! disableOpenFilterBypass && params.filter2.type == FilterType::LowPass
                       && ! params.filter2.slope24 && cutoff2 >= openFilterHz && reso2 <= 0.3f && fmOctaves2 == 0.0
                       && ! filterStereo (1);

    if (open2 != filter2Open)
    {
        // As Filter 1: the new model starts from rest; once Filter 2 has
        // played, the old one keeps its state and fades out over
        // filterFadeLength (processFilter2Block). It used to restart both,
        // which clicked on every crossing (bug hunt 2026-10-31).
        filter2Fade = filter2Ran ? filterFadeLength : 0;
        filter2FadeLinked = filter2Linked;
        filter2Open = open2;
        if (open2)
        {
            openFilter2L.reset();
            openFilter2R.reset();
            openCutoff2 = -1.0;
        }
        else
        {
            filter2L.reset();
            filter2R.reset();
        }
        filter2Linked = true;
    }

    if (open2)
    {
        if (cutoff2 != openCutoff2 || reso2 != openReso2)
        {
            openCutoff2 = cutoff2;
            openReso2 = reso2;
            openFilter2L.set (sampleRate, cutoff2, reso2);
            openFilter2R = openFilter2L;
        }

        if (! bothRouteActive)
            return;
    }

    for (auto* filter : { &filter2L, &filter2R, &bothFilter2L, &bothFilter2R })
        filter->setType (params.filter2.type, params.filter2.slope24);

    const auto morph2 = juce::jlimit (0.0f, 1.0f, params.filter2.morph + mods[(int) D::Filter2Morph]);
    const auto coefficients2 = FilterUnit::makeCoefficients (params.filter2.type, sampleRate, cutoff2, reso2, morph2);

    if (filterStereo (1))
    {
        const auto shift = (double) mods[(int) D::Count + Mod::StereoIndex::cutoff2] * (double) Mod::Range::cutoffOctaves;
        const auto resoShift = mods[(int) D::Count + Mod::StereoIndex::reso2];
        const auto morphShift = mods[(int) D::Count + Mod::StereoIndex::morph2];
        const auto left = FilterUnit::makeCoefficients (params.filter2.type, sampleRate,
                                                        juce::jlimit (20.0, sampleRate * 0.45, cutoff2 * std::exp2 (shift)),
                                                        juce::jlimit (0.0f, 1.0f, reso2 + resoShift), juce::jlimit (0.0f, 1.0f, morph2 + morphShift));
        const auto right = FilterUnit::makeCoefficients (params.filter2.type, sampleRate,
                                                         juce::jlimit (20.0, sampleRate * 0.45, cutoff2 * std::exp2 (-shift)),
                                                         juce::jlimit (0.0f, 1.0f, reso2 - resoShift), juce::jlimit (0.0f, 1.0f, morph2 - morphShift));

        if (filter2Linked)
        {
            filter2R = filter2L;
            filter2Linked = false;
        }

        filter2L.setCoefficients (left);
        filter2R.setCoefficients (right);
        bothFilter2L.setCoefficients (left);
        bothFilter2R.setCoefficients (right);
        return;
    }

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
    // The shared buffers cover the whole block; a render split by a MIDI
    // event starts part-way in (renderStart).
    const auto lfoSource = [this, sampleIndex] (int lfo, const float* shared)
    {
        if (params.lfos[lfo].perVoice)
            return lfoValues[lfo];

        return shared != nullptr ? shared[renderStart + sampleIndex] : 0.0f;
    };

    if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
        return lfoSource (lfo, params.lfoBuffers[lfo]);

    if (const auto lfo = Mod::lfoBIndexFor (source); lfo >= 0)
    {
        if (params.lfos[lfo].perVoice)
            return lfoValuesB[lfo];
        return params.lfoBuffersB[lfo] != nullptr ? params.lfoBuffersB[lfo][renderStart + sampleIndex] : 0.0f;
    }

    switch (source)
    {
        case Mod::Source::ModEnv:     return modValue * velocityScaleFor (params.modEnvVelocity);
        case Mod::Source::FilterEnv:  return filterValue * velocityScaleFor (params.filterVelocity);
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
        case Mod::Source::Macro5:     return params.macros[4];
        case Mod::Source::Macro6:     return params.macros[5];
        case Mod::Source::Macro7:     return params.macros[6];
        case Mod::Source::Macro8:     return params.macros[7];
        case Mod::Source::ClockSh:    return params.clockSh != nullptr ? params.clockSh[renderStart + sampleIndex] : 0.0f;
        case Mod::Source::Mseg:       return params.mseg != nullptr ? params.mseg[renderStart + sampleIndex] : 0.0f;
        case Mod::Source::Env4:       return env4Value * velocityScaleFor (params.env4Velocity);
        case Mod::Source::FilterEnv2: return filter2Value * velocityScaleFor (params.filter2EnvVelocity);
        case Mod::Source::InputEnv:   return params.inputEnv != nullptr ? params.inputEnv[renderStart + sampleIndex] : 0.0f;
        case Mod::Source::VectorX:    return params.vectorX;
        case Mod::Source::VectorY:    return params.vectorY;
        case Mod::Source::OpLfo:      return dx7Playing ? dx7Note.getLfoOutput() : 0.0f;
        case Mod::Source::OpPitchEnv: return dx7Playing ? dx7Note.getPitchShape() : 0.0f;
        case Mod::Source::None:
        case Mod::Source::Count:
        default:                      return 0.0f;
    }
}
