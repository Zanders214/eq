#include "EqGraphComponent.h"
#include "NeonLookAndFeel.h"

namespace zeq
{
using namespace theme;

EqGraphComponent::EqGraphComponent (ZandersEqAudioProcessor& p)
    : proc (p), apvts (p.getApvts())
{
    setOpaque (true);
}

// ---- parameter helpers ------------------------------------------------------
void EqGraphComponent::setParam (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}
void EqGraphComponent::setChoice (const juce::String& id, int idx)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
}
void EqGraphComponent::setBool (const juce::String& id, bool v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (v ? 1.0f : 0.0f);
}
void EqGraphComponent::beginGesture (const juce::String& id) { if (auto* p = apvts.getParameter (id)) p->beginChangeGesture(); }
void EqGraphComponent::endGesture   (const juce::String& id) { if (auto* p = apvts.getParameter (id)) p->endChangeGesture(); }

// ---- model reads ------------------------------------------------------------
bool EqGraphComponent::anySolo() const
{
    for (int i = 0; i < numBands; ++i)
        if (apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f)
            return true;
    return false;
}

EqGraphComponent::BandView EqGraphComponent::readBand (int i) const
{
    BandView b;
    b.type  = static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (i))->load());
    b.freq  = apvts.getRawParameterValue (ids::freq (i))->load();
    b.gain  = apvts.getRawParameterValue (ids::gain (i))->load();
    b.q     = apvts.getRawParameterValue (ids::q (i))->load();
    b.slope = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (i))->load());
    b.on    = apvts.getRawParameterValue (ids::on (i))->load() > 0.5f;
    const bool solo = apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f;
    b.live  = b.on && (! anySolo() || solo);
    b.channel = (int) apvts.getRawParameterValue (ids::channel (i))->load();
    b.dynOn = apvts.getRawParameterValue (ids::dynOn (i))->load() > 0.5f && ! sitsOnZeroLine (b.type);
    b.range = apvts.getRawParameterValue (ids::dynRange (i))->load();
    b.dynGain = proc.getDynGainDb (i);
    return b;
}

juce::Point<float> EqGraphComponent::nodePosition (const BandView& b) const
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float x = freqToX (b.freq, w);
    const float y = sitsOnZeroLine (b.type) ? gainToY (0.0f, h) : gainToY (b.gain, h);
    return { x, y };
}

int EqGraphComponent::spareBand() const
{
    for (int i = 0; i < numBands; ++i)
        if (apvts.getRawParameterValue (ids::on (i))->load() <= 0.5f)
            return i;
    return -1;
}

