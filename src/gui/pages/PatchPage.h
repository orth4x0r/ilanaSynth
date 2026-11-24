// The PATCH tab: roadmap steps 16 and 17 as one view (ilana, 2026-10-11:
// "unify reroutable oscillators and patcher"). One canvas runs from the
// oscillators and SUB + NOISE through Filter 1, Filter 2 and WEST (per note,
// on a band of their own) to VOICES, where every note sums, then through the
// rack's effects to OUT. Under the hood it is still two patches, so each part
// costs what it did: the voice's cables (route_cables and cable_*,
// dsp/VoiceRouting.h) and the FX patch (FxPatch.h). The page merges them into
// one graph for the cable view (PatchCableView.h) and splits each edit back.
// The classes are declared here; their member functions are in PatchPage.cpp.
//
// No modes (ilana, 2026-10-12: "why chain or cables? let the user just pick
// that themselves, reorder everything freely"): the page has no CHAIN /
// CABLES or SERIES / PARALLEL / PATCHER switch. It always shows the patch
// as cables. A patch that still plays the fixed chain or the rack shows
// that routing as its cables (getChainAsCables, getFxRackGraph), so the
// sound stays exactly as it was; the first edit on a side writes those
// cables and switches that side to play them (one undo step puts it back).
// The old switch parameters stay for older patches; only the UI is gone.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../ClipState.h"
#include "../../PluginProcessor.h"
#include "../FilterWidgets.h"
#include "../FxCarousel.h"
#include "../FxDisplays.h"
#include "../FxInstanceControls.h"
#include "../IlanaLookAndFeel.h"
#include "../LfoShapeMenu.h"
#include "../ParamControls.h"
#include "../PatchCableView.h"
#include "FxPage.h"
#include "FxWidgets.h"
#include "PatchLfo.h"

// The selected loop cable's LEVEL and DELAY knobs (ilana 2026-10-11): the
// usual KnobControl on two parameters of their own, written into the cable.
class LoopKnobs : private juce::AudioProcessorParameter::Listener
{
public:
    explicit LoopKnobs (float shortestMs);

    ~LoopKnobs() override;

    // The cable's values onto the knobs (not heard back as an edit).
    void show (const PatchGraph::Cable& cable);

    std::function<void (float level, float delayMs)> onChange; // a turn (inside onGesture, or one step)
    std::function<void (bool)> onGesture;

    std::unique_ptr<KnobControl> level, delay;

private:
    void set (const char* id, float value);

    float get (const char* id) const;

    void parameterValueChanged (int, float) override;

    void parameterGestureChanged (int, bool starting) override;

    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    bool syncing = false;
};

// An effect's mini window on the PATCH tab (ilana, 2026-10-11: "open mini
// windowed versions of effects inside the patcher"): its picture and its
// card's controls, built here as copies on the same parameters (the RACK
// page keeps its own), so it can be edited without leaving the cables.
// Dragged by its title, closed with x; the page keeps it inside the canvas.
class FxMiniWindow : public juce::Component
{
public:
    FxMiniWindow (IlanaSynthAudioProcessor& p, FxPage& rack, int slotIndex, int fxType, const juce::String& title);

    ~FxMiniWindow() override;

    const int slot, type;
    std::function<void (FxMiniWindow&)> onClose, onMoved;

    // Whether its controls are on a copy's own settings (or on the front ones).
    bool isOwnSettings() const { return shadow != nullptr; }

    // The PATCH tab's LFO boxes: each knob's right-click menu gets `menu`'s
    // items (assign to an LFO cabled to this effect), and a knob an LFO moves
    // carries a dot in that LFO's colour (`marker`: transparent for none).
    // Knobs are named by their parameter ID, the slot's MIX by "mix".
    void setLfoHooks (std::function<void (juce::PopupMenu&, const juce::String&)> menu, std::function<juce::Colour (const juce::String&)> marker);

    void paintOverChildren (juce::Graphics& g) override;

    // Changes made elsewhere (its RACK card, undo) onto a copy's controls.
    void refresh();

    void paint (juce::Graphics& g) override;

    void resized() override;

    // Dragged by its title bar.
    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;

    // Keeps it inside this area of its parent (the canvas).
    void setLimits (juce::Rectangle<int> area);

private:
    static constexpr int pad = Space::windowPad, gap = Space::windowGap, titleHeight = Space::windowTitleHeight, displayHeight = 64, cellWidth = Space::windowCellWidth, cellHeight = Space::windowCellHeight;

    // Dimmed as its RACK card is: everything while the effect is off, and a
    // control that does nothing in the present settings (FxPage::isIdleControl).
    void updateDimming();

    IlanaSynthAudioProcessor& processorRef;

