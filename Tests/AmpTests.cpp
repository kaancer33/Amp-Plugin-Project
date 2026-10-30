// Offline test & render harness for Forge Amp.
//
//   ForgeAmpTests [outputDir] [--ui-only] [--vst3 path/to/Forge Amp.vst3]
//
// Runs the real processor without a host: DSP sanity checks, tone-stack accuracy, gain
// structure, latency / bypass transparency, odd block sizes, state round trip, sample rates,
// user IR loading, CPU benchmark. Renders a synthetic guitar DI through every factory preset
// to WAV, and snapshots every editor page to PNG.

#include <JuceHeader.h>
#include <iostream>
#include <iomanip>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace forge;

namespace
{
juce::File outDir;
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << std::endl;
    if (! ok) ++failures;
}

void section (const char* name) { std::cout << "\n== " << name << " ==" << std::endl; }

double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

//==============================================================================
// Synthetic guitar DI: Karplus-Strong strings through a humbucker-ish pickup response.
struct Note { double time; std::vector<int> midi; double duration; bool palmMute; float velocity; };

juce::AudioBuffer<float> makeGuitarDI (double sr)
{
    const double beat = 0.5;  // 120 BPM
    std::vector<Note> notes;
    auto chug = [&] (double t, std::vector<int> m) { notes.push_back ({ t, m, beat * 0.45, true, 0.9f }); };

    for (int bar = 0; bar < 2; ++bar)
    {
        const double t0 = bar * 4 * beat;
        for (int i = 0; i < 4; ++i) chug (t0 + i * beat * 0.5, { 40, 47, 52 });
        notes.push_back ({ t0 + 2 * beat, bar == 0 ? std::vector<int> { 43, 50, 55 } : std::vector<int> { 50, 57, 62 }, beat * 0.95, false, 1.0f });
        notes.push_back ({ t0 + 3 * beat, bar == 0 ? std::vector<int> { 45, 52, 57 } : std::vector<int> { 48, 55, 60 }, beat * 0.95, false, 1.0f });
    }

    notes.push_back ({ 8 * beat, { 40, 47, 52, 56, 59, 64 }, beat * 2.0, false, 0.85f });   // open E chord
    const int lead[] { 64, 67, 69, 71 };
    for (int i = 0; i < 4; ++i)
        notes.push_back ({ 10 * beat + i * beat * 0.5, { lead[i] }, i == 3 ? beat * 3.0 : beat * 0.48, false, 1.0f });

    const int total = (int) (sr * (16 * beat + 1.5));
    juce::AudioBuffer<float> out (1, total);
    out.clear();
    juce::Random rng (1234);

    for (const auto& n : notes)
    {
        for (size_t s = 0; s < n.midi.size(); ++s)
        {
            const double f = 440.0 * std::pow (2.0, (n.midi[s] - 69) / 12.0);
            const int period = std::max (2, (int) std::round (sr / f));
            std::vector<float> line ((size_t) period);
            float lp = 0.0f;
            for (auto& v : line) { lp += 0.55f * ((rng.nextFloat() * 2.0f - 1.0f) - lp); v = lp * n.velocity; }

            const int start = (int) ((n.time + 0.012 * (double) s) * sr);   // strum
            const int len   = (int) (n.duration * sr);
            const float rho = n.palmMute ? 0.985f : 0.9985f;
            const int fadeLen = (int) (0.015 * sr);

            for (int i = 0; i < len + fadeLen && start + i < total; ++i)
            {
                const size_t idx = (size_t) (i % period);
                const float y = line[idx];
                line[idx] = rho * 0.5f * (y + line[(idx + 1) % (size_t) period]);
                const float env = i < len ? 1.0f : 1.0f - (float) (i - len) / (float) fadeLen;
                out.addSample (0, start + i, y * env);
            }
        }
    }

    // Pickup: resonant peak around 3.2 kHz, then roll-off; remove DC.
    Biquad res, lp2, hp;
    res.set (Biquad::Type::peak, sr, 3200.0, 2.0, 5.0);
    lp2.set (Biquad::Type::lowPass, sr, 5500.0, 0.7071);
    hp.set (Biquad::Type::highPass, sr, 40.0, 0.7071);
    auto* d = out.getWritePointer (0);
    for (int i = 0; i < total; ++i)
        d[i] = (float) hp.process (lp2.process (res.process (d[i])));

    out.applyGain (0.35f / out.getMagnitude (0, 0, total));   // peaks at about -9 dBFS, a typical DI level
    return out;
}

