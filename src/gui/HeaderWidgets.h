#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

// Small vector icons for the header, drawn as paths so they stay crisp at
// every zoom level (the Jost font has no arrow or gear glyphs).
namespace IlanaIcons
{
enum class Icon
{
    Undo,
    Redo,
    History,
    Gear,
    Dice,
    ChevronLeft,
    ChevronRight,
    Star,
    More,
    Save
};

inline juce::Path make (Icon icon, juce::Rectangle<float> r)
{
    juce::Path p;
    const auto cx = r.getCentreX();
    const auto cy = r.getCentreY();
    const auto s = juce::jmin (r.getWidth(), r.getHeight());

    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            // A hooked arrow: arc over the top, arrowhead on the left (undo).
            const auto radius = s * 0.3f;
            p.addCentredArc (cx, cy + s * 0.06f, radius, radius, 0.0f,
                             -juce::MathConstants<float>::halfPi * 1.25f, juce::MathConstants<float>::halfPi * 0.95f, true);
            const juce::Point<float> tip (cx - radius * 0.96f, cy - radius * 0.2f);
            juce::Path head;
            head.addTriangle (tip.x - s * 0.13f, tip.y - s * 0.02f, tip.x + s * 0.1f, tip.y - s * 0.02f,
                              tip.x - s * 0.02f, tip.y + s * 0.15f);

            juce::Path stroked;
            juce::PathStrokeType (s * 0.09f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (stroked, p);
            stroked.addPath (head);

            if (icon == Icon::Redo)
                stroked.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, cx, cy));

            return stroked;
        }

        case Icon::History:
        {
            const auto radius = s * 0.34f;
            juce::Path outline;
            outline.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ cx, cy }));
            outline.startNewSubPath (cx, cy - radius * 0.62f);
            outline.lineTo (cx, cy);
            outline.lineTo (cx + radius * 0.48f, cy + radius * 0.3f);

            juce::PathStrokeType (s * 0.085f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (p, outline);
            return p;
        }

        case Icon::Gear:
        {
            const auto outer = s * 0.4f;
            const auto inner = s * 0.29f;
            constexpr int teeth = 8;

            for (int i = 0; i < teeth * 2; ++i)
            {
                const auto a0 = (float) i / (float) (teeth * 2) * juce::MathConstants<float>::twoPi;
                const auto a1 = (float) (i + 1) / (float) (teeth * 2) * juce::MathConstants<float>::twoPi;
                const auto radius = i % 2 == 0 ? outer : inner;
                const juce::Point<float> pa (cx + std::cos (a0) * radius, cy + std::sin (a0) * radius);
                const juce::Point<float> pb (cx + std::cos (a1) * radius, cy + std::sin (a1) * radius);

                if (i == 0)
                    p.startNewSubPath (pa);
                else
                    p.lineTo (pa);

                p.lineTo (pb);
            }

            p.closeSubPath();
            p.addEllipse (juce::Rectangle<float> (s * 0.26f, s * 0.26f).withCentre ({ cx, cy }));
            p.setUsingNonZeroWinding (false);
            return p;
        }

        case Icon::Dice:
        {
            const auto box = juce::Rectangle<float> (s * 0.66f, s * 0.66f).withCentre ({ cx, cy });
            p.addRoundedRectangle (box, s * 0.12f);

            const auto pip = s * 0.12f;

            for (const auto& offset : { juce::Point<float> (-0.18f, -0.18f), juce::Point<float> (0.18f, 0.18f),
                                        juce::Point<float> (0.0f, 0.0f), juce::Point<float> (0.18f, -0.18f),
                                        juce::Point<float> (-0.18f, 0.18f) })
                p.addEllipse (juce::Rectangle<float> (pip, pip).withCentre ({ cx + offset.x * s, cy + offset.y * s }));

            p.setUsingNonZeroWinding (false);
            return p;
        }

        case Icon::ChevronLeft:
        case Icon::ChevronRight:
        {
            juce::Path line;
            const auto dir = icon == Icon::ChevronLeft ? 1.0f : -1.0f;
            line.startNewSubPath (cx + dir * s * 0.1f, cy - s * 0.22f);
            line.lineTo (cx - dir * s * 0.1f, cy);
            line.lineTo (cx + dir * s * 0.1f, cy + s * 0.22f);
            juce::PathStrokeType (s * 0.1f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
                .createStrokedPath (p, line);
            return p;
        }

        case Icon::Star:
            p.addStar ({ cx, cy + s * 0.02f }, 5, s * 0.17f, s * 0.38f, 0.0f);
            return p;

        case Icon::More:
            for (int i = -1; i <= 1; ++i)
                p.addEllipse (juce::Rectangle<float> (s * 0.13f, s * 0.13f).withCentre ({ cx + (float) i * s * 0.22f, cy }));
            return p;

        case Icon::Save:
        {
            // Down arrow into a tray.
            juce::Path line;
            line.startNewSubPath (cx, cy - s * 0.3f);
            line.lineTo (cx, cy + s * 0.08f);
            line.startNewSubPath (cx - s * 0.15f, cy - s * 0.06f);
            line.lineTo (cx, cy + s * 0.1f);
            line.lineTo (cx + s * 0.15f, cy - s * 0.06f);
            line.startNewSubPath (cx - s * 0.3f, cy + s * 0.08f);
            line.lineTo (cx - s * 0.3f, cy + s * 0.3f);
            line.lineTo (cx + s * 0.3f, cy + s * 0.3f);
            line.lineTo (cx + s * 0.3f, cy + s * 0.08f);
            juce::PathStrokeType (s * 0.085f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (p, line);
            return p;
        }
    }

    return p;
}
} // namespace IlanaIcons

