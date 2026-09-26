#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>

#include "../PluginProcessor.h"
#include "../dsp/FmAlgorithms.h"
#include "IlanaLookAndFeel.h"

// M5: the sixteen algorithm presets as a row of small routing pictures. Click
// one to route the operators that way; the one the patch matches is lit.
class FmAlgorithmStrip : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    explicit FmAlgorithmStrip (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Algorithms: click one to route the operators that way. Amounts stay editable.");
        matching = processorRef.findMatchingFmAlgorithm();
        startTimerHz (5);
    }

    static juce::Colour accent() { return juce::Colour (0xffe3a56f); }

    void paint (juce::Graphics& g) override
    {
        for (int index = 0; index < FmAlgorithms::count(); ++index)
        {
            const auto cell = cellBounds (index).toFloat().reduced (2.0f);
            const auto lit = index == matching;
            const auto hovered = index == hover;

            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (lit ? accent().withAlpha (0.22f) : juce::Colours::white.withAlpha (hovered ? 0.07f : 0.03f));
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (lit ? accent() : juce::Colours::white.withAlpha (hovered ? 0.35f : 0.12f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, lit ? 1.6f : 1.0f);

            g.setColour (lit ? accent() : juce::Colours::white.withAlpha (0.45f));
            g.setFont (IlanaTheme::font (9.5f, true));
            g.drawText (juce::String (index + 1), cell.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);

            paintAlgorithm (g, FmAlgorithms::all()[(size_t) index], cell.withTrimmedTop (10.0f).reduced (4.0f, 3.0f), lit);
        }
    }

    // Operators by depth: carriers on the bottom row, each modulator one row
    // above the deepest operator it drives.
    static void paintAlgorithm (juce::Graphics& g, const FmAlgorithms::Algorithm& algorithm,
                                juce::Rectangle<float> area, bool lit)
    {
        const auto count = algorithm.numOperators;
        std::array<int, 6> level {};
        level.fill (-1);

        for (const auto carrier : algorithm.carriers)
            level[(size_t) carrier] = 0;

        for (int pass = 0; pass < 6; ++pass)
            for (const auto& route : algorithm.routes)
                if (level[(size_t) route.second] >= 0)
                    level[(size_t) route.first] = juce::jmax (level[(size_t) route.first], level[(size_t) route.second] + 1);

        auto levels = 1;
        for (int op = 0; op < count; ++op)
            levels = juce::jmax (levels, level[(size_t) op] + 1);

        std::array<juce::Point<float>, 6> centres {};
        const auto rowHeight = area.getHeight() / (float) levels;

        for (int row = 0; row < levels; ++row)
        {
            std::vector<int> ops;
            for (int op = 0; op < count; ++op)
                if (juce::jmax (0, level[(size_t) op]) == row)
                    ops.push_back (op);

            for (size_t i = 0; i < ops.size(); ++i)
                centres[(size_t) ops[i]] = { area.getX() + area.getWidth() * 0.92f * ((float) i + 0.5f) / (float) ops.size(),
                                             area.getBottom() - rowHeight * ((float) row + 0.5f) };
        }

        auto widest = 1;
        for (int row = 0; row < levels; ++row)
        {
            auto onRow = 0;
            for (int op = 0; op < count; ++op)
                onRow += juce::jmax (0, level[(size_t) op]) == row ? 1 : 0;
            widest = juce::jmax (widest, onRow);
        }

        // Leave room on the right for the feedback loop.
        const auto radius = juce::jlimit (1.8f, 4.2f, juce::jmin (rowHeight * 0.28f, area.getWidth() / ((float) widest * 3.0f + 1.5f)));
        const auto colour = lit ? accent() : juce::Colours::white.withAlpha (0.6f);

        g.setColour (colour.withAlpha (0.7f));
        for (const auto& route : algorithm.routes)
            g.drawLine ({ centres[(size_t) route.first], centres[(size_t) route.second] }, 1.0f);

        if (algorithm.feedbackOperator >= 0 && algorithm.feedbackOperator < count)
        {
            const auto centre = centres[(size_t) algorithm.feedbackOperator];
            g.drawEllipse (juce::Rectangle<float> (radius * 2.2f, radius * 2.2f).withCentre (centre.translated (radius * 1.3f, -radius * 1.3f)), 0.9f);
        }

        for (int op = 0; op < count; ++op)
        {
            const auto node = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centres[(size_t) op]);

            if (FmAlgorithms::isCarrier (algorithm, op))
            {
                g.setColour (colour);
                g.fillEllipse (node);
            }
            else
            {
                g.setColour (juce::Colour (0xff17171b));
                g.fillEllipse (node);
                g.setColour (colour);
                g.drawEllipse (node, 1.0f);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position.toInt());

        if (index != hover)
        {
            hover = index;
            setTooltip (index >= 0 ? juce::String (index + 1) + ": " + FmAlgorithms::all()[(size_t) index].name
                                         + " (" + juce::String (FmAlgorithms::all()[(size_t) index].numOperators) + " operators)"
                                   : juce::String ("Algorithms: click one to route the operators that way. Amounts stay editable."));
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position.toInt());

        if (index >= 0 && event.getDistanceFromDragStart() < 6)
        {
            processorRef.applyFmAlgorithm (index);
            matching = processorRef.findMatchingFmAlgorithm();
            repaint();
        }
    }

    // The page calls this when it is shown.
    void refreshMatch()
    {
        matching = processorRef.findMatchingFmAlgorithm();
        repaint();
    }

    int getMatching() const { return matching; }
    juce::Point<int> getCellCentre (int index) const { return cellBounds (index).getCentre(); }

private:
    juce::Rectangle<int> cellBounds (int index) const
    {
        const auto count = FmAlgorithms::count();
        const auto columns = getWidth() >= 16 * 38 ? count : count / 2;
        const auto rows = count / columns;
        const auto width = getWidth() / columns;
        const auto height = getHeight() / rows;
        return { (index % columns) * width, (index / columns) * height, width, height };
    }

    int indexAt (juce::Point<int> position) const
    {
        for (int index = 0; index < FmAlgorithms::count(); ++index)
            if (cellBounds (index).contains (position))
                return index;

        return -1;
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        const auto now = processorRef.findMatchingFmAlgorithm();

        if (now != matching)
        {
            matching = now;
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    int matching = -1;
    int hover = -1;
};
