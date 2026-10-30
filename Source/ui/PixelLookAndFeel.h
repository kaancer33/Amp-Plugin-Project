#pragma once
#include <JuceHeader.h>
#include "Pixel.h"

namespace forge
{
/** Draws JUCE's stock controls (knobs, buttons, popup menus) as 1-bit pixel art. Popup menus
    open inside the pixel canvas, so they get the same crisp scaling as everything else. */
class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PixelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    juce::Component* getParentComponentForMenuOptions (const juce::PopupMenu::Options&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    void drawPopupMenuUpDownArrow (juce::Graphics&, int width, int height, bool isScrollUpArrow) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;
    int getPopupMenuBorderSize() override { return 2; }
    juce::Font getPopupMenuFont() override;
    /** JUCE draws this translucent frame over menus opened inside a component; the menu
        background already has a 1-bit frame. */
    void drawResizableFrame (juce::Graphics&, int, int, const juce::BorderSize<int>&) override {}

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

    /** The face of a pixel button: everything inside the 1 px hard shadow, shifted when pressed. */
    static juce::Rectangle<int> buttonFace (juce::Rectangle<int> bounds, bool down);
};
} // namespace forge
