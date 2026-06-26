#include "BandStrip.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"

namespace zeq
{
using namespace theme;

BandStrip::BandStrip (ZandersEqAudioProcessor& p) : proc (p), apvts (p.getApvts()) {}

bool BandStrip::bandActive (int slot) const
{
    // Forward-compat (CONTRACT C3): Branch 1 adds a per-band `band{i}_active` bool. Until
    // then the param is absent and every populated slot reads as active, preserving the
    // current fixed-6 behaviour. (Constructs the id Branch 1 freezes as ids::active(i).)
    if (auto* pr = apvts.getParameter ("band" + juce::String (slot) + "_active"))
        return pr->getValue() > 0.5f;
    return true;
}

std::vector<int> BandStrip::activeSlots() const
{
    std::vector<int> slots;
    for (int i = 0; i < numBands; ++i)
        if (bandActive (i))
            slots.push_back (i);
    if (slots.empty())            // never render an empty strip; fall back to the selection
        slots.push_back (proc.getSelectedBand());
    return slots;
}

juce::String BandStrip::typeNameFor (int slot) const
{
    if (auto* pr = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ids::type (slot))))
    {
        const int idx = (int) apvts.getRawParameterValue (ids::type (slot))->load();
        if (idx >= 0 && idx < pr->choices.size())
            return pr->choices[idx].toUpperCase();
    }
    return typeLabel (static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (slot))->load()));
}

juce::Rectangle<float> BandStrip::cellBounds (int cellIndex, int count) const
{
    const float gap = 6.0f;
    const int   n   = juce::jmax (1, count);
    const float w   = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
    return { (w + gap) * (float) cellIndex, 0.0f, w, (float) getHeight() };
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

    const auto slots = activeSlots();
    const int  n     = (int) slots.size();
    for (int k = 0; k < n; ++k)
        paintCell (g, slots[(size_t) k], cellBounds (k, n), slots[(size_t) k] == sel, anySolo);
}

void BandStrip::paintCell (juce::Graphics& g, int slot, juce::Rectangle<float> cell,
                           bool selected, bool anySolo) const
{
    const float freq = apvts.getRawParameterValue (ids::freq (slot))->load();
    const float gain = apvts.getRawParameterValue (ids::gain (slot))->load();
    const float q    = apvts.getRawParameterValue (ids::q (slot))->load();
    const auto  type = static_cast<FilterType> ((int) apvts.getRawParameterValue (ids::type (slot))->load());
    const int   slope = slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (slot))->load());
    const bool  on   = apvts.getRawParameterValue (ids::on (slot))->load() > 0.5f;
    const bool  solo = apvts.getRawParameterValue (ids::solo (slot))->load() > 0.5f;
    const bool  live = on && (! anySolo || solo);
    const auto  col  = colourForFreq (freq);

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
    g.drawText (typeNameFor (slot), labelArea, juce::Justification::centredLeft, true);

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

void BandStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto slots = activeSlots();
    const int  n     = (int) slots.size();
    for (int k = 0; k < n; ++k)
    {
        auto cell = cellBounds (k, n);
        if (! cell.contains (e.position))
            continue;

        const int slot = slots[(size_t) k];
        if (pillBounds (cell).contains (e.position))
        {
            const bool on = apvts.getRawParameterValue (ids::on (slot))->load() > 0.5f;
            proc.recordUndoableEdit ([this, slot, on]
            {
                if (auto* p = apvts.getParameter (ids::on (slot)))
                    p->setValueNotifyingHost (on ? 0.0f : 1.0f);
            });
        }
        else if (slot != proc.getSelectedBand())
        {
            proc.setSelectedBand (slot);
            if (onSelectionChanged) onSelectionChanged();
        }
        repaint();
        return;
    }
}

} // namespace zeq
