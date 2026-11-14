#include "ModalPianoTable.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "WavetableData.h"

namespace
{
    struct Reader
    {
        const unsigned char* data;
        size_t size, position = 0;

        bool has (size_t bytes) const { return position + bytes <= size; }

        template <typename T>
        T read()
        {
            T value {};
            if (has (sizeof (T)))
                std::memcpy (&value, data + position, sizeof (T));
            position += sizeof (T);
            return value;
        }
    };

    void buildBodyCurve (ModalPianoTable& table)
    {
        constexpr int bins = ModalPianoTable::curveBins;
        const auto t = juce::jmin (table.lateTime + 1, (int) table.times.size() - 1); // (0.08 s)
        const auto times = (int) table.times.size();
        std::vector<double> sum (bins, 0.0), weight (bins, 0.0);
        for (const auto& e : table.entries)
        {
            const auto hz = 440.0 * std::exp2 ((e.note - 69) / 12.0);
            auto power = 1.0e-30;
            for (int k = 0; k < e.partials; ++k)
                power += std::pow (10.0, e.levels[(size_t) (k * times + t)] / 10.0);
            const auto total = 10.0 * std::log10 (power);
            for (int k = 0; k < e.partials; ++k)
            {
                const auto f = (double) e.ratios[(size_t) k] * hz;
                if (! (f > ModalPianoTable::curveLowHz))
                    continue;
                const auto x = ModalPianoTable::curveBinsPerOctave * std::log2 (f / ModalPianoTable::curveLowHz);
                if (x >= bins - 1)
                    continue;
                const auto i = (int) x;
                const auto a = x - i;
                const auto level = e.levels[(size_t) (k * times + t)] - total;
                sum[(size_t) i] += (1.0 - a) * level;
                weight[(size_t) i] += 1.0 - a;
                sum[(size_t) i + 1] += a * level;
                weight[(size_t) i + 1] += a;
            }
        }
        // Empty bins take their nearest filled neighbours' line (flat past the ends).
        std::vector<double> curve (bins, 0.0);
        std::vector<int> filled;
        for (int i = 0; i < bins; ++i)
            if (weight[(size_t) i] > 0.5)
                filled.push_back (i);
        if (filled.empty())
            return;
        for (int i = 0; i < bins; ++i)
        {
            const auto next = std::lower_bound (filled.begin(), filled.end(), i);
            const auto value = [&] (int j) { return sum[(size_t) j] / weight[(size_t) j]; };
            if (next == filled.end())
                curve[(size_t) i] = value (filled.back());
            else if (*next == i || next == filled.begin())
                curve[(size_t) i] = value (*next);
            else
            {
                const auto hi = *next, lo = *(next - 1);
                curve[(size_t) i] = value (lo) + (value (hi) - value (lo)) * (i - lo) / (double) (hi - lo);
            }
        }
        // An octave-wide triangle (edges held).
        constexpr int reach = ModalPianoTable::curveBinsPerOctave;
        table.bodyCurve.assign (bins, 0.0f);
        for (int i = 0; i < bins; ++i)
        {
            double total = 0.0, norm = 0.0;
            for (int j = -reach; j <= reach; ++j)
            {
                const auto w = (double) (reach + 1 - std::abs (j));
                total += w * curve[(size_t) juce::jlimit (0, bins - 1, i + j)];
                norm += w;
            }
            table.bodyCurve[(size_t) i] = (float) (total / norm);
        }
    }

