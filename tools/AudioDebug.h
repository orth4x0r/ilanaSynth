// Dev tool, part of ilanaStress: ILANA_AUDIO_DEBUG=1 ilanaStress renders the
// "buggy audio" cases and reports, for each, the worst discontinuity, NaN/Inf,
// the peak and (for one scenario) the per-block CPU outliers.
//
//   ILANA_AUDIO_DEBUG=1 ilanaStress
//
//   ILANA_AD_STEP=n        every n-th factory preset (default 9), ILANA_AD_FIRST=k offsets it
//   ILANA_AD_ONLY=a|b      presets whose name contains a or b (replaces the step)
//   ILANA_AD_SCEN=a,b,..   scenarios: quiet, carry, note, steal, steal2, wake, swap, ab, cpu, zip, zipfx, zipfx2, extreme, rates
//                          (default: all)
//   ILANA_AD_THREADS=0|1   one core (default) or several voice threads
//
// A click is a sample whose second difference e[i] = x[i] - 2 x[i-1] + x[i-2]
// is far above anything in the 24 ms around it (a jump of size d shows as
// |e| = d; a steady tone's own e is the reference). It counts when |e| is at
// least 3e-4 (-70 dB) and 6 times the local maximum.
//
// Output: one "R,..." CSV row per measurement:
//   R,scenario,preset,engine,finite,peak,jump,ratio,time,near,extra

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <iostream>

#include "PluginProcessor.h"
#include "dsp/Voice.h"

namespace audiodebug
{
struct Mark
{
    double time;
    juce::String name;
};

struct Event
{
    double time = 0.0;
    std::function<void (IlanaSynthAudioProcessor&)> action;
    juce::MidiMessage message;
    bool isMidi = false;
};

inline Event on (double t, int note, int velocity = 100) { return { t, {}, juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), true }; }
inline Event off (double t, int note) { return { t, {}, juce::MidiMessage::noteOff (1, note), true }; }
inline Event act (double t, std::function<void (IlanaSynthAudioProcessor&)> f) { return { t, std::move (f), {}, false }; }

struct Rendered
{
    std::vector<float> left, right;
    std::vector<double> blockMs;
    double rate = 48000.0;
    int block = 256;
};

inline Rendered& lastRendered() { static Rendered r; return r; }

struct Worst
{
    bool finite = true;
    float peak = 0.0f;
    float jump = 0.0f;
    float ratio = 0.0f;
    double time = 0.0;
    int channel = 0;
};

inline void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

inline float getParamValue (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        return parameter->convertFrom0to1 (parameter->getValue());
    return 0.0f;
}

// Renders: the processor is prepared fresh, events are applied at the start of
// the block that holds them (MIDI at the right offset inside it).
inline Rendered render (IlanaSynthAudioProcessor& processor, std::vector<Event> events, double seconds, double rate = 48000.0,
                        int block = 256, bool randomBlocks = false)
{
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    processor.panic();
    processor.releaseResources();
    processor.prepareToPlay (rate, block);

    Rendered out;
    out.rate = rate;
    out.block = block;
    juce::AudioBuffer<float> buffer (2, juce::jmax (block, 1));
    juce::Random random (99);
    size_t next = 0;
    long long done = 0;
    const auto total = (long long) (seconds * rate);

    while (done < total)
    {
        const auto n = (int) juce::jmin ((long long) (randomBlocks ? 1 + random.nextInt (block) : block), total - done);
        const auto start = (double) done / rate, end = (double) (done + n) / rate;
        juce::MidiBuffer midi;
        while (next < events.size() && events[next].time < end)
        {
            const auto& e = events[next++];
            if (e.isMidi)
                midi.addEvent (e.message, juce::jlimit (0, n - 1, (int) ((e.time - start) * rate)));
            else
                e.action (processor);
        }
        buffer.clear();
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, n);
        const auto t0 = juce::Time::getHighResolutionTicks();
        processor.processBlock (view, midi);
        out.blockMs.push_back (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0);
        for (int i = 0; i < n; ++i)
        {
            out.left.push_back (view.getSample (0, i));
            out.right.push_back (view.getSample (1, i));
        }
        done += n;
    }
    return out;
}

// out[i] = max of a[j] for j in [i - window, i - lag] (0 if none).
inline std::vector<float> trailingMax (const std::vector<float>& a, int window, int lag)
{
    std::vector<float> out (a.size(), 0.0f);
    std::deque<int> q;
    for (int i = 0; i < (int) a.size(); ++i)
    {
        const auto add = i - lag;
        if (add >= 0)
        {
            while (! q.empty() && a[(size_t) q.back()] <= a[(size_t) add])
                q.pop_back();
            q.push_back (add);
        }
        while (! q.empty() && q.front() < i - window)
            q.pop_front();
        out[(size_t) i] = q.empty() ? 0.0f : a[(size_t) q.front()];
    }
    return out;
}

