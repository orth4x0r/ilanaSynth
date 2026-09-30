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
#include "gui/VectorPad.h"
#include "gui/PhysicalView.h"
#include "gui/FmDiagram.h"
#include "gui/FmWidgets.h"
#include "gui/GenerativeWidgets.h"
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
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

// A page section's heading (a display, a pool, the rack): drawn like a card
// title, with a neutral tag, so every section on every page is headed the
// same way.
void paintSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                        const juce::String& subtitle = {})
{
    IlanaTheme::paintCardHeader (g, area, text, subtitle, IlanaTheme::Ui::text2, 0);
}

// Page headings sit where a card's title does: 12 px in from the card edge
// (cards start 12 px in from the page), 26 px tall like a card's header.
constexpr int headingX = 24, headingHeight = 26;

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
              registerMap (state, prefix + "_register", "REGISTER"),
              epDistance (state, prefix + "_ep_distance", "DISTANCE"),
              epPosition (state, prefix + "_ep_position", "OFFSET"),
              fbGain (state, prefix + "_fb_gain", "AMP GAIN"),
              fbDistance (state, prefix + "_fb_distance", "DISTANCE") {}

        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &stiffness, &pickup, &excitePos, &hardness, &pickPos, &bowPressure, &bowSpeed,
                                &bridgeBuzz, &fretRattle, &hammer, &couple, &damper, &registerMap,
                                &epDistance, &epPosition, &fbGain, &fbDistance })
                knob->setIdentityColour (colour);
        }

        KnobControl stiffness, pickup, excitePos, hardness, pickPos;
        KnobControl bowPressure, bowSpeed, bridgeBuzz, fretRattle;
        KnobControl hammer, couple, damper, registerMap;
        KnobControl epDistance, epPosition; // M7.3 Tine / Reed pickup
        KnobControl fbGain, fbDistance;     // M8.5 feedback amp
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
              uniMode (state, prefix + "_uni_mode", "UNI MODE"),
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
              grainLive (state, prefix + "_grain_live", "LIVE"),
              warp2 (state, prefix + "_warp2", "WARP 2"),
              pdEnv (state, prefix + "_pd_env", "WARP ENV"),
              warp2Amt (state, prefix + "_warp2_amt", "WARP 2 AMT"),
              pdEnvAmt (state, prefix + "_pd_env_amt", "ENV AMT") {}

        // Every knob in the oscillator's own colour, as on PLAY and PHYSICAL.
        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &warp2Amt, &pdEnvAmt, &frame, &level, &pan, &semi, &fine, &unison, &detune, &spread,
                                &stringDecay, &stringDamp, &stringSustain, &sampleStart, &sampleEnd, &sampleFadeIn,
                                &sampleFadeOut, &warpAmt, &uniBlend, &spectralAmt, &grainPosition, &grainSize,
                                &grainDensity, &grainSpray, &grainPitch, &grainSpread })
                knob->setIdentityColour (colour);
        }

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
        ToggleControl grainLive; // M7.5: grains from the live input (ilanaSynth FX)
    };

public:
    std::function<void()> onModeChanged;

    explicit OscPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          subShape (p.apvts, "sub_shape", "SHAPE"),
          subOctave (p.apvts, "sub_octave", "OCTAVE"),
          noiseLevel (p.apvts, "noise_level", "NOISE")
          , symOn (p.apvts, "sym_on", "ON"), symManual (p.apvts, "sym_manual", "MANUAL")
          , symAmount (p.apvts, "sym_amount", "AMOUNT"), symDecay (p.apvts, "sym_decay", "DECAY")
          , symCount (p.apvts, "sym_count", "STRINGS")
          , sbOn (p.apvts, "sb_on", "BOARD"), sbModel (p.apvts, "sb_model", "MODEL"), sbMix (p.apvts, "sb_mix", "BODY MIX"), sbTone (p.apvts, "sb_tone", "TONE")
          , sbSize (p.apvts, "sb_size", "SIZE"), stretch (p.apvts, "stretch", "STRETCH")
          , pedalRes (p.apvts, "pedal_res", "PEDAL RES"), mechKey (p.apvts, "mech_key", "KEY NOISE")
          , mechDamper (p.apvts, "mech_damper", "DAMPER NOISE"), mechPedal (p.apvts, "mech_pedal", "PEDAL NOISE")
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            controls[(size_t) i] = std::make_unique<OscControls> (p.apvts, prefix);
            controls[(size_t) i]->setColour (oscColour (i));
            waveDisplays[(size_t) i] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison",
                prefix + "_spread", prefix + "_detune", false, juce::String {},
                prefix + "_mode", i, oscColour (i), false);
            loadButtons[(size_t) i] = std::make_unique<juce::TextButton> ("LOAD .WAV");
            editButtons[(size_t) i] = std::make_unique<juce::TextButton> ("EDIT");
            bounceButtons[(size_t) i] = std::make_unique<juce::TextButton> ("BOUNCE");
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            physical[(size_t) i] = std::make_unique<PhysicalControls> (p.apvts, prefix);
            physical[(size_t) i]->setColour (oscColour (i));
            auto& physicalControls = *physical[(size_t) i];
            addAll (*this, physicalControls.stiffness, physicalControls.pickup, physicalControls.excitePos,
                    physicalControls.hardness, physicalControls.pickPos, physicalControls.slap,
                    physicalControls.bowPressure, physicalControls.bowSpeed,
                    physicalControls.bridgeBuzz, physicalControls.fretRattle,
                    physicalControls.hammer, physicalControls.couple,
                    physicalControls.damper, physicalControls.registerMap,
                    physicalControls.epDistance, physicalControls.epPosition,
                    physicalControls.fbGain, physicalControls.fbDistance);
        }

        addAll (*this, symOn, symManual, symAmount, symDecay, symCount);
        addAll (*this, sbOn, sbModel, sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal);
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
                    osc.grainSpray, osc.grainPitch, osc.grainSpread, osc.grainLive,
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
            auto& edit = *editButtons[(size_t) i];
            edit.setTooltip ("Edit this wavetable: draw frames, set harmonics, formulas and morphs.\n"
                             "A factory table is copied into one of the patch's 16 tables first.");
            edit.onClick = [this, i] { openTableEditor (i); };
            addAndMakeVisible (edit);
            auto& bounce = *bounceButtons[(size_t) i];
            bounce.setTooltip ("Resample: play the whole patch (one note, optionally with its effects) and put the "
                               "result on this oscillator, as a tuned sample or cut into a wavetable. "
                               "The bounce is saved inside the patch.");
            bounce.onClick = [this, i] { showBounceMenu (i); };
            addAndMakeVisible (bounce);
        }

        addAll (*this, subShape, subOctave, noiseLevel);
        sbOn.showAsSwitch();

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

        // Sub and voice are cards like the rest of the page: title, switch on
        // the right, labelled knobs underneath.
        voiceSpread = std::make_unique<KnobControl> (p.apvts, "voice_spread", "SPREAD");
        unisonRandom = std::make_unique<KnobControl> (p.apvts, "unison_random", "UNI PHASE");
        drift = std::make_unique<KnobControl> (p.apvts, "drift", "DRIFT");
        addAll (*this, *voiceSpread, *unisonRandom, *drift);

        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subOscLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB LEVEL", IlanaTheme::accent(), true);
        noiseStrip = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        addAll (*this, *subOscOn, *subOscLevel, *noiseStrip);
        noiseLevel.setVisible (false);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env" })
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on" })
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

        for (const auto* id : { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on" })
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
        if (bouncingOsc >= 0)
            updateBounce();

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

            const auto strip = juce::Rectangle<float> ((float) bounds.getX() + 2.0f, (float) bounds.getY() + 6.0f,
                                                       3.0f, (float) bounds.getHeight() - 12.0f);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (strip, 1.5f);

            const std::array<const char*, 5> modeNames { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
            const auto mode = juce::jlimit (0, 4, getMode (band));

            const auto headerY = headerCentreY (band, bounds);
            IlanaTheme::paintTag (g, { (float) bounds.getX() + 17.0f, (float) headerY }, tint);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (band + 1),
                        juce::Rectangle<int> (bounds.getX() + 28, headerY - 8, 60, 16),
                        juce::Justification::centredLeft);

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (isFolded (band) ? juce::String ("OFF  -  ") + modeNames[(size_t) mode] + "  -  switch on to edit"
                                        : juce::String (modeNames[(size_t) mode]),
                        juce::Rectangle<int> (bounds.getX() + 80, headerY - 7, 400, 14),
                        juce::Justification::centredLeft);

            if (! controlBay[(size_t) band].isEmpty())
                IlanaTheme::paintRecessedPanel (g, controlBay[(size_t) band].toFloat(), 6.0f);

            if (! chainLabel[(size_t) band].isEmpty())
            {
                IlanaTheme::paintRecessedPanel (g, chainBay[(size_t) band].toFloat(), 6.0f);
                const auto label = chainLabel[(size_t) band];
                g.setColour (tint.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                g.drawText ("WARP CHAIN", label.withHeight (18), juce::Justification::centredLeft);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawFittedText ("second stage and\nthe DCW envelope", label.withTrimmedTop (18), juce::Justification::topLeft, 2);
            }
        }

        // Every card on the page titled the same way: a tag, the title, and a
        // quiet subtitle right after it.
        const auto cardTitle = [&g] (juce::Rectangle<int> header, const juce::String& title, const juce::String& subtitle,
                                     juce::Colour tag)
        {
            IlanaTheme::paintTag (g, { (float) header.getX() + 17.0f, (float) header.getCentreY() }, tag);
            const auto titleFont = IlanaTheme::font (IlanaTheme::TextSize::body, true);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (titleFont);
            auto area = header.withTrimmedLeft (28);
            g.drawText (title, area, juce::Justification::centredLeft);

            if (subtitle.isNotEmpty())
            {
                area.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (juce::Font (titleFont), title) + 16);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText (subtitle, area.withTrimmedRight (90), juce::Justification::centredLeft);
            }
        };

        if (! subStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, subStrip.toFloat(), 6.0f);
            cardTitle (subStrip.withHeight (symHeaderHeight), "SUB + NOISE", "a sub an octave or two under the oscillators, and noise", IlanaTheme::accent());
        }

        if (! voiceStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, voiceStrip.toFloat(), 6.0f);
            cardTitle (voiceStrip.withHeight (symHeaderHeight), "VOICE", "how the unison voices spread and drift", IlanaTheme::Ui::text2);
        }

        if (! symCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, symCard.toFloat(), 6.0f);
            cardTitle (symCard.withHeight (symHeaderHeight), "SYMPATHETIC STRINGS",
                       "shared drone strings that ring with everything you play", IlanaTheme::Ui::text2);
        }

        if (! keysCard.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, keysCard.toFloat(), 6.0f);
            cardTitle (keysCard.withHeight (symHeaderHeight), "ACOUSTIC KEYS",
                       "soundboard, tuning, sustain pedal (CC64) and the action's noises; for Physical oscillators with the Hammer",
                       IlanaTheme::Ui::text2);
        }
    }

    static juce::Colour oscColour (int index) { return IlanaTheme::oscColour (index); }

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
                                   (fitHeight - pageMargin * 2 - bandGap * 4 - bandFitStripHeight - symCardHeight()) / 3);
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
        // Sub and voice share the page's eight-column grid (four each), as
        // do the sympathetic strings below.
        subStrip = strip.removeFromLeft (strip.getWidth() / 2 - 4);
        strip.removeFromLeft (8);
        voiceStrip = strip;

        // Header (title, and the sub's switch on the right like every card's),
        // then one row of labelled controls.
        {
            auto header = subStrip.withHeight (symHeaderHeight);
            subOscOn->setBounds (IlanaTheme::cardSwitchBounds (subStrip, header.getCentreY()));
            layoutRow (subStrip.withTrimmedTop (symHeaderHeight).reduced (8, 0).withTrimmedBottom (4),
                       { &subShape, &subOctave, subOscLevel.get(), noiseStrip.get() });
            layoutRow (voiceStrip.withTrimmedTop (symHeaderHeight).reduced (8, 0).withTrimmedBottom (4),
                       { voiceSpread.get(), unisonRandom.get(), drift.get(), nullptr });
        }

        // The shared sympathetic strings: a one-line header with their switch,
        // which opens into their settings (and the notes, in MANUAL).
        area.removeFromTop (bandGap);
        symCard = area.removeFromTop (symCardHeight());
        auto symArea = symCard;
        auto header = symArea.removeFromTop (symHeaderHeight);
        // ToggleControl keeps 13 px above its button for a label; place it so
        // the button itself sits centred on the header line.
        symOn.setBounds (IlanaTheme::cardSwitchBounds (symCard, header.getCentreY()));

        if (readBool ("sym_on"))
        {
            symArea.reduce (10, 0);
            // Exactly on the SUB card's four columns above.
            layoutRow (symCard.withTrimmedTop (symHeaderHeight).withHeight (symRowHeight).withWidth (subStrip.getWidth()).reduced (8, 0),
                       { &symAmount, &symDecay, &symCount, &symManual });
            symArea.removeFromTop (symRowHeight);

            if (readBool ("sym_manual"))
                layoutSlots (symArea.removeFromTop (symRowHeight),
                             { symNotes[0].get(), symNotes[1].get(), symNotes[2].get(),
                               symNotes[3].get(), symNotes[4].get(), symNotes[5].get() });
        }

        area.removeFromTop (bandGap);
        keysCard = area.removeFromTop (keysCardHeight);
        auto keysArea = keysCard.withTrimmedTop (symHeaderHeight).reduced (10, 0);
        layoutSlots (keysArea.removeFromTop (symRowHeight),
                     { &sbOn, &sbModel, &sbMix, &sbTone, &sbSize, &stretch, &pedalRes, &mechKey, &mechDamper, &mechPedal });
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
    // Dials the size of the oscillator cards' above.
    static constexpr int stripHeight = 28 + 13 + 42 + 16 + 12;
    // The oscillator cards are sized as when the sub and voice were a 50 px
    // strip; the taller cards just scroll a little further.
    static constexpr int bandFitStripHeight = 50;
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

    // A switched-off oscillator folds to its title line.
    bool isFolded (int index) const
    {
        return ! readBool (juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]) + "_on");
    }

    static constexpr int foldedHeight = 40;

    int heightOfBand (int index) const
    {
        if (isFolded (index))
            return foldedHeight;

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

    // The header line's centre: a folded card is just that line, centred.
    int headerCentreY (int index, juce::Rectangle<int> band) const
    {
        return isFolded (index) ? band.getCentreY() : band.getY() + 17;
    }

    void layoutBand (juce::Rectangle<int> band, int index)
    {
        auto titleStrip = band.reduced (8, 0).withHeight (18).withY (headerCentreY (index, band) - 9);
        removeButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (22).withSizeKeepingCentre (20, 15));
        titleStrip.removeFromRight (6);
        // The on switch keeps one place, left of the remove button, whether
        // the card is folded or open (it used to jump into the controls when
        // switched on); the card's buttons come before it.
        controls[(size_t) index]->on.setBounds (IlanaTheme::cardSwitchBounds (band, headerCentreY (index, band), true));
        titleStrip.removeFromRight (56 + 12);
        loadButton (index).setBounds (titleStrip.removeFromRight (86).withSizeKeepingCentre (86, 15));
        titleStrip.removeFromRight (4);
        if (getMode (index) == 0)
        {
            editButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (44).withSizeKeepingCentre (44, 15));
            titleStrip.removeFromRight (4);
        }
        bounceButtons[(size_t) index]->setBounds (titleStrip.removeFromRight (bounceButtonWide == index ? 92 : 58).withSizeKeepingCentre (bounceButtonWide == index ? 92 : 58, 15));

        if (isFolded (index))
        {
            // Just the switch on the title line; the rest opens when it's on.
            controlBay[(size_t) index] = {};
            chainLabel[(size_t) index] = {};
            return;
        }

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
            if (IlanaSynthAudioProcessor::isEffectBuild)
                layoutSlots (topRow, { &osc.mode, &osc.grainLive, &osc.sampleTuned, &osc.sampleReverse,
                                       &osc.uniMode, &osc.chord, &osc.ampEnv });
            else
                layoutSlots (topRow, { &osc.mode, &osc.sampleTuned, &osc.sampleReverse,
                                       &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (bottomRow, { &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                                      &osc.grainSpray, &osc.grainPitch, &osc.grainSpread,
                                      &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune });
            return;
        }

        if (mode == 4)
        {
            // M7.5 Live: the input itself, so no pitch or shape controls.
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.ampEnv });
            layoutSlots (bottomRow, { &osc.level, &osc.pan, nullptr, nullptr, nullptr, nullptr });
            return;
        }

        if (isString && isElectric (index))
        {
            // M7.3 Tine / Reed: the pickup and the hammer; the string's own
            // controls don't apply.
            auto& physicalControls = *physical[(size_t) index];
            auto middleRow = bottomRow.removeFromTop (bottomRow.getHeight() / 2);
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.excite, &osc.uniMode, &osc.chord, &osc.ampEnv });
            layoutSlots (middleRow, { &osc.stringDecay, &osc.stringDamp, &physicalControls.epDistance,
                                      &physicalControls.epPosition, &physicalControls.hammer, &physicalControls.damper });
            layoutSlots (bottomRow, { &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune, &osc.uniBlend, &osc.spread });
            return;
        }

        if (isString)
        {
            auto& physicalControls = *physical[(size_t) index];
            auto middleRow = bottomRow.removeFromTop (bottomRow.getHeight() / 3);
            auto extraRow = bottomRow.removeFromTop (bottomRow.getHeight() / 2);
            controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
            layoutSlots (topRow, { &osc.mode, &osc.excite, &physicalControls.slap,
                                   &osc.uniMode, &osc.chord, &osc.ampEnv });
            // The string's controls that apply to this exciter flow left to
            // right over two rows on the same eight columns as the row below,
            // so no row starts with holes where hidden controls would be.
            const auto feedbackExcite = processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_excite")->load() == 10.0f;
            std::vector<juce::Component*> flow;

            for (juce::Component* control : { (juce::Component*) &osc.stringDecay, (juce::Component*) &osc.stringDamp,
                                              (juce::Component*) &osc.stringSustain, (juce::Component*) &physicalControls.stiffness,
                                              (juce::Component*) &physicalControls.pickup, (juce::Component*) &physicalControls.excitePos,
                                              (juce::Component*) &physicalControls.hardness, (juce::Component*) &physicalControls.pickPos,
                                              (juce::Component*) &physicalControls.hammer,
                                              feedbackExcite ? (juce::Component*) &physicalControls.fbGain : (juce::Component*) &physicalControls.bowPressure,
                                              feedbackExcite ? (juce::Component*) &physicalControls.fbDistance : (juce::Component*) &physicalControls.bowSpeed,
                                              (juce::Component*) &physicalControls.bridgeBuzz, (juce::Component*) &physicalControls.fretRattle,
                                              (juce::Component*) &physicalControls.couple, (juce::Component*) &physicalControls.damper,
                                              (juce::Component*) &physicalControls.registerMap })
                if (control->isVisible())
                    flow.push_back (control);

            std::vector<juce::Component*> first (8, nullptr), second (8, nullptr);

            for (size_t i = 0; i < flow.size() && i < 16; ++i)
                (i < 8 ? first[i] : second[i - 8]) = flow[i];

            layoutSlots (middleRow, first);
            layoutSlots (extraRow, second);
            layoutSlots (bottomRow, { &osc.level, &osc.pan, &osc.semi, &osc.fine,
                                      &osc.unison, &osc.detune, &osc.uniBlend, &osc.spread });
            return;
        }

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
                 &osc.grainSpread, &osc.grainLive, &osc.warp2, &osc.pdEnv, &osc.warp2Amt, &osc.pdEnvAmt, &phys.stiffness, &phys.pickup, &phys.excitePos, &phys.hardness,
                 &phys.pickPos, &phys.bowPressure, &phys.bowSpeed, &phys.bridgeBuzz, &phys.fretRattle,
                 &phys.hammer, &phys.couple, &phys.damper, &phys.registerMap, &phys.slap,
                 &phys.epDistance, &phys.epPosition, &phys.fbGain, &phys.fbDistance, &waveDisplay (i), &loadButton (i), editButtons[(size_t) i].get(),
                 removeButtons[(size_t) i].get(), bounceButtons[(size_t) i].get() };
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

            if (isFolded (i))
            {
                for (auto* component : componentsOf (i))
                    if (component != &controls[(size_t) i]->on && component != removeButtons[(size_t) i].get())
                        component->setVisible (false);

                continue;
            }

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
            // M8.5: the Feedback exciter's amp; SUSTAIN is its FEEDBACK.
            const auto feedbackExcite = stringVisible && processorRef.apvts.getRawParameterValue (prefix + "_excite")->load() == 10.0f;
            physicalControls.fbGain.setVisible (feedbackExcite);
            physicalControls.fbDistance.setVisible (feedbackExcite);
            controls[(size_t) i]->stringSustain.setLabelText (feedbackExcite ? "FEEDBACK" : "SUSTAIN");
            physicalControls.bowSpeed.setVisible (bow);
            const auto electric = stringVisible && isElectric (i);
            const auto exciteChoice = processorRef.apvts.getRawParameterValue (prefix + "_excite")->load();
            physicalControls.hammer.setVisible (electric || (stringVisible && (exciteChoice == 5.0f || exciteChoice == 9.0f)));
            physicalControls.epDistance.setVisible (electric);
            physicalControls.epPosition.setVisible (electric);
            // M8.2: the Piano exciter's hammer and strings are physical; the
            // pick, pickup and buzz controls don't apply.
            if (stringVisible && exciteChoice == 9.0f)
                for (juce::Component* control : { (juce::Component*) &physicalControls.pickup,
                                                  (juce::Component*) &physicalControls.hardness,
                                                  (juce::Component*) &physicalControls.pickPos,
                                                  (juce::Component*) &physicalControls.slap,
                                                  (juce::Component*) &physicalControls.bridgeBuzz,
                                                  (juce::Component*) &physicalControls.fretRattle })
                    control->setVisible (false);
            if (electric)
                for (juce::Component* control : { (juce::Component*) &physicalControls.stiffness,
                                                  (juce::Component*) &physicalControls.pickup,
                                                  (juce::Component*) &physicalControls.excitePos,
                                                  (juce::Component*) &physicalControls.hardness,
                                                  (juce::Component*) &physicalControls.pickPos,
                                                  (juce::Component*) &physicalControls.slap,
                                                  (juce::Component*) &physicalControls.bridgeBuzz,
                                                  (juce::Component*) &physicalControls.fretRattle,
                                                  (juce::Component*) &physicalControls.couple,
                                                  (juce::Component*) &physicalControls.registerMap })
                    control->setVisible (false);

            auto& osc = *controls[(size_t) i];
            editButtons[(size_t) i]->setVisible (mode == 0);
            osc.table.setVisible (mode == 0);
            osc.frame.setVisible (mode == 0);
            osc.excite.setVisible (mode == 1);
            osc.stringDecay.setVisible (mode == 1);
            osc.stringDamp.setVisible (mode == 1);
            osc.stringSustain.setVisible (mode == 1 && ! isElectric (i));
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
            osc.grainLive.setVisible (mode == 3 && IlanaSynthAudioProcessor::isEffectBuild);
            osc.uniBlend.setVisible (mode != 3);
            osc.spread.setVisible (mode != 3);
            // M7.5 Live: the input has no pitch, unison or table to show.
            if (mode == 4)
                for (auto* control : std::initializer_list<juce::Component*> { &osc.semi, &osc.fine, &osc.unison, &osc.detune,
                                                                               &osc.uniBlend, &osc.spread, &osc.uniMode, &osc.chord,
                                                                               &loadButton (i) })
                    control->setVisible (false);
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

    // M7.3: the Tine and Reed excites (7, 8) have their own controls.
    bool isElectric (int index) const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
        const auto excite = processorRef.apvts.getRawParameterValue (prefix + "_excite")->load();
        return excite == 7.0f || excite == 8.0f;
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
        // The sub's controls follow its switch; noise has its own level.
        const auto subIsOn = readBool ("subosc_on");
        for (auto* control : { static_cast<juce::Component*> (&subShape), static_cast<juce::Component*> (&subOctave),
                               static_cast<juce::Component*> (subOscLevel.get()) })
            if (control != nullptr && control->getAlpha() != (subIsOn ? 1.0f : 0.4f))
                control->setAlpha (subIsOn ? 1.0f : 0.4f);

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
                               &physicalControls.damper, &physicalControls.registerMap,
                               &physicalControls.epDistance, &physicalControls.epPosition }, enabled);

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
            editButtons[(size_t) index]->setEnabled (enabled);
            editButtons[(size_t) index]->setAlpha (alpha);
        }
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

