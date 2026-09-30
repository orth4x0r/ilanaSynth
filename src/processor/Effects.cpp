#include "ProcessorInternal.h"

void IlanaSynthAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto& ids = fxSlotIds[(size_t) (slot - 1)];
        const auto type = (int) getParam (ids.type);

        if (type == 0 || getParam (ids.bypass) > 0.5f)
        {
            fxSlotCpu[(size_t) (slot - 1)].store (0.0f);
            continue;
        }

        const auto solo = getParam (ids.solo) > 0.5f;
        const auto blend = getParam (ids.mix);
        const auto startTicks = juce::Time::getHighResolutionTicks();
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();
        const auto band = (int) getParam (ids.band);

        if (band > 0 && numChannels == 2)
        {
            processSlotBand (slot - 1, type, band, buffer, solo, blend);
        }
        else if (solo || blend < 0.999f)
        {
            if (fxScratch.getNumChannels() < numChannels || fxScratch.getNumSamples() < numSamples)
                fxScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                fxScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            processSlot (type, fxScratch);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                if (solo)
                {
                    buffer.copyFrom (channel, 0, fxScratch, channel, 0, numSamples);
                }
                else
                {
                    buffer.applyGain (channel, 0, numSamples, 1.0f - blend);
                    buffer.addFrom (channel, 0, fxScratch, channel, 0, numSamples, blend);
                }
            }
        }
        else
        {
            processSlot (type, buffer);
        }

        sanitiseBuffer (buffer);

        const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
        const auto availableSeconds = (double) numSamples / juce::jmax (1.0, currentSampleRate);
        const auto usage = (float) juce::jlimit (0.0, 1.0, elapsedSeconds / juce::jmax (1.0e-9, availableSeconds));

        auto& cpu = fxSlotCpu[(size_t) (slot - 1)];
        cpu.store (cpu.load() * 0.9f + usage * 0.1f);
    }

    sanitiseBuffer (buffer);
}

void IlanaSynthAudioProcessor::processSlotBand (int slot, int type, int band, juce::AudioBuffer<float>& buffer, bool solo, float blend)
{
    const auto numSamples = buffer.getNumSamples();
    if (fxBand.getNumChannels() < 2 || fxBand.getNumSamples() < numSamples)
        fxBand.setSize (2, numSamples, false, false, true);
    if (fxScratch.getNumChannels() < 2 || fxScratch.getNumSamples() < numSamples)
        fxScratch.setSize (2, numSamples, false, false, true);

    auto& split = fxSplit[(size_t) slot];
    const auto lowHz = getParam ("fx_split_low");
    const auto highHz = juce::jmax (lowHz * 1.5f, getParam ("fx_split_high"));
    const auto rate = juce::jmax (1.0, currentSampleRate);
    if (split.lastBand != band || split.lastLow != lowHz || split.lastHigh != highHz)
    {
        const auto fresh = split.lastBand != band;
        const auto low = juce::IIRCoefficients::makeLowPass (rate, juce::jlimit (20.0, rate * 0.45, (double) lowHz), 0.70710678);
        const auto lowCut = juce::IIRCoefficients::makeHighPass (rate, juce::jlimit (20.0, rate * 0.45, (double) lowHz), 0.70710678);
        const auto high = juce::IIRCoefficients::makeLowPass (rate, juce::jlimit (20.0, rate * 0.45, (double) highHz), 0.70710678);
        const auto highCut = juce::IIRCoefficients::makeHighPass (rate, juce::jlimit (20.0, rate * 0.45, (double) highHz), 0.70710678);
        const auto allpass = [rate] (float hz)
        {
            return juce::IIRCoefficients::makeAllPass (rate, juce::jlimit (20.0, rate * 0.45, (double) hz), 0.70710678);
        };
        for (int channel = 0; channel < 2; ++channel)
        {
            split.allLow[(size_t) channel].setCoefficients (allpass (lowHz));
            split.allHigh[(size_t) channel].setCoefficients (allpass (highHz));
            if (fresh)
            {
                split.allLow[(size_t) channel].reset();
                split.allHigh[(size_t) channel].reset();
            }
        }
        for (int stage = 0; stage < 2; ++stage)
            for (int channel = 0; channel < 2; ++channel)
            {
                // Low: below the low split. Mid: above it and below the
                // high one. High: above the high split.
                auto& a = split.lowA[(size_t) stage][(size_t) channel];
                auto& b = split.lowB[(size_t) stage][(size_t) channel];
                a.setCoefficients (band == 1 ? low : (band == 2 ? lowCut : highCut));
                b.setCoefficients (high);
                if (fresh)
                {
                    a.reset();
                    b.reset();
                }
            }
        split.lastBand = band;
        split.lastLow = lowHz;
        split.lastHigh = highHz;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    auto* bandL = fxBand.getWritePointer (0);
    auto* bandR = fxBand.getWritePointer (1);

    // The part the slot works on, taken out of the signal.
    if (band >= 4)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto part = band == 4 ? 0.5f * (left[i] + right[i]) : 0.5f * (left[i] - right[i]);
            bandL[i] = bandR[i] = part;
        }
    }
    else
    {
        fxBand.copyFrom (0, 0, buffer, 0, 0, numSamples);
        fxBand.copyFrom (1, 0, buffer, 1, 0, numSamples);
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = fxBand.getWritePointer (channel);
            for (int stage = 0; stage < 2; ++stage)
            {
                split.lowA[(size_t) stage][(size_t) channel].processSamples (data, numSamples);
                if (band == 2)
                    split.lowB[(size_t) stage][(size_t) channel].processSamples (data, numSamples);
            }
        }
    }

    // Everything else, which passes around the slot: for a frequency band,
    // the signal through the crossovers' allpasses less the band, so the
    // two sum to a flat response.
    if (band <= 3)
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            if (band != 3)
                split.allLow[(size_t) channel].processSamples (data, numSamples);
            if (band != 1)
                split.allHigh[(size_t) channel].processSamples (data, numSamples);
        }
    for (int i = 0; i < numSamples; ++i)
    {
        if (band == 4)
            left[i] -= bandL[i], right[i] -= bandL[i];
        else if (band == 5)
            left[i] -= bandL[i], right[i] += bandL[i];
        else
            left[i] -= bandL[i], right[i] -= bandR[i];
    }

    // The slot on its band, with its blend and solo as on the full signal.
    fxScratch.copyFrom (0, 0, fxBand, 0, 0, numSamples);
    fxScratch.copyFrom (1, 0, fxBand, 1, 0, numSamples);
    processSlot (type, fxScratch);
    const auto wet = solo ? 1.0f : blend, dry = solo ? 0.0f : 1.0f - blend;
    for (int channel = 0; channel < 2; ++channel)
    {
        auto* out = fxScratch.getWritePointer (channel);
        const auto* in = fxBand.getReadPointer (channel);
        for (int i = 0; i < numSamples; ++i)
            out[i] = out[i] * wet + in[i] * dry;
    }

    // Soloed, the slot's band is all that is heard.
    if (solo)
        buffer.clear();

    const auto* wetL = fxScratch.getReadPointer (0);
    const auto* wetR = fxScratch.getReadPointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        if (band == 4 || band == 5)
        {
            // Mid or side stays mono: the slot's two channels averaged.
            const auto part = 0.5f * (wetL[i] + wetR[i]);
            left[i] += part;
            right[i] += band == 4 ? part : -part;
        }
        else
        {
            left[i] += wetL[i];
            right[i] += wetR[i];
        }
    }
}

