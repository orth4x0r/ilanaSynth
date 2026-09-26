#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "PluginEditor.h"

#include <algorithm>
#include <array>

#include "dsp/Modulation.h"
#include "dsp/TableFactory.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/EqCurve.h"
#include "gui/FilterDisplay.h"
#include "gui/LfoDisplay.h"
#include "gui/CardTabs.h"
#include "gui/EnvThumbs.h"
#include "gui/FilterWidgets.h"
#include "gui/FmDiagram.h"
#include "gui/FmWidgets.h"
#include "gui/TableBrowser.h"
#include "gui/LfoThumbs.h"
#include "gui/MatrixWidgets.h"
#include "gui/ParamControls.h"
#include "gui/ScopeDisplay.h"
#include "gui/SequencerEditors.h"
#include "gui/SubTabBar.h"
#include "gui/TutorialOverlay.h"
#include "gui/WaveDisplay.h"
#include "gui/XtraDisplays.h"

namespace
{
#if defined (_WIN32)
// GetDpiForMonitor ignores the caller's DPI awareness (unlike the DPI APIs
// JUCE's peer uses), so it still reports the real monitor scale inside hosts
// that virtualise plugin windows.  Loaded dynamically to avoid extra link
// dependencies.
float queryMonitorScale (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return 1.0f;

    using MonitorFromWindowFn = void* (*) (void*, unsigned long);
    using GetDpiForMonitorFn = long (*) (void*, int, unsigned int*, unsigned int*);

    auto* user32 = GetModuleHandleW (L"user32.dll");
    auto* shcore = LoadLibraryW (L"shcore.dll");

    if (user32 == nullptr || shcore == nullptr)
        return 1.0f;

    auto* monitorFromWindow = (MonitorFromWindowFn) (void*) GetProcAddress (user32, "MonitorFromWindow");
    auto* getDpiForMonitor = (GetDpiForMonitorFn) (void*) GetProcAddress (shcore, "GetDpiForMonitor");

    if (monitorFromWindow == nullptr || getDpiForMonitor == nullptr)
        return 1.0f;

    auto* monitor = monitorFromWindow (nativeHandle, 2 /*MONITOR_DEFAULTTONEAREST*/);
    unsigned int dpiX = 96;
    unsigned int dpiY = 96;

    if (monitor == nullptr || getDpiForMonitor (monitor, 0 /*MDT_EFFECTIVE_DPI*/, &dpiX, &dpiY) != 0 /*S_OK*/)
        return 1.0f;

    return juce::jlimit (1.0f, 3.0f, (float) dpiX / 96.0f);
}

void* setThreadDpiContext (void* context)
{
    using SetThreadDpiAwarenessContextFn = void* (*) (void*);

    static auto fn = (SetThreadDpiAwarenessContextFn) (void*)
        GetProcAddress (GetModuleHandleW (L"user32.dll"), "SetThreadDpiAwarenessContext");

    return fn != nullptr ? fn (context) : nullptr;
}

void* perMonitorAwareV2Context() { return (void*) (juce::pointer_sized_int) -4; }

int processDpiAwareness()
{
    using GetProcessDpiAwarenessFn = long (*) (void*, int*);

    static auto* shcore = LoadLibraryW (L"shcore.dll");
    static auto getProcessAwareness = shcore != nullptr
                                          ? (GetProcessDpiAwarenessFn) (void*) GetProcAddress (shcore, "GetProcessDpiAwareness")
                                          : nullptr;

    if (getProcessAwareness == nullptr)
        return -1;

    auto* process = OpenProcess (0x1000 /*PROCESS_QUERY_LIMITED_INFORMATION*/, 0, GetCurrentProcessId());
    int awareness = -1;

    if (process != nullptr)
    {
        if (getProcessAwareness (process, &awareness) != 0)
            awareness = -1;

        CloseHandle (process);
    }

    return awareness;
}
#endif

template <typename... Components>
void addAll (juce::Component& parent, Components&... components)
{
    (parent.addAndMakeVisible (components), ...);
}

void paintSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    g.setColour (IlanaTheme::accent());
    g.setFont (IlanaTheme::font (13.0f, true));
    g.drawText (text, area, juce::Justification::centredLeft);
}

class OscPage : public juce::Component,
                private juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater,
                private juce::Timer
{
    struct PhysicalControls
    {
        PhysicalControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : stiffness (state, prefix + "_string_stiffness", "STIFF"),
              pickup (state, prefix + "_string_pickup", "PICKUP"),
              excitePos (state, prefix + "_string_excite_pos", "EXCITE POS"),
              hardness (state, prefix + "_string_pick_hardness", "HARDNESS"),
              pickPos (state, prefix + "_string_pick_pos", "PICK POS"),
              slap (state, prefix + "_string_slap", "SLAP"),
              bowPressure (state, prefix + "_bow_pressure", "BOW PRESS"),
              bowSpeed (state, prefix + "_bow_speed", "BOW SPEED"),
              bridgeBuzz (state, prefix + "_bridge_buzz", "BRIDGE BUZZ"),
              fretRattle (state, prefix + "_fret_rattle", "FRET RATTLE"),
              hammer (state, prefix + "_hammer_hard", "HAMMER"),
              couple (state, prefix + "_couple", "COUPLING"),
              damper (state, prefix + "_damper", "DAMPER"),
              registerMap (state, prefix + "_register", "REGISTER") {}

        KnobControl stiffness, pickup, excitePos, hardness, pickPos;
        KnobControl bowPressure, bowSpeed, bridgeBuzz, fretRattle;
        KnobControl hammer, couple, damper, registerMap;
        ToggleControl slap;
    };

    struct OscControls
    {
        OscControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : on (state, prefix + "_on", "ON"),
              mode (state, prefix + "_mode", "MODE"),
              table (state, prefix + "_table", "TABLE"),
              excite (state, prefix + "_excite", "EXCITE"),
              frame (state, prefix + "_frame", "FRAME"),
              level (state, prefix + "_level", "LEVEL"),
              pan (state, prefix + "_pan", "PAN"),
              semi (state, prefix + "_semi", "SEMI"),
              fine (state, prefix + "_fine", "FINE"),
              unison (state, prefix + "_unison", "UNISON"),
              detune (state, prefix + "_detune", "DETUNE"),
              spread (state, prefix + "_spread", "SPREAD"),
              stringDecay (state, prefix + "_string_decay", "DECAY"),
              stringDamp (state, prefix + "_string_damp", "DAMP"),
              stringSustain (state, prefix + "_string_sustain", "SUSTAIN"),
              sampleTuned (state, prefix + "_sample_tuned", "TUNED"),
              sampleLoop (state, prefix + "_sample_loop", "LOOP"),
              sampleReverse (state, prefix + "_sample_reverse", "REVERSE"),
              sampleStart (state, prefix + "_sample_start", "START"),
              sampleEnd (state, prefix + "_sample_end", "END"),
              sampleFadeIn (state, prefix + "_sample_fade_in", "FADE IN"),
              sampleFadeOut (state, prefix + "_sample_fade_out", "FADE OUT"),
              chord (state, prefix + "_chord", "CHORD"),
              ampEnv (state, prefix + "_amp_env", "AMP ENV"),
              warp (state, prefix + "_warp", "WARP"),
              uniMode (state, prefix + "_uni_mode", "UNISON"),
              warpAmt (state, prefix + "_warp_amt", "WARP AMT"),
              uniBlend (state, prefix + "_uni_blend", "BLEND"),
              spectral (state, prefix + "_spectral", "SPECTRAL"),
              spectralAmt (state, prefix + "_spectral_amt", "SPEC AMT"),
              grainPosition (state, prefix + "_sample_start", "POSITION"),
              grainSize (state, prefix + "_grain_size", "SIZE"),
              grainDensity (state, prefix + "_grain_density", "DENSITY"),
              grainSpray (state, prefix + "_grain_spray", "SPRAY"),
              grainPitch (state, prefix + "_grain_pitch", "PITCH RND"),
              grainSpread (state, prefix + "_grain_spread", "STEREO"),
              warp2 (state, prefix + "_warp2", "WARP 2"),
              pdEnv (state, prefix + "_pd_env", "WARP ENV"),
              warp2Amt (state, prefix + "_warp2_amt", "WARP 2 AMT"),
              pdEnvAmt (state, prefix + "_pd_env_amt", "ENV AMT") {}

        ToggleControl on, sampleTuned, sampleLoop, sampleReverse;
        ComboControl mode, table, excite, chord, ampEnv, warp, uniMode, spectral;
        // M6: the PD chain's second stage and the warp (DCW) envelope.
        ComboControl warp2, pdEnv;
        KnobControl warp2Amt, pdEnvAmt;
        KnobControl frame, level, pan, semi, fine, unison, detune, spread;
        KnobControl stringDecay, stringDamp, stringSustain;
        KnobControl sampleStart, sampleEnd, sampleFadeIn, sampleFadeOut;
        KnobControl warpAmt, uniBlend, spectralAmt;
        KnobControl grainPosition, grainSize, grainDensity, grainSpray, grainPitch, grainSpread;
    };

public:
    std::function<void()> onModeChanged;

    explicit OscPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          subShape (p.apvts, "sub_shape", "SHAPE"),
          subOctave (p.apvts, "sub_octave", "OCT"),
          noiseLevel (p.apvts, "noise_level", "NOISE")
          , symOn (p.apvts, "sym_on", "ON"), symManual (p.apvts, "sym_manual", "MANUAL")
          , symAmount (p.apvts, "sym_amount", "AMOUNT"), symDecay (p.apvts, "sym_decay", "DECAY")
          , symCount (p.apvts, "sym_count", "STRINGS")
          , sbOn (p.apvts, "sb_on", "BOARD"), sbMix (p.apvts, "sb_mix", "BODY MIX"), sbTone (p.apvts, "sb_tone", "TONE")
          , sbSize (p.apvts, "sb_size", "SIZE"), stretch (p.apvts, "stretch", "STRETCH")
          , pedalRes (p.apvts, "pedal_res", "PEDAL RES"), mechKey (p.apvts, "mech_key", "KEY NOISE")
          , mechDamper (p.apvts, "mech_damper", "DAMPER NOISE"), mechPedal (p.apvts, "mech_pedal", "PEDAL NOISE")
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            controls[(size_t) i] = std::make_unique<OscControls> (p.apvts, prefix);
            waveDisplays[(size_t) i] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison",
                prefix + "_spread", prefix + "_detune", false, juce::String {},
                prefix + "_mode", i, oscColour (i), i == 0);
            loadButtons[(size_t) i] = std::make_unique<juce::TextButton> ("LOAD .WAV");
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            physical[(size_t) i] = std::make_unique<PhysicalControls> (p.apvts, prefix);
            auto& physicalControls = *physical[(size_t) i];
            addAll (*this, physicalControls.stiffness, physicalControls.pickup, physicalControls.excitePos,
                    physicalControls.hardness, physicalControls.pickPos, physicalControls.slap,
                    physicalControls.bowPressure, physicalControls.bowSpeed,
                    physicalControls.bridgeBuzz, physicalControls.fretRattle,
                    physicalControls.hammer, physicalControls.couple,
                    physicalControls.damper, physicalControls.registerMap);
        }

        addAll (*this, symOn, symManual, symAmount, symDecay, symCount);
        addAll (*this, sbOn, sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal);
        for (int i = 0; i < 6; ++i)
        {
            symNotes[(size_t) i] = std::make_unique<KnobControl> (p.apvts, "sym_note" + juce::String (i + 1),
                                                                    "NOTE " + juce::String (i + 1));
            addAndMakeVisible (*symNotes[(size_t) i]);
        }

        for (auto& item : controls)
        {
            auto& osc = *item;
            addAll (*this, osc.grainPosition, osc.grainSize, osc.grainDensity,
                    osc.grainSpray, osc.grainPitch, osc.grainSpread,
                    osc.spectral, osc.spectralAmt, osc.warp, osc.uniMode,
                    osc.warpAmt, osc.uniBlend, osc.warp2, osc.pdEnv, osc.warp2Amt, osc.pdEnvAmt);

            addAll (*this, osc.on, osc.mode, osc.table, osc.excite,
                    osc.frame, osc.level, osc.pan, osc.semi, osc.fine,
                    osc.unison, osc.detune, osc.spread, osc.stringDecay,
                    osc.stringDamp, osc.stringSustain, osc.sampleTuned,
                    osc.sampleLoop, osc.sampleReverse, osc.sampleStart,
                    osc.sampleEnd, osc.sampleFadeIn, osc.sampleFadeOut,
                    osc.chord, osc.ampEnv);
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            addAndMakeVisible (waveDisplay (i));
            setupLoadButton (loadButton (i),
                             juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table", 0);
            addAndMakeVisible (loadButton (i));
        }

        addAll (*this, subShape, subOctave, noiseLevel);

        // The TABLE lists open the wavetable browser.
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto* control = &controls[(size_t) i]->table;
            const auto id = juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table";
            const auto colour = oscColour (i);
            control->setPopupOverride ([this, control, id, colour]
            {
                TableBrowser::show (processorRef, id, colour, control->getComboBox());
            });
        }

        voiceSpread = std::make_unique<StripKnob> (p, "voice_spread", "Spread");
        unisonRandom = std::make_unique<StripKnob> (p, "unison_random", "Uni Phase");
        drift = std::make_unique<StripKnob> (p, "drift", "Drift");
        addAll (*this, *voiceSpread, *unisonRandom, *drift);

        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subOscLevel = std::make_unique<StripKnob> (p, "subosc_level", "Sub Level", -1, juce::Colour (0xffff9f43), false);
        noiseStrip = std::make_unique<StripKnob> (p, "noise_level", "Noise", -1, juce::Colour (0xffc8c8d0), false);
        addAll (*this, *subOscOn, *subOscLevel, *noiseStrip);
        noiseLevel.setVisible (false);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on" })
            processorRef.apvts.addParameterListener (id, this);

        // Phase Plant style: remove any oscillator, add the next hidden one.
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            removeButtons[(size_t) i] = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
            removeButtons[(size_t) i]->setTooltip ("Remove this oscillator (switches it off and hides it)");
            removeButtons[(size_t) i]->onClick = [this, i]
            {
                processorRef.removeOscillator (i);
                updateModeVisibility();
                updateEnabled();
            };
            addAndMakeVisible (*removeButtons[(size_t) i]);
        }

        addButton.setButtonText ("+  ADD OSCILLATOR");
        addButton.setTooltip ("Add the next oscillator, switched on");
        addButton.onClick = [this]
        {
            for (int i = 0; i < OscillatorIds::count; ++i)
                if (! processorRef.isOscillatorShown (i))
                {
                    processorRef.addOscillator (i);
                    break;
                }

            updateModeVisibility();
            updateEnabled();
        };
        addAndMakeVisible (addButton);

        lastRevealVersion = processorRef.getRevealVersion();
        updateModeVisibility();
        updateEnabled();
        startTimerHz (5);
    }

    ~OscPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on" })
            processorRef.apvts.removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String&, float) override
    {
        // Parameter changes can arrive on the audio thread (host automation),
        // so defer the GUI work to the message thread.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        updateModeVisibility();
        updateEnabled();
    }

    // Patch loads change which oscillators are shown.
    void timerCallback() override
    {
        if (const auto version = processorRef.getRevealVersion(); version != lastRevealVersion)
        {
            lastRevealVersion = version;
            updateModeVisibility();
            updateEnabled();
        }
    }

    // The viewport's height: cards are sized as if three fill it, and more
    // than that scroll.
    void setAvailableHeight (int height) { availableHeight = height; }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (int band = 0; band < OscillatorIds::count; ++band)
        {
            if (! shown[(size_t) band])
                continue;

            const auto bounds = bandBounds (band);
            const auto tint = oscColour (band);

            IlanaTheme::paintCard (g, bounds.toFloat(), 6.0f, tint);

            // Hardware screws live in the bottom corners so they never crowd
            // the oscillator title or the LOAD button.
            IlanaTheme::paintScrew (g, { (float) bounds.getX() + 12.0f, (float) bounds.getBottom() - 12.0f }, 9.0f);
            IlanaTheme::paintScrew (g, { (float) bounds.getRight() - 12.0f, (float) bounds.getBottom() - 12.0f }, 9.0f);

            const auto strip = juce::Rectangle<float> ((float) bounds.getX() + 2.0f, (float) bounds.getY() + 6.0f,
                                                       3.0f, (float) bounds.getHeight() - 12.0f);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (strip, 1.5f);

            const std::array<const char*, 4> modeNames { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR" };
            const auto mode = juce::jlimit (0, 3, getMode (band));

            g.setColour (tint);
            g.setFont (IlanaTheme::font (13.0f, true));
            g.fillEllipse ((float) bounds.getX() + 14.0f, (float) bounds.getY() + 14.0f, 6.0f, 6.0f);
            g.drawText ("OSC " + juce::String (band + 1),
                        juce::Rectangle<int> (bounds.getX() + 28, bounds.getY() + 9, 60, 16),
                        juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (11.0f, true));
            g.drawText (modeNames[(size_t) mode],
                        juce::Rectangle<int> (bounds.getX() + 72, bounds.getY() + 10, 150, 14),
                        juce::Justification::centredLeft);

            if (! controlBay[(size_t) band].isEmpty())
                IlanaTheme::paintRecessedPanel (g, controlBay[(size_t) band].toFloat(), 6.0f);

            if (! chainLabel[(size_t) band].isEmpty())
            {
                IlanaTheme::paintRecessedPanel (g, chainBay[(size_t) band].toFloat(), 6.0f);
                const auto label = chainLabel[(size_t) band];
                g.setColour (tint.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (11.0f, true));
                g.drawText ("WARP CHAIN", label.withHeight (18), juce::Justification::centredLeft);
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.setFont (IlanaTheme::font (10.0f));
                g.drawFittedText ("second stage and\nthe DCW envelope", label.withTrimmedTop (18), juce::Justification::topLeft, 2);
            }
        }

        if (! subStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, subStrip.toFloat(), 6.0f);
            g.setColour (juce::Colour (0xffff9f43));
            g.setFont (IlanaTheme::font (12.0f, true));
            g.drawText ("SUB", subStrip.withWidth (60).withTrimmedLeft (14), juce::Justification::centredLeft);
        }

        if (! voiceStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, voiceStrip.toFloat(), 6.0f);
            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (12.0f, true));
            g.drawText ("VOICE", voiceStrip.withWidth (70).withTrimmedLeft (14), juce::Justification::centredLeft);
        }
        if (! symCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, symCard.toFloat(), 6.0f);
            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (12.0f, true));
            g.drawText ("SYMPATHETIC STRINGS", symCard.withHeight (symHeaderHeight).withTrimmedLeft (14),
                        juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (11.0f));
            g.drawText ("shared drone strings that ring with everything you play",
                        symCard.withHeight (symHeaderHeight).withTrimmedLeft (180).withTrimmedRight (90),
                        juce::Justification::centredLeft);
        }

        if (! keysCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, keysCard.toFloat(), 6.0f);
            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (12.0f, true));
            g.drawText ("ACOUSTIC KEYS", keysCard.withHeight (symHeaderHeight).withTrimmedLeft (14),
                        juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (11.0f));
            g.drawText ("soundboard, tuning, sustain pedal (CC64) and the action's noises; for Physical oscillators with the Hammer",
                        keysCard.withHeight (symHeaderHeight).withTrimmedLeft (180).withTrimmedRight (14),
                        juce::Justification::centredLeft);
        }
    }

    static juce::Colour oscColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xff5b8cff);
            case 2: return juce::Colour (0xffffd447);
            case 3: return juce::Colour (0xff6fe3c1);
            case 4: return juce::Colour (0xffff7f9e);
            case 5: return juce::Colour (0xffb28aff);
            default: return IlanaTheme::accent();
        }
    }

    // Height the page needs so every card keeps its minimum size; a Physical
    // card has an extra row of knobs, so it gets extra height.
    int getMinimumHeight() const
    {
        auto height = pageMargin * 2 + bandGap * 2 + stripHeight + symCardHeight();

        for (int band = 0; band < OscillatorIds::count; ++band)
            if (shown[(size_t) band])
                height += heightOfBand (band) + bandGap;

        return height + (anyHidden() ? addCardHeight + bandGap : 0) + keysCardHeight + bandGap;
    }

    void resized() override
    {
        // Size cards as the three-oscillator page did; extra ones scroll.
        const auto fitHeight = availableHeight > 0 ? availableHeight : getHeight();
        bandHeight = juce::jlimit (minBandHeight, 176,
                                   (fitHeight - pageMargin * 2 - bandGap * 4 - stripHeight - symCardHeight()) / 3);
        auto area = getLocalBounds().reduced (12, pageMargin);

        for (int band = 0; band < OscillatorIds::count; ++band)
        {
            if (! shown[(size_t) band])
                continue;

            layoutBand (area.removeFromTop (heightOfBand (band)), band);
            area.removeFromTop (bandGap);
        }

        addButton.setVisible (anyHidden());
        if (anyHidden())
        {
            addButton.setBounds (area.removeFromTop (addCardHeight));
            area.removeFromTop (bandGap);
        }

        // One strip: the sub and noise on the left, voice settings on the right.
        auto strip = area.removeFromTop (stripHeight);
        subStrip = strip.removeFromLeft (strip.getWidth() * 58 / 100);
        strip.removeFromLeft (8);
        voiceStrip = strip;

        auto row = subStrip.reduced (6, 2);
        row.removeFromLeft (54);
        subOscOn->setBounds (row.removeFromLeft (70).reduced (2, 2));
        row.removeFromLeft (6);
        subShape.setBounds (row.removeFromLeft (row.getWidth() * 22 / 100).reduced (3, 0));
        subOctave.setBounds (row.removeFromLeft (row.getWidth() * 22 / 100).reduced (3, 0));
        row.removeFromLeft (10);
        const auto knobWidth = row.getWidth() / 2;
        subOscLevel->setBounds (row.removeFromLeft (knobWidth));
        noiseStrip->setBounds (row);

        row = voiceStrip.reduced (6, 3);
        row.removeFromLeft (64);
        const auto third = row.getWidth() / 3;
        voiceSpread->setBounds (row.removeFromLeft (third));
        unisonRandom->setBounds (row.removeFromLeft (third));
        drift->setBounds (row);

        // The shared sympathetic strings: a one-line header with their switch,
        // which opens into their settings (and the notes, in MANUAL).
        area.removeFromTop (bandGap);
        symCard = area.removeFromTop (symCardHeight());
        auto symArea = symCard;
        auto header = symArea.removeFromTop (symHeaderHeight);
        // ToggleControl keeps 13 px above its button for a label; place it so
        // the button itself sits centred on the header line.
        symOn.setBounds (header.getRight() - 84, header.getCentreY() - 13 - 10, 76, 13 + 20);

        if (readBool ("sym_on"))
        {
            symArea.reduce (10, 0);
            layoutSlots (symArea.removeFromTop (symRowHeight), { &symAmount, &symDecay, &symCount, &symManual });

            if (readBool ("sym_manual"))
                layoutSlots (symArea.removeFromTop (symRowHeight),
                             { symNotes[0].get(), symNotes[1].get(), symNotes[2].get(),
                               symNotes[3].get(), symNotes[4].get(), symNotes[5].get() });
        }

        area.removeFromTop (bandGap);
        keysCard = area.removeFromTop (keysCardHeight);
        auto keysArea = keysCard.withTrimmedTop (symHeaderHeight).reduced (10, 0);
        layoutSlots (keysArea.removeFromTop (symRowHeight),
                     { &sbOn, &sbMix, &sbTone, &sbSize, &stretch, &pedalRes, &mechKey, &mechDamper, &mechPedal });
    }

