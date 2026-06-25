// Performance micro-benchmark for the ZandersEQ audio engine.
//
// Built only under -DZEQ_BUILD_BENCH=ON. Uses nanobench to time the REAL processBlock and
// emits a github-action-benchmark "customSmallerIsBetter" JSON so perf.yml can track the
// per-block CPU cost over time and comment on regressions.
//
// Two tiers are reported so a regression points at the subsystem:
//   * base rate (HQ off)        — the core six-band EQ + one dynamic band
//   * HQ 2x (HQ on)             — the same chain through the 2x oversampler
// The headline number per tier is "DSP load %" = ns_per_block / (blockSize/sampleRate);
// under 100% means the chain is real-time capable on this runner.
//
// ANKERL_NANOBENCH_IMPLEMENT is defined in THIS translation unit only; nanobench is used
// header-only (the CMake target adds its include dir and does NOT link a nanobench library,
// which would duplicate these symbols).

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>

using namespace zeq;

namespace
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    void setP (ZandersEqAudioProcessor& p, const juce::String& id, float v)
    {
        if (auto* a = p.getApvts().getRawParameterValue (id))
            a->store (v);
    }

    // All six bells active + one dynamic band: a realistic worst-ish-case patch.
    void configure (ZandersEqAudioProcessor& p, bool hq)
    {
        for (int i = 0; i < numBands; ++i)
        {
            setP (p, ids::on (i),   1.0f);
            setP (p, ids::type (i), (float) (int) FilterType::bell);
            setP (p, ids::freq (i), 80.0f * std::pow (2.0f, (float) i * 1.4f));   // spread 80 Hz..~10 kHz
            setP (p, ids::gain (i), (i % 2 == 0) ? 4.0f : -4.0f);
            setP (p, ids::q (i),    1.2f);
        }
        setP (p, ids::dynOn (0),     1.0f);          // exercise the detector / envelope path
        setP (p, ids::dynThresh (0), -30.0f);
        setP (p, ids::dynRange (0),  -10.0f);
        setP (p, ids::hq,            hq ? 1.0f : 0.0f);
    }

    // Time the real processBlock at the given HQ setting; return median ns/block.
    double benchTier (ankerl::nanobench::Bench& bench, const char* name, bool hq)
    {
        ZandersEqAudioProcessor proc;
        configure (proc, hq);
        proc.prepareToPlay (sr, bs);

        const int nCh = juce::jmax (1, proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
        juce::AudioBuffer<float> buffer (nCh, bs);

        long long n = 0;
        auto fill = [&]
        {
            for (int s = 0; s < bs; ++s, ++n)
            {
                const float x = 0.25f * (float) std::sin (2.0 * kPi * 220.0 * (double) n / sr);
                for (int c = 0; c < nCh; ++c)
                    buffer.setSample (c, s, x);
            }
        };

        // Warm up: settle smoother ramps and the dynamics envelope before measuring.
        juce::MidiBuffer midi;
        for (int b = 0; b < 32; ++b) { fill(); proc.processBlock (buffer, midi); }

        bench.run (name, [&]
        {
            fill();
            juce::MidiBuffer m;
            proc.processBlock (buffer, m);
            ankerl::nanobench::doNotOptimizeAway (buffer.getReadPointer (0)[0]);
        });

        // GOTCHA: the Measure enum is nested in Result, not the namespace.
        return bench.results().back().median (ankerl::nanobench::Result::Measure::elapsed) * 1.0e9;
    }
}

int main (int argc, char** argv)
{
    ankerl::nanobench::Bench bench;
    bench.title ("ZandersEQ DSP @48k/512").unit ("block").warmup (20).minEpochIterations (200);

    const double nsBase = benchTier (bench, "processBlock @48k/512",     false);
    const double nsHq   = benchTier (bench, "processBlock HQ 2x @48k/512", true);

    const double budgetNs = (double) bs / sr * 1.0e9;        // 10667 ns @48k/512
    const double loadBase = nsBase / budgetNs * 100.0;
    const double loadHq   = nsHq   / budgetNs * 100.0;

    // github-action-benchmark "customSmallerIsBetter" schema.
    juce::String json;
    json << "[\n"
         << "  { \"name\": \"processBlock @48k/512\",      \"unit\": \"ns/block\", \"value\": " << juce::String (nsBase,  3) << " },\n"
         << "  { \"name\": \"DSP load @48k/512\",          \"unit\": \"%\",        \"value\": " << juce::String (loadBase, 3) << " },\n"
         << "  { \"name\": \"processBlock HQ 2x @48k/512\", \"unit\": \"ns/block\", \"value\": " << juce::String (nsHq,    3) << " },\n"
         << "  { \"name\": \"DSP load HQ 2x @48k/512\",     \"unit\": \"%\",        \"value\": " << juce::String (loadHq,   3) << " }\n"
         << "]\n";

    const juce::String out = (argc > 1) ? juce::String (argv[1]) : juce::String ("bench_result.json");
    juce::File::getCurrentWorkingDirectory().getChildFile (out).replaceWithText (json);

    std::printf ("base=%.0f ns (%.1f%% RT load)   hq=%.0f ns (%.1f%% RT load)\n",
                 nsBase, loadBase, nsHq, loadHq);
    return 0;
}
