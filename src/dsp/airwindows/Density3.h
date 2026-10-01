// Airwindows Density3 by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Density3: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Density3 final : public Algorithm
{
public:
    enum { kParamA =0, kParamB =1, kParamC =2, kParamD =3, kNumParameters = 4 };
    Density3()
    {
        A = 0.0f;
        B = 0.0f;
        C = 1.0f;
        D = 1.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        iirSampleL = 0.0;
        iirSampleR = 0.0;

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
            case 1: B = value; break;
            case 2: C = value; break;
            case 3: D = value; break;

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

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= getSampleRate();
        double density = A*5.0;
        double iirAmount = pow(B,3)/overallscale;
        if (iirAmount == 0.0) {iirSampleL = 0.0; iirSampleR = 0.0;}
        double output = C;
        double wet = D;

        while (--sampleFrames >= 0)
        {
            double inputSampleL = *in1;
            double inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            iirSampleL = (iirSampleL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
            inputSampleL -= iirSampleL;
            iirSampleR = (iirSampleR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleR -= iirSampleR;

            double altered = inputSampleL;
            if (density > 1.0) {
                altered = fmax(fmin(inputSampleL*density*M_PI_2,M_PI_2),-M_PI_2);
                double X = altered*altered;
                double temp = altered*X;
                altered -= (temp / 6.0); temp *= X;
                altered += (temp / 120.0); temp *= X;
                altered -= (temp / 5040.0); temp *= X;
                altered += (temp / 362880.0); temp *= X;
                altered -= (temp / 39916800.0);
            }
            if (density < 1.0) {
                altered = fmax(fmin(inputSampleL,1.0),-1.0);
                double polarity = altered;
                double X = inputSampleL * altered;
                double temp = X;
                altered = (temp / 2.0); temp *= X;
                altered -= (temp / 24.0); temp *= X;
                altered += (temp / 720.0); temp *= X;
                altered -= (temp / 40320.0); temp *= X;
                altered += (temp / 3628800.0);
                altered *= ((polarity<0.0)?-1.0:1.0);
            }
            if (density > 2.0) inputSampleL = altered;
            else inputSampleL = (inputSampleL*(1.0-fabs(density-1.0)))+(altered*fabs(density-1.0));

            altered = inputSampleR;
            if (density > 1.0) {
                altered = fmax(fmin(inputSampleR*density*M_PI_2,M_PI_2),-M_PI_2);
                double X = altered*altered;
                double temp = altered*X;
                altered -= (temp / 6.0); temp *= X;
                altered += (temp / 120.0); temp *= X;
                altered -= (temp / 5040.0); temp *= X;
                altered += (temp / 362880.0); temp *= X;
                altered -= (temp / 39916800.0);
            }
            if (density < 1.0) {
                altered = fmax(fmin(inputSampleR,1.0),-1.0);
                double polarity = altered;
                double X = inputSampleR * altered;
                double temp = X;
                altered = (temp / 2.0); temp *= X;
                altered -= (temp / 24.0); temp *= X;
                altered += (temp / 720.0); temp *= X;
                altered -= (temp / 40320.0); temp *= X;
                altered += (temp / 3628800.0);
                altered *= ((polarity<0.0)?-1.0:1.0);
            }
            if (density > 2.0) inputSampleR = altered;
            else inputSampleR = (inputSampleR*(1.0-fabs(density-1.0)))+(altered*fabs(density-1.0));

            inputSampleL = (drySampleL*(1.0-wet))+(inputSampleL*output*wet);
            inputSampleR = (drySampleR*(1.0-wet))+(inputSampleR*output*wet);

            //begin 32 bit stereo floating point dither
            int expon; frexpf((float)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
            frexpf((float)inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            if (fpdL-fpdR < 1073741824 || fpdR-fpdL < 1073741824) {
                fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;}
            inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 3.553e-44l * pow(2,expon+62));
            //end 32 bit stereo floating point dither

            *out1 = inputSampleL;
            *out2 = inputSampleR;

            in1++;
            in2++;
            out1++;
            out2++;
        }
    }

    float A;
    float B;
    float C;
    float D;

    double iirSampleL;
    double iirSampleR;

    uint32_t fpdL;
    uint32_t fpdR;
};
} // namespace airwindows
