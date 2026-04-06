#include "PluginProcessor.h"
#include "PluginEditor.h"

// NAM Core includes
#include "NAM/get_dsp.h"

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Try to load the bundled default model on construction
    // The model file should be next to the plugin binary or in a known location
    auto exeDir = juce::File::getSpecialLocation (
        juce::File::SpecialLocationType::currentApplicationFile).getParentDirectory();

    // Search order: next to binary, then in Models/ subfolder
    juce::File defaultModel = exeDir.getChildFile ("default.nam");
    if (!defaultModel.existsAsFile())
        defaultModel = exeDir.getChildFile ("Models").getChildFile ("default.nam");
    if (!defaultModel.existsAsFile())
    {
        // During development: check project source directory
        auto projectModels = juce::File ("/Users/ulaskavuncuoglu/Documents/Amp-Plugin-Project-main/Models/default.nam");
        if (projectModels.existsAsFile())
            defaultModel = projectModels;
    }

    if (defaultModel.existsAsFile())
        loadNAMModel (defaultModel);
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() {}

//==============================================================================
void NewProjectAudioProcessor::loadNAMModel (const juce::File& namFile)
{
    if (!namFile.existsAsFile())
        return;

    try
    {
        auto newModel = nam::get_dsp (namFile.getFullPathName().toStdString());
        if (newModel)
        {
            // Initialize model with current sample rate
            const double sr = (currentSampleRate > 0) ? currentSampleRate : 48000.0;
            const int maxBuf = 4096;
            newModel->ResetAndPrewarm (sr, maxBuf);

            // Hot-swap under lock
            const juce::SpinLock::ScopedLockType lock (namModelLock);
            namModel = std::move (newModel);
            currentModelName = namFile.getFileNameWithoutExtension();
        }
    }
    catch (const std::exception& e)
    {
        DBG ("NAM model load failed: " << e.what());
    }
}

//==============================================================================
// NAM Model Directory Management
//==============================================================================
juce::File NewProjectAudioProcessor::getModelsDirectory() const
{
    // Use a persistent "Models" folder in the user's app data
    auto appData = juce::File::getSpecialLocation (
        juce::File::userApplicationDataDirectory)
        .getChildFile ("NewProject").getChildFile ("Models");

    if (!appData.isDirectory())
        appData.createDirectory();

    // On first run, copy bundled models into the app data directory
    auto bundledDir = juce::File::getSpecialLocation (
        juce::File::currentApplicationFile).getParentDirectory().getChildFile ("Models");

    // Also check project source dir during development
    if (!bundledDir.isDirectory())
        bundledDir = juce::File ("/Users/ulaskavuncuoglu/Documents/Amp-Plugin-Project-main/Models");

    if (bundledDir.isDirectory())
    {
        for (auto& f : bundledDir.findChildFiles (juce::File::findFiles, false, "*.nam"))
        {
            auto dest = appData.getChildFile (f.getFileName());
            if (!dest.existsAsFile())
                f.copyFileTo (dest);
        }
    }

    return appData;
}

juce::StringArray NewProjectAudioProcessor::getAvailableModelNames() const
{
    juce::StringArray names;
    auto dir = getModelsDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.nam");
        files.sort();
        for (auto& f : files)
            names.add (f.getFileNameWithoutExtension());
    }
    return names;
}

void NewProjectAudioProcessor::loadModelByName (const juce::String& name)
{
    auto dir = getModelsDirectory();
    auto file = dir.getChildFile (name + ".nam");
    if (file.existsAsFile())
        loadNAMModel (file);
}

void NewProjectAudioProcessor::importNAMModel (const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile() || sourceFile.getFileExtension().toLowerCase() != ".nam")
        return;

    auto dir = getModelsDirectory();
    auto dest = dir.getChildFile (sourceFile.getFileName());

    // If file already exists, add a number suffix
    int counter = 1;
    while (dest.existsAsFile())
    {
        dest = dir.getChildFile (sourceFile.getFileNameWithoutExtension()
                                 + " (" + juce::String (counter++) + ").nam");
    }

    sourceFile.copyFileTo (dest);
    loadNAMModel (dest);
}

