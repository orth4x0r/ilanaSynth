#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/WavetableDoc.h"
#include "IlanaLookAndFeel.h"

// M7.4: the wavetable editor. It edits one of the patch's 16 tables: a frame
// list (add, duplicate, delete, reorder), three ways to shape the selected
// frame (DRAW with the mouse, SPECTRUM bars for each harmonic's level and
// phase, a FORMULA), generators (shapes, morphs between frames) and import,
// export and the user library. Edits reach the sound as you make them.
class WavetableEditor : public juce::Component,
                        private juce::Timer
{
public:
    enum class Mode { Draw = 0, Spectrum, Formula };

    WavetableEditor (IlanaSynthAudioProcessor& p, int slotIn, juce::Colour colourIn)
        : processorRef (p), slot (slotIn), colour (colourIn)
    {
        doc = processorRef.getUserTableDoc (slot);
        if (doc.frames.empty())
            doc = WavetableDoc::sine();
        doc.recipes.resize (doc.frames.size());

        nameEditor.setText (doc.name, false);
        nameEditor.setJustification (juce::Justification::centredLeft);
        nameEditor.onTextChange = [this] { doc.name = nameEditor.getText(); commitSoon(); };
        addAndMakeVisible (nameEditor);

        for (auto* button : { &closeButton, &undoButton, &importButton, &exportButton, &libraryButton,
                              &addButton, &duplicateButton, &deleteButton, &upButton, &downButton,
                              &drawButton, &spectrumButton, &formulaButton, &shapesButton, &morphButton,
                              &applyFrameButton, &applyAllButton })
            addAndMakeVisible (*button);

        closeButton.onClick = [this] { close(); };
        undoButton.onClick = [this] { undo(); };
        importButton.onClick = [this] { showImportMenu(); };
        exportButton.onClick = [this] { exportTable(); };
        libraryButton.onClick = [this] { showLibraryMenu(); };
        addButton.onClick = [this] { edit ([this] { doc.insertFrame (selected + 1, WavetableDoc::sine().frames[0], WavetableDoc::sine().recipes[0]); ++selected; }); };
        duplicateButton.onClick = [this] { edit ([this] { doc.insertFrame (selected + 1, doc.frames[(size_t) selected], doc.recipes[(size_t) selected]); ++selected; }); };
        deleteButton.onClick = [this] { edit ([this] { doc.removeFrame (selected); selected = juce::jmin (selected, doc.getNumFrames() - 1); }); };
        upButton.onClick = [this] { if (selected > 0) edit ([this] { doc.moveFrame (selected, selected - 1); --selected; }); };
        downButton.onClick = [this] { if (selected < doc.getNumFrames() - 1) edit ([this] { doc.moveFrame (selected, selected + 1); ++selected; }); };
        drawButton.onClick = [this] { setMode (Mode::Draw); };
        spectrumButton.onClick = [this] { setMode (Mode::Spectrum); };
        formulaButton.onClick = [this] { setMode (Mode::Formula); };
        shapesButton.onClick = [this] { showShapesMenu(); };
        morphButton.onClick = [this] { showMorphMenu(); };
        applyFrameButton.onClick = [this] { applyFormula (false); };
        applyAllButton.onClick = [this] { applyFormula (true); };

        addButton.setTooltip ("Add a sine frame after the selected one (up to 256)");
        duplicateButton.setTooltip ("Duplicate the selected frame");
        deleteButton.setTooltip ("Delete the selected frame");
        upButton.setTooltip ("Move the selected frame earlier");
        downButton.setTooltip ("Move the selected frame later");
        shapesButton.setTooltip ("Replace the selected frame with a basic shape (as harmonics, so SPECTRUM can edit it)");
        morphButton.setTooltip ("Fill frames between two frames with a crossfade or a spectral morph");
        importButton.setTooltip ("Open a wavetable file, or make one from any recording (the pitch is detected)");
        exportButton.setTooltip ("Save as a .wav that Serum and Vital read (2048-sample frames)");
        libraryButton.setTooltip ("Your wavetable library: Documents/ilanaSynth Wavetables");

        smoothing.setSliderStyle (juce::Slider::LinearHorizontal);
        smoothing.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        smoothing.setRange (0.0, 1.0, 0.01);
        smoothing.setValue (0.0, juce::dontSendNotification);
        smoothing.setTooltip ("Smoothing for drawn frames");
        smoothing.onValueChange = [this]
        {
            auto& recipe = doc.recipes[(size_t) selected];
            if (recipe.kind == FrameRecipe::Kind::Draw)
                edit ([this, &recipe] { recipe.smoothing = (float) smoothing.getValue(); doc.renderFrame (selected); }, false);
        };
        addAndMakeVisible (smoothing);
        smoothLabel.setText ("SMOOTH", juce::dontSendNotification);
        smoothLabel.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        smoothLabel.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        smoothLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (smoothLabel);

        snap.addItemList ({ "Snap off", "Snap 8", "Snap 16", "Snap 32", "Snap 64" }, 1);
        snap.setSelectedId (1, juce::dontSendNotification);
        snap.setTooltip ("Draw in steps across the cycle");
        addAndMakeVisible (snap);

        formulaEditor.setMultiLine (false);
        formulaEditor.setText ("sin(2*pi*x) * (1 - f) + saw(x) * f", false);
        formulaEditor.setTooltip ("x: phase 0..1, f: position through the table 0..1, n: frame index.\n"
                                  "sin cos tan tanh abs sqrt exp log floor fract sign min max pow clamp mix, "
                                  "saw square tri pulse(x, width), pi, ^ for powers");
        formulaEditor.onReturnKey = [this] { applyFormula (false); };
        addAndMakeVisible (formulaEditor);
        addAndMakeVisible (formulaMessage);
        formulaMessage.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        formulaMessage.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);

        frameStrip.owner = this;
        frameView.setViewedComponent (&frameStrip, false);
        frameView.setScrollBarsShown (true, false);
        frameView.setScrollBarThickness (6);
        addAndMakeVisible (frameView);

        canvas.owner = this;
        addAndMakeVisible (canvas);

        setMode (Mode::Draw);
        setWantsKeyboardFocus (true);
        refresh();
    }

    ~WavetableEditor() override
    {
        if (isTimerRunning())
            commitNow();
    }

    std::function<void()> onClose;

    int getSlot() const { return slot; }
    const WavetableDoc& getDoc() const { return doc; }
    int getSelectedFrame() const { return selected; }
    void selectFrame (int index) { selected = juce::jlimit (0, doc.getNumFrames() - 1, index); refresh(); }
    Mode getMode() const { return mode; }

    // Draws a straight line from (x0, y0) to (x1, y1) into the selected
    // frame, as a mouse drag would (x 0..1, y -1..1). Used by the UI test.
    void drawLine (float x0, float y0, float x1, float y1)
    {
        beginDraw();
        drawSegment (x0, y0, x1, y1);
        commitNow();
    }

    // Sets harmonic k's level (0..1), as dragging its bar would.
    void setHarmonic (int harmonic, float level)
    {
        edit ([this, harmonic, level]
        {
            auto& recipe = asHarmonics();
            if ((int) recipe.magnitudes.size() < harmonic)
            {
                recipe.magnitudes.resize ((size_t) harmonic, 0.0f);
                recipe.phases.resize ((size_t) harmonic, 0.0f);
            }
            recipe.magnitudes[(size_t) harmonic - 1] = juce::jlimit (0.0f, 1.0f, level);
            doc.renderFrame (selected);
        });
        commitNow();
    }

    bool applyFormulaText (const juce::String& text, bool allFrames)
    {
        formulaEditor.setText (text, false);
        return applyFormula (allFrames);
    }

    void commitNow()
    {
        stopTimer();
        doc.recipes.resize (doc.frames.size());
        processorRef.setUserTable (slot, doc);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xf0101014));
        g.fillRoundedRectangle (bounds, 8.0f);
        IlanaTheme::paintCard (g, bounds.reduced (2.0f), 8.0f, colour);

        g.setColour (colour);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        g.drawText ("WAVETABLE EDITOR", titleArea, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        auto status = "User " + juce::String (slot + 1) + "  |  " + juce::String (doc.getNumFrames())
                      + (doc.getNumFrames() == 1 ? " frame" : " frames") + "  |  frame " + juce::String (selected + 1);
        if (doc.getNumFrames() >= WavetableDoc::largeTableFrames)
            status << "  |  256 frames is about 1 MB in the patch";
        g.drawText (status, statusArea, juce::Justification::centredLeft);

        const auto& recipe = doc.recipes[(size_t) selected];
        const char* kinds[] { "raw (imported or morphed)", "drawn", "harmonics", "formula" };
        g.drawText (juce::String ("Frame source: ") + kinds[(int) recipe.kind], kindArea, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14, 10);
        auto header = area.removeFromTop (30);
        titleArea = header.removeFromLeft (170);
        closeButton.setBounds (header.removeFromRight (70).reduced (2));
        header.removeFromRight (6);
        libraryButton.setBounds (header.removeFromRight (90).reduced (2));
        exportButton.setBounds (header.removeFromRight (80).reduced (2));
        importButton.setBounds (header.removeFromRight (80).reduced (2));
        undoButton.setBounds (header.removeFromRight (64).reduced (2));
        header.removeFromRight (6);
        nameEditor.setBounds (header.reduced (2));
        statusArea = area.removeFromTop (22);
        area.removeFromTop (4);

        auto left = area.removeFromLeft (150);
        auto frameButtons = left.removeFromBottom (58);
        auto row1 = frameButtons.removeFromTop (28);
        const auto third = row1.getWidth() / 3;
        addButton.setBounds (row1.removeFromLeft (third).reduced (2));
        duplicateButton.setBounds (row1.removeFromLeft (third).reduced (2));
        deleteButton.setBounds (row1.reduced (2));
        auto row2 = frameButtons.removeFromTop (28);
        upButton.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (2));
        downButton.setBounds (row2.reduced (2));
        frameView.setBounds (left.reduced (0, 2));
        frameStrip.setSize (frameView.getWidth() - 8, juce::jmax (frameView.getHeight(), doc.getNumFrames() * FrameStrip::rowHeight));
        area.removeFromLeft (10);

        auto modes = area.removeFromTop (30);
        drawButton.setBounds (modes.removeFromLeft (84).reduced (2));
        spectrumButton.setBounds (modes.removeFromLeft (96).reduced (2));
        formulaButton.setBounds (modes.removeFromLeft (90).reduced (2));
        modes.removeFromLeft (12);
        morphButton.setBounds (modes.removeFromRight (84).reduced (2));
        shapesButton.setBounds (modes.removeFromRight (84).reduced (2));
        kindArea = modes;

        auto tools = area.removeFromBottom (32);
        area.removeFromBottom (4);
        if (mode == Mode::Formula)
        {
            applyAllButton.setBounds (tools.removeFromRight (110).reduced (2));
            applyFrameButton.setBounds (tools.removeFromRight (110).reduced (2));
            formulaEditor.setBounds (tools.reduced (2));
            formulaMessage.setBounds (area.removeFromBottom (20));
        }
        else
        {
            snap.setBounds (tools.removeFromLeft (110).reduced (2));
            tools.removeFromLeft (10);
            smoothLabel.setBounds (tools.removeFromLeft (64));
            smoothing.setBounds (tools.removeFromLeft (200).reduced (2));
        }
        canvas.setBounds (area);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
        {
            undo();
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            selectFrame (selected + (key == juce::KeyPress::upKey ? -1 : 1));
            return true;
        }
        return false;
    }

