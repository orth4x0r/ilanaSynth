#pragma once

// The Operator EG: a voice's operator envelopes, keyboard and velocity
// scaling, pitch envelope and LFO, computed exactly as the DX7 does, from a
// 156-byte voice (VCED order, OP6 first, as Dexed keeps it). The processor
// fills that voice from the oscillators' EG parameters (an oscillator whose
// ENVELOPE is Operator EG); the sound itself comes from ilanaSynth's six
// oscillators and FM matrix. This supplies each operator's gain in
// modulation cycles, which is the same unit ilanaSynth's phase modulation
// uses. .syx files and the DX7 banks are read into those parameters
// (Dx7Presets.h).
//
// Ported from msfa (Dexed's engine): env.cc, dx7note.cc, pitchenv.cc and
// lfo.cc. Copyright 2012 Google Inc., 2016-2025 Pascal Gauthier; Apache
// License 2.0 (http://www.apache.org/licenses/LICENSE-2.0).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Dx7
{
constexpr int voiceBytes = 156;
constexpr int block = 64;          // msfa advances envelopes once per 64 samples
using Voice = std::array<std::uint8_t, voiceBytes>;

inline void clampRanges (Voice& v);

// Packed 128-byte cartridge voice (VMEM) to the 156-byte layout.
inline Voice unpack (const std::uint8_t* p)
{
    Voice v {};
    for (int op = 0; op < 6; ++op)
    {
        const auto* s = p + op * 17;
        auto* d = v.data() + op * 21;
        for (int i = 0; i < 11; ++i)
            d[i] = s[i];
        d[11] = s[11] & 3;
        d[12] = (s[11] >> 2) & 3;
        d[13] = s[12] & 7;
        d[20] = (s[12] >> 3) & 15;
        d[14] = s[13] & 3;
        d[15] = (s[13] >> 2) & 7;
        d[16] = s[14];
        d[17] = s[15] & 1;
        d[18] = (s[15] >> 1) & 31;
        d[19] = s[16];
    }
    for (int i = 0; i < 8; ++i)
        v[126 + (size_t) i] = p[102 + i];
    v[134] = p[110] & 31;
    v[135] = p[111] & 7;
    v[136] = (p[111] >> 3) & 1;
    v[137] = p[112];
    v[138] = p[113];
    v[139] = p[114];
    v[140] = p[115];
    v[141] = p[116] & 1;
    v[142] = (p[116] >> 1) & 7;
    v[143] = (p[116] >> 4) & 7;
    v[144] = p[117];
    for (int i = 0; i < 10; ++i)
        v[145 + (size_t) i] = p[118 + i];
    v[155] = 0x3f;
    clampRanges (v);
    return v;
}

// Bytes from a damaged cartridge or state stay in their ranges, so no table
// is read past its end.
inline void clampRanges (Voice& v)
{
    static constexpr std::uint8_t maxima[21] { 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14 };
    for (int op = 0; op < 6; ++op)
        for (int i = 0; i < 21; ++i)
            v[(size_t) (op * 21 + i)] = std::min (v[(size_t) (op * 21 + i)], maxima[i]);
    for (int i = 126; i < 134; ++i)
        v[(size_t) i] = std::min<std::uint8_t> (v[(size_t) i], 99);
    v[134] = std::min<std::uint8_t> (v[134], 31);
    v[137] = std::min<std::uint8_t> (v[137], 99);
    v[138] = std::min<std::uint8_t> (v[138], 99);
    v[139] = std::min<std::uint8_t> (v[139], 99);
    v[140] = std::min<std::uint8_t> (v[140], 99);
    v[142] = std::min<std::uint8_t> (v[142], 5);
    v[144] = std::min<std::uint8_t> (v[144], 48);
    v[135] = std::min<std::uint8_t> (v[135], 7);
    v[143] = std::min<std::uint8_t> (v[143], 7);
}

inline std::string name (const Voice& v)
{
    std::string text (reinterpret_cast<const char*> (v.data() + 145), 10);
    std::string out;
    for (auto c : text)
    {
        if (c < 32 || c > 126)
            c = ' ';
        // Cartridges pad names with runs of spaces ("BRASS   1"): one is enough.
        if (c != ' ' || (! out.empty() && out.back() != ' '))
            out += c;
    }
    while (! out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

// Operator k (1-6) lives at (6 - k) * 21 in the voice.
inline const std::uint8_t* op (const Voice& v, int k) { return v.data() + (6 - k) * 21; }

// msfa's algorithm table (OP6 first), and the routing it implies.
inline const std::array<std::array<std::uint8_t, 6>, 32>& algorithms()
{
    static const std::array<std::array<std::uint8_t, 6>, 32> table { {
        { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0x11, 0x14, 0xc1, 0x14 },
        { 0xc1, 0x11, 0x14, 0x01, 0x11, 0x14 }, { 0xc1, 0x11, 0x94, 0x01, 0x11, 0x14 },
        { 0xc1, 0x14, 0x01, 0x14, 0x01, 0x14 }, { 0xc1, 0x94, 0x01, 0x14, 0x01, 0x14 },
        { 0xc1, 0x11, 0x05, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0xc5, 0x14, 0x01, 0x14 },
        { 0x01, 0x11, 0x05, 0x14, 0xc1, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x11, 0x14 },
        { 0xc1, 0x05, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x05, 0x14, 0xc1, 0x14 },
        { 0xc1, 0x05, 0x05, 0x14, 0x01, 0x14 }, { 0xc1, 0x05, 0x11, 0x14, 0x01, 0x14 },
        { 0x01, 0x05, 0x11, 0x14, 0xc1, 0x14 }, { 0xc1, 0x11, 0x02, 0x25, 0x05, 0x14 },
        { 0x01, 0x11, 0x02, 0x25, 0xc5, 0x14 }, { 0x01, 0x11, 0x11, 0xc5, 0x05, 0x14 },
        { 0xc1, 0x14, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x14 },
        { 0x01, 0x14, 0x14, 0xc1, 0x14, 0x14 }, { 0xc1, 0x14, 0x14, 0x14, 0x01, 0x14 },
        { 0xc1, 0x14, 0x14, 0x01, 0x14, 0x04 }, { 0xc1, 0x14, 0x14, 0x14, 0x04, 0x04 },
        { 0xc1, 0x14, 0x14, 0x04, 0x04, 0x04 }, { 0xc1, 0x05, 0x14, 0x01, 0x14, 0x04 },
        { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x01, 0x14 },
        { 0xc1, 0x14, 0x01, 0x14, 0x04, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x04, 0x04 },
        { 0xc1, 0x14, 0x04, 0x04, 0x04, 0x04 }, { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 },
    } };
    return table;
}

struct Routing
{
    std::array<std::array<bool, 6>, 6> modulates {}; // [source][target], 0-based operators 1-6
    std::array<bool, 6> carrier {};
    int feedbackOp = -1;                              // 0-based, or -1
};

inline Routing routing (int algorithm) // 0..31
{
    Routing r;
    const auto& flags = algorithms()[(size_t) std::clamp (algorithm, 0, 31)];
    std::array<std::vector<int>, 3> buses;
    for (int index = 0; index < 6; ++index)
    {
        const auto op = 5 - index;
        const auto f = flags[(size_t) index];
        const auto inbus = (f >> 4) & 3, outbus = f & 3;
        const auto add = (f & 4) != 0;
        if (inbus != 0)
            for (auto source : buses[(size_t) inbus])
                r.modulates[(size_t) source][(size_t) op] = true;
        if ((f & 0xc0) == 0xc0)
            r.feedbackOp = op;
        if (outbus == 0)
            r.carrier[(size_t) op] = true;
        else
        {
            if (! add)
                buses[(size_t) outbus].clear();
            buses[(size_t) outbus].push_back (op);
        }
    }
    return r;
}

// Operator frequency: a ratio of the note's pitch, or a fixed frequency,
// and the detune in cents at this note (msfa's osc_freq).
struct OpTuning
{
    bool fixed = false;
    double ratio = 1.0, fixedHz = 440.0, detuneCents = 0.0;
};

inline OpTuning tuning (const Voice& v, int k, int midiNote)
{
    const auto* d = op (v, k);
    OpTuning t;
    t.fixed = d[17] != 0;
    const int coarse = d[18], fine = d[19], detune = d[20];
    if (! t.fixed)
    {
        t.ratio = (coarse == 0 ? 0.5 : (double) coarse) * (1.0 + 0.01 * fine);
        const auto logfreq = 50857777.0 + (double) ((1 << 24) / 12) * midiNote; // log2 Hz, Q24
        const auto detuneRatio = 0.0209 * std::exp (-0.396 * logfreq / (1 << 24)) / 7.0;
        t.detuneCents = detuneRatio * logfreq * (detune - 7) / (1 << 24) * 1200.0;
    }
    else
    {
        t.fixedHz = std::pow (10.0, (coarse & 3) + fine / 100.0);
        t.detuneCents = detune > 7 ? 13457.0 * (detune - 7) / (1 << 24) * 1200.0 : 0.0;
    }
    return t;
}

// ---- msfa's envelope, keyboard and velocity scaling ----

inline int scaleOutLevel (int outlevel)
{
    static constexpr int lut[] { 0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46 };
    return outlevel >= 20 ? 28 + outlevel : lut[std::clamp (outlevel, 0, 19)];
}

inline int scaleVelocity (int velocity, int sensitivity)
{
    static constexpr std::uint8_t data[64] {
        0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
        166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202,
        205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232,
        233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251,
        252, 253, 254 };
    const auto value = data[std::clamp (velocity, 0, 127) >> 1] - 239;
    return ((sensitivity * value + 7) >> 3) << 4;
}

inline int scaleRate (int midiNote, int sensitivity)
{
    const auto x = std::clamp (midiNote / 3 - 7, 0, 31);
    return (sensitivity * x) >> 3;
}

inline int scaleCurve (int group, int depth, int curve)
{
    static constexpr std::uint8_t expData[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
                                              80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250 };
    int scale;
    if (curve == 0 || curve == 3)
        scale = (group * depth * 329) >> 12;
    else
        scale = (expData[std::min (group, (int) sizeof (expData) - 1)] * depth * 329) >> 15;
    return curve < 2 ? -scale : scale;
}

inline int scaleLevel (int midiNote, int breakPoint, int leftDepth, int rightDepth, int leftCurve, int rightCurve)
{
    const auto offset = midiNote - breakPoint - 17;
    return offset >= 0 ? scaleCurve ((offset + 1) / 3, rightDepth, rightCurve)
                       : scaleCurve (-(offset - 1) / 3, leftDepth, leftCurve);
}

class Envelope
{
public:
    void init (const int r[4], const int l[4], int outlevel, int rateScaling, double sampleRate)
    {
        std::copy (r, r + 4, rates);
        std::copy (l, l + 4, levels);
        outLevel = outlevel;
        rateScale = rateScaling;
        srMultiplier = (std::int64_t) ((44100.0 / sampleRate) * (1 << 24));
        level = 0;
        down = true;
        advance (0);
    }

    std::int32_t next() // one 64-sample block
    {
        if (ix < 3 || (ix < 4 && ! down))
        {
            if (rising)
            {
                constexpr int jumpTarget = 1716;
                if (level < (jumpTarget << 16))
                    level = jumpTarget << 16;
                level += (((17 << 24) - level) >> 24) * inc;
                if (level >= targetLevel)
                {
                    level = targetLevel;
                    advance (ix + 1);
                }
            }
            else
            {
                level -= inc;
                if (level <= targetLevel)
                {
                    level = targetLevel;
                    advance (ix + 1);
                }
            }
        }
        return level;
    }

    void keyUp()
    {
        if (down)
        {
            down = false;
            advance (3);
        }
    }

    bool isActive() const { return ix < 4 || levels[3] > 0; }
    // 0-2: moving to L1-L3; 3: held at L3 (key down) or moving to L4; 4: done.
    int getStage() const { return ix; }

    // Live edits while the note sounds (a knob, or a modulated parameter):
    // new rates and levels take over the running stage from where it is;
    // a new output level shifts the whole envelope, as the DX7's does.
    void setShape (const int r[4], const int l[4], int outlevel)
    {
        std::copy (r, r + 4, rates);
        std::copy (l, l + 4, levels);
        const auto shift = (std::int32_t) (outlevel - outLevel) << 16;
        outLevel = outlevel;
        level = std::max<std::int32_t> (0, level + shift);
        if (ix < 4)
            advance (ix);
    }

private:
    void advance (int newIx)
    {
        ix = newIx;
        if (ix >= 4)
            return;
        auto actual = scaleOutLevel (levels[ix]) >> 1;
        actual = (actual << 6) + outLevel - 4256;
        actual = std::max (16, actual);
        targetLevel = actual << 16;
        rising = targetLevel > level;
        auto qrate = (rates[ix] * 41) >> 6;
        qrate = std::min (qrate + rateScale, 63);
        inc = (4 + (qrate & 3)) << (2 + 6 + (qrate >> 2)); // LG_N = 6
        inc = (std::int32_t) (((std::int64_t) inc * srMultiplier) >> 24);
    }

    int rates[4] {}, levels[4] {};
    int outLevel = 0, rateScale = 0, ix = 0;
    std::int32_t level = 0, targetLevel = 0, inc = 0;
    std::int64_t srMultiplier = 1 << 24;
    bool rising = false, down = true;
};

class PitchEnvelope
{
public:
    void init (const int r[4], const int l[4], double sampleRate)
    {
        unit = (int) (block * (1 << 24) / (21.3 * sampleRate) + 0.5);
        std::copy (r, r + 4, rates);
        std::copy (l, l + 4, levels);
        level = table (l[3]) << 19;
        down = true;
        advance (0);
    }

    std::int32_t next()
    {
        if (ix < 3 || (ix < 4 && ! down))
        {
            if (rising)
            {
                level += inc;
                if (level >= targetLevel) { level = targetLevel; advance (ix + 1); }
            }
            else
            {
                level -= inc;
                if (level <= targetLevel) { level = targetLevel; advance (ix + 1); }
            }
        }
        return level;
    }

    void keyUp()
    {
        if (down)
        {
            down = false;
            advance (3);
        }
    }

    int getStage() const { return ix; }
    // A pitch level (0-99) in log2 Q19 steps: 50 is 0, 0 and 99 about four
    // octaves down and up (32 steps an octave).
    static int levelSteps (int l) { return table (l); }

    // True when every level is 50 (no pitch movement), so it can be skipped.
    bool isFlat() const { return levels[0] == 50 && levels[1] == 50 && levels[2] == 50 && levels[3] == 50; }

    // A live edit: new rates and levels take over the running stage.
    void setShape (const int r[4], const int l[4])
    {
        std::copy (r, r + 4, rates);
        std::copy (l, l + 4, levels);
        if (ix < 4)
            advance (ix);
    }

    // The largest swing of its four levels from the note, log2 Q24 (0 when
    // flat): the "Op Pitch Env" source is the envelope over this.
    std::int32_t span() const
    {
        auto most = 0;
        for (auto l : levels)
            most = std::max (most, std::abs (table (l)));
        return most << 19;
    }

private:
    static int table (int l)
    {
        static constexpr std::int8_t tab[] {
            -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43,
            -41, -39, -37, -35, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24,
            -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
            -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
            28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73,
            82, 92, 103, 115, 127 };
        return tab[std::clamp (l, 0, 99)];
    }

    void advance (int newIx)
    {
        static constexpr std::uint8_t rateTab[] {
            1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
            12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47,
            49, 51, 53, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82,
            85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141, 147,
            153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255 };
        ix = newIx;
        if (ix < 4)
        {
            targetLevel = table (levels[ix]) << 19;
            rising = targetLevel > level;
            inc = rateTab[std::clamp (rates[ix], 0, 99)] * unit;
        }
    }

    int rates[4] {}, levels[4] {};
    int ix = 0, unit = 0;
    std::int32_t level = 0, targetLevel = 0, inc = 0;
    bool rising = false, down = true;
};

class Lfo
{
public:
    void init (const Voice& v, double sampleRate)
    {
        const auto unit = (std::uint32_t) (block * 25190424 / sampleRate + 0.5);
        setSpeed (v[137], sampleRate);
        auto a = 99 - (int) v[138];
        if (a == 99)
            delayInc = delayInc2 = ~0u;
        else
        {
            a = (16 + (a & 15)) << (1 + (a >> 4));
            delayInc = unit * (std::uint32_t) a;
            a &= 0xff80;
            a = std::max (0x80, a);
            delayInc2 = unit * (std::uint32_t) a;
        }
        waveform = v[142];
        sync = v[141] != 0;
    }

    // The speed (0-99) in Hz, as the DX7 runs it.
    static double hz (int speed)
    {
        return speedTable()[(size_t) std::clamp (speed, 0, 99)];
    }

    void setSpeed (int speed, double sampleRate)
    {
        const auto ratio = (std::uint32_t) (4437500000.0 * block / sampleRate);
        delta = (std::uint32_t) (speedTable()[(size_t) std::clamp (speed, 0, 99)] * ratio);
    }

    void setWaveform (int wave) { waveform = wave; }

    void keyDown()
    {
        if (sync)
            phase = (1u << 31) - 1;
        delayState = 0;
    }

    // Q24: 0..1 << 24, centre 1 << 23.
    std::int32_t value()
    {
        phase += delta;
        std::int32_t x;
        switch (waveform)
        {
            case 0: x = (std::int32_t) (phase >> 7); x ^= -(std::int32_t) (phase >> 31); return x & ((1 << 24) - 1);
            case 1: return (std::int32_t) ((~phase ^ (1u << 31)) >> 8);
            case 2: return (std::int32_t) ((phase ^ (1u << 31)) >> 8);
            case 3: return (std::int32_t) (((~phase) >> 7) & (1u << 24));
            case 4: return (1 << 23) + (std::int32_t) (std::sin (phase * (6.283185307179586 / 4294967296.0)) * (1 << 23));
            case 5:
                if (phase < delta)
                    randState = (randState * 179 + 17) & 0xff;
                return ((randState ^ 0x80) + 1) << 16;
            default: return 1 << 23;
        }
    }

    std::int32_t delay()
    {
        const auto inc = delayState < (1u << 31) ? delayInc : delayInc2;
        const auto d = (std::uint64_t) delayState + inc;
        if (d > ~0u)
            return 1 << 24;
        delayState = (std::uint32_t) d;
        return d < (1u << 31) ? 0 : (std::int32_t) ((d >> 7) & ((1 << 24) - 1));
    }

private:
    static const std::array<double, 100>& speedTable()
    {
        static const std::array<double, 100> table {
            0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
            1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
            3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
            4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
            6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
            7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
            9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
            16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
            25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
            38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084 };
        return table;
    }

    std::uint32_t phase = 0, delta = 0, delayState = 0, delayInc = 0, delayInc2 = 0;
    int waveform = 0, randState = 0;
    bool sync = false;
};

// One note's DX7 control side. step() runs once per 64 samples and gives
// each operator's gain (modulation cycles, as msfa's) at the end of the
// block, and the pitch offset in octaves; the voice ramps across the block.
class Note
{
public:
    // carriers: the operators heard (their envelopes decide when the note
    // has ended).
    void start (const Voice& v, int midiNote, int velocity, double sampleRate, const std::array<bool, 6>& carriers)
    {
        note = midiNote;
        noteVelocity = velocity;
        for (int k = 1; k <= 6; ++k)
        {
            const auto* d = op (v, k);
            int r[4], l[4];
            for (int i = 0; i < 4; ++i)
            {
                r[i] = d[i];
                l[i] = d[4 + i];
            }
            env[(size_t) (k - 1)].init (r, l, outLevelFor (d), scaleRate (midiNote, d[13]), sampleRate);
            ams[(size_t) (k - 1)] = ampModSensFor (d[14]);
        }
        int r[4], l[4];
        for (int i = 0; i < 4; ++i)
        {
            r[i] = v[126 + (size_t) i];
            l[i] = v[130 + (size_t) i];
        }
        pitchEnv.init (r, l, sampleRate);
        flatPitch = pitchEnv.isFlat();
        pitchSpan = pitchEnv.span();
        setDepths (v);
        carrier = carriers;
        lfo.init (v, sampleRate);
        lfo.keyDown();
        gains.fill (0.0f);
        sampleRateUsed = sampleRate;
        released = false;
        releasedBlocks = 0;
        playedBlocks = -1;
        step();
    }

    // The voice's settings changed while the note sounds (a knob turned, or
    // a modulated parameter such as the wheel on PITCH DEPTH): the
    // envelopes' rates, levels and output levels, the pitch envelope and the
    // LFO's speed, wave and depths follow from here. Only called on a
    // change, so a note nobody touches plays exactly as before.
    void update (const Voice& v)
    {
        for (int k = 1; k <= 6; ++k)
        {
            const auto* d = op (v, k);
            int r[4], l[4];
            for (int i = 0; i < 4; ++i)
            {
                r[i] = d[i];
                l[i] = d[4 + i];
            }
            env[(size_t) (k - 1)].setShape (r, l, outLevelFor (d));
            ams[(size_t) (k - 1)] = ampModSensFor (d[14]);
        }
        int r[4], l[4];
        for (int i = 0; i < 4; ++i)
        {
            r[i] = v[126 + (size_t) i];
            l[i] = v[130 + (size_t) i];
        }
        pitchEnv.setShape (r, l);
        flatPitch = pitchEnv.isFlat();
        pitchSpan = pitchEnv.span();
        setDepths (v);
        lfo.setSpeed (v[137], sampleRateUsed);
        lfo.setWaveform (v[142]);
    }

    void keyUp()
    {
        for (auto& e : env)
            e.keyUp();
        pitchEnv.keyUp();
        released = true;
        releasedBlocks = 0;
        releaseLimit = (int) (10.0 * sampleRateUsed / block);
    }

    void step()
    {
        // A voice whose release level isn't zero would ring for ever (a DX7
        // keeps it until the voice is stolen); here it ends 10 s after the key.
        ++playedBlocks;
        if (released && ++releasedBlocks > releaseLimit)
        {
            active = false;
            gains.fill (0.0f);
            return;
        }

        const auto lfoValue = lfo.value();
        const auto lfoDelay = lfo.delay();
        // The LFO after its delay, -1..1, for the "Op LFO" source.
        lfoOutput = ((float) lfoValue / (float) (1 << 23) - 1.0f) * (float) lfoDelay / (float) (1 << 24);

        // Pitch: envelope plus LFO, log2 Q24.
        const auto pmd = (std::uint32_t) pitchModDepth * (std::uint32_t) lfoDelay;
        const auto sens = (std::int32_t) pitchModSensitivity * (lfoValue - (1 << 23));
        auto pmod = (std::int32_t) (((std::int64_t) pmd * (std::int64_t) sens) >> 39);
        pmod = std::abs (pmod) * (sens < 0 ? -1 : 1);
        const auto pitchLevel = flatPitch ? 0 : pitchEnv.next();
        pitchShape = pitchSpan > 0 ? (float) pitchLevel / (float) pitchSpan : 0.0f;
        const auto pitch = pitchLevel + pmod;
        pitchOctaves = (float) pitch / (float) (1 << 24);

        // Amplitude modulation.
        const auto inverted = (1 << 24) - lfoValue;
        auto amd = (std::uint32_t) (((std::int64_t) ampModDepth * (std::int64_t) lfoDelay) >> 8);
        amd = (std::uint32_t) (((std::int64_t) amd * (std::int64_t) inverted) >> 24);

        active = false;
        for (int i = 0; i < 6; ++i)
        {
            auto level = env[(size_t) i].next();
            if (ams[(size_t) i] != 0)
            {
                const auto sensAmp = (std::uint32_t) (((std::uint64_t) amd * ams[(size_t) i]) >> 24);
                const auto pt = (std::uint32_t) std::exp ((float) sensAmp / 262144.0f * 0.07f + 12.2f);
                level -= (std::int32_t) (((std::uint64_t) level * ((std::uint64_t) pt << 4)) >> 28);
            }
            gains[(size_t) i] = (float) std::exp2 ((double) level / (1 << 24) - 14.0);
            active = active || (carrier[(size_t) i] && env[(size_t) i].isActive());
        }
    }

    // Operators 1-6 (0-based), in cycles of phase modulation.
    const std::array<float, 6>& getGains() const { return gains; }
    float getPitchOctaves() const { return pitchOctaves; }
    // The two sources other modules can use: the LFO (after its delay) and
    // the pitch envelope's shape, both -1..1.
    float getLfoOutput() const { return lfoOutput; }
    // How long the note has played, and since its key was let go (-1 while
    // held), in seconds: the editor's playhead.
    float getSecondsPlayed() const { return (float) (playedBlocks * block / sampleRateUsed); }
    float getSecondsReleased() const { return released ? (float) (releasedBlocks * block / sampleRateUsed) : -1.0f; }
    float getPitchShape() const { return pitchShape; }
    bool isActive() const { return active; }
    bool isCarrier (int i) const { return carrier[(size_t) i]; }

private:
    // An operator's output level as its envelope adds it: the output knob,
    // keyboard scaling and velocity (msfa's dx7note).
    int outLevelFor (const std::uint8_t* d) const
    {
        auto out = scaleOutLevel (d[16]);
        out += scaleLevel (note, d[8], d[9], d[10], d[11], d[12]);
        out = std::min (127, out) << 5;
        out += scaleVelocity (noteVelocity, d[15]);
        return std::max (0, out);
    }

    static std::uint32_t ampModSensFor (int sensitivity)
    {
        static constexpr std::uint32_t ampModSens[] { 0, 4342338, 7171437, 16777216 };
        return ampModSens[sensitivity & 3];
    }

    void setDepths (const Voice& v)
    {
        static constexpr int pitchModSens[] { 0, 10, 20, 33, 55, 92, 153, 255 };
        pitchModDepth = (v[139] * 165) >> 6;
        pitchModSensitivity = pitchModSens[v[143] & 7];
        ampModDepth = (v[140] * 165) >> 6;
    }

    std::array<Envelope, 6> env;
    std::array<std::uint32_t, 6> ams {};
    std::array<float, 6> gains {};
    std::array<bool, 6> carrier {};
    PitchEnvelope pitchEnv;
    Lfo lfo;
    int pitchModDepth = 0, pitchModSensitivity = 0, ampModDepth = 0;
    int note = 60, noteVelocity = 100;
    std::int32_t pitchSpan = 0;
    float pitchOctaves = 0.0f, lfoOutput = 0.0f, pitchShape = 0.0f;
    bool flatPitch = true, active = false, released = false;
    double sampleRateUsed = 48000.0;
    int releasedBlocks = 0, releaseLimit = 0, playedBlocks = 0;
};
} // namespace Dx7
