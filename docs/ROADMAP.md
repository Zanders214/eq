# Roadmap — reaching true Pro-Q parity

The shipped plugin covers the full design handoff: 6-band parametric EQ, draggable
curve + FFT analyzer, band strip, presets, side rail, output gain, Stereo/MS, HQ
oversampling and A/B. The features below are **proposed, not built** — they are the
gap between this and FabFilter Pro-Q 3. Each is sketched with where it would land in
the code. Tell me which to implement and I'll do them in priority order.

## 1. Dynamic EQ (highest impact)
Per-band downward/upward dynamics: each band gets `threshold`, `range`, `attack`,
`release` and a detector. The band's effective gain becomes
`staticGain + dynamicOffset(level)`, where the offset is driven by a per-band envelope
follower on the band's own filtered signal (or a sidechain).

- **DSP** (`PluginProcessor::processEq`): add an envelope follower per band; compute a
  gain-reduction/expansion amount per sub-block and fold it into the band's `gainDb`
  before `updateCoeffs`. Bands already recompute coefficients per 32-sample sub-block,
  so this slots in cleanly and stays real-time safe.
- **Params** (`Parameters.h`): `bandN_threshold/range/attack/release` + a `dynOn` toggle.
- **UI** (`EqGraphComponent`): a second handle on the node for threshold/range, and a
  moving "live gain" ghost on the curve. Rail gains a small dynamics sub-panel.

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
- Piano-key / note readout next to the frequency value.
- Per-band bypass via right-click on a node; alt-drag to constrain to gain-only.
- Resizable / scalable UI (the panel is currently fixed at 1100×690).
- Undo/redo via `UndoManager` on the APVTS.
- `pluginval` strictness level 10 in CI.
