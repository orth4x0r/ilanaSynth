#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>

#include "../PluginProcessor.h"
#include "../dsp/FmAlgorithms.h"
#include "../dsp/OperatorEgParams.h"
#include "IlanaLookAndFeel.h"

// M5: one-click routings as a row of small pictures. Click one to route the
// operators that way; the one the patch matches is lit. UI review 6: three
// pages, BASIC (nine house routings) and the DX7's 32 algorithms under their
// own numbers (1-16, 17-32), so a DX7 voice lights its algorithm.
class FmAlgorithmStrip : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    enum Page { basic = 0, dx7Low, dx7High, numPages };

    explicit FmAlgorithmStrip (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip (defaultTooltip());
        matching = processorRef.findMatchingFmAlgorithm();
        matchingDx7 = processorRef.findMatchingDx7Algorithm();
        page = pageForMatch();
        startTimerHz (5);
    }

    static juce::Colour accent() { return juce::Colour (0xffe3a56f); }

    static juce::String defaultTooltip()
    {
        return "Algorithms: click one to route the operators that way (amounts stay editable). BASIC holds simple "
               "routings; the DX7 pages hold the DX7's 32 algorithms under their own numbers.";
    }

    // The page shown, and a call when it changes (the FM page's tabs).
    int getPage() const { return page; }
    void setPage (int newPage)
    {
        newPage = juce::jlimit (0, numPages - 1, newPage);
        if (newPage == page)
            return;
        page = newPage;
        hover = -1;
        if (onPageChanged != nullptr)
            onPageChanged();
        repaint();
    }
    std::function<void()> onPageChanged;
    // The name of the cell under the mouse, for the page's heading (empty
    // when none).
    std::function<void (const juce::String&)> onHoverChanged;

    int getNumCells() const { return page == basic ? FmAlgorithms::numBasic : 16; }

    // The algorithm a cell holds: its name, routing and DX7 number (0 on BASIC).
    const FmAlgorithms::Algorithm& algorithmAt (int cell) const
    {
        return page == basic ? FmAlgorithms::all()[(size_t) cell] : FmAlgorithms::dx7 (dx7NumberAt (cell));
    }
    int dx7NumberAt (int cell) const { return page == basic ? 0 : cell + 1 + (page == dx7High ? 16 : 0); }

    bool isLit (int cell) const
    {
        return page == basic ? cell == matching && matchingDx7 == 0 : dx7NumberAt (cell) == matchingDx7;
    }

    void paint (juce::Graphics& g) override
    {
        const auto shown = shownCount();
        for (int index = 0; index < getNumCells(); ++index)
        {
            const auto cell = cellBounds (index).toFloat().reduced (2.0f);
            const auto lit = isLit (index);
            const auto hovered = index == hover;
            const auto& algorithm = algorithmAt (index);

            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (lit ? accent().withAlpha (0.22f) : juce::Colours::white.withAlpha (hovered ? 0.07f : 0.03f));
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour (lit ? accent() : juce::Colours::white.withAlpha (hovered ? 0.35f : 0.12f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, lit ? 1.6f : 1.0f);

            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            if (page != basic)
            {
                // The DX7's own number.
                g.setColour (lit ? accent() : IlanaTheme::Ui::text2);
                g.drawText (juce::String (dx7NumberAt (index)), cell.reduced (3.0f, 1.0f).toNearestInt(), juce::Justification::topLeft);
            }
            // Picking it adds oscillators: say how many (UI review 6, S6-45).
            if (algorithm.numOperators > shown)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText ("+" + juce::String (algorithm.numOperators - shown), cell.reduced (3.0f, 1.0f).toNearestInt(),
                            juce::Justification::topRight);
            }

            paintAlgorithm (g, algorithm, cell.withTrimmedTop (page == basic ? 4.0f : 11.0f).reduced (3.0f, 3.0f), lit);
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
                g.setColour (IlanaTheme::Ui::panel);
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
            setTooltip (index >= 0 ? describeCell (index) : defaultTooltip());
            if (onHoverChanged != nullptr)
                onHoverChanged (index >= 0 ? juce::String (algorithmAt (index).name) : juce::String());
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        if (onHoverChanged != nullptr)
            onHoverChanged ({});
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position.toInt());

        if (index >= 0 && event.getDistanceFromDragStart() < 6)
        {
            if (page == basic)
                processorRef.applyFmAlgorithm (index);
            else
                processorRef.applyDx7Algorithm (dx7NumberAt (index));
            matching = processorRef.findMatchingFmAlgorithm();
            matchingDx7 = processorRef.findMatchingDx7Algorithm();
            repaint();
        }
    }

    // The page calls this when it is shown and on its timer, so a preset
    // load or undo updates the highlight (and turns to the lit page).
    void refreshMatch()
    {
        const auto now = processorRef.findMatchingFmAlgorithm();
        const auto nowDx7 = processorRef.findMatchingDx7Algorithm();

        if (now != matching || nowDx7 != matchingDx7)
        {
            const auto turn = nowDx7 != matchingDx7 && nowDx7 > 0;
            matching = now;
            matchingDx7 = nowDx7;
            if (turn)
                setPage (pageForMatch());
            repaint();
        }
    }

    int getMatching() const { return matching; }
    int getMatchingDx7() const { return matchingDx7; }
    juce::Point<int> getCellCentre (int index) const { return cellBounds (index).getCentre(); }

private:
    juce::String describeCell (int index) const
    {
        const auto& algorithm = algorithmAt (index);
        auto text = page == basic ? juce::String (algorithm.name) + " (" + juce::String (algorithm.numOperators) + " operators)"
                                  : juce::String (algorithm.name) + ": " + FmAlgorithms::dx7Description (dx7NumberAt (index));
        if (algorithm.numOperators > shownCount())
            text << ". Adds OSC " << (shownCount() + 1) << (algorithm.numOperators - shownCount() > 1 ? "-" + juce::String (algorithm.numOperators) : juce::String())
                 << " to the patch";
        return text + ".";
    }

    int shownCount() const
    {
        auto count = 0;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            count += processorRef.isOscillatorShown (osc) ? 1 : 0;
        return count;
    }

    // The page a match is on: a DX7 algorithm's, else BASIC unless the patch
    // is on the Operator Env (a DX7 voice), which opens on DX7 1-16.
    int pageForMatch() const
    {
        if (matchingDx7 > 0)
            return matchingDx7 <= 16 ? dx7Low : dx7High;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc)
                && juce::roundToInt (processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_amp_env")->load())
                       == OperatorEg::envelopeChoice)
                return dx7Low;
        return page;
    }

    juce::Rectangle<int> cellBounds (int index) const
    {
        const auto count = getNumCells();
        const auto width = getWidth() / count;
        return { index * width, 0, width, getHeight() };
    }

    int indexAt (juce::Point<int> position) const
    {
        for (int index = 0; index < getNumCells(); ++index)
            if (cellBounds (index).contains (position))
                return index;

        return -1;
    }

    void timerCallback() override
    {
        if (isShowing())
            refreshMatch();
    }

    IlanaSynthAudioProcessor& processorRef;
    int matching = -1, matchingDx7 = 0;
    int page = basic;
    int hover = -1;
};
