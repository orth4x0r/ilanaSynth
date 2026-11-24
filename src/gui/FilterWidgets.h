#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/FilterUnit.h"
#include "../dsp/OscillatorIds.h"
#include "IlanaLookAndFeel.h"

// Base for small custom controls bound to one choice/bool parameter.
class ParamBoundComponent : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    ParamBoundComponent (juce::AudioProcessorValueTreeState& state, const juce::String& id)
        : parameter (*state.getParameter (id)),
          attachment (parameter, [this] (float value) { current = juce::roundToInt (value); repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
    }

    // For the UI tests, which find these by type: the bound parameter and the text drawn.
    juce::String getParameterId() const { return parameter.getParameterID(); }
    virtual juce::String getDisplayText() const { return {}; }

protected:
    void setValue (int value)
    {
        if (value != current)
            attachment.setValueAsCompleteGesture ((float) value);
    }

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    int current = 0;
};

// The filter section's colours, one per block wherever it is drawn (the
// cards, the response graph, the signal flow): Filter 1, Filter 2, WEST
// (its own, not the accent: UI review 6, I6-16) and the BODY (the accent,
// as its card).
namespace FilterColours
{
inline juce::Colour filter (int index) { return IlanaTheme::envIdentityColour (index == 0 ? 1 : 2); }
inline juce::Colour west()
{
    const auto picked = SectionColours::pickedFor (SectionColours::westKey);
    return juce::Colour (picked != 0 ? picked : 0xffbfe35a);
}
inline juce::Colour body() { return IlanaTheme::physicalColour(); } // STRINGS and BODY share the wood colour

// A module switched off (WEST, BODY, an FX card) draws its controls at this
// alpha, as PLAY's switched-off oscillators do: plainly off, still editable
// (UI review 7, V7-34, S7-23). Lighter than IlanaTheme::dimmedAlpha, which
// marks one control with no effect in a module that is on.
inline constexpr float offAlpha = 0.55f;
// WEST and BODY keep their full-height cards while off: their controls dim less than the
// default (they stay readable) and the header says what the switch does (N16-8).
inline constexpr float cardOffAlpha = 0.72f;
}

// The filter models by what they are (UI review 6, V5-17, S5-12, S6-20):
// the categories and families shared by the FILTER page's type picker and
// PLAY's TYPE menu, the names they show and each model's small response
// drawing. Display only: the saved indices and the parameter's own choice
// strings are unchanged.
struct FilterTypes
{
    // One per FilterType, in index order: the name on PLAY's TYPE box, the
    // FILTER page's own name (I7-40: "LP" on one page, "Low Pass" on the
    // other read as two things) without an Airwindows model's plugin name.
    static juce::StringArray shortNames()
    {
        juce::StringArray names;
        for (int type = 0; type < FilterType::Count; ++type)
            names.add (displayName (type).upToFirstOccurrenceOf (" (", false, false));
        return names;
    }

    // The name in the picker and its menu: the model's full name; an
    // Airwindows model by its job, then the plugin's own name.
    static juce::String displayName (int type)
    {
        using namespace FilterType;
        switch (type)
        {
            case AwZLow:    return "Smooth LP (Z)";
            case AwZHigh:   return "Smooth HP (Z)";
            case AwZBand:   return "Smooth BP (Z)";
            case AwAcid:    return "Melt LP (Z Acid)";
            case AwXLow:    return "Grit LP (X)";
            case AwYNotLow: return "Reso LP (YNot)";
            case AwHolt:    return "Round LP (Holt)";
            case AwAngle:   return "Hard LP (Angle)";
            case AwPear:    return "Slope LP (Pear)";
            default:        return FilterType::getNames()[juce::jlimit (0, FilterType::Count - 1, type)];
        }
    }

    static int getNumPages() { return numPages; }
    static juce::String getPageName (int p) { return pageNames()[juce::jlimit (0, numPages - 1, p)]; }
    static std::vector<std::pair<juce::String, std::vector<int>>> getFamilies (int p)
    {
        std::vector<std::pair<juce::String, std::vector<int>>> families;
        for (const auto& group : pages()[(size_t) juce::jlimit (0, numPages - 1, p)])
            families.push_back ({ group.name, group.types });
        return families;
    }

    static int pageOf (int type)
    {
        for (int p = 0; p < numPages; ++p)
            for (const auto& group : pages()[(size_t) p])
                if (std::find (group.types.begin(), group.types.end(), type) != group.types.end())
                    return p;
        return 0;
    }

