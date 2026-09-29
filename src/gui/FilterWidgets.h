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

// All filter models as a grid of buttons (two pages), each with a small drawing
// of its response, so the type can be picked by eye.
class FilterTypeGrid : public ParamBoundComponent
{
public:
    FilterTypeGrid (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour colourIn)
        : ParamBoundComponent (state, id), colour (colourIn)
    {
        setTooltip ("Filter type\nClick a model.  The drawing shows its basic response.");
        setRepaintsOnMouseActivity (true);
    }

    static juce::StringArray shortNames()
    {
        return { "LP", "BP", "HP", "NOTCH", "LADDER", "LAD HP", "DIODE", "MS-20", "COMB +", "COMB -", "FORMANT", "MORPH",
                 "LAD BP", "DRIVE", "SEM", "OTA LP", "OTA BP", "MS HP", "STEINER", "PHASER", "DAMPED", "MIX",
                 "VOWEL", "TALK", "TWIN" };
    }

    void paint (juce::Graphics& g) override
    {
        followCurrent();
        const auto names = shortNames();
        const auto& groups = pages()[(size_t) page];

        // Family labels over each block, with thin dividers between.
        for (int group = 0; group < (int) groups.size(); ++group)
        {
            const auto block = groupBounds (group);
            const auto& types = groups[(size_t) group].types;
            const auto holdsCurrent = std::find (types.begin(), types.end(), current) != types.end();
            g.setColour (holdsCurrent ? colour.withAlpha (0.9f) : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            auto label = block.withHeight (labelHeight).reduced (3.0f, 0.0f);
            label.setRight (juce::jmin (label.getRight(), pageBounds().getX() - 4.0f));
            g.drawText (groups[(size_t) group].name, label.toNearestInt(), juce::Justification::centredLeft);

            if (group > 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (juce::Rectangle<float> (block.getX() - groupGap * 0.5f - 0.5f, block.getY() + 2.0f,
                                                    1.0f, block.getHeight() - 4.0f));
            }
        }

        // The page switch: the classic twelve, then M8.4's models.
        {
            const auto pill = pageBounds();
            const auto hovered = isMouseOver() && pill.contains (getMouseXYRelative().toFloat());
            g.setColour (colour.withAlpha (hovered ? 0.35f : 0.18f));
            g.fillRoundedRectangle (pill, 5.0f);
            g.setColour (colour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (page == 0 ? "MORE  >" : "<  CLASSIC", pill, juce::Justification::centred);
        }

        for (int group = 0; group < (int) groups.size(); ++group)
        {
            for (auto type : groups[(size_t) group].types)
            {
                const auto cell = cellBounds (type).reduced (2.0f);
                const auto active = type == current;
                const auto hovered = isMouseOver() && cellBounds (type).contains (getMouseXYRelative().toFloat());

                g.setColour (active ? colour.withAlpha (0.2f) : juce::Colours::white.withAlpha (hovered ? 0.07f : 0.03f));
                g.fillRoundedRectangle (cell, 4.0f);
                g.setColour (active ? colour.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.1f));
                g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, active ? 1.3f : 1.0f);

                // Small response drawing on the left, name beside it.
                auto inner = cell.reduced (6.0f, 4.0f);
                const auto curveArea = inner.removeFromLeft (juce::jmin (30.0f, inner.getWidth() * 0.4f));
                inner.removeFromLeft (4.0f);
                paintCurve (g, type, curveArea, active);

                g.setColour (active ? colour : IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, active));
                g.drawFittedText (names[type], inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        followCurrent();
        if (pageBounds().contains (event.position))
        {
            page = 1 - page;
            pageChosen = true;
            repaint();
            return;
        }
        for (const auto& group : pages()[(size_t) page])
            for (auto type : group.types)
                if (cellBounds (type).contains (event.position))
                    setValue (type);
    }

    // Label strip above the cells (the page adds it to the grid's height).
    static constexpr int labelHeight = 13;

    int getPage() const { return page; }
    void setPage (int newPage) { page = juce::jlimit (0, 1, newPage); pageChosen = true; repaint(); }

private:
    struct Group
    {
        const char* name;
        std::vector<int> types;
    };

    // Two pages of families, each a block two cells high. Page 1 is the
    // classic twelve (CLASSIC, CHARACTER, SPECIAL); page 2 holds M8.4's
    // models. Type indices are unchanged.
    static const std::array<std::vector<Group>, 2>& pages()
    {
        using namespace FilterType;
        static const std::array<std::vector<Group>, 2> list {
            std::vector<Group> { { "CLASSIC", { LowPass, BandPass, HighPass, Notch } },
                                 { "CHARACTER", { LadderLow, LadderHigh, DiodeLow, Ms20Low } },
                                 { "SPECIAL", { CombPlus, CombMinus, Formant, Morph } } },
            std::vector<Group> { { "ANALOG", { LadderDrive, LadderBand, OtaLow, OtaBand, Ms20High, Sem } },
                                 { "SHAPES", { Steiner, PhaserNotch, TwinPeak, CombMorph } },
                                 { "VOICE", { VowelBank, Talking, CombDamped } } }
        };
        return list;
    }

    static constexpr float groupGap = 10.0f;

    static int columnsOf (const Group& group) { return ((int) group.types.size() + 1) / 2; }

    // Right edge level with the cells' (they're inset 2 px).
    juce::Rectangle<float> pageBounds() const { return { (float) getWidth() - 72.0f, 0.0f, 70.0f, (float) labelHeight }; }

    juce::Rectangle<float> groupBounds (int group) const
    {
        const auto& groups = pages()[(size_t) page];
        auto columns = 0;
        for (const auto& g : groups)
            columns += columnsOf (g);
        const auto columnWidth = ((float) getWidth() - groupGap * (float) (groups.size() - 1)) / (float) juce::jmax (1, columns);
        auto x = 0.0f;
        for (int i = 0; i < group; ++i)
            x += columnWidth * (float) columnsOf (groups[(size_t) i]) + groupGap;
        return { x, 0.0f, columnWidth * (float) columnsOf (groups[(size_t) group]), (float) getHeight() };
    }

    juce::Rectangle<float> cellBounds (int type) const
    {
        const auto& groups = pages()[(size_t) page];
        for (int group = 0; group < (int) groups.size(); ++group)
        {
            const auto& types = groups[(size_t) group].types;
            const auto it = std::find (types.begin(), types.end(), type);
            if (it == types.end())
                continue;
            const auto index = (int) (it - types.begin());
            const auto columns = columnsOf (groups[(size_t) group]);
            const auto block = groupBounds (group).withTrimmedTop ((float) labelHeight);
            const auto width = block.getWidth() / (float) columns;
            const auto height = block.getHeight() * 0.5f;
            return { block.getX() + (float) (index % columns) * width, block.getY() + (float) (index / columns) * height, width, height };
        }
        return {};
    }

    // Show the page holding the current type, unless the user flipped it.
    void followCurrent()
    {
        if (! pageChosen)
            page = current >= FilterType::LadderBand ? 1 : 0;
    }

    int page = 0;
    bool pageChosen = false;

    void paintCurve (juce::Graphics& g, int type, juce::Rectangle<float> area, bool active) const
    {
        juce::Path path;
        constexpr int points = 32;

        for (int i = 0; i <= points; ++i)
        {
            // Four octaves either side of the cutoff.
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

        g.setColour (active ? colour : juce::Colours::white.withAlpha (0.45f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    juce::Colour colour;
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
// left wire into the filters (serial or parallel), then the resonator and
// out.  Click an oscillator to choose where it goes, the SERIAL/PARALLEL
// badge to switch, and BODY to toggle the body.
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
        const auto mouse = getMouseXYRelative().toFloat();
        const auto over = isMouseOver();
        const auto count = (int) layout.sources.size();

        // Wires from each source.
        for (int row = 0; row < count; ++row)
        {
            const auto osc = layout.sources[(size_t) row];
            const auto on = osc == subNoise ? (read ("subosc_on") > 0.5f || read ("noise_level") > 0.001f)
                                            : read (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on") > 0.5f;
            const auto route = (int) read (routeId (osc));
            const auto colour = oscColour (osc).withAlpha (on ? 0.85f : 0.2f);
            const auto start = layout.osc[(size_t) row].getCentre().withX (layout.osc[(size_t) row].getRight());
            const auto yOffset = ((float) row - 0.5f * (float) (count - 1)) * 3.0f;

            const auto wireTo = [&] (juce::Rectangle<float> target)
            {
                drawWire (g, start, { target.getX(), target.getCentreY() + yOffset }, colour);
            };

            if (route == 3)
                wireTo (layout.bypass);
            else if (route == 2)
                wireTo (layout.f2);
            else if (route == 1 || ! parallel)
                wireTo (layout.f1);
            else
            {
                wireTo (layout.f1);
                wireTo (layout.f2);
            }
        }

        // Filter chain.
        const auto chainColour = juce::Colours::white.withAlpha (0.5f);

        if (parallel)
        {
            drawWire (g, { layout.f1.getRight(), layout.f1.getCentreY() }, { layout.res.getX(), layout.res.getCentreY() }, chainColour);
            drawWire (g, { layout.f2.getRight(), layout.f2.getCentreY() }, { layout.res.getX(), layout.res.getCentreY() }, chainColour);
        }
        else
        {
            drawWire (g, { layout.f1.getRight(), layout.f1.getCentreY() }, { layout.f2.getX(), layout.f2.getCentreY() }, chainColour);
            drawWire (g, { layout.f2.getRight(), layout.f2.getCentreY() }, { layout.res.getX(), layout.res.getCentreY() }, chainColour);
        }

        drawWire (g, { layout.bypass.getRight(), layout.bypass.getCentreY() }, { layout.res.getX(), layout.res.getCentreY() },
                  chainColour.withAlpha (0.25f));
        drawWire (g, { layout.res.getRight(), layout.res.getCentreY() }, { layout.out.getX(), layout.out.getCentreY() }, chainColour);

        // Blocks.
        for (int row = 0; row < count; ++row)
        {
            const auto osc = layout.sources[(size_t) row];
            drawBlock (g, layout.osc[(size_t) row], osc == subNoise ? "SUB+N" : "OSC " + juce::String (osc + 1), oscColour (osc),
                       true, over && layout.osc[(size_t) row].contains (mouse));
        }

        drawBlock (g, layout.f1, "F1", juce::Colour (0xffc86bff), true, false);
        drawBlock (g, layout.f2, "F2", juce::Colour (0xff8f9dff), true, false);
        drawBlock (g, layout.res, "BODY", juce::Colour (0xff8f9dff), resOn, over && layout.res.contains (mouse));
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
        else if (layout.res.contains (event.position))
            toggle ("res_on");
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto layout = computeLayout();
        juce::String tip = "Signal flow\nClick an oscillator to route it, the badge to switch serial/parallel, BODY to toggle the body.";

        for (size_t row = 0; row < layout.sources.size(); ++row)
            if (layout.osc[row].contains (event.position))
            {
                const auto osc = layout.sources[row];
                tip = "Click to choose where " + juce::String (osc == subNoise ? "the sub and noise go." : "OSC " + juce::String (osc + 1) + " goes.");
            }

        setTooltip (tip);
    }

private:
    // Sources are the oscillators the patch has added, then the sub + noise.
    static constexpr int subNoise = OscillatorIds::count;

    struct Layout
    {
        std::vector<int> sources;
        std::vector<juce::Rectangle<float>> osc;
        juce::Rectangle<float> f1, f2, res, out, bypass, badge;
    };

    Layout computeLayout() const
    {
        Layout layout;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                layout.sources.push_back (osc);

        layout.sources.push_back (subNoise);
        const auto count = (int) layout.sources.size();

        auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
        const auto blockHeight = juce::jmin (24.0f, area.getHeight() / 4.8f);
        const auto sourceHeight = juce::jmin (22.0f, area.getHeight() / ((float) juce::jmax (4, count) * 1.25f));
        const auto rowGap = count > 1 ? (area.getHeight() - sourceHeight * (float) count) / (float) (count - 1) : 0.0f;

        auto oscColumn = area.removeFromLeft (juce::jmin (78.0f, area.getWidth() * 0.18f));

        for (int row = 0; row < count; ++row)
            layout.osc.push_back (oscColumn.withHeight (sourceHeight).withY (oscColumn.getY() + (float) row * (sourceHeight + rowGap)));

        layout.out = area.removeFromRight (46.0f).withSizeKeepingCentre (46.0f, blockHeight);
        area.removeFromRight (18.0f);
        layout.res = area.removeFromRight (52.0f).withSizeKeepingCentre (52.0f, blockHeight);
        area.removeFromRight (26.0f);
        area.removeFromLeft (40.0f);

        const auto parallel = read ("filters_parallel") > 0.5f;
        const auto filterWidth = juce::jmin (64.0f, area.getWidth() * 0.34f);
        const auto top = area.getY();
        const auto middle = area.getCentreY() - blockHeight * 0.5f;
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

        layout.bypass = juce::Rectangle<float> (area.getCentreX() - 30.0f, bottom, 60.0f, blockHeight);
        return layout;
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
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
            parameter->endChangeGesture();
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

                                parameter->beginChangeGesture();
                                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                parameter->endChangeGesture();
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

    static void drawBlock (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& text, juce::Colour colour,
                           bool lit, bool hovered)
    {
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (colour.withAlpha (lit ? (hovered ? 1.0f : 0.8f) : 0.25f));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, hovered ? 1.8f : 1.2f);
        g.setColour (lit ? colour : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (text, box, juce::Justification::centred);
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
};
