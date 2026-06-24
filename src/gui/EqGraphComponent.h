#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "Theme.h"

namespace zeq
{

// The hero: log-frequency grid, a live FFT spectrum, the summed response curve,
// and one draggable node per band. Curve and audio come from the same math, so
// the display can never lie.
class EqGraphComponent : public juce::Component // NOSONAR(cpp:S5414): public data member onSelectionChanged is assigned externally (PluginEditor.cpp); must stay public
{
public:
    explicit EqGraphComponent (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override { haveCurveKey = false; }   // force a curve rebuild on size change

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Driven by the editor's timer: pull a new FFT frame and advance the spectrum.
    void updateAnimation();

    // EQ match
    void toggleCapture();   // start/stop capturing reference (sidechain) + source (main)
    void runMatch();        // fit the bands to the captured difference

    std::function<void()> onSelectionChanged;

private:
    struct BandView { FilterType type;
                      float freq;
                      float gain;
                      float q;
                      int slope;
                      bool on;
                      bool live;
                      int channel;
                      bool dynOn;
                      float range;
                      float dynGain; };

    BandView readBand (int i) const;
    bool anySolo() const;
    int  nodeAtPosition (juce::Point<float>) const;
    int  rangeHandleAt (juce::Point<float>) const;
    juce::Point<float> nodePosition (const BandView&) const;
    float effectiveGain (const BandView& b) const { return b.gain + (b.dynOn ? b.dynGain : 0.0f); }

    int   spareBand() const;                 // first disabled band, or -1
    float snapToSpectrumPeak (float x) const; // nearest analyzer peak frequency
    int   beginSpectrumGrab (float x);        // create+select a bell at a resonance

    void setParam (const juce::String& id, float realValue) const;
    void setChoice (const juce::String& id, int index) const;
    void setBool (const juce::String& id, bool v) const;
    void beginGesture (const juce::String& id) const;
    void endGesture (const juce::String& id) const;

    void drawGrid (juce::Graphics&) const;
    void drawSpectrum (juce::Graphics&);
    void drawCurve (juce::Graphics&) const;
    // Curve cache helpers (keep drawCurve's cognitive complexity low).
    bool curveCacheStale (const std::array<BandView, numBands>& bv, float w, float h, double sr) const;
    void rebuildCurve    (const std::array<BandView, numBands>& bv, float w, float h, double sr) const;
    void drawNodes (juce::Graphics&) const;
    void drawChannelBadge (juce::Graphics&, const BandView& b, juce::Point<float> pos,
                           juce::Colour col, float r, bool ms) const;
    void drawDynamicHandles (juce::Graphics&, const BandView& b, juce::Point<float> pos,
                             juce::Colour col) const;

    // EQ-match capture helpers
    void accumulateTap (AnalyzerFifo&, std::array<double, kMatchBins>&, int& frames) const;
    void finishCapture();

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    // FFT analyzer
    juce::dsp::FFT fft { AnalyzerFifo::order };
    juce::dsp::WindowingFunction<float> window { (size_t) AnalyzerFifo::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    static constexpr int numPoints = 240;
    std::array<float, numPoints> scope {};
    std::array<float, numPoints> peaks {};

    // Cached summed-response curve: the 360-point × 6-band magnitude sweep is
    // expensive, so it is rebuilt only when a band's static params (or the size)
    // change. While any band is in dynamic mode the curve genuinely animates, so
    // it is rebuilt every frame in that case (see drawCurve()). Mutable because
    // drawCurve() is const but caches into these.
    struct CurveKey
    {
        FilterType type;
        float freq;
        float gain;
        float q;
        int slope;
        bool live;
    };
    mutable juce::Path cachedCurve;
    mutable std::array<CurveKey, numBands> lastCurveKey {};
    mutable bool   haveCurveKey = false;
    mutable int    cachedCurveW = -1;
    mutable int    cachedCurveH = -1;
    mutable double cachedCurveSr = 0.0;   // sample rate affects coefficient shape — part of the key

    // EQ-match capture accumulators (message-thread only; raw power per log bin)
    std::array<double, kMatchBins> accumSrc {};
    std::array<double, kMatchBins> accumRef {};
    int capFramesSrc = 0;
    int capFramesRef = 0;

    // drag state
    int dragBand = -1;
    bool draggingGain = false;
    bool draggingRange = false;        // dragging a band's dynamic-range handle
    bool pendingGrab = false;          // mouse is down on empty graph, may become a grab
    juce::Point<float> grabDownPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraphComponent)
};

} // namespace zeq
