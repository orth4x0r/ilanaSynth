#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// A spectrum on a log-frequency axis (20 Hz to 20 kHz), one value per pixel
// column, from an FFT's bins (shared by the scope and the PLAY page's view).
// A column spanning several bins takes the loudest; below a few hundred Hz a
// column is narrower than a bin, so the value is interpolated between the two
// nearest bins and the low end draws as a curve instead of wide steps.
namespace SpectrumColumns
{
inline void fill (const std::complex<float>* bins, int fftSize, double sampleRate, std::vector<float>& out, int width)
{
    out.assign ((size_t) juce::jmax (0, width), 0.0f);
    const auto binsPerHz = (double) fftSize / sampleRate;
    const auto lastBin = fftSize / 2 - 1;

    for (int x = 0; x < width; ++x)
    {
        const auto low = 20.0 * std::pow (1000.0, (double) x / (double) width) * binsPerHz;
        const auto high = 20.0 * std::pow (1000.0, (double) (x + 1) / (double) width) * binsPerHz;
        auto magnitude = 0.0f;

        if (high - low < 1.0)
        {
            const auto centre = juce::jlimit (1.0, (double) lastBin - 1.0, 20.0 * std::pow (1000.0, ((double) x + 0.5) / (double) width) * binsPerHz);
            const auto bin = (int) centre;
            const auto fraction = (float) (centre - (double) bin);
            const auto a = std::abs (bins[bin]), b = std::abs (bins[bin + 1]);
            // Smoothstep between the bins: no corner at each bin.
            const auto t = fraction * fraction * (3.0f - 2.0f * fraction);
            magnitude = a + (b - a) * t;
        }
        else
        {
            const auto binLow = juce::jlimit (1, lastBin, (int) low);
            const auto binHigh = juce::jlimit (1, lastBin, (int) high);

            for (int bin = binLow; bin <= binHigh; ++bin)
                magnitude = juce::jmax (magnitude, std::abs (bins[bin]));
        }

        out[(size_t) x] = magnitude / (float) fftSize * 4.0f;
    }
}

// The frequency names under a spectrum, on their own band below the plot.
inline void paintFrequencyBand (juce::Graphics& g, juce::Rectangle<float> plot, juce::Rectangle<float> band)
{
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));

    for (const auto& [frequency, name] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        const auto x = plot.getX() + (float) (std::log (frequency / 20.0) / std::log (1000.0)) * plot.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight() + 3.0f).withX (x).withY (plot.getY()));
        g.setColour (IlanaTheme::Ui::text3);
        g.drawText (name, juce::Rectangle<float> (x - 20.0f, band.getY(), 40.0f, band.getHeight()).toNearestInt(),
                    juce::Justification::centred);
    }
}
} // namespace SpectrumColumns

