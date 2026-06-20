// Standalone unit tests for the EQ-match fitter (pure, no JUCE).
// Build with -DZEQ_BUILD_TESTS=ON; run the ZandersEQTests executable.

#include "../src/dsp/MatchFit.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace zeq;

namespace
{
    int failures = 0;

    void check (bool ok, const char* name, double detail = 0.0)
    {
        if (ok) { std::printf ("  PASS  %s\n", name); }
        else    { std::printf ("  FAIL  %s (%.3f)\n", name, detail); ++failures; }
    }

    constexpr int K = kMatchBins;
    constexpr double SR = 48000.0;

    // Summed dB response of a fit across the match grid.
    std::array<double, K> fittedResponse (const MatchResult& r)
    {
        std::array<double, K> out {};
        for (int k = 0; k < K; ++k)
        {
            double db = 0.0;
            for (int i = 0; i < r.used; ++i)
            {
                const auto& b = r.bands[(size_t) i];
                if (b.on)
                    db += bandMagnitudeDb (b.type, b.freq, b.gain, b.q, b.slope, true, matchBinFreq (k, K), SR);
            }
            out[(size_t) k] = db;
        }
        return out;
    }

    double meanOf (const std::array<double, K>& a)
    {
        double s = 0.0; for (double v : a) s += v; return s / K;
    }

    // Max |demean(a) - demean(b)| over the interior (ignore the extreme edge bins).
    double maxDemeanedError (const std::array<double, K>& a, const std::array<double, K>& b)
    {
        const double ma = meanOf (a), mb = meanOf (b);
        double e = 0.0;
        for (int k = 4; k < K - 4; ++k)
            e = std::max (e, std::abs ((a[(size_t) k] - ma) - (b[(size_t) k] - mb)));
        return e;
    }
}

int main()
{
    std::printf ("MatchFit tests (K=%d)\n", K);

    // --- 1. Synthetic round-trip: a known shape must be recovered ------------
    {
        const MatchBand truthArr[3] = {
            { FilterType::bell,      200.0f,  6.0f, 1.0f, 12, true },
            { FilterType::bell,     2000.0f, -5.0f, 2.0f, 12, true },
            { FilterType::highShelf, 9000.0f, 4.0f, 0.7f, 12, true },
        };
        std::array<double, K> target {};
        std::vector<float> refP (K), srcP (K, 1.0f);
        for (int k = 0; k < K; ++k)
        {
            double db = 0.0;
            for (auto& b : truthArr)
                db += bandMagnitudeDb (b.type, b.freq, b.gain, b.q, b.slope, true, matchBinFreq (k, K), SR);
            target[(size_t) k] = db;
            refP[(size_t) k] = (float) std::pow (10.0, db / 10.0); // 10log10(ref/src) == db
        }
        const auto res = fitMatch (refP.data(), srcP.data(), K, SR, 1.0f);
        const auto fit = fittedResponse (res);
        const double err = maxDemeanedError (fit, target);
        check (res.used >= 2, "round-trip uses multiple bands");
        check (err < 2.0, "round-trip recovers shape within 2 dB", err);
    }

    // --- 2. Pink vs white: match curve must fall with frequency --------------
    {
        std::vector<float> refP (K), srcP (K, 1.0f);
        for (int k = 0; k < K; ++k)
            refP[(size_t) k] = (float) (1.0 / matchBinFreq (k, K)); // pink-ish (falling)
        const auto res = fitMatch (refP.data(), srcP.data(), K, SR, 1.0f);
        const auto fit = fittedResponse (res);
        check (res.used >= 1, "pink/white places a band");
        check (fit[(size_t) (K / 8)] > fit[(size_t) (7 * K / 8)] + 3.0,
               "pink/white tilts downward (low > high)", fit[(size_t)(K/8)] - fit[(size_t)(7*K/8)]);
    }

    // --- 3. Identical spectra: ~no correction -------------------------------
    {
        std::vector<float> refP (K, 0.5f), srcP (K, 0.5f);
        const auto res = fitMatch (refP.data(), srcP.data(), K, SR, 1.0f);
        const auto fit = fittedResponse (res);
        double maxAbs = 0.0;
        for (int k = 4; k < K - 4; ++k) maxAbs = std::max (maxAbs, std::abs (fit[(size_t) k]));
        check (maxAbs < 0.5, "identical spectra -> flat", maxAbs);
    }

    // --- 4. Amount scaling halves the moves ---------------------------------
    {
        const MatchBand truth { FilterType::bell, 1000.0f, 8.0f, 1.5f, 12, true };
        std::vector<float> refP (K), srcP (K, 1.0f);
        for (int k = 0; k < K; ++k)
            refP[(size_t) k] = (float) std::pow (10.0,
                bandMagnitudeDb (truth.type, truth.freq, truth.gain, truth.q, truth.slope, true, matchBinFreq (k, K), SR) / 10.0);
        const auto full = fitMatch (refP.data(), srcP.data(), K, SR, 1.0f);
        const auto half = fitMatch (refP.data(), srcP.data(), K, SR, 0.5f);
        double pf = 0.0, ph = 0.0;
        for (int k = 0; k < K; ++k) { pf = std::max (pf, std::abs (fittedResponse (full)[(size_t) k])); }
        for (int k = 0; k < K; ++k) { ph = std::max (ph, std::abs (fittedResponse (half)[(size_t) k])); }
        check (ph < pf && ph > 0.0, "amount=0.5 is between 0 and full", ph / std::max (1e-9, pf));
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
