// The PLAY tab. The classes are declared here; their member functions are
// in PlayPage.cpp.
#pragma once

#include <complex>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../../SectionColours.h"
#include "../AnimationUtils.h"
#include "../DisplayStyle.h"
#include "../EnvelopeDisplay.h"
#include "../FilterDisplay.h"
#include "../FilterWidgets.h"
#include "../IlanaLookAndFeel.h"
#include "../LfoDisplay.h"
#include "../LfoShapeMenu.h"
#include "../ModNames.h"
#include "../ModuleMenu.h"
#include "../ModulePool.h"
#include "../ParamControls.h"
#include "../SectionColourPicker.h"
#include "EnvLfoPages.h"
#include "VectorPage.h"
#include "ModSourcePanel.h"
#include "OscPage.h"
#include "PageHelpers.h"

// A small round glyph button (the carousel's arrows and +).
class GlyphButton : public juce::Button
{
public:
    explicit GlyphButton (const juce::String& glyphIn);

    void paintButton (juce::Graphics& g, bool hovered, bool down) override;

private:
    juce::String glyph;
};

// Pills that pick a view inside a card (F1 / F2, STRINGS / KEYS, BODY, BOARD): no parameter, solid while chosen,
// as wide as their words.
class ViewPills : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    ViewPills (juce::StringArray labelsIn, juce::Colour colourIn, const juce::String& tip);

    std::function<void (int)> onPick;
    // A small dot on a pill that is not shown but is switched on (FILTER's WEST pill while WEST plays).
    std::function<bool (int)> isMarked;
    std::function<juce::Colour (int)> markColour;

    int getSelected() const { return selected; }

    // -1: none chosen (a lone tab whose card shows the other view).
    void setSelected (int index, bool notify);

    void setColour (juce::Colour newColour);

    static constexpr int pad = Space::packedPad, pillGap = Space::pillGap;

    static int wordWidth (const juce::String& text);

    int idealWidth() const;

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& event) override;
    int optionAt (juce::Point<int> point) const;

private:
    // Each pill as wide as its word; the leftover width shared out evenly.
    juce::Rectangle<int> cellBounds (int option) const;

    juce::StringArray labels;
    juce::Colour colour;
    int selected = 0;
};

// A small outlined header button that opens a pop-up editor (ENVELOPE and LFO's EDIT, the dock's STEREO):
// its word and an arrow pointing up and out.
class PopButton : public juce::Button
{
public:
    PopButton (const juce::String& text, juce::Colour colourIn);

    void setColour (juce::Colour newColour);

    // Filled in the colour (the dock's STEREO tag), not only outlined.
    void setTagStyle (bool tag) { tagStyle = tag; }

    int idealWidth() const { return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true)), getButtonText()) + 25; }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override;

private:
    juce::Colour colour;
    bool tagStyle = false;
};

// STRINGS' tuning button: a drawn eighth note (the six notes open over the page).
class NoteButton : public juce::Button
{
public:
    NoteButton() : juce::Button ("Tuning") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override;
};

