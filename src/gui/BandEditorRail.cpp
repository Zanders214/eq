#include "BandEditorRail.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"

#include <array>

namespace zeq
{
using namespace theme;

namespace
{
    constexpr std::array<const char*, numFilterTypes> typeChipLabels { "HP", "LO", "BELL", "NTCH", "HI", "LP" };

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
        auto inactiveDotCol = isDanger ? theme::danger.withAlpha (0.8f) : accent.withAlpha (0.8f);
        auto dotCol = active ? juce::Colours::white : inactiveDotCol;
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

    void drawSliderHead (juce::Graphics& g, juce::Rectangle<int> block, const juce::String& label,
                         const juce::String& value, bool dim)
    {
        auto head = block.withHeight (16);
        g.setColour (textLabel);
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.16f));
        g.drawText (label, head, juce::Justification::centredLeft);
        g.setColour (dim ? text3.withAlpha (0.5f) : text2);
        g.setFont (Fonts::mono (12.0f, true));
        g.drawText (value, head, juce::Justification::centredRight);
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
        s.onDragStart = [this] { proc.beginUndoTransaction(); };  // one undo step per drag
        s.onDragEnd   = [this] { proc.commitUndoTransaction(); };
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
    outputDial.onDragStart = [this] { proc.beginUndoTransaction(); };
    outputDial.onDragEnd   = [this] { proc.commitUndoTransaction(); };
    outAtt = std::make_unique<Attachment> (apvts, ids::output, outputDial);

    setupSlider (matchAmountSlider, "accent");
    matchAmtAtt = std::make_unique<Attachment> (apvts, ids::matchamount, matchAmountSlider);

    setupSlider (threshSlider,  "cool");
    setupSlider (rangeSlider,   "accent");
    setupSlider (attackSlider,  "warm");
    setupSlider (releaseSlider, "warm");

    BandEditorRail::bindToSelected();
}

BandEditorRail::~BandEditorRail() = default;

FilterType BandEditorRail::selectedType() const
{
    return static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (selected()))->load());
}

void BandEditorRail::setChoice (const juce::String& id, int idx)
{
    proc.recordUndoableEdit ([this, &id, idx]
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
    });
}
void BandEditorRail::setBool (const juce::String& id, bool v)
{
    proc.recordUndoableEdit ([this, &id, v]
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (v ? 1.0f : 0.0f);
    });
}

