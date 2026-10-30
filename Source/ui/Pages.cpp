#include "Pages.h"
#include "../PluginProcessor.h"

namespace forge
{
using namespace juce;
using namespace pixel;

namespace
{
    float value (ForgeAmpProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    /** Lays out components in a centred row of equal cells. */
    void layoutRow (Rectangle<int> area, std::initializer_list<Component*> items, int cellWidth)
    {
        const int total = cellWidth * (int) items.size();
        int x = area.getX() + (area.getWidth() - total) / 2;
        for (auto* c : items)
        {
            c->setBounds (x, area.getY(), cellWidth, area.getHeight());
            x += cellWidth;
        }
    }

    std::complex<double> onePoleLP (double fc, double f) { return 1.0 / std::complex<double> (1.0, f / fc); }
    std::complex<double> onePoleHP (double fc, double f) { const std::complex<double> s (0.0, f / fc); return s / (1.0 + s); }

    /** A speaker seen from the front: surround, cone edge, optional cone texture, dust cap. */
    void drawSpeaker (Pix& p, int cx, int cy, int r, bool textured, int cap = 0)
    {
        p.disc (cx, cy, r, paper);
        p.ring (cx, cy, r);
        if (r >= 6)
            p.ring (cx, cy, r - 2);
        if (textured && r >= 10)
            p.disc (cx, cy, r - 4, ink, Pattern::sparse);
        if (cap <= 0)
            cap = jmax (1, r / 3);
        p.disc (cx, cy, cap + 1, paper);
        p.disc (cx, cy, cap);
    }

    /** A frame with its corner pixels knocked out, a thickness of t pixels. */
    void notchedFrame (Pix& p, Rectangle<int> r, int t)
    {
        for (int i = 0; i < t; ++i)
        {
            const auto f = r.reduced (i);
            p.hline (f.getX() + 1, f.getRight() - 2, f.getY());
            p.hline (f.getX() + 1, f.getRight() - 2, f.getBottom() - 1);
            p.vline (f.getX(), f.getY() + 1, f.getBottom() - 2);
            p.vline (f.getRight() - 1, f.getY() + 1, f.getBottom() - 2);
        }
    }

    void screw (Pix& p, int cx, int cy)
    {
        p.disc (cx, cy, 2, paper);
        p.ring (cx, cy, 2);
        p.hline (cx - 1, cx + 1, cy);
    }
}

void TextBlock::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto& f = small ? tiny() : font();
    if (! centred)
    {
        p.paragraph (text, getLocalBounds(), ink, f);
        return;
    }
    int y = 0;
    for (const auto& line : f.wrap (text, getWidth()))
    {
        p.text (line, { 0, y, getWidth(), f.ascent }, Justification::centred, ink, f);
        y += f.lineHeight;
    }
}

//==============================================================================
// PRE: gate + boost
//==============================================================================
PrePage::PrePage (ForgeAmpProcessor& p)
    : proc (p),
      gateCard ("Noise gate", &p.apvts, ids::gateOn),
      boostCard ("Boost", &p.apvts, ids::boostOn),
      gateThreshold (p.apvts, ids::gateThreshold, "Threshold", "Level the signal must exceed to open the gate."),
      gateRelease (p.apvts, ids::gateRelease, "Release", "How quickly the gate closes after you stop playing."),
      boostDrive (p.apvts, ids::boostDrive, "Drive", "Clipping amount of the overdrive."),
      boostTone (p.apvts, ids::boostTone, "Tone", "Brightness of the overdrive."),
      boostLevel (p.apvts, ids::boostLevel, "Level", "Output of the overdrive into the amp.", true),
      boostCurve ("Voicing", -12.0f, 18.0f, 40.0f, 16000.0f)
{
    addAndMakeVisible (gateCard);
    addAndMakeVisible (boostCard);
    for (auto* c : std::initializer_list<Component*> { &gateThreshold, &gateRelease, &scope, &gateHint })
        gateCard.addAndMakeVisible (c);
    for (auto* c : std::initializer_list<Component*> { &boostDrive, &boostTone, &boostLevel, &boostCurve, &boostHint })
        boostCard.addAndMakeVisible (c);

    gateHint.setText ("Set the threshold just above the hiss you hear between notes. Dithered stripes "
                      "show where the gate is shut. A short release tightens palm mutes.");
    boostHint.setText ("Tube Screamer style drive in front of the amp. Tight metal rhythm: Drive 0, "
                       "Level up. Blues lead: more Drive, Level near 0.");

    boostCurve.setCompute ([this] (const double* f, double* out, int n)
    {
        // Small-signal voicing: unity path plus the band-passed clipping leg (shown at a
        // typical clipped level rather than its raw gain), then tone and level.
        const double drive = value (proc, ids::boostDrive) * 0.1;
        const double tone  = value (proc, ids::boostTone) * 0.1;
        const double level = value (proc, ids::boostLevel);
        const double hump  = 1.0 + 3.0 * drive;
        const double fbFc  = 1.0 / (kTwoPi * (51.0e3 + drive * 500.0e3) * 51.0e-12);
        const double toneFc = 900.0 * std::pow (10.0, tone);
        for (int i = 0; i < n; ++i)
        {
            const auto h = (1.0 + hump * onePoleHP (720.0, f[i]) * onePoleLP (fbFc, f[i])) * onePoleLP (toneFc, f[i]);
            out[i] = gainToDb (std::abs (h)) + level;
        }
    });
}

void PrePage::resized()
{
    gateCard.setBounds (0, 0, 203, getHeight());
    boostCard.setBounds (209, 0, 203, getHeight());
    const int knobH = Knob::heightFor (12);

    auto g = gateCard.getBody();
    layoutRow (g.removeFromTop (knobH), { &gateThreshold, &gateRelease }, 80);
    g.removeFromTop (4);
    scope.setBounds (g.removeFromTop (92));
    g.removeFromTop (6);
    gateHint.setBounds (g);

    auto b = boostCard.getBody();
    layoutRow (b.removeFromTop (knobH), { &boostDrive, &boostTone, &boostLevel }, 62);
    b.removeFromTop (4);
    boostCurve.setBounds (b.removeFromTop (92));
    b.removeFromTop (6);
    boostHint.setBounds (b);
}

void PrePage::pushInputLevel (float peak)
{
    scope.push (peak, proc.getGateGain(), value (proc, ids::gateThreshold), gateCard.isOn());
}

void PrePage::refresh()
{
    const bool gateOn = gateCard.isOn();
    gateThreshold.setDimmed (! gateOn);
    gateRelease.setDimmed (! gateOn);
    gateCard.setStatus (! gateOn ? "Bypassed" : proc.getGateGain() > 0.5f ? "Open" : "Shut");

    const bool boostOn = boostCard.isOn();
    for (auto* k : { &boostDrive, &boostTone, &boostLevel })
        k->setDimmed (! boostOn);
    boostCard.setStatus (boostOn ? "Into amp" : "Bypassed");

    const float now[3] { value (proc, ids::boostDrive), value (proc, ids::boostTone), value (proc, ids::boostLevel) };
    if (now[0] != lastBoost[0] || now[1] != lastBoost[1] || now[2] != lastBoost[2])
    {
        std::copy (now, now + 3, lastBoost);
        boostCurve.refresh();
    }
}

//==============================================================================
// AMP
//==============================================================================
void AmpPage::Faceplate::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto r = getLocalBounds();
    p.fill (r, paper);
    notchedFrame (p, r, 2);
    for (auto pt : { Point<int> (7, 7), Point<int> (r.getRight() - 8, 7),
                     Point<int> (7, r.getBottom() - 8), Point<int> (r.getRight() - 8, r.getBottom() - 8) })
        screw (p, pt.x, pt.y);

