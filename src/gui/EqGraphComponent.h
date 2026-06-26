#pragma once

#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "Theme.h"

namespace zeq
{

// The hero: a full-bleed log-frequency grid, the input + output FFT spectra, the summed
// response curve, and one draggable node per *active* band in the dynamic pool. Curve and
// audio come from the same math (bandMagnitudeDb), so the display can never lie.
class EqGraphComponent : public juce::Component // NOSONAR(cpp:S5414): public std::function members are assigned externally (PluginEditor.cpp); they must stay public
{
public:
    explicit EqGraphComponent (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override { haveCurveKey = false; }   // force a curve rebuild on size change

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Driven by the editor's timer: pull the pre/post FFT frames and advance the spectra.
    void updateAnimation();

    // EQ match
    void toggleCapture();   // start/stop capturing reference (sidechain) + source (main)
    void runMatch();        // fit the bands to the captured difference

    // --- C4: graph -> floating-panel positioning seam (Branch 2 consumes these) -----------
    // Node bounds for `slot` in graph-LOCAL coords; empty if the slot is inactive/off-screen.
    juce::Rectangle<int> getBandScreenBounds (int slot) const;
    // Fires on selection change AND when the selected node moves, so the panel can follow.
    std::function<void(int slot)> onBandFocused;
    // Kept for the existing editor wiring (rail rebind / strip + graph repaint).
    std::function<void()> onSelectionChanged;

    // --- Transient view toggles (Branch 2's toolbar binds straight to these after merge) ---
    void setPianoVisible (bool v);
    bool isPianoVisible() const noexcept { return pianoVisible; }
    void setSketchActive (bool v);
    bool isSketchActive() const noexcept { return sketchActive; }

    // Pure, headless-testable node placement (no Component state) — mirrors the audio-honest
    // theme mapping. Shared by getBandScreenBounds + node drawing; handy for Branches 2/4.
    static juce::Rectangle<int> nodeBounds (FilterType type, float freq, float gain,
                                            float w, float h, float radius) noexcept;

private:
    static constexpr int   numPoints  = 240;
    static constexpr float nodeRadius = 9.0f;

    struct BandView { FilterType type;
                      float freq;
                      float gain;
                      float q;
                      int slope;
                      bool on;
                      bool active;     // pool-slot existence flag (Pro-Q add/remove)
                      bool live;       // active && on && (!anySolo || solo) — matches the audio gate
                      int channel;
                      bool dynOn;
                      float range;
                      float dynGain; };

    BandView readBand (int i) const;
    bool anySolo() const;
    int  firstActiveBand() const;
    int  nodeAtPosition (juce::Point<float>) const;
    int  rangeHandleAt (juce::Point<float>) const;
    int  slopeHandleAt (juce::Point<float>) const;
    juce::Point<float> nodePosition (const BandView&) const;
    juce::Point<float> slopeHandlePos (const BandView&) const;
    float effectiveGain (const BandView& b) const { return b.gain + (b.dynOn ? b.dynGain : 0.0f); }

    float snapToSpectrumPeak (float x) const;       // nearest analyzer peak frequency
    int   createBand (float freq);                  // proc.addBand(bell) + fire callbacks; -1 if pool full

    void setParam (const juce::String& id, float realValue) const;
    void setChoice (const juce::String& id, int index) const;
    void setBool (const juce::String& id, bool v) const;
    void beginGesture (const juce::String& id) const;
    void endGesture (const juce::String& id) const;
    void focusBand (int slot);                      // setSelectedBand + fire onSelectionChanged/onBandFocused

    // analyzer config (read from APVTS each frame; lock-free)
    int   analyzerMode() const;     // 0 Off / 1 Pre / 2 Post / 3 Pre+Post
    float analyzerFloorDb() const;  // 60 / 90 / 120 dB vertical range
    void  reduceFifo (AnalyzerFifo&, std::array<float, numPoints>& sc,
                      std::array<float, numPoints>& pk, float floorDb);

    void drawGrid (juce::Graphics&) const;
    void drawPianoOverlay (juce::Graphics&) const;
    void drawSpectrum (juce::Graphics&) const;
    void drawSpectrumLayer (juce::Graphics&, const std::array<float, numPoints>& sc,
                            const std::array<float, numPoints>& pk, bool neon) const;
    void drawCurve (juce::Graphics&) const;
    // Curve cache helpers (keep drawCurve's cognitive complexity low).
    bool curveCacheStale (const std::array<BandView, numBands>& bv, float w, float h, double sr) const;
    void rebuildCurve    (const std::array<BandView, numBands>& bv, float w, float h, double sr) const;
    void drawNodes (juce::Graphics&) const;
    void drawChannelBadge (juce::Graphics&, const BandView& b, juce::Point<float> pos,
                           juce::Colour col, float r, bool ms) const;
    void drawDynamicHandles (juce::Graphics&, const BandView& b, juce::Point<float> pos,
                             juce::Colour col) const;
    void drawSlopeHandle (juce::Graphics&, const BandView& b, bool selected) const;
    void drawSketch (juce::Graphics&) const;
    void commitSketch();            // turn the drawn path into a series of bells via addBand
    void showContextMenu();         // right-click empty space: piano / sketch toggles

    // EQ-match capture helpers
    void accumulateTap (AnalyzerFifo&, std::array<double, kMatchBins>&, int& frames) const;
    void finishCapture();

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    // FFT analyzer (shared transform engine, reused per FIFO on the message thread).
    juce::dsp::FFT fft { AnalyzerFifo::order };
    juce::dsp::WindowingFunction<float> window { (size_t) AnalyzerFifo::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, numPoints> scope {};      // post-EQ (output) — neon ramp
    std::array<float, numPoints> peaks {};
    std::array<float, numPoints> preScope {};   // pre-EQ (input) — dim/ghosted gray
    std::array<float, numPoints> prePeaks {};

    // Cached summed-response curve: the magnitude sweep is expensive, so it is rebuilt only
    // when a band's static params (or the size/sample-rate) change; a dynamic band animates,
    // so it rebuilds every frame in that case. Mutable because drawCurve() is const.
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
    mutable double cachedCurveSr = 0.0;

    // EQ-match capture accumulators (message-thread only; raw power per log bin)
    std::array<double, kMatchBins> accumSrc {};
    std::array<double, kMatchBins> accumRef {};
    int capFramesSrc = 0;
    int capFramesRef = 0;

    // drag / hover state
    int  dragBand = -1;
    int  hoverBand = -1;
    bool draggingGain = false;
    bool draggingRange = false;        // dragging a band's dynamic-range handle
    bool draggingSlope = false;        // dragging a cut band's on-curve slope handle
    int  dragStartSlopeIdx = 0;
    bool pendingGrab = false;          // mouse is down on empty graph, may become a grab/create
    juce::Point<float> grabDownPos;

    // transient view toggles (no param — contract C5 declares none for these)
    bool pianoVisible = false;
    bool sketchActive = false;
    bool sketching = false;            // mid-gesture sketch recording
    std::vector<juce::Point<float>> sketchPts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraphComponent)
};

} // namespace zeq
