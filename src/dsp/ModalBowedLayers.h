#pragma once

// (Round 11, the violin's) The Modal Bowed residual layers' noise loops
// (ModalBowed.h). No audio is stored: each loop is synthesised from the
// per-note band levels in ModalBowedNotes.h (residualDb), with a random phase
// per bin fixed by the note, layer and loop, by one inverse FFT (mixed radix:
// 16384 = 2 ^ 14, 14641 = 11 ^ 4). Built once per process and sample rate on
// a background thread, from the first prepare, shared by every voice and
// plugin instance and freed at exit; each note is readable once its ready flag
// is set (a voice whose note isn't ready plays without the layers, as round 10
// did). Offline renders call waitUntilBuilt() so every render is the same.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ModalBowedNotes.h"

class ModalBowedLayers
{
public:
    using Notes = ModalBowedNotes;
    static constexpr int layers = Notes::residualLayers;
    // (A loop jumps to a random place once a length, crossfading over this.)
    static constexpr double fadeSeconds = 0.015;

    // One table row's three layers: two loops each (float, in the engine's output units).
    struct Note
    {
        std::vector<float> loop[layers][2];
        float rms[layers] {};   // (of the two loops summed)
        bool any[layers] {};
    };

    // All the residual tables' rows at one sample rate.
    struct Bank
    {
        double rate = 0.0;
        int count = 0;
        std::unique_ptr<Note[]> notes;
        std::unique_ptr<std::atomic<bool>[]> ready;
        std::atomic<bool> done { false };
        // The crossfade when a loop jumps (fadeSeconds): equal power, sin of
        // a quarter turn over fadeLength samples (cos is it read backwards).
        int fadeLength = 1;
        std::vector<float> fadeIn;

        // (any thread) The row's layers once built, else nullptr.
        const Note* get (int table, int row) const
        {
            const auto i = offset (table) + row;
            return i >= 0 && i < count && ready[(size_t) i].load (std::memory_order_acquire) ? &notes[(size_t) i] : nullptr;
        }

        static int offset (int table)
        {
            auto sum = 0;
            for (int i = 0; i < table; ++i)
                if (Notes::tables[i].residual)
                    sum += Notes::tables[i].last - Notes::tables[i].first + 1;
            return Notes::tables[table].residual ? sum : -1000000;
        }
    };

    // (message thread: prepare) The bank for this rate, its build started if
    // it is new. At most maxBanks rates per process (then nullptr: no layers).
    static const Bank* bankFor (double rate)
    {
        auto& b = builder();
        std::lock_guard<std::mutex> lock (b.mutex);
        for (auto& bank : b.banks)
            if (bank->rate == rate)
                return bank.get();
        if ((int) b.banks.size() >= maxBanks)
            return nullptr;
        auto bank = std::make_unique<Bank>();
        bank->rate = rate;
        bank->fadeLength = std::max (1, (int) std::lround (fadeSeconds * rate));
        bank->fadeIn.resize ((size_t) bank->fadeLength);
        for (int i = 0; i < bank->fadeLength; ++i)
            bank->fadeIn[(size_t) i] = (float) std::sin (0.5 * 3.14159265358979323846 * ((double) i + 0.5) / (double) bank->fadeLength);
        for (int i = 0; i < Notes::tableCount; ++i)
            if (Notes::tables[i].residual)
                bank->count += Notes::tables[i].last - Notes::tables[i].first + 1;
        bank->notes.reset (new Note[(size_t) bank->count]);
        bank->ready.reset (new std::atomic<bool>[(size_t) bank->count]);
        for (int i = 0; i < bank->count; ++i)
            bank->ready[(size_t) i].store (false);
        b.banks.push_back (std::move (bank));
        b.idle.store (false);   // (before the thread wakes: waitUntilBuilt waits for this one)
        b.wake.notify_all();
        if (! b.thread.joinable())
            b.thread = std::thread ([&b] { b.run(); });
        return b.banks.back().get();
    }

