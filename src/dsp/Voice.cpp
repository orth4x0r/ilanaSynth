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
} // namespace

Voice::Voice() = default;

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

    ampEnv.setSampleRate (newRate);
    filterEnv.setSampleRate (newRate);
    filter2Env.setSampleRate (newRate);
    modEnv.setSampleRate (newRate);
    env4.setSampleRate (newRate);

    for (auto& osc : osc1Unison)
        osc.setSampleRate (newRate);

    for (auto& osc : osc2Unison)
        osc.setSampleRate (newRate);

    for (auto& string : string1Unison)
        string.prepare (newRate);

    for (auto& string : string2Unison)
        string.prepare (newRate);

    for (auto& osc : subUnison)
        osc.setSampleRate (newRate);

    for (auto& string : subStrings)
        string.prepare (newRate);

    for (auto& player : sample1Unison)
        player.prepare (newRate);

    for (auto& player : sample2Unison)
        player.prepare (newRate);

    for (auto& player : subSamples)
        player.prepare (newRate);

    resonatorL.prepare (newRate);
    resonatorR.prepare (newRate);

    for (auto* filter : { &filter1L, &filter1R, &filter2L, &filter2R })
        filter->reset();
}

void Voice::syncSamplePlayers()
{
    const auto setup = [] (const VoiceParams::OscParams& osc, SamplePlayer* players,
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

        for (int u = 0; u < VoiceParams::maxUnison; ++u)
            players[u].setParams (sampleParams);
    };

    setup (params.osc1, sample1Unison,
           evaluateModAtBlockStart (Mod::Destination::Osc1SampleStart),
           evaluateModAtBlockStart (Mod::Destination::Osc1SampleEnd));
    setup (params.osc2, sample2Unison,
           evaluateModAtBlockStart (Mod::Destination::Osc2SampleStart),
           evaluateModAtBlockStart (Mod::Destination::Osc2SampleEnd));
    setup (params.sub, subSamples,
           evaluateModAtBlockStart (Mod::Destination::SubSampleStart),
           evaluateModAtBlockStart (Mod::Destination::SubSampleEnd));
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
        const auto basePhase = (double) u / (double) VoiceParams::maxUnison;
        const auto jitter = (double) random.nextFloat() * (double) params.unisonRandom;

        auto phase = basePhase + jitter;
        phase -= std::floor (phase);

        osc1Unison[u].resetPhase (phase);
        osc2Unison[u].resetPhase (phase);
    }

    resonatorL.reset();
    resonatorR.reset();

    if (params.osc1.stringMode && params.osc1Enabled)
        for (auto& string : string1Unison)
            string.trigger (velocity);

    if (params.osc2.stringMode && params.osc2Enabled)
        for (auto& string : string2Unison)
            string.trigger (velocity);

    if (params.sub.stringMode && params.subEnabled)
        for (auto& string : subStrings)
            string.trigger (velocity);

    syncSamplePlayers();

    if (params.osc1.sampleMode && params.osc1Enabled && params.osc1.sample != nullptr)
        for (int u = 0; u < juce::jlimit (1, VoiceParams::maxUnison, params.osc1.unison); ++u)
            sample1Unison[u].trigger();

    if (params.osc2.sampleMode && params.osc2Enabled && params.osc2.sample != nullptr)
        for (int u = 0; u < juce::jlimit (1, VoiceParams::maxUnison, params.osc2.unison); ++u)
            sample2Unison[u].trigger();

    if (params.sub.sampleMode && params.subEnabled && params.sub.sample != nullptr)
        for (int u = 0; u < juce::jlimit (1, VoiceParams::maxUnison, params.sub.unison); ++u)
            subSamples[u].trigger();

    lastStringMode1 = params.osc1.stringMode;
    lastStringMode2 = params.osc2.stringMode;
    lastSubStringMode = params.sub.stringMode;
    lastSampleMode1 = params.osc1.sampleMode;
    lastSampleMode2 = params.osc2.sampleMode;
    lastSubSampleMode = params.sub.sampleMode;

    previousOsc1 = 0.0f;
    previousOsc2 = 0.0f;
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

