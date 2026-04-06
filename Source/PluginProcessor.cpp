#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      // 2 channels, order 1 (2x oversampling), IIR half-band
      oversampling (2, 1,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                    true)
{
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() {}

//==============================================================================
// Parameter layout — 17 parameters
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

    // ── Distortion ──────────────────────────────────────────────────────
    // Drive 0-1: internally mapped to -20..+50 dB per gain stage
    // (like ToobAmp).  Skew 0.4 puts crunch sweet spot at ~30% knob.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distDrive", "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.4f), 0.3f));

    // 3-band tone stack (post-distortion, like amp tone controls)
    // 0.0 = -12 dB, 0.5 = flat, 1.0 = +12 dB
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distBass", "Bass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distMid", "Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distTreble", "Treble",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // Noise gate threshold (-80 = off, -20 = aggressive)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distGate", "Gate",
        juce::NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -55.0f));

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

    // Noise gate
    gateL.prepare (sr); gateL.reset();
    gateR.prepare (sr); gateR.reset();

    // Pre-distortion HP at 100 Hz (tight low-end for chuggy metal)
    preHPL.setCutoff (100.0f, sr); preHPL.reset();
    preHPR.setCutoff (100.0f, sr); preHPR.reset();

    // Pre-distortion mid-boost: +8 dB at 900 Hz, Q=0.8
    // This is the "metal bite" — 5150/Rectifier-style pre-gain EQ push
    preEqL.setPeakEQ (900.0f, 0.8f, 8.0f, sr); preEqL.reset();
    preEqR.setPeakEQ (900.0f, 0.8f, 8.0f, sr); preEqR.reset();

    // Power sag operates at oversampled rate
    const float osRate = sr * 2.0f;
    sagL.prepare (osRate); sagL.reset();
    sagR.prepare (osRate); sagR.reset();

    // Post-distortion 3-band tone stack (flat defaults)
    bassEqL.setLowShelf (200.0f, 0.0f, sr);    bassEqL.reset();
    bassEqR.setLowShelf (200.0f, 0.0f, sr);    bassEqR.reset();
    midEqL.setPeakEQ (800.0f, 0.7f, 0.0f, sr); midEqL.reset();
    midEqR.setPeakEQ (800.0f, 0.7f, 0.0f, sr); midEqR.reset();
    trebleEqL.setHighShelf (3500.0f, 0.0f, sr); trebleEqL.reset();
    trebleEqR.setHighShelf (3500.0f, 0.0f, sr); trebleEqR.reset();

    distSmoothed.reset (sampleRate, 0.05);
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
    reverb.prepare (stereoSpec); reverb.reset();
    int maxPreDly = (int) (0.1 * sampleRate) + 1;
    preDelayL.prepare (monoSpec); preDelayL.setMaximumDelayInSamples (maxPreDly);
    preDelayR.prepare (monoSpec); preDelayR.setMaximumDelayInSamples (maxPreDly);
    float initHC = 16000.0f - 0.5f * 8000.0f;
    reverbHCL.setCutoff (initHC, sr); reverbHCL.reset();
    reverbHCR.setCutoff (initHC, sr); reverbHCR.reset();
    prevReverbRoom = -1.0f;
    prevReverbDamp = -1.0f;

    // ── Bypass crossfade ─────────────────────────────────────────────────
    distBypassGain.reset  (sampleRate, 0.01);
    delayBypassGain.reset (sampleRate, 0.01);
    reverbBypassGain.reset(sampleRate, 0.01);
    distBypassGain.setCurrentAndTargetValue  (0.0f);
    delayBypassGain.setCurrentAndTargetValue (1.0f);
    reverbBypassGain.setCurrentAndTargetValue(1.0f);

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
    preEqL.reset();       preEqR.reset();
    bassEqL.reset();      bassEqR.reset();
    midEqL.reset();       midEqR.reset();
    trebleEqL.reset();    trebleEqR.reset();
    delayFbLPL.reset();   delayFbLPR.reset();
    reverbHCL.reset();    reverbHCR.reset();
    gateL.reset();        gateR.reset();
    sagL.reset();         sagR.reset();
}

//==============================================================================
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo()) return false;
    return mainIn == juce::AudioChannelSet::stereo()
        || mainIn == juce::AudioChannelSet::mono();
}

