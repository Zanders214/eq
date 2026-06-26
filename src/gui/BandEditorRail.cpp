#include "BandEditorRail.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"

namespace zeq
{
using namespace theme;

namespace
{
    // ---- panel metrics ------------------------------------------------------
    constexpr int kPanelW   = 300; // natural width reported by getDesiredSize()
    constexpr int kPad      = 14;
    constexpr int kHeaderH  = 18;
    constexpr int kTypeH    = 26;
    constexpr int kSliderH  = 40;  // 16 label + 24 control
    constexpr int kSlopeH   = 22;
    constexpr int kStereoH  = 26;
    constexpr int kDynHeadH = 30;
    constexpr int kDynDirH  = 22;
    constexpr int kDynSldH  = 38;
    constexpr int kMatchLblH = 16;
    constexpr int kMatchAmtH = 38;
    constexpr int kMatchBtnH = 32;
    constexpr int kGap      = 12;  // between major sections
    constexpr int kSub      = 6;   // small in-section gap

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

bool BandEditorRail::selectedZeroLine() const
{
    const auto t = selectedType();
    if (sitsOnZeroLine (t))
        return true;
    // Defensive forward-compat: until Branch 5 classifies the new shapes, infer from the
    // choice name (Band-Pass / All-Pass sit on the zero line; Tilt Shelf keeps its gain).
    const auto name = choiceName (ids::type (selected()), (int) t);
    return name.containsIgnoreCase ("pass") || name.equalsIgnoreCase ("notch");
}

juce::AudioParameterChoice* BandEditorRail::choiceParam (const juce::String& id) const
{
    return dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
}

int BandEditorRail::choiceIndex (const juce::String& id) const
{
    return (int) apvts.getRawParameterValue (id)->load();
}

juce::String BandEditorRail::choiceName (const juce::String& id, int idx) const
{
    if (auto* p = choiceParam (id))
        if (idx >= 0 && idx < p->choices.size())
            return p->choices[idx];
    return {};
}

juce::String BandEditorRail::typeChipLabel (const juce::String& n)
{
    if (n == "High Pass")  return "HP";
    if (n == "Low Shelf")  return "LO";
    if (n == "Bell")       return "BELL";
    if (n == "Notch")      return "NTCH";
    if (n == "High Shelf") return "HI";
    if (n == "Low Pass")   return "LP";
    if (n == "Tilt Shelf") return "TILT";
    if (n == "Band-Pass")  return "BP";
    if (n == "All-Pass")   return "AP";
    // Fallback for any unforeseen shape: initials of its words (so it never crashes/blanks).
    juce::String s;
    for (auto& w : juce::StringArray::fromTokens (n, " -", ""))
        if (w.isNotEmpty())
            s << w[0];
    return s.toUpperCase().substring (0, 4);
}

juce::String BandEditorRail::slopeChipLabel (const juce::String& n)
{
    if (n.equalsIgnoreCase ("Brickwall"))
        return "BW";
    // "12 dB/oct" -> "12"
    auto num = n.upToFirstOccurrenceOf (" ", false, false).trim();
    return num.isNotEmpty() ? num : n;
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
    gainSlider.setEnabled (! selectedZeroLine());
    gainSlider.getProperties().set ("gradient",
        apvts.getRawParameterValue (ids::gain (selected()))->load() >= 0.0f ? "warm" : "cool");
    repaint();
}

bool BandEditorRail::hasLiveReadout() const
{
    // The selected band shows live gain reduction while its dynamic EQ is active.
    return ! selectedZeroLine()
        && apvts.getRawParameterValue (ids::dynOn (selected()))->load() > 0.5f;
}

BandEditorRail::Layout BandEditorRail::computeLayout() const
{
    Layout L;
    L.gainShown    = ! selectedZeroLine();
    L.slopeShown   = isCut (selectedType());
    L.dynAvailable = ! selectedZeroLine();
    L.expertShown  = dynExpertOpen && L.dynAvailable;

    auto full = getLocalBounds();
    auto r = full.reduced (kPad);
    int  y = kPad;   // running natural-height accumulator (constants only → robust if bounds are short)

    auto take = [&r, &y] (int h, int gapAfter) -> juce::Rectangle<int>
    {
        auto box = r.removeFromTop (h);
        r.removeFromTop (gapAfter);
        y += h + gapAfter;
        return box;
    };

    L.header    = take (kHeaderH, 10);
    L.typeChips = take (kTypeH, kGap);
    L.freqBlock = take (kSliderH, kSub);
    if (L.gainShown)
        L.gainBlock = take (kSliderH, kSub);
    L.qBlock    = take (kSliderH, kSub);
    if (L.slopeShown)
        L.slopeRow = take (kSlopeH, kSub);
    r.removeFromTop (4); y += 4;
    L.stereoRow = take (kStereoH, kGap);

    L.dynHead = take (kDynHeadH, L.expertShown ? kSub : kGap);
    if (L.expertShown)
    {
        L.dynDir = take (kDynDirH, kSub);
        L.dynS0  = take (kDynSldH, kSub);
        L.dynS1  = take (kDynSldH, kSub);
        L.dynS2  = take (kDynSldH, kSub);
        L.dynS3  = take (kDynSldH, kGap);
    }

    L.matchLabel  = take (kMatchLblH, kSub);
    L.matchAmtRow = take (kMatchAmtH, kSub);
    auto btnRow   = take (kMatchBtnH, 0);
    L.captureBtn  = btnRow.withWidth (btnRow.getWidth() / 2 - 3);
    L.matchBtn    = btnRow.withTrimmedLeft (btnRow.getWidth() / 2 + 3);

    L.totalHeight = y + kPad;
    return L;
}

juce::Rectangle<int> BandEditorRail::getDesiredSize() const
{
    return { 0, 0, kPanelW, computeLayout().totalHeight };
}

juce::Rectangle<float> BandEditorRail::segment (juce::Rectangle<int> row, int i, int n, float gap) const
{
    const float w = ((float) row.getWidth() - gap * static_cast<float> (n - 1)) / (float) juce::jmax (1, n);
    return { (float) row.getX() + (w + gap) * (float) i, (float) row.getY(), w, (float) row.getHeight() };
}

void BandEditorRail::resized()
{
    auto L = computeLayout();

    freqSlider.setVisible (true);
    qSlider.setVisible    (true);
    gainSlider.setVisible (L.gainShown);
    const bool expert = L.expertShown;
    threshSlider.setVisible  (expert);
    rangeSlider.setVisible   (expert);
    attackSlider.setVisible  (expert);
    releaseSlider.setVisible (expert);

    freqSlider.setBounds (L.freqBlock.withTrimmedTop (16));
    if (L.gainShown)
        gainSlider.setBounds (L.gainBlock.withTrimmedTop (16));
    qSlider.setBounds (L.qBlock.withTrimmedTop (16));

    if (expert)
    {
        threshSlider.setBounds  (L.dynS0.withTrimmedTop (14));
        rangeSlider.setBounds   (L.dynS1.withTrimmedTop (14));
        attackSlider.setBounds  (L.dynS2.withTrimmedTop (14));
        releaseSlider.setBounds (L.dynS3.withTrimmedTop (14));
    }

    matchAmountSlider.setBounds (L.matchAmtRow.withTrimmedTop (18));
}

void BandEditorRail::paint (juce::Graphics& g)
{
    auto L = computeLayout();

    // floating card (sized to the natural content height; the rest of any taller bounds
    // stays empty until Branch 2 sizes the panel to getDesiredSize()).
    auto card = getLocalBounds().withHeight (juce::jmin (getHeight(), L.totalHeight)).toFloat();
    juce::Path cardPath;
    cardPath.addRoundedRectangle (card.reduced (0.5f), 14.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 24, { 0, 8 }).drawForPath (g, cardPath);
    g.setGradientFill (juce::ColourGradient (panelTop, card.getCentreX(), card.getY(),
                                             panelBase, card.getCentreX(), card.getBottom(), false));
    g.fillPath (cardPath);
    g.setColour (whiteAlpha (0.08f));
    g.strokePath (cardPath, juce::PathStrokeType (1.0f));

    const int s = selected();

    // header: colour dot + band number
    const float freq = apvts.getRawParameterValue (ids::freq (s))->load();
    const auto  col  = colourForFreq (freq);
    auto header = L.header;
    auto dot = header.removeFromLeft (12).withSizeKeepingCentre (12, 12).toFloat();
    NeonLookAndFeel::glow (g, dot, col, 6.0f, 0.7f);
    g.setColour (col);
    g.fillEllipse (dot);
    g.setColour (text1);
    g.setFont (Fonts::grotesk (14.0f, Fonts::semibold));
    g.drawText ("BAND " + juce::String (s + 1), header.withTrimmedLeft (9), juce::Justification::centredLeft);

    paintBandControls (g, L, s);
    paintDynSection   (g, L, s);
    paintMatchSection (g, L);
}

void BandEditorRail::paintBandControls (juce::Graphics& g, const Layout& L, int s) const
{
    // type / shape chips (read live from the APVTS choice param)
    const int typeSel = choiceIndex (ids::type (s));
    if (auto* tp = choiceParam (ids::type (s)))
    {
        const int n = tp->choices.size();
        for (int i = 0; i < n; ++i)
            drawChip (g, segment (L.typeChips, i, n, 5.0f), typeSel == i,
                      typeChipLabel (tp->choices[i]), 9.0f, 0.02f);
    }

    const float freq = apvts.getRawParameterValue (ids::freq (s))->load();
    const float gain = apvts.getRawParameterValue (ids::gain (s))->load();
    const float q    = apvts.getRawParameterValue (ids::q (s))->load();

    drawSliderHead (g, L.freqBlock, "FREQUENCY", fmtFreq (freq) + "  " + noteName (freq), false);
    if (L.gainShown)
        drawSliderHead (g, L.gainBlock, "GAIN", fmtGain (gain), false);
    drawSliderHead (g, L.qBlock, "Q", "Q " + juce::String (q, 2), false);

    // slope picker (cuts only, read live so 72/96/Brickwall appear automatically)
    if (L.slopeShown)
    {
        const int slopeSel = choiceIndex (ids::slope (s));
        if (auto* sp = choiceParam (ids::slope (s)))
        {
            const int n = sp->choices.size();
            for (int i = 0; i < n; ++i)
                drawChip (g, segment (L.slopeRow, i, n, 5.0f), slopeSel == i,
                          slopeChipLabel (sp->choices[i]), 9.0f, 0.02f);
        }
    }

    // per-band stereo placement (labels follow the global Stereo/MS domain)
    const bool ms = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const int  ch = choiceIndex (ids::channel (s));
    const std::array<const char*, 3> lr  { "L + R", "L", "R" };
    const std::array<const char*, 3> msl { "M + S", "M", "S" };
    for (int i = 0; i < 3; ++i)
        drawChip (g, segment (L.stereoRow, i, 3, 5.0f), ch == i,
                  ms ? msl[(size_t) i] : lr[(size_t) i], 9.0f, 0.06f);
}

void BandEditorRail::paintDynSection (juce::Graphics& g, const Layout& L, int s) const
{
    if (! L.dynAvailable)
    {
        g.setColour (text3.withAlpha (0.55f));
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.04f));
        g.drawText ("DYNAMICS — BELL / SHELF ONLY", L.dynHead, juce::Justification::centredLeft);
        return;
    }

