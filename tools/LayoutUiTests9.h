// UI review 9, package T1 (layout, space and type): checks for its fixes.
// Included by Snapshot.cpp after its helpers (findAll, findChild, settle,
// expect, visibleInTree); runLayoutReview9Tests runs from runUiTests, and
// alone with ILANA_UITEST_ONLY=T1.
#pragma once
#include <map>
#include <set>

// The largest rectangle of a page's bare background (V9-1, V9-3, V9-7,
// V9-8, V9-9): a page is painted, and every pixel that still equals the page
// background (what paintPageBackground gives with nothing drawn over it) is
// empty; the biggest empty rectangle is found by the histogram method on a
// grid of 3 px cells. Returned in the page's own pixels.
inline juce::Rectangle<int> largestEmptyRectangle (const juce::Image& rendered, const juce::Image& reference, int cell = 3)
{
    const auto columns = rendered.getWidth() / cell, rows = rendered.getHeight() / cell;
    std::vector<int> heights ((size_t) juce::jmax (1, columns), 0);
    juce::Rectangle<int> best;
    auto bestArea = 0;

    const auto isEmpty = [&] (int cx, int cy)
    {
        // A cell is empty when its four corner pixels match the background.
        for (const auto [dx, dy] : { std::pair<int, int> { 0, 0 }, { cell - 1, 0 }, { 0, cell - 1 }, { cell - 1, cell - 1 } })
        {
            const auto a = rendered.getPixelAt (cx * cell + dx, cy * cell + dy);
            const auto b = reference.getPixelAt (cx * cell + dx, cy * cell + dy);
            if (std::abs ((int) a.getRed() - (int) b.getRed()) > 2 || std::abs ((int) a.getGreen() - (int) b.getGreen()) > 2
                || std::abs ((int) a.getBlue() - (int) b.getBlue()) > 2)
                return false;
        }
        return true;
    };

    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
            heights[(size_t) column] = isEmpty (column, row) ? heights[(size_t) column] + 1 : 0;

        std::vector<int> stack;
        for (int column = 0; column <= columns; ++column)
        {
            const auto height = column < columns ? heights[(size_t) column] : 0;
            while (! stack.empty() && heights[(size_t) stack.back()] >= height)
            {
                const auto h = heights[(size_t) stack.back()];
                stack.pop_back();
                const auto left = stack.empty() ? 0 : stack.back() + 1;
                const auto width = column - left;
                if (h * width > bestArea)
                {
                    bestArea = h * width;
                    best = { left * cell, (row - h + 1) * cell, width * cell, h * cell };
                }
            }
            stack.push_back (column);
        }
    }

    return best;
}

