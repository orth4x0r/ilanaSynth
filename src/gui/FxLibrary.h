#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/airwindows/Categories.h"
#include "IlanaLookAndFeel.h"
#include "FxDisplays.h"
#include "HeaderWidgets.h"

// The FX page's shared pieces (UI review 6), outside the editor's anonymous
// namespace so the UI test can find them: the family colours, the library
// (its names, buttons and view), a card's type menu title, the slot blend
// bar and a split group's crossover strip.

// Each effect family has its own colour: drive/distortion warm, modulation
// blue-violet, time and space green-cyan, dynamics teal, filters/EQ pink.
inline juce::Colour fxColour (int type)
{
    // One colour per category, as the library groups them (it read as
    // confetti when colours followed the individual effects).
    switch (type)
    {
        // (The Airwindows category modules, 32-41, wear their function's.)
        case 13: case 9: case 15: case 11: case 12: case 8: case 22: case 34: case 35: case 41:
            return juce::Colour (0xff5cc4e8); // space: reverb, delay, dimension, smear, freeze, haas, widener, AW reverb/delay/stereo
        case 2: case 1: case 3: case 26: case 28: case 30: case 32: case 33: case 39: case 40:
            return juce::Colour (0xffff8a5c); // drive: drive, amp, crush, octaver, feedback, airwindows, AW tape/saturation/console/lo-fi
        case 7: case 6: case 14: case 23: case 24: case 25: case 27: case 5: case 31: case 36:
            return juce::Colour (0xff9a8cff); // motion: chorus .. comb, vocoder, AW modulation
        case 16: case 10: case 17:
            return juce::Colour (0xff7ad98e); // rhythm: trance gate, stutter, tape stop
        case 29: case 18: case 4: case 20: case 21: case 19: case 37: case 38:
            return juce::Colour (0xffe0c35c); // tone & level: eq, tilt, comp, ott, limiter, utility, AW dynamics/EQ
        default:
            return IlanaTheme::Ui::text3;
    }
}

// An FX card's title as its type menu: the tag, the name and a chevron;
// a click opens the list of effects to swap this slot's module for.
class FxTypeButton : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    FxTypeButton()
    {
        setTooltip ("Change this slot's effect");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;

    void setTitle (const juce::String& newTitle, juce::Colour newColour)
    {
        if (newTitle == title && newColour == colour)
            return;
        title = newTitle;
        colour = newColour;
        repaint();
    }

    // Wide enough for the tag, the name and the chevron.
    int preferredWidth() const { return IlanaTheme::cardTitleWidth (title) + 4; }

    void paint (juce::Graphics& g) override
    {
        if (isMouseOver())
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedLeft (-4.0f), 5.0f);
        }

        IlanaTheme::paintCardTitle (g, getLocalBounds(), title, colour);

        // The chevron after the name.
        const auto x = (float) IlanaTheme::cardTitleWidth (title) - 12.0f;
        const auto y = (float) getHeight() * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (x, y - 2.0f);
        chevron.lineTo (x + 4.0f, y + 2.0f);
        chevron.lineTo (x + 8.0f, y - 2.0f);
        g.setColour (IlanaTheme::Ui::text2.withAlpha (isMouseOver() ? 1.0f : 0.7f));
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr && getLocalBounds().contains (event.getPosition()))
            onClick();
    }

private:
    juce::String title;
    juce::Colour colour;
};

// ---- The effect library (UI review 6) ----
// Every type once, under what it does, named by its job (S5-19, S6-27,
// I6-26). UI review 7 (I7-28, S7-36): an Airwindows module that does a
// family effect's job is that effect's Airwindows model, not a shelf of its
// own: the library offers it as an AW button beside the effect, and the card
// switches between the two models in place. Airwindows modules with no twin
// (Tape, Console) are entries with an AW tag; the all-in-one module is the
// last row, "More Airwindows". Display only: the saved choice strings and
// indices stay the same.
struct FxLibraryEntry
{
    int type;
    const char* name;
    int twin = -1;               // its Airwindows model's type, or -1
    const char* twinName = nullptr;
};

