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
inline juce::Colour filter (int index) { return index == 0 ? juce::Colour (0xffc86bff) : juce::Colour (0xff8f9dff); }
inline juce::Colour west() { return juce::Colour (0xffbfe35a); }
inline juce::Colour body() { return IlanaTheme::accent(); }
}

// The filter models by what they are (UI review 6, V5-17, S5-12, S6-20):
// the categories and families shared by the FILTER page's type picker and
// PLAY's TYPE menu, the names they show and each model's small response
// drawing. Display only: the saved indices and the parameter's own choice
// strings are unchanged.
struct FilterTypes
{
    // One per FilterType, in index order: the short name on PLAY's TYPE box.
    // The Airwindows models are named by what they do (I6-26), their
    // Airwindows names kept in displayName().
    static juce::StringArray shortNames()
    {
        return { "LP", "BP", "HP", "NOTCH", "LADDER", "LAD HP", "DIODE", "MS-20", "COMB +", "COMB -", "FORMANT", "MORPH",
                 "LAD BP", "DRIVE", "SEM", "OTA LP", "OTA BP", "MS HP", "STEINER", "PHASER", "DAMPED", "MIX",
                 "VOWEL", "TALK", "TWIN",
                 "303", "MOOG", "V MORPH", "BODY",
                 "SMOOTH LP", "SMOOTH HP", "SMOOTH BP", "MELT LP", "GRIT LP", "RESO LP", "ROUND LP", "HARD LP", "SLOPE LP",
                 "DISPERSE" };
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
        g.drawFittedText (FilterTypes::displayName (current), inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);

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
            .showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
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

// 12 / 24 dB switch.  Hidden by the page for types without a slope.
class SlopeSwitch : public ParamBoundComponent
{
public:
    SlopeSwitch (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour colourIn)
        : ParamBoundComponent (state, id), colour (colourIn)
    {
        setTooltip ("Slope\n12 dB is gentler and brighter; 24 dB cuts harder.");
    }

    void paint (juce::Graphics& g) override
    {
        // Two choice pills, like every other selector.
        const auto bounds = getLocalBounds().toFloat();

        for (int option = 0; option < 2; ++option)
        {
            const auto half = bounds.withWidth (bounds.getWidth() * 0.5f).withX (bounds.getX() + bounds.getWidth() * 0.5f * (float) option);
            IlanaTheme::paintPill (g, half.reduced (2.0f, 1.0f), option == 0 ? "12 dB" : "24 dB", colour, option == current);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        setValue (event.position.x < (float) getWidth() * 0.5f ? 0 : 1);
    }

private:
    juce::Colour colour;
};

// The voice's signal path, drawn live and clickable: oscillators on the
// left wire into the filters (serial or parallel), then WEST, the body and
// out. Click an oscillator to choose where it goes, the SERIAL/PARALLEL
// badge to switch, WEST or BODY to toggle them. An oscillator whose OUT is
// off (an FM modulator) is not heard: it wires into the operators it
// modulates, dashed (UI review 6, S6-16). WEST sits after the filters or in
// Filter 2's place, as its PLACE says (I6-16).
class SignalFlow : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    explicit SignalFlow (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setRepaintsOnMouseActivity (true);
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 6.0f);
        const auto layout = computeLayout();
        const auto parallel = read ("filters_parallel") > 0.5f;
        const auto resOn = read ("res_on") > 0.5f;
        const auto westOn = read ("west_on") > 0.5f;
        const auto mouse = getMouseXYRelative().toFloat();
        const auto over = isMouseOver();
        const auto count = (int) layout.sources.size();
        // Where Filter 2's wires land: WEST when it takes Filter 2's place.
        const auto f2Target = layout.westReplaces ? layout.west : layout.f2;

        // Wires from each source: into the filters, or (an FM modulator)
        // into its carriers.
        for (int row = 0; row < count; ++row)
        {
            const auto osc = layout.sources[(size_t) row];
            const auto on = isSourceOn (osc);
            const auto colour = oscColour (osc).withAlpha (on ? 0.85f : 0.2f);
            const auto box = layout.osc[(size_t) row];
            const auto start = box.getCentre().withX (box.getRight());

            if (const auto carriers = fmTargets (osc); ! isHeard (osc))
            {
                for (size_t k = 0; k < carriers.size(); ++k)
                    for (int target = 0; target < count; ++target)
                        if (layout.sources[(size_t) target] == carriers[k])
                            drawFmWire (g, box, layout.osc[(size_t) target], colour, 12.0f + 6.0f * (float) k);
                continue;
            }

            const auto route = (int) read (routeId (osc));
            const auto yOffset = ((float) row - 0.5f * (float) (count - 1)) * 3.0f;
            const auto wireTo = [&] (juce::Rectangle<float> target)
            {
                drawWire (g, start, { target.getX(), target.getCentreY() + yOffset }, colour);
            };

            if (route == 3)
                wireTo (layout.bypass);
            else if (route == 2)
                wireTo (f2Target);
            else if (route == 1 || ! parallel)
                wireTo (layout.f1);
            else
            {
                wireTo (layout.f1);
                wireTo (f2Target);
            }
        }

        // The chain: the filters, then WEST (after them), the body, the
        // pedal strings and soundboard when on, and out.
        const auto chainColour = juce::Colours::white.withAlpha (0.5f);
        const auto afterFilters = layout.westAfter ? layout.west : layout.res;
        const auto link = [&g] (juce::Rectangle<float> from, juce::Rectangle<float> to, juce::Colour colour)
        {
            drawWire (g, { from.getRight(), from.getCentreY() }, { to.getX(), to.getCentreY() }, colour);
        };

        if (parallel)
        {
            link (layout.f1, afterFilters, chainColour);
            link (f2Target, afterFilters, chainColour);
        }
        else
        {
            link (layout.f1, f2Target, chainColour);
            link (f2Target, afterFilters, chainColour);
        }

        link (layout.bypass, afterFilters, chainColour.withAlpha (0.25f));
        if (layout.westAfter)
            link (layout.west, layout.res, chainColour.withAlpha (westOn ? 0.5f : 0.3f));
        if (! layout.post.isEmpty())
        {
            link (layout.res, layout.post, chainColour);
            link (layout.post, layout.out, chainColour);
        }
        else
            link (layout.res, layout.out, chainColour);

        // Blocks.
        for (int row = 0; row < count; ++row)
        {
            const auto osc = layout.sources[(size_t) row];
            // A modulator (OUT off) says so on its box; its dashed loop shows what it modulates.
            const auto name = osc == subNoise ? juce::String ("SUB+N")
                                              : "OSC " + juce::String (osc + 1) + (isHeard (osc) ? juce::String() : juce::String (" FM"));
            drawBlock (g, layout.osc[(size_t) row], name, oscColour (osc),
                       isSourceOn (osc) && (isHeard (osc) || ! fmTargets (osc).empty()), over && layout.osc[(size_t) row].contains (mouse));
        }

        drawBlock (g, layout.f1, "F1", FilterColours::filter (0), true, false);
        if (! layout.westReplaces)
            drawBlock (g, layout.f2, "F2", FilterColours::filter (1), true, false);
        drawBlock (g, layout.west, "WEST", FilterColours::west(), westOn, over && layout.west.contains (mouse));
        drawBlock (g, layout.res, "BODY", FilterColours::body(), resOn, over && layout.res.contains (mouse));
        if (! layout.post.isEmpty())
            drawBlock (g, layout.post, postLabel(), IlanaTheme::Ui::text2, true, false);
        drawBlock (g, layout.out, "OUT", juce::Colours::white, true, false);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("BYPASS", layout.bypass, juce::Justification::centred);

        // Serial / parallel badge.
        const auto badgeHover = over && layout.badge.contains (mouse);
        g.setColour (IlanaTheme::accent().withAlpha (badgeHover ? 0.35f : 0.2f));
        g.fillRoundedRectangle (layout.badge, 8.0f);
        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (parallel ? "PARALLEL" : "SERIAL", layout.badge, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto layout = computeLayout();

        for (size_t row = 0; row < layout.sources.size(); ++row)
        {
            if (layout.osc[row].contains (event.position))
            {
                showRouteMenu (layout.sources[row]);
                return;
            }
        }

        if (layout.badge.contains (event.position))
            toggle ("filters_parallel");
        else if (layout.west.contains (event.position))
            toggle ("west_on");
        else if (layout.res.contains (event.position))
            toggle ("res_on");
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto layout = computeLayout();
        juce::String tip = "Signal flow\nClick an oscillator to route it, the badge to switch serial/parallel, WEST or BODY to switch them on or off.";

        for (size_t row = 0; row < layout.sources.size(); ++row)
            if (layout.osc[row].contains (event.position))
            {
                const auto osc = layout.sources[row];
                const auto name = osc == subNoise ? juce::String ("the sub and noise") : "OSC " + juce::String (osc + 1);
                tip = "Click to choose where " + name + " goes.";

                if (! isHeard (osc))
                {
                    juce::StringArray carriers;
                    for (const auto target : fmTargets (osc))
                        carriers.add ("OSC " + juce::String (target + 1));
                    tip = name + "'s OUT is off: it isn't heard"
                          + (carriers.isEmpty() ? juce::String (" (and modulates nothing).") : ", it frequency-modulates " + carriers.joinIntoString (" and ") + ".");
                }
            }

        if (layout.west.contains (event.position))
            tip = juce::String ("WEST: wavefolder into a low-pass gate, ") + (layout.westReplaces ? "in Filter 2's place" : "after the filters")
                  + ". Click to switch it " + (read ("west_on") > 0.5f ? "off." : "on.");
        else if (layout.res.contains (event.position))
            tip = juce::String ("BODY: the resonant body. Click to switch it ") + (read ("res_on") > 0.5f ? "off." : "on.");
        else if (! layout.post.isEmpty() && layout.post.contains (event.position))
            tip = "After the voices: " + juce::String (read ("sym_on") > 0.5f ? "the sympathetic strings" : "")
                  + (read ("sym_on") > 0.5f && read ("sb_on") > 0.5f ? " and " : "") + (read ("sb_on") > 0.5f ? "the soundboard" : "")
                  + " (OSC > PHYSICAL).";

        setTooltip (tip);
    }

private:
    // Sources are the oscillators the patch has added, then the sub + noise.
    static constexpr int subNoise = OscillatorIds::count;

    struct Layout
    {
        std::vector<int> sources;
        std::vector<juce::Rectangle<float>> osc;
        juce::Rectangle<float> f1, f2, west, res, post, out, bypass, badge;
        bool westAfter = true, westReplaces = false;
    };

    Layout computeLayout() const
    {
        Layout layout;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                layout.sources.push_back (osc);

        layout.sources.push_back (subNoise);
        const auto count = (int) layout.sources.size();
        layout.westReplaces = juce::roundToInt (read ("west_pos")) == 1;
        layout.westAfter = ! layout.westReplaces;
        const auto hasPost = read ("sym_on") > 0.5f || read ("sb_on") > 0.5f;

        auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
        const auto blockHeight = juce::jmin (24.0f, area.getHeight() / 4.8f);
        const auto sourceHeight = juce::jmin (22.0f, area.getHeight() / ((float) juce::jmax (4, count) * 1.25f));
        const auto rowGap = count > 1 ? (area.getHeight() - sourceHeight * (float) count) / (float) (count - 1) : 0.0f;

        auto oscColumn = area.removeFromLeft (juce::jmin (70.0f, area.getWidth() * 0.18f));

        for (int row = 0; row < count; ++row)
            layout.osc.push_back (oscColumn.withHeight (sourceHeight).withY (oscColumn.getY() + (float) row * (sourceHeight + rowGap)));

        // Room for the FM loops and the wires' fan-in.
        area.removeFromLeft (34.0f);

        // The blocks after the filters, right to left, sized to the width.
        const auto blocksAfter = 2.4f + (layout.westAfter ? 1.0f : 0.0f) + (hasPost ? 1.3f : 0.0f);
        const auto blockWidth = juce::jlimit (34.0f, 52.0f, area.getWidth() / (blocksAfter + 2.6f));
        const auto gap = juce::jlimit (10.0f, 18.0f, blockWidth * 0.35f);
        const auto centreY = area.getCentreY();
        const auto place = [&] (float width)
        {
            return area.removeFromRight (width).withSizeKeepingCentre (width, blockHeight).withY (centreY - blockHeight * 0.5f);
        };

        layout.out = place (blockWidth * 0.9f);
        area.removeFromRight (gap);
        if (hasPost)
        {
            layout.post = place (blockWidth * 1.45f);
            area.removeFromRight (gap);
        }
        layout.res = place (blockWidth);
        area.removeFromRight (gap);
        if (layout.westAfter)
        {
            layout.west = place (blockWidth);
            area.removeFromRight (gap);
        }
        area.removeFromRight (gap * 0.6f);

        const auto parallel = read ("filters_parallel") > 0.5f;
        const auto filterWidth = juce::jmin (blockWidth * 1.15f, area.getWidth() * 0.42f);
        const auto top = area.getY();
        const auto middle = centreY - blockHeight * 0.5f;
        const auto bottom = area.getBottom() - blockHeight;

        if (parallel)
        {
            const auto x = area.getCentreX() - filterWidth * 0.5f;
            layout.f1 = { x, top, filterWidth, blockHeight };
            layout.f2 = { x, middle, filterWidth, blockHeight };
            layout.badge = juce::Rectangle<float> (x - 6.0f, (top + blockHeight + middle) * 0.5f - 8.0f, filterWidth + 12.0f, 16.0f);
        }
        else
        {
            layout.f1 = { area.getX(), middle, filterWidth, blockHeight };
            layout.f2 = { area.getRight() - filterWidth, middle, filterWidth, blockHeight };
            layout.badge = juce::Rectangle<float> (area.getCentreX() - 34.0f, top + 2.0f, 68.0f, 16.0f);
        }

        if (layout.westReplaces)
            layout.west = layout.f2;

        layout.bypass = juce::Rectangle<float> (area.getCentreX() - 30.0f, bottom, 60.0f, blockHeight);
        return layout;
    }

    juce::String postLabel() const
    {
        const auto strings = read ("sym_on") > 0.5f;
        const auto board = read ("sb_on") > 0.5f;
        return strings && board ? "STR+BRD" : (strings ? "STRINGS" : "BOARD");
    }

    bool isSourceOn (int osc) const
    {
        return osc == subNoise ? (read ("subosc_on") > 0.5f || read ("noise_level") > 0.001f)
                               : read (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on") > 0.5f;
    }

    // Heard: the sub + noise always, an oscillator while its OUT is on.
    bool isHeard (int osc) const
    {
        if (osc == subNoise)
            return true;
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_out";
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value == nullptr || value->load() > 0.5f;
    }

    // The shown oscillators this one frequency-modulates.
    std::vector<int> fmTargets (int osc) const
    {
        std::vector<int> targets;
        if (osc == subNoise)
            return targets;
        for (int target = 0; target < OscillatorIds::count; ++target)
            if (target != osc && processorRef.isOscillatorShown (target)
                && read (IlanaSynthAudioProcessor::fmRouteId (osc, target)) > 0.001f)
                targets.push_back (target);
        return targets;
    }

    static juce::Colour oscColour (int osc)
    {
        // OSC 1-6 match the FM page; the sub + noise is neutral.
        return osc >= subNoise ? IlanaTheme::Ui::text2 : IlanaTheme::oscColour (osc);
    }

    static juce::String routeId (int osc)
    {
        return osc == subNoise ? juce::String ("subosc_route") : juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_route";
    }

    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void toggle (const char* id)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
        {
            processorRef.performEdit (parameter->getName (64), [parameter]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
                parameter->endChangeGesture();
            });
        }

        repaint();
    }

    void showRouteMenu (int osc)
    {
        auto* parameter = processorRef.apvts.getParameter (routeId (osc));

        if (parameter == nullptr)
            return;

        const auto current = (int) read (routeId (osc));
        juce::PopupMenu menu;
        const juce::StringArray options { "Default (follows serial/parallel)", "Filter 1 only", "Filter 2 only", "Bypass the filters" };

        for (int i = 0; i < options.size(); ++i)
            menu.addItem (i + 1, options[i], true, i == current);

        juce::Component::SafePointer<SignalFlow> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis, parameter] (int result)
                            {
                                if (result <= 0 || safeThis == nullptr)
                                    return;

                                safeThis->processorRef.performEdit (parameter->getName (64), [parameter, result]
                                {
                                    parameter->beginChangeGesture();
                                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                    parameter->endChangeGesture();
                                });
                                safeThis->repaint();
                            });
    }

    static void drawWire (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour)
    {
        juce::Path wire;
        const auto bend = (to.x - from.x) * 0.5f;
        wire.startNewSubPath (from);
        wire.cubicTo (from.x + bend, from.y, to.x - bend, to.y, to.x, to.y);
        g.setColour (colour);
        g.strokePath (wire, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A modulator's dashed loop from its box into a carrier's, out to the
    // right of the column, with an arrowhead.
    static void drawFmWire (juce::Graphics& g, juce::Rectangle<float> from, juce::Rectangle<float> to, juce::Colour colour, float reach)
    {
        const juce::Point<float> a (from.getRight(), from.getCentreY());
        const juce::Point<float> b (to.getRight(), to.getCentreY());
        const auto x = juce::jmax (a.x, b.x) + reach;
        juce::Path loop;
        loop.startNewSubPath (a);
        loop.cubicTo (x, a.y, x, b.y, b.x + 3.0f, b.y);
        juce::Path dashed;
        const float dashes[] { 3.0f, 2.5f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, loop, dashes, 2);
        g.setColour (colour);
        g.fillPath (dashed);

        juce::Path head;
        head.addTriangle (b.x + 1.0f, b.y, b.x + 6.0f, b.y - 3.0f, b.x + 6.0f, b.y + 3.0f);
        g.fillPath (head);
    }

    static void drawBlock (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& text, juce::Colour colour,
                           bool lit, bool hovered)
    {
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (colour.withAlpha (lit ? (hovered ? 1.0f : 0.8f) : 0.25f));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, hovered ? 1.8f : 1.2f);
        g.setColour (lit ? colour : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawFittedText (text, box.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
};