void Voice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    ampEnv.setParameters (params.ampEnv);
    filterEnv.setParameters (params.filterEnv);
    filter2Env.setParameters (params.filter2Env);
    modEnv.setParameters (params.modEnv);
    env4.setParameters (params.env4);

    glideCoeff = params.glideTime > 0.001f
                     ? 1.0f - std::exp (-1.0f / (float) (params.glideTime * sampleRate))
                     : 1.0f;

    numOsc1Unison = juce::jlimit (1, VoiceParams::maxUnison, params.osc1.unison);
    numOsc2Unison = juce::jlimit (1, VoiceParams::maxUnison, params.osc2.unison);
    numSubUnison = juce::jlimit (1, VoiceParams::maxUnison, params.sub.unison);

    for (int u = 0; u < numOsc1Unison; ++u)
        osc1Unison[u].setWavetable (params.osc1.table);

    for (int u = 0; u < numOsc2Unison; ++u)
        osc2Unison[u].setWavetable (params.osc2.table);

    for (int u = 0; u < numSubUnison; ++u)
        subUnison[u].setWavetable (params.sub.table);

    for (int u = 0; u < numSubUnison; ++u)
        subStrings[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.sub.stringExcite)),
                                 params.sub.stringSustain, params.sub.stringDamping, params.sub.stringDecay);

    if (params.sub.stringMode && ! lastSubStringMode && params.subEnabled)
        for (int u = 0; u < numSubUnison; ++u)
            subStrings[u].trigger (velocityLevel);

    lastSubStringMode = params.sub.stringMode;

    const auto panMod = juce::jlimit (-1.0f, 1.0f, evaluateModAtBlockStart (Mod::Destination::Pan));

    for (int u = 0; u < numOsc1Unison; ++u)
    {
        const auto spreadOffset = numOsc1Unison > 1
                                      ? params.osc1.spread * ((float) u / (float) (numOsc1Unison - 1) * 2.0f - 1.0f)
                                      : 0.0f;
        const auto pan = juce::jlimit (-1.0f, 1.0f, params.osc1.pan + spreadOffset + panMod + params.voiceSpread * voicePan);
        const auto angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
        panGain1L[u] = std::cos (angle);
        panGain1R[u] = std::sin (angle);
    }

    for (int u = 0; u < numOsc2Unison; ++u)
    {
        const auto spreadOffset = numOsc2Unison > 1
                                      ? params.osc2.spread * ((float) u / (float) (numOsc2Unison - 1) * 2.0f - 1.0f)
                                      : 0.0f;
        const auto pan = juce::jlimit (-1.0f, 1.0f, params.osc2.pan + spreadOffset + panMod + params.voiceSpread * voicePan);
        const auto angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
        panGain2L[u] = std::cos (angle);
        panGain2R[u] = std::sin (angle);
    }

    for (int u = 0; u < numSubUnison; ++u)
    {
        const auto spreadOffset = numSubUnison > 1
                                      ? params.sub.spread * ((float) u / (float) (numSubUnison - 1) * 2.0f - 1.0f)
                                      : 0.0f;
        const auto pan = juce::jlimit (-1.0f, 1.0f, params.sub.pan + spreadOffset + panMod + params.voiceSpread * voicePan);
        const auto angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
        panGainSubL[u] = std::cos (angle);
        panGainSubR[u] = std::sin (angle);
    }

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

    const auto unisonGain1 = 1.0f / std::sqrt ((float) numOsc1Unison);
    const auto unisonGain2 = 1.0f / std::sqrt ((float) numOsc2Unison);
    const auto unisonGainSub = 1.0f / std::sqrt ((float) numSubUnison);
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

    if (sampleChanged1)
    {
        if (! lastSampleMode1 || ! lastEnabled1)
            for (int u = 0; u < numOsc1Unison; ++u)
                sample1Unison[u].trigger();
        else
            for (int u = lastUnison1; u < numOsc1Unison; ++u)
                sample1Unison[u].trigger();
    }

    if (sampleChanged2)
    {
        if (! lastSampleMode2 || ! lastEnabled2)
            for (int u = 0; u < numOsc2Unison; ++u)
                sample2Unison[u].trigger();
        else
            for (int u = lastUnison2; u < numOsc2Unison; ++u)
                sample2Unison[u].trigger();
    }

    if (sampleChangedSub)
    {
        if (! lastSubSampleMode || ! lastEnabledSub)
            for (int u = 0; u < numSubUnison; ++u)
                subSamples[u].trigger();
        else
            for (int u = lastUnisonSub; u < numSubUnison; ++u)
                subSamples[u].trigger();
    }

    lastSampleMode1 = params.osc1.sampleMode;
    lastSampleMode2 = params.osc2.sampleMode;
    lastSubSampleMode = params.sub.sampleMode;
    lastEnabled1 = params.osc1Enabled;
    lastEnabled2 = params.osc2Enabled;
    lastEnabledSub = params.subEnabled;
    lastUnison1 = numOsc1Unison;
    lastUnison2 = numOsc2Unison;
    lastUnisonSub = numSubUnison;

    for (int u = 0; u < numOsc1Unison; ++u)
        string1Unison[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.osc1.stringExcite)),
                                    params.osc1.stringSustain, params.osc1.stringDamping, params.osc1.stringDecay);

    for (int u = 0; u < numOsc2Unison; ++u)
        string2Unison[u].setParams (static_cast<KarplusStrong::Excite> (juce::jlimit (0, 3, params.osc2.stringExcite)),
                                    params.osc2.stringSustain, params.osc2.stringDamping, params.osc2.stringDecay);

    if (params.osc1.stringMode && ! lastStringMode1)
        for (int u = 0; u < numOsc1Unison; ++u)
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
        for (int u = 0; u < numOsc2Unison; ++u)
            string2Unison[u].trigger (velocityLevel);

    lastStringMode1 = params.osc1.stringMode;
    lastStringMode2 = params.osc2.stringMode;

    resonatorL.setParams (params.resonatorOn ? params.resonatorAmount : 0.0f,
                          params.resonatorDecay, 0.35f);
    resonatorR.setParams (params.resonatorOn ? params.resonatorAmount : 0.0f,
                          params.resonatorDecay, 0.35f);

    auto* left = outputBuffer.getWritePointer (0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1) : nullptr;

    const auto ampVelScale = 1.0f - params.ampVelocity + params.ampVelocity * velocityLevel;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto ampValue = ampEnv.getNextSample();
        const auto filterValue = filterEnv.getNextSample();
        const auto filter2Value = filter2Env.getNextSample();
        const auto modValue = modEnv.getNextSample();
        const auto env4Value = env4.getNextSample();

        std::array<float, (size_t) Mod::Destination::Count> mods {};
        auto* modsPtr = mods.data();

        for (int s = 0; s < params.numModSlots; ++s)
        {
            const auto& slot = params.modSlots[s];

            if (slot.source == Mod::Source::None
                || slot.destination == Mod::Destination::None
                || slot.depth == 0.0f)
                continue;

            modsPtr[(int) slot.destination] += slot.depth
                                               * sourceValue (slot.source, i, ampValue, filterValue, filter2Value,
                                                              modValue, env4Value);
        }

        if ((i & 15) == 0)
            updateSubBlock (modsPtr, filterValue, filter2Value);
        else if (params.filter1Fm != 0.0f || params.filter2Fm != 0.0f)
            updateFilterCoefficients (modsPtr, filterValue, filter2Value);

        auto oscL = 0.0f;
        auto oscR = 0.0f;
        auto osc1Mono = 0.0f;
        auto osc2Mono = 0.0f;

        const auto phaseModulation = (double) (params.fmAmount * previousOsc2 + params.fmFeedback * previousOsc1);

        const auto enable1 = osc1EnableSmooth.getNextValue();

        if (osc1Active || enable1 > 0.0005f)
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth1.getNextValue() + modsPtr[(int) Mod::Destination::Osc1Frame]);
            const auto level = juce::jlimit (0.0f, 1.0f, levelSmooth1.getNextValue() + modsPtr[(int) Mod::Destination::Osc1Level]);

            for (int u = 0; u < numOsc1Unison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.osc1.sampleMode)
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
                    raw = osc1Unison[u].getNextSample (phaseModulation);
                }

                const auto gain = unisonGain1 * level * enable1;
                osc1Mono += raw * gain;
                oscL += (params.osc1.sampleMode ? sampleL : raw) * gain * panGain1L[u];
                oscR += (params.osc1.sampleMode ? sampleR : raw) * gain * panGain1R[u];
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

        if (osc2Active || enable2 > 0.0005f)
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, frameSmooth2.getNextValue() + modsPtr[(int) Mod::Destination::Osc2Frame]);
            const auto level = juce::jlimit (0.0f, 1.0f, levelSmooth2.getNextValue() + modsPtr[(int) Mod::Destination::Osc2Level]);

            for (int u = 0; u < numOsc2Unison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.osc2.sampleMode)
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
                    raw = osc2Unison[u].getNextSample();
                }

                const auto gain = unisonGain2 * level * enable2;
                osc2Mono += raw * gain;
                oscL += (params.osc2.sampleMode ? sampleL : raw) * gain * panGain2L[u];
                oscR += (params.osc2.sampleMode ? sampleR : raw) * gain * panGain2R[u];
            }
        }
        else
        {
            frameSmooth2.getNextValue();
            levelSmooth2.getNextValue();
        }

        if (params.ringMod > 0.0f && osc1Active && osc2Active)
        {
            oscL += (oscL * osc2Mono - oscL) * params.ringMod;
            oscR += (oscR * osc2Mono - oscR) * params.ringMod;
        }

        previousOsc1 = juce::jlimit (-2.0f, 2.0f, osc1Mono);
        previousOsc2 = juce::jlimit (-2.0f, 2.0f, osc2Mono);

        const auto subLevel = juce::jlimit (0.0f, 1.0f, subSmooth.getNextValue() + modsPtr[(int) Mod::Destination::SubLevel]);
        const auto enableSub = subEnableSmooth.getNextValue();

        if (subLevel > 0.0f && (subActive || enableSub > 0.0005f))
        {
            const auto frame = juce::jlimit (0.0f, 1.0f, subFrameSmooth.getNextValue()
                                                          + modsPtr[(int) Mod::Destination::SubFrame]);

            for (int u = 0; u < numSubUnison; ++u)
            {
                float raw = 0.0f;
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (params.sub.sampleMode)
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
                    raw = subUnison[u].getNextSample();
                }

                const auto gain = unisonGainSub * subLevel * enableSub;
                oscL += (params.sub.sampleMode ? sampleL : raw) * gain * panGainSubL[u];
                oscR += (params.sub.sampleMode ? sampleR : raw) * gain * panGainSubR[u];
            }
        }
        else
        {
            subFrameSmooth.getNextValue();
        }

        const auto noiseLevel = juce::jlimit (0.0f, 1.0f, noiseSmooth.getNextValue() + modsPtr[(int) Mod::Destination::NoiseLevel]);

        if (noiseLevel > 0.0f)
        {
            const auto value = (random.nextFloat() * 2.0f - 1.0f) * noiseLevel * 0.5f;
            oscL += value;
            oscR += value;
        }

        auto inL = oscL;
        auto inR = oscR;

        if (params.filter1.drive > 1.0f)
        {
            inL = std::tanh (inL * params.filter1.drive);
            inR = std::tanh (inR * params.filter1.drive);
        }

        auto f1L = filter1L.process (inL);
        auto f1R = filter1R.process (inR);

        float outL, outR;

        if (params.filtersParallel)
        {
            auto in2L = inL;
            auto in2R = inR;

            if (params.filter2.drive > 1.0f)
            {
                in2L = std::tanh (in2L * params.filter2.drive);
                in2R = std::tanh (in2R * params.filter2.drive);
            }

            outL = (f1L + filter2L.process (in2L)) * 0.7071f;
            outR = (f1R + filter2R.process (in2R)) * 0.7071f;
        }
        else
        {
            auto f2inL = f1L;
            auto f2inR = f1R;

            if (params.filter2.drive > 1.0f)
            {
                f2inL = std::tanh (f2inL * params.filter2.drive);
                f2inR = std::tanh (f2inR * params.filter2.drive);
            }

            outL = filter2L.process (f2inL);
            outR = filter2R.process (f2inR);
        }

        if (params.resonatorOn && params.resonatorAmount > 0.001f)
        {
            outL = resonatorL.process (outL);
            outR = resonatorR.process (outR);
        }

        const auto ampGain = ampValue * ampVelScale
                             * juce::jlimit (0.0f, 2.0f, 1.0f + modsPtr[(int) Mod::Destination::AmpLevel]);

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
    currentFrequency += (baseFrequency - currentFrequency) * (double) glideCoeff;

    auto driftFactor = 1.0;

    if (params.drift > 0.0f)
    {
        if (driftRandom.nextFloat() > 0.995f)
            driftTarget = driftRandom.nextFloat() * 2.0f - 1.0f;

        driftValue += (driftTarget - driftValue) * 0.002f;
        driftFactor = std::exp2 ((double) driftValue * (double) params.drift * 0.25 / 12.0);
    }

    const auto driftedFrequency = currentFrequency * driftFactor;

    const auto pitch1 = std::exp2 ((params.osc1.semitones + params.osc1.cents / 100.0 + bendSemitones
                                    + (double) mods[(int) Mod::Destination::Osc1Pitch] * 48.0) / 12.0);
    const auto baseFreq1 = driftedFrequency * pitch1;

    for (int u = 0; u < numOsc1Unison; ++u)
    {
        const auto detune = numOsc1Unison > 1
                                ? juce::jlimit (-1.0f, 1.0f, (float) u / (float) (numOsc1Unison - 1) * 2.0f - 1.0f)
                                : 0.0f;
        const auto interval = (double) chordInterval (params.osc1Chord, u);
        const auto frequencyU = baseFreq1 * std::exp2 ((interval + (double) detune * (double) params.osc1.detuneCents / 100.0) / 12.0);
        osc1Unison[u].setFrequency (frequencyU);
        string1Unison[u].setFrequency (frequencyU);
        sampleRatio1[u] = params.osc1.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
        sample1Unison[u].setPlaybackRatio ((params.osc1.sample != nullptr ? params.osc1.sample->sampleRate / sampleRate : 1.0)
                                           * sampleRatio1[u]);
    }

    const auto pitch2 = std::exp2 ((params.osc2.semitones + params.osc2.cents / 100.0 + bendSemitones
                                    + (double) mods[(int) Mod::Destination::Osc2Pitch] * 48.0) / 12.0);
    const auto baseFreq2 = driftedFrequency * pitch2;

    for (int u = 0; u < numOsc2Unison; ++u)
    {
        const auto detune = numOsc2Unison > 1
                                ? juce::jlimit (-1.0f, 1.0f, (float) u / (float) (numOsc2Unison - 1) * 2.0f - 1.0f)
                                : 0.0f;
        const auto interval = (double) chordInterval (params.osc2Chord, u);
        const auto frequencyU = baseFreq2 * std::exp2 ((interval + (double) detune * (double) params.osc2.detuneCents / 100.0) / 12.0);
        osc2Unison[u].setFrequency (frequencyU);
        string2Unison[u].setFrequency (frequencyU);
        sampleRatio2[u] = params.osc2.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
        sample2Unison[u].setPlaybackRatio ((params.osc2.sample != nullptr ? params.osc2.sample->sampleRate / sampleRate : 1.0)
                                           * sampleRatio2[u]);
    }

    if (params.sub.level > 0.0f)
    {
        const auto subPitch = std::exp2 ((params.sub.semitones + (double) params.subOctaveOffset
                                          + params.sub.cents / 100.0 + bendSemitones
                                          + (double) mods[(int) Mod::Destination::SubPitch] * 48.0) / 12.0);

        for (int u = 0; u < numSubUnison; ++u)
        {
            const auto detune = numSubUnison > 1
                                    ? juce::jlimit (-1.0f, 1.0f, (float) u / (float) (numSubUnison - 1) * 2.0f - 1.0f)
                                    : 0.0f;
            const auto interval = (double) chordInterval (params.sub.chord, u);
            const auto frequencyU = driftedFrequency * subPitch
                                    * std::exp2 ((interval + (double) detune * (double) params.sub.detuneCents / 100.0) / 12.0);
            subUnison[u].setFrequency (frequencyU);
            subStrings[u].setFrequency (frequencyU);
            sampleRatioSub[u] = params.sub.sampleTuned ? frequencyU / 261.6255653005986 : 1.0;
            subSamples[u].setPlaybackRatio ((params.sub.sample != nullptr ? params.sub.sample->sampleRate / sampleRate : 1.0)
                                            * sampleRatioSub[u]);
        }
    }

    if (params.resonatorOn && params.resonatorAmount > 0.001f)
    {
        resonatorL.setTuning (driftedFrequency, params.resonatorOffset, params.resonatorKeytrack);
        resonatorR.setTuning (driftedFrequency, params.resonatorOffset, params.resonatorKeytrack);
    }

    updateFilterCoefficients (mods, filterEnvValue, filter2EnvValue);
}

