#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

#include <array>

class StepEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private IlanaAnim::FrameTimer
{
public:
    StepEditor (IlanaSynthAudioProcessor& processor, int lfoIndex, juce::Colour stepColourIn = IlanaTheme::accent(),
                bool followsThemeIn = false)
        : processorRef (processor),
          index (lfoIndex),
          stepColour (IlanaSynthAudioProcessor::lfoColour (lfoIndex)),
          followsTheme (followsThemeIn)
    {
        // Drawn in its LFO's colour (as on its chip and card), whatever
        // colour the page asked for.
        juce::ignoreUnused (stepColourIn);
        setTooltip ("Drag to draw the 16 step values");
        startTimerHz (24);
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

    int getLfoIndex() const { return index; }

    void setLfoIndex (int lfoIndex)
    {
        index = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, lfoIndex);
        stepColour = IlanaSynthAudioProcessor::lfoColour (index);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        IlanaTheme::paintWell (g, bounds, 6.0f);

        auto plot = bounds.reduced (8.0f, 10.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.44f;
        const auto stepWidth = plot.getWidth() / 16.0f;

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        for (int step = 0; step < 16; ++step)
        {
            const auto value = readStep (step);
            const auto x = plot.getX() + (float) step * stepWidth;
            const auto y = centreY - value * halfHeight;

            g.setColour (stepColour.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (x + 1.0f, juce::jmin (y, centreY), juce::jmax (1.0f, stepWidth - 2.0f),
                                                std::abs (y - centreY)));

            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (x + stepWidth - 1.0f, plot.getY(), 1.0f, plot.getHeight()));
        }
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    float readStep (int step) const
    {
        const auto id = "lfo" + juce::String (index + 1) + "_step" + juce::String (step + 1);

        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void setFromPosition (juce::Point<float> position)
    {
        const auto plot = getLocalBounds().toFloat().reduced (8.0f, 10.0f);

        if (plot.getWidth() <= 1.0f)
            return;

        const auto step = juce::jlimit (0, 15, (int) ((position.x - plot.getX()) / plot.getWidth() * 16.0f));
        const auto value = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (position.y - plot.getY()) / plot.getHeight());
        const auto id = "lfo" + juce::String (index + 1) + "_step" + juce::String (step + 1);

        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));

        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        processorRef.beginEdit ("LFO " + juce::String (index + 1) + " steps");
        setFromPosition (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override { setFromPosition (event.position); }
    void mouseUp (const juce::MouseEvent&) override { processorRef.endEdit(); }

    IlanaSynthAudioProcessor& processorRef;
    int index = 0;
    juce::Colour stepColour;
    bool followsTheme = false;
    float appear = 1.0f;
};

// The MSEG's four stages, edited the way the LFO's MSEG shape and the
// envelopes are (UI review 6, I6-21 and I6-22): drag a point and it follows
// the mouse (up and down is its level; sideways moves the boundary between
// the stages either side of it, so the cycle keeps its length), the value
// shows while dragging, double-click resets a point, right-click for shapes.
// GRID snaps a dragged point's time to columns, as on the LFO graph (UI
// review 7, I7-30: display only, not an audio parameter). The time ruler
// sits under the graph's frame.
class MsegEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private IlanaAnim::FrameTimer
{
public:
    explicit MsegEditor (IlanaSynthAudioProcessor& processor)
        : processorRef (processor)
    {
        setTooltip ("MSEG\nDrag a point: up and down sets its level, sideways moves it in time (GRID, top right, sets the snap). "
                    "Double-click a point to reset it; right-click for shapes. The ruler gives the time into one cycle at the "
                    "current RATE; the line and dot show where it is now.");
        startTimerHz (24);
    }

    static juce::Colour colour() { return juce::Colour (0xffe0e6f0); } // the MSEG's source colour

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        IlanaTheme::paintWell (g, bounds.withTrimmedBottom (rulerHeight + 2.0f), 6.0f);

        const auto plot = plotArea();
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.5f;

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        paintTimeGrid (g, plot, centreY, halfHeight);

