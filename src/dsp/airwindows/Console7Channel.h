// Airwindows Console7Channel by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/Console7Channel: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class Console7Channel final : public Algorithm
{
public:
    enum { kParamA = 0, kNumParameters = 1 };
    Console7Channel()
    {
        A = 0.772f;
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
        gainchase = -1.0;
        chasespeed = 64.0;
        for (int x = 0; x < 15; x++) {biquadA[x] = 0.0;}
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

        double inputgain = A*1.272019649514069;
        //which is, in fact, the square root of 1.618033988749894848204586...
        //this happens to give us a boost factor where the track continues to get louder even
        //as it saturates and loses a bit of peak energy. Console7Channel channels go to 12! (.272,etc)
        //Neutral gain through the whole system with a full scale sine ia 0.772 on the gain knob
        if (gainchase != inputgain) chasespeed *= 2.0;
        if (chasespeed > sampleFrames) chasespeed = sampleFrames;
        if (gainchase < 0.0) gainchase = inputgain;

        biquadA[0] = 20000.0 / getSampleRate();
        biquadA[1] = 1.618033988749894848204586;

        double K = tan(M_PI * biquadA[0]); //lowpass
        double norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
        biquadA[2] = K * K * norm;
        biquadA[3] = 2.0 * biquadA[2];
        biquadA[4] = biquadA[2];
        biquadA[5] = 2.0 * (K * K - 1.0) * norm;
        biquadA[6] = (1.0 - K / biquadA[1] + K * K) * norm;

        while (--sampleFrames >= 0)
        {
            double inputSampleL = *in1;
            double inputSampleR = *in2;
            if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;

            double outSampleL = biquadA[2]*inputSampleL+biquadA[3]*biquadA[7]+biquadA[4]*biquadA[8]-biquadA[5]*biquadA[9]-biquadA[6]*biquadA[10];
            biquadA[8] = biquadA[7]; biquadA[7] = inputSampleL; inputSampleL = outSampleL; biquadA[10] = biquadA[9]; biquadA[9] = inputSampleL; //DF1 left

            double outSampleR = biquadA[2]*inputSampleR+biquadA[3]*biquadA[11]+biquadA[4]*biquadA[12]-biquadA[5]*biquadA[13]-biquadA[6]*biquadA[14];
            biquadA[12] = biquadA[11]; biquadA[11] = inputSampleR; inputSampleR = outSampleR; biquadA[14] = biquadA[13]; biquadA[13] = inputSampleR; //DF1 right

            chasespeed *= 0.9999; chasespeed -= 0.01; if (chasespeed < 64.0) chasespeed = 64.0;
            //we have our chase speed compensated for recent fader activity
            gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
            //gainchase is chasing the target, as a simple multiply gain factor
            if (1.0 != gainchase) {inputSampleL *= pow(gainchase,3); inputSampleR *= pow(gainchase,3);}
            //this trim control cuts back extra hard because we will amplify after the distortion
            //that will shift the distortion/antidistortion curve, in order to make faded settings
            //slightly 'expanded' and fall back in the soundstage, subtly

            if (inputSampleL > 1.097) inputSampleL = 1.097;
            if (inputSampleL < -1.097) inputSampleL = -1.097;
            inputSampleL = ((sin(inputSampleL*fabs(inputSampleL))/((fabs(inputSampleL) == 0.0) ?1:fabs(inputSampleL)))*0.8)+(sin(inputSampleL)*0.2);
            if (inputSampleR > 1.097) inputSampleR = 1.097;
            if (inputSampleR < -1.097) inputSampleR = -1.097;
            inputSampleR = ((sin(inputSampleR*fabs(inputSampleR))/((fabs(inputSampleR) == 0.0) ?1:fabs(inputSampleR)))*0.8)+(sin(inputSampleR)*0.2);
            //this is a version of Spiral blended 80/20 with regular Density ConsoleChannel.
            //It's blending between two different harmonics in the overtones of the algorithm

            if (1.0 != gainchase && 0.0 != gainchase) {inputSampleL /= gainchase; inputSampleR /= gainchase;}
            //we re-amplify after the distortion relative to how much we cut back previously.

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

    double gainchase;
    double chasespeed;

    double biquadA[15];

    uint32_t fpdL;
    uint32_t fpdR;

    float A;
};
} // namespace airwindows
