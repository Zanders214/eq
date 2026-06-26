# Prompt — Branch 3: Hero display — graph & analyzer (`feat/proq4-graph`)

> Paste everything below into a fresh Claude Code session pointed at the ZandersEQ repo.

---

You are working on **ZandersEQ**, a JUCE 8 / C++17 parametric EQ audio plugin built with CMake.
We are reworking it to look and behave like **FabFilter Pro-Q 4** while **keeping the existing
neon visual brand**. This is a **5-branch parallel effort**; you are **Branch 3 (the hero
graph)**. You code in parallel against the contract from day 1; your branch *final-compiles*
after Branch 1 (engine) and Branch 5 (DSP) land, so build against their frozen signatures.

**Before doing anything, read `docs/proq4-redesign/CONTRACT.md`** (frozen interface C1–C6 +
ownership matrix). You are the **sole writer** of `src/gui/EqGraphComponent.cpp/.h`. You may
**read** `EqMath.h`, `Theme.h`, and `PluginProcessor.h`, but must **not edit** them or any other
file. **Do not edit `Theme.h` / `NeonLookAndFeel` — keep the neon palette (C6).**

## Setup

```bash
git fetch origin dev
git checkout -b feat/proq4-graph origin/dev
```
If `feat/proq4-engine` / `feat/proq4-dsp-shapes` are already merged to `dev`, rebase on the
latest `dev` so the new processor API and FilterType shapes are available; otherwise code
against the signatures in `CONTRACT.md` and rebase before final compile.

## Pro-Q 4 reference for this slice

A **full-bleed** dark display dominates the window: a clean log-frequency grid with freq/dB
axis labels, the EQ curve as a thin bright line, and a spectrum analyzer drawing the **input**
spectrum and the **output** spectrum together. Band **dots** sit on the curve, color-coded;
hovering/ selecting a band emphasizes its dot. You create a band by **clicking empty space**
and delete one by **double-clicking its dot**. There's an optional **piano-keyboard overlay**
on the frequency axis and an **EQ Sketch** mode (draw a curve left→right and bands are created
to match). **Spectrum Grab** (hover a peak, drag to make a band) already exists here — keep it.

## Tasks

1. **Full-bleed layout.** Make the graph fill its bounds edge-to-edge (the layout branch gives
   it the full display region). Refine the log grid and the freq/dB axis labels. Support a
   selectable vertical **dB range** by reading `ids::analyzerRange` (declared by Branch 1).

2. **Input vs output spectrum (C6 keeps neon).** Read **two** FIFOs from the processor:
   `getPreEqAnalyzerFifo()` (input — draw as a **dim/ghosted gray** fill behind) and the
   existing post-EQ `getAnalyzerFifo()` (output — draw with the existing **neon ramp**). This
   gives Pro-Q's pre/post distinction without abandoning the palette. Respect `ids::analyzerOn`
   (Off / Pre / Post) if you wire the toolbar control's meaning.

3. **Dynamic bands (C3).** Replace the hand-rolled `spareBand()` + `beginSpectrumGrab()`
   "find a disabled band and turn it on" logic with the processor API:
   - **Click empty space →** `proc.addBand(freq, gain, FilterType::bell)` (or snap-to-peak for
     the spectrum-grab gesture), then drag to set freq/gain in the same gesture.
   - **Double-click a dot →** `proc.removeBand(slot)`.
   - **Draw ALL active bands:** loop slots `0..numBands-1`, skip `!isBandActive(slot)`. Don't
     assume 6. Pro-Q-style dots with clear hover and selected/active emphasis.
   - Handle the new shapes from Branch 5: use the contract's `sitsOnZeroLine()` to decide
     whether a band gets a gain handle (bandPass/allPass don't; tiltShelf does).

4. **Publish the positioning contract (C4).** Add
   `juce::Rectangle<int> getBandScreenBounds(int slot) const` (node bounds in graph-local
   coords; empty if inactive/off-screen) and `std::function<void(int slot)> onBandFocused`
   (fire it on selection change **and** when the selected node moves, so the floating panel
   follows). Selection stays single-source in the processor (`get/setSelectedBand`) — keep the
   existing `onSelectionChanged` working too if the layout branch still wires it.

5. **On-curve handles.** Add draggable handles on the curve for shelf/cut **slope** (and keep
   the existing scroll-for-Q and the spectrum-grab gesture).

6. **Lower-priority, do last (self-contained, don't block others):**
   - **Piano-keyboard overlay** toggle along the frequency axis (use a local flag or a
     piano-toggle param if the layout branch adds one; note `Theme::noteName()` already exists).
   - **EQ Sketch** gesture: while in sketch mode, the user drags left→right and you create a
     series of bands (via `addBand`) approximating the drawn curve.

## Constraints / done criteria

- **Keep the curve honest:** it must keep using the same `bandMagnitudeDb` the audio path uses.
- Keep `updateAnimation()` lock-free (it pulls the FIFO on the message thread).
- GUI is excluded from the coverage gate, but `EqGraphComponent.cpp` is compiled+linked by
  `EngineTests`, so it must **compile headless**. Optionally extract the freq→x / gain→y mapping
  into a small pure helper so `getBandScreenBounds` is unit-testable (helps Branches 2/4).
- Keep green: build, `ctest`, pluginval (editor open/close). Don't edit `Theme.h`/`Parameters`/
  `Processor`/any other GUI file.
- Build & test:
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
- Commit and push:
  ```bash
  git push -u origin feat/proq4-graph
  ```
  (Retry with exponential backoff on network errors. No PR unless asked.)
