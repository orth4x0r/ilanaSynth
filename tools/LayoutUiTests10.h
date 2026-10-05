// UI review 10, package U1 (layout, type and theme): checks for its fixes.
// Included by Snapshot.cpp after LayoutUiTests9.h; runLayoutReview10Tests runs
// from runUiTests, and alone with ILANA_UITEST_ONLY=U1.
#pragma once
#include <map>
#include <set>

// The most of its card a rectangle of bare card fill may take, in thousandths
// (V10-3; the card is found by scanning out to the page's background, which
// over-reads a card whose cells are as dark as the page, so this is generous).
constexpr int maxEmptyCardPermille = 450;

void runLayoutReview10Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (400);
    };

    // V10-1: no double-encoded UTF-8 ("Â·", "â€", "Ã") in any string literal
    // of the sources, nor in text on any page (labels, buttons, menus, tips,
    // what the fitter draws, the wave view's captions).
    {
        const auto sources = findSourceTree();
        juce::StringArray literals;
        if (! sources.isDirectory())
            literals.add ("(the source tree wasn't found)");
        else
            for (const auto& file : sources.findChildFiles (juce::File::findFiles, true, "*.h;*.cpp"))
            {
                // (Comments in ported third-party code may quote a song's "Âµ".)
                if (file.getFullPathName().contains ("thirdparty") || file.getFullPathName().contains ("airwindows"))
                    continue;
                auto number = 0;
                for (const auto& line : juce::StringArray::fromLines (file.loadFileAsString()))
                {
                    ++number;
                    const auto code = line.upToFirstOccurrenceOf ("//", false, false);
                    if (IlanaTheme::isMojibake (code))
                        literals.add (file.getFileName() + ":" + juce::String (number));
                }
            }
        expect (literals.isEmpty(), "no string literal holds double-encoded UTF-8"
                                        + (literals.isEmpty() ? juce::String() : ": " + literals.joinIntoString (", ")));

        auto& probe = IlanaTheme::textFitProbe();
        juce::StringArray garbled;
        const auto scanTree = [&] (const juce::String& where)
        {
            std::vector<juce::Label*> labels;
            findAll<juce::Label> (editor, labels);
            for (auto* label : labels)
                if (IlanaTheme::isMojibake (label->getText()) || IlanaTheme::isMojibake (label->getTooltip()))
                    garbled.addIfNotAlreadyThere (where + ": " + label->getText());
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (editor, buttons);
            for (auto* button : buttons)
                if (IlanaTheme::isMojibake (button->getButtonText()) || IlanaTheme::isMojibake (button->getTooltip()))
                    garbled.addIfNotAlreadyThere (where + ": " + button->getButtonText());
            std::vector<juce::ComboBox*> combos;
            findAll<juce::ComboBox> (editor, combos);
            for (auto* combo : combos)
                for (int i = 0; i < combo->getNumItems(); ++i)
                    if (IlanaTheme::isMojibake (combo->getItemText (i)))
                        garbled.addIfNotAlreadyThere (where + ": " + combo->getItemText (i));
            std::vector<WaveDisplay*> waves;
            findAll<WaveDisplay> (editor, waves);
            for (auto* wave : waves)
                if (IlanaTheme::isMojibake (wave->getFrameReadout()) || IlanaTheme::isMojibake (wave->getDragHint()))
                    garbled.addIfNotAlreadyThere (where + ": " + wave->getFrameReadout());
        };

        for (const auto* preset : { "Neuro Wobble", "Felt Hammer Board", "E.PIANO 1 (ROM1A)" })
        {
            loadNamed (preset);
            for (const auto& page : editor.getPageIds())
            {
                editor.showPage (page);
                settle (200);
                probe = {};
                probe.armed = true;
                editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
                probe.armed = false;
                for (const auto& text : probe.garbled)
                    garbled.addIfNotAlreadyThere (juce::String (preset) + " " + page + ": " + text);
                scanTree (juce::String (preset) + " " + page);
            }
        }
        probe = {};
        editor.showPage ("MAIN");
        expect (garbled.isEmpty(), "no page shows double-encoded UTF-8 text" + (garbled.isEmpty() ? juce::String() : ": " + garbled.joinIntoString (" | ")));
    }

    // V10-3: no card is mostly air. LayoutUiTests9's measure counts only bare
    // page background, so a card stretched taller around the same controls
    // passed. This one looks inside the cards: the biggest rectangle that is
    // still the card's own fill (no text, graph, control or line on it) in
    // every page; the review's cases (a 195 px strip with 70 px empty under
    // its controls, an OSC card with a quarter of it blank, an FM matrix of
    // empty boxes) are all such rectangles.
    {
        // The measure itself: a card with its controls at the top and nothing
        // under them is caught; one filled evenly is not.
        const auto measure = [] (const juce::Image& image, juce::Rectangle<int>* where = nullptr)
        {
            auto best = 0;
            for (const auto& fill : { IlanaTheme::Ui::panel, IlanaTheme::Ui::bg.interpolatedWith (IlanaTheme::Ui::panel, 0.5f) })
            {
                juce::Image reference (juce::Image::ARGB, image.getWidth(), image.getHeight(), true);
                {
                    juce::Graphics g (reference);
                    g.fillAll (fill);
                }
                const auto empty = largestEmptyRectangle (image, reference);
                if (empty.getWidth() * empty.getHeight() > best && where != nullptr)
                    *where = empty;
                best = juce::jmax (best, empty.getWidth() * empty.getHeight());
            }
            return best;
        };
        const auto drawCard = [] (int contentHeight)
        {
            juce::Image image (juce::Image::ARGB, 480, 200, true);
            juce::Graphics g (image);
            g.fillAll (IlanaTheme::Ui::panel);
            g.setColour (IlanaTheme::Ui::text2);
            for (int i = 0; i < contentHeight; i += 6)
                g.fillRect (10, i + 4, 460, 2);
            return image;
        };
        const auto bare = measure (drawCard (60)), full = measure (drawCard (200));
        expect (bare > 480 * 200 / 2 && full < 480 * 200 / 10,
                "the card-interior measure sees a card with 70 % of it empty (" + juce::String (bare) + " px) and not a filled one (" + juce::String (full) + " px)");

        auto* top = editor.getTopLevelComponent();
        const auto before = top->getBounds();
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
                    // (Init's empty rack and matrix are composed empty states.)
                    if (shown == nullptr || (juce::String (preset) == "Init" && (page == "FX" || page == "MATRIX")))
                        continue;

                    const auto pageArea = editor.getLocalArea (shown, shown->getLocalBounds()).reduced (4);
                    const auto snapshot = editor.createComponentSnapshot (pageArea, true, 1.0f);
                    // (A PHYSICAL page that has no physical oscillator is a composed
                    // empty state: its picture and message are not measured.)
                    if (page == "PHYSICAL" && juce::String (preset) != "Felt Hammer Board")
                        continue;
                    // (The matrix's rows and its REMAP note share the page's
                    // height by their own rule, tested in LayoutUiTests9.)
                    if (page == "MATRIX")
                        continue;

                    juce::Rectangle<int> emptyAt;
                    const auto empty = measure (snapshot, &emptyAt);
                    // The card it lies in: out from the rectangle to the page's
                    // own background on each side.
                    const auto isPage = [&snapshot] (int x, int y)
                    {
                        const auto c = snapshot.getPixelAt (x, y);
                        return std::abs ((int) c.getRed() - (int) IlanaTheme::Ui::bg.getRed()) <= 1
                               && std::abs ((int) c.getGreen() - (int) IlanaTheme::Ui::bg.getGreen()) <= 1
                               && std::abs ((int) c.getBlue() - (int) IlanaTheme::Ui::bg.getBlue()) <= 1;
                    };
                    auto card = emptyAt;
                    if (! emptyAt.isEmpty())
                    {
                        const auto cx = emptyAt.getCentreX(), cy = emptyAt.getCentreY();
                        auto left = cx, right = cx, topY = cy, bottomY = cy;
                        while (left > 0 && ! isPage (left, cy)) --left;
                        while (right < snapshot.getWidth() - 1 && ! isPage (right, cy)) ++right;
                        while (topY > 0 && ! isPage (cx, topY)) --topY;
                        while (bottomY < snapshot.getHeight() - 1 && ! isPage (cx, bottomY)) ++bottomY;
                        card = juce::Rectangle<int> (left, topY, right - left, bottomY - topY).getUnion (emptyAt);
                    }
                    const auto score = empty * 1000 / juce::jmax (1, card.getWidth() * card.getHeight());
                    // (A card too small to hold a real block of air is not judged.)
                    const auto judged = card.getWidth() * card.getHeight() >= 24000;
                    const auto where = juce::String (preset) + (small ? " 75% " : " ") + page + " (" + juce::String (score / 10.0, 1) + " % of its card "
                                       + card.toString() + ", empty " + emptyAt.toString() + ")";
                    if (judged && score > worst)
                    {
                        worst = score;
                        worstWhere = where;
                    }
                    if (judged && score > 350)
                        std::cout << "    empty card interior " << where << std::endl;
                    if (judged && score > maxEmptyCardPermille)
                        tooBig.add (where);
                }
            }
        }

        top->setBounds (before);
        editor.showPage ("MAIN");
        settle (300);
        std::cout << "  (largest empty card interior: " << worst / 10.0 << " % of its card, " << worstWhere << ")" << std::endl;
        expect (tooBig.isEmpty(), "no card holds a large empty interior at 100 % or 75 %" + (tooBig.isEmpty() ? juce::String() : ": " + tooBig.joinIntoString ("; ")));
    }

    // V10-11: SAVE AS's tag chips clear the tags field above them, with the
    // field open (tags the chips don't offer) and closed.
    {
        loadNamed ("Neuro Wobble");
        editor.savePresetAs();
        settle (300);
        auto& overlay = editor.getSaveOverlay();
        auto clear = true;
        juce::String detail;
        for (const auto open : { true, false })
        {
            overlay.setTagsFieldOpen (open);
            settle (100);
            const auto chips = overlay.getChipBoxes();
            auto firstTop = 100000;
            for (const auto& [box, tag] : chips)
                firstTop = juce::jmin (firstTop, box.getY());
            const auto fieldBottom = overlay.getTagsField().getBottom();
            detail << (open ? " open: chips from " : " closed: chips from ") << firstTop << ", field ends " << fieldBottom << ";";
            if (open)
                clear = clear && ! chips.empty() && firstTop - fieldBottom >= 8;
        }
        overlay.cancel();
        settle (200);
        expect (clear, "SAVE AS's tag chips sit at least 8 px under the tags field" + detail);
    }

    const auto area = [&editor] (juce::Component* c)
    {
        return c == nullptr ? juce::Rectangle<int>() : editor.getLocalArea (c, c->getLocalBounds());
    };
    const auto shownWaves = [&editor]
    {
        std::vector<WaveDisplay*> all, shown;
        findAll<WaveDisplay> (editor, all);
        for (auto* wave : all)
            if (visibleInTree (wave) && ! wave->getBounds().isEmpty())
                shown.push_back (wave);
        return shown;
    };

    // V10-6: tracking never closes a word gap: the fitter sets letters at most
    // 0.04 of their height closer, at any width.
    {
        auto worst = 0.0f;
        for (const auto* text : { "AMPENV", "Piano Hammer", "KEY RATE", "STRING COUPLING", "SOUNDBOARD (DENSE)" })
            for (float room = 20.0f; room < 260.0f; room += 7.0f)
            {
                const auto font = IlanaTheme::fittedFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body)), text, room, IlanaTheme::TextSize::minPassive);
                worst = juce::jmin (worst, font.getExtraKerningFactor());
            }
        expect (worst >= -0.0401f, "the text fitter tracks at most -0.04 em (" + juce::String (worst, 3) + ")");
    }

    // V10-7: one label size per row. PLAY's OSC 1 row (SEMI, LEVEL, FRAME,
    // UNISON) draws every name at the same size.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (400);
        std::vector<KnobControl*> all;
        findAll<KnobControl> (editor, all);
        std::set<int> sizes;
        auto labels = 0;
        for (auto* knob : all)
            if (visibleInTree (knob) && ! knob->getBounds().isEmpty() && ! knob->isCompact()
                && knob->getParameterId().startsWith ("osc1_") && (knob->getParameterId().endsWith ("_semi") || knob->getParameterId().endsWith ("_level")
                                                                 || knob->getParameterId().endsWith ("_frame") || knob->getParameterId().endsWith ("_unison")))
            {
                auto& label = knob->getNameLabel();
                auto font = IlanaTheme::font (IlanaTheme::TextSize::body);
                const auto fitted = IlanaTheme::fittedFont (juce::Font (font), label.getText().trim(),
                                                            (float) label.getBorderSize().subtractedFrom (label.getLocalBounds()).getWidth(),
                                                            IlanaTheme::TextSize::minPassive);
                const auto cap = label.getProperties().contains ("fitCap") ? (float) label.getProperties()["fitCap"] : 1000.0f;
                sizes.insert (juce::roundToInt (juce::jmin (cap, fitted.getHeight()) * 100.0f));
                ++labels;
            }
        expect (labels == 4 && sizes.size() == 1, "OSC 1's labels on PLAY share one size (" + juce::String (labels) + " labels, " + juce::String ((int) sizes.size()) + " sizes)");
    }

    // V10-2, V10-4: PLAY's strips are capped and their pictures square; the
    // PATCH tile shows on Init at the default size.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (400);
        auto tallest = 0;
        auto squarish = true;
        juce::String shape;
        for (auto* wave : shownWaves())
        {
            const auto bounds = area (wave);
            tallest = juce::jmax (tallest, bounds.getHeight());
            const auto ratio = (float) bounds.getWidth() / (float) juce::jmax (1, bounds.getHeight());
            squarish = squarish && ratio > 0.75f && ratio < 1.34f;
            shape << bounds.getWidth() << "x" << bounds.getHeight() << " ";
        }
        expect (! shownWaves().empty() && tallest <= 150 && squarish, "PLAY's oscillator pictures are square and no taller than 150 px (" + shape + ")");

        loadNamed ("Init");
        editor.showPage ("MAIN");
        settle (400);
        std::vector<SignalFlow*> flows;
        findAll<SignalFlow> (editor, flows);
        auto patchShown = false;
        for (auto* flow : flows)
            patchShown = patchShown || (visibleInTree (flow) && area (flow).getHeight() >= 50);
        expect (patchShown, "the PATCH tile shows on PLAY at the default size (Init)");
    }

    // V10-8: a value keeps its unit's space in a DX7 voice's strips (nothing
    // respelled to fit).
    {
        loadNamed ("E.PIANO 1 (ROM1A)");
        editor.showPage ("MAIN");
        settle (400);
        auto& probe = IlanaTheme::textFitProbe();
        probe = {};
        probe.armed = true;
        editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
        probe.armed = false;
        expect (probe.respelled.isEmpty(), "no value on a DX7 voice's PLAY page drops the space before its unit" + (probe.respelled.isEmpty() ? juce::String() : ": " + probe.respelled.joinIntoString (", ")));
        probe = {};
    }

    // V10-5: an FX card's knobs sit beside its picture in a tight group.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("FX");
        settle (500);
        std::vector<FxDisplay*> displays;
        findAll<FxDisplay> (editor, displays);
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        auto cards = 0, loose = 0;
        juce::String detail;
        for (auto* display : displays)
        {
            if (! visibleInTree (display))
                continue;
            const auto picture = area (display);
            // (The card's own knobs: up to the next card's picture on the row.)
            auto limit = 100000;
            for (auto* other : displays)
                if (other != display && visibleInTree (other) && area (other).getX() > picture.getRight()
                    && area (other).getCentreY() > picture.getY() && area (other).getCentreY() < picture.getBottom())
                    limit = juce::jmin (limit, area (other).getX());
            std::vector<int> xs;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && ! knob->getBounds().isEmpty())
                {
                    const auto bounds = area (knob);
                    if (bounds.getX() >= picture.getRight() - 2 && bounds.getRight() <= limit
                        && bounds.getCentreY() > picture.getY() && bounds.getCentreY() < picture.getBottom())
                        xs.push_back (bounds.getX());
                }
            if (xs.size() < 2)
                continue;
            std::sort (xs.begin(), xs.end());
            ++cards;
            auto widest = 0;
            for (size_t i = 1; i < xs.size(); ++i)
                widest = juce::jmax (widest, xs[i] - xs[i - 1]);
            if (xs.front() - picture.getRight() > 60 || widest > 120)
            {
                ++loose;
                detail << " (first knob " << xs.front() - picture.getRight() << " px from its picture, widest gap " << widest << ")";
            }
        }
        expect (cards > 0 && loose == 0, "FX cards group their knobs beside their picture" + detail);
    }

    // V10-9: SUB + NOISE (OSC) and BODY (PHYSICAL) fold while off, and open on
    // their switch.
    {
        loadNamed ("Felt Hammer Board");
        const auto setParam = [&processor] (const juce::String& id, float plain)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
        };
        setParam ("osc1_mode", 1.0f);
        setParam ("subosc_on", 0.0f);
        setParam ("noise_level", 0.0f);
        setParam ("res_on", 0.0f);
        setParam ("sb_on", 0.0f);
        setParam ("body_coupling_mode", 0.0f);
        const auto shownKnob = [&editor] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                    return true;
            return false;
        };
        editor.showPage ("OSC");
        settle (300);
        {
            // SUB + NOISE (an earlier test may leave VOICE open).
            std::vector<StateTabs*> rows;
            findAll<StateTabs> (editor, rows);
            for (auto* tabs : rows)
                if (tabs->getNumItems() > 1 && tabs->getItem (1).name == "SUB + NOISE" && tabs->onSelect != nullptr)
                    tabs->onSelect (1); // (VOICE is first now: review 11, S11-1)
        }
        settle (300);
        const auto subOff = ! shownKnob ("subosc_level");
        setParam ("subosc_on", 1.0f);
        settle (600);
        const auto subOn = shownKnob ("subosc_level");
        setParam ("subosc_on", 0.0f);
        settle (300);
        editor.showPage ("PHYSICAL");
        settle (500);
        const auto bodyOff = ! shownKnob ("res_amount");
        setParam ("res_on", 1.0f);
        settle (600);
        const auto bodyOn = shownKnob ("res_amount");
        setParam ("res_on", 0.0f);
        settle (300);
        expect (subOff && subOn && bodyOff && bodyOn, "SUB + NOISE and BODY fold while off and open when switched on (sub "
                                                          + juce::String (subOff ? "folds" : "stays") + "/" + juce::String (subOn ? "opens" : "stays shut")
                                                          + ", body " + juce::String (bodyOff ? "folds" : "stays") + "/" + juce::String (bodyOn ? "opens" : "stays shut") + ")");
        editor.showPage ("MAIN");
        settle (200);
    }

    // V10-17: the output meter is 100 px or wider.
    {
        auto* meter = findChild<OutputMeter> (editor);
        const auto bounds = area (meter);
        expect (meter != nullptr && bounds.getWidth() >= 100, "the OUT meter is at least 100 px wide (" + bounds.toString() + ")");
    }
}
