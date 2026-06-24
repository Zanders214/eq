#include "BandStrip.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"

namespace zeq
{
using namespace theme;

BandStrip::BandStrip (ZandersEqAudioProcessor& p) : proc (p), apvts (p.getApvts()) {}

juce::Rectangle<float> BandStrip::cellBounds (int i) const
{
    const float gap = 6.0f;
    const float w = ((float) getWidth() - gap * (numBands - 1)) / (float) numBands;
    return { (w + gap) * (float) i, 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> BandStrip::pillBounds (juce::Rectangle<float> cell) const
{
    return { cell.getRight() - 10.0f - 24.0f, cell.getY() + 9.0f, 24.0f, 17.0f };
}

void BandStrip::paint (juce::Graphics& g)
{
    const int sel = proc.getSelectedBand();
    bool anySolo = false;
    for (int i = 0; i < numBands; ++i)
        anySolo = anySolo || apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f;

    for (int i = 0; i < numBands; ++i)
        paintCell (g, i, sel, anySolo);
}

void BandStrip::paintCell (juce::Graphics& g, int i, int sel, bool anySolo) const
{
    {
        auto cell = cellBounds (i);
        const auto type = static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (i))->load());
        const float freq = apvts.getRawParameterValue (ids::freq (i))->load();
        const float gain = apvts.getRawParameterValue (ids::gain (i))->load();
        const float q    = apvts.getRawParameterValue (ids::q (i))->load();
        const int slope  = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (i))->load());
        const bool on    = apvts.getRawParameterValue (ids::on (i))->load() > 0.5f;
        const bool solo  = apvts.getRawParameterValue (ids::solo (i))->load() > 0.5f;
        const bool live  = on && (! anySolo || solo);
        const bool selected = (i == sel);
        const auto col = colourForFreq (freq);

        // cell background
        g.setColour (selected ? accent.withAlpha (0.10f) : whiteAlpha (0.05f));
        g.fillRoundedRectangle (cell, 9.0f);
        g.setColour (selected ? accent.withAlpha (0.45f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (cell.reduced (0.5f), 9.0f, 1.0f);

        auto inner = cell.reduced (10.0f, 9.0f);

        // dot + type label
        auto top = inner.removeFromTop (17.0f);
        auto dot = top.removeFromLeft (9.0f).withSizeKeepingCentre (9.0f, 9.0f);
        NeonLookAndFeel::glow (g, dot, col, 4.0f, live ? 0.7f : 0.2f);
        g.setColour (col.withAlpha (live ? 1.0f : 0.3f));
        g.fillEllipse (dot);

        // ON/OFF pill
        auto pill = pillBounds (cell);
        g.setColour (on ? accent.withAlpha (0.18f) : whiteAlpha (0.06f));
        g.fillRoundedRectangle (pill, 5.0f);
        g.setColour (on ? accent.withAlpha (0.30f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (on ? accent : textLabel);
        g.setFont (Fonts::grotesk (7.5f, Fonts::bold).withExtraKerningFactor (0.04f));
        g.drawText (on ? "ON" : "OFF", pill, juce::Justification::centred);

        g.setColour (textLabel);
        g.setFont (Fonts::grotesk (9.0f, Fonts::semibold).withExtraKerningFactor (0.1f));
        auto labelArea = top.withTrimmedLeft (6.0f).withRight (pill.getX() - 4.0f);
        g.drawText (typeLabel (type), labelArea, juce::Justification::centredLeft, true);

        inner.removeFromTop (6.0f);
        g.setColour (on ? text1 : text1.withAlpha (0.5f));
        g.setFont (Fonts::mono (13.0f, true));
        g.drawText (fmtFreq (freq) + "  " + noteName (freq), inner.removeFromTop (15.0f), juce::Justification::centredLeft);

        inner.removeFromTop (2.0f);
        g.setColour (text3);
        g.setFont (Fonts::mono (10.0f));
        const juce::String sub = isCut (type)
            ? juce::String (slope) + " dB/oct"
            : fmtGain (gain) + "  Q" + juce::String (q, 1);
        g.drawText (sub, inner.removeFromTop (12.0f), juce::Justification::centredLeft, true);

        if (! on)
        {
            g.setColour (well.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 9.0f);
        }
    }
}

void BandStrip::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numBands; ++i)
    {
        auto cell = cellBounds (i);
        if (! cell.contains (e.position))
            continue;

        if (pillBounds (cell).contains (e.position))
        {
            const bool on = apvts.getRawParameterValue (ids::on (i))->load() > 0.5f;
            proc.recordUndoableEdit ([this, i, on]
            {
                if (auto* p = apvts.getParameter (ids::on (i)))
                    p->setValueNotifyingHost (on ? 0.0f : 1.0f);
            });
        }
        else if (i != proc.getSelectedBand())
        {
            proc.setSelectedBand (i);
            if (onSelectionChanged) onSelectionChanged();
        }
        repaint();
        return;
    }
}

} // namespace zeq
