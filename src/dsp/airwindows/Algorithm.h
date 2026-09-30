// The common shape of a ported Airwindows algorithm (Chris Johnson's plugins,
// MIT: see LICENSE.txt). Each port keeps Chris's state and per-sample code;
// this supplies what his code expects from the VST host.
#pragma once

#include <cmath>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif

#ifndef UINT32_MAX
 #define UINT32_MAX 0xffffffffu
#endif

namespace airwindows
{
using VstInt32 = int32_t;

class Algorithm
{
public:
    virtual ~Algorithm() = default;

    // The sample rate the code reads through getSampleRate(); clears the state.
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    virtual int getNumParameters() const = 0;
    // The state as the plugin's constructor leaves it (parameters kept).
    virtual void reset() = 0;
    // The plugin's own parameter index (A = 0, B = 1 ...), 0..1.
    virtual void setParam (int index, float value) = 0;
    // In place, stereo.
    virtual void process (float* left, float* right, int numSamples) = 0;

protected:
    double getSampleRate() const { return sampleRate; }

    // Stands in for rand() where the constructor seeds the dither: the same
    // 0..32767 range as MSVC's, but each instance's own, so every instance
    // and every reset starts alike.
    int awRand()
    {
        randomState = randomState * 214013u + 2531011u;
        return (int) ((randomState >> 16) & 0x7fffu);
    }

    void restartRandom() { randomState = 1u; }

private:
    double sampleRate = 44100.0;
    uint32_t randomState = 1u;
};
} // namespace airwindows
