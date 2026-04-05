#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Heavy distortion waveshaper — cascaded soft→hard clipping.
//
// Stage 1: Asymmetric soft clip via tanh with DC bias
//          - DC bias (0.12) breaks symmetry → adds even harmonics (tube warmth)
//          - Gain coefficient (2.5) ensures saturation kicks in quickly
// Stage 2: Hard clip at ±0.9
//          - Adds edgy odd-harmonic content (fuzz/metal character)
//          - The extra 1.4× gain before clip guarantees hard-clip engagement
//
// Together this gives the aggressive, "chuggy" saturation that death metal,
// djent and hard-rock players expect — not the gentle bluesy breakup of tanh.
//==============================================================================
float NewProjectAudioProcessor::distortionShaper (float x)
{
    constexpr float kBias      = 0.12f;
    constexpr float kInputGain = 2.5f;
    constexpr float kStage2Gain = 1.4f;
    constexpr float kHardClip  = 0.9f;
    // tanh(kBias * kInputGain) = tanh(0.3) ≈ 0.2913126
    constexpr float kDcOffset  = 0.29131261f;

    // Stage 1: asymmetric soft clip
    float y = std::tanh ((x + kBias) * kInputGain) - kDcOffset;
    // Stage 2: hard clip for bite
    y = juce::jlimit (-kHardClip, kHardClip, y * kStage2Gain);
    return y;
}

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      oversampling (2, 1,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                    true)
{
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() {}

//==============================================================================
// Parameter layout — 14 parameters total
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
NewProjectAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Bypass toggles ───────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "distortionOn", "Distortion On", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "delayOn", "Delay On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "reverbOn", "Reverb On", true));

    // ── Distortion ───────────────────────────────────────────────────────
    // Range 1–80 so knob can cover everything from clean→crunch→high-gain→metal.
    // Skew 0.25 packs the cleaner range into the first ~40% of the knob,
    // leaving the upper half for aggressive metal territory.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distortion", "Distortion",
        juce::NormalisableRange<float> (1.0f, 80.0f, 0.01f, 0.25f), 10.0f));

    // Post-clipper tone (LP) — tames harsh frequencies from the nonlinear stage
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distTone", "Distortion Tone",
        juce::NormalisableRange<float> (200.0f, 12000.0f, 1.0f, 0.35f), 4000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "level", "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // ── Delay ────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayTime", "Delay Time (ms)",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f), 250.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayFeedback", "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayTone", "Delay Tone",
        juce::NormalisableRange<float> (500.0f, 12000.0f, 1.0f, 0.35f), 6000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayMix", "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // ── Reverb ───────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbRoom", "Reverb Room",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "reverbPreDelay", "Reverb Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f));

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
double NewProjectAudioProcessor::getTailLengthSeconds() const { return 10.0; }

int NewProjectAudioProcessor::getNumPrograms()   { return 1; }
int NewProjectAudioProcessor::getCurrentProgram() { return 0; }
void NewProjectAudioProcessor::setCurrentProgram (int) {}
const juce::String NewProjectAudioProcessor::getProgramName (int) { return {}; }
void NewProjectAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto sr = (float) sampleRate;

    // ── Distortion ───────────────────────────────────────────────────────
    oversampling.initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampling.getLatencyInSamples());

    // Pre-clip high-pass at 100 Hz — tight low end for chuggy metal palm-mutes
    preHPL.setCutoff (100.0f, sr);   preHPL.reset();
    preHPR.setCutoff (100.0f, sr);   preHPR.reset();

    // Pre-clip mid-boost at 900 Hz, +8 dB, Q=0.8
    // This is the "metal bite" — cuts through the mix with aggression.
    // (5150, Dual Rectifier, and similar amps do this in their input stage.)
    midBoostL.set (900.0f, 0.8f, 8.0f, sr); midBoostL.reset();
    midBoostR.set (900.0f, 0.8f, 8.0f, sr); midBoostR.reset();

    toneLPL.setCutoff (4000.0f, sr); toneLPL.reset();
    toneLPR.setCutoff (4000.0f, sr); toneLPR.reset();

    distAmountSmoothed.reset (sampleRate, 0.05);
    levelSmoothed.reset (sampleRate, 0.05);

    // ── Delay ────────────────────────────────────────────────────────────
    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };

    delayL.prepare (monoSpec);
    delayR.prepare (monoSpec);
    int maxDelaySmp = (int) (kMaxDelaySeconds * sampleRate) + 1;
    delayL.setMaximumDelayInSamples (maxDelaySmp);
    delayR.setMaximumDelayInSamples (maxDelaySmp);

    delayFbLPL.setCutoff (6000.0f, sr); delayFbLPL.reset();
    delayFbLPR.setCutoff (6000.0f, sr); delayFbLPR.reset();

    delayTimeSmoothL.reset (sampleRate, 0.05);
    delayTimeSmoothR.reset (sampleRate, 0.05);
    lfoPhase = 0.0f;

    // ── Reverb ───────────────────────────────────────────────────────────
    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    reverb.prepare (stereoSpec);
    reverb.reset();

    int maxPreDly = (int) (0.1 * sampleRate) + 1;
    preDelayL.prepare (monoSpec);  preDelayL.setMaximumDelayInSamples (maxPreDly);
    preDelayR.prepare (monoSpec);  preDelayR.setMaximumDelayInSamples (maxPreDly);

    float initHC = 16000.0f - 0.5f * 8000.0f;
    reverbHCL.setCutoff (initHC, sr); reverbHCL.reset();
    reverbHCR.setCutoff (initHC, sr); reverbHCR.reset();

    prevReverbRoom = -1.0f;
    prevReverbDamp = -1.0f;

    // ── Bypass crossfade ─────────────────────────────────────────────────
    distBypassGain.reset     (sampleRate, 0.01);
    delayBypassGain.reset    (sampleRate, 0.01);
    reverbBypassGain.reset   (sampleRate, 0.01);
    distBypassGain.setCurrentAndTargetValue     (0.0f);
    delayBypassGain.setCurrentAndTargetValue    (1.0f);
    reverbBypassGain.setCurrentAndTargetValue   (1.0f);

    tempBuffer.setSize (2, samplesPerBlock);
}