void BandEditorRail::bindToSelected()
{
    const int s = selected();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    freqAtt = std::make_unique<Attachment> (apvts, ids::freq (s), freqSlider);
    gainAtt = std::make_unique<Attachment> (apvts, ids::gain (s), gainSlider);
    qAtt    = std::make_unique<Attachment> (apvts, ids::q (s),    qSlider);

    threshAtt.reset(); rangeAtt.reset(); attackAtt.reset(); releaseAtt.reset();
    threshAtt  = std::make_unique<Attachment> (apvts, ids::dynThresh (s),  threshSlider);
    rangeAtt   = std::make_unique<Attachment> (apvts, ids::dynRange (s),   rangeSlider);
    attackAtt  = std::make_unique<Attachment> (apvts, ids::dynAttack (s),  attackSlider);
    releaseAtt = std::make_unique<Attachment> (apvts, ids::dynRelease (s), releaseSlider);

    lastSelected = s;
    BandEditorRail::resized();
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

bool BandEditorRail::hasLiveReadout() const
{
    // Auto-gain shows a live trim figure on its toggle (any tab).
    if (apvts.getRawParameterValue (ids::autogain)->load() > 0.5f)
        return true;

    // The DYN tab shows live gain reduction for an active dynamic band.
    if (showDyn && ! sitsOnZeroLine (selectedType())
        && apvts.getRawParameterValue (ids::dynOn (selected()))->load() > 0.5f)
        return true;

    return false;
}

BandEditorRail::Layout BandEditorRail::computeLayout() const
{
    Layout L;
    auto top = getLocalBounds();
    L.header    = top.removeFromTop (18);
    {
        auto tabs = L.header.removeFromRight (74).withSizeKeepingCentre (74, 18);
        L.eqTab  = tabs.removeFromLeft (35);
        tabs.removeFromLeft (4);
        L.dynTab = tabs;
    }
    top.removeFromTop (16);
    L.typeChips = top.removeFromTop (28); top.removeFromTop (18);
    L.freqBlock = top.removeFromTop (42); top.removeFromTop (16);
    L.gainBlock = top.removeFromTop (42); top.removeFromTop (16);
    L.qBlock    = top.removeFromTop (42); top.removeFromTop (8);
    L.slopeRow  = top.removeFromTop (22); top.removeFromTop (16);

    // The DYN tab reuses the freq..slope span: an enable toggle + 4 sliders.
    {
        auto dynArea = L.freqBlock.getUnion (L.slopeRow);
        L.dynEnable = dynArea.removeFromTop (30); dynArea.removeFromTop (10);
        const int h = (dynArea.getHeight() - 3 * 8) / 4;
        L.dynS0 = dynArea.removeFromTop (h); dynArea.removeFromTop (8);
        L.dynS1 = dynArea.removeFromTop (h); dynArea.removeFromTop (8);
        L.dynS2 = dynArea.removeFromTop (h); dynArea.removeFromTop (8);
        L.dynS3 = dynArea.removeFromTop (h);
    }
    L.onSolo    = top.removeFromTop (36); top.removeFromTop (14);
    L.channelRow = top.removeFromTop (28);

    // EQ-match panel fills the gap between on/solo and the bottom dial/toggle block.
    auto matchArea = top;
    matchArea.removeFromBottom (124 + 14);
    matchArea.removeFromTop (20);
    L.matchLabel  = matchArea.removeFromTop (16);
    matchArea.removeFromTop (6);
    L.matchAmtRow = matchArea.removeFromTop (38);
    matchArea.removeFromTop (8);
    L.matchBtnRow = matchArea.removeFromTop (34);
    L.captureBtn  = L.matchBtnRow.withWidth (L.matchBtnRow.getWidth() / 2 - 3);
    L.matchBtn    = L.matchBtnRow.withTrimmedLeft (L.matchBtnRow.getWidth() / 2 + 3);

    auto bottom = getLocalBounds().removeFromBottom (124);
    L.bottom = bottom;
    auto inner = bottom.withTrimmedTop (18);
    L.dial = inner.removeFromLeft (72);
    inner.removeFromLeft (18);
    L.modeBtn = inner.removeFromTop (30);
    inner.removeFromTop (6);
    L.hqBtn = inner.removeFromTop (30);
    inner.removeFromTop (6);
    L.autoBtn = inner.removeFromTop (30);
    return L;
}

juce::Rectangle<float> BandEditorRail::segment (juce::Rectangle<int> row, int i, int n, float gap) const
{
    const float w = ((float) row.getWidth() - gap * static_cast<float> (n - 1)) / (float) n;
    return { (float) row.getX() + (w + gap) * (float) i, (float) row.getY(), w, (float) row.getHeight() };
}

void BandEditorRail::resized()
{
    auto L = computeLayout();
    const bool cut = sitsOnZeroLine (selectedType());
    const bool dynControls = showDyn && ! cut;

    freqSlider.setVisible (! showDyn);
    gainSlider.setVisible (! showDyn);
    qSlider.setVisible    (! showDyn);
    threshSlider.setVisible (dynControls);
    rangeSlider.setVisible  (dynControls);
    attackSlider.setVisible (dynControls);
    releaseSlider.setVisible (dynControls);

    if (! showDyn)
    {
        freqSlider.setBounds (L.freqBlock.withTrimmedTop (24));
        gainSlider.setBounds (L.gainBlock.withTrimmedTop (24));
        qSlider.setBounds    (L.qBlock.withTrimmedTop (24));
    }
    else if (dynControls)
    {
        threshSlider.setBounds  (L.dynS0.withTrimmedTop (15));
        rangeSlider.setBounds   (L.dynS1.withTrimmedTop (15));
        attackSlider.setBounds  (L.dynS2.withTrimmedTop (15));
        releaseSlider.setBounds (L.dynS3.withTrimmedTop (15));
    }

    outputDial.setBounds (L.dial.removeFromTop (60));
    matchAmountSlider.setBounds (L.matchAmtRow.withTrimmedTop (20));
}

void BandEditorRail::paint (juce::Graphics& g)
{
    auto L = computeLayout();
    const int s = selected();
    const auto type = selectedType();
    const float freq = apvts.getRawParameterValue (ids::freq (s))->load();
    const bool on    = apvts.getRawParameterValue (ids::on (s))->load() > 0.5f;
    const bool solo  = apvts.getRawParameterValue (ids::solo (s))->load() > 0.5f;
    const bool ms    = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const auto col   = colourForFreq (freq);

    // header
    auto header = L.header;
    auto dot = header.removeFromLeft (12).withSizeKeepingCentre (12, 12).toFloat();
    NeonLookAndFeel::glow (g, dot, col, 6.0f, 0.7f);
    g.setColour (col);
    g.fillEllipse (dot);
    g.setColour (text1);
    g.setFont (Fonts::grotesk (14.0f, Fonts::semibold));
    g.drawText ("BAND " + juce::String (s + 1), header.withTrimmedLeft (9), juce::Justification::centredLeft);

    // EQ | DYN tab toggle (right of the header)
    drawChip (g, L.eqTab.toFloat(),  ! showDyn, "EQ",  9.0f, 0.08f);
    drawChip (g, L.dynTab.toFloat(),   showDyn, "DYN", 9.0f, 0.08f);

    // type chips
    for (int i = 0; i < numFilterTypes; ++i)
        drawChip (g, segment (L.typeChips, i, numFilterTypes, 5.0f), (int) type == i,
                  typeChipLabels[i], 9.0f, 0.05f);

    // slider labels + values (FREQ/GAIN/Q or the DYN section)
    paintSliderSection (g, L, s);

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

    // per-band channel lane (labels follow the global Stereo/MS domain)
    {
        const auto ch = (int) apvts.getRawParameterValue (ids::channel (s))->load();
        const std::array<const char*, 3> lr  { "L + R", "L", "R" };
        const std::array<const char*, 3> msl { "M + S", "M", "S" };
        for (int i = 0; i < 3; ++i)
            drawChip (g, segment (L.channelRow, i, 3, 5.0f), ch == i,
                      ms ? msl[i] : lr[i], 9.0f, 0.06f);
    }

    // EQ-match panel
    paintMatchPanel (g, L);

    // bottom: divider, output dial caption/value, mode + hq
    paintBottomBar (g, L);
}

void BandEditorRail::paintSliderSection (juce::Graphics& g, const Layout& L, int s) const
{
    const auto type  = selectedType();
    const float freq = apvts.getRawParameterValue (ids::freq (s))->load();
    const float gain = apvts.getRawParameterValue (ids::gain (s))->load();
    const float q    = apvts.getRawParameterValue (ids::q (s))->load();
    const int slope  = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (s))->load());
    const bool cut   = isCut (type);

    if (! showDyn)
    {
        drawSliderHead (g, L.freqBlock, "FREQUENCY", fmtFreq (freq) + "  " + noteName (freq), false);
        drawSliderHead (g, L.gainBlock, "GAIN", cut ? juce::String ("—") : fmtGain (gain), cut);
        drawSliderHead (g, L.qBlock, "Q / SLOPE",
                        cut ? juce::String (slope) + " dB/oct  Q" + juce::String (q, 2)
                            : "Q " + juce::String (q, 2), false);

        // slope picker (cut filters only)
        if (cut)
        {
            const std::array<int, 3> slopes { 12, 24, 48 };
            for (int i = 0; i < 3; ++i)
                drawChip (g, segment (L.slopeRow, i, 3, 5.0f), slope == slopes[i],
                          juce::String (slopes[i]) + " dB/oct", 9.0f, 0.03f);
        }
        return;
    }

    if (cut)
    {
        // dynamics not available for cut/notch filters
        g.setColour (text3.withAlpha (0.6f));
        g.setFont (Fonts::grotesk (11.0f, Fonts::semibold).withExtraKerningFactor (0.04f));
        g.drawText ("DYNAMICS AVAILABLE ON BELL / SHELF BANDS",
                    L.freqBlock.getUnion (L.slopeRow), juce::Justification::centred);
        return;
    }

    const bool dynOn = apvts.getRawParameterValue (ids::dynOn (s))->load() > 0.5f;
    const float thr  = apvts.getRawParameterValue (ids::dynThresh (s))->load();
    const float rng  = apvts.getRawParameterValue (ids::dynRange (s))->load();
    const float att  = apvts.getRawParameterValue (ids::dynAttack (s))->load();
    const float rel  = apvts.getRawParameterValue (ids::dynRelease (s))->load();
    const float live = proc.getDynGainDb (s);

    drawToggle (g, L.dynEnable.toFloat(), dynOn, dynOn ? "DYNAMICS ON" : "DYNAMICS OFF");
    drawSliderHead (g, L.dynS0, "THRESHOLD", juce::String (thr, 1) + " dB", ! dynOn);
    drawSliderHead (g, L.dynS1, "RANGE",
                    (rng >= 0 ? "+" : "") + juce::String (rng, 1) + " dB"
                    + (dynOn && std::abs (live) > 0.05f ? "  (" + juce::String (live, 1) + ")" : ""),
                    ! dynOn);
    drawSliderHead (g, L.dynS2, "ATTACK",  juce::String (att, att < 10 ? 1 : 0) + " ms", ! dynOn);
    drawSliderHead (g, L.dynS3, "RELEASE", juce::String (rel, 0) + " ms", ! dynOn);
}