private:
    // The frame list: every frame's waveform, the selected one highlighted.
    struct FrameStrip : juce::Component
    {
        static constexpr int rowHeight = 46;
        WavetableEditor* owner = nullptr;

        void paint (juce::Graphics& g) override
        {
            const auto& frames = owner->doc.frames;
            const auto clip = g.getClipBounds();
            for (int i = juce::jmax (0, clip.getY() / rowHeight);
                 i < juce::jmin ((int) frames.size(), clip.getBottom() / rowHeight + 1); ++i)
            {
                auto row = juce::Rectangle<float> (0.0f, (float) (i * rowHeight), (float) getWidth(), (float) rowHeight).reduced (2.0f);
                const auto isSelected = i == owner->selected;
                g.setColour (isSelected ? owner->colour.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (row, 4.0f);
                if (isSelected)
                {
                    g.setColour (owner->colour);
                    g.drawRoundedRectangle (row.reduced (0.5f), 4.0f, 1.2f);
                }
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawText (juce::String (i + 1), row.removeFromLeft (22.0f).toNearestInt(), juce::Justification::centred);
                owner->paintWave (g, frames[(size_t) i], row.reduced (2.0f, 4.0f),
                                  isSelected ? owner->colour : owner->colour.withAlpha (0.7f), 1.2f, 64);
            }
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            owner->selectFrame (event.y / rowHeight);
        }
    };

    // The selected frame, drawn over its neighbours; the mouse draws into it
    // (DRAW) or sets its harmonics (SPECTRUM).
    struct Canvas : juce::Component
    {
        WavetableEditor* owner = nullptr;

        void paint (juce::Graphics& g) override
        {
            owner->paintCanvas (g, getLocalBounds().toFloat());
        }

        void mouseDown (const juce::MouseEvent& event) override { owner->canvasMouse (event, true); }
        void mouseDrag (const juce::MouseEvent& event) override { owner->canvasMouse (event, false); }
        void mouseUp (const juce::MouseEvent&) override { owner->commitSoon (1); }
    };

    static constexpr int drawPoints = 256;
    static constexpr int shownHarmonics = 64;

    void paintWave (juce::Graphics& g, const std::vector<float>& frame, juce::Rectangle<float> area,
                    juce::Colour lineColour, float thickness, int resolution) const
    {
        auto peak = 1.0f;
        for (auto value : frame)
            peak = juce::jmax (peak, std::abs (value));
        juce::Path path;
        for (int i = 0; i <= resolution; ++i)
        {
            const auto index = juce::jmin ((int) frame.size() - 1, i * (int) frame.size() / resolution);
            const auto x = area.getX() + area.getWidth() * (float) i / (float) resolution;
            const auto y = area.getCentreY() - frame[(size_t) index] / peak * area.getHeight() * 0.47f;
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        g.setColour (lineColour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    }

    void paintCanvas (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        IlanaTheme::paintWell (g, bounds, 6.0f);
        auto area = bounds.reduced (10.0f);

        if (mode == Mode::Spectrum)
        {
            paintSpectrum (g, area);
            return;
        }

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        const auto steps = snapSteps();
        for (int i = 1; i < (steps > 0 ? steps : 8); ++i)
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / (float) (steps > 0 ? steps : 8)),
                                area.getY(), area.getBottom());

        if (selected > 0)
            paintWave (g, doc.frames[(size_t) selected - 1], area, colour.withAlpha (0.15f), 1.0f, 256);
        if (selected + 1 < doc.getNumFrames())
            paintWave (g, doc.frames[(size_t) selected + 1], area, colour.withAlpha (0.15f), 1.0f, 256);
        paintWave (g, doc.frames[(size_t) selected], area, colour, 2.0f, 512);

        if (mode == Mode::Draw)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("Drag to draw the cycle", area.removeFromBottom (18.0f).toNearestInt(), juce::Justification::centredRight);
        }
    }

    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
    {
        std::vector<float> magnitudes, phases;
        const auto& recipe = doc.recipes[(size_t) selected];
        if (recipe.kind == FrameRecipe::Kind::Harmonics)
        {
            magnitudes = recipe.magnitudes;
            phases = recipe.phases;
        }
        else
            WavetableDoc::analyse (doc.frames[(size_t) selected], magnitudes, phases, shownHarmonics);

        auto peak = 1.0e-6f;
        for (auto value : magnitudes)
            peak = juce::jmax (peak, value);

        auto phaseArea = area.removeFromBottom (area.getHeight() * 0.22f);
        area.removeFromBottom (6.0f);
        const auto width = area.getWidth() / (float) shownHarmonics;
        for (int k = 0; k < shownHarmonics; ++k)
        {
            const auto level = k < (int) magnitudes.size() ? magnitudes[(size_t) k] / peak : 0.0f;
            const auto db = level > 1.0e-4f ? juce::jlimit (0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels (level) / 60.0f) : 0.0f;
            auto bar = juce::Rectangle<float> (area.getX() + (float) k * width, area.getY(), width, area.getHeight()).reduced (1.0f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (bar);
            g.setColour (colour.withAlpha (0.85f));
            g.fillRect (bar.removeFromBottom (bar.getHeight() * db));

            const auto phase = k < (int) phases.size() ? phases[(size_t) k] : 0.0f;
            auto phaseBar = juce::Rectangle<float> (phaseArea.getX() + (float) k * width, phaseArea.getY(), width, phaseArea.getHeight()).reduced (1.0f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (phaseBar);
            const auto wrapped = std::remainder (phase, juce::MathConstants<float>::twoPi) / juce::MathConstants<float>::twoPi + 0.5f;
            g.setColour (colour.withAlpha (level > 1.0e-4f ? 0.7f : 0.2f));
            g.fillRect (phaseBar.withY (phaseBar.getBottom() - phaseBar.getHeight() * wrapped).withHeight (2.0f));
        }
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("Harmonics 1-64: drag the bars for level (dB), the strip below for phase",
                    area.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredRight);
    }

    int snapSteps() const
    {
        switch (snap.getSelectedId())
        {
            case 2: return 8;
            case 3: return 16;
            case 4: return 32;
            case 5: return 64;
            default: return 0;
        }
    }

    // The selected frame as a Draw recipe (a drawn copy of whatever it was).
    void beginDraw()
    {
        pushUndo();
        auto& recipe = doc.recipes[(size_t) selected];
        if (recipe.kind != FrameRecipe::Kind::Draw || (int) recipe.points.size() != drawPoints * 2)
        {
            const auto& frame = doc.frames[(size_t) selected];
            auto peak = 1.0f;
            for (auto value : frame)
                peak = juce::jmax (peak, std::abs (value));
            FrameRecipe drawn;
            drawn.kind = FrameRecipe::Kind::Draw;
            drawn.smoothing = (float) smoothing.getValue();
            for (int i = 0; i < drawPoints; ++i)
            {
                drawn.points.push_back ((float) i / (float) drawPoints);
                drawn.points.push_back (frame[(size_t) (i * WavetableDoc::frameSize / drawPoints)] / peak);
            }
            recipe = std::move (drawn);
        }
        doc.factoryIndex = -1;
    }

    void drawSegment (float x0, float y0, float x1, float y1)
    {
        auto& points = doc.recipes[(size_t) selected].points;
        const auto steps = snapSteps();
        if (x1 < x0)
        {
            std::swap (x0, x1);
            std::swap (y0, y1);
        }
        const auto first = juce::jlimit (0, drawPoints - 1, (int) std::floor (x0 * drawPoints));
        const auto last = juce::jlimit (0, drawPoints - 1, (int) std::ceil (x1 * drawPoints));
        for (int i = first; i <= last; ++i)
        {
            const auto x = (float) i / (float) drawPoints;
            const auto t = x1 > x0 ? juce::jlimit (0.0f, 1.0f, (x - x0) / (x1 - x0)) : 1.0f;
            points[(size_t) i * 2 + 1] = juce::jlimit (-1.0f, 1.0f, y0 + (y1 - y0) * t);
        }
        if (steps > 0)
        {
            // Stairs: every point in a step takes the step's first value.
            const auto perStep = drawPoints / steps;
            for (int step = first / perStep; step <= last / perStep && step < steps; ++step)
                for (int i = 1; i < perStep; ++i)
                    points[(size_t) (step * perStep + i) * 2 + 1] = points[(size_t) (step * perStep) * 2 + 1];
        }
        doc.renderFrame (selected);
        canvas.repaint();
        frameStrip.repaint();
    }

    void canvasMouse (const juce::MouseEvent& event, bool down)
    {
        const auto area = canvas.getLocalBounds().toFloat().reduced (10.0f);
        const auto x = juce::jlimit (0.0f, 1.0f, (event.position.x - area.getX()) / area.getWidth());

        if (mode == Mode::Draw)
        {
            const auto y = juce::jlimit (-1.0f, 1.0f, (area.getCentreY() - event.position.y) / (area.getHeight() * 0.47f));
            if (down)
            {
                beginDraw();
                lastDraw = { x, y };
            }
            drawSegment (lastDraw.x, lastDraw.y, x, y);
            lastDraw = { x, y };
            return;
        }

        if (mode == Mode::Spectrum)
        {
            auto bars = area;
            auto phaseArea = bars.removeFromBottom (bars.getHeight() * 0.22f);
            bars.removeFromBottom (6.0f);
            const auto harmonic = juce::jlimit (1, shownHarmonics, 1 + (int) (x * (float) shownHarmonics));
            if (down)
            {
                pushUndo();
                spectrumDragPhase = phaseArea.contains (event.position);
            }
            auto& recipe = asHarmonics();
            if ((int) recipe.magnitudes.size() < harmonic)
            {
                recipe.magnitudes.resize ((size_t) harmonic, 0.0f);
                recipe.phases.resize ((size_t) harmonic, 0.0f);
            }
            if (spectrumDragPhase)
            {
                const auto t = juce::jlimit (0.0f, 1.0f, (phaseArea.getBottom() - event.position.y) / phaseArea.getHeight());
                recipe.phases[(size_t) harmonic - 1] = (t - 0.5f) * juce::MathConstants<float>::twoPi;
            }
            else
            {
                // Bars are in dB relative to the loudest harmonic (60 dB tall).
                auto peak = 1.0e-6f;
                for (auto value : recipe.magnitudes)
                    peak = juce::jmax (peak, value);
                const auto t = juce::jlimit (0.0f, 1.0f, (bars.getBottom() - event.position.y) / bars.getHeight());
                recipe.magnitudes[(size_t) harmonic - 1] = t <= 0.01f ? 0.0f : peak * juce::Decibels::decibelsToGain ((t - 1.0f) * 60.0f);
            }
            doc.renderFrame (selected);
            canvas.repaint();
            frameStrip.repaint();
        }
    }

    FrameRecipe& asHarmonics()
    {
        auto& recipe = doc.recipes[(size_t) selected];
        if (recipe.kind != FrameRecipe::Kind::Harmonics)
        {
            FrameRecipe harmonics;
            harmonics.kind = FrameRecipe::Kind::Harmonics;
            WavetableDoc::analyse (doc.frames[(size_t) selected], harmonics.magnitudes, harmonics.phases);
            recipe = std::move (harmonics);
        }
        doc.factoryIndex = -1;
        return recipe;
    }

    bool applyFormula (bool allFrames)
    {
        const auto text = formulaEditor.getText().trim();
        juce::String error;
        WavetableDoc::renderFormula (text, 0.0, 0, &error);
        if (error.isNotEmpty() || text.isEmpty())
        {
            formulaMessage.setText (text.isEmpty() ? "Type a formula first" : "Formula: " + error, juce::dontSendNotification);
            return false;
        }
        formulaMessage.setText (allFrames ? "Applied to every frame (f runs 0..1 through the table)" : "Applied to frame "
                                                + juce::String (selected + 1), juce::dontSendNotification);
        edit ([this, text, allFrames]
        {
            for (int i = allFrames ? 0 : selected; i <= (allFrames ? doc.getNumFrames() - 1 : selected); ++i)
            {
                FrameRecipe recipe;
                recipe.kind = FrameRecipe::Kind::Formula;
                recipe.formula = text;
                doc.recipes[(size_t) i] = recipe;
                doc.renderFrame (i);
            }
        });
        commitNow();
        return true;
    }

    void showShapesMenu()
    {
        juce::PopupMenu menu;
        const char* names[] { "Sine", "Saw", "Square", "Triangle", "Pulse 25%", "Organ", "Hollow", "Formant ah" };
        for (int i = 0; i < 8; ++i)
            menu.addItem (i + 1, names[i]);
        juce::Component::SafePointer<WavetableEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapesButton), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->applyShape (result - 1);
        });
    }