    // Vent across the control section
    p.fill ({ 104, 6, r.getWidth() - 120, 5 }, ink, Pattern::diagonal);

    // Logo plate, voice, model line
    const Rectangle<int> badge (12, 13, font().width ("FORGE") + 6, 11);
    p.fill (badge);
    p.text ("FORGE", badge.getX() + 3, badge.getY() + 2, paper);
    p.text (voiceName.toUpperCase(), 12, 30, ink, font(), 2);
    p.text ("TUBE PREAMP", 12, 49, ink, tiny());

    // Jewel pilot light beside the power switch
    const Point<int> jc (50, 72);
    for (int dy = -4; dy <= 4; ++dy)
    {
        const int hw = 4 - std::abs (dy);
        if (on)
            p.hline (jc.x - hw, jc.x + hw, jc.y + dy);
        else
        {
            p.dot (jc.x - hw, jc.y + dy);
            p.dot (jc.x + hw, jc.y + dy);
        }
    }
    if (on)
    {
        p.dot (jc.x - 1, jc.y - 1, paper);
        for (auto d : { Point<int> (-1, -1), Point<int> (1, -1), Point<int> (-1, 1), Point<int> (1, 1) })
            p.line (jc.x + d.x * 5, jc.y + d.y * 5, jc.x + d.x * 6, jc.y + d.y * 6);
    }
    p.text ("POWER", 12, 82, ink, tiny());
    p.text ("PILOT", 40, 82, ink, tiny());

    p.dottedV (98, 12, r.getBottom() - 13);

