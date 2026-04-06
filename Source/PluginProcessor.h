#pragma once
#include <JuceHeader.h>
#include <memory>

// Forward declaration — NAM core
namespace nam { class DSP; }

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

    // ── NAM Model Loading ────────────────────────────────────────────────
    void loadNAMModel (const juce::File& namFile);
    bool isModelLoaded() const { return namModel != nullptr; }
    juce::String getModelName() const { return currentModelName; }

private:
    // ====================================================================
    //  DSP primitives — all RT-safe, zero heap allocation
    // ====================================================================

    // ── 1-pole LP ──────────────────────────────────────────────────────
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

    // ── Biquad (TDF-II) ────────────────────────────────────────────────
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
            b0 = (1.0f + alpha * A) * a0inv;
            b1 = (-2.0f * cosW)     * a0inv;
            b2 = (1.0f - alpha * A) * a0inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * a0inv;
        }
        void setLowShelf (float freq, float gainDB, float sr)
        {
            const float A    = std::pow (10.0f, gainDB / 40.0f);
            const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
            const float sinW = std::sin (w0), cosW = std::cos (w0);
            const float alpha = sinW * 0.5f * std::sqrt (2.0f);
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

    // ── Noise Gate (exact ToobAmp state machine) ───────────────────────
    struct NoiseGate
    {
        enum State { kDisabled, kReleased, kReleasing, kAttacking, kHolding };
        State state = kReleased;
        float gateGain = 0.0f;
        float dx = 0.0f;
        int   holdCount = 0;
        float attackRate = 0.0f;
        float releaseRate = 0.0f;
        int   holdSamples = 0;
        float attackThreshold = 0.001f;
        float releaseThreshold = 0.00025f;
        bool  enabled = true;

        void prepare (float sr)
        {
            attackRate  = 1.0f / (0.001f * sr);
            releaseRate = 1.0f / (0.3f * sr);
            holdSamples = (int)(0.2f * sr);
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

    // ====================================================================
    //  Member variables
    // ====================================================================

    // ── Neural Amp Model (core inference engine) ─────────────────────
    std::unique_ptr<nam::DSP> namModel;
    juce::String currentModelName { "No Model" };
    juce::SpinLock namModelLock;   // protects hot-swap

    // ── NAM internal buffers (double precision — NAM expects double) ──
    std::vector<double> namInputBuffer;
    std::vector<double> namOutputBuffer;

    // ── Noise gate ────────────────────────────────────────────────────
    NoiseGate gateL, gateR;

    // ── Pre-NAM input conditioning ───────────────────────────────────
    OnePoleHP preHPL, preHPR;   // tighten low-end before neural net

    // ── Post-NAM 3-band tone stack ───────────────────────────────────
    Biquad bassEqL, bassEqR;
    Biquad midEqL,  midEqR;
    Biquad trebleEqL, trebleEqR;

    // ── Cabinet simulation (SM57 on 4x12 V30) ───────────────────────
    Biquad cabHPL,    cabHPR;
    Biquad cabResoL,  cabResoR;
    Biquad cabBoxL,   cabBoxR;
    Biquad cabPresL,  cabPresR;
    Biquad cabNotchL, cabNotchR;
    Biquad cabLPL,    cabLPR;
    Biquad cabLP2L,   cabLP2R;

    juce::SmoothedValue<float> levelSmoothed;

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

    // ── Bypass crossfade ──────────────────────────────────────────────
    juce::SmoothedValue<float> distBypassGain, delayBypassGain, reverbBypassGain;

    juce::AudioBuffer<float> tempBuffer;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
