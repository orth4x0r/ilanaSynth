// Airwindows Air3 by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Air3: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Air3 final : public Algorithm
{
public:
    enum { kParamA = 0, kParamB = 1, kNumParameters = 2 };
    Air3()
    {
        A = 0.5f;
        B = 0.5f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        for (int x = 0; x < air_total; x++) air[x] = 0.0;

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

        double airGain = A*2.0;
        if (airGain > 1.0) airGain = pow(airGain,3.0+sqrt(overallscale));
        double gndGain = B*2.0;

        while (--sampleFrames >= 0)
        {
            double inputSampleL = *in1;
            double inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            air[pvSL4] = air[pvAL4] - air[pvAL3];
            air[pvSL3] = air[pvAL3] - air[pvAL2];
            air[pvSL2] = air[pvAL2] - air[pvAL1];
            air[pvSL1] = air[pvAL1] - inputSampleL;

            air[accSL3] = air[pvSL4] - air[pvSL3];
            air[accSL2] = air[pvSL3] - air[pvSL2];
            air[accSL1] = air[pvSL2] - air[pvSL1];

            air[acc2SL2] = air[accSL3] - air[accSL2];
            air[acc2SL1] = air[accSL2] - air[accSL1];

            air[outAL] = -(air[pvAL1] + air[pvSL3] + air[acc2SL2] - ((air[acc2SL2] + air[acc2SL1])*0.5));

            air[gainAL] *= 0.5;
            air[gainAL] += fabs(drySampleL-air[outAL])*0.5;
            if (air[gainAL] > 0.3*sqrt(overallscale)) air[gainAL] = 0.3*sqrt(overallscale);
            air[pvAL4] = air[pvAL3];
            air[pvAL3] = air[pvAL2];
            air[pvAL2] = air[pvAL1];
            air[pvAL1] = (air[gainAL] * air[outAL]) + drySampleL;

            double gnd = drySampleL - ((air[outAL]*0.5)+(drySampleL*(0.457-(0.017*overallscale))));
            double temp = (gnd + air[gndavgL])*0.5; air[gndavgL] = gnd; gnd = temp;

            inputSampleL = ((drySampleL-gnd)*airGain)+(gnd*gndGain);

            air[pvSR4] = air[pvAR4] - air[pvAR3];
            air[pvSR3] = air[pvAR3] - air[pvAR2];
            air[pvSR2] = air[pvAR2] - air[pvAR1];
            air[pvSR1] = air[pvAR1] - inputSampleR;

            air[accSR3] = air[pvSR4] - air[pvSR3];
            air[accSR2] = air[pvSR3] - air[pvSR2];
            air[accSR1] = air[pvSR2] - air[pvSR1];

            air[acc2SR2] = air[accSR3] - air[accSR2];
            air[acc2SR1] = air[accSR2] - air[accSR1];

            air[outAR] = -(air[pvAR1] + air[pvSR3] + air[acc2SR2] - ((air[acc2SR2] + air[acc2SR1])*0.5));

            air[gainAR] *= 0.5;
            air[gainAR] += fabs(drySampleR-air[outAR])*0.5;
            if (air[gainAR] > 0.3*sqrt(overallscale)) air[gainAR] = 0.3*sqrt(overallscale);
            air[pvAR4] = air[pvAR3];
            air[pvAR3] = air[pvAR2];
            air[pvAR2] = air[pvAR1];
            air[pvAR1] = (air[gainAR] * air[outAR]) + drySampleR;

            gnd = drySampleR - ((air[outAR]*0.5)+(drySampleR*(0.457-(0.017*overallscale))));
            temp = (gnd + air[gndavgR])*0.5; air[gndavgR] = gnd; gnd = temp;

            inputSampleR = ((drySampleR-gnd)*airGain)+(gnd*gndGain);

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

    enum {
        pvAL1,
        pvSL1,
        accSL1,
        acc2SL1,
        pvAL2,
        pvSL2,
        accSL2,
        acc2SL2,
        pvAL3,
        pvSL3,
        accSL3,
        pvAL4,
        pvSL4,
        gndavgL,
        outAL,
        gainAL,
        pvAR1,
        pvSR1,
        accSR1,
        acc2SR1,
        pvAR2,
        pvSR2,
        accSR2,
        acc2SR2,
        pvAR3,
        pvSR3,
        accSR3,
        pvAR4,
        pvSR4,
        gndavgR,
        outAR,
        gainAR,
        air_total
    };
    double air[air_total];

    uint32_t fpdL;
    uint32_t fpdR;

    float A;
    float B;
};
} // namespace airwindows