struct FxLibraryGroup
{
    const char* title;
    int column; // of five; -1 is the row under the columns
    std::vector<FxLibraryEntry> entries;
};

inline const std::vector<FxLibraryGroup>& fxLibraryGroups()
{
    static const std::vector<FxLibraryGroup> groups {
        { "SPACE", 0, { { 13, "Reverb", 34, "Spaces" }, { 9, "Delay", 35, "Echoes" }, { 15, "Dimension" }, { 11, "Smear" }, { 12, "Freeze" },
                        { 8, "Haas" }, { 22, "Widener", 41, "Stereo" } } },
        { "DRIVE", 1, { { 2, "Drive", 33, "Saturation" }, { 1, "Amp" }, { 3, "Crush", 40, "Lo-Fi" }, { 26, "Octaver" }, { 28, "Feedback" },
                        { 32, "Tape" }, { 39, "Console" } } },
        { "MOTION", 2, { { 7, "Chorus", 36, "Ensemble" }, { 6, "Phaser" }, { 14, "Flanger" }, { 23, "Tremolo" }, { 24, "Freq Shift" },
                         { 25, "Ring Mod" }, { 5, "Comb" } } },
        { "RHYTHM", 3, { { 16, "Trance Gate" }, { 10, "Stutter" }, { 17, "Tape Stop" } } },
        { "VOICE", 3, { { 27, "Vowel" }, { 31, "Vocoder" } } },
        { "TONE & LEVEL", 4, { { 29, "EQ", 38, "Tone" }, { 18, "Tilt" }, { 4, "Comp", 37, "Glue" }, { 20, "OTT" }, { 21, "Limiter" },
                               { 19, "Utility" } } },
        { "MORE AIRWINDOWS", -1, { { 30, "Airwindows (all)" } } } };
    return groups;
}

// An effect's name, as the library, the card titles and the menus show it.
inline juce::String fxTypeName (int type)
{
    for (const auto& group : fxLibraryGroups())
        for (const auto& entry : group.entries)
        {
            if (entry.type == type)
                return entry.name;
            if (entry.twin == type)
                return entry.twinName;
        }
    return "-";
}

// Airwindows' effects: the all-in-one module and the ten category modules.
inline bool isAirwindowsFxType (int type) { return type == 30 || airwindows::categoryForFxType (type) >= 0; }

// The other model of the same effect (Reverb and its Airwindows model,
// Spaces, each way round), or -1.
inline int fxTwinOf (int type)
{
    for (const auto& group : fxLibraryGroups())
        for (const auto& entry : group.entries)
        {
            if (entry.twin >= 0 && entry.type == type)
                return entry.twin;
            if (entry.twin >= 0 && entry.twin == type)
                return entry.type;
        }
    return -1;
}

// What the AIRWINDOWS badge's tooltip says.
inline juce::String airwindowsBadgeTip (int type)
{
    juce::String text ("Airwindows: Chris Johnson's open-source effects, here as ");
    if (type == 30)
        return text + "one module with every algorithm, grouped by family.";
    const auto category = airwindows::categoryForFxType (type);
    if (category < 0)
        return text + "a module.";
    juce::StringArray names;
    for (const auto algorithm : airwindows::categoryModules()[(size_t) category].algorithms)
        names.add (airwindows::registry()[(size_t) algorithm].name);
    return text + "a choice of " + names.joinIntoString (", ") + ".";
}

// A library entry: the effect's name on its family's tint, and a dot in its
// colour once it is in the rack (V6-25: a dot, and a click on it shows its
// card, as the rack takes each effect once; V7-42: its name dims, so it
// doesn't read as a second one to add). An Airwindows model beside its
// effect is a narrow "AW" button; an Airwindows module without a twin
// carries an AW tag.
class FxLibraryButton : public juce::TextButton
{
public:
    enum class Kind { effect, airwindowsModel, airwindowsOnly, more };

