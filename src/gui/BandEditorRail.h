#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace zeq
{

// Pro-Q-4-style floating band-control panel. Repurposed in place from the old fixed
// right-rail (the class name + filenames are frozen by CONTRACT C4): it now renders a
// compact card with type/shape chips, FREQ/GAIN/Q, a slope picker (cuts), stereo
// placement, a dynamics toggle that expands an "expert" sub-pane, and a small EQ-match
// section. The band controls rebind to whichever band is selected.
//
// The choice lists (filter types, slopes) are read live from the APVTS params, so the
// new shapes (Tilt Shelf / Band-Pass / All-Pass) and steep slopes (72/96 dB/oct,
// Brickwall) appear automatically once Branch 1/5 land — no edit here required (C2).
//
// Branch 2 (layout) owns *where* this panel is placed: it queries the graph for the
// selected node's position and calls getDesiredSize(). This class owns *what* it renders.
class BandEditorRail : public juce::Component // NOSONAR(cpp:S5414): public std::function callbacks onCapture/onMatch are assigned from PluginEditor.cpp; NOSONAR(cpp:S1820): field count reflects the panel's many bound controls, splitting would ripple across the plugin
{
public:
    explicit BandEditorRail (ZandersEqAudioProcessor&);
    ~BandEditorRail() override;

    // Natural size of the panel for the current selection (taller when the dynamics
    // expert pane is open). Cheap + const so the layout branch can call it freely.
    //
    // NOTE (CONTRACT C4 correction): the contract specifies juce::Size<int>, but JUCE 8
    // has no Size class (geometry types are Point/Rectangle/Line/BorderSize). We return
    // juce::Rectangle<int> (origin 0,0; read .getWidth()/.getHeight()) — the idiomatic
    // JUCE size carrier — and flag this so Branch 2 consumes the same type.
    juce::Rectangle<int> getDesiredSize() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void bindToSelected();   // rebind the FREQ/GAIN/Q + dynamics sliders to the selected band
    void refresh();          // poll values, update dynamic styling, repaint

    // True while the panel shows a continuously-updating readout (dynamic-EQ gain
    // reduction for the selected band), so the editor keeps it repainting even when no
    // parameter changed.
    bool hasLiveReadout() const;

    std::function<void()> onCapture;   // EQ-match: start/stop capture
    std::function<void()> onMatch;     // EQ-match: apply the fit

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Layout // NOSONAR(cpp:S1820): plain layout aggregate; one rectangle per UI region, splitting would obscure the layout
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> typeChips;
        juce::Rectangle<int> freqBlock;
        juce::Rectangle<int> gainBlock;
        juce::Rectangle<int> qBlock;
        juce::Rectangle<int> slopeRow;
        juce::Rectangle<int> stereoRow;
        juce::Rectangle<int> dynHead;
        juce::Rectangle<int> dynDir;
        juce::Rectangle<int> dynS0;
        juce::Rectangle<int> dynS1;
        juce::Rectangle<int> dynS2;
        juce::Rectangle<int> dynS3;
        juce::Rectangle<int> matchLabel;
        juce::Rectangle<int> matchAmtRow;
        juce::Rectangle<int> captureBtn;
        juce::Rectangle<int> matchBtn;
        int  totalHeight = 0;     // natural card height for the current state
        bool gainShown   = false; // hidden for shapes that sit on the zero line
        bool slopeShown  = false; // cuts only
        bool dynAvailable = false; // bell/shelf only
        bool expertShown = false; // dynamics expert sub-pane expanded
    };
    Layout computeLayout() const;

    int  selected() const { return proc.getSelectedBand(); }
    FilterType selectedType() const;
    bool selectedZeroLine() const;   // gain handle hidden when true (HP/LP/Notch/BP/AP)

    // Choice params read live so the chips stay in sync with Branch 1/5 (C2).
    juce::AudioParameterChoice* choiceParam (const juce::String& id) const;
    int          choiceIndex (const juce::String& id) const;
    juce::String choiceName  (const juce::String& id, int idx) const;
    static juce::String typeChipLabel  (const juce::String& fullName);
    static juce::String slopeChipLabel (const juce::String& fullName);

    juce::Rectangle<float> segment (juce::Rectangle<int> row, int i, int n, float gap) const;

    void setChoice (const juce::String& id, int idx);
    void setBool   (const juce::String& id, bool v);

    // paint() helpers (extracted to keep cognitive complexity low; pure draw of current state)
    void paintBandControls (juce::Graphics& g, const Layout& L, int s) const;
    void paintDynSection   (juce::Graphics& g, const Layout& L, int s) const;
    void paintMatchSection (juce::Graphics& g, const Layout& L) const;

    // mouseDown() helper: hit-test the dynamics sub-region; returns true if handled
    bool handleDynClick (const Layout& L, juce::Point<float> p, int s);

    ZandersEqAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Slider freqSlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Slider matchAmountSlider;
    juce::Slider threshSlider;
    juce::Slider rangeSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    std::unique_ptr<Attachment> freqAtt;
    std::unique_ptr<Attachment> gainAtt;
    std::unique_ptr<Attachment> qAtt;
    std::unique_ptr<Attachment> matchAmtAtt;
    std::unique_ptr<Attachment> threshAtt;
    std::unique_ptr<Attachment> rangeAtt;
    std::unique_ptr<Attachment> attackAtt;
    std::unique_ptr<Attachment> releaseAtt;

    bool dynExpertOpen = false;
    int  lastSelected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandEditorRail)
};

} // namespace zeq