inline std::vector<float> leadingMax (const std::vector<float>& a, int window, int lag)
{
    std::vector<float> r (a.rbegin(), a.rend());
    auto m = trailingMax (r, window, lag);
    std::reverse (m.begin(), m.end());
    return m;
}

// The worst click in [from, to] seconds (whole render by default).
inline Worst analyse (const Rendered& r, double from = 0.0, double to = 1.0e9, float floorLevel = 3.0e-4f, float minRatio = 6.0f)
{
    lastRendered() = r;
    Worst w;
    const auto window = (int) (0.024 * r.rate);
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto& x = channel == 0 ? r.left : r.right;
        std::vector<float> e (x.size(), 0.0f);
        for (size_t i = 0; i < x.size(); ++i)
        {
            if (! std::isfinite (x[i]))
            {
                w.finite = false;
                continue;
            }
            w.peak = juce::jmax (w.peak, std::abs (x[i]));
            if (i >= 2 && std::isfinite (x[i - 1]) && std::isfinite (x[i - 2]))
                e[i] = std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]);
        }
        const auto before = trailingMax (e, window, 3);
        const auto after = leadingMax (e, window, 3);
        for (size_t i = 0; i < x.size(); ++i)
        {
            const auto t = (double) i / r.rate;
            if (t < from || t > to || e[i] < floorLevel)
                continue;
            const auto ratio = e[i] / juce::jmax (1.0e-9f, juce::jmax (before[i], after[i]));
            if (ratio >= minRatio && e[i] > w.jump)
            {
                w.jump = e[i];
                w.ratio = ratio;
                w.time = t;
                w.channel = channel;
            }
        }
    }
    return w;
}

// Every place the output drops to exact digital silence (>= 64 zero samples on both channels) and
// the level in the 2 ms before it: a voice freed by release-silence, or the rack going to sleep,
// while still audible. Reports the loudest such stop.
inline Worst cutLevel (const Rendered& r)
{
    Worst w;
    const auto n = (long long) r.left.size();
    long long zeros = 0;
    for (long long i = 0; i < n; ++i)
    {
        const auto zero = r.left[(size_t) i] == 0.0f && r.right[(size_t) i] == 0.0f;
        zeros = zero ? zeros + 1 : 0;
        if (zeros == 64)
        {
            const auto end = i - 63;
            float level = 0.0f;
            for (long long k = juce::jmax (0LL, end - (long long) (0.002 * r.rate)); k < end; ++k)
                level = juce::jmax (level, std::abs (r.left[(size_t) k]), std::abs (r.right[(size_t) k]));
            if (level > w.peak)
            {
                w.peak = level;
                w.time = (double) end / r.rate;
            }
        }
    }
    return w;
}

inline juce::String nearest (const std::vector<Mark>& marks, double t)
{
    juce::String best = "-";
    auto distance = 0.05;
    for (const auto& m : marks)
        if (std::abs (m.time - t) < distance)
        {
            distance = std::abs (m.time - t);
            best = m.name + (t >= m.time ? "+" : "-") + juce::String (std::abs (t - m.time) * 1000.0, 1) + "ms";
        }
    return best;
}

struct Context
{
    IlanaSynthAudioProcessor& processor;
    juce::StringArray names, categories;
    int preset = 0, nextPreset = 0;
    juce::String engine;
    int home = 60;
};

inline void row (const Context& c, const juce::String& scenario, const Worst& w, const std::vector<Mark>& marks, const juce::String& extra = {})
{
    // ILANA_AD_DUMP=folder: the render behind each row as mono-interleaved stereo f32 (rate in the name).
    if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_DUMP", ""); dump.isNotEmpty() && w.jump > 0.0f)
    {
        const auto& r = lastRendered();
        std::vector<float> data;
        for (size_t i = 0; i < r.left.size(); ++i)
        {
            data.push_back (r.left[i]);
            data.push_back (r.right[i]);
        }
        juce::File (dump).getChildFile (juce::File::createLegalFileName (scenario + "__" + c.names[c.preset]) + "_" + juce::String ((int) r.rate) + ".f32")
            .replaceWithData (data.data(), data.size() * sizeof (float));
    }
    std::cout << "R," << scenario << ",\"" << c.names[c.preset].replace ("\"", "'") << "\"," << c.engine << "," << (w.finite ? 1 : 0) << ","
              << juce::String (w.peak, 4) << "," << juce::String (w.jump, 5) << "," << juce::String (w.ratio, 1) << ","
              << juce::String (w.time, 4) << "," << nearest (marks, w.time) << "," << extra << std::endl;
}

