// Unit tests for per-band channel-lane routing (pure, no JUCE).
// Exercises the shipping `applyBand` primitive from src/dsp/Biquad.h so the test
// proves the real audio path, not a copy.

#include "../src/dsp/Biquad.h"

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

    BandDsp makeBell (double sr)
    {
        BandDsp b;
        b.active = true;
        b.updateCoeffs (FilterType::bell, 1000.0, 6.0, 1.0, 12, sr);
        return b;
    }
}

int main()
{
    const double sr = 48000.0;
    std::printf ("ChannelRouting tests\n");

    // 1. Second lane filters b, leaves a untouched.
    {
        auto band = makeBell (sr);
        double maxA = 0.0; bool bChanged = false;
        for (int n = 0; n < 64; ++n)
        {
            float a = (n == 0 ? 1.0f : 0.0f), aIn = a;
            float b = (n == 0 ? 1.0f : 0.0f), bIn = b;
            applyBand (band, 2, a, b);
            maxA = std::max (maxA, (double) std::abs (a - aIn));
            if (std::abs (b - bIn) > 1.0e-6f) bChanged = true;
        }
        check (maxA < 1.0e-9, "lane=Second leaves first lane untouched", maxA);
        check (bChanged, "lane=Second filters second lane");
    }

    // 2. First lane filters a, leaves b untouched.
    {
        auto band = makeBell (sr);
        double maxB = 0.0; bool aChanged = false;
        for (int n = 0; n < 64; ++n)
        {
            float a = (n == 0 ? 1.0f : 0.0f), aIn = a;
            float b = (n == 0 ? 1.0f : 0.0f), bIn = b;
            applyBand (band, 1, a, b);
            maxB = std::max (maxB, (double) std::abs (b - bIn));
            if (std::abs (a - aIn) > 1.0e-6f) aChanged = true;
        }
        check (maxB < 1.0e-9, "lane=First leaves second lane untouched", maxB);
        check (aChanged, "lane=First filters first lane");
    }

    // 3. Both lane == independent per-channel filtering (legacy behaviour).
    {
        auto band = makeBell (sr);
        auto ref  = makeBell (sr);
        double maxDiff = 0.0;
        for (int n = 0; n < 128; ++n)
        {
            float a = (n == 0 ? 1.0f : 0.0f), b = (n == 0 ? 0.5f : 0.0f);
            float ra = a, rb = b;
            applyBand (band, 0, a, b);
            const float refA = ref.processSample (0, ra);
            const float refB = ref.processSample (1, rb);
            maxDiff = std::max (maxDiff, (double) std::max (std::abs (a - refA), std::abs (b - refB)));
        }
        check (maxDiff < 1.0e-9, "lane=Both matches independent L/R filtering", maxDiff);
    }

    // 4. M/S isolation: a Mid (first-lane) band leaves a pure-side signal intact.
    {
        auto band = makeBell (sr);
        double maxDiff = 0.0;
        for (int n = 0; n < 128; ++n)
        {
            const float L = (n == 0 ? 1.0f : 0.0f), R = (n == 0 ? -1.0f : 0.0f); // pure side
            float a = 0.5f * (L + R), b = 0.5f * (L - R);                        // mid=0, side=L
            applyBand (band, 1, a, b);                                           // filter mid (=0)
            const float outL = a + b, outR = a - b;
            maxDiff = std::max (maxDiff, (double) std::max (std::abs (outL - L), std::abs (outR - R)));
        }
        check (maxDiff < 1.0e-6, "Mid band leaves a pure-side signal intact", maxDiff);
    }

    // 4b. Mid band on a pure-mid signal alters it and keeps L==R (no side leak).
    {
        auto band = makeBell (sr);
        double maxLR = 0.0; bool changed = false;
        for (int n = 0; n < 128; ++n)
        {
            const float L = (n == 0 ? 1.0f : 0.0f), R = (n == 0 ? 1.0f : 0.0f); // pure mid
            float a = 0.5f * (L + R), b = 0.5f * (L - R);                       // mid=L, side=0
            applyBand (band, 1, a, b);
            const float outL = a + b, outR = a - b;
            maxLR = std::max (maxLR, (double) std::abs (outL - outR));
            if (std::abs (outL - L) > 1.0e-6f) changed = true;
        }
        check (maxLR < 1.0e-9, "Mid band keeps L==R (no side leak)", maxLR);
        check (changed, "Mid band alters the mid signal");
    }

    // 5. Encode/decode identity.
    {
        double maxErr = 0.0;
        for (int n = 0; n < 100; ++n)
        {
            const float L = std::sin (n * 0.30f), R = std::cos (n * 0.17f);
            const float a = 0.5f * (L + R), b = 0.5f * (L - R);
            const float dL = a + b, dR = a - b;
            maxErr = std::max (maxErr, (double) std::max (std::abs (dL - L), std::abs (dR - R)));
        }
        check (maxErr < 1.0e-6, "encode/decode round-trips", maxErr);
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
