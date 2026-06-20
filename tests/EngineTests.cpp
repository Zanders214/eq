// Integration tests for the REAL audio engine.
//
// Unlike the other suites (which exercise the pure DSP primitives), this one drives
// the actual zeq::ZandersEqAudioProcessor::processBlock — the literal shipping path,
// including the output stage, auto-gain and M/S orchestration that live in
// processBlock above the private processEq. It feeds signals through the processor and
// asserts on the output: band gains match the design, the notch nulls, cut slopes
// cascade, auto-gain matches loudness, dynamics duck, M/S routing is isolated, and the
// engine stays finite under load. The "display can't lie" invariant is checked too by
// comparing the measured time-domain gain against bandMagnitudeDb() (the curve math).
//
// Parameters are set deterministically by storing denormalised values straight into the
// atomics the engine reads (apvts.getRawParameterValue(id)->store(v)); the editor is
// never created, so the test runs headless with no message thread or display.

#include "PluginProcessor.h"
#include "gui/Theme.h"   // noteName()

#include <cmath>
#include <cstdio>
#include <random>

using namespace zeq;

namespace
{
    int failures = 0;
    void check (bool ok, const char* name, double detail = 0.0)
    {
        if (ok) std::printf ("  PASS  %s\n", name);
        else  { std::printf ("  FAIL  %s (%g)\n", name, detail); ++failures; }
    }
    bool within (double a, double b, double eps) { return std::abs (a - b) <= eps; }

    constexpr double sr        = 48000.0;
    constexpr int    blockSize = 512;
    constexpr int    kSettle   = 60;   // warm-up blocks before measuring (smoothers/ramps)
    constexpr int    kMeasure  = 40;   // measurement-window blocks

    // Store a denormalised value straight into the atomic the engine reads.
    void setP (ZandersEqAudioProcessor& p, const juce::String& id, float v)
    {
        if (auto* a = p.getApvts().getRawParameterValue (id))
            a->store (v);
    }

    // Reset every parameter to a fully-flat baseline: all bands off, unity output,
    // stereo, no dynamics / auto-gain / HQ. Tests then tweak only what they need.
    void flatBaseline (ZandersEqAudioProcessor& p)
    {
        for (int i = 0; i < numBands; ++i)
        {
            setP (p, ids::on (i),        0.0f);
            setP (p, ids::solo (i),      0.0f);
            setP (p, ids::type (i),      (float) (int) FilterType::bell);
            setP (p, ids::freq (i),      1000.0f);
            setP (p, ids::gain (i),      0.0f);
            setP (p, ids::q (i),         1.0f);
            setP (p, ids::slope (i),     0.0f);     // choice index 0 = 12 dB/oct
            setP (p, ids::channel (i),   0.0f);     // Both
            setP (p, ids::dynOn (i),     0.0f);
            setP (p, ids::dynThresh (i), -24.0f);
            setP (p, ids::dynRange (i),  0.0f);
            setP (p, ids::dynAttack (i), 10.0f);
            setP (p, ids::dynRelease (i),150.0f);
        }
        setP (p, ids::output,      0.0f);
        setP (p, ids::mode,        0.0f);
        setP (p, ids::hq,          0.0f);
        setP (p, ids::autogain,    0.0f);
        setP (p, ids::matchamount, 100.0f);
    }