    FxLibraryButton (int typeIn, const juce::String& name, Kind kindIn = Kind::effect)
        : juce::TextButton (kindIn == Kind::airwindowsModel ? juce::String ("AW") : name.toUpperCase()), type (typeIn), kind (kindIn) {}

    int getType() const { return type; }
    Kind getKind() const { return kind; }
    int getInRackSlot() const { return inRackSlot; }

    void setInRackSlot (int slot)
    {
        if (slot == inRackSlot)
            return;
        inRackSlot = slot;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto colour = fxColour (type);
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto inRack = inRackSlot >= 0;
        auto fill = IlanaTheme::Ui::raised.interpolatedWith (colour, highlighted ? 0.16f : (kind == Kind::airwindowsModel ? 0.04f : 0.08f));
        if (down)
            fill = fill.darker (0.2f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (highlighted ? colour.withAlpha (0.6f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
        const auto textColour = colour.interpolatedWith (juce::Colours::white, 0.35f).withAlpha (inRack ? 0.5f : 1.0f);

        if (kind == Kind::airwindowsModel)
        {
            if (inRack)
            {
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ bounds.getRight() - 6.0f, bounds.getY() + 6.0f }));
            }
            g.setColour (textColour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
            return;
        }

        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds.withWidth (3.0f).reduced (0.0f, 6.0f).translated (4.0f, 0.0f), 1.5f);

        auto text = getLocalBounds().reduced (12, 0);
        if (inRack)
        {
            const auto dot = text.removeFromRight (10).toFloat();
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot.getCentre()));
            text.removeFromRight (4);
        }
        if (kind == Kind::airwindowsOnly)
        {
            g.setColour (colour.withAlpha (0.6f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            // The word, not its initials (review 10, S10-11).
            g.drawText ("AIRWINDOWS", text.removeFromRight (64), juce::Justification::centredRight);
        }

        g.setColour (textColour);
        g.setFont (IlanaTheme::pillFont());
        IlanaTheme::drawFitted (g, getButtonText(), text, juce::Justification::centredLeft, 1);
    }

private:
    int type;
    Kind kind;
    int inRackSlot = -1;
};

// The whole library in five columns, each group under a small heading, its
// Airwindows models beside their effects, and "More Airwindows" under the
// columns. The page shows it while the rack is empty; + ADD EFFECT opens a
// copy in a call-out.
class FxLibraryView : public juce::Component
{
public:
    // slotOf: the slot holding a type, or -1. onPick: a type was clicked.
    FxLibraryView (std::function<int (int)> slotOfIn, std::function<void (int)> onPickIn)
        : slotOf (std::move (slotOfIn)), onPick (std::move (onPickIn))
    {
        const auto add = [this] (int type, const juce::String& name, FxLibraryButton::Kind kind)
        {
            auto button = std::make_unique<FxLibraryButton> (type, name, kind);
            button->onClick = [this, type]
            {
                if (onPick != nullptr)
                    onPick (type);
                if (afterPick != nullptr)
                    afterPick();
            };
            addAndMakeVisible (*button);
            buttons.push_back (std::move (button));
        };

        for (const auto& group : fxLibraryGroups())
            for (const auto& entry : group.entries)
            {
                if (group.column < 0)
                    add (entry.type, "More Airwindows: every algorithm in one module", FxLibraryButton::Kind::more);
                else
                    add (entry.type, entry.name, isAirwindowsFxType (entry.type) ? FxLibraryButton::Kind::airwindowsOnly : FxLibraryButton::Kind::effect);
                if (entry.twin >= 0)
                    add (entry.twin, entry.twinName, FxLibraryButton::Kind::airwindowsModel);
            }
        refresh();
    }

