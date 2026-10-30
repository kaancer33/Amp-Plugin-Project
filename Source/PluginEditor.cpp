#include "PluginEditor.h"

namespace forge
{
using namespace juce;
using namespace pixel;

namespace
{
    constexpr int kTop = 28, kColW = 38, kColH = 276;
    constexpr int kInX = 6, kOutX = MainView::W - 6 - kColW;
    constexpr int kTileX = 51, kTileW = 60, kTileH = 42, kTileStep = 70;
    constexpr int kWireY = kTop + kTileH / 2;

    // Which page each chain tile opens.
    constexpr int tilePage[6] { 0, 0, 1, 2, 3, 3 };

    String paramText (ForgeAmpProcessor& p, const char* id)
    {
        return p.apvts.getParameter (id)->getCurrentValueAsText();
    }

    /** The x ranges of the wires: IN -> tiles -> OUT. */
    std::array<Range<int>, 7> wireSpans()
    {
        std::array<Range<int>, 7> spans;
        spans[0] = { kInX + kColW, kTileX };
        for (int i = 1; i < 6; ++i)
            spans[(size_t) i] = { kTileX + (i - 1) * kTileStep + kTileW, kTileX + i * kTileStep };
        spans[6] = { kTileX + 5 * kTileStep + kTileW, kOutX };
        return spans;
    }
}

MainView::MainView (ForgeAmpProcessor& p)
    : proc (p),
      presetBar (p.getPresetManager(), prompt),
      inputKnob (p.apvts, ids::inputGain, "Level", "Input level into the rig. Aim for peaks around -12 to -6 dB.", true, 9),
      outputKnob (p.apvts, ids::outputGain, "Level", "Final output level.", true, 9),
      prePage (p), ampPage (p), cabPage (p, prompt), fxPage (p)
{
    markAsCanvas (*this);
    setOpaque (true);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (viewButton);
    addAndMakeVisible (inputKnob);
    addAndMakeVisible (outputKnob);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    inMeter.setTooltip ("Input level before the gate. Click to clear the CLIP light.");
    outMeter.setTooltip ("Output level. Click to clear the CLIP light.");
    viewButton.setTooltip ("Zoom and version");
    viewButton.setMouseCursor (MouseCursor::PointingHandCursor);
    viewButton.onClick = [this] { showViewMenu(); };

    const struct { const char* name; Icon icon; const char* param; } tileInfo[6] {
        { "Gate",   Icon::gate,   ids::gateOn },
        { "Boost",  Icon::boost,  ids::boostOn },
        { "Amp",    Icon::amp,    ids::ampOn },
        { "Cab",    Icon::cab,    ids::cabOn },
        { "Delay",  Icon::delay,  ids::delayOn },
        { "Reverb", Icon::reverb, ids::reverbOn },
    };

    for (int i = 0; i < 6; ++i)
    {
        auto& tile = tiles[(size_t) i];
        tile = std::make_unique<ChainTile> (tileInfo[i].name, tileInfo[i].icon, p.apvts, tileInfo[i].param);
        tile->onSelect = [this, i] { showPage (tilePage[i]); };
        tile->setSummary (summaryFor (i));
        addAndMakeVisible (*tile);
    }

    pages = { &prePage, &ampPage, &cabPage, &fxPage };
    for (auto* page : pages)
        addChildComponent (page);

    addChildComponent (prompt);   // last, so it sits on top
    addMouseListener (this, true);

    showPage (jlimit (0, 3, proc.uiPage));
    startTimerHz (30);
}

MainView::~MainView()
{
    removeMouseListener (this);
    stopTimer();
}

void MainView::showPage (int page)
{
    currentPage = jlimit (0, 3, page);
    proc.uiPage = currentPage;
    for (int i = 0; i < 4; ++i)
        pages[(size_t) i]->setVisible (i == currentPage);
    for (int i = 0; i < 6; ++i)
        tiles[(size_t) i]->setSelected (tilePage[i] == currentPage);
    pages[(size_t) currentPage]->refresh();
}

String MainView::summaryFor (int tile) const
{
    auto& p = proc;
    switch (tile)
    {
        case 0: return paramText (p, ids::gateThreshold);
        case 1: return "Drive " + paramText (p, ids::boostDrive);
        case 2: return paramText (p, ids::ampModel) + " " + paramText (p, ids::ampGain);
        case 3: return paramText (p, ids::cabModel);
        case 4: return (p.apvts.getRawParameterValue (ids::delaySync)->load() >= 0.5f ? paramText (p, ids::delayDivision)
                                                                                     : paramText (p, ids::delayTime))
                       + " " + paramText (p, ids::delayMix);
        case 5: return paramText (p, ids::reverbDecay) + " " + paramText (p, ids::reverbMix);
        default: return {};
    }
}

void MainView::updateStatus()
{
    String text;
    auto* under = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    if (under != nullptr && isParentOf (under))
    {
        for (auto* c = under; c != nullptr && c != this; c = c->getParentComponent())
            if (auto* client = dynamic_cast<TooltipClient*> (c))
                if (text = client->getTooltip(); text.isNotEmpty())
                    break;
    }

    if (text != status)
    {
        status = text;
        repaint (statusArea());
    }
}

void MainView::timerCallback()
{
    const float inPeak = proc.inputMeter.take();
    const float outPeak = proc.outputMeter.take();
    inMeter.pushLevel (inPeak);
    outMeter.pushLevel (outPeak);
    prePage.pushInputLevel (inPeak);

    for (auto& t : tiles)
        t->refresh();

    presetBar.refresh();
    pages[(size_t) currentPage]->refresh();

    ++tick;
    if (tick % 6 == 0)
        for (int i = 0; i < 6; ++i)
            tiles[(size_t) i]->setSummary (summaryFor (i));

    // Signal runs along the wires while there's audio at the output
    const bool nowSignal = outPeak > 0.003f;
    if (nowSignal || nowSignal != signal)
    {
        signal = nowSignal;
        if (tick % 2 == 0)
            wirePhase = (wirePhase + 1) & 3;
        for (auto span : wireSpans())
            repaint (span.getStart(), kWireY - 3, span.getLength() + 1, 7);
    }

    if (tick % 15 == 0)
    {
        cursorOn = ! cursorOn;
        if (status.isEmpty())
            repaint (statusArea());
    }
}

void MainView::showViewMenu()
{
    PopupMenu menu;
    menu.addSectionHeader ("Zoom");
    const float current = proc.uiScale;
    const std::array<float, 6> zooms { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f };
    for (int i = 0; i < (int) zooms.size(); ++i)
    {
        const float z = zooms[(size_t) i];
        menu.addItem (i + 1, String (z, z == std::floor (z) ? 0 : 1) + "x   " + String (roundToInt ((float) W * z))
                                 + " x " + String (roundToInt ((float) H * z)),
                      true, std::abs (current - z) < 0.02f);
    }
    menu.addSeparator();
    menu.addItem (100, String (info::productName) + " " + info::version, false);
    menu.addItem (101, "Built " + String (info::buildDate), false);

    menu.showMenuAsync (pixelMenuOptions (viewButton), [this, zooms] (int r)
    {
        if (r >= 1 && r <= (int) zooms.size() && onScaleRequested)
            onScaleRequested (zooms[(size_t) r - 1]);
    });
}

void MainView::resized()
{
    presetBar.setBounds (148, 5, 260, 15);
    viewButton.setBounds (kOutX, 5, kColW, 15);

    inputKnob.setBounds (kInX + 1, kTop + 14, kColW - 2, Knob::heightFor (9));
    outputKnob.setBounds (kOutX + 1, kTop + 14, kColW - 2, Knob::heightFor (9));
    inMeter.setBounds (kInX + 2, kTop + 64, kColW - 6, kColH - 68);
    outMeter.setBounds (kOutX + 2, kTop + 64, kColW - 6, kColH - 68);

    for (int i = 0; i < 6; ++i)
        tiles[(size_t) i]->setBounds (kTileX + i * kTileStep, kTop, kTileW, kTileH);

    for (auto* page : pages)
        page->setBounds (50, 76, 412, 228);

    prompt.setBounds (getLocalBounds());
}

void MainView::paint (Graphics& g)
{
    Pix p (g, *this);
    p.fill (getLocalBounds(), paper);

    // Header: anvil, wordmark, AMP tag, double rule
    drawIcon (p, Icon::anvil, 8, 5);
    p.text ("FORGE", 28, 5, ink, font(), 2);
    const Rectangle<int> tag (93, 7, font().width ("AMP") + 6, 11);
    p.fill (tag);
    p.text ("AMP", tag.getX() + 3, tag.getY() + 2, paper);
    p.hline (0, W - 1, 23);
    p.dottedH (0, W - 1, 25);

    // I/O columns
    drawWindow (p, { kInX, kTop, kColW, kColH }, "In");
    drawWindow (p, { kOutX, kTop, kColW, kColH }, "Out");

    // Wires between the chain tiles, with the signal marching along them
    for (auto span : wireSpans())
    {
        const int x0 = span.getStart(), x1 = span.getEnd() - 1;
        for (int x = x0; x <= x1 - 1; ++x)
            if (! signal || ((x - wirePhase) & 3) != 0)
                p.dot (x, kWireY);
        p.dot (x1 - 1, kWireY - 1);
        p.dot (x1 - 1, kWireY + 1);
        p.dot (x1 - 2, kWireY - 2);
        p.dot (x1 - 2, kWireY + 2);
        p.dot (x1, kWireY);
    }

    // Status bar: hover help, or a blinking prompt
    const auto bar = statusArea();
    p.hline (0, W - 1, bar.getY());
    const auto version = "V" + String (info::version);
    const int vw = tiny().width (version);
    p.text (version, W - 14 - vw, bar.getY() + 5, ink, tiny());
    if (status.isNotEmpty())
    {
        p.text (font().fit (status, W - 30 - vw), 6, bar.getY() + 4);
    }
    else
    {
        const int w = p.text ("Ready.", 6, bar.getY() + 4);
        if (cursorOn)
            p.fill ({ 6 + w + 3, bar.getY() + 4, 5, 7 });
    }
}
} // namespace forge

