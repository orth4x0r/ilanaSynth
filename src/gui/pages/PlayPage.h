// Included only from PluginEditor.cpp, after OscPage.h and ModSourcePanel.h; the
// contents stay in the same anonymous namespace.
#pragma once

namespace
{
// A small selector of two or three solid-fill pills (the slope, SUB's octave).
class PillChoice : public ParamBoundComponent
{
public:
    PillChoice (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray labelsIn, juce::Colour colourIn, const juce::String& tip, std::vector<int> valuesIn = {})
        : ParamBoundComponent (state, id), labels (std::move (labelsIn)), colour (colourIn), values (std::move (valuesIn))
    {
        if (values.empty())
            for (int i = 0; i < labels.size(); ++i)
                values.push_back (i);
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth() / (float) labels.size();
        const auto mouse = getMouseXYRelative().toFloat();
        for (int option = 0; option < labels.size(); ++option)
        {
            const auto cellBounds = juce::Rectangle<float> (bounds.getX() + width * (float) option, bounds.getY(), width, bounds.getHeight()).reduced (1.5f, 1.0f);
            const auto on = values[(size_t) option] == current;
            const auto hovered = isMouseOver() && cellBounds.contains (mouse);
            g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, hovered ? 0.12f : 0.0f));
            g.fillRoundedRectangle (cellBounds, 4.0f);
            if (! on)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (cellBounds.reduced (0.5f), 4.0f, 1.0f);
            }
            g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (8.5f, true));
            g.drawText (labels[option], cellBounds, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        setValue (values[(size_t) juce::jlimit (0, labels.size() - 1, (int) (event.position.x / (float) getWidth() * (float) labels.size()))]);
    }

private:
    juce::StringArray labels;
    juce::Colour colour;
    std::vector<int> values;
};

// An on/off as one pill (the LFO's SYNC, RETRIG and KEY): solid while on.
class PillToggle : public ParamBoundComponent
{
public:
    PillToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text, juce::Colour colourIn, const juce::String& tip)
        : ParamBoundComponent (state, id), label (text), colour (colourIn)
    {
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto on = current > 0;
        g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, isMouseOver() ? 0.12f : 0.0f));
        const auto radius = bounds.getHeight() * 0.5f;
        g.fillRoundedRectangle (bounds, radius);
        if (! on)
        {
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        }
        g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (8.5f, true));
        g.drawText (label, bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { setValue (current > 0 ? 0 : 1); }

    // The text's width, so a row of pills can be sized to what they say.
    float textWidth() const { return juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (8.5f, true)), label) + 2.0f; }

private:
    juce::String label;
    juce::Colour colour;
};

// A small round glyph button (the carousel's arrows and +).
class GlyphButton : public juce::Button
{
public:
    explicit GlyphButton (const juce::String& glyphIn) : juce::Button (glyphIn), glyph (glyphIn)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (IlanaTheme::Ui::raised.interpolatedWith (IlanaTheme::Ui::text, down ? 0.2f : hovered ? 0.1f : 0.0f));
        g.fillEllipse (bounds);
        g.setColour (IlanaTheme::Ui::line);
        g.drawEllipse (bounds.reduced (0.5f), 1.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (10.0f, true));
        g.drawText (glyph, bounds.translated (0.0f, -0.5f), juce::Justification::centred);
    }

private:
    juce::String glyph;
};

// A carousel card for one kind of source (PLAY's ENVELOPE and LFO): the sources
// of that kind as pills (each one still drags onto a knob), the selected one's
// graph and main controls under them. The pills' "+" adds the next one to the
// patch, the arrows step through them.
class SourceCarouselCard : public juce::Component,
                           private juce::Timer
{
public:
    enum class Kind { envelope, lfo };

    SourceCarouselCard (IlanaSynthAudioProcessor& p, Kind kindIn) : processorRef (p), kind (kindIn)
    {
        for (auto* arrow : { &previous, &next })
            addAndMakeVisible (*arrow);
        previous.setTooltip ("The previous one");
        next.setTooltip ("The next one");
        previous.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        add.setTooltip (kind == Kind::envelope ? "Add an envelope to the patch" : "Add an LFO to the patch");
        add.onClick = [this] { showAddMenu(); };
        addAndMakeVisible (add);
        refreshSources();
        startTimerHz (6);
    }

    // The selected source's full editor on MOD (a click on the title).
    std::function<void (int source)> onOpenInMod;

    static constexpr int headerHeight = 20;

    int getSelectedSource() const { return selected; }

    void select (int source)
    {
        selected = source;
        for (auto& tab : tabs)
            tab->setSelected (tab->getSource() == selected);
        buildBody();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto colour = sourceTabColour (selected);
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 10.0f, colour);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (1, headerHeight - 1, getWidth() - 2, 1));
        const auto centre = headerHeight / 2;
        IlanaTheme::paintTag (g, { 14.0f, (float) centre }, colour);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (13.0f, true));
        IlanaTheme::drawFitted (g, kind == Kind::envelope ? "ENVELOPE" : "LFO", juce::Rectangle<int> (23, centre - 9, 60, 18), juce::Justification::centredLeft, 1);

        if (! chipBounds.isEmpty())
        {
            const auto chip = chipBounds.toFloat().reduced (0.5f);
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (chip, chip.getHeight() * 0.5f, 1.0f);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ chip.getX() + 8.0f, chip.getCentreY() }));
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (8.5f, true));
            IlanaTheme::drawFitted (g, chipText, chipBounds.withTrimmedLeft (14).withTrimmedRight (4), juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        const auto centre = headerHeight / 2;
        auto x = kind == Kind::envelope ? 78 : 44;
        previous.setBounds ({ x, centre - 7, 14, 14 });
        x += 17;
        const auto tabWidth = kind == Kind::envelope ? 42 : 20;
        hiddenCount = 0;
        for (auto& tab : tabs)
        {
            const auto fits = x + tabWidth + 17 + 17 + 8 <= getWidth() - 40;
            tab->setVisible (fits);
            if (fits)
            {
                tab->setBounds ({ x, centre - 8, tabWidth, 16 });
                x += tabWidth + 3;
            }
            else
            {
                ++hiddenCount;
            }
        }
        add.setBounds ({ x, centre - 7, 14, 14 });
        next.setBounds ({ x + 17, centre - 7, 14, 14 });
        layoutBody();
    }

    // The sources the card lists: the envelopes (or LFOs) in the patch.
    std::vector<int> wantedSources() const
    {
        std::vector<int> wanted;
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (envelopeShown (processorRef, env))
                    wanted.push_back ((int) envelopeSource (env));
        }
        else
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (processorRef.isLfoShown (lfo))
                    wanted.push_back ((int) Mod::lfoSourceFor (lfo));
        }
        return wanted;
    }

