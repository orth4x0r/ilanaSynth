#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/EpTuning.h"

#include <cmath>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/SampleFactory.h"
#include "../dsp/Voice.h"
#include "IlanaLookAndFeel.h"

class WaveDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer
{
public:
    WaveDisplay (IlanaSynthAudioProcessor& processor,
                 juce::String tableIdIn,
                 juce::String frameIdIn,
                 juce::String unisonIdIn,
                 juce::String spreadIdIn,
                 juce::String detuneIdIn,
                 bool subTableMappingIn = false,
                 juce::String shapeIdIn = {},
                 juce::String modeIdIn = {},
                 int oscIndexIn = 0,
                 juce::Colour traceColourIn = IlanaTheme::accent(),
                 bool followsThemeIn = false)
        : processorRef (processor),
          tableId (std::move (tableIdIn)),
          frameId (std::move (frameIdIn)),
          unisonId (std::move (unisonIdIn)),
          spreadId (std::move (spreadIdIn)),
          detuneId (std::move (detuneIdIn)),
          shapeId (std::move (shapeIdIn)),
          modeId (std::move (modeIdIn)),
          oscIndex (oscIndexIn),
          traceColour (traceColourIn),
          followsTheme (followsThemeIn),
          subTableMapping (subTableMappingIn)
    {
        const juce::String samplePrefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]);
        startId = samplePrefix + "_sample_start";
        endId = samplePrefix + "_sample_end";
        fadeInId = samplePrefix + "_sample_fade_in";
        fadeOutId = samplePrefix + "_sample_fade_out";
        reverseId = samplePrefix + "_sample_reverse";
        loopId = samplePrefix + "_sample_loop";

        displayedFrame = readValue (frameId);

        setTooltip ("Drag to scrub the frame, click 3D to toggle the waterfall view");

        modeButton.setClickingTogglesState (true);
        modeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d1d22));
        modeButton.setColour (juce::TextButton::buttonOnColourId,
                              (followsTheme ? IlanaTheme::accent() : traceColour).withAlpha (0.8f));
        modeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.6f));
        modeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        modeButton.onClick = [this]
        {
            threeD = modeButton.getToggleState();
            repaint();
        };

        addAndMakeVisible (modeButton);

        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto borderColour = sampleDragHover ? traceColour : juce::Colour (0xff2a2a31);
        const auto borderThickness = sampleDragHover ? 2.0f : 1.0f;

        if (isLiveInput())
        {
            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness);
            const auto level = processorRef.getInputLevel();
            auto plot = bounds.reduced (14.0f, 12.0f);
            g.setColour (traceColour);
            g.setFont (IlanaTheme::font (15.0f, true));
            g.drawText (readChoice (modeId) == 4 ? "LIVE INPUT" : "LIVE GRAINS", plot.removeFromTop (22.0f).toNearestInt(),
                        juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (IlanaTheme::font (11.5f));
            g.drawText (IlanaSynthAudioProcessor::isEffectBuild ? "the audio coming into ilanaSynth FX"
                                                                : "needs ilanaSynth FX (the effect plugin)",
                        plot.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft);
            auto meter = plot.removeFromBottom (10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (meter, 3.0f);
            const auto db = level > 1.0e-5f ? juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (level) / 60.0f) : 0.0f;
            g.setColour (traceColour.withAlpha (0.85f));
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * db), 3.0f);
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        if (! isSampleMode())
        {
            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness);
        }
        else
        {
            if (loadFlash > 0.01f)
            {
                g.setColour (traceColour.withAlpha (0.25f * loadFlash));
                g.fillRoundedRectangle (bounds, 6.0f);
            }

            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness + loadFlash * 1.5f);
            drawSample (g);
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        if (isPhysicalString())
        {
            drawString (g, bounds.reduced (12.0f));
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        if (isElectricPiano())
        {
            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness);
            drawPickup (g, bounds.reduced (12.0f));
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        const auto tableIndex = resolveTableIndex();
        const auto frame = displayedFrame;
        const auto warped = processorRef.getSpectralDisplayTable (oscIndex, tableIndex);
        const auto* table = warped != nullptr ? warped.get() : processorRef.getWavetable (tableIndex);

        if (table == nullptr || table->getNumFrames() == 0)
        {
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        const auto plot = bounds.reduced (10.0f);

        if (threeD)
        {
            drawWaterfall (g, table, frame, plot);
        }
        else
        {
            const auto centreY = plot.getCentreY();
            const auto halfHeight = plot.getHeight() * 0.44f;

            drawFrame (g, table, 0, plot, centreY, halfHeight, juce::Colours::white.withAlpha (0.12f), 1.0f);

            const auto frameCount = table->getNumFrames();
            const auto framePosition = frame * (float) (frameCount - 1);
            const auto frameIndex = juce::jlimit (0, frameCount - 1, (int) std::round (framePosition));

            // With a warp (or the PD chain) on, the trace is the warped cycle
            // at the knob settings, over a faint copy of the plain frame.
            if (hasWarp())
            {
                drawFrame (g, table, frameIndex, plot, centreY, halfHeight, traceColour.withAlpha (0.25f), 1.0f);
                drawWarpedFrame (g, table, frameIndex, plot, centreY, halfHeight);
            }
            else
            {
                drawFrame (g, table, frameIndex, plot, centreY, halfHeight, traceColour, 1.6f);
            }

            const auto markerX = plot.getX() + plot.getWidth() * frame;
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ markerX, centreY }));

            drawUnison (g, plot);
            drawPlayhead (g, table, frameIndex, plot, centreY, halfHeight);
        }

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
        {
            if (isGranularMode())
                setPositionFromX (event.position.x);

            return;
        }

        juce::PopupMenu menu;
        menu.addSectionHeader ("Factory Samples");

        const auto paramId = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)])
                             + "_sample_factory";
        const auto current = (int) readPlain (paramId);

        for (int i = 0; i < SampleFactory::getNumFactorySamples(); ++i)
            menu.addItem (i + 1, SampleFactory::getFactorySampleName (i), true, current == i + 1);

        juce::Component::SafePointer<WaveDisplay> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis, paramId] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                if (auto* parameter = safeThis->processorRef.apvts.getParameter (paramId))
                                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) result));

                                safeThis->switchToSampleMode();
                            });
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (isGranularMode())
            setPositionFromX (event.position.x);

        if (isSampleMode() || isPhysicalString() || isElectricPiano() || isLiveInput())
            return;

        setFrameFromX (event.position.x);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        if (files.size() != 1)
            return false;

        const auto extension = juce::File (files[0]).getFileExtension().toLowerCase();

        return extension == ".wav" || extension == ".aif" || extension == ".aiff"
               || extension == ".flac" || extension == ".ogg" || extension == ".mp3" || extension == ".m4a";
    }

    void fileDragEnter (const juce::StringArray&, int, int) override
    {
        sampleDragHover = true;
        repaint();
    }

    void fileDragExit (const juce::StringArray&) override
    {
        sampleDragHover = false;
        repaint();
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        sampleDragHover = false;

        if (files.size() != 1)
            return;

        const juce::File file (files[0]);

        if (! file.existsAsFile())
            return;

        if (processorRef.loadUserSample (oscIndex, file))
            switchToSampleMode();

        repaint();
    }

    void resized() override
    {
        modeButton.setBounds (getWidth() - 44, 6, 36, 18);
    }

