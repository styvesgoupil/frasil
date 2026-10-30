/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The card laid out again, the mockups' prototype: what moves in one column at the left (GLINT's walk or sequence,
 * the bass's sequence under it, the LFOs, three abreast, under them), every page at the right (wider; HARMONY gone
 * down to the keys; an opened LFO shows its picture, live, as BASS shows its sequence), and THE KEYS the whole width
 * at the bottom: KEYS (DRONE, GLINT, BASS), KEY MODE (LATCH, HOLD), beside them the track's own settings (HARMONY for
 * the drone, the arpeggio's or the sequence's for GLINT, the bass's sequence's), and a keyboard C1 to C8 to latch,
 * hold, or write a sequence's steps. Staged only (nothing is edited here):
 *   Keys.lay(art, sc, {track, mode, hover, step, openLfo, play})   the card art (opened) of scene sc */
"use strict";
window.Keys = (() => {
  const { h, s, nn, look, word } = RiverCards;
  const S = FB.S, ID = S.ids, P = S.params;
  const C = { steel: "#26303c", edge: "#3b4654", slate: "#4a5563", dim: "#77818f", mist: "#8b95a4", fog: "#dce3ea", white: "#f3f6f9",
              ice: "#6ed0ff", lamp: "#f2a65a", keyW: "#151c25", keyB: "#05070a", keyEdge: "#222b36" };
  const r2 = (v) => Math.round(v * 100) / 100;
  const hex = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
  const mix = (a, b, t) => "#" + hex(a).map((v, i) => Math.round(v + (hex(b)[i] - v) * t).toString(16).padStart(2, "0")).join("");
  const WH = [0, 2, 4, 5, 7, 9, 11];

  // ---------------------------------------------------------------- harmony.c's chord (js/harmony.js's, here)
  const bits = (...ks) => ks.reduce((b, k) => b | (1 << k), 0);
  const SCALE = [0xfff, bits(0, 2, 4, 5, 7, 9, 11), bits(0, 2, 3, 5, 7, 8, 10), bits(0, 2, 3, 5, 7, 9, 10), bits(0, 1, 3, 5, 7, 8, 10),
    bits(0, 2, 4, 6, 7, 9, 11), bits(0, 2, 4, 5, 7, 9, 10), bits(0, 1, 3, 5, 6, 8, 10), bits(0, 2, 3, 5, 7, 8, 11), bits(0, 2, 3, 5, 7, 9, 11),
    bits(0, 2, 4, 7, 9), bits(0, 3, 5, 7, 10)];
  const SHAPE = [[0], [0, 4], [0, 2, 4], [0, 2, 4, 6], [0, 1, 4], [0, 3, 4], [0, 2, 4, 8], [0, 3, 6], [0, 4, 7], [0, 3, 7], [0, 4, 7, 11],
    [0, 3, 7, 10], [0, 4, 7, 10], [0, 4, 8], [0, 3, 6]];
  const inScale = (n, r, b) => (b >> (((n - r) % 12 + 12) % 12)) & 1;
  const up = (n, k, r, b) => { while (k > 0) { n++; if (inScale(n, r, b)) k--; } return n; };
  function chordOf(played, L) {
    const r = L.raw(ID.P_ROOT) % 12, b = SCALE[L.raw(ID.P_SCALE) % 12], c = L.raw(ID.P_CHORD) % 15, v = L.raw(ID.P_VOICING);
    let base = played;
    while (!inScale(base, r, b)) base--;
    const out = SHAPE[c].map((k) => (c < 8 ? up(base, k, r, b) : base + k));
    if (v === 1 && out.length >= 3) out[1] += 12; else if (v === 2 && out.length >= 2) out[0] -= 12;
    return out;
  }

  // ---------------------------------------------------------------- a setting, as the card's cells draw it
  function cell(p, text, x, max, kind) {
    const m = /^(.*\d) (\S{1,3})$/.exec(text), val = m && kind !== "target" ? m[1] : text, unit = m && kind !== "target" ? m[2] : "";
    let meter;
    if (kind === "steps" && max <= 24) { meter = h("span", { class: "dots" }); for (let i = 0; i <= max; i++) meter.append(h("i", i === x ? { class: "on" } : null)); }
    else meter = h("span", { class: "gauge", style: `--f:${r2(Math.max(1, Math.round(x / Math.max(1, max) * 88)) / 88)}` }, h("i"));
    return h("div", { class: "cell", "data-p": p }, h("div", { class: "cell-n" }, P[p].name),
      h("div", { class: "cell-v" }, h("span", { class: "v" }, val), unit && h("span", { class: "u" }, unit)), h("div", { class: "cell-m" }, meter));
  }
  const sound = (L, p) => cell(p, L.txt(p), L.raw(p), P[p].max, P[p].kind);
  const setting = (p, x) => cell(p, word(p, x), x, P[p].max, P[p].kind);

  // ---------------------------------------------------------------- the keyboard: C1 .. C8, large
  function board(sc, L, o) {
    const lo = 24, hi = 108, kw = 24, wy = 40, kh = 128, bh = 80, bedY = wy + kh + 6;
    let whites = 0;
    for (let n = lo; n <= hi; n++) if (WH.includes(n % 12)) whites++;
    const W = whites * kw, svg = s("svg", { viewBox: `0 0 ${W} ${bedY + 26}`, role: "img", "aria-label": "The keys, C1 to C8" });
    const id = "kb" + Math.random().toString(36).slice(2, 7);
    svg.append(s("defs", null,
      s("radialGradient", { id: `${id}-w` }, s("stop", { offset: 0, "stop-color": C.white, "stop-opacity": 0.45 }), s("stop", { offset: 1, "stop-color": C.white, "stop-opacity": 0 })),
      s("radialGradient", { id: `${id}-i` }, s("stop", { offset: 0, "stop-color": C.ice, "stop-opacity": 0.7 }), s("stop", { offset: 1, "stop-color": C.ice, "stop-opacity": 0 })),
      s("radialGradient", { id: `${id}-l` }, s("stop", { offset: 0, "stop-color": C.lamp, "stop-opacity": 0.6 }), s("stop", { offset: 1, "stop-color": C.lamp, "stop-opacity": 0 })),
      s("linearGradient", { id: `${id}-c`, x1: 0, y1: 1, x2: 0, y2: 0 }, s("stop", { offset: 0, "stop-color": C.white, "stop-opacity": 0.85 }), s("stop", { offset: 1, "stop-color": C.white, "stop-opacity": 0 }))),
      s("rect", { x: 0, y: wy - 0.5, width: W, height: 1, fill: C.slate, opacity: 0.6 }));
    const r = L.raw(ID.P_ROOT) % 12, sb = SCALE[L.raw(ID.P_SCALE) % 12], track = o.track;
    const lat = {};                                   // what is latched, by track: note -> kind
    sc.chords.forEach((ch) => { lat[ch.root] = { t: "drone", k: "root" }; ch.notes.forEach((n) => { if (n !== ch.root && !lat[n]) lat[n] = { t: "drone", k: "note" }; }); });
    const arpN = L.arpOn ? sc.arp.map((a) => a + 12 * L.oct) : [];
    const bassN = sc.bass >= 0 ? [sc.bass] : [];
    const ghost = new Set(o.hover != null && track === "drone" ? chordOf(o.hover, L) : o.hover != null ? [o.hover] : []);
    const keys = new Map(), wk = [], bk = [], marks = [], over = [], labels = [], bed = [];
    let wi = 0;
    for (let n = lo; n <= hi; n++) {
      const white = WH.includes(n % 12), cx = white ? wi * kw + kw / 2 : wi * kw;
      const R = white ? { x: wi * kw + 0.6, y: wy + 1, width: kw - 1.2, height: kh, rx: 2.5 } : { x: r2(cx - 7.5), y: wy + 1, width: 15, height: bh, rx: 2 };
      let fill = white ? C.keyW : C.keyB;
      const l = lat[n], on = track === "drone";
      if (l && l.k === "root") fill = on ? C.white : mix(fill, C.white, 0.32);
      else if (l) fill = mix(fill, C.fog, on ? 0.5 : 0.22);
      if (arpN.includes(n)) fill = track === "glint" ? C.ice : mix(fill, C.ice, 0.3);
      (white ? wk : bk).push(s("rect", { ...R, fill, stroke: white ? "none" : C.keyEdge, "stroke-width": white ? 0 : 1 }));
      if (ghost.has(n)) over.push(s("rect", { ...R, fill: "none", stroke: C.lamp, "stroke-width": 1.6, opacity: n === o.hover ? 1 : 0.75 }));
      if (o.stepKey === n) over.push(s("rect", { ...R, fill: C.lamp, "fill-opacity": 0.22, stroke: C.lamp, "stroke-width": 1.6 }));
      if (inScale(n, r, sb)) marks.push(s("circle", { cx: r2(cx), cy: white ? wy + kh - 9 : wy + bh - 8, r: (n - r) % 12 === 0 ? 2.2 : 1.4,
        fill: (n - r) % 12 === 0 ? C.ice : white ? C.slate : C.edge, opacity: (n - r) % 12 === 0 ? 0.9 : 0.9 }));
      if (bassN.includes(n)) bed.push(s("rect", { x: r2(cx - 7), y: bedY, width: 14, height: 3, rx: 1.5, fill: C.lamp, opacity: track === "bass" ? 1 : 0.45 }));
      if (n % 12 === 0) labels.push(s("text", { x: r2(Math.max(8, Math.min(W - 8, cx))), y: bedY + 20, "text-anchor": "middle", fill: C.dim, class: "svgt" }, nn(n)));
      keys.set(n, { x: cx, white, R });
      if (white) wi++;
    }
    const liveG = s("g"), liveK = s("g");
    svg.append(liveG, ...wk, liveK, ...bk, ...marks, ...over, ...bed, ...labels);
    if (o.hover != null) {
      const k = keys.get(o.hover), txt = track === "drone" ? `${nn(o.hover)} · ${L.txt(ID.P_CHORD)} ${L.txt(ID.P_VOICING)}: ${chordOf(o.hover, L).map(nn).join(" ")}` : nn(o.hover);
      const w = txt.length * 6.6 + 18, x = Math.max(4, Math.min(W - w - 4, k.x - w / 2));
      svg.append(s("rect", { x: r2(x), y: 6, width: r2(w), height: 22, rx: 5, fill: "#18212c", stroke: C.lamp, "stroke-opacity": 0.6 }),
        s("text", { x: r2(x + w / 2), y: 21, "text-anchor": "middle", fill: C.fog, class: "svgt", style: "font-size:11px" }, txt));
    }
    /* live: a voice's light on its key, as the card's keyboard does, larger */
    svg._live = (f) => {
      liveG.replaceChildren(); liveK.replaceChildren();
      if (!f) return;
      for (const v of f.drone) { const k = keys.get(v.n); if (!k) continue; liveG.append(s("circle", { cx: r2(k.x), cy: wy, r: 16, fill: `url(#${id}-w)`, opacity: r2(v.l) }), s("rect", { x: r2(k.x - 1), y: r2(wy - 30 * v.l), width: 2, height: r2(30 * v.l), fill: `url(#${id}-c)`, opacity: r2(v.l) })); }
      for (const v of f.glint) { const k = keys.get(v.n); if (!k) continue; const y = wy - 12 - (1 - v.l) * 14; liveG.append(s("circle", { cx: r2(k.x), cy: r2(y), r: r2(6 + 10 * v.l), fill: `url(#${id}-i)`, opacity: r2(v.l) }), s("circle", { cx: r2(k.x), cy: r2(y), r: 1.6, fill: C.white, opacity: r2(v.l) })); }
      if (f.bass) { const k = keys.get(f.bass.n); if (k) liveG.append(s("ellipse", { cx: r2(k.x), cy: bedY + 2, rx: 26, ry: 8, fill: `url(#${id}-l)`, opacity: r2(f.bass.l) })); }
    };
    return svg;
  }

  // ---------------------------------------------------------------- GLINT's sequence as a grid (a piano roll)
  /* a column a step (its LENGTH lit, the rest in the shade, a beat every four), a row a note (as it sounds, at GLINT's
   * OCTAVE; the scale's rows lighter, the root's an ice line), the keys upright at the left; a note a bar on its row,
   * as long as the TIEs after it, brighter with ACCENT. One note a step: a click in a column puts the note there (the
   * one it had goes), on a note takes it away (REST), its end dragged ties it on; the step playing lit, live.
   * Staged: o.hoverCell [step, note], o.selected (a step whose note is chosen: its end's grip) */
  function grid(sc, L, o) {
    const sh = 12 * L.oct, len = L.raw(ID.P_SQ_LEN) + 1, n = len <= 16 ? 16 : 32, REST = S.seq.rest, TIE = S.seq.tie, ACC = S.seq.accent;
    const notes = [];
    sc.seq.forEach((b) => { const v = b & 127; if (v !== REST && v !== TIE) notes.push(v + sh); });
    let lo = Math.min(...notes, 999), hi = Math.max(...notes, -1);
    if (hi < 0) { lo = 60 + sh; hi = 72 + sh; }
    lo -= 3; hi += 3;
    if (hi - lo < 24) { const m = Math.round((hi + lo) / 2); lo = m - 12; hi = m + 12; }
    const rows = hi - lo + 1, rh = 13, kw = 64, W = 1200, gx = kw + 8, cw = (W - gx) / n, H = rows * rh + 22;
    const Y = (note) => (hi - note) * rh + 18, X = (i) => gx + i * cw;
    const r = L.raw(ID.P_ROOT) % 12, sb = SCALE[L.raw(ID.P_SCALE) % 12];
    const svg = s("svg", { viewBox: `0 0 ${W} ${H}`, role: "img", "aria-label": "GLINT's sequence, a grid" });
    const id = "gr" + Math.random().toString(36).slice(2, 7);
    svg.append(s("defs", null, s("linearGradient", { id: `${id}-n`, x1: 0, y1: 0, x2: 0, y2: 1 }, s("stop", { offset: 0, "stop-color": "#9be0ff" }), s("stop", { offset: 1, "stop-color": C.ice }))));
    for (let note = hi; note >= lo; note--) {                    // the rows, and the keys upright
      const y = Y(note), pc = ((note % 12) + 12) % 12, blk = !WH.includes(pc), inS = inScale(note, r, sb);
      svg.append(s("rect", { x: gx, y, width: W - gx, height: rh, fill: inS ? "#0f151c" : "#0a0e13" }),
        s("rect", { x: gx, y: y + rh - 0.5, width: W - gx, height: 0.5, fill: "#151c25" }));
      if ((pc - r + 12) % 12 === 0) svg.append(s("rect", { x: gx, y: y + rh - 1, width: W - gx, height: 1, fill: C.ice, opacity: 0.22 }));
      svg.append(s("rect", { x: blk ? 22 : 0, y: y + 0.5, width: blk ? kw - 22 : kw, height: rh - 1, rx: 2, fill: blk ? C.keyB : C.keyW, stroke: blk ? C.keyEdge : "none" }));
      if (pc === 0) svg.append(s("text", { x: 6, y: y + rh - 3, fill: C.dim, class: "svgt" }, nn(note)));
    }
    for (let i = 0; i <= n; i++)                                 // the columns: a step each, a beat every four
      svg.append(s("rect", { x: r2(X(i) - 0.5), y: 18, width: i % 4 ? 0.5 : 1, height: rows * rh, fill: i % 4 ? "#18202a" : "#26303c" }));
    for (let i = 0; i < n; i++)
      svg.append(s("text", { x: r2(X(i) + cw / 2), y: 12, "text-anchor": "middle", fill: i < len ? (i % 4 ? C.slate : C.mist) : "#2b3440", class: "svgt" }, String(i + 1)));
    if (len < n) svg.append(s("rect", { x: X(len), y: 18, width: W - X(len), height: rows * rh, fill: "#000", "fill-opacity": 0.42 }));
    {                                                             // LENGTH: its end, a grip to drag (16 steps, or 32)
      const lx = Math.min(X(len), W - 1);
      svg.append(s("rect", { x: r2(lx - 1), y: 14, width: 2, height: rows * rh + 6, fill: C.edge }),
        s("rect", { x: r2(Math.min(lx - 24, W - 50)), y: 0, width: 48, height: 14, rx: 3, fill: "#1a222c", stroke: C.edge }),
        s("text", { x: r2(Math.min(lx, W - 26)), y: 10, "text-anchor": "middle", fill: C.fog, class: "svgt", style: "font-size:8.5px" }, "LENGTH " + len));
    }
    const head = s("rect", { y: 18, width: cw, height: rows * rh, fill: C.ice, opacity: 0 });
    svg.append(head);
    const bars = [];
    for (let i = 0; i < n; i++) {                                 // the notes: a bar each, on through its TIEs
      const b = sc.seq[i], v = b & 127;
      if (v === REST || v === TIE) continue;
      let j = i + 1;
      while (j < n && (sc.seq[j] & 127) === TIE) j++;
      const note = v + sh, acc = (b & ACC) !== 0, x = X(i) + 2, w = (j - i) * cw - 4, y = Y(note) + 1.5;
      const out = i >= len;
      const bar = s("rect", { x: r2(x), y: r2(y), width: r2(w), height: rh - 3, rx: 3, fill: out ? C.slate : `url(#${id}-n)`, opacity: out ? 0.5 : acc ? 1 : 0.78 });
      svg.append(bar);
      if (acc && !out) svg.append(s("rect", { x: r2(x + 3), y: r2(y + 1.5), width: r2(Math.max(2, w - 6)), height: 1.4, rx: 0.7, fill: C.white, opacity: 0.9 }));
      if (o.selected === i) svg.append(s("rect", { x: r2(x - 1.5), y: r2(y - 1.5), width: r2(w + 3), height: rh, rx: 4, fill: "none", stroke: C.lamp, "stroke-width": 1.6 }),
        s("rect", { x: r2(x + w - 4), y: r2(y + 1), width: 3, height: rh - 5, rx: 1.5, fill: C.lamp }));
      bars.push({ i, j, note, bar });
    }
    if (o.hoverCell) {                                            // the cell under the mouse: the note a click puts there
      const [i, note] = o.hoverCell, x = X(i) + 2, y = Y(note) + 1.5;
      svg.append(s("rect", { x: r2(x), y: r2(y), width: r2(cw - 4), height: rh - 3, rx: 3, fill: C.lamp, "fill-opacity": 0.18, stroke: C.lamp, "stroke-width": 1.4 }),
        s("rect", { x: 0, y: Y(note) + 0.5, width: kw, height: rh - 1, rx: 2, fill: C.lamp, opacity: 0.45 }));
      const t = `STEP ${i + 1} · ${nn(note)}`, w = t.length * 6.4 + 16, tx = Math.min(W - w - 4, x + cw + 6);
      svg.append(s("rect", { x: r2(tx), y: r2(y - 4), width: r2(w), height: 20, rx: 5, fill: "#18212c", stroke: C.lamp, "stroke-opacity": 0.6 }),
        s("text", { x: r2(tx + w / 2), y: r2(y + 10), "text-anchor": "middle", fill: C.fog, class: "svgt", style: "font-size:10.5px" }, t));
    }
    svg._live = (f) => {
      const pos = f && f.seq != null ? f.seq : -1;
      if (pos < 0 || pos >= n) { head.setAttribute("opacity", 0); bars.forEach((b) => b.bar.removeAttribute("filter")); return; }
      head.setAttribute("x", r2(X(pos))); head.setAttribute("opacity", 0.16);
      bars.forEach((b) => { const on = pos >= b.i && pos < b.j; b.bar.setAttribute("stroke", on ? C.white : "none"); b.bar.setAttribute("stroke-width", on ? 1.2 : 0); });
    };
    return svg;
  }

  // ---------------------------------------------------------------- a sequence's steps, over the keys
  function steps(sc, L, o) {
    if (o.track === "glint" && L.seqOn) {
      const len = L.raw(ID.P_SQ_LEN) + 1, n = len <= 16 ? 16 : 32, sh = 12 * L.oct;
      const box = h("div", { class: "k-steps", style: `grid-template-columns:repeat(${n},minmax(0,1fr));--n:${n}` });
      for (let i = 0; i < n; i++) {
        const b = sc.seq[i], v = b & 127, rest = v === S.seq.rest, tie = v === S.seq.tie;
        box.append(h("div", { class: "k-step" + (rest ? " rest" : "") + (tie ? " tie" : "") + (b & S.seq.accent ? " acc" : "") + (i >= len ? " out" : "") + (i === o.step ? " sel" : ""), "data-i": i },
          h("span", { class: "n" }, String(i + 1)), h("span", { class: "v" }, rest ? "·" : tie ? "—" : nn(v + sh))));
      }
      return box;
    }
    if (o.track === "bass" && sc.bseq && sc.bseq.on) {
      const bq = sc.bseq, n = bq.steps + 1, oct = 12 * (L.raw(ID.P_BS_OCTAVE) - 2), T = S.bseq.ticks, maxT = Math.max(...T);
      const box = h("div", { class: "k-steps", style: `grid-template-columns:repeat(16,minmax(0,1fr));--n:16` });
      for (let i = 0; i < 16; i++) {
        const b = bq.notes[i], cut = b === S.bseq.cut, hold = b === S.bseq.hold, t = T[bq.lens[i] % T.length];
        let nt = b + oct; while (nt < 24) nt += 12;
        box.append(h("div", { class: "k-step bass" + (cut ? " rest" : "") + (hold ? " tie" : "") + (i >= n ? " out" : "") + (i === o.step ? " sel" : ""), "data-i": i },
          h("span", { class: "n" }, String(i + 1)), h("span", { class: "v" }, i >= n ? "" : cut ? "CUT" : hold ? "HOLD" : nn(nt)),
          i < n ? h("span", { class: "len", title: word(150, bq.lens[i]) }, h("i", { style: `--w:${r2(Math.max(0.06, Math.log2(t + 1) / Math.log2(maxT + 1)) * 100)}%` })) : null));
      }
      return box;
    }
    return null;
  }

  // ---------------------------------------------------------------- THE KEYS
  function panel(sc, L, o) {
    const T = (t, label) => h("button", { type: "button", class: `t-${t}` + (o.track === t ? " on" : "") }, label);
    const M = (m, label) => h("button", { type: "button", class: o.mode === m ? "on" : "" }, label);
    let title = "", cells = [], tools = [], say = "";
    const tool = (w) => h("button", { type: "button", class: "a" }, w);
    if (o.track === "drone") {
      title = "HARMONY";
      cells = [ID.P_ROOT, ID.P_SCALE, ID.P_CHORD, ID.P_VOICING].map((p) => sound(L, p));
      tools = [tool("Clear")];
      say = [h("b", null, "Click a key"), `: its chord latches (${L.txt(ID.P_CHORD)} ${L.txt(ID.P_VOICING)}); again, it lets go.`];
    } else if (o.track === "glint" && L.seqOn) {
      title = "GLINT SEQ";
      cells = [ID.P_SQ_LEN, ID.P_ARP_RATE, ID.P_SQ_GATE, ID.P_ARP_CHANCE, ID.P_SQ_TRANS, ID.P_GL_OCTAVE].map((p) => sound(L, p));
      tools = o.grid ? [tool("Clear")] : [tool("Rest"), tool("Tie"), tool("Accent"), tool("Clear")];
      say = o.grid ? [h("b", null, "Click in the grid"), ": a note on that step (one note a step)."]
                   : [h("b", null, `Step ${o.step + 1}`), ": a key writes its note, then the next step."];
    } else if (o.track === "glint") {
      title = L.arpOn ? "GLINT ARP" : "GLINT";
      cells = (L.arpOn ? [ID.P_ARP_MODE, ID.P_ARP_RATE, ID.P_ARP_RANGE, ID.P_ARP_CHANCE, ID.P_GL_OCTAVE] : [ID.P_GL_OCTAVE]).map((p) => sound(L, p));
      tools = [tool("Clear")];
      say = o.mode === "hold" ? [h("b", null, "Press a key"), ": the arpeggio plays it while it is down; nothing is kept."]
                              : [h("b", null, "Click keys"), ": each joins the arpeggio; again, it leaves."];
    } else {
      const bq = sc.bseq;
      if (bq && bq.on) {
        title = "BASS SEQ";
        cells = [setting(ID.P_BQ_STEPS ?? 152, bq.steps), setting(150, bq.lens[o.step || 0]), setting(155, bq.trans), sound(L, ID.P_BS_OCTAVE)];
        tools = [h("div", { class: "k-ctl" }, h("span", { class: "k-l" }, "AUTO NEXT STEP"),
                   h("div", { class: "k-seg", role: "group" }, h("button", { type: "button", class: o.next === false ? "" : "on" }, "ON"), h("button", { type: "button", class: o.next === false ? "on" : "" }, "OFF"))),
                 tool("Cut"), tool("Hold"), tool("Clear")];
        say = [h("b", null, `Step ${(o.step || 0) + 1} selected`), `. Click a key: its note goes into step ${(o.step || 0) + 1}, then step ${(o.step || 0) + 2} is selected.`];
      } else {
        title = "BASS";
        cells = [ID.P_BS_OCTAVE, ID.P_BS_MODEL].map((p) => sound(L, p));
        say = [h("b", null, "Click a key"), ": the bass holds it; another, it glides there."];
      }
    }
    let stepKey = null;                            // (the key the chosen step plays)
    if (o.step != null && o.track === "glint" && L.seqOn) { const v = sc.seq[o.step] & 127; if (v !== S.seq.rest && v !== S.seq.tie) stepKey = v + 12 * L.oct; }
    if (o.step != null && o.track === "bass" && sc.bseq && sc.bseq.on) {
      const b = sc.bseq.notes[o.step];
      if (b !== S.bseq.cut && b !== S.bseq.hold) { stepKey = b + 12 * (L.raw(ID.P_BS_OCTAVE) - 2); while (stepKey < 24) stepKey += 12; }
    }
    const roll = o.track === "glint" && L.seqOn && o.grid, lane = roll ? null : steps(sc, L, o), kb = roll ? grid(sc, L, o) : board(sc, L, { ...o, stepKey });
    const el = h("section", { class: "oc-keys", "aria-label": "The keys" },
      h("div", { class: "k-top" }, h("h4", { class: "k-title" }, "THE KEYS"),
        h("div", { class: "k-ctl" }, h("span", { class: "k-l" }, "KEYS"), h("div", { class: "k-seg", role: "group" }, T("drone", "DRONE"), T("glint", "GLINT"), T("bass", "BASS"))),
        h("div", { class: "k-ctl" }, h("span", { class: "k-l" }, "KEY MODE"), h("div", { class: "k-seg", role: "group" }, M("latch", "LATCH"), M("hold", "HOLD"))),
        null),
      lane, h("div", { class: "k-board" }, kb),
      roll ? h("p", { class: "k-how" }, "CLICK A NOTE: IT GOES · DRAG ITS END: LONGER · DRAG IT UP OR DOWN: ANOTHER NOTE · SHIFT-CLICK: ACCENT · LENGTH: DRAG ITS GRIP") : null,
      h("div", { class: "k-bottom" }, h("div", { class: "k-set" }, h("p", { class: "k-l" }, title), h("div", { class: "k-cells" }, cells)),
        tools.length ? h("div", { class: "k-tools" }, tools) : null),
      legendOf(sc, L));
    el._kb = kb;
    el._lane = lane;
    return el;
  }
  function legendOf(sc, L) {
    const items = [], nOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn).join(" ");
    if (sc.chords.length) items.push(["DRONES", "w", sc.chords.map((ch) => [nn(ch.root), nOf(ch)])]);
    if (L.arpOn && sc.arp.length) items.push(["ARP", "i", [[sc.arp.map((x) => nn(x + 12 * L.oct)).join(" "), ""]]]);
    if (sc.bass >= 0) items.push(["BASS", "l", [[nn(sc.bass), ""]]]);
    const box = h("div", { class: "lg k-legend" });
    for (const [label, sw, groups] of items)
      box.append(h("div", { class: "lg-i" }, h("span", { class: "sw sw-" + sw }), h("span", { class: "lg-l" }, label),
        groups.map(([root, notes]) => h("span", { class: "lg-g" }, root && h("b", null, root), notes && h("span", null, notes)))));
    return box;
  }

  // ---------------------------------------------------------------- the card, laid out again
  const secOf = (art, name) => [...art.querySelectorAll("details.sec")].find((d) => d.querySelector(".sec-name").firstChild.textContent === name);
  function lay(art, sc, o) {
    const L = look(sc.v), body = art.querySelector(".oc-body"), left = body.querySelector(".oc-left");
    body.classList.add("k-two");
    body.classList.remove("has-lfo");
    left.querySelector(".sh-plays").remove();                      // (what plays: the keys at the bottom show it)
    const bd = secOf(art, "BASS");
    let strip = null;
    if (sc.bseq && sc.bseq.on) { bd.open = true; bd._build(); strip = bd.querySelector(".sec-body > .scr"); bd.open = false; }
    const bass = h("section", { class: "k-bass" },
      h("div", { class: "sh-sub gl-sub" }, h("h4", null, "BASS"), sc.bseq && sc.bseq.on ? h("span", { class: "gl-type k-bq" }, "SEQ") : h("span", { class: "gl-type is-off" }, "NOTE"),
        h("span", { class: "gl-meta" }, h("span", { class: "sig-l" }, "MODEL"), h("span", { class: "gl-v" }, L.txt(ID.P_BS_MODEL)),
          h("span", { class: "sig-l" }, "OCTAVE"), h("span", { class: "gl-v" }, L.txt(ID.P_BS_OCTAVE)))),
      strip || h("p", { class: "bq-none" }, sc.bass >= 0 ? `NO SEQUENCE · ${nn(sc.bass)} HELD` : "NO SEQUENCE · NOTHING HELD"));
    left.append(bass, body.querySelector(".oc-lfo"));
    const hm = secOf(art, "HARMONY");
    if (hm) hm.remove();                                           // (HARMONY: down at the keys)
    for (const i of o.openLfo || []) {                             // an LFO's section, opened: its picture first, live
      const d = secOf(art, "LFO " + i);
      d.open = true; d._build();
      const src = body.querySelectorAll(".oc-lfo .lf .lf-sc")[i - 1], copy = src.cloneNode(true);
      copy.querySelectorAll("[id]").forEach((e) => e.setAttribute("id", e.getAttribute("id") + "-m"));
      copy.querySelectorAll("[fill^='url(#']").forEach((e) => e.setAttribute("fill", e.getAttribute("fill").replace(")", "-m)")));
      d.querySelector(".sec-body").prepend(h("div", { class: "scr k-lfo" }, copy));
      mirrors.push([src, copy]);
    }
    const keys = panel(sc, L, o);
    art.querySelector(".oc-in").append(keys);
    art._lv = null;
    return keys;
  }
  /* the copies of the LFOs' pictures follow their originals; the keys' lights, what is heard */
  const mirrors = [];
  let raf = 0;
  function run(keys) {
    cancelAnimationFrame(raf);
    const frame = () => {
      for (const [a, b] of mirrors) {
        const A = a.querySelectorAll("*"), B = b.querySelectorAll("*");
        A.forEach((x, i) => { for (const at of ["d", "cx", "cy", "x", "opacity", "r"]) { const v = x.getAttribute(at); if (v != null && B[i]) B[i].setAttribute(at, v); } });
      }
      const v = window.RiverPlayer && RiverPlayer.live && RiverPlayer.live();
      keys._kb._live(v ? decode(v) : null);
      if (keys._lane && v) keys._lane.querySelectorAll(".k-step").forEach((e) => e.classList.toggle("now", +e.dataset.i === (keys._lane.querySelector(".bass") ? v[40] : v[38])));
      raf = requestAnimationFrame(frame);
    };
    raf = requestAnimationFrame(frame);
  }
  function decode(v) {
    const f = { drone: [], glint: [], bass: null };
    for (let i = 0; i < 8; i++) if (v[i] >= 0 && v[8 + i] > 0.003) f.drone.push({ n: Math.round(v[i]), l: v[8 + i] });
    for (let i = 0; i < 4; i++) if (v[16 + i] >= 0 && v[20 + i] > 0.003) f.glint.push({ n: Math.round(v[16 + i]), l: v[20 + i] });
    if (v[24] >= 0 && v[25] > 0.003) f.bass = { n: Math.round(v[24]), l: v[25] };
    return f;
  }
  return { lay, run };
})();