// A carousel card for one kind of source (PLAY's ENVELOPE and LFO): the sources
// of that kind as pills (each one still drags onto a knob), the selected one's
// graph and main controls under them. The pills' "+" adds the next one to the
// patch, the arrows step through them.
class SourceCarouselCard : public juce::Component,
                           private juce::Timer
{
public:
    enum class Kind { envelope, lfo };

    SourceCarouselCard (IlanaSynthAudioProcessor& p, Kind kindIn);

    // The selected source's pop-up editor (EDIT, or a click on the title), by `anchor`.
    std::function<void (int source, juce::Component& anchor)> onEdit;

    static constexpr int headerHeight = Space::cardHeaderPacked;

    int getSelectedSource() const { return selected; }

    // Colour coding: the card, its tabs and controls in the source's new colour.
    void refreshColours();

    void select (int source);

    void paint (juce::Graphics& g) override;

    void resized() override;

    // A right-click on the title or a pill: Copy, Paste and Init that
    // envelope or LFO (ilana 2026-10-28); on the dot, its colour.
    void mouseDown (const juce::MouseEvent& event) override;

    juce::PopupMenu buildModuleMenu (int source);

    void showModuleMenu (int source);

    // A click on the dot, left of the pills, opens the pop-up editor too.
    void mouseUp (const juce::MouseEvent& event) override;
    juce::MouseCursor getMouseCursor() override;

    // The sources the card lists: the envelopes (or LFOs) in the patch.
    std::vector<int> wantedSources() const;

private:
    // The pill's name: "AMP ENV" is AMP, "LFO 2" is 2.
    juce::String compactName (int source) const;

    void step (int direction);

    bool anyToAdd() const;

    // "+" adds the next free one, selected.
    void showAddMenu();

    static juce::String destinationShortName (int destination);

    void refreshSources();

    // The selected source's graph and controls.
    void buildBody();

    void layoutBody();

    static constexpr int cell = 35, envPitch = 45, lfoPitch = 43, gapToKnobs = 5, rowHeight = 16;

    void timerCallback() override;

    static constexpr float dialSize = 28.0f;

    IlanaSynthAudioProcessor& processorRef;
    Kind kind;
    std::vector<int> sources;
    std::vector<std::unique_ptr<SourceTab>> tabs;
    GlyphButton previous { juce::String::fromUTF8 ("\xe2\x80\xb9") }, next { juce::String::fromUTF8 ("\xe2\x80\xba") }, add { "+" };
    PopButton edit { "EDIT", IlanaTheme::accent() };
    int selected = 0, firstShown = 0;
    juce::Rectangle<int> chipBounds;
    juce::String chipText;
    bool drivesNothing = false;
    std::unique_ptr<juce::Component> graph;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::vector<std::unique_ptr<PillToggle>> toggles;
    std::unique_ptr<ComboControl> shape;
    std::unique_ptr<LfoRateControl> rate;
};

// WEST COAST's live picture: the folder's transfer curve (what one input level comes out as),
// computed as WestCoast.h's Wavefolder does it, so FOLD, BIAS and STAGES redraw it.
class WestDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    IlanaAnim::MorphSource shapeMorph { *this }; // (its old shape fades out over a new one)

    explicit WestDisplay (IlanaSynthAudioProcessor& p);

    void paint (juce::Graphics& g) override;

private:
    float read (const char* id) const;

    void timerCallback() override;

    IlanaSynthAudioProcessor& processorRef;
    float lastSignature = -1.0e9f;
};

// PLAY (ilana, 2026-10-19, mockup "fit8"): the oscillators as the OSC page's own
// cards on top, the voice's modules in two bands under them: FILTER 1, FILTER 2
// and WEST COAST; then ENVELOPE, LFO, BODY and SUB + NOISE. One knob size and
// pitch in every card, the menus in the headers.
// A small live picture of a few parameters (BODY's resonance, SUB's wave, NOISE's colour): it
// repaints when one of them moves, in the same well and trace as the other pictures.
class ParamPicture : public juce::Component,
                     public juce::SettableTooltipClient,
                     private IlanaAnim::FrameTimer
{
public:
    IlanaAnim::MorphSource shapeMorph { *this }; // (its old shape fades out over a new one)

    ParamPicture (IlanaSynthAudioProcessor& p, std::vector<juce::String> idsIn, const juce::String& tip);
    ~ParamPicture() override { endDrag(); }

    // (UI review 31, 29) The picture is a control too, as the filter, envelope and LFO graphs beside it: drag across
    // for one of its card's two main knobs, up or down for the other (one drag, one undo step), Shift for fine;
    // a double-click resets both. A ring marks where the two stand while the mouse is over it.
    void setDragParameters (const juce::String& acrossId, const juce::String& upId);

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override { endDrag(); }

    void mouseDoubleClick (const juce::MouseEvent&) override;

    void paintOverChildren (juce::Graphics& g) override;

protected:
    float read (const char* id) const;

    IlanaSynthAudioProcessor& processorRef;

private:
    void beginDrag();

    void endDrag();

    void set (juce::RangedAudioParameter* parameter, float normalised);

    juce::RangedAudioParameter* acrossParameter = nullptr;
    juce::RangedAudioParameter* upParameter = nullptr;
    juce::Point<float> dragStart;
    bool dragging = false;

    void timerCallback() override;

    std::vector<juce::String> ids;
    double lastSignature = -1.0e9;
};

