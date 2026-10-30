/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * THE KEYS: an opened card's keyboard, the whole width at its foot (river-cards.js body()): what the shard plays, as it
 * sounds, and on a card the editor has (river-edit.js), what it plays edited, as the FM-1's keys and pages do it.
 *   KEYS        OVERVIEW (everything it plays, as it sounds), DRONE (fog), GLINT (ice), BASS (lamp): a track the keys
 *               play, alone on them. KEY MODE: LATCH (what it plays, edited) or HOLD (a key sounds while it is down, the
 *               shard playing; nothing kept). DRONE: CHORD MODE, HARMONY's CHORD at SINGLE (OFF) or back at the chord
 *               it was (ON). What the track plays, the legend, beside them
 *   the keys    C1 to C8: the scale's notes marked (the root ice), what is latched and what sounds lit (the card's
 *               frames), the track's own (OVERVIEW: all); the key under the pointer ghosts in lamp what a click does.
 *               ZOOM (- +, two fingers, a trackpad's pinch): the keys wider, scrolled (the strip under them: where
 *               they are, the track's notes)
 * On a screen wide and tall enough, THE KEYS (all but the track's settings) stay at the bottom of the window while
 * the card is on it (CSS: sticky; .is-stuck while they are).
 *   DRONE       a click latches the chord HARMONY makes on that key (eight at most), again lets it go; HARMONY under
 *               the keys
 *   GLINT       on ARP, a click adds the note to the arpeggio (sixteen at most), again takes it out; on SEQ, the
 *               sequence as a grid (a column a step, a row a note as it sounds), written with the pointer
 *   BASS        a click holds that note (another: it goes there), again lets it go; its SEQUENCE on, its sixteen steps
 *               over the keys: one selected, a key writes its NOTE there (AUTO NEXT STEP on: then the next is selected)
 * Each change goes through FRASIL's own engine (river-edit.js: the draft, the sound playing); the card's state (the
 * track, the mode, the step selected) stays the card's (art._keys) as it is drawn again.
 *   RiverKeys.panel(sc, L, art, id)   THE KEYS of card art, showing scene sc (L: RiverCards.look(sc.v)): [the keys,
 *               the track's settings under them]
 *   RiverKeys.values(sc, art)         sc's settings as THE KEYS show them (the bass's sequence's at their numbers)
 *   RiverKeys.hooks.act(art, ops, phase) -> {sc, res} | null   river-edit.js's: ops (["p", p, x] a setting, the
 *               sequences' steps too; ["latch", t, n]; ["clear", t]) into the card's shard; phase "one" (a gesture,
 *               whole), "move" (one under way: the scene it makes, to draw), "end" */
"use strict";
window.RiverKeys = (() => {
  const RC = RiverCards, { h, s, nn, word, look } = RC, X = RC.parts, { C, live, at, star, mix, r2 } = X;
  const S = FB.S, ID = S.ids, P = S.params, HM = S.harmony;
  const REST = S.seq.rest, TIE = S.seq.tie, ACC = S.seq.accent, CUT = S.bseq.cut, HOLD = S.bseq.hold;
  const WH = [0, 2, 4, 5, 7, 9, 11], LO = 24, HI = 108, TRACKS = ["drone", "glint", "bass"];
  const ZOOM = [1, 1.5, 2, 3, 4], ZMAX = 4, TRIAD = 2;   // (CHORD MODE ON with no chord before it: TRIAD)
  const hooks = { act: null };

  // ---------------------------------------------------------------- harmony.c, as the schema has it
  const inScale = (n, r, b) => (b >> ((((n - r) % 12) + 12) % 12)) & 1;
  const bitsOf = (L) => HM.scale_bits[L.raw(ID.P_SCALE) % HM.scale_bits.length];
  /* chord_build: the chord a played note makes (a note outside the scale falls to the scale's note below), its root */
  function chordOf(played, L) {
    const r = L.raw(ID.P_ROOT) % 12, b = bitsOf(L), ch = L.raw(ID.P_CHORD) % HM.shapes.length, vo = L.raw(ID.P_VOICING) % 3;
    let base = played;
    while (!inScale(base, r, b)) base--;
    const up = (n, k) => { while (k > 0) { n++; if (inScale(n, r, b)) k--; } return n; };
    const out = HM.shapes[ch].map((k) => (ch < HM.diatonic ? up(base, k) : base + k));
    if (vo === 1 && out.length >= 3) out[1] += 12;
    else if (vo === 2 && out.length >= 2) out[0] -= 12;
    return { root: base, notes: out.filter((n) => n >= 0 && n <= 127) };
  }

  // ---------------------------------------------------------------- a card's state
  const stateOf = (art) => art._keys || (art._keys = { track: "overview", mode: "latch", step: 0, next: true, hover: null });
  /* what the keys show of a track: all of it on OVERVIEW, else the track's own alone */
  const shows = (K, t) => K.track === "overview" || K.track === t;
  const editable = (art) => !!(art._edit && hooks.act);
  const playing = (art) => window.RiverPlayer && RiverPlayer.id === art._key && RiverPlayer.state === "playing";
  /* the settings THE KEYS show: the sound's, then the bass's sequence's at their own numbers (SEQUENCE, SEQ STEPS,
   * TRANSPOSE, the selected step's STEP LENGTH) */
  function values(sc, art) {
    const v = Array.from(sc.v), bq = sc.bseq;
    if (bq) {
      v[ID.P_BQ_ON] = bq.on ? 1 : 0;
      v[ID.P_BQ_STEPS] = bq.steps;
      v[ID.P_BQ_TRANS] = bq.trans;
      v[ID.P_BQ_LEN] = bq.lens[(art && art._keys ? art._keys.step : 0) % bq.lens.length];
    }
    return v;
  }
  const T = { drone: 0, glint: 1, bass: 2 };
  /* the bass's step as the keys show it: its NOTE with BASS's OCTAVE (C1 at the lowest), as a key writes it back */
  const bassOct = (L) => 12 * (L.raw(ID.P_BS_OCTAVE) - 2);
  const bassKey = (b, L) => { let n = b + bassOct(L); while (n < LO) n += 12; return Math.min(n, 127); };

  // ================================================================ THE KEYS
  function panel(sc, L, art, id) {
    const el = h("section", { class: "oc-keys", "aria-labelledby": `${id}-k` });
    el._art = art;
    el._id = id;
    el._set = h("div", { class: "oc-kset" });
    fill(el, sc, L);
    return [el, el._set];
  }
  /* the panel drawn (again, in place: a track, a mode, a step chosen) */
  function fill(el, sc, L) {
    const art = el._art, K = stateOf(art), ed = editable(art), id = el._id;
    if (!ed) K.mode = "latch";
    if (el._board) holdsOff();                       // (drawn again: what its keys held let go)
    el._sc = sc;
    el._L = L;
    const seg = (label, items, cur, pick, cls) => h("div", { class: "k-ctl" }, h("span", { class: "k-l", "aria-hidden": "true" }, label),
      h("div", { class: "k-seg", role: "group", "aria-label": label }, items.map(([v, t]) => h("button", {
        type: "button", class: (cls ? cls + v : "") + (v === cur ? " on" : ""), "aria-pressed": String(v === cur), "data-k": label + ":" + v,
        onclick: () => { if (v !== cur) pick(v); } }, t))));
    const chord = L.raw(ID.P_CHORD);
    if (chord) K.chord = chord;                      // (the chord CHORD MODE ON goes back to)
    const top = h("div", { class: "k-top" }, h("h4", { class: "k-title", id: `${id}-k` }, "THE KEYS"),
      seg("KEYS", [["overview", "OVERVIEW"], ["drone", "DRONE"], ["glint", "GLINT"], ["bass", "BASS"]], K.track, (v) => { K.track = v; K.hover = null; K.gwin = null; again(el); }, "t-"),
      ed && K.track !== "overview" ? seg("KEY MODE", [["latch", "LATCH"], ["hold", "HOLD"]], K.mode, (v) => { K.mode = v; K.hover = null; again(el); }) : null,
      ed && K.track === "drone" ? seg("CHORD MODE", [["on", "ON"], ["off", "OFF"]], chord ? "on" : "off",
        (v) => edit(el, [["p", ID.P_CHORD, v === "on" ? K.chord || TRIAD : 0]])) : null,
      legendOf(sc, L, K));
    const bqOn = !!(sc.bseq && sc.bseq.on), latch = K.mode === "latch" || K.track === "overview";
    const grid = latch && K.track === "glint" && L.seqOn, lane = latch && K.track === "bass" && bqOn;
    const board = h("div", { class: "k-board" + (grid ? " is-grid" : ""), tabindex: ed ? 0 : null, "data-k": "board",
      role: ed ? "application" : null, "aria-roledescription": ed ? (grid ? "grid of steps" : "keyboard") : null });
    el._board = board;
    el.classList.toggle("is-grid", grid);              // (GLINT's grid stays in its place: the keyboard alone is held)
    if (!grid) board.style.setProperty("--kz", zoomOf(K));
    live(board, (f, ctx) => { if (board._draw) board._draw._live(f, ctx); });
    if (ed) (grid ? wireGrid : wireKeys)(el, board);
    if (!grid) wireZoom(el, board);
    el._say = h("p", { class: "sr-only", "aria-live": "polite" });
    const parts = [top, el._say, lane ? laneOf(el, sc, L, K, ed) : null, board, grid ? null : stripOf(el, board),
      grid && ed ? h("p", { class: "k-how" }, "CLICK A NOTE: IT GOES · DRAG ITS END: LONGER · DRAG IT UP OR DOWN: ANOTHER NOTE · SHIFT-CLICK: ACCENT · LENGTH: DRAG ITS GRIP") : null];
    el.replaceChildren(...parts.filter(Boolean));
    el._set.replaceChildren(...[bottom(el, sc, L, K, ed)].filter(Boolean));
    draw(el);
    const sx = grid ? "gx" : "kx";                   // (the keys' scroll and the grid's, each its own)
    const put = () => { if (K[sx] != null) board.scrollLeft = K[sx]; if (grid && K.gy != null) board.scrollTop = K.gy; };
    if (K[sx] != null || (grid && K.gy != null)) { put(); requestAnimationFrame(put); }   // (a card drawn again is built apart: once in)
    else if (!grid) requestAnimationFrame(() => {    // (a phone: the keys opened where the notes are; a grid at its step 1)
      if (board.scrollWidth <= board.clientWidth + 2) return;
      const ns = notesOf(sc, L, K);
      const mid = ns.length ? (Math.min(...ns) + Math.max(...ns)) / 2 : 60;
      const x = keyX(Math.round(mid)) / 1200 * board.scrollWidth;
      board.scrollLeft = Math.max(0, x - board.clientWidth / 2);
      K.kx = board.scrollLeft;
    });
    board.addEventListener("scroll", () => {
      if (!board.isConnected) return;
      K[sx] = board.scrollLeft;
      if (grid) K.gy = board.scrollTop; else if (el._strip) el._strip._sync();
    }, { passive: true });
    if (!grid) requestAnimationFrame(() => { if (el._strip) el._strip._sync(); });
    stuckSoon();
  }
  /* drawn again in place, its live parts found again, its settings the editor's again */
  function again(el) {
    if (!el.isConnected) return;
    const f = focusKey(el), held = el.classList.contains("is-stuck"), r0 = el.getBoundingClientRect();
    if (held) el.style.minHeight = `${Math.max(parseFloat(el.style.minHeight) || 0, r0.height)}px`;   // (held: grown from its top,
    fill(el, el._sc, el._L);                         // its controls would move; as tall as it has been, they stay)
    if (held) {                                      // (held at the bottom, then no longer: the page follows them)
      stuck();
      if (!el.classList.contains("is-stuck")) scrollBy(0, el.getBoundingClientRect().top - r0.top);
    }
    RC.relive(el._art);
    RC.hooks.built(el._art);
    if (f) { const t = el.querySelector(`[data-k="${f}"]`); if (t) t.focus({ preventScroll: true }); }
  }
  const focusKey = (el) => { const a = document.activeElement; return a && el.contains(a) && a.dataset ? a.dataset.k : null; };
  /* the board's picture: the grid (GLINT's sequence, LATCH) or the keyboard */
  function draw(el) {
    const board = el._board;
    board._draw = board.classList.contains("is-grid") ? gridOf(el) : keysOf(el);
    board.replaceChildren(board._draw);
  }
  /* the legend, beside the controls: what the track plays (OVERVIEW: all), RANGE over C8 too; nothing for a track that
   * holds nothing there */
  function legendOf(sc, L, K) {
    const own = { ...sc, chords: shows(K, "drone") ? sc.chords : [], arp: shows(K, "glint") ? sc.arp : [], bass: shows(K, "bass") ? sc.bass : -1,
                  bseq: sc.bseq && !shows(K, "bass") ? { ...sc.bseq, on: false } : sc.bseq };
    const lg = X.legend(own, L, 127);
    if (K.track !== "overview" && lg.querySelector(".lg-only")) lg.replaceChildren();   // (nothing)
    lg.classList.add("k-legend");
    return lg;
  }
  /* the notes the track has, as they sound (where a phone opens the keys) */
  function notesOf(sc, L, K) {
    if (K.track === "overview") return ["drone", "glint", "bass"].flatMap((t) => notesOf(sc, L, { track: t }));
    if (K.track === "drone") return sc.chords.flatMap((c) => c.notes);
    if (K.track === "glint") return L.arpOn ? sc.arp.map((a) => a + 12 * L.oct) : L.seqOn ? sc.seq.map((b) => b & 127).filter((n) => n !== REST && n !== TIE).map((n) => n + 12 * L.oct) : [];
    if (sc.bseq && sc.bseq.on) return sc.bseq.notes.slice(0, sc.bseq.steps + 1).filter((b) => b !== CUT && b !== HOLD).map((b) => bassKey(b, L));
    return sc.bass >= 0 ? [sc.bass] : [];
  }

  /* an edit: river-edit.js's act; a gesture under way, the board drawn again from the scene it makes; else the card is
   * drawn again (by the editor; if nothing changed, here) */
  function edit(el, ops, phase = "one") {
    if (!hooks.act) return null;
    const r = hooks.act(el._art, ops, phase);
    if (r && r.res) r.res.forEach((x, i) => {
      if (x !== -1 || ops[i][0] !== "latch") return;
      const t = ops[i][1];
      if (t === 0) RC.toast("Eight drones at most: let one go first.");
      else if (t === 1) RC.toast("Sixteen notes at most in the arpeggio.");
    });
    if (phase === "move") {
      if (r && el.isConnected) { el._sc = r.sc; el._L = look(r.sc.v); draw(el); }
    } else if (el.isConnected) again(el);
    const K = el._art._keys;                          // (rows added over the grid: scrolled away, the notes kept still)
    if (K && K.gup && phase !== "move") {
      const b = el._art.querySelector(".oc-keys .k-board.is-grid"), svg = b && b.querySelector("svg");
      if (svg) {
        const px = K.gup * RH * svg.getBoundingClientRect().height / svg.viewBox.baseVal.height;
        if (b.scrollHeight > b.clientHeight + 1) b.scrollTop += px; else scrollBy(0, px);
      }
      K.gup = 0;
    }
    return r;
  }

  // ---------------------------------------------------------------- under the keys: the track's settings, its tools
  function bottom(el, sc, L, K, ed) {
    if (K.track === "overview") return null;
    const bq = sc.bseq, bqOn = !!(bq && bq.on), v = values(sc, el._art), LL = look(v), latch = K.mode === "latch";
    let title, ps, tools = [];
    const tool = (t, k, ops, off) => h("button", { type: "button", class: "a", "data-k": k, disabled: !!off, onclick: () => ops() }, t);
    if (K.track === "drone") {
      title = "HARMONY";
      ps = [ID.P_ROOT, ID.P_SCALE, ID.P_CHORD, ID.P_VOICING];
      tools = [tool("Clear", "clear", () => edit(el, [["clear", 0]]), !sc.chords.length)];
    } else if (K.track === "glint") {
      title = L.arpOn ? "GLINT ARP" : L.seqOn ? "GLINT SEQ" : "GLINT";
      ps = L.arpOn ? [ID.P_SQ_TYPE, ID.P_ARP_MODE, ID.P_ARP_RATE, ID.P_ARP_RANGE, ID.P_ARP_CHANCE, ID.P_GL_OCTAVE]
         : L.seqOn ? [ID.P_SQ_TYPE, ID.P_SQ_LEN, ID.P_ARP_RATE, ID.P_SQ_GATE, ID.P_ARP_CHANCE, ID.P_SQ_TRANS, ID.P_GL_OCTAVE]
         : [ID.P_SQ_TYPE, ID.P_GL_OCTAVE];
      if (L.arpOn) tools = [tool("Clear", "clear", () => edit(el, [["clear", 1]]), !sc.arp.length)];
      else if (L.seqOn) tools = [tool("Clear", "clear", () => edit(el, [["p", ID.P_SQ_CLEAR, 2]]), sc.seq.every((b) => (b & 127) === REST))];
    } else if (bqOn) {
      title = "BASS SEQ";
      ps = [ID.P_BQ_ON, ID.P_BQ_STEPS, ID.P_BQ_LEN, ID.P_BQ_TRANS, ID.P_BS_OCTAVE];
      const put = (b) => edit(el, writeStep(sc, K, b));
      tools = [h("div", { class: "k-ctl k-next" }, h("span", { class: "k-l", "aria-hidden": "true" }, "AUTO NEXT STEP"),
        h("div", { class: "k-seg", role: "group", "aria-label": "AUTO NEXT STEP" }, [[true, "ON"], [false, "OFF"]].map(([x, t]) =>
          h("button", { type: "button", class: K.next === x ? "on" : "", "aria-pressed": String(K.next === x), "data-k": "next:" + t,
            onclick: () => { if (K.next !== x) { K.next = x; again(el); } } }, t)))),
        tool("Cut", "cut", () => put(CUT)), tool("Hold", "hold", () => put(HOLD)),
        tool("Clear", "clear", () => edit(el, [["p", ID.P_BQ_CLEAR, 2]]), bq.notes.every((b) => b === CUT) && bq.lens.every((x) => !x))];
    } else {
      title = "BASS";
      ps = [ID.P_BQ_ON, ID.P_BS_OCTAVE, ID.P_BS_MODEL];
      tools = [tool("Clear", "clear", () => edit(el, [["clear", 2]]), sc.bass < 0)];
    }
    if (!sc.bseq) ps = ps.filter((p) => p < S.sizes.NSOUND);
    return h("div", { class: "k-bottom" }, h("div", { class: "k-set" }, h("p", { class: "k-l" }, title),
      h("div", { class: "k-cells" }, ps.map((p) => X.cell(p, LL)))), ed && latch && tools.length ? h("div", { class: "k-tools" }, tools) : null);
  }
  /* the selected step written (b: its NOTE as kept, CUT or HOLD), SEQ STEPS taking it in; AUTO NEXT STEP: the next
   * selected, round SEQ STEPS as the FM-1's STEP record goes (past them: on, each step written taking them in) */
  function writeStep(sc, K, b) {
    const i = K.step % 16, last = sc.bseq.steps, ops = [["p", ID.P_BQ_STEP, i], ["p", ID.P_BQ_NOTE, b]];
    if (i > last) ops.push(["p", ID.P_BQ_STEPS, i]);
    if (K.next) K.step = i < last ? i + 1 : i === last ? 0 : (i + 1) % 16;
    return ops;
  }

  // ---------------------------------------------------------------- the bass's sixteen steps, over the keys
  function laneOf(el, sc, L, K, ed) {
    const bq = sc.bseq, n = bq.steps + 1, Tk = S.bseq.ticks, maxT = Math.max(...Tk), words = P[ID.P_BQ_LEN].texts;
    const box = h("div", { class: "k-steps", role: "group", "aria-label": "BASS's steps" });
    const cells = [];
    for (let i = 0; i < 16; i++) {
      const b = bq.notes[i], cut = b === CUT, hold = b === HOLD, t = Tk[bq.lens[i] % Tk.length], out = i >= n;
      const v = out ? "" : cut ? "CUT" : hold ? "HOLD" : nn(bassKey(b, L));
      const kids = [h("span", { class: "n" }, String(i + 1)), h("span", { class: "v" }, v),
        out ? null : h("span", { class: "len", "aria-hidden": "true" }, h("i", { style: `--w:${r2(Math.max(0.06, Math.log2(t + 1) / Math.log2(maxT + 1)) * 100)}%` }))];
      const cls = "k-step" + (cut ? " rest" : "") + (hold ? " tie" : "") + (out ? " out" : "") + (ed && i === K.step ? " sel" : "");
      const label = `Step ${i + 1}: ${out ? "not played" : (v + ", " + words[bq.lens[i] % words.length])}`;
      const c = ed ? h("button", { type: "button", class: cls, "data-k": "step:" + i, "aria-pressed": String(i === K.step), "aria-label": label,
        onclick: () => { K.step = i; again(el); },
        onkeydown: (e) => {
          const d = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
          if (d == null) return;
          e.preventDefault();
          K.step = (i + d + 16) % 16;
          again(el);
          const t2 = el.querySelector(`[data-k="step:${K.step}"]`);
          if (t2) t2.focus({ preventScroll: true });
        } }, kids)
        : h("div", { class: cls, "aria-label": label }, kids);
      cells.push(c);
      box.append(c);
    }
    return live(box, (f) => cells.forEach((c, i) => c.classList.toggle("now", !!f && f.bq === i)));
  }

  // ---------------------------------------------------------------- the keyboard: C1 .. C8
  const KW = 24, WY = 40, KH = 128, BH = 80, BED = WY + KH + 6, VH = BED + 26;
  const keyX = (() => {                              // a key's middle (a black key's on the line between two whites)
    const xs = {};
    let wi = 0;
    for (let n = LO; n <= HI; n++) { const w = WH.includes(n % 12); xs[n] = w ? wi * KW + KW / 2 : wi * KW; if (w) wi++; }
    return (n) => xs[Math.max(LO, Math.min(HI, n))];
  })();
  function keysOf(el) {
    const sc = el._sc, L = el._L, art = el._art, K = stateOf(art, L), ed = editable(art), track = K.track, latch = K.mode === "latch" || track === "overview";
    const z = zoomOf(K), kw = KW * z, W = 1200 * z, id = "kk" + X.uid(), g = (x) => `url(#${id}-${x})`;   // (zoomed: wider keys, as tall)
    const rad = (x, col, op) => s("radialGradient", { id: `${id}-${x}` }, s("stop", { offset: 0, "stop-color": col, "stop-opacity": op }), s("stop", { offset: 1, "stop-color": col, "stop-opacity": 0 }));
    const svg = s("svg", { class: "k-kb", viewBox: `0 0 ${W} ${VH}` },
      s("defs", null, rad("w", C.white, 0.45), rad("i", C.ice, 0.7), rad("l", C.lamp, 0.6),
        s("linearGradient", { id: `${id}-c`, x1: 0, y1: 1, x2: 0, y2: 0 }, s("stop", { offset: 0, "stop-color": C.white, "stop-opacity": 0.85 }), s("stop", { offset: 1, "stop-color": C.white, "stop-opacity": 0 }))),
      s("rect", { x: 0, y: WY - 0.5, width: W, height: 1, fill: C.slate, opacity: 0.6 }));
    const r = L.raw(ID.P_ROOT) % 12, sb = bitsOf(L), lat = {}, roots = new Map(), all = track === "overview";
    if (shows(K, "drone")) sc.chords.forEach((ch) => { roots.set(ch.root, ch.notes); lat[ch.root] = "root"; ch.notes.forEach((n) => { if (n !== ch.root && !lat[n]) lat[n] = "note"; }); });
    const arpN = new Set(L.arpOn && shows(K, "glint") ? sc.arp.map((a) => a + 12 * L.oct) : []);
    const bqOn = !!(sc.bseq && sc.bseq.on);
    let stepKey = null;                               // (the selected step's key, the bass's sequence)
    if (latch && track === "bass" && bqOn && ed) { const b = sc.bseq.notes[K.step % 16]; if (b !== CUT && b !== HOLD && K.step <= 15) stepKey = bassKey(b, L); }
    const keys = new Map(), wk = [], bk = [], wl = [], bl = [], marks = [], bed = [], labels = [];
    let wi = 0;
    for (let n = LO; n <= HI; n++) {
      const white = WH.includes(n % 12), cx = keyX(n) * z;
      const R = white ? { x: r2(wi * kw + 0.6), y: WY + 1, width: r2(kw - 1.2), height: KH, rx: 2.5 } : { x: r2(cx - 7.5 * z), y: WY + 1, width: r2(15 * z), height: BH, rx: 2 };
      let fl = white ? C.keyW : C.keyB;
      const l = lat[n];
      if (l === "root") fl = C.white;
      else if (l === "note") fl = mix(fl, C.fog, 0.5);
      if (arpN.has(n)) fl = C.ice;
      (white ? wk : bk).push(s("rect", { ...R, class: "k-key", "data-n": n, fill: fl, stroke: white ? "none" : C.keyEdge, "stroke-width": white ? 0 : 1 }));
      if (n === stepKey) (white ? wk : bk).push(s("rect", { ...R, fill: C.lamp, "fill-opacity": 0.22, stroke: C.lamp, "stroke-width": 1.6 }));
      const over = s("rect", { ...R, class: "k-on", opacity: 0 });
      (white ? wl : bl).push(over);
      if (inScale(n, r, sb)) marks.push(s("circle", { cx: r2(cx), cy: white ? WY + KH - 9 : WY + BH - 8, r: (n - r) % 12 === 0 ? 2.2 : 1.4,
        fill: (n - r) % 12 === 0 ? C.ice : white ? C.slate : C.edge, opacity: 0.9 }));
      if (n === sc.bass && shows(K, "bass")) bed.push(s("rect", { x: r2(cx - 7 * z), y: BED, width: r2(14 * z), height: 3, rx: 1.5, fill: C.lamp }));
      if (n % 12 === 0) labels.push(s("text", { x: r2(Math.max(8, Math.min(W - 8, cx))), y: BED + 20, "text-anchor": "middle", fill: C.dim, class: "svgt" }, nn(n)));
      keys.set(n, { x: cx, white, R, over, base: white ? C.keyW : C.keyB });
      if (white) wi++;
    }
    /* what sounds: a light a voice (the drones' eight, GLINT's four, the bass), as the card's keyboard had them */
    const dr = Array.from({ length: 8 }, () => ({ halo: s("circle", { r: 16, fill: g("w"), opacity: 0 }), col: s("rect", { width: 2, fill: g("c"), opacity: 0 }) }));
    const gl = Array.from({ length: 4 }, () => ({ halo: s("circle", { fill: g("i"), opacity: 0 }), star: s("path", { fill: C.ice, opacity: 0 }), core: s("circle", { r: 1.6, fill: C.white, opacity: 0 }) }));
    const bs = { glow: s("ellipse", { rx: r2(26 * z), ry: 8, fill: g("l"), opacity: 0 }), bar: s("rect", { width: r2(14 * z), height: 3, rx: 1.5, fill: C.lamp, opacity: 0 }) };
    const ghost = s("g", { class: "k-ghost" }), tag = s("g", { class: "k-tag" });
    svg.append(...dr.map((d) => [d.halo, d.col]).flat(), ...wk, ...wl, ...bk, ...bl, ...marks, ghost, ...bed, bs.glow, bs.bar, ...labels,
      ...gl.map((x) => [x.halo, x.star, x.core]).flat(), tag);
    el._board.setAttribute("aria-label", `The keys, C1 to C8: ${track.toUpperCase()}${ed && track !== "overview" ? ", " + K.mode : ""}.` +
      (ed && track !== "overview" ? " Arrow keys: a key; Enter: as a click." : ""));
    svg.setAttribute("aria-hidden", "true");
    const dim = ed && !latch && !playing(art);
    svg.classList.toggle("is-dim", dim);
    svg.classList.toggle("is-ed", ed && !all && !(latch && track === "glint" && !L.arpOn));   // (a key does something: the hand)
    /* the key under the pointer (or the keys' cursor): what a click does, ghosted in lamp, a tag over it */
    const say = (n) => {
      ghost.replaceChildren();
      tag.replaceChildren();
      if (n == null || !ed || dim || all || !keys.has(n)) return;
      let outline = [n], text = nn(n), go = false;
      if (track === "drone") {
        const c = chordOf(n, L), own = roots.has(n) ? n : roots.has(c.root) ? c.root : null;
        if (latch && own != null) { outline = roots.get(own); text = nn(own) + " · RELEASE"; go = true; }
        else { outline = c.notes; text = `${nn(n)} · ${L.txt(ID.P_CHORD)} ${L.txt(ID.P_VOICING)}: ${c.notes.map(nn).join(" ")}`; }
        if (latch && !go && roots.size >= S.sizes.SC_CHORDS) text = nn(n) + " · 8 AT MOST";
      } else if (track === "glint") {
        if (n - 12 * L.oct < 1 || (latch && !L.arpOn)) return;
        if (latch && arpN.has(n)) { text += " · RELEASE"; go = true; }
        else if (latch && arpN.size >= S.sizes.SC_ARP) text += " · 16 AT MOST";
      } else if (latch && bqOn) {
        text = `STEP ${K.step % 16 + 1} · ${nn(n)}`;
      } else if (latch && n === sc.bass) { text += " · RELEASE"; go = true; }
      for (const m of outline) {
        const k = keys.get(m);
        if (k) ghost.append(s("rect", { ...k.R, fill: "none", stroke: C.lamp, "stroke-width": 1.6, "stroke-dasharray": go ? "4 3" : null, opacity: m === n ? 1 : 0.75 }));
      }
      const k = keys.get(n), w = text.length * 6.6 + 18, x = Math.max(4, Math.min(W - w - 4, k.x - w / 2));
      tag.append(s("rect", { x: r2(x), y: 6, width: r2(w), height: 22, rx: 5, fill: "#18212c", stroke: C.lamp, "stroke-opacity": 0.6 }),
        s("text", { x: r2(x + w / 2), y: 21, "text-anchor": "middle", fill: C.fog, class: "svgt k-tagt" }, text));
      return text;
    };
    const pressed = s("g");
    ghost.after(pressed);
    svg._press = (ns) => {                              // (the keys held down now)
      pressed.replaceChildren();
      for (const n of ns) {
        const k = keys.get(n);
        if (k) pressed.append(s("rect", { ...k.R, fill: C.lamp, "fill-opacity": 0.35, stroke: C.lamp, "stroke-width": 1.6 }));
      }
    };
    svg._say = say;
    say(K.hover);
    svg._live = (f, ctx) => {
      svg.classList.toggle("is-live", !!f);
      for (const k of keys.values()) if (k.lit) { k.over.setAttribute("opacity", 0); k.lit = false; }
      const light = (k, col, op) => {
        if (k.lit && op <= +k.over.getAttribute("opacity")) return;
        at(k.over, { fill: col, opacity: op });
        k.lit = true;
      };
      const kp = ctx.keep;                                         // (the drones' breath: their meter against its peak)
      kp.kpeak = f ? Math.max((kp.kpeak || 0.05) * 0.995, f.m[0]) : 0.05;
      const breath = f ? Math.max(0.55, Math.min(1, 0.55 + 0.45 * f.m[0] / kp.kpeak)) : 0;
      const fd = shows(K, "drone") && f, fg = shows(K, "glint") && f, fb = shows(K, "bass") && f;   // (the track's own lights)
      dr.forEach((d, i) => {
        const v = fd && f.drone[i], k = v && keys.get(v.n);
        if (!k) { d.halo.setAttribute("opacity", 0); d.col.setAttribute("opacity", 0); return; }
        const op = v.l * breath, ht = 8 + 28 * op;
        at(d.halo, { cx: k.x, cy: WY, opacity: op });
        at(d.col, { x: k.x - 1, y: WY - ht, height: ht, opacity: op });
        light(k, k.white ? C.fog : mix(C.keyB, C.fog, 0.62), (k.white ? 0.6 : 0.8) * op);
      });
      gl.forEach((x, i) => {
        const v = fg && f.glint[i], k = v && keys.get(v.n);
        if (!k) { x.halo.setAttribute("opacity", 0); x.star.setAttribute("opacity", 0); x.core.setAttribute("opacity", 0); return; }
        const y = WY - 12 - (1 - v.l) * 14;
        at(x.halo, { cx: k.x, cy: y, r: 6 + 10 * v.l, opacity: v.l });
        at(x.star, { d: star(k.x, y, 3 + 7 * v.l), opacity: 0.35 + 0.65 * v.l });
        at(x.core, { cx: k.x, cy: y, opacity: v.l });
        light(k, C.ice, 0.25 + 0.6 * v.l);
      });
      const b = fb && f.bass, k = b && keys.get(b.n);
      if (!k) { bs.glow.setAttribute("opacity", 0); bs.bar.setAttribute("opacity", 0); return; }
      at(bs.glow, { cx: k.x, cy: BED + 2, opacity: b.l });
      at(bs.bar, { x: k.x - 7 * z, y: BED, opacity: 0.4 + 0.6 * b.l });
      light(k, mix(k.base, C.lamp, 0.55), 0.8 * b.l);
    };
    return svg;
  }
  /* HOLD: the notes held now, by pointer (the keys': "key"), whichever board they began on; each let go when its pointer
   * is (wherever it ends), when the page loses the focus, when THE KEYS are drawn again (a track, a mode) */
  const HELD = new Map();                            // id -> {t, n, board}
  const holdOff = (id) => {
    const x = HELD.get(id);
    if (!x) return;
    HELD.delete(id);
    if (window.RiverPlayer) RiverPlayer.keys({ op: "hold", t: x.t, n: x.n, down: false });
    if (x.board._draw && x.board._draw._press) x.board._draw._press([...HELD.values()].filter((y) => y.board === x.board).map((y) => y.n));
  };
  const holdsOff = () => [...HELD.keys()].forEach(holdOff);
  function holdOn(id, t, n, board) {
    holdOff(id);
    HELD.set(id, { t, n, board });
    if (window.RiverPlayer) RiverPlayer.keys({ op: "hold", t, n, down: true });
    board._draw._press([...HELD.values()].filter((y) => y.board === board).map((y) => y.n));
  }
  document.addEventListener("pointerup", (e) => holdOff(e.pointerId), true);
  document.addEventListener("pointercancel", (e) => holdOff(e.pointerId), true);
  addEventListener("blur", holdsOff);
  document.addEventListener("visibilitychange", () => { if (document.hidden) holdsOff(); });

  /* the keyboard's pointer and keys (on the board): LATCH, a key let go where it was pressed (a swipe scrolls the keys,
   * nothing else); HOLD, a key sounding while it is down (sliding: the next key; fingers each their own) */
  function wireKeys(el, board) {
    const art = el._art, K = art._keys;
    const say = (n) => { const t = board._draw && board._draw._say(n); return t || ""; };
    const keyAt = (e) => { const t = document.elementFromPoint(e.clientX, e.clientY); return t && t.dataset && t.dataset.n != null && board.contains(t) ? +t.dataset.n : null; };
    const nudge = () => { const b = art.querySelector(".play"); if (!b) return; b.classList.remove("nudge"); void b.offsetWidth; b.classList.add("nudge"); };
    const click = (n) => {
      const sc = el._sc, L = el._L;
      if (K.track === "overview") return;
      if (K.track === "drone") edit(el, [["latch", 0, n]]);
      else if (K.track === "glint") { if (L.arpOn && n - 12 * L.oct >= 1) edit(el, [["latch", 1, n]]); }
      else if (sc.bseq && sc.bseq.on) edit(el, writeStep(sc, K, Math.max(1, Math.min(126, n - bassOct(L)))));
      else edit(el, [["latch", 2, n]]);
    };
    let tap = null;                                  // (LATCH: the press, {id, n, x, y})
    const downs = new Set();                         // (the pointers down: two, a pinch, no tap)
    board.addEventListener("pointermove", (e) => {
      const n = keyAt(e), x = HELD.get(e.pointerId);
      if (x) { if (n != null && n !== x.n) holdOn(e.pointerId, T[K.track], n, board); return; }
      if (tap && tap.id === e.pointerId && Math.hypot(e.clientX - tap.x, e.clientY - tap.y) > 6) tap = null;
      if (n !== K.hover) { K.hover = n; say(n); }
    });
    board.addEventListener("pointerleave", (e) => { if (!HELD.has(e.pointerId) && K.hover != null) { K.hover = null; say(null); } });
    board.addEventListener("pointerdown", (e) => {
      if (e.button !== 0) return;
      if (e.isPrimary) downs.clear();                // (one let go elsewhere: not counted for ever)
      downs.add(e.pointerId);
      if (downs.size > 1) tap = null;                // (a second finger, on a key or not: a pinch)
      const n = keyAt(e);
      if (n == null) return;
      if (e.pointerType === "mouse") e.preventDefault();
      if (K.mode === "latch" || K.track === "overview") { tap = downs.size > 1 ? null : { id: e.pointerId, n, x: e.clientX, y: e.clientY }; return; }
      if (!playing(art)) { nudge(); return; }
      holdOn(e.pointerId, T[K.track], n, board);
      try { board.setPointerCapture(e.pointerId); } catch (_) { /* gone */ }
    });
    board.addEventListener("pointerup", (e) => {
      downs.delete(e.pointerId);
      if (!tap || tap.id !== e.pointerId) return;
      const n = tap.n, touch = e.pointerType !== "mouse";
      tap = null;
      if (touch) K.hover = null;                     // (a finger: no hover to show after)
      if ((keyAt(e) ?? n) === n) click(n);
    });
    board.addEventListener("pointercancel", (e) => { downs.delete(e.pointerId); tap = null; });
    board.addEventListener("focus", () => {           // (the keys' cursor, from the keyboard: the track's notes, else C4)
      if (K.hover != null || !board.matches(":focus-visible")) return;
      const ns = notesOf(el._sc, el._L, K);
      K.hover = ns.length ? Math.min(...ns) : 60;
      el._say.textContent = say(K.hover);
    });
    board.addEventListener("keydown", (e) => {
      const zd = { "+": 1, "=": 1, "-": -1, "_": -1 }[e.key];
      if (zd && !e.metaKey && !e.ctrlKey) { e.preventDefault(); zoomStep(el, zd); return; }
      const d = { ArrowRight: 1, ArrowLeft: -1, ArrowUp: 12, ArrowDown: -12 }[e.key];
      if (d != null) {
        e.preventDefault();
        holdOff("key");
        K.hover = Math.max(LO, Math.min(HI, (K.hover ?? 60) + d));
        el._say.textContent = say(K.hover) || nn(K.hover);
        inSight(board, K.hover);
        return;
      }
      if ((e.key === "Enter" || e.key === " ") && K.hover != null) {
        e.preventDefault();
        if (K.mode === "latch" || K.track === "overview") { if (!e.repeat) click(K.hover); return; }
        if (!playing(art)) { nudge(); return; }
        if (!HELD.has("key")) holdOn("key", T[K.track], K.hover, board);
      }
    });
    board.addEventListener("keyup", (e) => { if (e.key === "Enter" || e.key === " ") holdOff("key"); });
    board.addEventListener("blur", () => holdOff("key"));
  }

  // ---------------------------------------------------------------- ZOOM: the keys wider, scrolled
  const zoomOf = (K) => K.zoom || 1;
  /* key n scrolled into the keys' sight (the keys' cursor, zoomed) */
  function inSight(board, n) {
    const k = board.querySelector(`.k-key[data-n="${n}"]`);
    if (!k) return;
    const rb = board.getBoundingClientRect(), r = k.getBoundingClientRect(), pad = 24;
    if (r.left < rb.left + pad) board.scrollLeft -= rb.left + pad - r.left;
    else if (r.right > rb.right - pad) board.scrollLeft += r.right - (rb.right - pad);
  }
  /* the keys at zoom z, what is at x (a client x: the pointer's, the pinch's; else the keys' middle) kept there */
  function zoomTo(el, z, x, force) {
    const K = el._art._keys, board = el._board, svg = board.querySelector("svg.k-kb");
    z = Math.max(1, Math.min(ZMAX, z));
    if (!svg || (!force && Math.abs(z - zoomOf(K)) < 0.005)) return;
    const rb = board.getBoundingClientRect(), rs = svg.getBoundingClientRect();
    if (x == null) x = rb.left + rb.width / 2;
    const f = (x - rs.left) / rs.width;
    K.zoom = z;
    board.style.setProperty("--kz", z);
    draw(el);
    RC.relive(el._art);                              // (what sounds, on the new keys at once; the keys held down)
    board._draw._press([...HELD.values()].filter((y) => y.board === board).map((y) => y.n));
    const r2s = board.querySelector("svg.k-kb").getBoundingClientRect();   // (an SVG has no offsetLeft)
    board.scrollLeft = board.scrollLeft + r2s.left - rb.left + f * r2s.width - (x - rb.left);
    K.kx = board.scrollLeft;
    if (el._strip) el._strip._sync();
  }
  /* - or +: the next zoom down or up */
  function zoomStep(el, d) {
    const z = zoomOf(el._art._keys);
    zoomTo(el, d > 0 ? ZOOM.find((x) => x > z + 0.01) || ZMAX : [...ZOOM].reverse().find((x) => x < z - 0.01) || 1);
  }
  /* two fingers on the keys (LATCH: in HOLD they play), a trackpad's pinch (Ctrl and the wheel; Safari's gestures).
   * While fingers or a trackpad's gesture last, the keys are stretched, not drawn again (a touch's events go to the key
   * it began on: drawn again, it would be gone), then drawn at their zoom once it ends */
  function wireZoom(el, board) {
    const K = el._art._keys;
    const can = () => !editable(el._art) || K.mode === "latch" || K.track === "overview";
    const holding = () => [...HELD.values()].some((x) => x.board === board);
    let pinch = null, g0 = null, want = null, at = null, touching = false;
    const stretch = (z, x) => {
      const svg = board.querySelector("svg.k-kb");
      if (!svg) return;
      z = Math.max(1, Math.min(ZMAX, z));
      const rb = board.getBoundingClientRect(), rs = svg.getBoundingClientRect(), f = (x - rs.left) / rs.width;
      if (!svg.style.height) { svg.style.height = `${rs.height}px`; svg.setAttribute("preserveAspectRatio", "none"); }
      board.style.setProperty("--kz", z);
      const r2s = svg.getBoundingClientRect();
      board.scrollLeft = board.scrollLeft + r2s.left - rb.left + f * r2s.width - (x - rb.left);
      at = { z, x };
      if (el._strip) el._strip._sync();
    };
    const done = () => { const a = at; at = null; if (a && board.isConnected) zoomTo(el, a.z, a.x, true); };
    const soon = (z, x, now) => {                    // (a frame's worth at once)
      if (!want) requestAnimationFrame(() => { const w = want; want = null; if (w && board.isConnected) (w.now ? zoomTo(el, w.z, w.x) : stretch(w.z, w.x)); });
      want = { z, x, now };
    };
    const span = (t) => Math.hypot(t[0].clientX - t[1].clientX, t[0].clientY - t[1].clientY) || 1;
    const own = (e) => [...e.touches].filter((t) => board.contains(t.target));   // (a thumb resting elsewhere: not one)
    board.addEventListener("touchstart", (e) => {
      touching = true;
      const t = own(e);
      if (t.length === 2 && can()) pinch = { d0: span(t), z0: zoomOf(K) };
    }, { passive: true });
    board.addEventListener("touchmove", (e) => {
      const t = own(e);
      if (!pinch || t.length !== 2) return;
      if (e.cancelable) e.preventDefault();
      soon(pinch.z0 * span(t) / pinch.d0, (t[0].clientX + t[1].clientX) / 2);
    }, { passive: false });
    const lift = (e) => {
      if (!e.touches.length) touching = false;
      if (pinch && own(e).length < 2) { pinch = null; requestAnimationFrame(done); }
    };
    board.addEventListener("touchend", lift);
    board.addEventListener("touchcancel", lift);
    board.addEventListener("wheel", (e) => {          // (each its own event, wherever it is: drawn at once)
      if (!e.ctrlKey) return;
      e.preventDefault();
      const d = e.deltaY * (e.deltaMode === 1 ? 16 : e.deltaMode === 2 ? 400 : 1);   // (lines, pages: in pixels)
      soon((want ? want.z : zoomOf(K)) * Math.exp(-Math.max(-50, Math.min(50, d)) * 0.01), e.clientX, true);
    }, { passive: false });
    board.addEventListener("gesturestart", (e) => { if (pinch || holding() || (touching && !can())) return; e.preventDefault(); g0 = zoomOf(K); });
    board.addEventListener("gesturechange", (e) => { if (g0 == null || pinch) return; e.preventDefault(); soon(g0 * e.scale, e.clientX); });
    board.addEventListener("gestureend", () => { if (g0 == null) return; g0 = null; requestAnimationFrame(done); });
  }
  /* under the keys: where they are (a track, the window on it to drag, the track's notes on it) and ZOOM's - + */
  function stripOf(el, board) {
    const K = el._art._keys, z = zoomOf(K);
    const thumb = h("i", { class: "k-thumb" });
    const ticks = notesOf(el._sc, el._L, K).filter((n) => n >= LO && n <= HI).map((n) => h("b", { style: `left:${r2(keyX(n) / 12)}%` }));
    const track = h("div", { class: "k-scroll", "aria-hidden": "true" }, h("span", { class: "k-rail" }), ...ticks, thumb);
    const zb = (d, t, label) => h("button", { type: "button", class: "k-z", "data-k": "zoom:" + (d > 0 ? "+" : "-"), "aria-label": label,
      "aria-disabled": String(d > 0 ? z >= ZMAX : z <= 1), onclick: (e) => { if (e.currentTarget.getAttribute("aria-disabled") !== "true") zoomStep(el, d); } }, t);
    const strip = h("div", { class: "k-strip t-" + K.track }, track,
      h("div", { class: "k-ctl k-zoom" }, h("span", { class: "k-l", "aria-hidden": "true" }, "ZOOM"),
        h("div", { class: "k-seg", role: "group", "aria-label": "ZOOM" }, zb(-1, "−", "Zoom out"), zb(1, "+", "Zoom in"))));
    strip._sync = () => {
      const sw = board.scrollWidth, cw = board.clientWidth, can = sw > cw + 2;
      strip.classList.toggle("can", can);
      thumb.style.left = `${r2(board.scrollLeft / sw * 100)}%`;
      thumb.style.width = `${r2(Math.min(1, cw / sw) * 100)}%`;
      const zz = zoomOf(K), [m, p] = strip.querySelectorAll(".k-z");   // (aria-disabled: one pressed to its end keeps the focus)
      m.setAttribute("aria-disabled", String(zz <= 1));
      p.setAttribute("aria-disabled", String(zz >= ZMAX));
    };
    let drag = null;                                 // (the window dragged: {id, x0, s0}; the track pressed: there)
    track.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || !strip.classList.contains("can")) return;
      e.preventDefault();
      const r = track.getBoundingClientRect(), sw = board.scrollWidth;
      if (e.target !== thumb) board.scrollLeft = (e.clientX - r.left) / r.width * sw - board.clientWidth / 2;
      drag = { id: e.pointerId, x0: e.clientX, s0: board.scrollLeft, k: sw / r.width };
      try { track.setPointerCapture(e.pointerId); } catch (_) { /* gone */ }
      strip.classList.add("drag");
    });
    track.addEventListener("pointermove", (e) => { if (drag && e.pointerId === drag.id) board.scrollLeft = drag.s0 + (e.clientX - drag.x0) * drag.k; });
    const off = (e) => { if (drag && e.pointerId === drag.id) { drag = null; strip.classList.remove("drag"); } };
    track.addEventListener("pointerup", off);
    track.addEventListener("pointercancel", off);
    el._strip = strip;
    return strip;
  }
  /* the keys at the window's bottom (CSS sticky, a wide and tall enough screen): .is-stuck while they are, apart from
   * the settings under them */
  const DOCK = matchMedia("(min-width: 761px) and (min-height: 560px)");
  let stuckRaf = 0;
  function stuck() {
    stuckRaf = 0;
    let pad = 0;
    for (const el of document.querySelectorAll(".sr-item.open .oc-keys")) {
      let on = false;
      const b = el.getBoundingClientRect();
      el.classList.toggle("no-dock", b.height > innerHeight * 0.6);   // (more than the window can spare: in its place)
      if (DOCK.matches && !el.classList.contains("no-dock")) {
        const set = el._set;
        const next = set && set.isConnected && set.childElementCount ? set.getBoundingClientRect().top : el.parentElement.getBoundingClientRect().bottom;
        on = next - b.bottom > 1 && b.bottom <= innerHeight + 1;
      }
      el.classList.toggle("is-stuck", on);
      if (on) pad = Math.max(pad, b.height);
      else el.style.minHeight = "";
    }
    document.documentElement.style.scrollPaddingBottom = pad ? `${Math.ceil(pad) + 12}px` : "";   // (a focus behind it: brought out,
                                                     // as the browser brings a focus into sight)
  }
  const stuckSoon = () => { if (!stuckRaf) stuckRaf = requestAnimationFrame(stuck); };
  addEventListener("scroll", stuckSoon, { passive: true });
  addEventListener("resize", () => {
    document.querySelectorAll(".oc-keys").forEach((el) => { el.style.minHeight = ""; if (el._strip) el._strip._sync(); });
    stuckSoon();
  });
  if (window.ResizeObserver) new ResizeObserver(stuckSoon).observe(document.body);   // (a section opened, a card closed)

  // ---------------------------------------------------------------- GLINT's sequence as a grid (a piano roll)
  /* a column a step (sixteen, or thirty-two once LENGTH is past sixteen; a beat every four, the steps past LENGTH in
   * the shade), a row a note as it sounds (at GLINT's OCTAVE: the scale's rows lighter, the root's an ice line; the
   * notes and three over and under, two octaves at least), the keys upright at the left; a note a bar on its row, as
   * long as the TIEs after it, brighter (a white line on it) with ACCENT. The step playing lit, the note it plays
   * outlined. One note a step */
  const GX = 72, RH = 13, GT = 18, GW = 1200;
  function windowOf(sc, L) {
    const sh = 12 * L.oct, ns = sc.seq.map((b) => b & 127).filter((n) => n !== REST && n !== TIE).map((n) => n + sh);
    let lo = ns.length ? Math.min(...ns) - 3 : 54 + sh, hi = ns.length ? Math.max(...ns) + 3 : 78 + sh;
    if (hi - lo < 24) { const m = Math.round((hi + lo) / 2); lo = m - 12; hi = m + 12; }
    lo = Math.max(lo, sh + 1, 0);
    hi = Math.min(hi, sh + 126, 127);
    return [lo, hi];
  }
  /* the rows the grid shows: the ones it showed (K.gwin, same OCTAVE), never centred again; a note within two rows of
   * an edge: six more there (those added above: K.gup, for edit() to scroll them away) */
  function rowsOf(sc, L, K) {
    const w = K.gwin, sh = 12 * L.oct;
    if (!w || w.oct !== L.oct) { const [lo, hi] = windowOf(sc, L); K.gwin = { lo, hi, oct: L.oct }; return [lo, hi]; }
    const ns = sc.seq.map((b) => b & 127).filter((x) => x !== REST && x !== TIE).map((x) => x + sh);
    let { lo, hi } = w;
    if (ns.length && Math.min(...ns) < lo + 2) lo = Math.max(Math.min(...ns) - 6, sh + 1, 0);
    if (ns.length && Math.max(...ns) > hi - 2) { const h2 = Math.min(Math.max(...ns) + 6, sh + 126, 127); K.gup = (K.gup || 0) + h2 - hi; hi = h2; }
    K.gwin = { lo, hi, oct: L.oct };
    return [lo, hi];
  }
  /* the sequence's notes as bars: {i (its step), j (the step after its TIEs), note (as it sounds), acc} */
  function barsOf(seq, sh, n) {
    const bars = [];
    for (let i = 0; i < n; i++) {
      const v = seq[i] & 127;
      if (v === REST || v === TIE) continue;
      let j = i + 1;
      while (j < n && (seq[j] & 127) === TIE) j++;
      bars.push({ i, j, note: v + sh, acc: (seq[i] & ACC) !== 0 });
    }
    return bars;
  }
  function gridOf(el) {
    const sc = el._sc, L = el._L, art = el._art, K = stateOf(art, L), ed = editable(art), sh = 12 * L.oct, len = L.raw(ID.P_SQ_LEN) + 1;
    const n = K.g && K.g.cols ? K.g.cols : len <= 16 ? 16 : 32;
    const [lo, hi] = K.g && K.g.win ? K.g.win : rowsOf(sc, L, K);
    const rows = hi - lo + 1, cw = (GW - GX) / n, H = rows * RH + GT + 4;
    const Y = (note) => (hi - note) * RH + GT, Xs = (i) => GX + i * cw;
    const r = L.raw(ID.P_ROOT) % 12, sb = bitsOf(L), id = "gr" + X.uid();
    const svg = s("svg", { class: "k-grid", viewBox: `0 0 ${GW} ${H}`, "aria-hidden": "true" },
      s("defs", null, s("linearGradient", { id: `${id}-n`, x1: 0, y1: 0, x2: 0, y2: 1 }, s("stop", { offset: 0, "stop-color": "#9be0ff" }), s("stop", { offset: 1, "stop-color": C.ice }))));
    for (let note = hi; note >= lo; note--) {                      // the rows, and the keys upright
      const y = Y(note), pc = ((note % 12) + 12) % 12, blk = !WH.includes(pc), inS = inScale(note, r, sb);
      svg.append(s("rect", { x: GX, y, width: GW - GX, height: RH, fill: inS ? "#0f151c" : "#0a0e13" }),
        s("rect", { x: GX, y: y + RH - 0.5, width: GW - GX, height: 0.5, fill: "#151c25" }));
      if ((pc - r + 12) % 12 === 0) svg.append(s("rect", { x: GX, y: y + RH - 1, width: GW - GX, height: 1, fill: C.ice, opacity: 0.22 }));
      svg.append(s("rect", { x: blk ? 22 : 0, y: y + 0.5, width: blk ? 42 : 64, height: RH - 1, rx: 2, fill: blk ? C.keyB : C.keyW, stroke: blk ? C.keyEdge : "none" }));
      if (pc === 0) svg.append(s("text", { x: 6, y: y + RH - 3, fill: C.dim, class: "svgt" }, nn(note)));
    }
    for (let i = 0; i <= n; i++)                                   // the columns: a step each, a beat every four
      svg.append(s("rect", { x: r2(Xs(i) - 0.5), y: GT, width: i % 4 ? 0.5 : 1, height: rows * RH, fill: i % 4 ? "#18202a" : "#26303c" }));
    for (let i = 0; i < n; i++)
      svg.append(s("text", { x: r2(Xs(i) + cw / 2), y: 12, "text-anchor": "middle", fill: i < len ? (i % 4 ? C.slate : C.mist) : "#2b3440", class: "svgt" }, String(i + 1)));
    if (len < n) svg.append(s("rect", { x: r2(Xs(len)), y: GT, width: r2(GW - Xs(len)), height: rows * RH, fill: "#000", "fill-opacity": 0.42 }));
    const lx = Math.min(Xs(Math.min(len, n)), GW - 1);               // LENGTH: its end, a grip to drag
    svg.append(s("rect", { x: r2(lx - 1), y: 14, width: 2, height: rows * RH + 6, fill: C.edge }),
      s("rect", { class: "k-grip", x: r2(Math.max(GX, Math.min(lx - 26, GW - 54))), y: 0, width: 52, height: 15, rx: 3, fill: "#1a222c", stroke: C.edge }),
      s("text", { x: r2(Math.max(GX + 26, Math.min(lx, GW - 28))), y: 10.5, "text-anchor": "middle", fill: C.fog, class: "svgt", style: "font-size:8.5px" }, "LENGTH " + len));
    const head = s("rect", { y: GT, width: r2(cw), height: rows * RH, fill: C.ice, opacity: 0 });
    svg.append(head);
    const bars = barsOf(sc.seq, sh, n);
    for (const b of bars) {                                         // the notes: a bar each, on through its TIEs
      if (b.note < lo || b.note > hi) continue;
      const x = Xs(b.i) + 2, w = (b.j - b.i) * cw - 4, y = Y(b.note) + 1.5, out = b.i >= len;
      b.el = s("rect", { x: r2(x), y: r2(y), width: r2(w), height: RH - 3, rx: 3, fill: out ? C.slate : `url(#${id}-n)`, opacity: out ? 0.5 : b.acc ? 1 : 0.78 });
      svg.append(b.el);
      if (b.acc && !out) svg.append(s("rect", { x: r2(x + 3), y: r2(y + 1.5), width: r2(Math.max(2, w - 6)), height: 1.4, rx: 0.7, fill: C.white, opacity: 0.9 }));
    }
    const ghost = s("g", { class: "k-ghost" });
    svg.append(ghost);
    /* over the picture, where a finger drags: a note (up or down: another note; its end: longer), LENGTH's grip (no
     * scroll there; the browser takes no touch-action from an SVG's own shapes). The rest scrolls under a finger */
    const pc = (v, of) => `${r2(v / of * 100)}%`, hits = h("div", { class: "k-hits", "aria-hidden": "true" });
    const hitAt = (x, y, w, ht, cls) => hits.append(h("i", { class: cls || null, style: `left:${pc(x, GW)};top:${pc(y, H)};width:${pc(Math.max(0, w), GW)};height:${pc(ht, H)}` }));
    if (ed) {
      const zone = Math.min(26, cw * 0.45);
      for (const b of bars) if (b.el) { hitAt(Xs(b.i), Y(b.note), (b.j - b.i) * cw - zone, RH, "k-body"); hitAt(Xs(b.j) - zone, Y(b.note), zone, RH); }
      hitAt(Math.max(GX, Math.min(lx - 30, GW - 60)), 0, 60, 16);
    }
    const wrap = h("div", { class: "k-gw" }, svg, hits);
    const where = (e) => {                                          // (the pointer, in the grid's own units)
      const rc = svg.getBoundingClientRect(), x = (e.clientX - rc.left) * GW / rc.width, y = (e.clientY - rc.top) * H / rc.height;
      const i = Math.floor((x - GX) / cw), note = hi - Math.floor((y - GT) / RH);
      return { x, y, i: i >= 0 && i < n ? i : -1, note: note >= lo && note <= hi ? note : null };
    };
    const barAt = (i) => bars.find((b) => i >= b.i && i < b.j);
    /* hovered: the cell (a note a click puts there, the upright key too, a tag), a note (outlined, its end's grip) */
    wrap._say = (c) => {
      ghost.replaceChildren();
      if (!c || !ed) { wrap.style.cursor = ""; return; }
      if (c.grip) { wrap.style.cursor = "ew-resize"; return; }
      if (c.i < 0 || c.note == null) { wrap.style.cursor = ""; return; }
      const b = barAt(c.i);
      if (b && b.note === c.note) {
        const x = Xs(b.i) + 2, w = (b.j - b.i) * cw - 4, y = Y(b.note) + 1.5;
        ghost.append(s("rect", { x: r2(x - 1.5), y: r2(y - 1.5), width: r2(w + 3), height: RH, rx: 4, fill: "none", stroke: C.lamp, "stroke-width": 1.6 }),
          s("rect", { x: r2(x + w - 4), y: r2(y + 1), width: 3, height: RH - 5, rx: 1.5, fill: C.lamp }));
        wrap.style.cursor = c.end ? "ew-resize" : "pointer";
        return;
      }
      wrap.style.cursor = "pointer";
      const x = Xs(c.i) + 2, y = Y(c.note) + 1.5;
      ghost.append(s("rect", { x: r2(x), y: r2(y), width: r2(cw - 4), height: RH - 3, rx: 3, fill: C.lamp, "fill-opacity": 0.18, stroke: C.lamp, "stroke-width": 1.4 }),
        s("rect", { x: 0, y: Y(c.note) + 0.5, width: 64, height: RH - 1, rx: 2, fill: C.lamp, opacity: 0.45 }));
      const t = `STEP ${c.i + 1} · ${nn(c.note)}`, tw = t.length * 6.4 + 16, tx = x + cw + 6 + tw > GW - 4 ? x - tw - 6 : x + cw + 2;
      ghost.append(s("rect", { x: r2(tx), y: r2(y - 4), width: r2(tw), height: 20, rx: 5, fill: "#18212c", stroke: C.lamp, "stroke-opacity": 0.6 }),
        s("text", { x: r2(tx + tw / 2), y: r2(y + 10), "text-anchor": "middle", fill: C.fog, class: "svgt", style: "font-size:10.5px" }, t));
      return t;
    };
    wrap._live = (f) => {
      const pos = f ? f.seq : -1;
      if (pos < 0 || pos >= n) { head.setAttribute("opacity", 0); bars.forEach((b) => b.el && b.el.setAttribute("stroke-width", 0)); return; }
      at(head, { x: Xs(pos), opacity: 0.16 });
      bars.forEach((b) => { if (b.el) { const on = pos >= b.i && pos < b.j; b.el.setAttribute("stroke", C.white); b.el.setAttribute("stroke-width", on ? 1.2 : 0); } });
    };
    const say = sc.seq.slice(0, len).map((b) => { const v = b & 127; return v === REST ? "REST" : v === TIE ? "TIE" : nn(v + sh) + (b & ACC ? " with ACCENT" : ""); });
    el._board.setAttribute("aria-label", `GLINT's sequence, a grid: ${len} steps play, ${say.join(", ")}.` +
      (ed ? " Arrow keys: a step and a note; Enter: a note there, or it goes; Shift-Enter: its ACCENT; on a note, Shift and the arrows: longer, shorter, another note." : ""));
    wrap._at = { where, barAt, bars, n, len, cw, lo, hi, Xs };
    if (K.g && K.g.hover) wrap._say(K.g.hover);
    return wrap;
  }
  /* the grid's gestures: a cell pressed and let go, a note there (a mouse dragging on from it: longer; a finger's swipe
   * scrolls, nothing placed); a note's end dragged, its TIEs; a note dragged up or down, another note; a note clicked,
   * it goes (Shift: its ACCENT); LENGTH's grip dragged (past the grid's end: 17, the grid's 32 steps once let go). A
   * gesture is one step back; one
   * the browser takes back (a finger scrolling after all) leaves the sequence as it was */
  function wireGrid(el, board) {
    const art = el._art, K = art._keys;
    const A = () => board._draw._at, sh = () => 12 * el._L.oct, say = (c) => (board._draw && board._draw._say(c)) || "";
    const seq = () => el._sc.seq;
    const jOf = (i) => { const s0 = seq(), n = A().n; let j = i + 1; while (j < n && (s0[j] & 127) === TIE) j++; return j; };
    const stepOps = (i, note, acc) => {               // (step i: its NOTE, and its ACCENT when given)
      const o = [["p", ID.P_SQ_STEP, i], ["p", ID.P_SQ_NOTE, note]];
      if (acc != null) o.push(["p", ID.P_SQ_ACC, acc]);
      return o;
    };
    const lenOps = (j) => (j > el._L.raw(ID.P_SQ_LEN) + 1 ? [["p", ID.P_SQ_LEN, j - 1]] : []);   // (LENGTH taking the steps in)
    /* a note put on step i (note as it sounds): a step with a note of its own, that note moved (its TIEs, its ACCENT
     * kept: one note a step); a REST, a note; a TIE, a note, the TIEs after it let go (the note before ends there) ->
     * {ops, j: the step after it} */
    const place = (i, note) => {
      const s0 = seq(), v = s0[i] & 127, x = Math.max(1, Math.min(126, note - sh()));
      if (v !== REST && v !== TIE) return { ops: stepOps(i, x), j: jOf(i) };
      const ops = stepOps(i, x, 0);
      if (v === TIE) for (let k = i + 1; k < jOf(i); k++) ops.push(...stepOps(k, REST, 0));
      return { ops: [...ops, ...lenOps(i + 1)], j: i + 1 };
    };
    const remove = (b) => { const ops = []; for (let k = b.i; k < b.j; k++) ops.push(...stepOps(k, REST, 0)); return ops; };
    const accent = (b) => [["p", ID.P_SQ_STEP, b.i], ["p", ID.P_SQ_ACC, seq()[b.i] & ACC ? 0 : 1]];
    /* a note's TIEs to step j (the step after its last): on, up to the next note; back, RESTs */
    const tieTo = (b, j) => {
      const s0 = seq(), a = A();
      let lim = a.n;
      for (let k = b.i + 1; k < a.n; k++) { const v = s0[k] & 127; if (v !== REST && v !== TIE) { lim = k; break; } }
      j = Math.max(b.i + 1, Math.min(lim, j));
      if (j === b.j) return null;
      const ops = [];
      for (let k = b.i + 1; k < j; k++) if ((s0[k] & 127) !== TIE) ops.push(...stepOps(k, TIE, 0));
      for (let k = j; k < b.j; k++) ops.push(...stepOps(k, REST, 0));
      b.j = j;
      return [...ops, ...lenOps(j)];
    };
    const hit = (e) => {
      const a = A(), w = a.where(e);
      if (w.y < 16 && Math.abs(w.x - a.Xs(Math.min(a.len, a.n))) < 30) return { ...w, grip: true };
      const b = w.i >= 0 && a.barAt(w.i), zone = Math.min(e.pointerType === "touch" ? 26 : 12, a.cw * 0.45);
      return { ...w, end: !!(b && b.note === w.note && w.x > a.Xs(b.j) - zone) };
    };
    let g = null;
    const mv = (ops) => { g.edited = true; edit(el, ops, "move"); };   // (under way: the gesture holds a change)
    board.addEventListener("pointermove", (e) => {
      if (!g) { if (e.pointerType === "mouse") { const c = hit(e); K.g = { hover: c }; say(c); } return; }
      if (e.pointerId !== g.id) return;
      const a = A(), w = a.where(e);
      if (g.kind === "place") {                      // (a mouse dragging on from the cell: the note, then longer)
        if (g.touch || Math.hypot(e.clientX - g.x0, e.clientY - g.y0) < 5) return;
        const p = place(g.c.i, g.c.note);
        g = { kind: "tie", id: g.id, bar: { i: g.c.i, j: p.j, note: g.c.note }, place: true };
        mv(p.ops);
      }
      if (g.kind === "len") {
        let j = Math.max(1, Math.min(a.n, Math.round((w.x - GX) / a.cw)));
        if (a.n === 16 && w.x > GW + 8) j = 17;      // (on past the grid's end: its 32 steps once let go)
        if (j !== a.len) mv([["p", ID.P_SQ_LEN, j - 1]]);
      } else if (g.kind === "tie") {
        g.far = g.far || (g.x0 != null && Math.hypot(e.clientX - g.x0, e.clientY - g.y0) > 6);
        const ops = tieTo(g.bar, Math.floor((w.x - GX) / a.cw) + 1);
        if (ops && ops.length) mv(ops);
      } else if (g.kind === "bar") {
        g.far = g.far || Math.hypot(e.clientX - g.x0, e.clientY - g.y0) > 6;
        if (w.note == null || w.note === g.bar.note) return;
        g.moved = true;
        g.bar.note = w.note;
        mv(stepOps(g.bar.i, Math.max(1, Math.min(126, w.note - sh()))));
      }
    });
    board.addEventListener("pointerleave", (e) => { if (!g && e.pointerType === "mouse") { K.g = null; say(null); } });
    board.addEventListener("pointerdown", (e) => {
      if (e.button !== 0) return;
      if (g) end({ pointerId: g.id }, true);         // (one left over: given up)
      const c = hit(e), a = A();
      if (!c.grip && (c.i < 0 || c.note == null)) return;
      const b = !c.grip && a.barAt(c.i), onBar = !!(b && b.note === c.note);
      if (e.pointerType === "mouse") e.preventDefault();   // (a finger: the zones' touch-action decides what scrolls)
      K.g = { win: [a.lo, a.hi], cols: a.n, hover: null };   // (the grid holds still while a gesture lasts)
      if (c.grip) g = { kind: "len" };
      else if (onBar) g = { kind: c.end ? "tie" : "bar", bar: { ...b }, shift: e.shiftKey, moved: false, x0: e.clientX, y0: e.clientY };
      else g = { kind: "place", c, x0: e.clientX, y0: e.clientY, touch: e.pointerType === "touch" };
      g.id = e.pointerId;
      try { board.setPointerCapture(e.pointerId); } catch (_) { /* gone */ }
      say(null);
    });
    const end = (e, cancel) => {
      if (!g || e.pointerId !== g.id) return;
      const was = g;
      g = null;
      K.g = null;
      if (was.kind === "place") { if (!cancel) edit(el, place(was.c.i, was.c.note).ops, "one"); return; }
      if (!cancel && (was.kind === "bar" || (was.kind === "tie" && !was.place)) && !was.moved && !was.far && !was.edited) {
        edit(el, was.shift ? accent(was.bar) : remove(was.bar), "one");   // (a note tapped, its end too: it goes)
        return;
      }
      if (was.edited) edit(el, [], cancel ? "cancel" : "end");   // (nothing changed: nothing drawn again, a scroll goes on)
    };
    board.addEventListener("pointerup", (e) => end(e, false));
    board.addEventListener("pointercancel", (e) => end(e, true));
    board.addEventListener("lostpointercapture", (e) => end(e, true));
    board.addEventListener("focus", () => {
      if (K.gc || !board.matches(":focus-visible")) return;
      const a = A(), b = a.bars.find((x) => x.note >= a.lo && x.note <= a.hi);
      K.gc = b ? { i: b.i, note: b.note } : { i: 0, note: Math.round((a.lo + a.hi) / 2) };
      K.g = { hover: { ...K.gc } };
      el._say.textContent = say(K.g.hover);
    });
    board.addEventListener("keydown", (e) => {
      const a = A(), c = K.gc || (K.gc = { i: 0, note: Math.round((a.lo + a.hi) / 2) });
      const d = { ArrowRight: [1, 0], ArrowLeft: [-1, 0], ArrowUp: [0, 1], ArrowDown: [0, -1] }[e.key];
      const b = a.barAt(c.i), on = b && b.note === c.note;
      if (d && e.shiftKey && on) {                   // (on a note: Shift and the arrows, longer, shorter, another note)
        e.preventDefault();
        if (d[0]) { const ops = tieTo({ ...b }, b.j + d[0]); if (ops && ops.length) edit(el, ops); }
        else {
          const n2 = Math.max(a.lo, Math.min(a.hi, b.note + d[1]));
          if (n2 !== b.note) { c.note = n2; edit(el, stepOps(b.i, Math.max(1, Math.min(126, n2 - sh())))); }
        }
        return;
      }
      if (d) {
        e.preventDefault();
        c.i = Math.max(0, Math.min(a.n - 1, c.i + d[0]));
        c.note = Math.max(a.lo, Math.min(a.hi, c.note + d[1]));
        K.g = { hover: { ...c } };
        const b2 = a.barAt(c.i);
        el._say.textContent = b2 && b2.note === c.note ? `STEP ${c.i + 1} · ${nn(c.note)}, a note` : say(K.g.hover);
        return;
      }
      if (e.key !== "Enter" && e.key !== " ") return;
      e.preventDefault();
      if (on) edit(el, e.shiftKey ? accent(b) : remove(b));
      else edit(el, place(c.i, c.note).ops);
    });
  }

  RC.onSound((k) => {
    const c = RC.card(k), el = c && c.art.querySelector(".oc-keys");
    if (el && el._art._keys && el._art._keys.mode === "hold") again(el);
  });

  return { panel, values, chordOf, hooks, TRACKS };
})();
