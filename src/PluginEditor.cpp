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

#include "dsp/Modulation.h"
#include "dsp/TableFactory.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/FilterDisplay.h"
#include "gui/LfoDisplay.h"
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
          waveDisplay3 (p, "sub_table", "sub_frame", "sub_unison", "sub_spread", "sub_detune", true, "sub_shape", "sub_mode", 2,
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
          noiseLevel (p.apvts, "noise_level", "NOISE")
    {
        addAndMakeVisible (waveDisplay1);
        addAndMakeVisible (waveDisplay2);
        addAndMakeVisible (waveDisplay3);

        setupLoadButton (loadTableButton1, "osc1_table", 0);
        setupLoadButton (loadTableButton2, "osc2_table", 0);
        setupLoadButton (loadTableButton3, "sub_table", 4);

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
        auto area = getLocalBounds().reduced (12);

        for (int band = 0; band < 3; ++band)
        {
            layoutBand (area.removeFromTop (bandHeight), band);
            area.removeFromTop (bandGap);
        }
    }

private:
    static constexpr int bandHeight = 137;
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
            addTop (&osc1Chord);

            addBottom (isSample ? (juce::Component*) &osc1SampleStart
                                : (isString ? (juce::Component*) &osc1StringDecay : (juce::Component*) &osc1Frame));
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
            addTop (&osc2Chord);

            addBottom (isSample ? (juce::Component*) &osc2SampleStart
                                : (isString ? (juce::Component*) &osc2StringDecay : (juce::Component*) &osc2Frame));
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
            addBottom (&osc2Spread);
        }
        else
        {
            addTop (&subOn);
            addTop (&subMode);
            addTop (isSample ? (juce::Component*) &subSampleTuned
                             : (isString ? (juce::Component*) &subExcite : (juce::Component*) &subTable));
            addTop (isSample ? (juce::Component*) &subSampleLoop
                             : (isString ? nullptr : (juce::Component*) &subShape));
            addTop (isSample ? (juce::Component*) &subSampleReverse : nullptr);
            addTop (&subOctave);
            addTop (&subChord);

            addBottom (isSample ? (juce::Component*) &subSampleStart
                                : (isString ? (juce::Component*) &subStringDecay : (juce::Component*) &subFrame));
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
            addBottom (&subSpread);
            addBottom (&noiseLevel);
        }

        controlBay[(size_t) index] = topRow.getUnion (bottomRow).expanded (4, 0);
        layoutSlots (topRow, top);
        layoutSlots (bottomRow, bottom);
    }

    void setupLoadButton (juce::TextButton& button, const juce::String& tableId, int tableChoiceOffset)
    {
        button.setTooltip ("Load a .wav file into this oscillator's user table slots");
        button.onClick = [this, tableId, tableChoiceOffset]
        {
            if (chooserOpen)
                return;

            chooserOpen = true;

            for (int index = 0; index < 3; ++index)
                loadButton (index).setEnabled (false);

            if (tableChooser == nullptr)
                tableChooser = std::make_unique<juce::FileChooser> (
                    "Load Wavetable (.wav)",
                    juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                    "*.wav");

            juce::Component::SafePointer<OscPage> safeThis (this);

            tableChooser->launchAsync (juce::FileBrowserComponent::openMode
                                           | juce::FileBrowserComponent::canSelectFiles,
                                       [safeThis, tableId, tableChoiceOffset] (const juce::FileChooser& chooser)
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

                                           if (safeThis->processorRef.loadUserWavetable (slot, file))
                                           {
                                               if (auto* parameter = safeThis->processorRef.apvts.getParameter (tableId))
                                                   parameter->setValueNotifyingHost (
                                                       parameter->convertTo0to1 ((float) (tableChoiceOffset + factoryCount + slot)));
                                           }
                                       });
        };
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

        const auto mode3 = getMode (2);
        subTable.setVisible (mode3 == 0);
        subFrame.setVisible (mode3 == 0);
        subShape.setVisible (mode3 == 0);
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
                           &osc1Chord },
                         enabled1);
        setGroupEnabled ({ &osc2Mode, &osc2Table, &osc2Excite, &osc2Frame, &osc2Level, &osc2Pan, &osc2Semi,
                           &osc2Fine, &osc2Unison, &osc2Detune, &osc2Spread,
                           &osc2StringDecay, &osc2StringDamp, &osc2StringSustain,
                           &osc2SampleTuned, &osc2SampleLoop, &osc2SampleReverse,
                           &osc2SampleStart, &osc2SampleEnd, &osc2SampleFadeIn, &osc2SampleFadeOut,
                           &osc2Chord },
                         enabled2);
        setGroupEnabled ({ &subMode, &subTable, &subExcite, &subFrame, &subShape, &subOctave, &subLevel,
                           &subPan, &subSemi, &subFine, &subUnison, &subDetune, &subSpread,
                           &subStringDecay, &subStringDamp, &subStringSustain,
                           &subSampleTuned, &subSampleLoop, &subSampleReverse,
                           &subSampleStart, &subSampleEnd, &subSampleFadeIn, &subSampleFadeOut,
                           &subChord, &noiseLevel },
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
};

