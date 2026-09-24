#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "KarplusStrong.h"
#include "Modulation.h"
#include "PolyBlepOsc.h"
#include "ResonatorBank.h"
#include "SamplePlayer.h"
#include "FilterUnit.h"
#include "Svf.h"
#include "TensionAdsr.h"
#include "WavetableOscillator.h"

struct VoiceParams
{
    static constexpr int maxUnison = 8;

    struct OscParams
    {
        const Wavetable* table = nullptr;
        float frame = 0.0f;
        float level = 1.0f;
        float pan = 0.0f;
        double semitones = 0.0;
        double cents = 0.0;
        int unison = 1;
        float detuneCents = 0.0f;
        float spread = 0.0f;

        bool stringMode = false;
        int stringExcite = 0;
        float stringDecay = 0.75f;
        float stringDamping = 0.35f;
        float stringSustain = 0.0f;
        int chord = 0;

        bool sampleMode = false;
        const SampleData* sample = nullptr;
        bool sampleTuned = true;
        bool sampleLoop = false;
        bool sampleReverse = false;
        float sampleStart = 0.0f;
        float sampleEnd = 1.0f;
        float sampleFadeIn = 0.0f;
        float sampleFadeOut = 0.0f;
    };

    struct FilterParams
    {
        int type = FilterType::LowPass;
        bool slope24 = false;
        float cutoffHz = 20000.0f;
        float resonance = 0.2f;
        float drive = 1.0f;
        float envAmount = 0.0f;
        float keyTrack = 0.0f;
    };

    OscParams osc1;
    bool osc1Enabled = true;
    OscParams osc2;
    bool osc2Enabled = false;
    OscParams sub;
    bool subEnabled = true;
    int subOctaveOffset = -12;

    float fmAmount = 0.0f;
    float fmFeedback = 0.0f;
    float ringMod = 0.0f;
    bool hardSync = false;
    float drift = 0.0f;

    int osc1Chord = 0;
    int osc2Chord = 0;
    float voiceSpread = 0.0f;
    float unisonRandom = 0.0f;

    float filter1Fm = 0.0f;
    float filter2Fm = 0.0f;

    bool resonatorOn = false;
    float resonatorAmount = 0.0f;
    float resonatorDecay = 0.7f;
    float resonatorOffset = 0.0f;
    float resonatorKeytrack = 1.0f;

    float subLevel = 0.0f;
    PolyBlepOsc::Shape subShape = PolyBlepOsc::Shape::Square;
    int subOctave = 1;
    float noiseLevel = 0.0f;

    FilterParams filter1;
    FilterParams filter2;
    bool filtersParallel = false;

    TensionAdsr::Parameters ampEnv { 0.005f, 0.3f, 0.8f, 0.25f, 0.0f };
    TensionAdsr::Parameters filterEnv { 0.01f, 0.4f, 0.4f, 0.3f, 0.0f };
    TensionAdsr::Parameters filter2Env { 0.01f, 0.4f, 0.4f, 0.3f, 0.0f };
    TensionAdsr::Parameters modEnv { 0.05f, 0.4f, 0.5f, 0.3f, 0.0f };
    TensionAdsr::Parameters env4 { 0.05f, 0.4f, 0.5f, 0.3f, 0.0f };
    float ampVelocity = 0.5f;
    float filterVelocity = 0.5f;

    float glideTime = 0.0f;
    float pitchBendRange = 2.0f;

    float macros[4] { 0.0f, 0.0f, 0.0f, 0.0f };

    const float* lfo1 = nullptr;
    const float* lfo2 = nullptr;
    const float* lfo3 = nullptr;
    const float* lfo4 = nullptr;
    const float* clockSh = nullptr;
    const float* mseg = nullptr;

    Mod::Slot modSlots[Mod::maxSlots] {};
    int numModSlots = 0;
};

class Voice : public juce::SynthesiserVoice
{
public:
    Voice();

    void setParams (const VoiceParams& newParams) { params = newParams; }

    float getLastAmpValue() const { return lastAmpValue; }
    float getLastSamplePosition (int oscIndex) const
    {
        return oscIndex == 0 ? lastSamplePosition1 : (oscIndex == 1 ? lastSamplePosition2 : lastSamplePositionSub);
    }