public:
    // One of the basic shapes, as harmonics (index as in the SHAPES menu).
    void applyShape (int shape)
    {
        FrameRecipe recipe;
        recipe.kind = FrameRecipe::Kind::Harmonics;
        const auto count = 256;
        recipe.magnitudes.assign ((size_t) count, 0.0f);
        recipe.phases.assign ((size_t) count, 0.0f);
        for (int k = 1; k <= count; ++k)
        {
            auto& level = recipe.magnitudes[(size_t) k - 1];
            switch (shape)
            {
                case 0: level = k == 1 ? 1.0f : 0.0f; break;
                case 1: level = 1.0f / (float) k; break;
                case 2: level = (k % 2 == 1) ? 1.0f / (float) k : 0.0f; break;
                case 3:
                    level = (k % 2 == 1) ? 1.0f / (float) (k * k) : 0.0f;
                    recipe.phases[(size_t) k - 1] = ((k / 2) % 2 == 1) ? juce::MathConstants<float>::pi : 0.0f;
                    break;
                case 4: level = std::abs (std::sin (juce::MathConstants<float>::pi * (float) k * 0.25f)) / (float) k; break;
                case 5: level = (k == 1 || k == 2 || k == 3 || k == 4 || k == 6 || k == 8) ? 1.0f / std::sqrt ((float) k) : 0.0f; break;
                case 6: level = (k % 2 == 1) ? 1.0f / std::sqrt ((float) k) * std::exp (-(float) k / 24.0f) : 0.0f; break;
                case 7:
                {
                    // Vowel "ah": formants near 700 and 1200 Hz on a 110 Hz voice.
                    const auto hz = (float) k * 110.0f;
                    const auto f1 = std::exp (-std::pow ((hz - 700.0f) / 160.0f, 2.0f));
                    const auto f2 = 0.6f * std::exp (-std::pow ((hz - 1200.0f) / 200.0f, 2.0f));
                    level = (f1 + f2 + 0.03f) / std::sqrt ((float) k);
                    break;
                }
                default: break;
            }
        }
        auto last = count;
        while (last > 1 && recipe.magnitudes[(size_t) last - 1] == 0.0f)
            --last;
        recipe.magnitudes.resize ((size_t) last);
        recipe.phases.resize ((size_t) last);
        edit ([this, recipe] { doc.recipes[(size_t) selected] = recipe; doc.renderFrame (selected); });
        commitNow();
    }

    // Grows or shrinks the table to count frames, keeping the first and
    // last, and morphs everything between them.
    void morphTable (int count, bool spectral)
    {
        count = juce::jlimit (3, WavetableDoc::maxFrames, count);
        edit ([this, count, spectral]
        {
            auto first = doc.frames.front();
            auto last = doc.frames.back();
            auto firstRecipe = doc.recipes.front();
            auto lastRecipe = doc.recipes.back();
            doc.frames.assign ((size_t) count, first);
            doc.recipes.assign ((size_t) count, {});
            doc.recipes.front() = firstRecipe;
            doc.frames.back() = last;
            doc.recipes.back() = lastRecipe;
            doc.morph (0, count - 1, spectral);
            selected = juce::jmin (selected, count - 1);
        });
        commitNow();
    }

