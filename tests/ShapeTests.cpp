// Unit tests for the Pro-Q-4 filter shapes and steeper slopes (pure, no JUCE).
// Exercises the shipping DSP — makeCoeffs / bandMagnitudeDb / stagesForSlope and the real
// BandDsp cascade from src/dsp/Biquad.h — so the test proves the audio path, not a copy.
//
// Covers: tilt shelf (sign flips across f0, pivots on 0 dB), band-pass (0 dB peak, rolls off
// both sides), all-pass (unity magnitude + energy-preserving), the 72/96/Brickwall slope math,
// the maxCascade=8 cascade (no out-of-bounds, roll-off scales with stage count), and the
// display==audio invariant for the new shapes.

#include "../src/dsp/Biquad.h"

#include <array>
#include <cmath>
#include <cstdio>

using namespace zeq;

namespace
{
    int failures = 0;
    void check (bool ok, const char* name, double detail = 0.0)
    {
        if (ok) std::printf ("  PASS  %s\n", name);
        else  { std::printf ("  FAIL  %s (%g)\n", name, detail); ++failures; }
    }

    bool approx (double a, double b, double eps) { return std::abs (a - b) <= eps; }

    // dB magnitude of one band at f via the shared math (slope only matters for cut filters).
    double dbAt (FilterType t, double f0, double gain, double q, double f, double sr,
                 int slope = 12)
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
    const double sr = 48000.0;
    std::printf ("Shape tests\n");

    // ------------------------------------------------------------------ Tilt shelf
    // Pivots through 0 dB exactly at f0 for every Q; +gain at one edge, -gain at the other.
    // Convention: positive gain => highs up, lows down (high-shelf basis).
    {
        const double f0 = 1000.0, G = 6.0;
        bool crossoverOk = true, asymptoteOk = true, signFlipOk = true;
        for (double q : { 0.5, 0.707, 2.0, 4.0 })
        {
            const double atF0   = dbAt (FilterType::tiltShelf, f0, G, q, f0,     sr);
            const double atLow   = dbAt (FilterType::tiltShelf, f0, G, q, 30.0,   sr);
            const double atHigh = dbAt (FilterType::tiltShelf, f0, G, q, 18000.0, sr);
            if (! approx (atF0, 0.0, 0.05))               crossoverOk = false;
            if (! approx (atLow, -G, 1.0) || ! approx (atHigh, +G, 1.0)) asymptoteOk = false;
            if (! (atLow < 0.0 && atHigh > 0.0))          signFlipOk = false;
        }
        check (crossoverOk, "tilt pivots on 0 dB at f0 for all Q");
        check (asymptoteOk, "tilt asymptotes to -gain (lows) / +gain (highs)");
        check (signFlipOk,  "tilt sign flips across the pivot");

        // Negative gain inverts the tilt (lows up, highs down), still 0 dB at f0.
        const double nF0  = dbAt (FilterType::tiltShelf, f0, -G, 1.0, f0,     sr);
        const double nLow = dbAt (FilterType::tiltShelf, f0, -G, 1.0, 30.0,   sr);
        const double nHi  = dbAt (FilterType::tiltShelf, f0, -G, 1.0, 18000.0, sr);
        check (approx (nF0, 0.0, 0.05) && nLow > 0.0 && nHi < 0.0, "tilt inverts with negative gain");
    }

    // ------------------------------------------------------------------ Band-pass
    // RBJ constant 0 dB peak: 0 dB at f0, rolls off both sides, gain ignored.
    {
        const double f0 = 1000.0, q = 1.0;
        const double peak = dbAt (FilterType::bandPass, f0, 0.0, q, f0,    sr);
        const double lo   = dbAt (FilterType::bandPass, f0, 0.0, q, 250.0, sr);
        const double hi   = dbAt (FilterType::bandPass, f0, 0.0, q, 4000.0, sr);
        check (approx (peak, 0.0, 1.0e-3),       "band-pass peak is 0 dB at f0", peak);
        check (lo < -1.0 && hi < -1.0,           "band-pass rolls off both sides", std::max (lo, hi));
        check (peak > lo && peak > hi,           "band-pass peak is at the band frequency");
        check (approx (lo, hi, 0.5),             "band-pass is symmetric in log-frequency", std::abs (lo - hi));

        // Gain is ignored for a band-pass (it sits on the zero line).
        const double withGain = dbAt (FilterType::bandPass, f0, 12.0, q, f0, sr);
        check (approx (withGain, peak, 1.0e-9),  "band-pass ignores gain", std::abs (withGain - peak));
        check (sitsOnZeroLine (FilterType::bandPass), "band-pass classified on the zero line");
    }

