#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/AmpEngine.h"
#include "dsp/Pedals.h"
#include "dsp/CabSim.h"
#include "dsp/StereoDelay.h"
#include "dsp/FdnReverb.h"

/** Signal flow (the guitar chain is mono, like a real rig; the effects are stereo):

      input gain -> gate -> [4x oversampled: boost -> preamp -> tone stack -> power amp]
                 -> cab (built-in model or IR) -> delay -> reverb -> output gain
*/
class ForgeAmpProcessor : public juce::AudioProcessor
{
public:
    ForgeAmpProcessor();
    ~ForgeAmpProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return forge::info::productName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 14.0; }

    // Presets are handled by the plugin's own browser. Exposing them as host programs makes
    // some hosts call setCurrentProgram(0) after restoring a session, which would wipe it.
    int getNumPrograms() override                               { return 1; }
    int getCurrentProgram() override                            { return 0; }
    void setCurrentProgram (int) override                       {}
    const juce::String getProgramName (int) override            { return {}; }
    void changeProgramName (int, const juce::String&) override  {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    forge::PresetManager& getPresetManager() noexcept { return *presets; }

    forge::LevelMeter inputMeter, outputMeter;
    float getGateGain() const noexcept { return gateGainForUi.load (std::memory_order_relaxed); }
    double getBpm() const noexcept     { return bpm.load (std::memory_order_relaxed); }

    /** Loads a .wav/.aif impulse response and switches the cab to "User IR".
        Message thread only. Returns an error message, or an empty string on success. */
    juce::String loadUserImpulseResponse (const juce::File& file, bool selectUserIrModel = true);
    void clearUserImpulseResponse();
    juce::File getUserImpulseResponseFile() const;
    bool hasUserImpulseResponse() const noexcept { return userIrLoaded.load(); }

    /** The user IR's magnitude response in dB at log-spaced frequencies, for the UI. */
    std::vector<float> getUserIrResponse() const;
    static constexpr float kResponseMinHz = 20.0f, kResponseMaxHz = 20000.0f;
    static constexpr int   kResponsePoints = 256;

    /** Editor zoom (window width / 512 art pixels) and last page, remembered with the session. */
    float uiScale = 2.0f;
    int uiPage = 1;

private:
    void cacheParameterPointers();
    void updateParameters();
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n, bool stereoInput);

    std::unique_ptr<forge::PresetManager> presets;

    struct Params
    {
        std::atomic<float>* inputGain;
        std::atomic<float>* outputGain;
        std::atomic<float>* gateOn;   std::atomic<float>* gateThreshold; std::atomic<float>* gateRelease;
        std::atomic<float>* boostOn;  std::atomic<float>* boostDrive; std::atomic<float>* boostTone; std::atomic<float>* boostLevel;
        std::atomic<float>* ampOn;    std::atomic<float>* ampModel;
        std::atomic<float>* ampGain;  std::atomic<float>* ampBass; std::atomic<float>* ampMid; std::atomic<float>* ampTreble;
        std::atomic<float>* ampPresence; std::atomic<float>* ampDepth; std::atomic<float>* ampMaster;
        std::atomic<float>* cabOn;    std::atomic<float>* cabModel; std::atomic<float>* cabMic;
        std::atomic<float>* cabLowCut; std::atomic<float>* cabHighCut;
        std::atomic<float>* delayOn;  std::atomic<float>* delaySync; std::atomic<float>* delayTime;
        std::atomic<float>* delayDivision; std::atomic<float>* delayFeedback; std::atomic<float>* delayTone;
        std::atomic<float>* delayMix; std::atomic<float>* delayPingPong;
        std::atomic<float>* reverbOn; std::atomic<float>* reverbSize; std::atomic<float>* reverbDecay;
        std::atomic<float>* reverbPreDelay; std::atomic<float>* reverbTone; std::atomic<float>* reverbMix;
    } p {};

    // ── DSP ──────────────────────────────────────────────────────────────────
    static constexpr int kOversamplingOrder = 2;    // 2^2 = 4x
    static constexpr int kControlInterval   = 32;   // oversampled samples between control updates

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    forge::NoiseGate gate;
    forge::Overdrive boost;
    forge::AmpEngine amp;
    forge::CabSim cab;
    forge::StereoDelay delay;
    forge::FdnReverb reverb;

    forge::SwitchFader ampSwitch, cabSwitch;
    int pendingAmpModel = 1, pendingCabModel = 0;

    // Control-rate smoothed knobs (stepped once per kControlInterval oversampled samples).
    juce::SmoothedValue<float> sGain, sBass, sMid, sTreble, sPresence, sDepth, sMaster;
    juce::SmoothedValue<float> sBoostDrive, sBoostTone, sBoostLevel;
    float lastBoost[3] { -1.0f, -1.0f, -1.0f };
    // Per-sample smoothed gains and bypass cross-fades.
    juce::SmoothedValue<float> inGain, outGain, boostMix, ampMix, cabMix;

    juce::AudioBuffer<float> monoBuffer, dryBuffer;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    bool firstBlock = true;

    std::atomic<float> gateGainForUi { 0.0f };
    std::atomic<double> bpm { 120.0 };

    // ── User IR ──────────────────────────────────────────────────────────────
    juce::File userIrFile;
    std::atomic<bool> userIrLoaded { false };  // juce::dsp::Convolution keeps the IR across prepare()
    std::vector<float> userIrResponse;
    mutable juce::SpinLock userIrLock;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ForgeAmpProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeAmpProcessor)
};
