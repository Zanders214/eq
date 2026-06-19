#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// Row of six preset chips. Click applies; the chip lights when the current state
// matches that preset exactly.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onPresetApplied;

private:
    juce::Rectangle<float> chipBounds (int i) const;

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace zeq
