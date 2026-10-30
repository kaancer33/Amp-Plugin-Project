#pragma once
#include "DspCommon.h"

namespace forge
{
/** Mix law shared by the delay and reverb: up to 50 % the wet signal is added to an
    untouched dry signal (pedal style), above 50 % the dry signal fades out. */
inline void mixLaw (float mix, float& dry, float& wet) noexcept
{
    dry = std::min (1.0f, 2.0f * (1.0f - mix));
    wet = std::min (1.0f, 2.0f * mix);
}

/** Stereo / ping-pong delay with a filtered, softly saturating feedback loop, slight
    modulation, a tape-like glide when the time changes, and trails (switching it off stops
    new input but lets the repeats die away naturally). */
class StereoDelay
{
public:
    static constexpr double kMaxDelayMs = 4000.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxSamples = (int) std::ceil ((kMaxDelayMs + 50.0) * 0.001 * fs);
        lineL.allocate (maxSamples);
        lineR.allocate (maxSamples);

        timeSmoothed.reset (fs, 0.25);
        sendGain.reset (fs, 0.02);
        dryGain.reset (fs, 0.02);
        wetGain.reset (fs, 0.02);
        feedbackSm.reset (fs, 0.05);

        lfoInc = kTwoPi * 0.35 / fs;
        hpL.setCutoff (fs, 90.0);
        hpR.setCutoff (fs, 90.0);
        setTone (toneHz);
        reset();
    }

    void reset()
    {
        lineL.clear(); lineR.clear();
        lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
        timeSmoothed.setCurrentAndTargetValue (timeSmoothed.getTargetValue());
        for (auto* s : { &sendGain, &dryGain, &wetGain, &feedbackSm })
            s->setCurrentAndTargetValue (s->getTargetValue());
        lfoPhase = 0.0;
    }

    void setParams (bool on, double timeMs, float feedback, float newToneHz, float mix, bool newPingPong) noexcept
    {
        timeSmoothed.setTargetValue (std::clamp (timeMs, 1.0, kMaxDelayMs) * 0.001 * fs);
        feedbackSm.setTargetValue (feedback);
        if (newToneHz != toneHz) setTone (newToneHz);
        pingPong = newPingPong;

        float d, w;
        mixLaw (mix, d, w);
        sendGain.setTargetValue (on ? 1.0f : 0.0f);
        dryGain.setTargetValue (on ? d : 1.0f);
        wetGain.setTargetValue (w);
    }

    void process (float* L, float* R, int n) noexcept
    {
        const double modDepth = 0.0004 * fs;   // +-0.4 ms of drift

        for (int i = 0; i < n; ++i)
        {
            const double base = timeSmoothed.getNextValue();
            const double mod  = modDepth * std::sin (lfoPhase);
            lfoPhase += lfoInc;
            if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

            const float wl = lineL.readHermite (base + mod);
            const float wr = lineR.readHermite (base - mod);

            const float fb   = feedbackSm.getNextValue();
            const float send = sendGain.getNextValue();
            const float fbl  = (float) fastTanh (lpL.process (hpL.process (wl)) * fb);
            const float fbr  = (float) fastTanh (lpR.process (hpR.process (wr)) * fb);

            const float inL = L[i] * send, inR = R[i] * send;

            if (pingPong)
            {
                lineL.push (0.5f * (inL + inR) + fbr);
                lineR.push (fbl);
            }
            else
            {
                lineL.push (inL + fbl);
                lineR.push (inR + fbr);
            }

            const float dg = dryGain.getNextValue(), wg = wetGain.getNextValue();
            L[i] = L[i] * dg + wl * wg;
            R[i] = R[i] * dg + wr * wg;
        }
    }

private:
    void setTone (float hz) noexcept
    {
        toneHz = hz;
        lpL.setCutoff (fs, hz);
        lpR.setCutoff (fs, hz);
    }

    double fs = 48000.0;
    DelayLine lineL, lineR;
    OnePoleLP lpL, lpR;
    OnePoleHP hpL, hpR;
    float toneHz = 4500.0f;
    bool pingPong = false;

    juce::SmoothedValue<double> timeSmoothed { 18000.0 };
    juce::SmoothedValue<float> sendGain, dryGain { 1.0f }, wetGain, feedbackSm;

    double lfoPhase = 0.0, lfoInc = 0.0;
};
} // namespace forge
