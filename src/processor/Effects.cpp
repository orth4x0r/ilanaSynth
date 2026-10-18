#include "ProcessorInternal.h"
#include "PatchGraphRunner.h"
#include "PatchGraphState.h"
#include "../dsp/FastMath.h"

// The rack. PATCHER on: the slots as the nodes of the patcher's graph
// (processEffectsPatched). Otherwise the rack's routing (fx_routing): SERIES,
// each effect feeds the next one (as every patch before it was added);
// PARALLEL, each effect hears the rack's input and their outputs are averaged.
//
// Roadmap step 17: SERIES and PARALLEL run through the rack's compiled graph
// (getFxRackGraph), which renders exactly what the fixed loop renders
// (ILANA_GRAPH_TEST). The plan is made on the message thread for the slots as
// they are; for the moment after a slot is added, removed, bypassed or
// soloed, until the new plan is in, the fixed loop runs instead (the same
// sound). Offline the plan is made here, at once.
//
// The patcher's LFO boxes (FxLfo.h): while any of their cables moves a knob,
// the rack runs in chunks of FxLfo::chunk samples and the LFOs step between
// them (the effects read their knobs once per call). Without one, nothing
// here changes what runs.
void IlanaSynthAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    const auto* program = fxLfoProgram.load (std::memory_order_acquire);
    if (program == nullptr || program->numRoutes == 0)
    {
        if (fxLfoRunning)
        {
            for (auto& mods : fxLfoSlotMods)
                mods.count = 0;
            fxLfoRunning = false;
        }
        fxBlockLength = buffer.getNumSamples();
        processEffectsNow (buffer);
        fxClock += buffer.getNumSamples();
        return;
    }

    fxLfoRunning = true;
    const auto total = buffer.getNumSamples();
    fxBlockLength = total;
    for (int start = 0; start < total; start += FxLfo::chunk)
    {
        const auto length = juce::jmin (FxLfo::chunk, total - start);
        advanceFxLfos (*program, length);
        if (start == 0 && length == total)
        {
            processEffectsNow (buffer);
            break;
        }
        juce::AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
        processEffectsNow (part);
    }
    fxClock += total;
}

// The LFOs' values for the chunk about to run (each slot's offsets), then
// on by the chunk.
void IlanaSynthAudioProcessor::advanceFxLfos (const FxLfoProgram& program, int numSamples)
{
    const auto sampleRate = juce::jmax (1.0, currentSampleRate);
    const auto bpm = juce::jlimit (20.0, 999.0, currentBpm.load());
    const auto playing = hostPlaying.load();
    const auto ppq = hostPpq.load();
    if (ppq != fxLfoLastPpq)
    {
        fxLfoLastPpq = ppq;
        fxLfoSamplesSincePpq = 0;
    }
    const auto ppqNow = ppq + (double) fxLfoSamplesSincePpq / sampleRate * bpm / 60.0;
    fxLfoSamplesSincePpq += numSamples;

    // RETRIG: notes since the last chunk.
    const auto notes = noteOnCount.load (std::memory_order_relaxed);
    const auto newNote = notes != fxLfoLastNoteCount;
    fxLfoLastNoteCount = notes;

    std::array<float, FxLfo::maxLfos> outputs {};
    for (int l = 0; l < FxLfo::maxLfos; ++l)
    {
        if (! program.used[(size_t) l])
            continue;
        const auto& settings = program.settings[(size_t) l];
        auto& state = fxLfoStates[(size_t) l];

        double cyclesPerSample = settings.rateHz / sampleRate;
        double synced = -1.0;
        if (settings.sync)
        {
            const auto beats = FxLfo::divisionBeats (settings.division);
            cyclesPerSample = bpm / 60.0 / beats / sampleRate;
            if (playing)
            {
                const auto atEnd = (ppqNow + (double) numSamples / sampleRate * bpm / 60.0) / beats;
                synced = atEnd - std::floor (atEnd);
            }
        }

        // (The bar decides a synced LFO while the transport plays.)
        if (settings.retrig && newNote && synced < 0.0)
            state.retrigger (settings);

        const auto shapeValue = state.smooth (settings, state.shapeValue (settings), cyclesPerSample, numSamples, sampleRate);
        outputs[(size_t) l] = FxLfo::output (settings, shapeValue);
        fxLfoShownValue[(size_t) l].store (shapeValue, std::memory_order_relaxed);
        fxLfoShownPhase[(size_t) l].store ((float) std::fmod (state.phase + settings.phase, 1.0), std::memory_order_relaxed);

        state.advance (settings, cyclesPerSample, numSamples, synced);
    }

    for (auto& mods : fxLfoSlotMods)
        mods.count = 0;
    for (int r = 0; r < program.numRoutes; ++r)
    {
        const auto& route = program.routes[(size_t) r];
        auto& mods = fxLfoSlotMods[(size_t) route.slot];
        const auto offset = outputs[(size_t) route.lfo] * route.amount;
        // (Two LFOs on one knob add up.)
        auto k = 0;
        while (k < mods.count && mods.index[(size_t) k] != route.index)
            ++k;
        if (k == mods.count)
        {
            mods.index[(size_t) k] = route.index;
            mods.parameter[(size_t) k] = route.parameter;
            mods.offset[(size_t) k] = 0.0f;
            ++mods.count;
        }
        mods.offset[(size_t) k] = juce::jlimit (-1.0f, 1.0f, mods.offset[(size_t) k] + offset);
    }
}

void IlanaSynthAudioProcessor::resetFxLfos()
{
    for (int l = 0; l < FxLfo::maxLfos; ++l)
        fxLfoStates[(size_t) l].reset (l + 1);
    fxLfoLastPpq = -1.0;
    fxLfoSamplesSincePpq = 0;
    fxLfoLastNoteCount = noteOnCount.load (std::memory_order_relaxed);
}

float IlanaSynthAudioProcessor::getFxLfoValue (int id) const
{
    return juce::isPositiveAndBelow (id - 1, FxLfo::maxLfos) ? fxLfoShownValue[(size_t) (id - 1)].load (std::memory_order_relaxed) : 0.0f;
}

float IlanaSynthAudioProcessor::getFxLfoPhase (int id) const
{
    return juce::isPositiveAndBelow (id - 1, FxLfo::maxLfos) ? fxLfoShownPhase[(size_t) (id - 1)].load (std::memory_order_relaxed) : 0.0f;
}

float IlanaSynthAudioProcessor::getFxLfoOffset (int slotIndex, const juce::String& parameterId) const
{
    const auto* entry = findParam (parameterId.toRawUTF8());
    if (entry == nullptr || ! juce::isPositiveAndBelow (slotIndex, numFxSlots))
        return 0.0f;
    const auto& mods = fxLfoSlotMods[(size_t) slotIndex];
    for (int k = 0; k < mods.count; ++k)
        if (mods.index[(size_t) k] == entry->index)
            return mods.offset[(size_t) k];
    return 0.0f;
}

void IlanaSynthAudioProcessor::processEffectsNow (juce::AudioBuffer<float>& buffer)
{
    if (getParam (fxPatcherRef) > 0.5f)
        return processEffectsPatched (buffer);

    const auto signature = fxRackSignature();
    const auto* plan = fxRackRunner->activePlan();

    if (plan == nullptr || plan->tag != signature)
    {
        if (isNonRealtime())
        {
            compileFxRackPlan (signature);
            plan = fxRackRunner->activePlan();
        }
        else if (! fxRackCompileWanted.exchange (true))
        {
            triggerAsyncUpdate();
        }
    }

    if (plan != nullptr && plan->tag == signature)
        return processEffectsGraph (buffer, *fxRackRunner);

    processEffectsFixed (buffer);
}

// The fixed loop (the rack before step 17, and its stand-in while a new plan
// is on its way).
void IlanaSynthAudioProcessor::processEffectsFixed (juce::AudioBuffer<float>& buffer)
{
    if (getParam (fxRoutingRef) > 0.5f)
        return processEffectsParallel (buffer);

    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        if (! fxSlotRuns (slot - 1))
        {
            clearFxSlotMeters (slot - 1);
            continue;
        }

        runFxSlot (slot, buffer);
    }

    sanitiseBuffer (buffer);
}