    // ------------------------------------------------------------------ All-pass
    // Unity magnitude everywhere (phase only) and energy-preserving — but NOT an identity filter.
    {
        const double f0 = 1000.0, q = 1.0;
        bool unity = true;
        for (double f : { 50.0, 200.0, 1000.0, 5000.0, 18000.0 })
            if (! approx (dbAt (FilterType::allPass, f0, 0.0, q, f, sr), 0.0, 1.0e-6)) unity = false;
        check (unity, "all-pass magnitude is unity across the band");
        check (sitsOnZeroLine (FilterType::allPass), "all-pass classified on the zero line");

        // Impulse response: energy ~= 1 (Parseval) but the response is spread, not a unit spike.
        auto band = makeBand (FilterType::allPass, f0, 0.0, q, 12, sr);
        band.reset();
        double energy = 0.0, tailEnergy = 0.0; float h0 = 0.0f;
        for (int n = 0; n < 16384; ++n)
        {
            const float x = (n == 0 ? 1.0f : 0.0f);
            const float h = band.processSample (0, x);
            energy += (double) h * h;
            if (n == 0) h0 = h; else tailEnergy += (double) h * h;
        }
        check (approx (energy, 1.0, 5.0e-3),       "all-pass preserves impulse energy", energy);
        check (! approx (h0, 1.0f, 1.0e-3) && tailEnergy > 1.0e-3,
               "all-pass is a real filter, not an identity", tailEnergy);
    }

    // ------------------------------------------------------------------ stagesForSlope
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
        check (stagesForSlope (FilterType::bell,      96) == 1, "non-cut type is always 1 stage");
        check (stagesForSlope (FilterType::tiltShelf, 96) == 1
            && stagesForSlope (FilterType::bandPass,  96) == 1
            && stagesForSlope (FilterType::allPass,   96) == 1, "new shapes are single-stage");
    }

    // ------------------------------------------------- Audio path: 8-stage cascade, no OOB
    // Drives the real BandDsp at the deepest slopes; would trap an out-of-bounds if the
    // stagesForSlope clamp regressed. Roll-off scales with stage count (identical-stage cascade
    // => dB adds), so attenuation at the cutoff is ~proportional to the number of stages.
    {
        const double fc = 1000.0, q = 0.707;
        auto hp12 = makeBand (FilterType::highPass, fc, 0.0, q, 12, sr);
        auto hp48 = makeBand (FilterType::highPass, fc, 0.0, q, 48, sr);
        auto hp96 = makeBand (FilterType::highPass, fc, 0.0, q, 96, sr);
        auto hpBw = makeBand (FilterType::highPass, fc, 0.0, q, kBrickwallSlope, sr);

        check (hp96.activeStages == 8, "slope 96 activates 8 cascade stages", hp96.activeStages);
        check (hpBw.activeStages == 8, "Brickwall activates the deepest cascade", hpBw.activeStages);

        const double a12 = sineGainDb (hp12, fc, sr); // attenuation at cutoff (negative dB)
        const double a48 = sineGainDb (hp48, fc, sr);
        const double a96 = sineGainDb (hp96, fc, sr);
        check (std::isfinite (a96) && a96 < a48 && a48 < a12, "deeper slope attenuates more", a96);
        check (approx (a96 / a12, 8.0, 0.4), "roll-off at cutoff scales x8 vs single stage", a96 / a12);
        check (approx (a96 / a48, 2.0, 0.2), "roll-off at cutoff scales x2 vs 48 dB/oct", a96 / a48);

        // Display == audio at the deepest slope: the drawn curve equals the measured cascade.
        const double shown = dbAt (FilterType::highPass, fc, 0.0, q, fc, sr, 96);
        check (approx (a96, shown, 1.0), "display matches audio for the 8-stage high-pass",
               std::abs (a96 - shown));
    }

    // ------------------------------------------------- Display == audio for the new shapes
    {
        // Tilt: measured gain through the cascade matches the drawn curve at a high and low probe.
        auto tilt = makeBand (FilterType::tiltShelf, 1000.0, 6.0, 0.707, 12, sr);
        const double th = sineGainDb (tilt, 5000.0, sr), thShown = dbAt (FilterType::tiltShelf, 1000.0, 6.0, 0.707, 5000.0, sr);
        const double tl = sineGainDb (tilt,  200.0, sr), tlShown = dbAt (FilterType::tiltShelf, 1000.0, 6.0, 0.707,  200.0, sr);
        check (approx (th, thShown, 0.5) && approx (tl, tlShown, 0.5),
               "display matches audio for the tilt shelf", std::abs (th - thShown));

        // Band-pass: 0 dB at the peak and matching the curve off-peak.
        auto bp = makeBand (FilterType::bandPass, 1000.0, 0.0, 1.0, 12, sr);
        const double bpk = sineGainDb (bp, 1000.0, sr), bOff = sineGainDb (bp, 500.0, sr);
        check (approx (bpk, 0.0, 0.3), "band-pass passes the centre frequency at 0 dB", bpk);
        check (approx (bOff, dbAt (FilterType::bandPass, 1000.0, 0.0, 1.0, 500.0, sr), 0.5),
               "display matches audio for the band-pass", bOff);
    }

    // ------------------------------------------------- Robustness near Nyquist (44.1 kHz, 20 kHz)
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
