// The FX tab (the rack). The class is declared here; its member functions
// are in FxPage.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../AnimationUtils.h"
#include "../EqCurve.h"
#include "../FilterWidgets.h"
#include "../FxDisplays.h"
#include "../FxInfoText.h"
#include "../FxInstanceControls.h"
#include "../FxLibrary.h"
#include "../IlanaLookAndFeel.h"
#include "../MacroStrip.h"
#include "../MatrixWidgets.h"
#include "../OperatorEnvDisplay.h"
#include "../OutputMeter.h"
#include "../ParamControls.h"
#include "FxWidgets.h"
#include "PageHelpers.h"

class FxPage : public juce::Component,
               private IlanaAnim::FrameTimer
{
public:
    // KEEP FX (the CHAIN menu): while on, loading a preset leaves the rack as it is (the editor
    // puts the chain back after the load). Editor-side, never saved.
    static bool& keepRackOnPresetLoad() { static bool on = false; return on; }

    explicit FxPage (IlanaSynthAudioProcessor& p);
    ~FxPage() override
    {
        if (openLibraryBox != nullptr)
        {
            openLibraryBox->setLookAndFeel (nullptr);
            openLibraryBox->dismiss();
        }
        if (auto* parent = pickerDimmer.getParentComponent())
            parent->removeChildComponent (&pickerDimmer);
    }

    void paint (juce::Graphics& g) override;

    void paintOutput (juce::Graphics& g);

    // Cards cut by the foot or the head of a scrolling rack fade out there,
    // so they read as more beyond rather than clipped hard by OUTPUT or
    // under the CHAIN row (UI review 8, S8-14, V8-19; 9, V9-23).
    void paintOverChildren (juce::Graphics& g) override;

    void resized() override;

    void updateAddButton();

    // Choosing PATCHER routing opens the PATCH tab (the editor sets it).
    std::function<void()> onShowPatcher;

    // A shared copy's MAKE OWN (older patches), and the patcher's EDIT
    // SETTINGS and double-click on a node: the slot's card.
    void editInstance (int slot);

    void showSlot (int slot);

    // The library as a call-out by another page's button (the patcher's + ADD).
    void openLibrary (juce::Component& target) { showLibrary (target, target.getLocalBounds()); }

    // The PATCH tab's mini windows build their own copies of a module's
    // controls from these: the ones its card lays out (without its on
    // switch), and the name its slot MIX goes by (empty: the card shows none).
    std::vector<juce::Component*> windowControls (int type) const { return rowItems (type); }
    juce::String windowMixName (int slot, int type) const;

    // A control that does nothing in its effect's present settings, drawn
    // at IlanaTheme::dimmedAlpha on its RACK card and in its PATCH window
    // alike (`state`: the front settings, or a copy's own).
    static bool isIdleControl (int type, const juce::String& parameterId, const juce::AudioProcessorValueTreeState& state);

    static juce::String parameterIdOf (const juce::Component* item);

    // The module's own on parameter, where it has one.
    static const char* enableParamFor (int type);

private:
    int getSlotType (int slot) const;

    int getSlotBand (int slot) const;

    // An effect's name, from the library's table (the card title and the
    // menus use it; the parameter's choice strings stay as saved).
    juce::String getSlotName (int type) const;

    // The slot already holding this type, or -1. A module type has one set
    // of settings, so the rack takes each type once.
    int slotHoldingType (int type, int except = -1) const;

    bool stackPanelsWouldBeEmpty() const;

    // The loaded slot before / after this one in the chain, or -1.
    int neighbourSlot (int slot, int direction) const;

    void select (int slot);

    // Where a new effect goes in the chain: after the card picked last (a
    // click on it, or the effect added before), else at the end (UI review 31).
    int insertAfterSlot() const { return cardChosen && getSlotType (selectedSlot) != 0 ? selectedSlot : -1; }

    // Moves the effect just added at `added` to right after `after` in the
    // chain, the ones between shifting over (inside the caller's edit); the
    // slot it ends in.
    int placeAfter (int added, int after);

    // A library pick: a new effect goes into the first empty slot, or into
    // replacingSlot in place of its effect (a card's type button); one the
    // rack already holds is shown instead.
    void pickFromLibrary (int type, int replacingSlot = -1);

    void removeSlot (int slot);

    // The editor's scaled content (the page's ancestor under the editor), so
    // the call-out is drawn at the UI zoom like the page around it; it was
    // added to the unscaled window and came out at half size, its text about
    // 7 px, on a zoomed editor (ilana's PC test).
    juce::Component* calloutHost();

    // The library in a call-out over the page: adding (slot -1) or swapping a
    // slot's effect (its card's type button).
    void showLibrary (juce::Component& target, juce::Rectangle<int> area, int slot = -1);

    void showFileMenu();

    void showTypeMenu (int slot, juce::Component* target = nullptr);

    // A card's right-click menu, and what a pick in it does (the UI test runs
    // both through the page's "fxCardMenu" and "fxCardAction" properties).
    juce::PopupMenu buildTypeMenu (int slot);

    void typeMenuResult (int slot, int result);

    // A slot is its module plus its bypass, solo, blend and band; they move
    // together so a moved slot keeps how it was set up.
    void swapSlots (int a, int b) { processorRef.swapFxSlots (a, b); }

    // Moves a slot to another's place, the slots between shifting over by
    // one (as one undo step).
    void moveSlot (int from, int to);

    // The slots between shift over by one (inside the caller's edit).
    void shiftSlot (int from, int to);

    // A new instance of the slot's effect with a copy of its settings, right
    // after it (as PATCH's DUPLICATE), as one undo step.
    void duplicateSlot (int slot);

    // One effect's settings: the parameters while its card is in front, else
    // its instance's own copy.
    std::vector<std::pair<juce::String, float>> settingsOf (int slot) const;

    void applySettings (int slot, const juce::String& name, const std::vector<std::pair<juce::String, float>>& values);

    void copySettings (int slot);

    void pasteSettings (int slot);

    void resetSettings (int slot);

    // The settings Copy settings took: one effect type's parameter values.
    struct SettingsClip
    {
        int type = 0;
        std::vector<std::pair<juce::String, float>> values;
    };
    SettingsClip settingsClip;

    void saveChain();

    void updateKeepMark();

    void loadChain();

    // The routing switch: a click is one named undo step; the parameter
    // (a click, undo, a patch or chain load) sets the switch and the rail.
    void setRouting (bool parallel);

    // PATCHER: the cables on the PATCH tab decide the routing, and the
    // tab opens on them (one choice: the routing and where it is edited).
    void setPatcher();

    void routingChanged (bool parallel, bool force = false);

    juce::String routingHintText() const;

    void chainABChanged();

    void updateVisibility();

    bool tapsEnabled() const { return processorRef.apvts.getRawParameterValue ("fx_taps_on")->load() > 0.5f; }

    // One row per loaded slot, in chain order (the design's signal rail:
    // the rail at the left, then a headless row: number, name, solo, power
    // and engine; the effect's live picture; its knobs). A module type
    // loaded twice shares its settings, so later copies say so and offer
    // REMOVE.
    struct StackPanel
    {
        int slot = 0, type = 0;
        bool duplicate = false;
        bool instance = false;       // an instance whose card isn't in front: its own controls (instanceCards)
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> well;   // the picture slot of a row with no display: a sentence on what the effect does
        juce::Rectangle<int> handle; // the left block: where a drag reorders the chain
        juce::Rectangle<int> number; // the slot's number badge
        juce::Rectangle<int> meters; // a tall card's IN and OUT levels, under its left block
    };

    // A slot with its own settings whose card isn't the one in front.
    bool isOwnCard (int slot) const { return ! processorRef.isFxSlotInFront (slot); }

    void syncInstanceCards (const std::vector<StackPanel>& cards);

    void buildInstanceCard (int slot, int type, const FxPatchData::Patch& patch);

    // Banded slots next to each other, drawn as one bracketed group: a
    // LOW / MID / HIGH split with its crossovers, or a MID / SIDE one.
    struct SplitGroup
    {
        juce::Rectangle<int> bounds;
        bool frequency = true;
    };

    // Row geometry (px of the 1060 x 720 design): the rail is 56 wide, a row
    // is 80 high whatever the rack holds (ilana 2026-11-17: one size, a rack of
    // five's), its left block 156 wide; 32 px dials on a 56 px grid.
    // The parallel ladder's buses, x in the rail (inside the IN / OUT pills).
    static constexpr int parallelInBusX = 13, parallelOutBusX = 43;
    static constexpr int leftMetersHeight = 36, tallCardHeight = 280;
    static constexpr int railWidth = 56, railGap = 10, rowGap = 10, rowPad = 8, leftWidth = 156;
    static constexpr int rowHeightStandard = 80, duplicateHeight = 56;
    // The add row pinned at the page's foot: + ADD EFFECT and the suggested
    // next effects as chips beside it (UI review 31: one way to add, and the
    // rack takes the height the tiles and the OUTPUT strip had).
    static constexpr int addTileHeight = 36, suggestChipWidth = 122, suggestChipGap = 8, suggestCaptionWidth = 78, addTileMinWidth = 200;
    static constexpr int minDisplayWidth = 150, tapGridHeight = 56;
    static constexpr int splitHeaderHeight = 38, splitInsetLeft = 18, splitInsetRight = 6;
    static constexpr int toolbarHeight = 38, toolbarItemHeight = 24, stackTopMargin = 4, stackFootMargin = 14;
    // What the rack's content keeps under its last card: the level tag hangs 13 px below it (half a gap, 8 px of tag), so a
    // rack that scrolls ends with that tag whole, and one that fits by a pixel does not scroll for it (polish pass 1).
    static constexpr int stackTailMargin = 13;

    static int splitKind (int band) { return band >= 1 && band <= 3 ? 1 : (band >= 4 ? 2 : 0); }

    // PLAY's packed style (review 23): ring knobs with the value inside and the
    // name under, menus with their name inside the box, switches named at the right.
    // One dial size: 32 (PLAY's 28 dial's number in a ring 4 px wider, so a 3 or 4
    // character value keeps 3 px from it); a short rack's 38 went with its grown cards.
    struct DialGrid
    {
        float dial, value;
        int pitch, rowHeight;
    };
    static constexpr DialGrid compactGrid { 32.0f, 28.0f * KnobControl::kPackedValueScale, 56, 60 };
    static constexpr int knobPitch = 56, sideWidth = 104, sideRow = 18, sideGap = 5, sideColumnGap = 8;
    // (One grid on every long-rack card, counted from the right: a side column is two knob columns, so menus,
    // knobs and the pictures' right edges land on the same 56 px lines from card to card.)
    static_assert (sideWidth + sideColumnGap == 2 * knobPitch && compactGrid.pitch == knobPitch);
    const DialGrid& dialGrid() const { return compactGrid; }

    static void packControl (juce::Component* item, const DialGrid& grid = compactGrid);

    // A card's controls on PLAY's grid: menus, switches and buttons in side
    // columns at the left (three to a column on one row of knobs, five on two),
    // the knobs at 56 px in one row, or two when one would leave the picture
    // under its minimum width.
    struct PackedPlan
    {
        int knobRows = 1, knobColumns = 0, sideColumns = 0, sidePerColumn = 3, pitch = knobPitch;
        int width() const { return knobColumns * pitch + sideColumns * (sideWidth + sideColumnGap); }
    };

    static PackedPlan packedPlanFor (int knobs, int sides, int room, int pitch);

    // FILTER as PLAY's filter card: MORPH only on a model that morphs, SLOPE only on a classic one.
    int filterFxType() const { return juce::jlimit (0, FilterType::Count - 1, juce::roundToInt (processorRef.apvts.getRawParameterValue ("fx_filter_type")->load())); }
    bool filterFxMorphs() const { return FilterType::usesMorph (filterFxType()); }
    bool filterFxHasSlope() const;
    static constexpr int filterTypeBlockWidth = 4 * knobPitch - sideColumnGap;

    static bool isSideItem (const juce::Component* item) { return dynamic_cast<const KnobControl*> (item) == nullptr; }

    // The room a card's controls have beside its picture (the rack's width).
    int controlsRoom() const;

    PackedPlan packedPlanFor (const StackPanel& panel) const;

    // The controls a card lays out in its rows (its on switch goes in the
    // left block; an Airwindows knob the algorithm doesn't use is skipped).
    std::vector<juce::Component*> rowItems (int type, ToggleControl** power = nullptr) const;

    // The controls a card shows: its type's, or an instance's own.
    const std::vector<juce::Component*>& groupFor (const StackPanel& panel) const;

    std::vector<juce::Component*> rowItems (const std::vector<juce::Component*>& group, int type, ToggleControl** power) const;

    // A row's controls: its module's, then the slot's MIX where the
    // effect has none of its own (in the family's colour), or BLEND on an
    // old patch that set the slot's dry / wet beside the effect's MIX.
    std::vector<juce::Component*> cardItems (const StackPanel& panel, ToggleControl** power = nullptr) const;

    // Everything the knob grid holds: the controls, the band menu of a banded
    // slot (first) and the reverb's LOAD IR (last).
    std::vector<juce::Component*> gridItems (const StackPanel& panel, ToggleControl** power = nullptr);

    static bool hasCardDisplay (int type) { return type == 29 || type == 16 || FxDisplay::hasDisplay (type); }

    // Every row has a picture in its middle: the effect's display, or for one
    // without (FREEZE, the WIDENER, most Airwindows modules) a sentence on
    // what it does with its in and out levels beside it, so no row is half
    // blank (UI review 13, V13-1, I13-3).
    static bool hasCardPicture (int type) { return type > 0; }

    // The Airwindows algorithm a module type is running (its registry index), or -1.
    int airwindowsIndexFor (int type) const;

    juce::String cardInfoText (int type) const;

    // The band menu only where a slot works on a band (inside a split
    // group); the type menu sets it otherwise (S7-15).
    bool showsBand (int slot) const { return getSlotBand (slot) != 0; }

    // The slot's MIX knob only where the effect has no MIX of its own, or
    // once it is set below 100 % (V7-7, S7-15: one dry/wet per card); never
    // on a duplicate card, which does nothing of its own (V8-38).
    bool hasOwnMix (int type) const { return juce::isPositiveAndBelow (type, (int) ownMix.size()) && ownMix[(size_t) type]; }

    bool showsSlotMix (const StackPanel& panel) const;

    // An effect and its Airwindows model switch on the card (I7-28); the
    // card is titled by the effect ("REVERB", either model).
    static bool hasModelSwitch (int type) { return fxTwinOf (type) > 0; }
    juce::String cardTitle (int type) const;

    // A row's height: 80 for one line of knobs, taller for more (the grid is
    // 44 per line), plus the delay's tap grid while TAPS is on.
    int naturalHeight (const StackPanel& panel) const;

    // The delay's tap grid edits the type's parameters: on the card in front.
    bool hasTapGrid (const StackPanel& panel) const { return panel.type == 9 && ! panel.instance && tapsEnabled(); }

    void layoutStack();

    // The card's left block: its number and name on one line, SOLO, the
    // power switch and the engine tag under them.
    struct LeftBlock { juce::Rectangle<int> number, name, solo, power, chip; };

    static LeftBlock leftBlockFor (juce::Rectangle<int> card);

    void placeCard (StackPanel panel, juce::Rectangle<int> bounds);

    int firstEmptySlot() const;

    void paintSplitGroup (juce::Graphics& g, const SplitGroup& group);

    static juce::Colour railGood() { return IlanaTheme::oscColour (2); }

    // The first and the last loaded effects' colours (the rail's ends).
    juce::Colour firstRailColour() const;
    juce::Colour lastRailColour() const;

    // A node on the rail: IN or OUT, a pill in the signal's green.
    static void paintRailPill (juce::Graphics& g, juce::Rectangle<int> column, const juce::String& text);

    // The rail's line in a slot's colour: solid while the slot works, dashed
    // and faint while it is bypassed or off.
    static void paintRailLine (juce::Graphics& g, float x, float from, float to, juce::Colour colour, bool dashed);

    static void paintRailArrow (juce::Graphics& g, float x, float tipY, juce::Colour colour, bool dim);

    // The signal rail down the left of the rows: one node per effect in its
    // colour, the gain change of each slot in a chip between it and the next,
    // and a dashed node and BYPASS for a slot that is off.
    void paintRail (juce::Graphics& g);

    // PATCHER: the cables decide what feeds what, so the rail draws no chain:
    // each effect's node alone, on a faint dashed line.
    void paintPatcherRail (juce::Graphics& g);

    // A slot's node on the rail: filled in its colour, hollow while it is off.
    static void paintRailNode (juce::Graphics& g, float x, float centre, juce::Colour colour, bool off);

    // PARALLEL: a ladder. The input's bus runs down the left from IN, the
    // sum's down the right to OUT; each effect's node sits on a rung between
    // them (dashed while it is off), so every effect hears the input and
    // every output joins the sum. The chips read each branch's own gain.
    void paintParallelRail (juce::Graphics& g);

    // A rung of the parallel ladder, left to right, its arrow at the right end.
    static void paintRailRung (juce::Graphics& g, float from, float to, float y, juce::Colour colour, bool dim);

    // The chip in the gap under a row: what the slot did to the level.
    void paintRailChip (juce::Graphics& g, const StackPanel& panel, float x);

    // The slot's gain change as the chip reads it: "+2.4 dB", or a dash while
    // nothing passes through (nothing to compare).
    juce::String railText (int slot) const;

    // The level leaving the slot, 0 to 1 over -48 to 0 dB, for the chip's bar.
    float railFill (int slot) const;

    void paintStack (juce::Graphics& g);

    // The slot's IN and OUT levels, two bars one over the other.
    void paintLevelMeters (juce::Graphics& g, const StackPanel& panel, juce::Rectangle<int> meters, bool off);

    void paintInfoWell (juce::Graphics& g, const StackPanel& panel, bool off);

    // While a card is dragged by its header: where it will land, and its
    // ghost under the pointer.
    void paintDragGhost (juce::Graphics& g);

    // The card under a point, else the nearest one.
    int slotAt (juce::Point<int> position) const;

    void stackMouseDown (const juce::MouseEvent& event);

    void stackMouseDrag (const juce::MouseEvent& event);

    void stackMouseUp (const juce::MouseEvent&);

    // Esc during a reorder drag drops the card back where it was: no swap.
    // The drag state is gone, so the rest of the gesture (drag, mouse-up) does nothing.
    // The rack has the keys after a click on a card (not while a knob or a
    // menu in it is focused, where Delete is no order to remove the card).
    bool rackHasKeys() const;

    bool stackKeyPressed (const juce::KeyPress& key);

    void stackMouseMove (const juce::MouseEvent& event);

    void scrollToSlot (int slot);

    void visibilityChanged() override;

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    TapGrid tapGrid;
    std::unique_ptr<GateGrid> gateGrid;
    std::unique_ptr<ToggleControl> softClip;
    std::unique_ptr<StripKnob> clipGain;
    std::unique_ptr<OutputMeter> outputMeter;
    juce::Rectangle<int> emptyHeading, inColumn, outColumn, chainSeg, routingSeg;
    juce::Rectangle<int> outputSoftLabel, outputSeparator;
    juce::StringArray slotNames;
    std::vector<std::vector<juce::Component*>> slotGroups;

    juce::Viewport stackView;
    FxStackContent stackContent;
    std::array<std::unique_ptr<SlotSwitch>, IlanaSynthAudioProcessor::numFxSlots> slotSwitches;
    std::unique_ptr<FxLibraryView> library;
    FxPickerLook pickerLook;
    FxPickerDimmer pickerDimmer;
    juce::Component::SafePointer<juce::CallOutBox> openLibraryBox;
    // (At most five split groups: each needs a plain card or the end after it.)
    std::array<std::unique_ptr<CrossoverStrip>, 5> crossoverStrips;

    // Per slot: the card header's type menu, blend, solo, band and (on a
    // duplicate) REMOVE, and the card's display.
    struct CardHeader
    {
        FxTypeButton type;
        juce::TextButton solo { "S" };
        juce::ComboBox band;
        BlendSlider blend;
        std::unique_ptr<KnobControl> mix;
        juce::TextButton remove { "REMOVE" };
        juce::TextButton edit { "EDIT" };
        FxModelSwitch model;
        FxEngineChip chip;
        std::unique_ptr<juce::ButtonParameterAttachment> soloAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> bandAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> blendAttachment;
    };
    // Declared BEFORE displays so C++ destroys displays (which may hold raw
    // ShadowParameter* gestures during a Filter FX drag) before instanceCards
    // frees each card's Shadow.
    using InstanceCard = FxInstance::Card;
    std::array<InstanceCard, IlanaSynthAudioProcessor::numFxSlots> instanceCards;

    std::array<CardHeader, IlanaSynthAudioProcessor::numFxSlots> cardHeaders;
    std::array<std::unique_ptr<FxDisplay>, IlanaSynthAudioProcessor::numFxSlots> displays;

    // Takes a slot's instance card down: its picture stops reading the shadow, then the card goes (controls first).
    void dropInstanceCard (int slot);

    // Off: the slot is bypassed, or the module's own switch is off.
    bool isModuleOff (int slot) const;

    // An effect that is off dims its controls (the arcs go grey).
    bool updateModuleDimming();

    std::vector<StackPanel> stackPanels;
    juce::Image removeImage;           // a removed card, fading out where it stood
    juce::Rectangle<int> removeBounds;
    float removeFade = 0.0f;
    std::vector<SplitGroup> splitGroups;
    int selectedSlot = 0;
    bool cardChosen = false; // (a card picked by a click, not the first one by default)
    int dragSlot = -1, dropTarget = -1;
    bool dragActive = false;
    juce::Image dragImage;
    juce::Point<int> dragPosition;
    juce::Point<int> dragGrabOffset;
    float dropFlash = 0.0f;
    int dropSlot = -1;
    float cpuTicks = 0.0f, railTicks = 0.0f;
    juce::String lastReadings;
    std::array<int, IlanaSynthAudioProcessor::numFxSlots> lastTypes {};
    juce::String lastSignature;
    // The toolbar: CHAIN 1 / 2, named apart from the header's A / B
    // compare; the dice as the header's, for the chain (V7-43).
    DiceFxButton diceButton;
    juce::TextButton fileButton { juce::String::fromUTF8 ("CHAIN \xe2\x96\xbe") };
    juce::TextButton loadIrButton { "LOAD IR" };
    juce::TextButton chainAButton { "CHAIN 1" };
    juce::TextButton chainBButton { "CHAIN 2" };
    juce::TextButton routingOnPatchButton { "ROUTING ON PATCH" };
    juce::TextButton seriesButton { "SERIES" }, parallelButton { "PARALLEL" }, patcherButton { "PATCHER" };
    std::unique_ptr<juce::ParameterAttachment> routingAttachment, patcherAttachment;
    bool parallelRouting = false, patcherOn = false;
    juce::TextButton copyChainButton { "COPY TO 2" };
    juce::TextButton addButton { "+ ADD EFFECT" };
    DashedAddButton addTile { "+ ADD EFFECT", "+ ADD EFFECT" };
    std::vector<std::unique_ptr<FxSuggestTile>> suggestTiles;
    juce::Rectangle<int> suggestCaption;
    std::array<bool, 64> ownMix {};
    int stackNaturalHeight = 0, stackRows = 0, rowsPlaced = 0, availableStackHeight = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    ComboControl ampMode;
    KnobControl ampDrive, ampBass, ampMid, ampTreble, ampLevel;
    ToggleControl driveOn;
    KnobControl driveAmount, driveMix, foldAmount;
    ComboControl driveType, foldType;
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
    ToggleControl reverbKeepDry;
    KnobControl flangerRate, flangerDepth, flangerFeedback, flangerMix;
    KnobControl dimRate, dimDepth, dimMix;
    ComboControl gateDiv, gatePattern;
    KnobControl gateSteps, gateSwing, gateLength, gateSmooth, gateMix;
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
    ComboControl vocSource;
    KnobControl vocBands, vocWidth, vocAttack, vocRelease, vocFormant, vocUnvoiced, vocRate, vocLevel, vocMix;
    ComboControl filterType, filterSlope;
    KnobControl filterCutoff, filterReso, filterDrive, filterMorph, filterSpread;
    // The FILTER card in front: the voice filter's own picker, the categories
    // over it and the slope as PLAY's stacked pills (an instance's card keeps
    // the TYPE and SLOPE menus, which its shadow settings can drive).
    FilterTypePicker filterPicker;
    FilterCategoryTabs filterCategories;
    PillChoice filterSlopePills;

    int airwindowsAlgorithm() const;

    // Airwindows' knob names are cut to fit its 8-character plugin labels
    // ("Pressre", "Feedbk"), and say in the plugin's words what the card's
    // own modules call AMOUNT, LOW CUT or OUTPUT: the card shows the whole
    // word, in the rack's vocabulary (UI review 9, I9-15 / S9-5).
    static juce::String airwindowsKnobLabel (const char* name);

    // The knobs whose plugin value is a plain gain (the code multiplies the
    // signal by it, 0 to 1): these read in dB. The rest have no unit in the
    // plugin and read 0 to 100 % of their range.
    static bool isAirwindowsGain (const airwindows::Info& info, int k);

    // The knobs whose plugin value maps to a real unit in the algorithm's own
    // code (read from the ports: the same formulas, at the host's rate); the
    // text says it in ms, Hz or dB instead of a bare percentage (I14-2). Empty
    // for the rest.
    std::function<juce::String (double)> airwindowsUnitText (const airwindows::Info& info, int k) const;

    // A gain knob's text in dB; the others keep the parameter's own %.
    void setAirwindowsUnit (KnobControl& knob, const airwindows::Info& info, int k);

    std::map<juce::Slider*, std::function<juce::String (double)>> awPercentText;

    // Airwindows' plugin names as words: "ToTape6" reads "To Tape 6",
    // "Console7Channel" "Console 7 Channel". Display only: the saved choice
    // strings stay the plugins' own (and show in the tooltip).
    static juce::String airwindowsDisplayName (const char* name);

    // The algorithm's own Dry/Wet stays where it is (as chosen, usually
    // 100 %): the module's MIX is the one wet/dry control on the card.
    static bool isAirwindowsDryWet (const char* name) { return juce::String (name) == "Dry/Wet"; }

    static juce::String airwindowsTooltip (const airwindows::Info& info);

    // An Airwindows knob's tooltip: the plugin's name for it, and why it
    // reads in % (the plugins give their controls no unit: UI review 8,
    // I8-34).
    juce::String airwindowsKnobTip (const airwindows::Info& info, int k) const;

    static void setKnobTip (KnobControl& knob, const juce::String& tip);

    // The chosen algorithm's knobs carry its own names; the rest hide.
    void updateAirwindowsKnobs (bool loaded);

    // The algorithms by family; picking one sets its knobs to the plugin's
    // own defaults.
    void showAirwindowsMenu();

    void chooseAirwindows (int algorithm);

    // ---- The Airwindows category modules (types 32-41) ----
    struct AwCategoryControls
    {
        std::unique_ptr<ComboControl> algo;
        std::array<std::unique_ptr<KnobControl>, airwindows::Module::numKnobs> knobs;
        std::unique_ptr<KnobControl> mix;
        std::unique_ptr<ToggleControl> sync; // AW Delay only
        std::unique_ptr<ComboControl> div;
    };
    std::vector<AwCategoryControls> awCategories;

    static bool isAirwindowsType (int type) { return type == 30 || airwindows::categoryForFxType (type) >= 0; }

    int categoryChoice (int c) const;

    void updateCategoryKnobs (int c, bool loaded);

    // The category's effects; picking one sets its knobs to the plugin's
    // own defaults.
    // FILTER's TYPE list, grouped as the FILTER page's grid.
    void showFilterTypeMenu();

    void showCategoryMenu (int c);

    void chooseCategoryEffect (int c, int choice);
};