void IlanaSynthAudioProcessor::processSlot (int type, juce::AudioBuffer<float>& buffer)
{
    switch (type)
    {
        case 1:  processAmp (buffer); break;
        case 2:  processDrive (buffer); break;
        case 3:  processCrush (buffer); break;
        case 4:  processCompressor (buffer); break;
        case 5:  processComb (buffer); break;
        case 6:  processPhaser (buffer); break;
        case 7:  processChorus (buffer); break;
        case 8:  processHaas (buffer); break;
        case 9:  processDelay (buffer); break;
        case 10: processStutter (buffer); break;
        case 11: processSmear (buffer); break;
        case 12: processFreeze (buffer); break;
        case 13: processReverb (buffer); break;
        case 14: processFlanger (buffer); break;
        case 15: processDimension (buffer); break;
        case 16: processGate (buffer); break;
        case 17: processTapeStop (buffer); break;
        case 18: processTilt (buffer); break;
        case 19: processUtility (buffer); break;
        case 20: processOtt (buffer); break;
        case 21: processLimiter (buffer); break;
        case 22: processWidener (buffer); break;
        case 23: processTremolo (buffer); break;
        case 24: processFreqShift (buffer); break;
        case 25: processRingMod (buffer); break;
        case 26: processOctaver (buffer); break;
        case 27: processVowel (buffer); break;
        case 28: processFeedback (buffer); break;
        case 29: processEq (buffer); break;
        case 30: processAirwindows (buffer); break;
        case 31: processVocoder (buffer); break;
        default: break;
    }
}

void IlanaSynthAudioProcessor::processDrive (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_drive_on") > 0.5f)
    {
        const auto amount = juce::jlimit (1.0f, 20.0f, getParam ("fx_drive_amount") + getFxMod (Mod::Destination::FxDriveAmount, 19.0f));
        const auto mix = getParam ("fx_drive_mix");

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto driven = std::tanh (data[i] * amount);
                data[i] = data[i] + (driven - data[i]) * mix;
            }
        }
    }

    if (getParam ("fx_fold") > 0.001f)
    {
        const auto amount = getParam ("fx_fold");
        const auto gain = 1.0f + amount * 5.0f;
        const auto compensation = 1.0f / (1.0f + amount * 1.5f);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
                data[i] = foldTriangle (data[i] * gain) * compensation;
        }
    }
}

void IlanaSynthAudioProcessor::processCrush (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_crush_on") > 0.5f)
    {
        const auto bits = juce::jlimit (1.0f, 16.0f, getParam ("fx_crush_bits"));
        const auto downsample = juce::jlimit (1, 64, (int) getParam ("fx_crush_down"));
        const auto mix = juce::jlimit (0.0f, 1.0f, getParam ("fx_crush_mix") + getFxMod (Mod::Destination::FxCrushMix, 1.0f));
        const auto levels = std::pow (2.0f, bits) - 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (crushCounter == 0)
            {
                for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
                {
                    const auto value = buffer.getSample (channel, i);
                    crushHold[channel] = std::round (value * levels) / levels;
                }
            }

            crushCounter = (crushCounter + 1) % downsample;

            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                const auto value = buffer.getSample (channel, i);
                buffer.setSample (channel, i, value + (crushHold[channel] - value) * mix);
            }
        }
    }

}

void IlanaSynthAudioProcessor::processComb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_comb_on") > 0.5f)
    {
        const auto frequency = juce::jlimit (20.0f, 2000.0f, getParam ("fx_comb_freq") * std::exp2 (getFxMod (Mod::Destination::FxCombFreq, 4.0f)));
        const auto feedback = juce::jlimit (0.0f, 0.97f, getParam ("fx_comb_feedback"));
        const auto mix = getParam ("fx_comb_mix");
        const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.149),
                                                (float) (currentSampleRate / frequency));

        combLine.setDelay (delaySamples);

        for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto delayed = combLine.popSample (channel);
                const auto input = data[i];

                combLine.pushSample (channel, input + delayed * feedback);
                data[i] = input + delayed * mix;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processPhaser (juce::AudioBuffer<float>& buffer)
{
    if (getParam ("fx_phaser_on") > 0.5f)
    {
        phaser.setRate (juce::jlimit (0.05f, 8.0f, getParam ("fx_phaser_rate") * std::exp2 (getFxMod (Mod::Destination::FxPhaserRate, 4.0f))));
        phaser.setDepth (getParam ("fx_phaser_depth"));
        phaser.setCentreFrequency (800.0f);
        phaser.setFeedback (getParam ("fx_phaser_feedback"));
        phaser.setMix (getParam ("fx_phaser_mix"));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        phaser.process (context);
    }

}

void IlanaSynthAudioProcessor::processChorus (juce::AudioBuffer<float>& buffer)
{
    if (getParam ("fx_chorus_on") > 0.5f)
    {
        chorus.setRate (getParam ("fx_chorus_rate"));
        chorus.setDepth (juce::jlimit (0.0f, 1.0f, getParam ("fx_chorus_depth") + getFxMod (Mod::Destination::FxChorusDepth, 1.0f)));
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (getParam ("fx_chorus_mix"));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        chorus.process (context);
    }

}

void IlanaSynthAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_delay_on") > 0.5f)
    {
        auto timeMs = getParam ("fx_delay_time");
        auto timeMsR = getParam ("fx_delay_time_r");

        if (getParam ("fx_delay_sync") > 0.5f)
        {
            const auto beats = getSyncDivisionBeats ((int) getParam ("fx_delay_div"));
            timeMs = (float) ((60.0 / currentBpm.load()) * beats * 1000.0);
            timeMsR = timeMs;
        }

        float delaySamples[2];
        float baseDelaySamples[2];
        baseDelaySamples[0] = juce::jlimit (1.0f, (float) (currentSampleRate * 1.9),
                                            (float) (timeMs * 0.001 * currentSampleRate));
        baseDelaySamples[1] = juce::jlimit (1.0f, (float) (currentSampleRate * 1.9),
                                            (float) (timeMsR * 0.001 * currentSampleRate));
        delaySamples[0] = baseDelaySamples[0];
        delaySamples[1] = baseDelaySamples[1];
        const auto feedback = juce::jlimit (0.0f, 0.95f, getParam ("fx_delay_feedback") + getFxMod (Mod::Destination::FxDelayFeedback, 0.9f));
        const auto mix = juce::jlimit (0.0f, 1.0f, getParam ("fx_delay_mix") + getFxMod (Mod::Destination::FxDelayMix, 1.0f));
        const auto damping = getParam ("fx_delay_damping");
        const auto pingPong = getParam ("fx_delay_pingpong") > 0.5f;
        const auto duck = getParam ("fx_delay_duck");
        const auto dampCoeff = 1.0f - juce::jlimit (0.0f, 0.95f, damping) * 0.92f;
        const auto channels = juce::jmin (2, numChannels);

        const auto wowAmount = getParam ("fx_delay_wow");

        const auto pitchSemitones = getParam ("fx_delay_pitch");
        const auto shiftActive = std::abs (pitchSemitones) > 0.01f;
        const auto shiftRatio = std::exp2 ((double) pitchSemitones / 12.0);

        if (shiftActive)
            for (int channel = 0; channel < 2; ++channel)
                tapeShift[channel].setRatio (shiftRatio);

        const auto duckCoefficient = (float) std::exp (-1.0 / (0.05 * currentSampleRate));

        static constexpr int maxTaps = 4;
        static constexpr float tapTimes[6][maxTaps] = {
            { 0.25f, 0.5f, 0.0f, 0.0f },
            { 0.25f, 0.375f, 0.75f, 0.0f },
            { 0.375f, 0.75f, 0.0f, 0.0f },
            { 1.0f, 2.0f, 3.0f, 0.0f },
            { 0.125f, 0.25f, 0.375f, 0.5f },
            { 1.5f, 1.0f, 0.5f, 0.0f }
        };
        static constexpr float tapGains[6][maxTaps] = {
            { 0.7f, 0.5f, 0.0f, 0.0f },
            { 0.6f, 0.5f, 0.4f, 0.0f },
            { 0.6f, 0.45f, 0.0f, 0.0f },
            { 0.5f, 0.35f, 0.25f, 0.0f },
            { 0.6f, 0.55f, 0.5f, 0.45f },
            { 0.4f, 0.5f, 0.6f, 0.0f }
        };

        const auto tapsOn = getParam ("fx_taps_on") > 0.5f;
        const auto tapPattern = juce::jlimit (0, 6, (int) getParam ("fx_taps_pattern"));
        const auto tapMix = getParam ("fx_taps_mix");
        float customTapGains[16] {};

        if (tapsOn && tapPattern == 6)
            for (int step = 0; step < 16; ++step)
                customTapGains[step] = getParam (tapStepIds[(size_t) step]);

        for (int i = 0; i < numSamples; ++i)
        {
            if (wowAmount > 0.001f)
            {
                wowPhase += 0.6 / currentSampleRate;
                wowPhase -= std::floor (wowPhase);

                const auto wowScale = 1.0f + wowAmount * 0.01f
                                                * (float) std::sin (juce::MathConstants<double>::twoPi * wowPhase);

                for (int channel = 0; channel < channels; ++channel)
                    delaySamples[channel] = baseDelaySamples[channel] * wowScale;
            }

            float delayed[2] {};
            float input[2] {};
            auto dryLevel = 0.0f;

            for (int channel = 0; channel < channels; ++channel)
            {
                input[channel] = buffer.getSample (channel, i);
                dryLevel = juce::jmax (dryLevel, std::abs (input[channel]));
                delayed[channel] = delayLine.popSample (channel, delaySamples[channel], true);
            }

            duckEnvelope = dryLevel + (duckEnvelope - dryLevel) * duckCoefficient;
            const auto duckGain = 1.0f - duck * juce::jlimit (0.0f, 1.0f, duckEnvelope * 6.0f);

            for (int channel = 0; channel < channels; ++channel)
            {
                auto feedbackSource = (pingPong && channels > 1) ? delayed[1 - channel] : delayed[channel];

                tapeShift[channel].push (feedbackSource);

                if (shiftActive)
                    feedbackSource = tapeShift[channel].process();

                delayDampState[channel] += (feedbackSource - delayDampState[channel]) * dampCoeff;
                delayLine.pushSample (channel, input[channel] + delayDampState[channel] * feedback);
                buffer.setSample (channel, i, input[channel] + (delayed[channel] * duckGain - input[channel]) * mix);
            }

            if (tapsOn)
            {
                for (int channel = 0; channel < channels; ++channel)
                {
                    auto sum = 0.0f;

                    if (tapPattern == 6)
                    {
                        for (int step = 0; step < 16; ++step)
                        {
                            if (customTapGains[step] < 0.001f)
                                continue;

                            const auto tapDelay = juce::jlimit (1.0f, (float) (currentSampleRate * 1.95),
                                                                delaySamples[channel] * (float) (step + 1) * 0.25f);
                            sum += delayLine.popSample (channel, tapDelay, false) * customTapGains[step];
                        }
                    }
                    else
                    {
                        for (int tap = 0; tap < maxTaps; ++tap)
                        {
                            const auto time = tapTimes[tapPattern][tap];

                            if (time <= 0.0f)
                                continue;

                            const auto tapDelay = juce::jlimit (1.0f, (float) (currentSampleRate * 1.95),
                                                                delaySamples[channel] * time);
                            sum += delayLine.popSample (channel, tapDelay, false) * tapGains[tapPattern][tap];
                        }
                    }

                    buffer.addSample (channel, i, sum * tapMix);
                }
            }
        }
    }

}