public:
    // M7.4: EDIT. A user table is edited in place; a factory table is first
    // copied into a free patch table, which the oscillator then plays.
    // M8.6: the BOUNCE menu. The choices stay set for the next bounce.
    void showBounceMenu (int index)
    {
        if (bouncingOsc >= 0)
            return;
        juce::PopupMenu menu;
        menu.addSectionHeader ("Bounce the patch into OSC " + juce::String (index + 1));
        menu.addItem (1, "As a sample (tuned, one note)");
        menu.addItem (2, "As a wavetable (cut into single cycles)");
        menu.addSeparator();
        menu.addItem (3, "Include the effects", true, bounceRequest.withFx);
        menu.addItem (4, "Mute the other oscillators", true, bounceRequest.muteOthers);
        juce::PopupMenu notes, lengths;
        for (int note : { 36, 48, 60, 72 })
            notes.addItem (100 + note, juce::MidiMessage::getMidiNoteName (note, true, true, 4), true, bounceRequest.note == note);
        for (double hold : { 0.5, 1.0, 2.0, 4.0, 8.0 })
            lengths.addItem (300 + (int) (hold * 2.0), juce::String (hold, hold < 1.0 ? 1 : 0) + " s held + 2 s release",
                             true, std::abs (bounceRequest.holdSeconds - hold) < 1.0e-3);
        menu.addSubMenu ("Note", notes);
        menu.addSubMenu ("Length", lengths);

        juce::Component::SafePointer<OscPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bounceButtons[(size_t) index].get()),
                            [safe, index] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;
                                auto& request = safe->bounceRequest;
                                if (result == 1 || result == 2)
                                {
                                    request.targetOsc = index;
                                    request.toTable = result == 2;
                                    request.tailSeconds = 2.0;
                                    if (safe->processorRef.startBounce (request))
                                    {
                                        safe->bouncingOsc = index;
                                        safe->updateBounce();
                                    }
                                    return;
                                }
                                if (result == 3) request.withFx = ! request.withFx;
                                if (result == 4) request.muteOthers = ! request.muteOthers;
                                if (result >= 100 && result < 300) request.note = result - 100;
                                if (result >= 300) request.holdSeconds = (result - 300) / 2.0;
                                safe->showBounceMenu (index); // keep choosing
                            });
    }

    void updateBounce()
    {
        const auto state = processorRef.getBounceState();
        auto& button = *bounceButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, bouncingOsc)];
        if (state == IlanaSynthAudioProcessor::BounceState::Rendering)
        {
            button.setButtonText ("BOUNCING " + juce::String (juce::roundToInt (processorRef.getBounceProgress() * 100.0f)) + "%");
            if (bounceButtonWide != bouncingOsc)
            {
                bounceButtonWide = bouncingOsc;
                resized();
            }
            for (auto& other : bounceButtons)
                other->setEnabled (false);
            return;
        }
        button.setButtonText ("BOUNCE");
        for (auto& other : bounceButtons)
            other->setEnabled (true);
        bouncingOsc = -1;
        bounceButtonWide = -1;
        resized();
        const auto message = processorRef.getBounceMessage();
        if (state == IlanaSynthAudioProcessor::BounceState::Failed)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Bounce", message);
        else
            button.setTooltip (message);
        updateModeVisibility();
        updateEnabled();
    }

    void openTableEditor (int index)
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) index]) + "_table";
        const auto factoryCount = TableFactory::getNumFactoryTables();
        const auto choice = readTableChoiceIndex (id);
        auto slot = choice - factoryCount;

        if (slot < 0)
        {
            slot = processorRef.findFreeUserSlot();
            if (slot < 0)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Wavetable editor",
                                                        "All 16 patch tables are in use. Pick a User table to edit it.");
                return;
            }
            auto doc = WavetableDoc::fromFactory (choice);
            doc.name << " edit";
            processorRef.setUserTable (slot, doc);
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (factoryCount + slot)));
                parameter->endChangeGesture();
            }
        }

        if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
            editor->openWavetableEditor (slot, oscColour (index));
    }

private:
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

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, removeButtons, editButtons, bounceButtons;
    IlanaSynthAudioProcessor::BounceRequest bounceRequest;
    int bouncingOsc = -1, bounceButtonWide = -1;
    juce::TextButton addButton;
    std::unique_ptr<juce::FileChooser> tableChooser;
    std::array<std::unique_ptr<PhysicalControls>, OscillatorIds::count> physical;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<KnobControl> voiceSpread, unisonRandom, drift;
    juce::Rectangle<int> voiceStrip;
    juce::Rectangle<int> symCard;
    ToggleControl symOn, symManual;
    KnobControl symAmount, symDecay, symCount;

    // Acoustic keys (M4): soundboard, stretch tuning, pedal resonance and
    // the mechanism's noises, shared by every voice.
    juce::Rectangle<int> keysCard;
    ToggleControl sbOn;
    ComboControl sbModel;
    KnobControl sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal;
    std::array<std::unique_ptr<KnobControl>, 6> symNotes;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<KnobControl> subOscLevel, noiseStrip;
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
        IlanaTheme::paintCardTitle (g, header, title, colour);
        header.removeFromLeft (14);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));

        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true)), title);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (FilterType::getNames()[type], header.withTrimmedLeft (titleWidth + 10), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        auto header = area.removeFromTop (28);
        slope.setBounds (header.removeFromRight (96).reduced (0, 5));

        grid.setBounds (area.removeFromTop (juce::jlimit (52, 64, getHeight() / 4) + FilterTypeGrid::labelHeight));
        area.removeFromTop (4);
        area.removeFromBottom (6);

        std::vector<juce::Component*> knobs { &cutoff, &reso, &drive, &env, &key, &fm };

        if (morph.isVisible())
            knobs.push_back (&morph);

        layoutRow (area, knobs);
    }

private:
    // Only the classic models have a slope; FilterType::usesMorph says which use MORPH.
    void refreshType()
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + "_type"))
            type = juce::jlimit (0, FilterType::Count - 1, (int) value->load());

        const auto hasSlope = type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
                              && FilterType::usesSlope (type);
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

// M8.3: the WEST card: a wavefolder into a low-pass gate. It sits where
// Filter 2's card is (the FILTER 2 / WEST tabs), and runs after the filters
// or in Filter 2's place.
class WestPanel : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    explicit WestPanel (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          on (p.apvts, "west_on", "ON"),
          position (p.apvts, "west_pos", "PLACE"),
          mode (p.apvts, "west_mode", "GATE"),
          source (p.apvts, "west_src", "STRIKE BY"),
          fold (p.apvts, "west_fold", "FOLD", colour(), true),
          symmetry (p.apvts, "west_sym", "SYMMETRY", colour(), true),
          stages (p.apvts, "west_stages", "STAGES", colour(), true),
          decay (p.apvts, "west_decay", "DECAY", colour(), true),
          resonance (p.apvts, "west_res", "RESO", colour(), true),
          strike (p.apvts, "west_strike", "STRIKE", colour(), true),
          open (p.apvts, "west_open", "OPEN", colour(), true)
    {
        addAll (*this, on, position, mode, source, fold, symmetry, stages, decay, resonance, strike, open);
        startTimerHz (30);
    }

    // Not a modulation source: the accent, not a source's colour.
    static juce::Colour colour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 7.0f, colour().withAlpha (0.35f));
        auto header = getLocalBounds().reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "WEST", "wavefolder into a low-pass gate", colour());

        // The fold's transfer curve and the gate's vactrol, lit by its level.
        const auto plot = picture.toFloat();
        IlanaTheme::paintWell (g, plot, 5.0f);
        const auto curveArea = plot.withWidth (plot.getWidth() * 0.62f).reduced (8.0f, 6.0f);
        juce::Path curve;
        const auto gain = 0.35 + 11.65 * (double) (read ("west_fold") * read ("west_fold"));
        const auto bias = 0.5 * (double) read ("west_sym");
        const auto stagesNow = juce::jlimit (1, 4, (int) read ("west_stages"));
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = -1.0 + 2.0 * i / 120.0;
            auto y = x * gain + bias;
            for (int s = 0; s < stagesNow; ++s)
            {
                y = Wavefolder::fold (y);
                if (s + 1 < stagesNow)
                    y *= 1.0 + 0.35 * gain / (double) stagesNow;
            }
            const auto px = curveArea.getX() + curveArea.getWidth() * (float) i / 120.0f;
            const auto py = curveArea.getCentreY() - (float) y * curveArea.getHeight() * 0.45f;
            if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
        }
        g.setColour (colour());
        g.strokePath (curve, juce::PathStrokeType (1.6f));

        // How open the gate is right now: a level meter (a lit dot read as
        // an on/off switch).
        const auto level = juce::jlimit (0.0f, 1.0f, processorRef.getWestGateLevel());
        const auto cell = plot.withTrimmedLeft (plot.getWidth() * 0.66f).reduced (10.0f, 8.0f);
        auto meter = cell.withSizeKeepingCentre (8.0f, juce::jmin (cell.getHeight() - 20.0f, 64.0f)).withX (cell.getX() + 10.0f);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (meter, 3.0f);
        const auto lit = meter.withTop (meter.getBottom() - meter.getHeight() * level);
        if (level > 0.001f)
        {
            IlanaTheme::paintGlow (g, lit, 3.0f, colour(), 0.4f + 0.6f * level);
            g.setColour (colour());
            g.fillRoundedRectangle (lit, 3.0f);
        }
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("GATE", cell.withTrimmedLeft (26.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 0);
        area.removeFromTop (30);
        auto top = area.removeFromTop (area.getHeight() * 2 / 5);
        auto options = top.removeFromLeft (top.getWidth() / 2);
        picture = top.reduced (4, 2);
        const auto optionHeight = options.getHeight() / 2;
        auto row1 = options.removeFromTop (optionHeight);
        // Its on switch in the header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (getLocalBounds(), 14));
        position.setBounds (row1.reduced (3, 1));
        auto row2 = options;
        mode.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (3, 1));
        source.setBounds (row2.reduced (3, 1));
        area.removeFromTop (4);
        layoutRow (area, { &fold, &symmetry, &stages, &decay, &resonance, &strike, &open });
    }

private:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto active = read ("west_on") > 0.5f;
        for (juce::Component* c : { (juce::Component*) &fold, (juce::Component*) &symmetry, (juce::Component*) &stages,
                                     (juce::Component*) &decay, (juce::Component*) &resonance, (juce::Component*) &strike,
                                     (juce::Component*) &open, (juce::Component*) &mode, (juce::Component*) &source,
                                     (juce::Component*) &position })
        {
            const auto alpha = active ? 1.0f : 0.4f;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint (picture);
    }

    IlanaSynthAudioProcessor& processorRef;
    ToggleControl on;
    ComboControl position, mode, source;
    KnobControl fold, symmetry, stages, decay, resonance, strike, open;
    juce::Rectangle<int> picture;
};

// M8.5: the VECTOR page. The vector pad (four oscillators at the corners,
// moved by hand, by a path or by drift) and EVOLVE (each macro drifting
// within a range; FREEZE keeps where they are).
class VectorPage : public juce::Component,
                   private IlanaAnim::FrameTimer
{
public:
    explicit VectorPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          pad (p),
          on (p.apvts, "vec_on", "ON"),
          path (p.apvts, "vec_path", "PATH"),
          cornerA (p.apvts, "vec_a", "TOP LEFT"),
          cornerB (p.apvts, "vec_b", "TOP RIGHT"),
          cornerC (p.apvts, "vec_c", "BOTTOM LEFT"),
          cornerD (p.apvts, "vec_d", "BOTTOM RIGHT"),
          x (p.apvts, "vec_x", "X", colour(), true),
          y (p.apvts, "vec_y", "Y", colour(), true),
          rate (p.apvts, "vec_rate", "PATH RATE", colour(), true),
          drift (p.apvts, "vec_drift", "DRIFT", colour(), true),
          driftRate (p.apvts, "vec_drift_rate", "DRIFT RATE", colour(), true)
    {
        addAll (*this, pad, on, path, cornerA, cornerB, cornerC, cornerD, x, y, rate, drift, driftRate);
        path.showAsSwitch();
        // The EVOLVE and RATE names head their columns once, on the first row.
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            evolveAmount.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve", m == 0 ? "EVOLVE" : "",
                                                                    evolveColour(), true));
            evolveRate.push_back (std::make_unique<KnobControl> (p.apvts, "macro" + juce::String (m + 1) + "_evolve_rate", m == 0 ? "RATE" : "",
                                                                  evolveColour(), true));
            addAndMakeVisible (*evolveAmount.back());
            addAndMakeVisible (*evolveRate.back());
        }
        freeze.setButtonText ("FREEZE");
        freeze.setTooltip ("Keeps the macros where Evolve has taken them, and stops the drift.");
        freeze.onClick = [this] { processorRef.freezeEvolve(); };
        addAndMakeVisible (freeze);
        startTimerHz (20);
    }

    // Not modulation sources, so not in a source's colour: the accent.
    static juce::Colour colour() { return IlanaTheme::accent(); }
    static juce::Colour evolveColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintCard (g, vectorCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, evolveCard.toFloat(), 7.0f, evolveColour().withAlpha (0.35f));

        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR", "four oscillators at the corners; Vector X / Y are mod sources", colour());

        header = evolveCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "EVOLVE", "macros drift in range", evolveColour(), 110);

        // Each macro: its name, where it is set and where it has drifted to,
        // with a hairline between rows.
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            const auto row = macroRows[(size_t) m];
            if (m > 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRect (evolveCard.getX() + 12, row.getY() - 10, evolveCard.getWidth() - 24, 1);
            }
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (processorRef.getMacroName (m).toUpperCase(), row.withWidth (110).withHeight (18), juce::Justification::centredLeft);
            const auto bar = juce::Rectangle<float> ((float) row.getX(), (float) row.getY() + 24.0f, 100.0f, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (bar, 3.0f);
            const auto set = readParam ("macro" + juce::String (m + 1));
            const auto now = processorRef.macroValue (m);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRect (bar.getX() + bar.getWidth() * set - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
            g.setColour (evolveColour());
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ bar.getX() + bar.getWidth() * now, bar.getCentreY() }));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        // The pad gets most of the page: EVOLVE's rows need little width.
        vectorCard = area.removeFromLeft (area.getWidth() * 70 / 100);
        area.removeFromLeft (10);
        evolveCard = area;

        auto inner = vectorCard.reduced (12, 0);
        inner.removeFromTop (30);
        inner.removeFromBottom (12);
        // The pad fills the card's height; the controls take the width left.
        const auto controlsWidth = 250;
        const auto side = juce::jmax (200, juce::jmin (inner.getHeight(), inner.getWidth() - controlsWidth - 12));
        pad.setBounds (inner.removeFromLeft (side).withSizeKeepingCentre (side, side));
        inner.removeFromLeft (12);
        const auto controlsHeight = 40 + 44 + 44 + 6 + 112 * 2 + 18;
        inner = inner.withSizeKeepingCentre (inner.getWidth(), juce::jmin (inner.getHeight(), controlsHeight));
        // The vector's on switch in its header, like every card's.
        on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14));
        auto toggles = inner.removeFromTop (40);
        path.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2).reduced (3, 1));
        auto combos1 = inner.removeFromTop (44);
        cornerA.setBounds (combos1.removeFromLeft (combos1.getWidth() / 2).reduced (3, 1));
        cornerB.setBounds (combos1.reduced (3, 1));
        auto combos2 = inner.removeFromTop (44);
        cornerC.setBounds (combos2.removeFromLeft (combos2.getWidth() / 2).reduced (3, 1));
        cornerD.setBounds (combos2.reduced (3, 1));
        // Knob rows sized to the knobs, with a gap between, so each label
        // sits with its own knob rather than under the row above's values.
        inner.removeFromTop (6);
        const auto knobHeight = juce::jmin (112, inner.getHeight() / 2 - 8);
        layoutRow (inner.removeFromTop (knobHeight), { &x, &y, &rate });
        inner.removeFromTop (18);
        layoutRow (inner.removeFromTop (knobHeight), { &drift, &driftRate, nullptr }); // on the row above's grid

        auto rows = evolveCard.reduced (12, 0);
        rows.removeFromTop (30);
        // FREEZE is an action on the whole card: in its header, at the right.
        freeze.setBounds (evolveCard.getRight() - 12 - 96, evolveCard.getY() + 4, 96, 20);
        const auto rowHeight = rows.getHeight() / Mod::numMacros;
        for (int m = 0; m < Mod::numMacros; ++m)
        {
            auto row = rows.removeFromTop (rowHeight);
            macroRows[(size_t) m] = row.withWidth (116).withTrimmedTop (8);
            row.removeFromLeft (120);
            // A gap under each row, so a row's labels don't read as the
            // values of the row above.
            row.removeFromBottom (8);
            evolveAmount[(size_t) m]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
            evolveRate[(size_t) m]->setBounds (row.reduced (2, 0));
        }
    }

private:
    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto active = readParam ("vec_on") > 0.5f;
        for (juce::Component* c : { (juce::Component*) &path, (juce::Component*) &cornerA, (juce::Component*) &cornerB,
                                     (juce::Component*) &cornerC, (juce::Component*) &cornerD, (juce::Component*) &x,
                                     (juce::Component*) &y, (juce::Component*) &rate, (juce::Component*) &drift,
                                     (juce::Component*) &driftRate, (juce::Component*) &pad })
        {
            const auto alpha = active ? 1.0f : 0.45f;
            if (c->getAlpha() != alpha)
                c->setAlpha (alpha);
        }
        rate.setAlpha (active && readParam ("vec_path") > 0.5f ? 1.0f : 0.45f);
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint (evolveCard);
    }

    IlanaSynthAudioProcessor& processorRef;
    VectorPadDisplay pad;
    ToggleControl on, path;
    ComboControl cornerA, cornerB, cornerC, cornerD;
    KnobControl x, y, rate, drift, driftRate;
    std::vector<std::unique_ptr<KnobControl>> evolveAmount, evolveRate;
    juce::TextButton freeze;
    juce::Rectangle<int> vectorCard, evolveCard;
    std::array<juce::Rectangle<int>, Mod::numMacros> macroRows;
};

// M8.7: the PHYSICAL page. The animated string, its exciter and the body
// for one oscillator (the first in Physical mode unless another is picked),
// with that oscillator's string controls and the shared body and
// soundboard switches beside it.
class PhysicalPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit PhysicalPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p),
          resOn (p.apvts, "res_on", "BODY"),
          bodyType (p.apvts, "body_type", "BODY TYPE"),
          sbOn (p.apvts, "sb_on", "SOUNDBOARD"),
          sbModel (p.apvts, "sb_model", "BOARD MODEL")
    {
        addAll (*this, view, resOn, bodyType, sbOn, sbModel);
        // Two named switches side by side (the body and the soundboard are
        // separate), rather than one card switch that reads as "all off".
        resOn.showAsSwitch();
        sbOn.showAsSwitch();
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto& button = oscButtons[(size_t) i];
            button.setButtonText ("OSC " + juce::String (i + 1));
            button.setClickingTogglesState (false);
            IlanaTheme::makePill (button, IlanaTheme::oscColour (i));
            button.onClick = [this, i] { choose (i, true); };
            addAndMakeVisible (button);
        }
        makePhysical.setButtonText ("SWITCH TO PHYSICAL");
        makePhysical.setTooltip ("Puts this oscillator in Physical mode.");
        makePhysical.onClick = [this]
        {
            if (auto* parameter = processorRef.apvts.getParameter (prefix() + "_mode"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
        };
        addChildComponent (makePhysical);
        choose (firstPhysical(), false);
        startTimerHz (5);
    }

    // The page belongs to the chosen oscillator: its identity colour.
    juce::Colour colour() const { return IlanaTheme::oscColour (chosen); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        const auto title = [&g] (juce::Rectangle<int> card, const juce::String& name, const juce::String& note, juce::Colour tag)
        {
            IlanaTheme::paintCardHeader (g, card.reduced (12, 0).removeFromTop (28), name, note, tag);
        };

        // Not a Physical oscillator: one centred card that says so and
        // offers the switch, rather than an empty picture beside it.
        if (! isPhysical (chosen))
        {
            IlanaTheme::paintCard (g, emptyCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
            title (emptyCard, "PHYSICAL", "a string, what excites it and its body", colour());
            static const char* const plays[] { "a wavetable", "a string", "a sample", "grains", "the live input" };
            const auto mode = juce::jlimit (0, 4, juce::roundToInt (readParam (prefix() + "_mode")));
            auto message = makePhysical.getBounds().withHeight (44).translated (0, -58).withWidth (emptyCard.getWidth() - 28)
                                                  .withX (emptyCard.getX() + 14);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (chosen + 1) + " plays " + plays[mode] + ", so it has no string.",
                        message.removeFromTop (22), juce::Justification::centred);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("Switch it to Physical to edit its string, exciter and body here.", message, juce::Justification::centred);
            return;
        }

        IlanaTheme::paintCard (g, viewCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, stringCard.toFloat(), 7.0f, colour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, bodyCard.toFloat(), 7.0f, colour().withAlpha (0.25f));
        title (viewCard, "PHYSICAL", "the string, what excites it and the body, from OSC " + juce::String (chosen + 1) + "'s settings", colour());
        title (stringCard, "OSC " + juce::String (chosen + 1) + " STRING", "", colour());
        title (bodyCard, "BODY", "a body and a soundboard for every string", colour());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        const auto physical = isPhysical (chosen);

        for (juce::Component* c : { (juce::Component*) &resOn, (juce::Component*) &bodyType,
                                    (juce::Component*) &sbOn, (juce::Component*) &sbModel })
            c->setVisible (physical);

        // Not physical: the view still shows, in its own preview look, what
        // the switch gives (drawn from the oscillator's string settings).
        view.setInterceptsMouseClicks (physical, physical);

        if (! physical)
        {
            emptyCard = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 900), juce::jmin (area.getHeight(), 600));
            auto inner = emptyCard.reduced (14, 0);
            inner.removeFromTop (38);
            auto shownButtons = 0;
            for (auto& button : oscButtons)
                shownButtons += button.isVisible() ? 1 : 0;
            auto picker = inner.removeFromTop (30).withSizeKeepingCentre (juce::jmax (1, shownButtons) * 96, 30);
            for (auto& button : oscButtons)
                if (button.isVisible())
                    button.setBounds (picker.removeFromLeft (96).reduced (3, 3));
            makePhysical.setBounds (juce::Rectangle<int> (240, 34).withCentre ({ emptyCard.getCentreX(), emptyCard.getBottom() - 40 }));
            inner.removeFromTop (6);
            inner.removeFromBottom (130); // the message and the switch
            view.setBounds (inner);
            return;
        }

        emptyCard = {};
        viewCard = area.removeFromLeft (area.getWidth() * 60 / 100);
        area.removeFromLeft (10);
        bodyCard = area.removeFromBottom (juce::jmin (150, area.getHeight() / 3));
        area.removeFromBottom (10);
        stringCard = area;

        auto inner = viewCard.reduced (10, 0);
        inner.removeFromTop (30);
        auto picker = inner.removeFromTop (30);
        const auto buttonWidth = picker.getWidth() / OscillatorIds::count;
        for (auto& button : oscButtons)
            button.setBounds (picker.removeFromLeft (buttonWidth).reduced (3, 3));
        inner.removeFromTop (6);
        view.setBounds (inner.withTrimmedBottom (10));

        auto controls = stringCard.reduced (10, 0);
        controls.removeFromTop (30);
        makePhysical.setBounds (juce::Rectangle<int> (220, 34).withCentre (controls.getCentre().translated (0, 20)));
        if (excite != nullptr)
        {
            excite->setBounds (controls.removeFromTop (44).reduced (3, 1));
            controls.removeFromTop (4);
            const auto rows = (int) (knobs.size() + 3) / 4;
            const auto rowHeight = juce::jmin (120, controls.getHeight() / juce::jmax (1, rows));
            for (int row = 0; row < rows; ++row)
            {
                std::vector<juce::Component*> items;
                for (size_t k = (size_t) row * 4; k < juce::jmin (knobs.size(), (size_t) row * 4 + 4); ++k)
                    items.push_back (knobs[k].get());
                while (items.size() < 4)
                    items.push_back (nullptr);
                auto rowArea = controls.removeFromTop (rowHeight);
                const auto cell = rowArea.getWidth() / 4;
                for (auto* item : items)
                {
                    auto slot = rowArea.removeFromLeft (cell);
                    if (item != nullptr)
                        item->setBounds (slot.reduced (2, 6));
                }
            }
        }

        // The body's switch in its header; its type, and the soundboard's
        // switch and model, in one row under it.
        auto body = bodyCard.reduced (10, 0).withTrimmedTop (30).withTrimmedBottom (6);
        layoutRow (body, { &resOn, &bodyType, &sbOn, &sbModel });
    }

    int getChosenOscillator() const { return chosen; }