// Snap a frequency to the strongest analyzer bin within ~1/6-octave of the cursor.
float EqGraphComponent::snapToSpectrumPeak (float x) const
{
    const float w = (float) getWidth();
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

int EqGraphComponent::beginSpectrumGrab (float x)
{
    const int b = spareBand();
    if (b < 0)
        return -1;

    const float f = snapToSpectrumPeak (x);
    setChoice (ids::type (b), (int) FilterType::bell);
    setParam (ids::freq (b), f);
    setParam (ids::gain (b), 0.0f);
    setParam (ids::q (b), 1.4f);
    setBool (ids::on (b), true);
    proc.setSelectedBand (b);
    if (onSelectionChanged) onSelectionChanged();
    return b;
}

int EqGraphComponent::nodeAtPosition (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int i = 0; i < numBands; ++i)
    {
        const auto pos = nodePosition (readBand (i));
        const float d = pos.getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

// A dynamic-range handle, only when it has separated enough from its node.
int EqGraphComponent::rangeHandleAt (juce::Point<float> p) const
{
    const float w = (float) getWidth(), h = (float) getHeight();
    for (int i = 0; i < numBands; ++i)
    {
        const auto b = readBand (i);
        if (! b.dynOn) continue;
        const float x = freqToX (b.freq, w);
        const float yStatic = gainToY (b.gain, h);
        const float yRange  = gainToY (b.gain + b.range, h);
        if (std::abs (yRange - yStatic) < 11.0f) continue;   // too close to the node
        if (juce::Point<float> (x, yRange).getDistanceFrom (p) < 9.0f) return i;
    }
    return -1;
}

// ---- painting ---------------------------------------------------------------
void EqGraphComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (well, b.getCentreX(), 0.0f,
                                             wellDeep, b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    drawGrid (g);
    drawSpectrum (g);
    drawCurve (g);
    drawNodes (g);

    // inner-shadow lip
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (getLocalBounds(), 1);
}

void EqGraphComponent::drawGrid (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    struct FL { float f; const char* lab; };
    static const FL fl[] = {
        {30,nullptr},{40,nullptr},{50,nullptr},{60,nullptr},{80,nullptr},{100,"100"},
        {200,nullptr},{300,nullptr},{400,nullptr},{500,nullptr},{600,nullptr},{800,nullptr},
        {1000,"1k"},{2000,nullptr},{3000,nullptr},{4000,nullptr},{5000,nullptr},{6000,nullptr},
        {8000,nullptr},{10000,"10k"},{15000,nullptr},{20000,"20k"} };

    g.setFont (Fonts::mono (9.0f));
    for (auto& l : fl)
    {
        const float x = std::round (freqToX (l.f, w)) + 0.5f;
        g.setColour (whiteAlpha (l.lab ? 0.085f : 0.035f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        if (l.lab)
        {
            g.setColour (textFaint);
            g.drawText (l.lab, (int) x + 3, (int) h - 14, 30, 12, juce::Justification::left);
        }
    }

    for (int d : { -12, -6, 0, 6, 12 })
    {
        const float y = std::round (gainToY ((float) d, h)) + 0.5f;
        g.setColour (whiteAlpha (d == 0 ? 0.14f : 0.045f));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (textFaint);
        g.drawText ((d > 0 ? "+" : "") + juce::String (d), 4, (int) y - 12, 30, 12, juce::Justification::left);
    }
}

void EqGraphComponent::drawSpectrum (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();

    juce::Path fill;
    fill.startNewSubPath (0.0f, h);
    for (int p = 0; p < numPoints; ++p)
    {
        const float x = (float) p / (numPoints - 1) * w;
        const float y = h - scope[(size_t) p] * h * 0.92f;
        fill.lineTo (x, y);
    }
    fill.lineTo (w, h);
    fill.closeSubPath();

    juce::ColourGradient grad (cyan.withAlpha (0.30f), 0.0f, 0.0f, amber.withAlpha (0.30f), w, 0.0f, false);
    grad.addColour (0.34, violet.withAlpha (0.30f));
    grad.addColour (0.67, pink.withAlpha (0.30f));
    g.setGradientFill (grad);
    g.fillPath (fill);

    juce::Path top;
    for (int p = 0; p < numPoints; ++p)
    {
        const float x = (float) p / (numPoints - 1) * w;
        const float y = h - scope[(size_t) p] * h * 0.92f;
        if (p == 0) top.startNewSubPath (x, y); else top.lineTo (x, y);
    }
    g.setColour (whiteAlpha (0.30f));
    g.strokePath (top, juce::PathStrokeType (1.0f));

    g.setColour (text1.withAlpha (0.22f));
    for (int p = 0; p < numPoints; ++p)
    {
        const float x = (float) p / (numPoints - 1) * w;
        const float py = h - peaks[(size_t) p] * h * 0.92f;
        g.fillRect (x - 0.5f, py - 1.0f, 1.5f, 1.5f);
    }
}

void EqGraphComponent::drawCurve (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const double sr = proc.getActiveSampleRate();

    std::array<BandView, numBands> bv;
    for (int i = 0; i < numBands; ++i) bv[(size_t) i] = readBand (i);

    const int step = 2;
    juce::Path curve;
    for (int x = 0; x <= (int) w; x += step)
    {
        const float f = xToFreq ((float) x, w);
        double db = 0.0;
        for (auto& b : bv)
            db += bandMagnitudeDb (b.type, b.freq, effectiveGain (b), b.q, b.slope, b.live, f, sr);
        const float y = juce::jlimit (-2.0f, h + 2.0f, gainToY ((float) db, h));
        if (x == 0) curve.startNewSubPath ((float) x, y); else curve.lineTo ((float) x, y);
    }

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
        const float gdash[] = { 4.0f, 4.0f };
        juce::Path gdashed;
        juce::PathStrokeType (1.4f).createDashedStroke (gdashed, ghost, gdash, 2);
        g.setColour (accentVio.withAlpha (0.55f));
        g.fillPath (gdashed);
    }

    // selected band's individual (dashed) curve
    const int sel = proc.getSelectedBand();
    if (sel >= 0 && sel < numBands)
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
        const float dashes[] = { 3.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, bp, dashes, 2);
        g.setColour (colourForFreq (b.freq).withAlpha (0.5f));
        g.fillPath (dashed);
    }

    // glow + crisp main curve
    g.setColour (juce::Colour (0xffb4c8ff).withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (text1);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EqGraphComponent::drawNodes (juce::Graphics& g)
{
    const int sel = proc.getSelectedBand();
    const bool ms = apvts.getRawParameterValue (ids::mode)->load() > 0.5f;
    for (int i = 0; i < numBands; ++i)
    {
        const auto b = readBand (i);
        const auto pos = nodePosition (b);
        const auto col = colourForFreq (b.freq);
        const bool selected = (i == sel);
        const float r = 9.0f;

        if (selected)
        {
            g.setColour (col.withAlpha (0.35f));
            g.drawEllipse (pos.x - (r + 6), pos.y - (r + 6), (r + 6) * 2, (r + 6) * 2, 1.0f);
        }

        NeonLookAndFeel::glow (g, { pos.x - r, pos.y - r, r * 2, r * 2 }, col, selected ? 9.0f : 5.0f, 0.55f);

        g.setColour (col.withAlpha (b.live ? (selected ? 1.0f : 0.85f) : 0.25f));
        g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
        g.setColour (selected ? juce::Colours::white : whiteAlpha (0.5f));
        g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, selected ? 2.0f : 1.0f);

        g.setColour (b.live ? juce::Colour (0xff0a0b12) : whiteAlpha (0.6f));
        g.setFont (Fonts::grotesk (9.0f, Fonts::semibold));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2),
                    juce::Justification::centred);

        // channel-lane badge (first/second lane only) at the node's upper-right
        if (b.channel == 1 || b.channel == 2)
        {
            const juce::String letter = ms ? (b.channel == 1 ? "M" : "S")
                                           : (b.channel == 1 ? "L" : "R");
            const float br = 6.0f;
            juce::Point<float> bp (pos.x + r * 0.75f, pos.y - r * 0.95f);
            g.setColour (col);
            g.fillEllipse (bp.x - br, bp.y - br, br * 2, br * 2);
            g.setColour (juce::Colour (0xff0a0b12));
            g.setFont (Fonts::grotesk (8.0f, Fonts::bold));
            g.drawText (letter, juce::Rectangle<float> (bp.x - br, bp.y - br, br * 2, br * 2),
                        juce::Justification::centred);
        }

        // dynamic-EQ: range bracket + draggable handle + live-gain dot
        if (b.dynOn)
        {
            const float h = (float) getHeight();
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
    }
}

// ---- interaction ------------------------------------------------------------
void EqGraphComponent::mouseDown (const juce::MouseEvent& e)
{
    // dynamic-range handle takes priority over the node beneath it
    const int rh = rangeHandleAt (e.position);
    if (rh >= 0)
    {
        if (rh != proc.getSelectedBand())
        {
            proc.setSelectedBand (rh);
            if (onSelectionChanged) onSelectionChanged();
        }
        dragBand = rh;
        draggingRange = true;
        beginGesture (ids::dynRange (rh));
        return;
    }

    const int b = nodeAtPosition (e.position);
    if (b >= 0)
    {
        if (b != proc.getSelectedBand())
        {
            proc.setSelectedBand (b);
            if (onSelectionChanged) onSelectionChanged();
        }
        dragBand = b;
        const auto bv = readBand (b);
        draggingGain = ! sitsOnZeroLine (bv.type);
        beginGesture (ids::freq (b));
        if (draggingGain) beginGesture (ids::gain (b));
    }
    else
    {
        dragBand = -1;
        pendingGrab = true;          // may turn into a spectrum grab if dragged
        grabDownPos = e.position;
    }
}

void EqGraphComponent::mouseDrag (const juce::MouseEvent& e)
{
    const float w = (float) getWidth(), h = (float) getHeight();

    if (draggingRange && dragBand >= 0)
    {
        const float staticGain = readBand (dragBand).gain;
        const float y = juce::jlimit (0.0f, h, e.position.y);
        setParam (ids::dynRange (dragBand), juce::jlimit (-30.0f, 30.0f, yToGain (y, h) - staticGain));
        return;
    }

    // Promote a press-on-empty into a spectrum grab once the user actually drags.
    if (dragBand < 0 && pendingGrab && e.position.getDistanceFrom (grabDownPos) > 4.0f)
    {
        const int b = beginSpectrumGrab (grabDownPos.x);
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
    setParam (ids::freq (dragBand), juce::jlimit (fMin, fMax, xToFreq (x, w)));
    if (draggingGain)
        setParam (ids::gain (dragBand), juce::jlimit (-18.0f, 18.0f, yToGain (y, h)));
}

void EqGraphComponent::mouseUp (const juce::MouseEvent&)
{
    pendingGrab = false;
    if (dragBand < 0) return;
    if (draggingRange)
    {
        endGesture (ids::dynRange (dragBand));
        draggingRange = false;
        dragBand = -1;
        return;
    }
    endGesture (ids::freq (dragBand));
    if (draggingGain) endGesture (ids::gain (dragBand));
    dragBand = -1;
}

void EqGraphComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAtPosition (e.position);
    if (b >= 0)
    {
        int onCount = 0;
        for (int i = 0; i < numBands; ++i)
            if (apvts.getRawParameterValue (ids::on (i))->load() > 0.5f) ++onCount;
        if (onCount > 1)
            setBool (ids::on (b), false);   // "remove" = disable
    }
    else
    {
        for (int i = 0; i < numBands; ++i)
        {
            if (apvts.getRawParameterValue (ids::on (i))->load() <= 0.5f)
            {
                const float f = juce::jlimit (fMin, fMax, xToFreq (e.position.x, (float) getWidth()));
                setChoice (ids::type (i), (int) FilterType::bell);
                setParam (ids::freq (i), f);
                setParam (ids::gain (i), 0.0f);
                setParam (ids::q (i), 1.0f);
                setBool (ids::on (i), true);    // "add" = enable a spare band
                proc.setSelectedBand (i);
                if (onSelectionChanged) onSelectionChanged();
                break;
            }
        }
    }
}

void EqGraphComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = nodeAtPosition (e.position);
    if (b < 0) return;
    const float factor = w.deltaY > 0 ? 1.12f : 1.0f / 1.12f;
    const float q = juce::jlimit (0.1f, 18.0f, apvts.getRawParameterValue (ids::q (b))->load() * factor);
    setParam (ids::q (b), q);
}

// ---- animation --------------------------------------------------------------
void EqGraphComponent::updateAnimation()
{
    auto& a = proc.getAnalyzerFifo();
    if (a.blockReady.load (std::memory_order_acquire))
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
            const float target = juce::jlimit (0.0f, 1.12f, juce::jmap (db, -90.0f, 0.0f, 0.0f, 1.0f));

            scope[(size_t) p] += (target - scope[(size_t) p]) * 0.35f;
            peaks[(size_t) p] = juce::jmax (scope[(size_t) p], peaks[(size_t) p] - 0.004f);
        }
        a.blockReady.store (false, std::memory_order_release);
    }
    else
    {
        for (int p = 0; p < numPoints; ++p)
            peaks[(size_t) p] = juce::jmax (scope[(size_t) p], peaks[(size_t) p] - 0.004f);
    }

    if (proc.isCapturing())
    {
        accumulateTap (proc.getCaptureFifo (ZandersEqAudioProcessor::CaptureSlot::source), accumSrc, capFramesSrc);
        if (proc.sidechainActive())
            accumulateTap (proc.getCaptureFifo (ZandersEqAudioProcessor::CaptureSlot::reference), accumRef, capFramesRef);
    }
}