    // What each mode is, for SIZE (2026-11-13, checked against both
    // tables). The guitar's: in each entry, the air mode (A0) is its
    // loudest mode at 0.08 s from 80 to 180 Hz (102.5 Hz in most notes,
    // 108.4 in the notes resampled a semitone up; the top notes' copies
    // climb to 170 Hz with their pitch), the top mode (T(1,1)2) its
    // loudest from 1.8 to 2.8 times that (240 or 255 Hz, some 27 dB
    // under A0); the rest are the plate's. (No peaks of the open strings'
    // harmonics recur: 12 % of the modes sit within 1.5 % of one, about
    // chance, and those are A0 and T(1,1)2 copies; none are strings.) The
    // table's air and top modes: the most common ones. The grand's: its
    // board modes run 21-250 Hz (and some 290-900 Hz in the middle); in
    // the bass notes (under 60 Hz) the modes above 400 Hz are the strings'
    // longitudinal modes (0.4-1.2 kHz, up to -46 dB), which stay put.
    void classifyModes (ModalPianoTable& table)
    {
        const auto late = juce::jmin (table.lateTime + 1, (int) table.times.size() - 1); // (0.08 s)
        const auto times = (size_t) table.times.size();
        std::vector<float> airs, tops;
        for (auto& e : table.entries)
        {
            const auto modes = e.modes.size();
            e.modeKind.assign (modes, ModalPianoTable::boardMode);
            const auto level = [&] (size_t m) { return e.modeLevels[m * times + (size_t) late]; };
            if (! table.guitarBody)
            {
                if (440.0 * std::exp2 ((e.note - 69) / 12.0) < 60.0)
                    for (size_t m = 0; m < modes; ++m)
                        if (e.modes[m] > 400.0f)
                            e.modeKind[m] = ModalPianoTable::stringMode;
                continue;
            }
            const auto loudest = [&] (float lo, float hi)
            {
                auto best = -1;
                for (size_t m = 0; m < modes; ++m)
                    if (e.modes[m] >= lo && e.modes[m] <= hi && e.modeKind[m] == ModalPianoTable::boardMode
                        && (best < 0 || level (m) > level ((size_t) best)))
                        best = (int) m;
                return best;
            };
            auto air = table.airHz;
            if (const auto a = loudest (80.0f, 180.0f); a >= 0)
            {
                e.modeKind[(size_t) a] = ModalPianoTable::airMode;
                air = e.modes[(size_t) a];
                airs.push_back (air);
            }
            if (const auto t = loudest (1.8f * air, 2.8f * air); t >= 0)
            {
                e.modeKind[(size_t) t] = ModalPianoTable::topMode;
                tops.push_back (e.modes[(size_t) t]);
            }
        }
        const auto mostCommon = [] (std::vector<float> hz, float fallback)
        {
            if (hz.empty())
                return fallback;
            std::sort (hz.begin(), hz.end());
            auto best = hz.front();
            size_t bestRun = 0;
            for (size_t i = 0; i < hz.size();)
            {
                auto j = i;
                while (j < hz.size() && hz[j] == hz[i])
                    ++j;
                if (j - i > bestRun)
                {
                    bestRun = j - i;
                    best = hz[i];
                }
                i = j;
            }
            return best;
        };
        if (table.guitarBody)
        {
            table.airHz = mostCommon (airs, table.airHz);
            table.topHz = mostCommon (tops, table.topHz);
        }
    }

