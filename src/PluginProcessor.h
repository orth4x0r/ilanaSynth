#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <vector>

#include "dsp/GranularPitchShift.h"
#include "dsp/GranularSmear.h"
#include "dsp/Mseg.h"
#include "dsp/SpectralFreeze.h"
#include "dsp/Svf.h"
#include "dsp/Modulation.h"
#include "dsp/SamplePlayer.h"
#include "dsp/Wavetable.h"

class IlanaSynthAudioProcessor : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
{
public:
    static constexpr int numUserSlots = 4;
    static constexpr int numFxSlots = 10;
    static constexpr int numLfos = 4;
    float getFxMod (Mod::Destination destination, float depth) const
    {
        return modDisplayValues[(size_t) destination].load() * depth;
    }
    float getCompGainReduction() const { return compGainReduction.load(); }
    float getFxSlotCpu (int slot) const { return fxSlotCpu[(size_t) juce::jlimit (0, numFxSlots - 1, slot)].load(); }
    // Puts a module type into an FX slot and switches on the module's own
    // enable flag, so a freshly added effect is audible straight away.
    void assignFxSlot (int slot, int type);
    void randomizeFxChain();
    bool saveFxChainToFile (const juce::File& file);
    bool loadFxChainFromFile (const juce::File& file);
    void loadReverbIr (const juce::File& file);
    void switchFxChain();
    void copyFxChainToOtherBank();
    bool isShowingChainA() const { return showingChainA; }
    IlanaSynthAudioProcessor();
    ~IlanaSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    const Wavetable* getWavetable (int index) const { return getTableForChoice (index); }

    float getModDisplay (Mod::Destination destination) const
    {
        return modDisplayValues[(size_t) destination].load();
    }

    float getLfoPhase (int index) const
    {
        return lfoPhaseDisplays[(size_t) juce::jlimit (0, numLfos - 1, index)].load();
    }
    float getDisplayPhase() const { return displayPhase.load(); }
    float getDisplayFrequency() const { return displayFrequency.load(); }
    float getSamplePosition (int oscIndex) const
    {
        return displaySamplePositions[(size_t) juce::jlimit (0, 2, oscIndex)].load();
    }

    float getWavetablePhase (int oscIndex) const
    {
        return oscDisplayPhases[(size_t) juce::jlimit (0, 2, oscIndex)].load();
    }
    double getCurrentBpm() const { return currentBpm.load(); }
    float getArpStepRateHz() const;
    float getSourceDisplayValue (int sourceIndex) const;
    void setOversampling (bool shouldOversample);
    bool isOversampling() const { return oversamplingActive.load(); }
    double getCurrentSampleRate() const { return displaySampleRate.load(); }

    juce::UndoManager& getUndoManager() { return undoManager; }

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getFactoryPresetCategories() const;
    juce::Array<juce::File> getUserPresetFiles() const;
    juce::StringArray getAllPresetNames() const;
    juce::StringArray getAllPresetCategories() const;
    int getNumAllPresets() const;
    void loadPresetByIndex (int index);
    void loadFactoryPreset (int index);
    bool savePresetToFile (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);

    // The loaded preset's name lives in the state tree so it survives editor
    // re-opens and host session reloads. Message thread only.
    juce::String getCurrentPresetName() const { return apvts.state.getProperty ("presetName").toString(); }
    void setCurrentPresetName (const juce::String& name) { apvts.state.setProperty ("presetName", name, nullptr); }

    bool assignModSlot (int sourceIndex, Mod::Destination destination, float depth);
    bool clearModSlotsForTarget (Mod::Destination destination);

    static constexpr int scopeSize = 4096;
    void copyScopeData (float* left, float* right, int numSamples) const;

    static constexpr int lfoDrawSteps = 64;
    void setLfoCustomPoint (int lfoIndex, int step, float value);
    float getLfoCustomPoint (int lfoIndex, int step) const;

    void triggerPreviewNote (int midiNote, bool isOn, float velocity = 0.7f);
    void panic() { synth.allNotesOff (0, false); }
    float getCpuUsage() const { return cpuUsage.load(); }
    int getActiveVoiceCount() const { return activeVoiceCount.load(); }
    void startMacroLearn (int macroIndex);
    void cancelMacroLearn();
    int getMacroLearnTarget() const { return macroLearn.load(); }
    int getMacroCc (int macroIndex) const { return macroCc[juce::jlimit (0, 3, macroIndex)].load(); }
    juce::File getUserPresetDirectory() const;
    float getEnvMonitorAmp() const { return envMonitorAmp.load(); }
    float getEnvMonitorFilter() const { return envMonitorFilter.load(); }
    float getEnvMonitorFilter2() const { return envMonitorFilter2.load(); }
    float getEnvMonitorMod() const { return envMonitorMod.load(); }
    float getEnvMonitorEnv4() const { return envMonitorEnv4.load(); }

