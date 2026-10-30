#pragma once
#include <JuceHeader.h>

/** 1-bit pixel art: bitmap fonts, dither patterns, sprites, and a canvas cache that renders a
    component tree at its own low resolution and scales it up with nearest neighbour.

    Everything inside the canvas is laid out and drawn in whole "art pixels". Nothing uses
    anti-aliased paths or system fonts, so every edge lands on the pixel grid at any zoom. */
namespace forge::pixel
{
inline const juce::Colour ink   { 0xffffffff };
inline const juce::Colour paper { 0xff000000 };

//==============================================================================
/** A bitmap font. Each glyph is written as rows of '#' (on) and '.' (off) separated by spaces;
    rows 0..ascent-1 sit above the baseline and any extra rows are descenders. */
class BitmapFont
{
public:
    struct Glyph
    {
        int width = 0, rows = 0;
        std::array<uint16_t, 9> bits {};
    };

    using Definition = std::pair<juce::juce_wchar, const char*>;

    BitmapFont (int ascent, int lineHeight, bool upperCaseOnly,
                std::initializer_list<Definition> definitions, const char* fallback);

    const Glyph& glyph (juce::juce_wchar) const;
    int width (const juce::String&, int scale = 1) const;

    /** Shortens the text with ".." until it fits. */
    juce::String fit (const juce::String&, int maxWidth) const;

    /** Word-wraps the text into lines no wider than maxWidth. */
    juce::StringArray wrap (const juce::String&, int maxWidth) const;

    const int ascent, lineHeight;
    static constexpr int spacing = 1;

private:
    static Glyph parse (const char*);

    std::unordered_map<juce::juce_wchar, Glyph> glyphs;
    Glyph fallbackGlyph;
    bool upperOnly;
};

/** 5x7 with lower case and descenders: labels, values, sentences. */
const BitmapFont& font();
/** 3x5 capitals: scales, axis labels, small print. */
const BitmapFont& tiny();

//==============================================================================
/** Ordered dither patterns. They're anchored to the canvas, so neighbouring components line up. */
enum class Pattern { checker, dots, sparse, hlines, vlines, diagonal, weave };

/** Small 1-bit image written as rows of '#' and '.'. */
struct Sprite
{
    Sprite (std::initializer_list<const char*> rows);
    int w = 0, h = 0;
    std::vector<uint8_t> on;
};

//==============================================================================
/** Pixel painter. Coordinates are whole art pixels in the component's own space. */
class Pix
{
public:
    Pix (juce::Graphics&, const juce::Component&);
    explicit Pix (juce::Graphics&, juce::Point<int> canvasOrigin = {});

    void fill (juce::Rectangle<int>, juce::Colour = ink);
    void fill (juce::Rectangle<int>, juce::Colour, Pattern);
    /** The classic dithered "greyed out" look: knocks out every other pixel. */
    void grey (juce::Rectangle<int> r) { fill (r, paper, Pattern::checker); }

    void dot (int x, int y, juce::Colour = ink);
    void hline (int x0, int x1, int y, juce::Colour = ink);
    void vline (int x, int y0, int y1, juce::Colour = ink);
    void dottedH (int x0, int x1, int y, int step = 2, juce::Colour = ink);
    void dottedV (int x, int y0, int y1, int step = 2, juce::Colour = ink);
    void frame (juce::Rectangle<int>, juce::Colour = ink);
    void dottedFrame (juce::Rectangle<int>, juce::Colour = ink);
    void line (int x0, int y0, int x1, int y1, juce::Colour = ink);
    void disc (int cx, int cy, int r, juce::Colour = ink);
    void disc (int cx, int cy, int r, juce::Colour, Pattern);
    void ring (int cx, int cy, int r, juce::Colour = ink);
    /** Solid arrow head filling the box: 'r', 'l', 'u' or 'd'. */
    void triangle (juce::Rectangle<int> box, char direction, juce::Colour = ink);
    void sprite (const Sprite&, int x, int y, juce::Colour = ink, int scale = 1);

    /** Draws text with its cap height starting at y. Returns the width drawn. */
    int text (const juce::String&, int x, int y, juce::Colour = ink, const BitmapFont& = font(), int scale = 1);
    void text (const juce::String&, juce::Rectangle<int>, juce::Justification, juce::Colour = ink,
               const BitmapFont& = font(), int scale = 1);
    /** Word-wrapped text from the top of the area. Returns the number of lines drawn. */
    int paragraph (const juce::String&, juce::Rectangle<int>, juce::Colour = ink, const BitmapFont& = font());

    juce::Graphics& g;
    const juce::Point<int> origin;
};

/** Half-width of row dy in a pixel disc of radius r (-1 outside). Shared by disc and ring. */
int discHalfWidth (int r, int dy);

//==============================================================================
/** Marks the component whose coordinate space is the pixel canvas (patterns and popups anchor to it). */
void markAsCanvas (juce::Component&);
juce::Component* findCanvas (juce::Component*);
juce::Point<int> canvasOrigin (const juce::Component&);

/** Renders its owner into an image at 1 image pixel per art pixel, scales that up by the
    largest useful whole factor with nearest neighbour, and finishes any fractional zoom with a
    single linear pass. Pixel art stays crisp at any window size or display scaling, and only
    the regions that were repainted are redrawn. */
class CanvasCache : public juce::CachedComponentImage
{
public:
    explicit CanvasCache (juce::Component& c) : owner (c) {}

    void paint (juce::Graphics&) override;
    bool invalidateAll() override;
    bool invalidate (const juce::Rectangle<int>&) override;
    void releaseResources() override;

private:
    static bool contentsLost (const juce::Image&);

    juce::Component& owner;
    juce::Image art, up;
    juce::RectangleList<int> dirty, upDirty;
    int upFactor = 0;
};
} // namespace forge::pixel
