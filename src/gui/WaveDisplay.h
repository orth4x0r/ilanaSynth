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
#include "ParamControls.h"
#include "AnimationUtils.h"
#include "DisplayStyle.h"
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

    // The picture with a slim line over its top (the table's name between
    // arrows, a chip that cycles the view) and the frame's number at its foot:
    // the OSC cards' 150 px well. Set after setCompact.
    void setSlim (bool shouldBeSlim)
    {
        slim = shouldBeSlim;
        repaint();
    }

    bool isSlim() const { return slim; }

    // An FM operator plays one plain cycle: no 3D or SPEC views to pick
    // (UI review 8, I8-15).
    void setSingleCycle (bool shouldBeSingle)
    {
        if (singleCycle == shouldBeSingle)
            return;
        singleCycle = shouldBeSingle;
        updateViewButtons();
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

        auto plot = bounds.reduced (10.0f, compact ? 5.0f : 10.0f);

        // The OSC card's well (design round 2): a quarter grid behind the
        // picture, the picture under the full height (the slim line shows
        // over its top only while the mouse is on it), and the WAVE view as
        // the design draws it, the table's frames stacked flat with the one
        // playing lit.
        const auto flatStack = slim && shownViewMode() == 0 && ! isStaticTable (processorRef.getWavetable (tableIndex));
        if (slim)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            for (int i = 1; i < 4; ++i)
            {
                g.fillRect (juce::Rectangle<float> (bounds.getWidth() - 2.0f, 1.0f).withPosition (bounds.getX() + 1.0f, bounds.getY() + bounds.getHeight() * (float) i / 4.0f));
                g.fillRect (juce::Rectangle<float> (1.0f, bounds.getHeight() - 2.0f).withPosition (bounds.getX() + bounds.getWidth() * (float) i / 4.0f, bounds.getY() + 1.0f));
            }
            plot.removeFromBottom (8.0f); // (the frame's number)
        }

        if (flatStack)
        {
            drawFlatStack (g, table, frame, plot);
        }
        else if (shownViewMode() == 1)
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
                drawFrame (g, table, frameIndex, plot, centreY, halfHeight, traceColour, 2.0f); // (the sheet's 2 px trace)
            }

            // The x axis is the phase of one cycle, so the frame isn't marked
            // on it (UI review 4, V11): a slim scrubber along the plot's foot
            // and a readout under it say where in the table the cycle comes
            // from. (Unison shows on the UNISON knob, not as marks here.)
            if (! isStaticTable (processorRef.getWavetable (tableIndex)))
                drawFramePosition (g, frameCount, frame, plot);
        }

        if (slim)
            paintSlimLine (g, table);

        IlanaTheme::paintGlassOverlay (g, bounds, 6.0f);
    }

    // The design's 2D well: up to nine of the table's frames as flat lines one
    // under the other (front to back, top to bottom), faint, and the cycle
    // playing now lit at its place among them (warped when a warp is on).
    void drawFlatStack (juce::Graphics& g, const Wavetable* table, float frame, juce::Rectangle<float> plot) const
    {
        const auto frameCount = table->getNumFrames();
        const auto lines = juce::jmin (9, frameCount);
        const auto amplitude = plot.getHeight() * 0.1f;
        const auto top = plot.getY() + 1.5f * amplitude, span = plot.getHeight() - 3.0f * amplitude;
        const auto yFor = [&] (float position) { return top + span * position; };

        for (int i = 0; i < lines; ++i)
        {
            const auto position = lines > 1 ? (float) i / (float) (lines - 1) : 0.0f;
            const auto index = juce::jlimit (0, frameCount - 1, juce::roundToInt (position * (float) (frameCount - 1)));
            drawFrame (g, table, index, plot, yFor (position), amplitude, juce::Colours::white.withAlpha (0.2f), 1.0f);
        }

        const auto frameIndex = juce::jlimit (0, frameCount - 1, (int) std::round (frame * (float) (frameCount - 1)));
        const auto centre = yFor (juce::jlimit (0.0f, 1.0f, frame));
        if (hasWarp())
        {
            drawFrame (g, table, frameIndex, plot, centre, amplitude, traceColour.withAlpha (0.25f), 1.0f);
            drawWarpedFrame (g, table, frameIndex, plot, centre, amplitude);
        }
        else
            drawFrame (g, table, frameIndex, plot, centre, amplitude, traceColour, 2.0f);
    }

    // Where the slim line's view chip and arrows sit, for the UI tests.
    juce::Point<float> getSlimTarget (int which) const // 0 view chip, 1 previous, 2 next
    {
        const auto areas = slimAreas();
        return (which == 0 ? areas.view : which == 1 ? areas.previous : areas.next).getCentre().toFloat();
    }

    // The slim line's parts: arrow, name, arrow, view chip.
    struct SlimAreas { juce::Rectangle<int> previous, name, next, view; };

    SlimAreas slimAreas() const
    {
        auto line = getLocalBounds().removeFromTop (slimHeader).reduced (2, 1);
        SlimAreas areas;
        areas.view = line.removeFromRight (32);
        areas.previous = line.removeFromLeft (13);
        areas.next = line.removeFromRight (13);
        areas.name = line.reduced (1, 0);
        return areas;
    }

    void paintSlimLine (juce::Graphics& g, const Wavetable* table) const
    {
        const auto areas = slimAreas();
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);

        // The table's name, its arrows and the view chip show over the top of
        // the picture while the mouse is on the well (the design's well is the
        // picture alone); a view other than WAVE keeps its chip showing.
        const auto hovered = isMouseOver (true);
        if (hovered)
        {
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.85f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().removeFromTop ((float) slimHeader + 1.0f).reduced (1.0f, 1.0f), 5.0f);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (font);
            g.drawText (juce::String::fromUTF8 ("\xe2\x80\xb9"), areas.previous, juce::Justification::centred);
            g.drawText (juce::String::fromUTF8 ("\xe2\x80\xba"), areas.next, juce::Justification::centred);
            g.setColour (IlanaTheme::Ui::text2);
            IlanaTheme::drawFitted (g, getTableName(), areas.name, juce::Justification::centredLeft, 1);
        }

        static const char* const names[] { "WAVE", "3D", "SPEC" };
        if (hovered || shownViewMode() != 0)
        {
            const auto chip = areas.view.toFloat().reduced (0.0f, 1.0f);
            g.setColour (traceColour.withAlpha (0.18f));
            g.fillRoundedRectangle (chip, 4.0f);
            g.setColour (traceColour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            IlanaTheme::drawFitted (g, names[juce::jlimit (0, 2, shownViewMode())], areas.view, juce::Justification::centred, 1);
        }

        // The frame the cycle comes from, at the picture's foot.
        if (table != nullptr && table->getNumFrames() > 1 && ! isStaticTable (table) && shownViewMode() != 2)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (font);
            IlanaTheme::drawFitted (g, "FRAME " + juce::String (juce::jlimit (0, table->getNumFrames() - 1, juce::roundToInt (displayedFrame * (float) (table->getNumFrames() - 1)))),
                                    getLocalBounds().removeFromBottom (15).withTrimmedBottom (3).withTrimmedLeft (8), juce::Justification::centredLeft, 1);
        }
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

        return frameText (table->getNumFrames(), displayedFrame, frameIsMoving (table->getNumFrames()));
    }

    // A drag on a table: across scrubs the frame (the scrubber under the
    // plot follows the mouse), up and down change the first WARP's amount when a warp is
    // chosen. Each parameter moves inside one gesture, so a host records one
    // undo step and automation writes a clean move. Grains: across moves the
    // read position.
    void mouseDown (const juce::MouseEvent& event) override
    {
        endGestures();
        pressInHeader = (! compact && event.position.y < (float) headerHeight)
                        || (slim && isTableMode() && event.position.y < (float) slimHeader);

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

            // A plain or multi sample: START and END are the two handles on the wave
            // (V14 / S14-4: the loop markers are dragged where they are drawn).
            if (isSampleMode() && ! compact)
                if (const auto handle = sampleHandleAt (event.position.x); handle != 0)
                {
                    handleDrag = handle;
                    processorRef.beginEdit (editName());
                    beginGesture (handle == 1 ? startId : endId);
                    setHandleFromX (event.position.x);
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

        if (handleDrag != 0)
        {
            setHandleFromX (event.position.x);
            return;
        }

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
        handleDrag = 0;

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
        // PLAY's strip is too small for 3D or the harmonics: it always shows
        // the cycle (UI review 8, S8-34).
        if (compact && ! slim)
            return 0;

        if (singleCycle)
            return 0;
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

        // (Through the processor, so an operator's ratio tuning goes too.)
        if (modeId == juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]) + "_mode")
            processorRef.setOscillatorMode (oscIndex, OscMode::sample);
        else if (auto* parameter = processorRef.apvts.getParameter (modeId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (2.0f));
    }

    // The sample's plot as drawSample lays it out horizontally.
    juce::Rectangle<float> samplePlotArea() const { return wellArea().reduced (10.0f, compact ? 5.0f : 10.0f); }

    // 1: the START handle, 2: END, 0: neither, for a press at x (within 9 px; the
    // nearer one when both are).
    int sampleHandleAt (float x) const
    {
        const auto* sample = processorRef.getSampleForOsc (oscIndex);
        if (sample == nullptr || sample->getNumSamples() < 2)
            return 0;

        const auto plot = samplePlotArea();
        const auto startX = plot.getX() + juce::jlimit (0.0f, 1.0f, readPlain (startId)) * plot.getWidth();
        const auto endX = plot.getX() + juce::jlimit (0.0f, 1.0f, readPlain (endId)) * plot.getWidth();
        const auto nearStart = std::abs (x - startX), nearEnd = std::abs (x - endX);
        if (juce::jmin (nearStart, nearEnd) > 9.0f)
            return 0;
        return nearStart <= nearEnd ? 1 : 2;
    }

    void setHandleFromX (float x)
    {
        const auto plot = samplePlotArea();
        const auto position = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
        // (They never cross: at least 1 % of the sample stays between them.)
        if (handleDrag == 1)
            setPlain (startId, juce::jmin (position, readPlain (endId) - 0.01f));
        else
            setPlain (endId, juce::jmax (position, readPlain (startId) + 0.01f));
        repaint();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (isSampleMode() && ! compact && sampleHandleAt (event.position.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor
                                                                                             : juce::MouseCursor::NormalCursor);
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
        // A plain sample shows its map too: one box, every key and velocity
        // (S13-1); granular mode reads a position, not zones.
        if (sample != nullptr && sample->getNumSamples() >= 2 && ! compact && ! isGranularMode() && plot.getHeight() > 96.0f)
            drawZones (g, *sample, plot.removeFromBottom (juce::jmin (64.0f, plot.getHeight() * 0.3f)));

        if (sample == nullptr || sample->getNumSamples() < 2)
        {
            if (compact)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                IlanaTheme::drawFitted (g, "Drop a sample", wellArea().toNearestInt().reduced (6), juce::Justification::centred, 2);
                return;
            }

            // A drop zone that looks like one (S13-1): a dashed edge, the
            // hint, and LOAD... drawn as the button a click on it acts as.
            const auto well = wellArea().reduced (6.0f);

            // A ghost of the wave a sample would draw, so the well is a
            // picture first and a prompt second (review 14, S14-3).
            {
                juce::Path ghost;
                const auto ghostArea = well.reduced (10.0f, 14.0f);
                const auto columns = juce::jmax (2, (int) ghostArea.getWidth() / 2);
                for (int i = 0; i < columns; ++i)
                {
                    const auto t = (float) i / (float) (columns - 1);
                    const auto envelope = std::exp (-3.2f * t) * (0.55f + 0.45f * std::sin (t * 23.0f)) + 0.04f;
                    const auto amplitude = envelope * std::abs (std::sin (t * 410.0f) * 0.7f + std::sin (t * 97.0f) * 0.3f);
                    const auto x = ghostArea.getX() + t * ghostArea.getWidth();
                    ghost.startNewSubPath (x, ghostArea.getCentreY() - amplitude * ghostArea.getHeight() * 0.45f);
                    ghost.lineTo (x, ghostArea.getCentreY() + amplitude * ghostArea.getHeight() * 0.45f);
                }
                g.setColour (traceColour.withAlpha (0.14f));
                g.strokePath (ghost, juce::PathStrokeType (1.2f));
            }

            juce::Path outline, dashed;
            outline.addRoundedRectangle (well, 8.0f);
            const float dashes[] { 5.0f, 4.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
            g.fillPath (dashed);

            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            // (A hint, so a sentence: UI review 9, I9-22.)
            const auto hint = well.withSizeKeepingCentre (well.getWidth() - 20.0f, 40.0f).translated (0.0f, -26.0f);
            IlanaTheme::drawFitted (g, "Drop a .wav or .sfz here", hint.toNearestInt(), juce::Justification::centred, 2);
            const auto pill = juce::Rectangle<float> (110.0f, 28.0f).withCentre (well.getCentre().translated (0.0f, 20.0f));
            g.setColour (IlanaTheme::Ui::raised);
            g.fillRoundedRectangle (pill, 6.0f);
            g.setColour (IlanaTheme::Ui::text);
            g.drawText ("LOAD...", pill, juce::Justification::centred);
            return;
        }

        // The time ruler under the wave (seconds, ticks where they are 70 px
        // apart or more), as a sample editor has it (S14-4).
        if (! compact && ! isGranularMode() && plot.getHeight() > 80.0f)
        {
            const auto ruler = plot.removeFromBottom (12.0f);
            const auto seconds = (double) sample->getNumSamples() / juce::jmax (1.0, sample->sampleRate);
            auto step = 0.001;
            for (const auto candidate : { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 60.0 })
            {
                step = candidate;
                if (candidate / seconds * (double) plot.getWidth() >= 70.0)
                    break;
            }
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            for (double t = 0.0; t <= seconds + 1.0e-9; t += step)
            {
                const auto x = plot.getX() + (float) (t / seconds) * plot.getWidth();
                g.setColour (IlanaTheme::Ui::text3.withAlpha (0.6f));
                g.fillRect (x, ruler.getY(), 1.0f, 4.0f);
                const auto text = step < 1.0 ? juce::String (juce::roundToInt ((float) (t * 1000.0))) + " ms" : juce::String (t, step < 1.0 ? 2 : (step == std::floor (step) ? 0 : 1)) + " s";
                if (x + 44.0f < plot.getRight())
                    g.drawText (text, juce::Rectangle<float> (x + 3.0f, ruler.getY(), 44.0f, ruler.getHeight()), juce::Justification::centredLeft, false);
            }
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

        // While it loops, the loop's span is lit and the handles say LOOP.
        if (loop && ! compact)
        {
            g.setColour (traceColour.withAlpha (0.10f));
            g.fillRect (juce::Rectangle<float> (startX, plot.getY(), juce::jmax (0.0f, endX - startX), plot.getHeight()));
        }

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ startX, centreY }));
        g.fillRect (juce::Rectangle<float> (1.5f, plot.getHeight()).withCentre ({ endX, centreY }));

        // The two handles, flags on the lines you can drag (S14-4).
        if (! compact && plot.getHeight() > 60.0f)
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            const auto startName = loop ? juce::String ("LOOP START") : juce::String ("START");
            const auto endName = loop ? juce::String ("LOOP END") : juce::String ("END");
            const auto startWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), startName) + 10.0f;
            const auto endWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), endName) + 10.0f;
            const auto startFlag = juce::Rectangle<float> (startX, plot.getY(), startWidth, 13.0f);
            const auto endFlag = juce::Rectangle<float> (endX - endWidth, plot.getBottom() - 13.0f, endWidth, 13.0f);
            for (const auto& [flag, name] : { std::pair<juce::Rectangle<float>, juce::String> { startFlag, startName }, { endFlag, endName } })
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRoundedRectangle (flag, 2.0f);
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.drawText (name, flag, juce::Justification::centred, false);
            }
        }

        // A multisample region's own sustain loop (SF2 / SFZ loop points, the
        // ones the voice wraps at): dashed lines and flags while LOOP is on
        // (S15-4). Display only.
        if (loop && ! compact && ! sample->zones.empty() && numSamples > 1)
            if (const auto* zone = sample->zoneFor (60, 100); zone != nullptr && zone->loop && zone->loopEnd > zone->loopStart)
            {
                const auto inX = plot.getX() + juce::jlimit (0.0f, 1.0f, (float) zone->loopStart / (float) numSamples) * plot.getWidth();
                const auto outX = plot.getX() + juce::jlimit (0.0f, 1.0f, (float) zone->loopEnd / (float) numSamples) * plot.getWidth();
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                for (const auto& [x, name, leftOf] : { std::tuple<float, juce::String, bool> { inX, "LOOP IN", false }, { outX, "LOOP OUT", true } })
                {
                    const float dashes[] { 3.0f, 3.0f };
                    g.setColour (traceColour.withAlpha (0.95f));
                    g.drawDashedLine (juce::Line<float> (x, plot.getY(), x, plot.getBottom()), dashes, 2, 1.5f);
                    if (plot.getHeight() > 60.0f)
                    {
                        const auto width = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name) + 10.0f;
                        const auto flag = juce::Rectangle<float> (leftOf ? x - width : x, plot.getY() + 16.0f, width, 13.0f);
                        g.fillRoundedRectangle (flag, 2.0f);
                        g.setColour (juce::Colours::black.withAlpha (0.8f));
                        g.drawText (name, flag, juce::Justification::centred, false);
                    }
                }
            }

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
            drawHeaderText (g, sample->name, (sample->zones.size() > 1 ? juce::String ((int) sample->zones.size()) + juce::String::fromUTF8 (" zones \xc2\xb7 ")
                                                                           : juce::String())
                                                 + (reverse ? juce::String::fromUTF8 ("reversed \xc2\xb7 ") : juce::String()) + juce::String (loop ? "loop" : "1-shot"));
    }

    // The zones as boxes on a keyboard strip: the used key range across
    // (octave lines, C notes named), velocity up; the zone middle C plays
    // lit.
    void drawZones (juce::Graphics& g, const SampleData& sample, juce::Rectangle<float> area) const
    {
        // A plain sample is one zone over every key and velocity.
        std::vector<SampleZone> plain;
        if (sample.zones.empty())
            plain.emplace_back();
        const auto& zones = sample.zones.empty() ? plain : sample.zones;

        auto low = 127, high = 0;
        for (const auto& zone : zones)
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

        const auto* middle = sample.zones.empty() ? &zones.front() : sample.zoneFor (60, 100);
        for (const auto& zone : zones)
        {
            const auto box = juce::Rectangle<float> (xOf (zone.loKey), area.getBottom() - area.getHeight() * (float) zone.hiVel / 127.0f,
                                                     (float) (zone.hiKey - zone.loKey + 1) * keyWidth,
                                                     area.getHeight() * (float) (zone.hiVel - zone.loVel + 1) / 127.0f).reduced (0.5f);
            g.setColour (traceColour.withAlpha (&zone == middle ? 0.55f : 0.22f));
            g.fillRect (box);
            g.setColour (traceColour.withAlpha (0.8f));
            g.drawRect (box, 1.0f);

            // The root note: where the sample plays at its own pitch (review
            // 11, S11-10).
            const auto root = juce::roundToInt ((float) zone.rootNote);
            if (! sample.zones.empty() && root >= low && root <= high)
            {
                const auto x = xOf (root) + keyWidth * 0.5f;
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillEllipse (x - 2.0f, box.getBottom() - 5.0f, 4.0f, 4.0f);
            }
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

    // Whether the frame playing is another than the knob's (modulated): only
    // then does the caption add "playing" (V12-28).
    bool frameIsMoving (int frames) const
    {
        return std::abs (displayedFrame - readValue (frameId)) * (float) juce::jmax (0, frames - 1) >= 0.5f;
    }

    static juce::String frameText (int frames, float position, bool playingNow = true)
    {
        // (The frame playing now, modulation included; the FRAME knob is the
        // set value: S9-12. One caption, "frame 6 of 64", with ", playing" only when modulation moves it off the knob: S10-6, V12-28.)
        return "frame " + juce::String (juce::roundToInt (position * (float) juce::jmax (0, frames - 1)) + 1)
               + " of " + juce::String (frames) + (playingNow ? ", playing" : "");
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

    // The parameter with whatever modulates it right now (LFOs, envelopes,
    // macros), as the voices hear it: the picture of the warp follows a
    // modulated WARP amount (ilana, 2026-10-10: warp modulation didn't
    // animate the oscillator display).
    float readModulated (const juce::String& id) const
    {
        const auto plain = readPlain (id);

        if (const auto config = modRingConfigFor (id); config.destination != 0)
            if (const auto* parameter = processorRef.apvts.getParameter (id))
            {
                const auto range = parameter->getNormalisableRange();
                const auto offset = processorRef.getModDisplay (config.destination);
                return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (plain) + offset));
            }

        return plain;
    }

    // Changes whenever a modulated warp amount moves enough to redraw.
    juce::uint64 warpSignature() const
    {
        const auto stages = readWarp();
        return IlanaAnim::phaseSignature (stages.amount1, 11) ^ IlanaAnim::phaseSignature (stages.amount2, 12);
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
            setTooltip (isPhysicalString() ? "The string after a strike, from where it is struck (EXCITE POS)." : isElectricPiano() ? "The pickup's response across the swing: the shaded bands are a medium and a hard note. "
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

        if (! IlanaAnim::showing (*this))
            return;

        if (gliding || loadFlash > 0.01f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ warpSignature()))
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

        IlanaTrace::paintTrace (g, path, colour, thickness);
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
        stages.amount1 = readModulated (prefix + "_warp_amt");
        stages.mode2 = Warp::modeForStageTwoChoice (readChoice (prefix + "_warp2"));
        stages.amount2 = readModulated (prefix + "_warp2_amt");

        if (! Warp::isOscillatorWarp (stages.mode1)) stages.amount1 = 0.0f;
        if (! Warp::isOscillatorWarp (stages.mode2)) stages.amount2 = 0.0f;
        return stages;
    }

    bool hasWarp() const
    {
        if (subTableMapping || (modeId.isNotEmpty() && ! OscMode::playsWavetable (readChoice (modeId))))
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

        IlanaTrace::paintTrace (g, path, traceColour);
    }

    // The view switch and the table arrows show over a wavetable only, and
    // not in a compact strip.
    void updateViewButtons()
    {
        const auto table = ! compact && isTableMode() && ! subTableMapping;

        for (int view = 0; view < 3; ++view)
        {
            viewButtons[(size_t) view].setVisible (table && ! singleCycle);
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
            // (A sample has no readout line: its picture takes that height, S14-3.)
            bounds.removeFromBottom (isSampleMode() && ! isGranularMode() ? 4.0f : (float) footerHeight + 2.0f);
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
                readout = frameText (table->getNumFrames(), readValue (frameId), false);

                if (canDragWarp())
                    readout << "   WARP " << juce::roundToInt (readPlain (warpAmountId()) * 100.0f) << "%";
            }
            else if (shownViewMode() == 2)
                readout = "HARMONICS 1-" + juce::String (juce::jlimit (8, 128, (int) ((wellArea().getWidth() - 20.0f) / 4.0f)));
            else if (! isStaticTable (table))
                readout = frameText (table->getNumFrames(), displayedFrame, frameIsMoving (table->getNumFrames()));
        }

        g.setColour (dragging ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (readout, footerArea().reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);

        // What a drag on the plot does, at the footer's right while there is
        // room (UI review 9, S9-18: the warp drag had no visible hint).
        if (const auto hint = getDragHint(); hint.isNotEmpty() && ! dragging)
        {
            const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            const auto footer = footerArea().reduced (4.0f, 0.0f);
            const auto used = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), readout);
            // (The full hint, else its short form; a hint that fits neither
            // is left out rather than shrunk.)
            const auto shortHint = hint.contains ("frame") ? juce::String ("drag: frame") : juce::String ("drag: warp");
            // Both gestures keep a middle form before one is dropped (review
            // 11, S11-6).
            const auto bothHint = hint.contains ("frame") && hint.contains ("warp") ? juce::String::fromUTF8 ("drag: frame \xc2\xb7 warp") : shortHint;
            for (const auto& candidate : { hint, bothHint, shortHint })
                if (juce::GlyphArrangement::getStringWidth (font, candidate) + used + 16.0f <= footer.getWidth())
                {
                    g.setFont (font);
                    g.setColour (IlanaTheme::Ui::text3);
                    g.drawText (candidate, footer, juce::Justification::centredRight, false);
                    break;
                }
        }
    }

