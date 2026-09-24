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
                private juce::AsyncUpdater
{
public:
    explicit OscPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          waveDisplay1 (p, "osc1_table", "osc1_frame", "osc1_unison", "osc1_spread", "osc1_detune", false, {}, "osc1_mode", 0,
                        IlanaTheme::accent(), true),
          waveDisplay2 (p, "osc2_table", "osc2_frame", "osc2_unison", "osc2_spread", "osc2_detune", false, {}, "osc2_mode", 1,
                        juce::Colour (0xff5b8cff)),
          waveDisplay3 (p, "sub_table", "sub_frame", "sub_unison", "sub_spread", "sub_detune", false, {}, "sub_mode", 2,
                        juce::Colour (0xffffd447)),
          osc1On (p.apvts, "osc1_on", "ON"),
          osc1Mode (p.apvts, "osc1_mode", "MODE"),
          osc1Table (p.apvts, "osc1_table", "TABLE"),
          osc1Excite (p.apvts, "osc1_excite", "EXCITE"),
          osc1Frame (p.apvts, "osc1_frame", "FRAME"),
          osc1Level (p.apvts, "osc1_level", "LEVEL"),
          osc1Pan (p.apvts, "osc1_pan", "PAN"),
          osc1Semi (p.apvts, "osc1_semi", "SEMI"),
          osc1Fine (p.apvts, "osc1_fine", "FINE"),
          osc1Unison (p.apvts, "osc1_unison", "UNISON"),
          osc1Detune (p.apvts, "osc1_detune", "DETUNE"),
          osc1Spread (p.apvts, "osc1_spread", "SPREAD"),
          osc1StringDecay (p.apvts, "osc1_string_decay", "DECAY"),
          osc1StringDamp (p.apvts, "osc1_string_damp", "DAMP"),
          osc1StringSustain (p.apvts, "osc1_string_sustain", "SUSTAIN"),
          osc1SampleTuned (p.apvts, "osc1_sample_tuned", "TUNED"),
          osc1SampleLoop (p.apvts, "osc1_sample_loop", "LOOP"),
          osc1SampleReverse (p.apvts, "osc1_sample_reverse", "REVERSE"),
          osc1SampleStart (p.apvts, "osc1_sample_start", "START"),
          osc1SampleEnd (p.apvts, "osc1_sample_end", "END"),
          osc1SampleFadeIn (p.apvts, "osc1_sample_fade_in", "FADE IN"),
          osc1SampleFadeOut (p.apvts, "osc1_sample_fade_out", "FADE OUT"),
          osc1Chord (p.apvts, "osc1_chord", "CHORD"),
          osc2On (p.apvts, "osc2_on", "ON"),
          osc2Mode (p.apvts, "osc2_mode", "MODE"),
          osc2Table (p.apvts, "osc2_table", "TABLE"),
          osc2Excite (p.apvts, "osc2_excite", "EXCITE"),
          osc2Frame (p.apvts, "osc2_frame", "FRAME"),
          osc2Level (p.apvts, "osc2_level", "LEVEL"),
          osc2Pan (p.apvts, "osc2_pan", "PAN"),
          osc2Semi (p.apvts, "osc2_semi", "SEMI"),
          osc2Fine (p.apvts, "osc2_fine", "FINE"),
          osc2Unison (p.apvts, "osc2_unison", "UNISON"),
          osc2Detune (p.apvts, "osc2_detune", "DETUNE"),
          osc2Spread (p.apvts, "osc2_spread", "SPREAD"),
          osc2StringDecay (p.apvts, "osc2_string_decay", "DECAY"),
          osc2StringDamp (p.apvts, "osc2_string_damp", "DAMP"),
          osc2StringSustain (p.apvts, "osc2_string_sustain", "SUSTAIN"),
          osc2SampleTuned (p.apvts, "osc2_sample_tuned", "TUNED"),
          osc2SampleLoop (p.apvts, "osc2_sample_loop", "LOOP"),
          osc2SampleReverse (p.apvts, "osc2_sample_reverse", "REVERSE"),
          osc2SampleStart (p.apvts, "osc2_sample_start", "START"),
          osc2SampleEnd (p.apvts, "osc2_sample_end", "END"),
          osc2SampleFadeIn (p.apvts, "osc2_sample_fade_in", "FADE IN"),
          osc2SampleFadeOut (p.apvts, "osc2_sample_fade_out", "FADE OUT"),
          osc2Chord (p.apvts, "osc2_chord", "CHORD"),
          subOn (p.apvts, "sub_on", "ON"),
          subMode (p.apvts, "sub_mode", "MODE"),
          subTable (p.apvts, "sub_table", "TABLE"),
          subExcite (p.apvts, "sub_excite", "EXCITE"),
          subShape (p.apvts, "sub_shape", "SHAPE"),
          subOctave (p.apvts, "sub_octave", "OCT"),
          subFrame (p.apvts, "sub_frame", "FRAME"),
          subLevel (p.apvts, "sub_level", "LEVEL"),
          subPan (p.apvts, "sub_pan", "PAN"),
          subSemi (p.apvts, "sub_semi", "SEMI"),
          subFine (p.apvts, "sub_fine", "FINE"),
          subUnison (p.apvts, "sub_unison", "UNISON"),
          subDetune (p.apvts, "sub_detune", "DETUNE"),
          subSpread (p.apvts, "sub_spread", "SPREAD"),
          subStringDecay (p.apvts, "sub_string_decay", "DECAY"),
          subStringDamp (p.apvts, "sub_string_damp", "DAMP"),
          subStringSustain (p.apvts, "sub_string_sustain", "SUSTAIN"),
          subSampleTuned (p.apvts, "sub_sample_tuned", "TUNED"),
          subSampleLoop (p.apvts, "sub_sample_loop", "LOOP"),
          subSampleReverse (p.apvts, "sub_sample_reverse", "REVERSE"),
          subSampleStart (p.apvts, "sub_sample_start", "START"),
          subSampleEnd (p.apvts, "sub_sample_end", "END"),
          subSampleFadeIn (p.apvts, "sub_sample_fade_in", "FADE IN"),
          subSampleFadeOut (p.apvts, "sub_sample_fade_out", "FADE OUT"),
          subChord (p.apvts, "sub_chord", "CHORD"),
          noiseLevel (p.apvts, "noise_level", "NOISE"),
          osc1Warp (p.apvts, "osc1_warp", "WARP"), osc1UniMode (p.apvts, "osc1_uni_mode", "UNISON"),
          osc2Warp (p.apvts, "osc2_warp", "WARP"), osc2UniMode (p.apvts, "osc2_uni_mode", "UNISON"),
          subWarp (p.apvts, "sub_warp", "WARP"), subUniMode (p.apvts, "sub_uni_mode", "UNISON"),
          osc1WarpAmt (p.apvts, "osc1_warp_amt", "WARP AMT"), osc1UniBlend (p.apvts, "osc1_uni_blend", "BLEND"),
          osc2WarpAmt (p.apvts, "osc2_warp_amt", "WARP AMT"), osc2UniBlend (p.apvts, "osc2_uni_blend", "BLEND"),
          subWarpAmt (p.apvts, "sub_warp_amt", "WARP AMT"), subUniBlend (p.apvts, "sub_uni_blend", "BLEND")
    {
        addAll (*this, osc1Warp, osc1UniMode, osc2Warp, osc2UniMode, subWarp, subUniMode,
                osc1WarpAmt, osc1UniBlend, osc2WarpAmt, osc2UniBlend, subWarpAmt, subUniBlend);

        addAndMakeVisible (waveDisplay1);
        addAndMakeVisible (waveDisplay2);
        addAndMakeVisible (waveDisplay3);

        setupLoadButton (loadTableButton1, "osc1_table", 0);
        setupLoadButton (loadTableButton2, "osc2_table", 0);
        setupLoadButton (loadTableButton3, "sub_table", 0);

        addAndMakeVisible (loadTableButton1);
        addAndMakeVisible (loadTableButton2);
        addAndMakeVisible (loadTableButton3);

        addAll (*this,
                osc1On, osc1Mode, osc1Table, osc1Excite, osc1Frame, osc1Level, osc1Pan, osc1Semi,
                osc1Fine, osc1Unison, osc1Detune, osc1Spread, osc1StringDecay, osc1StringDamp, osc1StringSustain,
                osc1SampleTuned, osc1SampleLoop, osc1SampleReverse, osc1SampleStart, osc1SampleEnd,
                osc1SampleFadeIn, osc1SampleFadeOut, osc1Chord,
                osc2On, osc2Mode, osc2Table, osc2Excite, osc2Frame, osc2Level, osc2Pan, osc2Semi,
                osc2Fine, osc2Unison, osc2Detune, osc2Spread, osc2StringDecay, osc2StringDamp, osc2StringSustain,
                osc2SampleTuned, osc2SampleLoop, osc2SampleReverse, osc2SampleStart, osc2SampleEnd,
                osc2SampleFadeIn, osc2SampleFadeOut, osc2Chord,
                subOn, subMode, subTable, subExcite, subShape, subOctave, subFrame, subLevel, subPan, subSemi,
                subFine, subUnison, subDetune, subSpread, subStringDecay, subStringDamp, subStringSustain,
                subSampleTuned, subSampleLoop, subSampleReverse, subSampleStart, subSampleEnd,
                subSampleFadeIn, subSampleFadeOut, subChord,
                noiseLevel);

        // The TABLE lists open the wavetable browser.
        for (auto [control, id, colour] : { std::tuple<ComboControl*, const char*, juce::Colour> { &osc1Table, "osc1_table", IlanaTheme::accent() },
                                            { &osc2Table, "osc2_table", juce::Colour (0xff5b8cff) },
                                            { &subTable, "sub_table", juce::Colour (0xffffd447) } })
        {
            control->setPopupOverride ([this, control, id = juce::String (id), colour]
            {
                TableBrowser::show (processorRef, id, colour, control->getComboBox());
            });
        }

        voiceSpread = std::make_unique<StripKnob> (p, "voice_spread", "Spread");
        unisonRandom = std::make_unique<StripKnob> (p, "unison_random", "Uni Phase");
        drift = std::make_unique<StripKnob> (p, "drift", "Drift");
        addAll (*this, *voiceSpread, *unisonRandom, *drift);

        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "SUB");
        subOscLevel = std::make_unique<StripKnob> (p, "subosc_level", "Sub Level", -1, juce::Colour (0xffff9f43), false);
        noiseStrip = std::make_unique<StripKnob> (p, "noise_level", "Noise", -1, juce::Colour (0xffc8c8d0), false);
        addAll (*this, *subOscOn, *subOscLevel, *noiseStrip);
        noiseLevel.setVisible (false);

        for (const auto* id : { "osc1_mode", "osc2_mode", "sub_mode", "osc1_on", "osc2_on", "sub_on" })
            processorRef.apvts.addParameterListener (id, this);

        updateModeVisibility();
        updateEnabled();
    }

    ~OscPage() override
    {
        for (const auto* id : { "osc1_mode", "osc2_mode", "sub_mode", "osc1_on", "osc2_on", "sub_on" })
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

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (int band = 0; band < 3; ++band)
        {
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

            const std::array<const char*, 3> modeNames { "WAVETABLE", "STRING", "SAMPLE" };
            const auto mode = juce::jlimit (0, 2, getMode (band));

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
    }

    static juce::Colour oscColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xff5b8cff);
            case 2: return juce::Colour (0xffffd447);
            default: return IlanaTheme::accent();
        }
    }

    void resized() override
    {
        constexpr int stripHeight = 50;
        bandHeight = juce::jlimit (128, 176, (getHeight() - 24 - bandGap * 3 - stripHeight) / 3);
        auto area = getLocalBounds().reduced (12);

        for (int band = 0; band < 3; ++band)
        {
            layoutBand (area.removeFromTop (bandHeight), band);
            area.removeFromTop (bandGap);
        }

        // One strip: the sub and noise on the left, voice settings on the right.
        auto strip = area.removeFromTop (stripHeight);
        subStrip = strip.removeFromLeft (strip.getWidth() * 58 / 100);
        strip.removeFromLeft (8);
        voiceStrip = strip;

        auto row = subStrip.reduced (6, 2);
        row.removeFromLeft (54);
        subOscOn->setBounds (row.removeFromLeft (64).reduced (2, 2));
        subShape.setBounds (row.removeFromLeft (104).reduced (3, 0));
        subOctave.setBounds (row.removeFromLeft (88).reduced (3, 0));
        const auto knobWidth = row.getWidth() / 2;
        subOscLevel->setBounds (row.removeFromLeft (knobWidth));
        noiseStrip->setBounds (row);

        row = voiceStrip.reduced (6, 3);
        row.removeFromLeft (64);
        const auto third = row.getWidth() / 3;
        voiceSpread->setBounds (row.removeFromLeft (third));
        unisonRandom->setBounds (row.removeFromLeft (third));
        drift->setBounds (row);
    }