    // Every type once, in the menu's order (the picker's arrows walk it).
    static const std::vector<int>& order()
    {
        static const std::vector<int> list = []
        {
            std::vector<int> all;
            for (const auto& page : pages())
                for (const auto& group : page)
                    all.insert (all.end(), group.types.begin(), group.types.end());
            return all;
        }();
        return list;
    }

    // The type `step` places along order() from `type` (wrapping).
    static int stepFrom (int type, int step)
    {
        const auto& list = order();
        const auto it = std::find (list.begin(), list.end(), type);
        const auto index = it == list.end() ? 0 : (int) (it - list.begin());
        const auto count = (int) list.size();
        return list[(size_t) (((index + step) % count + count) % count)];
    }

    // The menu: a submenu per category, a section per family, each model
    // with its response drawn beside its name. Item ids are type + 1.
    static juce::PopupMenu buildMenu (int current, juce::Colour colour)
    {
        juce::PopupMenu menu;
        for (int p = 0; p < numPages; ++p)
        {
            juce::PopupMenu pageMenu;
            auto holdsCurrent = false;
            for (const auto& group : pages()[(size_t) p])
            {
                pageMenu.addSectionHeader (group.name);
                for (const auto type : group.types)
                {
                    auto drawable = std::make_unique<juce::DrawablePath>();
                    drawable->setPath (curvePath (type, { 0.0f, 0.0f, 30.0f, 18.0f }));
                    drawable->setFill (juce::FillType (juce::Colours::transparentBlack));
                    drawable->setStrokeFill (type == current ? colour : juce::Colours::white.withAlpha (0.55f));
                    drawable->setStrokeThickness (1.6f);
                    pageMenu.addItem (juce::PopupMenu::Item (displayName (type)).setID (type + 1).setTicked (type == current)
                                          .setImage (std::move (drawable)));
                    holdsCurrent = holdsCurrent || type == current;
                }
            }
            menu.addSubMenu (getPageName (p), pageMenu, true, nullptr, holdsCurrent);
        }
        return menu;
    }

    // A model's basic response, four octaves either side of its cutoff.
    static juce::Path curvePath (int type, juce::Rectangle<float> area)
    {
        juce::Path path;
        constexpr int points = 32;
        for (int i = 0; i <= points; ++i)
        {
            const auto ratio = std::exp2 (-4.0 + 8.0 * (double) i / (double) points);
            const auto response = FilterType::response (type, false, 0.55, { 0.0, ratio }, 0.5, 1000.0);
            const auto db = juce::jlimit (-30.0, 18.0, 20.0 * std::log10 (std::abs (response) + 1.0e-9));
            const auto x = area.getX() + area.getWidth() * (float) i / (float) points;
            const auto y = juce::jmap ((float) db, -30.0f, 18.0f, area.getBottom(), area.getY());
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        return path;
    }

private:
    struct Group
    {
        const char* name;
        std::vector<int> types;
    };

    // Five categories, the analogue models together under ANALOG (S6-20).
    // Every type is in exactly one family; indices are unchanged.
    static constexpr int numPages = 5;

    static const juce::StringArray& pageNames()
    {
        static const juce::StringArray names { "BASIC", "ANALOG", "VOICE", "COMB & PHASE", "AIRWINDOWS" };
        return names;
    }

    static const std::array<std::vector<Group>, numPages>& pages()
    {
        using namespace FilterType;
        static const std::array<std::vector<Group>, numPages> list {
            std::vector<Group> { { "PASS", { LowPass, BandPass, HighPass, Notch } },
                                 { "SWEEP", { Morph, TwinPeak } } },
            std::vector<Group> { { "LADDER", { LadderLow, LadderHigh, LadderBand, LadderDrive, MoogDrive, Acid303 } },
                                 { "DIODE / MS-20", { DiodeLow, Ms20Low, Ms20High } },
                                 { "OTA / STATE VARIABLE", { OtaLow, OtaBand, Sem, Steiner } } },
            std::vector<Group> { { "FORMANT", { Formant, VowelBank, Talking, VowelMorph } } },
            std::vector<Group> { { "COMB", { CombPlus, CombMinus, CombDamped, CombMorph, CombBody } },
                                 { "PHASE", { PhaserNotch, Disperser } } },
            std::vector<Group> { { "CLEAN", { AwZLow, AwZHigh, AwZBand } },
                                 { "COLOUR", { AwAcid, AwXLow, AwYNotLow, AwHolt, AwAngle, AwPear } } }
        };
        return list;
    }
};

// (PLAY's TYPE menu still calls the catalogue by the old grid's name.)
using FilterTypeGrid = FilterTypes;

// The filter type as one compact picker (UI review 6, V5-17, S5-12, S6-20;
// it replaces a 12-button grid per filter): the model's response and name,
// a menu of categories on click, and arrows that step through the list.
class FilterTypePicker : public ParamBoundComponent
{
public:
    FilterTypePicker (IlanaSynthAudioProcessor& p, const juce::String& id, juce::Colour colourIn, juce::String undoNameIn)
        : ParamBoundComponent (p.apvts, id), processorRef (p), colour (colourIn), undoName (std::move (undoNameIn))
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // Width for the arrows, the drawing and the longest name.
    static constexpr int idealWidth = 236;

