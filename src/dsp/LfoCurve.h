#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

// A drawable LFO shape: points joined by curved segments. Each point's
// tension bends the segment that starts at it (0 straight, + slow start,
// - fast start). The first point sits at x = 0 and the last at x = 1.
struct LfoCurve
{
    struct Point
    {
        float x = 0.0f;
        float y = 0.0f;
        float tension = 0.0f;
    };

    static constexpr int tableSize = 256;

    std::vector<Point> points { { 0.0f, 0.0f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.75f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };

    static float bend (float t, float tension)
    {
        if (std::abs (tension) < 1.0e-3f)
            return t;

        const auto k = tension * 5.0f;
        return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
    }

    float valueAt (double phase) const
    {
        if (points.empty())
            return 0.0f;

        const auto x = (float) (phase - std::floor (phase));

        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            const auto& a = points[i];
            const auto& b = points[i + 1];

            if (x <= b.x)
            {
                const auto width = b.x - a.x;
                const auto t = width > 1.0e-6f ? (x - a.x) / width : 1.0f;
                return a.y + (b.y - a.y) * bend (juce::jlimit (0.0f, 1.0f, t), a.tension);
            }
        }

        return points.back().y;
    }

    void renderTable (float* table) const
    {
        for (int i = 0; i < tableSize; ++i)
            table[i] = valueAt ((double) i / (double) tableSize);
    }

    // Keeps the shape valid: sorted, endpoints pinned to 0 and 1, values
    // in range, at least two points.
    void sanitise()
    {
        for (auto& point : points)
        {
            point.x = juce::jlimit (0.0f, 1.0f, point.x);
            point.y = juce::jlimit (-1.0f, 1.0f, point.y);
            point.tension = juce::jlimit (-1.0f, 1.0f, point.tension);
        }

        std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });

        if (points.size() < 2)
            points = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };

        points.front().x = 0.0f;
        points.back().x = 1.0f;
    }

    juce::String toString() const
    {
        juce::StringArray parts;

        for (const auto& point : points)
            parts.add (juce::String (point.x, 4) + "," + juce::String (point.y, 4) + "," + juce::String (point.tension, 3));

        return parts.joinIntoString (";");
    }

    static LfoCurve fromString (const juce::String& text)
    {
        LfoCurve curve;
        curve.points.clear();

        for (const auto& part : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto values = juce::StringArray::fromTokens (part, ",", "");

            if (values.size() >= 2)
                curve.points.push_back ({ values[0].getFloatValue(), values[1].getFloatValue(),
                                          values.size() > 2 ? values[2].getFloatValue() : 0.0f });
        }

        curve.sanitise();
        return curve;
    }

    static juce::StringArray getPresetNames()
    {
        return { "Sine-ish", "Triangle", "Ramp Up", "Ramp Down", "Square", "Pluck", "Sidechain", "Swell",
                 "Stairs 4", "Bounce", "Wobble 3/4", "Flat" };
    }

    static LfoCurve preset (int index)
    {
        LfoCurve curve;

        switch (index)
        {
            case 1:  curve.points = { { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 2:  curve.points = { { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 3:  curve.points = { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 4:  curve.points = { { 0.0f, 1.0f, 0.0f }, { 0.499f, 1.0f, 0.0f }, { 0.5f, -1.0f, 0.0f },
                                      { 0.999f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 5:  curve.points = { { 0.0f, 1.0f, -0.8f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 6:  curve.points = { { 0.0f, -1.0f, -0.7f }, { 0.35f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 7:  curve.points = { { 0.0f, -1.0f, 0.8f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 8:  curve.points = { { 0.0f, -1.0f, 0.0f }, { 0.249f, -1.0f, 0.0f }, { 0.25f, -0.33f, 0.0f },
                                      { 0.499f, -0.33f, 0.0f }, { 0.5f, 0.33f, 0.0f }, { 0.749f, 0.33f, 0.0f },
                                      { 0.75f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 9:  curve.points = { { 0.0f, 1.0f, -0.6f }, { 0.4f, -1.0f, 0.6f }, { 0.6f, 0.2f, -0.6f },
                                      { 0.8f, -1.0f, 0.6f }, { 0.9f, -0.4f, -0.6f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 10: curve.points = { { 0.0f, -1.0f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.375f, -1.0f, 0.0f },
                                      { 0.5f, 1.0f, 0.0f }, { 0.75f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 11: curve.points = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; break;
            default:
                curve.points = { { 0.0f, 0.0f, -0.45f }, { 0.25f, 1.0f, 0.45f }, { 0.5f, 0.0f, -0.45f },
                                 { 0.75f, -1.0f, 0.45f }, { 1.0f, 0.0f, 0.0f } };
                break;
        }

        return curve;
    }
};
