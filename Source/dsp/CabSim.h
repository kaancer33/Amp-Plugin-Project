#pragma once
#include "DspCommon.h"

namespace forge
{
struct CabFilterSpec
{
    Biquad::Type type;
    float freq, q, gainDb;
};

/** A speaker cabinet + close mic described as a filter cascade plus two short reflections
    (the back wall / baffle bounce that gives real cab IRs their comb-like ripple). */
struct CabModel
{
    const char* name;
    const char* description;
    std::array<CabFilterSpec, 9> filters;
    int numFilters;
    float reflectionMs[2];
    float reflectionGain[2];
};

inline const std::array<CabModel, 4>& cabModels()
{
    using T = Biquad::Type;
    static const std::array<CabModel, 4> models {{
        {
            "4x12 Modern", "Closed back, V30-style speakers, close dynamic mic. Tight lows, upper-mid bite.",
            {{ { T::highPass,   72.0f, 0.85f,  0.0f },
               { T::peak,      105.0f, 1.40f,  4.5f },   // closed-box resonance
               { T::peak,      380.0f, 1.00f, -3.5f },   // box
               { T::peak,     1500.0f, 1.60f, -2.0f },
               { T::peak,     2600.0f, 1.50f,  5.0f },   // V30 presence peak
               { T::peak,     4200.0f, 3.00f,  3.0f },   // cone breakup
               { T::peak,     5400.0f, 3.50f, -7.0f },   // breakup notch
               { T::lowPass,  6200.0f, 0.70f,  0.0f },
               { T::lowPass,  7600.0f, 1.10f,  0.0f } }},
            9, { 0.55f, 1.30f }, { 0.18f, 0.10f }
        },
        {
            "4x12 Vintage", "Closed back, Greenback-style speakers. Warm, mid-forward, early roll-off.",
            {{ { T::highPass,   78.0f, 0.90f,  0.0f },
               { T::peak,      110.0f, 1.30f,  3.5f },
               { T::peak,      800.0f, 0.80f,  2.0f },   // greenback mids
               { T::peak,     2000.0f, 1.30f,  3.0f },
               { T::peak,     3500.0f, 2.50f, -4.0f },
               { T::lowPass,  4800.0f, 0.70f,  0.0f },
               { T::lowPass,  6000.0f, 0.90f,  0.0f },
               {}, {} }},
            7, { 0.60f, 1.40f }, { 0.16f, 0.09f }
        },
        {
            "2x12 Open", "Open back, alnico-style speakers. Looser lows, chime and air for cleans.",
            {{ { T::highPass,   95.0f, 0.60f,  0.0f },   // open back: bass cancels
               { T::peak,      220.0f, 1.00f,  1.5f },
               { T::peak,      500.0f, 1.00f, -2.5f },
               { T::peak,     2800.0f, 1.20f,  4.0f },   // chime
               { T::peak,     4800.0f, 3.00f, -3.0f },
               { T::lowPass,  7000.0f, 0.70f,  0.0f },
               { T::lowPass,  9000.0f, 0.80f,  0.0f },
               {}, {} }},
            7, { 0.90f, 2.10f }, { 0.12f, 0.07f }
        },
        {
            "1x12 Combo", "Small open-back combo. Boxy, focused midrange, limited low end.",
            {{ { T::highPass,  110.0f, 0.70f,  0.0f },
               { T::peak,      180.0f, 1.50f,  3.0f },   // small-box resonance
               { T::peak,     1200.0f, 1.00f,  2.5f },
               { T::peak,     3200.0f, 2.00f,  2.0f },
               { T::lowPass,  5500.0f, 0.80f,  0.0f },
               { T::lowPass,  6500.0f, 1.00f,  0.0f },
               {}, {}, {} }},
            6, { 0.40f, 1.00f }, { 0.14f, 0.08f }
        }
    }};
    return models;
}

//==============================================================================
/** Mono cabinet stage: one of the built-in models or a user impulse response, followed by a
    mic-position tilt and low/high cut. */
class CabSim
{
public:
    static constexpr int kUserIR = 4;

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        convolution.prepare ({ fs, (juce::uint32) maxBlock, 1 });
        reflections.allocate ((int) std::ceil (0.004 * fs));
        setModel (model);
        setTone (mic, lowCut, highCut);
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : filters) f.reset();
        micShelf.reset(); lowCutF.reset(); highCutF.reset();
        reflections.clear();
        convolution.reset();
    }

    void setModel (int index) noexcept
    {
        model = std::clamp (index, 0, kUserIR);
        if (model < kUserIR)
        {
            const auto& m = cabModels()[(size_t) model];
            numFilters = m.numFilters;
            for (int i = 0; i < numFilters; ++i)
                filters[(size_t) i].set (m.filters[(size_t) i].type, fs, m.filters[(size_t) i].freq,
                                         m.filters[(size_t) i].q, m.filters[(size_t) i].gainDb);
            for (int i = 0; i < 2; ++i)
            {
                tapDelay[i] = m.reflectionMs[i] * 0.001 * fs;
                tapGain[i]  = m.reflectionGain[i];
            }
        }
        for (auto& f : filters) f.reset();
        reflections.clear();
    }

    int getModel() const noexcept { return model; }

    /** mic: 0 = cap centre (bright) .. 1 = cone edge (dark). */
    void setTone (float newMic, float newLowCutHz, float newHighCutHz) noexcept
    {
        mic = newMic; lowCut = newLowCutHz; highCut = newHighCutHz;
        micShelf.set (Biquad::Type::highShelf, fs, 2500.0, 0.6, micTiltDb (mic));
        lowCutF.set  (Biquad::Type::highPass,  fs, lowCut,  0.7071);
        highCutF.set (Biquad::Type::lowPass,   fs, highCut, 0.7071);
    }

    /** Called from the message thread; JUCE swaps the new IR in on its background thread. */
    void loadImpulseResponse (juce::AudioBuffer<float>&& ir, double irSampleRate)
    {
        convolution.loadImpulseResponse (std::move (ir), irSampleRate,
                                         juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);
    }

    void process (float* data, int n) noexcept
    {
        if (model == kUserIR)
        {
            float* chans[] = { data };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
            convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                const float in = data[i];
                double x = in + tapGain[0] * reflections.readLinear (tapDelay[0])
                              + tapGain[1] * reflections.readLinear (tapDelay[1]);
                reflections.push (in);

                for (int k = 0; k < numFilters; ++k)
                    x = filters[(size_t) k].process (x);

                data[i] = (float) x;
            }
        }

        for (int i = 0; i < n; ++i)
            data[i] = (float) highCutF.process (lowCutF.process (micShelf.process (data[i])));
    }

    static double micTiltDb (float mic) noexcept { return (0.5 - (double) mic) * 10.0; }

    /** Magnitude in dB of a built-in model including mic and cuts, for the UI. */
    static void displayResponseDb (int modelIndex, float mic, float lowCutHz, float highCutHz,
                                   const double* freqs, double* outDb, int n)
    {
        constexpr double dfs = 48000.0;
        const auto& m = cabModels()[(size_t) std::clamp (modelIndex, 0, 3)];
        std::array<Biquad, 9> fl;
        for (int i = 0; i < m.numFilters; ++i)
            fl[(size_t) i].set (m.filters[(size_t) i].type, dfs, m.filters[(size_t) i].freq,
                                m.filters[(size_t) i].q, m.filters[(size_t) i].gainDb);

        for (int k = 0; k < n; ++k)
        {
            const double f = freqs[k];
            double mag = 1.0;
            for (int i = 0; i < m.numFilters; ++i)
                mag *= fl[(size_t) i].magnitude (dfs, f);

            const auto refl = 1.0 + (double) m.reflectionGain[0] * std::polar (1.0, -kTwoPi * f * m.reflectionMs[0] * 0.001)
                                  + (double) m.reflectionGain[1] * std::polar (1.0, -kTwoPi * f * m.reflectionMs[1] * 0.001);
            outDb[k] = gainToDb (mag * std::abs (refl)) + postFilterDb (mic, lowCutHz, highCutHz, f);
        }
    }

    /** Mic tilt + cuts in dB at frequency f (also applied on top of a user IR's response). */
    static double postFilterDb (float mic, float lowCutHz, float highCutHz, double f)
    {
        constexpr double dfs = 48000.0;
        Biquad shelf, lc, hc;
        shelf.set (Biquad::Type::highShelf, dfs, 2500.0, 0.6, micTiltDb (mic));
        lc.set (Biquad::Type::highPass, dfs, lowCutHz, 0.7071);
        hc.set (Biquad::Type::lowPass,  dfs, highCutHz, 0.7071);
        return gainToDb (shelf.magnitude (dfs, f) * lc.magnitude (dfs, f) * hc.magnitude (dfs, f));
    }

private:
    double fs = 48000.0;
    int model = 0, numFilters = 0;
    float mic = 0.3f, lowCut = 70.0f, highCut = 11000.0f;

    std::array<Biquad, 9> filters;
    Biquad micShelf, lowCutF, highCutF;
    DelayLine reflections;
    double tapDelay[2] {}, tapGain[2] {};

    juce::dsp::Convolution convolution;
};
} // namespace forge
