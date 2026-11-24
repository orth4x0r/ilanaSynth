#pragma once

#include <juce_core/juce_core.h>

// Hostile-file guard for every XML the synth reads from disk or from a host
// (presets, session state, FX chains, table sidecars, sample options). JUCE's
// XmlDocument and ValueTree::fromXml recurse once per nesting level with no
// limit, so a 200 KB file of nested tags overflowed the stack, and a preset
// merely sitting in the folder crashed the browser's listing. Real files are
// under ten levels deep and a few MB at most, except a patch with an embedded
// sample: up to 64 MB of raw audio (maxEmbeddedSampleBytes), stored gzip +
// base64, which grows to about 85 MB. The depth scan is the real guard; the
// size cap only refuses absurd files, so it sits well above that (Fable P1/B2:
// at 64 MB a Live set or preset with a long embedded sample would not load).
namespace SafeXml
{
static constexpr juce::int64 maxBytes = 256 * 1024 * 1024;
static constexpr int maxDepth = 256;

// True when no element in the text nests deeper than maxDepth. A light scan:
// comments, CDATA, declarations and quoted attribute values are skipped.
inline bool depthIsSafe (const juce::String& text)
{
    auto p = text.getCharPointer();
    int depth = 0;
    while (! p.isEmpty())
    {
        if (*p != '<')
        {
            ++p;
            continue;
        }
        ++p;
        if (p.compareUpTo (juce::CharPointer_ASCII ("!--"), 3) == 0)
        {
            const auto end = p.indexOf (juce::CharPointer_ASCII ("-->"));
            if (end < 0)
                return true; // unterminated: the parser refuses it
            p += end + 3;
            continue;
        }
        if (*p == '?' || *p == '!')
        {
            // A declaration or CDATA: skip to its end.
            const auto cdata = p.compareUpTo (juce::CharPointer_ASCII ("![CDATA["), 8) == 0;
            const auto end = p.indexOf (juce::CharPointer_ASCII (cdata ? "]]>" : ">"));
            if (end < 0)
                return true;
            p += end + 1;
            continue;
        }
        const auto closing = *p == '/';
        juce::juce_wchar quote = 0, last = 0;
        while (! p.isEmpty())
        {
            const auto c = p.getAndAdvance();
            if (quote != 0)
            {
                if (c == quote)
                    quote = 0;
                continue;
            }
            if (c == '"' || c == '\'')
                quote = c;
            else if (c == '>')
                break;
            last = c;
        }
        if (closing)
            --depth;
        else if (last != '/' && ++depth > maxDepth)
            return false;
    }
    return true;
}

inline std::unique_ptr<juce::XmlElement> parse (const juce::String& text)
{
    if (text.getNumBytesAsUTF8() > (size_t) maxBytes || ! depthIsSafe (text))
        return nullptr;
    return juce::parseXML (text);
}

inline std::unique_ptr<juce::XmlElement> parse (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() > maxBytes)
        return nullptr;
    return parse (file.loadFileAsString());
}
} // namespace SafeXml