void IlanaSynthAudioProcessor::processStutter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto stutterOn = getParam ("fx_stutter_on") > 0.5f;

    if (stutterOn && ! stutterWasOn && stutterBuffer.getNumSamples() > 0)
    {
        const auto beats = getSyncDivisionBeats ((int) getParam ("fx_stutter_div"));
        const auto length = (int) ((60.0 / juce::jmax (20.0, currentBpm.load())) * beats * currentSampleRate);

        stutterLength = juce::jlimit (16, juce::jmax (16, stutterBuffer.getNumSamples()), length);
        stutterWrite = 0;
        stutterRead = 0;
        stutterPosition = 0.0;
        stutterRecording = true;
    }
    else if (! stutterOn && stutterWasOn)
    {
        stutterRecording = false;
    }

    stutterWasOn = stutterOn;

    if (stutterOn && stutterLength > 0 && stutterLength <= stutterBuffer.getNumSamples())
    {
        const auto mix = getParam ("fx_stutter_mix");
        const auto reverse = getParam ("fx_stutter_reverse") > 0.5f;
        const auto channels = juce::jmin (2, numChannels);
        stutterRate = (float) std::exp2 (getParam ("fx_stutter_pitch") / 12.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            if (stutterRecording)
            {
                for (int channel = 0; channel < channels; ++channel)
                    stutterBuffer.setSample (channel, stutterWrite, buffer.getSample (channel, i));

                if (++stutterWrite >= stutterLength)
                {
                    stutterWrite = 0;
                    stutterRead = 0;
                    stutterPosition = 0.0;
                    stutterRecording = false;
                }
            }
            else
            {
                const auto readPosition = reverse
                                              ? (double) (stutterLength - 1) - stutterPosition
                                              : stutterPosition;
                const auto index = juce::jlimit (0, stutterLength - 1, (int) readPosition);
                const auto next = (index + 1) % stutterLength;
                const auto frac = (float) (readPosition - (double) index);

                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto value = buffer.getSample (channel, i);
                    const auto held = stutterBuffer.getSample (channel, index)
                                      + (stutterBuffer.getSample (channel, next)
                                         - stutterBuffer.getSample (channel, index)) * frac;
                    buffer.setSample (channel, i, value + (held - value) * mix);
                }

                stutterPosition += (double) stutterRate;

                if (stutterPosition >= (double) stutterLength)
                    stutterPosition -= (double) stutterLength;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processSmear (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto smearOn = getParam ("fx_smear_on") > 0.5f;
    const auto smearMix = juce::jlimit (0.0f, 1.0f, getParam ("fx_smear_mix") + getFxMod (Mod::Destination::FxSmearMix, 1.0f));
    const auto smearChannels = juce::jmin (2, numChannels);

    for (int channel = 0; channel < smearChannels; ++channel)
    {
        smear[channel].setParams (getParam ("fx_smear_size"), getParam ("fx_smear_density"));

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            smear[channel].push (data[i]);

            if (smearOn && smearMix > 0.001f)
            {
                const auto wet = smear[channel].process();
                data[i] = data[i] * (1.0f - smearMix) + wet * smearMix;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processFreeze (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto freezeOn = getParam ("fx_freeze_on") > 0.5f;
    const auto freezeMix = juce::jlimit (0.0f, 1.0f, getParam ("fx_freeze_mix") + getFxMod (Mod::Destination::FxFreezeMix, 1.0f));

    for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        freeze[channel].process (buffer.getWritePointer (channel), numSamples, freezeOn,
                                 freezeOn ? freezeMix : 0.0f);

}

void IlanaSynthAudioProcessor::processReverb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_reverb_on") > 0.5f)
    {
        const auto type = juce::jlimit (0, 6, (int) getParam ("fx_reverb_type"));

        if (type == 6 && reverbIrLoaded.load())
        {
            if (reverbScratch.getNumChannels() < numChannels || reverbScratch.getNumSamples() < numSamples)
                reverbScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                reverbScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            juce::dsp::AudioBlock<float> block (reverbScratch);
            juce::dsp::ProcessContextReplacing<float> context (block);
            convolution.process (context);

            const auto wet = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));

            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addFrom (channel, 0, reverbScratch, channel, 0, numSamples, wet);

            return;
        }

        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_size") + getFxMod (Mod::Destination::FxReverbSize, 1.0f));
        reverbParams.damping = getParam ("fx_reverb_damping");
        reverbParams.width = getParam ("fx_reverb_width");
        reverbParams.wetLevel = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));
        reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
        reverbParams.freezeMode = 0.0f;

        switch (type)
        {
            case 1: // Hall: bigger, smoother
                reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, reverbParams.roomSize * 0.5f + 0.5f);
                reverbParams.damping = reverbParams.damping * 0.8f;
                break;

            case 2: // Plate: bright and wide
                reverbParams.damping = reverbParams.damping * 0.35f;
                reverbParams.width = 1.0f;
                break;

            case 4: // Spring: bright with a comb-y spring tank in front
                reverbParams.damping = reverbParams.damping * 0.25f;
                break;

            case 5: // Gated: long tank, wet-only, gated after the fact
                reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, reverbParams.roomSize * 0.4f + 0.6f);
                reverbParams.wetLevel = 1.0f;
                reverbParams.dryLevel = 0.0f;
                break;

            default:
                break;
        }

        if (type == 4)
        {
            const auto springDelay = (float) (currentSampleRate * 0.008);
            springComb.setDelay (springDelay);

            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto delayed = springComb.popSample (channel, springDelay, true);
                    springComb.pushSample (channel, data[i] + delayed * 0.72f);
                    data[i] += delayed * 0.4f;
                }
            }
        }

        reverb.setParameters (reverbParams);

        if (type == 5)
        {
            if (reverbScratch.getNumChannels() < numChannels || reverbScratch.getNumSamples() < numSamples)
                reverbScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                reverbScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            if (numChannels >= 2)
                reverb.processStereo (reverbScratch.getWritePointer (0), reverbScratch.getWritePointer (1), numSamples);
            else
                reverb.processMono (reverbScratch.getWritePointer (0), numSamples);

            const auto gateCoefficient = (float) std::exp (-1.0 / (0.06 * currentSampleRate));
            const auto gateGain = getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                auto dryLevel = 0.0f;

                for (int channel = 0; channel < numChannels; ++channel)
                    dryLevel = juce::jmax (dryLevel, std::abs (buffer.getSample (channel, i)));

                if (dryLevel > 0.02f)
                    gatedReverbEnvelope = 1.0f;
                else
                    gatedReverbEnvelope *= gateCoefficient;

                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.addSample (channel, i, reverbScratch.getSample (channel, i) * gatedReverbEnvelope * gateGain);
            }
        }
        else
        {
            if (numChannels >= 2)
                reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
            else
                reverb.processMono (buffer.getWritePointer (0), numSamples);
        }

        if (type == 3)
        {
            // Shimmer: add an octave-up granulated layer on top of the verb
            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                shimmerShift[channel].setRatio (2.0);

                for (int i = 0; i < numSamples; ++i)
                {
                    shimmerShift[channel].push (data[i]);
                    data[i] += shimmerShift[channel].process() * 0.28f;
                }
            }
        }
    }
}

