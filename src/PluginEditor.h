#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// macOS and Linux draw the UI through OpenGL (CMake links juce_opengl there);
// Windows already draws with Direct2D.
#if JUCE_MODULE_AVAILABLE_juce_opengl && ! JUCE_WINDOWS
 #include <juce_opengl/juce_opengl.h>
 #define ILANA_GPU_UI 1
#else
 #define ILANA_GPU_UI 0
#endif

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "gui/HeaderWidgets.h"
#include "gui/ConfirmOverlay.h"
#include "gui/IlanaLookAndFeel.h"
#include "gui/InfoStrip.h"
#include "gui/KeyboardStrip.h"
#include "gui/LogoComponent.h"
#include "gui/MacroStrip.h"
#include "gui/ModHoverPopup.h"
#include "gui/ModSourceChip.h"
#include "gui/OutputMeter.h"
#include "gui/OutputView.h"
#include "gui/PresetPanel.h"
#include "gui/SavePresetOverlay.h"
#include "gui/SectionPage.h"
#include "gui/TutorialOverlay.h"

class WavetableEditor;

class IlanaSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       public juce::FileDragAndDropTarget,
                                       private juce::ChangeListener,
                                       private juce::Timer
{
public:
    explicit IlanaSynthAudioProcessorEditor (IlanaSynthAudioProcessor&);
    ~IlanaSynthAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // M7.4: opens the wavetable editor on patch table slot (0-15) over the
    // whole window.
    void openWavetableEditor (int slot, juce::Colour colour);
    void closeWavetableEditor();
    WavetableEditor* getWavetableEditor() const { return wavetableEditor.get(); }

    // Pages by id ("MAIN", "VECTOR", "OSC", "PHYSICAL", "FILTER", "ENV/LFO",
    // "STEPS", "MATRIX", "FM", "ARP/SEQ", "FX", "INPUT"): each lives in one of
    // the seven top-level tabs. "SCOPE" opens the scope panel.
    void showPage (const juce::String& id);
    juce::String getCurrentPageId() const;
    juce::StringArray getPageIds() const;
    juce::Component* getCurrentPage() const;
    void setScopeOpen (bool shouldBeOpen);
    bool isScopeOpen() const;

    // UI review 4 (S1): EDITED covers parameters and the patch's other data
    // (drawn curves, remaps, clips); a load or random patch that would
    // replace an edited patch asks first, unless the user ticked "Don't ask
    // again" (settings file, also in the settings menu).
    bool isPatchEdited() const;
    bool asksBeforeReplacingEdits() const;
    void setAsksBeforeReplacingEdits (bool shouldAsk);

    // UI review 4 (V3): SAVE (Ctrl+S) writes the loaded user preset in
    // place; a factory or never-saved patch goes to SAVE AS (Ctrl+Shift+S),
    // the themed panel that asks before overwriting.
    void savePreset();
    void savePresetAs();
    SavePresetOverlay& getSaveOverlay() { return saveOverlay; }

    // Review 6: the header's EDITED badge as drawn (the confirm reads the
    // same isPatchEdited, and refreshes the badge before it asks); the
    // DX7 bank import (the browser's IMPORT .SYX); the docked browser.
    bool isEditedBadgeShown() const { return presetDisplay.isShowingModified(); }
    void importSyxFile (const juce::File& file);
    // A .syx bank dropped anywhere on the window imports like IMPORT .SYX
    // (the oscillators' sample drop zones take their own files first).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    bool isPresetDockOpen() const { return presetDockShown; }
    InfoStrip& getHoverLine() { return infoStrip; }
    // The header's VOICES count as drawn ("3/32") and its tuning text ("" in
    // 12-TET), for the tests.
    juce::String getVoicesText() const;
    juce::String getTuningIndicatorText() const { return tuningArea.isVisible() ? tuningText : juce::String(); }

    // GPU drawing on macOS and Linux (settings menu > GPU rendering, saved as
    // "gpuRendering", on by default; ILANA_NO_GPU=1 turns it off for a run).
    // Windows always draws with Direct2D, so these do nothing there.
    void setGpuRendering (bool shouldUseGpu);
    bool isGpuRendering() const;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;

        void paint (juce::Graphics& g) override
        {
            if (onPaint != nullptr)
                onPaint (g);
        }
    };

    void paintHeader (juce::Graphics& g);
    void presetSaved (bool inPlace);
    bool closeTopPopup();
    void exportPreset();
    void loadPreset();
    void togglePresetPanel();
    // The browser docked over the page area (the window keeps its size).
    void createPresetPanel();
    void setPresetDockShown (bool shown);
    bool isPresetDockShown() const { return presetDockShown; }
    juce::Rectangle<int> pageArea() const;
    void chooseSyxFile();
    int currentDesignWidth() const { return designWidth; }
    void applyAspectAndLimits();
    void showPresetMenu();
    void showDiceMenu();
    void showSettingsMenu (bool voicesOnly = false, bool tuningOnly = false);
    void randomize();
    void randomizeGroup (int group);
    void mutate (float amount);
    // Asks first when the patch is edited (see isPatchEdited).
    void loadPresetIndex (int index, std::function<void (bool loaded)> then = {});
    void confirmReplacingPatch (const juce::String& replacement, const juce::String& confirmText,
                                std::function<void (bool confirmed)> then);
    void undoOrRedo (bool redo);
    void updateHeaderButtons();
    void updateUndoButtons();
    void showHistoryMenu();
    bool isFavourite (const juce::String& presetName) const;
    void toggleFavourite();
    void toggleAB();
    void setTheme (int newThemeIndex);
    void setKeyboardVisible (bool shouldBeVisible);
    juce::int64 parameterFingerprint() const;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    void startTabTransition (juce::Component* page = nullptr);
    void layoutTabRow();
    SectionPage* currentSection() const;
    void applyDisplayScale();
    void applyUiZoom (float newZoom);
    float displayScale() const;
    float hostScaleFactor() const;

    static constexpr int designWidth = 1060;
    static constexpr int designHeight = 720;
    static constexpr const char* appVersion = "1.3";

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, (size_t) Mod::Source::Count> usedModSources {};
    IlanaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 900 };
    Content content;
    LogoComponent logo;
    InfoStrip infoStrip;
    TutorialOverlay tutorial;
    ConfirmOverlay confirmOverlay;
    SavePresetOverlay saveOverlay { processorRef };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::vector<SectionPage*> sections; // owned by tabs
    std::unique_ptr<juce::Component> scopePanel;
    juce::TextButton scopeButton { "SCOPE" };

    PresetDisplay presetDisplay;
    IconButton prevButton { "prev", IlanaIcons::Icon::ChevronLeft, "Previous preset" };
    IconButton nextButton { "next", IlanaIcons::Icon::ChevronRight, "Next preset" };
    IconButton favButton { "fav", IlanaIcons::Icon::Star, "Favourite\nMark this preset as a favourite." };
    IconButton saveButton { "save", IlanaIcons::Icon::Save, "Save preset\nSave the current sound as a user preset." };
    IconButton moreButton { "more", IlanaIcons::Icon::More, "Preset options\nInit, load from file, open the preset folder." };
    IconButton undoButton { "undo", IlanaIcons::Icon::Undo, "Undo  (Ctrl+Z)" };
    IconButton redoButton { "redo", IlanaIcons::Icon::Redo, "Redo  (Ctrl+Shift+Z)" };
    IconButton historyButton { "history", IlanaIcons::Icon::History, "History\nJump back to any earlier change." };
    ABButton abButton;
    IconButton diceButton { "dice", IlanaIcons::Icon::Dice, "Randomise\nRoll a new patch, or randomise one part of it." };
    IconButton settingsButton { "settings", IlanaIcons::Icon::Gear, "Settings\nVoice mode, voices and pitch-bend range, skin, interface size, keyboard and the welcome tour." };
    juce::TextButton keysButton { "KEYS" };
    juce::TextButton helpButton { "?" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<WavetableEditor> wavetableEditor;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<PresetPanel> presetPanel;
    ModHoverPopup modHoverPopup { processorRef };
    // The header's live waveform strip (click: the scope).
    OutputView headerScope { processorRef };
    // VOICES in the status line: a click opens the voice settings.
    struct ClickArea : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<void()> onClick;
        void mouseUp (const juce::MouseEvent& event) override
        {
            if (onClick != nullptr && getLocalBounds().contains (event.getPosition()))
                onClick();
        }
    };
    ClickArea voicesArea;
    // TUNING in the status line while a Scala scale or MTS-ESP plays (a
    // click opens the tuning menu).
    ClickArea tuningArea;
    juce::String tuningText;
    void updateTuningIndicator();
    // After "Save and load": the load waiting for SAVE AS to finish.
    std::function<void()> afterSave;
    bool presetDocked = false;     // the user's choice: dock rather than drop down
    bool presetDockShown = false;  // the docked browser is open

    std::vector<std::unique_ptr<ModSourceChip>> chips;
    // Every LFO and envelope has a chip, shown while that module is in the
    // pool (or the matrix uses it). Parallel to chips; kind -1 for the
    // performance sources, which always show.
    std::vector<std::pair<int, int>> chipReveal;
    std::vector<bool> chipWanted;
    // "+": a picker for the LFOs and envelopes not in the pool yet.
    juce::TextButton moreChipsButton;
    void updateChipVisibility();
    void layoutChips (juce::Rectangle<int> row);
    void showChipPicker();

