#include "Parameters.h"
#include "dsp/AmpEngine.h"
#include "dsp/CabSim.h"

namespace forge
{
namespace
{
    using Float  = juce::AudioParameterFloat;
    using Bool   = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using Range  = juce::NormalisableRange<float>;

    juce::ParameterID pid (const char* id) { return { id, 1 }; }

    juce::String formatDb (float v, int)
    {
        if (std::abs (v) < 0.05f) return "0.0 dB";
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
    }

    juce::String formatTen (float v, int)  { return juce::String (v, 1); }
    juce::String formatPct (float v, int)  { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String formatSec (float v, int)  { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; }

    juce::String formatHz (float v, int)
    {
        if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " Hz";
        return juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
    }

    juce::String formatMs (float v, int)
    {
        if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v / 1000.0f, 2) + " s";
    }

    /** Parses "6", "6 dB", "2.5k", "2.5 kHz", "1.2 s" etc. back into the parameter's unit. */
    float parseValue (const juce::String& text)
    {
        auto t = text.trim().toLowerCase();
        const bool kilo    = t.containsChar ('k');
        const bool seconds = t.endsWith ("s") && ! t.endsWith ("ms");
        const auto v = t.retainCharacters ("0123456789.-+").getFloatValue();
        return kilo || seconds ? v * 1000.0f : v;
    }

    std::unique_ptr<Float> floatParam (const char* id, const char* name, Range range, float def,
                                       juce::String (*fmt) (float, int), const char* label = "",
                                       bool parseAsMs = false)
    {
        auto attrs = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction (fmt)
                        .withLabel (label);

        if (parseAsMs)
            attrs = attrs.withValueFromStringFunction ([] (const juce::String& s) { return parseValue (s); });
        else
            attrs = attrs.withValueFromStringFunction ([] (const juce::String& s)
                                                       {
                                                           auto t = s.trim().toLowerCase();
                                                           const auto v = t.retainCharacters ("0123456789.-+").getFloatValue();
                                                           return t.containsChar ('k') ? v * 1000.0f : v;
                                                       });

        return std::make_unique<Float> (pid (id), name, range, def, attrs);
    }

    Range tenRange() { return { 0.0f, 10.0f, 0.01f }; }

    Range skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        Range r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<Bool> boolParam (const char* id, const char* name, bool def)
    {
        return std::make_unique<Bool> (pid (id), name, def);
    }
}

const juce::StringArray& ampModelNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray s;
        for (auto& v : ampVoices()) s.add (v.name);
        return s;
    }();
    return names;
}

const juce::StringArray& cabModelNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray s;
        for (auto& m : cabModels()) s.add (m.name);
        s.add ("User IR");
        return s;
    }();
    return names;
}

const juce::StringArray& delayDivisionNames()
{
    static const juce::StringArray names { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                           "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" };
    return names;
}