inline juce::String engineOf (IlanaSynthAudioProcessor& p)
{
    juce::StringArray found;
    for (int osc = 1; osc <= 6; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);
        if (getParamValue (p, prefix + "_on") < 0.5f)
            continue;
        const auto mode = (int) std::round (getParamValue (p, prefix + "_mode"));
        const char* names[] { "wt", "phys", "sample", "gran", "live", "fm" };
        found.addIfNotAlreadyThere (names[juce::jlimit (0, 5, mode)]);
    }
    return found.isEmpty() ? juce::String ("none") : found.joinIntoString ("+");
}

inline void load (Context& c, int index)
{
    c.processor.loadFactoryPreset (index);
}

// --- scenarios --------------------------------------------------------------

// A preset loaded after another one's last render must start silent (state left over from the
// previous preset shows as output with no note played).
inline void scenarioQuiet (Context& c)
{
    load (c, c.preset);
    const auto r = render (c.processor, {}, 0.5);
    float peak = 0.0f;
    double at = 0.0;
    for (size_t i = 0; i < r.left.size(); ++i)
    {
        const auto v = juce::jmax (std::abs (r.left[i]), std::abs (r.right[i]));
        if (v > peak)
        {
            peak = v;
            at = (double) i / r.rate;
        }
    }
    Worst w;
    w.peak = peak;
    w.time = at;
    w.jump = peak > 1.0e-5f ? peak : 0.0f;
    row (c, "quiet_start", w, {});
    if (w.jump > 0.0f)
    {
        std::cout << "  voices after quiet render: " << c.processor.getRenderingVoiceCount() << std::endl;
        // What differs from the same preset loaded into a fresh processor.
        IlanaSynthAudioProcessor fresh;
        fresh.loadFactoryPreset (c.preset);
        for (auto* parameter : c.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (auto* other = fresh.apvts.getParameter (ranged->getParameterID()))
                    if (std::abs (other->getValue() - ranged->getValue()) > 1.0e-6f)
                        std::cout << "  differs: " << ranged->getParameterID() << " " << ranged->convertFrom0to1 (ranged->getValue())
                                  << " vs fresh " << ranged->convertFrom0to1 (other->getValue()) << std::endl;
    }
}

// Effects left ringing by the previous patch: Init with FX type t in slot 1 plays a note, then this
// preset is loaded (as the stress test does) and nothing is played: any output after the 2 ms
// declick came from state the patch change did not clear.
inline void scenarioCarry (Context& c)
{
    auto* slotParam = dynamic_cast<juce::AudioParameterChoice*> (c.processor.apvts.getParameter ("fx_slot1"));
    const auto numTypes = slotParam != nullptr ? slotParam->choices.size() : 0;
    const auto target = c.preset;
    for (int type = 0; type < numTypes; ++type)
        for (const auto high : { false, true })
        {
            if (type == 0 && high)
                continue;
            load (c, 0);
            for (auto* parameter : c.processor.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                {
                    const auto id = ranged->getParameterID();
                    if (id.startsWith ("fx_") && ! id.startsWith ("fx_slot"))
                    {
                        if (id.endsWith ("_on"))
                            ranged->setValueNotifyingHost (1.0f);
                        else if (high)
                            ranged->setValueNotifyingHost (0.8f);
                    }
                }
            setParam (c.processor, "fx_slot1", (float) type);
            const auto r = render (c.processor, { on (0.1, 60, 120), on (0.15, 67, 120), off (0.5, 60), off (0.5, 67),
                                                  act (0.6, [target] (IlanaSynthAudioProcessor& p) { p.loadFactoryPreset (target); }) },
                                   3.0);
            Rendered after;
            after.rate = r.rate;
            after.block = r.block;
            const auto from = (size_t) (0.625 * r.rate);
            after.left.assign (r.left.begin() + (long) from, r.left.end());
            after.right.assign (r.right.begin() + (long) from, r.right.end());
            float peak = 0.0f;
            double at = 0.0;
            for (size_t i = 0; i < after.left.size(); ++i)
            {
                const auto v = juce::jmax (std::abs (after.left[i]), std::abs (after.right[i]));
                if (v > peak)
                {
                    peak = v;
                    at = 0.625 + (double) i / r.rate;
                }
            }
            Worst w;
            w.peak = peak;
            w.time = at;
            w.jump = peak > 1.0e-5f ? peak : 0.0f;
 if (peak > 1.0e-5f)
                row (c, "carry_fx" + juce::String (type) + (high ? "_hi" : "_def"), w, {});
        }
}

inline void scenarioDbg (Context& c)
{
    const auto quiet = [&c] (const char* what)
    {
        load (c, c.preset);
        const auto r = render (c.processor, {}, 0.5);
        float peak = 0.0f;
        for (size_t i = 0; i < r.left.size(); ++i)
            peak = juce::jmax (peak, std::abs (r.left[i]), std::abs (r.right[i]));
        std::cout << "DBG " << what << " -> quiet peak " << peak << std::endl;
    };
    const auto other = c.names.indexOf ("Octave Hyper Bass");
    for (int drop = 0; drop <= 10; ++drop)
    {
        load (c, other);
        for (int i = 1; i <= 10; ++i)
            setParam (c.processor, "fx_slot" + juce::String (i), i == 1 ? 20.0f : 0.0f);
        render (c.processor, { on (0.1, 60) }, 1.6);
        load (c, c.preset);
        if (drop > 0)
            setParam (c.processor, "fx_slot" + juce::String (drop), 0.0f);
        const auto r = render (c.processor, {}, 0.5);
        float peak = 0.0f;
        for (size_t i = 0; i < r.left.size(); ++i)
            peak = juce::jmax (peak, std::abs (r.left[i]), std::abs (r.right[i]));
        if (drop == 0)
            for (int w = 0; w < 10; ++w)
            {
                float m = 0.0f;
                for (size_t i = (size_t) (w * 2400); i < (size_t) ((w + 1) * 2400) && i < r.left.size(); ++i)
                    m = juce::jmax (m, std::abs (r.left[i]), std::abs (r.right[i]));
                std::cout << "DBG   window " << w << " (50ms) peak " << m << std::endl;
            }
        std::cout << "DBG target slot " << drop << " removed -> " << peak << " (slot type " << getParamValue (c.processor, "fx_slot" + juce::String (juce::jmax (1, drop))) << ")" << std::endl;
    }
}

inline void scenarioNote (Context& c)
{
    load (c, c.preset);
    const auto r = render (c.processor, { on (0.2, c.home), off (1.5, c.home) }, 6.0);
    const std::vector<Mark> marks { { 0.2, "on" }, { 1.5, "off" } };
    row (c, "note", analyse (r), marks);
    row (c, "note_onset", analyse (r, 0.19, 0.25), marks);
    // Where the sound stops: the level just before the last non-zero sample (a voice freed by
    // release-silence, or the rack going to sleep, while still audible).
    row (c, "cut_level", cutLevel (r), marks, "peak=level-2ms-before-a-drop-to-digital-silence");
}

inline void scenarioSteal (Context& c)
{
    load (c, c.preset);
    std::vector<Event> events;
    std::vector<Mark> marks;
    for (int k = 0; k < 40; ++k)
    {
        events.push_back (on (0.2 + k * 0.012, 36 + k, 90));
        events.push_back (off (1.6, 36 + k));
        marks.push_back ({ 0.2 + k * 0.012, "on" + juce::String (k) });
    }
    const auto r = render (c.processor, events, 3.5);
    row (c, "steal_before", analyse (r, 0.19, 0.2 + 31 * 0.012 + 0.03), marks);
    row (c, "steal_after", analyse (r, 0.2 + 32 * 0.012 - 0.005, 1.55), marks);
}

// A voice stolen at the voice limit (here 2): the third note takes the first one's voice while it sounds.
inline void scenarioSteal2 (Context& c)
{
    load (c, c.preset);
    setParam (c.processor, "voice_mode", 0.0f);
    setParam (c.processor, "poly_voices", 2.0f);
    const auto r = render (c.processor, { on (0.2, c.home, 100), on (0.5, c.home + 4, 100), on (0.8, c.home + 7, 100),
                                          off (1.4, c.home), off (1.4, c.home + 4), off (1.4, c.home + 7) }, 3.0);
    const std::vector<Mark> marks { { 0.2, "on1" }, { 0.5, "on2" }, { 0.8, "steal" } };
    row (c, "steal2_second_onset", analyse (r, 0.49, 0.56), marks);
    row (c, "steal2_steal", analyse (r, 0.79, 0.86), marks);
}

inline void scenarioWake (Context& c)
{
    load (c, c.preset);
    const std::vector<Event> events { on (0.1, c.home), off (0.4, c.home), on (6.0, c.home), off (6.3, c.home) };
    const std::vector<Mark> marks { { 6.0, "on2" } };
    const auto asleep = render (c.processor, events, 7.5);
    row (c, "wake_click", analyse (asleep, 2.0, 7.0), marks);
    row (c, "wake_cut_level", cutLevel (asleep), marks, "peak=level-2ms-before-a-drop-to-digital-silence");
}

inline void scenarioSwap (Context& c)
{
    const auto next = c.nextPreset;
    load (c, c.preset);
    const auto held = render (c.processor, { on (0.2, c.home), act (1.0, [next] (IlanaSynthAudioProcessor& p) { p.loadFactoryPreset (next); }),
                                             on (1.3, c.home), off (2.0, c.home) }, 3.0);
    const std::vector<Mark> marks { { 1.0, "swap" }, { 1.3, "on2" } };
    row (c, "swap_held", analyse (held, 0.9, 1.25), marks);
    load (c, c.preset);
    const auto tail = render (c.processor, { on (0.2, c.home), off (0.5, c.home), act (0.8, [next] (IlanaSynthAudioProcessor& p) { p.loadFactoryPreset (next); }) }, 2.0);
    row (c, "swap_tail", analyse (tail, 0.7, 1.0), { { 0.8, "swap" } });
}

inline void scenarioAB (Context& c)
{
    // Like the editor's A/B button: the other patch's parameters are
    // swapped in under a held note, with no patch load.
    load (c, c.nextPreset);
    const auto other = c.processor.apvts.copyState();
    load (c, c.preset);
    const auto r = render (c.processor, { on (0.2, c.home), act (1.0, [other] (IlanaSynthAudioProcessor& p) { p.apvts.replaceState (other); p.easeNextBlock(); }),
                                          off (2.0, c.home) }, 3.5);
    row (c, "ab_switch", analyse (r, 0.95, 1.4), { { 1.0, "ab" } });
}

inline void scenarioCpu (Context& c)
{
    // Realtime mode (tables built in the background), effects kept awake:
    // block times through the sustain and through the decaying tail.
    c.processor.setNonRealtime (false);
    IlanaSynthAudioProcessor::disableFxSleep = true;
    load (c, c.preset);
    const auto r = render (c.processor, { on (0.2, c.home), off (1.5, c.home) }, 9.0, 48000.0, 256);
    c.processor.setNonRealtime (true);
    IlanaSynthAudioProcessor::disableFxSleep = false;
    const auto stats = [&r] (double from, double to)
    {
        std::vector<double> v;
        for (size_t i = 0; i < r.blockMs.size(); ++i)
        {
            const auto t = (double) i * r.block / r.rate;
            if (t >= from && t < to)
                v.push_back (r.blockMs[i]);
        }
        std::sort (v.begin(), v.end());
        return std::array<double, 3> { v.empty() ? 0.0 : v[v.size() / 2], v.empty() ? 0.0 : v[v.size() * 99 / 100], v.empty() ? 0.0 : v.back() };
    };
    const auto sustain = stats (0.5, 1.4), release = stats (1.6, 3.5), tail = stats (5.0, 9.0), all = stats (0.0, 9.0);
    const auto budget = 256.0 / 48.0;
    Worst w;
    w.peak = (float) (all[2] / budget); // peak column: worst block as a share of its real-time budget
    w.jump = (float) (tail[0] / juce::jmax (1.0e-4, sustain[0])); // tail median over sustain median
    w.ratio = (float) (all[2] / juce::jmax (1.0e-4, all[0]));    // worst block over median block
    w.time = all[2];
    row (c, "cpu", w, {},
         "sustainMed=" + juce::String (sustain[0], 3) + "ms;releaseMed=" + juce::String (release[0], 3) + ";tailMed="
             + juce::String (tail[0], 3) + ";tailMax=" + juce::String (tail[2], 3) + ";p99=" + juce::String (all[1], 3)
             + ";maxMs=" + juce::String (all[2], 3) + ";atS=" + juce::String ((double) (std::max_element (r.blockMs.begin(), r.blockMs.end()) - r.blockMs.begin()) * r.block / r.rate, 2));
}

// A knob moved in one step under a held note, block 512.
inline void scenarioZipper (Context& c)
{
    struct Step
    {
        const char* name;
        std::function<void (IlanaSynthAudioProcessor&, bool)> apply;
    };
    const auto fx = [] (int type, const char* on, const char* mix)
    {
        return std::function<void (IlanaSynthAudioProcessor&, bool)> ([type, on, mix] (IlanaSynthAudioProcessor& p, bool second)
        {
            if (! second)
            {
                setParam (p, "fx_slot1", (float) type);
                setParam (p, "fx_slot1_mix", 1.0f);
                setParam (p, on, 1.0f);
            }
            setParam (p, mix, second ? 0.7f : 0.2f);
        });
    };
    using Fn = std::function<void (IlanaSynthAudioProcessor&, bool)>;
    const std::vector<Step> steps {
        { "master", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "master", s ? -6.0f : -26.0f); }) },
        { "osc1_level", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "osc1_level", s ? 1.0f : 0.2f); }) },
        { "osc1_pan", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "osc1_pan", s ? 0.9f : -0.9f); }) },
        { "f1_cutoff", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "f1_cutoff", s ? 6000.0f : 600.0f); }) },
        { "f1_reso", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "f1_reso", s ? 0.9f : 0.1f); }) },
        { "macro1", Fn ([] (IlanaSynthAudioProcessor& p, bool s) { setParam (p, "macro1", s ? 1.0f : 0.0f); }) },
        { "slot_blend", Fn ([] (IlanaSynthAudioProcessor& p, bool s) {
              if (! s) { setParam (p, "fx_slot1", 2.0f); setParam (p, "fx_drive_on", 1.0f); setParam (p, "fx_drive_amount", 8.0f); setParam (p, "fx_drive_mix", 1.0f); }
              setParam (p, "fx_slot1_mix", s ? 1.0f : 0.2f); }) },
        { "drive_mix", fx (2, "fx_drive_on", "fx_drive_mix") },
        { "delay_mix", fx (9, "fx_delay_on", "fx_delay_mix") },
        { "chorus_mix", fx (7, "fx_chorus_on", "fx_chorus_mix") },
        { "reverb_mix", fx (13, "fx_reverb_on", "fx_reverb_mix") },
    };
    const auto zipOnly = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_ZIP", "");
    for (const auto& step : steps)
    {
        if (zipOnly.isNotEmpty() && zipOnly != step.name)
            continue;
        load (c, c.preset);
        step.apply (c.processor, false);
        const auto apply = step.apply;
        const auto r = render (c.processor, { on (0.1, c.home), act (1.0, [apply] (IlanaSynthAudioProcessor& p) { apply (p, true); }) }, 1.6, 48000.0, 512);
        // The step lands at the start of the block holding 1.0 s.
        const auto blockStart = std::floor (1.0 * 48000.0 / 512.0) * 512.0 / 48000.0;
        row (c, juce::String ("zip_") + step.name, analyse (r, blockStart - 0.002, blockStart + 0.03), { { blockStart, "step" } });
    }
}

