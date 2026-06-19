/* @ds-bundle: {"format":3,"namespace":"NeonPluginsDesignSystem_54a692","components":[{"name":"Badge","sourcePath":"components/buttons/Badge.jsx"},{"name":"Chip","sourcePath":"components/buttons/Chip.jsx"},{"name":"GlowButton","sourcePath":"components/buttons/GlowButton.jsx"},{"name":"Dial","sourcePath":"components/controls/Dial.jsx"},{"name":"Knob","sourcePath":"components/controls/Knob.jsx"},{"name":"Meter","sourcePath":"components/controls/Meter.jsx"},{"name":"Slider","sourcePath":"components/controls/Slider.jsx"},{"name":"Keyboard","sourcePath":"components/surfaces/Keyboard.jsx"},{"name":"Panel","sourcePath":"components/surfaces/Panel.jsx"},{"name":"Wordmark","sourcePath":"components/surfaces/Wordmark.jsx"}],"sourceHashes":{"components/buttons/Badge.jsx":"1e77f0e93f07","components/buttons/Chip.jsx":"cbedfaa342bd","components/buttons/GlowButton.jsx":"b1bed9498cb1","components/controls/Dial.jsx":"10a42c88aa43","components/controls/Knob.jsx":"14d382d8fa0c","components/controls/Meter.jsx":"fabe0ed08077","components/controls/Slider.jsx":"c0fc4c4d2650","components/surfaces/Keyboard.jsx":"4616376b6b35","components/surfaces/Panel.jsx":"f218f9303c2d","components/surfaces/Wordmark.jsx":"680f205e89aa"},"inlinedExternals":[],"unexposedExports":[]} */

(() => {

const __ds_ns = (window.NeonPluginsDesignSystem_54a692 = window.NeonPluginsDesignSystem_54a692 || {});

const __ds_scope = {};

(__ds_ns.__errors = __ds_ns.__errors || []);

// components/buttons/Badge.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * Badge — a tiny all-caps pill in the blue accent. Used for the
 * product mode label (BUILD-UP, WIND-DOWN, GRAND) in the panel header.
 */
function Badge({
  children,
  style,
  ...rest
}) {
  return /*#__PURE__*/React.createElement("span", _extends({
    style: {
      fontFamily: "var(--font-display)",
      fontSize: "var(--fs-label-sm)",
      fontWeight: "var(--fw-semibold)",
      letterSpacing: "var(--tracking-label)",
      color: "var(--accent)",
      background: "var(--accent-soft)",
      borderRadius: "var(--radius-pill)",
      padding: "4px 11px",
      whiteSpace: "nowrap",
      ...style
    }
  }, rest), children);
}
Object.assign(__ds_scope, { Badge });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/buttons/Badge.jsx", error: String((e && e.message) || e) }); }

// components/buttons/Chip.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * Chip — a small status tile: a glowing color dot + a label and a mono
 * value beneath it. Used in rows to show effect bands (HPF, REV, DLY, RIS).
 * Dims to 0.4 opacity when its band is inactive.
 */
function Chip({
  label,
  value,
  color = "var(--spectrum-cyan)",
  active = true,
  glow = 1,
  style,
  ...rest
}) {
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      flex: 1,
      display: "flex",
      alignItems: "center",
      gap: "var(--space-3)",
      padding: 9,
      borderRadius: "var(--radius-button)",
      background: "var(--layer-1)",
      opacity: active ? 1 : 0.4,
      transition: "opacity var(--dur-base) var(--ease)",
      ...style
    }
  }, rest), /*#__PURE__*/React.createElement("span", {
    style: {
      width: 8,
      height: 8,
      borderRadius: "var(--radius-full)",
      background: color,
      boxShadow: `0 0 7px ${color}`,
      opacity: glow,
      flex: "none"
    }
  }), /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      fontFamily: "var(--font-display)",
      fontSize: "var(--fs-label-sm)",
      fontWeight: "var(--fw-semibold)",
      color: "var(--text-2)"
    }
  }, label), /*#__PURE__*/React.createElement("div", {
    style: {
      fontFamily: "var(--font-mono)",
      fontSize: "var(--fs-tick)",
      color: "var(--text-label)"
    }
  }, value)));
}
Object.assign(__ds_scope, { Chip });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/buttons/Chip.jsx", error: String((e && e.message) || e) }); }

// components/buttons/GlowButton.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * GlowButton — the kit's primary toggle. A wide pill with a small square
 * status dot and an outer color glow when engaged. Idle is a flat
 * white-alpha fill; engaged fills with a color gradient and lights up.
 */
