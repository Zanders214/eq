#pragma once

// Real-time-safety annotation macro.
//
// ZEQ_RT_NONBLOCKING expands to [[clang::nonblocking]] ONLY under the RTSan build
// (the ZEQ_RT_SANITIZE CMake target defines the macro to the attribute on the command
// line). Everywhere else — MSVC/gcc release builds, the SonarCloud analysis build, the
// pure-DSP unit tests — it expands to nothing, so it has zero effect on the shipped code.
//
// Placed on the audio-thread DSP we own (processEq and below, the per-sample biquad math),
// it makes clang's RealtimeSanitizer verify that nothing inside those functions allocates,
// locks, or makes a syscall. It is deliberately NOT placed on processBlock itself, which
// calls framework code with an unavoidable lock/allocation surface (setLatencySamples,
// juce::dsp::Oversampling) — exactly the JUCE-internal boundary the annotation should sit
// inside of, not around (see PERFORMANCE_MONITORING_GUIDE / the juce::Synthesiser gotcha).
#ifndef ZEQ_RT_NONBLOCKING
#define ZEQ_RT_NONBLOCKING
#endif