// A header key with an icon (and optional short text) instead of a word.
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, IlanaIcons::Icon iconIn, const juce::String& tooltip)
        : juce::Button (name), icon (iconIn)
    {
        setTooltip (tooltip);
    }

    void setText (const juce::String& newText)
    {
        text = newText;
        repaint();
    }

    // The one key that should read as the header's main action (SAVE): a
    // tinted accent fill and edge, white icon and text.
    void setEmphasis (bool shouldEmphasise)
    {
        emphasis = shouldEmphasise;
        repaint();
    }

    // No key background: the button sits inside another display (the
    // preset display's prev / next segments).
    void setFlat (bool shouldBeFlat)
    {
        flat = shouldBeFlat;
        repaint();
    }

    void setIconColour (std::optional<juce::Colour> colour)
    {
        iconColour = colour;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto on = getToggleState();
        auto background = findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);

        if (! flat)
            getLookAndFeel().drawButtonBackground (g, *this, background, isHighlighted, isDown);
        else if (isHighlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (isDown ? 0.1f : 0.05f));
            g.fillRect (getLocalBounds());
        }

        // SAVE, the header's one main action: the accent solid with dark text.
        if (emphasis && isEnabled())
        {
            const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (IlanaTheme::accent().brighter (isDown ? 0.0f : (isHighlighted ? 0.12f : 0.0f)).withMultipliedBrightness (isDown ? 0.9f : 1.0f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }

        auto area = getLocalBounds().toFloat().reduced (3.0f);
        const auto enabled = isEnabled();
        auto colour = iconColour.value_or (emphasis ? juce::Colour (0xff1a0b06) : on ? juce::Colours::white : IlanaTheme::Ui::text2);

        if (! enabled)
            colour = colour.withAlpha (0.22f);
        else if (isHighlighted)
            colour = colour.interpolatedWith (juce::Colours::white, 0.6f);

        g.setColour (colour);

        if (text.isNotEmpty())
        {
            const auto iconArea = area.removeFromLeft (area.getHeight());
            g.fillPath (IlanaIcons::make (icon, iconArea));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f));
            g.drawText (text, area.withTrimmedRight (2.0f), juce::Justification::centred);
        }
        else
        {
            // 15 px of icon in a 30 px key (the mockup's size); the flat prev / next arrows keep their 24.
            const auto side = flat ? juce::jmin (area.getWidth(), area.getHeight()) : juce::jmin (area.getWidth(), area.getHeight(), 21.0f);
            g.fillPath (IlanaIcons::make (icon, area.withSizeKeepingCentre (side, side)));
        }
    }

private:
    IlanaIcons::Icon icon;
    juce::String text;
    bool emphasis = false, flat = false;
    std::optional<juce::Colour> iconColour;
};

// SCOPE, KEYBOARD and ? in the tab bar: a small ghost button (clear fill,
// a thin rim, 6 px corners); lit with the accent while it is switched on.
class GhostButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto on = getToggleState();

        if (on)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.16f));
            g.fillRoundedRectangle (bounds, 6.0f);
        }
        else if (isHighlighted || isDown)
        {
            g.setColour (juce::Colours::white.withAlpha (isDown ? 0.1f : 0.05f));
            g.fillRoundedRectangle (bounds, 6.0f);
        }

        g.setColour (on ? IlanaTheme::accent().withAlpha (0.8f) : IlanaTheme::Ui::line.brighter (isHighlighted ? 0.3f : 0.0f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        g.setColour (on || isHighlighted ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }

    // The width its text needs.
    int getIdealWidth() const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f)), getButtonText()) + 20;
    }
};

