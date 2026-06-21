#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/EqMath.h"

// Design tokens + graph mappings from the Neon Plugins handoff
// (design/_ds/.../tokens/*.css and design/EQGraph.jsx).
namespace zeq::theme
{

// ---- spectrum ramp (the brand) ----------------------------------------------
inline const juce::Colour cyan   { 0xff34d8ff };
inline const juce::Colour violet { 0xff8b7bff };
inline const juce::Colour pink   { 0xffff5fa8 };
inline const juce::Colour amber  { 0xffffc24b };

// ---- accent / danger --------------------------------------------------------
inline const juce::Colour accent     { 0xff5e93ff };
inline const juce::Colour accentVio  { 0xff8b7bff };
inline const juce::Colour danger     { 0xffff5a5a };
inline const juce::Colour dangerDeep { 0xffe23b3b };

// ---- surface ----------------------------------------------------------------
inline const juce::Colour panelTop  { 0xff1a2030 };
inline const juce::Colour panelBase  { 0xff0a0b12 };
inline const juce::Colour well       { 0xff070a0e };
inline const juce::Colour wellDeep   { 0xff050608 };
inline const juce::Colour stageTop   { 0xff14191f };
inline const juce::Colour stageBase  { 0xff050608 };

// ---- text -------------------------------------------------------------------
inline const juce::Colour textBright { 0xffffffff };
inline const juce::Colour text1      { 0xffe8ecf3 };
inline const juce::Colour text2      { 0xffcfd4dc };
inline const juce::Colour text3      { 0xff9aa3b3 };
inline const juce::Colour textMuted  { 0xff8a93a3 };
inline const juce::Colour textLabel  { 0xff7e8794 };
inline const juce::Colour textFaint  { 0xff5d6473 };

inline juce::Colour whiteAlpha (float a) { return juce::Colours::white.withAlpha (a); }

// ---- four-stop ramp ---------------------------------------------------------
inline juce::Colour rampColour (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    const juce::Colour stops[4] = { cyan, violet, pink, amber };
    const float seg = t * 3.0f;
    const int   i   = juce::jmin (2, (int) seg);
    const float f   = seg - (float) i;
    return stops[i].interpolatedWith (stops[i + 1], f);
}

// Band node colour = position of its frequency along log(20 -> 20k).
inline juce::Colour colourForFreq (float freq)
{
    return rampColour (std::log (freq / 20.0f) / std::log (1000.0f));
}

// ---- graph mappings (match EQGraph.jsx) -------------------------------------
inline constexpr float fMin = 20.0f, fMax = 20000.0f;
inline const float logSpan = std::log (fMax / fMin);
inline constexpr float dbRange = 18.0f;     // vertical half-range
inline constexpr float vSpan   = 0.84f;     // fraction of half-height used by +-dbRange

inline float freqToX (float f, float w)  { return std::log (f / fMin) / logSpan * w; }
inline float xToFreq (float x, float w)  { return fMin * std::exp ((x / w) * logSpan); }
inline float gainToY (float g, float h)  { return h * 0.5f - (g / dbRange) * (h * 0.5f * vSpan); }
inline float yToGain (float y, float h)  { return ((h * 0.5f - y) / (h * 0.5f * vSpan)) * dbRange; }

// ---- formatters -------------------------------------------------------------
inline juce::String fmtFreq (float f)
{
    if (f < 1000.0f)
        return juce::String (juce::roundToInt (f)) + " Hz";
    return juce::String (f / 1000.0f, f < 10000.0f ? 2 : 1) + " kHz";
}

// Nearest equal-tempered note name (12-TET, A4 = 440 Hz), e.g. 2637 Hz -> "E7".
inline juce::String noteName (float f)
{
    if (f <= 0.0f)
        return {};
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = juce::roundToInt (69.0f + 12.0f * std::log2 (f / 440.0f));
    const int idx  = ((midi % 12) + 12) % 12;   // wrap negatives for very low notes
    return juce::String (names[idx]) + juce::String (midi / 12 - 1);
}

inline juce::String fmtGain (float g)
{
    return (g >= 0.0f ? "+" : "") + juce::String (g, 1) + " dB";
}

inline juce::String typeLabel (FilterType t)
{
    switch (t)
    {
        case FilterType::highPass:  return "HI-PASS";
        case FilterType::lowShelf:  return "LO SHELF";
        case FilterType::bell:      return "BELL";
        case FilterType::notch:     return "NOTCH";
        case FilterType::highShelf: return "HI SHELF";
        case FilterType::lowPass:   return "LO-PASS";
    }
    return "BELL";
}

} // namespace zeq::theme
