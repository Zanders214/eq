# Handoff: ZandersEQ — Parametric / Dynamic EQ Plugin

## Overview
ZandersEQ is a FabFilter Pro-Q-style **parametric EQ audio plugin** for DAWs (Ableton, Logic, FL, Reaper, Pro Tools, etc.). The hero is a live frequency-response curve with a spectrum analyzer behind it; users drag band nodes on the curve and refine them in a side-rail editor.

This bundle is the **design + interaction reference** plus the **EQ DSP math**, ready to be implemented as a real plugin.

> ⚠️ **Read this first — target environment.** A DAW EQ is an **audio plugin**, not a web app. The HTML files here are a *visual and behavioral specification*, NOT production code to ship. Recreate this design in an audio-plugin framework — **JUCE (C++)** is the standard, strongly recommended choice; iPlug2 (C++) or nih-plug (Rust) are alternatives. Build the UI in that framework's drawing layer (JUCE: `juce::Graphics` / OpenGL, or a WebView UI via `juce::WebBrowserComponent` if you want to reuse the HTML). Do **not** attempt to ship the HTML as the plugin.

## Fidelity
**High-fidelity.** Final colors, typography, spacing, layout, and interaction model are all intentional and should be matched closely. Exact tokens are listed below.

---

## The two design files

| File | What it is |
|---|---|
| `design/ZandersEQ Editor.dc.html` | **The primary design** — the full single-panel editor (1080×~600). Build this. |
| `design/ZandersEQ.dc.html` | Three layout explorations side-by-side (analyzer-first / knob-bank / side-rail). Reference only — the side-rail take became the Editor. |
| `design/EQGraph.jsx` | The analyzer + curve + draggable-node widget. **Contains the real biquad EQ math** (see "DSP" below) — this is the most directly portable asset. |

To view: open `ZandersEQ Editor.dc.html` in a browser. Drag nodes (freq/gain), scroll a node (Q), double-click empty graph (add band), double-click a node (remove).

---

## Screen: ZandersEQ Editor

### Layout
- Fixed-width plugin panel, **1080px wide**, dark radial-glass background, 26px padding, 16px corner radius, floats on `0 18px 50px rgba(0,0,0,0.4)`.
- Centered on a dark stage (`radial-gradient(130% 90% at 50% -10%, #14191f, #050608 70%)`).
- **Header row:** wordmark `Zanders` + spectrum-violet `EQ` + `SHAPE` badge (left); A/B toggle + `OUT` readout (right).
- **Body:** two columns, 22px gap.
  - **Left (fixed):** analyzer graph **720×440** in an inset well (12px radius, deep inner shadow, 4px pad) → all-bands strip (6 equal cells) → presets row (6 equal chips).
  - **Right (flex):** band-editor rail.

### Components

**Analyzer graph (720×440)** — three stacked layers:
1. Canvas: log-frequency grid (20 Hz–20 kHz), dB grid (±12, ±6, 0), animated FFT spectrum fill using the spectrum ramp at ~20–30% alpha, peak-hold dots.
2. SVG response curve: summed band magnitude, white `#e8ecf3` 2px stroke with `drop-shadow(0 0 6px rgba(180,200,255,0.35))`, faint fill below.
3. SVG nodes: one circle per band, radius 9px, filled in its **spectrum color** (see Band color), glowing `drop-shadow(0 0 5–9px color)`, white number label. Selected node gets a ring + brighter glow + dashed per-band curve.

**Band strip cell (×6)** — `var(--layer-1)` fill, selected = `rgba(94,147,255,0.10)` + `rgba(94,147,255,0.45)` border. Shows colored dot + type label, ON/OFF pill, frequency (mono), and `gain · Q` sub-line.

**Preset chip (×6)** — Flat, Vocal Air, De-Mud, Bass Tight, Lo-Fi, Bright. Selected = violet fill `rgba(139,123,255,0.16)` + `#b6abff` text.

**Band-editor rail:**
- Header: colored dot + `BAND <n>` + filter type (mono).
- Type chips: `HP · LO · BELL · NTCH · HI · LP` — active = accent fill.
- Three sliders (kit `Slider` component): `FREQUENCY` (cool ramp), `GAIN` (warm if boost / cool if cut), `Q / SLOPE` (warm ramp). Value labels in mono.
- `ENABLED/BYPASSED` toggle + `SOLO` toggle (solo = amber→pink gradient).
- Divider, then `OUTPUT` dial (accent) + `STEREO↔MID/SIDE` toggle + `HQ OVERSAMPLING` toggle.

---

## Interactions & Behavior
- **Drag node** → set band frequency (x, log scale) + gain (y), live. Cut filters (HP/LP/notch) lock to the 0 dB line and only move in frequency.
- **Scroll/wheel on node** → adjust Q (×1.12 per notch, clamped 0.1–18).
- **Double-click empty graph** → add a bell band at that frequency.
- **Double-click node** → remove that band (min 1 band).
- **Click band strip cell / node** → select; the rail edits the selected band.
- **Solo** → other bands visually drop to ~30% and (in DSP) are muted.
- **A/B** → swap to a second independent band set; lets users compare two curves.
- Transitions are short and mechanical (`0.15s` fades, no bounce). Spectrum animation is `requestAnimationFrame`-driven.

