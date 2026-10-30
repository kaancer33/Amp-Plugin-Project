#pragma once
#include <JuceHeader.h>
#include "Pixel.h"
#include "../PresetManager.h"

namespace forge
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Height of a window title bar, including its bottom rule. */
constexpr int kTitleBarH = 13;

/** Popup menu options that open the menu inside the pixel canvas, next to the target. JUCE
    decides a menu's parent with the default look-and-feel, so this has to be explicit. */
juce::PopupMenu::Options pixelMenuOptions (juce::Component& target);

/** A classic window: 1 px frame, striped title bar with the title knocked out, optional status on
    the right. titleX leaves room for a power box at the left of the bar. */
void drawWindow (pixel::Pix&, juce::Rectangle<int>, const juce::String& title,
                 const juce::String& status = {}, int titleX = 5);

//==============================================================================
/** Single-line text entry drawn with the bitmap font (blinking caret, select-all on open). */
class PixelTextField : public juce::Component, private juce::Timer
{
public:
    PixelTextField();

    void setText (const juce::String&, bool selectAll = true);
    const juce::String& getText() const noexcept { return text; }

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onReturn, onEscape, onFocusLost;
    int maxLength = 48;

private:
    void timerCallback() override;
    void insert (const juce::String&);

    juce::String text;
    int caret = 0;
    bool allSelected = false, caretOn = true;
};

//==============================================================================
/** Rotary control: name above, live value below. Drag (Shift for fine), wheel, double-click
    the knob to reset, double-click the value to type one. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& label,
          const juce::String& help = {}, bool bipolar = false, int radius = 12, bool smallLabel = false);

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void setDimmed (bool shouldDim);

    /** Height this knob needs for its radius: label, dial, value. */
    static int heightFor (int radius) { return 9 + (radius + 3) * 2 + 1 + 11; }

private:
    struct FineSlider : juce::Slider
    {
        void mouseDown (const juce::MouseEvent& e) override
        {
            setMouseDragSensitivity (e.mods.isShiftDown() ? 600 : 120);
            juce::Slider::mouseDown (e);
        }
        void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); if (auto* p = getParentComponent()) p->repaint(); }
        void mouseExit (const juce::MouseEvent& e) override  { juce::Slider::mouseExit (e);  if (auto* p = getParentComponent()) p->repaint(); }
    };

    struct Readout : juce::Component, juce::SettableTooltipClient
    {
        explicit Readout (Knob& k) : owner (k) { setTooltip ("Double-click to type a value"); }
        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override { owner.beginEdit(); }
        Knob& owner;
    };

    void beginEdit();
    void endEdit (bool commit);
    juce::Rectangle<int> readoutArea() const;

    juce::String name;
    int radius;
    bool smallLabel, dimmed = false;
    FineSlider slider;
    Readout readout { *this };
    PixelTextField editor;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** The ON/OFF box from the original design. Small uses the 3x5 font, large the 5x7. */
class PowerBox : public juce::Button
{
public:
    explicit PowerBox (bool large = false);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setInverted (bool shouldInvert) { if (inverted != shouldInvert) { inverted = shouldInvert; repaint(); } }

    static juce::Rectangle<int> sizeFor (bool large) { return large ? juce::Rectangle<int> (25, 13) : juce::Rectangle<int> (15, 9); }

private:
    bool large, inverted = false;
};

/** [x] LABEL check box. */
class CheckToggle : public juce::Button
{
public:
    explicit CheckToggle (const juce::String& text);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    int idealWidth() const { return 10 + pixel::font().width (getButtonText()); }
};

//==============================================================================
/** Choice parameter as tabs (horizontal) or a list with a selection bar (vertical). */
class ChoiceSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSelector (APVTS& state, const juce::String& paramId, bool vertical);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    int getSelected() const noexcept { return selected; }
    std::function<juce::String (int)> itemNote;
    static constexpr int rowHeight = 12;

private:
    int indexAt (juce::Point<int>) const;
    juce::Rectangle<int> itemBounds (int index) const;

    juce::StringArray items;
    bool vertical;
    int selected = 0, hovered = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
/** Segmented peak meter with a dB scale, peak hold and a CLIP latch (click to clear). */
class LevelMeterView : public juce::Component, public juce::SettableTooltipClient
{
public:
    void pushLevel (float linearPeak);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clipped = false; repaint(); }

private:
    float displayDb = -90.0f, holdDb = -90.0f;
    int holdFrames = 0, lastLit = -1, lastHold = -1;
    bool clipped = false, lastClipped = false;
};