private:
    // The pill's name: "AMP ENV" is AMP, "LFO 2" is 2.
    juce::String compactName (int source) const
    {
        auto name = sourceTabName (source, processorRef);
        if (kind == Kind::lfo)
            return name.fromLastOccurrenceOf (" ", false, false);
        return name.endsWith (" ENV") ? name.dropLastCharacters (4) : name;
    }

    void step (int direction)
    {
        if (sources.empty())
            return;
        auto index = (int) (std::find (sources.begin(), sources.end(), selected) - sources.begin());
        index = (index + direction + (int) sources.size()) % (int) sources.size();
        select (sources[(size_t) index]);
    }

    bool anyToAdd() const
    {
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (! envelopeShown (processorRef, env))
                    return true;
            return false;
        }
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processorRef.isLfoShown (lfo))
                return true;
        return false;
    }

    // "+" adds the next free one, selected.
    void showAddMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (kind == Kind::envelope ? "Add an envelope" : "Add an LFO");
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (! envelopeShown (processorRef, env))
                    menu.addItem (100 + env, ModNames::source ((int) envelopeSource (env)));
        }
        else
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (! processorRef.isLfoShown (lfo))
                    menu.addItem (1 + lfo, "LFO " + juce::String (lfo + 1));
        }
        juce::Component::SafePointer<SourceCarouselCard> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [safeThis] (int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;
            auto& p = safeThis->processorRef;
            const auto isLfo = safeThis->kind == Kind::lfo;
            const auto index = isLfo ? result - 1 : result - 100;
            p.setRevealed (isLfo ? IlanaSynthAudioProcessor::Module::Lfo : IlanaSynthAudioProcessor::Module::Envelope, index, true);
            safeThis->refreshSources();
            safeThis->select (isLfo ? (int) Mod::lfoSourceFor (index) : (int) envelopeSource (index));
        });
    }

    static juce::String destinationShortName (int destination)
    {
        const auto parts = ModNames::destinationParts (destination);
        return parts.control.isEmpty() ? parts.module : parts.control;
    }

    void refreshSources()
    {
        auto wanted = wantedSources();
        if (wanted != sources)
        {
            sources = std::move (wanted);
            tabs.clear();
            for (const auto source : sources)
            {
                auto tab = std::make_unique<SourceTab> (processorRef, source);
                tab->setCompactLabel (compactName (source));
                tab->onSelect = [this] (int picked) { select (picked); };
                addAndMakeVisible (*tab);
                tabs.push_back (std::move (tab));
            }
            if (std::find (sources.begin(), sources.end(), selected) == sources.end())
                selected = sources.empty() ? 0 : sources.front();
            select (selected);
        }

        for (auto& tab : tabs)
            tab->setRouteCount (modRouteCount (processorRef, tab->getSource()));
        add.setVisible (anyToAdd());

        if (kind == Kind::lfo && ! sources.empty())
        {
            const auto slots = modSlotsDrivenBy (processorRef, selected);
            auto text = slots.empty() ? juce::String ("Drives nothing")
                                      : destinationShortName (processorRef.readModSlot (slots.front()).destination)
                                            + (slots.size() > 1 ? " +" + juce::String ((int) slots.size() - 1) : juce::String());
            if (text != chipText)
            {
                chipText = text;
                repaint();
            }
        }
    }

    // The selected source's graph and controls.
    void buildBody()
    {
        graph.reset();
        knobs.clear();
        toggles.clear();
        shape.reset();
        rate.reset();
        if (sources.empty())
            return;

        const auto colour = sourceTabColour (selected);
        auto& state = processorRef.apvts;
        const auto dial = [&] (const juce::String& id, const juce::String& label)
        {
            auto knob = std::make_unique<KnobControl> (state, id, label, colour, false);
            knob->setPackedDial (dialSize);
            knob->setPackedValueRoom (12);
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        };

        if (kind == Kind::envelope)
        {
            const auto env = SourceEditor::envelopeOf (selected);
            const auto prefix = SourceEditor::envelopePrefix (juce::jmax (0, env));
            auto display = std::make_unique<EnvelopeDisplay> (processorRef, prefix, colour, false);
            display->setSlim (true);
            display->setHandlesOnHover (true);
            graph = std::move (display);
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_delay", "DELAY" }, { "_attack", "ATTACK" }, { "_hold", "HOLD" },
                                                 { "_decay", "DECAY" }, { "_sustain", "SUSTAIN" }, { "_release", "RELEASE" }, { "_curve", "CURVE" } })
                dial (prefix + suffix, label);
        }
        else
        {
            const auto lfo = juce::jmax (0, Mod::lfoIndexFor ((Mod::Source) selected));
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto display = std::make_unique<LfoDisplay> (processorRef, lfo, colour);
            display->setMinimal (true);
            graph = std::move (display);
            rate = std::make_unique<LfoRateControl> (processorRef, lfo, colour, false);
            rate->addTo (*this);
            rate->getRateKnob().setPackedDial (dialSize);
            rate->getRateKnob().setPackedValueRoom (12);
            rate->getDivisionKnob().setPackedDial (dialSize);
            rate->setShown (true);
            dial (prefix + "_smooth", "SMOOTH");
            dial (prefix + "_phase", "START");
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_sync", "SYNC" }, { "_retrig", "RETRIG" }, { "_key", "KEY" } })
            {
                auto toggle = std::make_unique<PillToggle> (state, prefix + suffix, label, colour, juce::String (label) + "\nThis LFO's " + juce::String (label).toLowerCase() + " setting.");
                addAndMakeVisible (*toggle);
                toggles.push_back (std::move (toggle));
            }
            shape = std::make_unique<ComboControl> (state, prefix + "_shape", "SHAPE");
            LfoShapeMenu::apply (*shape, processorRef);
            shape->setPackedMenu (true);
            addAndMakeVisible (*shape);
        }
        addAndMakeVisible (*graph);
    }

    void layoutBody()
    {
        if (graph == nullptr)
            return;

        // (Equal above and below the graph, and at both sides: the envelope's last dial and the LFO's buttons end as far from the card as the graph starts.)
        auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (7, 3);
        body.setRight (body.getRight() + (kind == Kind::envelope ? 4 : 0));
        // (The LFO's three dials sit in 34 px cells so its pills keep their padding; the graph keeps its size.)
        const auto cell = kind == Kind::envelope ? 36 : 34;
        // The graphs take every pixel the knobs and the controls leave, the card's full height (bigger than mockup fit8's, ilana 2026-10-19).
        const auto lfoColumn = 91;
        const auto graphWidth = kind == Kind::envelope ? juce::jmax (60, body.getWidth() - 7 * cell - 3)
                                                        : juce::jmax (50, body.getWidth() - 3 * cell - lfoColumn - 9);
        graph->setBounds (body.removeFromLeft (graphWidth));
        // (The LFO's graph keeps its size; the buttons' column takes its width from the two gaps beside the knobs, which stay equal.)
        const auto gap = kind == Kind::envelope ? 3 : 1;
        body.removeFromLeft (gap);
        const auto place = [&] (KnobControl& knob, int x)
        {
            knob.setBounds (KnobControl::packedBoundsFor ({ x, body.getY() + (body.getHeight() - (int) dialSize - 10) / 2, cell, (int) dialSize + 10 }));
        };

        if (kind == Kind::envelope)
        {
            auto x = body.getX() + (body.getWidth() - (int) knobs.size() * cell) / 2;
            for (auto& knob : knobs)
            {
                place (*knob, x);
                x += cell;
            }
            return;
        }

        // RATE, SMOOTH, START as knobs; SYNC / RETRIG / KEY over SHAPE in a column.
        auto x = body.getX();
        place (rate->getRateKnob(), x);
        rate->matchBounds();
        x += cell;
        for (auto& knob : knobs)
        {
            place (*knob, x);
            x += cell;
        }
        // The column runs the graph's full height: three rows, the same height, the leftover pixels shared by the two gaps,
        // so the top row starts where the graph starts and the chip ends where it ends.
        auto column = body.withTrimmedLeft (x - body.getX() + gap);
        // (Gaps near 9: the first one that lets the three rows come out whole, so rows and gaps are each equal.)
        auto gapA = 9;
        for (const auto candidate : { 9, 8, 10, 7, 11, 6, 12 })
            if ((column.getHeight() - 2 * candidate) % 3 == 0)
            {
                gapA = candidate;
                break;
            }
        const auto gapB = gapA;
        const auto rowH = juce::jmax (11, (column.getHeight() - 2 * gapA) / 3);
        auto switches = column.removeFromTop (rowH);
        column.removeFromTop (gapA);
        // Three pills at one gap (each draws 1 px inside its bounds, so the row reaches 1 px past the column on both sides).
        const auto count = juce::jmax (1, (int) toggles.size());
        const auto across = switches.getWidth() + 2;
        // One gap between the pills (2 design px, each pill also draws 1 px inside its bounds); the padding each side of every word is the same on all
        // three and takes the rest, so the row ends flush with the column on both edges (no under 4 design px of padding at the 8.5 text).
        auto textTotal = 0.0f;
        for (auto& toggle : toggles)
            textTotal += toggle->textWidth();
        const auto pillGap = 2.0f;
        const auto pad = ((float) across - pillGap * (float) (count - 1) - textTotal) / (float) (2 * count);
        auto pills = switches.expanded (1, 0).withTrimmedBottom (-1);
        auto edge = (float) pills.getX();
        for (size_t i = 0; i < toggles.size(); ++i)
        {
            const auto right = edge + toggles[i]->textWidth() + 2.0f * pad;
            toggles[i]->setBounds (pills.withLeft (juce::roundToInt (edge)).withRight (juce::roundToInt (right)));
            edge = right + pillGap;
        }
        shape->setBounds (column.removeFromTop (rowH));
        column.removeFromTop (gapB);
        chipBounds = column.removeFromTop (rowH);
    }

    void timerCallback() override
    {
        // (Visible up the tree: an offscreen card, as in the UI tests, follows the pool too.)
        for (auto* c = static_cast<juce::Component*> (this); c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return;
        refreshSources();
    }

    static constexpr float dialSize = 28.0f;

    IlanaSynthAudioProcessor& processorRef;
    Kind kind;
    std::vector<int> sources;
    std::vector<std::unique_ptr<SourceTab>> tabs;
    GlyphButton previous { juce::String::fromUTF8 ("\xe2\x80\xb9") }, next { juce::String::fromUTF8 ("\xe2\x80\xba") }, add { "+" };
    int selected = 0, hiddenCount = 0;
    juce::Rectangle<int> chipBounds;
    juce::String chipText;
    std::unique_ptr<juce::Component> graph;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::vector<std::unique_ptr<PillToggle>> toggles;
    std::unique_ptr<ComboControl> shape;
    std::unique_ptr<LfoRateControl> rate;
};

