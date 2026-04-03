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
    // ── RT-safe 1-pole filters (no heap allocation) ───────────────────────
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

    // ── Tube waveshaper ───────────────────────────────────────────────────
    static float tubeWaveshaper (float x);

    // ── Overdrive ─────────────────────────────────────────────────────────
    // 2x oversampling to prevent aliasing from the nonlinear waveshaper
    juce::dsp::Oversampling<float> oversampling;

    // Pre-OD high-pass removes sub-bass before clipping (prevents intermod)
    OnePoleHP preHPL, preHPR;

    // Post-OD tone control (low-pass)
    OnePoleLP toneLPL, toneLPR;

    // Smoothed drive & level to avoid zipper noise
    juce::SmoothedValue<float> driveSmoothed, levelSmoothed;

    // ── Delay ─────────────────────────────────────────────────────────────
    static constexpr float kMaxDelaySeconds    = 2.0f;
    static constexpr float kStereoOffsetRatio  = 1.07f;   // R channel 7% longer
    static constexpr float kLfoRateHz          = 0.7f;    // chorus-like modulation
    static constexpr float kLfoDepthMs         = 0.7f;    // ±0.7 ms

    // Lagrange3rd interpolation for smooth modulated reads
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>
        delayL, delayR;

    // LP filter in the feedback path (darkens repeats like analog/tape)
    OnePoleLP delayFbLPL, delayFbLPR;

    // Smoothed delay time prevents pitch glitches on knob changes
    juce::SmoothedValue<float> delayTimeSmoothL, delayTimeSmoothR;

    float lfoPhase = 0.0f;

    // ── Reverb ────────────────────────────────────────────────────────────
    juce::dsp::Reverb reverb;

    // Pre-delay separates dry attack from reverb tail
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
        preDelayL, preDelayR;

    // High-cut on wet signal (derived from damping param)
    OnePoleLP reverbHCL, reverbHCR;

    // Change detection to avoid setting params every block
    float prevReverbRoom = -1.0f, prevReverbDamp = -1.0f;

    // ── Bypass crossfade (10 ms ramp, click-free) ─────────────────────────
    juce::SmoothedValue<float> odBypassGain, delayBypassGain, reverbBypassGain;

    // ── Pre-allocated temp buffer (no audio-thread allocation) ────────────
    juce::AudioBuffer<float> tempBuffer;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