//==============================================================================
/** Scrolling input level against the gate threshold; closed stretches are dithered. */
class GateScope : public juce::Component
{
public:
    void push (float inputPeak, float gateGain, float thresholdDb, bool enabled);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> plotArea() const { return getLocalBounds().reduced (2).withTrimmedTop (8); }

    std::vector<float> levelDb, gain;
    int writePos = 0;
    float threshold = -60.0f;
    bool on = true;
};

//==============================================================================
/** Log-frequency response plot in a box. */
class ResponseCurveView : public juce::Component
{
public:
    using Compute = std::function<void (const double* freqs, double* outDb, int n)>;

    ResponseCurveView (const juce::String& title, float minDb, float maxDb, float minHz = 30.0f, float maxHz = 16000.0f);

    void setCompute (Compute c) { compute = std::move (c); refresh(); }
    void setNote (const juce::String& n) { if (n != note) { note = n; repaint(); } }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override { refresh(); }

private:
    juce::Rectangle<int> plotArea() const;
    int xForHz (float hz, juce::Rectangle<int>) const;
    int yForDb (float db, juce::Rectangle<int>) const;

    juce::String title, note;
    float minDb, maxDb, minHz, maxHz;
    Compute compute;
    std::vector<double> freqs, dbs;
};

//==============================================================================
/** A window with an ON box in its title bar (when bound to a power parameter). */
class Card : public juce::Component
{
public:
    explicit Card (const juce::String& title, APVTS* state = nullptr, const juce::String& powerParam = {});

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Rectangle<int> getBody() const { return getLocalBounds().reduced (1).withTrimmedTop (kTitleBarH - 1).reduced (3, 2); }
    void setStatus (const juce::String& s) { if (s != status) { status = s; repaint (getLocalBounds().removeFromTop (kTitleBarH)); } }
    bool isOn() const { return power == nullptr || power->getToggleState(); }

private:
    juce::String title, status;
    std::unique_ptr<PowerBox> power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
};

//==============================================================================
enum class Icon { gate, boost, amp, cab, delay, reverb, anvil };
/** 16x16 pixel icons (the anvil logo is 16x12). */
void drawIcon (pixel::Pix&, Icon, int x, int y, juce::Colour = pixel::ink);

/** One module in the signal-chain strip: icon, name, live summary and ON box. Click to open
    its page; the current page's tile is shown inverted. */
class ChainTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChainTile (const juce::String& name, Icon, APVTS&, const juce::String& powerParam);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void setSelected (bool s) { if (s != selected) { selected = s; power.setInverted (s); repaint(); } }
    void setSummary (const juce::String& s) { if (s != summary) { summary = s; repaint(); } }
    void refresh() { if (power.getToggleState() != lastOn) { lastOn = power.getToggleState(); repaint(); } }
    bool isOn() const { return power.getToggleState(); }

    std::function<void()> onSelect;

private:
    juce::String name, summary;
    Icon icon;
    bool selected = false, lastOn = false;
    PowerBox power;
    APVTS::ButtonAttachment attachment;
};

//==============================================================================
/** In-window modal dialog, drawn like a classic alert box over a dithered backdrop. */
class PromptOverlay : public juce::Component
{
public:
    PromptOverlay();

    void show (const juce::String& title, const juce::String& message, bool withText,
               const juce::String& initialText, const juce::String& okLabel,
               std::function<void (const juce::String&)> onOk, bool showCancel = true);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}
    bool keyPressed (const juce::KeyPress&) override;

private:
    void close();
    juce::Rectangle<int> panelBounds() const;

    juce::String title, message;
    bool hasText = false;
    PixelTextField field;
    juce::TextButton ok, cancel { "CANCEL" };
    std::function<void (const juce::String&)> callback;
};

//==============================================================================
/** Preset browser: previous / next, the name display (opens the list), save, and a menu. */
class PresetBar : public juce::Component
{
public:
    PresetBar (PresetManager& manager, PromptOverlay& prompt);

    void resized() override;
    void refresh();

private:
    void showPresetMenu();
    void showActionsMenu();
    void save();

    struct NameDisplay : juce::Button
    {
        NameDisplay() : juce::Button ("preset") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        juce::String presetName;
        int number = 0;
        bool dirty = false;
    };

    struct ArrowButton : juce::Button
    {
        explicit ArrowButton (bool isNext) : juce::Button (isNext ? "next" : "previous"), right (isNext) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool right;
    };

    PresetManager& presets;
    PromptOverlay& prompt;
    ArrowButton prev { false }, next { true };
    NameDisplay nameButton;
    juce::TextButton saveButton { "SAVE" }, menuButton { juce::String::charToString ((juce::juce_wchar) 0x2261) };
};
} // namespace forge
