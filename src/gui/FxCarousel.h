#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FxLibrary.h"
#include "IlanaLookAndFeel.h"

// The PATCH tab's row of effects under the canvas: every effect the library
// has, in the library's order, one small tile each, coloured by its family.
// A click adds the effect (wired in just before OUT, like + ADD EFFECT); a
// drag picks it up: the page adds it where the pointer is and hands the drag
// to the canvas, so it drops into a cable, or on free space, like any box.
// The row scrolls with the arrows, the wheel or a touchpad sweep.
class FxCarousel : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    struct Tile
    {
        int type;
        juce::String name;
        bool airwindows;
        int group; // an index into fxLibraryGroups()
    };

    // Click: add (type, name). Drag: add it with the pointer (in this
    // component's space) over its box; true when added.
    std::function<void (const Tile&)> onPick;
    std::function<bool (const Tile&, juce::Point<float>)> onGrab;
    // The drag after a grab, handed on (the page forwards it to the canvas).
    std::function<void (const juce::MouseEvent&)> onGrabDrag, onGrabEnd;
    std::function<bool()> isFull;

    // The category picker (a line of small text) above the tiles.
    static constexpr int pickerHeight = 16, stripHeight = pickerHeight + 4 + 28 + 4;

    FxCarousel()
    {
        auto index = 0;
        for (const auto& group : fxLibraryGroups())
        {
            for (const auto& entry : group.entries)
            {
                allTiles.push_back ({ entry.type, entry.name, false, index });
                if (entry.twin > 0)
                    allTiles.push_back ({ entry.twin, entry.twinName, true, index });
            }
            ++index;
        }
        showGroup (-1);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Click an effect to add it before OUT, or drag it onto the canvas, or onto a cable to drop it in.");
    }

    // The tiles on show: every effect, or one category's.
    int numTiles() const { return (int) tiles.size(); }
    // The picker: ALL, then one chip a category. -1 is ALL.
    int numChips() const { return 1 + (int) fxLibraryGroups().size(); }
    int getShownGroup() const { return shownGroup; }
    juce::Rectangle<int> chipBounds (int chip) const { return chipArea (chip); }
    void showGroup (int group)
    {
        shownGroup = group;
        tiles.clear();
        for (const auto& tile : allTiles)
            if (group < 0 || tile.group == group)
                tiles.push_back (tile);
        scroll = 0.0f;
        repaint();
    }
    const Tile& tileAt (int index) const { return tiles[(size_t) index]; }
    juce::Rectangle<int> tileBounds (int index) const { return tileArea (index); }
    juce::Rectangle<int> viewport() const { return strip; }
    float getScroll() const { return scroll; }

    void scrollTo (float newScroll)
    {
        scroll = juce::jlimit (0.0f, maxScroll(), newScroll);
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (0, 4);
        picker = area.removeFromTop (pickerHeight).withTrimmedLeft (arrowWidth + 4);
        area.removeFromTop (4);
        leftArrow = area.removeFromLeft (arrowWidth);
        rightArrow = area.removeFromRight (arrowWidth);
        area.reduce (4, 0);
        strip = area;
        scroll = juce::jlimit (0.0f, maxScroll(), scroll);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        const auto full = isFull != nullptr && isFull();
        for (int chip = 0; chip < numChips(); ++chip)
        {
            const auto selected = chip - 1 == shownGroup;
            const auto colour = chip == 0 ? Ui::text : fxColour (groupColourType (chip - 1));
            const auto box = chipArea (chip).toFloat();
            g.setFont (font (TextSize::label, selected));
            g.setColour (selected ? colour : (chip == hoveredChip ? Ui::text2 : Ui::text3));
            g.drawText (chipName (chip), box, juce::Justification::centred, false);
            if (selected)
                g.fillRect (box.getX() + 4.0f, box.getBottom() - 1.0f, box.getWidth() - 8.0f, 1.5f);
        }
        paintArrow (g, leftArrow, true, scroll > 0.5f);
        paintArrow (g, rightArrow, false, scroll < maxScroll() - 0.5f);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (strip);
        for (int i = 0; i < numTiles(); ++i)
        {
            const auto box = tileArea (i).toFloat();
            if (! box.intersects (strip.toFloat()))
                continue;
            const auto colour = fxColour (tiles[(size_t) i].type);
            const auto over = i == hovered && ! full;
            g.setColour (over ? Ui::hover : Ui::raised);
            g.fillRoundedRectangle (box, 6.0f);
            g.setColour (over ? colour.withAlpha (0.8f) : Ui::line);
            g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
            // The family's colour: a dot, as the tags on the cards.
            g.setColour (colour.withAlpha (full ? 0.35f : 1.0f));
            g.fillEllipse (box.getX() + 9.0f, box.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (full ? Ui::text3 : (over ? Ui::text : Ui::text2));
            g.setFont (font (TextSize::label));
            g.drawText (tiles[(size_t) i].name, box.withTrimmedLeft (21.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft, true);
        }

        // The ends fade into the arrows.
        const auto edge = (float) strip.getX();
        if (scroll > 0.5f)
        {
            g.setGradientFill (juce::ColourGradient (Ui::bg, edge, 0.0f, Ui::bg.withAlpha (0.0f), edge + 18.0f, 0.0f, false));
            g.fillRect (strip.withWidth (18));
        }
        if (scroll < maxScroll() - 0.5f)
        {
            const auto right = (float) strip.getRight();
            g.setGradientFill (juce::ColourGradient (Ui::bg, right, 0.0f, Ui::bg.withAlpha (0.0f), right - 18.0f, 0.0f, false));
            g.fillRect (strip.withTrimmedLeft (strip.getWidth() - 18));
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setHovered (tileIndexAt (e.getPosition()));
        const auto chip = chipIndexAt (e.getPosition());
        if (chip != hoveredChip)
        {
            hoveredChip = chip;
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        setHovered (-1);
        hoveredChip = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabbed = false;
        pressed = -1;
        if (const auto chip = chipIndexAt (e.getPosition()); chip >= 0)
            return showGroup (chip - 1);
        if (leftArrow.contains (e.getPosition()))
            return page (-1);
        if (rightArrow.contains (e.getPosition()))
            return page (1);
        pressed = tileIndexAt (e.getPosition());
        dragScroll = scroll;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (grabbed)
            return forward (e, onGrabDrag);
        if (pressed < 0)
        {
            // Empty space between tiles: drag the row.
            if (strip.contains (e.getMouseDownPosition()))
                scrollTo (dragScroll - (float) e.getDistanceFromDragStartX());
            return;
        }
        // Up, towards the canvas, picks the tile up; sideways along the row scrolls it.
        const auto dx = e.getDistanceFromDragStartX(), dy = e.getDistanceFromDragStartY();
        if (dy < -8 && (isFull == nullptr || ! isFull()) && onGrab != nullptr && onGrab (tiles[(size_t) pressed], e.position))
        {
            grabbed = true;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            forward (e, onGrabDrag);
        }
        else if (std::abs (dx) > 6 && dy >= -8)
        {
            pressed = -1; // (a sweep along the row, not a pick)
            scrollTo (dragScroll - (float) dx);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        if (grabbed)
        {
            grabbed = false;
            forward (e, onGrabEnd);
            return;
        }
        if (pressed >= 0 && pressed == tileIndexAt (e.getPosition()) && e.getDistanceFromDragStart() < 6
            && (isFull == nullptr || ! isFull()) && onPick != nullptr)
            onPick (tiles[(size_t) pressed]);
        pressed = -1;
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        // A mouse notch (vertical) and a touchpad sweep (either way) both slide the row.
        const auto delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : -wheel.deltaY;
        scrollTo (scroll + delta * 360.0f);
    }

private:
    static constexpr int arrowWidth = 22, tileHeight = 28, gap = 6;

    std::vector<Tile> allTiles, tiles;
    int shownGroup = -1, hoveredChip = -1;
    juce::Rectangle<int> leftArrow, rightArrow, strip, picker;
    float scroll = 0.0f, dragScroll = 0.0f;
    int hovered = -1, pressed = -1;
    bool grabbed = false;

    static int tileWidthOf (const Tile& tile)
    {
        return 21 + 8 + (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label)), tile.name));
    }

    int contentWidth() const
    {
        auto width = 0;
        for (const auto& tile : tiles)
            width += tileWidthOf (tile) + gap;
        return width - gap;
    }

    float maxScroll() const { return (float) std::max (0, contentWidth() - strip.getWidth()); }

    juce::Rectangle<int> tileArea (int index) const
    {
        auto x = strip.getX() - (int) std::round (scroll);
        for (int i = 0; i < index; ++i)
            x += tileWidthOf (tiles[(size_t) i]) + gap;
        return { x, strip.getCentreY() - tileHeight / 2, tileWidthOf (tiles[(size_t) index]), tileHeight };
    }

    static juce::String chipName (int chip)
    {
        if (chip == 0)
            return "ALL";
        const juce::String title (fxLibraryGroups()[(size_t) chip - 1].title);
        return title == "TONE & LEVEL" ? "TONE" : (title == "MORE AIRWINDOWS" ? "AIRWINDOWS" : title);
    }

    // A type of the category's family, for its chip's colour.
    static int groupColourType (int group) { return fxLibraryGroups()[(size_t) group].entries.front().type; }

    int chipWidth (int chip) const
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label, true)), chipName (chip))) + 18;
    }

    juce::Rectangle<int> chipArea (int chip) const
    {
        auto x = picker.getX();
        for (int i = 0; i < chip; ++i)
            x += chipWidth (i);
        return { x, picker.getY(), chipWidth (chip), picker.getHeight() };
    }

    int chipIndexAt (juce::Point<int> point) const
    {
        for (int chip = 0; chip < numChips(); ++chip)
            if (chipArea (chip).contains (point))
                return chip;
        return -1;
    }

    int tileIndexAt (juce::Point<int> point) const
    {
        if (! strip.contains (point))
            return -1;
        for (int i = 0; i < numTiles(); ++i)
            if (tileArea (i).contains (point))
                return i;
        return -1;
    }

    void setHovered (int index)
    {
        if (index == hovered)
            return;
        hovered = index;
        repaint();
    }

    void page (int direction) { scrollTo (scroll + (float) direction * (float) strip.getWidth() * 0.8f); }

    void forward (const juce::MouseEvent& e, const std::function<void (const juce::MouseEvent&)>& to)
    {
        if (to != nullptr)
            to (e);
    }

    static void paintArrow (juce::Graphics& g, juce::Rectangle<int> box, bool left, bool enabled)
    {
        using namespace IlanaTheme;
        const auto centre = box.toFloat().getCentre();
        const auto s = left ? -1.0f : 1.0f;
        juce::Path chevron;
        chevron.startNewSubPath (centre.x - 2.5f * s, centre.y - 5.0f);
        chevron.lineTo (centre.x + 2.5f * s, centre.y);
        chevron.lineTo (centre.x - 2.5f * s, centre.y + 5.0f);
        g.setColour (enabled ? Ui::text2 : Ui::track);
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxCarousel)
};