private:
    int bandHeight = 137;
    static constexpr int bandGap = 6;

    bool readBool (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load() > 0.5f;

        return true;
    }

    juce::Rectangle<int> bandBounds (int index) const
    {
        const auto row = getLocalBounds().reduced (12).removeFromTop (bandHeight * 3 + bandGap * 2);
        return { row.getX(), row.getY() + index * (bandHeight + bandGap), row.getWidth(), bandHeight };
    }

    WaveDisplay& waveDisplay (int index)
    {
        return index == 0 ? waveDisplay1 : (index == 1 ? waveDisplay2 : waveDisplay3);
    }

    juce::TextButton& loadButton (int index)
    {
        return index == 0 ? loadTableButton1 : (index == 1 ? loadTableButton2 : loadTableButton3);
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
        const auto id = oscIndex == 0 ? "osc1_mode" : (oscIndex == 1 ? "osc2_mode" : "sub_mode");
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    void layoutBand (juce::Rectangle<int> band, int index)
    {
        auto titleStrip = band.reduced (8).removeFromTop (18);
        loadButton (index).setBounds (titleStrip.removeFromRight (86).withSizeKeepingCentre (86, 15));

        auto content = band.reduced (8);
        content.removeFromTop (20);
        waveDisplay (index).setBounds (content.removeFromLeft (276));
        content.removeFromLeft (8);

        auto topRow = content.removeFromTop (38);
        content.removeFromTop (3);
        auto bottomRow = content;

        const auto mode = getMode (index);
        const auto isSample = mode == 2;
        const auto isString = mode == 1;
        const auto isWavetable = mode == 0;

        std::vector<juce::Component*> top;
        std::vector<juce::Component*> bottom;

        const auto addTop = [&top] (juce::Component* item) { if (item != nullptr) top.push_back (item); };
        const auto addBottom = [&bottom] (juce::Component* item) { if (item != nullptr) bottom.push_back (item); };

        if (index == 0)
        {
            addTop (&osc1On);
            addTop (&osc1Mode);
            addTop (isSample ? (juce::Component*) &osc1SampleTuned
                             : (isString ? (juce::Component*) &osc1Excite : (juce::Component*) &osc1Table));
            addTop (isSample ? (juce::Component*) &osc1SampleLoop : nullptr);
            addTop (isSample ? (juce::Component*) &osc1SampleReverse : nullptr);
            addTop (isWavetable ? (juce::Component*) &osc1Warp : nullptr);
            addTop (&osc1UniMode);
            addTop (&osc1Chord);

            addBottom (isSample ? (juce::Component*) &osc1SampleStart
                                : (isString ? (juce::Component*) &osc1StringDecay : (juce::Component*) &osc1Frame));
            addBottom (isWavetable ? (juce::Component*) &osc1WarpAmt : nullptr);
            addBottom (isSample ? (juce::Component*) &osc1SampleEnd
                                : (isString ? (juce::Component*) &osc1StringDamp : nullptr));
            addBottom (isSample ? (juce::Component*) &osc1SampleFadeIn
                                : (isString ? (juce::Component*) &osc1StringSustain : nullptr));
            addBottom (isSample ? (juce::Component*) &osc1SampleFadeOut : nullptr);
            addBottom (&osc1Level);
            addBottom (&osc1Pan);
            addBottom (&osc1Semi);
            addBottom (&osc1Fine);
            addBottom (&osc1Unison);
            addBottom (&osc1Detune);
            addBottom (&osc1UniBlend);
            addBottom (&osc1Spread);
        }
        else if (index == 1)
        {
            addTop (&osc2On);
            addTop (&osc2Mode);
            addTop (isSample ? (juce::Component*) &osc2SampleTuned
                             : (isString ? (juce::Component*) &osc2Excite : (juce::Component*) &osc2Table));
            addTop (isSample ? (juce::Component*) &osc2SampleLoop : nullptr);
            addTop (isSample ? (juce::Component*) &osc2SampleReverse : nullptr);
            addTop (isWavetable ? (juce::Component*) &osc2Warp : nullptr);
            addTop (&osc2UniMode);
            addTop (&osc2Chord);

            addBottom (isSample ? (juce::Component*) &osc2SampleStart
                                : (isString ? (juce::Component*) &osc2StringDecay : (juce::Component*) &osc2Frame));
            addBottom (isWavetable ? (juce::Component*) &osc2WarpAmt : nullptr);
            addBottom (isSample ? (juce::Component*) &osc2SampleEnd
                                : (isString ? (juce::Component*) &osc2StringDamp : nullptr));
            addBottom (isSample ? (juce::Component*) &osc2SampleFadeIn
                                : (isString ? (juce::Component*) &osc2StringSustain : nullptr));
            addBottom (isSample ? (juce::Component*) &osc2SampleFadeOut : nullptr);
            addBottom (&osc2Level);
            addBottom (&osc2Pan);
            addBottom (&osc2Semi);
            addBottom (&osc2Fine);
            addBottom (&osc2Unison);
            addBottom (&osc2Detune);
            addBottom (&osc2UniBlend);
            addBottom (&osc2Spread);
        }
        else
        {
            addTop (&subOn);
            addTop (&subMode);
            addTop (isSample ? (juce::Component*) &subSampleTuned
                             : (isString ? (juce::Component*) &subExcite : (juce::Component*) &subTable));
            addTop (isSample ? (juce::Component*) &subSampleLoop : nullptr);
            addTop (isSample ? (juce::Component*) &subSampleReverse : nullptr);
            addTop (isWavetable ? (juce::Component*) &subWarp : nullptr);
            addTop (&subUniMode);
            addTop (&subChord);

            addBottom (isSample ? (juce::Component*) &subSampleStart
                                : (isString ? (juce::Component*) &subStringDecay : (juce::Component*) &subFrame));
            addBottom (isWavetable ? (juce::Component*) &subWarpAmt : nullptr);
            addBottom (isSample ? (juce::Component*) &subSampleEnd
                                : (isString ? (juce::Component*) &subStringDamp : nullptr));
            addBottom (isSample ? (juce::Component*) &subSampleFadeIn
                                : (isString ? (juce::Component*) &subStringSustain : nullptr));
            addBottom (isSample ? (juce::Component*) &subSampleFadeOut : nullptr);
            addBottom (&subLevel);
            addBottom (&subPan);
            addBottom (&subSemi);
            addBottom (&subFine);
            addBottom (&subUnison);
            addBottom (&subDetune);
            addBottom (&subUniBlend);
            addBottom (&subSpread);
        }

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

        for (int index = 0; index < 3; ++index)
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

    void updateModeVisibility()
    {
        const auto mode1 = getMode (0);
        osc1Table.setVisible (mode1 == 0);
        osc1Frame.setVisible (mode1 == 0);
        osc1Excite.setVisible (mode1 == 1);
        osc1StringDecay.setVisible (mode1 == 1);
        osc1StringDamp.setVisible (mode1 == 1);
        osc1StringSustain.setVisible (mode1 == 1);
        osc1SampleTuned.setVisible (mode1 == 2);
        osc1SampleLoop.setVisible (mode1 == 2);
        osc1SampleReverse.setVisible (mode1 == 2);
        osc1SampleStart.setVisible (mode1 == 2);
        osc1SampleEnd.setVisible (mode1 == 2);
        osc1SampleFadeIn.setVisible (mode1 == 2);
        osc1SampleFadeOut.setVisible (mode1 == 2);
        osc1Warp.setVisible (mode1 == 0);
        osc1WarpAmt.setVisible (mode1 == 0);

        const auto mode2 = getMode (1);
        osc2Table.setVisible (mode2 == 0);
        osc2Frame.setVisible (mode2 == 0);
        osc2Excite.setVisible (mode2 == 1);
        osc2StringDecay.setVisible (mode2 == 1);
        osc2StringDamp.setVisible (mode2 == 1);
        osc2StringSustain.setVisible (mode2 == 1);
        osc2SampleTuned.setVisible (mode2 == 2);
        osc2SampleLoop.setVisible (mode2 == 2);
        osc2SampleReverse.setVisible (mode2 == 2);
        osc2SampleStart.setVisible (mode2 == 2);
        osc2SampleEnd.setVisible (mode2 == 2);
        osc2SampleFadeIn.setVisible (mode2 == 2);
        osc2SampleFadeOut.setVisible (mode2 == 2);
        osc2Warp.setVisible (mode2 == 0);
        osc2WarpAmt.setVisible (mode2 == 0);

        const auto mode3 = getMode (2);
        subTable.setVisible (mode3 == 0);
        subFrame.setVisible (mode3 == 0);
        subExcite.setVisible (mode3 == 1);
        subStringDecay.setVisible (mode3 == 1);
        subStringDamp.setVisible (mode3 == 1);
        subStringSustain.setVisible (mode3 == 1);
        subSampleTuned.setVisible (mode3 == 2);
        subSampleLoop.setVisible (mode3 == 2);
        subSampleReverse.setVisible (mode3 == 2);
        subSampleStart.setVisible (mode3 == 2);
        subSampleEnd.setVisible (mode3 == 2);
        subSampleFadeIn.setVisible (mode3 == 2);
        subSampleFadeOut.setVisible (mode3 == 2);
        subWarp.setVisible (mode3 == 0);
        subWarpAmt.setVisible (mode3 == 0);

        resized();
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
        const auto enabled1 = readBool ("osc1_on");
        const auto enabled2 = readBool ("osc2_on");
        const auto enabled3 = readBool ("sub_on");

        setGroupEnabled ({ &osc1Mode, &osc1Table, &osc1Excite, &osc1Frame, &osc1Level, &osc1Pan, &osc1Semi,
                           &osc1Fine, &osc1Unison, &osc1Detune, &osc1Spread,
                           &osc1StringDecay, &osc1StringDamp, &osc1StringSustain,
                           &osc1SampleTuned, &osc1SampleLoop, &osc1SampleReverse,
                           &osc1SampleStart, &osc1SampleEnd, &osc1SampleFadeIn, &osc1SampleFadeOut,
                           &osc1Chord, &osc1Warp, &osc1WarpAmt, &osc1UniMode, &osc1UniBlend },
                         enabled1);
        setGroupEnabled ({ &osc2Mode, &osc2Table, &osc2Excite, &osc2Frame, &osc2Level, &osc2Pan, &osc2Semi,
                           &osc2Fine, &osc2Unison, &osc2Detune, &osc2Spread,
                           &osc2StringDecay, &osc2StringDamp, &osc2StringSustain,
                           &osc2SampleTuned, &osc2SampleLoop, &osc2SampleReverse,
                           &osc2SampleStart, &osc2SampleEnd, &osc2SampleFadeIn, &osc2SampleFadeOut,
                           &osc2Chord, &osc2Warp, &osc2WarpAmt, &osc2UniMode, &osc2UniBlend },
                         enabled2);
        setGroupEnabled ({ &subMode, &subTable, &subExcite, &subFrame, &subLevel,
                           &subPan, &subSemi, &subFine, &subUnison, &subDetune, &subSpread,
                           &subStringDecay, &subStringDamp, &subStringSustain,
                           &subSampleTuned, &subSampleLoop, &subSampleReverse,
                           &subSampleStart, &subSampleEnd, &subSampleFadeIn, &subSampleFadeOut,
                           &subChord, &subWarp, &subWarpAmt, &subUniMode, &subUniBlend },
                         enabled3);

        const std::array<bool, 3> enabled { enabled1, enabled2, enabled3 };

        for (int index = 0; index < 3; ++index)
        {
            const auto alpha = enabled[(size_t) index] ? 1.0f : 0.3f;
            waveDisplay (index).setAlpha (alpha);
            waveDisplay (index).setEnabled (enabled[(size_t) index]);
            loadButton (index).setEnabled (! chooserOpen && enabled[(size_t) index]);
            loadButton (index).setAlpha (alpha);
        }
    }

    int readTableChoiceIndex (const juce::String& tableId) const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId)))
            return param->getIndex();

        return 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    WaveDisplay waveDisplay1, waveDisplay2, waveDisplay3;
    std::array<juce::Rectangle<int>, 3> controlBay {};
    juce::TextButton loadTableButton1 { "LOAD .WAV" };
    juce::TextButton loadTableButton2 { "LOAD .WAV" };
    juce::TextButton loadTableButton3 { "LOAD .WAV" };
    std::unique_ptr<juce::FileChooser> tableChooser;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<StripKnob> voiceSpread, unisonRandom, drift;
    juce::Rectangle<int> voiceStrip;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<StripKnob> subOscLevel, noiseStrip;
    juce::Rectangle<int> subStrip;

    ToggleControl osc1On;
    ComboControl osc1Mode, osc1Table, osc1Excite;
    KnobControl osc1Frame, osc1Level, osc1Pan, osc1Semi, osc1Fine, osc1Unison, osc1Detune, osc1Spread;
    KnobControl osc1StringDecay, osc1StringDamp, osc1StringSustain;
    ToggleControl osc1SampleTuned, osc1SampleLoop, osc1SampleReverse;
    KnobControl osc1SampleStart, osc1SampleEnd, osc1SampleFadeIn, osc1SampleFadeOut;
    ComboControl osc1Chord;
    ToggleControl osc2On;
    ComboControl osc2Mode, osc2Table, osc2Excite;
    KnobControl osc2Frame, osc2Level, osc2Pan, osc2Semi, osc2Fine, osc2Unison, osc2Detune, osc2Spread;
    KnobControl osc2StringDecay, osc2StringDamp, osc2StringSustain;
    ToggleControl osc2SampleTuned, osc2SampleLoop, osc2SampleReverse;
    KnobControl osc2SampleStart, osc2SampleEnd, osc2SampleFadeIn, osc2SampleFadeOut;
    ComboControl osc2Chord;
    ToggleControl subOn;
    ComboControl subMode, subTable, subExcite, subShape, subOctave;
    KnobControl subFrame, subLevel, subPan, subSemi, subFine, subUnison, subDetune, subSpread;
    KnobControl subStringDecay, subStringDamp, subStringSustain;
    ToggleControl subSampleTuned, subSampleLoop, subSampleReverse;
    KnobControl subSampleStart, subSampleEnd, subSampleFadeIn, subSampleFadeOut;
    ComboControl subChord;
    KnobControl noiseLevel;
    ComboControl osc1Warp, osc1UniMode, osc2Warp, osc2UniMode, subWarp, subUniMode;
    KnobControl osc1WarpAmt, osc1UniBlend, osc2WarpAmt, osc2UniBlend, subWarpAmt, subUniBlend;
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
          balance (p.apvts, "filter_balance", "F1  /  F2", IlanaTheme::accent(), true),
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
        balance.setBounds (flowArea.removeFromRight (86));
        flowArea.removeFromRight (6);
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

        fade (balance, read ("filters_parallel"));

        const auto resonating = read ("res_on");

        for (auto* knob : { &resAmount, &resDecay, &resOffset, &resKeytrack })
            fade (*knob, resonating);
    }

    IlanaSynthAudioProcessor& processorRef;
    FilterDisplay filterDisplay;
    FilterPanel panel1, panel2;
    SignalFlow flow;
    KnobControl balance;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    juce::Rectangle<int> flowTitle, resonatorCard;
};