    float getLastWavetablePhase (int oscIndex) const
    {
        const auto* unison = oscIndex == 0 ? osc1Unison : (oscIndex == 1 ? osc2Unison : subUnison);
        return unison[0].getPhase();
    }
    float getLastFilterValue() const { return lastFilterValue; }
    float getLastFilter2Value() const { return lastFilter2Value; }
    float getLastModValue() const { return lastModValue; }
    float getLastEnv4Value() const { return lastEnv4Value; }

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    // Mono / legato: the synth calls this right before handing the voice a
    // new note. keepRunning stops the hard reset between notes; legato also
    // skips retriggering the envelopes; glide says whether to slide pitch
    // from the previous note or jump.
    void prepareMonoNote (bool legato, bool keepRunning, bool glide)
    {
        monoLegato = legato;
        monoKeepRunning = keepRunning;
        monoGlide = glide;
        monoPending = true;
    }

    void setCurrentPlaybackSampleRate (double newRate) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void channelPressureChanged (int newValue) override;
    void aftertouchChanged (int newValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    void syncSamplePlayers();
    void updateSubBlock (const float* mods, float filterEnvValue, float filter2EnvValue);
    void updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue);
    float sourceValue (Mod::Source source, int sampleIndex, float ampValue, float filterValue,
                       float filter2Value, float modValue, float env4Value) const;
    float evaluateModAtBlockStart (Mod::Destination destination) const;

    VoiceParams params;

    WavetableOscillator osc1Unison[VoiceParams::maxUnison];
    WavetableOscillator osc2Unison[VoiceParams::maxUnison];
    WavetableOscillator subUnison[VoiceParams::maxUnison];
    KarplusStrong string1Unison[VoiceParams::maxUnison];
    KarplusStrong string2Unison[VoiceParams::maxUnison];
    KarplusStrong subStrings[VoiceParams::maxUnison];
    SamplePlayer sample1Unison[VoiceParams::maxUnison];
    SamplePlayer sample2Unison[VoiceParams::maxUnison];
    SamplePlayer subSamples[VoiceParams::maxUnison];
    double sampleRatio1[VoiceParams::maxUnison] {}, sampleRatio2[VoiceParams::maxUnison] {};
    double sampleRatioSub[VoiceParams::maxUnison] {};
    ResonatorBank resonatorL, resonatorR;

    FilterUnit filter1L, filter1R, filter2L, filter2R;

    TensionAdsr ampEnv, filterEnv, filter2Env, modEnv, env4;
    juce::Random random;

    juce::SmoothedValue<float> frameSmooth1, frameSmooth2, subFrameSmooth;
    juce::SmoothedValue<float> levelSmooth1, levelSmooth2;
    juce::SmoothedValue<float> subSmooth, noiseSmooth;
    juce::SmoothedValue<float> osc1EnableSmooth, osc2EnableSmooth, subEnableSmooth;

    double sampleRate = 44100.0;
    double baseFrequency = 440.0;
    double currentFrequency = 440.0;
    double bendSemitones = 0.0;
    float velocityLevel = 1.0f;
    float keyTrackValue = 0.0f;
    float keyTrackOctaves = 0.0f;
    float randomValue = 0.0f;
    float modWheel = 0.0f;
    float aftertouchValue = 0.0f;
    float expressionValue = 1.0f;

    float lastAmpValue = 0.0f;
    float lastSamplePosition1 = -1.0f;
    float lastSamplePosition2 = -1.0f;
    float lastSamplePositionSub = -1.0f;
    float lastFilterValue = 0.0f;
    float lastFilter2Value = 0.0f;
    float lastModValue = 0.0f;
    float lastEnv4Value = 0.0f;

    int numOsc1Unison = 1;
    int numOsc2Unison = 1;
    float panGain1L[VoiceParams::maxUnison] {}, panGain1R[VoiceParams::maxUnison] {};
    float panGain2L[VoiceParams::maxUnison] {}, panGain2R[VoiceParams::maxUnison] {};
    float panGainSubL[VoiceParams::maxUnison] {}, panGainSubR[VoiceParams::maxUnison] {};
    int numSubUnison = 1;
    float glideCoeff = 1.0f;
    bool hasPlayedNote = false;

    float previousOsc1 = 0.0f;
    float previousOsc2 = 0.0f;
    float driftValue = 0.0f;
    float driftTarget = 0.0f;
    juce::Random driftRandom;
    bool lastStringMode1 = false;
    bool lastStringMode2 = false;
    bool lastSubStringMode = false;
    bool lastSampleMode1 = false;
    bool lastSampleMode2 = false;
    bool lastSubSampleMode = false;
    bool lastEnabled1 = true;
    bool lastEnabled2 = false;
    bool lastEnabledSub = true;
    int lastUnison1 = 1;
    int lastUnison2 = 1;
    int lastUnisonSub = 1;
    float voicePan = 0.0f;

    bool monoPending = false;
    bool monoLegato = false;
    bool monoKeepRunning = false;
    bool monoGlide = true;
};
