#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace IlanaAnim
{
// Implemented by components that fade/slide in when they become visible.
struct PageAnimated
{
    virtual ~PageAnimated() = default;
    virtual void replayAppear() = 0;
};

inline void replayPageAppear (juce::Component& component)
{
    for (auto* child : component.getChildren())
    {
        if (auto* animated = dynamic_cast<PageAnimated*> (child))
            animated->replayAppear();

        replayPageAppear (*child);
    }
}

inline float easeOutCubic (float t)
{
    const auto inv = 1.0f - juce::jlimit (0.0f, 1.0f, t);
    return 1.0f - inv * inv * inv;
}

inline float easeInOutCubic (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

inline float easeOutBack (float t)
{
    constexpr auto c1 = 1.70158f;
    constexpr auto c3 = c1 + 1.0f;
    const auto inv = juce::jlimit (0.0f, 1.0f, t) - 1.0f;
    return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
}

inline float approach (float current, float target, float rate)
{
    return current + (target - current) * juce::jlimit (0.0f, 1.0f, rate);
}

// Downscale + separable box blur + upscale, used for frosted-glass overlays.
inline juce::Image blurredSnapshot (juce::Component& source, float radius, float scale = 0.16f)
{
    auto snapshot = source.createComponentSnapshot (source.getLocalBounds(), false);

    if (! snapshot.isValid())
        return {};

    const auto smallW = juce::jmax (2, (int) ((float) snapshot.getWidth() * scale));
    const auto smallH = juce::jmax (2, (int) ((float) snapshot.getHeight() * scale));
    auto small = snapshot.rescaled (smallW, smallH, juce::Graphics::mediumResamplingQuality);

    {
        juce::Image::BitmapData data (small, juce::Image::BitmapData::readWrite);
        const auto passes = juce::jlimit (1, 3, (int) std::round (radius));

        for (int pass = 0; pass < passes; ++pass)
        {
            const auto horizontal = (pass % 2) == 0;

            for (int y = 0; y < smallH; ++y)
            {
                for (int x = 0; x < smallW; ++x)
                {
                    auto r = 0.0f;
                    auto g = 0.0f;
                    auto b = 0.0f;
                    auto count = 0.0f;

                    for (int k = -2; k <= 2; ++k)
                    {
                        const auto sx = horizontal ? x + k : x;
                        const auto sy = horizontal ? y : y + k;

                        if (sx < 0 || sx >= smallW || sy < 0 || sy >= smallH)
                            continue;

                        const auto* pixel = data.getPixelPointer (sx, sy);
                        r += (float) pixel[0];
                        g += (float) pixel[1];
                        b += (float) pixel[2];
                        count += 1.0f;
                    }

                    auto* pixel = data.getPixelPointer (x, y);
                    pixel[0] = (juce::uint8) juce::jlimit (0.0f, 255.0f, r / count);
                    pixel[1] = (juce::uint8) juce::jlimit (0.0f, 255.0f, g / count);
                    pixel[2] = (juce::uint8) juce::jlimit (0.0f, 255.0f, b / count);
                }
            }
        }
    }

    return small.rescaled (snapshot.getWidth(), snapshot.getHeight(), juce::Graphics::mediumResamplingQuality);
}
} // namespace IlanaAnim
