#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/FilterUnit.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class FilterDisplay : public juce::Component,
                      public juce::SettableTooltipClient,
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

        setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
        setTooltip ("Drag a marker (or anywhere on the graph): left / right sets the cutoff, up / down the resonance. "
                    "The solid curve and markers are the settings; the faint ones follow the modulation.");
        startTimerHz (30);
    }

    ~FilterDisplay() override { endGestures(); }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

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

        // The frequency axis, named as on the scope.
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto& [frequency, name] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
        {
            const auto x = plot.getX() + (float) frequencyToX (frequency) * plot.getWidth();
            g.drawText (name, juce::Rectangle<float> (x + 3.0f, bounds.getBottom() - 13.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
        }
        g.setColour (juce::Colours::white.withAlpha (0.07f));

        g.setColour (juce::Colours::white.withAlpha (0.05f));

        for (const auto db : { -36.0, -24.0, -12.0, 0.0, 12.0 })
        {
            const auto y = plot.getY() + (float) dbToY (db) * plot.getHeight();
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withY (y));
        }

        drawSpectrum (g, plot);

        // The modulated response, faint, under the set one (UI review 4,
        // V23 and S4): the curve and markers you drag stay where the knobs
        // are, and modulation shows as a moving shadow.
        if (isModulated (0) || isModulated (1))
        {
            drawCurve (g, plot, 1, filterColour (1), true);
            drawCurve (g, plot, 0, filterColour (0), true);
        }

        drawCurve (g, plot, 1, filterColour (1), false);
        drawCurve (g, plot, 0, filterColour (0), false);

        const auto markers = markerCentres (plot);

        for (int filterIndex = 1; filterIndex >= 0; --filterIndex)
            if (isModulated (filterIndex))
                drawModMarker (g, plot, filterIndex, markers[(size_t) filterIndex], filterColour (filterIndex));

        drawMarker (g, plot, markers[1], 1, filterColour (1));
        drawMarker (g, plot, markers[0], 0, filterColour (0));

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    // Where the two set-value markers are drawn (and grabbed): inside the
    // plot, and fanned apart when they would overlap (both at 20 kHz on
    // Init). Component coordinates; the UI test reads them.
    std::array<juce::Point<float>, 2> getMarkerCentres() const { return markerCentres (plotBounds()); }

    // The filter a click here drags: the nearest set marker, else the
    // nearest cutoff across, so a click anywhere picks a filter.
    int filterAt (juce::Point<float> position) const
    {
        const auto markers = getMarkerCentres();
        auto best = -1;
        auto bestDistance = 1.0e9f;

        for (int filterIndex = 0; filterIndex < 2; ++filterIndex)
        {
            const auto marker = markers[(size_t) filterIndex];
            const auto distance = marker.getDistanceFrom (position) < 24.0f
                                      ? marker.getDistanceFrom (position)
                                      : 24.0f + std::abs (marker.x - position.x);

            if (distance < bestDistance - 0.5f)
            {
                bestDistance = distance;
                best = filterIndex;
            }
        }

        return best;
    }

private:
    static constexpr float markerSize = 10.0f;

    static juce::Colour filterColour (int filterIndex) { return filterIndex == 0 ? juce::Colour (0xffc86bff) : juce::Colour (0xff8f9dff); }

    juce::Rectangle<float> plotBounds() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f); }

    // A marker's place before it is kept inside the plot: x from the cutoff
    // (as set, or with the live modulation), y from the resonance.
    juce::Point<float> rawMarker (juce::Rectangle<float> plot, int filterIndex, bool withMod) const
    {
        return { plot.getX() + (float) frequencyToX (cutoffOf (filterIndex, withMod)) * plot.getWidth(),
                 resoToY (plot, readParam (filterIndex == 0 ? "f1_reso" : "f2_reso")) };
    }

    std::array<juce::Point<float>, 2> markerCentres (juce::Rectangle<float> plot) const
    {
        const auto inside = plot.reduced (markerSize * 0.5f + 1.0f);
        std::array<juce::Point<float>, 2> centres;

        for (int filterIndex = 0; filterIndex < 2; ++filterIndex)
        {
            const auto raw = rawMarker (plot, filterIndex, false);
            centres[(size_t) filterIndex] = { juce::jlimit (inside.getX(), inside.getRight(), raw.x),
                                              juce::jlimit (inside.getY(), inside.getBottom(), raw.y) };
        }

        // Overlapping markers fan apart across (filter 1 on the left when
        // the cutoffs are equal), the pair kept inside the plot.
        auto& a = centres[0];
        auto& b = centres[1];
        const auto gap = markerSize + 3.0f;

        if (std::abs (a.x - b.x) < gap && std::abs (a.y - b.y) < gap)
        {
            const auto firstLeft = cutoffOf (0, false) <= cutoffOf (1, false);
            const auto middle = juce::jlimit (inside.getX() + gap * 0.5f, inside.getRight() - gap * 0.5f, (a.x + b.x) * 0.5f);
            a.x = middle + (firstLeft ? -gap : gap) * 0.5f;
            b.x = middle + (firstLeft ? gap : -gap) * 0.5f;
        }

        return centres;
    }

    bool isModulated (int filterIndex) const
    {
        return std::abs (cutoffOf (filterIndex, true) / cutoffOf (filterIndex, false) - 1.0) > 0.005
               || std::abs (processorRef.getModDisplay (filterIndex == 0 ? Mod::Destination::Filter1Morph
                                                                         : Mod::Destination::Filter2Morph)) > 0.002f;
    }

    void timerCallback() override
    {
        // The glow pulses only while notes sound.
        if (processorRef.getActiveVoiceCount() > 0)
            pulse += 0.09f * frameTicks();
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        const auto falling = decaying() && juce::Time::getMillisecondCounterHiRes() - lastLiveMs < 4000.0;

        if (processorRef.getActiveVoiceCount() > 0 || processorRef.getOutputPeak() > 1.0e-5f)
            lastLiveMs = juce::Time::getMillisecondCounterHiRes();

        if (isShowing() && (appear < 1.0f || falling || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
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

    // The cutoff as set, or with the live modulation on top.
    double cutoffOf (int filterIndex, bool withMod) const
    {
        const auto base = (double) readParam (filterIndex == 0 ? "f1_cutoff" : "f2_cutoff");
        const auto mod = withMod ? (double) processorRef.getModDisplay (
                                       filterIndex == 0 ? Mod::Destination::Filter1Cutoff : Mod::Destination::Filter2Cutoff)
                                 : 0.0;

        return juce::jlimit (20.0, 20000.0, base * std::exp2 (mod * 6.0));
    }

    std::complex<double> response (int filterIndex, double frequency, bool withMod) const
    {
        const auto cutoff = cutoffOf (filterIndex, withMod);
        const auto reso = (double) readParam (filterIndex == 0 ? "f1_reso" : "f2_reso");
        const auto type = (int) readParam (filterIndex == 0 ? "f1_type" : "f2_type");
        const auto slope24 = readParam (filterIndex == 0 ? "f1_slope" : "f2_slope") > 0.5f;

        const auto morph = (double) readParam (filterIndex == 0 ? "f1_morph" : "f2_morph")
                           + (withMod ? (double) processorRef.getModDisplay (filterIndex == 0 ? Mod::Destination::Filter1Morph
                                                                                              : Mod::Destination::Filter2Morph)
                                      : 0.0);

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

    // The response as set (solid, glowing), or with the modulation (faint).
    void drawCurve (juce::Graphics& g, juce::Rectangle<float> plot, int filterIndex, juce::Colour colour, bool withMod)
    {
        const auto width = juce::jmax (2, (int) plot.getWidth());
        const auto parallel = readParam ("filters_parallel") > 0.5f;

        juce::Path path;

        for (int x = 0; x < width; ++x)
        {
            const auto frequency = xToFrequency ((double) x / (double) (width - 1));

            auto h = response (filterIndex, frequency, withMod);

            if (parallel)
            {
                h = (response (0, frequency, withMod) + response (1, frequency, withMod)) * 0.707;
            }
            else if (filterIndex == 1)
            {
                h = response (0, frequency, withMod) * response (1, frequency, withMod);
            }

            const auto db = 20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (h)));
            const auto y = plot.getY() + (float) dbToY (db) * plot.getHeight();
            const auto px = plot.getX() + (float) x;

            if (x == 0)
                path.startNewSubPath (px, y);
            else
                path.lineTo (px, y);
        }

        if (withMod)
        {
            g.setColour (colour.withAlpha (parallel ? 0.22f : 0.3f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
            return;
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

    // The set value: a solid, numbered marker (the one you drag; its height
    // is the resonance), with a faint line at its cutoff to the curve's
    // corner.
    void drawMarker (juce::Graphics& g, juce::Rectangle<float> plot, juce::Point<float> centre, int filterIndex, juce::Colour colour) const
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withX (centre.x - 0.5f).withY (plot.getY()));

        const auto dot = juce::Rectangle<float> (markerSize, markerSize).withCentre (centre);
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.8f));
        g.fillEllipse (dot.expanded (1.5f));
        g.setColour (colour);
        g.fillEllipse (dot);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (juce::String (filterIndex + 1), dot, juce::Justification::centred);
    }

    // Where the modulation has the cutoff right now: a faint ring on the set
    // marker's line, tied to it by a thin stroke.
    void drawModMarker (juce::Graphics& g, juce::Rectangle<float> plot, int filterIndex, juce::Point<float> setCentre,
                        juce::Colour colour) const
    {
        const auto inside = plot.reduced (markerSize * 0.5f + 1.0f);
        const juce::Point<float> live (juce::jlimit (inside.getX(), inside.getRight(), rawMarker (plot, filterIndex, true).x),
                                       setCentre.y);

        if (live.getDistanceFrom (setCentre) < 2.0f)
            return;

        g.setColour (colour.withAlpha (0.35f));
        g.drawLine ({ setCentre, live }, 1.0f);
        g.setColour (colour.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (markerSize - 1.0f, markerSize - 1.0f).withCentre (live), 1.3f);
    }

    // Drag anywhere on the graph: the nearest filter's marker follows the
    // mouse, left / right for the cutoff and up / down for the resonance,
    // as one gesture per parameter (so a host records one undo step and
    // automation writes a clean move).
    void mouseDown (const juce::MouseEvent& event) override
    {
        endGestures();
        draggingFilter = filterAt (event.position);
        dragOffset = {};

        if (draggingFilter >= 0)
        {
            // A grabbed marker moves on from where it is (one drawn inset at
            // the plot's edge, or fanned apart, doesn't jump on the click);
            // a click elsewhere brings the cutoff to the mouse.
            if (getMarkerCentres()[(size_t) draggingFilter].getDistanceFrom (event.position) < markerSize)
                dragOffset = rawMarker (plotBounds(), draggingFilter, false) - event.position;

            processorRef.beginEdit ("Filter " + juce::String (draggingFilter + 1) + " graph");
            beginGestures (draggingFilter);
            applyDrag (event.position);
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (draggingFilter >= 0)
            applyDrag (event.position);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endGestures();
        processorRef.endEdit();
    }

    void applyDrag (juce::Point<float> mouse)
    {
        const auto plot = plotBounds();
        const auto position = mouse + dragOffset;
        const auto proportion = (double) juce::jlimit (0.0f, 1.0f, (position.x - plot.getX()) / plot.getWidth());
        const auto frequency = juce::jlimit (20.0, 20000.0, xToFrequency (proportion));
        const auto reso = yToReso (plot, position.y);

        setParameter (draggingFilter == 0 ? "f1_cutoff" : "f2_cutoff", (float) frequency);
        setParameter (draggingFilter == 0 ? "f1_reso" : "f2_reso", reso);
    }

    void beginGestures (int filterIndex)
    {
        for (const auto* id : { filterIndex == 0 ? "f1_cutoff" : "f2_cutoff", filterIndex == 0 ? "f1_reso" : "f2_reso" })
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                openGestures.push_back (parameter);
            }
    }

    void endGestures()
    {
        for (auto* parameter : openGestures)
            parameter->endChangeGesture();

        openGestures.clear();
    }

    void setParameter (const char* id, float plainValue)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    std::vector<juce::RangedAudioParameter*> openGestures;

    IlanaSynthAudioProcessor& processorRef;
    int draggingFilter = -1;
    juce::Point<float> dragOffset; // a grabbed marker's set place minus the mouse
    float pulse = 0.0f;
    float paintTicks = 1.0f; // smoothing steps (at 30 Hz) this paint stands for
    double lastPaintMs = 0.0, lastLiveMs = 0.0;

    // Something still falling (peaks, spectrum): keep painting until it rests.
    bool decaying() const
    {
        auto level = 0.0f;
        for (const auto v : spectrumSmoothed)
            level = juce::jmax (level, v);
        return level > 0.002f;
    }
    IlanaAnim::ChangeGate changeGate;
    float appear = 1.0f;

    static constexpr int fftSize = 2048;
    juce::dsp::FFT fft { 11 };
    std::vector<float> scopeL, scopeR, windowBuffer, spectrumSmoothed;
    std::vector<std::complex<float>> fftInput, fftOutput;
    float spectrumGain = 1.0f;
};
