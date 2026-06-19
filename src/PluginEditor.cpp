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

ZandersEqEditor::ZandersEqEditor (ZandersEqAudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p),
      graph (p), strip (p), presetBar (p), rail (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (graph);
    addAndMakeVisible (strip);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (rail);

    auto rebind = [this] { rail.bindToSelected(); strip.repaint(); graph.repaint(); };
    graph.onSelectionChanged = rebind;
    strip.onSelectionChanged = rebind;
    presetBar.onPresetApplied = [this] { rail.refresh(); graph.repaint(); strip.repaint(); };

    setSize (1100, 716);
    setResizable (false, false);
    startTimerHz (60);
}

ZandersEqEditor::~ZandersEqEditor()
{
    setLookAndFeel (nullptr);
}

void ZandersEqEditor::timerCallback()
{
    graph.updateAnimation();
    graph.repaint();
    strip.repaint();
    presetBar.repaint();
    rail.refresh();
    repaint (headerBounds);
}

void ZandersEqEditor::resized()
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
}

void ZandersEqEditor::paint (juce::Graphics& g)
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

void ZandersEqEditor::drawHeader (juce::Graphics& g)
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
    auto badge = juce::Rectangle<float> (hr.getX() + zw + 40.0f, (float) hr.getCentreY() - 10.0f, 56.0f, 20.0f);
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

    // A/B toggle
    const bool isA = proc.getCurrentSlot() == "A";
    auto drawAB = [&] (juce::Rectangle<int> r, const char* label, bool active)
    {
        if (active)
        {
            juce::ColourGradient grad (accent, r.getX(), r.getY(), accentVio, r.getX(), r.getBottom(), false);
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

void ZandersEqEditor::mouseDown (const juce::MouseEvent& e)
{
    if (abA.contains (e.getPosition())) { proc.toggleABSlot ("A"); rail.bindToSelected(); repaint(); }
    else if (abB.contains (e.getPosition())) { proc.toggleABSlot ("B"); rail.bindToSelected(); repaint(); }
}

} // namespace zeq