double delayDivisionBeats (int index)
{
    static const double beats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                                  2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Group = juce::AudioProcessorParameterGroup;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Group> ("io", "Input / Output", "|",
        floatParam (ids::inputGain,  "Input",  { -24.0f, 24.0f, 0.1f }, 0.0f, formatDb, "dB"),
        floatParam (ids::outputGain, "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, formatDb, "dB")));

    layout.add (std::make_unique<Group> ("gate", "Gate", "|",
        boolParam  (ids::gateOn, "Gate On", true),
        floatParam (ids::gateThreshold, "Gate Threshold", { -96.0f, -20.0f, 0.5f }, -62.0f, formatDb, "dB"),
        floatParam (ids::gateRelease, "Gate Release", skewed (10.0f, 500.0f, 100.0f, 1.0f), 90.0f, formatMs, "ms", true)));

    layout.add (std::make_unique<Group> ("boost", "Boost", "|",
        boolParam  (ids::boostOn, "Boost On", false),
        floatParam (ids::boostDrive, "Boost Drive", tenRange(), 2.0f, formatTen),
        floatParam (ids::boostTone,  "Boost Tone",  tenRange(), 5.0f, formatTen),
        floatParam (ids::boostLevel, "Boost Level", { -12.0f, 12.0f, 0.1f }, 0.0f, formatDb, "dB")));

    layout.add (std::make_unique<Group> ("amp", "Amp", "|",
        boolParam  (ids::ampOn, "Amp On", true),
        std::make_unique<Choice> (pid (ids::ampModel), "Amp Model", ampModelNames(), 1),
        floatParam (ids::ampGain,     "Gain",     tenRange(), 5.0f, formatTen),
        floatParam (ids::ampBass,     "Bass",     tenRange(), 5.0f, formatTen),
        floatParam (ids::ampMid,      "Middle",   tenRange(), 5.0f, formatTen),
        floatParam (ids::ampTreble,   "Treble",   tenRange(), 5.0f, formatTen),
        floatParam (ids::ampPresence, "Presence", tenRange(), 5.0f, formatTen),
        floatParam (ids::ampDepth,    "Depth",    tenRange(), 5.0f, formatTen),
        floatParam (ids::ampMaster,   "Master",   tenRange(), 5.0f, formatTen)));

    layout.add (std::make_unique<Group> ("cab", "Cabinet", "|",
        boolParam  (ids::cabOn, "Cab On", true),
        std::make_unique<Choice> (pid (ids::cabModel), "Cab Model", cabModelNames(), 0),
        floatParam (ids::cabMic,     "Mic Position", tenRange(), 3.0f, formatTen),
        floatParam (ids::cabLowCut,  "Cab Low Cut",  skewed (20.0f, 300.0f, 80.0f, 1.0f), 70.0f, formatHz, "Hz"),
        floatParam (ids::cabHighCut, "Cab High Cut", skewed (2000.0f, 20000.0f, 7000.0f, 10.0f), 11000.0f, formatHz, "Hz")));

    layout.add (std::make_unique<Group> ("delay", "Delay", "|",
        boolParam  (ids::delayOn, "Delay On", false),
        boolParam  (ids::delaySync, "Delay Sync", true),
        floatParam (ids::delayTime, "Delay Time", skewed (1.0f, 2000.0f, 350.0f, 1.0f), 375.0f, formatMs, "ms", true),
        std::make_unique<Choice> (pid (ids::delayDivision), "Delay Division", delayDivisionNames(), 8),
        floatParam (ids::delayFeedback, "Delay Feedback", { 0.0f, 95.0f, 0.1f }, 35.0f, formatPct, "%"),
        floatParam (ids::delayTone, "Delay Tone", skewed (500.0f, 12000.0f, 3000.0f, 1.0f), 4500.0f, formatHz, "Hz"),
        floatParam (ids::delayMix,  "Delay Mix",  { 0.0f, 100.0f, 0.1f }, 25.0f, formatPct, "%"),
        boolParam  (ids::delayPingPong, "Delay Ping-Pong", false)));

    layout.add (std::make_unique<Group> ("reverb", "Reverb", "|",
        boolParam  (ids::reverbOn, "Reverb On", true),
        floatParam (ids::reverbSize,  "Reverb Size",  tenRange(), 4.0f, formatTen),
        floatParam (ids::reverbDecay, "Reverb Decay", skewed (0.3f, 12.0f, 2.5f, 0.01f), 1.6f, formatSec, "s"),
        floatParam (ids::reverbPreDelay, "Reverb Pre-Delay", { 0.0f, 200.0f, 1.0f }, 15.0f, formatMs, "ms", true),
        floatParam (ids::reverbTone, "Reverb Tone", skewed (1000.0f, 16000.0f, 5000.0f, 1.0f), 6500.0f, formatHz, "Hz"),
        floatParam (ids::reverbMix,  "Reverb Mix",  { 0.0f, 100.0f, 0.1f }, 18.0f, formatPct, "%")));

    return layout;
}

const juce::StringArray& allParameterIds()
{
    static const juce::StringArray all {
        ids::inputGain, ids::outputGain,
        ids::gateOn, ids::gateThreshold, ids::gateRelease,
        ids::boostOn, ids::boostDrive, ids::boostTone, ids::boostLevel,
        ids::ampOn, ids::ampModel, ids::ampGain, ids::ampBass, ids::ampMid, ids::ampTreble,
        ids::ampPresence, ids::ampDepth, ids::ampMaster,
        ids::cabOn, ids::cabModel, ids::cabMic, ids::cabLowCut, ids::cabHighCut,
        ids::delayOn, ids::delaySync, ids::delayTime, ids::delayDivision, ids::delayFeedback,
        ids::delayTone, ids::delayMix, ids::delayPingPong,
        ids::reverbOn, ids::reverbSize, ids::reverbDecay, ids::reverbPreDelay, ids::reverbTone, ids::reverbMix
    };
    return all;
}
} // namespace forge
