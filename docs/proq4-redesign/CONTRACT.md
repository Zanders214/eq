# Frozen interface contract (C1–C6)

Every branch codes against this surface so the 5 sessions can run in parallel and integrate
cleanly. **Do not deviate from these names/signatures/orderings** — they are the seams. If you
think the contract is wrong, stop and flag it rather than silently changing it (a change here
ripples into other branches).

---

## C1 — `numBands` / pool size is owned by Branch 1 only

`numBands` is a single `constexpr` in `src/dsp/EqMath.h` referenced at 40+ sites
(`std::array<…, numBands>` across the processor, the graph, the band strip, MatchFit, presets).

- **Branch 1** edits **only** the `numBands` line, setting it `6 → 24`. Every
  `std::array<…, numBands>` member resizes automatically.
- **Branch 5** edits **only** the enum + math region of `EqMath.h`, far below the constant.
- The two hunks are far apart → git auto-merges cleanly. No other branch edits `EqMath.h`.

## C2 — `FilterType` + slope lists are APPEND-ONLY

Existing indices are **frozen** for preset/session back-compat. The audio path does
`static_cast<FilterType>((int) type->load())`, so a choice index must equal its enum value.

```cpp
// src/dsp/EqMath.h  (Branch 5 implements the new enumerators + coeffs)
enum class FilterType {
  highPass = 0, lowShelf, bell, notch, highShelf, lowPass,   // FROZEN 0..5
  tiltShelf, bandPass, allPass                               // NEW, appended in THIS order
};

// src/Parameters.h  (Branch 1 declares choice strings in THIS exact order)
filterTypeChoices() = { "High Pass","Low Shelf","Bell","Notch","High Shelf","Low Pass",
                        "Tilt Shelf","Band-Pass","All-Pass" };

slopeChoices() = { "12 dB/oct","24 dB/oct","48 dB/oct",   // FROZEN 0..2
                   "72 dB/oct","96 dB/oct","Brickwall" };  // NEW
// slopeIndexToValue: 0->12, 1->24, 2->48, 3->72, 4->96, 5->Brickwall (sentinel value)
```

Classification predicates (Branch 5, in `EqMath.h`), consumed by the graph + audio path:
- `bandPass`, `allPass` → `sitsOnZeroLine() == true` (no gain handle drawn).
- `tiltShelf` → has gain (gain handle drawn).
- `maxCascade` in `src/dsp/Biquad.h` rises **4 → 8** to support 96 dB/oct (8 × 12).

## C3 — Processor dynamic-band API (Branch 1 publishes; Branch 3/4 consume)

Freeze these in the **public** section of `src/PluginProcessor.h`:

```cpp
int  activeBandCount() const noexcept;                 // number of active slots
bool isBandActive (int slot) const noexcept;           // by pool index 0..numBands-1
int  firstFreeSlot() const noexcept;                   // -1 if the pool is full
int  addBand (float freq, float gain, FilterType type) noexcept; // returns slot, -1 if full; UNDOABLE
void removeBand (int slot) noexcept;                   // deactivates slot; UNDOABLE
int  getSelectedBand() const noexcept;                 // ALREADY EXISTS — keep
void setSelectedBand (int slot) noexcept;              // ALREADY EXISTS — keep
```

Semantics:
- `active` is a per-band APVTS bool param `band{i}_active` (automatable, persisted, undoable).
- Audio gate becomes `active && on && (!anySolo || solo)`.
- `addBand` finds `firstFreeSlot`, sets that slot's freq/gain/type, sets `active=true`,
  selects it. `removeBand` sets `active=false`. Both wrap in `recordUndoableEdit` so they are
  single undo steps. **No allocation** — message-thread only, flipping pre-declared atomics.
- `addBand` replaces the current hand-rolled `EqGraphComponent::spareBand()` +
  `beginSpectrumGrab()` "find a disabled band and turn it on" logic.

## C4 — Graph → floating-panel positioning (3-way: Branch 2/3/4)

The band panel floats *under the selected node*. Freeze on `EqGraphComponent` (Branch 3):

```cpp
juce::Rectangle<int> getBandScreenBounds (int slot) const; // node bounds in graph-LOCAL coords; empty if inactive/off-screen
std::function<void(int slot)> onBandFocused;               // fires on selection change OR when the selected node moves
```