class EnvSection : public juce::Component
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : settings (settingsRef),
          thumbs (p, { EnvThumbBar::Env { "AMP", "amp", Mod::Source::AmpEnv, IlanaTheme::accent() },
                       EnvThumbBar::Env { "FILTER 1", "fe", Mod::Source::FilterEnv, juce::Colour (0xffff4fd8) },
                       EnvThumbBar::Env { "FILTER 2", "f2e", Mod::Source::FilterEnv2, juce::Colour (0xffb28aff) },
                       EnvThumbBar::Env { "MOD", "me", Mod::Source::ModEnv, juce::Colour (0xff8fff3b) },
                       EnvThumbBar::Env { "ENV 4", "e4", Mod::Source::Env4, juce::Colour (0xffffd447) } }),
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
          f2Curve (p.apvts, "f2e_curve", "TENSION", juce::Colour (0xffb28aff), false),
          meA (p.apvts, "me_attack", "ATTACK"), meD (p.apvts, "me_decay", "DECAY"),
          meS (p.apvts, "me_sustain", "SUSTAIN"), meR (p.apvts, "me_release", "RELEASE"),
          meCurve (p.apvts, "me_curve", "TENSION", juce::Colour (0xff8fff3b), false),
          e4A (p.apvts, "e4_attack", "ATTACK"), e4D (p.apvts, "e4_decay", "DECAY"),
          e4S (p.apvts, "e4_sustain", "SUSTAIN"), e4R (p.apvts, "e4_release", "RELEASE"),
          e4Curve (p.apvts, "e4_curve", "TENSION", juce::Colour (0xffffd447), false)
    {
        addAndMakeVisible (thumbs);

        addAll (*this, ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display,
                ampA, ampD, ampS, ampR, ampVel, ampCurve,
                feA, feD, feS, feR, feVel, feCurve,
                f2A, f2D, f2S, f2R, f2Curve,
                meA, meD, meS, meR, meCurve,
                e4A, e4D, e4S, e4R, e4Curve);

        units.push_back ({ &ampDisplay, { &ampA, &ampD, &ampS, &ampR, &ampVel, &ampCurve } });
        units.push_back ({ &feDisplay, { &feA, &feD, &feS, &feR, &feVel, &feCurve } });
        units.push_back ({ &f2eDisplay, { &f2A, &f2D, &f2S, &f2R, nullptr, &f2Curve } });
        units.push_back ({ &meDisplay, { &meA, &meD, &meS, &meR, nullptr, &meCurve } });
        units.push_back ({ &e4Display, { &e4A, &e4D, &e4S, &e4R, nullptr, &e4Curve } });

        selected = juce::jlimit (0, (int) units.size() - 1, settings.getIntValue ("envSelected", 0));

        thumbs.onSelect = [this] (int index)
        {
            selected = index;
            settings.setValue ("envSelected", selected);
            updateVisibility();
        };

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

        thumbs.setBounds (area.removeFromTop (48));
        area.removeFromTop (8);

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (area.getWidth() * 55 / 100).reduced (2));
        area.removeFromLeft (8);

        // Same panel shape as the LFOs: heading, then the stage knobs.
        panel = area;
        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        layoutFixed (inner.withSizeKeepingCentre (inner.getWidth(), juce::jmin (inner.getHeight(), 140)),
                     units[(size_t) unitIndex].knobs);
    }

    void paint (juce::Graphics& g) override
    {
        if (panel.isEmpty())
            return;

        const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xffff4fd8), juce::Colour (0xffb28aff),
                                       juce::Colour (0xff8fff3b), juce::Colour (0xffffd447) };
        const juce::StringArray titles { "AMP ENVELOPE", "FILTER 1 ENVELOPE", "FILTER 2 ENVELOPE", "MOD ENVELOPE", "ENVELOPE 4" };
        const auto index = juce::jlimit (0, 4, selected);

        IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, colours[index].withAlpha (0.35f));
        auto header = panel.reduced (12, 0).withHeight (26);
        g.setColour (colours[index]);
        g.setFont (IlanaTheme::font (12.5f, true));
        g.drawText (titles[index], header, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("drag the graph or the knobs", header, juce::Justification::centredRight);
    }

