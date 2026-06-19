#pragma once

#include "EqMath.h"
#include <array>

namespace zeq
{

// A single Transposed Direct-Form II biquad. Coefficients live in BiquadCoeffs;
// this just carries the per-channel state and runs samples through it.
struct Biquad
{
    BiquadCoeffs c;
    float z1 = 0.0f, z2 = 0.0f;

    inline float processSample (float x) noexcept
    {
        const float y = static_cast<float> (c.b0) * x + z1;
        z1 = static_cast<float> (c.b1) * x - static_cast<float> (c.a1) * y + z2;
        z2 = static_cast<float> (c.b2) * x - static_cast<float> (c.a2) * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }
};

inline constexpr int maxCascade = 4; // 48 dB/oct = four cascaded biquads

// One EQ band's runtime DSP: up to `maxCascade` identical biquads in series,
// one independent state set per audio channel. Coefficients are shared across
// channels; only the delay-line state differs.
struct BandDsp
{
    std::array<std::array<Biquad, maxCascade>, 2> stages; // [channel][stage]
    int activeStages = 1;
    bool active = false; // contributes to the chain this block?

    void reset() noexcept
    {
        for (auto& ch : stages)
            for (auto& s : ch)
                s.reset();
    }

    // Recompute coefficients from current parameter values.
    void updateCoeffs (FilterType type, double freq, double gainDb, double q,
                       int slopeDbPerOct, double sampleRate) noexcept
    {
        const auto coeffs = makeCoeffs (type, freq, gainDb, q, sampleRate);
        activeStages = stagesForSlope (type, slopeDbPerOct);

        for (auto& ch : stages)
            for (int s = 0; s < maxCascade; ++s)
                ch[s].c = coeffs;
    }

    inline float processSample (int channel, float x) noexcept
    {
        auto& ch = stages[(size_t) channel];
        for (int s = 0; s < activeStages; ++s)
            x = ch[s].processSample (x);
        return x;
    }
};

// Per-band lane routing within the current global domain (L/R or M/S).
//   a = first lane  (L or Mid, state set 0)
//   b = second lane (R or Side, state set 1)
//   lane: 0 = both, 1 = first only, 2 = second only.
// Shared by the audio thread (processEq) and the routing unit test.
inline void applyBand (BandDsp& band, int lane, float& a, float& b) noexcept
{
    if (lane != 2) a = band.processSample (0, a);   // both or first
    if (lane != 1) b = band.processSample (1, b);   // both or second
}

} // namespace zeq