    // Silk-screened groups under the knobs
    auto bracket = [&p] (int x0, int x1, int y, const String& label)
    {
        p.hline (x0, x1, y);
        p.vline (x0, y - 2, y);
        p.vline (x1, y - 2, y);
        const int w = tiny().width (label);
        const int lx = (x0 + x1 - w) / 2;
        p.fill ({ lx - 3, y - 2, w + 6, 5 }, paper);
        p.text (label, lx, y - 2, ink, tiny());
    };
    bracket (108, 104 + 4 * 43 - 5, 84, "PREAMP");
    bracket (104 + 4 * 43 + 4, 104 + 7 * 43 - 5, 84, "POWER AMP");
}

AmpPage::AmpPage (ForgeAmpProcessor& p)
    : proc (p),
      voices (p.apvts, ids::ampModel, false),
      powerAttachment (p.apvts, ids::ampOn, power),
      gain     (p.apvts, ids::ampGain,     "Gain",     "Preamp gain: how hard the tube stages are driven.", false, 12, true),
      bass     (p.apvts, ids::ampBass,     "Bass",     "Tone stack bass. The tone controls interact like the real circuit.", false, 12, true),
      mid      (p.apvts, ids::ampMid,      "Middle",   "Tone stack middle.", false, 12, true),
      treble   (p.apvts, ids::ampTreble,   "Treble",   "Tone stack treble.", false, 12, true),
      presence (p.apvts, ids::ampPresence, "Presence", "Upper-mid and treble emphasis in the power amp.", true, 12, true),
      depth    (p.apvts, ids::ampDepth,    "Depth",    "Low-end resonance in the power amp.", true, 12, true),
      master   (p.apvts, ids::ampMaster,   "Master",   "Power amp level. Higher settings add power-tube saturation and sag.", false, 12, true),
      toneCard ("Tone stack response"),
      toneCurve ("Bass / mid / treble + presence / depth", -24.0f, 18.0f, 30.0f, 16000.0f)
{
    addAndMakeVisible (voices);
    addAndMakeVisible (description);
    addAndMakeVisible (plate);
    addAndMakeVisible (toneCard);
    toneCard.addAndMakeVisible (toneCurve);
    plate.addAndMakeVisible (power);
    power.setTooltip ("Switch the amp on or off");
    voices.setTooltip ("Amp voice");
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
        plate.addAndMakeVisible (k);

    voices.itemNote = [] (int i) { return String (ampVoices()[(size_t) jlimit (0, 2, i)].description); };

    toneCurve.setCompute ([this] (const double* f, double* out, int n)
    {
        AmpEngine::Controls c;
        c.bass = lastTone[1] * 0.1f; c.mid = lastTone[2] * 0.1f; c.treble = lastTone[3] * 0.1f;
        c.presence = lastTone[4] * 0.1f; c.depth = lastTone[5] * 0.1f;
        AmpEngine::displayResponseDb (lastVoice, c, f, out, n);
    });

    refresh();
}

void AmpPage::resized()
{
    voices.setBounds (0, 0, 168, 15);
    description.setBounds (176, 0, getWidth() - 176, 20);
    plate.setBounds (0, 22, getWidth(), 100);
    power.setBounds (12, 66, 25, 13);
    int x = 104;
    for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
    {
        k->setBounds (x, 18, 43, Knob::heightFor (12));
        x += 43;
    }
    toneCard.setBounds (0, 127, getWidth(), getHeight() - 127);
    toneCurve.setBounds (toneCard.getBody());
}

void AmpPage::refresh()
{
    const int voice = jlimit (0, (int) ampVoices().size() - 1, (int) value (proc, ids::ampModel));
    const bool on = power.getToggleState();
    const std::array<float, 7> tone { value (proc, ids::ampGain), value (proc, ids::ampBass), value (proc, ids::ampMid),
                                      value (proc, ids::ampTreble), value (proc, ids::ampPresence),
                                      value (proc, ids::ampDepth), value (proc, ids::ampMaster) };

    if (voice != lastVoice || on != plate.on)
    {
        plate.voiceName = ampVoices()[(size_t) voice].name;
        plate.on = on;
        plate.repaint();
        description.setText (ampVoices()[(size_t) voice].description);
        for (auto* k : { &gain, &bass, &mid, &treble, &presence, &depth, &master })
            k->setDimmed (! on);
    }

    if (voice != lastVoice || tone != lastTone)
    {
        lastVoice = voice;
        lastTone = tone;
        toneCard.setStatus (ampVoices()[(size_t) voice].name);
        toneCurve.refresh();
    }
}