// The mix knob of every effect moved in one step (block 512) under a held note: which effects ease it.
inline void scenarioZipFx (Context& c)
{
    struct Fx { int type; const char* name; };
    const Fx table[] { { 2, "drive" }, { 3, "crush" }, { 4, "comp" }, { 5, "comb" }, { 6, "phaser" }, { 7, "chorus" }, { 8, "haas" },
                       { 9, "delay" }, { 10, "stutter" }, { 11, "smear" }, { 12, "freeze" }, { 13, "reverb" }, { 14, "flanger" },
                       { 15, "dim" }, { 16, "gate" }, { 17, "tape_stop" }, { 20, "ott" }, { 22, "width" }, { 24, "shifter" },
                       { 25, "ring" }, { 26, "octaver" }, { 27, "vowel" }, { 28, "feedback" }, { 30, "aw" }, { 31, "voc" } };
    for (const auto& fx : table)
    {
        const auto mixId = juce::String ("fx_") + fx.name + "_mix";
        if (c.processor.apvts.getParameter (mixId) == nullptr)
        {
            std::cout << "# no parameter " << mixId << std::endl;
            continue;
        }
        load (c, c.preset);
        setParam (c.processor, "fx_slot1", (float) fx.type);
        setParam (c.processor, "fx_slot1_mix", 1.0f);
        setParam (c.processor, "fx_slot1_bypass", 0.0f);
        if (c.processor.apvts.getParameter (juce::String ("fx_") + fx.name + "_on") != nullptr)
            setParam (c.processor, juce::String ("fx_") + fx.name + "_on", 1.0f);
        setParam (c.processor, mixId, 0.2f);
        const auto r = render (c.processor, { on (0.1, c.home), act (1.0, [mixId] (IlanaSynthAudioProcessor& p) { setParam (p, mixId, 0.8f); }) }, 1.6, 48000.0, 512);
        const auto blockStart = std::floor (1.0 * 48000.0 / 512.0) * 512.0 / 48000.0;
        row (c, juce::String ("zipfx_") + fx.name, analyse (r, blockStart - 0.002, blockStart + 0.03), { { blockStart, "step" } });
    }
}