private:
    int bandHeight = 137;
    int availableHeight = 0;
    int lastRevealVersion = -1;
    std::array<bool, OscillatorIds::count> shown { true, true, true };
    static constexpr int bandGap = 6;
    static constexpr int addCardHeight = 40;

    bool anyHidden() const
    {
        return std::find (shown.begin(), shown.end(), false) != shown.end();
    }

    int numShown() const { return (int) std::count (shown.begin(), shown.end(), true); }
    static constexpr int stripHeight = 50;
    static constexpr int pageMargin = 6;
    static constexpr int symHeaderHeight = 28;
    static constexpr int symRowHeight = 64;
    static constexpr int keysCardHeight = 28 + 64 + 6;

    // The sympathetic card: just its header while the strings are off, one
    // row of settings when on, plus the notes in MANUAL.
    int symCardHeight() const
    {
        if (! readBool ("sym_on"))
            return symHeaderHeight;

        return symHeaderHeight + 6 + symRowHeight * (readBool ("sym_manual") ? 2 : 1);
    }
    static constexpr int minBandHeight = 124;
    static constexpr int physicalExtra = 140;

    static constexpr int warpChainExtra = 58;

    // A wavetable oscillator with a warp picked opens a row for the PD
    // chain's second stage and the warp envelope.
    bool showsWarpChain (int index) const
    {
        if (getMode (index) != 0)
            return false;

        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]);
        const auto* warp = processorRef.apvts.getRawParameterValue (prefix + "_warp");
        const auto* warp2 = processorRef.apvts.getRawParameterValue (prefix + "_warp2");
        return (warp != nullptr && warp->load() > 0.5f) || (warp2 != nullptr && warp2->load() > 0.5f);
    }

    int heightOfBand (int index) const
    {
        return bandHeight + (getMode (index) == 1 ? physicalExtra : 0) + (showsWarpChain (index) ? warpChainExtra : 0);
    }

    bool readBool (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load() > 0.5f;

        return true;
    }

    juce::Rectangle<int> bandBounds (int index) const
    {
        const auto area = getLocalBounds().reduced (12, pageMargin);
        auto y = area.getY();

        for (int band = 0; band < index; ++band)
            if (shown[(size_t) band])
                y += heightOfBand (band) + bandGap;

        return { area.getX(), y, area.getWidth(), heightOfBand (index) };
    }

    WaveDisplay& waveDisplay (int index)
    {
        return *waveDisplays[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    juce::TextButton& loadButton (int index)
    {
        return *loadButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    static float slotWeight (juce::Component* item)
    {
        if (dynamic_cast<ComboControl*> (item) != nullptr)
            return 1.5f;

        if (dynamic_cast<ToggleControl*> (item) != nullptr)
            return 0.55f;

        return 1.1f;
    }

    static void layoutSlots (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
    {
        if (items.empty())
            return;

        const auto totalWidth = area.getWidth();
        auto totalWeight = 0.0f;

        for (auto* item : items)
            totalWeight += slotWeight (item);

        if (totalWeight <= 0.0f)
            return;

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto width = i + 1 == items.size()
                                   ? area.getWidth()
                                   : juce::jmax (1, (int) std::round ((float) totalWidth * slotWeight (items[i]) / totalWeight));

            auto cell = area.removeFromLeft (width).reduced (3);

            if (items[i] != nullptr)
                items[i]->setBounds (cell);
        }
    }

    int getMode (int oscIndex) const
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]) + "_mode";
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    void layoutBand (juce::Rectangle<int> band, int index)
    {
        auto titleStrip = band.reduced (8).removeFromTop (18);
        removeButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (22).withSizeKeepingCentre (20, 15));
        titleStrip.removeFromRight (6);
        loadButton (index).setBounds (titleStrip.removeFromRight (86).withSizeKeepingCentre (86, 15));

        auto content = band.reduced (8);
        content.removeFromTop (20);
        waveDisplay (index).setBounds (content.removeFromLeft (276));
        content.removeFromLeft (8);

        auto topRow = content.removeFromTop (38);
        content.removeFromTop (3);
        chainLabel[(size_t) index] = {};

        if (showsWarpChain (index))
        {
            auto chainRow = content.removeFromBottom (warpChainExtra).withTrimmedTop (4);
            chainBay[(size_t) index] = chainRow.expanded (4, 0);
            auto& chainControls = *controls[(size_t) index];
            chainLabel[(size_t) index] = chainRow.removeFromLeft (120).reduced (8, 6);
            layoutSlots (chainRow, { &chainControls.warp2, &chainControls.warp2Amt, &chainControls.pdEnv, &chainControls.pdEnvAmt });
        }

        auto bottomRow = content;

        const auto mode = getMode (index);
        const auto isSample = mode == 2;
        const auto isString = mode == 1;
        const auto isWavetable = mode == 0;
        const auto isGranular = mode == 3;


        std::vector<juce::Component*> top;
        std::vector<juce::Component*> bottom;

        const auto addTop = [&top] (juce::Component* item) { if (item != nullptr) top.push_back (item); };
        const auto addBottom = [&bottom] (juce::Component* item) { if (item != nullptr) bottom.push_back (item); };

        auto& osc = *controls[(size_t) index];

        if (isGranular)
        {
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.on, &osc.mode, &osc.sampleTuned, &osc.sampleReverse,
                                   &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (bottomRow, { &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                                      &osc.grainSpray, &osc.grainPitch, &osc.grainSpread,
                                      &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune });
            return;
        }

        if (isString)
        {
            auto& physicalControls = *physical[(size_t) index];
            auto middleRow = bottomRow.removeFromTop (bottomRow.getHeight() / 3);
            auto extraRow = bottomRow.removeFromTop (bottomRow.getHeight() / 2);
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.on, &osc.mode, &osc.excite, &physicalControls.slap,
                                   &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (middleRow, { &osc.stringDecay, &osc.stringDamp, &osc.stringSustain,
                                      &physicalControls.stiffness, &physicalControls.pickup,
                                      &physicalControls.excitePos, &physicalControls.hardness,
                                      &physicalControls.pickPos });
            layoutSlots (extraRow, { &physicalControls.hammer, &physicalControls.bowPressure, &physicalControls.bowSpeed,
                                     &physicalControls.bridgeBuzz, &physicalControls.fretRattle,
                                     &physicalControls.couple, &physicalControls.damper, &physicalControls.registerMap });
            layoutSlots (bottomRow, { &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune, &osc.uniBlend, &osc.spread });
            return;
        }

        addTop (&osc.on);
        addTop (&osc.mode);
        addTop (isSample ? (juce::Component*) &osc.sampleTuned
                         : (isString ? (juce::Component*) &osc.excite : (juce::Component*) &osc.table));
        addTop (isSample ? (juce::Component*) &osc.sampleLoop : nullptr);
        addTop (isSample ? (juce::Component*) &osc.sampleReverse : nullptr);
        addTop (isWavetable ? (juce::Component*) &osc.warp : nullptr);
        addTop (isWavetable ? (juce::Component*) &osc.spectral : nullptr);
        addTop (&osc.uniMode);
        addTop (&osc.chord);
        addTop (&osc.ampEnv);

        addBottom (isSample ? (juce::Component*) &osc.sampleStart
                            : (isString ? (juce::Component*) &osc.stringDecay : (juce::Component*) &osc.frame));
        addBottom (isWavetable ? (juce::Component*) &osc.warpAmt : nullptr);
        addBottom (isWavetable ? (juce::Component*) &osc.spectralAmt : nullptr);
        addBottom (isSample ? (juce::Component*) &osc.sampleEnd
                            : (isString ? (juce::Component*) &osc.stringDamp : nullptr));
        addBottom (isSample ? (juce::Component*) &osc.sampleFadeIn
                            : (isString ? (juce::Component*) &osc.stringSustain : nullptr));
        addBottom (isSample ? (juce::Component*) &osc.sampleFadeOut : nullptr);
        addBottom (&osc.level);
        addBottom (&osc.pan);
        addBottom (&osc.semi);
        addBottom (&osc.fine);
        addBottom (&osc.unison);
        addBottom (&osc.detune);
        addBottom (&osc.uniBlend);
        addBottom (&osc.spread);

        controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
        layoutSlots (topRow, top);
        layoutSlots (bottomRow, bottom);
    }

    void setupLoadButton (juce::TextButton& button, const juce::String& tableId, int tableChoiceOffset)
    {
        button.setTooltip ("Load a wavetable (.wav of single-cycle frames), or turn any recording into a wavetable");
        button.onClick = [this, &button, tableId, tableChoiceOffset]
        {
            if (chooserOpen)
                return;

            juce::PopupMenu menu;
            menu.addItem (1, "Load wavetable file...");
            menu.addItem (2, "Make a wavetable from any audio...");
            menu.addSeparator();
            menu.addItem (3, "(Any audio: the pitch is detected and one cycle per frame is taken across the file)", false);

            juce::Component::SafePointer<OscPage> safeMenu (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                                [safeMenu, tableId, tableChoiceOffset] (int result)
                                {
                                    if (safeMenu != nullptr && (result == 1 || result == 2))
                                        safeMenu->chooseTable (tableId, tableChoiceOffset,
                                                               result == 2 ? Wavetable::LoadMode::Resynthesize
                                                                           : Wavetable::LoadMode::Automatic);
                                });
        };
    }

    void chooseTable (const juce::String& tableId, int tableChoiceOffset, Wavetable::LoadMode mode)
    {
        chooserOpen = true;

        for (int index = 0; index < OscillatorIds::count; ++index)
            loadButton (index).setEnabled (false);

        if (tableChooser == nullptr)
            tableChooser = std::make_unique<juce::FileChooser> (
                "Load Wavetable or Audio",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

        juce::Component::SafePointer<OscPage> safeThis (this);

        tableChooser->launchAsync (juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis, tableId, tableChoiceOffset, mode] (const juce::FileChooser& chooser)
                                   {
                                       if (safeThis == nullptr)
                                           return;

                                       safeThis->chooserOpen = false;
                                       safeThis->updateEnabled();

                                       const auto file = chooser.getResult();

                                       if (! file.existsAsFile())
                                           return;

                                       const auto factoryCount = TableFactory::getNumFactoryTables();
                                       const auto domain = juce::jmax (0, safeThis->readTableChoiceIndex (tableId) - tableChoiceOffset);
                                       const auto slot = domain >= factoryCount
                                                             ? juce::jlimit (0, IlanaSynthAudioProcessor::numUserSlots - 1,
                                                                             domain - factoryCount)
                                                             : 0;

                                       if (safeThis->processorRef.loadUserWavetable (slot, file, mode))
                                       {
                                           if (auto* parameter = safeThis->processorRef.apvts.getParameter (tableId))
                                               parameter->setValueNotifyingHost (
                                                   parameter->convertTo0to1 ((float) (tableChoiceOffset + factoryCount + slot)));
                                       }
                                   });
    }

    // Every child component that belongs to oscillator i.
    std::vector<juce::Component*> componentsOf (int i)
    {
        auto& osc = *controls[(size_t) i];
        auto& phys = *physical[(size_t) i];
        return { &osc.on, &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.mode, &osc.table,
                 &osc.excite, &osc.chord, &osc.ampEnv, &osc.warp, &osc.uniMode, &osc.spectral, &osc.frame,
                 &osc.level, &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune, &osc.spread,
                 &osc.stringDecay, &osc.stringDamp, &osc.stringSustain, &osc.sampleStart, &osc.sampleEnd,
                 &osc.sampleFadeIn, &osc.sampleFadeOut, &osc.warpAmt, &osc.uniBlend, &osc.spectralAmt,
                 &osc.grainPosition, &osc.grainSize, &osc.grainDensity, &osc.grainSpray, &osc.grainPitch,
                 &osc.grainSpread, &osc.warp2, &osc.pdEnv, &osc.warp2Amt, &osc.pdEnvAmt, &phys.stiffness, &phys.pickup, &phys.excitePos, &phys.hardness,
                 &phys.pickPos, &phys.bowPressure, &phys.bowSpeed, &phys.bridgeBuzz, &phys.fretRattle,
                 &phys.hammer, &phys.couple, &phys.damper, &phys.registerMap, &phys.slap, &waveDisplay (i), &loadButton (i), removeButtons[(size_t) i].get() };
    }

    void updateModeVisibility()
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            shown[(size_t) i] = processorRef.isOscillatorShown (i);

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            for (auto* component : componentsOf (i))
                component->setVisible (shown[(size_t) i]);

            if (! shown[(size_t) i])
                continue;

            // Keep one oscillator on the page.
            removeButtons[(size_t) i]->setVisible (numShown() > 1);

            const auto mode = getMode (i);
            const auto stringVisible = mode == 1;
            auto& physicalControls = *physical[(size_t) i];

            for (juce::Component* control : { (juce::Component*) &physicalControls.stiffness,
                                              (juce::Component*) &physicalControls.pickup,
                                              (juce::Component*) &physicalControls.excitePos,
                                              (juce::Component*) &physicalControls.hardness,
                                              (juce::Component*) &physicalControls.pickPos,
                                              (juce::Component*) &physicalControls.slap,
                                              (juce::Component*) &physicalControls.bridgeBuzz,
                                              (juce::Component*) &physicalControls.fretRattle,
                                              (juce::Component*) &physicalControls.couple,
                                              (juce::Component*) &physicalControls.damper,
                                              (juce::Component*) &physicalControls.registerMap })
                control->setVisible (stringVisible);

            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            const auto bow = stringVisible
                             && processorRef.apvts.getRawParameterValue (prefix + "_excite")->load() == 4.0f;
            physicalControls.bowPressure.setVisible (bow);
            physicalControls.bowSpeed.setVisible (bow);
            physicalControls.hammer.setVisible (stringVisible
                                                && processorRef.apvts.getRawParameterValue (prefix + "_excite")->load() == 5.0f);

            auto& osc = *controls[(size_t) i];
            osc.table.setVisible (mode == 0);
            osc.frame.setVisible (mode == 0);
            osc.excite.setVisible (mode == 1);
            osc.stringDecay.setVisible (mode == 1);
            osc.stringDamp.setVisible (mode == 1);
            osc.stringSustain.setVisible (mode == 1);
            osc.sampleTuned.setVisible (mode >= 2);
            osc.sampleLoop.setVisible (mode == 2);
            osc.sampleReverse.setVisible (mode >= 2);
            osc.sampleStart.setVisible (mode == 2);
            osc.sampleEnd.setVisible (mode == 2);
            osc.sampleFadeIn.setVisible (mode == 2);
            osc.sampleFadeOut.setVisible (mode == 2);
            osc.warp.setVisible (mode == 0);
            osc.warpAmt.setVisible (mode == 0);
            osc.spectral.setVisible (mode == 0);
            osc.spectralAmt.setVisible (mode == 0);
            const auto chain = showsWarpChain (i);
            osc.warp2.setVisible (chain);
            osc.warp2Amt.setVisible (chain);
            osc.pdEnv.setVisible (chain);
            osc.pdEnvAmt.setVisible (chain);
            osc.grainPosition.setVisible (mode == 3);
            osc.grainSize.setVisible (mode == 3);
            osc.grainDensity.setVisible (mode == 3);
            osc.grainSpray.setVisible (mode == 3);
            osc.grainPitch.setVisible (mode == 3);
            osc.grainSpread.setVisible (mode == 3);
            osc.uniBlend.setVisible (mode != 3);
            osc.spread.setVisible (mode != 3);
        }

        const auto symOnNow = readBool ("sym_on");
        const auto manual = symOnNow && readBool ("sym_manual");

        for (auto* control : { (juce::Component*) &symAmount, (juce::Component*) &symDecay,
                               (juce::Component*) &symCount, (juce::Component*) &symManual })
            control->setVisible (symOnNow);

        for (auto& note : symNotes)
            note->setVisible (manual);

        resized();
        repaint();
        if (onModeChanged != nullptr)
            onModeChanged();
    }

    static void setGroupEnabled (std::initializer_list<juce::Component*> controls, bool enabled)
    {
        for (auto* control : controls)
        {
            control->setEnabled (enabled);
            control->setAlpha (enabled ? 1.0f : 0.3f);
        }
    }

    void updateEnabled()
    {
        const auto boardOn = readBool ("sb_on");
        for (auto* control : { &sbMix, &sbTone, &sbSize })
            control->setAlpha (boardOn ? 1.0f : 0.4f);

        // Manual notes past STRINGS are not sounding.
        const auto stringCount = juce::roundToInt (processorRef.apvts.getRawParameterValue ("sym_count")->load());
        for (int i = 0; i < (int) symNotes.size(); ++i)
            symNotes[(size_t) i]->setAlpha (i < stringCount ? 1.0f : 0.4f);

        for (int index = 0; index < OscillatorIds::count; ++index)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            const auto enabled = readBool (prefix + "_on");
            auto& physicalControls = *physical[(size_t) index];
            setGroupEnabled ({ &physicalControls.stiffness, &physicalControls.pickup,
                               &physicalControls.excitePos, &physicalControls.hardness,
                               &physicalControls.pickPos, &physicalControls.slap,
                               &physicalControls.bowPressure, &physicalControls.bowSpeed,
                               &physicalControls.bridgeBuzz, &physicalControls.fretRattle,
                               &physicalControls.hammer, &physicalControls.couple,
                               &physicalControls.damper, &physicalControls.registerMap }, enabled);

            auto& osc = *controls[(size_t) index];
            setGroupEnabled ({ &osc.mode, &osc.table, &osc.excite, &osc.frame, &osc.level,
                               &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune,
                               &osc.spread, &osc.stringDecay, &osc.stringDamp, &osc.stringSustain,
                               &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse,
                               &osc.sampleStart, &osc.sampleEnd, &osc.sampleFadeIn, &osc.sampleFadeOut,
                               &osc.chord, &osc.warp, &osc.warpAmt, &osc.spectral, &osc.spectralAmt,
                               &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                               &osc.grainSpray, &osc.grainPitch, &osc.grainSpread,
                               &osc.uniMode, &osc.uniBlend, &osc.ampEnv,
                               &osc.warp2, &osc.warp2Amt, &osc.pdEnv, &osc.pdEnvAmt }, enabled);

            // An amount whose stage or envelope is Off does nothing: dim it.
            if (enabled)
            {
                osc.warp2Amt.setAlpha (readChoice (prefix + "_warp2") > 0 ? 1.0f : 0.4f);
                osc.pdEnvAmt.setAlpha (readChoice (prefix + "_pd_env") > 0 ? 1.0f : 0.4f);
            }

            const auto alpha = enabled ? 1.0f : 0.3f;
            waveDisplay (index).setAlpha (alpha);
            waveDisplay (index).setEnabled (enabled);
            loadButton (index).setEnabled (! chooserOpen && enabled);
            loadButton (index).setAlpha (alpha);
        }
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

    int readTableChoiceIndex (const juce::String& tableId) const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId)))
            return param->getIndex();

        return 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waveDisplays;
    std::array<juce::Rectangle<int>, OscillatorIds::count> controlBay {};
    std::array<juce::Rectangle<int>, OscillatorIds::count> chainLabel {}, chainBay {};

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, removeButtons;
    juce::TextButton addButton;
    std::unique_ptr<juce::FileChooser> tableChooser;
    std::array<std::unique_ptr<PhysicalControls>, OscillatorIds::count> physical;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<StripKnob> voiceSpread, unisonRandom, drift;
    juce::Rectangle<int> voiceStrip;
    juce::Rectangle<int> symCard;
    ToggleControl symOn, symManual;
    KnobControl symAmount, symDecay, symCount;

    // Acoustic keys (M4): soundboard, stretch tuning, pedal resonance and
    // the mechanism's noises, shared by every voice.
    juce::Rectangle<int> keysCard;
    ToggleControl sbOn;
    KnobControl sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal;
    std::array<std::unique_ptr<KnobControl>, 6> symNotes;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<StripKnob> subOscLevel, noiseStrip;
    juce::Rectangle<int> subStrip;

    std::array<std::unique_ptr<OscControls>, OscillatorIds::count> controls;
    ComboControl subShape, subOctave;
    KnobControl noiseLevel;
};

class OscPageViewport : public juce::Viewport
{
public:
    explicit OscPageViewport (IlanaSynthAudioProcessor& processor)
    {
        setScrollBarsShown (true, false);
        auto* page = new OscPage (processor);
        page->onModeChanged = [this] { resized(); };
        setViewedComponent (page, true);
    }

    void resized() override
    {
        juce::Viewport::resized();
        if (auto* page = dynamic_cast<OscPage*> (getViewedComponent()))
        {
            // Scroll only when the cards cannot fit at their minimum height.
            page->setAvailableHeight (getHeight());
            const auto needed = page->getMinimumHeight();
            const auto scrolls = needed > getHeight();
            page->setSize (juce::jmax (1, getWidth() - (scrolls ? getScrollBarThickness() : 0)),
                           juce::jmax (getHeight(), needed));
        }
    }
};

// One filter: its type grid, slope switch and only the knobs its model uses.
class FilterPanel : public juce::Component,
                    private juce::Timer
{
public:
    FilterPanel (IlanaSynthAudioProcessor& p, int index, juce::Colour colourIn)
        : processorRef (p),
          prefix (index == 1 ? "f1" : "f2"),
          title ("FILTER " + juce::String (index)),
          colour (colourIn),
          grid (p.apvts, prefix + "_type", colourIn),
          slope (p.apvts, prefix + "_slope", colourIn),
          cutoff (p.apvts, prefix + "_cutoff", "CUTOFF", colourIn, false),
          reso (p.apvts, prefix + "_reso", "RESO", colourIn, false),
          drive (p.apvts, prefix + "_drive", "DRIVE", colourIn, false),
          env (p.apvts, prefix + "_env", "ENV AMT", colourIn, false),
          key (p.apvts, prefix + "_keytrack", "KEY TRK", colourIn, false),
          fm (p.apvts, prefix + "_fm", "AUDIO FM", colourIn, false),
          morph (p.apvts, prefix + "_morph", "MORPH", colourIn, false)
    {
        addAll (*this, grid, slope, cutoff, reso, drive, env, key, fm, morph);
        refreshType();
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour.withAlpha (0.35f));

        auto header = getLocalBounds().reduced (12, 0).removeFromTop (28);
        g.setColour (colour);
        g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
        header.removeFromLeft (14);

        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText (title, header, juce::Justification::centredLeft);

        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (13.0f, true)), title);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (IlanaTheme::font (12.0f));
        g.drawText (FilterType::getNames()[type], header.withTrimmedLeft (titleWidth + 10), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        auto header = area.removeFromTop (28);
        slope.setBounds (header.removeFromRight (96).reduced (0, 5));

        grid.setBounds (area.removeFromTop (juce::jlimit (52, 64, getHeight() / 4)));
        area.removeFromTop (4);
        area.removeFromBottom (6);

        std::vector<juce::Component*> knobs { &cutoff, &reso, &drive, &env, &key, &fm };

        if (morph.isVisible())
            knobs.push_back (&morph);

        layoutRow (area, knobs);
    }

private:
    // Comb and formant have no slope; only Formant and Morph use MORPH.
    void refreshType()
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            type = juce::jlimit (0, FilterType::Count - 1, (int) value->load());

        const auto hasSlope = type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant;
        slope.setVisible (hasSlope);
        morph.setVisible (FilterType::usesMorph (type));
        resized();
        repaint();
    }

    void timerCallback() override
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            if ((int) value->load() != type)
                refreshType();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix, title;
    juce::Colour colour;
    FilterTypeGrid grid;
    SlopeSwitch slope;
    KnobControl cutoff, reso, drive, env, key, fm, morph;
    int type = -1;
};

class FilterPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit FilterPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          panel1 (p, 1, juce::Colour (0xffff4fd8)),
          panel2 (p, 2, juce::Colour (0xffb28aff)),
          flow (p),
          balance (p.apvts, "filter_balance", "", IlanaTheme::accent(), true),
          resOn (p.apvts, "res_on", "ON"),
          resAmount (p.apvts, "res_amount", "AMOUNT", resonatorColour(), false),
          resDecay (p.apvts, "res_decay", "DECAY", resonatorColour(), false),
          resOffset (p.apvts, "res_offset", "OFFSET", resonatorColour(), false),
          resKeytrack (p.apvts, "res_keytrack", "KEY TRK", resonatorColour(), false)
    {
        addAll (*this, filterDisplay, panel1, panel2, flow, balance,
                resOn, resAmount, resDecay, resOffset, resKeytrack);
        startTimerHz (8);
    }

    static juce::Colour resonatorColour() { return juce::Colour (0xff6fe3c1); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintSectionTitle (g, "RESPONSE", { 14, 10, 200, 16 });
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText ("drag the markers to set cutoff and resonance", juce::Rectangle<int> (100, 10, 400, 16),
                    juce::Justification::centredLeft);

        paintSectionTitle (g, "SIGNAL FLOW", flowTitle);

        // Balance: its own small card, with a hint when serial makes it idle.
        {
            const auto* parallel = processorRef.apvts.getRawParameterValue ("filters_parallel");
            const auto active = parallel != nullptr && parallel->load() > 0.5f;
            IlanaTheme::paintCard (g, balanceCard.toFloat(), 7.0f, IlanaTheme::accent().withAlpha (active ? 0.35f : 0.12f));
            g.setColour (active ? IlanaTheme::accent() : juce::Colours::white.withAlpha (0.45f));
            g.setFont (IlanaTheme::font (11.5f, true));
            g.drawText ("BALANCE", balanceCard.withHeight (24), juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (10.0f));
            g.drawText (active ? "parallel mix" : "parallel only", balanceCard.withTrimmedTop (balanceCard.getHeight() - 18),
                        juce::Justification::centred);
        }

        IlanaTheme::paintCard (g, resonatorCard.toFloat(), 7.0f, resonatorColour().withAlpha (0.35f));
        g.setColour (resonatorColour());
        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText ("RESONATOR", resonatorCard.reduced (12, 0).removeFromTop (26), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText ("tuned body after the filters", resonatorCard.reduced (12, 0).removeFromTop (26),
                    juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (18);

        const auto panelHeight = juce::jlimit (200, 260, area.getHeight() * 9 / 20);
        const auto bottomHeight = juce::jlimit (128, 170, area.getHeight() / 4);
        const auto displayHeight = juce::jmax (90, area.getHeight() - panelHeight - bottomHeight - 16);

        filterDisplay.setBounds (area.removeFromTop (displayHeight));
        area.removeFromTop (8);

        auto panels = area.removeFromTop (panelHeight);
        panel1.setBounds (panels.removeFromLeft ((panels.getWidth() - 10) / 2));
        panels.removeFromLeft (10);
        panel2.setBounds (panels);

        area.removeFromTop (8);
        auto bottom = area.removeFromTop (bottomHeight);

        auto flowArea = bottom.removeFromLeft (bottom.getWidth() / 2 - 5);
        flowTitle = flowArea.removeFromTop (18);
        balanceCard = flowArea.removeFromRight (104);
        balance.setBounds (balanceCard.reduced (8, 0).withTrimmedTop (24).withTrimmedBottom (18));
        flowArea.removeFromRight (8);
        flow.setBounds (flowArea);

        bottom.removeFromLeft (10);
        resonatorCard = bottom;
        auto resArea = bottom.reduced (8, 0);
        resArea.removeFromTop (26);
        resArea.removeFromBottom (4);
        layoutRow (resArea, { &resOn, &resAmount, &resDecay, &resOffset, &resKeytrack });
    }

private:
    // Balance only acts in parallel; resonator knobs only when it is on.
    void timerCallback() override
    {
        const auto read = [this] (const char* id)
        {
            const auto* value = processorRef.apvts.getRawParameterValue (id);
            return value != nullptr && value->load() > 0.5f;
        };

        const auto fade = [] (juce::Component& component, bool active)
        {
            const auto alpha = active ? 1.0f : 0.4f;

            if (component.getAlpha() != alpha)
                component.setAlpha (alpha);
        };

        const auto parallelNow = read ("filters_parallel");

        if (parallelNow != wasParallel)
        {
            wasParallel = parallelNow;
            repaint (balanceCard);
        }

        fade (balance, parallelNow);

        const auto resonating = read ("res_on");

        for (auto* knob : { &resAmount, &resDecay, &resOffset, &resKeytrack })
            fade (*knob, resonating);
    }

    IlanaSynthAudioProcessor& processorRef;
    FilterDisplay filterDisplay;
    FilterPanel panel1, panel2;
    SignalFlow flow;
    KnobControl balance;
    bool wasParallel = false;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    juce::Rectangle<int> flowTitle, resonatorCard, balanceCard;
};

// Scrolls a sideways card bar so the given card is in view.
inline void scrollToCard (juce::Viewport& view, juce::Rectangle<int> card)
{
    if (card.isEmpty())
        return;

    const auto x = view.getViewPositionX();
    if (card.getX() < x)
        view.setViewPosition (card.getX(), 0);
    else if (card.getRight() > x + view.getViewWidth())
        view.setViewPosition (card.getRight() - view.getViewWidth(), 0);
}

class EnvSection : public juce::Component
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : settings (settingsRef),
          thumbs (p, []
          {
              std::vector<EnvThumbBar::Env> envs { EnvThumbBar::Env { "AMP", "amp", Mod::Source::AmpEnv, IlanaTheme::accent() },
                       EnvThumbBar::Env { "FILTER 1", "fe", Mod::Source::FilterEnv, juce::Colour (0xffff4fd8) },
                       EnvThumbBar::Env { "FILTER 2", "f2e", Mod::Source::FilterEnv2, juce::Colour (0xffb28aff) },
                       EnvThumbBar::Env { "MOD", "me", Mod::Source::ModEnv, juce::Colour (0xff8fff3b) },
                       EnvThumbBar::Env { "ENV 5", "e4", Mod::Source::Env4, juce::Colour (0xffffd447) } };
              for (int env = 6; env <= 16; ++env)
                  envs.push_back ({ "ENV " + juce::String (env), "env" + juce::String (env),
                                    (Mod::Source) ((int) Mod::Source::Env6 + env - 6), extraColour (env) });
              return envs;
          }()),
          ampDisplay (p, "amp", IlanaTheme::accent(), true),
          feDisplay (p, "fe", juce::Colour (0xffff4fd8)),
          f2eDisplay (p, "f2e", juce::Colour (0xffb28aff)),
          meDisplay (p, "me", juce::Colour (0xff8fff3b)),
          e4Display (p, "e4", juce::Colour (0xffffd447)),
          ampA (p.apvts, "amp_attack", "ATTACK"), ampD (p.apvts, "amp_decay", "DECAY"),
          ampS (p.apvts, "amp_sustain", "SUSTAIN"), ampR (p.apvts, "amp_release", "RELEASE"),
          ampVel (p.apvts, "amp_velocity", "VEL"), ampCurve (p.apvts, "amp_curve", "TENSION"),
          feA (p.apvts, "fe_attack", "ATTACK"), feD (p.apvts, "fe_decay", "DECAY"),
          feS (p.apvts, "fe_sustain", "SUSTAIN"), feR (p.apvts, "fe_release", "RELEASE"),
          feVel (p.apvts, "filter_velocity", "VEL"), feCurve (p.apvts, "fe_curve", "TENSION", juce::Colour (0xffff4fd8), false),
          f2A (p.apvts, "f2e_attack", "ATTACK"), f2D (p.apvts, "f2e_decay", "DECAY"),
          f2S (p.apvts, "f2e_sustain", "SUSTAIN"), f2R (p.apvts, "f2e_release", "RELEASE"),
          f2Vel (p.apvts, "f2e_velocity", "VEL", juce::Colour (0xffb28aff), false),
          f2Curve (p.apvts, "f2e_curve", "TENSION", juce::Colour (0xffb28aff), false),
          meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
          meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
          meVel (p.apvts, "me_velocity", "VEL", juce::Colour (0xff8fff3b), false),
          meCurve (p.apvts, "me_curve", "TENSION", juce::Colour (0xff8fff3b), false),
          e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
          e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
          e4Vel (p.apvts, "e4_velocity", "VEL", juce::Colour (0xffffd447), false),
          e4Curve (p.apvts, "e4_curve", "TENSION", juce::Colour (0xffffd447), false)
    {
        // The cards keep one size and scroll sideways once there are more than five.
        thumbView.setViewedComponent (&thumbs, false);
        thumbView.setScrollBarsShown (false, true);
        thumbView.setScrollBarThickness (6);
        addAndMakeVisible (thumbView);

        addAll (*this, ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display,
                ampA, ampD, ampS, ampR, ampVel, ampCurve,
                feA, feD, feS, feR, feVel, feCurve,
                f2A, f2D, f2S, f2R, f2Vel, f2Curve,
                meA, meD, meS, meR, meVel, meCurve,
                e4A, e4D, e4S, e4R, e4Vel, e4Curve);

        units.push_back ({ &ampDisplay, { &ampA, &ampD, &ampS, &ampR, &ampVel, &ampCurve } });
        units.push_back ({ &feDisplay, { &feA, &feD, &feS, &feR, &feVel, &feCurve } });
        units.push_back ({ &f2eDisplay, { &f2A, &f2D, &f2S, &f2R, &f2Vel, &f2Curve } });
        units.push_back ({ &meDisplay, { &meA, &meD, &meS, &meR, &meVel, &meCurve } });
        units.push_back ({ &e4Display, { &e4A, &e4D, &e4S, &e4R, &e4Vel, &e4Curve } });

        // M5 DAHDSR and rate key scaling: a second row on every envelope.
        {
            const char* const prefixes[] { "amp", "fe", "f2e", "me", "e4" };
            const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xffff4fd8), juce::Colour (0xffb28aff),
                                           juce::Colour (0xff8fff3b), juce::Colour (0xffffd447) };

            for (int env = 0; env < 5; ++env)
                addStageTwoKnobs (p, prefixes[env], colours[env], units[(size_t) env], env == 0);
        }

        for (int env = 6; env <= 16; ++env)
        {
            const auto prefix = "env" + juce::String (env);
            const auto colour = extraColour (env);
            ExtraUnit extra;
            extra.display = std::make_unique<EnvelopeDisplay> (p, prefix, colour);
            addChildComponent (*extra.display);
            const char* const suffixes[] { "attack", "decay", "sustain", "release", "velocity", "curve" };
            const char* const labels[] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VEL", "TENSION" };
            for (int control = 0; control < 6; ++control)
            {
                extra.knobs[(size_t) control] = std::make_unique<KnobControl> (
                    p.apvts, prefix + "_" + suffixes[control], labels[control], colour, false);
                addChildComponent (*extra.knobs[(size_t) control]);
            }
            std::vector<juce::Component*> knobs;
            for (auto& knob : extra.knobs)
                knobs.push_back (knob.get());
            units.push_back ({ extra.display.get(), std::move (knobs) });
            addStageTwoKnobs (p, prefix, colour, units.back(), false);
            extraUnits.push_back (std::move (extra));
        }

        selected = juce::jlimit (0, (int) units.size() - 1, settings.getIntValue ("envSelected", 0));

        thumbs.onSelect = [this] (int index)
        {
            selected = index;
            settings.setValue ("envSelected", selected);
            updateVisibility();
        };
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

        updateVisibility();
    }

    void select (int index)
    {
        selected = juce::jlimit (0, (int) units.size() - 1, index);
        settings.setValue ("envSelected", selected);
        updateVisibility();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        thumbs.setViewWidth (area.getWidth());
        const auto thumbWidth = thumbs.getPreferredWidth();
        const auto scrolls = thumbWidth > area.getWidth();
        thumbView.setBounds (area.removeFromTop (48 + (scrolls ? thumbView.getScrollBarThickness() + 2 : 0)));
        thumbs.setSize (thumbWidth, 48);
        area.removeFromTop (8);

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (area.getWidth() * 55 / 100).reduced (2));
        area.removeFromLeft (8);

        // Same panel shape as the LFOs: heading, then the stage knobs.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        // First row: the ADSR, velocity and tension as before; second row:
        // delay, hold and key rate.
        const auto& knobs = units[(size_t) unitIndex].knobs;
        const std::vector<juce::Component*> first (knobs.begin(), knobs.begin() + juce::jmin ((int) knobs.size(), 6));
        const std::vector<juce::Component*> second (knobs.begin() + (int) first.size(), knobs.end());
        const auto rowHeight = juce::jmin (inner.getHeight() / 2, 96);
        auto rows = inner.withSizeKeepingCentre (inner.getWidth(), rowHeight * 2);
        layoutFixed (rows.removeFromTop (rowHeight), first);

        if (! second.empty())
            layoutFixed (rows.withWidth (rows.getWidth() * (int) second.size() / 6), second);
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xffff4fd8), juce::Colour (0xffb28aff),
                                       juce::Colour (0xff8fff3b), juce::Colour (0xffffd447) };
        const juce::StringArray titles { "AMP ENVELOPE", "FILTER 1 ENVELOPE", "FILTER 2 ENVELOPE", "MOD ENVELOPE", "ENVELOPE 5" };
        const auto index = juce::jlimit (0, 4, selected);
        const auto colour = selected < 5 ? colours[index] : extraColour (selected + 1);

        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));
        auto header = panel.reduced (12, 0).withHeight (26);
        g.setColour (colour);
        g.setFont (IlanaTheme::font (12.5f, true));
        g.drawText (selected < 5 ? titles[index] : "ENVELOPE " + juce::String (selected + 1),
                    header, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("drag the graph or the knobs", header, juce::Justification::centredRight);
    }

