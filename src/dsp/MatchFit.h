#pragma once

#include "EqMath.h"
#include <array>
#include <cmath>
#include <algorithm>

/*  MatchFit — EQ-match curve fitting.

    Given an averaged REFERENCE and SOURCE power spectrum (log-spaced, kMatchBins
    bins on the same 20 Hz–20 kHz geometry as the analyzer), compute the de-meaned
    tonal-balance difference and fit the plugin's `numBands` EQ bands to it. The fit
    models each band with the same `bandMagnitudeDb` the audio path uses, so the
    resulting bands recreate the match curve exactly — and stay fully editable.

    Pure / header-only: no JUCE dependency, so it is unit-testable in isolation.
*/
namespace zeq
{

inline constexpr int kMatchBins = 256;

// Frequency at match bin k (log-spaced 20..20k — matches Theme.h's analyzer grid).
inline double matchBinFreq (int k, int K) noexcept
{
    const double fr = (double) k / (double) (K - 1);
    return 20.0 * std::exp (fr * std::log (1000.0));
}

struct MatchBand
{
    FilterType type  = FilterType::bell;
    float      freq  = 1000.0f;
    float      gain  = 0.0f;
    float      q     = 1.0f;
    int        slope = 12;
    bool       on    = false;
};

struct MatchResult
{
    std::array<MatchBand, numBands> bands {};
    int used = 0;
};

namespace matchdetail
{
    // Smooth a log-spaced curve over a fixed bin half-width (≈ constant fractional
    // octave), weighting each tap by per-bin confidence.
    inline void smoothOctave (std::array<double, kMatchBins>& t,
                              const std::array<double, kMatchBins>& w, int K, int halfBins)
    {
        std::array<double, kMatchBins> out {};
        for (int k = 0; k < K; ++k)
        {
            double sum = 0.0;
            double wsum = 0.0;
            for (int j = std::max (0, k - halfBins); j <= std::min (K - 1, k + halfBins); ++j)
            {
                const double tri = 1.0 - (double) std::abs (j - k) / (double) (halfBins + 1);
                const double ww  = w[(size_t) j] * tri;
                sum  += t[(size_t) j] * ww;
                wsum += ww;
            }
            out[(size_t) k] = wsum > 1.0e-9 ? sum / wsum : t[(size_t) k];
        }
        t = out;
    }

    // Build the conditioned target (dB, de-meaned, smoothed) and per-bin weights.
    inline void conditionTarget (const float* refPower, const float* srcPower, int K,
                                 std::array<double, kMatchBins>& t,
                                 std::array<double, kMatchBins>& w)
    {
        std::array<double, kMatchBins> rDb {};
        std::array<double, kMatchBins> sDb {};
        double rPeak = -300.0;
        double sPeak = -300.0;
        for (int k = 0; k < K; ++k)
        {
            rDb[(size_t) k] = 10.0 * std::log10 ((double) std::max (1.0e-20f, refPower[k]));
            sDb[(size_t) k] = 10.0 * std::log10 ((double) std::max (1.0e-20f, srcPower[k]));
            rPeak = std::max (rPeak, rDb[(size_t) k]);
            sPeak = std::max (sPeak, sDb[(size_t) k]);
        }

        // Confidence weight relative to each spectrum's own peak (level-independent):
        // bins fade out as they drop ~80 dB below their peak.
        auto rel = [] (double db, double peak)
        {
            return std::clamp ((db - (peak - 80.0)) / 40.0, 0.0, 1.0);
        };

        double wsum = 0.0;
        double wt = 0.0;
        for (int k = 0; k < K; ++k)
        {
            t[(size_t) k] = rDb[(size_t) k] - sDb[(size_t) k];
            w[(size_t) k] = rel (rDb[(size_t) k], rPeak) * rel (sDb[(size_t) k], sPeak);
            wsum += w[(size_t) k];
            wt   += w[(size_t) k] * t[(size_t) k];
        }

        // De-mean: match tone, not level.
        const double mean = wsum > 1.0e-9 ? wt / wsum : 0.0;
        for (int k = 0; k < K; ++k)
            t[(size_t) k] -= mean;

        // ~1/3-octave smoothing.
        const double binsPerOct = (K - 1) / std::log2 (1000.0);
        const int halfBins = std::max (1, (int) std::round (binsPerOct / 6.0));
        smoothOctave (t, w, K, halfBins);
    }

