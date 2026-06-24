#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// Row of six built-in preset chips plus SAVE / LOAD buttons for user presets.
// Click a chip to apply (it lights when the current state matches); SAVE writes the
// current state to a named .zeqpreset file; LOAD opens a menu of saved presets.
class PresetBar : public juce::Component // NOSONAR(cpp:S5414): public onPresetApplied is set externally (PluginEditor.cpp); cannot be made private
{
public:
    explicit PresetBar (ZandersEqAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onPresetApplied;

private:
    juce::Rectangle<float> chipBounds (int i) const;
    juce::Rectangle<float> saveBtnBounds() const;
    juce::Rectangle<float> menuBtnBounds() const;
    void showSaveDialog();
    void showPresetMenu();

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace zeq
