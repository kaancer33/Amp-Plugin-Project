#pragma once
#include "DspCommon.h"
#include "StereoDelay.h"

namespace forge
{
/** 8-channel feedback-delay-network reverb after Geraint Luff's "Let's Write a Reverb"
    (Signalsmith Audio, ADC 2021):

      pre-delay -> 4 diffusion steps (random delays, polarity flips, Hadamard mix)
                -> 8 modulated delay lines with Householder feedback and damping.

    The diffuser output doubles as early reflections. Decay gains are set per line from the
    exact line length, so RT60 is accurate at any size. Modulation breaks up the metallic
    ringing that plain comb-filter reverbs (Freeverb) have on guitar. */
class FdnReverb
{
public:
    static constexpr int N = 8;
    static constexpr int kSteps = 4;
    static constexpr double kMinSizeMs = 18.0, kMaxSizeMs = 120.0, kMaxPreDelayMs = 200.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;

        for (int s = 0; s < kSteps; ++s)
            for (int c = 0; c < N; ++c)
                diffusers[s][c].allocate ((int) std::ceil (kMaxSizeMs * 0.001 * fs / (double) (2 << s)) + 8);

        modDepth = 0.0005 * fs;
        for (int c = 0; c < N; ++c)
            lines[c].allocate ((int) std::ceil (2.0 * kMaxSizeMs * 0.001 * fs + modDepth) + 8);

        preL.allocate ((int) std::ceil (kMaxPreDelayMs * 0.001 * fs) + 8);
        preR.allocate ((int) std::ceil (kMaxPreDelayMs * 0.001 * fs) + 8);

        // Fixed pseudo-random layout so the reverb sounds the same every session.
        juce::Random rng (0x46524745);
        for (int s = 0; s < kSteps; ++s)
            for (int c = 0; c < N; ++c)
            {
                diffusePos[s][c] = ((double) c + rng.nextDouble()) / (double) N;
                flip[s][c] = rng.nextBool();
            }

        for (int c = 0; c < N; ++c)
        {
            lineRatio[c] = std::pow (2.0, (double) c / (double) N);
            const double rate = 0.13 + 0.08 * c;
            lfoCos[c] = std::cos ((double) c);
            lfoSin[c] = std::sin ((double) c);
            rotCos[c] = std::cos (kTwoPi * rate / fs);
            rotSin[c] = std::sin (kTwoPi * rate / fs);
        }

        inHPL.setCutoff (fs, 110.0);
        inHPR.setCutoff (fs, 110.0);
        sendGain.reset (fs, 0.02);
        dryGain.reset (fs, 0.02);
        wetGain.reset (fs, 0.02);
        preDelaySm.reset (fs, 0.1);

        setTone (toneHz);
        reset();
    }

    void reset()
    {
        for (auto& step : diffusers) for (auto& d : step) d.clear();
        for (auto& l : lines) l.clear();
        for (auto& d : damping) d.reset();
        preL.clear(); preR.clear();
        inHPL.reset(); inHPR.reset();
        for (auto* s : { &sendGain, &dryGain, &wetGain, &preDelaySm })
            s->setCurrentAndTargetValue (s->getTargetValue());

        currentSize = targetSize;
        currentRt60 = targetRt60;
        updateLengths (true, 1);
    }

    /** size 0..1, decay = RT60 in seconds. */
    void setParams (bool on, float size, float decaySeconds, float preDelayMs, float newToneHz, float mix) noexcept
    {
        targetSize = std::clamp ((double) size, 0.0, 1.0);
        targetRt60 = std::max (0.1, (double) decaySeconds);
        preDelaySm.setTargetValue ((float) (std::clamp ((double) preDelayMs, 0.0, kMaxPreDelayMs) * 0.001 * fs));
        if (newToneHz != toneHz) setTone (newToneHz);

        float d, w;
        mixLaw (mix, d, w);
        sendGain.setTargetValue (on ? 1.0f : 0.0f);
        dryGain.setTargetValue (on ? d : 1.0f);
        wetGain.setTargetValue (w);
    }

