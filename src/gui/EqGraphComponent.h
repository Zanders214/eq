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
class EqGraphComponent : public juce::Component
{
public:
    explicit EqGraphComponent (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override {}

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
    struct BandView { FilterType type; float freq, gain, q; int slope; bool on; bool live; int channel;
                      bool dynOn; float range; float dynGain; };

    BandView readBand (int i) const;
    bool anySolo() const;
    int  nodeAtPosition (juce::Point<float>) const;
    int  rangeHandleAt (juce::Point<float>) const;
    juce::Point<float> nodePosition (const BandView&) const;
    float effectiveGain (const BandView& b) const { return b.gain + (b.dynOn ? b.dynGain : 0.0f); }

    int   spareBand() const;                 // first disabled band, or -1
    float snapToSpectrumPeak (float x) const; // nearest analyzer peak frequency
    int   beginSpectrumGrab (float x);        // create+select a bell at a resonance

    void setParam (const juce::String& id, float realValue);
    void setChoice (const juce::String& id, int index);
    void setBool (const juce::String& id, bool v);
    void beginGesture (const juce::String& id);
    void endGesture (const juce::String& id);

    void drawGrid (juce::Graphics&);
    void drawSpectrum (juce::Graphics&);
    void drawCurve (juce::Graphics&);
    void drawNodes (juce::Graphics&);

    // EQ-match capture helpers
    void accumulateTap (AnalyzerFifo&, std::array<double, kMatchBins>&, int& frames);
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

    // EQ-match capture accumulators (message-thread only; raw power per log bin)
    std::array<double, kMatchBins> accumSrc {}, accumRef {};
    int capFramesSrc = 0, capFramesRef = 0;

    // drag state
    int dragBand = -1;
    bool draggingGain = false;
    bool draggingRange = false;        // dragging a band's dynamic-range handle
    bool pendingGrab = false;          // mouse is down on empty graph, may become a grab
    juce::Point<float> grabDownPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraphComponent)
};

} // namespace zeq