    // Called after a pick (the call-out closes itself with it).
    std::function<void()> afterPick;

    static constexpr int columns = 5, buttonHeight = 26, headingHeight = 18, groupGap = 10, twinWidth = 34, legendHeight = 14;

    // The tallest column's height, buttons and headings, and the row under them.
    static int preferredHeight()
    {
        std::array<int, columns> heights {};
        auto below = 0;
        for (const auto& group : fxLibraryGroups())
        {
            if (group.column < 0)
            {
                below += groupGap + (int) group.entries.size() * buttonHeight;
                continue;
            }
            auto& height = heights[(size_t) group.column];
            if (height > 0)
                height += groupGap;
            height += headingHeight + (int) group.entries.size() * buttonHeight;
        }
        return *std::max_element (heights.begin(), heights.end()) + below + groupGap + legendHeight;
    }

    // Marks what is in the rack, and says so in the tooltips.
    void refresh()
    {
        for (auto& button : buttons)
        {
            const auto type = button->getType();
            const auto slot = slotOf != nullptr ? slotOf (type) : -1;
            button->setInRackSlot (slot);
            const auto twin = fxTwinOf (type);
            const auto name = button->getKind() == FxLibraryButton::Kind::airwindowsModel
                                  ? fxTypeName (twin) + ", Airwindows model (" + fxTypeName (type) + ")"
                                  : fxTypeName (type);
            auto tip = slot >= 0 ? name + " is in slot " + juce::String (slot + 1)
                                       + ": click to show its card (the rack takes each effect once)."
                                 : "Add " + name + " to the first empty slot.";
            if (button->getKind() == FxLibraryButton::Kind::effect && twin >= 0)
                tip << "\nAW beside it is its Airwindows model; a card switches between the two.";
            if (isAirwindowsFxType (type))
                tip << "\n" << airwindowsBadgeTip (type);
            button->setTooltip (tip);
            // The all-in-one module only while a patch already uses it: the
            // family cards' AIRWINDOWS models are the one way in (I8-33).
            if (button->getKind() == FxLibraryButton::Kind::more)
                button->setVisible (slot >= 0);
        }
        repaint();
    }

    // The button that adds a type (an Airwindows model's AW button too).
    FxLibraryButton* findButton (int type) const
    {
        for (const auto& button : buttons)
            if (button->getType() == type)
                return button.get();
        return nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& heading : headings)
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (heading.text, heading.bounds.withTrimmedLeft (2), juce::Justification::bottomLeft);
        }

        // What the small AW boxes are (S10-11).
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText ("AW = the Airwindows version of the effect beside it; a card switches between the two.", legend, juce::Justification::centredLeft);

        // A hairline over the row under the columns.
        if (! moreRow.isEmpty() && std::any_of (buttons.begin(), buttons.end(), [] (const auto& b)
                                                { return b->getKind() == FxLibraryButton::Kind::more && b->isVisible(); }))
        {
            g.setColour (IlanaTheme::Ui::line);
            g.fillRect (moreRow.toFloat().withHeight (1.0f).translated (0.0f, -(float) groupGap * 0.5f));
        }
    }

    void resized() override
    {
        headings.clear();
        const auto gap = 10;
        const auto columnWidth = (getWidth() - gap * (columns - 1)) / columns;
        std::array<int, columns> y {};
        size_t index = 0;

        const auto placeEntry = [&] (const FxLibraryEntry& entry, int x, int top, int width)
        {
            const auto mainWidth = entry.twin >= 0 ? width - twinWidth - 3 : width;
            buttons[index++]->setBounds (x, top, mainWidth, buttonHeight - 3);
            if (entry.twin >= 0)
                buttons[index++]->setBounds (x + mainWidth + 3, top, twinWidth, buttonHeight - 3);
        };

        for (const auto& group : fxLibraryGroups())
        {
            if (group.column < 0)
                continue;
            const auto x = group.column * (columnWidth + gap);
            auto& top = y[(size_t) group.column];
            if (top > 0)
                top += groupGap;
            headings.push_back ({ { x, top, columnWidth, headingHeight - 4 }, group.title });
            top += headingHeight;

            for (const auto& entry : group.entries)
            {
                placeEntry (entry, x, top, columnWidth);
                top += buttonHeight;
            }
        }

        // The row under the columns, across them.
        auto bottom = *std::max_element (y.begin(), y.end()) + groupGap;
        legend = { 2, bottom - groupGap / 2, getWidth() - 4, legendHeight };
        bottom += legendHeight;
        moreRow = {};
        for (const auto& group : fxLibraryGroups())
            if (group.column < 0)
                for (const auto& entry : group.entries)
                {
                    moreRow = moreRow.isEmpty() ? juce::Rectangle<int> (0, bottom, getWidth(), buttonHeight - 3) : moreRow;
                    placeEntry (entry, 0, bottom, columnWidth * 2 + gap);
                    bottom += buttonHeight;
                }
    }

