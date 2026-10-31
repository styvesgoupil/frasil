/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's row (the Community River, Your Private River, Quick Backup's slots): a shard drawn from its bytes with
 * the schema the firmware's code writes (FB.S), closed (▶, its name, a line of what plays, its key, GLINT's type, the
 * bass's sequence and the page's own cells) and opened in place: at the left what moves (GLINT's walk or sequence, the
 * bass's sequence, the LFOs), at the right every page as the screen writes it, at its foot THE KEYS (river-keys.js);
 * ▶ with FRASIL's engine (river-player.js), one shard at a time, its card following what sounds (follow(): the keys lit
 * by the voices, the LFOs moving, the sequences' playheads, the MIXER's meters).
 * What users typed (names, descriptions, authors) is only ever text: no HTML is built from strings here.
 *   RiverCards.row(scene, o)       a shard as a row on the river, opened in place into its card, its head the one
 *                                  action zone (see row())
 *   RiverCards.settingsRow(settings, scenes) · openRow(art, open) · ticked(art, on) · say(art, ...) (a line on a row)
 *   RiverCards.line(scene) (what plays, in one line) · I (the River's line icons)
 *   RiverCards.toast(...parts) · confirm({title, text, ok, danger, check}) -> Promise<boolean>
 *   RiverCards.stopSound() · onSound(fn(key, state, scene)) · h, s, two, nn, word, key, k, fill, look, shard
 *   RiverCards.relive(art) (its live parts found again, what is heard put on them) · parts (the drawing's own, for
 *   river-keys.js) · redraw(art, sc) (a card drawn again: only what differs changes) · patch (how: patch, kids,
 *   place, stamp, sig) */
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
      else if (a.startsWith("on") && typeof v === "function") { e.addEventListener(a.slice(2), v); e._on = true; }   // (_on: patch() never
                                                                                                                        // keeps one changed)
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

  // ---------------------------------------------------------------- a drawing laid over the one shown (patch)
  /* The editor draws a card again after each change: the new drawing is laid over the one shown, so only what differs
   * changes (a value, a gauge, a section's line), and nothing else is replaced (no section closes, nothing fades in
   * again, a focus and a drag stay). Kept, an element takes the new one's attributes and children (its class's
   * transient words kept: is-active, is-open; a details' open; the waterline .ms-wave, as follow() ripples it);
   * children matched by kind and key (data-p, data-k, data-key), in order.
   * Some elements hold more than what they show (listeners: _on; a live part's _live, a section's _build ...): one of
   * those is kept only if drawn the same (_sig: its markup at rest, stamped before it is shown, ids aside, and _sigx,
   * what it holds besides), else replaced whole; one marked _shell (its own state found from itself) is patched like
   * any other, its own props the new one's; one with _remorph(fresh) does its own (THE KEYS) */
  const TRANSIENT = ["is-active", "is-open"];
  const norm = (t) => t.replace(/((?:id|labelledby|controls)="|url\(#)[a-z]+\d+/g, "$1#");   // (each drawing's own ids)
  const own = (n) => Object.keys(n).filter((x) => x[0] === "_" && x !== "_sig");
  const held = (n) => n.nodeType === 1 && !n._shell && own(n).length > 0;
  const sigOf = (n) => norm(n.outerHTML) + (n._sigx != null ? "\u0000" + n._sigx : "");
  /* a fresh drawing's held elements given their _sig (before it is shown: at rest, not marked by the editor) */
  function stamp(root) {
    const walk = (n) => {
      if (n.nodeType !== 1 || n._remorph) return;
      if (held(n)) { if (n._sig == null) n._sig = sigOf(n); return; }
      for (const c of n.children) walk(c);
    };
    walk(root);
    return root;
  }
  const keyOf = (n) => (n.nodeType !== 1 ? "#" + n.nodeType
    : n.nodeName + "|" + (n.getAttribute("data-p") ?? n.getAttribute("data-k") ?? n.getAttribute("data-key") ?? ""));
  function attrs(o, f) {
    for (const a of [...o.attributes]) if (!f.hasAttribute(a.name) && !(a.name === "open" && o.nodeName === "DETAILS")) o.removeAttribute(a.name);
    for (const a of f.attributes) {
      if (a.name === "class") {
        const want = new Set(f.classList);
        TRANSIENT.forEach((c) => { if (o.classList.contains(c)) want.add(c); });
        if (want.size !== o.classList.length || [...o.classList].some((c) => !want.has(c))) o.setAttribute("class", [...want].join(" "));
      } else if (o.getAttribute(a.name) !== a.value) o.setAttribute(a.name, a.value);
    }
  }
  /* node o (shown) made as f (fresh) -> the node there now (o, or f in its place) */
  function patch(o, f) {
    if (o.nodeType === 3 && f.nodeType === 3) { if (o.data !== f.data) o.data = f.data; return o; }
    if (o.nodeType !== 1 || f.nodeType !== 1 || o.nodeName !== f.nodeName) { o.replaceWith(f); return f; }
    if (o._remorph) return o._remorph(f);
    if (o.classList.contains("ms-wave")) return o;
    if (held(o) || held(f)) {
      if (o._sig != null && o._sig === (f._sig ?? sigOf(f))) return o;
      o.replaceWith(f);
      return f;
    }
    attrs(o, f);
    patchKids(o, f);
    if (o._shell || f._shell) {
      for (const x of own(o)) if (!(x in f)) delete o[x];
      for (const x of own(f)) o[x] = f[x];
    }
    return o;
  }
  /* o's children made as f's */
  function patchKids(o, f) {
    const pool = new Map();
    for (const c of o.childNodes) { const x = keyOf(c); if (!pool.has(x)) pool.set(x, []); pool.get(x).push(c); }
    place(o, [...f.childNodes].map((c) => { const q = pool.get(keyOf(c)), m = q && q.shift(); return m ? patch(m, c) : c; }));
  }
  /* parent's children: nodes, in order (the others gone; one already in its place not moved) */
  function place(parent, nodes) {
    const want = new Set(nodes);
    for (const c of [...parent.childNodes]) if (!want.has(c)) c.remove();
    let cur = parent.firstChild;
    for (const n of nodes) { if (n === cur) cur = cur.nextSibling; else parent.insertBefore(n, cur); }
  }

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
   * its name in its track's colour. One an LFO moves, while it plays: its value now, a tick along its gauge in the
   * colour of the LFO's TARGET's track */
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
    const el = h("div", { class: "cell", "data-p": p },
      h("div", { class: "cell-n" }, d.name, tag && h("span", { class: "lfo-tag" }, tag)),
      h("div", { class: "cell-v" }, h("span", { class: "v", style: col ? `color:${col}` : null }, val), unit && h("span", { class: "u" }, unit)),
      h("div", { class: "cell-m" }, meter));
    if (!tag || !meter || !meter.classList.contains("gauge")) return el;
    const tg = S.lfo.targets.find((t) => t && t.p === p);
    meter.append(h("b", { style: `background:${TRACK[tg && tg.track] || C.ice}` }));
    el._x = x;                                       // (its value and its tick found from the cell itself: patched,
    el._shell = true;                                // the cell shown takes the new one's)
    return live(el, function (f, ctx) {
      const now = this.querySelector(".gauge > b");
      if (!now) return;
      if (!f || ctx.mod[p] === undefined) { now.style.opacity = 0; return; }
      const v = Math.max(0, Math.min(d.max, this._x + ctx.mod[p]));
      now.style.left = `calc(${r2(v / Math.max(1, d.max) * 100)}% - .75px)`;
      now.style.opacity = 1;
    });
  }

  // ---------------------------------------------------------------- what sounds: a ▶ playing (river-player.js live())
  /* app/web.c web_live's floats, read: the voices sounding, each its note and its level (the envelope; GLINT's as
   * heard), the LFOs (on, where in its turn, SHAPE's value, the turns gone by), GLINT's sequence's step and the note it
   * or the arpeggio struck last (and how many they have struck), the bass's step and its time in it (MIDI clocks), the
   * meters (drones, bass, glints, the cloud), the tide; struck: GLINT's voices, the last struck first ({n, t: seconds
   * since}) */
  function heardOf(v) {
    const f = { drone: [], glint: [], bass: null, lfo: [], seq: v[38], note: v[39], bq: v[40], bqT: v[41], m: [v[42], v[43], v[44], v[45]], tide: v[46], strikes: v[47] };
    for (let i = 0; i < 8; i++) if (v[i] >= 0 && v[8 + i] > 0.003) f.drone.push({ n: Math.round(v[i]), l: v[8 + i] });
    for (let i = 0; i < 4; i++) if (v[16 + i] >= 0 && v[20 + i] > 0.003) f.glint.push({ n: Math.round(v[16 + i]), l: v[20 + i] });
    if (v[24] >= 0 && v[25] > 0.003) f.bass = { n: Math.round(v[24]), l: v[25] };
    for (let i = 0; i < 3; i++) f.lfo.push({ on: v[26 + i] > 0, ph: v[29 + i], out: v[32 + i], cyc: Math.round(v[35 + i]) });
    f.struck = [0, 1, 2, 3].filter((i) => v[16 + i] >= 0).map((i) => ({ n: Math.round(v[16 + i]), t: v.length > 48 ? v[48 + i] : 0 })).sort((a, b) => a.t - b.t);
    return f;
  }
  /* what the LFOs add to the settings now, as app.c's lfo_run does: DEPTH's share of SHAPE's value (as POLARITY has
   * it) times each TARGET's whole range -> {mod: {p: amount}, offs: each LFO's share} */
  function moved(L, f) {
    const mod = {}, offs = [0, 0, 0];
    if (!f || !L) return { mod, offs };
    S.lfo.base.forEach((b, i) => {
      const q = f.lfo[i];
      if (!q || !q.on) return;
      const pol = L.raw(b + 3) % 3, w = q.out;
      const off = offs[i] = L.raw(b + 2) / 127 * (pol === 1 ? 0.5 * (w + 1) : pol === 2 ? -0.5 * (w + 1) : w);
      for (let t = 4; t < 8; t++) {
        const tg = S.lfo.targets[L.raw(b + t)];
        if (L.raw(b + t) && tg && tg.p >= 0) mod[tg.p] = (mod[tg.p] || 0) + off * P[tg.p].max;
      }
    });
    return { mod, offs };
  }
  /* a part of a card that follows the sound: its element marked (.lv) and given fn(f, ctx) (f: heardOf's, or null at
   * rest; ctx: {t (seconds), mod, offs}) */
  const live = (el, fn) => { el.classList.add("lv"); el._live = fn; return el; };
  const at = (el, a) => { for (const [k, v] of Object.entries(a)) el.setAttribute(k, typeof v === "number" ? r2(v) : v); };
  const dB = (m) => Math.max(0, Math.min(1, 1 + 20 * Math.log10(Math.max(m, 1e-6)) / 48));   // a meter, 48 dB to the top

  // ---------------------------------------------------------------- WHAT PLAYS: C1 to the C over its highest note
  /* the keyboard's top: the C at or over the highest note the scene plays (its drones, GLINT's held notes up their
   * RANGE at its OCTAVE, its sequence's notes as it plays them: TRANSPOSE along the scale, then its OCTAVE), C7 at
   * least, C9 at most */
  function kbTop(sc, L) {
    let hi = 0;
    sc.chords.forEach((ch) => ch.notes.forEach((n) => { hi = Math.max(hi, n); }));
    if (L && L.arpOn) sc.arp.forEach((a) => { hi = Math.max(hi, a + 12 * (L.oct + L.range - 1)); });
    if (L && L.seqOn) {
      const d = L.raw(ID.P_SQ_TRANS) - P[ID.P_SQ_TRANS].def;
      sc.seq.forEach((b) => { const n = b & 127; if (n !== S.seq.rest && n !== S.seq.tie) hi = Math.max(hi, Math.min(127, scaleShift(L, n, d)) + 12 * L.oct); });
    }
    return Math.min(120, Math.max(96, Math.ceil(hi / 12) * 12));
  }
  /* the drones' lights stand on it (a chord's root white, its notes glowing), GLINT's held notes ice (their RANGE's
   * octaves ringed), the bass lamp light in the bed. While it plays, what is latched dims and what sounds lights over
   * it, each voice as bright as its envelope: a drone fog on its key, a column of light over it (breathing with the
   * drones' meter), a glint ice in the sky above its key (rising a little as it fades, as on the FM-1's play page),
   * the bass lamp in the bed under its key. svg._top: its top note */
  function keyboard(sc, L, id) {
    const kw = 12, bw = 7.4, wy = 33, kh = 47, bh = 29, WH = [0, 2, 4, 5, 7, 9, 11], top = kbTop(sc, L);
    let whites = 0;
    for (let n = 24; n <= top; n++) if (WH.includes(n % 12)) whites++;
    const W = whites * kw;
    const st = litNotes(sc, L, top), arp = L && L.arpOn ? sc.arp : [], bq = bqNotes(sc, L);
    const g = (x) => `url(#${id}-${x})`;
    const lin = (x, col, op) => s("linearGradient", { id: `${id}-${x}`, x1: 0, y1: 1, x2: 0, y2: 0 },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const rad = (x, col, op) => s("radialGradient", { id: `${id}-${x}` },
      s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const svg = s("svg", { class: "kb", viewBox: `0 0 ${W} 106`, role: "img" },
      s("defs", null, lin("cw", C.white, 0.85), lin("cf", C.fog, 0.55), lin("ci", C.ice, 0.9),
        rad("hw", C.white, 0.42), rad("hf", C.fog, 0.24), rad("hi", C.ice, 0.48), rad("hl", C.lamp, 0.55), rad("hI", C.ice, 0.7),
        s("linearGradient", { id: `${id}-wl`, x1: 0, y1: 0, x2: 1, y2: 0 },
          [[0, C.slate, 0], [0.18, C.slate, 0.75], [0.5, C.fog, 0.5], [0.82, C.slate, 0.75], [1, C.slate, 0]]
            .map(([o, c, a]) => s("stop", { offset: o, "stop-color": c, "stop-opacity": a })))),
      s("rect", { x: 0, y: 32.5, width: W, height: 1, fill: g("wl") }));
    const LIGHT = { root: ["w", 24, 11, 1], note: ["f", 14, 8, 1], arp: ["i", 20, 10, 1], arpr: ["i", 8, 6, 0.5] };
    const halos = [], cols = [], wk = [], bk = [], wl = [], bl = [], bed = [], octs = [], keys = new Map();
    let wi = 0;
    for (let n = 24; n <= top; n++) {
      const white = WH.includes(n % 12), lit = st[n];
      const cx = white ? wi * kw + 6 : wi * kw, base = white ? C.keyW : C.keyB;
      const r = white ? { x: wi * kw + 0.5, y: wy + 1, width: kw - 1, height: kh, rx: 1.5 }
                      : { x: r2(cx - bw / 2), y: wy + 1, width: bw, height: bh, rx: 1 };
      (white ? wk : bk).push(s("rect", { ...r, fill: base, stroke: white ? "none" : C.keyEdge, "stroke-width": white ? 0 : 0.8 }));
      const latched = lit === "root" ? C.white : lit === "note" ? mix(base, C.fog, 0.5) : lit === "arp" ? C.ice : lit === "arpr" ? mix(base, C.ice, 0.14) : null;
      if (latched) (white ? wk : bk).push(s("rect", { ...r, class: "kb-st", fill: latched, stroke: lit === "arpr" ? C.ice : "none", "stroke-width": 0.9 }));
      const over = s("rect", { ...r, opacity: 0 });
      (white ? wl : bl).push(over);
      keys.set(n, { x: cx, white, over });
      if (lit) {
        const lg = LIGHT[lit];
        halos.push(s("circle", { class: "kb-st", cx: r2(cx), cy: wy, r: lg[2], fill: g("h" + lg[0]), opacity: lg[3] }));
        cols.push(s("rect", { class: "kb-st", x: r2(cx - 0.75), y: wy - lg[1], width: 1.5, height: lg[1], fill: g("c" + lg[0]), opacity: lg[3] }));
      }
      if (n === sc.bass) {
        bed.push(s("ellipse", { class: "kb-st", cx: r2(cx), cy: wy + kh + 5, rx: 18, ry: 6, fill: g("hl") }),
                 s("rect", { class: "kb-st", x: r2(cx - 5), y: wy + kh + 3.5, width: 10, height: 2, rx: 1, fill: C.lamp }));
      } else if (bq.includes(n)) bed.push(s("rect", { class: "kb-st", x: r2(cx - 3), y: wy + kh + 3.5, width: 6, height: 2, rx: 1, fill: C.lamp, opacity: 0.75 }));
      if (n % 12 === 0) octs.push(s("text", { x: r2(Math.max(6, Math.min(W - 6, cx))), y: 101, "text-anchor": "middle", fill: C.dim, class: "svgt" }, nn(n)));
      if (white) wi++;
    }
    /* what sounds: a light a voice, the drones' eight, GLINT's four, the bass */
    const dr = Array.from({ length: 8 }, () => ({ halo: s("circle", { r: 11, fill: g("hw"), opacity: 0 }), col: s("rect", { width: 1.5, fill: g("cw"), opacity: 0 }) }));
    const gl = Array.from({ length: 4 }, () => ({ halo: s("circle", { fill: g("hI"), opacity: 0 }), star: s("path", { fill: C.ice, opacity: 0 }), core: s("circle", { r: 1.2, fill: C.white, opacity: 0 }) }));
    const bs = { glow: s("ellipse", { rx: 20, ry: 7, fill: g("hl"), opacity: 0 }), bar: s("rect", { width: 12, height: 2.4, rx: 1.2, fill: C.lamp, opacity: 0 }) };
    svg.append(...halos, ...cols, ...dr.map((d) => [d.halo, d.col]).flat(), ...wk, ...wl, ...bk, ...bl, ...bed, bs.glow, bs.bar,
               ...gl.map((x) => [x.halo, x.star, x.core]).flat(), ...octs);
    const notesOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn);
    const drs = sc.chords.map((ch) => nn(ch.root) + (notesOf(ch).length ? " (" + notesOf(ch).join(" ") + ")" : ""));
    let a = `Keyboard, C1 to ${nn(top)}.`;
    if (drs.length) a += " Drones latched: " + drs.join(", ") + ".";
    if (arp.length) a += " GLINT's arpeggio: " + arp.map((x) => nn(x + 12 * L.oct)).join(" ") + ", over RANGE " + L.txt(ID.P_ARP_RANGE) + ".";
    if (sc.bass >= 0) a += " Bass: " + nn(sc.bass) + ".";
    if (bq.length) a += " The bass's sequence: " + bq.map(nn).join(" ") + ".";
    if (!drs.length && !arp.length && sc.bass < 0 && !bq.length) a += " Sound only: nothing latched.";
    svg.setAttribute("aria-label", a);
    svg._top = top;
    return live(svg, (f, ctx) => {
      svg.classList.toggle("is-live", !!f);
      for (const k of keys.values()) k.over.setAttribute("opacity", 0);
      const lightKey = (k, col, op) => {
        if (op <= +k.over.getAttribute("opacity")) return;
        at(k.over, { fill: col, opacity: op });
      };
      const K = ctx.keep;                                           // (the drones' breath: their meter against its peak,
      K.peak = f ? Math.max((K.peak || 0.05) * 0.995, f.m[0]) : 0.05;   // the card's, through a redraw)
      const breath = f ? Math.max(0.55, Math.min(1, 0.55 + 0.45 * f.m[0] / K.peak)) : 0;
      dr.forEach((d, i) => {
        const v = f && f.drone[i], k = v && keys.get(v.n);
        if (!k) { d.halo.setAttribute("opacity", 0); d.col.setAttribute("opacity", 0); return; }
        const op = v.l * breath, ht = 6 + 18 * op;
        at(d.halo, { cx: k.x, cy: wy, opacity: op });
        at(d.col, { x: k.x - 0.75, y: wy - ht, height: ht, opacity: op });
        lightKey(k, k.white ? C.fog : mix(C.keyB, C.fog, 0.62), (k.white ? 0.78 : 0.9) * op);
      });
      gl.forEach((x, i) => {
        const v = f && f.glint[i], k = v && keys.get(v.n);
        if (!k) { x.halo.setAttribute("opacity", 0); x.star.setAttribute("opacity", 0); x.core.setAttribute("opacity", 0); return; }
        const y = wy - 9 - (1 - v.l) * 14;
        at(x.halo, { cx: k.x, cy: y, r: 4 + 8 * v.l, opacity: v.l });
        at(x.star, { d: star(k.x, y, 2.2 + 6 * v.l), opacity: 0.35 + 0.65 * v.l });
        at(x.core, { cx: k.x, cy: y, opacity: v.l });
        lightKey(k, C.ice, 0.25 + 0.75 * v.l);
      });
      const b = f && f.bass, k = b && keys.get(b.n);
      if (!k) { bs.glow.setAttribute("opacity", 0); bs.bar.setAttribute("opacity", 0); return; }
      at(bs.glow, { cx: k.x, cy: wy + kh + 5, opacity: b.l });
      at(bs.bar, { x: k.x - 6, y: wy + kh + 3.3, opacity: 0.4 + 0.6 * b.l });
      lightKey(k, mix(k.white ? C.keyW : C.keyB, C.lamp, 0.55), 0.85 * b.l);
    });
  }

  function legend(sc, L, top = 96) {
    const items = [];
    const notesOf = (ch) => ch.notes.filter((x) => x !== ch.root).map(nn).join(" ");
    if (sc.chords.length) items.push([sc.chords.length > 1 ? "DRONES" : "DRONE", "w", sc.chords.map((ch) => [nn(ch.root), notesOf(ch)])]);
    if (L && L.arpOn && sc.arp.length) {
      items.push(["ARP", "i", [[sc.arp.map((x) => nn(x + 12 * L.oct)).join(" "), ""]]]);
      if (L.range > 1) {
        const up = new Set(), held = new Set(sc.arp.map((x) => x + 12 * L.oct));
        for (let r = 1; r < L.range; r++) sc.arp.forEach((x) => { const n = x + 12 * (L.oct + r); if (n <= top && !held.has(n)) up.add(n); });
        items.push(["RANGE", "r", [["", [...up].sort((p, q) => p - q).map(nn).join(" ")]]]);
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
   * a dot on the line. While it plays, its playhead: a line of light down through the step playing, the note it
   * sounds white */
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
    const head = s("rect", { x: -10, y: top - 8, width: 1.2, height: bot - top + 18, fill: C.ice, opacity: 0 });
    svg.append(s("line", { x1: 26, y1: top, x2: 512, y2: top, stroke: C.steel, "stroke-dasharray": "1 3" }),
               s("line", { x1: 26, y1: bot, x2: 512, y2: bot, stroke: C.steel, "stroke-dasharray": "1 3" }),
               s("rect", { x: 26, y: 84, width: 486, height: 1, fill: C.steel }), head);
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
    const glow = s("circle", { r: 15, fill: `url(#${id}-sg)`, opacity: 0 }), core = s("circle", { r: 2.4, fill: C.white, opacity: 0 });
    svg.append(...ties, ...rests, ...glows, ...stars, ...cores, glow, core);
    const sh = 12 * L.oct;
    svg.append(label(4, r2(top + 3.5), "start", nn(hi + sh), C.dim), label(4, r2(bot + 3.5), "start", nn(lo + sh), C.dim),
               label(r2(X(0)), 106, "middle", "1", C.dim), label(r2(X(n32 - 1)), 106, "middle", String(n32), C.dim));
    if (len < n32) svg.append(label(r2(X(len - 1) + 4), 106, "end", "LENGTH " + L.txt(ID.P_SQ_LEN), C.fog));
    const say = sc.seq.slice(0, len).map((b) => {
      const n = b & 127;
      return n === REST ? "REST" : n === TIE ? "TIE" : nn(n + sh) + (b & ACC ? " with ACCENT" : "");
    });
    svg.setAttribute("aria-label", `GLINT's sequence: ${len} of its ${n32} steps play, ${say.join(", ")}.`);
    return live(svg, (f) => {
      const pos = f ? f.seq : -1;
      if (pos < 0 || pos >= n32) { [head, glow, core].forEach((e) => e.setAttribute("opacity", 0)); return; }
      const x = X(pos), n = sc.seq[pos] & 127;
      at(head, { x: x - 0.6, opacity: 0.5 });
      const v = n !== REST && n !== TIE && f.glint.find((g) => g.n === f.note + sh);
      if (!v) { glow.setAttribute("opacity", 0); core.setAttribute("opacity", 0); return; }
      const y = bot - (n - lo) * (bot - top) / (hi - lo);
      at(glow, { cx: x, cy: y, opacity: 0.5 + 0.5 * v.l });
      at(core, { cx: x, cy: y, opacity: 0.5 + 0.5 * v.l });
    });
  }

  /* GLINT on ARP: its MODE drawn as the walk it takes over the held notes and their RANGE (UP, DOWN, UPDOWN as app.c
   * steps them; RANDOM and DRIFT, a repeatable draw of their kind: DRIFT a step either way, or stay). While it plays,
   * the walk it really takes instead, each step it strikes flowing left from the right edge at RATE's pace (at the
   * scene's TEMPO), a step CHANCE leaves out a gap; with reduced motion, the walk its MODE takes, still */
  function arpStrip(sc, L, id) {
    const x0 = 40, x1 = 504, top = 12, bot = 62, mode = L.raw(ID.P_ARP_MODE), notes = [];
    for (let r = 0; r < L.range; r++) sc.arp.forEach((a) => notes.push(a + 12 * (L.oct + r)));
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 74", role: "img" }, glowDef(id));
    if (!notes.length) {               /* (no notes kept for it: GLINT is silent, its strip dimmed; the drones are
                                        * the keyboard's, not this) */
      svg.classList.add("is-empty");
      svg.append(s("line", { x1: 26, y1: 37, x2: 512, y2: 37, stroke: C.steel, "stroke-dasharray": "1 3" }),
                 label(258, 40.5, "middle", "NO ARP NOTES", C.dim));
      svg.setAttribute("aria-label", `GLINT's arpeggio, MODE ${L.txt(ID.P_ARP_MODE)}: no notes to play, GLINT is silent.`);
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
    const trail = s("polyline", { fill: "none", stroke: C.ice, "stroke-opacity": 0.42, "stroke-width": 1.1, "stroke-linejoin": "round", "stroke-linecap": "round" });
    const walked = s("g");
    svg.append(s("g", { class: "st-walk" }, s("polyline", { points: pts.join(" "), fill: "none", stroke: C.ice, "stroke-opacity": 0.5, "stroke-width": 1.1, "stroke-linejoin": "round", "stroke-linecap": "round" }),
                 ...glows, ...stars, ...cores),
               s("g", { class: "lv-walk" }, trail, walked),
               label(4, r2(top + 3.5), "start", nn(hi), C.dim), label(4, r2(bot + 3.5), "start", nn(lo), C.dim));
    svg.setAttribute("aria-label", `GLINT's arpeggio, MODE ${L.txt(ID.P_ARP_MODE)}: its walk over ${sc.arp.map((x) => nn(x + 12 * L.oct)).join(" ")} and RANGE ${L.txt(ID.P_ARP_RANGE)}, ${nn(lo)} to ${nn(hi)}.`);
    const slot = (x1 - x0) / (steps - 1), N = 40;
    const lights = Array.from({ length: N }, () => {
      const l = { glow: s("circle", { fill: `url(#${id}-sg)`, opacity: 0 }), arm: s("path", { fill: C.ice, opacity: 0 }), core: s("circle", { opacity: 0 }) };
      walked.append(l.glow, l.arm, l.core);
      return l;
    });
    const m = /^1\/(\d+)(T?)$/.exec(L.txt(ID.P_ARP_RATE)), bpm = (sc.tempo ?? P[ID.P_TEMPO].def) + 30;
    const step = (m ? 96 / +m[1] * (m[2] ? 2 / 3 : 1) : 6) * 60 / (24 * bpm);   // (seconds: RATE's MIDI clocks at TEMPO)
    svg._sigx = r2(step * 1000);                     // (RATE, TEMPO: not drawn, but its pace)
    return live(svg, (f, ctx) => {
      const on = !!f && !reduce.matches, K = ctx.keep.arp || (ctx.keep.arp = { hits: [], seen: -1 });   // (the steps struck:
      svg.classList.toggle("is-live", on);                                    // {n (as it sounds), t}, the card's)
      if (!on) { K.hits = []; K.seen = -1; return; }
      const hits = K.hits;
      if (K.seen >= 0 && f.strikes > K.seen)            // (those struck since the last frame: the voices struck last)
        f.struck.slice(0, Math.min(4, f.strikes - K.seen)).reverse().forEach((v) => hits.push({ n: v.n, t: ctx.t - v.t }));
      K.seen = f.strikes;
      if (hits.length > N) hits.splice(0, hits.length - N);
      const line = [];
      lights.forEach((l, j) => {
        const hit = hits[hits.length - 1 - j], x = hit ? x1 - (ctx.t - hit.t) / step * slot : 0;
        if (!hit || x < 34) { l.glow.setAttribute("opacity", 0); l.arm.setAttribute("opacity", 0); l.core.setAttribute("opacity", 0); return; }
        const y = Y(hit.n), fresh = Math.max(0, 1 - (x1 - x) / slot / 3), fade = Math.min(1, (x - 34) / 120);
        line.unshift(r2(x) + "," + r2(y));
        at(l.glow, { cx: x, cy: y, r: 6.5 + 6 * fresh, opacity: fade });
        at(l.arm, { d: star(x, y, 3.8 + 3.4 * fresh), opacity: (0.7 + 0.3 * fresh) * fade });
        at(l.core, { cx: x, cy: y, r: 1.1 + 0.9 * fresh, fill: fresh > 0.5 ? C.white : C.fog, opacity: fade });
      });
      trail.setAttribute("points", line.join(" "));
    });
  }

  /* a note moved d steps along the scene's scale (harmony.c scale_shift; a scale this page doesn't know: as it is) */
  function scaleShift(L, note, d) {
    const deg = DEGREES[L.txt(ID.P_SCALE)], r = L.raw(ID.P_ROOT) % 12;
    if (!deg || !d) return note;
    const inS = (n) => deg.includes(((n - r) % 12 + 12) % 12) ? 1 : 0;
    let base = note;
    while (!inS(base)) base--;
    note -= base;
    while (d > 0) { base++; d -= inS(base); }
    while (d < 0) { base--; d += inS(base); }
    return Math.max(1, Math.min(126, note + base));
  }
  /* the bass's sequence as it plays (app.c bseq_note), its STEPS: a NOTE moved TRANSPOSE's steps along the scale, then
   * BASS's OCTAVE, C1 at the lowest (bass_fold); a CUT -1, a HOLD -2 */
  function bassPlays(bq, L) {
    const oct = 12 * (L.raw(ID.P_BS_OCTAVE) - 2);
    return bq.notes.slice(0, bq.steps + 1).map((b) => {
      if (b === S.bseq.cut) return -1;
      if (b === S.bseq.hold) return -2;
      let n = scaleShift(L, b, bq.trans - P[ID.P_BQ_TRANS].def) + oct;
      while (n < 24) n += 12;
      return Math.min(n, 127);
    });
  }
  /* the notes its sequence plays (none while SEQUENCE is off) */
  const bqNotes = (sc, L) => (L && sc.bseq && sc.bseq.on ? bassPlays(sc.bseq, L).filter((n) => n >= 0) : []);
  /* BASS's sequence: a line of time, each step as wide as its LENGTH; a NOTE a lamp line at its height, on through the
   * HOLDs after it (round from the end: the sequence loops), a CUT a gap. While it plays, its playhead along it, the
   * note sounding lit where it is */
  function bassStrip(bq, L, id) {
    const x0 = 40, x1 = 504, top = 12, bot = 50, axis = 62, notes = bassPlays(bq, L);
    const len = notes.map((_, i) => S.bseq.ticks[bq.lens[i] % S.bseq.ticks.length]), total = len.reduce((a, t) => a + t, 0);
    const words = BQ_LEN != null && P[BQ_LEN] ? P[BQ_LEN].texts : S.bseq.ticks.map(String), at0 = [0];
    len.forEach((t) => at0.push(at0[at0.length - 1] + t));
    const X = (t) => x0 + t * (x1 - x0) / total;
    const head = s("rect", { x: -10, y: 4, width: 1.2, height: 62, fill: C.lamp, opacity: 0 });
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 82", role: "img" },
      s("defs", null, s("radialGradient", { id: `${id}-bg` },
        s("stop", { offset: 0, "stop-color": C.lamp, "stop-opacity": 0.45 }), s("stop", { offset: 1, "stop-color": C.lamp, "stop-opacity": 0 }))),
      s("line", { x1: 26, y1: top, x2: 512, y2: top, stroke: C.steel, "stroke-dasharray": "1 3" }),
      s("line", { x1: 26, y1: bot, x2: 512, y2: bot, stroke: C.steel, "stroke-dasharray": "1 3" }),
      s("rect", { x: 26, y: axis, width: 486, height: 1, fill: C.steel }), head);
    const sound = notes.filter((n) => n >= 0);
    let lo = Math.min(...sound), hi = Math.max(...sound);
    if (!sound.length) { lo = 36; hi = 48; }
    if (hi - lo < 12) { const m = Math.floor((hi + lo) / 2); lo = m - 6; hi = m + 6; }
    const Y = (n) => bot - (n - lo) * (bot - top) / (hi - lo);
    let cur = -1;                                    // (what sounds as it comes round: the end's)
    notes.forEach((n) => { if (n !== -2) cur = n; });
    const runs = [];
    notes.forEach((n, i) => {
      if (n >= 0) runs.push({ n, a: at0[i], b: at0[i + 1], on: true });
      else if (n === -2 && cur >= 0) {
        const last = runs[runs.length - 1];
        if (last && last.b === at0[i]) last.b = at0[i + 1]; else runs.push({ n: cur, a: at0[i], b: at0[i + 1], on: false });
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
    const glow = s("circle", { r: 9, fill: `url(#${id}-bg)`, opacity: 0 }), core = s("circle", { r: 2.2, fill: C.white, opacity: 0 });
    svg.append(...at0.map((t, i) => s("rect", { x: r2(X(t) - 0.5), y: axis + 2, width: 1, height: i % 4 ? 2 : 4, fill: i % 4 ? C.steel : C.slate })),
               ...glows, ...lines, ...heads, glow, core);
    const tr = bq.trans !== P[ID.P_BQ_TRANS].def ? "TRANSPOSE " + word(ID.P_BQ_TRANS, bq.trans) : "";
    svg.append(label(4, r2(top + 3.5), "start", nn(hi), C.dim), label(4, r2(bot + 3.5), "start", nn(lo), C.dim),
               label(r2(X(0)), 77, "middle", "1", C.dim), label(r2(x1), 77, "end", (tr ? tr + " · " : "") + "STEPS " + notes.length, C.fog));
    const say = notes.map((n, i) => (n === -1 ? "CUT" : n === -2 ? "HOLD" : nn(n)) + " " + words[bq.lens[i] % words.length]);
    svg.setAttribute("aria-label", `BASS's sequence: ${notes.length} steps${tr ? ", " + tr : ""}: ${say.join(", ")}.`);
    return live(svg, (f) => {
      if (!f || f.bq < 0 || f.bq >= notes.length || !total) { [head, glow, core].forEach((e) => e.setAttribute("opacity", 0)); return; }
      const x = X(at0[f.bq] + Math.min(Math.max(f.bqT, 0), len[f.bq]));
      at(head, { x: x - 0.6, opacity: 0.55 });
      if (!f.bass) { glow.setAttribute("opacity", 0); core.setAttribute("opacity", 0); return; }
      const y = Math.max(4, Math.min(58, Y(f.bass.n)));
      at(glow, { cx: x, cy: y, opacity: f.bass.l });
      at(core, { cx: x, cy: y, opacity: f.bass.l });
    });
  }

  // ---------------------------------------------------------------- the sections' small pictures
  const glyphSvg = (w, ht) => s("svg", { class: "sec-gl", width: w, height: ht, viewBox: `0 0 ${w} ${ht}`, "aria-hidden": "true" });
  /* the play page's strata: CHOIR the current, PARTIALS the crystals, BREATH the haze, WARMTH embers. While it plays,
   * each as long as the LFOs make it now, breathing with the tide, the dotted ones drifting along as a current does */
  const STRATA = [[C.mist, 1.2, "none", 1, "butt"], [C.ice, 1.2, "2.2 1.8", 1, "butt"], [C.mist, 3, "none", 0.42, "round"], [C.ember, 1.3, "1 2.4", 1, "butt"]];
  function strata(L) {
    const g = glyphSvg(52, 15), lines = [];
    STRATA.forEach((st, i) => {
      const y = r2(1.75 + i * 3.85), n = L.raw(SOUND[i]) / 127 * 48;
      const ln = s("line", { x1: 2, x2: r2(2 + n), y1: y, y2: y, stroke: st[0], "stroke-width": st[1], "stroke-dasharray": st[2], "stroke-opacity": n > 0.5 ? st[3] : 0, "stroke-linecap": st[4] });
      g.append(s("line", { x1: 2, x2: 50, y1: y, y2: y, stroke: C.steel, "stroke-width": 1, "stroke-dasharray": "1 2" }), ln);
      lines.push(ln);
    });
    return live(g, (f, ctx) => lines.forEach((ln, i) => {
      const p = SOUND[i], n = Math.max(0, Math.min(127, L.raw(p) + (f ? ctx.mod[p] || 0 : 0))) / 127 * 48;
      at(ln, { x2: 2 + n, "stroke-opacity": n > 0.5 ? STRATA[i][3] * (f ? 0.6 + 0.2 * (f.tide + 1) : 1) : 0,
               "stroke-dashoffset": f && STRATA[i][2] !== "none" && !reduce.matches ? -((ctx.t * (2 + i)) % 8) : 0 });
    }));
  }
  /* app.c's lfo_rnd: RANDOM's value for turn c of LFO i (0..2), -1..1, the engine's own */
  function lfoRnd(i, c) {
    let x = (Math.imul((c + 1) >>> 0, 2654435761) ^ Math.imul(i + 1, 0x9E3779B9)) >>> 0;
    x ^= x >>> 15;
    x = Math.imul(x, 0x85EBCA77) >>> 0;
    x ^= x >>> 13;
    return (x >>> 8) * (2 / 16777216) - 1;
  }
  /* app.c's lfo_shape: SHAPE at x (0..1) of turn c of LFO i, -1..1 */
  function lfoShape(sh, x, c, i) {
    if (sh === 0) return Math.sin(2 * Math.PI * x);
    if (sh === 1) return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4;
    if (sh === 2) return 2 * x - 1;
    if (sh === 3) return 1 - 2 * x;
    if (sh === 4) return x < 0.5 ? 1 : -1;
    if (sh === 5) return lfoRnd(i, c);
    const a = lfoRnd(i, c), b = lfoRnd(i, c + 1);
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
      const u = x / W * turns, c = Math.floor(u), f = rnd ? u - c : Math.min(u, 0.9999), v = lfoShape(sh, f, c, i - 1);
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
  /* MIXER: each track's level in its colour (DRONE fog, BASS lamp, GLINT ice, CLOUD haze lit), 100 marked; while it
   * plays, the track's meter beside it */
  const MIX = [[ID.P_MX_DRONE, C.fog, "DRONE"], [ID.P_MX_BASS, C.lamp, "BASS"], [ID.P_MX_GLINT, C.ice, "GLINT"], [ID.P_MX_CLOUD, C.cloud, "CLOUD"]];
  function faders(L) {
    const g = glyphSvg(32, 15), meters = [];
    MIX.forEach(([p, col], i) => {
      const x = 2 + i * 8, ht = L.raw(p) / 127 * 14, m = s("rect", { x: x + 4.3, y: 14.5, width: 1.2, height: 0, fill: col });
      g.append(s("rect", { x: x + 1, y: 0.5, width: 1, height: 14, fill: C.steel }),
               s("rect", { x, y: r2(14.5 - ht), width: 3, height: r2(ht), fill: col }),
               s("rect", { x: x - 1.5, y: r2(14.5 - 100 / 127 * 14), width: 6, height: 0.8, fill: C.mist }), m);
      meters.push(m);
    });
    return live(g, (f) => meters.forEach((m, i) => { const ht = f ? dB(f.m[i]) * 14 : 0; at(m, { y: 14.5 - ht, height: ht }); }));
  }
  /* MIXER, opened: the FM-1's MIXER page, a fader a track (its level, 100 marked, 127 at the top), and beside it the
   * track's meter while it plays (48 dB) */
  function mixerStrip(L) {
    const top = 8, bot = 66, H = bot - top, meters = [];
    const svg = s("svg", { class: "strip", viewBox: "0 0 516 84", role: "img" });
    MIX.forEach(([p, col, name], i) => {
      const cx = 74 + i * 122, v = Math.min(L.raw(p), 127), y = bot - v / 127 * H, y100 = bot - 100 / 127 * H;
      const m = s("rect", { class: "mx-m", x: cx + 11, y: bot, width: 3, height: 0, fill: col, opacity: 0.9 });
      svg.append(s("rect", { x: cx - 0.5, y: top, width: 1, height: H, fill: C.steel }),
        s("rect", { x: cx - 1.5, y: r2(y), width: 3, height: r2(bot - y), fill: col, opacity: 0.8 }),
        s("rect", { x: cx - 8, y: r2(y100 - 0.5), width: 4, height: 1, fill: C.mist }),
        s("rect", { x: cx - 6, y: r2(y - 1.2), width: 12, height: 2.4, rx: 1.2, fill: C.fog }),
        s("rect", { x: cx + 11, y: top, width: 3, height: H, fill: C.rule }), m,
        label(cx + 4, 80, "middle", name, C.dim));
      meters.push(m);
    });
    svg.setAttribute("aria-label", "MIXER: " + MIX.map(([p, , name]) => `${name} ${L.txt(p)}`).join(", ") + ", each track's meter beside it while it plays.");
    return live(svg, (f) => meters.forEach((m, i) => { const ht = f ? dB(f.m[i]) * H : 0; at(m, { y: bot - ht, height: ht }); }));
  }
  /* the LFOs, a column at the card's left: each as lfo_scope draws it, its shape over a turn (four for RANDOM and
   * DRIFT: the three before and this one) as deep as DEPTH from POLARITY's side, its name, SHAPE and RATE over it and
   * what it moves under it, each in its track's colour with a gauge of its value. While it plays, a glint where it is
   * now (its turns' own values), and the gauges at the values the LFOs make. Off (no DEPTH, no TARGET): slate, OFF */
  function lfoColumn(L) {
    return h("aside", { class: "oc-lfo", "aria-label": "The LFOs" }, h("div", { class: "sh-sub" }, h("h4", null, "LFOS")), [0, 1, 2].map((i) => lfoBlock(L, i)));
  }
  /* lfo_scope's picture of LFO i (0..2), Wd × Ht: its shape over a turn (four for RANDOM and DRIFT: the three before
   * and this one) as deep as DEPTH from POLARITY's side. While it plays, a glint where it is now (its turns' own
   * values); off (no DEPTH, no TARGET), slate */
  function lfoScope(L, i, Wd, Ht) {
    const b = S.lfo.base[i], V = (k2) => L.raw(b + k2), z = Math.min(1.3, Ht / 56);
    const sh = V(0) % 7, depth = V(2), pol = V(3) % 3, rnd = sh >= 5, turns = rnd ? 4 : 1;
    const on = lfoOn(L, b), col = on ? C.ice : C.slate, id = "lf" + (++uid);
    const top = Ht * 9 / 56, bot = Ht * 47 / 56, d = 0.2 + 0.8 * depth / 127, base = pol === 1 ? bot : pol === 2 ? top : (top + bot) / 2;
    const Y = (v) => (pol === 1 ? bot - 0.5 * (v + 1) * (bot - top) * d : pol === 2 ? top + 0.5 * (v + 1) * (bot - top) * d : base - v * 0.5 * (bot - top) * d);
    const path = s("path", { fill: "none", stroke: col, "stroke-width": r2(1.4 * Math.sqrt(z)), "stroke-linejoin": "round", "stroke-opacity": on ? 0.9 : 0.8 });
    const soft = s("path", { fill: "none", stroke: col, "stroke-width": r2(4 * z), "stroke-opacity": on ? 0.1 : 0, "stroke-linejoin": "round" });
    const head = s("rect", { y: r2(Ht * 3 / 56), width: 1, height: r2(Ht * 50 / 56), fill: col, opacity: 0 });
    const glow = s("circle", { r: r2(7 * z), fill: `url(#${id}-g)`, opacity: 0 }), arms = s("path", { fill: C.ice, opacity: 0 }), core = s("circle", { r: r2(1.5 * z), fill: C.white, opacity: 0 });
    const svg = s("svg", { class: "lf-sc", viewBox: `0 0 ${Wd} ${Ht}`, "aria-hidden": "true" },
      s("defs", null, s("radialGradient", { id: `${id}-g` }, s("stop", { offset: 0, "stop-color": C.ice, "stop-opacity": 0.55 }), s("stop", { offset: 1, "stop-color": C.ice, "stop-opacity": 0 }))),
      s("line", { x1: 0, x2: Wd, y1: r2(base), y2: r2(base), stroke: C.steel }), head, soft, path, glow, arms, core);
    let drawn = null;
    const draw = (cyc) => {                          // (its turns: cyc the one it is in, the three before it)
      if (drawn === cyc) return;
      drawn = cyc;
      let d2 = "";
      for (let x = 0; x <= Wd; x += Wd > 300 ? 2 : 1) {
        const u = Math.min(x / Wd * turns, turns - 1e-4), c = rnd ? cyc + Math.floor(u) - 3 : 0;
        d2 += (x ? "L" : "M") + x + " " + r2(Y(lfoShape(sh, u - Math.floor(u), c, i)));
      }
      path.setAttribute("d", d2);
      soft.setAttribute("d", d2);
    };
    draw(3);
    return live(svg, (f) => {
      const q = f && on && f.lfo[i];
      if (!q || !q.on) { [head, glow, arms, core].forEach((e) => e.setAttribute("opacity", 0)); draw(3); return; }
      if (rnd) draw(q.cyc);
      const x = (rnd ? (3 + q.ph) / 4 : q.ph) * Wd, y = Y(q.out);
      at(head, { x, opacity: 0.22 });
      at(glow, { cx: x, cy: y, opacity: 1 });
      at(arms, { d: star(x, y, 6.5 * z), opacity: 0.85 });
      at(core, { cx: x, cy: y, opacity: 1 });
    });
  }
  function lfoBlock(L, i) {
    const b = S.lfo.base[i], V = (k2) => L.raw(b + k2);
    const tg = [4, 5, 6, 7].map((t) => V(t)).filter((t) => t > 0).map((t) => S.lfo.targets[t] || { name: word(b + 4, t), track: "drone", p: -1 });
    const on = V(2) > 0 && tg.length > 0;
    /* a TARGET's gauge: its value (_v) and range (_mx), what moves it (_p), found from itself (patched, the gauge shown
     * takes the new one's) */
    const put2 = (g, v) => g.style.setProperty("--f", r2(Math.max(0, Math.min(1, v / Math.max(1, g._mx)))));
    const gauges = tg.map((t) => {
      const g = h("span", { class: "lf-g", "aria-hidden": "true" }, h("i"), h("b"));
      Object.assign(g, { _p: t.p, _v: t.p >= 0 ? L.raw(t.p) : 0, _mx: t.p >= 0 ? P[t.p].max : 127, _shell: true });
      put2(g, g._v);
      return h("li", null, h("span", { class: "lf-t", style: `color:${TRACK[t.track] || C.fog}` }, t.name), g);
    });
    const el = h("div", { class: "lf" + (on ? "" : " is-off") },
      h("div", { class: "lf-h" }, h("span", { class: "lf-n" }, "LFO " + (i + 1)),
        h("span", { class: "lf-r" }, on ? [h("span", null, L.txt(b)), h("span", { class: "lf-rt" }, L.txt(b + 1))] : h("span", null, "OFF"))),
      lfoScope(L, i, 176, 56), tg.length ? h("ul", { class: "lf-ts", role: "list" }, gauges) : h("p", { class: "lf-none" }, "NO TARGET"));
    el._shell = true;                                // (patched: its picture, held, kept if drawn the same)
    return live(el, function (f, ctx) { this.querySelectorAll(".lf-g").forEach((g) => put2(g, g._v + (f ? ctx.mod[g._p] || 0 : 0))); });
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
   * (built when first opened); MIXER's faders first in it. A card's (o.card): no HARMONY (THE KEYS have it), an LFO's
   * picture first in its own, wide (as it moves: one turned is seen moving here); else BASS's sequence (bq), drawn
   * first in it while on */
  function sections(L, bq, o = {}) {
    const box = h("div", { class: "secs" });
    for (const sec of SECTIONS) {
      if (o.card && sec.name === "HARMONY") continue;
      const pages = sec.pages.filter((pg) => shownFor(pg, L)), lfo = o.card && sec.name.match(/^LFO (\d+)$/);
      if (!pages.length) continue;
      const { gl, sig } = signature(sec, L, bq);
      const d = h("details", { class: "sec", "data-key": sec.name },
        h("summary", null, h("span", { class: "sec-row" }, chev(), h("span", { class: "sec-name" }, sec.name),
          h("span", { class: "sec-mid" }, gl, sig), hints(sec.keys))));
      d._shell = true;                           // (patched: the section shown takes this one's _build, _built)
      d._build = (el) => {                       // (on its first opening; redraw: at once, its cells there for the focus)
        if (el._built) return;
        el._built = true;
        const art = el.closest("article");
        if (art) art._lv = null;
        el.append(stamp(h("div", { class: "sec-body" }, !o.card && sec.name === "BASS" && bq && bq.on && h("div", { class: "scr" }, bassStrip(bq, L, "c" + (++uid))),
          lfo && S.lfo.base[lfo[1] - 1] !== undefined && h("div", { class: "scr lf-wide" }, lfoScope(L, lfo[1] - 1, 516, 90)),
          sec.name === "MIXER" && h("div", { class: "scr" }, mixerStrip(L)),
          pages.map((pg) => h("div", { class: "pg", "data-key": pg.name }, h("div", { class: "pg-name" }, pg.name, h("span", { class: "pg-k" }, h("span", { class: "sr-only" }, ", on "), pg.button)),
            pg.keep.map((p) => cell(p, L)))))));
        if (art) hooks.built(art);
      };
      d.addEventListener("toggle", () => { if (d.open) d._build(d); });
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
  function litNotes(sc, L, top = 96) {
    const PRI = { arpr: 1, arp: 2, note: 3, root: 4 }, st = {};
    const set = (n, what) => { if (n >= 24 && n <= top && (!st[n] || PRI[what] > PRI[st[n]])) st[n] = what; };
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
  /* WHAT PLAYS: the keyboard (in kb-in, the waterline that ripples while it sounds) and its legend */
  function plays(sc, L, id) {
    const kb = keyboard(sc, L, id);
    return h("section", { class: "sh-plays", "aria-labelledby": `${id}-wp` },
      h("div", { class: "sh-sub" }, h("h4", { id: `${id}-wp` }, "WHAT PLAYS"), h("span", { class: "sh-range", "aria-hidden": "true" }, "C1 – " + nn(kb._top))),
      h("div", { class: "scr" }, L ? h("div", { class: "kb-in" }, kb, waveSvg()) : kb), legend(sc, L, kb._top));
  }
  /* GLINT at the card's left: its type (ARP, SEQ, OFF), PRESET and OCTAVE, then its walk or its sequence (live) */
  function glint(sc, L, id) {
    return h("section", { class: "sh-glint", "aria-labelledby": `${id}-gl` },
      h("div", { class: "sh-sub gl-sub" }, h("h4", { id: `${id}-gl` }, "GLINT"), h("span", { class: "gl-type" + (L.type ? "" : " is-off") }, L.txt(ID.P_SQ_TYPE)),
        h("span", { class: "gl-meta" }, h("span", { class: "sig-l" }, "PRESET"), h("span", { class: "gl-v" }, L.txt(ID.P_GL_PRESET)),
          h("span", { class: "sig-l" }, "OCTAVE"), h("span", { class: "gl-v" }, L.txt(ID.P_GL_OCTAVE)))),
      L.seqOn && h("div", { class: "scr" }, seqStrip(sc, L, id)), L.arpOn && h("div", { class: "scr" }, arpStrip(sc, L, id)));
  }
  /* BASS under it: SEQ (lamp: its sequence on) or NOTE, MODEL and OCTAVE, then its sequence (live) or one quiet line:
   * the note it holds */
  function bassBlock(sc, L, id) {
    const bq = sc.bseq, on = !!(bq && bq.on);
    return h("section", { class: "sh-bass", "aria-labelledby": `${id}-bs` },
      h("div", { class: "sh-sub gl-sub" }, h("h4", { id: `${id}-bs` }, "BASS"), h("span", { class: "gl-type" + (on ? " is-bq" : " is-off") }, on ? "SEQ" : "NOTE"),
        h("span", { class: "gl-meta" }, h("span", { class: "sig-l" }, "MODEL"), h("span", { class: "gl-v" }, L.txt(ID.P_BS_MODEL)),
          h("span", { class: "sig-l" }, "OCTAVE"), h("span", { class: "gl-v" }, L.txt(ID.P_BS_OCTAVE)))),
      on ? h("div", { class: "scr" }, bassStrip(bq, L, id + "b"))
         : h("p", { class: "bq-none" }, "NO SEQUENCE · " + (sc.bass >= 0 ? nn(sc.bass) + " HELD" : "NOTHING HELD")));
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
      [P[ID.P_LOAD].name, word(ID.P_LOAD, st.load)], [P[ID.P_SPEAKER].name, st.speaker ? "ON" : "OFF"],
      ...(ID.P_ANIM != null ? [[P[ID.P_ANIM].name, st.anim ? "LOW" : "NORMAL"]] : []),
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
  function row(sc0, o = {}) {
    const id = "c" + (++uid), k2 = o.key != null ? String(o.key) : id;
    const sc = hooks.scene(k2, sc0, o), L = sc.current ? look(sc.v) : null;   // (a draft of the editor's: shown edited)
    const art = h("article", { class: "sr-item" + (o.class ? " " + o.class : ""), id: o.id || null, "aria-labelledby": `${id}-n`, style: `--i:${Math.min(o.i || 0, 24)}` });
    const play = h("button", { type: "button", class: "play", disabled: !sc.bytes, "aria-label": `Play ${sc.name}`, "aria-pressed": "false",
                               title: sc.bytes ? null : "Restore it to the FM-1 to hear it",
                               onclick: () => { toggleSound(k2); if (o.openOnPlay) openRow(art, true); } }, playIcon());   // (o.openOnPlay: ▶
                                                                                                                  // opens the card too)
    const tick = o.tick && h("label", { class: "ck" }, h("input", { type: "checkbox", checked: !!o.tick.checked, disabled: !!o.tick.disabled,
      "aria-label": o.tick.aria || `Select ${sc.name}`, onchange: (e) => { if (o.tick.onChange) o.tick.onChange(e.target.checked); } }));
    const open = h("button", { type: "button", class: "sr-open", "aria-expanded": "false", "aria-controls": `${id}-more`, "aria-label": `Open ${sc.name}`,
                               onclick: () => openRow(art) }, I.up(15));
    art.append(h("div", { class: "sr" }, tick, o.no != null ? h("span", { class: "no" }, h("span", { class: "sr-only" }, "Slot "), o.no) : null, play,
      h("span", { class: "nm" }, shard(sc, 26, true), h("span", { class: "nm-t" },
        h("h3", { class: "nm-n", id: `${id}-n`, tabindex: "-1" }, sc.name), o.desc ? h("span", { class: "nm-d" }, o.desc) : null,
        o.sub ? h("span", { class: "nm-s" }, o.sub) : null, o.mark ? h("span", { class: "nm-mk" }, o.mark) : null)),
      headCells(sc, L), (o.cells || []).map((c) => { if (c && c.classList) c.classList.add("cx"); return c; }),
      o.side ? h("span", { class: "side cx" }, o.side) : null,
      h("span", { class: "acts" }, o.actions || null), open),
      h("p", { class: "sh-say", role: "status" }),
      h("div", { class: "oc-more", id: `${id}-more`, inert: true }, h("div", { class: "oc-in" })));
    art._sc = sc;
    art._build = () => {
      const sc = art._sc, L = sc.current ? look(sc.v) : null;
      const intro = o.intro ? h("div", { class: "oc-intro" }, o.intro()) : null;
      if (!L) return [intro, h("div", { class: "oc-body is-old" }, h("div", { class: "oc-left" }, plays(sc, null, id)),
        h("p", { class: "sh-note" }, "Made by an earlier FRASIL: its sound is brought up to date when the backup is restored. To make it a shard on its own, restore the whole backup, then back up again."))];
      return [intro, body(sc, L, id, art)];
    };
    art._open = o.onOpen || null;
    art._key = k2;
    art._id = id;
    REG.set(k2, { art, sc, o, sc0 });
    hooks.row(art, sc0, o);
    return art;
  }
  /* a row's own columns in its head: what plays, its key, GLINT's type, the bass's sequence */
  function headCells(sc, L) {
    const kv = L ? h("span", { class: "kv cx" }, h("span", { class: "sr-only" }, "Key "), `${L.txt(ID.P_ROOT)} ${L.txt(ID.P_SCALE)} `, h("i", null, `· ${L.txt(ID.P_CHORD)}`))
                 : h("span", { class: "kv cx" }, h("span", { class: "badge is-old" }, "EARLIER FRASIL"));
    const gt = L && L.type ? h("span", { class: "gt cx" }, h("span", { class: "sr-only" }, "GLINT "), L.txt(ID.P_SQ_TYPE))
                           : h("span", { class: "gt off cx", "aria-hidden": "true" }, "—");
    const bq = sc.bseq && sc.bseq.on ? h("span", { class: "bq cx", title: "The bass plays its sequence" }, h("span", { class: "sr-only" }, "BASS "), "SEQ")
                                     : h("span", { class: "bq-no cx" });
    const ms = mini(sc, L);
    ms.classList.add("cx");
    return [ms, kv, gt, bq];
  }
  /* an opened card's body: what moves at the left (GLINT, the bass, the LFOs), every page at the right, THE KEYS the
   * whole width under them (river-keys.js: the keys, then the track's settings; the keys held at the window's bottom
   * while the card is on it, so the body holds them, not a grid of its own) */
  function body(sc, L, id, art) {
    const left = h("div", { class: "oc-left" }, glint(sc, L, id), bassBlock(sc, L, id), lfoColumn(L));
    const right = h("div", { class: "oc-right" }, h("div", { class: "sx-h", "aria-hidden": "true" }, h("span", null, "EVERY PAGE"), h("span", null, "ON THE FM-1")),
      sections(L, sc.bseq, { card: true }));
    return h("div", { class: "oc-body" }, h("div", { class: "oc-cols" }, left, right), window.RiverKeys ? RiverKeys.panel(sc, L, art, id) : null);
  }
  /* a row drawn again with another scene (the editor's: the same shard, its sound changed): its head's columns, its
   * card's body if built, what ▶ plays. The new drawing laid over the one shown (patch()): only what differs changes,
   * the sections built built again (open as they were), THE KEYS their own (river-keys.js); the editor's marks on it
   * first (hooks.fresh), so that they stay as they are */
  function redraw(art, sc) {
    const c = REG.get(art._key), L = sc.current ? look(sc.v) : null;
    art._sc = sc;
    if (c) { c.sc = sc; c.L = undefined; }
    const old = art.querySelectorAll(":scope > .sr > .ms, :scope > .sr > .kv, :scope > .sr > .gt, :scope > .sr > .bq, :scope > .sr > .bq-no");
    const fresh = headCells(sc, L);
    if (old.length === 4) old.forEach((e, i) => patch(e, fresh[i]));
    const was = art.querySelector(".oc-body");
    if (was && L) {
      const built = new Set([...was.querySelectorAll("details.sec")].filter((d) => d._built).map((d) => d.dataset.key));
      const now = body(sc, L, art._id || "c" + (++uid), art);
      now.querySelectorAll("details.sec").forEach((d) => { if (built.has(d.dataset.key)) d._build(d); });
      stamp(now);
      hooks.fresh(art, now);
      patch(was, now);
    }
    art._lv = null;
    if (soundKey === art._key && c) heard(c, c.last ? c.last.f : null, c.last ? c.last.t : 0);   // (playing: what is heard on it at once)
  }
  /* a card's live parts found again (THE KEYS drawn again in place), what is heard put on them at once */
  function relive(art) {
    art._lv = null;
    const c = REG.get(art._key);
    if (c && soundKey === art._key) heard(c, c.last ? c.last.f : null, c.last ? c.last.t : 0);
  }
  /* a cell drawn again in place (the editor, while a setting turns: the element kept, its pointer held; only what
   * differs in it changes; prep(fresh): the editor's marks on the new one first) */
  function refreshCell(el, p, v, prep) {
    const fresh = cell(p, look(v)), was = !!el._live;
    if (prep) prep(fresh);
    patchKids(el, fresh);
    for (const x of own(el)) if (!(x in fresh)) delete el[x];
    for (const x of own(fresh)) el[x] = fresh[x];
    el.classList.toggle("lv", !!fresh._live);
    const art = was !== !!fresh._live && el.closest(".sr-item");
    if (art) art._lv = null;
  }
  /* the editor's hooks (river-edit.js): the scene a row shows (its draft), a row made, a card's body or a section
   * built, a card's new drawing before it is laid over the one shown (fresh(art, root): its marks) */
  const hooks = { scene: (k, sc) => sc, row: () => {}, built: () => {}, fresh: () => {} };

  /* a row opened (or closed) in place: its head the card's, the rows below glide down */
  function openRow(art, open = !art.classList.contains("open"), quiet = false) {
    if (open === art.classList.contains("open")) return;
    const inner = art.querySelector(".oc-in"), btn = art.querySelector(".sr-open"), name = art.querySelector(".nm-n").textContent;
    if (open && art._build && !inner.firstChild) { inner.append(...art._build().filter(Boolean).map(stamp)); hooks.built(art); }
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
    pl.base = window.RIVER_EMU || "/emu/";   // (the engine at the River's root, whichever page plays; the Play page's beside it)
    pl.follow = true;                  // (what sounds, for the card playing: follow())
    pl.addEventListener("loading", (e) => sound(e.detail.id, "loading"));
    pl.addEventListener("playing", (e) => {
      sound(e.detail.id, "playing");
      follow();
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
    if (!state) {
      c.art.querySelectorAll(".ms-wave polyline").forEach((w) => w.setAttribute("points", "0,10 100,10"));
      heard(c, null, 0);
      c.keep = null;
    }
    soundFns.forEach((f) => f(k2, state, c.sc));
  }
  /* while it sounds, its card follows it. Its line ripples: two slow swells along it, as tall as the sound is loud (in
   * a row, its line of what plays; opened, the keyboard's waterline too). Opened, its live parts (.lv: the keyboard's
   * lights, the LFOs, GLINT's and the bass's playheads, the strata, the MIXER's meters, the settings an LFO moves)
   * take what is heard now (river-player.js live()), each frame. With reduced motion: no ripple, and the rest four
   * times a second */
  function follow() {
    cancelAnimationFrame(rippleRaf);
    let amp = 0, last = -1e9;
    const frame = (t) => {
      const pl = window.RiverPlayer, c = REG.get(soundKey);
      if (!pl || pl.state !== "playing" || !c) return;
      if (!reduce.matches) {
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
      }
      if (c.art.classList.contains("open") && (!reduce.matches || t - last >= 250)) {
        last = t;
        const v = pl.live ? pl.live() : null;
        heard(c, v ? heardOf(v) : null, t / 1000);
      }
      rippleRaf = requestAnimationFrame(frame);
    };
    rippleRaf = requestAnimationFrame(frame);
  }
  /* a card given what is heard (f: heardOf's), or put at rest (null): each of its live parts */
  function heard(c, f, t) {
    const art = c.art;
    if (!art._build || !art.querySelector(".oc-body")) return;
    if (c.L === undefined) c.L = c.sc.current ? look(c.sc.v) : null;
    if (!c.L) return;
    if (!art._lv) art._lv = [...art.querySelectorAll(".lv")];
    c.last = f ? { f, t } : null;
    const ctx = { t, keep: c.keep || (c.keep = {}), ...moved(c.L, f) };
    for (const el of art._lv) el._live(f, ctx);
  }
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !e.defaultPrevented && soundKey != null && !document.querySelector("dialog[open]")) stopSound(); });

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
    settingsRow, ticked, say, glide, row, openRow, redraw, refreshCell, relive, hooks, card: (k) => REG.get(k),
    stopSound, onSound: (f) => soundFns.push(f), get soundKey() { return soundKey; },
    toast, confirm,
    parts: { C, live, at, star, mix, r2, cell, legend, bassPlays, scaleShift, uid: () => ++uid },
    patch: { patch, kids: patchKids, place, stamp, sig: sigOf },
  };
})();