class RoutingSwitch : public juce::Button
{
public:
    RoutingSwitch() : juce::Button ("filters_parallel")
    {
        setClickingTogglesState (true);
        setTooltip ("Serial: Filter 1 feeds Filter 2.   Parallel: both filters run side by side and are summed.");
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto parallel = getToggleState();

        IlanaTheme::paintCard (g, bounds, 6.0f, IlanaTheme::accent().withAlpha (0.35f));

        const auto halfWidth = bounds.getWidth() * 0.5f;

        for (int half = 0; half < 2; ++half)
        {
            const auto area = juce::Rectangle<float> (bounds.getX() + (float) half * halfWidth, bounds.getY(),
                                                      halfWidth, bounds.getHeight()).reduced (6.0f, 4.0f);
            const auto active = (half == 1) == parallel;
            const auto colour = half == 1 ? juce::Colour (0xffb28aff) : juce::Colour (0xffff4fd8);

            if (active)
            {
                g.setColour (colour.withAlpha (0.13f));
                g.fillRoundedRectangle (area.expanded (3.0f, 1.0f), 5.0f);
                g.setColour (colour.withAlpha (0.65f));
                g.drawRoundedRectangle (area.expanded (3.0f, 1.0f), 5.0f, 1.0f);
            }

            drawDiagram (g, area, half == 1, active ? 1.0f : 0.35f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (bounds.getCentreX() - 0.5f, bounds.getY() + 8.0f, 1.0f, bounds.getHeight() - 16.0f);

        if (over || down)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.3f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.2f);
        }
    }

private:
    static void drawDiagram (juce::Graphics& g, juce::Rectangle<float> area, bool parallel, float alpha)
    {
        const auto colour = (parallel ? juce::Colour (0xffb28aff) : juce::Colour (0xffff4fd8)).withAlpha (alpha);
        auto labelArea = area.removeFromBottom (12.0f);

        constexpr auto boxWidth = 30.0f;
        constexpr auto boxHeight = 14.0f;

        const auto drawBox = [&g, colour] (juce::Rectangle<float> box, const juce::String& boxText)
        {
            g.setColour (colour.withAlpha (colour.getFloatAlpha() * 0.3f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (box, 3.0f, 1.0f);
            g.setFont (IlanaTheme::font (10.0f, true));
            g.drawText (boxText, box.toNearestInt(), juce::Justification::centred);
        };

        const auto arrow = [&g, colour] (float x1, float y1, float x2, float y2)
        {
            g.setColour (colour);
            g.drawLine (x1, y1, x2, y2, 1.2f);

            juce::Path head;
            head.addTriangle (x2 - 4.0f, y2 - 2.4f, x2 - 4.0f, y2 + 2.4f, x2, y2);
            g.fillPath (head);
        };

        if (! parallel)
        {
            const auto y = area.getCentreY();
            const auto x = area.getX();

            drawBox ({ x, y - boxHeight * 0.5f, boxWidth, boxHeight }, "F1");
            arrow (x + boxWidth + 1.0f, y, x + boxWidth + 10.0f, y);
            drawBox ({ x + boxWidth + 13.0f, y - boxHeight * 0.5f, boxWidth, boxHeight }, "F2");
            arrow (x + boxWidth * 2.0f + 14.0f, y, x + boxWidth * 2.0f + 24.0f, y);
        }
        else
        {
            const auto x = area.getX();
            const auto yTop = area.getCentreY() - boxHeight * 0.62f;
            const auto yBottom = area.getCentreY() + boxHeight * 0.62f;
            const auto sumX = x + boxWidth + 13.0f;

            drawBox ({ x, yTop - boxHeight * 0.5f, boxWidth, boxHeight }, "F1");
            drawBox ({ x, yBottom - boxHeight * 0.5f, boxWidth, boxHeight }, "F2");

            arrow (x + boxWidth + 1.0f, yTop, sumX - 1.0f, yTop);
            arrow (x + boxWidth + 1.0f, yBottom, sumX - 1.0f, yBottom);

            g.setColour (colour);
            g.fillRect (sumX - 1.5f, yTop - boxHeight * 0.5f, 1.5f, yBottom - yTop + boxHeight);
            arrow (sumX, area.getCentreY(), sumX + 9.0f, area.getCentreY());
        }

        g.setColour (colour);
        g.setFont (IlanaTheme::font (10.5f, true));
        g.drawText (parallel ? "PARALLEL" : "SERIAL", labelArea.toNearestInt(), juce::Justification::centred);
    }
};

class FilterPage : public juce::Component
{
public:
    explicit FilterPage (IlanaSynthAudioProcessor& p)
        : filterDisplay (p),
          f1Type (p.apvts, "f1_type", "TYPE"),
          f1Slope (p.apvts, "f1_slope", "SLOPE"),
          f1Cutoff (p.apvts, "f1_cutoff", "CUTOFF", juce::Colour (0xffff4fd8), false),
          f1Reso (p.apvts, "f1_reso", "RESO", juce::Colour (0xffff4fd8), false),
          f1Drive (p.apvts, "f1_drive", "DRIVE", juce::Colour (0xffff4fd8), false),
          f1Env (p.apvts, "f1_env", "ENV AMT", juce::Colour (0xffff4fd8), false),
          f1Key (p.apvts, "f1_keytrack", "KEY TRK", juce::Colour (0xffff4fd8), false),
          f1Fm (p.apvts, "f1_fm", "FM", juce::Colour (0xffff4fd8), false),
          f2Type (p.apvts, "f2_type", "TYPE"),
          f2Slope (p.apvts, "f2_slope", "SLOPE"),
          f2Cutoff (p.apvts, "f2_cutoff", "CUTOFF", juce::Colour (0xffb28aff), false),
          f2Reso (p.apvts, "f2_reso", "RESO", juce::Colour (0xffb28aff), false),
          f2Drive (p.apvts, "f2_drive", "DRIVE", juce::Colour (0xffb28aff), false),
          f2Env (p.apvts, "f2_env", "ENV AMT", juce::Colour (0xffb28aff), false),
          f2Key (p.apvts, "f2_keytrack", "KEY TRK", juce::Colour (0xffb28aff), false),
          f2Fm (p.apvts, "f2_fm", "FM", juce::Colour (0xffb28aff), false)
    {
        addAndMakeVisible (filterDisplay);
        addAll (*this, f1Type, f1Slope, f1Cutoff, f1Reso, f1Drive, f1Env, f1Key, f1Fm,
                f2Type, f2Slope, f2Cutoff, f2Reso, f2Drive, f2Env, f2Key, f2Fm);

        addAndMakeVisible (routing);
        routingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            p.apvts, "filters_parallel", routing);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintSectionTitle (g, "RESPONSE  (drag the markers)", { 14, 12, 400, 16 });

        g.setColour (juce::Colour (0xffff4fd8));
        g.setFont (IlanaTheme::font (13.0f, true));
        g.fillEllipse (14.0f, 148.0f, 6.0f, 6.0f);
        g.drawText ("FILTER 1", juce::Rectangle<int> (28, 143, 300, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffb28aff));
        g.fillEllipse (14.0f, 258.0f, 6.0f, 6.0f);
        g.drawText ("FILTER 2", juce::Rectangle<int> (28, 253, 300, 16), juce::Justification::centredLeft);

        paintSectionTitle (g, "ROUTING", { 14, 365, 300, 16 });

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (12.5f));
        g.drawFittedText ("Serial chains Filter 1 into Filter 2, each with its own level controls on the left.  "
                          "Parallel runs both filters from the oscillators and sums them.",
                    juce::Rectangle<int> (256, 363, 760, 44), juce::Justification::centredLeft, 2);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        area.removeFromTop (18);
        filterDisplay.setBounds (area.removeFromTop (110));
        area.removeFromTop (18);

        auto row1 = area.removeFromTop (92);
        layoutRow (row1, { &f1Type, &f1Slope, &f1Cutoff, &f1Reso, &f1Drive, &f1Env, &f1Key, &f1Fm });

        area.removeFromTop (20);

        auto row2 = area.removeFromTop (92);
        layoutRow (row2, { &f2Type, &f2Slope, &f2Cutoff, &f2Reso, &f2Drive, &f2Env, &f2Key, &f2Fm });

        area.removeFromTop (20);

        auto row3 = area.removeFromTop (50);
        routing.setBounds (row3.removeFromLeft (230).reduced (0, 2));
    }

private:
    FilterDisplay filterDisplay;
    ComboControl f1Type, f1Slope;
    KnobControl f1Cutoff, f1Reso, f1Drive, f1Env, f1Key, f1Fm;
    ComboControl f2Type, f2Slope;
    KnobControl f2Cutoff, f2Reso, f2Drive, f2Env, f2Key, f2Fm;
    RoutingSwitch routing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> routingAttachment;
};

