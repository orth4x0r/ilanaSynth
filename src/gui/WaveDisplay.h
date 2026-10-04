#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/EpTuning.h"

#include <cmath>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/SampleFactory.h"
#include "../dsp/Voice.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"
#include "TableBrowser.h"
#include "PhysicalView.h"

class WaveDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    public juce::FileDragAndDropTarget,
                    private IlanaAnim::FrameTimer
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

        setTooltip (tableTooltip);

        // The view as a segmented switch above the plot (UI review 6, V21):
        // the cycle (WAVE), every frame (3D) or the harmonics (SPEC).
        static const char* const viewNames[] { "WAVE", "3D", "SPEC" };
        static const char* const viewTips[] { "One cycle of the frame playing now",
                                              "Every frame of the table, the one playing now lit",
                                              "The cycle's harmonics, warps included" };

        for (int view = 0; view < 3; ++view)
        {
            auto& button = viewButtons[(size_t) view];
            button.setButtonText (viewNames[view]);
            button.setClickingTogglesState (false);
            button.setTooltip (viewTips[view]);
            IlanaTheme::makePill (button, followsTheme ? IlanaTheme::accent() : traceColour);
            button.onClick = [this, view]
            {
                viewPicked = true;
                pickedTable = resolveTableIndex();
                setViewMode (view);
            };
            addChildComponent (button);
        }

        // The table's name on the display, with arrows to step through the
        // tables (V5-27): click the name for the browser.
        for (auto* arrow : { &previousTable, &nextTable })
        {
            arrow->setButtonText (arrow == &previousTable ? "<" : ">");
            arrow->setTooltip (arrow == &previousTable ? "Previous wavetable" : "Next wavetable");
            arrow->onClick = [this, arrow] { stepTable (arrow == &previousTable ? -1 : 1); };
            addChildComponent (*arrow);
        }

        updateViewButtons();

        startTimerHz (30);
    }

    ~WaveDisplay() override { endGestures(); }

    // PLAY's strips: the picture only (no view switch, names or readouts).
    void setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        modeEpoch = ~(juce::uint64) 0;
        resized();
        repaint();
    }

    void setViewMode (int mode)
    {
        viewMode = juce::jlimit (0, 2, mode);
        updateViewButtons();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = wellArea();

        paintChrome (g);
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto borderColour = sampleDragHover ? traceColour : IlanaTheme::Ui::line;
        const auto borderThickness = sampleDragHover ? 2.0f : 1.0f;

        if (isLiveInput())
        {
            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness);
            const auto level = processorRef.getInputLevel();
            auto plot = bounds.reduced (14.0f, 12.0f);
            g.setColour (traceColour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
            g.drawText (readChoice (modeId) == 4 ? "LIVE INPUT" : "LIVE GRAINS", plot.removeFromTop (22.0f).toNearestInt(),
                        juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
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
            drawString (g, bounds.reduced (compact ? 6.0f : 12.0f));
            IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
            return;
        }

        if (isElectricPiano())
        {
            g.setColour (borderColour);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, borderThickness);
            drawPickup (g, bounds.reduced (compact ? 6.0f : 12.0f));
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

        const auto plot = bounds.reduced (10.0f, compact ? 5.0f : 10.0f);

        if (shownViewMode() == 1)
        {
            drawWaterfall (g, table, frame, plot);
        }
        else if (shownViewMode() == 2)
        {
            const auto frameCount = table->getNumFrames();
            const auto frameIndex = juce::jlimit (0, frameCount - 1, (int) std::round (frame * (float) (frameCount - 1)));
            drawSpectrum (g, table, frameIndex, plot);
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

            // The x axis is the phase of one cycle, so the frame isn't marked
            // on it (UI review 4, V11): a slim scrubber along the plot's foot
            // and a readout under it say where in the table the cycle comes
            // from. (Unison shows on the UNISON knob, not as marks here.)
            if (! isStaticTable (processorRef.getWavetable (tableIndex)))
                drawFramePosition (g, frameCount, frame, plot);
        }

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    // The frame readout under the plot ("FRAME 12 / 64"), empty when the
    // view shows no table, the table has one frame, or the view is SPEC (the
    // UI test reads it).
    juce::String getFrameReadout() const
    {
        if (shownViewMode() == 2 || ! isTableMode() || isPhysicalString() || isElectricPiano())
            return {};

        const auto* table = processorRef.getWavetable (resolveTableIndex());

        if (table == nullptr || isStaticTable (table))
            return {};

        return frameText (table->getNumFrames(), displayedFrame);
    }

    // A drag on a table: across scrubs the frame (the scrubber under the
    // plot follows the mouse), up and down change the first WARP's amount when a warp is
    // chosen. Each parameter moves inside one gesture, so a host records one
    // undo step and automation writes a clean move. Grains: across moves the
    // read position.
    void mouseDown (const juce::MouseEvent& event) override
    {
        endGestures();
        pressInHeader = ! compact && event.position.y < (float) headerHeight;

        if (pressInHeader)
        {
            if (! event.mods.isPopupMenu())
                mouseDownOnHeader (event);

            return;
        }

        if (! event.mods.isPopupMenu())
        {
            if (isGranularMode())
            {
                processorRef.beginEdit (editName());
                beginGesture (startId);
                setPositionFromX (event.position.x);
                return;
            }

            if (! isTableMode())
                return;

            processorRef.beginEdit (editName());
            beginGesture (frameId);

            if (canDragWarp())
            {
                beginGesture (warpAmountId());
                warpAtDragStart = readPlain (warpAmountId());
            }

            dragStartY = event.position.y;
            dragging = true;
            setFrameFromX (event.position.x);
            repaint();
            return;
        }

        showSampleMenu (*this);
    }

    // The sample menu: the factory samples, then a file (a sample, or an SF2
    // / SFZ multisample). The display's right-click and the OSC card's LOAD
    // button open it (UI review 7, I7-24).
    void showSampleMenu (juce::Component& target)
    {
        juce::PopupMenu menu;
        constexpr int loadFileItem = 10000;
        menu.addItem (loadFileItem, "Load Sample or SoundFont (SF2 / SFZ)...");
        menu.addSeparator();
        menu.addSectionHeader ("Factory Samples");

        const auto paramId = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)])
                             + "_sample_factory";
        const auto current = (int) readPlain (paramId);

        for (int i = 0; i < SampleFactory::getNumFactorySamples(); ++i)
            menu.addItem (i + 1, SampleFactory::getFactorySampleName (i), true, current == i + 1);

        juce::Component::SafePointer<WaveDisplay> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                            [safeThis, paramId] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                if (result == loadFileItem)
                                {
                                    safeThis->chooseSampleFile();
                                    return;
                                }

                                safeThis->processorRef.performEdit (safeThis->editName(), [&]
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (paramId))
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) result));

                                    safeThis->switchToSampleMode();
                                });
                            });
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (pressInHeader)
            return;

        if (isGranularMode())
            setPositionFromX (event.position.x);

        if (! dragging || ! isTableMode())
            return;

        setFrameFromX (event.position.x);

        if (canDragWarp())
        {
            // A full sweep of the amount takes about 1.5 display heights
            // (shift for fine steps).
            const auto span = (float) juce::jmax (60, getHeight()) * (event.mods.isShiftDown() ? 6.0f : 1.5f);
            setPlain (warpAmountId(), juce::jlimit (0.0f, 1.0f, warpAtDragStart + (dragStartY - event.position.y) / span));
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (std::exchange (pressInHeader, false))
            return;

        endGestures();
        processorRef.endEdit();

        if (dragging)
        {
            dragging = false;
            repaint();
        }
    }

    // Double-click on a warped table sets the warp amount back to zero.
    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() || ! isTableMode() || ! canDragWarp())
            return;

        processorRef.performEdit ("Reset OSC " + juce::String (oscIndex + 1) + " warp", [this]
        {
            beginGesture (warpAmountId());
            setPlain (warpAmountId(), 0.0f);
            endGestures();
        });
    }

    // The undo step's name for a drag or pick on this display.
    juce::String editName() const { return "OSC " + juce::String (oscIndex + 1) + " display"; }

    // A multisample's zone count (0 for a plain sample; the UI test).
    int getZoneCount() const
    {
        const auto* sample = isSampleMode() ? processorRef.getSampleForOsc (oscIndex) : nullptr;
        return sample != nullptr && sample->zones.size() > 1 ? (int) sample->zones.size() : 0;
    }

    // The view shown: a table whose frames are all one cycle shows as WAVE
    // until a view is picked by hand.
    int getViewMode() const { return shownViewMode(); }
    int getOscIndex() const { return oscIndex; }
    bool isDraggingWarp() const { return dragging && canDragWarp(); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        if (files.size() != 1)
            return false;

        const auto extension = juce::File (files[0]).getFileExtension().toLowerCase();

        return extension == ".wav" || extension == ".aif" || extension == ".aiff"
               || extension == ".flac" || extension == ".ogg" || extension == ".mp3" || extension == ".m4a"
               || extension == ".sf2" || extension == ".sfz";
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
        // The header line above the plot: the table's name between its
        // arrows at the left, the view switch at the right.
        auto header = getLocalBounds().removeFromTop (headerHeight);
        auto views = header.removeFromRight (juce::jmin (114, header.getWidth() / 2));
        const auto viewWidth = views.getWidth() / 3;

        for (auto& button : viewButtons)
            button.setBounds (views.removeFromLeft (viewWidth).reduced (1, 1));

        header.removeFromRight (6);
        previousTable.setBounds (header.removeFromLeft (20).reduced (0, 1));
        nextTable.setBounds (header.removeFromRight (20).reduced (0, 1));
        tableNameArea = header.reduced (4, 0);
    }

    // The table's name as the header shows it (the UI test reads it).
    juce::String getTableName() const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId)))
            return param->getCurrentChoiceName();

        return {};
    }

    bool isCompact() const { return compact; }

    void chooseSampleFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load sample or SoundFont",
                                                           juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                           "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.sf2;*.sfz");
        juce::Component::SafePointer<WaveDisplay> safeThis (this);
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis] (const juce::FileChooser& chooser)
                                  {
                                      if (safeThis == nullptr || ! chooser.getResult().existsAsFile())
                                          return;
                                      if (safeThis->processorRef.loadUserSample (safeThis->oscIndex, chooser.getResult()))
                                          safeThis->switchToSampleMode();
                                      safeThis->repaint();
                                  });
    }

