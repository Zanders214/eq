#include "BandEditorRail.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"

namespace zeq
{
using namespace theme;

namespace
{
    const char* typeChipLabels[numFilterTypes] = { "HP", "LO", "BELL", "NTCH", "HI", "LP" };

    void drawChip (juce::Graphics& g, juce::Rectangle<float> r, bool active,
                   const juce::String& text, float fontH, float tracking)
    {
        g.setColour (active ? accent.withAlpha (0.16f) : whiteAlpha (0.06f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (active ? accent.withAlpha (0.30f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (active ? accent : text3);
        g.setFont (Fonts::grotesk (fontH, Fonts::semibold).withExtraKerningFactor (tracking));
        g.drawText (text, r, juce::Justification::centred);
    }

    void drawToggle (juce::Graphics& g, juce::Rectangle<float> r, bool active,
                     const juce::String& text, bool isDanger = false)
    {
        if (active)
        {
            NeonLookAndFeel::glow (g, r, isDanger ? theme::danger : accent, 6.0f, 0.20f);
            juce::ColourGradient grad (isDanger ? theme::danger : accent, r.getX(), r.getY(),
                                       isDanger ? theme::dangerDeep : accentVio, r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour ((isDanger ? juce::Colour (0xffffc0c0) : juce::Colour (0xff96aaff)).withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        else
        {
            g.setColour (whiteAlpha (0.06f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (whiteAlpha (0.12f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }

        // square status dot
        auto dotCol = active ? juce::Colours::white
                             : (isDanger ? theme::danger.withAlpha (0.8f) : accent.withAlpha (0.8f));
        auto content = r.reduced (14.0f, 0.0f);
        auto dot = juce::Rectangle<float> (content.getX(), r.getCentreY() - 4.0f, 8.0f, 8.0f);
        NeonLookAndFeel::glow (g, dot, dotCol, 4.0f, 0.6f);
        g.setColour (dotCol);
        g.fillRoundedRectangle (dot, 2.0f);

        g.setColour (active ? juce::Colours::white : text2);
        g.setFont (Fonts::grotesk (10.0f, Fonts::bold).withExtraKerningFactor (0.12f));
        g.drawText (text, r.withTrimmedLeft (28.0f).withTrimmedRight (6.0f),
                    juce::Justification::centredLeft);
    }
}

BandEditorRail::BandEditorRail (ZandersEqAudioProcessor& p)
    : proc (p), apvts (p.getApvts())
{
    auto setupSlider = [this] (juce::Slider& s, const juce::String& gradient)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.getProperties().set ("gradient", gradient);
        addAndMakeVisible (s);
    };
    setupSlider (freqSlider, "cool");
    setupSlider (gainSlider, "warm");
    setupSlider (qSlider,    "warm");

    outputDial.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    outputDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    const float pi = juce::MathConstants<float>::pi;
    outputDial.setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    addAndMakeVisible (outputDial);
    outAtt = std::make_unique<Attachment> (apvts, ids::output, outputDial);

    bindToSelected();
}

BandEditorRail::~BandEditorRail() = default;

FilterType BandEditorRail::selectedType() const
{
    return static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (selected()))->load());
}

void BandEditorRail::setChoice (const juce::String& id, int idx)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
}
void BandEditorRail::setBool (const juce::String& id, bool v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (v ? 1.0f : 0.0f);
}

void BandEditorRail::bindToSelected()
{
    const int s = selected();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    freqAtt = std::make_unique<Attachment> (apvts, ids::freq (s), freqSlider);
    gainAtt = std::make_unique<Attachment> (apvts, ids::gain (s), gainSlider);
    qAtt    = std::make_unique<Attachment> (apvts, ids::q (s),    qSlider);
    lastSelected = s;
    refresh();
}

void BandEditorRail::refresh()
{
    if (selected() != lastSelected)
    {
        bindToSelected();
        return;
    }
    const bool onZero = sitsOnZeroLine (selectedType());
    gainSlider.setEnabled (! onZero);
    gainSlider.getProperties().set ("gradient",
        apvts.getRawParameterValue (ids::gain (selected()))->load() >= 0.0f ? "warm" : "cool");
    repaint();
}

BandEditorRail::Layout BandEditorRail::computeLayout() const
{
    Layout L;
    auto top = getLocalBounds();
    L.header    = top.removeFromTop (18); top.removeFromTop (16);
    L.typeChips = top.removeFromTop (28); top.removeFromTop (18);
    L.freqBlock = top.removeFromTop (42); top.removeFromTop (16);
    L.gainBlock = top.removeFromTop (42); top.removeFromTop (16);
    L.qBlock    = top.removeFromTop (42); top.removeFromTop (8);
    L.slopeRow  = top.removeFromTop (22); top.removeFromTop (16);
    L.onSolo    = top.removeFromTop (36);

    auto bottom = getLocalBounds().removeFromBottom (112);
    L.bottom = bottom;
    auto inner = bottom.withTrimmedTop (18);
    L.dial = inner.removeFromLeft (72);
    inner.removeFromLeft (18);
    L.modeBtn = inner.removeFromTop (34);
    inner.removeFromTop (8);
    L.hqBtn = inner.removeFromTop (34);
    return L;
}

juce::Rectangle<float> BandEditorRail::segment (juce::Rectangle<int> row, int i, int n, float gap) const
{
    const float w = ((float) row.getWidth() - gap * (n - 1)) / (float) n;
    return { (float) row.getX() + (w + gap) * (float) i, (float) row.getY(), w, (float) row.getHeight() };
}

void BandEditorRail::resized()
{
    auto L = computeLayout();
    freqSlider.setBounds (L.freqBlock.withTrimmedTop (24));
    gainSlider.setBounds (L.gainBlock.withTrimmedTop (24));
    qSlider.setBounds    (L.qBlock.withTrimmedTop (24));
    outputDial.setBounds (L.dial.removeFromTop (60));
}

void BandEditorRail::paint (juce::Graphics& g)
{
    auto L = computeLayout();
    const int s = selected();
    const auto type = selectedType();
    const float freq = apvts.getRawParameterValue (ids::freq (s))->load();
    const float gain = apvts.getRawParameterValue (ids::gain (s))->load();
    const float q    = apvts.getRawParameterValue (ids::q (s))->load();
    const int slope  = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (s))->load());
    const bool on    = apvts.getRawParameterValue (ids::on (s))->load() > 0.5f;
    const bool solo  = apvts.getRawParameterValue (ids::solo (s))->load() > 0.5f;
    const bool ms    = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const bool hq    = apvts.getRawParameterValue (ids::hq)->load() > 0.5f;
    const float out  = apvts.getRawParameterValue (ids::output)->load();
    const auto col   = colourForFreq (freq);
    const bool cut   = isCut (type);

    // header
    auto header = L.header;
    auto dot = header.removeFromLeft (12).withSizeKeepingCentre (12, 12).toFloat();
    NeonLookAndFeel::glow (g, dot, col, 6.0f, 0.7f);
    g.setColour (col);
    g.fillEllipse (dot);
    g.setColour (text1);
    g.setFont (Fonts::grotesk (14.0f, Fonts::semibold));
    g.drawText ("BAND " + juce::String (s + 1), header.withTrimmedLeft (9), juce::Justification::centredLeft);
    g.setColour (text3);
    g.setFont (Fonts::mono (12.0f));
    g.drawText (typeLabel (type), L.header, juce::Justification::centredRight);

    // type chips
    for (int i = 0; i < numFilterTypes; ++i)
        drawChip (g, segment (L.typeChips, i, numFilterTypes, 5.0f), (int) type == i,
                  typeChipLabels[i], 9.0f, 0.05f);

    // slider labels + values
    auto drawSliderHead = [&] (juce::Rectangle<int> block, const juce::String& label,
                               const juce::String& value, bool dim)
    {
        auto head = block.withHeight (16);
        g.setColour (textLabel);
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.16f));
        g.drawText (label, head, juce::Justification::centredLeft);
        g.setColour (dim ? text3.withAlpha (0.5f) : text2);
        g.setFont (Fonts::mono (12.0f, true));
        g.drawText (value, head, juce::Justification::centredRight);
    };
    drawSliderHead (L.freqBlock, "FREQUENCY", fmtFreq (freq), false);
    drawSliderHead (L.gainBlock, "GAIN", cut ? juce::String ("—") : fmtGain (gain), cut);
    drawSliderHead (L.qBlock, "Q / SLOPE",
                    cut ? juce::String (slope) + " dB/oct  Q" + juce::String (q, 2)
                        : "Q " + juce::String (q, 2), false);

    // slope picker (cut filters only)
    if (cut)
    {
        const int slopes[3] = { 12, 24, 48 };
        for (int i = 0; i < 3; ++i)
            drawChip (g, segment (L.slopeRow, i, 3, 5.0f), slope == slopes[i],
                      juce::String (slopes[i]) + " dB/oct", 9.0f, 0.03f);
    }

    // enable / solo
    auto onBtn = juce::Rectangle<int> (L.onSolo).removeFromLeft (L.onSolo.getWidth() / 2 - 3).toFloat();
    auto soloBtn = juce::Rectangle<int> (L.onSolo).removeFromRight (L.onSolo.getWidth() / 2 - 3).toFloat();
    drawToggle (g, onBtn, on, on ? "ENABLED" : "BYPASSED");
    if (solo)
    {
        juce::ColourGradient grad (amber, soloBtn.getX(), soloBtn.getY(), pink, soloBtn.getX(), soloBtn.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (soloBtn, 8.0f);
        g.setColour (amber.withAlpha (0.6f));
        g.drawRoundedRectangle (soloBtn.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (juce::Colour (0xff1c1a16));
        g.setFont (Fonts::grotesk (10.0f, Fonts::bold).withExtraKerningFactor (0.12f));
        g.drawText ("SOLO", soloBtn, juce::Justification::centred);
    }
    else
    {
        g.setColour (whiteAlpha (0.06f));
        g.fillRoundedRectangle (soloBtn, 8.0f);
        g.setColour (whiteAlpha (0.12f));
        g.drawRoundedRectangle (soloBtn.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (text2);
        g.setFont (Fonts::grotesk (10.0f, Fonts::bold).withExtraKerningFactor (0.12f));
        g.drawText ("SOLO", soloBtn, juce::Justification::centred);
    }

    // bottom: divider, output dial caption/value, mode + hq
    g.setColour (whiteAlpha (0.08f));
    g.drawHorizontalLine (L.bottom.getY(), 0.0f, (float) getWidth());

    auto dialArea = L.dial;
    auto caption = dialArea.withTop (dialArea.getY() + 60);
    g.setColour (textMuted);
    g.setFont (Fonts::grotesk (9.0f, Fonts::semibold).withExtraKerningFactor (0.22f));
    g.drawText ("OUTPUT", caption.withHeight (12), juce::Justification::centred);
    g.setColour (text2);
    g.setFont (Fonts::mono (11.0f, true));
    g.drawText ((out >= 0.0f ? "+" : "") + juce::String (out, 1) + " dB",
                caption.withTrimmedTop (12).withHeight (14), juce::Justification::centred);

    drawToggle (g, L.modeBtn.toFloat(), ms, ms ? "MID/SIDE" : "STEREO");
    drawToggle (g, L.hqBtn.toFloat(), hq, "HQ OVERSAMPLING");
}

void BandEditorRail::mouseDown (const juce::MouseEvent& e)
{
    auto L = computeLayout();
    const int s = selected();
    const auto p = e.position;

    for (int i = 0; i < numFilterTypes; ++i)
        if (segment (L.typeChips, i, numFilterTypes, 5.0f).contains (p))
        {
            setChoice (ids::type (s), i);
            refresh();
            return;
        }

    if (isCut (selectedType()))
        for (int i = 0; i < 3; ++i)
            if (segment (L.slopeRow, i, 3, 5.0f).contains (p))
            {
                setChoice (ids::slope (s), i);
                repaint();
                return;
            }

    auto onBtn = juce::Rectangle<int> (L.onSolo).removeFromLeft (L.onSolo.getWidth() / 2 - 3).toFloat();
    auto soloBtn = juce::Rectangle<int> (L.onSolo).removeFromRight (L.onSolo.getWidth() / 2 - 3).toFloat();
    if (onBtn.contains (p))   { setBool (ids::on (s),   ! (apvts.getRawParameterValue (ids::on (s))->load() > 0.5f)); repaint(); return; }
    if (soloBtn.contains (p)) { setBool (ids::solo (s), ! (apvts.getRawParameterValue (ids::solo (s))->load() > 0.5f)); repaint(); return; }

    if (L.modeBtn.toFloat().contains (p)) { setChoice (ids::mode, apvts.getRawParameterValue (ids::mode)->load() > 0.5f ? 0 : 1); repaint(); return; }
    if (L.hqBtn.toFloat().contains (p))   { setBool (ids::hq, ! (apvts.getRawParameterValue (ids::hq)->load() > 0.5f)); repaint(); return; }
}

} // namespace zeq
