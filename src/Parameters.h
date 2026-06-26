#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EqMath.h"

namespace zeq
{

// numBands is defined in dsp/EqMath.h (shared with non-JUCE code like MatchFit).

namespace ids
{
    // Global
    inline constexpr const char* output = "output";   // -24..+24 dB
    inline constexpr const char* mode   = "mode";     // 0 = Stereo, 1 = Mid/Side
    inline constexpr const char* hq     = "hq";       // HQ oversampling
    inline constexpr const char* autogain = "autogain"; // loudness-matched output trim
    inline constexpr const char* matchamount = "matchamount"; // EQ-match strength 0..100%
    inline constexpr const char* gainScale     = "gainscale";     // 0..200% overall EQ gain
    inline constexpr const char* analyzerOn    = "analyzeron";    // analyzer display mode
    inline constexpr const char* analyzerRange = "analyzerrange"; // analyzer vertical dB range
    inline constexpr const char* globalBypass  = "globalbypass";  // bypass the whole EQ
    inline constexpr const char* phaseMode     = "phasemode";     // Zero Latency / Natural / Linear

    // Per-band, suffixed with the band index, e.g. "band0_freq".
    inline juce::String band  (int i)               { return "band" + juce::String (i) + "_"; }
    inline juce::String type  (int i)               { return band (i) + "type"; }
    inline juce::String freq  (int i)               { return band (i) + "freq"; }
    inline juce::String gain  (int i)               { return band (i) + "gain"; }
    inline juce::String q     (int i)               { return band (i) + "q"; }
    inline juce::String slope (int i)               { return band (i) + "slope"; }
    inline juce::String on    (int i)               { return band (i) + "on"; }
    inline juce::String active (int i)              { return band (i) + "active"; }
    inline juce::String solo  (int i)               { return band (i) + "solo"; }
    inline juce::String channel (int i)             { return band (i) + "channel"; }
    inline juce::String dynOn     (int i)           { return band (i) + "dynon"; }
    inline juce::String dynThresh (int i)           { return band (i) + "dynthresh"; }
    inline juce::String dynRange  (int i)           { return band (i) + "dynrange"; }
    inline juce::String dynAttack (int i)           { return band (i) + "dynattack"; }
    inline juce::String dynRelease(int i)           { return band (i) + "dynrelease"; }
    inline juce::String dynDir    (int i)           { return band (i) + "dyndir"; }
}

// Parameter ranges (shared by the engine and the UI mappings).
inline constexpr float freqMin = 20.0f;
inline constexpr float freqMax = 20000.0f;
inline constexpr float gainMin = -18.0f;
inline constexpr float gainMax = 18.0f;
inline constexpr float qMin    = 0.1f;
inline constexpr float qMax    = 18.0f;
inline constexpr float outMin  = -24.0f;
inline constexpr float outMax  = 24.0f;

inline juce::StringArray filterTypeChoices()
{
    // Append-only (CONTRACT C2): indices 0..5 are frozen for preset/session back-compat and
    // each index must equal its FilterType enum value. 6/7/8 = Tilt Shelf / Band-Pass / All-Pass.
    return { "High Pass", "Low Shelf", "Bell", "Notch", "High Shelf", "Low Pass",
             "Tilt Shelf", "Band-Pass", "All-Pass" };
}

inline juce::StringArray slopeChoices()
{
    // Append-only (C2): 0..2 frozen; 3/4/5 add the steeper cuts + Brickwall. (The deep
    // cascade is kept finite by BandDsp::processSample's non-finite guard in Biquad.h.)
    return { "12 dB/oct", "24 dB/oct", "48 dB/oct",
             "72 dB/oct", "96 dB/oct", "Brickwall" };
}

inline int slopeIndexToValue (int idx)
{
    switch (idx)
    {
        case 0:  return 12;
        case 1:  return 24;
        case 2:  return 48;
        case 3:  return 72;
        case 4:  return 96;
        default: return kBrickwallSlope;   // 5 = Brickwall (sentinel from dsp/EqMath.h)
    }
}

inline int slopeValueToIndex (int v)
{
    if (v <= 12) return 0;
    if (v <= 24) return 1;
    if (v <= 48) return 2;
    if (v <= 72) return 3;
    if (v <= 96) return 4;
    return 5;   // Brickwall (kBrickwallSlope)
}

// A log-frequency range that matches the design's f01 mapping (20 Hz .. 20 kHz).
inline juce::NormalisableRange<float> makeFreqRange()
{
    juce::NormalisableRange<float> r (freqMin, freqMax, // NOSONAR(cpp:S6012): CTAD cannot deduce ValueType here — this ctor takes lambdas, not std::function
        [] (float start, float end, float t) { return start * std::pow (end / start, t); },
        [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
        [] (float start, float end, float v) { return juce::jlimit (start, end, v); });
    return r;
}

inline juce::NormalisableRange<float> makeQRange()
{
    juce::NormalisableRange<float> r (qMin, qMax, // NOSONAR(cpp:S6012): CTAD cannot deduce ValueType here — this ctor takes lambdas, not std::function
        [] (float start, float end, float t) { return start * std::pow (end / start, t); },
        [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
        [] (float start, float end, float v) { return juce::jlimit (start, end, v); });
    return r;
}

inline juce::NormalisableRange<float> makeLogRange (float lo, float hi)
{
    return { lo, hi,
        [] (float s, float e, float t) { return s * std::pow (e / s, t); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

// The default six-band shape from the design handoff (README "Default bands").
struct BandDefault { FilterType type; float freq; float gain; float q; int slope; };

inline std::array<BandDefault, numBands> defaultBands()
{
    // Slots 0-5 keep the design-handoff shape (active by default). Slots 6-23 are inert
    // pool slots: valid, in-range values (so host/pluginval see sane defaults) but silent
    // because their `active` flag defaults false — they only come alive via addBand().
    constexpr BandDefault inert { FilterType::bell, 1000.0f, 0.0f, 0.707f, 12 };
    std::array<BandDefault, numBands> d {};
    d[0] = { FilterType::highPass,  30.0f,   0.0f, 0.71f, 24 };
    d[1] = { FilterType::bell,      95.0f,   3.2f, 0.90f, 12 };
    d[2] = { FilterType::bell,     420.0f,  -3.8f, 1.50f, 12 };
    d[3] = { FilterType::bell,    2600.0f,   2.6f, 1.10f, 12 };
    d[4] = { FilterType::highShelf, 8200.0f, 3.6f, 0.70f, 12 };
    d[5] = { FilterType::lowPass, 19000.0f,  0.0f, 0.71f, 12 };
    for (int i = 6; i < numBands; ++i)
        d[(size_t) i] = inert;
    return d;
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

        // Pool-slot existence flag (Pro-Q add/remove model): slots 0-5 are present by
        // default, 6-23 are spare slots that addBand() activates. Gates the audio path
        // ahead of `on`, so missing/old states fall back to exactly six live bands.
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::active (i), 1 }, g + "Active", i < 6));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::solo (i), 1 }, g + "Solo", false));