//==============================================================================
//  MAIN AUDIO PROCESSING
//==============================================================================
void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Mono → stereo
    if (getTotalNumInputChannels() < 2 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // ── Read bypass flags ────────────────────────────────────────────────
    const bool distortionOn = *apvts.getRawParameterValue ("distortionOn") >= 0.5f;
    const bool delayOn      = *apvts.getRawParameterValue ("delayOn")      >= 0.5f;
    const bool reverbOn     = *apvts.getRawParameterValue ("reverbOn")     >= 0.5f;

    // ── Read all parameters ──────────────────────────────────────────────
    const float distDrive   = apvts.getRawParameterValue ("distDrive")->load();
    const float distBass    = apvts.getRawParameterValue ("distBass")->load();
    const float distMid     = apvts.getRawParameterValue ("distMid")->load();
    const float distTreble  = apvts.getRawParameterValue ("distTreble")->load();
    const float distGate    = apvts.getRawParameterValue ("distGate")->load();
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

    // ── Bypass crossfade targets ─────────────────────────────────────────
    distBypassGain.setTargetValue  (distortionOn ? 1.0f : 0.0f);
    delayBypassGain.setTargetValue (delayOn      ? 1.0f : 0.0f);
    reverbBypassGain.setTargetValue(reverbOn     ? 1.0f : 0.0f);

    //======================================================================
    //  1. DISTORTION — ToobAmp-inspired architecture
    //
    //     NoiseGate → PreHP(100Hz) → PreEQ(+8dB @900Hz) →
    //     2x Oversample →
    //       GainStage1 (atan + normalize, drive-mapped) →
    //       GainStage2 (atan + normalize, 70% of drive) →
    //       GainStage3 (atan + normalize, kicks in >33% drive) →
    //       Power Sag (PSU droop simulation) →
    //     2x Downsample →
    //     Bass Shelf(200Hz) → Mid Peak(800Hz) → Treble Shelf(3.5kHz) →
    //     Level
    //======================================================================
    if (distortionOn || distBypassGain.isSmoothing())
    {
        // Save dry for bypass crossfade
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // ── Configure gain stages (like ToobAmp UpdateControls) ─────────
        // Stage 1: full drive
        stage1.configure (distDrive, 0.3f);
        // Stage 2: 70% of drive (slightly less aggressive)
        stage2.configure (distDrive * 0.7f, 0.2f);
        // Stage 3: kicks in above 33% drive, max at 100%
        float s3drive = juce::jmax (0.0f, distDrive * 1.5f - 0.5f);
        stage3.configure (s3drive, 0.15f);
        const bool stage3Active = (s3drive > 0.01f);

        // Configure power sag (auto-scales with drive)
        sagL.setSagAmount (distDrive);
        sagR.setSagAmount (distDrive);

        // Configure noise gate
        gateL.setThreshold (distGate);
        gateR.setThreshold (distGate);

        // Configure tone stack: 0-1 → -12..+12 dB
        const float bassDb   = (distBass   - 0.5f) * 24.0f;
        const float midDb    = (distMid    - 0.5f) * 24.0f;
        const float trebleDb = (distTreble - 0.5f) * 24.0f;
        bassEqL.setLowShelf  (200.0f,  bassDb,   sr);
        bassEqR.setLowShelf  (200.0f,  bassDb,   sr);
        midEqL.setPeakEQ     (800.0f,  0.7f, midDb, sr);
        midEqR.setPeakEQ     (800.0f,  0.7f, midDb, sr);
        trebleEqL.setHighShelf(3500.0f, trebleDb, sr);
        trebleEqR.setHighShelf(3500.0f, trebleDb, sr);

        // ─── Noise Gate ─────────────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = gateL.process (L[i]);
            R[i] = gateR.process (R[i]);
        }

        // ─── Pre-distortion HP (100 Hz) ─────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = preHPL.process (L[i]);
            R[i] = preHPR.process (R[i]);
        }

        // ─── Pre-distortion mid-boost EQ ────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = preEqL.process (L[i]);
            R[i] = preEqR.process (R[i]);
        }

        // ─── 2x Oversampling + cascaded gain stages + sag ──────────────
        float* channels[2] = { L, R };
        juce::dsp::AudioBlock<float> distBlock (channels, 2, (size_t) numSamples);
        auto osBlock = oversampling.processSamplesUp (distBlock);

        auto* osL = osBlock.getChannelPointer (0);
        auto* osR = osBlock.getChannelPointer (1);
        const int osN = (int) osBlock.getNumSamples();

        for (int i = 0; i < osN; ++i)
        {
            float xL = osL[i], xR = osR[i];

            // Stage 1: primary saturation
            xL = stage1.process (xL);
            xR = stage1.process (xR);

            // Stage 2: harmonic stacking (2nd + 3rd → 5th, 7th)
            xL = stage2.process (xL);
            xR = stage2.process (xR);

            // Stage 3: extreme harmonics (death metal territory)
            if (stage3Active)
            {
                xL = stage3.process (xL);
                xR = stage3.process (xR);
            }

            // Power supply sag: loud signal → output compresses
            xL = sagL.process (xL);
            xR = sagR.process (xR);

            osL[i] = xL;
            osR[i] = xR;
        }

        oversampling.processSamplesDown (distBlock);

        // ─── Post-distortion 3-band tone stack ──────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = bassEqL.process (L[i]);
            R[i] = bassEqR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = midEqL.process (L[i]);
            R[i] = midEqR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = trebleEqL.process (L[i]);
            R[i] = trebleEqR.process (R[i]);
        }

        // ─── Level (smoothed) ───────────────────────────────────────────
        levelSmoothed.setTargetValue (level);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = levelSmoothed.getNextValue();
            L[i] *= g;
            R[i] *= g;
        }

        // ─── Bypass crossfade ───────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = distBypassGain.getNextValue();
            L[i] = tempBuffer.getSample (0, i) * (1.0f - g) + L[i] * g;
            R[i] = tempBuffer.getSample (1, i) * (1.0f - g) + R[i] * g;
        }
    }
    else
    {
        distSmoothed.skip (numSamples);
        levelSmoothed.skip (numSamples);
        distBypassGain.skip (numSamples);
    }

    //======================================================================
    //  2. DELAY — stereo offset + LFO modulation + feedback LP
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

            const float dryL = L[i], dryR = R[i];
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
    //  3. REVERB — pre-delay + Freeverb + high-cut + crossfade
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
            const float inL = L[i], inR = R[i];
            L[i] = preDelayL.popSample (0);
            R[i] = preDelayR.popSample (0);
            preDelayL.pushSample (0, inL);
            preDelayR.pushSample (0, inR);
        }

        if (reverbRoom != prevReverbRoom || reverbDamp != prevReverbDamp)
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize = reverbRoom; rp.damping = reverbDamp;
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
            rp.width = 0.8f; rp.freezeMode = 0.0f;
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
