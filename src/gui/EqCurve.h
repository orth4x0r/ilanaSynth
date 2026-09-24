#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

// Response curve for the EQ module. Each band has a handle: drag
// sideways for frequency and up/down for gain.
class EqCurve : public juce::Component,
                public juce::SettableTooltipClient,
                private juce::Timer
{
public:
    explicit EqCurve (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("EQ\nDrag a point sideways for frequency, up or down for gain.  Double-click a point to flatten it.");
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto plot = plotArea();

        g.setColour (juce::Colours::white.withAlpha (0.06f));

        for (const auto db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), db == 0.0f ? 1.2f : 1.0f).withPosition (plot.getX(), dbToY (db)));

        for (const auto hz : { 100.0f, 1000.0f, 10000.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (frequencyToX (hz), plot.getY()));

        const auto settings = processorRef.getEqSettings();
        Biquad::Coefficients bands[3];
        settings.makeCoefficients (sampleRate, bands);

        juce::Path curve;

        for (int x = 0; x <= (int) plot.getWidth(); x += 2)
        {
            const auto hz = xToFrequency (plot.getX() + (float) x);
            auto magnitude = 1.0;

            for (const auto& band : bands)
                magnitude *= band.magnitude (hz, sampleRate);

            const auto y = dbToY ((float) juce::Decibels::gainToDecibels (magnitude));

            if (x == 0)
                curve.startNewSubPath (plot.getX() + (float) x, y);
            else
                curve.lineTo (plot.getX() + (float) x, y);
        }

        auto fill = curve;
        fill.lineTo (plot.getRight(), dbToY (0.0f));
        fill.lineTo (plot.getX(), dbToY (0.0f));
        fill.closeSubPath();

        g.setColour (IlanaTheme::accent().withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (IlanaTheme::accent());
        g.strokePath (curve, juce::PathStrokeType (1.8f));

        const char* const labels[] { "LOW", "MID", "HIGH" };

        for (int band = 0; band < 3; ++band)
        {
            const auto handle = handlePosition (settings, band);
            const auto active = band == dragBand || band == hoverBand;

            g.setColour (IlanaTheme::accent().withAlpha (active ? 0.35f : 0.18f));
            g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (handle));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (handle));

            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.setFont (IlanaTheme::font (9.5f, true));
            g.drawText (labels[band], juce::Rectangle<float> (40.0f, 12.0f).withCentre (handle.translated (0.0f, -15.0f)),
                        juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto band = bandAt (event.position);

        if (band != hoverBand)
        {
            hoverBand = band;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragBand = bandAt (event.position);

        if (dragBand >= 0)
            for (auto* parameter : bandParameters (dragBand))
                if (parameter != nullptr)
                    parameter->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragBand < 0)
            return;

        const auto parameters = bandParameters (dragBand);
        const auto frequency = xToFrequency (juce::jlimit (plotArea().getX(), plotArea().getRight(), event.position.x));
        const auto gain = yToDb (juce::jlimit (plotArea().getY(), plotArea().getBottom(), event.position.y));

        if (parameters[0] != nullptr)
            parameters[0]->setValueNotifyingHost (parameters[0]->convertTo0to1 (frequency));

        if (parameters[1] != nullptr)
            parameters[1]->setValueNotifyingHost (parameters[1]->convertTo0to1 (gain));

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand >= 0)
            for (auto* parameter : bandParameters (dragBand))
                if (parameter != nullptr)
                    parameter->endChangeGesture();

        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto band = bandAt (event.position);

        if (band >= 0)
            if (auto* gain = bandParameters (band)[1])
                gain->setValueNotifyingHost (gain->convertTo0to1 (0.0f));
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr float maxDb = 18.0f;

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 8.0f); }

    float frequencyToX (float hz) const
    {
        const auto plot = plotArea();
        return plot.getX() + plot.getWidth() * (float) (std::log (hz / 20.0) / std::log (1000.0));
    }

    double xToFrequency (float x) const
    {
        const auto plot = plotArea();
        return 20.0 * std::pow (1000.0, (double) ((x - plot.getX()) / plot.getWidth()));
    }

    float dbToY (float db) const
    {
        const auto plot = plotArea();
        return plot.getCentreY() - juce::jlimit (-maxDb, maxDb, db) / maxDb * plot.getHeight() * 0.5f;
    }

    float yToDb (float y) const
    {
        const auto plot = plotArea();
        return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * maxDb;
    }

    juce::Point<float> handlePosition (const EqSettings& settings, int band) const
    {
        const float frequencies[] { settings.lowFreq, settings.midFreq, settings.highFreq };
        const float gains[] { settings.lowGain, settings.midGain, settings.highGain };
        return { frequencyToX (frequencies[band]), dbToY (gains[band]) };
    }

    int bandAt (juce::Point<float> position) const
    {
        const auto settings = processorRef.getEqSettings();

        for (int band = 0; band < 3; ++band)
            if (handlePosition (settings, band).getDistanceFrom (position) < 12.0f)
                return band;

        return -1;
    }

    std::array<juce::RangedAudioParameter*, 2> bandParameters (int band) const
    {
        const char* const frequencyIds[] { "fx_eq_low_freq", "fx_eq_mid_freq", "fx_eq_high_freq" };
        const char* const gainIds[] { "fx_eq_low_gain", "fx_eq_mid_gain", "fx_eq_high_gain" };
        return { processorRef.apvts.getParameter (frequencyIds[band]), processorRef.apvts.getParameter (gainIds[band]) };
    }

    void timerCallback() override
    {
        const auto settings = processorRef.getEqSettings();
        const auto signature = settings.lowFreq + settings.lowGain * 3.1f + settings.midFreq * 0.7f + settings.midGain * 5.3f
                               + settings.midQ * 11.0f + settings.highFreq * 0.3f + settings.highGain * 7.7f;

        if (signature != lastSignature)
        {
            lastSignature = signature;
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    int dragBand = -1;
    int hoverBand = -1;
    float lastSignature = 0.0f;
};