    int numChans (ZandersEqAudioProcessor& p)
    {
        return juce::jmax (1, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    }

    // Drive a continuous sine (identical on every channel) through the real processBlock
    // and return the steady-state RMS gain in dB, measured over a window after warm-up.
    double runSineGainDb (ZandersEqAudioProcessor& p, double freq, double amp,
                          int settle = kSettle, int measure = kMeasure)
    {
        const int nCh = numChans (p);
        juce::AudioBuffer<float> buf (nCh, blockSize);
        juce::MidiBuffer midi;

        double inSS = 0.0, outSS = 0.0;
        long long n = 0;
        for (int b = 0; b < settle + measure; ++b)
        {
            const bool meas = b >= settle;
            for (int s = 0; s < blockSize; ++s, ++n)
            {
                const float x = (float) (amp * std::sin (2.0 * kPi * freq * (double) n / sr));
                for (int c = 0; c < nCh; ++c) buf.setSample (c, s, x);
                if (meas) inSS += (double) nCh * (double) x * x;
            }
            p.processBlock (buf, midi);
            if (meas)
                for (int c = 0; c < nCh; ++c)
                {
                    const float* d = buf.getReadPointer (c);
                    for (int s = 0; s < blockSize; ++s) outSS += (double) d[s] * d[s];
                }
        }
        return 20.0 * std::log10 ((std::sqrt (outSS) + 1.0e-12) / (std::sqrt (inSS) + 1.0e-12));
    }

    // Max sample-wise deviation of the output from (scale * input), input identical on
    // every channel. Used for the flat-identity and output-gain checks.
    double maxDevVsScaledInput (ZandersEqAudioProcessor& p, double freq, double amp,
                                double scale, int settle = kSettle, int measure = kMeasure)
    {
        const int nCh = numChans (p);
        juce::AudioBuffer<float> buf (nCh, blockSize);
        juce::MidiBuffer midi;

        double maxDev = 0.0;
        long long n = 0;
        for (int b = 0; b < settle + measure; ++b)
        {
            const bool meas = b >= settle;
            const long long nStart = n;
            for (int s = 0; s < blockSize; ++s, ++n)
            {
                const float x = (float) (amp * std::sin (2.0 * kPi * freq * (double) n / sr));
                for (int c = 0; c < nCh; ++c) buf.setSample (c, s, x);
            }
            p.processBlock (buf, midi);
            if (meas)
                for (int s = 0; s < blockSize; ++s)
                {
                    const double x = amp * std::sin (2.0 * kPi * freq * (double) (nStart + s) / sr);
                    for (int c = 0; c < nCh; ++c)
                        maxDev = std::max (maxDev, std::abs ((double) buf.getSample (c, s) - scale * x));
                }
        }
        return maxDev;
    }

    // M/S: drive a pure-side signal (L=x, R=-x) and return the max deviation of the
    // output from the untouched input — a Mid-lane band must leave the side intact.
    double msSideDev (ZandersEqAudioProcessor& p, double freq, double amp,
                      int settle = kSettle, int measure = kMeasure)
    {
        const int nCh = numChans (p);
        juce::AudioBuffer<float> buf (nCh, blockSize);
        juce::MidiBuffer midi;

        double maxDev = 0.0;
        long long n = 0;
        for (int b = 0; b < settle + measure; ++b)
        {
            const bool meas = b >= settle;
            const long long nStart = n;
            for (int s = 0; s < blockSize; ++s, ++n)
            {
                const float x = (float) (amp * std::sin (2.0 * kPi * freq * (double) n / sr));
                buf.setSample (0, s, x);
                if (nCh > 1) buf.setSample (1, s, -x);
            }
            p.processBlock (buf, midi);
            if (meas)
                for (int s = 0; s < blockSize; ++s)
                {
                    const double x = amp * std::sin (2.0 * kPi * freq * (double) (nStart + s) / sr);
                    maxDev = std::max (maxDev, std::abs ((double) buf.getSample (0, s) - x));
                    if (nCh > 1) maxDev = std::max (maxDev, std::abs ((double) buf.getSample (1, s) + x));
                }
        }
        return maxDev;
    }

    // M/S: drive a pure-mid signal (L=R=x) and report both the channel imbalance
    // (max |L-R|, should stay ~0) and the mid-channel gain in dB.
    struct MidResult { double maxLR; double gainDb; };
    MidResult msMid (ZandersEqAudioProcessor& p, double freq, double amp,
                     int settle = kSettle, int measure = kMeasure)
    {
        const int nCh = numChans (p);
        juce::AudioBuffer<float> buf (nCh, blockSize);
        juce::MidiBuffer midi;

        double maxLR = 0.0, inSS = 0.0, outSS = 0.0;
        long long n = 0;
        for (int b = 0; b < settle + measure; ++b)
        {
            const bool meas = b >= settle;
            for (int s = 0; s < blockSize; ++s, ++n)
            {
                const float x = (float) (amp * std::sin (2.0 * kPi * freq * (double) n / sr));
                for (int c = 0; c < nCh; ++c) buf.setSample (c, s, x);
                if (meas) inSS += (double) x * x;
            }
            p.processBlock (buf, midi);
            if (meas)
                for (int s = 0; s < blockSize; ++s)
                {
                    const double l = buf.getSample (0, s);
                    const double r = nCh > 1 ? buf.getSample (1, s) : l;
                    maxLR = std::max (maxLR, std::abs (l - r));
                    outSS += l * l;
                }
        }
        return { maxLR, 20.0 * std::log10 ((std::sqrt (outSS) + 1.0e-12) / (std::sqrt (inSS) + 1.0e-12)) };
    }

    // Push white noise through a busy multi-band config; return the largest |sample|
    // seen (or +inf if any output is non-finite).
    double maxAbsUnderNoise (ZandersEqAudioProcessor& p, int blocks)
    {
        const int nCh = numChans (p);
        juce::AudioBuffer<float> buf (nCh, blockSize);
        juce::MidiBuffer midi;
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> dist (-0.5f, 0.5f);

        double peak = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int s = 0; s < blockSize; ++s)
                for (int c = 0; c < nCh; ++c) buf.setSample (c, s, dist (rng));
            p.processBlock (buf, midi);
            for (int c = 0; c < nCh; ++c)
            {
                const float* d = buf.getReadPointer (c);
                for (int s = 0; s < blockSize; ++s)
                {
                    if (! std::isfinite (d[s])) return std::numeric_limits<double>::infinity();
                    peak = std::max (peak, std::abs ((double) d[s]));
                }
            }
        }
        return peak;
    }
}

