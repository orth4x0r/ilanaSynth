#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <tuple>

#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// The hover line (UI review 6: the status line only while hovering). It
// has no row of its own: while the mouse rests on a control that explains
// itself (its tooltip text), the line fades in over the source chip row
// with the control's name and description, and fades out when the mouse
// leaves it, comes down to the chips or the macros (quietArea), or a drag
// is under way. It never takes a click.
class InfoStrip : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    InfoStrip()
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (20);
    }

    // Where the line stays hidden (in the parent's coordinates): the rows
    // it covers and the strip under them.
    void setQuietArea (juce::Rectangle<int> area) { quietArea = area; }

    // Whether the line is showing, and what it says, for the tests.
    bool isShowingLine() const { return target > 0.5f; }
    juce::String getShownTitle() const { return isShowingLine() ? title : juce::String(); }

    // The text under a component, as the hover line reads it: the first
    // tooltip on the way up, its first line the title.
    static std::pair<juce::String, juce::String> describe (juce::Component* component)
    {
        for (auto* current = component; current != nullptr; current = current->getParentComponent())
        {
            if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*> (current))
            {
                const auto tooltip = tooltipClient->getTooltip();

                if (tooltip.isNotEmpty())
                {
                    const auto lines = juce::StringArray::fromLines (tooltip);
                    juce::String description;

                    for (int i = 1; i < lines.size(); ++i)
                        description += (i > 1 ? "  " : "") + lines[i];

                    return { lines[0], description };
                }
            }
        }

        return {};
    }

    void paint (juce::Graphics& g) override
    {
        if (shown <= 0.01f)
            return;

        g.setOpacity (juce::jlimit (0.0f, 1.0f, shown));
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::bg);
        g.fillRect (bounds);
        IlanaTheme::paintWell (g, bounds.reduced (0.0f, 1.0f), 4.0f);

        auto textArea = getLocalBounds().reduced (10, 0);
        const auto titleFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto titleWidth = juce::jmin (280, juce::GlyphArrangement::getStringWidthInt (titleFont, title) + 16);

        g.setColour (IlanaTheme::accent());
        g.setFont (titleFont);
        g.drawText (title, textArea.removeFromLeft (titleWidth), juce::Justification::centredLeft, true);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (description, textArea, juce::Justification::centredLeft);

        // Long text trails off instead of being chopped off.
        const auto fadeWidth = juce::jmin (30.0f, (float) textArea.getWidth() * 0.5f);
        const auto fadeX = (float) textArea.getRight() - fadeWidth;

        if (fadeWidth > 4.0f)
        {
            juce::ColourGradient fade (IlanaTheme::Ui::well.withAlpha (0.0f), fadeX, 0.0f,
                                       IlanaTheme::Ui::well, (float) textArea.getRight(), 0.0f, false);
            g.setGradientFill (fade);
            g.fillRect (juce::Rectangle<float> (fadeX, 0.0f, fadeWidth, (float) getHeight()));
        }
    }

    // For the tests: what the line does with the mouse resting on a
    // component (or nothing) for long enough.
    void restOn (juce::Component* component)
    {
        forced = true;

        for (int i = 0; i < 12; ++i)
            step (component, false);

        forced = false;
    }

private:
    void timerCallback() override
    {
        if (forced)
            return;

        auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto* under = mouse.getComponentUnderMouse();
        auto* parent = getParentComponent();
        const auto dragging = mouse.isDragging() || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
        const auto quiet = parent != nullptr
                           && quietArea.contains (parent->getLocalPoint (nullptr, mouse.getScreenPosition()).roundToInt());
        const auto inside = parent != nullptr && under != nullptr && (under == parent || parent->isParentOf (under));

        step (inside && ! quiet ? under : nullptr, dragging);
    }

    void step (juce::Component* under, bool dragging)
    {
        juce::String newTitle, newDescription;

        if (under != nullptr && ! dragging)
            std::tie (newTitle, newDescription) = describe (under);

        // A control's text shows once the mouse has rested on it a moment,
        // so sweeping across the page doesn't flicker the chips away.
        if (newTitle != pendingTitle || newDescription != pendingDescription)
        {
            pendingTitle = newTitle;
            pendingDescription = newDescription;
            restTicks = 0.0f;
        }
        else
        {
            restTicks += forced ? 1.0f : frameTicks();
        }

        const auto wanted = pendingTitle.isNotEmpty() && restTicks >= 6.0f;

        if (wanted && (pendingTitle != title || pendingDescription != description))
        {
            title = pendingTitle;
            description = pendingDescription;
            repaint();
        }

        target = wanted ? 1.0f : 0.0f;

        if (forced)
        {
            shown = target;
            repaint();
        }
        else if (std::abs (shown - target) > 0.01f)
        {
            shown = target > shown ? juce::jmin (1.0f, shown + 0.35f * frameTicks()) : juce::jmax (0.0f, shown - 0.5f * frameTicks());
            repaint();
        }
        else if (shown != target)
        {
            shown = target;
            repaint();
        }
    }

    juce::String title, description, pendingTitle, pendingDescription;
    juce::Rectangle<int> quietArea;
    float shown = 0.0f, target = 0.0f, restTicks = 0.0f;
    bool forced = false;
};