// The preset name as the centrepiece of the header: an inset display with
// the category above the name. Clicking it opens the browser.
class PresetDisplay : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PresetDisplay() { setTooltip ("Preset\nClick to browse presets.  Use the arrows to step through them."); }

    std::function<void()> onClick;

    void setPreset (const juce::String& newName, const juce::String& newCategory, bool favourite, bool modified)
    {
        if (newName == name && newCategory == category && favourite == isFavourite && modified == isModified)
            return;

        name = newName;
        category = newCategory;
        isFavourite = favourite;
        isModified = modified;
        repaint();
    }

    // A word in place of the category for a moment ("SAVED").
    void showNotice (const juce::String& text)
    {
        notice = text;
        repaint();
        const auto shown = ++noticeCount;
        juce::Timer::callAfterDelay (1800, [safeThis = juce::Component::SafePointer<PresetDisplay> (this), shown]
        {
            if (safeThis != nullptr && safeThis->noticeCount == shown)
            {
                safeThis->notice.clear();
                safeThis->repaint();
            }
        });
    }

    juce::String getNotice() const { return notice; }
    bool isShowingModified() const { return isModified; }

    void setFlash (float amount)
    {
        if (std::abs (amount - flash) > 0.005f)
        {
            flash = amount;
            repaint();
        }
    }

    // The two 28 px segments at the ends hold the prev / next buttons (the
    // editor lays them over this display); the live scope sits right of the
    // name, faint.
    static constexpr int arrowWidth = 28;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        // (The field itself is painted by the editor, under the live wave.)
        g.setColour (hover ? IlanaTheme::accent().withAlpha (0.6f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        // A preset load flashes the field with a glow that fades out.
        if (flash > 0.01f)
        {
            IlanaTheme::paintGlow (g, bounds.reduced (1.0f), 10.0f, IlanaTheme::accent(), 2.0f * flash);
            g.setColour (IlanaTheme::accent().withAlpha (0.12f * flash));
            g.fillRoundedRectangle (bounds, 10.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.7f * flash));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.2f);
        }

        // The rules between the arrows and the name.
        g.setColour (IlanaTheme::Ui::line.darker (0.2f));
        g.fillRect (juce::Rectangle<float> ((float) arrowWidth, 1.0f, 1.0f, bounds.getHeight() - 1.0f));
        g.fillRect (juce::Rectangle<float> ((float) (getWidth() - arrowWidth - 1), 1.0f, 1.0f, bounds.getHeight() - 1.0f));

        auto text = getLocalBounds().withTrimmedLeft (arrowWidth + 12).withTrimmedRight (arrowWidth + 8).reduced (0, 3);

        // Init's category is its own name: show it as the start-up label does.
        const auto categoryText = category.isNotEmpty() && ! category.equalsIgnoreCase ("Init") ? category.toUpperCase()
                                                                                                : name.isEmpty() || name.equalsIgnoreCase ("Init") ? juce::String ("NEW PATCH") // (I14-7: not "PRESET" over Init)
                                                                                                                                                      : juce::String ("PRESET");
        // "BASS · EDITED": the category and the edited mark in the accent, one line.
        auto categoryRow = text.removeFromTop (14);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
        g.setColour (notice.isNotEmpty() ? IlanaTheme::accent() : (isModified ? IlanaTheme::accent() : IlanaTheme::Ui::text3));
        g.drawText (notice.isNotEmpty() ? notice : categoryText + (isModified ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 EDITED")) : juce::String()),
                    categoryRow, juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (nameSize, true));
        const auto nameText = name.isNotEmpty() ? name : juce::String ("Init");
        g.drawText (nameText, text, juce::Justification::centredLeft, true);

        if (isFavourite)
        {
            const auto nameWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (nameSize, true)), nameText);
            const auto starX = juce::jmin ((float) text.getRight() - 10.0f, (float) text.getX() + nameWidth + 12.0f);
            g.setColour (juce::Colour (0xffffd447));
            g.fillPath (IlanaIcons::make (IlanaIcons::Icon::Star,
                                          juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ starX, (float) text.getCentreY() })));
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked() && onClick != nullptr)
            onClick();
    }

private:
    // The preset name is the header's centrepiece: a size up from the type
    // scale's "large".
    static constexpr float nameSize = 17.0f;
    juce::String name, category;
    bool isFavourite = false;
    bool isModified = false;
    juce::String notice;
    int noticeCount = 0;
    bool hover = false;
    float flash = 0.0f;
};

// The header's compare switch (A/B stays in the header, a settled
// decision), drawn compact as "A | B" with the version playing lit. The
// button's text is the side shown ("A" or "B"); a click flips it.
class ABButton : public juce::TextButton
{
public:
    ABButton() : juce::TextButton ("A") {}

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.3f);
        g.setColour (IlanaTheme::Ui::raised.brighter (isDown ? 0.12f : (isHighlighted ? 0.06f : 0.0f)));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

        const auto onB = getButtonText() == "B";
        auto left = bounds.reduced (3.0f);
        const auto right = left.removeFromRight (left.getWidth() * 0.5f);
        const auto lit = onB ? right : left;
        g.setColour (IlanaTheme::accent().withAlpha (0.85f));
        g.fillRoundedRectangle (lit.reduced (1.0f, 0.0f), radius - 1.5f);

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.setColour (onB ? IlanaTheme::Ui::text2 : juce::Colours::white);
        g.drawText ("A", left.toNearestInt(), juce::Justification::centred);
        g.setColour (onB ? juce::Colours::white : IlanaTheme::Ui::text2);
        g.drawText ("B", right.toNearestInt(), juce::Justification::centred);
    }
};

