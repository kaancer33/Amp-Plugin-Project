#include "PixelLookAndFeel.h"

namespace forge
{
using namespace juce;
using namespace pixel;

namespace
{
    /** The pixels of a ring of radius r, sorted clockwise from 12 o'clock, with their angles. */
    const std::vector<std::pair<float, Point<int>>>& ringPixels (int r)
    {
        static std::map<int, std::vector<std::pair<float, Point<int>>>> cache;
        auto& pts = cache[r];
        if (pts.empty())
        {
            auto half = [r] (int dy) { return std::abs (dy) > r ? -1 : discHalfWidth (r, dy); };
            for (int dy = -r; dy <= r; ++dy)
            {
                const int hw = half (dy);
                if (hw < 0)
                    continue;
                const int from = jmin (hw, jmin (half (dy - 1), half (dy + 1)) + 1);
                for (int x = from; x <= hw; ++x)
                    for (int sx : { x, -x })
                    {
                        if (x == 0 && sx != x)
                            continue;
                        float a = std::atan2 ((float) sx, (float) -dy);
                        if (a < 0.0f)
                            a += MathConstants<float>::twoPi;
                        pts.push_back ({ a, { sx, dy } });
                    }
            }
            std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        }
        return pts;
    }

    /** True if angle a (0..2pi from 12 o'clock) lies within [from, to] on the rotary range. */
    bool inRange (float a, float rangeStart, float from, float to)
    {
        while (a < rangeStart)
            a += MathConstants<float>::twoPi;
        return a >= from - 0.001f && a <= to + 0.001f;
    }
}

PixelLookAndFeel::PixelLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, paper);
    setColour (PopupMenu::backgroundColourId, paper);
    setColour (PopupMenu::textColourId, ink);
    setColour (PopupMenu::highlightedBackgroundColourId, ink);
    setColour (PopupMenu::highlightedTextColourId, paper);
}

//==============================================================================
void PixelLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, Slider& slider)
{
    Pix p (g, slider);
    const int size = jmin (width, height);
    const int cx = x + width / 2, cy = y + height / 2;
    const int r = (size - 1) / 2 - 3;
    if (r < 3)
        return;

    const bool bipolar = slider.getProperties()["bipolar"];
    const bool hot = slider.isMouseOverOrDragging();
    const float value = startAngle + pos * (endAngle - startAngle);
    const float originAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float lo = jmin (originAngle, value), hi = jmax (originAngle, value);

    // Value track: dotted over the whole range, solid between the origin and the value.
    // Walk the ring clockwise from the start angle so the dots stay evenly spaced.
    const auto& track = ringPixels (r + 3);
    const float startWrapped = std::fmod (startAngle, MathConstants<float>::twoPi);
    const auto first = (size_t) std::distance (track.begin(), std::lower_bound (track.begin(), track.end(), startWrapped,
                                                                                [] (auto& e, float a) { return e.first < a; }));
    int n = 0;
    for (size_t k = 0; k < track.size(); ++k)
    {
        const auto& [a, pt] = track[(first + k) % track.size()];
        if (! inRange (a, startAngle, startAngle, endAngle))
            continue;
        if (inRange (a, startAngle, lo, hi) || (n & 1) == 0)
            p.dot (cx + pt.x, cy + pt.y);
        ++n;
    }

    // End stops
    for (float a : { startAngle, endAngle })
    {
        const float s = std::sin (a), c = -std::cos (a);
        p.line (cx + roundToInt (s * (float) (r + 2)), cy + roundToInt (c * (float) (r + 2)),
                cx + roundToInt (s * (float) (r + 4)), cy + roundToInt (c * (float) (r + 4)));
    }

    // Body, pointer, hub
    p.disc (cx, cy, r, paper);
    p.ring (cx, cy, r);
    if (hot)
        p.ring (cx, cy, r - 1);

    const float s = std::sin (value), c = -std::cos (value);
    p.line (cx, cy, cx + roundToInt (s * (float) (r - 3)), cy + roundToInt (c * (float) (r - 3)));
    p.fill ({ cx - 1, cy - 1, 3, 3 });
}

//==============================================================================
Rectangle<int> PixelLookAndFeel::buttonFace (Rectangle<int> bounds, bool down)
{
    auto face = bounds.withTrimmedRight (1).withTrimmedBottom (1);
    return down ? face.translated (1, 1) : face;
}

void PixelLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour&, bool highlighted, bool down)
{
    Pix p (g, button);
    const auto bounds = button.getLocalBounds();
    const bool on = button.getToggleState();
    const bool primary = button.getProperties()["primary"];
    const auto face = buttonFace (bounds, down);

    if (! down)
    {
        p.hline (bounds.getX() + 1, bounds.getRight() - 1, bounds.getBottom() - 1);
        p.vline (bounds.getRight() - 1, bounds.getY() + 1, bounds.getBottom() - 1);
    }

    p.fill (face, on ? ink : paper);
    p.frame (face);
    if (primary)
        p.frame (face.reduced (2), on ? paper : ink);
    else if (highlighted && button.isEnabled())
        p.frame (face.reduced (1), on ? paper : ink);
}

void PixelLookAndFeel::drawButtonText (Graphics& g, TextButton& button, bool, bool down)
{
    Pix p (g, button);
    const auto face = buttonFace (button.getLocalBounds(), down);
    p.text (button.getButtonText(), face.withTrimmedTop (1), Justification::centred, button.getToggleState() ? paper : ink);
    if (! button.isEnabled())
        p.grey (face.reduced (1));
}

//==============================================================================
Component* PixelLookAndFeel::getParentComponentForMenuOptions (const PopupMenu::Options& options)
{
    if (auto* parent = options.getParentComponent())
        return parent;
    if (auto* canvas = findCanvas (options.getTargetComponent()))
        return canvas;
    return LookAndFeel_V4::getParentComponentForMenuOptions (options);
}

void PixelLookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    Pix p (g);
    p.fill ({ 0, 0, width, height }, paper);
    p.frame ({ 0, 0, width, height });
}

void PixelLookAndFeel::getIdealPopupMenuItemSize (const String& text, bool isSeparator, int,
                                                  int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 20;
        idealHeight = 5;
        return;
    }
    idealHeight = 11;
    idealWidth = pixel::font().width (text) + 24;
}

void PixelLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                                          bool isHighlighted, bool isTicked, bool hasSubMenu, const String& text,
                                          const String&, const Drawable*, const Colour*)
{
    Pix p (g);
    if (isSeparator)
    {
        p.dottedH (area.getX() + 2, area.getRight() - 3, area.getCentreY());
        return;
    }

    const bool hi = isHighlighted && isActive;
    if (hi)
        p.fill (area, ink);
    const auto c = hi ? paper : ink;
    const int ty = area.getY() + (area.getHeight() - 7) / 2;

    if (isTicked)
        p.text (String::charToString ((juce_wchar) 0x2713), area.getX() + 3, ty, c);
    p.text (pixel::font().fit (text, area.getWidth() - 22), area.getX() + 12, ty, c);
    if (hasSubMenu)
        p.triangle ({ area.getRight() - 8, ty, 4, 7 }, 'r', c);
    if (! isActive)
        p.grey (area);
}

void PixelLookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name)
{
    Pix p (g);
    const int ty = area.getY() + (area.getHeight() - 5) / 2 + 1;
    const int w = p.text (name.toUpperCase(), area.getX() + 3, ty, ink, tiny());
    p.dottedH (area.getX() + w + 6, area.getRight() - 4, ty + 2);
}

void PixelLookAndFeel::drawPopupMenuUpDownArrow (Graphics& g, int width, int height, bool isScrollUpArrow)
{
    Pix p (g);
    p.fill ({ 0, 0, width, height }, paper);
    p.triangle (Rectangle<int> (7, 4).withCentre ({ width / 2, height / 2 }), isScrollUpArrow ? 'u' : 'd');
}

Font PixelLookAndFeel::getPopupMenuFont()
{
    return Font (FontOptions (11.0f));
}

//==============================================================================
void PixelLookAndFeel::drawCornerResizer (Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    // Lives outside the canvas (in window pixels), so draw whole squares sized to the window.
    const float unit = jmax (2.0f, (float) jmin (w, h) / 9.0f);
    juce::ignoreUnused (isMouseOver, isMouseDragging);
    g.setColour (ink);
    for (int d = 1; d <= 3; ++d)
        for (int k = 0; k < d; ++k)
            g.fillRect (Rectangle<float> ((float) w - unit * (float) (2 * (d - k)),
                                          (float) h - unit * (float) (2 * (k + 1)), unit, unit).getSmallestIntegerContainer());
}
} // namespace forge
