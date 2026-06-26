// Standalone unit tests for the EQ-match fitter and the Pro-Q-4 filter shapes/slopes
// (pure, no JUCE). Build with -DZEQ_BUILD_TESTS=ON; run the ZandersEQTests executable.

#include "../src/dsp/MatchFit.h"
#include "../src/dsp/Biquad.h"   // BandDsp + maxCascade for the shape/slope tests

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

    // ---- helpers for the filter-shape / slope tests ----
    bool approx (double a, double b, double eps) { return std::abs (a - b) <= eps; }

    // dB magnitude of one band at f via the shared math (slope only matters for cut filters).
    double dbAt (FilterType t, double f0, double gain, double q, double f, double sr, int slope = 12)
    {
        return bandMagnitudeDb (t, f0, gain, q, slope, true, f, sr);
    }

    bool coeffsFinite (const BiquadCoeffs& c)
    {
        return std::isfinite (c.b0) && std::isfinite (c.b1) && std::isfinite (c.b2)
            && std::isfinite (c.a1) && std::isfinite (c.a2);
    }

    // Steady-state gain (dB) of a sine pushed through the real cascade — the audio path.
    double sineGainDb (BandDsp& band, double freq, double sr, int channel = 0)
    {
        band.reset();
        const int warm = 8192, meas = 16384;
        const double w = 2.0 * kPi * freq / sr;
        double inSq = 0.0, outSq = 0.0;
        for (int n = 0; n < warm + meas; ++n)
        {
            const float x = (float) std::sin (w * n);
            const float y = band.processSample (channel, x);
            if (n >= warm) { inSq += (double) x * x; outSq += (double) y * y; }
        }
        return 10.0 * std::log10 (std::max (1.0e-300, outSq) / std::max (1.0e-300, inSq));
    }

    BandDsp makeBand (FilterType t, double f0, double gain, double q, int slope, double sr)
    {
        BandDsp b;
        b.active = true;
        b.updateCoeffs (t, f0, gain, q, slope, sr);
        return b;
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

    // ========================== Pro-Q 4 shapes & slopes ==========================
    // Same makeCoeffs/bandMagnitudeDb the audio path and the on-screen curve share, plus the
    // real BandDsp cascade — so these prove the shipping DSP, not a copy.

    // --- 5. Tilt shelf: pivots on 0 dB at f0 for every Q; +gain one side, -gain the other ---
    // Convention: positive gain => highs up, lows down.
    {
        const double f0 = 1000.0, G = 6.0;
        bool crossoverOk = true, asymptoteOk = true, signFlipOk = true;
        for (double q : { 0.5, 0.707, 2.0, 4.0 })
        {
            const double atF0  = dbAt (FilterType::tiltShelf, f0, G, q, f0,      SR);
            const double atLow  = dbAt (FilterType::tiltShelf, f0, G, q, 30.0,    SR);
            const double atHigh = dbAt (FilterType::tiltShelf, f0, G, q, 18000.0, SR);
            if (! approx (atF0, 0.0, 0.05))                              crossoverOk = false;
            if (! approx (atLow, -G, 1.0) || ! approx (atHigh, +G, 1.0)) asymptoteOk = false;
            if (! (atLow < 0.0 && atHigh > 0.0))                         signFlipOk = false;
        }
        check (crossoverOk, "tilt pivots on 0 dB at f0 for all Q");
        check (asymptoteOk, "tilt asymptotes to -gain (lows) / +gain (highs)");
        check (signFlipOk,  "tilt sign flips across the pivot");

        const double nF0  = dbAt (FilterType::tiltShelf, f0, -G, 1.0, f0,      SR);
        const double nLow = dbAt (FilterType::tiltShelf, f0, -G, 1.0, 30.0,    SR);
        const double nHi  = dbAt (FilterType::tiltShelf, f0, -G, 1.0, 18000.0, SR);
        check (approx (nF0, 0.0, 0.05) && nLow > 0.0 && nHi < 0.0, "tilt inverts with negative gain");

        // Tilt has gain, so it is NOT on the zero line (also covers sitsOnZeroLine's false path).
        check (! sitsOnZeroLine (FilterType::tiltShelf), "tilt shelf keeps its gain handle");
    }

    // --- 6. Band-pass: RBJ constant 0 dB peak, rolls off both sides, ignores gain ---
    {
        const double f0 = 1000.0, q = 1.0;
        const double peak = dbAt (FilterType::bandPass, f0, 0.0, q, f0,     SR);
        const double lo   = dbAt (FilterType::bandPass, f0, 0.0, q, 250.0,  SR);
        const double hi   = dbAt (FilterType::bandPass, f0, 0.0, q, 4000.0, SR);
        check (approx (peak, 0.0, 1.0e-3),      "band-pass peak is 0 dB at f0", peak);
        check (lo < -1.0 && hi < -1.0,          "band-pass rolls off both sides", std::max (lo, hi));
        check (peak > lo && peak > hi,          "band-pass peak is at the band frequency");
        check (approx (lo, hi, 0.5),            "band-pass is symmetric in log-frequency", std::abs (lo - hi));
        const double withGain = dbAt (FilterType::bandPass, f0, 12.0, q, f0, SR);
        check (approx (withGain, peak, 1.0e-9), "band-pass ignores gain", std::abs (withGain - peak));
        check (sitsOnZeroLine (FilterType::bandPass), "band-pass classified on the zero line");
    }

    // --- 7. All-pass: unity magnitude + energy-preserving, but not an identity ---
    {
        const double f0 = 1000.0, q = 1.0;
        bool unity = true;
        for (double f : { 50.0, 200.0, 1000.0, 5000.0, 18000.0 })
            if (! approx (dbAt (FilterType::allPass, f0, 0.0, q, f, SR), 0.0, 1.0e-6)) unity = false;
        check (unity, "all-pass magnitude is unity across the band");
        check (sitsOnZeroLine (FilterType::allPass), "all-pass classified on the zero line");

        auto band = makeBand (FilterType::allPass, f0, 0.0, q, 12, SR);
        band.reset();
        double energy = 0.0, tailEnergy = 0.0; float h0 = 0.0f;
        for (int n = 0; n < 16384; ++n)
        {
            const float x = (n == 0 ? 1.0f : 0.0f);
            const float h = band.processSample (0, x);
            energy += (double) h * h;
            if (n == 0) h0 = h; else tailEnergy += (double) h * h;
        }
        check (approx (energy, 1.0, 5.0e-3), "all-pass preserves impulse energy", energy);
        check (! approx (h0, 1.0f, 1.0e-3) && tailEnergy > 1.0e-3,
               "all-pass is a real filter, not an identity", tailEnergy);
    }

    // --- 8. stagesForSlope: 72/96/Brickwall mapping + clamp ---
    {
        check (stagesForSlope (FilterType::highPass, 12) == 1, "slope 12 -> 1 stage");
        check (stagesForSlope (FilterType::highPass, 24) == 2, "slope 24 -> 2 stages");
        check (stagesForSlope (FilterType::highPass, 48) == 4, "slope 48 -> 4 stages");
        check (stagesForSlope (FilterType::highPass, 72) == 6, "slope 72 -> 6 stages");
        check (stagesForSlope (FilterType::lowPass,  96) == 8, "slope 96 -> 8 stages");
        check (stagesForSlope (FilterType::highPass, kBrickwallSlope) == kMaxCascadeStages,
               "Brickwall -> deepest cascade");
        check (kMaxCascadeStages == maxCascade, "kMaxCascadeStages == maxCascade");
        check (stagesForSlope (FilterType::highPass, 200) == kMaxCascadeStages,
               "over-range slope clamps to maxCascade");
        check (stagesForSlope (FilterType::bell, 96) == 1, "non-cut type is always 1 stage");
        check (stagesForSlope (FilterType::tiltShelf, 96) == 1
            && stagesForSlope (FilterType::bandPass,  96) == 1
            && stagesForSlope (FilterType::allPass,   96) == 1, "new shapes are single-stage");
    }

    // --- 9. Audio path: 8-stage cascade (no OOB), roll-off scales with stage count ---
    {
        const double fc = 1000.0, q = 0.707;
        auto hp12 = makeBand (FilterType::highPass, fc, 0.0, q, 12, SR);
        auto hp48 = makeBand (FilterType::highPass, fc, 0.0, q, 48, SR);
        auto hp96 = makeBand (FilterType::highPass, fc, 0.0, q, 96, SR);
        auto hpBw = makeBand (FilterType::highPass, fc, 0.0, q, kBrickwallSlope, SR);
        check (hp96.activeStages == 8, "slope 96 activates 8 cascade stages", hp96.activeStages);
        check (hpBw.activeStages == 8, "Brickwall activates the deepest cascade", hpBw.activeStages);

        const double a12 = sineGainDb (hp12, fc, SR);
        const double a48 = sineGainDb (hp48, fc, SR);
        const double a96 = sineGainDb (hp96, fc, SR);
        check (std::isfinite (a96) && a96 < a48 && a48 < a12, "deeper slope attenuates more", a96);
        check (approx (a96 / a12, 8.0, 0.4), "roll-off at cutoff scales x8 vs single stage", a96 / a12);
        check (approx (a96 / a48, 2.0, 0.2), "roll-off at cutoff scales x2 vs 48 dB/oct", a96 / a48);
        const double shown = dbAt (FilterType::highPass, fc, 0.0, q, fc, SR, 96);
        check (approx (a96, shown, 1.0), "display matches audio for the 8-stage high-pass",
               std::abs (a96 - shown));
    }

    // --- 10. Display == audio for the new shapes ---
    {
        auto tilt = makeBand (FilterType::tiltShelf, 1000.0, 6.0, 0.707, 12, SR);
        const double th = sineGainDb (tilt, 5000.0, SR), thShown = dbAt (FilterType::tiltShelf, 1000.0, 6.0, 0.707, 5000.0, SR);
        const double tl = sineGainDb (tilt,  200.0, SR), tlShown = dbAt (FilterType::tiltShelf, 1000.0, 6.0, 0.707,  200.0, SR);
        check (approx (th, thShown, 0.5) && approx (tl, tlShown, 0.5),
               "display matches audio for the tilt shelf", std::abs (th - thShown));

        auto bp = makeBand (FilterType::bandPass, 1000.0, 0.0, 1.0, 12, SR);
        const double bpk = sineGainDb (bp, 1000.0, SR), bOff = sineGainDb (bp, 500.0, SR);
        check (approx (bpk, 0.0, 0.3), "band-pass passes the centre frequency at 0 dB", bpk);
        check (approx (bOff, dbAt (FilterType::bandPass, 1000.0, 0.0, 1.0, 500.0, SR), 0.5),
               "display matches audio for the band-pass", bOff);
    }

    // --- 11. Robustness near Nyquist (44.1 kHz, 20 kHz) ---
    {
        const double sr2 = 44100.0, fHi = 20000.0;
        bool finite = true;
        for (FilterType t : { FilterType::tiltShelf, FilterType::bandPass, FilterType::allPass })
        {
            const auto c = makeCoeffs (t, fHi, 6.0, 1.0, sr2);
            if (! coeffsFinite (c) || ! std::isfinite (c.magnitude (fHi, sr2))) finite = false;
        }
        check (finite, "new shapes stay finite at 20 kHz / 44.1 kHz");
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
