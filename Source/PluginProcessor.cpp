#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace forge;

//==============================================================================
ForgeAmpProcessor::ForgeAmpProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    cacheParameterPointers();

    presets = std::make_unique<PresetManager> (
        apvts,
        [this] (const juce::XmlElement* extra)
        {
            const auto path = extra != nullptr ? extra->getStringAttribute ("userIR") : juce::String();
            if (path.isNotEmpty() && juce::File (path).existsAsFile())
                loadUserImpulseResponse (juce::File (path), false);
        },
        [this] (juce::XmlElement& extra)
        {
            if (userIrLoaded.load())
                extra.setAttribute ("userIR", getUserImpulseResponseFile().getFullPathName());
        });
}

ForgeAmpProcessor::~ForgeAmpProcessor() = default;

void ForgeAmpProcessor::cacheParameterPointers()
{
    auto get = [this] (const char* id)
    {
        auto* v = apvts.getRawParameterValue (id);
        jassert (v != nullptr);
        return v;
    };

    p.inputGain = get (ids::inputGain);           p.outputGain = get (ids::outputGain);
    p.gateOn = get (ids::gateOn);                 p.gateThreshold = get (ids::gateThreshold);
    p.gateRelease = get (ids::gateRelease);
    p.boostOn = get (ids::boostOn);               p.boostDrive = get (ids::boostDrive);
    p.boostTone = get (ids::boostTone);           p.boostLevel = get (ids::boostLevel);
    p.ampOn = get (ids::ampOn);                   p.ampModel = get (ids::ampModel);
    p.ampGain = get (ids::ampGain);               p.ampBass = get (ids::ampBass);
    p.ampMid = get (ids::ampMid);                 p.ampTreble = get (ids::ampTreble);
    p.ampPresence = get (ids::ampPresence);       p.ampDepth = get (ids::ampDepth);
    p.ampMaster = get (ids::ampMaster);
    p.cabOn = get (ids::cabOn);                   p.cabModel = get (ids::cabModel);
    p.cabMic = get (ids::cabMic);                 p.cabLowCut = get (ids::cabLowCut);
    p.cabHighCut = get (ids::cabHighCut);
    p.delayOn = get (ids::delayOn);               p.delaySync = get (ids::delaySync);
    p.delayTime = get (ids::delayTime);           p.delayDivision = get (ids::delayDivision);
    p.delayFeedback = get (ids::delayFeedback);   p.delayTone = get (ids::delayTone);
    p.delayMix = get (ids::delayMix);             p.delayPingPong = get (ids::delayPingPong);
    p.reverbOn = get (ids::reverbOn);             p.reverbSize = get (ids::reverbSize);
    p.reverbDecay = get (ids::reverbDecay);       p.reverbPreDelay = get (ids::reverbPreDelay);
    p.reverbTone = get (ids::reverbTone);         p.reverbMix = get (ids::reverbMix);
}

//==============================================================================
void ForgeAmpProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    maxBlock = std::max (1, samplesPerBlock);

    // Polyphase IIR half-band filters: low latency, and integer latency so the host can
    // compensate exactly. The whole guitar chain always runs through the oversampler, so
    // latency stays the same whether modules are on or off.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, kOversamplingOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    setLatencySamples ((int) std::lround (oversampler->getLatencyInSamples()));

    const double osRate = sampleRate * (double) (1 << kOversamplingOrder);

    gate.prepare (sampleRate);
    boost.prepare (osRate);
    amp.prepare (osRate);
    cab.prepare (sampleRate, maxBlock);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);

    ampSwitch.prepare (osRate, 12.0);
    cabSwitch.prepare (sampleRate, 12.0);

    const double controlRate = osRate / (double) kControlInterval;
    for (auto* s : { &sGain, &sBass, &sMid, &sTreble, &sPresence, &sDepth, &sMaster,
                     &sBoostDrive, &sBoostTone, &sBoostLevel })
        s->reset (controlRate, 0.04);

    inGain.reset (sampleRate, 0.03);
    outGain.reset (sampleRate, 0.03);
    cabMix.reset (sampleRate, 0.015);
    boostMix.reset (osRate, 0.015);
    ampMix.reset (osRate, 0.015);

    monoBuffer.setSize (1, maxBlock);
    dryBuffer.setSize (2, maxBlock);
    firstBlock = true;
}

void ForgeAmpProcessor::releaseResources()
{
    if (oversampler != nullptr)
        oversampler->reset();
}

