#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/Dx7Engine.h"
#include "../dsp/OperatorEgParams.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"

// The Operator Env (the DX7's envelope generator, OperatorEgParams.h) as the
// engine plays it, for the editor: an operator's level envelope (C3 at
// velocity 100, its keyboard scaling and output level included) or the
// voice's pitch envelope, with where each stage ends. UI review 6: it is
// edited like the synth's own envelopes (drag a point: across for its time,
// up and down for its level) and labelled in ms and dB, the 0-99 values in
// the tooltips.
namespace OperatorEnv
{
constexpr double displayRate = 44100.0;
constexpr int displayNote = 60, displayVelocity = 100;
// A stage that takes longer is drawn to here (a rate of 0 never ends).
constexpr double stageCapSeconds = 20.0;

inline double blocksToSeconds (int blocks) { return blocks * (double) Dx7::block / displayRate; }

// Stage names in the synth's words (R1-R4 move to L1-L4 on a DX7).
inline const char* rateName (int stage)
{
    static const char* const names[] { "ATTACK", "DECAY 1", "DECAY 2", "RELEASE" };
    return names[juce::jlimit (0, 3, stage)];
}

inline const char* levelName (int stage)
{
    static const char* const names[] { "PEAK", "MID", "SUSTAIN", "END" };
    return names[juce::jlimit (0, 3, stage)];
}

// What the envelope is built from (one operator's settings, or the pitch
// envelope's), read from the patch.
struct Settings
{
    std::array<int, 4> rates {}, levels {};
    int outLevel = 0, rateScale = 0;
    bool pitch = false;

    bool operator== (const Settings& other) const
    {
        return rates == other.rates && levels == other.levels && outLevel == other.outLevel
               && rateScale == other.rateScale && pitch == other.pitch;
    }
    bool operator!= (const Settings& other) const { return ! (*this == other); }
};

inline int readInt (const IlanaSynthAudioProcessor& p, const juce::String& id)
{
    const auto* value = p.apvts.getRawParameterValue (id);
    return value != nullptr ? juce::roundToInt (value->load()) : 0;
}

// An oscillator prefix ("osc1", "sub", ...), or empty for the pitch envelope.
inline Settings read (const IlanaSynthAudioProcessor& p, const juce::String& prefix)
{
    Settings s;
    s.pitch = prefix.isEmpty();
    if (s.pitch)
    {
        for (int i = 0; i < 4; ++i)
        {
            s.rates[(size_t) i] = readInt (p, OperatorEg::voiceFields()[(size_t) i].suffix);
            s.levels[(size_t) i] = readInt (p, OperatorEg::voiceFields()[(size_t) (4 + i)].suffix);
        }
        return s;
    }

    const auto field = [&p, &prefix] (int index) { return readInt (p, prefix + OperatorEg::operatorFields()[(size_t) index].suffix); };
    for (int i = 0; i < 4; ++i)
    {
        s.rates[(size_t) i] = field (i);
        s.levels[(size_t) i] = field (4 + i);
    }
    // The key its scaling follows: C3 moved by KEY OFFSET, as the engine does.
    const auto key = juce::jlimit (0, 127, displayNote + readInt (p, OperatorEg::keyOffsetId));
    auto out = Dx7::scaleOutLevel (field (16)) + Dx7::scaleLevel (key, field (8), field (9), field (10), field (11), field (12));
    out = std::min (127, out) << 5;
    s.outLevel = std::max (0, out + Dx7::scaleVelocity (displayVelocity, field (15)));
    s.rateScale = Dx7::scaleRate (key, field (13));
    return s;
}

// One run of the engine's envelope: its value every 64 samples (an
// operator's gain in octaves, 0 at full; the pitch in octaves) and the block
// where each stage ends (L1, L2 and L3 reached, then the release's end; the
// key goes up at the end of stage 3).
struct Run
{
    std::vector<double> values;
    std::array<int, 4> stageEnd {};
    std::array<bool, 4> endless {}; // drawn only to stageCapSeconds

