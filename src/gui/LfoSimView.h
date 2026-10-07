#pragma once

// M8.1: the LFO card's picture of a simulated shape. It runs its own copy of
// the simulation at the LFO's settings in real time (so it moves even when
// the LFO isn't routed), draws the object or the attractor's trail on the
// left and a scope of outputs A and B on the right.

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <vector>

#include "../dsp/LfoSim.h"
#include "IlanaLookAndFeel.h"

class LfoSimPreview
{
public:
    static constexpr double previewRate = 1200.0;
    static constexpr int historySize = 360;
    static constexpr int trailSize = 900;

    // Advances the picture by `seconds` of real time.
    void advance (const LfoSimSettings& settings, double rateHz, double seconds)
    {
        if (settings.shape != shape || settings.p != lastParams || settings.axis != lastAxis)
        {
            shape = settings.shape;
            lastParams = settings.p;
            lastAxis = settings.axis;
            sim.sampleRate = previewRate;
            sim.reset (settings, 7);
            trail.clear();
            historyA.fill (0.0f);
            historyB.fill (0.0f);
            restSeconds = 0.0;
        }

        const auto samples = juce::jlimit (0, 2400, (int) std::round (seconds * previewRate));
        // The scope spans about two cycles (at least a second).
        const auto window = juce::jlimit (1.0, 20.0, 2.0 / juce::jmax (0.01, rateHz));
        const auto samplesPerPoint = juce::jmax (1, (int) (window * previewRate / historySize));

        for (int i = 0; i < samples; ++i)
        {
            float a = 0.0f, b = 0.0f;
            sim.next (settings, rateHz / previewRate, a, b);
            peakA = juce::jmax (peakA, a);
            lowA = juce::jmin (lowA, a);
            peakB = std::abs (b) > std::abs (peakB) ? b : peakB;
            lastA = a;
            lastB = b;

            if (++pointCounter >= samplesPerPoint)
            {
                pointCounter = 0;
                historyA[(size_t) head] = lastA;
                historyB[(size_t) head] = peakB;
                peakB = 0.0f;
                head = (head + 1) % historySize;
            }

            if (++trailCounter >= juce::jmax (1, samplesPerPoint / 4))
            {
                trailCounter = 0;
                trail.push_back ({ (float) sim.state()[0], (float) sim.state()[1], (float) sim.state()[2], a, b });
                if ((int) trail.size() > trailSize)
                    trail.erase (trail.begin(), trail.begin() + (long) (trail.size() - (size_t) trailSize));
            }
        }

        // The picture restarts an object that has been at rest a while, so
        // it keeps showing what a trigger does.
        if (LfoSimShapes::isPhysics (shape) && ! settings.loop)
        {
            const auto still = std::abs (lastA - restValue) < 0.002f;
            restValue = lastA;
            restSeconds = still ? restSeconds + seconds : 0.0;
            if (restSeconds > 1.2)
            {
                sim.trigger (settings, 7);
                restSeconds = 0.0;
            }
        }
    }

    void trigger (const LfoSimSettings& settings) { sim.trigger (settings, 7); }

    // Output A's recent history as a rolling picture for a small card:
    // phase 0 is the oldest point, 1 the newest (what the dot sits on).
    float historyAt (double phase) const
    {
        const auto index = juce::jlimit (0, historySize - 1, (int) (phase * (double) historySize));
        return historyA[(size_t) ((head + index) % historySize)];
    }

    float latestA() const { return lastA; }

    // What outputs A and B carry, spelled out ("X axis", "impacts"), following
    // OUTPUT A's axis on the attractors (B is the next axis; Mix puts X and Z
    // on A, Y on B).
    static std::pair<juce::String, juce::String> outputNames (const LfoSimInfo::Shape& info, int axis)
    {
        const auto spell = [] (const juce::String& name)
        {
            if (name == "STEREO") return juce::String ("stereo twin of A");
            if (name == "IMPACT") return juce::String ("impacts");
            if (name == "SLIP") return juce::String ("slips");
            if (name == "PREVIOUS") return juce::String ("previous value");
            if (name == "X" || name == "Y") return name + " axis";
            return name.toLowerCase();
        };

        if (! info.usesAxis)
            return { spell (info.outA), spell (info.outB) };

        // Duffing's three outputs have names of their own; the attractors'
        // are their axes.
        const auto duffing = juce::String (info.outA) == "POSITION";
        const juce::StringArray axes = duffing ? juce::StringArray { "position", "velocity", "drive phase" }
                                               : juce::StringArray { "X axis", "Y axis", "Z axis" };
        axis = juce::jlimit (0, 3, axis);

        if (axis == 3)
            return { axes[0] + " + " + axes[2] + " mix", axes[1] };

        return { axes[axis], axes[(axis + 1) % 3] };
    }