void BandEditorRail::paintMatchPanel (juce::Graphics& g, const Layout& L) const
{
    auto label = L.matchLabel;
    g.setColour (textLabel);
    g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.16f));
    g.drawText ("EQ MATCH", label, juce::Justification::centredLeft);

    auto srcChip = juce::Rectangle<float> ((float) label.getRight() - 78.0f, (float) label.getY(), 36.0f, 16.0f);
    auto refChip = juce::Rectangle<float> ((float) label.getRight() - 38.0f, (float) label.getY(), 36.0f, 16.0f);
    drawChip (g, srcChip, proc.hasSource(),    "SRC", 8.5f, 0.06f);
    drawChip (g, refChip, proc.hasReference(), "REF", 8.5f, 0.06f);

    g.setColour (textLabel);
    g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.16f));
    g.drawText ("MATCH AMOUNT", L.matchAmtRow.withHeight (14), juce::Justification::centredLeft);
    g.setColour (text2);
    g.setFont (Fonts::mono (12.0f, true));
    g.drawText (juce::String (juce::roundToInt (apvts.getRawParameterValue (ids::matchamount)->load())) + " %",
                L.matchAmtRow.withHeight (14), juce::Justification::centredRight);

    const bool capturing = proc.isCapturing();
    drawToggle (g, L.captureBtn.toFloat(), capturing, capturing ? "CAPTURING" : "CAPTURE");

    auto mb = L.matchBtn.toFloat();
    const bool canMatch = proc.hasMatchData();
    g.setColour (canMatch ? accentVio.withAlpha (0.16f) : whiteAlpha (0.05f));
    g.fillRoundedRectangle (mb, 8.0f);
    g.setColour (canMatch ? accentVio.withAlpha (0.45f) : whiteAlpha (0.08f));
    g.drawRoundedRectangle (mb.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (canMatch ? juce::Colour (0xffb6abff) : text3.withAlpha (0.5f));
    g.setFont (Fonts::grotesk (10.0f, Fonts::bold).withExtraKerningFactor (0.12f));
    g.drawText ("MATCH", mb, juce::Justification::centred);
}

