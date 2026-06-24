#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// The right-hand band editor: type chips, FREQ/GAIN/Q sliders, slope picker,
// enable/solo, output dial and the Stereo/MS + HQ global toggles. The three band
// sliders rebind to whichever band is selected.
class BandEditorRail : public juce::Component // NOSONAR(cpp:S5414): public std::function callbacks onCapture/onMatch are assigned from PluginEditor.cpp; NOSONAR(cpp:S1820): field count reflects the rail's many bound controls, splitting would ripple across the plugin
{
public:
    explicit BandEditorRail (ZandersEqAudioProcessor&);
    ~BandEditorRail() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void bindToSelected();   // rebind the FREQ/GAIN/Q sliders to the selected band
    void refresh();          // poll values, update dynamic styling, repaint

    std::function<void()> onCapture;   // EQ-match: start/stop capture
    std::function<void()> onMatch;     // EQ-match: apply the fit

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Layout // NOSONAR(cpp:S1820): plain layout aggregate; one rectangle per UI region, splitting would obscure the layout
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> eqTab;
        juce::Rectangle<int> dynTab;
        juce::Rectangle<int> typeChips;
        juce::Rectangle<int> freqBlock;
        juce::Rectangle<int> gainBlock;
        juce::Rectangle<int> qBlock;
        juce::Rectangle<int> slopeRow;
        juce::Rectangle<int> onSolo;
        juce::Rectangle<int> channelRow;
        juce::Rectangle<int> bottom;
        juce::Rectangle<int> dial;
        juce::Rectangle<int> modeBtn;
        juce::Rectangle<int> hqBtn;
        juce::Rectangle<int> autoBtn;
        juce::Rectangle<int> matchLabel;
        juce::Rectangle<int> matchAmtRow;
        juce::Rectangle<int> matchBtnRow;
        juce::Rectangle<int> captureBtn;
        juce::Rectangle<int> matchBtn;
        juce::Rectangle<int> dynEnable;
        juce::Rectangle<int> dynS0;
        juce::Rectangle<int> dynS1;
        juce::Rectangle<int> dynS2;
        juce::Rectangle<int> dynS3;
    };
    Layout computeLayout() const;

    int  selected() const { return proc.getSelectedBand(); }
    FilterType selectedType() const;

    juce::Rectangle<float> segment (juce::Rectangle<int> row, int i, int n, float gap) const;

    void setChoice (const juce::String& id, int idx);
    void setBool   (const juce::String& id, bool v);

    // paint() helpers (extracted to keep cognitive complexity low; pure draw of current state)
    void paintSliderSection (juce::Graphics& g, const Layout& L, int s) const;
    void paintMatchPanel    (juce::Graphics& g, const Layout& L) const;
    void paintBottomBar     (juce::Graphics& g, const Layout& L) const;

    // mouseDown() helper: hit-test the EQ/DYN-specific sub-region; returns true if handled
    bool handleSubRegionClick (const Layout& L, juce::Point<float> p, int s);

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Slider outputDial;
    juce::Slider matchAmountSlider;
    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    std::unique_ptr<Attachment> freqAtt;
    std::unique_ptr<Attachment> gainAtt;
    std::unique_ptr<Attachment> qAtt;
    std::unique_ptr<Attachment> outAtt;
    std::unique_ptr<Attachment> matchAmtAtt;
    std::unique_ptr<Attachment> threshAtt;
    std::unique_ptr<Attachment> rangeAtt;
    std::unique_ptr<Attachment> attackAtt;
    std::unique_ptr<Attachment> releaseAtt;

    bool showDyn = false;
    int lastSelected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandEditorRail)
};

} // namespace zeq
