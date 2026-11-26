#pragma once

// header-check: fragment. Included by IlanaLookAndFeel.h after the IlanaTheme helpers it uses (drawFitted, TextSize).
namespace IlanaTheme
{
// A button's words, centred. A button that opens a menu ends its text in
// a down or up caret (U+25BE, U+25B4): it is drawn as the selects' chevron
// after the words instead of the font's triangle (UI polish 2026-11-25).
inline void drawButtonWords (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    const auto down = text.endsWithChar ((juce::juce_wchar) 0x25be), up = text.endsWithChar ((juce::juce_wchar) 0x25b4);
    if (! down && ! up)
    {
        IlanaTheme::drawFitted (g, text, area, juce::Justification::centred, 1, IlanaTheme::TextSize::minInteractive);
        return;
    }
    const auto words = text.dropLastCharacters (1).trimEnd();
    constexpr float chevronW = 7.0f, gap = 5.0f;
    const auto wordsW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), words);
    const auto room = (float) area.getWidth() - chevronW - gap;
    const auto shownW = juce::jmin (wordsW, room);
    const auto x0 = (float) area.getX() + ((float) area.getWidth() - (shownW + gap + chevronW)) * 0.5f;
    IlanaTheme::drawFitted (g, words, juce::Rectangle<float> (x0, (float) area.getY(), shownW + 1.0f, (float) area.getHeight()).toNearestInt(),
                            juce::Justification::centredLeft, 1, IlanaTheme::TextSize::minInteractive);
    const auto cx = x0 + shownW + gap + chevronW * 0.5f, cy = (float) area.getCentreY();
    const auto dy = (up ? -1.0f : 1.0f) * chevronW * 0.25f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - chevronW * 0.5f, cy - dy);
    chevron.lineTo (cx, cy + dy);
    chevron.lineTo (cx + chevronW * 0.5f, cy - dy);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace IlanaTheme