private:
    // ENV 6-16 spread around the hue wheel.
    static juce::Colour extraColour (int env)
    {
        return juce::Colour::fromHSV ((float) (env - 6) / 11.0f, 0.55f, 0.95f, 1.0f);
    }

    struct ExtraUnit
    {
        std::unique_ptr<EnvelopeDisplay> display;
        std::array<std::unique_ptr<KnobControl>, 6> knobs;
    };
    struct Unit
    {
        juce::Component* display = nullptr;
        std::vector<juce::Component*> knobs;
    };

    void addStageTwoKnobs (IlanaSynthAudioProcessor& p, const juce::String& prefix, juce::Colour colour, Unit& unit,
                           bool followsTheme)
    {
        const char* const suffixes[] { "_delay", "_hold", "_keyrate" };
        const char* const labels[] { "DELAY", "HOLD", "KEY RATE" };

        for (int i = 0; i < 3; ++i)
        {
            auto knob = std::make_unique<KnobControl> (p.apvts, prefix + suffixes[i], labels[i], colour, followsTheme);
            addChildComponent (*knob);
            unit.knobs.push_back (knob.get());
            stageTwoKnobs.push_back (std::move (knob));
        }
    }

    std::vector<std::unique_ptr<KnobControl>> stageTwoKnobs;


    static void layoutFixed (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
    {
        const auto width = juce::jmax (1, area.getWidth() / (int) items.size());

        for (auto* item : items)
        {
            auto cell = area.removeFromLeft (width).reduced (3);

            if (item != nullptr)
                item->setBounds (cell);
        }
    }

    void updateVisibility()
    {
        for (int i = 0; i < (int) units.size(); ++i)
        {
            const auto visible = i == selected;
            units[(size_t) i].display->setVisible (visible);

            for (auto* knob : units[(size_t) i].knobs)
                if (knob != nullptr)
                    knob->setVisible (visible);
        }

        thumbs.setSelected (selected);
        resized();

        scrollToCard (thumbView, thumbs.boundsOfCard (selected));

        repaint();
    }

    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    juce::Viewport thumbView;
    EnvThumbBar thumbs;
    EnvelopeDisplay ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display;
    KnobControl ampA, ampD, ampS, ampR, ampVel, ampCurve;
    KnobControl feA, feD, feS, feR, feVel, feCurve;
    KnobControl f2A, f2D, f2S, f2R, f2Vel, f2Curve;
    KnobControl meA, meD, meS, meR, meVel, meCurve;
    KnobControl e4A, e4D, e4S, e4R, e4Vel, e4Curve;
    std::vector<Unit> units;
    std::vector<ExtraUnit> extraUnits;
    int selected = 0;
};

class LfoSection : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private juce::Timer
{
public:
    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, [] (int index) { return lfoColour (index); })
    {
        // Cards keep one size and scroll sideways past four.
        thumbView.setViewedComponent (&thumbs, false);
        thumbView.setScrollBarsShown (false, true);
        thumbView.setScrollBarThickness (6);
        addAndMakeVisible (thumbView);
        thumbs.onLayoutChanged = [this] { resized(); repaint(); };

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            auto display = std::make_unique<LfoDisplay> (p, lfo, lfoColour (lfo), lfo == 0);
            addAndMakeVisible (*display);
            displays.push_back (std::move (display));

            auto controls = std::make_unique<Controls> (p.apvts, lfo + 1, lfoColour (lfo), lfo == 0);
            addAll (*this, controls->shape, controls->rate, controls->sync, controls->div, controls->retrig, controls->key,
                    controls->phase, controls->physA, controls->physB, controls->kick);
            controlsList.push_back (std::move (controls));
        }

        selected = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, settings.getIntValue ("lfoSelected", 0));

        thumbs.onSelect = [this] (int index)
        {
            selected = index;
            settings.setValue ("lfoSelected", selected);
            updateVisibility();
        };

        updateVisibility();
        for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
            processorRef.apvts.addParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
        startTimerHz (10);
    }

    ~LfoSection() override
    {
        for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
            processorRef.apvts.removeParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
        cancelPendingUpdate();
    }

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { lastShape = -1; timerCallback(); }

    void resized() override
    {
        auto area = getLocalBounds();

        thumbs.setViewWidth (area.getWidth());
        const auto thumbWidth = thumbs.getPreferredWidth();
        const auto scrolls = thumbWidth > area.getWidth();
        thumbView.setBounds (area.removeFromTop (58 + (scrolls ? thumbView.getScrollBarThickness() + 2 : 0)));
        thumbs.setSize (thumbWidth, 58);
        area.removeFromTop (8);

        const auto displayIndex = juce::jlimit (0, (int) displays.size() - 1, selected);
        displays[(size_t) displayIndex]->setBounds (area.removeFromLeft (area.getWidth() * 55 / 100).reduced (2));
        area.removeFromLeft (8);

        // Control panel: options across the top, knobs underneath.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        auto& c = *controlsList[(size_t) displayIndex];

        // Options stacked on the left, the two knobs full height on the right.
        auto options = inner.removeFromLeft (inner.getWidth() / 2);
        const auto rowHeight = options.getHeight() / 3;
        c.shape.setBounds (options.removeFromTop (rowHeight).reduced (3, 1));
        auto toggles = options.removeFromTop (rowHeight);
        const auto toggleWidth = toggles.getWidth() / 3;
        c.sync.setBounds (toggles.removeFromLeft (toggleWidth).reduced (3, 1));
        c.retrig.setBounds (toggles.removeFromLeft (toggleWidth).reduced (3, 1));
        c.key.setBounds (toggles.reduced (3, 1));
        auto lastOptions = options;
        c.div.setBounds (lastOptions.removeFromLeft (lastOptions.getWidth() * 2 / 3).reduced (3, 1));
        c.kick.setBounds (lastOptions.reduced (3, 1));

        inner.removeFromLeft (8);
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (displayIndex + 1) + "_shape")->load();
        if (LfoShapes::isPhysics (shape))
        {
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            c.rate.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (2));
            c.phase.setBounds (top.reduced (2));
            c.physA.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (2));
            c.physB.setBounds (inner.reduced (2));
        }
        else
        {
            c.rate.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (3, 0));
            c.phase.setBounds (inner.reduced (3, 0));
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const auto colour = lfoColour (selected);
        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));

        auto header = panel.reduced (12, 0).withHeight (26);
        g.setColour (colour);
        g.setFont (IlanaTheme::font (12.5f, true));
        g.drawText ("LFO " + juce::String (selected + 1), header, juce::Justification::centredLeft);

        const auto* retrig = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_retrig");
        const auto* key = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_key");
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText (key != nullptr && key->load() > 0.5f         ? "per voice, rate follows the note (4 Hz = its pitch)"
                    : retrig != nullptr && retrig->load() > 0.5f ? "runs per voice, restarts on each note"
                                                                 : "free-running, shared by all voices",
                    header, juce::Justification::centredRight);
    }

    static juce::Colour lfoColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xff35c8ff);
            case 2: return juce::Colour (0xff6fe3c1);
            case 3: return juce::Colour (0xffe3a56f);
            case 0: return IlanaTheme::accent();
            default: return IlanaSynthAudioProcessor::lfoColour (index);
        }
    }

    void select (int index)
    {
        selected = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, index);
        settings.setValue ("lfoSelected", selected);
        updateVisibility();
    }

private:
    struct Controls
    {
        Controls (juce::AudioProcessorValueTreeState& state, int lfo, juce::Colour accent, bool followsTheme)
            : shape (state, "lfo" + juce::String (lfo) + "_shape", "SHAPE"),
              rate (state, "lfo" + juce::String (lfo) + "_rate", "RATE", accent, followsTheme),
              sync (state, "lfo" + juce::String (lfo) + "_sync", "SYNC"),
              div (state, "lfo" + juce::String (lfo) + "_div", "DIVISION"),
              retrig (state, "lfo" + juce::String (lfo) + "_retrig", "RETRIG"),
              key (state, "lfo" + juce::String (lfo) + "_key", "KEY"),
              phase (state, "lfo" + juce::String (lfo) + "_phase", "START", accent, followsTheme)
              , physA (state, "lfo" + juce::String (lfo) + "_phys_a", "HEIGHT", accent, followsTheme)
              , physB (state, "lfo" + juce::String (lfo) + "_phys_b", "BOUNCE", accent, followsTheme)
              , kick (state, "lfo" + juce::String (lfo) + "_kick", "KICK")
        {
        }

        ComboControl shape;
        KnobControl rate;
        ToggleControl sync;
        ComboControl div;
        ToggleControl retrig;
        ToggleControl key;
        KnobControl phase;
        KnobControl physA, physB;
        ToggleControl kick;
    };

    void updateVisibility()
    {
        // A remembered selection can point at an LFO this patch doesn't show.
        if (! processorRef.isLfoShown (selected))
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (processorRef.isLfoShown (lfo))
                {
                    selected = lfo;
                    break;
                }

        for (int lfo = 0; lfo < (int) displays.size(); ++lfo)
        {
            displays[(size_t) lfo]->setVisible (lfo == selected);
            auto& c = *controlsList[(size_t) lfo];
            const auto visible = lfo == selected;
            c.shape.setVisible (visible);
            c.rate.setVisible (visible);
            c.sync.setVisible (visible);
            c.div.setVisible (visible);
            c.retrig.setVisible (visible);
            c.key.setVisible (visible);
            c.phase.setVisible (visible);
            const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape")->load();
            c.physA.setVisible (visible && LfoShapes::isPhysics (shape));
            c.physB.setVisible (visible && LfoShapes::isPhysics (shape));
            c.kick.setVisible (visible && shape == LfoShapes::Pendulum);
        }

        thumbs.setSelected (selected);
        resized();
        scrollToCard (thumbView, thumbs.boundsOfCard (selected));
        repaint();
    }

    // RATE only matters free-running and DIVISION only when synced, so the
    // unused one steps back.
    void timerCallback() override
    {
        auto& c = *controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, selected)];
        const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_sync");
        const auto synced = sync != nullptr && sync->load() > 0.5f;
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_shape")->load();
        if (shape != lastShape)
        {
            lastShape = shape;
            const juce::String labelsA[] { "HEIGHT", "SWING", "STIFF", "DRIVE" };
            const juce::String labelsB[] { "BOUNCE", "DAMP", "DAMP", "STICK" };
            if (LfoShapes::isPhysics (shape))
            {
                c.physA.setLabelText (labelsA[shape - LfoShapes::Bounce]);
                c.physB.setLabelText (labelsB[shape - LfoShapes::Bounce]);
            }
            updateVisibility();
        }
        const auto rateAlpha = synced ? 0.35f : 1.0f;
        const auto divAlpha = synced ? 1.0f : 0.35f;

        if (c.rate.getAlpha() != rateAlpha)
            c.rate.setAlpha (rateAlpha);

        if (c.div.getAlpha() != divAlpha)
            c.div.setAlpha (divAlpha);

        repaint (panel);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    juce::Viewport thumbView;
    LfoThumbBar thumbs;
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    int selected = 0;
    int lastShape = -1;
};

class EnvLfoPage : public juce::Component
{
public:
    EnvLfoPage (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : lfoSection (p, settingsRef),
          envSection (p, settingsRef)
    {
        addAndMakeVisible (lfoSection);
        addAndMakeVisible (envSection);
    }

    void selectLfo (int index) { lfoSection.select (index); }
    void selectEnvelope (int index) { envSection.select (index); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText ("LFO", juce::Rectangle<int> (14, 2, 200, 14), juce::Justification::centredLeft);
        g.drawText ("ENVELOPES", juce::Rectangle<int> (14, lfoBottom + 6, 200, 14),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (12.5f));
        g.drawText ("Assign LFOs and envelopes in the MATRIX tab.  Sync uses host tempo.",
                    juce::Rectangle<int> (14, getHeight() - 20, 700, 16), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (18);
        area.removeFromBottom (18);

        const auto lfoHeight = (area.getHeight() - 22) / 2;
        auto lfoArea = area.removeFromTop (lfoHeight);
        lfoBottom = lfoArea.getBottom();
        area.removeFromTop (22);
        envSection.setBounds (area);
        lfoSection.setBounds (lfoArea);
    }

private:
    LfoSection lfoSection;
    EnvSection envSection;
    int lfoBottom = 0;
};

// FM between six oscillators: the algorithms, the operator diagram and the
// selected operator's settings on the left; the full matrix of amounts on the
// right (rows = from, columns = to, plus the noise operator), with the FM
// style, each oscillator's output switch, ring mod and hard sync.
class FmPage : public juce::Component,
               private juce::Timer
{
    // One operator's M5 settings (tuning, key scaling, feedback style) and
    // the oscillator controls that matter most when it is an operator.
    struct OperatorControls
    {
        OperatorControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, juce::Colour colour)
            : tune (state, prefix + "_tune", "TUNING"),
              snap (state, prefix + "_ratio_snap", "SNAP"),
              feedbackType (state, prefix + "_fb_type", "FB TYPE"),
              ampEnv (state, prefix + "_amp_env", "ENVELOPE"),
              ratio (state, prefix + "_ratio", "RATIO", colour, false),
              fixedHz (state, prefix + "_fixed_hz", "FIXED", colour, false),
              semi (state, prefix + "_semi", "SEMI", colour, false),
              fine (state, prefix + "_fine", "FINE", colour, false),
              level (state, prefix + "_level", "LEVEL", colour, false),
              keyLevel (state, prefix + "_key_level", "KEY LVL", colour, false) {}

        ComboControl tune, snap, feedbackType, ampEnv;
        KnobControl ratio, fixedHz, semi, fine, level, keyLevel;

        std::vector<juce::Component*> all()
        {
            return { &tune, &snap, &feedbackType, &ampEnv, &ratio, &fixedHz, &semi, &fine, &level, &keyLevel };
        }
    };

public:
    explicit FmPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          diagram (p),
          algorithms (p),
          mode (p.apvts, "fm_mode", "FM MODE"),
          hardSync (p.apvts, "hard_sync", "HARD SYNC 1>2")
    {
        addAll (*this, diagram, algorithms, mode, hardSync);
        ringMod = std::make_unique<StripKnob> (p, "ring_mod", "Ring Mod", -1, fmColour(), false);
        addAndMakeVisible (*ringMod);

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            for (int target = 0; target < OscillatorIds::count; ++target)
            {
                auto knob = std::make_unique<KnobControl> (p.apvts, FmDiagram::routeId (source, target), "",
                                                           FmDiagram::oscColour (source), false);
                addAndMakeVisible (*knob);
                knobs[(size_t) source][(size_t) target] = std::move (knob);
            }

            outs[(size_t) source] = std::make_unique<ToggleControl> (
                p.apvts, juce::String (OscillatorIds::prefixes[(size_t) source]) + "_out", "OUT");
            addAndMakeVisible (*outs[(size_t) source]);

            noiseKnobs[(size_t) source] = std::make_unique<KnobControl> (p.apvts, "fm_noise" + juce::String (source + 1), "",
                                                                          noiseColour(), false);
            addAndMakeVisible (*noiseKnobs[(size_t) source]);

            const juce::String prefix (OscillatorIds::prefixes[(size_t) source]);
            operators[(size_t) source] = std::make_unique<OperatorControls> (p.apvts, prefix, FmDiagram::oscColour (source));
            for (auto* control : operators[(size_t) source]->all())
                addChildComponent (control);

            auto button = std::make_unique<juce::TextButton> ("OP " + juce::String (source + 1));
            button->setClickingTogglesState (false);
            button->setColour (juce::TextButton::buttonOnColourId, FmDiagram::oscColour (source).withAlpha (0.55f));
            button->onClick = [this, source] { selectOperator (source); };
            addAndMakeVisible (*button);
            operatorButtons[(size_t) source] = std::move (button);
        }

        noiseColourKnob = std::make_unique<StripKnob> (p, "fm_noise_color", "Noise Colour", -1, noiseColour(), false);
        addAndMakeVisible (*noiseColourKnob);

        for (const auto* prefix : OscillatorIds::prefixes)
            tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + "_tune"));

        refreshShown();
        refreshFmInputs (true);
        selectOperator (0);
        startTimerHz (12);
    }

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }
    static juce::Colour noiseColour() { return juce::Colour (0xffc8c8d0); }

    int getSelectedOperator() const { return selectedOperator; }

    void selectOperator (int op)
    {
        selectedOperator = juce::jlimit (0, OscillatorIds::count - 1, op);

        for (int i = 0; i < OscillatorIds::count; ++i)
            operatorButtons[(size_t) i]->setToggleState (i == selectedOperator, juce::dontSendNotification);

        updateOperatorVisibility();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        paintSectionTitle (g, "ALGORITHMS", { 14, 10, 300, 16 });
        paintSectionTitle (g, "OPERATORS", operatorsTitle);
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        // The selected operator's settings.
        {
            const auto colour = FmDiagram::oscColour (selectedOperator);
            IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
            g.setColour (colour);
            g.setFont (IlanaTheme::font (12.5f, true));
            g.drawText ("OSC " + juce::String (selectedOperator + 1) + " AS AN OPERATOR",
                        operatorCard.reduced (12, 0).withHeight (26), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (IlanaTheme::font (11.5f));
            g.drawText (soundingText(), operatorCard.reduced (12, 0).withHeight (26), juce::Justification::centredRight);
        }

        g.setColour (fmColour());
        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText ("FM MATRIX", matrixCard.reduced (12, 0).withHeight (26), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("rows modulate columns", matrixCard.reduced (12, 0).withHeight (26), juce::Justification::centredRight);

        // Matrix cells: tinted by the source, brighter the deeper the route.
        const auto paintCell = [&g] (juce::Rectangle<float> cell, float amount, juce::Colour colour)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (0.04f + 0.22f * amount));
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (colour.withAlpha (amount > 0.001f ? 0.55f : 0.12f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
        };

        for (const auto source : shown)
        {
            for (const auto target : shown)
            {
                const auto cell = cells[(size_t) source][(size_t) target].toFloat();
                const auto colour = FmDiagram::oscColour (source);

                if (! fmIn[(size_t) target])
                {
                    // This oscillator ignores FM: a flat, empty cell.
                    g.setColour (juce::Colours::black.withAlpha (0.3f));
                    g.fillRoundedRectangle (cell, 6.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.05f));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
                    continue;
                }

                paintCell (cell, read (FmDiagram::routeId (source, target)), colour);

                if (source == target)
                {
                    g.setColour (colour.withAlpha (0.6f));
                    g.setFont (IlanaTheme::font (9.5f, true));
                    const auto type = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_fb_type"));
                    g.drawText (type == FmFeedback::Filtered ? "FB~" : type == FmFeedback::Cross ? "FB<>" : "FB",
                                cell.reduced (6.0f, 4.0f).toNearestInt(), juce::Justification::topLeft);
                }
            }

            if (fmIn[(size_t) source])
                paintCell (noiseCells[(size_t) source].toFloat(), read ("fm_noise" + juce::String (source + 1)), noiseColour());
        }

        // Column and row headings.
        g.setFont (IlanaTheme::font (11.0f, true));

        for (const auto i : shown)
        {
            g.setColour (FmDiagram::oscColour (i).withAlpha (fmIn[(size_t) i] ? 1.0f : 0.4f));
            g.drawText (fmIn[(size_t) i] ? "TO OSC " + juce::String (i + 1) : "OSC " + juce::String (i + 1) + ": NO FM IN",
                        columnHeads[(size_t) i], juce::Justification::centred);
            g.setColour (FmDiagram::oscColour (i));
            g.drawText ("OSC " + juce::String (i + 1), rowHeads[(size_t) i].withHeight (18), juce::Justification::centredLeft);
        }

        g.setColour (noiseColour());
        g.drawText ("NOISE", noiseHead.withHeight (18), juce::Justification::centredLeft);
    }

    void visibilityChanged() override
    {
        if (isVisible())
            algorithms.refreshMatch();
    }

    // The matrix follows the oscillators added to the patch.
    void timerCallback() override
    {
        const auto tuneChanged = [this]
        {
            auto changed = false;
            for (size_t i = 0; i < tuneValues.size(); ++i)
            {
                const auto value = tuneValues[i] != nullptr ? juce::roundToInt (tuneValues[i]->load()) : 0;
                changed = changed || value != lastTune[i];
                lastTune[i] = value;
            }
            return changed;
        }();

        if (isShowing())
            algorithms.refreshMatch();

        if (refreshFmInputs (false))
            repaint (matrixCard);

        if (refreshShown() || tuneChanged)
        {
            updateOperatorVisibility();
            resized();
            repaint();
        }
        else if (isShowing())
        {
            repaint (matrixCard);
            repaint (operatorCard.withHeight (26));
        }
    }

    // Columns of oscillators that ignore FM (sample, granular, a string
    // not set to Osc In) are greyed out and can't be edited; routes already
    // there stay in the patch but do nothing.
    bool refreshFmInputs (bool force)
    {
        std::array<bool, OscillatorIds::count> now {};

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            now[(size_t) osc] = FmDiagram::receivesFm (processorRef, osc);

        if (now == fmIn && ! force)
            return false;

        fmIn = now;

        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            const auto on = fmIn[(size_t) target];
            const auto note = FmDiagram::fmInputNote (processorRef, target);

            std::vector<juce::Component*> column { noiseKnobs[(size_t) target].get() };
            for (int source = 0; source < OscillatorIds::count; ++source)
                column.push_back (knobs[(size_t) source][(size_t) target].get());

            for (auto* control : column)
            {
                control->setEnabled (on);
                control->setAlpha (on ? 1.0f : 0.22f);

                if (auto* tooltipClient = dynamic_cast<juce::SettableTooltipClient*> (control))
                {
                    if (! on)
                        tooltipClient->setTooltip (note);
                    else if (auto* knob = dynamic_cast<KnobControl*> (control))
                        tooltipClient->setTooltip (knob->getSlider().getTooltip());
                }
            }
        }

        return true;
    }

    bool refreshShown()
    {
        std::vector<int> nowShown;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                nowShown.push_back (osc);

        if (nowShown == shown)
            return false;

        shown = nowShown;

        if (std::find (shown.begin(), shown.end(), selectedOperator) == shown.end() && ! shown.empty())
            selectedOperator = shown.front();

        return true;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (18);

        matrixCard = area.removeFromRight (area.getWidth() * 48 / 100);
        area.removeFromRight (10);

        // Left column: algorithms, the diagram, the selected operator.
        algorithms.setBounds (area.removeFromTop (area.getWidth() >= 16 * 38 ? 48 : 80));
        area.removeFromTop (6);
        operatorsTitle = area.removeFromTop (18).withX (14).withWidth (300);
        operatorCard = area.removeFromBottom (juce::jmin (176, area.getHeight() / 2));
        area.removeFromBottom (8);
        diagram.setBounds (area);

        layoutOperatorCard();
        layoutMatrix();
    }

