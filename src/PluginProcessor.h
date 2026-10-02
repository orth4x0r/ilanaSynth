#pragma once

#include <optional>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "dsp/Biquad.h"
#include "dsp/Generative.h"
#include "dsp/GranularPitchShift.h"
#include "dsp/GranularSmear.h"
#include "dsp/IlanaSynth.h"
#include "dsp/KarplusStrong.h"
#include "MtsEsp.h"
#include "dsp/LfoCurve.h"
#include "dsp/LfoShape.h"
#include "dsp/Evolve.h"
#include "dsp/Mseg.h"
#include "dsp/SpectralFreeze.h"
#include "dsp/Svf.h"
#include "dsp/SympatheticStrings.h"
#include "dsp/AcousticKeys.h"
#include "dsp/Modulation.h"
#include "dsp/OscillatorIds.h"
#include "dsp/airwindows/AirwindowsModule.h"
#include "dsp/airwindows/Categories.h"
#include "dsp/Vocoder.h"
#include "dsp/SamplePlayer.h"
#include "dsp/SpectralCache.h"
#include "dsp/Wavetable.h"
#include "dsp/WavetableDoc.h"
#include "TuningState.h"
#include "ClipState.h"

class IlanaSynthAudioProcessor : public juce::AudioProcessor,
                                 private juce::AsyncUpdater
{
    // First member, so it runs before any string is built: the strings'
    // seeds start over for each instance, and every instance renders alike.
    struct StringSeedStart
    {
        StringSeedStart() { KarplusStrong::restartSeeds(); }
    } stringSeedStart;

public:
    // M7.4: 16 patch tables (was 4 user slots; the choices were appended).
    static constexpr int numUserSlots = 16;
    static constexpr int numFxSlots = 10;
    static constexpr int numFxTypes = 41; // 30: Airwindows, 31: Vocoder, 32-41: Airwindows categories

    EqSettings getEqSettings() const;
    static constexpr int numLfos = Mod::numLfoSources;
    // LFO 1-4 keep lfoBuffers channels 0-3; the clocked S&H and MSEG sit at
    // 4 and 5, and LFO 5-16 follow.
    static constexpr int lfoChannel (int lfo) { return lfo < 4 ? lfo : lfo + 2; }
    // M8.1: each LFO's output B follows, from channel numLfos + 2.
    static constexpr int lfoChannelB (int lfo) { return numLfos + 2 + lfo; }
    static constexpr int numLfoChannels = 2 * numLfos + 2;
    static constexpr int maxDestinations = 512;

    float getFxMod (Mod::Destination destination, float depth) const
    {
        return modDisplayValues[(size_t) destination].load() * depth;
    }
    float getCompGainReduction() const { return compGainReduction.load(); }
    // Display only (the FX cards' meters): the limiter's deepest gain in the
    // last block, and OTT's LOW / MID / HIGH gains at the block's end.
    float getLimiterGainReduction() const { return limiterGainReduction.load(); }
    float getOttBandGain (int band) const { return ottBandGain[(size_t) juce::jlimit (0, 2, band)].load(); }
    float getFxSlotCpu (int slot) const { return fxSlotCpu[(size_t) juce::jlimit (0, numFxSlots - 1, slot)].load(); }
    // Puts a module type into an FX slot and switches on the module's own
    // enable flag, so a freshly added effect is audible straight away.
    void assignFxSlot (int slot, int type);
    // Makes an Airwindows algorithm ahead of its use (the editor picks one).
    void preloadAirwindows (int algorithm) { airwindowsModule.preload (algorithm); }
    void preloadAirwindowsCategory (int category, int choice);
    void randomizeFxChain();
    bool saveFxChainToFile (const juce::File& file);
    bool loadFxChainFromFile (const juce::File& file);
    void loadReverbIr (const juce::File& file);
    void switchFxChain();
    void copyFxChainToOtherBank();
    bool isShowingChainA() const { return showingChainA; }
    IlanaSynthAudioProcessor();
    ~IlanaSynthAudioProcessor() override;

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
    bool isSpectralWarpReady (int osc) const { return spectralCache->isReady (osc); }
    float getLfoLiveValue (int lfo) const { return lfoLastValues[(size_t) juce::jlimit (0, numLfos - 1, lfo)].load(); }
    // M8.1: a simulated LFO shape's settings (the card's picture reads them too).
    LfoSimSettings readLfoSimSettings (int lfo) const;
    // M8.5: Evolve and the vector pad.
    static constexpr int numVectorPoints = 8;
    float macroValue (int macro) const;
    static std::array<float, 4> vectorWeights (float x, float y);
    juce::Point<float> getVectorPosition() const { return { vectorX.load(), vectorY.load() }; }
    int getVectorCorner (int corner) const;
    bool isVectorPathOn() const;
    juce::Point<float> getVectorPathPoint (int point) const;
    float getMacroDrift (int macro) const { return macroDrift[(size_t) juce::jlimit (0, Mod::numMacros - 1, macro)].load(); }
    void freezeEvolve();

    // M8.3: the loudest voice's WEST gate conductance, for the card.
    float getWestGateLevel() const { return westGateDisplay.load(); }
    float getLfoLiveValueB (int lfo) const { return lfoLastValuesB[(size_t) juce::jlimit (0, numLfos - 1, lfo)].load(); }

    // The spectrally warped table an oscillator is playing, for display
    // (null when its warp is off or still building).
    std::shared_ptr<const Wavetable> getSpectralDisplayTable (int osc, int tableChoice) const
    {
        static const char* ids[] { "osc1_spectral", "osc2_spectral", "sub_spectral" };
        const auto* mode = apvts.getRawParameterValue (ids[juce::jlimit (0, 2, osc)]);
        return spectralCache->getForDisplay (juce::jlimit (0, 2, osc), tableChoice, mode != nullptr ? (int) mode->load() : 0);
    }

    // Synth-wide modulation of a destination this block (loudest voice for
    // per-voice sources), for knob rings and the effects.
    float getModDisplay (int destination) const
    {
        return juce::isPositiveAndBelow (destination, maxDestinations) ? modDisplayValues[(size_t) destination].load() : 0.0f;
    }

    float getModDisplay (Mod::Destination destination) const { return getModDisplay ((int) destination); }

    // Reads one mod slot's settings from the parameters (message thread).
    Mod::Slot readModSlot (int slotIndex) const;
    int getNumUsedModSlots() const;

    float getLfoPhase (int index) const
    {
        return lfoPhaseDisplays[(size_t) juce::jlimit (0, numLfos - 1, index)].load();
    }
    float getDisplayPhase() const { return displayPhase.load(); }
    float getDisplayFrequency() const { return displayFrequency.load(); }
    float getSamplePosition (int oscIndex) const
    {
        return displaySamplePositions[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)].load();
    }

    float getWavetablePhase (int oscIndex) const
    {
        return oscDisplayPhases[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)].load();
    }
    double getCurrentBpm() const { return currentBpm.load(); }
    float getArpStepRateHz() const;
    float getSourceDisplayValue (int sourceIndex) const;
    // 1 (off), 2 or 4. Switching reprepares the voices, so it happens on
    // the message thread (the audio thread asks for it asynchronously).
    void setOversampling (int factor);
    bool isOversampling() const { return oversamplingFactor.load() > 1; }
    int getOversamplingFactor() const { return oversamplingFactor.load(); }
    double getCurrentSampleRate() const { return displaySampleRate.load(); }

    juce::UndoManager& getUndoManager() { return undoManager; }

    // Undo (UI review 4): every edit made by a gesture other than a knob
    // drag (JUCE's attachments start those) is one named undo step.
    // beginEdit starts the step at the gesture's start; endEdit closes it at
    // its end. Edits to data that isn't a parameter (drawn LFO steps and
    // curves, remap curves, clips) made in between join the step as one
    // action that puts the data back through the same setters. performEdit
    // wraps a one-shot edit (a menu item, a double-click). Message thread only.
    void beginEdit (const juce::String& name);
    void endEdit();
    void performEdit (const juce::String& name, const std::function<void()>& edit)
    {
        beginEdit (name);
        edit();
        endEdit();
    }
    // A hash of that data, for the editor's EDITED marker (cached until the
    // data epoch moves).
    juce::int64 getPatchDataHash() const;

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getFactoryPresetCategories() const;
    juce::Array<juce::File> getUserPresetFiles() const;
    // The four macro names a factory preset loads with (its own, the
    // voicing's, then the automatic ones), worked out without loading it;
    // an empty string is a macro with no name.
    juce::StringArray getFactoryMacroNames (int factoryIndex) const;
    juce::StringArray getAllPresetNames() const;
    juce::StringArray getAllPresetCategories() const;
    juce::StringArray getAllPresetTags() const;
    bool isUserPreset (int index) const { return index >= (int) getFactoryPresetNames().size(); }
    int getNumAllPresets() const;
    void loadPresetByIndex (int index);

    // Category and tags travel with the patch (state tree) and are read back
    // from user preset files for the browser. Message thread only.
    static juce::StringArray getPresetCategoryChoices()
    {
        return { "Bass", "Lead", "Pluck", "Pad", "Keys", "Chords", "Arp", "Drone", "FX", "Other" };
    }

    void setPresetMeta (const juce::String& category, const juce::String& tags)
    {
        apvts.state.setProperty ("presetCategory", category, nullptr);
        apvts.state.setProperty ("presetTags", tags, nullptr);
    }

    juce::String getPresetCategory() const { return apvts.state.getProperty ("presetCategory").toString(); }
    juce::String getPresetTags() const { return apvts.state.getProperty ("presetTags").toString(); }
    void loadFactoryPreset (int index);
    // The critic's trims (src/PresetTrims.h) for a factory preset: its level,
    // and the depth of its macros' routings. ILANA_NO_TRIMS turns them off
    // (for the tuning tool's own renders).
    // The diversity pass's redesigns (src/PresetVoicing.h): parameter
    // changes and macro rewiring over the recipe, applied before the trims.
    // Off in tests that check what a recipe itself loads.
    static inline bool presetVoicingEnabled = true;
    static void applyPresetVoicing (const char* presetName, std::vector<std::pair<juce::String, float>>& values,
                                    std::array<juce::String, 4>& macroNames);
    static void applyPresetTrims (const char* presetName, const juce::String& category,
                                  std::vector<std::pair<juce::String, float>>& values);
    bool savePresetToFile (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);

    // The loaded preset's name lives in the state tree so it survives editor
    // re-opens and host session reloads. Message thread only.
    juce::String getCurrentPresetName() const { return apvts.state.getProperty ("presetName").toString(); }
    void setCurrentPresetName (const juce::String& name) { apvts.state.setProperty ("presetName", name, nullptr); }

    // Macro names travel with the patch (state tree), so presets and host
    // sessions keep them. Message thread only.
    juce::String getMacroName (int macroIndex) const
    {
        const auto name = apvts.state.getProperty ("macroName" + juce::String (macroIndex + 1)).toString();
        return name.isNotEmpty() ? name : "Macro " + juce::String (macroIndex + 1);
    }

    void setMacroName (int macroIndex, const juce::String& name)
    {
        ++dataEpoch;
        apvts.state.setProperty ("macroName" + juce::String (macroIndex + 1), name, nullptr);
    }

    // Routes a source to a destination in the first free slot; returns the
    // slot index or -1 when all slots are taken.
    int assignModSlot (int sourceIndex, int destination, float depth);

    // Gives a patch without macro mappings a sensible set (tone, timbre,
    // drive, space), chosen from what the patch uses. Silent at macro 0.
    // Macros marked in keep (the preset voicing's own) are left alone.
    void applyDefaultMacros (const std::array<bool, 4>& keep = {});

    static void migrateLegacyOsc3 (const std::function<float (const juce::String&, float)>& get,
                                   const std::function<void (const juce::String&, float)>& set);
    bool clearModSlotsForTarget (int destination);
    void clearModSlot (int slotIndex);
    // Two slots routing the same source to the same destination (with the
    // same via, polarity, curve and bypass, and no drawn remap) play as one
    // slot with the depths added, so they can be merged without changing
    // the sound. canMergeModSlots says why not when they can't be (they
    // differ, or the sum would pass 100%). mergeModSlots folds `from` into
    // `into`; mergeDuplicateModSlots merges every such pair (factory presets
    // load merged) and returns how many slots it freed. Message thread.
    bool canMergeModSlots (int into, int from, juce::String* reason = nullptr) const;
    bool mergeModSlots (int into, int from);
    int mergeDuplicateModSlots();
    void setModSlotValue (int slotIndex, const juce::String& field, float value);
    juce::String getModSlotParamId (int slotIndex, const juce::String& field) const;

    static constexpr int scopeSize = 4096;
    void copyScopeData (float* left, float* right, int numSamples) const;

    static constexpr int lfoDrawSteps = 64;
    void setLfoCustomPoint (int lfoIndex, int step, float value);
    float getLfoCustomPoint (int lfoIndex, int step) const;

    // The drawable "Curve" LFO shape (message thread).
    static constexpr int curveShape = 8;
    LfoCurve getLfoCurve (int lfoIndex) const;
    void setLfoCurve (int lfoIndex, const LfoCurve& curve);

    // A drawn remap curve per mod slot (Vital's per-route remap). A straight
    // line from -1 to 1 is off; the slot then shapes as before.
    static LfoCurve identityRemap() { LfoCurve curve; curve.points = { { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; return curve; }
    static bool isIdentityRemap (const LfoCurve& curve);
    LfoCurve getModRemap (int slotIndex) const;
    bool isModRemapOn (int slotIndex) const;
    void setModRemap (int slotIndex, const LfoCurve& curve);
    void resetModRemap (int slotIndex) { setModRemap (slotIndex, identityRemap()); }
    void resetAllModRemaps();
    float getLfoCurveValue (int lfoIndex, double phase) const;

    void triggerPreviewNote (int midiNote, bool isOn, float velocity = 0.7f);
    void panic() { synth.allNotesOff (0, false); }
    float getCpuUsage() const { return cpuUsage.load(); }

    // The loudest output sample per channel since the last call (for the meter).
    float takeOutputPeak (int channel) { return outputPeaks[(size_t) juce::jlimit (0, 1, channel)].exchange (0.0f); }
    // For views that animate with the playing (the PHYSICAL page): notes
    // started so far, and the last block's peak (not reset by reading).
    unsigned getNoteOnCount() const { return noteOnCount.load(); }
    float getOutputPeak() const { return outputLevelDisplay.load(); }
    int getActiveVoiceCount() const { return activeVoiceCount.load(); }

    // Changes whenever something the editor draws may have changed: any
    // parameter, an edit to data that isn't a parameter (LFO curves and
    // drawn steps, tables, samples, macro names, a restored state), a table
    // or reveal change, a new note, or a block rendered while something
    // sounds (voices, output or input). Views compare it to
    // skip repainting while nothing moves.
    // For the editor's EDITED marker: the preset name and the parameters'
    // fingerprint when it was loaded or saved (kept here so it outlives the
    // editor). Message thread only.
    std::optional<std::pair<juce::String, juce::int64>> loadedPresetFingerprint;

    unsigned getDataEpoch() const { return dataEpoch.load(); }
    // Counts sample loads (a new sample can reuse a freed one's address).
    unsigned getSampleEpoch() const { return sampleEpoch.load(); }

    juce::uint64 getUiEpoch() const
    {
        return (juce::uint64) paramEpoch.load() + ((juce::uint64) liveEpoch.load() << 32)
             + (juce::uint64) dataEpoch.load() * 15485863u
             + (juce::uint64) revealVersion.load() * 7919u + (juce::uint64) tableNoticeVersion.load() * 104729u
             + (juce::uint64) noteOnCount.load() * 1299709u;
    }

    // LFO phase of every sounding voice (for tests and diagnostics).
    std::vector<float> getVoiceLfoPhasesForTest (int lfo)
    {
        std::vector<float> phases;

        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* voice = dynamic_cast<Voice*> (synth.getVoice (i)))
                if (voice->isVoiceActive())
                    phases.push_back (voice->getLfoPhase (lfo));

        return phases;
    }
    void startMacroLearn (int macroIndex);
    void cancelMacroLearn();
    int getMacroLearnTarget() const { return macroLearn.load(); }
    int getMacroCc (int macroIndex) const { return macroCc[juce::jlimit (0, Mod::numMacros - 1, macroIndex)].load(); }
    // MIDI learn for any automatable parameter (macros keep their own CCs
    // above): the next controller moved drives it, one CC per parameter.
    // Saved with the patch as "midiCcMap"; a state without it (and a factory
    // preset) leaves the current map alone, as it belongs to the controller.
    void startParamLearn (const juce::String& parameterId);
    void cancelParamLearn();
    juce::String getParamLearnTarget() const;
    int getParamCc (const juce::String& parameterId) const; // -1: none
    void clearParamCc (const juce::String& parameterId);
    juce::File getUserPresetDirectory() const;
    // Points the user preset folder elsewhere (the UI test's temporary
    // folder, so it never touches the user's own presets). Empty: the default.
    static inline juce::File userPresetDirectoryOverride;
    float getEnvMonitorAmp() const { return envMonitorAmp.load(); }
    float getEnvMonitorFilter() const { return envMonitorFilter.load(); }
    float getEnvMonitorFilter2() const { return envMonitorFilter2.load(); }
    float getEnvMonitorMod() const { return envMonitorMod.load(); }
    float getEnvMonitorEnv4() const { return envMonitorEnv4.load(); }
    float getEnvMonitorExtra (int index) const { return envMonitorExtra[(size_t) juce::jlimit (0, 10, index)].load(); }
    // ENV 1-16 (amp, filter, filter 2, mod, ENV 5, ENV 6-16) of the voice the
    // monitors follow: stage plus progress (TensionAdsr::getDisplayPosition).
    float getEnvMonitorPosition (int env) const { return envMonitorPositions[(size_t) juce::jlimit (0, 15, env)].load(); }
    // The MSEG's place in its cycle (0..1), for its playhead.
    float getMsegPhase() const { return msegPhaseDisplay.load(); }

    // Which optional modules the patch shows, Phase Plant style: a few by
    // default and a "+" to add more. Saved with the patch; the UI also always
    // shows a module that is in use.
    enum class Module { Oscillator = 0, Envelope, Lfo };
    static constexpr int defaultRevealMask = 0b111;
    bool isRevealed (Module kind, int index) const
    {
        return ((revealMasks[(size_t) kind].load() >> index) & 1) != 0;
    }
    void setRevealed (Module kind, int index, bool shouldShow)
    {
        auto& mask = revealMasks[(size_t) kind];
        const auto bit = 1 << index;
        mask.store (shouldShow ? (mask.load() | bit) : (mask.load() & ~bit));
        ++revealVersion;
    }
    // Bumped whenever a mask changes (including on patch load) so editors can
    // relayout without listening to every parameter.
    int getRevealVersion() const { return revealVersion.load(); }
    // Shows an oscillator and switches it on, as adding one should sound.
    void addOscillator (int index);
    void removeOscillator (int index);
    bool isOscillatorShown (int index) const;

    // M5: sets the FM matrix and the operators' outputs to one of the
    // FmAlgorithms (adding the operators it needs). Existing routes keep
    // their amounts. Message thread.
    void applyFmAlgorithm (int index);
    // The algorithm the current routing matches, or -1.
    int findMatchingFmAlgorithm() const;
    // [source][target] FM parameter id, 0-based.
    static juce::String fmRouteId (int source, int target);
    // An operator's sounding ratio after SNAP (for display).
    double getSnappedRatio (int osc) const;
    // Added to the patch, or routed in the matrix (message thread).
    bool isLfoShown (int index) const;
    // Colours for LFO cards and chips: the first four as before, the rest
    // around the hue wheel.
    static juce::Colour lfoColour (int index);

    bool loadUserWavetable (int slot, const juce::File& file,
                            Wavetable::LoadMode mode = Wavetable::LoadMode::Automatic);

#if ILANA_FX
    static constexpr bool isEffectBuild = true;
#else
    static constexpr bool isEffectBuild = false;
#endif
    // M7.5: the input's level and envelope for the INPUT page (0..1).
    float getInputLevel() const { return inputLevelDisplay.load(); }
    float getInputEnvelope() const { return inputEnvDisplay.load(); }

    // M7.4 patch tables (message thread). setUserTable builds the table from
    // the doc and swaps it in; the doc is saved with the patch.
    bool setUserTable (int slot, const WavetableDoc& doc);
    // The slot's doc; a slot never edited gives its default (factory) table.
    WavetableDoc getUserTableDoc (int slot) const;
    bool isUserSlotEdited (int slot) const;
    // An unedited slot that no oscillator plays, or -1.
    int findFreeUserSlot() const;
    // Set when a saved table could not be restored (see WavetableDoc).
    juce::String getTableNotice() const;
    int getTableNoticeVersion() const { return tableNoticeVersion.load(); }
    void clearTableNotice();
    bool loadUserSample (int oscIndex, const juce::File& file);
    // The FX rack alone over a buffer (the audio path calls it; public for the tests).
    void processEffects (juce::AudioBuffer<float>& buffer);
    // Puts audio on an oscillator's sample slot. An embedded sample (a
    // bounce) is saved inside the patch; a file-backed one by its path.
    void setUserSample (int oscIndex, std::shared_ptr<SampleData> data, const juce::String& path);
    bool isSampleEmbedded (int oscIndex) const;
    const SampleData* getSampleForOsc (int oscIndex) const;
    void flushAsyncUpdates();

    // M8.6: resample to oscillator. The patch plays one note on a copy of
    // the processor, off the audio thread, and the result lands on an
    // oscillator: as its sample (Sample mode) or resynthesised into a patch
    // wavetable (Wavetable mode).
    struct BounceRequest
    {
        int targetOsc = 0;
        bool toTable = false;
        bool withFx = true;         // false: the voice alone, every effect off
        bool muteOthers = true;     // the other oscillators switched off after
        int note = 60, velocity = 100;
        double holdSeconds = 2.0, tailSeconds = 2.0;
    };
    enum class BounceState { Idle, Rendering, Done, Failed };
    // Message thread. False if a bounce is already running.
    bool startBounce (const BounceRequest& request);
    BounceState getBounceState() const { return bounceState.load(); }
    float getBounceProgress() const { return bounceProgress.load(); }
    juce::String getBounceMessage() const;
    // The render itself: the state playing one note, trimmed and
    // normalised. Null if silent or cancelled. Any thread.
    static std::shared_ptr<SampleData> renderBounce (const juce::ValueTree& state, const BounceRequest& request,
                                                     std::atomic<float>* progress = nullptr,
                                                     const std::atomic<bool>* cancel = nullptr);
    // Puts a render on the target oscillator (message thread).
    bool applyBounce (const BounceRequest& request, std::shared_ptr<SampleData> audio, juce::String& message);

    // Scala microtuning, saved in the patch (TuningState). Loading a scale
    // switches tuning_on on; reset goes back to 12-TET and switches it off.
    // Message thread; false (and an error) for a malformed file.
    bool loadTuningScale (const juce::String& sclText, juce::String& error);
    bool loadTuningMapping (const juce::String& kbmText, juce::String& error);
    void resetTuning();
    const TuningState& getTuningState() const { return tuningState; }
    // The clip sequencer's clips, saved in the patch (ClipState). Message thread.
    ClipState& getClipState() { return clipState; }
    void clipsEdited() { ++dataEpoch; }
    // The beat the clip has reached (0 to its length), or -1 when it isn't playing.
    float getClipPlayhead() const { return clipPlayhead.load(); }
    // MTS-ESP: a tuning master in the session overrides the Scala tuning.
    bool isMtsEspConnected() const { return mtsEsp.hasMaster(); }
    juce::String getMtsEspScaleName() const { return juce::String::fromUTF8 (mtsEsp.scaleName()); }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

private:
    TuningState tuningState;
    ClipState clipState;

    // The patch data that isn't a parameter, as one undo action sees it
    // (beginEdit / endEdit; State.cpp). Remaps that are off hold the
    // straight line.
    struct PatchData
    {
        std::array<std::array<float, lfoDrawSteps>, (size_t) numLfos> draws {};
        std::array<LfoCurve, (size_t) numLfos> curves;
        std::array<LfoCurve, (size_t) Mod::maxSlots> remaps;
        std::shared_ptr<const ClipState::Clips> clips;
    };
    struct PatchDataEdit;
    PatchData capturePatchData() const;
    // Puts back the parts of data that differ from reference.
    void restorePatchData (const PatchData& data, const PatchData& reference);
    static bool samePatchData (const PatchData& a, const PatchData& b);
    std::optional<PatchData> pendingEditData; // between beginEdit and endEdit
    mutable unsigned hashedDataEpoch = ~0u;
    mutable juce::int64 cachedDataHash = 0;

    MtsEspClient mtsEsp;
    Tuning mtsTuning; // filled from the master each block while one is connected
    void handleAsyncUpdate() override;

    // M8.6 bounce: the render thread hands its result over through these
    // (under stateLock) and the async update applies it.
    class BounceThread;
    std::unique_ptr<BounceThread> bounceThread;
    std::atomic<BounceState> bounceState { BounceState::Idle };
    std::atomic<float> bounceProgress { 0.0f };
    BounceRequest pendingBounce;
    std::shared_ptr<SampleData> bounceResult;
    bool bounceReady = false;
    juce::String bounceMessage;
    static juce::ValueTree encodeSample (const SampleData& data);
    static std::shared_ptr<SampleData> decodeSample (const juce::ValueTree& tree);

    struct ParamCacheEntry
    {
        const char* id = nullptr;
        std::uint32_t hash = 0;
        const std::atomic<float>* value = nullptr;
        int destination = -1;
        bool discrete = false; // a choice, int or bool: read back rounded
        float fallback = 0.0f; // the default, read in place of a non-finite value
    };

    // A parameter ID that remembers where its value lives: the first read
    // looks it up, later reads go straight to it. Assigned and passed
    // around like the juce::String it wraps.
    class ParamRef
    {
    public:
        ParamRef() = default;
        ParamRef (const juce::String& newId) : id (newId) {}
        ParamRef (const char* newId) : id (newId) {}
        ParamRef (const ParamRef& other) : id (other.id) {}

        ParamRef& operator= (const ParamRef& other)
        {
            id = other.id;
            entry.store (nullptr, std::memory_order_relaxed);
            return *this;
        }

        const char* toRawUTF8() const { return id.toRawUTF8(); }
        operator const juce::String&() const { return id; }
        operator juce::StringRef() const { return id; }

        juce::String id;
        mutable std::atomic<const ParamCacheEntry*> entry { nullptr };
    };

    float getParam (const char* id) const;
    float getParam (const ParamRef& ref) const;
    // The stored value without modulation (as apvts.getRawParameterValue).
    float getRawParam (const ParamRef& ref) const;
    const ParamCacheEntry* findParam (const char* id) const;
    float readParam (const ParamCacheEntry& entry) const;
    const Wavetable* getTableForChoice (int choiceIndex) const;
    void renderLfos (int numSamples, const juce::MidiBuffer& midiMessages);
    float staticSourceValue (int sourceIndex) const;
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
    void processAirwindows (juce::AudioBuffer<float>& buffer);
    void processVocoder (juce::AudioBuffer<float>& buffer);
    void processOtt (juce::AudioBuffer<float>& buffer);
    void processLimiter (juce::AudioBuffer<float>& buffer);
    void processWidener (juce::AudioBuffer<float>& buffer);
    void processTremolo (juce::AudioBuffer<float>& buffer);
    void processFreqShift (juce::AudioBuffer<float>& buffer);
    void processRingMod (juce::AudioBuffer<float>& buffer);
    void processOctaver (juce::AudioBuffer<float>& buffer);
    void processVowel (juce::AudioBuffer<float>& buffer);
    void processFeedback (juce::AudioBuffer<float>& buffer);
    void processEq (juce::AudioBuffer<float>& buffer);
    void processSlot (int type, juce::AudioBuffer<float>& buffer);
    juce::String captureFxChain();
    juce::ValueTree buildFullState();
    void applyFullState (const juce::ValueTree& state);
    void applyFxChain (const juce::String& state);
    void processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void updateLatency();
    juce::MidiBuffer chunkMidi;
    void processArpeggiator (juce::MidiBuffer& midiMessages, int numSamples, juce::MidiBuffer& output);
    int selectArpNote (int mode, int octaves);

    IlanaSynth synth;

    juce::MidiBuffer midiForSynth;
    juce::MidiBuffer generatedMidi;
    NoteSpray noteSpray;
    std::unique_ptr<SpectralCache> spectralCache;
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
    bool arpHostWasPlaying = false;
    // M7.1: probability sequencer and Euclid state. The sequencer has its
    // own random generator, so the arp's sequence is unchanged.
    juce::Random pseqRandom { 16180 };
    long long engineStepCount = 0;
    std::array<ParamRef, 16> pseqChanceIds, pseqRangeIds, pseqRatchetIds;
    void addEuclidExciterHits (juce::MidiBuffer& midi, int numSamples);
    // Clip sequencer: plays the current clip into the synth's MIDI.
    void processClip (juce::MidiBuffer& midi, int numSamples);
    struct ClipActiveNote { int note; double endBeat; };
    ParamRef clipOnRef { "clip_on" }, clipIndexRef { "clip_index" }, clipModeRef { "clip_mode" };
    juce::MidiBuffer clipScratch;
    std::vector<ClipActiveNote> clipActive;
    juce::Array<int> clipHeld; // keys down, in press order (Key transpose)
    std::array<juce::uint8, 128> clipHeldVelocity {}; // their velocities, to give them back
    double clipBase = 0.0;     // the beat at the block's first sample
    double clipExpected = 0.0; // where the host's beat should be next block
    bool clipWasOn = false, clipRunning = false, clipUsedHost = false;
    int clipLastIndex = -1, clipLastMode = -1;
    std::atomic<float> clipPlayhead { -1.0f };
    int arpRatchetNote = 0, arpRatchetsLeft = 0, arpRatchetInterval = 0, arpRatchetCounter = 0;
    double euclidExciterPhase = 0.0;
    long long euclidExciterLastStep = -1;

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
    void swapUserTable (int slot, std::shared_ptr<Wavetable> table);
    std::array<std::shared_ptr<const WavetableDoc>, (size_t) numUserSlots> userTableDocs; // null: default
    juce::String tableNotice;
    std::atomic<int> tableNoticeVersion { 0 };
    mutable juce::SpinLock tableLock;

    std::array<std::array<ParamRef, 11>, OscillatorIds::count> stringParamIds;
    std::array<std::array<ParamRef, 4>, OscillatorIds::count> bowBuzzIds;
    std::array<std::array<ParamRef, 4>, OscillatorIds::count> keysParamIds;
    std::array<std::array<ParamRef, 2>, OscillatorIds::count> electricParamIds;
    std::array<std::array<ParamRef, 2>, OscillatorIds::count> feedbackParamIds;

    // M7.5 audio input (ilanaSynth FX). The instrument has no input, so all
    // of this stays silent there.
    void captureLiveInput (juce::AudioBuffer<float>& buffer);
    void prepareLiveInput (int numSamples, int factor, juce::MidiBuffer& midi);
    std::array<ParamRef, OscillatorIds::count> grainLiveIds;
    ParamRef inGainRef { "in_gain" }, inDryRef { "in_dry" }, inBodyRef { "in_body" }, inStringsRef { "in_strings" },
        inTriggerRef { "in_trigger" }, inThresholdRef { "in_threshold" }, inNoteRef { "in_note" },
        inAttackRef { "in_attack" }, inReleaseRef { "in_release" };
    ParamRef tuningOnRef { "tuning_on" };
    // The Airwindows module (FX type 30): only the chosen algorithm runs.
    airwindows::Module airwindowsModule;
    ParamRef awAlgoRef { "fx_aw_algo" }, awMixRef { "fx_aw_mix" };
    std::array<ParamRef, airwindows::Module::numKnobs> awKnobRefs { ParamRef ("fx_aw_p1"), ParamRef ("fx_aw_p2"),
        ParamRef ("fx_aw_p3"), ParamRef ("fx_aw_p4"), ParamRef ("fx_aw_p5") };
    std::vector<float> airwindowsMonoRight;
    // The Airwindows category modules (FX types 32-41), one engine each;
    // their refs: algo, p1..p5, mix.
    static constexpr int numAwCategories = 10;
    std::array<airwindows::Module, numAwCategories> awCategoryModules;
    static std::array<std::array<ParamRef, 7>, numAwCategories> makeAwCategoryRefs()
    {
        std::array<std::array<ParamRef, 7>, numAwCategories> refs;
        for (int c = 0; c < numAwCategories; ++c)
        {
            const auto prefix = juce::String ("fx_") + airwindows::categoryModules()[(size_t) c].id;
            refs[(size_t) c][0] = ParamRef (prefix + "_algo");
            for (int k = 0; k < 5; ++k)
                refs[(size_t) c][(size_t) k + 1] = ParamRef (prefix + "_p" + juce::String (k + 1));
            refs[(size_t) c][6] = ParamRef (prefix + "_mix");
        }
        return refs;
    }
    std::array<std::array<ParamRef, 7>, numAwCategories> awCategoryRefs = makeAwCategoryRefs();
    void processAirwindowsCategory (juce::AudioBuffer<float>& buffer, int category);
    // The vocoder (FX type 31).
    Vocoder vocoder;
    std::vector<float> vocoderModulator;
    ParamRef vocSourceRef { "fx_voc_source" }, vocBandsRef { "fx_voc_bands" }, vocWidthRef { "fx_voc_width" },
        vocAttackRef { "fx_voc_attack" }, vocReleaseRef { "fx_voc_release" }, vocFormantRef { "fx_voc_formant" },
        vocUnvoicedRef { "fx_voc_unvoiced" }, vocRateRef { "fx_voc_rate" }, vocLevelRef { "fx_voc_level" },
        vocMixRef { "fx_voc_mix" };
    juce::AudioBuffer<float> liveDry;       // the input as it came in (for DRY)
    std::vector<float> liveVoice, liveEnvVoice; // at the voice rate, after INPUT GAIN
    SampleData liveHistory;                 // the last few seconds, for live grains
    int liveHistoryWrite = 0;
    float liveLast = 0.0f, liveEnvState = 0.0f;
    bool liveGateOpen = false;
    int liveGateNote = -1;
    int liveInputSamples = 0;               // valid samples in liveDry this block
    // DRY is delayed by the reported latency so it lines up with the
    // oversampled wet path (a blend would comb-filter otherwise).
    void delayLiveDry (int numSamples);
    juce::AudioBuffer<float> dryDelayRing;
    int dryDelayWrite = 0, dryDelaySamples = 0;
    std::atomic<bool> liveRetrigger { false }; // a patch loaded: restart the drone
    // A patch loaded: stop the old one's voices and effect tails at the next
    // block, easing from the last output sample to silence rather than
    // stepping to it.
    std::atomic<bool> patchCut { false };
    // VoiceParams::exciterLevelMatch; saved as the state's "exciterLevels".
    std::atomic<bool> exciterLevelMatch { true };
    void updateExciterLevelMatch (bool savedWithMatch);

public:
    // Loads one DX7 voice as the current patch (named after it): ordinary
    // parameters, the operators on the Operator EG (Dx7Presets.h).
    void loadDx7Voice (const Dx7::Voice& voice, const juce::String& name);
    // A .syx bank (32 voices) or single voice, saved as user presets under
    // DX7/<file name>/. Returns how many were imported; message says why not.
    int importDx7File (const juce::File& file, juce::String& message);

private:
    float lastOutput[2] {}, declick[2] {};
    std::array<std::array<std::array<float, 2>, 2>, 2> dcBlock {}; // [before/after the effects][channel][x, y]
    void cutPatchTails();
    std::atomic<float> inputLevelDisplay { 0.0f }, inputEnvDisplay { 0.0f };
    struct OscCoreIds
    {
        ParamRef on, table, frame, level, pan, semi, fine, unison, detune, spread, spectral, spectralAmount, chord, out;
    };
    std::array<OscCoreIds, OscillatorIds::count> oscCoreIds;

    // Parameter IDs built once, so the audio thread never allocates strings.
    struct ModSlotIds { ParamRef src, dst, amt, curve, polarity, aux, bypass; };
    struct ModSlotRaw
    {
        std::atomic<float>* src = nullptr;
        std::atomic<float>* dst = nullptr;
        std::atomic<float>* amt = nullptr;
        std::atomic<float>* curve = nullptr;
        std::atomic<float>* polarity = nullptr;
        std::atomic<float>* aux = nullptr;
        std::atomic<float>* bypass = nullptr;
    };
    std::array<ModSlotRaw, (size_t) Mod::maxSlots> modSlotRaw;
    std::array<ModSlotIds, (size_t) Mod::maxSlots> modSlotIds;
    struct LfoIds
    {
        ParamRef shape, rate, sync, div, retrig, phase, key, physA, physB, kick;
        std::array<ParamRef, 16> steps;
        std::array<ParamRef, LfoSimInfo::numParams> sim;
        ParamRef smooth, axis, trigger, loop, seed, stereo, fire;
    };
    struct OscShapeIds { ParamRef warp, warpAmount, unisonMode, unisonBlend, route; };
    std::array<OscShapeIds, OscillatorIds::count> oscShapeIds;
    // M5/M6 operator and phase-distortion settings.
    struct OperatorIds
    {
        ParamRef tune, ratio, snap, fixedHz, keyLevel, feedbackType, warp2, warp2Amount, pdEnv, pdEnvAmount;
    };
    std::array<OperatorIds, OscillatorIds::count> operatorIds;
    std::array<ParamRef, OscillatorIds::count> fmNoiseIds;
    // DAHDSR extras and rate key scaling for ENV 1..16.
    struct EnvelopeExtraIds { ParamRef delay, hold, keyRate; };
    std::array<EnvelopeExtraIds, 16> envelopeExtraIds;
    std::array<ParamRef, Mseg::numPoints> msegLevelIds, msegTimeIds;
    // ENV 6..16's ADSR, curve and velocity.
    struct ExtraEnvIds { ParamRef attack, decay, sustain, release, curve, velocity; };
    std::array<ExtraEnvIds, 11> extraEnvIds;
    // Every FM matrix cell ([source][target]), each oscillator's amp
    // envelope choice and sample source.
    std::array<std::array<ParamRef, OscillatorIds::count>, OscillatorIds::count> fmMatrixIds;
    std::array<ParamRef, OscillatorIds::count> oscAmpEnvIds, sampleFactoryIds;
    // The Operator EG's parameters, in OperatorEg::operatorFields() and
    // voiceFields() order.
    std::array<std::array<ParamRef, 17>, OscillatorIds::count> operatorEgIds;
    std::array<ParamRef, 15> operatorEgVoiceIds;
    ParamRef operatorEgKeyOffsetId { OperatorEg::keyOffsetId };

    std::array<LfoIds, (size_t) numLfos> lfoIds;
    std::array<int, (size_t) numLfos> lfoPreviousShapes = [] { std::array<int, (size_t) numLfos> shapes {}; shapes.fill (-1); return shapes; }();
    // Which LFOs a mod slot uses: LFO 5-16 only render in full when routed.
    std::array<bool, (size_t) numLfos> lfoRouted {};
    struct FxSlotIds { ParamRef type, bypass, solo, mix, band; };
    std::array<FxSlotIds, (size_t) numFxSlots> fxSlotIds;
    std::array<ParamRef, 16> tapStepIds;
    std::array<ParamRef, 16> gateStepIds;
    std::array<std::array<ParamRef, 7>, OscillatorIds::count> sampleParamIds;
    std::array<std::array<ParamRef, 5>, OscillatorIds::count> grainParamIds;

    static constexpr int numSampleOscs = OscillatorIds::count;
    std::vector<std::shared_ptr<SampleData>> sampleSlots;
    std::array<std::shared_ptr<SampleData>, 5> factorySamples;
    std::array<std::array<std::shared_ptr<SampleData>, 3>, (size_t) numSampleOscs> retiredSamples;
    std::array<int, (size_t) numSampleOscs> retiredIndex {};
    mutable juce::SpinLock sampleLock;
    mutable juce::SpinLock stateLock;
    std::array<juce::String, (size_t) numSampleOscs> samplePaths;
    std::array<std::shared_ptr<SampleData>, (size_t) numSampleOscs> embeddedSamples; // under stateLock
    std::array<juce::String, (size_t) numSampleOscs> pendingSamplePaths;
    std::array<bool, (size_t) numSampleOscs> pendingSampleClear {};
    bool samplesReloadPending = false;
    std::array<juce::String, (size_t) numUserSlots> userTablePaths;
    std::array<juce::String, (size_t) numUserSlots> pendingUserTablePaths;
    std::array<int, (size_t) numUserSlots> userTableModes {};

    struct UserPresetMeta
    {
        juce::int64 modified = -1;
        juce::String category, tags;
    };

    const UserPresetMeta& getUserPresetMeta (const juce::File& file) const;
    mutable std::unordered_map<std::string, UserPresetMeta> userPresetMetaCache;
    std::array<int, (size_t) numUserSlots> pendingUserTableModes {};
    std::array<bool, (size_t) numUserSlots> pendingUserTableClear {};
    bool userTablesReloadPending = false;

    juce::AudioBuffer<float> lfoBuffers;
    std::array<double, (size_t) numLfos> lfoPhases {};
    std::array<std::atomic<float>, (size_t) numLfos> lfoSampleHolds {};
    std::array<std::atomic<float>, (size_t) numLfos> lfoLastValues {};
    std::array<LfoChaos, (size_t) numLfos> lfoChaos;
    // M8.1: the simulated shapes, SMOOTH, output B and the triggers.
    std::array<LfoSim, (size_t) numLfos> lfoSims;
    std::atomic<float> westGateDisplay { 0.0f };
    // M8.5
    void updateEvolveAndVector (int numSamples);
    MacroEvolve evolve, vectorDrift;
    std::array<std::atomic<float>, Mod::numMacros> macroDrift {};
    std::atomic<float> vectorX { 0.5f }, vectorY { 0.5f };
    std::array<float, OscillatorIds::count> vectorGains = [] { std::array<float, OscillatorIds::count> g {}; g.fill (1.0f); return g; }();
    double vectorPathPhase = 0.0;
    std::array<ParamRef, Mod::numMacros> macroIds;
    std::array<std::pair<ParamRef, ParamRef>, Mod::numMacros> evolveIds;
    std::array<std::pair<ParamRef, ParamRef>, numVectorPoints> vectorPathIds;
    std::array<LfoSmoother, (size_t) numLfos> lfoSmoothers;
    std::array<bool, (size_t) numLfos> lfoRoutedB {};
    std::array<std::atomic<float>, (size_t) numLfos> lfoLastValuesB {};
    std::array<bool, (size_t) numLfos> lfoFireWas {};
    std::array<long long, (size_t) numLfos> lfoBeatLast = [] { std::array<long long, (size_t) numLfos> a {}; a.fill (-1); return a; }();
    std::array<double, (size_t) numLfos> lfoBeatPhase {};
    long long lfoGenerativeLast = -1;
    double lfoGenerativePhase = 0.0;
    std::array<unsigned, (size_t) numLfos> lfoTriggerCounts {};
    std::uint32_t lfoSimSeedCounter = 1;
    juce::Random lfoRandom;
    juce::Random lfoPoolRandom { 31415 };
    juce::Random& randomForLfo (int lfo) { return lfo < 4 ? lfoRandom : lfoPoolRandom; }

    float modWheelValue = 0.0f;
    float aftertouchValue = 0.0f;
    float expressionValue = 1.0f;
    std::atomic<float> modWheelDisplay { 0.0f };
    std::atomic<float> aftertouchDisplay { 0.0f };
    std::atomic<float> expressionDisplay { 1.0f };
    std::atomic<float> clockShDisplay { 0.0f };
    std::atomic<float> msegDisplay { 0.0f };
    std::atomic<float> msegPhaseDisplay { 0.0f };

    double currentSampleRate = 44100.0;
    double baseSampleRate = 44100.0;
    int expectedBlockSize = 512;
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<double> displaySampleRate { 44100.0 };

    std::array<std::atomic<float>, (size_t) maxDestinations> modDisplayValues {};

    // Modulation of plain parameters (effects, master...): offsets in the
    // parameter's normalised range, applied inside getParam().
    struct ParamDestination
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* parameter = nullptr;
    };
    std::vector<ParamDestination> paramDestinations;
    std::unordered_map<const std::atomic<float>*, int> rawToParamDestination;

    // getParam's lookup: an open-addressed hash of every parameter ID, built
    // once in the constructor and read-only after (so any thread can use
    // it). The value tree's own lookup is a string-keyed map, and a block
    // makes over a thousand reads.
    std::vector<ParamCacheEntry> paramCache;
    std::vector<juce::String> paramCacheIds;
    std::uint32_t paramCacheMask = 0;
    void buildParamCache();
    std::array<float, (size_t) maxDestinations> paramDestinationOffsets {};
    bool anyParamModulation = false;

    // The last note played and the loudest voice's per-voice sources, for
    // modulation that has to be synth-wide.
    std::atomic<float> monitorVelocity { 0.0f };
    std::atomic<float> monitorKeyTrack { 0.0f };
    std::atomic<float> monitorRandom { 0.0f };
    float lfoStepValues[numLfos][16] {};
    float globalSourceValue (Mod::Source source) const;
    void evaluateGlobalModulation (const Mod::Slot* slots, int numSlots);
    std::array<std::atomic<float>, (size_t) numLfos> lfoPhaseDisplays {};
    std::atomic<float> displayPhase { 0.0f };
    std::atomic<float> displayFrequency { 0.0f };
    std::array<std::atomic<float>, OscillatorIds::count> displaySamplePositions {};
    std::array<std::atomic<float>, OscillatorIds::count> oscDisplayPhases {};

    std::vector<float> scopeLeft, scopeRight;
    std::atomic<int> scopeWritePos { 0 };
    mutable juce::SpinLock scopeLock;

    std::array<std::array<float, lfoDrawSteps>, (size_t) numLfos> lfoCustom {};
    std::array<std::array<float, lfoDrawSteps>, (size_t) numLfos> activeLfoCustom {};
    std::array<LfoCurve, (size_t) numLfos> lfoCurves;
    std::array<std::array<float, LfoCurve::tableSize>, (size_t) numLfos> lfoCurveTables {};
    std::array<std::array<float, LfoCurve::tableSize>, (size_t) numLfos> activeLfoCurveTables {};
    mutable juce::SpinLock lfoShapeLock;

    using RemapTable = std::array<float, (size_t) Mod::remapSize + 1>;
    std::array<LfoCurve, (size_t) Mod::maxSlots> modRemaps;
    std::array<RemapTable, (size_t) Mod::maxSlots> modRemapTables {};
    std::array<std::atomic<bool>, (size_t) Mod::maxSlots> modRemapOn {};
    std::array<RemapTable, (size_t) Mod::maxSlots> activeModRemapTables {};
    std::array<bool, (size_t) Mod::maxSlots> activeModRemapOn {};
    std::atomic<int> remapEpoch { 0 };
    int activeRemapEpoch = -1;
    mutable juce::SpinLock remapLock;

    // On-screen keyboard notes, queued lock-free from the message thread so
    // none are lost between blocks; they join the MIDI input (and so go
    // through scale snap, note spray and the arpeggiator).
    struct PreviewEvent { int note = 0; float velocity = 0.0f; bool isOn = false; };
    static constexpr int previewQueueSize = 128;
    juce::AbstractFifo previewFifo { previewQueueSize };
    std::array<PreviewEvent, (size_t) previewQueueSize> previewEvents {};
    std::atomic<float> cpuUsage { 0.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> macroLearn { -1 };
    std::atomic<int> macroCc[Mod::numMacros] {};

    std::atomic<int> pendingProgramChange { -1 };
    std::atomic<float> pendingMacros[Mod::numMacros] {};
    std::atomic<bool> macroPendingFlags[Mod::numMacros] {};
    std::atomic<bool> macrosPending { false };
    // Parameter MIDI learn: CC -> index into getParameters() (-1: none), the
    // parameter waiting for a CC, and values queued for the message thread.
    std::array<std::atomic<int>, 128> ccParameter;
    std::atomic<int> paramLearn { -1 };
    std::array<std::atomic<float>, 128> pendingCcValues;
    std::array<std::atomic<bool>, 128> ccValuePending;
    std::atomic<bool> ccPending { false };
    juce::String getParamCcMapText() const;
    void setParamCcMapText (const juce::String& text);
    std::atomic<float> envMonitorAmp { 0.0f };
    std::atomic<float> envMonitorFilter { 0.0f };
    std::atomic<float> envMonitorFilter2 { 0.0f };
    std::atomic<float> envMonitorMod { 0.0f };
    std::atomic<float> envMonitorEnv4 { 0.0f };
    std::array<std::atomic<float>, 11> envMonitorExtra {};
    std::array<std::atomic<float>, 16> envMonitorPositions {};
    std::array<std::atomic<int>, 3> revealMasks { defaultRevealMask, defaultRevealMask, defaultRevealMask };
    std::atomic<int> revealVersion { 0 };

    struct ParamEpoch : juce::AudioProcessorParameter::Listener
    {
        explicit ParamEpoch (std::atomic<unsigned>& e) : epoch (e) {}
        void parameterValueChanged (int, float) override { ++epoch; }
        void parameterGestureChanged (int, bool) override {}
        std::atomic<unsigned>& epoch;
    };
    std::atomic<unsigned> paramEpoch { 0 }, liveEpoch { 0 }, dataEpoch { 0 }, sampleEpoch { 0 };
    ParamEpoch paramEpochListener { paramEpoch };

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Convolution convolution;
    juce::dsp::Oversampling<float> oversampler2x { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::Oversampling<float> oversampler4x { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    std::atomic<int> oversamplingFactor { 1 };
    int wantedOversamplingFactor() const;
    juce::dsp::Oversampling<float>& activeOversampler() { return oversamplingFactor.load() == 4 ? oversampler4x : oversampler2x; }
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
    SympatheticStrings sympatheticStrings;
    bool sympatheticWasOn = false;

    // M4 acoustic keys, shared by every voice (base rate, after the voices).
    Soundboard soundboard;
    DenseSoundboard denseSoundboard;
    PedalResonance pedalResonance;
    MechanicalNoise mechanicalNoise;
    bool keysPedalDown = false, soundboardWasOn = false;
    std::array<bool, 128> pedalHeldNotes {};
    void processAcousticKeys (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);

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
    int gateLastStep = -1;
    std::atomic<int> gateDisplayStep { -1 };
    std::atomic<int> engineDisplayStep { -1 };
    std::atomic<int> euclidDisplayStep { -1 };
    std::atomic<double> hostPpq { 0.0 };
    std::atomic<bool> hostPlaying { false };
    std::array<std::atomic<float>, 2> outputPeaks {};
    std::atomic<unsigned> noteOnCount { 0 };
    std::atomic<float> outputLevelDisplay { 0.0f };

public:
    // Pattern built-ins as step levels (for the editor and the Custom copy).
    static float gatePatternLevel (int pattern, int step);
    int getGateDisplayStep() const { return gateDisplayStep.load(); }

    // The note engine's current step number (arp, probability sequencer or
    // Euclid), or -1 while no key is held. For the Generative card's playhead.
    int getEngineDisplayStep() const { return engineDisplayStep.load(); }

    // The Euclid rhythm's current step (whichever target plays it), or -1.
    int getEuclidDisplayStep() const { return euclidDisplayStep.load(); }

    // Tests only: what the last block sent to the voices.
    const juce::MidiBuffer& getSynthMidiForTest() const { return midiForSynth; }

    // Tests only: one arpeggiator note choice from the currently held notes.
    int pickArpNoteForTest (int mode, int octaves) { return selectArpNote (mode, octaves); }

private:
    float tapeStopRate = 1.0f;
    juce::AudioBuffer<float> tapeStopBuffer;
    int tapeStopWrite = 0;
    double tapeStopRead = 0.0;
    float tapeStopLagFade = 1.0f; // 1 = playing the lagging tape; fades to 0 to rejoin live
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
    Biquad eqBands[2][3];
    Svf vowelFilters[2][3];
    juce::AudioBuffer<float> fxScratch;
    // FX splitters: a slot set to a band processes only that part of the
    // signal (Linkwitz-Riley crossovers, or mid/side) and the rest passes
    // around it, so an untouched band sums back exactly.
    struct SplitFilter
    {
        // Two cascaded Butterworth sections per channel (24 dB/oct, LR4) cut
        // the band; the crossovers' allpasses give the phase the band's
        // complement needs (LR4 low + high = a second-order allpass).
        std::array<std::array<juce::IIRFilter, 2>, 2> lowA, lowB;
        std::array<juce::IIRFilter, 2> allLow, allHigh;
        int lastBand = 0;
        float lastLow = 0.0f, lastHigh = 0.0f;
    };
    std::array<SplitFilter, (size_t) numFxSlots> fxSplit;
    juce::AudioBuffer<float> fxBand;
    void processSlotBand (int slot, int type, int band, juce::AudioBuffer<float>& buffer, bool solo, float blend);
    juce::AudioBuffer<float> reverbScratch;
    std::atomic<float> compGainReduction { 1.0f };
    std::atomic<float> limiterGainReduction { 1.0f };
    std::array<std::atomic<float>, 3> ottBandGain { 1.0f, 1.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IlanaSynthAudioProcessor)
};
