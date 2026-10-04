#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/airwindows/Categories.h"
#include "IlanaLookAndFeel.h"
#include "FxDisplays.h"

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
// I6-26): the Airwindows category modules sit in their group under a small
// AIRWINDOWS heading instead of "AW" prefixes, and the all-in-one module is
// "Airwindows (all)". Display only: the saved choice strings stay the same.
struct FxLibraryEntry
{
    int type;
    const char* name;
};

struct FxLibraryGroup
{
    const char* title;
    int column; // of five
    std::vector<FxLibraryEntry> classic, airwindows;
};

inline const std::vector<FxLibraryGroup>& fxLibraryGroups()
{
    static const std::vector<FxLibraryGroup> groups {
        { "SPACE", 0, { { 13, "Reverb" }, { 9, "Delay" }, { 15, "Dimension" }, { 11, "Smear" }, { 12, "Freeze" }, { 8, "Haas" }, { 22, "Widener" } },
                      { { 34, "Spaces" }, { 35, "Echoes" }, { 41, "Stereo" } } },
        { "DRIVE", 1, { { 2, "Drive" }, { 1, "Amp" }, { 3, "Crush" }, { 26, "Octaver" }, { 28, "Feedback" } },
                      { { 32, "Tape" }, { 33, "Saturation" }, { 39, "Console" }, { 40, "Lo-Fi" } } },
        { "MOTION", 2, { { 7, "Chorus" }, { 6, "Phaser" }, { 14, "Flanger" }, { 23, "Tremolo" }, { 24, "Freq Shift" }, { 25, "Ring Mod" }, { 5, "Comb" } },
                       { { 36, "Ensemble" } } },
        { "RHYTHM", 3, { { 16, "Trance Gate" }, { 10, "Stutter" }, { 17, "Tape Stop" } }, {} },
        { "VOICE", 3, { { 27, "Vowel" }, { 31, "Vocoder" } }, {} },
        { "AIRWINDOWS", 3, {}, { { 30, "Airwindows (all)" } } },
        { "TONE & LEVEL", 4, { { 29, "EQ" }, { 18, "Tilt" }, { 4, "Comp" }, { 20, "OTT" }, { 21, "Limiter" }, { 19, "Utility" } },
                             { { 38, "Tone" }, { 37, "Glue" } } } };
    return groups;
}

// An effect's name, as the library, the card titles and the menus show it.
inline juce::String fxTypeName (int type)
{
    for (const auto& group : fxLibraryGroups())
        for (const auto* list : { &group.classic, &group.airwindows })
            for (const auto& entry : *list)
                if (entry.type == type)
                    return entry.name;
    return "-";
}

// Airwindows' effects: the all-in-one module and the ten category modules.
inline bool isAirwindowsFxType (int type) { return type == 30 || airwindows::categoryForFxType (type) >= 0; }

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
// colour once it is in the rack (V6-25: a dot, not greyed out; a click on it
// shows its card, as the rack takes each effect once).
class FxLibraryButton : public juce::TextButton
{
public:
    FxLibraryButton (int typeIn, const juce::String& name)
        : juce::TextButton (name.toUpperCase()), type (typeIn) {}

    int getType() const { return type; }
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
        auto fill = IlanaTheme::Ui::raised.interpolatedWith (colour, highlighted ? 0.16f : 0.08f);
        if (down)
            fill = fill.darker (0.2f);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (highlighted ? colour.withAlpha (0.6f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds.withWidth (3.0f).reduced (0.0f, 6.0f).translated (4.0f, 0.0f), 1.5f);

        auto text = getLocalBounds().reduced (12, 0);
        if (inRackSlot >= 0)
        {
            const auto dot = text.removeFromRight (10).toFloat();
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot.getCentre()));
            text.removeFromRight (4);
        }

        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.35f));
        g.setFont (IlanaTheme::pillFont());
        g.drawFittedText (getButtonText(), text, juce::Justification::centredLeft, 1, 0.9f);
    }

private:
    int type;
    int inRackSlot = -1;
};

// The whole library in five columns, each group under a small heading and
// its Airwindows entries under a quieter one. The page shows it while the
// rack is empty; + ADD EFFECT opens a copy in a call-out.
class FxLibraryView : public juce::Component
{
public:
    // slotOf: the slot holding a type, or -1. onPick: a type was clicked.
    FxLibraryView (std::function<int (int)> slotOfIn, std::function<void (int)> onPickIn)
        : slotOf (std::move (slotOfIn)), onPick (std::move (onPickIn))
    {
        for (const auto& group : fxLibraryGroups())
            for (const auto* list : { &group.classic, &group.airwindows })
                for (const auto& entry : *list)
                {
                    auto button = std::make_unique<FxLibraryButton> (entry.type, entry.name);
                    const auto type = entry.type;
                    button->onClick = [this, type]
                    {
                        if (onPick != nullptr)
                            onPick (type);
                        if (afterPick != nullptr)
                            afterPick();
                    };
                    addAndMakeVisible (*button);
                    buttons.push_back (std::move (button));
                }
        refresh();
    }