    void process (float* L, float* R, int n) noexcept
    {
        // Size and decay glide at block rate (0.2 s time constant) to avoid zipper noise.
        const double a = 1.0 - std::exp (-(double) n / (0.2 * fs));
        currentSize += (targetSize - currentSize) * a;
        currentRt60 += (targetRt60 - currentRt60) * a;
        updateLengths (false, n);

        for (int i = 0; i < n; ++i)
        {
            const float send = sendGain.getNextValue();
            const float inL = (float) inHPL.process (L[i]) * send;
            const float inR = (float) inHPR.process (R[i]) * send;

            const float pd = std::max (1.0f, preDelaySm.getNextValue());
            const float pl = preL.readLinear (pd);
            const float pr = preR.readLinear (pd);
            preL.push (inL);
            preR.push (inR);

            float x[N];
            for (int c = 0; c < N; ++c)
                x[c] = 0.5f * ((c & 1) ? pr : pl) * ((c & 2) ? -1.0f : 1.0f);

            for (int s = 0; s < kSteps; ++s)
            {
                float y[N];
                for (int c = 0; c < N; ++c)
                {
                    diffLen[s][c] += diffInc[s][c];
                    y[c] = diffusers[s][c].readLinear (diffLen[s][c]);
                    diffusers[s][c].push (x[c]);
                    if (flip[s][c]) y[c] = -y[c];
                }
                hadamard (y);
                std::copy (y, y + N, x);
            }

            const float erL = x[0] + x[2] + x[4] + x[6];
            const float erR = x[1] + x[3] + x[5] + x[7];

            float d[N];
            float sum = 0.0f;
            for (int c = 0; c < N; ++c)
            {
                // rotate the LFO phasor
                const double s0 = lfoSin[c], c0 = lfoCos[c];
                lfoSin[c] = s0 * rotCos[c] + c0 * rotSin[c];
                lfoCos[c] = c0 * rotCos[c] - s0 * rotSin[c];

                lineLen[c] += lineInc[c];
                const float v = lines[c].readLinear (lineLen[c] + modDepth * lfoSin[c]);
                d[c] = (float) damping[c].process (v) * lineGain[c];
                sum += d[c];
            }

            const float lateL = d[0] + d[2] + d[4] + d[6];
            const float lateR = d[1] + d[3] + d[5] + d[7];

            const float house = sum * (2.0f / (float) N);
            for (int c = 0; c < N; ++c)
                lines[c].push (x[c] + d[c] - house);

            const float dg = dryGain.getNextValue(), wg = wetGain.getNextValue();
            L[i] = L[i] * dg + (kLateGain * lateL + kEarlyGain * erL) * wg;
            R[i] = R[i] * dg + (kLateGain * lateR + kEarlyGain * erR) * wg;
        }

        // keep the phasors on the unit circle
        for (int c = 0; c < N; ++c)
        {
            const double r = 1.0 / std::sqrt (lfoSin[c] * lfoSin[c] + lfoCos[c] * lfoCos[c]);
            lfoSin[c] *= r;
            lfoCos[c] *= r;
        }
    }

private:
    static constexpr float kLateGain = 0.42f, kEarlyGain = 0.22f;

    static inline void hadamard (float* x) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float p = x[j], q = x[j + h];
                    x[j] = p + q;
                    x[j + h] = p - q;
                }

        constexpr float scale = 0.35355339f;   // 1 / sqrt(8): keeps the matrix orthonormal
        for (int i = 0; i < N; ++i)
            x[i] *= scale;
    }

    void setTone (float hz) noexcept
    {
        toneHz = hz;
        for (auto& d : damping)
            d.setCutoff (fs, hz);
    }

    /** Recomputes the delay lengths for the current size and spreads the change over the
        next n samples. */
    void updateLengths (bool snap, int n) noexcept
    {
        const double sizeSamples = (kMinSizeMs + (kMaxSizeMs - kMinSizeMs) * currentSize) * 0.001 * fs;

        for (int s = 0; s < kSteps; ++s)
        {
            const double range = sizeSamples / (double) (2 << s);
            for (int c = 0; c < N; ++c)
            {
                const double target = std::max (1.0, range * diffusePos[s][c]);
                if (snap) { diffLen[s][c] = target; diffInc[s][c] = 0.0; }
                else      diffInc[s][c] = (target - diffLen[s][c]) / (double) n;
            }
        }

        for (int c = 0; c < N; ++c)
        {
            const double target = sizeSamples * lineRatio[c];
            if (snap) { lineLen[c] = target; lineInc[c] = 0.0; }
            else      lineInc[c] = (target - lineLen[c]) / (double) n;

            // -60 dB after rt60 seconds, exactly, for this line's length.
            lineGain[c] = (float) std::pow (10.0, -3.0 * (target / fs) / currentRt60);
        }
    }

    double fs = 48000.0;
    DelayLine diffusers[kSteps][N];
    DelayLine lines[N];
    DelayLine preL, preR;
    OnePoleLP damping[N];
    OnePoleHP inHPL, inHPR;

    double diffusePos[kSteps][N] {}, diffLen[kSteps][N] {}, diffInc[kSteps][N] {};
    bool flip[kSteps][N] {};
    double lineRatio[N] {}, lineLen[N] {}, lineInc[N] {};
    float lineGain[N] {};
    double lfoSin[N] {}, lfoCos[N] {}, rotSin[N] {}, rotCos[N] {};
    double modDepth = 24.0;

    double targetSize = 0.4, currentSize = 0.4, targetRt60 = 1.8, currentRt60 = 1.8;
    float toneHz = 6500.0f;

    juce::SmoothedValue<float> sendGain, dryGain { 1.0f }, wetGain, preDelaySm;
};
} // namespace forge