// The rack sleeps through silence: once its input and output have both been
// under -110 dB for 2 s (every tail rung out, nothing the effects make on
// their own), the effects stop running and the output is silence, until the
// input is above -110 dB again. Effects that sound without input (feedback,
// self-oscillation) keep their output up, so they never sleep.
void IlanaSynthAudioProcessor::processEffectsUnlessAsleep (juce::AudioBuffer<float>& buffer)
{
    constexpr auto silence = 3.0e-6f;
    const auto peakOf = [&buffer]
    {
        auto peak = 0.0f;
        for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (buffer.getReadPointer (channel), buffer.getNumSamples());
            peak = juce::jmax (peak, -range.getStart(), range.getEnd());
        }
        return peak;
    };

    const auto inputSilent = peakOf() < silence;
    if (fxAsleep && inputSilent)
    {
        buffer.clear();
        for (int slot = 0; slot < numFxSlots; ++slot)
            clearFxSlotMeters (slot);
        return;
    }

    fxAsleep = false;
    processEffects (buffer);

    if (inputSilent && peakOf() < silence)
    {
        fxSilentSamples += buffer.getNumSamples();
        fxAsleep = fxSilentSamples >= (int) (2.0 * juce::jmax (1.0, baseSampleRate)) && ! disableFxSleep;
    }
    else
    {
        fxSilentSamples = 0;
    }
}

bool IlanaSynthAudioProcessor::isFxSlotActive (int index)
{
    const auto& ids = fxSlotIds[(size_t) index];
    return (int) getParam (ids.type) != 0 && getParam (ids.bypass) <= 0.5f;
}

// A slot to run this block: a working one, or one bypassed since it last
// ran, which runs once more with its wet part ramped out (runFxSlot) so
// bypassing is not a step.
bool IlanaSynthAudioProcessor::fxSlotRuns (int index)
{
    return isFxSlotActive (index) || fxSlotLive[(size_t) index];
}

void IlanaSynthAudioProcessor::clearFxSlotMeters (int index)
{
    fxSlotCpu[(size_t) index].store (0.0f);
    fxSlotIn[(size_t) index].store (0.0f);
    fxSlotOut[(size_t) index].store (0.0f);
}

// PARALLEL: every working slot runs on its own copy of the rack's input
// (with its own MIX, band and solo exactly as in series), and the copies are
// averaged: out = (1 / N) * sum of the N branches. Averaging, not 1 / sqrt(N),
// because each branch carries the dry signal (MIX below 100 %, a band split's
// untouched part) and EQ / drive / dynamics keep the branches correlated:
// N branches that leave the signal alone give back exactly the input, and a
// rack with one working slot sounds the same in either routing. A soloed
// slot is heard alone (with any other soloed ones, averaged among them);
// the others keep running so their tails and meters stay live.
void IlanaSynthAudioProcessor::processEffectsParallel (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    auto active = 0, soloed = 0, fading = 0;

    for (int index = 0; index < numFxSlots; ++index)
    {
        if (! isFxSlotActive (index))
        {
            clearFxSlotMeters (index);
            if (fxSlotLive[(size_t) index])
                ++fading; // (one more block, ramping out)
            continue;
        }

        ++active;
        if (getParam (fxSlotIds[(size_t) index].solo) > 0.5f)
            ++soloed;
    }

    if (active + fading > 0)
    {
        if (fxParallelIn.getNumChannels() < numChannels || fxParallelIn.getNumSamples() < numSamples)
            fxParallelIn.setSize (numChannels, numSamples, false, false, true);
        fxBranch.setSize (numChannels, numSamples, false, false, true); // (the slot works on its whole length)

        for (int channel = 0; channel < numChannels; ++channel)
            fxParallelIn.copyFrom (channel, 0, buffer, channel, 0, numSamples);
        buffer.clear();

        const auto gain = 1.0f / (float) (soloed > 0 ? soloed : active + fading);

        for (int slot = 1; slot <= numFxSlots; ++slot)
        {
            if (! fxSlotRuns (slot - 1))
                continue;

            for (int channel = 0; channel < numChannels; ++channel)
                fxBranch.copyFrom (channel, 0, fxParallelIn, channel, 0, numSamples);

            runFxSlot (slot, fxBranch);

            if (soloed == 0 || getParam (fxSlotIds[(size_t) (slot - 1)].solo) > 0.5f)
                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.addFrom (channel, 0, fxBranch, channel, 0, numSamples, gain);
        }
    }

    sanitiseBuffer (buffer);
}

