/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's row (the Community River, Your Private River, Quick Backup's slots): a shard drawn from its bytes with
 * the schema the firmware's code writes (FB.S), closed (▶, its name, a line of what plays, its key, GLINT's type, the
 * bass's sequence and the page's own cells) and opened in place (what plays on the keyboard, GLINT, every page as the
 * screen writes it); ▶ with FRASIL's engine (river-player.js), one shard at a time.
 * What users typed (names, descriptions, authors) is only ever text: no HTML is built from strings here.
 *   RiverCards.row(scene, o)       a shard as a row on the river, opened in place into its card, its head the one
 *                                  action zone (see row())
 *   RiverCards.settingsRow(settings, scenes) · openRow(art, open) · ticked(art, on) · say(art, ...) (a line on a row)
 *   RiverCards.line(scene) (what plays, in one line) · I (the River's line icons)
 *   RiverCards.toast(...parts) · confirm({title, text, ok, danger, check}) -> Promise<boolean>
 *   RiverCards.stopSound() · onSound(fn(key, state, scene)) · h, s, two, nn, word, key, k, fill, look, shard */
"use strict";
window.RiverCards = (() => {
  const S = FB.S, P = S.params, ID = S.ids, N = S.sizes;
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");

  // ================================================================ drawing
  /* base.css's colours, for SVG attributes (which take no var()) */
  const C = {
    screen: "#090c11", glass: "#151b23", rule: "#1a212a", steel: "#26303c", edge: "#3b4654", slate: "#4a5563",
    dim: "#77818f", mist: "#8b95a4", text2: "#aeb7c2", fog: "#dce3ea", white: "#f3f6f9", ice: "#6ed0ff",
    lamp: "#f2a65a", ember: "#d69660", cloud: "#80abc6", sea: "#3480ec", keyW: "#151c25", keyB: "#05070a", keyEdge: "#222b36",
  };
  const TRACK = { drone: C.fog, glint: C.ice, bass: C.lamp, off: C.mist };   // an LFO TARGET's track (ui.c's tgt_col)
  const SVGNS = "http://www.w3.org/2000/svg";
  let uid = 0;

  /* an element: attributes (on*: listeners; false or null: left out), then children (strings become text) */
  function put(e, attrs, kids) {
    if (attrs) for (const [a, v] of Object.entries(attrs)) {
      if (v == null || v === false) continue;
      if (a === "text") e.textContent = v;
      else if (a.startsWith("on") && typeof v === "function") e.addEventListener(a.slice(2), v);
      else e.setAttribute(a, v === true ? "" : String(v));
    }
    for (const c of kids.flat(Infinity)) if (c != null && c !== false && c !== "") e.append(c.nodeType ? c : String(c));
    return e;
  }
  const h = (tag, attrs, ...kids) => put(document.createElement(tag), attrs, kids);
  const s = (tag, attrs, ...kids) => put(document.createElementNS(SVGNS, tag), attrs, kids);
  /* an element's content: text and nodes, never HTML */
  const fill = (el, ...parts) => { el.replaceChildren(); put(el, null, parts); return el; };
  const r2 = (v) => Math.round(v * 100) / 100;
  const two = (n) => String(n).padStart(2, "0");
  const nn = (n) => S.notes[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);   // as the screen names a note: C4 is 60
  const hex = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
  const mix = (a, b, t) => "#" + hex(a).map((v, i) => Math.round(v + (hex(b)[i] - v) * t).toString(16).padStart(2, "0")).join("");
  const word = (p, x) => { const d = P[p]; return d.texts[Math.max(0, Math.min(x, d.max))] ?? String(x); };
  const listWords = (a) => (a.length < 2 ? a.join("") : a.slice(0, -1).join(", ") + " and " + a[a.length - 1]);
  const k = (t) => h("span", { class: "k" }, t);          // a word from the screen
  const key = (t) => h("span", { class: "key" }, t);      // a button

  /* harmony.c's scales as degrees above the root, by the screen's name (the glyph only: an unknown scale draws none) */
  const DEGREES = {
    CHROMATIC: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], MAJOR: [0, 2, 4, 5, 7, 9, 11], MINOR: [0, 2, 3, 5, 7, 8, 10],
    DORIAN: [0, 2, 3, 5, 7, 9, 10], PHRYGIAN: [0, 1, 3, 5, 7, 8, 10], LYDIAN: [0, 2, 4, 6, 7, 9, 11],
    MIXOLYDIAN: [0, 2, 4, 5, 7, 9, 10], LOCRIAN: [0, 1, 3, 5, 6, 8, 10], HARMONIC: [0, 2, 3, 5, 7, 8, 11],
    MELODIC: [0, 2, 3, 5, 7, 9, 11], "PENTA MAJ": [0, 2, 4, 7, 9], "PENTA MIN": [0, 3, 5, 7, 10],
  };

  // ---------------------------------------------------------------- the schema's pages, as a scene shows them
  /* a page's settings that a scene keeps (SWELL, FREEZE, the SEQ page's STEP, NOTE, ACCENT, RECORD, CLEAR, NEW TAKE,
   * UNDO are the page's live controls: not part of a scene) */
  const PAGES = S.pages.map((pg, i) => ({ ...pg, i, keep: pg.p.filter((p) => p >= 0 && p < N.NSOUND) }));
  const elsewhere = (pg) => pg.keep.every((p) => PAGES.some((o) => o !== pg && o.keep.includes(p)));   // (SOUND)
  /* a setting: by its id when the schema has one (id), else by its page's name and its own, each a name or the names
   * it has had (the firmware renames pages: GLINT PARAMS became GLINT I), else on any page of the same family (the
   * first page's first word: GLINT ...); -1 if none */
  const pp = (pages, names, id) => {
    if (id && ID[id] != null) return ID[id];
    const want = [].concat(names), on = (pg) => (pg ? pg.p.find((q) => q >= 0 && P[q] && want.includes(P[q].name)) : undefined);
    for (const page of [].concat(pages)) { const p = on(S.pages.find((x) => x.name === page)); if (p !== undefined) return p; }
    const family = [].concat(pages)[0].split(" ")[0];
    for (const pg of S.pages) if (pg.name.split(" ")[0] === family) { const p = on(pg); if (p !== undefined) return p; }
    return -1;
  };
  /* the sections, in the order of the FM-1's buttons: the drone (EDIT, ENV, its MOTION on LFO), GLINT (ARP), BASS
   * (SEQ), FX, each LFO, HARMONY (SEL), the MIXER (HOME), MASTERING (GLO, with GLOBAL's sound settings) */
  const ORDER = ["DRONE", "GLINT", "BASS", "FX", "LFO 1", "LFO 2", "LFO 3", "HARMONY", "MIXER", "MASTERING"];
  const BY_BUTTON = { EDIT: "DRONE", ENV: "DRONE", LFO: "DRONE", ARP: "GLINT", SEQ: "BASS", FX: "FX", SEL: "HARMONY", HOME: "MIXER", GLO: "MASTERING" };
  const SECTIONS = (() => {
    const by = new Map();
    for (const pg of PAGES) {
      if (!pg.keep.length || elsewhere(pg)) continue;
      const lfo = pg.name.match(/^LFO \d+/), name = lfo ? lfo[0] : BY_BUTTON[pg.button] || pg.button;
      if (!by.has(name)) by.set(name, { name, pages: [], keys: [] });
      const sec = by.get(name);
      sec.pages.push(pg);
      if (!sec.keys.includes(pg.button)) sec.keys.push(pg.button);
    }
    for (const sec of by.values()) {             // the page named as its section first (MASTERING, then GLOBAL)
      const own = sec.pages.findIndex((pg) => pg.name === sec.name);
      if (own > 0) sec.pages.unshift(...sec.pages.splice(own, 1));
    }
    const rank = (n) => (ORDER.includes(n) ? ORDER.indexOf(n) : ORDER.length);
    return [...by.values()].sort((a, b) => rank(a.name) - rank(b.name));
  })();
  /* BASS SEQ's STEP LENGTH (LENGTH before the schema named it so; SEQ LENGTH maybe): by its id, else by name */
  const BQ_LEN = pp("BASS SEQ", ["STEP LENGTH", "SEQ LENGTH", "LENGTH"], "P_BQ_LEN");
  /* GLINT's ARP page only when its TYPE is ARP, its SEQ pages only when SEQ (as the FM-1 hides them) */
  const ARP_PAGES = new Set([ID.PG_ARP]), SEQ_PAGES = new Set([ID.PG_SEQ, ID.PG_SEQ2, ID.PG_SEQ3]);
  const shownFor = (pg, L) => (ARP_PAGES.has(pg.i) ? L.arpOn : SEQ_PAGES.has(pg.i) ? L.seqOn : true);
  const SOUND = (S.pages.find((pg) => pg.name === "SOUND") || { p: [0, 1, 2, 3] }).p;   // CHOIR PARTIALS BREATH WARMTH
  const GL_CELLS = {
    off: [pp(["GLINT I", "GLINT PARAMS"], "LEVEL", "P_GL_LEVEL"), pp(["GLINT I", "GLINT PARAMS"], "BRIGHT", "P_GL_BRIGHT"),
          pp("GLINT II", "DECAY", "P_GL_DECAY"), pp("GLINT II", "ATTACK", "P_GL_ATTACK")],
    arp: S.pages[ID.PG_ARP] ? S.pages[ID.PG_ARP].p : [],
    seq: [ID.P_ARP_RATE, ID.P_SQ_GATE, ID.P_ARP_CHANCE, ID.P_SQ_TRANS],
  };

  /* a sound as the screen shows it (v: the drone's preset applied) */
  function look(v) {
    const type = v[ID.P_SQ_TYPE];
    const L = {
      v, raw: (p) => v[p], txt: (p) => word(p, v[p]), type, arpOn: type === ID.SQT_ARP, seqOn: type === ID.SQT_SEQ,
      oct: v[ID.P_GL_OCTAVE], range: v[ID.P_ARP_RANGE] + 1, tags: {},
    };
    S.lfo.base.forEach((b, i) => {                 // the settings an LFO moves (DEPTH above 0), tagged where they show
      if (!v[b + 2]) return;
      for (let t = 4; t < 8; t++) {
        const tg = S.lfo.targets[v[b + t]];
        if (v[b + t] && tg && tg.p >= 0) L.tags[tg.p] = (L.tags[tg.p] ? L.tags[tg.p] + " " : "") + "LFO " + (i + 1);
      }
    });
    return L;
  }

  // ---------------------------------------------------------------- a setting, as ui.c's cell draws it
  /* its name, its value (a unit after a number set small), a gauge, dots for a choice among a few, or (a TARGET)
   * its name in its track's colour */
  function cell(p, L) {
    const d = P[p], x = Math.min(L.raw(p), d.max), t = word(p, x);
    let val = t, unit = "";
    if (d.kind !== "target") { const m = t.match(/^(.*\d) (\S{1,3})$/); if (m) { val = m[1]; unit = m[2]; } }
    const tag = L.tags[p];
    const col = d.kind === "target" ? TRACK[(S.lfo.targets[x] || {}).track] || C.fog : null;
    let meter = null;
    if (d.kind === "steps" && d.max >= 1 && d.max <= 24) {
      meter = h("span", { class: "dots", "aria-hidden": "true" });
      for (let i = 0; i <= d.max; i++) meter.append(h("i", i === x ? { class: "on" } : null));
    } else if (d.kind !== "target") {
      const f = Math.max(1, Math.min(88, Math.round(x / Math.max(1, d.max) * 88))) / 88;
      meter = h("span", { class: "gauge", "aria-hidden": "true", style: `--f:${r2(f)}` }, h("i"));
    }
    return h("div", { class: "cell" },
      h("div", { class: "cell-n" }, d.name, tag && h("span", { class: "lfo-tag" }, tag)),
      h("div", { class: "cell-v" }, h("span", { class: "v", style: col ? `color:${col}` : null }, val), unit && h("span", { class: "u" }, unit)),
      h("div", { class: "cell-m" }, meter));
  }

  // ---------------------------------------------------------------- WHAT PLAYS: C1 .. C7 under the waterline
  /* the drones' lights stand on it (a chord's root white, its notes glowing), GLINT's held notes ice (their RANGE's
   * octaves ringed), the bass lamp light in the bed */
  function keyboard(sc, L, id) {
    const kw = 12, bw = 7.4, wy = 33, kh = 47, bh = 29, WH = [0, 2, 4, 5, 7, 9, 11];
    const st = litNotes(sc, L), arp = L && L.arpOn ? sc.arp : [], bq = bqNotes(sc, L);
    const g = (x) => `url(#${id}-${x})`;
    const lin = (x, col, op) => s("linearGradient", { id: `${id}-${x}`, x1: 0, y1: 1, x2: 0, y2: 0 },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const rad = (x, col, op) => s("radialGradient", { id: `${id}-${x}` },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const svg = s("svg", { class: "kb", viewBox: "0 0 516 106", role: "img" },
      s("defs", null, lin("cw", C.white, 0.85), lin("cf", C.fog, 0.55), lin("ci", C.ice, 0.9),
        rad("hw", C.white, 0.42), rad("hf", C.fog, 0.24), rad("hi", C.ice, 0.48), rad("hl", C.lamp, 0.55),
        s("linearGradient", { id: `${id}-wl`, x1: 0, y1: 0, x2: 1, y2: 0 },
          [[0, C.slate, 0], [0.18, C.slate, 0.75], [0.5, C.fog, 0.5], [0.82, C.slate, 0.75], [1, C.slate, 0]]
            .map(([o, c, a]) => s("stop", { offset: o, "stop-color": c, "stop-opacity": a })))),
      s("rect", { x: 0, y: 32.5, width: 516, height: 1, fill: g("wl") }));
    const LIGHT = { root: ["w", 24, 11, 1], note: ["f", 14, 8, 1], arp: ["i", 20, 10, 1], arpr: ["i", 8, 6, 0.5] };
    const halos = [], cols = [], wk = [], bk = [], bed = [], octs = [];
    let wi = 0;
    for (let n = 24; n <= 96; n++) {
      const white = WH.includes(n % 12), lit = st[n];
      const cx = white ? wi * kw + 6 : wi * kw, base = white ? C.keyW : C.keyB;
      let fill = base, stroke = white ? "none" : C.keyEdge, sw = white ? 0 : 0.8;
      if (lit === "root") fill = C.white;
      else if (lit === "note") fill = mix(base, C.fog, 0.5);
      else if (lit === "arp") fill = C.ice;
      else if (lit === "arpr") { fill = mix(base, C.ice, 0.14); stroke = C.ice; sw = 0.9; }
      const r = white ? { x: wi * kw + 0.5, y: wy + 1, width: kw - 1, height: kh, rx: 1.5 }
                      : { x: r2(cx - bw / 2), y: wy + 1, width: bw, height: bh, rx: 1 };
      (white ? wk : bk).push(s("rect", { ...r, fill, stroke, "stroke-width": sw }));
      if (lit) {
        const lg = LIGHT[lit];
        halos.push(s("circle", { cx: r2(cx), cy: wy, r: lg[2], fill: g("h" + lg[0]), opacity: lg[3] }));
        cols.push(s("rect", { x: r2(cx - 0.75), y: wy - lg[1], width: 1.5, height: lg[1], fill: g("c" + lg[0]), opacity: lg[3] }));
      }
      if (n === sc.bass) {
        bed.push(s("ellipse", { cx: r2(cx), cy: wy + kh + 5, rx: 18, ry: 6, fill: g("hl") }),
                 s("rect", { x: r2(cx - 5), y: wy + kh + 3.5, width: 10, height: 2, rx: 1, fill: C.lamp }));
      } else if (bq.includes(n)) bed.push(s("rect", { x: r2(cx - 3), y: wy + kh + 3.5, width: 6, height: 2, rx: 1, fill: C.lamp, opacity: 0.75 }));
      if (n % 12 === 0) octs.push(s("text", { x: r2(cx), y: 101, "text-anchor": "middle", fill: C.dim, class: "svgt" }, nn(n)));
      if (white) wi++;
    }
    svg.append(...halos, ...cols, ...wk, ...bk, ...bed, ...octs);
    const notesOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn);
    const dr = sc.chords.map((ch) => nn(ch.root) + (notesOf(ch).length ? " (" + notesOf(ch).join(" ") + ")" : ""));
    let a = "Keyboard, C1 to C7.";
    if (dr.length) a += " Drones latched: " + dr.join(", ") + ".";
    if (arp.length) a += " GLINT's arpeggio: " + arp.map((x) => nn(x + 12 * L.oct)).join(" ") + ", over RANGE " + L.txt(ID.P_ARP_RANGE) + ".";
    if (sc.bass >= 0) a += " Bass: " + nn(sc.bass) + ".";
    if (bq.length) a += " The bass's sequence: " + bq.map(nn).join(" ") + ".";
    if (!dr.length && !arp.length && sc.bass < 0 && !bq.length) a += " Sound only: nothing latched.";
    svg.setAttribute("aria-label", a);
    return svg;
  }

  function legend(sc, L) {
    const items = [];
    const notesOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn).join(" ");
    if (sc.chords.length) items.push([sc.chords.length > 1 ? "DRONES" : "DRONE", "w", sc.chords.map((ch) => [nn(ch.root), notesOf(ch)])]);
    if (L && L.arpOn && sc.arp.length) {
      items.push(["ARP", "i", [[sc.arp.map((x) => nn(x + 12 * L.oct)).join(" "), ""]]]);
      if (L.range > 1) {
        const up = [];
        for (let r = 1; r < L.range; r++) sc.arp.forEach((x) => { if (x + 12 * (L.oct + r) <= 96) up.push(nn(x + 12 * (L.oct + r))); });
        items.push(["RANGE", "r", [["", up.join(" ")]]]);
      }
    }
    if (sc.bass >= 0) items.push(["BASS", "l", [[nn(sc.bass), ""]]]);
    const bq = bqNotes(sc, L);
    if (bq.length) items.push(["BASS SEQ", "l", [[[...new Set(bq)].map(nn).join(" "), ""]]]);
    const box = h("div", { class: "lg" });
    if (!items.length) { box.append(h("span", { class: "lg-only" }, "SOUND ONLY")); return box; }
    for (const [label, sw, groups] of items) {
      box.append(h("div", { class: "lg-i" }, h("span", { class: "sw sw-" + sw, "aria-hidden": "true" }), h("span", { class: "lg-l" }, label),
        groups.map(([root, notes]) => h("span", { class: "lg-g" }, root && h("b", null, root), notes && h("span", null, notes)))));
    }
    return box;
  }

  /* a glint: four arms, the vertical ones a little shorter (seq_scope's) */
  const star = (x, y, a) => {
    const q = a * 0.2, v = a * 0.82;
    return `M${r2(x)} ${r2(y - v)}L${r2(x + q)} ${r2(y - q)}L${r2(x + a)} ${r2(y)}L${r2(x + q)} ${r2(y + q)}` +
           `L${r2(x)} ${r2(y + v)}L${r2(x - q)} ${r2(y + q)}L${r2(x - a)} ${r2(y)}L${r2(x - q)} ${r2(y - q)}Z`;
  };
  const glowDef = (id) => s("defs", null, s("radialGradient", { id: `${id}-sg` },
    s("stop", { offset: 0, "stop-color": C.ice, "stop-opacity": 0.4 }), s("stop", { offset: 1, "stop-color": C.ice, "stop-opacity": 0 })));
  const label = (x, y, anchor, t, fill) => s("text", { x, y, "text-anchor": anchor, fill, class: "svgt" }, t);

  /* GLINT on SEQ: seq_scope's constellation, all 32 steps (past LENGTH slate, in the shade): a star a note at its height
   * (the steps' lowest to highest, an octave at least), larger with ACCENT; a TIE a line on from the last note; a REST
   * a dot on the line */
  function seqStrip(sc, L, id) {
    const x0 = 40, x1 = 504, top = 16, bot = 70, baseY = 84.5, len = L.raw(ID.P_SQ_LEN) + 1;
    const REST = S.seq.rest, TIE = S.seq.tie, ACC = S.seq.accent, n32 = sc.seq.length;
    let lo = 127, hi = 0;
    sc.seq.forEach((b) => { const n = b & 127; if (n !== REST && n !== TIE) { lo = Math.min(lo, n); hi = Math.max(hi, n); } });
    if (hi < lo) { lo = 60; hi = 72; }
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const X = (i) => x0 + i * (x1 - x0) / (n32 - 1);
    const bx = len < n32 ? r2((X(len - 1) + X(len)) / 2) : -10;
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 112", role: "img" }, glowDef(id));
    if (len < n32) svg.append(s("rect", { x: bx, y: 0, width: r2(516 - bx), height: 96, fill: "#000", "fill-opacity": 0.26 }));
    svg.append(s("line", { x1: 26, y1: top, x2: 512, y2: top, stroke: C.steel, "stroke-dasharray": "1 3" }),
               s("line", { x1: 26, y1: bot, x2: 512, y2: bot, stroke: C.steel, "stroke-dasharray": "1 3" }),
               s("rect", { x: 26, y: 84, width: 486, height: 1, fill: C.steel }));
    const ticks = [], ties = [], rests = [], glows = [], stars = [], cores = [];
    let py = -1, px = 0;
    for (let i = 0; i < n32; i++) {
      const x = X(i), inn = i < len, b = sc.seq[i], n = b & 127;
      ticks.push(s("rect", { x: r2(x - 0.5), y: 87, width: 1, height: i % 4 ? 2 : 4, fill: i % 4 ? C.steel : C.slate }));
      if (n === REST) { rests.push(s("circle", { cx: r2(x), cy: baseY, r: inn ? 1.8 : 1.3, fill: inn ? C.ice : C.slate, opacity: inn ? 0.8 : 0.6 })); py = -1; continue; }
      if (n === TIE) {
        if (py >= 0) ties.push(s("line", { x1: r2(px), y1: r2(py), x2: r2(x), y2: r2(py), stroke: inn ? C.ice : C.slate, "stroke-opacity": inn ? 0.6 : 0.45, "stroke-width": 1.3, "stroke-linecap": "round" }));
        px = x;
        continue;
      }
      const y = bot - (n - lo) * (bot - top) / (hi - lo), acc = (b & ACC) !== 0, a = acc ? 7.5 : 4.6;
      if (inn) glows.push(s("circle", { cx: r2(x), cy: r2(y), r: acc ? 13 : 8, fill: `url(#${id}-sg)` }));
      stars.push(s("path", { d: star(x, y, a), fill: inn ? C.ice : C.slate, "fill-opacity": inn ? 0.95 : 0.7 }));
      cores.push(s("circle", { cx: r2(x), cy: r2(y), r: acc ? 1.9 : 1.35, fill: inn ? C.fog : C.slate }));
      py = y; px = x;
    }
    svg.append(...ticks);
    if (len < n32) svg.append(s("line", { x1: bx, y1: 4, x2: bx, y2: 96, stroke: C.edge, "stroke-dasharray": "2 3" }));
    svg.append(...ties, ...rests, ...glows, ...stars, ...cores);
    const sh = 12 * L.oct;
    svg.append(label(4, r2(top + 3.5), "start", nn(hi + sh), C.dim), label(4, r2(bot + 3.5), "start", nn(lo + sh), C.dim),
               label(r2(X(0)), 106, "middle", "1", C.dim), label(r2(X(n32 - 1)), 106, "middle", String(n32), C.dim));
    if (len < n32) svg.append(label(r2(X(len - 1) + 4), 106, "end", "LENGTH " + L.txt(ID.P_SQ_LEN), C.fog));
    const say = sc.seq.slice(0, len).map((b) => {
      const n = b & 127;
      return n === REST ? "REST" : n === TIE ? "TIE" : nn(n + sh) + (b & ACC ? " with ACCENT" : "");
    });
    svg.setAttribute("aria-label", `GLINT's sequence: ${len} of its ${n32} steps play, ${say.join(", ")}.`);
    return svg;
  }

  /* GLINT on ARP: its MODE drawn as the walk it takes over the held notes and their RANGE (UP, DOWN, UPDOWN as app.c
   * steps them; RANDOM and DRIFT, a repeatable draw of their kind: DRIFT a step either way, or stay) */
  function arpStrip(sc, L, id) {
    const x0 = 40, x1 = 504, top = 12, bot = 62, mode = L.raw(ID.P_ARP_MODE), notes = [];
    for (let r = 0; r < L.range; r++) sc.arp.forEach((a) => notes.push(a + 12 * (L.oct + r)));
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 74", role: "img" }, glowDef(id));
    if (!notes.length) {
      svg.append(s("line", { x1: 26, y1: 37, x2: 512, y2: 37, stroke: C.steel, "stroke-dasharray": "1 3" }),
                 label(258, 40.5, "middle", "NO NOTES HELD", C.dim));
      svg.setAttribute("aria-label", `GLINT's arpeggio, MODE ${L.txt(ID.P_ARP_MODE)}: no notes held.`);
      return svg;
    }
    const count = notes.length;
    let lo = Math.min(...notes), hi = Math.max(...notes);
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const Y = (n) => bot - (n - lo) * (bot - top) / (hi - lo);
    notes.forEach((n, j) => svg.append(s("line", { x1: 26, y1: r2(Y(n)), x2: 512, y2: r2(Y(n)), stroke: j < sc.arp.length ? C.ice : C.slate, "stroke-opacity": j < sc.arp.length ? 0.32 : 0.7, "stroke-dasharray": "1 3" })));
    const DRIFT = [0, 1, 1, 0, 1, -1, 0, 1, 1, 1, 0, -1, -1, 1, 1, 0, 1, -1, -1, 0, -1, -1, 1, 0];
    let idx = -1, dir = 1, seed = 0x2545f491;
    const rnd = () => { seed ^= seed << 13; seed >>>= 0; seed ^= seed >>> 17; seed ^= seed << 5; seed >>>= 0; return seed; };
    const steps = 24, pts = [], glows = [], stars = [], cores = [];
    for (let i = 0; i < steps; i++) {
      if (mode === 0) idx = (idx + 1) % count;
      else if (mode === 1) idx = idx <= 0 ? count - 1 : idx - 1;
      else if (mode === 2) { idx += dir; if (idx >= count) { idx = count > 1 ? count - 2 : 0; dir = -1; } else if (idx < 0) { idx = count > 1 ? 1 : 0; dir = 1; } }
      else if (mode === 3) idx = rnd() % count;
      else { idx = idx < 0 ? 0 : idx + DRIFT[i]; if (idx < 0) idx = count > 1 ? 1 : 0; if (idx >= count) idx = count > 1 ? count - 2 : 0; }
      const x = x0 + i * (x1 - x0) / (steps - 1), y = Y(notes[idx]);
      pts.push(r2(x) + "," + r2(y));
      glows.push(s("circle", { cx: r2(x), cy: r2(y), r: 6.5, fill: `url(#${id}-sg)` }));
      stars.push(s("path", { d: star(x, y, 3.8), fill: C.ice, "fill-opacity": 0.95 }));
      cores.push(s("circle", { cx: r2(x), cy: r2(y), r: 1.1, fill: C.fog }));
    }
    svg.append(s("polyline", { points: pts.join(" "), fill: "none", stroke: C.ice, "stroke-opacity": 0.5, "stroke-width": 1.1, "stroke-linejoin": "round", "stroke-linecap": "round" }),
               ...glows, ...stars, ...cores, label(4, r2(top + 3.5), "start", nn(hi), C.dim), label(4, r2(bot + 3.5), "start", nn(lo), C.dim));
    svg.setAttribute("aria-label", `GLINT's arpeggio, MODE ${L.txt(ID.P_ARP_MODE)}: its walk over ${sc.arp.map((x) => nn(x + 12 * L.oct)).join(" ")} and RANGE ${L.txt(ID.P_ARP_RANGE)}, ${nn(lo)} to ${nn(hi)}.`);
    return svg;
  }

  /* the bass's sequence as it plays (app.c bseq_note), its STEPS: a NOTE moved TRANSPOSE's steps along the scale, then
   * BASS's OCTAVE, C1 at the lowest (bass_fold); a CUT -1, a HOLD -2 */
  function bassPlays(bq, L) {
    const deg = DEGREES[L.txt(ID.P_SCALE)], r = L.raw(ID.P_ROOT) % 12, oct = 12 * (L.raw(ID.P_BS_OCTAVE) - 2);
    const inS = (n) => deg.includes(((n - r) % 12 + 12) % 12) ? 1 : 0;
    const shift = (note, d) => {                     // (harmony.c scale_shift)
      let base = note;
      while (!inS(base)) base--;
      note -= base;
      while (d > 0) { base++; d -= inS(base); }
      while (d < 0) { base--; d += inS(base); }
      return Math.max(1, Math.min(126, note + base));
    };
    return bq.notes.slice(0, bq.steps + 1).map((b) => {
      if (b === S.bseq.cut) return -1;
      if (b === S.bseq.hold) return -2;
      let n = (deg ? shift(b, bq.trans - P[ID.P_BQ_TRANS].def) : b) + oct;
      while (n < 24) n += 12;
      return Math.min(n, 127);
    });
  }
  /* the notes its sequence plays (none while SEQUENCE is off) */
  const bqNotes = (sc, L) => (L && sc.bseq && sc.bseq.on ? bassPlays(sc.bseq, L).filter((n) => n >= 0) : []);
  /* BASS's sequence: a line of time, each step as wide as its LENGTH; a NOTE a lamp line at its height, on through the
   * HOLDs after it (round from the end: the sequence loops), a CUT a gap */
  function bassStrip(bq, L, id) {
    const x0 = 40, x1 = 504, top = 12, bot = 50, axis = 62, notes = bassPlays(bq, L);
    const len = notes.map((_, i) => S.bseq.ticks[bq.lens[i] % S.bseq.ticks.length]), total = len.reduce((a, t) => a + t, 0);
    const words = BQ_LEN != null && P[BQ_LEN] ? P[BQ_LEN].texts : S.bseq.ticks.map(String), at = [0];
    len.forEach((t) => at.push(at[at.length - 1] + t));
    const X = (t) => x0 + t * (x1 - x0) / total;
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 82", role: "img" },
      s("defs", null, s("radialGradient", { id: `${id}-bg` },
        s("stop", { offset: 0, "stop-color": C.lamp, "stop-opacity": 0.45 }), s("stop", { offset: 1, "stop-color": C.lamp, "stop-opacity": 0 }))),
      s("line", { x1: 26, y1: top, x2: 512, y2: top, stroke: C.steel, "stroke-dasharray": "1 3" }),
      s("line", { x1: 26, y1: bot, x2: 512, y2: bot, stroke: C.steel, "stroke-dasharray": "1 3" }),
      s("rect", { x: 26, y: axis, width: 486, height: 1, fill: C.steel }));
    const sound = notes.filter((n) => n >= 0);
    let lo = Math.min(...sound), hi = Math.max(...sound);
    if (!sound.length) { lo = 36; hi = 48; }
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const Y = (n) => bot - (n - lo) * (bot - top) / (hi - lo);
    let cur = -1;                                    // (what sounds as it comes round: the end's)
    notes.forEach((n) => { if (n !== -2) cur = n; });
    const runs = [];
    notes.forEach((n, i) => {
      if (n >= 0) runs.push({ n, a: at[i], b: at[i + 1], on: true });
      else if (n === -2 && cur >= 0) {
        const last = runs[runs.length - 1];
        if (last && last.b === at[i]) last.b = at[i + 1]; else runs.push({ n: cur, a: at[i], b: at[i + 1], on: false });
      }
      if (n !== -2) cur = n;
    });
    const glows = [], lines = [], heads = [];
    for (const r of runs) {
      const y = r2(Y(r.n)), xa = r2(X(r.a)), xb = r2(Math.max(X(r.a) + 1, X(r.b) - 2));
      glows.push(s("line", { x1: xa, y1: y, x2: xb, y2: y, stroke: C.lamp, "stroke-opacity": 0.16, "stroke-width": 7, "stroke-linecap": "round" }));
      lines.push(s("line", { x1: xa, y1: y, x2: xb, y2: y, stroke: C.lamp, "stroke-width": 2, "stroke-linecap": "round" }));
      if (r.on) heads.push(s("circle", { cx: xa, cy: y, r: 7, fill: `url(#${id}-bg)` }), s("circle", { cx: xa, cy: y, r: 2.4, fill: C.lamp }),
                          s("circle", { cx: xa, cy: y, r: 1, fill: C.fog }));
    }
    svg.append(...at.map((t, i) => s("rect", { x: r2(X(t) - 0.5), y: axis + 2, width: 1, height: i % 4 ? 2 : 4, fill: i % 4 ? C.steel : C.slate })),
               ...glows, ...lines, ...heads);
    const tr = bq.trans !== P[ID.P_BQ_TRANS].def ? "TRANSPOSE " + word(ID.P_BQ_TRANS, bq.trans) : "";
    svg.append(label(4, r2(top + 3.5), "start", nn(hi), C.dim), label(4, r2(bot + 3.5), "start", nn(lo), C.dim),
               label(r2(X(0)), 77, "middle", "1", C.dim), label(r2(x1), 77, "end", (tr ? tr + " · " : "") + "STEPS " + notes.length, C.fog));
    const say = notes.map((n, i) => (n === -1 ? "CUT" : n === -2 ? "HOLD" : nn(n)) + " " + words[bq.lens[i] % words.length]);
    svg.setAttribute("aria-label", `BASS's sequence: ${notes.length} steps${tr ? ", " + tr : ""}: ${say.join(", ")}.`);
    return svg;
  }

  // ---------------------------------------------------------------- the sections' small pictures
  const glyphSvg = (w, ht) => s("svg", { class: "sec-gl", width: w, height: ht, viewBox: `0 0 ${w} ${ht}`, "aria-hidden": "true" });
  /* the play page's strata: CHOIR the current, PARTIALS the crystals, BREATH the haze, WARMTH embers */
  function strata(L) {
    const g = glyphSvg(52, 15);
    [[C.mist, 1.2, "none", 1, "butt"], [C.ice, 1.2, "2.2 1.8", 1, "butt"], [C.mist, 3, "none", 0.42, "round"], [C.ember, 1.3, "1 2.4", 1, "butt"]].forEach((st, i) => {
      const y = r2(1.75 + i * 3.85), n = L.raw(SOUND[i]) / 127 * 48;
      g.append(s("line", { x1: 2, x2: 50, y1: y, y2: y, stroke: C.steel, "stroke-width": 1, "stroke-dasharray": "1 2" }));
      if (n > 0.5) g.append(s("line", { x1: 2, x2: r2(2 + n), y1: y, y2: y, stroke: st[0], "stroke-width": st[1], "stroke-dasharray": st[2], "stroke-opacity": st[3], "stroke-linecap": st[4] }));
    });
    return g;
  }
  /* app.c's lfo_shape: SHAPE at x (0..1) of turn c, -1..1 (RANDOM and DRIFT from a repeatable hash) */
  function lfoShape(sh, x, c, seed) {
    const rv = (q) => { let v = (q * 374761393 + seed * 668265263) >>> 0; v = Math.imul(v ^ (v >>> 13), 1274126177) >>> 0; return ((v ^ (v >>> 16)) & 65535) / 32767.5 - 1; };
    if (sh === 0) return Math.sin(2 * Math.PI * x);
    if (sh === 1) return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4;
    if (sh === 2) return 2 * x - 1;
    if (sh === 3) return 1 - 2 * x;
    if (sh === 4) return x < 0.5 ? 1 : -1;
    if (sh === 5) return rv(c);
    const a = rv(c), b = rv(c + 1);
    return a + (b - a) * x * x * (3 - 2 * x);
  }
  /* lfo_scope's: the shape over a turn (four for RANDOM, DRIFT), as deep as DEPTH, from POLARITY's side */
  const lfoOn = (L, b) => L.raw(b + 2) > 0 && [4, 5, 6, 7].some((t) => L.raw(b + t) > 0);
  function wave(L, b, i) {
    const g = glyphSvg(46, 14);
    const sh = L.raw(b) % 7, depth = L.raw(b + 2), pol = L.raw(b + 3) % 3, on = lfoOn(L, b);
    const d = 0.2 + 0.8 * depth / 127, top = 1.5, bot = 12.5, W = 44, rnd = sh >= 5, turns = rnd ? 4 : 1;
    const base = pol === 1 ? bot : pol === 2 ? top : (top + bot) / 2;
    let path = "";
    for (let x = 0; x <= W; x++) {
      const u = x / W * turns, c = Math.floor(u), f = rnd ? u - c : Math.min(u, 0.9999), v = lfoShape(sh, f, c, i + 1);
      const y = pol === 1 ? bot - 0.5 * (v + 1) * (bot - top) * d : pol === 2 ? top + 0.5 * (v + 1) * (bot - top) * d : base - v * 0.5 * (bot - top) * d;
      path += (x ? "L" : "M") + (x + 1) + " " + r2(y);
    }
    g.append(s("line", { x1: 1, x2: 45, y1: r2(base), y2: r2(base), stroke: C.steel, "stroke-width": 1 }),
             s("path", { d: path, fill: "none", stroke: on ? C.ice : C.slate, "stroke-width": 1.3, "stroke-linejoin": "round" }));
    return g;
  }
  /* the twelve notes from the root: the root ice, the scale's fog */
  function scaleGl(L) {
    const deg = DEGREES[L.txt(ID.P_SCALE)];
    if (!deg) return null;
    const g = glyphSvg(62, 8);
    for (let i = 0; i < 12; i++) {
      const inS = deg.includes(i);
      g.append(s("circle", { cx: r2(3 + i * 5.1), cy: 4, r: i === 0 ? 2.3 : inS ? 1.5 : 0.9, fill: i === 0 ? C.ice : inS ? C.fog : C.slate }));
    }
    return g;
  }
  /* MIXER: each track's level in its colour (DRONE fog, BASS lamp, GLINT ice, CLOUD haze lit), 100 marked */
  function faders(L) {
    const g = glyphSvg(32, 15);
    [[ID.P_MX_DRONE, C.fog], [ID.P_MX_BASS, C.lamp], [ID.P_MX_GLINT, C.ice], [ID.P_MX_CLOUD, C.cloud]].forEach(([p, col], i) => {
      const x = 2 + i * 8, ht = L.raw(p) / 127 * 14;
      g.append(s("rect", { x: x + 1, y: 0.5, width: 1, height: 14, fill: C.steel }),
               s("rect", { x, y: r2(14.5 - ht), width: 3, height: r2(ht), fill: col }),
               s("rect", { x: x - 1.5, y: r2(14.5 - 100 / 127 * 14), width: 6, height: 0.8, fill: C.mist }));
    });
    return g;
  }
  const sv = (t, col) => h("span", { class: "sig-v", style: col ? `color:${col}` : null }, t);
  const sl = (t) => h("span", { class: "sig-l" }, t);
  const lv = (L, name, p) => (p >= 0 ? [sl(name), sv(L.txt(p))] : []);
  /* each section's line when closed: a picture, a few values (BASS: SEQ, its sequence on) */
  function signature(sec, L, bq) {
    const lfo = sec.name.match(/^LFO (\d+)$/);
    if (lfo) {
      const i = +lfo[1], b = S.lfo.base[i - 1];
      if (b === undefined) return { gl: null, sig: [] };
      const tg = [4, 5, 6, 7].map((t) => L.raw(b + t)).filter((t) => t > 0)
        .map((t) => { const x = S.lfo.targets[t] || { name: word(b + 4, t), track: "drone" }; return sv(x.name, TRACK[x.track]); });
      return { gl: wave(L, b, i), sig: lfoOn(L, b) ? [sv(L.txt(b)), sv(L.txt(b + 1), C.text2), ...tg] : [sv("OFF", C.dim)] };
    }
    switch (sec.name) {
      case "DRONE": return { gl: strata(L), sig: [sv(L.txt(ID.P_DR_PRESET))] };
      case "GLINT": return { gl: null, sig: [sv(L.txt(ID.P_GL_PRESET)), sv(L.txt(ID.P_SQ_TYPE), C.ice)] };
      case "BASS": return { gl: null, sig: [sv(L.txt(ID.P_BS_MODEL)), bq && bq.on && sv("SEQ", C.lamp), ...lv(L, "FILTER", pp("BASS", "FILTER"))] };
      case "FX": return { gl: null, sig: [...lv(L, "CLOUD", pp("CLOUD", "DECAY")), ...lv(L, "PUMP", pp("PUMP", "RATE"))] };
      case "HARMONY": return { gl: scaleGl(L), sig: [sv(L.txt(ID.P_ROOT) + " " + L.txt(ID.P_SCALE)), sv(L.txt(ID.P_CHORD), C.text2), sv(L.txt(ID.P_VOICING), C.text2)] };
      case "MIXER": return { gl: faders(L), sig: [] };
      case "MASTERING": return { gl: null, sig: [...lv(L, "THRESHOLD", pp("MASTERING", "THRESHOLD")), ...lv(L, "RATIO", pp("MASTERING", "RATIO"))] };
      default: return { gl: null, sig: [] };
    }
  }
  const chev = () => s("svg", { class: "chev", width: 10, height: 10, viewBox: "0 0 10 10", "aria-hidden": "true" },
    s("path", { d: "M3.5 1.5L7 5L3.5 8.5", fill: "none", "stroke-width": 1.4, "stroke-linecap": "round", "stroke-linejoin": "round" }));

  /* where a page is on the FM-1: its block of twelve buttons (two rows of six, as on the panel), the ones that lead
   * there lit, then their names in quiet text. A picture of the instrument, never a button: no frame, no hover */
  const PANEL = [["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]];
  function panelMap(lit) {
    const w = 6, ht = 3.8, gx = 2, gy = 2.4, W = 6 * w + 5 * gx, H = r2(2 * ht + gy);
    const svg = s("svg", { class: "dh-map", width: W, height: H, viewBox: `0 0 ${W} ${H}`, "aria-hidden": "true" });
    PANEL.forEach((rw, y) => rw.forEach((b, x) => svg.append(s("rect", { class: lit.includes(b) ? "on" : null, x: x * (w + gx), y: r2(y * (ht + gy)), width: w, height: ht, rx: 0.9 }))));
    return svg;
  }
  const hints = (keys) => h("span", { class: "dh" }, panelMap(keys),
    h("span", { class: "dh-k" }, h("span", { class: "sr-only" }, "On the FM-1: "), keys.map((t, i) => [i ? h("span", { class: "dh-sep", "aria-hidden": "true" }, " · ") : null, h("b", null, t)])));

  /* the sections, each a disclosure; a page's settings as its K1 .. K4 have them, its own button under its name
   * (built when first opened); BASS's sequence (bq), drawn first in it while on */
  function sections(L, bq) {
    const box = h("div", { class: "secs" });
    for (const sec of SECTIONS) {
      const pages = sec.pages.filter((pg) => shownFor(pg, L));
      if (!pages.length) continue;
      const { gl, sig } = signature(sec, L, bq);
      const d = h("details", { class: "sec" },
        h("summary", null, h("span", { class: "sec-row" }, chev(), h("span", { class: "sec-name" }, sec.name),
          h("span", { class: "sec-mid" }, gl, sig), hints(sec.keys))));
      let built = false;
      d.addEventListener("toggle", () => {
        if (!d.open || built) return;
        built = true;
        d.append(h("div", { class: "sec-body" }, sec.name === "BASS" && bq && bq.on && h("div", { class: "scr" }, bassStrip(bq, L, "c" + (++uid))),
          pages.map((pg) => h("div", { class: "pg" }, h("div", { class: "pg-name" }, pg.name, h("span", { class: "pg-k" }, h("span", { class: "sr-only" }, ", on "), pg.button)),
            pg.keep.map((p) => cell(p, L))))));
      });
      box.append(d);
    }
    return box;
  }

  // ---------------------------------------------------------------- each scene's own shard
  /* a shard of ice, the same for the same scene everywhere (seeded by its bytes), half under the waterline as the
   * logo is: its upper half lit, its lower half sea blue */
  const hash = (sc) => {
    let x = 2166136261 >>> 0;
    const feed = (b) => { x ^= b & 255; x = Math.imul(x, 16777619) >>> 0; };
    if (sc.bytes) sc.bytes.forEach(feed); else for (const c of sc.name + "/" + sc.slot) feed(c.charCodeAt(0));
    return x;
  };
  function shardPath(seed) {
    let t = seed >>> 0;
    const rnd = () => { t = (t + 0x6D2B79F5) >>> 0; let r = Math.imul(t ^ (t >>> 15), 1 | t); r ^= r + Math.imul(r ^ (r >>> 7), 61 | r); return ((r ^ (r >>> 14)) >>> 0) / 4294967296; };
    const j = (a, b) => a + (b - a) * rnd();
    const up = j(11.5, 15), dn = j(7.5, 10.5), w = j(4.8, 6.6), tilt = j(-1.6, 1.6);
    const pts = [[tilt, -up], [w * j(0.5, 0.95), -up * j(0.3, 0.68)], [w * j(0.8, 1.05), dn * j(-0.05, 0.35)],
                 [w * j(-0.25, 0.35), dn], [-w * j(0.6, 1), dn * j(0.15, 0.55)], [-w * j(0.75, 1.05), -up * j(0.1, 0.5)]];
    const d = "M" + pts.map(([x, y]) => `${r2(x)} ${r2(y)}`).join("L") + "Z";
    const mid = [r2(j(-1.6, 1.6)), r2(j(-3, 1))];
    return { d, facet: `M${r2(pts[0][0])} ${r2(pts[0][1])}L${mid[0]} ${mid[1]}L${r2(pts[3][0])} ${r2(pts[3][1])}` };
  }
  /* size: its height in px; withLine: a short waterline through it (where no line runs behind it) */
  function shard(sc, size, withLine) {
    const svg = s("svg", { class: "shard-g is-" + (sc ? sc.state : "empty"), viewBox: "-13 -17 26 30", width: r2(size * 26 / 30), height: size, "aria-hidden": "true" });
    if (withLine) svg.append(s("line", { class: "g-line", x1: -13, x2: 13, y1: 0, y2: 0 }));
    if (!sc || sc.state === "empty") { svg.append(s("circle", { class: "g-dot", cx: 0, cy: 0, r: 1.7 })); return svg; }
    const { d, facet } = shardPath(hash(sc));
    const scale = sc.state === "factory" ? 0.78 : 1;
    const g = s("g", { transform: `scale(${scale})` });
    for (const part of ["dn", "up"]) {
      g.append(s("g", { class: "g-" + part, "clip-path": `url(#rv-clip-${part})` },
        s("path", { class: "g-body", d }), s("path", { class: "g-facet", d: facet })));
    }
    svg.append(g);
    return svg;
  }
  document.body.append(s("svg", { width: 0, height: 0, "aria-hidden": "true", style: "position:absolute" }, s("defs", null,
    s("clipPath", { id: "rv-clip-up", clipPathUnits: "userSpaceOnUse" }, s("rect", { x: -20, y: -30, width: 40, height: 30 })),
    s("clipPath", { id: "rv-clip-dn", clipPathUnits: "userSpaceOnUse" }, s("rect", { x: -20, y: 0, width: 40, height: 30 })))));


  // ================================================================ what a scene plays, in one line
  const WORD = { saved: "SAVED", factory: "FACTORY", empty: "EMPTY" };
  /* the notes a scene lights, each once, the strongest light kept: a chord's root, its notes, GLINT's held notes
   * (at its OCTAVE), their RANGE's octaves above */
  function litNotes(sc, L) {
    const PRI = { arpr: 1, arp: 2, note: 3, root: 4 }, st = {};
    const set = (n, what) => { if (n >= 24 && n <= 96 && (!st[n] || PRI[what] > PRI[st[n]])) st[n] = what; };
    sc.chords.forEach((ch) => { set(ch.root, "root"); ch.notes.forEach((n) => { if (n !== ch.root) set(n, "note"); }); });
    if (L && L.arpOn) sc.arp.forEach((a) => { set(a + 12 * L.oct, "arp"); for (let r = 1; r < L.range; r++) set(a + 12 * (L.oct + r), "arpr"); });
    return st;
  }
  /* what plays, in one line: C1 .. C7 on a waterline, the drones standing on it (roots white, notes fog), GLINT's held
   * notes ice, its sequence's notes ice points above, the bass lamp light under; while it sounds, the line moves */
  function mini(sc, L) {
    const X = (n) => `calc(6px + (100% - 12px) * ${r2((n - 24) / 72)})`;
    const st = litNotes(sc, L), box = h("div", { class: "ms", role: "img" }, h("span", { class: "ms-line" }));
    for (let c = 24; c <= 96; c += 12) box.append(h("span", { class: "ms-c", style: `left:${X(c)}` }));
    for (const [n, what] of Object.entries(st)) box.append(h("span", { class: "ms-n ms-" + what, style: `left:${X(+n)}` }));
    if (L && L.seqOn) {
      const seen = new Set();
      sc.seq.slice(0, L.raw(ID.P_SQ_LEN) + 1).forEach((b) => {
        const n = (b & 127) + 12 * L.oct;
        if ((b & 127) !== S.seq.rest && (b & 127) !== S.seq.tie && n >= 24 && n <= 96 && !seen.has(n)) {
          seen.add(n);
          box.append(h("span", { class: "ms-seq", style: `left:${X(n)}` }));
        }
      });
    }
    if (sc.bass >= 24 && sc.bass <= 96) box.append(h("span", { class: "ms-bass", style: `left:${X(sc.bass)}` }));
    const bq = bqNotes(sc, L);
    new Set(bq).forEach((n) => { if (n <= 96) box.append(h("span", { class: "ms-bq", style: `left:${X(n)}` })); });
    if (!Object.keys(st).length && sc.bass < 0 && !(L && L.seqOn) && !bq.length) box.append(h("span", { class: "ms-only" }, "SOUND ONLY"));
    box.append(s("svg", { class: "ms-wave", viewBox: "0 0 100 20", preserveAspectRatio: "none", "aria-hidden": "true" }, s("polyline", { points: "0,10 100,10" })));
    box.setAttribute("aria-label", playsWords(sc, L));
    return box;
  }
  /* a scene's line of what plays, on its own (the dialogs' rows) */
  const line = (sc) => mini(sc, sc.current ? look(sc.v) : null);
  function playsWords(sc, L) {
    const notesOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn);
    const dr = sc.chords.map((ch) => nn(ch.root) + (notesOf(ch).length ? " (" + notesOf(ch).join(" ") + ")" : ""));
    const arp = L && L.arpOn ? sc.arp : [];
    let a = "What plays:";
    if (dr.length) a += " drones " + dr.join(", ") + ".";
    if (arp.length) a += " GLINT's arpeggio " + arp.map((x) => nn(x + 12 * L.oct)).join(" ") + ".";
    if (L && L.seqOn) a += " GLINT's sequence.";
    if (sc.bass >= 0) a += " Bass " + nn(sc.bass) + ".";
    if (bqNotes(sc, L).length) a += " The bass's sequence.";
    if (a === "What plays:") a = "Sound only: nothing latched.";
    return a;
  }
  const playIcon = () => s("svg", { class: "play-i", viewBox: "0 0 16 16", width: 14, height: 14, "aria-hidden": "true" },
    s("path", { class: "i-play", d: "M5.2 3.1v9.8l7.6-4.9z" }), s("rect", { class: "i-stop", x: 4.3, y: 4.3, width: 7.4, height: 7.4, rx: 1.2 }));
  function plays(sc, L, id) {
    return h("section", { class: "sh-plays", "aria-labelledby": `${id}-wp` },
      h("div", { class: "sh-sub" }, h("h4", { id: `${id}-wp` }, "WHAT PLAYS"), h("span", { class: "sh-range", "aria-hidden": "true" }, "C1 – C7")),
      h("div", { class: "scr" }, keyboard(sc, L, id)), legend(sc, L));
  }
  function glint(sc, L, id) {
    const cells = L.seqOn ? GL_CELLS.seq : L.arpOn ? GL_CELLS.arp : GL_CELLS.off;
    return h("section", { class: "sh-glint", "aria-labelledby": `${id}-gl` },
      h("div", { class: "sh-sub gl-sub" }, h("h4", { id: `${id}-gl` }, "GLINT"), h("span", { class: "gl-type" + (L.type ? "" : " is-off") }, L.txt(ID.P_SQ_TYPE)),
        h("span", { class: "gl-meta" }, h("span", { class: "sig-l" }, "PRESET"), h("span", { class: "gl-v" }, L.txt(ID.P_GL_PRESET)),
          h("span", { class: "sig-l" }, "OCTAVE"), h("span", { class: "gl-v" }, L.txt(ID.P_GL_OCTAVE)))),
      L.seqOn && h("div", { class: "scr" }, seqStrip(sc, L, id)), L.arpOn && h("div", { class: "scr" }, arpStrip(sc, L, id)),
      h("div", { class: "cells4" }, cells.filter((p) => p >= 0 && p < N.NSOUND).map((p) => cell(p, L))));
  }
  function metaOf(L) {
    return h("dl", { class: "sh-meta" }, [["DRONE PRESET", L.txt(ID.P_DR_PRESET)], ["KEY", L.txt(ID.P_ROOT) + " " + L.txt(ID.P_SCALE)],
      ["CHORD", L.txt(ID.P_CHORD)], ["VOICING", L.txt(ID.P_VOICING)]].map(([dt, dd]) => h("div", null, h("dt", null, dt), h("dd", null, dd))));
  }
  const REG = new Map();             // ▶'s key -> {art, sc, o}

  /* the settings, as a row (Quick Backup's, under the sixteen slots): SETTINGS, one line (the last scene, the tempo,
   * what comes back at power-on), opened in place into every setting kept beside the scenes */
  function settingsRow(st, scenes, o = {}) {
    const id = "c" + (++uid), L = st && st.current ? look(st.v) : null, last = L ? scenes[st.scene] : null;
    const tempo = L ? word(ID.P_TEMPO, st.tempo) : "";
    const words = !st ? "No settings saved yet: the FM-1 starts from FRASIL’s own."
      : !L ? "Settings from an earlier FRASIL: brought up to date when restored."
      : `Last scene ${two(st.scene + 1)}${last ? " " + last.name : ""} · tempo ${tempo} · what comes back at power-on`;
    const art = h("article", { class: "sr-item is-set", id: o.id || "settings", "aria-labelledby": `${id}-n` });
    art.append(h("div", { class: "sr" }, h("span"),
      h("span", { class: "nm" }, h("span", { class: "set-gl", "aria-hidden": "true" }, L ? faders(L) : null),
        h("span", { class: "nm-t" }, h("h3", { class: "nm-n", id: `${id}-n`, tabindex: "-1" }, "SETTINGS"), h("span", { class: "nm-d" }, words))),
      L ? h("button", { type: "button", class: "sr-open", "aria-expanded": "false", "aria-controls": `${id}-more`, "aria-label": "Open SETTINGS", onclick: () => openRow(art) }, I.up(15)) : null),
      h("div", { class: "oc-more", id: `${id}-more`, inert: true }, h("div", { class: "oc-in" })));
    if (!L) return art;
    const set = [
      ["LAST SCENE", two(st.scene + 1), last ? last.name : ""], [P[ID.P_TEMPO].name, tempo],
      ["METRONOME", st.metro ? "ON" : "OFF"], [P[ID.P_KEYS].name, word(ID.P_KEYS, st.keys)],
      ["OCTAVE", (st.octave > 0 ? "+" : "") + st.octave], [P[ID.P_CANAL].name, word(ID.P_CANAL, st.channel)],
      [P[ID.P_CH_GLINT].name, word(ID.P_CH_GLINT, st.ch_glint)], [P[ID.P_CH_BASS].name, word(ID.P_CH_BASS, st.ch_bass)],
      [P[ID.P_MORPH].name, word(ID.P_MORPH, st.morph)], [P[ID.P_LOAD].name, word(ID.P_LOAD, st.load)],
      ["MIDI LEARN", st.learned.length ? String(st.learned.length) : "NONE", st.learned.length ? (st.learned.length > 1 ? "CCS" : "CC") : ""],
    ];
    art._build = () => {
      const left = h("div", { class: "set-left" },
        h("p", { class: "set-lede" }, "Kept beside the scenes, and restored with everything: what the FM-1 comes back to at power-on."),
        h("dl", { class: "set-grid" }, set.map(([dt, v, small]) => {
          const m = !small && v.match(/^(.*\d) (\S{1,3})$/);
          return h("div", null, h("dt", null, dt), h("dd", null, m ? m[1] : v, (small || (m && m[2])) && h("span", { class: "u" }, small || m[2])));
        })));
      if (st.learned.length) {
        const pageOf = (p) => { const pg = S.pages.find((x) => x.p.includes(p)); return pg ? pg.name + " · " : ""; };
        left.append(h("ul", { class: "learned", role: "list", "aria-label": "MIDI LEARN" },
          st.learned.map((l) => h("li", null, h("span", { class: "sig-l" }, "CC " + l.cc), h("span", { class: "sig-v" }, P[l.p] ? pageOf(l.p) + P[l.p].name : "#" + l.p)))));
      }
      return [h("div", { class: "sh-body set-in" }, left, h("div", { class: "set-right" },
        h("section", { class: "set-sound", "aria-labelledby": `${id}-pw` },
          h("div", { class: "sh-sub" }, h("h4", { id: `${id}-pw` }, "THE SOUND AT POWER-ON"), h("span", { class: "sh-range" }, "SILENT UNTIL YOU PLAY")),
          metaOf(L)), sections(L, st.bseq)))];
    };
    return art;
  }
  const ticked = (art, on) => { art.classList.toggle("kept", on); const cb = art.querySelector(".ck input"); if (cb) cb.checked = on; };
  const say = (art, ...parts) => { const p = art && art.querySelector(".sh-say"); if (p) fill(p, ...parts); };

  /* FLIP: the rows in view measured, the change made, each one moved back and let go */
  function glide(box, change, watch) {
    if (!box || reduce.matches || !box.animate) { change(); return watch ? watch.getBoundingClientRect() : null; }
    const near = [...box.children].filter((el) => { const r = el.getBoundingClientRect(); return r.bottom > -innerHeight && r.top < innerHeight * 2; });
    const before = new Map(near.map((el) => [el, el.getBoundingClientRect()]));
    change();
    let mine = null;
    for (const [el, a] of before) {
      const b = el.getBoundingClientRect();
      if (el === watch) mine = b;
      const dx = a.left - b.left, dy = a.top - b.top;
      if (Math.abs(dx) < 1 && Math.abs(dy) < 1) continue;
      el.animate([{ transform: `translate(${dx}px, ${dy}px)` }, { transform: "none" }], { duration: 520, easing: "cubic-bezier(.2,.7,.2,1)" });
    }
    return mine || (watch ? watch.getBoundingClientRect() : null);
  }

  // ================================================================ the River's lists: a shard as a row on the river
  /* the River's line icons (16 px) */
  const ico = (w, ...paths) => s("svg", { viewBox: "0 0 16 16", width: w, height: w, "aria-hidden": "true", fill: "none", stroke: "currentColor",
    "stroke-linecap": "round", "stroke-linejoin": "round" }, paths.map(([d, sw]) => s("path", { d, "stroke-width": sw || 1.4 })));
  const WAVE = "M1.8 13.1c1.55 0 1.55 1 3.1 1s1.55-1 3.1-1 1.55 1 3.1 1 1.55-1 3.1-1";
  const I = {
    get: (w = 17) => ico(w, ["M8 1.6v8.1M4.7 6.5 8 9.8l3.3-3.3", 1.5], [WAVE, 1.3]),
    send: (w = 17) => ico(w, ["M8 10.2V2M4.7 5.3 8 2l3.3 3.3", 1.5], [WAVE, 1.3]),
    star: (w = 15, on = false) => s("svg", { viewBox: "0 0 16 16", width: w, height: w, "aria-hidden": "true" },
      s("path", { d: "M8 2.1L9.25 6.75L14.3 8L9.25 9.25L8 13.9L6.75 9.25L1.7 8L6.75 6.75Z", fill: on ? "currentColor" : "none", stroke: "currentColor", "stroke-width": 1.3, "stroke-linejoin": "round" })),
    link: (w = 15) => ico(w, ["M6.6 9.4l2.8-2.8M7 4.6l1.3-1.3a2.6 2.6 0 0 1 3.7 3.7L10.7 8.3M9 11.4l-1.3 1.3A2.6 2.6 0 0 1 4 9l1.3-1.3"]),
    edit: (w = 15) => ico(w, ["M10.6 2.6l2.8 2.8-7.6 7.6H3v-2.8z"]),
    share: (w = 15) => ico(w, ["M1.5 11c2 0 2-1.6 4-1.6s2 1.6 4 1.6 2-1.6 4-1.6", 1.35], ["M10.1 4.6a2.1 2.1 0 1 1-4.2 0a2.1 2.1 0 1 1 4.2 0", 1.35]),
    dl: (w = 15) => ico(w, ["M8 2.5v7.5M4.8 6.8 8 10l3.2-3.2M3 13.5h10", 1.5]),
    q: (w = 16) => ico(w, ["M11.6 7a4.6 4.6 0 1 1-9.2 0a4.6 4.6 0 1 1 9.2 0M10.4 10.4L14 14"]),
    dn: (w = 10) => s("svg", { viewBox: "0 0 10 10", width: w, height: w, "aria-hidden": "true" }, s("path", { d: "M2 3.6L5 6.6L8 3.6", fill: "none", stroke: "currentColor", "stroke-width": 1.3, "stroke-linecap": "round", "stroke-linejoin": "round" })),
    up: (w = 15) => ico(w, ["M3.5 10L8 5.5L12.5 10", 1.5]),
    ext: (w = 12) => ico(w, ["M6 3H3.5v9.5H13V10M9 3h4v4M13 3L7.5 8.5"]),
    x: (w = 16) => ico(w, ["M3.5 3.5l9 9M12.5 3.5l-9 9", 1.5]),
  };
  const waveSvg = () => s("svg", { class: "ms-wave", viewBox: "0 0 100 20", preserveAspectRatio: "none", "aria-hidden": "true" }, s("polyline", { points: "0,10 100,10" }));

  /* a row: ✓ (o.tick), its slot's number (o.no), ▶, its shard, its name (all 16 characters) and its description's
   * first line (o.desc; or o.sub, a line in the instrument's words: FACTORY SCENE), what plays, its KEY · chord, GLINT
   * (ARP or SEQ, ice), BASS (SEQ, lamp: its sequence on), then the page's own cells (o.cells: SHARED, its author, a
   * count, the date) and o.side (a phone's, beside the name); o.mark: what its own column says (HIDDEN, no longer
   * shared), under its name where that column is left out.
   * Opened in place, the row becomes the card's head, its one action zone (o.actions: nodes), over o.intro() (what was
   * written about it, one quiet line of facts), what plays and GLINT at the left and every page at the right.
   * o: {id, key (▶'s), i, class, tick {checked, disabled, aria, onChange}, no, desc, sub, mark, cells, side, actions, intro(),
   *     onOpen(open), onPlay()} */
  function row(sc, o = {}) {
    const id = "c" + (++uid), L = sc.current ? look(sc.v) : null, k2 = o.key != null ? String(o.key) : id;
    const art = h("article", { class: "sr-item" + (o.class ? " " + o.class : ""), id: o.id || null, "aria-labelledby": `${id}-n`, style: `--i:${Math.min(o.i || 0, 24)}` });
    const play = h("button", { type: "button", class: "play", disabled: !sc.bytes, "aria-label": `Play ${sc.name}`, "aria-pressed": "false",
                               title: sc.bytes ? null : "Restore it to the FM-1 to hear it", onclick: () => toggleSound(k2) }, playIcon());
    const tick = o.tick && h("label", { class: "ck" }, h("input", { type: "checkbox", checked: !!o.tick.checked, disabled: !!o.tick.disabled,
      "aria-label": o.tick.aria || `Select ${sc.name}`, onchange: (e) => { if (o.tick.onChange) o.tick.onChange(e.target.checked); } }));
    const kv = L ? h("span", { class: "kv cx" }, h("span", { class: "sr-only" }, "Key "), `${L.txt(ID.P_ROOT)} ${L.txt(ID.P_SCALE)} `, h("i", null, `· ${L.txt(ID.P_CHORD)}`))
                 : h("span", { class: "kv cx" }, h("span", { class: "badge is-old" }, "EARLIER FRASIL"));
    const gt = L && L.type ? h("span", { class: "gt cx" }, h("span", { class: "sr-only" }, "GLINT "), L.txt(ID.P_SQ_TYPE))
                           : h("span", { class: "gt off cx", "aria-hidden": "true" }, "—");
    const bq = sc.bseq && sc.bseq.on ? h("span", { class: "bq cx", title: "The bass plays its sequence" }, h("span", { class: "sr-only" }, "BASS "), "SEQ")
                                     : h("span", { class: "bq-no cx" });
    const ms = mini(sc, L);
    ms.classList.add("cx");
    const open = h("button", { type: "button", class: "sr-open", "aria-expanded": "false", "aria-controls": `${id}-more`, "aria-label": `Open ${sc.name}`,
                               onclick: () => openRow(art) }, I.up(15));
    art.append(h("div", { class: "sr" }, tick, o.no != null ? h("span", { class: "no" }, h("span", { class: "sr-only" }, "Slot "), o.no) : null, play,
      h("span", { class: "nm" }, shard(sc, 26, true), h("span", { class: "nm-t" },
        h("h3", { class: "nm-n", id: `${id}-n`, tabindex: "-1" }, sc.name), o.desc ? h("span", { class: "nm-d" }, o.desc) : null,
        o.sub ? h("span", { class: "nm-s" }, o.sub) : null, o.mark ? h("span", { class: "nm-mk" }, o.mark) : null)),
      ms, kv, gt, bq, (o.cells || []).map((c) => { if (c && c.classList) c.classList.add("cx"); return c; }),
      o.side ? h("span", { class: "side cx" }, o.side) : null,
      h("span", { class: "acts" }, o.actions || null), open),
      h("p", { class: "sh-say", role: "status" }),
      h("div", { class: "oc-more", id: `${id}-more`, inert: true }, h("div", { class: "oc-in" })));
    art._build = () => {
      const intro = o.intro ? h("div", { class: "oc-intro" }, o.intro()) : null;
      if (!L) return [intro, h("div", { class: "oc-body is-old" }, h("div", { class: "oc-left" }, plays(sc, null, id)),
        h("p", { class: "sh-note" }, "Made by an earlier FRASIL: its sound is brought up to date when the backup is restored. To make it a shard on its own, restore the whole backup, then back up again."))];
      const left = h("div", { class: "oc-left" },
        h("section", { class: "sh-plays", "aria-labelledby": `${id}-wp` },
          h("div", { class: "sh-sub" }, h("h4", { id: `${id}-wp` }, "WHAT PLAYS"), h("span", { class: "sh-range", "aria-hidden": "true" }, "C1 – C7")),
          h("div", { class: "scr" }, h("div", { class: "kb-in" }, keyboard(sc, L, id), waveSvg())), legend(sc, L)),
        glint(sc, L, id));
      const right = h("div", { class: "oc-right" }, h("div", { class: "sx-h", "aria-hidden": "true" }, h("span", null, "EVERY PAGE"), h("span", null, "ON THE FM-1")), sections(L, sc.bseq));
      return [intro, h("div", { class: "oc-body" }, left, right)];
    };
    art._open = o.onOpen || null;
    art._key = k2;
    REG.set(k2, { art, sc, o });
    return art;
  }
  /* a row opened (or closed) in place: its head the card's, the rows below glide down */
  function openRow(art, open = !art.classList.contains("open"), quiet = false) {
    if (open === art.classList.contains("open")) return;
    const inner = art.querySelector(".oc-in"), btn = art.querySelector(".sr-open"), name = art.querySelector(".nm-n").textContent;
    if (open && art._build && !inner.firstChild) inner.append(...art._build().filter(Boolean));
    const after = glide(art.parentElement, () => {
      art.classList.toggle("open", open);
      btn.setAttribute("aria-expanded", String(open));
      btn.setAttribute("aria-label", (open ? "Close " : "Open ") + name);
      const more = art.querySelector(".oc-more");
      if (open) more.removeAttribute("inert"); else more.setAttribute("inert", "");
    }, art);
    if (open && !quiet && after && (after.top < 0 || after.top > innerHeight * 0.72))
      scrollTo({ top: scrollY + after.top - 24, behavior: reduce.matches ? "auto" : "smooth" });
    if (art._open) art._open(open);
  }

  // ================================================================ ▶: a shard, played by FRASIL's engine (river-player.js)
  const MISSING = "Sound needs a moment: FRASIL’s engine isn’t on this site yet.";
  let soundKey = null, rippleRaf = 0;
  const soundFns = [];
  function needPlayer() {
    if (window.RiverPlayer) return Promise.resolve(wire(window.RiverPlayer));
    if (!needPlayer.p) needPlayer.p = new Promise((res, rej) => {     // (a page without it: fetched once)
      const tag = document.createElement("script");
      tag.src = "/assets/js/river-player.js";
      tag.onload = () => (window.RiverPlayer ? res(wire(window.RiverPlayer)) : rej(new Error("missing")));
      tag.onerror = () => { needPlayer.p = null; tag.remove(); rej(new Error("missing")); };
      document.head.append(tag);
    });
    return needPlayer.p;
  }
  function wire(pl) {
    if (pl._river) return pl;
    pl._river = true;
    pl.base = "/emu/";                 // (the engine at the River's root, whichever page plays)
    pl.addEventListener("loading", (e) => sound(e.detail.id, "loading"));
    pl.addEventListener("playing", (e) => {
      sound(e.detail.id, "playing");
      ripple();
      const c = REG.get(e.detail.id);
      if (c && c.o.onPlay) c.o.onPlay();
    });
    pl.addEventListener("stopped", (e) => sound(e.detail.id, ""));
    pl.addEventListener("error", (e) => {
      sound(e.detail.id, "");
      const c = REG.get(e.detail.id);
      say(c && c.art, e.detail.code === "missing" ? MISSING : e.detail.message);
    });
    return pl;
  }
  if (window.RiverPlayer) wire(window.RiverPlayer);
  async function toggleSound(k2) {
    const c = REG.get(k2);
    if (!c || !c.sc.bytes) return;
    const pl = window.RiverPlayer;
    if (pl && pl.id === k2 && pl.state !== "idle") { pl.stop(); return; }
    say(c.art, "");
    try { (await needPlayer()).play(c.sc.bytes, k2); }
    catch (_) { say(c.art, MISSING); }
  }
  function stopSound() { if (window.RiverPlayer) window.RiverPlayer.stop(); }
  /* a card's sound state: ▶ / ■, its name for a screen reader; the page told (its waterline) */
  function sound(k2, state) {
    if (state) soundKey = k2; else if (soundKey === k2) soundKey = null;
    const c = REG.get(k2);
    if (!c) return;
    c.art.dataset.sound = state;
    const b = c.art.querySelector(".play");
    b.setAttribute("aria-label", (state === "playing" ? "Stop " : state === "loading" ? "Starting the sound of " : "Play ") + c.sc.name);
    b.setAttribute("aria-pressed", String(state === "playing"));
    if (!state) c.art.querySelectorAll(".ms-wave polyline").forEach((w) => w.setAttribute("points", "0,10 100,10"));
    soundFns.forEach((f) => f(k2, state, c.sc));
  }
  /* while it sounds, its line ripples: two slow swells along it, as tall as the sound is loud (in a row, its line of
   * what plays; opened, the keyboard's waterline too) */
  function ripple() {
    cancelAnimationFrame(rippleRaf);
    if (reduce.matches) return;
    let amp = 0;
    const frame = (t) => {
      const pl = window.RiverPlayer, c = REG.get(soundKey);
      if (!pl || pl.state !== "playing" || !c) return;
      const lv = Math.min(1, pl.level() * 9);
      amp += (lv - amp) * 0.08;
      const pts = [];
      for (let i = 0; i <= 48; i++) {
        const x = i / 48, edge = Math.sin(Math.PI * x);
        const y = 10 - amp * 8 * edge * (0.62 * Math.sin(x * 9.4 - t / 900) + 0.38 * Math.sin(x * 23 + t / 640));
        pts.push(`${r2(x * 100)},${r2(y)}`);
      }
      const p = pts.join(" ");
      c.art.querySelectorAll(".ms-wave polyline").forEach((w) => w.setAttribute("points", p));
      rippleRaf = requestAnimationFrame(frame);
    };
    rippleRaf = requestAnimationFrame(frame);
  }
  addEventListener("keydown", (e) => { if (e.key === "Escape" && soundKey != null && !document.querySelector("dialog[open]")) stopSound(); });

  // ================================================================ a word for a moment; a question before something lasting
  let toastEl = null, toastT = 0;
  function toast(...parts) {
    if (!toastEl) { toastEl = h("p", { class: "toast", role: "status" }); document.body.append(toastEl); }
    fill(toastEl, ...parts);
    toastEl.classList.add("in");
    clearTimeout(toastT);
    toastT = setTimeout(() => toastEl.classList.remove("in"), 5200);
  }
  /* {title, text (parts), ok (its button's words), danger, check (a box to tick first)} -> true if confirmed */
  function confirm({ title, text, ok = "OK", danger = false, check = null }) {
    return new Promise((res) => {
      const box = check && h("input", { type: "checkbox" });
      const go = h("button", { type: "button", class: "btn " + (danger ? "btn-danger" : "btn-ice"), disabled: !!check }, ok);
      const no = h("button", { type: "button", class: "btn" }, "Cancel");
      const dlg = h("dialog", { class: "sheet sheet-sm", "aria-labelledby": "cf-h" },
        h("div", { class: "sheet-in" },
          h("h2", { class: "sheet-h", id: "cf-h" }, title),
          h("p", { class: "sheet-lede" }, text),
          check && h("label", { class: "cf-check" }, box, h("span", null, check)),
          h("footer", { class: "sheet-foot" }, no, go)));
      if (box) box.addEventListener("change", () => { go.disabled = !box.checked; });
      let answer = false;
      go.addEventListener("click", () => { answer = true; dlg.close(); });
      no.addEventListener("click", () => dlg.close());
      dlg.addEventListener("close", () => { dlg.remove(); res(answer); });
      document.body.append(dlg);
      dlg.showModal();
      no.focus();
    });
  }

  return {
    h, s, fill, two, nn, word, listWords, key, k, look, shard, WORD, reduce, I, line,
    settingsRow, ticked, say, glide, row, openRow,
    stopSound, onSound: (f) => soundFns.push(f), get soundKey() { return soundKey; },
    toast, confirm,
  };
})();
