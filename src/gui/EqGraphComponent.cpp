#include "EqGraphComponent.h"
#include "NeonLookAndFeel.h"

#include <algorithm>

namespace zeq
{
using namespace theme;

EqGraphComponent::EqGraphComponent (ZandersEqAudioProcessor& p)
    : proc (p), apvts (p.getApvts())
{
    setOpaque (true);
}

// ---- parameter helpers ------------------------------------------------------
void EqGraphComponent::setParam (const juce::String& id, float v) const
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}
void EqGraphComponent::setChoice (const juce::String& id, int idx) const
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
}
void EqGraphComponent::setBool (const juce::String& id, bool v) const
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (v ? 1.0f : 0.0f);
}
void EqGraphComponent::beginGesture (const juce::String& id) const { if (auto* p = apvts.getParameter (id)) p->beginChangeGesture(); }
void EqGraphComponent::endGesture   (const juce::String& id) const { if (auto* p = apvts.getParameter (id)) p->endChangeGesture(); }

// Selection is single-source in the processor; this just routes the notifications.
void EqGraphComponent::focusBand (int slot)
{
    if (slot != proc.getSelectedBand())
        proc.setSelectedBand (slot);
    if (onSelectionChanged) onSelectionChanged();
    if (onBandFocused)      onBandFocused (slot);
}

// ---- model reads ------------------------------------------------------------
// anySolo mirrors the engine exactly (OR of every slot's solo flag, not gated by active),
// so the displayed `live` state can never disagree with what you hear.
bool EqGraphComponent::anySolo() const
{
    for (int i = 0; i < numBands; ++i)
        if (apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f)
            return true;
    return false;
}

int EqGraphComponent::firstActiveBand() const
{
    for (int i = 0; i < numBands; ++i)
        if (proc.isBandActive (i))
            return i;
    return 0;
}

EqGraphComponent::BandView EqGraphComponent::readBand (int i) const
{
    BandView b;
    b.type   = static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (i))->load());
    b.freq   = apvts.getRawParameterValue (ids::freq (i))->load();
    b.gain   = apvts.getRawParameterValue (ids::gain (i))->load();
    b.q      = apvts.getRawParameterValue (ids::q (i))->load();
    b.slope  = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (i))->load());
    b.on     = apvts.getRawParameterValue (ids::on (i))->load() > 0.5f;
    b.active = apvts.getRawParameterValue (ids::active (i))->load() > 0.5f;
    const bool solo = apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f;
    b.live   = b.active && b.on && (! anySolo() || solo);    // exactly the engine's gate
    b.channel = (int) apvts.getRawParameterValue (ids::channel (i))->load();
    b.dynOn   = apvts.getRawParameterValue (ids::dynOn (i))->load() > 0.5f && ! sitsOnZeroLine (b.type);
    b.range   = apvts.getRawParameterValue (ids::dynRange (i))->load();
    b.dynGain = proc.getDynGainDb (i);
    return b;
}

// Pure node placement (no Component state) — the one source of truth for where a dot sits.
juce::Rectangle<int> EqGraphComponent::nodeBounds (FilterType type, float freq, float gain,
                                                   float w, float h, float radius) noexcept
{
    if (w <= 0.0f || h <= 0.0f)
        return {};
    const float x = freqToX (freq, w);
    const float y = sitsOnZeroLine (type) ? gainToY (0.0f, h) : gainToY (gain, h);
    return juce::Rectangle<float> (x - radius, y - radius, radius * 2.0f, radius * 2.0f).toNearestInt();
}

juce::Point<float> EqGraphComponent::nodePosition (const BandView& b) const
{
    const auto r = nodeBounds (b.type, b.freq, b.gain, (float) getWidth(), (float) getHeight(), nodeRadius);
    return r.toFloat().getCentre();
}

// C4: node bounds for the floating panel; empty if the slot is inactive or off-screen.
juce::Rectangle<int> EqGraphComponent::getBandScreenBounds (int slot) const
{
    if (! proc.isBandActive (slot))
        return {};
    const auto b = readBand (slot);
    const auto r = nodeBounds (b.type, b.freq, b.gain, (float) getWidth(), (float) getHeight(), nodeRadius);
    if (r.isEmpty() || ! getLocalBounds().intersects (r))
        return {};
    return r;
}

// Snap a frequency to the strongest analyzer bin within ~1/6-octave of the cursor.
float EqGraphComponent::snapToSpectrumPeak (float x) const
{
    const auto w = (float) getWidth();
    const float centreFrac = juce::jlimit (0.0f, 1.0f, x / w);
    const int   centre = juce::roundToInt (centreFrac * (numPoints - 1));
    const int   span = juce::jmax (3, numPoints / 36);  // ~1/6 octave window

    int bestIdx = centre;
    float bestLvl = -1.0f;
    for (int i = juce::jmax (0, centre - span); i <= juce::jmin (numPoints - 1, centre + span); ++i)
        if (scope[(size_t) i] > bestLvl) { bestLvl = scope[(size_t) i]; bestIdx = i; }

    // Only snap if there's a meaningful peak; otherwise use the cursor frequency.
    const float frac = (bestLvl > 0.04f) ? (float) bestIdx / (numPoints - 1) : centreFrac;
    return juce::jlimit (fMin, fMax, fMin * std::exp (frac * logSpan));
}