Positioning is owned by `EqContent` in `src/PluginEditor.cpp` (Branch 2): it owns the panel
instance, listens to `graph.onBandFocused`, queries `graph.getBandScreenBounds(slot)`,
translates to `EqContent` coords, asks the panel for `getDesiredSize()`, clamps to the window,
sets the panel bounds + visibility. **Selection stays single-source in the processor**
(`get/setSelectedBand`) — no duplicated selection state.

The panel (Branch 4) **keeps the class name `BandEditorRail` and the filenames
`BandEditorRail.cpp/.h`** (renaming would conflict across three CMake source-lists and churn
PluginEditor). It keeps its public surface and **adds one method**:

```cpp
class BandEditorRail : public juce::Component {
public:
  explicit BandEditorRail (ZandersEqAudioProcessor&);  // KEEP ctor signature
  juce::Size<int> getDesiredSize() const;              // NEW — panel's natural size
  void bindToSelected();                                // KEEP
  void refresh();                                       // KEEP
  bool hasLiveReadout() const;                          // KEEP
  std::function<void()> onCapture, onMatch;             // KEEP — EQ-match wiring
};
```

## C5 — New param IDs (Branch 1 declares ALL of them; nobody else edits `Parameters.h`)

```cpp
// src/Parameters.h, namespace ids
inline constexpr const char* gainScale     = "gainscale";     // overall EQ gain 0..200%
inline constexpr const char* analyzerOn    = "analyzeron";    // choice: Off / Pre / Post (or Pre+Post)
inline constexpr const char* analyzerRange = "analyzerrange"; // choice: spectrum dB scale
inline constexpr const char* globalBypass  = "bypass";        // bool: master bypass
inline constexpr const char* phaseMode     = "phasemode";     // choice: Zero Latency / Natural / Linear (DECLARE now, impl deferred)
// plus per-band: ids::active(i) -> "band{i}_active" (bool)
```

`phaseMode` is declared so Branch 2's toolbar layout is final, but only "Zero Latency" behaves
this round; "Natural"/"Linear" are inert/disabled. (Linear-phase is a separate FFT engine with
variable latency that would threaten the RT-safety + pluginval gates — explicitly a later round.)

## C6 — Keep the neon brand

No branch edits `src/gui/Theme.h` or `src/gui/NeonLookAndFeel.*`. Reuse existing tokens
(`colourForFreq`, `rampColour`, `whiteAlpha`, the glow helper, the fonts). For the
input-vs-output analyzer, draw the **input** spectrum as a dim/ghosted gray fill and the
**output** spectrum as the existing neon ramp — Pro-Q's pre/post distinction without
abandoning the palette. If you genuinely need a new shared color token, flag it rather than
editing `Theme.h` from a branch that doesn't own it.

---

## File-ownership × branch matrix

`W` = sole writer · `R` = read-only (no edits) · `r` = depends on its public API (rebase if it changes)

| File | B1 Engine | B2 Layout | B3 Graph | B4 Panel | B5 DSP |
|---|---|---|---|---|---|
| `src/Parameters.h` | **W** | r | r | r | r |
| `src/PluginProcessor.*` | **W** | r | r | r | r |
| `src/Presets.h` | **W** | – | – | – | r |
| `src/dsp/EqMath.h` | W (`numBands` line only) | – | R | R | **W** (enum+math) |
| `src/dsp/Biquad.h` | – | – | – | – | **W** |
| `src/dsp/MatchFit.h` | r | – | R | – | r |
| `src/PluginEditor.*` | – | **W** | r | r | – |
| `src/gui/EqGraphComponent.*` | – | r | **W** | r | R |
| `src/gui/BandEditorRail.*` | – | r (ctor) | – | **W** | – |
| `src/gui/BandStrip.*` | – | r | – | **W** | – |
| `src/gui/PresetBar.*` | – | **W** | – | – | – |
| `src/gui/Theme.h`, `NeonLookAndFeel.*` | R | R | R | R | – |
| `CMakeLists.txt` | **W** (all edits) | – | – | – | hands test lines to B1 |

## Merge order

```
B1 (Engine) → B5 (DSP) → ( B3 Graph ∥ B4 Panel ) → B2 Layout (last)
```
