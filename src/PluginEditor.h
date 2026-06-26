#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "gui/NeonLookAndFeel.h"
#include "gui/EqGraphComponent.h"
#include "gui/BandStrip.h"
#include "gui/PresetBar.h"
#include "gui/BandEditorRail.h"

namespace zeq
{

// The full UI, laid out at the fixed design size. The editor host scales this to
// whatever window size the user picks (uniform zoom), so every absolute-pixel
// layout and hit-test below stays exactly as designed.
//
// Pro-Q-4 reflow: a full-width spectrum graph dominates; the band-control panel
// (BandEditorRail) floats over the graph rather than living in a fixed side rail;
// and a slim bottom toolbar carries the global controls. The brand stays neon.
class EqContent : public juce::Component,
                  private juce::Timer,
                  private juce::AudioProcessorParameter::Listener
{
public:
    explicit EqContent (ZandersEqAudioProcessor&);
    ~EqContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void drawHeader (juce::Graphics&);
    void paintToolbar (juce::Graphics&);
    void layoutToolbar (juce::Rectangle<int>);
    void positionBandPanel();                 // floats the band card; see C4 seam in the .cpp

    // Toolbar bindings (consume the frozen param surface only — never edit Parameters.h).
    void         toggleBoolParam  (const juce::String& id);
    void         cycleChoiceParam (const juce::String& id);
    bool         boolParam        (const juce::String& id) const;
    juce::String choiceLabel      (const juce::String& id) const;
    float        floatParam       (const juce::String& id) const;
    void         startParamDrag   (const juce::String& id, const juce::MouseEvent&, float perPx);

    // Any parameter change (UI edit, host automation, preset/undo) flags the chrome
    // (strip/rail/preset bar/header/toolbar) for a repaint on the next timer tick — so
    // it is no longer repainted blindly every frame.
    void parameterValueChanged (int, float) override { uiDirty.store (true); }
    void parameterGestureChanged (int, bool) override { /* gesture begin/end doesn't change what the panels display */ }
    std::atomic<bool> uiDirty { true };

    ZandersEqAudioProcessor& proc;
    NeonLookAndFeel lnf;

    EqGraphComponent graph;
    BandStrip        strip;
    PresetBar        presetBar;
    BandEditorRail   rail;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> wellBounds;
    juce::Rectangle<int> toolbarBounds;
    juce::Rectangle<int> outBox;              // OUT readout (drag to set output) — set in resized()
    juce::Rectangle<int> abA;
    juce::Rectangle<int> abB;
    juce::Rectangle<int> undoBtn;
    juce::Rectangle<int> redoBtn;

    // Bottom-toolbar hit-rects (set in layoutToolbar()).
    juce::Rectangle<int> phaseSeg;            // phaseMode segmented control (Zero Latency live)
    juce::Rectangle<int> anOnBtn;             // analyzerOn   (cycle)
    juce::Rectangle<int> anRangeBtn;          // analyzerRange(cycle)
    juce::Rectangle<int> gainScaleBtn;        // gainScale    (drag)
    juce::Rectangle<int> bypassBtn;           // globalBypass (toggle)
    juce::Rectangle<int> hqBtn;               // hq           (toggle)
    juce::Rectangle<int> autoBtn;             // autogain     (toggle)
    juce::Rectangle<int> fsBtn;               // full-screen  (pending Branch 3)
    juce::Rectangle<int> sketchBtn;           // EQ-Sketch    (pending Branch 3)
    juce::Rectangle<int> pianoBtn;            // piano overlay(pending Branch 3)

    // Drag state for the continuous readouts (output / gain-scale): one gesture = one
    // undo step (begin on mouseDown, commit on mouseUp).
    juce::String dragParamId;
    float dragStartValue = 0.0f;
    float dragPerPx      = 0.0f;
    int   dragStartY     = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqContent)
};

// Thin resizable host: scales EqContent to fit the (aspect-locked) window.
class ZandersEqEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZandersEqEditor (ZandersEqAudioProcessor&);
    ~ZandersEqEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Full-screen toggle (no graph API for this — it's window-level): maximise the editor to
    // its max allowed size and back. Driven by the toolbar's FS button.
    void toggleFullscreen();
    bool isMaximized() const noexcept;

    // Pro-Q-style landscape canvas (was 1100x772). Uniform aspect-locked scaling and the
    // persisted-width logic below are unchanged — only the design proportions widened.
    static constexpr int designW = 1280;
    static constexpr int designH = 800;

private:
    ZandersEqAudioProcessor& proc;
    EqContent content;
    juce::ComponentBoundsConstrainer constrainer;
    int preFsWidth = 0;   // width remembered before maximising, restored on toggle-off

    // GPU-accelerated rendering: offloads all the vector rasterisation (spectrum,
    // curve, glows) from the CPU. Detached explicitly in the destructor.
    juce::OpenGLContext openGLContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqEditor)
};

} // namespace zeq