    int stageStart (int stage) const { return stage == 0 ? 0 : stageEnd[(size_t) stage - 1]; }
    double stageSeconds (int stage) const { return blocksToSeconds (stageEnd[(size_t) stage] - stageStart (stage)); }
};

// An operator's level as the graph shows it: octaves below full (out 99,
// level 99), as the engine's exp2 (level - 14).
inline double operatorValue (std::int32_t level) { return (double) level / (1 << 24) - 15.0; }

// Where a level of 0-99 puts an operator's stage target (the engine's own
// formula) or the pitch, in the graph's units.
inline double levelValue (const Settings& s, int level)
{
    if (s.pitch)
        return Dx7::PitchEnvelope::levelSteps (level) / 32.0;
    auto actual = (Dx7::scaleOutLevel (level) >> 1);
    actual = (actual << 6) + s.outLevel - 4256;
    return std::max (16, actual) / 256.0 - 15.0;
}

inline Run run (const Settings& s, int lastStage = 3)
{
    Run result;
    const auto cap = (int) (stageCapSeconds * displayRate / Dx7::block);
    int r[4], l[4];
    for (int i = 0; i < 4; ++i)
    {
        r[i] = s.rates[(size_t) i];
        l[i] = s.levels[(size_t) i];
    }

    const auto go = [&] (auto& env, auto&& value, double start)
    {
        result.values.push_back (start);
        auto stage = 0;
        for (; stage < 3 && stage <= lastStage; ++stage)
        {
            auto blocks = 0;
            while (env.getStage() == stage && blocks < cap)
            {
                result.values.push_back (value (env.next()));
                ++blocks;
            }
            result.stageEnd[(size_t) stage] = (int) result.values.size() - 1;
            if (blocks >= cap)
            {
                result.endless[(size_t) stage] = true;
                break;
            }
        }
        for (auto later = stage + 1; later < 3; ++later)
            result.stageEnd[(size_t) later] = (int) result.values.size() - 1;
        if (lastStage < 3)
            return;

        env.keyUp();
        auto blocks = 0;
        while (env.getStage() == 3 && blocks < cap)
        {
            result.values.push_back (value (env.next()));
            ++blocks;
        }
        result.stageEnd[3] = (int) result.values.size() - 1;
        result.endless[3] = blocks >= cap;
    };

    if (s.pitch)
    {
        Dx7::PitchEnvelope env;
        env.init (r, l, displayRate);
        go (env, [] (std::int32_t level) { return (double) level / (1 << 24); }, levelValue (s, l[3]));
    }
    else
    {
        Dx7::Envelope env;
        env.init (r, l, s.outLevel, s.rateScale, displayRate);
        go (env, [] (std::int32_t level) { return operatorValue (level); }, operatorValue (0));
    }
    return result;
}

// "4 ms", "1.2 s", or "> 20 s" for a stage that doesn't end.
inline juce::String formatSeconds (double seconds, bool endless = false)
{
    if (endless)
        return "> " + juce::String ((int) stageCapSeconds) + " s";
    if (seconds < 0.0005)
        return "0 ms";
    if (seconds < 1.0)
        return juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";
    return describeNumber ((float) seconds, 1) + " s";
}

// A stage's time as its knob reads it. A flat stage (the same level at both
// ends) takes no time whatever its rate, so its knob would read "0 ms" at
// every angle (UI review 8, I8-3); it reads instead the time a full sweep
// (silence to full, or the whole pitch range) takes at its rate, and says
// it is flat, so the knob always shows what it holds.
struct StageTime
{
    double seconds = 0.0;
    bool endless = false, flat = false;
};

inline StageTime stageTime (const Settings& s, int stage)
{
    stage = juce::jlimit (0, 3, stage);
    const auto actual = run (s, stage);
    StageTime result { actual.stageSeconds (stage), actual.endless[(size_t) stage], false };
    const auto& values = actual.values;
    const auto from = (size_t) juce::jlimit (0, (int) values.size() - 1, actual.stageStart (stage));
    const auto to = (size_t) juce::jlimit (0, (int) values.size() - 1, actual.stageEnd[(size_t) stage]);
    if (result.endless || values.empty() || std::abs (values[to] - values[from]) > 1.0e-6)
        return result;

    // The same rate over the whole range: from the top to the bottom (or,
    // for the attack, up from where the note starts).
    auto full = s;
    if (! full.pitch)
        full.outLevel = 127 << 5;
    if (stage == 0)
    {
        full.levels[0] = 99;
        if (full.pitch)
            full.levels[3] = 0;
    }
    else
    {
        full.levels[(size_t) stage - 1] = 99;
        full.levels[(size_t) stage] = 0;
    }
    const auto sweep = run (full, stage);
    return { sweep.stageSeconds (stage), sweep.endless[(size_t) stage], true };
}

// The rate (0-99) whose stage takes closest to `seconds`, the rest as set:
// a stage takes longer as its rate falls.
inline int rateForSeconds (Settings s, int stage, double seconds)
{
    const auto timeFor = [&s, stage] (int rate)
    {
        s.rates[(size_t) stage] = rate;
        const auto result = stageTime (s, stage);
        return result.endless ? 1.0e9 : result.seconds;
    };
    auto lo = 0, hi = 99; // timeFor falls from lo to hi
    while (hi - lo > 1)
    {
        const auto mid = (lo + hi) / 2;
        (timeFor (mid) > seconds ? lo : hi) = mid;
    }
    return std::abs (timeFor (lo) - seconds) < std::abs (timeFor (hi) - seconds) ? lo : hi;
}

// The level (0-99) whose target is closest to a value in the graph's units.
inline int levelForValue (const Settings& s, double value)
{
    auto best = 0;
    for (int level = 1; level <= 99; ++level)
        if (std::abs (levelValue (s, level) - value) < std::abs (levelValue (s, best) - value))
            best = level;
    return best;
}

// An operator level as dB below full (its output level and scaling included).
inline juce::String formatLevel (const Settings& s, int level)
{
    if (s.pitch)
    {
        const auto semitones = levelValue (s, level) * 12.0;
        return (semitones > 0.04 ? "+" : semitones < -0.04 ? "-" : "") + describeFixed ((float) std::abs (semitones), 1) + " st";
    }
    const auto db = levelValue (s, level) * 6.0206;
    return db < -90.0 ? silentDecibels() : describeFixed ((float) db, 1) + " dB";
}
} // namespace OperatorEnv

