/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's editor: an opened card's settings turned as the FM-1 turns them, the sound following, then SAVE AS (a
 * new shard in Your Private River) and, on one's own shard, SAVE (its sound replaced). No edit mode: every card a page
 * makes editable (row()'s o.edit) turns at once.
 *   a setting      dragged left or right (a vertical drag still scrolls), or the arrow keys (Shift, PageUp/PageDown:
 *                  by ten; Home, End); a choice also opens its list (a click, Enter, Space)
 *   each change    through FRASIL's own engine on this page, silent (app.wasm: web_param, as a MIDI CC learned on the
 *                  setting sets it: a preset's sound, CUSTOM once a drone's setting is turned; web_scene_edited, the
 *                  shard as STORE would keep it now), and to the one playing (RiverPlayer.param). Nothing changes in the
 *                  shard until it is saved
 *   a draft        a shard's (by its id: the same draft wherever it shows; else the row's key): the shard as it was, as
 *                  it is, the steps back (UNDO); kept in this tab (sessionStorage), so a list drawn again, or a page
 *                  left (to sign in) and come back to, still has it. The shard renamed meanwhile: the draft follows;
 *                  its sound changed meanwhile: the draft goes (said)
 *   what it plays  THE KEYS (river-keys.js): chords latched, the arpeggio's notes, the bass held, the sequences' steps,
 *                  through the same engine (web_latch, web_clear, web_param on the steps), each gesture a step back;
 *                  the sound playing follows (RiverPlayer.keys, .param), and UNDO of what it plays takes the shard again
 *   the card       a changed setting lamp (the hand), a dot before its name, a tick where it was; a section holding one,
 *                  a dot (THE KEYS too: what it plays, or HARMONY, changed); one line under its description (EDITED);
 *                  its head's actions the editor's (UNDO, REVERT, SAVE, SAVE AS); closed, EDITED · NOT SAVED under its
 *                  name
 * row()'s o.edit: {saveAs({bytes, name, description}) -> Promise (an element to focus after, or nothing; null, or
 *   canSave() false: signing in first), save(bytes) -> Promise (one's own: its sound replaced), shared (one's own,
 *   shared: SAVE asks first), description (one's own: SAVE AS's to start from), from {name, author, first} (someone
 *   else's: credited, CC BY 4.0: first, its first author, whom the service credits when there is one)} */
"use strict";
window.RiverEdit = (() => {
  const RC = RiverCards, { h, I, toast, confirm } = RC;
  const S = FB.S, P = S.params, ID = S.ids, NS = S.sizes.NSOUND, NL = S.scene_t.name_len, V = S.scene_t.v;
  const NAME = new RegExp(`^[A-Z0-9 .#+-]{1,${NL}}$`), b64 = RiverAPI.b64, unb64 = RiverAPI.unb64;
  const store = {
    get: (k) => { try { return sessionStorage.getItem(k); } catch (_) { return null; } },
    set: (k, v) => { try { if (v == null) sessionStorage.removeItem(k); else sessionStorage.setItem(k, v); } catch (_) { /* this page only */ } },
  };
  const same = (a, b) => !!a && !!b && a.length === b.length && a.every((v, i) => v === b[i]);
  const sound = (b) => b.subarray(NL);                          // (a shard's bytes but its name)
  const raw = (b) => b.subarray(V, V + NS);                     // (its sound as kept: a drone's CUSTOM, its preset's number)
  const plays = (b) => b.subarray(S.scene_t.nch, S.scene_t.tempo);   // (what it plays: chords, arpeggio, bass, sequences)
  const word = (p, x) => { const d = P[p]; return d.texts[Math.max(0, Math.min(x, d.max))] ?? String(x); };
  const NUM = ["No", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten"];

  // ================================================================ the model: FRASIL's engine on the page, silent
  let ex = null, asked = null, holds = null, preFailed = false;   // holds: the shard the model has, as it is now
  function model() {
    if (ex) return Promise.resolve(ex);
    if (!asked) asked = (async () => {
      const m = document.querySelector('meta[name="frasil-engine"]'), v = m && m.content ? "?v=" + encodeURIComponent(m.content) : "";
      const r = await fetch("/emu/app.wasm" + v);
      if (!r.ok) throw new Error("missing");
      const stub = new Proxy({ clock_time_get: () => 0, fd_write: () => 0, proc_exit: () => {} }, { get: (t, k) => (k in t ? t[k] : () => 0) });
      const env = new Proxy({}, { get: () => () => 0 });
      const { instance } = await WebAssembly.instantiate(await r.arrayBuffer(), { wasi_snapshot_preview1: stub, env });
      const x = instance.exports;
      if (!x.web_param || !x.web_scene_edited || !x.web_latch || !x.web_scene_take) throw new Error("missing");
      if (x._initialize) x._initialize();
      x.web_master(0);
      x.web_boot();
      ex = x;
      return x;
    })().catch((e) => { asked = null; throw e; });
    return asked;
  }
  /* a shard's bytes with ops done, as the FM-1 does them: ["p", p, x] a setting set (the sequences' steps too),
   * ["latch", t, n], ["clear", t] (THE KEYS) -> {bytes, res: each op's answer (web_latch's: 1, 0, -1 refused)} */
  function apply(bytes, ops) {
    const n = ex.web_scene_size();
    if (!same(holds, bytes)) {
      new Uint8Array(ex.memory.buffer, ex.web_scene_buf(), n).set(bytes.subarray(0, n));
      ex.web_scene_take();
    }
    const res = ops.map(([k, a, b]) => (k === "p" ? (ex.web_param(a, b), 0) : k === "latch" ? ex.web_latch(a, b) : k === "clear" ? (ex.web_clear(a), 0) : -1));
    holds = Uint8Array.from(new Uint8Array(ex.memory.buffer, ex.web_scene_edited(), n));
    return { bytes: holds, res };
  }
  /* the settings as the card shows them (THE KEYS: the bass's sequence's at their own numbers, its selected step's) */
  const vals = (sc, art) => (window.RiverKeys ? RiverKeys.values(sc, art) : sc.v);
  /* setting p to x, as ops (STEP LENGTH: the step THE KEYS have selected, first) */
  const opsFor = (art, p, x) => (p === ID.P_BQ_LEN ? [["p", ID.P_BQ_STEP, (art._keys ? art._keys.step : 0) % 16], ["p", p, x]] : [["p", p, x]]);
  /* a card's scene from its bytes (as the model reads a package's: the drone's preset applied), the row's facts kept */
  function sceneOf(bytes, like) {
    const pack = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: S.magic.shard, firmware: FRASIL_VERSION,
                                              scenes: [{ name: like.name, from: (like.slot ?? 0) + 1, data: b64(bytes) }] }));
    return { ...pack.scenes[0], slot: like.slot, state: like.state, shard: like.shard, fav: like.fav };
  }

  // ================================================================ the drafts
  const drafts = new Map();                     // a draft's key -> {b0, cur, back: [bytes], sc0, sc, mark}
  const KEY = (k) => "river-edit:" + k;
  const keyOf = (k, sc0) => (sc0.shard && sc0.shard.id ? "shard:" + sc0.shard.id : k);
  /* a draft's bytes under another name (the shard renamed since: its sound kept) */
  const named = (b, as) => Uint8Array.from([...as.subarray(0, NL), ...sound(b)]);
  function draftOf(dk, sc0) {
    let d = drafts.get(dk);
    if (!d) {
      const t = store.get(KEY(dk));
      if (t) {
        try { const j = JSON.parse(t); d = { b0: unb64(j.b0), cur: unb64(j.cur), back: (j.back || []).map(unb64) }; drafts.set(dk, d); }
        catch (_) { store.set(KEY(dk), null); }
      }
    }
    if (!d) return null;
    if (!same(d.b0, sc0.bytes)) {
      if (!same(sound(d.b0), sound(sc0.bytes))) {          // (its sound changed since, elsewhere: the draft goes)
        drop(dk);
        toast(sc0.name, " changed since: the changes made to it here are gone.");
        return null;
      }
      d.b0 = sc0.bytes;                                     // (renamed since: the draft under its name)
      d.cur = named(d.cur, sc0.bytes);
      d.back = d.back.map((b) => named(b, sc0.bytes));
      d.sc = null;
      keep(dk, d);
    }
    if (same(d.cur, d.b0)) { drop(dk); return null; }
    d.sc0 = sc0;
    if (!d.sc || !same(d.sc.bytes, d.cur)) d.sc = sceneOf(d.cur, sc0);
    return d;
  }
  function keep(dk, d) {
    store.set(KEY(dk), d ? JSON.stringify({ b0: b64(d.b0), cur: b64(d.cur), back: d.back.slice(-24).map(b64) }) : null);
  }
  function drop(dk) { drafts.delete(dk); keep(dk, null); }
  /* the settings changed: as shown, or as kept (a drone's CUSTOM under a preset) */
  const changed = (d) => {
    const a = raw(d.cur), b = raw(d.b0);
    let n = 0;
    for (let p = 0; p < NS; p++) if (d.sc.v[p] !== d.sc0.v[p] || a[p] !== b[p]) n++;
    return n;
  };
  const notesChanged = (d) => !same(plays(d.cur), plays(d.b0));
  /* what changed, in words: its notes, n settings */
  function what(d, cap) {
    const n = changed(d), notes = notesChanged(d), num = (k) => NUM[k] || String(k);
    const set = n === 1 ? "one setting" : n ? `${num(n).toLowerCase()} settings` : "";
    const t = notes ? (set ? `its notes and ${set}` : "its notes") : set || "nothing";
    return cap ? t[0].toUpperCase() + t.slice(1) : t;
  }

  // ================================================================ the River's rows (river-cards' hooks)
  const editable = (sc, o) => !!(o.edit && sc.current && sc.bytes);
  RC.hooks.scene = (k, sc0, o) => {
    if (!editable(sc0, o)) return sc0;
    const d = draftOf(keyOf(k, sc0), sc0);
    return d ? d.sc : sc0;
  };
  RC.hooks.row = (art, sc0, o) => {
    if (!editable(sc0, o)) return;
    if (!preFailed) model().catch(() => { preFailed = true; });   // (ready before a card is opened: app.wasm, once, as the list shows)
    art.classList.add("is-editable");
    art._edit = { o, sc0, dk: keyOf(art._key, sc0), acts: null };   // (acts: the page's, set aside while the editor's show)
    paint(art);
  };
  RC.hooks.built = (art) => {
    if (!art._edit) return;
    model().catch(() => {});                     // (ready by the first turn)
    paint(art);
  };
  const draft = (art) => drafts.get(art._edit.dk);

  /* a card as its draft has it: its head's actions, its mark, its line, its settings */
  function paint(art) {
    const E = art._edit, d = draft(art), name = E.sc0.name;
    art.classList.toggle("is-edited", !!d);
    const acts = art.querySelector(".acts");
    if (d) {
      if (!E.acts) E.acts = [...acts.childNodes];
      acts.replaceChildren(...actions(art, d));
    } else if (E.acts) {
      acts.replaceChildren(...E.acts);
      E.acts = null;
    }
    const t = art.querySelector(".nm-t");
    let mk = t.querySelector(".nm-ed");
    if (d && !mk) t.append(mk = h("span", { class: "nm-ed" }, "EDITED · NOT SAVED"));
    if (!d && mk) mk.remove();
    const inner = art.querySelector(".oc-in"), bodyEl = art.querySelector(".oc-body");
    let band = inner.querySelector(".ed-band");
    if (d && bodyEl) {
      if (!band) inner.insertBefore(band = h("div", { class: "ed-band", role: "status" }, h("b", null, "EDITED"), h("span")), bodyEl);
      RC.fill(band.lastChild, h("em", null, what(d, true) + " changed"), `. The sound follows; ${name} stays as it was until you save.`);
    } else if (band) band.remove();
    if (!bodyEl) return;
    const v = vals(d ? d.sc : E.sc0, art), v0 = vals(E.sc0, art);
    bodyEl.querySelectorAll(".cell[data-p]").forEach((c) => mark(c, +c.dataset.p, v, v0));
    const touched = new Set();
    for (let p = 0; p < NS; p++) if (v[p] !== v0[p]) touched.add(p);
    const kt = bodyEl.querySelector(".k-title");                  // (THE KEYS: what it plays, or HARMONY, changed)
    if (kt) {
      const on = !!d && (notesChanged(d) || (SECS.get("HARMONY") || []).some((p) => touched.has(p)));
      let dot = kt.querySelector(".ed-dot");
      if (on && !dot) kt.append(h("span", { class: "ed-dot" }, h("span", { class: "sr-only" }, ", changed")));
      if (!on && dot) dot.remove();
    }
    bodyEl.querySelectorAll("details.sec").forEach((sec) => {
      const nm = sec.querySelector(".sec-name"), on = SECS.get(nm.firstChild.textContent)?.some((p) => touched.has(p));
      let dot = nm.querySelector(".ed-dot");
      if (on && !dot) nm.append(dot = h("span", { class: "ed-dot" }, h("span", { class: "sr-only" }, ", changed")));
      if (!on && dot) dot.remove();
    });
  }
  /* each section's settings, by its name as the card writes it (a page's: its section, as river-cards groups them) */
  const SECS = (() => {
    const by = new Map(), SEC = { EDIT: "DRONE", ENV: "DRONE", LFO: "DRONE", ARP: "GLINT", SEQ: "BASS", FX: "FX", SEL: "HARMONY", HOME: "MIXER", GLO: "MASTERING" };
    for (const pg of S.pages) {
      const nm = (pg.name.match(/^LFO \d+/) || [])[0] || SEC[pg.button];
      if (!nm) continue;
      if (!by.has(nm)) by.set(nm, []);
      by.get(nm).push(...pg.p.filter((p) => p >= 0 && p < NS));
    }
    return by;
  })();
  /* a setting's cell: a slider (its name, its value in the screen's words), lamp once changed, a tick where it was */
  function mark(c, p, v, v0) {
    const d = P[p], was = v[p] !== v0[p];
    c.tabIndex = 0;
    c.setAttribute("role", "slider");
    c.setAttribute("aria-label", d.name);
    c.setAttribute("aria-valuemin", "0");
    c.setAttribute("aria-valuemax", String(d.max));
    c.setAttribute("aria-valuenow", String(v[p]));
    c.setAttribute("aria-valuetext", word(p, v[p]) + (was ? `, was ${word(p, v0[p])}` : ""));
    c.classList.toggle("is-choice", d.kind !== "gauge");
    c.classList.toggle("is-edited", was);
    c.title = was ? `Was ${word(p, v0[p])}` : "";
    const g = c.querySelector(".gauge");
    let t = g && g.querySelector(".was");
    if (g && was) {
      if (!t) g.append(t = h("i", { class: "was", "aria-hidden": "true" }));
      t.style.left = `${Math.max(1, Math.min(88, Math.round(v0[p] / Math.max(1, d.max) * 88))) / 88 * 100}%`;
    } else if (t) t.remove();
  }
  function actions(art, d) {
    const E = art._edit, o = E.o.edit, nm = E.sc0.name;
    return [
      h("button", { type: "button", class: "a", "data-act": "undo", disabled: !d.back.length, "aria-label": `Undo: ${nm}`, onclick: () => undoOne(art) }, undoIcon(), "Undo"),
      h("button", { type: "button", class: "a a-txt", "data-act": "revert", "aria-label": `Revert: ${nm}`, onclick: () => revert(art) }, "Revert"),
      o.save ? h("button", { type: "button", class: "a", "data-act": "save", "aria-label": `Save: ${nm}`, onclick: () => saveOwn(art) }, "Save") : null,
      h("button", { type: "button", class: "a a-ice", "data-act": "save-as", "aria-label": `Save as: ${nm}`, onclick: () => saveAs(art) }, "Save as…"),
    ].filter(Boolean);
  }
  const undoIcon = () => RC.s("svg", { viewBox: "0 0 16 16", width: 15, height: 15, "aria-hidden": "true", fill: "none", stroke: "currentColor", "stroke-width": 1.4,
    "stroke-linecap": "round", "stroke-linejoin": "round" }, RC.s("path", { d: "M5.5 3.5L2.5 6.5l3 3M2.8 6.5h6.7a3.8 3.8 0 0 1 0 7.6H7" }));

  // ================================================================ a setting turned
  /* ops on the card art's shard: the model, the draft (where this gesture began kept: UNDO's step), the sound if it
   * plays (a latch the model refused, not) -> {d, res}, d null if nothing changed */
  function change(art, ops) {
    const E = art._edit;
    let d = draft(art);
    const base = d ? d.cur : E.sc0.bytes, r = apply(base, ops);
    if (same(r.bytes, base)) return { d: null, res: r.res };
    if (!d) { d = { b0: E.sc0.bytes, cur: base, back: [], sc0: E.sc0 }; drafts.set(E.dk, d); }
    if (!d.mark) d.mark = base;
    d.cur = r.bytes;
    d.sc = sceneOf(r.bytes, E.sc0);
    const pl = window.RiverPlayer;
    if (pl && pl.id === art._key && pl.state !== "idle")
      ops.forEach(([k, a, b], i) => {
        if (k === "p") pl.param(a, b);
        else if (r.res[i] !== -1) pl.keys(k === "latch" ? { op: "latch", t: a, n: b } : { op: "clear", t: a });
      });
    return { d, res: r.res };
  }
  /* p to x on the card art: the model, the sound if it plays, its cells drawn again in place (a setting can show twice:
   * in THE KEYS and in its section; the rest at the gesture's end: settle) */
  function set(art, p, x) {
    const E = art._edit;
    if (!ex || E.busy) return false;
    const { d } = change(art, opsFor(art, p, x));
    if (!d) return false;
    const v = vals(d.sc, art), v0 = vals(E.sc0, art);
    art.querySelectorAll(`.oc-body .cell[data-p="${p}"]`).forEach((c) => { RC.refreshCell(c, p, v); mark(c, p, v, v0); });
    return true;
  }
  /* THE KEYS (river-keys.js): ops on the card's shard, a gesture whole ("one": a step back kept, the card drawn
   * again), under way ("move": the scene it makes, for THE KEYS to draw), ended ("end"), or taken back ("cancel": a
   * finger the browser took for a scroll after all; the shard as the gesture found it) -> {sc, res} | null */
  function act(art, ops, phase = "one") {
    const E = art._edit;
    if (!E || E.busy) return null;
    if (!ex) {                                    // (a gesture whole, before the engine came: done once it is)
      model().then(() => { if (phase === "one" && ops.length && art.isConnected) act(art, ops, phase); },
        () => toast("The editor needs FRASIL’s engine, which isn’t on this site yet. Try again in a moment."));
      return null;
    }
    if (keyed && keyed.art === art) {              // (keys stepping a setting: their step back first; mid-gesture, the
      if (phase === "move") { clearTimeout(keyed.t); keyed = null; seal(art); }   // card isn't drawn again under it)
      else flush(art);
    }
    if (phase === "cancel") {
      const d = draft(art);
      if (d && d.mark) {
        const from = d.cur;
        d.cur = d.mark;
        d.mark = null;
        d.sc = sceneOf(d.cur, E.sc0);
        retune(art, from, d.cur);
        if (same(d.cur, d.b0)) drop(E.dk); else keep(E.dk, d);
      }
      show(art, "keys");
      return { sc: (draft(art) || { sc: E.sc0 }).sc, res: [] };
    }
    let out = { sc: (draft(art) || { sc: E.sc0 }).sc, res: [] };
    if (ops.length) {
      const r = change(art, ops);
      out = { sc: r.d ? r.d.sc : out.sc, res: r.res };
    }
    if (phase !== "move") settle(art, "keys");
    return out;
  }
  if (window.RiverKeys) RiverKeys.hooks.act = act;
  /* where the focus is in a card, to find it again once drawn again: a setting (and which of its cells), a section */
  function focusOf(art) {
    const a = document.activeElement;
    if (!a || !art.contains(a)) return null;
    if (a.dataset && a.dataset.p != null) return { p: a.dataset.p, i: [...art.querySelectorAll(`.oc-body .cell[data-p="${a.dataset.p}"]`)].indexOf(a) };
    if (a.dataset && a.dataset.k != null) return { k: a.dataset.k };   // (THE KEYS: the keyboard, a step, a button)
    const sum = a.closest("summary");
    if (sum) return { sec: sum.querySelector(".sec-name").firstChild.textContent };
    return null;
  }
  function refocus(art, f) {
    if (!f) return;
    let el = null;
    if (f.p != null) el = art.querySelectorAll(`.oc-body .cell[data-p="${f.p}"]`)[Math.max(0, f.i)];
    else if (f.k != null) el = [...art.querySelectorAll(".oc-keys [data-k], .oc-kset [data-k]")].find((e) => e.dataset.k === f.k);
    else el = [...art.querySelectorAll("details.sec summary")].find((s) => s.querySelector(".sec-name").firstChild.textContent === f.sec);
    if (el) el.focus({ preventScroll: true });
  }
  /* the card drawn again from its draft (or as it was), where it was on the screen: what the change came from keeps
   * its place (from: "keys", THE KEYS' board; "head", the card's head (UNDO, REVERT, SAVE AS); a setting's cell; else
   * the board in view, or the setting focused), whatever changed above it (EDITED's line coming, a section drawn
   * again); no browser's own scroll anchoring trusted with it (Safari has none) */
  function show(art, from) {
    const d = draft(art);
    if (pop && art.contains(pop.c)) close();
    if (!art.isConnected) return;
    const f = focusOf(art), at = anchor(art, from), y0 = at && at() && at().getBoundingClientRect().top;
    RC.redraw(art, d ? d.sc : art._edit.sc0);
    paint(art);
    refocus(art, f);
    const el = at && y0 != null && at();
    if (el) { const dy = el.getBoundingClientRect().top - y0; if (Math.abs(dy) >= 1) scrollBy(0, dy); }
  }
  /* what keeps its place on the screen: a way to find it again once drawn again, or null (a card closed: none) */
  function anchor(art, from) {
    if (!art.classList.contains("open")) return null;
    const board = () => art.querySelector(".oc-keys .k-board");
    const keys = () => (art.querySelector(".oc-keys.is-stuck") ? () => art.querySelector(".oc-cols") : board);   // (THE KEYS held
    if (from === "keys") return keys();                                                    // at the bottom stay: what is behind them, too)
    if (from === "head") return () => art.querySelector(":scope > .sr");
    if (from && from.dataset && from.dataset.p != null) {
      const p = from.dataset.p, i = [...art.querySelectorAll(`.oc-body .cell[data-p="${p}"]`)].indexOf(from);
      return () => art.querySelectorAll(`.oc-body .cell[data-p="${p}"]`)[Math.max(0, i)] || null;
    }
    const kb = board();
    if (kb) { const r = kb.getBoundingClientRect(); if (r.bottom > 0 && r.top < innerHeight) return keys(); }
    const f = focusOf(art);
    if (f && f.p != null) return () => art.querySelectorAll(`.oc-body .cell[data-p="${f.p}"]`)[Math.max(0, f.i)] || null;
    if (f && f.k != null) return () => [...art.querySelectorAll(".oc-keys [data-k], .oc-kset [data-k]")].find((e) => e.dataset.k === f.k) || null;
    const a = document.activeElement && document.activeElement.closest && document.activeElement.closest(".cell[data-p]");
    return a && art.contains(a) ? (() => art.querySelector(`.oc-body .cell[data-p="${a.dataset.p}"]`)) : null;
  }
  /* a gesture ended: a step back kept; back to the shard as it was, no draft (seal: without drawing the card again) */
  function seal(art) {
    const E = art._edit, d = draft(art);
    if (!d) return null;
    if (d.mark && !same(d.mark, d.cur)) d.back.push(d.mark);
    d.mark = null;
    if (same(d.cur, d.b0)) drop(E.dk); else keep(E.dk, d);
    return d;
  }
  function settle(art, from) { if (seal(art)) show(art, from); }
  /* a gesture of keys under way on this card: ended now (before UNDO, REVERT, SAVE) */
  function flush(art) {
    if (keyed && keyed.art === art) { const c = keyed.c; clearTimeout(keyed.t); keyed = null; settle(art, c); }
  }
  /* the sound playing, from one shard's sound to another's (UNDO, REVERT, SAVE AS), as kept: a drone's settings and
   * preset through CUSTOM (its preset back after), GLINT's preset before its settings, its type, then what differs;
   * what it plays different too: the shard taken again, whole (its notes let go, its sequences from the top) */
  function retune(art, from, to) {
    const pl = window.RiverPlayer;
    if (!pl || pl.id !== art._key || pl.state === "idle") return;
    if (!same(plays(from), plays(to))) { pl.scene(to); return; }
    const f = raw(from), t = raw(to), DRP = ID.P_DR_PRESET, GLP = ID.P_GL_PRESET, SQT = ID.P_SQ_TYPE, dr = new Set(S.drone.p);
    if (f[DRP] !== t[DRP] || [...dr].some((p) => f[p] !== t[p])) {
      pl.param(DRP, 0);
      for (const p of dr) pl.param(p, t[p]);
      if (t[DRP]) pl.param(DRP, t[DRP]);
    }
    const gl = f[GLP] !== t[GLP];
    if (gl) pl.param(GLP, t[GLP]);
    if (f[SQT] !== t[SQT]) pl.param(SQT, t[SQT]);
    for (let p = 0; p < NS; p++)
      if (p !== DRP && p !== GLP && p !== SQT && !dr.has(p) && (gl || f[p] !== t[p])) pl.param(p, t[p]);
  }
  function undoOne(art) {
    flush(art);
    const E = art._edit, d = draft(art);
    if (!d || !d.back.length) return;
    const from = d.cur;
    d.cur = d.back.pop();
    d.sc = sceneOf(d.cur, E.sc0);
    retune(art, from, d.cur);
    if (same(d.cur, d.b0)) drop(E.dk); else keep(E.dk, d);
    show(art, "head");
    const u = art.querySelector('[data-act="undo"]:not(:disabled)') || art.querySelector(".nm-n");
    if (u) u.focus({ preventScroll: true });
  }
  async function revert(art) {
    flush(art);
    const E = art._edit, d = draft(art);
    if (!d) return;
    if ((changed(d) || notesChanged(d)) && !(await confirm({ title: `Revert ${E.sc0.name}?`, text: `The changes go (${what(d)}): it plays as it was saved.`, ok: "Revert" }))) return;
    retune(art, d.cur, E.sc0.bytes);
    drop(E.dk);
    show(art, "head");
    art.querySelector(".nm-n").focus({ preventScroll: true });
  }

  // ---------------------------------------------------------------- the pointer: a drag across a setting
  let drag = null;
  const per = (p) => { const d = P[p]; return d.kind === "gauge" ? Math.max(1.4, 220 / (d.max + 1)) : d.kind === "target" ? 12 : 22; };
  const valueOf = (art, p) => { const d = draft(art); return vals(d ? d.sc : art._edit.sc0, art)[p]; };
  const CELL = ".sr-item.is-editable.open .oc-body .cell[data-p]";
  document.addEventListener("pointerdown", (e) => {
    const c = e.button === 0 && e.target.closest(CELL);
    if (!c) return;
    const art = c.closest(".sr-item"), p = +c.dataset.p;
    if (!ex) model().catch(() => toast("The editor needs FRASIL’s engine, which isn’t on this site yet. Try again in a moment."));
    if (keyed && keyed.art === art) flush(art);
    drag = { art, c, p, x0: e.clientX, y0: e.clientY, v0: valueOf(art, p), moved: false, id: e.pointerId };
  });
  document.addEventListener("pointermove", (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    const dx = e.clientX - drag.x0, dy = e.clientY - drag.y0;
    if (!drag.moved) {
      if (Math.abs(dy) > 8 && Math.abs(dy) > Math.abs(dx)) { drag = null; return; }   // (a scroll)
      if (Math.abs(dx) < 4) return;
      drag.moved = true;
      try { drag.c.setPointerCapture(drag.id); } catch (_) { /* gone */ }
      drag.c.classList.add("is-active");
      document.documentElement.classList.add("ed-dragging");
    }
    e.preventDefault();
    const x = Math.max(0, Math.min(P[drag.p].max, drag.v0 + Math.round(dx / per(drag.p))));
    if (x !== valueOf(drag.art, drag.p)) set(drag.art, drag.p, x);
  });
  function end(e, cancel) {
    if (!drag || (e && e.pointerId !== drag.id)) return;
    const g = drag;
    drag = null;
    document.documentElement.classList.remove("ed-dragging");
    g.c.classList.remove("is-active");
    if (g.moved) settle(g.art, g.c);
    else if (!cancel && P[g.p].kind !== "gauge") list(g.art, g.c, g.p);
  }
  document.addEventListener("pointerup", (e) => end(e, false));
  document.addEventListener("pointercancel", (e) => end(e, true));

  // ---------------------------------------------------------------- the keys: a setting stepped
  let keyed = null;                               // (keys on one setting in a row: one step back for them all)
  document.addEventListener("keydown", (e) => {
    const c = e.target.closest && e.target.closest(CELL);
    if (!c || e.target !== c || e.altKey || e.ctrlKey || e.metaKey) return;
    const art = c.closest(".sr-item"), p = +c.dataset.p, d = P[p], v = valueOf(art, p), big = Math.max(10, Math.round(d.max / 10));
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();                         // (Space: not a scroll)
      if (d.kind !== "gauge") list(art, c, p);
      return;
    }
    const step = { ArrowRight: 1, ArrowUp: 1, ArrowLeft: -1, ArrowDown: -1, PageUp: big, PageDown: -big }[e.key];
    let x = step != null ? v + (e.shiftKey && Math.abs(step) === 1 ? step * big : step) : e.key === "Home" ? 0 : e.key === "End" ? d.max : null;
    if (x == null) return;
    e.preventDefault();
    if (!ex) { model().catch(() => {}); return; }
    x = Math.max(0, Math.min(d.max, x));
    if (keyed && (keyed.art !== art || keyed.p !== p)) flush(keyed.art);
    clearTimeout(keyed && keyed.t);
    set(art, p, x);
    keyed = { art, p, c, t: setTimeout(() => { const k = keyed; keyed = null; if (k) settle(k.art, k.c); }, 700) };
  });

  // ---------------------------------------------------------------- a choice's list
  let pop = null;
  function list(art, c, p) {
    if (pop && pop.c === c) { close(); return; }  // (its list open: the cell clicked again closes it)
    close();
    if (!ex) { model().then(() => list(art, c, p), () => {}); return; }
    const d = P[p], now = valueOf(art, p), was = vals(art._edit.sc0, art)[p];
    const box = h("div", { class: "ed-pop", role: "listbox", "aria-label": d.name });
    const item = (x, col) => h("button", { type: "button", role: "option", class: x === now ? "is-on" : null, "aria-selected": String(x === now),
      style: col && x !== now ? `color:${col}` : null, "data-x": x }, word(p, x), x === was && x !== now ? h("span", { class: "w" }, "WAS") : null);
    if (d.kind === "target") {
      const COL = { drone: "#dce3ea", glint: "#6ed0ff", bass: "#f2a65a" };
      box.append(item(0));
      for (const [tr, nm] of [["drone", "DRONE"], ["glint", "GLINT"], ["bass", "BASS"]]) {
        const of = S.lfo.targets.map((t, x) => (x && t && t.track === tr ? x : -1)).filter((x) => x > 0 && x <= d.max);
        if (of.length) box.append(h("h5", { "aria-hidden": "true" }, nm), ...of.map((x) => item(x, COL[tr])));
      }
    } else for (let x = 0; x <= d.max; x++) box.append(item(x));
    box.addEventListener("click", (e) => {
      const b = e.target.closest("button[data-x]");
      if (!b) return;
      const at = [...art.querySelectorAll(`.oc-body .cell[data-p="${p}"]`)].indexOf(c);
      close();
      if (set(art, p, +b.dataset.x)) settle(art, c.isConnected ? c : null);
      const again = art.querySelectorAll(`.oc-body .cell[data-p="${p}"]`)[Math.max(0, at)];
      if (again) again.focus({ preventScroll: true });
    });
    box.addEventListener("keydown", (e) => {
      const opts = [...box.querySelectorAll("button")], i = opts.indexOf(document.activeElement);
      if (e.key === "ArrowDown" || e.key === "ArrowUp") { e.preventDefault(); opts[Math.max(0, Math.min(opts.length - 1, i + (e.key === "ArrowDown" ? 1 : -1)))].focus(); }
      else if (e.key === "Home" || e.key === "End") { e.preventDefault(); opts[e.key === "Home" ? 0 : opts.length - 1].focus(); }
      else if (e.key === "Escape" || e.key === "Tab") {
        e.preventDefault();
        e.stopPropagation();                      // (Escape here closes the list, nothing else: the sound plays on)
        close();
        if (c.isConnected) c.focus({ preventScroll: true });
      }
    });
    document.body.append(box);
    const r = c.getBoundingClientRect(), below = innerHeight - r.bottom;
    box.style.left = `${Math.max(8, Math.min(r.left, innerWidth - box.offsetWidth - 8)) + scrollX}px`;
    box.style.top = `${(below < 260 && r.top > below ? r.top - box.offsetHeight - 4 : r.bottom + 4) + scrollY}px`;
    c.classList.add("is-open");
    pop = { box, c };
    (box.querySelector(".is-on") || box.querySelector("button")).focus({ preventScroll: true });
  }
  function close() {
    if (!pop) return;
    pop.box.remove();
    pop.c.classList.remove("is-open");
    pop = null;
  }
  document.addEventListener("pointerdown", (e) => { if (pop && !pop.box.contains(e.target) && !pop.c.contains(e.target)) close(); }, true);

  // ================================================================ SAVE, SAVE AS
  async function saveOwn(art) {
    flush(art);
    const E = art._edit, d = draft(art);
    if (!d || !E.o.edit.save || E.busy) return;
    if (E.o.edit.shared && !(await confirm({ title: `Replace ${E.sc0.name}?`, ok: "Replace it",
      text: "It is shared: those who play it in the Community River, and those who favorited it, will hear the new sound." }))) return;
    E.busy = true;
    const now = draft(art) || d;
    drop(E.dk);                                   // (the page draws the row again, saved: no draft)
    try { await E.o.edit.save(now.cur); }
    catch (e) {
      drafts.set(E.dk, now);
      keep(E.dk, now);
      if (art.isConnected) show(art, "head");
      toast(e.message);
    } finally { E.busy = false; }
  }
  /* the next name: ABYSS II, ABYSS III ... (16 characters at most) */
  function nextName(name) {
    const R = ["II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"], m = /^(.*?)\s+(II|III|IV|V|VI|VII|VIII|IX)$/.exec(name);
    const base = m ? m[1] : name, n = m ? R[R.indexOf(m[2]) + 1] || "II" : "II";
    return (base.slice(0, NL - n.length - 1).trimEnd() + " " + n).trim();
  }
  function saveAs(art) {
    flush(art);
    const E = art._edit, o = E.o.edit;
    if (!draft(art) || E.busy) return;
    const can = o.saveAs && (!o.canSave || o.canSave()), from = o.from;
    const name = h("input", { class: "name-in", name: "name", required: true, maxlength: NL, autocomplete: "off", spellcheck: "false", value: nextName(E.sc0.name) });
    const desc = h("textarea", { name: "description", maxlength: 1000, rows: 4 });
    desc.value = o.description || "";
    const say = h("p", { class: "form-say", role: "alert" });
    const go = h("button", { class: "btn btn-ice", type: "submit" }, "Save");
    name.addEventListener("input", () => { const at = name.selectionStart; name.value = name.value.toUpperCase(); name.setSelectionRange(at, at); });
    const credited = from && (from.first || from.author);
    const form = h("form", { class: "sheet-in", novalidate: true },
      h("header", { class: "sheet-head" }, h("p", { class: "label" }, `${E.sc0.name} · edited`),
        h("button", { class: "sheet-x", type: "button", "data-close": "", "aria-label": "Close" }, I.x())),
      h("h2", { class: "sheet-h", id: "ed-h" }, "Save as a new shard"),
      can ? [
        h("p", { class: "sheet-lede" }, from ? `A new shard in Your Private River. ${E.sc0.name} stays as ${from.author} shared it.` : `A new shard in Your Private River. ${E.sc0.name} stays as it is.`),
        h("label", { class: "field" }, h("span", { class: "field-l" }, "Name, as the FM-1 shows it"), name,
          h("span", { class: "field-n" }, `${NL} characters at most: A to Z, 0 to 9, space - . # +`)),
        h("label", { class: "field" }, h("span", { class: "field-l" }, "Description"), desc, h("span", { class: "field-n" }, "Optional. Its first line shows in the list.")),
        from ? h("div", { class: "ed-from" }, h("span", { class: "cc", "aria-hidden": "true" }, "CC BY"),
          h("p", null, h("b", null, `After ${from.name}, by ${from.author}` + (from.first && from.first !== from.author ? `, first shared by ${from.first}` : "")), h("br"),
            `It was shared under CC BY 4.0, so your shard names ${credited}, here and in the Community River if you share yours.`)) : null,
        say,
        h("footer", { class: "sheet-foot" }, h("button", { class: "btn", type: "button", "data-close": "" }, "Cancel"), go),
      ] : [
        h("p", { class: "sheet-lede" }, "Sign in to keep it in your River, and send it to your FM-1 from there. Your changes stay in this tab: come back here once signed in."),
        h("div", { class: "ed-sign" }, h("a", { class: "btn btn-ice", href: "/yours/#signin" }, "Sign in"), h("a", { class: "btn", href: "/yours/#register" }, "Create an account")),
      ]);
    const dlg = h("dialog", { class: "sheet sheet-sm ed-sheet", "aria-labelledby": "ed-h" }, form);
    dlg.addEventListener("click", (e) => { if (e.target === dlg || e.target.closest("[data-close]")) dlg.close(); });
    dlg.addEventListener("close", () => dlg.remove());
    form.addEventListener("submit", async (e) => {
      e.preventDefault();
      const nm = name.value.trim().toUpperCase();
      if (!NAME.test(nm)) { say.textContent = `A name: 1 to ${NL} characters, A to Z, 0 to 9, space - . # +`; name.focus(); return; }
      const now = draft(art);
      if (!now) { dlg.close(); return; }
      go.disabled = true;
      E.busy = true;
      drop(E.dk);                                  // (the changes go into the new shard: this one as it was)
      try {
        const el = await o.saveAs({ bytes: now.cur, name: nm, description: desc.value });
        dlg.close();
        if (art.isConnected) { retune(art, now.cur, E.sc0.bytes); show(art, "head"); }
        const to = el && el.isConnected ? el : art.isConnected ? art.querySelector(".nm-n") : null;
        if (to) to.focus({ preventScroll: true });
      } catch (err) {
        drafts.set(E.dk, now);
        keep(E.dk, now);
        if (art.isConnected) show(art, "head");
        if (dlg.open) say.textContent = err.message; else toast(err.message);
      } finally { go.disabled = false; E.busy = false; }
    });
    document.body.append(dlg);
    dlg.showModal();
    if (can) { name.focus(); name.setSelectionRange(name.value.length, name.value.length); }
  }

  return { drafts, nextName, ready: () => model() };
})();