// One working slot on the buffer, in place: its MIX, band and solo, the
// level meters and the CPU share.
void IlanaSynthAudioProcessor::runFxSlot (int slot, juce::AudioBuffer<float>& buffer)
{
    // A slot that is its own instance reads its own copy of its effect's
    // settings (while its card isn't the one in front) and runs on its own
    // engine (engineForSlot).
    struct OwnValues
    {
        const float* previous;
        ~OwnValues() { swapFxParamValues (previous); }
    } ownValues { swapFxParamValues (slotValues[(size_t) (slot - 1)].load (std::memory_order_acquire)) };
    // Its LFO cables' offsets, while it runs (readParam).
    struct LfoMods
    {
        const FxLfoSlotMods* previous;
        ~LfoMods() { swapFxLfoMods (previous); }
    } lfoMods { swapFxLfoMods (fxLfoSlotMods[(size_t) (slot - 1)].count > 0 ? &fxLfoSlotMods[(size_t) (slot - 1)] : nullptr) };

    {
        const auto& ids = fxSlotIds[(size_t) (slot - 1)];
        const auto type = (int) getParam (ids.type);
        // A slot bypassed since it last ran comes here once more: its wet part
        // ramps out over the block (the same ramp as MIX), alone, not soloed.
        const auto working = isFxSlotActive (slot - 1);
        const auto solo = working && getParam (ids.solo) > 0.5f;
        const auto startTicks = juce::Time::getHighResolutionTicks();
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();
        // The slot's blend eases across the block from the last one.
        auto& blendRamp = slotBlendRamps[(size_t) (slot - 1)];
        if (working && fxSlotFaded[(size_t) (slot - 1)])
        {
            // Back from a bypass: the wet part ramps in from the dry.
            fxSlotFaded[(size_t) (slot - 1)] = false;
            blendRamp.last = 0.0f;
        }
        fxSlotLive[(size_t) (slot - 1)] = working;
        if (! working)
            fxSlotFaded[(size_t) (slot - 1)] = true;
        blendRamp.begin (working ? getParam (ids.mix) : 0.0f, numSamples);
        const auto blend = blendRamp.last;
        const auto blendFrom = blendRamp.from;
        const auto band = (int) getParam (ids.band);
        const auto levelOf = [&buffer, numChannels, numSamples]
        {
            auto sum = 0.0f;
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto* data = buffer.getReadPointer (channel);
                for (int i = 0; i < numSamples; ++i)
                    sum += data[i] * data[i];
            }
            return std::sqrt (sum / (float) juce::jmax (1, numChannels * numSamples));
        };
        const auto levelIn = levelOf();

        // MIX is 0 and has been since the last block: the slot passes the
        // signal untouched, so the effect is not run (a bypassed slot is
        // skipped the same way; its memory stays as it was).
        if (blend <= 0.0f && blendFrom <= 0.0f && ! solo && band == 0)
        {
            fxSlotIn[(size_t) (slot - 1)].store (fxSlotIn[(size_t) (slot - 1)].load() * 0.8f + levelIn * 0.2f);
            fxSlotOut[(size_t) (slot - 1)].store (fxSlotOut[(size_t) (slot - 1)].load() * 0.8f + levelIn * 0.2f);
            fxSlotCpu[(size_t) (slot - 1)].store (fxSlotCpu[(size_t) (slot - 1)].load() * 0.9f);
            fxSlotPrevType[(size_t) (slot - 1)] = type;
            return;
        }

        // Skipped for longer than the longest delay memory (2 s): whatever it
        // holds is stale and would replay as new echoes, so it starts empty.
        // (A shared engine is left alone while another slot of the type runs.)
        if (fxClock - fxSlotLastRun[(size_t) (slot - 1)] > (std::int64_t) (2.0 * juce::jmax (1.0, currentSampleRate)) + fxBlockLength)
        {
            auto shared = false;
            if (slotEngines[(size_t) (slot - 1)].load (std::memory_order_acquire) == nullptr)
                for (int other = 0; other < numFxSlots; ++other)
                    if (other != slot - 1 && (int) getParam (fxSlotIds[(size_t) other].type) == type
                        && fxClock - fxSlotLastRun[(size_t) other] <= 2 * (std::int64_t) expectedBlockSize)
                        shared = true;
            if (! shared)
                engineForSlot (slot - 1).reset();
        }
        fxSlotLastRun[(size_t) (slot - 1)] = fxClock;

        // A different effect than last block (swapped or chosen while playing):
        // its wet part fades in as the old one's fades out across this block,
        // instead of one cutting to the other with a click.
        const auto previousType = fxSlotPrevType[(size_t) (slot - 1)];
        fxSlotPrevType[(size_t) (slot - 1)] = type;
        const auto typeChanged = working && previousType > 0 && previousType != type && band == 0;

        if (band > 0 && numChannels == 2)
        {
            processSlotBand (slot - 1, type, band, buffer, solo, blend, blendFrom);
        }
        else if (solo || typeChanged || blend < 0.999f || blendFrom < 0.999f)
        {
            // (Sized in prepareToPlay; the grow is for a longer block than announced.)
            if (fxScratch.getNumChannels() < numChannels || fxScratch.getNumSamples() < numSamples)
                fxScratch.setSize (numChannels, numSamples, false, false, true);
            if (typeChanged && (fxScratchOld.getNumChannels() < numChannels || fxScratchOld.getNumSamples() < numSamples))
                fxScratchOld.setSize (numChannels, numSamples, false, false, true);

            // The effect gets a view of exactly this block (it works on getNumSamples()).
            juce::AudioBuffer<float> scratch (fxScratch.getArrayOfWritePointers(), numChannels, numSamples);
            for (int channel = 0; channel < numChannels; ++channel)
                scratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            engineForSlot (slot - 1).processSlot (type, scratch);

            if (typeChanged)
            {
                juce::AudioBuffer<float> old (fxScratchOld.getArrayOfWritePointers(), numChannels, numSamples);
                for (int channel = 0; channel < numChannels; ++channel)
                    old.copyFrom (channel, 0, buffer, channel, 0, numSamples);
                engineForSlot (slot - 1).processSlot (previousType, old);
                for (int channel = 0; channel < numChannels; ++channel)
                {
                    scratch.applyGainRamp (channel, 0, numSamples, 0.0f, 1.0f);
                    scratch.addFromWithRamp (channel, 0, old.getReadPointer (channel), numSamples, 1.0f, 0.0f);
                }
                sanitiseBuffer (scratch);
            }

            for (int channel = 0; channel < numChannels; ++channel)
            {
                if (solo)
                {
                    buffer.copyFrom (channel, 0, scratch, channel, 0, numSamples);
                }
                else if (blendRamp.moving())
                {
                    buffer.applyGainRamp (channel, 0, numSamples, 1.0f - blendFrom, 1.0f - blend);
                    buffer.addFromWithRamp (channel, 0, scratch.getReadPointer (channel), numSamples, blendFrom, blend);
                }
                else
                {
                    buffer.applyGain (channel, 0, numSamples, 1.0f - blend);
                    buffer.addFrom (channel, 0, scratch, channel, 0, numSamples, blend);
                }
            }
        }
        else
        {
            engineForSlot (slot - 1).processSlot (type, buffer);
        }

        // An effect whose output is not finite has latched: it starts empty.
        if (sanitiseBuffer (buffer))
            engineForSlot (slot - 1).reset();

        {
            auto& in = fxSlotIn[(size_t) (slot - 1)];
            auto& out = fxSlotOut[(size_t) (slot - 1)];
            in.store (in.load() * 0.8f + levelIn * 0.2f);
            out.store (out.load() * 0.8f + levelOf() * 0.2f);
        }

        const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
        const auto availableSeconds = (double) numSamples / juce::jmax (1.0, currentSampleRate);
        const auto usage = (float) juce::jlimit (0.0, 1.0, elapsedSeconds / juce::jmax (1.0e-9, availableSeconds));

        auto& cpu = fxSlotCpu[(size_t) (slot - 1)];
        cpu.store (cpu.load() * 0.9f + usage * 0.1f);
    }
}

void IlanaSynthAudioProcessor::processSlotBand (int slot, int type, int band, juce::AudioBuffer<float>& buffer, bool solo, float blend, float blendFrom)
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
    juce::AudioBuffer<float> scratch (fxScratch.getArrayOfWritePointers(), 2, numSamples);
    scratch.copyFrom (0, 0, fxBand, 0, 0, numSamples);
    scratch.copyFrom (1, 0, fxBand, 1, 0, numSamples);
    engineForSlot (slot).processSlot (type, scratch);
    const auto wet = solo ? 1.0f : blend, dry = solo ? 0.0f : 1.0f - blend;
    const auto wetFrom = solo ? 1.0f : blendFrom, dryFrom = solo ? 0.0f : 1.0f - blendFrom;
    const auto wetStep = numSamples > 0 ? (wet - wetFrom) / (float) numSamples : 0.0f;
    const auto dryStep = numSamples > 0 ? (dry - dryFrom) / (float) numSamples : 0.0f;
    for (int channel = 0; channel < 2; ++channel)
    {
        auto* out = scratch.getWritePointer (channel);
        const auto* in = fxBand.getReadPointer (channel);
        for (int i = 0; i < numSamples; ++i)
            out[i] = out[i] * (wetFrom + wetStep * (float) i) + in[i] * (dryFrom + dryStep * (float) i);
    }

    // Soloed, the slot's band is all that is heard.
    if (solo)
        buffer.clear();

    const auto* wetL = scratch.getReadPointer (0);
    const auto* wetR = scratch.getReadPointer (1);
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

void IlanaSynthAudioProcessor::FxEngine::processSlot (int type, juce::AudioBuffer<float>& buffer)
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
        case 42: processFilter (buffer); break;
        default:
            if (const auto category = airwindows::categoryForFxType (type); category >= 0)
                processAirwindowsCategory (buffer, category);
            break;
    }
}

void IlanaSynthAudioProcessor::FxEngine::processDrive (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_drive_on") > 0.5f)
    {
        auto& amountRamp = fxRamps[(size_t) FxRamp::DriveAmount];
        amountRamp.begin (getParam ("fx_drive_amount"), numSamples);
        const auto amountMod = getFxMod (Mod::Destination::FxDriveAmount, 19.0f);
        auto& mixRamp = fxRamps[(size_t) FxRamp::DriveMix];
        mixRamp.begin (getParam ("fx_drive_mix"), numSamples);
        const auto hardClip = getParam ("fx_drive_type") > 0.5f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto amount = juce::jlimit (1.0f, 20.0f, amountRamp.at (i) + amountMod);
                const auto driven = hardClip ? juce::jlimit (-1.0f, 1.0f, data[i] * amount) : std::tanh (data[i] * amount);
                data[i] = data[i] + (driven - data[i]) * mixRamp.at (i);
            }
        }
    }

    if (getParam ("fx_fold") > 0.001f)
    {
        auto& foldRamp = fxRamps[(size_t) FxRamp::FoldAmount];
        foldRamp.begin (getParam ("fx_fold"), numSamples);
        const auto sineFold = getParam ("fx_fold_type") > 0.5f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto amount = foldRamp.at (i);
                const auto gain = 1.0f + amount * 5.0f;
                const auto compensation = 1.0f / (1.0f + amount * 1.5f);
                data[i] = (sineFold ? std::sin (data[i] * gain * juce::MathConstants<float>::halfPi)
                                    : foldTriangle (data[i] * gain)) * compensation;
            }
        }
    }
}