private:
    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    // "sounds at x1.414 of the note" and the like, after SNAP.
    juce::String soundingText() const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) selectedOperator]);
        const auto tune = juce::roundToInt (read (prefix + "_tune"));

        if (tune == OscTuning::Ratio)
            return "sounds at x" + juce::String (processorRef.getSnappedRatio (selectedOperator), 3) + " the note";

        if (tune == OscTuning::Fixed)
            return "fixed at " + describeValue (prefix + "_fixed_hz", read (prefix + "_fixed_hz"));

        return "tuned in semitones";
    }

    void updateOperatorVisibility()
    {
        for (int op = 0; op < OscillatorIds::count; ++op)
        {
            const auto isShown = std::find (shown.begin(), shown.end(), op) != shown.end();
            operatorButtons[(size_t) op]->setVisible (isShown);

            auto& controls = *operators[(size_t) op];
            const auto selected = op == selectedOperator;
            const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) op]) + "_tune"));

            for (auto* control : controls.all())
                control->setVisible (selected);

            controls.ratio.setVisible (selected && tune == OscTuning::Ratio);
            controls.snap.setVisible (selected && tune == OscTuning::Ratio);
            controls.fixedHz.setVisible (selected && tune == OscTuning::Fixed);
        }
    }

    void layoutOperatorCard()
    {
        auto inner = operatorCard.reduced (10, 0);
        inner.removeFromTop (26);

        auto tabs = inner.removeFromTop (22);
        const auto tabWidth = juce::jmin (64, tabs.getWidth() / OscillatorIds::count);

        for (int op = 0; op < OscillatorIds::count; ++op)
            if (std::find (shown.begin(), shown.end(), op) != shown.end())
                operatorButtons[(size_t) op]->setBounds (tabs.removeFromLeft (tabWidth).reduced (2, 0));

        inner.removeFromTop (6);
        auto& controls = *operators[(size_t) selectedOperator];
        const auto tune = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) selectedOperator]) + "_tune"));

        auto topRow = inner.removeFromTop (46);
        std::vector<juce::Component*> top { &controls.tune };
        if (tune == OscTuning::Ratio)
            top.push_back (&controls.snap);
        top.push_back (&controls.feedbackType);
        top.push_back (&controls.ampEnv);

        const auto topWidth = topRow.getWidth() / (int) top.size();
        for (auto* item : top)
            item->setBounds (topRow.removeFromLeft (topWidth).reduced (3, 1));

        inner.removeFromTop (4);
        std::vector<juce::Component*> bottom;
        if (tune == OscTuning::Ratio)
            bottom.push_back (&controls.ratio);
        if (tune == OscTuning::Fixed)
            bottom.push_back (&controls.fixedHz);
        for (auto* item : { &controls.semi, &controls.fine, &controls.level, &controls.keyLevel })
            bottom.push_back (item);

        const auto knobWidth = juce::jmin (90, inner.getWidth() / (int) bottom.size());
        for (auto* item : bottom)
            item->setBounds (inner.removeFromLeft (knobWidth).reduced (3, 0));
    }

    void layoutMatrix()
    {
        auto inner = matrixCard.reduced (10, 0);
        inner.removeFromTop (26);
        inner.removeFromBottom (8);

        // Mode, ring mod, the noise colour and sync across the top.
        auto top = inner.removeFromTop (48);
        const auto topWidth = top.getWidth();
        mode.setBounds (top.removeFromLeft (topWidth * 21 / 100).reduced (3, 1));
        ringMod->setBounds (top.removeFromLeft (topWidth * 25 / 100).reduced (3, 1));
        noiseColourKnob->setBounds (top.removeFromLeft (topWidth * 30 / 100).reduced (3, 1));
        hardSync.setBounds (top.reduced (3, 1));
        inner.removeFromTop (6);

        for (int source = 0; source < OscillatorIds::count; ++source)
        {
            const auto sourceShown = std::find (shown.begin(), shown.end(), source) != shown.end();
            outs[(size_t) source]->setVisible (sourceShown);
            noiseKnobs[(size_t) source]->setVisible (sourceShown);

            for (int target = 0; target < OscillatorIds::count; ++target)
                knobs[(size_t) source][(size_t) target]->setVisible (
                    sourceShown && std::find (shown.begin(), shown.end(), target) != shown.end());
        }

        const auto count = juce::jmax (1, (int) shown.size());
        auto heads = inner.removeFromTop (18);
        heads.removeFromLeft (70);
        const auto columnWidth = heads.getWidth() / count;

        for (const auto i : shown)
            columnHeads[(size_t) i] = heads.removeFromLeft (columnWidth);

        inner.removeFromTop (4);

        const auto rowHeight = inner.getHeight() / (count + 1);

        const auto layoutRow = [&] (juce::Rectangle<int> row, auto&& knobFor, auto&& storeCell)
        {
            for (const auto target : shown)
            {
                auto cell = row.removeFromLeft (columnWidth).reduced (4, 0);
                storeCell (target, cell);
                const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 8, 110);
                knobFor (target).setBounds (cell.withSizeKeepingCentre (knobSize, knobSize + 4));
            }
        };

        for (const auto source : shown)
        {
            auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
            auto head = row.removeFromLeft (70);
            rowHeads[(size_t) source] = head.withTrimmedTop (head.getHeight() / 2 - 26);
            outs[(size_t) source]->setBounds (rowHeads[(size_t) source].withTrimmedTop (20).withHeight (40).reduced (0, 2));

            layoutRow (row,
                       [this, source] (int target) -> juce::Component& { return *knobs[(size_t) source][(size_t) target]; },
                       [this, source] (int target, juce::Rectangle<int> cell) { cells[(size_t) source][(size_t) target] = cell; });
        }

        auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
        auto head = row.removeFromLeft (70);
        noiseHead = head.withTrimmedTop (head.getHeight() / 2 - 26);
        layoutRow (row,

                   [this] (int target) -> juce::Component& { return *noiseKnobs[(size_t) target]; },
                   [this] (int target, juce::Rectangle<int> cell) { noiseCells[(size_t) target] = cell; });
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    ComboControl mode;
    ToggleControl hardSync;
    std::unique_ptr<StripKnob> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    std::unique_ptr<StripKnob> noiseColourKnob;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> operatorButtons;
    std::array<juce::Rectangle<int>, OscillatorIds::count> columnHeads, rowHeads, noiseCells;
    std::array<std::array<juce::Rectangle<int>, OscillatorIds::count>, OscillatorIds::count> cells;
    juce::Rectangle<int> matrixCard, operatorCard, operatorsTitle, noiseHead;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0;
};

class SeqPage : public juce::Component,
                private juce::Timer
{
public:
    explicit SeqPage (IlanaSynthAudioProcessor& p)
        : step1 (p, 0, IlanaTheme::accent(), true),
          step2 (p, 1, juce::Colour (0xff35c8ff)),
          mseg (p),
          msegLoop (p.apvts, "mseg_loop", "LOOP"),
          msegRate (p.apvts, "mseg_rate", "RATE", msegColour(), false),
          clockDiv (p.apvts, "clock_div", "S&H CLOCK", msegColour(), false),
          processorRef (p),
          arpDisplay (p, arpColour()),
          arpOn (p.apvts, "arp_on", "ARP"),
          arpMode (p.apvts, "arp_mode", "MODE"),
          arpDiv (p.apvts, "arp_div", "RATE"),
          arpOctaves (p.apvts, "arp_octaves", "OCTAVES", arpColour(), false),
          arpGate (p.apvts, "arp_gate", "GATE", arpColour(), false),
          arpChance (p.apvts, "arp_chance", "CHANCE", arpColour(), false),
          genScale (p.apvts, "gen_scale", "SCALE"),
          genRoot (p.apvts, "gen_root", "ROOT"),
          genSnap (p.apvts, "gen_snap", "SNAP PLAYED"),
          sprayOn (p.apvts, "spray_on", "SPRAY"),
          sprayDirection (p.apvts, "spray_direction", "DIRECTION")
    {
        sprayCount = std::make_unique<KnobControl> (p.apvts, "spray_count", "NOTES", generateColour(), false);
        sprayRange = std::make_unique<KnobControl> (p.apvts, "spray_range", "RANGE", generateColour(), false);
        spraySpread = std::make_unique<KnobControl> (p.apvts, "spray_spread", "STRUM", generateColour(), false);
        sprayChance = std::make_unique<KnobControl> (p.apvts, "spray_chance", "CHANCE", generateColour(), false);
        sprayVelocity = std::make_unique<KnobControl> (p.apvts, "spray_velocity", "VEL RND", generateColour(), false);
        addAll (*this, arpChance, genScale, genRoot, genSnap, sprayOn, sprayDirection,
                *sprayCount, *sprayRange, *spraySpread, *sprayChance, *sprayVelocity);

        addAndMakeVisible (step1);
        addAndMakeVisible (step2);

        // Each step row can edit any of the four LFOs.
        for (int row = 0; row < 2; ++row)
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            {
                auto& button = lfoButtons[(size_t) row][(size_t) lfo];
                button.setButtonText ("LFO " + juce::String (lfo + 1));
                button.setClickingTogglesState (true);
                button.setRadioGroupId (100 + row);
                button.setTooltip ("Edit the steps of LFO " + juce::String (lfo + 1) + " in this row");
                button.onClick = [this, row, lfo]
                {
                    if (lfoButtons[(size_t) row][(size_t) lfo].getToggleState())
                        showLfo (row, lfo);
                };
                addAndMakeVisible (button);
            }
        }

        showLfo (0, 0);
        showLfo (1, 1);

        addAll (*this, mseg, msegLoop, msegRate, clockDiv,
                arpDisplay, arpOn, arpMode, arpDiv, arpOctaves, arpGate);
        startTimerHz (8);
    }

    static juce::Colour msegColour() { return juce::Colour (0xff6fe3c1); }
    static juce::Colour arpColour() { return juce::Colour (0xffff7ac6); }
    static juce::Colour generateColour() { return juce::Colour (0xffffd447); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto title = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour)
        {
            g.setColour (colour);
            g.setFont (IlanaTheme::font (13.0f, true));
            g.fillEllipse ((float) area.getX(), (float) area.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.drawText (text, area.withTrimmedLeft (14), juce::Justification::centredLeft);
        };

        title (stepTitle1, "STEPS", IlanaTheme::accent());
        title (stepTitle2, "STEPS", juce::Colour (0xff35c8ff));

        IlanaTheme::paintCard (g, msegCard.toFloat(), 7.0f, msegColour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, arpCard.toFloat(), 7.0f, arpColour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, generateCard.toFloat(), 7.0f, generateColour().withAlpha (0.35f));
        title (msegCard.reduced (12, 0).removeFromTop (26), "MSEG", msegColour());
        title (arpCard.reduced (12, 0).removeFromTop (26), "ARPEGGIATOR", arpColour());
        title (generateCard.reduced (12, 0).removeFromTop (26), "GENERATE", generateColour());

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText ("drag points; assign it in the MATRIX", msegCard.reduced (12, 0).removeFromTop (26),
                    juce::Justification::centredRight);
        g.drawText ("hold notes to play the pattern", arpCard.reduced (12, 0).removeFromTop (26),
                    juce::Justification::centredRight);
        // "NOTE SPRAY" divider: the label, then a hairline to the card edge.
        if (! sprayDivider.isEmpty())
        {
            const auto font = IlanaTheme::font (10.5f, true);
            const juce::String text ("NOTE SPRAY");
            const auto width = juce::GlyphArrangement::getStringWidthInt (font, text);
            g.setColour (generateColour().withAlpha (0.9f));
            g.setFont (font);
            g.drawText (text, sprayDivider.withTrimmedLeft (3), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (sprayDivider.getX() + width + 12, sprayDivider.getCentreY(), sprayDivider.getWidth() - width - 15, 1);
        }

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("scale snap and note spray", generateCard.reduced (12, 0).removeFromTop (26),
                    juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        const auto layoutPicker = [this] (int rowIndex, juce::Rectangle<int> header)
        {
            header.removeFromLeft (80);
            auto& buttons = lfoButtons[(size_t) rowIndex];
            auto count = 0;

            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                count += buttons[(size_t) lfo].isVisible() ? 1 : 0;

            const auto width = juce::jmin (60, header.getWidth() / juce::jmax (1, count));

            for (auto& button : buttons)
                if (button.isVisible())
                    button.setBounds (header.removeFromLeft (width).reduced (2, 0));
        };

        // Left: the two step rows and the MSEG. Right: arp and generate.
        auto right = area.removeFromRight (area.getWidth() * 43 / 100);
        area.removeFromRight (10);

        const auto msegHeight = juce::jlimit (170, 260, area.getHeight() * 2 / 5);
        const auto stepHeight = (area.getHeight() - msegHeight - 16) / 2;

        auto row = area.removeFromTop (stepHeight);
        stepTitle1 = row.removeFromTop (22).withWidth (80);
        layoutPicker (0, row.withHeight (22).translated (0, -22).withTrimmedBottom (2));
        step1.setBounds (row);

        area.removeFromTop (8);
        row = area.removeFromTop (stepHeight);
        stepTitle2 = row.removeFromTop (22).withWidth (80);
        layoutPicker (1, row.withHeight (22).translated (0, -22).withTrimmedBottom (2));
        step2.setBounds (row);

        area.removeFromTop (8);
        msegCard = area;

        auto msegArea = msegCard.reduced (10, 0);
        msegArea.removeFromTop (26);
        msegArea.removeFromBottom (8);
        auto msegControls = msegArea.removeFromRight (juce::jmin (180, msegArea.getWidth() / 3));
        mseg.setBounds (msegArea.reduced (0, 2));
        msegLoop.setBounds (msegControls.removeFromTop (40).reduced (8, 4));
        layoutRow (msegControls, { &msegRate, &clockDiv });

        // Arp card.
        arpCard = right.removeFromTop ((right.getHeight() - 8) * 47 / 100);
        right.removeFromTop (8);
        generateCard = right;

        auto arpArea = arpCard.reduced (10, 0);
        arpArea.removeFromTop (26);
        arpArea.removeFromBottom (6);
        arpDisplay.setBounds (arpArea.removeFromTop (juce::jmax (36, arpArea.getHeight() - 104)).reduced (0, 2));
        layoutRow (arpArea, { &arpOn, &arpMode, &arpDiv, &arpOctaves, &arpGate, &arpChance });

        // Generate card: scale row, then note spray (switch and direction,
        // with its amounts as one row of knobs).
        auto generate = generateCard.reduced (10, 0);
        generate.removeFromTop (26);
        generate.removeFromBottom (6);

        auto scaleRow = generate.removeFromTop (46);
        genScale.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() * 42 / 100).reduced (3, 1));
        genRoot.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() * 40 / 100).reduced (3, 1));
        genSnap.setBounds (scaleRow.reduced (3, 1));

        sprayDivider = generate.removeFromTop (22);

        auto sprayRow = generate.removeFromTop (46);
        sprayOn.setBounds (sprayRow.removeFromLeft (sprayRow.getWidth() * 42 / 100).reduced (3, 1));
        sprayDirection.setBounds (sprayRow.removeFromLeft (sprayRow.getWidth() * 40 / 100).reduced (3, 1));
        generate.removeFromTop (4);
        layoutRow (generate, { sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get() });
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            return;

        // Bring the LFOs that are actually set to Steps into the two rows.
        std::vector<int> stepLfos;

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape"))
                if ((int) shape->load() == 7)
                    stepLfos.push_back (lfo);

        const auto shown = [this] (int lfo) { return step1.getLfoIndex() == lfo || step2.getLfoIndex() == lfo; };

        for (const auto lfo : stepLfos)
        {
            if (shown (lfo))
                continue;

            // Replace a row that is not showing a Steps LFO, preferring the second.
            const auto rowIsSteps = [&stepLfos] (int index)
            {
                return std::find (stepLfos.begin(), stepLfos.end(), index) != stepLfos.end();
            };

            if (! rowIsSteps (step2.getLfoIndex()))
                showLfo (1, lfo);
            else if (! rowIsSteps (step1.getLfoIndex()))
                showLfo (0, lfo);
        }
    }

private:
    void showLfo (int row, int lfo)
    {
        auto& editor = row == 0 ? step1 : step2;
        auto& other = row == 0 ? step2 : step1;

        // Both rows showing the same LFO would just duplicate it; swap instead.
        if (other.getLfoIndex() == lfo && editor.getLfoIndex() != lfo)
        {
            const auto previous = editor.getLfoIndex();
            other.setLfoIndex (previous);
            lfoButtons[row == 0 ? 1 : 0][(size_t) previous].setToggleState (true, juce::dontSendNotification);
        }

        editor.setLfoIndex (lfo);
        lfoButtons[(size_t) row][(size_t) lfo].setToggleState (true, juce::dontSendNotification);
    }

    // Arp controls step back while the arp is off.
    void timerCallback() override
    {
        // The step-row pickers list the patch's LFOs (and whatever a row shows).
        auto pickersChanged = false;
        for (int row = 0; row < 2; ++row)
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            {
                auto& button = lfoButtons[(size_t) row][(size_t) lfo];
                const auto shown = processorRef.isLfoShown (lfo) || button.getToggleState();
                if (button.isVisible() != shown)
                {
                    button.setVisible (shown);
                    pickersChanged = true;
                }
            }
        if (pickersChanged)
            resized();

        const auto* on = processorRef.apvts.getRawParameterValue ("arp_on");
        const auto alpha = on != nullptr && on->load() > 0.5f ? 1.0f : 0.45f;

        for (juce::Component* control : { static_cast<juce::Component*> (&arpMode), static_cast<juce::Component*> (&arpDiv),
                                          static_cast<juce::Component*> (&arpOctaves), static_cast<juce::Component*> (&arpGate),
                                          static_cast<juce::Component*> (&arpChance) })
            if (control->getAlpha() != alpha)
                control->setAlpha (alpha);

        const auto* spray = processorRef.apvts.getRawParameterValue ("spray_on");
        const auto sprayAlpha = spray != nullptr && spray->load() > 0.5f ? 1.0f : 0.45f;

        for (juce::Component* control : { static_cast<juce::Component*> (&sprayDirection), static_cast<juce::Component*> (sprayCount.get()),
                                          static_cast<juce::Component*> (sprayRange.get()), static_cast<juce::Component*> (spraySpread.get()),
                                          static_cast<juce::Component*> (sprayChance.get()), static_cast<juce::Component*> (sprayVelocity.get()) })
            if (control->getAlpha() != sprayAlpha)
                control->setAlpha (sprayAlpha);
    }

    StepEditor step1, step2;
    MsegEditor mseg;
    ToggleControl msegLoop;
    KnobControl msegRate;
    KnobControl clockDiv;
    IlanaSynthAudioProcessor& processorRef;
    ArpDisplay arpDisplay;
    ToggleControl arpOn;
    ComboControl arpMode, arpDiv;
    KnobControl arpOctaves, arpGate, arpChance;
    ComboControl genScale, genRoot;
    ToggleControl genSnap, sprayOn;
    ComboControl sprayDirection;
    std::unique_ptr<KnobControl> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity;
    juce::Rectangle<int> sprayDivider;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, msegCard, arpCard, generateCard;
};

// The overview: everything needed to shape a basic sound on one screen
// (oscillators, filter 1, amp envelope, LFOs). The other tabs hold the
// detail.
class MainPage : public juce::Component,
                 private juce::Timer,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater
{
public:
    explicit MainPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          lfoThumbs (p, [] (int index) { return lfoColour (index); }),
          filterTabs ({ "F1", "F2" }, { filterColour (0), filterColour (1) }, true),
          envTabs ({ "AMP", "FLT 1", "FLT 2", "MOD", "ENV 5" },
                   { envColour (0), envColour (1), envColour (2), envColour (3), envColour (4) }, true),
          lfoTabs ({}, {}, true)
    {
        const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xff5b8cff), juce::Colour (0xffffd447),
                                       juce::Colour (0xff6fe3c1), juce::Colour (0xffff7f9e), juce::Colour (0xffb28aff) };

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            waves[(size_t) osc] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison", prefix + "_spread",
                prefix + "_detune", false, juce::String {}, prefix + "_mode", osc, colours[osc], osc == 0);
            oscColumn.addAndMakeVisible (*waves[(size_t) osc]);
            auto strip = std::make_unique<OscStrip>();
            const auto colour = colours[osc];
            const auto themed = osc == 0;

            strip->on = std::make_unique<ToggleControl> (p.apvts, prefix + "_on", "ON");
            strip->mode = std::make_unique<ComboControl> (p.apvts, prefix + "_mode", "MODE");
            strip->excite = std::make_unique<ComboControl> (p.apvts, prefix + "_excite", "EXCITE");
            strip->table = std::make_unique<ComboControl> (p.apvts, prefix + "_table", "TABLE");
            strip->table->setPopupOverride ([this, table = strip->table.get(), id = prefix + "_table", colour]
            {
                TableBrowser::show (processorRef, id, colour, table->getComboBox());
            });
            strip->warp = std::make_unique<ComboControl> (p.apvts, prefix + "_warp", "WARP");
            // One row of knobs per oscillator mode: wavetable, string, sample, granular.
            const auto knob = [&] (const juce::String& suffix, const juce::String& label)
            {
                const auto id = prefix + suffix;

                for (auto& existing : strip->allKnobs)
                    if (existing.first == id + label)
                        return existing.second.get();

                strip->allKnobs.push_back ({ id + label, std::make_unique<KnobControl> (p.apvts, id, label, colour, themed) });
                oscColumn.addChildComponent (*strip->allKnobs.back().second);
                return strip->allKnobs.back().second.get();
            };

            strip->modeKnobs[0] = { knob ("_frame", "FRAME"), knob ("_warp_amt", "WARP"), knob ("_level", "LEVEL"),
                                    knob ("_semi", "SEMI"), knob ("_unison", "UNISON"), knob ("_detune", "DETUNE") };
            strip->modeKnobs[1] = { knob ("_string_decay", "DECAY"), knob ("_string_damp", "DAMP"),
                                    knob ("_string_sustain", "SUSTAIN"), knob ("_level", "LEVEL"), knob ("_semi", "SEMI"),
                                    knob ("_unison", "UNISON") };
            strip->modeKnobs[2] = { knob ("_sample_start", "START"), knob ("_sample_end", "END"),
                                    knob ("_sample_fade_out", "FADE OUT"), knob ("_level", "LEVEL"), knob ("_semi", "SEMI"),
                                    knob ("_unison", "UNISON") };
            strip->modeKnobs[3] = { knob ("_sample_start", "POSITION"), knob ("_grain_size", "SIZE"),
                                    knob ("_grain_density", "DENSITY"), knob ("_grain_spray", "SPRAY"), knob ("_level", "LEVEL"),
                                    knob ("_semi", "SEMI") };

            strip->remove = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
            strip->remove->setTooltip ("Remove this oscillator (switches it off and hides it)");
            strip->remove->onClick = [this, osc]
            {
                processorRef.removeOscillator (osc);
                updateStrips();
            };

            addAll (oscColumn, *strip->on, *strip->mode, *strip->table, *strip->warp, *strip->remove);
            oscColumn.addChildComponent (*strip->excite);

            strips.push_back (std::move (strip));
        }

        addOscButton.setButtonText ("+  ADD OSCILLATOR");
        addOscButton.setTooltip ("Add the next oscillator, switched on");
        addOscButton.onClick = [this]
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (! processorRef.isOscillatorShown (osc))
                {
                    processorRef.addOscillator (osc);
                    break;
                }

            updateStrips();
        };
        oscColumn.addChildComponent (addOscButton);
        oscColumn.onPaint = [this] (juce::Graphics& g)
        {
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc])
                    paintCard (g, oscCards[(size_t) osc], "OSC " + juce::String (osc + 1), OscPage::oscColour (osc));
        };
        oscView.setViewedComponent (&oscColumn, false);
        oscView.setScrollBarsShown (true, false);
        oscView.setScrollBarThickness (6);
        addAndMakeVisible (oscView);

        // Filters: one set of controls per filter, swapped by the F1/F2 tabs.
        addAndMakeVisible (filterDisplay);

        for (int f = 0; f < 2; ++f)
        {
            const juce::String prefix (f == 0 ? "f1" : "f2");
            auto set = std::make_unique<ControlSet>();
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_type", "TYPE"));
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_slope", "SLOPE"));

            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" },
                                      { "_drive", "DRIVE" }, { "_env", "ENV AMT" }, { "_keytrack", "KEY TRK" } })
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, filterColour (f), false));

            for (auto& item : set->items)
                addChildComponent (*item);

            filterSets.push_back (std::move (set));
        }

        // Envelopes: graph plus ADSR per envelope, swapped by the tabs.
        const char* const envPrefixes[] { "amp", "fe", "f2e", "me", "e4" };

        for (int e = 0; e < 5; ++e)
        {
            const juce::String prefix (envPrefixes[e]);
            auto set = std::make_unique<ControlSet>();
            set->display = std::make_unique<EnvelopeDisplay> (p, prefix, envColour (e), e == 0);

            for (const auto& spec : { std::pair<const char*, const char*> { "_attack", "ATTACK" }, { "_decay", "DECAY" },
                                      { "_sustain", "SUSTAIN" }, { "_release", "RELEASE" } })
                set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + spec.first, spec.second, envColour (e), e == 0));

            addChildComponent (*set->display);

            for (auto& item : set->items)
                addChildComponent (*item);

            envSets.push_back (std::move (set));
        }

        // LFOs: the cards plus the selected LFO's main controls.
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto set = std::make_unique<ControlSet>();
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_shape", "SHAPE"));
            set->items.push_back (std::make_unique<StripKnob> (p, prefix + "_rate", "Rate", -1, lfoColour (lfo), lfo == 0));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_sync", "SYNC"));
            set->items.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_div", "DIV"));
            set->items.push_back (std::make_unique<ToggleControl> (p.apvts, prefix + "_retrig", "RETRIG"));

            for (auto& item : set->items)
                addChildComponent (*item);

            lfoSets.push_back (std::move (set));
        }

        lfoThumbs.onSelect = [this] (int index) { lfoTabs.setSelected (index, true); };
        lfoThumbView.setViewedComponent (&lfoThumbs, false);
        lfoThumbView.setScrollBarsShown (false, true);
        lfoThumbView.setScrollBarThickness (6);
        addAndMakeVisible (lfoThumbView);
        lfoThumbs.onLayoutChanged = [this] { resized(); };

        filterTabs.onSelect = [this] (int) { updateVisibility(); };
        envTabs.onSelect = [this] (int) { updateVisibility(); };
        lfoTabs.onSelect = [this] (int index)
        {
            lfoThumbs.setSelected (index);
            updateVisibility();
        };

        filterTabs.onOpen = [this] { if (onOpenPage != nullptr) onOpenPage ("FILTER"); };
        envTabs.onOpen = [this] { if (onEditEnvelope != nullptr) onEditEnvelope (envTabs.getSelected()); };
        lfoTabs.onOpen = [this] { if (onEditLfo != nullptr) onEditLfo (lfoTabs.getSelected()); };

        addAll (*this, filterTabs, envTabs, lfoTabs);
        updateVisibility();
        updateStrips();

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        startTimerHz (8);
    }

    ~MainPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on" })
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);
    }

    std::function<void (int)> onEditLfo, onEditEnvelope;
    std::function<void (const juce::String&)> onOpenPage;

    static juce::Colour lfoColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xff35c8ff);
            case 2: return juce::Colour (0xff6fe3c1);
            case 3: return juce::Colour (0xffe3a56f);
            case 0: return IlanaTheme::accent();
            default: return IlanaSynthAudioProcessor::lfoColour (index);
        }
    }

    static juce::Colour filterColour (int index) { return index == 0 ? juce::Colour (0xffff4fd8) : juce::Colour (0xffb28aff); }

    static juce::Colour envColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xffff4fd8);
            case 2: return juce::Colour (0xffb28aff);
            case 3: return juce::Colour (0xff8fff3b);
            case 4: return juce::Colour (0xffffd447);
            default: return IlanaTheme::accent();
        }
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintCard (g, filterCard, "FILTER", filterColour (filterTabs.getSelected()));
        paintCard (g, envCard, "ENVELOPE", envColour (envTabs.getSelected()));
        paintCard (g, lfoCard, "LFO", lfoColour (lfoTabs.getSelected()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 10);
        auto left = area.removeFromLeft ((int) ((float) area.getWidth() * 0.54f));
        area.removeFromLeft (10);
        auto right = area;

        // Cards keep the three-oscillator size; added ones scroll.
        oscView.setBounds (left);
        const auto oscHeight = (left.getHeight() - 16) / 3;
        const auto anyHidden = std::find (shownStrips.begin(), shownStrips.end(), false) != shownStrips.end();
        auto columnHeight = anyHidden ? addButtonHeight : -8;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                columnHeight += oscHeight + 8;

        const auto scrolls = columnHeight > left.getHeight();
        oscColumn.setSize (left.getWidth() - (scrolls ? oscView.getScrollBarThickness() + 3 : 0),
                           juce::jmax (columnHeight, left.getHeight()));
        auto column = oscColumn.getLocalBounds();

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            if (! shownStrips[(size_t) osc])
            {
                oscCards[(size_t) osc] = {};
                continue;
            }

            oscCards[(size_t) osc] = column.removeFromTop (oscHeight);
            column.removeFromTop (8);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

        addOscButton.setVisible (anyHidden);
        addOscButton.setBounds (column.removeFromTop (addButtonHeight));
        oscColumn.repaint();

        const auto lfoHeight = juce::jlimit (132, 170, right.getHeight() / 3);
        const auto remaining = right.getHeight() - lfoHeight - 16;
        filterCard = right.removeFromTop ((int) ((float) remaining * 0.53f));
        right.removeFromTop (8);
        envCard = right.removeFromTop (remaining - filterCard.getHeight());
        right.removeFromTop (8);
        lfoCard = right;

        const auto placeTabs = [] (CardTabs& tabs, juce::Rectangle<int> card)
        {
            auto header = card.reduced (8, 0).withHeight (26).reduced (0, 5);
            tabs.setBounds (header.removeFromRight (tabs.getIdealWidth()));
        };

        placeTabs (filterTabs, filterCard);
        placeTabs (envTabs, envCard);
        placeTabs (lfoTabs, lfoCard);

        {
            auto inner = filterCard.reduced (10).withTrimmedTop (18);
            filterDisplay.setBounds (inner.removeFromTop (juce::jmax (50, inner.getHeight() - 96)));
            inner.removeFromTop (4);

            for (auto& set : filterSets)
            {
                auto row = inner;
                auto combos = row.removeFromLeft (104);
                set->items[0]->setBounds (combos.removeFromTop (combos.getHeight() / 2).reduced (0, 1));
                set->items[1]->setBounds (combos.reduced (0, 1));
                layoutRow (row, { set->items[2].get(), set->items[3].get(), set->items[4].get(), set->items[5].get(), set->items[6].get() });
            }
        }

        {
            auto inner = envCard.reduced (10).withTrimmedTop (18);
            auto displayArea = inner.removeFromLeft (juce::jmin (230, inner.getWidth() / 2));
            inner.removeFromLeft (6);

            for (auto& set : envSets)
            {
                set->display->setBounds (displayArea);
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(), set->items[3].get() });
            }
        }

        {
            auto inner = lfoCard.reduced (10).withTrimmedTop (18);
            auto cards = inner.removeFromTop (juce::jmax (40, inner.getHeight() - 52));
            lfoThumbs.setViewWidth (cards.getWidth());
            const auto thumbWidth = lfoThumbs.getPreferredWidth();
            lfoThumbView.setBounds (cards);
            lfoThumbs.setSize (thumbWidth, cards.getHeight() - (thumbWidth > cards.getWidth() ? lfoThumbView.getScrollBarThickness() + 1 : 0));
            inner.removeFromTop (4);

            for (auto& set : lfoSets)
            {
                auto row = inner;
                set->items[0]->setBounds (row.removeFromLeft (row.getWidth() * 24 / 100).reduced (2, 0));
                set->items[1]->setBounds (row.removeFromLeft (row.getWidth() * 34 / 100).reduced (2, 0));
                set->items[2]->setBounds (row.removeFromLeft (row.getWidth() / 3).reduced (2, 0));
                set->items[3]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
                set->items[4]->setBounds (row.reduced (2, 0));
            }
        }
    }

