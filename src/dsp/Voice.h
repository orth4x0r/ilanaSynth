#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <vector>

#include "FilterUnit.h"
#include "GranularOsc.h"
#include "KarplusStrong.h"
#include "LfoShape.h"
#include "WestCoast.h"
#include "MaterialBody.h"
#include "Modulation.h"
#include "Mseg.h"
#include "OscillatorIds.h"
#include "PolyBlepOsc.h"
#include "ResonatorBank.h"
#include "SamplePlayer.h"
#include "Tuning.h"
#include "Svf.h"
#include "TensionAdsr.h"
#include "UnisonBank.h"
#include "WavetableOscillator.h"

namespace UnisonMode
{
enum
{
    Classic = 0,
    Hypersaw,
    Octaves,
    Fifths,
    Count
};

inline juce::StringArray getNames() { return { "Classic", "Hypersaw", "Octaves", "Fifths" }; }
} // namespace UnisonMode

// Where an oscillator enters the filter section. Default follows the
// serial/parallel switch (into Filter 1 when serial, both when parallel).
namespace FilterRoute
{
enum { Default = 0, Filter1, Filter2, Direct, Both, Count };

inline juce::StringArray getNames() { return { "Default", "Filter 1", "Filter 2", "No filter", "Both" }; }
} // namespace FilterRoute

// How an operator's self-feedback (the matrix diagonal) is taken.
namespace FmFeedback
{
enum { Plain = 0, Filtered, Cross, Count };

inline juce::StringArray getNames() { return { "Plain", "Filtered", "Cross" }; }

// Cross feedback runs between the two oscillators of a pair: 1-2, 3-4, 5-6.
inline int partnerOf (int osc) { return osc ^ 1; }
} // namespace FmFeedback

// Operator tuning (M5).
namespace OscTuning
{
enum { Semitones = 0, Ratio, Fixed, Count };

inline juce::StringArray getModeNames() { return { "Semitones", "Ratio", "Fixed Hz" }; }
inline juce::StringArray getSnapNames() { return { "Free", "Harmonic", "Inharmonic", "Bell" }; }

// The ratio sets the SNAP knob picks from: whole-number harmonics, square
// roots of non-square numbers (classic inharmonic FM ratios), and the
// partials of a tuned church bell and a free bar.
inline const std::vector<double>& snapSet (int set)
{
    static const std::vector<double> harmonic = []
    {
        std::vector<double> ratios { 0.25, 0.5 };
        for (int n = 1; n <= 32; ++n)
            ratios.push_back ((double) n);
        return ratios;
    }();
    static const std::vector<double> inharmonic = []
    {
        std::vector<double> ratios { 0.5 * std::sqrt (2.0), 0.5 * std::sqrt (3.0) };
        for (int n = 2; n <= 128; ++n)
        {
            const auto root = std::sqrt ((double) n);
            if (std::abs (root - std::round (root)) > 1.0e-9)
                ratios.push_back (root);
        }
        return ratios;
    }();
    // Bell: hum, prime, tierce, quint, nominal, deciem, undeciem, duodeciem,
    // upper octave; bar: the free-bar modes 2.756, 5.404, 8.933, 13.34.
    static const std::vector<double> bell { 0.5, 1.0, 1.2, 1.5, 2.0, 2.5, 2.667, 2.756, 3.0, 4.0,
                                            5.0, 5.404, 6.0, 8.0, 8.933, 13.34 };
    static const std::vector<double> none;

    switch (set)
    {
        case 1:  return harmonic;
        case 2:  return inharmonic;
        case 3:  return bell;
        default: return none;
    }
}

// The nearest ratio of the set (in pitch), or the ratio itself when Free.
inline double snapRatio (double ratio, int set)
{
    const auto& ratios = snapSet (set);

    if (ratios.empty() || ratio <= 0.0)
        return ratio;

    auto best = ratios.front();
    auto bestDistance = 1.0e9;

    for (const auto candidate : ratios)
    {
        const auto distance = std::abs (std::log (candidate / ratio));

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}
} // namespace OscTuning

struct VoiceParams
{
    static constexpr int numOscillators = OscillatorIds::count;
    static constexpr int maxUnison = 16;
    // String and sample unison each carry a long delay/sample buffer, so they
    // stop at 8 voices; wavetable unison goes to 16.
    static constexpr int maxBufferedUnison = 8;
    static constexpr int numLfos = Mod::numLfoSources;

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
        int unisonMode = UnisonMode::Classic;
        float unisonBlend = 1.0f;
        int warpMode = 0;
        float warpAmount = 0.0f;
        int route = 0; // FilterRoute
        int ampEnv = 0; // 0..15, ENV 1..16; 16 = the MSEG as a one-shot envelope

