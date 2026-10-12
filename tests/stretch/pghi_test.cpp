// Standalone test for OfflineStretch::renderPghi / resampleTo.
// Build (from the repo root):
//   gcc -O2 -c /tmp/libs/jp/pffft.c -o /tmp/pffft.o
//   gcc -O2 -c /tmp/libs/jp/fftpack.c -o /tmp/fftpack.o
//   g++ -O2 -std=c++20 -I/tmp/libs/jp -Isrc/dsp/stretch tests/stretch/pghi_test.cpp
//       src/dsp/stretch/StretchPghi.cpp src/dsp/stretch/StretchResample.cpp /tmp/pffft.o /tmp/fftpack.o -o /tmp/pghi_test
#include "OfflineStretch.h"
#include "pffft.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace OfflineStretch;
static const double PI = 3.14159265358979323846;
static int failures = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++failures; }

static Audio makeMono (const std::vector<float>& v, double sr) { Audio a; a.sampleRate = sr; a.channels = { v }; return a; }

static std::vector<float> sine (double f, double sr, int n, double amp = 0.5)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2 * PI * f * i / sr));
    return v;
}

// Frequency from positive-going zero crossings between sample a and b
static double zcFreq (const std::vector<float>& x, double sr, int a, int b)
{
    double first = -1, last = -1; int cnt = 0;
    for (int i = a + 1; i < b; ++i)
        if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0)
        {
            double t = i - 1 + (-x[(size_t) i - 1]) / (double) (x[(size_t) i] - x[(size_t) i - 1]);
            if (cnt == 0) first = t;
            last = t; ++cnt;
        }
    return cnt < 2 ? 0 : (cnt - 1) * sr / (last - first);
}
static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }
static double rms (const std::vector<float>& x, int a, int b)
{
    double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, b - a));
}
static double peakOf (const Audio& a) { double p = 0; for (auto& c : a.channels) for (float v : c) p = std::max (p, (double) std::fabs (v)); return p; }
static bool allFinite (const Audio& a) { for (auto& c : a.channels) for (float v : c) if (! std::isfinite (v)) return false; return true; }

// magnitude spectrum (Hann) of x[start, start+N), N power of 2
static std::vector<float> magSpec (const std::vector<float>& x, int start, int N)
{
    PFFFT_Setup* s = pffft_new_setup (N, PFFFT_REAL);
    float* in = (float*) pffft_aligned_malloc (N * sizeof (float));
    float* out = (float*) pffft_aligned_malloc (N * sizeof (float));
    float* wk = (float*) pffft_aligned_malloc (N * sizeof (float));
    for (int i = 0; i < N; ++i) in[i] = x[(size_t) (start + i)] * (float) (0.5 - 0.5 * std::cos (2 * PI * i / N));
    pffft_transform_ordered (s, in, out, wk, PFFFT_FORWARD);
    std::vector<float> m ((size_t) N / 2);
    m[0] = std::fabs (out[0]);
    for (int k = 1; k < N / 2; ++k) m[(size_t) k] = std::hypot (out[2 * k], out[2 * k + 1]);
    pffft_aligned_free (in); pffft_aligned_free (out); pffft_aligned_free (wk); pffft_destroy_setup (s);
    return m;
}
// strongest local maximum near f (+- 3 Hz), parabolic interpolation -> (freq, mag)
static void peakNear (const std::vector<float>& m, int N, double sr, double f, double& fo, double& mo)
{
    int k0 = (int) std::lround (f * N / sr), best = k0;
    for (int k = std::max (1, k0 - 3); k <= k0 + 3; ++k) if (m[(size_t) k] > m[(size_t) best]) best = k;
    double a = std::log (m[(size_t) best - 1] + 1e-20), b = std::log (m[(size_t) best] + 1e-20), c = std::log (m[(size_t) best + 1] + 1e-20);
    double d = 0.5 * (a - c) / (a - 2 * b + c);
    fo = (best + d) * sr / N; mo = m[(size_t) best];
}