private:
    juce::String prefix() const { return OscillatorIds::prefixes[(size_t) chosen]; }

    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool isPhysical (int osc) const
    {
        return juce::roundToInt (readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode")) == 1;
    }

    int firstPhysical() const
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (isPhysical (i) && processorRef.isOscillatorShown (i))
                return i;
        return 0;
    }

    void choose (int osc, bool byHand)
    {
        chosen = juce::jlimit (0, OscillatorIds::count - 1, osc);
        pickedByHand = pickedByHand || byHand;
        const auto id = prefix();
        view.setOscillator (id);
        view.setColour (colour());

        // The string's controls, rebound to the chosen oscillator.
        knobs.clear();
        if (excite == nullptr || excitePrefix != id) // not while its own menu may be calling back
        {
            excite = std::make_unique<ComboControl> (processorRef.apvts, id + "_excite", "EXCITE");
            addAndMakeVisible (*excite);
            excitePrefix = id;
        }
        // The knobs this exciter uses (as on the OSC card).
        shownExcite = juce::roundToInt (readParam (id + "_excite"));
        const auto hammer = shownExcite == 5 || shownExcite == 9, feedback = shownExcite == 10, bow = shownExcite == 4;
        std::vector<std::pair<const char*, const char*>> knobIds {
            { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_string_sustain", feedback ? "FEEDBACK" : "SUSTAIN" },
            { "_string_stiffness", "STIFF" }, { "_string_excite_pos", "EXCITE POS" }, { "_string_pickup", "PICKUP" } };
        if (hammer)
            knobIds.push_back ({ "_hammer_hard", "HAMMER" });
        else if (bow)
            knobIds.insert (knobIds.end(), { { "_bow_pressure", "PRESSURE" }, { "_bow_speed", "SPEED" } });
        else if (feedback)
            knobIds.insert (knobIds.end(), { { "_fb_gain", "AMP GAIN" }, { "_fb_distance", "DISTANCE" } });
        else
            knobIds.insert (knobIds.end(), { { "_string_pick_hardness", "HARDNESS" }, { "_bridge_buzz", "BUZZ" } });
        for (const auto& [suffix, label] : knobIds)
            if (processorRef.apvts.getParameter (id + suffix) != nullptr)
            {
                knobs.push_back (std::make_unique<KnobControl> (processorRef.apvts, id + suffix, label, colour(), false));
                addAndMakeVisible (*knobs.back());
            }
        for (int i = 0; i < OscillatorIds::count; ++i)
            oscButtons[(size_t) i].setToggleState (i == chosen, juce::dontSendNotification);
        updateAvailability();
        resized();
        repaint();
    }

    void updateAvailability()
    {
        const auto physical = isPhysical (chosen);
        makePhysical.setVisible (! physical);
        if (excite != nullptr)
            excite->setVisible (physical);
        for (auto& knob : knobs)
            knob->setVisible (physical);
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto& button = oscButtons[(size_t) i];
            button.setVisible (processorRef.isOscillatorShown (i));
            button.setAlpha (isPhysical (i) ? 1.0f : 0.5f);
        }
        const auto resonator = readParam ("res_on") > 0.5f;
        bodyType.setAlpha (resonator ? 1.0f : 0.45f);
        sbModel.setAlpha (readParam ("sb_on") > 0.5f ? 1.0f : 0.45f);
    }

    void timerCallback() override
    {
        // Follow the patch: a preset with its Physical string on another
        // oscillator moves the view there, unless one was picked by hand.
        if (! pickedByHand && ! isPhysical (chosen) && isPhysical (firstPhysical()))
            choose (firstPhysical(), false);
        else if (juce::roundToInt (readParam (prefix() + "_excite")) != shownExcite)
            choose (chosen, false);
        if (lastPhysical != isPhysical (chosen))
        {
            lastPhysical = isPhysical (chosen);
            resized();
            repaint();
        }
        updateAvailability();
    }

    IlanaSynthAudioProcessor& processorRef;
    PhysicalView view;
    ToggleControl resOn;
    ComboControl bodyType;
    ToggleControl sbOn;
    ComboControl sbModel;
    std::array<juce::TextButton, OscillatorIds::count> oscButtons;
    juce::TextButton makePhysical;
    std::unique_ptr<ComboControl> excite;
    juce::String excitePrefix;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    int chosen = 0, shownExcite = -1;
    bool pickedByHand = false, lastPhysical = false;
    juce::Rectangle<int> emptyCard, viewCard, stringCard, bodyCard;
};

class FilterPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit FilterPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          filterDisplay (p),
          panel1 (p, 1, juce::Colour (0xffc86bff)),
          panel2 (p, 2, juce::Colour (0xff8f9dff)),
          westPanel (p),
          secondTabs ({ "FILTER 2", "WEST" }, { juce::Colour (0xff8f9dff), WestPanel::colour() }, false),
          flow (p),
          balance (p.apvts, "filter_balance", "BALANCE", IlanaTheme::accent(), true),
          resOn (p.apvts, "res_on", "ON"),
          resAmount (p.apvts, "res_amount", "AMOUNT", resonatorColour(), true),
          resDecay (p.apvts, "res_decay", "DECAY", resonatorColour(), true),
          resOffset (p.apvts, "res_offset", "OFFSET", resonatorColour(), true),
          resKeytrack (p.apvts, "res_keytrack", "KEY TRK", resonatorColour(), true),
          bodyType (p.apvts, "body_type", "BODY"),
          bodyMaterial (p.apvts, "body_material", "MATERIAL", resonatorColour(), true),
          bodySize (p.apvts, "body_size", "SIZE", resonatorColour(), true),
          bodyCouplingMode (p.apvts, "body_coupling_mode", "COUPLING"),
          bodyCoupling (p.apvts, "body_coupling", "COUPLE", resonatorColour(), true)
    {
        addChildComponent (westPanel);
        addAndMakeVisible (secondTabs);
        secondTabs.onSelect = [this] (int index)
        {
            panel2.setVisible (index == 0);
            westPanel.setVisible (index == 1);
        };
        // Open on WEST when a patch uses it in Filter 2's place.
        if (const auto* west = p.apvts.getRawParameterValue ("west_on"); west != nullptr && west->load() > 0.5f)
            secondTabs.setSelected (1, false);
        addAll (*this, filterDisplay, panel1, panel2, flow, balance,
                resOn, resAmount, resDecay, resOffset, resKeytrack,
                bodyType, bodyMaterial, bodySize, bodyCouplingMode, bodyCoupling);
        startTimerHz (8);
    }

    // Sections that aren't modulation sources take the accent, so a source's
    // colour always means that source.
    static juce::Colour resonatorColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintSectionTitle (g, "RESPONSE", { headingX, 12, 700, headingHeight }, "drag the markers to set cutoff and resonance");

        // Signal flow: a card like BODY beside it, the diagram and the
        // BALANCE knob inside; the subtitle says what BALANCE does now.
        {
            const auto* parallel = processorRef.apvts.getRawParameterValue ("filters_parallel");
            const auto active = parallel != nullptr && parallel->load() > 0.5f;
            IlanaTheme::paintCard (g, flowCard.toFloat(), 7.0f, IlanaTheme::Ui::text2.withAlpha (0.2f));
            IlanaTheme::paintCardHeader (g, flowCard.reduced (12, 0).removeFromTop (26), "SIGNAL FLOW",
                                         active ? "parallel: BALANCE mixes F1 and F2" : "serial: F1 into F2 (BALANCE is for parallel)",
                                         IlanaTheme::Ui::text2, 0);
        }

        IlanaTheme::paintCard (g, resonatorCard.toFloat(), 7.0f, resonatorColour().withAlpha (0.35f));
        IlanaTheme::paintCardTitle (g, resonatorCard.reduced (12, 0).removeFromTop (26), "BODY", resonatorColour());
        // The subtitle follows the title; the switch has the right of the header.
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("oscillator mix excites the body", resonatorCard.reduced (12, 0).removeFromTop (26).withTrimmedLeft (78),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (headingHeight);

        const auto panelHeight = juce::jlimit (200, 260, area.getHeight() * 9 / 20);
        const auto bottomHeight = juce::jlimit (160, 180, area.getHeight() / 4);
        const auto displayHeight = juce::jmax (90, area.getHeight() - panelHeight - bottomHeight - 16);

        filterDisplay.setBounds (area.removeFromTop (displayHeight));
        area.removeFromTop (8);

        auto panels = area.removeFromTop (panelHeight);
        panel1.setBounds (panels.removeFromLeft ((panels.getWidth() - 10) / 2));
        panels.removeFromLeft (10);
        panel2.setBounds (panels);
        westPanel.setBounds (panels);
        // The FILTER 2 / WEST tabs, top right of that card, left of the
        // slope switch (96 px from the right).
        const auto tabWidth = secondTabs.getIdealWidth();
        secondTabs.setBounds (panels.getRight() - tabWidth - 118, panels.getY() + 5, tabWidth, 18);
        panel2.setVisible (secondTabs.getSelected() == 0);
        westPanel.setVisible (secondTabs.getSelected() == 1);
        secondTabs.toFront (false);

        area.removeFromTop (8);
        auto bottom = area.removeFromTop (bottomHeight);

        // A card level with BODY's, so the titles line up.
        flowCard = bottom.removeFromLeft (bottom.getWidth() / 2 - 5);
        auto flowArea = flowCard.reduced (8, 0).withTrimmedTop (26).withTrimmedBottom (8);
        auto balanceArea = flowArea.removeFromRight (92);
        const auto balanceHeight = preferredControlHeight (&balance, balanceArea.getWidth() - 6);
        balance.setBounds (balanceArea.withSizeKeepingCentre (balanceArea.getWidth(), juce::jmin (balanceArea.getHeight(), balanceHeight)));
        flowArea.removeFromRight (6);
        flow.setBounds (flowArea);

        bottom.removeFromLeft (10);
        resonatorCard = bottom;
        auto resArea = bottom.reduced (8, 0);
        // The on switch at the right of the header, as on the oscillator cards.
        resOn.setBounds (IlanaTheme::cardSwitchBounds (resonatorCard, resonatorCard.getY() + 13));
        resArea.removeFromTop (26);
        resArea.removeFromBottom (4);
        // Menus stacked on the left, the first label on the knobs' label line.
        auto menus = resArea.removeFromLeft (juce::jmin (150, resArea.getWidth() / 5));
        constexpr int menuHeight = 13 + 24 + 6;
        const auto knobBand = preferredControlHeight (&resAmount, resArea.getWidth() / 7 - 6) + 6;
        menus = menus.withTop (resArea.withSizeKeepingCentre (resArea.getWidth(), juce::jmin (resArea.getHeight(), knobBand)).getY() + 3)
                     .withHeight (menuHeight * 2);
        bodyType.setBounds (menus.removeFromTop (menuHeight).reduced (3, 0).withTrimmedBottom (6));
        bodyCouplingMode.setBounds (menus.reduced (3, 0).withTrimmedBottom (6));
        layoutRow (resArea, { &resAmount, &resDecay, &bodyMaterial, &bodySize, &resOffset, &resKeytrack, &bodyCoupling });
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
            repaint (flowCard);
        }

        fade (balance, parallelNow);

        const auto resonating = read ("res_on");

        // MATERIAL and SIZE shape the modal bodies only; Classic is the old comb bank.
        // String-to-string coupling works without the body, the other modes need one.
        const auto* type = processorRef.apvts.getRawParameterValue ("body_type");
        const auto* coupling = processorRef.apvts.getRawParameterValue ("body_coupling_mode");
        const auto modal = resonating && type != nullptr && type->load() > 0.5f;
        const auto couplingMode = coupling != nullptr ? (int) coupling->load() : 0;

        for (auto* knob : { &resAmount, &resDecay, &resOffset, &resKeytrack })
            fade (*knob, resonating);
        fade (bodyType, resonating);
        fade (bodyMaterial, modal);
        fade (bodySize, modal);
        // Without the body only Strings coupling does anything, so the menu
        // is dimmed like the rest unless that is what it holds.
        fade (bodyCouplingMode, resonating || couplingMode == 3);
        fade (bodyCoupling, couplingMode == 3 || (couplingMode != 0 && modal));
    }

    IlanaSynthAudioProcessor& processorRef;
    FilterDisplay filterDisplay;
    FilterPanel panel1, panel2;
    WestPanel westPanel;
    CardTabs secondTabs;
    SignalFlow flow;
    KnobControl balance;
    bool wasParallel = false;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    ComboControl bodyType, bodyCouplingMode;
    KnobControl bodyMaterial, bodySize, bodyCoupling;
    juce::Rectangle<int> flowCard, resonatorCard;
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
              std::vector<EnvThumbBar::Env> envs { EnvThumbBar::Env { "AMP ENV", "amp", Mod::Source::AmpEnv, modSourceColour ((int) Mod::Source::AmpEnv) },
                       EnvThumbBar::Env { "FILT ENV", "fe", Mod::Source::FilterEnv, juce::Colour (0xffc86bff) },
                       EnvThumbBar::Env { "FILT 2 ENV", "f2e", Mod::Source::FilterEnv2, juce::Colour (0xff8f9dff) },
                       EnvThumbBar::Env { "MOD ENV", "me", Mod::Source::ModEnv, juce::Colour (0xff8fff3b) },
                       EnvThumbBar::Env { "ENV 5", "e4", Mod::Source::Env4, juce::Colour (0xff5b8cff) } };
              for (int env = 6; env <= 16; ++env)
                  envs.push_back ({ "ENV " + juce::String (env), "env" + juce::String (env),
                                    (Mod::Source) ((int) Mod::Source::Env6 + env - 6), extraColour (env) });
              return envs;
          }()),
          ampDisplay (p, "amp", modSourceColour ((int) Mod::Source::AmpEnv), false),
          feDisplay (p, "fe", juce::Colour (0xffc86bff)),
          f2eDisplay (p, "f2e", juce::Colour (0xff8f9dff)),
          meDisplay (p, "me", juce::Colour (0xff8fff3b)),
          e4Display (p, "e4", juce::Colour (0xff5b8cff)),
          ampA (p.apvts, "amp_attack", "ATTACK", modSourceColour ((int) Mod::Source::AmpEnv), false), ampD (p.apvts, "amp_decay", "DECAY", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampS (p.apvts, "amp_sustain", "SUSTAIN", modSourceColour ((int) Mod::Source::AmpEnv), false), ampR (p.apvts, "amp_release", "RELEASE", modSourceColour ((int) Mod::Source::AmpEnv), false),
          ampVel (p.apvts, "amp_velocity", "VEL", modSourceColour ((int) Mod::Source::AmpEnv), false), ampCurve (p.apvts, "amp_curve", "TENSION", modSourceColour ((int) Mod::Source::AmpEnv), false),
          feA (p.apvts, "fe_attack", "ATTACK"), feD (p.apvts, "fe_decay", "DECAY"),
          feS (p.apvts, "fe_sustain", "SUSTAIN"), feR (p.apvts, "fe_release", "RELEASE"),
          feVel (p.apvts, "filter_velocity", "VEL"), feCurve (p.apvts, "fe_curve", "TENSION", juce::Colour (0xffc86bff), false),
          f2A (p.apvts, "f2e_attack", "ATTACK"), f2D (p.apvts, "f2e_decay", "DECAY"),
          f2S (p.apvts, "f2e_sustain", "SUSTAIN"), f2R (p.apvts, "f2e_release", "RELEASE"),
          f2Vel (p.apvts, "f2e_velocity", "VEL", juce::Colour (0xff8f9dff), false),
          f2Curve (p.apvts, "f2e_curve", "TENSION", juce::Colour (0xff8f9dff), false),
          meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
          meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
          meVel (p.apvts, "me_velocity", "VEL", juce::Colour (0xff8fff3b), false),
          meCurve (p.apvts, "me_curve", "TENSION", juce::Colour (0xff8fff3b), false),
          e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
          e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
          e4Vel (p.apvts, "e4_velocity", "VEL", juce::Colour (0xff5b8cff), false),
          e4Curve (p.apvts, "e4_curve", "TENSION", juce::Colour (0xff5b8cff), false)
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
            const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), juce::Colour (0xffc86bff), juce::Colour (0xff8f9dff),
                                           juce::Colour (0xff8fff3b), juce::Colour (0xff5b8cff) };

            for (int env = 0; env < 5; ++env)
                addStageTwoKnobs (p, prefixes[env], colours[env], units[(size_t) env], false);
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
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (area.getWidth() * 47 / 100 /* the LFO display above splits at the same place */).reduced (2));
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
        // Two rows when both fit full-size knobs; otherwise one row of all
        // of them, so the dials stay as big as the LFO's rather than
        // shrinking to fit two short rows.
        constexpr int fullRow = 13 + 58 + 16 + 6;

        if (second.empty() || inner.getHeight() < fullRow * 2)
        {
            std::vector<juce::Component*> all (first);
            all.insert (all.end(), second.begin(), second.end());
            layoutRow (inner, all);
            return;
        }

        auto rows = inner.withSizeKeepingCentre (inner.getWidth(), fullRow * 2);
        layoutRow (rows.removeFromTop (fullRow), first);
        layoutRow (rows.withWidth (rows.getWidth() * (int) second.size() / 6), second);
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const juce::Colour colours[] { modSourceColour ((int) Mod::Source::AmpEnv), juce::Colour (0xffc86bff), juce::Colour (0xff8f9dff),
                                       juce::Colour (0xff8fff3b), juce::Colour (0xff5b8cff) };
        const juce::StringArray titles { "AMP ENV", "FILT ENV", "FILT 2 ENV", "MOD ENV", "ENV 5" };
        const auto index = juce::jlimit (0, 4, selected);
        const auto colour = selected < 5 ? colours[index] : extraColour (selected + 1);

        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));
        auto header = panel.reduced (12, 0).withHeight (26);
        IlanaTheme::paintCardHeader (g, header, selected < 5 ? titles[index] : "ENV " + juce::String (selected + 1),
                                     "drag the graph or the knobs", colour);
    }

