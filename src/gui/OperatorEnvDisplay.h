#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
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

// The rate (0-99) whose stage takes closest to `seconds`, the rest as set:
// a stage takes longer as its rate falls.
inline int rateForSeconds (Settings s, int stage, double seconds)
{
    const auto timeFor = [&s, stage] (int rate)
    {
        s.rates[(size_t) stage] = rate;
        const auto result = run (s, stage);
        return result.endless[(size_t) stage] ? 1.0e9 : result.stageSeconds (stage);
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
    return db < -90.0 ? juce::String ("-inf dB") : describeFixed ((float) db, 1) + " dB";
}
} // namespace OperatorEnv

// The graph: drag a point across for its stage's time (the rate) and up or
// down for its level; double-click resets them. Time runs on a square-root
// scale per stage, as on the synth's other envelope graphs, with the same
// ticks; the dot is the last note played.
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
        IlanaTheme::paintWell (g, bounds, 6.0f);
        if (curve.values.empty())
            return;

        const auto geo = layout();
        paintTicks (g, geo);

        // The key goes up: a dashed line across the hold.
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        const float dashes[] { 3.0f, 3.0f };
        g.drawDashedLine ({ geo.keyUpX, geo.plot.getY(), geo.keyUpX, geo.plot.getBottom() }, dashes, 2, 1.0f);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text3);
        // Right of the line, or left of it when the release is too short
        // (a narrow graph) to hold the words; on its own line under the
        // corner caption (V7-41).
        const auto keyUpWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "KEY UP") + 2.0f;
        const auto keyUpLeft = geo.keyUpX + 3.0f + keyUpWidth > geo.plot.getRight() ? geo.keyUpX - 3.0f - keyUpWidth : geo.keyUpX + 3.0f;
        g.drawText ("KEY UP", juce::Rectangle<float> (keyUpLeft, geo.plot.getY() + 13.0f, keyUpWidth, 12.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);

        if (isPitch())
        {
            // The note itself, and how far the scale reaches.
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawHorizontalLine (juce::roundToInt (geo.y (0.0)), geo.plot.getX(), geo.plot.getRight());
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText ("+" + describeFixed ((float) (geo.span * 12.0), 1) + " st",
                        juce::Rectangle<float> (geo.plot.getX() + 2.0f, geo.plot.getY(), 60.0f, 12.0f).toNearestInt(),
                        juce::Justification::centredLeft);
            g.drawText ("-" + describeFixed ((float) (geo.span * 12.0), 1) + " st",
                        juce::Rectangle<float> (geo.plot.getX() + 2.0f, geo.plot.getBottom() - 24.0f, 60.0f, 12.0f).toNearestInt(),
                        juce::Justification::centredLeft);
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

        // Stage handles.
        const auto handles = getHandlePositions();
        for (int i = 0; i < 4; ++i)
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
            const auto words = juce::String (isPitch() ? "No pitch movement: drag a point" : "No movement: drag a point");
            const auto line = isPitch() ? geo.y (0.0) : geo.plot.getBottom();
            const auto width = juce::jmin (geo.plot.getWidth(), (float) juce::GlyphArrangement::getStringWidthInt (font, words) + 12.0f);
            const auto text = juce::Rectangle<float> (width, 18.0f).withCentre ({ geo.plot.getCentreX(), line - 22.0f });
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
        dragHandle = findHandle (event.position);
        if (dragHandle < 0)
            return;

        frozen = layout();
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
        const auto set = [] (juce::RangedAudioParameter* parameter, int value)
        {
            if (parameter != nullptr && juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())) != value)
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
        };

        // Across: the stage's time, back through the square-root scale from
        // where it starts (the release from the key going up).
        const auto start = (dragHandle == 3 ? geo.keyUpX : geo.stageX[(size_t) dragHandle]) + minimumStageWidth;
        const auto units = juce::jmax (0.0f, event.position.x - start) / geo.scale;
        set (rateParameter, OperatorEnv::rateForSeconds (settings, dragHandle, (double) (units * units)));
        // Up and down: its level.
        set (levelParameter, OperatorEnv::levelForValue (settings, geo.value (event.position.y)));

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

    static constexpr float holdUnits = 0.55f;
    // Each stage at least this wide, so short stages' handles never pile up
    // (V7-9); time runs on the square-root scale beyond it.
    static constexpr float minimumStageWidth = 14.0f;
    static float timeUnits (double seconds) { return (float) std::sqrt (juce::jmax (0.0, seconds)); }

    Geometry layout() const
    {
        Geometry geo;
        // The time labels go under the plot (V7-9), not in it.
        geo.plot = getLocalBounds().toFloat().reduced (12.0f, 6.0f).withTrimmedTop (10.0f).withTrimmedBottom (13.0f);
        geo.pitch = isPitch();
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
                if (span > 0.0 && seconds < start + span)
                    return geo.stageX[(size_t) stage] + (float) ((seconds - start) / span) * (geo.stageX[(size_t) stage + 1] - geo.stageX[(size_t) stage]);
                start += span;
            }
            return -1.0f;
        };
        const auto releaseX = [&] (double seconds) -> float
        {
            const auto span = curve.stageSeconds (3);
            return span > 0.0 && seconds < span ? geo.keyUpX + (float) (seconds / span) * (geo.stageX[4] - geo.keyUpX) : -1.0f;
        };

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
                    g.fillRect (juce::Rectangle<float> (1.0f, 4.0f).withPosition (x, geo.plot.getBottom() - 4.0f));
                    continue;
                }
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (juce::Rectangle<float> (1.0f, geo.plot.getHeight()).withPosition (x, geo.plot.getY()));
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x, geo.plot.getBottom()));

                const auto text = (inRelease ? "+" : "") + OperatorEnv::formatSeconds (seconds);
                const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
                const auto span = juce::Range<float> (x + 3.0f, x + 3.0f + width);
                if (span.getEnd() > geo.plot.getRight() + 10.0f
                    || std::any_of (labelled.begin(), labelled.end(), [span] (juce::Range<float> other) { return other.expanded (4.0f).intersects (span); }))
                    continue;
                labelled.push_back (span);
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText (text, juce::Rectangle<float> (span.getStart() - 3.0f, geo.plot.getBottom() + 1.0f, width, 12.0f),
                            juce::Justification::centredLeft);
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
    juce::RangedAudioParameter* rateParameter = nullptr;
    juce::RangedAudioParameter* levelParameter = nullptr;
    juce::String readout;
    float lastPlayed = -1.0f;
};
