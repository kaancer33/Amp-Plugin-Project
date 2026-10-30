#include "Widgets.h"
#include "PixelLookAndFeel.h"

namespace forge
{
using namespace juce;
using namespace pixel;

PopupMenu::Options pixelMenuOptions (Component& target)
{
    return PopupMenu::Options().withTargetComponent (&target).withParentComponent (findCanvas (&target));
}

void drawWindow (Pix& p, Rectangle<int> r, const String& title, const String& status, int titleX)
{
    const int x0 = r.getX(), y0 = r.getY();
    p.fill (r, paper);
    p.frame (r);

    for (int y = y0 + 3; y <= y0 + 9; y += 2)
        p.hline (x0 + 2, r.getRight() - 3, y);
    p.hline (x0, r.getRight() - 1, y0 + kTitleBarH - 1);

    if (titleX > 5)
        p.fill ({ x0 + 1, y0 + 1, titleX - 2, kTitleBarH - 2 }, paper);

    const auto t = title.toUpperCase();
    p.fill ({ x0 + titleX - 3, y0 + 1, font().width (t) + 6, kTitleBarH - 2 }, paper);
    p.text (t, x0 + titleX, y0 + 3);

    if (status.isNotEmpty())
    {
        const auto s = status.toUpperCase();
        const int sw = tiny().width (s);
        const int sx = r.getRight() - 5 - sw;
        p.fill ({ sx - 3, y0 + 1, sw + 6, kTitleBarH - 2 }, paper);
        p.text (s, sx, y0 + 4, ink, tiny());
    }
}

//==============================================================================
// PixelTextField
//==============================================================================
PixelTextField::PixelTextField()
{
    setWantsKeyboardFocus (true);
    setMouseCursor (MouseCursor::IBeamCursor);
}

void PixelTextField::setText (const String& t, bool selectAll)
{
    text = t.substring (0, maxLength);
    caret = text.length();
    allSelected = selectAll && text.isNotEmpty();
    caretOn = true;
    repaint();
}

void PixelTextField::insert (const String& raw)
{
    const auto s = raw.removeCharacters ("\r\n\t");
    if (allSelected)
    {
        text.clear();
        caret = 0;
        allSelected = false;
    }
    text = (text.substring (0, caret) + s + text.substring (caret)).substring (0, maxLength);
    caret = jmin (text.length(), caret + s.length());
}

bool PixelTextField::keyPressed (const KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();

    if (key == KeyPress::returnKey)  { if (onReturn) onReturn(); return true; }
    if (key == KeyPress::escapeKey)  { if (onEscape) onEscape(); return true; }

    if (mods.isCommandDown())
    {
        if (code == 'A')      { allSelected = text.isNotEmpty(); caret = text.length(); }
        else if (code == 'C') { SystemClipboard::copyTextToClipboard (text); return true; }
        else if (code == 'V') { insert (SystemClipboard::getTextFromClipboard()); }
        else return false;
    }
    else if (key == KeyPress::backspaceKey)
    {
        if (allSelected)       { text.clear(); caret = 0; }
        else if (caret > 0)    { text = text.substring (0, caret - 1) + text.substring (caret); --caret; }
        allSelected = false;
    }
    else if (key == KeyPress::deleteKey)
    {
        if (allSelected)       { text.clear(); caret = 0; }
        else                   text = text.substring (0, caret) + text.substring (caret + 1);
        allSelected = false;
    }
    else if (key == KeyPress::leftKey)   { caret = allSelected ? 0 : jmax (0, caret - 1); allSelected = false; }
    else if (key == KeyPress::rightKey)  { caret = allSelected ? text.length() : jmin (text.length(), caret + 1); allSelected = false; }
    else if (key == KeyPress::homeKey)   { caret = 0; allSelected = false; }
    else if (key == KeyPress::endKey)    { caret = text.length(); allSelected = false; }
    else if (key.getTextCharacter() >= ' ')
    {
        insert (String::charToString (key.getTextCharacter()));
    }
    else
    {
        return false;
    }

    caretOn = true;
    repaint();
    return true;
}

void PixelTextField::focusGained (FocusChangeType)
{
    caretOn = true;
    startTimer (500);
    repaint();
}

void PixelTextField::focusLost (FocusChangeType)
{
    stopTimer();
    repaint();
    if (onFocusLost)
        onFocusLost();
}

void PixelTextField::mouseDown (const MouseEvent&)
{
    allSelected = false;
    caret = text.length();
    grabKeyboardFocus();
    repaint();
}

void PixelTextField::timerCallback()
{
    caretOn = ! caretOn;
    repaint();
}

void PixelTextField::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto r = getLocalBounds();
    p.fill (r, paper);
    p.frame (r);