// WEST COAST's live picture: the folder's transfer curve (what one input level comes out as),
// computed as WestCoast.h's Wavefolder does it, so FOLD, BIAS and STAGES redraw it.
class WestDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit WestDisplay (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setInterceptsMouseClicks (false, false);
        setTooltip ("The wavefolder's curve: input level across, output level up. FOLD bends it, BIAS tilts it, STAGES fold it again.");
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);
        const auto plot = bounds.reduced (6.0f, 6.0f);
        const auto colour = FilterColours::west();
        const auto on = read ("west_on") > 0.5f ? 1.0f : 0.4f;

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre (plot.getCentre()));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withCentre (plot.getCentre()));

        const auto fold = read ("west_fold"), symmetry = read ("west_sym");
        const auto stages = juce::jlimit (1, 4, juce::roundToInt (read ("west_stages")));
        const auto gain = 0.35 + 11.65 * (double) (fold * fold);
        const auto bias = 0.5 * (double) symmetry;
        const auto stretch = 1.0 + 0.35 * gain / (double) stages;

        juce::Path path;
        const auto steps = juce::jmax (8, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto input = -1.0 + 2.0 * (double) i / (double) steps;
            auto v = input * gain + bias;
            for (int stage = 0; stage < stages; ++stage)
            {
                const auto y = std::sin (juce::MathConstants<double>::halfPi * v);
                v = stage + 1 < stages ? y * stretch : y;
            }
            const auto point = juce::Point<float> (plot.getX() + (float) (i) / (float) steps * plot.getWidth(),
                                                   plot.getCentreY() - (float) juce::jlimit (-1.0, 1.0, v) * plot.getHeight() * 0.5f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (on);
        IlanaTrace::paintTrace (g, path, colour);
    }

private:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;
        const auto signature = read ("west_fold") * 7.0f + read ("west_sym") * 13.0f + read ("west_stages") * 31.0f + read ("west_on");
        if (juce::approximatelyEqual (signature, lastSignature))
            return;
        lastSignature = signature;
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    float lastSignature = -1.0e9f;
};

