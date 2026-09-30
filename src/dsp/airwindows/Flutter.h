// Airwindows Flutter by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Flutter: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Flutter final : public Algorithm
{
public:
    enum { kParamA = 0, kNumParameters = 1 };
    Flutter()
    {
        A = 0.0f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        for (int temp = 0; temp < 1001; temp++) {dL[temp] = 0.0;dR[temp] = 0.0;}
        gcount = 0;
        sweep = M_PI;
        rateof = 0.5;
        nextmax = 0.5;
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

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= getSampleRate();

        double depth = pow(A,2)*overallscale*70;
        double fluttertrim = (0.0024*pow(A,2))/overallscale;

        while (--sampleFrames >= 0)
        {
            double inputSampleL = *in1;
            double inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

            if (gcount < 0 || gcount > 999) gcount = 999;
            dL[gcount] = inputSampleL; dR[gcount] = inputSampleR;
            int count = gcount;
            double offset = depth + (depth * pow(rateof,2) * sin(sweep));
            count += (int)floor(offset);

            inputSampleL = (dL[count-((count > 999)?1000:0)] * (1-(offset-floor(offset))));
            inputSampleL += (dL[count+1-((count+1 > 999)?1000:0)] * (offset-floor(offset)));
            inputSampleR = (dR[count-((count > 999)?1000:0)] * (1-(offset-floor(offset))));
            inputSampleR += (dR[count+1-((count+1 > 999)?1000:0)] * (offset-floor(offset)));

            rateof = (rateof * (1.0-fluttertrim)) + (nextmax * fluttertrim);
            sweep += rateof * fluttertrim;
            if (sweep >= (M_PI*2.0)) {sweep -= M_PI; nextmax = 0.24 + (fpdL / (double)UINT32_MAX * 0.74);}
            //apply to input signal only when flutter is present, interpolate samples
            gcount--;

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

            in1++;
            in2++;
            out1++;
            out2++;
        }
    }

    double dL[1002];
    double dR[1002];
    int gcount;
    double rateof;
    double sweep;
    double nextmax;
    uint32_t fpdL;
    uint32_t fpdR;

    float A;
};
} // namespace airwindows