    const auto inner = r.reduced (2, 1);
    const auto& f = font();
    const int ty = (getHeight() - 7) / 2;
    const int caretX = f.width (text.substring (0, caret)) + (caret > 0 ? 1 : 0);
    const int scroll = jmax (0, caretX + 6 - inner.getWidth());
    const int x0 = inner.getX() + 1 - scroll;

    Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (inner);

    if (allSelected)
    {
        p.fill ({ x0 - 1, ty - 1, f.width (text) + 2, 10 });
        p.text (text, x0, ty, paper);
        return;
    }

    p.text (text, x0, ty);
    if (caretOn && hasKeyboardFocus (false))
    {
        if (caret >= text.length())
            p.fill ({ x0 + caretX, ty, 5, 7 });
        else
            p.vline (x0 + caretX - 1, ty - 1, ty + 7);
    }
}

//==============================================================================
// Knob
//==============================================================================
Knob::Knob (APVTS& state, const String& paramId, const String& label, const String& help, bool bipolar,
            int r, bool small)
    : name (label.toUpperCase()), radius (r), smallLabel (small)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (120);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip ((help.isNotEmpty() ? help + "  " : String()) + "Drag / Shift-drag for fine / double-click resets.");
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    if (auto* param = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    slider.onValueChange = [this] { readout.repaint(); };
    slider.onDragStart   = [this] { readout.repaint(); };
    slider.onDragEnd     = [this] { readout.repaint(); };
    addAndMakeVisible (readout);

    editor.maxLength = 16;
    editor.onReturn    = [this] { endEdit (true); };
    editor.onEscape    = [this] { endEdit (false); };
    editor.onFocusLost = [this] { endEdit (true); };
    addChildComponent (editor);
}

Rectangle<int> Knob::readoutArea() const
{
    return { 0, getHeight() - 11, getWidth(), 11 };
}

void Knob::resized()
{
    const int d = (radius + 3) * 2 + 1;
    slider.setBounds ((getWidth() - d) / 2, 9, d, d);
    readout.setBounds (readoutArea());
    editor.setBounds (readoutArea());
}

void Knob::paint (Graphics& g)
{
    Pix p (g, *this);
    p.text (name, { 0, 0, getWidth(), 7 }, Justification::centred, ink, smallLabel ? tiny() : font());
}

void Knob::paintOverChildren (Graphics& g)
{
    // Dither just the dial, so the name and value stay readable while bypassed
    if (dimmed)
        Pix (g, *this).grey (slider.getBounds());
}

void Knob::setDimmed (bool shouldDim)
{
    if (shouldDim != dimmed)
    {
        dimmed = shouldDim;
        repaint();
    }
}

void Knob::beginEdit()
{
    editor.setText (slider.getTextFromValue (slider.getValue()), true);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
}

void Knob::endEdit (bool commit)
{
    if (! editor.isVisible())
        return;
    editor.setVisible (false);
    if (commit && editor.getText().trim().isNotEmpty())
        slider.setValue (slider.getValueFromText (editor.getText()), sendNotificationSync);
    readout.repaint();
}

void Knob::Readout::paint (Graphics& g)
{
    Pix p (g, *this);
    auto& s = owner.slider;
    const auto text = s.getTextFromValue (s.getValue());
    const auto& f = font().width (text) <= getWidth() - 4 ? font() : tiny();
    const int w = f.width (text);
    const Rectangle<int> box ((getWidth() - w - 4) / 2, 1, w + 4, 9);
    const int ty = box.getY() + (9 - f.ascent) / 2;

    if (s.isMouseOverOrDragging() || isMouseOver())
    {
        p.fill (box);
        p.text (text, box.getX() + 2, ty, paper, f);
    }
    else
    {
        p.text (text, box.getX() + 2, ty, ink, f);
    }
}