// Pro-Q add: create a bell in the next free slot (addBand self-selects + self-undoes).
int EqGraphComponent::createBand (float freq)
{
    const int slot = proc.addBand (juce::jlimit (fMin, fMax, freq), 0.0f, FilterType::bell);
    if (slot >= 0)
    {
        if (onSelectionChanged) onSelectionChanged();   // addBand already set selection
        if (onBandFocused)      onBandFocused (slot);
    }
    return slot;
}

int EqGraphComponent::nodeAtPosition (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int i = 0; i < numBands; ++i)
    {
        if (! proc.isBandActive (i))
            continue;
        const auto pos = nodePosition (readBand (i));
        const float d = pos.getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

// A dynamic-range handle, only when it has separated enough from its node.
int EqGraphComponent::rangeHandleAt (juce::Point<float> p) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();
    for (int i = 0; i < numBands; ++i)
    {
        const auto b = readBand (i);
        if (! b.active || ! b.dynOn) continue;
        const float x = freqToX (b.freq, w);
        const float yStatic = gainToY (b.gain, h);
        const float yRange  = gainToY (b.gain + b.range, h);
        if (std::abs (yRange - yStatic) < 11.0f) continue;   // too close to the node
        if (juce::Point<float> (x, yRange).getDistanceFrom (p) < 9.0f) return i;
    }
    return -1;
}

// On-curve slope handle for the selected cut band (HP/LP): one octave into the stopband.
juce::Point<float> EqGraphComponent::slopeHandlePos (const BandView& b) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();
    const double sr = proc.getActiveSampleRate();
    const float hf = juce::jlimit (fMin, fMax, b.type == FilterType::highPass ? b.freq * 0.5f : b.freq * 2.0f);
    const double db = bandMagnitudeDb (b.type, b.freq, 0.0f, b.q, b.slope, true, hf, sr);
    return { freqToX (hf, w), juce::jlimit (0.0f, h, gainToY ((float) db, h)) };
}

int EqGraphComponent::slopeHandleAt (juce::Point<float> p) const
{
    const int sel = proc.getSelectedBand();
    if (sel < 0 || ! proc.isBandActive (sel)) return -1;
    const auto b = readBand (sel);
    if (! isCut (b.type)) return -1;
    return slopeHandlePos (b).getDistanceFrom (p) < 9.0f ? sel : -1;
}

// ---- analyzer config --------------------------------------------------------
int   EqGraphComponent::analyzerMode() const   { return (int) apvts.getRawParameterValue (ids::analyzerOn)->load(); }
float EqGraphComponent::analyzerFloorDb() const
{
    switch ((int) apvts.getRawParameterValue (ids::analyzerRange)->load())
    {
        case 0:  return 60.0f;
        case 2:  return 120.0f;
        default: return 90.0f;   // index 1
    }
}

// ---- painting ---------------------------------------------------------------
void EqGraphComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (well, b.getCentreX(), 0.0f,
                                             wellDeep, b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    drawGrid (g);
    if (pianoVisible) drawPianoOverlay (g);
    drawSpectrum (g);
    drawCurve (g);
    drawNodes (g);
    if (sketching) drawSketch (g);

    // inner-shadow lip
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (getLocalBounds(), 1);
}