//==============================================================================
struct Stats { double peakDb, rmsDb; bool finite; };

Stats measure (const juce::AudioBuffer<float>& b, int from = 0)
{
    double sum = 0.0, peak = 0.0;
    bool finite = true;
    long count = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (ch, i);
            if (! std::isfinite (v)) { finite = false; continue; }
            sum += (double) v * v;
            peak = std::max (peak, (double) std::abs (v));
            ++count;
        }
    return { toDb (peak), toDb (std::sqrt (sum / (double) std::max (1L, count))), finite };
}

void setParam (ForgeAmpProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float getParam (ForgeAmpProcessor& p, const char* id)
{
    auto* param = p.apvts.getParameter (id);
    return param->convertFrom0to1 (param->getValue());
}

void allModulesOff (ForgeAmpProcessor& p)
{
    for (auto* id : { ids::gateOn, ids::boostOn, ids::ampOn, ids::cabOn, ids::delayOn, ids::reverbOn })
        setParam (p, id, 0.0f);
}

/** Runs mono input (copied to both channels) through the processor in blocks. */
juce::AudioBuffer<float> render (ForgeAmpProcessor& p, const juce::AudioBuffer<float>& in, double sr,
                                 int blockSize, bool randomBlocks = false, bool prepare = true)
{
    if (prepare)
    {
        p.setRateAndBufferSizeDetails (sr, blockSize);
        p.prepareToPlay (sr, blockSize);
    }

    juce::AudioBuffer<float> out (2, in.getNumSamples());
    out.copyFrom (0, 0, in, 0, 0, in.getNumSamples());
    out.copyFrom (1, 0, in, 0, 0, in.getNumSamples());

    juce::MidiBuffer midi;
    juce::Random rng (99);
    for (int pos = 0; pos < in.getNumSamples();)
    {
        const int n = std::min (randomBlocks ? 1 + rng.nextInt (2048) : blockSize, in.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (view, midi);
        pos += n;
    }
    return out;
}

void writeWav (const juce::AudioBuffer<float>& b, double sr, const juce::String& name)
{
    auto file = outDir.getChildFile (name + ".wav");
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr) { check (false, "open " + file.getFullPathName()); return; }

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                   .withSampleRate (sr)
                                                   .withNumChannels (b.getNumChannels())
                                                   .withBitsPerSample (24));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::AudioBuffer<float> sine (double sr, double f, double seconds, double ampDb)
{
    juce::AudioBuffer<float> b (1, (int) (sr * seconds));
    const double a = std::pow (10.0, ampDb / 20.0);
    for (int i = 0; i < b.getNumSamples(); ++i)
        b.setSample (0, i, (float) (a * std::sin (kTwoPi * f * i / sr)));
    return b;
}

double goertzel (const float* x, int n, double sr, double f)
{
    const double w = kTwoPi * f / sr, cw = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        // Hann window
        const double win = 0.5 - 0.5 * std::cos (kTwoPi * i / (n - 1));
        const double s0 = x[i] * win + cw * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - cw * s1 * s2));
}

