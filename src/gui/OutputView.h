#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <complex>
#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ScopeDisplay.h"

// A small live view of the synth's output for the PLAY page: the waveform on
// top, the spectrum under it (log frequency, auto-gained). Click to switch
// between both, wave only and spectrum only. Repaints only while sound plays
// or the spectrum is still falling.
class OutputView : public juce::Component,
                   public juce::SettableTooltipClient,
                   private IlanaAnim::FrameTimer
{
public:
    explicit OutputView (IlanaSynthAudioProcessor& processor) : processorRef (processor), fft (11)
    {
        setTooltip ("Live output: waveform and spectrum.  Click to switch between both, wave and spectrum.");
        scopeL.assign (fftSize, 0.0f);
        scopeR.assign (fftSize, 0.0f);
        windowBuffer.assign (fftSize, 0.0f);
        fftInput.assign (fftSize, {});
        fftOutput.assign (fftSize, {});
        startTimerHz (30);
    }

    int getViewMode() const { return viewMode; }

    // The header's strip: the waveform only, drawn straight on the header
    // (no card), so every page has a live view of the sound as Vital's top
    // bar does. A click calls onStripClick (the editor opens the scope).
    void setStrip (bool shouldBeStrip)
    {
        strip = shouldBeStrip;
        setTooltip (strip ? "Live output.  Click for the scope." : "Live output: waveform and spectrum.  Click to switch between both, wave and spectrum.");
        repaint();
    }

    bool isStrip() const { return strip; }
    std::function<void()> onStripClick;

    void mouseDown (const juce::MouseEvent&) override
    {
        if (strip)
        {
            if (onStripClick != nullptr)
                onStripClick();
            return;
        }

        viewMode = (viewMode + 1) % 3;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        IlanaAnim::countPaint ("outputView");
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            paintTicks = lastPaintMs > 0.0 ? (float) juce::jlimit (0.0, 300.0, (now - lastPaintMs) * 0.03) : 1.0f;
            lastPaintMs = now;
        }

        const auto bounds = getLocalBounds().toFloat();

        if (strip)
        {
            processorRef.copyScopeDataSmooth (scopeL.data(), scopeR.data(), fftSize);
            // Quiet: it sits under the preset name, the most-read text, and
            // moves all the time (review 8, S8-41).
            // On a faint band of its own, so it doesn't read as an underline
            // of the preset name (UI review 9, S9-26).
            // A small display, well and rim, with its baseline, not a line
            // under the name (S10-14).
            // In the preset display (the shell mockup): the wave alone,
            // faint, with no well or rim of its own.
            drawWave (g, bounds.reduced (0.0f, 1.0f), IlanaTheme::accent().withMultipliedAlpha (0.55f), true);
            return;
        }

        IlanaTheme::paintCard (g, bounds, 6.0f, IlanaTheme::accent());
        IlanaTheme::paintWell (g, bounds.reduced (6.0f), 5.0f);

        processorRef.copyScopeDataSmooth (scopeL.data(), scopeR.data(), fftSize);
        auto area = bounds.reduced (12.0f, 10.0f);
        const auto colour = IlanaTheme::accent();

        auto waveArea = juce::Rectangle<float>();
        auto specArea = juce::Rectangle<float>();

        if (viewMode == 0 && area.getHeight() > 70.0f)
        {
            waveArea = area.removeFromTop (area.getHeight() * 0.42f);
            area.removeFromTop (6.0f);
            specArea = area;
        }
        else if (viewMode == 1)
        {
            waveArea = area;
        }
        else
        {
            specArea = area;
        }

        if (! waveArea.isEmpty())
            drawWave (g, waveArea, colour);

        if (! specArea.isEmpty())
            drawSpectrum (g, specArea, colour);

        // (No name in the view: the PLAY card's title says OUTPUT, and the label
        // sat under the trace: V14-10.)
        IlanaTheme::paintGlassOverlay (g, bounds.reduced (6.0f), 5.0f);
    }