// BODY: where its modes sit and how long they ring, from TYPE, MATERIAL and SIZE (the engine's own tuning tables).
class BodyDisplay : public ParamPicture
{
public:
    explicit BodyDisplay (IlanaSynthAudioProcessor& p);

    void paint (juce::Graphics& g) override;
};

// SUB: one wave shape, two cycles at -1 octave and one at -2.
class SubWaveDisplay : public ParamPicture
{
public:
    explicit SubWaveDisplay (IlanaSynthAudioProcessor& p);

    void paint (juce::Graphics& g) override;
};

// NOISE: its spectrum, 20 Hz to 20 kHz across (ten octaves, log), drawn from the same filters the voice runs
// (NoiseColours.h's generators at 48 kHz) and the COLOUR knob's one-pole low-pass (Voice.cpp's heard-noise colour:
// a corner from 200 Hz to white, with its partial gain make-up). White is flat, pink falls about 3 dB an octave,
// brown about 6 (flat under its 40 Hz leak), blue rises 6 and violet 12 (first and second difference), grey is the
// ear's inverse curve. The shape always shows; LEVEL lifts the whole picture.
class NoiseColourDisplay : public ParamPicture
{
public:
    explicit NoiseColourDisplay (IlanaSynthAudioProcessor& p);

    // The filter's gain at `hz` for noise type `type` (white is 1), as the voice's generator makes it.
    double typeGainAt (int type, double hz) const;

    void paint (juce::Graphics& g) override;

private:
    static constexpr double sampleRateShown = 48000.0;
    NoiseColours::Generator generator;
};

// BOARD: the soundboard's modes as bars across the pitch range (70 Hz to 3.6 kHz), each as tall as it
// rings long. SIZE slides them (a bigger board is lower and rings longer), TONE's lid darkens the top,
// MODEL Dense packs them closer. Drawn from AcousticKeys.h's own tables.
class BoardResponseDisplay : public ParamPicture
{
public:
    explicit BoardResponseDisplay (IlanaSynthAudioProcessor& p);

    void paint (juce::Graphics& g) override;
};

// STRINGS' picture: the sympathetic strings, COUNT of them, each ringing as wide as AMOUNT and dying away along
// the string as fast as DECAY says. On the KEYS tab: the piano string's partials, STRETCH pulling them sharp of
// the harmonic marks (faint ticks).
class StringsPicture : public ParamPicture
{
public:
    explicit StringsPicture (IlanaSynthAudioProcessor& p);

    void setKeys (bool shouldShowKeys);

    void paint (juce::Graphics& g) override;

private:
    static constexpr const char* stringsTip = "The sympathetic strings: one line per string, as wide as they ring (AMOUNT), fading as fast as DECAY.\n"
                                              "Drag: across for DECAY, up or down for AMOUNT. Double-click resets them.";
    bool keys = false;
};

// BODY's picture: the body's resonances, its peaks tuned by OFFSET, as sharp as DECAY makes them and as loud as
// AMOUNT; faint while BODY is off.
class BodyPicture : public ParamPicture
{
public:
    explicit BodyPicture (IlanaSynthAudioProcessor& p);

    void paint (juce::Graphics& g) override;
};