private:
    struct Heading
    {
        juce::Rectangle<int> bounds;
        juce::String text;
    };

    std::function<int (int)> slotOf;
    std::function<void (int)> onPick;
    std::vector<std::unique_ptr<FxLibraryButton>> buttons;
    std::vector<Heading> headings;
    juce::Rectangle<int> moreRow, legend;
};

// A card's model switch, for an effect with an Airwindows model (I7-28): two
// segments, the effect's own model and Airwindows'; a click on the other
// swaps the slot's effect in place (each keeps its own settings).
class FxModelSwitch : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    FxModelSwitch() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    std::function<void()> onSwitch;

    static constexpr int preferredWidth = 132;

    // airwindows: which model the card holds. otherFree: the other model
    // isn't in another slot (so the switch can take it).
    void setState (bool airwindows, bool otherFree, juce::Colour colourIn)
    {
        if (airwindows == isAirwindows && otherFree == canSwitch && colourIn == colour)
            return;
        isAirwindows = airwindows;
        canSwitch = otherFree;
        colour = colourIn;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto split = bounds.getX() + bounds.getWidth() * 0.42f;
        const auto radius = bounds.getHeight() * 0.5f;
        g.setColour (IlanaTheme::Ui::well);
        g.fillRoundedRectangle (bounds, radius);
        const auto lit = isAirwindows ? bounds.withLeft (split) : bounds.withRight (split);
        g.setColour (colour.withAlpha (0.22f));
        g.fillRoundedRectangle (lit, radius);
        g.setColour (isMouseOver() && canSwitch ? colour.withAlpha (0.7f) : IlanaTheme::Ui::line.brighter (0.15f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (isAirwindows ? IlanaTheme::Ui::text3.withAlpha (canSwitch ? 1.0f : 0.5f) : colour.interpolatedWith (juce::Colours::white, 0.4f));
        g.drawText ("BUILT-IN", bounds.withRight (split), juce::Justification::centred);
        g.setColour (isAirwindows ? colour.interpolatedWith (juce::Colours::white, 0.4f) : IlanaTheme::Ui::text3.withAlpha (canSwitch ? 1.0f : 0.5f));
        g.drawText ("AIRWINDOWS", bounds.withLeft (split), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto onAirwindowsSide = (float) event.x > (float) getWidth() * 0.42f;
        if (canSwitch && onAirwindowsSide != isAirwindows && getLocalBounds().contains (event.getPosition()) && onSwitch != nullptr)
            onSwitch();
    }

private:
    bool isAirwindows = false, canSwitch = true;
    juce::Colour colour { IlanaTheme::accent() };
};

// The FX chain's dice: the header's dice icon and "FX" (V7-43: "DICE FX"
// read as a verb and didn't look related to the header's dice).
class DiceFxButton : public juce::TextButton
{
public:
    DiceFxButton() : juce::TextButton ("FX") {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);
        auto area = getLocalBounds().toFloat();
        const auto content = area.withSizeKeepingCentre (38.0f, area.getHeight());
        const auto colour = highlighted ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2;
        g.setColour (colour);
        g.fillPath (IlanaIcons::make (IlanaIcons::Icon::Dice, content.withWidth (18.0f).withSizeKeepingCentre (18.0f, 18.0f)));
        g.setFont (IlanaTheme::pillFont());
        g.drawText (getButtonText(), content.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    }
};

// A slot's blend in its card header: a thin slider that reads "BLEND 100%"
// (I7-38: drawn as a slider, a fill and a handle, not a pill). Shown on an
// effect without a MIX of its own, or once it is set below 100 % (V7-7,
// S7-15: one dry/wet per card).
class BlendSlider : public juce::Slider
{
public:
    BlendSlider() : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        setTooltip ("Slot blend: this slot's output against the signal that came in (0 % dry only, 100 % the slot as set). "
                    "The effect's own MIX works inside it. Double-click for 100 %.");
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        setSliderSnapsToMousePosition (false);
        setDoubleClickReturnValue (true, 1.0);
    }

    void setTint (juce::Colour newColour)
    {
        if (newColour != colour)
        {
            colour = newColour;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto amount = (float) juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
        const auto hot = isMouseOverOrDragging();

        // The value as text, then a thin track with its fill and handle.
        auto text = bounds;
        const auto track = text.removeFromBottom (4.0f).reduced (2.0f, 0.0f);
        g.setColour (IlanaTheme::Ui::text.withAlpha (isEnabled() ? (hot ? 1.0f : 0.8f) : 0.5f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("BLEND " + juce::String (juce::roundToInt (amount * 100.0f)) + "%", text, juce::Justification::centred);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (track, 1.5f);
        const auto filled = track.withWidth (track.getWidth() * amount);
        g.setColour (colour.withAlpha (isEnabled() ? 0.85f : 0.4f));
        g.fillRoundedRectangle (filled, 1.5f);
        g.fillEllipse (juce::Rectangle<float> (hot ? 8.0f : 6.0f, hot ? 8.0f : 6.0f).withCentre ({ filled.getRight(), track.getCentreY() }));
    }

private:
    juce::Colour colour { IlanaTheme::accent() };
};

// A splitter group's two crossovers on a frequency axis (I6-27, S5-18):
// LOW | MID | HIGH, the bands the group's slots use lit, each divider
// dragged sideways (double-click resets it). The same two parameters as
// every banded slot uses (fx_split_low, fx_split_high).
class CrossoverStrip : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    explicit CrossoverStrip (IlanaSynthAudioProcessor& p)
        : low (p.apvts.getParameter ("fx_split_low")), high (p.apvts.getParameter ("fx_split_high"))
    {
        setTooltip ("The crossovers: drag a divider to move where the low band ends or the high band starts. "
                    "Every banded slot splits at these two. Double-click a divider to reset it.");
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        startTimerHz (15);
    }

    ~CrossoverStrip() override { endDrag(); }

    // The bands the group's slots work on (LOW, MID, HIGH), lit.
    void setBandsUsed (std::array<bool, 3> used)
    {
        if (used != bandsUsed)
        {
            bandsUsed = used;
            repaint();
        }
    }

    float frequencyOf (bool upper) const
    {
        auto* parameter = upper ? high : low;
        return parameter != nullptr ? parameter->convertFrom0to1 (parameter->getValue()) : (upper ? 2500.0f : 250.0f);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 5.0f);
        const auto lowX = xOf (frequencyOf (false)), highX = xOf (frequencyOf (true));
        const juce::Rectangle<float> regions[] { bounds.withRight (lowX), bounds.withLeft (lowX).withRight (highX), bounds.withLeft (highX) };
        static const char* const names[] { "LOW", "MID", "HIGH" };

        for (int b = 0; b < 3; ++b)
        {
            const auto region = regions[b].reduced (0.0f, 2.0f);
            if (bandsUsed[(size_t) b])
            {
                g.setColour (IlanaTheme::accent().withAlpha (0.16f));
                g.fillRect (region);
            }
            // A band no slot works on passes through dry, and says so (S7-15).
            g.setColour (bandsUsed[(size_t) b] ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (juce::String (names[b]) + (bandsUsed[(size_t) b] ? "" : ": DRY"), region.toNearestInt(), juce::Justification::centred, false);
        }

        for (const auto upper : { false, true })
        {
            const auto x = upper ? highX : lowX;
            const auto active = dragging == (upper ? 2 : 1) || hovered == (upper ? 2 : 1);
            g.setColour (active ? IlanaTheme::accent() : IlanaTheme::Ui::text2);
            g.fillRect (juce::Rectangle<float> (active ? 2.0f : 1.5f, bounds.getHeight() - 4.0f).withCentre ({ x, bounds.getCentreY() }));
            g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, 10.0f).withCentre ({ x, bounds.getCentreY() }), 2.0f);

            // The frequency, on the side with room.
            const auto text = hzText (frequencyOf (upper));
            const auto width = 56.0f;
            const auto label = upper ? juce::Rectangle<float> (x + 7.0f, bounds.getY(), width, bounds.getHeight())
                                     : juce::Rectangle<float> (x - 7.0f - width, bounds.getY(), width, bounds.getHeight());
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true));
            g.drawText (text, label.toNearestInt(), upper ? juce::Justification::centredLeft : juce::Justification::centredRight, false);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto nearest = nearestDivider ((float) event.x);
        if (nearest != hovered)
        {
            hovered = nearest;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovered = 0;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragging = nearestDivider ((float) event.x);
        if (auto* parameter = draggedParameter())
            parameter->beginChangeGesture();
        mouseDrag (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        auto* parameter = draggedParameter();
        if (parameter == nullptr)
            return;
        auto frequency = frequencyAt ((float) event.x);
        // The low crossover stays under the high one (and the other way).
        frequency = dragging == 1 ? juce::jmin (frequency, frequencyOf (true) / 1.25f) : juce::jmax (frequency, frequencyOf (false) * 1.25f);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (frequency));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { endDrag(); }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        dragging = nearestDivider ((float) event.x);
        if (auto* parameter = draggedParameter())
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
        }
        endDrag();
    }

    static juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

private:
    static constexpr float lowest = 20.0f, highest = 20000.0f;

    float xOf (float hz) const
    {
        return (float) getWidth() * std::log (juce::jlimit (lowest, highest, hz) / lowest) / std::log (highest / lowest);
    }

    float frequencyAt (float x) const
    {
        return lowest * std::pow (highest / lowest, juce::jlimit (0.0f, 1.0f, x / juce::jmax (1.0f, (float) getWidth())));
    }

    // 1 the low divider, 2 the high one.
    int nearestDivider (float x) const
    {
        return std::abs (x - xOf (frequencyOf (false))) <= std::abs (x - xOf (frequencyOf (true))) ? 1 : 2;
    }

    juce::RangedAudioParameter* draggedParameter() const { return dragging == 1 ? low : (dragging == 2 ? high : nullptr); }

    void endDrag()
    {
        if (auto* parameter = draggedParameter())
            parameter->endChangeGesture();
        dragging = 0;
        repaint();
    }

    void timerCallback() override
    {
        const auto now = std::make_pair (frequencyOf (false), frequencyOf (true));
        if (now != shown)
        {
            shown = now;
            repaint();
        }
    }

    juce::RangedAudioParameter* low = nullptr;
    juce::RangedAudioParameter* high = nullptr;
    std::array<bool, 3> bandsUsed {};
    int dragging = 0, hovered = 0;
    std::pair<float, float> shown;
};
