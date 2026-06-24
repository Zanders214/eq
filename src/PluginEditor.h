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
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void drawHeader (juce::Graphics&);

    // Any parameter change (UI edit, host automation, preset/undo) flags the
    // static panels (strip/rail/preset bar/header) for a repaint on the next
    // timer tick — so they are no longer repainted blindly every frame.
    void parameterValueChanged (int, float) override { uiDirty.store (true, std::memory_order_relaxed); }
    void parameterGestureChanged (int, bool) override {}
    std::atomic<bool> uiDirty { true };

    ZandersEqAudioProcessor& proc;
    NeonLookAndFeel lnf;

    EqGraphComponent graph;
    BandStrip        strip;
    PresetBar        presetBar;
    BandEditorRail   rail;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> wellBounds;
    juce::Rectangle<int> abA;
    juce::Rectangle<int> abB;
    juce::Rectangle<int> undoBtn;
    juce::Rectangle<int> redoBtn;

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

    static constexpr int designW = 1100;
    static constexpr int designH = 772;

private:
    ZandersEqAudioProcessor& proc;
    EqContent content;
    juce::ComponentBoundsConstrainer constrainer;

    // GPU-accelerated rendering: offloads all the vector rasterisation (spectrum,
    // curve, glows) from the CPU. Detached explicitly in the destructor.
    juce::OpenGLContext openGLContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqEditor)
};

} // namespace zeq