        // M5 operator settings. Tuning: 0 = semitones (as before), 1 = a
        // frequency ratio of the note (already snapped), 2 = a fixed pitch.
        int tuneMode = 0;
        double ratio = 1.0;
        double fixedHz = 440.0;
        float keyLevel = 0.0f;   // level key scaling: dB per octave from C3, x6
        int feedbackType = 0;    // FmFeedback: plain, filtered, cross

        // M6: the PD chain's second stage and the DCW-style warp envelope
        // (0 = off, 1..16 = ENV 1..16, 17 = MSEG).
        int warpMode2 = 0;
        float warpAmount2 = 0.0f;
        int pdEnv = 0;
        float pdEnvAmount = 1.0f;

        bool stringMode = false;
        int stringExcite = 0;
        float stringDecay = 0.75f;
        float stringDamping = 0.35f;
        float stringSustain = 0.0f;
        float stringStiffness = 0.0f;
        float stringPickup = 0.0f;
        float stringExcitationPosition = 0.0f;
        float stringPickHardness = 1.0f;
        float stringPickPosition = 0.0f;
        bool stringSlap = false;
        float bowPressure = 0.5f, bowSpeed = 0.5f;
        float bridgeBuzz = 0.0f, fretRattle = 0.0f;
        // M4 keys: felt hardness, strings sharing the bridge, dampers on
        // release, and how much the string changes from bass to treble.
        float hammerHardness = 0.5f, couple = 0.0f, damper = 0.0f, registerMap = 0.0f;
        float epDistance = 0.5f, epPosition = 0.5f; // M7.3 tine/reed pickup
        float fbGain = 0.5f, fbDistance = 0.5f;     // M8.5 feedback amp
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

        bool granularMode = false; // also sets sampleMode: grains read the sample
        bool grainLive = false;    // M7.5: grains read the live input's history
        int liveWrite = 0;
        bool liveMode = false;     // M7.5: the Live mode plays the audio input
        float grainSizeMs = 80.0f;
        float grainDensity = 0.5f;
        float grainSpray = 0.15f;
        float grainPitch = 0.0f;
        float grainSpread = 0.6f;
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
        float morph = 0.0f;
    };

    // Per-voice LFO settings. When perVoice is off the voice reads the
    // synth-wide LFO buffer instead (free-running, shared by all voices).
    struct LfoParams
    {
        bool perVoice = false;
        bool keyTrack = false;         // rate follows the note: RATE 4 Hz = the note's pitch
        int shape = 0;
        double baseIncrement = 0.0;   // cycles per voice-rate sample, before modulation
        float startPhase = 0.0f;
        float physA = 0.5f, physB = 0.5f;
        bool kick = false;
        const float* steps = nullptr;  // 16 values
        const float* custom = nullptr; // lfoDrawSteps values
        int customSize = 0;
        // M8.1
        LfoSimSettings sim;            // the simulated shapes' settings
        float smooth = 0.0f;           // SMOOTH, a fraction of a cycle
        bool needsB = false;           // output B is routed
        unsigned triggerCount = 0;     // beats, Generative steps and FIRE, counted by the processor
    };

    std::array<OscParams, numOscillators> oscillators;
    std::array<bool, numOscillators> oscillatorEnabled { true, false, true };
    int subOctaveOffset = -12;

    float fmAmount = 0.0f;
    float fmFeedback = 0.0f;
    float fmMatrix[numOscillators][numOscillators] {}; // [source][target]
    int fmMode = 0;               // 0 phase, 1 through-zero, 2 exponential
    float fmNoise[numOscillators] {}; // M5: the noise operator into each oscillator
    float fmNoiseColour = 1.0f;   // 0 dark (about 200 Hz) .. 1 white
    std::array<bool, numOscillators> oscOut { true, true, true };
    float ringMod = 0.0f;
    bool hardSync = false;
    float drift = 0.0f;