//==============================================================================
// CAB
//==============================================================================
void CabPage::CabArt::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto bounds = getLocalBounds();
    const int cx = bounds.getCentreX(), cy = bounds.getCentreY();

    auto cabinet = [&p] (Rectangle<int> box, Pattern grille, bool piping)
    {
        p.fill (box.reduced (2), ink, grille);
        notchedFrame (p, box, 2);
        if (piping)
            p.frame (box.reduced (4));
        // corner protectors
        for (auto c : { box.getTopLeft(), box.getTopRight().translated (-4, 0),
                        box.getBottomLeft().translated (0, -4), box.getBottomRight().translated (-4, -4) })
            p.fill ({ c.x, c.y, 4, 4 });
    };

    switch (irLoaded && model == CabSim::kUserIR ? -1 : (model == CabSim::kUserIR ? -2 : model))
    {
        case 0:   // 4x12 modern: black grille
        {
            const auto box = Rectangle<int> (62, 62).withCentre ({ cx, cy });
            cabinet (box, Pattern::dots, false);
            for (int i = 0; i < 4; ++i)
                drawSpeaker (p, cx + ((i & 1) ? 14 : -14), cy + ((i & 2) ? 14 : -14), 12, false);
            break;
        }
        case 1:   // 4x12 vintage: basketweave, piping, slant line
        {
            const auto box = Rectangle<int> (62, 62).withCentre ({ cx, cy });
            cabinet (box, Pattern::weave, true);
            p.hline (box.getX() + 5, box.getRight() - 6, box.getY() + 12);
            for (int i = 0; i < 4; ++i)
                drawSpeaker (p, cx + ((i & 1) ? 14 : -14), cy + 2 + ((i & 2) ? 13 : -13), 11, false);
            break;
        }
        case 2:   // 2x12 open back
        {
            const auto box = Rectangle<int> (92, 50).withCentre ({ cx, cy });
            cabinet (box, Pattern::diagonal, false);
            drawSpeaker (p, cx - 22, cy, 17, true);
            drawSpeaker (p, cx + 22, cy, 17, true);
            break;
        }
        case 3:   // 1x12 combo with its control strip
        {
            const auto box = Rectangle<int> (64, 60).withCentre ({ cx, cy });
            cabinet (box, Pattern::checker, false);
            const Rectangle<int> panel (box.getX() + 3, box.getY() + 3, box.getWidth() - 6, 10);
            p.fill (panel, paper);
            p.hline (panel.getX(), panel.getRight() - 1, panel.getBottom());
            for (int k = 0; k < 6; ++k)
                p.fill ({ panel.getX() + 6 + k * 9, panel.getY() + 4, 2, 2 });
            drawSpeaker (p, cx, cy + 6, 20, true);
            break;
        }
        default:  // user IR: a floppy disk
        {
            const auto d = Rectangle<int> (34, 34).withCentre ({ cx, cy });
            p.fill (d, paper);
            p.hline (d.getX(), d.getRight() - 5, d.getY());
            p.line (d.getRight() - 5, d.getY(), d.getRight() - 1, d.getY() + 4);
            p.vline (d.getRight() - 1, d.getY() + 4, d.getBottom() - 1);
            p.hline (d.getX(), d.getRight() - 1, d.getBottom() - 1);
            p.vline (d.getX(), d.getY(), d.getBottom() - 1);
            const Rectangle<int> shutter (d.getX() + 8, d.getY(), 16, 11);
            p.fill (shutter);
            p.fill ({ shutter.getX() + 10, shutter.getY() + 2, 3, 7 }, paper);
            const Rectangle<int> label (d.getX() + 4, d.getY() + 15, d.getWidth() - 8, d.getHeight() - 18);
            p.frame (label);
            p.text (irLoaded ? "IR" : "?", label, Justification::centred);
            if (! irLoaded)
                p.grey (d);
            break;
        }
    }
}

void CabPage::MicArt::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto b = getLocalBounds();
    p.frame (b);
    const int r = jmin (b.getWidth(), b.getHeight() - 12) / 2 - 3;
    const int cx = b.getCentreX(), cy = b.getY() + 9 + r + 1;
    drawSpeaker (p, cx, cy, r, true, r / 5);

    // Mic crosshair from the dust cap (0) to the cone edge (1), knocked out so it reads on
    // the cap and on the cone alike
    const int mx = cx + roundToInt (position * (float) (r - 6));
    p.disc (mx, cy, 7, paper);
    p.ring (mx, cy, 6);
    p.ring (mx, cy, 3);
    p.dot (mx, cy);
    p.fill ({ mx - 11, cy - 1, 4, 3 }, paper);
    p.fill ({ mx + 8, cy - 1, 4, 3 }, paper);
    p.fill ({ mx - 1, cy - 11, 3, 4 }, paper);
    p.fill ({ mx - 1, cy + 8, 3, 4 }, paper);
    p.hline (mx - 10, mx - 8, cy);
    p.hline (mx + 8, mx + 10, cy);
    p.vline (mx, cy - 10, cy - 8);
    p.vline (mx, cy + 8, cy + 10);

    p.text ("CAP", 3, 2, ink, tiny());
    p.text ("EDGE", { 0, 2, b.getWidth() - 3, 5 }, Justification::right, ink, tiny());
}