int NewProjectAudioProcessor::getCurrentModelIndex() const
{
    auto names = getAvailableModelNames();
    return names.indexOf (currentModelName);
}

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

    // ── Distortion (NAM) ────────────────────────────────────────────────
    // Drive: input gain into the neural network (0-1 → -12..+12 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "distDrive", "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.4f), 0.3f));

    // 3-band tone stack (post-NAM)
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

    // ── NAM model: reset to current sample rate ─────────────────────────
    {
        const juce::SpinLock::ScopedLockType lock (namModelLock);
        if (namModel)
            namModel->ResetAndPrewarm (sampleRate, samplesPerBlock);
    }

    // ── NAM internal buffers (double precision) ─────────────────────────
    namInputBuffer.resize ((size_t) samplesPerBlock, 0.0);
    namOutputBuffer.resize ((size_t) samplesPerBlock, 0.0);

    // ── Noise gate ──────────────────────────────────────────────────────
    gateL.prepare (sr); gateL.reset();
    gateR.prepare (sr); gateR.reset();

    // ── Pre-NAM HP at 80 Hz (remove sub-bass rumble before neural net) ──
    preHPL.setCutoff (80.0f, sr); preHPL.reset();
    preHPR.setCutoff (80.0f, sr); preHPR.reset();

    // ── Post-NAM 3-band tone stack (flat defaults) ──────────────────────
    bassEqL.setLowShelf (200.0f, 0.0f, sr);    bassEqL.reset();
    bassEqR.setLowShelf (200.0f, 0.0f, sr);    bassEqR.reset();
    midEqL.setPeakEQ (800.0f, 0.7f, 0.0f, sr); midEqL.reset();
    midEqR.setPeakEQ (800.0f, 0.7f, 0.0f, sr); midEqR.reset();
    trebleEqL.setHighShelf (3500.0f, 0.0f, sr); trebleEqL.reset();
    trebleEqR.setHighShelf (3500.0f, 0.0f, sr); trebleEqR.reset();

    // ── Cabinet simulation (SM57 on 4x12 V30) ──────────────────────────
    cabHPL.setHighPass (70.0f, sr);               cabHPL.reset();
    cabHPR.setHighPass (70.0f, sr);               cabHPR.reset();
    cabResoL.setPeakEQ (120.0f, 1.5f, 3.0f, sr);  cabResoL.reset();
    cabResoR.setPeakEQ (120.0f, 1.5f, 3.0f, sr);  cabResoR.reset();
    cabBoxL.setPeakEQ (400.0f, 0.8f, -3.0f, sr);  cabBoxL.reset();
    cabBoxR.setPeakEQ (400.0f, 0.8f, -3.0f, sr);  cabBoxR.reset();
    cabPresL.setPeakEQ (2500.0f, 1.2f, 2.0f, sr); cabPresL.reset();
    cabPresR.setPeakEQ (2500.0f, 1.2f, 2.0f, sr); cabPresR.reset();
    cabNotchL.setPeakEQ (4500.0f, 2.0f, -6.0f, sr); cabNotchL.reset();
    cabNotchR.setPeakEQ (4500.0f, 2.0f, -6.0f, sr); cabNotchR.reset();
    cabLPL.setLowPass (5500.0f, sr);              cabLPL.reset();
    cabLPR.setLowPass (5500.0f, sr);              cabLPR.reset();
    cabLP2L.setLowPass (8000.0f, sr, 0.5f);       cabLP2L.reset();
    cabLP2R.setLowPass (8000.0f, sr, 0.5f);       cabLP2R.reset();

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
    delayL.reset();       delayR.reset();
    preDelayL.reset();    preDelayR.reset();
    reverb.reset();
    preHPL.reset();       preHPR.reset();
    bassEqL.reset();      bassEqR.reset();
    midEqL.reset();       midEqR.reset();
    trebleEqL.reset();    trebleEqR.reset();
    cabHPL.reset();       cabHPR.reset();
    cabResoL.reset();     cabResoR.reset();
    cabBoxL.reset();      cabBoxR.reset();
    cabPresL.reset();     cabPresR.reset();
    cabNotchL.reset();    cabNotchR.reset();
    cabLPL.reset();       cabLPR.reset();
    cabLP2L.reset();      cabLP2R.reset();
    delayFbLPL.reset();   delayFbLPR.reset();
    reverbHCL.reset();    reverbHCR.reset();
    gateL.reset();        gateR.reset();
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

    // Mono -> stereo
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
    //  1. DISTORTION — Neural Amp Modeler (Mesa Boogie neural inference)
    //
    //     NoiseGate → PreHP(80Hz) → Drive Gain →
    //     NAM Neural Inference (mono, process L+R separately) →
    //     Bass Shelf(200Hz) → Mid Peak(800Hz) → Treble Shelf(3.5kHz) →
    //     Cabinet Sim (SM57/4x12/V30) → Level
    //======================================================================
    if (distortionOn || distBypassGain.isSmoothing())
    {
        // Save dry for bypass crossfade
        tempBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        tempBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // Configure noise gate
        gateL.setThreshold (distGate);
        gateR.setThreshold (distGate);

        // Configure tone stack: 0-1 -> -12..+12 dB
        const float bassDb   = (distBass   - 0.5f) * 24.0f;
        const float midDb    = (distMid    - 0.5f) * 24.0f;
        const float trebleDb = (distTreble - 0.5f) * 24.0f;
        bassEqL.setLowShelf  (200.0f,  bassDb,   sr);
        bassEqR.setLowShelf  (200.0f,  bassDb,   sr);
        midEqL.setPeakEQ     (800.0f,  0.7f, midDb, sr);
        midEqR.setPeakEQ     (800.0f,  0.7f, midDb, sr);
        trebleEqL.setHighShelf(3500.0f, trebleDb, sr);
        trebleEqR.setHighShelf(3500.0f, trebleDb, sr);

        // ─── Drive gain: 0-1 -> -12..+12 dB input boost into NAM ────────
        const float driveDb = (distDrive - 0.5f) * 24.0f;
        const float driveGain = std::pow (10.0f, driveDb / 20.0f);

        // ─── Noise Gate ─────────────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = gateL.process (L[i]);
            R[i] = gateR.process (R[i]);
        }

        // ─── Pre-NAM HP (80 Hz) ────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = preHPL.process (L[i]);
            R[i] = preHPR.process (R[i]);
        }

        // ─── Apply drive gain ──────────────────────────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] *= driveGain;
            R[i] *= driveGain;
        }

        // ─── NAM Neural Inference ──────────────────────────────────────
        // NAM is mono with internal state — we mix L+R to mono, process once,
        // then write the result back to both channels.
        // NAM expects double** (NAM_SAMPLE = double by default).
        {
            const juce::SpinLock::ScopedTryLockType lock (namModelLock);
            if (lock.isLocked() && namModel)
            {
                // Ensure buffers are large enough
                if ((int) namInputBuffer.size() < numSamples)
                {
                    namInputBuffer.resize ((size_t) numSamples);
                    namOutputBuffer.resize ((size_t) numSamples);
                }

                // ── Mix L+R to mono (equal power sum) ───────────────────
                for (int i = 0; i < numSamples; ++i)
                    namInputBuffer[(size_t)i] = (double) (L[i] + R[i]) * 0.5;

                double* inPtr  = namInputBuffer.data();
                double* outPtr = namOutputBuffer.data();
                double* inPtrs[1]  = { inPtr };
                double* outPtrs[1] = { outPtr };
                namModel->process (inPtrs, outPtrs, numSamples);

                // ── Write mono result back to both channels ─────────────
                for (int i = 0; i < numSamples; ++i)
                {
                    float out = (float) namOutputBuffer[(size_t)i];
                    L[i] = out;
                    R[i] = out;
                }
            }
        }

        // ─── Post-NAM 3-band tone stack ─────────────────────────────────
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

        // ─── Cabinet simulation (SM57 on 4x12 V30) ────────────────────
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabHPL.process (L[i]);
            R[i] = cabHPR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabResoL.process (L[i]);
            R[i] = cabResoR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabBoxL.process (L[i]);
            R[i] = cabBoxR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabPresL.process (L[i]);
            R[i] = cabPresR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabNotchL.process (L[i]);
            R[i] = cabNotchR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabLPL.process (L[i]);
            R[i] = cabLPR.process (R[i]);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = cabLP2L.process (L[i]);
            R[i] = cabLP2R.process (R[i]);
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