// FILTER's BALANCE while the filters run in parallel: a slider in the header, F1 at the left, F2 at
// the right, the middle marked. Drag it; a double-click centres it.
class BalanceSlider : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    BalanceSlider (juce::AudioProcessorValueTreeState& state, const juce::String& id);
    ~BalanceSlider() override
    {
        if (dragging)
            attachment.endGesture();
    }

    void setColour (juce::Colour newColour);

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& event) override;

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragging)
            dragTo (event.position.x);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging)
        {
            attachment.endGesture();
            dragging = false;
        }
    }
    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            return;
        if (dragging)
        {
            attachment.endGesture();
            dragging = false;
        }
        attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
    }

private:
    static constexpr float labelWidth = 14.0f;

    juce::Rectangle<float> trackArea() const { return getLocalBounds().toFloat().reduced (labelWidth + 6.0f, 0.0f); }

    void dragTo (float x);

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    juce::Colour colour = IlanaTheme::Ui::text2;
    bool dragging = false;
};

// The OSC area's thin scroll bar: a track and a thumb that read on the dark page.
class OscScrollBar : public juce::ScrollBar
{
public:
    using juce::ScrollBar::ScrollBar;

    void paint (juce::Graphics& g) override;
};

// The dock's VOICES pop-up (ilana's mockup v3, 2026-10-25): the stereo settings that were the OSC tab's STEREO card,
// SPREAD, PHASE, DRIFT and LOOSE with LOOSE's "Sharp and flat" switch. It floats over the page above the dock;
// the cross, Escape or a click elsewhere closes it.
class StereoPopover : public juce::Component
{
public:
    explicit StereoPopover (IlanaSynthAudioProcessor& p);

    std::function<void()> onClose;
    std::function<bool (juce::Component*)> isOwnButton; // (the dock's button toggles it itself)

    ~StereoPopover() override { juce::Desktop::getInstance().removeGlobalMouseListener (this); }

    void visibilityChanged() override;

    // A click anywhere else closes it.
    void mouseDown (const juce::MouseEvent& event) override;

    static juce::Colour colour() { return juce::Colour (0xff3fd0e6); }
    // (Packed as the other pop-ups, UI review 26: four 48 px cells, PLAY's knob pitch, not spread over 262 px.)
    static constexpr int cell = 48, width = 4 * cell + 2 * 12 + 6, height = 98, headerHeight = Space::cardHeaderOsc;
    static constexpr float dialSize = 28.0f;

    void paint (juce::Graphics& g) override;

    void resized() override;

    bool keyPressed (const juce::KeyPress& key) override;

private:
    std::vector<std::unique_ptr<KnobControl>> knobs;
    ToggleControl looseBoth;
    GlyphButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
};

// The fold control of PLAY's module block (SUB, NOISE, FILTER, ENVELOPE, LFO, STRINGS, BODY). Open: a small chevron
// tab in the middle of the edge between the oscillators and the block, half over each, a few px low (ilana
// 2026-11-05, mock C2), with no row of its own, so no oscillator card changes size (her 2026-10-21 rule). Folded:
// one card-like line at the page's foot naming what is inside; a click opens the block again. The block slides.
class BandsFoldRail : public juce::Button
{
public:
    explicit BandsFoldRail (bool isBar);

    void setFolded (bool shouldBeFolded);

    static constexpr int tabWidth = 30, tabHeight = 14;

    static juce::String foldedText();

    void paintButton (juce::Graphics& g, bool hovered, bool down) override;

private:
    bool folded = false;
};