    float voiceSpread = 0.0f;
    float stretch = 0.0f; // M4: piano stretch tuning, 0 = equal temperament
    const Tuning* tuning = nullptr; // Scala tuning; nullptr = 12-TET (the old path)
    float unisonRandom = 0.0f;

    float filter1Fm = 0.0f;
    float filter2Fm = 0.0f;

    bool resonatorOn = false;
    float resonatorAmount = 0.0f;
    float resonatorDecay = 0.7f;
    float resonatorOffset = 0.0f;
    float resonatorKeytrack = 1.0f;
    int bodyType = 0; // Classic, Bar, Plate, Bell, Shell; Classic preserves res_*
    float bodyMaterial = 0.0f, bodySize = 0.5f;
    int bodyCouplingMode = 0; // Off, String to body, Body to string, Strings
    float bodyCoupling = 0.0f;

    // The dedicated sub oscillator (sine/square/saw one or two octaves down).
    // It and the noise share one route.
    bool subOscEnabled = false;
    float subOscLevel = 0.5f;
    int subOscOctave = -12;
    int subOscRoute = 0;
    const Wavetable* subOscTable = nullptr;
    float noiseLevel = 0.0f;

    FilterParams filter1;
    FilterParams filter2;
    bool filtersParallel = false;
    float filterBalance = 0.0f; // parallel only: -1 all Filter 1 .. +1 all Filter 2

    TensionAdsr::Parameters ampEnv { 0.005f, 0.3f, 0.8f, 0.25f, 0.0f };
    TensionAdsr::Parameters filterEnv { 0.01f, 0.4f, 0.4f, 0.3f, 0.0f };
    TensionAdsr::Parameters filter2Env { 0.01f, 0.4f, 0.4f, 0.3f, 0.0f };
    TensionAdsr::Parameters modEnv { 0.05f, 0.4f, 0.5f, 0.3f, 0.0f };
    TensionAdsr::Parameters env4 { 0.05f, 0.4f, 0.5f, 0.3f, 0.0f };
    std::array<TensionAdsr::Parameters, 11> extraEnvs {};
    std::array<float, 11> extraEnvVelocity {};
    std::array<bool, 11> extraEnvNeeded {};
    // M5: envelope times shrink up the keyboard (0 = off, 1 = halve per octave),
    // ENV 1..16.
    std::array<float, 16> envKeyRate {};

    // The MSEG's shape, run per voice as a one-shot envelope when an
    // oscillator picks it as its amp or warp envelope.
    struct MsegShape
    {
        float levels[Mseg::numPoints] { 0.0f, 1.0f, 0.0f, -1.0f };
        float times[Mseg::numPoints] { 0.25f, 0.25f, 0.25f, 0.25f };
        double rateHz = 0.5;
        bool loop = true;
    };
    MsegShape msegShape;
    bool msegEnvNeeded = false;
    int quality = 1;
    float ampVelocity = 0.5f;
    float filterVelocity = 0.5f;
    // ENV 3-5's own velocity (0 = off), like ENV 6-16's.
    float filter2EnvVelocity = 0.0f, modEnvVelocity = 0.0f, env4Velocity = 0.0f;

    float glideTime = 0.0f;
    float pitchBendRange = 2.0f;

    float macros[Mod::numMacros] {};
    float vectorX = 0.5f, vectorY = 0.5f; // M8.5

    const float* lfoBuffers[numLfos] {}; // free-running LFOs, shared by every voice
    const float* lfoBuffersB[numLfos] {}; // their outputs B (M8.1)
    const float* clockSh = nullptr;
    const float* mseg = nullptr;
    // M7.5 live input (ilanaSynth FX), at the voice rate for this block, and
    // its envelope follower; null when there is no input.
    const float* liveInput = nullptr;
    const float* inputEnv = nullptr;
    float inputToBody = 0.0f, inputToStrings = 0.0f;

    // M8.3: the west-coast voice (wavefolder into a low-pass gate).
    struct WestParams
    {
        bool on = false;
        int position = 0;       // 0 after the filters, 1 in place of Filter 2
        float fold = 0.3f, symmetry = 0.0f;
        int stages = 2;
        int mode = 0;           // LowPassGate::Mode
        float decay = 1.0f, resonance = 0.2f, strike = 1.0f, open = 0.0f;
        int source = 0;         // 0: a strike on each note; else Mod::Source (source - 1 + 1)
    } west;
    LfoParams lfos[numLfos];