// The graph: drag a point across for its stage's time (the rate) and up or
// down for its level; double-click resets them. Time runs on a square-root
// scale per stage, as on the synth's other envelope graphs, with the same
// ruler under the frame (KEY UP on it, the release's times after it as
// "+100 ms": UI review 8, I8-7, I8-28, V8-8); the dot is the last note
// played. Read-only (OSC's picture of the operator), a click opens the
// editor instead.
class OperatorEnvDisplay : public juce::Component,
                           public juce::SettableTooltipClient,
                           private IlanaAnim::FrameTimer
{
public:
    explicit OperatorEnvDisplay (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        startTimerHz (30);
    }

    // An oscillator prefix, or empty for the pitch envelope.
    void setSource (const juce::String& newPrefix, juce::Colour newColour)
    {
        if (newPrefix == prefix && newColour == colour && hasSettings)
            return;
        prefix = newPrefix;
        colour = newColour;
        hasSettings = false;
        if (readOnly)
        {
            setTooltip ("This operator's OP ENV, as it plays C3 at velocity 100. Click to edit it (FM page).");
            refresh();
            return;
        }
        setTooltip (prefix.isEmpty()
                        ? juce::String ("The pitch envelope every oscillator on the Operator Env follows. Drag a point: across "
                                        "for its time, up or down for its pitch; double-click to reset it. Shown for C3.")
                        : juce::String ("This operator's Operator Env, as it plays C3 at velocity 100 (its output level, "
                                        "keyboard and velocity scaling included). Drag a point: across for its time, up or "
                                        "down for its level; double-click to reset it. The dashed line is the key going up."));
        refresh();
    }

    const juce::String& getPrefix() const { return prefix; }
    bool isPitch() const { return prefix.isEmpty(); }

    // A picture only (no handles): a click calls onOpen.
    void setReadOnly (bool shouldBeReadOnly)
    {
        readOnly = shouldBeReadOnly;
        hoverHandle = -1;
        hasSettings = false;
        setSource (prefix, colour);
        setMouseCursor (readOnly ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    bool isReadOnly() const { return readOnly; }
    std::function<void()> onOpen;

    // The frame around the plot; the time ruler sits under it (the UI test
    // checks that no handle sits on the ruler).
    juce::Rectangle<float> getFrameBounds() const { return getLocalBounds().toFloat().withTrimmedBottom (rulerHeight + 2.0f); }
    juce::Rectangle<float> getPlotBounds() const { return layout().plot; }

    // Re-reads the patch; repaints when the shape changed.
    void refresh()
    {
        const auto now = OperatorEnv::read (processorRef, prefix);
        if (! hasSettings || now != settings)
        {
            settings = now;
            hasSettings = true;
            curve = OperatorEnv::run (settings);
            repaint();
        }
    }

    const OperatorEnv::Run& getRun() const { return curve; }

    // The stage handles' places (L1, L2, L3, then the release's end).
    std::array<juce::Point<float>, 4> getHandlePositions() const
    {
        const auto geo = layout();
        std::array<juce::Point<float>, 4> points;
        for (int stage = 0; stage < 4; ++stage)
            points[(size_t) stage] = { geo.stageX[(size_t) stage + 1], geo.y (targetValue (stage)) };
        return points;
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        // The well frames the plot only; the ruler sits under it, as on
        // every envelope graph.
        IlanaTheme::paintWell (g, getFrameBounds(), 6.0f);
        if (curve.values.empty())
            return;

        const auto geo = layout();
        paintTicks (g, geo);

        if (isPitch())
        {
            // The note itself, and how far the scale reaches: an axis in
            // the gutter left of the plot.
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawHorizontalLine (juce::roundToInt (geo.y (0.0)), geo.plot.getX(), geo.plot.getRight());
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            const auto reach = describeFixed ((float) (geo.span * 12.0), 1);
            const auto gutter = juce::Rectangle<float> (bounds.getX() + 2.0f, geo.plot.getY(), geo.plot.getX() - bounds.getX() - 6.0f,
                                                        geo.plot.getHeight());
            g.drawText ("+" + reach, gutter.withHeight (12.0f).toNearestInt(), juce::Justification::centredRight, false);
            g.drawText ("0 st", gutter.withHeight (12.0f).withCentre ({ gutter.getCentreX(), geo.y (0.0) }).toNearestInt(),
                        juce::Justification::centredRight, false);
            g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92") + reach, gutter.withTop (gutter.getBottom() - 12.0f).toNearestInt(),
                        juce::Justification::centredRight, false);
        }

        juce::Path path;
        for (int i = 0; i < (int) curve.values.size(); ++i)
        {
            const auto point = juce::Point<float> (geo.x (i), geo.y (curve.values[(size_t) i]));
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        auto filled = path;
        const auto base = isPitch() ? geo.y (0.0) : geo.plot.getBottom();
        filled.lineTo (geo.x ((int) curve.values.size() - 1), base);
        filled.lineTo (geo.plot.getX(), base);
        filled.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (filled);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.8f));

        // Stage handles (a read-only picture has none).
        const auto handles = getHandlePositions();
        for (int i = 0; i < (readOnly ? 0 : 4); ++i)
        {
            const auto active = hoverHandle == i || dragHandle == i;
            if (active)
            {
                g.setColour (colour.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (handles[(size_t) i]));
            }
            g.setColour (juce::Colours::white.withAlpha (active ? 1.0f : 0.85f));
            g.fillEllipse (juce::Rectangle<float> (active ? 10.0f : 8.0f, active ? 10.0f : 8.0f).withCentre (handles[(size_t) i]));
        }

        // The last note played.
        if (const auto playhead = playheadPoint (geo); playhead.has_value())
        {
            g.setColour (colour.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (*playhead));
            g.setColour (colour.interpolatedWith (juce::Colours::white, 0.15f));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (*playhead));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (*playhead), 1.4f);
        }

        // What it is drawn for, and a still envelope said in words.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text3);
        const auto corner = bounds.reduced (8.0f, 3.0f).withHeight (12.0f).toNearestInt();
        g.drawText (IlanaTheme::fittedHint (isPitch() ? "PITCH, at C3" : "C3, velocity 100", g.getCurrentFont(), (float) corner.getWidth()),
                    corner, juce::Justification::centredRight, false);
        const auto flat = std::all_of (curve.values.begin(), curve.values.end(),
                                       [this] (double v) { return std::abs (v - curve.values.front()) < 1.0e-9; });
        if (flat && dragHandle < 0)
        {
            // Above the line the handles sit on (the pitch's centre, an
            // operator's floor), on a backdrop the KEY UP line passes
            // under (I7-34).
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::label);
            const auto words = juce::String (readOnly ? (isPitch() ? "No pitch movement" : "No movement")
                                                      : (isPitch() ? "No pitch movement: drag a point" : "No movement: drag a point"));
            const auto line = isPitch() ? geo.y (0.0) : geo.plot.getBottom();
            const auto width = juce::jmin (geo.plot.getWidth(), (float) juce::GlyphArrangement::getStringWidthInt (font, words) + 12.0f);
            // Past the key-up line when it fits there, so the line doesn't
            // run through the words (I8-28).
            const auto afterKeyUp = juce::Range<float> (geo.keyUpX + 6.0f, geo.plot.getRight());
            const auto centreX = afterKeyUp.getLength() >= width ? afterKeyUp.getStart() + afterKeyUp.getLength() * 0.5f : geo.plot.getCentreX();
            const auto text = juce::Rectangle<float> (width, 18.0f).withCentre ({ centreX, line - 22.0f });
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
            g.fillRoundedRectangle (text, 4.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (font);
            IlanaTheme::drawFitted (g, words, text.toNearestInt(), juce::Justification::centred, 1);
        }

        if (readout.isNotEmpty())
        {
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, readout) + 12.0f;
            const auto box = juce::Rectangle<float> (geo.plot.getX() + 2.0f, geo.plot.getY() + 14.0f, width, 16.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (colour.withAlpha (0.95f));
            g.setFont (font);
            g.drawText (readout, box.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (readOnly)
            return;
        const auto handle = findHandle (event.position);
        if (handle != hoverHandle)
        {
            hoverHandle = handle;
            setMouseCursor (handle >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverHandle = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (readOnly)
        {
            if (onOpen != nullptr && ! event.mods.isPopupMenu())
                onOpen();
            return;
        }
        dragHandle = findHandle (event.position);
        if (dragHandle < 0)
            return;

        frozen = layout();
        // The point keeps its place under the press (no jump to the pointer);
        // the drag moves it by the pointer's movement.
        dragPoint = getHandlePositions()[(size_t) dragHandle];
        lastMousePosition = event.position;
        rateParameter = parameterFor (dragHandle, false);
        levelParameter = parameterFor (dragHandle, true);
        processorRef.beginEdit (juce::String ("OP ENV ") + OperatorEnv::rateName (dragHandle));
        for (auto* parameter : { rateParameter, levelParameter })
            if (parameter != nullptr)
                parameter->beginChangeGesture();
        updateReadout();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragHandle < 0 || ! frozen.has_value())
            return;

        const auto& geo = *frozen;
        // Shift (or Ctrl / Cmd) moves it 10x finer than the pointer.
        dragPoint += (event.position - lastMousePosition) * (event.mods.isShiftDown() || event.mods.isCommandDown() ? 0.1f : 1.0f);
        lastMousePosition = event.position;
        dragPoint.y = juce::jlimit (geo.plot.getY(), geo.plot.getBottom(), dragPoint.y);
        const auto set = [] (juce::RangedAudioParameter* parameter, int value)
        {
            if (parameter != nullptr && juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())) != value)
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
        };

        // Across: the stage's time, back through the square-root scale from
        // where it starts (the release from the key going up).
        const auto start = (dragHandle == 3 ? geo.keyUpX : geo.stageX[(size_t) dragHandle]) + minimumStageWidth;
        const auto units = juce::jmax (0.0f, dragPoint.x - start) / geo.scale;
        set (rateParameter, OperatorEnv::rateForSeconds (settings, dragHandle, (double) (units * units)));
        // Up and down: its level.
        set (levelParameter, OperatorEnv::levelForValue (settings, geo.value (dragPoint.y)));

        refresh();
        updateReadout();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragHandle < 0)
            return;
        for (auto* parameter : { rateParameter, levelParameter })
            if (parameter != nullptr)
                parameter->endChangeGesture();
        processorRef.endEdit();
        rateParameter = levelParameter = nullptr;
        dragHandle = -1;
        frozen.reset();
        readout.clear();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (readOnly)
            return;
        const auto handle = findHandle (event.position);
        if (handle < 0)
            return;
        processorRef.performEdit (juce::String ("Reset OP ENV ") + OperatorEnv::rateName (handle), [this, handle]
        {
            for (auto* parameter : { parameterFor (handle, false), parameterFor (handle, true) })
                if (parameter != nullptr)
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost (parameter->getDefaultValue());
                    parameter->endChangeGesture();
                }
        });
        refresh();
    }

