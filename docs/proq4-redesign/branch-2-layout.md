# Prompt — Branch 2: Layout reflow & bottom toolbar (`feat/proq4-layout`)

> Paste everything below into a fresh Claude Code session pointed at the ZandersEQ repo.

---

You are working on **ZandersEQ**, a JUCE 8 / C++17 parametric EQ audio plugin built with CMake.
We are reworking it to look and behave like **FabFilter Pro-Q 4** while **keeping the existing
neon visual brand**. This is a **5-branch parallel effort**; you are **Branch 2 (layout) and you
MERGE LAST**, because `PluginEditor.cpp` constructs and wires the graph (Branch 3) and the
floating panel (Branch 4) together. You can build the toolbar + region layout from day 1 against
the contract; do the final float-panel wiring after Branch 3/4 land.

**Before doing anything, read `docs/proq4-redesign/CONTRACT.md`** (frozen interface C1–C6 +
ownership matrix). You are the **sole writer** of `src/PluginEditor.cpp/.h` and
`src/gui/PresetBar.cpp/.h`. You **consume** (but must not edit) `EqGraphComponent`,
`BandEditorRail`, `Parameters.h`, `PluginProcessor.h`, `Theme.h`. **Do not edit `Theme.h` /
`NeonLookAndFeel` — keep the neon palette (C6).**

## Setup

```bash
git fetch origin dev
git checkout -b feat/proq4-layout origin/dev
```
Rebase onto the latest `dev` after Branches 1, 5, 3, 4 are merged before you do the final
floating-panel integration (you need `EqGraphComponent::getBandScreenBounds` / `onBandFocused`
from Branch 3 and `BandEditorRail::getDesiredSize()` from Branch 4).

## Pro-Q 4 reference for this slice

Pro-Q 4 has **no side panel**. A **full-width spectrum display** dominates the window; band
parameters appear in a **floating panel under the selected band**; and a **slim bottom toolbar**
runs along the bottom with the global controls. The window is landscape and resizable
(full-screen capable). Today ZandersEQ instead has a 1100×772 panel with a 720×440 graph on the
left and a **350px rail on the right** — that rail goes away.

## Tasks

1. **Reflow `EqContent::resized()`** (`PluginEditor.cpp`):
   - **Remove the 350px right rail region.** Make the graph occupy the **full width** of the
     content area.
   - Reserve a **slim bottom toolbar** strip beneath the graph (replace/repurpose the current
     preset-bar + band-strip stack as appropriate).
   - Widen the design aspect toward Pro-Q's landscape: adjust `designW`/`designH` in
     `PluginEditor.h` (keep the existing aspect-locked uniform `AffineTransform` scaling and the
     persisted size logic). Pick sensible new proportions (e.g. ~1280×720-ish) and keep the
     min/max scale clamps working.

2. **Build the Pro-Q bottom toolbar.** Repurpose `PresetBar` (you own it) into the toolbar, or
   draw a toolbar region in `EqContent` and keep `PresetBar` for the preset chips — your call,
   but keep it neon-styled and slim. Bind the controls to the params **Branch 1 declared** (see
   C5 — don't add params yourself):
   - **Processing mode** selector bound to `ids::phaseMode` {Zero Latency, Natural, Linear} —
     render **Natural/Linear as disabled / "coming soon"** (only Zero Latency works this round).
   - **Analyzer settings** (`ids::analyzerOn`, `ids::analyzerRange`).
   - **Global bypass** (`ids::globalBypass`), **gain-scale** (`ids::gainScale`), **output**
     (existing `ids::output`), **auto-gain** (existing), **HQ** (existing).
   - **Full-screen** toggle, **EQ-Sketch** toggle, **piano-display** toggle (these can be UI
     state / call into the graph; coordinate the toggle semantics with Branch 3).
   - Keep the existing **A/B**, **undo/redo**, **preset** chips, and the **OUT** readout
     reachable (currently drawn in the header — move them into the toolbar or keep a slim header).

3. **Wire the floating band panel (C4).** `EqContent` owns the `BandEditorRail` instance
   (already a member). Instead of placing it in a fixed rail:
   - Subscribe to `graph.onBandFocused`.
   - On focus, query `graph.getBandScreenBounds(slot)`, translate to `EqContent` coords, ask
     `rail.getDesiredSize()`, **clamp to the window**, set the panel's bounds, and show it.
   - Hide the panel when nothing is selected. Keep the panel above the graph in z-order.
   - Keep the existing callback wiring (`onSelectionChanged`, `onCapture`, `onMatch`,
     `onPresetApplied`) intact.

4. **Headless compile.** `EngineTests` links `PluginEditor.cpp`, so `createEditor()` and
   `EqContent` must still construct headless (the CI uses `ZEQ_DISABLE_GL`). Verify the build
   without the rail occupying the old region.

## Constraints / done criteria

- **Keep the neon palette** — reuse `Theme.h` tokens, glows and fonts.
- Don't edit `Theme.h`/`Parameters`/`Processor`/`EqGraphComponent`/`BandEditorRail` internals —
  only consume their frozen public surfaces (C3/C4/C5).
- Keep green: build, `ctest`, **pluginval strictness 10** (editor open/close/resize), Sonar
  (GUI excluded).
- Build & test:
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
  Then launch the Standalone and confirm: full-width graph, no side rail, the floating panel
  appears under the selected band and follows it, and the bottom toolbar binds correctly.
- Commit and push:
  ```bash
  git push -u origin feat/proq4-layout
  ```
  (Retry with exponential backoff on network errors. No PR unless asked.)