function GlowButton({
  children,
  engaged = false,
  variant = "accent",
  // "accent" | "danger"
  idleLabel,
  onClick,
  style,
  ...rest
}) {
  const palette = variant === "danger" ? {
    grad: "linear-gradient(180deg,#ff5a5a,#e23b3b)",
    border: "rgba(255,120,120,0.6)",
    glow: "rgba(255,80,80,0.5)",
    idleGlow: "rgba(255,80,80,0.8)"
  } : {
    grad: "var(--accent-grad)",
    border: "rgba(150,170,255,0.6)",
    glow: "var(--accent-glow)",
    idleGlow: "rgba(94,147,255,0.8)"
  };
  const on = {
    background: palette.grad,
    color: "#fff",
    border: `1px solid ${palette.border}`,
    boxShadow: `0 0 22px ${palette.glow}, inset 0 1px 0 rgba(255,255,255,0.3)`,
    dotBg: "#fff",
    dotGlow: "#fff"
  };
  const off = {
    background: "var(--layer-2)",
    color: "var(--text-1)",
    border: "1px solid var(--layer-5)",
    boxShadow: "var(--inset-top)",
    dotBg: "var(--text-1)",
    dotGlow: palette.idleGlow
  };
  const s = engaged ? on : off;
  return /*#__PURE__*/React.createElement("button", _extends({
    type: "button",
    onClick: onClick,
    style: {
      height: "var(--button-h)",
      width: "100%",
      borderRadius: "var(--radius-button-lg)",
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      gap: "var(--space-4)",
      cursor: "pointer",
      userSelect: "none",
      fontFamily: "var(--font-display)",
      fontWeight: "var(--fw-bold)",
      letterSpacing: "var(--tracking-cap)",
      fontSize: "14px",
      transition: "all var(--dur-base) var(--ease)",
      background: s.background,
      color: s.color,
      border: s.border,
      boxShadow: s.boxShadow,
      ...style
    }
  }, rest), /*#__PURE__*/React.createElement("span", {
    style: {
      width: 9,
      height: 9,
      borderRadius: 2,
      background: s.dotBg,
      boxShadow: `0 0 8px ${s.dotGlow}`
    }
  }), engaged ? children : idleLabel || children);
}
Object.assign(__ds_scope, { GlowButton });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/buttons/GlowButton.jsx", error: String((e && e.message) || e) }); }

// components/controls/Dial.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useRef
} = React;
/**
 * Dial — compact arc control. A 270° SVG arc (rotated -135°) in a single
 * spectrum color over a faint track, with a filled core that scales with
 * the value. Drag vertically to set. Label + mono value sit beneath.
 */
function Dial({
  value = 0.5,
  // 0..1
  onChange,
  color = "var(--spectrum-violet)",
  label = "MAIN",
  size = 78,
  format = v => Math.round(v * 100) + "%",
  sensitivity = 200,
  style,
  ...rest
}) {
  const startRef = useRef(null);
  const R = 28;
  const ARC = 2 * Math.PI * R * 0.75; // 270deg
  const FULL = 2 * Math.PI * R;
  const clamp = x => Math.min(1, Math.max(0, x));
  const onPointerDown = e => {
    if (!onChange) return;
    e.preventDefault();
    startRef.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => onChange(clamp(startRef.current.v + (startRef.current.y - ev.clientY) * (1 / sensitivity) * 40));
    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      gap: "var(--space-5)",
      ...style
    }
  }, rest), /*#__PURE__*/React.createElement("div", {
    onPointerDown: onPointerDown,
    style: {
      width: size,
      height: size,
      cursor: onChange ? "ns-resize" : "default",
      touchAction: "none"
    }
  }, /*#__PURE__*/React.createElement("svg", {
    viewBox: "0 0 80 80",
    style: {
      width: "100%",
      height: "100%",
      overflow: "visible",
      transform: "rotate(135deg)"
    }
  }, /*#__PURE__*/React.createElement("circle", {
    cx: "40",
    cy: "40",
    r: R,
    fill: "rgba(8,11,18,0.6)",
    stroke: "var(--layer-5)",
    strokeWidth: "4",
    strokeLinecap: "round",
    strokeDasharray: `${ARC.toFixed(2)} ${FULL.toFixed(2)}`
  }), /*#__PURE__*/React.createElement("circle", {
    cx: "40",
    cy: "40",
    r: R,
    fill: "none",
    stroke: color,
    strokeWidth: "4",
    strokeLinecap: "round",
    strokeDasharray: `${(value * ARC).toFixed(2)} ${FULL.toFixed(2)}`,
    style: {
      filter: `drop-shadow(0 0 ${(3 + value * 6).toFixed(1)}px ${color})`
    }
  }), /*#__PURE__*/React.createElement("circle", {
    cx: "40",
    cy: "40",
    r: (4 + value * 5).toFixed(2),
    fill: color,
    style: {
      filter: `drop-shadow(0 0 6px ${color})`
    }
  }))), label && /*#__PURE__*/React.createElement("div", {
    style: {
      textAlign: "center"
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      fontFamily: "var(--font-display)",
      fontSize: "var(--fs-label-sm)",
      fontWeight: "var(--fw-semibold)",
      letterSpacing: "var(--tracking-data)",
      color: "var(--text-2)"
    }
  }, label), /*#__PURE__*/React.createElement("div", {
    style: {
      fontFamily: "var(--font-mono)",
      fontSize: "var(--fs-label-sm)",
      color,
      marginTop: 2
    }
  }, format(value))));
}
Object.assign(__ds_scope, { Dial });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Dial.jsx", error: String((e && e.message) || e) }); }

