/* EQGraph — controlled parametric-EQ display for the Zanders kit.
 *
 * Renders three layers stacked in a fixed box:
 *   1. <canvas>  grid + animated FFT spectrum (rAF, synthesized, reacts to the EQ curve)
 *   2. <svg>     the summed frequency-response curve (filled) + per-band curve when selected
 *   3. <svg>     draggable band nodes (drag = freq/gain, wheel = Q)
 *
 * Pure & controlled: all band data comes in via props; edits are reported through
 * onChange(id, patch) / onSelect(id) / onAdd(freq) / onRemove(id). No internal band state.
 *
 * props:
 *   bands        [{ id, type, freq, gain, q, slope, color, on, solo }]
 *                type ∈ bell | lowshelf | highshelf | hpf | lpf | notch
 *   selectedId   number | null
 *   onSelect, onChange, onAdd, onRemove
 *   width, height
 *   showAnalyzer bool   (default true)
 *   dbRange      number (default 18)
 *   compact      bool   (smaller node radius / fewer labels)
 */
(function () {
  const React = window.React;
  const { useRef, useEffect, useMemo, useCallback } = React;

  const FMIN = 20, FMAX = 20000, FS = 48000;
  const LOG_SPAN = Math.log(FMAX / FMIN);

  // ---- frequency / gain <-> pixel ----------------------------------------
  const fToX = (f, w) => (Math.log(f / FMIN) / LOG_SPAN) * w;
  const xToF = (x, w) => FMIN * Math.exp((x / w) * LOG_SPAN);
  const gToY = (g, h, R) => h / 2 - (g / R) * (h * 0.5 * 0.84);
  const yToG = (y, h, R) => ((h / 2 - y) / (h * 0.5 * 0.84)) * R;
  const clamp = (x, a, b) => Math.min(b, Math.max(a, x));

  // ---- RBJ biquad magnitude (dB) at frequency f --------------------------
  function bandDb(band, f) {
    if (!band.on) return 0;
    const w0 = (2 * Math.PI * band.freq) / FS;
    const cw = Math.cos(w0), sw = Math.sin(w0);
    const Q = Math.max(0.1, band.q || 0.7);
    const alpha = sw / (2 * Q);
    const A = Math.pow(10, (band.gain || 0) / 40);
    let b0, b1, b2, a0, a1, a2;
    switch (band.type) {
      case "lowshelf": {
        const ap = 2 * Math.sqrt(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + ap);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - ap);
        a0 = (A + 1) + (A - 1) * cw + ap;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - ap;
        break;
      }
      case "highshelf": {
        const ap = 2 * Math.sqrt(A) * alpha;
        b0 = A * ((A + 1) + (A - 1) * cw + ap);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - ap);
        a0 = (A + 1) - (A - 1) * cw + ap;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - ap;
        break;
      }
      case "hpf": {
        b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        break;
      }
      case "lpf": {
        b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        break;
      }
      case "notch": {
        b0 = 1; b1 = -2 * cw; b2 = 1;
        a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
        break;
      }
      default: { // bell / peaking
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
        a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
      }
    }
    // |H(e^jw)| at f
    const w = (2 * Math.PI * f) / FS;
    const cw1 = Math.cos(w), sw1 = Math.sin(w);
    const cw2 = Math.cos(2 * w), sw2 = Math.sin(2 * w);
    const numRe = b0 + b1 * cw1 + b2 * cw2;
    const numIm = -(b1 * sw1 + b2 * sw2);
    const denRe = a0 + a1 * cw1 + a2 * cw2;
    const denIm = -(a1 * sw1 + a2 * sw2);
    const mag = Math.sqrt((numRe * numRe + numIm * numIm) / (denRe * denRe + denIm * denIm));
    let db = 20 * Math.log10(Math.max(1e-7, mag));
    // slope multiplier for cut filters (cascaded biquads)
    if ((band.type === "hpf" || band.type === "lpf") && band.slope) {
      db *= band.slope / 12;
    }
    return db;
  }

  function totalDb(bands, f) {
    let sum = 0;
    for (const b of bands) sum += bandDb(b, f);
    return sum;
  }

  // -------------------------------------------------------------------------
  function EQGraph(props) {
    const { bands = [], selectedId = null, onSelect, onChange, onAdd, onRemove } = props;
    const width = Number(props.width) || 600;
    const height = Number(props.height) || 340;
    const dbRange = Number(props.dbRange) || 18;
    const showAnalyzer = props.showAnalyzer !== false;
    const compact = !!props.compact && props.compact !== "false";

    const canvasRef = useRef(null);
    const wrapRef = useRef(null);
    const bandsRef = useRef(bands);
    bandsRef.current = bands;
    const dragRef = useRef(null);

    // analyzer state held in refs (not React state) so rAF never re-renders
    const specRef = useRef(null);
    const peakRef = useRef(null);
    const seedRef = useRef(null);
    const NB = 96;
    if (!specRef.current) {
      specRef.current = new Float32Array(NB);
      peakRef.current = new Float32Array(NB);
      seedRef.current = Array.from({ length: NB }, (_, i) => ({
        ph: Math.random() * 6.28, sp: 0.4 + Math.random() * 1.6, amp: Math.random(),
      }));
    }

    // ---- animated grid + spectrum ----------------------------------------
    useEffect(() => {
      const cv = canvasRef.current;
      if (!cv) return;
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      cv.width = width * dpr; cv.height = height * dpr;
      const ctx = cv.getContext("2d");
      ctx.scale(dpr, dpr);
      let raf, t0 = performance.now(), running = true;

      const fLines = [
        [30], [40], [50], [60], [80], [100, "100"], [200], [300], [400], [500],
        [600], [800], [1000, "1k"], [2000], [3000], [4000], [5000], [6000],
        [8000], [10000, "10k"], [15000], [20000, "20k"],
      ];
      const dbLines = [-12, -6, 0, 6, 12];

      function frame(now) {
        if (!running) return;
        const t = (now - t0) / 1000;
        ctx.clearRect(0, 0, width, height);

        // grid
        ctx.lineWidth = 1;
        for (const [f, lab] of fLines) {
          const x = Math.round(fToX(f, width)) + 0.5;
          ctx.strokeStyle = lab ? "rgba(255,255,255,0.085)" : "rgba(255,255,255,0.035)";
          ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke();
          if (lab && !compact) {
            ctx.fillStyle = "rgba(125,135,148,0.9)";
            ctx.font = "9px 'JetBrains Mono', monospace";
            ctx.fillText(lab, x + 3, height - 5);
          }
        }
        for (const d of dbLines) {
          const y = Math.round(gToY(d, height, dbRange)) + 0.5;
          ctx.strokeStyle = d === 0 ? "rgba(255,255,255,0.14)" : "rgba(255,255,255,0.045)";
          ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
          if (!compact) {
            ctx.fillStyle = "rgba(125,135,148,0.85)";
            ctx.font = "9px 'JetBrains Mono', monospace";
            ctx.fillText((d > 0 ? "+" : "") + d, 4, y - 3);
          }
        }

        // analyzer
        if (showAnalyzer) {
          const spec = specRef.current, peak = peakRef.current, seed = seedRef.current;
          ctx.beginPath();
          ctx.moveTo(0, height);
          const pts = [];
          for (let i = 0; i < NB; i++) {
            const fr = i / (NB - 1);
            const f = FMIN * Math.exp(fr * LOG_SPAN);
            // pink-ish base falloff + slow per-bin modulation + EQ shaping
            const base = Math.pow(1 - fr, 1.7) * 0.82 + 0.06;
            const s = seed[i];
            const mod = 0.5 + 0.5 * Math.sin(t * s.sp + s.ph);
            const wob = 0.55 + 0.45 * Math.sin(t * 0.7 + fr * 9);
            let target = base * (0.45 + 0.55 * mod * s.amp) * wob;
            // EQ curve pushes the spectrum up/down
            const eq = totalDb(bandsRef.current, f);
            target *= Math.pow(10, eq / 40);
            target = clamp(target, 0, 1.15);
            spec[i] += (target - spec[i]) * 0.18;
            peak[i] = Math.max(spec[i], peak[i] - 0.006);
            const x = fr * width;
            const y = height - spec[i] * height * 0.92;
            pts.push([x, y, peak[i]]);
            ctx.lineTo(x, y);
          }
          ctx.lineTo(width, height);
          ctx.closePath();
          const grad = ctx.createLinearGradient(0, 0, width, 0);
          grad.addColorStop(0.0, "rgba(52,216,255,0.30)");
          grad.addColorStop(0.34, "rgba(139,123,255,0.30)");
          grad.addColorStop(0.67, "rgba(255,95,168,0.30)");
          grad.addColorStop(1.0, "rgba(255,194,75,0.30)");
          ctx.fillStyle = grad;
          ctx.fill();
          // crisp top line
          ctx.beginPath();
          pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
          ctx.strokeStyle = "rgba(200,210,230,0.30)";
          ctx.lineWidth = 1; ctx.stroke();
          // peak-hold dots
          ctx.fillStyle = "rgba(232,236,243,0.22)";
          for (const [x, , pk] of pts) {
            const py = height - pk * height * 0.92;
            ctx.fillRect(x - 0.5, py - 1, 1.5, 1.5);
          }
        }
        raf = requestAnimationFrame(frame);
      }
      raf = requestAnimationFrame(frame);
      return () => { running = false; cancelAnimationFrame(raf); };
    }, [width, height, showAnalyzer, dbRange, compact]);

    // ---- response curve (React-driven SVG path) --------------------------
    const { totalPath, totalFill, bandPath } = useMemo(() => {
      const STEP = 2;
      let d = "", fill = "";
      for (let x = 0; x <= width; x += STEP) {
        const f = xToF(x, width);
        const y = clamp(gToY(totalDb(bands, f), height, dbRange), -2, height + 2);
        d += (x === 0 ? "M" : "L") + x.toFixed(1) + " " + y.toFixed(1) + " ";
      }
      const mid = gToY(0, height, dbRange);
      fill = d + "L" + width + " " + mid + " L0 " + mid + " Z";
      let bp = "";
      const sel = bands.find((b) => b.id === selectedId);
      if (sel) {
        for (let x = 0; x <= width; x += STEP) {
          const f = xToF(x, width);
          const y = clamp(gToY(bandDb(sel, f), height, dbRange), -2, height + 2);
          bp += (x === 0 ? "M" : "L") + x.toFixed(1) + " " + y.toFixed(1) + " ";
        }
      }
      return { totalPath: d, totalFill: fill, bandPath: bp };
    }, [bands, selectedId, width, height, dbRange]);

    // ---- node dragging ----------------------------------------------------
    const startDrag = useCallback((e, band) => {
      e.preventDefault(); e.stopPropagation();
      onSelect && onSelect(band.id);
      const rect = wrapRef.current.getBoundingClientRect();
      const isCut = band.type === "hpf" || band.type === "lpf" || band.type === "notch";
      dragRef.current = { id: band.id, isCut };
      const move = (ev) => {
        const x = clamp(ev.clientX - rect.left, 0, width);
        const y = clamp(ev.clientY - rect.top, 0, height);
        const patch = { freq: clamp(xToF(x, width), FMIN, FMAX) };
        if (!isCut) patch.gain = clamp(yToG(y, height, dbRange), -dbRange, dbRange);
        onChange && onChange(band.id, patch);
      };
      const up = () => {
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
        dragRef.current = null;
      };
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    }, [onChange, onSelect, width, height, dbRange]);

    const onWheel = useCallback((e, band) => {
      if (!onChange) return;
      e.preventDefault();
      const factor = e.deltaY < 0 ? 1.12 : 1 / 1.12;
      onChange(band.id, { q: clamp((band.q || 0.7) * factor, 0.1, 18) });
    }, [onChange]);

    const bgClick = useCallback((e) => {
      if (e.target === e.currentTarget && onAdd) {
        const rect = wrapRef.current.getBoundingClientRect();
        const f = clamp(xToF(e.clientX - rect.left, width), FMIN, FMAX);
        onAdd(f);
      }
    }, [onAdd, width]);

    const R = compact ? 7 : 9;

    return React.createElement(
      "div",
      { ref: wrapRef, style: { position: "relative", width, height, touchAction: "none" } },
      React.createElement("canvas", {
        ref: canvasRef,
        style: { position: "absolute", inset: 0, width: "100%", height: "100%", display: "block" },
      }),
      // curve layer
      React.createElement(
        "svg",
        {
          width, height, onDoubleClick: bgClick,
          style: { position: "absolute", inset: 0, overflow: "visible" },
        },
        bandPath && React.createElement("path", {
          d: bandPath, fill: "none",
          stroke: (bands.find((b) => b.id === selectedId) || {}).color || "#fff",
          strokeOpacity: 0.5, strokeWidth: 1.5, strokeDasharray: "3 3",
        }),
        React.createElement("path", { d: totalFill, fill: "rgba(232,236,243,0.06)" }),
        React.createElement("path", {
          d: totalPath, fill: "none", stroke: "#e8ecf3",
          strokeWidth: 2, strokeLinejoin: "round",
          style: { filter: "drop-shadow(0 0 6px rgba(180,200,255,0.35))" },
        }),
        // nodes
        bands.map((b) => {
          const x = fToX(b.freq, width);
          const isCut = b.type === "hpf" || b.type === "lpf" || b.type === "notch";
          const y = isCut ? gToY(0, height, dbRange) : gToY(b.gain, height, dbRange);
          const sel = b.id === selectedId;
          return React.createElement(
            "g",
            {
              key: b.id, transform: `translate(${x},${y})`,
              style: { cursor: "grab" },
              onPointerDown: (e) => startDrag(e, b),
              onWheel: (e) => onWheel(e, b),
              onDoubleClick: (e) => { e.stopPropagation(); onRemove && onRemove(b.id); },
            },
            sel && React.createElement("circle", { r: R + 6, fill: "none", stroke: b.color, strokeOpacity: 0.35, strokeWidth: 1 }),
            React.createElement("circle", {
              r: R, fill: b.color, fillOpacity: b.on ? (sel ? 1 : 0.85) : 0.25,
              stroke: sel ? "#fff" : "rgba(255,255,255,0.5)", strokeWidth: sel ? 2 : 1,
              style: { filter: `drop-shadow(0 0 ${sel ? 9 : 5}px ${b.color})` },
            }),
            React.createElement("text", {
              textAnchor: "middle", dy: 3.5,
              fontSize: 9, fontFamily: "'Space Grotesk', sans-serif", fontWeight: 600,
              fill: b.on ? "#0a0b12" : "rgba(255,255,255,0.6)",
              style: { pointerEvents: "none", userSelect: "none" },
            }, b.num != null ? b.num : b.id + 1),
          );
        }),
      ),
    );
  }

  window.EQGraph = EQGraph;
  if (typeof module !== "undefined" && module.exports) module.exports = { EQGraph };
})();