private:
    void showMorphMenu()
    {
        juce::PopupMenu menu;
        const auto next = nextKeyframe();
        menu.addItem (1, "Crossfade frame " + juce::String (selected + 1) + " to " + juce::String (next + 1), next - selected >= 2);
        menu.addItem (2, "Spectral morph frame " + juce::String (selected + 1) + " to " + juce::String (next + 1), next - selected >= 2);
        menu.addSeparator();
        juce::PopupMenu crossfade, spectral;
        for (int count : { 8, 16, 32, 64, 128, 256 })
        {
            crossfade.addItem (100 + count, juce::String (count) + " frames");
            spectral.addItem (1000 + count, juce::String (count) + " frames");
        }
        menu.addSubMenu ("Whole table: crossfade first to last", crossfade);
        menu.addSubMenu ("Whole table: spectral morph first to last", spectral);
        juce::Component::SafePointer<WavetableEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&morphButton), [safe, next] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1 || result == 2)
            {
                safe->edit ([safe, next, result] { safe->doc.morph (safe->selected, next, result == 2); });
                safe->commitNow();
            }
            else if (result >= 1000)
                safe->morphTable (result - 1000, true);
            else if (result >= 100)
                safe->morphTable (result - 100, false);
        });
    }

    // The next frame after the selected one that isn't a morph result (the
    // other end of a morph), or the last frame.
    int nextKeyframe() const
    {
        for (int i = selected + 2; i < doc.getNumFrames(); ++i)
            if (doc.recipes[(size_t) i].kind != FrameRecipe::Kind::Raw)
                return i;
        return doc.getNumFrames() - 1;
    }

    void showImportMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Open wavetable file (Serum, Vital, ilanaSynth...)");
        menu.addItem (2, "Make a wavetable from any audio (resynthesis)...");
        juce::Component::SafePointer<WavetableEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&importButton), [safe] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            safe->chooser = std::make_unique<juce::FileChooser> ("Import a wavetable",
                                                                 juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                                 "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            const auto mode = result == 2 ? Wavetable::LoadMode::Resynthesize : Wavetable::LoadMode::Automatic;
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe, mode] (const juce::FileChooser& chooser)
            {
                if (safe != nullptr && chooser.getResult().existsAsFile())
                    safe->importFile (chooser.getResult(), mode);
            });
        });
    }

