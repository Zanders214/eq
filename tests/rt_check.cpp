// Real-time-safety driver for clang RealtimeSanitizer (RTSan).
//
// Built only under -DZEQ_RT_SANITIZE=ON, where the engine's audio-thread DSP (processEq
// and everything it calls) is compiled with [[clang::nonblocking]]. This driver runs the
// REAL processBlock for ~2 s of callbacks so RTSan can prove that subtree never allocates,
// locks, or makes a syscall on the audio thread. A clean exit with no RTSan report is PASS;
// a violation prints the offending stack and aborts (RTSAN_OPTIONS=halt_on_error=1).
//
// ZandersEQ is an effect, not a synth (NEEDS_MIDI_INPUT=FALSE), so we feed a continuous
// sine through the main bus rather than MIDI notes (cf. the guide's synth driver). We enable
// a bell band WITH dynamic EQ (so the detector / envelope path runs, not just the static
// biquad path) and flip HQ on midway, so both the base-rate and the 2x-oversampled processEq
// paths are exercised. Mirrors the headless setup proven by tests/EngineTests.cpp.

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>

using namespace zeq;

namespace
{
    // Store a denormalised value straight into the atomic the engine reads (no message thread).
    void setP (ZandersEqAudioProcessor& p, const juce::String& id, float v)
    {
        if (auto* a = p.getApvts().getRawParameterValue (id))
            a->store (v);
    }
}

int main()
{
    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    ZandersEqAudioProcessor proc;

    // Band 0: a bell with dynamic EQ engaged. dynamics require a non-zero-line type (bell/shelf)
    // and the band enabled, or the detector path at updateBandCoeffsForBlock is skipped.
    setP (proc, ids::on (0),         1.0f);
    setP (proc, ids::type (0),       (float) (int) FilterType::bell);
    setP (proc, ids::freq (0),       1000.0f);
    setP (proc, ids::gain (0),       6.0f);
    setP (proc, ids::q (0),          1.0f);
    setP (proc, ids::dynOn (0),      1.0f);
    setP (proc, ids::dynThresh (0), -30.0f);
    setP (proc, ids::dynRange (0),  -12.0f);
    setP (proc, ids::dynAttack (0),  5.0f);
    setP (proc, ids::dynRelease (0), 50.0f);

    proc.prepareToPlay (sr, bs);

    const int nCh = juce::jmax (1, proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (nCh, bs);

    const int blocks = (int) (sr / bs) * 2;   // ~2 s of callbacks
    long long n = 0;
    for (int b = 0; b < blocks; ++b)
    {
        // Flip HQ on at the half-way point so the 2x-oversampled processEq path runs too.
        if (b == blocks / 2)
            setP (proc, ids::hq, 1.0f);

        for (int s = 0; s < bs; ++s, ++n)
        {
            const float x = 0.25f * (float) std::sin (2.0 * kPi * 220.0 * (double) n / sr);
            for (int c = 0; c < nCh; ++c)
                buffer.setSample (c, s, x);
        }

        juce::MidiBuffer midi;          // empty: this is an effect, not a synth
        proc.processBlock (buffer, midi);
    }

    std::printf ("PASS: %d processBlock calls (base + HQ), no RT-safety violations\n", blocks);
    return 0;
}
