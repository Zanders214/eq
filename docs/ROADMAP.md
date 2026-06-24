# Roadmap — reaching true Pro-Q parity

The shipped plugin covers the full design handoff: 6-band parametric EQ, draggable
curve + FFT analyzer, band strip, presets, side rail, output gain, Stereo/MS, HQ
oversampling and A/B. The features below are **proposed, not built** — they are the
gap between this and FabFilter Pro-Q 3. Each is sketched with where it would land in
the code. Tell me which to implement and I'll do them in priority order.

## 1. Dynamic EQ ✅ (implemented — over-threshold, signed range)
Each bell/shelf band has a detector (band-pass on its own region + peak envelope follower)
and `dynon/threshold/range/attack/release`. Effective gain = `staticGain + dynamicGainDb(...)`,
folded into `updateCoeffs` at the 32-sample control rate (detector runs per sample on the
band's *input* to avoid self-feedback).

- **DSP**: `makeBandpass` + `dynamicGainDb` (`dsp/EqMath.h`), detector state on `BandDsp`
  (`dsp/Biquad.h`), wired into `processEq`. Unit-tested in `tests/DynamicsTests.cpp`.
- **UI**: an EQ | DYN tab in the rail (threshold/range/attack/release + enable); the curve
  animates with the live gain and the node carries a draggable dynamic-range handle.
- *Follow-ups*: ✅ Over/Under (below-threshold) direction; auto-threshold; external/sidechain
  detector; threshold-on-graph drag.

## 2. Per-band channel mode ✅ (implemented — domain-master model)
The global Stereo/Mid-Side toggle is kept as a **domain master**; each band then targets a
lane *within* that domain: **Both / first / second** (= L+R·L·R in Stereo, M+S·M·S in
Mid-Side). Because the whole chain shares one domain, the M/S encode/decode stays once per
sample and "Both" reproduces the old global behaviour exactly.

- **DSP** (`PluginProcessor::processEq` + `applyBand` in `dsp/Biquad.h`): encode the domain
  once per sample, run each active band through its lane(s), decode once. State resets on a
  global-domain flip (all bands) and per-band lane change. Unit-tested in
  `tests/ChannelRoutingTests.cpp`.
- **UI**: per-band `bandN_channel` choice; a 3-chip rail row that relabels with the global
  domain; an L/R/M/S letter badge on non-Both nodes.
- *Possible follow-up*: free per-band L/R/M/S mixing (would require per-band encode/decode).

## 3. Phase modes: Zero-Latency / Natural / Linear-Phase
A linear-phase path via FFT (overlap-add) convolution of the summed impulse response,
selectable alongside the current minimum-phase IIR engine.

- **DSP**: build the summed frequency response (we already have it for the curve),
  IFFT to an impulse, and run a partitioned-convolution path with reported latency. Big
  but self-contained; the IIR path stays the default "Zero-Latency".
- **UI**: a phase-mode selector in the header/rail.

## 4. Spectrum grab & EQ match
✅ **Both implemented.** *Spectrum grab*: press-drag on empty analyzer space spawns a bell
snapped to the nearest spectral peak (`EqGraphComponent::beginSpectrumGrab`). *EQ match*:
capture a reference (sidechain) and the source simultaneously (pre-EQ taps), average their
power spectra, and fit the 6 bands to the de-meaned difference via greedy peak-picking +
weighted least-squares (`src/dsp/MatchFit.h`, unit-tested in `tests/MatchFitTests.cpp`). A
Match Amount slider scales the correction and a ghost curve shows the target. Possible
follow-ups: load a reference from an audio file, and an LUFS/K-weighted detector.

## 5. Auto-gain
✅ **Implemented** — input vs. post-EQ RMS drives a smoothed output trim (±12 dB clamp) so
perceived loudness stays constant while A/B-ing EQ moves. Toggle in the rail shows the live
trim. See `PluginProcessor::processBlock`. Possible follow-up: switch the detector from RMS
to an LUFS/K-weighted measure.

## Smaller polish
- ✅ Piano-key / note readout next to the frequency value (12-TET, A4 = 440; rail + strip).
- ✅ Per-band bypass via right-click on a node; alt-drag to constrain to gain-only.
- ✅ Resizable / scalable UI — the editor hosts the fixed-design panel in a scaled content
  component (aspect-locked uniform zoom, ~0.6×–1.7×), size persisted with the plugin state.
- ✅ Undo/redo — whole-parameter snapshots (reusing the A/B capture/restore), Ctrl-Z /
  Ctrl-Shift-Z + header buttons, one step per gesture. (Chosen over an APVTS `UndoManager` so
  it stays synchronous and headless-testable.)
- ✅ User presets — save/recall the full state to `.zeqpreset` files (SAVE + LOAD in the preset
  bar); round-trip unit-tested. Follow-up: a richer browser, factory-preset bundling.
- ✅ CI: GitHub Actions builds + runs the unit tests + `pluginval` strictness 10 on every push
  (`.github/workflows/ci.yml`). Follow-up: macOS/Windows runners; pin pluginval; LTO-off on PRs.