static void writeWav (const std::string& path, const Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    int n = a.numFrames(), ch = (int) a.channels.size(), sr = (int) a.sampleRate;
    uint32_t dataBytes = (uint32_t) (n * ch * 2), riff = 36 + dataBytes, fmtLen = 16, rate = (uint32_t) sr, byteRate = rate * ch * 2;
    uint16_t pcm = 1, chs = (uint16_t) ch, align = (uint16_t) (ch * 2), bits = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&chs, 2, 1, f); std::fwrite (&rate, 4, 1, f);
    std::fwrite (&byteRate, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
        {
            float v = std::max (-1.0f, std::min (1.0f, a.channels[(size_t) c][(size_t) i]));
            int16_t s = (int16_t) std::lround (v * 32767.0f);
            std::fwrite (&s, 2, 1, f);
        }
    std::fclose (f);
}

static uint32_t rngState = 12345;
static float noise() { rngState = rngState * 1664525u + 1013904223u; return (float) ((rngState >> 8) / 8388608.0 - 1.0); }

// 2 bars at 120 BPM: kick, snare (noise), hats, saw bass; slight stereo spread
static Audio drumLoop (double sr)
{
    const double beat = 0.5; const int n = (int) (sr * beat * 8);
    Audio a; a.sampleRate = sr; a.channels.assign (2, std::vector<float> ((size_t) n, 0.0f));
    auto add = [&] (double t, int ch, int i, float v) { (void) t; a.channels[(size_t) ch][(size_t) i] += v; };
    for (int b = 0; b < 8; ++b)
    {
        int s0 = (int) (b * beat * sr);
        if (b % 2 == 0) // kick
            for (int i = 0; i < (int) (0.3 * sr) && s0 + i < n; ++i)
            { double t = i / sr; double ph = 2 * PI * (50 * t + 90 * (1 - std::exp (-t * 30)) / 30); float v = (float) (0.8 * std::sin (ph) * std::exp (-t * 12)); add (t, 0, s0 + i, v); add (t, 1, s0 + i, v); }
        if (b % 2 == 1) // snare
            for (int i = 0; i < (int) (0.25 * sr) && s0 + i < n; ++i)
            { double t = i / sr; float v = (float) (0.5 * noise() * std::exp (-t * 20) + 0.3 * std::sin (2 * PI * 190 * t) * std::exp (-t * 25)); add (t, 0, s0 + i, v * 0.9f); add (t, 1, s0 + i, v * 1.1f); }
        for (int h = 0; h < 2; ++h) // hats on 8ths
        {
            int s1 = s0 + (int) (h * beat * 0.5 * sr);
            for (int i = 0; i < (int) (0.05 * sr) && s1 + i < n; ++i)
            { double t = i / sr; float v = (float) (0.15 * noise() * std::exp (-t * 90)); add (t, 0, s1 + i, v * 1.2f); add (t, 1, s1 + i, v * 0.8f); }
        }
        // bass: saw, notes A1 A1 C2 G1
        static const double bf[4] = { 55.0, 55.0, 65.41, 49.0 };
        double f = bf[(b / 2) % 4];
        for (int i = 0; i < (int) (beat * sr * 0.9) && s0 + i < n; ++i)
        { double t = i / sr; double ph = std::fmod (f * t, 1.0); float v = (float) (0.3 * (2 * ph - 1) * std::exp (-t * 2.5) * std::min (1.0, t * 200)); add (t, 0, s0 + i, v); add (t, 1, s0 + i, v); }
    }
    return a;
}
static Audio chord (double sr, double secs)
{
    const double fs[4] = { 220.0, 277.18, 329.63, 440.0 }; // A major-ish stack
    int n = (int) (sr * secs); Audio a; a.sampleRate = sr; a.channels.assign (2, std::vector<float> ((size_t) n, 0.0f));
    for (int i = 0; i < n; ++i)
    {
        double t = i / sr, env = std::min (1.0, t * 20) * std::min (1.0, (secs - t) * 20);
        for (int k = 0; k < 4; ++k)
            for (int h = 1; h <= 3; ++h)
            { float v = (float) (0.12 * env * std::sin (2 * PI * fs[k] * h * t + k) / h); a.channels[0][(size_t) i] += v * (k % 2 ? 1.2f : 0.8f); a.channels[1][(size_t) i] += v * (k % 2 ? 0.8f : 1.2f); }
    }
    return a;
}

