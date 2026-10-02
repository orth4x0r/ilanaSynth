#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"
#include "ParamControls.h"

// A modulation source in the bar under the pages: its colour and its full
// name (never a code: a crowded bar groups chips instead, see
// ModSourceGroupChip). Drag it onto a knob to modulate the knob; hover to
// light the knobs it drives (and, after a moment, see where it goes); click
// to keep them lit.
class ModSourceChip : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~ModSourceChip() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;

        if (pinnedModSource() == index && ! keepsPinOnDelete)
            pinnedModSource() = 0;
    }

    ModSourceChip (const juce::String& sourceName, int sourceIndex)
        : name (sourceName),
          index (sourceIndex)
    {
        setTooltip (ModNames::source (sourceIndex) + "\nDrag onto any knob to modulate it.  Knobs it already modulates light up "
                                                     "while you hover; click to keep them lit, click again to clear.");
        startTimerHz (30);
    }

    const juce::String& getSourceName() const { return name; }
    int getSourceIndex() const { return index; }

    // A chip in a group's tray is rebuilt each time the tray opens; its pin
    // outlives it.
    void setKeepsPinOnDelete (bool shouldKeep) { keepsPinOnDelete = shouldKeep; }

    // The width the chip needs for its name (the bar shares out the rest).
    static float widthFor (const juce::String& text)
    {
        return (float) juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true), text) + 30.0f;
    }

    float getNaturalWidth() const { return widthFor (name); }

    // The source's live value (LFO position, envelope level, wheel...); the
    // chip glows with it. Optional.
    std::function<float()> valueProvider;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const auto colour = modSourceColour (index);
        const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.3f);
        const auto glow = juce::jlimit (0.0f, 1.0f, activity);
        const auto pinned = isPinned();
        const auto lit = juce::jmax (hover, glow, pinned ? 1.0f : 0.0f);

        // Grey chips; the source's colour is only a dot, which glows while
        // the source is moving something.
        if (lit > 0.02f)
            IlanaTheme::paintGlow (g, bounds, radius, colour, 0.9f * hover + 0.8f * glow);

        g.setColour (IlanaTheme::Ui::raised.interpolatedWith (colour, 0.08f * hover + 0.1f * glow));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (IlanaTheme::Ui::line.interpolatedWith (colour, 0.7f * lit));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, pinned ? 2.0f : 1.0f);

        const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ bounds.getX() + 11.0f, bounds.getCentreY() });
        g.setColour (colour.withAlpha (0.25f + 0.35f * glow));
        g.fillEllipse (dot.expanded (1.5f + 2.0f * glow));
        g.setColour (colour);
        g.fillEllipse (dot);

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));
        g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, lit));
        g.drawFittedText (name, getLocalBounds().withTrimmedLeft (juce::roundToInt (bounds.getX() + 17.0f)).withTrimmedRight (3),
                          juce::Justification::centred, 1, 0.85f);
    }

    void mouseEnter (const juce::MouseEvent&) override { highlightedModSource() = index; }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (highlightedModSource() == index)
            highlightedModSource() = 0;
    }

    // A click (not a drag) pins the source: every knob it drives stays lit
    // until the chip is clicked again.
    void togglePinned()
    {
        pinnedModSource() = pinnedModSource() == index ? 0 : index;
    }

    bool isPinned() const { return pinnedModSource() == index; }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasDraggedSinceMouseDown() && ! event.mods.isPopupMenu() && getLocalBounds().contains (event.getPosition()))
            togglePinned();
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String (index), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        auto changed = false;
        const auto target = isMouseOver() ? 1.0f : 0.0f;
        const auto pinnedNow = isPinned();

        if (pinnedNow != wasPinned)
        {
            wasPinned = pinnedNow;
            changed = true;
        }

        if (std::abs (hover - target) >= 0.005f)
        {
            hover = IlanaAnim::approach (hover, target, 0.22f, frameTicks());
            changed = true;
        }

        // Resting on the chip (not dragging it) shows where the source goes.
        const auto resting = isMouseOver() && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
        if (resting && hoverRest < 0.6f && (hoverRest += frameSeconds()) >= 0.6f && modHoverHooks().showSource != nullptr)
            modHoverHooks().showSource (*this, index);
        else if (! resting)
            hoverRest = 0.0f;

        if (valueProvider != nullptr)
        {
            const auto value = std::abs (valueProvider());
            const auto next = IlanaAnim::approach (activity, juce::jlimit (0.0f, 1.0f, value), 0.35f, frameTicks());

            if (std::abs (next - activity) > 0.01f)
            {
                activity = next;
                changed = true;
            }
        }

        if (changed)
            repaint();
    }

    juce::String name;
    int index = 0;
    bool wasPinned = false, keepsPinOnDelete = false;
    float hover = 0.0f;
    float activity = 0.0f;
    float hoverRest = 0.0f;
};