CabPage::CabPage (ForgeAmpProcessor& p, PromptOverlay& overlay)
    : proc (p), prompt (overlay),
      cabCard ("Cabinet", &p.apvts, ids::cabOn),
      responseCard ("Response"),
      micCard ("Microphone"),
      models (p.apvts, ids::cabModel, true),
      mic (p.apvts, ids::cabMic, "Position", "0 = centre of the speaker cap (bright), 10 = cone edge (dark)."),
      lowCut (p.apvts, ids::cabLowCut, "Low cut", "Removes boom below this frequency."),
      highCut (p.apvts, ids::cabHighCut, "High cut", "Removes fizz above this frequency."),
      cabCurve ("Speaker + mic", -30.0f, 12.0f, 40.0f, 16000.0f)
{
    addAndMakeVisible (cabCard);
    addAndMakeVisible (responseCard);
    addAndMakeVisible (micCard);
    for (auto* c : std::initializer_list<Component*> { &models, &art, &loadIr, &clearIr, &irInfo })
        cabCard.addAndMakeVisible (c);
    responseCard.addAndMakeVisible (cabCurve);
    micCard.addAndMakeVisible (micArt);
    for (auto* k : { &mic, &lowCut, &highCut })
        micCard.addAndMakeVisible (k);

    models.itemNote = [] (int i)
    {
        if (i >= 0 && i < (int) cabModels().size())
            return String (cabModels()[(size_t) i].description);
        return String ("Your own impulse response (WAV or AIFF).");
    };

    loadIr.setTooltip ("Load a cabinet impulse response (WAV or AIFF). It's level-matched to the built-in cabs.");
    clearIr.setTooltip ("Unload the impulse response");
    loadIr.onClick = [this] { chooseImpulseResponse(); };
    clearIr.onClick = [this] { proc.clearUserImpulseResponse(); refresh(); };
    irInfo.small = true;

    cabCurve.setCompute ([this] (const double* f, double* out, int n)
    {
        const int model = (int) lastCab[0];
        const float micPos = lastCab[1] * 0.1f, lc = lastCab[2], hc = lastCab[3];

        if (model == CabSim::kUserIR && proc.hasUserImpulseResponse())
        {
            const auto resp = proc.getUserIrResponse();
            const double lo = ForgeAmpProcessor::kResponseMinHz, hi = ForgeAmpProcessor::kResponseMaxHz;
            for (int i = 0; i < n; ++i)
            {
                const double pos = std::log (f[i] / lo) / std::log (hi / lo) * (ForgeAmpProcessor::kResponsePoints - 1);
                const int i0 = jlimit (0, ForgeAmpProcessor::kResponsePoints - 2, (int) pos);
                const double t = jlimit (0.0, 1.0, pos - i0);
                const double db = resp.size() == (size_t) ForgeAmpProcessor::kResponsePoints
                                    ? resp[(size_t) i0] * (1.0 - t) + resp[(size_t) i0 + 1] * t : 0.0;
                out[i] = db + CabSim::postFilterDb (micPos, lc, hc, f[i]);
            }
        }
        else
        {
            CabSim::displayResponseDb (model == CabSim::kUserIR ? 0 : model, micPos, lc, hc, f, out, n);
        }
    });
}

void CabPage::chooseImpulseResponse()
{
    const auto start = proc.getUserImpulseResponseFile().existsAsFile()
                           ? proc.getUserImpulseResponseFile().getParentDirectory()
                           : File::getSpecialLocation (File::userDocumentsDirectory);

    chooser = std::make_unique<FileChooser> ("Load a cabinet impulse response", start, "*.wav;*.aif;*.aiff");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file == File())
                                  return;

                              const auto error = proc.loadUserImpulseResponse (file);
                              if (error.isNotEmpty())
                                  prompt.show ("Couldn't load IR", error, false, {}, "OK", nullptr, false);
                              refresh();
                          });
}

void CabPage::resized()
{
    cabCard.setBounds (0, 0, 150, getHeight());
    auto body = cabCard.getBody();
    models.setBounds (body.removeFromTop (5 * ChoiceSelector::rowHeight));
    body.removeFromTop (3);
    art.setBounds (body.removeFromTop (68));
    body.removeFromTop (4);
    auto row = body.removeFromTop (15);
    loadIr.setBounds (row.removeFromLeft (row.getWidth() - 56));
    row.removeFromLeft (4);
    clearIr.setBounds (row);
    body.removeFromTop (6);
    irInfo.setBounds (body);

    responseCard.setBounds (156, 0, getWidth() - 156, 118);
    cabCurve.setBounds (responseCard.getBody());

    micCard.setBounds (156, 123, getWidth() - 156, getHeight() - 123);
    auto m = micCard.getBody();
    micArt.setBounds (m.removeFromLeft (82));
    layoutRow (m.withSizeKeepingCentre (m.getWidth(), Knob::heightFor (12)), { &mic, &lowCut, &highCut }, 54);
}