    // Called after a pick (the call-out closes itself with it).
    std::function<void()> afterPick;

    static constexpr int columns = 5, buttonHeight = 26, headingHeight = 18, subHeadingHeight = 16, groupGap = 10;

    // The tallest column's height, buttons and headings.
    static int preferredHeight()
    {
        std::array<int, columns> heights {};
        for (const auto& group : fxLibraryGroups())
        {
            auto& height = heights[(size_t) group.column];
            if (height > 0)
                height += groupGap;
            height += headingHeight + (int) group.classic.size() * buttonHeight;
            if (! group.airwindows.empty())
                height += (group.classic.empty() ? 0 : subHeadingHeight) + (int) group.airwindows.size() * buttonHeight;
        }
        return *std::max_element (heights.begin(), heights.end());
    }

    // Marks what is in the rack, and says so in the tooltips.
    void refresh()
    {
        for (auto& button : buttons)
        {
            const auto type = button->getType();
            const auto slot = slotOf != nullptr ? slotOf (type) : -1;
            button->setInRackSlot (slot);
            auto tip = slot >= 0 ? fxTypeName (type) + " is in slot " + juce::String (slot + 1)
                                       + ": click to show its card (the rack takes each effect once)."
                                 : "Add " + fxTypeName (type) + " to the first empty slot.";
            if (isAirwindowsFxType (type))
                tip << "\n" << airwindowsBadgeTip (type);
            button->setTooltip (tip);
        }
    }

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
            g.setColour (heading.quiet ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (heading.text, heading.bounds.withTrimmedLeft (2), juce::Justification::bottomLeft);
            if (heading.quiet)
            {
                // A hairline after the word, so the sub-group reads as one.
                const auto x = (float) heading.bounds.getX() + 6.0f
                               + (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), heading.text);
                g.setColour (IlanaTheme::Ui::line);
                g.fillRect (juce::Rectangle<float> (juce::jmax (0.0f, (float) heading.bounds.getRight() - x), 1.0f)
                                .withPosition (x, (float) heading.bounds.getBottom() - 6.0f));
            }
        }
    }

    void resized() override
    {
        headings.clear();
        const auto gap = 10;
        const auto columnWidth = (getWidth() - gap * (columns - 1)) / columns;
        std::array<int, columns> y {};
        size_t index = 0;

        for (const auto& group : fxLibraryGroups())
        {
            const auto x = group.column * (columnWidth + gap);
            auto& top = y[(size_t) group.column];
            if (top > 0)
                top += groupGap;
            headings.push_back ({ { x, top, columnWidth, headingHeight - 4 }, group.title, false });
            top += headingHeight;

            for (size_t i = 0; i < group.classic.size(); ++i, ++index)
            {
                buttons[index]->setBounds (x, top, columnWidth, buttonHeight - 3);
                top += buttonHeight;
            }

            if (! group.airwindows.empty() && ! group.classic.empty())
            {
                headings.push_back ({ { x, top, columnWidth, subHeadingHeight }, "AIRWINDOWS", true });
                top += subHeadingHeight;
            }

            for (size_t i = 0; i < group.airwindows.size(); ++i, ++index)
            {
                buttons[index]->setBounds (x, top, columnWidth, buttonHeight - 3);
                top += buttonHeight;
            }
        }
    }

private:
    struct Heading
    {
        juce::Rectangle<int> bounds;
        juce::String text;
        bool quiet;
    };

    std::function<int (int)> slotOf;
    std::function<void (int)> onPick;
    std::vector<std::unique_ptr<FxLibraryButton>> buttons;
    std::vector<Heading> headings;
};

// A slot's blend in its card header (S6-25: on every card, not only the
// selected one): a small bar that reads "BLEND 100%", dragged sideways.
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
        const auto radius = bounds.getHeight() * 0.5f;
        const auto amount = (float) juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
        g.setColour (IlanaTheme::Ui::well);
        g.fillRoundedRectangle (bounds, radius);
        if (amount > 0.0f)
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (bounds.withWidth (bounds.getWidth() * amount).toNearestInt());
            g.setColour (colour.withAlpha (isEnabled() ? 0.22f : 0.1f));
            g.fillRoundedRectangle (bounds, radius);
        }
        g.setColour (isMouseOverOrDragging() ? colour.withAlpha (0.8f) : IlanaTheme::Ui::line.brighter (0.15f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        g.setColour (IlanaTheme::Ui::text.withAlpha (isEnabled() ? 0.9f : 0.5f));
        g.setFont (IlanaTheme::pillFont());
        g.drawText ("BLEND " + juce::String (juce::roundToInt (amount * 100.0f)) + "%", bounds, juce::Justification::centred);
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
            g.setColour (bandsUsed[(size_t) b] ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (names[b], region.toNearestInt(), juce::Justification::centred, false);
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