// When the bar can't fit every chip, the LFOs, the envelopes and then the
// performance sources each fold their last chips into one group chip
// ("LFO +4"), with a dot per source in its colour. Hover or click it to open
// a tray of those chips above it; they drag and pin like any other.
class ModSourceGroupChip : public juce::Component,
                           public juce::SettableTooltipClient,
                           private IlanaAnim::FrameTimer
{
public:
    explicit ModSourceGroupChip (const juce::String& groupName) : group (groupName)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    // Opens (or closes) the tray of this group's chips.
    std::function<void (ModSourceGroupChip&)> onOpen;

    void setSources (const std::vector<int>& newSources)
    {
        if (newSources == sources)
            return;

        sources = newSources;
        juce::StringArray names;
        for (const auto source : sources)
            names.add (ModNames::sourceUpper (source));
        setTooltip (names.joinIntoString (",  ") + "\nHover or click to show them; drag one onto a knob to modulate it.");
        repaint();
    }

    const std::vector<int>& getSources() const { return sources; }
    juce::String getLabel() const { return group + " +" + juce::String ((int) sources.size()); }
    float getNaturalWidth() const { return widthFor (group, (int) sources.size()); }

    // The width a group chip of n sources needs.
    static float widthFor (const juce::String& groupName, int count)
    {
        return (float) juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true),
                                                                 groupName + " +" + juce::String (count))
               + 28.0f;
    }

    void setOpen (bool shouldBeOpen)
    {
        if (open != shouldBeOpen)
        {
            open = shouldBeOpen;
            repaint();
        }
    }

    bool isOpen() const { return open; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.3f);
        const auto lit = juce::jmax (hover, open ? 1.0f : 0.0f);

        g.setColour (IlanaTheme::Ui::raised.interpolatedWith (juce::Colours::white, 0.05f * lit));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (IlanaTheme::Ui::line.interpolatedWith (IlanaTheme::Ui::text2, 0.6f * lit));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

        // The members' colours, as a row of small dots under the name.
        const auto count = juce::jmin ((int) sources.size(), 8);
        const auto pitch = 5.0f;
        auto x = bounds.getCentreX() - pitch * (float) (count - 1) * 0.5f - 5.0f;
        for (int i = 0; i < count; ++i, x += pitch)
        {
            g.setColour (modSourceColour (sources[(size_t) i]));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, bounds.getBottom() - 3.5f }));
        }

        auto text = getLocalBounds().reduced (6, 0).withTrimmedBottom (3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));
        g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, lit));
        const auto caret = text.removeFromRight (10).toFloat();
        g.drawFittedText (getLabel(), text, juce::Justification::centred, 1, 0.85f);

        juce::Path down;
        const auto c = caret.getCentre();
        down.addTriangle (c.x - 3.5f, c.y - 1.5f, c.x + 3.5f, c.y - 1.5f, c.x, c.y + 2.5f);
        g.fillPath (down);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains (event.getPosition()) && onOpen != nullptr)
            onOpen (*this);
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        const auto target = isMouseOver() ? 1.0f : 0.0f;

        if (std::abs (hover - target) >= 0.005f)
        {
            hover = IlanaAnim::approach (hover, target, 0.22f, frameTicks());
            repaint();
        }

        // Resting on it opens the tray (a click opens it at once).
        if (isMouseOver() && ! open && (rest += frameSeconds()) > 0.3f && onOpen != nullptr)
        {
            rest = 0.0f;
            onOpen (*this);
        }
        else if (! isMouseOver())
        {
            rest = 0.0f;
        }
    }

    juce::String group;
    std::vector<int> sources;
    bool open = false;
    float hover = 0.0f, rest = 0.0f;
};

