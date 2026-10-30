#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/PixelLookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/Pages.h"

namespace forge
{
/** The whole UI as 1-bit pixel art on a 512 x 320 canvas; the editor zooms it to the window. */
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 512, H = 320;

    explicit MainView (ForgeAmpProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int page);

    void mouseEnter (const juce::MouseEvent&) override { updateStatus(); }
    void mouseMove (const juce::MouseEvent&) override  { updateStatus(); }
    void mouseExit (const juce::MouseEvent&) override  { updateStatus(); }
    void mouseUp (const juce::MouseEvent&) override    { updateStatus(); }

    std::function<void (float)> onScaleRequested;

private:
    void timerCallback() override;
    void showViewMenu();
    void updateStatus();
    juce::String summaryFor (int tile) const;
    juce::Rectangle<int> statusArea() const { return { 0, H - 13, W, 13 }; }
    juce::Rectangle<int> wireArea() const   { return { 43, 44, 426, 11 }; }

    ForgeAmpProcessor& proc;
    PromptOverlay prompt;
    PresetBar presetBar;
    juce::TextButton viewButton { "VIEW" };
    Knob inputKnob, outputKnob;
    LevelMeterView inMeter, outMeter;
    std::array<std::unique_ptr<ChainTile>, 6> tiles;
    PrePage prePage;
    AmpPage ampPage;
    CabPage cabPage;
    FxPage fxPage;
    std::array<Page*, 4> pages {};
    juce::String status;
    int currentPage = 1, tick = 0, wirePhase = 0;
    bool signal = false, cursorOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
} // namespace forge

//==============================================================================
class ForgeAmpEditor : public juce::AudioProcessorEditor
{
public:
    explicit ForgeAmpEditor (ForgeAmpProcessor&);
    ~ForgeAmpEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (forge::pixel::paper); }

    /** 0 = pre (gate + boost), 1 = amp, 2 = cab, 3 = effects. */
    void showPage (int page) { view.showPage (page); }

private:
    ForgeAmpProcessor& proc;
    forge::PixelLookAndFeel lookAndFeel;
    forge::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeAmpEditor)
};