void EqGraphComponent::drawGrid (juce::Graphics& g) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();
    struct FL { float f; bool label; };
    static const std::array<FL, 22> fl = { {
        {30,true},{40,false},{50,true},{60,false},{80,false},{100,true},
        {200,true},{300,false},{400,false},{500,true},{600,false},{800,false},
        {1000,true},{2000,true},{3000,false},{4000,false},{5000,true},{6000,false},
        {8000,false},{10000,true},{15000,false},{20000,true} } };

    g.setFont (Fonts::mono (9.0f));
    for (auto& l : fl)
    {
        const float x = std::round (freqToX (l.f, w)) + 0.5f;
        g.setColour (whiteAlpha (l.label ? 0.085f : 0.035f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        if (l.label)
        {
            g.setColour (textFaint);
            g.drawText (fmtFreq (l.f), (int) x + 3, (int) h - 14, 44, 12, juce::Justification::left);
        }
    }

    // EQ-gain dB grid (±18 dB, neon-brand mapping) with labels on BOTH edges.
    for (int d : { -12, -6, 0, 6, 12 })
    {
        const float y = std::round (gainToY ((float) d, h)) + 0.5f;
        g.setColour (whiteAlpha (d == 0 ? 0.14f : 0.045f));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (textFaint);
        const juce::String lab = (d > 0 ? "+" : "") + juce::String (d);
        g.drawText (lab, 4, (int) y - 12, 30, 12, juce::Justification::left);
        g.drawText (lab, (int) w - 34, (int) y - 12, 30, 12, juce::Justification::right);
    }
}

// Optional piano-keyboard strip along the frequency axis (toggled by Branch 2 / context menu).
void EqGraphComponent::drawPianoOverlay (juce::Graphics& g) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();
    const float kh = 20.0f;
    const float ky = h - kh;
    auto isSharp = [] (int m) { const int n = ((m % 12) + 12) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; };

    g.setColour (juce::Colours::black.withAlpha (0.32f));
    g.fillRect (0.0f, ky, w, kh);

    for (int m = 12; m <= 132; ++m)   // C0 .. ~ C10
    {
        const float f0 = 440.0f * std::pow (2.0f, (float) (m - 69) / 12.0f);
        const float f1 = 440.0f * std::pow (2.0f, (float) (m + 1 - 69) / 12.0f);
        if (f1 < fMin || f0 > fMax) continue;
        const float x0 = freqToX (juce::jlimit (fMin, fMax, f0), w);
        const float x1 = freqToX (juce::jlimit (fMin, fMax, f1), w);
        g.setColour (isSharp (m) ? juce::Colours::black.withAlpha (0.55f) : whiteAlpha (0.10f));
        g.fillRect (x0, ky, juce::jmax (1.0f, x1 - x0 - 0.5f), kh);

        if ((m % 12) == 0)   // C: octave marker line + label
        {
            g.setColour (whiteAlpha (0.16f));
            g.drawVerticalLine ((int) x0, 0.0f, ky);
            g.setColour (textFaint);
            g.setFont (Fonts::mono (8.0f));
            g.drawText (noteName (f0), (int) x0 + 1, (int) (ky + kh) - 10, 26, 10, juce::Justification::left);
        }
    }
}

void EqGraphComponent::drawSpectrumLayer (juce::Graphics& g, const std::array<float, numPoints>& sc,
                                          const std::array<float, numPoints>& pk, bool neon) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();

    juce::Path fill;
    fill.startNewSubPath (0.0f, h);
    for (int p = 0; p < numPoints; ++p)
    {
        const float x = (float) p / (numPoints - 1) * w;
        const float y = h - sc[(size_t) p] * h * 0.92f;
        fill.lineTo (x, y);
    }
    fill.lineTo (w, h);
    fill.closeSubPath();

    juce::Path top;
    for (int p = 0; p < numPoints; ++p)
    {
        const float x = (float) p / (numPoints - 1) * w;
        const float y = h - sc[(size_t) p] * h * 0.92f;
        if (p == 0) top.startNewSubPath (x, y); else top.lineTo (x, y);
    }

    if (neon)   // output: the brand ramp
    {
        juce::ColourGradient grad (cyan.withAlpha (0.30f), 0.0f, 0.0f, amber.withAlpha (0.30f), w, 0.0f, false);
        grad.addColour (0.34, violet.withAlpha (0.30f));
        grad.addColour (0.67, pink.withAlpha (0.30f));
        g.setGradientFill (grad);
        g.fillPath (fill);
        g.setColour (whiteAlpha (0.30f));
        g.strokePath (top, juce::PathStrokeType (1.0f));

        g.setColour (text1.withAlpha (0.22f));
        for (int p = 0; p < numPoints; ++p)
        {
            const float x = (float) p / (numPoints - 1) * w;
            const float py = h - pk[(size_t) p] * h * 0.92f;
            g.fillRect (x - 0.5f, py - 1.0f, 1.5f, 1.5f);
        }
    }
    else        // input: dim/ghosted gray fill behind (keeps the palette per C6)
    {
        g.setColour (textMuted.withAlpha (0.13f));
        g.fillPath (fill);
        g.setColour (textMuted.withAlpha (0.30f));
        g.strokePath (top, juce::PathStrokeType (1.0f));
    }
}

void EqGraphComponent::drawSpectrum (juce::Graphics& g) const
{
    const int mode = analyzerMode();
    if (mode == 0) return;                                   // Off
    if (mode == 1 || mode == 3) drawSpectrumLayer (g, preScope, prePeaks, false);  // Pre (input, gray, behind)
    if (mode == 2 || mode == 3) drawSpectrumLayer (g, scope,    peaks,    true);   // Post (output, neon, front)
}

bool EqGraphComponent::curveCacheStale (const std::array<BandView, numBands>& bv,
                                        float w, float h, double sr) const
{
    if (! haveCurveKey || (int) w != cachedCurveW || (int) h != cachedCurveH || sr != cachedCurveSr)
        return true;

    for (int i = 0; i < numBands; ++i)
    {
        const auto& b = bv[(size_t) i];
        const auto& k = lastCurveKey[(size_t) i];
        if (b.type != k.type || b.freq != k.freq || b.gain != k.gain
            || b.q != k.q || b.slope != k.slope || b.live != k.live)
            return true;
    }

    // A band in dynamic mode moves the curve every frame.
    for (const auto& b : bv)
        if (b.live && b.dynOn)
            return true;

    return false;
}