void IlanaSynthAudioProcessor::processAmp (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mode = juce::jlimit (0, 2, (int) getParam ("fx_amp_mode"));
    const auto drive = getParam ("fx_amp_drive");
    const auto bass = getParam ("fx_amp_bass");
    const auto mid = getParam ("fx_amp_mid");
    const auto treble = getParam ("fx_amp_treble");
    const auto level = getParam ("fx_amp_level");

    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 250.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2200.0 / currentSampleRate);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            auto value = data[i] * drive;

            switch (mode)
            {
                case 1:  value = std::tanh (value * 1.6f); break;
                case 2:  value = value / (1.0f + std::abs (value) * 0.6f); break;
                default: value = std::tanh (value + 0.15f) - 0.1489f; break;
            }

            ampLowState[channel] += (value - ampLowState[channel]) * lowCoefficient;
            ampHighState[channel] += (value - ampHighState[channel]) * highCoefficient;

            const auto low = ampLowState[channel];
            const auto high = value - ampHighState[channel];
            const auto mids = value - low - high;

            data[i] = (low * bass + mids * mid + high * treble) * level;
        }
    }
}

void IlanaSynthAudioProcessor::processCompressor (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto threshold = juce::Decibels::decibelsToGain (getParam ("fx_comp_threshold"));
    const auto ratio = juce::jmax (1.0f, getParam ("fx_comp_ratio"));
    const auto attackMs = juce::jmax (0.1f, getParam ("fx_comp_attack"));
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_comp_release"));
    const auto attackCoefficient = (float) std::exp (-1.0 / ((double) attackMs * 0.001 * currentSampleRate));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));
    const auto makeup = juce::Decibels::decibelsToGain (getParam ("fx_comp_makeup"));
    const auto mix = getParam ("fx_comp_mix");

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        auto envelope = compEnvelope[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto magnitude = std::abs (data[i]);
            const auto coefficient = magnitude > envelope ? attackCoefficient : releaseCoefficient;
            envelope = magnitude + (envelope - magnitude) * coefficient;

            auto gain = 1.0f;

            if (envelope > threshold)
                gain = std::pow (envelope / threshold, 1.0f / ratio - 1.0f);

            const auto processed = data[i] * gain * makeup;
            data[i] = data[i] + (processed - data[i]) * mix;
        }

        compEnvelope[channel] = envelope;
    }
}

void IlanaSynthAudioProcessor::processHaas (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());

    if (numChannels < 2)
        return;

    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.055),
                                            (float) (getParam ("fx_haas_delay") * 0.001 * currentSampleRate));
    const auto mix = getParam ("fx_haas_mix");

    haasLine.setDelay (delaySamples);

    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto input = right[i];
        haasLine.pushSample (1, input);
        const auto delayed = haasLine.popSample (1);
        right[i] = input + (delayed - input) * mix;
    }
}

void IlanaSynthAudioProcessor::processFlanger (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 8.0f, getParam ("fx_flanger_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_flanger_depth"));
    const auto feedback = juce::jlimit (0.0f, 0.9f, getParam ("fx_flanger_feedback"));
    const auto mix = getParam ("fx_flanger_mix");
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto base = (float) (currentSampleRate * 0.0025);
            const auto modulated = base * (1.0f + depth * 0.8f
                                                     * (float) std::sin (juce::MathConstants<double>::twoPi * flangerPhase
                                                                         + channel * 0.6));
            const auto delayed = flangerLine.popSample (channel, modulated, true);
            flangerLine.pushSample (channel, data[i] + delayed * feedback);
            data[i] = data[i] + (delayed - data[i]) * mix;
        }

        flangerPhase += increment;

        if (flangerPhase >= 1.0)
            flangerPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processDimension (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 4.0f, getParam ("fx_dim_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_dim_depth"));
    const auto mix = getParam ("fx_dim_mix");
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto base = (float) (currentSampleRate * (channel == 0 ? 0.012 : 0.019));
            const auto modulated = base * (1.0f + depth * 0.25f
                                                     * (float) std::sin (juce::MathConstants<double>::twoPi * dimPhase
                                                                         + channel * 2.1));
            const auto delayed = dimLine.popSample (channel, modulated, true);
            dimLine.pushSample (channel, data[i]);
            data[i] = data[i] + (delayed - data[i]) * mix * 0.8f;
        }

        dimPhase += increment;

        if (dimPhase >= 1.0)
            dimPhase -= 1.0;
    }
}

float IlanaSynthAudioProcessor::gatePatternLevel (int pattern, int step)
{
    static constexpr juce::uint16 gatePatterns[8] = {
        0xFFFF, 0xAAAA, 0x9249, 0xEEEE, 0x0000, 0x3333, 0xF0F0, 0x0F0F
    };

    return (gatePatterns[juce::jlimit (0, 7, pattern)] & (1u << (step & 15))) != 0 ? 1.0f : 0.0f;
}

// Trance gate. DIV is the length of one step; STEPS the pattern length; the
// pattern is one of the built-ins or Custom (16 drawable step levels).
// Locked to the host's beat position while it plays.
void IlanaSynthAudioProcessor::processGate (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    // M7.1: with Euclid on and aimed at the Trance Gate, the gate plays the
    // Euclid rhythm at Euclid's rate and length.
    const auto euclid = getParam ("euc_on") > 0.5f && (int) getParam ("euc_target") == 2;
    const auto stepBeats = juce::jmax (0.001, getSyncDivisionBeats ((int) getParam (euclid ? "euc_div" : "fx_gate_div")));
    const auto stepsPerSample = (currentBpm.load() / 60.0) / stepBeats / currentSampleRate;
    const auto pattern = juce::jlimit (0, 8, (int) getParam ("fx_gate_pattern"));
    const auto steps = euclid ? juce::jlimit (2, 32, (int) getParam ("euc_steps"))
                              : juce::jlimit (2, 16, (int) getParam ("fx_gate_steps"));
    const auto euclidHits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto euclidRotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto swing = (double) juce::jlimit (0.0f, 0.5f, getParam ("fx_gate_swing"));
    const auto smooth = juce::jlimit (0.0f, 1.0f, getParam ("fx_gate_smooth"));
    const auto mix = getParam ("fx_gate_mix");

    // SMOOTH sets the edge times: quick clicks-free edges up to soft swells.
    const auto attackCoeff = 1.0f - std::exp (-1.0f / ((0.0005f + smooth * 0.03f) * (float) currentSampleRate));
    const auto releaseCoeff = 1.0f - std::exp (-1.0f / ((0.001f + smooth * 0.12f) * (float) currentSampleRate));

    std::array<float, 16> levels {};

    for (int step = 0; step < 16; ++step)
        levels[(size_t) step] = pattern == 8 ? getParam (gateStepIds[(size_t) step])
                                             : gatePatternLevel (pattern, step);

    // Position in steps: from the host while playing, else free-running.
    auto position = hostPlaying.load() ? hostPpq.load() / stepBeats : gatePhase;

    for (int i = 0; i < numSamples; ++i)
    {
        // Swing pushes every second step later within its pair.
        // (floor-based, so hosts that report negative pre-roll positions work)
        const auto pairStart = 2.0 * std::floor (position * 0.5);
        const auto pair = position - pairStart;
        const auto stepCount = (long long) pairStart + (pair < 1.0 + swing ? 0 : 1);
        const auto step = (int) (((stepCount % steps) + steps) % steps);

        if (step != gateLastStep)
        {
            // New cycle: re-roll Random, advance Build/Break once per cycle.
            if (step == 0)
            {
                gateRandomMask = (juce::uint16) lfoRandom.nextInt (0x10000);
                gateCycleCount = (gateCycleCount + 1) % 32;
            }

            gateLastStep = step;
        }

        auto level = euclid ? (euclidHit (step, euclidHits, steps, euclidRotate) ? 1.0f : 0.0f)
                            : levels[(size_t) (step & 15)];

        if (! euclid && pattern == 4)
            level = (gateRandomMask & (1u << step)) != 0 ? 1.0f : 0.0f;
        else if (! euclid && pattern == 6)
            level = step <= gateCycleCount % steps ? 1.0f : 0.0f;
        else if (! euclid && pattern == 7)
            level = step >= gateCycleCount % steps ? 1.0f : 0.0f;

        gateEnvelope += (level - gateEnvelope) * (level > gateEnvelope ? attackCoeff : releaseCoeff);
        const auto applied = 1.0f - (1.0f - gateEnvelope) * mix;

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer (channel)[i] *= applied;

        position += stepsPerSample;
    }

    gatePhase = std::fmod (position, 64.0);
    gateDisplayStep = gateLastStep;

    if (euclid)
        euclidDisplayStep.store (gateLastStep);
}

