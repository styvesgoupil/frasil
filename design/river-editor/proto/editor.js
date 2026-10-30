/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's editor, the mockups' prototype: a card's settings turned as the FM-1 turns them. The edits go through
 * FRASIL's own engine on this page (app.wasm, silent: web_param, as a MIDI CC learned on a setting sets it, then
 * web_scene_edited), so a preset's sound, CUSTOM from a drone's setting turned, are the FM-1's own; the card is drawn
 * again from what comes back. Here only to stage the mockups' states:
 *   Editor.edited(bytes, [[p, x], ...]) -> the shard's bytes, edited       Editor.scene(bytes, like) -> a card's scene
 *   Editor.stage(art, sc0, edits, o)   the card art (opened) of sc0, edited: its body drawn again, the changes marked,
 *                                       its head the editor's; o: {open: [sections], active: p, hover: p, pop: p, popHover}
 *   Editor.sheet(o)                     SAVE AS: {source, name, description, from: {name, author}, signedOut, ownName} */
"use strict";
window.Editor = (() => {
  const { h, s, I } = RiverCards;
  const S = FB.S, P = S.params, ID = S.ids;
  let ex = null;

  async function model() {
    if (ex) return ex;
    const wasm = await (await fetch("/emu/app.wasm")).arrayBuffer();
    const stub = new Proxy({ clock_time_get: () => 0, fd_write: () => 0, proc_exit: () => {} }, { get: (t, k) => (k in t ? t[k] : () => 0) });
    const env = new Proxy({}, { get: () => () => 0 });
    const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
    ex = instance.exports;
    if (ex._initialize) ex._initialize();
    ex.web_master(2800);
    ex.web_boot();
    return ex;
  }
  async function edited(bytes, edits) {
    const x = await model(), n = x.web_scene_size();
    new Uint8Array(x.memory.buffer, x.web_scene_buf(), n).set(bytes.subarray(0, n));
    x.web_scene_play();
    for (const [p, v] of edits) x.web_param(p, v);
    return Uint8Array.from(new Uint8Array(x.memory.buffer, x.web_scene_edited(), n));
  }
  function scene(bytes, like) {
    const pack = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: S.magic.shard, firmware: FRASIL_VERSION,
                                              scenes: [{ name: like.name, from: 1, data: RiverAPI.b64(bytes) }] }));
    return { ...pack.scenes[0], slot: like.slot, state: like.state, shard: like.shard };
  }
  const word = (p, x) => { const d = P[p]; return d.texts[Math.max(0, Math.min(x, d.max))] ?? String(x); };

  /* each cell of the opened sections, with its setting: a page's cells are its kept settings in order */
  function cellsOf(art) {
    const out = [];
    art.querySelectorAll(".oc-right .pg").forEach((pgEl) => {
      const name = pgEl.querySelector(".pg-name").firstChild.textContent, pg = S.pages.find((x) => x.name === name);
      if (!pg) return;
      const keep = pg.p.filter((p) => p >= 0 && p < S.sizes.NSOUND);
      pgEl.querySelectorAll(".cell").forEach((c, i) => out.push({ c, p: keep[i] }));
    });
    return out;
  }

  async function stage(art, sc0, edits, o = {}) {
    const bytes = await edited(sc0.bytes, edits), sc = scene(bytes, sc0);
    const tmp = RiverCards.row(sc, {}), body = tmp._build()[1];
    art.querySelector(".oc-body").replaceWith(body);
    body.querySelectorAll("details.sec").forEach((d) => { if ((o.open || []).includes(d.querySelector(".sec-name").textContent)) d.open = true; });
    await new Promise((r) => setTimeout(r, 30));                    // (the sections build when opened)
    const changed = new Set();
    for (let p = 0; p < S.sizes.NSOUND; p++) if (sc.v[p] !== sc0.v[p]) changed.add(p);
    for (const { c, p } of cellsOf(art)) {
      const d = P[p];
      c.classList.toggle("is-choice", d.kind !== "gauge");
      if (changed.has(p)) {
        c.classList.add("is-edited");
        c.title = "Was " + word(p, sc0.v[p]);
        const g = c.querySelector(".gauge");
        if (g) g.append(h("i", { class: "was", style: `left:${Math.max(1, Math.min(88, Math.round(sc0.v[p] / Math.max(1, d.max) * 88))) / 88 * 100}%` }));
      }
      if (p === o.hover) c.classList.add("is-hover");
      if (p === o.active) { c.classList.add("is-active"); c.querySelector(".cell-v").append(h("span", { class: "was-t" }, "WAS " + word(p, sc0.v[p]))); }
      if (p === o.pop) pop(c, p, sc.v[p], sc0.v[p], o.popHover);
    }
    const touched = new Set(S.pages.filter((pg) => pg.p.some((p) => changed.has(p))).map(sectionOf));
    body.querySelectorAll("details.sec .sec-name").forEach((nm) => { if (touched.has(nm.textContent)) nm.append(h("span", { class: "ed-dot", "aria-hidden": "true" })); });
    const n = edits.length;
    const band = h("div", { class: "ed-band" }, h("b", null, "EDITED"),
      h("span", null, h("em", null, n === 1 ? "One change" : `${["", "", "Two", "Three", "Four", "Five"][n] || n} changes`),
        `. The sound follows them; ${sc0.name} stays as it was until you save them as a new shard.`));
    const old = art.querySelector(".ed-band");
    if (old) old.remove();
    art.querySelector(".oc-in").insertBefore(band, art.querySelector(".oc-body"));
    art.querySelector(".acts").replaceChildren(
      h("button", { type: "button", class: "a a-undo" }, undoIcon(), "Undo"),
      h("button", { type: "button", class: "a a-txt" }, "Revert"),
      h("button", { type: "button", class: "a a-ice" }, "Save as…"));
    art.classList.add("is-edited");
    return { bytes, sc };
  }
  const sectionOf = (pg) => (pg.name.match(/^LFO \d+/) || [])[0] || SECTION_OF(pg);
  const SECTION_OF = (pg) => ({ EDIT: "DRONE", ENV: "DRONE", LFO: "DRONE", ARP: "GLINT", SEQ: "BASS", FX: "FX", SEL: "HARMONY", HOME: "MIXER", GLO: "MASTERING" })[pg.button];
  const undoIcon = () => s("svg", { viewBox: "0 0 16 16", width: 15, height: 15, "aria-hidden": "true", fill: "none", stroke: "currentColor", "stroke-width": 1.4, "stroke-linecap": "round", "stroke-linejoin": "round" },
    s("path", { d: "M5.5 3.5L2.5 6.5l3 3M2.8 6.5h6.7a3.8 3.8 0 0 1 0 7.6H7" }));

  /* a choice's list under its value: each the screen's word, the one now ice, the one it was named; a TARGET's grouped
   * by track, each in its colour */
  function pop(c, p, now, was, hov) {
    const d = P[p], box = h("div", { class: "ed-pop", role: "listbox" });
    const item = (x, col) => h("button", { type: "button", role: "option", class: (x === now ? "is-on" : "") + (x === hov ? " is-hover" : ""), style: col ? `color:${col}` : null },
      word(p, x), x === was && x !== now ? h("span", { class: "w" }, "WAS") : null);
    if (d.kind === "target") {
      const COL = { drone: "#dce3ea", glint: "#6ed0ff", bass: "#f2a65a" };
      box.append(item(0));
      for (const [tr, nm] of [["drone", "DRONE"], ["glint", "GLINT"], ["bass", "BASS"]]) {
        box.append(h("h5", null, nm));
        S.lfo.targets.forEach((t, x) => { if (x && t && t.track === tr) box.append(item(x, x === now ? null : COL[tr])); });
      }
    } else for (let x = 0; x <= d.max; x++) box.append(item(x));
    document.body.append(box);                                       // (over the page: never clipped by the card)
    requestAnimationFrame(() => {
      const r = c.getBoundingClientRect();
      box.style.left = `${r.left + scrollX}px`;
      box.style.top = `${r.bottom + scrollY + 4}px`;
    });
    c.classList.add("is-open");
  }

  function sheet(o) {
    const name = h("input", { value: o.name, maxlength: 16, "aria-label": "Name", spellcheck: "false" });
    const left = h("span", { class: "n" }, `${o.name.length} / 16`);
    const body = o.signedOut ? [
      h("p", { class: "sheet-lede" }, "Sign in to keep it in your River, and send it to your FM-1 from there. Your changes wait on this page."),
      h("div", { class: "ed-sign" }, h("a", { class: "btn btn-ice", href: "#" }, "Sign in"), h("a", { class: "btn", href: "#" }, "Create an account")),
    ] : [
      h("p", { class: "sheet-lede" }, o.from ? `A new shard in Your Private River. ${o.from.name} stays as ${o.from.author} shared it.` : `A new shard in Your Private River. ${o.ownName} stays as it is.`),
      h("div", { class: "ed-f" }, h("label", null, "NAME, AS THE FM-1 SHOWS IT"), h("div", { class: "ed-name" }, name, left),
        h("span", { class: "ed-hint" }, "Capitals, numbers, space and - . # +")),
      h("div", { class: "ed-f" }, h("label", null, "DESCRIPTION"), h("textarea", { placeholder: "Optional: what it is, how to play it" }, o.description || "")),
      o.from && h("div", { class: "ed-from" }, h("span", { class: "cc" }, "CC BY"),
        h("p", null, h("b", null, `After ${o.from.name}, by ${o.from.author}`), h("br"),
          "It was shared under CC BY 4.0, so your shard names it and its author, here and in the Community River if you share yours.")),
    ];
    const dlg = h("dialog", { class: "sheet sheet-sm ed-sheet", "aria-labelledby": "ed-h" },
      h("form", { class: "sheet-in", method: "dialog" },
        h("header", { class: "sheet-head" }, h("p", { class: "label" }, `${o.source} · edited`),
          h("button", { class: "sheet-x", type: "button", "aria-label": "Close" }, I.x())),
        h("h2", { class: "sheet-h", id: "ed-h" }, "SAVE AS A NEW SHARD"), body,
        o.signedOut ? null : h("footer", { class: "sheet-foot" }, h("button", { class: "btn", type: "button" }, "Cancel"), h("button", { class: "btn btn-ice", type: "button" }, "Save"))));
    document.body.append(dlg);
    dlg.showModal();
    if (!o.signedOut) { name.focus(); name.setSelectionRange(name.value.length, name.value.length); } else dlg.querySelector(".btn-ice").focus();
    return dlg;
  }
  return { model, edited, scene, stage, sheet };
})();