// Other effect knobs (not MIX) moved a notch in one step (block 512) under a held note.
inline void scenarioZipFx2 (Context& c)
{
    struct Knob { int type; const char* id; const char* onId; };
    const Knob table[] { { 1, "fx_amp_drive", nullptr }, { 1, "fx_amp_level", nullptr }, { 2, "fx_drive_amount", "fx_drive_on" }, { 2, "fx_fold", nullptr },
                         { 3, "fx_crush_bits", "fx_crush_on" }, { 4, "fx_comp_makeup", nullptr }, { 4, "fx_comp_threshold", nullptr },
                         { 5, "fx_comb_freq", "fx_comb_on" }, { 5, "fx_comb_feedback", "fx_comb_on" }, { 8, "fx_haas_delay", nullptr },
                         { 9, "fx_delay_time", "fx_delay_on" }, { 9, "fx_delay_feedback", "fx_delay_on" }, { 13, "fx_reverb_size", "fx_reverb_on" },
                         { 13, "fx_reverb_damping", "fx_reverb_on" }, { 14, "fx_flanger_depth", nullptr }, { 14, "fx_flanger_rate", nullptr },
                         { 18, "fx_tilt", nullptr }, { 18, "fx_tilt_level", nullptr }, { 19, "fx_util_gain", nullptr }, { 21, "fx_limit_ceiling", nullptr },
                         { 22, "fx_width", nullptr }, { 24, "fx_shifter_shift", nullptr }, { 25, "fx_ring_freq", nullptr },
                         { 28, "fx_feedback_amount", nullptr }, { 29, "fx_eq_low_gain", nullptr }, { 29, "fx_eq_mid_gain", nullptr }, { 29, "fx_eq_high_gain", nullptr },
                         { 29, "fx_eq_mid_freq", nullptr } };
    for (const auto& knob : table)
    {
        auto* parameter = c.processor.apvts.getParameter (knob.id);
        if (parameter == nullptr)
        {
            std::cout << "# no parameter " << knob.id << std::endl;
            continue;
        }
        load (c, c.preset);
        setParam (c.processor, "fx_slot1", (float) knob.type);
        setParam (c.processor, "fx_slot1_mix", 1.0f);
        if (knob.onId != nullptr)
            setParam (c.processor, knob.onId, 1.0f);
        parameter->setValueNotifyingHost (0.30f);
        const auto r = render (c.processor, { on (0.1, c.home), act (1.0, [parameter] (IlanaSynthAudioProcessor&) { parameter->setValueNotifyingHost (0.42f); }) }, 1.6, 48000.0, 512);
        const auto blockStart = std::floor (1.0 * 48000.0 / 512.0) * 512.0 / 48000.0;
        row (c, juce::String ("zipfx2_") + knob.id, analyse (r, blockStart - 0.002, blockStart + 0.03), { { blockStart, "step" } });
    }
}