int main()
{
    const double sr = 48000.0;
    std::printf ("(a) sine 440 Hz\n");
    for (double lf : { 1.5, 0.5 })
    {
        Audio in = makeMono (sine (440, sr, (int) (3 * sr)), sr);
        Audio out = renderPghi (in, lf, 1.0);
        int expect = (int) std::lround (in.numFrames() * lf);
        char buf[160];
        std::snprintf (buf, sizeof buf, "length exact (%d == %d) lf=%.2f", out.numFrames(), expect, lf);
        check (out.numFrames() == expect && allFinite (out), buf);
        double f = zcFreq (out.channels[0], sr, out.numFrames() / 4, out.numFrames() * 3 / 4);
        std::snprintf (buf, sizeof buf, "freq %.3f Hz, %.2f cents off lf=%.2f", f, cents (f, 440), lf);
        check (std::fabs (cents (f, 440)) < 3.0, buf);
    }
    std::printf ("(b) pitch 1.5, length 1\n");
    {
        Audio in = makeMono (sine (440, sr, (int) (3 * sr)), sr);
        Audio out = renderPghi (in, 1.0, 1.5);
        double f = zcFreq (out.channels[0], sr, out.numFrames() / 4, out.numFrames() * 3 / 4);
        char buf[160]; std::snprintf (buf, sizeof buf, "freq %.3f Hz (%.2f cents), len %d vs %d", f, cents (f, 660), out.numFrames(), in.numFrames());
        check (std::fabs (cents (f, 660)) < 3.0 && out.numFrames() == in.numFrames(), buf);
        Audio out2 = renderPghi (in, 2.0, 0.75);
        f = zcFreq (out2.channels[0], sr, out2.numFrames() / 4, out2.numFrames() * 3 / 4);
        std::snprintf (buf, sizeof buf, "pitch 0.75 lf 2: %.3f Hz (%.2f cents), len %d", f, cents (f, 330), out2.numFrames());
        check (std::fabs (cents (f, 330)) < 3.0 && out2.numFrames() == 288000, buf);
    }
    std::printf ("(c) chord of 3 sines, stereo, lf 1.7\n");
    {
        const double fs[3] = { 261.63, 329.63, 392.0 };
        const double am[3] = { 0.3, 0.2, 0.25 };
        int n = (int) (4 * sr); std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) { double v = 0; for (int k = 0; k < 3; ++k) v += am[k] * std::sin (2 * PI * fs[k] * i / sr + k); l[(size_t) i] = (float) v; r[(size_t) i] = (float) (0.7 * v); }
        Audio in; in.sampleRate = sr; in.channels = { l, r };
        Audio out = renderPghi (in, 1.7, 1.0);
        const int N = 32768; auto mi = magSpec (in.channels[0], 20000, N); auto mo = magSpec (out.channels[0], out.numFrames() / 2 - N / 2, N);
        for (int k = 0; k < 3; ++k)
        {
            double fi, ai, fo, ao; peakNear (mi, N, sr, fs[k], fi, ai); peakNear (mo, N, sr, fs[k], fo, ao);
            char buf[200]; std::snprintf (buf, sizeof buf, "peak %.2f Hz -> %.2f Hz, level change %.2f dB", fs[k], fo, 20 * std::log10 (ao / ai));
            check (std::fabs (cents (fo, fs[k])) < 3.0 && std::fabs (20 * std::log10 (ao / ai)) < 1.5, buf);
        }
        // spurious peaks: strongest bin outside +-30 Hz of the three
        double worst = 0, top = 0; for (float v : mo) top = std::max (top, (double) v);
        for (int k = 5; k < N / 2; ++k) { double f = k * sr / N; bool near = false; for (double c : fs) if (std::fabs (f - c) < 30) near = true; if (! near) worst = std::max (worst, (double) mo[(size_t) k]); }
        char buf[120]; std::snprintf (buf, sizeof buf, "off-peak floor %.1f dB below the peaks", 20 * std::log10 (top / worst));
        check (20 * std::log10 (top / worst) > 40, buf);
        // stereo image kept: level ratio R/L
        double ratio = rms (out.channels[1], n / 2, n) / rms (out.channels[0], n / 2, n);
        char b2[120]; std::snprintf (b2, sizeof b2, "stereo R/L ratio %.3f (input 0.700)", ratio);
        check (std::fabs (ratio - 0.7) < 0.03, b2);
    }
    std::printf ("(d) RMS level on steady part\n");
    {
        Audio in = chord (sr, 4.0);
        for (double lf : { 0.6, 1.25, 2.0, 4.0 })
        {
            Audio out = renderPghi (in, lf, 1.0);
            int a = (int) (out.numFrames() * 0.3), b = (int) (out.numFrames() * 0.7);
            int a0 = (int) (in.numFrames() * 0.3), b0 = (int) (in.numFrames() * 0.7);
            double d = 20 * std::log10 (rms (out.channels[0], a, b) / rms (in.channels[0], a0, b0));
            char buf[120]; std::snprintf (buf, sizeof buf, "chord lf=%.2f RMS diff %.2f dB, peak out/in %.2f", lf, d, peakOf (out) / peakOf (in));
            check (std::fabs (d) < 1.5, buf);
        }
        Audio loop = drumLoop (sr);
        Audio out = renderPghi (loop, 1.25, 1.0);
        double d = 20 * std::log10 ((rms (out.channels[0], 0, out.numFrames()) + 1e-9) / rms (loop.channels[0], 0, loop.numFrames()));
        char buf[160]; std::snprintf (buf, sizeof buf, "drum loop whole-file RMS diff %.2f dB, peak out/in %.2f", d, peakOf (out) / peakOf (loop));
        check (std::fabs (d) < 1.5 && allFinite (out), buf);
    }
    std::printf ("(e) impulse train / edge cases\n");
    {
        int n = (int) (2 * sr); std::vector<float> v ((size_t) n, 0.0f);
        for (int i = 1000; i < n; i += 12000) v[(size_t) i] = 1.0f;
        Audio in = makeMono (v, sr);
        for (double lf : { 0.5, 1.5, 4.0 })
        {
            Audio out = renderPghi (in, lf, 1.0);
            char buf[120]; std::snprintf (buf, sizeof buf, "clicks lf=%.1f finite, peak %.3f, len %d", lf, peakOf (out), out.numFrames());
            check (allFinite (out) && peakOf (out) < 4.0 && out.numFrames() == (int) std::lround (n * lf), buf);
        }
        // short inputs
        bool ok = true;
        for (int len : { 1, 2, 7, 100, 1000, 4095 })
            for (double lf : { 0.0625, 0.7, 1.0, 3.0, 16.0 })
                for (double pr : { 0.5, 1.0, 2.0 })
                {
                    Audio s = renderPghi (makeMono (sine (440, sr, len), sr), lf, pr);
                    int ex = (int) std::lround (len * std::max (1.0 / 16, std::min (16.0, lf)));
                    if (s.channels.empty() || s.numFrames() != ex || ! allFinite (s)) { ok = false; std::printf ("    bad len=%d lf=%g pr=%g -> %d (want %d)\n", len, lf, pr, s.numFrames(), ex); }
                }
        check (ok, "short inputs: exact length, finite");
        Audio z = renderPghi (makeMono (std::vector<float> (50000, 0.0f), sr), 1.5, 1.0);
        check (z.numFrames() == 75000 && peakOf (z) == 0.0, "silence in, silence out");
        check (renderPghi (Audio{}, 1.0, 1.0).channels.empty(), "empty in, empty out");
        Audio clampA = renderPghi (makeMono (sine (440, sr, 4800), sr), 100.0, 1.0);
        check (clampA.numFrames() == 4800 * 16, "lengthFactor clamped to 16");
        Audio r1 = renderPghi (makeMono (sine (440, sr, 50000), sr), 1.3, 1.0), r2 = renderPghi (makeMono (sine (440, sr, 50000), sr), 1.3, 1.0);
        check (r1.channels == r2.channels, "deterministic");
        Audio in44; in44.sampleRate = 44100; in44.channels = { sine (440, 44100, 100000) };
        Audio o44 = renderPghi (in44, 1.1, 1.0);
        double f = zcFreq (o44.channels[0], 44100, 20000, 90000);
        char buf[100]; std::snprintf (buf, sizeof buf, "44.1k sine %.3f Hz (%.2f cents)", f, cents (f, 440));
        check (std::fabs (cents (f, 440)) < 3.0, buf);
    }
    std::printf ("(r) resampleTo\n");
    {
        Audio in = makeMono (sine (1000, sr, 48000), sr);
        bool lenOk = true; for (int nl : { 1, 5, 24000, 47999, 48000, 48001, 96000, 200000 }) lenOk &= renderPghi (in, 1, 1).numFrames() == 48000 && resampleTo (in, nl).numFrames() == nl;
        check (lenOk, "exact output lengths");
        Audio up = resampleTo (in, 96000);
        double f = zcFreq (up.channels[0], sr, 2000, 94000);
        check (std::fabs (f - 500.0 * 1) < 0.01 || std::fabs (cents (f, 500)) < 0.1, "2x length halves pitch (as frequency per sample)");
        // shrinking must filter: 20 kHz tone shrunk 4x should vanish
        Audio hi = makeMono (sine (20000, sr, 48000, 0.8), sr);
        Audio dn = resampleTo (hi, 12000);
        check (rms (dn.channels[0], 2000, 10000) < 1e-3, "band-limit when shrinking (20k tone gone)");
        Audio dc = makeMono (std::vector<float> (4000, 0.5f), sr);
        Audio dd = resampleTo (dc, 1234);
        check (std::fabs (dd.channels[0][600] - 0.5f) < 1e-4, "DC gain 1");
        Audio rt = resampleTo (resampleTo (in, 71234), 48000);
        double e = 0; for (int i = 2000; i < 46000; ++i) e = std::max (e, (double) std::fabs (rt.channels[0][(size_t) i] - in.channels[0][(size_t) i]));
        char buf[100]; std::snprintf (buf, sizeof buf, "round trip max err %.2e", e); check (e < 2e-3, buf);
    }
    std::printf ("(f) timing, 4 s stereo 48k lf 1.25\n");
    {
        Audio in = chord (sr, 4.0);
        Audio loop = drumLoop (sr);
        double best = 1e9;
        for (int r = 0; r < 3; ++r)
        {
            auto t0 = std::chrono::steady_clock::now();
            Audio out = renderPghi (loop, 1.25, 1.0);
            auto t1 = std::chrono::steady_clock::now();
            best = std::min (best, std::chrono::duration<double, std::milli> (t1 - t0).count());
        }
        std::printf ("  render time (best of 3): %.1f ms for %d frames stereo\n", best, loop.numFrames());
        auto t0 = std::chrono::steady_clock::now(); Audio o = renderPghi (in, 1.25, 1.2); auto t1 = std::chrono::steady_clock::now();
        std::printf ("  with pitch 1.2: %.1f ms\n", std::chrono::duration<double, std::milli> (t1 - t0).count());
        auto t2 = std::chrono::steady_clock::now(); Audio o2 = renderPghi (in, 8.0, 1.0); auto t3 = std::chrono::steady_clock::now();
        std::printf ("  lf 8: %.1f ms; ", std::chrono::duration<double, std::milli> (t3 - t2).count());
        auto t4 = std::chrono::steady_clock::now(); Audio o3 = renderPghi (in, 0.125, 1.0); auto t5 = std::chrono::steady_clock::now();
        std::printf ("lf 0.125: %.1f ms\n", std::chrono::duration<double, std::milli> (t5 - t4).count());
    }

    // listening renders
    mkdir ("/tmp/pghi-demo", 0755);
    {
        Audio loop = drumLoop (sr), ch = chord (sr, 4.0);
        writeWav ("/tmp/pghi-demo/loop_original.wav", loop);
        writeWav ("/tmp/pghi-demo/loop_x1.5.wav", renderPghi (loop, 1.5, 1.0));
        writeWav ("/tmp/pghi-demo/loop_x0.75.wav", renderPghi (loop, 0.75, 1.0));
        writeWav ("/tmp/pghi-demo/loop_x2.wav", renderPghi (loop, 2.0, 1.0));
        writeWav ("/tmp/pghi-demo/loop_pitch+3st.wav", renderPghi (loop, 1.0, std::pow (2.0, 3 / 12.0)));
        writeWav ("/tmp/pghi-demo/loop_x1.25_pitch-5st.wav", renderPghi (loop, 1.25, std::pow (2.0, -5 / 12.0)));
        writeWav ("/tmp/pghi-demo/chord_original.wav", ch);
        writeWav ("/tmp/pghi-demo/chord_x2.wav", renderPghi (ch, 2.0, 1.0));
        writeWav ("/tmp/pghi-demo/chord_x0.5.wav", renderPghi (ch, 0.5, 1.0));
        writeWav ("/tmp/pghi-demo/chord_x4.wav", renderPghi (ch, 4.0, 1.0));
        std::printf ("WAVs in /tmp/pghi-demo\n");
    }
    std::printf ("%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