// components/controls/Knob.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useRef
} = React;
/**
 * Knob — the hero control. A 270° masked-donut spectrum ring tracks the
 * value, a recessed face carries a white indicator, and the center shows
 * a large numeric readout. Drag vertically to set (ns-resize).
 */
function Knob({
  value = 0.5,
  // 0..1
  onChange,
  size = 172,
  label = "AMOUNT",
  format = v => Math.round(v * 100),
  unit = "%",
  sensitivity = 220,
  // px of drag for full travel
  style,
  ...rest
}) {
  const startRef = useRef(null);
  const clamp = x => Math.min(1, Math.max(0, x));
  const onPointerDown = e => {
    if (!onChange) return;
    e.preventDefault();
    startRef.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => onChange(clamp(startRef.current.v + (startRef.current.y - ev.clientY) / sensitivity));
    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };
  const ringDeg = (value * 270).toFixed(1) + "deg";
  const knobDeg = (-135 + value * 270).toFixed(1) + "deg";
  const mask = "radial-gradient(circle, transparent 66px, #000 67px, #000 78px, transparent 79px)";
  const scale = size / 172;
  const inner = Math.round(24 * scale);
  return /*#__PURE__*/React.createElement("div", _extends({
    onPointerDown: onPointerDown,
    style: {
      position: "relative",
      width: size,
      height: size,
      cursor: onChange ? "ns-resize" : "default",
      touchAction: "none",
      ...style
    }
  }, rest), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      inset: 6,
      borderRadius: "50%",
      background: "conic-gradient(from 225deg, var(--layer-3) 0deg, var(--layer-3) 270deg, transparent 270deg)",
      WebkitMask: mask,
      mask,
      transform: scale !== 1 ? `scale(${scale})` : undefined
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      inset: 6,
      borderRadius: "50%",
      background: `conic-gradient(from 225deg, var(--spectrum-cyan), var(--spectrum-violet), var(--spectrum-pink), var(--spectrum-amber) ${ringDeg}, transparent ${ringDeg})`,
      WebkitMask: mask,
      mask,
      filter: "var(--glow-ring)",
      transform: scale !== 1 ? `scale(${scale})` : undefined
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      inset: inner,
      borderRadius: "50%",
      background: "var(--knob-face)",
      border: "var(--border-line)",
      boxShadow: "var(--shadow-knob)"
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      inset: inner,
      transform: `rotate(${knobDeg})`
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      left: "50%",
      top: 12,
      width: 9,
      height: 9,
      borderRadius: "50%",
      background: "#fff",
      transform: "translateX(-50%)",
      boxShadow: "var(--indicator-glow)"
    }
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      inset: 0,
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      justifyContent: "center",
      pointerEvents: "none",
      fontFamily: "var(--font-display)"
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: Math.round(38 * scale),
      fontWeight: "var(--fw-semibold)",
      letterSpacing: "var(--tracking-display)",
      lineHeight: 1,
      color: "var(--text-1)"
    }
  }, format(value), /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: Math.round(16 * scale),
      color: "var(--text-3)"
    }
  }, unit)), label && /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: Math.round(10 * scale),
      letterSpacing: "var(--tracking-wide)",
      color: "var(--text-muted)",
      marginTop: 4
    }
  }, label)));
}
Object.assign(__ds_scope, { Knob });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Knob.jsx", error: String((e && e.message) || e) }); }

// components/controls/Meter.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * Meter — a thin progress bar carrying the full spectrum ramp with a warm
 * glow halo. Read-only level indicator (output, energy, tape speed).
 */
