#pragma once
#include <JuceHeader.h>

class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PixelLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,                  juce::Colours::white);
        setColour (juce::Slider::rotarySliderFillColourId,       juce::Colours::white);
        setColour (juce::Slider::rotarySliderOutlineColourId,    juce::Colours::white);
        setColour (juce::TextButton::buttonColourId,             juce::Colours::black);
        setColour (juce::TextButton::buttonOnColourId,           juce::Colours::white);
        setColour (juce::TextButton::textColourOffId,            juce::Colours::white);
        setColour (juce::TextButton::textColourOnId,             juce::Colours::black);
        setColour (juce::ToggleButton::textColourId,             juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId,             juce::Colours::white);
        setColour (juce::Label::textColourId,                    juce::Colours::white);
        setColour (juce::Label::backgroundColourId,              juce::Colours::black);
        setColour (juce::TextEditor::backgroundColourId,         juce::Colours::black);
        setColour (juce::TextEditor::textColourId,               juce::Colours::white);
        setColour (juce::TextEditor::outlineColourId,            juce::Colours::white);
        setColour (juce::TextEditor::highlightColourId,          juce::Colours::white);
        setColour (juce::TextEditor::highlightedTextColourId,    juce::Colours::black);
        setColour (juce::AlertWindow::backgroundColourId,        juce::Colours::black);
        setColour (juce::AlertWindow::textColourId,              juce::Colours::white);
        setColour (juce::AlertWindow::outlineColourId,           juce::Colours::white);
        setColour (juce::ResizableWindow::backgroundColourId,    juce::Colours::black);
        setColour (juce::PopupMenu::backgroundColourId,          juce::Colours::black);
        setColour (juce::PopupMenu::textColourId,                juce::Colours::white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::white);
        setColour (juce::PopupMenu::highlightedTextColourId,     juce::Colours::black);
        setColour (juce::ComboBox::backgroundColourId,           juce::Colours::black);
        setColour (juce::ComboBox::textColourId,                 juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId,              juce::Colours::white);
        setColour (juce::ComboBox::arrowColourId,                juce::Colours::white);
    }

    static juce::Font pixelFont (float height = 13.0f)
    {
        return juce::Font (juce::FontOptions ("Consolas", height, juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label&) override               { return pixelFont (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return pixelFont ((float)h * 0.45f); }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                                const juce::Colour&, bool highlighted, bool pressed) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced (0.5f);
        bool on = btn.getToggleState();
        g.setColour (on || pressed ? juce::Colours::white : juce::Colours::black);
        g.fillRect (b);
        g.setColour (juce::Colours::white);
        g.drawRect (b, 1.0f);
        if (highlighted && !on && !pressed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (b);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        bool on = btn.getToggleState();
        g.setColour (on ? juce::Colours::black : juce::Colours::white);
        g.setFont (getTextButtonFont (btn, btn.getHeight()));
        g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(),
                           juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& btn,
                            bool, bool) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced (0.5f);
        bool on = btn.getToggleState();
        g.setColour (on ? juce::Colours::white : juce::Colours::black);
        g.fillRect (b);
        g.setColour (juce::Colours::white);
        g.drawRect (b, 1.0f);
        g.setColour (on ? juce::Colours::black : juce::Colours::white);
        g.setFont (pixelFont (b.getHeight() * 0.45f));
        g.drawFittedText (on ? "ON" : "OFF", btn.getLocalBounds(),
                           juce::Justification::centred, 1);
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                            float sliderPos, float startAngle, float endAngle,
                            juce::Slider&) override
    {
        const float cx     = x + w * 0.5f;
        const float cy     = y + h * 0.5f;
        const float radius = juce::jmin (w, h) * 0.38f;

        // Fill
        g.setColour (juce::Colours::black);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Border
        g.setColour (juce::Colours::white);
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // Tick marks (9 positions around the arc)
        for (int i = 0; i < 9; ++i)
        {
            float t     = (float)i / 8.0f;
            float angle = startAngle + t * (endAngle - startAngle);
            float sinA  = std::sin (angle), cosA = std::cos (angle);
            float outer = radius + 5.0f, inner = outer - 3.0f;
            g.drawLine (cx + inner * sinA, cy - inner * cosA,
                         cx + outer * sinA, cy - outer * cosA, 1.5f);
        }

        // Value indicator
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float sinA  = std::sin (angle), cosA = std::cos (angle);
        const float len   = radius * 0.72f;
        g.setColour (juce::Colours::white);
        g.drawLine (cx, cy, cx + len * sinA, cy - len * cosA, 2.5f);

        // Center dot
        g.fillRect (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

    //==========================================================================
    void fillTextEditorBackground (juce::Graphics& g, int, int, juce::TextEditor& te) override
    {
        g.fillAll (juce::Colours::black);
        g.setColour (juce::Colours::white);
        g.drawRect (te.getLocalBounds(), 1);
    }

    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
};
