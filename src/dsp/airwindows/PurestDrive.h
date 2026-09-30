// Airwindows PurestDrive by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/PurestDrive: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class PurestDrive final : public Algorithm
{
public:
    enum { kParamA = 0, kNumParameters = 1 };
    PurestDrive()
    {
        A = 0.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        previousSampleL = 0.0;
        previousSampleR = 0.0;

        fpdL = 1.0; while (fpdL < 16386) fpdL = awRand()*UINT32_MAX;
        fpdR = 1.0; while (fpdR < 16386) fpdR = awRand()*UINT32_MAX;
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

        double intensity = A;
        double drySampleL;
        double drySampleR;
        double inputSampleL;
        double inputSampleR;
        double apply;

        while (--sampleFrames >= 0)
        {
            inputSampleL = *in1;
            inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;

            inputSampleL = sin(inputSampleL);
            //basic distortion factor
            apply = (fabs(previousSampleL + inputSampleL) / 2.0) * intensity;
            //saturate less if previous sample was undistorted and low level, or if it was
            //inverse polarity. Lets through highs and brightness more.
            inputSampleL = (drySampleL * (1.0 - apply)) + (inputSampleL * apply);
            //dry-wet control for intensity also has FM modulation to clean up highs
            previousSampleL = sin(drySampleL);
            //apply the sine while storing previous sample

            inputSampleR = sin(inputSampleR);
            //basic distortion factor
            apply = (fabs(previousSampleR + inputSampleR) / 2.0) * intensity;
            //saturate less if previous sample was undistorted and low level, or if it was
            //inverse polarity. Lets through highs and brightness more.
            inputSampleR = (drySampleR * (1.0 - apply)) + (inputSampleR * apply);
            //dry-wet control for intensity also has FM modulation to clean up highs
            previousSampleR = sin(drySampleR);
            //apply the sine while storing previous sample

            //begin 32 bit stereo floating point dither
            int expon; frexpf((float)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
            frexpf((float)inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
            //end 32 bit stereo floating point dither

            *out1 = inputSampleL;
            *out2 = inputSampleR;

            *in1++;
            *in2++;
            *out1++;
            *out2++;
        }
    }

    uint32_t fpdL;
    uint32_t fpdR;

    double previousSampleL;
    double previousSampleR;

    float A;
    float B;
    float C;
    float D;
    float E; //parameters. Always 0-1, and we scale/alter them elsewhere.
};
} // namespace airwindows
