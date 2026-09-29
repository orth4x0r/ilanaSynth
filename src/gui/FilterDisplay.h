#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/FilterUnit.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class FilterDisplay : public juce::Component,
                      public IlanaAnim::PageAnimated,
                      private IlanaAnim::FrameTimer
{
public:
    explicit FilterDisplay (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        scopeL.assign (fftSize, 0.0f);
        scopeR.assign (fftSize, 0.0f);
        windowBuffer.assign (fftSize, 0.0f);
        fftInput.assign (fftSize, {});
        fftOutput.assign (fftSize, {});

        startTimerHz (30);
    }

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

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto plot = bounds.reduced (10.0f, 12.0f);

        g.setColour (juce::Colours::white.withAlpha (0.07f));

        for (const auto frequency : { 100.0, 1000.0, 10000.0 })
        {
            const auto x = plot.getX() + (float) frequencyToX (frequency) * plot.getWidth();
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withX (x));
        }

        g.setColour (juce::Colours::white.withAlpha (0.05f));

        for (const auto db : { -36.0, -24.0, -12.0, 0.0, 12.0 })
        {
            const auto y = plot.getY() + (float) dbToY (db) * plot.getHeight();
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withY (y));
        }

        drawSpectrum (g, plot);
        drawCurve (g, plot, 1, juce::Colour (0xffb28aff));
        drawCurve (g, plot, 0, juce::Colour (0xffff4fd8));
        drawMarker (g, plot, 1, juce::Colour (0xffb28aff));
        drawMarker (g, plot, 0, juce::Colour (0xffff4fd8));

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