void Voice::updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue)
{
    const auto velocityEnvScale = 1.0f - params.filterVelocity + params.filterVelocity * velocityLevel;

    const auto fmOctaves1 = (double) params.filter1Fm * (double) previousOsc2 * 4.0;
    const auto fmOctaves2 = (double) params.filter2Fm * (double) previousOsc2 * 4.0;

    const auto keyOctaves1 = (double) params.filter1.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves1 = (double) params.filter1.envAmount * (double) filterEnvValue * (double) velocityEnvScale;
    const auto cutoff1 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter1.cutoffHz
                                           * std::exp2 (keyOctaves1 + envOctaves1 + fmOctaves1
                                                        + (double) mods[(int) Mod::Destination::Filter1Cutoff] * 6.0));
    const auto reso1 = juce::jlimit (0.0f, 1.0f, params.filter1.resonance + mods[(int) Mod::Destination::Filter1Reso]);

    for (auto* filter : { &filter1L, &filter1R })
        filter->setType (params.filter1.type, params.filter1.slope24);

    const auto coefficients1 = FilterUnit::makeCoefficients (params.filter1.type, sampleRate, cutoff1, reso1);
    filter1L.setCoefficients (coefficients1);
    filter1R.setCoefficients (coefficients1);

    const auto keyOctaves2 = (double) params.filter2.keyTrack * (double) keyTrackOctaves;
    const auto envOctaves2 = (double) params.filter2.envAmount * (double) filter2EnvValue * (double) velocityEnvScale;
    const auto cutoff2 = juce::jlimit (20.0, sampleRate * 0.45,
                                       (double) params.filter2.cutoffHz
                                           * std::exp2 (keyOctaves2 + envOctaves2 + fmOctaves2
                                                        + (double) mods[(int) Mod::Destination::Filter2Cutoff] * 6.0));
    const auto reso2 = juce::jlimit (0.0f, 1.0f, params.filter2.resonance + mods[(int) Mod::Destination::Filter2Reso]);

    for (auto* filter : { &filter2L, &filter2R })
        filter->setType (params.filter2.type, params.filter2.slope24);

    const auto coefficients2 = FilterUnit::makeCoefficients (params.filter2.type, sampleRate, cutoff2, reso2);
    filter2L.setCoefficients (coefficients2);
    filter2R.setCoefficients (coefficients2);
}

