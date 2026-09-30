// Airwindows Spiral2 by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Spiral2: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Spiral2 final : public Algorithm
{
public:
    enum { kParamA = 0, kParamB = 1, kParamC = 2, kParamD = 3, kParamE = 4, kNumParameters = 5 };
    Spiral2()
    {
        A = 0.5f;
        B = 0.0f;
        C = 0.5f;
        D = 1.0f;
        E = 1.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        iirSampleAL = 0.0;
        iirSampleBL = 0.0;
        prevSampleL = 0.0;
        fpdL = 1.0; while (fpdL < 16386) fpdL = awRand()*UINT32_MAX;
        fpdR = 1.0; while (fpdR < 16386) fpdR = awRand()*UINT32_MAX;

        iirSampleAR = 0.0;
        iirSampleBR = 0.0;
        prevSampleR = 0.0;
        flip = true;
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
            case 4: E = value; break;

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

        double gain = pow(A*2.0,2.0);
        double iirAmount = pow(B,3.0)/overallscale;
        double presence = C;
        double output = D;
        double wet = E;

        while (--sampleFrames >= 0)
        {
            double inputSampleL = *in1;
            double inputSampleR = *in2;

            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            if (gain != 1.0) {
                inputSampleL *= gain;
                inputSampleR *= gain;
                prevSampleL *= gain;
                prevSampleR *= gain;
            }

            if (flip)
            {
                iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleL -= iirSampleAL;
                inputSampleR -= iirSampleAR;
            }
            else
            {
                iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
                iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleL -= iirSampleBL;
                inputSampleR -= iirSampleBR;
            }
            //highpass section

            double presenceSampleL = sin(inputSampleL * fabs(prevSampleL)) / ((prevSampleL == 0.0) ?1:fabs(prevSampleL));
            double presenceSampleR = sin(inputSampleR * fabs(prevSampleR)) / ((prevSampleR == 0.0) ?1:fabs(prevSampleR));
            //change from first Spiral: delay of one sample on the scaling factor.
            inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((fabs(inputSampleL) == 0.0) ?1:fabs(inputSampleL));
            inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((fabs(inputSampleR) == 0.0) ?1:fabs(inputSampleR));

            if (output < 1.0) {
                inputSampleL *= output;
                inputSampleR *= output;
                presenceSampleL *= output;
                presenceSampleR *= output;
            }
            if (presence > 0.0) {
                inputSampleL = (inputSampleL * (1.0-presence)) + (presenceSampleL * presence);
                inputSampleR = (inputSampleR * (1.0-presence)) + (presenceSampleR * presence);
            }
            if (wet < 1.0) {
                inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
                inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
            }
            //nice little output stage template: if we have another scale of floating point
            //number, we really don't want to meaninglessly multiply that by 1.0.

            prevSampleL = drySampleL;
            prevSampleR = drySampleR;
            flip = !flip;

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

    double iirSampleAL;
    double iirSampleBL;
    double prevSampleL;
    uint32_t fpdL;
    uint32_t fpdR;

    double iirSampleAR;
    double iirSampleBR;
    double prevSampleR;
    bool flip;

    float A;
    float B;
    float C;
    float D;
    float E; //parameters. Always 0-1, and we scale/alter them elsewhere.
};
} // namespace airwindows