private:
    // ENV 6-16 in their mod source colours.
    static juce::Colour extraColour (int env)
    {
        return modSourceColour ((int) Mod::Source::Env6 + env - 6);
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
                   private IlanaAnim::FrameTimer
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
            auto display = std::make_unique<LfoDisplay> (p, lfo, lfoColour (lfo), false);
            addAndMakeVisible (*display);
            displays.push_back (std::move (display));

            auto controls = std::make_unique<Controls> (p.apvts, lfo + 1, lfoColour (lfo), false);
            addAll (*this, controls->shape, controls->rate, controls->sync, controls->div, controls->retrig, controls->key,
                    controls->phase, controls->physA, controls->physB, controls->kick);
            addAll (*this, controls->smooth, controls->stereo, controls->seed, controls->trigger, controls->axis, controls->loop);
            for (auto& knob : controls->sim)
                addChildComponent (*knob);
            addChildComponent (controls->fire);
            controls->fire.onClick = [this, lfo] { fire (lfo); };
            groupShapeMenu (controls->shape.getComboBox());
            controls->shape.getComboBox().onChange = [this, lfo] { if (userPickingShape) shapePicked (lfo); };
            controls->shape.setPopupOverride ([this, lfo]
            {
                auto& combo = controlsList[(size_t) lfo]->shape.getComboBox();
                juce::PopupMenu menu (*combo.getRootMenu());
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&combo).withMinimumWidth (combo.getWidth())
                                        .withItemThatMustBeVisible (combo.getSelectedId()),
                                    [this, lfo] (int id)
                                    {
                                        if (id <= 0)
                                            return;
                                        userPickingShape = true;
                                        controlsList[(size_t) lfo]->shape.getComboBox().setSelectedId (id, juce::sendNotificationSync);
                                        userPickingShape = false;
                                    });
            });
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
        {
            processorRef.apvts.removeParameterListener ("lfo" + juce::String (lfo) + "_shape", this);
            // Let go of a FIRE pressed just before closing (its release timer
            // won't run once this is gone, and FIRE only acts on a press).
            if (auto* fireParameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo) + "_fire"))
                if (fireParameter->getValue() > 0.5f)
                    fireParameter->setValueNotifyingHost (0.0f);
        }
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
        const auto shape = (int) processorRef.apvts.getRawParameterValue ("lfo" + juce::String (displayIndex + 1) + "_shape")->load();
        const auto simulated = LfoSimShapes::isSim (shape);
        displays[(size_t) displayIndex]->setBounds (area.removeFromLeft (area.getWidth() * 47 / 100).reduced (2)); // the same split for every shape (and as the envelopes below)
        area.removeFromLeft (8);

        // Control panel: options across the top, knobs underneath.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        auto& c = *controlsList[(size_t) displayIndex];

        if (simulated)
        {
            layoutSimulated (c, inner, shape);
            return;
        }

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
        if (LfoShapes::isPhysics (shape))
        {
            auto top = inner.removeFromTop (inner.getHeight() / 2);
            c.rate.setBounds (top.removeFromLeft (top.getWidth() / 3).reduced (2));
            c.phase.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (2));
            c.smooth.setBounds (top.reduced (2));
            c.physA.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (2));
            c.physB.setBounds (inner.reduced (2));
        }
        else
        {
            c.rate.setBounds (inner.removeFromLeft (inner.getWidth() / 3).reduced (3, 0));
            c.phase.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (3, 0));
            c.smooth.setBounds (inner.reduced (3, 0));
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const auto colour = lfoColour (selected);
        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colour.withAlpha (0.35f));

        auto header = panel.reduced (12, 0).withHeight (26);
        const auto* retrig = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_retrig");
        const auto* key = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_key");
        IlanaTheme::paintCardHeader (g, header, "LFO " + juce::String (selected + 1),
                                     key != nullptr && key->load() > 0.5f         ? "per voice, rate follows the note (4 Hz = its pitch)"
                                     : retrig != nullptr && retrig->load() > 0.5f ? "runs per voice, restarts on each note"
                                                                                  : "free-running, shared by all voices",
                                     colour, 0);
    }

    static juce::Colour lfoColour (int index)
    {
        return IlanaSynthAudioProcessor::lfoColour (index);
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
              , smooth (state, "lfo" + juce::String (lfo) + "_smooth", "SMOOTH", accent, followsTheme)
              , stereo (state, "lfo" + juce::String (lfo) + "_stereo", "STEREO", accent, followsTheme)
              , seed (state, "lfo" + juce::String (lfo) + "_seed", "SEED", accent, followsTheme)
              , trigger (state, "lfo" + juce::String (lfo) + "_trigger", "TRIGGER")
              , axis (state, "lfo" + juce::String (lfo) + "_axis", "OUTPUT A")
              , loop (state, "lfo" + juce::String (lfo) + "_loop", "LOOP")
        {
            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                sim.push_back (std::make_unique<KnobControl> (state, "lfo" + juce::String (lfo) + "_p" + juce::String (param + 1),
                                                              "P" + juce::String (param + 1), accent, followsTheme));
            fire.setButtonText ("FIRE");
            fire.setTooltip ("Triggers the LFO now: drops the ball, plucks the spring, restarts a seeded sequence.");
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
        // M8.1
        KnobControl smooth, stereo, seed;
        ComboControl trigger, axis;
        ToggleControl loop;
        juce::TextButton fire;
        std::vector<std::unique_ptr<KnobControl>> sim;
        int labelledShape = -1;
    };

    // The shape menu with section headings. The attachment maps menu
    // positions to the parameter, so the items stay in parameter order.
    static void groupShapeMenu (juce::ComboBox& combo)
    {
        juce::StringArray names;
        for (int i = 0; i < combo.getNumItems(); ++i)
            names.add (combo.getItemText (i));
        const auto selected = combo.getSelectedId();
        combo.clear (juce::dontSendNotification);
        const auto add = [&] (const juce::String& heading, int first, int last)
        {
            combo.addSectionHeading (heading);
            for (int i = first; i <= last && i < names.size(); ++i)
                combo.addItem (names[i], i + 1);
        };
        add ("Waves", 0, LfoShapes::SmoothRandom - 1);
        add ("Classic (M2)", LfoShapes::SmoothRandom, LfoShapes::Friction);
        add ("Random", LfoSimShapes::RandomHold, LfoSimShapes::DrunkWalk);
        add ("Chaos", LfoSimShapes::Lorenz, LfoSimShapes::DoublePendulum);
        add ("Physics", LfoSimShapes::Bounce, LfoSimShapes::Friction);
        combo.setSelectedId (selected, juce::dontSendNotification);
    }

    // Choosing a simulated shape from the menu loads its knobs' defaults
    // (presets and automation keep whatever they set).
    void shapePicked (int lfo)
    {
        const auto shape = controlsList[(size_t) lfo]->shape.getComboBox().getSelectedId() - 1;
        if (! LfoSimShapes::isSim (shape))
            return;
        const auto& info = LfoSimInfo::get (shape);
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_p" + juce::String (param + 1)))
                parameter->setValueNotifyingHost (info.params[(size_t) param].defaultValue);
    }

    // FIRE: a momentary press of the LFO's fire parameter.
    void fire (int lfo)
    {
        if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_fire"))
        {
            parameter->setValueNotifyingHost (1.0f);
            // Through a SafePointer: closing the plugin within the 60 ms would
            // otherwise leave this writing to a deleted parameter.
            juce::Component::SafePointer<juce::Component> safeThis (this);
            juce::Timer::callAfterDelay (60, [safeThis, parameter]
            {
                if (safeThis != nullptr)
                    parameter->setValueNotifyingHost (0.0f);
            });
        }
        displays[(size_t) lfo]->triggerPreview();
    }

    // Names and value text of a simulated shape's knobs.
    void labelSimulated (Controls& c, int shape)
    {
        if (c.labelledShape == shape)
            return;
        c.labelledShape = shape;
        const auto& info = LfoSimInfo::get (shape);
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
        {
            auto& knob = *c.sim[(size_t) param];
            if (info.params[(size_t) param].name != nullptr)
                knob.setLabelText (info.params[(size_t) param].name);
            knob.getSlider().textFromValueFunction = [shape, param] (double value) { return LfoSimInfo::text (shape, param, (float) value); };
            knob.getSlider().updateText();
        }
    }

    // Simulated shapes: combos and switches on two rows, then a row of
    // named knobs (RATE, SMOOTH, the shape's own, STEREO and SEED).
    void layoutSimulated (Controls& c, juce::Rectangle<int> inner, int shape)
    {
        const auto& info = LfoSimInfo::get (shape);
        labelSimulated (c, shape);

        // Options on the left: SHAPE, then TRIGGER / DIVISION / OUTPUT, then
        // the switches and FIRE.
        auto options = inner.removeFromLeft (inner.getWidth() * 45 / 100);
        inner.removeFromLeft (6);
        const auto rowHeight = options.getHeight() / 3;
        c.shape.setBounds (options.removeFromTop (rowHeight).reduced (3, 1));
        auto combos = options.removeFromTop (rowHeight);
        const auto comboWidth = combos.getWidth() / (info.usesAxis ? 3 : 2);
        c.trigger.setBounds (combos.removeFromLeft (comboWidth).reduced (3, 1));
        c.div.setBounds (combos.removeFromLeft (comboWidth).reduced (3, 1));
        if (info.usesAxis)
            c.axis.setBounds (combos.reduced (3, 1));

        std::vector<juce::Component*> row { &c.sync, &c.retrig, &c.key };
        if (info.usesLoop)
            row.push_back (&c.loop);
        if (shape == LfoSimShapes::Pendulum)
            row.push_back (&c.kick);
        const auto toggleWidth = options.getWidth() / ((int) row.size() + 1);
        for (auto* component : row)
            component->setBounds (options.removeFromLeft (toggleWidth).reduced (2, 1));
        c.fire.setBounds (options.reduced (2, 1).withTrimmedTop (13).withHeight (juce::jmin (24, juce::jmax (16, options.getHeight() - 14))));

        // Knobs on the right, two rows of four.
        std::vector<juce::Component*> knobs { &c.rate, &c.smooth };
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            if (info.params[(size_t) param].name != nullptr)
                knobs.push_back (c.sim[(size_t) param].get());
        if (info.usesStereo)
            knobs.push_back (&c.stereo);
        if (info.usesSeed)
            knobs.push_back (&c.seed);
        // Two rows on one grid (a gap between them, so the second row's
        // names don't read as the first row's values).
        const auto perRow = (size_t) juce::jmax (3, ((int) knobs.size() + 1) / 2);
        constexpr int rowGap = 8;
        const auto knobHeight = (inner.getHeight() - rowGap) / 2;
        std::vector<juce::Component*> first (perRow, nullptr), second (perRow, nullptr);
        for (size_t k = 0; k < knobs.size() && k < perRow * 2; ++k)
            (k < perRow ? first[k] : second[k - perRow]) = knobs[k];
        layoutRow (inner.removeFromTop (knobHeight), first);
        inner.removeFromTop (rowGap);
        layoutRow (inner.removeFromTop (knobHeight), second);
    }

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
            const auto simulated = LfoSimShapes::isSim (shape);
            const auto& info = LfoSimInfo::get (simulated ? shape : LfoSimShapes::RandomHold);
            c.phase.setVisible (visible && ! simulated);
            c.physA.setVisible (visible && LfoShapes::isPhysics (shape));
            c.physB.setVisible (visible && LfoShapes::isPhysics (shape));
            c.kick.setVisible (visible && (shape == LfoShapes::Pendulum || shape == LfoSimShapes::Pendulum));
            c.smooth.setVisible (visible);
            c.trigger.setVisible (visible && simulated);
            c.axis.setVisible (visible && simulated && info.usesAxis);
            c.loop.setVisible (visible && simulated && info.usesLoop);
            c.stereo.setVisible (visible && simulated && info.usesStereo);
            c.seed.setVisible (visible && simulated && info.usesSeed);
            c.fire.setVisible (visible && simulated);
            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                c.sim[(size_t) param]->setVisible (visible && simulated && info.params[(size_t) param].name != nullptr);
        }

        thumbs.setSelected (selected);
        resized();
        scrollToCard (thumbView, thumbs.boundsOfCard (selected));
        repaint();
    }

    // RATE only matters free-running and DIVISION only when synced, so the
    // unused one steps back.
    IlanaAnim::ChangeGate changeGate;

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

        if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
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
    bool userPickingShape = false;
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

        // Headings on the card-title line every page uses (12 px down).
        paintSectionTitle (g, "LFO", juce::Rectangle<int> (headingX, 12, 200, headingHeight));
        paintSectionTitle (g, "ENVELOPES", juce::Rectangle<int> (headingX, lfoBottom + 4, 200, headingHeight));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (headingHeight);

        const auto lfoHeight = (area.getHeight() - headingHeight - 8) / 2;
        auto lfoArea = area.removeFromTop (lfoHeight);
        lfoBottom = lfoArea.getBottom();
        area.removeFromTop (headingHeight + 8);
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
               private IlanaAnim::FrameTimer
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
        hardSync.showAsSwitch();
        ringMod = std::make_unique<KnobControl> (p.apvts, "ring_mod", "RING MOD", fmColour(), false);
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

            auto button = std::make_unique<juce::TextButton> ("OSC " + juce::String (source + 1));
            button->setClickingTogglesState (false);
            IlanaTheme::makePill (*button, FmDiagram::oscColour (source));
            button->onClick = [this, source] { selectOperator (source); };
            addAndMakeVisible (*button);
            operatorButtons[(size_t) source] = std::move (button);
        }

        noiseColourKnob = std::make_unique<KnobControl> (p.apvts, "fm_noise_color", "NOISE COLOUR", noiseColour(), false);
        addAndMakeVisible (*noiseColourKnob);

        for (const auto* prefix : OscillatorIds::prefixes)
            tuneValues.push_back (p.apvts.getRawParameterValue (juce::String (prefix) + "_tune"));

        refreshShown();
        refreshFmInputs (true);
        selectOperator (0);
        startTimerHz (12);
    }

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }
    static juce::Colour noiseColour() { return IlanaTheme::Ui::text2; }

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
        paintSectionTitle (g, "ALGORITHMS", algorithmsTitle);
        paintSectionTitle (g, "OPERATORS", operatorsTitle);
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        // The selected operator's settings.
        {
            const auto colour = FmDiagram::oscColour (selectedOperator);
            IlanaTheme::paintCard (g, operatorCard.toFloat(), 7.0f, colour.withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, operatorCard.reduced (12, 0).withHeight (26),
                                         "OSC " + juce::String (selectedOperator + 1) + " AS AN OPERATOR", soundingText(), colour, 0);
        }

        IlanaTheme::paintCardHeader (g, matrixCard.reduced (12, 0).withHeight (26), "FM MATRIX", "rows modulate columns", fmColour(), 0);

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
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    const auto type = juce::roundToInt (read (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_fb_type"));
                    g.drawText (type == FmFeedback::Filtered ? "FB~" : type == FmFeedback::Cross ? "FB<>" : "FB",
                                cell.reduced (6.0f, 4.0f).toNearestInt(), juce::Justification::topLeft);
                }
            }

            if (fmIn[(size_t) source])
                paintCell (noiseCells[(size_t) source].toFloat(), read ("fm_noise" + juce::String (source + 1)), noiseColour());
        }

        // Compact cells: each amount under its knob.
        if (compactCells)
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            const auto value = [&g, this] (juce::Rectangle<int> cell, const juce::String& id)
            {
                const auto amount = read (id);
                g.setColour (juce::Colours::white.withAlpha (amount > 0.001f ? 0.85f : 0.4f));
                g.drawText (describeValue (id, amount), cell.removeFromBottom (15), juce::Justification::centred);
            };
            for (const auto source : shown)
            {
                for (const auto target : shown)
                    if (fmIn[(size_t) target])
                        value (cells[(size_t) source][(size_t) target], FmDiagram::routeId (source, target));
                if (fmIn[(size_t) source])
                    value (noiseCells[(size_t) source], "fm_noise" + juce::String (source + 1));
            }
        }

        // Column and row headings.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

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
    IlanaAnim::ChangeGate changeGate;

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
        else if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
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

        matrixCard = area.removeFromRight (area.getWidth() * 48 / 100);
        area.removeFromRight (10);

        // Left column: algorithms, the diagram, the selected operator; its
        // heading on the FM MATRIX card's header line.
        algorithmsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
        algorithms.setBounds (area.removeFromTop (area.getWidth() >= 16 * 38 ? 48 : 80));
        area.removeFromTop (6);
        operatorsTitle = area.removeFromTop (headingHeight).withTrimmedLeft (12);
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

        inner.removeFromTop (4);
        std::vector<juce::Component*> bottom;
        if (tune == OscTuning::Ratio)
            bottom.push_back (&controls.ratio);
        if (tune == OscTuning::Fixed)
            bottom.push_back (&controls.fixedHz);
        for (auto* item : { &controls.semi, &controls.fine, &controls.level, &controls.keyLevel })
            bottom.push_back (item);

        // Menus and knobs on one grid (as many columns as the longer row), so
        // each menu sits over a knob.
        const auto columns = juce::jmax (top.size(), bottom.size());
        top.resize (columns, nullptr);
        bottom.resize (columns, nullptr);
        layoutRow (topRow, top);
        layoutRow (inner, bottom);
    }

    void layoutMatrix()
    {
        auto inner = matrixCard.reduced (10, 0);
        inner.removeFromTop (26);
        inner.removeFromBottom (8);

        // Mode, ring mod, the noise colour and sync across the top, labels
        // above like every card's controls (smaller dials: the cells below
        // need the height).
        layoutRow (inner.removeFromTop (72), { &mode, ringMod.get(), noiseColourKnob.get(), &hardSync });
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

        // Short cells (six oscillators) get compact knobs, their values
        // drawn in the cell's corner: the knob's own value box overlapped it.
        compactCells = rowHeight < 82;
        const auto layoutRow = [&] (juce::Rectangle<int> row, auto&& knobFor, auto&& storeCell)
        {
            for (const auto target : shown)
            {
                auto cell = row.removeFromLeft (columnWidth).reduced (4, 0);
                storeCell (target, cell);
                auto& knob = knobFor (target);
                knob.setCompact (compactCells);
                if (compactCells)
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 22, 110);
                    knob.setBounds (cell.withSizeKeepingCentre (knobSize, knobSize).translated (0, -6));
                }
                else
                {
                    const auto knobSize = juce::jmin (cell.getWidth() - 12, cell.getHeight() - 8, 110);
                    knob.setBounds (cell.withSizeKeepingCentre (knobSize, knobSize + 4));
                }
            }
        };

        // A row's name above its OUT button, centred in the row head (they
        // overlapped the next row's name in short rows).
        const auto layoutHead = [] (juce::Rectangle<int> head)
        {
            return head.withSizeKeepingCentre (head.getWidth(), juce::jmin (60, head.getHeight()));
        };

        for (const auto source : shown)
        {
            auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
            auto head = row.removeFromLeft (70);
            rowHeads[(size_t) source] = layoutHead (head);
            // The row's name, then its OUT switch with that name above it.
            const auto& block = rowHeads[(size_t) source];
            outs[(size_t) source]->setBounds (block.withTrimmedTop (20).withHeight (13 + juce::jmin (22, block.getHeight() - 33)));

            layoutRow (row,
                       [this, source] (int target) -> KnobControl& { return *knobs[(size_t) source][(size_t) target]; },
                       [this, source] (int target, juce::Rectangle<int> cell) { cells[(size_t) source][(size_t) target] = cell; });
        }

        auto row = inner.removeFromTop (rowHeight).reduced (0, 3);
        auto head = row.removeFromLeft (70);
        noiseHead = layoutHead (head);
        layoutRow (row,

                   [this] (int target) -> KnobControl& { return *noiseKnobs[(size_t) target]; },
                   [this] (int target, juce::Rectangle<int> cell) { noiseCells[(size_t) target] = cell; });
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, OscillatorIds::count> fmIn {};
    FmDiagram diagram;
    FmAlgorithmStrip algorithms;
    ComboControl mode;
    ToggleControl hardSync;
    std::unique_ptr<KnobControl> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, OscillatorIds::count>, OscillatorIds::count> knobs;
    std::array<std::unique_ptr<ToggleControl>, OscillatorIds::count> outs;
    std::array<std::unique_ptr<KnobControl>, OscillatorIds::count> noiseKnobs;
    bool compactCells = false;
    std::unique_ptr<KnobControl> noiseColourKnob;
    std::array<std::unique_ptr<OperatorControls>, OscillatorIds::count> operators;
    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> operatorButtons;
    std::array<juce::Rectangle<int>, OscillatorIds::count> columnHeads, rowHeads, noiseCells;
    std::array<std::array<juce::Rectangle<int>, OscillatorIds::count>, OscillatorIds::count> cells;
    juce::Rectangle<int> matrixCard, operatorCard, algorithmsTitle, operatorsTitle, noiseHead;
    std::vector<std::atomic<float>*> tuneValues;
    std::array<int, OscillatorIds::count> lastTune {};
    std::vector<int> shown;
    int selectedOperator = 0;
};