class EnvSection : public juce::Component
{
public:
    EnvSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : settings (settingsRef),
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
        addAndMakeVisible (tabBar);

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

        revealed = juce::jlimit (1, (int) units.size(), settings.getIntValue ("envRevealed", 1));

        tabBar.onSelect = [this] (int index)
        {
            selected = index;
            updateVisibility();
        };

        tabBar.onAdd = [this]
        {
            if (revealed < (int) units.size())
            {
                ++revealed;
                selected = revealed - 1;
                settings.setValue ("envRevealed", revealed);
                updateVisibility();
            }
        };

        updateVisibility();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        tabBar.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (area.removeFromLeft (470).reduced (2));

        auto knobRow = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 130));
        layoutFixed (knobRow, units[(size_t) unitIndex].knobs);
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

        tabBar.setItems ({ "AMP", "FILTER 1", "FILTER 2", "MOD", "ENV 4" }, revealed, selected);
        resized();
        repaint();
    }

    juce::PropertiesFile& settings;
    SubTabBar tabBar;
    EnvelopeDisplay ampDisplay, feDisplay, f2eDisplay, meDisplay, e4Display;
    KnobControl ampA, ampD, ampS, ampR, ampVel, ampCurve;
    KnobControl feA, feD, feS, feR, feVel, feCurve;
    KnobControl f2A, f2D, f2S, f2R, f2Curve;
    KnobControl meA, meD, meS, meR, meCurve;
    KnobControl e4A, e4D, e4S, e4R, e4Curve;
    std::vector<Unit> units;
    int revealed = 1;
    int selected = 0;
};
class LfoSection : public juce::Component
{
public:
    LfoSection (IlanaSynthAudioProcessor& p, juce::PropertiesFile& settingsRef)
        : processorRef (p),
          settings (settingsRef)
    {
        addAndMakeVisible (tabBar);

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
        {
            auto display = std::make_unique<LfoDisplay> (p, lfo, lfoColour (lfo), lfo == 0);
            addAndMakeVisible (*display);
            displays.push_back (std::move (display));

            auto controls = std::make_unique<Controls> (p.apvts, lfo + 1, lfoColour (lfo), lfo == 0);
            addAll (*this, controls->shape, controls->rate, controls->sync, controls->div, controls->retrig);
            controlsList.push_back (std::move (controls));
        }

        revealed = juce::jlimit (1, IlanaSynthAudioProcessor::numLfos,
                                 settings.getIntValue ("lfoRevealed", 1));

        tabBar.onSelect = [this] (int index)
        {
            selected = index;
            updateVisibility();
        };

        tabBar.onAdd = [this]
        {
            if (revealed < IlanaSynthAudioProcessor::numLfos)
            {
                ++revealed;
                selected = revealed - 1;
                settings.setValue ("lfoRevealed", revealed);
                updateVisibility();
            }
        };

        updateVisibility();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        tabBar.setBounds (area.removeFromTop (24));
        area.removeFromTop (4);

        const auto displayIndex = juce::jlimit (0, (int) displays.size() - 1, selected);
        displays[(size_t) displayIndex]->setBounds (area.removeFromLeft (470).reduced (2));

        auto knobRow = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 130));
        layoutRow (knobRow, { &controlsList[(size_t) displayIndex]->shape, &controlsList[(size_t) displayIndex]->rate,
                              &controlsList[(size_t) displayIndex]->sync, &controlsList[(size_t) displayIndex]->div,
                              &controlsList[(size_t) displayIndex]->retrig });
    }

private:
    struct Controls
    {
        Controls (juce::AudioProcessorValueTreeState& state, int lfo, juce::Colour accent, bool followsTheme)
            : shape (state, "lfo" + juce::String (lfo) + "_shape", "SHAPE"),
              rate (state, "lfo" + juce::String (lfo) + "_rate", "RATE", accent, followsTheme),
              sync (state, "lfo" + juce::String (lfo) + "_sync", "SYNC"),
              div (state, "lfo" + juce::String (lfo) + "_div", "DIVISION"),
              retrig (state, "lfo" + juce::String (lfo) + "_retrig", "RETRIG")
        {
        }

        ComboControl shape;
        KnobControl rate;
        ToggleControl sync;
        ComboControl div;
        ToggleControl retrig;
    };

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
        }

        juce::StringArray names;

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            names.add ("LFO " + juce::String (lfo + 1));

        tabBar.setItems (names, revealed, selected);
        resized();
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile& settings;
    SubTabBar tabBar;
    std::vector<std::unique_ptr<LfoDisplay>> displays;
    std::vector<std::unique_ptr<Controls>> controlsList;
    int revealed = 1;
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

