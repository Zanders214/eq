# Prompt — Branch 4: Floating band-control panel (`feat/proq4-band-panel`)

> Paste everything below into a fresh Claude Code session pointed at the ZandersEQ repo.

---

You are working on **ZandersEQ**, a JUCE 8 / C++17 parametric EQ audio plugin built with CMake.
We are reworking it to look and behave like **FabFilter Pro-Q 4** while **keeping the existing
neon visual brand**. This is a **5-branch parallel effort**; you are **Branch 4 (the band-control
panel)**. You code in parallel against the contract; your branch *final-compiles* after Branch 1
(engine) lands.

**Before doing anything, read `docs/proq4-redesign/CONTRACT.md`** (frozen interface C1–C6 +
ownership matrix). You are the **sole writer** of `src/gui/BandEditorRail.cpp/.h` and
`src/gui/BandStrip.cpp/.h`. You may **read** `PluginProcessor.h` and `Theme.h` but must **not
edit** them or any other file. **Do not edit `Theme.h` / `NeonLookAndFeel` — keep the neon
palette (C6).**

## Setup

```bash
git fetch origin dev
git checkout -b feat/proq4-band-panel origin/dev
```
Rebase on the latest `dev` once `feat/proq4-engine` is merged so the processor's dynamic-band
API and new params exist; until then, code against the signatures in `CONTRACT.md`.

## Pro-Q 4 reference for this slice

In Pro-Q 4 there is **no fixed side panel**. When a band is selected, a compact **floating
control panel** appears **under that band** showing: filter **type/shape**, **frequency**,
**gain**, **Q**, **slope** (for cuts), **stereo placement** (L/R/M/S), and a **dynamics**
toggle that expands an "expert" sub-pane (threshold/range/attack/release/direction). It
disappears when nothing is selected. Multiple bands can be selected and edited together.

## Critical constraint (C4): keep the class name and filenames

`EqContent` in `PluginEditor.cpp` constructs this as `BandEditorRail rail{proc}` and three
CMake source-lists name `BandEditorRail.cpp`. **Do NOT rename the class or the files** —
repurpose `BandEditorRail` *internally* from a fixed right-rail into a floating panel. Keep its
public surface intact and **add exactly one method**:

```cpp
class BandEditorRail : public juce::Component {
public:
  explicit BandEditorRail (ZandersEqAudioProcessor&);  // KEEP this ctor signature
  juce::Size<int> getDesiredSize() const;              // NEW — the panel's natural size
  void bindToSelected();                                // KEEP — rebind controls to the selected band
  void refresh();                                       // KEEP — poll values, repaint
  bool hasLiveReadout() const;                          // KEEP — drives the editor's lazy-repaint loop
  std::function<void()> onCapture, onMatch;             // KEEP — EQ-match wiring used by PluginEditor
};
```

The layout branch (Branch 2) owns *where* the panel is placed (it queries the graph for the
selected node's position and calls your `getDesiredSize()`); you own *what the panel renders*
and how it binds to params. Selection comes from the processor (`getSelectedBand()`).

## Tasks

1. **Repurpose the rail into a compact floating panel.** Restyle/relayout the existing controls
   into a small panel sized by `getDesiredSize()`:
   - **Type chips** including the three new shapes (Tilt Shelf, Band-Pass, All-Pass) — read the
     choice strings from the APVTS param so they stay in sync with Branch 5/1.
   - **Frequency / Gain / Q** controls (keep the existing slider attachments; rebind on
     selection via `bindToSelected()`).
   - **Slope picker** including the new `72 dB/oct`, `96 dB/oct`, `Brickwall` options.
   - **Stereo placement** (the existing per-band channel chips).
   - **Dynamics** toggle + the "expert" sub-pane (threshold/range/attack/release/direction) —
     reuse the existing dynamic-EQ controls; render the expert pane as an expandable section.
   - Hide gain for shapes that sit on the zero line (band-pass/all-pass); show it for tilt shelf.
2. **`getDesiredSize()`** returns the panel's natural width/height for the current selection
   (e.g. taller when the dynamics expert pane is open). Keep it cheap and `const`.
3. **Keep `hasLiveReadout()`** truthful (it returns true while dynamic-EQ gain reduction or the
   auto-gain trim is animating) — the editor's timer uses it to keep repainting.
4. **Repurpose `BandStrip`** for the Pro-Q world: either a thin active-bands overview or fold it
   into the panel. **Do not change its constructor signature or its addAndMakeVisible contract**
   that `EqContent` relies on — if you want it gone, make it render nothing rather than removing
   the class (the layout branch owns whether it's added).
5. **Multi-select awareness** if feasible: if the processor exposes multiple selected bands,
   render shared controls that edit them together. (Keep it simple if selection is single.)

## Constraints / done criteria

- **Keep the neon palette** — reuse `Theme.h` tokens and the existing slider/dial/chip styles.
- GUI is excluded from the coverage gate, but `BandEditorRail.cpp` + `BandStrip.cpp` are
  compiled/linked by `EngineTests`, so they must **compile headless**.
- Don't break the EQ-match `onCapture`/`onMatch` wiring (PluginEditor connects to them).
- Keep green: build, `ctest`, pluginval (editor open/close). Don't edit `Theme.h`/`Parameters`/
  `Processor`/`EqGraphComponent`/`PluginEditor`.
- Build & test:
  ```bash
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEQ_BUILD_TESTS=ON
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
- Commit and push:
  ```bash
  git push -u origin feat/proq4-band-panel
  ```
  (Retry with exponential backoff on network errors. No PR unless asked.)
