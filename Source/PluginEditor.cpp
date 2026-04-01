#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// EffectPanel
//==============================================================================
EffectPanel::EffectPanel (const juce::String& title,
                           juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& bypassParamId,
                           const std::vector<std::pair<juce::String, juce::String>>& paramLabels)
    : panelTitle (title)
{
    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, bypassParamId, bypassBtn);

    for (auto& [paramId, labelText] : paramLabels)
    {
        auto knob = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag,
                                                     juce::Slider::NoTextBox);
        addAndMakeVisible (*knob);

        auto lbl = std::make_unique<juce::Label>();
        lbl->setText (labelText, juce::dontSendNotification);
        lbl->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*lbl);

        sliderAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, paramId, *knob));
        knobLabels.push_back (std::move (lbl));
        knobs.push_back (std::move (knob));
    }
}

void EffectPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::black);
    g.fillRect (b);

    g.setColour (juce::Colours::white);
    g.drawRect (b, 1.5f);

    const int headerH = juce::roundToInt (getHeight() * 0.16f);
    g.drawHorizontalLine (headerH, b.getX(), b.getRight());

    g.setFont (PixelLookAndFeel::pixelFont ((float) headerH * 0.42f));
    g.drawFittedText (panelTitle,
                       6, 0, getWidth() - bypassBtn.getWidth() - 10, headerH,
                       juce::Justification::centredLeft, 1);
}

void EffectPanel::resized()
{
    const int headerH = juce::roundToInt (getHeight() * 0.16f);
    const int bypassW = juce::roundToInt (getWidth()  * 0.28f);

    bypassBtn.setBounds (getWidth() - bypassW - 4, 2, bypassW, headerH - 4);

    const int numKnobs  = (int) knobs.size();
    if (numKnobs == 0) return;

    const int knobAreaY = headerH + 4;
    const int knobAreaH = getHeight() - knobAreaY - 4;
    const int labelH    = juce::roundToInt (knobAreaH * 0.18f);
    const int slotW     = getWidth() / numKnobs;
    const int knobSize  = juce::jmin (slotW - 10, knobAreaH - labelH - 10);

    for (int i = 0; i < numKnobs; ++i)
    {
        const int slotX = i * slotW;
        const int knobX = slotX + (slotW - knobSize) / 2;
        const int knobY = knobAreaY + (knobAreaH - labelH - knobSize) / 2;
        knobs[i]->setBounds (knobX, knobY, knobSize, knobSize);
        knobLabels[i]->setBounds (slotX, knobY + knobSize + 2, slotW, labelH);
    }
}

//==============================================================================
// NewProjectAudioProcessorEditor
//==============================================================================
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      presetMgr (p),
      overdrivePanel ("OVERDRIVE", p.apvts, "overdriveOn",
                       { {"drive", "DRIVE"}, {"level", "LEVEL"} }),
      delayPanel     ("DELAY",     p.apvts, "delayOn",
                       { {"delayTime", "TIME"}, {"delayFeedback", "FDBK"}, {"delayMix", "MIX"} }),
      reverbPanel    ("REVERB",    p.apvts, "reverbOn",
                       { {"reverbRoom", "ROOM"}, {"reverbDamp", "DAMP"}, {"reverbMix", "MIX"} })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (overdrivePanel);
    addAndMakeVisible (delayPanel);
    addAndMakeVisible (reverbPanel);

    // ── Preset bar ──────────────────────────────────────────────────────────
    presetLbl.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (presetLbl);

    prevBtn.onClick = [this] { presetMgr.prevPreset();  refreshPresetLabel(); };
    nextBtn.onClick = [this] { presetMgr.nextPreset();  refreshPresetLabel(); };
    saveBtn.onClick = [this] { presetMgr.saveCurrentPreset ([this] { refreshPresetLabel(); }); };
    renameBtn.onClick = [this] { onRename(); };
    deleteBtn.onClick = [this] { onDelete(); };

    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &renameBtn, &deleteBtn })
        addAndMakeVisible (b);

    // ── Size buttons ────────────────────────────────────────────────────────
    sizeS.setClickingTogglesState (true);
    sizeM.setClickingTogglesState (true);
    sizeL.setClickingTogglesState (true);

    sizeS.onClick = [this] {
        sizeS.setToggleState (true,  juce::dontSendNotification);
        sizeM.setToggleState (false, juce::dontSendNotification);
        sizeL.setToggleState (false, juce::dontSendNotification);
        setSize (600, 310);
    };
    sizeM.onClick = [this] {
        sizeS.setToggleState (false, juce::dontSendNotification);
        sizeM.setToggleState (true,  juce::dontSendNotification);
        sizeL.setToggleState (false, juce::dontSendNotification);
        setSize (900, 460);
    };
    sizeL.onClick = [this] {
        sizeS.setToggleState (false, juce::dontSendNotification);
        sizeM.setToggleState (false, juce::dontSendNotification);
        sizeL.setToggleState (true,  juce::dontSendNotification);
        setSize (1200, 610);
    };

    for (auto* b : { &sizeS, &sizeM, &sizeL })
        addAndMakeVisible (b);

    refreshPresetLabel();

    sizeM.setToggleState (true, juce::dontSendNotification);
    setSize (900, 460);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void NewProjectAudioProcessorEditor::refreshPresetLabel()
{
    presetLbl.setText (presetMgr.getPresetName (presetMgr.getCurrentIndex()),
                        juce::dontSendNotification);
    const bool isFactory = presetMgr.isFactory (presetMgr.getCurrentIndex());
    renameBtn.setEnabled (!isFactory);
    deleteBtn.setEnabled (!isFactory);
}