//==============================================================================
void testToneStack()
{
    section ("Tone stack (Yeh/Smith model)");

    // Digital (bilinear, 4x rate) vs exact analog response.
    const double fs = 192000.0;
    for (int v = 0; v < (int) ampVoices().size(); ++v)
    {
        double worst = 0.0;
        for (double f : { 100.0, 400.0, 1000.0, 3000.0 })
        {
            ToneStack ts;
            ts.setComponents (ampVoices()[(size_t) v].toneStack);
            ts.prepare (fs);
            ts.setControls (0.3, 0.7, 0.6);
            const int n = (int) fs;
            std::vector<float> y ((size_t) n);
            for (int i = 0; i < n; ++i)
                y[(size_t) i] = (float) ts.process (std::sin (kTwoPi * f * i / fs));
            const int tail = n / 2;
            const double digital = goertzel (y.data() + tail, n - tail, fs, f) / ((n - tail) * 0.25);
            const double analog = ts.displayMagnitude (0.3, 0.7, 0.6, f);
            worst = std::max (worst, std::abs (toDb (digital) - toDb (analog)));
        }
        check (worst < 0.5, juce::String (ampVoices()[(size_t) v].name) + ": digital matches analog within "
                            + juce::String (worst, 3) + " dB");
    }

    std::cout << "\n  knob sweep (dB at 100 Hz / 500 Hz / 3 kHz, others at noon):" << std::endl;
    const double freqs[] { 100.0, 500.0, 3000.0 };
    for (int v = 0; v < (int) ampVoices().size(); ++v)
    {
        std::cout << "  " << std::setw (7) << ampVoices()[(size_t) v].name << ":";
        for (int knob = 0; knob < 3; ++knob)
        {
            std::cout << "  " << (knob == 0 ? "bass" : knob == 1 ? "mid" : "treb");
            for (float pos : { 0.0f, 0.5f, 1.0f })
            {
                AmpEngine::Controls c;
                (knob == 0 ? c.bass : knob == 1 ? c.mid : c.treble) = pos;
                double db[3];
                AmpEngine::displayResponseDb (v, c, freqs, db, 3);
                std::cout << " " << std::fixed << std::setprecision (1) << std::setw (5) << db[knob];
            }
        }
        std::cout << std::endl;
    }
}

