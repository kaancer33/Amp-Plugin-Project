#pragma once
#include <JuceHeader.h>

namespace forge
{
namespace info
{
    inline constexpr const char* productName = "Forge Amp";
   #if defined (FORGE_VERSION_STRING)
    inline constexpr const char* version = FORGE_VERSION_STRING;
   #elif defined (JucePlugin_VersionString)
    inline constexpr const char* version = JucePlugin_VersionString;
   #else
    inline constexpr const char* version = "dev";
   #endif
    inline constexpr const char* buildDate = __DATE__ " " __TIME__;
}

/** Parameter IDs. These are saved in sessions and presets - never rename one. */
namespace ids
{
    #define FORGE_PARAM(name) inline constexpr const char* name = #name;
    FORGE_PARAM (inputGain)
    FORGE_PARAM (outputGain)

    FORGE_PARAM (gateOn)
    FORGE_PARAM (gateThreshold)
    FORGE_PARAM (gateRelease)

    FORGE_PARAM (boostOn)
    FORGE_PARAM (boostDrive)
    FORGE_PARAM (boostTone)
    FORGE_PARAM (boostLevel)

    FORGE_PARAM (ampOn)
    FORGE_PARAM (ampModel)
    FORGE_PARAM (ampGain)
    FORGE_PARAM (ampBass)
    FORGE_PARAM (ampMid)
    FORGE_PARAM (ampTreble)
    FORGE_PARAM (ampPresence)
    FORGE_PARAM (ampDepth)
    FORGE_PARAM (ampMaster)

    FORGE_PARAM (cabOn)
    FORGE_PARAM (cabModel)
    FORGE_PARAM (cabMic)
    FORGE_PARAM (cabLowCut)
    FORGE_PARAM (cabHighCut)

    FORGE_PARAM (delayOn)
    FORGE_PARAM (delaySync)
    FORGE_PARAM (delayTime)
    FORGE_PARAM (delayDivision)
    FORGE_PARAM (delayFeedback)
    FORGE_PARAM (delayTone)
    FORGE_PARAM (delayMix)
    FORGE_PARAM (delayPingPong)

    FORGE_PARAM (reverbOn)
    FORGE_PARAM (reverbSize)
    FORGE_PARAM (reverbDecay)
    FORGE_PARAM (reverbPreDelay)
    FORGE_PARAM (reverbTone)
    FORGE_PARAM (reverbMix)
    #undef FORGE_PARAM
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

const juce::StringArray& ampModelNames();
const juce::StringArray& cabModelNames();
const juce::StringArray& delayDivisionNames();

/** Length of a delay division in quarter-note beats. */
double delayDivisionBeats (int index);

/** Every parameter ID, in a stable order. */
const juce::StringArray& allParameterIds();
} // namespace forge
