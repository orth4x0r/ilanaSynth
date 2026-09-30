// Airwindows Wider by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Wider: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Wider final : public Algorithm
{
public:
    enum { kParamA = 0, kParamB = 1, kParamC = 2, kNumParameters = 3 };
    Wider()
    {
        A = 0.5f;
        B = 0.5f;
        C = 1.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        for(int fcount = 0; fcount < 4098; fcount++) {p[fcount] = 0.0;}
        count = 0;
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

        double inputSampleL;
        double inputSampleR;
        double drySampleL;
        double drySampleR;
        double mid;
        double side;
        double out;
        double densityside = (A*2.0)-1.0;
        double densitymid = (B*2.0)-1.0;
        double wet = C;
        //removed extra dry variable
        wet *= 0.5; //we make mid-side by adding/subtracting both channels into each channel
        //and that's why we gotta divide it by 2: otherwise everything's doubled. So, premultiply it to save an extra 'math'
        double offset = (densityside-densitymid)/2;
        if (offset > 0) offset = sin(offset);
        if (offset < 0) offset = -sin(-offset);
        offset = -(pow(offset,4) * 20 * overallscale);
        int nearTap = (int)floor(fabs(offset));
        double farTapLevel = fabs(offset) - nearTap;
        int farTap = nearTap + 1;
        double nearTapLevel = 1.0 - farTapLevel;
        double bridgerectifier;
        //interpolating the sample

        while (--sampleFrames >= 0)
        {
            inputSampleL = *in1;
            inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;
            //assign working variables
            mid = inputSampleL + inputSampleR;
            side = inputSampleL - inputSampleR;
            //assign mid and side. Now, High Impact code

            if (densityside != 0.0)
            {
                out = fabs(densityside);
                bridgerectifier = fabs(side)*1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                //max value for sine function
                if (densityside > 0) bridgerectifier = sin(bridgerectifier);
                else bridgerectifier = 1-cos(bridgerectifier);
                //produce either boosted or starved version
                if (side > 0) side = (side*(1-out))+(bridgerectifier*out);
                else side = (side*(1-out))-(bridgerectifier*out);
                //blend according to density control
            }

            if (densitymid != 0.0)
            {
                out = fabs(densitymid);
                bridgerectifier = fabs(mid)*1.57079633;
                if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
                //max value for sine function
                if (densitymid > 0) bridgerectifier = sin(bridgerectifier);
                else bridgerectifier = 1-cos(bridgerectifier);
                //produce either boosted or starved version
                if (mid > 0) mid = (mid*(1-out))+(bridgerectifier*out);
                else mid = (mid*(1-out))-(bridgerectifier*out);
                //blend according to density control
            }

            if (count < 1 || count > 2048) {count = 2048;}
            if (offset > 0)
            {
                p[count+2048] = p[count] = mid;
                mid = p[count+nearTap]*nearTapLevel;
                mid += p[count+farTap]*farTapLevel;
            }

            if (offset < 0)
            {
                p[count+2048] = p[count] = side;
                side = p[count+nearTap]*nearTapLevel;
                side += p[count+farTap]*farTapLevel;
            }
            count -= 1;

            inputSampleL = (drySampleL * (1.0-wet)) + ((mid+side) * wet);
            inputSampleR = (drySampleR * (1.0-wet)) + ((mid-side) * wet);

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

    double p[4099];
    int count;

    float A;
    float B;
    float C;
};
} // namespace airwindows