// M7.5: ilanaSynth FX's INPUT page: the input's level and envelope, its
// gain and trigger, where it goes, and quick starts.
class InputPage : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    explicit InputPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          gain (p.apvts, "in_gain", "GAIN", inputColour(), false),
          dry (p.apvts, "in_dry", "DRY", inputColour(), false),
          trigger (p.apvts, "in_trigger", "TRIGGER"),
          threshold (p.apvts, "in_threshold", "THRESHOLD", inputColour(), false),
          note (p.apvts, "in_note", "NOTE", inputColour(), false),
          attack (p.apvts, "in_attack", "ATTACK", inputColour(), false),
          release (p.apvts, "in_release", "RELEASE", inputColour(), false),
          toBody (p.apvts, "in_body", "TO BODY", routeColour(), false),
          toStrings (p.apvts, "in_strings", "TO STRINGS", routeColour(), false)
    {
        addAll (*this, gain, dry, trigger, threshold, note, attack, release, toBody, toStrings);
        const char* names[] { "Live Body", "Live Wah", "Live Grains", "Live Strings" };
        const char* tips[] { "The input rings a metal body (BODY section)",
                             "A Live oscillator through a filter the input's envelope opens",
                             "A granular oscillator reading the last seconds of the input",
                             "The input keeps two strings ringing, E and B" };
        for (int i = 0; i < 4; ++i)
        {
            auto& button = quickStarts[(size_t) i];
            button.setButtonText (juce::String (names[i]).toUpperCase());
            button.setTooltip (tips[i]);
            button.onClick = [this, name = juce::String (names[i])]
            {
                const auto index = processorRef.getFactoryPresetNames().indexOf (name);
                if (index >= 0)
                    processorRef.loadFactoryPreset (index);
            };
            addAndMakeVisible (button);
        }
        startTimerHz (30);
    }

    static juce::Colour inputColour() { return juce::Colour (0xff5fd3ff); }
    static juce::Colour routeColour() { return juce::Colour (0xffffb454); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        const auto title = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour)
        {
            IlanaTheme::paintTag (g, { (float) area.getX() + 3.0f, (float) area.getCentreY() }, colour);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText (text, area.withTrimmedLeft (14), juce::Justification::centredLeft);
        };
        IlanaTheme::paintCard (g, inputCard.toFloat(), 7.0f, inputColour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, routeCard.toFloat(), 7.0f, routeColour().withAlpha (0.35f));
        IlanaTheme::paintCard (g, startCard.toFloat(), 7.0f, IlanaTheme::accent().withAlpha (0.35f));
        title (inputCard.reduced (12, 0).removeFromTop (26), "INPUT", inputColour());
        title (routeCard.reduced (12, 0).removeFromTop (26), "ROUTING", routeColour());
        title (startCard.reduced (12, 0).removeFromTop (26), "QUICK START", IlanaTheme::accent());

        // Level (peak) and envelope, with the gate threshold marked.
        IlanaTheme::paintWell (g, meter.toFloat(), 5.0f);
        const auto bar = [this, &g] (juce::Rectangle<float> area, float value, juce::Colour colour, const juce::String& label)
        {
            const auto db = value > 1.0e-5f ? juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (value) / 60.0f) : 0.0f;
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (area, 3.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (area.withWidth (area.getWidth() * db), 3.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText (label, area.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
            juce::ignoreUnused (this);
        };
        auto area = meter.toFloat().reduced (10.0f, 8.0f);
        const auto rowHeight = (area.getHeight() - 6.0f) / 2.0f;
        const auto levelRow = area.removeFromTop (rowHeight);
        area.removeFromTop (6.0f);
        bar (levelRow, shownLevel, inputColour().withAlpha (0.8f), "LEVEL");
        bar (area, shownEnvelope, routeColour().withAlpha (0.8f), "ENVELOPE (Input Env in the matrix)");
        if (readChoice ("in_trigger") == 1)
        {
            const auto thresholdDb = processorRef.apvts.getRawParameterValue ("in_threshold")->load();
            const auto x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, 1.0f + thresholdDb / 60.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (x - 1.0f, levelRow.getY(), 2.0f, area.getBottom() - levelRow.getY());
        }

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        const juce::String help[] {
            "TRIGGER: Off plays only on MIDI notes; Gate plays NOTE while the input is over THRESHOLD; Drone holds NOTE down.",
            "OSC: set an oscillator's MODE to Live to play the input through the filters, FM and effects,",
            "or turn on LIVE in Granular mode to granulate its last three seconds (POSITION is how far back).",
            "BODY: TO BODY rings the BODY section (switch it on in FILTER). TO STRINGS drives Physical strings, tines and reeds.",
            "DRY adds the untouched input back at the end. Input Env modulates anything from the MATRIX."
        };
        auto text = helpArea;
        for (const auto& line : help)
            g.drawText (line, text.removeFromTop (20), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto top = area.removeFromTop (juce::jmin (230, area.getHeight() / 2));
        inputCard = top.removeFromLeft (top.getWidth() * 3 / 5).reduced (0, 0);
        top.removeFromLeft (10);
        routeCard = top;
        area.removeFromTop (10);
        startCard = area;

        auto inside = inputCard.reduced (12, 0);
        inside.removeFromTop (28);
        meter = inside.removeFromTop (58);
        inside.removeFromTop (8);
        auto knobs = inside.removeFromTop (juce::jmin (100, inside.getHeight() - 6));
        const auto width = knobs.getWidth() / 7;
        for (auto* control : std::initializer_list<juce::Component*> { &gain, &trigger, &threshold, &note, &attack, &release, &dry })
            control->setBounds (knobs.removeFromLeft (width).reduced (3, 0));

        auto routes = routeCard.reduced (12, 0);
        routes.removeFromTop (34);
        routes = routes.removeFromTop (juce::jmin (110, routes.getHeight()));
        toBody.setBounds (routes.removeFromLeft (routes.getWidth() / 2).reduced (6, 0));
        toStrings.setBounds (routes.reduced (6, 0));

        auto start = startCard.reduced (14, 0);
        start.removeFromTop (34);
        auto buttons = start.removeFromTop (34);
        const auto buttonWidth = buttons.getWidth() / 4;
        for (auto& button : quickStarts)
            button.setBounds (buttons.removeFromLeft (buttonWidth).reduced (4, 2));
        start.removeFromTop (14);
        helpArea = start;
    }

private:
    void timerCallback() override
    {
        const auto level = processorRef.getInputLevel();
        const auto envelope = processorRef.getInputEnvelope();
        if (std::abs (level - shownLevel) > 1.0e-4f || std::abs (envelope - shownEnvelope) > 1.0e-4f)
        {
            shownLevel = level;
            shownEnvelope = envelope;
            repaint (meter);
        }
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    KnobControl gain, dry;
    ComboControl trigger;
    KnobControl threshold, note, attack, release, toBody, toStrings;
    std::array<juce::TextButton, 4> quickStarts;
    juce::Rectangle<int> inputCard, routeCard, startCard, meter, helpArea;
    float shownLevel = 0.0f, shownEnvelope = 0.0f;
};

class SeqPage : public juce::Component,
                private juce::Timer
{
public:
    // The page is shown in two places: its step LFOs and MSEG under MOD,
    // its note generators (arp, Euclid, prob seq, generate) as SEQ.
    enum class Part { modulators, notes };

    SeqPage (IlanaSynthAudioProcessor& p, Part partIn)
        : part (partIn),
          step1 (p, 0, IlanaTheme::accent(), true),
          step2 (p, 1, juce::Colour (0xff35c8ff)),
          mseg (p),
          msegLoop (p.apvts, "mseg_loop", "LOOP"),
          msegRate (p.apvts, "mseg_rate", "RATE", msegColour(), false),
          clockDiv (p.apvts, "clock_div", "S&H CLOCK", msegColour(), false),
          processorRef (p),
          arpDisplay (p, arpColour()),
          arpOn (p.apvts, "arp_on", "ON"),
          arpMode (p.apvts, "arp_mode", "MODE"),
          arpDiv (p.apvts, "arp_div", "RATE"),
          arpOctaves (p.apvts, "arp_octaves", "OCTAVES", arpColour(), true),
          arpGate (p.apvts, "arp_gate", "GATE", arpColour(), true),
          arpChance (p.apvts, "arp_chance", "CHANCE", arpColour(), true),
          genScale (p.apvts, "gen_scale", "SCALE"),
          genRoot (p.apvts, "gen_root", "ROOT"),
          genSnap (p.apvts, "gen_snap", "SNAP PLAYED"),
          sprayOn (p.apvts, "spray_on", "ON"),
          sprayDirection (p.apvts, "spray_direction", "DIRECTION"),
          sprayStrum (p.apvts, "spray_strum", "MODE"),
          engineTabs ({ "ARP", "EUCLID", "PROB SEQ" }, { arpColour(), euclidColour(), pseqColour() }, false),
          euclidDisplay (p, euclidColour()),
          eucOn (p.apvts, "euc_on", "ON"),
          eucTarget (p.apvts, "euc_target", "TARGET"),
          eucDiv (p.apvts, "euc_div", "RATE"),
          eucSteps (p.apvts, "euc_steps", "STEPS", euclidColour(), true),
          eucHits (p.apvts, "euc_hits", "HITS", euclidColour(), true),
          eucRotate (p.apvts, "euc_rotate", "ROTATE", euclidColour(), true),
          eucGate (p.apvts, "euc_gate", "GATE", euclidColour(), true),
          pseqEditor (p, pseqColour()),
          pseqOn (p.apvts, "pseq_on", "ON"),
          pseqDiv (p.apvts, "pseq_div", "RATE"),
          pseqLength (p.apvts, "pseq_length", "LENGTH", pseqColour(), true),
          pseqGate (p.apvts, "pseq_gate", "GATE", pseqColour(), true)
    {
        sprayCount = std::make_unique<KnobControl> (p.apvts, "spray_count", "NOTES", generateColour(), true);
        sprayRange = std::make_unique<KnobControl> (p.apvts, "spray_range", "RANGE", generateColour(), true);
        spraySpread = std::make_unique<KnobControl> (p.apvts, "spray_spread", "SPREAD", generateColour(), true);
        strumTime = std::make_unique<KnobControl> (p.apvts, "spray_strum_time", "TIME", generateColour(), true);
        sprayChance = std::make_unique<KnobControl> (p.apvts, "spray_chance", "CHANCE", generateColour(), true);
        sprayVelocity = std::make_unique<KnobControl> (p.apvts, "spray_velocity", "VEL RND", generateColour(), true);
        addAll (*this, arpChance, genScale, genRoot, genSnap, sprayOn, sprayDirection,
                *sprayCount, *sprayRange, *spraySpread, *sprayChance, *sprayVelocity, sprayStrum, *strumTime);

        // The Generative card: ARP, EUCLID and PROB SEQ share one card.
        addAll (*this, engineTabs, euclidDisplay, eucOn, eucTarget, eucDiv, eucSteps, eucHits, eucRotate, eucGate,
                pseqEditor, pseqOn, pseqDiv, pseqLength, pseqGate);
        engineTabs.onSelect = [this] (int) { showEngineTab(); };

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
                IlanaTheme::makePill (button, IlanaSynthAudioProcessor::lfoColour (lfo));
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

        msegLoop.showAsSwitch();
        genSnap.showAsSwitch();
        addAll (*this, mseg, msegLoop, msegRate, clockDiv,
                arpDisplay, arpOn, arpMode, arpDiv, arpOctaves, arpGate);

        // Open on whichever part of the card is switched on.
        engineTabs.setSelected (readOn ("pseq_on") ? 2 : readOn ("euc_on") ? 1 : 0, false);
        showEngineTab();

        if (part == Part::notes)
        {
            for (auto* control : std::initializer_list<juce::Component*> { &step1, &step2, &mseg, &msegLoop, &msegRate, &clockDiv })
                control->setVisible (false);

            for (auto& row : lfoButtons)
                for (auto& button : row)
                    button.setVisible (false);
        }
        else
        {
            for (auto* control : std::initializer_list<juce::Component*> {
                     &engineTabs, &euclidDisplay, &eucOn, &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate,
                     &pseqEditor, &pseqOn, &pseqDiv, &pseqLength, &pseqGate, &arpDisplay, &arpOn, &arpMode, &arpDiv,
                     &arpOctaves, &arpGate, &arpChance, &genScale, &genRoot, &genSnap, &sprayOn, &sprayDirection, &sprayStrum,
                     sprayCount.get(), sprayRange.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get(), strumTime.get() })
                control->setVisible (false);
        }

        startTimerHz (8);
    }

    static juce::Colour msegColour() { return juce::Colour (0xffe0e6f0); }
    // Not modulation sources: the accent (a source's colour always means
    // that source).
    static juce::Colour arpColour() { return IlanaTheme::accent(); }
    static juce::Colour generateColour() { return IlanaTheme::accent(); }
    static juce::Colour euclidColour() { return IlanaTheme::accent(); }
    static juce::Colour pseqColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto title = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour)
        {
            IlanaTheme::paintTag (g, { (float) area.getX() + 3.0f, (float) area.getCentreY() }, colour);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText (text, area.withTrimmedLeft (14), juce::Justification::centredLeft);
        };

        if (part == Part::modulators)
        {
            // Two step rows, A and B, each tagged in the colour of the LFO it
            // edits; the subtitle says whether that LFO plays its steps.
            const auto stepNote = [this] (int lfo)
            {
                const auto* shape = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape");
                return shape != nullptr && juce::roundToInt (shape->load()) == LfoShapes::Steps
                           ? juce::String ("LFO ") + juce::String (lfo + 1)
                           : juce::String ("LFO ") + juce::String (lfo + 1) + " isn't playing these: set its SHAPE to Steps";
            };
            const auto pillsWidth = [this] (int rowIndex)
            {
                auto left = 100000;
                for (auto& button : lfoButtons[(size_t) rowIndex])
                    if (button.isVisible())
                        left = juce::jmin (left, button.getX());
                return left < 100000 ? stepTitle1.getRight() - left + 12 : 0;
            };
            IlanaTheme::paintCardHeader (g, stepTitle1, "STEPS A", stepNote (step1.getLfoIndex()),
                                         IlanaSynthAudioProcessor::lfoColour (step1.getLfoIndex()), pillsWidth (0));
            IlanaTheme::paintCardHeader (g, stepTitle2, "STEPS B", stepNote (step2.getLfoIndex()),
                                         IlanaSynthAudioProcessor::lfoColour (step2.getLfoIndex()), pillsWidth (1));

            IlanaTheme::paintCard (g, msegCard.toFloat(), 7.0f, msegColour().withAlpha (0.35f));
            IlanaTheme::paintCardHeader (g, msegCard.reduced (12, 0).removeFromTop (26), "MSEG",
                                         "drag points; drag the MSEG chip onto a knob to use it", msegColour(), 0);
            return;
        }

        const auto tab = engineTabs.getSelected();
        const auto tabColour = tab == 1 ? euclidColour() : tab == 2 ? pseqColour() : arpColour();
        IlanaTheme::paintCard (g, arpCard.toFloat(), 7.0f, tabColour.withAlpha (0.35f));
        IlanaTheme::paintCard (g, generateCard.toFloat(), 7.0f, generateColour().withAlpha (0.35f));
        IlanaTheme::paintCardHeader (g, generateCard.reduced (12, 0).removeFromTop (26), "GENERATE",
                                     "snap to a scale, spray and strum the notes", generateColour(), 0);

        {
            // The subtitle says what the tab shown does right now.
            const auto arpOnNow = readOn ("arp_on"), seqOnNow = readOn ("pseq_on"), euclidOnNow = readOn ("euc_on");
            juce::String hint;

            if (tab == 0)
                hint = seqOnNow && arpOnNow ? "the probability sequencer is playing instead" : "hold notes to play the pattern";
            else if (tab == 1)
                hint = (int) readValue ("euc_target") == 0 ? (arpOnNow || seqOnNow ? "rests the steps between hits" : "plays the held chord on each hit")
                     : (int) readValue ("euc_target") == 1 ? "re-strikes Physical strings on each hit"
                                                           : "drives the Trance Gate effect (add it in FX)";
            else
                hint = seqOnNow && arpOnNow ? "takes over from the arp while on" : "hold notes: each step rolls its chance";

            if (tab == 1 && ! euclidOnNow)
                hint = "switch it on (top right) to use it";

            IlanaTheme::paintCardHeader (g, arpCard.reduced (12, 0).removeFromTop (26), "GENERATIVE", hint, tabColour,
                                         arpCard.getRight() - engineTabs.getX() + 80);
        }
        // Generate's group headings: the label, then a hairline to the end
        // of the group.
        for (const auto& [area, text] : { std::pair<juce::Rectangle<int>, const char*> { scaleDivider, "SCALE" },
                                          std::pair<juce::Rectangle<int>, const char*> { strumDivider, "STRUM" },
                                          std::pair<juce::Rectangle<int>, const char*> { sprayDivider, "NOTE SPRAY" } })
        {
            if (area.isEmpty())
                continue;

            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = juce::GlyphArrangement::getStringWidthInt (font, text);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (font);
            g.drawText (text, area, juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (area.getX() + width + 10, area.getCentreY(), juce::jmax (0, area.getWidth() - width - 16), 1);
        }


    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        // Each row's LFO buttons at the right of its header, like a card's tabs.
        const auto layoutPicker = [this] (int rowIndex, juce::Rectangle<int> header)
        {
            auto& buttons = lfoButtons[(size_t) rowIndex];
            auto count = 0;

            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                count += buttons[(size_t) lfo].isVisible() ? 1 : 0;

            const auto width = juce::jmin (60, header.getWidth() / 2 / juce::jmax (1, count));
            auto strip = header.removeFromRight (width * count).reduced (0, 3);

            for (auto& button : buttons)
                if (button.isVisible())
                    button.setBounds (strip.removeFromLeft (width).reduced (2, 0));
        };

        // MOD: the two step rows and the MSEG. SEQ: arp and generate side
        // by side.
        if (part == Part::notes)
        {
            layoutNotes (area);
            return;
        }

        const auto msegHeight = juce::jlimit (170, 260, area.getHeight() * 2 / 5);
        const auto stepHeight = (area.getHeight() - msegHeight - 16) / 2;

        auto row = area.removeFromTop (stepHeight);
        stepTitle1 = row.removeFromTop (26).withTrimmedLeft (12);
        layoutPicker (0, stepTitle1);
        step1.setBounds (row);

        area.removeFromTop (8);
        row = area.removeFromTop (stepHeight);
        stepTitle2 = row.removeFromTop (26).withTrimmedLeft (12);
        layoutPicker (1, stepTitle2);
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
    }

    void layoutNotes (juce::Rectangle<int> right)
    {
        // The Generative card (arp, Euclid, probability sequencer) above
        // generate, both full width.
        // Generate gets the height for two rows of full-size knobs (as big
        // as the arp's), the pattern display above takes the rest.
        constexpr int knobRowHeight = 13 + 58 + 16 + 6;
        const auto generateHeight = juce::jlimit (160, (right.getHeight() - 8) / 2, 26 + knobRowHeight * 2 + 18 + 12);
        generateCard = right.removeFromBottom (generateHeight);
        right.removeFromBottom (8);
        arpCard = right;

        auto arpArea = arpCard.reduced (10, 0);
        auto header = arpArea.removeFromTop (26);
        // The shown engine's on switch in the header's switch place, its tabs
        // just left of it.
        const auto engineSwitch = IlanaTheme::cardSwitchBounds (arpCard, header.getCentreY());
        for (auto* toggle : { &arpOn, &eucOn, &pseqOn })
            toggle->setBounds (engineSwitch);
        header.setRight (engineSwitch.getX() - 8);
        engineTabs.setBounds (header.removeFromRight (engineTabs.getIdealWidth()).reduced (0, 4));
        engineHint = arpCard.reduced (12, 0).withHeight (26);
        arpArea.removeFromBottom (6);
        const auto display = arpArea.removeFromTop (juce::jmax (36, arpArea.getHeight() - knobRowHeight - 8)).reduced (0, 2);
        arpArea.removeFromTop (8);
        arpDisplay.setBounds (display);
        euclidDisplay.setBounds (display);
        pseqEditor.setBounds (display);

        // Every engine's row on one six-column grid, packed from the left,
        // and the same grid runs through Generate below.
        layoutRow (arpArea, { &arpMode, &arpDiv, &arpOctaves, &arpGate, &arpChance, nullptr });
        layoutRow (arpArea, { &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate });
        layoutRow (arpArea, { &pseqDiv, &pseqLength, &pseqGate, nullptr, nullptr, nullptr });

        // Generate: three groups side by side, each under its own heading:
        // SCALE (two columns), STRUM (one) and NOTE SPRAY (three, its
        // switch on the heading's line at the card switch place), rows of
        // full-size controls on the shared grid.
        auto generate = generateCard.reduced (10, 0);
        generate.removeFromTop (26);
        generate.removeFromBottom (6);

        const auto column = generate.getWidth() / 6;
        const auto rowHeight = juce::jmin (knobRowHeight, (generate.getHeight() - 22 - 6) / 2);
        auto block = generate.withSizeKeepingCentre (generate.getWidth(), juce::jmin (generate.getHeight(), 22 + rowHeight * 2 + 6));
        const auto headings = block.removeFromTop (22);
        const auto first = block.removeFromTop (rowHeight);
        block.removeFromTop (6);
        const auto second = block.removeFromTop (rowHeight);

        scaleDivider = headings.withWidth (column * 2).reduced (3, 0);
        strumDivider = headings.withTrimmedLeft (column * 2).withWidth (column).reduced (3, 0);
        sprayDivider = headings.withTrimmedLeft (column * 3).reduced (3, 0);
        // The spray's switch in the card's header, at the card switch place.
        sprayOn.setBounds (IlanaTheme::cardSwitchBounds (generateCard, generateCard.getY() + 13));

        layoutRow (first, { &genScale, &genRoot, &sprayStrum, &sprayDirection, sprayCount.get(), sprayRange.get() });
        layoutRow (second, { nullptr, nullptr, strumTime.get(), spraySpread.get(), sprayChance.get(), sprayVelocity.get() });
        // SNAP PLAYED centred under the SCALE group, level with the dials.
        {
            const auto dialDrop = juce::jlimit (28, 58, column - 6) / 2 - 12;
            const auto rowBand = second.withSizeKeepingCentre (second.getWidth(), juce::jmin (second.getHeight(), preferredControlHeight (strumTime.get(), column - 6) + 6));
            genSnap.setBounds (rowBand.withWidth (column * 2).withSizeKeepingCentre (column, rowBand.getHeight()).reduced (3).withTrimmedTop (dialDrop));
        }
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
        repaint (stepTitle1.getUnion (stepTitle2)); // the titles name the LFOs
    }

    // Arp controls step back while the arp is off.
    void timerCallback() override
    {
        // The step-row pickers list the patch's LFOs (and whatever either row
        // shows), the same list in both rows.
        auto pickersChanged = false;
        for (int row = 0; row < (part == Part::modulators ? 2 : 0); ++row)
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            {
                auto& button = lfoButtons[(size_t) row][(size_t) lfo];
                const auto shown = processorRef.isLfoShown (lfo) || step1.getLfoIndex() == lfo || step2.getLfoIndex() == lfo;
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

        const auto dim = [] (std::initializer_list<juce::Component*> controls, bool on)
        {
            for (auto* control : controls)
                if (control->getAlpha() != (on ? 1.0f : 0.45f))
                    control->setAlpha (on ? 1.0f : 0.45f);
        };

        dim ({ &eucTarget, &eucDiv, &eucSteps, &eucHits, &eucRotate, &eucGate }, readOn ("euc_on"));
        dim ({ &pseqDiv, &pseqLength, &pseqGate }, readOn ("pseq_on"));
        dim ({ strumTime.get() }, (int) readValue ("spray_strum") != 0);
        repaint (engineHint);
        repaint (stepTitle1); // their notes follow the LFOs' shapes
        repaint (stepTitle2);

        const auto* spray = processorRef.apvts.getRawParameterValue ("spray_on");
        const auto sprayAlpha = spray != nullptr && spray->load() > 0.5f ? 1.0f : 0.45f;

        for (juce::Component* control : { static_cast<juce::Component*> (&sprayDirection), static_cast<juce::Component*> (sprayCount.get()),
                                          static_cast<juce::Component*> (sprayRange.get()), static_cast<juce::Component*> (spraySpread.get()),
                                          static_cast<juce::Component*> (sprayChance.get()), static_cast<juce::Component*> (sprayVelocity.get()) })
            if (control->getAlpha() != sprayAlpha)
                control->setAlpha (sprayAlpha);
    }

    float readValue (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool readOn (const char* id) const { return readValue (id) > 0.5f; }

    void showEngineTab()
    {
        if (part == Part::modulators)
            return;

        const auto tab = engineTabs.getSelected();

        for (auto* control : std::initializer_list<juce::Component*> { &arpDisplay, &arpOn, &arpMode, &arpDiv, &arpOctaves,
                                                                        &arpGate, &arpChance })
            control->setVisible (tab == 0);

        for (auto* control : std::initializer_list<juce::Component*> { &euclidDisplay, &eucOn, &eucTarget, &eucDiv, &eucSteps,
                                                                        &eucHits, &eucRotate, &eucGate })
            control->setVisible (tab == 1);

        for (auto* control : std::initializer_list<juce::Component*> { &pseqEditor, &pseqOn, &pseqDiv, &pseqLength, &pseqGate })
            control->setVisible (tab == 2);

        repaint();
    }

    Part part;
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
    ComboControl sprayDirection, sprayStrum;
    std::unique_ptr<KnobControl> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity, strumTime;
    CardTabs engineTabs;
    EuclidDisplay euclidDisplay;
    ToggleControl eucOn;
    ComboControl eucTarget, eucDiv;
    KnobControl eucSteps, eucHits, eucRotate, eucGate;
    ProbSeqEditor pseqEditor;
    ToggleControl pseqOn;
    ComboControl pseqDiv;
    KnobControl pseqLength, pseqGate;
    juce::Rectangle<int> engineHint;
    juce::Rectangle<int> sprayDivider, scaleDivider, strumDivider;
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
          envTabs ({ "AMP ENV", "FILT ENV", "FILT 2 ENV", "MOD ENV", "ENV 5" },
                   { envColour (0), envColour (1), envColour (2), envColour (3), envColour (4) }, true),
          lfoTabs ({}, {}, true)
    {
        const auto colours = [] (int osc) { return IlanaTheme::oscColour (osc); };

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            waves[(size_t) osc] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison", prefix + "_spread",
                prefix + "_detune", false, juce::String {}, prefix + "_mode", osc, colours (osc), false);
            oscColumn.addAndMakeVisible (*waves[(size_t) osc]);
            auto strip = std::make_unique<OscStrip>();
            const auto colour = colours (osc);

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

                strip->allKnobs.push_back ({ id + label, std::make_unique<KnobControl> (p.apvts, id, label, colour, false) });
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
            // M7.5 Live: the input has no pitch or shape to set.
            strip->modeKnobs[4] = { knob ("_level", "LEVEL"), knob ("_pan", "PAN") };

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
        oscColumn.addChildComponent (patchFlow);

        // Sub and noise under the oscillators: the rest of the sources, laid
        // out like the filter card (menus stacked left, knobs on the
        // oscillators' knob grid).
        subOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subShape = std::make_unique<ComboControl> (p.apvts, "sub_shape", "SHAPE");
        subOctave = std::make_unique<ComboControl> (p.apvts, "sub_octave", "OCTAVE");
        subLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB LEVEL", subColour(), true);
        noiseLevel = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        addAll (oscColumn, *subOn, *subShape, *subOctave, *subLevel, *noiseLevel);

        oscColumn.onPaint = [this] (juce::Graphics& g)
        {
            if (! subCard.isEmpty())
                paintCard (g, subCard, "SUB + NOISE", subColour());

            if (! patchCard.isEmpty())
                paintCard (g, patchCard, "PATCH", IlanaTheme::accent());

            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (shownStrips[(size_t) osc])
                {
                    const auto folded = ! strips[(size_t) osc]->shownOn;
                    paintCard (g, oscCards[(size_t) osc], "OSC " + juce::String (osc + 1), OscPage::oscColour (osc), folded);

                    if (folded)
                    {
                        static const char* const modeNames[] { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
                        g.setColour (IlanaTheme::Ui::text3);
                        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                        g.drawText (juce::String ("OFF  -  ") + modeNames[juce::jlimit (0, 4, readInt (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode"))]
                                        + "  -  switch on to edit",
                                    oscCards[(size_t) osc].withTrimmedLeft (80).withHeight (16).withY (titleCentreY (oscCards[(size_t) osc], true) - 8),
                                    juce::Justification::centredLeft);
                    }
                }
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
            set->items.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_rate", "RATE", lfoColour (lfo), false));
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
        return IlanaSynthAudioProcessor::lfoColour (index);
    }

    static juce::Colour filterColour (int index) { return index == 0 ? juce::Colour (0xffc86bff) : juce::Colour (0xff8f9dff); }

    static juce::Colour envColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xffc86bff);
            case 2: return juce::Colour (0xff8f9dff);
            case 3: return juce::Colour (0xff8fff3b);
            case 4: return juce::Colour (0xff5b8cff);
            default: return modSourceColour ((int) Mod::Source::AmpEnv);
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
        const auto anyHidden = std::find (shownStrips.begin(), shownStrips.end(), false) != shownStrips.end();

        // Switched-off oscillators fold to a title line.
        auto numOpen = 0, numFolded = 0;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                (strips[(size_t) osc]->shownOn ? numOpen : numFolded) += 1;

        const auto baseHeight = (left.getHeight() - 16) / 3;
        const auto spare = left.getHeight() - (numOpen + numFolded - 1) * 8 - numFolded * foldedHeight
                           - (anyHidden ? addButtonHeight + 8 : 0);
        // Open cards keep the three-card size: taller ones only spread their
        // rows apart.
        const auto oscHeight = baseHeight;
        juce::ignoreUnused (spare);
        auto columnHeight = (anyHidden ? addButtonHeight : -8) + 8 + subCardHeight;

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (shownStrips[(size_t) osc])
                columnHeight += (strips[(size_t) osc]->shownOn ? oscHeight : foldedHeight) + 8;

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

            oscCards[(size_t) osc] = column.removeFromTop (strips[(size_t) osc]->shownOn ? oscHeight : foldedHeight);
            column.removeFromTop (8);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

        // Height the column doesn't need goes to the ADD tile (a drop zone
        // for the next oscillator), so the sub card ends level with the LFO
        // card rather than leaving a gap under it.
        const auto leftover = scrolls ? 0 : juce::jmax (0, column.getHeight() - (anyHidden ? addButtonHeight + 8 : 0) - subCardHeight);
        // A tall tile shows the patch live (the signal flow, clickable as on
        // FILTER) with the ADD button in its header; a short one is the button.
        auto tile = column.removeFromTop (anyHidden ? addButtonHeight + leftover : 0);
        const auto showPatch = tile.getHeight() >= patchMinHeight;
        patchCard = showPatch ? tile : juce::Rectangle<int>();
        patchFlow.setVisible (showPatch);

        if (showPatch)
        {
            auto header = tile.reduced (8, 0).withHeight (26).reduced (0, 3);
            addOscButton.setButtonText ("+  ADD OSC");
            addOscButton.setBounds (header.removeFromRight (104));
            patchFlow.setBounds (tile.withTrimmedTop (30).reduced (12, 0).withTrimmedBottom (12));
        }
        else
        {
            addOscButton.setButtonText ("+  ADD OSCILLATOR");
            addOscButton.setBounds (tile);
        }

        addOscButton.setVisible (anyHidden);
        column.removeFromTop (anyHidden ? 8 : 0);
        subCard = column.removeFromTop (subCardHeight + (anyHidden ? 0 : leftover));
        layoutSubCard();
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
            // The selected LFO's controls: one row, labels above like the
            // filter's and envelope's (a small dial fits the card).
            constexpr int controlRow = 13 + 30 + 16 + 6;
            // (12 px between the cards and the row, so the row's labels
            // don't crowd the cards.)
            auto cards = inner.removeFromTop (juce::jmax (40, inner.getHeight() - controlRow - 8));
            lfoThumbs.setViewWidth (cards.getWidth());
            const auto thumbWidth = lfoThumbs.getPreferredWidth();
            lfoThumbView.setBounds (cards);
            lfoThumbs.setSize (thumbWidth, cards.getHeight() - (thumbWidth > cards.getWidth() ? lfoThumbView.getScrollBarThickness() + 1 : 0));
            inner.removeFromTop (12);

            for (auto& set : lfoSets)
                layoutRow (inner, { set->items[0].get(), set->items[1].get(), set->items[2].get(),
                                    set->items[3].get(), set->items[4].get() });
        }
    }

private:
    struct OscStrip
    {
        std::unique_ptr<ToggleControl> on;
        std::unique_ptr<ComboControl> mode, excite, table, warp;
        std::unique_ptr<juce::TextButton> remove;
        std::vector<std::pair<juce::String, std::unique_ptr<KnobControl>>> allKnobs;
        std::array<std::vector<juce::Component*>, 5> modeKnobs;
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
            const auto mode = juce::jlimit (0, 4, readInt (prefix + "_mode"));
            const auto on = readInt (prefix + "_on") > 0;
            const auto shown = processorRef.isOscillatorShown (index);

            if (mode != strip.shownMode || shown != shownStrips[(size_t) index] || on != strip.shownOn)
            {
                strip.shownMode = mode;
                strip.shownOn = on;
                shownStrips[(size_t) index] = shown;
                changed = true;

                // A switched-off oscillator folds to its title and switch.
                const auto open = shown && on;

                for (auto& entry : strip.allKnobs)
                    entry.second->setVisible (false);

                for (auto* item : strip.modeKnobs[(size_t) mode])
                    item->setVisible (open);

                strip.table->setVisible (open && mode == 0);
                strip.warp->setVisible (open && mode == 0);
                strip.excite->setVisible (open && mode == 1);
                strip.on->setVisible (shown);
                strip.mode->setVisible (open);
                wave (index).setVisible (open);
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

        // The sub's controls follow its switch; noise has its own level.
        {
            const auto* subSwitch = processorRef.apvts.getRawParameterValue ("subosc_on");
            const auto alpha = subSwitch != nullptr && subSwitch->load() > 0.5f ? 1.0f : 0.4f;
            for (auto* control : { static_cast<juce::Component*> (subShape.get()), static_cast<juce::Component*> (subOctave.get()),
                                   static_cast<juce::Component*> (subLevel.get()) })
                if (control->getAlpha() != alpha)
                    control->setAlpha (alpha);
        }

        // (Kept up to date while hidden too, so the page never opens stale.)
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

    // The title line's centre: a folded card is just that line, centred;
    // the switch and remove button share it.
    static int titleCentreY (juce::Rectangle<int> card, bool folded = false)
    {
        return folded ? card.getCentreY() : card.getY() + 14;
    }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, bool folded = false)
    {
        if (card.isEmpty())
            return;

        const auto centreY = titleCentreY (card, folded);
        IlanaTheme::paintCard (g, card.toFloat(), 6.0f, tint);
        IlanaTheme::paintTag (g, { (float) card.getX() + 15.0f, (float) centreY }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (title, juce::Rectangle<int> (card.getX() + 24, centreY - 8, 200, 16), juce::Justification::centredLeft);
    }

    WaveDisplay& wave (int index) { return *waves[(size_t) index]; }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];
        auto inner = card.reduced (10, 8);
        auto title = inner.removeFromTop (18).withY (titleCentreY (card, ! strip.shownOn) - 9);
        strip.remove->setBounds (title.removeFromRight (22).withSizeKeepingCentre (20, 15));
        title.removeFromRight (6);
        strip.on->setBounds (IlanaTheme::cardSwitchBounds (card, title.getCentreY(), true));
        inner.removeFromTop (2);

        if (! strip.shownOn)
            return;

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

    static juce::Colour subColour() { return IlanaTheme::accent(); }

    void layoutSubCard()
    {
        auto inner = subCard.reduced (10, 8);
        inner.removeFromTop (18);
        subOn->setBounds (IlanaTheme::cardSwitchBounds (subCard, titleCentreY (subCard)));
        inner.removeFromTop (2);

        // One row across the whole card: the sub's menus and level, and the
        // noise.
        layoutRow (inner, { subShape.get(), subOctave.get(), subLevel.get(), noiseLevel.get() });
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
    SignalFlow patchFlow { processorRef };
    juce::Rectangle<int> patchCard;
    static constexpr int patchMinHeight = 90;
    std::array<bool, OscillatorIds::count> shownStrips {};
    int lastRevealVersion = -1;
    static constexpr int addButtonHeight = 36;
    static constexpr int foldedHeight = 36;
    static constexpr int subCardHeight = 8 + 20 + 13 + 58 + 16 + 12;
    juce::Rectangle<int> subCard;
    std::unique_ptr<ToggleControl> subOn;
    std::unique_ptr<ComboControl> subShape, subOctave;
    std::unique_ptr<KnobControl> subLevel, noiseLevel;
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
                   private IlanaAnim::FrameTimer
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

        // One-click starting points for an empty matrix.
        for (size_t i = 0; i < starterRoutings().size(); ++i)
        {
            const auto& starter = starterRoutings()[i];
            auto button = std::make_unique<juce::TextButton> (starter.label);
            button->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised.interpolatedWith (modSourceColour ((int) starter.source), 0.08f));
            button->setColour (juce::TextButton::textColourOffId, modSourceColour ((int) starter.source).interpolatedWith (juce::Colours::white, 0.4f));
            button->setTooltip (juce::String (starter.tip) + "\nAdds the routing; change it in its row afterwards.");
            button->onClick = [this, i] { addStarter (starterRoutings()[i]); };
            addChildComponent (*button);
            starterButtons.push_back (std::move (button));
        }

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

        paintSectionTitle (g, "MODULATION", juce::Rectangle<int> (headingX, 12, 1000, headingHeight),
                           juce::String (used) + " of " + juce::String (Mod::maxSlots) + " slots in use.   "
                           "Tip: drag a source onto any knob, then drag the coloured dot beside the knob to set the depth.");

        // An empty matrix has no columns to head: just the ways in.
        if (visibleRows.empty())
        {
            paintEmptyState (g);
            return;
        }

        // Column headings, aligned with MatrixRow's layout.
        using C = MatrixRow::Columns;
        auto x = headerArea.getX() + C::number;
        const auto heading = [&g, &x, this] (const char* text, int width, int gapAfter)
        {
            g.drawText (text, juce::Rectangle<int> (x, headerArea.getY(), width, headerArea.getHeight()),
                        juce::Justification::centredLeft);
            x += width + gapAfter;
        };

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        heading ("ON", C::bypass, C::gap + C::meter + C::gap);
        heading ("SOURCE", C::source, C::gap);
        heading ("VIA", C::via, C::gap * 2);
        heading ("AMOUNT", C::amount, C::gap);
        heading ("CURVE", C::curve, C::gap);
        heading ("POLARITY", C::polarity, C::gap * 3);
        heading ("DESTINATION", C::destination, C::gap);
    }

    juce::Rectangle<float> emptyStateCard() const
    {
        const auto area = viewport.getBounds().withTrimmedTop (56);
        return juce::Rectangle<float> (560.0f, 250.0f).withCentre (area.toFloat().getCentre()).withY ((float) area.getY() + 20.0f);
    }

    // An empty matrix explains the three ways in, with a little animated
    // routing and an arrow down to the source chips.
    void paintEmptyState (juce::Graphics& g)
    {
        const auto now = emptyClock;
        const auto area = viewport.getBounds().withTrimmedTop (56);
        const auto card = emptyStateCard();
        IlanaTheme::paintCard (g, card, 10.0f, IlanaTheme::accent().withAlpha (0.3f));

        // Source dot -> animated cable -> knob.
        const auto sourceCentre = juce::Point<float> (card.getX() + 150.0f, card.getY() + 62.0f);
        const auto knobCentre = juce::Point<float> (card.getRight() - 150.0f, card.getY() + 62.0f);
        juce::Path cable;
        cable.startNewSubPath (sourceCentre);
        cable.cubicTo (sourceCentre.translated (80.0f, -40.0f + 10.0f * std::sin (now * 2.0f)),
                       knobCentre.translated (-80.0f, 40.0f - 10.0f * std::sin (now * 2.0f)), knobCentre);
        // Drawn in LFO 1's own colour, as its chip, cable and ring really are.
        const auto sourceColour = modSourceColour ((int) Mod::Source::Lfo1);
        g.setColour (sourceColour.withAlpha (0.45f));
        g.strokePath (cable, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto travel = std::fmod (now * 0.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (cable.getPointAlongPath (travel * cable.getLength())));

        g.setColour (sourceColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText ("LFO 1", juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), juce::Justification::centred);

        const auto knob = juce::Rectangle<float> (34.0f, 34.0f).withCentre (knobCentre);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillEllipse (knob);
        const auto sweep = 0.6f + 0.35f * std::sin (now * 2.0f);
        juce::Path arc;
        arc.addCentredArc (knobCentre.x, knobCentre.y, 21.0f, 21.0f, 0.0f, -2.4f, -2.4f + 4.8f * sweep, true);
        g.setColour (sourceColour);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
        g.drawText ("Nothing is modulated yet", card.withTrimmedTop (104.0f).withHeight (24.0f), juce::Justification::centred);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        const char* const tips[] {
            "Drag a source chip from the bar below onto any knob",
            "or right-click a knob for quick modulation",
            "or press  + ADD MODULATION  above to build a routing here"
        };

        for (int i = 0; i < 3; ++i)
            g.drawText (tips[i], card.withTrimmedTop (136.0f + (float) i * 22.0f).withHeight (20.0f), juce::Justification::centred);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("OR START FROM ONE OF THESE", starterArea().withHeight (16).translated (0, -22), juce::Justification::centred);

        // A chevron bobbing towards the source chips, under the starters
        // (left out when there is no room: it used to sit on a starter).
        const auto bob = 4.0f * std::sin (now * 3.0f);
        const auto tipY = juce::jmax ((float) getHeight() - 22.0f, (float) starterArea().getBottom() + 18.0f);
        if (tipY + 6.0f > (float) getHeight())
            return;
        const auto tip = juce::Point<float> (area.toFloat().getCentreX(), tipY + bob);
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
        area.removeFromTop (32);
        headerArea = area.removeFromTop (18);
        viewport.setBounds (area);
        layoutList();
        layoutStarters();
    }

    // Switching to the tab shows the current routings straight away.
    void visibilityChanged() override
    {
        if (isVisible())
            updateRows();
    }

private:
    static constexpr int rowHeight = 38;

    struct Starter
    {
        const char* label;
        const char* tip;
        Mod::Source source, aux;
        std::vector<Mod::Destination> destinations;
        float depth;
    };

    static const std::vector<Starter>& starterRoutings()
    {
        using S = Mod::Source;
        using D = Mod::Destination;
        static const std::vector<Starter> starters {
            { "LFO 1  >  CUTOFF", "LFO 1 sweeps filter 1's cutoff.", S::Lfo1, S::None, { D::Filter1Cutoff }, 0.3f },
            { "MOD ENV  >  FRAME", "The mod envelope moves OSC 1's wavetable position on every note.", S::ModEnv, S::None, { D::Osc1Frame }, 0.5f },
            { "WHEEL  >  VIBRATO", "LFO 2 wobbles the pitch of OSC 1-3, as far as the mod wheel lets it.", S::Lfo2, S::ModWheel,
              { D::Osc1Pitch, D::Osc2Pitch, D::SubPitch }, 0.006f },
            { "VELOCITY  >  CUTOFF", "Harder notes open filter 1.", S::Velocity, S::None, { D::Filter1Cutoff }, 0.35f },
            { "LFO 2  >  PAN", "LFO 2 moves OSC 1 across the stereo field.", S::Lfo2, S::None, { D::Osc1Pan }, 0.5f },
            { "MACRO 1  >  DRIVE", "Macro 1 drives filter 1.", S::Macro1, S::None, { D::Filter1Drive }, 0.5f }
        };
        return starters;
    }

    juce::Rectangle<int> starterArea() const
    {
        const auto area = viewport.getBounds().withTrimmedTop (56);
        return juce::Rectangle<int> (600, 74).withCentre (area.getCentre()).withY (area.getY() + 20 + 250 + 36);
    }

    void layoutStarters()
    {
        const auto area = starterArea();
        const auto width = area.getWidth() / 3;

        for (size_t i = 0; i < starterButtons.size(); ++i)
            starterButtons[i]->setBounds (juce::Rectangle<int> (area.getX() + (int) (i % 3) * width, area.getY() + (int) (i / 3) * 37,
                                                                width, 37).reduced (4, 3));
    }

    void addStarter (const Starter& starter)
    {
        processorRef.getUndoManager().beginNewTransaction ("Add " + juce::String (starter.label));
        auto next = 0;

        for (const auto destination : starter.destinations)
        {
            for (; next < Mod::maxSlots; ++next)
            {
                const auto slot = processorRef.readModSlot (next);

                if (slot.source == Mod::Source::None && slot.destination == 0)
                    break;
            }

            if (next >= Mod::maxSlots)
                break;

            processorRef.clearModSlot (next);
            processorRef.setModSlotValue (next, "src", (float) starter.source);
            processorRef.setModSlotValue (next, "dst", (float) destination);
            processorRef.setModSlotValue (next, "aux", (float) starter.aux);
            processorRef.setModSlotValue (next, "amt", starter.depth);
            ++next;
        }

        updateRows();
    }

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

        for (auto& button : starterButtons)
            button->setVisible (used.empty());

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
        {
            emptyShownSeconds = 0.0f;
            return;
        }

        if (changeGate.check (processorRef.getUiEpoch()))
            updateRows();

        // The empty state's cable plays for a few seconds after the page
        // opens, and while the mouse is over it, then rests.
        if (visibleRows.empty() && (emptyShownSeconds < 6.0f || isMouseOver (true)))
        {
            emptyShownSeconds += frameSeconds();
            emptyClock += frameSeconds();
            repaint (emptyStateCard().expanded (4.0f).getSmallestIntegerContainer());
        }
    }

    float emptyShownSeconds = 0.0f, emptyClock = 0.0f;
    IlanaAnim::ChangeGate changeGate;

    // Shows the patch's macro names in the source lists.
    void refreshMacroNames()
    {
        juce::StringArray macroNames;

        for (int m = 0; m < Mod::numMacros; ++m)
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
    std::vector<std::unique_ptr<juce::TextButton>> starterButtons;
    juce::Rectangle<int> headerArea;
};

class TapGrid : public juce::Component,
                private IlanaAnim::FrameTimer
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

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
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

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    int hoverStep = -1;
};

// Each effect family has its own colour: drive/distortion warm, modulation
// blue-violet, time and space green-cyan, dynamics teal, filters/EQ pink.
inline juce::Colour fxColour (int type)
{
    // One colour per category, as the library groups them (it read as
    // confetti when colours followed the individual effects).
    switch (type)
    {
        case 13: case 9: case 15: case 11: case 12: case 8: case 22:
            return juce::Colour (0xff5cc4e8); // space: reverb, delay, dimension, smear, freeze, haas, widener
        case 2: case 1: case 3: case 26: case 28: case 30:
            return juce::Colour (0xffff8a5c); // drive: drive, amp, crush, octaver, feedback, airwindows
        case 7: case 6: case 14: case 23: case 24: case 25: case 27: case 5:
            return juce::Colour (0xff9a8cff); // motion: chorus .. comb
        case 16: case 10: case 17:
            return juce::Colour (0xff7ad98e); // rhythm: trance gate, stutter, tape stop
        case 29: case 18: case 4: case 20: case 21: case 19:
            return juce::Colour (0xffe0c35c); // tone & level: eq, tilt, comp, ott, limiter, utility
        default:
            return IlanaTheme::Ui::text3;
    }
}

// Trance gate steps: bar height is each step's level, the playing step
// lights up. Editing a built-in pattern copies it into Custom first.
class GateGrid : public juce::Component,
                 public juce::SettableTooltipClient,
                 private IlanaAnim::FrameTimer
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

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
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

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
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

// An FX slot's on switch for modules without an on parameter of their own:
// on means playing, off bypasses the slot. Drawn like a ToggleControl switch
// (13 px of label space above the pill), so every module's header matches.
class SlotSwitch : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    SlotSwitch (IlanaSynthAudioProcessor& p, int slotIndex)
        : parameter (p.apvts.getParameter ("fx_slot" + juce::String (slotIndex + 1) + "_bypass"))
    {
        setTooltip ("On\nSwitch this effect off to bypass its slot.");
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintSwitch (g, getLocalBounds().withTrimmedTop (13).toFloat(), isOn() ? 1.0f : 0.0f,
                                 IlanaTheme::accent(), isMouseOver() ? 1.0f : 0.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (isOn() ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool isOn() const { return parameter == nullptr || parameter->getValue() < 0.5f; }

    void timerCallback() override
    {
        if (isShowing() && isOn() != shownOn)
        {
            shownOn = isOn();
            repaint();
        }
    }

    juce::RangedAudioParameter* parameter = nullptr;
    bool shownOn = true;
};

class FxPage : public juce::Component,
               private IlanaAnim::FrameTimer
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
          stutterOn (p.apvts, "fx_stutter_on", "ENGAGE"),
          stutterDiv (p.apvts, "fx_stutter_div", "DIV"),
          stutterMix (p.apvts, "fx_stutter_mix", "MIX"),
          smearOn (p.apvts, "fx_smear_on", "ON"),
          smearSize (p.apvts, "fx_smear_size", "GRAIN MS"),
          smearDensity (p.apvts, "fx_smear_density", "DENSITY"),
          smearMix (p.apvts, "fx_smear_mix", "MIX"),
          freezeOn (p.apvts, "fx_freeze_on", "HOLD"),
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
          eqCurve (p),
          awAlgo (p.apvts, "fx_aw_algo", "ALGORITHM"),
          awP1 (p.apvts, "fx_aw_p1", "1"), awP2 (p.apvts, "fx_aw_p2", "2"), awP3 (p.apvts, "fx_aw_p3", "3"),
          awP4 (p.apvts, "fx_aw_p4", "4"), awP5 (p.apvts, "fx_aw_p5", "5"),
          awMix (p.apvts, "fx_aw_mix", "MIX")
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
        slotGroups.push_back ({ &awAlgo, &awP1, &awP2, &awP3, &awP4, &awP5, &awMix });
        // Airwindows: the algorithms grouped by family, not one long list.
        awAlgo.setPopupOverride ([this] { showAirwindowsMenu(); });

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
        IlanaTheme::makePill (chainAButton, IlanaTheme::accent());
        IlanaTheme::makePill (chainBButton, IlanaTheme::accent());
        addAndMakeVisible (chainAButton);
        addAndMakeVisible (chainBButton);
        addAndMakeVisible (copyChainButton);
        addAndMakeVisible (tapGrid);
        tapGrid.setVisible (false);

        // The final stage after the rack.
        softClip = std::make_unique<ToggleControl> (p.apvts, "master_clip", "SOFT CLIP");
        softClip->showAsSwitch();
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
            if (addEffectCard.contains (position))
            {
                if (const auto slot = firstEmptySlot(); slot >= 0)
                    showTypeMenu (slot);

                return;
            }

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

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            slotSwitches[(size_t) slot] = std::make_unique<SlotSwitch> (p, slot);
            stackContent.addChildComponent (*slotSwitches[(size_t) slot]);
        }

        stackView.setViewedComponent (&stackContent, false);
        stackView.setScrollBarsShown (true, false);
        stackView.setScrollBarThickness (8);
        addAndMakeVisible (stackView);

        // Quick picks for an empty rack, every effect grouped by what it
        // does: one click adds it to the first empty slot.
        for (const auto& group : quickAddGroups())
        {
            auto label = std::make_unique<juce::Label> ("", group.title);
            label->setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)));
            label->setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
            label->setJustificationType (juce::Justification::centredRight);
            addChildComponent (*label);
            quickAddLabels.push_back (std::move (label));
        }

        for (const auto& pick : quickAddPicks())
        {
            auto button = std::make_unique<juce::TextButton> (pick.second);
            const auto type = pick.first;
            button->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised.interpolatedWith (fxColour (type), 0.08f));
            button->setColour (juce::TextButton::textColourOffId, fxColour (type).interpolatedWith (juce::Colours::white, 0.35f));
            button->setTooltip ("Add " + juce::String (pick.second).toLowerCase() + " to the first empty slot");
            button->onClick = [this, type]
            {
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                {
                    if (getSlotType (slot) == 0)
                    {
                        processorRef.assignFxSlot (slot + 1, type);
                        selectedSlot = slot;
                        updateVisibility();
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

        paintSectionTitle (g, "CHAIN", { headingX, 17, 200, 14 }); // on the toolbar's centre line

        if (! outputStrip.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, outputStrip.toFloat(), 6.0f);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            IlanaTheme::paintCardTitle (g, outputStrip.withWidth (110).withTrimmedLeft (14), "OUTPUT", IlanaTheme::Ui::text2);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            // Read in order after its controls, like a card's subtitle.
            g.drawText ("after the rack, before the master volume",
                        outputStrip.withLeft (clipGain != nullptr ? clipGain->getRight() + 24 : outputStrip.getX()).withTrimmedRight (14),
                        juce::Justification::centredLeft);
        }

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("BLEND", juce::Rectangle<int> (236, 15, 40, 18), juce::Justification::centredRight);

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto row = rowBounds (slot);

            if (row.isEmpty())
                continue;

            const auto selected = slot == selectedSlot;
            const auto slotType = getSlotType (slot);
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            const auto bypassed = isModuleOff (slot);
            const auto soloed = processorRef.apvts.getParameter (prefix + "_solo")->getValue() > 0.5f;
            const auto dragSource = cardDragActive && slot == selectedSlot;
            const auto dim = bypassed ? 0.45f : 1.0f;

            const auto typeColour = fxColour (slotType);

            if (dragSource)
                g.setColour (IlanaTheme::Ui::bg.withAlpha (0.5f));
            else if (slotType != 0)
            {
                // Loaded modules wear their family colour.
                juce::ColourGradient rowGradient (IlanaTheme::Ui::raised.interpolatedWith (typeColour, selected ? 0.2f : 0.1f).withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  IlanaTheme::Ui::raised.interpolatedWith (typeColour, selected ? 0.1f : 0.04f).withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }
            else
            {
                juce::ColourGradient rowGradient (IlanaTheme::Ui::raised.withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  IlanaTheme::Ui::panel.withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
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
            g.setColour ((selected ? (slotType != 0 ? typeColour : IlanaTheme::accent()) : IlanaTheme::Ui::track).withMultipliedAlpha (dim));
            g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 6.0f, selected ? 1.6f : 1.0f);

            if (dragSource)
                continue;

            g.setColour (juce::Colours::white.withAlpha (0.35f * dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
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

            g.setColour ((slotType != 0 ? IlanaTheme::Ui::text
                                        : IlanaTheme::accent().interpolatedWith (juce::Colours::white, 0.55f)
                                              .withAlpha (0.42f + 0.45f * rowHover[(size_t) slot]))
                             .withMultipliedAlpha (dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, slotType != 0));
            // A slot on one band of the signal says which.
            static const char* const bandTags[] { "", "  LOW", "  MID", "  HIGH", "  M", "  S" };
            const auto bandTag = slotType != 0 ? bandTags[juce::jlimit (0, 5, getSlotBand (slot))] : "";
            g.drawFittedText (slotType != 0 ? getSlotName (slotType) + bandTag : juce::String ("+  add effect"),
                              row.reduced (30, 6).withTrimmedRight (18), 1, juce::Justification::centredLeft);

            if (slotType != 0 && ! bypassed)
            {
                const auto cpu = processorRef.getFxSlotCpu (slot);

                if (cpu > 0.0005f)
                {
                    g.setColour (IlanaTheme::Ui::text3);
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
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
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
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
            const auto height = (float) (rowHeight * numVisibleRows());
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
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
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
        copyChainButton.setBounds (toolbar.removeFromRight (104).reduced (0, 3));
        toolbar.removeFromRight (6);
        chainBButton.setBounds (toolbar.removeFromRight (58).reduced (0, 3));
        toolbar.removeFromRight (4);
        chainAButton.setBounds (toolbar.removeFromRight (58).reduced (0, 3));

        area.removeFromTop (6);

        auto chainColumn = area.removeFromLeft (300);
        rowsTop = chainColumn.getY() + 22; // level with the first effect card (under the hint line)

        area.removeFromLeft (22);

        auto panel = area;
        outputStrip = panel.removeFromBottom (46);
        panel.removeFromBottom (6);
        {
            auto strip = outputStrip.reduced (6, 3);
            strip.removeFromLeft (104); // the tagged OUTPUT title
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
            // A drag reorders among the loaded slots only (fixed for the
            // drag, so it can't walk into the hidden rows).
            dragLimit = juce::jmax (0, numVisibleRows() - 2);
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
        {
            cardDragActive = true;

            // The picks would sit over the dragged row's ghost.
            for (auto& button : quickAddButtons)
                button->setVisible (false);

            for (auto& label : quickAddLabels)
                label->setVisible (false);
        }

        if (! cardDragActive)
            return;

        const auto target = juce::jlimit (0, dragLimit, (event.getPosition().y - rowsTop) / rowHeight);

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

        const auto wasDragging = cardDragActive;
        cardDragActive = false;
        dragImage = {};

        if (wasDragging)
            layoutStack();

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
    // Rows up to the last loaded slot, plus one to add to: empty slots
    // beyond that are not drawn.
    int numVisibleRows() const
    {
        auto last = -1;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) != 0)
                last = slot;

        return juce::jmin (IlanaSynthAudioProcessor::numFxSlots, last + 2);
    }

    juce::Rectangle<int> rowBounds (int slot) const
    {
        if (slot >= numVisibleRows())
            return {};

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

    int getSlotBand (int slot) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_band"))
            return (int) value->load();
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

        // Splitters: the slot works on one band, the rest passes around it.
        juce::PopupMenu bands;
        const juce::StringArray bandNames { "Full signal", "Low band", "Mid band", "High band", "Mid (M/S)", "Side (M/S)" };
        for (int b = 0; b < bandNames.size(); ++b)
            bands.addItem (1100 + b, bandNames[b], true, getSlotBand (slot) == b);
        bands.addSeparator();
        bands.addItem (1110, "Set crossovers...");
        menu.addSubMenu ("Band", bands);

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
                                else if (result >= 1100 && result < 1110)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (slotPrefix + "_band"))
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1100)));
                                }
                                else if (result == 1110)
                                {
                                    safeThis->showCrossovers();
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

    // The splitters' two crossovers, as knobs in a callout.
    void showCrossovers()
    {
        struct Crossovers : public juce::Component
        {
            explicit Crossovers (juce::AudioProcessorValueTreeState& state)
                : low (state, "fx_split_low", "LOW / MID", IlanaTheme::accent(), false),
                  high (state, "fx_split_high", "MID / HIGH", IlanaTheme::accent(), false)
            {
                addAndMakeVisible (low);
                addAndMakeVisible (high);
                setSize (220, 120);
            }
            void resized() override
            {
                auto area = getLocalBounds().reduced (6);
                low.setBounds (area.removeFromLeft (area.getWidth() / 2));
                high.setBounds (area);
            }
            KnobControl low, high;
        };
        juce::CallOutBox::launchAsynchronously (std::make_unique<Crossovers> (processorRef.apvts),
                                                getScreenBounds().withSizeKeepingCentre (10, 10), nullptr);
    }

    void moveSelectedSlot (int direction)
    {
        const auto target = selectedSlot + direction;

        if (target < 0 || target >= IlanaSynthAudioProcessor::numFxSlots)
            return;

        // A slot is its module plus its bypass, solo and blend settings; move
        // them together so a dragged slot keeps how it was set up.
        for (const auto* suffix : { "", "_bypass", "_solo", "_mix", "_band" })
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

        updateAirwindowsKnobs (shown[30]);
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
    std::vector<std::unique_ptr<juce::Label>> quickAddLabels;

    void layoutStack()
    {
        stackPanels.clear();

        for (auto& slotSwitch : slotSwitches)
            slotSwitch->setVisible (false);
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
            // (The on switch goes in the header, so it doesn't count.)
            const auto rowItems = std::count_if (group.begin(), group.end(), [type] (juce::Component* item)
            {
                if (type == 30 && ! item->isVisible())
                    return false;
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                return toggle == nullptr || ! toggle->isSwitch();
            });
            const auto rows = rowItems > 8 ? 2 : 1;
            auto height = 30 + (panel.duplicate ? 30 : rows * 112 + 8);

            if (! panel.duplicate && type == 29)
                height += 130;

            if (! panel.duplicate && type == 9)
                height += 58;

            if (! panel.duplicate && type == 16)
                height += 78;

            panel.bounds = { 0, y, width, height };
            y += height + 8;

            // Duplicates share settings but bypass on their own.
            if (panel.duplicate)
            {
                slotSwitches[(size_t) slot]->setVisible (true);
                slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
            }

            if (! panel.duplicate)
            {
                auto body = panel.bounds.reduced (10, 0);
                body.removeFromTop (30);

                // The module's on switch sits in its header (left of SLOT n),
                // as on the oscillator and body cards; the rest fill the rows.
                std::vector<juce::Component*> items;
                ToggleControl* power = nullptr;

                for (auto* item : group)
                {
                    auto* toggle = dynamic_cast<ToggleControl*> (item);

                    if (type == 30 && ! item->isVisible())
                        continue; // an Airwindows knob the algorithm doesn't use

                    if (toggle != nullptr && toggle->isSwitch() && power == nullptr)
                        power = toggle;
                    else
                        items.push_back (item);
                }

                // One on switch per module, always at the header's switch
                // place: its own on parameter, or else the slot's bypass.
                if (enableParamFor (type) != nullptr && power != nullptr)
                    power->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
                else
                {
                    if (power != nullptr)
                        items.insert (items.begin(), power);

                    slotSwitches[(size_t) slot]->setVisible (true);
                    slotSwitches[(size_t) slot]->setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, panel.bounds.getY() + 14));
                }

                if (items.size() > 8)
                {
                    const auto half = (int) (items.size() + 1) / 2;
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (items.begin(), items.begin() + half));
                    layoutRow (body.removeFromTop (112), std::vector<juce::Component*> (items.begin() + half, items.end()));
                }
                else
                {
                    // A short row is centred in the card, not pinned left.
                    const auto maxWidth = juce::jmin (body.getWidth(), (int) items.size() * 120);
                    layoutRow (body.removeFromTop (112).withSizeKeepingCentre (maxWidth, 112), items);
                }

                if (type == 29)
                    eqCurve.setBounds (body.removeFromTop (126).reduced (0, 2));

                if (type == 9)
                    tapGrid.setBounds (body.removeFromTop (54).reduced (0, 2));

                if (type == 16)
                    gateGrid->setBounds (body.removeFromTop (74).reduced (0, 2));

                if (type == 13)
                    loadIrButton.setBounds (IlanaTheme::cardSwitchBounds (panel.bounds, 0).getX() - 8 - 96, panel.bounds.getY() + 5, 96, 18);
            }

            stackPanels.push_back (panel);
        }

        updateModuleDimming();

        // Like PLAY's ADD OSCILLATOR: a quiet card after the last effect, while
        // the rack has room and the library isn't showing beside it (one
        // way in besides the slot rows, not two).
        addEffectCard = {};

        if (! stackPanels.empty() && firstEmptySlot() >= 0 && ! libraryFits())
        {
            addEffectCard = { 0, y, width, 44 };
            y += 44 + 8;
        }

        stackContent.setSize (width, juce::jmax (y, stackView.getHeight()));
        stackContent.repaint();

        // The library sits under the rack's rows in every state (three
        // across, a small heading per group) while it fits.
        size_t pick = 0;
        auto column = libraryColumn();
        const auto fits = libraryFits();

        for (size_t group = 0; group < quickAddGroups().size(); ++group)
        {
            quickAddLabels[group]->setVisible (fits);

            if (fits)
            {
                quickAddLabels[group]->setJustificationType (juce::Justification::bottomLeft);
                quickAddLabels[group]->setBounds (column.removeFromTop (libraryHeadingHeight).withTrimmedLeft (2));
            }

            const auto count = quickAddGroups()[group].count;

            for (int i = 0; i < count; ++i, ++pick)
            {
                auto& button = *quickAddButtons[pick];
                button.setVisible (fits);

                if (fits)
                {
                    if (i % libraryAcross == 0 && i > 0)
                        column.removeFromTop (libraryButtonHeight);

                    const auto cellWidth2 = column.getWidth() / libraryAcross;
                    button.setBounds (juce::Rectangle<int> (column.getX() + (i % libraryAcross) * cellWidth2, column.getY(),
                                                            cellWidth2, libraryButtonHeight).reduced (2, 2));
                }
            }

            if (fits)
                column.removeFromTop (libraryButtonHeight + 2);
        }
    }

    struct QuickAddGroup
    {
        const char* title;
        int count;
    };

    // Groups, in order, and how many of quickAddPicks() each takes.
    static const std::vector<QuickAddGroup>& quickAddGroups()
    {
        static const std::vector<QuickAddGroup> groups { { "SPACE", 7 }, { "DRIVE", 6 }, { "MOTION", 8 },
                                                         { "RHYTHM", 3 }, { "TONE & LEVEL", 6 } };
        return groups;
    }

    static const std::vector<std::pair<int, const char*>>& quickAddPicks()
    {
        static const std::vector<std::pair<int, const char*>> picks {
            { 13, "REVERB" }, { 9, "DELAY" }, { 15, "DIMENSION" }, { 11, "SMEAR" }, { 12, "FREEZE" }, { 8, "HAAS" }, { 22, "WIDENER" },
            { 2, "DRIVE" }, { 1, "AMP" }, { 3, "CRUSH" }, { 26, "OCTAVER" }, { 28, "FEEDBACK" }, { 30, "AIRWINDOWS" },
            { 7, "CHORUS" }, { 6, "PHASER" }, { 14, "FLANGER" }, { 23, "TREMOLO" }, { 24, "FREQ SHIFT" }, { 25, "RING MOD" },
            { 27, "VOWEL" }, { 5, "COMB" },
            { 16, "TRANCE GATE" }, { 10, "STUTTER" }, { 17, "TAPE STOP" },
            { 29, "EQ" }, { 18, "TILT" }, { 4, "COMP" }, { 20, "OTT" }, { 21, "LIMITER" }, { 19, "UTILITY" }
        };
        return picks;
    }

    int firstEmptySlot() const
    {
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if (getSlotType (slot) == 0)
                return slot;

        return -1;
    }

    // Where the effect library goes: under the rack's slot rows.
    juce::Rectangle<int> libraryColumn() const
    {
        auto column = juce::Rectangle<int> (14, rowsTop + numVisibleRows() * rowHeightOfSlots + 14, 300, 0);
        column.setBottom (getHeight() - 12);
        return column;
    }

    static constexpr int libraryHeadingHeight = 14, libraryButtonHeight = 22, libraryAcross = 3;

    bool libraryFits() const
    {
        auto needed = 0;

        for (const auto& group : quickAddGroups())
            needed += libraryHeadingHeight + (group.count + libraryAcross - 1) / libraryAcross * libraryButtonHeight + 2;

        return needed <= libraryColumn().getHeight();
    }

    void paintStack (juce::Graphics& g)
    {
        if (! addEffectCard.isEmpty())
        {
            const auto card = addEffectCard.toFloat().reduced (0.5f);
            g.setColour (IlanaTheme::Ui::panel.withAlpha (0.6f));
            g.fillRoundedRectangle (card, 8.0f);
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (card, 8.0f, 1.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("+  ADD EFFECT", addEffectCard, juce::Justification::centred);
        }

        if (stackPanels.empty())
        {
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            const auto middle = stackContent.getHeight() / 2 - 40;
            g.drawText ("The rack is empty", stackContent.getLocalBounds().withY (middle).withHeight (24),
                        juce::Justification::centred);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Click an effect in the library on the left to add it, or click the slot.",
                        stackContent.getLocalBounds().withY (middle + 28).withHeight (20), juce::Justification::centred);
            return;
        }

        for (const auto& panel : stackPanels)
        {
            const auto colour = fxColour (panel.type);
            const auto bounds = panel.bounds.toFloat();
            const auto selected = panel.slot == selectedSlot;
            const auto off = isModuleOff (panel.slot);

            if (selected && ! off)
                IlanaTheme::paintGlow (g, bounds, 8.0f, colour, 1.0f);

            IlanaTheme::paintCard (g, bounds, 8.0f, off ? IlanaTheme::Ui::line : colour);

            if (selected)
            {
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.2f);
            }

            auto header = panel.bounds.withHeight (28).reduced (12, 0);
            IlanaTheme::paintCardHeader (g, header, getSlotName (panel.type).toUpperCase(),
                                         juce::String ("slot ") + juce::String (panel.slot + 1) + (off ? "  -  off" : ""),
                                         off ? IlanaTheme::Ui::text3 : colour, panel.type == 13 ? 220 : 110);

            if (panel.duplicate)
            {
                g.setColour (IlanaTheme::Ui::text3);
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
        if (! isShowing())
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

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        const auto ticks = frameTicks();
        // The modulation effects' swinging dots move while there is sound.
        if (processorRef.getOutputPeak() > 1.0e-4f)
            meterPhase += 0.12f * ticks;
        dropFlash = IlanaAnim::decay (dropFlash, 0.85f, ticks);
        chainSweep = IlanaAnim::decay (chainSweep, 0.9f, ticks);
        paramsAppear = juce::jmin (1.0f, paramsAppear + 0.1f * ticks);
        auto hoverMoving = false;

        const auto showingA = processorRef.isShowingChainA();

        if (chainAButton.getToggleState() != showingA)
        {
            chainAButton.setToggleState (showingA, juce::dontSendNotification);
            chainBButton.setToggleState (! showingA, juce::dontSendNotification);
        }

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto target = slot == hoveredRow ? 1.0f : 0.0f;
            hoverMoving = hoverMoving || std::abs (rowHover[(size_t) slot] - target) > 0.005f;
            rowHover[(size_t) slot] = IlanaAnim::approach (rowHover[(size_t) slot], target, 0.25f, ticks);

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

        signature += juce::String (airwindowsAlgorithm()); // its knobs differ

        if (signature != lastSignature)
        {
            lastSignature = signature;

            // The selection stays on a drawn row.
            if (selectedSlot >= numVisibleRows())
            {
                selectedSlot = juce::jmax (0, numVisibleRows() - 1);
                bindBlend();
            }

            updateVisibility();
        }

        // The rows animate (meters, hover, flashes); the rest of the page is
        // still, so only the rack column repaints unless a row is dragged.
        const auto animating = hoverMoving || dropFlash > 0.01f || chainSweep > 0.01f || paramsAppear < 1.0f;

        if (cardDragActive)
            repaint();
        else if (animating || changeGate.check (processorRef.getUiEpoch()))
        {
            repaint (juce::Rectangle<int> (0, rowsTop - 10, 336, numVisibleRows() * rowHeight + 20));
            // A module switched on or off: its card and controls follow.
            if (updateModuleDimming())
                stackContent.repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoveredRow != -1)
        {
            hoveredRow = -1;
            repaint();
        }
    }

    int dragLimit = 0;
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
    juce::Rectangle<int> addEffectCard;
    std::array<std::unique_ptr<SlotSwitch>, IlanaSynthAudioProcessor::numFxSlots> slotSwitches;

    // The module's own on parameter, where it has one.
    static const char* enableParamFor (int type)
    {
        switch (type)
        {
            case 2:  return "fx_drive_on";
            case 3:  return "fx_crush_on";
            case 5:  return "fx_comb_on";
            case 6:  return "fx_phaser_on";
            case 7:  return "fx_chorus_on";
            case 9:  return "fx_delay_on";
            case 11: return "fx_smear_on";
            case 13: return "fx_reverb_on";
            default: return nullptr;
        }
    }

    // Off: the slot is bypassed, or the module's own switch is off.
    bool isModuleOff (int slot) const
    {
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        if (processorRef.apvts.getRawParameterValue (prefix + "_bypass")->load() > 0.5f)
            return true;

        if (const auto* id = enableParamFor (getSlotType (slot)))
            if (const auto* value = processorRef.apvts.getRawParameterValue (id))
                return value->load() < 0.5f;

        return false;
    }

    // An effect that is off dims its controls (the arcs go grey).
    bool updateModuleDimming()
    {
        auto changed = false;

        for (const auto& panel : stackPanels)
        {
            if (panel.duplicate)
                continue;

            const auto alpha = isModuleOff (panel.slot) ? 0.45f : 1.0f;

            for (auto* item : slotGroups[(size_t) panel.type])
            {
                auto* toggle = dynamic_cast<ToggleControl*> (item);
                const auto isPower = toggle != nullptr && toggle->isSwitch() && item->getY() < panel.bounds.getY() + 20;

                if (! isPower && item->getAlpha() != alpha)
                {
                    item->setAlpha (alpha);
                    changed = true;
                }
            }
        }

        return changed;
    }
    std::vector<StackPanel> stackPanels;
    int selectedSlot = 0;
    static constexpr int rowHeight = 39;
    static constexpr int rowHeightOfSlots = rowHeight;
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
    // RACK A / B, named apart from the header's COMPARE A/B.
    juce::TextButton chainAButton { "RACK A" };
    juce::TextButton chainBButton { "RACK B" };
    juce::TextButton copyChainButton { "COPY TO OTHER" };
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
    ComboControl awAlgo;
    KnobControl awP1, awP2, awP3, awP4, awP5, awMix;

    int airwindowsAlgorithm() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_aw_algo"))
            return juce::jlimit (0, airwindows::count() - 1, (int) value->load());

        return 0;
    }

    // The chosen algorithm's knobs carry its own names; the rest hide.
    void updateAirwindowsKnobs (bool loaded)
    {
        const auto& info = airwindows::registry()[(size_t) airwindowsAlgorithm()];
        KnobControl* knobs[] { &awP1, &awP2, &awP3, &awP4, &awP5 };

        for (int k = 0; k < airwindows::Module::numKnobs; ++k)
        {
            const auto used = k < info.numKnobs;
            if (used)
                knobs[k]->setLabelText (juce::String (info.knobs[k].name).toUpperCase());
            knobs[k]->setVisible (loaded && used);
        }
    }

    // The algorithms by family; picking one sets its knobs to the plugin's
    // own defaults.
    void showAirwindowsMenu()
    {
        juce::PopupMenu menu;
        const auto& list = airwindows::registry();
        const auto chosen = airwindowsAlgorithm();
        juce::StringArray categories;

        for (const auto& info : list)
            categories.addIfNotAlreadyThere (info.category);

        for (const auto& category : categories)
        {
            juce::PopupMenu family;
            auto holdsChosen = false;

            for (int i = 0; i < (int) list.size(); ++i)
            {
                if (category == list[(size_t) i].category)
                {
                    family.addItem (i + 1, list[(size_t) i].name, true, i == chosen);
                    holdsChosen = holdsChosen || i == chosen;
                }
            }

            menu.addSubMenu (category, family, true, nullptr, holdsChosen);
        }

        juce::Component::SafePointer<FxPage> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&awAlgo),
                            [safeThis] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->chooseAirwindows (result - 1);
                            });
    }

    void chooseAirwindows (int algorithm)
    {
        algorithm = juce::jlimit (0, airwindows::count() - 1, algorithm);
        const auto& info = airwindows::registry()[(size_t) algorithm];
        processorRef.preloadAirwindows (algorithm);

        const auto set = [this] (const juce::String& id, float value)
        {
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                parameter->endChangeGesture();
            }
        };

        for (int k = 0; k < info.numKnobs; ++k)
            set ("fx_aw_p" + juce::String (k + 1), info.knobs[k].defaultValue);

        set ("fx_aw_algo", (float) algorithm);
        updateVisibility();
        repaint();
    }
};
} // namespace