void EqGraphComponent::rebuildCurve (const std::array<BandView, numBands>& bv,
                                     float w, float h, double sr) const
{
    const int step = 2;
    cachedCurve.clear();
    for (int x = 0; x <= (int) w; x += step)
    {
        const float f = xToFreq ((float) x, w);
        double db = 0.0;
        for (const auto& b : bv)
            db += bandMagnitudeDb (b.type, b.freq, effectiveGain (b), b.q, b.slope, b.live, f, sr);
        const float y = juce::jlimit (-2.0f, h + 2.0f, gainToY ((float) db, h));
        if (x == 0) cachedCurve.startNewSubPath ((float) x, y); else cachedCurve.lineTo ((float) x, y);
    }

    for (int i = 0; i < numBands; ++i)
    {
        const auto& b = bv[(size_t) i];
        lastCurveKey[(size_t) i] = { b.type, b.freq, b.gain, b.q, b.slope, b.live };
    }
    cachedCurveW = (int) w;
    cachedCurveH = (int) h;
    cachedCurveSr = sr;
    haveCurveKey = true;
}

void EqGraphComponent::drawCurve (juce::Graphics& g) const
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();
    const double sr = proc.getActiveSampleRate();

    std::array<BandView, numBands> bv;
    for (int i = 0; i < numBands; ++i) bv[(size_t) i] = readBand (i);

    const int step = 2;

    if (curveCacheStale (bv, w, h, sr))
        rebuildCurve (bv, w, h, sr);

    const juce::Path& curve = cachedCurve;

    // faint fill to the 0 dB line
    juce::Path fill = curve;
    fill.lineTo (w, gainToY (0.0f, h));
    fill.lineTo (0.0f, gainToY (0.0f, h));
    fill.closeSubPath();
    g.setColour (text1.withAlpha (0.06f));
    g.fillPath (fill);

    // EQ-match ghost: the target (reference - source) difference curve
    if (proc.hasMatchData())
    {
        std::array<float, kMatchBins> tgt {};
        computeTargetDb (proc.getReferenceCurve(), proc.getSourceCurve(), kMatchBins, tgt.data());
        juce::Path ghost;
        for (int k = 0; k < kMatchBins; ++k)
        {
            const float gx = (float) k / (kMatchBins - 1) * w;
            const float gy = juce::jlimit (-2.0f, h + 2.0f, gainToY (tgt[(size_t) k], h));
            if (k == 0) ghost.startNewSubPath (gx, gy); else ghost.lineTo (gx, gy);
        }
        const std::array<float, 2> gdash = { 4.0f, 4.0f };
        juce::Path gdashed;
        juce::PathStrokeType (1.4f).createDashedStroke (gdashed, ghost, gdash.data(), 2);
        g.setColour (accentVio.withAlpha (0.55f));
        g.fillPath (gdashed);
    }

    // selected band's individual (dashed) curve
    if (const int sel = proc.getSelectedBand(); sel >= 0 && sel < numBands && proc.isBandActive (sel))
    {
        const auto& b = bv[(size_t) sel];
        juce::Path bp;
        for (int x = 0; x <= (int) w; x += step)
        {
            const float f = xToFreq ((float) x, w);
            const double db = bandMagnitudeDb (b.type, b.freq, effectiveGain (b), b.q, b.slope, b.live, f, sr);
            const float y = juce::jlimit (-2.0f, h + 2.0f, gainToY ((float) db, h));
            if (x == 0) bp.startNewSubPath ((float) x, y); else bp.lineTo ((float) x, y);
        }
        const std::array<float, 2> dashes = { 3.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, bp, dashes.data(), 2);
        g.setColour (colourForFreq (b.freq).withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // glow + crisp main curve
    g.setColour (juce::Colour (0xffb4c8ff).withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (text1);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// channel-lane badge (first/second lane only) at the node's upper-right
void EqGraphComponent::drawChannelBadge (juce::Graphics& g, const BandView& b,
                                         juce::Point<float> pos, juce::Colour col,
                                         float r, bool ms) const
{
    if (b.channel != 1 && b.channel != 2)
        return;

    juce::String letter;
    if (ms)
        letter = (b.channel == 1 ? "M" : "S");
    else
        letter = (b.channel == 1 ? "L" : "R");

    const float br = 6.0f;
    juce::Point bp { pos.x + r * 0.75f, pos.y - r * 0.95f };
    g.setColour (col);
    g.fillEllipse (bp.x - br, bp.y - br, br * 2, br * 2);
    g.setColour (juce::Colour (0xff0a0b12));
    g.setFont (Fonts::grotesk (8.0f, Fonts::bold));
    g.drawText (letter, juce::Rectangle<float> (bp.x - br, bp.y - br, br * 2, br * 2),
                juce::Justification::centred);
}

// dynamic-EQ: range bracket + draggable handle + live-gain dot
void EqGraphComponent::drawDynamicHandles (juce::Graphics& g, const BandView& b,
                                           juce::Point<float> pos, juce::Colour col) const
{
    if (! b.dynOn)
        return;

    const auto h = (float) getHeight();
    const float yRange = juce::jlimit (-2.0f, h + 2.0f, gainToY (b.gain + b.range, h));
    const float yLive  = juce::jlimit (-2.0f, h + 2.0f, gainToY (b.gain + b.dynGain, h));
    g.setColour (col.withAlpha (0.45f));
    g.drawLine (pos.x, pos.y, pos.x, yRange, 1.5f);
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (pos.x - 5.0f, yRange - 5.0f, 10.0f, 10.0f);
    g.setColour (col);
    g.drawEllipse (pos.x - 5.0f, yRange - 5.0f, 10.0f, 10.0f, 1.5f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (pos.x - 2.5f, yLive - 2.5f, 5.0f, 5.0f);
}

// on-curve slope handle for a cut band (drawn for the selected band only)
void EqGraphComponent::drawSlopeHandle (juce::Graphics& g, const BandView& b, bool selected) const
{
    if (! selected || ! isCut (b.type))
        return;
    const auto pos = slopeHandlePos (b);
    const auto col = colourForFreq (b.freq);
    NeonLookAndFeel::glow (g, { pos.x - 4.0f, pos.y - 4.0f, 8.0f, 8.0f }, col, 4.0f, 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (pos.x - 4.0f, pos.y - 4.0f, 8.0f, 8.0f);
    g.setColour (col);
    g.drawEllipse (pos.x - 4.0f, pos.y - 4.0f, 8.0f, 8.0f, 1.5f);
}

void EqGraphComponent::drawNodes (juce::Graphics& g) const
{
    const int sel = proc.getSelectedBand();
    const bool ms = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    const float r = nodeRadius;
    for (int i = 0; i < numBands; ++i)
    {
        if (! proc.isBandActive (i))
            continue;

        const auto b = readBand (i);
        const auto pos = nodePosition (b);
        const auto col = colourForFreq (b.freq);
        const bool selected = (i == sel);
        const bool hovered  = (i == hoverBand);

        if (selected)
        {
            g.setColour (col.withAlpha (0.35f));
            g.drawEllipse (pos.x - (r + 6), pos.y - (r + 6), (r + 6) * 2, (r + 6) * 2, 1.0f);
        }
        else if (hovered)
        {
            g.setColour (col.withAlpha (0.22f));
            g.drawEllipse (pos.x - (r + 4), pos.y - (r + 4), (r + 4) * 2, (r + 4) * 2, 1.0f);
        }

        NeonLookAndFeel::glow (g, { pos.x - r, pos.y - r, r * 2, r * 2 }, col,
                               selected ? 9.0f : (hovered ? 7.0f : 5.0f), 0.55f);

        g.setColour (col.withAlpha (b.live ? (selected ? 1.0f : (hovered ? 0.95f : 0.85f)) : 0.25f));
        g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
        g.setColour (selected ? juce::Colours::white : whiteAlpha (hovered ? 0.7f : 0.5f));
        g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, selected ? 2.0f : 1.0f);

        g.setColour (b.live ? juce::Colour (0xff0a0b12) : whiteAlpha (0.6f));
        g.setFont (Fonts::grotesk (9.0f, Fonts::semibold));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2),
                    juce::Justification::centred);

        drawChannelBadge (g, b, pos, col, r, ms);
        drawDynamicHandles (g, b, pos, col);
        drawSlopeHandle (g, b, selected);
    }
}

void EqGraphComponent::drawSketch (juce::Graphics& g) const
{
    if (sketchPts.size() < 2)
        return;
    juce::Path p;
    p.startNewSubPath (sketchPts.front());
    for (size_t i = 1; i < sketchPts.size(); ++i)
        p.lineTo (sketchPts[i]);
    g.setColour (accent.withAlpha (0.30f));
    g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---- interaction ------------------------------------------------------------
void EqGraphComponent::mouseDown (const juce::MouseEvent& e)
{
    // right-click a node to bypass it; right-click empty space for the view toggles
    if (e.mods.isPopupMenu())
    {
        if (const int b = nodeAtPosition (e.position); b >= 0)
        {
            const bool on = apvts.getRawParameterValue (ids::on (b))->load() > 0.5f;
            proc.recordUndoableEdit ([this, b, on] { setBool (ids::on (b), ! on); });
            focusBand (b);
            repaint();
        }
        else
        {
            showContextMenu();
        }
        return;
    }

    // EQ-Sketch: an empty-space press starts drawing a curve
    if (sketchActive && nodeAtPosition (e.position) < 0 && slopeHandleAt (e.position) < 0)
    {
        sketching = true;
        sketchPts.clear();
        sketchPts.push_back (e.position);
        return;
    }

    // on-curve slope handle (selected cut band) takes priority over the node beneath it
    if (const int sh = slopeHandleAt (e.position); sh >= 0)
    {
        dragBand = sh;
        draggingSlope = true;
        grabDownPos = e.position;
        dragStartSlopeIdx = (int) apvts.getRawParameterValue (ids::slope (sh))->load();
        proc.beginUndoTransaction();
        beginGesture (ids::slope (sh));
        return;
    }

    // dynamic-range handle takes priority over the node beneath it
    if (const int rh = rangeHandleAt (e.position); rh >= 0)
    {
        focusBand (rh);
        dragBand = rh;
        draggingRange = true;
        proc.beginUndoTransaction();
        beginGesture (ids::dynRange (rh));
        return;
    }

    const int b = nodeAtPosition (e.position);
    if (b >= 0)
    {
        focusBand (b);
        dragBand = b;
        const auto bv = readBand (b);
        draggingGain = ! sitsOnZeroLine (bv.type);
        proc.beginUndoTransaction();     // node move = one undo step (committed in mouseUp)
        beginGesture (ids::freq (b));
        if (draggingGain) beginGesture (ids::gain (b));
    }
    else
    {
        dragBand = -1;
        pendingGrab = true;              // a plain click creates; a drag becomes a spectrum grab
        grabDownPos = e.position;
    }
}

void EqGraphComponent::mouseDrag (const juce::MouseEvent& e)
{
    const auto w = (float) getWidth();
    const auto h = (float) getHeight();

    if (sketching)
    {
        sketchPts.push_back (e.position);
        repaint();
        return;
    }

    if (draggingSlope && dragBand >= 0)
    {
        const int n = slopeChoices().size();
        const int steps = juce::roundToInt ((e.position.y - grabDownPos.y) / 28.0f);  // drag down = steeper
        setChoice (ids::slope (dragBand), juce::jlimit (0, n - 1, dragStartSlopeIdx + steps));
        if (onBandFocused) onBandFocused (dragBand);
        return;
    }

    if (draggingRange && dragBand >= 0)
    {
        const float staticGain = readBand (dragBand).gain;
        const float y = juce::jlimit (0.0f, h, e.position.y);
        setParam (ids::dynRange (dragBand), juce::jlimit (-30.0f, 30.0f, yToGain (y, h) - staticGain));
        return;
    }

    // Promote a press-on-empty into a spectrum grab (a bell snapped to the nearest peak).
    if (dragBand < 0 && pendingGrab && e.position.getDistanceFrom (grabDownPos) > 4.0f)
    {
        const int b = createBand (snapToSpectrumPeak (grabDownPos.x));
        pendingGrab = false;
        if (b >= 0)
        {
            dragBand = b;
            draggingGain = true;
            beginGesture (ids::freq (b));
            beginGesture (ids::gain (b));
        }
    }

    if (dragBand < 0) return;
    const float x = juce::jlimit (0.0f, w, e.position.x);
    const float y = juce::jlimit (0.0f, h, e.position.y);
    if (! e.mods.isAltDown())      // Alt-drag constrains to gain-only (frequency held)
        setParam (ids::freq (dragBand), juce::jlimit (fMin, fMax, xToFreq (x, w)));
    if (draggingGain)
        setParam (ids::gain (dragBand), juce::jlimit (gainMin, gainMax, yToGain (y, h)));
    if (onBandFocused) onBandFocused (dragBand);   // selected node moved -> panel follows
}

void EqGraphComponent::mouseUp (const juce::MouseEvent& e)
{
    if (sketching)
    {
        sketching = false;
        commitSketch();
        sketchPts.clear();
        repaint();
        return;
    }

    if (dragBand >= 0)
    {
        if (draggingSlope)
        {
            endGesture (ids::slope (dragBand));
            proc.commitUndoTransaction();
            draggingSlope = false;
        }
        else if (draggingRange)
        {
            endGesture (ids::dynRange (dragBand));
            proc.commitUndoTransaction();
            draggingRange = false;
        }
        else
        {
            endGesture (ids::freq (dragBand));
            if (draggingGain) endGesture (ids::gain (dragBand));
            // A node move was bracketed in mouseDown; a grab-create was self-undone by addBand,
            // so commit is a harmless no-op there (pendingUndo is already cleared).
            proc.commitUndoTransaction();
        }
        dragBand = -1;
        draggingGain = false;
    }
    else if (pendingGrab && e.getNumberOfClicks() <= 1)
    {
        createBand (xToFreq (grabDownPos.x, (float) getWidth()));   // single click on empty space -> create
    }
    pendingGrab = false;
}

void EqGraphComponent::mouseMove (const juce::MouseEvent& e)
{
    const int hit = nodeAtPosition (e.position);
    if (hit != hoverBand) { hoverBand = hit; repaint(); }
}

void EqGraphComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoverBand != -1) { hoverBand = -1; repaint(); }
}

void EqGraphComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAtPosition (e.position);
    if (b >= 0)
    {
        // Close any gesture the preceding press opened, then remove the band (self-undoing).
        if (dragBand == b)
        {
            endGesture (ids::freq (b));
            if (draggingGain) endGesture (ids::gain (b));
            proc.commitUndoTransaction();
            dragBand = -1;
            draggingGain = false;
        }
        if (proc.activeBandCount() > 1)
            proc.removeBand (b);     // double-click a dot = remove

        focusBand (firstActiveBand());
        repaint();
    }
    // Empty space: the single-click create (mouseUp) already handled it; a double-click
    // there is a no-op so it never spawns a second band.
}

void EqGraphComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = nodeAtPosition (e.position);
    if (b < 0) return;
    const float factor = w.deltaY > 0 ? 1.12f : 1.0f / 1.12f;
    const float q = juce::jlimit (qMin, qMax, apvts.getRawParameterValue (ids::q (b))->load() * factor);
    proc.recordUndoableEdit ([this, b, q] { setParam (ids::q (b), q); });
}

// ---- view toggles + context menu --------------------------------------------
void EqGraphComponent::setPianoVisible (bool v)
{
    if (v != pianoVisible) { pianoVisible = v; repaint(); }
}

void EqGraphComponent::setSketchActive (bool v)
{
    if (v == sketchActive) return;
    sketchActive = v;
    if (! v) { sketching = false; sketchPts.clear(); }
    repaint();
}

void EqGraphComponent::showContextMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Piano keyboard overlay", true, pianoVisible);
    m.addItem (2, "EQ Sketch mode",         true, sketchActive);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this] (int r)
                     {
                         if (r == 1) setPianoVisible (! pianoVisible);
                         else if (r == 2) setSketchActive (! sketchActive);
                     });
}

// Turn the drawn left->right path into a series of bells approximating it (each via addBand).
void EqGraphComponent::commitSketch()
{
    if (sketchPts.size() < 2)
        return;

    const auto w = (float) getWidth();
    const auto h = (float) getHeight();

    auto pts = sketchPts;
    std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.x < b.x; });
    const float x0 = pts.front().x;
    const float x1 = pts.back().x;
    if (x1 - x0 < 8.0f)
        return;

    const int freeSlots = numBands - proc.activeBandCount();
    const int want = juce::jlimit (0, juce::jmin (6, freeSlots), 5);
    if (want <= 0)
        return;

    auto yAt = [&pts] (float x)
    {
        for (size_t i = 1; i < pts.size(); ++i)
            if (pts[i].x >= x)
            {
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                const float t = (b.x > a.x) ? (x - a.x) / (b.x - a.x) : 0.0f;
                return a.y + t * (b.y - a.y);
            }
        return pts.back().y;
    };

    for (int k = 0; k < want; ++k)
    {
        const float x = (want == 1) ? 0.5f * (x0 + x1)
                                    : x0 + (x1 - x0) * (float) k / (float) (want - 1);
        const float f = juce::jlimit (fMin, fMax, xToFreq (x, w));
        const float gdb = juce::jlimit (gainMin, gainMax, yToGain (yAt (x), h));
        proc.addBand (f, gdb, FilterType::bell);   // each is its own undo step
    }
}

