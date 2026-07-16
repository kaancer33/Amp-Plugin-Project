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
    auto b = getLocalBounds().reduced (2);
    const int px = 2;
    const int headerH = juce::roundToInt (getHeight() * 0.16f);

    // Frosted glass card with a deep-blue pixel border
    PixelLookAndFeel::fillSteppedRect (g, b, px, PixelLookAndFeel::deepBlue.withAlpha (0.60f));
    PixelLookAndFeel::fillSteppedRect (g, b.reduced (2), px, juce::Colours::white.withAlpha (0.38f));

    // Glossy aqua header strip
    auto header = b.reduced (2).removeFromTop (headerH);
    PixelLookAndFeel::fillGlossSteppedRect (g, header, px, PixelLookAndFeel::glossBlue, true, false);
    g.setColour (PixelLookAndFeel::deepBlue);
    g.fillRect (header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    g.setFont (PixelLookAndFeel::pixelFont ((float) headerH * 0.42f, true));
    auto titleArea = juce::Rectangle<int> (10, header.getY(),
                                           getWidth() - bypassBtn.getWidth() - 20, header.getHeight());
    g.setColour (PixelLookAndFeel::navy.withAlpha (0.55f));
    g.drawFittedText (panelTitle, titleArea.translated (1, 1), juce::Justification::centredLeft, 1);
    g.setColour (juce::Colours::white);
    g.drawFittedText (panelTitle, titleArea, juce::Justification::centredLeft, 1);
}