void IlanaSynthAudioProcessor::FxEngine::processCrush (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_crush_on") > 0.5f)
    {
        const auto bits = juce::jlimit (1.0f, 16.0f, getParam ("fx_crush_bits"));
        const auto downsample = juce::jlimit (1, 64, (int) getParam ("fx_crush_down"));
        auto& mixRamp = fxRamps[(size_t) FxRamp::CrushMix];
        mixRamp.begin (getParam ("fx_crush_mix"), numSamples);
        const auto mixMod = getFxMod (Mod::Destination::FxCrushMix, 1.0f);
        const auto levels = std::pow (2.0f, bits) - 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto mix = juce::jlimit (0.0f, 1.0f, mixRamp.at (i) + mixMod);

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

void IlanaSynthAudioProcessor::FxEngine::processComb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_comb_on") > 0.5f)
    {
        const auto frequency = juce::jlimit (20.0f, 2000.0f, getParam ("fx_comb_freq") * std::exp2 (getFxMod (Mod::Destination::FxCombFreq, 4.0f)));
        const auto feedback = juce::jlimit (0.0f, 0.97f, getParam ("fx_comb_feedback"));
        auto& mixRamp = fxRamps[(size_t) FxRamp::CombMix];
        mixRamp.begin (getParam ("fx_comb_mix"), numSamples);
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
                data[i] = input + delayed * mixRamp.at (i);
            }
        }
    }

}

void IlanaSynthAudioProcessor::FxEngine::processPhaser (juce::AudioBuffer<float>& buffer)
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

void IlanaSynthAudioProcessor::FxEngine::processChorus (juce::AudioBuffer<float>& buffer)
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

void IlanaSynthAudioProcessor::FxEngine::processDelay (juce::AudioBuffer<float>& buffer)
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
        // The delay time glides across the block from the last one (a step is a click).
        auto& timeRampL = fxRamps[(size_t) FxRamp::DelayTimeL];
        auto& timeRampR = fxRamps[(size_t) FxRamp::DelayTimeR];
        timeRampL.begin (baseDelaySamples[0], numSamples);
        timeRampR.begin (baseDelaySamples[1], numSamples);
        const auto feedback = juce::jlimit (0.0f, 0.95f, getParam ("fx_delay_feedback") + getFxMod (Mod::Destination::FxDelayFeedback, 0.9f));
        auto& mixRamp = fxRamps[(size_t) FxRamp::DelayMix];
        mixRamp.begin (getParam ("fx_delay_mix"), numSamples);
        const auto mixMod = getFxMod (Mod::Destination::FxDelayMix, 1.0f);
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
            const auto mix = juce::jlimit (0.0f, 1.0f, mixRamp.at (i) + mixMod);
            baseDelaySamples[0] = delaySamples[0] = timeRampL.at (i);
            baseDelaySamples[1] = delaySamples[1] = timeRampR.at (i);

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

void IlanaSynthAudioProcessor::FxEngine::processStutter (juce::AudioBuffer<float>& buffer)
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
        auto& mixRamp = fxRamps[(size_t) FxRamp::StutterMix];
        mixRamp.begin (getParam ("fx_stutter_mix"), numSamples);
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
                    buffer.setSample (channel, i, value + (held - value) * mixRamp.at (i));
                }

                stutterPosition += (double) stutterRate;

                if (stutterPosition >= (double) stutterLength)
                    stutterPosition -= (double) stutterLength;
            }
        }
    }

}

void IlanaSynthAudioProcessor::FxEngine::processSmear (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto smearOn = getParam ("fx_smear_on") > 0.5f;
    auto& smearRamp = fxRamps[(size_t) FxRamp::SmearMix];
    smearRamp.begin (getParam ("fx_smear_mix"), numSamples);
    const auto smearMod = getFxMod (Mod::Destination::FxSmearMix, 1.0f);
    const auto smearChannels = juce::jmin (2, numChannels);

    for (int channel = 0; channel < smearChannels; ++channel)
    {
        smear[channel].setParams (getParam ("fx_smear_size"), getParam ("fx_smear_density"));

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            smear[channel].push (data[i]);
            const auto smearMix = juce::jlimit (0.0f, 1.0f, smearRamp.at (i) + smearMod);

            if (smearOn && smearMix > 0.001f)
            {
                const auto wet = smear[channel].process();
                data[i] = data[i] * (1.0f - smearMix) + wet * smearMix;
            }
        }
    }

}

void IlanaSynthAudioProcessor::FxEngine::processFreeze (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto freezeOn = getParam ("fx_freeze_on") > 0.5f;
    const auto freezeMix = juce::jlimit (0.0f, 1.0f, getParam ("fx_freeze_mix") + getFxMod (Mod::Destination::FxFreezeMix, 1.0f));

    freeze.process (buffer.getWritePointer (0), numChannels > 1 ? buffer.getWritePointer (1) : nullptr,
                    numSamples, freezeOn, freezeOn ? freezeMix : 0.0f);

}

void IlanaSynthAudioProcessor::FxEngine::processReverb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // KEEP DRY (review 7): the reverb runs on a copy with no dry of its own,
    // and that is added to the untouched signal only while there is some
    // wet, so MIX 0 is the dry signal bit for bit.
    if (getParam ("fx_reverb_on") > 0.5f && getParam ("fx_reverb_keep_dry") > 0.5f && ! reverbWetOnly)
    {
        if (reverbDryScratch.getNumChannels() < numChannels || reverbDryScratch.getNumSamples() < numSamples)
            reverbDryScratch.setSize (numChannels, numSamples, false, false, true);
        juce::AudioBuffer<float> wet (reverbDryScratch.getArrayOfWritePointers(), numChannels, numSamples);
        for (int channel = 0; channel < numChannels; ++channel)
            wet.copyFrom (channel, 0, buffer, channel, 0, numSamples);

        reverbWetOnly = true;
        processReverb (wet);
        reverbWetOnly = false;

        // (Held a moment after MIX reaches 0, while the reverb's own wet
        // gain glides down.)
        if (getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f) > 0.0f)
            reverbKeepDryHold = juce::roundToInt (currentSampleRate * 0.05);
        else
            reverbKeepDryHold = juce::jmax (0, reverbKeepDryHold - numSamples);
        if (reverbKeepDryHold > 0)
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addFrom (channel, 0, wet, channel, 0, numSamples);
        return;
    }

    if (getParam ("fx_reverb_on") > 0.5f)
    {
        const auto type = juce::jlimit (0, 6, (int) getParam ("fx_reverb_type"));

        if (type == 6 && reverbIrLoaded.load())
        {
            if (reverbScratch.getNumChannels() < numChannels || reverbScratch.getNumSamples() < numSamples)
                reverbScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                reverbScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            // (Only this block's samples: the scratch can be longer.)
            juce::dsp::AudioBlock<float> block (reverbScratch);
            auto part = block.getSubBlock (0, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> context (part);
            convolution.process (context);

            const auto wet = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));

            if (reverbWetOnly)
                buffer.clear();
            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addFrom (channel, 0, reverbScratch, channel, 0, numSamples, wet);

            return;
        }

        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_size") + getFxMod (Mod::Destination::FxReverbSize, 1.0f));
        reverbParams.damping = getParam ("fx_reverb_damping");
        reverbParams.width = getParam ("fx_reverb_width");
        reverbParams.wetLevel = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));
        reverbParams.dryLevel = reverbWetOnly ? 0.0f : 1.0f - reverbParams.wetLevel;
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
                    buffer.setSample (channel, i, (reverbWetOnly ? 0.0f : buffer.getSample (channel, i))
                                                      + reverbScratch.getSample (channel, i) * gatedReverbEnvelope * gateGain);
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

void IlanaSynthAudioProcessor::FxEngine::processAmp (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mode = juce::jlimit (0, 2, (int) getParam ("fx_amp_mode"));
    auto& driveRamp = fxRamps[(size_t) FxRamp::AmpDrive];
    driveRamp.begin (getParam ("fx_amp_drive"), numSamples);
    const auto bass = getParam ("fx_amp_bass");
    const auto mid = getParam ("fx_amp_mid");
    const auto treble = getParam ("fx_amp_treble");
    auto& levelRamp = fxRamps[(size_t) FxRamp::AmpLevel];
    levelRamp.begin (getParam ("fx_amp_level"), numSamples);

    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 250.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2200.0 / currentSampleRate);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            auto value = data[i] * driveRamp.at (i);

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

            data[i] = (low * bass + mids * mid + high * treble) * levelRamp.at (i);
        }
    }
}

