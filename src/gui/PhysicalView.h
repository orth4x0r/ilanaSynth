#pragma once

// M8.7: the PHYSICAL page's animated view of the first Physical oscillator:
// the string (its modes, shaped by the strike point, the felt or pick and
// the stiffness, decaying with DECAY and DAMP), the hammer, pick or bow
// that excites it, and the body underneath, glowing with the output. It is
// a picture of the model's settings, restarted by every note; slowed down
// so the motion can be seen.

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class PhysicalView : public juce::Component,
                     public juce::SettableTooltipClient,
                     private IlanaAnim::FrameTimer
{
public:
    explicit PhysicalView (IlanaSynthAudioProcessor& p, juce::String oscPrefix = "osc1")
        : processorRef (p), prefix (std::move (oscPrefix))
    {
        setTooltip ("The string, its exciter and the body, drawn from the oscillator's settings and restarted by every "
                    "note (slowed down so the motion shows). STIFF bends the partials, EXCITE POS moves the strike, "
                    "DECAY and DAMP set how the modes die away.");
        startTimerHz (40);
    }

    void setOscillator (const juce::String& newPrefix) { prefix = newPrefix; restart(); }
    // Drawn in the oscillator's identity colour, like the rest of its page.
    void setColour (juce::Colour newColour) { colour = newColour; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 8.0f);
        const auto accent = colour;
        auto area = bounds.reduced (18.0f, 14.0f);
        const auto bodyArea = area.removeFromBottom (area.getHeight() * 0.38f);
        area.removeFromBottom (8.0f);
        const auto stringY = area.getCentreY() + area.getHeight() * 0.12f;
        const auto left = area.getX() + 12.0f, right = area.getRight() - 12.0f;
        const auto excite = (int) read ("_excite");
        const auto strike = excitePosition();

        // Nut and bridge.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRoundedRectangle (left - 6.0f, stringY - 16.0f, 6.0f, 32.0f, 2.0f);
        g.fillRoundedRectangle (right, stringY - 12.0f, 8.0f, 24.0f, 2.0f);

        // The string: the sum of its modes.
        juce::Path string;
        constexpr int points = 160;
        const auto amplitude = area.getHeight() * 0.3f;
        for (int i = 0; i <= points; ++i)
        {
            const auto x = (double) i / points;
            auto y = 0.0;
            for (int n = 1; n <= numModes; ++n)
                y += modeLevel[(size_t) n - 1] * std::sin (juce::MathConstants<double>::pi * n * x) * std::cos (modePhase[(size_t) n - 1]);
            const auto px = left + (right - left) * (float) x;
            const auto py = stringY - (float) y * amplitude;
            if (i == 0) string.startNewSubPath (px, py); else string.lineTo (px, py);
        }
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (string, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (string, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

        // The exciter at its position.
        const auto ex = left + (right - left) * strike;
        if (excite == 4) // bow
        {
            const auto sway = (float) std::sin (clock * 3.0) * 10.0f;
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawLine (ex - 40.0f + sway, stringY - 6.0f, ex + 40.0f + sway, stringY + 6.0f, 3.0f);
        }
        else if (excite == 5 || excite == 9) // hammers
        {
            const auto lift = hammerLift();
            const auto head = juce::Rectangle<float> (18.0f, 14.0f).withCentre ({ ex, stringY - 12.0f - lift * area.getHeight() * 0.35f });
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawLine (head.getCentreX(), head.getY(), head.getCentreX() - 30.0f, area.getY(), 2.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (head, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRoundedRectangle (head.withTrimmedTop (head.getHeight() * 0.55f), 4.0f);
        }
        else if (excite == 10) // feedback: the amp, pushing sound back at the string
        {
            const auto amp = juce::Rectangle<float> (46.0f, 40.0f).withCentre ({ right - 40.0f, area.getY() + 30.0f });
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillRoundedRectangle (amp, 4.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawEllipse (amp.reduced (9.0f, 6.0f), 2.0f);
            const auto glow = juce::jlimit (0.0f, 1.0f, bodyGlow);
            for (int wave = 0; wave < 3; ++wave)
            {
                const auto r = 14.0f + (float) std::fmod (clock * 30.0 + wave * 14.0, 42.0);
                juce::Path arc;
                arc.addCentredArc (amp.getX(), amp.getCentreY(), r, r, 0.0f, -juce::MathConstants<float>::pi * 0.85f,
                                   -juce::MathConstants<float>::pi * 0.15f, true);
                g.setColour (accent.withAlpha ((0.15f + 0.6f * glow) * (1.0f - (r - 14.0f) / 42.0f)));
                g.strokePath (arc, juce::PathStrokeType (1.5f));
            }
            juce::Path pick;
            const auto tip = juce::Point<float> (ex, stringY - 4.0f - hammerLift() * 20.0f);
            pick.addTriangle (tip, tip.translated (-9.0f, -18.0f), tip.translated (9.0f, -18.0f));
            g.setColour (accent);
            g.fillPath (pick);
        }
        else if (excite == 7 || excite == 8) // tine or reed
        {
            g.setColour (accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 22.0f).withCentre ({ ex, stringY - 22.0f + hammerLift() * 12.0f }), 3.0f);
        }
        else // pluck
        {
            juce::Path pick;
            const auto tip = juce::Point<float> (ex, stringY - 4.0f - hammerLift() * 20.0f);
            pick.addTriangle (tip, tip.translated (-9.0f, -18.0f), tip.translated (9.0f, -18.0f));
            g.setColour (accent);
            g.fillPath (pick);
        }

        // The body: a plate with its nodal lines, glowing with the output.
        const auto glow = juce::jlimit (0.0f, 1.0f, bodyGlow);
        IlanaTheme::paintGlow (g, bodyArea, 10.0f, accent, glow * 1.5f);
        g.setColour (IlanaTheme::Ui::well.interpolatedWith (accent, 0.06f + 0.2f * glow));
        g.fillRoundedRectangle (bodyArea, 10.0f);
        g.setColour (IlanaTheme::Ui::line.interpolatedWith (accent, 0.6f * glow));
        g.drawRoundedRectangle (bodyArea, 10.0f, 1.2f);
        for (int line = 1; line < 5; ++line)
        {
            const auto t = (float) line / 5.0f;
            const auto wobble = (float) std::sin (clock * 5.0 + line) * 3.0f * glow;
            g.drawLine (bodyArea.getX() + bodyArea.getWidth() * t + wobble, bodyArea.getY() + 6.0f,
                        bodyArea.getX() + bodyArea.getWidth() * t - wobble, bodyArea.getBottom() - 6.0f, 0.8f);
        }
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.setColour (IlanaTheme::Ui::text2);
        g.drawText (bodyName(), bodyArea.reduced (12.0f, 6.0f), juce::Justification::bottomLeft);
        g.drawText (exciteName (excite), area.withHeight (16.0f), juce::Justification::topRight);

        // Not a Physical oscillator: the picture is only what it would be.
        if (juce::roundToInt (read ("_mode")) != 1)
        {
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.86f));
            g.fillRoundedRectangle (bounds, 8.0f);
            // The card beside it says why and offers the switch; here only
            // a quiet label, so the page doesn't say it twice.
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText ("PREVIEW  -  Physical oscillators only", bounds.reduced (14.0f, 10.0f), juce::Justification::topLeft);
        }
    }

private:
    static constexpr int numModes = 16;

    float read (const char* suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + suffix))
            return value->load();
        return 0.0f;
    }

    float readGlobal (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    float excitePosition() const
    {
        const auto position = read ("_string_excite_pos");
        return position > 0.0f ? juce::jlimit (0.02f, 0.5f, position) : 0.125f;
    }

    float hammerLift() const
    {
        // Down onto the string at the note, thrown back up after it.
        const auto t = (float) sinceNote;
        return t < 0.05f ? 1.0f - t / 0.05f : juce::jmin (1.0f, (t - 0.05f) / 0.25f);
    }

    juce::String bodyName() const
    {
        const juce::StringArray bodies { "CLASSIC BODY", "BAR", "PLATE", "BELL", "SHELL" };
        const auto type = (int) readGlobal ("body_type");
        if (readGlobal ("sb_on") > 0.5f)
            return readGlobal ("sb_model") > 0.5f ? "SOUNDBOARD (DENSE)" : "SOUNDBOARD";
        return readGlobal ("res_on") > 0.5f ? bodies[juce::jlimit (0, bodies.size() - 1, type)] : "NO BODY";
    }

    static juce::String exciteName (int excite)
    {
        const juce::StringArray names { "BURST", "NOISE", "SAW", "PULSE", "BOW", "HAMMER (CLASSIC)", "OSC IN", "TINE", "REED", "PIANO HAMMER", "FEEDBACK" };
        return names[juce::jlimit (0, names.size() - 1, excite)];
    }

    void restart()
    {
        // Mode levels from the strike point and the exciter's hardness.
        const auto strike = (double) excitePosition();
        const auto hardness = (double) read ("_hammer_hard");
        const auto rolloff = 1.2 + 1.2 * (1.0 - hardness);
        for (int n = 1; n <= numModes; ++n)
        {
            modeLevel[(size_t) n - 1] = std::sin (juce::MathConstants<double>::pi * n * strike) / std::pow ((double) n, rolloff) * 1.4;
            modePhase[(size_t) n - 1] = 0.0;
        }
        sinceNote = 0.0;
    }

    void timerCallback() override
    {
        const auto dt = (double) frameSeconds();
        clock += dt;
        sinceNote += dt;

        const auto notes = processorRef.getNoteOnCount();
        if (notes != lastNotes)
        {
            lastNotes = notes;
            restart();
        }

        // Slowed down: the fundamental turns about once a second; stiffness
        // speeds the upper modes up (n sqrt (1 + B n^2)); DECAY and DAMP
        // take them away (the upper ones faster).
        const auto stiffness = (double) read ("_string_stiffness");
        const auto b = 1.0e-4 * std::pow (10.0, 2.5 * stiffness);
        const auto decay = (double) read ("_string_decay");
        const auto damp = (double) read ("_string_damp");
        for (int n = 1; n <= numModes; ++n)
        {
            modePhase[(size_t) n - 1] += juce::MathConstants<double>::twoPi * 0.9 * n * std::sqrt (1.0 + b * n * n) * dt;
            const auto seconds = (0.4 + 6.0 * decay) / (1.0 + damp * 0.4 * (n - 1));
            modeLevel[(size_t) n - 1] *= std::exp (-dt / seconds);
        }
        // A held note that keeps sounding (bow, feedback, SUSTAIN) keeps the
        // string moving with the output.
        const auto level = juce::jmin (1.0f, processorRef.getOutputPeak() * 2.0f);
        if (sinceNote > 0.3 && level > 0.05f)
            for (int n = 1; n <= numModes; ++n)
                modeLevel[(size_t) n - 1] = juce::jmax (modeLevel[(size_t) n - 1],
                                                        0.5 * level * std::abs (std::sin (juce::MathConstants<double>::pi * n * excitePosition())) / (n * n));
        bodyGlow = IlanaAnim::approach (bodyGlow, juce::jmin (1.0f, processorRef.getOutputPeak() * 2.0f), 0.15f, frameTicks());

        // Only a Physical oscillator's string moves; the preview of another
        // mode stays at rest.
        if (juce::roundToInt (read ("_mode")) != 1)
            modeLevel.fill (0.0);

        // The string rests once its motion is under a tenth of a pixel.
        auto total = 0.0;
        for (const auto level : modeLevel)
            total += std::abs (level);
        const auto moving = bodyGlow > 0.005f || total * (double) getHeight() * 0.3 > 0.1;
        if (! moving && total > 0.0)
            modeLevel.fill (0.0);

        if (isShowing() && (moving || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour { IlanaTheme::oscColour (0) };
    std::array<double, numModes> modeLevel {}, modePhase {};
    double clock = 0.0, sinceNote = 10.0;
    unsigned lastNotes = 0;
    float bodyGlow = 0.0f;
};