//==============================================================================
// PowerBox, CheckToggle
//==============================================================================
PowerBox::PowerBox (bool isLarge) : Button ("power"), large (isLarge)
{
    setClickingTogglesState (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void PowerBox::paintButton (Graphics& g, bool highlighted, bool)
{
    Pix p (g, *this);
    const auto fg = inverted ? paper : ink, bg = inverted ? ink : paper;
    const auto box = getLocalBounds();
    const bool on = getToggleState();

    p.fill (box, on ? fg : bg);
    p.frame (box, fg);
    if (highlighted && ! on)
        p.frame (box.reduced (1), fg);
    p.text (on ? "ON" : "OFF", box, Justification::centred, on ? bg : fg, large ? font() : tiny());
}

CheckToggle::CheckToggle (const String& t) : Button (t)
{
    setClickingTogglesState (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void CheckToggle::paintButton (Graphics& g, bool highlighted, bool)
{
    Pix p (g, *this);
    const int y = (getHeight() - 7) / 2;
    p.frame ({ 0, y, 7, 7 });
    if (getToggleState())
    {
        p.line (1, y + 1, 5, y + 5);
        p.line (5, y + 1, 1, y + 5);
    }
    const int w = p.text (getButtonText(), 10, y);
    if (highlighted)
        p.dottedH (10, 10 + w - 1, y + 8);
}

//==============================================================================
// ChoiceSelector
//==============================================================================
ChoiceSelector::ChoiceSelector (APVTS& state, const String& paramId, bool isVertical)
    : vertical (isVertical)
{
    auto* param = dynamic_cast<AudioParameterChoice*> (state.getParameter (paramId));
    jassert (param != nullptr);
    items = param->choices;
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v)
    {
        selected = roundToInt (v);
        repaint();
    }, state.undoManager);
    attachment->sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

Rectangle<int> ChoiceSelector::itemBounds (int index) const
{
    if (vertical)
        return { 0, index * rowHeight, getWidth(), rowHeight - 1 };

    const int n = items.size(), gap = 2;
    const int w = (getWidth() - gap * (n - 1)) / n;
    const int x = index * (w + gap);
    return { x, 0, index == n - 1 ? getWidth() - x : w, getHeight() };
}

int ChoiceSelector::indexAt (Point<int> p) const
{
    for (int i = 0; i < items.size(); ++i)
        if (itemBounds (i).contains (p))
            return i;
    return -1;
}

void ChoiceSelector::paint (Graphics& g)
{
    Pix p (g, *this);
    for (int i = 0; i < items.size(); ++i)
    {
        const auto b = itemBounds (i);
        const bool sel = i == selected, hov = i == hovered;
        const auto c = sel ? paper : ink;

        if (vertical)
        {
            if (sel)
                p.fill (b);
            else if (hov)
                p.dottedFrame (b);
            if (sel)
                p.triangle ({ b.getX() + 3, b.getY() + 2, 4, 7 }, 'r', c);
            p.text (font().fit (items[i], b.getWidth() - 12), b.getX() + 10, b.getY() + 2, c);
        }
        else
        {
            if (sel)
                p.fill (b);
            p.frame (b);
            if (hov && ! sel)
                p.frame (b.reduced (1));
            p.text (items[i].toUpperCase(), b, Justification::centred, c);
        }
    }
}

void ChoiceSelector::mouseDown (const MouseEvent& e)
{
    const int idx = indexAt (e.getPosition());
    if (idx >= 0 && idx != selected)
        attachment->setValueAsCompleteGesture ((float) idx);
}

void ChoiceSelector::mouseMove (const MouseEvent& e)
{
    const int idx = indexAt (e.getPosition());
    if (idx != hovered)
    {
        hovered = idx;
        repaint();
    }
}

void ChoiceSelector::mouseExit (const MouseEvent&)
{
    hovered = -1;
    repaint();
}

String ChoiceSelector::getTooltip()
{
    if (hovered >= 0 && itemNote)
        return itemNote (hovered);
    return SettableTooltipClient::getTooltip();
}

//==============================================================================
// LevelMeterView
//==============================================================================
namespace
{
    constexpr float meterFloorDb = -60.0f;
    float meterFraction (float db) { return jlimit (0.0f, 1.0f, (db - meterFloorDb) / -meterFloorDb); }

    struct MeterLayout
    {
        explicit MeterLayout (Rectangle<int> b)
        {
            bar = { b.getRight() - 11, b.getY() + 13, 10, b.getHeight() - 16 };
            clip = { b.getRight() - 20, b.getY(), 19, 9 };
            segments = (bar.getHeight() + 1) / 3;
        }
        int segmentTop (int i) const { return bar.getBottom() - (i + 1) * 3 + 1; }
        int litFor (float db) const   { return roundToInt (meterFraction (db) * (float) segments); }

        Rectangle<int> bar, clip;
        int segments = 0;
    };
}

void LevelMeterView::pushLevel (float peak)
{
    const float db = Decibels::gainToDecibels (peak, -90.0f);
    if (peak >= 0.999f)
        clipped = true;
    displayDb = db > displayDb ? db : jmax (db, displayDb - 0.9f);   // ~27 dB/s fall at 30 fps

    if (db >= holdDb)          { holdDb = db; holdFrames = 40; }
    else if (--holdFrames < 0) { holdDb = jmax (displayDb, holdDb - 1.5f); }

    const MeterLayout m (getLocalBounds());
    const int lit = m.litFor (displayDb), hold = m.litFor (holdDb);
    if (lit != lastLit || hold != lastHold || clipped != lastClipped)
    {
        lastLit = lit;
        lastHold = hold;
        lastClipped = clipped;
        repaint();
    }
}

void LevelMeterView::paint (Graphics& g)
{
    Pix p (g, *this);
    const MeterLayout m (getLocalBounds());

    // CLIP latch
    if (clipped)
    {
        p.fill (m.clip);
        p.text ("CLIP", m.clip, Justification::centred, paper, tiny());
    }
    else
    {
        p.frame (m.clip);
        p.text ("CLIP", m.clip, Justification::centred, ink, tiny());
        p.grey (m.clip.reduced (1));
    }

    // Scale
    for (int db : { 0, 6, 12, 18, 24, 36, 48, 60 })
    {
        const int i = m.litFor ((float) -db);
        const int y = i == 0 ? m.bar.getBottom() - 1 : m.segmentTop (i - 1);
        p.hline (m.bar.getX() - 3, m.bar.getX() - 2, y);
        p.text (String (db), { 0, y - 2, m.bar.getX() - 5, 5 }, Justification::right, ink, tiny());
    }

    // Segments
    const int lit = m.litFor (displayDb), hold = m.litFor (holdDb);
    for (int i = 0; i < m.segments; ++i)
    {
        const Rectangle<int> seg (m.bar.getX(), m.segmentTop (i), m.bar.getWidth(), 2);
        if (i < lit || (i == hold - 1 && holdDb > meterFloorDb))
            p.fill (seg);
        else
            p.fill (seg, ink, Pattern::dots);
    }
}

//==============================================================================
// GateScope
//==============================================================================
void GateScope::resized()
{
    const int n = jmax (2, plotArea().getWidth());
    levelDb.assign ((size_t) n, -96.0f);
    gain.assign ((size_t) n, 1.0f);
    writePos = 0;
}

void GateScope::push (float inputPeak, float gateGain, float thresholdDb, bool enabled)
{
    if (levelDb.empty())
        return;
    levelDb[(size_t) writePos] = Decibels::gainToDecibels (inputPeak, -96.0f);
    gain[(size_t) writePos] = gateGain;
    writePos = (writePos + 1) % (int) levelDb.size();
    threshold = thresholdDb;
    on = enabled;
    repaint (plotArea());
}

void GateScope::paint (Graphics& g)
{
    Pix p (g, *this);
    p.frame (getLocalBounds());
    p.text ("INPUT LEVEL", 3, 2, ink, tiny());

    const auto plot = plotArea();
    if (levelDb.empty())
        return;

    const int n = (int) levelDb.size();
    auto yFor = [&] (float db)
    {
        const float t = jlimit (0.0f, 1.0f, (db + 96.0f) / 96.0f);
        return plot.getBottom() - 1 - roundToInt (t * (float) (plot.getHeight() - 1));
    };
    auto sample = [&] (int i) { return (writePos + i) % n; };

    // Closed stretches, dithered
    if (on)
    {
        for (int i = 0; i < n;)
        {
            if (gain[(size_t) sample (i)] > 0.5f) { ++i; continue; }
            int j = i;
            while (j < n && gain[(size_t) sample (j)] <= 0.5f)
                ++j;
            p.fill ({ plot.getX() + i, plot.getY(), j - i, plot.getHeight() }, ink, Pattern::checker);
            i = j;
        }
    }

    // Level history as a bar graph
    for (int i = 0; i < n; ++i)
    {
        const int x = plot.getX() + i;
        const int y = yFor (levelDb[(size_t) sample (i)]);
        if (y >= plot.getBottom() - 1)
            continue;
        if (((x + p.origin.x) & 1) == 0)
            p.vline (x, y, plot.getBottom() - 1);
        else
            p.dot (x, y);
    }

    // Threshold
    const int ty = yFor (threshold);
    for (int x = plot.getX(); x < plot.getRight(); ++x)
        if (((x + p.origin.x) % 5) < 3)
            p.dot (x, ty);
    const String label = on ? "THRESHOLD" : "GATE OFF";
    const int lw = tiny().width (label);
    const int ly = ty - 7 < plot.getY() ? ty + 3 : ty - 7;
    p.fill ({ plot.getRight() - lw - 4, ly - 1, lw + 4, 7 }, paper);
    p.text (label, plot.getRight() - lw - 2, ly, ink, tiny());
}

//==============================================================================
// ResponseCurveView
//==============================================================================
ResponseCurveView::ResponseCurveView (const String& t, float minD, float maxD, float minH, float maxH)
    : title (t.toUpperCase()), minDb (minD), maxDb (maxD), minHz (minH), maxHz (maxH)
{
    setInterceptsMouseClicks (false, false);
}

Rectangle<int> ResponseCurveView::plotArea() const
{
    return getLocalBounds().withTrimmedLeft (17).withTrimmedRight (4).withTrimmedTop (11).withTrimmedBottom (9);
}

void ResponseCurveView::refresh()
{
    const auto plot = plotArea();
    const int n = jmax (2, plot.getWidth());
    if ((int) freqs.size() != n)
    {
        freqs.resize ((size_t) n);
        dbs.assign ((size_t) n, 0.0);
        for (int i = 0; i < n; ++i)
            freqs[(size_t) i] = minHz * std::pow ((double) maxHz / minHz, (double) i / (n - 1));
    }
    if (compute)
        compute (freqs.data(), dbs.data(), n);
    repaint();
}

int ResponseCurveView::xForHz (float hz, Rectangle<int> r) const
{
    return r.getX() + roundToInt ((float) (r.getWidth() - 1) * std::log (hz / minHz) / std::log (maxHz / minHz));
}

int ResponseCurveView::yForDb (float db, Rectangle<int> r) const
{
    const float t = (jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return r.getBottom() - 1 - roundToInt (t * (float) (r.getHeight() - 1));
}

void ResponseCurveView::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto bounds = getLocalBounds();
    p.frame (bounds);
    p.text (title, 4, 3, ink, tiny());
    if (note.isNotEmpty())
        p.text (tiny().fit (note.toUpperCase(), bounds.getWidth() - tiny().width (title) - 16),
                { 0, 3, bounds.getWidth() - 4, 5 }, Justification::right, ink, tiny());

    const auto plot = plotArea();

    // Grid: decades dotted, other octave-ish lines as sparse dots; 0 dB dashed
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        if (f < minHz || f > maxHz)
            continue;
        const int x = xForHz (f, plot);
        const bool major = f == 100.0f || f == 1000.0f || f == 10000.0f;
        p.dottedV (x, plot.getY(), plot.getBottom() - 1, major ? 2 : 4);
        if (major)
            p.text (f >= 1000.0f ? String ((int) (f / 1000.0f)) + "K" : String ((int) f),
                    { x - 10, plot.getBottom() + 2, 21, 5 }, Justification::centred, ink, tiny());
    }

    const float step = (maxDb - minDb) > 36.0f ? 12.0f : 6.0f;
    for (float db = std::ceil (minDb / step) * step; db <= maxDb + 0.01f; db += step)
    {
        const int y = yForDb (db, plot);
        if (db == 0.0f)
        {
            for (int x = plot.getX(); x < plot.getRight(); ++x)
                if (((x + p.origin.x) % 4) < 2)
                    p.dot (x, y);
        }
        else
        {
            p.dottedH (plot.getX(), plot.getRight() - 1, y, 4);
        }
        p.text ((db > 0 ? "+" : "") + String ((int) db), { 1, y - 2, 14, 5 }, Justification::right, ink, tiny());
    }

    // Curve with a dithered fill underneath
    if (dbs.empty())
        return;
    int prevY = 0;
    for (int i = 0; i < (int) dbs.size(); ++i)
    {
        const int x = plot.getX() + i;
        const int y = yForDb ((float) dbs[(size_t) i], plot);
        if (y + 1 < plot.getBottom())
            p.fill ({ x, y + 1, 1, plot.getBottom() - y - 1 }, ink, Pattern::sparse);

        // 1 px polyline: each column joins up with the previous one
        if (i == 0 || y == prevY) p.dot (x, y);
        else if (y > prevY)       p.vline (x, prevY + 1, y);
        else                      p.vline (x, y, prevY - 1);
        prevY = y;
    }
}

//==============================================================================
// Card
//==============================================================================
Card::Card (const String& t, APVTS* state, const String& powerParam)
    : title (t)
{
    if (state != nullptr && powerParam.isNotEmpty())
    {
        power = std::make_unique<PowerBox>();
        power->setTooltip ("Switch the " + title.toLowerCase() + " on or off");
        addAndMakeVisible (*power);
        powerAttachment = std::make_unique<APVTS::ButtonAttachment> (*state, powerParam, *power);
    }
}

void Card::resized()
{
    if (power != nullptr)
        power->setBounds (4, 2, 15, 9);
}

void Card::paint (Graphics& g)
{
    Pix p (g, *this);
    drawWindow (p, getLocalBounds(), title, status, power != nullptr ? 25 : 5);
}

//==============================================================================
// Icons
//==============================================================================
void drawIcon (Pix& p, Icon icon, int x, int y, Colour c)
{
    static const Sprite gate {
        "........##......",
        "........##......",
        "#..#....##......",
        ".#...#..##......",
        "...#..#.##......",
        "#.#.....##......",
        ".#..##..##.#####",
        "#..#..#.##.#####",
        "..#.#...##......",
        "#....#..##......",
        ".#.#..#.##......",
        "...#....##......",
        "........##......",
        "........##......",
    };
    static const Sprite boost {
        "..############..",
        "..#..........#..",
        "..#.##....##.#..",
        "..#.##....##.#..",
        "..#..........#..",
        "..#.########.#..",
        "..#.#......#.#..",
        "..#.########.#..",
        "..#..........#..",
        "..#....##....#..",
        "..#...####...#..",
        "..#....##....#..",
        "..#..........#..",
        "..############..",
    };
    static const Sprite amp {
        "................",
        "....########....",
        "....#......#....",
        "################",
        "#.#.#.#.#.#.#.##",
        "##.#.#.#.#.#.#.#",
        "#.#.#.#.#.#.#.##",
        "##.#.#.#.#.#.#.#",
        "################",
        "#..............#",
        "#.#.#.#.#.#..#.#",
        "#..............#",
        "################",
    };
    static const Sprite cab {
        "################",
        "#..............#",
        "#..####..####..#",
        "#.#....##....#.#",
        "#.#.##.##.##.#.#",
        "#.#.##.##.##.#.#",
        "#.#....##....#.#",
        "#..####..####..#",
        "#..####..####..#",
        "#.#....##....#.#",
        "#.#.##.##.##.#.#",
        "#.#.##.##.##.#.#",
        "#.#....##....#.#",
        "#..####..####..#",
        "#..............#",
        "################",
    };
    static const Sprite delay {
        "##..............",
        "##..............",
        "##..............",
        "##..##..........",
        "##..##..........",
        "##..##..........",
        "##..##..##......",
        "##..##..##......",
        "##..##..##......",
        "##..##..##..##..",
        "##..##..##..##..",
        "##..##..##..##..",
        "................",
        "################",
    };
    static const Sprite anvil {
        "...........#....",
        "..........#..#..",
        "............#...",
        "................",
        "...#############",
        "################",
        ".###############",
        "....###########.",
        "......#######...",
        "......#######...",
        ".....#########..",
        "....###########.",
    };

    switch (icon)
    {
        case Icon::gate:   p.sprite (gate, x, y + 1, c); break;
        case Icon::boost:  p.sprite (boost, x, y + 1, c); break;
        case Icon::amp:    p.sprite (amp, x, y + 1, c); break;
        case Icon::cab:    p.sprite (cab, x, y, c); break;
        case Icon::delay:  p.sprite (delay, x, y + 1, c); break;
        case Icon::anvil:  p.sprite (anvil, x, y, c); break;
        case Icon::reverb:
        {
            // Concentric arcs spreading from a source in the lower-left corner
            Graphics::ScopedSaveState s (p.g);
            p.g.reduceClipRegion (x + 1, y, 15, 15);
            for (int r : { 5, 9, 13 })
                p.ring (x + 1, y + 14, r, c);
            p.fill ({ x, y + 13, 2, 2 }, c);
            break;
        }
    }
}

//==============================================================================
// ChainTile
//==============================================================================
ChainTile::ChainTile (const String& n, Icon ic, APVTS& state, const String& powerParam)
    : name (n.toUpperCase()), icon (ic), attachment (state, powerParam, power)
{
    power.setTooltip ("Switch the " + n.toLowerCase() + " on or off");
    addAndMakeVisible (power);
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip ("Open the " + n.toLowerCase() + " page");
    lastOn = power.getToggleState();
}

void ChainTile::resized()
{
    power.setBounds (getWidth() - 18, 3, 15, 9);
}

void ChainTile::mouseUp (const MouseEvent& e)
{
    if (e.mouseWasClicked() && onSelect)
        onSelect();
}

void ChainTile::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto r = getLocalBounds();
    const bool on = power.getToggleState();
    const auto fg = selected ? paper : ink, bg = selected ? ink : paper;

    p.fill (r, bg);
    p.frame (r, ink);
    if (isMouseOver (true) && ! selected)
        p.dottedFrame (r.reduced (2));

    drawIcon (p, icon, 4, 4, fg);
    if (! on)
        p.fill ({ 4, 4, 16, 16 }, bg, Pattern::checker);

    p.text (name, 4, 23, fg);
    p.text (tiny().fit (on ? summary : String ("OFF"), r.getWidth() - 8), 4, 33, fg, tiny());
}

//==============================================================================
// PromptOverlay
//==============================================================================
PromptOverlay::PromptOverlay()
{
    setVisible (false);
    setWantsKeyboardFocus (true);

    field.onReturn = [this] { ok.triggerClick(); };
    field.onEscape = [this] { close(); };
    addChildComponent (field);

    ok.getProperties().set ("primary", true);
    ok.onClick = [this]
    {
        auto cb = callback;
        const auto text = field.getText();
        close();
        if (cb)
            cb (text);
    };
    cancel.onClick = [this] { close(); };
    addAndMakeVisible (ok);
    addAndMakeVisible (cancel);
}

void PromptOverlay::show (const String& t, const String& m, bool withText, const String& initialText,
                          const String& okLabel, std::function<void (const String&)> onOk, bool showCancel)
{
    title = t;
    message = m;
    hasText = withText;
    callback = std::move (onOk);
    ok.setButtonText (okLabel.toUpperCase());
    cancel.setVisible (showCancel);
    field.setVisible (withText);
    field.setText (initialText, true);

    setVisible (true);
    toFront (true);
    resized();
    repaint();

    if (withText)
        field.grabKeyboardFocus();
    else
        grabKeyboardFocus();
}

void PromptOverlay::close()
{
    setVisible (false);
    callback = nullptr;
}

bool PromptOverlay::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::escapeKey)  { close(); return true; }
    if (key == KeyPress::returnKey)  { ok.triggerClick(); return true; }
    return false;
}