    // Only the slots that are switched on, packed at the front, plus the
    // explicit destinations they touch (so the voice only clears those).
    Mod::Slot modSlots[Mod::maxSlots] {};
    int numModSlots = 0;
    int activeDestinations[Mod::maxSlots] {};
    int numActiveDestinations = 0;
    bool anyExtendedFmMods = false;   // a slot targets an FM cell to or from OSC 4-6

    // Appends a routing (for code that drives a voice directly, e.g. tests).
    void addModSlot (Mod::Source source, Mod::Destination destination, float depth)
    {
        if (numModSlots >= Mod::maxSlots)
            return;

        Mod::Slot slot;
        slot.source = source;
        slot.destination = (int) destination;
        slot.depth = depth;
        modSlots[numModSlots++] = slot;

        for (int d = 0; d < numActiveDestinations; ++d)
            if (activeDestinations[d] == (int) destination)
                return;

        activeDestinations[numActiveDestinations++] = (int) destination;
    }
};

class Voice : public juce::SynthesiserVoice
{
public:
    Voice();

    void setParams (const VoiceParams& newParams) { params = newParams; }

    float getLastAmpValue() const { return lastAmpValue; }
    float getWestGateLevel() const { return params.west.on && isVoiceActive() ? westGateL.getConductance() : 0.0f; }
    float getLastLifetimeValue() const { return lastLifetimeValue; }
    float getLastExtraEnvValue (int index) const { return extraEnvValues[(size_t) juce::jlimit (0, 10, index)]; }
    float getLastSamplePosition (int oscIndex) const
    {
        return lastSamplePosition[(size_t) juce::jlimit (0, VoiceParams::numOscillators - 1, oscIndex)];
    }

    float getLastWavetablePhase (int oscIndex) const
    {
        return oscBank[juce::jlimit (0, VoiceParams::numOscillators - 1, oscIndex)].getPhase (0);
    }
    float getLastFilterValue() const { return lastFilterValue; }
    float getLastFilter2Value() const { return lastFilter2Value; }
    float getLastModValue() const { return lastModValue; }
    float getLastEnv4Value() const { return lastEnv4Value; }
    float getVelocity() const { return velocityLevel; }
    float getKeyTrack() const { return keyTrackValue; }
    float getRandomValue() const { return randomValue; }
    float getLfoPhase (int lfo) const { return (float) lfoPhases[(size_t) juce::jlimit (0, 3, lfo)]; }

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