bool ForgeAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void ForgeAmpProcessor::updateParameters()
{
    auto v  = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto on = [&] (std::atomic<float>* a) { return v (a) >= 0.5f; };

    inGain.setTargetValue ((float) dbToGain (v (p.inputGain)));
    outGain.setTargetValue ((float) dbToGain (v (p.outputGain)));

    gate.setParams (on (p.gateOn), v (p.gateThreshold), v (p.gateRelease));

    boostMix.setTargetValue (on (p.boostOn) ? 1.0f : 0.0f);
    sBoostDrive.setTargetValue (v (p.boostDrive) * 0.1f);
    sBoostTone.setTargetValue (v (p.boostTone) * 0.1f);
    sBoostLevel.setTargetValue (v (p.boostLevel));

    ampMix.setTargetValue (on (p.ampOn) ? 1.0f : 0.0f);
    sGain.setTargetValue (v (p.ampGain) * 0.1f);
    sBass.setTargetValue (v (p.ampBass) * 0.1f);
    sMid.setTargetValue (v (p.ampMid) * 0.1f);
    sTreble.setTargetValue (v (p.ampTreble) * 0.1f);
    sPresence.setTargetValue (v (p.ampPresence) * 0.1f);
    sDepth.setTargetValue (v (p.ampDepth) * 0.1f);
    sMaster.setTargetValue (v (p.ampMaster) * 0.1f);

    pendingAmpModel = juce::jlimit (0, (int) ampVoices().size() - 1, (int) v (p.ampModel));
    if (pendingAmpModel != amp.getVoice())
        ampSwitch.request();

    // Until an IR is loaded, "User IR" falls back to the first built-in cab rather than
    // letting the raw amp (which is very fizzy without a cab) through.
    pendingCabModel = juce::jlimit (0, CabSim::kUserIR, (int) v (p.cabModel));
    if (pendingCabModel == CabSim::kUserIR && ! userIrLoaded.load())
        pendingCabModel = 0;
    if (pendingCabModel != cab.getModel())
        cabSwitch.request();

    cabMix.setTargetValue (on (p.cabOn) ? 1.0f : 0.0f);
    cab.setTone (v (p.cabMic) * 0.1f, v (p.cabLowCut), v (p.cabHighCut));

    const double delayMs = on (p.delaySync)
        ? delayDivisionBeats ((int) v (p.delayDivision)) * 60000.0 / bpm.load()
        : (double) v (p.delayTime);
    delay.setParams (on (p.delayOn), delayMs, v (p.delayFeedback) * 0.01f, v (p.delayTone),
                     v (p.delayMix) * 0.01f, on (p.delayPingPong));

    reverb.setParams (on (p.reverbOn), v (p.reverbSize) * 0.1f, v (p.reverbDecay),
                      v (p.reverbPreDelay), v (p.reverbTone), v (p.reverbMix) * 0.01f);
}

void ForgeAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (n <= 0 || oversampler == nullptr || buffer.getNumChannels() < 2)
        return;

    if (auto* host = getPlayHead())
        if (const auto pos = host->getPosition())
            if (const auto hostBpm = pos->getBpm())
                if (*hostBpm > 1.0)
                    bpm.store (*hostBpm);

    updateParameters();

    if (firstBlock)
    {
        // Start exactly where the parameters are instead of gliding in from defaults.
        for (auto* s : { &sGain, &sBass, &sMid, &sTreble, &sPresence, &sDepth, &sMaster,
                         &sBoostDrive, &sBoostTone, &sBoostLevel,
                         &inGain, &outGain, &boostMix, &ampMix, &cabMix })
            s->setCurrentAndTargetValue (s->getTargetValue());

        amp.setVoice (pendingAmpModel);
        cab.setModel (pendingCabModel);
        ampSwitch.reset();
        cabSwitch.reset();
        delay.reset();
        reverb.reset();
        firstBlock = false;
    }

    // Hosts may send more samples than promised in prepareToPlay; never overrun.
    for (int start = 0; start < n; start += maxBlock)
        processChunk (buffer, start, std::min (maxBlock, n - start), numIn >= 2);
}

void ForgeAmpProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, bool stereoIn)
{
    float* L = buffer.getWritePointer (0, start);
    float* R = buffer.getWritePointer (1, start);
    float* mono = monoBuffer.getWritePointer (0);

    // ── 1. Mono guitar signal, input gain, gate ───────────────────────────────
    for (int i = 0; i < n; ++i)
        mono[i] = (stereoIn ? 0.5f * (L[i] + R[i]) : L[i]) * inGain.getNextValue();

    inputMeter.push (mono, n);
    gate.process (mono, n);
    gateGainForUi.store (gate.getGain(), std::memory_order_relaxed);

    // ── 2. Boost + amp, 4x oversampled ────────────────────────────────────────
    float* chans[] = { mono };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    auto os = oversampler->processSamplesUp (block);
    float* x = os.getChannelPointer (0);
    const int osN = (int) os.getNumSamples();

    for (int i0 = 0; i0 < osN; i0 += kControlInterval)
    {
        const int m = std::min (kControlInterval, osN - i0);

        const AmpEngine::Controls c { sGain.getNextValue(), sBass.getNextValue(), sMid.getNextValue(),
                                      sTreble.getNextValue(), sPresence.getNextValue(),
                                      sDepth.getNextValue(), sMaster.getNextValue() };
        amp.setControls (c);

        const float bd = sBoostDrive.getNextValue(), bt = sBoostTone.getNextValue(), bl = sBoostLevel.getNextValue();
        if (bd != lastBoost[0] || bt != lastBoost[1] || bl != lastBoost[2])
        {
            boost.setControls (bd, bt, bl);
            lastBoost[0] = bd; lastBoost[1] = bt; lastBoost[2] = bl;
        }

        for (int i = i0; i < i0 + m; ++i)
        {
            double s = x[i];

            const float bm = boostMix.getNextValue();
            if (bm > 0.0f)
                s += bm * (boost.process (s) - s);

            bool swapNow = false;
            const float fade = ampSwitch.next (swapNow);
            if (swapNow)
                amp.setVoice (pendingAmpModel);

            const float am = ampMix.getNextValue();
            if (am > 0.0f)
                s += am * (amp.process (s) * fade - s);

            x[i] = (float) s;
        }
    }

    oversampler->processSamplesDown (block);

    // ── 3. Cabinet ────────────────────────────────────────────────────────────
    if (cabMix.getTargetValue() > 0.0f || cabMix.isSmoothing())
    {
        float* dry  = dryBuffer.getWritePointer (0);
        float* fade = dryBuffer.getWritePointer (1);
        std::copy (mono, mono + n, dry);

        // Split the block where a model swap happens so the old model fades out and the
        // new one fades in.
        for (int i = 0; i < n;)
        {
            int j = i;
            bool swap = false;
            while (j < n && ! swap)
                fade[j++] = cabSwitch.next (swap);

            cab.process (mono + i, j - i);
            if (swap)
                cab.setModel (pendingCabModel);
            i = j;
        }

        for (int i = 0; i < n; ++i)
        {
            const float cm = cabMix.getNextValue();
            mono[i] = dry[i] + cm * (mono[i] * fade[i] - dry[i]);
        }
    }
    else
    {
        cabMix.skip (n);
    }

    // ── 4. Stereo effects ─────────────────────────────────────────────────────
    std::copy (mono, mono + n, L);
    std::copy (mono, mono + n, R);
    delay.process (L, R, n);
    reverb.process (L, R, n);

    // ── 5. Output ─────────────────────────────────────────────────────────────
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }

    outputMeter.push (L, n);
    outputMeter.push (R, n);
}