void BandEditorRail::paintBottomBar (juce::Graphics& g, const Layout& L) const
{
    const bool ms   = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const bool hq   = apvts.getRawParameterValue (ids::hq)->load() > 0.5f;
    const float out = apvts.getRawParameterValue (ids::output)->load();

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

    const bool ag = apvts.getRawParameterValue (ids::autogain)->load() > 0.5f;
    juce::String autoLabel = "AUTO GAIN";
    if (ag) autoLabel << "   " << juce::String (proc.getAutoGainTrimDb(), 1) << " dB";

    drawToggle (g, L.modeBtn.toFloat(), ms, ms ? "MID/SIDE" : "STEREO");
    drawToggle (g, L.hqBtn.toFloat(), hq, "HQ OVERSAMPLING");
    drawToggle (g, L.autoBtn.toFloat(), ag, autoLabel);
}

bool BandEditorRail::handleSubRegionClick (const Layout& L, juce::Point<float> p, int s)
{
    if (showDyn)
    {
        if (! sitsOnZeroLine (selectedType()) && L.dynEnable.toFloat().contains (p))
        {
            setBool (ids::dynOn (s), ! (apvts.getRawParameterValue (ids::dynOn (s))->load() > 0.5f));
            refresh();
            return true;
        }
        return false;
    }

    if (isCut (selectedType()))
    {
        for (int i = 0; i < 3; ++i)
            if (segment (L.slopeRow, i, 3, 5.0f).contains (p))
            {
                setChoice (ids::slope (s), i);
                repaint();
                return true;
            }
    }

    return false;
}

