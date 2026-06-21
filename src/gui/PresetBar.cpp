#include "PresetBar.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"
#include "../Presets.h"

namespace zeq
{
using namespace theme;

PresetBar::PresetBar (ZandersEqAudioProcessor& p) : proc (p), apvts (p.getApvts()) {}

juce::Rectangle<float> PresetBar::chipBounds (int i) const
{
    const int n = (int) presets().size();
    const float gap = 6.0f;
    const float w = ((float) getWidth() - gap * (n - 1)) / (float) n;
    return { (w + gap) * (float) i, 0.0f, w, (float) getHeight() };
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto& list = presets();
    const juce::Colour violetText (0xffb6abff);

    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto chip = chipBounds (i);
        const bool active = matchesPreset (apvts, list[(size_t) i]);

        g.setColour (active ? accentVio.withAlpha (0.16f) : whiteAlpha (0.05f));
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (active ? accentVio.withAlpha (0.45f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (chip.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (active ? violetText : text2);
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.03f));
        g.drawText (list[(size_t) i].name, chip, juce::Justification::centred);
    }
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    const auto& list = presets();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (chipBounds (i).contains (e.position))
        {
            proc.recordUndoableEdit ([&] { applyPreset (apvts, list[(size_t) i]); });
            if (onPresetApplied) onPresetApplied();
            repaint();
            return;
        }
    }
}

} // namespace zeq