    struct Constrainer : juce::ComponentBoundsConstrainer
    {
        FxMiniWindow* owner = nullptr;
        void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>&, const juce::Rectangle<int>&, bool, bool, bool, bool) override
        {
            if (owner != nullptr && ! owner->limits.isEmpty())
                bounds = bounds.constrainedWithin (owner->limits);
        }
    };

    juce::String name;
    juce::Colour colour;
    SlotSwitch power;
    FxDisplay display;
    std::unique_ptr<FxInstance::Shadow> shadow;
    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::function<juce::Colour (const juce::String&)> lfoMarker;
    int columns = 1, rows = 1;
    juce::ComponentDragger dragger;
    Constrainer constrainer;
    juce::Rectangle<int> limits;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxMiniWindow)
};

class PatchPage : public juce::Component,
                  private juce::Timer
{
public:
    // The voice's boxes in the merged graph: id voiceId + kind, node kind
    // voiceKind + kind, where kind is 0-5 for OSC 1-6, 6 for SUB + NOISE and
    // stageKind + VoiceRouting::Stage for the stages. The effects keep their
    // FX patch ids and types; the FX patch's INPUT is VOICES.
    static constexpr PatchGraph::NodeId voiceId = 1u << 20;
    static constexpr int voiceKind = 1000;
    static constexpr int stageKind = 10;
    // The LFO boxes (FxLfo.h, PatchLfo.h): id lfoNodeId + the LFO's number.
    static constexpr PatchGraph::NodeId lfoNodeId = 1u << 21;
    static constexpr int lfoKind = 2000;

    PatchPage (IlanaSynthAudioProcessor& p, FxPage& rackPage);

    ~PatchPage() override;

    // Set by the editor: show a page ("OSC", "FILTER"), or an effect's card
    // on the RACK page.
    std::function<void (const juce::String&)> onShowPage;
    std::function<void (int)> onOpenSlot;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;

    void resized() override;

    PatchCableView& getView() { return view; }

    static bool isVoice (const PatchGraph::Node& node) { return node.id >= voiceId && node.id < lfoNodeId; }
    static bool isLfo (const PatchGraph::Node& node) { return node.id > lfoNodeId && node.id <= lfoNodeId + (PatchGraph::NodeId) FxLfo::maxLfos; }
    static int lfoOf (const PatchGraph::Node& node) { return isLfo (node) ? (int) (node.id - lfoNodeId) : 0; }

    // The voice's patch a merged graph's cables make (a cable into VOICES
    // is VoiceRouting's OUT).
    static VoiceRouting::Patch voicePatchOf (const PatchGraph::Graph& graph);

private:
    static constexpr int toolbarHeight = 28;
    static constexpr int inspectorHeight = 70;
    static constexpr int idleInspectorHeight = 40;
    // (FIT opens at 100 % at the most: polish pass 4; it was 1.8 x.)
    static constexpr float minZoom = 0.5f, fitMaxZoom = 1.0f, fitPadX = 60.0f, fitPadY = 90.0f;
    // The 10 px grid: voice columns 160 apart (a 130 px box and 30 px for the
    // cables; R18-1: tighter than the rack's 180, so a whole patch fits the
    // canvas at full size), rows 70 apart (a 50 px box and a 20 px gap).
    // (Review 19, R19-1: with four boxes or fewer in the per-note column the
    // rows are 100 apart, so the fitted graph fills the canvas's height, not a
    // strip of it; five or six sources keep 70 so they still fit.)
    static constexpr float columnStep = 160.0f;
    static float rowStepFor (size_t sourceCount) { return sourceCount <= 4 ? 100.0f : 70.0f; }
    // (R20-1: and stretched, on the 10 px grid, up to 1.8 times, so the fitted graph fills the canvas's height.)
    static float stepOf (size_t sourceCount, float stretch) { return std::round (rowStepFor (sourceCount) * stretch / 10.0f) * 10.0f; }
    float rowStretch = 1.0f;
    bool stretching = false;
    size_t lastSourceCount = 0;
    static constexpr float rackStep = 180.0f; // PatchGraph::rackGraph's

    static int localKind (const PatchGraph::Node& node) { return node.kind - voiceKind; }

    int slotOf (const PatchGraph::Node* node) const;

    //==========================================================================
    // Names, colours and the boxes' second lines.
    static juce::Colour colourOf (const PatchGraph::Node& node);

    static juce::String voiceName (int kind);

    float read (const juce::String& id) const;

    static juce::String onParamOf (int kind);

    bool sourceOn (int kind) const;

    juce::String statusOf (const PatchGraph::Node& node) const;

    juce::String hintText() const;

    // "WEST > FILTER 1 > FILTER 2", the voice's stages in the order they run.
    juce::String voiceOrderText() const;

    juce::String fxSummaryText() const;

    // "Goes to FILTER 1 and VOICES", from the shown cables.
    juce::String destinationText (const PatchGraph::Node& node) const;

    //==========================================================================
    // The voice's band: behind every box that runs once per note, up to VOICES.
    void paintVoiceBand (juce::Graphics& g) const;

    void paintInspector (juce::Graphics& g);

    int splitSources() const;

    std::vector<juce::Component*> inspectorItems();

    // The selected box's row on the 10 px grid: name and detail, the
    // controls (one label line above one control line), the actions at the
    // right.
    // A control in the bottom strip's row (the row's full height): a knob is
    // the standard ring with its value inside and its name under it (UI
    // review 31, item 42: the strip was the last place with pointer knobs
    // and the value beside them); anything else keeps its 116 x 44 cell.
    static void placeStripControl (juce::Component& control, juce::Rectangle<int> cell);

    void layoutInspector();
    bool inspectorIsIdle() const;

    // The inspector follows the selected box.
    void inspect (const PatchGraph::Node* node);

    //==========================================================================
    // Edits.
    // Tighter margins and 5 % zoom steps than the default fit, so the graph keeps its size (R17-2).
    // (R18-1: up to 1.8 x, so a small patch fills the canvas with readable boxes.)
    // FIT and F glide there and give the view back to the page.
    void fit (bool animated = false);

    // A cable may not cross the line at VOICES: the per-note boxes feed only
    // each other and VOICES, the effects never feed back into a note.
    bool accept (const PatchGraph::Graph& graph);

    // The FX patch a merged graph makes: the effects' cables and places.
    PatchGraph::Graph fxGraphOf (const PatchGraph::Graph& graph) const;

    static bool sameCables (const PatchGraph::Graph& a, const PatchGraph::Graph& b);

    // A view edit: each side that changed goes to the processor (and plays
    // its cables, if it didn't: a changed cable means the cables now
    // decide). One undo step.
    void commit (PatchCableView::Edit edit);

    // The LFO cable the view has selected, or null.
    const PatchGraph::Cable* selectedModCable() const;

    // The loop cable the view has selected, or null.
    const PatchGraph::Cable* selectedLoop() const;

    float shortestLoopMs() const;

    void loopGesture (bool starting, const juce::String& name);

    // A turn of the selected loop's LEVEL or DELAY knob.
    void setLoop (float level, float delayMs);

    void resetVoiceToChain();

    // Right-click adds and duplicates put the new box where they were asked
    // for: that needs the cables, so the rack's routing becomes them first
    // (inside the caller's edit; the same sound).
    void playRackAsCables();

    void toggleBypass (int slot);

    void removeEffect (int slot);

    bool rackFull() const;

    void duplicateEffect (int slot);

    //==========================================================================
    // Copy and paste (ilana 2026-11-04): the picked effects with their settings and the cables between them; the
    // copies come in on their own, not wired to anything else (no route from the end of one chain into the next).
    struct Clip
    {
        struct Box { int slot = -1, type = 0; juce::Point<float> at; std::vector<std::pair<juce::String, float>> values; };
        struct Wire { int from = 0, fromPort = 0, to = 0, toPort = 0; float gain = 1.0f, delayMs = 0.0f; };
        std::vector<Box> boxes;
        std::vector<Wire> wires; // between the boxes (indices into boxes)
    };
    Clip clip;

    void copyEffects (const std::vector<PatchGraph::NodeId>& ids);

    int freeSlots() const;

    // at: a merged-graph position for the copies' top-left box, or NaN for beside the originals.
    void pasteEffects (juce::Point<float> at);

    // Delete with several boxes picked: one undo step.
    void removeMany (const std::vector<PatchGraph::NodeId>& ids);

    // The patcher's cables back to what they were (an effect just added comes in on its own, unwired).
    void restoreCables (const PatchGraph::Graph& before);

    void open (const PatchGraph::Node& node);

    //==========================================================================
    // Mini windows: at most four, one per box (a copy of an effect with its
    // own settings gets controls on those, not on the front ones).
    static constexpr size_t maxWindows = 4;

    void openWindow (int slot);

    // (Deferred: a window closes from its own button's click.)
    void closeWindow (FxMiniWindow* window);

    // A window whose slot changed effect or was emptied closes.
    void checkWindows();

    void openSelected();

    void addNodeItems (juce::PopupMenu& menu, const PatchGraph::Node& node);

    void addEffectItems (juce::PopupMenu& menu, juce::Point<float> at);

    // A new effect's box where the menu was opened (the processor wired it in
    // before OUT); at is a merged-graph position.
    void placeNodeNow (int slot, juce::Point<float> at);

    // A tile of the carousel: the effect added (before OUT; under the pointer
    // when it is dragged), and with a drag the new box is picked up on the
    // canvas. True when it was added.
    bool addFromCarousel (const FxCarousel::Tile& tile, std::optional<juce::Point<float>> at);

    //==========================================================================
    // The merged graph: the voice's boxes in columns at the left (sources,
    // then each stage one column right of the furthest stage feeding it),
    // VOICES after them, and the FX patch moved so its INPUT is VOICES.
    void refresh (bool force);

    void nameEffects (PatchGraph::Graph& fx) const;

    // The voice's boxes; returns where VOICES goes.
    juce::Point<float> addVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources);

    void connectVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources);

    //==========================================================================
    // LFO boxes (FxLfo.h, PatchLfo.h).

    // The knobs an LFO may move on a slot (ID or "mix", name), cached per refresh.
    const std::vector<std::pair<juce::String, juce::String>>& targetsOf (int slot);

    // The boxes and their cables into the merged graph (FX coordinates moved
    // as the effects are), and each cable's tag: the knobs it moves.
    void addLfoBoxes (PatchGraph::Graph& graph);

    juce::String modTagOf (const PatchGraph::Cable& cable) const;

    // An LFO box: its rate as the second line, its wave at the right.
    void decorateLfo (juce::Graphics& g, const PatchGraph::Node& node, juce::Rectangle<float> box) const;

    // A view edit that touched an LFO box or cable: made here (one undo step), true if so.
    bool commitLfos();

    // A new LFO box at a merged-graph position, or above the effects.
    int addLfo (std::optional<juce::Point<float>> at);

    void removeLfo (int id);

    void openLfoWindow (int id);

    void closeLfoWindow (FxLfoWindow* window);

    // A knob's right-click items in an effect's window: assign it to an LFO
    // cabled to this effect (or stop, or how far), or make an LFO for it.
    void addKnobLfoItems (juce::PopupMenu& menu, int slot, const juce::String& param);

    juce::Colour knobLfoColour (int slot, const juce::String& param) const;

    void timerCallback() override;

    void updateZoomLabel();

    IlanaSynthAudioProcessor& processorRef;
    FxPage& rack;
    PatchGraph::Graph shown, fxShown;
    PatchCableView view;
    FxCarousel carousel;
    const bool hasVoice;
    VoiceRouting::Table table;
    VoiceRouting::Patch voiceShown;
    juce::Point<float> fxShift;
    juce::Rectangle<float> voiceArea;

    juce::TextButton addButton { "+ ADD EFFECT" }, resetButton { "RESET" }, fitButton { "FIT" };
    juce::TextButton zoomOutButton { juce::String::fromUTF8 ("\xe2\x88\x92") }, zoomInButton { "+" }, zoomLabel { "100%" };
    juce::Rectangle<int> zoomWell, hint, inspector, inspectorText, onArea, powerArea, soloArea, bandArea;
    bool carouselDrag = false; // a carousel tile is being dragged onto the canvas: no refit under the pointer
    bool autoFit = true; // false once the user moves the view (FIT gives it back)

    int inspectedVoice = -1, inspectedSlot = -1;
    juce::TextButton onButton { "ON" };
    std::unique_ptr<juce::ButtonParameterAttachment> onAttachment;
    std::unique_ptr<SlotSwitch> power;
    std::unique_ptr<KnobControl> mixKnob;
    juce::TextButton soloButton { "S" };
    juce::ComboBox band;
    std::unique_ptr<juce::ButtonParameterAttachment> soloAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> bandAttachment;
    juce::TextButton editButton { "EDIT SETTINGS" }, duplicateButton { "DUPLICATE" }, removeButton { "REMOVE" };

    std::unique_ptr<LoopKnobs> loopKnobs;
    juce::TextButton removeCableButton { "REMOVE CABLE" };
    bool loopGestureOpen = false;

    std::vector<std::unique_ptr<FxMiniWindow>> windows;

    // LFO boxes: the patch as last shown, each cable's tag, the selected box's
    // row, a selected cable's amounts, the floating windows.
    FxPatchData::Patch lfoPatch;
    std::map<std::pair<int, int>, juce::String> modTags;
    std::map<int, std::vector<std::pair<juce::String, juce::String>>> lfoTargets;
    int inspectedLfo = 0;
    std::unique_ptr<FxLfoControls> lfoControls;
    std::unique_ptr<FxLfoDisplay> lfoDisplay;
    std::unique_ptr<FxLfoAmounts> lfoAmounts;
    juce::TextButton lfoButton { "+ LFO" }, openTargetButton { "OPEN ITS WINDOW" }, openLfoButton { "OPEN WINDOW" }, removeLfoButton { "REMOVE" };
    std::vector<std::unique_ptr<FxLfoWindow>> lfoWindows;
    juce::String lastSignature, lastState, refusal, lastReadouts;
    juce::uint32 refusedUntil = 0;
    int shownNodes = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchPage)
};
