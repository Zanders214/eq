#pragma once

#include "Parameters.h"

namespace zeq
{

// The six presets from the handoff README. Each lists up to `numBands` bands;
// any remaining bands are switched off when the preset is applied.
struct PresetBand { FilterType type; float freq; float gain; float q; int slope; };
struct Preset { const char* name; std::vector<PresetBand> bands; };

inline const std::vector<Preset>& presets()
{
    using FT = FilterType;
    static const std::vector<Preset> p = {
        { "Flat", {
            { FT::highPass,    24.0f,  0.0f, 0.71f, 12 },
            { FT::bell,       200.0f,  0.0f, 0.90f, 12 },
            { FT::bell,      1000.0f,  0.0f, 0.90f, 12 },
            { FT::bell,      5000.0f,  0.0f, 0.90f, 12 } } },
        { "Vocal Air", {
            { FT::highPass,    80.0f,  0.0f, 0.71f, 24 },
            { FT::bell,       320.0f, -2.5f, 1.40f, 12 },
            { FT::bell,      3000.0f,  2.0f, 0.90f, 12 },
            { FT::highShelf,11000.0f,  4.5f, 0.70f, 12 } } },
        { "De-Mud", {
            { FT::highPass,    45.0f,  0.0f, 0.71f, 24 },
            { FT::bell,       280.0f, -5.0f, 1.80f, 12 },
            { FT::bell,       500.0f, -2.5f, 1.20f, 12 },
            { FT::highShelf, 9000.0f,  1.8f, 0.70f, 12 } } },
        { "Bass Tight", {
            { FT::highPass,    32.0f,  0.0f, 0.71f, 48 },
            { FT::bell,        70.0f,  3.0f, 1.00f, 12 },
            { FT::bell,       220.0f, -3.5f, 1.60f, 12 },
            { FT::lowShelf,   110.0f,  1.5f, 0.70f, 12 } } },
        { "Lo-Fi", {
            { FT::highPass,   180.0f,  0.0f, 0.71f, 24 },
            { FT::bell,       900.0f,  4.0f, 0.80f, 12 },
            { FT::lowPass,   4200.0f,  0.0f, 0.71f, 24 } } },
        { "Bright", {
            { FT::bell,       400.0f, -1.5f, 1.00f, 12 },
            { FT::bell,      4000.0f,  2.5f, 0.90f, 12 },
            { FT::highShelf,10000.0f,  5.0f, 0.70f, 12 } } },
    };
    return p;
}

inline void applyPreset (const juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    auto set = [&] (const juce::String& id, float v01)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (v01);
    };
    auto setReal = [&] (const juce::String& id, float real)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    };

    for (int i = 0; i < numBands; ++i)
    {
        if (i < (int) preset.bands.size())
        {
            const auto& bnd = preset.bands[(size_t) i];
            set     (ids::type (i),  apvts.getParameter (ids::type (i))->convertTo0to1 ((float) (int) bnd.type));
            setReal (ids::freq (i),  bnd.freq);
            setReal (ids::gain (i),  bnd.gain);
            setReal (ids::q (i),     bnd.q);
            set     (ids::slope (i), apvts.getParameter (ids::slope (i))->convertTo0to1 ((float) slopeValueToIndex (bnd.slope)));
            set     (ids::active (i), 1.0f);
            set     (ids::on (i),    1.0f);
            set     (ids::solo (i),  0.0f);
        }
        else
        {
            set (ids::active (i), 0.0f);
            set (ids::on (i), 0.0f);
            set (ids::solo (i), 0.0f);
        }
    }
}

// True if the current parameter state matches this preset exactly (used to light
// the active preset chip; any edit moves the state away and clears the highlight).
inline bool matchesPreset (const juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    auto approx = [] (float a, float b, float eps) { return std::abs (a - b) <= eps; };

    for (int i = 0; i < numBands; ++i)
    {
        // `active` is the existence flag under the dynamic-pool model: a preset matches when
        // exactly its bands are present (active) and every spare slot is inactive.
        const bool active = apvts.getRawParameterValue (ids::active (i))->load() > 0.5f;
        if (i < (int) preset.bands.size())
        {
            const auto& bnd = preset.bands[(size_t) i];
            if (! active) return false;
            if ((int) apvts.getRawParameterValue (ids::type (i))->load() != (int) bnd.type) return false;
            if (! approx (apvts.getRawParameterValue (ids::freq (i))->load(), bnd.freq, juce::jmax (0.5f, bnd.freq * 0.01f))) return false;
            if (! approx (apvts.getRawParameterValue (ids::gain (i))->load(), bnd.gain, 0.05f)) return false;
            if (! approx (apvts.getRawParameterValue (ids::q (i))->load(), bnd.q, 0.02f)) return false;
            if (slopeIndexToValue ((int) apvts.getRawParameterValue (ids::slope (i))->load()) != bnd.slope) return false;
        }
        else if (active)
        {
            return false;
        }
    }
    return true;
}

} // namespace zeq