    // (offline) Waits until every bank asked for is built.
    static void waitUntilBuilt()
    {
        auto& b = builder();
        if (b.idle.load (std::memory_order_acquire))
            return;
        std::unique_lock<std::mutex> lock (b.mutex);
        b.wake.wait (lock, [&b] { return b.idle.load() || b.cancel.load(); });
    }

    // Synthesises one note's layers (exposed for the tests and the benchmark).
    static void build (Note& note, int table, int row, double rate)
    {
        const auto& r = Notes::tables[table].rows[row];
        const auto midi = Notes::tables[table].first + row;
        for (int l = 0; l < layers; ++l)
        {
            auto any = false;
            for (int k = 0; k < Notes::residualBands; ++k)
                any = any || r.residualDb[l][k] > -150.0f;
            note.any[l] = any;
            if (! any)
                continue;
            auto sumSquares = 0.0;
            for (int which = 0; which < 2; ++which)
            {
                const auto length = Notes::residualLengths[which];
                // (the twin's seed: the note, the layer and the loop)
                synthesise (note.loop[l][which], r.residualDb[l], which, length, rate,
                            (std::uint64_t) (midi * 10 * 3 + l) * 7u + (std::uint64_t) length);
            }
            // (the two loops are coprime: their sum's mean square is the sum of theirs)
            for (int which = 0; which < 2; ++which)
                for (auto x : note.loop[l][which])
                    sumSquares += (double) x * x / (double) note.loop[l][which].size();
            note.rms[l] = (float) std::sqrt (sumSquares);
        }
    }

    // Bytes held by the built banks (for the report).
    static std::size_t bytes()
    {
        auto& b = builder();
        std::lock_guard<std::mutex> lock (b.mutex);
        std::size_t total = 0;
        for (auto& bank : b.banks)
            for (int i = 0; i < bank->count; ++i)
                for (auto& layer : bank->notes[(size_t) i].loop)
                    for (auto& loop : layer)
                        total += loop.capacity() * sizeof (float);
        return total;
    }

private:
    static constexpr int maxBanks = 4;

    struct Builder
    {
        std::mutex mutex;
        std::condition_variable wake;
        std::vector<std::unique_ptr<Bank>> banks;
        std::thread thread;
        std::atomic<bool> cancel { false }, idle { true }, finished { false };

        void run()
        {
            for (;;)
            {
                Bank* bank = nullptr;
                {
                    std::unique_lock<std::mutex> lock (mutex);
                    for (;;)
                    {
                        for (auto& b : banks)
                            if (! b->done.load())
                            {
                                bank = b.get();
                                break;
                            }
                        if (bank != nullptr || cancel.load())
                            break;
                        idle.store (true, std::memory_order_release);
                        wake.notify_all();
                        wake.wait (lock);
                    }
                    if (bank == nullptr)
                        break;
                    idle.store (false);
                }
                auto i = 0;
                for (int t = 0; t < Notes::tableCount && ! cancel.load(); ++t)
                    if (Notes::tables[t].residual)
                        for (int row = 0; row <= Notes::tables[t].last - Notes::tables[t].first && ! cancel.load(); ++row, ++i)
                        {
                            build (bank->notes[(size_t) i], t, row, bank->rate);
                            bank->ready[(size_t) i].store (true, std::memory_order_release);
                        }
                bank->done.store (true);
            }
            idle.store (true);
            wake.notify_all();
            finished.store (true, std::memory_order_release);
        }

        Builder() { idle.store (true); }

        ~Builder()
        {
            {
                std::lock_guard<std::mutex> lock (mutex);
                cancel.store (true);
            }
            wake.notify_all();
            if (thread.joinable())
            {
               #if defined (_WIN32)
                // (at a DLL's unload the loader lock is held: joining would
                // wait on a thread that can't exit; wait for it to finish its
                // work instead, then let it go)
                for (int i = 0; i < 2000 && ! finished.load(); ++i)
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                thread.detach();
               #else
                thread.join();
               #endif
            }
        }
    };

    static Builder& builder()
    {
        static Builder instance;
        return instance;
    }