Rectangle<int> PromptOverlay::panelBounds() const
{
    const int lines = jmax (1, font().wrap (message, 220).size());
    const int h = kTitleBarH + 8 + lines * 10 + (hasText ? 20 : 4) + 22;
    return Rectangle<int> (240, h).withCentre (getLocalBounds().getCentre());
}

void PromptOverlay::resized()
{
    auto r = panelBounds().reduced (1).withTrimmedTop (kTitleBarH).reduced (8, 6);
    auto buttons = r.removeFromBottom (15);
    ok.setBounds (buttons.removeFromRight (58));
    buttons.removeFromRight (6);
    cancel.setBounds (buttons.removeFromRight (58));
    if (hasText)
    {
        r.removeFromBottom (6);
        field.setBounds (r.removeFromBottom (13));
    }
}

void PromptOverlay::paint (Graphics& g)
{
    Pix p (g, *this);
    p.fill (getLocalBounds(), paper, Pattern::checker);

    const auto panel = panelBounds();
    p.fill (panel.translated (3, 3), ink, Pattern::checker);
    drawWindow (p, panel, title);
    p.paragraph (message, panel.reduced (1).withTrimmedTop (kTitleBarH).reduced (8, 6));
}

//==============================================================================
// PresetBar
//==============================================================================
void PresetBar::NameDisplay::paintButton (Graphics& g, bool highlighted, bool)
{
    Pix p (g, *this);
    const auto r = getLocalBounds();
    p.fill (r, paper);
    p.frame (r);
    if (highlighted)
        p.frame (r.reduced (1));

    const int ty = (r.getHeight() - 7) / 2;
    const auto num = String (number).paddedLeft ('0', 2);
    const Rectangle<int> numBox (3, ty, tiny().width (num) + 6, 7);
    p.fill (numBox);
    p.text (num, numBox, Justification::centred, paper, tiny());

    int right = r.getRight() - 10;
    p.triangle ({ right + 1, ty + 2, 5, 3 }, 'd');
    if (dirty)
    {
        const int tw = tiny().width ("EDITED") + 4;
        right -= tw + 4;
        p.fill ({ right, ty, tw, 7 });
        p.text ("EDITED", right + 2, ty + 1, paper, tiny());
    }

    const int nameX = numBox.getRight() + 5;
    p.text (font().fit (presetName, right - nameX - 4), nameX, ty);
}

