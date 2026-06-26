#include "PluginEditor.h"

namespace zeq
{
using namespace theme;

static float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// ============================ EqContent (the UI) ============================
EqContent::EqContent (ZandersEqAudioProcessor& p)
    : proc (p), graph (p), strip (p), presetBar (p), rail (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);   // so Ctrl-Z / Ctrl-Shift-Z reach keyPressed

    addAndMakeVisible (graph);
    addAndMakeVisible (strip);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (rail);

    // Selection changed anywhere -> rebind the floating band card, reposition it (its
    // natural size depends on the selected band's shape), and repaint.
    auto rebind = [this] { rail.bindToSelected(); positionBandPanel(); strip.repaint(); graph.repaint(); };
    graph.onSelectionChanged = rebind;
    strip.onSelectionChanged = rebind;
    // C4: the graph fires this on selection change AND while the selected node is dragged, so the
    // floating band card tracks the node (rebind covers BandStrip selection, which doesn't fire it).
    graph.onBandFocused = [this] (int) { positionBandPanel(); };
    presetBar.onPresetApplied = [this] { rail.refresh(); positionBandPanel(); graph.repaint(); strip.repaint(); };
    rail.onCapture = [this] { graph.toggleCapture(); };
    rail.onMatch   = [this] { graph.runMatch(); strip.repaint(); rail.refresh(); positionBandPanel(); graph.repaint(); };

    for (auto* param : proc.getParameters())
        param->addListener (this);

    startTimerHz (30);
}

EqContent::~EqContent()
{
    for (auto* param : proc.getParameters())
        param->removeListener (this);

    setLookAndFeel (nullptr);
}

void EqContent::timerCallback()
{
    // The spectrum (and any live dynamic-EQ node) is the only thing that moves
    // every frame, so only the graph is repainted unconditionally.
    graph.updateAnimation();
    graph.repaint();

    // The static chrome only changes when a parameter does — repaint it lazily.
    if (uiDirty.exchange (false))
    {
        strip.repaint();
        presetBar.repaint();
        rail.refresh();
        repaint (headerBounds);
        repaint (toolbarBounds);   // AUTO/HQ/bypass/analyzer/gain-scale track host automation too
    }
    else if (rail.hasLiveReadout())
    {
        // Dynamic-EQ gain reduction / auto-gain trim update continuously.
        rail.repaint();
    }
}

// ---- layout -----------------------------------------------------------------
void EqContent::resized()
{
    auto panel   = getLocalBounds().reduced (12);
    auto content = panel.reduced (24);

    // Slim header (brand + A/B + OUT + undo/redo) at the top.
    headerBounds = content.removeFromTop (34);
    content.removeFromTop (14);

    // Slim bottom toolbar (global controls + preset chips) at the bottom.
    toolbarBounds = content.removeFromBottom (52);
    content.removeFromBottom (12);

    // Active-bands overview strip just above the toolbar.
    strip.setBounds (content.removeFromBottom (56));
    content.removeFromBottom (12);

    // Everything that remains is the full-width spectrum well (the Pro-Q hero).
    wellBounds = content;
    graph.setBounds (wellBounds.reduced (4));

    layoutToolbar (toolbarBounds);
    positionBandPanel();

    // Header right cluster: A/B + OUT + undo/redo (geometry unchanged from the old design).
    auto hr = headerBounds;
    auto ab = hr.removeFromRight (74).reduced (0, -1);
    abA = ab.removeFromLeft (37);
    abB = ab;

    outBox = juce::Rectangle<int> (abA.getX() - 116, headerBounds.getCentreY() - 16, 108, 32);

    const int outLeft = outBox.getX();
    const int bw = 26;
    const int bh = 22;
    const int cy = headerBounds.getCentreY();
    redoBtn = juce::Rectangle<int> (outLeft - 10 - bw,       cy - bh / 2, bw, bh);
    undoBtn = juce::Rectangle<int> (redoBtn.getX() - 4 - bw, cy - bh / 2, bw, bh);
}