class ScopeDisplay : public juce::Component,
                     public juce::SettableTooltipClient,
                     private IlanaAnim::FrameTimer
{
public:
    explicit ScopeDisplay (IlanaSynthAudioProcessor& processor)
        : processorRef (processor),
          fft (11)
    {
        setTooltip ("Click to cycle scope, spectrum and split view.  Click a meter (L or R) to solo that channel; "
                    "click the peak numbers or CLIP to reset them.");

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

        // View: waveform, spectrum, or both stacked.
        const char* const viewNames[] { "WAVE", "SPEC", "BOTH" };

        for (int i = 0; i < 3; ++i)
        {
            auto& button = viewButtons[(size_t) i];
            button.setButtonText (viewNames[i]);
            button.setClickingTogglesState (true);
            button.setRadioGroupId (4711);
            IlanaTheme::makePill (button, IlanaTheme::accent());
            button.onClick = [this, i]
            {
                if (viewButtons[(size_t) i].getToggleState())
                {
                    viewMode = i;
                    repaint();
                }
            };
            addAndMakeVisible (button);
        }

        viewButtons[2].setToggleState (true, juce::dontSendNotification);

        oversamplingButton.setClickingTogglesState (true);
        oversamplingButton.setTooltip ("Oversampling\nRun the voice engine at a higher sample rate: cleaner highs from "
                                       "warps, FM and screaming filters, at the cost of CPU.  Pick 2x or 4x beside it.");
        factorBox.addItemList ({ "2x", "4x" }, 1);
        factorBox.setTooltip ("Oversampling factor");
        addAndMakeVisible (factorBox);
        factorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, "os_factor", factorBox);
        addAndMakeVisible (oversamplingButton);
        oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processor.apvts, "oversampling", oversamplingButton);
        qualityBox.addItemList ({ "Eco", "Normal", "High" }, 1);
        qualityBox.setTooltip ("Quality: Eco caps unison at four voices; Normal preserves the original sound; "
                               "High uses two wavetable reads per sample for smoother highs.");
        addAndMakeVisible (qualityBox);
        qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processor.apvts, "quality", qualityBox);

        scopeSamplesL.assign (fftSize, 0.0f);
        scopeSamplesR.assign (fftSize, 0.0f);
        windowBuffer.assign (fftSize, 0.0f);
        fftInput.assign (fftSize, {});
        fftOutput.assign (fftSize, {});

        startTimerHz (30);
    }

    void resized() override
    {
        // The view on the left, the options packed from the right; neither
        // runs into the other at the panel's size.
        auto row = juce::Rectangle<int> (52, 7, getWidth() - 60, 18);

        for (int i = 0; i < 3; ++i)
            viewButtons[(size_t) i].setBounds (row.removeFromLeft (52).withTrimmedRight (i < 2 ? 4 : 0));

        row.removeFromLeft (8);
        peakButton.setBounds (row.removeFromRight (42));
        row.removeFromRight (4);
        holdButton.setBounds (row.removeFromRight (42));
        row.removeFromRight (4);
        factorBox.setBounds (row.removeFromRight (42));
        row.removeFromRight (4);
        oversamplingButton.setBounds (row.removeFromRight (32));
        row.removeFromRight (4);
        qualityBox.setBounds (row.removeFromRight (juce::jmin (86, row.getWidth())));
    }

    void paint (juce::Graphics& g) override
    {
        // Smoothing and decay steps (tuned at 30 Hz) since the last paint,
        // however long ago that was.
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            paintTicks = lastPaintMs > 0.0 ? (float) juce::jlimit (0.0, 300.0, (now - lastPaintMs) * 0.03) : 1.0f;
            lastPaintMs = now;
        }

        const auto bounds = getLocalBounds().toFloat();

        IlanaTheme::paintWell (g, bounds, 6.0f);

        auto area = bounds.reduced (10.0f);
        auto meterArea = area.removeFromRight ((float) meterWidth).withTrimmedTop (22.0f);
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
            peakHoldL = juce::jmax (peakL, IlanaAnim::decay (peakHoldL, 0.985f, paintTicks));
            peakHoldR = juce::jmax (peakR, IlanaAnim::decay (peakHoldR, 0.985f, paintTicks));
        }

        if (! hold)
        {
            heldPeak[0] = juce::jmax (heldPeak[0], peakL);
            heldPeak[1] = juce::jmax (heldPeak[1], peakR);
            clipped = clipped || peakL > 1.0f || peakR > 1.0f;
        }

        const auto peak = juce::jmax (peakL, peakR);

        if (peak > 0.004f && ! hold)
        {
            const auto targetGain = juce::jlimit (0.5f, 10.0f, 0.9f / peak);
            scopeGain = IlanaAnim::approach (scopeGain, targetGain, 0.15f, paintTicks);
        }

        area.removeFromTop (18.0f);

        if (viewMode == 1)
        {
            drawSpectrum (g, area);
        }
        else if (viewMode == 0)
        {
            drawScope (g, area);
        }
        else
        {
            auto top = area.removeFromTop (area.getHeight() * 0.52f);
            drawScope (g, top.withTrimmedBottom (6.0f));
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (area.getX(), area.getY(), area.getWidth(), 1.0f);
            drawSpectrum (g, area.withTrimmedTop (6.0f));
        }

        drawMeter (g, meterArea);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText (hold ? "HOLD" : "VIEW",
                    getLocalBounds().reduced (12, 8), juce::Justification::topLeft);

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // The clip light and peak numbers reset; a meter's bar solos its
        // channel; the main area toggles scope/spectrum.
        const auto layout = meterLayout (getLocalBounds().toFloat().reduced (10.0f).removeFromRight ((float) meterWidth).withTrimmedTop (22.0f));
        const auto position = event.position;

        if (layout.clip.getUnion (layout.numbers[0]).getUnion (layout.numbers[1]).contains (position))
        {
            resetPeaks();
            return;
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            if (layout.bars[(size_t) channel].getUnion (layout.letters[(size_t) channel]).contains (position))
            {
                channelSolo = channelSolo == channel + 1 ? 0 : channel + 1;
                repaint();
                return;
            }
        }

        if (layout.whole.contains (position))
            return;

        viewMode = (viewMode + 1) % 3;
        viewButtons[(size_t) viewMode].setToggleState (true, juce::dontSendNotification);
        repaint();
    }

    // The meters' held peaks (dB, as shown) and clip light (the UI test).
    float getHeldPeakDb (int channel) const { return juce::Decibels::gainToDecibels (heldPeak[(size_t) (channel & 1)], -100.0f); }
    bool isClipLit() const { return clipped; }

    void resetPeaks()
    {
        heldPeak = {};
        clipped = false;
        repaint();
    }