    void paint (juce::Graphics& g, juce::Rectangle<float> area, const LfoSimSettings& settings, juce::Colour colour) const
    {
        const auto& info = LfoSimInfo::get (settings.shape);
        const auto scene = sceneArea (area, settings.shape);
        if (! scene.isEmpty())
            paintScene (g, scene.reduced (6.0f), settings, colour);
        paintScope (g, scopeArea (area, settings.shape), colour, info);
    }

    // The picture's two parts: the object or attractor on the left (none
    // for the random shapes) and the scope of outputs A and B.
    static juce::Rectangle<float> sceneArea (juce::Rectangle<float> area, int shape)
    {
        return LfoSimShapes::isRandom (shape) ? juce::Rectangle<float>()
                                              : area.removeFromLeft (juce::jmin (area.getWidth() * 0.45f, area.getHeight() * 1.25f));
    }

    static juce::Rectangle<float> scopeArea (juce::Rectangle<float> area, int shape)
    {
        const auto scene = sceneArea (area, shape);
        return scene.isEmpty() ? area : area.withTrimmedLeft (scene.getWidth() + 8.0f);
    }

    // The scope's "OUT 1: ..." and "OUT 2: ..." tags along its top, which drag the
    // LFO's two outputs onto knobs (UI review 6, I6-24).
    static juce::Rectangle<float> outputTagBounds (juce::Rectangle<float> scope, const juce::String& text, bool outputB)
    {
        const auto width = juce::jmin (scope.getWidth() * 0.5f - 4.0f,
                                       (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), text) + 14.0f);
        auto row = scope.removeFromTop (16.0f);
        return outputB ? row.removeFromRight (width) : row.removeFromLeft (width);
    }

    static juce::String outputTagText (const LfoSimInfo::Shape& info, int axis, bool outputB)
    {
        const auto names = outputNames (info, axis);
        return outputB ? "OUT 2: " + names.second : "OUT 1: " + names.first; // spelt out (UI review 9, V9-20)
    }