    const bool dynOn = apvts.getRawParameterValue (ids::dynOn (s))->load() > 0.5f;
    const float live = proc.getDynGainDb (s);

    // header: DYNAMICS on/off toggle (left) + expand chevron (right)
    auto head = L.dynHead;
    auto chevron = head.removeFromRight (26);
    head.removeFromRight (6);
    juce::String dynLabel = dynOn ? "DYNAMICS ON" : "DYNAMICS";
    if (dynOn && std::abs (live) > 0.05f)
        dynLabel = "DYN  " + juce::String (live, 1) + " dB";
    drawToggle (g, head.toFloat(), dynOn, dynLabel);

    // expand/collapse chevron (drawn as a triangle so it never depends on a glyph)
    {
        auto c = chevron.toFloat();
        g.setColour (dynExpertOpen ? accent.withAlpha (0.16f) : whiteAlpha (0.06f));
        g.fillRoundedRectangle (c, 6.0f);
        g.setColour (dynExpertOpen ? accent.withAlpha (0.30f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (c.reduced (0.5f), 6.0f, 1.0f);
        const auto cc = c.getCentre();
        juce::Path tri;
        if (dynExpertOpen)
            tri.addTriangle (cc.x - 4.0f, cc.y - 2.0f, cc.x + 4.0f, cc.y - 2.0f, cc.x, cc.y + 3.0f);
        else
            tri.addTriangle (cc.x - 2.0f, cc.y - 4.0f, cc.x - 2.0f, cc.y + 4.0f, cc.x + 3.0f, cc.y);
        g.setColour (dynExpertOpen ? accent : text3);
        g.fillPath (tri);
    }

    if (! L.expertShown)
        return;

    // expert sub-pane: REACT direction + threshold/range/attack/release
    const int dynDir = choiceIndex (ids::dynDir (s));
    {
        auto row = L.dynDir;
        auto labelArea = row.removeFromLeft (row.getWidth() / 2);
        g.setColour (dynOn ? textLabel : textLabel.withAlpha (0.5f));
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.16f));
        g.drawText ("REACT", labelArea, juce::Justification::centredLeft);
        drawChip (g, segment (row, 0, 2, 6.0f), dynDir == 0, "OVER",  9.0f, 0.04f);
        drawChip (g, segment (row, 1, 2, 6.0f), dynDir == 1, "UNDER", 9.0f, 0.04f);
    }