class PlayPage : public juce::Component,
                 private juce::ScrollBar::Listener,
                 private IlanaTheme::IdentityColourListener,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater,
                 private juce::Timer
{
public:
    explicit PlayPage (IlanaSynthAudioProcessor& p);

    ~PlayPage() override;

    // A right-click on a card's dot picks that section's colour (colour
    // coding); elsewhere on the FILTER header, the filter's menu.
    void mouseDown (const juce::MouseEvent& event) override;

    // Colour coding: the cards whose section was given another colour.
    void identityColoursChanged() override;

    std::function<void (int)> onEditOperator;
    OscPageViewport& getOscView() { return oscView; }
    std::function<void (const juce::String&)> onOpenPage;
    // EDIT on ENVELOPE or LFO: the source's floating editor, by the button.
    std::function<void (int source, juce::Component& anchor)> onEditSource;

    // A right-click on the FILTER card's header (or its F1 / F2 pills): Copy,
    // Paste and Init that filter (ilana 2026-10-28).
    void showFilterMenuFor (const juce::MouseEvent& event);

    juce::PopupMenu buildFilterMenu (int filter);

    void paint (juce::Graphics& g) override;

    // A group's name centred over its controls, a thin rule either side.
    static void paintGroupLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, bool lit);

    void resized() override;

    // The views the tests and the snapshots open.
    void showFilter (int index);

    void showStrings (int index);

    void showBody (int index);

    bool isTuningShown() const { return tuningPopover.isVisible(); }
    // Esc, from the editor: closes the tuning pop-over if it is open.
    bool closeTuning();

    // The fold's moving parts at the slide's place: the tab on the block's top edge, the folded line, and while
    // sliding the block's picture.
    void placeFoldParts();

    // The block as it stands open, pictured once when a slide starts (its live views wait the 0.3 s out).
    void pictureFoldBlock();

    void setBandsFolded (bool shouldFold, bool notify = false);
    std::function<void (bool)> onBandsFoldChanged;

    // (The anchor for a pop-up opened from elsewhere, as OP PITCH from FM.)
    juce::Component& getEnvelopeCard() { return envelopeCard; }