void IlanaSynthAudioProcessor::FxEngine::processCompressor (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto threshold = juce::Decibels::decibelsToGain (getParam ("fx_comp_threshold"));
    const auto ratio = juce::jmax (1.0f, getParam ("fx_comp_ratio"));
    const auto attackMs = juce::jmax (0.1f, getParam ("fx_comp_attack"));
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_comp_release"));
    const auto attackCoefficient = (float) std::exp (-1.0 / ((double) attackMs * 0.001 * currentSampleRate));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));
    auto& makeupRamp = fxRamps[(size_t) FxRamp::CompMakeup];
    makeupRamp.begin (juce::Decibels::decibelsToGain (getParam ("fx_comp_makeup")), numSamples);
    auto& mixRamp = fxRamps[(size_t) FxRamp::CompMix];
    mixRamp.begin (getParam ("fx_comp_mix"), numSamples);
    auto deepestGain = 1.0f; // for the card's meter only

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

            deepestGain = juce::jmin (deepestGain, gain);
            const auto processed = data[i] * gain * makeupRamp.at (i);
            data[i] = data[i] + (processed - data[i]) * mixRamp.at (i);
        }

        compEnvelope[channel] = envelope;
    }

    compGainReduction.store (deepestGain);
}

void IlanaSynthAudioProcessor::FxEngine::processHaas (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());

    if (numChannels < 2)
        return;

    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.055),
                                            (float) (getParam ("fx_haas_delay") * 0.001 * currentSampleRate));
    auto& mixRamp = fxRamps[(size_t) FxRamp::HaasMix];
    mixRamp.begin (getParam ("fx_haas_mix"), numSamples);
    auto& delayRamp = fxRamps[(size_t) FxRamp::HaasDelay];
    delayRamp.begin (delaySamples, numSamples);

    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        haasLine.setDelay (delayRamp.at (i));
        const auto input = right[i];
        haasLine.pushSample (1, input);
        const auto delayed = haasLine.popSample (1);
        right[i] = input + (delayed - input) * mixRamp.at (i);
    }
}

void IlanaSynthAudioProcessor::FxEngine::processFlanger (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 8.0f, getParam ("fx_flanger_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_flanger_depth"));
    const auto feedback = juce::jlimit (0.0f, 0.9f, getParam ("fx_flanger_feedback"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::FlangerMix];
    mixRamp.begin (getParam ("fx_flanger_mix"), numSamples);
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
            data[i] = data[i] + (delayed - data[i]) * mixRamp.at (i);
        }

        flangerPhase += increment;

        if (flangerPhase >= 1.0)
            flangerPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::FxEngine::processDimension (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 4.0f, getParam ("fx_dim_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_dim_depth"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::DimMix];
    mixRamp.begin (getParam ("fx_dim_mix"), numSamples);
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
            data[i] = data[i] + (delayed - data[i]) * mixRamp.at (i) * 0.8f;
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
void IlanaSynthAudioProcessor::FxEngine::processGate (juce::AudioBuffer<float>& buffer)
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
    auto& mixRamp = fxRamps[(size_t) FxRamp::GateMix];
    mixRamp.begin (getParam ("fx_gate_mix"), numSamples);

    // SMOOTH sets the edge times: quick clicks-free edges up to soft swells.
    const auto attackCoeff = 1.0f - std::exp (-1.0f / ((0.0005f + smooth * 0.03f) * (float) currentSampleRate));
    const auto releaseCoeff = 1.0f - std::exp (-1.0f / ((0.001f + smooth * 0.12f) * (float) currentSampleRate));

    std::array<float, 16> levels {};

    for (int step = 0; step < 16; ++step)
        levels[(size_t) step] = pattern == 8 ? getParam (gateStepIds[(size_t) step])
                                             : gatePatternLevel (pattern, step);

    // Position in steps: from the host while playing, else free-running. The
    // host's beat is the one at the start of the block, so a call on part of
    // it (the FX LFOs' or a loop's chunks) goes on by the samples already gone.
    auto position = gatePhase;
    if (hostPlaying.load())
    {
        const auto ppq = hostPpq.load();
        if (ppq != gateLastPpq)
        {
            gateLastPpq = ppq;
            gateSamplesSincePpq = 0;
        }
        position = ppq / stepBeats + (double) gateSamplesSincePpq * stepsPerSample;
        gateSamplesSincePpq += numSamples;
    }

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
        const auto applied = 1.0f - (1.0f - gateEnvelope) * mixRamp.at (i);

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer (channel)[i] *= applied;

        position += stepsPerSample;
    }

    // (Wrapped on a whole number of swing pairs of the pattern, else the
    // pattern would jump where it wraps.)
    gatePhase = std::fmod (position, 2.0 * (double) steps * 64.0);
    gateDisplayStep = gateLastStep;

    if (euclid)
        euclidDisplayStep.store (gateLastStep);
}

void IlanaSynthAudioProcessor::FxEngine::processTapeStop (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto size = tapeStopBuffer.getNumSamples();

    if (size < 16)
        return;

    const auto time = juce::jmax (0.05f, getParam ("fx_tape_stop_time"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::TapeStopMix];
    mixRamp.begin (getParam ("fx_tape_stop_mix"), numSamples);
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

            data[i] = data[i] + (wet - data[i]) * mixRamp.at (i);
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

void IlanaSynthAudioProcessor::FxEngine::processTilt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto tilt = juce::jlimit (-1.0f, 1.0f, getParam ("fx_tilt"));
    const auto level = juce::Decibels::decibelsToGain (getParam ("fx_tilt_level"));
    auto& lowRamp = fxRamps[(size_t) FxRamp::TiltLow];
    auto& highRamp = fxRamps[(size_t) FxRamp::TiltHigh];
    lowRamp.begin (juce::Decibels::decibelsToGain (-tilt * 12.0f) * level, numSamples);
    highRamp.begin (juce::Decibels::decibelsToGain (tilt * 12.0f) * level, numSamples);
    const auto coefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 700.0 / currentSampleRate);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            tiltLowState[channel] += (data[i] - tiltLowState[channel]) * coefficient;
            const auto low = tiltLowState[channel];
            const auto high = data[i] - low;
            data[i] = low * lowRamp.at (i) + high * highRamp.at (i);
        }
    }
}