// The tray a group chip opens: its sources as full chips, in a row above
// the bar (more rows when long). Closes a moment after the mouse leaves it
// and its group chip, never in the middle of a drag.
class ModSourceTray : public juce::Component,
                      private IlanaAnim::FrameTimer
{
public:
    std::function<std::unique_ptr<ModSourceChip> (int source)> makeChip;

    void openFor (ModSourceGroupChip& groupChip)
    {
        if (owner != nullptr)
            owner->setOpen (false);

        owner = &groupChip;
        owner->setOpen (true);
        chips.clear();

        for (const auto source : groupChip.getSources())
            if (makeChip != nullptr)
                if (auto chip = makeChip (source))
                {
                    chip->setKeepsPinOnDelete (true);
                    addAndMakeVisible (*chip);
                    chips.push_back (std::move (chip));
                }

        layout();
        leaveSeconds = 0.0f;
        setVisible (true);
        toFront (false);
        startTimerHz (30);
    }

    void close()
    {
        if (owner != nullptr)
            owner->setOpen (false);

        owner = nullptr;
        setVisible (false);
        stopTimer();
        chips.clear();
    }

    bool isOpenFor (const ModSourceGroupChip& groupChip) const { return isVisible() && owner.getComponent() == &groupChip; }
    const std::vector<std::unique_ptr<ModSourceChip>>& getChips() const { return chips; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.0f);
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    }

private:
    static constexpr int chipHeight = 24, pad = 6, perRow = 6;

    void layout()
    {
        auto* parent = getParentComponent();

        if (parent == nullptr || owner == nullptr || chips.empty())
            return;

        const auto rows = ((int) chips.size() + perRow - 1) / perRow;
        auto widest = 0.0f;
        for (auto& chip : chips)
            widest = juce::jmax (widest, chip->getNaturalWidth());

        const auto columns = juce::jmin ((int) chips.size(), perRow);
        const auto chipWidth = (int) std::ceil (widest) + 4;
        const auto width = columns * chipWidth + pad * 2;
        const auto height = rows * chipHeight + pad * 2;
        const auto anchor = parent->getLocalArea (owner.getComponent(), owner->getLocalBounds());
        const auto x = juce::jlimit (6, juce::jmax (6, parent->getWidth() - width - 6), anchor.getCentreX() - width / 2);
        setBounds (x, anchor.getY() - height - 3, width, height);

        for (int i = 0; i < (int) chips.size(); ++i)
            chips[(size_t) i]->setBounds (pad + (i % perRow) * chipWidth, pad + (i / perRow) * chipHeight, chipWidth, chipHeight);
    }

    void timerCallback() override
    {
        if (owner == nullptr || ! owner->isVisible())
        {
            close();
            return;
        }

        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        const auto dragging = container != nullptr && container->isDragAndDropActive();

        if (dragging || isMouseOver (true) || owner->isMouseOver (true))
            leaveSeconds = 0.0f;
        else if ((leaveSeconds += frameSeconds()) > 0.4f)
            close();
    }

    juce::Component::SafePointer<ModSourceGroupChip> owner;
    std::vector<std::unique_ptr<ModSourceChip>> chips;
    float leaveSeconds = 0.0f;
};
