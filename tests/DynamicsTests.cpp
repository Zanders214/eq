// Unit tests for the dynamic-EQ math (pure, no JUCE):
//   - dynamicGainDb mapping (EqMath.h)
//   - the per-band detector band-pass + envelope follower (Biquad.h BandDsp)

#include "../src/dsp/Biquad.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace zeq;

namespace
{
    int failures = 0;
    void check (bool ok, const char* name, double detail = 0.0)
    {
        if (ok) std::printf ("  PASS  %s\n", name);
        else  { std::printf ("  FAIL  %s (%g)\n", name, detail); ++failures; }
    }
    bool approx (double a, double b, double eps = 1.0e-4) { return std::abs (a - b) <= eps; }

    // Drive a band's detector with a sine of given freq/amplitude for n samples.
    float runDetector (BandDsp& band, double freq, double amp, int n, double sr)
    {
        for (int i = 0; i < n; ++i)
            band.pushDetector ((float) (amp * std::sin (2.0 * kPi * freq * i / sr)));
        return band.env;
    }
}

int main()
{
    const double sr = 48000.0;
    std::printf ("Dynamics tests\n");

    // --- 1. dynamicGainDb mapping ------------------------------------------
    check (dynamicGainDb (-30.0, -24.0, -6.0) == 0.0, "below threshold -> 0");
    check (dynamicGainDb (-24.0, -24.0, -6.0) == 0.0, "at threshold -> 0");
    check (approx (dynamicGainDb (-20.0, -24.0, -6.0, 8.0), -3.0), "half knee -> half range");
    check (approx (dynamicGainDb (-16.0, -24.0, -6.0, 8.0), -6.0), "full knee -> full range");
    check (dynamicGainDb (0.0, -24.0, -6.0, 8.0) == -6.0, "well above -> clamps at range");
    check (dynamicGainDb (-16.0, -24.0, 6.0, 8.0) == 6.0, "positive range boosts");

    // --- 1b. Under direction: acts below the threshold instead of above -----
    check (dynamicGainDb (-10.0, -24.0, 6.0, kDynKnee, DynDirection::under) == 0.0, "under: above threshold -> 0");
    check (dynamicGainDb (-24.0, -24.0, 6.0, kDynKnee, DynDirection::under) == 0.0, "under: at threshold -> 0");
    check (approx (dynamicGainDb (-28.0, -24.0, 6.0, 8.0, DynDirection::under), 3.0), "under: half knee below -> half range");
    check (approx (dynamicGainDb (-32.0, -24.0, 6.0, 8.0, DynDirection::under), 6.0), "under: full knee below -> full range");
    check (dynamicGainDb (-50.0, -24.0, 6.0, 8.0, DynDirection::under) == 6.0, "under: well below -> clamps at range");
    check (dynamicGainDb (-32.0, -24.0, -6.0, 8.0, DynDirection::under) == -6.0, "under: negative range cuts quiet");

    // --- 2. detector frequency selectivity ---------------------------------
    {
        BandDsp inBand;  inBand.updateDetector (1000.0, 1.0, 5.0, 100.0, sr);
        BandDsp outBand; outBand.updateDetector (1000.0, 1.0, 5.0, 100.0, sr);
        const float inEnv  = runDetector (inBand,  1000.0, 0.5, 4800, sr); // on the band
        const float outEnv = runDetector (outBand,   60.0, 0.5, 4800, sr); // far below the band
        check (inEnv > 0.3f, "detector tracks an in-band tone", inEnv);
        check (outEnv < 0.1f, "detector rejects an out-of-band tone", outEnv);
        check (outEnv < inEnv * 0.4f, "in-band envelope >> out-of-band", outEnv / inEnv);
    }

    // --- 3. attack speed: shorter attack reaches higher in a fixed window ---
    {
        BandDsp fast; fast.updateDetector (1000.0, 1.0, 1.0,  500.0, sr);  // 1 ms attack
        BandDsp slow; slow.updateDetector (1000.0, 1.0, 50.0, 500.0, sr);  // 50 ms attack
        const float fEnv = runDetector (fast, 1000.0, 0.5, 480, sr);  // 10 ms of tone
        const float sEnv = runDetector (slow, 1000.0, 0.5, 480, sr);
        check (fEnv > sEnv, "shorter attack rises faster", fEnv - sEnv);
    }

    // --- 4. release: envelope decays after the tone stops ------------------
    {
        BandDsp band; band.updateDetector (1000.0, 1.0, 5.0, 50.0, sr);
        const float peak = runDetector (band, 1000.0, 0.5, 4800, sr);
        for (int i = 0; i < 4800; ++i) band.pushDetector (0.0f);          // silence
        check (band.env < peak * 0.2f, "envelope releases toward silence", band.env);
    }

    // --- 5. non-finite guard: a diverged cascade never escapes as inf/NaN ----
    // A deep (up to 8-stage) high-Q cut can overflow float to inf in practice; processSample
    // must flush the channel's cascade and recover at silence so the plugin never emits a
    // non-finite sample (which crashes hosts / pluginval's fuzzer).
    {
        BandDsp band;
        band.active = true;
        band.activeStages = 1;
        band.stages[0][0].z1 = std::numeric_limits<float>::infinity();   // poison the state
        const float y = band.processSample (0, 0.25f);
        check (std::isfinite (y) && y == 0.0f, "non-finite cascade output is flushed to silence", y);
        const float y2 = band.processSample (0, 0.25f);                  // state was reset
        check (std::isfinite (y2), "band recovers to finite output after a flush", y2);
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
