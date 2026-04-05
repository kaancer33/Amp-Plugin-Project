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
    // ── RT-safe 1-pole filters ────────────────────────────────────────────
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

    // RBJ biquad peak EQ — used for metal mid-boost pre-clipping
    struct PeakEQ
    {
        float a1 = 0, a2 = 0, b0 = 1, b1 = 0, b2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void set (float freq, float q, float gainDb, float sr)
        {
            const float A     = std::pow (10.0f, gainDb / 40.0f);
            const float w0    = juce::MathConstants<float>::twoPi * freq / sr;
            const float alpha = std::sin (w0) / (2.0f * q);
            const float cosw  = std::cos (w0);

            const float nb0 = 1.0f + alpha * A;
            const float nb1 = -2.0f * cosw;
            const float nb2 = 1.0f - alpha * A;
            const float na0 = 1.0f + alpha / A;
            const float na1 = -2.0f * cosw;
            const float na2 = 1.0f - alpha / A;

            b0 = nb0 / na0;  b1 = nb1 / na0;  b2 = nb2 / na0;
            a1 = na1 / na0;  a2 = na2 / na0;
        }
        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
        void reset() { x1 = x2 = y1 = y2 = 0; }
    };

    // ── Distortion waveshaper (cascaded asym soft → hard clip) ────────────
    static float distortionShaper (float x);

    // ── Distortion ────────────────────────────────────────────────────────
    // 2x oversampling to prevent aliasing from the nonlinear stage
    juce::dsp::Oversampling<float> oversampling;

    // Pre-clip high-pass at 100 Hz (tight low end for metal chug)
    OnePoleHP preHPL, preHPR;

    // Pre-clip mid-boost peak EQ (900 Hz, +8 dB) — the "metal bite"
    // Modeled after Mesa/5150-style mid-push pre the saturation stage
    PeakEQ midBoostL, midBoostR;

    // Post-clip tone control (low-pass)
    OnePoleLP toneLPL, toneLPR;

    // Smoothed distortion & level to avoid zipper noise
    juce::SmoothedValue<float> distAmountSmoothed, levelSmoothed;

    // ── Delay ─────────────────────────────────────────────────────────────
    static constexpr float kMaxDelaySeconds    = 2.0f;
    static constexpr float kStereoOffsetRatio  = 1.07f;
    static constexpr float kLfoRateHz          = 0.7f;
    static constexpr float kLfoDepthMs         = 0.7f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
        delayL, delayR;

    OnePoleLP delayFbLPL, delayFbLPR;

    juce::SmoothedValue<float> delayTimeSmoothL, delayTimeSmoothR;
    float lfoPhase = 0.0f;

    // ── Reverb ────────────────────────────────────────────────────────────
    juce::dsp::Reverb reverb;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
        preDelayL, preDelayR;

    OnePoleLP reverbHCL, reverbHCR;

    float prevReverbRoom = -1.0f, prevReverbDamp = -1.0f;

    // ── Bypass crossfade (10 ms ramp, click-free) ─────────────────────────
    juce::SmoothedValue<float> distBypassGain, delayBypassGain, reverbBypassGain;

    // ── Pre-allocated temp buffer ─────────────────────────────────────────
    juce::AudioBuffer<float> tempBuffer;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
