# ZandersEQ → FabFilter Pro-Q 4 redesign — 5 parallel work-streams

This folder contains **5 self-contained prompts** plus a shared **interface contract**. Each
prompt is meant to be pasted into its **own** Claude Code session, working on its **own** git
branch, in parallel. Together they rework ZandersEQ to look and behave like FabFilter Pro-Q 4
while **keeping the neon palette**.

## How to use

1. Open a prompt file (`branch-1-engine.md` … `branch-5-dsp-shapes.md`).
2. Paste its entire contents into a fresh Claude Code session pointed at this repo.
3. Each session creates its own branch off `dev`, does the work, keeps CI green, commits and
   pushes. Open PRs and merge in the order below.

Every prompt tells its session to read [`CONTRACT.md`](./CONTRACT.md) first — the frozen
interface (C1–C6) that lets the branches be written in parallel without colliding.

## The 5 branches

| # | Prompt | Branch | Owns | Role |
|---|---|---|---|---|
| 1 | [branch-1-engine.md](./branch-1-engine.md) | `feat/proq4-engine` | `Parameters.h`, `PluginProcessor.*`, `Presets.h`, `numBands` line of `EqMath.h`, `CMakeLists.txt`, engine tests | **Foundation:** 24-band pool, add/remove API, new param IDs, 2nd analyzer tap |
| 2 | [branch-2-layout.md](./branch-2-layout.md) | `feat/proq4-layout` | `PluginEditor.*`, `PresetBar.*` | Full-width reflow, drop the rail, Pro-Q bottom toolbar, float-panel wiring |
| 3 | [branch-3-graph.md](./branch-3-graph.md) | `feat/proq4-graph` | `EqGraphComponent.*` | Full-bleed analyzer, input/output spectrum, dynamic-band gestures, piano/EQ-sketch |
| 4 | [branch-4-panel.md](./branch-4-panel.md) | `feat/proq4-band-panel` | `BandEditorRail.*`, `BandStrip.*` | Floating band-control panel under the selected band |
| 5 | [branch-5-dsp-shapes.md](./branch-5-dsp-shapes.md) | `feat/proq4-dsp-shapes` | enum+math of `EqMath.h`, `Biquad.h`, DSP tests | New shapes (Tilt/Band-Pass/All-Pass) + steeper slopes (72/96/Brickwall) |

## Merge / integration order (important)

```
CONTRACT (already in this repo)  →  B1 (Engine)  →  B5 (DSP)  →  ( B3 Graph ∥ B4 Panel )  →  B2 Layout (last)
```

- **B1 and B5** are genuinely parallel (disjoint files except a far-apart line split in
  `EqMath.h`). Land B1 first — more branches depend on it.
- **B3 and B4** are parallel with each other and code from day 1 against the contract, but
  their *final compile* needs B1 + B5 merged.
- **B2 is merged last** because `PluginEditor.cpp` constructs and wires the graph (B3) and the
  panel (B4) together.

## Ground rules shared by all branches

- **Keep the neon brand.** Nobody edits `src/gui/Theme.h` or `src/gui/NeonLookAndFeel.*`.
  Reuse the existing tokens (`colourForFreq`, `rampColour`, glows, Space Grotesk / JetBrains
  Mono). This is a layout/feature redesign, not a retheme.
- **One sole writer per file** (see [CONTRACT.md](./CONTRACT.md) §matrix). Don't edit files you
  don't own; only consume their frozen public surface.
- **CI must stay green** on every branch: all `ctest` unit tests, pluginval strictness 10,
  RealtimeSanitizer (no malloc/lock/syscall on the audio thread), SonarCloud coverage (GUI and
  `Parameters.h` are excluded), perf bench (soft).
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```

## Pro-Q 4 reference (the target)

FabFilter Pro-Q 4 (Dec 2024): up to 24 bands; shapes Bell/Notch/Shelves/Cuts/Band-Pass/Tilt
Shelf/Flat Tilt/All-Pass; slopes to 96 dB/oct + Brickwall; full-bleed dark display with **no
side panel**; band controls **float under the selected band**; a **slim bottom toolbar**; an
analyzer drawing **input + output** spectra; **Spectrum Grab**; **EQ Sketch**; a **piano**
overlay; per-band color coding; multi-band select. We already have dynamic EQ, per-band M/S,
EQ-match, auto-gain, spectrum grab, A/B and undo/redo — so the gap is mostly *layout + dynamic
bands + a couple of shape/slope additions*, which is how these 5 branches are scoped.