void CabPage::refresh()
{
    const bool on = cabCard.isOn();
    for (auto* k : { &mic, &lowCut, &highCut })
        k->setDimmed (! on);

    const std::array<float, 4> now { value (proc, ids::cabModel), value (proc, ids::cabMic),
                                     value (proc, ids::cabLowCut), value (proc, ids::cabHighCut) };
    const bool irLoaded = proc.hasUserImpulseResponse();
    const auto irPath = proc.getUserImpulseResponseFile().getFullPathName();

    if (now != lastCab || irLoaded != lastIrLoaded || irPath != lastIrPath)
    {
        lastCab = now;
        lastIrLoaded = irLoaded;
        lastIrPath = irPath;

        const int model = (int) now[0];
        const bool userSelected = model == CabSim::kUserIR;
        art.model = model;
        art.irLoaded = irLoaded;
        art.repaint();
        micArt.position = now[1] * 0.1f;
        micArt.repaint();

        if (irLoaded)
            irInfo.setText ("IR: " + proc.getUserImpulseResponseFile().getFileName()
                            + (userSelected ? String() : String ("  (pick USER IR to hear it)")));
        else
            irInfo.setText (userSelected ? "No IR loaded yet, so the 4x12 Modern plays. Load a WAV or AIFF."
                                         : "Load any cab IR (WAV/AIFF). It's level-matched to the built-in cabs.");
        clearIr.setEnabled (irLoaded);

        responseCard.setStatus (userSelected && irLoaded ? proc.getUserImpulseResponseFile().getFileNameWithoutExtension()
                                                         : cabModelNames()[userSelected ? 0 : model]);
        cabCurve.refresh();
    }
}

//==============================================================================
// FX: delay + reverb
//==============================================================================
void FxPage::EchoView::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto b = getLocalBounds();
    p.frame (b);
    p.text ("TAPS", 3, 2, ink, tiny());

    const auto plot = b.reduced (4).withTrimmedTop (6).withTrimmedLeft (6);
    const float windowMs = jmax (500.0f, timeMs * 8.5f);
    const int mid = plot.getCentreY();
    const int base = pingPong ? mid : plot.getBottom() - 1;
    const int full = pingPong ? plot.getHeight() / 2 - 1 : plot.getHeight() - 1;

    if (pingPong)
    {
        p.dottedH (plot.getX(), plot.getRight() - 1, mid);
        p.text ("L", b.getX() + 3, mid - 8, ink, tiny());
        p.text ("R", b.getX() + 3, mid + 4, ink, tiny());
    }
    else
    {
        p.hline (plot.getX(), plot.getRight() - 1, base);
    }

    // Heights on a 48 dB scale, so the repeats stay visible as they die away
    auto bar = [&] (float tMs, float level, bool up)
    {
        const int x = plot.getX() + roundToInt (tMs / windowMs * (float) (plot.getWidth() - 2));
        const int h = roundToInt (jlimit (0.0f, 1.0f, 1.0f + (float) gainToDb ((double) level) / 48.0f) * (float) full);
        if (h < 1 || x >= plot.getRight() - 1)
            return false;
        p.fill ({ x, up ? base - h : base + 1, 2, h });
        return true;
    };

    bar (0.0f, 1.0f, true);
    if (pingPong)
        bar (0.0f, 1.0f, false);
    const float wet = jmin (1.0f, mix * 2.0f);
    float level = wet;
    for (int n = 1; n < 64; ++n)
    {
        if (! bar (timeMs * (float) n, level, ! pingPong || (n & 1) == 1))
            break;
        level *= feedback;
    }

    const auto label = String (roundToInt (timeMs)) + "MS";
    const int lx = plot.getX() + roundToInt (timeMs / windowMs * (float) (plot.getWidth() - 2));
    if (lx + tiny().width (label) < plot.getRight())
        p.text (label, lx + 4, b.getY() + 2, ink, tiny());

    if (! on)
        p.grey (b.reduced (1));
}

