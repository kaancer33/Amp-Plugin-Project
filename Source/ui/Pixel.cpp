#include "Pixel.h"

namespace forge::pixel
{
using namespace juce;

//==============================================================================
// Fonts
//==============================================================================
BitmapFont::BitmapFont (int asc, int line, bool upperCaseOnly,
                        std::initializer_list<Definition> definitions, const char* fallback)
    : ascent (asc), lineHeight (line), upperOnly (upperCaseOnly)
{
    for (const auto& [c, def] : definitions)
        glyphs[c] = parse (def);
    fallbackGlyph = parse (fallback);
}

BitmapFont::Glyph BitmapFont::parse (const char* def)
{
    Glyph gl;
    int row = 0, col = 0;
    for (const char* p = def;; ++p)
    {
        const char ch = *p;
        if (ch == ' ' || ch == 0)
        {
            if (col > 0)
            {
                gl.width = jmax (gl.width, col);
                ++row;
                col = 0;
            }
            if (ch == 0)
                break;
            continue;
        }
        if (ch == '#' && row < (int) gl.bits.size())
            gl.bits[(size_t) row] |= (uint16_t) (1u << col);
        ++col;
    }
    gl.rows = jmin (row, (int) gl.bits.size());
    return gl;
}

const BitmapFont::Glyph& BitmapFont::glyph (juce_wchar c) const
{
    if (upperOnly)
        c = CharacterFunctions::toUpperCase (c);
    const auto it = glyphs.find (c);
    return it != glyphs.end() ? it->second : fallbackGlyph;
}

int BitmapFont::width (const String& s, int scale) const
{
    int w = 0;
    for (auto t = s.getCharPointer(); ! t.isEmpty();)
        w += glyph (t.getAndAdvance()).width + spacing;
    return jmax (0, w - spacing) * scale;
}

String BitmapFont::fit (const String& s, int maxWidth) const
{
    if (width (s) <= maxWidth)
        return s;
    auto t = s;
    while (t.isNotEmpty() && width (t.trimEnd() + "..") > maxWidth)
        t = t.dropLastCharacters (1);
    return t.trimEnd() + "..";
}

StringArray BitmapFont::wrap (const String& text, int maxWidth) const
{
    StringArray lines;
    for (const auto& para : StringArray::fromLines (text))
    {
        String line;
        for (const auto& word : StringArray::fromTokens (para, " ", {}))
        {
            if (word.isEmpty())
                continue;
            const auto candidate = line.isEmpty() ? word : line + " " + word;
            if (width (candidate) <= maxWidth || line.isEmpty())
            {
                line = width (candidate) <= maxWidth ? candidate : fit (candidate, maxWidth);
            }
            else
            {
                lines.add (line);
                line = width (word) <= maxWidth ? word : fit (word, maxWidth);
            }
        }
        lines.add (line);
    }
    return lines;
}

const BitmapFont& font()
{
    static const BitmapFont f (7, 10, false, {
        { 'A', ".###. #...# #...# ##### #...# #...# #...#" },
        { 'B', "####. #...# #...# ####. #...# #...# ####." },
        { 'C', ".###. #...# #.... #.... #.... #...# .###." },
        { 'D', "####. #...# #...# #...# #...# #...# ####." },
        { 'E', "##### #.... #.... ####. #.... #.... #####" },
        { 'F', "##### #.... #.... ####. #.... #.... #...." },
        { 'G', ".###. #...# #.... #.### #...# #...# .####" },
        { 'H', "#...# #...# #...# ##### #...# #...# #...#" },
        { 'I', "### .#. .#. .#. .#. .#. ###" },
        { 'J', "....# ....# ....# ....# #...# #...# .###." },
        { 'K', "#...# #..#. #.#.. ##... #.#.. #..#. #...#" },
        { 'L', "#.... #.... #.... #.... #.... #.... #####" },
        { 'M', "#...# ##.## #.#.# #.#.# #...# #...# #...#" },
        { 'N', "#...# #...# ##..# #.#.# #..## #...# #...#" },
        { 'O', ".###. #...# #...# #...# #...# #...# .###." },
        { 'P', "####. #...# #...# ####. #.... #.... #...." },
        { 'Q', ".###. #...# #...# #...# #.#.# #..#. .##.#" },
        { 'R', "####. #...# #...# ####. #.#.. #..#. #...#" },
        { 'S', ".#### #.... #.... .###. ....# ....# ####." },
        { 'T', "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#.." },
        { 'U', "#...# #...# #...# #...# #...# #...# .###." },
        { 'V', "#...# #...# #...# #...# #...# .#.#. ..#.." },
        { 'W', "#...# #...# #...# #.#.# #.#.# #.#.# .#.#." },
        { 'X', "#...# #...# .#.#. ..#.. .#.#. #...# #...#" },
        { 'Y', "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#.." },
        { 'Z', "##### ....# ...#. ..#.. .#... #.... #####" },

        { 'a', "..... ..... .###. ....# .#### #...# .####" },
        { 'b', "#.... #.... ####. #...# #...# #...# ####." },
        { 'c', ".... .... .### #... #... #... .###" },
        { 'd', "....# ....# .#### #...# #...# #...# .####" },
        { 'e', "..... ..... .###. #...# ##### #.... .###." },
        { 'f', "..## .#.. #### .#.. .#.. .#.. .#.." },
        { 'g', "..... ..... .#### #...# #...# #...# .#### ....# .###." },
        { 'h', "#.... #.... ####. #...# #...# #...# #...#" },
        { 'i', "# . # # # # #" },
        { 'j', "..# ... ..# ..# ..# ..# ..# ..# ##." },
        { 'k', "#... #... #..# #.#. ##.. #.#. #..#" },
        { 'l', "#. #. #. #. #. #. .#" },
        { 'm', "..... ..... ##.#. #.#.# #.#.# #.#.# #.#.#" },
        { 'n', "..... ..... ####. #...# #...# #...# #...#" },
        { 'o', "..... ..... .###. #...# #...# #...# .###." },
        { 'p', "..... ..... ####. #...# #...# #...# ####. #.... #...." },
        { 'q', "..... ..... .#### #...# #...# #...# .#### ....# ....#" },
        { 'r', ".... .... #.## ##.. #... #... #..." },
        { 's', "..... ..... .#### #.... .###. ....# ####." },
        { 't', ".#.. .#.. #### .#.. .#.. .#.. ..##" },
        { 'u', "..... ..... #...# #...# #...# #...# .####" },
        { 'v', "..... ..... #...# #...# #...# .#.#. ..#.." },
        { 'w', "..... ..... #...# #...# #.#.# #.#.# .#.#." },
        { 'x', "..... ..... #...# .#.#. ..#.. .#.#. #...#" },
        { 'y', "..... ..... #...# #...# #...# #...# .#### ....# .###." },
        { 'z', "..... ..... ##### ...#. ..#.. .#... #####" },

        { '0', ".###. #...# #..## #.#.# ##..# #...# .###." },
        { '1', "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###." },
        { '2', ".###. #...# ....# ...#. ..#.. .#... #####" },
        { '3', "##### ...#. ..#.. ...#. ....# #...# .###." },
        { '4', "...#. ..##. .#.#. #..#. ##### ...#. ...#." },
        { '5', "##### #.... ####. ....# ....# #...# .###." },
        { '6', "..##. .#... #.... ####. #...# #...# .###." },
        { '7', "##### ....# ...#. ..#.. .#... .#... .#..." },
        { '8', ".###. #...# #...# .###. #...# #...# .###." },
        { '9', ".###. #...# #...# .#### ....# ...#. .##.." },

        { ' ', "... ... ... ... ... ... ..." },
        { '!', "# # # # # . #" },
        { '"', "#.# #.# ... ... ... ... ..." },
        { '#', ".#.#. .#.#. ##### .#.#. ##### .#.#. .#.#." },
        { '$', "..#.. .#### #.#.. .###. ..#.# ####. ..#.." },
        { '%', "##..# ##..# ...#. ..#.. .#... #..## #..##" },
        { '&', ".##.. #..#. #.#.. .#... #.#.# #..#. .##.#" },
        { '\'', "# # . . . . ." },
        { '(', "..# .#. #.. #.. #.. .#. ..#" },
        { ')', "#.. .#. ..# ..# ..# .#. #.." },
        { '*', "..... ..#.. #.#.# .###. #.#.# ..#.. ....." },
        { '+', "..... ..#.. ..#.. ##### ..#.. ..#.. ....." },
        { ',', ".. .. .. .. .. .# .# #." },
        { '-', ".... .... .... #### .... .... ...." },
        { '.', ". . . . . . #" },
        { '/', "....# ....# ...#. ..#.. .#... #.... #...." },
        { ':', ". . # . . # ." },
        { ';', ".. .. .# .. .. .# .# #." },
        { '<', "...# ..#. .#.. #... .#.. ..#. ...#" },
        { '=', ".... .... #### .... #### .... ...." },
        { '>', "#... .#.. ..#. ...# ..#. .#.. #..." },
        { '?', ".###. #...# ....# ...#. ..#.. ..... ..#.." },
        { '@', ".###. #...# #.### #.#.# #.### #.... .####" },
        { '[', "### #.. #.. #.. #.. #.. ###" },
        { '\\', "#.... #.... .#... ..#.. ...#. ....# ....#" },
        { ']', "### ..# ..# ..# ..# ..# ###" },
        { '^', "..#.. .#.#. #...# ..... ..... ..... ....." },
        { '_', "..... ..... ..... ..... ..... ..... #####" },
        { '`', "#. .# .. .. .. .. .." },
        { '{', "..# .#. .#. #.. .#. .#. ..#" },
        { '|', "# # # # # # #" },
        { '}', "#.. .#. .#. ..# .#. .#. #.." },
        { '~', "..... ..... .#..# #.#.# #..#. ..... ....." },

        { 0x25B6, "#... ##.. ###. #### ###. ##.. #..." },          // play / right arrow
        { 0x25C0, "...# ..## .### #### .### ..## ...#" },          // left arrow
        { 0x25BC, "..... ..... ##### .###. ..#.. ..... ....." },   // down arrow
        { 0x25B2, "..... ..... ..#.. .###. ##### ..... ....." },   // up arrow
        { 0x2022, ".. .. .. ## ## .. .." },                        // bullet
        { 0x00B7, ". . . # . . ." },                               // middle dot
        { 0x00D7, "..... ..... #...# .#.#. ..#.. .#.#. #...#" },   // multiply
        { 0x2713, "..... ....# ...## #.##. ###.. .#... ....." },   // tick
        { 0x2026, "..... ..... ..... ..... ..... ..... #.#.#" },   // ellipsis
        { 0x2588, "##### ##### ##### ##### ##### ##### #####" },   // block (caret)
        { 0x2013, "..... ..... ..... ##### ..... ..... ....." },   // en dash
        { 0x2261, "..... ##### ..... ##### ..... ##### ....." },   // menu bars
        { 0x00B0, ".#. #.# .#. ... ... ... ..." },                 // degree
    }, "##### #...# #...# #...# #...# #...# #####");
    return f;
}

const BitmapFont& tiny()
{
    static const BitmapFont f (5, 7, true, {
        { 'A', ".#. #.# ### #.# #.#" },
        { 'B', "##. #.# ##. #.# ##." },
        { 'C', ".## #.. #.. #.. .##" },
        { 'D', "##. #.# #.# #.# ##." },
        { 'E', "### #.. ##. #.. ###" },
        { 'F', "### #.. ##. #.. #.." },
        { 'G', ".## #.. #.# #.# .##" },
        { 'H', "#.# #.# ### #.# #.#" },
        { 'I', "### .#. .#. .#. ###" },
        { 'J', "..# ..# ..# #.# .#." },
        { 'K', "#.# #.# ##. #.# #.#" },
        { 'L', "#.. #.. #.. #.. ###" },
        { 'M', "#...# ##.## #.#.# #...# #...#" },
        { 'N', "#..# ##.# #.## #..# #..#" },
        { 'O', ".#. #.# #.# #.# .#." },
        { 'P', "##. #.# ##. #.. #.." },
        { 'Q', ".#. #.# #.# ### .##" },
        { 'R', "##. #.# ##. #.# #.#" },
        { 'S', ".## #.. .#. ..# ##." },
        { 'T', "### .#. .#. .#. .#." },
        { 'U', "#.# #.# #.# #.# ###" },
        { 'V', "#.# #.# #.# .#. .#." },
        { 'W', "#...# #...# #.#.# ##.## #...#" },
        { 'X', "#.# #.# .#. #.# #.#" },
        { 'Y', "#.# #.# .#. .#. .#." },
        { 'Z', "### ..# .#. #.. ###" },
        { '0', "### #.# #.# #.# ###" },
        { '1', ".#. ##. .#. .#. ###" },
        { '2', "##. ..# .#. #.. ###" },
        { '3', "##. ..# .#. ..# ##." },
        { '4', "#.# #.# ### ..# ..#" },
        { '5', "### #.. ##. ..# ##." },
        { '6', ".## #.. ### #.# ###" },
        { '7', "### ..# .#. .#. .#." },
        { '8', "### #.# ### #.# ###" },
        { '9', "### #.# ### ..# ##." },
        { ' ', ".. .. .. .. .." },
        { '-', "... ... ### ... ..." },
        { '+', "... .#. ### .#. ..." },
        { '.', ". . . . #" },
        { ',', ". . . # #" },
        { ':', ". # . # ." },
        { '/', "..# ..# .#. #.. #.." },
        { '%', "#.# ..# .#. #.. #.#" },
        { '(', ".# #. #. #. .#" },
        { ')', "#. .# .# .# #." },
        { '!', "# # # . #" },
        { '?', "##. ..# .#. ... .#." },
        { '=', "... ### ... ### ..." },
        { '<', "..# .#. #.. .#. ..#" },
        { '>', "#.. .#. ..# .#. #.." },
        { '\'', "# # . . ." },
        { '"', "#.# #.# ... ... ..." },
        { '_', "... ... ... ... ###" },
        { '#', "#.# ### #.# ### #.#" },
        { '&', ".#. #.# .#. #.# .##" },
        { '*', "#.# .#. #.# ... ..." },
        { '[', "## #. #. #. ##" },
        { ']', "## .# .# .# ##" },
        { 0x25B6, "#.. ##. ### ##. #.." },
        { 0x25C0, "..# .## ### .## ..#" },
        { 0x00B7, ". . # . ." },
        { 0x00D7, "... #.# .#. #.# ..." },
    }, "### #.# #.# #.# ###");
    return f;
}

//==============================================================================
// Patterns and sprites
//==============================================================================
namespace
{
    bool patternBit (Pattern p, int x, int y)
    {
        switch (p)
        {
            case Pattern::checker:  return ((x + y) & 1) == 0;
            case Pattern::dots:     return (x & 1) == 0 && (y & 1) == 0;
            case Pattern::sparse:   return (y & 1) == 0 && ((x + (y & 2)) & 3) == 0;
            case Pattern::hlines:   return (y & 1) == 0;
            case Pattern::vlines:   return (x & 1) == 0;
            case Pattern::diagonal: return ((x + y) & 3) == 0;
            case Pattern::weave:    return (((x >> 1) + (y >> 1)) & 1) == 0;
        }
        return false;
    }

