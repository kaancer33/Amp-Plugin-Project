#pragma once
#include "DspCommon.h"
#include "ToneStack.h"

namespace forge
{
//==============================================================================
/** One 12AX7-style common-cathode stage as a Wiener-Hammerstein block:
    coupling-cap high-pass -> gain -> asymmetric saturation (inverting) -> Miller-cap low-pass.

    Asymmetry: positive grid swings run into grid conduction and compress early and softly;
    negative swings have more headroom and then cut off with a harder knee. Each stage inverts,
    so the asymmetry alternates through the cascade like on a real preamp, and the DC it creates
    is removed by the next coupling cap, which shifts the bias under heavy playing (the "bloom"). */
struct TubeStageSpec
{
    float couplingHz;   // coupling cap / cathode bypass: how much bass reaches the grid
    float millerHz;     // Miller capacitance roll-off
    float driveDb;      // stage gain into the grid
    float headroom;     // cutoff-side headroom relative to the grid-conduction side
};

class TubeStage
{
public:
    void configure (double fs, const TubeStageSpec& s) noexcept
    {
        coupling.setCutoff (fs, s.couplingHz);
        miller.setCutoff (fs, s.millerHz);
        drive = dbToGain (s.driveDb);
        headroom = s.headroom;
    }

    void reset() noexcept { coupling.reset(); miller.reset(); }

    inline double process (double x) noexcept
    {
        const double u = coupling.process (x) * drive;
        return miller.process (-shape (u, headroom));
    }

    static inline double shape (double u, double h) noexcept
    {
        if (u >= 0.0)
            return fastTanh (u);

        const double v = -u / h;
        return -h * v / std::sqrt (std::sqrt (1.0 + v * v * v * v));
    }

private:
    OnePoleHP coupling;
    OnePoleLP miller;
    double drive = 1.0, headroom = 1.5;
};

//==============================================================================
/** Describes one amp voice: circuit topology plus tone-stack components. */
struct AmpVoice
{
    const char* name;
    const char* description;
    ToneStackComponents toneStack;
    int numStages;
    int gainPotBeforeStage;    // the gain/volume pot sits in front of this stage
    int toneStackBeforeStage;  // ...and the tone stack in front of this one (numStages = after the last)
    TubeStageSpec stages[4];
    float gainMinDb, gainMaxDb;
    float brightDb;            // bright cap: treble bypasses the gain pot at low settings
    float inputHighPassHz;
    float sag;                 // power-supply sag; tube rectifiers sag more
    float powerDriveDb;
    float outputTrimDb;        // level match between voices
    float levelComp;           // fraction of the gain pot's dB swing compensated at the output, so
                               // Gain mostly changes distortion and Master sets the volume
};

inline const std::array<AmpVoice, 3>& ampVoices()
{
    static const std::array<AmpVoice, 3> voices {{
        {
            "Clean",
            "American clean. Twin Reverb tone stack, two gain stages, tube-rectifier sag.",
            toneStacks::twin,
            2, 1, 1,
            { { 20.0f, 14000.0f,  6.0f, 1.8f },
               { 25.0f, 11000.0f, 14.0f, 1.6f },
               {}, {} },
            -26.0f, 14.0f,
            0.0f,
            30.0f,
            0.55f,
            0.0f,
            -6.0f,
            0.25f
        },
        {
            "Crunch",
            "British crunch. JCM800 tone stack after three stages, bright cap, tight bass.",
            toneStacks::jcm800,
            3, 1, 3,
            { { 30.0f, 14000.0f,  8.0f, 1.6f },
               { 110.0f, 10000.0f, 16.0f, 1.4f },
               { 40.0f,  9000.0f, 10.0f, 1.5f },
               {} },
            -32.0f, 6.0f,
            7.0f,
            35.0f,
            0.30f,
            0.0f,
            -8.5f,
            0.18f
        },
        {
            "Lead",
            "Modern high gain. Four cascaded stages, Soldano tone stack, tight rectifier.",
            toneStacks::soldano,
            4, 1, 4,
            { { 90.0f, 12000.0f, 10.0f, 1.6f },
               { 160.0f, 9000.0f, 20.0f, 1.4f },
               { 100.0f, 7500.0f, 18.0f, 1.5f },
               { 50.0f,  7000.0f,  8.0f, 1.2f } },
            -50.0f, 6.0f,
            3.0f,
            60.0f,
            0.18f,
            0.0f,
            -12.0f,
            0.10f
        }
    }};
    return voices;
}

//==============================================================================
/** Preamp (cascaded triode stages + tone stack) and power amp. Runs at the oversampled rate. */
class AmpEngine
{
public:
    struct Controls
    {
        float gain = 0.5f, bass = 0.5f, mid = 0.5f, treble = 0.5f;
        float presence = 0.5f, depth = 0.5f, master = 0.5f;   // all 0..1
    };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        toneStack.prepare (fs);
        sagEnv.setCutoff (fs, 13.0);
        dcBlock.setCutoff (fs, 8.0);
        setVoice (voiceIndex);
    }