    const float thr = apvts.getRawParameterValue (ids::dynThresh (s))->load();
    const float rng = apvts.getRawParameterValue (ids::dynRange (s))->load();
    const float att = apvts.getRawParameterValue (ids::dynAttack (s))->load();
    const float rel = apvts.getRawParameterValue (ids::dynRelease (s))->load();

    drawSliderHead (g, L.dynS0, "THRESHOLD", juce::String (thr, 1) + " dB", ! dynOn);
    drawSliderHead (g, L.dynS1, "RANGE",
                    (rng >= 0 ? "+" : "") + juce::String (rng, 1) + " dB"
                    + (dynOn && std::abs (live) > 0.05f ? "  (" + juce::String (live, 1) + ")" : ""),
                    ! dynOn);
    drawSliderHead (g, L.dynS2, "ATTACK",  juce::String (att, att < 10 ? 1 : 0) + " ms", ! dynOn);
    drawSliderHead (g, L.dynS3, "RELEASE", juce::String (rel, 0) + " ms", ! dynOn);
}

void BandEditorRail::paintMatchSection (juce::Graphics& g, const Layout& L) const
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

bool BandEditorRail::handleDynClick (const Layout& L, juce::Point<float> p, int s)
{
    if (! L.dynAvailable)
        return false;

    auto head = L.dynHead;
    auto chevron = head.removeFromRight (26);
    head.removeFromRight (6);
    if (chevron.toFloat().contains (p))
    {
        dynExpertOpen = ! dynExpertOpen;
        resized();
        repaint();
        return true;
    }
    if (head.toFloat().contains (p))
    {
        const bool now = ! (apvts.getRawParameterValue (ids::dynOn (s))->load() > 0.5f);
        setBool (ids::dynOn (s), now);
        if (now)
            dynExpertOpen = true;   // turning dynamics on reveals the expert pane
        resized();
        repaint();
        return true;
    }

    if (! L.expertShown)
        return false;

    auto dirRow = L.dynDir;
    dirRow.removeFromLeft (dirRow.getWidth() / 2);
    for (int i = 0; i < 2; ++i)
        if (segment (dirRow, i, 2, 6.0f).contains (p))
        {
            setChoice (ids::dynDir (s), i);
            repaint();
            return true;
        }
    return false;
}