void IlanaSynthAudioProcessor::processTapeStop (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto size = tapeStopBuffer.getNumSamples();

    if (size < 16)
        return;

    const auto time = juce::jmax (0.05f, getParam ("fx_tape_stop_time"));
    const auto mix = getParam ("fx_tape_stop_mix");
    const auto target = getParam ("fx_tape_stop_trigger") > 0.5f ? 0.0f : 1.0f;
    const auto rateStep = 1.0f / (time * (float) currentSampleRate);
    const auto fadeStep = 1.0f / (0.03f * (float) currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        if (tapeStopRate < target)
            tapeStopRate = juce::jmin (target, tapeStopRate + rateStep);
        else
            tapeStopRate = juce::jmax (target, tapeStopRate - rateStep);

        // Slowing down leaves the tape behind the input. Once it is back up
        // to speed, crossfade to the live signal and re-lock the read head,
        // or that lag would stay as permanent latency.
        auto lag = (double) tapeStopWrite - tapeStopRead;

        if (lag < 0.0)
            lag += (double) size;

        const auto catchingUp = target >= 1.0f && tapeStopRate >= 1.0f && lag > 0.5;

        if (catchingUp)
            tapeStopLagFade = juce::jmax (0.0f, tapeStopLagFade - fadeStep);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            tapeStopBuffer.setSample (channel, tapeStopWrite, data[i]);

            const auto index = (int) tapeStopRead;
            const auto next = (index + 1) % size;
            const auto frac = (float) (tapeStopRead - (double) index);
            auto wet = tapeStopBuffer.getSample (channel, index)
                       + (tapeStopBuffer.getSample (channel, next) - tapeStopBuffer.getSample (channel, index)) * frac;

            if (catchingUp)
                wet = data[i] + (wet - data[i]) * tapeStopLagFade;

            data[i] = data[i] + (wet - data[i]) * mix;
        }

        if (catchingUp && tapeStopLagFade <= 0.0f)
        {
            tapeStopRead = (double) tapeStopWrite; // advanced below with the write head
            tapeStopLagFade = 1.0f;
        }

        tapeStopWrite = (tapeStopWrite + 1) % size;
        tapeStopRead += (double) tapeStopRate;

        while (tapeStopRead >= (double) size)
            tapeStopRead -= (double) size;
    }
}

void IlanaSynthAudioProcessor::processTilt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto tilt = juce::jlimit (-1.0f, 1.0f, getParam ("fx_tilt"));
    const auto level = juce::Decibels::decibelsToGain (getParam ("fx_tilt_level"));
    const auto lowGain = juce::Decibels::decibelsToGain (-tilt * 12.0f) * level;
    const auto highGain = juce::Decibels::decibelsToGain (tilt * 12.0f) * level;
    const auto coefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 700.0 / currentSampleRate);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            tiltLowState[channel] += (data[i] - tiltLowState[channel]) * coefficient;
            const auto low = tiltLowState[channel];
            const auto high = data[i] - low;
            data[i] = low * lowGain + high * highGain;
        }
    }
}

EqSettings IlanaSynthAudioProcessor::getEqSettings() const
{
    EqSettings settings;
    settings.lowFreq = getParam ("fx_eq_low_freq");
    settings.lowGain = getParam ("fx_eq_low_gain");
    settings.midFreq = getParam ("fx_eq_mid_freq");
    settings.midGain = getParam ("fx_eq_mid_gain");
    settings.midQ = getParam ("fx_eq_mid_q");
    settings.highFreq = getParam ("fx_eq_high_freq");
    settings.highGain = getParam ("fx_eq_high_gain");
    return settings;
}

void IlanaSynthAudioProcessor::processEq (juce::AudioBuffer<float>& buffer)
{
    Biquad::Coefficients coefficients[3];
    getEqSettings().makeCoefficients (currentSampleRate, coefficients);

    const auto channels = juce::jmin (2, buffer.getNumChannels());

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int band = 0; band < 3; ++band)
        {
            auto& filter = eqBands[channel][band];
            filter.setCoefficients (coefficients[band]);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = filter.process (data[i]);
        }
    }
}

