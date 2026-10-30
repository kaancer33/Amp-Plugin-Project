#pragma once
#include "Parameters.h"

namespace forge
{
/** Factory presets. Values are in each parameter's own units (dB, Hz, 0-10 knobs...);
    anything not listed takes its default, so adding a parameter never breaks a preset.
    Choice parameters take the choice index. */
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, double>> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace ids;
    // Amp models: 0 Clean, 1 Crunch, 2 Lead.
    // Cab models: 0 4x12 Modern, 1 4x12 Vintage, 2 2x12 Open, 3 1x12 Combo.
    // Delay divisions: 5 = 1/8, 6 = 1/8D, 8 = 1/4, 9 = 1/4D.
    static const std::vector<FactoryPreset> presets {
        { "Glass Clean", {
            { ampModel, 0 }, { ampGain, 3.5f }, { ampBass, 5.5f }, { ampMid, 4.5f }, { ampTreble, 6.5f },
            { ampPresence, 6.0f }, { ampDepth, 5.0f }, { ampMaster, 5.5f },
            { gateThreshold, -70.0f },
            { cabModel, 2 }, { cabMic, 3.0f }, { cabLowCut, 70.0f }, { cabHighCut, 12000.0f },
            { reverbOn, 1 }, { reverbSize, 5.5f }, { reverbDecay, 2.2f }, { reverbMix, 24.0f }, { reverbTone, 7500.0f } } },

        { "Ambient Swells", {
            { ampModel, 0 }, { ampGain, 3.0f }, { ampBass, 5.0f }, { ampMid, 5.0f }, { ampTreble, 6.0f },
            { ampPresence, 5.5f }, { ampMaster, 6.0f },
            { gateThreshold, -72.0f },
            { cabModel, 2 }, { cabMic, 4.0f },
            { delayOn, 1 }, { delaySync, 1 }, { delayDivision, 6 }, { delayFeedback, 48.0f }, { delayMix, 34.0f },
            { delayTone, 3800.0f }, { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbSize, 9.0f }, { reverbDecay, 5.5f }, { reverbPreDelay, 40.0f },
            { reverbMix, 40.0f }, { reverbTone, 6000.0f } } },

        { "Edge of Breakup", {
            { ampModel, 0 }, { ampGain, 7.5f }, { ampBass, 5.0f }, { ampMid, 6.0f }, { ampTreble, 6.0f },
            { ampPresence, 5.5f }, { ampMaster, 6.0f },
            { cabModel, 3 }, { cabMic, 3.5f },
            { reverbOn, 1 }, { reverbSize, 3.0f }, { reverbDecay, 1.3f }, { reverbMix, 16.0f } } },

        { "Plexi Crunch", {
            { ampModel, 1 }, { ampGain, 5.0f }, { ampBass, 5.0f }, { ampMid, 6.5f }, { ampTreble, 6.0f },
            { ampPresence, 5.5f }, { ampDepth, 5.0f }, { ampMaster, 6.0f },
            { gateThreshold, -66.0f },
            { cabModel, 1 }, { cabMic, 3.0f },
            { reverbOn, 1 }, { reverbSize, 3.5f }, { reverbDecay, 1.4f }, { reverbMix, 14.0f } } },

        { "Classic Rock", {
            { ampModel, 1 }, { ampGain, 7.0f }, { ampBass, 5.5f }, { ampMid, 6.5f }, { ampTreble, 6.0f },
            { ampPresence, 6.0f }, { ampMaster, 6.0f },
            { boostOn, 1 }, { boostDrive, 3.0f }, { boostTone, 5.0f }, { boostLevel, 2.0f },
            { gateThreshold, -64.0f },
            { cabModel, 1 },
            { delayOn, 1 }, { delayDivision, 8 }, { delayFeedback, 22.0f }, { delayMix, 14.0f },
            { reverbOn, 1 }, { reverbSize, 4.0f }, { reverbDecay, 1.6f }, { reverbMix, 14.0f } } },

        { "Blues Lead", {
            { ampModel, 1 }, { ampGain, 6.0f }, { ampBass, 4.5f }, { ampMid, 7.0f }, { ampTreble, 5.0f },
            { ampPresence, 5.0f }, { ampMaster, 5.5f },
            { boostOn, 1 }, { boostDrive, 5.0f }, { boostTone, 4.0f }, { boostLevel, 1.0f },
            { cabModel, 1 }, { cabMic, 4.5f },
            { delayOn, 1 }, { delayDivision, 6 }, { delayFeedback, 30.0f }, { delayMix, 20.0f }, { delayTone, 3500.0f },
            { reverbOn, 1 }, { reverbSize, 5.0f }, { reverbDecay, 2.0f }, { reverbMix, 20.0f } } },

        { "Modern Rhythm", {
            { ampModel, 2 }, { ampGain, 6.0f }, { ampBass, 6.0f }, { ampMid, 4.5f }, { ampTreble, 6.5f },
            { ampPresence, 6.0f }, { ampDepth, 6.0f }, { ampMaster, 5.5f },
            { boostOn, 1 }, { boostDrive, 0.0f }, { boostTone, 6.0f }, { boostLevel, 6.0f },
            { gateThreshold, -56.0f }, { gateRelease, 60.0f },
            { cabModel, 0 }, { cabMic, 3.0f }, { cabLowCut, 80.0f }, { cabHighCut, 10000.0f },
            { reverbOn, 1 }, { reverbSize, 3.0f }, { reverbDecay, 1.0f }, { reverbMix, 8.0f } } },

        { "Djent Tight", {
            { ampModel, 2 }, { ampGain, 7.0f }, { ampBass, 5.0f }, { ampMid, 6.0f }, { ampTreble, 6.5f },
            { ampPresence, 6.5f }, { ampDepth, 6.0f }, { ampMaster, 5.0f },
            { boostOn, 1 }, { boostDrive, 0.0f }, { boostTone, 7.0f }, { boostLevel, 8.0f },
            { gateThreshold, -50.0f }, { gateRelease, 35.0f },
            { cabModel, 0 }, { cabMic, 2.5f }, { cabLowCut, 95.0f }, { cabHighCut, 9500.0f },
            { reverbOn, 0 } } },

        { "Soaring Lead", {
            { ampModel, 2 }, { ampGain, 7.5f }, { ampBass, 5.0f }, { ampMid, 6.5f }, { ampTreble, 5.5f },
            { ampPresence, 5.5f }, { ampMaster, 5.5f },
            { gateThreshold, -62.0f },
            { cabModel, 0 }, { cabMic, 4.5f },
            { delayOn, 1 }, { delayDivision, 8 }, { delayFeedback, 38.0f }, { delayMix, 26.0f },
            { delayTone, 4000.0f }, { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbSize, 6.5f }, { reverbDecay, 3.0f }, { reverbPreDelay, 30.0f }, { reverbMix, 22.0f } } },

        { "Doom Sludge", {
            { ampModel, 2 }, { ampGain, 9.0f }, { ampBass, 7.5f }, { ampMid, 3.5f }, { ampTreble, 4.0f },
            { ampPresence, 4.0f }, { ampDepth, 7.0f }, { ampMaster, 7.0f },
            { gateThreshold, -58.0f }, { gateRelease, 150.0f },
            { cabModel, 1 }, { cabMic, 6.0f }, { cabLowCut, 50.0f }, { cabHighCut, 8000.0f },
            { reverbOn, 1 }, { reverbSize, 6.0f }, { reverbDecay, 2.6f }, { reverbMix, 14.0f }, { reverbTone, 4000.0f } } },
    };
    return presets;
}
} // namespace forge
