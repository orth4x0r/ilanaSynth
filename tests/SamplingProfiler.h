#pragma once

// A small sampling profiler for the test app (Windows only): a background
// thread pauses the profiled thread about once a millisecond, records its
// instruction pointer, and the report maps the samples to functions and
// source lines through the PDB. ILANA_PROFILE=<unison> runs it over the
// unison benchmark patch.

#include <memory>

class SamplingProfiler
{
public:
    SamplingProfiler();
    ~SamplingProfiler();

    // Samples the calling thread until stop().
    void start();
    void stop();
    void report (int topFunctions = 25, int topLines = 40) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