private:
    struct FilterCard
    {
        std::unique_ptr<ComboControl> type;
        std::unique_ptr<PillChoice> slope;
        std::vector<std::unique_ptr<KnobControl>> knobs; // CUTOFF, RESO, DRIVE, KEY TRACK, MORPH
    };

    static constexpr int sideMargin = 7, bottomMargin = 4, hGap = 5, vGap = 5, headerHeight = Space::cardHeaderPacked, cell = 35;
    static constexpr int minBand = 62, rowHeight = 119, foldedRailHeight = 22, tabDrop = 4;
    static constexpr float dialSize = 28.0f;

    static juce::Colour filterColour (int index) { return IlanaTheme::envIdentityColour (index == 0 ? 1 : 2); }
    static juce::Colour westColour() { return FilterColours::west(); }
    bool westShown() const { return filterTabs.getSelected() == 2; }
    juce::Colour filterCardColour() const { return westShown() ? westColour() : filterColour (filterTabs.getSelected()); }
    static juce::Colour stringsColour() { return IlanaTheme::physicalColour(); }
    static juce::Colour bodyColour() { return IlanaTheme::physicalColour(); }
    static juce::Colour subColour() { return IlanaTheme::subNoiseColour(); }

    void timerCallback() override;

    bool operatorFilterOff = false;
    int filterSignature = -1, stringsSignature = -1;

    // The shown filter's type, slope and knobs; MORPH only on a type that morphs, BALANCE only in parallel.
    void refreshFilterParts();

    // As the FILTER tab's switch: off parks Filter 2's CUTOFF at the top, on brings it back.
    void toggleFilter2();

    float rememberedCutoff = 8000.0f;
    int lastSwitches = -1;

    // The OSC area's own scroll bar (thin, over the cards' side margin), shown only while the cards overflow it.
    void syncOscBar();

    // Where the cards run on past the oscillator area (a Physical card is taller, so a second card can end under
    // the module block), the edge fades into the page, so a row cut there reads as "scroll for more", not as a
    // clipped label (UI review 29, B3). Cards keep their size; nothing folds.
    void paintOverChildren (juce::Graphics& g) override;

    bool oscFadeAbove = false, oscFadeBelow = false;

    void scrollBarMoved (juce::ScrollBar*, double newStart) override;

    int readInt (const juce::String& id) const;

    int switchesSignature() const;

    // The cards' header dots (review 26 flair): a dot fades over 180 ms as
    // its card is switched, glows once as it comes on, and while notes
    // sound its halo breathes with the synth's output level.
    struct HeaderDot
    {
        float lit = -1.0f, glow = 0.0f, breath = 0.0f;
        bool target = false;
    };
    std::array<HeaderDot, 6> dots;

    struct DotClock : IlanaAnim::FrameTimer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } dotClock;

    bool dotTarget (size_t index) const;

    juce::Rectangle<int> dotArea (size_t index) const;

    // A card's switch moved (from any thread): its dot fades at once, not on the next page tick.
    static constexpr const char* dotSwitchIds[] { "sym_on", "west_on", "sb_on", "res_on", "subosc_on", "noise_on" };
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { wakeDots(); }

    // Starts the dots' clock when a switch moved or notes sound (the 4 Hz page timer asks).
    void wakeDots();
    bool noiseAudible() const;
    void updateNoiseSilent();

    void tickDots();

    void paintCardWithHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, const HeaderDot& dot) const;

    static int titleWidth (const juce::String& title);

    // Knobs in one row at `pitch`, starting at `x` (or centred in `area` when x < 0).
    static void placeKnobRow (juce::Rectangle<int> area, const std::vector<KnobControl*>& row, int pitch = cell, int extraBelow = 0, int x = -1);

    static std::vector<KnobControl*> pointers (const std::vector<std::unique_ptr<KnobControl>>& knobs);

    // The switch at the right of a header, as the OSC cards': 30 x 17.
    static juce::Rectangle<int> switchIn (juce::Rectangle<int> card);

    // A packed menu's width for its name and its longest value (the look-and-feel's text sizes and insets).
    static int packedMenuWidth (const juce::String& prefix, const juce::StringArray& values);

    // ilana's layout (arranger, 2026-10-29): three columns on two rows. SUB over NOISE; FILTER (F1 | F2 | WEST)
    // over ENVELOPE and LFO; STRINGS · KEYS over BODY · BOARD. Design widths 124 / 600 / 310 of 1034.
    void layoutBands (juce::Rectangle<int> bands);

    void layoutFilterCard();

    // STRINGS and KEYS (ilana, 2026-10-29: no empty space): the picture at the left, the knobs at the right.
    // STRINGS (UI review 31, 20): TUNING's AUTO / MANUAL and NOTES in the header, AMOUNT COUNT DECAY one row of bigger
    // dials in the body's full height. KEYS: STRETCH and RING, then KEY DAMPER PEDAL under one NOISE name.
    // (Both centred between the picture and the border, UI review 31, 8.)
    void layoutStringsCard();

    // The right-hand column of STRINGS and BOARD (one width, so the two pictures line up): three dials of
    // bigDial at bigPitch, centred with 7 px either side for the modulation rings.
    static constexpr int bigDial = 36, bigPitch = 42, rightColumnWidth = 2 * bigPitch + bigDial + 2 * 7; // off-grid

    void placeBigRow (std::vector<std::unique_ptr<KnobControl>>& knobs, int fromX, int toX, juce::Rectangle<int> body);

    // The x of the first of `count` knob cells at `pitch`, centred between two edges.
    static int centredColumnsX (int fromX, int toX, int count, int pitch);

    // BODY · BOARD (UI review 31, 20): the view's menus in the header, right of the two switches. BODY: TYPE and
    // COUPLING, its response across the card over the seven knobs in one centred row. BOARD: MODEL, its response
    // (as wide as the STRINGS picture above) beside SIZE TONE MIX in one row.
    void layoutBodyCard();

    int couplingMenuWidth() const;

    void layoutSubNoise();

    // The strings' six notes, over the oscillator cards right above the STRINGS card.
    juce::Rectangle<int> tuningBounds() const;

    void showTuning (bool show);

    // PLAY's TYPE list, grouped as the FILTER page's grid: its pages, then
    // its families, under the tiles' names.
    void showFilterTypeMenu (juce::ComboBox& combo, const juce::String& id, int filterIndex);

    // As on the FILTER page: only the classic models have a slope.
    bool filterHasSlope (int filterIndex) const;

    // (UI review 26: FILTER back to mockup v4's width, its graph about 290 px; the knobs at 33 and WEST at 41 give the room.)
    static constexpr int westPitch = 42, stringsPitch = 43, bodyPitch = 42, filterPitch = 50, gapToKnobs = 5;

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    OscPageViewport oscView;
    OscScrollBar oscBar { true };
    SourceCarouselCard envelopeCard, lfoCard;
    FilterDisplay filterDisplay;
    ViewPills filterTabs { { "F1", "F2", "WEST" }, filterColour (0), "F1 and F2: which filter's type and knobs show (the graph draws both). WEST: the WEST COAST folder and gate" };
    std::array<FilterCard, 2> filters;
    std::unique_ptr<PillChoice> routing;
    std::unique_ptr<BalanceSlider> balance;
    juce::TextButton f2Switch;
    ViewPills stringsTabs { { "STRINGS", "KEYS" }, stringsColour(), "STRINGS: the sympathetic strings. KEYS: the piano's stretch, pedal and key noises." };
    std::unique_ptr<ToggleControl> stringsOn;
    std::unique_ptr<PillChoice> stringsManual;
    NoteButton tuneButton;
    TuningPopover tuningPopover;
    IlanaAnim::PopGrow tuningGrow { tuningPopover }; // (NOTES grows from its button)
    std::vector<std::unique_ptr<KnobControl>> stringsKnobs, keysKnobs, tuningNotes;
    juce::Rectangle<int> filterBounds, stringsBounds, westBounds, bodyBounds, subBounds, noiseBounds, tuningGroup, noiseGroup;
    ViewPills bodyTab { { "BODY" }, bodyColour(), "Show BODY's controls (its switch is beside it)" };
    ViewPills boardTab { { "BOARD" }, bodyColour(), "Show the SOUNDBOARD's controls (its switch is beside it)" };
    BandsFoldRail foldTab { false }, foldBar { true };
    float foldShown = 0.0f; // 0 open, 1 folded: where the slide is
    int foldOpenTop = 0, foldFoldedTop = 0;
    struct FoldSlide : IlanaAnim::FrameTimer // (in step with the display's frames)
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } foldSlide;
    struct FoldPicture : juce::Component
    {
        FoldPicture() { setOpaque (true); setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { g.drawImage (image, getLocalBounds().toFloat()); }
        juce::Image image;
        int height = 0;
    } foldPicture;
    bool bandsFolded = false;
    std::unique_ptr<ToggleControl> westOn, bodyOn, boardOn, subOn, noiseOn;
    std::unique_ptr<WestDisplay> westDisplay;
    std::unique_ptr<SubWaveDisplay> subWave;
    std::unique_ptr<NoiseColourDisplay> noiseBand;
    std::unique_ptr<BoardResponseDisplay> boardResponse;
    std::unique_ptr<StringsPicture> stringsPicture;
    std::unique_ptr<BodyPicture> bodyPicture;
    std::unique_ptr<ComboControl> westMode, westPosition, westSource, bodyType, bodyCoupling, boardModel, subShape, noiseType;
    std::unique_ptr<PillChoice> subOctave;
    std::vector<std::unique_ptr<KnobControl>> westKnobs, bodyKnobs, boardKnobs;
    std::unique_ptr<KnobControl> subLevel, subColourKnob, noiseLevel, noiseColour;
};