private:
    struct OscStrip
    {
        std::unique_ptr<ToggleControl> on;
        std::unique_ptr<ComboControl> mode, excite, table, warp;
        std::unique_ptr<juce::TextButton> remove;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        std::array<std::vector<juce::Component*>, 4> modeKnobs;
        int shownMode = -1;
        bool shownOn = true;
    };

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { updateStrips(); }

    int readInt (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // Shows the added oscillators, the controls for each one's mode, and dims
    // a switched-off one.
    void updateStrips()
    {
        auto changed = false;
        lastRevealVersion = processorRef.getRevealVersion();

        for (int index = 0; index < (int) strips.size(); ++index)
        {
            auto& strip = *strips[(size_t) index];
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            const auto mode = juce::jlimit (0, 3, readInt (prefix + "_mode"));
            const auto on = readInt (prefix + "_on") > 0;
            const auto shown = processorRef.isOscillatorShown (index);

            if (mode != strip.shownMode || shown != shownStrips[(size_t) index])
            {
                strip.shownMode = mode;
                shownStrips[(size_t) index] = shown;
                changed = true;

                for (auto& entry : strip.allKnobs)
                    entry.second->setVisible (false);

                for (auto* item : strip.modeKnobs[(size_t) mode])
                    item->setVisible (shown);

                strip.table->setVisible (shown && mode == 0);
                strip.warp->setVisible (shown && mode == 0);
                strip.excite->setVisible (shown && mode == 1);
                strip.on->setVisible (shown);
                strip.mode->setVisible (shown);
                wave (index).setVisible (shown);
            }

            if (on != strip.shownOn)
            {
                strip.shownOn = on;
                const auto alpha = on ? 1.0f : 0.4f;

                for (auto* item : { (juce::Component*) strip.mode.get(), (juce::Component*) strip.excite.get(),
                                    (juce::Component*) strip.table.get(), (juce::Component*) strip.warp.get(),
                                    (juce::Component*) &wave (index) })
                    item->setAlpha (alpha);

                for (auto& entry : strip.allKnobs)
                    entry.second->setAlpha (alpha);
            }
        }

        // Keep one oscillator on the page.
        const auto numShown = (int) std::count (shownStrips.begin(), shownStrips.end(), true);
        for (int index = 0; index < (int) strips.size(); ++index)
            strips[(size_t) index]->remove->setVisible (shownStrips[(size_t) index] && numShown > 1);

        if (changed)
            resized();
    }

    struct ControlSet
    {
        std::unique_ptr<EnvelopeDisplay> display;
        std::vector<std::unique_ptr<juce::Component>> items;
    };

    void updateVisibility()
    {
        const auto showSets = [] (std::vector<std::unique_ptr<ControlSet>>& sets, int selected)
        {
            for (int i = 0; i < (int) sets.size(); ++i)
            {
                const auto visible = i == selected;

                if (sets[(size_t) i]->display != nullptr)
                    sets[(size_t) i]->display->setVisible (visible);

                for (auto& item : sets[(size_t) i]->items)
                    item->setVisible (visible);
            }
        };

        showSets (filterSets, filterTabs.getSelected());
        showSets (envSets, envTabs.getSelected());
        showSets (lfoSets, lfoTabs.getSelected());
        repaint();
    }

    // Rate and division trade places with SYNC, like on the full page.
    void timerCallback() override
    {
        if (processorRef.getRevealVersion() != lastRevealVersion)
            updateStrips();

        if (! isShowing())
            return;

        const auto lfo = lfoTabs.getSelected();
        const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_sync");
        const auto synced = sync != nullptr && sync->load() > 0.5f;
        auto& items = lfoSets[(size_t) lfo]->items;

        for (auto [index, active] : { std::pair<int, bool> { 1, ! synced }, { 3, synced } })
        {
            const auto alpha = active ? 1.0f : 0.35f;

            if (items[(size_t) index]->getAlpha() != alpha)
                items[(size_t) index]->setAlpha (alpha);
        }
    }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint)
    {
        if (card.isEmpty())
            return;

        IlanaTheme::paintCard (g, card.toFloat(), 6.0f, tint);
        g.setColour (tint);
        g.fillEllipse ((float) card.getX() + 12.0f, (float) card.getY() + 11.0f, 6.0f, 6.0f);
        g.setFont (IlanaTheme::font (12.0f, true));
        g.drawText (title, juce::Rectangle<int> (card.getX() + 24, card.getY() + 6, 200, 16), juce::Justification::centredLeft);
    }

    WaveDisplay& wave (int index) { return *waves[(size_t) index]; }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];
        auto inner = card.reduced (10, 8);
        auto title = inner.removeFromTop (18);
        strip.remove->setBounds (title.removeFromRight (22).withSizeKeepingCentre (20, 15));
        title.removeFromRight (6);
        strip.on->setBounds (title.removeFromRight (56).withTrimmedTop (-13).withHeight (30));
        inner.removeFromTop (2);

        wave (index).setBounds (inner.removeFromLeft (juce::jmin (170, inner.getWidth() / 3)));
        inner.removeFromLeft (8);

        auto combos = inner.removeFromTop (40);
        const auto mode = juce::jmax (0, strip.shownMode);
        const auto third = combos.getWidth() / 3;
        strip.mode->setBounds (combos.removeFromLeft (third).reduced (3, 0));

        if (mode == 0)
        {
            strip.table->setBounds (combos.removeFromLeft (third).reduced (3, 0));
            strip.warp->setBounds (combos.reduced (3, 0));
        }
        else if (mode == 1)
        {
            strip.excite->setBounds (combos.removeFromLeft (third).reduced (3, 0));
        }

        layoutRow (inner, strip.modeKnobs[(size_t) mode]);
    }

    // The oscillator cards scroll inside this column.
    struct Column : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint != nullptr) onPaint (g); }
    };

    IlanaSynthAudioProcessor& processorRef;
    juce::Viewport oscView;
    Column oscColumn;
    juce::TextButton addOscButton;
    std::array<bool, OscillatorIds::count> shownStrips {};
    int lastRevealVersion = -1;
    static constexpr int addButtonHeight = 36;
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waves;
    FilterDisplay filterDisplay;
    juce::Viewport lfoThumbView;
    LfoThumbBar lfoThumbs;
    CardTabs filterTabs, envTabs, lfoTabs;
    std::vector<std::unique_ptr<OscStrip>> strips;
    std::vector<std::unique_ptr<ControlSet>> filterSets, envSets, lfoSets;
    std::array<juce::Rectangle<int>, OscillatorIds::count> oscCards;
    juce::Rectangle<int> filterCard, envCard, lfoCard;
};

class MatrixPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit MatrixPage (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            auto row = std::make_unique<MatrixRow> (p, i);
            list.addChildComponent (*row);
            rows.push_back (std::move (row));
        }

        addButton.setButtonText ("+  ADD MODULATION");
        addButton.setTooltip ("Add a routing.  You can also drag any source chip, macro name or LFO card onto a knob.");
        addButton.onClick = [this] { addRouting(); };
        list.addAndMakeVisible (addButton);

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);

        updateRows();
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto used = (int) visibleRows.size();

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText ("MODULATION", juce::Rectangle<int> (14, 6, 200, 18), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (12.0f));
        g.drawText (juce::String (used) + " of " + juce::String (Mod::maxSlots) + " slots in use.   "
                        "Tip: drag a source onto any knob, then drag the coloured dot beside the knob to set the depth.",
                    juce::Rectangle<int> (120, 6, 860, 18), juce::Justification::centredLeft);

        // Column headings, aligned with MatrixRow's layout.
        using C = MatrixRow::Columns;
        auto x = headerArea.getX() + C::number;
        const auto heading = [&g, &x, this] (const char* text, int width, int gapAfter)
        {
            g.drawText (text, juce::Rectangle<int> (x, headerArea.getY(), width, headerArea.getHeight()),
                        juce::Justification::centredLeft);
            x += width + gapAfter;
        };

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (IlanaTheme::font (10.5f, true));
        heading ("ON", C::bypass, C::gap + C::meter + C::gap);
        heading ("SOURCE", C::source, C::gap);
        heading ("VIA", C::via, C::gap * 2);
        heading ("AMOUNT", C::amount, C::gap);
        heading ("CURVE", C::curve, C::gap);
        heading ("POLARITY", C::polarity, C::gap * 3);
        heading ("DESTINATION", C::destination, C::gap);

        if (visibleRows.empty())
            paintEmptyState (g);
    }

    // An empty matrix explains the three ways in, with a little animated
    // routing and an arrow down to the source chips.
    void paintEmptyState (juce::Graphics& g)
    {
        const auto now = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const auto area = viewport.getBounds().withTrimmedTop (56);
        const auto card = juce::Rectangle<float> (560.0f, 250.0f).withCentre (area.toFloat().getCentre()).withY ((float) area.getY() + 20.0f);
        IlanaTheme::paintCard (g, card, 10.0f, IlanaTheme::accent().withAlpha (0.3f));

        // Source dot -> animated cable -> knob.
        const auto sourceCentre = juce::Point<float> (card.getX() + 150.0f, card.getY() + 62.0f);
        const auto knobCentre = juce::Point<float> (card.getRight() - 150.0f, card.getY() + 62.0f);
        juce::Path cable;
        cable.startNewSubPath (sourceCentre);
        cable.cubicTo (sourceCentre.translated (80.0f, -40.0f + 10.0f * std::sin (now * 2.0f)),
                       knobCentre.translated (-80.0f, 40.0f - 10.0f * std::sin (now * 2.0f)), knobCentre);
        g.setColour (IlanaTheme::accent().withAlpha (0.35f));
        g.strokePath (cable, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto travel = std::fmod (now * 0.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (cable.getPointAlongPath (travel * cable.getLength())));

        g.setColour (juce::Colour (0xff35c8ff));
        g.fillRoundedRectangle (juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.setFont (IlanaTheme::font (11.0f, true));
        g.drawText ("LFO 1", juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), juce::Justification::centred);

        const auto knob = juce::Rectangle<float> (34.0f, 34.0f).withCentre (knobCentre);
        g.setColour (juce::Colour (0xff202026));
        g.fillEllipse (knob);
        const auto sweep = 0.6f + 0.35f * std::sin (now * 2.0f);
        juce::Path arc;
        arc.addCentredArc (knobCentre.x, knobCentre.y, 21.0f, 21.0f, 0.0f, -2.4f, -2.4f + 4.8f * sweep, true);
        g.setColour (IlanaTheme::accent());
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (IlanaTheme::font (16.0f, true));
        g.drawText ("Nothing is modulated yet", card.withTrimmedTop (104.0f).withHeight (24.0f), juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (IlanaTheme::font (12.5f));
        const char* const tips[] {
            "Drag a source chip from the bar below onto any knob",
            "or right-click a knob for quick modulation",
            "or press  + ADD MODULATION  above to build a routing here"
        };

        for (int i = 0; i < 3; ++i)
            g.drawText (tips[i], card.withTrimmedTop (136.0f + (float) i * 22.0f).withHeight (20.0f), juce::Justification::centred);

        // A chevron bobbing towards the source chips.
        const auto bob = 4.0f * std::sin (now * 3.0f);
        const auto tip = juce::Point<float> (area.toFloat().getCentreX(), (float) getHeight() - 22.0f + bob);
        juce::Path chevron;
        chevron.startNewSubPath (tip.translated (-10.0f, -8.0f));
        chevron.lineTo (tip);
        chevron.lineTo (tip.translated (10.0f, -8.0f));
        g.setColour (IlanaTheme::accent().withAlpha (0.6f));
        g.strokePath (chevron, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 6);
        area.removeFromTop (22);
        headerArea = area.removeFromTop (18);
        viewport.setBounds (area);
        layoutList();
    }

    // Switching to the tab shows the current routings straight away.
    void visibilityChanged() override
    {
        if (isVisible())
            updateRows();
    }

private:
    static constexpr int rowHeight = 38;

    void addRouting()
    {
        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processorRef.readModSlot (i);

            if (slot.source == Mod::Source::None && slot.destination == 0)
            {
                // A new row starts from LFO 1 with no destination yet, so it
                // shows up but does nothing until a target is picked.
                processorRef.clearModSlot (i);
                processorRef.setModSlotValue (i, "src", (float) Mod::Source::Lfo1);
                processorRef.setModSlotValue (i, "amt", 0.5f);
                updateRows();
                viewport.setViewPosition (0, list.getHeight());
                return;
            }
        }
    }

    void updateRows()
    {
        refreshMacroNames();

        std::vector<int> used;

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processorRef.readModSlot (i);

            if (slot.source != Mod::Source::None || slot.destination != 0)
                used.push_back (i);
        }

        if (used != visibleRows)
        {
            visibleRows = used;

            for (auto& row : rows)
                row->setVisible (std::find (used.begin(), used.end(), row->getSlotIndex()) != used.end());

            addButton.setEnabled (used.size() < (size_t) Mod::maxSlots);
            layoutList();
            repaint();
        }

        for (const auto index : visibleRows)
            rows[(size_t) index]->refresh();
    }

    void layoutList()
    {
        const auto width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
        auto y = 0;

        for (const auto index : visibleRows)
        {
            rows[(size_t) index]->setBounds (0, y, width, rowHeight);
            y += rowHeight;
        }

        addButton.setBounds (juce::Rectangle<int> (0, y + 6, 220, 28));
        list.setSize (width, y + 40);
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        updateRows();

        if (visibleRows.empty())
            repaint();
    }

    // Shows the patch's macro names in the source lists.
    void refreshMacroNames()
    {
        juce::StringArray macroNames;

        for (int m = 0; m < 4; ++m)
            macroNames.add (processorRef.getMacroName (m));

        if (macroNames != shownMacroNames)
        {
            shownMacroNames = macroNames;

            for (auto& row : rows)
                row->setMacroNames (macroNames);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::StringArray shownMacroNames;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    std::vector<int> visibleRows;
    juce::TextButton addButton;
    juce::Rectangle<int> headerArea;
};

class TapGrid : public juce::Component,
                private juce::Timer
{
public:
    explicit TapGrid (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, area, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (10.5f, true));
        g.drawText ("CUSTOM TAP GRID", getLocalBounds().removeFromTop (14).reduced (9, 0),
                    juce::Justification::centredLeft);

        auto bars = getBarBounds().toFloat();
        const auto columnWidth = bars.getWidth() / 16.0f;

        for (int step = 0; step < 16; ++step)
        {
            auto cell = bars.withWidth (columnWidth).reduced (1.5f, 0.0f);
            bars.removeFromLeft (columnWidth);

            const auto value = getStep (step);

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (value > 0.001f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (step == hoverStep ? 0.95f : 0.65f));
                g.fillRoundedRectangle (cell.withTop (cell.getBottom() - cell.getHeight() * value), 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override { setFromMouse (event); }
    void mouseDrag (const juce::MouseEvent& event) override { setFromMouse (event); }
    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverStep != -1)
        {
            hoverStep = -1;
            repaint();
        }
    }

private:
    juce::Rectangle<int> getBarBounds() const
    {
        return getLocalBounds().reduced (8, 9).withTrimmedTop (12);
    }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds();
        const auto columnWidth = (float) bars.getWidth() / 16.0f;

        return juce::jlimit (0, 15, (int) ((float) (position.x - bars.getX()) / columnWidth));
    }

    float getStep (int index) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_taps_step" + juce::String (index + 1)))
            return value->load();

        return 0.0f;
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        const auto index = stepAt (event.getPosition());
        const auto value = juce::jlimit (0.0f, 1.0f,
                                         1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight()));

        if (auto* parameter = processorRef.apvts.getParameter ("fx_taps_step" + juce::String (index + 1)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter ("fx_taps_pattern")))
            if (choice->getIndex() != 6)
                choice->setValueNotifyingHost (choice->convertTo0to1 (6.0f));

        hoverStep = index;
        repaint();
    }

    void timerCallback() override { repaint(); }

    IlanaSynthAudioProcessor& processorRef;
    int hoverStep = -1;
};

// Each effect family has its own colour: drive/distortion warm, modulation
// blue-violet, time and space green-cyan, dynamics teal, filters/EQ pink.
inline juce::Colour fxColour (int type)
{
    switch (type)
    {
        case 1: case 2: case 3: case 25: case 26: case 28: return juce::Colour (0xffff7a45); // amp, drive, crush, ring, octaver, feedback
        case 4: case 16: case 19: case 20: case 21:        return juce::Colour (0xff4fd8c8); // comp, gate, utility, OTT, limiter
        case 6: case 7: case 8: case 14: case 15: case 22: case 23: case 24:
                                                           return juce::Colour (0xff7d8cff); // phaser .. freq shift
        case 9: case 10: case 17:                          return juce::Colour (0xff6fe38a); // delay, stutter, tape stop
        case 11: case 12: case 13:                         return juce::Colour (0xff45c8ff); // smear, freeze, reverb
        case 5: case 18: case 27: case 29:                 return juce::Colour (0xffff5fb0); // comb, tilt, vowel, EQ
        default:                                           return juce::Colour (0xff5a5a66);
    }
}

// Trance gate steps: bar height is each step's level, the playing step
// lights up. Editing a built-in pattern copies it into Custom first.
class GateGrid : public juce::Component,
                 public juce::SettableTooltipClient,
                 private juce::Timer
{
public:
    explicit GateGrid (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Drag up/down to set each step's level. Right-click a step to toggle it.");
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 6.0f);
        const auto colour = fxColour (16);
        const auto steps = numSteps();
        const auto playing = processorRef.getGateDisplayStep();
        auto bars = getBarBounds().toFloat();
        const auto columnWidth = bars.getWidth() / 16.0f;

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (10.5f, true));
        g.drawText ("STEPS", getLocalBounds().removeFromTop (14).reduced (9, 0), juce::Justification::centredLeft);

        for (int step = 0; step < 16; ++step)
        {
            const auto cell = bars.withWidth (columnWidth).reduced (1.5f, 0.0f);
            bars.removeFromLeft (columnWidth);
            const auto active = step < steps;
            const auto value = level (step);

            g.setColour (juce::Colours::white.withAlpha (active ? (step % 4 == 0 ? 0.11f : 0.07f) : 0.02f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (active && value > 0.001f)
            {
                g.setColour (colour.withAlpha (step == playing ? 1.0f : (step == hoverStep ? 0.85f : 0.6f)));
                g.fillRoundedRectangle (cell.withTop (cell.getBottom() - cell.getHeight() * value), 2.0f);
            }

            if (active && step == playing)
            {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (cell.withHeight (2.0f).withY (cell.getBottom() + 2.0f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            const auto step = stepAt (event.getPosition());
            setLevel (step, level (step) > 0.5f ? 0.0f : 1.0f);
            return;
        }

        setFromMouse (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            setFromMouse (event);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverStep = -1;
        repaint();
    }

private:
    juce::Rectangle<int> getBarBounds() const { return getLocalBounds().reduced (8, 10).withTrimmedTop (10); }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds();
        return juce::jlimit (0, 15, (int) ((float) (position.x - bars.getX()) / ((float) bars.getWidth() / 16.0f)));
    }

    int numSteps() const { return (int) processorRef.apvts.getRawParameterValue ("fx_gate_steps")->load(); }
    int pattern() const { return (int) processorRef.apvts.getRawParameterValue ("fx_gate_pattern")->load(); }

    float level (int step) const
    {
        if (pattern() == 8)
            return processorRef.apvts.getRawParameterValue ("fx_gate_step" + juce::String (step + 1))->load();

        return IlanaSynthAudioProcessor::gatePatternLevel (pattern(), step);
    }

    void setLevel (int step, float value)
    {
        // First edit of a built-in pattern: copy it into Custom.
        if (pattern() != 8)
        {
            for (int i = 0; i < 16; ++i)
                if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (i + 1)))
                    parameter->setValueNotifyingHost (IlanaSynthAudioProcessor::gatePatternLevel (pattern(), i));

            if (auto* choice = processorRef.apvts.getParameter ("fx_gate_pattern"))
                choice->setValueNotifyingHost (choice->convertTo0to1 (8.0f));
        }

        if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (step + 1)))
            parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));

        repaint();
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        auto value = 1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight());

        // Snap near the ends so full and closed steps are easy to hit.
        value = value > 0.92f ? 1.0f : (value < 0.08f ? 0.0f : value);
        hoverStep = stepAt (event.getPosition());
        setLevel (hoverStep, value);
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    int hoverStep = -1;
};

// Scrolling content for the FX stack: paints each module's panel and hands
// clicks back to the page.
class FxStackContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    std::function<void (juce::Point<int>)> onClick;

    void paint (juce::Graphics& g) override
    {
        if (painter != nullptr)
            painter (g);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr)
            onClick (event.getPosition());
    }
};