public:
    // Adds chip chipIndex's LFO or envelope to the pool (the "+" picker).
    void addPoolSource (int chipIndex);

private:
    std::unique_ptr<KeyboardStrip> keyboard;
    std::vector<std::unique_ptr<StripKnob>> macroKnobs;
    juce::TextButton macroPageButton; // shows macros 1-4 or 5-8 in the strip
    int macroPage = 0;
    void showMacroPage (int page);
    // (Voices, pitch-bend range and voice mode live in the settings menu.)
    std::unique_ptr<StripKnob> glideKnob, masterKnob;
    std::unique_ptr<OutputMeter> outputMeter;
    // Where the header's action groups (file, edit, tools) part, in header x.
    std::array<int, 2> headerSeparatorX {};
    std::unique_ptr<ToggleControl> legatoToggle;
    bool keyboardVisible = false;
    juce::int64 loadedFingerprint = 0;
    void rememberLoadedFingerprint();
    void adoptLoadedFingerprint();

    juce::ValueTree slotA, slotB;
    bool showingA = true;
    int currentPresetIndex = -1;
    juce::String shownPresetName;
    int themeIndex = 0;
    juce::String shownCategory, presetDisplayName;

    // Display-rate animation: the page transition and the preset flash.
    struct Animator : IlanaAnim::FrameTimer
    {
        std::function<void()> onFrame;
        void timerCallback() override { onFrame(); }
    };
    Animator animator;
    std::unique_ptr<IlanaAnim::FrameClock::Source> frameSource;
    void animate();

    juce::Component* transitionPage = nullptr;
    double transitionStart = 0.0;
    float presetLoadFlash = 0.0f;
    bool displayScaleApplied = false;
    void* previousDpiContext = nullptr;
    float uiZoom = 1.0f;
    bool zoomNeedsSaving = false;

   #if ILANA_GPU_UI
    std::unique_ptr<juce::OpenGLContext> openGL;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IlanaSynthAudioProcessorEditor)
};
