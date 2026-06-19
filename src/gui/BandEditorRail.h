#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// The right-hand band editor: type chips, FREQ/GAIN/Q sliders, slope picker,
// enable/solo, output dial and the Stereo/MS + HQ global toggles. The three band
// sliders rebind to whichever band is selected.
class BandEditorRail : public juce::Component
{
public:
    explicit BandEditorRail (ZandersEqAudioProcessor&);
    ~BandEditorRail() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void bindToSelected();   // rebind the FREQ/GAIN/Q sliders to the selected band
    void refresh();          // poll values, update dynamic styling, repaint

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Layout
    {
        juce::Rectangle<int> header, typeChips, freqBlock, gainBlock, qBlock,
                             slopeRow, onSolo, bottom, dial, modeBtn, hqBtn, autoBtn;
    };
    Layout computeLayout() const;

    int  selected() const { return proc.getSelectedBand(); }
    FilterType selectedType() const;

    juce::Rectangle<float> segment (juce::Rectangle<int> row, int i, int n, float gap) const;

    void setChoice (const juce::String& id, int idx);
    void setBool   (const juce::String& id, bool v);

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Slider freqSlider, gainSlider, qSlider, outputDial;
    std::unique_ptr<Attachment> freqAtt, gainAtt, qAtt, outAtt;

    int lastSelected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandEditorRail)
};

} // namespace zeq
