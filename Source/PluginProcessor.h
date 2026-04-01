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

    // Called by PresetManager after loading a preset to update DSP state
    void syncDSPToParameters();

private:
    // ── Overdrive ─────────────────────────────────────────────────────────
    // Signal path per channel: drive gain → tanh waveshaper → level gain
    enum OverdriveIndex { DriveGain, Shaper, LevelGain };
    using MonoOverdrive = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,
        juce::dsp::WaveShaper<float>,
        juce::dsp::Gain<float>
    >;
    MonoOverdrive leftOverdrive, rightOverdrive;

    // ── Delay ─────────────────────────────────────────────────────────────
    static constexpr float kMaxDelaySeconds = 2.0f;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL, delayR;

    // ── Reverb ────────────────────────────────────────────────────────────
    juce::dsp::Reverb reverb;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessor)
};