class FxPage : public juce::Component,
               private juce::Timer
{
public:
    explicit FxPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          tapGrid (p),
          ampMode (p.apvts, "fx_amp_mode", "MODE"),
          ampDrive (p.apvts, "fx_amp_drive", "DRIVE"),
          ampBass (p.apvts, "fx_amp_bass", "BASS"),
          ampMid (p.apvts, "fx_amp_mid", "MID"),
          ampTreble (p.apvts, "fx_amp_treble", "TREBLE"),
          ampLevel (p.apvts, "fx_amp_level", "LEVEL"),
          driveOn (p.apvts, "fx_drive_on", "ON"),
          driveAmount (p.apvts, "fx_drive_amount", "AMOUNT"),
          driveMix (p.apvts, "fx_drive_mix", "MIX"),
          foldAmount (p.apvts, "fx_fold", "FOLD"),
          crushOn (p.apvts, "fx_crush_on", "ON"),
          crushBits (p.apvts, "fx_crush_bits", "BITS"),
          crushDown (p.apvts, "fx_crush_down", "DOWN"),
          crushMix (p.apvts, "fx_crush_mix", "MIX"),
          compThreshold (p.apvts, "fx_comp_threshold", "THRESH"),
          compRatio (p.apvts, "fx_comp_ratio", "RATIO"),
          compAttack (p.apvts, "fx_comp_attack", "ATTACK"),
          compRelease (p.apvts, "fx_comp_release", "RELEASE"),
          compMakeup (p.apvts, "fx_comp_makeup", "MAKEUP"),
          compMix (p.apvts, "fx_comp_mix", "MIX"),
          combOn (p.apvts, "fx_comb_on", "ON"),
          combFreq (p.apvts, "fx_comb_freq", "FREQ"),
          combFeedback (p.apvts, "fx_comb_feedback", "FEEDBACK"),
          combMix (p.apvts, "fx_comb_mix", "MIX"),
          phaserOn (p.apvts, "fx_phaser_on", "ON"),
          phaserRate (p.apvts, "fx_phaser_rate", "RATE"),
          phaserDepth (p.apvts, "fx_phaser_depth", "DEPTH"),
          phaserFeedback (p.apvts, "fx_phaser_feedback", "FEEDBACK"),
          phaserMix (p.apvts, "fx_phaser_mix", "MIX"),
          chorusOn (p.apvts, "fx_chorus_on", "ON"),
          chorusRate (p.apvts, "fx_chorus_rate", "RATE"),
          chorusDepth (p.apvts, "fx_chorus_depth", "DEPTH"),
          chorusMix (p.apvts, "fx_chorus_mix", "MIX"),
          haasDelay (p.apvts, "fx_haas_delay", "DELAY MS"),
          haasMix (p.apvts, "fx_haas_mix", "WIDTH"),
          delayOn (p.apvts, "fx_delay_on", "ON"),
          delayTime (p.apvts, "fx_delay_time", "TIME"),
          delaySync (p.apvts, "fx_delay_sync", "SYNC"),
          delayDiv (p.apvts, "fx_delay_div", "DIV"),
          delayFeedback (p.apvts, "fx_delay_feedback", "FEEDBACK"),
          delayDamping (p.apvts, "fx_delay_damping", "DAMPING"),
          delayMix (p.apvts, "fx_delay_mix", "MIX"),
          delayPitch (p.apvts, "fx_delay_pitch", "TAPE PITCH"),
          delayWow (p.apvts, "fx_delay_wow", "TAPE WOW"),
          delayPingPong (p.apvts, "fx_delay_pingpong", "PING-PONG"),
          tapsOn (p.apvts, "fx_taps_on", "TAPS"),
          tapsPattern (p.apvts, "fx_taps_pattern", "PATTERN"),
          tapsMix (p.apvts, "fx_taps_mix", "TAPS MIX"),
          stutterOn (p.apvts, "fx_stutter_on", "ON"),
          stutterDiv (p.apvts, "fx_stutter_div", "DIV"),
          stutterMix (p.apvts, "fx_stutter_mix", "MIX"),
          smearOn (p.apvts, "fx_smear_on", "ON"),
          smearSize (p.apvts, "fx_smear_size", "GRAIN MS"),
          smearDensity (p.apvts, "fx_smear_density", "DENSITY"),
          smearMix (p.apvts, "fx_smear_mix", "MIX"),
          freezeOn (p.apvts, "fx_freeze_on", "ON"),
          freezeMix (p.apvts, "fx_freeze_mix", "MIX"),
          reverbOn (p.apvts, "fx_reverb_on", "ON"),
          reverbType (p.apvts, "fx_reverb_type", "ALGORITHM"),
          reverbSize (p.apvts, "fx_reverb_size", "SIZE"),
          reverbDamping (p.apvts, "fx_reverb_damping", "DAMPING"),
          reverbWidth (p.apvts, "fx_reverb_width", "WIDTH"),
          reverbMix (p.apvts, "fx_reverb_mix", "MIX"),
          flangerRate (p.apvts, "fx_flanger_rate", "RATE"),
          flangerDepth (p.apvts, "fx_flanger_depth", "DEPTH"),
          flangerFeedback (p.apvts, "fx_flanger_feedback", "FEEDBACK"),
          flangerMix (p.apvts, "fx_flanger_mix", "MIX"),
          dimRate (p.apvts, "fx_dim_rate", "RATE"),
          dimDepth (p.apvts, "fx_dim_depth", "DEPTH"),
          dimMix (p.apvts, "fx_dim_mix", "MIX"),
          gateDiv (p.apvts, "fx_gate_div", "DIV"),
          gatePattern (p.apvts, "fx_gate_pattern", "PATTERN"),
          gateSteps (p.apvts, "fx_gate_steps", "STEPS"),
          gateSwing (p.apvts, "fx_gate_swing", "SWING"),
          gateSmooth (p.apvts, "fx_gate_smooth", "SMOOTH"),
          gateMix (p.apvts, "fx_gate_mix", "MIX"),
          tapeStopTrigger (p.apvts, "fx_tape_stop_trigger", "STOP"),
          tapeStopTime (p.apvts, "fx_tape_stop_time", "TIME"),
          tapeStopMix (p.apvts, "fx_tape_stop_mix", "MIX"),
          tiltAmount (p.apvts, "fx_tilt", "TILT"),
          tiltLevel (p.apvts, "fx_tilt_level", "LEVEL"),
          utilGain (p.apvts, "fx_util_gain", "GAIN"),
          utilMono (p.apvts, "fx_util_mono", "MONO"),
          utilInvert (p.apvts, "fx_util_invert", "INVERT"),
          ottAmount (p.apvts, "fx_ott_amount", "AMOUNT"),
          ottMix (p.apvts, "fx_ott_mix", "MIX"),
          limitCeiling (p.apvts, "fx_limit_ceiling", "CEILING"),
          limitRelease (p.apvts, "fx_limit_release", "RELEASE"),
          widthAmount (p.apvts, "fx_width", "WIDTH"),
          widthMix (p.apvts, "fx_width_mix", "MIX"),
          tremRate (p.apvts, "fx_trem_rate", "RATE"),
          tremDepth (p.apvts, "fx_trem_depth", "DEPTH"),
          tremShape (p.apvts, "fx_trem_shape", "SHAPE"),
          shifterShift (p.apvts, "fx_shifter_shift", "SHIFT HZ"),
          shifterMix (p.apvts, "fx_shifter_mix", "MIX"),
          ringFreq (p.apvts, "fx_ring_freq", "FREQ"),
          ringMix (p.apvts, "fx_ring_mix", "MIX"),
          octaverMix (p.apvts, "fx_octaver_mix", "MIX"),
          vowelMorph (p.apvts, "fx_vowel_morph", "MORPH"),
          vowelMix (p.apvts, "fx_vowel_mix", "MIX"),
          delayTimeR (p.apvts, "fx_delay_time_r", "TIME R"),
          delayDuck (p.apvts, "fx_delay_duck", "DUCK"),
          stutterReverse (p.apvts, "fx_stutter_reverse", "REVERSE"),
          stutterPitch (p.apvts, "fx_stutter_pitch", "PITCH"),
          feedbackAmount (p.apvts, "fx_feedback_amount", "AMOUNT"),
          feedbackDelay (p.apvts, "fx_feedback_delay", "DELAY MS"),
          feedbackTone (p.apvts, "fx_feedback_tone", "TONE"),
          feedbackMix (p.apvts, "fx_feedback_mix", "MIX"),
          eqLowFreq (p.apvts, "fx_eq_low_freq", "LOW FREQ"), eqLowGain (p.apvts, "fx_eq_low_gain", "LOW GAIN"),
          eqMidFreq (p.apvts, "fx_eq_mid_freq", "MID FREQ"), eqMidGain (p.apvts, "fx_eq_mid_gain", "MID GAIN"),
          eqMidQ (p.apvts, "fx_eq_mid_q", "MID Q"),
          eqHighFreq (p.apvts, "fx_eq_high_freq", "HIGH FREQ"), eqHighGain (p.apvts, "fx_eq_high_gain", "HIGH GAIN"),
          eqCurve (p)
    {
        addAll (*this, eqLowFreq, eqLowGain, eqMidFreq, eqMidGain, eqMidQ, eqHighFreq, eqHighGain);
        addChildComponent (eqCurve);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("fx_slot1")))
            slotNames = choice->getAllValueStrings();

        addAll (*this, ampMode, ampDrive, ampBass, ampMid, ampTreble, ampLevel,
                driveOn, driveAmount, driveMix, foldAmount,
                crushOn, crushBits, crushDown, crushMix,
                compThreshold, compRatio, compAttack, compRelease, compMakeup, compMix,
                combOn, combFreq, combFeedback, combMix,
                phaserOn, phaserRate, phaserDepth, phaserFeedback, phaserMix,
                chorusOn, chorusRate, chorusDepth, chorusMix, haasDelay, haasMix,
                delayOn, delayTime, delaySync, delayDiv, delayFeedback, delayDamping, delayMix,
                delayPitch, delayWow, delayPingPong, tapsOn, tapsPattern, tapsMix,
                stutterOn, stutterDiv, stutterMix,
                smearOn, smearSize, smearDensity, smearMix, freezeOn, freezeMix,
                reverbOn, reverbType, reverbSize, reverbDamping, reverbWidth, reverbMix,
                flangerRate, flangerDepth, flangerFeedback, flangerMix,
                dimRate, dimDepth, dimMix,
                gateDiv, gatePattern, gateSteps, gateSwing, gateSmooth, gateMix,
                tapeStopTrigger, tapeStopTime, tapeStopMix,
                tiltAmount, tiltLevel,
                utilGain, utilMono, utilInvert,
                ottAmount, ottMix,
                limitCeiling, limitRelease,
                widthAmount, widthMix,
                tremRate, tremDepth, tremShape,
                shifterShift, shifterMix,
                ringFreq, ringMix,
                octaverMix,
                vowelMorph, vowelMix,
                delayTimeR, delayDuck,
                stutterReverse, stutterPitch,
                feedbackAmount, feedbackDelay, feedbackTone, feedbackMix);

        slotGroups.push_back ({});
        slotGroups.push_back ({ &ampMode, &ampDrive, &ampBass, &ampMid, &ampTreble, &ampLevel });
        slotGroups.push_back ({ &driveOn, &driveAmount, &driveMix, &foldAmount });
        slotGroups.push_back ({ &crushOn, &crushBits, &crushDown, &crushMix });
        slotGroups.push_back ({ &compThreshold, &compRatio, &compAttack, &compRelease, &compMakeup, &compMix });
        slotGroups.push_back ({ &combOn, &combFreq, &combFeedback, &combMix });
        slotGroups.push_back ({ &phaserOn, &phaserRate, &phaserDepth, &phaserFeedback, &phaserMix });
        slotGroups.push_back ({ &chorusOn, &chorusRate, &chorusDepth, &chorusMix });
        slotGroups.push_back ({ &haasDelay, &haasMix });
        slotGroups.push_back ({ &delayOn, &delayTime, &delayTimeR, &delaySync, &delayDiv, &delayFeedback,
                                &delayDamping, &delayPingPong, &delayMix, &delayPitch, &delayWow, &delayDuck,
                                &tapsOn, &tapsPattern, &tapsMix });
        slotGroups.push_back ({ &stutterOn, &stutterDiv, &stutterMix, &stutterReverse, &stutterPitch });
        slotGroups.push_back ({ &smearOn, &smearSize, &smearDensity, &smearMix });
        slotGroups.push_back ({ &freezeOn, &freezeMix });
        slotGroups.push_back ({ &reverbOn, &reverbType, &reverbSize, &reverbDamping, &reverbWidth, &reverbMix });
        slotGroups.push_back ({ &flangerRate, &flangerDepth, &flangerFeedback, &flangerMix });
        slotGroups.push_back ({ &dimRate, &dimDepth, &dimMix });
        slotGroups.push_back ({ &gateDiv, &gatePattern, &gateSteps, &gateSwing, &gateSmooth, &gateMix });
        slotGroups.push_back ({ &tapeStopTrigger, &tapeStopTime, &tapeStopMix });
        slotGroups.push_back ({ &tiltAmount, &tiltLevel });
        slotGroups.push_back ({ &utilGain, &utilMono, &utilInvert });
        slotGroups.push_back ({ &ottAmount, &ottMix });
        slotGroups.push_back ({ &limitCeiling, &limitRelease });
        slotGroups.push_back ({ &widthAmount, &widthMix });
        slotGroups.push_back ({ &tremRate, &tremDepth, &tremShape });
        slotGroups.push_back ({ &shifterShift, &shifterMix });
        slotGroups.push_back ({ &ringFreq, &ringMix });
        slotGroups.push_back ({ &octaverMix });
        slotGroups.push_back ({ &vowelMorph, &vowelMix });
        slotGroups.push_back ({ &feedbackAmount, &feedbackDelay, &feedbackTone, &feedbackMix });
        slotGroups.push_back ({ &eqLowFreq, &eqLowGain, &eqMidFreq, &eqMidGain, &eqMidQ, &eqHighFreq, &eqHighGain });

        prevSlotButton.onClick = [this] { moveSelectedSlot (-1); };
        nextSlotButton.onClick = [this] { moveSelectedSlot (1); };
        diceButton.onClick = [this] { processorRef.randomizeFxChain(); };
        saveChainButton.onClick = [this] { saveChain(); };
        loadChainButton.onClick = [this] { loadChain(); };
        chainAButton.onClick = [this]
        {
            if (! processorRef.isShowingChainA())
            {
                processorRef.switchFxChain();
                chainABChanged();
            }
        };
        chainBButton.onClick = [this]
        {
            if (processorRef.isShowingChainA())
            {
                processorRef.switchFxChain();
                chainABChanged();
            }
        };
        copyChainButton.onClick = [this]
        {
            processorRef.copyFxChainToOtherBank();
            chainABChanged();
        };
        loadIrButton.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Load Impulse Response (.wav)",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav");

            juce::Component::SafePointer<FxPage> safeThis (this);

            fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                          | juce::FileBrowserComponent::canSelectFiles,
                                      [safeThis] (const juce::FileChooser& chooser)
                                      {
                                          const auto file = chooser.getResult();

                                          if (file.existsAsFile() && safeThis != nullptr)
                                              safeThis->processorRef.loadReverbIr (file);
                                      });
        };

        addAndMakeVisible (prevSlotButton);
        addAndMakeVisible (nextSlotButton);
        addAndMakeVisible (diceButton);
        addAndMakeVisible (saveChainButton);
        addAndMakeVisible (loadChainButton);
        addAndMakeVisible (loadIrButton);
        addAndMakeVisible (chainAButton);
        addAndMakeVisible (chainBButton);
        addAndMakeVisible (copyChainButton);
        addAndMakeVisible (tapGrid);
        tapGrid.setVisible (false);

        // The final stage after the rack.
        softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "SOFT CLIP");
        clipGain = std::make_unique<StripKnob> (p, "master_clip_gain", "Clip Gain");
        addAndMakeVisible (*softClip);
        addAndMakeVisible (*clipGain);

        slotBlend.setSliderStyle (juce::Slider::LinearHorizontal);
        slotBlend.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        slotBlend.setTooltip ("Parallel blend for the selected slot: 0 is dry only, 1 is the full effect.");
        addAndMakeVisible (slotBlend);

        // Every loaded module's controls live in one scrolling stack.
        for (auto& group : slotGroups)
            for (auto* control : group)
                stackContent.addChildComponent (control);

        gateGrid = std::make_unique<GateGrid> (p);
        stackContent.addChildComponent (*gateGrid);
        stackContent.addChildComponent (tapGrid);
        stackContent.addChildComponent (eqCurve);
        stackContent.addChildComponent (loadIrButton);
        stackContent.painter = [this] (juce::Graphics& g) { paintStack (g); };
        stackContent.onClick = [this] (juce::Point<int> position)
        {
            for (const auto& panel : stackPanels)
            {
                if (panel.bounds.contains (position))
                {
                    selectedSlot = panel.slot;
                    bindBlend();
                    repaint();
                    stackContent.repaint();
                }
            }
        };

        stackView.setViewedComponent (&stackContent, false);
        stackView.setScrollBarsShown (true, false);
        stackView.setScrollBarThickness (8);
        addAndMakeVisible (stackView);

        // Quick picks for an empty rack: one click adds the effect.
        for (const auto& pick : { std::pair<int, const char*> { 13, "REVERB" }, { 9, "DELAY" }, { 7, "CHORUS" },
                                  { 2, "DRIVE" }, { 20, "OTT" }, { 16, "TRANCE GATE" }, { 6, "PHASER" }, { 29, "EQ" } })
        {
            auto button = std::make_unique<juce::TextButton> (juce::String ("+  ") + pick.second);
            const auto type = pick.first;
            button->setColour (juce::TextButton::buttonColourId, fxColour (type).withAlpha (0.18f));
            button->setColour (juce::TextButton::textColourOffId, fxColour (type).brighter (0.3f));
            button->setTooltip ("Add " + juce::String (pick.second).toLowerCase() + " to the first empty slot");
            button->onClick = [this, type]
            {
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                {
                    if (getSlotType (slot) == 0)
                    {
                        processorRef.assignFxSlot (slot + 1, type);
                        selectedSlot = slot;
                        return;
                    }
                }
            };
            addChildComponent (*button);
            quickAddButtons.push_back (std::move (button));
        }

        updateVisibility();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto type = getSlotType (selectedSlot);

        paintSectionTitle (g, "CHAIN", { 14, 6, 200, 14 });

        if (! outputStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, outputStrip.toFloat(), 6.0f);
            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (12.0f, true));
            g.drawText ("OUTPUT", outputStrip.withWidth (80).withTrimmedLeft (14), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (11.5f));
            g.drawText ("after the rack, before the master volume", outputStrip.reduced (14, 0),
                        juce::Justification::centredRight);
        }

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText ("BLEND", juce::Rectangle<int> (236, 15, 40, 18), juce::Justification::centredRight);

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto row = rowBounds (slot);
            const auto selected = slot == selectedSlot;
            const auto slotType = getSlotType (slot);
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            const auto bypassed = processorRef.apvts.getParameter (prefix + "_bypass")->getValue() > 0.5f;
            const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;
            const auto dragSource = cardDragActive && slot == selectedSlot;
            const auto dim = bypassed ? 0.45f : 1.0f;

            const auto typeColour = fxColour (slotType);

            if (dragSource)
                g.setColour (juce::Colour (0xff141416).withAlpha (0.5f));
            else if (slotType != 0)
            {
                // Loaded modules wear their family colour.
                juce::ColourGradient rowGradient (typeColour.withAlpha ((selected ? 0.34f : 0.18f) * dim), 0.0f, (float) row.getY(),
                                                  typeColour.withAlpha ((selected ? 0.16f : 0.06f) * dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }
            else
            {
                juce::ColourGradient rowGradient (juce::Colour (0xff20202a).withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  juce::Colour (0xff16161a).withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }

            g.fillRoundedRectangle (row.toFloat(), 6.0f);

            if (slotType != 0 && ! dragSource)
            {
                g.setColour (typeColour.withAlpha (dim));
                g.fillRoundedRectangle (row.toFloat().withWidth (4.0f).reduced (0.0f, 6.0f).translated (2.0f, 0.0f), 2.0f);
            }

            if (! selected && ! dragSource && rowHover[(size_t) slot] > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f * rowHover[(size_t) slot]));
                g.fillRoundedRectangle (row.toFloat(), 6.0f);
            }
            g.setColour ((selected ? (slotType != 0 ? typeColour : IlanaTheme::accent()) : juce::Colour (0xff33333a)).withMultipliedAlpha (dim));
            g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 6.0f, selected ? 1.6f : 1.0f);

            if (dragSource)
                continue;

            g.setColour (juce::Colours::white.withAlpha (0.35f * dim));
            g.setFont (IlanaTheme::font (11.0f, true));
            g.drawText (juce::String (slot + 1), row.reduced (8, 0).removeFromLeft (18), juce::Justification::centredLeft);

            const juce::Rectangle<float> led ((float) row.getRight() - 20.0f, (float) row.getCentreY() - 3.0f, 7.0f, 7.0f);

            if (slotType != 0 && ! bypassed)
            {
                g.setColour (typeColour);
                g.fillEllipse (led);
            }
            else if (slotType != 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawEllipse (led, 1.0f);
            }

            g.setColour ((slotType != 0 ? juce::Colours::white.withAlpha (0.92f)
                                        : IlanaTheme::accent().interpolatedWith (juce::Colours::white, 0.55f)
                                              .withAlpha (0.42f + 0.45f * rowHover[(size_t) slot]))
                             .withMultipliedAlpha (dim));
            g.setFont (IlanaTheme::font (13.0f, slotType != 0));
            g.drawFittedText (slotType != 0 ? getSlotName (slotType) : juce::String ("+  add effect"),
                              row.reduced (30, 6).withTrimmedRight (18), 1, juce::Justification::centredLeft);

            if (slotType != 0 && ! bypassed)
            {
                const auto cpu = processorRef.getFxSlotCpu (slot);

                if (cpu > 0.0005f)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.35f));
                    g.setFont (IlanaTheme::font (10.0f));
                    g.drawText (juce::String (cpu * 100.0f, 1) + "%",
                                juce::Rectangle<int> (row.getRight() - 78, row.getY() + 6, 26, 10),
                                juce::Justification::centredRight);
                }
            }

            if (slotType != 0 && soloed)
            {
                const juce::Rectangle<float> badge ((float) row.getRight() - 52.0f, (float) row.getY() + 6.0f, 16.0f, 10.0f);
                g.setColour (IlanaTheme::accent().withAlpha (0.9f));
                g.fillRoundedRectangle (badge, 2.5f);
                g.setColour (juce::Colours::black.withAlpha (0.85f));
                g.setFont (IlanaTheme::font (9.5f, true));
                g.drawText ("S", badge, juce::Justification::centred);
            }

            // Mini meter strip along the row bottom
            const auto meter = juce::Rectangle<float> ((float) row.getX() + 30.0f, (float) row.getBottom() - 8.0f,
                                                       (float) row.getWidth() - 58.0f, 2.5f);

            if (slotType == 4) // Comp: gain reduction
            {
                const auto reduction = juce::jlimit (0.0f, 1.0f, 1.0f - processorRef.getCompGainReduction());
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (IlanaTheme::accent().withAlpha (0.85f));
                g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * reduction), 1.2f);
            }
            else if (slotType == 9) // Delay: time bar
            {
                const auto division = juce::jlimit (0.0f, 1.0f, processorRef.apvts.getParameter ("fx_delay_div")->getValue());
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * division), 1.2f);
            }
            else if (slotType == 12 || slotType == 10 || slotType == 17) // Freeze / Stutter / TapeStop
            {
                const auto lit = processorRef.apvts.getParameter (slotType == 12 ? "fx_freeze_on"
                                                                                 : (slotType == 10 ? "fx_stutter_on"
                                                                                                   : "fx_tape_stop_trigger"))->getValue() > 0.5f;
                g.setColour (lit ? IlanaTheme::accent() : juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
            }
            else if (slotType == 22) // Widener: stereo correlation
            {
                auto correlation = 0.0f;

                if (updateCorrelation())
                    correlation = lastCorrelation;

                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (meter, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.fillRect (juce::Rectangle<float> (1.0f, meter.getHeight()).withCentre ({ meter.getCentreX(), meter.getCentreY() }));
                g.setColour (std::abs (correlation) > 0.9f ? juce::Colours::red.withAlpha (0.7f)
                                                           : IlanaTheme::accent().withAlpha (0.8f));
                g.fillRoundedRectangle (juce::Rectangle<float> (meter.getWidth() * 0.5f * correlation,
                                                                meter.getHeight())
                                            .withCentre ({ meter.getCentreX(), meter.getCentreY() }),
                                        1.2f);
            }
            else if (slotType == 6 || slotType == 7 || slotType == 14 || slotType == 15
                     || slotType == 23 || slotType == 11 || slotType == 8 || slotType == 24 || slotType == 25)
            {
                const auto x = meter.getX() + meter.getWidth() * (0.5f + 0.45f * std::sin (meterPhase * 0.5f));
                g.setColour (IlanaTheme::accent().withAlpha (0.75f));
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ x, meter.getCentreY() }));
            }
            else if (slotType != 0)
            {
                g.setColour (IlanaTheme::accent().withAlpha (0.45f * dim));
                g.fillRoundedRectangle (meter, 1.2f);
            }

        // Slot change flash
        if (slot == dropSlot && dropFlash > 0.01f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (dropFlash * 0.5f));
                g.drawRoundedRectangle (row.toFloat().expanded (dropFlash * 4.0f), 8.0f, 2.0f);
            }
        }

        // Chain bank switch sweep
        if (chainSweep > 0.01f)
        {
            const auto top = (float) rowsTop;
            const auto height = (float) (rowHeight * IlanaSynthAudioProcessor::numFxSlots);
            const auto y = top + (1.0f - chainSweep) * height;
            const auto alpha = chainSweep * 0.8f;

            g.setColour (IlanaTheme::accent().withAlpha (alpha * 0.22f));
            g.fillRoundedRectangle (14.0f, y - 12.0f, 300.0f, 24.0f, 6.0f);
            g.setColour (IlanaTheme::accent().withAlpha (alpha));
            g.fillRect (14.0f, y - 1.0f, 300.0f, 2.0f);
        }

        // Drag ghost
        if (cardDragActive && dragImage.isValid())
        {
            const auto ghost = juce::Rectangle<float> ((float) dragImage.getWidth(),
                                                       (float) dragImage.getHeight())
                                   .withPosition ((float) dragPosition.x - dragGrabOffset.x,
                                                  (float) dragPosition.y - dragGrabOffset.y);

            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (ghost.translated (3.0f, 3.0f), 6.0f);
            g.setOpacity (0.8f);
            g.drawImageAt (dragImage, (int) ghost.getX(), (int) ghost.getY());
            g.setOpacity (1.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.8f));
            g.drawRoundedRectangle (ghost, 6.0f, 1.5f);
        }

        juce::ignoreUnused (type);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText ("Click a slot to add or jump to it, drag to reorder, right-click to change it.",
                    juce::Rectangle<int> (stackView.getX(), stackView.getY() - 20, 600, 16), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto toolbar = area.removeFromTop (24);
        loadChainButton.setBounds (toolbar.removeFromRight (86).reduced (0, 3));
        toolbar.removeFromRight (4);
        saveChainButton.setBounds (toolbar.removeFromRight (86).reduced (0, 3));
        toolbar.removeFromRight (4);
        diceButton.setBounds (toolbar.removeFromRight (50).reduced (0, 3));
        toolbar.removeFromRight (12);
        nextSlotButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));
        toolbar.removeFromRight (4);
        prevSlotButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));
        toolbar.removeFromRight (12);
        copyChainButton.setBounds (toolbar.removeFromRight (64).reduced (0, 3));
        toolbar.removeFromRight (4);
        chainBButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));
        toolbar.removeFromRight (4);
        chainAButton.setBounds (toolbar.removeFromRight (26).reduced (0, 3));

        area.removeFromTop (6);

        auto chainColumn = area.removeFromLeft (300);
        rowsTop = chainColumn.getY() + 16;

        area.removeFromLeft (22);

        auto panel = area;
        outputStrip = panel.removeFromBottom (46);
        panel.removeFromBottom (6);
        {
            auto strip = outputStrip.reduced (6, 3);
            strip.removeFromLeft (80);
            softClip->setBounds (strip.removeFromLeft (120));
            strip.removeFromLeft (10);
            clipGain->setBounds (strip.removeFromLeft (160));
        }

        panel.removeFromTop (22); // hint line
        stackView.setBounds (panel);
        layoutStack();
        slotBlend.setBounds (280, 15, 150, 18);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto row = rowBounds (slot);

            if (! row.contains (event.getPosition()))
                continue;

            selectedSlot = slot;
            bindBlend();
            scrollToSlot (slot);
            stackContent.repaint();
            repaint();

            // Empty slots have nothing to bypass or drag, so any click picks a module.
            if (event.mods.isPopupMenu() || getSlotType (slot) == 0)
            {
                showTypeMenu (slot);
                return;
            }

            const juce::Rectangle<int> ledZone (row.getRight() - 28, row.getY(), 26, row.getHeight());

            if (ledZone.contains (event.getPosition()))
            {
                if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
                    parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);

                return;
            }

            const juce::Rectangle<int> soloZone (row.getRight() - 60, row.getY(), 32, row.getHeight());

            if (soloZone.contains (event.getPosition()))
            {
                if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_solo"))
                    parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);

                return;
            }

            dragImage = createComponentSnapshot (row);
            dragGrabOffset = event.getPosition() - juce::Point<int> (row.getX(), row.getY());
            dragPosition = event.getPosition();
            dragReady = true;
            return;
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragReady)
            return;

        dragPosition = event.getPosition();

        if (! cardDragActive && event.getDistanceFromDragStart() > 4)
            cardDragActive = true;

        if (! cardDragActive)
            return;

        const auto target = juce::jlimit (0, IlanaSynthAudioProcessor::numFxSlots - 1,
                                          (event.getPosition().y - rowsTop) / rowHeight);

        if (target != selectedSlot)
            moveSelectedSlotTo (target);

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragReady = false;

        if (cardDragActive)
        {
            dropSlot = selectedSlot;
            dropFlash = 1.0f;
        }

        cardDragActive = false;
        dragImage = {};
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            if (rowBounds (slot).contains (event.getPosition()))
            {
                selectedSlot = slot;
                showTypeMenu (slot);
                return;
            }
        }
    }