void BandEditorRail::mouseDown (const juce::MouseEvent& e)
{
    auto L = computeLayout();
    const int s = selected();
    const auto p = e.position;

    // type / shape chips
    if (auto* tp = choiceParam (ids::type (s)))
    {
        const int n = tp->choices.size();
        for (int i = 0; i < n; ++i)
            if (segment (L.typeChips, i, n, 5.0f).contains (p))
            {
                setChoice (ids::type (s), i);
                refresh();
                return;
            }
    }

    // slope chips (cuts only)
    if (L.slopeShown)
        if (auto* sp = choiceParam (ids::slope (s)))
        {
            const int n = sp->choices.size();
            for (int i = 0; i < n; ++i)
                if (segment (L.slopeRow, i, n, 5.0f).contains (p))
                {
                    setChoice (ids::slope (s), i);
                    repaint();
                    return;
                }
        }

    // stereo placement chips
    for (int i = 0; i < 3; ++i)
        if (segment (L.stereoRow, i, 3, 5.0f).contains (p))
        {
            setChoice (ids::channel (s), i);
            repaint();
            return;
        }

    if (handleDynClick (L, p, s))
        return;

    // EQ-match buttons
    if (L.captureBtn.toFloat().contains (p)) { if (onCapture) { onCapture(); } repaint(); return; }
    if (L.matchBtn.toFloat().contains (p) && proc.hasMatchData()) { if (onMatch) { onMatch(); } repaint(); return; }
}

} // namespace zeq
