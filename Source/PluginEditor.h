#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PixelLookAndFeel.h"
#include "PresetManager.h"

//==============================================================================
// A single effect panel: title bar with bypass toggle + N rotary knobs
//==============================================================================
class EffectPanel : public juce::Component
{
public:
    EffectPanel (const juce::String& title,
                 juce::AudioProcessorValueTreeState& apvts,
                 const juce::String& bypassParamId,
                 const std::vector<std::pair<juce::String, juce::String>>& paramLabels);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    juce::String panelTitle;
    juce::ToggleButton bypassBtn;

    // Knobs / labels declared before attachments — destroyed after them
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>>  knobLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>              bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectPanel)
};

//==============================================================================
class NewProjectAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void applySize (int w, int h);
    void refreshPresetLabel();
    void onRename();
    void onDelete();

    NewProjectAudioProcessor& audioProcessor;
    PixelLookAndFeel laf;
    PresetManager    presetMgr;

    // ── Effect panels ──────────────────────────────────────────────────────
    EffectPanel distortionPanel, delayPanel, reverbPanel;

    // ── Preset bar ────────────────────────────────────────────────────────
    juce::TextButton prevBtn   { "<" };
    juce::TextButton nextBtn   { ">" };
    juce::Label      presetLbl;
    juce::TextButton saveBtn   { "SAVE"   };
    juce::TextButton renameBtn { "RENAME" };
    juce::TextButton deleteBtn { "DELETE" };

    // ── Size buttons ──────────────────────────────────────────────────────
    juce::TextButton sizeS { "S" };
    juce::TextButton sizeM { "M" };
    juce::TextButton sizeL { "L" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