void EqContent::layoutToolbar (juce::Rectangle<int> tb)
{
    const int gap = 8;

    // Left cluster: processing mode + analyzer + gain-scale + bypass.
    phaseSeg     = tb.removeFromLeft (174); tb.removeFromLeft (gap);
    anOnBtn      = tb.removeFromLeft (74);  tb.removeFromLeft (6);
    anRangeBtn   = tb.removeFromLeft (64);  tb.removeFromLeft (gap);
    gainScaleBtn = tb.removeFromLeft (112); tb.removeFromLeft (gap);
    bypassBtn    = tb.removeFromLeft (58);  tb.removeFromLeft (gap);

    // Right cluster (pinned to the right edge): view toggles + HQ/AUTO.
    pianoBtn  = tb.removeFromRight (38); tb.removeFromRight (4);
    sketchBtn = tb.removeFromRight (38); tb.removeFromRight (4);
    fsBtn     = tb.removeFromRight (38); tb.removeFromRight (gap);
    autoBtn   = tb.removeFromRight (56); tb.removeFromRight (6);
    hqBtn     = tb.removeFromRight (46); tb.removeFromRight (gap);

    // Middle: the preset chips + SAVE/LOAD fill what remains (PresetBar reserves its own
    // right 120px for the buttons, and is height-agnostic, so no PresetBar edit is needed).
    presetBar.setBounds (tb.reduced (0, 8));
}

// The band card floats under the selected node (Pro-Q style) instead of living in a side rail.
void EqContent::positionBandPanel()
{
    // C4: the graph owns node geometry. Ask it for the selected node's bounds (graph-local),
    // translate into EqContent coords, then place + clamp the card.
    const int  slot      = proc.getSelectedBand();
    const auto nodeLocal = graph.getBandScreenBounds (slot);   // empty if inactive / off-screen
    if (nodeLocal.isEmpty())
    {
        rail.setVisible (false);                               // nothing to anchor to -> hide
        return;
    }
    const auto node = nodeLocal + graph.getPosition();         // -> EqContent coords

    const auto want = rail.getDesiredSize();                   // juce::Rectangle<int> (w, h)
    const int w = juce::jmax (1, want.getWidth());
    int       h = juce::jmax (1, want.getHeight());
    h = juce::jmin (h, juce::jmax (1, wellBounds.getHeight() - 16));

    // Prefer below the node; flip above if it would overflow the well bottom.
    int x = node.getCentreX() - w / 2;
    int y = node.getBottom() + 10;
    if (y + h > wellBounds.getBottom() - 8)
        y = node.getY() - 10 - h;

    // Clamp inside the well with jmax/jmin (jlimit would assert if the range is degenerate).
    x = juce::jmax (wellBounds.getX() + 8, juce::jmin (x, wellBounds.getRight()  - w - 8));
    y = juce::jmax (wellBounds.getY() + 8, juce::jmin (y, wellBounds.getBottom() - h - 8));

    rail.setBounds (x, y, w, h);
    rail.setVisible (true);
    rail.toFront (false);                                      // sit above the spectrum child
}

// ---- paint ------------------------------------------------------------------
void EqContent::paint (juce::Graphics& g)
{
    // dark stage
    g.setGradientFill (juce::ColourGradient (stageTop, (float) getWidth() * 0.5f, -40.0f,
                                             stageBase, (float) getWidth() * 0.5f, (float) getHeight(), true));
    g.fillAll();

    // floating panel
    auto panel = getLocalBounds().reduced (12).toFloat();
    juce::Path panelPath;
    panelPath.addRoundedRectangle (panel, 16.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.40f), 42, { 0, 16 }).drawForPath (g, panelPath);
    g.setGradientFill (juce::ColourGradient (panelTop, panel.getCentreX(), panel.getY() - panel.getHeight() * 0.1f,
                                             panelBase, panel.getCentreX(), panel.getBottom(), true));
    g.fillPath (panelPath);
    g.setColour (whiteAlpha (0.05f));
    g.strokePath (panelPath, juce::PathStrokeType (1.0f));

    // graph well frame (now full width)
    auto well = wellBounds.toFloat();
    juce::Path wellPath; wellPath.addRoundedRectangle (well, 12.0f);
    g.setColour (theme::well);
    g.fillPath (wellPath);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (wellPath, juce::PathStrokeType (1.0f));

    drawHeader (g);
    paintToolbar (g);
}

