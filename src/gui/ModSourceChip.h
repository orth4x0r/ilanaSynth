#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

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

        if (pinnedModSource() == index)
            pinnedModSource() = 0;
    }

    ModSourceChip (const juce::String& sourceName, int sourceIndex)
        : name (sourceName),
          index (sourceIndex)
    {
        setTooltip (sourceName + "\nDrag onto any knob to modulate it.  Knobs it already modulates light up while you hover; click to keep them lit, click again to clear.");
        startTimerHz (30);
    }

    const juce::String& getSourceName() const { return name; }
    int getSourceIndex() const { return index; }

    // A short name ("E6") for when the row is crowded; the tooltip keeps the
    // full one.
    void setShortName (const juce::String& text) { shortName = text; }
    const juce::String& getShortName() const { return shortName.isNotEmpty() ? shortName : name; }
    bool isCompact() const { return compact; }
    void setCompact (bool shouldBeCompact)
    {
        if (compact != shouldBeCompact)
        {
            compact = shouldBeCompact;
            repaint();
        }
    }

    // The source's live value (LFO position, envelope level, wheel...); the
    // chip glows with it. Optional.
    std::function<float()> valueProvider;

    // An LFO whose shape has a second output: a small "B" at the chip's right
    // end drags that output (LFO n B) instead. Optional.
    std::function<bool()> hasSecondOutput;
    int secondIndex = 0;

    // The "B" sub-chip's bounds, empty while there is none (the UI test reads it).
    juce::Rectangle<float> getSecondOutputBounds() const
    {
        if (compact || hasSecondOutput == nullptr || ! hasSecondOutput())
            return {};

        const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        return { bounds.getRight() - 19.0f, bounds.getY() + 3.0f, 16.0f, bounds.getHeight() - 6.0f };
    }

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

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));

        // A crowded row's short chips carry their colour as a bar under the
        // name instead of a dot beside it, so the name keeps the width.
        if (compact)
        {
            const auto bar = juce::Rectangle<float> (bounds.getWidth() - 10.0f, 2.5f)
                                 .withCentre ({ bounds.getCentreX(), bounds.getBottom() - 3.5f });
            g.setColour (colour.withAlpha (0.7f + 0.3f * glow));
            g.fillRoundedRectangle (bar, 1.25f);
            g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, lit));
            g.drawFittedText (getShortName(), getLocalBounds().reduced (2, 0).withTrimmedBottom (2),
                              juce::Justification::centred, 1, 0.8f);
            return;
        }

        const auto gripX = bounds.getX() + 1.0f;
        const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ gripX + 14.0f, bounds.getCentreY() });
        g.setColour (colour.withAlpha (0.25f + 0.35f * glow));
        g.fillEllipse (dot.expanded (1.5f + 2.0f * glow));
        g.setColour (colour);
        g.fillEllipse (dot);

        const auto second = getSecondOutputBounds();
        g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, lit));
        g.drawFittedText (name, getLocalBounds().withTrimmedLeft (juce::roundToInt (gripX + 19.0f))
                                    .withTrimmedRight (second.isEmpty() ? 3 : 22),
                          juce::Justification::centred, 1, 0.85f);

        if (! second.isEmpty())
        {
            const auto hot = highlightedModSource() == secondIndex;
            g.setColour (colour.withAlpha (hot ? 0.35f : 0.15f));
            g.fillRoundedRectangle (second, 3.0f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (second.reduced (0.5f), 3.0f, 1.0f);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("B", second, juce::Justification::centred);
        }
    }

    // Which output the mouse is over: the "B" sub-chip's, or the chip's own.
    int sourceAt (juce::Point<float> position) const
    {
        return getSecondOutputBounds().contains (position) ? secondIndex : index;
    }

    void mouseEnter (const juce::MouseEvent& event) override { highlightedModSource() = sourceAt (event.position); }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (highlightedModSource() != sourceAt (event.position))
        {
            highlightedModSource() = sourceAt (event.position);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (highlightedModSource() == index || (secondIndex != 0 && highlightedModSource() == secondIndex))
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

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                const auto second = getSecondOutputBounds();
                const auto dragsSecond = second.contains (event.mouseDownPosition);
                auto image = createComponentSnapshot (dragsSecond ? second.toNearestInt() : getLocalBounds(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String (dragsSecond ? secondIndex : index), this,
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

        if (const auto second = ! compact && hasSecondOutput != nullptr && hasSecondOutput(); second != showsSecond)
        {
            showsSecond = second;
            changed = true;
        }

        if (std::abs (hover - target) >= 0.005f)
        {
            hover = IlanaAnim::approach (hover, target, 0.22f, frameTicks());
            changed = true;
        }

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

    juce::String name, shortName;
    int index = 0;
    bool compact = false;
    bool wasPinned = false;
    bool showsSecond = false;
    float hover = 0.0f;
    float activity = 0.0f;
};