// PLAY (ilana, 2026-10-19, mockup "fit8"): the oscillators as the OSC page's own
// cards on top, the voice's modules in two bands under them: FILTER 1, FILTER 2
// and WEST COAST; then ENVELOPE, LFO, BODY and SUB + NOISE. One knob size and
// pitch in every card, the menus in the headers.
// A small live picture of a few parameters (BODY's resonance, SUB's wave, NOISE's colour): it
// repaints when one of them moves, in the same well and trace as the other pictures.
class ParamPicture : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    ParamPicture (IlanaSynthAudioProcessor& p, std::vector<juce::String> idsIn, const juce::String& tip)
        : processorRef (p), ids (std::move (idsIn))
    {
        setInterceptsMouseClicks (false, false);
        setTooltip (tip);
        startTimerHz (10);
    }

protected:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;

private:
    void timerCallback() override
    {
        auto signature = 0.0;
        auto weight = 1.0;
        for (const auto& id : ids)
        {
            signature += (double) read (id.toRawUTF8()) * weight;
            weight *= 1.7;
        }
        if (signature != lastSignature)
        {
            lastSignature = signature;
            repaint();
        }
    }

    std::vector<juce::String> ids;
    double lastSignature = -1.0e9;
};

// BODY: where its modes sit and how long they ring, from TYPE, MATERIAL and SIZE (the engine's own tuning tables).
class BodyDisplay : public ParamPicture
{
public:
    explicit BodyDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "body_type", "body_material", "body_size", "res_on" },
                        "The body's resonance: its modes across the pitch range. TYPE picks their spacing, MATERIAL how long they ring, SIZE slides them.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto type = juce::jlimit (0, 4, juce::roundToInt (read ("body_type")));
        const auto material = juce::jlimit (0.0f, 1.0f, read ("body_material"));
        const auto size = juce::jlimit (0.0f, 1.0f, read ("body_size"));
        const auto on = read ("res_on") > 0.5f ? 1.0f : 0.45f;
        const auto metal = juce::jlimit (0.0f, 1.0f, (material - 0.15f) / 0.55f);
        const auto glass = juce::jlimit (0.0f, 1.0f, (material - 0.8f) / 0.2f);
        const auto lifeScale = 0.9f + 0.6f * metal - 0.75f * glass;

        // The first mode sits at 160 Hz at the middle SIZE, an octave per half turn; the axis is 40 Hz to 8 kHz.
        const auto root = 160.0 * std::pow (2.0, (0.5 - (double) size) * 2.0);
        constexpr auto lowHz = 40.0, highHz = 8000.0;
        std::array<double, BodyTuning::modes> hz {}, height {}, width {};
        for (int mode = 0; mode < BodyTuning::modes; ++mode)
        {
            const auto m = (size_t) mode;
            const auto ratio = type == 0 ? (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].ratio[m];
            const auto gain = type == 0 ? 1.0 / (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].gain[m];
            const auto life = type == 0 ? 1.0 / (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].lifetime[m];
            hz[m] = root * ratio;
            height[m] = gain * (1.0 - 0.5 * (1.0 - (double) material) * mode / 7.0);
            width[m] = hz[m] * (0.012 + 0.07 / (1.0 + 5.0 * life * (double) lifeScale));
        }

        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto f = lowHz * std::pow (highHz / lowHz, (double) i / (double) steps);
            auto level = 0.0;
            for (size_t m = 0; m < hz.size(); ++m)
            {
                const auto d = (f - hz[m]) / width[m];
                level += height[m] / (1.0 + d * d);
            }
            const auto point = juce::Point<float> (plot.getX() + (float) i / (float) steps * plot.getWidth(),
                                                   plot.getBottom() - (float) juce::jlimit (0.0, 1.0, std::sqrt (level)) * plot.getHeight());
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (on);
        IlanaTrace::paintArea (g, path, juce::Colour (0xffff6a2a), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// SUB: one wave shape, two cycles at -1 octave and one at -2.
class SubWaveDisplay : public ParamPicture
{
public:
    explicit SubWaveDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "sub_shape", "sub_octave", "subosc_on" }, "The sub oscillator's wave: sine, square or saw, an octave or two below the note.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto shape = juce::roundToInt (read ("sub_shape"));
        const auto cycles = juce::roundToInt (read ("sub_octave")) == 1 ? 1.0 : 2.0;
        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto phase = std::fmod ((double) i / (double) steps * cycles, 1.0);
            const auto v = shape == 1 ? (phase < 0.5 ? 1.0 : -1.0)
                           : shape == 2 ? 1.0 - 2.0 * phase
                                        : std::sin (juce::MathConstants<double>::twoPi * phase);
            const auto point = juce::Point<float> (plot.getX() + (float) i / (float) steps * plot.getWidth(), plot.getCentreY() - (float) v * plot.getHeight() * 0.5f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (read ("subosc_on") > 0.5f ? 1.0f : 0.45f);
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// NOISE: its spectrum's slope, low pitches on the left: white is flat, pink falls 3 dB an octave, brown 6,
// blue and violet rise, grey dips in the middle like the ear's own curve.
class NoiseColourDisplay : public ParamPicture
{
public:
    explicit NoiseColourDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "noise_type", "noise_on" }, "The noise colour's spectrum: low pitches left, high right.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto type = juce::jlimit (0, 5, juce::roundToInt (read ("noise_type")));
        const std::array<double, 6> slope { 0.0, -3.0, -6.0, 3.0, 6.0, 0.0 };
        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto x = (double) i / (double) steps; // 0..1 across ten octaves
            auto db = slope[(size_t) type] * (x - 0.5) * 10.0;
            if (type == 5)
                db = 9.0 * (std::cos (juce::MathConstants<double>::twoPi * x * 0.9 + 0.2) * 0.5 - 0.1);
            const auto y = plot.getCentreY() - (float) juce::jlimit (-1.0, 1.0, db / 15.0) * plot.getHeight() * 0.5f;
            const auto point = juce::Point<float> (plot.getX() + (float) x * plot.getWidth(), y);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (read ("noise_on") > 0.5f ? 1.0f : 0.45f);
        IlanaTrace::paintArea (g, path, juce::Colour (0xffff6a2a), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// The OSC area's thin scroll bar: a track and a thumb that read on the dark page.
class OscScrollBar : public juce::ScrollBar
{
public:
    using juce::ScrollBar::ScrollBar;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, 2.5f);

        const auto total = getMaximumRangeLimit() - getMinimumRangeLimit();
        if (total <= 0.0)
            return;
        const auto top = (float) ((getCurrentRangeStart() - getMinimumRangeLimit()) / total) * bounds.getHeight();
        const auto height = juce::jmax (16.0f, (float) (getCurrentRangeSize() / total) * bounds.getHeight());
        g.setColour (IlanaTheme::Ui::text2);
        g.fillRoundedRectangle (bounds.getX(), bounds.getY() + top, bounds.getWidth(), juce::jmin (height, bounds.getHeight() - top), 2.5f);
    }
};

class PlayPage : public juce::Component,
                 private juce::ScrollBar::Listener,
                 private juce::Timer
{
public:
    explicit PlayPage (IlanaSynthAudioProcessor& p)
        : processorRef (p), oscView (p), envelopeCard (p, SourceCarouselCard::Kind::envelope), lfoCard (p, SourceCarouselCard::Kind::lfo)
    {
        if (auto* page = oscView.getPage())
            page->setEmbedded (true);
        oscView.setMaximumScale (1.0f);
        oscView.setOverlayScroll (true);
        oscView.onViewMoved = [this] { syncOscBar(); };
        addAndMakeVisible (oscView);
        oscBar.setAutoHide (false);
        oscBar.addListener (this);
        addChildComponent (oscBar);

        auto& state = p.apvts;
        const auto knob = [&] (const juce::String& id, const juce::String& label, juce::Colour colour)
        {
            auto control = std::make_unique<KnobControl> (state, id, label, colour, false);
            control->setPackedDial (dialSize);
            control->setPackedValueRoom (12);
            addAndMakeVisible (*control);
            return control;
        };

        // The filters: type and slope in the header, the response and six knobs.
        for (int f = 0; f < 2; ++f)
        {
            const juce::String prefix (f == 0 ? "f1" : "f2");
            const auto colour = filterColour (f);
            auto& card = filters[(size_t) f];
            card.display = std::make_unique<FilterDisplay> (p);
            card.display->setOnlyFilter (f);
            addAndMakeVisible (*card.display);
            card.type = std::make_unique<ComboControl> (state, prefix + "_type", "");
            {
                // (Same item ids, so the attachment still matches them.)
                auto& box = card.type->getComboBox();
                const auto shortNames = FilterTypeGrid::shortNames();
                const auto names = FilterType::getNames();
                box.clear (juce::dontSendNotification);
                for (int index = 0; index < names.size(); ++index)
                    box.addItem (index < shortNames.size() ? shortNames[index] : names[index], index + 1);
                box.setSelectedId (readInt (prefix + "_type") + 1, juce::dontSendNotification);
            }
            const juce::String typeTip ("FILTER TYPE\nThe filter model; the FILTER tab shows them all in a grid.");
            card.type->setTooltip (typeTip);
            card.type->getComboBox().setTooltip (typeTip);
            card.type->setPopupOverride ([this, combo = &card.type->getComboBox(), id = prefix + "_type", f] { showFilterTypeMenu (*combo, id, f); });
            card.type->setPackedMenu (true);
            addAndMakeVisible (*card.type);
            card.slope = std::make_unique<PillChoice> (state, prefix + "_slope", juce::StringArray { "12", "24" }, colour, "Slope\n12 dB is gentler and brighter; 24 dB cuts harder.");
            addAndMakeVisible (*card.slope);
            effectRules.add (*card.slope, [this, f] { return filterHasSlope (f); }, "this filter type has no slope");
            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" }, { "_drive", "DRIVE" },
                                      { "_env", "ENV AMOUNT" }, { "_keytrack", "KEY TRACK" }, { "_fm", "AUDIO FM" } })
            {
                card.knobs.push_back (knob (prefix + spec.first, spec.second, colour));
                card.knobs.back()->setPackedNameWrap (true);
                // A DX7 voice has no filter until CUTOFF comes down: the rest dims (I14-3).
                if (juce::String (spec.first) != "_cutoff")
                    effectRules.add (*card.knobs.back(), [this] { return ! operatorFilterOff; },
                                     "a DX7 voice has no filter: turn CUTOFF down to bring one in");
            }
        }

        // WEST COAST: its switch, PLACE and GATE menus in the header, seven knobs.
        westDisplay = std::make_unique<WestDisplay> (p);
        addAndMakeVisible (*westDisplay);
        westOn = std::make_unique<ToggleControl> (state, "west_on", "ON");
        westOn->setBareSwitch (true);
        westOn->setSwitchColour (westColour());
        addAndMakeVisible (*westOn);
        westMode = std::make_unique<ComboControl> (state, "west_mode", "");
        westPosition = std::make_unique<ComboControl> (state, "west_pos", "");
        {
            // "Replace Filter 2" squeezed the menu's font (I7-43); the saved choice string stays.
            auto& box = westPosition->getComboBox();
            const auto selectedId = box.getSelectedId();
            box.changeItemText (2, "Replaces F2");
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        westMode->setTooltip ("GATE\nHow the low-pass gate opens: Combo, Low Pass or VCA.");
        westMode->getComboBox().setTooltip (westMode->getTooltip());
        westPosition->setTooltip ("PLACE\nWhere WEST sits: after the filters, or in Filter 2's place.");
        westPosition->getComboBox().setTooltip (westPosition->getTooltip());
        for (auto* menu : { westMode.get(), westPosition.get() })
        {
            menu->setPackedMenu (true);
            addAndMakeVisible (*menu);
        }
        for (const auto& spec : { std::pair<const char*, const char*> { "west_fold", "FOLD" }, { "west_sym", "BIAS" }, { "west_stages", "STAGES" },
                                  { "west_decay", "DECAY" }, { "west_res", "RESO" }, { "west_strike", "STRIKE" }, { "west_open", "OPEN" } })
            westKnobs.push_back (knob (spec.first, spec.second, westColour()));

        // BODY: its type in the header, MATERIAL, SIZE and COUPLE (the coupling direction stays on FILTER).
        bodyType = std::make_unique<ComboControl> (state, "body_type", "");
        bodyType->setTooltip ("BODY TYPE\n" + juce::String ("Classic, bar, plate, bell or shell."));
        bodyType->getComboBox().setTooltip (bodyType->getTooltip());
        bodyType->setPackedMenu (true);
        addAndMakeVisible (*bodyType);
        for (const auto& spec : { std::pair<const char*, const char*> { "body_material", "MATERIAL" }, { "body_size", "SIZE" }, { "body_coupling", "COUPLE" } })
            bodyKnobs.push_back (knob (spec.first, spec.second, bodyColour()));

        // SUB + NOISE: each with its switch.
        subOn = std::make_unique<ToggleControl> (state, "subosc_on", "SUB");
        noiseOn = std::make_unique<ToggleControl> (state, "noise_on", "NOISE");
        for (auto* toggle : { subOn.get(), noiseOn.get() })
        {
            toggle->setSwitchColour (subColour());
            toggle->setPackedName (ToggleControl::NamePlace::left);
            addAndMakeVisible (*toggle);
        }
        subShape = std::make_unique<ComboControl> (state, "sub_shape", "SUB");
        subShape->setTooltip ("SUB WAVE\nSine for clean weight, Square or Saw for grit.");
        subShape->getComboBox().setTooltip (subShape->getTooltip());
        noiseType = std::make_unique<ComboControl> (state, "noise_type", "NOISE");
        noiseType->setTooltip ("NOISE COLOUR\nWhite, pink, brown, blue, violet or grey.");
        noiseType->getComboBox().setTooltip (noiseType->getTooltip());
        for (auto* menu : { subShape.get(), noiseType.get() })
        {
            menu->setPackedMenu (true);
            addAndMakeVisible (*menu);
        }
        subOctave = std::make_unique<PillChoice> (state, "sub_octave", juce::StringArray { "-2", "-1" }, subColour(), "SUB OCTAVE\n-1 or -2 octaves below the note.", std::vector<int> { 1, 0 });
        addAndMakeVisible (*subOctave);
        subLevel = knob ("subosc_level", "LEVEL", subColour());
        noiseLevel = knob ("noise_level", "LEVEL", subColour());
        noiseColour = knob ("sub_colour", "COLOUR", subColour());
        bodyDisplay = std::make_unique<BodyDisplay> (p);
        addAndMakeVisible (*bodyDisplay);
        subWave = std::make_unique<SubWaveDisplay> (p);
        addAndMakeVisible (*subWave);
        noiseBand = std::make_unique<NoiseColourDisplay> (p);
        addAndMakeVisible (*noiseBand);
        // Each half dims with its own switch; the cards keep their place.
        for (auto* part : std::initializer_list<juce::Component*> { subShape.get(), subOctave.get(), subLevel.get(), noiseColour.get() })
            effectRules.add (*part, [this] { return readInt ("subosc_on") != 0; }, "SUB is off: switch it on");
        for (auto* part : std::initializer_list<juce::Component*> { noiseType.get(), noiseLevel.get() })
            effectRules.add (*part, [this] { return readInt ("noise_on") != 0; }, "NOISE is off: switch it on");

        envelopeCard.onOpenInMod = [this] (int source)
        {
            if (const auto env = SourceEditor::envelopeOf (source); env >= 0 && onEditEnvelope != nullptr)
                onEditEnvelope (env);
        };
        lfoCard.onOpenInMod = [this] (int source)
        {
            if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) source); lfo >= 0 && onEditLfo != nullptr)
                onEditLfo (lfo);
        };
        addAndMakeVisible (envelopeCard);
        addAndMakeVisible (lfoCard);
        startTimerHz (4);
    }

    ~PlayPage() override { stopTimer(); }

    std::function<void (int)> onEditLfo, onEditEnvelope, onEditOperator;
    std::function<void (const juce::String&)> onOpenPage;

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (int f = 0; f < 2; ++f)
            paintCardWithHeader (g, filterCardBounds[(size_t) f], "FILTER " + juce::String (f + 1), filterColour (f));
        paintCardWithHeader (g, westBounds, "WEST COAST", westColour());
        paintCardWithHeader (g, bodyBounds, "BODY", bodyColour());
        paintCardWithHeader (g, subNoiseBounds, juce::String::fromUTF8 ("SUB \xc2\xb7 NOISE"), subColour());

        // SUB and NOISE name their rows at the left, in the card's colour.
        g.setColour (subColour().withAlpha (0.0f));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (sideMargin, 0);
        // The oscillator cards take what they need (all three of them, or the page scrolls them);
        // the two module bands sit under them at one height.
        const auto bottom = area.getBottom() - bottomMargin;
        const auto bandsHeight = juce::jlimit (2 * minBand + gap, 2 * maxBand + gap, bandsHeightFor (area.getHeight()));
        // (Three full oscillator cards: the bands sit right under them, the little slack goes to the foot.)
        const auto oscMin = oscView.getPage() != nullptr ? oscView.getPage()->getReferenceHeight() : 0;
        const auto bandsTop = oscMin > bottom - bandsHeight - 14 ? juce::jmin (bottom - bandsHeight, oscMin) : bottom - bandsHeight;
        oscView.setBounds (getLocalBounds().withHeight (juce::jmax (0, bandsTop)));
        oscBar.setBounds (getWidth() - sideMargin + 1, 4, sideMargin - 2, juce::jmax (0, bandsTop - 8));
        syncOscBar();

        // The filters need little height; the envelope and LFO pictures take the rest (ilana: they were too tiny).
        const auto band1Height = juce::jlimit (minBand, 84, bandsHeight - gap - 90);
        const auto bandHeight = bandsHeight - gap - band1Height;
        auto band1 = juce::Rectangle<int> (area.getX(), bandsTop, area.getWidth(), band1Height);
        auto band2 = juce::Rectangle<int> (area.getX(), bandsTop + band1Height + gap, area.getWidth(), bandHeight);
        layoutBand1 (band1);
        layoutBand2 (band2);

        // The cards' rectangles, for the layout tests' margin scan (x, y, width, height; name).
        juce::String cards;
        const auto note = [&cards] (const char* name, juce::Rectangle<int> r) { cards << name << "," << r.getX() << "," << r.getY() << "," << r.getWidth() << "," << r.getHeight() << ";"; };
        note ("FILTER 1", filterCardBounds[0]);
        note ("FILTER 2", filterCardBounds[1]);
        note ("WEST", westBounds);
        note ("ENVELOPE", envelopeCard.getBounds());
        note ("LFO", lfoCard.getBounds());
        note ("BODY", bodyBounds);
        note ("SUB NOISE", subNoiseBounds);
        getProperties().set ("playCards", cards);
    }

