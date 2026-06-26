#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// A thin "active bands" overview below the graph: one cell per ACTIVE band (colour dot +
// type, ON/OFF pill, frequency, gain·Q / slope). Click selects a band; the pill toggles
// enable. In the Pro-Q world the band pool is dynamic (CONTRACT C1/C3), so the strip shows
// only active slots; until Branch 1 lands the per-band `band{i}_active` flag, every slot
// reads as active and the strip behaves exactly as before. The constructor signature and
// the addAndMakeVisible contract that EqContent relies on are unchanged.
class BandStrip : public juce::Component // NOSONAR(cpp:S5414): public std::function callback onSelectionChanged is assigned externally (PluginEditor.cpp); cannot be made private
{
public:
    explicit BandStrip (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onSelectionChanged;

private:
    std::vector<int> activeSlots() const;          // pool indices that are active (in order)
    bool bandActive (int slot) const;              // forward-compat: true until band{i}_active exists
    juce::String typeNameFor (int slot) const;     // live from the APVTS choice param

    juce::Rectangle<float> cellBounds (int cellIndex, int count) const;
    juce::Rectangle<float> pillBounds (juce::Rectangle<float> cell) const;
    void paintCell (juce::Graphics& g, int slot, juce::Rectangle<float> cell,
                    bool selected, bool anySolo) const;

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
};

} // namespace zeq
