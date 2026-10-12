// Standalone test for StretchPaul.cpp.
// g++ -O2 -std=c++17 -I src/dsp/stretch -I /tmp/libs/jp tests/stretch/paul_test.cpp src/dsp/stretch/StretchPaul.cpp pffft.o fftpack.o -o paul_test
#include "OfflineStretch.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace OfflineStretch;
static const double PI = 3.14159265358979323846;

// Temporary linear resampler for the test only.
namespace OfflineStretch {
Audio resampleTo (const Audio& in, int n)
{
    Audio o; o.sampleRate = in.sampleRate;
    const int m = in.numFrames();
    for (auto& ch : in.channels)
    {
        std::vector<float> r ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            double p = (double) i * m / n; int a = (int) p; double f = p - a;
            float x0 = ch[(size_t) std::min (a, m - 1)], x1 = ch[(size_t) std::min (a + 1, m - 1)];
            r[(size_t) i] = (float) (x0 + (x1 - x0) * f);
        }
        o.channels.push_back(std::move(r));
    }
    return o;
}
}

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        auto w = std::polar (1.0, -2 * PI / len);
        for (size_t i = 0; i < n; i += len) { std::complex<double> c = 1; for (size_t k = 0; k < len / 2; ++k) { auto u = a[i+k], v = a[i+k+len/2]*c; a[i+k]=u+v; a[i+k+len/2]=u-v; c*=w; } }
    }
}
static double peakHz (const std::vector<float>& x, double sr)
{
    const size_t n = 65536; std::vector<std::complex<double>> a (n);
    size_t start = x.size() > n ? (x.size() - n) / 2 : 0;
    for (size_t i = 0; i < n && start + i < x.size(); ++i) a[i] = x[start+i] * (0.5 - 0.5 * std::cos (2*PI*i/n));
    fft (a); size_t best = 1;
    for (size_t k = 1; k < n/2; ++k) if (std::abs(a[k]) > std::abs(a[best])) best = k;
    return best * sr / n;
}
static double rms (const Audio& a)
{ double e = 0; for (auto& c : a.channels) for (float v : c) e += (double)v*v; return std::sqrt (e / ((double)a.numFrames()*a.channels.size())); }
static bool finite (const Audio& a)
{ for (auto& c : a.channels) for (float v : c) if (! std::isfinite (v)) return false; return true; }
static Audio sine (double f, double secs, double sr)
{ Audio a; a.sampleRate = sr; int n = (int)(secs*sr); a.channels.assign(2, std::vector<float>((size_t)n));
  for (int i=0;i<n;++i){ float v=(float)(0.5*std::sin(2*PI*f*i/sr)); a.channels[0][i]=v; a.channels[1][i]=v; } return a; }

