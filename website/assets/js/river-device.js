/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River and the FM-1 (every page): fm1-base's protocol (PB, Link, FW: inline in the page) over Web MIDI, as its
 * own backup page drives it; one conversation at a time. The dialogs are titled as the FM-1 titles a page, and every
 * state of the cable is told by one drawing: the laptop, the USB-C cable (the river), the FM-1 (its own screens, its
 * buttons lit as it lights them: SEL is yes or OK, FX is no).
 *   RiverDevice.backup(voice)            the FM-1's backup, read and checked -> {image, flags, scenes, settings, ...}
 *   RiverDevice.read()                   the same for a dialog of its own: no voice (errors thrown, each with a code)
 *   RiverDevice.place(shards, o)         SEND TO FM-1: shards into the slots chosen (the FM-1 read by itself on opening,
 *                                        read again just before sending, FB.patch, a confirmation, one question on the
 *                                        FM-1): shards [{name, bytes, slot (where it came from, or null), state}];
 *                                        o {choose: [{key, label, items: [{sc, desc, date}]}] (nothing selected yet: a
 *                                        first step picks up to 16), inRiver(scene) -> true, false or undefined (a slot
 *                                        replaced: is that scene in one's River?), onSent(places)}
 *   RiverDevice.restore(image, label, o) a whole backup back (the same dialog's states); o {onDone()}
 *   RiverDevice.sheet(id, o) · draw(sheet, {title, sub, size, body, foot}) · btn(word, onclick, o) · installBtn() ·
 *   stage(drawing, say) · trouble(error) · drawing(state) · waterline(pct) · deviceLine(fm)   (Get from FM-1's dialog)
 *   RiverDevice.voice(box, line)         what is happening, in words (say(parts, kind): text and nodes, never HTML)
 *   RiverDevice.busy · onBusy(fn) · onChanged(fn) (something was written to the FM-1) · lastRead · version */
"use strict";
window.RiverDevice = (() => {
  const { h, s, fill, two, shard, listWords, key, k, WORD, I, look } = RiverCards;
  const S = FB.S, N = S.sizes, ID = S.ids;
  const clock = (d) => d.toLocaleTimeString("en-GB", { hour: "2-digit", minute: "2-digit" });
  const hasMidi = !!navigator.requestMIDIAccess;
  const dialogOpen = () => !!document.querySelector("dialog[open]");
  const reduce = RiverCards.reduce;
  const coarse = matchMedia("(max-width: 760px)");
  const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;
  const installUrl = () => (document.getElementById("main") || document.body).dataset.install || "https://frasilsynth.com/guide/install/";

  // ---------------------------------------------------------------- one conversation at a time
  let busy = false, lastRead = null, version = FW.name;
  const busyFns = [], changedFns = [];
  function setBusy(on) {
    busy = on;
    document.body.classList.toggle("is-busy", on);
    paint();
    busyFns.forEach((f) => f(on));
  }
  addEventListener("beforeunload", (e) => { if (busy) { e.preventDefault(); e.returnValue = ""; } });

  // ---------------------------------------------------------------- what is happening, in words and on a waterline
  function voice(box, line) {
    const sayEl = box.querySelector(".voice-say"), alarm = box.querySelector(".voice-alarm");
    line = line || box.querySelector(".voice-line");
    const bar = line && line.querySelector("i");
    let lastSaid = 0, lastPct = -100;
    const v = {
      say(parts, kind = "") {
        const p = [].concat(parts == null ? [] : parts);
        fill(sayEl, ...(kind === "err" ? [] : p));
        fill(alarm, ...(kind === "err" ? p : []));
        box.dataset.kind = kind;
        lastSaid = performance.now();
      },
      /* progress in words at most every 2 s, or every 10 percent (a screen reader reads each change) */
      progress(parts, pct) {
        const now = performance.now();
        if (now - lastSaid < 2000 && !(pct != null && pct - lastPct >= 10)) return;
        if (pct != null) lastPct = pct;
        v.say(parts, "work");
      },
      flow() { if (line) { line.dataset.mode = "flow"; bar.style.transform = ""; } },
      pct(x) { if (line) { line.dataset.mode = "pct"; bar.style.transform = `scaleX(${Math.max(0, Math.min(1, x / 100))})`; } },
      still() { if (line) { delete line.dataset.mode; bar.style.transform = ""; } lastPct = -100; },
      clear() { v.still(); v.say("", ""); },
    };
    return v;
  }
  /* a restore the FM-1 refused, in words (its code) */
  function refused(code) {
    if (code === PB.PLAYING) return [k("STOP FIRST"), ": something still plays on the FM-1, a key held down maybe. Let go of it, or hold ", key("PLAY"), ", then try again. Nothing changed."];
    if (code === PB.DECLINED) return ["Not restored: ", key("FX"), " was pressed on the FM-1, or nobody answered within a minute. Nothing changed."];
    return [`Not restored. There was a problem with ${PB.ERRORS[code] || "code " + code}.` + (code === 12 ? "" : " Nothing changed. Try again.")];
  }
  const PLAYING_NOTE = () => [`It was taken while ${FW.name} was playing: changes not stored yet are not in it. To have them, hold `,
                              key("PLAY"), " to release everything, then back up again."];

  // ---------------------------------------------------------------- the FM-1 (as fm1-base's page reaches it)
  let access = null;
  const oops = (code, msg) => Object.assign(new Error(msg), { code });
  /* FRASIL's version from the FM-1's identity: a release's FM-1_47XXYYZZ -> FRASIL X.Y(.Z) */
  const versionOf = (id) => {
    const t = String(id || "").slice(PB.ID.length);
    return /^\d{6}$/.test(t) ? `${FW.name} ${+t.slice(0, 2)}.${+t.slice(2, 4)}${+t.slice(4) ? "." + +t.slice(4) : ""}` : FW.name;
  };
  async function connect(vc) {
    if (!hasMidi) throw oops("nomidi", "This browser has no Web MIDI. Open the River in Chrome or Edge on a computer to reach the FM-1.");
    if (!access) {
      try { access = await navigator.requestMIDIAccess({ sysex: true }); }
      catch (_) { throw oops("nosysex", "MIDI with SysEx was not allowed. Allow MIDI in the site settings (the address bar), then try again."); }
    }
    if (vc) vc.say(`Looking for ${FW.name}…`, "work");
    const dev = await PB.find(access, (i, o) => new Link(i, o));
    version = versionOf(dev.id);
    return dev;
  }
  /* the FM-1's backup, read and checked (onBytes(n): how much came) */
  async function readFM(dev, onBytes) {
    const { image, flags } = await PB.backup(dev.link, onBytes || (() => {}));
    const parsed = PB.parse(image);
    const fm = { image, ...FB.read(parsed), flags: (flags | parsed.flags) & 0xFF, at: new Date() };
    lastRead = fm;
    return fm;
  }
  /* everything let go first: MIDI's All Sound Off (CC 120) on every channel, FRASIL's RELEASE ALL */
  async function letGo(dev) {
    for (let ch = 0; ch < 16; ch++) dev.link.send([0xB0 | ch, 120, 0]);
    await new Promise((r) => setTimeout(r, 150));
  }
  /* the FM-1's backup for slots to be filled: its scenes still in an earlier FRASIL's layout, read while it plays (after
   * an update, nothing stored yet): everything let go, read again (stopped, the FM-1 saves them in this one first) */
  async function readNow(dev, onBytes) {
    const fm = await readFM(dev, onBytes);
    if (fm.current || !(fm.flags & 1)) return fm;
    await letGo(dev);
    return readFM(dev, onBytes);
  }
  /* an image sent (everything let go first, CC 120): onEvent("progress", pct) · onEvent("ask") -> 0 or the FM-1's code */
  async function sendImage(dev, image, onEvent) {
    await letGo(dev);
    return PB.restore(dev.link, image, onEvent);
  }
  /* a backup of the FM-1, on a page's own voice (errors thrown, in plain words) */
  async function backup(vc, what = "Receiving the backup") {
    if (busy) throw new Error("The FM-1 is busy with something else: wait a moment.");
    setBusy(true);
    let dev = null;
    try {
      dev = await connect(vc);
      vc.say(`${what} from ${FW.name}…`, "work");
      vc.flow();
      const fm = await readFM(dev, (n) => vc.progress(`${what}: ${n.toLocaleString("en")} bytes…`));
      vc.still();
      return fm;
    } finally {
      if (dev) dev.link.close();
      setBusy(false);
    }
  }
  /* the FM-1 read for a dialog (Get from FM-1): thrown errors carry a code (trouble() says what it means) */
  async function read() {
    if (busy) throw oops("busy", "The FM-1 is busy with something else: wait a moment.");
    setBusy(true);
    let dev = null;
    try { dev = await connect(); return await readNow(dev); }
    finally { if (dev) dev.link.close(); setBusy(false); }
  }
  const changed = () => changedFns.forEach((f) => f());

  // ================================================================ the drawing: the laptop, the cable (the river), the FM-1
  const ICE = "#6ed0ff", LAMP = "#f2a65a", WHITE = "#f3f6f9", UI = "Barlow Semi Condensed, Arial Narrow, sans-serif";
  const PANEL = [["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]];
  /* the buttons lit: only the ones that answer (SEL yes / OK, FX no) */
  const LIT = { ask: ["FX", "SEL"], restored: ["SEL"], notrestored: ["SEL"], stopfirst: ["SEL"] };
  const NS = { "vector-effect": "non-scaling-stroke" };
  const R = (x, y, w, ht, rx, a) => s("rect", { x, y, width: w, height: ht, rx, ...a });
  const T = (x, y, t, a) => s("text", { x, y, "font-family": UI, ...a }, t);
  let did = 0;
  /* a glint: FRASIL's four-pointed star, glowing (its gradient's id) */
  const glint = (gid, k1 = 1) => s("g", { class: "ill-glint" }, s("circle", { r: 11 * k1, fill: `url(#${gid})` }),
    s("path", { transform: `translate(${-8 * k1 * 0.9} ${-8 * k1 * 0.9}) scale(${0.9 * k1})`, d: "M8 2.1L9.25 6.75L14.3 8L9.25 9.25L8 13.9L6.75 9.25L1.7 8L6.75 6.75Z", fill: WHITE }));

  /* the FM-1's screen (240 x 240, as the device draws it): its play page, the restore question, a pop-up, another
   * firmware's colours, dark */
  const POP = {
    ask: { t: "RESTORE?", c: LAMP, lines: ["IT REPLACES ALL SCENES", "AND THE SETTINGS."], ask: true },
    restored: { t: "RESTORED", c: ICE, lines: ["THE SCENES AND THE SETTINGS", "ARE BACK."] },
    notrestored: { t: "NOT RESTORED", c: LAMP, lines: ["FX, OR NO ANSWER IN A MINUTE.", "NOTHING CHANGED."] },
    stopfirst: { t: "STOP FIRST", c: LAMP, lines: ["SOMETHING IS PLAYING.", "HOLD PLAY TO RELEASE ALL,", "THEN RESTORE AGAIN."] },
  };
  function screen(state, o) {
    const sv = s("svg", { x: 361, y: 57, width: 240, height: 240, viewBox: "0 0 240 240" }, R(0, 0, 240, 240, 0, { fill: state === "off" ? "#030405" : "#0a0d12" }));
    const p = POP[state];
    if (p) {
      sv.append(s("circle", { cx: 17.5, cy: 46, r: 2, fill: p.c }), T(26, 50, "RESTORE FROM USB", { "font-size": 11.5, "font-weight": 600, "letter-spacing": 1, fill: "#8b95a4" }),
        T(14, 82, p.t, { "font-size": 30, "font-weight": 700, "letter-spacing": 0.6, fill: p.c, textLength: p.t.length > 9 ? 198 : null }),
        R(14, 100, 212, 1, 0, { fill: "#3a4654" }));
      p.lines.forEach((t, i) => sv.append(T(14, 128 + i * 20, t, { "font-size": 14.5, "font-weight": 500, "letter-spacing": 0.3, fill: "#dce3ea" })));
      const btn = (x, w, word, b, on, c) => s("g", null,
        R(x, 192, w, 34, 2, { fill: "none", stroke: on ? c : "#aeb7c2", "stroke-width": on ? 2 : 1.4 }),
        T(x + 14, 215, word, { "font-size": 15, "font-weight": 700, "letter-spacing": 1.2, fill: WHITE }),
        R(x + w - 40, 199, 33, 20, 1.5, { fill: "#151b23", stroke: "#4a5563", "stroke-width": 1 }),
        T(x + w - 23.5, 214, b, { "font-size": 13, "font-weight": 700, "letter-spacing": 0.8, fill: WHITE, "text-anchor": "middle" }));
      if (p.ask) sv.append(btn(14, 94, "NO", "FX", false), btn(120, 106, "YES", "SEL", true, ICE));
      else sv.append(btn(72, 96, "OK", "SEL", true, p.c));
    } else if (state === "nofrasil") {
      const COL = ["#c0392b", "#e67e22", "#27ae60", "#2980b9", "#8e44ad", "#16a085"];
      for (let i = 0; i < 4; i++) for (let j = 0; j < 3; j++) sv.append(R(18 + i * 54, 30 + j * 62, 46, 52, 4, { fill: COL[(i + j * 2) % 6], opacity: 0.16 }));
      sv.append(R(16, 70, 208, 100, 10, { fill: "#0a0d12", stroke: LAMP, "stroke-width": 1.6, ...NS }));
      (o.plate || ["FRASIL", "NOT INSTALLED"]).forEach((t, i) => sv.append(T(120, 112 + i * 34, t, { "text-anchor": "middle", "font-size": i ? 26 : 30, "font-weight": 700, "letter-spacing": i ? 2.4 : 3, fill: LAMP })));
    } else if (state !== "off") {                      // its play page, as FRASIL draws it (a drone named MIST)
      sv.append(T(14, 30, "MIST", { "font-size": 19, "font-weight": 700, "letter-spacing": 1.4, fill: WHITE }), R(62, 16, 3, 3, 0, { fill: LAMP }),
        T(76, 29, "D MINOR", { "font-size": 12, "font-weight": 600, "letter-spacing": 1, fill: "#4a5563" }),
        R(150, 15, 78, 18, 2, { fill: "none", stroke: "#aeb7c2", "stroke-width": 1.2 }), T(170, 29, "DRONE", { "font-size": 12.5, "font-weight": 700, "letter-spacing": 1, fill: WHITE }));
      for (let i = 0; i < 9; i++) {
        const y = 64 + i * 3.3 + Math.sin(i * 1.7) * 2;
        sv.append(R(40, y, 190, 0.8, 0, { fill: i % 3 ? "#4a5563" : "#aeb7c2", opacity: 0.8 }));
      }
      for (let i = 0; i < 26; i++) sv.append(R(46 + ((i * 37) % 180), 62 + ((i * 13) % 30), i % 4 ? 1.4 : 2.4, 1.4, 0, { fill: i % 5 ? ICE : WHITE, opacity: 0.85 }));
      sv.append(T(14, 76, "F3", { "font-size": 12, "font-weight": 600, fill: "#aeb7c2" }), T(14, 94, "G2", { "font-size": 12, "font-weight": 600, fill: "#aeb7c2" }));
      ["CHOIR", "PARTIALS", "BREATH", "WARMTH"].forEach((t, i) => {
        const y = 128 + i * 15;
        sv.append(s("circle", { cx: 19, cy: y - 4, r: 5, fill: ICE, opacity: 0.85 }), T(30, y, t, { "font-size": 11.5, "font-weight": 600, "letter-spacing": 1, fill: "#dce3ea" }),
          R(94, y - 5, [74, 56, 20, 8][i], 1.2, 0, { fill: "#aeb7c2" }), T(226, y, ["59", "70", "25", "14"][i], { "font-size": 11.5, "font-weight": 600, fill: "#dce3ea", "text-anchor": "end" }));
      });
      [["KEYS", "DRONE", 14], ["PRESET", "CUSTOM", 88], ["BODY", "100", 182]].forEach(([l, v, x]) => sv.append(
        s("circle", { cx: x + 5, cy: 196, r: 5, fill: LAMP, opacity: 0.85 }), T(x + 14, 200, l, { "font-size": 11.5, "font-weight": 600, "letter-spacing": 1, fill: "#aeb7c2" }),
        T(x, 226, v, { "font-size": 19, "font-weight": 600, "letter-spacing": 0.6, fill: WHITE })));
    }
    return sv;
  }
  /* the FM-1 in the guide's units (1180 x 790): o.labels names every button (the close-up); o.hand rings the button the
   * River waits for in lamp, the hand's colour */
  function fm1(state, o = {}) {
    const off = state === "off", stroke = off ? "#26303c" : "#3a4654", lit = LIT[state] || [];
    const g = s("g", { class: "fm1" }, R(8, 8, 1164, 774, 40, { fill: off ? "#0c1015" : "#111821", stroke: off ? "#222b36" : "#2c3744", "stroke-width": 1.2, ...NS }));
    const knob = (x, y, r) => [s("circle", { cx: x, cy: y, r, fill: "#161d26", stroke, "stroke-width": 1, ...NS }),
      s("line", { x1: x, y1: y - r + 6, x2: x, y2: y - r + 15, stroke: off ? "#3a4654" : "#aeb7c2", "stroke-width": 1.2, ...NS })];
    for (const [x, y] of [[118, 112], [250, 112], [118, 246], [250, 246]]) g.append(...knob(x, y, 26));
    for (const x of [76, 190]) g.append(R(x, 346, 102, 38, 9, { fill: "#18202a", stroke, "stroke-width": 1, ...NS }));
    g.append(R(350, 46, 262, 262, 26, { fill: "#06080c", stroke: "#2a3440", "stroke-width": 1, ...NS }), screen(state, o));
    [692, 812, 932, 1052].forEach((x, i) => {
      g.append(...knob(x, 112, 24));
      if (o.labels) g.append(T(x, 166, `K${i + 1}`, { "text-anchor": "middle", "font-size": 15, "font-weight": 600, "letter-spacing": 1.6, fill: "#4a5563" }));
    });
    g.append(R(636, 226, 508, 246, 18, { fill: "#0b1016" }));
    PANEL.forEach((rw, j) => rw.forEach((b, i) => {
      const x = 648 + i * 81, y = j ? 350 : 238, on = lit.includes(b);
      if (o.hand === b) g.append(R(x - 17, y - 17, 98, 74, 20, { class: "ill-hand", fill: "none", stroke: LAMP, "stroke-width": 1.2, opacity: 0.3, ...NS }),
                                 R(x - 9, y - 9, 82, 58, 15, { fill: LAMP, opacity: 0.1, stroke: LAMP, "stroke-width": 1.4, ...NS }));
      if (on) g.append(R(x - 8, y - 8, 80, 56, 14, { fill: WHITE, opacity: 0.07 }));
      g.append(R(x, y, 64, 40, 9, { fill: on ? "#2b3644" : "#18202a", stroke: on ? WHITE : stroke, "stroke-width": on ? 1.4 : 1, ...NS }));
      if (on || o.labels) g.append(T(x + 32, y + 26, b, { "text-anchor": "middle", "font-size": o.labels ? 16 : 19, "font-weight": 700, "letter-spacing": 1.5, fill: on ? WHITE : "#4a5563" }));
    }));
    g.append(R(40, 486, 1100, 282, 18, { fill: "#0b1016" }));
    for (const x of [93.0, 160.3, 227.5, 362.0, 429.3, 563.8, 631.0, 698.3, 832.8, 900.0, 1034.5]) g.append(R(x, 504, 52.5, 93.1, 26.2, { fill: "#141b23", stroke: off ? "#1f2731" : "#2f3a47", "stroke-width": 1, ...NS }));
    for (let i = 0; i < 16; i++) g.append(R(Math.round((59.4 + i * 67.25) * 100) / 100, 607.3, 52.5, 146.6, 26.2, { fill: off ? "#141a22" : "#1c2530", stroke: off ? "#222b36" : "#3a4654", "stroke-width": 1, ...NS }));
    if (off) g.append(R(-14, 380, 42, 80, 14, { class: "ill-port", fill: LAMP, opacity: 0.1 }), R(2, 400, 10, 40, 4, { fill: "#06080c", stroke: LAMP, "stroke-width": 1.4, ...NS }));
    else g.append(R(2, 400, 10, 40, 4, { fill: "#06080c", stroke, "stroke-width": 1, ...NS }));
    return g;
  }
  /* the River, tiny, on the laptop's screen */
  function miniRiver() {
    const g = s("g", { transform: "translate(17 91) scale(1.1) translate(-21 -95)" },
      R(27, 101, 16, 2.4, 1, { fill: "#3480ec" }), R(27, 104.6, 11, 1.6, 0.8, { fill: ICE, opacity: 0.7 }),
      R(112, 102, 12, 1.6, 0, { fill: "#77818f" }), R(128, 102, 12, 1.6, 0, { fill: "#dce3ea" }), R(144, 102, 9, 1.6, 0, { fill: "#77818f" }),
      s("line", { x1: 27, y1: 112.5, x2: 153, y2: 112.5, stroke: "#8b95a4", "stroke-width": 0.5, opacity: 0.6 }));
    for (let i = 0; i < 5; i++) {
      const y = 120 + i * 11.5;
      g.append(s("circle", { cx: 31, cy: y, r: 2.6, fill: "none", stroke: "#4a5563", "stroke-width": 0.7 }), R(37, y - 1.4, 18 + (i * 7) % 12, 2.2, 0.6, { fill: "#dce3ea", opacity: 0.75 }),
        s("line", { x1: 70, y1: y + 1, x2: 112, y2: y + 1, stroke: "#4a5563", "stroke-width": 0.5 }));
      for (const j of [0, 1, 2]) g.append(R(78 + ((i * 13 + j * 9) % 30), y - 3, 0.9, 4, 0, { fill: j ? "#dce3ea" : ICE, opacity: 0.8 }));
      g.append(R(120, y - 1, 16, 1.8, 0, { fill: "#77818f", opacity: 0.7 }), s("line", { x1: 27, y1: y + 5.6, x2: 153, y2: y + 5.6, stroke: "#1a212a", "stroke-width": 0.5 }));
    }
    return g;
  }
  /* the whole scene: state off | reading | sending | plain (connected, still) | nofrasil (o.plate: its words) */
  function drawing(state, o = {}) {
    const id = "ill" + (++did), SC = 0.36, OX = 268, OY = 8, port = [OX + 2 * SC, OY + 420 * SC];
    const svg = s("svg", { class: "ill is-" + state, viewBox: "0 0 700 296", "aria-hidden": "true", focusable: "false" });
    svg.append(s("g", null, R(10, 84, 166, 108, 7, { fill: "#0b1016", stroke: "#3a4654", "stroke-width": 1.2 }), R(17, 91, 152, 94, 2, { fill: "#080b0f" }), miniRiver(),
      s("path", { d: "M0 196 H186 L181 202 H5 Z", fill: "#111821", stroke: "#3a4654", "stroke-width": 1.1, "stroke-linejoin": "round" }),
      R(79, 192, 28, 4, 1.5, { fill: "#0b1016" }), R(183, 196.6, 5, 3, 1, { fill: "#06080c", stroke: "#3a4654", "stroke-width": 0.8 })));
    if (state === "off") {
      svg.append(s("path", { d: "M188 198 C 204 198, 204 228, 216 238 S 230 247, 238 247", fill: "none", stroke: "#3a4654", "stroke-width": 2, "stroke-linecap": "round" }),
        R(237, 242, 11, 10, 3, { fill: "#18202a", stroke: "#4a5563", "stroke-width": 1 }), R(248, 244, 9, 6, 2, { fill: "#26303c", stroke: "#77818f", "stroke-width": 0.9 }));
    } else {
      const flowing = state === "reading" || state === "sending", E = [port[0] - 24, port[1]];
      const d = `M188 198 C 208 198, 196 262, 218 262 C 240 262, 226 ${E[1]}, ${E[0]} ${E[1]}`;
      svg.append(s("defs", null,
        s("linearGradient", { id: `${id}-c`, x1: 0, x2: 1 }, s("stop", { offset: 0, "stop-color": "#4a5563", "stop-opacity": 0.7 }),
          s("stop", { offset: 0.5, "stop-color": flowing ? "#5d6e80" : "#77818f", "stop-opacity": 0.9 }), s("stop", { offset: 1, "stop-color": "#4a5563", "stop-opacity": 0.7 })),
        s("radialGradient", { id: `${id}-g` }, s("stop", { offset: 0, "stop-color": WHITE, "stop-opacity": 0.9 }), s("stop", { offset: 0.3, "stop-color": ICE, "stop-opacity": 0.55 }),
          s("stop", { offset: 1, "stop-color": ICE, "stop-opacity": 0 }))),
        s("path", { id: `${id}-p`, d, fill: "none", stroke: `url(#${id}-c)`, "stroke-width": 2, "stroke-linecap": "round" }),
        R(E[0], E[1] - 5, 12, 10, 3, { fill: "#18202a", stroke: "#4a5563", "stroke-width": 1 }), R(E[0] + 12, E[1] - 3, 12, 6, 1.6, { fill: "#26303c", stroke: "#77818f", "stroke-width": 0.9 }));
      if (flowing) {
        // the glint travels: from the FM-1 to the River while reading, the other way while sending; its wake behind it
        const back = state === "reading";
        svg.append(s("path", { class: "ill-wake" + (back ? " is-back" : ""), d, pathLength: 1, fill: "none", stroke: ICE, "stroke-width": 2.6, "stroke-linecap": "round" }));
        const g = glint(`${id}-g`, 1.15);
        if (reduce.matches) g.setAttribute("transform", back ? "translate(214 262)" : "translate(229 240)");
        else g.append(s("animateMotion", { dur: "2.2s", repeatCount: "indefinite", keyPoints: back ? "1;0" : "0;1", keyTimes: "0;1", calcMode: "linear" }, s("mpath", { href: `#${id}-p` })));
        svg.append(g);
      }
    }
    svg.append(s("g", { transform: `translate(${OX} ${OY}) scale(${SC})` }, fm1(state, o)));
    return svg;
  }
  /* the FM-1 close: its screen (the device's own, legible) and its buttons, named, lit as the device lights them.
   * state: ask | restored | notrestored | stopfirst */
  function closeUp(state, o = {}) {
    const id = "ill" + (++did), VX = 318, VW = 862, VH = 438;
    return s("svg", { class: "ill ill-close", viewBox: `${VX} 0 ${VW} ${VH}`, "aria-hidden": "true", focusable: "false" },
      s("defs", null, s("linearGradient", { id: `${id}-l`, x1: 0, x2: 1 }, s("stop", { offset: 0, "stop-color": "#0c1117" }), s("stop", { offset: 1, "stop-color": "#0c1117", "stop-opacity": 0 })),
        s("linearGradient", { id: `${id}-b`, x1: 0, x2: 0, y1: 0, y2: 1 }, s("stop", { offset: 0, "stop-color": "#0c1117", "stop-opacity": 0 }), s("stop", { offset: 1, "stop-color": "#0c1117" }))),
      fm1(state, { labels: true, hand: o.hand }),
      R(VX - 2, 0, 30, VH + 2, 0, { fill: `url(#${id}-l)` }), R(VX - 2, VH - 64, VW + 4, 66, 0, { fill: `url(#${id}-b)` }));
  }
  /* a dialog's stage: the drawing, then one status (a title in tracked capitals), a sentence, a quieter one */
  function stage(draw, say) {
    return h("div", { class: "dlg-stage" }, draw, sayBox(say));
  }
  function sayBox({ title, kind = "", text, small, line }) {
    return h("div", { class: "dlg-say", role: kind === "warn" ? "alert" : "status" },
      title ? h("h3", { class: kind || null }, title) : null, line || null,
      text ? h("p", null, text) : null, small ? h("p", { class: "small" }, small) : null);
  }
  /* progress on a waterline: flowing (reading), or a share of it (sending) */
  const waterline = (pct) => h("div", { class: "wl" + (pct == null ? " flow" : ""), "aria-hidden": "true" }, h("i", { style: pct == null ? null : `width:${pct}%` }));
  /* what went wrong reaching the FM-1, as one of the cable's states: {draw, title, text, small, install} */
  function trouble(e) {
    const c = e && e.code, m = (e && e.message) || "";
    if (c === "notfound" || c === "lost")
      return { draw: "off", title: "NOT CONNECTED", text: c === "lost" ? m : "Plug the FM-1 into this computer with a USB-C data cable.",
               small: "Another app holding the FM-1’s MIDI can hide it: close that app." };
    if (c === "model" || (c === "stopped" && m.startsWith(`${FW.name} didn't answer`)))
      return { draw: "nofrasil", title: `${FW.name} NOT INSTALLED`, text: `The FM-1 answers, but it runs another firmware, or a ${FW.name} older than 1.1.`, install: true };
    if (c === "layout")
      return { draw: "nofrasil", plate: ["UPDATE", FW.name], title: `UPDATE ${FW.name}`, text: m, install: true };
    if (c === "nosysex") return { draw: "off", title: "MIDI NOT ALLOWED", text: m };
    if (c === "nomidi") return { draw: "off", title: "NO WEB MIDI HERE", text: m };
    return { draw: "plain", title: "SOMETHING WENT WRONG", text: m || "The FM-1 didn’t answer as it should.", small: "Nothing changed on it. Try again." };
  }
  /* where the FM-1 stands, once read: FRASIL 1.1 · read just now */
  const deviceLine = (fm) => h("p", { class: "devl" }, version, " ", h("i", null, "· ", fm && Date.now() - fm.at < 60000 ? "read just now" : fm ? `read at ${clock(fm.at)}` : "not read yet"));

  // ================================================================ the dialogs' frame: titled as the FM-1 titles a page
  function frame(id, o = {}) {
    const t = h("h2", { class: "dlg-t", id: id + "-h", tabindex: "-1" });
    const x = h("button", { class: "sheet-x", type: "button", "aria-label": "Close" }, I.x());
    const body = h("div", { class: "dlg-b" }), foot = h("footer", { class: "dlg-f" });
    const dlg = h("dialog", { class: "sheet dlg", id, "aria-labelledby": id + "-h" }, h("header", { class: "dlg-h" }, t, x), body, foot);
    const F = { dlg, t, body, foot, x, closable: () => true };
    F.close = () => { if (F.closable()) dlg.close(); };
    x.addEventListener("click", F.close);
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); F.close(); });
    dlg.addEventListener("click", (e) => { if (e.target === dlg) F.close(); });   // (the backdrop)
    dlg.addEventListener("close", () => { if (o.onClose) o.onClose(); });
    document.body.append(dlg);
    return F;
  }
  /* a dialog drawn again: its title, its size, its body and its footer; focus on its main button (or its title) */
  function draw(F, { title, sub, size = "", body, foot = [], focus = true }) {
    fill(F.t, title, sub ? h("span", null, sub) : null);
    F.dlg.className = "sheet dlg" + (size ? " " + size : "");
    F.body.replaceChildren(...[].concat(body).filter(Boolean));
    F.foot.replaceChildren(...foot.filter(Boolean));
    F.foot.hidden = !foot.filter(Boolean).length;
    F.body.scrollTop = 0;
    if (focus && F.dlg.open) (F.foot.querySelector(".a-ice:not(:disabled)") || F.t).focus({ preventScroll: true });
  }
  const btn = (word, onclick, o = {}) => h("button", { type: "button", class: "a" + (o.ice ? " a-ice" : "") + (o.txt ? " a-txt" : ""), id: o.id || null, disabled: !!o.disabled,
                                                       "aria-label": o.aria || null, onclick }, o.icon || null, h("span", null, word));
  const installBtn = () => h("a", { class: "a", href: installUrl(), target: "_blank", rel: "noopener" }, I.ext(), h("span", null, `Install ${FW.name}`));
  function paint() {
    if (PL) {
      PL.F.x.disabled = !PL.F.closable();
      if (PL.st === "ready") {
        PL.box.querySelectorAll("select, .lnk").forEach((x) => { x.disabled = busy; });
        const n = PL.at.size, send = PL.F.foot.querySelector("#place-send");
        if (send) {
          send.disabled = busy || !PL.fm || !n;
          fill(send.querySelector("span"), n === 1 && PL.items.length === 1 ? "Send to FM-1" : `Send ${n || ""} to FM-1`.replace("  ", " "));
        }
      }
    }
    if (WH) WH.F.x.disabled = !WH.F.closable();
  }

  // ================================================================ SEND TO FM-1: shards into slots
  let PL = null;
  const DT = "application/x-frasil-shard";
  const sameBytes = (a, b) => !!a && !!b && a.length === b.length && a.every((x, i) => x === b[i]);
  const sameScene = (a, b) => !!a && !!b && a.state === b.state &&
    (a.state !== "saved" || (a.current && b.current ? sameBytes(a.bytes, b.bytes) : a.name === b.name));
  const hasType = (e, t) => Array.from((e.dataTransfer && e.dataTransfer.types) || []).includes(t);
  const sub = () => (PL.st === "choose" ? "CHOOSE UP TO 16" : plural(PL.items.length, "SHARD", "SHARDS"));

  function placeDialog() {
    if (PL) return PL;
    const P = { st: "", items: [], at: new Map(), fm: null, o: {}, picked: [], tab: 0, q: "", done: [], code: 0 };
    P.F = frame("place", { onClose: () => { if (!P.F.dlg.open) P.st = ""; } });   // (a close event comes late: opened again since?)
    P.F.closable = () => !busy || P.st === "reading";
    PL = P;
    return P;
  }
  /* shards [{name, bytes, slot, state, ...}] into the FM-1's slots: the dialog (none: o.choose's first step) */
  function place(items, o = {}) {
    if (busy) return;
    const P = placeDialog();
    P.o = o;
    P.items = items.filter((sc) => sc.bytes);
    P.at = new Map();
    P.done = [];
    P.picked = [];
    P.q = "";
    P.tab = 0;
    if (!P.items.length && !o.choose) return;
    if (!P.F.dlg.open) P.F.dlg.showModal();
    if (P.items.length) readGo(); else choose();
  }

  // ---------------------------------------------------------------- choose: up to 16 of one's shards and favorites
  function choose() {
    const P = PL;
    P.st = "choose";
    const lists = P.o.choose.filter((l) => l.items.length);
    if (!lists.length) { P.F.dlg.close(); return; }
    P.tab = Math.min(P.tab, lists.length - 1);
    const n = h("b", { id: "choose-n" }), go = btn("Choose their slots", () => { P.items = [...P.picked]; if (P.items.length) readGo(); }, { ice: true, id: "choose-go" });
    const said = h("span", { class: "ch-said", role: "status" });
    const rows = h("div", { class: "ch-rows", role: "group", "aria-label": "The shards" });
    const q = h("input", { type: "search", maxlength: 100, placeholder: "Find a shard: a name, a word", autocomplete: "off", spellcheck: "false", "aria-label": "Find a shard: a word of its name or its description" });
    q.value = P.q;
    const count = () => {
      fill(n, String(P.picked.length));
      go.disabled = !P.picked.length;
      fill(said, "");
    };
    const paintRows = () => {
      const words = P.q.toLowerCase().split(/\s+/).filter(Boolean);
      const shown = lists[P.tab].items.filter((it) => words.every((w) => `${it.sc.name}\n${it.desc || ""}`.toLowerCase().includes(w)));
      rows.replaceChildren(...(shown.length ? shown.map((it) => pickRow(it, () => count(), (t) => fill(said, t))) : [h("p", { class: "list-none" }, "No shard matches.")]));
    };
    let typing = 0;
    q.addEventListener("input", () => { clearTimeout(typing); typing = setTimeout(() => { P.q = q.value.trim().slice(0, 100); paintRows(); }, 200); });
    const tabs = lists.length > 1 ? h("div", { class: "ch-tabs", role: "group", "aria-label": "Show" }, lists.map((l, i) => h("button", {
      type: "button", "aria-pressed": String(i === P.tab), onclick: (e) => {
        P.tab = i;
        e.currentTarget.parentElement.querySelectorAll("button").forEach((b, j) => b.setAttribute("aria-pressed", String(j === i)));
        paintRows();
      } }, l.label, " ", h("i", null, String(l.items.length))))) : null;
    draw(P.F, {
      title: "SEND TO FM-1", sub: sub(), size: "dlg-wide", focus: false,
      body: [h("div", { class: "ch-bar" }, h("label", { class: "tb-q" }, I.q(), q), tabs),
        h("div", { class: "lh ch-lh", "aria-hidden": "true" }, h("span"), h("span", null, "NAME"), h("span", { class: "ch-ms" }, "WHAT PLAYS"), h("span", null, "GLINT"), h("span", null, "BASS"), h("span", { class: "r" }, "ADDED")),
        rows],
      foot: [h("p", { class: "ch-n" }, n, " chosen ", h("span", { class: "dim" }, "· up to 16 at once, one per slot"), " ", said), btn("Cancel", () => P.F.close()), go],
    });
    paintRows();
    count();
    q.focus({ preventScroll: true });
  }
  /* a shard to choose: ✓, its shard and name (its description's first line), what plays, GLINT, BASS, the date */
  function pickRow(it, changed2, say) {
    const P = PL, sc = it.sc, L = sc.current ? look(sc.v) : null;
    const box = h("input", { type: "checkbox", checked: P.picked.includes(sc), "aria-label": `Choose ${sc.name}` });
    const row = h("label", { class: "ch-r" + (box.checked ? " is-on" : "") }, h("span", { class: "ck" }, box),
      h("span", { class: "nm" }, shard(sc, 22, true), h("span", { class: "nm-t" }, h("span", { class: "nm-n" }, sc.name), it.desc ? h("span", { class: "nm-d" }, it.desc) : null)),
      RiverCards.line(sc),
      L && L.type ? h("span", { class: "gt" }, L.txt(ID.P_SQ_TYPE)) : h("span", { class: "gt off", "aria-hidden": "true" }, "—"),
      sc.bseq && sc.bseq.on ? h("span", { class: "bq" }, "SEQ") : h("span"),
      h("span", { class: "dt" }, it.date || ""));
    box.addEventListener("change", () => {
      if (box.checked && P.picked.length >= N.NSCENES) { box.checked = false; say("16 at most: the FM-1 has sixteen slots."); return; }
      if (box.checked) P.picked.push(sc); else P.picked = P.picked.filter((x) => x !== sc);
      row.classList.toggle("is-on", box.checked);
      changed2();
    });
    return row;
  }

  // ---------------------------------------------------------------- reading the FM-1, by itself
  async function readGo() {
    const P = PL;
    if (busy) return;
    P.st = "reading";
    draw(P.F, { title: "SEND TO FM-1", sub: sub(), body: stage(drawing("reading"), { title: "READING THE FM-1", line: waterline(null), small: "To see what each of its sixteen slots holds now." }),
                foot: [btn("Cancel", () => P.F.close())] });
    setBusy(true);
    let dev = null, fm = null, err = null;
    try {
      dev = await connect();
      fm = await readNow(dev);
      try { FB.patch(fm.image, []); } catch (e) { throw oops("layout", e.message); }   // (an FM-1 keeping its scenes in another layout)
    } catch (e) { err = e; }
    finally { if (dev) dev.link.close(); setBusy(false); }
    if (P.st !== "reading" || !P.F.dlg.open) return;   // (closed meanwhile)
    if (err) { troubleShow(err, readGo); return; }
    P.fm = fm;
    P.moved = null;
    ready();
  }
  function troubleShow(e, retry) {
    const P = PL, t = trouble(e);
    P.st = "trouble";
    draw(P.F, { title: "SEND TO FM-1", sub: sub(), body: stage(drawing(t.draw, { plate: t.plate }), { title: t.title, kind: "warn", text: t.text, small: t.small }),
                foot: [t.install ? installBtn() : null, btn("Try again", retry, { ice: true, id: "place-again" })] });
  }

  // ---------------------------------------------------------------- ready: the cards, the FM-1's sixteen slots
  function ready() {
    const P = PL, n = P.items.length;
    P.st = "ready";
    P.list = h("ul", { class: "cds", id: "place-list", role: "list" });
    P.tiles = h("div", { class: "tiles", id: "slots", role: "list", "aria-label": "The FM-1’s sixteen slots" });
    P.sum = h("ul", { class: "p-sum", id: "place-sum", role: "list", "aria-label": "What will change" });
    P.alarm = h("p", { class: "p-alarm", id: "place-alarm", role: "alert" });
    const back = P.items.some((sc) => sc.slot != null)
      ? h("p", { class: "p-back" }, h("button", { type: "button", class: "lnk", id: "place-origin", onclick: () => P.items.forEach((sc, i) => { if (sc.slot != null) assign(i, sc.slot); }) }, "Back where they came from")) : null;
    P.sh = h("div", { class: "p-sh" }, h("div", { class: "p-cap" }, h("p", { class: "p-cap-t" }, "THE SHARDS", h("i", null, String(n)))), P.list, back);
    P.box = h("div", { class: "place2" }, P.sh,
      h("div", { class: "p-fm" }, h("div", { class: "p-cap" }, h("p", { class: "p-cap-t" }, "THE FM-1", h("i", null, "16 SLOTS")), deviceLine(P.fm),
        h("button", { type: "button", class: "lnk", id: "place-read", onclick: () => readGo() }, "Read again")), P.tiles));
    P.list.replaceChildren(...P.items.map((sc, i) => h("li", { class: "cd", draggable: "true", "data-i": i, ondragstart: (e) => dragStart(e, i), ondragend: dragEnd },
      h("div", { class: "cd-top" }, h("span", { class: "grip", "aria-hidden": "true" }), shard(sc, 22, true), h("span", { class: "cd-n" }, sc.name),
        sc.slot != null ? h("span", { class: "cd-from" }, `FROM ${two(sc.slot + 1)}`) : null),
      h("label", { class: "pick" }, h("span", { class: "sr-only" }, `Slot for ${sc.name}`),
        h("select", { onchange: (e) => assign(i, e.target.value === "" ? null : +e.target.value) }), I.dn()))));
    P.tiles.replaceChildren(...Array.from({ length: N.NSCENES }, (_, i) => h("div", { class: "tile", role: "listitem", "data-slot": i,
      ondragover: (e) => { if (!busy && hasType(e, DT)) { e.preventDefault(); e.dataTransfer.dropEffect = "move"; if (!e.currentTarget.classList.contains("over")) { e.currentTarget.classList.add("over"); paintTile(i, P.dragging); } } },
      ondragleave: (e) => { if (!e.currentTarget.contains(e.relatedTarget)) { e.currentTarget.classList.remove("over"); paintTile(i); } },
      ondrop: (e) => { e.preventDefault(); e.currentTarget.classList.remove("over"); const j = +e.dataTransfer.getData(DT); if (!busy && P.items[j]) assign(j, i); } })));
    P.sh.addEventListener("dragover", (e) => { if (!busy && hasType(e, DT)) { e.preventDefault(); P.sh.classList.add("over"); } });
    P.sh.addEventListener("dragleave", (e) => { if (!P.sh.contains(e.relatedTarget)) P.sh.classList.remove("over"); });
    P.sh.addEventListener("drop", (e) => { if (!hasType(e, DT)) return; e.preventDefault(); P.sh.classList.remove("over"); const i = +e.dataTransfer.getData(DT); if (P.items[i]) assign(i, null); });
    const lead = n === 1 ? ["Drag the shard onto a slot, or choose its slot.", "Choose its slot on the FM-1."] : ["Drag each shard onto a slot, or choose its slot.", "Choose a slot for each shard."];
    draw(P.F, {
      title: "SEND TO FM-1", sub: sub(), size: "dlg-place", focus: false,
      body: [h("p", { class: "p-lead" }, h("span", { class: "on-desk" }, lead[0]), h("span", { class: "on-phone" }, lead[1])), P.box, P.alarm, P.sum],
      foot: [h("p", { class: "p-stay" }, "The other slots and the settings stay as they are."), btn("Cancel", () => P.F.close()),
             btn("Send to FM-1", confirmSend, { ice: true, id: "place-send", icon: I.send(15) })],
    });
    options();
    placePaint();
    fill(P.alarm, ...(P.moved || []));
    const first = coarse.matches ? P.list.querySelector("select") : P.F.t;
    first.focus({ preventScroll: true });
  }
  function options() {
    const fm = PL.fm.scenes;
    PL.list.querySelectorAll("select").forEach((sel) => {
      const v = sel.value;
      sel.replaceChildren(h("option", { value: "" }, "Choose a slot"), ...Array.from({ length: N.NSCENES }, (_, i) =>
        h("option", { value: i }, `Slot ${two(i + 1)} · ${fm[i].state === "empty" ? "empty" : fm[i].name}`)));
      sel.value = v;
    });
  }
  const whoIn = (slot) => { for (const [i, at] of PL.at) if (at === slot) return i; return -1; };
  function assign(i, slot) {
    if (slot == null) PL.at.delete(i);
    else { for (const [j, at] of [...PL.at]) if (at === slot && j !== i) PL.at.delete(j); PL.at.set(i, slot); }
    PL.moved = null;
    if (PL.alarm) fill(PL.alarm);
    placePaint();
  }
  /* a slot: its number, its state or the shard going in, what happens to what is there (over: a card dragged onto it) */
  function paintTile(i, over) {
    const P = PL, el = P.tiles.children[i], now = P.fm.scenes[i], j = whoIn(i), sc = j >= 0 ? P.items[j] : null;
    const was = now.state === "empty" ? "INTO AN EMPTY SLOT" : `REPLACES ${now.name}`;
    el.className = `tile is-${now.state}` + (sc ? " is-placed" : "") + (over != null ? " over" : "");
    el.replaceChildren(h("span", { class: "t-top" }, h("span", { class: "t-no" }, two(i + 1)),
      sc ? shard(sc, 17, true) : h("span", { class: "t-st" }, now.state === "empty" ? "" : WORD[now.state])),
      sc ? h("span", { class: "t-n", draggable: "true", ondragstart: (e) => dragStart(e, j), ondragend: dragEnd }, sc.name)
         : h("span", { class: "t-n" }, now.state === "empty" ? "EMPTY" : now.name),
      ...((sc || over != null) ? [h("span", { class: "t-was" }, was)] : []));
    el.setAttribute("aria-label", `Slot ${i + 1}: ${now.state === "empty" ? "empty" : `${now.name}, ${now.state}`}` + (sc ? `. ${sc.name} goes here.` : ""));
  }
  /* where the summary says a scene replaced is: in one's River, or not (o.inRiver) */
  const river = (sc) => (PL.o.inRiver && sc.state === "saved" ? PL.o.inRiver(sc) : undefined);
  function placePaint() {
    const P = PL, fm = P.fm.scenes;
    P.list.querySelectorAll(".cd").forEach((li, i) => {
      const at = P.at.has(i) ? P.at.get(i) : null;
      li.querySelector("select").value = at == null ? "" : String(at);
      li.classList.toggle("is-placed", at != null);
    });
    for (let i = 0; i < N.NSCENES; i++) paintTile(i);
    const placed = [...P.at].sort((a, b) => a[1] - b[1]);
    P.sum.replaceChildren(...placed.map(([i, slot]) => {
      const sc = P.items[i], now = fm[slot], home = sc.slot === slot ? ": back where it came from" : "", inR = river(now);
      const tail = now.state === "empty" ? [`, empty now${home}.`] : now.state === "factory" ? [`, in place of the factory’s `, k(now.name), `${home}.`]
                 : [", in place of ", k(now.name), inR === true ? ": it is in your River." : inR === false ? ": it isn’t in your River." : `${home}.`];
      return h("li", null, shard(sc, 16, true), h("span", null, k(sc.name), ` goes into slot ${two(slot + 1)}`, ...tail));
    }), ...P.items.map((sc, i) => (P.at.has(i) ? null : h("li", { class: "sum-left" }, h("span", null, k(sc.name), placed.length ? " is not placed yet: it stays here." : " is not placed yet.")))).filter(Boolean));
    paint();
  }
  function dragStart(e, i) {
    if (busy) { e.preventDefault(); return; }
    e.dataTransfer.setData(DT, String(i));
    e.dataTransfer.setData("text/plain", PL.items[i].name);
    e.dataTransfer.effectAllowed = "move";
    PL.dragging = i;
    document.body.classList.add("is-dragging");
    requestAnimationFrame(() => { const li = PL.list.children[i]; if (li) li.classList.add("lifted"); });
  }
  function dragEnd() {
    document.body.classList.remove("is-dragging");
    PL.dragging = null;
    PL.list.querySelectorAll(".lifted").forEach((x) => x.classList.remove("lifted"));
    PL.tiles.querySelectorAll(".over").forEach((x) => { x.classList.remove("over"); paintTile(+x.dataset.slot); });
    PL.sh.classList.remove("over");
  }

  // ---------------------------------------------------------------- the confirmation, before anything goes
  /* the slots replaced, in words: SLOW THAW in place of ABYSS, which is in your River; POLAR NIGHT into an empty slot */
  function replacedWords(places, fm) {
    const out = [], empty = places.filter((p) => fm[p.slot].state === "empty");
    places.filter((p) => fm[p.slot].state !== "empty").forEach((p, j) => {
      const now = fm[p.slot], inR = river(now);
      out.push(j ? "; " : "", k(p.name), " in place of ", now.state === "factory" ? "the factory’s " : "", k(now.name),
               inR === true ? ", which is in your River" : inR === false ? ", which isn’t in your River" : "");
    });
    if (empty.length) {
      if (out.length) out.push("; ");
      empty.forEach((p, j) => out.push(j ? (j === empty.length - 1 ? " and " : ", ") : "", k(p.name)));
      out.push(empty.length === 1 ? " into an empty slot" : " into empty slots");
    }
    return out;
  }
  function confirmSend() {
    const P = PL;
    if (busy || !P.at.size) return;
    const places = placesOf(), slots = places.map((p) => h("b", null, two(p.slot + 1)));
    const slotWords = slots.flatMap((b, j) => [j ? (j === slots.length - 1 ? " and " : ", ") : "", b]);
    const go = btn("Send", null, { ice: true, id: "confirm-send", icon: I.send(15) }), no = btn("Cancel");
    const dlg = h("dialog", { class: "confirm", id: "confirm", role: "alertdialog", "aria-labelledby": "confirm-h", "aria-describedby": "confirm-l" },
      h("div", { class: "confirm-in" },
        h("h3", { id: "confirm-h" }, places.length === 1 ? "SEND 1 SHARD" : `SEND ${places.length} SHARDS`, h("span", null, "NOW?")),
        h("ul", { id: "confirm-l" },
          h("li", { class: "warm" }, h("span", null, "What plays on the FM-1 stops.")),
          h("li", null, h("span", null, places.length === 1 ? "Slot " : "Slots ", ...slotWords, places.length === 1 ? " is " : " are ",
            places.every((p) => P.fm.scenes[p.slot].state === "empty") ? "filled: " : "replaced: ", ...replacedWords(places, P.fm.scenes), ".")),
          h("li", null, h("span", null, "The other slots and the settings stay.")),
          h("li", { class: "then" }, h("span", null, "Then the FM-1 asks once: answer ", k("YES"), " with SEL.")))),
      h("div", { class: "confirm-f" }, no, go));
    let yes = false;
    go.addEventListener("click", () => { yes = true; dlg.close(); });
    no.addEventListener("click", () => dlg.close());
    dlg.addEventListener("click", (e) => { if (e.target === dlg) dlg.close(); });
    dlg.addEventListener("close", () => { dlg.remove(); if (yes) sendGo(); else { const s2 = P.F.foot.querySelector("#place-send"); if (s2) s2.focus(); } });
    document.body.append(dlg);
    dlg.showModal();
    go.focus();
  }
  const placesOf = () => [...PL.at].sort((a, b) => a[1] - b[1]).map(([i, slot]) => ({ slot, bytes: PL.items[i].bytes, name: PL.items[i].name, item: PL.items[i] }));

  // ---------------------------------------------------------------- sending, the FM-1's question, its answer
  /* the cable's flow while a backup goes: SENDING · 64% on a waterline */
  function sendingShow(F, title, subT, pct) {
    draw(F, { title, sub: subT, focus: false,
              body: stage(drawing("sending"), { title: pct == null ? "SENDING" : `SENDING · ${Math.floor(pct)}%`, line: waterline(pct), small: "Keep the cable in until the FM-1 asks." }) });
  }
  function askShow(F, title, subT, what) {
    draw(F, { title, sub: subT, focus: false, body: stage(closeUp("ask", { hand: "SEL" }), {
      title: "PRESS SEL ON THE FM-1", kind: "warn",
      text: ["Sent and checked. ", k("YES"), ` with SEL ${what}; `, k("NO"), " with FX leaves the FM-1 as it was."],
      small: "Its other scenes and its settings go back exactly as they are. It waits a minute for an answer." }) });
    F.t.focus({ preventScroll: true });
  }
  /* the FM-1's answer: 0 done, DECLINED, PLAYING, another code */
  function answerShow(F, title, subT, code, done, again) {
    const idp = F.dlg.id, close = (ice) => btn("Close", () => F.close(), { ice, id: `${idp}-close` });
    if (code === 0) {
      draw(F, { title, sub: subT, body: stage(closeUp("restored"), { title: "DONE", kind: "ice", text: done, small: "SEL closes the FM-1’s message." }), foot: [close(true)] });
      return;
    }
    const sayIt = code === PB.DECLINED ? { title: "NOT SENT", text: "FX was pressed on the FM-1, or nobody answered within a minute. Nothing changed on it.", small: "SEL closes the FM-1’s message." }
      : code === PB.PLAYING ? { title: "SOMETHING IS STILL HELD", text: ["The River let go of all it could. A key is still down on the FM-1, or a note held by another MIDI device. Let go of it, or hold ", key("PLAY"), ", then send again."],
                                small: "Nothing changed. SEL closes the FM-1’s message." }
      : { title: "NOT SENT", text: refused(code), small: "SEL closes the FM-1’s message." };
    draw(F, { title, sub: subT, body: stage(closeUp(code === PB.PLAYING ? "stopfirst" : "notrestored"), { ...sayIt, kind: "warn" }),
              foot: [close(false), btn("Send again", again, { ice: true, id: `${idp}-again` })] });
  }
  async function sendGo() {
    const P = PL;
    if (busy || !P.at.size) return;
    P.st = "sending";
    P.moved = null;
    sendingShow(P.F, "SEND TO FM-1", sub(), null);
    setBusy(true);
    let dev = null, code = null, err = null, image = null;
    const places = placesOf();
    try {
      dev = await connect();
      const was = P.fm, fm = await readNow(dev);
      const moved = places.map((p) => p.slot).filter((t) => !sameScene(was.scenes[t], fm.scenes[t]));
      P.fm = fm;
      if (moved.length) {
        P.moved = [moved.length === 1 ? `Slot ${two(moved[0] + 1)} has` : `Slots ${listWords(moved.map((t) => two(t + 1)))} have`,
                   " changed on the FM-1 since it was read. Check what will be replaced, then send again. Nothing changed."];
      } else {
        try { image = FB.patch(fm.image, places); } catch (e) { throw oops("layout", e.message); }
        code = await sendImage(dev, image, (ev, pct) => {
          if (ev === "progress" && P.st === "sending") { const t = P.F.body.querySelector(".dlg-say h3"), w = P.F.body.querySelector(".wl"); if (t) t.textContent = `SENDING · ${Math.floor(pct)}%`; if (w) { w.classList.remove("flow"); w.firstChild.style.width = `${pct}%`; } }
          if (ev === "ask") { P.st = "ask"; askShow(P.F, "SEND TO FM-1", sub(), `puts ${places.length === 1 ? "the shard" : `the ${places.length} shards`} in`); }
        });
      }
    } catch (e) { err = e; }
    finally { if (dev) dev.link.close(); setBusy(false); }
    if (err) { troubleShow(err, sendGo); return; }
    if (P.moved) { ready(); return; }
    P.st = code === 0 ? "done" : "answered";
    if (code === 0) {
      P.fm = { image, ...FB.read(PB.parse(image)), flags: 0, at: new Date() };
      lastRead = P.fm;
      changed();
      if (P.o.onSent) P.o.onSent(places);
    }
    answerShow(P.F, "SEND TO FM-1", sub(), code,
      [...places.flatMap((p, j) => [j ? (j === places.length - 1 ? " and " : ", ") : "", k(p.name), ` in ${two(p.slot + 1)}`]), ". The other slots and the settings are as they were."],
      sendGo);
  }

  // ================================================================ everything back: a whole backup
  let WH = null;
  function wholeDialog() {
    if (WH) return WH;
    const W = { st: "", image: null, label: "", o: {} };
    W.F = frame("whole", { onClose: () => { if (!W.F.dlg.open) W.st = ""; } });
    W.F.closable = () => !busy;
    WH = W;
    return W;
  }
  function restore(image, label, o = {}) {
    if (busy || !image) return;
    const W = wholeDialog();
    W.image = image;
    W.label = label || "This backup";
    W.o = o;
    wholeAsk();
    if (!W.F.dlg.open) W.F.dlg.showModal();
    (W.F.foot.querySelector(".a-ice") || W.F.t).focus();
  }
  function wholeAsk() {
    const W = WH;
    W.st = "confirm";
    draw(W.F, { title: "RESTORE IT ALL", focus: false, body: [
      h("p", { class: "p-lead" }, h("b", { id: "whole-name" }, W.label), " goes back on the FM-1, whole."),
      h("ul", { class: "confirm-l" },
        h("li", { class: "warm" }, h("span", null, "What plays on the FM-1 stops.")),
        h("li", null, h("span", null, "Its sixteen slots and its settings are replaced, the sound at power-on with them.")),
        h("li", { class: "then" }, h("span", null, "Then the FM-1 asks once: answer ", k("YES"), " with SEL.")))],
      foot: [btn("Cancel", () => W.F.close()), btn("Restore it all", wholeGo, { ice: true, id: "whole-send", icon: I.send(15) })] });
  }
  async function wholeGo() {
    const W = WH;
    if (busy || !W.image) return;
    W.st = "sending";
    sendingShow(W.F, "RESTORE IT ALL", null, null);
    setBusy(true);
    let dev = null, code = null, err = null;
    try {
      dev = await connect();
      code = await sendImage(dev, W.image, (ev, pct) => {
        if (ev === "progress" && W.st === "sending") { const t = W.F.body.querySelector(".dlg-say h3"), w = W.F.body.querySelector(".wl"); if (t) t.textContent = `SENDING · ${Math.floor(pct)}%`; if (w) { w.classList.remove("flow"); w.firstChild.style.width = `${pct}%`; } }
        if (ev === "ask") { W.st = "ask"; askShow(W.F, "RESTORE IT ALL", null, "restores it all"); }
      });
    } catch (e) { err = e; }
    finally { if (dev) dev.link.close(); setBusy(false); }
    if (err) {
      const t = trouble(err);
      W.st = "trouble";
      draw(W.F, { title: "RESTORE IT ALL", body: stage(drawing(t.draw, { plate: t.plate }), { title: t.title, kind: "warn", text: t.text, small: t.small }),
                  foot: [t.install ? installBtn() : null, btn("Try again", wholeGo, { ice: true, id: "whole-again" })] });
      return;
    }
    W.st = "answered";
    if (code === 0) {
      lastRead = { image: W.image, ...FB.read(PB.parse(W.image)), flags: 0, at: new Date() };
      changed();
      if (W.o.onDone) W.o.onDone();
    }
    answerShow(W.F, "RESTORE IT ALL", null, code, [`${FW.name} restored `, h("b", null, W.label), " and reloaded everything: its sixteen slots and its settings."], wholeGo);
  }

  return {
    voice, backup, read, place, restore, refused, PLAYING_NOTE, clock, hasMidi, dialogOpen,
    sheet: frame, draw, btn, installBtn, stage, trouble, drawing, deviceLine, waterline,
    get busy() { return busy; }, onBusy: (f) => busyFns.push(f), onChanged: (f) => changedFns.push(f),
    get lastRead() { return lastRead; }, set lastRead(fm) { lastRead = fm; }, get version() { return version; },
  };
})();