// FX type 42, FILTER: the voice's filter models (FilterUnit) on the rack's
// two channels, driven by a tanh in front as the voice drives them (or inside,
// for the models that take DRIVE in their loop). Coefficients every 32
// samples, the cutoff glided in octaves over about 20 ms. A model that ever
// gives a non-finite or runaway sample is reset (the loop guard bounds a PATCH
// loop; this keeps one bad sample from sticking).
void IlanaSynthAudioProcessor::FxEngine::processFilter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
    const auto type = juce::jlimit (0, (int) FilterType::Count - 1, juce::roundToInt (getParam (filterTypeRef)));
    const auto slope24 = getParam (filterSlopeRef) > 0.5f;
    const auto cutoff = juce::jlimit (20.0, sampleRate * 0.45, (double) getParam (filterCutoffRef));
    const auto reso = (double) juce::jlimit (0.0f, 1.0f, getParam (filterResoRef));
    const auto drive = juce::jlimit (1.0f, 10.0f, getParam (filterDriveRef));
    const auto morph = juce::jlimit (0.0f, 1.0f, getParam (filterMorphRef));
    const auto spread = juce::jlimit (0.0f, 1.0f, getParam (filterSpreadRef));
    const auto inside = FilterType::drivesInside (type);

    // Each side's target in octaves above 20 Hz (SPREAD: half an octave either way at most).
    const float target[2] { (float) std::log2 (cutoff / 20.0) + 0.5f * spread, (float) std::log2 (cutoff / 20.0) - 0.5f * spread };
    if (! filterGlideStarted)
    {
        filterOctaves[0] = target[0];
        filterOctaves[1] = target[1];
        filterGlideStarted = true;
    }
    constexpr int step = 32;
    const auto glide = 1.0f - std::exp (-(float) step / (0.02f * (float) sampleRate));
    const auto top = (float) std::log2 (sampleRate * 0.45 / 20.0);

    for (int channel = 0; channel < 2; ++channel)
    {
        filterUnits[channel].setType (type, slope24);
        filterUnits[channel].setDrive (inside ? drive : 1.0f);
    }

    for (int start = 0; start < numSamples; start += step)
    {
        const auto count = juce::jmin (step, numSamples - start);
        for (int channel = 0; channel < 2; ++channel)
            filterOctaves[channel] += (target[channel] - filterOctaves[channel]) * glide;
        const auto same = spread <= 0.0f && std::abs (filterOctaves[0] - filterOctaves[1]) < 1.0e-4f;
        FilterUnit::Coefficients coefficients[2];
        for (int channel = 0; channel < (same ? 1 : 2); ++channel)
            coefficients[channel] = FilterUnit::makeCoefficients (type, sampleRate,
                                                                  20.0 * std::exp2 ((double) juce::jlimit (0.0f, top, filterOctaves[channel])), reso, morph);
        for (int channel = 0; channel < channels; ++channel)
        {
            auto& unit = filterUnits[channel];
            unit.setCoefficients (coefficients[same ? 0 : channel]);
            auto* data = buffer.getWritePointer (channel, start);
            for (int i = 0; i < count; ++i)
            {
                const auto in = ! inside && drive > 1.0f ? FastMath::tanh (data[i] * drive) : data[i];
                auto out = unit.process (in);
                if (! std::isfinite (out) || std::abs (out) > 64.0f)
                {
                    unit.reset();
                    out = 0.0f;
                }
                data[i] = out;
            }
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

void IlanaSynthAudioProcessor::FxEngine::processEq (juce::AudioBuffer<float>& buffer)
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
void IlanaSynthAudioProcessor::FxEngine::processVocoder (juce::AudioBuffer<float>& buffer)
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

    // For the card's band display (read only; the sound doesn't change).
    for (size_t band = 0; band < vocoderBandDisplay.size(); ++band)
        vocoderBandDisplay[band].store (vocoder.getBandLevel ((int) band), std::memory_order_relaxed);
}

// Airwindows (type 30): the chosen algorithm, its knobs, the module's mix.
void IlanaSynthAudioProcessor::FxEngine::processAirwindows (juce::AudioBuffer<float>& buffer)
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

// An Airwindows category module (types 32-41): its chosen effect, as above.
void IlanaSynthAudioProcessor::FxEngine::processAirwindowsCategory (juce::AudioBuffer<float>& buffer, int category)
{
    const auto numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0)
        return;

    const auto& info = airwindows::categoryModules()[(size_t) category];
    const auto& refs = awCategoryRefs[(size_t) category];
    std::array<float, airwindows::Module::numKnobs> knobs {};
    for (size_t knob = 0; knob < knobs.size(); ++knob)
        knobs[knob] = getParam (refs[knob + 1]);
    const auto choice = juce::jlimit (0, (int) info.algorithms.size() - 1, (int) getParam (refs[0]));

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

    awCategoryModules[(size_t) category].process (left, right, numSamples, info.algorithms[(size_t) choice], knobs,
                                                  getParam (refs[6]));
}

void IlanaSynthAudioProcessor::FxEngine::preloadAirwindowsCategory (int category, int choice)
{
    if (category < 0 || category >= numAwCategories)
        return;
    const auto& info = airwindows::categoryModules()[(size_t) category];
    awCategoryModules[(size_t) category].preload (info.algorithms[(size_t) juce::jlimit (0, (int) info.algorithms.size() - 1, choice)]);
}

void IlanaSynthAudioProcessor::FxEngine::processUtility (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto gain = juce::Decibels::decibelsToGain (getParam ("fx_util_gain"));
    const auto mono = getParam ("fx_util_mono") > 0.5f;
    const auto invert = getParam ("fx_util_invert") > 0.5f;

    auto& gainRamp = fxRamps[(size_t) FxRamp::UtilGain];
    gainRamp.begin (gain, numSamples);
    const auto sign = invert ? -1.0f : 1.0f;

    for (int channel = 0; channel < channels; ++channel)
        if (gainRamp.moving())
            buffer.applyGainRamp (channel, 0, numSamples, gainRamp.from * sign, gain * sign);
        else
            buffer.applyGain (channel, 0, numSamples, gain * sign);

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

void IlanaSynthAudioProcessor::FxEngine::processOtt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 1.0f, getParam ("fx_ott_amount"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::OttMix];
    mixRamp.begin (getParam ("fx_ott_mix"), numSamples);
    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 200.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2000.0 / currentSampleRate);
    const auto envelopeCoefficient = (float) std::exp (-1.0 / (0.01 * currentSampleRate));
    float bandGains[3] { 1.0f, 1.0f, 1.0f }; // for the card's meters only

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
                bandGains[band] = gain;
            }

            data[i] = input + (output - input) * mixRamp.at (i);
        }
    }

    for (int band = 0; band < 3; ++band)
        ottBandGain[(size_t) band].store (bandGains[band]);
}

void IlanaSynthAudioProcessor::FxEngine::processLimiter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    auto& ceilingRamp = fxRamps[(size_t) FxRamp::LimitCeiling];
    ceilingRamp.begin (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, getParam ("fx_limit_ceiling"))), numSamples);
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_limit_release"));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));

    auto deepestGain = 1.0f; // for the card's meter only

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto magnitude = std::abs (data[i]);
            const auto ceiling = ceilingRamp.at (i);

            if (magnitude > limiterEnvelope[channel])
                limiterEnvelope[channel] = magnitude;
            else
                limiterEnvelope[channel] = magnitude + (limiterEnvelope[channel] - magnitude) * releaseCoefficient;

            const auto gain = limiterEnvelope[channel] > ceiling ? ceiling / limiterEnvelope[channel] : 1.0f;
            deepestGain = juce::jmin (deepestGain, gain);
            data[i] *= gain;
        }
    }

    limiterGainReduction.store (deepestGain);
}

void IlanaSynthAudioProcessor::FxEngine::processWidener (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto width = juce::jlimit (0.0f, 2.0f, getParam ("fx_width"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::WidthMix];
    mixRamp.begin (getParam ("fx_width_mix"), numSamples);
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid = (left[i] + right[i]) * 0.5f;
        const auto side = (left[i] - right[i]) * 0.5f * width;
        const auto wetL = mid + side;
        const auto wetR = mid - side;
        const auto mix = mixRamp.at (i);
        left[i] = left[i] + (wetL - left[i]) * mix;
        right[i] = right[i] + (wetR - right[i]) * mix;
    }
}

void IlanaSynthAudioProcessor::FxEngine::processTremolo (juce::AudioBuffer<float>& buffer)
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

void IlanaSynthAudioProcessor::FxEngine::processFreqShift (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto shiftHz = juce::jlimit (-2000.0f, 2000.0f, getParam ("fx_shifter_shift"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::ShifterMix];
    mixRamp.begin (getParam ("fx_shifter_mix"), numSamples);
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
            data[i] = input + (shifted - input) * mixRamp.at (i);
        }

        shifterPhase += increment;

        if (shifterPhase >= 1.0)
            shifterPhase -= 1.0;
        else if (shifterPhase < 0.0)
            shifterPhase += 1.0;
    }
}

void IlanaSynthAudioProcessor::FxEngine::processRingMod (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const auto frequency = juce::jlimit (1.0f, 5000.0f, getParam ("fx_ring_freq"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::RingMix];
    mixRamp.begin (getParam ("fx_ring_mix"), numSamples);
    const auto increment = (double) frequency / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto carrier = (float) std::sin (juce::MathConstants<double>::twoPi * ringPhase);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            data[i] = data[i] + (data[i] * carrier - data[i]) * mixRamp.at (i);
        }

        ringPhase += increment;

        if (ringPhase >= 1.0)
            ringPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::FxEngine::processOctaver (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    auto& mixRamp = fxRamps[(size_t) FxRamp::OctaverMix];
    mixRamp.begin (getParam ("fx_octaver_mix"), numSamples);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        octaverShift[channel].setRatio (0.5);

        for (int i = 0; i < numSamples; ++i)
        {
            octaverShift[channel].push (data[i]);
            const auto shifted = octaverShift[channel].process();
            data[i] = data[i] + (shifted - data[i]) * mixRamp.at (i);
        }
    }
}

