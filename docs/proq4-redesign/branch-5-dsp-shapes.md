# Prompt — Branch 5: DSP filter shapes & steep slopes (`feat/proq4-dsp-shapes`)

> Paste everything below into a fresh Claude Code session pointed at the ZandersEQ repo.

---

You are working on **ZandersEQ**, a JUCE 8 / C++17 parametric EQ audio plugin built with CMake.
We are reworking it toward **FabFilter Pro-Q 4** parity. This is a **5-branch parallel effort**;
you are **Branch 5 (DSP)**, which runs in parallel with the engine branch and merges right after
it.

**Before doing anything, read `docs/proq4-redesign/CONTRACT.md`** (frozen interface C1–C6 +
ownership matrix). You are the **sole writer** of the **enum + math region** of
`src/dsp/EqMath.h`, all of `src/dsp/Biquad.h`, and your DSP tests. You must **not** touch the
`numBands` line of `EqMath.h` (Branch 1 owns it), nor `Parameters.h`, `PluginProcessor.*`, or
any GUI file.

## Setup

```bash
git fetch origin dev
git checkout -b feat/proq4-dsp-shapes origin/dev
```

## Goal

Add Pro-Q 4's missing filter shapes and steeper slopes to the existing RBJ-biquad engine,
keeping the same coefficient math that both the audio path **and** the on-screen curve share
(so the display can never lie), and keeping the real-time path allocation-free.

## Tasks

1. **New filter shapes (APPEND-ONLY, see C2).** Append `tiltShelf, bandPass, allPass` to
   `enum class FilterType` **after index 5** (order is frozen by the contract):
   ```cpp
   enum class FilterType { highPass=0, lowShelf, bell, notch, highShelf, lowPass,
                           tiltShelf, bandPass, allPass };
   ```
   Implement their coefficients in `makeCoeffs` and their magnitude response in
   `bandMagnitudeDb`:
   - **Tilt Shelf:** pivots around the band frequency — boosts one side of the spectrum by
     `+gain` and cuts the other by `-gain` (effectively a low-shelf and high-shelf pair, or a
     first-order tilt). Has gain.
   - **Band-Pass:** RBJ constant-skirt (or 0 dB peak) band-pass; sits on the zero line (no gain).
   - **All-Pass:** unity magnitude, phase rotation only; sits on the zero line.
   Update the classification predicates (`isCut`, `sitsOnZeroLine`) so `bandPass`/`allPass`
   report `sitsOnZeroLine() == true` (the graph draws no gain handle) and `tiltShelf` reports
   it has gain. **Any new shape must be classified, or the graph will mis-draw it.**

2. **Steeper slopes (APPEND-ONLY).** The slope choice list gains `72 dB/oct`, `96 dB/oct`,
   `Brickwall` (Branch 1 declares the strings; you implement the math). Update `stagesForSlope`
   to map 72 → 6 stages and 96 → 8 stages (12 dB/oct per biquad). Raise **`maxCascade` in
   `Biquad.h` from 4 to 8** — this resizes `BandDsp`'s cascade arrays (pure Branch-5 change).
   For **Brickwall**, pick a representation (a sentinel slope value handled by a dedicated
   very-steep path) and document it; keep it allocation-free.

3. **Tests.** Add pure-DSP unit tests (no JUCE) for each new shape and slope:
   - tilt-shelf crossover behaves (sign flips across the pivot),
   - band-pass peak is at the band frequency and rolls off both sides,
   - all-pass magnitude ≈ unity across the band (phase changes, magnitude doesn't),
   - `stagesForSlope` returns 6/8 for 72/96 and the measured roll-off scales accordingly.
   Put these in a new test executable (e.g. `tests/ShapeTests.cpp`) **or** extend
   `tests/MatchFitTests.cpp`. **Hand the `add_executable` + `add_test` lines to Branch 1** (it
   is the sole writer of `CMakeLists.txt`) — include them in your PR description / a note so B1
   can add them; do not edit `CMakeLists.txt` yourself.

## Constraints / done criteria

- **Real-time safety is critical.** `makeCoeffs`, `stagesForSlope`, and the biquad cascade run
  on the audio thread under `[[clang::nonblocking]]`; the new shapes, the `maxCascade=8` bump,
  and the Brickwall path must not allocate, lock, or branch into anything that does. Keep RTSan
  green.
- The new shapes/slopes increase per-band cost; **perf_bench will regress** (8-stage cascades
  vs 4). That gate is soft — note the regression in your commit message as an informed tradeoff.
- Keep green: `MatchFitTests`, `DynamicsTests`, `ChannelRoutingTests`, `EngineTests`, RTSan.
- Edit **only** the enum/math region of `EqMath.h` (never the `numBands` line) and `Biquad.h`.
- Build & test:
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
- Commit and push:
  ```bash
  git push -u origin feat/proq4-dsp-shapes
  ```
  (Retry with exponential backoff on network errors. No PR unless asked.)