private:
    void timerCallback() override
    {
        paintTicks = frameTicks();
        pulse += 0.09f * paintTicks;
        appear = juce::jmin (1.0f, appear + 0.12f * paintTicks);

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    static double frequencyToX (double frequency)
    {
        return std::log (frequency / 20.0) / std::log (1000.0);
    }

    static double xToFrequency (double proportion)
    {
        return 20.0 * std::pow (1000.0, proportion);
    }

    static double dbToY (double db)
    {
        return juce::jlimit (0.0, 1.0, (24.0 - db) / 72.0);
    }

    float readParam (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    double cutoffWithMod (int filterIndex) const
    {
        const auto base = (double) readParam (filterIndex == 0 ? "f1_cutoff" : "f2_cutoff");
        const auto mod = (double) processorRef.getModDisplay (
            filterIndex == 0 ? Mod::Destination::Filter1Cutoff : Mod::Destination::Filter2Cutoff);

        return juce::jlimit (20.0, 20000.0, base * std::exp2 (mod * 6.0));
    }

    std::complex<double> response (int filterIndex, double frequency) const
    {
        const auto cutoff = cutoffWithMod (filterIndex);
        const auto reso = (double) readParam (filterIndex == 0 ? "f1_reso" : "f2_reso");
        const auto type = (int) readParam (filterIndex == 0 ? "f1_type" : "f2_type");
        const auto slope24 = readParam (filterIndex == 0 ? "f1_slope" : "f2_slope") > 0.5f;

        const auto morph = (double) readParam (filterIndex == 0 ? "f1_morph" : "f2_morph")
                           + (double) processorRef.getModDisplay (filterIndex == 0 ? Mod::Destination::Filter1Morph
                                                                                    : Mod::Destination::Filter2Morph);

        return FilterType::response (type, slope24, reso, std::complex<double> (0.0, frequency / cutoff),
                                     juce::jlimit (0.0, 1.0, morph), cutoff);
    }

    // Marker height <-> resonance, shared by drawing and dragging so the
    // marker stays under the mouse.
    static float resoToY (juce::Rectangle<float> plot, float reso)
    {
        return plot.getBottom() - (0.08f + juce::jlimit (0.0f, 1.0f, reso) * 0.8f) * plot.getHeight();
    }

    static float yToReso (juce::Rectangle<float> plot, float y)
    {
        return juce::jlimit (0.0f, 1.0f, ((plot.getBottom() - y) / plot.getHeight() - 0.08f) / 0.8f);
    }

    void drawCurve (juce::Graphics& g, juce::Rectangle<float> plot, int filterIndex, juce::Colour colour)
    {
        const auto width = juce::jmax (2, (int) plot.getWidth());
        const auto parallel = readParam ("filters_parallel") > 0.5f;

        juce::Path path;

        for (int x = 0; x < width; ++x)
        {
            const auto frequency = xToFrequency ((double) x / (double) (width - 1));

            auto h = response (filterIndex, frequency);

            if (parallel)
            {
                h = (response (0, frequency) + response (1, frequency)) * 0.707;
            }
            else if (filterIndex == 1)
            {
                h = response (0, frequency) * response (1, frequency);
            }

            const auto db = 20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (h)));
            const auto y = plot.getY() + (float) dbToY (db) * plot.getHeight();
            const auto px = plot.getX() + (float) x;

            if (x == 0)
                path.startNewSubPath (px, y);
            else
                path.lineTo (px, y);
        }

        g.setColour (colour.withAlpha ((parallel ? 0.16f : 0.2f) + 0.05f * (0.5f + 0.5f * std::sin (pulse))));
        g.strokePath (path, juce::PathStrokeType (parallel ? 4.0f : 5.0f));

        g.setColour (colour.withAlpha (parallel ? 0.55f : 0.95f));
        g.strokePath (path, juce::PathStrokeType (parallel ? 1.2f : 1.8f));
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        processorRef.copyScopeData (scopeL.data(), scopeR.data(), fftSize);
        const auto sampleRate = juce::jmax (8000.0, processorRef.getCurrentSampleRate());

        for (int i = 0; i < fftSize; ++i)
            windowBuffer[(size_t) i] = (scopeL[(size_t) i] + scopeR[(size_t) i]) * 0.5f;

        juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                    juce::dsp::WindowingFunction<float>::hann, true);
        window.multiplyWithWindowingTable (windowBuffer.data(), (size_t) fftSize);

        for (int i = 0; i < fftSize; ++i)
            fftInput[(size_t) i] = { windowBuffer[(size_t) i], 0.0f };

        fft.perform (fftInput.data(), fftOutput.data(), false);

        const auto width = juce::jmax (2, (int) plot.getWidth());

        if ((int) spectrumSmoothed.size() != width)
            spectrumSmoothed.assign ((size_t) width, 0.0f);

        juce::Path path;
        path.startNewSubPath (plot.getX(), plot.getBottom());

        std::vector<float> buckets ((size_t) width, 0.0f);
        auto peakNormalized = 0.0f;

        for (int x = 0; x < width; ++x)
        {
            const auto frequencyLow = 20.0 * std::pow (1000.0, (double) x / (double) width);
            const auto frequencyHigh = 20.0 * std::pow (1000.0, (double) (x + 1) / (double) width);
            const auto binLow = juce::jlimit (1, fftSize / 2 - 1, (int) (frequencyLow * (double) fftSize / sampleRate));
            const auto binHigh = juce::jlimit (1, fftSize / 2 - 1, (int) (frequencyHigh * (double) fftSize / sampleRate));

            auto magnitude = 0.0f;

            for (int bin = binLow; bin <= binHigh; ++bin)
                magnitude = juce::jmax (magnitude, std::abs (fftOutput[(size_t) bin]));

            const auto normalized = magnitude / (float) fftSize * 4.0f;
            buckets[(size_t) x] = normalized;
            peakNormalized = juce::jmax (peakNormalized, normalized);
        }

        if (peakNormalized > 0.01f)
        {
            const auto targetGain = juce::jlimit (0.25f, 8.0f, 0.9f / peakNormalized);
            spectrumGain = IlanaAnim::approach (spectrumGain, targetGain, 0.12f, paintTicks);
        }

        for (int x = 0; x < width; ++x)
        {
            const auto db = juce::Decibels::gainToDecibels (buckets[(size_t) x] * spectrumGain, -80.0f);
            const auto target = juce::jlimit (0.0f, 0.42f, (db + 80.0f) / 84.0f * 0.42f);

            spectrumSmoothed[(size_t) x] = IlanaAnim::approach (spectrumSmoothed[(size_t) x], target, 0.4f, paintTicks);
            path.lineTo (plot.getX() + (float) x, plot.getBottom() - spectrumSmoothed[(size_t) x] * plot.getHeight());
        }

        path.lineTo (plot.getRight(), plot.getBottom());
        path.closeSubPath();

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillPath (path);
    }

    void drawMarker (juce::Graphics& g, juce::Rectangle<float> plot, int filterIndex, juce::Colour colour)
    {
        const auto cutoff = cutoffWithMod (filterIndex);
        const auto reso = readParam (filterIndex == 0 ? "f1_reso" : "f2_reso");

        const auto x = plot.getX() + (float) frequencyToX (cutoff) * plot.getWidth();
        const auto y = resoToY (plot, reso);

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x, y }));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (juce::String (filterIndex + 1), juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x, y }),
                    juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        draggingFilter = -1;

        const auto bounds = getLocalBounds().toFloat();
        const auto plot = bounds.reduced (10.0f, 12.0f);
        auto bestDistance = 24.0f;

        for (int filterIndex = 0; filterIndex < 2; ++filterIndex)
        {
            const auto cutoff = cutoffWithMod (filterIndex);
            const auto reso = readParam (filterIndex == 0 ? "f1_reso" : "f2_reso");
            const juce::Point<float> marker (plot.getX() + (float) frequencyToX (cutoff) * plot.getWidth(),
                                             resoToY (plot, reso));
            const auto distance = marker.getDistanceFrom (event.position);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                draggingFilter = filterIndex;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (draggingFilter < 0)
            return;

        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
        const auto proportion = (double) juce::jlimit (0.0f, 1.0f,
                                                       (event.position.x - plot.getX()) / plot.getWidth());
        const auto frequency = juce::jlimit (20.0, 20000.0, xToFrequency (proportion));
        const auto reso = yToReso (plot, event.position.y);

        setParameter (draggingFilter == 0 ? "f1_cutoff" : "f2_cutoff", (float) frequency);
        setParameter (draggingFilter == 0 ? "f1_reso" : "f2_reso", reso);
    }

    void setParameter (const char* id, float plainValue)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    IlanaSynthAudioProcessor& processorRef;
    int draggingFilter = -1;
    float pulse = 0.0f;
    float paintTicks = 1.0f; // smoothing steps (at 30 Hz) the next paint stands for
    IlanaAnim::ChangeGate changeGate;
    float appear = 1.0f;

    static constexpr int fftSize = 2048;
    juce::dsp::FFT fft { 11 };
    std::vector<float> scopeL, scopeR, windowBuffer, spectrumSmoothed;
    std::vector<std::complex<float>> fftInput, fftOutput;
    float spectrumGain = 1.0f;
};