private:
    bool isSampleMode() const
    {
        const auto mode = modeId.isNotEmpty() ? readChoice (modeId) : 0;
        return mode == 2 || mode == 3;
    }

    // M7.5: the Live mode, or live grains, play the audio input.
    bool isLiveInput() const
    {
        if (modeId.isEmpty())
            return false;
        const auto mode = readChoice (modeId);
        return mode == 4 || (mode == 3 && readChoice (modeId.upToLastOccurrenceOf ("_mode", false, false) + "_grain_live") > 0);
    }

    // A Physical oscillator's string (the Tine and Reed excites show their
    // pickup instead): it has no wavetable to show.
    bool isPhysicalString() const
    {
        return modeId.isNotEmpty() && readChoice (modeId) == 1 && ! isElectricPiano();
    }

    // The string at rest after a strike: its first modes, weighted by where it
    // is struck (EXCITE POS; Auto is about an eighth of the way along).
    void drawString (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto prefix = modeId.upToLastOccurrenceOf ("_mode", false, false);
        static const char* const excites[] { "BURST", "NOISE", "SAW", "PULSE", "BOW", "HAMMER", "OSC IN", "TINE", "REED", "PIANO HAMMER", "FEEDBACK" };
        const auto excite = juce::jlimit (0, 10, readChoice (prefix + "_excite"));
        const auto position = readPlain (prefix + "_string_excite_pos");
        const auto strike = position > 0.005f ? juce::jlimit (0.02f, 0.5f, position) : 0.125f;

        g.setColour (traceColour);
        g.setFont (IlanaTheme::font (12.5f, true));
        auto header = area.removeFromTop (18.0f);
        g.drawText ("STRING", header.toNearestInt(), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (10.5f));
        g.drawText (juce::String (excites[excite]).toLowerCase() + " at " + (position > 0.005f ? juce::String (juce::roundToInt (strike * 100.0f)) + " %" : juce::String ("auto")),
                    header.toNearestInt(), juce::Justification::centredRight);

        auto footer = area.removeFromBottom (16.0f);
        g.drawText ("the PHYSICAL tab shows it moving", footer.toNearestInt(), juce::Justification::centredLeft);

        const auto left = area.getX() + 8.0f, right = area.getRight() - 8.0f, mid = area.getCentreY() + area.getHeight() * 0.18f;
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillRoundedRectangle (left - 4.0f, mid - 12.0f, 4.0f, 24.0f, 1.5f);
        g.fillRoundedRectangle (right, mid - 9.0f, 5.0f, 18.0f, 1.5f);

        const auto heightAt = [strike] (double x)
        {
            auto y = 0.0;
            for (int n = 1; n <= 12; ++n)
                y += std::sin (juce::MathConstants<double>::pi * n * strike) * std::sin (juce::MathConstants<double>::pi * n * x) / (n * n);
            return y;
        };
        const auto scale = area.getHeight() * 0.55f / (float) juce::jmax (0.05, heightAt (strike));
        juce::Path string;
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = (double) i / 120.0;
            const auto px = left + (right - left) * (float) x, py = mid - (float) heightAt (x) * scale;
            if (i == 0) string.startNewSubPath (px, py); else string.lineTo (px, py);
        }
        g.setColour (traceColour.withAlpha (0.25f));
        g.strokePath (string, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.setColour (traceColour);
        g.strokePath (string, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ left + (right - left) * strike, mid - (float) heightAt (strike) * scale }));
    }

    // M7.3: a Physical oscillator on the Tine or Reed excite shows its pickup.
    bool isElectricPiano() const
    {
        if (modeId.isEmpty() || readChoice (modeId) != 1)
            return false;
        return readChoice (modeId.upToLastOccurrenceOf ("_mode", false, false) + "_excite") >= 7;
    }

    // The pickup's response across the tine's (or reed's) swing, as the
    // model computes it (ElectricPiano), with the swing of a medium and a
    // hard note marked: the further the swing reaches over the curve's
    // bends, the more it barks or growls.
    void drawPickup (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto prefix = modeId.upToLastOccurrenceOf ("_mode", false, false);
        const auto tine = readChoice (prefix + "_excite") == 7;
        const auto& t = tine ? EpTuning::get().tine : EpTuning::get().reed;
        const auto distance = t.distance * std::pow (2.0f, (readPlain (prefix + "_ep_distance") - 0.5f) * 4.0f);
        const auto offset = (t.offset + t.offsetRange * (readPlain (prefix + "_ep_position") - 0.5f)) * distance;
        const auto pickup = [tine, distance, offset] (float x)
        {
            if (tine)
            {
                const auto u = (x - offset) / distance;
                return 1.0f / (1.0f + u * u);
            }
            auto z = (x + offset) / distance;
            if (z > 0.8f)
                z = 0.8f + 0.17f * std::tanh ((z - 0.8f) / 0.17f);
            return 1.0f / (1.0f - z);
        };
        const auto swing = 0.5f * (t.ampLow + t.ampHigh);
        const auto range = swing * 1.6f;

        g.setColour (traceColour);
        g.setFont (IlanaTheme::font (12.5f, true));
        auto header = area.removeFromTop (18.0f);
        g.drawText (tine ? "TINE PICKUP" : "REED PICKUP", header.toNearestInt(), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (10.5f));
        g.drawText (tine ? "flux vs tine position" : "charge vs reed position", header.toNearestInt(),
                    juce::Justification::centredRight);
        area.removeFromTop (4.0f);

        auto low = 1.0e9f, high = -1.0e9f;
        constexpr int points = 160;
        std::array<float, points + 1> values {};
        for (int i = 0; i <= points; ++i)
        {
            values[(size_t) i] = pickup (-range + 2.0f * range * (float) i / (float) points);
            low = juce::jmin (low, values[(size_t) i]);
            high = juce::jmax (high, values[(size_t) i]);
        }
        const auto toX = [area, range] (float x) { return area.getX() + (x + range) / (2.0f * range) * area.getWidth(); };

        // The swing of a medium and of a hard note, centred on rest.
        for (const auto& [reach, alpha] : { std::pair<float, float> { swing * 0.35f, 0.16f }, { swing, 0.08f } })
        {
            g.setColour (traceColour.withAlpha (alpha));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (toX (-reach), area.getY(), toX (reach), area.getBottom()));
        }
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawVerticalLine ((int) toX (0.0f), area.getY(), area.getBottom());

        juce::Path path;
        for (int i = 0; i <= points; ++i)
        {
            const auto x = area.getX() + area.getWidth() * (float) i / (float) points;
            const auto y = area.getBottom() - (values[(size_t) i] - low) / juce::jmax (1.0e-6f, high - low) * area.getHeight() * 0.9f;
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        g.setColour (traceColour);
        g.strokePath (path, juce::PathStrokeType (1.8f));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("rest", juce::Rectangle<float> (toX (0.0f) + 4.0f, area.getBottom() - 14.0f, 40.0f, 14.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    bool isGranularMode() const
    {
        return modeId.isNotEmpty() && readChoice (modeId) == 3;
    }

    // Picking or dropping a sample switches a wavetable or string oscillator
    // to Sample; a granular one stays granular.
    void switchToSampleMode()
    {
        if (modeId.isEmpty() || isSampleMode())
            return;

        if (auto* parameter = processorRef.apvts.getParameter (modeId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (2.0f));
    }

    void setPositionFromX (float x)
    {
        const auto plotWidth = (float) juce::jmax (1, getWidth() - 20);

        if (auto* parameter = processorRef.apvts.getParameter (startId))
            parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (x - 10.0f) / plotWidth));
    }

    void drawGrainCloud (juce::Graphics& g, juce::Rectangle<float> plot, float centreY, float halfHeight) const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]);
        const auto position = juce::jlimit (0.0f, 1.0f, readPlain (startId));
        const auto spray = readPlain (prefix + "_grain_spray");
        const auto density = readPlain (prefix + "_grain_density");
        const auto size = readPlain (prefix + "_grain_size");
        const auto positionX = plot.getX() + position * plot.getWidth();
        const auto halfSpray = spray * 0.25f * plot.getWidth();

        // The window grains are drawn from.
        const auto band = juce::Rectangle<float> (positionX - halfSpray, plot.getY(), halfSpray * 2.0f, plot.getHeight())
                              .getIntersection (plot);
        g.setColour (traceColour.withAlpha (0.12f));
        g.fillRect (band);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (plot.withRight (band.getX()));
        g.fillRect (plot.withLeft (band.getRight()));

        // Grains drifting through it: each lives for a moment, then respawns.
        const auto sounding = processorRef.getActiveVoiceCount() > 0;
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto count = 6 + (int) (density * 22.0f);
        const auto grainWidth = juce::jlimit (3.0f, 26.0f, size / 500.0f * plot.getWidth() * 0.25f + 3.0f);

        for (int i = 0; i < count; ++i)
        {
            const auto speed = 0.6 + 0.9 * (double) ((i * 37) % 11) / 11.0;
            const auto life = now * speed + (double) i * 0.37;
            const auto cycle = (int) std::floor (life);
            const auto age = (float) (life - (double) cycle);
            const auto hash = (float) ((cycle * 7919 + i * 104729) % 1000) / 1000.0f;
            const auto hashY = (float) ((cycle * 3571 + i * 6007) % 1000) / 1000.0f;
            const auto x = positionX + (hash * 2.0f - 1.0f) * halfSpray + age * grainWidth * 0.5f;
            const auto y = centreY + (hashY * 2.0f - 1.0f) * halfHeight * 0.8f;
            const auto alpha = std::sin (age * juce::MathConstants<float>::pi) * (sounding ? 0.9f : 0.35f);

            g.setColour (traceColour.withAlpha (alpha * 0.5f));
            g.fillRoundedRectangle (juce::Rectangle<float> (grainWidth, 5.0f).withCentre ({ x, y }), 2.5f);
            g.setColour (juce::Colours::white.withAlpha (alpha * 0.8f));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, y }));
        }

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ positionX, centreY }));
        g.setColour (traceColour);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ positionX, plot.getY() + 4.0f }));
    }

    void drawSample (juce::Graphics& g) const
    {
        const auto* sample = processorRef.getSampleForOsc (oscIndex);
        const auto plot = getLocalBounds().toFloat().reduced (10.0f);

        if (sample == nullptr || sample->getNumSamples() < 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (12.5f));
            g.drawFittedText ("DROP A SAMPLE HERE", getLocalBounds().reduced (12), juce::Justification::centred, 2);
            return;
        }

        const auto start = juce::jlimit (0.0f, 1.0f, readPlain (startId));
        const auto end = juce::jlimit (0.0f, 1.0f, readPlain (endId));
        const auto fadeIn = juce::jlimit (0.0f, 1.0f, readPlain (fadeInId));
        const auto fadeOut = juce::jlimit (0.0f, 1.0f, readPlain (fadeOutId));
        const auto reverse = readPlain (reverseId) > 0.5f;
        const auto loop = readPlain (loopId) > 0.5f;

        const auto numSamples = sample->getNumSamples();
        const auto width = juce::jmax (2, (int) plot.getWidth());
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.42f;

        if (cachedSample != sample || (int) cachedPeaks.size() != width)
        {
            cachedSample = sample;
            cachedPeaks.assign ((size_t) width, 0.0f);

            for (int x = 0; x < width; ++x)
            {
                const auto from = (int) ((float) x / (float) width * (float) numSamples);
                const auto to = juce::jmin (numSamples, (int) ((float) (x + 1) / (float) width * (float) numSamples) + 1);
                auto peak = 0.0f;

                for (int i = from; i < to; ++i)
                {
                    const auto value = sample->buffer.getSample (0, i)
                                       + (sample->getNumChannels() > 1 ? sample->buffer.getSample (1, i) : 0.0f);
                    peak = juce::jmax (peak, std::abs (value * 0.5f));
                }

                cachedPeaks[(size_t) x] = peak;
            }
        }

        juce::Path path;

        for (int x = 0; x < width; ++x)
        {
            const auto px = plot.getX() + (float) x;
            const auto py = centreY - cachedPeaks[(size_t) x] * halfHeight;

            if (x == 0)
                path.startNewSubPath (px, py);
            else
                path.lineTo (px, py);
        }

        g.setColour (traceColour.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        juce::Path mirrored;

        for (int x = 0; x < width; ++x)
        {
            const auto px = plot.getX() + (float) x;
            const auto py = centreY + cachedPeaks[(size_t) x] * halfHeight;

            if (x == 0)
                mirrored.startNewSubPath (px, py);
            else
                mirrored.lineTo (px, py);
        }

        g.setColour (traceColour.withAlpha (0.35f));
        g.strokePath (mirrored, juce::PathStrokeType (1.0f));

        if (isGranularMode())
        {
            drawGrainCloud (g, plot, centreY, halfHeight);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (IlanaTheme::font (10.5f, true));
            const auto wide = getWidth() > 240;
            g.drawText ((reverse ? "REV " : "") + juce::String (wide ? "GRAINS - drag to move" : "GRAINS"),
                        getLocalBounds().reduced (8, 6), juce::Justification::bottomLeft);
            g.drawText (sample->name, getLocalBounds().reduced (8, 6), juce::Justification::bottomRight);
            return;
        }

        const auto startX = plot.getX() + start * plot.getWidth();
        const auto endX = plot.getX() + end * plot.getWidth();

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY(), juce::jmax (0.0f, startX - plot.getX()), plot.getHeight()));
        g.fillRect (juce::Rectangle<float> (endX, plot.getY(), juce::jmax (0.0f, plot.getRight() - endX), plot.getHeight()));

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ startX, centreY }));
        g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ endX, centreY }));

        if (fadeIn > 0.001f)
        {
            juce::Path fadePath;
            fadePath.startNewSubPath (startX, centreY + halfHeight);
            fadePath.lineTo (startX + fadeIn * (endX - startX), centreY - halfHeight);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.strokePath (fadePath, juce::PathStrokeType (1.0f));
        }

        if (fadeOut > 0.001f)
        {
            juce::Path fadePath;
            fadePath.startNewSubPath (endX - fadeOut * (endX - startX), centreY - halfHeight);
            fadePath.lineTo (endX, centreY + halfHeight);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.strokePath (fadePath, juce::PathStrokeType (1.0f));
        }

        const auto playPosition = processorRef.getSamplePosition (oscIndex);

        if (playPosition >= 0.0f)
        {
            const auto cursorX = plot.getX() + playPosition * plot.getWidth();
            const auto readIndex = juce::jlimit (0, numSamples - 1, (int) (playPosition * (float) numSamples));
            const auto readValue = sample->buffer.getSample (0, readIndex)
                                   + (sample->getNumChannels() > 1 ? sample->buffer.getSample (1, readIndex) : 0.0f);
            const auto cursorY = juce::jlimit (plot.getY(), plot.getBottom(),
                                               centreY - readValue * 0.5f * halfHeight);

            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withCentre ({ cursorX, centreY }));

            const auto pulse = 0.6f + 0.4f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);
            g.setColour (traceColour.withAlpha (0.22f * pulse));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ cursorX, cursorY }));

            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre ({ cursorX, cursorY }));
        }

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (IlanaTheme::font (10.5f, true));
        g.drawText ((reverse ? "REV " : "") + juce::String (loop ? "LOOP" : "1-SHOT"),
                    getLocalBounds().reduced (8, 6), juce::Justification::bottomLeft);
        g.drawText (sample->name, getLocalBounds().reduced (8, 6), juce::Justification::bottomRight);
    }

    int resolveTableIndex() const
    {
        const auto choice = readChoice (tableId);

        if (! subTableMapping)
            return choice;

        if (choice == 0)
        {
            const auto shape = (int) readPlain (shapeId);
            return shape == 0 ? 8 : (shape == 1 ? 6 : 10);
        }

        if (choice <= 3)
            return choice == 1 ? 8 : (choice == 2 ? 6 : 10);

        return choice - 4;
    }

    void drawUnison (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const auto unison = juce::jlimit (1, VoiceParams::maxUnison, (int) readPlain (unisonId));
        const auto spread = readPlain (spreadId);
        const auto detune = readPlain (detuneId);
        const auto y = plot.getBottom() - 5.0f;
        const auto spreadWidth = plot.getWidth() * 0.35f * (0.25f + spread * 0.75f);

        for (int u = 0; u < unison; ++u)
        {
            const auto offset = unison > 1 ? ((float) u / (float) (unison - 1) * 2.0f - 1.0f) : 0.0f;
            const auto x = plot.getCentreX() + offset * spreadWidth;
            const auto size = 4.0f + juce::jmin (6.0f, detune * 0.2f);

            g.setColour (traceColour.withAlpha (0.35f + 0.4f * (1.0f - std::abs (offset))));
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
        }
    }

    float readPlain (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void setFrameFromX (float x)
    {
        const auto plotWidth = (float) juce::jmax (1, getWidth() - 20);
        const auto proportion = juce::jlimit (0.0f, 1.0f, (x - 10.0f) / plotWidth);

        if (auto* parameter = processorRef.apvts.getParameter (frameId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (proportion));
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
        {
            traceColour = IlanaTheme::accent();
            modeButton.setColour (juce::TextButton::buttonOnColourId, traceColour.withAlpha (0.8f));
        }
    }

    void timerCallback() override
    {
        // The 3D waterfall only applies to tables.
        modeButton.setVisible (! isSampleMode() && ! isElectricPiano() && ! isLiveInput() && ! isPhysicalString());
        setTooltip (isPhysicalString() ? "The string after a strike, from where it is struck (EXCITE POS). The PHYSICAL tab shows it moving." : isElectricPiano() ? "The pickup's response across the swing: the shaded bands are a medium and a hard note. "
                                        "A swing that reaches over the bends barks (tine) or growls (reed). DISTANCE and OFFSET move them."
                    : isLiveInput() ? "The audio coming into ilanaSynth FX."
                    : isGranularMode() ? "Grains are read from around the white line: drag to move it. Right-click for factory samples, or drop a wav."
                    : isSampleMode() ? "Showing the loaded sample. Drop a new wav here to replace it."
                                   : "Drag to scrub the frame, click 3D to toggle the waterfall view. Drop a wav to switch this oscillator to Sample.");

        const auto* sample = isSampleMode() ? processorRef.getSampleForOsc (oscIndex) : nullptr;

        if (sample != lastSample)
        {
            lastSample = sample;
            loadFlash = sample != nullptr ? 1.0f : 0.0f;
        }

        loadFlash *= 0.93f;

        // Follow the frame parameter including any modulation (LFO, envelope,
        // macros), smoothed so morphs glide rather than jump.
        displayedFrame += (resolvedFrame() - displayedFrame) * 0.25f;

        if (! isShowing())
            return;

        repaint();
    }

    float resolvedFrame() const
    {
        auto frame = readValue (frameId);
        const auto destination = frameDestination();

        if (destination != Mod::Destination::None)
            frame = juce::jlimit (0.0f, 1.0f, frame + processorRef.getModDisplay (destination));

        return frame;
    }

    Mod::Destination frameDestination() const
    {
        if (frameId == "osc1_frame") return Mod::Destination::Osc1Frame;
        if (frameId == "osc2_frame") return Mod::Destination::Osc2Frame;
        if (frameId == "sub_frame") return Mod::Destination::SubFrame;

        return Mod::Destination::None;
    }

    void drawPlayhead (juce::Graphics& g, const Wavetable* table, int frameIndex,
                       juce::Rectangle<float> plot, float centreY, float halfHeight) const
    {
        // Driven by the phase of the voice that is actually sounding, so the
        // playhead stops when nothing is playing instead of free running.
        if (processorRef.getActiveVoiceCount() <= 0)
            return;

        const auto phase = juce::jlimit (0.0f, 1.0f, processorRef.getWavetablePhase (oscIndex));
        const auto cursorX = plot.getX() + phase * plot.getWidth();
        const auto sampleIndex = juce::jlimit (1, Wavetable::frameSize,
                                               (int) (phase * (float) Wavetable::frameSize) + 1);
        const auto* data = table->getFrameData (0, frameIndex);
        const auto cursorY = centreY - data[sampleIndex] * halfHeight;

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withCentre ({ cursorX, centreY }));

        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ cursorX, cursorY }));
    }

    void drawWaterfall (juce::Graphics& g, const Wavetable* table, float frame, juce::Rectangle<float> plot) const
    {
        const auto numFrames = table->getNumFrames();
        const auto currentFrame = juce::jlimit (0, numFrames - 1,
                                                (int) std::round (frame * (float) (numFrames - 1)));

        for (int f = numFrames - 1; f >= 0; --f)
        {
            const auto depth = numFrames > 1 ? (float) f / (float) (numFrames - 1) : 0.0f;
            const auto inset = plot.getWidth() * 0.08f * depth;
            const auto baseline = plot.getBottom() - 6.0f - depth * plot.getHeight() * 0.62f;
            const auto amplitude = plot.getHeight() * 0.15f * (1.0f - depth * 0.35f);
            const auto* data = table->getFrameData (0, f);
            const auto highlight = f == currentFrame;

            const auto width = juce::jmax (2, (int) (plot.getWidth() - inset * 2.0f));
            const auto step = juce::jmax (1, Wavetable::frameSize / width);
            juce::Path path;

            for (int x = 0; x < width; x += 2)
            {
                const auto sampleIndex = juce::jlimit (1, Wavetable::frameSize, x * step + 1);
                const auto sample = data[sampleIndex];
                const auto px = plot.getX() + inset + (float) x;
                const auto py = baseline - sample * amplitude;

                if (x == 0)
                    path.startNewSubPath (px, py);
                else
                    path.lineTo (px, py);
            }

            if (highlight)
                g.setColour (traceColour.withAlpha (0.95f));
            else
                g.setColour (juce::Colours::white.withAlpha (0.14f));

            g.strokePath (path, juce::PathStrokeType (highlight ? 1.6f : 1.0f));
        }
    }

    int readChoice (const juce::String& id) const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (id)))
            return param->getIndex();

        return 0;
    }

    float readValue (const juce::String& id) const
    {
        if (auto* param = processorRef.apvts.getParameter (id))
            return param->getValue();

        return 0.0f;
    }

    void drawFrame (juce::Graphics& g, const Wavetable* table, int frameIndex, juce::Rectangle<float> plot,
                    float centreY, float halfHeight, juce::Colour colour, float thickness) const
    {
        const auto width = (int) plot.getWidth();

        if (width < 2)
            return;

        const auto* data = table->getFrameData (0, frameIndex);

        juce::Path path;

        for (int x = 0; x < width; ++x)
        {
            const auto index = (int) ((float) x / (float) (width - 1) * (float) Wavetable::frameSize);
            const auto sample = data[juce::jlimit (1, Wavetable::frameSize, index + 1)];
            const auto y = centreY - sample * halfHeight;

            if (x == 0)
                path.startNewSubPath ((float) plot.getX() + (float) x, y);
            else
                path.lineTo ((float) plot.getX() + (float) x, y);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    }

    // The phase warps the oscillator applies itself (not FM or Ring), for
    // the preview.
    struct WarpStages
    {
        int mode1 = 0, mode2 = 0;
        float amount1 = 0.0f, amount2 = 0.0f;
    };

    WarpStages readWarp() const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]);
        WarpStages stages;
        stages.mode1 = readChoice (prefix + "_warp");
        stages.amount1 = readPlain (prefix + "_warp_amt");
        stages.mode2 = Warp::modeForStageTwoChoice (readChoice (prefix + "_warp2"));
        stages.amount2 = readPlain (prefix + "_warp2_amt");

        if (! Warp::isOscillatorWarp (stages.mode1)) stages.amount1 = 0.0f;
        if (! Warp::isOscillatorWarp (stages.mode2)) stages.amount2 = 0.0f;
        return stages;
    }

    bool hasWarp() const
    {
        if (subTableMapping || (modeId.isNotEmpty() && readChoice (modeId) != 0))
            return false;

        const auto stages = readWarp();
        return stages.amount1 > 0.0f || stages.amount2 > 0.0f;
    }

    void drawWarpedFrame (juce::Graphics& g, const Wavetable* table, int frameIndex, juce::Rectangle<float> plot,
                          float centreY, float halfHeight) const
    {
        const auto width = (int) plot.getWidth();

        if (width < 2)
            return;

        const auto stages = readWarp();
        const auto* data = table->getFrameData (0, frameIndex);
        juce::Path path;

        const auto applyStage = [] (int mode, float amount, double phase, bool& silent, float& gain)
        {
            if (amount <= 0.0f)
                return phase;

            if (Warp::isPhaseDistortion (mode))
                return Warp::applyPhaseDistortion (mode, amount, phase, gain);

            return Warp::apply (mode, amount, phase, silent);
        };

        for (int x = 0; x < width; ++x)
        {
            auto silent = false;
            auto gain = 1.0f, gain2 = 1.0f;
            auto phase = juce::jlimit (0.0, 0.999999, (double) x / (double) (width - 1));
            phase = applyStage (stages.mode1, stages.amount1, phase, silent, gain);
            if (! silent)
                phase = applyStage (stages.mode2, stages.amount2, phase, silent, gain2);

            const auto index = (int) (phase * (double) Wavetable::frameSize);
            const auto sample = silent ? 0.0f : data[juce::jlimit (1, Wavetable::frameSize, index + 1)] * gain * gain2;
            const auto y = centreY - sample * halfHeight;

            if (x == 0)
                path.startNewSubPath (plot.getX(), y);
            else
                path.lineTo (plot.getX() + (float) x, y);
        }

        g.setColour (traceColour);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String tableId, frameId, unisonId, spreadId, detuneId, shapeId, modeId;
    int oscIndex = 0;
    juce::String startId, endId, fadeInId, fadeOutId, reverseId, loopId;
    juce::Colour traceColour;
    bool followsTheme = false;
    mutable const SampleData* cachedSample = nullptr;
    mutable std::vector<float> cachedPeaks;
    const SampleData* lastSample = nullptr;
    float loadFlash = 0.0f;
    float displayedFrame = 0.0f;
    bool sampleDragHover = false;
    bool subTableMapping = false;
    juce::TextButton modeButton { "3D" };
    bool threeD = false;
};
