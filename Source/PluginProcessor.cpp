#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Asymmetric tube-like waveshaper.
// A small DC bias before tanh breaks symmetry, producing even harmonics
// (2nd, 4th, 6th) — this is the "warmth" of a real tube amp.
// Symmetric tanh only produces odd harmonics (transistor / fuzz character).
//==============================================================================
float NewProjectAudioProcessor::tubeWaveshaper (float x)
{
    // bias = 0.2  →  tanh(0.2) ≈ 0.19738
    // normGain = 1 / sech²(0.2) ≈ 1.04053  (unity gain for small signals)
    constexpr float kBias     = 0.2f;
    constexpr float kTanhBias = 0.19737532022490f;
    constexpr float kNorm     = 1.04053354668475f;
    return (std::tanh (x + kBias) - kTanhBias) * kNorm;
}

//==============================================================================
// Constructor — mono-in/stereo-out AND stereo-in/stereo-out supported
//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      // 2 channels, order 1 (= 2x oversampling), IIR half-band, max quality
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
        "overdriveOn", "Overdrive On", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "delayOn", "Delay On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "reverbOn", "Reverb On", true));

    // ── Overdrive ────────────────────────────────────────────────────────
    // Skew 0.3 → knob's first half covers 1–3.5 (clean→crunch sweet spot)
    // instead of old 0.5 which wasted the upper range on inaudible change
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "drive", "Drive",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.3f), 1.0f));

    // Post-clipper tone control (LP filter cutoff)
    // Skew 0.35 for logarithmic feel on frequency
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "odTone", "OD Tone",
        juce::NormalisableRange<float> (200.0f, 12000.0f, 1.0f, 0.35f), 4000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "level", "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));

    // ── Delay ────────────────────────────────────────────────────────────
    // Minimum 20 ms — prevents DC / meaningless feedback at 0 ms
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayTime", "Delay Time (ms)",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f), 250.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "delayFeedback", "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.4f));

    // Feedback loop LP filter — darkens repeats like analog / tape
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

    // Pre-delay: separates dry attack from reverb onset (clarity for leads)
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

// Feedback 0.95 + 1000 ms delay → tails need ~6 s to reach -60 dB.
// Plus reverb tail → 10 s is a safe ceiling for bounce/export.
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

    // ── Overdrive ────────────────────────────────────────────────────────
    oversampling.initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampling.getLatencyInSamples());

    // Pre-OD high-pass at 250 Hz — kills E2 (82 Hz) fundamental before
    // clipping so two low notes don't create intermodulation mud.
    preHPL.setCutoff (250.0f, sr);   preHPL.reset();
    preHPR.setCutoff (250.0f, sr);   preHPR.reset();

    toneLPL.setCutoff (4000.0f, sr); toneLPL.reset();
    toneLPR.setCutoff (4000.0f, sr); toneLPR.reset();

    driveSmoothed.reset (sampleRate, 0.05);   // 50 ms ramp
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

    delayTimeSmoothL.reset (sampleRate, 0.05);  // 50 ms ramp prevents pitch glitch
    delayTimeSmoothR.reset (sampleRate, 0.05);
    lfoPhase = 0.0f;

    // ── Reverb ───────────────────────────────────────────────────────────
    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    reverb.prepare (stereoSpec);
    reverb.reset();

    // Pre-delay lines (max 100 ms)
    int maxPreDly = (int) (0.1 * sampleRate) + 1;
    preDelayL.prepare (monoSpec);  preDelayL.setMaximumDelayInSamples (maxPreDly);
    preDelayR.prepare (monoSpec);  preDelayR.setMaximumDelayInSamples (maxPreDly);

    // High-cut on reverb wet (derived from damping — default 0.5)
    float initHC = 16000.0f - 0.5f * 8000.0f;
    reverbHCL.setCutoff (initHC, sr); reverbHCL.reset();
    reverbHCR.setCutoff (initHC, sr); reverbHCR.reset();

    prevReverbRoom = -1.0f;
    prevReverbDamp = -1.0f;

    // ── Bypass crossfade (10 ms ramp — click-free toggle) ────────────────
    odBypassGain.reset     (sampleRate, 0.01);
    delayBypassGain.reset  (sampleRate, 0.01);
    reverbBypassGain.reset (sampleRate, 0.01);
    odBypassGain.setCurrentAndTargetValue     (0.0f);   // OD defaults to off
    delayBypassGain.setCurrentAndTargetValue  (1.0f);   // Delay defaults to on
    reverbBypassGain.setCurrentAndTargetValue (1.0f);   // Reverb defaults to on

    // ── Pre-allocated temp buffer (zero audio-thread allocations) ────────
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
    toneLPL.reset();      toneLPR.reset();
    delayFbLPL.reset();   delayFbLPR.reset();
    reverbHCL.reset();    reverbHCR.reset();
}

