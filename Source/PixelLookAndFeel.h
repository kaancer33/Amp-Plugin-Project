#pragma once
#include <JuceHeader.h>

//==============================================================================
//  "Pixel Aero" look & feel
//
//  Early-2000s Frutiger Aero rendered as pixel art: hard-edged blocks and
//  ordered dithering instead of smooth gradients, glossy aqua orbs, capsule
//  buttons and frosted-glass panels floating over a dithered sky.
//
//  Everything is drawn on a virtual pixel grid (px = one chunky screen
//  "pixel") so the style survives the S/M/L window sizes.
//==============================================================================
class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ── Palette ──────────────────────────────────────────────────────────
    static inline const juce::Colour skyTop      { 0xffd2f1ff };
    static inline const juce::Colour skyBottom   { 0xff45a9e4 };
    static inline const juce::Colour glossBlue   { 0xff2e86d9 };
    static inline const juce::Colour glossBlueLt { 0xff8fd0f7 };
    static inline const juce::Colour deepBlue    { 0xff0f4c9e };
    static inline const juce::Colour navy        { 0xff0a3055 };
    static inline const juce::Colour aquaGreen   { 0xff53bd25 };
    static inline const juce::Colour greyOff     { 0xff8fa9bd };
    static inline const juce::Colour statusGreen { 0xff128a12 };
    static inline const juce::Colour statusAmber { 0xffc98a00 };
    static inline const juce::Colour statusRed   { 0xffc62828 };

    PixelLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId,                    juce::Colours::white);
        setColour (juce::Slider::rotarySliderFillColourId,         glossBlue);
        setColour (juce::Slider::rotarySliderOutlineColourId,      deepBlue);
        setColour (juce::TextButton::buttonColourId,               glossBlue);
        setColour (juce::TextButton::buttonOnColourId,             aquaGreen);
        setColour (juce::TextButton::textColourOffId,              juce::Colours::white);
        setColour (juce::TextButton::textColourOnId,               juce::Colours::white);
        setColour (juce::ToggleButton::textColourId,               juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId,               juce::Colours::white);
        setColour (juce::Label::textColourId,                      navy);
        setColour (juce::Label::backgroundColourId,                juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId,           juce::Colours::white);
        setColour (juce::TextEditor::textColourId,                 navy);
        setColour (juce::TextEditor::outlineColourId,              glossBlue);
        setColour (juce::TextEditor::highlightColourId,            glossBlueLt);
        setColour (juce::TextEditor::highlightedTextColourId,      navy);
        setColour (juce::AlertWindow::backgroundColourId,          juce::Colour (0xffeaf6ff));
        setColour (juce::AlertWindow::textColourId,                navy);
        setColour (juce::AlertWindow::outlineColourId,             glossBlue);
        setColour (juce::ResizableWindow::backgroundColourId,      skyBottom);
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xfff2faff));
        setColour (juce::PopupMenu::textColourId,                  navy);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, glossBlue);
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId,             juce::Colours::transparentWhite);
        setColour (juce::ComboBox::textColourId,                   navy);
        setColour (juce::ComboBox::outlineColourId,                glossBlue);
        setColour (juce::ComboBox::arrowColourId,                  juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId,        juce::Colours::white);
        setColour (juce::TooltipWindow::textColourId,              navy);
        setColour (juce::TooltipWindow::outlineColourId,           glossBlue);
    }

    static juce::Font pixelFont (float height = 13.0f, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Consolas", height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return pixelFont (juce::jlimit (11.0f, 19.0f, (float) l.getHeight() * 0.50f), true);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return pixelFont ((float) h * 0.45f, true); }
    juce::Font getComboBoxFont (juce::ComboBox& box) override        { return pixelFont ((float) box.getHeight() * 0.5f); }
    juce::Font getPopupMenuFont() override                           { return pixelFont (14.0f); }

    //==========================================================================
    //  Pixel-art drawing helpers (public so the editor can reuse them)
    //==========================================================================

    // Vertical banded gradient with a checkerboard-dithered row at each seam.
    static void fillDitheredV (juce::Graphics& g, juce::Rectangle<int> area,
                               juce::Colour top, juce::Colour bottom,
                               int bands, int px)
    {
        if (area.isEmpty()) return;
        bands = juce::jlimit (2, juce::jmax (2, area.getHeight() / juce::jmax (1, px)), bands);
        const float bandH = (float) area.getHeight() / (float) bands;

        for (int i = 0; i < bands; ++i)
        {
            const int y0 = area.getY() + (int) (i * bandH);
            const int y1 = (i == bands - 1) ? area.getBottom()
                                            : area.getY() + (int) ((i + 1) * bandH);
            g.setColour (top.interpolatedWith (bottom, (float) i / (float) (bands - 1)));
            g.fillRect (area.getX(), y0, area.getWidth(), y1 - y0);
        }

        for (int i = 1; i < bands; ++i)
        {
            const int ySeam  = area.getY() + (int) (i * bandH);
            const auto above = top.interpolatedWith (bottom, (float) (i - 1) / (float) (bands - 1));
            const auto below = top.interpolatedWith (bottom, (float) i / (float) (bands - 1));

            for (int bx = area.getX(); bx < area.getRight(); bx += px)
            {
                const int bw = juce::jmin (px, area.getRight() - bx);
                if (((bx / px) & 1) != 0)
                {
                    g.setColour (below);
                    g.fillRect (bx, ySeam - px, bw, px);
                }
                else
                {
                    g.setColour (above);
                    g.fillRect (bx, ySeam, bw, juce::jmin (px, area.getBottom() - ySeam));
                }
            }
        }
    }

    // Flat filled circle built from px-sized scanline blocks (stepped edge).
    static void fillPixelCircleFlat (juce::Graphics& g, float cxF, float cyF, float rF,
                                     juce::Colour c, int px)
    {
        const int cx = (int) cxF, cy = (int) cyF, r = juce::jmax (px, (int) rF);
        const int rp = juce::jmax (1, r / px);
        g.setColour (c);
        for (int iy = -rp; iy < rp; ++iy)
        {
            const float yMid = (iy + 0.5f) * (float) px;
            const float span = std::sqrt (juce::jmax (0.0f, (float) r * (float) r - yMid * yMid));
            const int hw = juce::jmax (1, (int) span / px);
            g.fillRect (cx - hw * px, cy + iy * px, hw * 2 * px, px);
        }
    }

    // Vertically banded + dithered circle (the glossy orb body).
    static void fillPixelCircle (juce::Graphics& g, float cxF, float cyF, float rF,
                                 juce::Colour top, juce::Colour bottom,
                                 int bands, int px)
    {
        const int cx = (int) cxF, cy = (int) cyF, r = juce::jmax (px, (int) rF);
        const int rp = juce::jmax (1, r / px);
        bands = juce::jmax (2, bands);

        for (int iy = -rp; iy < rp; ++iy)
        {
            const float yMid = (iy + 0.5f) * (float) px;
            const float span = std::sqrt (juce::jmax (0.0f, (float) r * (float) r - yMid * yMid));
            const int hw = juce::jmax (1, (int) span / px);

            const float t     = (float) (iy + rp) / (float) (2 * rp - 1);
            const float bandF = t * (float) (bands - 1);
            const int   band  = (int) bandF;
            const float frac  = bandF - (float) band;

            const auto cA = top.interpolatedWith (bottom, (float) band / (float) (bands - 1));
            const auto cB = top.interpolatedWith (bottom, (float) juce::jmin (band + 1, bands - 1) / (float) (bands - 1));

            g.setColour (cA);
            g.fillRect (cx - hw * px, cy + iy * px, hw * 2 * px, px);

            if (frac > 0.30f)
            {
                g.setColour (cB);
                for (int ix = -hw; ix < hw; ++ix)
                {
                    const bool checker = (((ix ^ iy) & 1) != 0);
                    if (frac > 0.75f || checker)
                        g.fillRect (cx + ix * px, cy + iy * px, px, px);
                }
            }
        }
    }

    // Quantised white gloss cap over the top of an orb.
    static void glossCap (juce::Graphics& g, float cxF, float cyF, float rF, int px)
    {
        const int cx = (int) cxF, cy = (int) cyF, r = juce::jmax (px, (int) rF);
        const int rp = juce::jmax (1, r / px);

        for (int iy = -rp; iy < 0; ++iy)
        {
            const float t = (float) (iy + rp) / (float) rp;    // 0 top → 1 equator
            if (t > 0.80f) break;
            const float yMid = (iy + 0.5f) * (float) px;
            const float span = std::sqrt (juce::jmax (0.0f, (float) r * (float) r - yMid * yMid));
            const int hw = juce::jmax (1, (int) span / px);
            const float a = t < 0.30f ? 0.45f : (t < 0.55f ? 0.26f : 0.12f);
            g.setColour (juce::Colours::white.withAlpha (a));
            g.fillRect (cx - hw * px, cy + iy * px, hw * 2 * px, px);
        }
    }

    static int cornerInset (int mirrorRow, int rows, int px)
    {
        if (rows >= 8) return mirrorRow == 0 ? 2 * px : (mirrorRow == 1 ? px : 0);
        return mirrorRow == 0 ? px : 0;
    }

    // Flat rectangle with stepped (pixel-rounded) corners.
    static void fillSteppedRect (juce::Graphics& g, juce::Rectangle<int> b, int px,
                                 juce::Colour c, bool roundTop = true, bool roundBottom = true)
    {
        if (b.isEmpty()) return;
        const int rows = juce::jmax (1, b.getHeight() / px);
        g.setColour (c);
        for (int row = 0; row < rows; ++row)
        {
            const int y  = b.getY() + row * px;
            const int hh = (row == rows - 1) ? b.getBottom() - y : px;
            const int mirrorTop = row, mirrorBot = rows - 1 - row;
            int inset = 0;
            if (roundTop    && mirrorTop <= mirrorBot) inset = cornerInset (mirrorTop, rows, px);
            if (roundBottom && mirrorBot <  mirrorTop) inset = cornerInset (mirrorBot, rows, px);
            g.fillRect (b.getX() + inset, y, b.getWidth() - inset * 2, hh);
        }
    }

    // Glossy aqua capsule: bright dithered top half, dark horizon, subtle
    // bottom bloom — all quantised into chunky bands.
    static void fillGlossSteppedRect (juce::Graphics& g, juce::Rectangle<int> b, int px,
                                      juce::Colour base, bool roundTop = true, bool roundBottom = true)
    {
        if (b.isEmpty()) return;
        const int rows = juce::jmax (1, b.getHeight() / px);

        for (int row = 0; row < rows; ++row)
        {
            const int y  = b.getY() + row * px;
            const int hh = (row == rows - 1) ? b.getBottom() - y : px;
            const int mirrorTop = row, mirrorBot = rows - 1 - row;
            int inset = 0;
            if (roundTop    && mirrorTop <= mirrorBot) inset = cornerInset (mirrorTop, rows, px);
            if (roundBottom && mirrorBot <  mirrorTop) inset = cornerInset (mirrorBot, rows, px);

            const float t  = rows > 1 ? (float) row / (float) (rows - 1) : 0.0f;
            const float tq = std::floor (t * 6.0f) / 6.0f;
            juce::Colour c = tq < 0.5f
                ? base.brighter (0.60f).interpolatedWith (base, tq * 2.0f)
                : base.darker (0.32f).interpolatedWith (base, (tq - 0.5f) * 0.9f);
            g.setColour (c);
            g.fillRect (b.getX() + inset, y, b.getWidth() - inset * 2, hh);

            if (row == 0)   // crisp top highlight
            {
                g.setColour (juce::Colours::white.withAlpha (0.50f));
                g.fillRect (b.getX() + inset + px, y, b.getWidth() - (inset + px) * 2, hh);
            }
        }
    }

    // Bordered glossy button body.
    static void drawGlossyPixelButton (juce::Graphics& g, juce::Rectangle<int> b,
                                       juce::Colour base, int px, bool pressed)
    {
        if (pressed) base = base.darker (0.22f);
        fillSteppedRect (g, b, px, navy.withAlpha (0.75f));
        fillGlossSteppedRect (g, b.reduced (juce::jmax (1, px / 2) + 1), px, base);
    }

    // Classic aero "plus" sparkle.
    static void drawSparkle (juce::Graphics& g, int cx, int cy, int px, float alpha)
    {
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillRect (cx - px / 2, cy - px * 3, px, px * 6);
        g.fillRect (cx - px * 3, cy - px / 2, px * 6, px);
        g.setColour (juce::Colours::white.withAlpha (juce::jmin (1.0f, alpha + 0.3f)));
        g.fillRect (cx - px / 2, cy - px / 2, px, px);
    }

    // Blocky pixel cloud (flat-bottomed).
    static void drawPixelCloud (juce::Graphics& g, int x, int y, int s, float alpha)
    {
        static const int rows[5][2] = { {5, 6}, {3, 10}, {1, 14}, {0, 16}, {2, 12} };
        g.setColour (juce::Colours::white.withAlpha (alpha));
        for (int i = 0; i < 5; ++i)
            g.fillRect (x + rows[i][0] * s, y + i * s, rows[i][1] * s, s);
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour&, bool highlighted, bool pressed) override
    {
        auto base = btn.getToggleState() ? aquaGreen : glossBlue;
        if (! btn.isEnabled())            base = greyOff;
        else if (highlighted && !pressed) base = base.brighter (0.15f);
        drawGlossyPixelButton (g, btn.getLocalBounds(), base, 2, pressed);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        auto b = btn.getLocalBounds();
        g.setFont (getTextButtonFont (btn, btn.getHeight()));
        g.setColour (navy.withAlpha (btn.isEnabled() ? 0.55f : 0.30f));
        g.drawFittedText (btn.getButtonText(), b.translated (1, 1), juce::Justification::centred, 1);
        g.setColour (juce::Colours::white.withAlpha (btn.isEnabled() ? 1.0f : 0.75f));
        g.drawFittedText (btn.getButtonText(), b, juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& btn,
                           bool highlighted, bool pressed) override
    {
        const bool on = btn.getToggleState();
        auto base = on ? aquaGreen : greyOff;
        if (highlighted && !pressed) base = base.brighter (0.15f);
        drawGlossyPixelButton (g, btn.getLocalBounds(), base, 2, pressed);

        auto b = btn.getLocalBounds();
        g.setFont (pixelFont ((float) b.getHeight() * 0.45f, true));
        g.setColour (navy.withAlpha (0.55f));
        g.drawFittedText (on ? "ON" : "OFF", b.translated (1, 1), juce::Justification::centred, 1);
        g.setColour (juce::Colours::white);
        g.drawFittedText (on ? "ON" : "OFF", b, juce::Justification::centred, 1);
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const float cx     = (float) x + (float) w * 0.5f;
        const float cy     = (float) y + (float) h * 0.5f;
        const float radius = juce::jmin ((float) w, (float) h) * 0.34f;
        const int   px     = juce::jmax (2, (int) (radius / 14.0f));

        // Drop shadow
        fillPixelCircleFlat (g, cx, cy + (float) px * 1.5f, radius,
                             navy.withAlpha (0.18f), px);

        // White bezel ring + aqua orb body
        fillPixelCircleFlat (g, cx, cy, radius, juce::Colours::white, px);
        fillPixelCircle (g, cx, cy, radius - (float) px,
                         juce::Colour (0xffc4ecff), deepBlue, 6, px);

        // Gloss cap
        glossCap (g, cx, cy - (float) px, (radius - (float) px) * 0.92f, px);

        // Tick blocks around the rim — lit up to the current value
        const int nTicks = 13;
        for (int i = 0; i < nTicks; ++i)
        {
            const float t     = (float) i / (float) (nTicks - 1);
            const float angle = startAngle + t * (endAngle - startAngle);
            const float rOut  = radius + (float) px * 2.2f;
            const float tx    = cx + rOut * std::sin (angle);
            const float ty    = cy - rOut * std::cos (angle);
            const bool  lit   = t <= sliderPos + 0.001f;
            const int   ts    = juce::jmax (2, (px * 3) / 2);
            g.setColour (lit ? aquaGreen : juce::Colours::white.withAlpha (0.60f));
            g.fillRect ((int) tx - ts / 2, (int) ty - ts / 2, ts, ts);
        }

        // Chunky pointer
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float sinA = std::sin (angle), cosA = std::cos (angle);
        for (float rr = radius * 0.28f; rr < radius * 0.74f; rr += (float) px * 0.5f)
        {
            g.setColour (navy);
            g.fillRect ((int) (cx + rr * sinA) - px, (int) (cy - rr * cosA) - px, px * 2, px * 2);
        }
        for (float rr = radius * 0.30f; rr < radius * 0.66f; rr += (float) px * 0.5f)
        {
            g.setColour (juce::Colours::white);
            g.fillRect ((int) (cx + rr * sinA) - px / 2, (int) (cy - rr * cosA) - px / 2, px, px);
        }

        // Hub
        fillPixelCircleFlat (g, cx, cy, radius * 0.16f + (float) px, navy, px);
        fillPixelCircleFlat (g, cx, cy, radius * 0.12f, juce::Colours::white, px);
    }

    //==========================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override
    {
        const int px = 2;
        auto b = juce::Rectangle<int> (0, 0, width, height);

        fillSteppedRect (g, b, px, deepBlue.withAlpha (0.85f));
        fillSteppedRect (g, b.reduced (2), px, juce::Colours::white.withAlpha (0.90f));

        auto az = juce::Rectangle<int> (buttonX, buttonY, buttonW, buttonH).reduced (3);
        fillGlossSteppedRect (g, az, px, isButtonDown ? glossBlue.darker (0.2f) : glossBlue);

        const int acx = az.getCentreX(), acy = az.getCentreY();
        g.setColour (juce::Colours::white);
        g.fillRect (acx - (5 * px) / 2, acy - px, 5 * px, px);
        g.fillRect (acx - (3 * px) / 2, acy,      3 * px, px);
        g.fillRect (acx - px / 2,       acy + px, px,     px);
    }

    //==========================================================================
    void fillTextEditorBackground (juce::Graphics& g, int, int, juce::TextEditor& te) override
    {
        g.fillAll (juce::Colours::white);
        g.setColour (glossBlue);
        g.drawRect (te.getLocalBounds(), 1);
    }

    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
};
