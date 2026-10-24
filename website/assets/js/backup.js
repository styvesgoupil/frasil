/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's Quick Backup (pages/river/tools.html: Tools is Quick Backup, at /tools/ and at /backup/): no account; the
 * FM-1's backup read, kept, opened, put back. The rows are river-cards.js's, the FM-1's dialogs river-device.js's,
 * the model backup-model.js (FB).
 *   the dock     BACK UP THE FM-1 | OPEN A FILE (or one dropped on the page; the example: #example)
 *   the source   where what is shown comes from (FROM YOUR FM-1 · 20:43 · FRASIL 1.1 · 4 saved, 7 factory, 5 empty,
 *                and the settings), with its own actions: Download the backup, Restore it all…
 *   the slots    the sixteen as rows, numbered (empty ones dim), the settings last; ticked: Send to FM-1, Download */
"use strict";
(() => {
  if (!document.getElementById("act-backup")) return;
  const { h, two, listWords, ticked, stopSound, I } = RiverCards;
  const D = RiverDevice, RL = RiverList;
  const $ = (id) => document.getElementById(id);
  const EXT = FW.ext, PACK_EXT = FW.ext + "scenes";
  const V = D.voice($("voice"));

  // ================================================================ the state
  let src = null;            // what is shown: {kind: device|file|example|pack, name, image?, flags, scenes, settings, layout, current, at}
  let rows = [], kept = true;
  const picked = new Set();  // the shards ticked (their index in src.scenes)

  function paintControls() {
    const busy = D.busy, image = !!(src && src.image);
    $("act-backup").disabled = busy;
    $("act-open").disabled = busy;
    $("act-example").disabled = busy;
    $("act-restore-all").disabled = busy || !image;
    const send = document.querySelector('#cards .selbar [data-act="send"]');
    if (send) send.disabled = busy;
  }
  D.onBusy(paintControls);
  addEventListener("beforeunload", (e) => { if (!kept) { e.preventDefault(); e.returnValue = ""; } });

  // ---------------------------------------------------------------- names, words
  const day = (d = new Date()) => `${d.getFullYear()}-${two(d.getMonth() + 1)}-${two(d.getDate())}`;
  const slug = (t) => t.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "shard";
  const backupName = () => `${slug(FW.name)}-backup-${day()}.${EXT}`;
  function counts(sc) {
    const n = (st) => sc.filter((x) => x.state === st).length;
    return { saved: n("saved"), factory: n("factory"), empty: n("empty") };
  }
  function countWords(c) {
    const out = [];
    if (c.saved) out.push(`${c.saved} saved`);
    if (c.factory) out.push(`${c.factory} factory`);
    if (c.empty) out.push(`${c.empty} empty`);
    return out;
  }
  function save(blob, name) {
    const url = URL.createObjectURL(blob);
    const a = h("a", { href: url, download: name });
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
  }

  // ================================================================ what is shown
  function load(next, user) {
    stopSound();
    src = { at: new Date(), ...next };
    picked.clear();
    kept = src.kind !== "device";
    if (src.kind !== "example" && location.hash === "#example") history.replaceState(null, "", location.pathname + location.search);
    render();
    paintControls();
    V.say(loadedWords(), "ok");
    if (user) $("river").scrollIntoView({ behavior: RiverCards.reduce.matches ? "auto" : "smooth", block: "nearest" });
  }
  function loadedWords() {
    const c = countWords(counts(src.scenes)), old = src.current ? "" : " Its scenes were made by an earlier FRASIL: they are brought up to date when restored.";
    const names = src.scenes.filter((x) => x.state === "saved").map((x) => x.name);
    const playing = src.flags & 1 ? [" ", ...D.PLAYING_NOTE()] : [];
    if (src.kind === "device")
      return [`Read: ${listWords(c)}, and the settings. Nothing is on this computer yet: `, h("b", null, "Download the backup"), " saves it as ", h("b", null, src.name), "." + old, ...playing];
    if (src.kind === "example")
      return [`The example, a backup made on FRASIL’s simulator: ${listWords(names)}. Look around and select a shard; nothing reaches your FM-1 until you send it.`];
    if (src.kind === "pack")
      return [h("b", null, src.name), `: a package of ${src.scenes.length === 1 ? "one shard" : src.scenes.length + " shards"}, ${listWords(names)}${src.pack.firmware ? `, from FRASIL ${src.pack.firmware}` : ""}. Select them to send them to the FM-1.`];
    return [h("b", null, src.name), `: ${listWords(c)}, and the settings.` + old, ...playing];
  }
  /* the source line: where it comes from, then what it holds */
  function sourceWords() {
    const pack = src.kind === "pack", c = countWords(counts(src.scenes)).join(", ");
    const holds = pack ? [RL.plural(src.scenes.length, "shard", "shards"), src.pack.firmware ? `FRASIL ${src.pack.firmware}` : null]
                       : [src.settings ? (c ? `${c}, and the settings` : "the settings") : c];
    const from = { device: ["FROM YOUR FM-1", D.clock(src.at), D.version], example: ["THE EXAMPLE", "made on FRASIL’s simulator"],
                   file: ["A BACKUP", src.name], pack: ["A SCENE PACKAGE", src.name] }[src.kind];
    return [from[0], " ", h("i", null, ["", ...from.slice(1), ...holds].filter((x) => x != null).join(" · "))];
  }
  function render() {
    const pack = src.kind === "pack";
    $("qb-start").hidden = true;
    $("river").hidden = false;
    RiverCards.fill($("river-src"), ...sourceWords());
    const flag = $("river-flag");
    flag.hidden = !(src.flags & 1) && src.current;
    RiverCards.fill(flag, ...(src.flags & 1 ? D.PLAYING_NOTE() : !src.current ? ["Made by an earlier FRASIL: brought up to date when restored."] : []));
    $("act-keep-all").hidden = !(src.image && (src.kind === "device" || src.kind === "example"));
    $("act-restore-all").hidden = !src.image || !D.hasMidi;
    $("rv-kept").textContent = "";
    $("cards").classList.toggle("is-pack", pack);
    rows = src.scenes.map((sc, i) => (sc.state === "empty" ? emptyRow(sc, i) : RiverCards.row(sc, {
      id: `shard-${i}`, key: `qb-${i}`, i, class: `is-slot is-${sc.state}`, no: two(sc.slot + 1),
      tick: { disabled: !sc.current, aria: `Select ${sc.name}, slot ${sc.slot + 1}`, onChange: (on) => tick(i, on) },
      sub: sc.state === "factory" ? "FACTORY SCENE" : null,
    })));
    $("cards").replaceChildren(h("div", { class: "list-top" }), ...rows, ...(pack ? [] : [RiverCards.settingsRow(src.settings, src.scenes)]));
    paintSel();
  }
  /* nothing saved in the slot: its number, dim */
  const emptyRow = (sc, i) => h("div", { class: "sr-item is-empty", id: `shard-${i}` },
    h("div", { class: "sr" }, h("span", { class: "ck" }, h("input", { type: "checkbox", disabled: true, "aria-label": `Slot ${sc.slot + 1}: empty` })),
      h("span", { class: "no" }, two(sc.slot + 1)), h("span"), h("span", { class: "nm" }, h("span", { class: "nm-e" }, "EMPTY"))));
  /* the river shows the FM-1 as it was: once something is sent to it, say so */
  D.onChanged(() => {
    if (!src || src.kind !== "device") return;
    $("river-flag").hidden = false;
    $("river-flag").textContent = "The FM-1 has changed since this backup: back it up again to see it as it is now.";
  });

  // ---------------------------------------------------------------- the shards ticked: the column labels, or their bar
  function tick(i, on) {
    if (on) picked.add(i); else picked.delete(i);
    if (rows[i]) ticked(rows[i], on);
    paintSel();
  }
  const pickedList = () => [...picked].sort((a, b) => a - b).map((i) => src.scenes[i]);
  function paintSel(focusFirst) {
    const top = $("cards").querySelector(".list-top"), n = picked.size;
    if (n) {
      top.replaceChildren(RL.selbar(n, [
        D.hasMidi ? RL.act(I.send(15), "Send to FM-1", openPlace, { txt: true, act: "send" }) : null,
        RL.act(I.dl(), "Download", downloadPicked, { txt: true, act: "download" }),
      ].filter(Boolean), clearSel));
    } else {
      top.replaceChildren(RL.head([null, { t: "SLOT" }, null, { t: "NAME" }, { t: "WHAT PLAYS" }, { t: "KEY" }, { t: "GLINT" }, { t: "BASS" }]));
      if (focusFirst) { const r = $("cards").querySelector(".sr-item .nm-n"); if (r) r.focus({ preventScroll: true }); }
    }
    paintControls();
  }
  function clearSel() {
    [...picked].forEach((i) => { picked.delete(i); if (rows[i]) ticked(rows[i], false); });
    paintSel(true);
  }
  function downloadPicked() {
    const list = pickedList();
    try {
      const text = FB.packOf(list, FRASIL_VERSION);
      const name = list.length === 1 ? `${slug(list[0].name)}.${PACK_EXT}` : `${slug(FW.name)}-scenes-${day()}.${PACK_EXT}`;
      save(new Blob([text], { type: "application/json" }), name);
      RiverCards.toast(h("b", null, name), " is in your downloads.");
    } catch (e) { RiverCards.toast(e.message); }
  }
  function keepAll() {
    if (!src || !src.image) return;
    const name = src.kind === "device" ? src.name : `${slug(FW.name)}-example.${EXT}`;
    save(new Blob([src.image], { type: "application/octet-stream" }), name);
    kept = true;
    $("rv-kept").textContent = `${name} is in your downloads.`;
  }

  // ================================================================ the flows
  async function backUp() {
    if (D.busy) return;
    try { load({ kind: "device", name: backupName(), ...(await D.backup(V)) }, true); }
    catch (e) { V.still(); V.say(e.message, "err"); }
  }
  /* a file's bytes -> what is shown: a backup (FRBK...) or a scene package (JSON) */
  function readBytes(buf, name, kind) {
    const image = PB.MAGIC.every((b, i) => buf[i] === b);
    if (!image && (new RegExp(`\\.${PACK_EXT}$`, "i").test(name) || buf[0] === 0x7B || buf[0] === 0xEF)) {
      let text;
      try { text = new TextDecoder("utf-8", { fatal: true }).decode(buf); } catch (_) { throw new Error("This file is not a FRASIL scene package."); }
      const pack = FB.readPack(text.replace(/^﻿/, ""));
      return { kind: "pack", name, pack, scenes: pack.scenes, settings: null, layout: pack.layout, current: true, flags: 0 };
    }
    const parsed = PB.parse(buf);
    return { kind, name, image: buf, ...FB.read(parsed), flags: parsed.flags };
  }
  async function openFile(f) {
    if (D.busy || !f) return;
    try { load(readBytes(new Uint8Array(await f.arrayBuffer()), f.name, "file"), true); }
    catch (e) { V.still(); V.say([h("b", null, f.name), `: ${e.message} Open a FRASIL backup (.${EXT}) or a scene package (.${PACK_EXT}).`], "err"); }
  }
  async function openExample(user) {
    if (D.busy) return;
    V.say("Opening the example…", "work");
    try {
      const r = await fetch("/example.frasil", { cache: "no-cache" });
      if (!r.ok) throw new Error(`It isn't there (${r.status}).`);
      load(readBytes(new Uint8Array(await r.arrayBuffer()), "example.frasil", "example"), user);
      if (location.hash !== "#example") history.replaceState(null, "", "#example");
    } catch (e) { V.say(`The example didn't open. ${e.message}`, "err"); }
  }
  function openWhole() {
    if (D.busy || !src || !src.image) return;
    D.restore(src.image, src.kind === "device" ? "The backup you just read" : src.kind === "example" ? "The example" : src.name);
  }
  function openPlace() {
    if (D.busy || !picked.size) return;
    const list = pickedList().filter((sc) => sc.current && sc.bytes);
    if (list.length > 16) { RiverCards.toast("16 at most: the FM-1 has sixteen slots."); return; }
    RL.send(list);
  }

  // ---------------------------------------------------------------- a file dropped on the page
  const drop = $("drop");
  const hasFiles = (e) => Array.from((e.dataTransfer && e.dataTransfer.types) || []).includes("Files");
  let depth = 0;
  addEventListener("dragenter", (e) => { if (!hasFiles(e) || D.busy || D.dialogOpen()) return; e.preventDefault(); depth++; drop.hidden = false; });
  addEventListener("dragover", (e) => { if (!hasFiles(e)) return; e.preventDefault(); e.dataTransfer.dropEffect = D.busy || D.dialogOpen() ? "none" : "copy"; });
  addEventListener("dragleave", (e) => { if (!hasFiles(e)) return; depth = Math.max(0, depth - 1); if (!depth) drop.hidden = true; });
  addEventListener("drop", (e) => {
    if (!hasFiles(e)) return;
    e.preventDefault();
    depth = 0;
    drop.hidden = true;
    if (!D.busy && !D.dialogOpen()) openFile(e.dataTransfer.files[0]);
  });

  // ================================================================ the controls
  $("act-backup").addEventListener("click", backUp);
  $("act-open").addEventListener("click", () => $("file").click());
  $("file").addEventListener("change", (e) => { const f = e.target.files && e.target.files[0]; e.target.value = ""; openFile(f); });
  $("act-example").addEventListener("click", () => openExample(true));
  $("act-keep-all").addEventListener("click", keepAll);
  $("act-restore-all").addEventListener("click", openWhole);

  if (!D.hasMidi) {
    $("act-backup").hidden = true;
    $("rv-req").append(" ", h("span", { class: "rv-nomidi" }, "This browser can’t reach the FM-1 (it has no Web MIDI): open a file here, or the River in Chrome or Edge to back up."));
  }
  paintControls();
  if (location.hash === "#example") openExample(false);
  addEventListener("hashchange", () => { if (location.hash === "#example" && !(src && src.kind === "example")) openExample(true); });
})();