        // The snap columns, and GRID's own control.
        if (gridDivisions > 0)
        {
            g.setColour (colour().withAlpha (0.07f));
            for (int column = 1; column < gridDivisions; ++column)
                g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight())
                                .withPosition (plot.getX() + plot.getWidth() * (float) column / (float) gridDivisions, plot.getY()));
        }
        IlanaTheme::paintPill (g, gridChipBounds(), gridDivisions > 0 ? "GRID " + juce::String (gridDivisions) : juce::String ("GRID OFF"),
                               colour(), gridDivisions > 0, gridChipBounds().contains (getMouseXYRelative().toFloat()) ? 1.0f : 0.0f);

        juce::Path path;
        const auto points = pointPositions();

        for (int i = 0; i < 4; ++i)
        {
            if (i == 0)
                path.startNewSubPath (points[(size_t) i]);
            else
                path.lineTo (points[(size_t) i]);
        }

        // Back to the first level when it loops, else it holds the last.
        const auto endPoint = juce::Point<float> (plot.getRight(), centreY - (isLooping() ? readLevel (0) : readLevel (3)) * halfHeight);
        path.lineTo (endPoint);

        auto filled = path;
        filled.lineTo (plot.getRight(), centreY);
        filled.lineTo (plot.getX(), centreY);
        filled.closeSubPath();
        g.setColour (colour().withAlpha (0.07f));
        g.fillPath (filled);

        g.setColour (colour());
        g.strokePath (path, juce::PathStrokeType (1.8f));

        // The end: hollow, it isn't a point of its own.
        g.setColour (colour().withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (endPoint), 1.2f);

        for (int i = 0; i < 4; ++i)
        {
            const auto hot = i == dragHandle || (dragHandle < 0 && i == hoverHandle);

            if (hot)
            {
                g.setColour (colour().withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (points[(size_t) i]));
            }

            g.setColour (juce::Colours::white.withAlpha (hot ? 1.0f : 0.85f));
            g.fillEllipse (juce::Rectangle<float> (hot ? 10.0f : 8.0f, hot ? 10.0f : 8.0f).withCentre (points[(size_t) i]));
        }

        // The playhead: a faint line where the MSEG is in its cycle and a
        // dot riding the curve there, as on the LFO graph.
        const auto phase = juce::jlimit (0.0f, 1.0f, processorRef.getMsegPhase());
        const auto playX = plot.getX() + phase * plot.getWidth();
        g.setColour (colour().withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (playX, plot.getY()));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ playX, centreY - valueAt (phase) * halfHeight }));

        // The dragged point's value, beside it.
        if (dragHandle >= 0)
            paintReadout (g, points[(size_t) dragHandle], readoutText (dragHandle), plot);
    }

    // For the UI test: where point `index` (0-3) sits, and the time (0..1
    // of a cycle) at which it starts.
    juce::Point<float> getPointPosition (int index) const { return pointPositions()[(size_t) juce::jlimit (0, 3, index)]; }
    juce::Rectangle<float> getPlotArea() const { return plotArea(); }
    int getGridDivisions() const { return gridDivisions; }
    void setGridDivisions (int divisions) { gridDivisions = divisions; repaint(); }