    const Image& patternTile (Pattern p, Colour c)
    {
        static std::map<std::pair<int, uint32>, Image> cache;
        auto& tile = cache[{ (int) p, c.getARGB() }];
        if (tile.isNull())
        {
            tile = Image (Image::ARGB, 4, 4, true, SoftwareImageType());
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    if (patternBit (p, x, y))
                        tile.setPixelAt (x, y, c);
        }
        return tile;
    }
}

Sprite::Sprite (std::initializer_list<const char*> rows)
{
    h = (int) rows.size();
    for (auto* r : rows)
        w = jmax (w, (int) std::strlen (r));
    on.assign ((size_t) (w * h), 0);
    int y = 0;
    for (auto* r : rows)
    {
        for (int x = 0; r[x] != 0; ++x)
            on[(size_t) (y * w + x)] = r[x] == '#' ? 1 : 0;
        ++y;
    }
}

int discHalfWidth (int r, int dy)
{
    const double v = (double) r * r + r * 0.6 - (double) dy * dy;
    return v < 0.0 ? -1 : (int) std::floor (std::sqrt (v));
}

//==============================================================================
// Canvas
//==============================================================================
void markAsCanvas (Component& c)
{
    c.getProperties().set ("pixelCanvas", true);
}

Component* findCanvas (Component* c)
{
    for (; c != nullptr; c = c->getParentComponent())
        if (c->getProperties().contains ("pixelCanvas"))
            return c;
    return nullptr;
}

Point<int> canvasOrigin (const Component& c)
{
    Point<int> p;
    for (auto* comp = &c; comp != nullptr; comp = comp->getParentComponent())
    {
        if (comp->getProperties().contains ("pixelCanvas"))
            break;
        p += comp->getPosition();
    }
    return p;
}

//==============================================================================
// Pix
//==============================================================================
Pix::Pix (Graphics& graphics, const Component& c) : Pix (graphics, canvasOrigin (c)) {}

Pix::Pix (Graphics& graphics, Point<int> o) : g (graphics), origin (o)
{
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
}

void Pix::fill (Rectangle<int> r, Colour c)
{
    g.setColour (c);
    g.fillRect (r);
}

void Pix::fill (Rectangle<int> r, Colour c, Pattern p)
{
    if (r.isEmpty())
        return;
    g.setTiledImageFill (patternTile (p, c), -origin.x, -origin.y, 1.0f);
    g.fillRect (r);
    g.setColour (c);
}

void Pix::dot (int x, int y, Colour c)
{
    g.setColour (c);
    g.fillRect (x, y, 1, 1);
}

void Pix::hline (int x0, int x1, int y, Colour c)
{
    if (x1 < x0) std::swap (x0, x1);
    g.setColour (c);
    g.fillRect (x0, y, x1 - x0 + 1, 1);
}

void Pix::vline (int x, int y0, int y1, Colour c)
{
    if (y1 < y0) std::swap (y0, y1);
    g.setColour (c);
    g.fillRect (x, y0, 1, y1 - y0 + 1);
}

void Pix::dottedH (int x0, int x1, int y, int step, Colour c)
{
    g.setColour (c);
    for (int x = x0; x <= x1; ++x)
        if ((((x + origin.x) % step) + step) % step == 0)
            g.fillRect (x, y, 1, 1);
}

void Pix::dottedV (int x, int y0, int y1, int step, Colour c)
{
    g.setColour (c);
    for (int y = y0; y <= y1; ++y)
        if ((((y + origin.y) % step) + step) % step == 0)
            g.fillRect (x, y, 1, 1);
}

void Pix::frame (Rectangle<int> r, Colour c)
{
    if (r.isEmpty())
        return;
    hline (r.getX(), r.getRight() - 1, r.getY(), c);
    hline (r.getX(), r.getRight() - 1, r.getBottom() - 1, c);
    vline (r.getX(), r.getY(), r.getBottom() - 1, c);
    vline (r.getRight() - 1, r.getY(), r.getBottom() - 1, c);
}

void Pix::dottedFrame (Rectangle<int> r, Colour c)
{
    dottedH (r.getX(), r.getRight() - 1, r.getY(), 2, c);
    dottedH (r.getX(), r.getRight() - 1, r.getBottom() - 1, 2, c);
    dottedV (r.getX(), r.getY(), r.getBottom() - 1, 2, c);
    dottedV (r.getRight() - 1, r.getY(), r.getBottom() - 1, 2, c);
}

void Pix::line (int x0, int y0, int x1, int y1, Colour c)
{
    g.setColour (c);
    const int dx = std::abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        g.fillRect (x0, y0, 1, 1);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Pix::disc (int cx, int cy, int r, Colour c)
{
    g.setColour (c);
    for (int dy = -r; dy <= r; ++dy)
        if (const int hw = discHalfWidth (r, dy); hw >= 0)
            g.fillRect (cx - hw, cy + dy, hw * 2 + 1, 1);
}

void Pix::disc (int cx, int cy, int r, Colour c, Pattern p)
{
    for (int dy = -r; dy <= r; ++dy)
        if (const int hw = discHalfWidth (r, dy); hw >= 0)
            fill ({ cx - hw, cy + dy, hw * 2 + 1, 1 }, c, p);
}

void Pix::ring (int cx, int cy, int r, Colour c)
{
    g.setColour (c);
    auto half = [r] (int dy) { return std::abs (dy) > r ? -1 : discHalfWidth (r, dy); };
    for (int dy = -r; dy <= r; ++dy)
    {
        const int hw = half (dy);
        if (hw < 0)
            continue;
        const int inner = jmin (half (dy - 1), half (dy + 1));
        const int from = jmin (hw, inner + 1);
        g.fillRect (cx + from, cy + dy, hw - from + 1, 1);
        g.fillRect (cx - hw, cy + dy, hw - from + 1, 1);
    }
}

void Pix::triangle (Rectangle<int> box, char direction, Colour c)
{
    g.setColour (c);
    const int w = box.getWidth(), h = box.getHeight();
    if (direction == 'r' || direction == 'l')
    {
        for (int y = 0; y < h; ++y)
        {
            const int len = jmin (w, jmin (y, h - 1 - y) + 1);
            g.fillRect (direction == 'r' ? box.getX() : box.getRight() - len, box.getY() + y, len, 1);
        }
    }
    else
    {
        for (int k = 0; k < h; ++k)
        {
            const int len = w - 2 * k;
            if (len <= 0)
                break;
            const int y = direction == 'd' ? box.getY() + k : box.getBottom() - 1 - k;
            g.fillRect (box.getX() + k, y, len, 1);
        }
    }
}

void Pix::sprite (const Sprite& s, int x, int y, Colour c, int scale)
{
    g.setColour (c);
    for (int row = 0; row < s.h; ++row)
    {
        int col = 0;
        while (col < s.w)
        {
            if (s.on[(size_t) (row * s.w + col)] == 0) { ++col; continue; }
            int end = col;
            while (end < s.w && s.on[(size_t) (row * s.w + end)] != 0)
                ++end;
            g.fillRect (x + col * scale, y + row * scale, (end - col) * scale, scale);
            col = end;
        }
    }
}

int Pix::text (const String& s, int x, int y, Colour c, const BitmapFont& f, int scale)
{
    g.setColour (c);
    const int startX = x;
    for (auto t = s.getCharPointer(); ! t.isEmpty();)
    {
        const auto& gl = f.glyph (t.getAndAdvance());
        for (int row = 0; row < gl.rows; ++row)
        {
            const auto bits = gl.bits[(size_t) row];
            int col = 0;
            while (bits != 0 && col < gl.width)
            {
                if ((bits & (1u << col)) == 0) { ++col; continue; }
                int end = col;
                while (end < gl.width && (bits & (1u << end)) != 0)
                    ++end;
                g.fillRect (x + col * scale, y + row * scale, (end - col) * scale, scale);
                col = end;
            }
        }
        x += (gl.width + BitmapFont::spacing) * scale;
    }
    return jmax (0, x - startX - BitmapFont::spacing * scale);
}

void Pix::text (const String& s, Rectangle<int> area, Justification j, Colour c, const BitmapFont& f, int scale)
{
    const int w = f.width (s, scale), h = f.ascent * scale;
    int x = area.getX();
    if (j.testFlags (Justification::horizontallyCentred))  x = area.getX() + (area.getWidth() - w) / 2;
    else if (j.testFlags (Justification::right))           x = area.getRight() - w;

    int y = area.getY() + (area.getHeight() - h) / 2;
    if (j.testFlags (Justification::top))          y = area.getY();
    else if (j.testFlags (Justification::bottom))  y = area.getBottom() - h;

    text (s, x, y, c, f, scale);
}

int Pix::paragraph (const String& s, Rectangle<int> area, Colour c, const BitmapFont& f)
{
    int lines = 0;
    int y = area.getY();
    for (const auto& line : f.wrap (s, area.getWidth()))
    {
        if (y + f.ascent > area.getBottom())
            break;
        text (line, area.getX(), y, c, f);
        y += f.lineHeight;
        ++lines;
    }
    return lines;
}

//==============================================================================
// CanvasCache
//==============================================================================
bool CanvasCache::contentsLost (const Image& im)
{
    if (auto data = im.getPixelData())
        if (auto* backup = data->getBackupExtensions())
            return backup->needsBackup() && ! backup->canBackup();
    return false;
}

bool CanvasCache::invalidateAll()
{
    dirty.clear();
    dirty.add (owner.getLocalBounds());
    return true;
}

bool CanvasCache::invalidate (const Rectangle<int>& area)
{
    dirty.add (area.getIntersection (owner.getLocalBounds()));
    return true;
}

void CanvasCache::releaseResources()
{
    art = {};
    up = {};
    upFactor = 0;
}

void CanvasCache::paint (Graphics& g)
{
    auto& context = g.getInternalContext();
    const float scale = context.getPhysicalPixelScaleFactor();
    const int factor = jlimit (1, 6, (int) std::ceil (scale - 0.02f));
    const auto bounds = owner.getLocalBounds();
    if (bounds.isEmpty())
        return;

    const auto imageType = context.getPreferredImageTypeForTemporaryImages();

    if (art.isNull() || art.getBounds() != bounds || contentsLost (art))
    {
        art = Image (Image::ARGB, bounds.getWidth(), bounds.getHeight(), true, *imageType);
        art.setBackupEnabled (false);
        dirty.clear();
        dirty.add (bounds);
        upFactor = 0;
    }

    if (factor > 1 && (upFactor != factor || up.isNull() || contentsLost (up)))
    {
        up = Image (Image::ARGB, bounds.getWidth() * factor, bounds.getHeight() * factor, true, *imageType);
        up.setBackupEnabled (false);
        upFactor = factor;
        upDirty.clear();
        upDirty.add (bounds);
    }

    if (! dirty.isEmpty())
    {
        {
            Graphics artGraphics (art);
            artGraphics.reduceClipRegion (dirty);
            artGraphics.fillAll (paper);
            owner.paintEntireComponent (artGraphics, true);
        }
        upDirty.add (dirty);
        dirty.clear();
    }

    if (factor > 1 && ! upDirty.isEmpty())
    {
        RectangleList<int> scaled;
        for (auto r : upDirty)
            scaled.add (r * factor);

        Graphics upGraphics (up);
        upGraphics.reduceClipRegion (scaled);
        upGraphics.setImageResamplingQuality (Graphics::lowResamplingQuality);
        upGraphics.drawImageTransformed (art, AffineTransform::scale ((float) factor));
    }
    upDirty.clear();

    const bool exact = std::abs (scale - (float) factor) < 0.02f;
    g.setImageResamplingQuality (exact ? Graphics::lowResamplingQuality : Graphics::mediumResamplingQuality);
    g.setOpacity (1.0f);
    g.drawImageTransformed (factor > 1 ? up : art, AffineTransform::scale (1.0f / (float) factor));
}
} // namespace forge::pixel