private:
    struct Unit
    {
        juce::Component* display = nullptr;
        std::vector<juce::Component*> knobs;
    };

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
        repaint();
    }

    juce::PropertiesFile& settings;
    juce::Rectangle<int> panel;
    EnvThumbBar thumbs;
    EnvelopeDisplay ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display;
    KnobControl ampA, ampD, ampS, ampR, ampVel, ampCurve;
    KnobControl feA, feD, feS, feR, feVel, feCurve;
    KnobControl f2A, f2D, f2S, f2R, f2Curve;
    KnobControl meA, meD, meS, meR, meCurve;
    KnobControl e4A, e4D, e4S, e4R, e4Curve;
    std::vector<Unit> units;
    int selected = 0;
};

class LfoSection : public juce::Component,
                   private juce::Timer
{
public:
    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef),
          thumbs (p, [] (int index) { return lfoColour (index); })
    {
        addAndMakeVisible (thumbs);

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            auto display = std::make_unique<LfoDisplay> (p, lfo, lfoColour (lfo), lfo == 0);
            addAndMakeVisible (*display);
            displays.push_back (std::move (display));

            auto controls = std::make_unique<Controls> (p.apvts, lfo + 1, lfoColour (lfo), lfo == 0);
            addAll (*this, controls->shape, controls->rate, controls->sync, controls->div, controls->retrig, controls->phase);
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
        startTimerHz (10);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        thumbs.setBounds (area.removeFromTop (58));
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
        c.sync.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2).reduced (3, 1));
        c.retrig.setBounds (toggles.reduced (3, 1));
        c.div.setBounds (options.reduced (3, 1));

        inner.removeFromLeft (8);
        c.rate.setBounds (inner.removeFromLeft (inner.getWidth() / 2).reduced (3, 0));
        c.phase.setBounds (inner.reduced (3, 0));
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
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText (retrig != nullptr && retrig->load() > 0.5f ? "runs per voice, restarts on each note"
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
            default: return IlanaTheme::accent();
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
              phase (state, "lfo" + juce::String (lfo) + "_phase", "START", accent, followsTheme)
        {
        }

        ComboControl shape;
        KnobControl rate;
        ToggleControl sync;
        ComboControl div;
        ToggleControl retrig;
        KnobControl phase;
    };

    void updateVisibility()
    {
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
            c.phase.setVisible (visible);
        }

        thumbs.setSelected (selected);
        resized();
        repaint();
    }

    // RATE only matters free-running and DIVISION only when synced, so the
    // unused one steps back.
    void timerCallback() override
    {
        if (! isShowing())
            return;

        auto& c = *controlsList[(size_t) juce::jlimit (0, (int) controlsList.size() - 1, selected)];
        const auto* sync = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (selected + 1) + "_sync");
        const auto synced = sync != nullptr && sync->load() > 0.5f;
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
    LfoThumbBar thumbs;
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    int selected = 0;
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