void testGainStructure()
{
    section ("Gain structure (220 Hz sine at -12 dBFS, cab/fx off)");
    const double sr = 48000.0;
    const auto in = sine (sr, 220.0, 1.0, -12.0);

    std::cout << "  voice    gain   out RMS   THD" << std::endl;
    for (int v = 0; v < (int) ampVoices().size(); ++v)
    {
        for (float g : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
        {
            ForgeAmpProcessor p;
            allModulesOff (p);
            setParam (p, ids::ampOn, 1.0f);
            setParam (p, ids::ampModel, (float) v);
            setParam (p, ids::ampGain, g);
            auto out = render (p, in, sr, 512);

            const int from = (int) (0.5 * sr), n = out.getNumSamples() - from;
            const float* x = out.getReadPointer (0) + from;
            const double fund = goertzel (x, n, sr, 220.0);
            double harm = 0.0;
            for (int h = 2; h <= 12; ++h)
                harm += std::pow (goertzel (x, n, sr, 220.0 * h), 2.0);
            const double thd = 100.0 * std::sqrt (harm) / std::max (fund, 1e-12);
            const auto st = measure (out, from);

            std::cout << "  " << std::setw (7) << ampVoices()[(size_t) v].name << std::setw (6) << std::fixed
                      << std::setprecision (1) << g << std::setw (9) << st.rmsDb << " dB"
                      << std::setw (7) << std::setprecision (1) << thd << "%" << std::endl;
        }
    }
}

void testCalibration (const juce::AudioBuffer<float>& di)
{
    section ("Level calibration (DI through each voice + default cab, reverb off)");
    std::cout << "  voice    gain    RMS     peak" << std::endl;
    for (int v = 0; v < (int) ampVoices().size(); ++v)
        for (float g : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
        {
            ForgeAmpProcessor p;
            setParam (p, ids::reverbOn, 0.0f);
            setParam (p, ids::ampModel, (float) v);
            setParam (p, ids::ampGain, g);
            const auto st = measure (render (p, di, 48000.0, 512));
            std::cout << "  " << std::setw (7) << ampVoices()[(size_t) v].name << std::setw (6) << std::fixed
                      << std::setprecision (1) << g << std::setw (8) << st.rmsDb << std::setw (8) << st.peakDb << std::endl;
        }
}

void testPresets (const juce::AudioBuffer<float>& di)
{
    section ("Factory presets (synthetic DI, 48 kHz)");
    const double sr = 48000.0;
    writeWav (di, sr, "00_DI_input");

    int index = 1;
    for (const auto& preset : factoryPresets())
    {
        ForgeAmpProcessor p;
        auto& pm = p.getPresetManager();
        for (int i = 0; i < (int) pm.getEntries().size(); ++i)
            if (pm.getEntries()[(size_t) i].name == preset.name) pm.load (i);

        auto out = render (p, di, sr, 256);
        const auto st = measure (out);
        const auto name = juce::String (index).paddedLeft ('0', 2) + "_" + juce::String (preset.name).replaceCharacter (' ', '_');
        writeWav (out, sr, name);

        check (st.finite && st.peakDb < 0.0 && st.rmsDb > -26.0 && st.rmsDb < -10.0,
               juce::String (preset.name).paddedRight (' ', 16) + " peak " + juce::String (st.peakDb, 1)
               + " dBFS, RMS " + juce::String (st.rmsDb, 1) + " dBFS");
        ++index;
    }
}

/** Energy at non-harmonic frequencies (folded-back aliases) relative to the harmonics, for a
    high sine through a saturated voice with the cab off (the cab would hide aliasing). */
void testAliasing()
{
    section ("Aliasing (3001 Hz sine, -12 dBFS, cab and effects off)");
    const double sr = 48000.0, f0 = 3001.0;
    constexpr int order = 15, n = 1 << order;

    for (auto [voice, gainKnob] : { std::pair<int, float> { 2, 10.0f }, { 1, 7.5f }, { 0, 10.0f } })
    {
        ForgeAmpProcessor p;
        allModulesOff (p);
        setParam (p, ids::ampOn, 1.0f);
        setParam (p, ids::ampModel, (float) voice);
        setParam (p, ids::ampGain, gainKnob);
        const auto out = render (p, sine (sr, f0, 1.5, -12.0), sr, 512);

        std::vector<float> fftData ((size_t) n * 2, 0.0f);
        const int start = out.getNumSamples() - n;
        for (int i = 0; i < n; ++i)
            fftData[(size_t) i] = out.getSample (0, start + i) * (float) (0.5 - 0.5 * std::cos (kTwoPi * i / (n - 1)));
        juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (fftData.data());

        double harmonic = 0.0, other = 0.0;
        for (int b = (int) (40.0 * n / sr); b < n / 2; ++b)
        {
            const double f = b * sr / n, power = (double) fftData[(size_t) b] * fftData[(size_t) b];
            const double k = std::round (f / f0);
            const bool isHarmonic = k >= 1.0 && std::abs (f - k * f0) <= 4.0 * sr / n;
            (isHarmonic ? harmonic : other) += power;
        }
        const double asr = 10.0 * std::log10 (std::max (other, 1e-30) / std::max (harmonic, 1e-30));
        check (asr < -40.0, juce::String (ampVoices()[(size_t) voice].name) + " at gain " + juce::String (gainKnob, 1)
                             + ": aliasing " + juce::String (asr, 1) + " dB below the harmonics");
    }
}

void testLatencyAndBypass()
{
    section ("Latency and bypass transparency");
    const double sr = 48000.0;
    ForgeAmpProcessor p;
    allModulesOff (p);

    juce::AudioBuffer<float> imp (1, 8192);
    imp.clear();
    imp.setSample (0, 1000, 1.0f);
    auto out = render (p, imp, sr, 512);
    const int latency = p.getLatencySamples();

    int peakIdx = 0;
    for (int i = 0; i < out.getNumSamples(); ++i)
        if (std::abs (out.getSample (0, i)) > std::abs (out.getSample (0, peakIdx))) peakIdx = i;

    double energy = 0.0;
    for (int i = 0; i < out.getNumSamples(); ++i) energy += std::pow (out.getSample (0, i), 2.0);

    std::cout << "  reported latency " << latency << " samples, impulse peak lands at +" << (peakIdx - 1000) << std::endl;
    check (std::abs ((peakIdx - 1000) - latency) <= 1, "reported latency matches the measured delay");
    check (std::abs (toDb (std::sqrt (energy))) < 0.5, "all-bypassed energy within 0.5 dB of input ("
                                                       + juce::String (toDb (std::sqrt (energy)), 2) + " dB)");

    for (double f : { 100.0, 1000.0, 8000.0 })
    {
        ForgeAmpProcessor q;
        allModulesOff (q);
        auto s = render (q, sine (sr, f, 0.5, -6.0), sr, 512);
        const auto st = measure (s, (int) (0.2 * sr));
        check (std::abs (st.rmsDb - (-6.0 - 3.01)) < 0.3, "bypassed " + juce::String ((int) f) + " Hz sine level "
                                                           + juce::String (st.rmsDb + 3.01, 2) + " dB (expect -6)");
    }
}

void testBlockSizes (const juce::AudioBuffer<float>& di)
{
    section ("Block sizes");
    const double sr = 48000.0;
    ForgeAmpProcessor a, b;
    for (auto* p : { &a, &b })
    {
        setParam (*p, ids::delayOn, 1.0f);
        setParam (*p, ids::boostOn, 1.0f);
    }
    auto ref = render (a, di, sr, 512);
    auto odd = render (b, di, sr, 512, true);   // random 1..2048, larger than prepared

    double maxDiff = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < ref.getNumSamples(); ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (ref.getSample (ch, i) - odd.getSample (ch, i)));

    check (measure (odd).finite, "random block sizes (1..2048, prepared for 512) produce finite output");
    check (maxDiff < 1.0e-3, "output independent of block size (max diff " + juce::String (maxDiff, 6) + ")");
}

void testStateRoundTrip()
{
    section ("State round trip");
    {
        ForgeAmpProcessor fresh;
        auto& pm = fresh.getPresetManager();
        check (pm.getCurrentIndex() == -1 && pm.getCurrentName() == "Default" && ! pm.isDirty(),
               "a new instance shows the init patch, not a factory preset name");
        pm.loadPrevious();
        check (pm.getCurrentIndex() == (int) pm.getEntries().size() - 1, "previous from the init patch wraps to the last preset");
    }

    ForgeAmpProcessor a;
    a.getPresetManager().load (4);
    setParam (a, ids::ampGain, 7.3f);
    setParam (a, ids::cabModel, 2.0f);
    setParam (a, ids::delayDivision, 6.0f);
    setParam (a, ids::reverbDecay, 4.2f);
    a.uiScale = 1.25f;
    a.uiPage = 2;

    juce::MemoryBlock state;
    a.getStateInformation (state);

    ForgeAmpProcessor b;
    b.setStateInformation (state.getData(), (int) state.getSize());

    bool same = true;
    for (auto& id : allParameterIds())
        if (std::abs (getParam (a, id.toRawUTF8()) - getParam (b, id.toRawUTF8())) > 1.0e-4f)
        {
            std::cout << "    mismatch: " << id << std::endl;
            same = false;
        }

    check (same, "all " + juce::String (allParameterIds().size()) + " parameters restored");
    check (b.getPresetManager().getCurrentName() == a.getPresetManager().getCurrentName()
               && b.getPresetManager().isDirty(),
           "preset name '" + b.getPresetManager().getCurrentName() + "' and modified flag restored");
    check (std::abs (b.uiScale - 1.25f) < 1e-6f && b.uiPage == 2, "UI scale and page restored");

    setParam (b, ids::inputGain, 7.5f);
    b.getPresetManager().load (0);
    b.getPresetManager().loadNext();
    check (std::abs (getParam (b, ids::inputGain) - 7.5f) < 0.01f, "loading presets keeps the input-gain calibration");
}

void testSampleRates (const juce::AudioBuffer<float>& di48)
{
    section ("Sample rates");
    double ref = 0.0;
    for (double sr : { 48000.0, 44100.0, 96000.0, 192000.0 })
    {
        // resample the DI by linear interpolation - fine for a level comparison
        const int n = (int) (di48.getNumSamples() * sr / 48000.0);
        juce::AudioBuffer<float> di (1, n);
        for (int i = 0; i < n; ++i)
        {
            const double pos = i * 48000.0 / sr;
            const int i0 = std::min ((int) pos, di48.getNumSamples() - 2);
            const double t = pos - i0;
            di.setSample (0, i, (float) (di48.getSample (0, i0) * (1.0 - t) + di48.getSample (0, i0 + 1) * t));
        }

        ForgeAmpProcessor p;
        p.getPresetManager().load (6);
        const auto st = measure (render (p, di, sr, 512));
        if (sr == 48000.0) ref = st.rmsDb;
        check (st.finite && std::abs (st.rmsDb - ref) < 2.0,
               juce::String ((int) sr) + " Hz: RMS " + juce::String (st.rmsDb, 1) + " dBFS, latency "
               + juce::String (p.getLatencySamples()) + " samples");
    }
}

void testUserIR (const juce::AudioBuffer<float>& di)
{
    section ("User impulse response");
    const double sr = 48000.0;

    // Make an IR file by rendering the built-in 4x12 cab's impulse response.
    CabSim cab;
    cab.prepare (sr, 4096);
    cab.setModel (0);
    cab.setTone (0.5f, 20.0f, 20000.0f);
    juce::AudioBuffer<float> ir (1, 4096);
    ir.clear();
    ir.setSample (0, 0, 1.0f);
    cab.process (ir.getWritePointer (0), 4096);
    writeWav (ir, sr, "test_ir_4x12");

    ForgeAmpProcessor builtIn, user;
    for (auto* p : { &builtIn, &user })
        setParam (*p, ids::reverbOn, 0.0f);
    setParam (builtIn, ids::cabMic, 5.0f);
    setParam (user, ids::cabMic, 5.0f);

    const auto err = user.loadUserImpulseResponse (outDir.getChildFile ("test_ir_4x12.wav"));
    check (err.isEmpty(), "IR loads" + (err.isEmpty() ? juce::String() : ": " + err));
    check ((int) getParam (user, ids::cabModel) == CabSim::kUserIR, "cab switches to User IR");
    check (user.getUserIrResponse().size() == (size_t) ForgeAmpProcessor::kResponsePoints, "IR response curve computed");

    user.setRateAndBufferSizeDetails (sr, 512);
    user.prepareToPlay (sr, 512);
    juce::Thread::sleep (600);   // the convolution engine is built on a background thread
    const auto a = measure (render (builtIn, di, sr, 512));
    const auto b = measure (render (user, di, sr, 512, false, false));
    check (b.finite && std::abs (a.rmsDb - b.rmsDb) < 3.0,
           "IR of a built-in cab sounds as loud as the cab itself (" + juce::String (a.rmsDb, 1) + " vs "
           + juce::String (b.rmsDb, 1) + " dBFS)");
}

void testCpu (const juce::AudioBuffer<float>& di)
{
    section ("CPU");
    const double sr = 48000.0;
    ForgeAmpProcessor p;
    for (auto* id : { ids::boostOn, ids::delayOn, ids::reverbOn })
        setParam (p, id, 1.0f);
    setParam (p, ids::ampModel, 2.0f);

    juce::AudioBuffer<float> longDi (1, di.getNumSamples() * 3);
    for (int k = 0; k < 3; ++k)
        longDi.copyFrom (0, k * di.getNumSamples(), di, 0, 0, di.getNumSamples());

    const auto t0 = juce::Time::getHighResolutionTicks();
    render (p, longDi, sr, 128);
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double audioSecs = longDi.getNumSamples() / sr;
    std::cout << "  " << std::fixed << std::setprecision (2) << audioSecs << " s of audio in " << secs
              << " s -> " << std::setprecision (1) << (100.0 * secs / audioSecs) << "% of one core (block 128)" << std::endl;
   #if JUCE_DEBUG
    std::cout << "  (debug build - release is several times faster)" << std::endl;
   #else
    check (secs / audioSecs < 0.15, "uses under 15% of one core with everything on");
   #endif
}

void testHostedVst3 (const juce::File& pluginFile, const juce::AudioBuffer<float>& di)
{
    section ("Hosted VST3 (the built plugin binary, loaded like a DAW would)");
    if (! pluginFile.exists())
    {
        std::cout << "  (skipped: " << pluginFile.getFullPathName() << " not built yet)" << std::endl;
        return;
    }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, pluginFile.getFullPathName());
    check (found.size() == 1, "scanned: " + (found.isEmpty() ? juce::String ("nothing found")
                                                              : found[0]->name + " by " + found[0]->manufacturerName
                                                                + ", " + found[0]->category));
    if (found.isEmpty())
        return;

    juce::String error;
    auto plugin = format.createInstanceFromDescription (*found[0], 48000.0, 512, error);
    check (plugin != nullptr, "instantiated" + (error.isEmpty() ? juce::String() : ": " + error));
    if (plugin == nullptr)
        return;

    plugin->prepareToPlay (48000.0, 512);
    check (plugin->getLatencySamples() == 6, "reports " + juce::String (plugin->getLatencySamples()) + " samples latency to the host");
    check (plugin->getParameters().size() >= (int) allParameterIds().size(),
           juce::String (plugin->getParameters().size()) + " automatable parameters visible to the host");

    juce::AudioBuffer<float> buf (2, di.getNumSamples());
    buf.copyFrom (0, 0, di, 0, 0, di.getNumSamples());
    buf.copyFrom (1, 0, di, 0, 0, di.getNumSamples());
    juce::MidiBuffer midi;
    for (int pos = 0; pos < buf.getNumSamples(); pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, std::min (512, buf.getNumSamples() - pos));
        plugin->processBlock (view, midi);
    }
    const auto st = measure (buf);
    check (st.finite && st.rmsDb > -30.0 && st.peakDb < 0.0,
           "processes audio (RMS " + juce::String (st.rmsDb, 1) + " dBFS, peak " + juce::String (st.peakDb, 1) + " dBFS)");

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    plugin->setStateInformation (state.getData(), (int) state.getSize());
    check (state.getSize() > 200, "state saved and restored through the VST3 interface (" + juce::String ((int) state.getSize()) + " bytes)");

    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (plugin->createEditorAndMakeActive());
        check (ed != nullptr && ed->getWidth() > 0, "editor opens in the host"
                                                    + (ed != nullptr ? " (" + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()) + ")" : juce::String()));
    }
    plugin->releaseResources();
}

