#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "gui/IlanaLookAndFeel.h"
#include "gui/InfoStrip.h"
#include "gui/KeyboardStrip.h"
#include "gui/LogoComponent.h"
#include "gui/ModSourceChip.h"
#include "gui/PresetPanel.h"
#include "gui/TutorialOverlay.h"

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
    void loadPreset();
    void togglePresetPanel();
    void randomize();
    void loadPresetIndex (int index);
    void updateHeaderButtons();
    void updateUndoButtons();
    void updateSeqTab();
    void showHistoryMenu();
    bool isFavourite (const juce::String& presetName) const;
    void toggleFavourite();
    void toggleAB();
    void cycleTheme();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    void startTabTransition();
    void applyDisplayScale();
    void applyUiZoom (float newZoom);
    float displayScale() const;
    float hostScaleFactor() const;

    static constexpr int designWidth = 1060;
    static constexpr int designHeight = 720;
    static constexpr const char* appVersion = "0.9";

    IlanaSynthAudioProcessor& processorRef;
    IlanaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 900 };
    Content content;
    LogoComponent logo;
    InfoStrip infoStrip;
    TutorialOverlay tutorial;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::TextButton presetButton { "PRESETS" };
    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton initButton { "INIT" };
    juce::TextButton favButton { "FAV" };
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton historyButton { "HIST" };
    juce::TextButton abButton { "A" };
    juce::TextButton themeButton { "SKIN" };
    juce::TextButton zoomButton { "UI 100%" };
    juce::TextButton diceButton { "DICE" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton loadButton { "LOAD" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<PresetPanel> presetPanel;
    std::unique_ptr<juce::Component> seqPage;
    bool seqTabVisible = true;

    std::vector<std::unique_ptr<ModSourceChip>> chips;
    std::unique_ptr<KeyboardStrip> keyboard;
    std::unique_ptr<juce::Component> macro1Knob, macro2Knob, macro3Knob, macro4Knob, masterKnob;
    std::unique_ptr<juce::Component> glideKnob, bendKnob;

    juce::ValueTree slotA, slotB;
    bool showingA = true;
    int currentPresetIndex = -1;
    juce::String shownPresetName;
    int themeIndex = 0;

    juce::Component* transitionPage = nullptr;
    double transitionStart = 0.0;
    float presetLoadFlash = 0.0f;
    bool displayScaleApplied = false;
    void* previousDpiContext = nullptr;
    float uiZoom = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IlanaSynthAudioProcessorEditor)
};