private:
    struct Point { float x, y, z, a, b; };

    LfoSim sim;
    int shape = -1, lastAxis = 0;
    std::array<float, LfoSimInfo::numParams> lastParams {};
    std::array<float, historySize> historyA {}, historyB {};
    std::vector<Point> trail;
    int head = 0, pointCounter = 0, trailCounter = 0;
    float lastA = 0.0f, lastB = 0.0f, peakA = -1.0f, lowA = 1.0f, peakB = 0.0f, restValue = 0.0f;
    double restSeconds = 0.0;

    void paintScope (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, const LfoSimInfo::Shape& info) const
    {
        // The two outputs as tags, outlined like chips: each drags its output.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        for (const auto outputB : { false, true })
        {
            const auto text = outputTagText (info, lastAxis, outputB);
            const auto tag = outputTagBounds (area, text, outputB);
            const auto tagColour = outputB ? IlanaTheme::Ui::text2 : colour;
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.6f));
            g.fillRoundedRectangle (tag, 7.0f);
            g.setColour (tagColour.withAlpha (0.55f));
            g.drawRoundedRectangle (tag.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (tagColour);
            IlanaTheme::drawFitted (g, text, tag.reduced (7.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1);
        }
        area.removeFromTop (18.0f);

        const auto centre = area.getCentreY();
        const auto half = area.getHeight() * 0.45f;
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (area.getWidth(), 1.0f).withCentre ({ area.getCentreX(), centre }));

        const auto trace = [&] (const std::array<float, historySize>& values)
        {
            juce::Path path;
            for (int i = 0; i < historySize; ++i)
            {
                const auto value = values[(size_t) ((head + i) % historySize)];
                const auto x = area.getX() + area.getWidth() * (float) i / (float) (historySize - 1);
                const auto y = centre - juce::jlimit (-1.0f, 1.0f, value) * half;
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }
            return path;
        };

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (trace (historyB), juce::PathStrokeType (1.1f));
        g.setColour (colour);
        g.strokePath (trace (historyA), juce::PathStrokeType (1.7f));

        const auto dot = juce::Point<float> (area.getRight(), centre - juce::jlimit (-1.0f, 1.0f, lastA) * half);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
    }

    void paintTrail (juce::Graphics& g, juce::Rectangle<float> box, juce::Colour colour, bool dots,
                     std::function<juce::Point<float> (const Point&)> project) const
    {
        if (trail.size() < 2)
            return;

        // Fit the trail's own extent into the box.
        auto minX = 1.0e9f, maxX = -1.0e9f, minY = 1.0e9f, maxY = -1.0e9f;
        for (const auto& point : trail)
        {
            const auto p = project (point);
            minX = juce::jmin (minX, p.x); maxX = juce::jmax (maxX, p.x);
            minY = juce::jmin (minY, p.y); maxY = juce::jmax (maxY, p.y);
        }
        const auto span = juce::jmax (1.0e-6f, juce::jmax (maxX - minX, maxY - minY));
        const auto scale = juce::jmin (box.getWidth(), box.getHeight()) * 0.9f / span;
        const auto map = [&] (juce::Point<float> p)
        {
            return juce::Point<float> (box.getCentreX() + (p.x - 0.5f * (minX + maxX)) * scale,
                                       box.getCentreY() - (p.y - 0.5f * (minY + maxY)) * scale);
        };

        const auto count = (int) trail.size();
        for (int i = 1; i < count; ++i)
        {
            const auto alpha = 0.08f + 0.8f * (float) i / (float) count;
            g.setColour (colour.withAlpha (alpha));
            const auto p1 = map (project (trail[(size_t) i]));
            if (dots)
                g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre (p1));
            else
                g.drawLine ({ map (project (trail[(size_t) i - 1])), p1 }, 1.2f);
        }
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (map (project (trail.back()))));
    }

    void paintScene (juce::Graphics& g, juce::Rectangle<float> box, const LfoSimSettings& s, juce::Colour colour) const
    {
        using namespace LfoSimShapes;
        const auto& st = sim.state();
        const auto value = [&] (int index) { return LfoSimInfo::value (s.shape, index, s.p[(size_t) index]); };
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (box, 5.0f);

        switch (s.shape)
        {
            case Lorenz:
                paintTrail (g, box, colour, false, [] (const Point& p) { return juce::Point<float> (p.x, p.z); });
                break;
            case Rossler:
                paintTrail (g, box, colour, false, [] (const Point& p) { return juce::Point<float> (p.x, p.y); });
                break;
            case Duffing:
                paintTrail (g, box, colour, false, [] (const Point& p) { return juce::Point<float> (p.x, p.y); });
                break;
            case Henon: case Logistic:
                paintTrail (g, box, colour, true, [] (const Point& p) { return juce::Point<float> (p.x, p.y); });
                break;
            case DoublePendulum:
            {
                const auto l1 = (float) value (0), l2 = (float) value (1);
                const auto scale = box.getHeight() * 0.45f / (l1 + l2);
                const auto pivot = juce::Point<float> (box.getCentreX(), box.getCentreY());
                const auto tip1 = pivot + juce::Point<float> (std::sin ((float) st[0]), std::cos ((float) st[0])) * l1 * scale;
                const auto tip2 = tip1 + juce::Point<float> (std::sin ((float) st[1]), std::cos ((float) st[1])) * l2 * scale;
                // The tip's trail (outputs A and B are its x and y).
                const auto count = (int) trail.size();
                for (int i = 1; i < count; ++i)
                {
                    const auto p0 = pivot + juce::Point<float> (trail[(size_t) i - 1].a, -trail[(size_t) i - 1].b) * (l1 + l2) * scale;
                    const auto p1 = pivot + juce::Point<float> (trail[(size_t) i].a, -trail[(size_t) i].b) * (l1 + l2) * scale;
                    g.setColour (colour.withAlpha (0.05f + 0.5f * (float) i / (float) count));
                    g.drawLine ({ p0, p1 }, 1.0f);
                }
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawLine ({ pivot, tip1 }, 2.0f);
                g.drawLine ({ tip1, tip2 }, 2.0f);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pivot));
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (8.0f + 3.0f * (float) value (2), 8.0f + 3.0f * (float) value (2)).withCentre (tip1));
                g.fillEllipse (juce::Rectangle<float> (8.0f + 3.0f * (float) value (3), 8.0f + 3.0f * (float) value (3)).withCentre (tip2));
                break;
            }
            case Bounce:
            {
                const auto floorY = box.getBottom() - 8.0f;
                const auto top = box.getY() + 10.0f;
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillRect (box.getX() + 8.0f, floorY, box.getWidth() - 16.0f, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawHorizontalLine ((int) top, box.getX() + 8.0f, box.getRight() - 8.0f);
                const auto height = (float) (st[0] / juce::jmax (1.0e-6, value (1)));
                const auto radius = juce::jmin (12.0f, box.getWidth() * 0.08f);
                const auto y = floorY - radius - juce::jlimit (0.0f, 1.2f, height) * (floorY - top - 2.0f * radius);
                const auto squash = juce::jlimit (0.0f, 0.35f, lastB * 0.5f);
                g.setColour (colour.withAlpha (0.25f * lastB));
                g.fillEllipse (juce::Rectangle<float> (radius * 6.0f * lastB, 6.0f).withCentre ({ box.getCentreX(), floorY }));
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f * (1.0f + squash), radius * 2.0f * (1.0f - squash))
                                   .withCentre ({ box.getCentreX(), y + radius * squash }));
                break;
            }
            case Pendulum:
            {
                const auto pivot = juce::Point<float> (box.getCentreX(), box.getY() + 12.0f);
                const auto length = box.getHeight() - 30.0f;
                const auto angle = (float) st[0];
                const auto bob = pivot + juce::Point<float> (std::sin (angle), std::cos (angle)) * length;
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawLine ({ pivot, pivot + juce::Point<float> (0.0f, length) }, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawLine ({ pivot, bob }, 2.0f);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pivot));
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (bob));
                break;
            }
            case Spring:
            {
                const auto top = box.getY() + 8.0f;
                const auto restY = box.getCentreY();
                const auto travel = box.getHeight() * 0.32f;
                const auto massY = restY + (float) st[0] * travel;
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillRect (box.getCentreX() - 20.0f, top - 2.0f, 40.0f, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawHorizontalLine ((int) (restY + (float) value (3) * travel), box.getX() + 8.0f, box.getRight() - 8.0f);
                juce::Path coil;
                coil.startNewSubPath (box.getCentreX(), top);
                constexpr int turns = 9;
                for (int i = 1; i <= turns * 2; ++i)
                    coil.lineTo (box.getCentreX() + (i % 2 == 0 ? -9.0f : 9.0f), top + (massY - 10.0f - top) * (float) i / (float) (turns * 2));
                coil.lineTo (box.getCentreX(), massY - 10.0f);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.strokePath (coil, juce::PathStrokeType (1.4f));
                g.setColour (colour);
                g.fillRoundedRectangle (juce::Rectangle<float> (28.0f, 20.0f).withCentre ({ box.getCentreX(), massY }), 3.0f);
                break;
            }
            case Friction:
            {
                // The block on the surface, the spring and the hand dragging it.
                const auto surface = box.getCentreY() + 12.0f;
                g.setColour (juce::Colours::white.withAlpha (0.3f));
                g.fillRect (box.getX() + 6.0f, surface, box.getWidth() - 12.0f, 2.0f);
                const auto stretch = juce::jlimit (-1.0f, 1.0f, lastA);
                const auto blockX = box.getX() + box.getWidth() * 0.28f;
                const auto handX = blockX + 30.0f + (0.5f + 0.5f * stretch) * box.getWidth() * 0.4f;
                juce::Path coil;
                coil.startNewSubPath (blockX + 14.0f, surface - 10.0f);
                constexpr int turns = 7;
                for (int i = 1; i <= turns * 2; ++i)
                    coil.lineTo (blockX + 14.0f + (handX - blockX - 14.0f) * (float) i / (float) (turns * 2),
                                 surface - 10.0f + (i % 2 == 0 ? -5.0f : 5.0f));
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.strokePath (coil, juce::PathStrokeType (1.3f));
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ handX, surface - 10.0f }));
                g.setColour (colour.interpolatedWith (juce::Colours::white, juce::jlimit (0.0f, 1.0f, lastB)));
                g.fillRoundedRectangle (juce::Rectangle<float> (28.0f, 20.0f).withCentre ({ blockX, surface - 10.0f }), 3.0f);
                break;
            }
            default: break;
        }
    }
};