## State Management
Per EQ instance (and one extra set for the A/B "other" slot):
- `bands[]`: `{ id, type, freq (Hz), gain (dB), q, slope (dB/oct, cut filters only), on, solo }`
- `selectedId`, `nextId`, `output` (0..1 → ±24 dB), `mode` (STEREO | M/S), `hq` (bool), `slot` (A | B), `other` (the B band set).

Default bands: HP@30Hz/24, bell@95/+3.2/0.9, bell@420/−3.8/1.5, bell@2.6k/+2.6/1.1, hi-shelf@8.2k/+3.6/0.7, LP@19k/12.

---

## DSP — the actual EQ engine (most important section)

`design/EQGraph.jsx` already implements **RBJ biquad** coefficients for every filter type (`bandDb()` function). These are the same coefficients you compute in the audio thread — port them directly to C++ `processBlock`.

For each band, compute a `juce::dsp::IIR::Coefficients` (or hand-rolled Direct-Form II biquad) from `freq`, `gain`, `q`, sample rate, using the standard RBJ cookbook formulas:
- **bell/peaking**, **lowshelf**, **highshelf**, **hpf**, **lpf**, **notch** — all present in `bandDb()`.
- Cut filters (HP/LP) cascade biquads to reach the selected slope (12/24/48 dB/oct); the JS approximates this with a `slope/12` multiplier — in DSP, actually cascade N biquads.
- Run bands in series. Apply per-channel for Mid/Side mode (encode M/S, process, decode).
- `output` gain applied at the end.

The curve drawn on screen is `20·log10(|H(e^jω)|)` summed across bands — keep the display fed from the *same* coefficients the audio thread uses so the curve never lies.

### Recommended additions to reach true Pro-Q parity (not in this mock):
- **Dynamic EQ** per band: add `threshold`, `range`, `attack`, `release`; band gain reacts to a per-band detector. UI: a second handle on the node for threshold/range.
- **Per-band channel mode** (L/R or M/S), not just the global toggle shown.
- **Phase modes**: Zero-Latency / Natural / Linear-Phase (FFT convolution).
- **Spectrum grab**, EQ match, auto-gain.

---

## Design Tokens (exact)

**Spectrum ramp (the brand):** cyan `#34d8ff` → violet `#8b7bff` → pink `#ff5fa8` → amber `#ffc24b`. Band node color = position of its frequency along log(20→20k) mapped onto this ramp.

**Accent (single):** blue `#5e93ff`; engaged toggle gradient `linear-gradient(180deg,#5e93ff,#8b7bff)`, glow `0 0 16px rgba(94,147,255,0.45)`.

**Alarm:** red `#ff5a5a → #e23b3b` (destructive/STOP states only).

**Text:** primary `#e8ecf3`, then `#cfd4dc`, `#8a93a3`, `#7e8794`, ticks `#5d6473`.

**Surface:** panel `radial-gradient(120% 80% at 50% -10%, #1a2030, #0a0b12)`; inset display wells `#070a0e` / `#050608`. Structure built from white-alpha layers (5%→12%), never opaque grey.

**Type:** Space Grotesk (UI labels all-caps, tracking 0.14–0.22em, weight 600; big numerals tight `-0.02em`). JetBrains Mono for every measured value + unit (`1.24 kHz`, `-6.0 dB`, `Q 1.5`, `74%`). Both from Google Fonts.

**Radii:** panel 16, windows 12, buttons 10–12, chips 8, badges full pill, knobs/dots full circle.

**Shadows/glow:** one outer drop on the panel (`0 18px 50px rgba(0,0,0,0.4)`); everything else inset. Color elements glow in *their own color* via `box-shadow` (dots/buttons) or `drop-shadow` (rings/arcs).

**Motion:** state changes `0.15s`; fills `0.08s`; presses `translateY(2px)` over `0.04s`. No bounce. Continuous motion is rAF-driven and physically modeled.

The full token CSS is bundled at `design/_ds/.../tokens/` (`colors.css`, `typography.css`, `spacing.css`, `effects.css`) — read exact `var(--*)` values there.

## Assets
**None.** Every mark/control is pure CSS/SVG/canvas. The "logo" is type only (`Zanders` + colored product name). No image files. Icons are near-zero by design — a couple of Unicode glyphs only; do not introduce an icon library.

## Files
- `design/ZandersEQ Editor.dc.html` — primary design (build this)
- `design/ZandersEQ.dc.html` — 3 layout explorations (reference)
- `design/EQGraph.jsx` — analyzer + **biquad EQ math** (port to DSP)
- `design/_ds/` — the Neon Plugins design system (tokens, fonts, components, guide in `readme.md`)
- `CLAUDE_CODE_PROMPT.md` — paste-in starter prompt for Claude Code