    void setVoice (int index) noexcept
    {
        voiceIndex = std::clamp (index, 0, (int) ampVoices().size() - 1);
        const auto& v = ampVoices()[(size_t) voiceIndex];

        for (int i = 0; i < 4; ++i)
            if (i < v.numStages)
                stages[i].configure (fs, v.stages[i]);

        inputHP.setCutoff (fs, v.inputHighPassHz);
        toneStack.setComponents (v.toneStack);
        outputTrim = dbToGain (v.outputTrimDb);
        reset();
        setControls (controls, true);
    }

    int getVoice() const noexcept { return voiceIndex; }

    void reset() noexcept
    {
        for (auto& s : stages) s.reset();
        inputHP.reset();
        toneStack.reset();
        bright.reset();
        presence.reset();
        depth.reset();
        sagEnv.reset();
        dcBlock.reset();
    }

    void setControls (const Controls& c, bool force = false) noexcept
    {
        const auto& v = ampVoices()[(size_t) voiceIndex];

        if (force || c.gain != controls.gain)
        {
            const double potDb = v.gainMinDb + (v.gainMaxDb - v.gainMinDb) * c.gain;
            gainPot = dbToGain (potDb);
            levelComp = dbToGain (-v.levelComp * (potDb - 0.5 * (v.gainMinDb + v.gainMaxDb)));
            const double brightAmount = (1.0 - c.gain) * (1.0 - c.gain);
            bright.set (Biquad::Type::highShelf, fs, 1800.0, 0.6, v.brightDb * brightAmount);
        }

        if (force || c.bass != controls.bass || c.mid != controls.mid || c.treble != controls.treble)
            toneStack.setControls (c.bass, c.mid, c.treble);

        if (force || c.presence != controls.presence)
            presence.set (Biquad::Type::highShelf, fs, 3200.0, 0.6, (c.presence - 0.5) * 18.0);

        if (force || c.depth != controls.depth)
            depth.set (Biquad::Type::peak, fs, 95.0, 0.8, (c.depth - 0.5) * 18.0);

        if (force || c.master != controls.master)
            powerDrive = dbToGain (-20.0 + 30.0 * c.master + v.powerDriveDb);

        sagAmount = v.sag * (0.5 + c.master);
        controls = c;
    }

    inline double process (double x) noexcept
    {
        const auto& v = ampVoices()[(size_t) voiceIndex];
        x = inputHP.process (x);

        for (int i = 0; i < v.numStages; ++i)
        {
            if (i == v.toneStackBeforeStage) x = toneStack.process (x);
            if (i == v.gainPotBeforeStage)   x = bright.process (x * gainPot);
            x = stages[i].process (x);
        }

        if (v.toneStackBeforeStage >= v.numStages)
            x = toneStack.process (x);

        // Power amp. Presence and depth act inside the negative-feedback loop on a real amp;
        // here they shape what drives the push-pull pair.
        x = depth.process (presence.process (x)) * powerDrive;

        // Supply sag: the B+ rail droops with average current draw, so sustained loud
        // passages lose headroom and compress, then recover.
        const double env = sagEnv.process (x * x);
        const double sagGain = 1.0 / (1.0 + sagAmount * env);
        const double y = fastTanh (x * sagGain) * (0.65 + 0.35 * sagGain);

        return dcBlock.process (y) * outputTrim * levelComp;
    }

    /** The EQ the player hears from the tone controls (tone stack + presence + depth), in dB,
        for the response display. Independent of the audio thread's filter state. */
    static void displayResponseDb (int voice, const Controls& c, const double* freqs, double* outDb, int n)
    {
        ToneStack ts;
        ts.setComponents (ampVoices()[(size_t) std::clamp (voice, 0, (int) ampVoices().size() - 1)].toneStack);
        Biquad pres, dep;
        pres.set (Biquad::Type::highShelf, 48000.0, 3200.0, 0.6, (c.presence - 0.5) * 18.0);
        dep.set  (Biquad::Type::peak,      48000.0,   95.0, 0.8, (c.depth - 0.5) * 18.0);

        for (int i = 0; i < n; ++i)
            outDb[i] = gainToDb (ts.displayMagnitude (c.bass, c.mid, c.treble, freqs[i])
                                 * pres.magnitude (48000.0, freqs[i]) * dep.magnitude (48000.0, freqs[i]));
    }

private:
    double fs = 192000.0;
    int voiceIndex = 1;
    Controls controls;

    TubeStage stages[4];
    OnePoleHP inputHP;
    ToneStack toneStack;
    Biquad bright, presence, depth;
    OnePoleLP sagEnv;
    OnePoleHP dcBlock;

    double gainPot = 1.0, powerDrive = 1.0, sagAmount = 0.3, outputTrim = 1.0, levelComp = 1.0;
};
} // namespace forge
