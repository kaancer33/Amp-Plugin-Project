#pragma once
#include <JuceHeader.h>

class NewProjectAudioProcessor : public juce::AudioProcessor
{
public:
    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncDSPToParameters();

private:
    // ====================================================================
    //  RT-safe DSP structs — all stack-allocated, zero heap in audio thread
    // ====================================================================

    // ── 1-pole filters ─────────────────────────────────────────────────
    struct OnePoleLP
    {
        float z1 = 0.0f, a = 1.0f, b = 0.0f;
        void setCutoff (float freq, float sr)
        {
            b = std::exp (-juce::MathConstants<float>::twoPi * freq / sr);
            a = 1.0f - b;
        }
        float process (float x) { return z1 = x * a + z1 * b; }
        void reset() { z1 = 0.0f; }
    };

    struct OnePoleHP
    {
        OnePoleLP lp;
        void setCutoff (float freq, float sr) { lp.setCutoff (freq, sr); }
        float process (float x) { return x - lp.process (x); }
        void reset() { lp.reset(); }
    };

    // ── Biquad (TDF-II) — supports PeakEQ, Low Shelf, High Shelf ──────
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void setPeakEQ (float freq, float Q, float gainDB, float sr)
        {
            const float A  = std::pow (10.0f, gainDB / 40.0f);
            const float w0 = juce::MathConstants<float>::twoPi * freq / sr;
            const float alpha = std::sin (w0) / (2.0f * Q);
            const float cosW  = std::cos (w0);
            const float a0inv = 1.0f / (1.0f + alpha / A);
            b0 = (1.0f + alpha * A)  * a0inv;
            b1 = (-2.0f * cosW)      * a0inv;
            b2 = (1.0f - alpha * A)  * a0inv;
            a1 = b1;
            a2 = (1.0f - alpha / A)  * a0inv;
        }

        void setLowShelf (float freq, float gainDB, float sr)
        {
            const float A    = std::pow (10.0f, gainDB / 40.0f);
            const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
            const float sinW = std::sin (w0), cosW = std::cos (w0);
            const float alpha = sinW * 0.5f * std::sqrt (2.0f);  // S=1
            const float tsa  = 2.0f * std::sqrt (A) * alpha;
            const float a0inv = 1.0f / ((A+1) + (A-1)*cosW + tsa);
            b0 = A * ((A+1) - (A-1)*cosW + tsa) * a0inv;
            b1 = 2*A * ((A-1) - (A+1)*cosW)     * a0inv;
            b2 = A * ((A+1) - (A-1)*cosW - tsa) * a0inv;
            a1 = -2 * ((A-1) + (A+1)*cosW)      * a0inv;
            a2 = ((A+1) + (A-1)*cosW - tsa)     * a0inv;
        }

        void setHighShelf (float freq, float gainDB, float sr)
        {
            const float A    = std::pow (10.0f, gainDB / 40.0f);
            const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
            const float sinW = std::sin (w0), cosW = std::cos (w0);
            const float alpha = sinW * 0.5f * std::sqrt (2.0f);
            const float tsa  = 2.0f * std::sqrt (A) * alpha;
            const float a0inv = 1.0f / ((A+1) - (A-1)*cosW + tsa);
            b0 = A * ((A+1) + (A-1)*cosW + tsa)  * a0inv;
            b1 = -2*A * ((A-1) + (A+1)*cosW)     * a0inv;
            b2 = A * ((A+1) + (A-1)*cosW - tsa)  * a0inv;
            a1 = 2 * ((A-1) - (A+1)*cosW)        * a0inv;
            a2 = ((A+1) - (A-1)*cosW - tsa)      * a0inv;
        }

        // 2nd-order Butterworth HP (Q = 0.7071)
        void setHighPass (float freq, float sr, float Q = 0.7071f)
        {
            const float w0 = juce::MathConstants<float>::twoPi * freq / sr;
            const float alpha = std::sin (w0) / (2.0f * Q);
            const float cosW  = std::cos (w0);
            const float a0inv = 1.0f / (1.0f + alpha);
            b0 =  (1.0f + cosW) * 0.5f * a0inv;
            b1 = -(1.0f + cosW)        * a0inv;
            b2 =  (1.0f + cosW) * 0.5f * a0inv;
            a1 = -2.0f * cosW          * a0inv;
            a2 = (1.0f - alpha)        * a0inv;
        }