//==============================================================================
// Bus layout: stereo→stereo (default) and mono→stereo (guitar DI)
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

    // ── Handle mono input: copy ch0 → ch1 for stereo processing ──────────
    if (getTotalNumInputChannels() < 2 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // ── Read bypass flags ────────────────────────────────────────────────
    const bool overdriveOn = *apvts.getRawParameterValue ("overdriveOn") >= 0.5f;
    const bool delayOn     = *apvts.getRawParameterValue ("delayOn")     >= 0.5f;
    const bool reverbOn    = *apvts.getRawParameterValue ("reverbOn")    >= 0.5f;

    // ── Read all parameters (lock-free atomic loads) ─────────────────────
    const float drive       = apvts.getRawParameterValue ("drive")->load();
    const float odTone      = apvts.getRawParameterValue ("odTone")->load();
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

    // ── Update bypass crossfade targets ──────────────────────────────────
    odBypassGain.setTargetValue     (overdriveOn ? 1.0f : 0.0f);
    delayBypassGain.setTargetValue  (delayOn     ? 1.0f : 0.0f);
    reverbBypassGain.setTargetValue (reverbOn    ? 1.0f : 0.0f);

    //======================================================================
    //  1. OVERDRIVE
    //     Pre-HP → Drive gain → 2x oversample → tube waveshaper →
    //     downsample → Tone LP → Level gain
    //======================================================================
    if (overdriveOn || odBypassGain.isSmoothing())
    {
        // Save dry signal for bypass crossfade
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // ─── Pre-OD high-pass (250 Hz) ──────────────────────────────────
        // Removes low-end fundamentals before clipping.
        // Prevents intermodulation distortion (E2+A2 = 28 Hz ghost tone).
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = preHPL.process (L[i]);
            R[i] = preHPR.process (R[i]);
        }

        // ─── Drive gain (smoothed — no zipper noise) ────────────────────
        driveSmoothed.setTargetValue (drive);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = driveSmoothed.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        // ─── 2x Oversampling + asymmetric tube waveshaper ───────────────
        // Upsample to 88.2/96 kHz, apply nonlinearity, downsample.
        // This pushes aliasing artifacts above the audible range.
        float* channels[2] = { L, R };
        juce::dsp::AudioBlock<float> odBlock (channels, 2, (size_t) numSamples);
        auto osBlock = oversampling.processSamplesUp (odBlock);

        auto* osL = osBlock.getChannelPointer (0);
        auto* osR = osBlock.getChannelPointer (1);
        const int osN = (int) osBlock.getNumSamples();

        for (int i = 0; i < osN; ++i)
        {
            osL[i] = tubeWaveshaper (osL[i]);
            osR[i] = tubeWaveshaper (osR[i]);
        }

        oversampling.processSamplesDown (odBlock);

        // ─── Post-OD tone control (low-pass) ────────────────────────────
        // Tames harsh high-frequency harmonics generated by the waveshaper.
        // Without this, high gain = glass-shattering treble.
        toneLPL.setCutoff (odTone, sr);
        toneLPR.setCutoff (odTone, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = toneLPL.process (L[i]);
            R[i] = toneLPR.process (R[i]);
        }

        // ─── Level gain (smoothed) ──────────────────────────────────────
        levelSmoothed.setTargetValue (level);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = levelSmoothed.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        // ─── Bypass crossfade (10 ms ramp — no pop/click) ───────────────
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = odBypassGain.getNextValue();
            L[i] = tempBuffer.getSample (0, i) * (1.0f - g) + L[i] * g;
            R[i] = tempBuffer.getSample (1, i) * (1.0f - g) + R[i] * g;
        }
    }
    else
    {
        // Fully bypassed — skip processing, advance smoothers
        driveSmoothed.skip (numSamples);
        levelSmoothed.skip (numSamples);
        odBypassGain.skip  (numSamples);
    }

    //======================================================================
    //  2. DELAY
    //     Stereo offset (L/R 7% difference) + LFO modulation +
    //     feedback LP filter + equal-power crossfade
    //======================================================================
    if (delayOn || delayBypassGain.isSmoothing())
    {
        // Save dry for bypass crossfade
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // Update feedback filter cutoff (darkens repeats like analog/tape)
        delayFbLPL.setCutoff (delayTone, sr);
        delayFbLPR.setCutoff (delayTone, sr);

        // Target delay times — R channel 7% longer for stereo width
        const float dtBaseSmp = delayTimeMs * 0.001f * sr;
        delayTimeSmoothL.setTargetValue (dtBaseSmp);
        delayTimeSmoothR.setTargetValue (dtBaseSmp * kStereoOffsetRatio);

        // LFO constants
        const float lfoInc    = kLfoRateHz / sr;
        const float lfoDepSmp = kLfoDepthMs * 0.001f * sr;

        // Equal-power crossfade gains (constant across block)
        const float dMixWet = std::sin (delayMix * juce::MathConstants<float>::halfPi);
        const float dMixDry = std::cos (delayMix * juce::MathConstants<float>::halfPi);

        for (int i = 0; i < numSamples; ++i)
        {
            // ── LFO (subtle chorus on repeats — analog character) ────────
            const float lfoVal = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            const float modSmp = lfoVal * lfoDepSmp;

            // ── Smoothed delay time (prevents pitch glitch on knob turn) ─
            float dtL = juce::jmax (1.0f, delayTimeSmoothL.getNextValue() + modSmp);
            float dtR = juce::jmax (1.0f, delayTimeSmoothR.getNextValue() + modSmp);

            delayL.setDelay (dtL);
            delayR.setDelay (dtR);

            // ── Read from delay lines ────────────────────────────────────
            const float dryL = L[i];
            const float dryR = R[i];
            const float wetL = delayL.popSample (0);
            const float wetR = delayR.popSample (0);

            // ── Feedback with LP filter (each repeat gets darker) ────────
            const float fbL = delayFbLPL.process (wetL);
            const float fbR = delayFbLPR.process (wetR);
            delayL.pushSample (0, dryL + fbL * feedback);
            delayR.pushSample (0, dryR + fbR * feedback);

            // ── Equal-power crossfade (no volume dip at 50% mix) ─────────
            L[i] = dryL * dMixDry + wetL * dMixWet;
            R[i] = dryR * dMixDry + wetR * dMixWet;
        }

        // ── Bypass crossfade ─────────────────────────────────────────────
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
    //     Pre-delay → Freeverb (wet only) → high-cut on wet →
    //     equal-power crossfade with dry
    //======================================================================
    if (reverbOn || reverbBypassGain.isSmoothing())
    {
        // Save dry for crossfade
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // ─── Pre-delay (separates attack from reverb tail) ───────────────
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

        // ─── Update reverb params only when changed (no zipper noise) ────
        if (reverbRoom != prevReverbRoom || reverbDamp != prevReverbDamp)
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize   = reverbRoom;
            rp.damping    = reverbDamp;
            rp.wetLevel   = 1.0f;    // wet only — we do our own crossfade
            rp.dryLevel   = 0.0f;
            rp.width      = 0.8f;    // good default stereo spread
            rp.freezeMode = 0.0f;
            reverb.setParameters (rp);

            prevReverbRoom = reverbRoom;
            prevReverbDamp = reverbDamp;

            // High-cut on wet signal, derived from damping:
            //   damp=0 → 16 kHz (bright)   damp=1 → 8 kHz (warm)
            const float hcFreq = 16000.0f - reverbDamp * 8000.0f;
            reverbHCL.setCutoff (hcFreq, sr);
            reverbHCR.setCutoff (hcFreq, sr);
        }

        // ─── Process reverb (wet-only output) ────────────────────────────
        juce::dsp::AudioBlock<float> reverbBlock (buffer);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (reverbBlock));

        // ─── High-cut on reverb wet (tames metallic Freeverb treble) ─────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = reverbHCL.process (L[i]);
            R[i] = reverbHCR.process (R[i]);
        }

        // ─── Equal-power crossfade with dry ──────────────────────────────
        const float rMixWet = std::sin (reverbMix * juce::MathConstants<float>::halfPi);
        const float rMixDry = std::cos (reverbMix * juce::MathConstants<float>::halfPi);

        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = tempBuffer.getSample (0, i) * rMixDry + L[i] * rMixWet;
            R[i] = tempBuffer.getSample (1, i) * rMixDry + R[i] * rMixWet;
        }

        // ─── Bypass crossfade ────────────────────────────────────────────
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