class XtraPage : public juce::Component,
                 private juce::Timer
{
public:
    explicit XtraPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          voiceSpread (p.apvts, "voice_spread", "VOICE SPREAD", xtraColour (0), true),
          unisonRandom (p.apvts, "unison_random", "UNI PHASE RND", xtraColour (0), true),
          masterClip (p.apvts, "master_clip", "SOFT CLIP"),
          mpeMode (p.apvts, "mpe_mode", "MPE"),
          clipGain (p.apvts, "master_clip_gain", "CLIP GAIN", xtraColour (0), false),
          fmAmount (p.apvts, "fm_amount", "FM  OSC2>OSC1", xtraColour (1), false),
          fmFeedback (p.apvts, "fm_feedback", "FM FEEDBACK", xtraColour (1), false),
          ringMod (p.apvts, "ring_mod", "RING MOD", xtraColour (1), false),
          hardSync (p.apvts, "hard_sync", "HARD SYNC"),
          drift (p.apvts, "drift", "DRIFT", xtraColour (1), false),
          arpOn (p.apvts, "arp_on", "ARP"),
          arpMode (p.apvts, "arp_mode", "MODE"),
          arpDiv (p.apvts, "arp_div", "RATE"),
          arpOctaves (p.apvts, "arp_octaves", "OCTAVES", xtraColour (2), false),
          arpGate (p.apvts, "arp_gate", "GATE", xtraColour (2), false),
          resOn (p.apvts, "res_on", "RESONATOR"),
          resAmount (p.apvts, "res_amount", "RES AMOUNT", xtraColour (3), false),
          resDecay (p.apvts, "res_decay", "RES DECAY", xtraColour (3), false),
          resOffset (p.apvts, "res_offset", "RES OFFSET", xtraColour (3), false),
          resKeytrack (p.apvts, "res_keytrack", "RES KEYTRK", xtraColour (3), false),
          voiceDisplay (p),
          crossModDisplay (p, xtraColour (1)),
          arpDisplay (p, xtraColour (2)),
          resonatorDisplay (p, xtraColour (3))
    {
        addAndMakeVisible (tabBar);
        addAll (*this, voiceDisplay, crossModDisplay, arpDisplay, resonatorDisplay,
                voiceSpread, unisonRandom, masterClip, mpeMode, clipGain,
                fmAmount, fmFeedback, ringMod, hardSync, drift,
                arpOn, arpMode, arpDiv, arpOctaves, arpGate,
                resOn, resAmount, resDecay, resOffset, resKeytrack);

        units.push_back ({ &voiceDisplay, { &voiceSpread, &unisonRandom, &masterClip, &mpeMode, &clipGain } });
        units.push_back ({ &crossModDisplay, { &fmAmount, &fmFeedback, &ringMod, &hardSync, &drift } });
        units.push_back ({ &arpDisplay, { &arpOn, &arpMode, &arpDiv, &arpOctaves, &arpGate }, "arp_on" });
        units.push_back ({ &resonatorDisplay, { &resOn, &resAmount, &resDecay, &resOffset, &resKeytrack }, "res_on" });

        knobAlphas.resize (units.size());

        for (size_t u = 0; u < units.size(); ++u)
            knobAlphas[u].assign (units[u].knobs.size(), 1.0f);

        tabBar.onSelect = [this] (int index)
        {
            selected = index;
            updateVisibility();
        };

        updateVisibility();
        startTimerHz (30);
    }

    static juce::Colour xtraColour (int index)
    {
        switch (index)
        {
            case 1: return juce::Colour (0xffe3a56f);
            case 2: return juce::Colour (0xff6fe3c1);
            case 3: return juce::Colour (0xffb28aff);
            default: return IlanaTheme::accent();
        }
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const juce::StringArray titles { "VOICE", "CROSS MODULATION", "ARPEGGIATOR", "RESONATOR" };
        const auto colour = xtraColour (selected);

        g.setColour (colour);
        g.setFont (IlanaTheme::font (13.0f, true));
        g.fillEllipse (14.0f, 44.0f, 6.0f, 6.0f);
        g.drawText (titles[juce::jlimit (0, titles.size() - 1, selected)],
                    juce::Rectangle<int> (28, 40, 300, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText (juce::String (selected + 1) + " / 4",
                    juce::Rectangle<int> (getWidth() - 76, 40, 60, 16), juce::Justification::centredRight);

        if (! controlBay.isEmpty())
        {
            IlanaTheme::paintRecessedPanel (g, controlBay.toFloat(), 6.0f);

            const auto caption = controlBay.withHeight (16).translated (0, 2);

            g.setColour (colour.withAlpha (0.85f));
            g.setFont (IlanaTheme::font (11.0f, true));
            g.drawText (titles[juce::jlimit (0, titles.size() - 1, selected)] + " CONTROLS",
                        caption.reduced (10, 0), juce::Justification::centredLeft);
        }

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (12.5f));
        g.drawText ("Cross mod shapes the oscillators, the arpeggiator feeds notes and the resonator "
                    "adds tuned body after the filters.",
                    juce::Rectangle<int> (14, 424, 1000, 16), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        tabBar.setBounds (area.removeFromTop (26));
        area.removeFromTop (6);

        auto row = area.removeFromTop (313);
        row.removeFromTop (18);

        const auto unitIndex = juce::jlimit (0, (int) units.size() - 1, selected);
        units[(size_t) unitIndex].display->setBounds (row.removeFromLeft (470).reduced (2));

        controlBay = row.reduced (0, 4);

        auto knobRow = controlBay.withTrimmedTop (20);
        const auto knobHeight = juce::jmin (knobRow.getHeight(), 180);
        layoutRow (knobRow.withSizeKeepingCentre (knobRow.getWidth(), knobHeight), units[(size_t) unitIndex].knobs);
    }

private:
    struct Unit
    {
        juce::Component* display = nullptr;
        std::vector<juce::Component*> knobs;
        const char* enableParam = nullptr;
    };

    void updateVisibility()
    {
        for (int i = 0; i < (int) units.size(); ++i)
        {
            const auto visible = i == selected;
            units[(size_t) i].display->setVisible (visible);

            for (auto* knob : units[(size_t) i].knobs)
                knob->setVisible (visible);
        }

        if (juce::isPositiveAndBelow (selected, (int) units.size()))
        {
            if (auto* animated = dynamic_cast<IlanaAnim::PageAnimated*> (units[(size_t) selected].display))
                animated->replayAppear();
        }

        tabBar.setItems ({ "VOICE", "CROSS MOD", "ARPEGGIATOR", "RESONATOR" }, 4, selected);
        resized();
        repaint();
    }

    void timerCallback() override
    {
        for (size_t u = 0; u < units.size(); ++u)
        {
            auto& unit = units[u];

            if (unit.enableParam == nullptr)
                continue;

            const auto on = readFloat (unit.enableParam) > 0.5f;
            const auto target = on ? 1.0f : 0.45f;

            for (size_t k = 1; k < unit.knobs.size(); ++k)
            {
                auto& alpha = knobAlphas[u][k];

                if (std::abs (alpha - target) < 0.004f)
                    continue;

                alpha = IlanaAnim::approach (alpha, target, 0.25f);
                unit.knobs[k]->setAlpha (alpha);
            }
        }

        if (pendingRepaint)
        {
            pendingRepaint = false;
            repaint();
        }
    }

    float readFloat (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    SubTabBar tabBar;
    VoiceDisplay voiceDisplay;
    CrossModDisplay crossModDisplay;
    ArpDisplay arpDisplay;
    ResonatorDisplay resonatorDisplay;
    IlanaSynthAudioProcessor& processorRef;
    juce::Rectangle<int> controlBay;
    std::vector<std::vector<float>> knobAlphas;
    KnobControl voiceSpread, unisonRandom, clipGain;
    ToggleControl masterClip, mpeMode;
    KnobControl fmAmount, fmFeedback, ringMod;
    ToggleControl hardSync;
    KnobControl drift;
    ToggleControl arpOn;
    ComboControl arpMode, arpDiv;
    KnobControl arpOctaves, arpGate;
    ToggleControl resOn;
    KnobControl resAmount, resDecay, resOffset, resKeytrack;
    std::vector<Unit> units;
    int selected = 0;
    bool pendingRepaint = false;
};

class SeqPage : public juce::Component
{
public:
    explicit SeqPage (IlanaSynthAudioProcessor& p)
        : step1 (p, 0, IlanaTheme::accent(), true),
          step2 (p, 1, juce::Colour (0xff35c8ff)),
          mseg (p),
          msegLoop (p.apvts, "mseg_loop", "LOOP"),
          msegRate (p.apvts, "mseg_rate", "RATE"),
          clockDiv (p.apvts, "clock_div", "CLOCK DIV")
    {
        addAndMakeVisible (step1);
        addAndMakeVisible (step2);
        addAndMakeVisible (mseg);
        addAndMakeVisible (msegLoop);
        addAndMakeVisible (msegRate);
        addAndMakeVisible (clockDiv);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (13.0f, true));
        g.fillEllipse (14.0f, 12.0f, 6.0f, 6.0f);
        g.drawText ("STEP LFO 1", juce::Rectangle<int> (28, 8, 300, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff35c8ff));
        g.fillEllipse (14.0f, 136.0f, 6.0f, 6.0f);
        g.drawText ("STEP LFO 2", juce::Rectangle<int> (28, 132, 300, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff6fe3c1));
        g.fillEllipse (14.0f, 260.0f, 6.0f, 6.0f);
        g.drawText ("MSEG", juce::Rectangle<int> (28, 256, 300, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (12.5f));
        g.drawText ("Set the LFO shape to Steps, then draw here.  Assign them in the MATRIX tab.",
                    juce::Rectangle<int> (14, 424, 1000, 16), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto row = area.removeFromTop (120);
        row.removeFromTop (22);
        step1.setBounds (row);

        area.removeFromTop (4);
        row = area.removeFromTop (120);
        row.removeFromTop (22);
        step2.setBounds (row);

        area.removeFromTop (4);
        row = area.removeFromTop (160);
        row.removeFromTop (22);
        mseg.setBounds (row.removeFromLeft (600).reduced (2));
        row.removeFromTop (10);
        layoutRow (row.removeFromLeft (330), { &msegLoop, &msegRate, &clockDiv });
    }

private:
    StepEditor step1, step2;
    MsegEditor mseg;
    ToggleControl msegLoop;
    KnobControl msegRate;
    KnobControl clockDiv;
};

class MatrixPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit MatrixPage (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        for (int i = 1; i <= Mod::maxSlots; ++i)
        {
            const auto prefix = "mod" + juce::String (i);
            sources.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_src", i == 1 ? "SOURCE" : ""));
            destinations.push_back (std::make_unique<ComboControl> (p.apvts, prefix + "_dst", i == 1 ? "DESTINATION" : ""));
            depths.push_back (std::make_unique<ValueSliderControl> (p.apvts, prefix + "_amt"));
        }

        for (size_t i = 0; i < sources.size(); ++i)
            addAll (*this, *sources[i], *destinations[i], *depths[i]);

        updateSourceColours();
        startTimerHz (30);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        auto hovered = -1;

        for (int i = 0; i < Mod::maxSlots; ++i)
            if (event.getPosition().y >= rowTop (i) - 2 && event.getPosition().y < rowTop (i) + rowHeight)
                hovered = i;

        if (hovered != hoveredRow)
        {
            hoveredRow = hovered;
            repaint();
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

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (12.5f));
        g.drawText ("Depth is amount per slot.  Mod wheel / aftertouch / macros are live sources.",
                    juce::Rectangle<int> (14, 8, 600, 16), juce::Justification::centredLeft);

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto prefix = "mod" + juce::String (i + 1);
            const auto source = (int) readParam (prefix + "_src");
            const auto destination = (int) readParam (prefix + "_dst");
            const auto depth = readParam (prefix + "_amt");
            const auto active = source != 0 && destination != 0 && depth != 0.0f;
            const auto sourceColour = source != 0 ? modSourceColour (source) : IlanaTheme::accent();

            if (active)
            {
                g.setColour (sourceColour.withAlpha (0.09f));
                g.fillRoundedRectangle (juce::Rectangle<int> (10, rowTop (i) - 2, getWidth() - 20, rowHeight - 2).toFloat(),
                                        4.0f);
            }

            if (rowHover[(size_t) i] > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f * rowHover[(size_t) i]));
                g.fillRoundedRectangle (juce::Rectangle<int> (10, rowTop (i) - 2, getWidth() - 20, rowHeight - 2).toFloat(),
                                        4.0f);
            }

            g.setColour (active ? sourceColour : juce::Colours::white.withAlpha (0.5f));
            g.drawText (juce::String (i + 1),
                        juce::Rectangle<int> (14, rowTop (i), 24, rowHeight), juce::Justification::centredRight);

            // Live source value meter.
            const auto meterRect = juce::Rectangle<float> (42.0f, (float) rowTop (i) + 11.0f, 12.0f,
                                                           (float) rowHeight - 24.0f);
            const auto value = source != 0 ? processorRef.getSourceDisplayValue (source) : 0.0f;

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (meterRect, 2.0f);

            const auto midY = meterRect.getCentreY();
            const auto half = meterRect.getHeight() * 0.5f;
            const auto barHeight = juce::jlimit (0.0f, half, std::abs (value) * half);

            g.setColour (sourceColour.withAlpha (active ? 0.9f : 0.35f));

            if (value >= 0.0f)
                g.fillRect (juce::Rectangle<float> (meterRect.getX() + 1.0f, midY - barHeight,
                                                    meterRect.getWidth() - 2.0f, barHeight));
            else
                g.fillRect (juce::Rectangle<float> (meterRect.getX() + 1.0f, midY,
                                                    meterRect.getWidth() - 2.0f, barHeight));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (30);

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            auto row = area.removeFromTop (rowHeight);
            row.removeFromLeft (28);
            row.removeFromLeft (18);

            sources[(size_t) i]->setBounds (row.removeFromLeft (200).reduced (2));
            destinations[(size_t) i]->setBounds (row.removeFromLeft (210).reduced (2));
            depths[(size_t) i]->setBounds (row.removeFromLeft (300).reduced (2));
        }
    }

    void lookAndFeelChanged() override
    {
        for (auto& source : lastSources)
            source = -1;

        updateSourceColours();
    }

private:
    static int rowTop (int index) { return 42 + index * rowHeight; }

    void updateSourceColours()
    {
        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto source = (int) readParam ("mod" + juce::String (i + 1) + "_src");

            if (source == lastSources[(size_t) i])
                continue;

            lastSources[(size_t) i] = source;
            depths[(size_t) i]->getSlider().setColour (juce::Slider::rotarySliderFillColourId,
                                                       source != 0 ? modSourceColour (source) : IlanaTheme::accent());
        }
    }

    void timerCallback() override
    {
        updateSourceColours();

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto target = i == hoveredRow ? 1.0f : 0.0f;
            rowHover[(size_t) i] += (target - rowHover[(size_t) i]) * 0.25f;
        }

        repaint();
    }

    float readParam (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    static constexpr int rowHeight = 42;

    IlanaSynthAudioProcessor& processorRef;

    std::vector<std::unique_ptr<ComboControl>> sources;
    std::vector<std::unique_ptr<ComboControl>> destinations;
    std::vector<std::unique_ptr<ValueSliderControl>> depths;
    std::array<float, (size_t) Mod::maxSlots> rowHover {};
    std::array<int, (size_t) Mod::maxSlots> lastSources {};
    int hoveredRow = -1;
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
          tapsMix (p.apvts, "fx_taps_mix", "MIX"),
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
          feedbackMix (p.apvts, "fx_feedback_mix", "MIX")
    {
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
                gateDiv, gatePattern, gateSmooth, gateMix,
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
        slotGroups.push_back ({ &gateDiv, &gatePattern, &gateSmooth, &gateMix });
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

        slotBlend.setSliderStyle (juce::Slider::LinearHorizontal);
        slotBlend.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        slotBlend.setTooltip ("Parallel blend for the selected slot: 0 is dry only, 1 is the full effect.");
        addAndMakeVisible (slotBlend);

        updateVisibility();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto type = getSlotType (selectedSlot);

        paintSectionTitle (g, "CHAIN", { 14, 6, 200, 14 });

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

            if (dragSource)
                g.setColour (juce::Colour (0xff141416).withAlpha (0.5f));
            else if (selected)
                g.setColour (IlanaTheme::accent().withAlpha (0.22f));
            else
            {
                juce::ColourGradient rowGradient (juce::Colour (0xff20202a).withMultipliedAlpha (dim), 0.0f, (float) row.getY(),
                                                  juce::Colour (0xff16161a).withMultipliedAlpha (dim), 0.0f, (float) row.getBottom(), false);
                g.setGradientFill (rowGradient);
            }

            g.fillRoundedRectangle (row.toFloat(), 6.0f);

            if (! selected && ! dragSource && rowHover[(size_t) slot] > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f * rowHover[(size_t) slot]));
                g.fillRoundedRectangle (row.toFloat(), 6.0f);
            }
            g.setColour ((selected ? IlanaTheme::accent() : juce::Colour (0xff33333a)).withMultipliedAlpha (dim));
            g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 6.0f, selected ? 1.6f : 1.0f);

            if (dragSource)
                continue;

            g.setColour (juce::Colours::white.withAlpha (0.35f * dim));
            g.setFont (IlanaTheme::font (11.0f, true));
            g.drawText (juce::String (slot + 1), row.reduced (8, 0).removeFromLeft (18), juce::Justification::centredLeft);

            const juce::Rectangle<float> led ((float) row.getRight() - 20.0f, (float) row.getCentreY() - 3.0f, 7.0f, 7.0f);

            if (slotType != 0 && ! bypassed)
            {
                g.setColour (IlanaTheme::accent());
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

        if (type != 0 && type < (int) slotGroups.size())
        {
            paintSectionTitle (g, getSlotName (type).toUpperCase() + " PARAMETERS", { 336, 8, 400, 14 });
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.setFont (IlanaTheme::font (13.5f));
            g.drawText ("Right-click a slot to add an effect from the list.",
                        juce::Rectangle<int> (336, 38, 600, 16), juce::Justification::centredLeft);
        }
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
        auto tapArea = panel.removeFromBottom (52);

        for (auto& group : slotGroups)
        {
            auto groupArea = panel;

            if (group.size() > 8)
            {
                const auto half = (int) (group.size() + 1) / 2;
                std::vector<juce::Component*> firstRow (group.begin(), group.begin() + half);
                std::vector<juce::Component*> secondRow (group.begin() + half, group.end());

                layoutRow (groupArea.removeFromTop (130), firstRow);
                layoutRow (groupArea.removeFromTop (130), secondRow);
            }
            else
            {
                layoutRow (groupArea.removeFromTop (150), group);
            }
        }

        tapGrid.setBounds (tapArea);
        loadIrButton.setBounds (440, 15, 90, 18);
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
            updateVisibility();
            repaint();

            if (event.mods.isPopupMenu())
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

        auto* current = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1));
        auto* other = processorRef.apvts.getParameter ("fx_slot" + juce::String (target + 1));

        if (current != nullptr && other != nullptr)
        {
            const auto currentValue = current->getValue();
            const auto otherValue = other->getValue();

            current->setValueNotifyingHost (otherValue);
            other->setValueNotifyingHost (currentValue);
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
        const auto selectedType = getSlotType (selectedSlot);

        for (int type = 0; type < (int) slotGroups.size(); ++type)
            for (auto* control : slotGroups[(size_t) type])
                control->setVisible (type == selectedType);

        tapGrid.setVisible (selectedType == 9);
        loadIrButton.setVisible (selectedType == 13);

        paramsAppear = 0.0f;

        if (boundBlendSlot != selectedSlot)
        {
            boundBlendSlot = selectedSlot;
            slotBlendAttachment.reset();

            if (auto* parameter = processorRef.apvts.getParameter ("fx_slot" + juce::String (selectedSlot + 1) + "_mix"))
                slotBlendAttachment = std::make_unique<juce::SliderParameterAttachment> (*parameter, slotBlend, nullptr);
        }

        resized();
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

        const auto selectedType = getSlotType (selectedSlot);

        if (selectedType < (int) slotGroups.size())
            for (auto* control : slotGroups[(size_t) selectedType])
                control->setAlpha (paramsAppear);

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
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;
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
    KnobControl gateSmooth, gateMix;
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
    infoStrip.setToolbarButton (&zoomButton);

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

    tabs.addTab ("OSC", juce::Colour (0xff18181c), new OscPage (p), true);
    tabs.addTab ("FILTER", juce::Colour (0xff18181c), new FilterPage (p), true);
    tabs.addTab ("ENV/LFO", juce::Colour (0xff18181c), new EnvLfoPage (p, *settings), true);

    seqPage = std::make_unique<SeqPage> (p);
    tabs.addTab ("SEQ", juce::Colour (0xff18181c), seqPage.get(), false, 3);

    tabs.addTab ("XTRA", juce::Colour (0xff18181c), new XtraPage (p), true);
    tabs.addTab ("MATRIX", juce::Colour (0xff18181c), new MatrixPage (p), true);
    tabs.addTab ("FX", juce::Colour (0xff18181c), new FxPage (p), true);
    tabs.addTab ("SCOPE", juce::Colour (0xff18181c), new ScopeDisplay (p), true);

    content.addAndMakeVisible (tabs);

    macro1Knob = std::make_unique<KnobControl> (p.apvts, "macro1", "MACRO 1");
    macro2Knob = std::make_unique<KnobControl> (p.apvts, "macro2", "MACRO 2");
    macro3Knob = std::make_unique<KnobControl> (p.apvts, "macro3", "MACRO 3");
    macro4Knob = std::make_unique<KnobControl> (p.apvts, "macro4", "MACRO 4");
    glideKnob = std::make_unique<KnobControl> (p.apvts, "glide", "GLIDE");
    bendKnob = std::make_unique<KnobControl> (p.apvts, "bend_range", "BEND RANGE");
    masterKnob = std::make_unique<KnobControl> (p.apvts, "master", "MASTER",
                                                juce::Colour (0xffffd447));

    content.addAndMakeVisible (*macro1Knob);
    content.addAndMakeVisible (*macro2Knob);
    content.addAndMakeVisible (*macro3Knob);
    content.addAndMakeVisible (*macro4Knob);
    content.addAndMakeVisible (*glideKnob);
    content.addAndMakeVisible (*bendKnob);
    content.addAndMakeVisible (*masterKnob);

    presetButton.onClick = [this] { togglePresetPanel(); };

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
    initButton.onClick = [this] { loadPresetIndex (0); };
    favButton.setClickingTogglesState (true);
    favButton.onClick = [this] { toggleFavourite(); updateHeaderButtons(); };
    undoButton.onClick = [this] { processorRef.getUndoManager().undo(); };
    redoButton.onClick = [this] { processorRef.getUndoManager().redo(); };
    historyButton.onClick = [this] { showHistoryMenu(); };
    abButton.onClick = [this] { toggleAB(); };
    themeButton.onClick = [this] { cycleTheme(); };
    zoomButton.onClick = [this]
    {
        const float choices[] { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

        juce::PopupMenu menu;

        for (int i = 0; i < 5; ++i)
            menu.addItem (i + 1, juce::String (juce::roundToInt (choices[i] * 100.0f)) + "%",
                          true, juce::approximatelyEqual (uiZoom, choices[i]));

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&zoomButton),
                            [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                const float zoomChoices[] { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
                                safeThis->applyUiZoom (zoomChoices[juce::jlimit (0, 4, result - 1)]);
                            });
    };
    diceButton.onClick = [this] { randomize(); };

    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };

    for (auto* button : { &presetButton, &prevButton, &nextButton, &initButton, &favButton,
                          &undoButton, &redoButton, &historyButton, &abButton, &themeButton, &diceButton,
                          &saveButton, &loadButton })
        content.addAndMakeVisible (*button);

    keyboard = std::make_unique<KeyboardStrip> (p);
    content.addAndMakeVisible (*keyboard);

    setWantsKeyboardFocus (true);

    const struct
    {
        const char* name;
        int source;
    } chipSpecs[] = {
        { "LFO 1", 1 }, { "LFO 2", 2 }, { "LFO 3", 20 }, { "LFO 4", 21 },
        { "MOD ENV", 3 }, { "FILT ENV", 4 }, { "VELOCITY", 6 },
        { "MOD WHEEL", 9 }, { "MACRO 1", 12 }, { "MACRO 2", 13 }, { "MACRO 3", 14 }, { "MACRO 4", 15 }
    };

    for (const auto& spec : chipSpecs)
    {
        auto chip = std::make_unique<ModSourceChip> (spec.name, spec.source);
        content.addAndMakeVisible (*chip);
        chips.push_back (std::move (chip));
    }

    content.addAndMakeVisible (tutorial);

    // Applied after adding: addAndMakeVisible forces the component visible.
    tutorial.setVisible (! settings->getBoolValue ("seenIntro", false));

    if (tutorial.isVisible())
        tutorial.toFront (false);

    updateHeaderButtons();
    updateUndoButtons();
    updateSeqTab();

    setResizable (true, false);

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

