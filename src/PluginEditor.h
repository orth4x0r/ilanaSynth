#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "gui/HeaderWidgets.h"
#include "gui/IlanaLookAndFeel.h"
#include "gui/InfoStrip.h"
#include "gui/KeyboardStrip.h"
#include "gui/LogoComponent.h"
#include "gui/MacroStrip.h"
#include "gui/ModSourceChip.h"
#include "gui/OutputMeter.h"
#include "gui/PresetPanel.h"
#include "gui/SectionPage.h"
#include "gui/TutorialOverlay.h"

class WavetableEditor;

class IlanaSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
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
    void savePreset();
    void exportPreset();
    void loadPreset();
    void togglePresetPanel();
    void showPresetMenu();
    void showDiceMenu();
    void showSettingsMenu();
    void randomize();
    void randomizeGroup (int group);
    void mutate (float amount);
    void loadPresetIndex (int index);
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
    juce::TextButton abButton { "A" };
    IconButton diceButton { "dice", IlanaIcons::Icon::Dice, "Randomise\nRoll a new patch, or randomise one part of it." };
    IconButton settingsButton { "settings", IlanaIcons::Icon::Gear, "Settings\nSkin, interface size, keyboard and the welcome tour." };
    juce::TextButton keysButton { "KEYS" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<WavetableEditor> wavetableEditor;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<PresetPanel> presetPanel;

    std::vector<std::unique_ptr<ModSourceChip>> chips;
    // LFO 5-16 and ENV 6-16: a chip each, shown while that module is added
    // (or the matrix uses it). Parallel to chips; kind -1 for fixed chips.
    std::vector<std::pair<int, int>> chipReveal;
    std::vector<bool> chipWanted;
    // "+N" when the added LFOs and envelopes don't all fit in the row.
    juce::TextButton moreChipsButton;
    void updateChipVisibility();
    void layoutChips (juce::Rectangle<int> row);
    std::unique_ptr<KeyboardStrip> keyboard;
    std::vector<std::unique_ptr<StripKnob>> macroKnobs;
    std::unique_ptr<StripKnob> glideKnob, bendKnob, masterKnob, voicesKnob;
    std::unique_ptr<OutputMeter> outputMeter;
    std::unique_ptr<ComboControl> voiceModeBox;
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
    juce::String shownCategory;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IlanaSynthAudioProcessorEditor)
};