    bool loadUserWavetable (int slot, const juce::File& file);
    bool loadUserSample (int oscIndex, const juce::File& file);
    const SampleData* getSampleForOsc (int oscIndex) const;
    void flushAsyncUpdates();

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

private:
    void handleAsyncUpdate() override;

    float getParam (const char* id) const;
    const Wavetable* getTableForChoice (int choiceIndex) const;
    void renderLfos (int numSamples, const juce::MidiBuffer& midiMessages);
    float staticSourceValue (int sourceIndex) const;
    void processEffects (juce::AudioBuffer<float>& buffer);
    void processAmp (juce::AudioBuffer<float>& buffer);
    void processDrive (juce::AudioBuffer<float>& buffer);
    void processCrush (juce::AudioBuffer<float>& buffer);
    void processCompressor (juce::AudioBuffer<float>& buffer);
    void processComb (juce::AudioBuffer<float>& buffer);
    void processPhaser (juce::AudioBuffer<float>& buffer);
    void processChorus (juce::AudioBuffer<float>& buffer);
    void processHaas (juce::AudioBuffer<float>& buffer);
    void processDelay (juce::AudioBuffer<float>& buffer);
    void processStutter (juce::AudioBuffer<float>& buffer);
    void processSmear (juce::AudioBuffer<float>& buffer);
    void processFreeze (juce::AudioBuffer<float>& buffer);
    void processReverb (juce::AudioBuffer<float>& buffer);
    void processFlanger (juce::AudioBuffer<float>& buffer);
    void processDimension (juce::AudioBuffer<float>& buffer);
    void processGate (juce::AudioBuffer<float>& buffer);
    void processTapeStop (juce::AudioBuffer<float>& buffer);
    void processTilt (juce::AudioBuffer<float>& buffer);
    void processUtility (juce::AudioBuffer<float>& buffer);
    void processOtt (juce::AudioBuffer<float>& buffer);
    void processLimiter (juce::AudioBuffer<float>& buffer);
    void processWidener (juce::AudioBuffer<float>& buffer);
    void processTremolo (juce::AudioBuffer<float>& buffer);
    void processFreqShift (juce::AudioBuffer<float>& buffer);
    void processRingMod (juce::AudioBuffer<float>& buffer);
    void processOctaver (juce::AudioBuffer<float>& buffer);
    void processVowel (juce::AudioBuffer<float>& buffer);
    void processFeedback (juce::AudioBuffer<float>& buffer);
    void processSlot (int type, juce::AudioBuffer<float>& buffer);
    juce::String captureFxChain();
    juce::ValueTree buildFullState();
    void applyFullState (const juce::ValueTree& state);
    void applyFxChain (const juce::String& state);
    void processArpeggiator (juce::MidiBuffer& midiMessages, int numSamples, juce::MidiBuffer& output);
    int selectArpNote (int mode, int octaves);

    juce::Synthesiser synth;

    juce::MidiBuffer midiForSynth;
    juce::Array<int> arpHeldNotes;
    juce::Array<int> arpChordActive;
    juce::Array<int> arpChordNotes;
    juce::Random arpRandom;
    int arpStepIndex = 0;
    int arpDirection = 1;
    int arpCounter = 0;
    int arpGateRemaining = 0;
    int arpActiveNote = -1;
    bool arpWasEnabled = false;

    int crushCounter = 0;
    float crushHold[2] { 0.0f, 0.0f };
    float delayDampState[2] { 0.0f, 0.0f };

    juce::AudioBuffer<float> stutterBuffer;
    int stutterLength = 0;
    int stutterWrite = 0;
    int stutterRead = 0;
    bool stutterRecording = false;
    bool stutterWasOn = false;

    std::vector<std::shared_ptr<Wavetable>> userTables;
    std::array<std::shared_ptr<Wavetable>, (size_t) (numUserSlots * 3)> retiredTables;
    std::array<int, (size_t) numUserSlots> retiredTableIndex {};
    void resetUserTableToDefault (int slot);
    mutable juce::SpinLock tableLock;

    std::array<std::array<juce::String, 5>, 3> stringParamIds;
    std::array<std::array<juce::String, 7>, 3> sampleParamIds;

    static constexpr int numSampleOscs = 3;
    std::vector<std::shared_ptr<SampleData>> sampleSlots;
    std::array<std::shared_ptr<SampleData>, 5> factorySamples;
    std::array<std::array<std::shared_ptr<SampleData>, 3>, (size_t) numSampleOscs> retiredSamples;
    std::array<int, (size_t) numSampleOscs> retiredIndex {};
    mutable juce::SpinLock sampleLock;
    mutable juce::SpinLock stateLock;
    std::array<juce::String, (size_t) numSampleOscs> samplePaths;
    std::array<juce::String, (size_t) numSampleOscs> pendingSamplePaths;
    std::array<bool, (size_t) numSampleOscs> pendingSampleClear {};
    bool samplesReloadPending = false;
    std::array<juce::String, (size_t) numUserSlots> userTablePaths;
    std::array<juce::String, (size_t) numUserSlots> pendingUserTablePaths;
    std::array<bool, (size_t) numUserSlots> pendingUserTableClear {};
    bool userTablesReloadPending = false;

