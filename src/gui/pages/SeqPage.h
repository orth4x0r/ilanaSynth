// The SEQ tab (arp, Euclid, prob seq, clip, generate). The class is declared
// here; its member functions are in SeqPage.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../ClipState.h"
#include "../../PluginProcessor.h"
#include "../CardTabs.h"
#include "../ClipEditor.h"
#include "../GenerativeWidgets.h"
#include "../IlanaLookAndFeel.h"
#include "../ParamControls.h"
#include "../SequencerEditors.h"

// EXPORT MIDI drags the clip out as a .mid file (into a Live track, as
// Serum 2's clips do) and saves one on a click; a .mid dropped on the page
// imports into the chosen clip (ilana 2026-10-28).
class ClipDragButton : public juce::TextButton
{
public:
    // Writes the file to drag (empty: nothing to drag).
    std::function<juce::File()> makeFile;
    bool justDragged = false; // a drag, not a click: onClick skips it

    void mouseDrag (const juce::MouseEvent& event) override;

    void mouseDown (const juce::MouseEvent& event) override;

    void mouseEnter (const juce::MouseEvent& event) override;

private:
    bool dragging = false;
};

class SeqPage : public juce::Component,
                public juce::FileDragAndDropTarget,
                private juce::Timer
{
public:
    // The page is shown in two places: its step LFOs under MOD,
    // its note generators (arp, Euclid, prob seq, generate) as SEQ.
    enum class Part { modulators, notes };

    SeqPage (IlanaSynthAudioProcessor& p, Part partIn);

    // Not modulation sources. Each pattern engine its own fixed colour (its lead lane's: UI review 31, item 40);
    // the card's own chrome and the clip roll keep the accent.
    static juce::Colour arpColour() { return SeqColours::velocity(); }
    static juce::Colour generateColour() { return IlanaTheme::accent(); }
    static juce::Colour euclidColour() { return SeqColours::euclid(); }
    static juce::Colour pseqColour() { return SeqColours::chance(); }
    static juce::Colour clipColour() { return IlanaTheme::accent(); }

    void paint (juce::Graphics& g) override;

    // GENERATE in one line, for its folded title.
    juce::String generateSummary() const;

    void mouseUp (const juce::MouseEvent& event) override;

    void resized() override;

    void layoutNotes (juce::Rectangle<int> right);

    // GENERATE: three boxes side by side, SNAP TO KEY (ROOT, KEY, SNAP
    // PLAYED), STRUM (DIRECTION, TIME) and SPRAY (DIRECTION and five knobs),
    // each with its switch on its own title line and its controls inside.
    // Every name in the row on one line; menus get a little more width.
    void layoutGenerate();

    void visibilityChanged() override;

private:
    void showLfo (int row, int lfo);

    bool playsSteps (int lfo) const;

    // Sets the row's LFO to the Steps shape.
    void useSteps (int row);

    // Each row's Use button shows while its LFO plays another shape, and
    // the steps step back until they play.
    void updateUseButtons();

public:
    // The UI test reaches the rows through these.
    juce::TextButton& getUseButton (int row) { return useButtons[(size_t) juce::jlimit (0, 1, row)]; }

private:
    // Arp controls step back while the arp is off.
    void timerCallback() override;

public:

private:
    float readValue (const char* id) const;

    bool readOn (const char* id) const { return readValue (id) > 0.5f; }

    void showEngineTab();

    // The clip roll's EXPAND: GENERATE folds to its title line (a view
    // setting, not saved).
    void setClipExpanded (bool expanded);

    void updateEngineSwitches();

    // A tab's switch: the engine's on parameter, one undo step.
    void toggleEngine (int engine);

public:

private:
    bool generateFolded() const { return part == Part::notes && clipExpanded && engineTabs.getSelected() == 3; }

    std::vector<juce::Component*> generateControls();

    // Reads a .mid file into the chosen clip (IMPORT MIDI).
    void importMidiFile();

public:
    // Reads `file` into the chosen clip, one undo step (IMPORT MIDI, or a
    // .mid dropped on the page).
    bool importMidi (const juce::File& file);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;

    void filesDropped (const juce::StringArray& files, int, int) override;

    // The chosen clip as a .mid in a temporary folder, named after the
    // patch, for a drag out (File() when the clip has no notes).
    juce::File writeClipForDrag();

private:
    // Writes the chosen clip to a .mid file (EXPORT MIDI).
    void exportMidiFile();

    // PRINT TO CLIP: what the generators play for the held (or last) chord, into the chosen clip.
    void printToClip();
    static juce::String printTooltip (bool hasChord);

    Part part;
    StepEditor step1, step2;
    IlanaSynthAudioProcessor& processorRef;
    ArpLanesEditor arpLanes;
    static constexpr int engineMenuDrop = 7;
    ComboControl arpMode, arpDiv;
    KnobControl arpOctaves, arpGate, arpChance;
    ComboControl genScale, genRoot;
    ToggleControl genSnap, sprayOn;
    ComboControl sprayDirection, sprayStrum;
    std::unique_ptr<KnobControl> sprayCount, sprayRange, spraySpread, sprayChance, sprayVelocity, strumTime;
    CardTabs engineTabs;
    EuclidDisplay euclidDisplay;
    ComboControl eucTarget, eucDiv;
    KnobControl eucSteps, eucHits, eucRotate, eucGate;
    ProbSeqEditor pseqEditor;
    ComboControl pseqDiv;
    KnobControl pseqLength, pseqGate;
    ClipEditor clipEditor;
    ComboControl clipIndex, clipMode;
    ClipBarsControl clipBars;
    ClipGridControl clipGrid;
    juce::TextButton clipImport;
    ClipDragButton clipExport;
    juce::TextButton clipPrint;
    KnobControl arpSteps;
    NoteChainView noteChain;
    ChoiceSwitch scaleSwitch, strumSwitch;
    ClipZoomControl clipZoom;
    juce::TextButton clipQuantise, clipExpand { "EXPAND" }, clipDraw { "DRAW" };
    bool clipExpanded = false;
    juce::Rectangle<int> clipDivider, clipDivider2;
    int boxSignature = -1;
    static constexpr int boxHeaderHeight = 24, seqKnobWidth = 88, seqMenuWidth = 168;
    juce::Rectangle<int> snapBox, strumBox, sprayBox, engineBox;
    std::unique_ptr<juce::FileChooser> clipChooser;
    std::array<std::array<juce::TextButton, IlanaSynthAudioProcessor::numLfos>, 2> lfoButtons;
    std::array<juce::TextButton, 2> useButtons;
    juce::Rectangle<int> stepTitle1, stepTitle2, arpCard, generateCard;
    EffectRules effectRules { processorRef };
};