// ---- animation --------------------------------------------------------------
void EqGraphComponent::updateAnimation()
{
    const float floorDb = analyzerFloorDb();
    reduceFifo (proc.getAnalyzerFifo(),      scope,    peaks,    floorDb);   // post-EQ (output)
    reduceFifo (proc.getPreEqAnalyzerFifo(), preScope, prePeaks, floorDb);   // pre-EQ (input)

    if (proc.isCapturing())
    {
        accumulateTap (proc.getCaptureFifo (ZandersEqAudioProcessor::CaptureSlot::source), accumSrc, capFramesSrc);
        if (proc.sidechainActive())
            accumulateTap (proc.getCaptureFifo (ZandersEqAudioProcessor::CaptureSlot::reference), accumRef, capFramesRef);
    }
}

// Pull one FFT frame from a FIFO and fold it into a log-spaced display scope. Lock-free:
// the audio thread only ever flips `blockReady`; we copy out and clear it.
void EqGraphComponent::reduceFifo (AnalyzerFifo& a, std::array<float, numPoints>& sc,
                                   std::array<float, numPoints>& pk, float floorDb)
{
    if (a.blockReady.load())
    {
        window.multiplyWithWindowingTable (a.fftData.data(), (size_t) AnalyzerFifo::fftSize);
        fft.performFrequencyOnlyForwardTransform (a.fftData.data());

        const double sr = proc.getActiveSampleRate();
        const int half = AnalyzerFifo::fftSize / 2;
        for (int p = 0; p < numPoints; ++p)
        {
            const float fr = (float) p / (numPoints - 1);
            const float freq = fMin * std::exp (fr * logSpan);
            const float bin = freq * (float) AnalyzerFifo::fftSize / (float) sr;
            const int i0 = juce::jlimit (0, half - 2, (int) bin);
            const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) i0);
            const float mag = juce::jmap (frac, a.fftData[(size_t) i0], a.fftData[(size_t) i0 + 1]);

            const float magNorm = mag / ((float) AnalyzerFifo::fftSize * 0.5f);
            float db = juce::Decibels::gainToDecibels (magNorm + 1.0e-9f);
            db += std::log2 (juce::jmax (freq, fMin) / fMin) * 2.2f;   // analyzer tilt
            const float target = juce::jlimit (0.0f, 1.12f, juce::jmap (db, -floorDb, 0.0f, 0.0f, 1.0f));

            sc[(size_t) p] += (target - sc[(size_t) p]) * 0.35f;
            pk[(size_t) p] = juce::jmax (sc[(size_t) p], pk[(size_t) p] - 0.004f);
        }
        a.blockReady.store (false);
    }
    else
    {
        for (int p = 0; p < numPoints; ++p)
            pk[(size_t) p] = juce::jmax (sc[(size_t) p], pk[(size_t) p] - 0.004f);
    }
}