// FM between the three oscillators: the operator diagram on the left, the
// full matrix of amounts on the right (rows = from, columns = to), plus the
// FM style, each oscillator's output switch, ring mod and hard sync.
class FmPage : public juce::Component
{
public:
    explicit FmPage (IlanaSynthAudioProcessor& p)
        : diagram (p),
          mode (p.apvts, "fm_mode", "FM MODE"),
          hardSync (p.apvts, "hard_sync", "HARD SYNC 1>2")
    {
        addAll (*this, diagram, mode, hardSync);
        ringMod = std::make_unique<StripKnob> (p, "ring_mod", "Ring Mod", -1, fmColour(), false);
        addAndMakeVisible (*ringMod);

        for (int source = 0; source < 3; ++source)
        {
            for (int target = 0; target < 3; ++target)
            {
                auto knob = std::make_unique<KnobControl> (p.apvts, FmDiagram::routeId (source, target), "",
                                                           FmDiagram::oscColour (source), false);
                addAndMakeVisible (*knob);
                knobs[(size_t) source][(size_t) target] = std::move (knob);
            }

            outs[(size_t) source] = std::make_unique<ToggleControl> (p.apvts, source == 0 ? "osc1_out" : (source == 1 ? "osc2_out" : "sub_out"), "OUT");
            addAndMakeVisible (*outs[(size_t) source]);
        }
    }