void NewProjectAudioProcessor::syncDSPToParameters() { /* read live in processBlock */ }

void NewProjectAudioProcessor::releaseResources()
{
    oversampling.reset();
    delayL.reset();       delayR.reset();
    preDelayL.reset();    preDelayR.reset();
    reverb.reset();
    preHPL.reset();       preHPR.reset();
    midBoostL.reset();    midBoostR.reset();
    toneLPL.reset();      toneLPR.reset();
    delayFbLPL.reset();   delayFbLPR.reset();
    reverbHCL.reset();    reverbHCR.reset();
}

//==============================================================================
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == juce::AudioChannelSet::stereo()
        || mainIn == juce::AudioChannelSet::mono();
}

//==============================================================================
//  MAIN AUDIO PROCESSING
//==============================================================================
void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    if (getTotalNumInputChannels() < 2 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    const bool distortionOn = *apvts.getRawParameterValue ("distortionOn") >= 0.5f;
    const bool delayOn      = *apvts.getRawParameterValue ("delayOn")      >= 0.5f;
    const bool reverbOn     = *apvts.getRawParameterValue ("reverbOn")     >= 0.5f;

    const float distAmount  = apvts.getRawParameterValue ("distortion")->load();
    const float distTone    = apvts.getRawParameterValue ("distTone")->load();
    const float level       = apvts.getRawParameterValue ("level")->load();
    const float delayTimeMs = apvts.getRawParameterValue ("delayTime")->load();
    const float feedback    = apvts.getRawParameterValue ("delayFeedback")->load();
    const float delayTone   = apvts.getRawParameterValue ("delayTone")->load();
    const float delayMix    = apvts.getRawParameterValue ("delayMix")->load();
    const float reverbRoom  = apvts.getRawParameterValue ("reverbRoom")->load();
    const float reverbPreDly= apvts.getRawParameterValue ("reverbPreDelay")->load();
    const float reverbDamp  = apvts.getRawParameterValue ("reverbDamp")->load();
    const float reverbMix   = apvts.getRawParameterValue ("reverbMix")->load();

    const float sr = (float) currentSampleRate;

    distBypassGain.setTargetValue     (distortionOn ? 1.0f : 0.0f);
    delayBypassGain.setTargetValue    (delayOn      ? 1.0f : 0.0f);
    reverbBypassGain.setTargetValue   (reverbOn     ? 1.0f : 0.0f);

    //======================================================================
    //  1. DISTORTION
    //     Pre-HP(100) → Mid-boost peak EQ(900/Q0.8/+8dB) → Drive gain →
    //     2x oversample → cascaded soft/hard clipper → downsample →
    //     Tone LP → Level
    //======================================================================
    if (distortionOn || distBypassGain.isSmoothing())
    {
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // ─── Pre-clip HP (100 Hz) — tight low end ────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = preHPL.process (L[i]);
            R[i] = preHPR.process (R[i]);
        }

        // ─── Mid-boost peak EQ (pre-saturation "bite") ───────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = midBoostL.process (L[i]);
            R[i] = midBoostR.process (R[i]);
        }

        // ─── Distortion amount (smoothed gain before the shaper) ─────────
        distAmountSmoothed.setTargetValue (distAmount);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = distAmountSmoothed.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        // ─── 2x oversample → cascaded clipper → downsample ───────────────
        float* channels[2] = { L, R };
        juce::dsp::AudioBlock<float> distBlock (channels, 2, (size_t) numSamples);
        auto osBlock = oversampling.processSamplesUp (distBlock);

        auto* osL = osBlock.getChannelPointer (0);
        auto* osR = osBlock.getChannelPointer (1);
        const int osN = (int) osBlock.getNumSamples();

        for (int i = 0; i < osN; ++i)
        {
            osL[i] = distortionShaper (osL[i]);
            osR[i] = distortionShaper (osR[i]);
        }

        oversampling.processSamplesDown (distBlock);

        // ─── Post-clip tone LP ────────────────────────────────────────────
        toneLPL.setCutoff (distTone, sr);
        toneLPR.setCutoff (distTone, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = toneLPL.process (L[i]);
            R[i] = toneLPR.process (R[i]);
        }

        // ─── Level ────────────────────────────────────────────────────────
        levelSmoothed.setTargetValue (level);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = levelSmoothed.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        // ─── Bypass crossfade ─────────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = distBypassGain.getNextValue();
            L[i] = tempBuffer.getSample (0, i) * (1.0f - g) + L[i] * g;
            R[i] = tempBuffer.getSample (1, i) * (1.0f - g) + R[i] * g;
        }
    }
    else
    {
        distAmountSmoothed.skip (numSamples);
        levelSmoothed.skip (numSamples);
        distBypassGain.skip  (numSamples);
    }

    //======================================================================
    //  2. DELAY
    //======================================================================
    if (delayOn || delayBypassGain.isSmoothing())
    {
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        delayFbLPL.setCutoff (delayTone, sr);
        delayFbLPR.setCutoff (delayTone, sr);

        const float dtBaseSmp = delayTimeMs * 0.001f * sr;
        delayTimeSmoothL.setTargetValue (dtBaseSmp);
        delayTimeSmoothR.setTargetValue (dtBaseSmp * kStereoOffsetRatio);

        const float lfoInc    = kLfoRateHz / sr;
        const float lfoDepSmp = kLfoDepthMs * 0.001f * sr;

        const float dMixWet = std::sin (delayMix * juce::MathConstants<float>::halfPi);
        const float dMixDry = std::cos (delayMix * juce::MathConstants<float>::halfPi);

        for (int i = 0; i < numSamples; ++i)
        {
            const float lfoVal = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            const float modSmp = lfoVal * lfoDepSmp;

            float dtL = juce::jmax (1.0f, delayTimeSmoothL.getNextValue() + modSmp);
            float dtR = juce::jmax (1.0f, delayTimeSmoothR.getNextValue() + modSmp);

            delayL.setDelay (dtL);
            delayR.setDelay (dtR);

            const float dryL = L[i];
            const float dryR = R[i];
            const float wetL = delayL.popSample (0);
            const float wetR = delayR.popSample (0);

            const float fbL = delayFbLPL.process (wetL);
            const float fbR = delayFbLPR.process (wetR);
            delayL.pushSample (0, dryL + fbL * feedback);
            delayR.pushSample (0, dryR + fbR * feedback);

            L[i] = dryL * dMixDry + wetL * dMixWet;
            R[i] = dryR * dMixDry + wetR * dMixWet;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = delayBypassGain.getNextValue();
            L[i] = tempBuffer.getSample (0, i) * (1.0f - g) + L[i] * g;
            R[i] = tempBuffer.getSample (1, i) * (1.0f - g) + R[i] * g;
        }
    }
    else
    {
        delayTimeSmoothL.skip (numSamples);
        delayTimeSmoothR.skip (numSamples);
        delayBypassGain.skip  (numSamples);
    }

    //======================================================================
    //  3. REVERB
    //======================================================================
    if (reverbOn || reverbBypassGain.isSmoothing())
    {
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        const float preDelaySmp = juce::jmax (0.0f, reverbPreDly * 0.001f * sr);
        preDelayL.setDelay (preDelaySmp);
        preDelayR.setDelay (preDelaySmp);

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = L[i];
            const float inR = R[i];
            L[i] = preDelayL.popSample (0);
            R[i] = preDelayR.popSample (0);
            preDelayL.pushSample (0, inL);
            preDelayR.pushSample (0, inR);
        }

        if (reverbRoom != prevReverbRoom || reverbDamp != prevReverbDamp)
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize   = reverbRoom;
            rp.damping    = reverbDamp;
            rp.wetLevel   = 1.0f;
            rp.dryLevel   = 0.0f;
            rp.width      = 0.8f;
            rp.freezeMode = 0.0f;
            reverb.setParameters (rp);

            prevReverbRoom = reverbRoom;
            prevReverbDamp = reverbDamp;

            const float hcFreq = 16000.0f - reverbDamp * 8000.0f;
            reverbHCL.setCutoff (hcFreq, sr);
            reverbHCR.setCutoff (hcFreq, sr);
        }

        juce::dsp::AudioBlock<float> reverbBlock (buffer);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (reverbBlock));

        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = reverbHCL.process (L[i]);
            R[i] = reverbHCR.process (R[i]);
        }

        const float rMixWet = std::sin (reverbMix * juce::MathConstants<float>::halfPi);
        const float rMixDry = std::cos (reverbMix * juce::MathConstants<float>::halfPi);

        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = tempBuffer.getSample (0, i) * rMixDry + L[i] * rMixWet;
            R[i] = tempBuffer.getSample (1, i) * rMixDry + R[i] * rMixWet;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = reverbBypassGain.getNextValue();
            L[i] = tempBuffer.getSample (0, i) * (1.0f - g) + L[i] * g;
            R[i] = tempBuffer.getSample (1, i) * (1.0f - g) + R[i] * g;
        }
    }
    else
    {
        reverbBypassGain.skip (numSamples);
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