void PresetBar::ArrowButton::paintButton (Graphics& g, bool highlighted, bool down)
{
    Pix p (g, *this);
    const auto b = getLocalBounds();
    const auto face = PixelLookAndFeel::buttonFace (b, down);
    if (! down)
    {
        p.hline (b.getX() + 1, b.getRight() - 1, b.getBottom() - 1);
        p.vline (b.getRight() - 1, b.getY() + 1, b.getBottom() - 1);
    }
    p.fill (face, paper);
    p.frame (face);
    if (highlighted)
        p.frame (face.reduced (1));
    p.triangle ({ face.getCentreX() - 2, face.getCentreY() - 3, 4, 7 }, right ? 'r' : 'l');
}

PresetBar::PresetBar (PresetManager& manager, PromptOverlay& overlay)
    : presets (manager), prompt (overlay)
{
    for (auto* b : std::initializer_list<Component*> { &prev, &next, &nameButton, &saveButton, &menuButton })
        addAndMakeVisible (b);

    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    nameButton.setTooltip ("Browse presets");
    saveButton.setTooltip ("Save the current sound as a user preset");
    menuButton.setTooltip ("Save as, rename, delete, reset, presets folder");
    for (auto* b : std::initializer_list<Component*> { &prev, &next, &nameButton, &saveButton, &menuButton })
        b->setMouseCursor (MouseCursor::PointingHandCursor);

    prev.onClick = [this] { presets.loadPrevious(); refresh(); };
    next.onClick = [this] { presets.loadNext(); refresh(); };
    nameButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { save(); };
    menuButton.onClick = [this] { showActionsMenu(); };
    refresh();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (14));
    r.removeFromLeft (1);
    next.setBounds (r.removeFromLeft (14));
    r.removeFromLeft (4);
    menuButton.setBounds (r.removeFromRight (16));
    r.removeFromRight (3);
    saveButton.setBounds (r.removeFromRight (34));
    r.removeFromRight (4);
    nameButton.setBounds (r.withTrimmedBottom (1));
}

