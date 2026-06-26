# Prompt — Branch 1: Engine & parameter foundation (`feat/proq4-engine`)

> Paste everything below into a fresh Claude Code session pointed at the ZandersEQ repo.

---

You are working on **ZandersEQ**, a JUCE 8 / C++17 parametric EQ audio plugin (VST3/AU/
Standalone, built with CMake). We are reworking it to look and behave like **FabFilter Pro-Q 4**
while keeping the existing "neon" visual brand. This is a **5-branch parallel effort**; you are
**Branch 1, the foundation branch that everyone else depends on — merge yours first.**

**Before doing anything, read `docs/proq4-redesign/CONTRACT.md`** — it defines the frozen
interface (C1–C6) and the file-ownership matrix. You are the **sole writer** of:
`src/Parameters.h`, `src/PluginProcessor.cpp/.h`, `src/Presets.h`, the **`numBands` line only**
of `src/dsp/EqMath.h`, `CMakeLists.txt`, and the engine tests in `tests/`. Do **not** edit any
`src/gui/*` file, `Theme.h`, `NeonLookAndFeel`, `Biquad.h`, or the enum/math region of
`EqMath.h` (those belong to other branches).

## Setup

```bash
git fetch origin dev
git checkout -b feat/proq4-engine origin/dev
```

## Goal

Turn the fixed 6-band engine into a **dynamic 24-slot pool** (Pro-Q's add/remove model) and
become the single clearinghouse for all new parameters and CMake/test wiring, so the GUI and
DSP branches can build against a stable surface.

## Tasks

1. **Pool 6 → 24.** In `src/dsp/EqMath.h`, change `numBands` from 6 to 24 — **that line only**.
   All `std::array<…, numBands>` members across `PluginProcessor.h` resize automatically.
   Verify every loop bounded by `numBands` still behaves; update `defaultBands()` in
   `Parameters.h` so slots 0–5 keep today's shape and slots 6–23 are inert defaults.

2. **Per-band `active` flag.** Add an APVTS bool param `band{i}_active` (add `ids::active(int)`
   in `Parameters.h`). Default: slots **0–5 active**, **6–23 inactive** (so existing behavior is
   preserved). In the audio path (`updateBandCoeffsForBlock`), change the per-band gate from
   `on && (!anySolo || solo)` to `active && on && (!anySolo || solo)`.

3. **Dynamic-band API (C3).** Implement in `PluginProcessor.cpp/.h`, exactly these signatures:
   `activeBandCount()`, `isBandActive(int)`, `firstFreeSlot()`,
   `addBand(float freq, float gain, FilterType type)` (returns the slot or -1 if full),
   `removeBand(int slot)`. `addBand` picks `firstFreeSlot`, sets that slot's freq/gain/type,
   sets `active=true`, and selects it; `removeBand` clears `active`. **Both must be
   message-thread only, allocation-free, and wrapped in `recordUndoableEdit(...)`** so each is a
   single undo step. These replace the hand-rolled add logic the graph branch currently has.

4. **Fix a latent bug + cover `active`.** `forEachParamId` (top of `PluginProcessor.cpp`)
   currently omits `ids::dynDir` — A/B and undo silently drop per-band dynamic *direction*
   today. Add `dynDir` **and** the new `active` flag to that enumeration so undo/redo, A/B
   swap, and presets all capture them. (Otherwise add/remove won't be undoable and A/B breaks.)

5. **Second analyzer tap (for the graph's input-vs-output spectrum).** Add a second
   `AnalyzerFifo preEqAnalyzer` plus `getPreEqAnalyzerFifo()`, fed the **pre-EQ** (input) signal
   in `processBlock` (mono-summed, same pattern as the existing post-EQ `analyzer`). This keeps
   all FIFO/atomic plumbing in one branch so the RT-safety gate stays in one place — the graph
   branch will only *read* the two FIFOs.

6. **Declare all new global param IDs (C5)** in `Parameters.h` and add them to the layout:
   `gainScale` (0..200%, applied as an overall EQ gain — wire it into the output stage),
   `analyzerOn` (choice), `analyzerRange` (choice), `globalBypass` (bool — bypass the EQ when
   set), and `phaseMode` (choice {Zero Latency, Natural, Linear}). **Only "Zero Latency" needs
   to behave** (current minimum-phase path); Natural/Linear are declared-but-inert this round.
   Cache atomic pointers for any you read on the audio thread.

7. **State back-compat.** Confirm old 6-band `.zeqpreset` files and old DAW sessions load to
   exactly 6 active bands. `applyParams` already guards with `snapshot.hasProperty(id)`, so
   missing band6–23/`active` params fall back to defaults — verify the defaults give the right
   result and add a test (below).

8. **Own all `CMakeLists.txt` edits.** Branch 5 (DSP) will hand you `add_executable` /
   `add_test` lines for its new DSP test target — add them here so CMake has a single writer.

## Tests to add (in `tests/EngineTests.cpp`, the headless integration harness)

- `addBand` / `removeBand` / `activeBandCount` / `firstFreeSlot` / `isBandActive` behavior,
  including pool-full (`firstFreeSlot() == -1` after 24 adds) and that an inactive slot
  contributes no gain to the output.
- An undo/redo round-trip across an add **and** a remove (single step each).
- An A/B-swap round-trip that preserves `active` and the now-fixed `dyndir`.
- A state-load test: an old-format (6-band) XML loads to exactly 6 active bands.

## Constraints / done criteria

- Keep green: all `ctest` tests, **pluginval strictness 10** (note the param count jumps ~14×;
  verify IDs stay unique/stable and pluginval still passes), **RealtimeSanitizer** (the
  `active` gate and add/remove must add no allocation/lock on the audio thread), SonarCloud
  coverage (Parameters.h is excluded; cover the new processor logic).
- Don't touch GUI files, `Theme.h`, `Biquad.h`, or the enum/math region of `EqMath.h`.
- Build & test:
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
- Commit with clear messages and push:
  ```bash
  git push -u origin feat/proq4-engine
  ```
  (Retry with exponential backoff on network errors. Do **not** open a PR unless asked.)
