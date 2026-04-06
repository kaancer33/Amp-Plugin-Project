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
class NewProjectAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::FileDragAndDropTarget,
                                        private juce::Timer
{
public:
    explicit NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // ── FileDragAndDropTarget ────────────────────────────────────────────
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // ── Timer (updates NAM status indicator) ────────────────────────────
    void timerCallback() override;

private:
    void applySize (int w, int h);
    void refreshPresetLabel();
    void onRename();
    void onDelete();

    // ── NAM Model Selector ──────────────────────────────────────────────
    void refreshModelList();
    void onLoadModelFile();

    NewProjectAudioProcessor& audioProcessor;
    PixelLookAndFeel laf;
    PresetManager    presetMgr;

    // ── Effect panels ──────────────────────────────────────────────────────
    EffectPanel distortionPanel, delayPanel, reverbPanel;

    // ── NAM Model selector bar ──────────────────────────────────────────
    juce::ComboBox   modelSelector;
    juce::TextButton loadModelBtn  { "LOAD .NAM" };
    juce::Label      modelBarLabel;
    juce::Label      namStatusLabel;   // shows "NAM" (green) or "FALLBACK" (red)

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
