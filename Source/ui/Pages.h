#pragma once
#include "Widgets.h"

class ForgeAmpProcessor;

namespace forge
{
/** A page of the editor. refresh() is called from the editor's UI timer (~30 Hz). */
class Page : public juce::Component
{
public:
    virtual void refresh() {}
    /** Called with each new input-meter reading (pre-gate peak since the last tick). */
    virtual void pushInputLevel (float) {}
};

/** Word-wrapped text in the 5x7 (or 3x5) font. */
struct TextBlock : juce::Component
{
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint (juce::Graphics&) override;
    juce::String text;
    bool small = false, centred = false;
};

//==============================================================================
class PrePage : public Page
{
public:
    explicit PrePage (ForgeAmpProcessor&);
    void resized() override;
    void refresh() override;
    void pushInputLevel (float peak) override;

private:
    ForgeAmpProcessor& proc;
    Card gateCard, boostCard;
    Knob gateThreshold, gateRelease;
    Knob boostDrive, boostTone, boostLevel;
    GateScope scope;
    ResponseCurveView boostCurve;
    TextBlock gateHint, boostHint;
    float lastBoost[3] { -1.0f, -1.0f, -1.0f };
};

//==============================================================================
class AmpPage : public Page
{
public:
    explicit AmpPage (ForgeAmpProcessor&);
    void resized() override;
    void refresh() override;

private:
    /** The amp's control panel: notched frame, screws, vent, logo plate, jewel light. */
    struct Faceplate : juce::Component
    {
        void paint (juce::Graphics&) override;
        juce::String voiceName;
        bool on = true;
    };

    ForgeAmpProcessor& proc;
    ChoiceSelector voices;
    TextBlock description;
    Faceplate plate;
    PowerBox power { true };
    APVTS::ButtonAttachment powerAttachment;
    Knob gain, bass, mid, treble, presence, depth, master;
    Card toneCard;
    ResponseCurveView toneCurve;
    std::array<float, 7> lastTone {};
    int lastVoice = -1;
};

//==============================================================================
class CabPage : public Page
{
public:
    CabPage (ForgeAmpProcessor&, PromptOverlay&);
    void resized() override;
    void refresh() override;

private:
    /** Pixel drawing of the selected cabinet (or a floppy disk for a user IR). */
    struct CabArt : juce::Component
    {
        void paint (juce::Graphics&) override;
        int model = 0;
        bool irLoaded = false;
    };

    /** One speaker seen from the front, with a crosshair where the mic points. */
    struct MicArt : juce::Component
    {
        void paint (juce::Graphics&) override;
        float position = 0.3f;   // 0 = dust cap, 1 = cone edge
    };

    void chooseImpulseResponse();

    ForgeAmpProcessor& proc;
    PromptOverlay& prompt;
    Card cabCard, responseCard, micCard;
    ChoiceSelector models;
    CabArt art;
    juce::TextButton loadIr { "LOAD IR" }, clearIr { "CLEAR" };
    TextBlock irInfo;
    MicArt micArt;
    Knob mic, lowCut, highCut;
    ResponseCurveView cabCurve;
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<float, 4> lastCab {};
    juce::String lastIrPath;
    bool lastIrLoaded = false;
};

//==============================================================================
class FxPage : public Page
{
public:
    explicit FxPage (ForgeAmpProcessor&);
    void resized() override;
    void refresh() override;

private:
    /** The dry hit and the following repeats, with ping-pong shown as left/right. */
    struct EchoView : juce::Component
    {
        void paint (juce::Graphics&) override;
        float timeMs = 375.0f, feedback = 0.35f, mix = 0.25f;
        bool pingPong = false, on = true;
    };

    /** Pre-delay gap, early reflections and the decay envelope out to the RT60 marker. */
    struct DecayView : juce::Component
    {
        void paint (juce::Graphics&) override;
        float size = 0.5f, decay = 2.0f, preDelayMs = 15.0f, tone = 0.5f, mix = 0.2f;
        bool on = true;
    };

    ForgeAmpProcessor& proc;
    Card delayCard, reverbCard;
    CheckToggle sync { "SYNC" }, pingPong { "PING-PONG" };
    APVTS::ButtonAttachment syncAttachment, pingPongAttachment;
    EchoView echo;
    DecayView decay;
    Knob delayTime, delayDivision, delayFeedback, delayTone, delayMix;
    Knob reverbSize, reverbDecay, reverbPreDelay, reverbTone, reverbMix;
    TextBlock delayInfo, reverbInfo;
    std::array<float, 11> lastFx {};
};
} // namespace forge