void EffectPanel::resized()
{
    const int headerH = juce::roundToInt (getHeight() * 0.16f);
    const int bypassW = juce::roundToInt (getWidth()  * 0.28f);

    bypassBtn.setBounds (getWidth() - bypassW - 6, 5, bypassW, headerH - 6);

    const int numKnobs  = (int) knobs.size();
    if (numKnobs == 0) return;

    // Two-row grid for 4+ knobs so the orbs get room to breathe
    const int rows = numKnobs > 3 ? 2 : 1;
    const int cols = (numKnobs + rows - 1) / rows;

    const int areaY  = headerH + 6;
    const int areaH  = getHeight() - areaY - 6;
    const int rowH   = areaH / rows;
    const int slotW  = (getWidth() - 8) / cols;
    const int labelH = juce::roundToInt (rowH * 0.16f);
    const int knobSize = juce::jmin (slotW - 8, rowH - labelH - 8);

    for (int i = 0; i < numKnobs; ++i)
    {
        const int row         = i / cols;
        const int col         = i - row * cols;
        const int colsThisRow = (row == rows - 1) ? numKnobs - cols * (rows - 1) : cols;
        const int rowX0       = 4 + (getWidth() - 8 - colsThisRow * slotW) / 2;

        const int x0 = rowX0 + col * slotW;
        const int y0 = areaY + row * rowH;
        const int kx = x0 + (slotW - knobSize) / 2;
        const int ky = y0 + (rowH - labelH - knobSize) / 2;
        knobs[i]->setBounds (kx, ky, knobSize, knobSize);
        knobLabels[i]->setBounds (x0, ky + knobSize, slotW, labelH);
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

    // ── NAM status indicator ────────────────────────────────────────────────
    namStatusLabel.setJustificationType (juce::Justification::centred);
    namStatusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (namStatusLabel);
    startTimerHz (4);   // 4 Hz refresh for status

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
    stopTimer();
    setLookAndFeel (nullptr);
}

void NewProjectAudioProcessorEditor::timerCallback()
{
    bool loaded = audioProcessor.isModelLoaded();
    bool processing = audioProcessor.namIsProcessing.load (std::memory_order_relaxed);

    if (loaded && processing)
    {
        namStatusLabel.setText ("NAM", juce::dontSendNotification);
        namStatusLabel.setColour (juce::Label::textColourId, PixelLookAndFeel::statusGreen);
    }
    else if (loaded && !processing)
    {
        namStatusLabel.setText ("NAM?", juce::dontSendNotification);
        namStatusLabel.setColour (juce::Label::textColourId, PixelLookAndFeel::statusAmber);
    }
    else
    {
        namStatusLabel.setText ("FALLBACK", juce::dontSendNotification);
        namStatusLabel.setColour (juce::Label::textColourId, PixelLookAndFeel::statusRed);
    }

    // Update model bar label with error info if applicable
    if (!loaded && audioProcessor.lastLoadError.isNotEmpty())
    {
        modelBarLabel.setText ("ERR:", juce::dontSendNotification);
        modelBarLabel.setColour (juce::Label::textColourId, PixelLookAndFeel::statusRed);
        // Hover either label to see the actual load error
        modelBarLabel.setTooltip (audioProcessor.lastLoadError);
        namStatusLabel.setTooltip (audioProcessor.lastLoadError);
    }
    else
    {
        modelBarLabel.setText (loaded ? "MODEL:" : "NO MODEL!", juce::dontSendNotification);
        modelBarLabel.setColour (juce::Label::textColourId, PixelLookAndFeel::navy);
        modelBarLabel.setTooltip ({});
        namStatusLabel.setTooltip ({});
    }
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
    const int W = getWidth(), H = getHeight();
    const int px = juce::jmax (2, H / 220);

    // ── Dithered sky ─────────────────────────────────────────────────────
    PixelLookAndFeel::fillDitheredV (g, getLocalBounds(),
                                     PixelLookAndFeel::skyTop, PixelLookAndFeel::skyBottom,
                                     8, px);

    // Pixel clouds drifting behind the frosted panels
    PixelLookAndFeel::drawPixelCloud (g, (int) (W * 0.06f), (int) (H * 0.30f), px * 2, 0.55f);
    PixelLookAndFeel::drawPixelCloud (g, (int) (W * 0.52f), (int) (H * 0.24f), px * 3, 0.45f);
    PixelLookAndFeel::drawPixelCloud (g, (int) (W * 0.78f), (int) (H * 0.52f), px * 2, 0.40f);
    PixelLookAndFeel::drawPixelCloud (g, (int) (W * 0.24f), (int) (H * 0.68f), px * 3, 0.35f);

    // Rising bubbles
    PixelLookAndFeel::fillPixelCircleFlat (g, W * 0.90f, H * 0.72f, px * 5.0f,
                                           juce::Colours::white.withAlpha (0.20f), px);
    PixelLookAndFeel::fillPixelCircleFlat (g, W * 0.94f, H * 0.58f, px * 3.0f,
                                           juce::Colours::white.withAlpha (0.25f), px);
    PixelLookAndFeel::fillPixelCircleFlat (g, W * 0.87f, H * 0.44f, px * 2.0f,
                                           juce::Colours::white.withAlpha (0.30f), px);

    const int topBarH    = H / 13;
    const int modelBarH  = topBarH;
    const int presetBarH = topBarH;

    // ── Glossy deep-blue title bar ───────────────────────────────────────
    auto titleBar = juce::Rectangle<int> (0, 0, W, topBarH);
    PixelLookAndFeel::fillGlossSteppedRect (g, titleBar, px, PixelLookAndFeel::deepBlue,
                                            false, false);
    g.setColour (PixelLookAndFeel::navy);
    g.fillRect (0, topBarH - 1, W, 1);

    // ── Frosted strips behind the model & preset bars ────────────────────
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.fillRect (0, topBarH, W, modelBarH + presetBarH);
    g.setColour (PixelLookAndFeel::deepBlue.withAlpha (0.35f));
    g.fillRect (0, topBarH + modelBarH - 1, W, 1);
    g.fillRect (0, topBarH + modelBarH + presetBarH - 1, W, 1);

    // Sparkles on the title bar
    PixelLookAndFeel::drawSparkle (g, (int) (W * 0.62f), topBarH / 2, juce::jmax (1, px / 2), 0.8f);
    PixelLookAndFeel::drawSparkle (g, (int) (W * 0.66f), topBarH / 3, juce::jmax (1, px / 3), 0.5f);

    // Title text with pixel drop shadow
    g.setFont (PixelLookAndFeel::pixelFont ((float) topBarH * 0.48f, true));
    g.setColour (PixelLookAndFeel::navy.withAlpha (0.65f));
    g.drawFittedText ("NEWPROJECT", 9, 1, W / 2, topBarH, juce::Justification::centredLeft, 1);
    g.setColour (juce::Colours::white);
    g.drawFittedText ("NEWPROJECT", 8, 0, W / 2, topBarH, juce::Justification::centredLeft, 1);
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
    const int labelW  = juce::roundToInt (W * 0.08f);
    const int loadW   = juce::roundToInt (W * 0.12f);
    const int statusW = juce::roundToInt (W * 0.09f);
    const int comboW  = W - labelW - loadW - statusW - 20;

    int mx = 4;
    modelBarLabel.setBounds  (mx, mbY, labelW,  mbH);  mx += labelW + 2;
    modelSelector.setBounds  (mx, mbY, comboW,  mbH);  mx += comboW + 4;
    loadModelBtn.setBounds   (mx, mbY, loadW,   mbH);  mx += loadW + 4;
    namStatusLabel.setBounds (mx, mbY, statusW, mbH);

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
