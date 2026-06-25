#pragma once

#include <array>
#include <cmath>
#include <algorithm>

/*  EqMath — the single source of truth for the EQ's biquad math.

    This is a direct C++ port of the RBJ "Audio EQ Cookbook" coefficient math in
    design/EQGraph.jsx (the `bandDb()` function from the design handoff). The audio
    thread builds its filters from makeCoeffs(); the editor draws its response curve
    from bandMagnitudeDb(). Because both call the same code, the curve on screen can
    never disagree with what you hear.
*/
namespace zeq
{

inline constexpr double kPi = 3.14159265358979323846;

enum class FilterType
{
    highPass = 0,   // HP
    lowShelf,       // LO
    bell,           // BELL (peaking)
    notch,          // NTCH
    highShelf,      // HI
    lowPass         // LP
};

inline constexpr int numFilterTypes = 6;

// Fixed pool of EQ bands. Lives here (the pure header) so non-JUCE code such as
// MatchFit can share it. Bump to expand the EQ; strip/nodes/params follow.
inline constexpr int numBands = 6;

inline bool isCut (FilterType t) noexcept
{
    return t == FilterType::highPass || t == FilterType::lowPass;
}

// HP/LP/Notch sit on the 0 dB line on the graph (no gain handle).
inline bool sitsOnZeroLine (FilterType t) noexcept
{
    return t == FilterType::highPass || t == FilterType::lowPass || t == FilterType::notch;
}

// One biquad's coefficients, normalised so a0 == 1 (ready for Transposed Direct-Form II).
struct BiquadCoeffs
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;

    // |H(e^jw)| at frequency f for a given sample rate (linear magnitude).
    double magnitude (double f, double sampleRate) const noexcept
    {
        const double w   = 2.0 * kPi * f / sampleRate;
        const double cw1 = std::cos (w);
        const double sw1 = std::sin (w);
        const double cw2 = std::cos (2.0 * w);
        const double sw2 = std::sin (2.0 * w);

        const double numRe = b0 + b1 * cw1 + b2 * cw2;
        const double numIm = -(b1 * sw1 + b2 * sw2);
        const double denRe = 1.0 + a1 * cw1 + a2 * cw2;
        const double denIm = -(a1 * sw1 + a2 * sw2);

        const double num = numRe * numRe + numIm * numIm;
        const double den = denRe * denRe + denIm * denIm;
        return std::sqrt (num / std::max (1.0e-20, den));
    }
};

// RBJ cookbook coefficients for one band, normalised to a0 = 1.
inline BiquadCoeffs makeCoeffs (FilterType type, double freq, double gainDb, double q, double sampleRate) noexcept
{
    const double w0    = 2.0 * kPi * freq / sampleRate;
    const double cw    = std::cos (w0);
    const double sw    = std::sin (w0);
    const double Q     = std::max (0.1, q);
    const double alpha = sw / (2.0 * Q);
    const double A     = std::pow (10.0, gainDb / 40.0);

    double b0;
    double b1;
    double b2;
    double a0;
    double a1;
    double a2;

    switch (type)
    {
        case FilterType::lowShelf:
        {
            const double ap = 2.0 * std::sqrt (A) * alpha;
            b0 =       A * ((A + 1.0) - (A - 1.0) * cw + ap);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 =       A * ((A + 1.0) - (A - 1.0) * cw - ap);
            a0 =            (A + 1.0) + (A - 1.0) * cw + ap;
            a1 =    -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 =            (A + 1.0) + (A - 1.0) * cw - ap;
            break;
        }
        case FilterType::highShelf:
        {
            const double ap = 2.0 * std::sqrt (A) * alpha;
            b0 =        A * ((A + 1.0) + (A - 1.0) * cw + ap);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 =        A * ((A + 1.0) + (A - 1.0) * cw - ap);
            a0 =             (A + 1.0) - (A - 1.0) * cw + ap;
            a1 =     2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 =             (A + 1.0) - (A - 1.0) * cw - ap;
            break;
        }
        case FilterType::highPass:
        {
            b0 = (1.0 + cw) / 2.0; b1 = -(1.0 + cw); b2 = (1.0 + cw) / 2.0;
            a0 = 1.0 + alpha;      a1 = -2.0 * cw;   a2 = 1.0 - alpha;
            break;
        }
        case FilterType::lowPass:
        {
            b0 = (1.0 - cw) / 2.0; b1 = 1.0 - cw;  b2 = (1.0 - cw) / 2.0;
            a0 = 1.0 + alpha;      a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        }
        case FilterType::notch:
        {
            b0 = 1.0;         b1 = -2.0 * cw; b2 = 1.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
            break;
        }
        case FilterType::bell:
        default:
        {
            b0 = 1.0 + alpha * A; b1 = -2.0 * cw; b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A; a1 = -2.0 * cw; a2 = 1.0 - alpha / A;
            break;
        }
    }

    const double inv = 1.0 / a0;
    return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
}

// How many cascaded biquads a cut filter needs for the given slope (dB/oct).
inline int stagesForSlope (FilterType type, int slopeDbPerOct) noexcept
{
    if (! isCut (type))
        return 1;
    return std::max (1, slopeDbPerOct / 12); // 12->1, 24->2, 48->4
}

// RBJ band-pass (constant 0 dB peak gain), normalised to a0 = 1 — the per-band
// dynamic-EQ detector that isolates the band's own frequency region.
inline BiquadCoeffs makeBandpass (double freq, double q, double sampleRate) noexcept
{
    const double w0    = 2.0 * kPi * freq / sampleRate;
    const double cw    = std::cos (w0);
    const double sw    = std::sin (w0);
    const double alpha = sw / (2.0 * std::max (0.1, q));

    const double b0 = alpha;
    const double b1 = 0.0;
    const double b2 = -alpha;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cw;
    const double a2 = 1.0 - alpha;
    const double inv = 1.0 / a0;
    return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
}

// Dynamic-EQ detection direction: react when the band level is OVER the threshold
// (downward — duck/compress loud material) or UNDER it (upward — lift quiet material).
enum class DynDirection { over, under };

inline constexpr double kDynKnee = 8.0;   // soft-knee width (dB) for the dynamic offset

// Dynamic-EQ gain offset: 0 on the inactive side of the threshold, easing to
// `rangeDb` over a soft knee on the active side. Signed range (negative = cut,
// positive = boost). `dir` selects whether "active" means above or below threshold.
inline double dynamicGainDb (double levelDb, double thresholdDb, double rangeDb,
                             double kneeDb = kDynKnee,
                             DynDirection dir = DynDirection::over) noexcept
{
    const double delta = (dir == DynDirection::under) ? (thresholdDb - levelDb)
                                                      : (levelDb - thresholdDb);
    const double amount = std::clamp (delta / std::max (0.5, kneeDb), 0.0, 1.0);
    return rangeDb * amount;
}

// dB response of one band at frequency f, including the cut-filter cascade.
// N identical cascaded biquads multiply the magnitude, i.e. add in dB — so the
// displayed curve equals the audio path exactly.
inline double bandMagnitudeDb (FilterType type, double freq, double gainDb, double q, // NOSONAR(cpp:S107): callers in tests/ are out of editable scope
                               int slopeDbPerOct, bool on, double f, double sampleRate) noexcept
{
    if (! on)
        return 0.0;

    const auto c   = makeCoeffs (type, freq, gainDb, q, sampleRate);
    const double m = c.magnitude (f, sampleRate);
    double db = 20.0 * std::log10 (std::max (1.0e-7, m));
    db *= static_cast<double> (stagesForSlope (type, slopeDbPerOct));
    return db;
}

} // namespace zeq
