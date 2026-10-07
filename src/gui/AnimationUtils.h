#pragma once

#include <map>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace IlanaAnim
{
// Implemented by components that fade/slide in when they become visible.
struct PageAnimated
{
    virtual ~PageAnimated() = default;
    virtual void replayAppear() = 0;
};

inline void replayPageAppear (juce::Component& component)
{
    for (auto* child : component.getChildren())
    {
        if (auto* animated = dynamic_cast<PageAnimated*> (child))
            animated->replayAppear();

        replayPageAppear (*child);
    }
}

inline float easeOutCubic (float t)
{
    const auto inv = 1.0f - juce::jlimit (0.0f, 1.0f, t);
    return 1.0f - inv * inv * inv;
}

inline float easeInOutCubic (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

inline float easeOutBack (float t)
{
    constexpr auto c1 = 1.70158f;
    constexpr auto c3 = c1 + 1.0f;
    const auto inv = juce::jlimit (0.0f, 1.0f, t) - 1.0f;
    return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
}

inline float approach (float current, float target, float rate)
{
    return current + (target - current) * juce::jlimit (0.0f, 1.0f, rate);
}

// The same easing over `ticks` steps of the rate it was tuned at, so it
// runs at the same speed whatever the frame rate (see FrameTimer).
inline float approach (float current, float target, float rate, float ticks)
{
    return current + (target - current) * (1.0f - std::pow (1.0f - juce::jlimit (0.0f, 1.0f, rate), ticks));
}

// value * perTick, `ticks` times.
inline float decay (float value, float perTick, float ticks)
{
    return value * std::pow (perTick, ticks);
}

// Lets a view skip repainting while nothing it shows has changed: check()
// is true when the signature (usually the processor's getUiEpoch()) moved,
// and every two seconds regardless, for anything the signature misses.
class ChangeGate
{
public:
    bool check (juce::uint64 signature)
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (signature == last && now - lastPass < 2000.0)
            return false;

        last = signature;
        lastPass = now;
        return true;
    }

private:
    juce::uint64 last = ~(juce::uint64) 0;
    double lastPass = 0.0;
};

// The mouse as far as a view can draw it (hover, drags): changes when the
// pointer moves over the view, a button goes down or up, or it leaves; 0
// while it's elsewhere. Mixed into a ChangeGate signature, a resting
// pointer costs nothing.
inline juce::uint64 mouseSignature (const juce::Component& component)
{
    if (! component.isMouseOver (true))
        return 0;

    const auto position = component.getMouseXYRelative();
    const auto down = juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
    return 0x9e3779b97f4a7c15ull ^ ((juce::uint64) (juce::uint32) position.x << 21)
         ^ ((juce::uint64) (juce::uint32) position.y << 1) ^ (down ? 1ull : 0ull);
}

// A phase (0..1) as a signature term that changes every 1/512 of a cycle,
// about the smallest move a dot on a display can show.
inline juce::uint64 phaseSignature (float phase, int salt)
{
    return (juce::uint64) (juce::uint32) juce::roundToInt (phase * 512.0f) * (0x100000001b3ull + (juce::uint64) salt * 2654435761ull);
}

// Diagnostics: paint() calls per component kind, counted only while a tool
// (ilanaSnapshot --fps) has switched it on. One branch when off.
inline bool& paintStatsOn() { static bool on = false; return on; }
inline std::map<std::string, int>& paintStats() { static std::map<std::string, int> counts; return counts; }
inline void countPaint (const char* name)
{
    if (paintStatsOn())
        ++paintStats()[name];
}

// The audio thread publishes a display value once per block (a host buffer
// of 1024 samples is 43 times a second, 2048 is 21). A picture that jumps to
// each new value moves at that rate however often it is repainted. This
// glides from the last shown value to each new one over the time between
// the last two publications (one block of delay at most), so motion follows
// the display's refresh. `circular` values (a phase 0..1) take the short way
// round the wrap; `snapBack` is how far a value may fall before it is taken
// as a restart (an envelope's new note) and jumped to; a negative published
// value (nothing playing) is always jumped to.
class BlockSmoother
{
public:
    float get (float published, bool circular = false, float snapBack = 1.0e9f) const
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (published != target)
        {
            const auto measured = juce::jlimit (2.0, 60.0, now - changeMs);
            interval = changeMs > 0.0 ? 0.6 * interval + 0.4 * measured : measured;
            changeMs = now;
            from = shown;
            auto delta = published - from;

            if (circular)
            {
                if (delta < -0.5f) delta += 1.0f;
                else if (delta > 0.5f) delta -= 1.0f;
            }
            else if (published < 0.0f || from < 0.0f || delta < -snapBack)
            {
                from = published;
                delta = 0.0f;
            }

            target = published;
            span = delta;
        }

        const auto t = (float) juce::jlimit (0.0, 1.0, (now - changeMs) / interval);
        shown = from + span * t;

        if (circular)
            shown -= std::floor (shown);

        return shown;
    }

private:
    mutable float target = -1.0e9f, from = 0.0f, span = 0.0f, shown = 0.0f;
    mutable double changeMs = 0.0, interval = 12.0;
};