private:
    juce::Rectangle<int> rowBounds (int slot) const
    {
        return juce::Rectangle<int> (14, rowsTop + slot * rowHeight, 300, rowHeight - 4);
    }

    bool updateCorrelation()
    {
        if (correlationL.size() < 2048)
        {
            correlationL.assign (2048, 0.0f);
            correlationR.assign (2048, 0.0f);
        }

        processorRef.copyScopeData (correlationL.data(), correlationR.data(), 2048);

        auto sumLR = 0.0;
        auto sumLL = 0.0;
        auto sumRR = 0.0;

        for (int i = 0; i < 2048; ++i)
        {
            sumLR += (double) correlationL[(size_t) i] * (double) correlationR[(size_t) i];
            sumLL += (double) correlationL[(size_t) i] * (double) correlationL[(size_t) i];
            sumRR += (double) correlationR[(size_t) i] * (double) correlationR[(size_t) i];
        }

        const auto denominator = std::sqrt (sumLL * sumRR);

        if (denominator < 1.0e-9)
            return false;

        lastCorrelation = juce::jlimit (-1.0f, 1.0f, (float) (sumLR / denominator));
        return true;
    }

    int getSlotType (int slot) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1)))
            return juce::jlimit (0, juce::jmax (0, slotNames.size() - 1), (int) value->load());

        return 0;
    }

    juce::String getSlotName (int type) const
    {
        return juce::isPositiveAndBelow (type, slotNames.size()) ? slotNames[type] : juce::String ("-");
    }

    void showTypeMenu (int slot)
    {
        juce::PopupMenu menu;
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        const auto bypassed = processorRef.apvts.getParameter (prefix + "_bypass")->getValue() > 0.5f;
        const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;

        menu.addItem (1003, "Clear (None)", true, getSlotType (slot) == 0);
        menu.addSeparator();

        for (int i = 1; i < slotNames.size(); ++i)
            menu.addItem (i + 1, slotNames[i], true, getSlotType (slot) == i);

        menu.addSeparator();
        menu.addItem (1001, "Bypass", true, bypassed);
        menu.addItem (1002, "Solo (wet only)", true, soloed);

        juce::Component::SafePointer<FxPage> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis, slot] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                const auto slotPrefix = "fx_slot" + juce::String (slot + 1);

                                if (result >= 1 && result <= 1000)
                                {
                                    safeThis->processorRef.assignFxSlot (slot + 1, result - 1);
                                }
                                else if (result == 1001 || result == 1002)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (
                                            slotPrefix + (result == 1001 ? "_bypass" : "_solo")))
                                        parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
                                }
                                else if (result == 1003)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (slotPrefix))
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
                                }

                                safeThis->updateVisibility();
                                safeThis->repaint();
                            });
    }

    void moveSelectedSlot (int direction)
    {
        const auto target = selectedSlot + direction;

        if (target < 0 || target >= IlanaSynthAudioProcessor::numFxSlots)
            return;

        // A slot is its module plus its bypass, solo and blend settings; move
        // them together so a dragged slot keeps how it was set up.
        for (const auto* suffix : { "", "_bypass", "_solo", "_mix" })
        {
            auto* current = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1) + suffix);
            auto* other = processorRef.apvts.getParameter ("fx_slot" + juce::String (target + 1) + suffix);

            if (current != nullptr && other != nullptr)
            {
                const auto currentValue = current->getValue();
                const auto otherValue = other->getValue();

                current->setValueNotifyingHost (otherValue);
                other->setValueNotifyingHost (currentValue);
            }
        }

        selectedSlot = target;
        updateVisibility();
        repaint();
    }

    void moveSelectedSlotTo (int target)
    {
        while (selectedSlot != target)
        {
            const auto direction = target > selectedSlot ? 1 : -1;
            moveSelectedSlot (direction);
        }
    }

    void saveChain()
    {
        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();

        fileChooser = std::make_unique<juce::FileChooser> ("Save FX Chain",
                                                           directory.getChildFile ("My Chain.ilanafxchain"),
                                                           "*.ilanafxchain");

        juce::Component::SafePointer<FxPage> safeThis (this);

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file != juce::File() && safeThis != nullptr)
                                          safeThis->processorRef.saveFxChainToFile (file.withFileExtension ("ilanafxchain"));
                                  });
    }

    void loadChain()
    {
        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();

        fileChooser = std::make_unique<juce::FileChooser> ("Load FX Chain", directory, "*.ilanafxchain");

        juce::Component::SafePointer<FxPage> safeThis (this);

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file.existsAsFile() && safeThis != nullptr)
                                      {
                                          safeThis->processorRef.loadFxChainFromFile (file);
                                          safeThis->updateVisibility();
                                          safeThis->repaint();
                                      }
                                  });
    }

    void chainABChanged()
    {
        chainAButton.setToggleState (processorRef.isShowingChainA(), juce::dontSendNotification);
        chainBButton.setToggleState (! processorRef.isShowingChainA(), juce::dontSendNotification);
        chainSweep = 1.0f;
        updateVisibility();
        repaint();
    }

    void updateVisibility()
    {
        std::array<bool, 64> shown {};

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = getSlotType (slot);

            if (type > 0 && type < 64)
                shown[(size_t) type] = true;
        }

        for (int type = 0; type < (int) slotGroups.size(); ++type)
            for (auto* control : slotGroups[(size_t) type])
            {
                control->setVisible (shown[(size_t) type]);
                control->setAlpha (1.0f);
            }

        tapGrid.setVisible (shown[9]);
        gateGrid->setVisible (shown[16]);
        eqCurve.setVisible (shown[29]);
        loadIrButton.setVisible (shown[13]);

        bindBlend();
        resized();
    }

    void bindBlend()
    {
        if (boundBlendSlot != selectedSlot)
        {
            boundBlendSlot = selectedSlot;
            slotBlendAttachment.reset();

            if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1) + "_mix"))
                slotBlendAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameter, slotBlend, nullptr);
        }
    }

    // One panel per loaded slot, in chain order. A module type loaded twice
    // shares its settings, so later copies get a short note instead.
    std::vector<std::unique_ptr<juce::TextButton>> quickAddButtons;

    void layoutStack()
    {
        stackPanels.clear();
        const auto width = juce::jmax (100, stackView.getWidth() - stackView.getScrollBarThickness() - 4);
        auto y = 0;
        std::array<bool, 64> placed {};

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto type = getSlotType (slot);

            if (type <= 0 || type >= (int) slotGroups.size())
                continue;

            StackPanel panel;
            panel.slot = slot;
            panel.type = type;
            panel.duplicate = placed[(size_t) type];
            placed[(size_t) type] = true;

            const auto& group = slotGroups[(size_t) type];
            const auto rows = group.size() > 8 ? 2 : 1;
            auto height = 30 + (panel.duplicate ? 30 : rows * 112 + 8);

            if (! panel.duplicate && type == 29)
                height += 130;

            if (! panel.duplicate && type == 9)
                height += 58;

            if (! panel.duplicate && type == 16)
                height += 78;

            panel.bounds = { 0, y, width, height };
            y += height + 8;

            if (! panel.duplicate)
            {
                auto body = panel.bounds.reduced (10, 0);
                body.removeFromTop (30);

                if (group.size() > 8)
                {
                    const auto half = (int) (group.size() + 1) / 2;
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (group.begin(), group.begin() + half));
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (group.begin() + half, group.end()));
                }
                else
                {
                    const auto maxWidth = juce::jmin (body.getWidth(), (int) group.size() * 120);
                    layoutRow (body.removeFromTop (112).withWidth (maxWidth), group);
                }

                if (type == 29)
                    eqCurve.setBounds (body.removeFromTop (126).reduced (0, 2));

                if (type == 9)
                    tapGrid.setBounds (body.removeFromTop (54).reduced (0, 2));

                if (type == 16)
                    gateGrid->setBounds (body.removeFromTop (74).reduced (0, 2));

                if (type == 13)
                    loadIrButton.setBounds (panel.bounds.getRight() - 110, panel.bounds.getY() + 6, 96, 18);
            }

            stackPanels.push_back (panel);
        }

        stackContent.setSize (width, juce::jmax (y, stackView.getHeight()));
        stackContent.repaint();

        // Quick picks sit in the empty rack, four to a row.
        const auto empty = stackPanels.empty();
        auto grid = stackView.getBounds().withTrimmedTop (104).withSizeKeepingCentre (juce::jmin (620, stackView.getWidth() - 20), 84);
        grid.setY (stackView.getY() + 104);
        const auto columns = 4;
        const auto cellWidth = grid.getWidth() / columns;

        for (size_t i = 0; i < quickAddButtons.size(); ++i)
        {
            auto& button = *quickAddButtons[i];
            button.setVisible (empty);
            button.setBounds (grid.getX() + (int) (i % columns) * cellWidth, grid.getY() + (int) (i / columns) * 42,
                              cellWidth, 42);
            button.setBounds (button.getBounds().reduced (5, 4));
        }
    }

    void paintStack (juce::Graphics& g)
    {
        if (stackPanels.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (IlanaTheme::font (16.0f, true));
            g.drawText ("The rack is empty", stackContent.getLocalBounds().withTrimmedTop (40).withHeight (24),
                        juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (IlanaTheme::font (12.5f));
            g.drawText ("Start with one of these, or click any slot on the left for all 29 effects.",
                        stackContent.getLocalBounds().withTrimmedTop (68).withHeight (20), juce::Justification::centred);
            return;
        }

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto bounds = panel.bounds.toFloat();
            const auto selected = panel.slot == selectedSlot;
            const auto bypassed = processorRef.apvts.getParameter ("fx_slot" + juce::String (panel.slot + 1) + "_bypass")->getValue() > 0.5f;

            juce::ColourGradient body (colour.withAlpha (selected ? 0.16f : 0.1f), 0.0f, bounds.getY(),
                                       juce::Colour (0xff141418).withAlpha (0.9f), 0.0f, bounds.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (bounds, 8.0f);

            // The module's name, large and faint, fills the panel's open right side.
            if (! panel.duplicate && bounds.getHeight() > 80.0f)
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (panel.bounds.reduced (2));
                g.setColour (colour.withAlpha (0.06f));
                g.setFont (IlanaTheme::font (juce::jmin (64.0f, bounds.getHeight() * 0.42f), true));
                g.drawText (getSlotName (panel.type).toUpperCase(), panel.bounds.reduced (18, 10).withTrimmedTop (20),
                            juce::Justification::bottomRight);
            }

            g.setColour (colour.withAlpha (selected ? 0.9f : 0.35f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, selected ? 1.6f : 1.0f);

            auto header = panel.bounds.withHeight (28).reduced (12, 0);
            g.setColour (colour.withAlpha (bypassed ? 0.4f : 1.0f));
            g.fillRoundedRectangle (header.removeFromLeft (4).toFloat().reduced (0.0f, 7.0f), 2.0f);
            header.removeFromLeft (8);
            g.setFont (IlanaTheme::font (13.0f, true));
            g.drawText (getSlotName (panel.type).toUpperCase(), header, juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (IlanaTheme::font (11.0f));
            const auto note = juce::String ("SLOT ") + juce::String (panel.slot + 1) + (bypassed ? "  -  BYPASSED" : "");
            g.drawText (note, header.withTrimmedRight (panel.type == 13 ? 110 : 0), juce::Justification::centredRight);

            if (panel.duplicate)
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawText ("Shares its settings with the first " + getSlotName (panel.type) + " above.",
                            panel.bounds.withTrimmedTop (28).reduced (16, 0).withHeight (26), juce::Justification::centredLeft);
            }
        }
    }

    void scrollToSlot (int slot)
    {
        for (const auto& panel : stackPanels)
            if (panel.slot == slot)
                stackView.setViewPosition (0, juce::jmax (0, panel.bounds.getY() - 4));
    }

    void visibilityChanged() override
    {
        if (! isVisible())
        {
            cardDragActive = false;
            dragReady = false;
            dragImage = {};
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        auto hovered = -1;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (rowBounds (slot).contains (event.getPosition()))
                hovered = slot;

        if (hovered != hoveredRow)
        {
            hoveredRow = hovered;
            repaint();
        }
    }

    void timerCallback() override
    {
        meterPhase += 0.12f;
        dropFlash *= 0.85f;
        chainSweep *= 0.9f;
        paramsAppear = juce::jmin (1.0f, paramsAppear + 0.1f);

        const auto showingA = processorRef.isShowingChainA();

        if (chainAButton.getToggleState() != showingA)
        {
            chainAButton.setToggleState (showingA, juce::dontSendNotification);
            chainBButton.setToggleState (! showingA, juce::dontSendNotification);
        }

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto target = slot == hoveredRow ? 1.0f : 0.0f;
            rowHover[(size_t) slot] += (target - rowHover[(size_t) slot]) * 0.25f;

            const auto type = getSlotType (slot);

            if (type != lastTypes[(size_t) slot])
            {
                lastTypes[(size_t) slot] = type;
                dropSlot = slot;
                dropFlash = 1.0f;
            }
        }

        juce::String signature;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            signature += juce::String (getSlotType (slot)) + ",";

        if (signature != lastSignature)
        {
            lastSignature = signature;
            updateVisibility();
        }

        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    TapGrid tapGrid;
    std::unique_ptr<GateGrid> gateGrid;
    std::unique_ptr<ToggleControl> softClip;
    std::unique_ptr<StripKnob> clipGain;
    juce::Rectangle<int> outputStrip;
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;

    struct StackPanel
    {
        int slot = 0, type = 0;
        bool duplicate = false;
        juce::Rectangle<int> bounds;
    };

    juce::Viewport stackView;
    FxStackContent stackContent;
    std::vector<StackPanel> stackPanels;
    int selectedSlot = 0;
    static constexpr int rowHeight = 39;
    int rowsTop = 60;
    bool dragReady = false;
    bool cardDragActive = false;
    juce::Image dragImage;
    juce::Point<int> dragPosition;
    juce::Point<int> dragGrabOffset;
    float dropFlash = 0.0f;
    int dropSlot = -1;
    int hoveredRow = -1;
    std::array<float, IlanaSynthAudioProcessor::numFxSlots> rowHover {};
    std::array<int, IlanaSynthAudioProcessor::numFxSlots> lastTypes {};
    float paramsAppear = 1.0f;
    float chainSweep = 0.0f;
    std::vector<float> correlationL, correlationR;
    float lastCorrelation = 0.0f;
    juce::String lastSignature;
    juce::TextButton prevSlotButton { "<" };
    juce::TextButton nextSlotButton { ">" };
    juce::TextButton diceButton { "DICE" };
    juce::TextButton saveChainButton { "SAVE CHAIN" };
    juce::TextButton loadChainButton { "LOAD CHAIN" };
    juce::TextButton loadIrButton { "LOAD IR" };
    juce::TextButton chainAButton { "A" };
    juce::TextButton chainBButton { "B" };
    juce::TextButton copyChainButton { "COPY A/B" };
    juce::Slider slotBlend;
    std::unique_ptr<juce::SliderParameterAttachment> slotBlendAttachment;
    int boundBlendSlot = -1;
    std::unique_ptr<juce::FileChooser> fileChooser;
    float meterPhase = 0.0f;

    ComboControl ampMode;
    KnobControl ampDrive, ampBass, ampMid, ampTreble, ampLevel;
    ToggleControl driveOn;
    KnobControl driveAmount, driveMix, foldAmount;
    ToggleControl crushOn;
    KnobControl crushBits, crushDown, crushMix;
    KnobControl compThreshold, compRatio, compAttack, compRelease, compMakeup, compMix;
    ToggleControl combOn;
    KnobControl combFreq, combFeedback, combMix;
    ToggleControl phaserOn;
    KnobControl phaserRate, phaserDepth, phaserFeedback, phaserMix;
    ToggleControl chorusOn;
    KnobControl chorusRate, chorusDepth, chorusMix, haasDelay, haasMix;
    ToggleControl delayOn;
    KnobControl delayTime;
    ToggleControl delaySync;
    ComboControl delayDiv;
    KnobControl delayFeedback, delayDamping, delayMix, delayPitch, delayWow;
    ToggleControl delayPingPong;
    ToggleControl tapsOn;
    ComboControl tapsPattern;
    KnobControl tapsMix;
    ToggleControl stutterOn;
    ComboControl stutterDiv;
    KnobControl stutterMix;
    ToggleControl smearOn;
    KnobControl smearSize, smearDensity, smearMix;
    ToggleControl freezeOn;
    KnobControl freezeMix;
    ToggleControl reverbOn;
    ComboControl reverbType;
    KnobControl reverbSize, reverbDamping, reverbWidth, reverbMix;
    KnobControl flangerRate, flangerDepth, flangerFeedback, flangerMix;
    KnobControl dimRate, dimDepth, dimMix;
    ComboControl gateDiv, gatePattern;
    KnobControl gateSteps, gateSwing, gateSmooth, gateMix;
    ToggleControl tapeStopTrigger;
    KnobControl tapeStopTime, tapeStopMix;
    KnobControl tiltAmount, tiltLevel;
    KnobControl utilGain;
    ToggleControl utilMono, utilInvert;
    KnobControl ottAmount, ottMix;
    KnobControl limitCeiling, limitRelease;
    KnobControl widthAmount, widthMix;
    KnobControl tremRate, tremDepth;
    ComboControl tremShape;
    KnobControl shifterShift, shifterMix;
    KnobControl ringFreq, ringMix;
    KnobControl octaverMix;
    KnobControl vowelMorph, vowelMix;
    KnobControl delayTimeR, delayDuck;
    ToggleControl stutterReverse;
    KnobControl stutterPitch;
    KnobControl feedbackAmount, feedbackDelay, feedbackTone, feedbackMix;
    KnobControl eqLowFreq, eqLowGain, eqMidFreq, eqMidGain, eqMidQ, eqHighFreq, eqHighGain;
    EqCurve eqCurve;
};
} // namespace

IlanaSynthAudioProcessorEditor::IlanaSynthAudioProcessorEditor (IlanaSynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
   #if defined (_WIN32)
    // Some hosts create plugin windows on a thread that has been switched to
    // DPI-unaware, which makes Windows virtualise (bitmap-stretch) the editor
    // on scaled displays.  If the process itself is DPI aware we can safely
    // switch this thread back for the window creation so the window matches
    // the host's awareness; if the whole process is unaware there is nothing
    // a plug-in can do about the stretch, so we leave it alone.
    if (processDpiAwareness() > 0)
        previousDpiContext = setThreadDpiContext (perMonitorAwareV2Context());
   #endif

    setLookAndFeel (&lookAndFeel);

    {
        juce::PropertiesFile::Options options;
        options.applicationName = "ilanaSynth";
        options.filenameSuffix = "settings";
        options.folderName = "ilanaSynth";
        settings = std::make_unique<juce::PropertiesFile> (options);
    }

    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, settings->getIntValue ("themeIndex", 0));
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintHeader (g); };

    content.addAndMakeVisible (logo);
    content.addAndMakeVisible (infoStrip);

    keysButton.setClickingTogglesState (true);
    keysButton.setTooltip ("Keyboard\nShow or hide the on-screen keyboard.  Hiding it gives the pages more room.");
    keysButton.onClick = [this] { setKeyboardVisible (keysButton.getToggleState()); };
    infoStrip.setToolbarButton (&keysButton);

    infoStrip.onHelp = [this]
    {
        tutorial.setVisible (true);
        tutorial.toFront (false);
    };

    tutorial.onDismiss = [this] (bool dontShowAgain)
    {
        if (settings != nullptr)
        {
            settings->setValue ("seenIntro", dontShowAgain ? "1" : "0");
            settings->saveIfNeeded();
        }
    };

    auto* mainPage = new MainPage (p);
    auto* envLfoPage = new EnvLfoPage (p, *settings);

    tabs.addTab ("MAIN", juce::Colour (0xff18181c), mainPage, true);
    tabs.addTab ("OSC", juce::Colour (0xff18181c), new OscPageViewport (p), true);
    tabs.addTab ("FILTER", juce::Colour (0xff18181c), new FilterPage (p), true);
    tabs.addTab ("ENV/LFO", juce::Colour (0xff18181c), envLfoPage, true);

    mainPage->onEditLfo = [this, envLfoPage] (int lfo)
    {
        envLfoPage->selectLfo (lfo);
        tabs.setCurrentTabIndex (envLfoTabIndex);
    };

    mainPage->onEditEnvelope = [this, envLfoPage] (int envelope)
    {
        envLfoPage->selectEnvelope (envelope);
        tabs.setCurrentTabIndex (envLfoTabIndex);
    };

    mainPage->onOpenPage = [this] (const juce::String& name)
    {
        const auto index = tabs.getTabNames().indexOf (name);

        if (index >= 0)
            tabs.setCurrentTabIndex (index);
    };

    tabs.addTab ("FM", juce::Colour (0xff18181c), new FmPage (p), true);
    tabs.addTab ("ARP/SEQ", juce::Colour (0xff18181c), new SeqPage (p), true);
    tabs.addTab ("MATRIX", juce::Colour (0xff18181c), new MatrixPage (p), true);
    tabs.addTab ("FX", juce::Colour (0xff18181c), new FxPage (p), true);
    tabs.addTab ("SCOPE", juce::Colour (0xff18181c), new ScopeDisplay (p), true);

    content.addAndMakeVisible (tabs);

    // Bottom strip: macros, then performance controls, then master.
    for (int macro = 0; macro < 4; ++macro)
    {
        auto knob = std::make_unique<StripKnob> (p, "macro" + juce::String (macro + 1),
                                                 "Macro " + juce::String (macro + 1), macro,
                                                 juce::Colour (0xffffd447), false);
        content.addAndMakeVisible (*knob);
        macroKnobs.push_back (std::move (knob));
    }

    glideKnob = std::make_unique<StripKnob> (p, "glide", "Glide");
    legatoToggle = std::make_unique<ToggleControl> (p.apvts, "glide_legato", "LEGATO");
    legatoToggle->setTooltip ("Glide only between overlapping (legato) notes");
    content.addAndMakeVisible (*legatoToggle);
    bendKnob = std::make_unique<StripKnob> (p, "bend_range", "Bend");
    voicesKnob = std::make_unique<StripKnob> (p, "poly_voices", "Voices");
    masterKnob = std::make_unique<StripKnob> (p, "master", "Master", -1, juce::Colour (0xffffd447), false);
    outputMeter = std::make_unique<OutputMeter> (p);
    content.addAndMakeVisible (*outputMeter);
    voiceModeBox = std::make_unique<ComboControl> (p.apvts, "voice_mode", "VOICE MODE");

    for (auto* component : { static_cast<juce::Component*> (glideKnob.get()), static_cast<juce::Component*> (bendKnob.get()),
                             static_cast<juce::Component*> (voicesKnob.get()), static_cast<juce::Component*> (masterKnob.get()),
                             static_cast<juce::Component*> (voiceModeBox.get()) })
        content.addAndMakeVisible (*component);

    presetDisplay.onClick = [this] { togglePresetPanel(); };

    prevButton.onClick = [this]
    {
        const auto count = processorRef.getNumAllPresets();

        if (count > 0)
            loadPresetIndex ((juce::jmax (0, currentPresetIndex) + count - 1) % count);
    };
    nextButton.onClick = [this]
    {
        const auto count = processorRef.getNumAllPresets();

        if (count > 0)
            loadPresetIndex ((currentPresetIndex + 1) % count);
    };
    favButton.setClickingTogglesState (true);
    favButton.onClick = [this] { toggleFavourite(); updateHeaderButtons(); };
    saveButton.setText ("SAVE");
    saveButton.onClick = [this] { savePreset(); };
    moreButton.onClick = [this] { showPresetMenu(); };
    undoButton.onClick = [this] { processorRef.getUndoManager().undo(); };
    redoButton.onClick = [this] { processorRef.getUndoManager().redo(); };
    historyButton.onClick = [this] { showHistoryMenu(); };
    abButton.setTooltip ("A / B\nFlip between two versions of the patch to compare them.");
    abButton.onClick = [this] { toggleAB(); };
    diceButton.setText ("DICE");
    diceButton.onClick = [this] { showDiceMenu(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    for (auto* button : std::initializer_list<juce::Component*> { &presetDisplay, &prevButton, &nextButton, &favButton,
                                                                  &saveButton, &moreButton, &undoButton, &redoButton,
                                                                  &historyButton, &abButton, &diceButton, &settingsButton })
        content.addAndMakeVisible (*button);

    keyboard = std::make_unique<KeyboardStrip> (p);
    content.addAndMakeVisible (*keyboard);

    setWantsKeyboardFocus (true);

    // Macros are dragged from their own names in the strip, so they have no
    // chip here.
    const struct
    {
        const char* name;
        Mod::Source source;
    } chipSpecs[] = {
        { "LFO 1", Mod::Source::Lfo1 }, { "LFO 2", Mod::Source::Lfo2 }, { "LFO 3", Mod::Source::Lfo3 },
        { "LFO 4", Mod::Source::Lfo4 }, { "MOD ENV", Mod::Source::ModEnv }, { "FILT ENV", Mod::Source::FilterEnv },
        { "F2 ENV", Mod::Source::FilterEnv2 }, { "ENV 5", Mod::Source::Env4 }, { "MSEG", Mod::Source::Mseg },
        { "VELOCITY", Mod::Source::Velocity }, { "KEY", Mod::Source::KeyTrack }, { "RANDOM", Mod::Source::Random },
        { "WHEEL", Mod::Source::ModWheel }, { "PRESSURE", Mod::Source::Aftertouch }
    };

    for (const auto& spec : chipSpecs)
    {
        auto chip = std::make_unique<ModSourceChip> (spec.name, (int) spec.source);
        // The chip glows with its source while that source modulates
        // something (macros, the wheel and pressure always).
        chip->valueProvider = [this, source = spec.source]
        {
            using S = Mod::Source;
            const auto played = source == S::Macro1 || source == S::Macro2 || source == S::Macro3
                                || source == S::Macro4 || source == S::ModWheel || source == S::Aftertouch;

            if (! played && ! usedModSources[(size_t) juce::jlimit (0, (int) S::Count - 1, (int) source)])
                return 0.0f;

            if ((source == S::Velocity || source == S::KeyTrack || source == S::Random)
                && processorRef.getActiveVoiceCount() == 0)
                return 0.0f;

            return processorRef.getSourceDisplayValue ((int) source);
        };
        content.addAndMakeVisible (*chip);
        chips.push_back (std::move (chip));
    }

    content.addAndMakeVisible (tutorial);

    // Applied after adding: addAndMakeVisible forces the component visible.
    tutorial.setVisible (! settings->getBoolValue ("seenIntro", false));

    if (tutorial.isVisible())
        tutorial.toFront (false);

    keyboardVisible = settings->getBoolValue ("showKeyboard", true);
    keysButton.setToggleState (keyboardVisible, juce::dontSendNotification);
    keyboard->setVisible (keyboardVisible);

    updateHeaderButtons();
    updateUndoButtons();
    loadedFingerprint = parameterFingerprint();

    // Drag the corner (or the host's window edge) to any size between 75% and
    // 200%; the aspect ratio is fixed and the size is remembered.
    setResizable (true, true);

    startTimer (250);

    applyUiZoom ((float) settings->getDoubleValue ("uiZoom", 1.0));
    displayScaleApplied = settings->containsKey ("uiZoom") && ! juce::approximatelyEqual (uiZoom, 1.0f);

    tabs.getTabbedButtonBar().addChangeListener (this);
}

IlanaSynthAudioProcessorEditor::~IlanaSynthAudioProcessorEditor()
{
    tabs.getTabbedButtonBar().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void IlanaSynthAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto* page = tabs.getCurrentContentComponent())
    {
        IlanaAnim::replayPageAppear (*page);
        page->repaint();
    }

    startTabTransition();
}

void IlanaSynthAudioProcessorEditor::startTabTransition()
{
    if (transitionPage != nullptr)
    {
        transitionPage->setTransform ({});
        transitionPage->setAlpha (1.0f);
        transitionPage = nullptr;
    }

    if (auto* page = tabs.getCurrentContentComponent())
    {
        transitionPage = page;
        transitionStart = juce::Time::getMillisecondCounterHiRes();
        page->setAlpha (0.0f);

        const auto centre = page->getLocalBounds().toFloat().getCentre();
        page->setTransform (juce::AffineTransform::scale (0.985f, 0.985f, centre.x, centre.y)
                                .translated (0.0f, 12.0f));
        startTimerHz (60);
    }
}

float IlanaSynthAudioProcessorEditor::displayScale() const
{
    auto scale = 1.0f;

    if (auto* peer = getPeer())
    {
        scale = juce::jmax (scale, (float) peer->getPlatformScaleFactor());

       #if defined (_WIN32)
        scale = juce::jmax (scale, queryMonitorScale (peer->getNativeHandle()));
       #endif
    }

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmax (scale, (float) display->scale);

    return juce::jlimit (1.0f, 3.0f, scale);
}