//==============================================================================
ForgeAmpEditor::ForgeAmpEditor (ForgeAmpProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    using forge::MainView;

    // Read the saved zoom first: setResizeLimits() resizes the (still empty) editor, and
    // resized() writes the zoom back to the processor.
    const float zoom = juce::jlimit (1.0f, 4.0f, p.uiScale);

    setLookAndFeel (&lookAndFeel);
    view.setCachedComponentImage (new forge::pixel::CanvasCache (view));
    addAndMakeVisible (view);

    view.onScaleRequested = [this] (float z)
    {
        setSize (juce::roundToInt ((float) MainView::W * z), juce::roundToInt ((float) MainView::H * z));
    };

    setResizable (true, true);
    setResizeLimits (MainView::W, MainView::H, MainView::W * 4, MainView::H * 4);
    getConstrainer()->setFixedAspectRatio ((double) MainView::W / (double) MainView::H);
    setSize (juce::roundToInt ((float) MainView::W * zoom), juce::roundToInt ((float) MainView::H * zoom));
}

ForgeAmpEditor::~ForgeAmpEditor()
{
    setLookAndFeel (nullptr);
}

void ForgeAmpEditor::resized()
{
    const float zoom = (float) getWidth() / (float) forge::MainView::W;
    view.setTransform (juce::AffineTransform::scale (zoom));
    view.setBounds (0, 0, forge::MainView::W, forge::MainView::H);
    proc.uiScale = zoom;
}