    juce::AudioBuffer<float> lfoBuffers;
    std::array<double, (size_t) numLfos> lfoPhases {};
    std::array<std::atomic<float>, (size_t) numLfos> lfoSampleHolds {};
    juce::Random lfoRandom;

    float modWheelValue = 0.0f;
    float aftertouchValue = 0.0f;
    float expressionValue = 1.0f;
    std::atomic<float> modWheelDisplay { 0.0f };
    std::atomic<float> aftertouchDisplay { 0.0f };
    std::atomic<float> expressionDisplay { 1.0f };
    std::atomic<float> clockShDisplay { 0.0f };

    double currentSampleRate = 44100.0;
    double baseSampleRate = 44100.0;
    int expectedBlockSize = 512;
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<double> displaySampleRate { 44100.0 };

    std::array<std::atomic<float>, (size_t) Mod::Destination::Count> modDisplayValues {};
    std::array<std::atomic<float>, (size_t) numLfos> lfoPhaseDisplays {};
    std::atomic<float> displayPhase { 0.0f };
    std::atomic<float> displayFrequency { 0.0f };
    std::array<std::atomic<float>, 3> displaySamplePositions {};
    std::array<std::atomic<float>, 3> oscDisplayPhases {};

    std::vector<float> scopeLeft, scopeRight;
    std::atomic<int> scopeWritePos { 0 };
    mutable juce::SpinLock scopeLock;

    std::array<std::array<float, lfoDrawSteps>, (size_t) numLfos> lfoCustom {};
    std::array<std::array<float, lfoDrawSteps>, (size_t) numLfos> activeLfoCustom {};
    mutable juce::SpinLock lfoShapeLock;

    std::atomic<int> previewNoteOn { -1 };
    std::atomic<int> previewNoteOff { -1 };
    std::atomic<float> previewVelocity { 0.7f };
    std::atomic<float> cpuUsage { 0.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> macroLearn { -1 };
    std::atomic<int> macroCc[4] {};

    std::atomic<int> pendingProgramChange { -1 };
    std::atomic<float> pendingMacros[4] {};
    std::atomic<bool> macrosPending { false };
    std::atomic<float> envMonitorAmp { 0.0f };
    std::atomic<float> envMonitorFilter { 0.0f };
    std::atomic<float> envMonitorFilter2 { 0.0f };
    std::atomic<float> envMonitorMod { 0.0f };
    std::atomic<float> envMonitorEnv4 { 0.0f };

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Convolution convolution;
    juce::dsp::Oversampling<float> oversampler { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    std::atomic<bool> oversamplingActive { false };
    juce::MidiBuffer scaledMidiBuffer;
    std::atomic<bool> reverbIrLoaded { false };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> combLine { 96000 };
    juce::Reverb reverb;

    GranularPitchShift tapeShift[2];
    GranularPitchShift shimmerShift[2];
    GranularSmear smear[2];
    SpectralFreeze freeze[2];
    Mseg mseg;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> haasLine { 4800 };
    float compEnvelope[2] { 0.0f, 0.0f };
    float ampLowState[2] { 0.0f, 0.0f };
    float ampHighState[2] { 0.0f, 0.0f };

    double wowPhase = 0.0;
    double clockShPhase = 0.0;
    float clockShValue = 0.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> flangerLine { 4800 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dimLine { 9600 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> springComb { 4800 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> feedbackLine { 9600 };
    float feedbackState[2] {};
    double stutterPosition = 0.0;
    float stutterRate = 1.0f;
    std::array<std::atomic<float>, numFxSlots> fxSlotCpu {};
    juce::String chainA, chainB;
    bool chainBValid = false;
    bool showingChainA = true;
    double flangerPhase = 0.0;
    double dimPhase = 0.0;
    double tremoloPhase = 0.0;
    double shifterPhase = 0.0;
    double ringPhase = 0.0;
    double gatePhase = 0.0;
    float gateEnvelope = 1.0f;
    juce::uint16 gateRandomMask = 0xFFFF;
    int gateCycleCount = 8;
    float tapeStopRate = 1.0f;
    juce::AudioBuffer<float> tapeStopBuffer;
    int tapeStopWrite = 0;
    double tapeStopRead = 0.0;
    float tiltLowState[2] {};
    float tiltHighState[2] {};
    float ottLowState[2][2] {};
    float ottHighState[2][2] {};
    float ottEnvelope[2][3] {};
    float limiterEnvelope[2] {};
    float shifterAllpass[2] {};
    float shifterDelay[2] {};
    float gatedReverbEnvelope = 0.0f;
    float duckEnvelope = 0.0f;
    GranularPitchShift octaverShift[2];
    Svf vowelFilters[2][3];
    juce::AudioBuffer<float> fxScratch;
    juce::AudioBuffer<float> reverbScratch;
    std::atomic<float> compGainReduction { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IlanaSynthAudioProcessor)
};