private:
    struct Geometry
    {
        juce::Rectangle<float> plot;
        // Where each stage starts (0-3) and the release ends (4), and where
        // the key goes up (after the hold).
        std::array<float, 5> stageX {};
        float keyUpX = 0.0f, scale = 1.0f;
        double span = 1.0; // the pitch graph's reach, octaves each way
        bool pitch = false;
        std::array<int, 4> stageEnd {};

        float y (double value) const
        {
            const auto t = pitch ? 0.5 + 0.5 * value / span : (value + 11.0) / 11.0;
            return plot.getBottom() - plot.getHeight() * (float) juce::jlimit (0.0, 1.0, t);
        }

        double value (float yPosition) const
        {
            const auto t = (double) ((plot.getBottom() - yPosition) / juce::jmax (1.0f, plot.getHeight()));
            return pitch ? (juce::jlimit (0.0, 1.0, t) - 0.5) * 2.0 * span : juce::jlimit (0.0, 1.0, t) * 11.0 - 11.0;
        }

        // The x of block i of the run.
        float x (int block) const
        {
            for (int stage = 0; stage < 4; ++stage)
            {
                const auto from = stage == 0 ? 0 : stageEnd[(size_t) stage - 1];
                const auto to = stageEnd[(size_t) stage];
                if (block <= to || stage == 3)
                {
                    const auto left = stage == 3 ? keyUpX : stageX[(size_t) stage];
                    const auto right = stageX[(size_t) stage + 1];
                    return left + (right - left) * (to > from ? (float) (block - from) / (float) (to - from) : 1.0f);
                }
            }
            return stageX[4];
        }
    };

    // The ruler under the frame, as EnvelopeDisplay's.
    static constexpr float rulerHeight = 14.0f;
    static constexpr float holdUnits = 0.55f;
    // Each stage at least this wide, so short stages' handles never pile up
    // (V7-9); time runs on the square-root scale beyond it.
    static constexpr float minimumStageWidth = 14.0f;
    static float timeUnits (double seconds) { return (float) std::sqrt (juce::jmax (0.0, seconds)); }

    Geometry layout() const
    {
        Geometry geo;
        // Inside the frame, the ruler under it (V7-9, I8-7): room at the
        // top for the corner caption, at the right for the last handle, and
        // for the pitch a gutter for its axis.
        geo.pitch = isPitch();
        geo.plot = getFrameBounds().reduced (12.0f, 8.0f).withTrimmedTop (10.0f).withTrimmedRight (4.0f);
        if (geo.pitch)
            geo.plot = geo.plot.withTrimmedLeft (juce::jmin (34.0f, geo.plot.getWidth() * 0.18f));
        geo.stageEnd = curve.stageEnd;

        if (geo.pitch)
        {
            auto most = 0.25;
            for (const auto value : curve.values)
                most = std::max (most, std::abs (value) * 1.15);
            geo.span = frozen.has_value() ? frozen->span : most;
        }

        auto total = holdUnits;
        for (int stage = 0; stage < 4; ++stage)
            total += timeUnits (curve.stageSeconds (stage));
        geo.scale = frozen.has_value() ? frozen->scale
                                       : (geo.plot.getWidth() - 4.0f * minimumStageWidth) / juce::jmax (1.6f, total * 1.12f);

        auto x = geo.plot.getX();
        for (int stage = 0; stage < 4; ++stage)
        {
            geo.stageX[(size_t) stage] = x;
            if (stage == 3)
            {
                geo.keyUpX = x + geo.scale * holdUnits;
                x = geo.keyUpX;
            }
            x += minimumStageWidth + geo.scale * timeUnits (curve.stageSeconds (stage));
        }
        geo.stageX[4] = x;
        return geo;
    }

    // The level a stage moves to, in the graph's units.
    double targetValue (int stage) const
    {
        const auto end = curve.stageEnd[(size_t) stage];
        return juce::isPositiveAndBelow (end, (int) curve.values.size()) ? curve.values[(size_t) end] : 0.0;
    }

    // Faint time lines, as the other envelope graphs draw them: time since
    // the note started through stages 1-3, then since the key went up (+).
    void paintTicks (juce::Graphics& g, const Geometry& geo) const
    {
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        std::vector<juce::Range<float>> labelled;
        const auto noteX = [&] (double seconds) -> float
        {
            auto start = 0.0;
            for (int stage = 0; stage < 3; ++stage)
            {
                const auto span = curve.stageSeconds (stage);
                // A stage drawn at its minimum width has no time to mark (no
                // stray ticks on a still pitch envelope, I8-28).
                const auto drawn = geo.stageX[(size_t) stage + 1] - geo.stageX[(size_t) stage] - minimumStageWidth;
                if (span > 0.0 && seconds < start + span && drawn < 4.0f)
                    return -1.0f;
                if (span > 0.0 && seconds < start + span)
                    return geo.stageX[(size_t) stage] + (float) ((seconds - start) / span) * (geo.stageX[(size_t) stage + 1] - geo.stageX[(size_t) stage]);
                start += span;
            }
            // Past the stages, the note holds until KEY UP: its time runs on
            // at the graph's square-root scale, so a still envelope (the
            // pitch one, as often as not) has a ruler too (I9-11).
            const auto x = geo.stageX[3] + geo.scale * (timeUnits (seconds) - timeUnits (start));
            return seconds > start && x < geo.keyUpX - 2.0f ? x : -1.0f;
        };
        const auto releaseX = [&] (double seconds) -> float
        {
            const auto span = curve.stageSeconds (3);
            if (geo.stageX[4] - geo.keyUpX - minimumStageWidth < 4.0f)
                return -1.0f;
            return span > 0.0 && seconds < span ? geo.keyUpX + (float) (seconds / span) * (geo.stageX[4] - geo.keyUpX) : -1.0f;
        };

        const auto rulerY = (float) getHeight() - rulerHeight;

        // KEY UP, where the release starts: a dashed line through the plot
        // and its name on the ruler, so the release's "+" times read as
        // after it (as EnvelopeDisplay draws it).
        {
            const float dashes[] { 3.0f, 3.0f };
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.drawDashedLine ({ geo.keyUpX, geo.plot.getY(), geo.keyUpX, geo.plot.getBottom() }, dashes, 2, 1.0f);
            const auto boldFont = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (boldFont, "KEY UP") + 4.0f;
            const auto left = juce::jlimit (geo.plot.getX() - 8.0f, geo.plot.getRight() + 8.0f - width, geo.keyUpX - width * 0.5f);
            labelled.push_back ({ left, left + width });
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (boldFont);
            g.drawText ("KEY UP", juce::Rectangle<float> (left, rulerY + 2.0f, width, 12.0f), juce::Justification::centred);
            g.setFont (font);
        }

        // (A still envelope's stages take no time, so mark nothing: no
        // stray ticks, I8-28; the hold still has its times, I9-11.)
        for (const auto inRelease : { false, true })
            for (const auto seconds : { 20.0, 10.0, 5.0, 2.0, 1.0, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002 })
            {
                const auto x = inRelease ? releaseX (seconds) : noteX (seconds);
                if (x < 0.0f)
                    continue;
                const auto decade = std::abs (std::log10 (seconds) - std::round (std::log10 (seconds))) < 1.0e-3 && seconds >= 0.01;
                if (! decade)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.12f));
                    g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x, rulerY - 1.0f));
                    continue;
                }
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (juce::Rectangle<float> (1.0f, geo.plot.getHeight()).withPosition (x, geo.plot.getY()));

                const auto text = (inRelease ? "+" : "") + OperatorEnv::formatSeconds (seconds);
                const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
                const auto span = juce::Range<float> (x + 3.0f, x + 3.0f + width);
                if (span.getEnd() > geo.plot.getRight() + 10.0f
                    || std::any_of (labelled.begin(), labelled.end(), [span] (juce::Range<float> other) { return other.expanded (4.0f).intersects (span); }))
                    continue;
                labelled.push_back (span);
                g.setColour (IlanaTheme::Ui::text3);
                g.fillRect (juce::Rectangle<float> (1.0f, 4.0f).withPosition (x, rulerY - 1.0f));
                g.drawText (text, juce::Rectangle<float> (span.getStart(), rulerY + 2.0f, width, 12.0f), juce::Justification::centredLeft);
            }
    }

    std::optional<juce::Point<float>> playheadPoint (const Geometry& geo) const
    {
        const auto played = processorRef.getOpEnvMonitorSeconds();
        if (played < 0.0f || curve.values.empty())
            return std::nullopt;
        const auto released = processorRef.getOpEnvMonitorRelease();
        int block = 0;
        float x = 0.0f;
        if (released < 0.0f)
        {
            block = (int) ((double) played * OperatorEnv::displayRate / Dx7::block);
            if (block >= curve.stageEnd[2])
            {
                block = curve.stageEnd[2];
                x = geo.stageX[3] + 0.25f * (geo.keyUpX - geo.stageX[3]);
            }
            else
                x = geo.x (block);
        }
        else
        {
            block = juce::jmin (curve.stageEnd[3], curve.stageEnd[2] + (int) ((double) released * OperatorEnv::displayRate / Dx7::block));
            x = geo.x (block);
        }
        return juce::Point<float> (x, geo.y (curve.values[(size_t) juce::jlimit (0, (int) curve.values.size() - 1, block)]));
    }

    int findHandle (juce::Point<float> position) const
    {
        const auto handles = getHandlePositions();
        auto best = -1;
        auto bestDistance = 11.0f;
        // The later handle wins a tie: stacked handles (instant stages) stay
        // reachable from the last one back.
        for (int i = 3; i >= 0; --i)
        {
            const auto distance = handles[(size_t) i].getDistanceFrom (position);
            if (distance < bestDistance)
            {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    }

    juce::RangedAudioParameter* parameterFor (int stage, bool level) const
    {
        const auto index = (size_t) (stage + (level ? 4 : 0));
        const auto id = isPitch() ? juce::String (OperatorEg::voiceFields()[index].suffix)
                                  : prefix + OperatorEg::operatorFields()[index].suffix;
        return dynamic_cast<juce::RangedAudioParameter*> (processorRef.apvts.getParameter (id));
    }

    void updateReadout()
    {
        if (dragHandle < 0)
        {
            readout.clear();
            return;
        }
        const auto rate = settings.rates[(size_t) dragHandle];
        const auto level = settings.levels[(size_t) dragHandle];
        readout = juce::String (OperatorEnv::rateName (dragHandle)) + " "
                  + OperatorEnv::formatSeconds (curve.stageSeconds (dragHandle), curve.endless[(size_t) dragHandle])
                  + " (" + juce::String (rate) + ")   " + OperatorEnv::levelName (dragHandle) + " "
                  + OperatorEnv::formatLevel (settings, level) + " (" + juce::String (level) + ")";
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;
        refresh();
        // The playhead moves while a note plays (and once more when it stops).
        const auto played = processorRef.getOpEnvMonitorSeconds();
        if (played >= 0.0f || lastPlayed >= 0.0f)
            repaint();
        lastPlayed = played;
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour { IlanaTheme::accent() };
    OperatorEnv::Settings settings;
    OperatorEnv::Run curve;
    bool hasSettings = false;
    int hoverHandle = -1, dragHandle = -1;
    std::optional<Geometry> frozen;
    juce::Point<float> dragPoint, lastMousePosition;
    juce::RangedAudioParameter* rateParameter = nullptr;
    juce::RangedAudioParameter* levelParameter = nullptr;
    juce::String readout;
    float lastPlayed = -1.0f;
    bool readOnly = false;
};