    // Re-strikes this voice's Physical strings while its key is held
    // (Euclid's Exciter target). Other oscillator modes are left alone.
    void reExcite (float level);
    void stopNote (float velocity, bool allowTailOff) override;
    // A new patch: clear what the last one left in the voice (filter and
    // body memory, the vactrol, strings, glide origin, random sequences),
    // so its first notes don't carry the old patch's ringing.
    void resetForNewPatch();
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void channelPressureChanged (int newValue) override;
    void aftertouchChanged (int newValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    void syncSamplePlayers();
    void updateSubBlock (const float* mods, float filterEnvValue, float filter2EnvValue, const float* envelopeValues);
    double oscFrequencyFactor (const VoiceParams::OscParams& settings) const;
    void updateFilterCoefficients (const float* mods, float filterEnvValue, float filter2EnvValue);
    void updateUnisonLayout();
    // 1 at amount 0 exactly, so an envelope without velocity is untouched.
    float velocityScaleFor (float amount) const { return 1.0f - amount + amount * velocityLevel; }
    float sourceValue (Mod::Source source, int sampleIndex, float ampValue, float filterValue,
                       float filter2Value, float modValue, float env4Value) const;
    void evaluateMods (float* mods, int sampleIndex, float ampValue, float filterValue,
                       float filter2Value, float modValue, float env4Value) const;
    void prepareModSlots();
    void advanceVoiceLfos();

    // Per block: the per-voice LFOs and the extra envelopes in use, so the
    // sample loop walks short lists instead of testing every slot.
    int perVoiceLfos[VoiceParams::numLfos] {};
    int numPerVoiceLfos = 0;
    int neededExtraEnvs[11] {};
    int numNeededExtraEnvs = 0;
    bool hasActiveAmpEnvelope() const;
    void configureString (KarplusStrong& string, const VoiceParams::OscParams& settings) const;
    float voiceLfoValue (int lfo) const;
    float blockMod (Mod::Destination destination) const { return blockMods[(size_t) destination]; }

    VoiceParams params;

    // Wavetable unison: each oscillator's voices render together (SIMD).
    UnisonBank oscBank[VoiceParams::numOscillators];
    static_assert (VoiceParams::maxUnison <= UnisonBank::maxLanes);
    // Keep the first three banks' default-construction seed sequence exactly
    // as before M3b. Extra banks use explicit seeds and do not advance the
    // shared KarplusStrong counter used by existing presets.
    int renderStart = 0; // where this render call starts in the block (M7.5)
    KarplusStrong stringUnison[3][VoiceParams::maxBufferedUnison];
    struct ExtraStringBank
    {
        explicit ExtraStringBank (int osc) : voices {
            KarplusStrong (osc * 100003 + 0), KarplusStrong (osc * 100003 + 1),
            KarplusStrong (osc * 100003 + 2), KarplusStrong (osc * 100003 + 3),
            KarplusStrong (osc * 100003 + 4), KarplusStrong (osc * 100003 + 5),
            KarplusStrong (osc * 100003 + 6), KarplusStrong (osc * 100003 + 7) } {}
        KarplusStrong voices[VoiceParams::maxBufferedUnison];
    };
    ExtraStringBank extraStringUnison[3] { ExtraStringBank (3), ExtraStringBank (4), ExtraStringBank (5) };
    KarplusStrong& stringFor (int osc, int unison)
    {
        return osc < 3 ? stringUnison[osc][unison] : extraStringUnison[osc - 3].voices[unison];
    }
    SamplePlayer sampleUnison[VoiceParams::numOscillators][VoiceParams::maxBufferedUnison];
    GranularOsc grains[VoiceParams::numOscillators][VoiceParams::maxBufferedUnison];
    double sampleRatio[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    ResonatorBank resonatorL, resonatorR;
    MaterialBody materialBodyL, materialBodyR;
    // M8.3
    Wavefolder westFolderL, westFolderR;
    LowPassGate westGateL, westGateR;
    int westStrikeRemaining = 0;
    bool bodyStrikePending = false;
    int bodyTailSamplesRemaining = 0;

    FilterUnit filter1L, filter1R, filter2L, filter2R;

    // A pair fed the same signal from the same (reset) state gives the same
    // output, so while linked only the left filter runs (the right one's
    // state is stale). The first different input copies the left state over
    // and the pair runs apart until the next reset.
    bool filter1Linked = true, filter2Linked = true;

    static void processFilterPair (FilterUnit& left, FilterUnit& right, bool& linked,
                                   float inLeft, float inRight, float& outLeft, float& outRight)
    {
        if (linked)
        {
            if (inLeft == inRight)
            {
                outLeft = outRight = left.process (inLeft);
                return;
            }

            right = left;
            linked = false;
        }

        outLeft = left.process (inLeft);
        outRight = right.process (inRight);
    }
    FilterUnit bothFilter1L, bothFilter1R, bothFilter2L, bothFilter2R;

    TensionAdsr ampEnv, filterEnv, filter2Env, modEnv, env4;
    std::array<TensionAdsr, 11> extraEnvs;
    std::array<float, 11> extraEnvValues {};
    Mseg envMseg;
    float msegEnvValue = 0.0f;

    // M5 operator state: the noise operator (its own generator, so existing
    // random sequences don't shift), filtered feedback history, and each
    // oscillator's key-scaled level.
    juce::Random fmNoiseRandom { 31337 };
    float fmNoiseState = 0.0f;
    float feedbackHistory[VoiceParams::numOscillators] {};
    float feedbackFiltered[VoiceParams::numOscillators] {};
    float feedbackCoeff[VoiceParams::numOscillators] {};
    float keyLevelGain[VoiceParams::numOscillators] { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    juce::Random random;
    juce::Random lfoPoolRandom { 27183 };
    juce::Random liveGrainRandom { 0x1f3a }; // seeds for live grains only

    juce::SmoothedValue<float> frameSmooth[VoiceParams::numOscillators];
    juce::SmoothedValue<float> levelSmooth[VoiceParams::numOscillators];
    juce::SmoothedValue<float> noiseSmooth;
    juce::SmoothedValue<float> oscEnableSmooth[VoiceParams::numOscillators];
    WavetableOscillator subOsc;
    juce::SmoothedValue<float> subOscLevelSmooth, subOscEnableSmooth;

    // Modulation evaluated at the start of each block, for everything that
    // doesn't need to move within a block (envelope times, pans, detune...).
    std::array<float, (size_t) Mod::Destination::Count> blockMods {};
    mutable std::array<float, 36> fmCellMods {};   // per-voice mods of the OSC 4-6 FM cells

    // Per render (prepareModSlots): each slot's target (a mods index, or
    // -2 - cell for an OSC 4-6 FM cell, or -1 for none) and, for sources that
    // hold still through a render (velocity, key, wheels, macros, ...), its
    // amount, so the per-sample pass only evaluates the moving sources.
    std::array<int, Mod::maxSlots> slotTargets {};
    std::array<float, Mod::maxSlots> slotAmounts {};
    std::array<bool, Mod::maxSlots> slotHeld {};
    bool modSlotsPrepared = false;
    std::array<float, (size_t) Mod::Destination::Count> sampleMods {};

    double lfoPhases[VoiceParams::numLfos] {};
    double lfoIncrements[VoiceParams::numLfos] {};
    float lfoHolds[VoiceParams::numLfos] {};
    LfoChaos lfoChaos[VoiceParams::numLfos];
    float lfoValues[VoiceParams::numLfos] {};
    // M8.1: simulated shapes, SMOOTH and output B for the per-voice LFOs.
    LfoSim lfoSims[VoiceParams::numLfos];
    LfoSmoother lfoSmoothers[VoiceParams::numLfos];
    float lfoValuesB[VoiceParams::numLfos] {};
    float lfoSmoothCoefficients[VoiceParams::numLfos] {};
    unsigned lfoSeenTriggers[VoiceParams::numLfos] {};
    std::uint32_t lfoSimNotes = 0;

    double sampleRate = 44100.0;
    double baseFrequency = 440.0;
    double currentFrequency = 440.0;
    double bendSemitones = 0.0;
    float velocityLevel = 1.0f;
    bool noteHeld = false;
    float keyTrackValue = 0.0f;
    float keyTrackOctaves = 0.0f;
    float randomValue = 0.0f;
    float modWheel = 0.0f;
    float aftertouchValue = 0.0f;
    float expressionValue = 1.0f;

    float lastAmpValue = 0.0f;
    float lastLifetimeValue = 0.0f;
    float lastSamplePosition[VoiceParams::numOscillators] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float lastFilterValue = 0.0f;
    float lastFilter2Value = 0.0f;
    float lastModValue = 0.0f;
    float lastEnv4Value = 0.0f;

    int numOscUnison[VoiceParams::numOscillators] { 1, 1, 1 };
    float panGainL[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    float panGainR[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    float panGainSubOscL = 0.7071f, panGainSubOscR = 0.7071f;
    float previousOsc[VoiceParams::numOscillators] {};
    // Per-unison-voice pitch offsets (semitones) and gains from the unison
    // mode, detune and blend.
    double unisonOffset[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    // exp2 (offset / 12), worked out per block rather than every sub-block.
    double unisonRatio[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    float unisonGains[VoiceParams::numOscillators][VoiceParams::maxUnison] {};
    float glideCoeff = 1.0f;
    bool hasPlayedNote = false;

    float driftValue = 0.0f;
    float driftTarget = 0.0f;
    juce::Random driftRandom;
    bool lastStringMode[VoiceParams::numOscillators] {};
    bool lastSampleMode[VoiceParams::numOscillators] {};
    bool lastEnabled[VoiceParams::numOscillators] { true, false, true };
    int lastUnison[VoiceParams::numOscillators] { 1, 1, 1 };
    float voicePan = 0.0f;

    bool monoPending = false;
    bool monoLegato = false;
    bool monoKeepRunning = false;
    bool monoGlide = true;
};