void IlanaSynthAudioProcessor::FxEngine::processVowel (juce::AudioBuffer<float>& buffer)
{
    static const float formant1[5] { 800.0f, 400.0f, 350.0f, 450.0f, 325.0f };
    static const float formant2[5] { 1150.0f, 1600.0f, 1700.0f, 800.0f, 700.0f };

    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto morph = juce::jlimit (0.0f, 1.0f, getParam ("fx_vowel_morph"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::VowelMix];
    mixRamp.begin (getParam ("fx_vowel_mix"), numSamples);
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
            data[i] = input + (wet - input) * mixRamp.at (i);
        }
    }
}

void IlanaSynthAudioProcessor::FxEngine::processFeedback (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 0.95f, getParam ("fx_feedback_amount"));
    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.11),
                                            (float) (getParam ("fx_feedback_delay") * 0.001 * currentSampleRate));
    const auto tone = juce::jlimit (0.0f, 1.0f, getParam ("fx_feedback_tone"));
    auto& mixRamp = fxRamps[(size_t) FxRamp::FeedbackMix];
    mixRamp.begin (getParam ("fx_feedback_mix"), numSamples);
    const auto coefficient = 1.0f - tone * 0.9f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto delayed = feedbackLine.popSample (channel, delaySamples, true);

            feedbackState[channel] += (delayed - feedbackState[channel]) * coefficient;
            feedbackLine.pushSample (channel, data[i] + feedbackState[channel] * amount);
            data[i] += delayed * mixRamp.at (i);
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
        preloadAirwindows ((int) getParam ("fx_aw_algo"));
    if (const auto category = airwindows::categoryForFxType (type); category >= 0)
        preloadAirwindowsCategory (category, (int) getParam (makeAwCategoryRefs()[(size_t) category][0]));
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

    // Each effect at most once: the rack keeps one set of settings per type,
    // so a second copy would only mirror the first (V7-42).
    std::vector<bool> used ((size_t) typeCount + 1, false);
    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        auto type = random.nextFloat() < 0.15f ? 0 : 1 + random.nextInt (typeCount);
        while (type > 0 && used[(size_t) type])
            type = 1 + random.nextInt (typeCount);
        if (type > 0)
            used[(size_t) type] = true;
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

    if (const auto fx = getFxPatch(); ! fx.isDefault())
        xml.addChildElement (FxPatchData::toValueTree (fx).createXml().release());

    return xml.writeTo (file);
}

void IlanaSynthAudioProcessor::FxEngine::loadReverbIr (const juce::File& file)
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

    // A chain file saved before the rack had a routing switch is series.
    if (xml->getChildByAttribute ("id", "fx_routing") == nullptr)
        if (auto* routing = apvts.getParameter ("fx_routing"))
            routing->setValueNotifyingHost (routing->getDefaultValue());

    for (auto* element = xml->getFirstChildElement(); element != nullptr; element = element->getNextElement())
    {
        const auto id = element->getStringAttribute ("id");

        if (id.isEmpty())
            continue;

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 ((float) element->getDoubleAttribute ("v")));
    }

    // Its instances and patcher graph (none in a chain saved before step 17).
    const auto* fx = xml->getChildByName (FxPatchData::patchTag.toString());
    setFxPatch (fx != nullptr ? FxPatchData::fromValueTree (juce::ValueTree::fromXml (*fx)) : FxPatchData::Patch {});
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

    // The instances and the patcher graph ride along (step 17).
    if (const auto fx = getFxPatch(); ! fx.isDefault())
    {
        const auto text = FxPatchData::toValueTree (fx).toXmlString (juce::XmlElement::TextFormat().singleLine());
        state += "fxpatch=" + juce::Base64::toBase64 (text) + ";";
    }

    return state;
}

void IlanaSynthAudioProcessor::applyFxChain (const juce::String& state)
{
    const auto tokens = juce::StringArray::fromTokens (state, ";", "");

    // A chain stored before the rack had a routing switch was series.
    if (! state.contains ("fx_routing="))
        if (auto* routing = apvts.getParameter ("fx_routing"))
            routing->setValueNotifyingHost (routing->getDefaultValue());

    FxPatchData::Patch fx;
    for (const auto& token : tokens)
    {
        const auto id = token.upToFirstOccurrenceOf ("=", false, false);
        const auto text = token.fromFirstOccurrenceOf ("=", false, false);

        if (id.isEmpty())
            continue;

        if (id == "fxpatch")
        {
            juce::MemoryOutputStream decoded;
            if (juce::Base64::convertFromBase64 (decoded, text))
                if (const auto xml = juce::parseXML (decoded.toString()))
                    fx = FxPatchData::fromValueTree (juce::ValueTree::fromXml (*xml));
            continue;
        }

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (text.getFloatValue()));
    }
    // After the slots are set (a chain from before step 17: no instances).
    setFxPatch (fx);
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

// Roadmap step A: the rack as a graph. Each slot with an effect is a node
// (ref = its slot number), wired as SERIES or PARALLEL wires it today.
PatchGraph::Graph IlanaSynthAudioProcessor::getFxRackGraph (bool ignoreBypass)
{
    std::vector<PatchGraph::RackSlot> slots;
    for (int index = 0; index < numFxSlots; ++index)
    {
        const auto& ids = fxSlotIds[(size_t) index];
        PatchGraph::RackSlot slot;
        slot.type = (int) getParam (ids.type);
        slot.bypassed = ! ignoreBypass && getParam (ids.bypass) > 0.5f;
        slot.solo = getParam (ids.solo) > 0.5f;
        if (auto* parameter = apvts.getParameter ("fx_slot" + juce::String (index + 1)))
            slot.label = parameter->getCurrentValueAsText().toUpperCase().toStdString();
        slots.push_back (std::move (slot));
    }
    return PatchGraph::rackGraph (slots, getParam (fxRoutingRef) > 0.5f);
}

// The rack through a compiled graph: every node runs its slot exactly as the
// fixed loop does (MIX, band, solo, meters, CPU share), so a graph from
// getFxRackGraph renders what processEffects renders.
void IlanaSynthAudioProcessor::processEffectsGraph (juce::AudioBuffer<float>& buffer, PatchGraph::Runner& runner)
{
    for (int index = 0; index < numFxSlots; ++index)
        if (! isFxSlotActive (index))
            clearFxSlotMeters (index);

    runner.process (buffer, [this] (const PatchGraph::CompiledNode& node, PatchGraph::NodeBuffers& io, int)
    {
        if (node.ref >= 1 && node.ref <= numFxSlots && io.numOutputs > 0)
            runFxSlot (node.ref, *io.out[0]);
    });

    sanitiseBuffer (buffer);
}

//==============================================================================
// Roadmap step 17: the effects' memory in engines (FxEngine.h).