inline void scenarioExtreme (Context& c)
{
    // Self-oscillating filters, extreme FM, every FX at its maximum.
    auto run = [&c] (const juce::String& label, const std::function<void (IlanaSynthAudioProcessor&)>& setup, int note, double seconds = 3.5)
    {
        load (c, c.preset);
        setup (c.processor);
        const auto nonFinite0 = IlanaSynthAudioProcessor::sanitisedNonFinite.load(), clamped0 = IlanaSynthAudioProcessor::sanitisedClamped.load();
        const auto r = render (c.processor, { on (0.1, note, 127), off (1.2, note) }, seconds);
        const auto nonFinite = IlanaSynthAudioProcessor::sanitisedNonFinite.load() - nonFinite0, clamped = IlanaSynthAudioProcessor::sanitisedClamped.load() - clamped0;
        auto w = analyse (r);
        // Level at the end (runaway feedback keeps growing).
        float endPeak = 0.0f;
        for (size_t i = r.left.size() - r.left.size() / 10; i < r.left.size(); ++i)
            endPeak = juce::jmax (endPeak, std::abs (r.left[i]), std::abs (r.right[i]));
        row (c, "extreme_" + label, w, { { 0.1, "on" }, { 1.2, "off" } }, "endPeak=" + juce::String (endPeak, 4) + ";fxNonFinite=" + juce::String (nonFinite) + ";fxClamped=" + juce::String (clamped));
    };
    for (int type = 0; type < FilterType::Count; ++type)
        for (const auto cutoff : { 20.0f, 700.0f, 20000.0f })
            run ("f1type" + juce::String (type) + "_cut" + juce::String ((int) cutoff),
                 [type, cutoff] (IlanaSynthAudioProcessor& p) {
                     setParam (p, "f1_type", (float) type); setParam (p, "f1_reso", 1.0f); setParam (p, "f1_cutoff", cutoff);
                     setParam (p, "f1_drive", 10.0f); setParam (p, "f2_reso", 1.0f); },
                 cutoff < 100.0f ? 36 : 72, 2.5);
    for (int mode = 0; mode < 3; ++mode)
        run ("fm" + juce::String (mode),
             [mode] (IlanaSynthAudioProcessor& p) {
                 setParam (p, "fm_mode", (float) mode);
                 for (auto* parameter : p.getParameters())
                     if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                     {
                         const auto id = ranged->getParameterID();
                         if (id.startsWith ("fm_") && id != "fm_mode" && ! id.contains ("snap") && ! id.contains ("noise_color"))
                             ranged->setValueNotifyingHost (1.0f);
                     } },
             60);
    // Every FX type with all its parameters at the maximum, then the minimum.
    auto* slotParam = dynamic_cast<juce::AudioParameterChoice*> (c.processor.apvts.getParameter ("fx_slot1"));
    const auto numTypes = slotParam != nullptr ? slotParam->choices.size() : 0;
    for (int type = 1; type < numTypes; ++type)
        for (const auto high : { true, false })
            run ("fx" + juce::String (type) + (high ? "_max" : "_min"),
                 [type, high] (IlanaSynthAudioProcessor& p) {
                     for (auto* parameter : p.getParameters())
                         if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                         {
                             const auto id = ranged->getParameterID();
                             if (id.startsWith ("fx_") && ! id.startsWith ("fx_slot"))
                                 ranged->setValueNotifyingHost (high ? 1.0f : 0.0f);
                         }
                     for (int s = 2; s <= IlanaSynthAudioProcessor::numFxSlots; ++s)
                         setParam (p, "fx_slot" + juce::String (s), 0.0f);
                     setParam (p, "fx_slot1", (float) type);
                     setParam (p, "fx_slot1_mix", 1.0f);
                     setParam (p, "fx_slot1_bypass", 0.0f);
                     if (! high)
                         for (auto* parameter : p.getParameters())
                             if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                                 if (ranged->getParameterID().startsWith ("fx_") && ranged->getParameterID().endsWith ("_on"))
                                     ranged->setValueNotifyingHost (1.0f); },
                 60, 6.0);
}