private:
    static constexpr int meterWidth = 78;

    struct MeterLayout
    {
        juce::Rectangle<float> whole, clip, scale;
        std::array<juce::Rectangle<float>, 2> numbers, bars, letters;
    };

    // The meters' column: the clip light across the top, each channel's held
    // peak in dB under it, the bars with a dB scale at their left, and L and
    // R under the bars (not on the fills).
    static MeterLayout meterLayout (juce::Rectangle<float> area)
    {
        MeterLayout layout;
        layout.whole = area;
        layout.clip = area.removeFromTop (14.0f).withTrimmedLeft (24.0f);
        area.removeFromTop (3.0f);
        auto numbers = area.removeFromTop (13.0f).withTrimmedLeft (24.0f);
        area.removeFromTop (3.0f);
        auto letters = area.removeFromBottom (13.0f).withTrimmedLeft (24.0f);
        area.removeFromBottom (2.0f);
        layout.scale = area.removeFromLeft (22.0f);
        area.removeFromLeft (2.0f);
        constexpr float gap = 4.0f;
        const auto barWidth = (area.getWidth() - gap) * 0.5f;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto x = area.getX() + (float) channel * (barWidth + gap);
            layout.bars[(size_t) channel] = { x, area.getY(), barWidth, area.getHeight() };
            layout.numbers[(size_t) channel] = { x - gap * 0.5f, numbers.getY(), barWidth + gap, numbers.getHeight() };
            layout.letters[(size_t) channel] = { x, letters.getY(), barWidth, letters.getHeight() };
        }

        return layout;
    }

    // -60 dB .. 0 dBFS onto 0..1 up a meter.
    static float meterProportion (float gain)
    {
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain, -60.0f) + 60.0f) / 60.0f);
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        if (processorRef.getActiveVoiceCount() > 0 || processorRef.getOutputPeak() > 1.0e-5f)
            lastChangeMs = juce::Time::getMillisecondCounterHiRes();

        if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
        {
            repaint();
            return;
        }

        // Peaks and the spectrum fall away after the sound stops (for a few
        // seconds at most: a host that stops processing leaves old audio).
        const auto falling = peakHoldL > 0.002f || peakHoldR > 0.002f || decaying()
                             || std::any_of (spectrumPeak.begin(), spectrumPeak.end(), [] (float v) { return v > 0.002f; });

        if (falling && ! hold && juce::Time::getMillisecondCounterHiRes() - lastChangeMs < 4000.0)
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

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        // Labels sit just inside their grid lines, clear of the title above.
        const auto label = [&] (const juce::String& text, float y)
        {
            g.drawText (text, juce::Rectangle<float> (area.getX() + 4.0f, y, 30.0f, 12.0f),
                        juce::Justification::centredLeft);
        };

        label ("+1", centreY - halfHeight + 2.0f);
        label ("0", centreY + 2.0f);
        label ("-1", centreY + halfHeight - 14.0f);

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

        // The frequency names on their own band under the plot.
        const auto band = area.removeFromBottom (14.0f);
        const auto width = (int) area.getWidth();

        if (width < 2)
            return;

        if ((int) spectrumSmoothed.size() != width)
            spectrumSmoothed.assign ((size_t) width, 0.0f);

        std::vector<float> buckets;
        SpectrumColumns::fill (fftOutput.data(), fftSize, sampleRate, buckets, width);
        auto peakNormalized = 0.0f;

        for (const auto value : buckets)
            peakNormalized = juce::jmax (peakNormalized, value);

        if (peakNormalized > 0.01f)
        {
            const auto targetGain = juce::jlimit (0.25f, 8.0f, 0.9f / peakNormalized);
            spectrumGain = IlanaAnim::approach (spectrumGain, targetGain, 0.12f, paintTicks);
        }

        for (int x = 1; x < width; ++x)
        {
            const auto db = juce::Decibels::gainToDecibels (buckets[(size_t) x] * spectrumGain, -80.0f);
            const auto target = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 84.0f);
            spectrumSmoothed[(size_t) x] = IlanaAnim::approach (spectrumSmoothed[(size_t) x], target, 0.35f, paintTicks);
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
                spectrumPeak[(size_t) x] = juce::jmax (spectrumSmoothed[(size_t) x], IlanaAnim::decay (spectrumPeak[(size_t) x], 0.985f, paintTicks));

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

        SpectrumColumns::paintFrequencyBand (g, area, band);
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto layout = meterLayout (area);
        const auto channelColour = [] (int channel) { return channel == 0 ? IlanaTheme::accent() : juce::Colour (0xff5b8cff); };
        const auto yFor = [] (juce::Rectangle<float> bar, float proportion) { return bar.getBottom() - proportion * bar.getHeight(); };

        // The clip light: red once either channel went over 0 dBFS, until
        // clicked.
        {
            const auto light = layout.clip.reduced (0.0f, 1.0f);
            g.setColour (clipped ? juce::Colour (0xffff4f5e) : IlanaTheme::Ui::raised);
            g.fillRoundedRectangle (light, 3.0f);
            g.setColour (clipped ? juce::Colours::white : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("CLIP", light, juce::Justification::centred);
        }

        // The dB scale, its ticks running faintly across both bars.
        const auto bars = layout.bars[0].getUnion (layout.bars[1]);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true));

        for (const auto db : { 0, -6, -12, -24, -36, -48 })
        {
            const auto y = yFor (bars, meterProportion (juce::Decibels::decibelsToGain ((float) db)));
            g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.22f : 0.1f));
            g.fillRect (juce::Rectangle<float> (layout.scale.getRight() - 4.0f, y - 0.5f, bars.getRight() - layout.scale.getRight() + 4.0f, 1.0f));
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (juce::String (db), juce::Rectangle<float> (layout.scale.getX(), y - 6.0f, layout.scale.getWidth() - 5.0f, 12.0f),
                        juce::Justification::centredRight);
        }

        for (int channel = 0; channel < 2; ++channel)
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
            const auto bar = layout.bars[(size_t) channel];
            const auto colour = channelColour (channel);

            g.setColour (IlanaTheme::Ui::raised.withAlpha (0.7f));
            g.fillRoundedRectangle (bar, 3.0f);

            const auto soloedOut = channelSolo != 0 && channelSolo != channel + 1;
            const auto alphaScale = soloedOut ? 0.25f : 1.0f;

            const auto rmsBar = bar.withTop (yFor (bar, meterProportion (rms)));
            g.setColour (colour.withAlpha ((channel == 0 ? 0.85f : 0.75f) * alphaScale));
            g.fillRoundedRectangle (rmsBar, 3.0f);

            g.setColour (peak > 0.99f ? juce::Colours::red : juce::Colours::white.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (bar.getWidth(), 2.0f).withCentre ({ bar.getCentreX(), yFor (bar, meterProportion (peak)) }));

            if (peakHoldEnabled)
            {
                const auto holdLevel = channel == 0 ? peakHoldL : peakHoldR;
                g.setColour (colour.withAlpha (0.6f));
                g.fillRect (juce::Rectangle<float> (bar.getWidth(), 1.0f).withCentre ({ bar.getCentreX(), yFor (bar, meterProportion (holdLevel)) }));
            }

            // The held peak in dB over the bar, and the channel's letter under it.
            {
                const auto held = heldPeak[(size_t) channel];
                const auto db = juce::Decibels::gainToDecibels (held, -100.0f);
                const auto text = held < 1.0e-5f ? juce::String ("-inf")
                                                 : (std::abs (db) < 10.0f ? juce::String (db, 1) : juce::String (juce::roundToInt (db)));
                g.setColour (held > 1.0f ? juce::Colour (0xffff4f5e) : IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, false, true));
                g.drawText (text, layout.numbers[(size_t) channel], juce::Justification::centred);
            }

            g.setColour (colour.withAlpha (soloedOut ? 0.4f : 0.9f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (channel == 0 ? "L" : "R", layout.letters[(size_t) channel], juce::Justification::centred);

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
    juce::TextButton oversamplingButton { "OS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oversamplingAttachment;
    juce::ComboBox factorBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> factorAttachment;
    juce::ComboBox qualityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;

    std::vector<float> scopeSamplesL, scopeSamplesR, windowBuffer;
    std::vector<std::complex<float>> fftInput, fftOutput;
    std::vector<float> spectrumSmoothed;
    std::vector<float> spectrumPeak;
    float scopeGain = 1.0f;
    float paintTicks = 1.0f; // smoothing steps (at 30 Hz) this paint stands for
    double lastPaintMs = 0.0, lastChangeMs = 0.0;

    // Something still falling (peaks, spectrum): keep painting until it rests.
    bool decaying() const
    {
        auto level = 0.0f;
        for (const auto v : spectrumSmoothed)
            level = juce::jmax (level, v);
        return level > 0.002f;
    }
    IlanaAnim::ChangeGate changeGate;
    float spectrumGain = 1.0f;
    float peakHoldL = 0.0f;
    float peakHoldR = 0.0f;
    std::array<float, 2> heldPeak {}; // the loudest peak since the last reset
    bool clipped = false;
    int viewMode = 2; // 0 scope, 1 spectrum, 2 both
    std::array<juce::TextButton, 3> viewButtons;
    bool hold = false;
    bool peakHoldEnabled = true;
    int channelSolo = 0;
};