    static juce::Colour fmColour() { return juce::Colour (0xffe3a56f); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        paintSectionTitle (g, "OPERATORS", { 14, 10, 300, 16 });
        IlanaTheme::paintCard (g, matrixCard.toFloat(), 7.0f, fmColour().withAlpha (0.35f));

        g.setColour (fmColour());
        g.setFont (IlanaTheme::font (13.0f, true));
        g.drawText ("FM MATRIX", matrixCard.reduced (12, 0).withHeight (26), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("rows modulate columns", matrixCard.reduced (12, 0).withHeight (26), juce::Justification::centredRight);

        // Column and row headings.
        g.setFont (IlanaTheme::font (11.0f, true));

        for (int i = 0; i < 3; ++i)
        {
            g.setColour (FmDiagram::oscColour (i));
            g.drawText ("TO OSC " + juce::String (i + 1), columnHeads[(size_t) i], juce::Justification::centred);
            g.drawText ("OSC " + juce::String (i + 1), rowHeads[(size_t) i].withHeight (18), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (18);

        matrixCard = area.removeFromRight (area.getWidth() * 48 / 100);
        area.removeFromRight (10);
        diagram.setBounds (area);

        auto inner = matrixCard.reduced (10, 0);
        inner.removeFromTop (26);
        inner.removeFromBottom (8);

        // Mode, ring mod and sync across the top.
        auto top = inner.removeFromTop (48);
        mode.setBounds (top.removeFromLeft (top.getWidth() * 36 / 100).reduced (3, 1));
        ringMod->setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (3, 1));
        hardSync.setBounds (top.reduced (3, 1));
        inner.removeFromTop (6);

        auto heads = inner.removeFromTop (18);
        heads.removeFromLeft (70);
        const auto columnWidth = heads.getWidth() / 3;

        for (int i = 0; i < 3; ++i)
            columnHeads[(size_t) i] = heads.removeFromLeft (columnWidth);

        const auto rowHeight = inner.getHeight() / 3;

        for (int source = 0; source < 3; ++source)
        {
            auto row = inner.removeFromTop (rowHeight);
            auto head = row.removeFromLeft (70);
            rowHeads[(size_t) source] = head.withTrimmedTop (head.getHeight() / 2 - 26);
            outs[(size_t) source]->setBounds (rowHeads[(size_t) source].withTrimmedTop (20).withHeight (40).reduced (0, 2));

            for (int target = 0; target < 3; ++target)
                knobs[(size_t) source][(size_t) target]->setBounds (row.removeFromLeft (columnWidth).reduced (3, 0));
        }
    }

private:
    FmDiagram diagram;
    ComboControl mode;
    ToggleControl hardSync;
    std::unique_ptr<StripKnob> ringMod;
    std::array<std::array<std::unique_ptr<KnobControl>, 3>, 3> knobs;
    std::array<std::unique_ptr<ToggleControl>, 3> outs;
    std::array<juce::Rectangle<int>, 3> columnHeads, rowHeads;
    juce::Rectangle<int> matrixCard;
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
        sprayCount = std::make_unique<StripKnob> (p, "spray_count", "Notes", -1, generateColour(), false);
        sprayRange = std::make_unique<StripKnob> (p, "spray_range", "Range", -1, generateColour(), false);
        spraySpread = std::make_unique<StripKnob> (p, "spray_spread", "Spread", -1, generateColour(), false);
        sprayChance = std::make_unique<StripKnob> (p, "spray_chance", "Chance", -1, generateColour(), false);
        sprayVelocity = std::make_unique<StripKnob> (p, "spray_velocity", "Vel Random", -1, generateColour(), false);
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
        g.drawText ("scale snap and note spray", generateCard.reduced (12, 0).removeFromTop (26),
                    juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        const auto layoutPicker = [this] (int rowIndex, juce::Rectangle<int> header)
        {
            header.removeFromLeft (80);

            for (auto& button : lfoButtons[(size_t) rowIndex])
            {
                button.setBounds (header.removeFromLeft (58).reduced (2, 0));
                header.removeFromLeft (2);
            }
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

        // Generate card: scale row, spray row, then the spray amounts.
        auto generate = generateCard.reduced (10, 0);
        generate.removeFromTop (26);
        generate.removeFromBottom (6);
        const auto rowHeight = generate.getHeight() / 4;

        auto scaleRow = generate.removeFromTop (rowHeight);
        genScale.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() * 45 / 100).reduced (3, 1));
        genRoot.setBounds (scaleRow.removeFromLeft (scaleRow.getWidth() / 2).reduced (3, 1));
        genSnap.setBounds (scaleRow.reduced (3, 1));

        auto sprayRow = generate.removeFromTop (rowHeight);
        sprayOn.setBounds (sprayRow.removeFromLeft (sprayRow.getWidth() / 3).reduced (3, 1));
        sprayDirection.setBounds (sprayRow.removeFromLeft (sprayRow.getWidth() / 2).reduced (3, 1));
        sprayCount->setBounds (sprayRow.reduced (3, 1));

        auto amounts = generate.removeFromTop (rowHeight);
        sprayRange->setBounds (amounts.removeFromLeft (amounts.getWidth() / 2).reduced (3, 1));
        spraySpread->setBounds (amounts.reduced (3, 1));

        amounts = generate;
        sprayChance->setBounds (amounts.removeFromLeft (amounts.getWidth() / 2).reduced (3, 1));
        sprayVelocity->setBounds (amounts.reduced (3, 1));
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
    std::unique_ptr<StripKnob> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, msegCard, arpCard, generateCard;
};

// The overview: everything needed to shape a basic sound on one screen
// (oscillators, filter 1, amp envelope, LFOs). The other tabs hold the
// detail.
class MainPage : public juce::Component,
                 private juce::Timer
{
public:
    explicit MainPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          wave1 (p, "osc1_table", "osc1_frame", "osc1_unison", "osc1_spread", "osc1_detune", false, {}, "osc1_mode", 0,
                 IlanaTheme::accent(), true),
          wave2 (p, "osc2_table", "osc2_frame", "osc2_unison", "osc2_spread", "osc2_detune", false, {}, "osc2_mode", 1,
                 juce::Colour (0xff5b8cff)),
          wave3 (p, "sub_table", "sub_frame", "sub_unison", "sub_spread", "sub_detune", false, {}, "sub_mode", 2,
                 juce::Colour (0xffffd447)),
          filterDisplay (p),
          lfoThumbs (p, [] (int index) { return lfoColour (index); }),
          filterTabs ({ "F1", "F2" }, { filterColour (0), filterColour (1) }, true),
          envTabs ({ "AMP", "FLT 1", "FLT 2", "MOD", "ENV 4" },
                   { envColour (0), envColour (1), envColour (2), envColour (3), envColour (4) }, true),
          lfoTabs ({}, {}, true)
    {
        const char* const prefixes[] { "osc1", "osc2", "sub" };
        const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xff5b8cff), juce::Colour (0xffffd447) };

        for (int osc = 0; osc < 3; ++osc)
        {
            const juce::String prefix (prefixes[osc]);
            auto strip = std::make_unique<OscStrip>();
            const auto colour = colours[osc];
            const auto themed = osc == 0;

            strip->on = std::make_unique<ToggleControl> (p.apvts, prefix + "_on", "ON");
            strip->table = std::make_unique<ComboControl> (p.apvts, prefix + "_table", "TABLE");
            strip->table->setPopupOverride ([this, table = strip->table.get(), id = prefix + "_table", colour]
            {
                TableBrowser::show (processorRef, id, colour, table->getComboBox());
            });
            strip->warp = std::make_unique<ComboControl> (p.apvts, prefix + "_warp", "WARP");
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_frame", "FRAME", colour, themed));
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_warp_amt", "WARP", colour, themed));
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_level", "LEVEL", colour, themed));
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_semi", "SEMI", colour, themed));
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_unison", "UNISON", colour, themed));
            strip->knobs.push_back (std::make_unique<KnobControl> (p.apvts, prefix + "_detune", "DETUNE", colour, themed));

            addAll (*this, *strip->on, *strip->table, *strip->warp);

            for (auto& knob : strip->knobs)
                addAndMakeVisible (*knob);

            strips.push_back (std::move (strip));
        }

        addAndMakeVisible (wave1);
        addAndMakeVisible (wave2);
        addAndMakeVisible (wave3);

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
        addAndMakeVisible (lfoThumbs);

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
        startTimerHz (8);
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
            default: return IlanaTheme::accent();
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

        const juce::Colour oscColours[] { IlanaTheme::accent(), juce::Colour (0xff5b8cff), juce::Colour (0xffffd447) };

        for (int osc = 0; osc < 3; ++osc)
            paintCard (g, oscCards[(size_t) osc], "OSC " + juce::String (osc + 1), oscColours[osc]);

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

        const auto oscHeight = (left.getHeight() - 16) / 3;

        for (int osc = 0; osc < 3; ++osc)
        {
            oscCards[(size_t) osc] = left.removeFromTop (oscHeight);
            left.removeFromTop (8);
            layoutStrip (osc, oscCards[(size_t) osc]);
        }

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
            lfoThumbs.setBounds (inner.removeFromTop (juce::jmax (40, inner.getHeight() - 52)));
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
        std::unique_ptr<ComboControl> table, warp;
        std::vector<std::unique_ptr<KnobControl>> knobs;
    };

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