void FxPage::DecayView::paint (Graphics& g)
{
    Pix p (g, *this);
    const auto b = getLocalBounds();
    p.frame (b);
    p.text ("DECAY", 3, 2, ink, tiny());

    // Levels on a 60 dB scale, so the tail is a straight ramp that reaches the floor at RT60
    const auto plot = b.reduced (4).withTrimmedTop (6);
    const float preS = preDelayMs * 0.001f;
    const float earlyS = 0.02f + 0.06f * size;
    const float windowS = preS + earlyS + decay * 1.15f;
    auto xFor = [&] (float t) { return plot.getX() + roundToInt (t / windowS * (float) (plot.getWidth() - 1)); };
    const int base = plot.getBottom() - 1;
    const int full = plot.getHeight() - 2;
    auto heightForGain = [full] (float gain) { return roundToInt (jlimit (0.0f, 1.0f, 1.0f + (float) gainToDb ((double) gain) / 60.0f) * (float) full); };
    p.hline (plot.getX(), plot.getRight() - 1, base);

    // Dry hit (the mix law fades it above 50 %)
    const int dryH = heightForGain (jmin (1.0f, 2.0f * (1.0f - mix)));
    p.fill ({ plot.getX(), base - dryH, 2, dryH });

    // Early reflections
    const float wet = jmin (1.0f, mix * 2.0f) * 0.8f;
    const int numEarly = 3 + roundToInt (size * 5.0f);
    for (int i = 0; i < numEarly; ++i)
    {
        const float t = preS + earlyS * (float) (i + 1) / (float) (numEarly + 1);
        const float level = wet * (1.0f - 0.45f * (float) i / (float) numEarly) * (0.7f + 0.15f * (float) ((i * 7) % 3));
        const int h = heightForGain (level);
        p.vline (xFor (t), base - h, base);
    }

    // Late tail with a little grain; dither density follows the tone
    const float t0 = preS + earlyS * 0.5f;
    const auto fillPattern = tone > 0.66f ? Pattern::checker : tone > 0.33f ? Pattern::dots : Pattern::sparse;
    int prevY = base;
    for (int x = xFor (t0); x < plot.getRight(); ++x)
    {
        const float t = (float) (x - plot.getX()) / (float) (plot.getWidth() - 1) * windowS;
        const float env = wet * 0.8f * std::pow (10.0f, -3.0f * (t - t0) / decay);
        int h = heightForGain (env);
        if (h > 3)
            h += ((x * 37 + 11) % 5) - 2;
        const int y = base - jmax (0, h);
        if (y < base)
            p.fill ({ x, y + 1, 1, base - y - 1 }, ink, fillPattern);
        if (y > prevY) p.vline (x, prevY + 1, y);
        else if (y < prevY) p.vline (x, y, prevY - 1);
        else p.dot (x, y);
        prevY = y;
    }

    // RT60 marker and the pre-delay gap
    const int rx = xFor (t0 + decay);
    if (rx < plot.getRight())
    {
        p.dottedV (rx, plot.getY(), base, 2);
        p.text ("RT60", rx - tiny().width ("RT60") - 2, plot.getY(), ink, tiny());
    }
    const int px0 = plot.getX() + 3, px1 = xFor (preS);
    if (px1 - px0 > 18)
        p.text ("PRE", px0 + (px1 - px0 - tiny().width ("PRE")) / 2, base - 7, ink, tiny());

    if (! on)
        p.grey (b.reduced (1));
}

FxPage::FxPage (ForgeAmpProcessor& p)
    : proc (p),
      delayCard ("Delay", &p.apvts, ids::delayOn),
      reverbCard ("Reverb", &p.apvts, ids::reverbOn),
      syncAttachment (p.apvts, ids::delaySync, sync),
      pingPongAttachment (p.apvts, ids::delayPingPong, pingPong),
      delayTime (p.apvts, ids::delayTime, "Time", "Delay time in milliseconds."),
      delayDivision (p.apvts, ids::delayDivision, "Time", "Delay time as a note value, locked to the host tempo."),
      delayFeedback (p.apvts, ids::delayFeedback, "Feedback", "Number of repeats."),
      delayTone (p.apvts, ids::delayTone, "Tone", "Darkens each repeat, like an analog or tape delay."),
      delayMix (p.apvts, ids::delayMix, "Mix", "Up to 50% adds echoes to the full dry signal; above 50% the dry fades."),
      reverbSize (p.apvts, ids::reverbSize, "Size", "Room size, from small room to large hall."),
      reverbDecay (p.apvts, ids::reverbDecay, "Decay", "Reverb time (RT60)."),
      reverbPreDelay (p.apvts, ids::reverbPreDelay, "Pre-delay", "Gap before the reverb starts; keeps the attack clear."),
      reverbTone (p.apvts, ids::reverbTone, "Tone", "Damping: lower is darker."),
      reverbMix (p.apvts, ids::reverbMix, "Mix", "Up to 50% adds reverb to the full dry signal; above 50% the dry fades.")
{
    addAndMakeVisible (delayCard);
    addAndMakeVisible (reverbCard);
    sync.setTooltip ("Lock the delay time to the host tempo");
    pingPong.setTooltip ("Repeats bounce between left and right");

    for (auto* c : std::initializer_list<Component*> { &sync, &pingPong, &echo, &delayTime, &delayDivision,
                                                       &delayFeedback, &delayTone, &delayMix, &delayInfo })
        delayCard.addAndMakeVisible (c);
    for (auto* c : std::initializer_list<Component*> { &decay, &reverbSize, &reverbDecay, &reverbPreDelay,
                                                       &reverbTone, &reverbMix, &reverbInfo })
        reverbCard.addAndMakeVisible (c);

    delayInfo.centred = reverbInfo.centred = true;
    reverbInfo.setText ("Trails ring out when switched off.");
    refresh();
}