// Consume one capture FFT frame and accumulate RAW power per match bin (no tilt).
void EqGraphComponent::accumulateTap (AnalyzerFifo& f, std::array<double, kMatchBins>& accum, int& frames) const
{
    if (! f.blockReady.load())
        return;

    window.multiplyWithWindowingTable (f.fftData.data(), (size_t) AnalyzerFifo::fftSize);
    fft.performFrequencyOnlyForwardTransform (f.fftData.data());

    const double sr = proc.getActiveSampleRate();
    const int half = AnalyzerFifo::fftSize / 2;
    for (int k = 0; k < kMatchBins; ++k)
    {
        const double freq = matchBinFreq (k, kMatchBins);
        const auto bin = (float) (freq * AnalyzerFifo::fftSize / sr);
        const int i0 = juce::jlimit (0, half - 2, (int) bin);
        const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) i0);
        const float mag = juce::jmap (frac, f.fftData[(size_t) i0], f.fftData[(size_t) i0 + 1]);
        const double magNorm = (double) mag / ((double) AnalyzerFifo::fftSize * 0.5);
        accum[(size_t) k] += magNorm * magNorm;
    }
    ++frames;
    f.blockReady.store (false);
}

void EqGraphComponent::toggleCapture()
{
    if (proc.isCapturing())
    {
        finishCapture();
    }
    else
    {
        accumSrc.fill (0.0); accumRef.fill (0.0);
        capFramesRef = 0;
        capFramesSrc = capFramesRef;
        proc.setCapturing (true);
    }
}