void PresetBar::refresh()
{
    const auto name = presets.getCurrentName();
    const bool dirty = presets.isDirty();
    const int number = presets.getCurrentIndex() + 1;
    if (name != nameButton.presetName || dirty != nameButton.dirty || number != nameButton.number)
    {
        nameButton.presetName = name;
        nameButton.dirty = dirty;
        nameButton.number = number;
        nameButton.repaint();
    }
}

void PresetBar::showPresetMenu()
{
    presets.refresh();
    PopupMenu menu;
    const auto& entries = presets.getEntries();

    menu.addSectionHeader ("Factory");
    bool anyUser = false;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].factory)
            menu.addItem (i + 1, String (i + 1).paddedLeft ('0', 2) + "  " + entries[(size_t) i].name, true, i == presets.getCurrentIndex());
        else
            anyUser = true;

    menu.addSectionHeader ("User");
    if (! anyUser)
        menu.addItem (-1, "No saved presets yet", false);
    for (int i = 0; i < (int) entries.size(); ++i)
        if (! entries[(size_t) i].factory)
            menu.addItem (i + 1, String (i + 1).paddedLeft ('0', 2) + "  " + entries[(size_t) i].name, true, i == presets.getCurrentIndex());

    menu.showMenuAsync (pixelMenuOptions (nameButton).withMinimumWidth (nameButton.getWidth()),
                        [this] (int result)
                        {
                            if (result > 0)
                            {
                                presets.load (result - 1);
                                refresh();
                            }
                        });
}