private:
    struct FilterCard
    {
        std::unique_ptr<FilterDisplay> display;
        std::unique_ptr<ComboControl> type;
        std::unique_ptr<PillChoice> slope;
        std::vector<std::unique_ptr<KnobControl>> knobs;
    };

    static constexpr int sideMargin = 7, bottomMargin = 4, gap = 0, hGap = 5, headerHeight = 20, cardPad = 7, cell = 35, filterCell = 33, westCell = 31;
    static constexpr int minBand = 62, maxBand = 96;
    static constexpr float dialSize = 28.0f;

    static juce::Colour filterColour (int index) { return IlanaTheme::envIdentityColour (index == 0 ? 1 : 2); }
    static juce::Colour westColour() { return FilterColours::west(); }
    static juce::Colour bodyColour() { return juce::Colour (0xffff6a2a); }
    static juce::Colour subColour() { return juce::Colour (0xffff6a2a); }

    void timerCallback() override
    {
        operatorFilterOff = operatorVoiceFilterOff (processorRef);
        for (auto& card : filters)
            card.display->setVoiceOff (operatorFilterOff);
        effectRules.apply();
    }

    bool operatorFilterOff = false;

    // The OSC area's own scroll bar (thin, over the cards' side margin), shown only while the cards overflow it.
    void syncOscBar()
    {
        auto* page = oscView.getViewedComponent();
        if (page == nullptr)
            return;
        oscBar.setRangeLimits (0.0, (double) page->getHeight());
        oscBar.setCurrentRange ((double) oscView.getViewPositionY(), (double) oscView.getHeight());
        oscBar.setVisible (page->getHeight() > oscView.getHeight() + 1);
    }

    void scrollBarMoved (juce::ScrollBar*, double newStart) override
    {
        oscView.setViewPosition (0, oscView.snapToCard (juce::roundToInt (newStart)));
    }

    int readInt (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // What the bands may take: the oscillator cards' own height (their page's
    // minimum, with its add row) first.
    int bandsHeightFor (int pageHeight) const
    {
        auto oscHeight = 0;
        if (auto* page = oscView.getPage())
            oscHeight = page->getReferenceHeight();
        return pageHeight - bottomMargin - oscHeight;
    }

    void paintCardWithHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint) const
    {
        if (card.isEmpty())
            return;

        IlanaTheme::paintCard (g, card.toFloat(), 10.0f, tint);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (card.getX() + 1, card.getY() + headerHeight - 1, card.getWidth() - 2, 1));
        const auto centre = card.getY() + headerHeight / 2;
        IlanaTheme::paintTag (g, { (float) card.getX() + 14.0f, (float) centre }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (13.0f, true));
        IlanaTheme::drawFitted (g, title, juce::Rectangle<int> (card.getX() + 23, centre - 9, 80, 18), juce::Justification::centredLeft, 1);
    }

    static int titleWidth (const juce::String& title)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (13.0f, true)), title) + 4;
    }

    // Knobs in one row at the 35 unit pitch, centred in `area`.
    static void placeKnobRow (juce::Rectangle<int> area, const std::vector<KnobControl*>& row, int pitch = cell, int extraBelow = 0)
    {
        const auto width = (int) row.size() * pitch;
        auto x = area.getX() + (area.getWidth() - width) / 2;
        for (auto* knob : row)
        {
            knob->setBounds (KnobControl::packedBoundsFor ({ x, area.getY() + (area.getHeight() - (int) dialSize - 10 - extraBelow) / 2, pitch, (int) dialSize + 10 + extraBelow }));
            x += pitch;
        }
    }

    static std::vector<KnobControl*> pointers (const std::vector<std::unique_ptr<KnobControl>>& knobs)
    {
        std::vector<KnobControl*> row;
        for (auto& knob : knobs)
            row.push_back (knob.get());
        return row;
    }

    juce::Rectangle<int> bodyOf (juce::Rectangle<int> card) const
    {
        return card.withTrimmedTop (headerHeight).reduced (cardPad, 3);
    }

    // The switch at the right of a header, as the OSC cards': 30 x 17.
    static juce::Rectangle<int> switchIn (juce::Rectangle<int> card)
    {
        return juce::Rectangle<int> (30, 17).withCentre ({ card.getRight() - 6 - 15, card.getY() + headerHeight / 2 });
    }

    void layoutBand1 (juce::Rectangle<int> band)
    {
        const auto total = band.getWidth() - 2 * hGap;
        const auto filterWidth = juce::roundToInt ((float) total * 0.358f);
        auto row = band;
        for (int f = 0; f < 2; ++f)
        {
            filterCardBounds[(size_t) f] = row.removeFromLeft (filterWidth);
            row.removeFromLeft (hGap);
        }
        westBounds = row;

        for (int f = 0; f < 2; ++f)
        {
            const auto card = filterCardBounds[(size_t) f];
            auto& parts = filters[(size_t) f];
            const auto centre = card.getY() + headerHeight / 2;
            auto x = card.getX() + 23 + titleWidth ("FILTER 1") + 6;
            parts.type->setBounds ({ x, centre - 8, 92, 16 });
            x += 92 + 6;
            parts.slope->setBounds ({ x, centre - 8, 44, 16 });

            // (The graph's edge is 5 from the card on the left; the last dial's ring is 5 from it on the right.)
            auto body = card.withTrimmedTop (headerHeight).withTrimmedLeft (5).withTrimmedRight (1).reduced (0, 3);
            const auto knobsWidth = (int) parts.knobs.size() * filterCell;
            parts.display->setBounds (body.removeFromLeft (body.getWidth() - knobsWidth - 2));
            placeKnobRow (body, pointers (parts.knobs), filterCell, 9);
        }

        {
            const auto centre = westBounds.getY() + headerHeight / 2;
            westOn->setBounds (switchIn (westBounds));
            auto x = westBounds.getX() + 23 + titleWidth ("WEST COAST") + 6;
            westMode->setBounds ({ x, centre - 8, 52, 16 });
            x += 52 + 2;
            // (As wide as "After Filters" needs, not stretched to the switch.)
            westPosition->setBounds ({ x, centre - 8, juce::jmin (84, juce::jmax (40, westBounds.getRight() - 6 - 30 - 2 - x)), 16 });
            // The picture at the left, the seven knobs at a 33 pitch to its right.
            auto body = westBounds.withTrimmedTop (headerHeight).withTrimmedLeft (5).withTrimmedRight (1).reduced (0, 3);
            const auto knobsWidth = (int) westKnobs.size() * westCell;
            westDisplay->setBounds (body.removeFromLeft (body.getWidth() - knobsWidth - 2));
            placeKnobRow (body, pointers (westKnobs), westCell, 9);
        }
    }

    void layoutBand2 (juce::Rectangle<int> band)
    {
        const auto total = band.getWidth() - 3 * hGap;
        const auto envWidth = juce::roundToInt ((float) total * 0.3594f);
        const auto lfoWidth = juce::roundToInt ((float) total * 0.2895f);
        const auto bodyWidth = juce::roundToInt ((float) total * 0.1180f);
        auto row = band;
        envelopeCard.setBounds (row.removeFromLeft (envWidth));
        row.removeFromLeft (hGap);
        lfoCard.setBounds (row.removeFromLeft (lfoWidth));
        row.removeFromLeft (hGap);
        bodyBounds = row.removeFromLeft (bodyWidth);
        row.removeFromLeft (hGap);
        subNoiseBounds = row;

        {
            const auto centre = bodyBounds.getY() + headerHeight / 2;
            const auto menuX = bodyBounds.getX() + 23 + titleWidth ("BODY") + 4;
            // (The menu is as wide as its longest word needs, like the other header menus, not stretched to the card's edge.)
            bodyType->setBounds ({ menuX, centre - 8, juce::jmin (juce::jmax (36, bodyBounds.getRight() - 7 - menuX), 66), 16 });
            const auto body = bodyOf (bodyBounds).translated (3, 0);
            placeKnobRow (body, pointers (bodyKnobs));
            // The resonance picture takes the strip above the dials, the dials staying level with their neighbours'.
            const auto dialTop = body.getY() + (body.getHeight() - (int) dialSize - 10) / 2;
            const auto top = bodyBounds.getY() + headerHeight + 3;
            bodyDisplay->setBounds (juce::Rectangle<int> (bodyOf (bodyBounds).getX(), top, bodyOf (bodyBounds).getWidth(), juce::jmax (8, dialTop - 3 - top)));
        }

        {
            auto body = subNoiseBounds.withTrimmedTop (headerHeight).reduced (8, 3);
            const auto centre = subNoiseBounds.getY() + headerHeight / 2;
            noiseOn->setBounds (juce::Rectangle<int> (66, 17).withCentre ({ subNoiseBounds.getRight() - 6 - 33, centre }));
            subOn->setBounds (juce::Rectangle<int> (54, 17).withCentre ({ noiseOn->getX() - 4 - 27, centre }));
            const auto menuHeight = 16;
            const auto knobY = body.getY() + (body.getHeight() - (int) dialSize - 10) / 2;
            const auto knobAt = [&] (KnobControl& knob, int x) { knob.setBounds (KnobControl::packedBoundsFor ({ x, knobY, cell, (int) dialSize + 10 })); };
            // One row at one gap: SUB's wave menu over its octave pills, LEVEL, COLOUR, then NOISE's colour menu and LEVEL.
            // Both menus are the same size and sit on the dials' middle; the gaps are measured between what is drawn
            // (a menu's edges, a dial's ring, COLOUR's name), the same between every pair and at both ends.
            const auto menuWidth = 50, ringWidth = (int) dialSize, colourInk = 30;
            body = subNoiseBounds.withTrimmedTop (headerHeight).reduced (8, 3);
            const auto ink = 2 * menuWidth + 2 * ringWidth + colourInk;
            const auto gapInk = juce::jmax (2, (body.getWidth() - ink) / 6);
            auto x = body.getX() + gapInk;
            const auto subLeft = x;
            auto stack = juce::Rectangle<int> (x, knobY + (int) dialSize / 2 - (menuHeight * 2 + 3) / 2, menuWidth, menuHeight * 2 + 3);
            subShape->setBounds (stack.removeFromTop (menuHeight));
            stack.removeFromTop (3);
            subOctave->setBounds (stack);
            x += menuWidth + gapInk;
            knobAt (*subLevel, x - (cell - ringWidth) / 2);
            x += ringWidth + gapInk;
            knobAt (*noiseColour, x - (cell - colourInk) / 2);
            const auto subRight = x + colourInk;
            x += colourInk + gapInk;
            const auto noiseLeft = x;
            noiseType->setBounds ({ x, knobY + (int) dialSize / 2 - menuHeight / 2, menuWidth, menuHeight });
            x += menuWidth + gapInk;
            knobAt (*noiseLevel, x - (cell - ringWidth) / 2);
            const auto right = x + ringWidth;
            // The live strips over their halves, as BODY's over its dials.
            const auto top = subNoiseBounds.getY() + headerHeight + 3;
            const auto stripH = juce::jmax (8, knobY - 3 - top);
            subWave->setBounds ({ subLeft, top, subRight - subLeft, stripH });
            noiseBand->setBounds ({ noiseLeft, top, right - noiseLeft, stripH });
        }
    }

    // PLAY's TYPE list, grouped as the FILTER page's grid: its pages, then
    // its families, under the tiles' names.
    void showFilterTypeMenu (juce::ComboBox& combo, const juce::String& id, int filterIndex)
    {
        auto* parameter = processorRef.apvts.getParameter (id);

        if (parameter == nullptr)
            return;

        const auto current = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
        const auto names = FilterTypeGrid::shortNames();
        juce::PopupMenu menu;

        for (int page = 0; page < FilterTypeGrid::getNumPages(); ++page)
        {
            juce::PopupMenu pageMenu;
            auto holdsCurrent = false;

            for (const auto& family : FilterTypeGrid::getFamilies (page))
            {
                pageMenu.addSectionHeader (family.first);

                for (const auto type : family.second)
                {
                    pageMenu.addItem (type + 1, names[type], true, type == current);
                    holdsCurrent = holdsCurrent || type == current;
                }
            }

            menu.addSubMenu (FilterTypeGrid::getPageName (page), pageMenu, true, nullptr, holdsCurrent);
        }

        juce::Component::SafePointer<PlayPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&combo),
                            [safe, parameter, filterIndex] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;

                                safe->processorRef.performEdit ("Filter " + juce::String (filterIndex + 1) + " type", [parameter, result]
                                {
                                    parameter->beginChangeGesture();
                                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                    parameter->endChangeGesture();
                                });
                            });
    }

    // As on the FILTER page: only the classic models have a slope.
    bool filterHasSlope (int filterIndex) const
    {
        const auto type = juce::jlimit (0, FilterType::Count - 1, readInt (filterIndex == 0 ? "f1_type" : "f2_type"));
        return type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
               && FilterType::usesSlope (type);
    }

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    OscPageViewport oscView;
    OscScrollBar oscBar { true };
    SourceCarouselCard envelopeCard, lfoCard;
    std::array<FilterCard, 2> filters;
    std::array<juce::Rectangle<int>, 2> filterCardBounds;
    juce::Rectangle<int> westBounds, bodyBounds, subNoiseBounds;
    std::unique_ptr<ToggleControl> westOn, subOn, noiseOn;
    std::unique_ptr<WestDisplay> westDisplay;
    std::unique_ptr<BodyDisplay> bodyDisplay;
    std::unique_ptr<SubWaveDisplay> subWave;
    std::unique_ptr<NoiseColourDisplay> noiseBand;
    std::unique_ptr<ComboControl> westMode, westPosition, bodyType, subShape, noiseType;
    std::unique_ptr<PillChoice> subOctave;
    std::vector<std::unique_ptr<KnobControl>> westKnobs, bodyKnobs;
    std::unique_ptr<KnobControl> subLevel, noiseLevel, noiseColour;
};
} // namespace