private:
    static constexpr float rulerHeight = 14.0f;

    juce::Rectangle<float> plotArea() const
    {
        return getLocalBounds().toFloat().reduced (12.0f, 0.0f).withTrimmedTop (14.0f).withTrimmedBottom (rulerHeight + 2.0f + 8.0f);
    }

    juce::Rectangle<float> gridChipBounds() const
    {
        return getLocalBounds().toFloat().removeFromTop (22.0f).removeFromRight (70.0f).reduced (6.0f, 3.0f);
    }

    void showGridMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Snap to grid");
        for (const auto divisions : { 0, 4, 8, 16, 32 })
            menu.addItem (200 + divisions, divisions == 0 ? juce::String ("Off") : juce::String (divisions) + " columns",
                          true, gridDivisions == divisions);
        juce::Component::SafePointer<MsegEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result)
        {
            if (safeThis != nullptr && result >= 200)
                safeThis->setGridDivisions (result - 200);
        });
    }

    float totalTime() const
    {
        auto total = 0.0f;
        for (int i = 0; i < 4; ++i)
            total += readTime (i);
        return total;
    }

    // Where each point sits: point i starts stage i.
    std::array<juce::Point<float>, 4> pointPositions() const
    {
        const auto plot = plotArea();
        const auto total = totalTime();
        std::array<juce::Point<float>, 4> points;
        auto cumulative = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            points[(size_t) i] = { plot.getX() + (cumulative / total) * plot.getWidth(),
                                   plot.getCentreY() - readLevel (i) * plot.getHeight() * 0.5f };
            cumulative += readTime (i);
        }

        return points;
    }

    // The level the MSEG plays at `phase` (0..1), as Mseg::valueAt does.
    float valueAt (float phase) const
    {
        const auto total = totalTime();
        auto cumulative = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            const auto duration = readTime (i) / total;

            if (phase < cumulative + duration || i == 3)
            {
                const auto local = juce::jlimit (0.0f, 1.0f, duration > 0.0001f ? (phase - cumulative) / duration : 0.0f);
                const auto to = i < 3 ? readLevel (i + 1) : (isLooping() ? readLevel (0) : readLevel (3));
                return readLevel (i) + (to - readLevel (i)) * local;
            }

            cumulative += duration;
        }

        return readLevel (3);
    }

    // One cycle lasts 1 / RATE seconds (before modulation).
    double cycleSeconds() const
    {
        const auto* rate = processorRef.apvts.getRawParameterValue ("mseg_rate");
        return 1.0 / juce::jmax (0.001, rate != nullptr ? (double) rate->load() : 1.0);
    }

    static juce::String timeText (double seconds)
    {
        return seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms"
                             : juce::String (seconds, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " s";
    }

    // Faint level lines at +-0.5 and +-1, time lines through the plot and
    // their labels on a ruler below it.
    void paintTimeGrid (juce::Graphics& g, juce::Rectangle<float> plot, float centreY, float halfHeight) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));

        for (const auto level : { -1.0f, -0.5f, 0.5f, 1.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), centreY - level * halfHeight));

        const auto cycle = cycleSeconds();
        auto step = 0.001;

        for (const auto candidate : { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0 })
        {
            step = candidate;

            if (cycle / candidate <= 8.0)
                break;
        }

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        auto lastLabelRight = -1.0e9f;
        const auto rulerY = (float) getHeight() - rulerHeight + 2.0f;

        for (int tick = 0; (double) tick * step <= cycle + 1.0e-9; ++tick)
        {
            const auto seconds = (double) tick * step;
            const auto x = plot.getX() + (float) (seconds / cycle) * plot.getWidth();
            g.setColour (juce::Colours::white.withAlpha (tick == 0 ? 0.0f : 0.05f));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (x, plot.getY()));
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x, rulerY - 3.0f));

            const auto text = timeText (seconds);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
            auto labelX = x - width * 0.5f;
            labelX = juce::jlimit (plot.getX() - 8.0f, plot.getRight() + 8.0f - width, labelX);

            if (labelX < lastLabelRight + 6.0f)
                continue;

            lastLabelRight = labelX + width;
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (text, juce::Rectangle<float> (labelX, rulerY, width, rulerHeight - 3.0f), juce::Justification::centred);
        }
    }

    juce::String readoutText (int point) const
    {
        auto cumulative = 0.0f;
        for (int i = 0; i < point; ++i)
            cumulative += readTime (i);
        const auto level = readLevel (point);
        return "POINT " + juce::String (point + 1) + "  " + (level >= 0.0f ? "+" : "") + juce::String (level, 2)
               + (point > 0 ? "  at " + timeText (cumulative / totalTime() * cycleSeconds()) : juce::String());
    }

    static void paintReadout (juce::Graphics& g, juce::Point<float> anchor, const juce::String& text, juce::Rectangle<float> area)
    {
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 14.0f;
        auto box = juce::Rectangle<float> (width, 17.0f).withCentre ({ anchor.x, anchor.y - 18.0f });
        if (box.getY() < area.getY())
            box.setY (anchor.y + 10.0f);
        box.setX (juce::jlimit (area.getX(), area.getRight() - width, box.getX()));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (colour());
        g.setFont (font);
        g.drawText (text, box, juce::Justification::centred);
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                                                                ^ IlanaAnim::phaseSignature (processorRef.getMsegPhase(), 77))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    bool isLooping() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_loop"))
            return value->load() > 0.5f;

        return true;
    }

    float readLevel (int point) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_level" + juce::String (point + 1)))
            return value->load();

        return 0.0f;
    }

    float readTime (int point) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_time" + juce::String (point + 1)))
            return juce::jmax (0.01f, value->load());

        return 0.25f;
    }

    void setParameter (const juce::String& id, float plainValue)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    int findHandle (juce::Point<float> position) const
    {
        const auto points = pointPositions();
        auto best = -1;
        auto bestDistance = 14.0f;

        for (int i = 0; i < 4; ++i)
            if (const auto distance = points[(size_t) i].getDistanceFrom (position); distance < bestDistance)
            {
                best = i;
                bestDistance = distance;
            }

        return best;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto handle = findHandle (event.position);

        if (handle != hoverHandle)
        {
            hoverHandle = handle;
            setMouseCursor (handle >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverHandle = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            showShapeMenu();
            return;
        }

        if (gridChipBounds().contains (event.position))
        {
            showGridMenu();
            return;
        }

        dragHandle = findHandle (event.position);

        if (dragHandle >= 0)
            processorRef.beginEdit ("MSEG point " + juce::String (dragHandle + 1));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragHandle >= 0)
            processorRef.endEdit();

        dragHandle = -1;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragHandle < 0)
            return;

        const auto plot = plotArea();
        setParameter ("mseg_level" + juce::String (dragHandle + 1),
                      juce::jlimit (-1.0f, 1.0f, (plot.getCentreY() - event.position.y) / juce::jmax (1.0f, plot.getHeight() * 0.5f)));

        // Sideways: the stage before the point and the one after it share
        // their combined time, so the cycle (and the other points) stay put.
        if (dragHandle > 0)
        {
            const auto total = totalTime();
            auto before = 0.0f;
            for (int i = 0; i < dragHandle - 1; ++i)
                before += readTime (i);
            const auto shared = readTime (dragHandle - 1) + readTime (dragHandle);
            auto fraction = juce::jlimit (0.0f, 1.0f, (event.position.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
            if (gridDivisions > 0)
                fraction = std::round (fraction * (float) gridDivisions) / (float) gridDivisions;
            const auto wanted = fraction * total - before;
            const auto first = juce::jlimit (juce::jmax (minTime, shared - maxTime), juce::jmin (maxTime, shared - minTime), wanted);
            setParameter ("mseg_time" + juce::String (dragHandle), first);
            setParameter ("mseg_time" + juce::String (dragHandle + 1), shared - first);
        }

        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto handle = findHandle (event.position);

        if (handle < 0)
            return;

        processorRef.performEdit ("Reset MSEG point " + juce::String (handle + 1), [this, handle]
        {
            if (auto* parameter = processorRef.apvts.getParameter ("mseg_level" + juce::String (handle + 1)))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
        });
    }

    // The right-click menu: a few starting shapes, and flips.
    void showShapeMenu()
    {
        juce::PopupMenu shapes;
        const juce::StringArray names { "Triangle (default)", "Ramp up", "Ramp down", "Pulse", "Rise and fall" };
        for (int i = 0; i < names.size(); ++i)
            shapes.addItem (100 + i, names[i]);

        juce::PopupMenu menu;
        menu.addSubMenu ("Load shape", shapes);
        menu.addSeparator();
        menu.addItem (1, "Flip vertically");
        menu.addItem (2, "Even timing");

        juce::Component::SafePointer<MsegEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            auto& self = *safeThis;
            self.processorRef.performEdit ("MSEG shape", [&self, result]
            {
                std::array<float, 4> levels { self.readLevel (0), self.readLevel (1), self.readLevel (2), self.readLevel (3) };
                std::array<float, 4> times { self.readTime (0), self.readTime (1), self.readTime (2), self.readTime (3) };

                if (result == 1)
                    for (auto& level : levels)
                        level = -level;
                else if (result == 2)
                    times = { 0.25f, 0.25f, 0.25f, 0.25f };
                else
                {
                    static const std::array<std::array<float, 8>, 5> shapes { {
                        { 0.0f, 1.0f, 0.0f, -1.0f, 0.25f, 0.25f, 0.25f, 0.25f },
                        { -1.0f, -0.33f, 0.33f, 1.0f, 0.33f, 0.33f, 0.33f, 0.05f },
                        { 1.0f, 0.33f, -0.33f, -1.0f, 0.33f, 0.33f, 0.33f, 0.05f },
                        { 1.0f, 1.0f, -1.0f, -1.0f, 0.5f, 0.05f, 0.5f, 0.05f },
                        { -1.0f, 1.0f, 1.0f, -1.0f, 0.4f, 0.2f, 0.4f, 0.05f },
                    } };
                    const auto& shape = shapes[(size_t) juce::jlimit (0, 4, result - 100)];
                    for (int i = 0; i < 4; ++i)
                    {
                        levels[(size_t) i] = shape[(size_t) i];
                        times[(size_t) i] = shape[(size_t) i + 4];
                    }
                }

                for (int i = 0; i < 4; ++i)
                {
                    self.setParameter ("mseg_level" + juce::String (i + 1), levels[(size_t) i]);
                    self.setParameter ("mseg_time" + juce::String (i + 1), times[(size_t) i]);
                }
            });
            self.repaint();
        });
    }

    static constexpr float minTime = 0.05f, maxTime = 1.0f; // mseg_time's range

    IlanaSynthAudioProcessor& processorRef;
    int dragHandle = -1;
    int hoverHandle = -1;
    int gridDivisions = 8;
    float appear = 1.0f;
};