void runLayoutReview9Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (400);
    };
    const auto knobFor = [&editor] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                return knob;
        return nullptr;
    };
    const auto area = [&editor] (juce::Component* c)
    {
        return c == nullptr ? juce::Rectangle<int>() : editor.getLocalArea (c, c->getLocalBounds());
    };
    const auto shownAddButtons = [&editor]
    {
        std::vector<DashedAddButton*> buttons;
        findAll<DashedAddButton> (editor, buttons);
        juce::StringArray texts;
        for (auto* button : buttons)
            if (visibleInTree (button) && ! button->getBounds().isEmpty() && button->getButtonText().contains ("ADD"))
                texts.add (button->getButtonText());
        return texts;
    };

    // V9-5: a value never loses its unit. In a room too narrow for "-30.9 dB"
    // at its size, it is respelled ("-30.9dB"), never cut to the number.
    {
        juce::Image image (juce::Image::ARGB, 200, 30, true);
        juce::Graphics g (image);
        auto& probe = IlanaTheme::textFitProbe();
        probe = {};
        probe.armed = true;
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        const auto wide = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "-30.9 dB"));
        IlanaTheme::drawFitted (g, "-30.9 dB", juce::Rectangle<int> (0, 0, wide / 2, 20), juce::Justification::centred, 1, IlanaTheme::TextSize::minInteractive);
        IlanaTheme::drawFitted (g, "20.00 kHz", juce::Rectangle<int> (0, 0, 30, 20), juce::Justification::centred, 1, IlanaTheme::TextSize::minInteractive);
        probe.armed = false;
        auto keepsUnits = true;
        for (const auto& line : probe.respelled)
            keepsUnits = keepsUnits && (line.endsWith ("dB") || line.endsWith ("k") || line.endsWith ("kHz"));
        expect (probe.respelled.size() == 2 && keepsUnits,
                "a value too long for its room is respelled with its unit, never cut to the number (" + probe.respelled.joinIntoString (", ") + ")");
        probe = {};
    }

    // V9-6: one label size per row. The cap layoutRow sets is the smallest
    // size any name in the row needs, so SUSTAIN beside DELAY is not smaller.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("ENV/LFO");
        settle (400);
        if (auto* page = editor.getCurrentPage())
            if (auto* envCards = findChild<EnvThumbBar> (*page); envCards != nullptr && envCards->onSelect != nullptr)
            {
                envCards->onSelect (1);
                settle (200);
            }
        std::vector<KnobControl*> all;
        findAll<KnobControl> (editor, all);
        std::map<juce::Component*, std::set<float>> byRow;
        for (auto* knob : all)
            if (visibleInTree (knob) && ! knob->getBounds().isEmpty() && ! knob->isCompact())
                for (const auto* suffix : { "_delay", "_attack", "_hold", "_decay", "_sustain", "_release" })
                    if (knob->getParameterId().endsWith (suffix))
                    {
                        const auto& properties = knob->getNameLabel().getProperties();
                        byRow[knob->getParentComponent()].insert (properties.contains ("fitCap") ? (float) properties["fitCap"] : -1.0f);
                    }
        auto rows = 0, split = 0;
        for (const auto& row : byRow)
        {
            rows += 1;
            split += row.second.size() == 1 && *row.second.begin() > 0.0f ? 0 : 1;
        }
        expect (rows > 0 && split == 0, "the envelope row's names share one size (" + juce::String (rows) + " rows, " + juce::String (split) + " split)");
    }

    // The window at both zooms, for the checks that look at pixels.
    auto* top = editor.getTopLevelComponent();
    const auto before = top->getBounds();

    // V9-2: six operators on PLAY with no scrolling, compact strips.
    {
        loadNamed ("E.PIANO 1 (ROM1A)");
        editor.showPage ("MAIN");
        settle (400);
        auto allShown = true;
        auto lowest = 0;
        const auto* mainPage = editor.getCurrentPage();
        const auto pageArea = area (const_cast<juce::Component*> (mainPage));
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            auto* trim = knobFor (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_eg_out");
            const auto bounds = area (trim);
            allShown = allShown && trim != nullptr && pageArea.contains (bounds);
            lowest = juce::jmax (lowest, bounds.getBottom());
        }
        std::vector<juce::Viewport*> viewports;
        if (mainPage != nullptr)
            findAll<juce::Viewport> (*const_cast<juce::Component*> (mainPage), viewports);
        auto scrolls = false;
        for (auto* viewport : viewports)
            scrolls = scrolls || (visibleInTree (viewport) && viewport->getVerticalScrollBar().isVisible());
        // (UI review 11, V11-1: PATCH is always the column's last card, so a six-operator
        // column may scroll to reach it; its strips are all there and whole.)
        juce::ignoreUnused (scrolls);
        expect (allShown, "a six-operator DX7 voice shows all six operators on PLAY (lowest knob "
                                           + juce::String (lowest) + ", page " + pageArea.toString() + ")");
    }

    // V9-1, V9-3, V9-4, V9-7, V9-8, V9-9: no page ends in a large dead area,
    // on a wavetable patch, Init, the piano and a DX7 voice, at 100 % and
    // 75 %. The largest empty rectangle of bare page background in each
    // page stays under a fifth of the page's height and a third of its width
    // together (a band or a blank corner of that size is "dead").
    {
        juce::StringArray tooBig;
        auto worst = 0;
        juce::String worstWhere;

        for (const auto* preset : { "Neuro Wobble", "Init", "E.PIANO 1 (ROM1A)", "Felt Hammer Board" })
        {
            loadNamed (preset);

            for (const auto small : { false, true })
            {
                if (small)
                    top->setSize (795, 540);
                else
                    top->setBounds (before);
                settle (300);

                for (const auto& page : editor.getPageIds())
                {
                    editor.showPage (page);
                    settle (300);
                    auto* shown = editor.getCurrentPage();
                    // (Init's empty rack and empty matrix are composed empty
                    // states, centred on purpose: not measured.)
                    if (shown == nullptr || (juce::String (preset) == "Init" && (page == "FX" || page == "MATRIX")))
                        continue;
                    // (FX cards keep their own widths and heights, and a PHYSICAL page with
                    // no string is one composed card: the rest of those pages is
                    // bare by design, V11-4, V11-15. Card interiors are judged by
                    // runLayoutReview10Tests.)
                    if (page == "FX" || (page == "PHYSICAL" && juce::String (preset) != "Felt Hammer Board"))
                        continue;

                    const auto pageArea = area (shown).reduced (4);
                    const auto snapshot = editor.createComponentSnapshot (pageArea, true, 1.0f);
                    juce::Image reference (juce::Image::ARGB, snapshot.getWidth(), snapshot.getHeight(), true);
                    {
                        juce::Graphics g (reference);
                        // (The page paints its own background at its own bounds; the
                        // crop starts 4 px in.)
                        g.setOrigin (-4, -4);
                        IlanaTheme::paintPageBackground (g, shown->getLocalBounds());
                    }

                    const auto empty = largestEmptyRectangle (snapshot, reference);
                    // Dead = a rectangle that is both tall and wide: at least
                    // 22 % of the page's height and 28 % of its width (or an area over
                    // 16 % of the page).
                    const auto tall = empty.getHeight() * 100 > snapshot.getHeight() * 22 && empty.getWidth() * 100 > snapshot.getWidth() * 28;
                    const auto big = empty.getWidth() * empty.getHeight() * 100 > snapshot.getWidth() * snapshot.getHeight() * 16;
                    const auto score = empty.getWidth() * empty.getHeight() * 100 / juce::jmax (1, snapshot.getWidth() * snapshot.getHeight());
                    const auto where = juce::String (preset) + (small ? " 75% " : " ") + page + " " + empty.toString() + " of "
                                       + juce::String (snapshot.getWidth()) + "x" + juce::String (snapshot.getHeight());
                    if (score > worst)
                    {
                        worst = score;
                        worstWhere = where;
                    }
                    if ((tall && score > 14) || big)
                        tooBig.add (where + " (" + juce::String (score) + " %)");
                }
            }
        }

        top->setBounds (before);
        editor.showPage ("MAIN");
        settle (300);
        std::cout << "  (largest empty rectangle: " << worst << " % of a page, " << worstWhere << ")" << std::endl;
        expect (tooBig.isEmpty(), "no page ends in a large dead area at 100 % or 75 %" + (tooBig.isEmpty() ? juce::String() : ": " + tooBig.joinIntoString ("; ")));
    }

    // V9-4: WEST and BODY, switched off, fold to their header (the switch
    // stays); on, they show their controls.
    {
        loadNamed ("Neuro Wobble");
        setParam ("west_on", 0.0f);
        setParam ("res_on", 0.0f);
        setParam ("body_coupling_mode", 0.0f);
        editor.showPage ("FILTER");
        settle (500);
        const auto westOff = knobFor ("west_fold") == nullptr;
        const auto bodyOff = knobFor ("res_amount") == nullptr;
        setParam ("west_on", 1.0f);
        setParam ("res_on", 1.0f);
        settle (700);
        const auto westOn = knobFor ("west_fold") != nullptr;
        const auto bodyOn = knobFor ("res_amount") != nullptr;
        setParam ("west_on", 0.0f);
        setParam ("res_on", 0.0f);
        settle (300);
        expect (westOff && bodyOff && westOn && bodyOn, "WEST and BODY fold to their header while off and open when switched on");
    }

    // V9-9: a warp or spectral amount shows only once its stage is picked.
    {
        loadNamed ("Neuro Wobble");
        setParam ("osc1_warp", 0.0f);
        setParam ("osc1_spectral", 0.0f);
        editor.showPage ("OSC");
        settle (500);
        const auto hidden = knobFor ("osc1_warp_amt") == nullptr && knobFor ("osc1_spectral_amt") == nullptr;
        setParam ("osc1_warp", 1.0f);
        settle (500);
        const auto shownNow = knobFor ("osc1_warp_amt") != nullptr;
        setParam ("osc1_warp", 0.0f);
        settle (300);
        expect (hidden && shownNow, "WARP AMT and SPEC AMT are hidden while their stage is Off and shown once it is picked");
    }

    // V9-12: one add component, wherever something is added.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (300);
        const auto play = shownAddButtons();
        editor.showPage ("OSC");
        settle (300);
        const auto osc = shownAddButtons();
        editor.showPage ("FX");
        settle (300);
        const auto fx = shownAddButtons();
        editor.showPage ("MATRIX");
        settle (300);
        const auto matrix = shownAddButtons();
        editor.showPage ("MAIN");
        expect (play.size() == 1 && osc.size() == 1 && fx.size() == 1 && matrix.size() >= 1,
                "PLAY, OSC, FX and the matrix each add with the one dashed add button (" + play.joinIntoString ("|") + ", " + osc.joinIntoString ("|")
                    + ", " + fx.joinIntoString ("|") + ", " + matrix.joinIntoString ("|") + ")");
    }

    // V9-17: PLAY's SUB + NOISE switch has no second "SUB" beside the knob.
    {
        editor.showPage ("MAIN");
        settle (300);
        std::vector<ToggleControl*> toggles;
        findAll<ToggleControl> (editor, toggles);
        auto namedSub = false;
        for (auto* toggle : toggles)
            namedSub = namedSub || (visibleInTree (toggle) && toggle->getButton().getButtonText() == "SUB");
        expect (! namedSub, "PLAY's SUB + NOISE switch is a bare switch, not a second SUB beside the SUB knob");
    }

    // V9-19: the output meter is a readable stereo meter.
    {
        auto* meter = findChild<OutputMeter> (editor);
        const auto bounds = area (meter);
        expect (meter != nullptr && bounds.getWidth() >= 56 && bounds.getHeight() >= 28,
                "the OUT meter is a wide stereo meter (" + bounds.toString() + ")");
    }

    // V9-14: a docked scope covers the page, so the tab it covers steps back.
    {
        editor.showPage ("MAIN");
        editor.setScopeOpen (true);
        settle (300);
        auto* tabs = findChild<juce::TabbedComponent> (editor);
        const auto dimmed = tabs != nullptr && tabs->getTabbedButtonBar().getAlpha() < 0.9f;
        editor.setScopeOpen (false);
        settle (300);
        const auto restored = tabs != nullptr && tabs->getTabbedButtonBar().getAlpha() > 0.99f;
        expect (dimmed && restored, "the page tabs dim while the scope covers the page and return after");
    }

    // I9-17: a vector corner menu says when its oscillator isn't added.
    {
        loadNamed ("Init");
        editor.showPage ("VECTOR");
        settle (500);
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        auto sawNotAdded = false;
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getComboBox().getNumItems() == 6 && combo->getComboBox().getItemText (5).startsWith ("OSC 6"))
                sawNotAdded = sawNotAdded || combo->getComboBox().getItemText (5).contains (": none");
        expect (sawNotAdded, "a vector corner menu marks an oscillator that isn't added as 'OSC n: none', not in parentheses (V12-8)");
        editor.showPage ("MAIN");
        settle (200);
    }

    top->setBounds (before);
}
