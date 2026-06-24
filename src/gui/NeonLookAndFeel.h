#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace zeq
{

// Shared access to the bundled brand faces (Space Grotesk + JetBrains Mono).
struct Fonts
{
    enum Weight { medium = 0, semibold, bold }; // NOSONAR(cpp:S3642): enumerators used as Fonts::semibold/bold across many files outside this component; enum class would require Fonts::Weight:: qualification everywhere.

    static juce::Font grotesk (float height, Weight w = semibold);
    static juce::Font mono    (float height, bool mediumWeight = false);

    // All-caps tracked UI label (Space Grotesk, wide letter-spacing).
    static juce::Font label (float height, float trackingEm, Weight w = semibold)
    {
        return grotesk (height, w).withExtraKerningFactor (trackingEm);
    }
};

// The Neon Plugins look: dark glass, gradient line-sliders, an accent dial.
class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional, float startAngle, float endAngle,
                           juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    // soft outer colour glow used on dots / handles
    static void glow (juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour c,
                      float radius, float alpha);
};

} // namespace zeq
