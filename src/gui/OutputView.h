#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

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

    void mouseDown (const juce::MouseEvent&) override
    {
        viewMode = (viewMode + 1) % 3;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            paintTicks = lastPaintMs > 0.0 ? (float) juce::jlimit (0.0, 300.0, (now - lastPaintMs) * 0.03) : 1.0f;
            lastPaintMs = now;
        }

        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintCard (g, bounds, 6.0f, IlanaTheme::accent());
        IlanaTheme::paintWell (g, bounds.reduced (6.0f), 5.0f);

        processorRef.copyScopeData (scopeL.data(), scopeR.data(), fftSize);
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

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("OUTPUT", bounds.reduced (14.0f, 8.0f).removeFromTop (12.0f), juce::Justification::topLeft);
        IlanaTheme::paintGlassOverlay (g, bounds.reduced (6.0f), 5.0f);
    }

private:
    void drawWave (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (area.getWidth(), 1.0f).withCentre (area.getCentre()));

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
            const auto sample = (scopeL[(size_t) index] + scopeR[(size_t) index]) * 0.5f * gain;
            const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, sample) * area.getHeight() * 0.46f;

            if (x == 0)
                path.startNewSubPath (area.getX(), y);
            else
                path.lineTo (area.getX() + (float) x, y);
        }

        g.setColour (colour.withAlpha (0.2f));
        g.strokePath (path, juce::PathStrokeType (4.0f));
        g.setColour (colour.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.5f));
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

        // The frequency grid, named.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));

        for (const auto& [frequency, name] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
        {
            const auto x = area.getX() + (float) (std::log (frequency / 20.0) / std::log (1000.0)) * area.getWidth();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (juce::Rectangle<float> (1.0f, area.getHeight()).withX (x).withY (area.getY()));
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (name, juce::Rectangle<float> (x + 3.0f, area.getBottom() - 12.0f, 30.0f, 11.0f),
                        juce::Justification::centredLeft);
        }

        const auto width = juce::jmax (2, (int) area.getWidth());

        if ((int) smoothed.size() != width)
            smoothed.assign ((size_t) width, 0.0f);

        std::vector<float> buckets ((size_t) width, 0.0f);
        auto peakNormalized = 0.0f;

        for (int x = 0; x < width; ++x)
        {
            const auto low = 20.0 * std::pow (1000.0, (double) x / (double) width);
            const auto high = 20.0 * std::pow (1000.0, (double) (x + 1) / (double) width);
            const auto binLow = juce::jlimit (1, fftSize / 2 - 1, (int) (low * (double) fftSize / sampleRate));
            const auto binHigh = juce::jlimit (1, fftSize / 2 - 1, (int) (high * (double) fftSize / sampleRate));
            auto magnitude = 0.0f;

            for (int bin = binLow; bin <= binHigh; ++bin)
                magnitude = juce::jmax (magnitude, std::abs (fftOutput[(size_t) bin]));

            buckets[(size_t) x] = magnitude / (float) fftSize * 4.0f;
            peakNormalized = juce::jmax (peakNormalized, buckets[(size_t) x]);
        }

        if (peakNormalized > 0.01f)
            gain = IlanaAnim::approach (gain, juce::jlimit (0.25f, 8.0f, 0.9f / peakNormalized), 0.12f, paintTicks);

        juce::Path fill, line;
        fill.startNewSubPath (area.getX(), area.getBottom());
        const auto usable = area.getHeight() - 14.0f;

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

        if (live || level > 0.002f || firstPaint)
        {
            firstPaint = false;
            repaint();
        }
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
};
