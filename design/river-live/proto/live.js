/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's live view, the mockups' prototype: an opened card follows what FRASIL's engine plays. Fed the engine's
 * frames (web_live, app/web.c: 48 floats), it lights the keys the voices sound (the drones fog and white, GLINT ice, the
 * bass lamp in the bed, each as bright as its envelope), runs GLINT's sequence under its playhead (or draws the
 * arpeggio's real walk as it goes, flowing left), moves the bass's playhead along its line of time, and keeps a column of
 * the three LFOs at the card's left, each its shape with a glint where it is now and what it moves, the settings
 * breathing with it.
 *   Live.attach(art, sc)     the card (opened) of scene sc: its LFO column, its keyboard as wide as what it plays
 *   Live.feed(v, t)          a frame (Float32Array or array of 48) at time t (seconds)
 *   Live.rest()              not playing: the lights out, the LFOs at rest */
"use strict";
window.Live = (() => {
  const { h, s, nn, look, word } = RiverCards;
  const S = FB.S, ID = S.ids, P = S.params;
  const C = {
    screen: "#090c11", rule: "#1a212a", steel: "#26303c", edge: "#3b4654", slate: "#4a5563", dim: "#77818f", mist: "#8b95a4",
    text2: "#aeb7c2", fog: "#dce3ea", white: "#f3f6f9", ice: "#6ed0ff", lamp: "#f2a65a", keyW: "#151c25", keyB: "#05070a", keyEdge: "#222b36",
  };
  const TRACK = { drone: C.fog, glint: C.ice, bass: C.lamp, off: C.mist };
  const r2 = (v) => Math.round(v * 100) / 100;
  const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
  const hex = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
  const mix = (a, b, t) => "#" + hex(a).map((v, i) => Math.round(v + (hex(b)[i] - v) * t).toString(16).padStart(2, "0")).join("");
  const WH = [0, 2, 4, 5, 7, 9, 11];
  let uid = 0, card = null;

  // ---------------------------------------------------------------- a frame
  function decode(v) {
    const f = { drone: [], glint: [], bass: null, lfo: [], seq: v[38], note: v[39], bq: v[40], bqT: v[41], m: [v[42], v[43], v[44], v[45]], strikes: v[47] };
    for (let i = 0; i < 8; i++) if (v[i] >= 0 && v[8 + i] > 0.003) f.drone.push({ n: v[i], l: v[8 + i] });
    for (let i = 0; i < 4; i++) if (v[16 + i] >= 0 && v[20 + i] > 0.003) f.glint.push({ n: v[16 + i], l: v[20 + i] });
    if (v[24] >= 0 && v[25] > 0.003) f.bass = { n: v[24], l: v[25] };
    for (let i = 0; i < 3; i++) f.lfo.push({ on: v[26 + i] > 0, ph: v[29 + i], out: v[32 + i], cyc: v[35 + i] });
    return f;
  }

  // ---------------------------------------------------------------- the notes a scene can play: the keyboard's width
  const bassNotes = (sc, L) => {                       // (river-cards' bassPlays, its NOTEs only, roughly: for the range)
    if (!sc.bseq || !sc.bseq.on) return [];
    const oct = 12 * (L.raw(ID.P_BS_OCTAVE) - 2);
    return sc.bseq.notes.slice(0, sc.bseq.steps + 1).filter((b) => b !== S.bseq.cut && b !== S.bseq.hold).map((b) => { let n = b + oct; while (n < 24) n += 12; return n; });
  };
  function topNote(sc, L) {
    let hi = 0;
    sc.chords.forEach((ch) => ch.notes.forEach((n) => { hi = Math.max(hi, n); }));
    if (L.arpOn) sc.arp.forEach((a) => { hi = Math.max(hi, a + 12 * (L.oct + L.range - 1)); });
    if (L.seqOn) sc.seq.forEach((b) => { const n = b & 127; if (n !== S.seq.rest && n !== S.seq.tie) hi = Math.max(hi, n + 12 * L.oct); });
    return hi;
  }

  // ---------------------------------------------------------------- WHAT PLAYS: C1 up to the C over the highest note
  /* river-cards' keyboard, as wide as the scene plays (C7 at least, C9 at most), every light a layer of its own: what
   * is latched (its roots white, its notes fog, GLINT's held notes ice, their RANGE ringed), dimmed while it plays, and
   * over it what sounds */
  function keyboard(sc, L) {
    const id = "lv" + (++uid), top = Math.min(120, Math.max(96, Math.ceil((topNote(sc, L) + 1) / 12) * 12));
    const kw = 12, bw = 7.4, wy = 33, kh = 47, bh = 29;
    let whites = 0;
    for (let n = 24; n <= top; n++) if (WH.includes(n % 12)) whites++;
    const W = whites * kw;
    const g = (x) => `url(#${id}-${x})`;
    const lin = (x, col, op) => s("linearGradient", { id: `${id}-${x}`, x1: 0, y1: 1, x2: 0, y2: 0 },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const rad = (x, col, op) => s("radialGradient", { id: `${id}-${x}` },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const svg = s("svg", { class: "kb lv-kb", viewBox: `0 0 ${W} 106`, role: "img" },
      s("defs", null, lin("cw", C.white, 0.85), lin("cf", C.fog, 0.55), lin("ci", C.ice, 0.9),
        rad("hw", C.white, 0.42), rad("hf", C.fog, 0.24), rad("hi", C.ice, 0.48), rad("hl", C.lamp, 0.55), rad("hI", C.ice, 0.7),
        s("linearGradient", { id: `${id}-wl`, x1: 0, y1: 0, x2: 1, y2: 0 },
          [[0, C.slate, 0], [0.18, C.slate, 0.75], [0.5, C.fog, 0.5], [0.82, C.slate, 0.75], [1, C.slate, 0]]
            .map(([o, c, a]) => s("stop", { offset: o, "stop-color": c, "stop-opacity": a })))),
      s("rect", { x: 0, y: 32.5, width: W, height: 1, fill: g("wl") }));
    const st = {};                                   // (river-cards' litNotes, to the top)
    const PRI = { arpr: 1, arp: 2, note: 3, root: 4 };
    const set = (n, what) => { if (n >= 24 && n <= top && (!st[n] || PRI[what] > PRI[st[n]])) st[n] = what; };
    sc.chords.forEach((ch) => { set(ch.root, "root"); ch.notes.forEach((n) => { if (n !== ch.root) set(n, "note"); }); });
    if (L.arpOn) sc.arp.forEach((a) => { set(a + 12 * L.oct, "arp"); for (let r = 1; r < L.range; r++) set(a + 12 * (L.oct + r), "arpr"); });
    const bq = bassNotes(sc, L);
    const LIGHT = { root: ["w", 24, 11, 1], note: ["f", 14, 8, 1], arp: ["i", 20, 10, 1], arpr: ["i", 8, 6, 0.5] };
    const halos = [], cols = [], wk = [], bk = [], wl = [], bl = [], bed = [], octs = [];
    const keys = new Map();                          // note -> {x (its centre), white, over (its live light)}
    let wi = 0;
    for (let n = 24; n <= top; n++) {
      const white = WH.includes(n % 12), lit = st[n];
      const cx = white ? wi * kw + 6 : wi * kw, base = white ? C.keyW : C.keyB;
      let fill = base, stroke = white ? "none" : C.keyEdge, sw = white ? 0 : 0.8;
      const r = white ? { x: wi * kw + 0.5, y: wy + 1, width: kw - 1, height: kh, rx: 1.5 }
                      : { x: r2(cx - bw / 2), y: wy + 1, width: bw, height: bh, rx: 1 };
      let latched = null;
      if (lit === "root") latched = C.white;
      else if (lit === "note") latched = mix(base, C.fog, 0.5);
      else if (lit === "arp") latched = C.ice;
      else if (lit === "arpr") { latched = mix(base, C.ice, 0.14); stroke = C.ice; sw = 0.9; }
      (white ? wk : bk).push(s("rect", { ...r, fill, stroke, "stroke-width": sw }));
      if (latched) (white ? wk : bk).push(s("rect", { ...r, class: "lv-st", fill: latched }));
      const over = s("rect", { ...r, class: "lv-k", fill: C.fog, opacity: 0 });
      (white ? wl : bl).push(over);
      keys.set(n, { x: cx, white, over });
      if (lit) {
        const lg = LIGHT[lit];
        halos.push(s("circle", { class: "lv-st", cx: r2(cx), cy: wy, r: lg[2], fill: g("h" + lg[0]), opacity: lg[3] }));
        cols.push(s("rect", { class: "lv-st", x: r2(cx - 0.75), y: wy - lg[1], width: 1.5, height: lg[1], fill: g("c" + lg[0]), opacity: lg[3] }));
      }
      if (n === sc.bass) bed.push(s("rect", { class: "lv-st", x: r2(cx - 5), y: wy + kh + 3.5, width: 10, height: 2, rx: 1, fill: C.lamp }));
      else if (bq.includes(n)) bed.push(s("rect", { class: "lv-st", x: r2(cx - 3), y: wy + kh + 3.5, width: 6, height: 2, rx: 1, fill: C.lamp, opacity: 0.75 }));
      if (n % 12 === 0) octs.push(s("text", { x: r2(Math.min(W - 6, Math.max(6, cx))), y: 101, "text-anchor": "middle", fill: C.dim, class: "svgt" }, nn(n)));
      if (white) wi++;
    }
    /* what sounds: a light a voice (8 drones, 4 glints, the bass) */
    const pool = (k, make) => Array.from({ length: k }, make);
    const dr = pool(8, () => ({ halo: s("circle", { r: 11, fill: g("hw"), opacity: 0 }), col: s("rect", { width: 1.5, fill: g("cw"), opacity: 0 }) }));
    const gl = pool(4, () => ({ halo: s("circle", { r: 9, fill: g("hI"), opacity: 0 }), star: s("path", { fill: C.ice, opacity: 0 }), core: s("circle", { r: 1.2, fill: C.white, opacity: 0 }) }));
    const bs = { glow: s("ellipse", { rx: 20, ry: 7, fill: g("hl"), opacity: 0 }), bar: s("rect", { width: 12, height: 2.4, rx: 1.2, fill: C.lamp, opacity: 0 }) };
    svg.append(...halos, ...cols, s("g", { class: "lv-on" }, dr.map((d) => [d.halo, d.col])), ...wk, ...wl, ...bk, ...bl, ...bed,
      s("g", { class: "lv-on" }, bs.glow, bs.bar, gl.map((x) => [x.halo, x.star, x.core])), ...octs);
    svg.setAttribute("aria-label", `Keyboard, C1 to ${nn(top)}.`);
    const star = (x, y, a) => {
      const q = a * 0.2, v = a * 0.82;
      return `M${r2(x)} ${r2(y - v)}L${r2(x + q)} ${r2(y - q)}L${r2(x + a)} ${r2(y)}L${r2(x + q)} ${r2(y + q)}` +
             `L${r2(x)} ${r2(y + v)}L${r2(x - q)} ${r2(y + q)}L${r2(x - a)} ${r2(y)}L${r2(x - q)} ${r2(y - q)}Z`;
    };
    let peak = 0.05;
    function update(f) {
      svg.classList.toggle("is-live", !!f);
      for (const k of keys.values()) k.over.setAttribute("opacity", 0);
      if (!f) { [...dr, ...gl].forEach((x) => Object.values(x).forEach((e) => e.setAttribute("opacity", 0))); Object.values(bs).forEach((e) => e.setAttribute("opacity", 0)); return; }
      peak = Math.max(peak * 0.995, f.m[0]);                       // the drones' breath: their meter, against its recent peak
      const breath = clamp(0.55 + 0.45 * f.m[0] / peak, 0.55, 1);
      const lightKey = (n, col, a) => {
        const k = keys.get(Math.round(n));
        if (!k || a <= +k.over.getAttribute("opacity")) return;
        k.over.setAttribute("fill", col);
        k.over.setAttribute("opacity", r2(a));
      };
      dr.forEach((d, i) => {
        const v = f.drone[i], k = v && keys.get(Math.round(v.n));
        if (!k) { d.halo.setAttribute("opacity", 0); d.col.setAttribute("opacity", 0); return; }
        const a = v.l * breath, ht = 6 + 18 * a;
        d.halo.setAttribute("cx", r2(k.x)); d.halo.setAttribute("cy", wy); d.halo.setAttribute("opacity", r2(a));
        d.col.setAttribute("x", r2(k.x - 0.75)); d.col.setAttribute("y", r2(wy - ht)); d.col.setAttribute("height", r2(ht)); d.col.setAttribute("opacity", r2(a));
        lightKey(v.n, k.white ? C.fog : mix(C.keyB, C.fog, 0.62), (k.white ? 0.78 : 0.9) * a);
      });
      gl.forEach((x, i) => {
        const v = f.glint[i], k = v && keys.get(Math.round(v.n));
        if (!k) { Object.values(x).forEach((e) => e.setAttribute("opacity", 0)); return; }
        const y = wy - 9 - (1 - v.l) * 14, a = 2.2 + 6 * v.l;
        x.halo.setAttribute("cx", r2(k.x)); x.halo.setAttribute("cy", r2(y)); x.halo.setAttribute("r", r2(4 + 8 * v.l)); x.halo.setAttribute("opacity", r2(v.l));
        x.star.setAttribute("d", star(k.x, y, a)); x.star.setAttribute("opacity", r2(0.35 + 0.65 * v.l));
        x.core.setAttribute("cx", r2(k.x)); x.core.setAttribute("cy", r2(y)); x.core.setAttribute("opacity", r2(v.l));
        lightKey(v.n, C.ice, 0.25 + 0.75 * v.l);
      });
      const b = f.bass, k = b && keys.get(Math.round(b.n));
      if (!k) { bs.glow.setAttribute("opacity", 0); bs.bar.setAttribute("opacity", 0); }
      else {
        bs.glow.setAttribute("cx", r2(k.x)); bs.glow.setAttribute("cy", wy + kh + 5); bs.glow.setAttribute("opacity", r2(b.l));
        bs.bar.setAttribute("x", r2(k.x - 6)); bs.bar.setAttribute("y", wy + kh + 3.3); bs.bar.setAttribute("opacity", r2(0.4 + 0.6 * b.l));
        lightKey(b.n, mix(k.white ? C.keyW : C.keyB, C.lamp, 0.55), 0.85 * b.l);
      }
    }
    return { svg, update, top };
  }

  // ---------------------------------------------------------------- the LFO column
  /* app.c's lfo_rnd and lfo_shape: the same values the engine takes (RANDOM, DRIFT: a turn's value from its number) */
  function lfoRnd(i, c) {
    let x = (Math.imul((c + 1) >>> 0, 2654435761) ^ Math.imul(i + 1, 0x9E3779B9)) >>> 0;
    x ^= x >>> 15; x = Math.imul(x, 0x85EBCA77) >>> 0; x ^= x >>> 13;
    return (x >>> 8) * (2 / 16777216) - 1;
  }
  function lfoShape(sh, x, i, c) {
    if (sh === 0) return Math.sin(2 * Math.PI * x);
    if (sh === 1) return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4;
    if (sh === 2) return 2 * x - 1;
    if (sh === 3) return 1 - 2 * x;
    if (sh === 4) return x < 0.5 ? 1 : -1;
    if (sh === 5) return lfoRnd(i, c);
    const a = lfoRnd(i, c), b = lfoRnd(i, c + 1);
    return a + (b - a) * x * x * (3 - 2 * x);
  }
  /* an LFO: its name and RATE, its shape over a turn (four for RANDOM, DRIFT: the three before and this one) as deep as
   * DEPTH from POLARITY's side, where it is now a glint; under it what it moves, each in its track's colour with a
   * gauge of its value as the LFO has it now */
  function lfoBlock(L, i) {
    const b = S.lfo.base[i], V = (k) => L.raw(b + k);
    const sh = V(0) % 7, depth = V(2), pol = V(3) % 3, rnd = sh >= 5, turns = rnd ? 4 : 1;
    const tg = [4, 5, 6, 7].map((t) => V(t)).filter((t) => t > 0).map((t) => S.lfo.targets[t]).filter(Boolean);
    const on = depth > 0 && tg.length;
    const Wd = 176, top = 9, bot = 47, d = 0.2 + 0.8 * depth / 127, base = pol === 1 ? bot : pol === 2 ? top : (top + bot) / 2;
    const Y = (v) => (pol === 1 ? bot - 0.5 * (v + 1) * (bot - top) * d : pol === 2 ? top + 0.5 * (v + 1) * (bot - top) * d : base - v * 0.5 * (bot - top) * d);
    const id = "lf" + (++uid), col = on ? C.ice : C.slate;
    const path = s("path", { fill: "none", stroke: col, "stroke-width": 1.4, "stroke-linejoin": "round", "stroke-opacity": on ? 0.9 : 0.8 });
    const soft = s("path", { fill: "none", stroke: col, "stroke-width": 4, "stroke-opacity": on ? 0.1 : 0, "stroke-linejoin": "round" });
    const head = s("rect", { y: 3, width: 1, height: 50, fill: col, opacity: 0 });
    const glow = s("circle", { r: 7, fill: `url(#${id}-g)`, opacity: 0 });
    const arms = s("path", { fill: C.ice, opacity: 0 }), core = s("circle", { r: 1.5, fill: C.white, opacity: 0 });
    const svg = s("svg", { class: "lf-sc", viewBox: `0 0 ${Wd} 56`, "aria-hidden": "true" },
      s("defs", null, s("radialGradient", { id: `${id}-g` }, s("stop", { offset: 0, "stop-color": C.ice, "stop-opacity": 0.55 }), s("stop", { offset: 1, "stop-color": C.ice, "stop-opacity": 0 }))),
      s("line", { x1: 0, x2: Wd, y1: r2(base), y2: r2(base), stroke: C.steel }), head, soft, path, glow, arms, core);
    const draw = (cyc) => {
      let p = "";
      for (let x = 0; x <= Wd; x += 1) {
        const u = Math.min(x / Wd * turns, turns - 1e-4), c = rnd ? cyc + Math.floor(u) - 3 : 0, fr = u - Math.floor(u);
        p += (x ? "L" : "M") + x + " " + r2(Y(lfoShape(sh, fr, i, c)));
      }
      path.setAttribute("d", p); soft.setAttribute("d", p);
    };
    draw(3);
    const gauges = tg.map((t) => {
      const p = t.p, mx = p >= 0 ? P[p].max : 127;
      const g = h("span", { class: "lf-g", "aria-hidden": "true" }, h("i"), h("b"));
      const at = (v) => g.style.setProperty("--f", r2(clamp(v / Math.max(1, mx), 0, 1)));
      at(p >= 0 ? L.raw(p) : 0);
      return { t, el: h("li", null, h("span", { class: "lf-t", style: `color:${TRACK[t.track] || C.fog}` }, t.name), g), set: (off) => at((p >= 0 ? L.raw(p) : 0) + off * mx) };
    });
    const el = h("div", { class: "lf" + (on ? "" : " is-off") },
      h("div", { class: "lf-h" }, h("span", { class: "lf-n" }, "LFO " + (i + 1)),
        h("span", { class: "lf-r" }, on ? [h("span", null, L.txt(b)), h("span", { class: "lf-rt" }, L.txt(b + 1))] : h("span", null, "OFF"))),
      svg,
      tg.length ? h("ul", { class: "lf-ts", role: "list" }, gauges.map((x) => x.el)) : h("p", { class: "lf-none" }, "NO TARGET"));
    const star = (x, y, a) => {
      const q = a * 0.18, v = a * 0.8;
      return `M${r2(x)} ${r2(y - v)}L${r2(x + q)} ${r2(y - q)}L${r2(x + a)} ${r2(y)}L${r2(x + q)} ${r2(y + q)}` +
             `L${r2(x)} ${r2(y + v)}L${r2(x - q)} ${r2(y + q)}L${r2(x - a)} ${r2(y)}L${r2(x - q)} ${r2(y - q)}Z`;
    };
    let lastCyc = 3;
    function update(f) {
      const live = f && f.lfo[i] && f.lfo[i].on && on;
      [head, glow, arms, core].forEach((e) => e.setAttribute("opacity", 0));
      if (!live) { if (lastCyc !== 3 && rnd) draw(lastCyc = 3); gauges.forEach((g) => g.set(0)); return; }
      const q = f.lfo[i];
      if (rnd && q.cyc !== lastCyc) draw(lastCyc = q.cyc);
      const now = rnd ? (3 + q.ph) / 4 : q.ph, x = now * Wd, y = Y(q.out);
      head.setAttribute("x", r2(x)); head.setAttribute("opacity", 0.22);
      glow.setAttribute("cx", r2(x)); glow.setAttribute("cy", r2(y)); glow.setAttribute("opacity", 1);
      arms.setAttribute("d", star(x, y, 6.5)); arms.setAttribute("opacity", 0.85);
      core.setAttribute("cx", r2(x)); core.setAttribute("cy", r2(y)); core.setAttribute("opacity", 1);
      const off = depth / 127 * (pol === 1 ? 0.5 * (q.out + 1) : pol === 2 ? -0.5 * (q.out + 1) : q.out);
      gauges.forEach((g) => g.set(off));
      return off;
    }
    return { el, update, targets: tg, on };
  }

  // ---------------------------------------------------------------- GLINT's strip: SEQ's playhead, ARP's real walk
  function seqLive(svgEl, sc, L) {
    if (!svgEl) return () => {};
    const x0 = 40, x1 = 504, topY = 16, bot = 70, n32 = sc.seq.length, REST = S.seq.rest, TIE = S.seq.tie;
    let lo = 127, hi = 0;
    sc.seq.forEach((b) => { const n = b & 127; if (n !== REST && n !== TIE) { lo = Math.min(lo, n); hi = Math.max(hi, n); } });
    if (hi < lo) { lo = 60; hi = 72; }
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const X = (i) => x0 + i * (x1 - x0) / (n32 - 1);
    const head = s("rect", { y: topY - 6, width: 1.2, height: bot - topY + 16, fill: C.ice, opacity: 0 });
    const glow = s("circle", { r: 15, fill: C.ice, opacity: 0, filter: "blur(4px)" }), core = s("circle", { r: 2.4, fill: C.white, opacity: 0 });
    svgEl.insertBefore(head, svgEl.children[2] || null);
    svgEl.append(glow, core);
    return (f) => {
      const pos = f ? f.seq : -1;
      if (pos < 0) { [head, glow, core].forEach((e) => e.setAttribute("opacity", 0)); return; }
      const x = X(pos), b = sc.seq[pos] & 127;
      head.setAttribute("x", r2(x - 0.6)); head.setAttribute("opacity", 0.5);
      const sounding = f.glint.find((g) => Math.round(g.n) === Math.round(f.note + 12 * L.oct));
      if (b === REST || b === TIE || !sounding) { glow.setAttribute("opacity", 0); core.setAttribute("opacity", 0); return; }
      const y = bot - (b - lo) * (bot - topY) / (hi - lo);
      glow.setAttribute("cx", r2(x)); glow.setAttribute("cy", r2(y)); glow.setAttribute("opacity", r2(0.18 + 0.3 * sounding.l));
      core.setAttribute("cx", r2(x)); core.setAttribute("cy", r2(y)); core.setAttribute("opacity", r2(0.5 + 0.5 * sounding.l));
    };
  }
  /* ARP: the notes it walks over (its held notes and their RANGE), and the steps it has struck, flowing left from the
   * right edge as they go: RANDOM reads random, UP climbs; a step CHANCE left out, a gap */
  function arpLive(scr, sc, L) {
    if (!scr) return () => {};
    const W = 516, x0 = 26, x1 = 504, topY = 12, bot = 62, notes = [];
    for (let r = 0; r < L.range; r++) sc.arp.forEach((a) => notes.push(a + 12 * (L.oct + r)));
    if (!notes.length) return () => {};
    let lo = Math.min(...notes), hi = Math.max(...notes);
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const Y = (n) => bot - (n - lo) * (bot - topY) / (hi - lo);
    const gid = "ag" + (++uid);
    const svg = s("svg", { class: "strip lv-arp", viewBox: `0 0 ${W} 74`, role: "img", "aria-label": "GLINT's arpeggio as it plays" },
      s("defs", null, s("radialGradient", { id: gid }, s("stop", { offset: 0, "stop-color": C.ice, "stop-opacity": 0.4 }), s("stop", { offset: 1, "stop-color": C.ice, "stop-opacity": 0 }))));
    const lines = notes.map((n, j) => s("line", { x1: 26, y1: r2(Y(n)), x2: 512, y2: r2(Y(n)), stroke: j < sc.arp.length ? C.ice : C.slate, "stroke-opacity": j < sc.arp.length ? 0.32 : 0.7, "stroke-dasharray": "1 3" }));
    const trail = s("polyline", { fill: "none", stroke: C.ice, "stroke-opacity": 0.42, "stroke-width": 1.1, "stroke-linejoin": "round", "stroke-linecap": "round" });
    const dots = s("g");
    svg.append(...lines, trail, dots,
      s("text", { x: 4, y: r2(topY + 3.5), "text-anchor": "start", fill: C.dim, class: "svgt" }, nn(hi)),
      s("text", { x: 4, y: r2(bot + 3.5), "text-anchor": "start", fill: C.dim, class: "svgt" }, nn(lo)));
    const still = scr.firstElementChild;
    const hits = [];                                 // {n (as it sounds), t}
    let seen = -1, gap = 0.25;                       // (a step's time: the shortest of the last strikes' gaps)
    return (f, t) => {
      if (!f) { if (svg.isConnected) svg.replaceWith(still); hits.length = 0; seen = -1; return; }
      if (!svg.isConnected) still.replaceWith(svg);
      if (seen >= 0 && f.strikes !== seen && f.note >= 0) {
        hits.push({ n: f.note + 12 * L.oct, t });
        if (hits.length > 40) hits.shift();
        const gaps = hits.slice(-9).map((x, j, a) => (j ? x.t - a[j - 1].t : Infinity)).filter((g) => g > 0.03);
        if (gaps.length) gap = Math.min(...gaps);
      }
      seen = f.strikes;
      const slot = (x1 - 40) / 23, pts = [];
      dots.replaceChildren();
      hits.forEach((hit) => {
        const age = (t - hit.t) / gap, x = x1 - age * slot;
        if (x < 34) return;
        const y = Y(hit.n), fresh = clamp(1 - age / 3, 0, 1), fade = clamp((x - 34) / 120, 0, 1);
        pts.push(r2(x) + "," + r2(y));
        dots.append(s("circle", { cx: r2(x), cy: r2(y), r: r2(6.5 + 6 * fresh), fill: `url(#${gid})`, opacity: r2(fade) }),
          s("path", { d: starPath(x, y, 3.8 + 3.4 * fresh), fill: C.ice, opacity: r2((0.7 + 0.3 * fresh) * fade) }),
          s("circle", { cx: r2(x), cy: r2(y), r: r2(1.1 + 0.9 * fresh), fill: fresh > 0.5 ? C.white : C.fog, opacity: r2(fade) }));
      });
      trail.setAttribute("points", pts.join(" "));
    };
  }
  const starPath = (x, y, a) => {
    const q = a * 0.2, v = a * 0.82;
    return `M${r2(x)} ${r2(y - v)}L${r2(x + q)} ${r2(y - q)}L${r2(x + a)} ${r2(y)}L${r2(x + q)} ${r2(y + q)}` +
           `L${r2(x)} ${r2(y + v)}L${r2(x - q)} ${r2(y + q)}L${r2(x - a)} ${r2(y)}L${r2(x - q)} ${r2(y - q)}Z`;
  };

  // ---------------------------------------------------------------- the bass's line of time: its playhead
  function bassLive(svgEl, sc, L) {
    if (!svgEl || !sc.bseq || !sc.bseq.on) return () => {};
    const bq = sc.bseq, x0 = 40, x1 = 504, topY = 12, bot = 50;
    const steps = bq.steps + 1, len = [];
    for (let i = 0; i < steps; i++) len.push(S.bseq.ticks[bq.lens[i] % S.bseq.ticks.length]);
    const at = [0];
    len.forEach((t) => at.push(at[at.length - 1] + t));
    const total = at[at.length - 1], X = (t) => x0 + t * (x1 - x0) / total;
    const oct = 12 * (L.raw(ID.P_BS_OCTAVE) - 2);
    const sound = bq.notes.slice(0, steps).filter((b) => b !== S.bseq.cut && b !== S.bseq.hold).map((b) => { let n = b + oct; while (n < 24) n += 12; return n; });
    let lo = Math.min(...sound), hi = Math.max(...sound);
    if (!sound.length) { lo = 36; hi = 48; }
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const Y = (n) => bot - (n - lo) * (bot - topY) / (hi - lo);
    const head = s("rect", { y: 4, width: 1.2, height: 62, fill: C.lamp, opacity: 0 });
    const glow = s("circle", { r: 9, fill: C.lamp, opacity: 0, filter: "blur(3px)" }), core = s("circle", { r: 2.2, fill: C.white, opacity: 0 });
    svgEl.insertBefore(head, svgEl.children[4] || null);
    svgEl.append(glow, core);
    return (f) => {
      if (!f || f.bq < 0) { [head, glow, core].forEach((e) => e.setAttribute("opacity", 0)); return; }
      const x = X(at[Math.min(f.bq, steps - 1)] + Math.min(f.bqT, len[Math.min(f.bq, steps - 1)]));
      head.setAttribute("x", r2(x - 0.6)); head.setAttribute("opacity", 0.55);
      if (!f.bass) { glow.setAttribute("opacity", 0); core.setAttribute("opacity", 0); return; }
      const y = clamp(Y(f.bass.n), 4, 58);
      glow.setAttribute("cx", r2(x)); glow.setAttribute("cy", r2(y)); glow.setAttribute("opacity", r2(0.5 * f.bass.l));
      core.setAttribute("cx", r2(x)); core.setAttribute("cy", r2(y)); core.setAttribute("opacity", r2(f.bass.l));
    };
  }

  // ---------------------------------------------------------------- the settings an LFO moves, breathing with it
  function cellsLive(art, lfos) {
    const out = [];
    lfos.forEach((lf, i) => lf.targets.forEach((t) => {
      if (t.p < 0) return;
      const name = P[t.p].name;
      art.querySelectorAll(".cell").forEach((c) => {
        const n = c.querySelector(".cell-n"), g = c.querySelector(".gauge");
        if (!n || !g || n.firstChild.textContent !== name || !n.querySelector(".lfo-tag")) return;
        let ghost = g.querySelector("b");
        if (!ghost) { ghost = h("b", { style: `background:${TRACK[t.track] || C.fog}` }); g.append(ghost); }
        out.push({ i, p: t.p, g, ghost });
      });
    }));
    return out;
  }

  // ---------------------------------------------------------------- a card
  function attach(art, sc) {
    const L = look(sc.v), body = art.querySelector(".oc-body");
    body.classList.add("has-lfo");
    const lfos = [0, 1, 2].map((i) => lfoBlock(L, i));
    const col = h("aside", { class: "oc-lfo", "aria-label": "The LFOs" }, h("div", { class: "sh-sub" }, h("h4", null, "LFOS")), lfos.map((x) => x.el));
    body.prepend(col);
    const kb = keyboard(sc, L), old = art.querySelector(".kb");
    old.replaceWith(kb.svg);
    const rng = art.querySelector(".sh-plays .sh-range");
    if (rng) rng.textContent = "C1 – " + nn(kb.top);
    const glScr = art.querySelector(".sh-glint .scr");
    const seq = L.seqOn ? seqLive(glScr && glScr.querySelector("svg"), sc, L) : () => {};
    const arp = L.arpOn ? arpLive(glScr, sc, L) : () => {};
    const bassSvg = () => art.querySelector(".sec-body .scr svg.strip");
    let bass = null;
    const cells = cellsLive(art, lfos);
    card = { art, sc, L, kb, lfos, seq, arp, cells, bass: () => (bass || (bass = bassSvg() ? bassLive(bassSvg(), sc, L) : null)) };
    return card;
  }
  /* (the River's ripple, river-cards' ripple(): its line of what plays, two slow swells as tall as the sound is loud;
   * here from the engine's meters, RiverPlayer.level() not being there) */
  let amp = 0;
  function ripple(f, t) {
    const lv = f ? Math.min(1, Math.max(...f.m) * 1.6) : 0, pts = [];
    amp += (lv - amp) * 0.25;
    for (let i = 0; i <= 48; i++) {
      const x = i / 48, edge = Math.sin(Math.PI * x);
      pts.push(`${r2(x * 100)},${r2(10 - amp * 8 * edge * (0.62 * Math.sin(x * 9.4 - t * 1000 / 900) + 0.38 * Math.sin(x * 23 + t * 1000 / 640)))}`);
    }
    card.art.querySelectorAll(".ms-wave polyline").forEach((w) => w.setAttribute("points", f ? pts.join(" ") : "0,10 100,10"));
  }
  function feed(v, t) {
    if (!card) return;
    const f = v ? decode(v) : null;
    card.art.dataset.sound = f ? "playing" : "";
    ripple(f, t);
    card.kb.update(f);
    const offs = card.lfos.map((x) => x.update(f));
    card.seq(f);
    card.arp(f, t);
    const b = card.bass();
    if (b) b(f);
    card.cells.forEach((c) => {
      const off = offs[c.i] || 0, mx = P[c.p].max;
      c.ghost.style.left = `calc(${r2(clamp((card.L.raw(c.p) + off * mx) / Math.max(1, mx), 0, 1) * 100)}% - .75px)`;
      c.ghost.style.opacity = f && card.lfos[c.i].on ? 1 : 0;
    });
  }
  return { attach, feed, rest: () => feed(null, 0), decode };
})();