void FxPage::resized()
{
    delayCard.setBounds (0, 0, 203, getHeight());
    reverbCard.setBounds (209, 0, 203, getHeight());
    const int knobH = Knob::heightFor (12);

    auto d = delayCard.getBody();
    auto toggles = d.removeFromTop (11);
    sync.setBounds (toggles.removeFromLeft (sync.idealWidth() + 2).withTrimmedLeft (2));
    toggles.removeFromLeft (12);
    pingPong.setBounds (toggles.removeFromLeft (pingPong.idealWidth()));
    d.removeFromTop (4);
    echo.setBounds (d.removeFromTop (60));
    d.removeFromTop (4);
    layoutRow (d.removeFromTop (knobH), { &delayTime, &delayFeedback }, 80);
    delayDivision.setBounds (delayTime.getBounds());
    d.removeFromTop (2);
    layoutRow (d.removeFromTop (knobH), { &delayTone, &delayMix }, 80);
    delayInfo.setBounds (d.withTrimmedTop (4));

    auto r = reverbCard.getBody();
    decay.setBounds (r.removeFromTop (75));
    r.removeFromTop (4);
    layoutRow (r.removeFromTop (knobH), { &reverbSize, &reverbDecay, &reverbPreDelay }, 64);
    r.removeFromTop (2);
    layoutRow (r.removeFromTop (knobH), { &reverbTone, &reverbMix }, 64);
    reverbInfo.setBounds (r.withTrimmedTop (4));
}

void FxPage::refresh()
{
    const bool synced = sync.getToggleState();
    delayTime.setVisible (! synced);
    delayDivision.setVisible (synced);

    const bool delayOn = delayCard.isOn();
    for (auto* k : { &delayTime, &delayDivision, &delayFeedback, &delayTone, &delayMix })
        k->setDimmed (! delayOn);

    const bool reverbOn = reverbCard.isOn();
    for (auto* k : { &reverbSize, &reverbDecay, &reverbPreDelay, &reverbTone, &reverbMix })
        k->setDimmed (! reverbOn);

    const double bpm = proc.getBpm();
    const int div = (int) value (proc, ids::delayDivision);
    const float timeMs = synced ? (float) (delayDivisionBeats (div) * 60000.0 / bpm) : value (proc, ids::delayTime);

    const std::array<float, 11> now { timeMs, value (proc, ids::delayFeedback), value (proc, ids::delayMix),
                                      pingPong.getToggleState() ? 1.0f : 0.0f, delayOn ? 1.0f : 0.0f,
                                      value (proc, ids::reverbSize), value (proc, ids::reverbDecay),
                                      value (proc, ids::reverbPreDelay), value (proc, ids::reverbTone),
                                      value (proc, ids::reverbMix), reverbOn ? 1.0f : 0.0f };
    if (now != lastFx)
    {
        lastFx = now;
        echo.timeMs = timeMs;
        echo.feedback = now[1] * 0.01f;
        echo.mix = now[2] * 0.01f;
        echo.pingPong = now[3] > 0.5f;
        echo.on = delayOn;
        echo.repaint();

        decay.size = now[5] * 0.1f;
        decay.decay = now[6];
        decay.preDelayMs = now[7];
        decay.tone = std::log (now[8] / 1000.0f) / std::log (16.0f);
        decay.mix = now[9] * 0.01f;
        decay.on = reverbOn;
        decay.repaint();

        delayCard.setStatus (pingPong.getToggleState() ? "Ping-pong" : "Stereo");
        reverbCard.setStatus ("RT60 " + String (now[6], now[6] < 10.0f ? 1 : 0) + " s");
    }

    if (synced)
        delayInfo.setText (delayDivisionNames()[div] + " at " + String (bpm, bpm == std::floor (bpm) ? 0 : 1)
                           + " BPM = " + String (roundToInt (timeMs)) + " ms");
    else
        delayInfo.setText ("Free time. Tick SYNC to lock to tempo.");
}
} // namespace forge
