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
      distortionPanel ("DISTORTION", p.apvts, "distortionOn",
                       { {"distDrive", "DRIVE"}, {"distBass", "BASS"}, {"distMid", "MID"},
                         {"distTreble", "TREB"}, {"distGate", "GATE"}, {"level", "LEVEL"} }),
      delayPanel     ("DELAY",     p.apvts, "delayOn",
                       { {"delayTime", "TIME"}, {"delayFeedback", "FDBK"},
                         {"delayTone", "TONE"}, {"delayMix", "MIX"} }),
      reverbPanel    ("REVERB",    p.apvts, "reverbOn",
                       { {"reverbRoom", "ROOM"}, {"reverbPreDelay", "PRE-D"},
                         {"reverbDamp", "DAMP"}, {"reverbMix", "MIX"} })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (distortionPanel);
    addAndMakeVisible (delayPanel);
    addAndMakeVisible (reverbPanel);

    // ── NAM Model selector ─────────────────────────────────────────────────
    modelBarLabel.setText ("MODEL:", juce::dontSendNotification);
    modelBarLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (modelBarLabel);

    modelSelector.setJustificationType (juce::Justification::centredLeft);
    modelSelector.onChange = [this]
    {
        int idx = modelSelector.getSelectedItemIndex();
        if (idx >= 0)
        {
            auto names = audioProcessor.getAvailableModelNames();
            if (idx < names.size())
            {
                audioProcessor.loadModelByName (names[idx]);
            }
        }
    };
    addAndMakeVisible (modelSelector);

    loadModelBtn.onClick = [this] { onLoadModelFile(); };
    addAndMakeVisible (loadModelBtn);

    refreshModelList();

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
        setSize (600, 340);
    };
    sizeM.onClick = [this] {
        sizeS.setToggleState (false, juce::dontSendNotification);
        sizeM.setToggleState (true,  juce::dontSendNotification);
        sizeL.setToggleState (false, juce::dontSendNotification);
        setSize (900, 500);
    };
    sizeL.onClick = [this] {
        sizeS.setToggleState (false, juce::dontSendNotification);
        sizeM.setToggleState (false, juce::dontSendNotification);
        sizeL.setToggleState (true,  juce::dontSendNotification);
        setSize (1200, 660);
    };

    for (auto* b : { &sizeS, &sizeM, &sizeL })
        addAndMakeVisible (b);

    refreshPresetLabel();

    sizeM.setToggleState (true, juce::dontSendNotification);
    setSize (900, 500);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void NewProjectAudioProcessorEditor::refreshModelList()
{
    modelSelector.clear (juce::dontSendNotification);
    auto names = audioProcessor.getAvailableModelNames();

    if (names.size() == 0)
    {
        // No models found — show helpful message
        modelSelector.addItem ("-- No models found. Click LOAD .NAM --", 1);
        modelSelector.setSelectedItemIndex (0, juce::dontSendNotification);
        modelBarLabel.setText ("MODEL:", juce::dontSendNotification);
        return;
    }

    for (int i = 0; i < names.size(); ++i)
        modelSelector.addItem (names[i], i + 1);   // ComboBox IDs start at 1

    // Select current model
    int currentIdx = audioProcessor.getCurrentModelIndex();
    if (currentIdx >= 0)
        modelSelector.setSelectedItemIndex (currentIdx, juce::dontSendNotification);
    else if (names.size() > 0)
        modelSelector.setSelectedItemIndex (0, juce::dontSendNotification);

    // Show model status
    modelBarLabel.setText (audioProcessor.isModelLoaded() ? "MODEL:" : "NO MODEL!",
                           juce::dontSendNotification);
}

void NewProjectAudioProcessorEditor::onLoadModelFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load NAM Model",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.nam");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.size() > 0)
            {
                auto file = results[0];
                audioProcessor.importNAMModel (file);
                refreshModelList();
            }
        });
}

//==============================================================================
// Drag & Drop support — drop .nam files directly onto the plugin window
//==============================================================================
bool NewProjectAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".nam"))
            return true;
    return false;
}

void NewProjectAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.hasFileExtension (".nam"))
        {
            audioProcessor.importNAMModel (file);
            refreshModelList();
            break;  // Load first valid .nam file
        }
    }
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

    const int topBarH    = getHeight() / 13;
    const int modelBarH  = topBarH;
    const int presetBarH = topBarH;

    g.setColour (juce::Colours::white);
    g.drawHorizontalLine (topBarH,                         0.0f, (float) getWidth());
    g.drawHorizontalLine (topBarH + modelBarH,             0.0f, (float) getWidth());
    g.drawHorizontalLine (topBarH + modelBarH + presetBarH, 0.0f, (float) getWidth());

    // Panel dividers (vertical lines between the three panels)
    const int panelY = topBarH + modelBarH + presetBarH;
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
    const int W         = getWidth();
    const int H         = getHeight();
    const int topBarH   = H / 13;
    const int modelBarH = topBarH;
    const int presetH   = topBarH;
    const int panelY    = topBarH + modelBarH + presetH;
    const int panelH    = H - panelY;
    const int panelW    = W / 3;

    // ── Size buttons (top-right) ─────────────────────────────────────────
    const int sbH = topBarH - 8;
    const int sbW = sbH + 4;
    sizeL.setBounds (W - sbW - 4,         4, sbW, sbH);
    sizeM.setBounds (W - sbW * 2 - 8,     4, sbW, sbH);
    sizeS.setBounds (W - sbW * 3 - 12,    4, sbW, sbH);

    // ── NAM Model selector bar ──────────────────────────────────────────
    const int mbY = topBarH + 2;
    const int mbH = modelBarH - 4;
    const int labelW = juce::roundToInt (W * 0.08f);
    const int loadW  = juce::roundToInt (W * 0.12f);
    const int comboW = W - labelW - loadW - 14;

    int mx = 4;
    modelBarLabel.setBounds (mx, mbY, labelW, mbH);  mx += labelW + 2;
    modelSelector.setBounds (mx, mbY, comboW, mbH);  mx += comboW + 4;
    loadModelBtn.setBounds  (mx, mbY, loadW,  mbH);

    // ── Preset bar ──────────────────────────────────────────────────────
    const int pbY     = topBarH + modelBarH + 2;
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
    distortionPanel.setBounds (0,          panelY, panelW, panelH);
    delayPanel    .setBounds (panelW,     panelY, panelW, panelH);
    reverbPanel   .setBounds (panelW * 2, panelY, panelW, panelH);
}
