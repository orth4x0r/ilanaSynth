// Airwindows TapeDust by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/TapeDust: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class TapeDust final : public Algorithm
{
public:
    enum { kParamA = 0, kParamB = 1, kNumParameters = 2 };
    TapeDust()
    {
        A = 0.0f;
        B = 1.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();

        for(int count = 0; count < 11; count++) {
            bL[count] = 0.0;
            fL[count] = 0.0;
            bR[count] = 0.0;
            fR[count] = 0.0;
        }

        fpdL = 1.0; while (fpdL < 16386) fpdL = awRand()*UINT32_MAX;
        fpdR = 1.0; while (fpdR < 16386) fpdR = awRand()*UINT32_MAX;
        fpFlip = true;
        //this is reset: values being initialized only once. Startup values, whatever they are.

    }

    void setParam (int index, float value) override
    {
        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        switch (index)
        {
            case 0: A = value; break;
            case 1: B = value; break;

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
        double inputSampleL;
        double inputSampleR;

        double drySampleL;
        double drySampleR;
        double rRange = pow(A,2)*5.0;
        double xfuzz = rRange * 0.002;
        double rOffset = (rRange*0.4) + 1.0;
        double rDepthL; //the randomly fluctuating value
        double rDepthR; //the randomly fluctuating value
        double gainL;
        double gainR;
        double wet = B;
        //removed extra dry variable

        while (--sampleFrames >= 0)
        {
            inputSampleL = *in1;
            inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;

            for(int count = 9; count < 0; count--) {
                bL[count+1] = bL[count];
                bR[count+1] = bR[count];
            }

            bL[0] = inputSampleL;
            bR[0] = inputSampleR;
            inputSampleL = awRand() / (double)RAND_MAX;
            inputSampleR = awRand() / (double)RAND_MAX;
            gainL = rDepthL = (inputSampleL * rRange) + rOffset;
            gainR = rDepthR = (inputSampleR * rRange) + rOffset;
            inputSampleL *= ((1.0-fabs(bL[0]-bL[1]))*xfuzz);
            inputSampleR *= ((1.0-fabs(bR[0]-bR[1]))*xfuzz);
            if (fpFlip) {
                inputSampleL = -inputSampleL;
                inputSampleR = -inputSampleR;
            }
            fpFlip = !fpFlip;

            for(int count = 0; count < 9; count++) {
                if (gainL > 1.0) {
                    fL[count] = 1.0;
                    gainL -= 1.0;
                } else {
                    fL[count] = gainL;
                    gainL = 0.0;
                }
                if (gainR > 1.0) {
                    fR[count] = 1.0;
                    gainR -= 1.0;
                } else {
                    fR[count] = gainR;
                    gainR = 0.0;
                }
                fL[count] /= rDepthL;
                fR[count] /= rDepthR;
                inputSampleL += (bL[count] * fL[count]);
                inputSampleR += (bR[count] * fR[count]);
            }

            if (wet < 1.0) {
                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
            }

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
    bool fpFlip;

    double bL[11];
    double fL[11];
    double bR[11];
    double fR[11];

    float A;
    float B;
};
} // namespace airwindows