// The effects' part of prepareToPlay (as it was there, line for line).
void IlanaSynthAudioProcessor::FxEngine::prepare (double sampleRate, int samplesPerBlock, int expectedBlockSize)
{
    stutterBuffer.setSize (2, (int) (sampleRate * 2.0), false, false, true);

    for (int channel = 0; channel < 2; ++channel)
    {
        tapeShift[channel].prepare (sampleRate);
        shimmerShift[channel].prepare (sampleRate);
        smear[channel].prepare (sampleRate);
    }
    freeze.prepare (sampleRate);
    for (auto& unit : filterUnits)
        unit.prepare (sampleRate);
    filterGlideStarted = false;

    haasLine.setMaximumDelayInSamples ((int) (sampleRate * 0.06));

    tapeStopBuffer.setSize (2, (int) (sampleRate * 2.0), false, false, true);
    tapeStopWrite = 0;
    tapeStopRead = 0.0;
    tapeStopRate = 1.0f;
    tapeStopLagFade = 1.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        octaverShift[channel].prepare (sampleRate);

        for (auto& band : eqBands[channel])
            band.reset();

        for (int band = 0; band < 3; ++band)
        {
            vowelFilters[channel][band].setSampleRate (sampleRate);
            vowelFilters[channel][band].setMode (Svf::Mode::BandPass);
            vowelFilters[channel][band].reset();
        }
    }

    flangerPhase = 0.0;
    dimPhase = 0.0;
    tremoloPhase = 0.0;
    shifterPhase = 0.0;
    ringPhase = 0.0;
    gatePhase = 0.0;
    gateEnvelope = 1.0f;
    gatedReverbEnvelope = 0.0f;
    reverbKeepDryHold = 0;
    duckEnvelope = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        tiltLowState[channel] = 0.0f;
        tiltHighState[channel] = 0.0f;
        limiterEnvelope[channel] = 0.0f;
        shifterAllpass[channel] = 0.0f;
        shifterDelay[channel] = 0.0f;

        for (int band = 0; band < 3; ++band)
            ottEnvelope[channel][band] = 0.0f;
    }

    compEnvelope[0] = 0.0f;
    compEnvelope[1] = 0.0f;
    ampLowState[0] = 0.0f;
    ampLowState[1] = 0.0f;
    ampHighState[0] = 0.0f;
    ampHighState[1] = 0.0f;

    wowPhase = 0.0;

    crushCounter = 0;
    delayDampState[0] = 0.0f;
    delayDampState[1] = 0.0f;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    haasLine.prepare (spec);
    haasLine.reset();

    feedbackLine.setMaximumDelayInSamples ((int) (sampleRate * 0.12));
    feedbackLine.prepare (spec);
    feedbackLine.reset();
    feedbackState[0] = 0.0f;
    feedbackState[1] = 0.0f;
    stutterPosition = 0.0;
    stutterRate = 1.0f;

    flangerLine.setMaximumDelayInSamples ((int) (sampleRate * 0.05));
    flangerLine.prepare (spec);
    flangerLine.reset();

    dimLine.setMaximumDelayInSamples ((int) (sampleRate * 0.15));
    dimLine.prepare (spec);
    dimLine.reset();

    springComb.setMaximumDelayInSamples ((int) (sampleRate * 0.05));
    springComb.prepare (spec);
    springComb.reset();

    chorus.prepare (spec);
    chorus.reset();

    phaser.prepare (spec);
    phaser.reset();

    convolution.prepare (spec);
    convolution.reset();

    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayLine.prepare (spec);
    delayLine.reset();

    combLine.setMaximumDelayInSamples ((int) (sampleRate * 0.15));
    combLine.prepare (spec);
    combLine.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();
    reverbDryScratch.setSize (2, samplesPerBlock, false, false, true);
    // (Sized here so the audio thread need not; the grows in processReverb and
    // processAirwindows stay as a guard for a longer block.)
    reverbScratch.setSize (2, juce::jmax (samplesPerBlock, expectedBlockSize), false, false, true);
    airwindowsMonoRight.assign ((size_t) juce::jmax (samplesPerBlock, expectedBlockSize), 0.0f);
    airwindowsModule.prepare (sampleRate, samplesPerBlock);
    for (auto& module : awCategoryModules)
        module.prepare (sampleRate, samplesPerBlock);
    vocoder.prepare (sampleRate);
    vocoderModulator.assign ((size_t) juce::jmax (samplesPerBlock, expectedBlockSize), 0.0f);
}

// The effects' part of a patch change (cutPatchTails), on the audio thread
// and without allocating.
void IlanaSynthAudioProcessor::FxEngine::reset()
{
    for (auto& unit : filterUnits)
        unit.reset();
    filterGlideStarted = false;

    for (int channel = 0; channel < 2; ++channel)
    {
        tapeShift[channel].reset();
        shimmerShift[channel].reset();
        octaverShift[channel].reset();
        smear[channel].reset();
        feedbackState[channel] = 0.0f;
        delayDampState[channel] = 0.0f;
    }

    freeze.reset();
    stutterBuffer.clear();
    tapeStopBuffer.clear();
    haasLine.reset();
    feedbackLine.reset();
    flangerLine.reset();
    dimLine.reset();
    springComb.reset();
    chorus.reset();
    phaser.reset();
    convolution.reset();
    delayLine.reset();
    combLine.reset();
    reverb.reset();
    airwindowsModule.reset();
    for (auto& module : awCategoryModules)
        module.reset();
    vocoder.reset();

    // The effects' own LFOs and followers restart too (a Dimension or flanger
    // phase left by the last patch otherwise changes how this one starts).
    flangerPhase = 0.0;
    dimPhase = 0.0;
    tremoloPhase = 0.0;
    shifterPhase = 0.0;
    ringPhase = 0.0;
    gatePhase = 0.0;
    gateEnvelope = 1.0f;
    gatedReverbEnvelope = 0.0f;
    reverbKeepDryHold = 0;
    duckEnvelope = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
    {
        limiterEnvelope[channel] = 0.0f;
        for (int band = 0; band < 3; ++band)
            ottEnvelope[channel][band] = 0.0f;
    }
    compEnvelope[0] = compEnvelope[1] = 0.0f;

    // The effects' filters and followers that the lists above leave out
    // (OTT, tilt, EQ, vowel, frequency shifter, amp, crush): their state
    // otherwise rings into the next patch.
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int band = 0; band < 2; ++band)
            ottLowState[channel][band] = ottHighState[channel][band] = 0.0f;
        tiltLowState[channel] = tiltHighState[channel] = 0.0f;
        shifterAllpass[channel] = shifterDelay[channel] = 0.0f;
        ampLowState[channel] = ampHighState[channel] = 0.0f;
        crushHold[channel] = 0.0f;
        for (auto& band : eqBands[channel])
            band.reset();
        for (auto& filter : vowelFilters[channel])
            filter.reset();
    }
    crushCounter = 0;
}

IlanaSynthAudioProcessor::FxEngine& IlanaSynthAudioProcessor::engineForSlot (int slotIndex) const
{
    if (juce::isPositiveAndBelow (slotIndex, numFxSlots))
        if (auto* own = slotEngines[(size_t) slotIndex].load (std::memory_order_acquire))
            return *own;
    return *fxMain;
}

void IlanaSynthAudioProcessor::resetFxEngineRamps() noexcept
{
    fxMain->resetRamps();
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->resetRamps();
}

void IlanaSynthAudioProcessor::prepareFxEngines (double sampleRate, int samplesPerBlock)
{
    resetFxLfos();
    fxMain->prepare (sampleRate, samplesPerBlock, expectedBlockSize);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->prepare (sampleRate, samplesPerBlock, expectedBlockSize);
    fxEngineBlockSize = samplesPerBlock;
}

void IlanaSynthAudioProcessor::resetFxEngines()
{
    resetFxLfos();
    fxPatchRunner->reset(); // the patcher's loops start empty
    fxMain->reset();
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->reset();
}

// The card displays read the engine the card stands for: the slot its
// controls are showing (displayEngineFor), else the shared one.
float IlanaSynthAudioProcessor::getCompGainReduction() const { return displayEngine (4).compGainReduction.load(); }
float IlanaSynthAudioProcessor::getLimiterGainReduction() const { return displayEngine (21).limiterGainReduction.load(); }
float IlanaSynthAudioProcessor::getOttBandGain (int band) const { return displayEngine (20).ottBandGain[(size_t) juce::jlimit (0, 2, band)].load(); }
void IlanaSynthAudioProcessor::getFreezeBands (std::array<float, SpectralFreeze::numBands>& out) const { displayEngine (12).freeze.getBands (out); }
int IlanaSynthAudioProcessor::getGateDisplayStep() const { return displayEngine (16).gateDisplayStep.load(); }

float IlanaSynthAudioProcessor::getVocoderBandLevel (int band) const
{
    const auto& engine = displayEngine (31);
    return juce::isPositiveAndBelow (band, (int) engine.vocoderBandDisplay.size()) ? engine.vocoderBandDisplay[(size_t) band].load() : 0.0f;
}

void IlanaSynthAudioProcessor::preloadAirwindows (int algorithm)
{
    fxMain->preloadAirwindows (algorithm);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->preloadAirwindows (algorithm);
}

void IlanaSynthAudioProcessor::preloadAirwindowsCategory (int category, int choice)
{
    fxMain->preloadAirwindowsCategory (category, choice);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->preloadAirwindowsCategory (category, choice);
}

// An impulse response goes to the reverb the REVERB card is showing.
void IlanaSynthAudioProcessor::loadReverbIr (const juce::File& file)
{
    const_cast<FxEngine&> (displayEngine (13)).loadReverbIr (file);
}