void IlanaSynthAudioProcessorEditor::updateSeqTab()
{
    const auto readInt = [this] (const juce::String& id)
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return (int) value->load();

        return 0;
    };

    auto wanted = false;

    for (int lfo = 1; lfo <= 4 && ! wanted; ++lfo)
        wanted = readInt ("lfo" + juce::String (lfo) + "_shape") == 7;

    for (int slot = 1; slot <= Mod::maxSlots && ! wanted; ++slot)
        wanted = readInt ("mod" + juce::String (slot) + "_src") == (int) Mod::Source::Mseg;

    if (wanted == seqTabVisible)
        return;

    constexpr int seqIndex = 3;

    if (wanted)
    {
        if (seqPage != nullptr)
            tabs.addTab ("SEQ", juce::Colour (0xff18181c), seqPage.get(), false, seqIndex);
    }
    else
    {
        if (tabs.getNumTabs() > seqIndex && tabs.getTabNames()[seqIndex] == "SEQ")
        {
            if (tabs.getCurrentTabIndex() == seqIndex)
                tabs.setCurrentTabIndex (2);

            if (auto* seqContent = tabs.getTabContentComponent (seqIndex))
            {
                seqContent->setVisible (false);
                tabs.removeTab (seqIndex);

                if (auto* parent = seqContent->getParentComponent())
                    parent->removeChildComponent (seqContent);
            }
        }
    }

    seqTabVisible = wanted;
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

    zoomButton.setButtonText ("UI " + juce::String (juce::roundToInt (uiZoom * 100.0f)) + "%");

    if (settings != nullptr)
    {
        settings->setValue ("uiZoom", uiZoom);
        settings->saveIfNeeded();
    }

    setResizeLimits (juce::roundToInt (795.0f * uiZoom), juce::roundToInt (540.0f * uiZoom),
                     juce::roundToInt (1590.0f * uiZoom), juce::roundToInt (1080.0f * uiZoom));

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

    updateSeqTab();

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

    if (presetLoadFlash > 0.01f)
    {
        const auto flashBounds = presetButton.getBounds().toFloat().expanded (3.0f, 2.5f);

        juce::ColourGradient flash (IlanaTheme::accent().withAlpha (0.24f * presetLoadFlash), flashBounds.getX(), flashBounds.getY(),
                                    IlanaTheme::accent().withAlpha (0.02f * presetLoadFlash), flashBounds.getX(), flashBounds.getBottom(), false);
        g.setGradientFill (flash);
        g.fillRoundedRectangle (flashBounds, 7.0f);

        g.setColour (IlanaTheme::accent().withAlpha (0.5f * presetLoadFlash));
        g.drawRoundedRectangle (flashBounds, 7.0f, 1.4f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.setFont (IlanaTheme::font (10.5f));
    g.drawText (juce::String ("aggressive wavetable synthesizer   -   v") + appVersion,
                juce::Rectangle<int> (288, 40, 340, 14), juce::Justification::centredLeft);

    juce::ColourGradient headerLine (IlanaTheme::accent().withAlpha (0.5f), 16.0f, 0.0f,
                                     IlanaTheme::accent().withAlpha (0.0f), (float) designWidth - 16.0f, 0.0f, false);
    g.setGradientFill (headerLine);
    g.fillRect (juce::Rectangle<int> (16, 54, designWidth - 32, 2));

    const auto cpu = processorRef.getCpuUsage() * 100.0f;

    const auto cpuColour = cpu < 30.0f
                               ? juce::Colours::white.withAlpha (0.45f)
                               : (cpu < 60.0f
                                      ? juce::Colours::white.withAlpha (0.45f)
                                            .interpolatedWith (IlanaTheme::accent(), (cpu - 30.0f) / 30.0f)
                                      : IlanaTheme::accent().interpolatedWith (juce::Colours::red,
                                                                               juce::jlimit (0.0f, 1.0f, (cpu - 60.0f) / 40.0f)));

    g.setColour (cpuColour);
    g.setFont (IlanaTheme::font (11.5f));
    g.drawText ("CPU " + juce::String (cpu, 0) + "%",
                juce::Rectangle<int> (designWidth - 118, 41, 100, 12), juce::Justification::centredRight);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawText ("BPM " + juce::String (processorRef.getCurrentBpm(), 1),
                juce::Rectangle<int> (designWidth - 330, 41, 80, 12), juce::Justification::centredRight);

    const auto activeVoices = processorRef.getActiveVoiceCount();

    for (int i = 0; i < 16; ++i)
    {
        const auto lit = i < activeVoices;
        g.setColour (lit ? IlanaTheme::accent().withAlpha (0.9f)
                         : juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse ((float) (designWidth - 244 + i * 7), 43.0f, 5.0f, 5.0f);
    }
}

void IlanaSynthAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) designWidth;
    IlanaTheme::uiScaleRef() = scale * hostScaleFactor();

    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale));

    auto area = content.getLocalBounds();

    auto headerRow = area.removeFromTop (56).reduced (14, 10);
    headerRow.removeFromLeft (300);

    logo.setBounds (16, 5, 264, 46);

    // SAVE and LOAD are placed first so they can never be squeezed out, then
    // the smaller controls from the right, and the preset display takes
    // whatever is left in the middle.
    loadButton.setBounds (headerRow.removeFromLeft (50).reduced (0, 6));
    headerRow.removeFromLeft (4);
    saveButton.setBounds (headerRow.removeFromLeft (50).reduced (0, 6));
    headerRow.removeFromLeft (8);

    themeButton.setBounds (headerRow.removeFromRight (46).reduced (0, 6));
    headerRow.removeFromRight (6);
    diceButton.setBounds (headerRow.removeFromRight (46).reduced (0, 6));
    headerRow.removeFromRight (6);
    abButton.setBounds (headerRow.removeFromRight (32).reduced (0, 6));
    headerRow.removeFromRight (6);
    redoButton.setBounds (headerRow.removeFromRight (52).reduced (0, 6));
    headerRow.removeFromRight (4);
    historyButton.setBounds (headerRow.removeFromRight (48).reduced (0, 6));
    headerRow.removeFromRight (4);
    undoButton.setBounds (headerRow.removeFromRight (52).reduced (0, 6));
    headerRow.removeFromRight (8);
    favButton.setBounds (headerRow.removeFromRight (42).reduced (0, 6));
    headerRow.removeFromRight (4);
    initButton.setBounds (headerRow.removeFromRight (46).reduced (0, 6));
    headerRow.removeFromRight (4);
    nextButton.setBounds (headerRow.removeFromRight (26).reduced (0, 6));
    prevButton.setBounds (headerRow.removeFromRight (26).reduced (0, 6));
    headerRow.removeFromRight (4);

    presetButton.setBounds (headerRow.reduced (0, 6));

    auto bottom = area.removeFromBottom (178);

    auto chipsRow = bottom.removeFromTop (28).reduced (14, 2);

    if (! chips.empty())
    {
        const auto chipWidth = chipsRow.getWidth() / (int) chips.size();

        for (auto& chip : chips)
            chip->setBounds (chipsRow.removeFromLeft (chipWidth).reduced (2));
    }

    if (keyboard != nullptr)
        keyboard->setBounds (bottom.removeFromBottom (34).reduced (14, 2));

    infoStrip.setBounds (bottom.removeFromBottom (22).reduced (14, 1));
    tutorial.setBounds (content.getLocalBounds());

    auto macroRow = bottom.reduced (14, 2);
    auto masterArea = macroRow.removeFromRight (104);
    masterKnob->setBounds (masterArea);

    bendKnob->setBounds (macroRow.removeFromRight (86).reduced (4, 0));
    glideKnob->setBounds (macroRow.removeFromRight (86).reduced (4, 0));

    const auto macroWidth = macroRow.getWidth() / 4;

    for (auto* knob : { macro1Knob.get(), macro2Knob.get(), macro3Knob.get(), macro4Knob.get() })
        knob->setBounds (macroRow.removeFromLeft (macroWidth).reduced (6, 0));

    tabs.setBounds (area.reduced (14, 0));
}