// The scope as a panel floating over the current page (bottom right), or
// expanded over the whole page area.
class ScopePanel : public juce::Component
{
public:
    explicit ScopePanel (IlanaSynthAudioProcessor& p)
        : scope (p)
    {
        addAndMakeVisible (scope);

        expandButton.setButtonText ("EXPAND");
        expandButton.setClickingTogglesState (true);
        expandButton.setTooltip ("Fill the page area with the scope");
        expandButton.onClick = [this]
        {
            expandButton.setButtonText (expandButton.getToggleState() ? "SHRINK" : "EXPAND");

            if (onExpand != nullptr)
                onExpand();
        };

        closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        closeButton.setTooltip ("Close the scope");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        addAndMakeVisible (expandButton);
        addAndMakeVisible (closeButton);
    }

    std::function<void()> onClose, onExpand;

    bool isExpanded() const { return expandButton.getToggleState(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        // A floating layer: a soft dark shadow under it, so the cards it
        // covers read as behind it rather than cut off.
        for (int ring = 4; ring >= 1; --ring)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRoundedRectangle (bounds.expanded ((float) ring).translated (0.0f, 2.0f), 8.0f + (float) ring);
        }
        IlanaTheme::paintGlow (g, bounds, 8.0f, IlanaTheme::accent(), 0.9f);
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (IlanaTheme::Ui::line.interpolatedWith (IlanaTheme::accent(), 0.4f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        IlanaTheme::paintTag (g, { bounds.getX() + 15.0f, bounds.getY() + 15.0f }, IlanaTheme::accent());
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("SCOPE", bounds.withTrimmedLeft (26).withHeight (30), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        auto header = area.removeFromTop (30).reduced (8, 5);
        closeButton.setBounds (header.removeFromRight (22));
        header.removeFromRight (6);
        expandButton.setBounds (header.removeFromRight (66));
        scope.setBounds (area.reduced (6, 0).withTrimmedBottom (6));
    }

private:
    ScopeDisplay scope;
    juce::TextButton expandButton, closeButton;
};

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
    logo.isSounding = [this] { return processorRef.getActiveVoiceCount() > 0; };
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

    // Seven tabs; the ones holding several pages switch them from the tab
    // row (PLAY: overview and vector, OSC: oscillators and the physical
    // view, MOD: envelopes and LFOs, step LFOs and MSEG, the matrix).
    const auto addSection = [this] (const juce::String& name, std::initializer_list<std::tuple<juce::String, juce::String, juce::Component*>> pages)
    {
        auto* section = new SectionPage();

        for (const auto& [id, label, page] : pages)
            section->addPage (id, label, page);

        // The page slides in as a whole; its displays don't replay their
        // own entrances on top.
        section->onPageShown = [this] (juce::Component& page) { startTabTransition (&page); };
        section->switcher.onSelect = [this, section] (int index)
        {
            section->show (index, true);
        };
        tabs.addTab (name, IlanaTheme::Ui::panel, section, true);
        sections.push_back (section);
    };

    addSection ("PLAY", { { "MAIN", "OVERVIEW", mainPage }, { "VECTOR", "VECTOR", new VectorPage (p) } });
    addSection ("OSC", { { "OSC", "OSCILLATORS", new OscPageViewport (p) }, { "PHYSICAL", "PHYSICAL", new PhysicalPage (p) } });
    addSection ("FILTER", { { "FILTER", "FILTER", new FilterPage (p) } });
    addSection ("MOD", { { "ENV/LFO", "ENV / LFO", envLfoPage },
                         { "STEPS", "STEPS & MSEG", new SeqPage (p, SeqPage::Part::modulators) },
                         { "MATRIX", "MATRIX", new MatrixPage (p) } });
    addSection ("FM", { { "FM", "FM", new FmPage (p) } });
    addSection ("SEQ", { { "ARP/SEQ", "SEQ", new SeqPage (p, SeqPage::Part::notes) } });
    addSection ("FX", { { "FX", "FX", new FxPage (p) } });
    // M7.5: ilanaSynth FX adds its INPUT page (last, so tab shortcuts stay).
    if (IlanaSynthAudioProcessor::isEffectBuild)
        addSection ("INPUT", { { "INPUT", "INPUT", new InputPage (p) } });

    mainPage->onEditLfo = [this, envLfoPage] (int lfo)
    {
        envLfoPage->selectLfo (lfo);
        showPage ("ENV/LFO");
    };

    mainPage->onEditEnvelope = [this, envLfoPage] (int envelope)
    {
        envLfoPage->selectEnvelope (envelope);
        showPage ("ENV/LFO");
    };

    mainPage->onOpenPage = [this] (const juce::String& name) { showPage (name); };

    // The scope floats over any page.
    scopePanel = std::make_unique<ScopePanel> (p);
    static_cast<ScopePanel*> (scopePanel.get())->onClose = [this] { setScopeOpen (false); };
    static_cast<ScopePanel*> (scopePanel.get())->onExpand = [this] { resized(); };
    scopeButton.setClickingTogglesState (true);
    scopeButton.setTooltip ("Scope\nShow the oscilloscope and spectrum over any page.");
    scopeButton.onClick = [this] { setScopeOpen (scopeButton.getToggleState()); };

    content.addAndMakeVisible (tabs);

    // In front of the tab bar: the page switches, the scope button and panel.
    for (auto* section : sections)
        content.addChildComponent (section->switcher);

    content.addAndMakeVisible (scopeButton);
    content.addChildComponent (*scopePanel);

    // Bottom strip: macros, then performance controls, then master.
    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        auto knob = std::make_unique<StripKnob> (p, "macro" + juce::String (macro + 1),
                                                 "Macro " + juce::String (macro + 1), macro,
                                                 juce::Colour (0xffffd447), false);
        content.addChildComponent (*knob);
        macroKnobs.push_back (std::move (knob));
    }
    // Four macros fit the strip: a small switch pages between 1-4 and 5-8.
    macroPageButton.setTooltip ("Show macros 1-4 or 5-8");
    macroPageButton.onClick = [this] { showMacroPage (1 - macroPage); };
    content.addAndMakeVisible (macroPageButton);
    showMacroPage (0);

    glideKnob = std::make_unique<StripKnob> (p, "glide", "Glide");
    legatoToggle = std::make_unique<ToggleControl> (p.apvts, "glide_legato", "LEGATO");
    legatoToggle->showAsSwitch();
    legatoToggle->setTooltip ("Glide only between overlapping (legato) notes");
    content.addAndMakeVisible (*legatoToggle);
    bendKnob = std::make_unique<StripKnob> (p, "bend_range", "Bend");
    // Settings rather than sound controls: neutral, so they don't outshine
    // the page (and yellow stays the macros' colour).
    voicesKnob = std::make_unique<StripKnob> (p, "poly_voices", "Voices", -1, IlanaTheme::Ui::text2, false);
    masterKnob = std::make_unique<StripKnob> (p, "master", "Master", -1, IlanaTheme::Ui::text2, false);
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
    abButton.setButtonText ("A/B:  A");
    abButton.setTooltip ("Compare\nFlip between two versions of the patch (A and B) to compare them.");
    abButton.onClick = [this] { toggleAB(); };
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
    struct ChipSpec
    {
        juce::String name;
        Mod::Source source;
        int revealKind = -1, revealIndex = 0;
    };
    std::vector<ChipSpec> chipSpecs {
        { "LFO 1", Mod::Source::Lfo1 }, { "LFO 2", Mod::Source::Lfo2 }, { "LFO 3", Mod::Source::Lfo3 },
        { "LFO 4", Mod::Source::Lfo4 }
    };
    // The LFO pool and ENV 6-16 after their fixed neighbours, shown once added.
    for (int lfo = 4; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        chipSpecs.push_back ({ "LFO " + juce::String (lfo + 1), Mod::lfoSourceFor (lfo),
                               (int) IlanaSynthAudioProcessor::Module::Lfo, lfo });
    for (const auto& spec : { ChipSpec { "MOD ENV", Mod::Source::ModEnv }, ChipSpec { "FILT ENV", Mod::Source::FilterEnv },
                              ChipSpec { "FILT 2 ENV", Mod::Source::FilterEnv2 }, ChipSpec { "ENV 5", Mod::Source::Env4 } })
        chipSpecs.push_back (spec);
    for (int env = 6; env <= 16; ++env)
        chipSpecs.push_back ({ "ENV " + juce::String (env), (Mod::Source) ((int) Mod::Source::Env6 + env - 6),
                               (int) IlanaSynthAudioProcessor::Module::Envelope, env - 1 });
    for (const auto& spec : { ChipSpec { "MSEG", Mod::Source::Mseg }, ChipSpec { "VELOCITY", Mod::Source::Velocity },
                              ChipSpec { "KEY", Mod::Source::KeyTrack }, ChipSpec { "RANDOM", Mod::Source::Random },
                              ChipSpec { "WHEEL", Mod::Source::ModWheel }, ChipSpec { "PRESSURE", Mod::Source::Aftertouch },
                              ChipSpec { "INPUT", Mod::Source::InputEnv } })
        chipSpecs.push_back (spec);

    for (const auto& spec : chipSpecs)
    {
        // The input's envelope only exists in ilanaSynth FX.
        if (spec.source == Mod::Source::InputEnv && ! IlanaSynthAudioProcessor::isEffectBuild)
            continue;

        auto chip = std::make_unique<ModSourceChip> (spec.name, (int) spec.source);
        // The chip glows with its source while that source modulates
        // something (macros, the wheel and pressure always).
        chip->valueProvider = [this, source = spec.source]
        {
            using S = Mod::Source;
            const auto played = Mod::macroIndexFor (source) >= 0 || source == S::ModWheel || source == S::Aftertouch
                                || source == S::InputEnv;

            if (! played && ! usedModSources[(size_t) juce::jlimit (0, (int) S::Count - 1, (int) source)])
                return 0.0f;

            if ((source == S::Velocity || source == S::KeyTrack || source == S::Random)
                && processorRef.getActiveVoiceCount() == 0)
                return 0.0f;

            return processorRef.getSourceDisplayValue ((int) source);
        };
        if (spec.revealKind >= 0)
            chip->setShortName ((spec.revealKind == (int) IlanaSynthAudioProcessor::Module::Lfo ? "L" : "E")
                                + juce::String (spec.revealIndex + 1));
        content.addAndMakeVisible (*chip);
        chip->setVisible (spec.revealKind < 0);
        chips.push_back (std::move (chip));
        chipReveal.push_back ({ spec.revealKind, spec.revealIndex });
        chipWanted.push_back (spec.revealKind < 0);
    }

    moreChipsButton.setTooltip ("More added LFOs and envelopes than fit here: drag them from their cards on MOD > ENV / LFO.");
    moreChipsButton.onClick = [this] { showPage ("ENV/LFO"); };
    content.addChildComponent (moreChipsButton);

    tutorial.setPresetCount (processorRef.getFactoryPresetNames().size());
    content.addAndMakeVisible (tutorial);

    // Applied after adding: addAndMakeVisible forces the component visible.
    tutorial.setVisible (! settings->getBoolValue ("seenIntro", false));

    if (tutorial.isVisible())
        tutorial.toFront (false);

    // The keyboard opens on request (KEYS), so the pages get the room.
    keyboardVisible = settings->getBoolValue ("showKeyboard", false);
    keysButton.setToggleState (keyboardVisible, juce::dontSendNotification);
    keyboard->setVisible (keyboardVisible);

    updateHeaderButtons();
    updateUndoButtons();
    adoptLoadedFingerprint();

    // Drag the corner (or the host's window edge) to any size between 75% and
    // 200%; the aspect ratio is fixed and the size is remembered.
    setResizable (true, true);

    startTimer (250);
    frameSource = std::make_unique<IlanaAnim::FrameClock::Source> (*this);
    animator.onFrame = [this] { animate(); };

    applyUiZoom ((float) settings->getDoubleValue ("uiZoom", 1.0));
    displayScaleApplied = settings->containsKey ("uiZoom") && ! juce::approximatelyEqual (uiZoom, 1.0f);

    tabs.getTabbedButtonBar().addChangeListener (this);
}

void IlanaSynthAudioProcessorEditor::openWavetableEditor (int slot, juce::Colour colour)
{
    closeWavetableEditor();
    wavetableEditor = std::make_unique<WavetableEditor> (processorRef, slot, colour);
    wavetableEditor->onClose = [safe = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)]
    {
        if (safe != nullptr)
            safe->closeWavetableEditor();
    };
    content.addAndMakeVisible (*wavetableEditor);
    wavetableEditor->setBounds (content.getLocalBounds().reduced (24, 62));
    wavetableEditor->grabKeyboardFocus();
}

void IlanaSynthAudioProcessorEditor::closeWavetableEditor()
{
    if (wavetableEditor != nullptr)
    {
        content.removeChildComponent (wavetableEditor.get());
        wavetableEditor.reset();
    }
}

IlanaSynthAudioProcessorEditor::~IlanaSynthAudioProcessorEditor()
{
    closeWavetableEditor();
    tabs.getTabbedButtonBar().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void IlanaSynthAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    layoutTabRow();

    if (auto* page = tabs.getCurrentContentComponent())
        page->repaint();

    startTabTransition();
}

void IlanaSynthAudioProcessorEditor::startTabTransition (juce::Component* pageToAnimate)
{
    if (transitionPage != nullptr)
    {
        transitionPage->setTransform ({});
        transitionPage->setAlpha (1.0f);
        transitionPage = nullptr;
    }

    if (auto* page = pageToAnimate != nullptr ? pageToAnimate : tabs.getCurrentContentComponent())
    {
        transitionPage = page;
        transitionStart = juce::Time::getMillisecondCounterHiRes();
        page->setAlpha (0.0f);
        page->setTransform (juce::AffineTransform::translation (0.0f, 8.0f));
        animator.startTimerHz (60);
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

// Pool chips follow the modules added (and whatever the matrix uses), read
// against last tick's usage; the row re-lays out only when that changes.
void IlanaSynthAudioProcessorEditor::updateChipVisibility()
{
    auto changed = false;

    for (size_t i = 0; i < chips.size() && i < chipReveal.size(); ++i)
    {
        const auto [kind, index] = chipReveal[i];

        if (kind < 0)
            continue;

        const auto source = juce::jlimit (0, (int) Mod::Source::Count - 1, chips[i]->getSourceIndex());
        const auto wanted = processorRef.isRevealed ((IlanaSynthAudioProcessor::Module) kind, index) || usedModSources[(size_t) source];

        if (chipWanted[i] != wanted)
        {
            chipWanted[i] = wanted;
            changed = true;
        }
    }

    if (changed)
        resized();
}

// Every chip at its name's width with the spare shared out. When the added
// LFOs and envelopes crowd the row they shorten to "L5" / "E6", and any that
// still don't fit wait behind a "+N" (their cards drag just the same).
void IlanaSynthAudioProcessorEditor::layoutChips (juce::Rectangle<int> row)
{
    if (chips.empty())
        return;

    const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
    const auto widthOf = [&font] (const juce::String& text) { return (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 34.0f; };
    const auto isPool = [this] (size_t i) { return chipReveal[i].first >= 0; };

    std::vector<bool> shown (chipWanted.begin(), chipWanted.end());
    auto compact = false;
    const auto total = [&]
    {
        auto sum = 0.0f;
        for (size_t i = 0; i < chips.size(); ++i)
            if (shown[i])
                sum += isPool (i) && compact ? widthOf (chips[i]->getShortName()) : widthOf (chips[i]->getSourceName());
        return sum;
    };

    const auto available = (float) row.getWidth();
    compact = total() > available;
    auto hidden = 0;
    const auto moreWidth = 44.0f;

    for (auto i = (int) chips.size() - 1; i >= 0 && compact && total() + (hidden > 0 ? moreWidth : 0.0f) > available; --i)
        if (shown[(size_t) i] && isPool ((size_t) i))
        {
            shown[(size_t) i] = false;
            ++hidden;
        }

    const auto used = total() + (hidden > 0 ? moreWidth : 0.0f);
    const auto count = (float) std::count (shown.begin(), shown.end(), true) + (hidden > 0 ? 1.0f : 0.0f);
    const auto spare = juce::jmax (0.0f, (available - used) / juce::jmax (1.0f, count));
    const auto squeeze = juce::jmin (1.0f, available / juce::jmax (1.0f, used));
    auto x = (float) row.getX();

    for (size_t i = 0; i < chips.size(); ++i)
    {
        chips[i]->setVisible (shown[i]);
        chips[i]->setCompact (compact && isPool (i));

        if (! shown[i])
            continue;

        const auto natural = compact && isPool (i) ? widthOf (chips[i]->getShortName()) : widthOf (chips[i]->getSourceName());
        const auto width = (natural + spare) * squeeze;
        chips[i]->setBounds (juce::Rectangle<float> (x, (float) row.getY(), width, (float) row.getHeight()).toNearestInt().reduced (2, 1));
        x += width;
    }

    moreChipsButton.setVisible (hidden > 0);
    moreChipsButton.setButtonText ("+" + juce::String (hidden));
    if (hidden > 0)
        moreChipsButton.setBounds (juce::Rectangle<float> (x, (float) row.getY(), (moreWidth + spare) * squeeze, (float) row.getHeight())
                                       .toNearestInt().reduced (2, 1));
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

    // "Edited" marker: cheap checksum of every parameter against the one
    // taken when the preset was loaded.
    if (presetLoadFlash <= 0.01f)
        presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                                 parameterFingerprint() != loadedFingerprint);

    // Which sources the matrix uses, for the source chips' glow.
    updateChipVisibility();
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
    }
}

// Runs on every display frame while the page slides in or the preset
// display flashes, then stops.
void IlanaSynthAudioProcessorEditor::animate()
{
    if (presetLoadFlash > 0.0f)
    {
        // Tuned at one 0.86 step per 60 ms.
        presetLoadFlash = IlanaAnim::decay (presetLoadFlash, 0.86f, animator.frameSeconds() * 1000.0f / 60.0f);

        if (presetLoadFlash <= 0.01f)
            presetLoadFlash = 0.0f;

        presetDisplay.setFlash (presetLoadFlash);
    }

    if (transitionPage != nullptr)
    {
        // A short slide up with a fade: no scaling, which would soften text.
        constexpr double duration = 160.0;
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - transitionStart;
        const auto progress = juce::jlimit (0.0, 1.0, elapsed / duration);
        const auto eased = IlanaAnim::easeOutCubic ((float) progress);

        transitionPage->setAlpha (eased);
        transitionPage->setTransform (juce::AffineTransform::translation (0.0f, std::round ((1.0f - eased) * 8.0f)));

        if (progress >= 1.0)
        {
            transitionPage->setTransform ({});
            transitionPage->setAlpha (1.0f);
            transitionPage = nullptr;
        }
    }

    if (transitionPage == nullptr && presetLoadFlash <= 0.0f)
        animator.stopTimer();
}

void IlanaSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());
}