private:
    // A table whose frames are all the same cycle (a sine operator's): 3D
    // and a frame readout say nothing about it (UI review 7, V7-31).
    bool isStaticTable (const Wavetable* table) const
    {
        if (table == nullptr)
            return true;

        if (table != staticTable)
        {
            staticTable = table;
            staticFrames = true;
            const auto* first = table->getFrameData (0, 0);

            for (int frame = 1; frame < table->getNumFrames() && staticFrames; ++frame)
            {
                const auto* data = table->getFrameData (0, frame);

                for (int i = 1; i <= Wavetable::frameSize; ++i)
                    if (std::abs (data[i] - first[i]) > 1.0e-4f)
                    {
                        staticFrames = false;
                        break;
                    }
            }
        }

        return staticFrames;
    }

    int shownViewMode() const
    {
        if (viewMode == 1 && ! (viewPicked && pickedTable == resolveTableIndex()) && isTableMode() && ! subTableMapping
            && isStaticTable (processorRef.getWavetable (resolveTableIndex())))
            return 0;

        return viewMode;
    }

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
        const auto excite = juce::jlimit (0, 10, readChoice (prefix + "_excite"));
        const auto position = readPlain (prefix + "_string_excite_pos");
        const auto strike = position > 0.005f ? juce::jlimit (0.02f, 0.5f, position) : 0.125f;

        // Named in the header above the plot (not on it), the caption only
        // where both fit.
        if (! compact)
            drawHeaderText (g, "STRING", Exciters::name (excite).toLowerCase() + " at "
                                             + (position > 0.005f ? juce::String (juce::roundToInt (strike * 100.0f)) + "%" : juce::String ("auto")));

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

        // The title and caption in the header, the caption dropped when
        // both don't fit ("REED PICKUPcharge vs reed position": UI review
        // 6, V16 and S32).
        if (! compact)
            drawHeaderText (g, tine ? "TINE PICKUP" : "REED PICKUP", tine ? "flux vs tine position" : "charge vs reed position");

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

        // "rest" under the plot, at the rest line, off the curve.
        if (! compact)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText ("rest", juce::Rectangle<float> (toX (0.0f) - 20.0f, footerArea().getY(), 40.0f, footerArea().getHeight()).toNearestInt(),
                        juce::Justification::centred);
        }
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
        const auto now = liveSeconds;
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
        auto plot = wellArea().reduced (10.0f, compact ? 5.0f : 10.0f);

        // A multisample's zones under the wave: keys across, velocity up
        // (UI review 7, I7-24).
        if (sample != nullptr && sample->zones.size() > 1 && ! compact && plot.getHeight() > 120.0f)
            drawZones (g, *sample, plot.removeFromBottom (juce::jmin (64.0f, plot.getHeight() * 0.3f)));

        if (sample == nullptr || sample->getNumSamples() < 2)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (compact ? IlanaTheme::TextSize::tiny : IlanaTheme::TextSize::body));
            IlanaTheme::drawFitted (g, compact ? "DROP A SAMPLE" : "DROP A SAMPLE HERE", wellArea().toNearestInt().reduced (6), juce::Justification::centred, 2);
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

        if (cachedSample != sample || cachedSampleEpoch != processorRef.getSampleEpoch() || (int) cachedPeaks.size() != width)
        {
            cachedSample = sample;
            cachedSampleEpoch = processorRef.getSampleEpoch();
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

            if (! compact)
                drawHeaderText (g, sample->name, (reverse ? "REV " : "") + juce::String::fromUTF8 ("grains \xc2\xb7 drag to move"));

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

            const auto pulse = 0.6f + 0.4f * std::sin ((float) liveSeconds * 6.0f);
            g.setColour (traceColour.withAlpha (0.22f * pulse));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ cursorX, cursorY }));

            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre ({ cursorX, cursorY }));
        }

        if (! compact)
            drawHeaderText (g, sample->name, (sample->zones.size() > 1 ? juce::String ((int) sample->zones.size()) + " zones, " : juce::String())
                                                 + (reverse ? "REV " : "") + juce::String (loop ? "loop" : "1-shot"));
    }

    // The zones as boxes on a keyboard strip: the used key range across
    // (octave lines, C notes named), velocity up; the zone middle C plays
    // lit.
    void drawZones (juce::Graphics& g, const SampleData& sample, juce::Rectangle<float> area) const
    {
        auto low = 127, high = 0;
        for (const auto& zone : sample.zones)
        {
            low = juce::jmin (low, zone.loKey);
            high = juce::jmax (high, zone.hiKey);
        }
        low = juce::jmax (0, low - low % 12);
        high = juce::jmin (127, high + (11 - high % 12));

        area.removeFromTop (6.0f);
        const auto labels = area.removeFromBottom (11.0f);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (area, 3.0f);
        const auto keyWidth = area.getWidth() / (float) juce::jmax (1, high - low + 1);
        const auto xOf = [&] (int key) { return area.getX() + (float) (key - low) * keyWidth; };

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (int key = low; key <= high; key += 12)
        {
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRect (xOf (key), area.getY(), 1.0f, area.getHeight());
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (juce::MidiMessage::getMidiNoteName (key, true, true, 3), juce::Rectangle<float> (xOf (key) + 2.0f, labels.getY(), 40.0f, labels.getHeight()),
                        juce::Justification::centredLeft, false);
        }

        const auto* middle = sample.zoneFor (60, 100);
        for (const auto& zone : sample.zones)
        {
            const auto box = juce::Rectangle<float> (xOf (zone.loKey), area.getBottom() - area.getHeight() * (float) zone.hiVel / 127.0f,
                                                     (float) (zone.hiKey - zone.loKey + 1) * keyWidth,
                                                     area.getHeight() * (float) (zone.hiVel - zone.loVel + 1) / 127.0f).reduced (0.5f);
            g.setColour (traceColour.withAlpha (&zone == middle ? 0.55f : 0.22f));
            g.fillRect (box);
            g.setColour (traceColour.withAlpha (0.8f));
            g.drawRect (box, 1.0f);
        }
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

    static juce::String frameText (int frames, float position)
    {
        return "FRAME " + juce::String (juce::roundToInt (position * (float) juce::jmax (0, frames - 1)) + 1)
               + " / " + juce::String (frames);
    }

    // Where the cycle sits in the table (after modulation, gliding): a slim
    // scrubber along the plot's foot (the readout is under the plot, in
    // paintChrome, so it never covers the trace: UI review 6, V21).
    void drawFramePosition (juce::Graphics& g, int frames, float position, juce::Rectangle<float> plot) const
    {
        if (frames <= 1)
            return;

        const auto track = juce::Rectangle<float> (plot.getX(), plot.getBottom() - 2.0f, plot.getWidth(), 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (track, 1.0f);
        g.setColour (traceColour.withAlpha (0.45f));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * position), 1.0f);
        g.setColour (traceColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 4.0f).withCentre ({ track.getX() + track.getWidth() * position, track.getCentreY() }), 2.0f);
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

            for (auto& button : viewButtons)
                IlanaTheme::makePill (button, traceColour);
        }
    }

    void timerCallback() override
    {
        // The 3D waterfall only applies to tables. The mode is re-read only
        // when a parameter changes.
        if (const auto epoch = processorRef.getUiEpoch(); epoch != modeEpoch)
        {
            modeEpoch = epoch;
            updateViewButtons();
            setTooltip (isPhysicalString() ? "The string after a strike, from where it is struck (EXCITE POS). OSC > PHYSICAL shows it moving." : isElectricPiano() ? "The pickup's response across the swing: the shaded bands are a medium and a hard note. "
                                            "A swing that reaches over the bends barks (tine) or growls (reed). DISTANCE and OFFSET move them."
                        : isLiveInput() ? "The audio coming into ilanaSynth FX."
                        : isGranularMode() ? "Grains are read from around the white line: drag to move it. Right-click for factory samples, or drop a wav."
                        : isSampleMode() ? "Showing the loaded sample. Drop a new wav here to replace it."
                                       : tableTooltip);
        }

        const auto* sample = isSampleMode() ? processorRef.getSampleForOsc (oscIndex) : nullptr;

        if (sample != lastSample || processorRef.getSampleEpoch() != lastSampleEpoch)
        {
            lastSample = sample;
            lastSampleEpoch = processorRef.getSampleEpoch();
            loadFlash = sample != nullptr ? 1.0f : 0.0f;
        }

        loadFlash = IlanaAnim::decay (loadFlash, 0.93f, frameTicks());

        if (const auto view = shownViewMode(); view != lastShownView)
        {
            lastShownView = view;
            updateViewButtons();
            repaint();
        }

        // Follow the frame parameter including any modulation (LFO, envelope,
        // macros), smoothed so morphs glide rather than jump.
        const auto frame = resolvedFrame();
        const auto gliding = std::abs (frame - displayedFrame) > 1.0e-4f;
        displayedFrame = gliding ? IlanaAnim::approach (displayedFrame, frame, 0.25f, frameTicks()) : frame;

        // Grains drift and the play cursor pulses only while notes sound.
        if (processorRef.getActiveVoiceCount() > 0)
            liveSeconds += (double) frameSeconds();

        if (! isShowing())
            return;

        if (gliding || loadFlash > 0.01f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
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
        if (frameId == "osc4_frame") return Mod::Destination::Osc4Frame;
        if (frameId == "osc5_frame") return Mod::Destination::Osc5Frame;
        if (frameId == "osc6_frame") return Mod::Destination::Osc6Frame;

        return Mod::Destination::None;
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
            // The front frame's troughs stay inside the plot (on a tall
            // plot a fixed 6 px margin let them run under the frame readout).
            const auto baseline = plot.getBottom() - juce::jmax (6.0f, plot.getHeight() * 0.15f) - depth * plot.getHeight() * 0.62f;
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

    // The view switch and the table arrows show over a wavetable only, and
    // not in a compact strip.
    void updateViewButtons()
    {
        const auto table = ! compact && isTableMode() && ! subTableMapping;

        for (int view = 0; view < 3; ++view)
        {
            viewButtons[(size_t) view].setVisible (table);
            viewButtons[(size_t) view].setToggleState (view == shownViewMode(), juce::dontSendNotification);
        }

        previousTable.setVisible (table);
        nextTable.setVisible (table);
    }

    // The next or previous table that holds something (empty User slots are
    // skipped), as one undo step.
    void stepTable (int direction)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId));

        if (param == nullptr)
            return;

        const auto count = param->choices.size();
        auto choice = param->getIndex();

        for (int tries = 0; tries < count; ++tries)
        {
            choice = (choice + direction + count) % count;

            if (const auto* table = processorRef.getWavetable (choice); table != nullptr && table->getNumFrames() > 0)
                break;
        }

        processorRef.performEdit ("OSC " + juce::String (oscIndex + 1) + " table", [param, choice]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) choice));
            param->endChangeGesture();
        });
        repaint();
    }

    // The well the picture sits in: all of it in a compact strip; between
    // the header line and the readout line otherwise.
    juce::Rectangle<float> wellArea() const
    {
        auto bounds = getLocalBounds().toFloat();

        if (! compact)
        {
            bounds.removeFromTop ((float) headerHeight + 3.0f);
            bounds.removeFromBottom ((float) footerHeight + 2.0f);
        }

        return bounds;
    }

    juce::Rectangle<float> footerArea() const
    {
        return getLocalBounds().toFloat().removeFromBottom ((float) footerHeight);
    }

    // A title and a quieter caption in the header line; the caption only
    // where both fit (with a gap).
    void drawHeaderText (juce::Graphics& g, const juce::String& title, const juce::String& caption) const
    {
        auto header = getLocalBounds().toFloat().removeFromTop ((float) headerHeight).reduced (4.0f, 0.0f);
        const juce::Font titleFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        const juce::Font captionFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, title);
        const auto captionWidth = juce::GlyphArrangement::getStringWidth (captionFont, caption);

        g.setColour (traceColour);
        g.setFont (titleFont);
        g.drawText (title, header, juce::Justification::centredLeft, true);

        if (caption.isNotEmpty() && titleWidth + captionWidth + 14.0f <= header.getWidth())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (captionFont);
            g.drawText (caption, header, juce::Justification::centredRight);
        }
    }

    // Outside the well: the table's name (between its arrows) in the header
    // and the frame readout, or the drag readout, under the plot.
    void paintChrome (juce::Graphics& g) const
    {
        if (compact || ! isTableMode() || subTableMapping)
            return;

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText (getTableName(), tableNameArea, juce::Justification::centredLeft, true);

        const auto* table = processorRef.getWavetable (resolveTableIndex());
        juce::String readout;

        if (table != nullptr && table->getNumFrames() > 0)
        {
            if (dragging)
            {
                readout = frameText (table->getNumFrames(), readValue (frameId));

                if (canDragWarp())
                    readout << "   WARP " << juce::roundToInt (readPlain (warpAmountId()) * 100.0f) << "%";
            }
            else if (shownViewMode() == 2)
                readout = "HARMONICS 1-" + juce::String (juce::jlimit (8, 128, (int) ((wellArea().getWidth() - 20.0f) / 4.0f)));
            else if (! isStaticTable (table))
                readout = frameText (table->getNumFrames(), displayedFrame);
        }

        g.setColour (dragging ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (readout, footerArea().reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    void mouseDownOnHeader (const juce::MouseEvent& event)
    {
        if (tableNameArea.contains (event.getPosition()) && isTableMode() && ! subTableMapping)
            TableBrowser::show (processorRef, tableId, traceColour, *this);
    }

    // A plain wavetable oscillator (not sample, grains, physical or live).
    bool isTableMode() const
    {
        return frameId.isNotEmpty() && (modeId.isEmpty() || readChoice (modeId) == 0);
    }

    juce::String warpAmountId() const
    {
        return juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]) + "_warp_amt";
    }

    // The vertical drag has a warp to move only when one is chosen.
    bool canDragWarp() const
    {
        if (subTableMapping || ! isTableMode())
            return false;

        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]);
        return Warp::isOscillatorWarp (readChoice (prefix + "_warp")) && processorRef.apvts.getParameter (warpAmountId()) != nullptr;
    }

    void beginGesture (const juce::String& id)
    {
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

    void setPlain (const juce::String& id, float plainValue)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    // One cycle as the oscillator plays it: the frame through the warps.
    void fillCycle (const Wavetable* table, int frameIndex, std::vector<float>& out) const
    {
        const auto size = (int) out.size();
        const auto* data = table->getFrameData (0, frameIndex);
        const auto warped = hasWarp();
        const auto stages = readWarp();

        for (int i = 0; i < size; ++i)
        {
            auto phase = (double) i / (double) size;
            auto silent = false;
            auto gain = 1.0f, gain2 = 1.0f;

            if (warped)
            {
                const auto applyStage = [&silent] (int mode, float amount, double p, float& stageGain)
                {
                    if (amount <= 0.0f)
                        return p;
                    if (Warp::isPhaseDistortion (mode))
                        return Warp::applyPhaseDistortion (mode, amount, p, stageGain);
                    return Warp::apply (mode, amount, p, silent);
                };
                phase = applyStage (stages.mode1, stages.amount1, juce::jlimit (0.0, 0.999999, phase), gain);
                if (! silent)
                    phase = applyStage (stages.mode2, stages.amount2, phase, gain2);
            }

            const auto index = (int) (phase * (double) Wavetable::frameSize);
            out[(size_t) i] = silent ? 0.0f : data[juce::jlimit (1, Wavetable::frameSize, index + 1)] * gain * gain2;
        }
    }

    // The cycle's harmonics as bars (level in dB against the loudest, 60 dB
    // shown), the fundamental at the left, the way Vital's spectrum view
    // reads. Warps are included, so a warp sweep shows its new partials.
    void drawSpectrum (juce::Graphics& g, const Wavetable* table, int frameIndex, juce::Rectangle<float> plot) const
    {
        constexpr int order = 11;
        constexpr int size = 1 << order;
        static_assert (size == Wavetable::frameSize, "one cycle per FFT");
        cycleBuffer.assign ((size_t) size, 0.0f);
        fillCycle (table, frameIndex, cycleBuffer);

        spectrumBuffer.assign ((size_t) size * 2, 0.0f);
        std::copy (cycleBuffer.begin(), cycleBuffer.end(), spectrumBuffer.begin());
        if (fft == nullptr)
            fft = std::make_unique<juce::dsp::FFT> (order);
        fft->performFrequencyOnlyForwardTransform (spectrumBuffer.data(), true);

        const auto area = plot.withTrimmedTop (4.0f).withTrimmedBottom (4.0f);
        const auto bars = juce::jlimit (8, 128, (int) (area.getWidth() / 4.0f));
        auto loudest = 1.0e-9f;
        for (int h = 1; h <= bars; ++h)
            loudest = juce::jmax (loudest, spectrumBuffer[(size_t) h]);

        const auto barWidth = area.getWidth() / (float) bars;
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int db = -20; db >= -40; db -= 20)
        {
            const auto y = area.getY() + area.getHeight() * (float) -db / 60.0f;
            g.fillRect (juce::Rectangle<float> (area.getX(), y, area.getWidth(), 1.0f));
        }

        for (int h = 1; h <= bars; ++h)
        {
            const auto level = juce::Decibels::gainToDecibels (spectrumBuffer[(size_t) h] / loudest, -60.0f);
            const auto height = area.getHeight() * (1.0f + level / 60.0f);

            if (height < 0.5f)
                continue;

            const auto bar = juce::Rectangle<float> (area.getX() + (float) (h - 1) * barWidth + 0.5f, area.getBottom() - height,
                                                     juce::jmax (1.0f, barWidth - 1.0f), height);
            g.setColour (traceColour.withAlpha (h % 2 == 1 ? 0.9f : 0.65f));
            g.fillRect (bar);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    IlanaAnim::ChangeGate changeGate;
    juce::uint64 modeEpoch = ~(juce::uint64) 0;
    double liveSeconds = 0.0;
    juce::String tableId, frameId, unisonId, spreadId, detuneId, shapeId, modeId;
    int oscIndex = 0;
    juce::String startId, endId, fadeInId, fadeOutId, reverseId, loopId;
    juce::Colour traceColour;
    bool followsTheme = false;
    mutable const SampleData* cachedSample = nullptr;
    mutable unsigned cachedSampleEpoch = 0;
    mutable std::vector<float> cachedPeaks;
    const SampleData* lastSample = nullptr;
    unsigned lastSampleEpoch = 0;
    float loadFlash = 0.0f;
    float displayedFrame = 0.0f;
    bool sampleDragHover = false;
    bool subTableMapping = false;
    std::array<juce::TextButton, 3> viewButtons;
    juce::TextButton previousTable, nextTable;
    juce::Rectangle<int> tableNameArea;
    bool compact = false, pressInHeader = false;
    static constexpr int headerHeight = 20, footerHeight = 14;
    int viewMode = 0; // 0 the cycle, 1 the 3D waterfall, 2 the harmonics
    bool viewPicked = false; // a view chosen by hand, kept for that table even when static
    int pickedTable = -1;
    int lastShownView = -1;
    mutable const Wavetable* staticTable = nullptr;
    mutable bool staticFrames = false;
    bool dragging = false;
    float dragStartY = 0.0f, warpAtDragStart = 0.0f;
    std::vector<juce::RangedAudioParameter*> openGestures;
    mutable std::vector<float> cycleBuffer;
    mutable std::vector<float> spectrumBuffer;
    mutable std::unique_ptr<juce::dsp::FFT> fft;

    static constexpr const char* tableTooltip =
        "Drag across to scrub the frame; with a WARP chosen, drag up or down for its amount (shift: fine, double-click: zero). "
        "WAVE / 3D / SPEC above it switch the view; the arrows and the name pick the table. Drop a wav to switch this oscillator to Sample.";
    std::unique_ptr<juce::FileChooser> fileChooser;
};