// Consume one capture FFT frame and accumulate RAW power per match bin (no tilt).
void EqGraphComponent::accumulateTap (AnalyzerFifo& f, std::array<double, kMatchBins>& accum, int& frames)
{
    if (! f.blockReady.load (std::memory_order_acquire))
        return;

    window.multiplyWithWindowingTable (f.fftData.data(), (size_t) AnalyzerFifo::fftSize);
    fft.performFrequencyOnlyForwardTransform (f.fftData.data());

    const double sr = proc.getActiveSampleRate();
    const int half = AnalyzerFifo::fftSize / 2;
    for (int k = 0; k < kMatchBins; ++k)
    {
        const double freq = matchBinFreq (k, kMatchBins);
        const float bin = (float) (freq * AnalyzerFifo::fftSize / sr);
        const int i0 = juce::jlimit (0, half - 2, (int) bin);
        const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) i0);
        const float mag = juce::jmap (frac, f.fftData[(size_t) i0], f.fftData[(size_t) i0 + 1]);
        const double magNorm = (double) mag / ((double) AnalyzerFifo::fftSize * 0.5);
        accum[(size_t) k] += magNorm * magNorm;
    }
    ++frames;
    f.blockReady.store (false, std::memory_order_release);
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
        capFramesSrc = capFramesRef = 0;
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

    int firstUsed = -1;
    for (int i = 0; i < numBands; ++i)
    {
        const auto& b = res.bands[(size_t) i];
        beginGesture (ids::type (i)); beginGesture (ids::freq (i)); beginGesture (ids::gain (i));
        beginGesture (ids::q (i));    beginGesture (ids::slope (i)); beginGesture (ids::on (i));

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
        setBool (ids::solo (i), false);

        endGesture (ids::type (i)); endGesture (ids::freq (i)); endGesture (ids::gain (i));
        endGesture (ids::q (i));    endGesture (ids::slope (i)); endGesture (ids::on (i));
    }

    if (firstUsed >= 0)
    {
        proc.setSelectedBand (firstUsed);
        if (onSelectionChanged) onSelectionChanged();
    }
}

} // namespace zeq