void IlanaSynthAudioProcessorEditor::savePreset()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile ("ilanaSynth Presets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Save Preset",
                                                       directory.getChildFile ("My Preset.ilanapreset"),
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
    shownPresetName = processorRef.getCurrentPresetName();
    currentPresetIndex = processorRef.getAllPresetNames().indexOf (shownPresetName);

    const auto name = shownPresetName.isNotEmpty() ? shownPresetName : juce::String ("PRESETS");

    presetButton.setButtonText (name + (isFavourite (name) ? "  *" : ""));
    favButton.setToggleState (isFavourite (name), juce::dontSendNotification);
    abButton.setButtonText (showingA ? "A" : "B");
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

void IlanaSynthAudioProcessorEditor::cycleTheme()
{
    themeIndex = (themeIndex + 1) % IlanaTheme::numPalettes;
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]));

    if (settings != nullptr)
    {
        settings->setValue ("themeIndex", themeIndex);
        settings->saveIfNeeded();
    }

    sendLookAndFeelChange();
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
    setValue ("sub_level", randomRange (0.0f, 0.6f));
    setValue ("f1_type", (float) random.nextInt (3));
    setValue ("f1_cutoff", randomRange (200.0f, 8000.0f));
    setValue ("f1_reso", randomRange (0.0f, 0.6f));
    setValue ("f1_env", randomRange (-1.0f, 3.0f));
    setValue ("fe_decay", randomRange (0.05f, 1.5f));
    setValue ("amp_attack", random.nextFloat() < 0.3f ? randomRange (0.05f, 0.8f) : 0.005f);
    setValue ("amp_release", randomRange (0.05f, 2.0f));
    setValue ("fm_amount", random.nextFloat() < 0.6f ? randomRange (0.0f, 0.5f) : 0.0f);
    setValue ("fm_feedback", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.4f) : 0.0f);
    setValue ("ring_mod", random.nextFloat() < 0.3f ? randomRange (0.0f, 0.8f) : 0.0f);
    setValue ("hard_sync", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
    setValue ("drift", randomRange (0.0f, 0.5f));
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

    // The FX rack has its own DICE; module on-flags alone do nothing without
    // a slot, so the effects chain is left as it is here.
    setValue ("res_on", random.nextFloat() < 0.25f ? 1.0f : 0.0f);
    setValue ("osc1_chord", (float) random.nextInt (7));
    setValue ("voice_spread", randomRange (0.0f, 0.5f));
    setValue ("unison_random", randomRange (0.0f, 1.0f));
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