inline void scenarioRates (Context& c)
{
    struct Shape { double rate; int block; bool random; };
    std::vector<Shape> shapes;
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto block : { 1, 32, 64, 512, 2048 })
            shapes.push_back ({ rate, block, false });
    shapes.push_back ({ 48000.0, 777, true });
    shapes.push_back ({ 44100.0, 2048, true });
    for (const auto& shape : shapes)
    {
        if (shape.block == 1 && shape.rate > 50000.0)
            continue; // (2 s at 96 kHz one sample at a time takes minutes)
        load (c, c.preset);
        const auto r = render (c.processor, { on (0.1, c.home), off (0.8, c.home) }, shape.block == 1 ? 1.5 : 3.0, shape.rate, shape.block, shape.random);
        row (c, "rates_" + juce::String ((int) shape.rate) + "_b" + juce::String (shape.block) + (shape.random ? "r" : ""),
             analyse (r), { { 0.1, "on" }, { 0.8, "off" } });
    }
    // prepareToPlay twice with different settings, notes held across the second.
    load (c, c.preset);
    const auto r = render (c.processor, { on (0.1, c.home), act (0.6, [] (IlanaSynthAudioProcessor& p) { p.prepareToPlay (44100.0, 64); }), off (1.0, c.home) }, 2.0, 48000.0, 128);
    row (c, "prepare_twice", analyse (r), { { 0.6, "prepare" } });
}