    void paint (juce::Graphics& g) override
    {
        const auto mouse = getMouseXYRelative().toFloat();
        const auto over = isMouseOver();
        const auto box = boxBounds();

        // The box: a raised field like the menus', tinted while hovered.
        g.setColour (juce::Colours::white.withAlpha (over && box.contains (mouse) ? 0.08f : 0.045f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (colour.withAlpha (0.55f));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);

        auto inner = box.reduced (8.0f, 3.0f);
        const auto curveArea = inner.removeFromLeft (28.0f).reduced (0.0f, 2.0f);
        g.setColour (colour);
        g.strokePath (FilterTypes::curvePath (current, curveArea), juce::PathStrokeType (1.5f));
        inner.removeFromLeft (7.0f);

        // The chevron at the right.
        const auto chevronX = inner.getRight() - 9.0f;
        const auto chevronY = inner.getCentreY();
        juce::Path chevron;
        chevron.startNewSubPath (chevronX, chevronY - 2.0f);
        chevron.lineTo (chevronX + 4.0f, chevronY + 2.0f);
        chevron.lineTo (chevronX + 8.0f, chevronY - 2.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        inner.removeFromRight (14.0f);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        IlanaTheme::drawFitted (g, FilterTypes::displayName (current), inner.toNearestInt(), juce::Justification::centredLeft, 1);

        for (const auto direction : { -1, 1 })
        {
            const auto arrow = arrowBounds (direction);
            const auto hovered = over && arrow.contains (mouse);
            g.setColour (juce::Colours::white.withAlpha (hovered ? 0.1f : 0.03f));
            g.fillRoundedRectangle (arrow, 5.0f);
            const auto c = arrow.getCentre();
            juce::Path glyph;
            glyph.startNewSubPath (c.x + 2.0f * (float) direction * -1.0f, c.y - 4.0f);
            glyph.lineTo (c.x + 2.0f * (float) direction, c.y);
            glyph.lineTo (c.x + 2.0f * (float) direction * -1.0f, c.y + 4.0f);
            g.setColour (juce::Colours::white.withAlpha (hovered ? 0.95f : 0.6f));
            g.strokePath (glyph, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (arrowBounds (-1).contains (event.position))
            choose (FilterTypes::stepFrom (current, -1));
        else if (arrowBounds (1).contains (event.position))
            choose (FilterTypes::stepFrom (current, 1));
        else
            showMenu();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (arrowBounds (-1).contains (event.position))
            setTooltip ("Previous filter type");
        else if (arrowBounds (1).contains (event.position))
            setTooltip ("Next filter type");
        else
            setTooltip (FilterTypes::displayName (current) + " (" + FilterTypes::getPageName (FilterTypes::pageOf (current))
                        + ")\nClick for every model, by category. The arrows step through them.");
    }

    void showMenu()
    {
        juce::Component::SafePointer<FilterTypePicker> safeThis (this);
        FilterTypes::buildMenu (current, colour)
            .showMenuAsync (IlanaTheme::menuOptions (this).withMinimumWidth (getWidth()),
                            [safeThis] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->choose (result - 1);
                            });
    }

    int getType() const { return current; }

private:
    juce::Rectangle<float> arrowBounds (int direction) const
    {
        const auto bounds = getLocalBounds().toFloat();
        return direction < 0 ? bounds.withWidth (22.0f) : bounds.withTrimmedLeft (bounds.getWidth() - 22.0f);
    }

    juce::Rectangle<float> boxBounds() const { return getLocalBounds().toFloat().reduced (25.0f, 0.0f); }

    void choose (int type)
    {
        if (type == current)
            return;

        processorRef.performEdit (undoName, [this, type] { setValue (type); });
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    juce::String undoName;
};

// The filter categories as a row of pills (the FX rack's FILTER card): the
// current model's category lit; a click lists that category's models by
// family, each with its response, as the picker's menu.
class FilterCategoryTabs : public ParamBoundComponent
{
public:
    FilterCategoryTabs (IlanaSynthAudioProcessor& p, const juce::String& id, juce::Colour colourIn, juce::String undoNameIn)
        : ParamBoundComponent (p.apvts, id), processorRef (p), colour (colourIn), undoName (std::move (undoNameIn))
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto lit = FilterTypes::pageOf (current);
        const auto mouse = getMouseXYRelative().toFloat();
        for (int page = 0; page < FilterTypes::getNumPages(); ++page)
        {
            const auto cell = pillBounds (page);
            const auto on = page == lit;
            const auto hovered = isMouseOver() && cell.contains (mouse);
            g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, hovered ? 0.12f : 0.0f));
            g.fillRoundedRectangle (cell, 4.0f);
            if (! on)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);
            }
            g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
            g.setFont (font());
            g.drawText (FilterTypes::getPageName (page), cell, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto page = pageAt (event.position);
        setTooltip (page < 0 ? juce::String() : FilterTypes::getPageName (page) + ": click for its filter models.");
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto page = pageAt (event.position);
        if (page < 0)
            return;
        juce::PopupMenu menu;
        for (const auto& family : FilterTypes::getFamilies (page))
        {
            menu.addSectionHeader (family.first);
            for (const auto type : family.second)
            {
                auto drawable = std::make_unique<juce::DrawablePath>();
                drawable->setPath (FilterTypes::curvePath (type, { 0.0f, 0.0f, 30.0f, 18.0f }));
                drawable->setFill (juce::FillType (juce::Colours::transparentBlack));
                drawable->setStrokeFill (type == current ? colour : juce::Colours::white.withAlpha (0.55f));
                drawable->setStrokeThickness (1.6f);
                menu.addItem (juce::PopupMenu::Item (FilterTypes::displayName (type)).setID (type + 1).setTicked (type == current)
                                  .setImage (std::move (drawable)));
            }
        }
        juce::Component::SafePointer<FilterCategoryTabs> safeThis (this);
        const auto cell = pillBounds (page).toNearestInt();
        menu.showMenuAsync (IlanaTheme::menuOptions (this).withTargetScreenArea (localAreaToGlobal (cell)),
                            [safeThis] (int result)
                            {
                                if (safeThis != nullptr && result > 0 && result - 1 != safeThis->current)
                                    safeThis->processorRef.performEdit (safeThis->undoName, [safeThis, result] { safeThis->setValue (result - 1); });
                            });
    }

private:
    static juce::Font font() { return IlanaTheme::font (IlanaTheme::TextSize::packedBody, true); }

    // Pills as wide as their names, the spare width shared out, so the row fills its bounds.
    juce::Rectangle<float> pillBounds (int page) const
    {
        const auto bounds = getLocalBounds().toFloat();
        constexpr float gap = 3.0f;
        std::array<float, 8> widths {};
        auto total = 0.0f;
        for (int p = 0; p < FilterTypes::getNumPages(); ++p)
        {
            widths[(size_t) p] = juce::GlyphArrangement::getStringWidth (font(), FilterTypes::getPageName (p));
            total += widths[(size_t) p];
        }
        const auto pages = (float) FilterTypes::getNumPages();
        const auto spare = juce::jmax (0.0f, (bounds.getWidth() - gap * (pages - 1.0f) - total) / pages);
        auto x = bounds.getX();
        for (int p = 0; p < page; ++p)
            x += widths[(size_t) p] + spare + gap;
        return { x, bounds.getY(), widths[(size_t) page] + spare, bounds.getHeight() };
    }

    int pageAt (juce::Point<float> position) const
    {
        for (int page = 0; page < FilterTypes::getNumPages(); ++page)
            if (pillBounds (page).contains (position))
                return page;
        return -1;
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    juce::String undoName;
};

// Opens the PATCH tab (set by the editor; the editor hooks keep it per editor).
namespace FilterFlow
{
inline std::function<void()>& openRouting()
{
    static std::function<void()> open;
    return open;
}
} // namespace FilterFlow