// Vocoder (type 31): the buffer is the carrier; the modulator is the audio
// input (mono sum) or, without one, the module's own TALK.
void IlanaSynthAudioProcessor::processVocoder (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    Vocoder::Settings settings;
    settings.bands = (int) getParam (vocBandsRef);
    settings.width = getParam (vocWidthRef);
    settings.attackMs = getParam (vocAttackRef);
    settings.releaseMs = getParam (vocReleaseRef);
    settings.formantSemitones = getParam (vocFormantRef);
    settings.unvoiced = getParam (vocUnvoicedRef);
    settings.talkRate = getParam (vocRateRef);
    settings.levelDb = getParam (vocLevelRef);
    settings.mix = getParam (vocMixRef);

    // Auto: the input when the host gives one, else TALK. Input chosen with
    // none present gives a silent modulator.
    const auto source = (int) getParam (vocSourceRef);
    const auto haveInput = liveInputSamples > 0;
    const float* modulator = nullptr;

    if (source == 1 || (source == 0 && haveInput))
    {
        if ((int) vocoderModulator.size() < numSamples)
            vocoderModulator.resize ((size_t) numSamples);

        const auto valid = juce::jmin (numSamples, liveInputSamples);
        const auto* inL = liveDry.getReadPointer (0);
        const auto* inR = liveDry.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
            vocoderModulator[(size_t) i] = i < valid ? 0.5f * (inL[i] + inR[i]) : 0.0f;
        modulator = vocoderModulator.data();
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    vocoder.process (left, right, numSamples, modulator, settings);
}

// Airwindows (type 30): the chosen algorithm, its knobs, the module's mix.
void IlanaSynthAudioProcessor::processAirwindows (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || buffer.getNumChannels() == 0)
        return;

    std::array<float, airwindows::Module::numKnobs> knobs {};
    for (size_t knob = 0; knob < knobs.size(); ++knob)
        knobs[knob] = getParam (awKnobRefs[knob]);

    auto* left = buffer.getWritePointer (0);
    auto* right = left;

    if (buffer.getNumChannels() > 1)
        right = buffer.getWritePointer (1);
    else
    {
        if ((int) airwindowsMonoRight.size() < numSamples)
            airwindowsMonoRight.resize ((size_t) numSamples);
        std::copy (left, left + numSamples, airwindowsMonoRight.begin());
        right = airwindowsMonoRight.data();
    }

    airwindowsModule.process (left, right, numSamples, (int) getParam (awAlgoRef), knobs, getParam (awMixRef));
}

void IlanaSynthAudioProcessor::processUtility (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto gain = juce::Decibels::decibelsToGain (getParam ("fx_util_gain"));
    const auto mono = getParam ("fx_util_mono") > 0.5f;
    const auto invert = getParam ("fx_util_invert") > 0.5f;

    for (int channel = 0; channel < channels; ++channel)
        buffer.applyGain (channel, 0, numSamples, gain * (invert ? -1.0f : 1.0f));

    if (mono && channels > 1)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto average = (left[i] + right[i]) * 0.5f;
            left[i] = average;
            right[i] = average;
        }
    }
}

void IlanaSynthAudioProcessor::processOtt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 1.0f, getParam ("fx_ott_amount"));
    const auto mix = getParam ("fx_ott_mix");
    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 200.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2000.0 / currentSampleRate);
    const auto envelopeCoefficient = (float) std::exp (-1.0 / (0.01 * currentSampleRate));

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = data[i];
            ottLowState[channel][0] += (input - ottLowState[channel][0]) * lowCoefficient;
            const auto low = ottLowState[channel][0];
            const auto rest = input - low;
            ottHighState[channel][0] += (rest - ottHighState[channel][0]) * highCoefficient;
            const auto mid = ottHighState[channel][0];
            const auto high = rest - mid;

            const float bands[3] { low, mid, high };
            auto output = 0.0f;

            for (int band = 0; band < 3; ++band)
            {
                const auto magnitude = std::abs (bands[band]);
                auto& envelope = ottEnvelope[channel][band];
                envelope = magnitude + (envelope - magnitude) * envelopeCoefficient;

                const auto gain = juce::jlimit (0.25f, 4.0f, std::pow (envelope + 0.001f, -0.6f * amount));
                output += bands[band] * gain;
            }

            data[i] = input + (output - input) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processLimiter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto ceiling = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, getParam ("fx_limit_ceiling")));
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_limit_release"));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto magnitude = std::abs (data[i]);

            if (magnitude > limiterEnvelope[channel])
                limiterEnvelope[channel] = magnitude;
            else
                limiterEnvelope[channel] = magnitude + (limiterEnvelope[channel] - magnitude) * releaseCoefficient;

            const auto gain = limiterEnvelope[channel] > ceiling ? ceiling / limiterEnvelope[channel] : 1.0f;
            data[i] *= gain;
        }
    }
}

void IlanaSynthAudioProcessor::processWidener (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto width = juce::jlimit (0.0f, 2.0f, getParam ("fx_width"));
    const auto mix = getParam ("fx_width_mix");
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid = (left[i] + right[i]) * 0.5f;
        const auto side = (left[i] - right[i]) * 0.5f * width;
        const auto wetL = mid + side;
        const auto wetR = mid - side;
        left[i] = left[i] + (wetL - left[i]) * mix;
        right[i] = right[i] + (wetR - right[i]) * mix;
    }
}

void IlanaSynthAudioProcessor::processTremolo (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const auto rate = juce::jlimit (0.05f, 20.0f, getParam ("fx_trem_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_trem_depth"));
    const auto shape = juce::jlimit (0, 5, (int) getParam ("fx_trem_shape"));
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto value = lfoShapeValue (shape, tremoloPhase);
        const auto gain = 1.0f - depth * (0.5f - 0.5f * value);

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer (channel)[i] *= gain;

        tremoloPhase += increment;

        if (tremoloPhase >= 1.0)
            tremoloPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processFreqShift (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto shiftHz = juce::jlimit (-2000.0f, 2000.0f, getParam ("fx_shifter_shift"));
    const auto mix = getParam ("fx_shifter_mix");
    const auto increment = (double) shiftHz / currentSampleRate;
    constexpr auto coefficient = 0.6f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto phaseSin = (float) std::sin (juce::MathConstants<double>::twoPi * shifterPhase);
        const auto phaseCos = (float) std::cos (juce::MathConstants<double>::twoPi * shifterPhase);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto input = data[i];

            // First-order allpass quadrature approximation
            const auto quadrature = -coefficient * input + shifterDelay[channel] + coefficient * shifterAllpass[channel];
            shifterDelay[channel] = input;
            shifterAllpass[channel] = quadrature;

            const auto shifted = input * phaseCos - quadrature * phaseSin;
            data[i] = input + (shifted - input) * mix;
        }

        shifterPhase += increment;

        if (shifterPhase >= 1.0)
            shifterPhase -= 1.0;
        else if (shifterPhase < 0.0)
            shifterPhase += 1.0;
    }
}

