// Airwindows Density by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Density: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Density final : public Algorithm
{
public:
    enum { kParamA = 0, kParamB = 1, kParamC = 2, kParamD = 3, kNumParameters = 4 };
    Density()
    {
        A = 0.2f;
        B = 0.0f;
        C = 1.0f;
        D = 1.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        iirSampleAL = 0.0;
        iirSampleBL = 0.0;
        iirSampleAR = 0.0;
        iirSampleBR = 0.0;
        fpFlip = true;
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
            case 1: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
            case 2: C = value; break;
            case 3: D = value; break; //this is the popup, stored as a float

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
        double density = (A*5.0)-1.0;
        double iirAmount = pow(B,3)/overallscale;
        double output = C;
        double wet = D;
        double dry = 1.0-wet;
        double bridgerectifier;
        double out = fabs(density);
        density = density * fabs(density);
        double count;

        double inputSampleL;
        double inputSampleR;
        double drySampleL;
        double drySampleR;

        while (--sampleFrames >= 0)
        {
            inputSampleL = *in1;
            inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;

            if (fpFlip)
            {
                iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL -= iirSampleAL;
                iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR -= iirSampleAR;
            }
            else
            {
                iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL -= iirSampleBL;
                iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR -= iirSampleBR;
            }
            //highpass section
            fpFlip = !fpFlip;

            count = density;
            while (count > 1.0)
            {
                bridgerectifier = fabs(inputSampleL)*1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                //max value for sine function
                bridgerectifier = sin(bridgerectifier);
                if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
                else inputSampleL = -bridgerectifier;

                bridgerectifier = fabs(inputSampleR)*1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                //max value for sine function
                bridgerectifier = sin(bridgerectifier);
                if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
                else inputSampleR = -bridgerectifier;

                count = count - 1.0;
            }
            //we have now accounted for any really high density settings.

            while (out > 1.0) out = out - 1.0;

            bridgerectifier = fabs(inputSampleL)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (density > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
            else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
            //blend according to density control

            bridgerectifier = fabs(inputSampleR)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (density > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (inputSampleR > 0) inputSampleR = (inputSampleR*(1.0-out))+(bridgerectifier*out);
            else inputSampleR = (inputSampleR*(1.0-out))-(bridgerectifier*out);
            //blend according to density control

            if (output < 1.0) {
                inputSampleL *= output;
                inputSampleR *= output;
            }
            if (wet < 1.0) {
                inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
                inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
            }
            //nice little output stage template: if we have another scale of floating point
            //number, we really don't want to meaninglessly multiply that by 1.0.

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

    double iirSampleAL;
    double iirSampleBL;
    double iirSampleAR;
    double iirSampleBR;
    bool fpFlip;
    float A;
    float B;
    float C;
    float D; //parameters. Always 0-1, and we scale/alter them elsewhere.
};
} // namespace airwindows
