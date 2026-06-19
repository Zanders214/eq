#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/NeonLookAndFeel.h"
#include "gui/EqGraphComponent.h"
#include "gui/BandStrip.h"
#include "gui/PresetBar.h"
#include "gui/BandEditorRail.h"

namespace zeq
{

class ZandersEqEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit ZandersEqEditor (ZandersEqAudioProcessor&);
    ~ZandersEqEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawHeader (juce::Graphics&);

    ZandersEqAudioProcessor& proc;
    NeonLookAndFeel lnf;

    EqGraphComponent graph;
    BandStrip        strip;
    PresetBar        presetBar;
    BandEditorRail   rail;

    juce::Rectangle<int> headerBounds, wellBounds, abA, abB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqEditor)
};

} // namespace zeq