static void writeWav (const char* path, const Audio& a)
{
    FILE* f = fopen (path, "wb"); if (!f) return;
    int ch = (int) a.channels.size(), n = a.numFrames(), sr = (int) a.sampleRate;
    auto w32 = [&](uint32_t v){ fwrite(&v,4,1,f); }; auto w16 = [&](uint16_t v){ fwrite(&v,2,1,f); };
    fwrite("RIFF",1,4,f); w32(36 + n*ch*2); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(ch); w32(sr); w32(sr*ch*2); w16(ch*2); w16(16);
    fwrite("data",1,4,f); w32(n*ch*2);
    for (int i=0;i<n;++i) for (int c=0;c<ch;++c){ float v=std::max(-1.f,std::min(1.f,a.channels[c][i])); int16_t s=(int16_t)std::lrint(v*32767); fwrite(&s,2,1,f);} 
    fclose(f);
}
static int fails = 0;
static void check (bool ok, const char* what) { printf ("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++fails; }

int main()
{
    const double sr = 48000;
    Audio s = sine (440, 4.0, sr);
    for (double lf : { 0.5, 1.0, 4.0, 8.0 })
    {
        Audio o = renderPaulstretch (s, lf, 1.0);
        char b[128]; snprintf (b, 128, "length factor %.1f: %d == %d", lf, o.numFrames(), (int) std::llround (s.numFrames()*lf));
        check (o.numFrames() == (int) std::llround (s.numFrames()*lf) && finite (o), b);
        double db = 20*std::log10 (rms(o)/rms(s)); printf ("   rms diff %.2f dB\n", db);
        check (std::fabs (db) < 2.0, "rms within 2 dB");
    }
    { Audio o = renderPaulstretch (s, 4.0, 1.0); double p = peakHz (o.channels[0], sr);
      printf ("   4x peak %.2f Hz\n", p); check (std::fabs (p-440) < 4.4, "peak 440 +-1%"); }
    { Audio o = renderPaulstretch (s, 4.0, 2.0); double p = peakHz (o.channels[0], sr);
      printf ("   4x pitch2 peak %.2f Hz len %d\n", p, o.numFrames()); check (std::fabs (p-880) < 8.8 && o.numFrames()==(int)(s.numFrames()*4), "pitch 2 -> 880 +-1%"); }
    { Audio sh = sine (440, 0.01, sr); Audio o = renderPaulstretch (sh, 4.0, 1.0); check (o.numFrames()==(int)std::llround(sh.numFrames()*4.0) && finite(o), "short input"); }
    { Audio o = renderPaulstretch (s, 100.0, 0.001); check (o.numFrames()==(int)std::llround(s.numFrames()*16.0), "clamp"); }
    { Audio z = s; for (auto& c : z.channels) std::fill (c.begin(), c.end(), 0.f); Audio o = renderPaulstretch (z, 4, 1); check (finite (o) && o.numFrames() == z.numFrames()*4, "silence"); }
    { Audio a = renderPaulstretch (s, 4, 1), b = renderPaulstretch (s, 4, 1); check (a.channels == b.channels, "repeatable"); }
    {
        auto t0 = std::chrono::steady_clock::now();
        Audio o = renderPaulstretch (s, 4.0, 1.0);
        double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        printf ("render 4 s stereo 48k at 4x: %.1f ms (%d frames)\n", ms, o.numFrames());
    }

    // Demos
    std::mt19937 rng (1); std::uniform_real_distribution<float> nz (-1, 1);
    Audio pad; pad.sampleRate = sr; int n = (int)(3*sr); pad.channels.assign(2, std::vector<float>(n, 0.f));
    double notes[] = {220, 261.63, 329.63, 392.0};
    for (double f : notes) for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
        for (int h = 1; h <= 4; ++h) pad.channels[c][i] += (float)(0.06 / h * std::sin (2*PI*f*h*(1 + (c?0.002:-0.002))*i/sr));
    Audio loop; loop.sampleRate = sr; int ln = (int)(4*sr); loop.channels.assign(2, std::vector<float>(ln, 0.f));
    double beat = 0.5*sr;
    for (int b = 0; b < 8; ++b) {
        int t = (int)(b*beat);
        if (b % 2 == 0) for (int i = 0; i < (int)(0.2*sr) && t+i < ln; ++i) { double e = std::exp(-i/(0.06*sr)); float v=(float)(0.8*e*std::sin(2*PI*(50+90*std::exp(-i/(0.02*sr)))*i/sr)); loop.channels[0][t+i]+=v; loop.channels[1][t+i]+=v; }
        else for (int i = 0; i < (int)(0.15*sr) && t+i < ln; ++i) { float v=(float)(0.4*std::exp(-i/(0.04*sr)))*nz(rng); loop.channels[0][t+i]+=v; loop.channels[1][t+i]+=v; }
        double bf = (b%4<2)?55.0:73.4; for (int i = 0; i < (int)beat && t+i < ln; ++i) { float v=(float)(0.3*std::exp(-i/(0.4*sr))*std::sin(2*PI*bf*i/sr)); loop.channels[0][t+i]+=v; loop.channels[1][t+i]+=v; }
    }
    writeWav ("/tmp/paul-demo/pad_1x.wav", renderPaulstretch (pad, 1.0, 1.0));
    writeWav ("/tmp/paul-demo/pad_4x.wav", renderPaulstretch (pad, 4.0, 1.0));
    writeWav ("/tmp/paul-demo/loop_1x.wav", renderPaulstretch (loop, 1.0, 1.0));
    writeWav ("/tmp/paul-demo/loop_4x.wav", renderPaulstretch (loop, 4.0, 1.0));
    writeWav ("/tmp/paul-demo/pad_orig.wav", pad); writeWav ("/tmp/paul-demo/loop_orig.wav", loop);
    printf (fails ? "FAILURES: %d\n" : "ALL PASS\n", fails);
    return fails ? 1 : 0;
}
