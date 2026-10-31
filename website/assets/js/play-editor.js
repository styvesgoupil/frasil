/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The Play page's two modes: EMULATOR (fm1-base's emulator, the whole FM-1) and EDITOR (the River's editor, here): the
 * switch at the top (#emulator, #editor; the last one chosen kept in this browser; EDITOR the first time). One engine
 * sounds at a time: the emulator's audio sleeps in EDITOR, a shard playing stops in EMULATOR.
 *   EDITOR   NEW SHARD and the factory scenes (the schema's: FB.NEW_SHARD, FB.FACTORY) as the River's rows
 *            (river-cards.js), every setting turned while it plays (river-edit.js), THE KEYS (river-keys.js), FRASIL's
 *            engine beside the page (window.RIVER_EMU: the emulator's own app.wasm). No River service here: kept as
 *            DOWNLOAD (a scene package, as the River's), SEND TO FM-1 (river-device.js) and OPEN IN THE RIVER: the
 *            River's Your Private River, the shard in its address (#play=LAYOUT.FROM.DATA: FROM n NEW SHARD, 1-16 the
 *            factory slot it began as; DATA its bytes, base64url), where river-yours.js opens it as a draft to SAVE AS.
 * Loaded before the River's scripts (window.RIVER_EMU, a RiverAPI without a service); the page's first mode is set in
 * its head (data-play), before it is drawn. */
"use strict";
(() => {
  const root = document.documentElement, sw = document.getElementById("mode-switch");
  if (!sw) return;
  const MODES = ["emulator", "editor"], KEY = "frasil-play-mode";
  // (before the River's scripts: its engine is the emulator's, beside the page; no River service on this site, its
  // bytes' words only)
  window.RIVER_EMU = window.RIVER_EMU || "./";
  if (!window.RiverAPI) {
    const b64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
    window.RiverAPI = { b64, unb64: (t) => Uint8Array.from(atob(t), (c) => c.charCodeAt(0)), me: () => Promise.resolve(null), onMe: () => {},
                        signedIn: null, offline: null, scene: () => null, day: (iso) => (iso ? new Date(iso).toLocaleDateString("en-GB") : "") };
  }

  // ---------------------------------------------------------------- one engine at a time
  /* the emulator's AudioContext (made by its Switch on, in EMULATOR): asleep while EDITOR shows */
  const emu = new Set(), asleep = new Set();
  const AC = window.AudioContext;
  if (AC) window.AudioContext = new Proxy(AC, { construct(t, args, nt) {
    const ac = Reflect.construct(t, args, nt);
    if (root.dataset.play === "emulator") emu.add(ac);
    return ac;
  } });
  function show(mode, user) {
    if (!MODES.includes(mode)) mode = "editor";
    const was = root.dataset.play;
    root.dataset.play = mode;
    for (const b of sw.querySelectorAll("button")) b.setAttribute("aria-pressed", String(b.dataset.mode === mode));
    if (mode === "editor") {
      for (const ac of emu) if (ac.state === "running") { asleep.add(ac); ac.suspend().catch(() => {}); }
      build();
    } else {
      if (window.RiverCards) RiverCards.stopSound();
      for (const ac of asleep) ac.resume().catch(() => {});
      asleep.clear();
    }
    if (user && was !== mode) {
      try { localStorage.setItem(KEY, mode); } catch (_) { /* (a private window: not kept) */ }
      history.replaceState(null, "", location.pathname + location.search + "#" + mode);
    }
  }
  sw.addEventListener("click", (e) => { const b = e.target.closest("button[data-mode]"); if (b) show(b.dataset.mode, true); });
  addEventListener("hashchange", () => { const m = location.hash.slice(1); if (MODES.includes(m)) show(m, false); });

  // ---------------------------------------------------------------- EDITOR: the scenes as the River's rows
  const box = document.getElementById("ed-list"), sec = document.getElementById("editor");
  let built = false;
  function build() {
    if (built || !box || typeof FB === "undefined" || !window.RiverCards || !window.RiverEdit || !window.RiverList) return;
    built = true;
    const { h, I, toast, two } = RiverCards, RL = RiverList, S = FB.S, N = S.sizes;
    const river = (sec.dataset.river || "https://river.frasilsynth.com/").replace(/\/?$/, "/");
    const items = [
      ...(FB.NEW_SHARD ? [{ sc: { ...FB.NEW_SHARD, slot: null, state: "saved" }, from: "n", no: "+", sub: "START FRESH", cls: "is-new" }] : []),
      ...FB.FACTORY.slice(0, N.NFACTORY).map((sc, i) => ({ sc: { ...sc, slot: i, state: "factory" }, from: String(i + 1), no: two(i + 1), sub: "FACTORY SCENE", cls: "is-factory" })),
    ];
    const slug = (t) => t.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "shard";
    const b64url = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, ""); };
    /* the card's shard as it is now, as a scene the River's tools take */
    const now = (art, it) => { const n = RiverEdit.now(art); return { ...n.sc, bytes: n.bytes, slot: it.sc.slot, state: "saved", current: true }; };
    const card = (e) => e.currentTarget.closest(".sr-item");
    function download(art, it) {
      const sc = now(art, it), name = `${slug(sc.name)}.${FW.ext}scenes`;
      try {
        const url = URL.createObjectURL(new Blob([FB.packOf([{ ...sc, slot: sc.slot ?? 0 }], FRASIL_VERSION)], { type: "application/json" }));
        const a = h("a", { href: url, download: name });
        document.body.append(a); a.click(); a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
        toast(h("b", null, name), " is in your downloads.");
      } catch (e) { toast(e.message); }
    }
    const send = (art, it) => RL.send([now(art, it)]);
    function toRiver(art, it) {
      const url = `${river}yours/#play=${S.magic.shard}.${it.from}.${b64url(now(art, it).bytes)}`;
      if (!window.open(url, "_blank")) location.href = url;
    }
    /* a card's ways to keep it: in its head opened, and the editor's once edited (after UNDO and REVERT) */
    const keeps = (it) => [
      RL.act(I.dl(), "Download", (e) => download(card(e), it), { of: it.sc.name, act: "download" }),
      RL.act(I.send(15), "Send to FM-1", (e) => send(card(e), it), { ice: true, of: it.sc.name, act: "send" }),
      RL.act(I.ext(), "Open in the River", (e) => toRiver(card(e), it), { of: it.sc.name, act: "river" }),
    ];
    const rows = items.map((it, i) => RiverCards.row(it.sc, { id: `ed-${it.from}`, key: `play:${it.from}`, i, class: it.cls, no: it.no, sub: it.sub,
                                                            actions: keeps(it), edit: { keeps: () => keeps(it) }, openOnPlay: true }));
    box.replaceChildren(h("div", { class: "list-top" }, RL.head([null, null, { t: "NAME" }, { t: "WHAT PLAYS" }, { t: "KEY" }, { t: "GLINT" }, { t: "BASS" }])), ...rows);
  }

  // ---------------------------------------------------------------- the first mode: the address, the last chosen, EDITOR
  let first = location.hash.slice(1);
  if (!MODES.includes(first)) { try { first = localStorage.getItem(KEY); } catch (_) { first = null; } }
  const go = () => show(MODES.includes(first) ? first : "editor", false);
  if (document.readyState === "complete") go(); else document.addEventListener("DOMContentLoaded", go);   // (the River's scripts, deferred after this one, run first)
})();
