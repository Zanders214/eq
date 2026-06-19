# Paste this into Claude Code

Copy the prompt below into Claude Code, run from the root of the unzipped handoff folder.

---

I'm building **ZandersEQ**, a FabFilter Pro-Q-style parametric EQ **audio plugin** for DAWs. This folder contains the design reference and the EQ math. Read `README.md` first — it has the full UI spec, design tokens, and DSP notes.

**Stack:** Use **JUCE (C++)**, building VST3 + AU (+ Standalone for testing) via CMake and `juce_add_plugin`. If JUCE isn't set up, scaffold it (FetchContent or a JUCE submodule). I'm on <fill in: macOS / Windows>.

**Do this in order:**

1. **Scaffold** a JUCE plugin project (CMake), product name "ZandersEQ", with stereo in/out, an `AudioProcessor` + `AudioProcessorEditor`, and an `AudioProcessorValueTreeState` (APVTS) for parameters.

2. **DSP first.** Port the biquad coefficient math from `design/EQGraph.jsx` (`bandDb()` covers bell / lowshelf / highshelf / hpf / lpf / notch via standard RBJ formulas). Implement a 6-band (expandable) EQ chain:
   - Per band params: type, freq (20–20k, log), gain (±18 dB), Q (0.1–18), slope (12/24/48 dB/oct for cuts — cascade biquads), on/off, solo.
   - Use `juce::dsp::IIR::Filter` / `ProcessorChain` or hand-rolled Direct-Form II.
   - Output gain (±24 dB). Stereo and Mid/Side modes (encode→process→decode for M/S).
   - Smooth all parameter changes (no zipper noise).

3. **Editor UI** matching the design in `design/ZandersEQ Editor.dc.html`. Match the tokens in README exactly (spectrum ramp, blue accent, Space Grotesk + JetBrains Mono, dark glass panel, glows). Build:
   - Analyzer + draggable response curve (the hero) — drag node = freq/gain, scroll = Q, double-click empty = add band, double-click node = remove. Draw the curve from the **same coefficients** the audio thread uses.
   - Real-time **FFT spectrum analyzer** behind the curve (`juce::dsp::FFT`, log-frequency bins).
   - Band strip, presets row, and the band-editor rail (type chips, freq/gain/Q sliders, enable/solo, output dial, Stereo/MS, HQ, A/B) — all bound to APVTS.

4. **Presets:** implement the six in the README (Flat, Vocal Air, De-Mud, Bass Tight, Lo-Fi, Bright).

5. Build and confirm it loads in a DAW / `pluginval`.

**Then propose (don't build yet)** the Pro-Q-parity features that aren't in the mock: **dynamic EQ** (per-band threshold/range/attack/release with a detector), per-band L/R · M/S, linear-phase mode, spectrum grab, auto-gain. Ask me which to implement.

Keep the audio thread real-time-safe (no allocations/locks in `processBlock`). Ask before adding large dependencies.
