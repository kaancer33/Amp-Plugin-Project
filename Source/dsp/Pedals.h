#pragma once
#include "DspCommon.h"

namespace forge
{
//==============================================================================
/** Noise gate with a peak-envelope detector, open/close hysteresis and a hold time, so it
    doesn't chatter on decaying notes. Runs on the mono guitar signal before the amp. */
class NoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        envRelease  = (float) std::exp (-1.0 / (0.020 * fs));
        attackCoeff = (float) std::exp (-1.0 / (0.0005 * fs));
        holdSamples = (int) (0.040 * fs);
        reset();
    }

    void reset() noexcept { env = 0.0f; gain = enabled ? 0.0f : 1.0f; open = false; holdCounter = 0; }

    void setParams (bool on, float thresholdDb, float releaseMs) noexcept
    {
        enabled = on;
        openThreshold  = (float) dbToGain (thresholdDb);
        closeThreshold = (float) dbToGain (thresholdDb - 6.0f);
        releaseCoeff   = (float) std::exp (-1.0 / (std::max (1.0f, releaseMs) * 0.001 * fs));
    }

    void process (float* data, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (data[i]);
            env = a > env ? a : env * envRelease;

            if (! enabled)                  { open = true; }
            else if (! open)                { if (env > openThreshold) { open = true; holdCounter = holdSamples; } }
            else if (env > closeThreshold)  { holdCounter = holdSamples; }
            else if (--holdCounter <= 0)    { open = false; }

            const float target = open ? 1.0f : 0.0f;
            const float coeff  = target > gain ? attackCoeff : releaseCoeff;
            gain = target + (gain - target) * coeff;
            data[i] *= gain;
        }
    }

    float getGain() const noexcept { return gain; }

private:
    double fs = 48000.0;
    bool enabled = true, open = false;
    float env = 0.0f, gain = 0.0f;
    float envRelease = 0.999f, attackCoeff = 0.9f, releaseCoeff = 0.999f;
    float openThreshold = 0.001f, closeThreshold = 0.0005f;
    int holdSamples = 2000, holdCounter = 0;
};

//==============================================================================
/** Tube Screamer style overdrive, modelled on the TS808 clipping stage:
    the clean signal passes at unity, while the part above ~720 Hz (4.7k / 47n on the gain
    leg) is amplified by (51k + drive pot) / 4.7k and soft-clipped by the feedback diodes.
    The 51 pF feedback cap rolls the gain off, more so as drive goes up. Then a tone low-pass
    and level. The result is the classic mid hump that tightens a high-gain amp.
    Runs at the oversampled rate. */
class Overdrive
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        gainLegHP.setCutoff (fs, 720.0);
        outputHP.setCutoff (fs, 20.0);
        setControls (drive, tone, levelDb);
        reset();
    }

    void reset() noexcept { gainLegHP.reset(); feedbackLP.reset(); toneLP.reset(); outputHP.reset(); }

    /** drive and tone 0..1, level in dB. */
    void setControls (float newDrive, float newTone, float newLevelDb) noexcept
    {
        drive = newDrive; tone = newTone; levelDb = newLevelDb;

        const double rDrive = 51.0e3 + (double) drive * 500.0e3;
        clipGain = rDrive / 4.7e3;
        feedbackLP.setCutoff (fs, 1.0 / (kTwoPi * rDrive * 51.0e-12));
        toneLP.setCutoff (fs, 900.0 * std::pow (10.0, (double) tone));   // 900 Hz .. 9 kHz
        level = dbToGain (levelDb);
    }

    inline double process (double x) noexcept
    {
        const double u = feedbackLP.process (gainLegHP.process (x) * clipGain);
        const double clipped = 0.45 * fastTanh (u / 0.45);
        return outputHP.process (toneLP.process (x + clipped)) * level;
    }

private:
    double fs = 192000.0;
    float drive = 0.2f, tone = 0.5f, levelDb = 0.0f;
    double clipGain = 10.0, level = 1.0;
    OnePoleHP gainLegHP, outputHP;
    OnePoleLP feedbackLP, toneLP;
};
} // namespace forge