function Meter({
  value = 0.5,
  // 0..1
  height = 6,
  gradient = "var(--spectrum-ramp)",
  glow = true,
  style,
  ...rest
}) {
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      position: "relative",
      height,
      borderRadius: height / 2,
      background: "var(--track)",
      overflow: "hidden",
      ...style
    }
  }, rest), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      left: 0,
      top: 0,
      bottom: 0,
      width: (Math.min(1, Math.max(0, value)) * 100).toFixed(1) + "%",
      borderRadius: height / 2,
      background: gradient,
      boxShadow: glow ? "var(--glow-meter)" : "none"
    }
  }));
}
Object.assign(__ds_scope, { Meter });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Meter.jsx", error: String((e && e.message) || e) }); }

// components/controls/Slider.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useRef
} = React;
/**
 * Slider — a thin rail with a gradient fill and a round handle. Drag
 * horizontally to set. Label + mono value sit in a row above the rail.
 */
function Slider({
  value = 0.5,
  // 0..1
  onChange,
  label,
  valueLabel,
  gradient = "var(--ramp-cool)",
  style,
  ...rest
}) {
  const railRef = useRef(null);
  const clamp = x => Math.min(1, Math.max(0, x));
  const pct = (value * 100).toFixed(1) + "%";
  const onPointerDown = e => {
    if (!onChange) return;
    e.preventDefault();
    const rect = e.currentTarget.getBoundingClientRect();
    const set = ev => onChange(clamp((ev.clientX - rect.left) / rect.width));
    set(e);
    const move = ev => set(ev);
    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };
  return /*#__PURE__*/React.createElement("div", _extends({
    style: style
  }, rest), (label || valueLabel) && /*#__PURE__*/React.createElement("div", {
    style: {
      display: "flex",
      justifyContent: "space-between",
      fontSize: "var(--fs-label-sm)",
      marginBottom: 8
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      color: "var(--text-label)",
      letterSpacing: "0.08em",
      fontFamily: "var(--font-display)"
    }
  }, label), /*#__PURE__*/React.createElement("span", {
    style: {
      fontFamily: "var(--font-mono)",
      color: "var(--text-2)"
    }
  }, valueLabel)), /*#__PURE__*/React.createElement("div", {
    ref: railRef,
    onPointerDown: onPointerDown,
    style: {
      position: "relative",
      height: 18,
      display: "flex",
      alignItems: "center",
      cursor: onChange ? "ew-resize" : "default",
      touchAction: "none"
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      left: 0,
      right: 0,
      height: "var(--slider-track)",
      borderRadius: 2,
      background: "var(--track)"
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      left: 0,
      height: "var(--slider-track)",
      width: pct,
      borderRadius: 2,
      background: gradient
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: "absolute",
      left: pct,
      width: "var(--slider-handle)",
      height: "var(--slider-handle)",
      borderRadius: "50%",
      background: "var(--text-1)",
      boxShadow: "var(--shadow-handle)",
      transform: "translateX(-50%)"
    }
  })));
}
Object.assign(__ds_scope, { Slider });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Slider.jsx", error: String((e && e.message) || e) }); }

// components/surfaces/Keyboard.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useMemo
} = React;
/**
 * Keyboard — a playable octave-tiling keyboard. White keys flex to fill;
 * black keys overlay at 62% width. Pressing a key swaps its fill, lights
 * a colored glow and depresses it 2px. Reports MIDI-ish note indices.
 */
