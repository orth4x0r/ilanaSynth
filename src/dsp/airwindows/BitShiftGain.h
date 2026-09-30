// Airwindows BitShiftGain by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/BitShiftGain: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class BitShiftGain final : public Algorithm
{
public:
    enum { kParamA = 0, kNumParameters = 1 };
    BitShiftGain()
    {
        A = 0.5f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        //this is reset: values being initialized only once. Startup values, whatever they are.

    }

    void setParam (int index, float value) override
    {
        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        switch (index)
        {
            case 0: A = value; break;

            default: break;
        }
    }

    void process (float* left, float* right, int numSamples) override
    {
        float* inputs[2] { left, right };
        float* outputs[2] { left, right };
        processReplacing (inputs, outputs, numSamples);
    }

private:
    void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
    {
        float* in1  =  inputs[0];
        float* in2  =  inputs[1];
        float* out1 = outputs[0];
        float* out2 = outputs[1];

        int bitshiftGain = (A * 32)-16;
        double gain = 1.0;
        switch (bitshiftGain)
        {
            case -16: gain = 0.0000152587890625; break;
            case -15: gain = 0.000030517578125; break;
            case -14: gain = 0.00006103515625; break;
            case -13: gain = 0.0001220703125; break;
            case -12: gain = 0.000244140625; break;
            case -11: gain = 0.00048828125; break;
            case -10: gain = 0.0009765625; break;
            case -9: gain = 0.001953125; break;
            case -8: gain = 0.00390625; break;
            case -7: gain = 0.0078125; break;
            case -6: gain = 0.015625; break;
            case -5: gain = 0.03125; break;
            case -4: gain = 0.0625; break;
            case -3: gain = 0.125; break;
            case -2: gain = 0.25; break;
            case -1: gain = 0.5; break;
            case 0: gain = 1.0; break;
            case 1: gain = 2.0; break;
            case 2: gain = 4.0; break;
            case 3: gain = 8.0; break;
            case 4: gain = 16.0; break;
            case 5: gain = 32.0; break;
            case 6: gain = 64.0; break;
            case 7: gain = 128.0; break;
            case 8: gain = 256.0; break;
            case 9: gain = 512.0; break;
            case 10: gain = 1024.0; break;
            case 11: gain = 2048.0; break;
            case 12: gain = 4096.0; break;
            case 13: gain = 8192.0; break;
            case 14: gain = 16384.0; break;
            case 15: gain = 32768.0; break;
            case 16: gain = 65536.0; break;
        }
        //we are directly punching in the gain values rather than calculating them

        while (--sampleFrames >= 0)
        {
            *out1 = *in1 * gain;
            *out2 = *in2 * gain;

            *in1++;
            *in2++;
            *out1++;
            *out2++;
        }
    }

    float A;
};
} // namespace airwindows