void NewProjectAudioProcessorEditor::onRename()
{
    if (presetMgr.isFactory (presetMgr.getCurrentIndex())) return;

    auto* dlg = new juce::AlertWindow ("RENAME PRESET", "Enter new name:",
                                        juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor ("name", presetMgr.getPresetName (presetMgr.getCurrentIndex()));
    dlg->addButton ("OK",     1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dlg->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dlg] (int result) {
            if (result == 1)
            {
                auto name = dlg->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                {
                    presetMgr.renamePreset (presetMgr.getCurrentIndex(), name);
                    refreshPresetLabel();
                }
            }
        }), true);
}

void NewProjectAudioProcessorEditor::onDelete()
{
    if (presetMgr.isFactory (presetMgr.getCurrentIndex())) return;

    const auto name = presetMgr.getPresetName (presetMgr.getCurrentIndex());
    auto* dlg = new juce::AlertWindow ("DELETE PRESET",
                                        "Delete \"" + name + "\"?",
                                        juce::MessageBoxIconType::NoIcon);
    dlg->addButton ("DELETE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dlg->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dlg] (int result) {
            if (result == 1)
            {
                presetMgr.deletePreset (presetMgr.getCurrentIndex());
                presetMgr.loadPreset   (presetMgr.getCurrentIndex());
                refreshPresetLabel();
            }
        }), true);
}

//==============================================================================
void NewProjectAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const int topBarH   = getHeight() / 11;
    const int presetBarH = topBarH;

    g.setColour (juce::Colours::white);
    g.drawHorizontalLine (topBarH,             0.0f, (float) getWidth());
    g.drawHorizontalLine (topBarH + presetBarH, 0.0f, (float) getWidth());

    // Panel dividers (vertical lines between the three panels)
    const int panelY = topBarH + presetBarH;
    const int panelW = getWidth() / 3;
    g.drawVerticalLine (panelW,     (float) panelY, (float) getHeight());
    g.drawVerticalLine (panelW * 2, (float) panelY, (float) getHeight());

    // Title
    g.setFont (PixelLookAndFeel::pixelFont ((float) topBarH * 0.48f));
    g.drawFittedText ("NEWPROJECT", 8, 0, getWidth() / 2, topBarH,
                       juce::Justification::centredLeft, 1);
}

void NewProjectAudioProcessorEditor::resized()
{
    const int W        = getWidth();
    const int H        = getHeight();
    const int topBarH  = H / 11;
    const int presetH  = topBarH;
    const int panelY   = topBarH + presetH;
    const int panelH   = H - panelY;
    const int panelW   = W / 3;

    // ── Size buttons (top-right) ─────────────────────────────────────────
    const int sbH = topBarH - 8;
    const int sbW = sbH + 4;
    sizeL.setBounds (W - sbW - 4,         4, sbW, sbH);
    sizeM.setBounds (W - sbW * 2 - 8,     4, sbW, sbH);
    sizeS.setBounds (W - sbW * 3 - 12,    4, sbW, sbH);

    // ── Preset bar ──────────────────────────────────────────────────────
    const int pbY     = topBarH + 2;
    const int pbH     = presetH - 4;
    const int arrowW  = pbH;
    const int actW    = juce::roundToInt (W * 0.085f);
    const int nameW   = W - arrowW * 2 - actW * 3 - 14;

    int px = 2;
    prevBtn  .setBounds (px, pbY, arrowW, pbH); px += arrowW + 2;
    presetLbl.setBounds (px, pbY, nameW,  pbH); px += nameW  + 2;
    nextBtn  .setBounds (px, pbY, arrowW, pbH); px += arrowW + 4;
    saveBtn  .setBounds (px, pbY, actW,   pbH); px += actW   + 2;
    renameBtn.setBounds (px, pbY, actW,   pbH); px += actW   + 2;
    deleteBtn.setBounds (px, pbY, actW,   pbH);

    // ── Effect panels ────────────────────────────────────────────────────
    overdrivePanel.setBounds (0,          panelY, panelW, panelH);
    delayPanel    .setBounds (panelW,     panelY, panelW, panelH);
    reverbPanel   .setBounds (panelW * 2, panelY, panelW, panelH);
}