private:
    void drawWave (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, bool thin = false)
    {
        if (! thin)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (juce::Rectangle<float> (area.getWidth(), 1.0f).withCentre (area.getCentre()));
        }

        // Start on a rising zero crossing so the trace holds still.
        auto start = 0;
        const auto search = fftSize / 2;
        for (int i = 1; i < search; ++i)
            if (scopeL[(size_t) i - 1] <= 0.0f && scopeL[(size_t) i] > 0.0f)
            {
                start = i;
                break;
            }

        const auto span = fftSize / 4;
        const auto width = juce::jmax (2, (int) area.getWidth());
        juce::Path path;
        auto peak = 0.0f;

        for (int i = 0; i < span; ++i)
            peak = juce::jmax (peak, std::abs ((scopeL[(size_t) (start + i)] + scopeR[(size_t) (start + i)]) * 0.5f));

        const auto gain = peak > 0.02f ? juce::jlimit (0.5f, 8.0f, 0.9f / peak) : 1.0f;

        for (int x = 0; x < width; ++x)
        {
            const auto index = start + x * span / width;
            auto sample = (scopeL[(size_t) index] + scopeR[(size_t) index]) * 0.5f * gain;

            if (thin) // the header's trace is a calm line: smoothed over a few pixels' samples
            {
                auto sum = 0.0f;
                const auto taps = juce::jmax (1, 3 * span / width);
                for (int k = 0; k < taps; ++k)
                    sum += scopeL[(size_t) (index + k)] + scopeR[(size_t) (index + k)];
                sample = sum / (2.0f * (float) taps) * gain;
            }

            const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, sample) * area.getHeight() * (thin ? 0.34f : 0.46f);

            if (x == 0)
                path.startNewSubPath (area.getX(), y);
            else
                path.lineTo (area.getX() + (float) x, y);
        }

        if (! thin)
        {
            g.setColour (colour.withMultipliedAlpha (0.2f));
            g.strokePath (path, juce::PathStrokeType (4.0f));
        }
        g.setColour (colour.withMultipliedAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (thin ? 1.0f : 1.5f));
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const auto sampleRate = juce::jmax (8000.0, processorRef.getCurrentSampleRate());

        for (int i = 0; i < fftSize; ++i)
            windowBuffer[(size_t) i] = (scopeL[(size_t) i] + scopeR[(size_t) i]) * 0.5f;

        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, true);
        window.multiplyWithWindowingTable (windowBuffer.data(), (size_t) fftSize);

        for (int i = 0; i < fftSize; ++i)
            fftInput[(size_t) i] = { windowBuffer[(size_t) i], 0.0f };

        fft.perform (fftInput.data(), fftOutput.data(), false);

        // The frequency names on a band under the plot.
        const auto band = area.removeFromBottom (13.0f);
        SpectrumColumns::paintFrequencyBand (g, area, band);

        const auto width = juce::jmax (2, (int) area.getWidth());

        if ((int) smoothed.size() != width)
            smoothed.assign ((size_t) width, 0.0f);

        std::vector<float> buckets;
        SpectrumColumns::fill (fftOutput.data(), fftSize, sampleRate, buckets, width);
        auto peakNormalized = 0.0f;

        for (const auto value : buckets)
            peakNormalized = juce::jmax (peakNormalized, value);

        if (peakNormalized > 0.01f)
            gain = IlanaAnim::approach (gain, juce::jlimit (0.25f, 8.0f, 0.9f / peakNormalized), 0.12f, paintTicks);

        juce::Path fill, line;
        fill.startNewSubPath (area.getX(), area.getBottom());
        const auto usable = area.getHeight() - 2.0f;

        for (int x = 0; x < width; ++x)
        {
            const auto db = juce::Decibels::gainToDecibels (buckets[(size_t) x] * gain, -80.0f);
            const auto target = juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f);
            smoothed[(size_t) x] = IlanaAnim::approach (smoothed[(size_t) x], target, 0.4f, paintTicks);
            const auto y = area.getBottom() - 2.0f - smoothed[(size_t) x] * usable;
            fill.lineTo (area.getX() + (float) x, y);

            if (x == 0)
                line.startNewSubPath (area.getX(), y);
            else
                line.lineTo (area.getX() + (float) x, y);
        }

        fill.lineTo (area.getRight(), area.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), 0.0f, area.getY(),
                                                 colour.withAlpha (0.04f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour.withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.3f));
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        auto level = 0.0f;
        for (const auto v : smoothed)
            level = juce::jmax (level, v);

        const auto live = processorRef.getActiveVoiceCount() > 0 || processorRef.getOutputPeak() > 1.0e-5f;

        if (live || level > 0.002f || firstPaint || wasLive)
        {
            firstPaint = false;
            repaint();
        }

        wasLive = live;
    }

    static constexpr int fftSize = 2048;
    IlanaSynthAudioProcessor& processorRef;
    juce::dsp::FFT fft;
    std::vector<float> scopeL, scopeR, windowBuffer, smoothed;
    std::vector<std::complex<float>> fftInput, fftOutput;
    float gain = 1.0f;
    float paintTicks = 1.0f;
    double lastPaintMs = 0.0;
    int viewMode = 0;
    bool firstPaint = true;
    bool strip = false;
    bool wasLive = false;
};