function Keyboard({
  whites = 15,
  whiteFill = "#f8f5ef",
  whitePress = "#e7d8bd",
  blackFill = "#1a1714",
  blackPress = "#473d31",
  accent = "#e0a96d",
  keyBorder = "rgba(0,0,0,0.16)",
  onPress,
  onRelease,
  style,
  ...rest
}) {
  const N = Math.max(7, Math.round(whites));
  const ww = 100 / N;
  const whiteSemis = [0, 2, 4, 5, 7, 9, 11];
  const {
    white,
    black
  } = useMemo(() => {
    const white = [];
    for (let i = 0; i < N; i++) {
      white.push({
        idx: Math.floor(i / 7) * 12 + whiteSemis[i % 7],
        style: {
          flex: "1 1 0",
          height: "100%",
          background: whiteFill,
          borderRight: `1px solid ${keyBorder}`,
          borderRadius: "0 0 4px 4px",
          cursor: "pointer",
          boxShadow: "inset 0 -7px 9px -7px rgba(0,0,0,.22)",
          transition: "transform .04s, box-shadow .08s, background .08s"
        }
      });
    }
    const bw = ww * 0.62;
    const black = [];
    for (let i = 0; i < N - 1; i++) {
      if ([0, 1, 3, 4, 5].includes(i % 7)) {
        black.push({
          idx: Math.floor(i / 7) * 12 + whiteSemis[i % 7] + 1,
          style: {
            position: "absolute",
            top: 0,
            height: "61%",
            zIndex: 2,
            left: ((i + 1) * ww - bw / 2).toFixed(3) + "%",
            width: bw.toFixed(3) + "%",
            background: blackFill,
            borderRadius: "0 0 3px 3px",
            cursor: "pointer",
            boxShadow: "0 3px 4px rgba(0,0,0,.4), inset 0 -3px 5px rgba(255,255,255,.07)",
            transition: "transform .04s, box-shadow .08s, background .08s"
          }
        });
      }
    }
    return {
      white,
      black
    };
  }, [N, whiteFill, blackFill, keyBorder, ww]);
  const press = (base, pressCol, idx) => e => {
    const el = e.currentTarget;
    el.style.background = pressCol;
    el.style.boxShadow = `0 0 16px ${accent}, inset 0 0 10px ${accent}`;
    el.style.transform = "translateY(2px)";
    onPress && onPress(idx);
  };
  const release = (base, idx) => e => {
    const el = e.currentTarget;
    el.style.background = base;
    el.style.boxShadow = "";
    el.style.transform = "none";
    onRelease && onRelease(idx);
  };
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      position: "relative",
      display: "flex",
      width: "100%",
      height: "100%",
      borderRadius: 3,
      overflow: "hidden",
      ...style
    }
  }, rest), white.map((w, i) => /*#__PURE__*/React.createElement("div", {
    key: "w" + i,
    style: w.style,
    onPointerDown: press(whiteFill, whitePress, w.idx),
    onPointerUp: release(whiteFill, w.idx),
    onPointerLeave: release(whiteFill, w.idx)
  })), black.map((b, i) => /*#__PURE__*/React.createElement("div", {
    key: "b" + i,
    style: b.style,
    onPointerDown: press(blackFill, blackPress, b.idx),
    onPointerUp: release(blackFill, b.idx),
    onPointerLeave: release(blackFill, b.idx)
  })));
}
Object.assign(__ds_scope, { Keyboard });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/surfaces/Keyboard.jsx", error: String((e && e.message) || e) }); }

// components/surfaces/Panel.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * Panel — the dark-glass shell every product lives in. A radial well with
 * a hairline border and a deep outer drop shadow. Holds the whole plugin
 * face; pads at 26px by default.
 */
function Panel({
  children,
  width,
  pad = 26,
  radius = 16,
  style,
  ...rest
}) {
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      width,
      borderRadius: radius,
      padding: pad,
      background: "var(--panel)",
      border: "var(--border-hairline)",
      boxShadow: "var(--shadow-panel)",
      color: "var(--text-1)",
      fontFamily: "var(--font-display)",
      boxSizing: "border-box",
      ...style
    }
  }, rest), children);
}
Object.assign(__ds_scope, { Panel });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/surfaces/Panel.jsx", error: String((e && e.message) || e) }); }

// components/surfaces/Wordmark.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
/**
 * Wordmark — the house brand "Zanders" in primary text set tight against
 * the product name, which takes one spectrum color and NO space between.
 */
function Wordmark({
  product = "PreDrop",
  color = "var(--spectrum-pink)",
  size = 17,
  style,
  ...rest
}) {
  return /*#__PURE__*/React.createElement("div", _extends({
    style: {
      fontFamily: "var(--font-display)",
      fontSize: size,
      fontWeight: "var(--fw-semibold)",
      letterSpacing: "var(--tracking-title)",
      color: "var(--text-1)",
      ...style
    }
  }, rest), "Zanders", /*#__PURE__*/React.createElement("span", {
    style: {
      color
    }
  }, product));
}
Object.assign(__ds_scope, { Wordmark });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/surfaces/Wordmark.jsx", error: String((e && e.message) || e) }); }

__ds_ns.Badge = __ds_scope.Badge;

__ds_ns.Chip = __ds_scope.Chip;

__ds_ns.GlowButton = __ds_scope.GlowButton;

__ds_ns.Dial = __ds_scope.Dial;

__ds_ns.Knob = __ds_scope.Knob;

__ds_ns.Meter = __ds_scope.Meter;

__ds_ns.Slider = __ds_scope.Slider;

__ds_ns.Keyboard = __ds_scope.Keyboard;

__ds_ns.Panel = __ds_scope.Panel;

__ds_ns.Wordmark = __ds_scope.Wordmark;

})();