float IlanaSynthAudioProcessorEditor::hostScaleFactor() const
{
    const auto& transform = getTransform();
    return (float) juce::jmax (std::abs (transform.mat00), std::abs (transform.mat11));
}

void IlanaSynthAudioProcessorEditor::applyUiZoom (float newZoom)
{
    uiZoom = juce::jlimit (0.75f, 2.0f, newZoom);

    if (settings != nullptr)
    {
        settings->setValue ("uiZoom", uiZoom);
        settings->saveIfNeeded();
    }

    setResizeLimits (juce::roundToInt ((float) designWidth * 0.75f), juce::roundToInt ((float) designHeight * 0.75f),
                     designWidth * 2, designHeight * 2);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (juce::roundToInt ((float) designWidth * uiZoom),
             juce::roundToInt ((float) designHeight * uiZoom));
}

void IlanaSynthAudioProcessorEditor::applyDisplayScale()
{
   #if ! defined (_WIN32)
    // macOS and Linux hosts scale plugin windows themselves.
    displayScaleApplied = true;
    return;
   #else
    // The thread context was switched to per-monitor aware for the window
    // creation; put it back now that the peer exists.
    if (previousDpiContext != nullptr && getPeer() != nullptr)
    {
        setThreadDpiContext (previousDpiContext);
        previousDpiContext = nullptr;
    }

    if (displayScaleApplied || getPeer() == nullptr)
        return;

    // If JUCE applied a host-window scale to this editor, it is already
    // handling the DPI and resizing ourselves would double up.
    if (hostScaleFactor() > 1.01f)
    {
        displayScaleApplied = true;
        return;
    }

    const auto scale = displayScale();

    if (scale <= 1.01f)
        return;

    displayScaleApplied = true;

    // The host is not scaling for us (our window is at design size), so size
    // ourselves for the real display DPI: the host then creates a matching
    // window and the UI renders at native resolution instead of being
    // bitmap-stretched.
    if (getWidth() > designWidth + 20)
        return;

    setResizeLimits (juce::roundToInt (795.0f * scale), juce::roundToInt (540.0f * scale),
                     juce::roundToInt (1590.0f * scale), juce::roundToInt (1080.0f * scale));

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    setSize (juce::roundToInt ((float) designWidth * scale),
             juce::roundToInt ((float) designHeight * scale));
   #endif
}

void IlanaSynthAudioProcessorEditor::timerCallback()
{
    applyDisplayScale();

    if (zoomNeedsSaving && ! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()
        && settings != nullptr)
    {
        zoomNeedsSaving = false;
        settings->setValue ("uiZoom", uiZoom);
        settings->saveIfNeeded();
    }


    // Program changes, host state restores and A/B swaps change the preset
    // behind the editor's back.
    if (processorRef.getCurrentPresetName() != shownPresetName)
        updateHeaderButtons();

    if (presetLoadFlash > 0.01f)
    {
        presetLoadFlash *= 0.86f;

        if (presetLoadFlash <= 0.01f)
            presetLoadFlash = 0.0f;
    }

    presetDisplay.setFlash (presetLoadFlash);

    // "Edited" marker: cheap checksum of every parameter against the one
    // taken when the preset was loaded.
    if (presetLoadFlash <= 0.01f)
        presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                                 parameterFingerprint() != loadedFingerprint);

    // Which sources the matrix uses, for the source chips' glow.
    {
        usedModSources.fill (false);

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.isActive())
            {
                usedModSources[(size_t) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) routing.source)] = true;
                usedModSources[(size_t) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) routing.aux)] = true;
            }
        }
    }

    if (transitionPage == nullptr)
    {
        updateUndoButtons();
        content.repaint (0, 0, designWidth, 56);

        const auto interval = presetLoadFlash > 0.01f ? 60 : 250;

        if (getTimerInterval() != interval)
            startTimer (interval);

        return;
    }

    constexpr double duration = 220.0;
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - transitionStart;
    const auto progress = juce::jlimit (0.0, 1.0, elapsed / duration);
    const auto eased = (float) IlanaAnim::easeOutCubic ((float) progress);
    const auto center = transitionPage->getLocalBounds().toFloat().getCentre();
    const auto scale = 0.985f + 0.015f * eased;

    transitionPage->setAlpha (eased);
    transitionPage->setTransform (juce::AffineTransform::scale (scale, scale, center.x, center.y)
                                      .translated (0.0f, (1.0f - eased) * 12.0f));

    if (progress >= 1.0)
    {
        transitionPage->setTransform ({});
        transitionPage->setAlpha (1.0f);
        transitionPage = nullptr;
        startTimer (presetLoadFlash > 0.01f ? 60 : 250);
    }
}

void IlanaSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());
}

void IlanaSynthAudioProcessorEditor::paintHeader (juce::Graphics& g)
{
    // Brushed metal faceplate.
    juce::ColourGradient headerGradient (juce::Colour (0xff33333d), 0.0f, 0.0f,
                                         juce::Colour (0xff1a1a1f), 0.0f, 56.0f, false);
    headerGradient.addColour (0.12, juce::Colour (0xff3a3a45));
    headerGradient.addColour (0.6, juce::Colour (0xff24242b));
    g.setGradientFill (headerGradient);
    g.fillRect (juce::Rectangle<int> (0, 0, designWidth, 56));

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<int> (0, 0, designWidth, 56));
        g.setTiledImageFill (IlanaTheme::metalTexture(), 137, 61, 0.4f);
        g.fillAll();
    }

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.fillRect (juce::Rectangle<int> (0, 1, designWidth, 1));

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.setFont (IlanaTheme::font (10.5f));
    g.drawText (juce::String ("v") + appVersion, juce::Rectangle<int> (236, 40, 50, 14), juce::Justification::centredLeft);

    juce::ColourGradient headerLine (IlanaTheme::accent().withAlpha (0.5f), 16.0f, 0.0f,
                                     IlanaTheme::accent().withAlpha (0.0f), (float) designWidth - 16.0f, 0.0f, false);
    g.setGradientFill (headerLine);
    g.fillRect (juce::Rectangle<int> (16, 54, designWidth - 32, 2));

    const auto cpu = processorRef.getCpuUsage() * 100.0f;

    const auto cpuColour = cpu < 30.0f
                               ? juce::Colours::white.withAlpha (0.4f)
                               : (cpu < 60.0f
                                      ? juce::Colours::white.withAlpha (0.4f)
                                            .interpolatedWith (IlanaTheme::accent(), (cpu - 30.0f) / 30.0f)
                                      : IlanaTheme::accent().interpolatedWith (juce::Colours::red,
                                                                               juce::jlimit (0.0f, 1.0f, (cpu - 60.0f) / 40.0f)));

    // Status line along the bottom edge of the header: tempo, voices, CPU.
    const auto statusY = 43;
    g.setFont (IlanaTheme::font (10.5f));
    g.setColour (cpuColour);
    g.drawText ("CPU " + juce::String (cpu, 0) + "%",
                juce::Rectangle<int> (designWidth - 80, statusY, 64, 11), juce::Justification::centredRight);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawText (juce::String (processorRef.getCurrentBpm(), 1) + " BPM",
                juce::Rectangle<int> (designWidth - 316, statusY, 70, 11), juce::Justification::centredRight);

    const auto activeVoices = processorRef.getActiveVoiceCount();

    for (int i = 0; i < 16; ++i)
    {
        const auto lit = i < activeVoices;
        g.setColour (lit ? IlanaTheme::accent().withAlpha (0.9f)
                         : juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse ((float) (designWidth - 232 + i * 8), (float) statusY + 3.0f, 5.0f, 5.0f);
    }
}

void IlanaSynthAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) designWidth;
    IlanaTheme::uiScaleRef() = scale * hostScaleFactor();

    // A corner drag changes the zoom; remember it (saved on the timer so a
    // drag doesn't hit the disk on every step).
    if (displayScaleApplied && std::abs (scale - uiZoom) > 0.005f)
    {
        uiZoom = scale;
        zoomNeedsSaving = true;
    }

    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));

    auto area = content.getLocalBounds();

    logo.setBounds (16, 5, 216, 46);

    // Header: preset display in the middle with its browse / save controls,
    // editing tools on the right. Buttons sit in the top 36 px; the status
    // line runs underneath them.
    auto headerRow = area.removeFromTop (56).withTrimmedTop (5).withHeight (32).withTrimmedRight (14);
    headerRow.removeFromLeft (292);

    constexpr int key = 30;
    settingsButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (6);
    diceButton.setBounds (headerRow.removeFromRight (66));
    headerRow.removeFromRight (6);
    abButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (12);
    historyButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    redoButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    undoButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (16);

    moreButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    saveButton.setBounds (headerRow.removeFromRight (70));
    headerRow.removeFromRight (8);
    favButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (3);
    nextButton.setBounds (headerRow.removeFromRight (26));
    prevButton.setBounds (headerRow.removeFromLeft (26));
    headerRow.removeFromLeft (3);
    headerRow.removeFromRight (3);
    presetDisplay.setBounds (headerRow.withTrimmedTop (-3).withHeight (headerRow.getHeight() + 6));

    // Bottom: source chips, the macro / performance strip, the info line and
    // the optional keyboard.
    if (keyboard != nullptr)
    {
        keyboard->setVisible (keyboardVisible);

        if (keyboardVisible)
            keyboard->setBounds (area.removeFromBottom (32).reduced (14, 2));
    }

    infoStrip.setBounds (area.removeFromBottom (24).reduced (14, 2));
    tutorial.setBounds (content.getLocalBounds());

    auto strip = area.removeFromBottom (54).reduced (14, 2);
    outputMeter->setBounds (strip.removeFromRight (12).withSizeKeepingCentre (12, 40));
    strip.removeFromRight (4);
    masterKnob->setBounds (strip.removeFromRight (108));
    strip.removeFromRight (4);
    voicesKnob->setBounds (strip.removeFromRight (88));
    voiceModeBox->setBounds (strip.removeFromRight (96).withSizeKeepingCentre (92, 40));
    strip.removeFromRight (6);
    bendKnob->setBounds (strip.removeFromRight (86));
    legatoToggle->setBounds (strip.removeFromRight (80).withSizeKeepingCentre (76, 44));
    glideKnob->setBounds (strip.removeFromRight (92));
    strip.removeFromRight (6);

    const auto macroWidth = strip.getWidth() / juce::jmax (1, (int) macroKnobs.size());

    for (auto& knob : macroKnobs)
        knob->setBounds (strip.removeFromLeft (macroWidth).withTrimmedRight (6));

    auto chipsRow = area.removeFromBottom (28).reduced (14, 2);

    if (! chips.empty())
    {
        const auto chipWidth = chipsRow.getWidth() / (int) chips.size();

        for (auto& chip : chips)
            chip->setBounds (chipsRow.removeFromLeft (chipWidth).reduced (2, 1));
    }

    tabs.setBounds (area.reduced (14, 0).withTrimmedBottom (2));
}

void IlanaSynthAudioProcessorEditor::setKeyboardVisible (bool shouldBeVisible)
{
    keyboardVisible = shouldBeVisible;
    keysButton.setToggleState (shouldBeVisible, juce::dontSendNotification);

    if (settings != nullptr)
    {
        settings->setValue ("showKeyboard", shouldBeVisible);
        settings->saveIfNeeded();
    }

    resized();
}

juce::int64 IlanaSynthAudioProcessorEditor::parameterFingerprint() const
{
    juce::int64 hash = 0;
    auto index = 1;

    for (auto* parameter : processorRef.getParameters())
    {
        hash += (juce::int64) juce::roundToInt (parameter->getValue() * 100000.0f) * (index * 2654435761LL % 1000003);
        ++index;
    }

    return hash;
}

void IlanaSynthAudioProcessorEditor::savePreset()
{
    juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> safeThis (this);

    PresetPanel::showSaveDialog (processorRef, [safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->loadedFingerprint = safeThis->parameterFingerprint();

        if (safeThis->presetPanel != nullptr)
            safeThis->presetPanel->refresh();
    });
}

void IlanaSynthAudioProcessorEditor::exportPreset()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("ilanaSynth Presets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Export Preset",
                                                       directory.getChildFile (processorRef.getCurrentPresetName() + ".ilanapreset"),
                                                       "*.ilanapreset");

    juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file != juce::File() && safeThis != nullptr)
                                  {
                                      safeThis->processorRef.savePresetToFile (file.withFileExtension ("ilanapreset"));

                                      if (safeThis->presetPanel != nullptr)
                                          safeThis->presetPanel->refresh();
                                  }
                              });
}

void IlanaSynthAudioProcessorEditor::loadPreset()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("ilanaSynth Presets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Load Preset", directory, "*.ilanapreset");

    juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile() && safeThis != nullptr)
                                      safeThis->processorRef.loadPresetFromFile (file);
                              });
}

void IlanaSynthAudioProcessorEditor::loadPresetIndex (int index)
{
    // One undo step for the whole preset, apart from the last edit.
    const auto names = processorRef.getAllPresetNames();
    processorRef.getUndoManager().beginNewTransaction ("Load " + names[index]);
    processorRef.loadPresetByIndex (index);
    presetLoadFlash = 1.0f;

    if (getTimerInterval() != 60)
        startTimer (60);

    updateHeaderButtons();
    loadedFingerprint = parameterFingerprint();
}

void IlanaSynthAudioProcessorEditor::showHistoryMenu()
{
    auto& undoManager = processorRef.getUndoManager();

    const auto undos = undoManager.getUndoDescriptions();
    const auto redos = undoManager.getRedoDescriptions();

    constexpr int maxEntries = 32;

    juce::PopupMenu menu;

    if (undos.isEmpty() && redos.isEmpty())
    {
        menu.addItem (1, "No history yet", false);
    }
    else
    {
        if (! redos.isEmpty())
        {
            menu.addSectionHeader ("REDO");

            for (int i = 0; i < juce::jmin (maxEntries, redos.size()); ++i)
                menu.addItem (2000 + i, redos[i].isNotEmpty() ? redos[i]
                                                              : "Redo change " + juce::String (i + 1));
        }

        if (! undos.isEmpty())
        {
            if (! redos.isEmpty())
                menu.addSeparator();

            menu.addSectionHeader ("UNDO");

            for (int i = 0; i < juce::jmin (maxEntries, undos.size()); ++i)
                menu.addItem (1000 + i, undos[i].isNotEmpty() ? undos[i]
                                                              : "Undo change " + juce::String (i + 1));
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&historyButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result < 1000)
                                return;

                            auto& manager = safeThis->processorRef.getUndoManager();

                            if (result < 2000)
                            {
                                for (int i = 0; i <= result - 1000; ++i)
                                    manager.undo();
                            }
                            else
                            {
                                for (int i = 0; i <= result - 2000; ++i)
                                    manager.redo();
                            }

                            safeThis->updateHeaderButtons();
                        });
}

void IlanaSynthAudioProcessorEditor::updateHeaderButtons()
{
    const auto nameChanged = processorRef.getCurrentPresetName() != shownPresetName;
    shownPresetName = processorRef.getCurrentPresetName();

    const auto names = processorRef.getAllPresetNames();
    currentPresetIndex = names.indexOf (shownPresetName);
    shownCategory = juce::isPositiveAndBelow (currentPresetIndex, names.size())
                        ? processorRef.getAllPresetCategories()[currentPresetIndex]
                        : juce::String();

    if (nameChanged)
        loadedFingerprint = parameterFingerprint();

    presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                             parameterFingerprint() != loadedFingerprint);
    favButton.setToggleState (isFavourite (shownPresetName), juce::dontSendNotification);
    favButton.setIconColour (isFavourite (shownPresetName) ? std::optional<juce::Colour> (juce::Colour (0xffffd447))
                                                           : std::nullopt);
    abButton.setButtonText (showingA ? "A" : "B");

    for (auto& knob : macroKnobs)
        knob->refreshName();
}

void IlanaSynthAudioProcessorEditor::updateUndoButtons()
{
    auto& undoManager = processorRef.getUndoManager();
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
    historyButton.setEnabled (undoManager.canUndo() || undoManager.canRedo());
}

bool IlanaSynthAudioProcessorEditor::isFavourite (const juce::String& presetName) const
{
    return settings != nullptr && settings->getValue ("fav_" + presetName, "0") == "1";
}

void IlanaSynthAudioProcessorEditor::toggleFavourite()
{
    if (settings == nullptr)
        return;

    const auto names = processorRef.getAllPresetNames();

    if (! juce::isPositiveAndBelow (currentPresetIndex, names.size()))
        return;

    const auto name = names[currentPresetIndex];
    settings->setValue ("fav_" + name, isFavourite (name) ? "0" : "1");
    settings->saveIfNeeded();
}

void IlanaSynthAudioProcessorEditor::toggleAB()
{
    if (showingA)
    {
        slotA = processorRef.apvts.copyState();

        if (slotB.isValid())
            processorRef.apvts.replaceState (slotB);
        else
            slotB = slotA;
    }
    else
    {
        slotB = processorRef.apvts.copyState();
        processorRef.apvts.replaceState (slotA);
    }

    showingA = ! showingA;
    abButton.setButtonText (showingA ? "A" : "B");
}

void IlanaSynthAudioProcessorEditor::setTheme (int newThemeIndex)
{
    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, newThemeIndex);
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    if (settings != nullptr)
    {
        settings->setValue ("themeIndex", themeIndex);
        settings->saveIfNeeded();
    }

    sendLookAndFeelChange();
}

void IlanaSynthAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Init patch");
    menu.addSeparator();
    menu.addItem (2, "Save preset...");
    menu.addItem (5, "Export preset file...");
    menu.addItem (3, "Load preset file...");
    menu.addItem (4, "Open user preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            switch (result)
                            {
                                case 1: safeThis->loadPresetIndex (0); break;
                                case 2: safeThis->savePreset(); break;
                                case 3: safeThis->loadPreset(); break;
                                case 5: safeThis->exportPreset(); break;
                                case 4:
                                {
                                    const auto directory = safeThis->processorRef.getUserPresetDirectory();
                                    directory.createDirectory();
                                    directory.startAsProcess();
                                    break;
                                }
                                default: break;
                            }
                        });
}

void IlanaSynthAudioProcessorEditor::showDiceMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("RANDOMISE");
    menu.addItem (1, "Whole patch");
    menu.addItem (2, "Oscillators only");
    menu.addItem (3, "Filters only");
    menu.addItem (4, "Envelopes only");
    menu.addItem (5, "Modulation only");
    menu.addItem (6, "Effects chain only");
    menu.addSeparator();
    menu.addSectionHeader ("MUTATE");
    menu.addItem (10, "Nudge  (small changes)");
    menu.addItem (11, "Shake  (bigger changes)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&diceButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            if (result == 1)
                                safeThis->randomize();
                            else if (result == 6)
                                safeThis->processorRef.randomizeFxChain();
                            else if (result == 10)
                                safeThis->mutate (0.06f);
                            else if (result == 11)
                                safeThis->mutate (0.18f);
                            else
                                safeThis->randomizeGroup (result);
                        });
}

void IlanaSynthAudioProcessorEditor::showSettingsMenu()
{
    const char* const skinNames[] { "Ember", "Ice", "Acid", "Neon" };
    const float zoomChoices[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

    juce::PopupMenu skins;

    for (int i = 0; i < IlanaTheme::numPalettes; ++i)
        skins.addItem (100 + i, skinNames[i], true, i == themeIndex);

    juce::PopupMenu sizes;

    for (int i = 0; i < 6; ++i)
        sizes.addItem (200 + i, juce::String (juce::roundToInt (zoomChoices[i] * 100.0f)) + "%",
                       true, juce::approximatelyEqual (uiZoom, zoomChoices[i]));

    juce::PopupMenu menu;
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Interface size", sizes);
    menu.addItem (300, "Show keyboard", true, keyboardVisible);
    menu.addItem (500, "MPE mode (per-note pitch, pressure and slide)", true,
                  processorRef.apvts.getRawParameterValue ("mpe_mode")->load() > 0.5f);
    menu.addSeparator();
    menu.addItem (400, "Show welcome tour");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            const float zooms[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

                            if (result >= 100 && result < 200)
                                safeThis->setTheme (result - 100);
                            else if (result >= 200 && result < 300)
                                safeThis->applyUiZoom (zooms[juce::jlimit (0, 5, result - 200)]);
                            else if (result == 300)
                                safeThis->setKeyboardVisible (! safeThis->keyboardVisible);
                            else if (result == 500)
                            {
                                if (auto* mpe = safeThis->processorRef.apvts.getParameter ("mpe_mode"))
                                {
                                    mpe->beginChangeGesture();
                                    mpe->setValueNotifyingHost (mpe->getValue() > 0.5f ? 0.0f : 1.0f);
                                    mpe->endChangeGesture();
                                }
                            }
                            else if (result == 400)
                            {
                                safeThis->tutorial.setVisible (true);
                                safeThis->tutorial.toFront (false);
                            }
                        });
}

void IlanaSynthAudioProcessorEditor::togglePresetPanel()
{
    if (presetPanel == nullptr)
    {
        presetPanel = std::make_unique<PresetPanel> (processorRef, settings.get());
        presetPanel->onLoad = [this] (int index, bool closeAfter)
        {
            loadPresetIndex (index);

            if (closeAfter && presetPanel != nullptr)
                presetPanel->close();
        };
        presetPanel->onFavouriteChanged = [this] { updateHeaderButtons(); };
        presetPanel->setAnchor (&presetDisplay);

        // Hidden until open() fades it in.
        content.addChildComponent (*presetPanel);
    }

    // Drop down under the preset name, kept inside the window.
    {
        const auto width = 640;
        const auto x = juce::jlimit (10, designWidth - 10 - width, presetDisplay.getX() - 40);
        presetPanel->setBounds (x, presetDisplay.getBottom() + 6, width, 500);
    }

    if (presetPanel->isOpen())
        presetPanel->close();
    else
        presetPanel->open();
}

void IlanaSynthAudioProcessorEditor::randomize()
{
    for (const auto group : { 2, 3, 4, 5 })
        randomizeGroup (group);
}

// Groups: 2 oscillators, 3 filters, 4 envelopes, 5 modulation.
void IlanaSynthAudioProcessorEditor::randomizeGroup (int group)
{
    juce::Random random;

    const auto setValue = [this] (const char* id, float value)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    const auto randomRange = [&random] (float minimum, float maximum)
    {
        return minimum + random.nextFloat() * (maximum - minimum);
    };

    const auto tableCount = TableFactory::getNumFactoryTables();

    if (group == 2)
    {
        setValue ("osc1_on", 1.0f);
        setValue ("sub_on", 1.0f);
        setValue ("osc2_mode", 0.0f);
        setValue ("sub_mode", 0.0f);

        if (random.nextFloat() < 0.25f)
        {
            setValue ("osc1_mode", 1.0f);
            setValue ("osc1_excite", (float) random.nextInt (4));
            setValue ("osc1_string_decay", randomRange (0.6f, 0.95f));
            setValue ("osc1_string_damp", randomRange (0.1f, 0.6f));
        }
        else
        {
            setValue ("osc1_mode", 0.0f);
        }

        setValue ("osc1_table", (float) random.nextInt (tableCount));
        setValue ("osc1_frame", random.nextFloat());
        setValue ("osc1_unison", (float) (1 + random.nextInt (4)));
        setValue ("osc1_detune", randomRange (5.0f, 30.0f));
        setValue ("osc2_on", random.nextBool() ? 1.0f : 0.0f);
        setValue ("osc2_table", (float) random.nextInt (tableCount));
        setValue ("osc2_frame", random.nextFloat());
        setValue ("osc2_semi", (float) (random.nextBool() ? -12 : 12));
        setValue ("subosc_on", random.nextFloat() < 0.6f ? 1.0f : 0.0f);
        setValue ("subosc_level", randomRange (0.2f, 0.6f));
        setValue ("sub_shape", (float) random.nextInt (3));
        setValue ("fm_amount", random.nextFloat() < 0.6f ? randomRange (0.0f, 0.5f) : 0.0f);
        setValue ("fm_feedback", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.4f) : 0.0f);
        setValue ("ring_mod", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.8f) : 0.0f);
        setValue ("hard_sync", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
        setValue ("drift", randomRange (0.0f, 0.5f));
        setValue ("osc1_chord", (float) random.nextInt (7));
        setValue ("voice_spread", randomRange (0.0f, 0.5f));
        setValue ("unison_random", randomRange (0.0f, 1.0f));
    }
    else if (group == 3)
    {
        // Low pass, band pass, high pass and both ladders; notch is rarely useful at random.
        const int types[] { FilterType::LowPass, FilterType::BandPass, FilterType::HighPass,
                            FilterType::LadderLow, FilterType::LadderLow, FilterType::LadderHigh };
        setValue ("f1_type", (float) types[random.nextInt (6)]);
        setValue ("f1_cutoff", randomRange (200.0f, 8000.0f));
        setValue ("f1_reso", randomRange (0.0f, 0.6f));
        setValue ("f1_env", randomRange (-1.0f, 3.0f));
        setValue ("res_on", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
    }
    else if (group == 4)
    {
        setValue ("fe_decay", randomRange (0.05f, 1.5f));
        setValue ("amp_attack", random.nextFloat() < 0.3f ? randomRange (0.05f, 0.8f) : 0.005f);
        setValue ("amp_release", randomRange (0.05f, 2.0f));
    }
    else if (group == 5)
    {
        setValue ("lfo1_rate", randomRange (0.2f, 8.0f));

        if (random.nextFloat() < 0.6f)
        {
            setValue ("mod1_src", 1.0f);
            setValue ("mod1_dst", random.nextFloat() < 0.5f ? 9.0f : 2.0f);
            setValue ("mod1_amt", randomRange (0.15f, 0.6f));
        }
        else
        {
            setValue ("mod1_src", 0.0f);
            setValue ("mod1_dst", 0.0f);
            setValue ("mod1_amt", 0.0f);
        }
    }
}

// Nudges every continuous sound parameter by a small random amount, keeping
// the patch's character. Routing, switches, levels at the end of the chain
// and the effect rack layout are left alone.
void IlanaSynthAudioProcessorEditor::mutate (float amount)
{
    juce::Random random;

    for (auto* parameter : processorRef.getParameters())
    {
        auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*> (parameter);

        if (floatParameter == nullptr)
            continue;

        const auto id = floatParameter->getParameterID();

        if (id == "master" || id.startsWith ("macro") || id.startsWith ("mod") || id.startsWith ("fx_slot")
            || id == "bend_range" || id == "master_clip_gain" || id.contains ("_step"))
            continue;

        // Only touch what's already doing something: a parameter sitting at
        // its default is usually off on purpose.
        const auto current = parameter->getValue();

        if (std::abs (current - parameter->getDefaultValue()) < 1.0e-4f && random.nextFloat() < 0.7f)
            continue;

        const auto offset = ((random.nextFloat() + random.nextFloat() + random.nextFloat()) / 1.5f - 1.0f) * amount;
        floatParameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, current + offset));
    }
}

bool IlanaSynthAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (tutorial.isVisible() && key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        tutorial.setVisible (false);

        if (settings != nullptr)
        {
            settings->setValue ("seenIntro", "1");
            settings->saveIfNeeded();
        }

        return true;
    }

    const auto modifiers = key.getModifiers();

    if (modifiers.isCommandDown() || modifiers.isCtrlDown())
    {
        const auto code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());

        if (code == 'Z')
        {
            if (modifiers.isShiftDown())
                processorRef.getUndoManager().redo();
            else
                processorRef.getUndoManager().undo();

            return true;
        }

        if (code == 'Y')
        {
            processorRef.getUndoManager().redo();
            return true;
        }

        return false;
    }

    const auto code = key.getKeyCode();

    if (code >= '1' && code <= '9')
    {
        if (code - '1' < tabs.getNumTabs())
        {
            tabs.setCurrentTabIndex (code - '1');
            return true;
        }

        return false;
    }

    return false;
}
