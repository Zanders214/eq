#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// The six-cell "all bands" strip below the graph: colour dot + type, ON/OFF pill,
// frequency and a gain·Q sub-line. Click selects a band; the pill toggles enable.
class BandStrip : public juce::Component // NOSONAR(cpp:S5414): public std::function callback onSelectionChanged is assigned externally (PluginEditor.cpp); cannot be made private
{
public:
    explicit BandStrip (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onSelectionChanged;

private:
    juce::Rectangle<float> cellBounds (int i) const;
    juce::Rectangle<float> pillBounds (juce::Rectangle<float> cell) const;
    void paintCell (juce::Graphics& g, int i, int sel, bool anySolo) const;

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
};

} // namespace zeq