//==============================================================================
juce::String ForgeAmpProcessor::loadUserImpulseResponse (const juce::File& file, bool selectUserIrModel)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return "Couldn't read \"" + file.getFileName() + "\". Use a WAV or AIFF impulse response.";

    if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        return "\"" + file.getFileName() + "\" is empty.";

    const int maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));
    juce::AudioBuffer<float> raw ((int) reader->numChannels, maxLen);
    reader->read (&raw, 0, maxLen, 0, true, true);

    // Cab IRs are mono; fold stereo IRs down.
    juce::AudioBuffer<float> ir (1, maxLen);
    ir.clear();
    for (int ch = 0; ch < raw.getNumChannels(); ++ch)
        ir.addFrom (0, 0, raw, ch, 0, maxLen, 1.0f / (float) raw.getNumChannels());

    const float peak = ir.getMagnitude (0, 0, maxLen);
    if (peak < 1.0e-6f)
        return "\"" + file.getFileName() + "\" is silent.";

    // Trim leading silence (some IR packs add pre-delay) and the inaudible tail.
    int first = 0;
    while (first < maxLen && std::abs (ir.getSample (0, first)) < peak * 0.01f)
        ++first;
    first = std::max (0, first - 8);
    int last = maxLen - 1;
    while (last > first && std::abs (ir.getSample (0, last)) < peak * 1.0e-4f)
        --last;

    const int len = last - first + 1;
    juce::AudioBuffer<float> trimmed (1, len);
    trimmed.copyFrom (0, 0, ir, 0, first, len);

    // Magnitude response via FFT, used for loudness matching and for the UI curve.
    int order = 12;
    while ((1 << order) < len && order < 18)
        ++order;
    const int fftSize = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> spec ((size_t) fftSize * 2, 0.0f);
    std::copy (trimmed.getReadPointer (0), trimmed.getReadPointer (0) + std::min (len, fftSize), spec.begin());
    fft.performFrequencyOnlyForwardTransform (spec.data());

    const double irRate = reader->sampleRate;
    auto magAt = [&] (double f)
    {
        const double bin = std::clamp (f / irRate * fftSize, 0.0, fftSize * 0.5 - 1.0);
        const int b = (int) bin;
        const double t = bin - b;
        return (double) spec[(size_t) b] * (1.0 - t) + (double) spec[(size_t) b + 1] * t;
    };

    // Match the built-in cabs: the 200 Hz - 4 kHz band averages 0 dB.
    double sumDb = 0.0;
    int count = 0;
    for (double f = 200.0; f <= 4000.0; f *= 1.05, ++count)
        sumDb += gainToDb (magAt (f));
    const double normDb = -sumDb / std::max (1, count);
    trimmed.applyGain ((float) dbToGain (normDb));

    std::vector<float> response ((size_t) kResponsePoints);
    for (int i = 0; i < kResponsePoints; ++i)
    {
        const double f = kResponseMinHz * std::pow (kResponseMaxHz / kResponseMinHz, (double) i / (kResponsePoints - 1));
        response[(size_t) i] = f < irRate * 0.5 ? (float) (gainToDb (magAt (f)) + normDb) : -60.0f;
    }

    {
        const juce::SpinLock::ScopedLockType lock (userIrLock);
        userIrResponse = std::move (response);
        userIrFile = file;
    }

    cab.loadImpulseResponse (std::move (trimmed), irRate);
    userIrLoaded = true;

    if (selectUserIrModel)
        if (auto* param = apvts.getParameter (ids::cabModel))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) CabSim::kUserIR));

    return {};
}

void ForgeAmpProcessor::clearUserImpulseResponse()
{
    userIrLoaded = false;
    {
        const juce::SpinLock::ScopedLockType lock (userIrLock);
        userIrResponse.clear();
        userIrFile = juce::File();
    }

    if (auto* param = apvts.getParameter (ids::cabModel))
        if ((int) param->convertFrom0to1 (param->getValue()) == CabSim::kUserIR)
            param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
}

juce::File ForgeAmpProcessor::getUserImpulseResponseFile() const
{
    const juce::SpinLock::ScopedLockType lock (userIrLock);
    return userIrFile;
}

std::vector<float> ForgeAmpProcessor::getUserIrResponse() const
{
    const juce::SpinLock::ScopedLockType lock (userIrLock);
    return userIrResponse;
}

//==============================================================================
void ForgeAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presets->writeState (state);
    state.setProperty ("userIR", userIrLoaded.load() ? getUserImpulseResponseFile().getFullPathName() : juce::String(), nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("uiPage", uiPage, nullptr);
    state.setProperty ("pluginVersion", info::version, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ForgeAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);

    uiScale = juce::jlimit (1.0f, 4.0f, (float) tree.getProperty ("uiScale", 2.0f));
    uiPage  = juce::jlimit (0, 3, (int) tree.getProperty ("uiPage", 1));

    const auto irPath = tree.getProperty ("userIR").toString();
    if (irPath.isNotEmpty() && juce::File (irPath).existsAsFile())
        loadUserImpulseResponse (juce::File (irPath), false);

    // Hosts may restore state from any thread; the preset manager belongs to the UI thread.
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        presets->readState (tree);
    }
    else
    {
        juce::WeakReference<ForgeAmpProcessor> weakThis (this);
        juce::MessageManager::callAsync ([weakThis, tree]
        {
            if (weakThis != nullptr)
                weakThis->presets->readState (tree);
        });
    }
}

//==============================================================================
juce::AudioProcessorEditor* ForgeAmpProcessor::createEditor()
{
    return new ForgeAmpEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ForgeAmpProcessor();
}