    // One loop: bins of rate / length Hz; a bin in band b gets amplitude
    // sqrt (10 ^ (dB / 10) / 2 / residualScale) (the score's band energy,
    // half in each loop) times sqrt (rate / 48 kHz) (the same energy per Hz
    // at any rate), a random phase; the real inverse transform over length.
    static void synthesise (std::vector<float>& out, const float* bandDb, int which, int length, double rate, std::uint64_t key)
    {
        std::vector<std::complex<double>> spectrum ((size_t) length), result ((size_t) length);
        auto state = key * 6364136223846793005ull + 1442695040888963407ull;
        const auto random = [&state]
        {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            return (double) (state >> 11) * (1.0 / 9007199254740992.0);
        };
        const auto perHz = std::sqrt (rate / 48000.0);
        auto band = 0;
        for (int j = 1; 2 * j < length; ++j)
        {
            const auto phase = 2.0 * 3.14159265358979323846 * random();
            const auto hz = (double) j * rate / (double) length;
            while (band < Notes::residualBands && hz >= Notes::residualEdges[band][1])
                ++band;
            if (band >= Notes::residualBands)
                break;
            if (hz < Notes::residualEdges[band][0] || bandDb[band] <= -150.0f)
                continue;
            const auto amplitude = std::sqrt (std::pow (10.0, (double) bandDb[band] / 10.0) * 0.5 / Notes::residualScale[which][band]) * perHz;
            spectrum[(size_t) j] = std::polar (amplitude, phase);
        }
        transform (spectrum.data(), result.data(), length, 1, turnFor (which).data(), 1, length);
        out.resize ((size_t) length);
        for (int n = 0; n < length; ++n)
            out[(size_t) n] = (float) (2.0 / (double) length * result[(size_t) n].real());
    }

    // turn[k] = e^{2 pi i k / length} for loop `which`'s length: made once
    // per process (it was made again for each of the 366 loops).
    static const std::vector<std::complex<double>>& turnFor (int which)
    {
        const auto make = [] (int length)
        {
            std::vector<std::complex<double>> turn ((size_t) length);
            for (int k = 0; k < length; ++k)
                turn[(size_t) k] = std::polar (1.0, 2.0 * 3.14159265358979323846 * (double) k / (double) length);
            return turn;
        };
        static const std::vector<std::complex<double>> first = make (Notes::residualLengths[0]);
        static const std::vector<std::complex<double>> second = make (Notes::residualLengths[1]);
        return which == 0 ? first : second;
    }

    // The unnormalised inverse DFT (e^{+i}) of n points read at stride, into
    // out (contiguous): decimation in time by n's smallest prime factor.
    // turn[k] = e^{2 pi i k / total}; for n points, w_n^k is turn[k * step].
    // (CPU pass 2026-11-13: the same products and sums in the same order as
    // before, so bit for bit the same loops; only the twiddles' indices are
    // found without a 64-bit modulo per product: q k step < n step = total,
    // and q s m step wraps to ((q s) mod radix) m step.)
    static void transform (const std::complex<double>* in, std::complex<double>* out, int n, int stride,
                           const std::complex<double>* turn, int step, int total)
    {
        if (n == 1)
        {
            out[0] = in[0];
            return;
        }
        auto radix = 2;
        while (n % radix != 0)
            ++radix;
        const auto m = n / radix;
        for (int q = 0; q < radix; ++q)
            transform (in + q * stride, out + q * m, m, stride * radix, turn, step * radix, total);
        std::complex<double> part[16];
        std::complex<double> sum[16];
        // root[(q s) mod radix] = turn[q s m step mod total]
        std::complex<double> root[16];
        for (int j = 0; j < radix; ++j)
            root[j] = turn[(size_t) (j * m * step)];
        for (int k = 0; k < m; ++k)
        {
            for (int q = 0; q < radix; ++q)
                part[q] = out[q * m + k] * turn[(size_t) (q * k * step)];
            for (int s = 0; s < radix; ++s)
            {
                auto value = part[0];
                for (int q = 1, j = s; q < radix; ++q, j = j + s >= radix ? j + s - radix : j + s)
                    value += part[q] * root[j];
                sum[s] = value;
            }
            for (int s = 0; s < radix; ++s)
                out[s * m + k] = sum[s];
        }
    }
};