        // Per-band lane within the global domain: Both / first / second
        // (= L+R/L/R in Stereo, M+S/M/S in Mid-Side).
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::channel (i), 1 }, g + "Channel",
            StringArray { "Both", "Left / Mid", "Right / Side" }, 0));

        // Dynamic EQ (bell/shelf only): detector-driven gain offset.
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::dynOn (i), 1 }, g + "Dyn On", false));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::dynThresh (i), 1 }, g + "Dyn Threshold",
            NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::dynRange (i), 1 }, g + "Dyn Range",
            NormalisableRange<float> (-30.0f, 30.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::dynAttack (i), 1 }, g + "Dyn Attack",
            makeLogRange (0.5f, 200.0f), 10.0f,
            AudioParameterFloatAttributes().withLabel ("ms")));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::dynRelease (i), 1 }, g + "Dyn Release",
            makeLogRange (10.0f, 2000.0f), 150.0f,
            AudioParameterFloatAttributes().withLabel ("ms")));
        // Detection direction: Over = react above threshold (downward), Under = below (upward).
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ids::dynDir (i), 1 }, g + "Dyn Direction",
            StringArray { "Over", "Under" }, 0));
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

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::matchamount, 1 }, "Match Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // Overall EQ gain (0..200%, 100% = unity), applied in the output stage.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::gainScale, 1 }, "Gain Scale",
        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // Analyzer display mode (read by the graph; the engine always feeds both taps).
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::analyzerOn, 1 }, "Analyzer",
        StringArray { "Off", "Pre", "Post", "Pre + Post" }, 2));

    // Analyzer vertical dB range.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::analyzerRange, 1 }, "Analyzer Range",
        StringArray { "60 dB", "90 dB", "120 dB" }, 1));

    // Whole-EQ bypass: full passthrough (no filtering, no output stage).
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::globalBypass, 1 }, "Bypass", false));

    // Phase mode: only Zero Latency (the minimum-phase IIR path) behaves this round;
    // Natural / Linear are declared-but-inert until the linear-phase engine lands.
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::phaseMode, 1 }, "Phase Mode",
        StringArray { "Zero Latency", "Natural", "Linear" }, 0));

    return layout;
}

} // namespace zeq