public:
    // The footer's drag hint: across moves the frame of a table with more
    // than one, up and down the first WARP's amount once a warp is chosen
    // (the UI test reads it).
    juce::String getDragHint() const
    {
        if (compact || ! isTableMode() || subTableMapping || shownViewMode() == 2)
            return {};
        const auto* table = processorRef.getWavetable (resolveTableIndex());
        const auto frames = table != nullptr && ! isStaticTable (table);
        if (frames && canDragWarp())
            return juce::String::fromUTF8 ("drag across: frame \xc2\xb7 up / down: warp");
        // (Both gestures are always told, so the second can be found: V12-16.)
        if (frames)
            return juce::String::fromUTF8 ("drag across: frame \xc2\xb7 up / down: warp (choose a warp first)");
        return canDragWarp() ? "drag up / down: warp" : juce::String();
    }

private:

    void mouseDownOnHeader (const juce::MouseEvent& event)
    {
        if (slim)
        {
            const auto areas = slimAreas();
            const auto at = event.getPosition();

            if (areas.view.contains (at))
            {
                viewPicked = true;
                pickedTable = resolveTableIndex();
                setViewMode ((shownViewMode() + 1) % 3);
            }
            else if (areas.previous.contains (at))
                stepTable (-1);
            else if (areas.next.contains (at))
                stepTable (1);
            else if (areas.name.contains (at) && isTableMode() && ! subTableMapping)
                TableBrowser::show (processorRef, tableId, traceColour, *this);

            return;
        }

        if (tableNameArea.contains (event.getPosition()) && isTableMode() && ! subTableMapping)
            TableBrowser::show (processorRef, tableId, traceColour, *this);
    }

    // A plain wavetable oscillator (not sample, grains, physical or live).
    bool isTableMode() const
    {
        // (FM / DX7 plays the wavetable engine too.)
        return frameId.isNotEmpty() && (modeId.isEmpty() || OscMode::playsWavetable (readChoice (modeId)));
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
    int handleDrag = 0; // 1: START, 2: END while one is dragged on the wave
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
    bool compact = false, pressInHeader = false, singleCycle = false, slim = false;
    static constexpr int headerHeight = 20, footerHeight = 14, slimHeader = 17;
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
