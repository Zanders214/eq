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

## 2. Per-band channel mode (L / R / M / S / Stereo)
Today Stereo↔Mid/Side is global. Pro-Q lets each band target L, R, M, S or Stereo.

- **DSP**: process up to four lanes (L,R,M,S) and route each band to its lane. Cheapest
  path: always compute both stereo and M/S encodings and pick per band, decoding once at
  the end. A modest restructure of `processEq`.
- **Params/UI**: `bandN_channel` choice; a channel chip in the rail; node tint or a small
  L/R/M/S glyph per node.

## 3. Phase modes: Zero-Latency / Natural / Linear-Phase
A linear-phase path via FFT (overlap-add) convolution of the summed impulse response,
selectable alongside the current minimum-phase IIR engine.

- **DSP**: build the summed frequency response (we already have it for the curve),
  IFFT to an impulse, and run a partitioned-convolution path with reported latency. Big
  but self-contained; the IIR path stays the default "Zero-Latency".
- **UI**: a phase-mode selector in the header/rail.

## 4. Spectrum grab & EQ match
Click-drag on the analyzer to pull a bell toward a resonance; "match" a reference curve
by fitting bands to a captured average spectrum.

- We already have FFT magnitudes per bin; add a long-term average + a peak-finder, then a
  fitting pass that writes band params.

## 5. Auto-gain
Track input vs. output RMS and trim `output` so perceived loudness stays constant while
A/B-ing EQ moves. A small detector on the existing input/output taps plus a smoothed
trim on the output stage.

## Smaller polish
- Piano-key / note readout next to the frequency value.
- Per-band bypass via right-click on a node; alt-drag to constrain to gain-only.
- Resizable / scalable UI (the panel is currently fixed at 1100×690).
- Undo/redo via `UndoManager` on the APVTS.
- `pluginval` strictness level 10 in CI.
