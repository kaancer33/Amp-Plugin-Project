#include "PluginProcessor.h"
#include "PluginEditor.h"

NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Set waveshaper to soft-clip (tanh). Must be done before prepare().
    leftOverdrive.get<Shaper>().functionToUse  = [] (float x) { return std::tanh (x); };
    rightOverdrive.get<Shaper>().functionToUse = [] (float x) { return std::tanh (x); };
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
NewProjectAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Bypass toggles
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "overdriveOn", "Overdrive On", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "delayOn",     "Delay On",     true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "reverbOn",    "Reverb On",    true));

    // Overdrive
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "drive", "Drive",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "level", "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    // Delay
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayTime", "Delay Time (ms)",
        juce::NormalisableRange<float> (0.0f, 1000.0f, 1.0f), 250.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayFeedback", "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayMix", "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // Reverb
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbRoom", "Reverb Room",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbDamp", "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbMix", "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String NewProjectAudioProcessor::getName() const { return JucePlugin_Name; }
bool NewProjectAudioProcessor::acceptsMidi() const  { return false; }
bool NewProjectAudioProcessor::producesMidi() const { return false; }
bool NewProjectAudioProcessor::isMidiEffect() const { return false; }
double NewProjectAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int NewProjectAudioProcessor::getNumPrograms()   { return 1; }
int NewProjectAudioProcessor::getCurrentProgram() { return 0; }
void NewProjectAudioProcessor::setCurrentProgram (int) {}
const juce::String NewProjectAudioProcessor::getProgramName (int) { return {}; }
void NewProjectAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };

    // Overdrive chains (one per channel)
    leftOverdrive.prepare (monoSpec);
    rightOverdrive.prepare (monoSpec);

    // Smooth drive/level changes over 50 ms to avoid clicks
    leftOverdrive.get<DriveGain>().setRampDurationSeconds (0.05);
    rightOverdrive.get<DriveGain>().setRampDurationSeconds (0.05);
    leftOverdrive.get<LevelGain>().setRampDurationSeconds (0.05);
    rightOverdrive.get<LevelGain>().setRampDurationSeconds (0.05);

    // Delay lines
    delayL.prepare (monoSpec);
    delayR.prepare (monoSpec);
    const int maxDelaySamples = (int) (kMaxDelaySeconds * sampleRate) + 1;
    delayL.setMaximumDelayInSamples (maxDelaySamples);
    delayR.setMaximumDelayInSamples (maxDelaySamples);

    // Reverb (stereo)
    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    reverb.prepare (stereoSpec);
}

void NewProjectAudioProcessor::syncDSPToParameters() { /* params are read live in processBlock */ }

void NewProjectAudioProcessor::releaseResources()
{
    leftOverdrive.reset();
    rightOverdrive.reset();
    delayL.reset();
    delayR.reset();
    reverb.reset();
}

bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Require stereo in and stereo out (reverb DSP is wired for 2 channels)
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Read bypass flags
    const bool overdriveOn = *apvts.getRawParameterValue ("overdriveOn") >= 0.5f;
    const bool delayOn     = *apvts.getRawParameterValue ("delayOn")     >= 0.5f;
    const bool reverbOn    = *apvts.getRawParameterValue ("reverbOn")    >= 0.5f;

    // Read all parameters (lock-free atomic loads)
    const float drive        = apvts.getRawParameterValue ("drive")->load();
    const float level        = apvts.getRawParameterValue ("level")->load();
    const float delayTimeSmp = apvts.getRawParameterValue ("delayTime")->load()
                               / 1000.0f * (float) currentSampleRate;
    const float feedback     = apvts.getRawParameterValue ("delayFeedback")->load();
    const float delayMix     = apvts.getRawParameterValue ("delayMix")->load();
    const float reverbRoom   = apvts.getRawParameterValue ("reverbRoom")->load();
    const float reverbDamp   = apvts.getRawParameterValue ("reverbDamp")->load();
    const float reverbMix    = apvts.getRawParameterValue ("reverbMix")->load();

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int numSamples = buffer.getNumSamples();

    // ── 1. Overdrive ──────────────────────────────────────────────────────
    if (overdriveOn)
    {
        leftOverdrive.get<DriveGain>().setGainLinear (drive);
        rightOverdrive.get<DriveGain>().setGainLinear (drive);
        leftOverdrive.get<LevelGain>().setGainLinear (level);
        rightOverdrive.get<LevelGain>().setGainLinear (level);

        juce::dsp::AudioBlock<float> blockL (&L, 1, (size_t) numSamples);
        leftOverdrive.process (juce::dsp::ProcessContextReplacing<float> (blockL));

        juce::dsp::AudioBlock<float> blockR (&R, 1, (size_t) numSamples);
        rightOverdrive.process (juce::dsp::ProcessContextReplacing<float> (blockR));
    }

    // ── 2. Delay ──────────────────────────────────────────────────────────
    if (delayOn)
    {
        delayL.setDelay (delayTimeSmp);
        delayR.setDelay (delayTimeSmp);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = L[i];
            const float wetL = delayL.popSample (0);
            delayL.pushSample (0, dryL + wetL * feedback);
            L[i] = dryL * (1.0f - delayMix) + wetL * delayMix;

            const float dryR = R[i];
            const float wetR = delayR.popSample (0);
            delayR.pushSample (0, dryR + wetR * feedback);
            R[i] = dryR * (1.0f - delayMix) + wetR * delayMix;
        }
    }

    // ── 3. Reverb ─────────────────────────────────────────────────────────
    if (reverbOn)
    {
        juce::dsp::Reverb::Parameters reverbParams;
        reverbParams.roomSize   = reverbRoom;
        reverbParams.damping    = reverbDamp;
        reverbParams.wetLevel   = reverbMix;
        reverbParams.dryLevel   = 1.0f - reverbMix;
        reverbParams.width      = 1.0f;
        reverbParams.freezeMode = 0.0f;
        reverb.setParameters (reverbParams);

        juce::dsp::AudioBlock<float> reverbBlock (buffer);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (reverbBlock));
    }
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NewProjectAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