int main()
{
    std::printf ("Engine integration tests\n");

    // --- 1. Flat / bypass identity -----------------------------------------
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        p.prepareToPlay (sr, blockSize);
        const double dev = maxDevVsScaledInput (p, 1000.0, 0.25, 1.0);
        check (dev < 1.0e-5, "all bands off -> output == input", dev);
    }

    // --- 2. Bell boost & cut match the designed gain (and bandMagnitudeDb) --
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::on (0), 1.0f);
        setP (p, ids::type (0), (float) (int) FilterType::bell);
        setP (p, ids::freq (0), 1000.0f);
        setP (p, ids::gain (0), 6.0f);
        setP (p, ids::q (0), 1.0f);
        p.prepareToPlay (sr, blockSize);

        const double measured = runSineGainDb (p, 1000.0, 0.25);
        const double curve = bandMagnitudeDb (FilterType::bell, 1000.0, 6.0, 1.0, 12, true, 1000.0, sr);
        check (within (measured, 6.0, 0.5),     "bell +6 dB boosts a centre sine by 6 dB", measured);
        check (within (measured, curve, 0.5),   "measured bell gain == on-screen curve",   measured - curve);
    }
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::on (0), 1.0f);
        setP (p, ids::freq (0), 1000.0f);
        setP (p, ids::gain (0), -6.0f);
        setP (p, ids::q (0), 1.0f);
        p.prepareToPlay (sr, blockSize);
        const double measured = runSineGainDb (p, 1000.0, 0.25);
        check (within (measured, -6.0, 0.5), "bell -6 dB cuts a centre sine by 6 dB", measured);
    }

    // --- 3. Notch nulls a tone at its centre -------------------------------
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::on (0), 1.0f);
        setP (p, ids::type (0), (float) (int) FilterType::notch);
        setP (p, ids::freq (0), 1000.0f);
        setP (p, ids::q (0), 2.0f);
        p.prepareToPlay (sr, blockSize);
        const double measured = runSineGainDb (p, 1000.0, 0.25, 100, 40);
        check (measured < -30.0, "notch nulls a centre tone (< -30 dB)", measured);
    }

    // --- 4. High-pass slope + cascade (dB scales with stage count) ---------
    {
        const double probe = 250.0;   // ~2 octaves below the 1 kHz cutoff
        double m24 = 0.0, m48 = 0.0, pass = 0.0;
        {
            ZandersEqAudioProcessor p;
            flatBaseline (p);
            setP (p, ids::on (0), 1.0f);
            setP (p, ids::type (0), (float) (int) FilterType::highPass);
            setP (p, ids::freq (0), 1000.0f);
            setP (p, ids::q (0), 0.71f);
            setP (p, ids::slope (0), 1.0f);          // index 1 = 24 dB/oct (2 stages)
            p.prepareToPlay (sr, blockSize);
            m24  = runSineGainDb (p, probe, 0.25);
            pass = runSineGainDb (p, 4000.0, 0.25);  // ~2 octaves above -> passband
        }
        {
            ZandersEqAudioProcessor p;
            flatBaseline (p);
            setP (p, ids::on (0), 1.0f);
            setP (p, ids::type (0), (float) (int) FilterType::highPass);
            setP (p, ids::freq (0), 1000.0f);
            setP (p, ids::q (0), 0.71f);
            setP (p, ids::slope (0), 2.0f);          // index 2 = 48 dB/oct (4 stages)
            p.prepareToPlay (sr, blockSize);
            m48 = runSineGainDb (p, probe, 0.25);
        }
        const double curve24 = bandMagnitudeDb (FilterType::highPass, 1000.0, 0.0, 0.71, 24, true, probe, sr);
        check (within (pass, 0.0, 1.0),        "high-pass passband is ~unity", pass);
        check (m24 < -10.0,                    "high-pass attenuates the stopband", m24);
        check (within (m24, curve24, 1.0),     "measured HP stopband == on-screen curve", m24 - curve24);
        check (within (m48 / m24, 2.0, 0.15),  "48 dB/oct attenuates ~2x the dB of 24 dB/oct", m48 / m24);
    }

    // --- 5. Output gain ----------------------------------------------------
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::output, -6.0f);            // all bands off; pure output trim
        p.prepareToPlay (sr, blockSize);
        const double measured = runSineGainDb (p, 1000.0, 0.25);
        const double dev = maxDevVsScaledInput (p, 1000.0, 0.25, juce::Decibels::decibelsToGain (-6.0));
        check (within (measured, -6.0, 0.3), "output -6 dB scales the signal by 0.5", measured);
        check (dev < 1.0e-4,                  "output trim is a clean per-sample scale", dev);
    }

    // --- 6. Auto-gain matches loudness ------------------------------------
    {
        // off: a +10 dB bell at the tone boosts it ~+10 dB
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::on (0), 1.0f);
        setP (p, ids::freq (0), 1000.0f);
        setP (p, ids::gain (0), 10.0f);
        setP (p, ids::q (0), 1.0f);
        p.prepareToPlay (sr, blockSize);
        const double off = runSineGainDb (p, 1000.0, 0.2);
        check (within (off, 10.0, 0.6), "auto-gain off: +10 dB bell boosts ~+10 dB", off);

        // on: the same boost is trimmed back so output loudness ~= input
        ZandersEqAudioProcessor p2;
        flatBaseline (p2);
        setP (p2, ids::on (0), 1.0f);
        setP (p2, ids::freq (0), 1000.0f);
        setP (p2, ids::gain (0), 10.0f);
        setP (p2, ids::q (0), 1.0f);
        setP (p2, ids::autogain, 1.0f);
        p2.prepareToPlay (sr, blockSize);
        const double on = runSineGainDb (p2, 1000.0, 0.2);
        check (std::abs (on) < 1.5, "auto-gain on: loudness matched back to ~0 dB", on);
    }

    // --- 7. Dynamic EQ ducks when driven loud ------------------------------
    {
        auto configure = [] (ZandersEqAudioProcessor& p)
        {
            flatBaseline (p);
            setP (p, ids::on (0), 1.0f);
            setP (p, ids::type (0), (float) (int) FilterType::bell);
            setP (p, ids::freq (0), 1000.0f);
            setP (p, ids::gain (0), 0.0f);
            setP (p, ids::q (0), 1.0f);
            setP (p, ids::dynOn (0), 1.0f);
            setP (p, ids::dynThresh (0), -30.0f);
            setP (p, ids::dynRange (0), -12.0f);   // cut when loud
            setP (p, ids::dynAttack (0), 5.0f);
            setP (p, ids::dynRelease (0), 50.0f);
        };
        double loud = 0.0, quiet = 0.0;
        { ZandersEqAudioProcessor p; configure (p); p.prepareToPlay (sr, blockSize); loud  = runSineGainDb (p, 1000.0, 0.5); }
        { ZandersEqAudioProcessor p; configure (p); p.prepareToPlay (sr, blockSize); quiet = runSineGainDb (p, 1000.0, 0.01); }
        check (quiet > -1.5,             "dynamic band is flat below threshold", quiet);
        check (loud  < -6.0,             "dynamic band ducks when driven loud", loud);
        check (quiet - loud > 6.0,       "loud drive ducks >6 dB below the quiet case", quiet - loud);
    }

    // --- 8. Mid/Side routing is isolated -----------------------------------
    {
        // A Mid-lane band must leave a pure-side signal untouched.
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::mode, 1.0f);                 // Mid/Side domain
        setP (p, ids::on (0), 1.0f);
        setP (p, ids::freq (0), 1000.0f);
        setP (p, ids::gain (0), 12.0f);
        setP (p, ids::q (0), 1.0f);
        setP (p, ids::channel (0), 1.0f);          // Left/Mid = first lane = Mid
        p.prepareToPlay (sr, blockSize);
        const double sideDev = msSideDev (p, 1000.0, 0.25);
        check (sideDev < 1.0e-4, "Mid band leaves a pure-side signal intact", sideDev);

        // The same band on a pure-mid signal boosts it and keeps L==R.
        ZandersEqAudioProcessor p2;
        flatBaseline (p2);
        setP (p2, ids::mode, 1.0f);
        setP (p2, ids::on (0), 1.0f);
        setP (p2, ids::freq (0), 1000.0f);
        setP (p2, ids::gain (0), 12.0f);
        setP (p2, ids::q (0), 1.0f);
        setP (p2, ids::channel (0), 1.0f);
        p2.prepareToPlay (sr, blockSize);
        const auto mid = msMid (p2, 1000.0, 0.25);
        check (mid.maxLR < 1.0e-5,        "Mid band keeps L==R (no side leak)", mid.maxLR);
        check (mid.gainDb > 6.0,          "Mid band boosts the mid signal", mid.gainDb);
    }

    // --- 9. Stability: busy config + HQ + auto-gain stays finite -----------
    {
        ZandersEqAudioProcessor p;
        flatBaseline (p);
        setP (p, ids::type (0), (float) (int) FilterType::highPass); setP (p, ids::freq (0), 40.0f);   setP (p, ids::slope (0), 2.0f); setP (p, ids::on (0), 1.0f);
        setP (p, ids::type (1), (float) (int) FilterType::bell);     setP (p, ids::freq (1), 200.0f);  setP (p, ids::gain (1), 12.0f); setP (p, ids::on (1), 1.0f);
        setP (p, ids::type (2), (float) (int) FilterType::bell);     setP (p, ids::freq (2), 1000.0f); setP (p, ids::gain (2), -12.0f);setP (p, ids::on (2), 1.0f);
        setP (p, ids::type (3), (float) (int) FilterType::highShelf);setP (p, ids::freq (3), 6000.0f); setP (p, ids::gain (3), 6.0f);  setP (p, ids::on (3), 1.0f);
        setP (p, ids::type (4), (float) (int) FilterType::lowPass);  setP (p, ids::freq (4), 16000.0f);setP (p, ids::slope (4), 1.0f); setP (p, ids::on (4), 1.0f);
        setP (p, ids::autogain, 1.0f);
        setP (p, ids::hq, 1.0f);                    // exercise the oversampling branch
        p.prepareToPlay (sr, blockSize);
        const double peak = maxAbsUnderNoise (p, 200);
        check (std::isfinite (peak) && peak < 100.0, "busy config (HQ + auto-gain) stays finite", peak);
    }

    // --- 10. Undo / redo of parameter edits --------------------------------
    // Drive edits through the parameter objects (setValueNotifyingHost) so the
    // snapshot's getValue() reads stay consistent with the atomics processBlock sees.
    {
        ZandersEqAudioProcessor p;
        auto setNorm = [&] (const juce::String& id, float real)
        {
            if (auto* pp = p.getApvts().getParameter (id))
                pp->setValueNotifyingHost (pp->convertTo0to1 (real));
        };
        auto raw = [&] (const juce::String& id) { return (double) p.getApvts().getRawParameterValue (id)->load(); };

        setNorm (ids::gain (0), 0.0f);
        check (! p.canUndo(), "no undo history at start");

        p.beginUndoTransaction();
        setNorm (ids::gain (0), 6.0f);
        p.commitUndoTransaction();
        check (p.canUndo(),                      "an edit creates an undo entry");
        check (within (raw (ids::gain (0)), 6.0, 0.02), "edit applied", raw (ids::gain (0)));

        p.undo();
        check (within (raw (ids::gain (0)), 0.0, 0.02), "undo restores the old value", raw (ids::gain (0)));
        check (p.canRedo(),                      "undo enables redo");
        p.redo();
        check (within (raw (ids::gain (0)), 6.0, 0.02), "redo re-applies the value", raw (ids::gain (0)));

        // no-op gesture adds nothing
        ZandersEqAudioProcessor p2;
        p2.beginUndoTransaction();
        p2.commitUndoTransaction();
        check (! p2.canUndo(), "a no-op gesture leaves history empty");

        // a fresh edit clears the redo stack
        p.beginUndoTransaction();
        setNorm (ids::q (0), 4.0f);
        p.commitUndoTransaction();
        check (! p.canRedo(), "a fresh edit clears redo");

        // a multi-parameter edit undoes in a single step
        const double f0 = raw (ids::freq (0));
        p.recordUndoableEdit ([&]
        {
            setNorm (ids::freq (0), 777.0f);
            setNorm (ids::on (1), 1.0f);
        });
        check (within (raw (ids::freq (0)), 777.0, 0.5), "multi-edit applied", raw (ids::freq (0)));
        p.undo();
        check (within (raw (ids::freq (0)), f0, 0.5), "multi-param edit undoes in one step", raw (ids::freq (0)));
    }

    // --- 11. Musical-note readout ------------------------------------------
    {
        check (theme::noteName (440.0f)  == "A4",  "noteName(440) == A4");
        check (theme::noteName (261.63f) == "C4",  "noteName(261.63) == C4");
        check (theme::noteName (1000.0f) == "B5",  "noteName(1000) == B5", 0.0);
        check (theme::noteName (27.5f)   == "A0",  "noteName(27.5) == A0");
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
