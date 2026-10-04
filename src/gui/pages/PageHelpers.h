// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
#if defined (_WIN32)
// GetDpiForMonitor ignores the caller's DPI awareness (unlike the DPI APIs
// JUCE's peer uses), so it still reports the real monitor scale inside hosts
// that virtualise plugin windows.  Loaded dynamically to avoid extra link
// dependencies.
float queryMonitorScale (void* nativeHandle)
{
    if (nativeHandle == nullptr)
        return 1.0f;

    using MonitorFromWindowFn = void* (*) (void*, unsigned long);
    using GetDpiForMonitorFn = long (*) (void*, int, unsigned int*, unsigned int*);

    auto* user32 = GetModuleHandleW (L"user32.dll");
    auto* shcore = LoadLibraryW (L"shcore.dll");

    if (user32 == nullptr || shcore == nullptr)
        return 1.0f;

    auto* monitorFromWindow = (MonitorFromWindowFn) (void*) GetProcAddress (user32, "MonitorFromWindow");
    auto* getDpiForMonitor = (GetDpiForMonitorFn) (void*) GetProcAddress (shcore, "GetDpiForMonitor");

    if (monitorFromWindow == nullptr || getDpiForMonitor == nullptr)
        return 1.0f;

    auto* monitor = monitorFromWindow (nativeHandle, 2 /*MONITOR_DEFAULTTONEAREST*/);
    unsigned int dpiX = 96;
    unsigned int dpiY = 96;

    if (monitor == nullptr || getDpiForMonitor (monitor, 0 /*MDT_EFFECTIVE_DPI*/, &dpiX, &dpiY) != 0 /*S_OK*/)
        return 1.0f;

    return juce::jlimit (1.0f, 3.0f, (float) dpiX / 96.0f);
}

void* setThreadDpiContext (void* context)
{
    using SetThreadDpiAwarenessContextFn = void* (*) (void*);

    static auto fn = (SetThreadDpiAwarenessContextFn) (void*)
        GetProcAddress (GetModuleHandleW (L"user32.dll"), "SetThreadDpiAwarenessContext");

    return fn != nullptr ? fn (context) : nullptr;
}

void* perMonitorAwareV2Context() { return (void*) (juce::pointer_sized_int) -4; }

int processDpiAwareness()
{
    using GetProcessDpiAwarenessFn = long (*) (void*, int*);

    static auto* shcore = LoadLibraryW (L"shcore.dll");
    static auto getProcessAwareness = shcore != nullptr
                                          ? (GetProcessDpiAwarenessFn) (void*) GetProcAddress (shcore, "GetProcessDpiAwareness")
                                          : nullptr;

    if (getProcessAwareness == nullptr)
        return -1;

    auto* process = OpenProcess (0x1000 /*PROCESS_QUERY_LIMITED_INFORMATION*/, 0, GetCurrentProcessId());
    int awareness = -1;

    if (process != nullptr)
    {
        if (getProcessAwareness (process, &awareness) != 0)
            awareness = -1;

        CloseHandle (process);
    }

    return awareness;
}
#endif

template <typename... Components>
void addAll (juce::Component& parent, Components&... components)
{
    (parent.addAndMakeVisible (components), ...);
}

// A page section's heading (a display, a pool, the rack): the title in the
// label colour with a short rule under it, and no dot (review 6: a dot
// reads as a power light, and on cards it means a module's colour beside a
// real switch). The text starts where a card's tag does, so headings line
// up with the cards under them.
void paintSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                        const juce::String& subtitle = {})
{
    const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
    const auto width = juce::GlyphArrangement::getStringWidthInt (font, text);
    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (font);
    g.drawText (text, area, juce::Justification::centredLeft);
    g.setColour (IlanaTheme::Ui::line.brighter (0.5f));
    g.fillRect (juce::Rectangle<float> ((float) area.getX(), (float) area.getCentreY() + 8.0f, (float) juce::jmin (width, 18), 1.5f));

    if (subtitle.isEmpty())
        return;

    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
    g.drawText (subtitle, area.withTrimmedLeft (width + 16), juce::Justification::centredLeft, true);
}

// Page headings sit where a card's title does: 12 px in from the card edge
// (cards start 12 px in from the page), 26 px tall like a card's header.
constexpr int headingX = 24, headingHeight = 26;
} // namespace
