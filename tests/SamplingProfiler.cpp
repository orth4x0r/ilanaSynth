// See SamplingProfiler.h. Kept in its own file so <windows.h> stays out of
// the tests.

#ifdef _WIN32

#include "SamplingProfiler.h"

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct SamplingProfiler::Impl
{
    static constexpr int maxFrames = 24;

    struct Sample
    {
        int numFrames = 0;
        DWORD64 frames[maxFrames] {};
    };

    // Samples the calling thread until stop(). Nothing here may allocate
    // while the thread is suspended (it could be holding the heap lock), so
    // the sample buffer is reserved up front.
    void start()
    {
        HANDLE target = nullptr;
        DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &target,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
        samples.reserve (400000);
        running = true;

        sampler = std::thread ([this, target]
        {
            timeBeginPeriod (1);

            while (running && samples.size() < samples.capacity())
            {
                Sleep (1);

                if (SuspendThread (target) == (DWORD) -1)
                    continue;

                CONTEXT context {};
                context.ContextFlags = CONTEXT_FULL;

                if (GetThreadContext (target, &context))
                {
                    Sample sample;

                    while (sample.numFrames < maxFrames && context.Rip != 0)
                    {
                        sample.frames[sample.numFrames++] = context.Rip;
                        DWORD64 imageBase = 0;

                        if (auto* function = RtlLookupFunctionEntry (context.Rip, &imageBase, nullptr))
                        {
                            void* handlerData = nullptr;
                            DWORD64 establisherFrame = 0;
                            RtlVirtualUnwind (UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
                                              &handlerData, &establisherFrame, nullptr);
                        }
                        else
                        {
                            // A leaf function: the return address is on top.
                            context.Rip = *reinterpret_cast<const DWORD64*> (context.Rsp);
                            context.Rsp += 8;
                        }
                    }

                    samples.push_back (sample);
                }

                ResumeThread (target);
            }

            timeEndPeriod (1);
            CloseHandle (target);
        });
    }

    void stop()
    {
        running = false;
        sampler.join();
    }

    void report (int topFunctions, int topLines) const
    {
        const auto process = GetCurrentProcess();
        SymSetOptions (SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize (process, nullptr, TRUE);

        std::map<DWORD64, std::string> names;

        const auto nameOf = [&] (DWORD64 address) -> const std::string&
        {
            auto found = names.find (address);

            if (found != names.end())
                return found->second;

            alignas (SYMBOL_INFO) char buffer[sizeof (SYMBOL_INFO) + 512] {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*> (buffer);
            symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 displacement = 0;
            std::string name = SymFromAddr (process, address, &displacement, symbol) ? symbol->Name : "?";

            // Name the module too: an export near an unexported ntdll
            // routine is otherwise misleading.
            IMAGEHLP_MODULE64 module {};
            module.SizeOfStruct = sizeof (module);

            if (SymGetModuleInfo64 (process, address, &module) && module.SymType == SymExport)
                name = std::string (module.ModuleName) + "!" + name + " (export)";

            return names.emplace (address, name).first->second;
        };

        std::map<std::string, int> self, inclusive, lines;
        std::map<std::string, std::map<std::string, int>> callers;

        for (const auto& sample : samples)
        {
            if (sample.numFrames == 0)
                continue;

            const auto& leaf = nameOf (sample.frames[0]);
            ++self[leaf];

            std::set<std::string> seen;
            for (int f = 0; f < sample.numFrames; ++f)
                if (seen.insert (nameOf (sample.frames[f])).second)
                    ++inclusive[nameOf (sample.frames[f])];

            // The first caller outside the leaf's own function and the
            // runtime, for "who calls this" in the report.
            std::string chain;
            for (int f = 1, added = 0; f < sample.numFrames && added < 3; ++f)
            {
                const auto& name = nameOf (sample.frames[f]);
                if (name == leaf)
                    continue;
                chain += (added++ == 0 ? "" : " <- ") + name;
            }
            ++callers[leaf][chain];

            IMAGEHLP_LINE64 line {};
            line.SizeOfStruct = sizeof (line);
            DWORD lineDisplacement = 0;

            if (SymGetLineFromAddr64 (process, sample.frames[0], &lineDisplacement, &line))
            {
                std::string file = line.FileName;
                const auto slash = file.find_last_of ("\\/");
                ++lines[(slash != std::string::npos ? file.substr (slash + 1) : file) + ":" + std::to_string (line.LineNumber)];
            }
        }

        SymCleanup (process);

        const auto sorted = [] (const std::map<std::string, int>& counts)
        {
            std::vector<std::pair<int, std::string>> list;
            for (const auto& [key, count] : counts)
                list.emplace_back (count, key);
            std::sort (list.rbegin(), list.rend());
            return list;
        };

        const auto percent = [this] (int count)
        {
            return std::to_string (100.0 * count / (double) samples.size()).substr (0, 5) + "%  ";
        };

        std::cout << "Self time (" << samples.size() << " samples)" << std::endl;
        for (const auto& [count, name] : sorted (self))
        {
            std::cout << "  " << percent (count) << name << std::endl;

            const auto chains = sorted (callers.at (name));
            for (int c = 0; c < (std::min) (3, (int) chains.size()); ++c)
                std::cout << "        " << percent (chains[(size_t) c].first) << "<- " << chains[(size_t) c].second << std::endl;

            if (--topFunctions <= 0)
                break;
        }

        std::cout << "Inclusive time" << std::endl;
        auto shown = 0;
        for (const auto& [count, name] : sorted (inclusive))
        {
            std::cout << "  " << percent (count) << name << std::endl;
            if (++shown >= 30)
                break;
        }

        std::cout << "Lines" << std::endl;
        shown = 0;
        for (const auto& [count, name] : sorted (lines))
        {
            std::cout << "  " << percent (count) << name << std::endl;
            if (++shown >= topLines)
                break;
        }
    }

    std::atomic<bool> running { false };
    std::thread sampler;
    std::vector<Sample> samples;
};

SamplingProfiler::SamplingProfiler() : impl (std::make_unique<Impl>()) {}
SamplingProfiler::~SamplingProfiler() = default;
void SamplingProfiler::start() { impl->start(); }
void SamplingProfiler::stop() { impl->stop(); }
void SamplingProfiler::report (int topFunctions, int topLines) const { impl->report (topFunctions, topLines); }

#endif