void EqGraphComponent::finishCapture()
{
    proc.setCapturing (false);

    const int minFrames = 8;   // need a representative span, not a momentary blip
    std::array<float, kMatchBins> mean {};
    if (capFramesSrc >= minFrames)
    {
        for (int k = 0; k < kMatchBins; ++k) mean[(size_t) k] = (float) (accumSrc[(size_t) k] / capFramesSrc);
        proc.storeCaptureCurve (ZandersEqAudioProcessor::CaptureSlot::source, mean.data(), kMatchBins);
    }
    if (capFramesRef >= minFrames)
    {
        double energy = 0.0;
        for (int k = 0; k < kMatchBins; ++k) { mean[(size_t) k] = (float) (accumRef[(size_t) k] / capFramesRef); energy += mean[(size_t) k]; }
        if (energy > 1.0e-9)   // only store a reference that actually carried signal
            proc.storeCaptureCurve (ZandersEqAudioProcessor::CaptureSlot::reference, mean.data(), kMatchBins);
    }
}

void EqGraphComponent::runMatch()
{
    if (! proc.hasMatchData())
        return;

    const float amount = apvts.getRawParameterValue (ids::matchamount)->load() / 100.0f;
    const auto res = fitMatch (proc.getReferenceCurve(), proc.getSourceCurve(),
                               kMatchBins, proc.getActiveSampleRate(), amount);

    proc.beginUndoTransaction();   // the whole match is a single undo step
    int firstUsed = -1;
    for (int i = 0; i < numBands; ++i)
    {
        const auto& b = res.bands[(size_t) i];
        beginGesture (ids::type (i)); beginGesture (ids::freq (i)); beginGesture (ids::gain (i));
        beginGesture (ids::q (i));    beginGesture (ids::slope (i)); beginGesture (ids::on (i));
        beginGesture (ids::active (i));

        setChoice (ids::type (i), (int) b.type);
        if (b.on)
        {
            setParam (ids::freq (i), b.freq);
            setParam (ids::gain (i), b.gain);
            setParam (ids::q (i), b.q);
            setChoice (ids::slope (i), slopeValueToIndex (b.slope));
            if (firstUsed < 0) firstUsed = i;
        }
        setBool (ids::on (i), b.on);
        setBool (ids::active (i), b.on);   // a fitted band exists in the pool; unused slots clear
        setBool (ids::solo (i), false);

        endGesture (ids::type (i)); endGesture (ids::freq (i)); endGesture (ids::gain (i));
        endGesture (ids::q (i));    endGesture (ids::slope (i)); endGesture (ids::on (i));
        endGesture (ids::active (i));
    }
    proc.commitUndoTransaction();

    if (firstUsed >= 0)
        focusBand (firstUsed);
}

} // namespace zeq