void IlanaSynthAudioProcessorEditor::paintHeader (juce::Graphics& g)
{
    g.setColour (IlanaTheme::Ui::header);
    g.fillRect (juce::Rectangle<int> (0, 0, designWidth, 56));

    g.setColour (IlanaTheme::Ui::line);
    g.fillRect (juce::Rectangle<int> (0, 55, designWidth, 1));

    // Status line along the bottom edge of the header (tempo, voices and
    // CPU at the right), clear of the buttons above.
    const auto statusY = 41;


    const auto cpu = processorRef.getCpuUsage() * 100.0f;

    const auto cpuColour = cpu < 30.0f
                               ? IlanaTheme::Ui::text3
                               : (cpu < 60.0f
                                      ? IlanaTheme::Ui::text3
                                            .interpolatedWith (IlanaTheme::accent(), (cpu - 30.0f) / 30.0f)
                                      : IlanaTheme::accent().interpolatedWith (juce::Colours::red,
                                                                               juce::jlimit (0.0f, 1.0f, (cpu - 60.0f) / 40.0f)));

    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true)); // live numbers
    g.setColour (cpuColour);
    g.drawText ("CPU " + juce::String (juce::roundToInt (cpu)) + "%",
                juce::Rectangle<int> (designWidth - 80, statusY, 64, 11), juce::Justification::centredRight);

    g.setColour (IlanaTheme::Ui::text3);
    g.drawText (juce::String (processorRef.getCurrentBpm(), 1) + " BPM",
                juce::Rectangle<int> (designWidth - 356, statusY, 70, 11), juce::Justification::centredRight);
    g.drawText ("VOICES", juce::Rectangle<int> (designWidth - 280, statusY, 44, 11), juce::Justification::centredRight);

    const auto activeVoices = processorRef.getActiveVoiceCount();
    const auto* voicesValue = processorRef.apvts.getRawParameterValue ("poly_voices");
    const auto maxVoices = juce::jlimit (1, 32, voicesValue != nullptr ? juce::roundToInt (voicesValue->load()) : 32);
    // Up to 16 dots at full size; more share the same width at half size.
    const auto spacing = maxVoices > 16 ? 4.0f : 8.0f;
    const auto size = maxVoices > 16 ? 3.0f : 5.0f;

    for (int i = 0; i < maxVoices; ++i)
    {
        const auto lit = i < activeVoices;
        const auto dot = juce::Rectangle<float> ((float) (designWidth - 232) + (float) i * spacing,
                                                 (float) statusY + 3.0f + (5.0f - size) * 0.5f, size, size);

        // Playing voices light up with a halo.
        if (lit)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.25f));
            g.fillEllipse (dot.expanded (2.5f));
        }

        g.setColour (lit ? IlanaTheme::accent() : IlanaTheme::Ui::track);
        g.fillEllipse (dot);
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

    logo.setBounds (16, 5, 250, 46);
    logo.version = juce::String ("v") + appVersion;
    logo.nameCentreY = 19.0f - 5.0f; // the header buttons' centre line (they span 4 to 34)

    // Header: preset display in the middle with its browse / save controls,
    // editing tools on the right. Buttons sit in the top 36 px; the status
    // line runs underneath them.
    auto headerRow = area.removeFromTop (56).withTrimmedTop (4).withHeight (30).withTrimmedRight (14);
    headerRow.removeFromLeft (292);

    constexpr int key = 30;
    settingsButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (6);
    diceButton.setBounds (headerRow.removeFromRight (key));
    headerRow.removeFromRight (6);
    abButton.setBounds (headerRow.removeFromRight (92));
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

    infoStrip.setBounds (area.removeFromBottom (22).reduced (14, 2));
    tutorial.setBounds (content.getLocalBounds());

    auto strip = area.removeFromBottom (52).reduced (14, 1);
    outputMeter->setBounds (strip.removeFromRight (24).withSizeKeepingCentre (24, 48));
    strip.removeFromRight (4);
    masterKnob->setBounds (strip.removeFromRight (108));
    strip.removeFromRight (4);
    voicesKnob->setBounds (strip.removeFromRight (88));
    // The bar's menu and switch put their name on the knobs' title line and
    // their box or switch on the value line, like the knobs' text beside them.
    const auto titleTop = strip.getCentreY() - 15;
    voiceModeBox->setBounds (strip.removeFromRight (96).withTop (titleTop).withHeight (13 + 24).reduced (2, 0));
    strip.removeFromRight (6);
    bendKnob->setBounds (strip.removeFromRight (86));
    legatoToggle->setBounds (strip.removeFromRight (80).withTop (titleTop).withHeight (13 + 20).reduced (2, 0));
    glideKnob->setBounds (strip.removeFromRight (92));
    strip.removeFromRight (6);

    macroPageButton.setBounds (strip.removeFromLeft (30).withSizeKeepingCentre (30, 20));
    strip.removeFromLeft (4);
    const auto macroWidth = strip.getWidth() / 4;

    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
        if (macro % 4 == 0)
            for (int k = 0; k < 4 && macro + k < (int) macroKnobs.size(); ++k)
                macroKnobs[(size_t) (macro + k)]->setBounds (strip.getX() + k * macroWidth, strip.getY(),
                                                             macroWidth - 6, strip.getHeight());

    auto chipsRow = area.removeFromBottom (26).reduced (14, 1);

    layoutChips (chipsRow);

    tabs.setBounds (area.reduced (14, 0).withTrimmedBottom (2));
    layoutTabRow();

    if (scopePanel != nullptr)
    {
        auto pageArea = tabs.getBounds().withTrimmedTop (tabs.getTabBarDepth());
        const auto expanded = static_cast<ScopePanel*> (scopePanel.get())->isExpanded();
        // A running fade-in would snap the panel back to its old bounds.
        juce::Desktop::getInstance().getAnimator().cancelAnimation (scopePanel.get(), false);
        scopePanel->setAlpha (1.0f);
        scopePanel->setBounds (expanded ? pageArea
                                        : pageArea.reduced (8).removeFromRight (476)); // inside the page's frame, full height so it never cuts a card in half
    }
}