        // 2nd-order Butterworth LP (Q = 0.7071)
        void setLowPass (float freq, float sr, float Q = 0.7071f)
        {
            const float w0 = juce::MathConstants<float>::twoPi * freq / sr;
            const float alpha = std::sin (w0) / (2.0f * Q);
            const float cosW  = std::cos (w0);
            const float a0inv = 1.0f / (1.0f + alpha);
            b0 = (1.0f - cosW) * 0.5f * a0inv;
            b1 = (1.0f - cosW)        * a0inv;
            b2 = (1.0f - cosW) * 0.5f * a0inv;
            a1 = -2.0f * cosW         * a0inv;
            a2 = (1.0f - alpha)       * a0inv;
        }

        float process (float x)
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0.0f; }
    };

    // ── Noise Gate (ToobAmp state machine) ─────────────────────────────
    // States: Released → Attacking → Holding → Releasing → Released
    // Attack 1ms, Hold 200ms, Release 300ms, hysteresis 4:1
    struct NoiseGate
    {
        enum State { kDisabled, kReleased, kReleasing, kAttacking, kHolding };
        State state = kReleased;
        float gateGain = 0.0f;       // current gate multiplier (0..1)
        float dx = 0.0f;             // gain change per sample
        int   holdCount = 0;
        float attackRate = 0.0f;
        float releaseRate = 0.0f;
        int   holdSamples = 0;
        float attackThreshold = 0.001f;
        float releaseThreshold = 0.00025f;
        bool  enabled = true;

        void prepare (float sr)
        {
            attackRate  = 1.0f / (0.001f * sr);   // 1ms
            releaseRate = 1.0f / (0.3f * sr);     // 300ms
            holdSamples = (int)(0.2f * sr);       // 200ms
        }
        void setThreshold (float db)
        {
            float af = std::pow (10.0f, db / 20.0f);
            attackThreshold  = af;
            releaseThreshold = af * 0.25f;
            enabled = (db > -79.0f);
            if (!enabled) { state = kDisabled; gateGain = 1.0f; dx = 0.0f; }
            else if (state == kDisabled) { state = kReleased; gateGain = 0.0f; dx = 0.0f; }
        }
        float process (float input)
        {
            if (state == kDisabled) return input;
            float absVal = std::abs (input);
            if (absVal > attackThreshold && state < kAttacking)
            { state = kAttacking; dx = attackRate; holdCount = holdSamples; }
            else if (absVal > releaseThreshold && state >= kAttacking)
            { holdCount = holdSamples; }
            if (holdCount > 0 && --holdCount == 0)
            { state = kReleasing; dx = -releaseRate; }
            gateGain += dx;
            if (gateGain > 1.0f) { gateGain = 1.0f; dx = 0.0f; state = kHolding; }
            else if (gateGain < 0.0f) { gateGain = 0.0f; dx = 0.0f; state = kReleased; }
            return input * gateGain;
        }
        void reset()
        {
            gateGain = 0.0f; dx = 0.0f; holdCount = 0;
            state = enabled ? kReleased : kDisabled;
        }
    };

    // ── Gain Stage (ToobAmp atan + normalization) ──────────────────────
    // Key insight: output is normalized to ±1 regardless of gain setting.
    // More gain = more harmonics, NOT more volume.
    struct GainStageState
    {
        float effectiveGain = 1.0f;
        float bias = 0.3f;
        float postAdd = 0.0f;
        float gainScale = 1.0f;

        void configure (float driveNorm, float biasVal = 0.3f)
        {
            bias = biasVal;
            // Map 0-1 → -20..+50 dB (70 dB range, like ToobAmp)
            float gainDb = -20.0f + driveNorm * 70.0f;
            effectiveGain = std::pow (10.0f, gainDb / 20.0f);
            if (effectiveGain < 1e-7f) effectiveGain = 1e-7f;

            // DC compensation: output is zero when input is zero
            float yZero = std::atan (-bias);
            float yMax  = std::atan ( effectiveGain - bias);
            float yMin  = std::atan (-effectiveGain - bias);
            postAdd = -yZero;
            float maxAbs = std::max (yMax + postAdd, -(yMin + postAdd));
            if (maxAbs < 1e-7f) maxAbs = 1e-7f;
            gainScale = 1.0f / maxAbs;
        }

        inline float process (float x) const
        {
            return (std::atan (x * effectiveGain - bias) + postAdd) * gainScale;
        }
    };

    // ── Power Supply Sag (ToobAmp voltage-divider model) ───────────────
    // Models real tube amp PSU droop: loud signal → voltage drops →
    // output compresses.  Creates "breathing" feel and natural dynamics.
    struct SagProcessor
    {
        float z1 = 0.0f;             // LP filter state
        float lpA = 0.0f, lpB = 0.0f; // 13 Hz LP coefficients
        float sagAmount = 1.0f;       // sag factor
        float currentSag = 1.0f;

        void prepare (float sr)
        {
            lpB = std::exp (-juce::MathConstants<float>::twoPi * 13.0f / sr);
            lpA = 1.0f - lpB;
        }
        void setSagAmount (float driveNorm)
        {
            // More drive → more sag (auto-scaled)
            float sagDb = driveNorm * 20.0f;  // up to 20 dB sag at full drive
            sagAmount = std::pow (10.0f, sagDb / 20.0f);
        }
        float process (float x)
        {
            float power = x * x;
            z1 = lpA * power + lpB * z1;
            currentSag = 1.0f / (std::abs (z1) * (sagAmount - 1.0f) + 1.0f);
            return x * currentSag;
        }
        void reset() { z1 = 0.0f; currentSag = 1.0f; }
    };

    // ====================================================================
    //  Member variables
    // ====================================================================

    // ── Distortion chain ──────────────────────────────────────────────
    juce::dsp::Oversampling<float> oversampling;

    NoiseGate gateL, gateR;                  // noise gate before everything
    OnePoleHP preHPL, preHPR;                // 100 Hz pre-clip HP
    Biquad    preEqL, preEqR;                // 900 Hz +8dB mid-boost (the "metal bite")

    GainStageState stage1, stage2, stage3;   // 3 cascaded atan gain stages
    SagProcessor   sagL, sagR;               // power supply sag simulation

    // Post-distortion 3-band tone stack
    Biquad bassEqL,   bassEqR;               // low shelf 200 Hz
    Biquad midEqL,    midEqR;                // peak EQ 800 Hz
    Biquad trebleEqL, trebleEqR;             // high shelf 3500 Hz

    // ── Cabinet simulation (SM57 on 4x12 V30 approximation) ───────────
    // Without cab sim, raw amp output sounds harsh and fizzy.
    // This filter chain models the frequency response of a guitar speaker
    // cabinet mic'd with an SM57 — the standard for metal tones.
    Biquad cabHPL,    cabHPR;                // 2nd-order HP @ 70 Hz (cab box)
    Biquad cabResoL,  cabResoR;              // +3 dB @ 120 Hz (low-end thump)
    Biquad cabBoxL,   cabBoxR;               // -3 dB @ 400 Hz (remove boxiness)
    Biquad cabPresL,  cabPresR;              // +2 dB @ 2.5 kHz (presence)
    Biquad cabNotchL, cabNotchR;             // -6 dB @ 4.5 kHz (cone breakup dip)
    Biquad cabLPL,    cabLPR;                // 2nd-order LP @ 5.5 kHz (speaker rolloff)
    Biquad cabLP2L,   cabLP2R;               // extra LP @ 8 kHz (SM57 proximity)

    juce::SmoothedValue<float> distSmoothed, levelSmoothed;

    // ── Delay ─────────────────────────────────────────────────────────
    static constexpr float kMaxDelaySeconds   = 2.0f;
    static constexpr float kStereoOffsetRatio = 1.07f;
    static constexpr float kLfoRateHz         = 0.7f;
    static constexpr float kLfoDepthMs        = 0.7f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
        delayL, delayR;
    OnePoleLP delayFbLPL, delayFbLPR;
    juce::SmoothedValue<float> delayTimeSmoothL, delayTimeSmoothR;
    float lfoPhase = 0.0f;

    // ── Reverb ────────────────────────────────────────────────────────
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
        preDelayL, preDelayR;
    OnePoleLP reverbHCL, reverbHCR;
    float prevReverbRoom = -1.0f, prevReverbDamp = -1.0f;

    // ── Bypass crossfade (10 ms, click-free) ──────────────────────────
    juce::SmoothedValue<float> distBypassGain, delayBypassGain, reverbBypassGain;

    // ── Temp buffer (pre-allocated) ───────────────────────────────────
    juce::AudioBuffer<float> tempBuffer;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