void BandEditorRail::mouseDown (const juce::MouseEvent& e)
{
    auto L = computeLayout();
    const int s = selected();
    const auto p = e.position;

    // EQ | DYN tab
    if (L.eqTab.toFloat().contains (p))  { showDyn = false; resized(); repaint(); return; }
    if (L.dynTab.toFloat().contains (p)) { showDyn = true;  resized(); repaint(); return; }

    for (int i = 0; i < numFilterTypes; ++i)
        if (segment (L.typeChips, i, numFilterTypes, 5.0f).contains (p))
        {
            setChoice (ids::type (s), i);
            refresh();
            return;
        }

    if (handleSubRegionClick (L, p, s))
        return;

    auto onBtn = juce::Rectangle<int> (L.onSolo).removeFromLeft (L.onSolo.getWidth() / 2 - 3).toFloat();
    auto soloBtn = juce::Rectangle<int> (L.onSolo).removeFromRight (L.onSolo.getWidth() / 2 - 3).toFloat();
    if (onBtn.contains (p))   { setBool (ids::on (s),   ! (apvts.getRawParameterValue (ids::on (s))->load() > 0.5f)); repaint(); return; }
    if (soloBtn.contains (p)) { setBool (ids::solo (s), ! (apvts.getRawParameterValue (ids::solo (s))->load() > 0.5f)); repaint(); return; }

    for (int i = 0; i < 3; ++i)
        if (segment (L.channelRow, i, 3, 5.0f).contains (p))
        {
            setChoice (ids::channel (s), i);
            repaint();
            return;
        }

    if (L.modeBtn.toFloat().contains (p)) { setChoice (ids::mode, apvts.getRawParameterValue (ids::mode)->load() > 0.5f ? 0 : 1); repaint(); return; }
    if (L.hqBtn.toFloat().contains (p))   { setBool (ids::hq, ! (apvts.getRawParameterValue (ids::hq)->load() > 0.5f)); repaint(); return; }
    if (L.autoBtn.toFloat().contains (p)) { setBool (ids::autogain, ! (apvts.getRawParameterValue (ids::autogain)->load() > 0.5f)); repaint(); return; }

    if (L.captureBtn.toFloat().contains (p)) { if (onCapture) { onCapture(); } repaint(); return; }
    if (L.matchBtn.toFloat().contains (p) && proc.hasMatchData()) { if (onMatch) { onMatch(); } repaint(); return; }
}

} // namespace zeq