SectionPage* IlanaSynthAudioProcessorEditor::currentSection() const
{
    const auto index = tabs.getCurrentTabIndex();
    return index >= 0 && index < (int) sections.size() ? sections[(size_t) index] : nullptr;
}

void IlanaSynthAudioProcessorEditor::showPage (const juce::String& id)
{
    if (id == "SCOPE")
    {
        setScopeOpen (true);
        return;
    }

    for (int index = 0; index < (int) sections.size(); ++index)
    {
        const auto page = sections[(size_t) index]->indexOf (id);

        if (page < 0)
            continue;

        const auto sameTab = tabs.getCurrentTabIndex() == index;
        sections[(size_t) index]->show (page, sameTab);

        if (! sameTab)
            tabs.setCurrentTabIndex (index);

        layoutTabRow();
        return;
    }
}

juce::String IlanaSynthAudioProcessorEditor::getCurrentPageId() const
{
    if (auto* section = currentSection())
        return section->getCurrentId();

    return {};
}

juce::StringArray IlanaSynthAudioProcessorEditor::getPageIds() const
{
    juce::StringArray ids;

    for (auto* section : sections)
        for (int page = 0; page < section->getNumPages(); ++page)
            ids.add (section->getPageId (page));

    return ids;
}

juce::Component* IlanaSynthAudioProcessorEditor::getCurrentPage() const
{
    if (auto* section = currentSection())
        return section->getCurrentPage();

    return nullptr;
}

void IlanaSynthAudioProcessorEditor::setScopeOpen (bool shouldBeOpen)
{
    scopeButton.setToggleState (shouldBeOpen, juce::dontSendNotification);

    if (shouldBeOpen == scopePanel->isVisible())
        return;

    if (shouldBeOpen)
    {
        resized();
        scopePanel->setAlpha (0.0f);
        scopePanel->setVisible (true);
        scopePanel->toFront (false);
        juce::Desktop::getInstance().getAnimator().fadeIn (scopePanel.get(), 180);
    }
    else
    {
        scopePanel->setVisible (false);
    }
}

bool IlanaSynthAudioProcessorEditor::isScopeOpen() const
{
    return scopePanel != nullptr && scopePanel->isVisible();
}

// The current tab's page switch (and the scope button) sit at the right end
// of the tab row.
void IlanaSynthAudioProcessorEditor::layoutTabRow()
{
    const auto bar = tabs.getBounds().withHeight (tabs.getTabBarDepth());
    auto row = bar.reduced (4, 5);
    scopeButton.setBounds (row.removeFromRight (74));
    row.removeFromRight (10);

    for (auto* section : sections)
    {
        const auto current = section == currentSection() && section->getNumPages() > 1;
        section->switcher.setVisible (current);

        if (current)
            section->switcher.setBounds (row.removeFromRight (section->switcher.getIdealWidth()));
    }
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

        safeThis->rememberLoadedFingerprint();

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
    animator.startTimerHz (60);

    updateHeaderButtons();
    rememberLoadedFingerprint();
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
        adoptLoadedFingerprint();

    presetDisplay.setPreset (shownPresetName, shownCategory, isFavourite (shownPresetName),
                             parameterFingerprint() != loadedFingerprint);
    favButton.setToggleState (isFavourite (shownPresetName), juce::dontSendNotification);
    favButton.setIconColour (isFavourite (shownPresetName) ? std::optional<juce::Colour> (juce::Colour (0xffffd447))
                                                           : std::nullopt);
    abButton.setButtonText (showingA ? "A/B:  A" : "A/B:  B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);

    for (auto& knob : macroKnobs)
        knob->refreshName();
}

void IlanaSynthAudioProcessorEditor::showMacroPage (int page)
{
    macroPage = juce::jlimit (0, (Mod::numMacros - 1) / 4, page);
    for (int macro = 0; macro < (int) macroKnobs.size(); ++macro)
        macroKnobs[(size_t) macro]->setVisible (macro / 4 == macroPage);
    macroPageButton.setButtonText (macroPage == 0 ? "5-8" : "1-4");
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
    abButton.setButtonText (showingA ? "A/B:  A" : "A/B:  B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);
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

    // Engine quality and oversampling, also on the scope panel (where they were
    // hard to find).
    const auto read = [this] (const char* id) { return processorRef.apvts.getRawParameterValue (id)->load(); };
    juce::PopupMenu quality;
    const char* const qualityNames[] { "Eco (unison capped at 4)", "Normal", "High" };
    for (int i = 0; i < 3; ++i)
        quality.addItem (600 + i, qualityNames[i], true, juce::roundToInt (read ("quality")) == i);
    juce::PopupMenu oversampling;
    const auto oversampled = read ("oversampling") > 0.5f;
    const auto factor = juce::roundToInt (read ("os_factor"));
    oversampling.addItem (700, "Off", true, ! oversampled);
    oversampling.addItem (701, "2x", true, oversampled && factor == 0);
    oversampling.addItem (702, "4x", true, oversampled && factor == 1);

    // Scala microtuning: the tick shows it is on, with the scale's name.
    juce::PopupMenu tuning;
    const auto& tuningState = processorRef.getTuningState();
    const auto hasScale = tuningState.hasScale();
    const auto tuningOn = hasScale && read ("tuning_on") > 0.5f;
    tuning.addItem (800, hasScale ? "Tuning on: " + tuningState.getDescription() : juce::String ("Tuning on (no scale loaded)"),
                    hasScale, tuningOn);
    tuning.addSeparator();
    tuning.addItem (801, "Load Scala tuning (.scl)...");
    tuning.addItem (802, "Load keyboard mapping (.kbm)...");
    tuning.addItem (803, "Reset to 12-TET", hasScale || read ("tuning_on") > 0.5f);
    tuning.addSeparator();
    // MTS-ESP: shown for information; a master in the session takes over.
    tuning.addItem (804, processorRef.isMtsEspConnected() ? "MTS-ESP: " + processorRef.getMtsEspScaleName() + " (overrides the scale)"
                                                          : juce::String ("MTS-ESP: no master in this session"),
                    false, processorRef.isMtsEspConnected());

    juce::PopupMenu menu;
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Interface size", sizes);
    menu.addSubMenu ("Engine quality", quality);
    menu.addSubMenu ("Oversampling", oversampling);
    menu.addSubMenu (tuningOn ? "Tuning: " + tuningState.getDescription() : juce::String ("Tuning"), tuning, true, nullptr, tuningOn);
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
                            else if (result >= 600 && result < 800)
                            {
                                const auto set = [&safeThis] (const char* id, float value)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (id))
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                                        parameter->endChangeGesture();
                                    }
                                };
                                if (result < 700)
                                    set ("quality", (float) (result - 600));
                                else
                                {
                                    set ("oversampling", result == 700 ? 0.0f : 1.0f);
                                    if (result > 700)
                                        set ("os_factor", (float) (result - 701));
                                }
                            }
                            else if (result == 400)
                            {
                                safeThis->tutorial.setVisible (true);
                                safeThis->tutorial.toFront (false);
                            }
                            else if (result == 800)
                            {
                                if (auto* on = safeThis->processorRef.apvts.getParameter ("tuning_on"))
                                {
                                    on->beginChangeGesture();
                                    on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.0f : 1.0f);
                                    on->endChangeGesture();
                                }
                            }
                            else if (result == 801 || result == 802)
                            {
                                const auto mapping = result == 802;
                                safeThis->fileChooser = std::make_unique<juce::FileChooser> (
                                    mapping ? "Load keyboard mapping" : "Load Scala tuning",
                                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                    mapping ? "*.kbm" : "*.scl");
                                safeThis->fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                                                        | juce::FileBrowserComponent::canSelectFiles,
                                                                    [safeThis, mapping] (const juce::FileChooser& chooser)
                                                                    {
                                                                        const auto file = chooser.getResult();
                                                                        if (safeThis == nullptr || ! file.existsAsFile())
                                                                            return;
                                                                        juce::String error;
                                                                        const auto text = file.loadFileAsString();
                                                                        const auto loaded = mapping ? safeThis->processorRef.loadTuningMapping (text, error)
                                                                                                    : safeThis->processorRef.loadTuningScale (text, error);
                                                                        if (! loaded)
                                                                            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                                                                    "Could not load " + file.getFileName(), error);
                                                                    });
                            }
                            else if (result == 803)
                                safeThis->processorRef.resetTuning();
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
        presetPanel->setScrimArea (content.getLocalBounds());
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
        tutorial.dismiss();
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

    if (code >= '0' && code <= '9')
    {
        const auto index = code == '0' ? 9 : code - '1';
        if (index < tabs.getNumTabs())
        {
            tabs.setCurrentTabIndex (index);
            return true;
        }

        return false;
    }

    return false;
}

// The preset's fingerprint when it was loaded or saved, kept in the
// processor with the preset's name so a reopened editor still shows EDITED.
void IlanaSynthAudioProcessorEditor::rememberLoadedFingerprint()
{
    loadedFingerprint = parameterFingerprint();
    processorRef.loadedPresetFingerprint = { processorRef.getCurrentPresetName(), loadedFingerprint };
}

void IlanaSynthAudioProcessorEditor::adoptLoadedFingerprint()
{
    const auto& kept = processorRef.loadedPresetFingerprint;

    if (kept.has_value() && kept->first == processorRef.getCurrentPresetName())
        loadedFingerprint = kept->second;
    else
        rememberLoadedFingerprint();
}