float Voice::sourceValue (Mod::Source source, int sampleIndex, float ampValue, float filterValue,
                          float filter2Value, float modValue, float env4Value) const
{
    switch (source)
    {
        case Mod::Source::Lfo1:       return params.lfo1 != nullptr ? params.lfo1[sampleIndex] : 0.0f;
        case Mod::Source::Lfo2:       return params.lfo2 != nullptr ? params.lfo2[sampleIndex] : 0.0f;
        case Mod::Source::Lfo3:       return params.lfo3 != nullptr ? params.lfo3[sampleIndex] : 0.0f;
        case Mod::Source::Lfo4:       return params.lfo4 != nullptr ? params.lfo4[sampleIndex] : 0.0f;
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
        default:                      return 0.0f;
    }
}

float Voice::evaluateModAtBlockStart (Mod::Destination destination) const
{
    auto sum = 0.0f;

    for (int s = 0; s < params.numModSlots; ++s)
    {
        const auto& slot = params.modSlots[s];

        if (slot.destination != destination || slot.source == Mod::Source::None || slot.depth == 0.0f)
            continue;

        sum += slot.depth * sourceValue (slot.source, 0, lastAmpValue, lastFilterValue,
                                         lastFilter2Value, lastModValue, lastEnv4Value);
    }

    return sum;
}