void IlanaSynthAudioProcessor::processRingMod (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const auto frequency = juce::jlimit (1.0f, 5000.0f, getParam ("fx_ring_freq"));
    const auto mix = getParam ("fx_ring_mix");
    const auto increment = (double) frequency / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto carrier = (float) std::sin (juce::MathConstants<double>::twoPi * ringPhase);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            data[i] = data[i] + (data[i] * carrier - data[i]) * mix;
        }

        ringPhase += increment;

        if (ringPhase >= 1.0)
            ringPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processOctaver (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto mix = getParam ("fx_octaver_mix");

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        octaverShift[channel].setRatio (0.5);

        for (int i = 0; i < numSamples; ++i)
        {
            octaverShift[channel].push (data[i]);
            const auto shifted = octaverShift[channel].process();
            data[i] = data[i] + (shifted - data[i]) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processVowel (juce::AudioBuffer<float>& buffer)
{
    static const float formant1[5] { 800.0f, 400.0f, 350.0f, 450.0f, 325.0f };
    static const float formant2[5] { 1150.0f, 1600.0f, 1700.0f, 800.0f, 700.0f };

    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto morph = juce::jlimit (0.0f, 1.0f, getParam ("fx_vowel_morph"));
    const auto mix = getParam ("fx_vowel_mix");
    const auto position = morph * 4.0f;
    const auto index = juce::jlimit (0, 3, (int) position);
    const auto frac = position - (float) index;
    const auto f1 = formant1[index] + (formant1[index + 1] - formant1[index]) * frac;
    const auto f2 = formant2[index] + (formant2[index + 1] - formant2[index]) * frac;

    for (int channel = 0; channel < channels; ++channel)
    {
        vowelFilters[channel][0].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][1].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][2].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][0].setCutoff (f1);
        vowelFilters[channel][0].setResonance (0.82);
        vowelFilters[channel][1].setCutoff (f2);
        vowelFilters[channel][1].setResonance (0.82);
        vowelFilters[channel][2].setCutoff (juce::jlimit (100.0, currentSampleRate * 0.45, f2 * 2.4));
        vowelFilters[channel][2].setResonance (0.8);

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = data[i];
            const auto wet = vowelFilters[channel][0].processSample (input)
                             + vowelFilters[channel][1].processSample (input) * 0.7f
                             + vowelFilters[channel][2].processSample (input) * 0.35f;
            data[i] = input + (wet - input) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processFeedback (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 0.95f, getParam ("fx_feedback_amount"));
    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.11),
                                            (float) (getParam ("fx_feedback_delay") * 0.001 * currentSampleRate));
    const auto tone = juce::jlimit (0.0f, 1.0f, getParam ("fx_feedback_tone"));
    const auto mix = getParam ("fx_feedback_mix");
    const auto coefficient = 1.0f - tone * 0.9f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto delayed = feedbackLine.popSample (channel, delaySamples, true);

            feedbackState[channel] += (delayed - feedbackState[channel]) * coefficient;
            feedbackLine.pushSample (channel, data[i] + feedbackState[channel] * amount);
            data[i] += delayed * mix;
        }
    }
}

void IlanaSynthAudioProcessor::assignFxSlot (int slot, int type)
{
    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_slot" + juce::String (slot), (float) type);

    // Freeze and Stutter are momentary performance triggers, so they are left
    // off; the classic modules below otherwise stay silent until enabled.
    const char* enableId = nullptr;

    switch (type)
    {
        case 2:  enableId = "fx_drive_on"; break;
        case 3:  enableId = "fx_crush_on"; break;
        case 5:  enableId = "fx_comb_on"; break;
        case 6:  enableId = "fx_phaser_on"; break;
        case 7:  enableId = "fx_chorus_on"; break;
        case 9:  enableId = "fx_delay_on"; break;
        case 11: enableId = "fx_smear_on"; break;
        case 13: enableId = "fx_reverb_on"; break;
        default: break;
    }

    if (enableId != nullptr)
        set (enableId, 1.0f);

    // Airwindows: its algorithm is made here, off the audio thread.
    if (type == 30)
        airwindowsModule.preload ((int) getParam (awAlgoRef));
}

void IlanaSynthAudioProcessor::randomizeFxChain()
{
    juce::Random random;
    constexpr int typeCount = numFxTypes;

    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto type = random.nextFloat() < 0.15f ? 0 : 1 + random.nextInt (typeCount);
        assignFxSlot (slot, type);
        set ("fx_slot" + juce::String (slot) + "_bypass", 0.0f);
        set ("fx_slot" + juce::String (slot) + "_mix", 0.6f + random.nextFloat() * 0.4f);
    }

    set ("fx_drive_on", 1.0f);
    set ("fx_drive_amount", 2.0f + random.nextFloat() * 6.0f);
    set ("fx_delay_on", 1.0f);
    set ("fx_delay_sync", 1.0f);
    set ("fx_delay_div", (float) (2 + random.nextInt (4)));
    set ("fx_delay_feedback", 0.2f + random.nextFloat() * 0.4f);
    set ("fx_reverb_on", 1.0f);
    set ("fx_reverb_type", (float) random.nextInt (6));
    set ("fx_reverb_mix", 0.15f + random.nextFloat() * 0.3f);
}

bool IlanaSynthAudioProcessor::saveFxChainToFile (const juce::File& file)
{
    juce::XmlElement xml ("ilanafxchain");
    const auto tree = apvts.copyState();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        if (! id.startsWith ("fx_"))
            continue;

        if (const auto* value = apvts.getRawParameterValue (id))
        {
            auto* element = xml.createNewChildElement ("p");
            element->setAttribute ("id", id);
            element->setAttribute ("v", (double) value->load());
        }
    }

    return xml.writeTo (file);
}

void IlanaSynthAudioProcessor::loadReverbIr (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);
    reverbIrLoaded.store (true);
}

bool IlanaSynthAudioProcessor::loadFxChainFromFile (const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));

    if (xml == nullptr || ! xml->hasTagName ("ilanafxchain"))
        return false;

    for (auto* element = xml->getFirstChildElement(); element != nullptr; element = element->getNextElement())
    {
        const auto id = element->getStringAttribute ("id");

        if (id.isEmpty())
            continue;

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 ((float) element->getDoubleAttribute ("v")));
    }

    return true;
}

juce::String IlanaSynthAudioProcessor::captureFxChain()
{
    juce::String state;
    const auto tree = apvts.copyState();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        if (! id.startsWith ("fx_"))
            continue;

        if (const auto* value = apvts.getRawParameterValue (id))
            state += id + "=" + juce::String (value->load(), 6) + ";";
    }

    return state;
}

void IlanaSynthAudioProcessor::applyFxChain (const juce::String& state)
{
    const auto tokens = juce::StringArray::fromTokens (state, ";", "");

    for (const auto& token : tokens)
    {
        const auto id = token.upToFirstOccurrenceOf ("=", false, false);
        const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();

        if (id.isEmpty())
            continue;

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

void IlanaSynthAudioProcessor::switchFxChain()
{
    if (showingChainA)
    {
        chainA = captureFxChain();

        if (chainBValid)
            applyFxChain (chainB);
        else
        {
            chainB = chainA;
            chainBValid = true;
        }

        showingChainA = false;
    }
    else
    {
        chainB = captureFxChain();
        chainBValid = true;
        applyFxChain (chainA);
        showingChainA = true;
    }
}

void IlanaSynthAudioProcessor::copyFxChainToOtherBank()
{
    if (showingChainA)
    {
        chainA = captureFxChain();
        chainB = chainA;
        chainBValid = true;
    }
    else
    {
        chainB = captureFxChain();
        chainA = chainB;
    }
}