class FrameTimer;

// Every animation runs off one clock ticked by the display's refresh
// (vblank) of any open editor: 60 fps or more, in step with what is drawn
// (the idea Vital's renderer is built on). With no editor on screen
// (snapshots, tests) or a host that sends no vblank, a 60 Hz timer stands in.
class FrameClock : private juce::Timer,
                   private juce::DeletedAtShutdown
{
public:
    FrameClock() = default;
    static FrameClock& get() { return *getInstance(); }

    ~FrameClock() override { clearSingletonInstance(); }

    JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE (FrameClock, false)

    // An open editor holds one of these; its display's refresh drives the clock.
    struct Source
    {
        explicit Source (juce::Component& editorIn)
            : editor (editorIn), vblank (&editorIn, [] { FrameClock::get().tick (true); })
        {
            FrameClock::get().sources.push_back (this);
        }

        ~Source()
        {
            if (auto* clock = FrameClock::getInstanceWithoutCreating())
                clock->sources.erase (std::remove (clock->sources.begin(), clock->sources.end(), this), clock->sources.end());
        }

        juce::Component& editor;
        juce::VBlankAttachment vblank;
    };

    void add (FrameTimer* timer)
    {
        if (std::find (timers.begin(), timers.end(), timer) == timers.end())
            timers.push_back (timer);

        if (! isTimerRunning())
            startTimerHz (10);
    }

    void remove (FrameTimer* timer)
    {
        const auto it = std::find (timers.begin(), timers.end(), timer);

        if (it == timers.end())
            return;

        // Removed during a tick: blank the slot, compact afterwards.
        if (ticking)
            *it = nullptr;
        else
            timers.erase (it);
    }

    // Seconds since the previous frame.
    float frameSeconds() const { return dt; }

private:
    void tick (bool fromVBlank);

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto starved = now - lastVBlank > 100.0;

        // Stand in for vblank only where frames can be seen: an editor on
        // screen, or editors with no window at all (snapshots and tests). A
        // host that hides the editor's window without closing it gets no
        // animation work.
        const auto anyShowing = std::all_of (sources.begin(), sources.end(), [] (Source* s) { return s->editor.getPeer() == nullptr; })
                             || std::any_of (sources.begin(), sources.end(), [] (Source* s) { return s->editor.isShowing(); });

        if (starved && anyShowing)
        {
            if (getTimerInterval() > 20)
                startTimerHz (60);
            tick (false);
        }
        else if (getTimerInterval() < 100)
        {
            startTimerHz (10);
        }

        if (timers.empty())
            stopTimer();
    }

    std::vector<FrameTimer*> timers;
    std::vector<Source*> sources;
    bool ticking = false;
    double lastFrame = 0.0, lastVBlank = 0.0;
    float dt = 1.0f / 60.0f;
};

// Like juce::Timer but called on each frame of the FrameClock. The rate
// given to startTimerHz is only the reference that per-tick constants were
// tuned at: frameTicks() is how many of those ticks this frame stands for.
class FrameTimer
{
public:
    virtual ~FrameTimer() { stopTimer(); }
    virtual void timerCallback() = 0;

    void startTimerHz (int referenceHz)
    {
        reference = (float) juce::jmax (1, referenceHz);
        polling = false;
        running = true;
        FrameClock::get().add (this);
    }

