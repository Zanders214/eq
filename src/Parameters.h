#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EqMath.h"

namespace zeq
{

// Fixed pool of bands. Six matches the design's default set, the six-cell band
// strip and the six spectrum colours. Bump this constant to expand the EQ — the
// strip, nodes and parameter layout are all driven from it.
inline constexpr int numBands = 6;

namespace ids
{
    // Global
    inline constexpr const char* output = "output";   // -24..+24 dB
    inline constexpr const char* mode   = "mode";     // 0 = Stereo, 1 = Mid/Side
    inline constexpr const char* hq     = "hq";       // HQ oversampling
    inline constexpr const char* autogain = "autogain"; // loudness-matched output trim

    // Per-band, suffixed with the band index, e.g. "band0_freq".
    inline juce::String band  (int i)               { return "band" + juce::String (i) + "_"; }
    inline juce::String type  (int i)               { return band (i) + "type"; }
    inline juce::String freq  (int i)               { return band (i) + "freq"; }
    inline juce::String gain  (int i)               { return band (i) + "gain"; }
    inline juce::String q     (int i)               { return band (i) + "q"; }
    inline juce::String slope (int i)               { return band (i) + "slope"; }
    inline juce::String on    (int i)               { return band (i) + "on"; }
    inline juce::String solo  (int i)               { return band (i) + "solo"; }
}

// Parameter ranges (shared by the engine and the UI mappings).
inline constexpr float freqMin = 20.0f,   freqMax = 20000.0f;
inline constexpr float gainMin = -18.0f,  gainMax = 18.0f;
inline constexpr float qMin    = 0.1f,    qMax    = 18.0f;
inline constexpr float outMin  = -24.0f,  outMax  = 24.0f;

inline juce::StringArray filterTypeChoices()
{
    return { "High Pass", "Low Shelf", "Bell", "Notch", "High Shelf", "Low Pass" };
}

inline juce::StringArray slopeChoices()
{
    return { "12 dB/oct", "24 dB/oct", "48 dB/oct" };
}

inline int slopeIndexToValue (int idx)
{
    switch (idx) { case 0: return 12; case 1: return 24; default: return 48; }
}

inline int slopeValueToIndex (int v)
{
    if (v <= 12) return 0;
    if (v <= 24) return 1;
    return 2;
}

// A log-frequency range that matches the design's f01 mapping (20 Hz .. 20 kHz).
inline juce::NormalisableRange<float> makeFreqRange()
{
    juce::NormalisableRange<float> r (freqMin, freqMax,
        [] (float start, float end, float t) { return start * std::pow (end / start, t); },
        [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
        [] (float start, float end, float v) { return juce::jlimit (start, end, v); });
    return r;
}

inline juce::NormalisableRange<float> makeQRange()
{
    juce::NormalisableRange<float> r (qMin, qMax,
        [] (float start, float end, float t) { return start * std::pow (end / start, t); },
        [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
        [] (float start, float end, float v) { return juce::jlimit (start, end, v); });
    return r;
}

// The default six-band shape from the design handoff (README "Default bands").
struct BandDefault { FilterType type; float freq, gain, q; int slope; };

inline std::array<BandDefault, numBands> defaultBands()
{
    return { {
        { FilterType::highPass,  30.0f,   0.0f, 0.71f, 24 },
        { FilterType::bell,      95.0f,   3.2f, 0.90f, 12 },
        { FilterType::bell,     420.0f,  -3.8f, 1.50f, 12 },
        { FilterType::bell,    2600.0f,   2.6f, 1.10f, 12 },
        { FilterType::highShelf, 8200.0f, 3.6f, 0.70f, 12 },
        { FilterType::lowPass, 19000.0f,  0.0f, 0.71f, 12 },
    } };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto defs = defaultBands();

    for (int i = 0; i < numBands; ++i)
    {
        const auto& d = defs[(size_t) i];
        const String g = "Band " + String (i + 1) + " ";

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::type (i), 1 }, g + "Type",
            filterTypeChoices(), static_cast<int> (d.type)));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::freq (i), 1 }, g + "Freq",
            makeFreqRange(), d.freq,
            AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::gain (i), 1 }, g + "Gain",
            NormalisableRange<float> (gainMin, gainMax, 0.01f), d.gain,
            AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::q (i), 1 }, g + "Q",
            makeQRange(), d.q));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::slope (i), 1 }, g + "Slope",
            slopeChoices(), slopeValueToIndex (d.slope)));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::on (i), 1 }, g + "Enabled", true));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::solo (i), 1 }, g + "Solo", false));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::output, 1 }, "Output",
        NormalisableRange<float> (outMin, outMax, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::mode, 1 }, "Mode",
        StringArray { "Stereo", "Mid/Side" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::hq, 1 }, "HQ", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::autogain, 1 }, "Auto Gain", false));

    return layout;
}

} // namespace zeq
