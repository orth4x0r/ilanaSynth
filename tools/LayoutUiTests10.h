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
}