    inline FilterType chooseType (int peak, int K, const std::array<double, kMatchBins>& res)
    {
        const double f = matchBinFreq (peak, K);
        if (f < 120.0)
        {
            const int lo = std::max (0, peak - K / 12);
            if (std::abs (res[(size_t) lo]) >= std::abs (res[(size_t) peak]) * 0.6)
                return FilterType::lowShelf;
        }
        if (f > 6000.0)
        {
            const int hi = std::min (K - 1, peak + K / 12);
            if (std::abs (res[(size_t) hi]) >= std::abs (res[(size_t) peak]) * 0.6)
                return FilterType::highShelf;
        }
        return FilterType::bell;
    }

    // Q from the residual's half-height bandwidth around the peak.
    inline float estimateQ (const std::array<double, kMatchBins>& res, int peak, int K)
    {
        const double A = res[(size_t) peak];
        if (std::abs (A) < 1.0e-6) return 1.0f;
        const double halfMag = std::abs (A) * 0.5;
        const bool pos = A > 0.0;

        int l = peak;
        int r = peak;
        while (l > 0     && (res[(size_t) l] > 0.0) == pos && std::abs (res[(size_t) l]) > halfMag) --l;
        while (r < K - 1 && (res[(size_t) r] > 0.0) == pos && std::abs (res[(size_t) r]) > halfMag) ++r;

        const double fL = matchBinFreq (l, K);
        const double fR = matchBinFreq (r, K);
        if (fR <= fL) return 2.0f;
        const double bwOct = std::log2 (fR / fL);
        if (bwOct < 1.0e-3) return 6.0f;
        const double q = std::sqrt (std::pow (2.0, bwOct)) / (std::pow (2.0, bwOct) - 1.0);
        return (float) std::clamp (q, 0.5, 6.0);
    }

    // Solve A x = b (n<=6) via Gaussian elimination with partial pivoting.
    inline bool solveLinear (std::array<std::array<double, 6>, 6>& A,
                             std::array<double, 6>& b,
                             std::array<double, 6>& x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            int piv = i;
            double best = std::abs (A[i][i]);
            for (int r = i + 1; r < n; ++r)
            {
                if (std::abs (A[r][i]) > best) { best = std::abs (A[r][i]); piv = r; }
            }
            if (best < 1.0e-9) return false;
            if (piv != i) { for (int c = 0; c < n; ++c) std::swap (A[i][c], A[piv][c]); std::swap (b[i], b[piv]); }
            for (int r = i + 1; r < n; ++r)
            {
                const double f = A[r][i] / A[i][i];
                for (int c = i; c < n; ++c) A[r][c] -= f * A[i][c];
                b[r] -= f * b[i];
            }
        }
        for (int i = n - 1; i >= 0; --i)
        {
            double s = b[i];
            for (int c = i + 1; c < n; ++c) s -= A[i][c] * x[c];
            x[i] = s / A[i][i];
        }
        return true;
    }

    // Weighted dot product sum_k w[k] * a[k] * b[k] over [0, K).
    inline double dotWeighted (const std::array<double, kMatchBins>& a,
                               const std::array<double, kMatchBins>& b,
                               const std::array<double, kMatchBins>& w, int K)
    {
        double s = 0.0;
        for (int k = 0; k < K; ++k)
            s += w[(size_t) k] * a[(size_t) k] * b[(size_t) k];
        return s;
    }