    ModalPianoTable parse (const char* data, int size, bool guitarBody)
    {
        ModalPianoTable table;
        Reader in { reinterpret_cast<const unsigned char*> (data), (size_t) size };
        if (! in.has (14) || std::memcmp (in.data, "IMPT", 4) != 0)
            return table;
        in.position = 4;
        const auto version = in.read<std::uint16_t>();
        const auto count = in.read<std::uint16_t>();
        const auto timeCount = in.read<std::uint16_t>();
        table.rate = in.read<float>();
        if (version < 1 || version > 5 || timeCount == 0)
            return {};
        for (int t = 0; t < timeCount; ++t)
            table.times.push_back (in.read<float>());
        table.modelledPluck = version >= 5;
        while (table.lateTime < timeCount - 1 && table.times[(size_t) table.lateTime] < 0.0499f)
            ++table.lateTime;

        std::vector<ModalPianoTable::Entry> read;
        for (int e = 0; e < count; ++e)
        {
            ModalPianoTable::Entry entry;
            entry.note = in.read<std::uint16_t>();
            entry.velocity = in.read<std::uint16_t>();
            entry.partials = in.read<std::uint16_t>();
            const auto attackLength = in.read<std::uint32_t>();
            const auto scale = in.read<float>();
            if (! in.has ((size_t) entry.partials * (4 + 2 * (size_t) timeCount) + (size_t) attackLength * 2))
                return {};
            for (int p = 0; p < entry.partials; ++p)
                entry.ratios.push_back (in.read<float>());
            for (int i = 0; i < entry.partials * timeCount; ++i)
                entry.levels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            for (std::uint32_t i = 0; i < attackLength; ++i)
                entry.attack.push_back ((float) in.read<std::int16_t>() / 32767.0f * scale);
            if (version >= 2)
            {
                const auto modes = in.read<std::uint16_t>();
                if (! in.has ((size_t) modes * (4 + 2 * (size_t) timeCount)))
                    return {};
                for (int m = 0; m < modes; ++m)
                    entry.modes.push_back (in.read<float>());
                for (int i = 0; i < modes * timeCount; ++i)
                    entry.modeLevels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            }
            if (version >= 3)
            {
                const auto bands = in.read<std::uint16_t>();
                if (bands != ModalPianoTable::noiseBandCount || ! in.has ((size_t) bands * 2 * (size_t) timeCount))
                    return {};
                for (int i = 0; i < bands * timeCount; ++i)
                    entry.noiseLevels.push_back ((float) in.read<std::int16_t>() / 64.0f);
            }
            if (version >= 4)
            {
                const auto rightScale = in.read<float>();
                const auto pictures = (size_t) entry.partials + entry.modes.size();
                if (! in.has ((size_t) attackLength * 2 + pictures * 4))
                    return {};
                for (std::uint32_t i = 0; i < attackLength; ++i)
                    entry.attackRight.push_back ((float) in.read<std::int16_t>() / 32767.0f * rightScale);
                const auto readPicture = [&in] (size_t count, std::vector<float>& balance, std::vector<float>& phase)
                {
                    for (size_t i = 0; i < count; ++i)
                        balance.push_back ((float) in.read<std::int16_t>() / 64.0f);
                    for (size_t i = 0; i < count; ++i)
                        phase.push_back ((float) in.read<std::int16_t>() / 10000.0f);
                };
                readPicture ((size_t) entry.partials, entry.balance, entry.phase);
                readPicture (entry.modes.size(), entry.modeBalance, entry.modePhase);
            }
            else
            {
                entry.attackRight = entry.attack;
                entry.balance.assign ((size_t) entry.partials, 0.0f);
                entry.phase.assign ((size_t) entry.partials, 0.0f);
                entry.modeBalance.assign (entry.modes.size(), 0.0f);
                entry.modePhase.assign (entry.modes.size(), 0.0f);
            }
            if (version >= 5)
            {
                constexpr auto bands = (size_t) ModalPianoTable::noiseBandCount;
                if (! in.has (8 + ((size_t) entry.partials + entry.modes.size()) * 2 + bands * 8))
                    return {};
                entry.pluckMs = in.read<float>();
                entry.riseMs = in.read<float>();
                const auto readPhases = [&in] (size_t count, std::vector<float>& phases)
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        const auto v = in.read<std::int16_t>();
                        phases.push_back (v == -32768 ? ModalPianoTable::noPhase : (float) v / 10000.0f);
                    }
                };
                readPhases ((size_t) entry.partials, entry.startPhase);
                readPhases (entry.modes.size(), entry.modeStartPhase);
                for (size_t b = 0; b < bands; ++b)
                    entry.burstDb.push_back (in.read<float>());
                for (size_t b = 0; b < bands; ++b)
                    entry.burstFall.push_back (in.read<float>());
            }
            read.push_back (std::move (entry));
        }

        for (const auto& entry : read)
        {
            if (std::find (table.notes.begin(), table.notes.end(), entry.note) == table.notes.end())
                table.notes.push_back (entry.note);
            if (std::find (table.velocities.begin(), table.velocities.end(), entry.velocity) == table.velocities.end())
                table.velocities.push_back (entry.velocity);
        }
        std::sort (table.notes.begin(), table.notes.end());
        std::sort (table.velocities.begin(), table.velocities.end());

        // Every note at every velocity, or nothing.
        table.entries.resize (table.notes.size() * table.velocities.size());
        std::vector<bool> filled (table.entries.size(), false);
        for (auto& entry : read)
        {
            const auto n = (size_t) (std::find (table.notes.begin(), table.notes.end(), entry.note) - table.notes.begin());
            const auto v = (size_t) (std::find (table.velocities.begin(), table.velocities.end(), entry.velocity) - table.velocities.begin());
            const auto index = v * table.notes.size() + n;
            filled[index] = true;
            table.entries[index] = std::move (entry);
        }
        if (std::find (filled.begin(), filled.end(), false) != filled.end())
            return {};
        buildBodyCurve (table);
        table.guitarBody = guitarBody;
        classifyModes (table);
        for (auto& e : table.entries)
        {
            const auto n = juce::jmin (e.attack.size(), e.attackRight.size(), (size_t) (0.08 * table.rate));
            auto power = 1.0e-20;
            for (size_t i = 0; i < n; ++i)
                power += 0.5 * ((double) e.attack[i] * e.attack[i] + (double) e.attackRight[i] * e.attackRight[i]);
            e.attackDb = (float) (10.0 * std::log10 (power / (double) juce::jmax ((size_t) 1, n)));

            // Each partial's sine and cosine parts over 20-80 ms (Hann).
            const auto lo = (size_t) (0.02 * table.rate), hi = juce::jmin (e.attack.size(), (size_t) (0.08 * table.rate));
            if (hi <= lo + 16)
                continue;
            const auto f0 = 440.0 * std::exp2 ((e.note - 69) / 12.0);
            e.attackSin.assign ((size_t) e.partials, 0.0f);
            e.attackCos.assign ((size_t) e.partials, 0.0f);
            // (The window once per entry, not once per partial: the same
            // values, so the sums are bit for bit what they were; parse at
            // the first plugin load ran 34 million cosines for it.)
            const auto span = (double) (hi - lo);
            std::vector<double> hannAt (hi - lo);
            for (size_t i = lo; i < hi; ++i)
                hannAt[i - lo] = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - lo) / span);
            for (int k = 0; k < e.partials; ++k)
            {
                const auto w = juce::MathConstants<double>::twoPi * e.ratios[(size_t) k] * f0 / table.rate;
                if (w >= juce::MathConstants<double>::pi)
                    break;
                // (The sine and cosine by rotation: no sin/cos per sample.)
                auto c = std::cos (w * (double) lo), s = std::sin (w * (double) lo);
                const auto dc = std::cos (w), ds = std::sin (w);
                auto sumSin = 0.0, sumCos = 0.0;
                for (size_t i = lo; i < hi; ++i)
                {
                    const auto hann = hannAt[i - lo];
                    sumSin += hann * e.attack[i] * s;
                    sumCos += hann * e.attack[i] * c;
                    const auto nc = c * dc - s * ds;
                    s = s * dc + c * ds;
                    c = nc;
                }
                e.attackSin[(size_t) k] = (float) sumSin;
                e.attackCos[(size_t) k] = (float) sumCos;
            }
        }
        return table;
    }
}

const ModalPianoTable& ModalPianoTable::get()
{
    static const ModalPianoTable table = parse (WavetableData::modalpiano_bin, WavetableData::modalpiano_binSize, false);
    return table;
}

const ModalPianoTable& ModalPianoTable::guitar()
{
    static const ModalPianoTable table = []
    {
        auto t = parse (WavetableData::modalguitar_bin, WavetableData::modalguitar_binSize, true);
        t.nearestAttack = true;
        return t;
    }();
    return table;
}