void EqContent::drawHeader (juce::Graphics& g)
{
    auto hr = headerBounds;

    // wordmark: "Zanders" + violet "EQ"
    auto wmFont = Fonts::grotesk (21.0f, Fonts::bold).withExtraKerningFactor (-0.01f);
    g.setFont (wmFont);
    const float zw = textWidth (wmFont, "Zanders");
    g.setColour (text1);
    g.drawText ("Zanders", hr.withWidth (200), juce::Justification::centredLeft);
    g.setColour (accentVio);
    g.drawText ("EQ", hr.withTrimmedLeft ((int) zw + 2).withWidth (60), juce::Justification::centredLeft);

    // SHAPE badge
    auto badge = juce::Rectangle<float> (static_cast<float> (hr.getX()) + zw + 40.0f, (float) hr.getCentreY() - 10.0f, 56.0f, 20.0f);
    g.setColour (accent.withAlpha (0.12f));
    g.fillRoundedRectangle (badge, 10.0f);
    g.setColour (accent.withAlpha (0.30f));
    g.drawRoundedRectangle (badge.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent);
    g.setFont (Fonts::grotesk (9.5f, Fonts::semibold).withExtraKerningFactor (0.16f));
    g.drawText ("SHAPE", badge, juce::Justification::centred);

    // OUT readout (drag vertically to set the output trim)
    const float out = floatParam (ids::output);
    g.setColour (theme::well);
    g.fillRoundedRectangle (outBox.toFloat(), 8.0f);
    g.setColour (whiteAlpha (0.08f));
    g.drawRoundedRectangle (outBox.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (textLabel);
    g.setFont (Fonts::grotesk (9.0f, Fonts::semibold).withExtraKerningFactor (0.14f));
    g.drawText ("OUT", outBox.withTrimmedLeft (12).withWidth (30), juce::Justification::centredLeft);
    g.setColour (text1);
    g.setFont (Fonts::mono (13.0f));
    g.drawText ((out >= 0 ? "+" : "") + juce::String (out, 1) + " dB",
                outBox.withTrimmedRight (10), juce::Justification::centredRight);

    // Undo / redo (back / forward arrows; dim when nothing is available)
    auto drawHistBtn = [&] (juce::Rectangle<int> ri, bool enabled, bool redo)
    {
        auto r = ri.toFloat();
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (whiteAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour ((enabled ? accent : text1).withAlpha (enabled ? 0.95f : 0.22f));
        const float my = r.getCentreY();
        const float x0 = r.getX() + 8.0f;
        const float x1 = r.getRight() - 8.0f;
        const juce::Line<float> ln = redo ? juce::Line<float> (x0, my, x1, my)
                                          : juce::Line<float> (x1, my, x0, my);
        g.drawArrow (ln, 1.6f, 6.5f, 6.0f);
    };
    drawHistBtn (undoBtn, proc.canUndo(), false);
    drawHistBtn (redoBtn, proc.canRedo(), true);

    // A/B toggle
    const bool isA = proc.getCurrentSlot() == "A";
    auto drawAB = [&] (juce::Rectangle<int> r, const char* label, bool active)
    {
        if (active)
        {
            juce::ColourGradient grad (accent, static_cast<float> (r.getX()), static_cast<float> (r.getY()), accentVio, static_cast<float> (r.getX()), static_cast<float> (r.getBottom()), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r.toFloat().reduced (1.0f), 6.0f);
            g.setColour (juce::Colours::white);
        }
        else
        {
            g.setColour (text3);
        }
        g.setFont (Fonts::grotesk (10.0f, Fonts::bold).withExtraKerningFactor (0.1f));
        g.drawText (label, r, juce::Justification::centred);
    };
    auto abFrame = abA.getUnion (abB).expanded (3, 1);
    g.setColour (theme::well);
    g.fillRoundedRectangle (abFrame.toFloat(), 8.0f);
    g.setColour (whiteAlpha (0.08f));
    g.drawRoundedRectangle (abFrame.toFloat().reduced (0.5f), 8.0f, 1.0f);
    drawAB (abA, "A", isA);
    drawAB (abB, "B", ! isA);
}

void EqContent::paintToolbar (juce::Graphics& g)
{
    // toolbar tray
    auto tray = toolbarBounds.toFloat();
    g.setColour (whiteAlpha (0.03f));
    g.fillRoundedRectangle (tray, 10.0f);
    g.setColour (whiteAlpha (0.06f));
    g.drawRoundedRectangle (tray.reduced (0.5f), 10.0f, 1.0f);

    auto labelFont = Fonts::grotesk (9.0f, Fonts::semibold).withExtraKerningFactor (0.08f);

    // A neon pill: fill + border + centred label. `accentCol` lights when "on".
    auto pill = [&] (juce::Rectangle<int> ri, const juce::String& label,
                     bool on, bool enabled, juce::Colour accentCol)
    {
        auto r = ri.toFloat().reduced (0.0f, 8.0f);
        if (on)
        {
            g.setColour (accentCol.withAlpha (0.18f)); g.fillRoundedRectangle (r, 7.0f);
            g.setColour (accentCol.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (accentCol);
        }
        else
        {
            g.setColour (whiteAlpha (enabled ? 0.04f : 0.02f)); g.fillRoundedRectangle (r, 7.0f);
            g.setColour (whiteAlpha (enabled ? 0.08f : 0.04f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (enabled ? text2 : textFaint);
        }
        g.setFont (labelFont);
        g.drawText (label, r, juce::Justification::centred);
    };

    // phaseMode — segmented control. Only "Zero Latency" is live this round (C5); Natural
    // and Linear are declared-but-inert, drawn dimmed.
    {
        auto frame = phaseSeg.toFloat().reduced (0.0f, 8.0f);
        g.setColour (whiteAlpha (0.04f)); g.fillRoundedRectangle (frame, 7.0f);
        g.setColour (whiteAlpha (0.08f)); g.drawRoundedRectangle (frame.reduced (0.5f), 7.0f, 1.0f);

        auto inner = phaseSeg.reduced (3, 9);
        const int sw = inner.getWidth() / 3;
        auto s0 = inner.removeFromLeft (sw);
        auto s1 = inner.removeFromLeft (sw);
        auto s2 = inner;

        juce::ColourGradient grad (accent, (float) s0.getX(), (float) s0.getY(),
                                   accentVio, (float) s0.getX(), (float) s0.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (s0.toFloat().reduced (1.0f), 5.0f);
        g.setColour (juce::Colours::white);
        g.setFont (Fonts::grotesk (8.5f, Fonts::bold).withExtraKerningFactor (0.04f));
        g.drawText ("ZERO", s0, juce::Justification::centred);

        g.setFont (Fonts::grotesk (8.5f, Fonts::semibold).withExtraKerningFactor (0.04f));
        g.setColour (textFaint);
        g.drawText ("NAT", s1, juce::Justification::centred);
        g.drawText ("LIN", s2, juce::Justification::centred);
    }

    // analyzer mode + range (cycle on click) — show the current choice, value in accent.
    auto cyclePill = [&] (juce::Rectangle<int> ri, const juce::String& value)
    {
        auto r = ri.toFloat().reduced (0.0f, 8.0f);
        g.setColour (whiteAlpha (0.04f)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (whiteAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (accent.withAlpha (0.92f));
        g.setFont (labelFont);
        g.drawText (value, r, juce::Justification::centred);
    };
    cyclePill (anOnBtn,    choiceLabel (ids::analyzerOn));
    cyclePill (anRangeBtn, choiceLabel (ids::analyzerRange));

    // gain-scale (drag vertically) — "GAIN  xxx%"
    {
        auto r = gainScaleBtn.toFloat().reduced (0.0f, 8.0f);
        g.setColour (whiteAlpha (0.04f)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (whiteAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (textLabel);
        g.setFont (labelFont);
        g.drawText ("GAIN", r.withWidth (40.0f).withTrimmedLeft (10.0f), juce::Justification::centredLeft);
        g.setColour (text1);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (juce::String (juce::roundToInt (floatParam (ids::gainScale))) + "%",
                    r.withTrimmedRight (10.0f), juce::Justification::centredRight);
    }

    pill (bypassBtn, "BYPASS", boolParam (ids::globalBypass), true, danger);
    pill (hqBtn,     "HQ",     boolParam (ids::hq),           true, accent);
    pill (autoBtn,   "AUTO",   boolParam (ids::autogain),     true, accent);

    // View toggles — full-screen (editor window) + EQ-Sketch / piano overlay (Branch 3 graph).
    bool fsOn = false;
    if (auto* ed = findParentComponentOfClass<ZandersEqEditor>())
        fsOn = ed->isMaximized();
    pill (fsBtn,     "FS",  fsOn,                   true, accent);
    pill (sketchBtn, "SKT", graph.isSketchActive(), true, accent);
    pill (pianoBtn,  "PNO", graph.isPianoVisible(), true, accent);
}

// ---- param helpers ----------------------------------------------------------
bool EqContent::boolParam (const juce::String& id) const
{
    auto* v = proc.getApvts().getRawParameterValue (id);
    return v != nullptr && v->load() >= 0.5f;
}

float EqContent::floatParam (const juce::String& id) const
{
    auto* v = proc.getApvts().getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

juce::String EqContent::choiceLabel (const juce::String& id) const
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.getApvts().getParameter (id)))
        return cp->getCurrentChoiceName();
    return {};
}

void EqContent::toggleBoolParam (const juce::String& id)
{
    auto* p = proc.getApvts().getParameter (id);
    if (p == nullptr)
        return;
    const bool now = boolParam (id);
    proc.recordUndoableEdit ([p, now] { p->setValueNotifyingHost (now ? 0.0f : 1.0f); });
    repaint (toolbarBounds);
}

void EqContent::cycleChoiceParam (const juce::String& id)
{
    auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.getApvts().getParameter (id));
    if (cp == nullptr || cp->choices.isEmpty())
        return;
    const int next = (cp->getIndex() + 1) % cp->choices.size();
    proc.recordUndoableEdit ([cp, next] { *cp = next; });
    repaint (toolbarBounds);
}

void EqContent::startParamDrag (const juce::String& id, const juce::MouseEvent& e, float perPx)
{
    if (proc.getApvts().getParameter (id) == nullptr)
        return;
    dragParamId    = id;
    dragStartValue = floatParam (id);
    dragStartY     = e.getPosition().y;
    dragPerPx      = perPx;
    proc.beginUndoTransaction();   // the whole drag becomes a single undo step
}

// ---- input ------------------------------------------------------------------
void EqContent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();   // clicking the chrome reclaims focus for the shortcuts
    const auto pos = e.getPosition();

    // Header: history + A/B.
    if (undoBtn.contains (pos)) { proc.undo(); rail.bindToSelected(); positionBandPanel(); repaint(); return; }
    if (redoBtn.contains (pos)) { proc.redo(); rail.bindToSelected(); positionBandPanel(); repaint(); return; }
    if (abA.contains (pos))     { proc.toggleABSlot ("A"); rail.bindToSelected(); positionBandPanel(); repaint(); return; }
    if (abB.contains (pos))     { proc.toggleABSlot ("B"); rail.bindToSelected(); positionBandPanel(); repaint(); return; }

    // Header OUT readout: drag to set the output trim.
    if (outBox.contains (pos)) { startParamDrag (ids::output, e, 0.12f); return; }

    // Toolbar: continuous (drag) / toggle / cycle controls.
    if (gainScaleBtn.contains (pos)) { startParamDrag (ids::gainScale, e, 0.6f); return; }
    if (bypassBtn.contains (pos))    { toggleBoolParam (ids::globalBypass);  return; }
    if (hqBtn.contains (pos))        { toggleBoolParam (ids::hq);            return; }
    if (autoBtn.contains (pos))      { toggleBoolParam (ids::autogain);      return; }
    if (anOnBtn.contains (pos))      { cycleChoiceParam (ids::analyzerOn);    return; }
    if (anRangeBtn.contains (pos))   { cycleChoiceParam (ids::analyzerRange); return; }

    // View toggles: piano overlay + EQ-Sketch call into the graph (C4); FS maximises the window.
    if (pianoBtn.contains (pos))  { graph.setPianoVisible (! graph.isPianoVisible()); repaint (toolbarBounds); return; }
    if (sketchBtn.contains (pos)) { graph.setSketchActive (! graph.isSketchActive()); repaint (toolbarBounds); return; }
    if (fsBtn.contains (pos))     { if (auto* ed = findParentComponentOfClass<ZandersEqEditor>()) ed->toggleFullscreen(); repaint (toolbarBounds); return; }

    // phaseSeg: only Zero Latency behaves this round (C5) — Natural/Linear are no-ops.
}

void EqContent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragParamId.isEmpty())
        return;
    auto* p = proc.getApvts().getParameter (dragParamId);
    if (p == nullptr)
        return;

    const float v = dragStartValue + (float) (dragStartY - e.getPosition().y) * dragPerPx;
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getNormalisableRange().convertTo0to1 (v)));
    repaint (dragParamId == juce::String (ids::output) ? headerBounds : toolbarBounds);
}

void EqContent::mouseUp (const juce::MouseEvent&)
{
    if (! dragParamId.isEmpty())
    {
        proc.commitUndoTransaction();   // push the net change as one undo step (no-op if unchanged)
        dragParamId.clear();
    }
}

bool EqContent::keyPressed (const juce::KeyPress& k)
{
    if (! k.getModifiers().isCommandDown())   // Ctrl on Win/Linux, Cmd on macOS
        return false;

    const int code = k.getKeyCode();
    if (code == (int) 'Z')
    {
        if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo();
        rail.bindToSelected(); positionBandPanel(); repaint();
        return true;
    }
    if (code == (int) 'Y')   // common Windows redo
    {
        proc.redo(); rail.bindToSelected(); positionBandPanel(); repaint();
        return true;
    }
    return false;
}

// ============================ ZandersEqEditor (host) ========================
ZandersEqEditor::ZandersEqEditor (ZandersEqAudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), content (p)
{
    addAndMakeVisible (content);

    constrainer.setFixedAspectRatio ((double) designW / (double) designH);
    const int minW = juce::roundToInt (designW * 0.6);
    const int maxW = juce::roundToInt (designW * 1.7);
    constrainer.setSizeLimits (minW, juce::roundToInt (minW * (double) designH / designW),
                               maxW, juce::roundToInt (maxW * (double) designH / designW));
    setConstrainer (&constrainer);
    setResizable (true, false);

    const int w = juce::jlimit (minW, maxW, proc.getEditorWidth());
    setSize (w, juce::roundToInt (w * (double) designH / designW));

    // GPU rendering for real hosts. Skipped when ZEQ_DISABLE_GL is set, which the
    // headless CI (pluginval under xvfb, software-GL only) uses so editor open/close
    // validation never depends on a GL context being creatable.
    if (juce::SystemStats::getEnvironmentVariable ("ZEQ_DISABLE_GL", {}).isEmpty())
        openGLContext.attachTo (*this);
}

ZandersEqEditor::~ZandersEqEditor()
{
    // Detach before the component tree is torn down (must precede member destruction).
    openGLContext.detach();
}

bool ZandersEqEditor::isMaximized() const noexcept
{
    return getWidth() >= juce::roundToInt (designW * 1.7) - 1;   // within rounding of maxW
}

void ZandersEqEditor::toggleFullscreen()
{
    const int maxW = juce::roundToInt (designW * 1.7);
    if (! isMaximized())
    {
        preFsWidth = getWidth();
        setSize (maxW, juce::roundToInt (maxW * (double) designH / designW));
    }
    else
    {
        const int minW = juce::roundToInt (designW * 0.6);
        const int w = juce::jlimit (minW, maxW, preFsWidth > 0 ? preFsWidth : designW);
        setSize (w, juce::roundToInt (w * (double) designH / designW));
    }
}

void ZandersEqEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::stageBase);   // letterbox background (no-op when aspect matches)
}

void ZandersEqEditor::resized()
{
    const float s = juce::jmin ((float) getWidth() / designW, (float) getHeight() / designH);
    content.setBounds (0, 0, designW, designH);
    content.setTransform (juce::AffineTransform::scale (s)
                              .translated ((static_cast<float> (getWidth()) - designW * s) * 0.5f,
                                           (static_cast<float> (getHeight()) - designH * s) * 0.5f));
    proc.setEditorWidth (getWidth());
}

} // namespace zeq