    WaveDisplay& wave (int index) { return index == 0 ? wave1 : (index == 1 ? wave2 : wave3); }

    void layoutStrip (int index, juce::Rectangle<int> card)
    {
        auto& strip = *strips[(size_t) index];
        auto inner = card.reduced (10, 8);
        auto title = inner.removeFromTop (18);
        strip.on->setBounds (title.removeFromRight (56).withTrimmedTop (-13).withHeight (30));
        inner.removeFromTop (2);

        wave (index).setBounds (inner.removeFromLeft (juce::jmin (170, inner.getWidth() / 3)));
        inner.removeFromLeft (8);

        auto combos = inner.removeFromTop (40);
        strip.table->setBounds (combos.removeFromLeft (combos.getWidth() / 2).reduced (3, 0));
        strip.warp->setBounds (combos.reduced (3, 0));

        std::vector<juce::Component*> knobs;

        for (auto& knob : strip.knobs)
            knobs.push_back (knob.get());

        layoutRow (inner, knobs);
    }

    IlanaSynthAudioProcessor& processorRef;
    WaveDisplay wave1, wave2, wave3;
    FilterDisplay filterDisplay;
    LfoThumbBar lfoThumbs;
    CardTabs filterTabs, envTabs, lfoTabs;
    std::vector<std::unique_ptr<OscStrip>> strips;
    std::vector<std::unique_ptr<ControlSet>> filterSets, envSets, lfoSets;
    std::array<juce::Rectangle<int>, 3> oscCards;
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
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.setFont (IlanaTheme::font (14.0f));
            g.drawText ("Nothing is modulated yet.", viewport.getBounds().withTrimmedTop (60).withHeight (24),
                        juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 6);
        area.removeFromTop (22);
        headerArea = area.removeFromTop (18);
        viewport.setBounds (area);
        layoutList();
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
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawEllipse (led, 1.0f);
            }

            g.setColour (juce::Colours::white.withAlpha ((slotType != 0 ? 0.92f : 0.3f) * dim));
            g.setFont (IlanaTheme::font (13.0f, true));
            g.drawFittedText (getSlotName (slotType), row.reduced (30, 6).withTrimmedRight (18), 1,
                              juce::Justification::centredLeft);

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
    }

    void paintStack (juce::Graphics& g)
    {
        if (stackPanels.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (13.5f));
            g.drawText ("The rack is empty.  Click a slot on the left to add an effect.",
                        stackContent.getLocalBounds().withHeight (60), juce::Justification::centred);
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
    tabs.addTab ("OSC", juce::Colour (0xff18181c), new OscPage (p), true);
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
        { "F2 ENV", Mod::Source::FilterEnv2 }, { "ENV 4", Mod::Source::Env4 }, { "MSEG", Mod::Source::Mseg },
        { "VELOCITY", Mod::Source::Velocity }, { "KEY", Mod::Source::KeyTrack }, { "RANDOM", Mod::Source::Random },
        { "WHEEL", Mod::Source::ModWheel }, { "PRESSURE", Mod::Source::Aftertouch }
    };

    for (const auto& spec : chipSpecs)
    {
        auto chip = std::make_unique<ModSourceChip> (spec.name, (int) spec.source);
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
    masterKnob->setBounds (strip.removeFromRight (112));
    strip.removeFromRight (6);
    voicesKnob->setBounds (strip.removeFromRight (90));
    voiceModeBox->setBounds (strip.removeFromRight (96).withSizeKeepingCentre (92, 40));
    strip.removeFromRight (8);
    bendKnob->setBounds (strip.removeFromRight (88));
    legatoToggle->setBounds (strip.removeFromRight (80).withSizeKeepingCentre (76, 44));
    glideKnob->setBounds (strip.removeFromRight (96));
    strip.removeFromRight (10);

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
        presetPanel->onLoad = [this] (int index)
        {
            loadPresetIndex (index);

            if (presetPanel != nullptr)
                presetPanel->close();
        };
        presetPanel->onFavouriteChanged = [this] { updateHeaderButtons(); };

        content.addAndMakeVisible (*presetPanel);
        presetPanel->setBounds (designWidth - 14 - 300, 62, 300, 380);
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
