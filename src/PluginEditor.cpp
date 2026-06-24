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

    auto rebind = [this] { rail.bindToSelected(); strip.repaint(); graph.repaint(); };
    graph.onSelectionChanged = rebind;
    strip.onSelectionChanged = rebind;
    presetBar.onPresetApplied = [this] { rail.refresh(); graph.repaint(); strip.repaint(); };
    rail.onCapture = [this] { graph.toggleCapture(); };
    rail.onMatch   = [this] { graph.runMatch(); strip.repaint(); rail.refresh(); graph.repaint(); };

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

    // The static panels only change when a parameter does — repaint them lazily.
    if (uiDirty.exchange (false))
    {
        strip.repaint();
        presetBar.repaint();
        rail.refresh();
        repaint (headerBounds);
    }
    else if (rail.hasLiveReadout())
    {
        // Dynamic-EQ gain reduction / auto-gain trim update continuously.
        rail.repaint();
    }
}

void EqContent::resized()
{
    auto panel = getLocalBounds().reduced (12);
    auto content = panel.reduced (24);

    headerBounds = content.removeFromTop (30);
    content.removeFromTop (18);

    auto body = content;
    auto left = body.removeFromLeft (728);
    body.removeFromLeft (22);
    auto right = body;

    wellBounds = left.removeFromTop (448);
    graph.setBounds (wellBounds.reduced (4));
    left.removeFromTop (12);
    strip.setBounds (left.removeFromTop (60));
    left.removeFromTop (12);
    presetBar.setBounds (left.removeFromTop (30));

    rail.setBounds (right);

    // A/B + OUT live at the right of the header
    auto hr = headerBounds;
    auto ab = hr.removeFromRight (74).reduced (0, -1);
    abA = ab.removeFromLeft (37);
    abB = ab;

    // Undo / redo buttons sit just left of the OUT readout (OUT box is laid out in
    // drawHeader at abA.getX() - 116) — plenty of empty header space there.
    const int outLeft = abA.getX() - 116;
    const int bw = 26;
    const int bh = 22;
    const int cy = headerBounds.getCentreY();
    redoBtn = juce::Rectangle<int> (outLeft - 10 - bw,       cy - bh / 2, bw, bh);
    undoBtn = juce::Rectangle<int> (redoBtn.getX() - 4 - bw, cy - bh / 2, bw, bh);
}

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

    // graph well frame
    auto well = wellBounds.toFloat();
    juce::Path wellPath; wellPath.addRoundedRectangle (well, 12.0f);
    g.setColour (theme::well);
    g.fillPath (wellPath);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (wellPath, juce::PathStrokeType (1.0f));

    drawHeader (g);
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

    // OUT readout
    const float out = proc.getApvts().getRawParameterValue (ids::output)->load();
    auto outBox = juce::Rectangle<int> (abA.getX() - 116, headerBounds.getCentreY() - 16, 108, 32);
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

void EqContent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();   // clicking the header reclaims focus for the shortcuts
    if (undoBtn.contains (e.getPosition()))      { proc.undo(); rail.bindToSelected(); repaint(); }
    else if (redoBtn.contains (e.getPosition())) { proc.redo(); rail.bindToSelected(); repaint(); }
    else if (abA.contains (e.getPosition())) { proc.toggleABSlot ("A"); rail.bindToSelected(); repaint(); }
    else if (abB.contains (e.getPosition())) { proc.toggleABSlot ("B"); rail.bindToSelected(); repaint(); }
}

bool EqContent::keyPressed (const juce::KeyPress& k)
{
    if (! k.getModifiers().isCommandDown())   // Ctrl on Win/Linux, Cmd on macOS
        return false;

    const int code = k.getKeyCode();
    if (code == (int) 'Z')
    {
        if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo();
        rail.bindToSelected(); repaint();
        return true;
    }
    if (code == (int) 'Y')   // common Windows redo
    {
        proc.redo(); rail.bindToSelected(); repaint();
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