void snapshotEditor()
{
    section ("Editor snapshots");
    ForgeAmpProcessor p;
    p.getPresetManager().load (6);
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto* editor = dynamic_cast<ForgeAmpEditor*> (ed.get());
    check (editor != nullptr, "editor created");
    if (editor == nullptr) return;
    check (editor->getWidth() == 1024 && editor->getHeight() == 640,
           "editor opens at the default 2x zoom (" + juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) + ")");

    // Feed audio in real-time-sized blocks while the message loop runs, so the meters,
    // gate scope and timer-driven refreshes are live in the snapshots.
    const auto di = makeGuitarDI (48000.0);
    int pos = 0;
    juce::MidiBuffer midi;
    auto play = [&] (int ticks)
    {
        for (int k = 0; k < ticks; ++k)
        {
            juce::AudioBuffer<float> buf (2, 1600);
            for (int i = 0; i < 1600; ++i, ++pos)
                buf.setSample (0, i, di.getSample (0, pos % di.getNumSamples()));
            buf.copyFrom (1, 0, buf, 0, 0, 1600);
            p.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
    };

    // At a whole-number zoom every pixel must be pure black or white, and every zoom x zoom
    // block one colour: the pixel art reaches the window without blur or seams.
    // The window's resize grip (bottom-right corner, outside the canvas) is skipped.
    auto isCrispPixelArt = [] (const juce::Image& img, int zoom)
    {
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        int bad = 0;
        juce::Rectangle<int> where;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                if (x >= img.getWidth() - 20 && y >= img.getHeight() - 20)
                    continue;
                const auto c = data.getPixelColour (x, y).getARGB() & 0xffffffu;
                if ((c != 0 && c != 0xffffffu) || c != (data.getPixelColour (x - x % zoom, y - y % zoom).getARGB() & 0xffffffu))
                {
                    if (bad == 0)
                        std::cout << "         first stray pixel at " << x << "," << y << " = #"
                                  << juce::String::toHexString ((int) c) << std::endl;
                    where = bad++ == 0 ? juce::Rectangle<int> (x, y, 1, 1) : where.getUnion ({ x, y, 1, 1 });
                }
            }
        if (bad > 0)
            std::cout << "         " << bad << " stray pixels in " << where.toString() << std::endl;
        return bad == 0;
    };

    const char* pages[] { "pre", "amp", "cab", "fx" };
    for (int page = 0; page < 4; ++page)
    {
        if (page == 3)
            p.getPresetManager().load (8);   // Soaring Lead: ping-pong delay and a long reverb
        editor->showPage (page);
        play (page == 0 ? 120 : 30);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile (juce::String ("ui_") + juce::String (page) + "_" + pages[page] + ".png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        check (os.openedOk() && png.writeImageToStream (img, os),
               "snapshot " + file.getFileName() + " (" + juce::String (img.getWidth()) + "x" + juce::String (img.getHeight()) + ")");
        if (page == 1)
            check (isCrispPixelArt (img, 2), "UI renders as crisp 1-bit pixel art at 2x (pure black/white, exact 2x2 pixels)");
    }

    // Edited-preset badge
    setParam (p, ids::ampGain, 6.8f);
    editor->showPage (1);
    play (10);
    {
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile ("ui_edited.png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    check (p.getPresetManager().isDirty(), "editing a knob marks the preset as edited");

    // Other zooms: 1x and 3x must stay exact; 1.5x goes through the sharp-scaling path.
    for (auto [w, h, name] : { std::tuple (512, 320, "ui_zoom_1x.png"), std::tuple (768, 480, "ui_zoom_1_5x.png"),
                               std::tuple (1536, 960, "ui_zoom_3x.png") })
    {
        editor->setSize (w, h);
        editor->showPage (1);
        play (2);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        if (w != 768)
            check (isCrispPixelArt (img, w / 512), juce::String ("crisp pixel art at ") + juce::String (w / 512) + "x zoom");
    }
    check (std::abs (p.uiScale - 3.0f) < 1e-4f, "zoom is remembered for the session (" + juce::String (p.uiScale, 2) + "x)");

    // Popup menu and dialog: both live inside the pixel canvas, so they must stay crisp too
    editor->setSize (1024, 640);
    editor->showPage (1);
    play (2);

    std::function<juce::Button* (juce::Component&, const juce::String&)> findButton;
    findButton = [&findButton] (juce::Component& c, const juce::String& tip) -> juce::Button*
    {
        if (auto* b = dynamic_cast<juce::Button*> (&c))
            if (b->getTooltip() == tip || b->getButtonText() == tip)
                return b;
        for (auto* child : c.getChildren())
            if (auto* found = findButton (*child, tip))
                return found;
        return nullptr;
    };

    auto snapshotWith = [&] (const juce::String& tip, const char* fileName, const juce::String& what)
    {
        auto* button = findButton (*editor, tip);
        if (button == nullptr)
        {
            check (false, "found the '" + tip + "' control");
            return;
        }
        // Call the handler directly and capture at once: a console app is never the foreground
        // process, so JUCE would close a popup menu on its first timer tick.
        if (button->onClick)
            button->onClick();
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = outDir.getChildFile (fileName);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        check (isCrispPixelArt (img, 2), what + " renders as crisp pixel art");
    };

    snapshotWith ("Browse presets", "ui_menu.png", "preset menu");
    {
        // JUCE names its menu windows "menu"; it must be a visible child inside the editor
        std::function<bool (juce::Component&)> hasMenu = [&hasMenu] (juce::Component& c)
        {
            if (c.getName() == "menu" && c.isVisible())
                return true;
            for (auto* child : c.getChildren())
                if (hasMenu (*child))
                    return true;
            return false;
        };
        check (hasMenu (*editor), "preset menu opens inside the pixel canvas, not as a separate window");
    }
    juce::PopupMenu::dismissAllActiveMenus();
    play (2);
    snapshotWith ("Save the current sound as a user preset", "ui_dialog.png", "save dialog");
    if (auto* cancel = findButton (*editor, "CANCEL"))
        cancel->triggerClick();
    play (2);
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    bool uiOnly = false;
    outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    auto vst3 = juce::File::getCurrentWorkingDirectory().getChildFile ("build/ForgeAmp_artefacts/Release/VST3/Forge Amp.vst3");
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--ui-only") uiOnly = true;
        else if (arg == "--vst3" && i + 1 < argc) vst3 = juce::File::getCurrentWorkingDirectory().getChildFile (argv[++i]);
        else outDir = juce::File::getCurrentWorkingDirectory().getChildFile (arg);
    }
    outDir.createDirectory();

    std::cout << forge::info::productName << " " << forge::info::version << " test harness -> "
              << outDir.getFullPathName() << std::endl;

    if (! uiOnly)
    {
        const auto di = makeGuitarDI (48000.0);
        testToneStack();
        testGainStructure();
        testAliasing();
        testLatencyAndBypass();
        testBlockSizes (di);
        testStateRoundTrip();
        testSampleRates (di);
        testUserIR (di);
        testCalibration (di);
        testPresets (di);
        testCpu (di);
        testHostedVst3 (vst3, di);
    }

    snapshotEditor();

    std::cout << "\n" << (failures == 0 ? "ALL CHECKS PASSED" : juce::String (failures) + " CHECK(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