public:
    bool importFile (const juce::File& file, Wavetable::LoadMode loadMode)
    {
        WavetableDoc loaded;
        if (! WavetableDoc::loadFromFile (file, loaded, loadMode))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import",
                                                    "Could not read " + file.getFileName() + ".");
            return false;
        }
        edit ([this, &loaded] { doc = std::move (loaded); selected = 0; });
        nameEditor.setText (doc.name, false);
        commitNow();
        return true;
    }

private:
    void exportTable()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export wavetable",
                                                       WavetableDoc::getLibraryFolder().getChildFile (
                                                           juce::File::createLegalFileName (doc.name.isNotEmpty() ? doc.name : juce::String ("Wavetable")) + ".wav"),
                                                       "*.wav");
        juce::Component::SafePointer<WavetableEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fileChooser)
        {
            auto file = fileChooser.getResult();
            if (safe == nullptr || file == juce::File())
                return;
            file = file.withFileExtension ("wav");
            if (! safe->doc.exportWav (file))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export",
                                                        "Could not write " + file.getFullPathName() + ".");
        });
    }

    void showLibraryMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Save to library");
        menu.addItem (2, "Open the library folder");
        const auto files = WavetableDoc::getLibraryFolder().findChildFiles (juce::File::findFiles, false, "*.wav");
        juce::Array<juce::File> sorted (files);
        sorted.sort();
        if (! sorted.isEmpty())
        {
            menu.addSeparator();
            menu.addSectionHeader ("Load from library");
            for (int i = 0; i < juce::jmin (200, sorted.size()); ++i)
                menu.addItem (100 + i, sorted[i].getFileNameWithoutExtension()
                                           + (sorted[i].withFileExtension ("ilwt").existsAsFile() ? "  (editable)" : ""));
        }
        juce::Component::SafePointer<WavetableEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libraryButton), [safe, sorted] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1)
            {
                juce::File written;
                if (safe->doc.saveToLibrary (safe->doc.name, &written))
                    safe->formulaMessage.setText ("Saved " + written.getFileName(), juce::dontSendNotification);
                else
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Library",
                                                            "Could not save to " + WavetableDoc::getLibraryFolder().getFullPathName() + ".");
            }
            else if (result == 2)
            {
                WavetableDoc::getLibraryFolder().createDirectory();
                WavetableDoc::getLibraryFolder().startAsProcess();
            }
            else if (result - 100 < sorted.size())
                safe->importFile (sorted[result - 100], Wavetable::LoadMode::Frames);
        });
    }

    void setMode (Mode newMode)
    {
        mode = newMode;
        for (auto* button : { &drawButton, &spectrumButton, &formulaButton })
            button->setToggleState (false, juce::dontSendNotification);
        (mode == Mode::Draw ? drawButton : mode == Mode::Spectrum ? spectrumButton : formulaButton)
            .setToggleState (true, juce::dontSendNotification);
        const auto formula = mode == Mode::Formula;
        formulaEditor.setVisible (formula);
        formulaMessage.setVisible (formula);
        applyFrameButton.setVisible (formula);
        applyAllButton.setVisible (formula);
        snap.setVisible (mode == Mode::Draw);
        smoothing.setVisible (mode == Mode::Draw);
        smoothLabel.setVisible (mode == Mode::Draw);
        resized();
        repaint();
    }

    // Runs a change to the doc with undo, then refreshes the view and sends
    // the table to the sound shortly.
    template <typename Change>
    void edit (Change&& change, bool withUndo = true)
    {
        if (withUndo)
            pushUndo();
        change();
        doc.factoryIndex = -1; // edited: no longer the factory table
        doc.recipes.resize (doc.frames.size());
        selected = juce::jlimit (0, juce::jmax (0, doc.getNumFrames() - 1), selected);
        refresh();
        commitSoon();
    }

    void pushUndo()
    {
        undoStack.push_back (doc);
        undoSelected.push_back (selected);
        if (undoStack.size() > 30)
        {
            undoStack.erase (undoStack.begin());
            undoSelected.erase (undoSelected.begin());
        }
    }

    void undo()
    {
        if (undoStack.empty())
            return;
        doc = std::move (undoStack.back());
        selected = undoSelected.back();
        undoStack.pop_back();
        undoSelected.pop_back();
        nameEditor.setText (doc.name, false);
        refresh();
        commitNow();
    }

    void refresh()
    {
        selected = juce::jlimit (0, juce::jmax (0, doc.getNumFrames() - 1), selected);
        const auto& recipe = doc.recipes[(size_t) selected];
        if (recipe.kind == FrameRecipe::Kind::Draw)
            smoothing.setValue (recipe.smoothing, juce::dontSendNotification);
        if (recipe.kind == FrameRecipe::Kind::Formula)
            formulaEditor.setText (recipe.formula, false);
        addButton.setEnabled (doc.getNumFrames() < WavetableDoc::maxFrames);
        duplicateButton.setEnabled (doc.getNumFrames() < WavetableDoc::maxFrames);
        deleteButton.setEnabled (doc.getNumFrames() > 1);
        upButton.setEnabled (selected > 0);
        downButton.setEnabled (selected < doc.getNumFrames() - 1);
        undoButton.setEnabled (! undoStack.empty());
        frameStrip.setSize (juce::jmax (10, frameView.getWidth() - 8),
                            juce::jmax (frameView.getHeight(), doc.getNumFrames() * FrameStrip::rowHeight));
        const auto rowTop = selected * FrameStrip::rowHeight;
        if (rowTop < frameView.getViewPositionY() || rowTop + FrameStrip::rowHeight > frameView.getViewPositionY() + frameView.getHeight())
            frameView.setViewPosition (0, juce::jmax (0, rowTop - frameView.getHeight() / 2));
        frameStrip.repaint();
        canvas.repaint();
        repaint();
    }

    void commitSoon (int milliseconds = 120)
    {
        startTimer (milliseconds);
    }

    void timerCallback() override
    {
        commitNow();
    }

    void close()
    {
        if (isTimerRunning())
            commitNow();
        if (onClose != nullptr)
        {
            auto callback = onClose;
            juce::MessageManager::callAsync (callback);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    int slot = 0;
    juce::Colour colour;
    WavetableDoc doc;
    int selected = 0;
    Mode mode = Mode::Draw;
    bool spectrumDragPhase = false;
    juce::Point<float> lastDraw;
    std::vector<WavetableDoc> undoStack;
    std::vector<int> undoSelected;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> titleArea, statusArea, kindArea;
    juce::TextEditor nameEditor;
    juce::TextButton closeButton { "CLOSE" }, undoButton { "UNDO" }, importButton { "IMPORT" },
        exportButton { "EXPORT" }, libraryButton { "LIBRARY" };
    juce::TextButton addButton { "ADD" }, duplicateButton { "DUP" }, deleteButton { "DEL" },
        upButton { "UP" }, downButton { "DOWN" };
    juce::TextButton drawButton { "DRAW" }, spectrumButton { "SPECTRUM" }, formulaButton { "FORMULA" },
        shapesButton { "SHAPES" }, morphButton { "MORPH" };
    juce::TextButton applyFrameButton { "THIS FRAME" }, applyAllButton { "ALL FRAMES" };
    juce::Slider smoothing;
    juce::Label smoothLabel;
    juce::ComboBox snap;
    juce::TextEditor formulaEditor;
    juce::Label formulaMessage;
    FrameStrip frameStrip;
    juce::Viewport frameView;
    Canvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableEditor)
};