    // For timers that only watch for changes: called at most `hz` times a
    // second, still in step with the frames.
    void startPollingHz (int hz)
    {
        startTimerHz (hz);
        polling = true;
    }

    void startTimer (int milliseconds) { startTimerHz (juce::jmax (1, 1000 / juce::jmax (1, milliseconds))); }

    void stopTimer()
    {
        if (running)
        {
            running = false;

            if (auto* clock = FrameClock::getInstanceWithoutCreating())
                clock->remove (this);
        }
    }

    bool isTimerRunning() const { return running; }
    int getTimerInterval() const { return running ? juce::roundToInt (1000.0f / reference) : 0; }
    // Seconds since this timer's last call, and that in ticks of its reference rate.
    float frameSeconds() const { return seconds; }
    float frameTicks() const { return seconds * reference; }

private:
    friend class FrameClock;

    void frame (float dt)
    {
        if (polling)
        {
            waited += dt;

            if (waited < 1.0f / reference)
                return;

            seconds = juce::jmin (waited, 0.25f);
            waited = 0.0f;
        }
        else
        {
            seconds = dt;
        }

        timerCallback();
    }

    float reference = 60.0f, seconds = 1.0f / 60.0f, waited = 0.0f;
    bool running = false, polling = false;
};

inline void FrameClock::tick (bool fromVBlank)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (fromVBlank)
        lastVBlank = now;

    // Two editors (or a 240 Hz display) share the clock: at most ~200 frames a second.
    if (ticking || (lastFrame > 0.0 && now - lastFrame < 5.0))
        return;

    dt = lastFrame > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastFrame) * 0.001) : 1.0f / 60.0f;
    lastFrame = now;

    ticking = true;
    const auto count = timers.size();

    for (size_t i = 0; i < count && i < timers.size(); ++i)
        if (auto* timer = timers[i])
            timer->frame (dt);

    ticking = false;
    timers.erase (std::remove (timers.begin(), timers.end(), nullptr), timers.end());
}

// Downscale + separable box blur + upscale, used for frosted-glass overlays.
inline juce::Image blurredSnapshot (juce::Component& source, float radius, float scale = 0.16f)
{
    auto snapshot = source.createComponentSnapshot (source.getLocalBounds(), false);

    if (! snapshot.isValid())
        return {};

    const auto smallW = juce::jmax (2, (int) ((float) snapshot.getWidth() * scale));
    const auto smallH = juce::jmax (2, (int) ((float) snapshot.getHeight() * scale));
    auto shrunk = snapshot.rescaled (smallW, smallH, juce::Graphics::mediumResamplingQuality);

    {
        juce::Image::BitmapData data (shrunk, juce::Image::BitmapData::readWrite);
        const auto passes = juce::jlimit (1, 3, (int) std::round (radius));

        for (int pass = 0; pass < passes; ++pass)
        {
            const auto horizontal = (pass % 2) == 0;

            for (int y = 0; y < smallH; ++y)
            {
                for (int x = 0; x < smallW; ++x)
                {
                    auto r = 0.0f;
                    auto g = 0.0f;
                    auto b = 0.0f;
                    auto count = 0.0f;

                    for (int k = -2; k <= 2; ++k)
                    {
                        const auto sx = horizontal ? x + k : x;
                        const auto sy = horizontal ? y : y + k;

                        if (sx < 0 || sx >= smallW || sy < 0 || sy >= smallH)
                            continue;

                        const auto* pixel = data.getPixelPointer (sx, sy);
                        r += (float) pixel[0];
                        g += (float) pixel[1];
                        b += (float) pixel[2];
                        count += 1.0f;
                    }

                    auto* pixel = data.getPixelPointer (x, y);
                    pixel[0] = (juce::uint8) juce::jlimit (0.0f, 255.0f, r / count);
                    pixel[1] = (juce::uint8) juce::jlimit (0.0f, 255.0f, g / count);
                    pixel[2] = (juce::uint8) juce::jlimit (0.0f, 255.0f, b / count);
                }
            }
        }
    }

    return shrunk.rescaled (snapshot.getWidth(), snapshot.getHeight(), juce::Graphics::mediumResamplingQuality);
}
} // namespace IlanaAnim