    // Index of the highest weighted-residual bin in [0, K), or -1 if none exceeds 0.
    inline int findPeakBin (const std::array<double, kMatchBins>& residual,
                            const std::array<double, kMatchBins>& w, int K)
    {
        int peak = -1;
        double best = 0.0;
        for (int k = 0; k < K; ++k)
        {
            const double m = w[(size_t) k] * std::abs (residual[(size_t) k]);
            if (m > best) { best = m; peak = k; }
        }
        return peak;
    }

    // Weighted least-squares gain refinement (type/freq/Q fixed → linear in gain).
    // Recomputes each used band's gain in place from the conditioned target t.
    inline void refineGains (MatchResult& result,
                             const std::array<double, kMatchBins>& t,
                             const std::array<double, kMatchBins>& w, int K, double sr)
    {
        const int n = result.used;
        std::array<std::array<double, kMatchBins>, numBands> phi {};
        for (int i = 0; i < n; ++i)
        {
            const auto& b = result.bands[(size_t) i];
            for (int k = 0; k < K; ++k)
                phi[(size_t) i][(size_t) k] = bandMagnitudeDb (b.type, b.freq, 1.0, b.q, b.slope, true,
                                                               matchBinFreq (k, K), sr);
        }
        std::array<std::array<double, 6>, 6> A {};
        std::array<double, 6> bb {};
        std::array<double, 6> x {};
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
                A[i][j] = dotWeighted (phi[(size_t) i], phi[(size_t) j], w, K);
            bb[i] = dotWeighted (phi[(size_t) i], t, w, K);
        }
        if (solveLinear (A, bb, x, n))
            for (int i = 0; i < n; ++i)
                result.bands[(size_t) i].gain = (float) std::clamp (x[i], -18.0, 18.0);
    }
} // namespace matchdetail

// The conditioned target curve in dB (de-meaned, smoothed) — for the ghost overlay.
inline void computeTargetDb (const float* refPower, const float* srcPower, int K, float* outDb)
{
    std::array<double, kMatchBins> t {};
    std::array<double, kMatchBins> w {};
    matchdetail::conditionTarget (refPower, srcPower, std::min (K, kMatchBins), t, w);
    for (int k = 0; k < std::min (K, kMatchBins); ++k)
        outDb[k] = (float) t[(size_t) k];
}

// Fit the bands to ref/src. amount (0..1) scales the applied gains.
inline MatchResult fitMatch (const float* refPower, const float* srcPower, int K, double sr, float amount)
{
    using namespace matchdetail;
    MatchResult result;
    K = std::min (K, kMatchBins);

    std::array<double, kMatchBins> t {};
    std::array<double, kMatchBins> w {};
    std::array<double, kMatchBins> residual {};
    conditionTarget (refPower, srcPower, K, t, w);
    residual = t;

    // Greedy placement.
    const double gateDb = 0.75;
    for (int n = 0; n < numBands; ++n)
    {
        const int peak = findPeakBin (residual, w, K);
        if (peak < 0 || std::abs (residual[(size_t) peak]) < gateDb)
            break;

        MatchBand b;
        b.type  = chooseType (peak, K, residual);
        b.freq  = (float) std::clamp (matchBinFreq (peak, K), 20.0, 20000.0);
        b.gain  = (float) std::clamp (residual[(size_t) peak], -18.0, 18.0);
        b.q     = (b.type == FilterType::bell) ? estimateQ (residual, peak, K) : 0.7f;
        b.slope = 12;
        b.on    = true;
        result.bands[(size_t) n] = b;
        result.used = n + 1;

        for (int k = 0; k < K; ++k)
            residual[(size_t) k] -= bandMagnitudeDb (b.type, b.freq, b.gain, b.q, b.slope, true,
                                                     matchBinFreq (k, K), sr);
    }

    // Weighted least-squares gain refinement (type/freq/Q fixed → linear in gain).
    if (result.used >= 1)
        refineGains (result, t, w, K, sr);

    // Apply match amount.
    const float amt = std::clamp (amount, 0.0f, 1.0f);
    for (int i = 0; i < result.used; ++i)
        result.bands[(size_t) i].gain *= amt;

    return result;
}

} // namespace zeq