void PresetBar::showActionsMenu()
{
    const bool isUser = ! presets.isCurrentFactory() && presets.getCurrentIndex() >= 0;
    PopupMenu menu;
    menu.addItem (1, "Save as new preset...");
    menu.addItem (2, "Rename...", isUser);
    menu.addItem (3, "Delete", isUser);
    menu.addSeparator();
    menu.addItem (4, "Reset to default sound");
    menu.addItem (5, "Open presets folder");

    menu.showMenuAsync (pixelMenuOptions (menuButton), [this] (int result)
    {
        const auto name = presets.getCurrentName();
        switch (result)
        {
            case 1: save(); break;
            case 2:
                prompt.show ("Rename preset", "Enter a new name.", true, name, "Rename", [this] (const String& n)
                {
                    if (! presets.renameCurrent (n))
                        prompt.show ("Couldn't rename", "A preset with that name may already exist.", false, {}, "OK", nullptr, false);
                    refresh();
                });
                break;
            case 3:
                prompt.show ("Delete preset?", "\"" + name + "\" will be moved to the trash.", false, {}, "Delete",
                             [this] (const String&) { presets.deleteCurrent(); refresh(); });
                break;
            case 4:
                presets.resetToDefaults();
                refresh();
                break;
            case 5:
            {
                auto folder = presets.getUserFolder();
                folder.createDirectory();
                folder.startAsProcess();
                break;
            }
            default: break;
        }
    });
}

void PresetBar::save()
{
    const auto suggestion = presets.isCurrentFactory() ? presets.getCurrentName() + " (my edit)" : presets.getCurrentName();

    prompt.show ("Save preset", "Saved to Documents / Forge Amp / Presets.", true, suggestion, "Save", [this] (const String& raw)
    {
        const auto name = PresetManager::sanitiseName (raw);
        if (name.isEmpty())
            return;

        auto doSave = [this, name]
        {
            if (! presets.saveUserPreset (name))
                prompt.show ("Couldn't save", "Check that the presets folder is writable.", false, {}, "OK", nullptr, false);
            refresh();
        };

        if (presets.userPresetExists (name))
            prompt.show ("Replace preset?", "\"" + name + "\" already exists.", false, {}, "Replace",
                         [doSave] (const String&) { doSave(); });
        else
            doSave();
    });
}
} // namespace forge
