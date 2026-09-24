#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

class ScopeDisplay : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    explicit ScopeDisplay (IlanaSynthAudioProcessor& processor)
        : processorRef (processor),
          fft (11)
    {
        setTooltip ("Click to switch between scope and spectrum.  Click the L/R meters to solo a channel.");

        holdButton.setClickingTogglesState (true);
        holdButton.setTooltip ("Freeze the display");
        holdButton.onClick = [this]
        {
            hold = holdButton.getToggleState();
            repaint();
        };

        peakButton.setClickingTogglesState (true);
        peakButton.setToggleState (true, juce::dontSendNotification);
        peakButton.setTooltip ("Peak hold in the level meters");
        peakButton.onClick = [this]
        {
            peakHoldEnabled = peakButton.getToggleState();
            repaint();
        };

        addAndMakeVisible (holdButton);
        addAndMakeVisible (peakButton);

        oversamplingButton.setClickingTogglesState (true);
        oversamplingButton.setTooltip ("Run the voice engine at 2x sample rate (smoother highs, more CPU)");
        addAndMakeVisible (oversamplingButton);
        oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, "oversampling", oversamplingButton);

        scopeSamplesL.assign (fftSize, 0.0f);
        scopeSamplesR.assign (fftSize, 0.0f);
        windowBuffer.assign (fftSize, 0.0f);
        fftInput.assign (fftSize, {});
        fftOutput.assign (fftSize, {});

        startTimerHz (30);
    }

    void resized() override
    {
        oversamplingButton.setBounds (getWidth() - 168, 7, 60, 18);
        holdButton.setBounds (getWidth() - 106, 7, 48, 18);
        peakButton.setBounds (getWidth() - 54, 7, 46, 18);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        IlanaTheme::paintWell (g, bounds, 6.0f);

        auto area = bounds.reduced (10.0f);
        auto meterArea = area.removeFromRight (42.0f);
        area.removeFromRight (10.0f);

        if (! hold)
            processorRef.copyScopeData (scopeSamplesL.data(), scopeSamplesR.data(), fftSize);

        auto peakL = 0.0f;
        auto peakR = 0.0f;

        for (int i = 0; i < fftSize; ++i)
        {
            peakL = juce::jmax (peakL, std::abs (scopeSamplesL[(size_t) i]));
            peakR = juce::jmax (peakR, std::abs (scopeSamplesR[(size_t) i]));
        }

        if (! peakHoldEnabled)
        {
            peakHoldL = 0.0f;
            peakHoldR = 0.0f;
        }
        else if (! hold)
        {
            peakHoldL = juce::jmax (peakL, peakHoldL * 0.985f);
            peakHoldR = juce::jmax (peakR, peakHoldR * 0.985f);
        }

        const auto peak = juce::jmax (peakL, peakR);

        if (peak > 0.004f && ! hold)
        {
            const auto targetGain = juce::jlimit (0.5f, 10.0f, 0.9f / peak);
            scopeGain += (targetGain - scopeGain) * 0.15f;
        }

        if (spectrumMode)
            drawSpectrum (g, area);
        else
            drawScope (g, area);

        drawMeter (g, meterArea);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f, true));
        g.drawText (hold ? "SCOPE  HOLD" : (spectrumMode ? "SPECTRUM" : "SCOPE"),
                    getLocalBounds().reduced (12, 8), juce::Justification::topLeft);

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // Clicking a meter bar solos that channel; the main area toggles scope/spectrum.
        const auto meterZone = getLocalBounds().reduced (10).removeFromRight (42);

        if (meterZone.contains (event.getPosition()))
        {
            const auto half = meterZone.withHeight (meterZone.getHeight() / 2);
            const auto clicked = half.contains (event.getPosition()) ? 1 : 2;
            channelSolo = (channelSolo == clicked) ? 0 : clicked;
            repaint();
            return;
        }

        spectrumMode = ! spectrumMode;
        repaint();
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    void drawScope (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto centreY = area.getCentreY();
        const auto halfHeight = area.getHeight() * 0.46f;

        // Grid with dB labels.
        for (const auto level : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
        {
            const auto y = centreY - level * halfHeight;
            const auto isCentre = std::abs (level) < 0.01f;

            g.setColour (juce::Colours::white.withAlpha (isCentre ? 0.12f : 0.05f));
            g.fillRect (juce::Rectangle<float> (area.getWidth(), 1.0f).withCentre ({ area.getCentreX(), y }));
        }

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.setFont (IlanaTheme::font (10.0f));
        g.drawText ("+1", area.withHeight (12.0f).withX (area.getX() + 4.0f), juce::Justification::centredLeft);
        g.drawText ("0", area.withHeight (12.0f).withY (centreY - 6.0f).withX (area.getX() + 4.0f),
                    juce::Justification::centredLeft);
        g.drawText ("-1", area.withHeight (12.0f).withY (area.getBottom() - 12.0f).withX (area.getX() + 4.0f),
                    juce::Justification::centredLeft);

        auto start = 0;

        for (int i = 1; i < fftSize / 2; ++i)
        {
            if (scopeSamplesL[(size_t) (i - 1)] < 0.0f && scopeSamplesL[(size_t) i] >= 0.0f)
            {
                start = i;
                break;
            }
        }

        const auto visible = juce::jmin (fftSize - start, (int) area.getWidth() * 2);

        for (int channel = 0; channel < 2; ++channel)
        {
            if (channelSolo != 0 && channelSolo != channel + 1)
                continue;

            const auto* samples = channel == 0 ? scopeSamplesL.data() : scopeSamplesR.data();

            juce::Path path;

            for (int i = 0; i < visible; ++i)
            {
                const auto x = area.getX() + (float) i / (float) visible * area.getWidth();
                const auto y = centreY - samples[(size_t) (start + i)] * scopeGain * halfHeight;

                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            const auto colour = channel == 0 ? IlanaTheme::accent() : juce::Colour (0xff5b8cff);

            g.setColour (colour.withAlpha (0.14f));
            g.strokePath (path, juce::PathStrokeType (4.0f));

            g.setColour (colour.withAlpha (channel == 0 ? 0.95f : 0.85f));
            g.strokePath (path, juce::PathStrokeType (1.4f));
        }
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto sampleRate = juce::jmax (8000.0, processorRef.getCurrentSampleRate());

        for (int i = 0; i < fftSize; ++i)
        {
            const auto left = scopeSamplesL[(size_t) i];
            const auto right = scopeSamplesR[(size_t) i];

            if (channelSolo == 1)
                windowBuffer[(size_t) i] = left;
            else if (channelSolo == 2)
                windowBuffer[(size_t) i] = right;
            else
                windowBuffer[(size_t) i] = (left + right) * 0.5f;
        }

        juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                    juce::dsp::WindowingFunction<float>::hann, true);
        window.multiplyWithWindowingTable (windowBuffer.data(), (size_t) fftSize);

        for (int i = 0; i < fftSize; ++i)
            fftInput[(size_t) i] = { windowBuffer[(size_t) i], 0.0f };

        fft.perform (fftInput.data(), fftOutput.data(), false);

        const auto width = (int) area.getWidth();

        if (width < 2)
            return;

        if ((int) spectrumSmoothed.size() != width)
            spectrumSmoothed.assign ((size_t) width, 0.0f);

        std::vector<float> buckets ((size_t) width, 0.0f);
        auto peakNormalized = 0.0f;

        for (int x = 1; x < width; ++x)
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
            spectrumGain += (targetGain - spectrumGain) * 0.12f;
        }

        for (int x = 1; x < width; ++x)
        {
            const auto db = juce::Decibels::gainToDecibels (buckets[(size_t) x] * spectrumGain, -80.0f);
            const auto target = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 84.0f);
            spectrumSmoothed[(size_t) x] += (target - spectrumSmoothed[(size_t) x]) * 0.35f;
        }

        juce::Path path;
        path.startNewSubPath (area.getX(), area.getBottom());

        for (int x = 1; x < width; ++x)
        {
            const auto y = area.getBottom() - spectrumSmoothed[(size_t) x] * area.getHeight();
            path.lineTo (area.getX() + (float) x, y);
        }

        path.closeSubPath();

        juce::ColourGradient fillGradient (IlanaTheme::accent().withAlpha (0.45f), 0.0f, area.getY(),
                                           IlanaTheme::accent().withAlpha (0.03f), 0.0f, area.getBottom(), false);
        g.setGradientFill (fillGradient);
        g.fillPath (path);

        // Peak-hold spectrum outline with slow decay.
        if ((int) spectrumPeak.size() != width)
            spectrumPeak.assign ((size_t) width, 0.0f);

        if (! hold)
            for (int x = 1; x < width; ++x)
                spectrumPeak[(size_t) x] = juce::jmax (spectrumSmoothed[(size_t) x], spectrumPeak[(size_t) x] * 0.985f);

        juce::Path peakPath;
        peakPath.startNewSubPath (area.getX(), area.getBottom() - spectrumPeak[0] * area.getHeight());

        for (int x = 1; x < width; ++x)
        {
            const auto y = area.getBottom() - spectrumPeak[(size_t) x] * area.getHeight();
            peakPath.lineTo (area.getX() + (float) x, y);
        }

        g.setColour (IlanaTheme::accent().withAlpha (0.35f));
        g.strokePath (peakPath, juce::PathStrokeType (1.0f));

        juce::Path outline;
        outline.startNewSubPath (area.getX(), area.getBottom() - spectrumSmoothed[0] * area.getHeight());

        for (int x = 1; x < width; ++x)
        {
            const auto y = area.getBottom() - spectrumSmoothed[(size_t) x] * area.getHeight();
            outline.lineTo (area.getX() + (float) x, y);
        }

        g.setColour (IlanaTheme::accent().withAlpha (0.14f));
        g.strokePath (outline, juce::PathStrokeType (3.5f));

        g.setColour (IlanaTheme::accent());
        g.strokePath (outline, juce::PathStrokeType (1.4f));

        // Frequency ruler.
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.setFont (IlanaTheme::font (10.0f));

        const std::pair<double, const char*> marks[] { { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } };

        for (const auto& mark : marks)
        {
            const auto proportion = std::log (mark.first / 20.0) / std::log (1000.0);
            const auto x = area.getX() + (float) proportion * area.getWidth();

            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (1.0f, area.getHeight()).withX (x));

            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawText (mark.second, juce::Rectangle<float> (x + 3.0f, area.getBottom() - 12.0f, 40.0f, 12.0f)
                                          .toNearestInt(),
                        juce::Justification::centredLeft);
        }
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto channels = 2;
        const auto gap = 6.0f;
        const auto barWidth = (area.getWidth() - gap) * 0.5f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto* samples = channel == 0 ? scopeSamplesL.data() : scopeSamplesR.data();

            auto sumSquares = 0.0f;
            auto peak = 0.0f;

            for (int i = 0; i < fftSize; ++i)
            {
                const auto value = samples[(size_t) i];
                sumSquares += value * value;
                peak = juce::jmax (peak, std::abs (value));
            }

            const auto rms = std::sqrt (sumSquares / (float) fftSize);
            const auto rmsLevel = juce::jlimit (0.0f, 1.0f,
                                                (juce::Decibels::gainToDecibels (rms, -60.0f) + 60.0f) / 60.0f);
            const auto peakLevel = juce::jlimit (0.0f, 1.0f,
                                                 (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f);

            const juce::Rectangle<float> bar (area.getX() + (float) channel * (barWidth + gap),
                                              area.getY(), barWidth, area.getHeight());

            g.setColour (juce::Colour (0xff1b1b20));
            g.fillRoundedRectangle (bar, 3.0f);

            const auto soloedOut = channelSolo != 0 && channelSolo != channel + 1;
            const auto alphaScale = soloedOut ? 0.25f : 1.0f;

            const auto rmsBar = bar.withTrimmedTop (bar.getHeight() * (1.0f - rmsLevel));
            g.setColour (channel == 0 ? IlanaTheme::accent().withAlpha (0.85f * alphaScale)
                                      : juce::Colour (0xff5b8cff).withAlpha (0.75f * alphaScale));
            g.fillRoundedRectangle (rmsBar, 3.0f);

            const auto peakY = bar.getBottom() - peakLevel * bar.getHeight();
            g.setColour (peak > 0.99f ? juce::Colours::red : juce::Colours::white.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (bar.getWidth(), 2.0f).withCentre ({ bar.getCentreX(), peakY }));

            if (peakHoldEnabled)
            {
                const auto holdLevel = channel == 0 ? peakHoldL : peakHoldR;
                const auto holdY = bar.getBottom() - juce::jlimit (0.0f, 1.0f, holdLevel) * bar.getHeight();
                g.setColour ((channel == 0 ? IlanaTheme::accent()
                                           : juce::Colour (0xff5b8cff)).withAlpha (0.45f));
                g.fillRect (juce::Rectangle<float> (bar.getWidth(), 1.0f).withCentre ({ bar.getCentreX(), holdY }));
            }

            g.setColour (channel == 0 ? IlanaTheme::accent().withAlpha (0.7f)
                                      : juce::Colour (0xff5b8cff).withAlpha (0.7f));
            g.setFont (IlanaTheme::font (10.5f));
            g.drawText (channel == 0 ? "L" : "R",
                        juce::Rectangle<float> (bar.getX(), bar.getBottom() - 12.0f, bar.getWidth(), 12.0f),
                        juce::Justification::centred);

            if (channelSolo == channel + 1)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawRoundedRectangle (bar.reduced (0.5f), 3.0f, 1.4f);
            }
        }
    }

    static constexpr int fftSize = 2048;

    IlanaSynthAudioProcessor& processorRef;
    juce::dsp::FFT fft;
    juce::TextButton holdButton { "HOLD" };
    juce::TextButton peakButton { "PEAK" };
    juce::TextButton oversamplingButton { "2x OS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oversamplingAttachment;

    std::vector<float> scopeSamplesL, scopeSamplesR, windowBuffer;
    std::vector<std::complex<float>> fftInput, fftOutput;
    std::vector<float> spectrumSmoothed;
    std::vector<float> spectrumPeak;
    float scopeGain = 1.0f;
    float spectrumGain = 1.0f;
    float peakHoldL = 0.0f;
    float peakHoldR = 0.0f;
    bool spectrumMode = false;
    bool hold = false;
    bool peakHoldEnabled = true;
    int channelSolo = 0;
};