inline int run()
{
    IlanaSynthAudioProcessor::forceVoiceThreads = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_THREADS", "0").getIntValue();
    IlanaSynthAudioProcessor processor;
    processor.setNonRealtime (true);
    processor.prepareToPlay (48000.0, 256);
    Context c { processor, processor.getFactoryPresetNames(), processor.getFactoryPresetCategories() };

    const auto scenarioText = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_SCEN", "");
    const auto wantedScenarios = juce::StringArray::fromTokens (scenarioText, ",", "");
    const auto wants = [&] (const char* name) { return scenarioText.isEmpty() || wantedScenarios.contains (name); };
    const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_ONLY", "");
    const auto step = juce::jmax (1, juce::SystemStats::getEnvironmentVariable ("ILANA_AD_STEP", "9").getIntValue());
    const auto first = juce::SystemStats::getEnvironmentVariable ("ILANA_AD_FIRST", "0").getIntValue();

    for (int p = 0; p < c.names.size(); ++p)
    {
        auto matches = [&] { for (const auto& part : juce::StringArray::fromTokens (only, "|", "")) if (part.isNotEmpty() && c.names[p].containsIgnoreCase (part)) return true; return false; };
        if (only.isNotEmpty() ? ! matches() : (p + first) % step != 0)
            continue;
        c.preset = p;
        c.nextPreset = (p * 7 + 13) % c.names.size();
        c.home = c.categories[p] == "Bass" ? 36 : 60;
        c.processor.loadFactoryPreset (p);
        c.engine = engineOf (processor);
        std::cout << "# " << p << " " << c.names[p] << " [" << c.categories[p] << "] " << c.engine << std::endl;
        if (wants ("dbg")) scenarioDbg (c);
        if (wants ("quiet")) scenarioQuiet (c);
        if (wants ("carry")) scenarioCarry (c);
        if (wants ("note")) scenarioNote (c);
        if (wants ("steal")) scenarioSteal (c);
        if (wants ("steal2")) scenarioSteal2 (c);
        if (wants ("wake")) scenarioWake (c);
        if (wants ("swap")) scenarioSwap (c);
        if (wants ("ab")) scenarioAB (c);
        if (wants ("cpu")) scenarioCpu (c);
        if (wants ("zip")) scenarioZipper (c);
        if (wants ("zipfx")) scenarioZipFx (c);
        if (wants ("zipfx2")) scenarioZipFx2 (c);
        if (wants ("extreme")) scenarioExtreme (c);
        if (wants ("rates")) scenarioRates (c);
    }
    return 0;
}
} // namespace audiodebug
